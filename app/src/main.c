/*
 * BFAR is a read/write register (Armv8-M ARM D1.2.6). Zephyr's Secure firmware clears it at boot
 * before it makes BusFaults target Non-secure state (arch/arm/include/cortex_m/exception.h
 * z_arm_exc_setup()). The app writes BFAR the same way and checks that the value reads back.
 */
#include <zephyr/kernel.h>
#include <zephyr/sys/barrier.h>
#include <zephyr/sys/printk.h>
#include <cmsis_core.h>

static void check_bfar_write(uint32_t value)
{
	SCB->BFAR = value;
	barrier_dsync_fence_full();

	uint32_t read = SCB->BFAR;

	if (read != value) {
		printk("FAIL: BFAR reads 0x%08x after writing 0x%08x\n", read, value);
	} else {
		printk("PASS: BFAR reads back 0x%08x\n", value);
	}
}

int main(void)
{
	check_bfar_write(0x12345678);
	check_bfar_write(0);
	printk("REPRODUCER DONE\n");
	return 0;
}
