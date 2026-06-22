#pragma once

/**
 * Пример 4: автоматическая проверка путей ошибок и вывод в UART / MsgBox.
 *
 * HMI: page id=0, b0/b1/b2 на page0 (b0 id=1, b1 id=2, b2 id=3 — duplicate register на b1/b2 при необходимости).
 * Ошибки логируются через `onError` → `printStatusError`.
 *
 * b0 release — полный прогон (6 тестов, без QueueFull).
 * b1 release — только QueueFull + drain.
 *
 * Сборка: pio run -e example4
 */

#include <cstdint>
#include <cstdio>

#include "../../../Interfaces/iserial.hpp"
#include "nex.hpp"

namespace nex::examples {

using namespace nex::comp;

class ErrorTestApp : public Application {
public:
    static constexpr uint16_t kScreenWidth = 600;
    static constexpr uint16_t kScreenHeight = 1024;
    static constexpr uint8_t kPageId = 0u;
    static constexpr uint8_t kDupCompId = 2u;

    struct Stats {
        uint32_t register_err = 0u;
        uint32_t nis_panel = 0u;
        uint32_t app_session = 0u;
        uint32_t app_gateway = 0u;
        uint32_t app_stream = 0u;
        uint32_t app_command = 0u;
        uint32_t total = 0u;
    };

    Stats stats{};
    bool tests_running = false;
    bool tests_done = false;
    uint32_t tests_pass_mask = 0u;

    explicit ErrorTestApp(BIF::IHardwareSerial& stream, Application::ClockMsFn clockMs) noexcept
        : Application(stream, {kScreenWidth, kScreenHeight}, clockMs)
        , test_page(*this)
        , _link(stream)
    {}

    struct TestPage : PageImpl<3> {
        Button<> btn0;
        Button<> btn1;
        Button<> btn2;

        TestPage(ErrorTestApp& a) noexcept
            : PageImpl<3>(a, "page0", ErrorTestApp::kPageId)
            , btn0(*this, "b0", 1u)
            , btn1(*this, "b1", 2u)
            , btn2(*this, "b2", 3u)
        {}
    };

    TestPage test_page;

    void onError(const msg::Status& status, uint8_t page_id, uint8_t comp_id) noexcept override
    {
        Application::onError(status, page_id, comp_id);
        recordError(status);
    }

    void onTouch(const msg::evTouch& e) override
    {
        if (e.page_id == kPageId && e.state == TouchState::Release && !tests_running) {
            if (e.comp_id == 1u) {
                NEX_DBG("[ex4] full tests (b0 release)\n");
                beginTests();
            } else if (e.comp_id == 2u) {
                NEX_DBG("[ex4] queue-full test only (b1 release)\n");
                beginQueueFullTest();
            }
        }
        Application::onTouch(e);
    }

    void beginTests() noexcept
    {
        run_kind_ = RunKind::FullSuite;
        beginRunCommon();
        logTestSection("test 0: Register");
        NEX_DBG("[ex4] full error test run begin\n");
    }

    /** Отдельный прогон: переполнение Session (65× sendMe) + drain. */
    void beginQueueFullTest() noexcept
    {
        run_kind_ = RunKind::QueueFullOnly;
        beginRunCommon();
        phase_ = Phase::QueueFull;
        phase_started_ms_ = 0u;
        logTestSection("queue-only: Session QueueFull");
        NEX_DBG("[ex4] queue-full test begin\n");
    }

    void beginRunCommon() noexcept
    {
        const uint32_t boot_register = stats.register_err;
        tests_running = true;
        tests_done = false;
        tests_pass_mask = 0u;
        stats = {};
        stats.register_err = boot_register;
        phase_ = Phase::BootWait;
        phase_started_ms_ = 0u;
        phase_once_ = false;
        sanity_idx_ = 0u;
        queue_enqueued_ = 0u;
    }

