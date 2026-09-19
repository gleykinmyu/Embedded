/**
 * @file show_model.hpp
 * @brief Шоуфайл: формат на диске + ISection / Section<Rec, N> / ShowFile<SecN>.
 *
 * Секции — члены наследника ShowFile<SecN>: конструктор берёт IShowFile& и регистрируется.
 * Порядок объявления членов = порядок записи.
 * load/save — на IShowFile (обход реестра). CRC/magic — отказ; секции/слоты — Mismatch.
 * edited — на IShowFile; секция помечает через ISection::markEdited() при правке payload.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "iFileSystem.hpp"
#include "obj_registry.hpp"

namespace smcp {
namespace file {

class IShowFile;
class ISection;

using BIF::IFile;

inline constexpr uint32_t kMagic = 0x534D4350u; /**< "SMCP". */
inline constexpr uint16_t kVersion = 0x0115u;
inline constexpr std::size_t kCrcChunkSize = 64u;

/** Полный путь шоу: basename + запас под префикс тома (`0:/…`). */
inline constexpr std::size_t kPathPrefixRoom = 16u;
inline constexpr std::size_t kPathSize = BIF::kDirNameSize + kPathPrefixRoom;

inline constexpr uint8_t kMaxMismatches = 8u;

enum class Status : uint8_t {
    Ok = 0,
    IoError,
    BadMagic,
    BadVersion,
    BadHeaderCrc,
    BadBodyCrc,
    BadLayout,
    Truncated,
};

[[nodiscard]] inline const char* cstr(Status s) noexcept
{
    switch (s) {
    case Status::Ok: return "Ok";
    case Status::IoError: return "IoError";
    case Status::BadMagic: return "BadMagic";
    case Status::BadVersion: return "BadVersion";
    case Status::BadHeaderCrc: return "BadHeaderCrc";
    case Status::BadBodyCrc: return "BadBodyCrc";
    case Status::BadLayout: return "BadLayout";
    case Status::Truncated: return "Truncated";
    default: return "?";
    }
}

struct Header {
    char name[kPathSize]{};
    uint32_t magic = kMagic;
    uint16_t version = kVersion;
    uint16_t content_layers = 0;
    uint16_t section_count = 0;
    uint16_t header_crc16 = 0;
    uint32_t body_crc32 = 0;
    uint32_t total_size = 0;

    /** Смещение первого payload: sizeof(Header) + catalog. */
    [[nodiscard]] constexpr std::size_t payloadBase() const noexcept;
};

static_assert(sizeof(Header) == kPathSize + 20u);
static_assert(alignof(Header) == alignof(uint32_t));

/** Запись каталога на носителе. */
struct SectionDesc {
    uint32_t tag = 0;
    uint32_t offset = 0;
    uint32_t byte_size = 0;
    uint32_t record_count = 0;

    [[nodiscard]] constexpr std::size_t payloadSize(std::size_t record_size) const noexcept
    {
        return record_size * static_cast<std::size_t>(record_count);
    }
    [[nodiscard]] constexpr std::size_t endOffset() const noexcept
    {
        return static_cast<std::size_t>(offset) + static_cast<std::size_t>(byte_size);
    }
};

static_assert(sizeof(SectionDesc) == 16u);
static_assert(alignof(SectionDesc) == alignof(uint32_t));

constexpr std::size_t Header::payloadBase() const noexcept
{
    return sizeof(Header) + static_cast<std::size_t>(section_count) * sizeof(SectionDesc);
}

[[nodiscard]] uint16_t computeHeaderCrc16(const Header& hdr) noexcept;
[[nodiscard]] bool computeFileCrc32(IFile& io, std::size_t offset, std::size_t length,
                                    uint32_t& out_crc) noexcept;
[[nodiscard]] Status checkHeader(const Header& hdr) noexcept;
[[nodiscard]] Status checkFileSize(const Header& hdr, std::size_t fileSize) noexcept;
[[nodiscard]] Status checkSectionDesc(const SectionDesc& desc, std::size_t expectedOffset,
                                      uint32_t totalSize) noexcept;

namespace detail {
void registerSection(IShowFile& file, ISection& sec) noexcept;
} // namespace detail

enum class Diff : uint8_t {
    MissingSection = 0,
    ExtraSection,
    FewerSlots,
    ExtraSlots,
    SlotSizeMismatch,
};

struct Mismatch {
    Diff kind = Diff::MissingSection;
    uint32_t tag = 0;
    uint16_t expected = 0;
    uint16_t found = 0;
};

/**
 * Секция каталога. Тег FourCC — поле, не id реестра.
 * Конструктор принимает IShowFile& и регистрируется сам.
 * Payload — у наследника (Section<Rec, N>: Rec[N]).
 */
class ISection {
public:
    /** @a data — начало payload наследника (массив Rec[N]). */
    ISection(IShowFile& file, uint32_t tag, uint16_t slotCount, uint16_t slotSize, bool required,
             void* data) noexcept;

