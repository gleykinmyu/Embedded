#include "ph350_ctrl.h"

#include "device_guard.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "ota_busy.h"
#include "transport/types.hpp"

#include <cstdio>
#include <cstring>

static const char* TAG = "ph350";
static const char* TAG_BUS = "ccam";

uint32_t Ph350Ctrl::nowMs() {
    return static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
}

int64_t Ph350Ctrl::nowUs() {
    return esp_timer_get_time();
}

namespace {

void format_wire(const uint8_t* data, size_t len, char* out, size_t cap) {
    if (!out || cap == 0) {
        return;
    }
    size_t o = 0;
    for (size_t i = 0; i < len && o + 8 < cap; ++i) {
        const uint8_t b = data[i];
        int n = 0;
        if (b == ccam::kStx) {
            n = snprintf(out + o, cap - o, "%s", "<STX>");
        } else if (b == ccam::kEtx) {
            n = snprintf(out + o, cap - o, "%s", "<ETX>");
        } else if (b == ccam::kAck) {
            n = snprintf(out + o, cap - o, "%s", "<ACK>");
        } else if (b == ccam::kPtCr) {
            n = snprintf(out + o, cap - o, "%s", "<CR>");
        } else if (b == ccam::kPtPrefix) {
            n = snprintf(out + o, cap - o, "%s", "#");
        } else if (b >= 32 && b <= 126) {
            n = snprintf(out + o, cap - o, "%c", static_cast<char>(b));
        } else {
            n = snprintf(out + o, cap - o, "\\x%02X", b);
        }
        if (n <= 0) {
            break;
        }
        o += static_cast<size_t>(n);
    }
    out[o < cap ? o : cap - 1] = '\0';
}

void bus_trace(char dir, const uint8_t* data, size_t len) {
    char text[192];
    format_wire(data, len, text, sizeof(text));
    ESP_LOGI(TAG_BUS, "%cX len=%u %s", dir, static_cast<unsigned>(len), text);
}

constexpr char kNvsNs[] = "ph350";
constexpr char kKeyRate[] = "rate";
constexpr char kKeyInv[] = "invert"; // bit0 pan,1 tilt,2 zoom,3 focus
constexpr char kKeyPt[] = "pt_model";
constexpr char kKeyCam[] = "cam_model";
constexpr char kKeyLimits[] = "limits";

template <typename T>
T* place(void* storage, ccam::Transport& bus) {
    return new (storage) T(bus);
}

bool model_supported(ccam::devices::PtModelId pt, ccam::devices::CameraModelId cam) {
    using namespace ccam::devices;
    switch (pt) {
    case PtModelId::Ph350:
    case PtModelId::Ph360:
    case PtModelId::Ph650:
    case PtModelId::Ph405:
    case PtModelId::He130:
    case PtModelId::He870:
    case PtModelId::Ue150:
        break;
    default:
        return false;
    }
    switch (cam) {
    case CameraModelId::E650:
    case CameraModelId::E600:
    case CameraModelId::He130:
    case CameraModelId::He870:
    case CameraModelId::Ue150:
        break;
    default:
        return false;
    }
    return true;
}

} // namespace

Ph350Ctrl::DeviceLock::DeviceLock(Ph350Ctrl& ctrl) : ctrl_(ctrl) {
    if (ctrl_.dev_mu_ == nullptr) {
        return;
    }
    if (xSemaphoreGetMutexHolder(ctrl_.dev_mu_) == xTaskGetCurrentTaskHandle()) {
        return; /* уже держим — не брать повторно и не отдавать в деструкторе */
    }
    xSemaphoreTake(ctrl_.dev_mu_, portMAX_DELAY);
    held_ = true;
}

Ph350Ctrl::DeviceLock::~DeviceLock() {
    if (held_ && ctrl_.dev_mu_ != nullptr) {
        xSemaphoreGive(ctrl_.dev_mu_);
    }
}

Ph350Ctrl::Ph350Ctrl() : bus_(uart_, &Ph350Ctrl::nowMs, lock_) {
    dev_mu_ = xSemaphoreCreateMutex();
    constructDevices(pt_id_, cam_id_);
}

static void ota_quiesce(void *p)
{
    auto *self = static_cast<Ph350Ctrl *>(p);
    if (self == nullptr) {
        return;
    }
    Ph350Ctrl::DeviceLock lock(*self);
}

