/**
 * @file message.hpp
 * @brief Транспорт SMCP v1.5: кадр UART/RS-485, MSG_ID и payload между консолями и серверами.
 *
 * Блок 1 «Документация SMCP v1.5»: SYNC 0xAA55, заголовок 8 байт, CRC16-CCITT в конце кадра.
 * SRC_ID 0x01..0x0F — консоли; 0x10..0xEF — серверы; 0xFF — broadcast.
 *
 * Разобранное тело кадра — `smcp::msg::Message` (единый variant всех типов сообщений).
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <variant>

#include "group.hpp"
#include "mech.hpp"
#include "show_file.hpp"

namespace smcp {
namespace msg {

inline constexpr uint16_t kSyncMarker = 0xAA55u;
inline constexpr std::size_t kHeaderSize = 8u;
inline constexpr std::size_t kCrcSize = 2u;
inline constexpr std::size_t kMaxPayload = 250u;
inline constexpr std::size_t kMaxFrameSize = kHeaderSize + kMaxPayload + kCrcSize;

inline constexpr uint8_t kConsoleIdMin = 0x01u;
inline constexpr uint8_t kConsoleIdMax = 0x0Fu;
inline constexpr uint8_t kServerIdMin = 0x10u;
inline constexpr uint8_t kServerIdMax = 0xEFu;
inline constexpr uint8_t kBroadcastId = 0xFFu;

inline constexpr uint16_t kAckTimeoutControlMs = 100u;
inline constexpr uint16_t kAckTimeoutConfigMs = 200u;
inline constexpr uint8_t kAckRetryControl = 3u;
inline constexpr uint8_t kAckRetryConfig = 2u;
inline constexpr uint16_t kHeartbeatTimeoutMs = 500u;

/** Реестр MSG_ID (блок 1.5). */
enum class MsgId : uint8_t {
    Ack          = 0x01, /**< Подтверждение успешного выполнения (тот же PKT_ID). */
    Nack         = 0x02, /**< Отказ; payload — ErrorCode. */
    Heartbeat    = 0x03, /**< Контроль связи; таймаут сессии 500 мс. */
    SysInfo      = 0x04, /**< Версии ПО и статус Master/Checker. */
    GetLog       = 0x05, /**< Выгрузка записей «чёрного ящика» (W25Q16). */

    Select       = 0x10, /**< Выделение/снятие маски штанкетов (action в payload). */
    ResetFault   = 0x12, /**< Сброс аварийного состояния оси (Clear FAULT). */

    SetTarget    = 0x20, /**< Взвод движения: цель, скорость, ускорение. */

    ShowFile     = 0x30, /**< Операции с шоуфайлами на SD (action + имя в payload). */
    SetConfig    = 0x36, /**< Запись MechConfig во Flash. */    
    GetConfig    = 0x37, /**< Чтение MechConfig из Flash. */

    Telemetry    = 0x40, /**< Поток телеметрии: позиция, скорость, статус. */

    CriticalErr  = 0xEE, /**< Авария Checker (MCU2); аппаратный сбой. */
};

/** Код ошибки в payload NACK (блок 1.6). */
enum class ErrorCode : uint8_t {
    Busy   = 0x01,
    Limits = 0x02,
    Crc    = 0x03,
    Safety = 0x05,
};

/** FSM штанкета (блок 2.1). */
enum class FsmState : uint8_t {
    Idle     = 0x00,
    Armed    = 0x01,
    Moving   = 0x02,
    Stopping = 0x03,
    Fault    = 0x04,
};

/** Состояния побайтового парсера (блок 1.4). */
enum class ParserState : uint8_t {
    WaitSync,
    GetHeader,
    GetPayload,
    CheckCrc,
};

#pragma pack(push, 1)

/** Заголовок кадра — 8 байт, выровнен по границе 8. */
struct Header {
    uint16_t sync = kSyncMarker;
    uint8_t src_id = 0;
    uint8_t dst_id = 0;
    uint8_t pkt_id = 0;
    MsgId msg_id = MsgId::Heartbeat;
    uint8_t len = 0;
    uint8_t reserved = 0;
};

static_assert(sizeof(Header) == kHeaderSize);

// --- 0x01..0x05: системные сообщения ---

struct Ack {
    static constexpr MsgId kId = MsgId::Ack;
};

struct Nack {
    static constexpr MsgId kId = MsgId::Nack;
    ErrorCode code = ErrorCode::Busy;
};

struct Heartbeat {
    static constexpr MsgId kId = MsgId::Heartbeat;
};

struct SysInfo {
    static constexpr MsgId kId = MsgId::SysInfo;
    uint16_t fw_version = 0;
    uint8_t master_status = 0;
    uint8_t checker_status = 0;
};

struct GetLog {
    static constexpr MsgId kId = MsgId::GetLog;
    uint32_t offset = 0;
    uint16_t count = 0;
};

static_assert(sizeof(Nack) == 1u);

// --- 0x10: select (классификатор действия в payload) ---

struct Select {
    static constexpr MsgId kId = MsgId::Select;

    enum class Action : uint8_t {
        Select,   /**< Выделить маску. */
        Deselect, /**< Снять выделение маски. */
    };

    Action action = Action::Select;
    Selection selection;
};

static_assert(sizeof(Select) == sizeof(uint64_t) + 1u);

// --- 0x12 ---

struct ResetFault {
    static constexpr MsgId kId = MsgId::ResetFault;
    uint8_t local_id = 0;
};

// --- 0x20 ---

struct SetTarget {
    static constexpr MsgId kId = MsgId::SetTarget;
    uint8_t local_id = 0;
    MotionTarget target;
};

