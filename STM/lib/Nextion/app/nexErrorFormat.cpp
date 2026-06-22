#include "nexErrors.hpp"

#include <cstdarg>
#include <cstdio>

namespace nex {

namespace {

bool isKnownCstr(const char* name) noexcept {
    return name != nullptr && !(name[0] == '?' && name[1] == '\0');
}

size_t appendRoute(char* buf, size_t cap, size_t off, uint8_t page_id, uint8_t comp_id) noexcept {
    if (page_id == 0u && comp_id == 0u)
        return off;
    if (off >= cap)
        return off;
    return static_cast<size_t>(std::snprintf(buf + off, cap - off, " page=%u comp=%u",
        static_cast<unsigned>(page_id), static_cast<unsigned>(comp_id))) + off;
}

size_t formatAppErrorBody(AppError reporter, uint16_t detail, char* buf, size_t cap) noexcept {
    if (cap == 0u)
        return 0u;

    const char* layer = cstr(reporter);
    const char* primary = appErrorDetailCstr(reporter, detail);
    const uint8_t sessionSt = static_cast<uint8_t>(detail & 0xFFu);
    const uint8_t gwSt = static_cast<uint8_t>(detail >> 8);

    if (reporter == AppError::Session && gwSt != 0u) {
        const char* gwName = cstr(static_cast<Gateway::Status>(gwSt));
        if (isKnownCstr(primary) && isKnownCstr(gwName))
            return static_cast<size_t>(std::snprintf(buf, cap, "%s %s, Gateway %s", layer, primary, gwName));
        if (isKnownCstr(primary))
            return static_cast<size_t>(std::snprintf(buf, cap, "%s %s, Gateway 0x%02X", layer, primary,
                static_cast<unsigned>(gwSt)));
        if (isKnownCstr(gwName))
            return static_cast<size_t>(std::snprintf(buf, cap, "%s 0x%02X, Gateway %s", layer,
                static_cast<unsigned>(sessionSt), gwName));
        return static_cast<size_t>(std::snprintf(buf, cap, "%s 0x%02X, Gateway 0x%02X", layer,
            static_cast<unsigned>(sessionSt), static_cast<unsigned>(gwSt)));
    }

    if (isKnownCstr(primary))
        return static_cast<size_t>(std::snprintf(buf, cap, "%s %s", layer, primary));
    return static_cast<size_t>(std::snprintf(buf, cap, "%s 0x%04X", layer, static_cast<unsigned>(detail)));
}

size_t formatNisStatusBody(const msg::Status& st, char* buf, size_t cap) noexcept {
    if (cap == 0u)
        return 0u;
    size_t off = static_cast<size_t>(std::snprintf(buf, cap, "%s (0x%02X)", cstr(st.status),
        static_cast<unsigned>(static_cast<uint8_t>(st.status))));
    if (st.tag_1 != 0u || st.tag_2 != 0u) {
        if (off < cap)
            off += static_cast<size_t>(std::snprintf(buf + off, cap - off, " tag_1=%u tag_2=%u",
                static_cast<unsigned>(st.tag_1), static_cast<unsigned>(st.tag_2)));
    }
    return off;
}

} // namespace

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

    char body[96];
    size_t bodyLen = 0u;
    if (st.isAppError()) {
        bodyLen = formatAppErrorBody(appErrorReporter(st), appErrorDetail(st), body, sizeof(body));
        bodyLen = static_cast<size_t>(std::snprintf(buf, cap, "AppError %.*s",
            static_cast<int>(bodyLen), body));
    } else {
        bodyLen = formatNisStatusBody(st, body, sizeof(body));
        bodyLen = static_cast<size_t>(std::snprintf(buf, cap, "%.*s", static_cast<int>(bodyLen), body));
    }
    return appendRoute(buf, cap, bodyLen, page_id, comp_id);
}

void printStatusError(const msg::Status& st, uint8_t page_id, uint8_t comp_id) noexcept {
#if defined(NEX_DEBUG)
    char body[96];
    if (st.isAppError()) {
        const size_t bodyLen = formatAppErrorBody(appErrorReporter(st), appErrorDetail(st), body, sizeof(body));
        const size_t off = appendRoute(body, sizeof(body), bodyLen, page_id, comp_id);
        detail::nexLogPrint("onError AppError %.*s\n", static_cast<int>(off), body);
    } else {
        const size_t bodyLen = formatNisStatusBody(st, body, sizeof(body));
        const size_t off = appendRoute(body, sizeof(body), bodyLen, page_id, comp_id);
        detail::nexLogPrint("onError %.*s\n", static_cast<int>(off), body);
    }
#else
    (void)st;
    (void)page_id;
    (void)comp_id;
#endif
}

} // namespace nex
