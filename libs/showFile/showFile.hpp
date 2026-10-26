/**
 * @file showFile.hpp
 * @brief Шоуфайл: формат на диске + ISection / Section<Rec, N> / Show<SecN>.
 *
 * Секции — члены наследника Show<SecN>: конструктор берёт IShow& и регистрируется.
 * Порядок объявления членов = порядок записи.
 * load/save — на IShow (обход реестра). CRC/magic — отказ; секции/слоты — Mismatch.
 * Dirty — на секцию (и пул через свою секцию-заголовков); IShow::isEdited() = any.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "crc.hpp"
#include "iFileSystem.hpp"
#include "obj_registry.hpp"
#include "tags.hpp"

namespace sf {

class IShow;
class ISection;

using BIF::IFile;

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
    BadSectionCrc, /**< CRC payload секции ≠ SectionDesc.crc32. */
    BadLayout,
    Truncated,
    Overflow, /**< record_count > slotCount и !allowTruncate. */
    RegisterFailed, /**< Секция не встала в реестр (ctor / overflow N). */
    MissingRequired, /**< Required-секция не в маске / нет в файле / другой слот. */
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
    case Status::BadSectionCrc: return "BadSectionCrc";
    case Status::BadLayout: return "BadLayout";
    case Status::Truncated: return "Truncated";
    case Status::Overflow: return "Overflow";
    case Status::RegisterFailed: return "RegisterFailed";
    case Status::MissingRequired: return "MissingRequired";
    default: return "?";
    }
}

/** Заголовок файла на носителе. Поля — только фиксированной ширины (не size_t). */
struct Header {
    char name[kPathSize]{};
    uint32_t magic = 0;
    uint16_t version = 0;
    uint16_t content_layers = 0;
    uint16_t section_count = 0;
    uint16_t header_crc16 = 0;
    uint32_t body_crc32 = 0;
    uint32_t total_size = 0;

    /** Имя шоу с обрезкой по буферу; терминатор всегда; nullptr — пустое. */
    void setName(const char* src) noexcept;
    /** Смещение первого payload: sizeof(Header) + catalog. */
    [[nodiscard]] constexpr std::size_t payloadBase() const noexcept;
    /** CRC16-CCITT заголовка; поле header_crc16 в расчёт не входит. */
    [[nodiscard]] uint16_t crc16() const noexcept;
    void updateCrc() noexcept { header_crc16 = crc16(); }
    /** Прочитать заголовок с позиции @a offset файла. */
    [[nodiscard]] bool read(IFile& io, std::size_t offset) noexcept;
    /** Записать заголовок с позиции @a offset файла. */
    [[nodiscard]] bool write(IFile& io, std::size_t offset) const noexcept;
    /** Ok / BadMagic / BadVersion / BadHeaderCrc против ожидаемых @a magic_ / @a version_. */
    [[nodiscard]] Status check(uint32_t magic_, uint16_t version_) const noexcept;
    /** total_size vs payloadBase и реальный размер файла. Ok / BadLayout / Truncated. */
    [[nodiscard]] Status checkSize(std::size_t fileSize) const noexcept;
};

static_assert(sizeof(Header) == kPathSize + 20u);
static_assert(alignof(Header) == alignof(uint32_t));

/** Запись каталога на носителе. Поля — только фиксированной ширины (не size_t). */
struct SectionDesc {
    Tag tag = 0;
    uint32_t offset = 0;
    uint32_t byte_size = 0;
    uint32_t record_count = 0;
    uint32_t crc32 = 0; /**< CRC32 payload секции (ISO-HDLC). */

    [[nodiscard]] constexpr std::size_t payloadSize(std::size_t record_size) const noexcept
    {
        return record_size * static_cast<std::size_t>(record_count);
    }
    [[nodiscard]] constexpr std::size_t endOffset() const noexcept
    {
        return static_cast<std::size_t>(offset) + static_cast<std::size_t>(byte_size);
    }
    /** Прочитать запись каталога с позиции @a offset файла. */
    [[nodiscard]] bool read(IFile& io, std::size_t offset) noexcept
    {
        return io.seek(offset) && io.read(reinterpret_cast<uint8_t*>(this), sizeof(SectionDesc));
    }
    /** Записать запись каталога с позиции @a offset файла. */
    [[nodiscard]] bool write(IFile& io, std::size_t offset) const noexcept
    {
        return io.seek(offset)
            && io.write(reinterpret_cast<const uint8_t*>(this), sizeof(SectionDesc));
    }
    /** Ok / BadLayout: подряд без дыр, в пределах totalSize, byte_size % record_count. */
    [[nodiscard]] Status check(std::size_t expectedOffset, std::size_t totalSize) const noexcept;
};

static_assert(sizeof(SectionDesc) == 20u);
static_assert(alignof(SectionDesc) == alignof(uint32_t));

constexpr std::size_t Header::payloadBase() const noexcept
{
    return sizeof(Header) + static_cast<std::size_t>(section_count) * sizeof(SectionDesc);
}

[[nodiscard]] bool computeFileCrc32(IFile& io, std::size_t offset, std::size_t length,
                                    uint32_t& out_crc) noexcept;

