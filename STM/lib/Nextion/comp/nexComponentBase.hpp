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
            SLText          = 62, //type, id, objname, vscope, drag, aph, effect, sta (cropimage, color, image, transparent), font, bco, pco, xcen, left, ch, txt, txt_maxl, isbr, spax, spay, maxval_y, val_y, x, y, w, h
            FileStream      = 63, //type, id, objname, vscope, val, qty(RO), en(RO), method: open, read, write, close, find
            FileBrowser     = 65, //type, id, objname, vscope, drag, aph, effect, sta(cropimage, color, image, transparent), font, bco, pco, pco2, bco2, left, ch, dir, filter, val, txt(RO), qty(RO), dis, spax, spay, maxval_y, maxval_x, val_x, val_y, psta(RO), pic1, pic2, vvs2, buffsize(RO), fwarning(RO), x, y, w, h
            DataRecord      = 66, //type, id, objname, vscope, drag, aph, effect, sta(cropimage, color, image, transparent), font, bco (pic, picc), pco, xcen, path, lenth(RO), maxval(RO), val(RO), length, format, dir, mode, dis, order, qty(RO), spax, hig, left, gdc, gdw, gdh, bco1, pco1, bco2, pco2, val, txt(RO), ch, maxval_y, val_y, maxval_x, val_x,  y, w, h 
            ToggleSwitch    = 67, //type, id, objname, vscope, drag, aph, effect, val, bco, pco, bco2, pco2, pco1(font color), font, dis, txt (txt_maxl = 24), x, y, w, h  
            TextSelect      = 68, //type, id, objname, vscope, drag, aph, effect, sta (cropimage, color, image, transparent), font, bco (pic, picc), pco(font color), pco2(selected font color), spax, hig, dis, pco1(line color), txt(RO), val, ch, path, path_m(RO), x, y, w, h
            Button          = 98, //type, id, objname, vscope, drag, aph, effect, sta (cropimage, color, image, transparent), font, bco (pic, picc), bco2(pic2, picc2), pco(font color), pco2(pressed font color), xcen, ycen, txt, txt_maxl, isbr, spax, spay, x, y, w, h
            ProgressBar     = 106, //type, id, objname, vscope, drag, aph, effect, sta (color, image), val, dis (only when sta=color), bco (bpic), pco (ppic), x, y, w, h
            Hotspot         = 109, //type, id, objname, vscope, pos(x,y), w, h
            Picture         = 112, //type, id, objname, vscope, drag, aph, effect, pic, x, y, w, h
            CropPicture     = 113, //type, id, objname, vscope, drag, aph, effect, cpic, x, y, w, h
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
     * Дерево наследования C++ (план по enum Type — строки комментариев у значений enum = эталон атрибутов NIS).
     * Листья и impl-базы — `namespace nex::comp`; `Page` и `Component` — `namespace nex`.
     * Узел: только дельта к непосредственному родителю (без повторения type, id, vscope, objname и полей предков).
     * RO — как в enum; у PageComponent в редакторе нет objname.
     *
     * Ветки без font в enum вынесены отдельно от ветки с font:
     *   StaBcoComponent — в базе sta (режимы по типу), bco; без font. Атрибут pco как «цвет шрифта» (глифы) не на этом узле.
     *   FontedStaComponent — дельта + font; здесь же единый базовый `pco` (цвет шрифта NIS). У потомков FontedSta в дельтах
     *   `pco` не повторяем; оставляем только отличные от базового имена: pco1, pco2, pco3, … (отдельные атрибуты в панели).
     *   Под StaBco (без FontedSta): QR, Waveform, Gauge — в enum нет font; у листьев свои дельты (в т.ч. pco0…3, pco(ppic) — не смешивать с pco шрифта).
     *   Selection — у Checkbox/Radio в enum нет sta и font; у ToggleSwitch есть font (см. enum), sta нет.
     *
     *   PageComponent                          // effect; up; down; left; right; sta; bco(pic при sta≠no background)
     *   Component                              // type(RO), id(RO), vscope(RO); objname(RO) — если есть у типа
     *   │
     *   ├── Timer                              // setPeriod; enable/disable
     *   ├── NumericVariable                    // val (sta=Number в HMI)
     *   ├── StringVariable                     // txt, txt_maxl (sta=String в HMI)
     *   ├── Audio                              // from(RO); vid; en; loop; tim; stim(RO)
     *   ├── TouchCap                           // val(RO)
     *   ├── FileStream                         // val; qty(RO); en(RO); open, read, write, close, find
     *   │
     *   └── TouchArea                          // pos (x,y), w, h — компоненты с прямоугольником на странице
     *       ├── Hotspot                        // —
     *       │
     *       └── Drawable                         // drag, aph, effect
     *           ├── ExternalPicture            // path
     *           ├── MediaComponent             // vid; en; loop; dis; tim; stim(RO); qty(RO)
     *           │   ├── Gmov                   // —
     *           │   └── Video                  // from(RO); vid(path) при from=external
     *           │
     *           └── Styled<STYLE = CROP, COLOR, IMAGE, TRANSPARENT>  //templated structure bg: bg.color / bg.image / bg.cropimage / bg
     *               │
     *               ├── QRCode<COLOR>          // pco; dis; txt; txt_maxl
     *               ├── Picture<IMAGE>
     *               ├── CropPicture<CROP_IMAGE>
     *               │
     *               ├── Waveform                         // gdc; gdw; gdh; pco0…pco3; dis; wid; hig; ch
     *               ├── Gauge                            // val; format; up; down; left; pco; pco2; hig; vvs0…vvs2
     *               │
     *               ├── Linear                         // value
     *               │   ├── ProgressBar<Color>           // barColor; cornerRadius
     *               │   ├── ProgressBar<Image>           // value; bg.bpic; ppic
     *               │   └── Slider<CROP, COLOR, IMAGE> // cursor; bg2; value (Linear)
     *               │
     *               ├── Printable                   // font.setFontId; font.setTextColor; font.setCharSpacing
     *               │   ├── DataFile // txt; txt_maxl; left; ch; dir; val; txt(RO); qty(RO); dis;
     *               │   │   │                       // maxval_y; maxval_x; val_x; val_y; bco2; pco2
     *               │   │   │                       // (таблица/файлы — буфер txt как у Textual, ветка не через TextualComponent)
     *               │   │   │
     *               │   │   ├── DataRecord       //  path; lenth(RO); maxval(RO); val(RO); length; format; mode;
     *               │   │   │                    // order; hig; gdc; gdw; gdh; bco1; pco1; xcen
     *               │   │   └── FileBrowser      // spay; filter; pco2; psta(RO); pic1; pic2; vvs2; buffsize(RO); fwarning(RO);
     *               │   │
     *               │   ├── ListSelect              // path; val; ch; setItemSpacing; setRowHeight
     *               │   │   │
     *               │   │   ├── ComboBox       //  ycen; up; pco3; bco1; pco1; dir; qty; vvs0; bco2; pco2;
     *               │   │   │                  // down; mode; wid; vvs1; xcen
     *               │   │   └── TextSelect     // pco2; pco1(line); txt(RO);
     *               │   │
     *               │   └── Multiline                 // spay; isbr; ycen; xcen;
     *               │       │
     *               │       ├── TextComponent      // txt <txt_maxl>
     *               │       │   ├── SLText         // left; ch; val_y<maxval_y>; ycen = delete
     *               │       │   ├── Text           // key; pw
     *               │       │   ├── ScrollText     // key; dir; setScrollStep; setScrollPeriod; enableAutoScroll
     *               │       │   │
     *               │       │   └── ButtonLikeComponent // bco2(pic2,picc2); pco2
     *               │       │        ├── Button
     *               │       │        └── DualStateButton // val;
     *               │       │
     *               │       └── Numeric                 // key; val; format;
     *               │           ├── Number           // setDigitCount
     *               │           └── XFloat           // point.setDigitsBeforePoint / setDigitsAfterPoint
     *               │
     *               └── Selection<COLOR>  // pco; val — в enum у Checkbox/Radio нет sta и font
     *                   ├── Checkbox               // —
     *                   ├── Radio                  // —
     *                   └── ToggleSwitch           // setTrackColor; setOffColor; setOnColor; setLabelFontId; setLabelGap; txt
     *
     * Пояснения:
     *
     * - Timer, NumericVariable, StringVariable, Audio, TouchCap, FileStream — прямые наследники Component (в панели нет x,y,w,h).
     * - TouchArea: всё с pos,w,h; первый лист — Hotspot (только геометрия); PageComponent — без drag/aph/effect.
     * - Drawable: refresh/show/hide/placeAbove/move — команды NIS на видимых виджетах.
     *
     * - VisualBaseComponent: под Geometry — один узел (в NIS drag, aph, effect); потомки: картинки, Media, StaBco (QR,
     *   Waveform, Gauge, FontedSta), Selection; дельта drag/aph/effect не повторяется у детей.
     * - Picture / CropPicture / ExternalPicture — не StaBco (набор атрибутов в enum другой). QR — под StaBco, sta none|has,
     *   не путать с растром; обязательный txt как данные кода, pic условно.
     *
     * - ListSelectComponent: ComboBox и TextSelect — выбор из строк path, val, ch, dis, spax, hig; различие в листьях
     *   (раскрывающийся список vs строка с подсветкой выбора, pco* / txt(RO) у TextSelect).
     *
     * - FontedStaComponent: единственное место в ветке для базового `pco` (цвет шрифта); у потомков в дельтах не дублируется,
     *   кроме отдельных имён pco1, pco2, pco3 в NIS (другие роли, не «ещё раз pco»).
     * - DataFile: прямой потомок FontedSta (рядом с Textual, не внутри него); txt/txt_maxl + общие поля
     *   скролла/ячеек для DataRecord и FileBrowser; DataRecord — path и график; FileBrowser — filter, spay, pic*, предупреждения.
     *
     * - SLText — прямой потомок Textual (не MultilineText); в NIS без атрибута key; xcen, left, ch, maxval_y, val_y, isbr, spax, spay.
     * - MultilineTextComponent: spax; spay; isbr; xcen; ycen — общее для Text, ScrollText, ButtonLike;
     *   у Number/XFloat spay остаётся в дельте листа под NumericText.
     *
     * - Checkbox/Radio — Selection, не FontedSta (в enum нет sta и font); ToggleSwitch — Selection + дельта с font и txt (enum).
     *
     * - DataRecord: в enum дублируется val — сверять RO/RW с редактором; общие поля см. DataFile.
     * - Классы C++ не для всех листьев — см. nexWidgets.hpp.
     *
     * --- Архитектурное дерево (имена; атрибуты — сумма дельт от Component до листа) ---
     *
     * object → timer | numericvariable | stringvariable | audio | touchcap | filestream
     *
     *       → geometry (x,y,w,h) → hotspot | page | visual_base (drag, aph, effect)
     *
     *       → visual_base → picture | croppicture | externalpicture | media
     *                   → sta_bco ( qrcode | drawable_colored | fonted_sta → DataFile…; textual…; NumericText… )
     *                   → selection (checkbox | radio | toggleswitch)
     */

} // namespace nex
