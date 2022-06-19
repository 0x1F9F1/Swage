#pragma once

#include <type_traits>

namespace Swage
{
    template <typename Func, typename Storage = void* [2]>
    class StaticFunc;

    template <typename Result, typename... Params, typename Storage>
    class StaticFunc<Result(Params...), Storage>
    {
    public:
        static constexpr usize DataSize = sizeof(Storage);
        static constexpr usize DataAlign = (std::max)(alignof(Storage), alignof(void*));

        constexpr StaticFunc() noexcept = default;

        constexpr StaticFunc(std::nullptr_t) noexcept
            : StaticFunc()
        {}

        template <typename Func>
        inline StaticFunc(Func func) noexcept
            : invoke_([](const void* context, Params... args) -> Result {
                return (*static_cast<const Func*>(context))(std::forward<Params>(args)...);
            })
        {
            static_assert(sizeof(Func) <= DataSize, "Function is too large");
            static_assert(alignof(Func) <= DataAlign, "Function is over-aligned");
            static_assert(std::is_trivially_copyable_v<Func>, "Function is not trivially copyable");
            static_assert(!std::is_reference_v<Func>, "Function cannot be a reference");

            new (data_) Func(func);
        }

        constexpr explicit operator bool() const noexcept
        {
            return invoke_ != nullptr;
        }

        template <typename... Args>
        inline Result operator()(Args&&... args) const
        {
            return invoke_(data_, std::forward<Args>(args)...);
        }

    private:
        alignas(DataAlign) u8 data_[DataSize] {};
        Result (*invoke_)(const void* context, Params... args) {};
    };

    template <typename Func>
    class Callback;

    template <typename Result, typename... Params>
    class Callback<Result(Params...)> : public StaticFunc<Result(Params...), void*>
    {
    public:
        template <typename Func>
        inline Callback(const Func& func) noexcept
            : StaticFunc<Result(Params...), void*>(
                  [&func](Params... params) -> Result { return func(std::forward<Params>(params)...); })
        {}
    };
} // namespace Swage