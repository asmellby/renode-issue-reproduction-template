/* ARMv8-M stack limit checks (Armv8-M ARM DDI0553B.r, rules YVWT and ZLZG, ViolatesSPLim()).
 *
 * Thread mode runs on PSP with PSPLIM set. Each step violates the limit in a different way and
 * expects a synchronous UsageFault with UFSR.STKOF:
 *   0. MOV SP, Rm with Rm below PSPLIM: SP is left unchanged.
 *   1. STMDB SP!, {...} whose lowest address is below PSPLIM: SP is left unchanged and nothing
 *      is written below the limit.
 *   2. Exception entry whose frame would go below PSPLIM: PSP stops at PSPLIM.
 *   3. STRD Rt, Rt2, [SP, #-imm]! below PSPLIM: nothing is written below the limit.
 *   4. MOV SP, Rm below PSPLIM inside an IT block: the stacked xPSR keeps ITSTATE.
 *   5. NMI entry from Thread mode with CCR.STKOFHFNMIGN set: the limit still applies, because
 *      IsReqExcPriNeg() considers only the exceptions active before the NMI.
 *   6. Unprivileged MSR PSPLIM is ignored.
 *   7. PSPLIM bits [2:0] are RES0.
 *   8. VPUSH below PSPLIM: nothing is written below the limit.
 *   9. Exception entry with lazy FP state preservation whose FP context is below PSPLIM:
 *      FPCCR.SPLIMVIOL is set and the lazy preservation does not write below the limit.
 *  10. SUBS SP, SP, #imm below PSPLIM: the flags are left unchanged (RSPCheck[] precedes the APSR
 *      update).
 *  11. ADDS SP, SP, Rm below PSPLIM: the flags are left unchanged.
 *  12. Exception entry whose frame faults on the MPU above PSPLIM and continues below it: CFSR has
 *      both MSTKERR and STKOF, MemManage is taken and the STKOF UsageFault is pended (PushStack()
 *      and MergeExcInfo()).
 *  13. An UNDEFINED load/store encoding with SP writeback below PSPLIM raises UNDEFINSTR, not STKOF.
 *  14. Unprivileged MRS MSP reads zero (C2.4.125).
 */
#include "common.h"

#define LIMIT ((uint32_t)__thread_stack_bottom + 0x400)
#define PATTERN 0xA5A5A5A5u

#define CPACR REG(0xE000ED88)
#define FPCCR REG(0xE000EF34)
#define ICSR_NMIPENDSET (1u << 31)
#define CCR_STKOFHFNMIGN (1u << 10)
#define FPCCR_SPLIMVIOL (1u << 9)
#define ICSR_PENDSVCLR (1u << 27)
#define SHCSR_USGFAULTPENDED (1u << 12)
#define CFSR_UNDEFINSTR (1u << 16)
/* APSR.Z */
#define APSR_Z (1u << 30)
/* xPSR IT/ICI bits */
#define XPSR_IT_MASK 0x0600FC00u

static void fill_below_limit(int words)
{
    volatile uint32_t *below = (uint32_t *)(LIMIT - 4 * words);
    for(int i = 0; i < words; i++) {
        below[i] = PATTERN;
    }
}

static int intact_below_limit(int words)
{
    volatile uint32_t *below = (uint32_t *)(LIMIT - 4 * words);
    for(int i = 0; i < words; i++) {
        if(below[i] != PATTERN) {
            return 0;
        }
    }
    return 1;
}

/* resume_next_step() for a thread that had an FP context: the exception returns with an extended frame */
static void resume_next_step_fp(uint32_t exc_return)
{
    resume_next_step();
    if(!(exc_return & (1u << 4))) {
        uint32_t pc = __thread_stack_top[-2];
        uint32_t xpsr = __thread_stack_top[-1];
        uint32_t *frame = __thread_stack_top - 26;
        for(int i = 0; i < 26; i++) {
            frame[i] = 0;
        }
        frame[6] = pc;
        frame[7] = xpsr;
        set_psp((uint32_t)frame);
    }
}

void test_init(void)
{
    SHPR3 = 0xFFu << 16;  /* PendSV lowest priority */
    CPACR |= 0xFu << 20;  /* CP10 and CP11 full access */
    FPCCR |= 3u << 30;    /* ASPEN and LSPEN */
    barrier();
}

