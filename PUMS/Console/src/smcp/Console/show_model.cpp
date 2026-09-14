/**
 * @file show_model.cpp
 * @brief Регистрация секций, CRC, разбор каталога, load/save IShowFile.
 */

#include "smcp/Console/show_model.hpp"

#include <cstring>

#include "crc.hpp"
#include "smcp/debug.hpp"

namespace smcp {
namespace file {

uint16_t computeHeaderCrc16(const Header& hdr) noexcept
{
    Header tmp = hdr;
    tmp.header_crc16 = 0;
    return MISC::crc16Ccitt(reinterpret_cast<const uint8_t*>(&tmp), sizeof(tmp));
}

bool computeFileCrc32(IFile& io, std::size_t offset, std::size_t length,
                      uint32_t& out_crc) noexcept
{
    uint32_t crc = MISC::crc32Init();
    if (length != 0u) {
        if (!io.seek(offset)) {
            return false;
        }
        uint8_t buf[kCrcChunkSize]{};
        while (length > 0u) {
            const std::size_t n = (length < kCrcChunkSize) ? length : kCrcChunkSize;
            if (!io.read(buf, n)) {
                return false;
            }
            crc = MISC::crc32Update(crc, buf, n);
            length -= n;
        }
    }
    out_crc = MISC::crc32Final(crc);
    return true;
}

Status checkHeader(const Header& hdr) noexcept
{
    if (hdr.magic != kMagic) {
        return Status::BadMagic;
    }
    if (hdr.version != kVersion) {
        return Status::BadVersion;
    }
    if (computeHeaderCrc16(hdr) != hdr.header_crc16) {
        return Status::BadHeaderCrc;
    }
    return Status::Ok;
}

Status checkFileSize(const Header& hdr, std::size_t fileSize) noexcept
{
    if (hdr.total_size < hdr.payloadBase()) {
        return Status::BadLayout;
    }
    if (fileSize < static_cast<std::size_t>(hdr.total_size)) {
        return Status::Truncated;
    }
    return Status::Ok;
}

Status checkSectionDesc(const SectionDesc& desc, std::size_t expectedOffset,
                        uint32_t totalSize) noexcept
{
    if (desc.tag == 0u || desc.byte_size == 0u || desc.record_count == 0u) {
        return Status::BadLayout;
    }
    if (desc.offset != expectedOffset) {
        return Status::BadLayout;
    }
    if (desc.offset > 0xFFFFFFFFu - desc.byte_size) {
        return Status::BadLayout;
    }
    if (desc.endOffset() > static_cast<std::size_t>(totalSize)) {
        return Status::BadLayout;
    }
    if ((desc.byte_size % desc.record_count) != 0u) {
        return Status::BadLayout;
    }
    return Status::Ok;
}

ISection::ISection(IShowFile& file, uint32_t tag, uint16_t slotCount, uint16_t slotSize,
                   bool required, void* data) noexcept
    : _required(required)
    , _slotCount(slotCount)
    , _slotSize(slotSize)
    , _tag(tag)
    , _data(*static_cast<uint8_t*>(data))
    , _file(file)
{
    detail::registerSection(file, *this);
}

void ISection::markEdited() noexcept
{
    _file.markEdited();
}

Status IShowFile::fail(Status st) noexcept
{
    _status = st;
    return st;
}

Status IShowFile::fail(IFile& io, Status st) noexcept
{
    io.close();
    return fail(st);
}

void detail::registerSection(IShowFile& file, ISection& sec) noexcept
{
    if (sec.tag() == 0u || sec.slotCount() == 0u || sec.slotSize() == 0u) {
        return;
    }
    if (file.find(sec.tag()) != nullptr) {
        return;
    }
    uint8_t id = 0;
    (void)file._sections.registerAuto(&sec, id);
}

void IShowFile::setName(const char* name) noexcept
{
    if (name == nullptr) {
        _name[0] = '\0';
        return;
    }
    std::strncpy(_name, name, sizeof(_name) - 1u);
    _name[sizeof(_name) - 1u] = '\0';
}

void IShowFile::addMismatch(Diff kind, uint32_t tag, uint16_t expected, uint16_t found) noexcept
{
    if (_mismatchCount >= kMaxMismatches) {
        return;
    }
    Mismatch& m = _mismatches[_mismatchCount++];
    m.kind = kind;
    m.tag = tag;
    m.expected = expected;
    m.found = found;
    SMCP_SHOW("[SMCP] show mismatch kind=%u tag=0x%08lX exp=%u got=%u\n",
        static_cast<unsigned>(kind), static_cast<unsigned long>(tag),
        static_cast<unsigned>(expected), static_cast<unsigned>(found));
}

ISection* IShowFile::find(uint32_t tag) noexcept
{
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

const ISection* IShowFile::find(uint32_t tag) const noexcept
{
    return const_cast<IShowFile*>(this)->find(tag);
}

uint32_t IShowFile::crcBody() const noexcept
{
    uint32_t crc = MISC::crc32Init();
    forEachSection([&](const ISection& sec) {
        crc = MISC::crc32Update(crc, reinterpret_cast<const uint8_t*>(&sec.desc()),
                                sizeof(SectionDesc));
    });
    forEachSection([&](const ISection& sec) {
        const uint8_t* p = sec.payload();
        const std::size_t n = sec.payloadBytes();
        if (p != nullptr && n != 0u) {
            crc = MISC::crc32Update(crc, p, n);
        }
    });
    return MISC::crc32Final(crc);
}

bool IShowFile::allRequiredPresent() const noexcept
{
    bool ok = true;
    forEachSection([&](const ISection& s) {
        if (s.required() && s.desc().tag == 0u) {
            ok = false;
        }
    });
    return ok;
}

Status IShowFile::load(IFile& io, const char* path) noexcept
{
    _mismatchCount = 0;
    _status = Status::Ok;

    if (path == nullptr || !io.open(path, false)) {
        SMCP_SHOW("[SMCP] ShowFile::load open fail %s\n", cstr(Status::IoError));
        return fail(Status::IoError);
    }

    Header hdr{};
    if (!io.seek(0) || !io.read(reinterpret_cast<uint8_t*>(&hdr), sizeof(hdr))) {
        return fail(io, Status::IoError);
    }

    const Status hs = checkHeader(hdr);
    if (hs != Status::Ok) {
        return fail(io, hs);
    }
    if (hdr.section_count == 0u) {
        return fail(io, Status::BadLayout);
    }
    const Status fs = checkFileSize(hdr, io.size());
    if (fs != Status::Ok) {
        return fail(io, fs);
    }

    forEachSection([](ISection& sec) { sec.clearDesc(); });

    if (!io.seek(sizeof(Header))) {
        return fail(io, Status::IoError);
    }

    std::size_t cursor = hdr.payloadBase();
    for (uint16_t i = 0; i < hdr.section_count; ++i) {
        SectionDesc d{};
        if (!io.read(reinterpret_cast<uint8_t*>(&d), sizeof(d))) {
            return fail(io, Status::IoError);
        }
        const Status ds = checkSectionDesc(d, cursor, hdr.total_size);
        if (ds != Status::Ok) {
            return fail(io, ds);
        }
        SMCP_SHOW("[SMCP] catalog[%u] tag=0x%08lX off=%u bytes=%u rec=%u\n",
            static_cast<unsigned>(i), static_cast<unsigned long>(d.tag),
            static_cast<unsigned>(d.offset), static_cast<unsigned>(d.byte_size),
            static_cast<unsigned>(d.record_count));
        cursor = d.endOffset();

        ISection* sec = find(d.tag);
        if (sec == nullptr) {
            addMismatch(Diff::ExtraSection, d.tag, 0, static_cast<uint16_t>(d.record_count));
            continue;
        }
        if (sec->desc().tag != 0u) {
            return fail(io, Status::BadLayout);
        }
        sec->setDesc(d);
    }
    if (cursor != static_cast<std::size_t>(hdr.total_size)) {
        return fail(io, Status::BadLayout);
    }

    uint32_t body_crc = 0;
    const std::size_t body_len = static_cast<std::size_t>(hdr.total_size) - sizeof(Header);
    if (!computeFileCrc32(io, sizeof(Header), body_len, body_crc)) {
        return fail(io, Status::IoError);
    }
    if (body_crc != hdr.body_crc32) {
        return fail(io, Status::BadBodyCrc);
    }

    setName(hdr.name);

    bool ioFail = false;
    forEachSection([&](ISection& sec) {
        if (ioFail) {
            return;
        }
        const SectionDesc& desc = sec.desc();
        if (desc.tag == 0u) {
            addMismatch(Diff::MissingSection, sec.tag(), sec.slotCount(), 0);
            return;
        }

        const std::size_t fileSlot = static_cast<std::size_t>(desc.byte_size)
            / static_cast<std::size_t>(desc.record_count);
        if (fileSlot != static_cast<std::size_t>(sec.slotSize())) {
            addMismatch(Diff::SlotSizeMismatch, sec.tag(), sec.slotSize(),
                        static_cast<uint16_t>(fileSlot));
            return;
        }

        if (!io.seek(static_cast<std::size_t>(desc.offset))
            || !sec.readPayload(io, static_cast<uint16_t>(desc.record_count))) {
            ioFail = true;
            return;
        }

        const uint16_t nfile = static_cast<uint16_t>(desc.record_count);
        if (nfile < sec.slotCount()) {
            addMismatch(Diff::FewerSlots, sec.tag(), sec.slotCount(), nfile);
        } else if (nfile > sec.slotCount()) {
            addMismatch(Diff::ExtraSlots, sec.tag(), sec.slotCount(), nfile);
        }
    });
    if (ioFail) {
        return fail(io, Status::IoError);
    }

    io.close();
    _status = Status::Ok;
    SMCP_SHOW("[SMCP] ShowFile::load ok name=\"%s\" mismatches=%u\n", _name,
        static_cast<unsigned>(_mismatchCount));
    return _status;
}

Status IShowFile::save(IFile& io, const char* path) noexcept
{
    _status = Status::Ok;
    const uint16_t nsec = sectionCount();
    if (nsec == 0u || static_cast<std::size_t>(nsec) > _sections.capacity() || path == nullptr) {
        return fail(Status::BadLayout);
    }

    if (!io.open(path, true)) {
        SMCP_SHOW("[SMCP] ShowFile::save open fail %s\n", cstr(Status::IoError));
        return fail(Status::IoError);
    }

    Header hdr{};
    hdr.magic = kMagic;
    hdr.version = kVersion;
    hdr.section_count = nsec;
    std::strncpy(hdr.name, _name, sizeof(hdr.name) - 1u);
    hdr.name[sizeof(hdr.name) - 1u] = '\0';

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
        if (!io.seek(cursor) || !sec.writePayload(io)) {
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

    hdr.total_size = static_cast<uint32_t>(cursor);
    hdr.body_crc32 = crcBody();
    hdr.header_crc16 = computeHeaderCrc16(hdr);

    if (!io.seek(0) || !io.write(reinterpret_cast<const uint8_t*>(&hdr), sizeof(hdr))) {
        return fail(io, Status::IoError);
    }
    err = Status::Ok;
    forEachSection([&](const ISection& sec) {
        if (err != Status::Ok) {
            return;
        }
        if (!io.write(reinterpret_cast<const uint8_t*>(&sec.desc()), sizeof(SectionDesc))) {
            err = Status::IoError;
        }
    });
    if (err != Status::Ok || !io.sync()) {
        return fail(io, Status::IoError);
    }

    io.close();
    _status = Status::Ok;
    SMCP_SHOW("[SMCP] ShowFile::save ok name=\"%s\" total=%u\n", _name,
        static_cast<unsigned>(hdr.total_size));
    return _status;
}

} // namespace file
} // namespace smcp
