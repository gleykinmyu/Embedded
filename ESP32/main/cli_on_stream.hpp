#pragma once

/**
 * EmbeddedCLI (funbiscuit, v0.1.4) поверх BIF::IByteStream.
 * poll() отдаёт принятые байты в embeddedCliReceiveChar и вызывает embeddedCliProcess.
 * Ответы библиотека пишет по одному символу через write().
 */
#include "ibyte_stream.hpp"

#include "embedded_cli.h"

namespace Cli {

extern "C" inline void onStreamWriteChar(EmbeddedCli* cli, char c)
{
    auto* stream = static_cast<BIF::IByteStream*>(cli->appContext);
    const uint8_t byte = static_cast<uint8_t>(c);
    if (stream != nullptr)
        stream->write(&byte, 1u);
}

class OnStream {
public:
    static constexpr uint16_t kBufferBytes = 1024;

    explicit OnStream(BIF::IByteStream& stream)
        : _stream(stream)
    {
        EmbeddedCliConfig* config = embeddedCliDefaultConfig();
        config->cliBuffer = _buffer;
        config->cliBufferSize = kBufferBytes;
        _cli = embeddedCliNew(config);
        if (_cli == nullptr)
            return;
        _cli->appContext = &_stream;
        _cli->writeChar = &onStreamWriteChar;
    }

    [[nodiscard]] bool ok() const { return _cli != nullptr; }

    [[nodiscard]] EmbeddedCli* get() { return _cli; }

    bool add(CliCommandBinding binding)
    {
        return _cli != nullptr && embeddedCliAddBinding(_cli, binding);
    }

    void poll()
    {
        if (_cli == nullptr || !_stream.isOpen())
            return;
        uint8_t buf[32];
        const size_t n = _stream.read(buf, sizeof buf);
        for (size_t i = 0; i < n; ++i)
            embeddedCliReceiveChar(_cli, static_cast<char>(buf[i]));
        embeddedCliProcess(_cli);
    }

private:
    BIF::IByteStream& _stream;
    EmbeddedCli* _cli = nullptr;
    CLI_UINT _buffer[BYTES_TO_CLI_UINTS(kBufferBytes)]{};
};

} // namespace Cli
