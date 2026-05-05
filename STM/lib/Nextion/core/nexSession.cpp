#include "nexSession.hpp"

namespace nex {

void Session::resetConversationState() noexcept {
    _pendingGet = false;
    _awaitStatusAfterCmd = false;
    _tp = SessionTransparentPhase::Idle;
    _transparentRemaining = 0;
}

bool Session::pushCommand(const Command& cmd) {
    if (_tp != SessionTransparentPhase::Idle)
        return false;
    if (!_xc.pushCommand(cmd))
        return false;
    if (cmd.awaitsGetAttributeReply())
        _pendingGet = true;
    else
        _pendingGet = false;
    if (_reportCmdAck)
        _awaitStatusAfterCmd = true;
    return true;
}

bool Session::pushTransparentPreamble(const TransparentCommand& cmd) {
    if (_tp != SessionTransparentPhase::Idle)
        return false;
    if (!_xc.pushTransparentPreamble(cmd))
        return false;
    _tp = SessionTransparentPhase::AwaitingReady;
    _transparentRemaining = cmd.transparentPayloadBytes();
    if (_reportCmdAck)
        _awaitStatusAfterCmd = true;
    return true;
}

size_t Session::writeTransparentPayload(const uint8_t* data, size_t len) noexcept {
    if (_tp != SessionTransparentPhase::PumpingPayload || data == nullptr || len == 0u || _transparentRemaining == 0u)
        return 0u;
    const uint32_t cap = _transparentRemaining;
    const size_t toSend = len < static_cast<size_t>(cap) ? len : static_cast<size_t>(cap);
    const size_t w = _xc.writeTransparentRaw(data, toSend);
    if (w == 0u)
        return 0u;
    _transparentRemaining = static_cast<uint32_t>(cap - static_cast<uint32_t>(w));
    if (_transparentRemaining == 0u)
        _tp = SessionTransparentPhase::AwaitingBlockAck;
    return w;
}

bool Session::dispatchMessage(Message& raw, SessionIncoming& out) noexcept {
    if (std::holds_alternative<msg::StatusResponse>(raw)) {
        if (_reportCmdAck && _awaitStatusAfterCmd) {
            _awaitStatusAfterCmd = false;
            out.route = SessionRoute::CommandAck;
            out.msg = std::move(raw);
            return true;
        }
        out.route = SessionRoute::App;
        out.msg = std::move(raw);
        return true;
    }

    if (std::holds_alternative<msg::SystemEvent>(raw)) {
        const auto& se = std::get<msg::SystemEvent>(raw);
        const msg::SystemEvent::Code c = se.code;

        if (_tp == SessionTransparentPhase::AwaitingReady && c == msg::SystemEvent::Code::TransparentReadyToReceive) {
            _tp = SessionTransparentPhase::PumpingPayload;
            out.route = SessionRoute::Transparent;
            out.msg = std::move(raw);
            return true;
        }
        if (_tp == SessionTransparentPhase::AwaitingBlockAck && c == msg::SystemEvent::Code::TransparentBlockComplete) {
            _tp = SessionTransparentPhase::Idle;
            _transparentRemaining = 0;
            out.route = SessionRoute::Transparent;
            out.msg = std::move(raw);
            return true;
        }
        out.route = SessionRoute::App;
        out.msg = std::move(raw);
        return true;
    }

    if (_pendingGet) {
        if (std::holds_alternative<msg::NumericResponse>(raw) || std::holds_alternative<msg::StringResponse>(raw)) {
            _pendingGet = false;
            out.route = SessionRoute::GetReply;
            out.msg = std::move(raw);
            return true;
        }
    }

    out.route = SessionRoute::App;
    out.msg = std::move(raw);
    return true;
}

bool Session::receive(SessionIncoming& out) {
    Message raw;
    if (!_xc.receive(raw))
        return false;
    return dispatchMessage(raw, out);
}

} // namespace nex
