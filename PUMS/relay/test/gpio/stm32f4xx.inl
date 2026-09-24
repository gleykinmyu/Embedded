namespace detail {

inline uint8_t bit_of(Reg mask) noexcept
{
    uint8_t bit = 0;
    while ((mask & 1u) == 0u) {
        mask >>= 1;
        ++bit;
    }
    return bit;
}

} // namespace detail

inline void Pin::Init(Mode mode) const noexcept
{
    GPIO_TypeDef* block = detail::kPort[_port];
    const uint8_t shift = static_cast<uint8_t>(detail::bit_of(_mask) * 2u);
    uint32_t moder = block->MODER & ~(3u << shift);
    if (mode == Mode::Output)
        moder |= 1u << shift;
    block->MODER = moder;
}

inline void Pin::Set() const noexcept
{
    detail::kPort[_port]->BSRR = _inverted ? (_mask << 16) : _mask;
}

inline void Pin::Clear() const noexcept
{
    detail::kPort[_port]->BSRR = _inverted ? _mask : (_mask << 16);
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
    const bool phys = (detail::kPort[_port]->IDR & _mask) != 0u;
    return _inverted ? !phys : phys;
}
