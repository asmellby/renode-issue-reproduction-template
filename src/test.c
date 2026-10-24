/* An IDAU attributes each address as Non-secure, Secure, or Secure and Non-secure callable
 * (Armv8-M ARM DDI0553B.r, B10.4, rule MVCM), and TT reports the IDAU region it identifies
 * (TT_RESP.IRVALID and IREGION, D1.2.266), Secure regions included.
 *
 * An SAU region marks all addresses below Non-secure, so the IDAU decides. test.robot configures
 * IDAU region 0 Secure (RLAR bit 2) over SECURE_ADDRESS, region 1 Non-secure over NS_ADDRESS and
 * region 2 both Non-secure callable and Secure over NSC_ADDRESS. NO_REGION_ADDRESS is in no IDAU
 * region. For each address the test records TT_RESP.S, IRVALID and IREGION.
 */
#include "common.h"

#define SAU_CTRL       REG(0xE000EDD0)
#define SAU_RNR        REG(0xE000EDD8)
#define SAU_RBAR       REG(0xE000EDDC)
#define SAU_RLAR       REG(0xE000EDE0)
#define SECURE_ADDRESS 0x20008000u
#define NS_ADDRESS     0x20008800u
#define NSC_ADDRESS    0x20009000u
#define NO_REGION_ADDRESS 0x20009800u

void test_init(void)
{
    SAU_RNR = 0;
    SAU_RBAR = SECURE_ADDRESS;
    SAU_RLAR = (SECURE_ADDRESS + 0x1FE0) | 1u; /* Non-secure, enabled */
    SAU_CTRL = 1u;
    barrier();
}

/* TT_RESP.S, IRVALID and IREGION of `address` into results[n..n+2] */
static void tt(int n, uint32_t address)
{
    uint32_t resp;
    __asm volatile("tt %0, %1" : "=r"(resp) : "r"(address));
    results[n] = (resp >> 22) & 1;
    results[n + 1] = (resp >> 23) & 1;
    results[n + 2] = resp >> 24;
}

static void tt_addresses(void)
{
    tt(1, SECURE_ADDRESS);
    tt(4, NS_ADDRESS);
    tt(7, NSC_ADDRESS);
    tt(10, NO_REGION_ADDRESS);
}

const step_fn steps[] = {tt_addresses};
const int number_of_steps = sizeof(steps) / sizeof(steps[0]);

void on_exception(uint32_t exception, uint32_t *frame, uint32_t exc_return)
{
    unexpected_exception(exception, frame);
}
