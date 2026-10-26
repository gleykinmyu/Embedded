#pragma once

/** Размер очереди `Session` (`NEX_SESSION_QUEUE_CAPACITY`); переопределить в `build_flags` проекта. */
#ifndef NEX_SESSION_QUEUE_CAPACITY
#define NEX_SESSION_QUEUE_CAPACITY 64u
#endif

/**
 * NIS `covx` / `substr` / `strlen` / `btlen` / `spstr` → `cmd::String`.
 * 0 (дефолт): класс выключен, слот очереди 32 B (пик = `cmd::Move`).
 * 1: включить (`-DNEX_CMD_STRING=1`), слот 40 B.
 */
#ifndef NEX_CMD_STRING
#define NEX_CMD_STRING 0
#endif

