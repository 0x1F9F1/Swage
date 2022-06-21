#pragma once

#include <type_traits>

namespace Swage
{
    template <typename Func, typename Storage = void* [2]>
    class TrivialFunc;

    template <typename Result, typename... Params, typename Storage>
    class TrivialFunc<Result(Params...), Storage>
    {
    public:
        static constexpr usize DataSize = sizeof(Storage);
        static constexpr usize DataAlign = (std::max)(alignof(Storage), alignof(void*));

        constexpr TrivialFunc() noexcept = default;

        constexpr TrivialFunc(std::nullptr_t) noexcept
            : TrivialFunc()
        {}

        template <typename F>
        inline TrivialFunc(const F& func) noexcept
            : invoke_([](const void* context, Params... args) -> Result {
                return (*static_cast<const F*>(context))(std::forward<Params>(args)...);
            })
        {
            static_assert(sizeof(F) <= DataSize, "Function is too large");
            static_assert(alignof(F) <= DataAlign, "Function is over-aligned");
            static_assert(std::is_trivially_copyable_v<F>, "Function is not trivially copyable");
            static_assert(!std::is_reference_v<F>, "Function cannot be a reference");

            new (data_) F(func);
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
    class Callback<Result(Params...)>
    {
    public:
        constexpr Callback() noexcept = default;

        constexpr Callback(std::nullptr_t) noexcept
            : Callback()
        {}

        template <typename F>
        inline Callback(const F& func) noexcept
            : data_(&reinterpret_cast<const char&>(func))
            , invoke_([](const void* context, Params... params) -> Result {
                return (*static_cast<const F*>(context))(std::forward<Params>(params)...);
            })
        {}

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
        Result (*invoke_)(const void* context, Params... args) {};
        const void* data_ {};
    };
} // namespace Swage