/* CCR.BFHFNMIGN and CCR.TRD are shared by both Security states, the other CCR fields are banked
 * (Armv8-M ARM DDI0553B.r, D1.2.9). The firmware runs in Secure state and
 *   1. sets DIV_0_TRP (banked) and BFHFNMIGN (shared) in one Secure CCR write: both bits must read
 *      back;
 *   2. writes the Non-secure CCR through its alias CCR_NS: the Secure CCR must keep its value;
 *   3. clears BFHFNMIGN through CCR_NS while AIRCR.BFHFNMINS is 0: the write is ignored, as BFHFNMIGN
 *      is read-only from Non-secure state, and CCR_NS still reads it;
 *   4. clears BFHFNMIGN through CCR_NS with AIRCR.BFHFNMINS set: the write takes effect;
 *   5. sets TRD: it is RES0 on a Cortex-M33 (Armv8.0-M);
 *   6. reads bits [9] and [0], which are RES1;
 *   7. sets BFHFNMIGN and FAULTMASK and reads an address that raises a bus error: the BusFault is
 *      ignored, so execution continues.
 */
#include "common.h"

#define CCR_NS REG(0xE002ED14)
#define CCR_TRD (1u << 20)
#define AIRCR_VECTKEY (0x05FAu << 16)
#define AIRCR_BFHFNMINS (1u << 13)
#define BUS_ERROR_ADDRESS 0x60000000u

void test_init(void)
{
}

static void write_secure_ccr(void)
{
    results[1] = CCR;
    CCR = results[1] | CCR_DIV_0_TRP | CCR_BFHFNMIGN;
    barrier();
    results[2] = CCR;
}

static void write_non_secure_ccr(void)
{
    results[3] = CCR;
    CCR_NS = CCR_NS | CCR_DIV_0_TRP;
    barrier();
    results[4] = CCR;
}

static void clear_bfhfnmign_from_non_secure(void)
{
    CCR = CCR | CCR_BFHFNMIGN;
    CCR_NS = CCR_NS & ~CCR_BFHFNMIGN;
    barrier();
    results[5] = (CCR >> 8) & 1;
    results[6] = (CCR_NS >> 8) & 1;
}

static void clear_bfhfnmign_from_non_secure_with_bfhfnmins(void)
{
    AIRCR = AIRCR_VECTKEY | (AIRCR & 0xFFFFu) | AIRCR_BFHFNMINS;
    CCR = CCR | CCR_BFHFNMIGN;
    CCR_NS = CCR_NS & ~CCR_BFHFNMIGN;
    barrier();
    results[7] = (CCR >> 8) & 1;
    AIRCR = AIRCR_VECTKEY | ((AIRCR & 0xFFFFu) & ~AIRCR_BFHFNMINS);
    barrier();
}

static void set_trd(void)
{
    CCR = CCR | CCR_TRD;
    barrier();
    results[8] = (CCR >> 20) & 1;
}

static void read_res1_bits(void)
{
    results[9] = CCR & 0x201u;
}

static void ignored_bus_fault(void)
{
    CCR = CCR | CCR_BFHFNMIGN;
    __asm volatile("cpsid f\n"
                   "ldr r0, [%0]\n"
                   "cpsie f\n"
                   :
                   : "r"(BUS_ERROR_ADDRESS)
                   : "r0", "memory");
    results[10] = 1;
}

const step_fn steps[] = {write_secure_ccr,
                         write_non_secure_ccr,
                         clear_bfhfnmign_from_non_secure,
                         clear_bfhfnmign_from_non_secure_with_bfhfnmins,
                         set_trd,
                         read_res1_bits,
                         ignored_bus_fault};
const int number_of_steps = sizeof(steps) / sizeof(steps[0]);

void on_exception(uint32_t exception, uint32_t *frame, uint32_t exc_return)
{
    unexpected_exception(exception, frame);
}
