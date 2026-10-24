/*
 * An exception frame is stacked with the privilege of the code it interrupts (Armv8-M ARM
 * PushStack() and Stack()). Zephyr confines a user thread to its own stack with the MPU, so an
 * interrupt that arrives while a user thread's SP is at the bottom of its stack must fail to stack
 * with a MemManage stacking error (MSTKERR), which Zephyr reports as a fatal error of that thread
 * (arch/arm/core/cortex_m/fault.c mem_manage_fault()).
 *
 * The user thread moves its SP to 16 bytes above the bottom of its stack and spins until the next
 * timer interrupt, whose 32-byte frame would go 16 bytes below the stack. Zephyr must report the
 * stacking fault as a stack overflow (K_ERR_STACK_CHK_FAIL).
 */
#include <zephyr/kernel.h>
#include <zephyr/fatal.h>
#include <zephyr/sys/printk.h>

#define STACK_SIZE 1024

K_THREAD_STACK_DEFINE(user_stack, STACK_SIZE);
static struct k_thread user_thread;

static volatile unsigned int fatal_reason = UINT_MAX;
static volatile k_tid_t fatal_thread;

void k_sys_fatal_error_handler(unsigned int reason, const struct arch_esf *esf)
{
	ARG_UNUSED(esf);
	fatal_reason = reason;
	fatal_thread = k_current_get();
}

static void user_entry(void *bottom, void *p2, void *p3)
{
	__asm__ volatile("mov sp, %0\n"
			 "b .\n"
			 :
			 : "r"((uint32_t)bottom + 16));
}

int main(void)
{
	uint32_t bottom = (uint32_t)K_THREAD_STACK_BUFFER(user_stack);

	k_thread_create(&user_thread, user_stack, STACK_SIZE, user_entry, (void *)bottom, NULL,
			NULL, K_PRIO_PREEMPT(1), K_USER, K_NO_WAIT);
	k_msleep(100);

	if (fatal_thread != &user_thread) {
		printk("FAIL: no fault when an interrupt stacked below the user stack\n");
		k_thread_abort(&user_thread);
	} else if (fatal_reason == K_ERR_ARM_MEM_UNSTACKING) {
		printk("FAIL: the frame was stacked below the user stack, and only unstacking it "
		       "faulted (K_ERR_ARM_MEM_UNSTACKING)\n");
	} else if (fatal_reason != K_ERR_STACK_CHK_FAIL) {
		printk("FAIL: fatal error reason %u, expected K_ERR_STACK_CHK_FAIL (%u)\n",
		       fatal_reason, K_ERR_STACK_CHK_FAIL);
	} else {
		printk("PASS: stacking below the user stack was reported as a stack overflow\n");
	}
	printk("REPRODUCER DONE\n");
	return 0;
}
