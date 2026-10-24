/* Exception entry stacks the frame with the privilege of the interrupted code: unprivileged in
 * Thread mode with CONTROL.nPRIV set (Armv8-M ARM DDI0553B.r, PushStack() and Stack(), E2.1.328
 * and E2.1.387).
 *
 * The lower part of the thread stack is privileged-only in the MPU. An unprivileged thread with
 * its SP just above that part executes SVC, so the lower half of the frame goes into memory the
 * thread has no access to. Stacking must fail with a MemManage fault (MMFSR.MSTKERR) instead of
 * writing the frame.
 *
 * A privileged thread in the same place stacks its frame normally. An unprivileged thread with an FP
 * context whose lazily preserved FP area lies in a privileged-only window gets its frame stacked, but
 * the lazy preservation, triggered when the handler uses the FPU, must fail with MMFSR.MLSPERR
 * (UpdateFPCCR() records the privilege of the interrupted thread in FPCCR.USER, E2.1.411).
 *
 * A Secure SVCall handler interrupting a Secure thread on PSP_S tail-chains into a Non-secure
 * interrupt. PushCalleeStack() pushes the additional state context below the frame on PSP_S, the
 * stack EXC_RETURN selects (E2.1.327), so the return from the Non-secure handler restores R4-R11.
 */
#include "common.h"

#define BOUNDARY ((uint32_t)__thread_stack_bottom + 0x400)
/* Privileged-only window in the unprivileged part of the thread stack */
#define WINDOW (BOUNDARY + 0x200)
#define CPACR REG(0xE000ED88)
#define FPCCR REG(0xE000EF34)
#define CFSR_MLSPERR (1u << 5)
#define SHCSR_SVCALLPENDED (1u << 15)
#define SAU_CTRL    REG(0xE000EDD0)
#define SAU_RNR     REG(0xE000EDD8)
#define SAU_RBAR    REG(0xE000EDDC)
#define SAU_RLAR    REG(0xE000EDE0)
#define VTOR_NS     REG(0xE002ED08)
#define NVIC_ITNS0  REG(0xE000E380)
#define NVIC_IPR0   REG(0xE000E400)

/* Non-secure vector table, IRQ0 handler and its marker, in one SAU Non-secure region */
static uint32_t ns_block[64] __attribute__((aligned(256)));
#define NS_HANDLER (&ns_block[32])
#define NS_MARKER  (ns_block[40])
/* R4-R11 of the thread after the tail-chained Non-secure interrupt returned */
static uint32_t callee_after_return[8];

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

/* unexpected_exception() for an exception whose EXC_RETURN has DCRS = 0: the return pops a callee frame too */
static void unexpected_exception_callee_frame(uint32_t exception, uint32_t *frame, uint32_t exc_return)
{
    unexpected_exception(exception, frame);
    if(!(exc_return & (1u << 5))) {
        uint32_t *callee = (uint32_t *)get_psp() - 10;
        for(int i = 0; i < 10; i++) {
            callee[i] = 0;
        }
        callee[0] = 0xFEFA125Au | ((exc_return >> 4) & 1u);
        set_psp((uint32_t)callee);
    }
}

static void mpu_region(int n, uint32_t base, uint32_t limit, uint32_t ap_xn)
{
    MPU_RNR = n;
    MPU_RBAR = (base & ~0x1Fu) | ap_xn;
    MPU_RLAR = (limit & ~0x1Fu) | 1u; /* AttrIndx 0, enabled */
}

void test_init(void)
{
    SHPR2 = 0x80u << 24; /* SVCall below MemManage (0) */
    MPU_MAIR0 = 0x44;    /* Normal memory, non-cacheable */
    mpu_region(0, 0x00000000, 0x0000FFFF, MPU_AP_RO_ANY);
    mpu_region(1, (uint32_t)__thread_stack_bottom, BOUNDARY - 1, MPU_AP_RW_PRIV | MPU_XN);
    mpu_region(2, BOUNDARY, WINDOW - 1, MPU_AP_RW_ANY | MPU_XN);
    mpu_region(3, WINDOW, WINDOW + 0x3F, MPU_AP_RW_PRIV | MPU_XN);
    mpu_region(4, WINDOW + 0x40, (uint32_t)__thread_stack_top - 1, MPU_AP_RW_ANY | MPU_XN);
    CPACR |= 0xFu << 20; /* CP10 and CP11 full access */
    FPCCR |= 3u << 30;   /* ASPEN and LSPEN */
    MPU_CTRL = MPU_CTRL_ENABLE | MPU_CTRL_PRIVDEFENA;

    /* Non-secure IRQ0 handler: movs r1, #1; ldr r0, =&NS_MARKER; str r1, [r0]; bx lr */
    uint16_t *code = (uint16_t *)NS_HANDLER;
    code[0] = 0x2101;
    code[1] = 0x4801;
    code[2] = 0x6001;
    code[3] = 0x4770;
    NS_HANDLER[2] = (uint32_t)&NS_MARKER;
    ns_block[0] = (uint32_t)&ns_block[64];
    ns_block[16] = (uint32_t)NS_HANDLER | 1u;
    VTOR_NS = (uint32_t)ns_block;
    SAU_RNR = 0;
    SAU_RBAR = (uint32_t)ns_block;
    SAU_RLAR = ((uint32_t)&ns_block[56]) | 1u; /* Non-secure, enabled */
    SAU_CTRL = 1u;
    NVIC_ITNS0 = 1u;
    NVIC_IPR0 = 0xC0u; /* IRQ0 below SVCall */
    NVIC_ISER0 = 1u;
    barrier();
}

