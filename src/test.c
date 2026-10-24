/* The MPU does not apply to the PPB (Armv8-M ARM DDI0553B.r, rule TGQD): PPB accesses use the
 * default system address map, and accesses a PPB register does not permit are BusFaults raised by
 * that register (rules RTKK, NDML, DQQS).
 *
 * The MPU is enabled with no region covering the PPB, and an unprivileged thread
 *   0. writes STIR with CCR.USERSETMPEND set: the interrupt must be pended, with no fault;
 *   1. reads ICSR, which is privileged-only: a precise BusFault (BFAR = ICSR), not a MemManage;
 *   2. writes a byte of NVIC_IPR0: a precise BusFault with BFAR the byte address, and the register
 *      is not updated (rule NSWG);
 *   3. reads DWT_CTRL (not implemented on this platform): a precise BusFault (rule NDML);
 *   4. writes STIR with CCR.USERSETMPEND clear: a precise BusFault, and no interrupt.
 * A privileged thread
 *   5. writes SHPR3 with a privileged read-only MPU region over the SCS: no fault;
 *   6. reads VTOR with PRIVDEFENA clear: no fault, as the MPU does not apply to the PPB.
 * And an unprivileged thread
 *   7. writes an ITM stimulus port: no fault, as the ITM permits unprivileged accesses;
 *   8. with its SP in the PPB, pends an interrupt through STIR: the frame is stacked into the PPB
 *      without a fault (stacking accesses to the PPB may be performed, rule RJHJ);
 *   9. writes SHPR3 with the MPU disabled: a precise BusFault, and SHPR3 is not updated;
 *  10. reads STIR with CCR.USERSETMPEND set: no fault, and STIR reads as zero (D1.2.9, D1.2.236);
 *  11. runs VLSTM with its address in the PPB: a precise BusFault, as for any other store (VLSTM is
 *      not an exception sequence, rule RJHJ);
 *  12. runs VLLDM with its address in the PPB: a precise BusFault.
 * An unprivileged Non-secure thread
 *  13. reads an address in the PPB that is not exempt from attribution and that the SAU attributes
 *      Secure: an AUVIOL SecureFault, as attribution is checked before the PPB is accessed
 *      (MemA_with_priv_security(), ValidateAddress()).
 * And an unprivileged thread
 *  14. waits while test.robot translates ICSR through the CPU, as a debugger does, then reads ICSR:
 *      a precise BusFault, as the translation must not leave an entry that bypasses the check.
 *
 * The core is a TrustZone-enabled Cortex-M33 running in Secure state; steps 0-10 and 14 behave the
 * same without TrustZone.
 */
#include "common.h"

extern const void *const vector_table[];

#define NVIC_IPR0  REG(0xE000E400)
#define DWT_CTRL   REG(0xE0001000)
#define ITM_STIM0  REG(0xE0000000)
#define PPB_STACK  0xE0001800u
#define IPR0_VALUE 0x00004000u
#define CPACR      REG(0xE000ED88)
#define FPCCR      REG(0xE000EF34)
#define SAU_CTRL   REG(0xE000EDD0)
#define SAU_RNR    REG(0xE000EDD8)
#define SAU_RBAR   REG(0xE000EDDC)
#define SAU_RLAR   REG(0xE000EDE0)
#define SFSR       REG(0xE000EDE4)
#define SFAR       REG(0xE000EDE8)

#define FPCCR_LSPEN          (1u << 30)
#define SHCSR_SECUREFAULTENA (1u << 19)
#define EXC_SECUREFAULT      7
/* Not exempt from attribution (rule LDTN), and Secure: no SAU region covers it */
#define SECURE_PPB_ADDRESS 0xE0004000u

/* Steps 0-9 record their results at 1 + step * 4, later steps at 64 + (step - 10) * 4 */
#define RESULT_BASE(step)    ((step) < 10 ? 1 + (step) * 4 : 64 + ((step) - 10) * 4)
#define STIR_READ_VALUE      84
#define NS_SFSR              85
#define NS_SFAR              86
#define STIR_READ_DONE       87
#define DEBUGGER_STEP_WAIT   88 /* set by the thread while it waits for test.robot */
#define DEBUGGER_STEP_GO     89 /* set by test.robot after the translation */
#define NUMBER_OF_RESULTS    96

