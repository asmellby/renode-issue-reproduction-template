/* Handler mode runs with CONTROL.SPSEL cleared: ActivateException() sets CONTROL.SPSEL of the
 * target Security state to 0, and an exception preempting a handler records SP_main in
 * EXC_RETURN.SPSEL (Armv8-M ARM DDI0553B.r, E2.1.14 and E2.1.117; B3.18).
 *
 * Thread mode runs on PSP and executes SVC. The SVCall handler reads CONTROL and pends PendSV,
 * which has a higher priority and preempts it. PendSV must see EXC_RETURN with Mode = Handler and
 * SPSEL = 0, and returning from it must return to the SVCall handler.
 *
 * The same holds for two nested interrupts, and for an interrupt tail-chained after another one:
 * both handlers read CONTROL.SPSEL = 0, the inner one gets EXC_RETURN with Mode = Handler and
 * SPSEL = 0, the tail-chained one returns to Thread mode on PSP, and the thread resumes on PSP.
 */
#include "common.h"

#define NVIC_IPR0 REG(0xE000E400)

static inline uint32_t get_sp(void)
{
    uint32_t v;
    __asm volatile("mov %0, sp" : "=r"(v));
    return v;
}

/* Thread mode still runs on PSP */
static uint32_t thread_on_psp(void)
{
    return ((get_control() >> 1) & 1) && get_sp() == get_psp();
}

void test_init(void)
{
    SHPR2 = 0x80u << 24;      /* SVCall */
    SHPR3 = 0x40u << 16;      /* PendSV preempts SVCall */
    NVIC_IPR0 = 0x4080u;      /* IRQ0 0x80, IRQ1 0x40 */
    NVIC_ISER0 = 3u;
}

static void nested_irqs(void)
{
    NVIC_ISPR0 = 1u; /* IRQ0, which pends IRQ1 */
    barrier();
    results[9] = thread_on_psp();
}

static void tail_chained_irqs(void)
{
    __asm volatile("cpsid i" : : : "memory");
    NVIC_ISPR0 = 3u; /* IRQ1 runs first, IRQ0 is tail-chained */
    __asm volatile("cpsie i\n isb" : : : "memory");
    results[14] = thread_on_psp();
}

static void svc_then_pendsv(void)
{
    __asm volatile("svc #0" : : : "memory");
    results[4] = 1; /* returned to Thread mode */
}

const step_fn steps[] = {svc_then_pendsv, nested_irqs, tail_chained_irqs};
const int number_of_steps = sizeof(steps) / sizeof(steps[0]);

void on_exception(uint32_t exception, uint32_t *frame, uint32_t exc_return)
{
    switch(exception) {
        case EXC_SVCALL:
            results[1] = get_control();
            ICSR = ICSR_PENDSVSET;
            barrier();
            results[3] = 1; /* SVCall handler resumed after PendSV */
            break;
        case EXC_PENDSV:
            results[2] = exc_return;
            break;
        case EXC_IRQ(0):
            if(current_step == 1) {
                results[5] = get_control();
                NVIC_ISPR0 = 2u; /* IRQ1 preempts */
                barrier();
                results[8] = 1; /* IRQ0 handler resumed after IRQ1 */
            } else {
                results[12] = get_control();
                results[13] = exc_return;
            }
            break;
        case EXC_IRQ(1):
            if(current_step == 1) {
                results[6] = get_control();
                results[7] = exc_return;
            } else {
                results[10] = get_control();
                results[11] = exc_return;
            }
            break;
        default:
            unexpected_exception(exception, frame);
    }
}
