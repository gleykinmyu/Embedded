#pragma once
#include <stdint.h>
#include <variant>

#include "nexProtocol.hpp"
#include "nexComponents.hpp"

namespace Nextion {
    namespace detail {

        /** Сверка `Message` с байтом, записанным в `Transaction::expect_header`. */
        inline bool message_matches_expect(const Message& m, uint8_t expect) noexcept {
            return std::visit(
                [expect](const auto& v) -> bool {
                    using T = std::decay_t<decltype(v)>;
                    if constexpr (std::is_same_v<T, msg::Unknown>) {
                        if (v.reason == msg::Unknown::Reason::UninitializedMessage)
                            return false;
                        return v.header == expect;
                    }
                    if constexpr (std::is_same_v<T, msg::StatusResponse>)
                        return static_cast<uint8_t>(v.status) == expect;
                    if constexpr (std::is_same_v<T, msg::NumericResponse>)
                        return expect == msg::NumericResponse::Header;
                    if constexpr (std::is_same_v<T, msg::StringResponse>)
                        return expect == msg::StringResponse::Header;
                    if constexpr (std::is_same_v<T, msg::TouchCompEvent>)
                        return expect == msg::TouchCompEvent::Header;
                    if constexpr (std::is_same_v<T, msg::TouchXYEvent>)
                        return static_cast<uint8_t>(v.plane) == expect;
                    if constexpr (std::is_same_v<T, msg::PageEvent>)
                        return expect == msg::PageEvent::Header;
                    if constexpr (std::is_same_v<T, msg::SystemEvent>)
                        return static_cast<uint8_t>(v.code) == expect;
                    return false;
                },
                m);
        }

    } // namespace detail

    class NexManagerBase;

    struct Transaction {
        const char* cmd;
        uint8_t expect_header;
        void* target;
        uint16_t sz;
    };

    class NexSession {
        NexGate _gate;
        /** Приёмник кадра: живёт в сессии, не внутри `NexGate`. */
        Message _rx{};
        void* _target = nullptr;
        uint8_t _expect_header = 0;
        bool _busy = false;

    public:
        explicit NexSession(BIF::IByteStream& s) : _gate(s) {}

        bool execute(const Transaction& t, uint32_t nowMs) {
            if (_busy) return false;
            _target = t.target;
            _expect_header = t.expect_header;
            _busy = true;
            _gate.send(t.cmd, nowMs);
            return true;
        }

        void update(NexManagerBase& mgr, uint32_t nowMs);

        bool isIdle() const { return !_busy; }
    };

    class NexManagerBase {
    public:
        virtual void dispatchAsync(const Message& msg) = 0;
        virtual NexSession& getSession() = 0;
    };

    class NexPageBase;

    class NexComponent {
    protected:
        NexPageBase& _page;
        const char* _name;
        uint16_t _cid = 0xFFFF;

    public:
        NexComponent(NexPageBase& p, const char* n) : _page(p), _name(n) {}
        virtual ~NexComponent() = default;
        virtual void handle(msg::TouchState s) = 0;
        const char* getName() const { return _name; }
        uint16_t* getCidPtr() { return &_cid; }
    };

    class NexPageBase {
    public:
        virtual ~NexPageBase() = default;
        virtual uint8_t getId() const = 0;
        virtual void dispatch(uint16_t cid, msg::TouchState s) = 0;
        virtual NexManagerBase& getMgr() = 0;
    };

    template <uint8_t MAX_PAGES, uint16_t MAX_TOTAL_COMPS>
    class NexManager : public NexManagerBase {
        NexSession _session;
        NexPageBase* _pages[MAX_PAGES];
        uint8_t _pCount = 0;
        uint16_t _cidMap[MAX_TOTAL_COMPS];
        bool _ready = false;

    public:
        explicit NexManager(BIF::IByteStream& s) : _session(s) {}
        void update(uint32_t nowMs) { _session.update(*this, nowMs); }
        NexSession& getSession() override { return _session; }

        void dispatchAsync(const Message& msg) override {
            const auto* touch = std::get_if<msg::TouchCompEvent>(&msg);
            if (!touch) return;
            for (int i = 0; i < _pCount; ++i) {
                if (_pages[i]->getId() == touch->page_id)
                    _pages[i]->dispatch(touch->component_id, touch->state);
            }
        }
    };

    template <typename T, const char* PName>
    class NexProperty {
        NexComponent& _owner;

    public:
        explicit NexProperty(NexComponent& o) : _owner(o) {}
        void operator=(T /*val*/) { /* TODO: send set cmd via session */ }
        bool get(T* /*target*/) { return false; }
    };

    inline void NexSession::update(NexManagerBase& mgr, uint32_t nowMs) {
        bool send_aborted = false;
        if (!_gate.update(_rx, nowMs, &send_aborted)) {
            if (send_aborted) {
                _busy = false;
                _target = nullptr;
            }
            return;
        }

        if (_busy && detail::message_matches_expect(_rx, _expect_header)) {
            if (const auto* num = std::get_if<msg::NumericResponse>(&_rx)) {
                if (_target)
                    *static_cast<int32_t*>(_target) = num->value;
            }
            _busy = false;
        } else {
            mgr.dispatchAsync(_rx);
        }
    }

} // namespace Nextion
