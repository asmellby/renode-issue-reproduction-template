/*
 * Template for a reproducer: print "PASS: <check>" or "FAIL: <check>" for every check and
 * "REPRODUCER DONE" at the end, and wait for the PASS lines in test.robot.
 */
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

int main(void)
{
	printk("PASS: the example check\n");
	printk("REPRODUCER DONE\n");
	return 0;
}
