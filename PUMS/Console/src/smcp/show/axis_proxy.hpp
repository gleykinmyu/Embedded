/**
 * @file axis_proxy.hpp
 * @brief AxisProxy : IAxis — прокси оси спектакля (5.3, параллельно show::Axis).
 *
 * Слот APCH + AxisType + live; Profile — через show::Session::profile(axis_id).
 * axisId() = route.axis_id; логический PatchRec.id — showId().
 */

#pragma once

#include <cstdint>

#include "segment/axis.hpp"
#include "segment/message.hpp"
#include "smcp/show/patch.hpp"

namespace smcp {

namespace segment { class Console; }

namespace show {

/**
 * IAxis пульта на слоте APCH (стабильный объект, не временный view).
 * TX через Console, если isRouted(); иначе no-op.
 */
class AxisProxy : public IAxis {
public:
    AxisProxy() noexcept = default;

    /**
     * Привязать к слоту @a slot_index в show_patch.
     * После load / смены route — syncFromPatch().
     */
    void bind(segment::Console& console, Patch<>& show_patch, uint8_t slot_index) noexcept;

    /** Обновить type* и _axis_id из текущего PatchRec (route). */
    void syncFromPatch() noexcept;

    [[nodiscard]] bool isBound() const noexcept;
    [[nodiscard]] bool isEmpty() const noexcept;
    [[nodiscard]] bool isRouted() const noexcept;

    [[nodiscard]] PatchRec* patch() noexcept;
    [[nodiscard]] const PatchRec* patch() const noexcept;

    [[nodiscard]] const AxisType* type() const noexcept { return _type; }

    [[nodiscard]] axis::Route route() const noexcept;

    /** Логический id шоу (PatchRec.id); phys — axisId() / route().axis_id. */
    [[nodiscard]] uint16_t showId() const noexcept;
    [[nodiscard]] const char* name() const noexcept;

    [[nodiscard]] uint8_t slot() const noexcept { return _slot; }

    /** Phys axis_id из PatchRec.route (после syncFromPatch). */
    [[nodiscard]] uint8_t axisId() const noexcept { return _axis_id; }
    [[nodiscard]] int32_t position() const noexcept override { return _position; }
    [[nodiscard]] REG::BitMask<axis::Status> status() const noexcept override { return _status; }
    [[nodiscard]] uint8_t holder() const noexcept override { return _holder; }

    /**
     * Profile железа: show::Session диапазона peer'а, поиск по axis_id.
     * Нет сессии / диапазона — nullptr (не кэшируется: compact безопасен).
     */
    [[nodiscard]] const axis::Profile* profile() const noexcept;

    [[nodiscard]] bool select(uint8_t console_id) noexcept override;
    [[nodiscard]] bool block(bool blocked) noexcept override;
    [[nodiscard]] bool setTarget(const axis::MotionTarget& target) noexcept override;
    [[nodiscard]] bool resetFault() noexcept override;

    /** Зеркало Telemetry; чужой src / axis — no-op. */
    void onTelemetry(uint8_t src_id, const segment::Telemetry& telemetry) noexcept;

    void clearLive() noexcept;

private:
    segment::Console* _console = nullptr;
    Patch<>* _show = nullptr;
    const AxisType* _type = nullptr;
    uint8_t _slot = 0;
    uint8_t _axis_id = 0;
    uint8_t _holder = kHolderNone;
    int32_t _position = 0;
    REG::BitMask<axis::Status> _status{};
};

/**
 * N прокси параллельно APCH (ёмкости по умолчанию TypeSection/PatchSection).
 * Пока не в MConsole — рядом с банком show::Axis / CGMech.
 */
class AxisProxyTable {
public:
    static constexpr uint8_t kCount = kPatchCount;

    AxisProxyTable(Patch<>& show_patch, segment::Console& console) noexcept;

    /** После load / правки APCH — id/type с route. */
    void syncAll() noexcept;

    [[nodiscard]] AxisProxy& operator[](uint8_t i) noexcept;
    [[nodiscard]] const AxisProxy& operator[](uint8_t i) const noexcept;

    [[nodiscard]] AxisProxy* byId(uint16_t id) noexcept;

    void onTelemetry(uint8_t src_id, const segment::Telemetry& telemetry) noexcept;
    void clearLive() noexcept;
    /** Профили в show::Session; no-op (диапазон free на link down). */
    void clearProfiles(uint8_t server_id) noexcept;

private:
    Patch<>* _show;
    segment::Console* _console;
    AxisProxy _axes[kCount]{};
};

} // namespace show
} // namespace smcp