/* Non-secure code, alone in its 32-byte SAU granule: the access under test, then a Secure access
 * that faults in case the first one does not. */
void ns_ppb_read(void);
__asm(".section .text.ns_ppb_read, \"ax\", %progbits\n"
      ".balign 32\n"
      ".thumb\n"
      ".global ns_ppb_read\n"
      ".type ns_ppb_read, %function\n"
      ".thumb_func\n"
      "ns_ppb_read:\n"
      "ldr r1, [r0]\n"
      "ldr r1, [r2]\n"
      "b .\n"
      ".balign 32\n"
      ".text\n");

static uint32_t ns_stack[64] __attribute__((aligned(32)));

static void mpu_region(int n, uint32_t base, uint32_t limit, uint32_t ap_xn)
{
    MPU_RNR = n;
    MPU_RBAR = (base & ~0x1Fu) | ap_xn;
    MPU_RLAR = (limit & ~0x1Fu) | 1u;
}

void test_init(void)
{
    for(int i = 64; i < NUMBER_OF_RESULTS; i++) {
        results[i] = 0;
    }
    SHCSR |= SHCSR_SECUREFAULTENA;
    SHPR2 = 0x80u << 24; /* SVCall below the faults */
    CCR |= CCR_USERSETMPEND;
    NVIC_ISER0 = 1u << 0;
    NVIC_IPR0 = IPR0_VALUE;
    MPU_MAIR0 = 0x44;
    mpu_region(0, 0x00000000, 0x0000FFFF, MPU_AP_RO_ANY);
    mpu_region(1, (uint32_t)__thread_stack_bottom, (uint32_t)__thread_stack_top - 1, MPU_AP_RW_ANY | MPU_XN);
    mpu_region(2, RESULTS_ADDRESS, RESULTS_ADDRESS + NUMBER_OF_RESULTS * 4 - 1, MPU_AP_RW_ANY | MPU_XN);
    mpu_region(4, (uint32_t)__thread_stack_top, (uint32_t)__main_stack_top - 1, MPU_AP_RW_PRIV | MPU_XN);
    MPU_CTRL = MPU_CTRL_ENABLE | MPU_CTRL_PRIVDEFENA;
    barrier();
}

enum access { STORE, LOAD, STORE_BYTE };

/* Drop privileges, perform the access, then SVC back to privileged */
static void unprivileged_access(uint32_t address, uint32_t value, enum access access)
{
    __asm volatile("mrs r0, control\n"
                   "orr r0, r0, #1\n"
                   "msr control, r0\n"
                   "isb\n"
                   "cmp %2, #1\n"
                   "beq 1f\n"
                   "bhi 2f\n"
                   "str %1, [%0]\n"
                   "b 3f\n"
                   "1: ldr %1, [%0]\n"
                   "b 3f\n"
                   "2: strb %1, [%0]\n"
                   "3: dsb\n"
                   "isb\n"
                   "svc #0\n"
                   "b .\n"
                   :
                   : "r"(address), "r"(value), "r"(access)
                   : "r0", "memory", "cc");
}

static void unprivileged_stir_write(void)
{
    unprivileged_access((uint32_t)&STIR, 0, STORE);
}

static void unprivileged_icsr_read(void)
{
    unprivileged_access((uint32_t)&ICSR, 0, LOAD);
}

static void unprivileged_ipr_byte_write(void)
{
    unprivileged_access((uint32_t)&NVIC_IPR0 + 1, 0x80, STORE_BYTE);
}

static void unprivileged_dwt_read(void)
{
    unprivileged_access((uint32_t)&DWT_CTRL, 0, LOAD);
}

