/**
 * @file debug.hpp
 * @brief Отладочный вывод SMCP — флаги по слоям.
 *
 * Master:
 *   `-DSMCP_DEBUG` — включает NODE + SESSION + CONSOLE
 *                    (не SHOW — слишком шумно).
 *
 * Слои (можно включать по отдельности без SMCP_DEBUG):
 *   `-DSMCP_DBG_NODE`    — Node hooks: status, Ack/Nack, TxFull, SessionFull, …
 *   `-DSMCP_DBG_SESSION` — Session: setStatus, open reject, sendNack
 *   `-DSMCP_DBG_CONSOLE` — IConsole: setPhase
 *   `-DSMCP_DBG_SHOW`    — ShowFile Reader/Writer
 *
 * Wire dump:
 *   `-DSMCP_TRACE_LINK`  — TX/RX на ILink с разбором Message (без Heartbeat)
 *   `-DSMCP_TRACE_HB`    — + Heartbeat в TRACE_LINK
 *   `-DSMCP_TRACE_SHORT` — короткий формат (4.3" cnsl)
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

#if defined(SMCP_DBG_SHOW)
#  define SMCP_SHOW(...) std::printf(__VA_ARGS__)
#else
#  define SMCP_SHOW(...) ((void)0)
#endif

namespace smcp {
namespace msg {
struct Packet;
} // namespace msg

#if defined(SMCP_TRACE_LINK)
/** @a dir — "TX" / "RX". */
void smcpTracePacket(const char* dir, uint8_t node_id, const msg::Packet& pkt) noexcept;
#endif

} // namespace smcp

#if defined(SMCP_TRACE_LINK)
#  define SMCP_TRACE_PKT(dir, node, pkt) ::smcp::smcpTracePacket((dir), (node), (pkt))
#else
#  define SMCP_TRACE_PKT(dir, node, pkt) ((void)0)
#endif
