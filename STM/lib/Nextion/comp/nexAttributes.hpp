#pragma once

#include "../app/nexApplication.hpp"

#include <cstdint>
#include <cstring>
#include <type_traits>

namespace nex {

namespace attr_detail {

template<typename T>
struct is_color_storage : std::false_type {};

template<>
struct is_color_storage<Color> : std::true_type {};

template<typename T>
struct is_halign_storage : std::false_type {};

template<>
struct is_halign_storage<HAlign> : std::true_type {};

template<typename T>
struct is_valign_storage : std::false_type {};

template<>
struct is_valign_storage<VAlign> : std::true_type {};

template<typename T>
struct is_number_format_storage : std::false_type {};

template<>
struct is_number_format_storage<NumFormat> : std::true_type {};

template<typename T>
inline constexpr bool is_nex_numeric_storage_v = std::disjunction_v<
    std::is_convertible<T, bool>,
    std::is_convertible<T, int8_t>,
    std::is_convertible<T, uint8_t>,
    std::is_convertible<T, int16_t>,
    std::is_convertible<T, uint16_t>,
    std::is_convertible<T, int32_t>,
    is_color_storage<T>,
    is_halign_storage<T>,
    is_valign_storage<T>,
    is_number_format_storage<T>>;

template<typename T>
[[nodiscard]] constexpr T from_wire(int32_t wire) noexcept {
    return static_cast<T>(wire);
}

template<>
[[nodiscard]] constexpr Color from_wire<Color>(int32_t wire) noexcept {
    return Color(static_cast<uint16_t>(wire));
}

template<>
[[nodiscard]] constexpr HAlign from_wire<HAlign>(int32_t wire) noexcept {
    return static_cast<HAlign>(wire);
}

template<>
[[nodiscard]] constexpr VAlign from_wire<VAlign>(int32_t wire) noexcept {
    return static_cast<VAlign>(wire);
}

template<>
[[nodiscard]] constexpr NumFormat from_wire<NumFormat>(int32_t wire) noexcept {
    return static_cast<NumFormat>(wire);
}

template<typename T>
[[nodiscard]] constexpr int32_t to_wire(const T& v) noexcept {
    return static_cast<int32_t>(v);
}

/** MCU: `assign` числового атрибута без зеркала (только исходящая команда). */
template<typename T>
inline void assignNumeric(const Component& parent, const Literal& attrName, uint8_t tag, T value) noexcept
{
    const AttrRef target{parent.name, attrName};
    const cmd::assign::Numeric cmd(target, to_wire(value));
    parent.page.app.enqueue(Transaction{cmd, parent.page.ID, parent.id(), tag});
}

template<>
[[nodiscard]] constexpr int32_t to_wire(const Color& v) noexcept {
    return static_cast<int32_t>(v.raw);
}

template<>
[[nodiscard]] constexpr int32_t to_wire(const HAlign& v) noexcept {
    return static_cast<int32_t>(static_cast<uint8_t>(v));
}

template<>
[[nodiscard]] constexpr int32_t to_wire(const VAlign& v) noexcept {
    return static_cast<int32_t>(static_cast<uint8_t>(v));
}

template<>
[[nodiscard]] constexpr int32_t to_wire(const NumFormat& v) noexcept {
    return static_cast<int32_t>(static_cast<uint8_t>(v));
}

/** Копия ответа `get` (0x70) в зеркало `buf[buf_cap]` (NUL на `buf_cap - 1`). */
inline void copy_string_mirror(char* buf, uint16_t buf_cap, const msg::getString& response) noexcept {
    if (buf_cap == 0u)
        return;
    const std::size_t cap = static_cast<std::size_t>(buf_cap) - 1u;
    const std::size_t n = static_cast<std::size_t>(response.length);
    const std::size_t copy = (n < cap) ? n : cap;
    if (copy > 0u)
        std::memcpy(buf, response.chars, copy);
    buf[copy] = '\0';
}

} // namespace attr_detail

/**
 * Атрибуты NIS, привязанные к `Component`: `attr::Num`, `attr::NumRO`, `attr::String`, `attr::StringRO`.
 *
 * У каждого поля виджета — комментарий категории:
 * - `user` — публичное поле `attr::Num` / `attr::String` (касание, ползунок, ввод с клавиатуры).
 * - `mcu` (числовой) — только `set…()` с `attr_detail::assignNumeric` (без чтения зеркала).
 *   `attr::String` и `attr::NumRO` не трогаем. `x`, `y` — поля `attr::Num`.
 */
namespace attr {

/** Имя атрибута NIS (`txt`, `val`, …) и ссылка на родительский `Component`. */
class Base {
public:
    const Literal name;
    const uint8_t tag;