    ISection(const ISection&) = delete;
    ISection& operator=(const ISection&) = delete;
    virtual ~ISection() = default;

    /** Сбросить payload секции (каждая реализация по-своему). */
    virtual void clearData() noexcept = 0;
    /** Проверка содержимого; по умолчанию ок. */
    [[nodiscard]] virtual bool isValid() const noexcept { return true; }

    /** Владелец секции. */
    [[nodiscard]] IShowFile& showFile() noexcept { return _file; }
    [[nodiscard]] const IShowFile& showFile() const noexcept { return _file; }
    /** Пометить шоуфайл edited. */
    void markEdited() noexcept;

    [[nodiscard]] uint8_t id() const noexcept { return _id; }
    /** FourCC секции на диске. */
    [[nodiscard]] uint32_t tag() const noexcept { return _tag; }
    [[nodiscard]] uint16_t slotCount() const noexcept { return _slotCount; }
    [[nodiscard]] uint16_t slotSize() const noexcept { return _slotSize; }
    [[nodiscard]] bool required() const noexcept { return _required; }

    /** Каталожная запись с диска (tag==0 — секции не было в файле). */
    [[nodiscard]] const SectionDesc& desc() const noexcept { return _desc; }
    /** Запомнить desc с диска (load). */
    void setDesc(const SectionDesc& d) noexcept { _desc = d; }
    /** tag=0: секции в файле не было. */
    void clearDesc() noexcept { _desc = {}; }
    /** Собрать desc под текущий payload, начиная с @a offset. */
    void setDesc(std::size_t offset) noexcept
    {
        _desc.tag = _tag;
        _desc.offset = static_cast<uint32_t>(offset);
        _desc.byte_size = static_cast<uint32_t>(payloadBytes());
        _desc.record_count = _slotCount;
    }

    /** slotCount × slotSize. */
    [[nodiscard]] std::size_t payloadBytes() const noexcept
    {
        return static_cast<std::size_t>(_slotCount) * static_cast<std::size_t>(_slotSize);
    }
    /** Указатель на первый байт payload. */
    [[nodiscard]] const uint8_t* payload() const noexcept { return &_data; }

    /** Прочитать min(n, slotCount) записей в payload. */
    [[nodiscard]] bool readPayload(IFile& io, uint16_t n) noexcept
    {
        const uint16_t m = (n < _slotCount) ? n : _slotCount;
        return io.read(&_data, static_cast<std::size_t>(m) * static_cast<std::size_t>(_slotSize));
    }
    /** Записать весь payload. */
    [[nodiscard]] bool writePayload(IFile& io) const noexcept
    {
        return io.write(&_data, payloadBytes());
    }

    /** Payload + desc. Только секция той же раскладки (тег и размер). */
    [[nodiscard]] bool copyFrom(const ISection& src) noexcept;

private:
    template <typename, typename>
    friend class MISC::ObjRegistry;

    /** Id реестра; вызывает ObjRegistry при register. */
    void set_id(uint8_t id) noexcept { _id = id; }

    uint8_t _id = 0;
    bool _required = false;
    uint16_t _slotCount = 0;
    uint16_t _slotSize = 0;
    uint32_t _tag = 0;
    SectionDesc _desc{};
    uint8_t& _data;
    IShowFile& _file;
};

class IShowFile {
public:
    IShowFile(const IShowFile&) = delete;
    IShowFile& operator=(const IShowFile&) = delete;
    virtual ~IShowFile() = default;

    /** Сбросить имя, mismatches и payload всех секций. edited не трогает. */
    virtual void clearData() noexcept;
    /** Все секции isValid(). */
    [[nodiscard]] virtual bool isValid() const noexcept;

    [[nodiscard]] uint16_t sectionCount() const noexcept
    {
        return static_cast<uint16_t>(_sections.registeredCount());
    }
    /** Секция по id реестра; нет — nullptr. */
    [[nodiscard]] ISection* section(uint8_t id) noexcept { return _sections.get(id); }
    [[nodiscard]] const ISection* section(uint8_t id) const noexcept
    {
        return const_cast<IShowFile*>(this)->_sections.get(id);
    }
    /** Секция по FourCC. */
    [[nodiscard]] ISection* find(uint32_t tag) noexcept;
    [[nodiscard]] const ISection* find(uint32_t tag) const noexcept;

    /** Полный путь / имя в заголовке файла. */
    void setName(const char* name) noexcept;
    [[nodiscard]] const char* name() const noexcept { return _name; }

    [[nodiscard]] bool isEdited() const noexcept { return _edited; }
    void markEdited() noexcept { _edited = true; }
    /** Совпадает с носителем (после load/save). */
    void clearEdited() noexcept { _edited = false; }

