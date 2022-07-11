#pragma once

#include "rpf2.h"

namespace Swage::Rage
{
    using fiPackHeader4 = fiPackHeader2;
    using fiPackEntry4 = fiPackEntry2;

    Rc<VirtualFileDevice> LoadRPF4(Rc<Stream> input);
} // namespace Swage::Rage
