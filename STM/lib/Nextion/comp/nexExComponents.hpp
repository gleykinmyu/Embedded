#pragma once

#include "nexCompImpl.hpp"

namespace nex {
namespace comp {

/** from(RO); vid; en; loop; tim; stim(RO) */
class Audio : public Component {
public:
    enum Tag : uint8_t {
        From = 192u,
        Vid,
        En,
        Loop,
        Tim,
        Stim,
    };

    /** user: источник (RO) */
    attr::NumRO<uint8_t> from;
    /** mcu: путь к ресурсу (при external) */
    attr::String<512> vid;

    void enable() noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"en"}, Tag::En, true);
    }

    void disable() noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"en"}, Tag::En, false);
    }

    void setLoop(bool on) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"loop"}, Tag::Loop, on);
    }

    void setPeriod(uint32_t v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"tim"}, Tag::Tim, v);
    }

    /** user: позиция воспроизведения (RO) */
    attr::NumRO<uint32_t> stim;

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        switch (tag) {
        case Tag::From:
            from.applyResponse(response);
            return;
        case Tag::Stim:
            stim.applyResponse(response);
            return;
        default:
            break;
        }
        Component::onResponse(tag, response);
    }

    Audio(Page& owner, const Literal& name, uint8_t id = 0)
        : Component(owner, name, Component::Type::Audio, id)
        , from{*this, "from", Tag::From}
        , vid{*this, "vid", Tag::Vid}
        , stim{*this, "stim", Tag::Stim}
    {}
};

/** val; qty(RO); en(RO); open/read/write/close/find */
class FileStream : public Component {
public:
    enum Tag : uint8_t {
        Val = 192u,
        Qty,
        En,
    };

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
        case Tag::Val:
            val.applyResponse(response);
            return;
        case Tag::Qty:
            qty.applyResponse(response);
            return;
        case Tag::En:
            en.applyResponse(response);
            return;
        default:
            break;
        }
        Component::onResponse(tag, response);
    }

    FileStream(Page& owner, const Literal& name, uint8_t id = 0)
        : Component(owner, name, Component::Type::FileStream, id)
        , val{*this, "val", Tag::Val}
        , qty{*this, "qty", Tag::Qty}
        , en{*this, "en", Tag::En}
    {}
};

/** path */
class ExternalPicture : public Drawable {
public:
    enum Tag : uint8_t {
        Path = 192u,
    };

    /** mcu: путь к файлу изображения на панели */
    attr::String<512> path;

    void onResponse(uint8_t tag, const msg::getString& response) override
    {
        if (tag == Tag::Path) {
            path.applyResponse(response);
            return;
        }
        TouchArea::onResponse(tag, response);
    }

    ExternalPicture(Page& owner, const Literal& name, uint8_t id = 0)
        : Drawable(owner, name, Component::Type::ExternalPicture, id)
        , path{*this, "path", Tag::Path}
    {}
};

class Gmov : public MediaComponent {
public:
    Gmov(Page& owner, const Literal& name, uint8_t id = 0)
        : MediaComponent(owner, name, Component::Type::Gmov, id) {}
};

class Video : public MediaComponent {
public:
    enum Tag : uint8_t {
        From = 32u,
    };

    /** user: источник (RO) */
    attr::NumRO<uint8_t> from;

    void setPath(const char* path) noexcept
    {
        attr_detail::assignText(*this, Literal{"vid"}, MediaComponent::Tag::Vid, path);
    }

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        if (tag == Tag::From) {
            from.applyResponse(response);
            return;
        }
        MediaComponent::onResponse(tag, response);
    }

    Video(Page& owner, const Literal& name, uint8_t id = 0)
        : MediaComponent(owner, name, Component::Type::Video, id)
        , from{*this, "from", Tag::From}
    {}
};

template<BGStyle S = BGStyle::Color>
class FileBrowser : public DataFile<S> {
public:
    enum Tag : uint8_t {
        Filter = 80u,
        Spay,
        Psta,
        Pic1,
        Pic2,
        Vvs2,
        Buffsize,
        Fwarning,
    };

    void setFilter(const char* pattern) noexcept
    {
        attr_detail::assignText(*this, Literal{"filter"}, Tag::Filter, pattern);
    }

    void setLineSpacing(uint8_t v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"spay"}, Tag::Spay, v);
    }

    /** user: состояние панели (RO) */
    attr::NumRO<uint8_t> psta;

    void setIcon1(PicId v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"pic1"}, Tag::Pic1, v);
    }

    void setIcon2(PicId v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"pic2"}, Tag::Pic2, v);
    }

    void setItemSpacing(int16_t v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"vvs2"}, Tag::Vvs2, v);
    }

    /** user: размер буфера (RO) */
    attr::NumRO<uint32_t> buffsize;
    /** user: флаг предупреждения (RO) */
    attr::NumRO<uint8_t> fwarning;

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        switch (tag) {
        case Tag::Psta:
            psta.applyResponse(response);
            return;
        case Tag::Buffsize:
            buffsize.applyResponse(response);
            return;
        case Tag::Fwarning:
            fwarning.applyResponse(response);
            return;
        default:
            break;
        }
        DataFile<S>::onResponse(tag, response);
    }

    FileBrowser(Page& owner, const Literal& name, uint8_t id = 0)
        : DataFile<S>(owner, name, Component::Type::FileBrowser, id)
        , psta{*this, "psta", Tag::Psta}
        , buffsize{*this, "buffsize", Tag::Buffsize}
        , fwarning{*this, "fwarning", Tag::Fwarning}
    {}
};

} // namespace comp
} // namespace nex