    constexpr explicit Base(const Component& parent, const Literal& name, uint8_t tag) noexcept
        : name(name)
        , tag(tag)
        , _parent(parent)
    {}

    Base(const Base&) = delete;
    Base& operator=(const Base&) = delete;
    Base(Base&&) = delete;
    Base& operator=(Base&&) = delete;

    virtual ~Base() = default;

    [[nodiscard]] constexpr operator const Literal&() const noexcept { return name; }

protected:
    const Component& _parent;

    void pushCmdAssignText(const char* text, cmd::assign::Text::Op op) const noexcept;
    void pushCmdAssignTextSubtract(uint32_t n) const noexcept;

    void enqueueTransaction(const Command& cmd,
        Transaction::State state = Transaction::State::AwaitingStatus) const noexcept {
        _parent.page.app.enqueue(Transaction{cmd, _parent.page.ID, _parent.id(), tag, state});
    }
};

/** Числовой атрибут: зеркало в MCU, `operator=` → `cmd::assign::Numeric`, `get()` → `applyResponse`. */
template<typename T>
class Num : public Base {
public:
    static_assert(
        attr_detail::is_nex_numeric_storage_v<T>,
        "nex::attr::Num<T>: T — bool, целое 8/16/32 бит или nex::Color");

    constexpr explicit Num(const Component& parent, const Literal& name, uint8_t tag) noexcept
        : Base(parent, name, tag)
        , _val{}
    {}

    constexpr Num(const Component& parent, const Literal& name, const T& initial, uint8_t tag) noexcept
        : Base(parent, name, tag)
        , _val(initial)
    {}

    [[nodiscard]] constexpr operator T() const noexcept { return _val; }

    void applyResponse(const msg::getNumeric& response) noexcept {
        _val = attr_detail::from_wire<T>(response.value);
    }

    Num& operator=(const T& v) noexcept {
        _val = v;
        const AttrRef target{ _parent.name, name };
        const cmd::assign::Numeric cmd(target, attr_detail::to_wire(v));
        enqueueTransaction(cmd);
        return *this;
    }

    void get() noexcept {
        const AttrRef target{ _parent.name, name };
        enqueueTransaction(cmd::Get::numeric(target), Transaction::State::AwaitingNumericGet);
    }

    Num(const Num&) = delete;
    Num& operator=(const Num&) = delete;
    Num(Num&&) = delete;
    Num& operator=(Num&&) = delete;

private:
    T _val{};
};

/** Числовой атрибут RO в NIS: зеркало и `get()`, без `operator=`. */
template<typename T>
class NumRO : public Base {
public:
    static_assert(
        attr_detail::is_nex_numeric_storage_v<T>,
        "nex::attr::NumRO<T>: T должен быть приводим к bool, целому 8 / 16 бит или int32_t");

    constexpr explicit NumRO(const Component& parent, const Literal& name, uint8_t tag) noexcept
        : Base(parent, name, tag)
        , _val{}
    {}

    constexpr NumRO(const Component& parent, const Literal& name, const T& initial, uint8_t tag) noexcept
        : Base(parent, name, tag)
        , _val(initial)
    {}

    [[nodiscard]] constexpr operator T() const noexcept { return _val; }

    void applyResponse(const msg::getNumeric& response) noexcept {
        _val = attr_detail::from_wire<T>(response.value);
    }

    void get() noexcept {
        const AttrRef target{ _parent.name, name };
        enqueueTransaction(cmd::Get::numeric(target), Transaction::State::AwaitingNumericGet);
    }

