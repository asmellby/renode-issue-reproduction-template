/*
 * An IDAU attributes each address as Non-secure, Secure, or Secure and Non-secure callable
 * (Armv8-M ARM B10.4, rule MVCM), and an address is Secure when either the SAU or the IDAU says
 * so. TT reports the IDAU region it identifies (TT_RESP.IRVALID and IREGION), Secure regions
 * included. The platform enables TrustZone and test.robot configures IDAU region 0 Secure (RLAR
 * bit 2) over SECURE_ADDRESS and region 1 Non-secure over NS_ADDRESS. NO_REGION_ADDRESS is in no
 * IDAU region.
 *
 * The app marks all three addresses Non-secure in the SAU and checks with TT, as
 * arm_cmse_addr_is_secure() would (arch/arm/core/cortex_m/cmse/arm_core_cmse.c), that SECURE_ADDRESS
 * is Secure in IDAU region 0, NS_ADDRESS is Non-secure, and NO_REGION_ADDRESS is Secure with no
 * IDAU region.
 */
#include <zephyr/kernel.h>
#include <zephyr/sys/barrier.h>
#include <zephyr/sys/printk.h>

#define SECURE_ADDRESS 0x2003F000u
#define NS_ADDRESS     0x2003F800u
#define NO_REGION_ADDRESS 0x2003E800u

/* SAU registers; CMSIS only defines SAU for builds with the Security Extension (-mcmse) */
#define SAU_CTRL (*(volatile uint32_t *)0xE000EDD0u)
#define SAU_RNR  (*(volatile uint32_t *)0xE000EDD8u)
#define SAU_RBAR (*(volatile uint32_t *)0xE000EDDCu)
#define SAU_RLAR (*(volatile uint32_t *)0xE000EDE0u)

static uint32_t tt(uint32_t address)
{
	uint32_t resp;

	__asm__ volatile("tt %0, %1" : "=r"(resp) : "r"(address));
	return resp;
}

#define TT_S(resp)       (((resp) >> 22) & 1)
#define TT_IRVALID(resp) (((resp) >> 23) & 1)
#define TT_IREGION(resp) ((resp) >> 24)

int main(void)
{
	/* Region 0: NO_REGION_ADDRESS & ~0xFFF to SECURE_ADDRESS + 0xFFF, Non-secure, enabled */
	SAU_RNR = 0;
	SAU_RBAR = NO_REGION_ADDRESS & ~0xFFFu;
	SAU_RLAR = (SECURE_ADDRESS + 0xFE0) | 1u;
	SAU_CTRL = 1u;
	barrier_dsync_fence_full();
	barrier_isync_fence_full();

	uint32_t resp = tt(SECURE_ADDRESS);

	if (TT_S(resp) && TT_IRVALID(resp) && TT_IREGION(resp) == 0) {
		printk("PASS: TT reports the Secure IDAU region as Secure, in IDAU region 0\n");
	} else {
		printk("FAIL: TT reports the Secure IDAU region with TT_RESP 0x%08x\n", resp);
	}
	resp = tt(NS_ADDRESS);
	if (!TT_S(resp) && TT_IRVALID(resp) && TT_IREGION(resp) == 1) {
		printk("PASS: TT reports the Non-secure IDAU region as Non-secure, in IDAU region 1\n");
	} else {
		printk("FAIL: TT reports the Non-secure IDAU region with TT_RESP 0x%08x\n", resp);
	}
	resp = tt(NO_REGION_ADDRESS);
	if (TT_S(resp) && !TT_IRVALID(resp)) {
		printk("PASS: TT reports an address in no IDAU region as Secure, in no IDAU region\n");
	} else {
		printk("FAIL: TT reports an address in no IDAU region with TT_RESP 0x%08x\n", resp);
	}
	printk("REPRODUCER DONE\n");
	return 0;
}
