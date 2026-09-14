/**
 * @file mconsole_draft.hpp
 * @brief Черновик нового MConsole: Console + ShowFile + Browser + банки.
 *
 * Не подключать из UI. Живой MConsole пока старый.
 *
 * Состав:
 *   Console<24>     — шина, сессии, inventory CMech*
 *   Show            — объект шоуфайла внутри консоли (CGroupBank + SETT)
 *   CMechBank       — объекты осей (register в Console)
 *   Browser<64>     — каталог тома
 *   Show _scratch   — staging на import (тот же тип)
 */

#pragma once

#include <cstdint>

#include "iFileSystem.hpp"
#include "smcp/Console/browser.hpp"
#include "smcp/Console/cmech.hpp"
#include "smcp/Console/console.hpp"
#include "smcp/Console/group.hpp"
#include "smcp/Console/show_model.hpp"
#include "smcp/transport/ilink.hpp"

namespace draft {

inline constexpr uint32_t kSettingsSectionTag = 0x54544553u;
inline constexpr uint16_t kFileCache = 64u;

struct Settings {
    bool isolateGroup = false;
    uint8_t reserved[7]{};
};
static_assert(sizeof(Settings) == 8u, "SETT wire size");

/** Шоуфайл: GRUP = CGroupBank, SETT. Живой и staging — один тип. */
class Show : public smcp::file::ShowFile<2> {
public:
    smcp::CGroupBank<smcp::kGroupMaxCount> grup;
    smcp::file::Section<Settings, 1> sett;

    explicit Show(smcp::IConsole& console) noexcept
        : grup{*this, console, true}
        , sett{*this, kSettingsSectionTag, false}
    {}
};

class MConsole : public smcp::Console<24> {
public:
    using Console = smcp::Console<24>;
    using Console::kMechCount;
    static constexpr uint8_t kGroupCount = smcp::kGroupMaxCount;
    static constexpr uint8_t kNoActiveGroup = 0xFFu;

    enum class Mode : uint8_t { Work = 0, Show = 1 };

    enum class Status : uint8_t {
        Ok = 0,
        NoShowOpen,
        TemplateProtected,
        BadMagic,
        BadVersion,
        BadHeaderCrc,
        BadBodyCrc,
        BadLayout,
        Truncated,
        MissingGrup,
        BadGroups,
        IoError,
        InvalidName,
        NotFound,
        FileExists,
        NotMounted,
        OpenDirFailed,
        PathTooLong,
        OpenFileProtected,
    };

    MConsole(BIF::IVolume& volume, BIF::IDirectory& dir, BIF::IFile& file, smcp::ILink& link,
             smcp::Node::ClockFn clock) noexcept;

    smcp::CMechBank<kMechCount> cmechs;
    Show show;
    smcp::file::Browser<kFileCache> browser;

    void setMirror(BIF::IFile* mirror) noexcept { _mirror = mirror; }
    [[nodiscard]] bool restoreMirror() noexcept;

    [[nodiscard]] Status status() const noexcept { return _status; }
    [[nodiscard]] static const char* statusText(Status status) noexcept;

    [[nodiscard]] Mode mode() const noexcept { return _mode; }
    void setMode(Mode mode) noexcept { _mode = mode; }

    [[nodiscard]] bool hasSelection() const noexcept;

    [[nodiscard]] uint8_t activeGroup() const noexcept { return _activeGroup; }
    [[nodiscard]] uint8_t uiActiveGroup() const noexcept
    {
        return _groupSelectPending ? _pendingActiveGroup : _activeGroup;
    }

    void newShow() noexcept;
    [[nodiscard]] bool openShow(const char* name) noexcept;
    [[nodiscard]] bool saveShow() noexcept;
    [[nodiscard]] bool saveShowAs(const char* name, bool confirmed = false) noexcept;
    [[nodiscard]] bool removeShow(const char* name) noexcept;

    [[nodiscard]] const char* showName() const noexcept { return show.name(); }

    [[nodiscard]] static const char* showBaseName(const char* path) noexcept;
    [[nodiscard]] static bool isTemplateName(const char* name) noexcept;

    void setOnMechChanged(void (*fn)(uint8_t) noexcept) noexcept { _onMechChanged = fn; }
    void setOnNack(void (*fn)() noexcept) noexcept { _onNack = fn; }
    void setOnSelectAck(void (*fn)() noexcept) noexcept { _onSelectAck = fn; }
    void setOnPhase(void (*fn)(Phase) noexcept) noexcept { _onPhase = fn; }
    void setOnShowChanged(void (*fn)() noexcept) noexcept { _onShowChanged = fn; }

    [[nodiscard]] smcp::msg::ErrorCode lastNack() const noexcept { return _lastNack; }
    [[nodiscard]] smcp::msg::MsgId lastNackReq() const noexcept { return _lastNackReq; }
    [[nodiscard]] uint8_t lastNackDetail() const noexcept { return _lastNackDetail; }

private:
    void onTelemetry(const smcp::msg::Header& hdr,
                     const smcp::msg::Telemetry& body) noexcept override;
    void onAck(smcp::Session* session, const smcp::TxSlot& req) noexcept override;
    void onNack(smcp::Session* session, const smcp::TxSlot& req,
                const smcp::msg::Nack& reply) noexcept override;
    void onPhase(Phase phase) noexcept override;

    [[nodiscard]] bool fail(Status st) noexcept
    {
        _status = st;
        return false;
    }
    [[nodiscard]] bool ok() noexcept
    {
        _status = Status::Ok;
        return true;
    }

    [[nodiscard]] static bool groupsValid(const Show& show) noexcept;
    [[nodiscard]] static Status mapFile(smcp::file::Status st) noexcept;
    [[nodiscard]] static Status mapBrowser(smcp::file::IBrowser::Status st) noexcept;

    [[nodiscard]] bool ensureDir() noexcept;
    [[nodiscard]] bool commitScratch() noexcept;
    [[nodiscard]] bool importFrom(BIF::IFile& io, const char* path) noexcept;
    [[nodiscard]] bool exportTo(BIF::IFile& io, const char* path) noexcept;
    void persistMirror() noexcept;
    void notifyShow() noexcept;

    Show _scratch;
    BIF::IFile& _file;
    BIF::IFile* _mirror = nullptr;

    Mode _mode = Mode::Work;
    Status _status = Status::Ok;
    uint8_t _activeGroup = kNoActiveGroup;
    uint8_t _pendingActiveGroup = kNoActiveGroup;
    bool _groupSelectPending = false;

    smcp::msg::ErrorCode _lastNack = smcp::msg::ErrorCode::Ok;
    smcp::msg::MsgId _lastNackReq = smcp::msg::MsgId::Select;
    uint8_t _lastNackDetail = smcp::msg::kNackDetailNone;

    void (*_onMechChanged)(uint8_t) noexcept = nullptr;
    void (*_onNack)() noexcept = nullptr;
    void (*_onSelectAck)() noexcept = nullptr;
    void (*_onPhase)(Phase) noexcept = nullptr;
    void (*_onShowChanged)() noexcept = nullptr;
};

} // namespace draft