bool Ph350Ctrl::otaHalt(void *p)
{
    auto *self = static_cast<Ph350Ctrl *>(p);
    if (self == nullptr || self->pt_ == nullptr) {
        return false;
    }
    DeviceLock lock(*self);
    /* otaIsBusy уже true: это не новая скорость, а обязательный STOP до flash. */
    return self->motion_.haltTracked(*self->pt_, &Ph350Ctrl::nowUs) == ccam::Status::Ok;
}

Ph350Ctrl::~Ph350Ctrl() {
    otaBindQuiesce(nullptr, nullptr);
    otaBindHalt(nullptr, nullptr);
    health_run_ = false;
    for (int i = 0; i < 80 && health_alive_; ++i) {
        vTaskDelay(pdMS_TO_TICKS(25));
    }
    const bool forced = health_alive_;
    if (forced && health_task_ != nullptr) {
        ESP_LOGE(TAG, "health task stuck, deleting");
        vTaskDelete(health_task_);
        health_task_ = nullptr;
        health_alive_ = false;
    }
    destroyDevices();
    if (!forced && dev_mu_ != nullptr) {
        vSemaphoreDelete(dev_mu_);
        dev_mu_ = nullptr;
    }
}

void Ph350Ctrl::destroyDevices() {
    health_.bind(nullptr);
    health_.bindCam(nullptr);
    health_.reset();
    if (pt_) {
        pt_->~PtDeviceBase();
        pt_ = nullptr;
    }
    if (cam_) {
        cam_->~CameraDeviceBase();
        cam_ = nullptr;
    }
}

bool Ph350Ctrl::constructDevices(ccam::devices::PtModelId pt, ccam::devices::CameraModelId cam) {
    using namespace ccam::devices;
    DeviceLock guard(*this);
    if (pt_ != nullptr && motion_.haltTracked(*pt_, &Ph350Ctrl::nowUs) != ccam::Status::Ok) {
        /* StopPending на старом объекте. Не уничтожать его и не менять модель. */
        return false;
    }

    destroyDevices();
    publishHealth();
    pt_id_ = pt;
    cam_id_ = cam;

    switch (pt) {
    case PtModelId::Ph360:
        pt_ = place<Ph360Pt>(&pt_storage_, bus_);
        break;
    case PtModelId::Ph650:
        pt_ = place<Ph650Pt>(&pt_storage_, bus_);
        break;
    case PtModelId::Ph405:
        pt_ = place<Ph405Pt>(&pt_storage_, bus_);
        break;
    case PtModelId::He130:
        pt_ = place<He130Pt>(&pt_storage_, bus_);
        break;
    case PtModelId::He870:
        pt_ = place<He870Pt>(&pt_storage_, bus_);
        break;
    case PtModelId::Ue150:
        pt_ = place<Ue150Pt>(&pt_storage_, bus_);
        break;
    case PtModelId::Ph350:
    case PtModelId::IntegratedPtz:
    case PtModelId::Generic:
    default:
        pt_id_ = PtModelId::Ph350;
        pt_ = place<Ph350Pt>(&pt_storage_, bus_);
        break;
    }

    switch (cam) {
    case CameraModelId::E600:
        cam_ = place<E600Camera>(&cam_storage_, bus_);
        break;
    case CameraModelId::He130:
        cam_ = place<He130Camera>(&cam_storage_, bus_);
        break;
    case CameraModelId::He870:
        cam_ = place<He870Camera>(&cam_storage_, bus_);
        break;
    case CameraModelId::Ue150:
        cam_ = place<Ue150Camera>(&cam_storage_, bus_);
        break;
    case CameraModelId::E650:
    case CameraModelId::Generic:
    default:
        cam_id_ = CameraModelId::E650;
        cam_ = place<E650Camera>(&cam_storage_, bus_);
        break;
    }

    pt_->setAxisInvert(invert_);
    health_.bind(pt_);
    health_.bindCam(cam_);
    device_epoch_ += 1;
    return true;
}

