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

/** vid; en; loop; dis; tim; stim(RO); qty(RO) — Gmov, Video. */
class MediaComponent : public Drawable {
public:
    void setVideoId(uint32_t v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Vid, v);
    }

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

    void setStep(uint16_t v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Dis, v);
    }

    void setPeriod(uint32_t v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Tim, v);
    }

    /** user: позиция воспроизведения (RO) */
    attr::NumRO<uint32_t> stim;
    /** user: длительность / количество кадров (RO) */
    attr::NumRO<uint32_t> qty;

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        switch (tag) {
        case static_cast<uint8_t>(attr::Id::Stim):
            stim.applyResponse(response);
            return;
        case static_cast<uint8_t>(attr::Id::Qty):
            qty.applyResponse(response);
            return;
        default:
            break;
        }
        TouchArea::onResponse(tag, response);
    }

protected:
    explicit MediaComponent(Page& owner, const Literal& objectName, Component::Type componentType, uint8_t id = 0) noexcept
        : Drawable(owner, objectName, componentType, id)
        , stim{*this, attr::Id::Stim}
        , qty{*this, attr::Id::Qty}
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

/**
 * DataFile — поля таблицы/файлов (DataRecord, FileBrowser);
 * `txt`, `qty` — RO в NIS у FileBrowser / DataRecord.
 */
template<BG S = BG::Color>
class DataFile : public Printable<S> {
public:
    /** user: буфер/подпись ячейки (навигация по таблице) */
    attr::String<256> txt;
    /** user: смещение по горизонтали при прокрутке */
    attr::Num<Coord> left;
    /** user: индекс канала/строки */
    attr::Num<uint8_t> ch;
    /** user: направление прокрутки */
    attr::Num<uint8_t> dir;
    /** user: значение ячейки / выбор */
    attr::Num<int32_t> val;
    /** user: количество (RO, отражает состояние после действий на панели) */
    attr::NumRO<uint16_t> qty;
    //TODO проверить реальное назначение атрибута dis
    void setCellSpacing(uint16_t v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Dis, v);
    }

    void setMaxvalY(uint16_t v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::MaxvalY, v);
    }

    void setMaxvalX(uint16_t v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::MaxvalX, v);
    }

    /** user: позиция/значение по X */
    attr::Num<Coord> val_x;
    /** user: позиция/значение по Y */
    attr::Num<Coord> val_y;

    //TODO посмотреть реальное назначение атрибутов bco2 и pco2
    void setBco2(uint16_t v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Bco2, v);
    }

    void setPco2(uint16_t v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Pco2, v);
    }

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        switch (tag) {
        case static_cast<uint8_t>(attr::Id::Left):
            left.applyResponse(response);
            return;
        case static_cast<uint8_t>(attr::Id::Ch):
            ch.applyResponse(response);
            return;
        case static_cast<uint8_t>(attr::Id::Dir):
            dir.applyResponse(response);
            return;
        case static_cast<uint8_t>(attr::Id::Val):
            val.applyResponse(response);
            return;
        case static_cast<uint8_t>(attr::Id::Qty):
            qty.applyResponse(response);
            return;
        case static_cast<uint8_t>(attr::Id::ValX):
            val_x.applyResponse(response);
            return;
        case static_cast<uint8_t>(attr::Id::ValY):
            val_y.applyResponse(response);
            return;
        default:
            break;
        }
        Printable<S>::onResponse(tag, response);
    }

    void onResponse(uint8_t tag, const msg::getString& response) override
    {
        if (tag == static_cast<uint8_t>(attr::Id::Txt)) {
            txt.applyResponse(response);
            return;
        }
        Styled<S>::onResponse(tag, response);
    }

protected:
    explicit DataFile(Page& owner, const Literal& objectName, Component::Type componentType, uint8_t id = 0) noexcept
        : Printable<S>(owner, objectName, componentType, id)
        , txt{*this, attr::Id::Txt}
        , left{*this, attr::Id::Left}
        , ch{*this, attr::Id::Ch}
        , dir{*this, attr::Id::Dir}
        , val{*this, attr::Id::Val}
        , qty{*this, attr::Id::Qty}
        , val_x{*this, attr::Id::ValX}
        , val_y{*this, attr::Id::ValY}
    {}
};

template<BG S = BG::Color>
class DataRecord : public DataFile<S> {
public:
    /** mcu: путь к файлу данных */
    attr::String<512> path;
    /** user: длина файла (RO) */
    attr::NumRO<uint32_t> lenth;
    /** user: максимальное значение (RO) */
    attr::NumRO<int32_t> maxval;

    void setRecordLength(uint16_t v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Length, v);
    }

    void setFormat(NumFormat v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Format, v);
    }

    void setMode(uint8_t v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Mode, v);
    }

    void setOrder(uint8_t v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Order, v);
    }

    void setCellSize(uint8_t v) noexcept
    {
        static constexpr uint8_t kMin = 16u;
        static constexpr uint8_t kMax = 255u;
        attr_detail::assignNumeric(*this, attr::Id::Hig,
            clamp(v, kMin, kMax));
    }

    void setGridColor(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Gdc, v);
    }

    void setGridWidth(uint8_t v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Gdw, v);
    }

    void setGridHeight(uint8_t v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Gdh, v);
    }

    void setCellBgColor(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Bco1, v);
    }

    void setCellColor(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Pco1, v);
    }

    void setHAlign(HAlign v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Xcen, v);
    }

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        switch (tag) {
        case static_cast<uint8_t>(attr::Id::Lenth):
            lenth.applyResponse(response);
            return;
        case static_cast<uint8_t>(attr::Id::Maxval):
            maxval.applyResponse(response);
            return;
        default:
            break;
        }
        DataFile<S>::onResponse(tag, response);
    }

    void onResponse(uint8_t tag, const msg::getString& response) override
    {
        if (tag == static_cast<uint8_t>(attr::Id::Path)) {
            path.applyResponse(response);
            return;
        }
        DataFile<S>::onResponse(tag, response);
    }

    DataRecord(Page& owner, const Literal& name, uint8_t id = 0)
        : DataFile<S>(owner, name, Component::Type::DataRecord, id)
        , path{*this, attr::Id::Path}
        , lenth{*this, attr::Id::Lenth}
        , maxval{*this, attr::Id::Maxval}
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
