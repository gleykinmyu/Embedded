#include <stdint.h>
#include <string.h>

// --- LAYER 3: PROTOCOL DEFINITIONS (NexProtocol.h) ---
namespace Nextion {
    namespace Physical {
        static constexpr uint8_t  TERM_BYTE  = 0xFF;
        static constexpr uint8_t  TERM_COUNT = 3;
    }

    enum class Response : uint8_t {
        InvalidCmd = 0x00, Success = 0x01, InvalidComp = 0x02, InvalidVar = 0x1A,
        TouchEvent = 0x65, PageId = 0x66, Number = 0x71, String = 0x70,
        Ready = 0x88, Sleep = 0x86, Wakeup = 0x87
    };

    enum class TouchState : uint8_t { Release = 0x00, Press = 0x01 };

    struct Event {
        Response type;
        union {
            struct { uint8_t pid; uint8_t cid; TouchState state; } touch;
            int32_t numeric;
            struct { const char* volatile_text; uint16_t length; } string;
            uint8_t raw[4];
        };
    };

    namespace Prop {
        extern constexpr char Value[] = ".val";
        extern constexpr char Text[]  = ".txt";
        extern constexpr char Color[] = ".pco";
        extern constexpr char Pic[]   = ".pic";
    }

    struct Config {
        static constexpr uint16_t MAX_PAYLOAD = 64;
        static constexpr uint16_t RETRY_BUF   = 64;
        static constexpr uint16_t TIMEOUT_MS  = 150;
    };
}

// --- LAYER 1: PHYSICAL INTERFACE (IStream.h) ---
namespace Nextion {
    class IStream {
    public:
        enum class Status : uint8_t { OK = 0, OverFlow, BitError, Disconnected };
        virtual ~IStream() = default;
        virtual uint16_t available() const = 0;
        virtual uint16_t availableForWrite() const = 0;
        virtual Status   read(uint8_t* outData) = 0;
        virtual Status   write(const uint8_t* data, uint16_t len) = 0;
        virtual void     purge() = 0;
        virtual void     flush() = 0;
        virtual Status   getStatus() const = 0;
    };
}

// --- LAYER 2: DATA LINK (NexFramer.h) ---
namespace Nextion {
    struct Frame {
        uint8_t header;
        uint8_t payload[Config::MAX_PAYLOAD];
        uint16_t length;
    };

    class NexFramer {
        enum class State { WaitHeader, Collect, WaitTerm };
        State   _state = State::WaitHeader;
        Frame   _current;
        uint8_t _terms = 0;
    public:
        void reset() { _state = State::WaitHeader; _terms = 0; _current.length = 0; }
        bool feed(uint8_t b, Frame& out) {
            switch (_state) {
                case State::WaitHeader:
                    _current.header = b; _current.length = 0; _terms = 0;
                    _state = State::Collect; break;
                case State::Collect:
                    if (b == Physical::TERM_BYTE) { _terms = 1; _state = State::WaitTerm; }
                    else { if (_current.length < Config::MAX_PAYLOAD) _current.payload[_current.length++] = b; }
                    break;
                case State::WaitTerm:
                    if (b == Physical::TERM_BYTE) {
                        if (++_terms >= Physical::TERM_COUNT) { out = _current; reset(); return true; }
                    } else {
                        for (uint8_t i=0; i<_terms; i++) _current.payload[_current.length++] = Physical::TERM_BYTE;
                        _current.payload[_current.length++] = b;
                        _terms = 0; _state = State::Collect;
                    }
                    break;
            }
            return false;
        }
    };
}

// --- LAYER 3 & 4: TRANSLATOR & GATE (NexGate.h) ---
namespace Nextion {
    class NexTranslator {
    public:
        static Event parse(const Frame& f) {
            Event ev; ev.type = static_cast<Response>(f.header);
            if (ev.type == Response::TouchEvent) {
                ev.touch.pid = f.payload[0]; ev.touch.cid = f.payload[1];
                ev.touch.state = static_cast<TouchState>(f.payload[2]);
            } else if (ev.type == Response::Number) {
                memcpy(&ev.numeric, f.payload, 4);
            } else if (ev.type == Response::String) {
                ev.string.volatile_text = (const char*)f.payload;
                ev.string.length = f.length;
            }
            return ev;
        }
    };

