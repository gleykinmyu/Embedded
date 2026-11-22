/**
 * @file callback.hpp
 * @brief Callback<R(Args...)>: void* ctx + лямбда-метод. Вызов — operator().
 *
 * Лямбда без захвата, аргументы — auto&&. Первый — ctx, в теле это self.
 * Имена пишутся в макросе, типы берутся из Callback.
 *
 *   BIF_CB(rx, Drv, byte)
 *       self->onRx(byte);
 *   BIF_CB_END
 *   rx(byte);
 *   rx.clear();
 *
 * Пустой вызов: void — ничего; иначе R{}.
 * bind / clear не вызывать одновременно с operator()
 * (прерывание, которое зовёт callback, выключено).
 */

#pragma once

#include <new>
#include <type_traits>
#include <utility>

namespace BIF {

template <typename Sig>
class Callback;

template <typename R, typename... Args>
class Callback<R(Args...)> {
public:
    using method_t = R (*)(void* ctx, Args...) noexcept;

    Callback() = default;

    void bind(void* ctx, method_t method) noexcept
    {
        if (method == nullptr) {
            clear();
            return;
        }
        _ctx = ctx;
        _method = method;
    }

    /// Шаблонная лямбда (auto&&) не приводится к указателю. Копия лежит в Slot,
    /// наружу публикуется Slot::call с конкретной сигнатурой Callback.
    template <typename F, typename = std::enable_if_t<std::is_class_v<F>>>
    void bind(void* ctx, F fn) noexcept
    {
        static_assert(std::is_empty_v<F>, "callback lambda must not capture");
        Slot<F>::store(fn);
        _ctx = ctx;
        _method = &Slot<F>::call;
    }

    void clear() noexcept
    {
        _method = nullptr;
        _ctx = nullptr;
    }

    R operator()(Args... args) const noexcept
    {
        if (_method == nullptr) {
            if constexpr (std::is_void_v<R>) {
                return;
            } else {
                static_assert(std::is_nothrow_default_constructible_v<R>,
                    "empty Callback needs a nothrow default return");
                return R{};
            }
        }
        return _method(_ctx, args...);
    }

private:
    template <typename F>
    struct Slot {
        alignas(F) static inline unsigned char raw[sizeof(F)];
        static inline bool live = false;

        static void store(const F& fn) noexcept
        {
            if (live) {
                std::launder(reinterpret_cast<F*>(raw))->~F();
            }
            new (raw) F(fn);
            live = true;
        }

        static R call(void* ctx, Args... args) noexcept
        {
            return (*std::launder(reinterpret_cast<F*>(raw)))(
                ctx, std::forward<Args>(args)...);
        }
    };

    void* _ctx = nullptr;
    method_t _method = nullptr;
};

} // namespace BIF

/**
 * bind(this, шаблонная лямбда). В теле — self типа Type*.
 * После Type — только имена аргументов, до 8. Тип берётся из Callback.
 *
 *   BIF_CB(rx, Drv, byte)
 *       self->onRx(byte);
 *   BIF_CB_END
 */
#define BIF_CB_NARG(...) BIF_CB_NARG_I(0, ##__VA_ARGS__, 8, 7, 6, 5, 4, 3, 2, 1, 0)
#define BIF_CB_NARG_I(_0, _1, _2, _3, _4, _5, _6, _7, _8, N, ...) N
#define BIF_CB_CAT_I(a, b) a##b
#define BIF_CB_CAT(a, b) BIF_CB_CAT_I(a, b)
#define BIF_CB_P0()
#define BIF_CB_P1(a) , auto&& a
#define BIF_CB_P2(a, b) , auto&& a, auto&& b
#define BIF_CB_P3(a, b, c) , auto&& a, auto&& b, auto&& c
#define BIF_CB_P4(a, b, c, d) , auto&& a, auto&& b, auto&& c, auto&& d
#define BIF_CB_P5(a, b, c, d, e) , auto&& a, auto&& b, auto&& c, auto&& d, auto&& e
#define BIF_CB_P6(a, b, c, d, e, f) , auto&& a, auto&& b, auto&& c, auto&& d, auto&& e, auto&& f
#define BIF_CB_P7(a, b, c, d, e, f, g) \
    , auto&& a, auto&& b, auto&& c, auto&& d, auto&& e, auto&& f, auto&& g
#define BIF_CB_P8(a, b, c, d, e, f, g, h) \
    , auto&& a, auto&& b, auto&& c, auto&& d, auto&& e, auto&& f, auto&& g, auto&& h
#define BIF_CB_PARAMS(...) BIF_CB_CAT(BIF_CB_P, BIF_CB_NARG(__VA_ARGS__))(__VA_ARGS__)

#define BIF_CB(cb, Type, ...)                                      \
    (cb).bind(this, [](void* ctx BIF_CB_PARAMS(__VA_ARGS__)) noexcept { \
        auto* self = static_cast<Type*>(ctx);

#define BIF_CB_END                                                 \
    });
