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
    : Base(link, clock)
    , _sessionBank(*this)
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

void MConsole::onFioEvent(sf::Fio::Event ev) noexcept
{
    using Event = sf::Fio::Event;
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
    if (session == nullptr) {
        return;
    }
    _lastNack = reply;
    _lastNackReq = req.msg.id;

    const smcp::Selection mask = smcp::Selection::from_raw(_lastNack.detail);
    if (!mask.empty()
        && _lastNack.error != static_cast<uint8_t>(smcp::msg::ErrorCode::MechNotFound)
        && (_lastNackReq == smcp::msg::Select::kId || _lastNackReq == smcp::msg::SetTarget::kId
            || _lastNackReq == smcp::msg::Block::kId)) {
        if (session == _link) {
            _link->getTelemetry(mask);
        }
    }
}

void MConsole::requestMechTelemetry() noexcept
{
    if (_link == nullptr || !_link->isOpen()) {
        return;
    }
    smcp::Selection mask;
    for (uint8_t i = 0; i < kMechCount; ++i) {
        mask.add(i);
    }
    _link->getTelemetry(mask);
}

void MConsole::setUiReady() noexcept
{
    _uiReady = true;
    requestMechTelemetry();
}

void MConsole::onStatus(Status status) noexcept
{
    IGroupConsole::onStatus(status);
    if (status == Status::Ready) {
        _link = start(smcp::msg::kServerIdMin);
    }
    onConsoleChanged();
}

void MConsole::onFault(smcp::Session* session, smcp::Node::Fault reason) noexcept
{
    IGroupConsole::onFault(session, reason);
    if (reason != smcp::Node::Fault::HbLost || session == nullptr || session != _link
        || session->peerId() == 0u) {
        return;
    }
    _retry = true;
}

void MConsole::update() noexcept
{
    Node::update();
    if (!_retry || _link == nullptr) {
        return;
    }
    _retry = false;
    _link->start(_link->peerId());
}

void MConsole::onLink(smcp::Session* session, bool up) noexcept
{
    IGroupConsole::onLink(session, up);
    if (up && _uiReady && session == _link) {
        requestMechTelemetry();
    }
    onConsoleChanged();
}
