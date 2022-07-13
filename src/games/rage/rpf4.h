#pragma once

#include "rpf2.h"

namespace Swage::Rage
{
    // Same as RPF2, but with larger offsets
    using fiPackHeader4 = fiPackHeader2;
    using fiPackEntry4 = fiPackEntry2;

    Rc<VirtualFileDevice> LoadRPF4(Rc<Stream> input);
} // namespace Swage::Rage