namespace detail {
void registerSection(IShow& file, ISection& sec) noexcept;
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
    Tag tag = 0;
    uint16_t expected = 0;
    uint16_t found = 0;
};

/**
 * Секция каталога. Тег FourCC — поле, не id реестра.
 * Конструктор принимает IShow& и регистрируется сам.
 * Payload — у наследника (Section<Rec, N>: Rec[N]).
 */
class ISection {
public:
    /** @a data — начало payload наследника (массив Rec[N]). */
    ISection(IShow& file, Tag tag, uint16_t slotCount, uint16_t slotSize, bool required,
             void* data) noexcept;

    ISection(const ISection&) = delete;
    ISection& operator=(const ISection&) = delete;
    virtual ~ISection() = default;

    /** Сбросить payload секции (каждая реализация по-своему). */
    virtual void clearData() noexcept = 0;
    /** Проверка содержимого; по умолчанию ок. */
    [[nodiscard]] virtual bool isValid() const noexcept { return true; }

    /** Владелец секции. */
    [[nodiscard]] IShow& showFile() noexcept { return _file; }
    [[nodiscard]] const IShow& showFile() const noexcept { return _file; }

    [[nodiscard]] bool isEdited() const noexcept { return _edited; }
    /** Пометить эту секцию dirty (UI `*` = any на IShow). */
    void markEdited() noexcept { _edited = true; }
    void clearEdited() noexcept { _edited = false; }

    [[nodiscard]] uint8_t id() const noexcept { return _id; }
    /** FourCC секции на диске. */
    [[nodiscard]] Tag tag() const noexcept { return _tag; }
    [[nodiscard]] uint16_t slotCount() const noexcept { return _slotCount; }
    [[nodiscard]] uint16_t slotSize() const noexcept { return _slotSize; }
    [[nodiscard]] bool required() const noexcept { return _required; }

    /** Каталожная запись с диска (tag==0 — секции не было в файле). */
    [[nodiscard]] const SectionDesc& desc() const noexcept { return _desc; }
    /** Запомнить desc с диска (load). */
    void setDesc(const SectionDesc& d) noexcept { _desc = d; }
    /** tag=0: секции в файле не было. */
    void clearDesc() noexcept { _desc = {}; }
    /** Собрать desc под текущий payload, начиная с @a offset (вкл. crc32). */
    void setDesc(std::size_t offset) noexcept
    {
        _desc.tag = _tag;
        _desc.offset = static_cast<uint32_t>(offset);
        _desc.byte_size = static_cast<uint32_t>(payloadBytes());
        _desc.record_count = _slotCount;
        _desc.crc32 = MISC::Crc32::compute(&_data, payloadBytes());
    }

    /** slotCount × slotSize. */
    [[nodiscard]] std::size_t payloadBytes() const noexcept
    {
        return static_cast<std::size_t>(_slotCount) * static_cast<std::size_t>(_slotSize);
    }
    /** Указатель на первый байт payload. */
    [[nodiscard]] const uint8_t* payload() const noexcept { return &_data; }

    /** Прочитать min(n, slotCount) записей в payload с позиции @a offset файла. */
    [[nodiscard]] bool readPayload(IFile& io, std::size_t offset, uint16_t n) noexcept
    {
        const uint16_t m = (n < _slotCount) ? n : _slotCount;
        return io.seek(offset)
            && io.read(&_data, static_cast<std::size_t>(m) * static_cast<std::size_t>(_slotSize));
    }
    /** Записать весь payload с позиции @a offset файла. */
    [[nodiscard]] bool writePayload(IFile& io, std::size_t offset) const noexcept
    {
        return io.seek(offset) && io.write(&_data, payloadBytes());
    }
    /** Записать desc (запись каталога) с позиции @a offset файла. */
    [[nodiscard]] bool writeDesc(IFile& io, std::size_t offset) const noexcept
    {
        return _desc.write(io, offset);
    }

    /** Ok / BadLayout: та же раскладка, что у @a o (тег и размер payload). */
    [[nodiscard]] Status checkLayout(const ISection& o) const noexcept
    {
        return (tag() == o.tag() && payloadBytes() == o.payloadBytes()) ? Status::Ok
                                                                        : Status::BadLayout;
    }

    /** Payload + desc. Только секция той же раскладки (checkLayout). */
    [[nodiscard]] bool copyFrom(const ISection& src) noexcept;

private:
    template <typename, typename>
    friend class MISC::ObjRegistry;

    /** Id реестра; вызывает ObjRegistry при register. */
    void set_id(uint8_t id) noexcept { _id = id; }

    uint8_t _id = 0;
    bool _required = false;
    bool _edited = false;
    uint16_t _slotCount = 0;
    uint16_t _slotSize = 0;
    Tag _tag = 0;
    SectionDesc _desc{};
    uint8_t& _data;
    IShow& _file;
};

