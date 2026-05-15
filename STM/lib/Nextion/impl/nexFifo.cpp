#include "nexFifo.hpp"

namespace nex {

// Явная инстанциация — проверка компиляции шаблона в CI/сборке (слот 128 байт под типичные `cmd::*`).
template class CommandFifo<4u, 128u>;

} // namespace nex
