#pragma once

namespace BIF {

/**
 * Объект с критической секцией через lock()/unlock() (обычно PRIMASK на плате).
 * RAII: LockGuard.
 */
class ILockable
{
public:
    virtual ~ILockable() = default;

protected:
    virtual void lock() = 0;
    virtual void unlock() = 0;

    friend class LockGuard;
};

class LockGuard
{
    ILockable& _obj;

public:
    explicit LockGuard(ILockable& obj) noexcept : _obj(obj) { _obj.lock(); }
    ~LockGuard() { _obj.unlock(); }

    LockGuard(const LockGuard&) = delete;
    LockGuard& operator=(const LockGuard&) = delete;
};

} // namespace BIF
