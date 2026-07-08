#pragma once

#include "nexComponentBase.hpp"
#include "../app/nexApplication.hpp"
#include "nexAttrLexemes.hpp"

#include <cstdint>
#include <cstring>

/**
 * Поля атрибутов виджетов: `attr::Num`, `attr::String`, зеркала и `get`/`assign`.
 * Категории в листьях: `user` (поле) vs `mcu` (`setPeriod`, …).
 */

namespace nex {

namespace attr_detail {

/** MCU: `assign` числового атрибута без зеркала (только исходящая команда). */
template<typename T>
inline void assignNumeric(const Component& parent, attr::Id id, T value) noexcept
{
    const AttrRef target{parent.name, attr::literal(id)};
    const cmd::assign::Numeric cmd(target, wire::toWire(value));
    parent.page.app.enqueue(
        Transaction{cmd, parent.page.ID, parent.id(), static_cast<uint8_t>(id), Transaction::Kind::Command,
            msg::kAwaitingNone});
}

/** MCU: `assign` строкового атрибута без зеркала (только исходящая команда). */
inline void assignText(const Component& parent, attr::Id id, const char* text) noexcept
{
    const AttrRef target{parent.name, attr::literal(id)};
    const char* const p = text != nullptr ? text : "";
    parent.page.app.enqueue(Transaction{
        cmd::assign::Text(target, p, cmd::assign::Text::Op::Assign),
        parent.page.ID, parent.id(), static_cast<uint8_t>(id), Transaction::Kind::Command, msg::kAwaitingNone});
}

/** MCU: `append` строкового атрибута без зеркала (NIS `+=`, только исходящая команда). */
inline void appendText(const Component& parent, attr::Id id, const char* text) noexcept
{
    if (text == nullptr || *text == '\0')
        return;
    const AttrRef target{parent.name, attr::literal(id)};
    parent.page.app.enqueue(Transaction{
        cmd::assign::Text(target, text, cmd::assign::Text::Op::Append),
        parent.page.ID, parent.id(), static_cast<uint8_t>(id), Transaction::Kind::Command, msg::kAwaitingNone});
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
 * - `mcu` (числовой) — метод `set<Role>()` / `enable…()` / `disable…()` через `assignNumeric` (без зеркала).
 *   Имена по смыслу UI, не NIS (`setPeriod`, `font.setTextColor`, не `setTim` / `setPco`).
 *   `attr::String` и `attr::NumRO` — поля; `x`, `y` — поля `attr::Num`.
 */
namespace attr {

/** Идентификатор атрибута NIS (`txt`, `val`, …) и ссылка на родительский `Component`. */
class Base {
public:
    const Id id;

    constexpr explicit Base(const Component& parent, Id id) noexcept
        : id(id)
        , _parent(parent)
    {}

    Base(const Base&) = delete;
    Base& operator=(const Base&) = delete;
    Base(Base&&) = delete;
    Base& operator=(Base&&) = delete;

    virtual ~Base() = default;

    [[nodiscard]] constexpr const Literal& name() const noexcept { return literal(id); }
    [[nodiscard]] constexpr operator const Literal&() const noexcept { return name(); }
    [[nodiscard]] constexpr uint8_t tag() const noexcept { return static_cast<uint8_t>(id); }

protected:
    const Component& _parent;

    void pushCmdAssignText(const char* text, cmd::assign::Text::Op op) const noexcept;
    void pushCmdAssignTextSubtract(uint32_t n) const noexcept;

    void enqueueTransaction(const Command& cmd, Transaction::Kind kind = Transaction::Kind::Command,
        msg::Status::Mask awaiting_status = msg::kAwaitingAllPanel) const noexcept {
        _parent.page.app.enqueue(
            Transaction{cmd, _parent.page.ID, _parent.id(), tag(), kind, awaiting_status});
    }
};

/** Числовой атрибут: зеркало в MCU, `operator=` → `cmd::assign::Numeric`, `get()` → `applyResponse`. */
template<typename T>
class Num : public Base {
public:
    static_assert(
        wire::is_attr_numeric_v<T>,
        "nex::attr::Num<T>: T — bool, целое 8/16/32 бит, nex::Color или enum class : uint8_t");

    constexpr explicit Num(const Component& parent, Id id) noexcept
        : Base(parent, id)
        , _val{}
    {}

    [[nodiscard]] constexpr operator T() const noexcept { return _val; }

    void applyResponse(const msg::getNumeric& response) noexcept {
        _val = wire::fromWire<T>(response.value);
    }

    Num& operator=(const T& v) noexcept {
        _val = v;
        const AttrRef target{ _parent.name, name() };
        const cmd::assign::Numeric cmd(target, wire::toWire(v));
        enqueueTransaction(cmd, Transaction::Kind::Command, msg::kAwaitingNone);
        return *this;
    }

    void get() noexcept {
        const AttrRef target{ _parent.name, name() };
        enqueueTransaction(cmd::Get::numeric(target), Transaction::Kind::GetNumeric);
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
        wire::is_attr_numeric_v<T>,
        "nex::attr::NumRO<T>: T — bool, целое 8/16/32 бит, nex::Color или enum class : uint8_t");

    constexpr explicit NumRO(const Component& parent, Id id) noexcept
        : Base(parent, id)
        , _val{}
    {}

    [[nodiscard]] constexpr operator T() const noexcept { return _val; }

    void applyResponse(const msg::getNumeric& response) noexcept {
        _val = wire::fromWire<T>(response.value);
    }

    void get() noexcept {
        const AttrRef target{ _parent.name, name() };
        enqueueTransaction(cmd::Get::numeric(target), Transaction::Kind::GetNumeric);
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

    constexpr explicit String(const Component& parent, Id id) noexcept
        : Base(parent, id)
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

    void clear() noexcept { set(""); }

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
        const AttrRef target{ _parent.name, name() };
        enqueueTransaction(cmd::Get::string(target), Transaction::Kind::GetString);
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

    constexpr explicit StringRO(const Component& parent, Id id) noexcept
        : Base(parent, id)
    {}

    [[nodiscard]] const char* operator*() const noexcept { return buf; }

    void applyResponse(const msg::getString& response) noexcept {
        attr_detail::copy_string_mirror(buf, MaxL, response);
    }

    void get() noexcept {
        const AttrRef target{ _parent.name, name() };
        enqueueTransaction(cmd::Get::string(target), Transaction::Kind::GetString);
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

    constexpr explicit String(const Component& parent, Id id) noexcept
        : Base(parent, id)
    {}

    void set(const char* text) const noexcept {
        const char* const p = text != nullptr ? text : "";
        pushCmdAssignText(p, cmd::assign::Text::Op::Assign);
    }

    void clear() const noexcept { set(""); }

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
