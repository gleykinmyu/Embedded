#include "nexSmartApp.hpp"

#include <variant>

#include "../comp/nexAttrLexemes.hpp"
#include "../core/nexCommands.hpp"
#include "../core/nexDebug.hpp"

namespace nex {

namespace {

static constexpr uint8_t kPollTagBase = 0u;
static constexpr uint32_t kPageSwitchTimeoutMs = 1000u;
static constexpr uint32_t kSwitchPageResendMs = 500u;

} // namespace

SmartApp::SmartApp(BIF::IByteStream& stream, Rect screen, Application::ClockMsFn clockMs,
    idmap::Table& id_map_table) noexcept
    : Application(stream, screen, clockMs)
    , _table(id_map_table)
{}

void SmartApp::startDiscover() noexcept {
    _mode = IdMapMode::Discover;
    discoverArm();
    discoverBegin();
}

bool SmartApp::isDiscoverBusy() const noexcept {
    return _phase != DiscoverPhase::Idle && _phase != DiscoverPhase::Done && _phase != DiscoverPhase::Failed;
}

bool SmartApp::idMapLoadFromBuffer(const uint8_t* buf, const std::size_t len) noexcept {
    if (!_table.decode(buf, len))
        return false;
    printIdMapDiff();
    applyFromTable();
    _mode = IdMapMode::Flash;
    return true;
}

uint16_t SmartApp::printIdMapDiff() noexcept {
    uint16_t mismatches = 0u;

    for (uint16_t i = 0u; i < _table.count; ++i) {
        const idmap::Record& rec = _table.records[i];
        if (rec.panel_id == 0xFFu || rec.compiled_id < kFirstCompId)
            continue;
        if (rec.compiled_id == rec.panel_id)
            continue;

        ++mismatches;

        Page* const pg = getPage(rec.page_id);
        Component* const c = (pg != nullptr) ? pg->getComponent(rec.compiled_id) : nullptr;

        if (c != nullptr && c->name.len > 0u) {
            NEX_DBG_IDMAP("[IdMap] page%u %.*s -> id=%u\n", static_cast<unsigned>(rec.page_id),
                static_cast<int>(c->name.len), c->name.data, static_cast<unsigned>(rec.panel_id));
        } else {
            NEX_DBG_IDMAP("[IdMap] page%u #%u -> id=%u\n", static_cast<unsigned>(rec.page_id),
                static_cast<unsigned>(rec.compiled_id), static_cast<unsigned>(rec.panel_id));
        }
    }

    return mismatches;
}

void SmartApp::applyFromTable() noexcept {
    for (uint16_t i = 0u; i < _table.count; ++i) {
        const idmap::Record& rec = _table.records[i];
        if (rec.panel_id == 0xFFu || rec.panel_id == kPageCompId || rec.compiled_id < kFirstCompId)
            continue;

        Page* const pg = getPage(rec.page_id);
        if (pg == nullptr)
            continue;

        Component* const c = pg->getComponent(rec.compiled_id);
        if (c == nullptr)
            continue;

        MISC::ObjRegistry<Component, uint8_t>& reg = pg->registry();
        const MISC::RegStatus st = reg.rebind(rec.compiled_id, rec.panel_id);
        if (st != MISC::RegStatus::Ok && st != MISC::RegStatus::SwappedWithOccupiedSlot)
            nexComponentRegisterFailed(*this, *pg, *c, st);
    }
}

void SmartApp::update() noexcept {
    const uint32_t now_ms = nowMs();
    if (_mode == IdMapMode::Discover) {
        if (_phase == DiscoverPhase::Idle)
            discoverBegin();
        if (_phase != DiscoverPhase::Done && _phase != DiscoverPhase::Failed)
            discoverTick(now_ms);
    }
    Application::update();
}

void SmartApp::discoverArm() noexcept {
    _phase = DiscoverPhase::Idle;
    _page_index = 0u;
    _scan_id = kFirstCompId;
    _polled = 0u;
    _deadline_ms = 0u;
    _switch_page_last_sendme_ms = 0u;
}

void SmartApp::discoverBegin() noexcept {
    _table.clear();
    _page_index = 0u;
    _scan_id = kFirstCompId;
    _polled = 0u;
    _deadline_ms = 0u;
    _switch_page_last_sendme_ms = 0u;

    if (pageCount() == 0u) {
        discoverFail("no pages");
        return;
    }

    _phase = DiscoverPhase::SwitchPage;
    NEX_DBG_IDMAP("[IdMap] pollBegin pages=%u -> switchPage(0)\n", static_cast<unsigned>(pageCount()));
    switchPage(_page_index);
    requestCurrentPage();
}

void SmartApp::discoverTick(uint32_t now_ms) noexcept {
    switch (_phase) {
    case DiscoverPhase::Idle:
    case DiscoverPhase::Done:
    case DiscoverPhase::Failed:
    case DiscoverPhase::GetId:
        return;

    case DiscoverPhase::SwitchPage: {
        if (_deadline_ms == 0u)
            _deadline_ms = now_ms + kPageSwitchTimeoutMs;

        if (currentPageId() == _page_index) {
            _deadline_ms = 0u;
            discoverAdvanceCompId();
            return;
        }

        if (discoverCanProbe()
            && (_switch_page_last_sendme_ms == 0u
                || (now_ms - _switch_page_last_sendme_ms) >= kSwitchPageResendMs)) {
            requestCurrentPage();
            _switch_page_last_sendme_ms = now_ms;
        }

        if (now_ms >= _deadline_ms)
            discoverFail("SwitchPage timeout");
        return;
    }
    }
}

void SmartApp::discoverFail(const char* reason) noexcept {
    NEX_DBG_IDMAP("[IdMap] FAIL (%s) page_idx=%u\n", reason != nullptr ? reason : "?",
        static_cast<unsigned>(_page_index));
    _phase = DiscoverPhase::Failed;
    onDiscoverComplete(false);
}

void SmartApp::discoverFinishSuccess() noexcept {
    NEX_DBG_IDMAP("[IdMap] pollFinishSuccess records=%u\n", static_cast<unsigned>(_table.count));
    printIdMapDiff();
    applyFromTable();
    _phase = DiscoverPhase::Done;
    _mode = IdMapMode::Flash;
    onDiscoverComplete(true);
}

void SmartApp::discoverEnqueueGetId(uint8_t compiled_id) noexcept {
    Page* const pg = getPage(_page_index);
    if (pg == nullptr)
        return;
    Component* const c = pg->getComponent(compiled_id);
    if (c == nullptr)
        return;

    _phase = DiscoverPhase::GetId;
    const AttrRef target{c->name, attr::literal(attr::Id::Id)};
    enqueue(Transaction{cmd::Get::numeric(target), Route::kCompIdMapPollPageId, Route::kCompIdMapPollCompId,
        static_cast<uint8_t>(kPollTagBase + compiled_id), Transaction::Kind::GetNumeric});
}

void SmartApp::discoverAdvanceCompId() noexcept {
    const size_t toPoll = discoverComponentCount(_page_index);
    while (_polled < toPoll) {
        const uint8_t id = _scan_id++;
        if (_scan_id == 0u) {
            discoverFail("id scan overflow");
            return;
        }

        if (!discoverHasComponent(_page_index, id))
            continue;

        ++_polled;
        discoverEnqueueGetId(id);
        return;
    }

    discoverNextPage();
}

void SmartApp::discoverNextPage() noexcept {
    ++_page_index;
    if (_page_index >= pageCount()) {
        discoverFinishSuccess();
        return;
    }

    _scan_id = kFirstCompId;
    _polled = 0u;
    _phase = DiscoverPhase::SwitchPage;
    _deadline_ms = 0u;
    _switch_page_last_sendme_ms = 0u;
    NEX_DBG_IDMAP("[IdMap] pollNextPage -> switchPage(%u)\n", static_cast<unsigned>(_page_index));
    switchPage(_page_index);
}

void SmartApp::discoverOnPollResponse(uint8_t compiled_id, uint8_t panel_id) noexcept {
    if (_phase != DiscoverPhase::GetId)
        return;

    if (!discoverHasComponent(_page_index, compiled_id)) {
        discoverFail("onPollResponse: bad slot");
        return;
    }

    if (!_table.upsert(_page_index, compiled_id, panel_id)) {
        discoverFail("table upsert failed");
        return;
    }

    discoverAdvanceCompId();
}

void SmartApp::discoverOnPageChanged(uint8_t page_id) noexcept {
    if (_phase != DiscoverPhase::SwitchPage)
        return;
    if (page_id != _page_index)
        return;
    discoverAdvanceCompId();
}

bool SmartApp::discoverCanProbe() const noexcept {
    return _session.isIdle() && !_session.hasQueued();
}

uint8_t SmartApp::discoverComponentCount(uint8_t page_index) noexcept {
    Page* const pg = getPage(page_index);
    if (pg == nullptr)
        return 0u;
    return static_cast<uint8_t>(pg->compCount());
}

bool SmartApp::discoverHasComponent(uint8_t page_index, uint8_t compiled_id) noexcept {
    Page* const pg = getPage(page_index);
    if (pg == nullptr)
        return false;
    return pg->getComponent(compiled_id) != nullptr;
}

bool SmartApp::dispatchResponse(const Message& m, bool txIdle) noexcept {
    if (txIdle && !_session.isIdle()) {
        const Transaction& active = _session.active();
        if (active.kind == Transaction::Kind::GetNumeric && Route::isCompIdMapPoll(active.page_id, active.comp_id))
        {
            if (const auto* st = std::get_if<msg::Status>(&m)) {
                dispatchError(*st, active.page_id, active.comp_id);
                discoverFail("poll status");
                _session.end(false);
                return true;
            }
            if (const auto* nr = std::get_if<msg::getNumeric>(&m)) {
                discoverOnPollResponse(active.tag, static_cast<uint8_t>(nr->value & 0xFFu));
                _session.end(true);
                return true;
            }
            return false;
        }
    }

    return Application::dispatchResponse(m, txIdle);
}

void SmartApp::dispatchEvent(const Message& m) noexcept {
    // `sendme` / смена страницы — `0x66`; без этого SwitchPage не видит `currentPageId`.
    if (const auto* e = std::get_if<msg::evPage>(&m)) {
        onPageChange(e->page_id);
        return;
    }
    if (isDiscoverBusy())
        return;
    Application::dispatchEvent(m);
}

void SmartApp::onPageChange(uint8_t page_id) noexcept {
    Application::onPageChange(page_id);
    discoverOnPageChanged(page_id);
}

void SmartApp::onTimeout(const Transaction& active) noexcept {
    if (Route::isCompIdMapPoll(active.page_id, active.comp_id)) {
        if (_phase == DiscoverPhase::GetId)
            discoverFail("poll timeout");
        return;
    }
    Application::onTimeout(active);
}

} // namespace nex
