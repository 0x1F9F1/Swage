#pragma once

#include "rpf2.h"

namespace Swage::Rage
{
    using fiPackHeader3 = fiPackHeader2;
    using fiPackEntry3 = fiPackEntry2;

    Rc<VirtualFileDevice> LoadRPF3(Rc<Stream> input);
} // namespace Swage::Rage