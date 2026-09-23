#pragma once

/**
 * ATmega2560 (Arduino Mega 2560): только заголовки avr-libc / Atmel.
 * Arduino.h и framework Arduino не подключаются.
 */
#include <avr/io.h>
#include <stdint.h>

#if !defined(__AVR_ATmega2560__)
#error "board/mega2560: целевой MCU — ATmega2560"
#endif

#ifndef F_CPU
#error "board/mega2560: задайте F_CPU (Arduino Mega 2560 — 16000000)"
#endif
