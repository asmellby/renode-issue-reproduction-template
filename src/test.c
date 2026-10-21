/* Template for a reproducer: replace the steps and on_exception() with the scenario under test,
 * and check the recorded results in test.robot.
 */
#include "common.h"

void test_init(void)
{
}

static void example_step(void)
{
    results[1] = 1;
}

const step_fn steps[] = {example_step};
const int number_of_steps = sizeof(steps) / sizeof(steps[0]);

void on_exception(uint32_t exception, uint32_t *frame, uint32_t exc_return)
{
    unexpected_exception(exception, frame);
}
