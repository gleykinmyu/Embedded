#pragma once
#include <cstdint>
#include "../../Interfaces/obj_registry.hpp"
#include "../core/nexMessages.hpp"
#include "../core/nexTypes.hpp"

namespace nex {

    class Application;
    class SmartApp;
    class Component;

    /** Пустое имя страницы в HMI, если достаточно только `PageBase::ID`. */
    inline constexpr Literal kUnnamedPageLexeme{""};

    /**
     * Базовая страница без шаблона: `Application` хранит `PageBase*`.
     * Касание: `comp_id == kPageCompId` — `onTouchPage`; иначе `getComponent` → `Component::onTouch`.
     * Конкретная страница с таблицей виджетов — `PageImpl<MaxWidgets>` (число виджетов, panel id 1…N).
     */
    class Page {
    public:
        const Literal name;
        const uint8_t ID;
        /** Родительское приложение (регистрация, маршрутизация, команды). */
        Application& app;

        /** Регистрирует страницу в `application` по полю `ID`. */
        Page(Application& application, const Literal& pageObjName, uint8_t id) noexcept;
        virtual ~Page() = default;

        /** Касание по этой странице: фильтр `page_id`, затем `onTouchPage` при `comp_id == 0`, иначе виджет. */
        virtual void onTouch(const msg::evTouch& e);

        /** Touch с `comp_id == kPageCompId` — касание по странице (NIS). */
        virtual void onTouchPage(const msg::evTouch& e) { (void)e; }

        /** Панель перешла на эту страницу: `msg::evPage`, новый `page_id` отличается от предыдущего в `Application`. */
        virtual void onLoad() {}
        /** Панель уходит с этой страницы на другую (`page_id` меняется); до обновления `Application::currentPageId()`. */
        virtual void onExit() {}
        /** `status == Code::AppError`; далее — `Component::onError` по `comp_id`. */
        virtual void onError(const msg::Status& status, uint8_t comp_id);

        /** Ответ MCU `MsgBox`, маршрутизированный на эту страницу (`comp_id == 0`). */
        virtual void onMsgBox(const msg::evMsgBox& e) { (void)e; }

        Component* getComponent(uint8_t comp_id) noexcept;
        uint8_t compCount() const noexcept;

    private:
        friend class MISC::ObjRegistry<Page, uint8_t>;        
        friend class SmartApp;
        friend class Component;

        virtual MISC::ObjRegistry<Component, uint8_t>& registry() noexcept = 0;

        MISC::ObjRegistry<Component, uint8_t>& registry() const noexcept {
            return const_cast<Page*>(this)->registry();
        }

        /** `ID` фиксирован при создании; вызывается только из `ObjRegistry`. */
        void set_id(uint8_t) noexcept {}
        void registerComponent(Component& c) noexcept;
    };

