inline void Pin::Init(Mode mode) const noexcept
{
    (void)mode;
}

inline void Pin::Set() const noexcept
{
    volatile uint32_t* out = detail::out[_port];
    if (_inverted)
        *out &= ~_mask;
    else
        *out |= _mask;
}

inline void Pin::Clear() const noexcept
{
    volatile uint32_t* out = detail::out[_port];
    if (_inverted)
        *out |= _mask;
    else
        *out &= ~_mask;
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
    const bool phys = (*detail::in[_port] & _mask) != 0u;
    return _inverted ? !phys : phys;
}
