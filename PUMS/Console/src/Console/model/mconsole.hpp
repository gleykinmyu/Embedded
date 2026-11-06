/**
 * @file mconsole.hpp
 * @brief Пульт: smcp::show::GroupConsole + Show (GRUP/SETT) + Browser + FIO.
 *
 * UI-наследник: onAxisChanged / onGroupAck / onConsoleChanged; Nack — onNack.
 */

#pragma once

#include <cstdint>

#include "iFileSystem.hpp"
#include "browser.hpp"
#include "fio.hpp"
#include "mem/range_alloc.hpp"
#include "mem/slot.hpp"
#include "showFile.hpp"
#include "segment/axis.hpp"
#include "smcp/show/patch.hpp"
#include "smcp/show/group_bank.hpp"
#include "smcp/show/group_console.hpp"
#include "smcp/ilink.hpp"

struct Settings {
    bool isolateGroup = false;
    uint8_t reserved[7]{};
};
static_assert(sizeof(Settings) == 8u, "SETT wire size");

/** Шоуфайл: ATYP/APCH + GRUP + SETT. Одна копия в RAM (live). */
class Show : public sf::Show<4> {
public:
    static constexpr uint32_t kMagic = 0x534D4350u; /**< "SMCP". */
    static constexpr uint16_t kVersion = 0x0118u; /**< 0x0118: ATYP + APCH (show::Patch / PatchRec) */
    static constexpr sf::Tag kSettingsSectionTag = 0x54544553u;

    smcp::show::Patch<> patch;
    smcp::show::CGroupBank<smcp::show::kGroupMaxCount> group;
    sf::Section<Settings, 1> sett;

    explicit Show(smcp::show::GroupConsole& console) noexcept
        : sf::Show<4>(kMagic, kVersion)
        , patch{*this, true}
        , group{*this, console, true}
        , sett{*this, kSettingsSectionTag, false}
    {}

    [[nodiscard]] Settings& settings() noexcept { return *sett.begin(); }
    [[nodiscard]] const Settings& settings() const noexcept { return *sett.begin(); }

    [[nodiscard]] bool isValid() const noexcept override
    {
        return sf::Show<4>::isValid() && patch.isValid();
    }
};

class MConsole;

/** FIO с хуком в пульт. */
class Fio final : public sf::Fio {
public:
    Fio(MConsole& owner, sf::IBrowser& browser, sf::IShow& live, BIF::IFile& main,
        BIF::IFile& bak) noexcept
        : sf::Fio(browser, live, main, bak)
        , _owner(owner)
    {}

private:
    void onEvent(Event ev) noexcept override;
    MConsole& _owner;
};

class MConsole : public smcp::show::GroupConsole {
    friend class Fio;

public:
    using Base = smcp::show::GroupConsole;
    static constexpr uint8_t kMechCount = 24u;
    static constexpr uint8_t kSessionCount = 1u;
    static_assert(kMechCount <= smcp::kMechCount);
    using Settings = ::Settings;
    static constexpr uint8_t kGroupCount = smcp::show::kGroupMaxCount;
    static constexpr uint8_t kNoActiveGroup = smcp::show::GroupConsole::kNoActiveGroup;
    static constexpr sf::Tag kSettingsSectionTag = Show::kSettingsSectionTag;
    static constexpr uint16_t kFileCache = 64u;

    enum class Mode : uint8_t { Work = 0, Show = 1 };

    /** @a file — SD; @a bak — W25 LKG (отдельный IFile). */
    MConsole(BIF::IVolume& volume, BIF::IDirectory& dir, BIF::IFile& file, BIF::IFile& bak,
             smcp::ILink& link, smcp::Node::ClockFn clock) noexcept;

    /** Node::begin + LKG с bak (если есть). */
    void begin(uint8_t console_id) noexcept;

private:
    /* Плотный пул show::Session (PolyStore). */
    smcp::SessionBank<kSessionCount, smcp::show::Session> _sessionBank;

    /** Потолок payload: все оси × все сессии (фактические диапазоны — плотные). */
    static constexpr uint8_t kProfileSlotCap = static_cast<uint8_t>(
        smcp::axis::kAxisCount * kSessionCount);
    static_assert(static_cast<unsigned>(smcp::axis::kAxisCount) * kSessionCount
                      <= 255u,
                  "ProfileSlot pool must fit uint8_t SizeT");
    mem::SlotStore8<smcp::show::ProfileSlot, kProfileSlotCap> _profileSlots{};
    mem::RangeAllocator8<smcp::show::ProfileSlot, kSessionCount> _profileRanges;

public:
    /* До cmechs: smcp::show::CGMech смотрит live GRUP. */
    Show show;
    /* Плотный console::AxisStore&lt;CGMech&gt; = inventory пульта. */
    smcp::show::CGMechBank<kMechCount> cmechs;
    sf::Browser<kFileCache> browser;
    Fio fio;

    [[nodiscard]] bool hasSelection() const noexcept;
    [[nodiscard]] smcp::segment::Selection selectionFromMechs() const noexcept;

    using Base::activeGroup;
    using Base::clearActiveGroup;
    using Base::shownGroup;

    [[nodiscard]] Mode mode() const noexcept { return _mode; }
    void setMode(Mode mode) noexcept { _mode = mode; }

    [[nodiscard]] const smcp::session::Nack& lastNack() const noexcept { return _lastNack; }
    [[nodiscard]] uint8_t lastNackReq() const noexcept { return _lastNackReq; }

    /** UI загружен (wait.onLoad): можно GetTelemetry. */
    void setUiReady() noexcept;
    [[nodiscard]] bool uiReady() const noexcept { return _uiReady; }

    /** Node::update, затем start() сохранённой сессии после HbLost. */
    void update() noexcept;

    /** Сессия, которую открыл start(). Нет — nullptr. */
    [[nodiscard]] smcp::show::Session* link() const noexcept { return _link; }

protected:
    [[nodiscard]] smcp::SessionSlots& sessions() noexcept override { return _sessionBank; }
    [[nodiscard]] smcp::segment::console::AxisSlots& axes() noexcept override { return cmechs; }
    [[nodiscard]] smcp::show::ProfileAllocator& profileAlloc() noexcept override
    {
        return _profileRanges;
    }

    virtual void onAxisChanged(uint8_t axis_id) noexcept { (void)axis_id; }
    /** @deprecated → onAxisChanged */
    virtual void onMechChanged(uint8_t axis_id) noexcept { onAxisChanged(axis_id); }
    virtual void onConsoleChanged() noexcept {}

    void onNack(smcp::Session* session, const smcp::TxSlot& req,
                const smcp::session::Nack& reply) noexcept override;
    void onState(State state) noexcept override;
    void onLink(smcp::Session* session, Link link) noexcept override;

private:
    void onFioEvent(sf::Fio::Event ev) noexcept;
    void requestMechTelemetry() noexcept;

    Mode _mode = Mode::Work;
    smcp::session::Nack _lastNack{};
    uint8_t _lastNackReq = smcp::segment::Select::kId;
    /** Слот с start(). Тот же объект после close. */
    smcp::show::Session* _link = nullptr;
    /** update() снова зовёт start() после HbLost. */
    bool _retry = false;
    bool _uiReady = false;
};
