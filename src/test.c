/* For Device memory with the nE attribute only the endpoint returns the write acknowledgement
 * (Armv8-M ARM DDI0553B.r, B7.12.3, rule FWFR), so a write the endpoint rejects is a precise
 * BusFault (BFSR.PRECISERR, B3.12). The peripheral at 0x40000000 (MpuRegionBusErrorPeripheral.cs)
 * rejects every write; it looks up the memory type of the access in the MPU, selecting the regions
 * through MPU_RNR (D1.2.174), and answers with a bus error only for Device-nGnRnE, dropping the
 * write otherwise:
 *   0. MPU disabled (default map, Device-nGnRE): the write is dropped, no fault;
 *   1. MPU region over the peripheral, Device-nGnRE: the write is dropped, no fault;
 *   2. MPU region over the peripheral, Device-nGnRnE: a precise BusFault (CFSR.PRECISERR and
 *      BFARVALID, BFAR = the address).
 * The BusFault handler records the fault and continues with the next step.
 */
#include "common.h"

#define PERIPHERAL 0x40000000u
#define RESULT_BASE(step) (1 + 4 * (step))
/* MAIR attributes: Attr0 Device-nGnRnE, Attr1 Device-nGnRE */
#define MAIR0_VALUE 0x0400u
#define ATTR_NGNRNE 0u
#define ATTR_NGNRE  1u

void test_init(void)
{
}

static void write_peripheral(void)
{
    REG(PERIPHERAL) = 1;
    barrier();
}

static void map_peripheral(uint32_t attr_index)
{
    MPU_MAIR0 = MAIR0_VALUE;
    MPU_RNR = 0;
    MPU_RBAR = PERIPHERAL | MPU_AP_RW_ANY | MPU_XN;
    MPU_RLAR = (PERIPHERAL + 0xFE0u) | (attr_index << 1) | 1u;
    MPU_CTRL = MPU_CTRL_ENABLE | MPU_CTRL_PRIVDEFENA;
    barrier();
}

static void write_without_mpu(void)
{
    write_peripheral();
}

static void write_device_ngnre(void)
{
    map_peripheral(ATTR_NGNRE);
    write_peripheral();
    MPU_CTRL = 0;
    barrier();
}

static void write_device_ngnrne(void)
{
    map_peripheral(ATTR_NGNRNE);
    write_peripheral();
    MPU_CTRL = 0;
    barrier();
}

const step_fn steps[] = {write_without_mpu, write_device_ngnre, write_device_ngnrne};
const int number_of_steps = sizeof(steps) / sizeof(steps[0]);

void on_exception(uint32_t exception, uint32_t *frame, uint32_t exc_return)
{
    uint32_t cfsr = CFSR;
    int base = RESULT_BASE(current_step);
    if(exception != EXC_BUSFAULT) {
        unexpected_exception(exception, frame);
        return;
    }
    results[base] = exception;
    results[base + 1] = cfsr;
    results[base + 2] = BFAR;
    CFSR = cfsr;
    MPU_CTRL = 0;
    barrier();
    resume_next_step();
}
