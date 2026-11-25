#pragma once

#include "devices/device_caps.hpp"
#include "esp_http_server.h"

/**
 * Общие проверки доступа. Особые сочетания capability (вид aim, camera_link,
 * левая и правая клавиши меню) остаются в обработчике операции.
 * sessionActive() здесь не используется.
 */
enum class Caller { None, Operator, Service };

enum class Cap {
    AbsoluteAim,
    AbsoluteAimSpeed,
    PresetMap,
    PresetOptions,
    LimitSet,
    LimitToggle,
    Tally,
    Fan,
    Wiper,
    Washer,
    Wireless,
    Option,
    TiltSpan,
    Heater,
    Defroster,
    AimControl,
    AimDisplayed,
    Optics,
    ExposureReading,
    Landing,
    HeadVersion,
    PowerCameraLink,
    HdDetail,
    Negative,
    Scan,
    Chroma,
    BlackShade,
    Synchro,
    BlackBalance,
    Paint,
    Genlock,
    Gamma,
    AgcCeiling,
    MasterDetail,
    Monitor,
    LensInterface,
    MenuLeftRight,
};

enum class RawAccess { Disabled, Operator, All };

/** Общая операция. Заголовок Authorization, даже неверный, отсекает cookie. Idle не двигает. */
Caller http_caller(httpd_req_t *req);
/**
 * Зонд сессии. Cookie важнее Bearer. Неверный Bearer при живой cookie — оператор.
 * Idle не сдвигается.
 */
Caller http_presented(httpd_req_t *req);
/** Операторская операция. Authorization не смотрится. Idle не двигает. */
bool http_operator(httpd_req_t *req);
/**
 * Продлить idle той operator session, чья cookie всё ещё на этом запросе.
 * Вызывать только после успешного результата operator-операции.
 * ESP_OK отправки 401, 403 или 500 таким результатом не является.
 * Service, probe и неуспех сюда не попадают. Новая сессия с другой cookie не продлевается.
 */
void http_slide_operator(httpd_req_t *req);
bool http_ui_client(httpd_req_t *req);

bool http_head_cap(const ccam::devices::PtCaps &caps, Cap cap);
bool http_cam_cap(const ccam::devices::CamCaps &caps, Cap cap);

bool http_ota_busy();

/** Пока только RAM, по умолчанию Operator. NVS появится вместе с PUT политики. */
RawAccess http_raw_access();
void http_set_raw_access(RawAccess access);
bool http_raw_allowed(Caller caller);
