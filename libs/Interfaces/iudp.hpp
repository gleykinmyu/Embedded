/**
 * @file iudp.hpp
 * @brief Датаграммный UDP (Art-Net, sACN/E1.31). Без сокетов BSD в контракте.
 */
#pragma once

#include <cstddef>
#include <cstdint>

namespace BIF {
namespace NET {

/// IPv4 в host order (0xC0A8010A = 192.168.1.10).
using Ipv4 = uint32_t;

struct Endpoint {
    Ipv4 ip = 0;
    uint16_t port = 0;
};

[[nodiscard]] constexpr Ipv4 ipv4(uint8_t a, uint8_t b, uint8_t c, uint8_t d) noexcept
{
    return (static_cast<Ipv4>(a) << 24) | (static_cast<Ipv4>(b) << 16)
        | (static_cast<Ipv4>(c) << 8) | static_cast<Ipv4>(d);
}

[[nodiscard]] constexpr bool isMulticast(Ipv4 ip) noexcept
{
    return (ip >> 24) >= 224u && (ip >> 24) <= 239u;
}

/**
 * UDP-сокет. Реализация — lwIP / W5500 / ENET драйвер платы.
 *
 * `recvFrom` не блокирует: 0 — очередь пуста.
 * `joinGroup` нужен sACN (239.255.universeHi.universeLo).
 */
class IUdp {
public:
    virtual ~IUdp() = default;

    virtual bool open(uint16_t localPort) = 0;
    virtual void close() = 0;
    [[nodiscard]] virtual bool isOpen() const = 0;

    virtual bool sendTo(const uint8_t* data, std::size_t n, Endpoint dest) = 0;
    virtual std::size_t recvFrom(uint8_t* buf, std::size_t maxN, Endpoint& src) = 0;

    virtual bool joinGroup(Ipv4 group) = 0;
    virtual bool leaveGroup(Ipv4 group) = 0;

    [[nodiscard]] virtual std::size_t available() const = 0;

    virtual bool setTtl(uint8_t ttl)
    {
        (void)ttl;
        return true;
    }
};

} // namespace NET
} // namespace BIF
