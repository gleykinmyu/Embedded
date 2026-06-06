#pragma once

#include "nexCompImpl.hpp"

namespace nex {
namespace comp {

/** from(RO); vid; en; loop; tim; stim(RO) */
class Audio : public Component {
public:
    /** user: источник (RO) */
    attr::NumRO<uint8_t> from;
    /** mcu: путь к ресурсу (при external) */
    attr::String<512> vid;

    void enable() noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::En, true);
    }

    void disable() noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::En, false);
    }

    void setLoop(bool on) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Loop, on);
    }

    void setPeriod(uint32_t v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Tim, v);
    }

    /** user: позиция воспроизведения (RO) */
    attr::NumRO<uint32_t> stim;

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        switch (tag) {
        case static_cast<uint8_t>(attr::Id::From):
            from.applyResponse(response);
            return;
        case static_cast<uint8_t>(attr::Id::Stim):
            stim.applyResponse(response);
            return;
        default:
            break;
        }
        Component::onResponse(tag, response);
    }

    Audio(Page& owner, const Literal& name, uint8_t id = 0)
        : Component(owner, name, Component::Type::Audio, id)
        , from{*this, attr::Id::From}
        , vid{*this, attr::Id::Vid}
        , stim{*this, attr::Id::Stim}
    {}
};

/** val; qty(RO); en(RO); open/read/write/close/find */
class FileStream : public Component {
public:
    /** user: буфер / смещение / результат операции */
    attr::Num<int32_t> val;
    /** user: количество прочитанных/записанных байт (RO) */
    attr::NumRO<uint32_t> qty;
    /** user: поток открыт (RO) */
    attr::NumRO<bool> en;

    void open(const char* path) noexcept
    {
        page.app.enqueue(Transaction{cmd::FileStream::open(name, path), page.ID, id()});
    }

    void read(uint32_t offset, uint32_t byteCount) noexcept
    {
        page.app.enqueue(Transaction{cmd::FileStream::read(name, offset, byteCount), page.ID, id()});
    }

    void write(uint32_t byteCount) noexcept
    {
        page.app.enqueue(Transaction{cmd::FileStream::write(name, byteCount), page.ID, id()});
    }

    void close() noexcept
    {
        page.app.enqueue(Transaction{cmd::FileStream::close(name), page.ID, id()});
    }

    void find(const char* path) noexcept
    {
        page.app.enqueue(Transaction{cmd::FileStream::find(name, path), page.ID, id()});
    }

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        switch (tag) {
        case static_cast<uint8_t>(attr::Id::Val):
            val.applyResponse(response);
            return;
        case static_cast<uint8_t>(attr::Id::Qty):
            qty.applyResponse(response);
            return;
        case static_cast<uint8_t>(attr::Id::En):
            en.applyResponse(response);
            return;
        default:
            break;
        }
        Component::onResponse(tag, response);
    }

    FileStream(Page& owner, const Literal& name, uint8_t id = 0)
        : Component(owner, name, Component::Type::FileStream, id)
        , val{*this, attr::Id::Val}
        , qty{*this, attr::Id::Qty}
        , en{*this, attr::Id::En}
    {}
};

/** path */
class ExternalPicture : public Drawable {
public:
    /** mcu: путь к файлу изображения на панели */
    attr::String<512> path;

    void onResponse(uint8_t tag, const msg::getString& response) override
    {
        if (tag == static_cast<uint8_t>(attr::Id::Path)) {
            path.applyResponse(response);
            return;
        }
        TouchArea::onResponse(tag, response);
    }

    ExternalPicture(Page& owner, const Literal& name, uint8_t id = 0)
        : Drawable(owner, name, Component::Type::ExternalPicture, id)
        , path{*this, attr::Id::Path}
    {}
};

class Gmov : public MediaComponent {
public:
    Gmov(Page& owner, const Literal& name, uint8_t id = 0)
        : MediaComponent(owner, name, Component::Type::Gmov, id) {}
};

class Video : public MediaComponent {
public:
    /** user: источник (RO) */
    attr::NumRO<uint8_t> from;

    void setPath(const char* path) noexcept
    {
        attr_detail::assignText(*this, attr::Id::Vid, path);
    }

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        if (tag == static_cast<uint8_t>(attr::Id::From)) {
            from.applyResponse(response);
            return;
        }
        MediaComponent::onResponse(tag, response);
    }

    Video(Page& owner, const Literal& name, uint8_t id = 0)
        : MediaComponent(owner, name, Component::Type::Video, id)
        , from{*this, attr::Id::From}
    {}
};

template<BG S = BG::Color>
class FileBrowser : public DataFile<S> {
public:
    void setFilter(const char* pattern) noexcept
    {
        attr_detail::assignText(*this, attr::Id::Filter, pattern);
    }

    void setLineSpacing(uint8_t v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Spay, v);
    }

    /** user: состояние панели (RO) */
    attr::NumRO<uint8_t> psta;

    void setIcon1(PicId v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Pic1, v);
    }

    void setIcon2(PicId v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Pic2, v);
    }

    void setItemSpacing(int16_t v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Vvs2, v);
    }

    /** user: размер буфера (RO) */
    attr::NumRO<uint32_t> buffsize;
    /** user: флаг предупреждения (RO) */
    attr::NumRO<uint8_t> fwarning;

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        switch (tag) {
        case static_cast<uint8_t>(attr::Id::Psta):
            psta.applyResponse(response);
            return;
        case static_cast<uint8_t>(attr::Id::Buffsize):
            buffsize.applyResponse(response);
            return;
        case static_cast<uint8_t>(attr::Id::Fwarning):
            fwarning.applyResponse(response);
            return;
        default:
            break;
        }
        DataFile<S>::onResponse(tag, response);
    }

    FileBrowser(Page& owner, const Literal& name, uint8_t id = 0)
        : DataFile<S>(owner, name, Component::Type::FileBrowser, id)
        , psta{*this, attr::Id::Psta}
        , buffsize{*this, attr::Id::Buffsize}
        , fwarning{*this, attr::Id::Fwarning}
    {}
};

} // namespace comp
} // namespace nex