    /**
     * Объект компонента на странице Nextion (MCU: `name` ↔ objname, `type` ↔ атрибут type, `id()` ↔ id).
     * Внутри хранится копия-представление `nex::Literal` (указатель на литерал + длина); буфер символов — как у исходного литерала.
     *
     * Привязка к `PageBase`: компонент регистрируется на странице (`page.ID`, `id()`).
     *
     * В `nexWidgets.hpp` у наследников перечислены только атрибуты, характерные для данного типа виджета.
     */
    class Component {
    public:
        /** Виды компонентов; числовые коды — как у одноимённого атрибута в NIS. */
        enum class Type : uint8_t {
            Waveform        = 0, //type, id, objname, vscope, drag, aph, effect, sta (cropimage, color, image, transparent), ch, bco(pic, picc), gdc, gdw, gdh, pco0, pco1, pco2, pco3, dis, x, y, w, h
            Slider          = 1, //type, id, objname, vscope, drag, aph, effect, sta (cropimage, color, image), wid, hig, bco(pic, picc), bco1(pic1, picc1), pco, val, maxval, minval, ch, x, y, w, h
            Gmov            = 2, //type, id, objname, vscope, drag, aph, effect, vid, en, loop, dis, tim, stim(RO), qty(RO), x, y, w, h
            Video           = 3, //type, id, objname, vscope, drag, aph, effect, from(RO: internal, external), vid(path, when from=external), en, loop, dis, tim, stim(RO), qty(RO), x, y, w, h
            Audio           = 4, //type, id, objname, vscope, from(RO: internal, external), vid(path, when from=external), en, loop, tim, stim(RO)
            TouchCap        = 5, //type, id, objname, vscope, val(RO)
            Timer           = 51, //type, id, objname, vscope, tim, en
            Variable        = 52, // NIS `va`; sta=Number → NumericVariable (val); sta=String → StringVariable (txt, txt_maxl); один code type в панели, два класса в nexWidgets.hpp
            DualStateButton = 53, //type, id, objname, vscope, drag, aph, effect, sta (cropimage, color, image, transparent), font, bco (pic, picc), bco2(pic2, picc2), pco, pco2, xcen, ycen, val, txt, txt_maxl, isbr, spax, spay, x, y, w, h
            Number          = 54, //type, id, objname, vscope, drag, aph, effect, sta (cropimage, color, image, transparent), key, font, bco (pic, picc), pco, xcen, ycen, val, length, format, isbr, spax, spay, x, y, w, h
            ScrollText      = 55, //type, id, objname, vscope, drag, aph, effect, sta (cropimage, color, image, transparent), key, font, bco (pic, picc), pco, xcen, ycen, dir, dis, tim, en, txt, txt_maxl, isbr, spax, spay, x, y, w, h
            Checkbox        = 56, //type, id, objname, vscope, drag, aph, effect, bco, pco, val, x, y, w, h
            Radio           = 57, //type, id, objname, vscope, drag, aph, effect, bco, pco, val, x, y, w, h
            QRCode          = 58, //type, id, objname, vscope, drag, aph, effect, dis, bco, pco, txt, txt_maxl, x, y, w, h
            XFloat          = 59, //type, id, objname, vscope, drag, aph, effect, sta (cropimage, color, image, transparent), key, font, bco (pic, picc), pco, xcen, ycen, val, vvs0, vvs1, isbr, spax, spay, x, y, w, h
            ExternalPicture = 60, //type, id, objname, vscope, drag, aph, effect, path, x, y, w, h
            ComboBox        = 61, //type, id, objname, vscope, drag, aph, effect, sta (cropimage, color, image, transparent), font, bco, pco, xcen, ycen, spax, dis, txt, txt_maxl, up, pco3(color of arrow), bco1(cell bg color), pco1(font color), path, path_m(RO), dir, qty, vvs0, val, bco2(select bg color), pco2(select font color), hig, down, mode, wid, vvs1, ch, x, y, w, h
            SlidingText     = 62, //type, id, objname, vscope, drag, aph, effect, sta (cropimage, color, image, transparent), font, bco, pco, xcen, left, ch, txt, txt_maxl, isbr, spax, spay, maxval_y, val_y, x, y, w, h
            FileStream      = 63, //type, id, objname, vscope, val, qty(RO), en(RO), method: open, read, write, close, find
            FileBrowser     = 65, //type, id, objname, vscope, drag, aph, effect, sta(cropimage, color, image, transparent), font, bco, pco, pco2, bco2, left, ch, dir, filter, val, txt(RO), qty(RO), dis, spax, spay, maxval_y, maxval_x, val_x, val_y, psta(RO), pic1, pic2, vvs2, buffsize(RO), fwarning(RO), x, y, w, h
            DataRecord      = 66, //type, id, objname, vscope, drag, aph, effect, sta(cropimage, color, image, transparent), font, bco (pic, picc), pco, xcen, path, lenth(RO), maxval(RO), val(RO), length, format, dir, mode, dis, order, qty(RO), spax, hig, left, gdc, gdw, gdh, bco1, pco1, bco2, pco2, val, txt(RO), ch, maxval_y, val_y, maxval_x, val_x,  y, w, h 
            ToggleSwitch    = 67, //type, id, objname, vscope, drag, aph, effect, val, bco, pco, bco2, pco2, pco1(font color), font, dis, txt (txt_maxl = 24), x, y, w, h  
            TextSelect      = 68, //type, id, objname, vscope, drag, aph, effect, sta (cropimage, color, image, transparent), font, bco (pic, picc), pco(font color), pco2(selected font color), spax, hig, dis, pco1(line color), txt(RO), val, ch, path, path_m(RO), x, y, w, h
            Button          = 98, //type, id, objname, vscope, drag, aph, effect, sta (cropimage, color, image, transparent), font, bco (pic, picc), bco2(pic2, picc2), pco(font color), pco2(pressed font color), xcen, ycen, txt, txt_maxl, isbr, spax, spay, x, y, w, h
            ProgressBar     = 106, //type, id, objname, vscope, drag, aph, effect, sta (color, image), val, dis (only when sta=color), bco (bpic), pco (ppic), x, y, w, h
            Hotspot         = 109, //type, id, objname, vscope, pos(x,y), w, h
            Picture         = 112, //type, id, objname, vscope, drag, aph, effect, pic, x, y, w, h
            CropPicture     = 113, //type, id, objname, vscope, drag, aph, effect, picc, x, y, w, h
            Text            = 116, //type, id, objname, vscope, drag, aph, effect, sta (cropimage, color, image, transparent), key, font, bco (pic, picc), pco, xcen, ycen, pw, txt, txt_maxl, isbr,spax, spay, x, y, w, h
            PageComponent   = 121, //type, id, vscope, effect, up, down, left, right, sta(no background, color, image), bco(pic) - when sta != no background, x, y, w, h
            Gauge           = 122, //type, id, objname, vscope, drag, aph, effect, sta(cropimage, color, image, transparent), bco(picc, pic) - when sta != transparent, val, format, up, down, left, pco, pco2, hig, vvs0, vvs1, vvs2, x, y, w, h
        };

