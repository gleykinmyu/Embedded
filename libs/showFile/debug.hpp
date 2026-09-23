/**
 * @file debug.hpp
 * @brief Отладка showFile: load/save и адаптеры носителя.
 *
 *   `-DSF_DEBUG` — printf load/save / W25Q mirror
 */

#pragma once

#include <cstdio>

#if defined(SF_DEBUG)
#  define SF_DBG(...) std::printf(__VA_ARGS__)
#else
#  define SF_DBG(...) ((void)0)
#endif
