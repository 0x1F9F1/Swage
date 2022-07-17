#include "rpf4.h"

#include "asset/stream.h"

namespace Swage::Rage
{
    Rc<FileDevice> LoadRPF4(Rc<Stream> input)
    {
        return LoadRPF2(std::move(input));
    }
} // namespace Swage::Rage
