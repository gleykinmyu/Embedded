/**
 * @file showFile.cpp
 * @brief Регистрация секций, CRC, разбор каталога, load/save IShow.
 */

#include "showFile.hpp"

#include <cstring>
#include <limits>

#include "crc.hpp"
#include "debug.hpp"

namespace sf {

void detail::registerSection(IShow& file, ISection& sec) noexcept
{
    /* Секция без тега / слотов / размера слота не имеет смысла. */
    if (sec.tag() == 0u || sec.slotCount() == 0u || sec.slotSize() == 0u) {
        SF_DBG("[SF] registerSection reject tag=0x%08lX slots=%u size=%u\n",
            static_cast<unsigned long>(sec.tag()), static_cast<unsigned>(sec.slotCount()),
            static_cast<unsigned>(sec.slotSize()));
        file._status = Status::RegisterFailed;
        return;
    }
    /* Повтор того же FourCC не регистрируем. */
    if (file.find(sec.tag()) != nullptr) {
        SF_DBG("[SF] registerSection duplicate tag=0x%08lX\n",
            static_cast<unsigned long>(sec.tag()));
        file._status = Status::RegisterFailed;
        return;
    }
    /* id выдаётся плотно с 0 (FirstId = 0) — на этом держится индексация pending. */
    uint8_t id = 0;
    const MISC::RegStatus st = file._sections.registerAuto(&sec, id);
    if (st != MISC::RegStatus::Ok) {
        SF_DBG("[SF] registerSection fail tag=0x%08lX reg=%s\n",
            static_cast<unsigned long>(sec.tag()), MISC::cstr(st));
        file._status = Status::RegisterFailed;
    }
}

void Header::setName(const char* src) noexcept
{
    /* nullptr — пустое имя. */
    if (src == nullptr) {
        name[0] = '\0';
        return;
    }
    /* Обрезаем по буферу, терминатор ставим всегда. */
    std::strncpy(name, src, sizeof(name) - 1u);
    name[sizeof(name) - 1u] = '\0';
}

uint16_t Header::crc16() const noexcept
{
    /* CRC считается по заголовку с обнулённым полем header_crc16. */
    Header tmp = *this;
    tmp.header_crc16 = 0;
    return MISC::Crc16Ccitt::compute(reinterpret_cast<const uint8_t*>(&tmp), sizeof(tmp));
}

bool Header::read(IFile& io, std::size_t offset) noexcept
{
    return io.seek(offset) && io.read(reinterpret_cast<uint8_t*>(this), sizeof(Header));
}

bool Header::write(IFile& io, std::size_t offset) const noexcept
{
    return io.seek(offset) && io.write(reinterpret_cast<const uint8_t*>(this), sizeof(Header));
}

Status Header::check(uint32_t magic_, uint16_t version_) const noexcept
{
    /* Порядок: сначала «это вообще наш файл», потом версия формата, потом целостность. */
    if (magic != magic_) {
        return Status::BadMagic;
    }
    if (version != version_) {
        return Status::BadVersion;
    }
    if (crc16() != header_crc16) {
        return Status::BadHeaderCrc;
    }
    return Status::Ok;
}

bool computeFileCrc32(IFile& io, std::size_t offset, std::size_t length,
                      uint32_t& out_crc) noexcept
{
    MISC::Crc32 crc;
    /* Пустой диапазон — CRC пустых данных, без seek. */
    if (length != 0u) {
        if (!io.seek(offset)) {
            return false;
        }
        /* Чтение кусками kCrcChunkSize: буфер на стеке, без копии файла в RAM. */
        uint8_t buf[kCrcChunkSize]{};
        while (length > 0u) {
            const std::size_t n = (length < kCrcChunkSize) ? length : kCrcChunkSize;
            if (!io.read(buf, n)) {
                return false;
            }
            crc.update(buf, n);
            length -= n;
        }
    }
    out_crc = crc.value();
    return true;
}

Status Header::checkSize(std::size_t fileSize) const noexcept
{
    /* total_size должен вмещать хотя бы заголовок и каталог. */
    if (total_size < payloadBase()) {
        return Status::BadLayout;
    }
    /* Файл короче заявленного total_size — обрезан. Длиннее — допустимо. */
    if (fileSize < static_cast<std::size_t>(total_size)) {
        return Status::Truncated;
    }
    return Status::Ok;
}

Status SectionDesc::check(std::size_t expectedOffset, std::size_t totalSize) const noexcept
{
    /* Пустой тег / размер / число записей в файле недопустимы. */
    if (tag == 0u || byte_size == 0u || record_count == 0u) {
        return Status::BadLayout;
    }
    /* Секции идут подряд без дыр. */
    if (offset != expectedOffset) {
        return Status::BadLayout;
    }
    /* Защита от переполнения offset + byte_size. */
    if (offset > std::numeric_limits<uint32_t>::max() - byte_size) {
        return Status::BadLayout;
    }
    /* Секция не выходит за конец файла. */
    if (endOffset() > totalSize) {
        return Status::BadLayout;
    }
    /* Размер слота в файле = byte_size / record_count, должен делиться нацело. */
    if ((byte_size % record_count) != 0u) {
        return Status::BadLayout;
    }
    return Status::Ok;
}

ISection::ISection(IShow& file, Tag tag, uint16_t slotCount, uint16_t slotSize,
                   bool required, void* data) noexcept
    : _required(required)
    , _slotCount(slotCount)
    , _slotSize(slotSize)
    , _tag(tag)
    , _data(*static_cast<uint8_t*>(data))
    , _file(file)
{
    /* Секция сама регистрируется в шоуфайле; ошибка → file.status() = RegisterFailed. */
    detail::registerSection(file, *this);
}

bool ISection::copyFrom(const ISection& src) noexcept
{
    /* Самокопирование — no-op. */
    if (&src == this) {
        return true;
    }
    /* Копируем только между секциями одной раскладки. */
    if (checkLayout(src) != Status::Ok) {
        return false;
    }
    std::memcpy(&_data, src.payload(), payloadBytes());
    _desc = src.desc();
    return true;
}

Status IShow::fail(Status st) noexcept
{
    /* Запомнить статус и вернуть его — для `return fail(...)`. */
    _status = st;
    return st;
}

Status IShow::fail(IFile& io, Status st) noexcept
{
    /* Вариант для путей с открытым файлом: сначала закрыть. */
    io.close();
    return fail(st);
}

void IShow::setName(const char* name) noexcept
{
    /* nullptr — сброс имени. */
    if (name == nullptr) {
        _name[0] = '\0';
        return;
    }
    /* Обрезаем по буферу, терминатор ставим всегда. */
    std::strncpy(_name, name, sizeof(_name) - 1u);
    _name[sizeof(_name) - 1u] = '\0';
}

void IShow::clearData() noexcept
{
    /* Сброс метаданных шоу: имя, статус, список расхождений. */
    setName(nullptr);
    _status = Status::Ok;
    _mismatchCount = 0;
    for (uint8_t i = 0; i < kMaxMismatches; ++i) {
        _mismatches[i] = {};
    }
    /* edited не трогаем: решение за вызывающим (newShow / acceptLoaded). */
    forEachSection([](ISection& sec) { sec.clearData(); });
}

bool IShow::isEdited() const noexcept
{
    /* Шоу изменено, если изменена хотя бы одна секция. */
    bool any = false;
    forEachSection([&any](const ISection& sec) {
        if (sec.isEdited()) {
            any = true;
        }
    });
    return any;
}

void IShow::markEdited(const TagMask& tags) noexcept
{
    /* Пометить изменёнными секции из маски (по умолчанию все — например, после newShow). */
    forEachSection([&](ISection& sec) {
        if (tags.contains(sec.tag())) {
            sec.markEdited();
        }
    });
}

void IShow::clearEdited(const TagMask& tags) noexcept
{
    /* Снять edited только с секций из маски (сохранённых / загруженных). */
    forEachSection([&](ISection& sec) {
        if (tags.contains(sec.tag())) {
            sec.clearEdited();
        }
    });
}

bool IShow::copyFrom(const IShow& src) noexcept
{
    /* Самокопирование — no-op. */
    if (&src == this) {
        return true;
    }
    /* Разный формат файла — разные шоу, не копируем. */
    if (magic != src.magic || version != src.version) {
        return false;
    }
    if (sectionCount() != src.sectionCount()) {
        return false;
    }

    const uint8_t first = _sections.firstId();
    const uint8_t end = _sections.endId();
    /* Сначала сверка раскладки — dest не меняем при несовпадении. */
    for (uint8_t id = first; id < end; ++id) {
        const ISection* d = section(id);
        const ISection* s = src.section(id);
        if (d == nullptr || s == nullptr) {
            return false;
        }
        if (d->checkLayout(*s) != Status::Ok) {
            return false;
        }
    }

    /* Раскладка совпала — копируем метаданные и все секции; copyFrom не откажет (тот же checkLayout). */
    setName(src.name());
    _status = src._status;
    _mismatchCount = src._mismatchCount;
    for (uint8_t i = 0; i < kMaxMismatches; ++i) {
        _mismatches[i] = src._mismatches[i];
    }
    for (uint8_t id = first; id < end; ++id) {
        (void)section(id)->copyFrom(*src.section(id));
    }
    return true;
}

void IShow::addMismatch(Diff kind, Tag tag, uint16_t expected, uint16_t found) noexcept
{
    /* Список фиксированный: лишние расхождения молча отбрасываем. */
    if (_mismatchCount >= kMaxMismatches) {
        return;
    }
    Mismatch& m = _mismatches[_mismatchCount++];
    m.kind = kind;
    m.tag = tag;
    m.expected = expected;
    m.found = found;
    SF_DBG("[SF] show mismatch kind=%u tag=0x%08lX exp=%u got=%u\n",
        static_cast<unsigned>(kind), static_cast<unsigned long>(tag),
        static_cast<unsigned>(expected), static_cast<unsigned>(found));
}

SectionDesc& IShow::pendingDesc(uint8_t id) noexcept
{
    /* Нет буфера — отдаём static sink, чтобы не разыменовать nullptr. */
    const uint8_t cap = static_cast<uint8_t>(_sections.capacity());
    if (_pendingDesc == nullptr || cap == 0u) {
        SF_DBG("[SF] pendingDesc: no buffer\n");
        static SectionDesc sink{};
        return sink;
    }
    /* id вне ёмкости — страховка от выхода за массив (при плотных id не случается). */
    return _pendingDesc[(id < cap) ? id : 0u];
}

const SectionDesc& IShow::pendingDesc(uint8_t id) const noexcept
{
    /* const-версия через неконстантную: логика одна. */
    return const_cast<IShow*>(this)->pendingDesc(id);
}

ISection* IShow::find(Tag tag) noexcept
{
    /* Линейный поиск по зарегистрированным секциям. */
    const uint8_t first = _sections.firstId();
    const uint8_t end = _sections.endId();
    for (uint8_t id = first; id < end; ++id) {
        ISection* s = _sections.get(id);
        if (s != nullptr && s->tag() == tag) {
            return s;
        }
    }
    return nullptr;
}

const ISection* IShow::find(Tag tag) const noexcept
{
    /* const-версия через неконстантную: логика одна. */
    return const_cast<IShow*>(this)->find(tag);
}

bool IShow::allRequiredPresent() const noexcept
{
    /* Без маски: каждая required-секция должна иметь desc из файла (tag != 0). */
    bool ok = true;
    forEachSection([&](const ISection& s) {
        if (s.required() && s.desc().tag == 0u) {
            ok = false;
        }
    });
    return ok;
}

bool IShow::isValid() const noexcept
{
    /* Шоу валидно, если валидна каждая секция (её собственная проверка данных). */
    bool ok = true;
    forEachSection([&](const ISection& s) {
        if (!s.isValid()) {
            ok = false;
        }
    });
    return ok;
}

Status IShow::load(IFile& io, const char* path, const TagMask& tags, bool allowTruncate) noexcept
{
    _mismatchCount = 0;
    _status = Status::Ok;

    /* Пустая маска — грузить нечего, это ошибка вызывающего. */
    if (tags.empty()) {
        return fail(Status::BadLayout);
    }
    if (path == nullptr || !io.open(path, false)) {
        SF_DBG("[SF] Show::load open fail %s\n", cstr(Status::IoError));
        return fail(Status::IoError);
    }

    /* Заголовок: magic / версия / CRC16, затем размеры. */
    Header hdr{};
    if (!hdr.read(io, 0)) {
        return fail(io, Status::IoError);
    }

    const Status hs = hdr.check(magic, version);
    if (hs != Status::Ok) {
        return fail(io, hs);
    }
    if (hdr.section_count == 0u) {
        return fail(io, Status::BadLayout);
    }
    const Status fs = hdr.checkSize(io.size());
    if (fs != Status::Ok) {
        return fail(io, fs);
    }

    /* Body CRC (каталог + payload) до разбора: битый файл отсекаем сразу. */
    uint32_t body_crc = 0;
    const std::size_t body_len = static_cast<std::size_t>(hdr.total_size) - sizeof(Header);
    if (!computeFileCrc32(io, sizeof(Header), body_len, body_crc)) {
        return fail(io, Status::IoError);
    }
    if (body_crc != hdr.body_crc32) {
        return fail(io, Status::BadBodyCrc);
    }

    /*
     * Каталог → _pendingDesc (Show<N>); live desc не трогаем до commit.
     * Overflow / Io / BadLayout до commit → старые desc остаются.
     */
    const uint8_t cap = static_cast<uint8_t>(_sections.capacity());
    /* Show: ObjStorage FirstId = 0; pending[i] ↔ section id i. */
    if (_pendingDesc == nullptr || cap == 0u || _sections.firstId() != 0u) {
        return fail(io, Status::BadLayout);
    }
    for (uint8_t i = 0; i < cap; ++i) {
        pendingDesc(i) = {};
    }

    bool overflow = false;

    /* Один проход по каталогу: проверка раскладки, staging в pending, сбор mismatch. */
    std::size_t cursor = hdr.payloadBase();
    for (uint16_t i = 0; i < hdr.section_count; ++i) {
        SectionDesc d{};
        if (!d.read(io, sizeof(Header) + static_cast<std::size_t>(i) * sizeof(SectionDesc))) {
            return fail(io, Status::IoError);
        }
        const Status ds = d.check(cursor, hdr.total_size);
        if (ds != Status::Ok) {
            return fail(io, ds);
        }
        SF_DBG("[SF] catalog[%u] tag=0x%08lX off=%u bytes=%u rec=%u\n",
            static_cast<unsigned>(i), static_cast<unsigned long>(d.tag),
            static_cast<unsigned>(d.offset), static_cast<unsigned>(d.byte_size),
            static_cast<unsigned>(d.record_count));
        cursor = d.endOffset();

        /* Не из маски — только проверили раскладку, дальше не смотрим. */
        if (!tags.contains(d.tag)) {
            continue;
        }
        /* В файле есть, у пульта нет — расхождение, не ошибка. */
        ISection* sec = find(d);
        if (sec == nullptr) {
            addMismatch(Diff::ExtraSection, d.tag, 0, static_cast<uint16_t>(d.record_count));
            continue;
        }
        SectionDesc& pending = pendingDesc(sec->id());
        /* Повтор того же FourCC в каталоге файла. */
        if (pending.tag != 0u) {
            return fail(io, Status::BadLayout);
        }
        /* Другой размер слота — читать нельзя; в pending не берём (секция будет пустой). */
        const std::size_t fileSlot = static_cast<std::size_t>(d.byte_size)
            / static_cast<std::size_t>(d.record_count);
        if (fileSlot != static_cast<std::size_t>(sec->slotSize())) {
            addMismatch(Diff::SlotSizeMismatch, sec->tag(), sec->slotSize(),
                        static_cast<uint16_t>(fileSlot));
            continue;
        }
        pending = d;

        /* Записей в файле больше, чем слотов, — переполнение (решение после цикла). */
        const uint16_t nfile = static_cast<uint16_t>(d.record_count);
        if (nfile > sec->slotCount()) {
            addMismatch(Diff::ExtraSlots, sec->tag(), sec->slotCount(), nfile);
            overflow = true;
        }
    }
    /* Последняя секция должна заканчиваться ровно на total_size. */
    if (cursor != static_cast<std::size_t>(hdr.total_size)) {
        return fail(io, Status::BadLayout);
    }
    /* Required-секция должна прийти из файла (в маске, в каталоге, слот совпал) — live не тронут. */
    bool missingRequired = false;
    forEachSection([&](const ISection& sec) {
        if (sec.required() && pendingDesc(sec.id()).tag == 0u) {
            addMismatch(Diff::MissingSection, sec.tag(), sec.slotCount(), 0);
            missingRequired = true;
        }
    });
    if (missingRequired) {
        return fail(io, Status::MissingRequired);
    }
    /* Overflow без разрешения на обрезку — live не тронут. */
    if (overflow && !allowTruncate) {
        return fail(io, Status::Overflow);
    }

    /*
     * Commit — частичный open, не merge: очищаются ВСЕ секции (payload + desc + edited),
     * desc ставится только пришедшим из файла (pending заполнен лишь для маски).
     */
    forEachSection([&](ISection& sec) {
        sec.clearData();
        sec.clearEdited();
        const SectionDesc& pending = pendingDesc(sec.id());
        if (pending.tag != 0u) {
            sec.setDesc(pending);
        }
    });

    /* Имя — только при полном open; частичный — безымянный, чтобы save не затёр полный файл. */
    setName(tags.selectsAll() ? hdr.name : nullptr);

    /* Payload: секции из маски — CRC секции, чтение. */
    bool ioFail = false;
    bool badSecCrc = false;
    forEachSection([&](ISection& sec) {
        if (ioFail || badSecCrc || !tags.contains(sec.tag())) {
            return;
        }
        /* В маске, но не пришла из файла — расхождение; секция пустая после commit. */
        const SectionDesc& desc = sec.desc();
        if (desc.tag == 0u) {
            if (!sec.required()) {
                addMismatch(Diff::MissingSection, sec.tag(), sec.slotCount(), 0);
            }
            return;
        }

        /* CRC секции с диска до чтения в RAM. */
        uint32_t sec_crc = 0;
        if (!computeFileCrc32(io, static_cast<std::size_t>(desc.offset),
                              static_cast<std::size_t>(desc.byte_size), sec_crc)
            || sec_crc != desc.crc32) {
            badSecCrc = true;
            return;
        }

        /* readPayload читает не больше slotCount записей (обрезка при allowTruncate). */
        if (!sec.readPayload(io, static_cast<std::size_t>(desc.offset),
                             static_cast<uint16_t>(desc.record_count))) {
            ioFail = true;
            return;
        }

        /* Записей в файле меньше слотов — расхождение; хвост пустой после clearData. */
        const uint16_t nfile = static_cast<uint16_t>(desc.record_count);
        if (nfile < sec.slotCount()) {
            addMismatch(Diff::FewerSlots, sec.tag(), sec.slotCount(), nfile);
        }
        /* ExtraSlots при allowTruncate — mismatch уже в preflight. */
    });
    if (badSecCrc) {
        return fail(io, Status::BadSectionCrc);
    }
    if (ioFail) {
        return fail(io, Status::IoError);
    }

    io.close();
    /* Частичный open — новое несохранённое шоу. */
    if (!tags.selectsAll()) {
        markEdited();
    }
    _status = Status::Ok;
    SF_DBG("[SF] Show::load ok name=\"%s\" mismatches=%u\n", _name,
        static_cast<unsigned>(_mismatchCount));
    return _status;
}

Status IShow::save(IFile& io, const char* path) noexcept
{
    _status = Status::Ok;
    /* Проверки до открытия файла; шоу пишется всегда целиком. */
    const uint16_t nsec = sectionCount();
    if (path == nullptr || nsec == 0u) {
        return fail(Status::BadLayout);
    }

    if (!io.open(path, true)) {
        SF_DBG("[SF] Show::save open fail %s\n", cstr(Status::IoError));
        return fail(Status::IoError);
    }

    /* Порядок записи: payload → каталог → CRC по файлу → заголовок. */
    Header hdr{};
    hdr.magic = magic;
    hdr.version = version;
    hdr.section_count = nsec;
    hdr.setName(_name);

    /* Payload пишем первым: setDesc(cursor) задаёт offset и считает CRC секции. */
    std::size_t cursor = hdr.payloadBase();
    uint16_t written = 0;
    Status err = Status::Ok;
    forEachSection([&](ISection& sec) {
        if (err != Status::Ok) {
            return;
        }
        if (written >= nsec) {
            err = Status::BadLayout;
            return;
        }
        const std::size_t bytes = sec.payloadBytes();
        if (bytes == 0u || sec.payload() == nullptr) {
            err = Status::BadLayout;
            return;
        }
        sec.setDesc(cursor);
        if (!sec.writePayload(io, cursor)) {
            err = Status::IoError;
            return;
        }
        cursor += bytes;
        ++written;
    });
    if (err != Status::Ok) {
        return fail(io, err);
    }
    if (written != nsec) {
        return fail(io, Status::BadLayout);
    }

    /* Каталог сразу за заголовком, в том же порядке, что и payload. */
    std::size_t descOffset = sizeof(Header);
    forEachSection([&](const ISection& sec) {
        if (err != Status::Ok) {
            return;
        }
        if (!sec.writeDesc(io, descOffset)) {
            err = Status::IoError;
            return;
        }
        descOffset += sizeof(SectionDesc);
    });
    if (err != Status::Ok) {
        return fail(io, err);
    }

    /* Body CRC — по записанному файлу (read-back), тем же способом, что в load. */
    hdr.total_size = static_cast<uint32_t>(cursor);
    if (!computeFileCrc32(io, sizeof(Header), cursor - sizeof(Header), hdr.body_crc32)) {
        return fail(io, Status::IoError);
    }
    hdr.updateCrc();

    /* Заголовок последним: до его записи файл не проходит проверку magic / CRC заголовка. */
    if (!hdr.write(io, 0)) {
        return fail(io, Status::IoError);
    }
    /* sync — данные реально на носителе, только потом снимаем edited. */
    if (!io.sync()) {
        return fail(io, Status::IoError);
    }

    io.close();
    clearEdited();
    _status = Status::Ok;
    SF_DBG("[SF] Show::save ok name=\"%s\" total=%u nsec=%u\n", _name,
        static_cast<unsigned>(hdr.total_size), static_cast<unsigned>(nsec));
    return _status;
}

} // namespace sf