static_assert(sizeof(SetTarget) == 9u);

// --- 0x30: операции с шоуфайлом (классификатор действия в payload) ---

struct ShowFile {
    static constexpr MsgId kId = MsgId::ShowFile;

    enum class Action : uint8_t {
        Load,   /**< Загрузка шоу по ID/имени + CRC32. */
        Save,   /**< Сохранение текущего шоу + синхронизация MCU. */
        SaveAs, /**< Запись проекта под новым именем/ID. */
        New,    /**< Создание пустого шоу с дефолтными лимитами. */
        Delete, /**< Удаление шоуфайла с SD-карты. */
        List,   /**< Список имён всех шоуфайлов на SD-карте. */
    };

    Action action = Action::Load;
    char name[file::kShowFileNameSize]{};
    uint16_t show_id = 0;
    uint32_t crc32 = 0;
};

// --- 0x36..0x37, 0x40, 0xEE: отдельные сообщения ---
struct SetConfig {    
    static constexpr MsgId kId = MsgId::SetConfig;
    uint8_t local_id = 0;
};

struct GetConfig {
    static constexpr MsgId kId = MsgId::GetConfig;
    uint8_t local_id = 0;
};

struct Telemetry {
    static constexpr MsgId kId = MsgId::Telemetry;

    uint8_t local_id = 0;
    uint8_t select_owner_id = kSelectOwnerNone; /**< SRC_ID консоли; 0 — не выделен. */
    int32_t position_mm = 0;
    int16_t speed_mm_s = 0;
    REG::BitMask<IMech::Status> status{};
};

static_assert(sizeof(Telemetry) == 9u);

struct CriticalErr {
    static constexpr MsgId kId = MsgId::CriticalErr;
    ErrorCode code = ErrorCode::Safety;
    uint8_t detail = 0;
};

#pragma pack(pop)

/** Разобранное тело SMCP-кадра. */
using Message = std::variant<Ack,
                             Nack,
                             Heartbeat,
                             SysInfo,
                             GetLog,
                             Select,
                             ResetFault,
                             SetTarget,
                             ShowFile,
                             SetConfig,
                             GetConfig,
                             Telemetry,
                             CriticalErr>;

/** Заголовок + разобранное тело. */

namespace detail {

template <typename T>
[[nodiscard]] constexpr MsgId msgIdOfBody(const T& body) noexcept
{
    return T::kId;
}

} // namespace detail

[[nodiscard]] inline MsgId msgIdOf(const Message& message) noexcept
{
    return std::visit([](const auto& body) { return detail::msgIdOfBody(body); }, message);
}

[[nodiscard]] constexpr std::size_t frameSize(uint8_t payload_len) noexcept
{
    return kHeaderSize + payload_len + kCrcSize;
}

[[nodiscard]] constexpr std::size_t crcOffset(uint8_t payload_len) noexcept
{
    return kHeaderSize + payload_len;
}

[[nodiscard]] constexpr bool isConsoleId(uint8_t id) noexcept
{
    return id >= kConsoleIdMin && id <= kConsoleIdMax;
}

[[nodiscard]] constexpr bool isServerId(uint8_t id) noexcept
{
    return id >= kServerIdMin && id <= kServerIdMax;
}

[[nodiscard]] constexpr bool isBroadcastId(uint8_t id) noexcept
{
    return id == kBroadcastId;
}

/** Сервер 0x10 обслуживает global_id 0..63, 0x11 — 64..127 и т.д. (блок 3.1). */
[[nodiscard]] constexpr uint8_t serverIdForGlobalMech(uint16_t global_id) noexcept
{
    return static_cast<uint8_t>(kServerIdMin + (global_id / kMechCount));
}

[[nodiscard]] constexpr uint8_t localIdForGlobalMech(uint16_t global_id) noexcept
{
    return static_cast<uint8_t>(global_id % kMechCount);
}

/** Кадр адресован этому узлу (или broadcast). */
[[nodiscard]] constexpr bool isAddressedTo(const Header& hdr, uint8_t node_id) noexcept
{
    return hdr.dst_id == node_id || hdr.dst_id == kBroadcastId;
}

/** Требует ли MSG_ID подтверждения ACK (блок 1.3). */
[[nodiscard]] constexpr bool requiresAck(MsgId id) noexcept
{
    switch (id) {
    case MsgId::Ack:
    case MsgId::Nack:
    case MsgId::Heartbeat:
    case MsgId::SysInfo:
    case MsgId::Telemetry:
    case MsgId::CriticalErr:
        return false;
    default:
        return true;
    }
}

/** Таймаут ожидания ACK, мс (блок 1.3). */
[[nodiscard]] constexpr uint16_t ackTimeoutMs(MsgId id) noexcept
{
    switch (id) {
    case MsgId::SetConfig:
    case MsgId::GetConfig:
    case MsgId::ShowFile:
        return kAckTimeoutConfigMs;
    default:
        return requiresAck(id) ? kAckTimeoutControlMs : 0u;
    }
}

/** Число повторов при отсутствии ACK (блок 1.3). */
[[nodiscard]] constexpr uint8_t ackRetryCount(MsgId id) noexcept
{
    switch (id) {
    case MsgId::SetConfig:
    case MsgId::GetConfig:
    case MsgId::ShowFile:
        return kAckRetryConfig;
    case MsgId::Select:
    case MsgId::ResetFault:
    case MsgId::SetTarget:
    case MsgId::GetLog:
        return kAckRetryControl;
    default:
        return 0u;
    }
}

} // namespace msg
} // namespace smcp
