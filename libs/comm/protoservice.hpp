/**
 * @file comm/protoservice.hpp
 * @brief ProtoService — список протоколов над IFrameStream.
 */
#pragma once

#include "comm/iservice.hpp"
#include "comm/proto.hpp"
#include "iframestream.hpp"
#include "mem/list.hpp"
#include "mem/visit.hpp"

namespace comm {

template <typename Frame>
class ProtoService : public IService {
public:
    using baseFrame_t = Frame;
    using Stream = BIF::IFrameStream<Frame>;
    using proto_t = Protocol<Frame>;

    explicit ProtoService(ServiceManager* mgr, Stream& stream) noexcept
        : IService(mgr)
        , _stream(stream)
    {}

    bool attach(proto_t& p) noexcept { return _list.pushBack(p); }

    void tick(MISC::time_t now) noexcept override
    {
        pumpRx(now);
        tickTx(now);
    }

protected:
    ~ProtoService() = default;
    ProtoService(const ProtoService&) = delete;
    ProtoService& operator=(const ProtoService&) = delete;

private:
    void pumpRx(MISC::time_t now) noexcept
    {
        baseFrame_t frame{};
        while (_stream.recv(frame)) {
            _list.forEach([&](proto_t& p) -> bool { return p.onFrame(frame, now); });
        }
    }

    void tickTx(MISC::time_t now) noexcept
    {
        if (_list.empty()) {
            return;
        }
        proto_t* const start = (_cursor != nullptr) ? _cursor : _list.front();
        proto_t* p = start;
        bool can_send = true;
        do {
            p->tick(now);
            if (can_send) {
                baseFrame_t out{};
                if (p->peekTx(out, now)) {
                    if (!_stream.send(out)) {
                        can_send = false;
                    } else {
                        _cursor = _list.next(*p);
                    }
                }
            }
            proto_t* const nxt = _list.next(*p);
            p = (nxt != nullptr) ? nxt : _list.front();
        } while (p != start);
    }

    Stream& _stream;
    mem::List<proto_t> _list;
    proto_t* _cursor = nullptr;
};

} // namespace comm
