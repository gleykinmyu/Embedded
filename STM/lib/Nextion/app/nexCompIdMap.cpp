// nexCompIdMap.cpp — таблица CompIdMap, сериализация NXCI и машина опроса `Discover`.
//
// Discover (кратко):
//   pollBegin  → для каждой зарегистрированной страницы: switchPage → опрос comp_id get .id
//   → запись в `CompIdMapTable` → pollFinishSuccess → applyFromTable/setId → Flash + onCompIdMapComplete(true).
// Очередь команд и RX — в `Application::update`; здесь только enqueue и колбэки из dispatch.

#include "nexCompIdMap.hpp"
#include "nexApplication.hpp"

#include <cstring>

#include "../core/nexCommands.hpp"
#include "../core/nexDebug.hpp"

namespace nex {

namespace {

constexpr uint32_t kPageSwitchTimeoutMs = 1000u;
constexpr uint32_t kSwitchPageLogPeriodMs = 500u;

#if defined(NEX_IDMAP_DEBUG)
const char* pollPhaseName(CompIdMapPoll::Phase phase) noexcept
{
    switch (phase) {
    case CompIdMapPoll::Phase::Idle: return "Idle";
    case CompIdMapPoll::Phase::SwitchPage: return "SwitchPage";
    case CompIdMapPoll::Phase::GetId: return "GetId";
    case CompIdMapPoll::Phase::Done: return "Done";
    case CompIdMapPoll::Phase::Failed: return "Failed";
    }
    return "?";
}
#endif

void logCompName(const Literal& name) noexcept
{
    char buf[32]{};
    const uint8_t n = (name.len < 31u) ? name.len : 31u;
    if (n > 0u && name.data != nullptr)
        std::memcpy(buf, name.data, n);
    NEX_DBG_IDMAP("%s", buf);
}

void copyName(CompIdMapRecord& rec, const Literal& name) noexcept {
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
// CompIdMapTable и вспомогательные сравнения
//=============================================================================

bool compIdMapLiteralEquals(const Literal& a, const char* b, uint8_t b_len) noexcept {
    if (a.len != b_len)
        return false;
    if (a.len == 0u)
        return true;
    return std::memcmp(a.data, b, a.len) == 0;
}

const CompIdMapRecord* CompIdMapTable::find(uint8_t page_id, const Literal& comp_name) const noexcept {
    for (uint16_t i = 0u; i < count; ++i) {
        const CompIdMapRecord& r = records[i];
        if (r.page_id != page_id)
            continue;
        if (compIdMapLiteralEquals(comp_name, r.name, r.name_len))
            return &r;
    }
    return nullptr;
}

CompIdMapRecord* CompIdMapTable::find(uint8_t page_id, const Literal& comp_name) noexcept {
    return const_cast<CompIdMapRecord*>(static_cast<const CompIdMapTable*>(this)->find(page_id, comp_name));
}

bool CompIdMapTable::upsert(uint8_t page_id, const Literal& comp_name, uint8_t panel_id) noexcept {
    if (!isBound() || comp_name.len == 0u || panel_id == kPageCompId)
        return false;

    if (CompIdMapRecord* const existing = find(page_id, comp_name)) {
        existing->panel_id = panel_id;
        return true;
    }

    if (count >= capacity)
        return false;

    CompIdMapRecord& rec = records[count++];
    rec.page_id = page_id;
    rec.panel_id = panel_id;
    copyName(rec, comp_name);
    return true;
}

//=============================================================================
// Сериализация NXCI (flash/EEPROM — у пользователя)
//=============================================================================

std::size_t compIdMapTableEncode(const CompIdMapTable& table, uint8_t* buf, std::size_t cap) noexcept {
    const std::size_t need = sizeof(CompIdMapBlobHeader) + static_cast<std::size_t>(table.count) * kCompIdMapRecordWireSize;
    if (buf == nullptr || cap < need)
        return 0u;

    CompIdMapBlobHeader hdr{};
    hdr.count = table.count;
    std::memcpy(buf, &hdr, sizeof(hdr));

    uint8_t* p = buf + sizeof(hdr);
    for (uint16_t i = 0u; i < table.count; ++i) {
        const CompIdMapRecord& r = table.records[i];
        *p++ = r.page_id;
        *p++ = r.panel_id;
        *p++ = r.name_len;
        std::memcpy(p, r.name, sizeof(r.name));
        p += sizeof(r.name);
    }
    return need;
}

bool compIdMapTableDecode(CompIdMapTable& table, const uint8_t* buf, std::size_t len) noexcept {
    table.clear();
    if (!table.isBound() || buf == nullptr || len < sizeof(CompIdMapBlobHeader))
        return false;

    CompIdMapBlobHeader hdr{};
    std::memcpy(&hdr, buf, sizeof(hdr));
    if (hdr.magic != CompIdMapBlobHeader::kMagic || hdr.version != CompIdMapBlobHeader::kVersion)
        return false;

    const std::size_t need = sizeof(CompIdMapBlobHeader) + static_cast<std::size_t>(hdr.count) * kCompIdMapRecordWireSize;
    if (len < need || hdr.count > table.capacity)
        return false;

    const uint8_t* p = buf + sizeof(hdr);
    for (uint16_t i = 0u; i < hdr.count; ++i) {
        CompIdMapRecord& r = table.records[i];
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
// CompIdMap: режимы и применение таблицы
//=============================================================================

CompIdMap::CompIdMap(Application& app, CompIdMapTable& table) noexcept : _app(app), _table(table) {}

void CompIdMap::setMode(CompIdMapMode mode) noexcept {
    _mode = mode;
    if (mode == CompIdMapMode::Discover)
        _poll = CompIdMapPoll{};
}

bool CompIdMap::isDiscoveryBusy() const noexcept {
    if (_mode != CompIdMapMode::Discover)
        return false;
    const CompIdMapPoll::Phase p = _poll.phase;
    return p != CompIdMapPoll::Phase::Idle && p != CompIdMapPoll::Phase::Done && p != CompIdMapPoll::Phase::Failed;
}

bool CompIdMap::loadFromBuffer(const uint8_t* buf, const std::size_t len) noexcept {
    if (!compIdMapTableDecode(_table, buf, len))
        return false;
    applyFromTable();
    return true;
}

// Сопоставление записей таблицы с объектами по `objname` в реестре страницы.
void CompIdMap::applyFromTable() noexcept {
    for (uint16_t i = 0u; i < _table.count; ++i) {
        const CompIdMapRecord& rec = _table.records[i];
        if (rec.panel_id == 0xFFu || rec.panel_id == kPageCompId || rec.name_len == 0u)
            continue;

        Page* const pg = _app.getPage(rec.page_id);
        if (pg == nullptr)
            continue;

        MISC::ObjRegistry<Component, uint8_t>& reg = pg->registry();

        bool matched = false;
        uint8_t comp_id = 0u;
        while (Component* const c = reg.next(comp_id)) {
            if (matched)
                break;
            if (!compIdMapLiteralEquals(c->name, rec.name, rec.name_len))
                continue;
            const MISC::RegStatus st = reg.rebind(c->id(), rec.panel_id);
            if (st != MISC::RegStatus::Ok && st != MISC::RegStatus::SwappedWithOccupiedSlot)
                nexComponentRegisterFailed(_app, *pg, *c, st);
            matched = true;
        }
    }
}

//=============================================================================
// Discover: машина опроса
//=============================================================================

void CompIdMap::pollBegin() noexcept {
    _table.clear();
    _poll = CompIdMapPoll{};

    if (_app.pageCount() == 0u) {
        pollFail("no pages registered");
        return;
    }

    _poll.page_id = 0u;
    _poll.scan_id = kFirstCompId;
    _poll.polled = 0u;
    _poll.phase = CompIdMapPoll::Phase::SwitchPage;
    NEX_DBG_IDMAP("[IdMap] pollBegin pages=%u curPage=%u -> switchPage(%u)+sendme\n",
        static_cast<unsigned>(_app.pageCount()), static_cast<unsigned>(_app._currentPageId),
        static_cast<unsigned>(_poll.page_id));
    // `page N` не всегда шлёт 0x66; `sendme` — явный запрос evPage с id текущей страницы.
    _app.switchPage(_poll.page_id);
    _app.requestCurrentPage();
}

void CompIdMap::updateDiscovery(uint32_t now_ms) noexcept {
    if (_mode != CompIdMapMode::Discover)
        return;

    if (_poll.phase == CompIdMapPoll::Phase::Idle)
        pollBegin();

    if (_poll.phase == CompIdMapPoll::Phase::Done || _poll.phase == CompIdMapPoll::Phase::Failed)
        return;

    pollTick(now_ms);
}

void CompIdMap::pollFail(const char* reason) noexcept {
    NEX_DBG_IDMAP("[IdMap] FAIL (%s) phase=%s page=%u scan_id=%u curPage=%u\n", reason != nullptr ? reason : "?",
        pollPhaseName(_poll.phase), static_cast<unsigned>(_poll.page_id), static_cast<unsigned>(_poll.scan_id),
        static_cast<unsigned>(_app._currentPageId));
    _poll.phase = CompIdMapPoll::Phase::Failed;
    _app.onCompIdMapComplete(false);
}

void CompIdMap::pollFinishSuccess() noexcept {
    NEX_DBG_IDMAP("[IdMap] pollFinishSuccess records=%u\n", static_cast<unsigned>(_table.count));
    applyFromTable();
    _poll.phase = CompIdMapPoll::Phase::Done;
    _mode = CompIdMapMode::Flash;
    _app.onCompIdMapComplete(true);
}

void CompIdMap::pollEnqueueGetId(Component& c, uint8_t comp_id) noexcept {
    const AttrRef target{c.name, kCompIdMapAttrLexeme};
    NEX_DBG_IDMAP("[IdMap] enqueue get .id page=%u comp_id=%u obj=", static_cast<unsigned>(_poll.page_id),
        static_cast<unsigned>(comp_id));
    logCompName(c.name);
    NEX_DBG_IDMAP(" tag=%u\n", static_cast<unsigned>(kCompIdMapPollTagBase + comp_id));
    _app.enqueue(Transaction{cmd::Get::numeric(target), kRoutePageId, kRouteCompId,
        static_cast<uint8_t>(kCompIdMapPollTagBase + comp_id), Transaction::State::AwaitingNumericGet});
}

void CompIdMap::pollAdvanceCompId() noexcept {
    Page* const pg = _app.getPage(_poll.page_id);
    if (pg == nullptr) {
        pollFail("page not found");
        return;
    }

    const size_t toPoll = pg->compCount();
    NEX_DBG_IDMAP("[IdMap] pollAdvanceCompId page=%u toPoll=%u scan_id=%u polled=%u\n",
        static_cast<unsigned>(_poll.page_id), static_cast<unsigned>(toPoll),
        static_cast<unsigned>(_poll.scan_id), static_cast<unsigned>(_poll.polled));

    while (_poll.polled < toPoll) {
        const uint8_t id = _poll.scan_id++;
        if (_poll.scan_id == 0u) {
            pollFail("pollAdvanceCompId: id scan overflow");
            return;
        }
        Component* const c = pg->getComponent(id);
        if (c == nullptr)
            continue;

        _poll.phase = CompIdMapPoll::Phase::GetId;
        pollEnqueueGetId(*c, id);
        ++_poll.polled;
        return;
    }

    pollNextPage();
}

void CompIdMap::pollNextPage() noexcept {
    ++_poll.page_id;
    if (_poll.page_id >= _app.pageCount()) {
        pollFinishSuccess();
        return;
    }

    _poll.scan_id = kFirstCompId;
    _poll.polled = 0u;
    _poll.phase = CompIdMapPoll::Phase::SwitchPage;
    NEX_DBG_IDMAP("[IdMap] pollNextPage -> switchPage(%u) curPage=%u\n", static_cast<unsigned>(_poll.page_id),
        static_cast<unsigned>(_app._currentPageId));
    _app.switchPage(_poll.page_id);
}

// SwitchPage: ждём evPage (`onPageChange`) или уже актуальный `_currentPageId` (pollTick).
void CompIdMap::pollTick(uint32_t now_ms) noexcept {
    switch (_poll.phase) {
    case CompIdMapPoll::Phase::Idle:
    case CompIdMapPoll::Phase::Done:
    case CompIdMapPoll::Phase::Failed:
    case CompIdMapPoll::Phase::GetId:
        return;

    case CompIdMapPoll::Phase::SwitchPage: {
        if (_poll.deadline_ms == 0u) {
            _poll.deadline_ms = now_ms + kPageSwitchTimeoutMs;
            _poll.switch_page_last_sendme_ms = 0u;
            NEX_DBG_IDMAP("[IdMap] SwitchPage wait target=%u cur=%u deadline_ms=%lu\n",
                static_cast<unsigned>(_poll.page_id), static_cast<unsigned>(_app._currentPageId),
                static_cast<unsigned long>(_poll.deadline_ms));
        }
        if (_app._currentPageId == _poll.page_id) {
            NEX_DBG_IDMAP("[IdMap] SwitchPage OK (curPage match)\n");
            _poll.deadline_ms = 0u;
            _poll.switch_page_last_log_ms = 0u;
            _poll.switch_page_last_sendme_ms = 0u;
            pollAdvanceCompId();
            return;
        }
        if (_app._session.isIdle() && !_app._session.hasQueued()
            && (_poll.switch_page_last_sendme_ms == 0u
                || (now_ms - _poll.switch_page_last_sendme_ms) >= kSwitchPageLogPeriodMs)) {
            _app.requestCurrentPage();
            _poll.switch_page_last_sendme_ms = now_ms;
        }
        if (_poll.switch_page_last_log_ms == 0u
            || (now_ms - _poll.switch_page_last_log_ms) >= kSwitchPageLogPeriodMs) {
            _poll.switch_page_last_log_ms = now_ms;
            NEX_DBG_IDMAP("[IdMap] SwitchPage ... target=%u cur=%u left_ms=%lu q=%u\n",
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

void CompIdMap::onPollResponse(uint8_t comp_id, const msg::getNumeric& response) noexcept {
    Page* const pg = _app.getPage(_poll.page_id);
    if (pg == nullptr) {
        pollFail("onPollResponse: page missing");
        return;
    }

    Component* const c = pg->getComponent(comp_id);
    if (c == nullptr) {
        NEX_DBG_IDMAP("[IdMap] onPollResponse bad comp_id=%u\n", static_cast<unsigned>(comp_id));
        pollFail("onPollResponse: bad comp_id");
        return;
    }
    const uint8_t panel_id = static_cast<uint8_t>(response.value & 0xFFu);

    NEX_DBG_IDMAP("[IdMap] onPollResponse page=%u comp_id=%u obj=", static_cast<unsigned>(_poll.page_id),
        static_cast<unsigned>(comp_id));
    logCompName(c->name);
    NEX_DBG_IDMAP(" panel_id=%u raw=%ld\n", static_cast<unsigned>(panel_id), static_cast<long>(response.value));

    if (!_table.upsert(_poll.page_id, c->name, panel_id)) {
        pollFail("table upsert failed");
        return;
    }

    // Не вызывать rebind здесь: смена id ломает tag=comp_id в очереди опроса.
    // panel_id применится в pollFinishSuccess → applyFromTable().
    pollAdvanceCompId();
}

void CompIdMap::onSessionEnd(bool success) noexcept {
    if (_poll.phase != CompIdMapPoll::Phase::GetId)
        return;
    if (!success) {
        NEX_DBG_IDMAP("[IdMap] onSessionEnd FAIL (timeout or NIS error)\n");
        pollFail("session end");
    }
}

void CompIdMap::onPageChange(uint8_t page_id) noexcept {
    if (_poll.phase != CompIdMapPoll::Phase::SwitchPage)
        return;
    NEX_DBG_IDMAP("[IdMap] onPageChange evPage=%u want=%u\n", static_cast<unsigned>(page_id),
        static_cast<unsigned>(_poll.page_id));
    if (page_id != _poll.page_id)
        return;
    pollAdvanceCompId();
}

} // namespace nex
