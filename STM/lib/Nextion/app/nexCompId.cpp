// nexCompId.cpp — таблица cID, сериализация NXCI и машина опроса `Discover`.
//
// Discover (кратко):
//   pollBegin  → для каждой зарегистрированной страницы: switchPage → опрос слотов реестра get .id
//   → запись в `CidTable` → pollFinishSuccess → applyFromTable/setId → Flash + onCidPollComplete(true).
// Очередь команд и RX — в `Application::update`; здесь только enqueue и колбэки из dispatch.

#include "nexCompId.hpp"
#include "nexApplication.hpp"

#include <cstdio>
#include <cstring>

#include "../core/nexCommands.hpp"

namespace nex {

namespace {

/** Отладочный лог опроса Discover (`[CID]` в UART debug). Выключить: `0`. */
static constexpr bool kCidPollDebug = true;

constexpr uint32_t kPageSwitchTimeoutMs = 1000u;
constexpr uint32_t kSwitchPageLogPeriodMs = 500u;

const char* pollPhaseName(CidPoll::Phase phase) noexcept
{
    switch (phase) {
    case CidPoll::Phase::Idle: return "Idle";
    case CidPoll::Phase::SwitchPage: return "SwitchPage";
    case CidPoll::Phase::GetId: return "GetId";
    case CidPoll::Phase::Done: return "Done";
    case CidPoll::Phase::Failed: return "Failed";
    }
    return "?";
}

void logCompName(const Literal& name) noexcept
{
    if (!kCidPollDebug)
        return;
    char buf[32]{};
    const uint8_t n = (name.len < 31u) ? name.len : 31u;
    if (n > 0u && name.data != nullptr)
        std::memcpy(buf, name.data, n);
    std::printf("%s", buf);
}

void copyName(CidRecord& rec, const Literal& name) noexcept {
    rec.name_len = name.len;
    if (name.len == 0u) {
        rec.name[0] = '\0';
        return;
    }
    const uint8_t n = (name.len < sizeof(rec.name)) ? name.len : static_cast<uint8_t>(sizeof(rec.name) - 1u);
    rec.name_len = n;
    std::memcpy(rec.name, name.data, n);
    rec.name[n] = '\0';
}

} // namespace

//=============================================================================
// CidTable и вспомогательные сравнения
//=============================================================================

bool cidLiteralEquals(const Literal& a, const char* b, uint8_t b_len) noexcept {
    if (a.len != b_len)
        return false;
    if (a.len == 0u)
        return true;
    return std::memcmp(a.data, b, a.len) == 0;
}

const CidRecord* CidTable::find(uint8_t page_id, const Literal& comp_name) const noexcept {
    for (uint16_t i = 0u; i < count; ++i) {
        const CidRecord& r = records[i];
        if (r.page_id != page_id)
            continue;
        if (cidLiteralEquals(comp_name, r.name, r.name_len))
            return &r;
    }
    return nullptr;
}

CidRecord* CidTable::find(uint8_t page_id, const Literal& comp_name) noexcept {
    return const_cast<CidRecord*>(static_cast<const CidTable*>(this)->find(page_id, comp_name));
}

bool CidTable::upsert(uint8_t page_id, const Literal& comp_name, uint8_t panel_id) noexcept {
    if (!isBound() || comp_name.len == 0u || panel_id == kPageCompId)
        return false;

    if (CidRecord* const existing = find(page_id, comp_name)) {
        existing->panel_id = panel_id;
        return true;
    }

    if (count >= capacity)
        return false;

    CidRecord& rec = records[count++];
    rec.page_id = page_id;
    rec.panel_id = panel_id;
    copyName(rec, comp_name);
    return true;
}

//=============================================================================
// Сериализация NXCI (flash/EEPROM — у пользователя)
//=============================================================================

std::size_t cidTableEncode(const CidTable& table, uint8_t* buf, std::size_t cap) noexcept {
    const std::size_t need = sizeof(CidBlobHeader) + static_cast<std::size_t>(table.count) * kCidRecordWireSize;
    if (buf == nullptr || cap < need)
        return 0u;

    CidBlobHeader hdr{};
    hdr.count = table.count;
    std::memcpy(buf, &hdr, sizeof(hdr));

    uint8_t* p = buf + sizeof(hdr);
    for (uint16_t i = 0u; i < table.count; ++i) {
        const CidRecord& r = table.records[i];
        *p++ = r.page_id;
        *p++ = r.panel_id;
        *p++ = r.name_len;
        std::memcpy(p, r.name, sizeof(r.name));
        p += sizeof(r.name);
    }
    return need;
}

bool cidTableDecode(CidTable& table, const uint8_t* buf, std::size_t len) noexcept {
    table.clear();
    if (!table.isBound() || buf == nullptr || len < sizeof(CidBlobHeader))
        return false;

    CidBlobHeader hdr{};
    std::memcpy(&hdr, buf, sizeof(hdr));
    if (hdr.magic != CidBlobHeader::kMagic || hdr.version != CidBlobHeader::kVersion)
        return false;

    const std::size_t need = sizeof(CidBlobHeader) + static_cast<std::size_t>(hdr.count) * kCidRecordWireSize;
    if (len < need || hdr.count > table.capacity)
        return false;

    const uint8_t* p = buf + sizeof(hdr);
    for (uint16_t i = 0u; i < hdr.count; ++i) {
        CidRecord& r = table.records[i];
        r.page_id = *p++;
        r.panel_id = *p++;
        r.name_len = *p++;
        std::memcpy(r.name, p, sizeof(r.name));
        p += sizeof(r.name);
    }
    table.count = hdr.count;
    return true;
}

//=============================================================================
// Cid: режимы и применение таблицы
//=============================================================================

Cid::Cid(Application& app, CidTable& table) noexcept : _app(app), _table(table) {}

void Cid::setMode(CidMode mode) noexcept {
    _mode = mode;
    if (mode == CidMode::Discover)
        _poll = CidPoll{};
}

bool Cid::isDiscoveryBusy() const noexcept {
    if (_mode != CidMode::Discover)
        return false;
    const CidPoll::Phase p = _poll.phase;
    return p != CidPoll::Phase::Idle && p != CidPoll::Phase::Done && p != CidPoll::Phase::Failed;
}

bool Cid::loadFromBuffer(const uint8_t* buf, const std::size_t len) noexcept {
    if (!cidTableDecode(_table, buf, len))
        return false;
    applyFromTable();
    return true;
}

// Сопоставление записей таблицы с объектами по `objname` в реестре страницы.
void Cid::applyFromTable() noexcept {
    for (uint16_t i = 0u; i < _table.count; ++i) {
        const CidRecord& rec = _table.records[i];
        if (rec.panel_id == 0xFFu || rec.panel_id == kPageCompId || rec.name_len == 0u)
            continue;

        Page* const pg = _app.page(rec.page_id);
        if (pg == nullptr)
            continue;

        const ComponentRegistryDesc reg = pg->getRegistry();
        for (unsigned s = 0u; s < reg.size; ++s) {
            Component* const c = reg.slots[s];
            if (c == nullptr)
                continue;
            if (!cidLiteralEquals(c->name, rec.name, rec.name_len))
                continue;
            (void)c->setId(rec.panel_id);
            break;
        }
    }
}

//=============================================================================
// Discover: машина опроса
//=============================================================================

void Cid::pollBegin() noexcept {
    _table.clear();
    _poll = CidPoll{};

    if (_app._pageCount == 0u) {
        pollFail("no pages registered");
        return;
    }

    _poll.page_id = 0u;
    _poll.slot = 0u;
    _poll.phase = CidPoll::Phase::SwitchPage;
    if (kCidPollDebug) {
        std::printf("[CID] pollBegin pages=%u curPage=%u -> switchPage(%u)+sendme\n",
            static_cast<unsigned>(_app._pageCount), static_cast<unsigned>(_app._currentPageId),
            static_cast<unsigned>(_poll.page_id));
    }
    // `page N` не всегда шлёт 0x66; `sendme` — явный запрос evPage с id текущей страницы.
    _app.switchPage(_poll.page_id);
    _app.requestCurrentPage();
}

void Cid::updateDiscovery(uint32_t now_ms) noexcept {
    if (_mode != CidMode::Discover)
        return;

    if (_poll.phase == CidPoll::Phase::Idle)
        pollBegin();

    if (_poll.phase == CidPoll::Phase::Done || _poll.phase == CidPoll::Phase::Failed)
        return;

    pollTick(now_ms);
}

void Cid::pollFail(const char* reason) noexcept {
    if (kCidPollDebug) {
        std::printf("[CID] FAIL (%s) phase=%s page=%u slot=%u curPage=%u\n", reason != nullptr ? reason : "?",
            pollPhaseName(_poll.phase), static_cast<unsigned>(_poll.page_id),
            static_cast<unsigned>(_poll.slot), static_cast<unsigned>(_app._currentPageId));
    }
    _poll.phase = CidPoll::Phase::Failed;
    _app.onCidPollComplete(false);
}

void Cid::pollFinishSuccess() noexcept {
    if (kCidPollDebug)
        std::printf("[CID] pollFinishSuccess records=%u\n", static_cast<unsigned>(_table.count));
    applyFromTable();
    _poll.phase = CidPoll::Phase::Done;
    _mode = CidMode::Flash;
    _app.onCidPollComplete(true);
}

void Cid::pollEnqueueGetId(Component& c, uint8_t slot) noexcept {
    const AttrRef target{c.name, kCidAttrLexeme};
    if (kCidPollDebug) {
        std::printf("[CID] enqueue get .id page=%u slot=%u obj=", static_cast<unsigned>(_poll.page_id),
            static_cast<unsigned>(slot));
        logCompName(c.name);
        std::printf(" tag=%u\n", static_cast<unsigned>(kCidPollTagBase + slot));
    }
    _app.enqueue(Transaction{cmd::Get::numeric(target), kRoutePageId, kRouteCompId,
        static_cast<uint8_t>(kCidPollTagBase + slot), Transaction::State::AwaitingNumericGet});
}

void Cid::pollAdvanceSlot() noexcept {
    Page* const pg = _app.page(_poll.page_id);
    if (pg == nullptr) {
        pollFail("page not found");
        return;
    }

    const ComponentRegistryDesc reg = pg->getRegistry();
    if (kCidPollDebug)
        std::printf("[CID] pollAdvanceSlot page=%u registry=%u next_slot=%u\n",
            static_cast<unsigned>(_poll.page_id), static_cast<unsigned>(reg.size),
            static_cast<unsigned>(_poll.slot));

    while (_poll.slot < reg.size) {
        Component* const c = reg.slots[_poll.slot];
        const uint8_t slot = _poll.slot;
        ++_poll.slot;
        if (c == nullptr)
            continue;

        _poll.phase = CidPoll::Phase::GetId;
        pollEnqueueGetId(*c, slot);
        return;
    }

    pollNextPage();
}

void Cid::pollNextPage() noexcept {
    ++_poll.page_id;
    if (_poll.page_id >= _app._pageCount) {
        pollFinishSuccess();
        return;
    }

    _poll.slot = 0u;
    _poll.phase = CidPoll::Phase::SwitchPage;
    if (kCidPollDebug)
        std::printf("[CID] pollNextPage -> switchPage(%u) curPage=%u\n", static_cast<unsigned>(_poll.page_id),
            static_cast<unsigned>(_app._currentPageId));
    _app.switchPage(_poll.page_id);
}

// SwitchPage: ждём evPage (`onPageChange`) или уже актуальный `_currentPageId` (pollTick).
void Cid::pollTick(uint32_t now_ms) noexcept {
    switch (_poll.phase) {
    case CidPoll::Phase::Idle:
    case CidPoll::Phase::Done:
    case CidPoll::Phase::Failed:
    case CidPoll::Phase::GetId:
        return;

    case CidPoll::Phase::SwitchPage: {
        static uint32_t last_log_ms = 0u;
        static uint32_t last_sendme_ms = 0u;
        if (_poll.deadline_ms == 0u) {
            _poll.deadline_ms = now_ms + kPageSwitchTimeoutMs;
            last_sendme_ms = 0u;
            if (kCidPollDebug)
                std::printf("[CID] SwitchPage wait target=%u cur=%u deadline_ms=%lu\n",
                    static_cast<unsigned>(_poll.page_id), static_cast<unsigned>(_app._currentPageId),
                    static_cast<unsigned long>(_poll.deadline_ms));
        }
        if (_app._currentPageId == _poll.page_id) {
            if (kCidPollDebug)
                std::printf("[CID] SwitchPage OK (curPage match)\n");
            _poll.deadline_ms = 0u;
            last_log_ms = 0u;
            last_sendme_ms = 0u;
            pollAdvanceSlot();
            return;
        }
        if (_app._session.isIdle() && !_app._session.hasQueued()
            && (last_sendme_ms == 0u || (now_ms - last_sendme_ms) >= kSwitchPageLogPeriodMs)) {
            _app.requestCurrentPage();
            last_sendme_ms = now_ms;
        }
        if (kCidPollDebug && (last_log_ms == 0u || (now_ms - last_log_ms) >= kSwitchPageLogPeriodMs)) {
            last_log_ms = now_ms;
            std::printf("[CID] SwitchPage ... target=%u cur=%u left_ms=%lu q=%u\n",
                static_cast<unsigned>(_poll.page_id), static_cast<unsigned>(_app._currentPageId),
                static_cast<unsigned long>(_poll.deadline_ms > now_ms ? _poll.deadline_ms - now_ms : 0u),
                static_cast<unsigned>(_app._session.hasQueued() ? 1u : 0u));
        }
        if (now_ms >= _poll.deadline_ms)
            pollFail("SwitchPage timeout (no evPage)");
        return;
    }
    }
}

//=============================================================================
// Колбэки из Application (dispatchResponse / dispatchPageChange / sessionEnd)
//=============================================================================

void Cid::onPollResponse(uint8_t slot, const msg::getNumeric& response) noexcept {
    Page* const pg = _app.page(_poll.page_id);
    if (pg == nullptr) {
        pollFail("onPollResponse: page missing");
        return;
    }

    const ComponentRegistryDesc reg = pg->getRegistry();
    if (slot >= reg.size || reg.slots[slot] == nullptr) {
        if (kCidPollDebug)
            std::printf("[CID] onPollResponse bad slot=%u reg=%u\n", static_cast<unsigned>(slot),
                static_cast<unsigned>(reg.size));
        pollFail("onPollResponse: bad slot");
        return;
    }

    Component* const c = reg.slots[slot];
    const uint8_t panel_id = static_cast<uint8_t>(response.value & 0xFFu);

    if (kCidPollDebug) {
        std::printf("[CID] onPollResponse page=%u slot=%u obj=", static_cast<unsigned>(_poll.page_id),
            static_cast<unsigned>(slot));
        logCompName(c->name);
        std::printf(" panel_id=%u raw=%ld\n", static_cast<unsigned>(panel_id),
            static_cast<long>(response.value));
    }

    if (!_table.upsert(_poll.page_id, c->name, panel_id)) {
        pollFail("table upsert failed");
        return;
    }

    // Не вызывать setId() здесь: rebind меняет slots[] и ломает tag=индекс слота
    // (все компоненты при регистрации с id=0 получают слоты 0,1,2…; swap сбивает опрос).
    // panel_id применится в pollFinishSuccess → applyFromTable().
    pollAdvanceSlot();
}

void Cid::onSessionEnd(bool success) noexcept {
    if (_poll.phase != CidPoll::Phase::GetId)
        return;
    if (!success) {
        if (kCidPollDebug)
            std::printf("[CID] onSessionEnd FAIL (timeout or NIS error)\n");
        pollFail("session end");
    }
}

void Cid::onPageChange(uint8_t page_id) noexcept {
    if (_poll.phase != CidPoll::Phase::SwitchPage)
        return;
    if (kCidPollDebug)
        std::printf("[CID] onPageChange evPage=%u want=%u\n", static_cast<unsigned>(page_id),
            static_cast<unsigned>(_poll.page_id));
    if (page_id != _poll.page_id)
        return;
    pollAdvanceSlot();
}

} // namespace nex
