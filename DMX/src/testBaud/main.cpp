/**
 * testBaud: PP — rest @250000 → baud=512000 (921600 на APB1 ~1% — не держит).
 * Сборка: pio run -e testBaud -t upload -t monitor
 */

#include <cstdio>

#include "Debug.h"
#include "board.hpp"
#include "nex.hpp"
#include "phl/watchdog.hpp"

namespace {

constexpr uint32_t kWatchdogTimeoutMs = 6000u;
constexpr nex::Baudrate kNow = nex::Baudrate::b250000;
constexpr nex::Baudrate kBoot = nex::Baudrate::b250000;
constexpr nex::Baudrate kFast = nex::Baudrate::b512000;

char g_stdoutBuf[256];

class BaudProbe : public nex::Application {
public:
    explicit BaudProbe(BIF::IByteStream& link, nex::AppTiming timing) noexcept
        : Application(link, nex::Rect(480, 272), timing)
        , _link(link)
    {
    }

    void onStatus(const nex::msg::Status& status, nex::Route route = {}) noexcept override
    {
        Application::onStatus(status, route);
        if (status.isAppError())
            return;
        _gotStatus = true;
        _status = status;
    }

    void pumpMs(const uint32_t ms) noexcept
    {
        const uint32_t t0 = boardClockMs();
        while ((boardClockMs() - t0) < ms) {
            board.watchdog.kick();
            update();
        }
    }

    bool waitStatus(const uint32_t ms) noexcept
    {
        const uint32_t t0 = boardClockMs();
        while ((boardClockMs() - t0) < ms) {
            board.watchdog.kick();
            update();
            if (_gotStatus)
                return true;
        }
        return false;
    }

    void showBanner(const char* text) noexcept
    {
        constexpr nex::Rect box{300, 48};
        constexpr nex::Rect screen{480, 272};
        const nex::Region r{nex::Canvas::center(screen, box), box};
        cs.text_in_region(r, text, 0u, nex::Color::std::White, nex::HAlign::Center,
            nex::VAlign::Center, nex::Color::std::Black, nex::BG::Color);
        (void)pumpUntilIdle();
    }

    bool switchUart(const nex::Baudrate rate) noexcept
    {
        _link.flush();
        if (!_link.open(rate)) {
            std::printf("USART2 open(%u) FAIL\n", static_cast<unsigned>(rate));
            return false;
        }
        _link.purge();
        _link.clearErrors();
        clearErrors();
        std::printf("USART2 now %u PP\n", static_cast<unsigned>(rate));
        return true;
    }

    BIF::IByteStream& _link;
    bool _gotStatus = false;
    nex::msg::Status _status{};
};

} // namespace

int main()
{
    board.serial1.InitPins(GPIO::PortA::pin<9>, GPIO::PortA::pin<10>);
    board.serial2.InitPins(GPIO::PortA::pin<2>, GPIO::PortA::pin<3>, GPIO::ModeAlt::PP);

    board.serial1.open(250000);
    setSerial1LogEnabled(true);
    std::setvbuf(stdout, g_stdoutBuf, _IOLBF, sizeof(g_stdoutBuf));
    board.serial2.open(kNow);
    board.setLedAlive(true);

    (void)board.watchdog.begin(kWatchdogTimeoutMs);
    board.watchdog.kick();

    nex::AppTiming timing{boardClockMs, 500u};
    BaudProbe app(board.serial2, timing);

    app.restartScreen();
    (void)app.pumpUntilIdle();
    app.pumpMs(1500u);
    if (!app.switchUart(kBoot))
        return 0;

    app.showBanner("testBaud 250000 PP");
    app.requestCurrentPage();
    (void)app.pumpUntilIdle();

    app._gotStatus = false;
    app.bkcmd = nex::BkCmd::Always;
    (void)app.pumpUntilIdle();
    if (!app.waitStatus(300u))
        std::printf("bkcmd=3 -> no status\n");

    app._gotStatus = false;
    app.setBaudrate(kFast);
    const uint32_t tBaud = boardClockMs();
    while ((boardClockMs() - tBaud) < 300u) {
        board.watchdog.kick();
        app.update();
        if (app._gotStatus)
            break;
    }
    if (app._gotStatus)
        std::printf("baud=%u -> %s (0x%02X)\n", static_cast<unsigned>(kFast),
            nex::cstr(app._status.status),
            static_cast<unsigned>(static_cast<uint8_t>(app._status.status)));
    else
        std::printf("baud=%u -> no status\n", static_cast<unsigned>(kFast));

    if (app._gotStatus && app._status.status == nex::msg::Status::Code::Success
        && app.switchUart(kFast)) {
        app.pumpMs(20u);
        app.showBanner("testBaud 512000 PP");
        app.requestCurrentPage();
        (void)app.pumpUntilIdle();
    }

    for (;;) {
        board.tick();
        app.update();
    }
}
