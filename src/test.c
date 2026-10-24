/* The IDAU is not part of the PE (Armv8-M ARM DDI0553B.r, B10.2, rule RPKG) and is usually
 * fixed-function address decoding, so its attribution is the same after a reset.
 *
 * test.robot describes such an IDAU, with IDAU regions or with a custom IDAU model, that makes
 * NSC_ADDRESS Secure and Non-secure callable (NSC). An SAU region marks the same range
 * Non-secure, so a TT instruction on NSC_ADDRESS reports Secure (TT_RESP.S) only through the IDAU.
 * The firmware records that on every boot; test.robot checks it before and after a reset.
 *
 * test.robot can also add an implementation-defined attribution exemption over EXEMPT_ADDRESS, which
 * the SAU marks Non-secure too. An exempt address takes the Security state of the access, so TT from
 * Secure state reports it Secure only through the exemption.
 */
#include "common.h"

#define SAU_CTRL    REG(0xE000EDD0)
#define SAU_RNR     REG(0xE000EDD8)
#define SAU_RBAR    REG(0xE000EDDC)
#define SAU_RLAR    REG(0xE000EDE0)
#define NSC_ADDRESS    0x20008000u
#define EXEMPT_ADDRESS 0x20009000u

void test_init(void)
{
    SAU_RNR = 0;
    SAU_RBAR = NSC_ADDRESS;
    SAU_RLAR = (EXEMPT_ADDRESS + 0xFE0) | 1u; /* Non-secure, enabled */
    SAU_CTRL = 1u; /* ENABLE */
    barrier();
}

static void tt_nsc_address(void)
{
    uint32_t resp;
    __asm volatile("tt %0, %1" : "=r"(resp) : "r"(NSC_ADDRESS));
    results[1] = resp;
    results[2] = (resp >> 22) & 1; /* S */
    __asm volatile("tt %0, %1" : "=r"(resp) : "r"(EXEMPT_ADDRESS));
    results[3] = (resp >> 22) & 1; /* S */
}

const step_fn steps[] = {tt_nsc_address};
const int number_of_steps = sizeof(steps) / sizeof(steps[0]);

void on_exception(uint32_t exception, uint32_t *frame, uint32_t exc_return)
{
    unexpected_exception(exception, frame);
}
