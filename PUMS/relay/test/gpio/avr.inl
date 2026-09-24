inline void Pin::Init(Mode mode) const noexcept
{
    volatile uint8_t* ddr = detail::kPort[_port].ddr;
    if (mode == Mode::Output)
        *ddr = static_cast<uint8_t>(*ddr | _mask);
    else
        *ddr = static_cast<uint8_t>(*ddr & static_cast<uint8_t>(~_mask));
}

inline void Pin::Set() const noexcept
{
    volatile uint8_t* port = detail::kPort[_port].port;
    if (_inverted)
        *port = static_cast<uint8_t>(*port & static_cast<uint8_t>(~_mask));
    else
        *port = static_cast<uint8_t>(*port | _mask);
}

inline void Pin::Clear() const noexcept
{
    volatile uint8_t* port = detail::kPort[_port].port;
    if (_inverted)
        *port = static_cast<uint8_t>(*port | _mask);
    else
        *port = static_cast<uint8_t>(*port & static_cast<uint8_t>(~_mask));
}

inline void Pin::Write(bool level) const noexcept
{
    if (level)
        Set();
    else
        Clear();
}

inline bool Pin::Read() const noexcept
{
    const bool phys = (*detail::kPort[_port].pin & _mask) != 0u;
    return _inverted ? !phys : phys;
}
