/* BFAR, MMFAR and SFAR are read/write registers (Armv8-M ARM DDI0553B.r, D1.2.6, D1.2.165 and
 * D1.2.228). Firmware such as Zephyr's Secure image clears BFAR at boot. The firmware runs in
 * Secure state on a TrustZone-enabled Cortex-M33 and uses the Non-secure aliases for the
 * Non-secure view:
 *   1. a written BFAR value reads back;
 *   2. with AIRCR.BFHFNMINS clear, BFAR is RAZ/WI from Non-secure state;
 *   3. with AIRCR.BFHFNMINS set, a Non-secure write of BFAR takes effect;
 *   4. MMFAR is banked: each bank reads back its own value;
 *   5. a written SFAR value reads back, and SFAR is RAZ/WI from Non-secure state.
 */
#include "common.h"

#define BFAR_NS  REG(0xE002ED38)
#define MMFAR_NS REG(0xE002ED34)
#define SFAR     REG(0xE000EDE8)
#define SFAR_NS  REG(0xE002EDE8)
#define AIRCR_VECTKEY   (0x05FAu << 16)
#define AIRCR_BFHFNMINS (1u << 13)

void test_init(void)
{
}

static void write_bfar(void)
{
    BFAR = 0x12345678;
    barrier();
    results[1] = BFAR;
    BFAR = 0;
    barrier();
    results[2] = BFAR;
}

static void write_bfar_from_non_secure(void)
{
    BFAR_NS = 0xAAAA0000;
    barrier();
    results[3] = BFAR;
    results[4] = BFAR_NS;
}

static void write_bfar_from_non_secure_with_bfhfnmins(void)
{
    AIRCR = AIRCR_VECTKEY | (AIRCR & 0xFFFFu) | AIRCR_BFHFNMINS;
    BFAR_NS = 0x55550000;
    barrier();
    results[5] = BFAR;
    AIRCR = AIRCR_VECTKEY | ((AIRCR & 0xFFFFu) & ~AIRCR_BFHFNMINS);
    barrier();
}

static void write_mmfar(void)
{
    MMFAR = 0x11112222;
    MMFAR_NS = 0x33334444;
    barrier();
    results[6] = MMFAR;
    results[7] = MMFAR_NS;
}

static void write_sfar(void)
{
    SFAR = 0x77778888;
    SFAR_NS = 0;
    barrier();
    results[8] = SFAR;
    results[9] = SFAR_NS;
}

const step_fn steps[] = {write_bfar, write_bfar_from_non_secure, write_bfar_from_non_secure_with_bfhfnmins, write_mmfar,
                         write_sfar};
const int number_of_steps = sizeof(steps) / sizeof(steps[0]);

void on_exception(uint32_t exception, uint32_t *frame, uint32_t exc_return)
{
    unexpected_exception(exception, frame);
}
