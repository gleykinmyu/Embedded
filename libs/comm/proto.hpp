/**
 * @file comm/proto.hpp
 * @brief IProtocol — send/recv (payload + params); Protocol — интерфейс кадра.
 */
#pragma once

#include "mem/list.hpp"
#include "ms_timer.hpp"
#include "comm/frame.hpp"

namespace comm {

class IProtocol {
public:

    virtual void onRecv(const mem::IByteReader& br) noexcept = 0;

protected:
    IProtocol() = default;
    ~IProtocol() = default;
    IProtocol(const IProtocol&) = delete;
    IProtocol& operator=(const IProtocol&) = delete;
};

template <typename BaseParam = void>
class IProtoBase : IProtocol {
public:
    using param_t = BaseParam;

    [[nodiscard]] virtual bool send(const IFrame& frame, const BaseParam& params) noexcept = 0;

protected:
    IProtoBase() = default;
    ~IProtoBase() = default;
    IProtoBase(const IProtoBase&) = delete;
    IProtoBase& operator=(const IProtoBase&) = delete;
};




} // namespace comm
