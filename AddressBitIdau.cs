//
// A fixed-function IDAU with no registers, like the address-decoded IDAUs of Arm SSE subsystems:
// addresses in [NscStart, NscEnd] are Secure and Non-secure callable, and it attributes no other
// address.
//
using Antmicro.Renode.Core;
using Antmicro.Renode.Peripherals.CPU;

namespace Antmicro.Renode.Peripherals.Miscellaneous
{
    public class AddressBitIdau : CortexMImplementationDefinedAttributionUnit, IPeripheral
    {
        public AddressBitIdau(CortexM cpu, uint nscStart, uint nscEnd) : base(cpu)
        {
            this.nscStart = nscStart;
            this.nscEnd = nscEnd;
        }

        public void Reset()
        {
        }

        public override bool AttributionCheckCallback(uint address, bool secure, AccessType type, int accessWidth, out int region, out SecurityAttribution attribution)
        {
            region = 0;
            attribution = SecurityAttribution.NonSecureCallable;
            return address >= nscStart && address <= nscEnd;
        }

        private readonly uint nscStart;
        private readonly uint nscEnd;
    }
}