    NumRO(const NumRO&) = delete;
    NumRO& operator=(const NumRO&) = delete;
    NumRO(NumRO&&) = delete;
    NumRO& operator=(NumRO&&) = delete;

private:
    T _val{};
};

/**
 * Строковый атрибут (`MaxL > 0`): зеркало `buf[MaxL]`, `set` / `subtract`, `get()`.
 * Для `MaxL == 0` — `attr::String<0>`: без зеркала и без `get`.
 */
template<uint16_t MaxL>
class String : public Base {
    static_assert(MaxL > 0u, "nex::attr::String<MaxL>: для MaxL > 0; для MaxL == 0 — attr::String<0>");

public:
    static constexpr uint16_t maxl = MaxL;

    char buf[MaxL]{};

    constexpr explicit String(const Component& parent, const Literal& name, uint8_t tag) noexcept
        : Base(parent, name, tag)
    {}

    [[nodiscard]] const char* operator*() const noexcept { return buf; }
    [[nodiscard]] char* operator*() noexcept { return buf; }

    void set(const char* text) noexcept {
        if (text == nullptr) {
            buf[0] = '\0';
            pushCmdAssignText("", cmd::assign::Text::Op::Assign);
            return;
        }
        std::strncpy(buf, text, static_cast<std::size_t>(MaxL));
        buf[MaxL - 1u] = '\0';
        pushCmdAssignText(buf, cmd::assign::Text::Op::Assign);
    }

    void subtract(uint32_t n) noexcept {
        if (n == 0u)
            return;
        const std::size_t len = std::strlen(buf);
        if (len <= static_cast<std::size_t>(n))
            buf[0] = '\0';
        else
            buf[len - static_cast<std::size_t>(n)] = '\0';
        pushCmdAssignTextSubtract(n);
    }

    void applyResponse(const msg::getString& response) noexcept {
        attr_detail::copy_string_mirror(buf, MaxL, response);
    }

    void get() noexcept {
        const AttrRef target{ _parent.name, name };
        enqueueTransaction(cmd::Get::string(target), Transaction::State::AwaitingStringGet);
    }

    String(const String&) = delete;
    String& operator=(const String&) = delete;
    String(String&&) = delete;
    String& operator=(String&&) = delete;
};

/** Строковый атрибут RO в NIS: зеркало `buf[MaxL]`, только `get()`. */
template<uint16_t MaxL>
class StringRO : public Base {
    static_assert(MaxL > 0u, "nex::attr::StringRO<MaxL>: MaxL должен быть > 0");

public:
    static constexpr uint16_t maxl = MaxL;

    char buf[MaxL]{};

    constexpr explicit StringRO(const Component& parent, const Literal& name, uint8_t tag) noexcept
        : Base(parent, name, tag)
    {}

    [[nodiscard]] const char* operator*() const noexcept { return buf; }

    void applyResponse(const msg::getString& response) noexcept {
        attr_detail::copy_string_mirror(buf, MaxL, response);
    }

    void get() noexcept {
        const AttrRef target{ _parent.name, name };
        enqueueTransaction(cmd::Get::string(target), Transaction::State::AwaitingStringGet);
    }

    StringRO(const StringRO&) = delete;
    StringRO& operator=(const StringRO&) = delete;
    StringRO(StringRO&&) = delete;
    StringRO& operator=(StringRO&&) = delete;
};

/** Строковый атрибут без зеркала (`MaxL == 0`): только `set` / `append` / `subtract` на шину. */
template<>
class String<0> : public Base {
public:
    static constexpr uint8_t maxl = 0;

    constexpr explicit String(const Component& parent, const Literal& name, uint8_t tag) noexcept
        : Base(parent, name, tag)
    {}

    void set(const char* text) const noexcept {
        const char* const p = text != nullptr ? text : "";
        pushCmdAssignText(p, cmd::assign::Text::Op::Assign);
    }

    void append(const char* suffix) const noexcept {
        if (suffix == nullptr || *suffix == '\0')
            return;
        pushCmdAssignText(suffix, cmd::assign::Text::Op::Append);
    }

    void subtract(uint32_t n) const noexcept {
        if (n == 0u)
            return;
        pushCmdAssignTextSubtract(n);
    }

    String(const String&) = delete;
    String& operator=(const String&) = delete;
    String(String&&) = delete;
    String& operator=(String&&) = delete;
};

} // namespace attr

} // namespace nex
