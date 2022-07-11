#include "rpf3.h"

#include "asset/stream.h"

namespace Swage::Rage
{
    Rc<VirtualFileDevice> LoadRPF3(Rc<Stream> input)
    {
        return LoadRPF2(std::move(input));
    }
} // namespace Swage::Rage