static void unprivileged_svc_frame_in_privileged_memory(void)
{
    __asm volatile("mov sp, %0\n"
                   "mrs r0, control\n"
                   "orr r0, r0, #1\n"
                   "msr control, r0\n"
                   "isb\n"
                   "svc #0\n"
                   "b .\n"
                   :
                   : "r"(BOUNDARY + 16)
                   : "r0", "memory");
}

static void privileged_svc_frame_in_privileged_memory(void)
{
    __asm volatile("mov sp, %0\n"
                   "svc #0\n"
                   :
                   : "r"(BOUNDARY + 16)
                   : "memory");
}

static void secure_svc_tail_chains_to_non_secure_irq(void)
{
    register uint32_t *saved __asm("r0") = callee_after_return;
    __asm volatile("push {r4-r11}\n"
                   "mov r4, #0x44\n"
                   "mov r5, #0x55\n"
                   "mov r6, #0x66\n"
                   "mov r7, #0x77\n"
                   "mov r8, #0x88\n"
                   "mov r9, #0x99\n"
                   "mov r10, #0xAA\n"
                   "mov r11, #0xBB\n"
                   "svc #0\n"
                   "stm r0, {r4-r11}\n"
                   "pop {r4-r11}\n"
                   :
                   : "r"(saved)
                   : "memory");
    results[8] = NS_MARKER;
    uint32_t ok = 1;
    for(int i = 0; i < 8; i++) {
        ok &= callee_after_return[i] == 0x44u + 0x11u * i;
    }
    results[9] = ok;
}

static void unprivileged_svc_lazy_fp_in_privileged_memory(void)
{
    /* The extended frame is 0x68 bytes: the basic frame ends at WINDOW and the reserved FP area starts there */
    __asm volatile(".fpu fpv5-sp-d16\n"
                   "vmov s0, %1\n"
                   "mov sp, %0\n"
                   "mrs r0, control\n"
                   "orr r0, r0, #1\n"
                   "msr control, r0\n"
                   "isb\n"
                   "svc #0\n"
                   "b .\n"
                   :
                   : "r"(WINDOW + 0x48), "r"(0x12345678)
                   : "r0", "memory");
}

const step_fn steps[] = {unprivileged_svc_frame_in_privileged_memory, privileged_svc_frame_in_privileged_memory,
                         secure_svc_tail_chains_to_non_secure_irq, unprivileged_svc_lazy_fp_in_privileged_memory};
const int number_of_steps = sizeof(steps) / sizeof(steps[0]);

void on_exception(uint32_t exception, uint32_t *frame, uint32_t exc_return)
{
    switch(exception) {
        case EXC_MEMMANAGE:
            if(current_step == 3) {
                /* Lazy preservation from the SVCall handler failed: drop the lazy state and return to it */
                results[5] = CFSR;
                CFSR = CFSR;
                FPCCR &= ~1u; /* LSPACT */
                return;
            }
            results[1] = CFSR;
            CFSR = CFSR;
            /* MemManage replaced the SVCall, which stays pending; drop it so that step 1 runs its own SVC */
            SHCSR &= ~SHCSR_SVCALLPENDED;
            resume_next_step();
            break;
        case EXC_SVCALL:
            if(current_step == 0) {
                /* The frame was written below BOUNDARY */
                results[2] = 1;
            } else if(current_step == 1) {
                /* The frame of this step's SVC, below BOUNDARY */
                results[3] = frame_sp(frame) == BOUNDARY + 16;
                results[4] = CFSR;
            } else if(current_step == 2) {
                /* Tail-chain into the Non-secure interrupt when returning to the thread */
                results[7] = 1;
                NVIC_ISPR0 = 1u;
                return;
            } else if(current_step == 3) {
                results[6] = 1;
                /* Trigger the lazy preservation of the thread's FP context */
                __asm volatile(".fpu fpv5-sp-d16\n"
                               "vmov r0, s0\n"
                               :
                               :
                               : "r0", "memory");
            }
            resume_next_step_fp(exc_return);
            break;
        default:
            if(current_step == 2) {
                results[8] = NS_MARKER;
            }
            unexpected_exception_callee_frame(exception, frame, exc_return);
    }
}
