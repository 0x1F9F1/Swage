#pragma once

#include "atomic.h"

namespace Swage
{
    class RefCounted
    {
    public:
        inline constexpr RefCounted() noexcept = default;
        inline virtual ~RefCounted() = default;

        RefCounted(const RefCounted&) = delete;
        RefCounted& operator=(const RefCounted&) = delete;

        SW_FORCEINLINE usize AddRef() noexcept
        {
            return ++ref_count_;
        }

        SW_FORCEINLINE usize Release()
        {
            usize const refs = --ref_count_;

            if (refs == 0)
            {
                delete this;
            }

            return refs;
        }

    private:
        usize ref_count_ {1};
    };

    class AtomicRefCounted
    {
    public:
        inline constexpr AtomicRefCounted() noexcept = default;
        inline virtual ~AtomicRefCounted() = default;

        AtomicRefCounted(const AtomicRefCounted&) = delete;
        AtomicRefCounted& operator=(const AtomicRefCounted&) = delete;

        SW_FORCEINLINE usize AddRef() noexcept
        {
            return ref_count_.fetch_add(1, std::memory_order_relaxed);
        }

        SW_FORCEINLINE usize Release()
        {
            usize const refs = ref_count_.fetch_sub(1, std::memory_order_relaxed) - 1;

            if (refs == 0)
            {
                delete this;
            }

            return refs;
        }

    private:
        Atomic<usize> ref_count_ {1};
    };

    template <typename T>
    class Rc final
    {
    public:
        SW_FORCEINLINE constexpr Rc() noexcept = default;

        SW_FORCEINLINE constexpr Rc(std::nullptr_t ptr) noexcept
            : ptr_(ptr)
        {}

        template <typename U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
        SW_FORCEINLINE explicit Rc(U* ptr) noexcept
            : ptr_(ptr)
        {}

        template <typename U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
        SW_FORCEINLINE Rc(Ptr<U> ptr) noexcept
            : ptr_(ptr.release())
        {}

        SW_FORCEINLINE Rc(Rc&& other) noexcept
            : ptr_(other.release())
        {}

        template <typename U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
        SW_FORCEINLINE Rc(Rc<U>&& other) noexcept
            : ptr_(other.release())
        {}

        SW_FORCEINLINE Rc(const Rc& other) noexcept
            : ptr_(other.get())
        {
            if (ptr_)
                ptr_->AddRef();
        }

        template <typename U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
        SW_FORCEINLINE Rc(const Rc<U>& other) noexcept
            : ptr_(other.get())
        {
            if (ptr_)
                ptr_->AddRef();
        }

        SW_FORCEINLINE ~Rc()
        {
            if (ptr_)
                ptr_->Release();
        }

        SW_FORCEINLINE Rc& operator=(Rc&& other)
        {
            Rc(std::move(other)).swap(*this);

            return *this;
        }

        template <typename U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
        SW_FORCEINLINE Rc& operator=(Rc<U>&& other)
        {
            Rc(std::move(other)).swap(*this);

            return *this;
        }

        SW_FORCEINLINE Rc& operator=(const Rc& other)
        {
            Rc(other).swap(*this);

            return *this;
        }

        template <typename U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
        SW_FORCEINLINE Rc& operator=(const Rc<U>& other)
        {
            Rc(other).swap(*this);

            return *this;
        }

        SW_FORCEINLINE void reset() noexcept
        {
            Rc().swap(*this);
        }

        template <typename U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
        SW_FORCEINLINE void reset(U* ptr)
        {
            Rc(ptr).swap(*this);
        }

        SW_FORCEINLINE T& operator*() const noexcept
        {
            return *ptr_;
        }

        SW_FORCEINLINE T* operator->() const noexcept
        {
            return ptr_;
        }

        SW_FORCEINLINE T* get() const noexcept
        {
            return ptr_;
        }

        SW_FORCEINLINE [[nodiscard]] T* release() noexcept
        {
            return std::exchange(ptr_, nullptr);
        }

        SW_FORCEINLINE void swap(Rc& other) noexcept
        {
            std::swap(ptr_, other.ptr_);
        }

        SW_FORCEINLINE explicit operator bool() const noexcept
        {
            return ptr_ != nullptr;
        }

        template <typename U>
        SW_FORCEINLINE bool operator==(const Rc<U>& other) const noexcept
        {
            return ptr_ == other.ptr_;
        }

        template <typename U>
        SW_FORCEINLINE bool operator!=(const Rc<U>& other) const noexcept
        {
            return ptr_ != other.ptr_;
        }

        SW_FORCEINLINE bool operator==(std::nullptr_t other) const noexcept
        {
            return ptr_ == other;
        }

        SW_FORCEINLINE bool operator!=(std::nullptr_t other) const noexcept
        {
            return ptr_ != other;
        }

    private:
        T* ptr_ {};

        template <typename U>
        friend class Rc;
    };

    template <typename T, typename... Args>
    [[nodiscard]] SW_FORCEINLINE std::enable_if_t<!std::is_array_v<T>, Rc<T>> MakeRc(Args&&... args)
    {
        return Rc<T>(new T(std::forward<Args>(args)...));
    }

    template <typename T>
    [[nodiscard]] SW_FORCEINLINE std::enable_if_t<!std::is_array_v<T>, Rc<T>> AddRc(T* ptr) noexcept
    {
        if (ptr)
            ptr->AddRef();

        return Rc<T>(ptr);
    }

    template <typename T>
    Rc(T*) -> Rc<T>;
} // namespace Swage
