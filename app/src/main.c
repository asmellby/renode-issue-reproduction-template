/*
 * The IDAU is not part of the PE (Armv8-M ARM B10.2, rule RPKG) and is usually fixed-function
 * address decoding, so its attribution is the same after a reset. The platform enables TrustZone
 * and test.robot describes such an IDAU, with IDAU regions or with a custom IDAU model, that makes
 * NSC_ADDRESS Secure and Non-secure callable (NSC).
 *
 * The app marks NSC_ADDRESS Non-secure in the SAU, so TT reports it as Secure only through the
 * IDAU, as arm_cmse_addr_is_secure() would (arch/arm/core/cortex_m/cmse/arm_core_cmse.c). It
 * checks that on every boot and then reboots with sys_reboot(), which on Cortex-M requests a
 * system reset through AIRCR.SYSRESETREQ (arch/arm/core/cortex_m/scb.c).
 */
#include <zephyr/kernel.h>
#include <zephyr/sys/barrier.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/reboot.h>
#include <cmsis_core.h>

#define NSC_ADDRESS 0x2003F000u

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

int main(void)
{
	/* Region 0: NSC_ADDRESS-NSC_ADDRESS + 0xFFF, Non-secure, enabled */
	SAU_RNR = 0;
	SAU_RBAR = NSC_ADDRESS;
	SAU_RLAR = (NSC_ADDRESS + 0xFE0) | 1u;
	SAU_CTRL = 1u;
	barrier_dsync_fence_full();
	barrier_isync_fence_full();

	if (tt(NSC_ADDRESS) & (1u << 22)) {
		printk("PASS: TT reports the IDAU NSC region as Secure\n");
	} else {
		printk("FAIL: TT reports the IDAU NSC region as Non-secure\n");
	}

	printk("Rebooting\n");
	k_msleep(1);
	sys_reboot(SYS_REBOOT_COLD);
	return 0;
}
