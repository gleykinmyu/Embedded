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
protected:
    const Component& _parent;
    const Literal _lexeme;

    [[nodiscard]] bool pushCmdAssignText(const char* text, cmd::assign::Text::Op op) const noexcept {
        const cmd::TargetAttr target{ _parent.name, _lexeme };
        const char* const p = text != nullptr ? text : "";
        if (!_parent.application.pushCommand(cmd::assign::Text(target, p, op))) {
            _parent.application.Error(
                "nex: Attribute: pushCommand(assign::Text) failed comp=%.*s attr=%.*s\n",
                static_cast<int>(_parent.name.len), _parent.name.data, static_cast<int>(_lexeme.len), _lexeme.data);
            return false;
        }
        return true;
    }

    [[nodiscard]] bool pushCmdAssignTextSubtract(uint32_t n) const noexcept {
        const cmd::TargetAttr target{ _parent.name, _lexeme };
        if (!_parent.application.pushCommand(cmd::assign::TextSubtract(target, n))) {
            _parent.application.Error(
                "nex: Attribute: pushCommand(assign::TextSubtract) failed comp=%.*s attr=%.*s n=%lu\n",
                static_cast<int>(_parent.name.len), _parent.name.data, static_cast<int>(_lexeme.len), _lexeme.data,
                static_cast<unsigned long>(n));
            return false;
        }
        return true;
    }

public:
    constexpr explicit Attribute(const Component& parent, const Literal& lexeme) noexcept
        : _parent(parent)
        , _lexeme(lexeme)
    {}

    Attribute(const Attribute&) = delete;
    Attribute& operator=(const Attribute&) = delete;
    Attribute(Attribute&&) = delete;
    Attribute& operator=(Attribute&&) = delete;

    virtual ~Attribute() = default;

    /** Лексема имени атрибута (левая часть `comp.attr` после точки). */
    [[nodiscard]] constexpr operator const Literal&() const noexcept { return _lexeme; }
};

/**
 * Атрибут с числовым зеркалом в MCU: `T` должен быть приводим хотя бы к одному из
 * `bool` или целых 8 / 16 бит (со знаком и без) и знакового 32 бита (`int32_t`) — как при ограничении полей NIS (`val`, `tim`, …).
 *
 * Чтение зеркала: неявное приведение к `T` (`T x = attr;`). Запись на панель и в зеркало: `attr = v` → `cmd::assign::Numeric`.
 * Запрос с панели: `get(tag = 0)` → `Application::requestNumericAttributeGet(cmd::Get::numeric(...), …)`.
 */
template<typename T>
class AttributeNum : public Attribute {
    static_assert(
        attr_detail::is_nex_numeric_storage_v<T>,
        "nex::AttributeNum<T>: T должен быть приводим к bool, целому 8 / 16 бит или int32_t");

    T _val{};

public:
    constexpr explicit AttributeNum(const Component& parent, const Literal& lexeme) noexcept
        : Attribute(parent, lexeme)
        , _val{}
    {}

    constexpr AttributeNum(const Component& parent, const Literal& lexeme, const T& initial) noexcept
        : Attribute(parent, lexeme)
        , _val(initial)
    {}

    /** Текущее значение в зеркале MCU (ответ `get` обновляется снаружи по протоколу). */
    [[nodiscard]] constexpr operator T() const noexcept { return _val; }

    /** Записать в зеркало и поставить в очередь `cmd::assign::Numeric` на панель. */
    AttributeNum& operator=(const T& v) noexcept {
        _val = v;
        const cmd::TargetAttr target{ _parent.name, _lexeme };
        if (!_parent.application.pushCommand(cmd::assign::Numeric(target, static_cast<int32_t>(v)))) {
            _parent.application.Error(
                "nex: AttributeNum: pushCommand(assign::Numeric) failed comp=%.*s attr=%.*s val=%ld\n",
                static_cast<int>(_parent.name.len), _parent.name.data, static_cast<int>(_lexeme.len), _lexeme.data,
                static_cast<long>(static_cast<int32_t>(v)));
        }
        return *this;
    }

    /** Запрос значения с панели: `get <comp>.<attr>`; ответ — в `_val`; `tag` — для сопоставления с ответом 0x71. */
    [[nodiscard]] bool get(uint8_t tag = 0) noexcept {
        const cmd::TargetAttr target{ _parent.name, _lexeme };
        if (!_parent.application.requestNumericAttributeGet(
                cmd::Get::numeric(target), reinterpret_cast<uint8_t*>(&_val), sizeof(T), tag)) {
            _parent.application.Error(
                "nex: AttributeNum: requestNumericAttributeGet failed comp=%.*s attr=%.*s\n",
                static_cast<int>(_parent.name.len), _parent.name.data, static_cast<int>(_lexeme.len), _lexeme.data);
            return false;
        }
        return true;
    }

