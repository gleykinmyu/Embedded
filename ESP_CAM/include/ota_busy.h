#pragma once

/** true while espota принимает/пишет прошивку — health/UART poll должен молчать. */
bool otaIsBusy();
