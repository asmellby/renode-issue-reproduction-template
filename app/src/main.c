/*
 * CCR.BFHFNMIGN and CCR.TRD are shared by both Security states, the other CCR fields are banked
 * (Armv8-M ARM D1.2.9). The platform enables TrustZone, so Zephyr runs in Secure state.
 *
 * 1. Zephyr's HardFault handler sets CCR.BFHFNMIGN before it reads the instruction at the faulting
 *    PC, so that a bad PC does not lock up the core (arch/arm/core/cortex_m/fault.c
 *    z_arm_is_synchronous_svc()). The app does the same at negative execution priority
 *    (FAULTMASK) and reads PROBE_ADDRESS, which test.robot makes raise a bus error.
 * 2. A Secure write to the Non-secure CCR (through its alias CCR_NS) must not change the Secure
 *    CCR.
 */
#include <zephyr/kernel.h>
#include <zephyr/sys/barrier.h>
#include <zephyr/sys/printk.h>
#include <cmsis_core.h>

#define PROBE_ADDRESS 0x60000000u
#define CCR_NS        (*(volatile uint32_t *)0xE002ED14u)

static void probe_with_bfhfnmign(void)
{
	unsigned int key = irq_lock();

	__set_FAULTMASK(1);

	SCB->CCR |= SCB_CCR_BFHFNMIGN_Msk;
	barrier_dsync_fence_full();
	barrier_isync_fence_full();

	if ((SCB->CCR & SCB_CCR_BFHFNMIGN_Msk) == 0) {
		printk("FAIL: CCR.BFHFNMIGN reads back as 0 after setting it\n");
	}

	(void)*(volatile uint32_t *)PROBE_ADDRESS;

	SCB->CCR &= ~SCB_CCR_BFHFNMIGN_Msk;
	barrier_dsync_fence_full();
	barrier_isync_fence_full();

	__set_FAULTMASK(0);
	irq_unlock(key);

	printk("PASS: the BusFault at negative priority was ignored\n");
}

static void write_non_secure_ccr(void)
{
	uint32_t before = SCB->CCR;

	CCR_NS |= SCB_CCR_DIV_0_TRP_Msk;
	barrier_dsync_fence_full();

	uint32_t after = SCB->CCR;

	if (after != before) {
		printk("FAIL: writing CCR_NS changed the Secure CCR from 0x%x to 0x%x\n", before,
		       after);
	} else {
		printk("PASS: writing CCR_NS kept the Secure CCR\n");
	}
}

int main(void)
{
	probe_with_bfhfnmign();
	write_non_secure_ccr();
	printk("REPRODUCER DONE\n");
	return 0;
}