    /** @return true — прогон ещё идёт. */
    bool tickTests(uint32_t now_ms) noexcept
    {
        if (!tests_running)
            return false;

        if (phase_started_ms_ == 0u)
            phase_started_ms_ = now_ms;

        const uint32_t elapsed = now_ms - phase_started_ms_;
        update();

        switch (phase_) {
        case Phase::BootWait:
            if (elapsed >= 1000u) {
                markTest(0u, stats.register_err > 0u);
                NEX_DBG("[ex4] boot Register errors=%u (expect duplicate id %u)\n",
                    static_cast<unsigned>(stats.register_err), static_cast<unsigned>(kDupCompId));
                enterPhase(Phase::SanityAppError, now_ms);
            }
            break;

        case Phase::SanityAppError:
            if (elapsed >= 40u) {
                fireSanityError(sanity_idx_);
                ++sanity_idx_;
                enterPhase(Phase::SanityAppError, now_ms);
                if (sanity_idx_ >= kSanityCount) {
                    markTest(1u, true);
                    enterPhase(Phase::NisInvalidVar, now_ms);
                }
            }
            break;

        case Phase::NisInvalidVar:
            if (!phase_once_) {
                phase_once_ = true;
                enqueue(Transaction{cmd::Get::numeric(kBadAttr), kPageId, 4u, 0u, Transaction::Kind::GetNumeric});
            }
            if (elapsed >= 800u) {
                markTest(2u, stats.nis_panel > 0u);
                NEX_DBG("[ex4] NIS invalid attr errors=%u\n", static_cast<unsigned>(stats.nis_panel));
                enterPhase(Phase::NisInvalidComp, now_ms);
            }
            break;

        case Phase::NisInvalidComp:
            if (!phase_once_) {
                phase_once_ = true;
                enqueue(Transaction{cmd::Get::numeric(kGhostComp), kPageId, 1u, 0u, Transaction::Kind::GetNumeric});
            }
            if (elapsed >= 800u) {
                markTest(3u, stats.nis_panel >= 2u);
                NEX_DBG("[ex4] NIS invalid comp total=%u\n", static_cast<unsigned>(stats.nis_panel));
                enterPhase(Phase::TimeoutPrep, now_ms);
            }
            break;

        case Phase::QueueFull:
            if (!phase_once_) {
                phase_once_ = true;
                fillQueueUntilError();
            }
            if (elapsed >= 100u) {
                const bool ok = stats.app_session > metric_at_phase_;
                if (run_kind_ == RunKind::QueueFullOnly)
                    markTest(0u, ok);
                else
                    markTest(4u, ok);
                enterPhase(Phase::QueueDrain, now_ms);
            }
            break;

        case Phase::QueueDrain:
            if (!phase_once_) {
                phase_once_ = true;
                _link.purge();
                clearErrors();
                NEX_DBG("[ex4] queue drain start (RX purge)\n");
            }
            if (elapsed >= 12000u) {
                NEX_DBG("[ex4] queue drain done (timeout)\n");
                clearErrors();
                if (run_kind_ == RunKind::QueueFullOnly)
                    enterPhase(Phase::QueueSummary, now_ms);
                else
                    enterPhase(Phase::TimeoutPrep, now_ms);
            }
            break;

        case Phase::TimeoutPrep:
            if (!phase_once_) {
                phase_once_ = true;
                enqueue(Transaction{cmd::Get::numeric(kValidAttr), kPageId, 1u, 0u, Transaction::Kind::GetNumeric});
            }
            if (elapsed >= 120u) {
                metric_at_phase_ = stats.app_session;
                _link.close();
                NEX_DBG("[ex4] UART closed for timeout test\n");
                enterPhase(Phase::TimeoutWait, now_ms);
            }
            break;

        case Phase::TimeoutWait:
            if (elapsed >= 900u) {
                markTest(4u, stats.app_session > metric_at_phase_);
                NEX_DBG("[ex4] session errors=%u (delta in timeout phase)\n",
                    static_cast<unsigned>(stats.app_session));
                _link.open(250000u);
                clearErrors();
                enterPhase(Phase::LinkFault, now_ms);
            }
            break;

        case Phase::LinkFault:
            if (!phase_once_) {
                phase_once_ = true;
                metric_at_phase_ = stats.app_gateway + stats.app_stream;
                _link.close();
                enqueue(Transaction{cmd::Page::sendMe(), 0u, 0u, 0u, Transaction::Kind::Command, msg::kAwaitingPageCommand});
            }
            if (elapsed >= 200u) {
                markTest(5u, (stats.app_gateway + stats.app_stream) > metric_at_phase_);
                NEX_DBG("[ex4] gateway=%u stream=%u\n", static_cast<unsigned>(stats.app_gateway),
                    static_cast<unsigned>(stats.app_stream));
                _link.open(250000u);
                clearErrors();
                enterPhase(Phase::Summary, now_ms);
            }
            break;

        case Phase::QueueSummary:
            if (!phase_once_) {
                phase_once_ = true;
                printQueueSummary();
            }
            if (elapsed >= 2000u) {
                tests_running = false;
                tests_done = true;
                phase_ = Phase::Done;
                NEX_DBG("[ex4] === queue test done (mask=0x%02lX) ===\n",
                    static_cast<unsigned long>(tests_pass_mask));
                logTestSection("queue test done");
            }
            break;

        case Phase::Summary:
            if (!phase_once_) {
                phase_once_ = true;
                printSummary();
            }
            if (elapsed >= 3000u) {
                tests_running = false;
                tests_done = true;
                phase_ = Phase::Done;
                NEX_DBG("[ex4] === done (mask=0x%02lX) ===\n", static_cast<unsigned long>(tests_pass_mask));
                logTestSection("done");
            }
            break;

        case Phase::Done:
            tests_running = false;
            break;
        }

        return tests_running;
    }

private:
    enum class RunKind : uint8_t { FullSuite, QueueFullOnly };

