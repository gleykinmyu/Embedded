#include "nexErrors.hpp"

#include <cstdarg>
#include <cstdio>

namespace nex {
namespace detail {

namespace {

NexLogTickFn g_nexLogTickFn = nullptr;

#if defined(NEX_LOG_TICKS) && defined(NEX_DEBUG)
/** Ширина поля тиков в `[Nex %010u]` — префикс всегда 16 символов. */
constexpr unsigned kNexLogTickWidth = 10u;

void nexLogPrefix() noexcept {
    const unsigned long tick = g_nexLogTickFn != nullptr ? static_cast<unsigned long>(g_nexLogTickFn()) : 0ul;
    std::printf("[Nex %0*lu] ", static_cast<int>(kNexLogTickWidth), tick);
}
#endif

} // namespace

void setNexLogTickFn(NexLogTickFn fn) noexcept {
    g_nexLogTickFn = fn;
}

#if defined(NEX_DEBUG)
void nexLogPrint(const char* fmt, ...) noexcept {
    va_list args;
    va_start(args, fmt);
#if defined(NEX_LOG_TICKS)
    nexLogPrefix();
#else
    std::printf("[Nex] ");
#endif
    std::vprintf(fmt, args);
    va_end(args);
}
#endif

} // namespace detail

size_t formatStatusMessage(const msg::Status& st, uint8_t page_id, uint8_t comp_id, char* buf, size_t cap) noexcept {
    if (cap == 0u)
        return 0u;
    if (st.isAppError()) {
        const AppError reporter = appErrorReporter(st);
        const uint16_t detail = appErrorDetail(st);
        if ((detail >> 8) != 0u && reporter == AppError::Session)
            return static_cast<size_t>(std::snprintf(buf, cap, "AppError %s %s gw=%s p%u c%u", cstr(reporter),
                appErrorDetailCstr(reporter, detail), cstr(static_cast<Gateway::Status>(detail >> 8)),
                static_cast<unsigned>(page_id), static_cast<unsigned>(comp_id)));
        return static_cast<size_t>(std::snprintf(buf, cap, "AppError %s %s p%u c%u", cstr(reporter),
            appErrorDetailCstr(reporter, detail), static_cast<unsigned>(page_id), static_cast<unsigned>(comp_id)));
    }
    return static_cast<size_t>(std::snprintf(buf, cap, "%s t1=%u t2=%u p%u c%u", cstr(st.status),
        static_cast<unsigned>(st.tag_1), static_cast<unsigned>(st.tag_2), static_cast<unsigned>(page_id),
        static_cast<unsigned>(comp_id)));
}

void printStatusError(const msg::Status& st, uint8_t page_id, uint8_t comp_id) noexcept {
#if defined(NEX_DEBUG)
    if (st.isAppError()) {
        const AppErrorReporter reporter = appErrorReporter(st);
        const uint16_t detail = appErrorDetail(st);
        if ((detail >> 8) != 0u && reporter == AppErrorReporter::Session) {
            detail::nexLogPrint("onError AppError %s %s gw=%s page=%u comp=%u\n", cstr(reporter),
                appErrorDetailCstr(reporter, detail), cstr(static_cast<Gateway::Status>(detail >> 8)),
                static_cast<unsigned>(page_id), static_cast<unsigned>(comp_id));
        } else {
            detail::nexLogPrint("onError AppError %s %s page=%u comp=%u\n", cstr(reporter),
                appErrorDetailCstr(reporter, detail), static_cast<unsigned>(page_id),
                static_cast<unsigned>(comp_id));
        }
    } else {
        detail::nexLogPrint("onError %s (0x%02X) tag_1=%u tag_2=%u page=%u comp=%u\n", cstr(st.status),
            static_cast<unsigned>(static_cast<uint8_t>(st.status)), static_cast<unsigned>(st.tag_1),
            static_cast<unsigned>(st.tag_2), static_cast<unsigned>(page_id), static_cast<unsigned>(comp_id));
    }
#else
    (void)st;
    (void)page_id;
    (void)comp_id;
#endif
}

} // namespace nex