        Page& page;
        const Literal name;
        /** Вид этого экземпляра (тот же код, что атрибут `type` объекта на панели). */
        const Type type;
        uint8_t id() const noexcept { return id_; }

        /**
         * `@p id` — panel id (`≥ kFirstCompId`) или `0` = автослот при регистрации
         * (не panel id; на панели у виджетов id начинается с 1).
         */
        Component(Page& owner, const Literal& compName, Type compType, uint8_t id = 0) noexcept;

        virtual void onTouch(const msg::evTouch& e);
        
        /** Ответ `get` (0x71 / 0x70); `tag` — атрибут, заданный при запросе. */
        virtual void onResponse(uint8_t tag, const msg::getNumeric& response);
        virtual void onResponse(uint8_t tag, const msg::getString& response);

        /** Ответ **status** с ошибкой выполнения команды этого компонента. */
        virtual void onError(const msg::Status& response);

        /** Ответ MCU `MsgBox`, маршрутизированный на этот компонент. */
        virtual void onMsgBox(const msg::evMsgBox& e) { (void)e; }

    private:
        friend class MISC::ObjRegistry<Component, uint8_t>;

        void set_id(uint8_t id) noexcept { id_ = id; }

        uint8_t id_;
    };

    /** Сообщает `ComponentRegisterFailed` с кодом `MISC::RegStatus`. */
    void nexComponentRegisterFailed(Application& app, Page& page, const Component& c, MISC::RegStatus st) noexcept;

    /**
     * Страница с `ObjStorage<Component, MaxWidgets, uint8_t, kFirstCompId>` — только виджеты (panel id 1…N).
     * `kPageCompId` в реестре не хранится; `getComponent(0)` всегда `nullptr`.
     */
    template <uint8_t MaxWidgets>
    class PageImpl : public Page {
        static_assert(MaxWidgets < 255u, "PageImpl: MaxWidgets must be < 255");
        static constexpr size_t kRegistryCapacity = (MaxWidgets > 0u) ? static_cast<size_t>(MaxWidgets) : 1u;

    public:
        using Page::Page;

    protected:
        MISC::ObjRegistry<Component, uint8_t>& registry() noexcept override { return _components; }

    private:
        MISC::ObjStorage<Component, kRegistryCapacity, uint8_t, kFirstCompId> _components;
    };