    enum class Phase : uint8_t {
        BootWait,
        SanityAppError,
        NisInvalidVar,
        NisInvalidComp,
        QueueFull,
        QueueDrain,
        QueueSummary,
        TimeoutPrep,
        TimeoutWait,
        LinkFault,
        Summary,
        Done,
    };

    static constexpr unsigned kSanityCount = 5u;
    static constexpr unsigned kFullTestCount = 6u;
    static constexpr unsigned kQueueOnlyTestCount = 1u;
    static constexpr Literal kCompB0{"b0"};
    static constexpr Literal kAttrTxt{"txt"};
    static constexpr Literal kAttrBad{"nope_attr"};
    static constexpr Literal kCompGhost{"ghost99"};
    static constexpr AttrRef kValidAttr{kCompB0, kAttrTxt};
    static constexpr AttrRef kBadAttr{kCompB0, kAttrBad};
    static constexpr AttrRef kGhostComp{kCompGhost, kAttrTxt};

    Phase phase_ = Phase::Done;
    RunKind run_kind_ = RunKind::FullSuite;
    uint32_t phase_started_ms_ = 0u;
    uint32_t metric_at_phase_ = 0u;
    bool phase_once_ = false;
    uint8_t sanity_idx_ = 0u;
    unsigned queue_enqueued_ = 0u;
    BIF::IHardwareSerial& _link;

    void enterPhase(Phase p, uint32_t now_ms) noexcept
    {
        phase_ = p;
        phase_started_ms_ = now_ms;
        phase_once_ = false;
        if (const char* const title = phaseTitle(p))
            logTestSection(title);
    }

    static void logTestSection(const char* title) noexcept
    {
        NEX_DBG("\n===========================\n[ex4] %s\n===========================\n", title);
    }

    static const char* phaseTitle(Phase p) noexcept
    {
        switch (p) {
        case Phase::SanityAppError: return "test 1: AppError sanity";
        case Phase::NisInvalidVar: return "test 2: NIS invalid attr";
        case Phase::NisInvalidComp: return "test 3: NIS invalid comp";
        case Phase::TimeoutPrep: return "test 4: Session Timeout";
        case Phase::LinkFault: return "test 5: Gateway / Stream";
        case Phase::Summary: return "summary";
        default: return nullptr;
        }
    }

    void markTest(unsigned bit, bool ok) noexcept
    {
        if (ok)
            tests_pass_mask |= (1u << bit);
        NEX_DBG("[ex4] test[%u] %s\n", bit, ok ? "PASS" : "FAIL");
    }

