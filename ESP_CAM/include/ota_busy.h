#pragma once

/** true while espota принимает/пишет прошивку — health/UART poll должен молчать. */
bool otaIsBusy();

/** Ph350Ctrl: захват шины на время вызова (войти и сразу выйти). */
void otaBindQuiesce(void (*fn)(void *ctx), void *ctx);

/**
 * Ждёт, пока опрос здоровья отпустит шину.
 * Вызывать только после того, как otaIsBusy() уже true.
 */
void otaWaitHealthQuiet();
