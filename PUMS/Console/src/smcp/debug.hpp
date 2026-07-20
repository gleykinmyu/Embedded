/**
 * @file debug.hpp
 * @brief Отладочный вывод SMCP (`-DSMCP_DEBUG` в build_flags).
 */

#pragma once

#include <cstdio>

#if defined(SMCP_DEBUG)
#  define SMCP_DBG(...) std::printf(__VA_ARGS__)
#else
#  define SMCP_DBG(...) ((void)0)
#endif