static void sp_write_below_limit(void)
{
    set_psplim(LIMIT);
    __asm volatile("mov r1, sp\n"
                   "mov sp, %0\n"
                   "mov sp, r1\n"
                   :
                   : "r"(LIMIT - 0x40)
                   : "r1", "memory");
    set_psplim(0);
}

static void push_below_limit(void)
{
    fill_below_limit(4);
    /* SP = LIMIT + 40, then push 14 registers (56 bytes): the lowest address is LIMIT - 16, while
     * an exception frame from SP stays above the limit. */
    __asm volatile("mov r12, sp\n"
                   "mov sp, %0\n"
                   "msr psplim, %1\n"
                   "stmdb sp!, {r0-r12, lr}\n"
                   "movs r0, #0\n"
                   "msr psplim, r0\n"
                   "mov sp, r12\n"
                   :
                   : "r"(LIMIT + 40), "r"(LIMIT)
                   : "r0", "r12", "memory");
}

static void exception_entry_below_limit(void)
{
    /* SP = LIMIT + 16 and pend PendSV: its 32-byte frame would go below the limit */
    __asm volatile("mov r12, sp\n"
                   "mov sp, %0\n"
                   "msr psplim, %1\n"
                   "str %2, [%3]\n"
                   "dsb\n"
                   "isb\n"
                   "movs r0, #0\n"
                   "msr psplim, r0\n"
                   "mov sp, r12\n"
                   :
                   : "r"(LIMIT + 16), "r"(LIMIT), "r"(ICSR_PENDSVSET), "r"(&ICSR)
                   : "r0", "r12", "memory");
}

static void strd_below_limit(void)
{
    fill_below_limit(1);
    /* SP = LIMIT + 0x24, then STRD to SP - 0x28 = LIMIT - 4 with writeback. The exception frame
     * from SP (aligned down to LIMIT + 0x20) stays above the limit. */
    __asm volatile("mov r12, sp\n"
                   "mov sp, %0\n"
                   "msr psplim, %1\n"
                   "strd r0, r1, [sp, #-0x28]!\n"
                   "movs r0, #0\n"
                   "msr psplim, r0\n"
                   "mov sp, r12\n"
                   :
                   : "r"(LIMIT + 0x24), "r"(LIMIT)
                   : "r0", "r1", "r12", "memory");
}

static void sp_write_in_it_block(void)
{
    __asm volatile("mov r12, sp\n"
                   "msr psplim, %1\n"
                   "cmp r0, r0\n"
                   "itt eq\n"
                   "moveq sp, %0\n"
                   "addeq r1, r1, #1\n"
                   "movs r0, #0\n"
                   "msr psplim, r0\n"
                   "mov sp, r12\n"
                   :
                   : "r"(LIMIT - 0x40), "r"(LIMIT)
                   : "r0", "r1", "r12", "memory", "cc");
}

static void nmi_entry_below_limit(void)
{
    CCR |= CCR_STKOFHFNMIGN;
    /* SP = LIMIT + 16 and pend the NMI: its 32-byte frame would go below the limit */
    __asm volatile("mov r12, sp\n"
                   "mov sp, %0\n"
                   "msr psplim, %1\n"
                   "str %2, [%3]\n"
                   "dsb\n"
                   "isb\n"
                   "movs r0, #0\n"
                   "msr psplim, r0\n"
                   "mov sp, r12\n"
                   :
                   : "r"(LIMIT + 16), "r"(LIMIT), "r"(ICSR_NMIPENDSET), "r"(&ICSR)
                   : "r0", "r12", "memory");
    CCR &= ~CCR_STKOFHFNMIGN;
}

static void unprivileged_psplim_write(void)
{
    set_psplim(LIMIT);
    set_control(get_control() | 1u); /* nPRIV = 1 */
    __asm volatile("movs r0, #0\n"
                   "msr psplim, r0\n"
                   "svc #0\n"
                   :
                   :
                   : "r0", "memory");
}

static void psplim_res0(void)
{
    set_psplim(LIMIT + 5);
    results[28] = get_psplim() == LIMIT;
    set_psplim(0);
}

static void vpush_below_limit(void)
{
    fill_below_limit(4);
    /* SP = LIMIT + 0x70, then VPUSH {d0-d15} (128 bytes): the lowest address is LIMIT - 0x10. The
     * extended exception frame from SP (0x68 bytes) stays above the limit. */
    __asm volatile(".fpu fpv5-sp-d16\n"
                   "mov r12, sp\n"
                   "mov sp, %0\n"
                   "msr psplim, %1\n"
                   "vpush {d0-d15}\n"
                   "movs r0, #0\n"
                   "msr psplim, r0\n"
                   "mov sp, r12\n"
                   :
                   : "r"(LIMIT + 0x70), "r"(LIMIT)
                   : "r0", "r12", "memory");
}

