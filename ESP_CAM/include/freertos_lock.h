#pragma once

#include "ilockable.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

/** BIF::ILockable поверх FreeRTOS mutex. */
class FreertosLock : public BIF::ILockable {
public:
    FreertosLock() : mu_(xSemaphoreCreateMutex()) {}

    ~FreertosLock()
    {
        if (mu_ != nullptr) {
            vSemaphoreDelete(mu_);
            mu_ = nullptr;
        }
    }

    FreertosLock(const FreertosLock&) = delete;
    FreertosLock& operator=(const FreertosLock&) = delete;

    bool ok() const { return mu_ != nullptr; }

protected:
    void lock() override { xSemaphoreTake(mu_, portMAX_DELAY); }
    void unlock() override { xSemaphoreGive(mu_); }

private:
    SemaphoreHandle_t mu_;
};
