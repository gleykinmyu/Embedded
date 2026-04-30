#pragma once
#include <cstddef>
#include <cstring>

// ==========================================================
// 1. ИНТЕРФЕЙСЫ
// ==========================================================

struct StringData {
    size_t length;
    char buffer[1];
};

class IReadable {
protected:
    const StringData& dataRef;

    explicit constexpr IReadable(const StringData& ref) : dataRef(ref) {}

public:
    constexpr const char* getData() const { return dataRef.buffer; }

    constexpr size_t getSize() const { return dataRef.length; }
};

// Интерфейс для записи: хранит capacity и умеет менять StringData.
class IWritable : public IReadable {
protected:
    constexpr IWritable(StringData& ref, size_t cap)
        : IReadable(ref), capacity(cap) {}

public:
    const size_t capacity = 0;
        
    void clear() {
        if (capacity == 0) return;
        auto& writable = const_cast<StringData&>(dataRef);
        writable.buffer[0] = '\0';
        writable.length = 0;
    }

    void set(const char* str) {
        if (capacity == 0 || !str) return;
        auto& writable = const_cast<StringData&>(dataRef);
        std::strncpy(writable.buffer, str, capacity - 1);
        writable.buffer[capacity - 1] = '\0';
        writable.length = std::strlen(writable.buffer);
    }

    void append(const char* str) {
        if (capacity == 0 || !str) return;
        auto& writable = const_cast<StringData&>(dataRef);
        if (writable.length >= capacity - 1) return;
        std::strncat(writable.buffer, str, capacity - writable.length - 1);
        writable.length = std::strlen(writable.buffer);
    }
};

// ==========================================================
// 2. РЕАЛИЗАЦИЯ ДЛЯ FLASH (ReadOnly)
// ==========================================================

class Literal final : public IReadable {
public:
    explicit constexpr Literal(const StringData& ref) : IReadable(ref) {}
    Literal() = delete;
};

// Строковый литерал как static Literal в rodata.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-string-literal-operator-template"
#endif
template<typename T, T... chars>
inline const Literal* operator""_sl() {
    struct LiteralStorage {
        size_t length;
        char buffer[sizeof...(chars) + 1];
    };
    static const LiteralStorage storage = {
        sizeof...(chars),
        {chars..., '\0'}
    };
    static const Literal view(*reinterpret_cast<const StringData*>(&storage));
    return &view;
}
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

// ==========================================================
// 3. РЕАЛИЗАЦИЯ ДЛЯ RAM (Read/Write)
// ==========================================================

template<size_t Capacity>
class RamString : public IWritable {
private:
    struct RamStorage {
        size_t length;
        char buffer[Capacity];
    } storage;

public:
    static_assert(Capacity > 1, "RamString Capacity must be greater than 1");

    RamString()
        : IWritable(*reinterpret_cast<StringData*>(&storage), Capacity), storage{0, {'\0'}} { }
};
