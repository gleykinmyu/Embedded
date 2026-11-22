/**
 * @file comm/iservice.hpp
 * @brief comm::IService / ServiceManager — tick по списку служб.
 *
 * IService в конструкторе получает ServiceManager* и сам делает attach.
 * Ошибка attach — флаг у ServiceManager и указатель на службу, которую
 * не удалось зарегистрировать. tick() берёт время из ClockFn и обходит список.
 */
#pragma once

#include "mem/list.hpp"
#include "ms_timer.hpp"

namespace comm {

class ServiceManager;

class IService : private mem::ListNode {
    friend class mem::List<IService>;
    friend class ServiceManager;

public:
    virtual ~IService() = default;

    virtual void tick(MISC::time_t now) noexcept = 0;
    virtual const char* name() const noexcept { return "default Service"; }

    virtual void start() noexcept { _running = true; }
    virtual void stop() noexcept { _running = false; }
    [[nodiscard]] bool isRunning() const noexcept { return _running; }

protected:
    explicit IService(ServiceManager* mgr) noexcept;
    IService(const IService&) = delete;
    IService& operator=(const IService&) = delete;

private:
    bool _running = false;
};

class ServiceManager {
public:
    using ClockFn = MISC::time_t (*)();

    explicit ServiceManager(ClockFn now) noexcept
        : _now(now)
    {}

    void attach(IService& s) noexcept
    {
        if (_list.pushBack(s)) {
            return;
        }

        if (_failed == nullptr) {
            _failed = &s;
        }
        return;
    }

    [[nodiscard]] IService* failed() const noexcept { return _failed; }

    void tick() noexcept
    {
        const MISC::time_t now = _now != nullptr ? _now() : MISC::time_t{0};
        _list.forEach([&](IService& s) { 
            if (s.isRunning()) s.tick(now); 
        });
    }

private:
    ClockFn _now = nullptr;
    mem::List<IService> _list;
    IService* _failed = nullptr;
};

inline IService::IService(ServiceManager* mgr) noexcept
{
    if (mgr != nullptr) {
        mgr->attach(*this);
    }
}

} // namespace comm
