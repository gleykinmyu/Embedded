/**
 * @file mconsole.cpp
 * @brief MConsole: сессия шоу, Ack/Nack, хуки UI.
 */

#include "model/mconsole.hpp"

void Fio::onEvent(Event ev) noexcept
{
    _owner.onFioEvent(ev);
}

MConsole::MConsole(BIF::IVolume& volume, BIF::IDirectory& dir, BIF::IFile& file, BIF::IFile& bak,
                   smcp::ILink& link, smcp::Node::ClockFn clock) noexcept
    : smcp::GroupConsole<kMechCount>(link, clock)
    , _incoming(*this)
    , show(*this)
    , cmechs(*this, show.group)
    , browser(volume, dir)
    , fio(*this, browser, show, _incoming, file, bak)
{}

bool MConsole::hasSelection() const noexcept
{
    for (uint8_t i = 0; i < kMechCount; ++i) {
        if (cmechs[i].isSelectedBy(consoleId())) {
            return true;
        }
    }
    return false;
}

smcp::Selection MConsole::selectionFromMechs() const noexcept
{
    smcp::Selection selection;
    for (uint8_t i = 0; i < kMechCount; ++i) {
        if (cmechs[i].isSelectedBy(consoleId())) {
            selection.add(i);
        }
    }
    return selection;
}

void MConsole::onFioEvent(smcp::file::FIOManager::Event ev) noexcept
{
    using Event = smcp::file::FIOManager::Event;
    switch (ev) {
    case Event::New:
    case Event::Loaded:
        clearActiveGroup();
        _mode = Mode::Work;
        onConsoleChanged();
        break;
    case Event::Saved:
        onConsoleChanged();
        break;
    case Event::Removed:
        break;
    }
}

void MConsole::onTelemetry(uint8_t src_id, const smcp::msg::Telemetry& body) noexcept
{
    GroupConsole::onTelemetry(src_id, body);
    onMechChanged(body.mech_id);
}

void MConsole::onNack(smcp::Session* session, const smcp::TxSlot& req,
                      const smcp::msg::Nack& reply) noexcept
{
    IGroupConsole::onNack(session, req, reply);
    if (session != &_primary) {
        return;
    }
    _lastNack = reply;
    _lastNackReq = req.msg.id;

    const uint8_t detail = _lastNack.detail;
    if (detail != smcp::msg::kNackDetailNone && detail < kMechCount
        && (_lastNackReq == smcp::msg::Select::kId || _lastNackReq == smcp::msg::SetTarget::kId
            || _lastNackReq == smcp::msg::Block::kId)) {
        smcp::Selection mask;
        mask.add(detail);
        getTelemetry(mask);
    }
}

void MConsole::requestMechTelemetry() noexcept
{
    smcp::Selection mask;
    for (uint8_t i = 0; i < kMechCount; ++i) {
        mask.add(i);
    }
    getTelemetry(mask);
}

void MConsole::setUiReady() noexcept
{
    _uiReady = true;
    tryGoOnline();
}

void MConsole::onPhase(Phase phase) noexcept
{
    if (phase == Phase::Online) {
        requestMechTelemetry();
    }
    onConsoleChanged();
}