static void privileged_read_without_privdefena(void)
{
    uint32_t vtor;
    /* Only the flash, thread stack and results regions are mapped while PRIVDEFENA is clear */
    __asm volatile("str %2, [%1]\n"
                   "dsb\n"
                   "isb\n"
                   "ldr %0, [%3]\n"
                   "str %4, [%1]\n"
                   "dsb\n"
                   "isb\n"
                   : "=&r"(vtor)
                   : "r"(&MPU_CTRL), "r"(MPU_CTRL_ENABLE), "r"(&VTOR), "r"(MPU_CTRL_ENABLE | MPU_CTRL_PRIVDEFENA)
                   : "memory");
    results[51] = vtor == (uint32_t)vector_table;
}

static void privileged_write_under_read_only_region(void)
{
    mpu_region(3, 0xE000E000, 0xE000EFFF, MPU_AP_RO_PRIV | MPU_XN);
    barrier();
    SHPR3 = 0;
    barrier();
    MPU_RNR = 3;
    MPU_RLAR = 0;
    barrier();
    results[52] = 1;
}

static void unprivileged_stir_write_without_usersetmpend(void)
{
    CCR &= ~CCR_USERSETMPEND;
    barrier();
    unprivileged_access((uint32_t)&STIR, 0, STORE);
}

static void unprivileged_itm_write(void)
{
    unprivileged_access((uint32_t)&ITM_STIM0, 'A', STORE);
}

static void unprivileged_stacking_into_ppb(void)
{
    CCR |= CCR_USERSETMPEND;
    barrier();
    __asm volatile("mrs r0, control\n"
                   "orr r0, r0, #1\n"
                   "msr control, r0\n"
                   "isb\n"
                   "mov sp, %0\n"
                   "movs r0, #0\n"
                   "str r0, [%1]\n"
                   "dsb\n"
                   "isb\n"
                   "b .\n"
                   :
                   : "r"(PPB_STACK), "r"(&STIR)
                   : "r0", "memory");
}

static void unprivileged_write_with_mpu_disabled(void)
{
    MPU_CTRL = 0;
    barrier();
    unprivileged_access((uint32_t)&SHPR3, 0xFFFFFFFFu, STORE);
}

static void unprivileged_stir_read(void)
{
    results[STIR_READ_VALUE] = 0xFFFFFFFFu;
    __asm volatile("mrs r0, control\n"
                   "orr r0, r0, #1\n"
                   "msr control, r0\n"
                   "isb\n"
                   "ldr r0, [%0]\n"
                   "str r0, [%1]\n"
                   "movs r0, #1\n"
                   "str r0, [%2]\n"
                   "dsb\n"
                   "isb\n"
                   "svc #0\n"
                   "b .\n"
                   :
                   : "r"(&STIR), "r"(&results[STIR_READ_VALUE]), "r"(&results[STIR_READ_DONE])
                   : "r0", "memory");
}

/* VLSTM or VLLDM, encoding T1 with Rn = r0, unprivileged, with CONTROL_S.SFPA set and FPCCR.LSPEN
 * clear so that the instruction accesses memory */
static void unprivileged_fp_lazy_access(uint32_t instruction)
{
    CPACR |= 0xFu << 20; /* CP10 and CP11 full access */
    FPCCR &= ~FPCCR_LSPEN;
    barrier();
    __asm volatile("mrs r0, control\n"
                   "orr r0, r0, #9\n" /* SFPA, nPRIV */
                   "msr control, r0\n"
                   "isb\n"
                   "mov r0, %0\n"
                   "cmp %1, #0\n"
                   "bne 1f\n"
                   ".inst.w 0xec200a00\n" /* vlstm r0 */
                   "b 2f\n"
                   "1: .inst.w 0xec300a00\n" /* vlldm r0 */
                   "2: dsb\n"
                   "isb\n"
                   "svc #0\n"
                   "b .\n"
                   :
                   : "r"((uint32_t)&DWT_CTRL), "r"(instruction)
                   : "r0", "memory", "cc");
}

static void unprivileged_vlstm_to_ppb(void)
{
    unprivileged_fp_lazy_access(0);
}

static void unprivileged_vlldm_from_ppb(void)
{
    unprivileged_fp_lazy_access(1);
}

static void sau_region(int n, uint32_t base, uint32_t size)
{
    SAU_RNR = n;
    SAU_RBAR = base & ~0x1Fu;
    SAU_RLAR = ((base + size - 1) & ~0x1Fu) | 1u; /* Non-secure, enabled */
}