static void lazy_fp_below_limit(void)
{
    fill_below_limit(4);
    /* With an FP context, SP = LIMIT + 0x40 and pend PendSV: the extended frame is 0x68 bytes, so
     * the reserved FP area starts at LIMIT - 8 */
    __asm volatile(".fpu fpv5-sp-d16\n"
                   "vmov s0, %4\n"
                   "mov r12, sp\n"
                   "mov sp, %0\n"
                   "msr psplim, %1\n"
                   "str %2, [%3]\n"
                   "dsb\n"
                   "isb\n"
                   "movs r0, #0\n"
                   "msr psplim, r0\n"
                   "mov sp, r12\n"
                   :
                   : "r"(LIMIT + 0x40), "r"(LIMIT), "r"(ICSR_PENDSVSET), "r"(&ICSR), "r"(0x12345678)
                   : "r0", "r12", "memory");
}

/* Steps after lazy_fp_below_limit() resume with an FP context. Drop it so that exception frames are basic frames. */
static void clear_fp_context(void)
{
    set_control(get_control() & ~4u); /* CONTROL.FPCA = 0 */
}

static void subs_sp_below_limit(void)
{
    clear_fp_context();
    /* SP = LIMIT + 0x40 with only Z set, then SUBS SP, SP, #0x50: the result would clear Z and set C */
    __asm volatile("mov r12, sp\n"
                   "mov sp, %0\n"
                   "msr psplim, %1\n"
                   "msr apsr_nzcvq, %2\n"
                   "subs.w sp, sp, #0x50\n"
                   "movs r0, #0\n"
                   "msr psplim, r0\n"
                   "mov sp, r12\n"
                   :
                   : "r"(LIMIT + 0x40), "r"(LIMIT), "r"(APSR_Z)
                   : "r0", "r12", "memory", "cc");
}

static void adds_sp_below_limit(void)
{
    clear_fp_context();
    /* SP = LIMIT + 0x40 with only Z set, then ADDS SP, SP, Rm with Rm = -0x50 */
    __asm volatile("mov r12, sp\n"
                   "mov sp, %0\n"
                   "msr psplim, %1\n"
                   "msr apsr_nzcvq, %2\n"
                   "adds.w sp, sp, %3\n"
                   "movs r0, #0\n"
                   "msr psplim, r0\n"
                   "mov sp, r12\n"
                   :
                   : "r"(LIMIT + 0x40), "r"(LIMIT), "r"(APSR_Z), "r"(-0x50)
                   : "r0", "r12", "memory", "cc");
}

static void mpu_region(int n, uint32_t base, uint32_t limit, uint32_t ap_xn)
{
    MPU_RNR = n;
    MPU_RBAR = (base & ~0x1Fu) | ap_xn;
    MPU_RLAR = (limit & ~0x1Fu) | 1u; /* AttrIndx 0, enabled */
}

static void stacking_fault_and_limit(void)
{
    clear_fp_context();
    fill_below_limit(4);
    /* LIMIT..LIMIT + 0x1F is read-only. SP = LIMIT + 16 and pend PendSV: its frame faults at
     * LIMIT + 12 and its lower half is below the limit */
    MPU_MAIR0 = 0x44;
    mpu_region(0, LIMIT, LIMIT + 0x1F, MPU_AP_RO_PRIV | MPU_XN);
    MPU_CTRL = MPU_CTRL_ENABLE | MPU_CTRL_PRIVDEFENA;
    barrier();
    exception_entry_below_limit();
}

static void undefined_ldst_below_limit(void)
{
    clear_fp_context();
    /* SP = LIMIT + 0x40, then a store with size 0b11 (UNDEFINED) to [SP, #-0x50]! */
    __asm volatile("mov r12, sp\n"
                   "mov sp, %0\n"
                   "msr psplim, %1\n"
                   ".short 0xf86d, 0x0d50\n"
                   "movs r0, #0\n"
                   "msr psplim, r0\n"
                   "mov sp, r12\n"
                   :
                   : "r"(LIMIT + 0x40), "r"(LIMIT)
                   : "r0", "r12", "memory");
}

