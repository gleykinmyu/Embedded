#pragma once

#include "ibyte_stream.hpp"

namespace Usart {

/// stdout/stderr (printf) пишутся в этот поток. nullptr — прежний обработчик консоли.
void bindStdio(BIF::IByteStream* stream);

} // namespace Usart
