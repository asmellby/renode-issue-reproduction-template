/* Minimal bare-metal test harness for Cortex-M33 reproducers.
 *
 * Thread code runs on PSP. Faults are handled on MSP and reported to on_exception(), which may
 * resume the thread at the next step with resume_next_step(). Results are written to the `results`
 * array at RESULTS_ADDRESS, which test.robot reads once results[0] is RESULTS_DONE.
 */
#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

#define REG(a) (*(volatile uint32_t *)(a))

#define ICSR  REG(0xE000ED04)
#define VTOR  REG(0xE000ED08)
#define AIRCR REG(0xE000ED0C)
#define CCR   REG(0xE000ED14)
#define SHPR1 REG(0xE000ED18)
#define SHPR2 REG(0xE000ED1C)
#define SHPR3 REG(0xE000ED20)
#define SHCSR REG(0xE000ED24)
#define CFSR  REG(0xE000ED28)
#define HFSR  REG(0xE000ED2C)
#define MMFAR REG(0xE000ED34)
#define BFAR  REG(0xE000ED38)

#define MPU_CTRL  REG(0xE000ED94)
#define MPU_RNR   REG(0xE000ED98)
#define MPU_RBAR  REG(0xE000ED9C)
#define MPU_RLAR  REG(0xE000EDA0)
#define MPU_MAIR0 REG(0xE000EDC0)

#define NVIC_ISER0 REG(0xE000E100)
#define NVIC_ISPR0 REG(0xE000E200)
#define NVIC_ICPR0 REG(0xE000E280)
#define STIR       REG(0xE000EF00)

#define ICSR_PENDSVSET (1u << 28)

#define CCR_USERSETMPEND (1u << 1)
#define CCR_DIV_0_TRP    (1u << 4)
#define CCR_BFHFNMIGN    (1u << 8)

#define SHCSR_MEMFAULTPENDED (1u << 13)
#define SHCSR_MEMFAULTENA    (1u << 16)
#define SHCSR_BUSFAULTENA    (1u << 17)
#define SHCSR_USGFAULTENA    (1u << 18)

#define CFSR_DACCVIOL    (1u << 1)
#define CFSR_MSTKERR     (1u << 4)
#define CFSR_MMARVALID   (1u << 7)
#define CFSR_PRECISERR   (1u << 9)
#define CFSR_BFARVALID   (1u << 15)
#define CFSR_STKOF       (1u << 20)

#define MPU_CTRL_ENABLE     (1u << 0)
#define MPU_CTRL_PRIVDEFENA (1u << 2)
/* PMSAv8 RBAR.AP */
#define MPU_AP_RW_PRIV (0u << 1)
#define MPU_AP_RW_ANY  (1u << 1)
#define MPU_AP_RO_PRIV (2u << 1)
#define MPU_AP_RO_ANY  (3u << 1)
#define MPU_XN         (1u << 0)

#define EXC_MEMMANAGE 4
#define EXC_BUSFAULT  5
#define EXC_USAGE     6
#define EXC_SVCALL    11
#define EXC_PENDSV    14
#define EXC_IRQ(n)    (16 + (n))

#define RESULTS_ADDRESS 0x2000F000u
#define RESULTS_DONE    0xD0D0D0D0u
#define results         ((volatile uint32_t *)RESULTS_ADDRESS)

/* Top of the thread stack (PSP) and of the handler stack (MSP), see link.ld */
extern uint32_t __thread_stack_top[];
extern uint32_t __thread_stack_bottom[];
extern uint32_t __main_stack_top[];

/* Provided by the test */
typedef void (*step_fn)(void);
extern const step_fn steps[];
extern const int number_of_steps;
/* Called before the steps run, privileged, on MSP */
void test_init(void);
/* Called for every exception except reset. `frame` is the stacked exception frame, `exc_return`
 * the EXC_RETURN value. Returning from it returns from the exception. */
void on_exception(uint32_t exception, uint32_t *frame, uint32_t exc_return);

/* Records an exception the test did not expect in results[60..63] and continues with the next step */
void unexpected_exception(uint32_t exception, uint32_t *frame);

/* SP of the interrupted context, from its exception frame */
static inline uint32_t frame_sp(uint32_t *frame)
{
    return (uint32_t)frame + 32 + (((frame[7] >> 9) & 1) ? 4 : 0);
}

/* Index of the step that runs, or runs next */
extern volatile int current_step;

/* From an exception handler: abandon the interrupted step and continue with the next one,
 * privileged, on a fresh thread stack once the handler returns. */
void resume_next_step(void);

static inline uint32_t get_control(void)
{
    uint32_t v;
    __asm volatile("mrs %0, control" : "=r"(v));
    return v;
}

static inline void set_control(uint32_t v)
{
    __asm volatile("msr control, %0\n isb" : : "r"(v) : "memory");
}

static inline uint32_t get_psp(void)
{
    uint32_t v;
    __asm volatile("mrs %0, psp" : "=r"(v));
    return v;
}

static inline void set_psp(uint32_t v)
{
    __asm volatile("msr psp, %0" : : "r"(v) : "memory");
}

static inline void set_psplim(uint32_t v)
{
    __asm volatile("msr psplim, %0" : : "r"(v) : "memory");
}

static inline uint32_t get_psplim(void)
{
    uint32_t v;
    __asm volatile("mrs %0, psplim" : "=r"(v));
    return v;
}

static inline void barrier(void)
{
    __asm volatile("dsb\n isb" : : : "memory");
}

#endif
