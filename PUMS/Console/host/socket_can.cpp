/**
 * @file socket_can.cpp
 */

#include "socket_can.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>

namespace smcp {
namespace host {
namespace {

bool setNonBlocking(int fd) noexcept
{
    const int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return false;
    }
    return ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

bool setReuseAddr(int fd) noexcept
{
    const int one = 1;
    return ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) == 0;
}

} // namespace

SocketCan::~SocketCan()
{
    close();
}

bool SocketCan::configure(Role role, uint16_t port) noexcept
{
    if (_open) {
        return false;
    }
    _role = role;
    _port = port;
    _configured = true;
    return true;
}

bool SocketCan::open(uint32_t /*bitrate*/)
{
    if (!_configured || _open) {
        return false;
    }

    if (_role == Role::Listen) {
        _listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (_listen_fd < 0) {
            return false;
        }
        (void)setReuseAddr(_listen_fd);
        if (!setNonBlocking(_listen_fd)) {
            closeFds();
            return false;
        }

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(_port);
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (::bind(_listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
            std::fprintf(stderr, "[SocketCan] bind 127.0.0.1:%u failed: %s\n",
                static_cast<unsigned>(_port), std::strerror(errno));
            closeFds();
            return false;
        }
        if (::listen(_listen_fd, 1) != 0) {
            closeFds();
            return false;
        }
        std::printf("[SocketCan] listen 127.0.0.1:%u\n", static_cast<unsigned>(_port));
    } else {
        std::printf("[SocketCan] will connect 127.0.0.1:%u\n", static_cast<unsigned>(_port));
    }

    _open = true;
    _status = BIF::CAN::Status::OK;
    _partial_n = 0;
    _rx.clearData();
    return true;
}

void SocketCan::close()
{
    closeFds();
    _open = false;
    _partial_n = 0;
    _rx.clearData();
}

bool SocketCan::isOpen()
{
    return _open;
}

bool SocketCan::send(const BIF::CAN::Frame& frame)
{
    if (!_open) {
        return false;
    }
    if (_mute_tx) {
        return true;
    }
    if (!ensurePeer()) {
        return false;
    }

    uint8_t wire[kWireSize]{};
    if (!packWire(frame, wire)) {
        return false;
    }
    return writeAll(wire, kWireSize);
}

bool SocketCan::recv(BIF::CAN::Frame& out)
{
    poll();
    return _rx.pop(out);
}

std::size_t SocketCan::available() const
{
    const_cast<SocketCan*>(this)->poll();
    return _rx.size();
}

std::size_t SocketCan::availableForWrite() const
{
    if (!_open || _fd < 0) {
        return 0u;
    }
    return 8u;
}

void SocketCan::purge()
{
    _rx.clearData();
    _partial_n = 0;
}

void SocketCan::purgeOutput() {}

void SocketCan::flush() {}

BIF::CAN::Status SocketCan::getStatus()
{
    return _status;
}

void SocketCan::clearErrors()
{
    _status = BIF::CAN::Status::OK;
    _rx.clearOverflows();
}

void SocketCan::poll() noexcept
{
    if (!_open) {
        return;
    }
    (void)ensurePeer();
    if (_fd < 0) {
        return;
    }

    for (;;) {
        uint8_t chunk[64]{};
        const ssize_t n = ::recv(_fd, chunk, sizeof(chunk), 0);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            std::fprintf(stderr, "[SocketCan] recv error: %s\n", std::strerror(errno));
            ::close(_fd);
            _fd = -1;
            break;
        }
        if (n == 0) {
            std::printf("[SocketCan] peer closed\n");
            ::close(_fd);
            _fd = -1;
            break;
        }

        std::size_t off = 0;
        while (off < static_cast<std::size_t>(n)) {
            const std::size_t need = kWireSize - _partial_n;
            const std::size_t take = (static_cast<std::size_t>(n) - off) < need
                ? (static_cast<std::size_t>(n) - off)
                : need;
            std::memcpy(_partial + _partial_n, chunk + off, take);
            _partial_n += take;
            off += take;
            if (_partial_n == kWireSize) {
                BIF::CAN::Frame frame{};
                if (unpackWire(_partial, frame) && !_mute_rx) {
                    if (!_rx.push(frame)) {
                        _status = BIF::CAN::Status::OverFlowRX;
                    }
                }
                _partial_n = 0;
            }
        }
    }
}

bool SocketCan::ensurePeer() noexcept
{
    if (_fd >= 0) {
        return true;
    }

    if (_role == Role::Listen) {
        if (_listen_fd < 0) {
            return false;
        }
        sockaddr_in peer{};
        socklen_t len = sizeof(peer);
        const int fd = ::accept(_listen_fd, reinterpret_cast<sockaddr*>(&peer), &len);
        if (fd < 0) {
            return false;
        }
        if (!setNonBlocking(fd)) {
            ::close(fd);
            return false;
        }
        const int one = 1;
        (void)::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
        _fd = fd;
        std::printf("[SocketCan] peer accepted\n");
        return true;
    }

    if (_connecting_fd < 0) {
        const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) {
            return false;
        }
        if (!setNonBlocking(fd)) {
            ::close(fd);
            return false;
        }
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(_port);
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        const int rc = ::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
        if (rc == 0) {
            const int one = 1;
            (void)::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
            _fd = fd;
            std::printf("[SocketCan] connected\n");
            return true;
        }
        if (errno == EINPROGRESS || errno == EALREADY) {
            _connecting_fd = fd;
            return false;
        }
        ::close(fd);
        return false;
    }

