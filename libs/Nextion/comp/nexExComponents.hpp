#pragma once

#include "nexCompImpl.hpp"

/**
 * Расширенные виджеты (Audio, FileStream, DataRecord, Video, …): те же правила attr, что в `nexComponents.hpp`.
 */

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

    void onResponse(const msg::getNumeric& response, uint8_t tag) override
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
        Component::onResponse(response, tag);
    }

    Audio(IPage& owner, const Literal& name, uint8_t id = 0)
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

    void onResponse(const msg::getNumeric& response, uint8_t tag) override
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
        Component::onResponse(response, tag);
    }

    FileStream(IPage& owner, const Literal& name, uint8_t id = 0)
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

    void onResponse(const msg::getString& response, uint8_t tag) override
    {
        if (tag == static_cast<uint8_t>(attr::Id::Path)) {
            path.applyResponse(response);
            return;
        }
        TouchArea::onResponse(response, tag);
    }

    ExternalPicture(IPage& owner, const Literal& name, uint8_t id = 0)
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

    void onResponse(const msg::getNumeric& response, uint8_t tag) override
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
        TouchArea::onResponse(response, tag);
    }

protected:
    explicit MediaComponent(IPage& owner, const Literal& objectName, Component::Type componentType, uint8_t id = 0) noexcept
        : Drawable(owner, objectName, componentType, id)
        , stim{*this, attr::Id::Stim}
        , qty{*this, attr::Id::Qty}
    {}
};

class Gmov : public MediaComponent {
public:
    Gmov(IPage& owner, const Literal& name, uint8_t id = 0)
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

    void onResponse(const msg::getNumeric& response, uint8_t tag) override
    {
        if (tag == static_cast<uint8_t>(attr::Id::From)) {
            from.applyResponse(response);
            return;
        }
        MediaComponent::onResponse(response, tag);
    }

    Video(IPage& owner, const Literal& name, uint8_t id = 0)
        : MediaComponent(owner, name, Component::Type::Video, id)
        , from{*this, attr::Id::From}
    {}
};

/**
 * Общая база type 65/66 (`FileBrowser`, `DataRecord`): прокрутка, RO `txt`/`qty`, позиция.
 * `dir`, `val`, `dis`, `bco2`/`pco2` — разная семантика NIS; только в листьях.
 */
template<BG S = BG::Color>
class DataFile : public Printable<S> {
public:
    /** user: выбранная ячейка / имя файла (RO, NIS `txt`) */
    attr::String<256> txt;
    /** user: горизонтальное смещение прокрутки (NIS `left`) */
    attr::Num<Coord> left;
    /** user: индекс канала / столбца (NIS `ch`) */
    attr::Num<uint8_t> ch;
    /** user: число записей / элементов (RO, NIS `qty`) */
    attr::NumRO<uint16_t> qty;
    /** user: позиция прокрутки X (NIS `val_x`) */
    attr::Num<Coord> val_x;
    /** user: позиция прокрутки Y (NIS `val_y`) */
    attr::Num<Coord> val_y;

    /** Межсимвольный интервал в ячейке (NIS `spax`). */
    void setCharSpacing(uint16_t v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Spax, v);
    }

    void setMaxvalY(uint16_t v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::MaxvalY, v);
    }

    void setMaxvalX(uint16_t v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::MaxvalX, v);
    }

    void onResponse(const msg::getNumeric& response, uint8_t tag) override
    {
        switch (tag) {
        case static_cast<uint8_t>(attr::Id::Left):
            left.applyResponse(response);
            return;
        case static_cast<uint8_t>(attr::Id::Ch):
            ch.applyResponse(response);
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
        Printable<S>::onResponse(response, tag);
    }

    void onResponse(const msg::getString& response, uint8_t tag) override
    {
        if (tag == static_cast<uint8_t>(attr::Id::Txt)) {
            txt.applyResponse(response);
            return;
        }
        Styled<S>::onResponse(response, tag);
    }

protected:
    explicit DataFile(IPage& owner, const Literal& objectName, Component::Type componentType, uint8_t id = 0) noexcept
        : Printable<S>(owner, objectName, componentType, id)
        , txt{*this, attr::Id::Txt}
        , left{*this, attr::Id::Left}
        , ch{*this, attr::Id::Ch}
        , qty{*this, attr::Id::Qty}
        , val_x{*this, attr::Id::ValX}
        , val_y{*this, attr::Id::ValY}
    {}
};

/** type 66: таблица записей на SD (`path`, `insert`/`delete`/…). */
template<BG S = BG::Color>
class DataRecord : public DataFile<S> {
public:
    /** mcu: путь к файлу данных (NIS `path`) */
    attr::String<512> path;
    /** mcu: ширины колонок `w1^w2^…` (NIS `format`) */
    attr::String<256> format;
    /** mcu: заголовки колонок `h1^h2^…` (NIS `dir`) */
    attr::String<256> dir;
    /** user: длина файла, байт (RO, NIS `lenth`) */
    attr::NumRO<uint32_t> lenth;
    /** user: макс. число записей (RO, NIS `maxval`) */
    attr::NumRO<int32_t> maxval;
    /** user: выбранная строка (RO, NIS `val`) */
    attr::NumRO<int32_t> val;

