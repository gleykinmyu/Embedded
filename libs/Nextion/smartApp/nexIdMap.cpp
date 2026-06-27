#include "nexIdMap.hpp"

#include <cstring>

#include "../core/nexTypes.hpp"

namespace nex {
namespace idmap {

std::size_t Table::encode(uint8_t* buf, std::size_t cap) const noexcept {
    const std::size_t need = sizeof(BlobHeader) + static_cast<std::size_t>(count) * kRecordWireSize;
    if (buf == nullptr || cap < need)
        return 0u;

    BlobHeader hdr{};
    hdr.count = count;
    std::memcpy(buf, &hdr, sizeof(hdr));

    uint8_t* p = buf + sizeof(hdr);
    for (uint16_t i = 0u; i < count; ++i) {
        const Record& r = records[i];
        *p++ = r.page;
        *p++ = r.compiled_id;
        *p++ = r.panel_id;
    }
    return need;
}

bool Table::decode(const uint8_t* buf, std::size_t len) noexcept {
    clear();
    if (!isBound() || buf == nullptr || len < sizeof(BlobHeader))
        return false;

    BlobHeader hdr{};
    std::memcpy(&hdr, buf, sizeof(hdr));
    if (hdr.magic != BlobHeader::kMagic || hdr.version != BlobHeader::kVersion)
        return false;

    const std::size_t need = sizeof(BlobHeader) + static_cast<std::size_t>(hdr.count) * kRecordWireSize;
    if (len < need || hdr.count > capacity)
        return false;

    const uint8_t* p = buf + sizeof(hdr);
    for (uint16_t i = 0u; i < hdr.count; ++i) {
        Record& r = records[i];
        r.page = *p++;
        r.compiled_id = *p++;
        r.panel_id = *p++;
    }
    count = hdr.count;
    return true;
}

const Record* Table::find(uint8_t page, uint8_t compiled_id) const noexcept {
    for (uint16_t i = 0u; i < count; ++i) {
        const Record& r = records[i];
        if (r.page == page && r.compiled_id == compiled_id)
            return &r;
    }
    return nullptr;
}

Record* Table::find(uint8_t page, uint8_t compiled_id) noexcept {
    return const_cast<Record*>(static_cast<const Table*>(this)->find(page, compiled_id));
}

bool Table::upsert(uint8_t page, uint8_t compiled_id, uint8_t panel_id) noexcept {
    if (!isBound() || compiled_id < kFirstCompId || panel_id == kPageCompId)
        return false;

    if (Record* const existing = find(page, compiled_id)) {
        existing->panel_id = panel_id;
        return true;
    }

    if (count >= capacity)
        return false;

    Record& rec = records[count++];
    rec.page = page;
    rec.compiled_id = compiled_id;
    rec.panel_id = panel_id;
    return true;
}

} // namespace idmap
} // namespace nex
