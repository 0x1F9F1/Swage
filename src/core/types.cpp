#include "types.h"

#include <limits>

namespace Swage
{
    static_assert(std::numeric_limits<unsigned char>::digits == 8);

    static_assert(sizeof(usize) == sizeof(void*));

    static_assert(sizeof(usize) == sizeof(isize));
    static_assert(std::numeric_limits<usize>::digits == std::numeric_limits<isize>::digits + 1);

    static_assert(std::numeric_limits<f32>::is_iec559 && sizeof(f32) == 4);
    static_assert(std::numeric_limits<f64>::is_iec559 && sizeof(f64) == 8);
} // namespace Swage
