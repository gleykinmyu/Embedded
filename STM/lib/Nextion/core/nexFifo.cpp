#include "nexFifo.hpp"

namespace nex {

// Явная инстанциация — проверка компиляции шаблонов в CI/сборке библиотеки.
template class CommandFifo<4u, 128u>;
template class MessageFifo<8u>;

} // namespace nex