    void recordError(const msg::Status& status) noexcept
    {
        ++stats.total;
        if (status.isAppError()) {
            switch (appErrorReporter(status)) {
            case AppError::Register: ++stats.register_err; break;
            case AppError::Command: ++stats.app_command; break;
            case AppError::Session: ++stats.app_session; break;
            case AppError::Gateway: ++stats.app_gateway; break;
            case AppError::Stream: ++stats.app_stream; break;
            default: break;
            }
            return;
        }
        if (status.status != msg::Status::Code::Success)
            ++stats.nis_panel;
    }

    void logErrorLine(const msg::Status& status, uint8_t page_id, uint8_t comp_id) noexcept
    {
        char buf[80]{};
        formatStatusMessage(status, page_id, comp_id, buf, sizeof(buf));
        NEX_DBG("[ex4] err: %s\n", buf);
    }

    void fireSanityError(unsigned idx) noexcept
    {
        switch (idx) {
        case 0u: onError(appErrorFrom(MISC::RegStatus::SlotOccupied), kPageId, kDupCompId); break;
        case 1u: onError(appErrorFrom(Command::Status::EmptyLiteral), 0u, 0u); break;
        case 2u: onError(appErrorFrom(Session::Status::QueueFull), 0u, 0u); break;
        case 3u: onError(appErrorFrom(Gateway::Status::RxOverflow), 0u, 0u); break;
        case 4u: onError(appErrorFrom(BIF::IByteStream::Status::FrameError), 0u, 0u); break;
        default: break;
        }
    }

    static unsigned popcount(uint32_t v) noexcept
    {
        unsigned n = 0u;
        while (v != 0u) {
            n += static_cast<unsigned>(v & 1u);
            v >>= 1u;
        }
        return n;
    }

    void fillQueueUntilError() noexcept
    {
        metric_at_phase_ = stats.app_session;
        queue_enqueued_ = 0u;
        NEX_DBG("[ex4] queue fill: NEX_TRACE_TX → каждый pushCommand (sendme + 0xFF×3)\n");
        for (unsigned i = 0u; i < 65; ++i) {
            const uint32_t before = stats.total;
            //NEX_DBG("[ex4] enqueue try #%u\n", i + 1u);
            enqueue(Transaction{cmd::Page::sendMe(), 0u, 0u, 0u, Transaction::Kind::Command,
                msg::kAwaitingPageCommand});
            ++queue_enqueued_;
            if (stats.total > before)
                break;
        }
        NEX_DBG("[ex4] queue fill tries=%u session_err=%u\n", static_cast<unsigned>(queue_enqueued_),
            static_cast<unsigned>(stats.app_session - metric_at_phase_));
    }

    void printQueueSummary() noexcept
    {
        const bool pass = (tests_pass_mask & 1u) != 0u;
        NEX_DBG("[ex4] queue summary: %s, fill tries=%u, session_err=%u\n", pass ? "PASS" : "FAIL",
            static_cast<unsigned>(queue_enqueued_),
            static_cast<unsigned>(stats.app_session - metric_at_phase_));

        char body[96]{};
        std::snprintf(body, sizeof(body), "queue %s tries %u", pass ? "OK" : "FAIL",
            static_cast<unsigned>(queue_enqueued_));
        msgBox.show("ex4 queue", body, MsgBox::Preset::OK);
    }

    void printSummary() noexcept
    {
        const unsigned passed = popcount(tests_pass_mask);
        NEX_DBG("[ex4] summary: %u/%u passed, total errors=%u\n", passed, kFullTestCount,
            static_cast<unsigned>(stats.total));
        NEX_DBG("[ex4]   Register=%u NIS=%u Session=%u Gateway=%u Stream=%u Command=%u\n",
            static_cast<unsigned>(stats.register_err), static_cast<unsigned>(stats.nis_panel),
            static_cast<unsigned>(stats.app_session), static_cast<unsigned>(stats.app_gateway),
            static_cast<unsigned>(stats.app_stream), static_cast<unsigned>(stats.app_command));

        char body[96]{};
        std::snprintf(body, sizeof(body), "pass %u/%u err %lu", passed, kFullTestCount,
            static_cast<unsigned long>(stats.total));
        msgBox.show("ex4 tests", body, MsgBox::Preset::OK);
    }
};

} // namespace nex::examples
