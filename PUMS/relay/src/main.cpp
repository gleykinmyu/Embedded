#include "board.hpp"

CBoard board;

namespace {

void log(const char* text)
{
    while (*text != '\0') {
        const uint8_t byte = static_cast<uint8_t>(*text++);
        while (board.serial0.write(&byte, 1) != 1) {
        }
    }
}

void logHex8(uint8_t value)
{
    static const char kHex[] = "0123456789ABCDEF";
    const char text[] = {
        '0', 'x',
        kHex[value >> 4],
        kHex[value & 0x0Fu],
        '\0',
    };
    log(text);
}

/** 24 реле: PF0..PF7, PK0..PK7, PC0..PC7. Ноль включает реле. */
constexpr uint8_t kRelayCount = 24;
constexpr uint32_t kRelayStepMs = 1000;

void relaysInit()
{
    PORTF = 0xFFu;
    PORTK = 0xFFu;
    PORTC = 0xFFu;
    DDRF = 0xFFu;
    DDRK = 0xFFu;
    DDRC = 0xFFu;
}

void relaysApply(uint8_t index)
{
    uint8_t f = 0xFFu;
    uint8_t k = 0xFFu;
    uint8_t c = 0xFFu;
    if (index < 8u)
        f = static_cast<uint8_t>(~static_cast<uint8_t>(1u << index));
    else if (index < 16u)
        k = static_cast<uint8_t>(~static_cast<uint8_t>(1u << (index - 8u)));
    else
        c = static_cast<uint8_t>(~static_cast<uint8_t>(1u << (index - 16u)));

    PORTF = f;
    PORTK = k;
    PORTC = c;
}

} // namespace

int main()
{
    relaysInit();

    board.serial0.InitPins();
    if (!board.serial0.open(115200)) {
        for (;;) {
            board.led.Toggle();
            board.Delay(100);
        }
    }

    log("relay ATmega2560 reset ");
    logHex8(PHL::ResetFlags::raw());
    log("\r\n");

    uint8_t step = 0;
    uint32_t stepMs = board.GetTick();
    relaysApply(step);

    for (;;) {
        if (board.tick())
            log("tick\r\n");

        const uint32_t now = board.GetTick();
        if ((now - stepMs) < kRelayStepMs)
            continue;
        stepMs += kRelayStepMs;
        step = static_cast<uint8_t>((step + 1u) % kRelayCount);
        relaysApply(step);
    }
}