    /*
     * ---------------------------------------------------------------------------
     * Дерево наследования C++ (соответствует `nexCompImpl.hpp` + листья в `nexComponents.hpp` / `nexExComponents.hpp`).
     * `Page`, `Component` — `namespace nex`; базы и листья виджетов — `namespace nex::comp`.
     * У узла — только дельта к родителю. Комментарии у `enum Type` выше — эталон атрибутов NIS.
     *
     *   PageComponent (121)                    // нет C++-листа
     *   Component                              // type(RO), id(RO), vscope(RO); objname(RO) где есть
     *   │
     *   ├── Timer                              // setPeriod; enable/disable
     *   ├── NumericVar                         // val (`type`=52, sta=Number)
     *   ├── StringVar<TxtMaxL>                 // txt (`type`=52, sta=String)
     *   ├── Audio                              // nexExComponents: from(RO); vid; en; loop; tim; stim(RO)
     *   ├── TouchCap (5)                       // нет C++-листа
     *   ├── FileStream                         // nexExComponents: val; qty(RO); en(RO); open/read/write/close/find
     *   │
     *   └── TouchArea                          // x, y, w, h; touchSwitch; click
     *       ├── Hotspot
     *       │
     *       └── Drawable                         // drag, aph, effect; refresh; show/hide; placeAbove; move
     *           ├── ExternalPicture            // nexExComponents: path
     *           ├── MediaComponent             // nexExComponents: setVideoId; en; loop; dis; tim; stim(RO); qty(RO)
     *           │   ├── Gmov
     *           │   └── Video                  // from(RO); vid
     *           │
     *           ├── Waveform<S, ChN>           // bg (WfBackground); ch[].setColor/add/addt; setDataScale
     *           ├── ProgressBar<S>             // value; bg (PbBackground); bar (PbBar)  [S = Color | Image]
     *           ├── Picture                    // pic (Drawable, не Styled)
     *           ├── CropPicture                // picc (Drawable, не Styled)
     *           │
     *           └── Styled<S>                  // bg (Background<S>) — compile-time sta
     *               ├── QRCode                 // setPenColor; setDataSpacing; setText
     *               ├── Gauge                  // setAngle; center; pointer
     *               ├── Slider<S>              // value; cursor; bg; bg2
     *               │
     *               ├── Printable<S>           // font (FontColor → FontId → Font)
     *               │   ├── DataFile           // nexExComponents: txt; left; ch; dir; val; qty(RO); setCellSpacing; …
     *               │   │   ├── DataRecord     // nexExComponents: path; lenth(RO); maxval(RO); setRecordLength; …
     *               │   │   └── FileBrowser    // nexExComponents: filter; spay; pic1; pic2; …
     *               │   ├── ListSelect         // path; val; setCellSize
     *               │   │   ├── ComboBox       // border; arrow; cells; isOpened; setVAlign; setHAlign
     *               │   │   └── TextSelect     // setSelColor; setLineColor; setSelectionLine; path/val (без txt mirror)
     *               │   │
     *               │   └── Multiline          // setLineSpacing; setWordWrap; setHAlign
     *               │       ├── Textual        // setText
     *               │       │   ├── SlidingText // txt; setShowProgressBar; val_y; setVAlign=delete; (ch/maxval_y — не в API)
     *               │       │   ├── Text       // setVAlign; txt; enablePassword/disablePassword
     *               │       │   ├── ScrollText // setVAlign; txt; setScrollDirection; setScrollStep; setPeriod; enable/disable
     *               │       │   └── ButtonBase  // setVAlign; pressed (Pressed<S>)
     *               │       │       ├── Button
     *               │       │       └── DualStateButton  // val
     *               │       └── Numeric        // setVAlign; val
     *               │           ├── Number     // setDigitCount; setFormat
     *               │           └── XFloat     // setFormat(before, after)
     *               │
     *               └── Selection              // setMarkerColor; val; bg
     *                   ├── Checkbox
     *                   ├── Radio
     *                   └── ToggleSwitch       // pressed (PressedMarker); font (FontId<1>); setLabelGap; txt
     *
     * Пояснения:
     *
     * - Без геометрии (прямые потомки Component): Timer, NumericVar, StringVar, Audio, FileStream.
     * - Waveform и ProgressBar — Drawable, не Styled: фон через `resources::WfBackground` / `PbBackground`, не `Background<S>`.
     * - Шрифт: `Printable::font` — `resources::Font`; у ToggleSwitch подпись — `FontId<1>` (pco1); нажатое — `Pressed` / `PressedMarker`.
     * - `pco` / `pco1` / `pco2` в NIS — разные роли; не смешивать с `pco0…3` каналов Waveform.
     * - ComboBox vs TextSelect: общая база ListSelect (path, val, hig); дельта — раскрывающийся список vs подсветка строки.
     * - Листья без C++: TouchCap (5), PageComponent (121). Остальное — `nexComponents.hpp` + `nexExComponents.hpp`.
     *
     * resources (поля внутри виджетов, не узлы наследования):
     *   Background<S,i>  Pressed<S>  PressedMarker  FontColor/FontId/Font
     *   ComboBorder/Arrow/Cells  PbBackground/PbBar  WfBackground/WaveformChannels
     *   GaugeCenter/Pointer  Cursor
     */

} // namespace nex
