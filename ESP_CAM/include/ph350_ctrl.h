#pragma once

#include "devices/camera_device.hpp"
#include "devices/device_caps.hpp"
#include "devices/device_types.hpp"
#include "devices/ipt_device.hpp"
#include "devices/models/e600.hpp"
#include "devices/models/e650.hpp"
#include "devices/models/he130.hpp"
#include "devices/models/he870.hpp"
#include "devices/models/ph350.hpp"
#include "devices/models/ph360.hpp"
#include "devices/models/ue150.hpp"
#include "devices/pt_device.hpp"
#include "devices/pt_link_health.hpp"
#include "freertos_lock.h"
#include "rs422_uart.h"
#include "transport/transport.hpp"
#include "transport/types.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>

namespace ph350_detail {

template <size_t A, size_t B>
struct Max2 {
    static constexpr size_t value = A > B ? A : B;
};

template <size_t A, size_t... Rest>
struct MaxN {
    static constexpr size_t value = Max2<A, MaxN<Rest...>::value>::value;
};

template <size_t A>
struct MaxN<A> {
    static constexpr size_t value = A;
};

} // namespace ph350_detail

/** Плата ETH01: UART RS-422 + lock + Transport + выбираемые PT/Cam модели. */
class Ph350Ctrl {
public:
    Ph350Ctrl();
    ~Ph350Ctrl();

    Ph350Ctrl(const Ph350Ctrl&) = delete;
    Ph350Ctrl& operator=(const Ph350Ctrl&) = delete;

    bool begin();

    ccam::devices::PtDeviceBase& pt() { return *pt_; }
    const ccam::devices::PtDeviceBase& pt() const { return *pt_; }

    ccam::devices::CameraDeviceBase& cam() { return *cam_; }
    const ccam::devices::CameraDeviceBase& cam() const { return *cam_; }

    ccam::devices::PtModelId ptModel() const { return pt_id_; }
    ccam::devices::CameraModelId camModel() const { return cam_id_; }

    /** Сменить модели (сразу + NVS). */
    bool setModels(ccam::devices::PtModelId pt, ccam::devices::CameraModelId cam,
                   bool persist = true);

    ccam::devices::PtCaps ptCaps() const { return pt_->capabilities(); }
    ccam::devices::CamCaps camCaps() const { return cam_->capabilities(); }

    /** Watchdog #O (ccam::PtLinkHealth). */
    const ccam::devices::PtLinkHealth& linkHealth() const { return health_; }
    ccam::devices::PtLinkHealth& linkHealth() { return health_; }

    /** UI/CLI скорость в %: 1=медленно .. 100=быстро (для ±move). */
    uint8_t defaultRate() const { return rate_; }
    void setDefaultRate(uint8_t rate_1_to_100, bool persist = true);

    /** Инверсии осей (NVS). */
    void setAxisInvert(const pt::AxisInvert& inv, bool persist = true);
    const pt::AxisInvert& axisInvert() const { return pt_->axisInvert(); }

    /** 1..100% + знак → int8_t для move() (−100..+100). */
    static int8_t rateToSigned(uint8_t rate_1_to_100, int sign);

    static const char* statusText(ccam::Status st);

    /** Сериализует смену моделей и опрос #O. Повторный захват с той же задачи — no-op. */
    class DeviceLock {
    public:
        explicit DeviceLock(Ph350Ctrl& ctrl);
        ~DeviceLock();
        DeviceLock(const DeviceLock&) = delete;
        DeviceLock& operator=(const DeviceLock&) = delete;

    private:
        Ph350Ctrl& ctrl_;
        bool held_{false};
    };

private:
    static uint32_t nowMs();
    static void healthTask(void* arg);
    void loadSettings();
    void saveSettings() const;
    void destroyDevices();
    void constructDevices(ccam::devices::PtModelId pt, ccam::devices::CameraModelId cam);
    void startHealthTask();

    static constexpr size_t kPtSize = ph350_detail::MaxN<
        sizeof(ccam::devices::Ph350Pt), sizeof(ccam::devices::Ph360Pt),
        sizeof(ccam::devices::Ph650Pt), sizeof(ccam::devices::Ph405Pt),
        sizeof(ccam::devices::He130Pt), sizeof(ccam::devices::He870Pt),
        sizeof(ccam::devices::Ue150Pt)>::value;
    static constexpr size_t kPtAlign = ph350_detail::MaxN<
        alignof(ccam::devices::Ph350Pt), alignof(ccam::devices::Ph360Pt),
        alignof(ccam::devices::Ph650Pt), alignof(ccam::devices::Ph405Pt),
        alignof(ccam::devices::He130Pt), alignof(ccam::devices::He870Pt),
        alignof(ccam::devices::Ue150Pt)>::value;
    static constexpr size_t kCamSize = ph350_detail::MaxN<
        sizeof(ccam::devices::E650Camera), sizeof(ccam::devices::E600Camera),
        sizeof(ccam::devices::He130Camera), sizeof(ccam::devices::He870Camera),
        sizeof(ccam::devices::Ue150Camera)>::value;
    static constexpr size_t kCamAlign = ph350_detail::MaxN<
        alignof(ccam::devices::E650Camera), alignof(ccam::devices::E600Camera),
        alignof(ccam::devices::He130Camera), alignof(ccam::devices::He870Camera),
        alignof(ccam::devices::Ue150Camera)>::value;

    FreertosLock lock_{};
    Rs422Uart uart_{};
    ccam::Transport bus_;

    alignas(kPtAlign) unsigned char pt_storage_[kPtSize]{};
    alignas(kCamAlign) unsigned char cam_storage_[kCamSize]{};
    ccam::devices::PtDeviceBase* pt_{nullptr};
    ccam::devices::CameraDeviceBase* cam_{nullptr};
    ccam::devices::PtModelId pt_id_{ccam::devices::PtModelId::Ph350};
    ccam::devices::CameraModelId cam_id_{ccam::devices::CameraModelId::E650};

    uint8_t rate_{60};
    pt::AxisInvert invert_{};

    ccam::devices::PtLinkHealth health_{1000, 3};
    SemaphoreHandle_t dev_mu_{};
    TaskHandle_t health_task_{nullptr};
    volatile bool health_run_{false};
    volatile bool health_alive_{false};
};
