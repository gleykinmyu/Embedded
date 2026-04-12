#pragma once

/**
 * stf::function<R(Args...), Capacity> — вызов без heap: функтор до Capacity байт в SBO.
 * Использование: конструктор от F, emplace(F&&), operator(), reset().
 */

#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>
#include <utility>

namespace stf {

template <typename Signature, std::size_t Capacity = 16>
class function;

template <typename R, typename... Args, std::size_t Capacity>
class function<R(Args...), Capacity> {
    static_assert(!std::is_reference_v<R>, "stf::function: R must not be a reference type");

    using InvokerSig = R (*)(void*, Args...);
    using DtorSig = void (*)(void*);

    alignas(std::max_align_t) std::uint8_t storage[Capacity];
    void* ctx_ = nullptr;
    InvokerSig invoker = nullptr;
    DtorSig dtor = nullptr;

    void destroy() noexcept
    {
        if (dtor != nullptr)
            dtor(storage);
        dtor = nullptr;
        invoker = nullptr;
        ctx_ = nullptr;
    }

public:
    function() = default;

    function(const function&) = delete;
    function& operator=(const function&) = delete;
    function(function&&) = delete;
    function& operator=(function&&) = delete;

    template <typename F, typename = std::enable_if_t<!std::is_same_v<std::decay_t<F>, function>>>
    function(F&& f)
    {
        emplace(std::forward<F>(f));
    }

    template <typename F, typename = std::enable_if_t<!std::is_same_v<std::decay_t<F>, function>>>
    void emplace(F&& f)
    {
        static_assert(std::is_invocable_r_v<R, F, Args...>,
                      "F must be callable with Args... and return R");
        static_assert(sizeof(F) <= Capacity, "Functor too large for stf::function buffer");
        static_assert(alignof(F) <= alignof(std::max_align_t), "Functor over-aligned for buffer");

        destroy();

        new (static_cast<void*>(storage)) F(std::forward<F>(f));
        ctx_ = static_cast<void*>(storage);

        invoker = [](void* data, Args... args) -> R {
            return (*reinterpret_cast<F*>(data))(std::forward<Args>(args)...);
        };

        dtor = [](void* data) noexcept {
            reinterpret_cast<F*>(data)->~F();
        };
    }

    ~function() { destroy(); }

    R operator()(Args... args) const
    {
        if (invoker == nullptr) {
            if constexpr (std::is_void_v<R>)
                return;
            else
                return R{};
        }
        return invoker(ctx_, std::forward<Args>(args)...);
    }

    explicit operator bool() const noexcept { return invoker != nullptr; }

    void reset() noexcept { destroy(); }
};

} // namespace stf
