#pragma once

/**
 * Отладочный вывод библиотеки (`NEX_DEBUG`, `NEX_IDMAP_DEBUG`, `NEX_TRACE_TX`/`RX` — см. `build_flags`).
 */

#include <cstdio>

#if defined(NEX_DEBUG)
#  if !defined(NEX_IDMAP_DEBUG)
#    define NEX_IDMAP_DEBUG 1
#  endif
#  if !defined(NEX_TRACE_TX)
#    //define NEX_TRACE_TX 1
#  endif
#  if !defined(NEX_TRACE_RX)
#    //define NEX_TRACE_RX 1
#  endif
#  if !defined(NEX_NO_LOG_TICKS)
#    define NEX_LOG_TICKS 1
#  endif
#endif

#if defined(NEX_CID_DEBUG) && !defined(NEX_IDMAP_DEBUG)
#  define NEX_IDMAP_DEBUG 1
#endif

#if defined(NEX_DEBUG)
#  define NEX_DBG(...) std::printf(__VA_ARGS__)
#else
#  define NEX_DBG(...) ((void)0)
#endif

#if defined(NEX_IDMAP_DEBUG)
#  define NEX_DBG_IDMAP(...) std::printf(__VA_ARGS__)
#else
#  define NEX_DBG_IDMAP(...) ((void)0)
#endif

#if defined(NEX_TRACE_TX)
#  define NEX_DBG_TRACE_TX(...) std::printf(__VA_ARGS__)
#else
#  define NEX_DBG_TRACE_TX(...) ((void)0)
#endif

#if defined(NEX_TRACE_RX)
#  define NEX_DBG_TRACE_RX(...) std::printf(__VA_ARGS__)
#else
#  define NEX_DBG_TRACE_RX(...) ((void)0)
#endif

/** Проверка инварианта; активна при `NEX_DEBUG`, иначе no-op (как `assert` в release). */
#if defined(NEX_DEBUG)
#  define NEX_ASSERT(expr) \
    do { \
        if (!(expr)) { \
            NEX_DBG("NEX_ASSERT: %s (%s:%u)\n", #expr, __FILE__, static_cast<unsigned>(__LINE__)); \
            for (;;) { \
            } \
        } \
    } while (0)
#else
#  define NEX_ASSERT(expr) ((void)0)
#endif