    class NexGate {
        IStream& _stream; NexFramer _framer;
        char _retryBuf[Config::RETRY_BUF];
        uint32_t _ts = 0; bool _isWaiting = false;
    public:
        NexGate(IStream& s) : _stream(s) {}
        void send(const char* cmd) { strncpy(_retryBuf, cmd, Config::RETRY_BUF); transmit(cmd); }
        bool update(Event& ev) {
            while (_stream.available()) {
                uint8_t b;
                if (_stream.read(&b) == IStream::Status::OK) {
                    Frame f; if (_framer.feed(b, f)) { ev = NexTranslator::parse(f); _isWaiting = false; return true; }
                } else { _framer.reset(); _stream.purge(); }
            }
            // Simple Retry logic (L4)
            // if (_isWaiting && (millis() - _ts > Config::TIMEOUT_MS)) transmit(_retryBuf); 
            return false;
        }
        void transmit(const char* c) {
            _stream.write((uint8_t*)c, strlen(c));
            for(int i=0; i<3; i++) { uint8_t b = Physical::TERM_BYTE; _stream.write(&b, 1); }
            _stream.flush(); _isWaiting = true; // _ts = millis();
        }
    };
}

// --- LAYER 5: SESSION (NexSession.h) ---
namespace Nextion {
    struct Transaction { const char* cmd; Response expect; void* target; uint16_t sz; };

    class NexSession {
        NexGate _gate; void* _target = nullptr; Response _expect; bool _busy = false;
    public:
        NexSession(IStream& s) : _gate(s) {}
        bool execute(const Transaction& t) {
            if (_busy) return false;
            _target = t.target; _expect = t.expect; _busy = true;
            _gate.send(t.cmd); return true;
        }
        void update(class NexManagerBase& mgr) {
            Event ev;
            if (_gate.update(ev)) {
                if (_busy && ev.type == _expect) {
                    if (_target && ev.type == Response::Number) *(int32_t*)_target = ev.numeric;
                    _busy = false;
                } else { mgr.dispatchAsync(ev); }
            }
        }
        bool isIdle() const { return !_busy; }
    };
}

// --- LAYER 7: APPLICATION (Manager, Page, Component) ---
namespace Nextion {
    class NexManagerBase {
    public:
        virtual void dispatchAsync(const Event& ev) = 0;
        virtual NexSession& getSession() = 0;
    };

    class NexComponent {
    protected:
        class NexPageBase& _page; const char* _name; uint16_t _cid = 0xFFFF;
    public:
        NexComponent(NexPageBase& p, const char* n);
        virtual void handle(TouchState s) = 0;
        const char* getName() const { return _name; }
        uint16_t* getCidPtr() { return &_cid; }
    };

    class NexPageBase {
    public:
        virtual uint8_t getId() const = 0;
        virtual void dispatch(uint16_t cid, TouchState s) = 0;
        virtual NexManagerBase& getMgr() = 0;
    };

    template <uint8_t MAX_PAGES, uint16_t MAX_TOTAL_COMPS>
    class NexManager : public NexManagerBase {
        NexSession _session;
        NexPageBase* _pages[MAX_PAGES]; uint8_t _pCount = 0;
        uint16_t _cidMap[MAX_TOTAL_COMPS]; bool _ready = false;
    public:
        NexManager(IStream& s) : _session(s) {}
        void update() { _session.update(*this); /* Add SmartInit Logic here */ }
        NexSession& getSession() override { return _session; }
        void dispatchAsync(const Event& ev) override {
            if (ev.type == Response::TouchEvent) {
                for(int i=0; i<_pCount; i++) 
                    if(_pages[i]->getId() == ev.touch.pid) _pages[i]->dispatch(ev.touch.cid, ev.touch.state);
            }
        }
    };

    // --- PROPERTY TEMPLATE ---
    template <typename T, const char* PName>
    class NexProperty {
        NexComponent& _owner;
    public:
        NexProperty(NexComponent& o) : _owner(o) {}
        void operator=(T val) { /* Send set cmd via session */ }
        bool get(T* target) { /* Send get cmd via session */ }
    };
}