    AttributeNum(const AttributeNum&) = delete;
    AttributeNum& operator=(const AttributeNum&) = delete;
    AttributeNum(AttributeNum&&) = delete;
    AttributeNum& operator=(AttributeNum&&) = delete;
};

/**
 * Строковый атрибут NIS, параметр `MaxL` (`uint8_t`):
 * - **`MaxL == 0`** — см. `AttributeString<0>`: без зеркала, без `get`; только `cmd::assign::Text` / `TextSubtract` на шину.
 * - **`MaxL > 0`** — зеркало `buf[MaxL]`, `*attr`, `set` / `append` / `subtract`, `get(tag = 0)` → `Application::requestStringAttributeGet(cmd::Get::string(...), …)`.
 */
template<uint8_t MaxL>
class AttributeString : public Attribute {
    static_assert(MaxL > 0u, "nex::AttributeString<MaxL>: для MaxL > 0 используйте этот шаблон; для MaxL == 0 — AttributeString<0>");

public:
    static constexpr uint8_t maxl = MaxL;

    char buf[MaxL]{};

    constexpr explicit AttributeString(const Component& parent, const Literal& lexeme) noexcept
        : Attribute(parent, lexeme)
    {}

    [[nodiscard]] const char* operator*() const noexcept { return buf; }
    [[nodiscard]] char* operator*() noexcept { return buf; }

    /** `cmd::assign::Text` с `Op::Assign`: зеркало = `text` (NUL-terminated), затем отправка. */
    [[nodiscard]] bool set(const char* text) noexcept {
        if (text == nullptr) {
            buf[0] = '\0';
            return pushCmdAssignText("", cmd::assign::Text::Op::Assign);
        }
        std::strncpy(buf, text, static_cast<std::size_t>(MaxL));
        buf[MaxL - 1u] = '\0';
        return pushCmdAssignText(buf, cmd::assign::Text::Op::Assign);
    }

    /** `cmd::assign::TextSubtract`: укоротить зеркало и строку на панели на `n` символов с конца. */
    [[nodiscard]] bool subtract(uint32_t n) noexcept {
        if (n == 0u)
            return true;
        const std::size_t len = std::strlen(buf);
        if (len <= static_cast<std::size_t>(n))
            buf[0] = '\0';
        else
            buf[len - static_cast<std::size_t>(n)] = '\0';
        return pushCmdAssignTextSubtract(n);
    }

    /** Запрос строки с панели: `get <comp>.<attr>`; ответ — в `buf`; `tag` — для сопоставления с ответом 0x70. */
    [[nodiscard]] bool get(uint8_t tag = 0) noexcept {
        const cmd::TargetAttr target{ _parent.name, _lexeme };
        if (!_parent.application.requestStringAttributeGet(
                cmd::Get::string(target), buf, static_cast<uint32_t>(MaxL), tag)) {
            _parent.application.Error(
                "nex: AttributeString: requestStringAttributeGet failed comp=%.*s attr=%.*s\n",
                static_cast<int>(_parent.name.len), _parent.name.data, static_cast<int>(_lexeme.len), _lexeme.data);
            return false;
        }
        return true;
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

    constexpr explicit AttributeString(const Component& parent, const Literal& lexeme) noexcept
        : Attribute(parent, lexeme)
    {}

    /** `cmd::assign::Text` с `Op::Assign`; `text` — NUL-terminated (или `nullptr` → пустая строка). */
    [[nodiscard]] bool set(const char* text) const noexcept {
        const char* const p = text != nullptr ? text : "";
        return pushCmdAssignText(p, cmd::assign::Text::Op::Assign);
    }

    /** `cmd::assign::Text` с `Op::Append`; `suffix` — NUL-terminated. */
    [[nodiscard]] bool append(const char* suffix) const noexcept {
        if (suffix == nullptr || *suffix == '\0')
            return true;
        return pushCmdAssignText(suffix, cmd::assign::Text::Op::Append);
    }

    /** `cmd::assign::TextSubtract`. */
    [[nodiscard]] bool subtract(uint32_t n) const noexcept {
        if (n == 0u)
            return true;
        return pushCmdAssignTextSubtract(n);
    }

    AttributeString(const AttributeString&) = delete;
    AttributeString& operator=(const AttributeString&) = delete;
    AttributeString(AttributeString&&) = delete;
    AttributeString& operator=(AttributeString&&) = delete;
};

} // namespace nex
