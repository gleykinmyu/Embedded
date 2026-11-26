#pragma once

#include "transport/types.hpp"

/**
 * Одноразовый конец operator-сессии.
 * Писать и читать под mutex HttpAuth. Поколения нет: допуск команды
 * сам применяет ожидающий конец, удерживая DeviceLock.
 */
struct SessionEndFlag {
    bool pending{false};

    void note() { pending = true; }

    bool take()
    {
        if (!pending) {
            return false;
        }
        pending = false;
        return true;
    }

    bool isPending() const { return pending; }
};

/** Результат одного прохода допуска. Второй конец в том же захвате не обрабатывается. */
enum class MotionGate : uint8_t {
    Proceed = 0,
    OtaBusy,
    StopFailed,
    Superseded,
    HaltOpen,
    Denied,
};

struct MotionGateResult {
    MotionGate gate{MotionGate::Proceed};
    ccam::Status stop_status{ccam::Status::Ok};
};

/**
 * Один проход. Вызывающий уже держит DeviceLock.
 * take_end / end_pending / admit берут и отпускают mutex HttpAuth.
 * halt — UART и выполняется без этого mutex.
 * Новый конец во время halt не запускает второй STOP: команда отклоняется.
 */
template <class TakeEnd, class Halt, class EndPending, class Admit>
MotionGateResult gateMotion(bool ota_busy, bool halt_open, TakeEnd take_end, Halt halt,
                            EndPending end_pending, Admit admit)
{
    MotionGateResult result;
    if (ota_busy) {
        result.gate = MotionGate::OtaBusy;
        return result;
    }
    if (take_end()) {
        result.stop_status = halt();
        if (end_pending()) {
            result.gate = MotionGate::Superseded;
            return result;
        }
        if (result.stop_status != ccam::Status::Ok) {
            result.gate = MotionGate::StopFailed;
            return result;
        }
    } else if (halt_open) {
        result.gate = MotionGate::HaltOpen;
        return result;
    }
    if (end_pending()) {
        result.gate = MotionGate::Superseded;
        return result;
    }
    if (!admit()) {
        result.gate = MotionGate::Denied;
        return result;
    }
    return result;
}
