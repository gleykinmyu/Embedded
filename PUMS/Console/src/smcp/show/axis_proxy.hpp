/**
 * @file axis_proxy.hpp
 * @brief AxisProxy : IAxis — прокси оси спектакля (5.3, параллельно CMech).
 *
 * Слот APCH + ShowType + live в IAxis + кэш Profile*.
 * IAxis::id() = route.axis_id; логический Patch.id — showId().
 */

#pragma once

#include <cstdint>

#include "smcp/mech/axis.hpp"
#include "smcp/mech/message.hpp"
#include "smcp/show/axis_pool.hpp"

namespace smcp {

class IConsole;

namespace show {

/**
 * IAxis пульта на слоте APCH (стабильный объект, не временный view).
 * TX через IConsole, если isPatched(); иначе no-op.
 */
class AxisProxy : public IAxis {
public:
    AxisProxy() noexcept;

    /**
     * Привязать к слоту @a slot_index в show_patch.
     * После load / смены route — syncFromPatch().
     */
    void bind(IConsole& console, AxisPool<>& show_patch, uint8_t slot_index) noexcept;

    /** Обновить type* и IAxis::id из текущего show::Patch (route). */
    void syncFromPatch() noexcept;

    [[nodiscard]] bool isBound() const noexcept;
    [[nodiscard]] bool isEmpty() const noexcept;
    [[nodiscard]] bool isPatched() const noexcept;

    [[nodiscard]] Patch* patch() noexcept;
    [[nodiscard]] const Patch* patch() const noexcept;

    [[nodiscard]] const ShowType* type() const noexcept { return _type; }

    [[nodiscard]] axis::Route route() const noexcept;

    /** Логический id шоу (show::Patch.id); phys — IAxis::id() / route().axis_id. */
    [[nodiscard]] uint16_t showId() const noexcept;
    [[nodiscard]] const char* name() const noexcept;

    [[nodiscard]] uint8_t slot() const noexcept { return _slot; }

    [[nodiscard]] const axis::Profile* profile() const noexcept { return _profile; }
    void setProfile(const axis::Profile* hw) noexcept { _profile = hw; }

    void select(uint8_t console_id) noexcept override;
    void block(bool blocked) noexcept override;
    void setTarget(const MotionTarget& target) noexcept override;
    void resetFault() noexcept override;

    /** Зеркало Telemetry в поля IAxis; чужой src / axis — no-op. */
    void onTelemetry(uint8_t src_id, const msg::Telemetry& telemetry) noexcept;

    void clearLive() noexcept;

private:
    IConsole* _console = nullptr;
    AxisPool<>* _show = nullptr;
    const ShowType* _type = nullptr;
    const axis::Profile* _profile = nullptr;
    uint8_t _slot = 0;
};

/**
 * N прокси параллельно APCH (ёмкости по умолчанию TypeBank/PatchBank).
 * Пока не в MConsole — рядом с CMechBank.
 */
class AxisProxyTable {
public:
    static constexpr uint8_t kCount = kPatchCount;

    AxisProxyTable(AxisPool<>& show_patch, IConsole& console) noexcept;

    /** После load / правки APCH — id/type с route. */
    void syncAll() noexcept;

    [[nodiscard]] AxisProxy& operator[](uint8_t i) noexcept;
    [[nodiscard]] const AxisProxy& operator[](uint8_t i) const noexcept;

    [[nodiscard]] AxisProxy* byId(uint16_t id) noexcept;

    void onTelemetry(uint8_t src_id, const msg::Telemetry& telemetry) noexcept;
    void clearLive() noexcept;
    void clearProfiles(uint8_t server_id) noexcept;

private:
    AxisPool<>* _show;
    IConsole* _console;
    AxisProxy _axes[kCount]{};
};

} // namespace show
} // namespace smcp