    /** CRC / magic / I/O последней load/save. */
    [[nodiscard]] Status status() const noexcept { return _status; }
    /** Сколько записей Diff накоплено после load. */
    [[nodiscard]] uint8_t mismatchCount() const noexcept { return _mismatchCount; }
    /** Запись Diff; индекс вне диапазона → [0]. */
    [[nodiscard]] const Mismatch& mismatch(uint8_t i) const noexcept
    {
        return _mismatches[(i < _mismatchCount) ? i : 0u];
    }
    /** У каждой required-секции есть desc с диска. */
    [[nodiscard]] bool allRequiredPresent() const noexcept;

    /** Прочитать файл в секции; mismatches копятся, CRC/magic — отказ. */
    [[nodiscard]] Status load(IFile& io, const char* path) noexcept;
    /** Записать секции: payload, затем header+catalog. */
    [[nodiscard]] Status save(IFile& io, const char* path) noexcept;

    /**
     * Имя, mismatches, payload по id реестра. edited не копируется.
     * Оба файла — один конкретный тип (тот же набор секций). Иначе false, dest не меняется.
     */
    [[nodiscard]] bool copyFrom(const IShowFile& src) noexcept;

protected:
    /** Реестр секций (storage наследника ShowFile<N>). */
    explicit IShowFile(MISC::ObjRegistry<ISection, uint8_t>& sections) noexcept
        : _sections(sections)
    {}

private:
    friend void detail::registerSection(IShowFile& file, ISection& sec) noexcept;

    void addMismatch(Diff kind, uint32_t tag, uint16_t expected = 0, uint16_t found = 0) noexcept;
    /** CRC каталога + payload всех секций (как на диске после save). */
    [[nodiscard]] uint32_t crcBody() const noexcept;
    [[nodiscard]] Status fail(Status st) noexcept;
    /** Закрыть файл и вернуть ошибку. */
    [[nodiscard]] Status fail(IFile& io, Status st) noexcept;

    /** Обход зарегистрированных секций по id. */
    template <typename F>
    void forEachSection(F&& fn) noexcept
    {
        const uint8_t first = _sections.firstId();
        const uint8_t end = _sections.endId();
        for (uint8_t id = first; id < end; ++id) {
            ISection* s = _sections.get(id);
            if (s != nullptr) {
                fn(*s);
            }
        }
    }

    template <typename F>
    void forEachSection(F&& fn) const noexcept
    {
        const uint8_t first = _sections.firstId();
        const uint8_t end = _sections.endId();
        for (uint8_t id = first; id < end; ++id) {
            const ISection* s = const_cast<IShowFile*>(this)->_sections.get(id);
            if (s != nullptr) {
                fn(*s);
            }
        }
    }

    MISC::ObjRegistry<ISection, uint8_t>& _sections;
    char _name[kPathSize]{};
    Status _status = Status::Ok;
    Mismatch _mismatches[kMaxMismatches]{};
    uint8_t _mismatchCount = 0;
    bool _edited = false;
};

template <typename Rec, uint16_t N>
class Section : public ISection {
    static_assert(N > 0u, "Section: N > 0");
    static_assert(!std::is_pointer_v<Rec>, "Section: Rec is the record type, not Rec*");
    static_assert(sizeof(Rec) > 0u && sizeof(Rec) <= 0xFFFFu, "Section: sizeof(Rec) fits uint16");

public:
    static constexpr uint16_t kSlotCount = N;
    static constexpr uint16_t kSlotSize = static_cast<uint16_t>(sizeof(Rec));
    static constexpr std::size_t kPayloadBytes = static_cast<std::size_t>(N) * sizeof(Rec);

    /** Регистрирует секцию в @a file; payload — _rec. */
    explicit Section(IShowFile& file, uint32_t tag, bool required = false) noexcept
        : ISection(file, tag, kSlotCount, kSlotSize, required, _rec)
    {}

    /** Занулить записи и сбросить desc. */
    void clearData() noexcept override
    {
        for (uint16_t i = 0; i < N; ++i) {
            _rec[i] = Rec{};
        }
        clearDesc();
    }

    /** Итераторы по Rec[N]. */
    Rec* begin() noexcept { return _rec; }
    Rec* end() noexcept { return _rec + N; }
    const Rec* begin() const noexcept { return _rec; }
    const Rec* end() const noexcept { return _rec + N; }

protected:
    /** Запись слота; индекс вне диапазона → слот 0. */
    [[nodiscard]] Rec& rec(uint16_t i) noexcept { return _rec[(i < N) ? i : 0u]; }
    [[nodiscard]] const Rec& rec(uint16_t i) const noexcept { return _rec[(i < N) ? i : 0u]; }

private:
    Rec _rec[N]{};
};

template <uint8_t N>
class ShowFile : public IShowFile {
    static_assert(N > 0u, "ShowFile: N > 0");

public:
    static constexpr uint8_t kSectionMax = N;

    /** Реестр секций — _store; секции-члены наследника регистрируются в своих ctor. */
    ShowFile() noexcept : IShowFile(_store) {}

private:
    using Store = MISC::ObjStorage<ISection, N, uint8_t, 0>;
    Store _store{};
};

} // namespace file
} // namespace smcp