class IShow {
public:
    IShow(const IShow&) = delete;
    IShow& operator=(const IShow&) = delete;
    virtual ~IShow() = default;

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
        return const_cast<IShow*>(this)->_sections.get(id);
    }
    /** Секция по FourCC / записи каталога. */
    [[nodiscard]] ISection* find(Tag tag) noexcept;
    [[nodiscard]] const ISection* find(Tag tag) const noexcept;
    [[nodiscard]] ISection* find(const SectionDesc& d) noexcept;
    [[nodiscard]] const ISection* find(const SectionDesc& d) const noexcept;

    /** Полный путь / имя в заголовке файла. */
    void setName(const char* name) noexcept;
    [[nodiscard]] const char* name() const noexcept { return _name; }

    /** Magic / версия формата этого шоу (заданы в конструкторе). */
    const uint32_t magic;
    const uint16_t version;

    /** true, если хотя бы одна секция dirty. */
    [[nodiscard]] bool isEdited() const noexcept;
    /** Пометить dirty секции из маски (default = все, newShow). */
    void markEdited(const TagMask& tags = TagMask::all) noexcept;
    /** Снять dirty у секций из маски (default = все / совпадает с носителем). */
    void clearEdited(const TagMask& tags = TagMask::all) noexcept;

    /** CRC / magic / I/O последней load/save. */
    [[nodiscard]] Status status() const noexcept { return _status; }
    /** Сколько записей Diff накоплено после load. */
    [[nodiscard]] uint8_t mismatchCount() const noexcept { return _mismatchCount; }
    /** Запись Diff; индекс вне диапазона → [0]. */
    [[nodiscard]] const Mismatch& mismatch(uint8_t i) const noexcept
    {
        return _mismatches[(i < _mismatchCount) ? i : 0u];
    }
    /** Все required-секции имеют desc (маску не смотрит). */
    [[nodiscard]] bool allRequiredPresent() const noexcept;

    /**
     * Open (не merge). Body CRC целиком; из файла берутся секции из @a tags,
     * ВСЕ секции перед этим очищаются. selectsAll() — полный open (имя из файла);
     * частичный — без имени и edited (новое несохранённое шоу).
     * До изменения live: Overflow, MissingRequired (required не в маске / нет в файле).
     * @a allowTruncate — при ExtraSlots читать min(n,N); иначе Status::Overflow.
     */
    [[nodiscard]] Status load(IFile& io, const char* path, const TagMask& tags = TagMask::all,
                              bool allowTruncate = false) noexcept;

    /** Записать шоу целиком (все секции). */
    [[nodiscard]] Status save(IFile& io, const char* path) noexcept;

    /**
     * Имя, mismatches, payload по id реестра. edited не копируется.
     * Оба файла — один конкретный тип (тот же набор секций). Иначе false, dest не меняется.
     */
    [[nodiscard]] bool copyFrom(const IShow& src) noexcept;

protected:
    /**
     * @a sections — реестр Show<N>; @a pendingDesc — staging каталога [capacity],
     * слот занят ⇔ tag != 0. Указатель с адреса члена Show — в mem-init ок.
     * @a magic_ / @a version_ — пишутся в заголовок и проверяются при load.
     */
    IShow(MISC::ObjRegistry<ISection, uint8_t>& sections, SectionDesc* pendingDesc,
          uint32_t magic_, uint16_t version_) noexcept
        : magic(magic_)
        , version(version_)
        , _sections(sections)
        , _pendingDesc(pendingDesc)
    {}

private:
    friend void detail::registerSection(IShow& file, ISection& sec) noexcept;

    void addMismatch(Diff kind, Tag tag, uint16_t expected = 0, uint16_t found = 0) noexcept;
    [[nodiscard]] SectionDesc& pendingDesc(uint8_t id) noexcept;
    [[nodiscard]] const SectionDesc& pendingDesc(uint8_t id) const noexcept;
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
            const ISection* s = const_cast<IShow*>(this)->_sections.get(id);
            if (s != nullptr) {
                fn(*s);
            }
        }
    }

    MISC::ObjRegistry<ISection, uint8_t>& _sections;
    SectionDesc* _pendingDesc = nullptr; /**< Show<N>::_pendingDesc[N]. */
    char _name[kPathSize]{};
    Status _status = Status::Ok;
    Mismatch _mismatches[kMaxMismatches]{};
    uint8_t _mismatchCount = 0;
};

inline ISection* IShow::find(const SectionDesc& d) noexcept
{
    return find(d.tag);
}

inline const ISection* IShow::find(const SectionDesc& d) const noexcept
{
    return find(d.tag);
}

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
    explicit Section(IShow& file, Tag tag, bool required = false) noexcept
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
class Show : public IShow {
    static_assert(N > 0u, "Show: N > 0");

public:
    static constexpr uint8_t kSectionMax = N;

    /** Реестр секций — _store; секции-члены наследника регистрируются в своих ctor. */
    Show(uint32_t magic, uint16_t version) noexcept
        : IShow(_store, _pendingDesc, magic, version)
    {}

private:
    /** FirstId = 0 → индекс pending == id секции. */
    using Store = MISC::ObjStorage<ISection, N, uint8_t, 0>;
    Store _store{};
    SectionDesc _pendingDesc[N]{};
};

} // namespace sf
