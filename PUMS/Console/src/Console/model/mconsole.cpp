/**
 * @file mconsole.cpp
 * @brief MConsole: сессия шоу, Ack/Nack, хуки UI.
 */

#include "model/mconsole.hpp"

#include "core/nexDebug.hpp"

void Fio::onEvent(Event ev) noexcept
{
    _owner.onFioEvent(ev);
}

MConsole::MConsole(BIF::IVolume& volume, BIF::IDirectory& dir, BIF::IFile& file, BIF::IFile& bak,
                   smcp::ILink& link, smcp::Node::ClockFn clock) noexcept
    : Base(link, clock)
    , _sessionBank(*this)
    , _profileRanges(_profileSlots)
    , show(*this)
    , cmechs(*this, show.group)
    , browser(volume, dir)
    , fio(*this, browser, show, file, bak)
{
    /* show после Base: указатель на патч выставляем здесь. */
    patch = &show.patch;
}

void MConsole::begin(uint8_t console_id) noexcept
{
    Base::begin(console_id);
    if (fio.useBak()) {
        NEX_DBG("Show from LKG: '%s'\n", show.name());
        remapAxes();
    } else {
        NEX_DBG("No show in LKG (%s)\n", sf::Fio::cstr(fio.status()));
    }
}

bool MConsole::hasSelection() const noexcept
{
    for (uint8_t i = 0; i < kMechCount; ++i) {
        if (cmechs[i].isSelectedBy(consoleId())) {
            return true;
        }
    }
    return false;
}

smcp::segment::Selection MConsole::selectionFromMechs() const noexcept
{
    smcp::segment::Selection selection;
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
        remapAxes();
        onConsoleChanged();
        break;
    case Event::Saved:
        onConsoleChanged();
        break;
    case Event::Removed:
        break;
    }
}

void MConsole::onNack(smcp::Session* session, const smcp::TxSlot& req,
                      const smcp::session::Nack& reply) noexcept
{
    smcp::show::GroupConsole::onNack(session, req, reply);
    if (session == nullptr) {
        return;
    }
    _lastNack = reply;
    _lastNackReq = req.msg.id;

    const smcp::segment::Selection mask = smcp::segment::Selection::from_raw(_lastNack.detail);
    if (!mask.empty()
        && _lastNack.error.raw()
            != static_cast<uint8_t>(smcp::segment::Error::AxisNotFound)
        && (_lastNackReq == smcp::segment::Select::kId || _lastNackReq == smcp::segment::SetTarget::kId
            || _lastNackReq == smcp::segment::Block::kId)) {
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
    smcp::segment::Selection mask;
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

void MConsole::onState(State state) noexcept
{
    smcp::show::GroupConsole::onState(state);
    if (state == State::Ready) {
        _link = start(smcp::segment::kServerIdMin);
    }
    onConsoleChanged();
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

void MConsole::onLink(smcp::Session* session, Link link) noexcept
{
    smcp::show::GroupConsole::onLink(session, link);
    if (link == Link::HbLost && session != nullptr && session == _link
        && session->peerId() != 0u) {
        _retry = true;
    }
    if (link == Link::Up && _uiReady && session == _link) {
        requestMechTelemetry();
    }
    onConsoleChanged();
}