    pollfd pfd{};
    pfd.fd = _connecting_fd;
    pfd.events = POLLOUT;
    const int pr = ::poll(&pfd, 1, 0);
    if (pr <= 0) {
        return false;
    }
    int so_err = 0;
    socklen_t sl = sizeof(so_err);
    if (::getsockopt(_connecting_fd, SOL_SOCKET, SO_ERROR, &so_err, &sl) != 0 || so_err != 0) {
        ::close(_connecting_fd);
        _connecting_fd = -1;
        return false;
    }
    const int one = 1;
    (void)::setsockopt(_connecting_fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    _fd = _connecting_fd;
    _connecting_fd = -1;
    std::printf("[SocketCan] connected\n");
    return true;
}

bool SocketCan::writeAll(const uint8_t* data, std::size_t n) noexcept
{
    std::size_t sent = 0;
    while (sent < n) {
        const ssize_t w = ::send(_fd, data + sent, n - sent, 0);
        if (w < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            std::fprintf(stderr, "[SocketCan] send error: %s\n", std::strerror(errno));
            ::close(_fd);
            _fd = -1;
            return false;
        }
        sent += static_cast<std::size_t>(w);
    }
    return true;
}

void SocketCan::closeFds() noexcept
{
    if (_fd >= 0) {
        ::close(_fd);
        _fd = -1;
    }
    if (_connecting_fd >= 0) {
        ::close(_connecting_fd);
        _connecting_fd = -1;
    }
    if (_listen_fd >= 0) {
        ::close(_listen_fd);
        _listen_fd = -1;
    }
}

bool SocketCan::packWire(const BIF::CAN::Frame& frame, uint8_t out[kWireSize]) noexcept
{
    const uint32_t packed = frame.id.pack();
    out[0] = static_cast<uint8_t>(packed);
    out[1] = static_cast<uint8_t>(packed >> 8);
    out[2] = static_cast<uint8_t>(packed >> 16);
    out[3] = static_cast<uint8_t>(packed >> 24);
    out[4] = frame.dlc > 8u ? 8u : frame.dlc;
    std::memcpy(out + 5, frame.data, 8);
    return true;
}

bool SocketCan::unpackWire(const uint8_t in[kWireSize], BIF::CAN::Frame& out) noexcept
{
    const uint32_t packed = static_cast<uint32_t>(in[0])
        | (static_cast<uint32_t>(in[1]) << 8)
        | (static_cast<uint32_t>(in[2]) << 16)
        | (static_cast<uint32_t>(in[3]) << 24);
    out.id = BIF::CAN::Frame::Id::unpack(packed);
    out.dlc = in[4] > 8u ? 8u : in[4];
    std::memcpy(out.data, in + 5, 8);
    return true;
}

} // namespace host
} // namespace smcp