static void unprivileged_msp_read(void)
{
    set_control(get_control() | 1u); /* nPRIV = 1 */
    uint32_t msp;
    __asm volatile("mrs %0, msp" : "=r"(msp));
    results[48] = msp;
    results[49] = 1;
    __asm volatile("svc #0" : : : "memory");
}

const step_fn steps[] = {sp_write_below_limit,      push_below_limit,     exception_entry_below_limit,
                         strd_below_limit,          sp_write_in_it_block, nmi_entry_below_limit,
                         unprivileged_psplim_write, psplim_res0,          vpush_below_limit,
                         lazy_fp_below_limit,       subs_sp_below_limit,  adds_sp_below_limit,
                         stacking_fault_and_limit,  undefined_ldst_below_limit, unprivileged_msp_read};
const int number_of_steps = sizeof(steps) / sizeof(steps[0]);

void on_exception(uint32_t exception, uint32_t *frame, uint32_t exc_return)
{
    if(exception == EXC_PENDSV) {
        results[(current_step < 10 ? 10 : 40) + current_step] = 1; /* PendSV ran during this step */
        return;
    }
    if(exception == 2 && current_step == 5) {
        /* NMI: CFSR and PSP after its stacking */
        results[25] = CFSR;
        results[26] = get_psp() == LIMIT;
        return;
    }
    if(exception == EXC_SVCALL && current_step == 6) {
        results[27] = get_psplim() == LIMIT;
        resume_next_step();
        return;
    }
    if(exception == EXC_SVCALL && current_step == 14) {
        resume_next_step();
        return;
    }
    if(exception == EXC_MEMMANAGE && current_step == 12) {
        /* Return to the pending UsageFault, which tail-chains */
        results[41] = CFSR;
        results[42] = get_psp() == LIMIT;
        results[43] = (SHCSR & SHCSR_USGFAULTPENDED) != 0;
        MPU_CTRL = 0;
        barrier();
        if(!results[43]) {
            CFSR = CFSR;
            ICSR = ICSR_PENDSVCLR;
            resume_next_step();
        }
        return;
    }
    if(exception != EXC_USAGE) {
        unexpected_exception(exception, frame);
        return;
    }

    uint32_t cfsr = CFSR;
    CFSR = cfsr;
    uint32_t psp = get_psp();
    switch(current_step) {
        case 0:
            results[1] = cfsr;
            results[2] = frame_sp(frame) == frame[1]; /* SP unchanged: still the value saved in r1 */
            break;
        case 1:
            results[3] = cfsr;
            results[4] = frame_sp(frame) == LIMIT + 40;
            results[5] = intact_below_limit(4);
            break;
        case 2:
            results[6] = cfsr;
            results[7] = psp == LIMIT;
            break;
        case 3:
            results[21] = cfsr;
            results[22] = intact_below_limit(1);
            break;
        case 4:
            results[23] = cfsr;
            results[24] = (frame[7] & XPSR_IT_MASK) != 0;
            break;
        case 5:
            /* The STKOF of the NMI stacking, taken once the NMI returns */
            CCR &= ~CCR_STKOFHFNMIGN;
            break;
        case 8:
            results[29] = cfsr;
            results[30] = (uint32_t)frame == LIMIT + 8; /* SP unchanged: extended frame below LIMIT + 0x70 */
            results[31] = intact_below_limit(4);
            break;
        case 10:
            results[35] = cfsr;
            results[36] = frame[7] >> 28 == APSR_Z >> 28; /* NZCV unchanged */
            results[37] = frame_sp(frame) == LIMIT + 0x40;
            break;
        case 11:
            results[38] = cfsr;
            results[39] = frame[7] >> 28 == APSR_Z >> 28;
            results[40] = frame_sp(frame) == LIMIT + 0x40;
            break;
        case 12:
            results[44] = cfsr; /* The pended STKOF UsageFault ran */
            results[45] = intact_below_limit(4);
            MPU_CTRL = 0;
            ICSR = ICSR_PENDSVCLR;
            barrier();
            break;
        case 13:
            results[46] = cfsr;
            results[47] = frame_sp(frame) == LIMIT + 0x40;
            break;
        case 9:
            results[32] = cfsr;
            results[33] = (FPCCR & FPCCR_SPLIMVIOL) != 0;
            /* Trigger the lazy preservation of the thread's FP context */
            __asm volatile(".fpu fpv5-sp-d16\n"
                           "vmov r0, s0\n"
                           :
                           :
                           : "r0", "memory");
            results[34] = intact_below_limit(4);
            break;
    }
    resume_next_step_fp(exc_return);
}
