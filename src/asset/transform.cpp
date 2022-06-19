#include "transform.h"

namespace Swage
{
    usize BinaryTransform::GetOptimalBufferSize()
    {
        return 32 * 1024;
    }
} // namespace Swage