    void setRecordLength(uint16_t v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Length, v);
    }

    void setColumnWidths(const char* pattern) noexcept
    {
        attr_detail::assignText(*this, attr::Id::Format, pattern);
    }

    void setColumnHeaders(const char* headers) noexcept
    {
        attr_detail::assignText(*this, attr::Id::Dir, headers);
    }

    /** Число полей в строке (NIS `mode`). */
    void setFieldCount(uint8_t v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Mode, v);
    }

    /** Порядок вставки: `0` — новые сверху (NIS `order`). */
    void setInsertOrder(uint8_t v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Order, v);
    }

    /** Интервал между колонками (NIS `dis`). */
    void setColumnSpacing(uint8_t v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Dis, v);
    }

    void setCellSize(uint8_t v) noexcept
    {
        static constexpr uint8_t kMin = 16u;
        static constexpr uint8_t kMax = 255u;
        attr_detail::assignNumeric(*this, attr::Id::Hig, clamp(v, kMin, kMax));
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

    void setAltCellBgColor(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Bco1, v);
    }

    void setAltCellColor(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Pco1, v);
    }

    void setSelCellBgColor(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Bco2, v);
    }

    void setSelCellColor(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Pco2, v);
    }

    void setHAlign(HAlign v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Xcen, v);
    }

    void onResponse(const msg::getNumeric& response, uint8_t tag) override
    {
        switch (tag) {
        case static_cast<uint8_t>(attr::Id::Lenth):
            lenth.applyResponse(response);
            return;
        case static_cast<uint8_t>(attr::Id::Maxval):
            maxval.applyResponse(response);
            return;
        case static_cast<uint8_t>(attr::Id::Val):
            val.applyResponse(response);
            return;
        default:
            break;
        }
        DataFile<S>::onResponse(response, tag);
    }

    void onResponse(const msg::getString& response, uint8_t tag) override
    {
        switch (tag) {
        case static_cast<uint8_t>(attr::Id::Path):
            path.applyResponse(response);
            return;
        case static_cast<uint8_t>(attr::Id::Format):
            format.applyResponse(response);
            return;
        case static_cast<uint8_t>(attr::Id::Dir):
            dir.applyResponse(response);
            return;
        default:
            break;
        }
        DataFile<S>::onResponse(response, tag);
    }

    DataRecord(IPage& owner, const Literal& name, uint8_t id = 0)
        : DataFile<S>(owner, name, Component::Type::DataRecord, id)
        , path{*this, attr::Id::Path}
        , format{*this, attr::Id::Format}
        , dir{*this, attr::Id::Dir}
        , lenth{*this, attr::Id::Lenth}
        , maxval{*this, attr::Id::Maxval}
        , val{*this, attr::Id::Val}
    {}
};

/** type 65: браузер файлов на SD; `dir` — папка, `txt` — выбранное имя (RO). */
template<BG S = BG::Color>
class FileBrowser : public DataFile<S> {
public:
    /** mcu: текущая папка (NIS `dir`) */
    attr::String<512> dir;
    /** mcu: маска файлов (NIS `filter`) */
    attr::String<256> filter;
    /** user: выбранный элемент (NIS `val`) */
    attr::Num<int32_t> val;
    /** user: состояние панели (RO, NIS `psta`) */
    attr::NumRO<uint8_t> psta;
    /** user: размер буфера (RO, NIS `buffsize`) */
    attr::NumRO<uint32_t> buffsize;
    /** user: флаг предупреждения (RO, NIS `fwarning`) */
    attr::NumRO<uint8_t> fwarning;

    void setFolderPath(const char* folder) noexcept
    {
        attr_detail::assignText(*this, attr::Id::Dir, folder);
    }

    void setFilter(const char* pattern) noexcept
    {
        attr_detail::assignText(*this, attr::Id::Filter, pattern);
    }

    void setLineSpacing(uint8_t v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Spay, v);
    }

    static constexpr uint8_t kScrollStepMin = 2u;
    static constexpr uint8_t kScrollStepMax = 50u;

    /** Шаг прокрутки списка (NIS `dis`). */
    void setScrollStep(uint8_t v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Dis, clamp(v, kScrollStepMin, kScrollStepMax));
    }

    void setSelBgColor(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Bco2, v);
    }

    void setSelColor(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Pco2, v);
    }

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

    void onResponse(const msg::getNumeric& response, uint8_t tag) override
    {
        switch (tag) {
        case static_cast<uint8_t>(attr::Id::Val):
            val.applyResponse(response);
            return;
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
        DataFile<S>::onResponse(response, tag);
    }

    void onResponse(const msg::getString& response, uint8_t tag) override
    {
        switch (tag) {
        case static_cast<uint8_t>(attr::Id::Dir):
            dir.applyResponse(response);
            return;
        case static_cast<uint8_t>(attr::Id::Filter):
            filter.applyResponse(response);
            return;
        default:
            break;
        }
        DataFile<S>::onResponse(response, tag);
    }

    FileBrowser(IPage& owner, const Literal& name, uint8_t id = 0)
        : DataFile<S>(owner, name, Component::Type::FileBrowser, id)
        , dir{*this, attr::Id::Dir}
        , filter{*this, attr::Id::Filter}
        , val{*this, attr::Id::Val}
        , psta{*this, attr::Id::Psta}
        , buffsize{*this, attr::Id::Buffsize}
        , fwarning{*this, attr::Id::Fwarning}
    {}
};

} // namespace comp
} // namespace nex
