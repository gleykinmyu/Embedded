#include "ph350_ctrl.h"

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
        return;
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

Ph350Ctrl::~Ph350Ctrl() {
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
    if (pt_) {
        pt_->~PtDeviceBase();
        pt_ = nullptr;
    }
    if (cam_) {
        cam_->~CameraDeviceBase();
        cam_ = nullptr;
    }
}

void Ph350Ctrl::constructDevices(ccam::devices::PtModelId pt, ccam::devices::CameraModelId cam) {
    using namespace ccam::devices;
    DeviceLock guard(*this);

    destroyDevices();
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
}

void Ph350Ctrl::healthTask(void* arg) {
    auto* self = static_cast<Ph350Ctrl*>(arg);
    while (self->health_run_) {
        if (!otaIsBusy()) {
            DeviceLock guard(*self);
            if (self->pt_ != nullptr) {
                self->health_.tick(Ph350Ctrl::nowMs());
            }
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    self->health_task_ = nullptr;
    self->health_alive_ = false;
    vTaskDelete(nullptr);
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
        constructDevices(pt, cam);
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
    constructDevices(pt, cam);
    if (persist) {
        saveSettings();
    }
    ESP_LOGI(TAG, "models pt=%s cam=%s", ccam::devices::ptModelKey(pt_id_),
             ccam::devices::camModelKey(cam_id_));
    return true;
}

void Ph350Ctrl::setDefaultRate(uint8_t rate_1_to_100, bool persist) {
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
