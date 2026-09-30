/**
 * @file mconsole.hpp
 * @brief Пульт: GroupConsole + Show (GRUP/SETT) + Browser + FIO.
 *
 * UI-наследник: onMechChanged / onGroupAck / onConsoleChanged; Nack — onNack.
 */

#pragma once

#include <cstdint>

#include "iFileSystem.hpp"
#include "browser.hpp"
#include "fio.hpp"
#include "showFile.hpp"
#include "smcp/mech/Console/group_bank.hpp"
#include "smcp/mech/Console/group_console.hpp"
#include "smcp/ilink.hpp"

struct Settings {
    bool isolateGroup = false;
    uint8_t reserved[7]{};
};
static_assert(sizeof(Settings) == 8u, "SETT wire size");

/** Шоуфайл: GRUP = CGroupBank, SETT. Живой и staging — один тип. */
class Show : public sf::Show<2> {
public:
    static constexpr uint32_t kSettingsSectionTag = 0x54544553u;

    smcp::CGroupBank<smcp::kGroupMaxCount, 1> group;
    sf::Section<Settings, 1> sett;

    explicit Show(smcp::IGroupConsole& console) noexcept
        : group{*this, console, true}
        , sett{*this, kSettingsSectionTag, false}
    {}

    [[nodiscard]] Settings& settings() noexcept { return *sett.begin(); }
    [[nodiscard]] const Settings& settings() const noexcept { return *sett.begin(); }
};

class MConsole;

/** FIO с хуком в пульт. */
class Fio final : public sf::Fio {
public:
    Fio(MConsole& owner, sf::IBrowser& browser, sf::IShow& live, sf::IShow& incoming,
        BIF::IFile& main, BIF::IFile& bak) noexcept
        : sf::Fio(browser, live, incoming, main, bak)
        , _owner(owner)
    {}

private:
    void onEvent(Event ev) noexcept override;
    MConsole& _owner;
};

class MConsole : public smcp::GroupConsole<24> {
    friend class Fio;

public:
    using Base = smcp::GroupConsole<24>;
    using Base::kMechCount;
    using Base::kSessionCount;
    static_assert(kMechCount <= smcp::kMechCount);
    using Settings = ::Settings;
    static constexpr uint8_t kGroupCount = smcp::kGroupMaxCount;
    static constexpr uint8_t kNoActiveGroup = smcp::IGroupConsole::kNoActiveGroup;
    static constexpr uint32_t kSettingsSectionTag = Show::kSettingsSectionTag;
    static constexpr uint16_t kFileCache = 64u;

    enum class Mode : uint8_t { Work = 0, Show = 1 };

    /** @a bak — тот же объект, что @a file, если резерва нет. */
    MConsole(BIF::IVolume& volume, BIF::IDirectory& dir, BIF::IFile& file, BIF::IFile& bak,
             smcp::ILink& link, smcp::Node::ClockFn clock) noexcept;

private:
    /* После Base registry: GServerSession регистрируется в Node::sessions(). */
    smcp::SessionBank<kSessionCount, smcp::GServerSession> _sessionBank;
    /* До cmechs: CMech ctor → storage() → registerAt. */
    smcp::IConsole::MechStore<kMechCount> _mechs;
    /* Staging до fio; show до cmechs — CGMech смотрит live GRUP. */
    Show _incoming;

public:
    Show show;
    smcp::CGMechBank<kMechCount> cmechs;
    sf::Browser<kFileCache> browser;
    Fio fio;

    [[nodiscard]] bool hasSelection() const noexcept;
    [[nodiscard]] smcp::Selection selectionFromMechs() const noexcept;

    using GroupConsole::activeGroup;
    using GroupConsole::clearActiveGroup;
    using GroupConsole::shownGroup;

    [[nodiscard]] Mode mode() const noexcept { return _mode; }
    void setMode(Mode mode) noexcept { _mode = mode; }

    [[nodiscard]] const smcp::msg::Nack& lastNack() const noexcept { return _lastNack; }
    [[nodiscard]] uint8_t lastNackReq() const noexcept { return _lastNackReq; }

    /** UI загружен (wait.onLoad): можно GetTelemetry. */
    void setUiReady() noexcept;
    [[nodiscard]] bool uiReady() const noexcept { return _uiReady; }

    /** Node::update, затем start() сохранённой сессии после HbLost. */
    void update() noexcept;

    /** Сессия, которую открыл start(). Нет — nullptr. */
    [[nodiscard]] smcp::ServerSession* link() const noexcept { return _link; }

    [[nodiscard]] uint8_t serverIndex(uint8_t server_id) const noexcept override
    {
        return server_id == smcp::msg::kServerIdMin ? 0u : 0xFFu;
    }

    [[nodiscard]] uint8_t serverId(uint8_t index) const noexcept override
    {
        return index == 0u ? smcp::msg::kServerIdMin : 0u;
    }

protected:
    [[nodiscard]] smcp::IConsole::MechReg* storage(uint8_t server_id) noexcept override
    {
        return server_id == smcp::msg::kServerIdMin ? &_mechs : nullptr;
    }

    virtual void onMechChanged(uint8_t mech_id) noexcept { (void)mech_id; }
    virtual void onConsoleChanged() noexcept {}

    void onTelemetry(uint8_t src_id, const smcp::msg::Telemetry& body) noexcept override;
    void onNack(smcp::Session* session, const smcp::TxSlot& req,
                const smcp::msg::Nack& reply) noexcept override;
    void onStatus(Status status) noexcept override;
    void onFault(smcp::Session* session, smcp::Node::Fault reason) noexcept override;
    void onLink(smcp::Session* session, bool up) noexcept override;

private:
    void onFioEvent(sf::Fio::Event ev) noexcept;
    void requestMechTelemetry() noexcept;

    Mode _mode = Mode::Work;
    smcp::msg::Nack _lastNack{};
    uint8_t _lastNackReq = smcp::msg::Select::kId;
    /** Слот с start(). Тот же объект после close. */
    smcp::ServerSession* _link = nullptr;
    /** update() снова зовёт start() после HbLost. */
    bool _retry = false;
    bool _uiReady = false;
};
