#include "common.h"

volatile int current_step;

void exception_entry(void);
void reset_handler(void);

__attribute__((section(".vectors"), used)) const void *const vector_table[16 + 32] = {
    [0] = __main_stack_top,
    [1] = reset_handler,
    [2 ... 16 + 31] = exception_entry,
};

static void run_steps(void)
{
    for(; current_step < number_of_steps; current_step++) {
        steps[current_step]();
    }
    results[0] = RESULTS_DONE;
    for(;;) {
        __asm volatile("wfi");
    }
}

/* Thread mode entry after resume_next_step(): continue the steps on a fresh stack */
__attribute__((noreturn)) static void thread_resume(void)
{
    run_steps();
    __builtin_unreachable();
}

void resume_next_step(void)
{
    uint32_t *frame = __thread_stack_top - 8;
    for(int i = 0; i < 6; i++) {
        frame[i] = 0;
    }
    frame[6] = (uint32_t)thread_resume & ~1u;
    frame[7] = 1u << 24; /* xPSR.T */
    current_step++;
    set_psplim(0);
    set_psp((uint32_t)frame);
    set_control(get_control() & ~1u); /* nPRIV = 0 */
}

void unexpected_exception(uint32_t exception, uint32_t *frame)
{
    results[60] = exception;
    results[61] = CFSR;
    results[62] = HFSR;
    results[63] = frame[6];
    CFSR = CFSR;
    HFSR = HFSR;
    resume_next_step();
}

void exception_dispatch(uint32_t *frame, uint32_t exc_return)
{
    uint32_t ipsr;
    __asm volatile("mrs %0, ipsr" : "=r"(ipsr));
    on_exception(ipsr & 0x1FF, frame, exc_return);
}

/* Pass the stacked frame and EXC_RETURN to exception_dispatch(), which returns with EXC_RETURN in LR */
__attribute__((naked)) void exception_entry(void)
{
    __asm volatile("tst lr, #4\n"
                   "ite eq\n"
                   "mrseq r0, msp\n"
                   "mrsne r0, psp\n"
                   "mov r1, lr\n"
                   "push {r1, lr}\n"
                   "bl exception_dispatch\n"
                   "pop {r1, pc}\n");
}

void reset_handler(void)
{
    for(int i = 0; i < 64; i++) {
        results[i] = 0;
    }
    VTOR = (uint32_t)vector_table;
    SHCSR |= SHCSR_MEMFAULTENA | SHCSR_BUSFAULTENA | SHCSR_USGFAULTENA;
    test_init();
    current_step = 0;
    set_psp((uint32_t)__thread_stack_top);
    set_control(get_control() | 2u); /* SPSEL = 1: Thread mode uses PSP */
    run_steps();
}
