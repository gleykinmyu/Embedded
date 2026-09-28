/**
 * @file debug.hpp
 * @brief Отладочный вывод SMCP — флаги по слоям.
 *
 * Master:
 *   `-DSMCP_DEBUG` — включает NODE + SESSION + CONSOLE
 *
 * Слои (можно включать по отдельности без SMCP_DEBUG):
 *   `-DSMCP_DBG_NODE`    — Node hooks: status, Ack/Nack, TxFull, SessionFull, …
 *   `-DSMCP_DBG_SESSION` — Session: setStatus, open reject, sendNack
 *   `-DSMCP_DBG_CONSOLE` — IConsole / MConsole (если включён слой)
 *
 * Wire dump:
 *   `-DSMCP_TRACE_LINK`  — TX/RX на ILink (без Heartbeat)
 *   `-DSMCP_TRACE_HB`    — + Heartbeat в TRACE_LINK
 *   `-DSMCP_TRACE_SHORT` — короткий формат (4.3" cnsl); выбор строки — `SMCP_LOG`
 *
 * Default dump (debug.cpp, weak) — конверт. Приложение может перекрыть
 * (PUMS: debug_trace.cpp, northbound PDU).
 */

#pragma once

#include <cstdio>
#include <cstdint>

#if defined(SMCP_DEBUG)
#  if !defined(SMCP_DBG_NODE)
#    define SMCP_DBG_NODE
#  endif
#  if !defined(SMCP_DBG_SESSION)
#    define SMCP_DBG_SESSION
#  endif
#  if !defined(SMCP_DBG_CONSOLE)
#    define SMCP_DBG_CONSOLE
#  endif
#endif

#if defined(SMCP_DBG_NODE)
#  define SMCP_NODE(...) std::printf(__VA_ARGS__)
#else
#  define SMCP_NODE(...) ((void)0)
#endif

#if defined(SMCP_DBG_SESSION)
#  define SMCP_SESS(...) std::printf(__VA_ARGS__)
#else
#  define SMCP_SESS(...) ((void)0)
#endif

#if defined(SMCP_DBG_CONSOLE)
#  define SMCP_CONS(...) std::printf(__VA_ARGS__)
#else
#  define SMCP_CONS(...) ((void)0)
#endif

/** Две строки формата, один набор аргументов. `SMCP_TRACE_SHORT` — первая, иначе вторая.
 *  Невыбранная строка в код не попадает. chan — `SMCP_NODE` / `SMCP_SESS` / `SMCP_CONS`.
 *  `SMCP_PICK` — то же для одного выражения, если аргумент тоже разный (`cstrS` / `cstr`).
 *
 *  SMCP_LOG(SMCP_SESS, "S%u\n", "[SMCP] Session[%u]\n", id);
 */
#if defined(SMCP_TRACE_SHORT)
#  define SMCP_LOG(chan, sfmt, lfmt, ...) chan(sfmt, ##__VA_ARGS__)
#  define SMCP_PICK(s, l) s
#else
#  define SMCP_LOG(chan, sfmt, lfmt, ...) chan(lfmt, ##__VA_ARGS__)
#  define SMCP_PICK(s, l) l
#endif

namespace smcp {
struct Packet;

#if defined(SMCP_TRACE_LINK)
/** @a dir — "TX" / "RX". */
void smcpTracePacket(const char* dir, uint8_t node_id, const Packet& pkt) noexcept;
#endif

} // namespace smcp

#if defined(SMCP_TRACE_LINK)
#  define SMCP_TRACE_PKT(dir, node, pkt) ::smcp::smcpTracePacket((dir), (node), (pkt))
#else
#  define SMCP_TRACE_PKT(dir, node, pkt) ((void)0)
#endif
