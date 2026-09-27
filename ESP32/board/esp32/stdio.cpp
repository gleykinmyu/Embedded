#include "stdio.hpp"

#include <reent.h>
#include <unistd.h>

namespace {
BIF::IByteStream* g_stdio = nullptr;
}

namespace Usart {

void bindStdio(BIF::IByteStream* stream)
{
    g_stdio = stream;
}

} // namespace Usart

extern "C" ssize_t __real__write_r(struct _reent* r, int fd, const void* data, size_t size);

/// printf идёт в _write_r. VFS задаёт его сам, поэтому символ подменяется --wrap.
extern "C" ssize_t __wrap__write_r(struct _reent* r, int fd, const void* data, size_t size)
{
    BIF::IByteStream* out = g_stdio;
    if (out != nullptr && out->isOpen() && data != nullptr && size > 0u &&
        (fd == STDOUT_FILENO || fd == STDERR_FILENO)) {
        const auto* bytes = static_cast<const uint8_t*>(data);
        size_t left = size;
        while (left > 0u) {
            const size_t n = out->write(bytes, left);
            if (n == 0u)
                break;
            bytes += n;
            left -= n;
        }
        return static_cast<ssize_t>(size - left);
    }
    return __real__write_r(r, fd, data, size);
}
