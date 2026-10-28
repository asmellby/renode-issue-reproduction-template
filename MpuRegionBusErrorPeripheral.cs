//
// Reproducer peripheral: a device that rejects every write, behind a bus that acknowledges writes early
// where the memory type allows it.
//
// For Device memory with the nE attribute only the endpoint returns the write acknowledgement (Armv8-M
// ARM DDI0553B.r, B7.12.3, rule FWFR), so the rejection comes back with the write as a precise BusFault
// (BFSR.PRECISERR, B3.12). Otherwise the write is acknowledged before it reaches the device, and this
// model drops it. To tell the two apart, the model looks up the memory type the MPU gives the address:
// the attribute index of the enabled region that holds it and MPU_MAIR0/1 (D1.2.168, D1.2.169), with
// the regions selected through MPU_RNR (D1.2.174). With the MPU disabled, or no region holding the
// address, the default memory map makes the device Device-nGnRE (B8.1), so the write is dropped.
//

using Antmicro.Renode.Core;
using Antmicro.Renode.Peripherals.Bus;
using Antmicro.Renode.Peripherals.CPU;

namespace Antmicro.Renode.Peripherals.Miscellaneous
{
    public class MpuRegionBusErrorPeripheral : IDoubleWordPeripheral, IKnownSize, IAbsoluteAddressAware
    {
        public MpuRegionBusErrorPeripheral(CortexM cpu)
        {
            this.cpu = cpu;
        }

        public void Reset()
        {
        }

        public void SetAbsoluteAddress(ulong address)
        {
            this.address = (uint)address;
        }

        public uint ReadDoubleWord(long offset)
        {
            return 0;
        }

        public void WriteDoubleWord(long offset, uint value)
        {
            if(MemoryAttributes() == DeviceNGnRnE)
            {
                throw new BusAccessException(BusAccessError.GenericError);
            }
        }

        public long Size => 0x1000;

        // The MAIR attribute of the access, from the MPU region that holds the address
        private uint MemoryAttributes()
        {
            if((cpu.PmsaV8Ctrl & MpuCtrlEnable) == 0)
            {
                return DeviceNGnRE;
            }
            var attributes = DeviceNGnRE;
            var rnr = cpu.PmsaV8Rnr;
            for(var n = 0u; n < cpu.NumberOfMPURegions; n++)
            {
                cpu.PmsaV8Rnr = n;
                var rbar = cpu.PmsaV8Rbar;
                var rlar = cpu.PmsaV8Rlar;
                if((rlar & 1) == 0 || address < (rbar & ~0x1Fu) || address > (rlar | 0x1Fu))
                {
                    continue;
                }
                var index = (int)((rlar >> 1) & 0x7);
                var mair = index < 4 ? cpu.PmsaV8Mair0 : cpu.PmsaV8Mair1;
                attributes = (mair >> (8 * (index % 4))) & 0xFF;
            }
            cpu.PmsaV8Rnr = rnr;
            return attributes;
        }

        private uint address;

        private readonly CortexM cpu;

        private const uint MpuCtrlEnable = 1;
        private const uint DeviceNGnRnE = 0x00;
        private const uint DeviceNGnRE = 0x04;
    }
}