void Ph350Ctrl::healthTask(void* arg) {
    auto* self = static_cast<Ph350Ctrl*>(arg);
    while (self->health_run_) {
        const uint32_t now = Ph350Ctrl::nowMs();
        uint32_t epoch = 0;
        ccam::devices::PtLinkHealth::PollStep next = ccam::devices::PtLinkHealth::PollStep::Idle;
        {
            DeviceLock guard(*self);
            epoch = self->device_epoch_;
            if (!otaIsBusy() && self->health_.powerDue(now)) {
                self->expireMotion();
                next = self->health_.stepPower(now);
            }
            if (next == ccam::devices::PtLinkHealth::PollStep::Idle) {
                self->publishHealth();
            }
        }
        if (next == ccam::devices::PtLinkHealth::PollStep::Optics) {
            DeviceLock guard(*self);
            const DeviceGuard gate = device_guard(epoch, self->device_epoch_, otaIsBusy());
            if (gate == DeviceGuard::EpochChanged || self->pt_ == nullptr) {
                next = ccam::devices::PtLinkHealth::PollStep::Idle;
            } else if (gate == DeviceGuard::OtaBusy) {
                /* Питание этого прохода уже записано в health_, оптика — ещё нет.
                   Не публиковать смешанный снимок: снаружи остаётся последний целый. */
                next = ccam::devices::PtLinkHealth::PollStep::Idle;
            } else {
                self->expireMotion();
                next = self->health_.stepOptics(now);
                if (next == ccam::devices::PtLinkHealth::PollStep::Idle) {
                    self->publishHealth();
                }
            }
        }
        if (next == ccam::devices::PtLinkHealth::PollStep::Cam) {
            DeviceLock guard(*self);
            const DeviceGuard gate = device_guard(epoch, self->device_epoch_, otaIsBusy());
            if (gate == DeviceGuard::Ok && self->cam_ != nullptr) {
                self->expireMotion();
                self->health_.stepCam(now);
                self->publishHealth();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    self->health_task_ = nullptr;
    self->health_alive_ = false;
    vTaskDelete(nullptr);
}

void Ph350Ctrl::publishHealth() {
    HealthView next{};
    next.alive = health_.alive();
    next.powered = health_.powered();
    next.optics_alive = health_.opticsAlive();
    next.cam_alive = health_.camAlive();
    next.cam_tx = health_.camTxLikely();
    next.power_code = health_.powerCode();
    next.fail_count = health_.failStreak();
    next.optics_fail = health_.opticsFailStreak();
    next.cam_fail = health_.camFailStreak();
    const char *id = health_.camId();
    if (id != nullptr) {
        snprintf(next.cam_id, sizeof(next.cam_id), "%s", id);
    }
    portENTER_CRITICAL(&health_mux_);
    health_view_ = next;
    portEXIT_CRITICAL(&health_mux_);
}

void Ph350Ctrl::copyHealth(HealthView &out) const {
    portENTER_CRITICAL(&health_mux_);
    out = health_view_;
    portEXIT_CRITICAL(&health_mux_);
}

void Ph350Ctrl::startHealthTask() {
    if (health_alive_) {
        return;
    }
    health_run_ = true;
    health_alive_ = true;
    health_.bind(pt_);
    TaskHandle_t task = nullptr;
    if (xTaskCreate(&Ph350Ctrl::healthTask, "pt_health", 3072, this, 5, &task) != pdPASS) {
        health_run_ = false;
        health_alive_ = false;
        ESP_LOGE(TAG, "health task failed");
        return;
    }
    health_task_ = task;
}

bool Ph350Ctrl::begin() {
    if (!lock_.ok() || dev_mu_ == nullptr) {
        return false;
    }
    if (!uart_.open(ccam::kBaudRate)) {
        return false;
    }
    loadSettings();
    bus_.setTrace(&bus_trace);
    otaBindQuiesce(&ota_quiesce, this);
    otaBindHalt(&Ph350Ctrl::otaHalt, this);
    startHealthTask();
    ESP_LOGI(TAG, "ready pt=%s (%s) cam=%s (%s) rate=%u inv=p%u/t%u/z%u/f%u",
             pt_->modelLabel(), ccam::devices::ptModelKey(pt_id_), cam_->modelLabel(),
             ccam::devices::camModelKey(cam_id_), static_cast<unsigned>(rate_),
             invert_.pan ? 1u : 0u, invert_.tilt ? 1u : 0u, invert_.zoom ? 1u : 0u,
             invert_.focus ? 1u : 0u);
    return true;
}

void Ph350Ctrl::loadSettings() {
    nvs_handle_t h{};
    if (nvs_open(kNvsNs, NVS_READONLY, &h) != ESP_OK) {
        return;
    }
    uint8_t v = 0;
    if (nvs_get_u8(h, kKeyRate, &v) == ESP_OK && v >= 1 && v <= 100) {
        rate_ = v;
    }
    if (nvs_get_u8(h, kKeyInv, &v) == ESP_OK) {
        invert_.pan = (v & 0x01) != 0;
        invert_.tilt = (v & 0x02) != 0;
        invert_.zoom = (v & 0x04) != 0;
        invert_.focus = (v & 0x08) != 0;
    }
    if (nvs_get_u8(h, kKeyLimits, &v) == ESP_OK) {
        limits_ = v;
    }

    char pt_key[12] = {};
    char cam_key[12] = {};
    size_t pt_len = sizeof(pt_key);
    size_t cam_len = sizeof(cam_key);
    const bool have_pt = nvs_get_str(h, kKeyPt, pt_key, &pt_len) == ESP_OK;
    const bool have_cam = nvs_get_str(h, kKeyCam, cam_key, &cam_len) == ESP_OK;
    nvs_close(h);

    const auto pt =
        have_pt ? ccam::devices::ptModelFromKey(pt_key) : ccam::devices::PtModelId::Ph350;
    const auto cam =
        have_cam ? ccam::devices::camModelFromKey(cam_key) : ccam::devices::CameraModelId::E650;
    if (pt != pt_id_ || cam != cam_id_) {
        if (!constructDevices(pt, cam)) {
            ESP_LOGW(TAG, "NVS models kept aside, axis stop failed");
        }
    } else {
        setAxisInvert(invert_, false);
    }
}

void Ph350Ctrl::saveSettings() const {
    nvs_handle_t h{};
    if (nvs_open(kNvsNs, NVS_READWRITE, &h) != ESP_OK) {
        return;
    }
    const uint8_t bits = static_cast<uint8_t>((invert_.pan ? 0x01 : 0) | (invert_.tilt ? 0x02 : 0) |
                                              (invert_.zoom ? 0x04 : 0) | (invert_.focus ? 0x08 : 0));
    (void)nvs_set_u8(h, kKeyRate, rate_);
    (void)nvs_set_u8(h, kKeyInv, bits);
    (void)nvs_set_str(h, kKeyPt, ccam::devices::ptModelKey(pt_id_));
    (void)nvs_set_str(h, kKeyCam, ccam::devices::camModelKey(cam_id_));
    (void)nvs_commit(h);
    nvs_close(h);
}

bool Ph350Ctrl::setModels(ccam::devices::PtModelId pt, ccam::devices::CameraModelId cam,
                          bool persist) {
    if (!model_supported(pt, cam)) {
        return false;
    }
    const bool pt_changed = pt != pt_id_;
    if (!constructDevices(pt, cam)) {
        return false;
    }
    if (pt_changed && limits_ != 0) {
        /* Лимиты живут в головке: у другой модели они неизвестны. */
        limits_ = 0;
        saveLimits();
    }
    if (persist) {
        saveSettings();
    }
    ESP_LOGI(TAG, "models pt=%s cam=%s", ccam::devices::ptModelKey(pt_id_),
             ccam::devices::camModelKey(cam_id_));
    return true;
}

void Ph350Ctrl::setDefaultRate(uint8_t rate_1_to_100, bool persist) {
    /* Скорость по умолчанию не меняет Hold/Lease и не шлёт кадр. */
    if (rate_1_to_100 < 1) {
        rate_1_to_100 = 1;
    }
    if (rate_1_to_100 > 100) {
        rate_1_to_100 = 100;
    }
    if (rate_ == rate_1_to_100) {
        return;
    }
    rate_ = rate_1_to_100;
    if (persist) {
        saveSettings();
    }
}

void Ph350Ctrl::noteLatestPreset(uint8_t preset_1_to_99) {
    if (preset_1_to_99 < 1 || preset_1_to_99 > 99) {
        latest_preset_ = 0;
        at_latest_ = false;
        return;
    }
    latest_preset_ = preset_1_to_99;
    at_latest_ = true;
}

void Ph350Ctrl::clearAtLatest() {
    at_latest_ = false;
}

void Ph350Ctrl::setAxisInvert(const pt::AxisInvert& inv, bool persist) {
    DeviceLock guard(*this);
    const bool same = invert_.pan == inv.pan && invert_.tilt == inv.tilt && invert_.zoom == inv.zoom &&
                      invert_.focus == inv.focus;
    invert_ = inv;
    if (pt_ != nullptr) {
        pt_->setAxisInvert(invert_);
    }
    if (persist && !same) {
        saveSettings();
    }
}

uint8_t Ph350Ctrl::physLimitIndex(ccam::LimitAxis axis) const {
    using ccam::LimitAxis;
    switch (axis) {
    case LimitAxis::TiltUp:
        return invert_.tilt ? 1 : 0;
    case LimitAxis::TiltDown:
        return invert_.tilt ? 0 : 1;
    case LimitAxis::PanLeft:
        return invert_.pan ? 3 : 2;
    case LimitAxis::PanRight:
        return invert_.pan ? 2 : 3;
    }
    return 0xFF;
}

void Ph350Ctrl::noteLimit(ccam::LimitAxis axis, ccam::Limitation state) {
    const uint8_t i = physLimitIndex(axis);
    if (i > 3) {
        return;
    }
    const auto v = static_cast<uint8_t>(state == ccam::Limitation::Set ? LimitState::Set
                                                                        : LimitState::Release);
    const uint8_t next = static_cast<uint8_t>((limits_ & ~(0x03u << (2 * i))) | (v << (2 * i)));
    if (next == limits_) {
        return;
    }
    limits_ = next;
    saveLimits();
}

Ph350Ctrl::LimitState Ph350Ctrl::limitState(ccam::LimitAxis axis) const {
    const uint8_t i = physLimitIndex(axis);
    if (i > 3) {
        return LimitState::Unknown;
    }
    const uint8_t v = (limits_ >> (2 * i)) & 0x03u;
    return v <= 2 ? static_cast<LimitState>(v) : LimitState::Unknown;
}

void Ph350Ctrl::saveLimits() const {
    nvs_handle_t h{};
    if (nvs_open(kNvsNs, NVS_READWRITE, &h) != ESP_OK) {
        ESP_LOGW(TAG, "limits: nvs open fail");
        return;
    }
    if (nvs_set_u8(h, kKeyLimits, limits_) != ESP_OK || nvs_commit(h) != ESP_OK) {
        ESP_LOGW(TAG, "limits: nvs write fail");
    }
    nvs_close(h);
}

int8_t Ph350Ctrl::rateToSigned(uint8_t rate_1_to_100, int sign) {
    if (rate_1_to_100 < 1) {
        rate_1_to_100 = 1;
    }
    if (rate_1_to_100 > 100) {
        rate_1_to_100 = 100;
    }
    return static_cast<int8_t>(sign < 0 ? -static_cast<int>(rate_1_to_100)
                                        : static_cast<int>(rate_1_to_100));
}

ccam::Status Ph350Ctrl::driveAxis(pt::Axis axis, int8_t speed, int64_t lease_us) {
    DeviceLock guard(*this);
    if (pt_ == nullptr) {
        return ccam::Status::Io;
    }
    return motion_.drive(*pt_, &Ph350Ctrl::nowUs, axis, speed, lease_us, !otaIsBusy());
}

ccam::Status Ph350Ctrl::stopAxis(pt::Axis axis) {
    DeviceLock guard(*this);
    if (pt_ == nullptr) {
        return ccam::Status::Io;
    }
    return motion_.stopAxis(*pt_, &Ph350Ctrl::nowUs, axis);
}

ccam::Status Ph350Ctrl::stopAxes(const pt::Axis *axes, size_t count) {
    DeviceLock guard(*this);
    if (pt_ == nullptr) {
        return ccam::Status::Io;
    }
    return motion_.stopAxes(*pt_, &Ph350Ctrl::nowUs, axes, count);
}

void Ph350Ctrl::expireMotion() {
    DeviceLock guard(*this);
    if (otaIsBusy() || pt_ == nullptr) {
        return;
    }
    motion_.expire(*pt_, &Ph350Ctrl::nowUs);
}

ccam::Status Ph350Ctrl::haltMotion() {
    DeviceLock guard(*this);
    if (pt_ == nullptr) {
        return ccam::Status::Io;
    }
    return motion_.haltTracked(*pt_, &Ph350Ctrl::nowUs);
}

bool Ph350Ctrl::globalStopOpen() {
    DeviceLock guard(*this);
    return motion_.globalStopOpen();
}

ccam::Status Ph350Ctrl::stopLegacy() {
    DeviceLock guard(*this);
    if (pt_ == nullptr) {
        return ccam::Status::Io;
    }
    return motion_.stopLegacy(*pt_, &Ph350Ctrl::nowUs);
}

const char* Ph350Ctrl::statusText(ccam::Status st) {
    switch (st) {
    case ccam::Status::Ok:
        return "ok";
    case ccam::Status::Param:
        return "param";
    case ccam::Status::Io:
        return "io";
    case ccam::Status::Timeout:
        return "timeout";
    case ccam::Status::Nack:
        return "nack";
    case ccam::Status::Buffer:
        return "buffer";
    }
    return "?";
}
