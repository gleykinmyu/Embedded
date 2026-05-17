#pragma once

#include "nexApplication.hpp"

#include <cstdint>
#include <cstring>
#include <type_traits>

namespace nex {

namespace attr_detail {

template<typename T>
inline constexpr bool is_nex_numeric_storage_v = std::disjunction_v<
    std::is_convertible<T, bool>,
    std::is_convertible<T, int8_t>,
    std::is_convertible<T, uint8_t>,
    std::is_convertible<T, int16_t>,
    std::is_convertible<T, uint16_t>,
    std::is_convertible<T, int32_t>>;

} // namespace attr_detail

/**
 * Имя атрибута NIS (`txt`, `val`, `bco`, …) и ссылка на родительский `Component`.
 */
class Attribute {
public:
    /** Лексема имени атрибута NIS (`txt`, `val`, …) — правая часть `comp.attr`. */
    const Literal name;
    /** Байт маршрутизации: `Component::onResponse` / `onError`, `Status::tag_2` при `Application::Status`. */
    const uint8_t tag;

    constexpr explicit Attribute(const Component& parent, const Literal& name, uint8_t tag) noexcept
        : name(name)
        , tag(tag)
        , _parent(parent)
    {}

    Attribute(const Attribute&) = delete;
    Attribute& operator=(const Attribute&) = delete;
    Attribute(Attribute&&) = delete;
    Attribute& operator=(Attribute&&) = delete;

    virtual ~Attribute() = default;

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

/**
 * Атрибут с числовым зеркалом в MCU: `T` должен быть приводим хотя бы к одному из
 * `bool` или целых 8 / 16 бит (со знаком и без) и знакового 32 бита (`int32_t`) — как при ограничении полей NIS (`val`, `tim`, …).
 *
 * Чтение зеркала: неявное приведение к `T` (`T x = attr;`). Запись на панель и в зеркало: `attr = v` → `cmd::assign::Numeric`.
 * Запрос с панели: `get()` → ответ в `Component::onResponse(tag, …)` (зеркало обновляет виджет по `tag`).
 */
template<typename T>
class AttributeNum : public Attribute {
public:
    static_assert(
        attr_detail::is_nex_numeric_storage_v<T>,
        "nex::AttributeNum<T>: T должен быть приводим к bool, целому 8 / 16 бит или int32_t");

    constexpr explicit AttributeNum(const Component& parent, const Literal& name, uint8_t tag) noexcept
        : Attribute(parent, name, tag)
        , _val{}
    {}

    constexpr AttributeNum(const Component& parent, const Literal& name, const T& initial, uint8_t tag) noexcept
        : Attribute(parent, name, tag)
        , _val(initial)
    {}

    /** Текущее значение в зеркале MCU (ответ `get` обновляется снаружи по протоколу). */
    [[nodiscard]] constexpr operator T() const noexcept { return _val; }

    /** Записать в зеркало и поставить в очередь `cmd::assign::Numeric` на панель. */
    AttributeNum& operator=(const T& v) noexcept {
        _val = v;
        const AttrRef target{ _parent.name, name };
        const cmd::assign::Numeric cmd(target, static_cast<int32_t>(v));
        enqueueTransaction(cmd);
        return *this;
    }

    /** Запрос значения с панели: `get <comp>.<attr>`; ответ — `Component::onResponse(tag, …)`. */
    void get() noexcept {
        const AttrRef target{ _parent.name, name };
        enqueueTransaction(cmd::Get::numeric(target), Transaction::State::AwaitingNumericGet);
    }

    AttributeNum(const AttributeNum&) = delete;
    AttributeNum& operator=(const AttributeNum&) = delete;
    AttributeNum(AttributeNum&&) = delete;
    AttributeNum& operator=(AttributeNum&&) = delete;

private:
    T _val{};
};

/**
 * Строковый атрибут NIS, параметр `MaxL` (`uint8_t`):
 * - **`MaxL == 0`** — см. `AttributeString<0>`: без зеркала, без `get`; только `cmd::assign::Text` / `TextSubtract` на шину.
 * - **`MaxL > 0`** — зеркало `buf[MaxL]`, `*attr`, `set` / `append` / `subtract`, `get()` → `Component::onResponse(tag, …)`.
 */
template<uint8_t MaxL>
class AttributeString : public Attribute {
    static_assert(MaxL > 0u, "nex::AttributeString<MaxL>: для MaxL > 0 используйте этот шаблон; для MaxL == 0 — AttributeString<0>");

public:
    static constexpr uint8_t maxl = MaxL;

    char buf[MaxL]{};

    constexpr explicit AttributeString(const Component& parent, const Literal& name, uint8_t tag) noexcept
        : Attribute(parent, name, tag)
    {}

    [[nodiscard]] const char* operator*() const noexcept { return buf; }
    [[nodiscard]] char* operator*() noexcept { return buf; }

    /** `cmd::assign::Text` с `Op::Assign`: зеркало = `text` (NUL-terminated), затем отправка. */
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

    /** `cmd::assign::TextSubtract`: укоротить зеркало и строку на панели на `n` символов с конца. */
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

    /** Запрос строки с панели: `get <comp>.<attr>`; ответ — `Component::onResponse(tag, …)`. */
    void get() noexcept {
        const AttrRef target{ _parent.name, name };
        enqueueTransaction(cmd::Get::string(target), Transaction::State::AwaitingStringGet);
    }

    AttributeString(const AttributeString&) = delete;
    AttributeString& operator=(const AttributeString&) = delete;
    AttributeString(AttributeString&&) = delete;
    AttributeString& operator=(AttributeString&&) = delete;
};

/**
 * Строковый атрибут без зеркала в MCU (`MaxL == 0`): только адресация; без `buf`, без `get`.
 * Команды `cmd::assign::Text` / `TextSubtract`: указатели на строки должны жить до сериализации команды.
 */
template<>
class AttributeString<0> : public Attribute {
public:
    static constexpr uint8_t maxl = 0;

    constexpr explicit AttributeString(const Component& parent, const Literal& name, uint8_t tag) noexcept
        : Attribute(parent, name, tag)
    {}

    /** `cmd::assign::Text` с `Op::Assign`; `text` — NUL-terminated (или `nullptr` → пустая строка). */
    void set(const char* text) const noexcept {
        const char* const p = text != nullptr ? text : "";
        pushCmdAssignText(p, cmd::assign::Text::Op::Assign);
    }

    /** `cmd::assign::Text` с `Op::Append`; `suffix` — NUL-terminated. */
    void append(const char* suffix) const noexcept {
        if (suffix == nullptr || *suffix == '\0')
            return;
        pushCmdAssignText(suffix, cmd::assign::Text::Op::Append);
    }

    /** `cmd::assign::TextSubtract`. */
    void subtract(uint32_t n) const noexcept {
        if (n == 0u)
            return;
        pushCmdAssignTextSubtract(n);
    }

    AttributeString(const AttributeString&) = delete;
    AttributeString& operator=(const AttributeString&) = delete;
    AttributeString(AttributeString&&) = delete;
    AttributeString& operator=(AttributeString&&) = delete;
};

} // namespace nex