static void non_secure_unprivileged_secure_ppb_read(void)
{
    sau_region(0, (uint32_t)ns_ppb_read & ~1u, 32);
    sau_region(1, (uint32_t)ns_stack, sizeof(ns_stack));
    SAU_CTRL = 1u;
    barrier();
    __asm volatile("msr psp_ns, %0\n"
                   "msr control_ns, %1\n"
                   "isb\n"
                   "mov r0, %2\n"
                   "mov r2, %3\n"
                   "bxns %4\n"
                   :
                   : "r"((uint32_t)ns_stack + sizeof(ns_stack)), "r"(3u), "r"(SECURE_PPB_ADDRESS), "r"(RESULTS_ADDRESS),
                     "r"((uint32_t)ns_ppb_read & ~1u)
                   : "r0", "r2", "memory");
}

static void unprivileged_icsr_read_after_debugger_translation(void)
{
    __asm volatile("mrs r0, control\n"
                   "orr r0, r0, #1\n"
                   "msr control, r0\n"
                   "isb\n"
                   "movs r0, #1\n"
                   "str r0, [%0]\n"
                   "1: ldr r0, [%1]\n"
                   "cmp r0, #0\n"
                   "beq 1b\n"
                   "ldr r0, [%2]\n"
                   "dsb\n"
                   "isb\n"
                   "svc #0\n"
                   "b .\n"
                   :
                   : "r"(&results[DEBUGGER_STEP_WAIT]), "r"(&results[DEBUGGER_STEP_GO]), "r"(&ICSR)
                   : "r0", "memory", "cc");
}

/* Return from the current exception to Secure Thread mode on PSP, where resume_next_step() has put
 * the next step's frame */
__attribute__((noreturn)) static void return_to_secure_thread(void)
{
    __asm volatile("bx %0" : : "r"(0xFFFFFFFDu));
    __builtin_unreachable();
}

const step_fn steps[] = {unprivileged_stir_write,
                         unprivileged_icsr_read,
                         unprivileged_ipr_byte_write,
                         unprivileged_dwt_read,
                         unprivileged_stir_write_without_usersetmpend,
                         privileged_write_under_read_only_region,
                         privileged_read_without_privdefena,
                         unprivileged_itm_write,
                         unprivileged_stacking_into_ppb,
                         unprivileged_write_with_mpu_disabled,
                         unprivileged_stir_read,
                         unprivileged_vlstm_to_ppb,
                         unprivileged_vlldm_from_ppb,
                         non_secure_unprivileged_secure_ppb_read,
                         unprivileged_icsr_read_after_debugger_translation};
const int number_of_steps = sizeof(steps) / sizeof(steps[0]);

void on_exception(uint32_t exception, uint32_t *frame, uint32_t exc_return)
{
    uint32_t cfsr = CFSR;
    int base = RESULT_BASE(current_step);
    switch(exception) {
        case EXC_IRQ(0):
            results[base] = 1; /* IRQ 0 taken */
            if(current_step == 8) {
                /* The frame went into the PPB: record the stacking faults and abandon the thread */
                results[54] = cfsr;
                resume_next_step();
            }
            return;
        case EXC_SVCALL:
            resume_next_step();
            return;
        case EXC_MEMMANAGE:
        case EXC_BUSFAULT:
        case EXC_SECUREFAULT:
            results[base + 1] = exception;
            results[base + 2] = cfsr;
            results[base + 3] = BFAR;
            CFSR = cfsr;
            if(current_step == 2) {
                results[50] = NVIC_IPR0 == IPR0_VALUE;
            } else if(current_step == 9) {
                results[53] = SHPR3 == 0;
            } else if(current_step == 13) {
                results[NS_SFSR] = SFSR;
                results[NS_SFAR] = SFAR;
                SFSR = SFSR;
                SAU_CTRL = 0;
                barrier();
            }
            resume_next_step();
            break;
        default:
            unexpected_exception(exception, frame);
            break;
    }
    if(!(exc_return & (1u << 6))) {
        /* The exception was taken from Non-secure state */
        return_to_secure_thread();
    }
}
