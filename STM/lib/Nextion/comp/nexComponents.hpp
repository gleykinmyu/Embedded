#pragma once
#include <cstdint>
#include "../core/nexMessages.hpp"

namespace nex {

    class Component;

    /**
     * Базовая страница: таблица указателей на компоненты заполняется в их конструкторах (registerComponent).
     * Обработка touch без switch — обход таблицы в dispatchTouch.
     */
    class Page {
        friend class Component;

    public:
        static constexpr unsigned MAX_COMPONENTS = 24;

        const uint8_t ID;
        explicit Page(uint8_t id) noexcept;
        void dispatchTouch(const msg::TouchCompEvent& e) noexcept;

    protected:
        void registerComponent(Component* c) noexcept;

    private:
        Component* _registry[MAX_COMPONENTS]{};
        unsigned _count = 0;
    };

    /**
     * Объект компонента на странице Nextion (MCU: `name` ↔ objname, `type` ↔ атрибут type, `ID()` ↔ id).
     *
     * Общие атрибуты NIS для большинства виджетов на странице (полный набор зависит от типа и серии):
     * - Геометрия: x, y, w, h — RW
     * - Включение / прозрачность: en, aph — RW
     * - Идентификация и область имени: type, id, objname, vscope — всегда RO (как в Attribute Pane)
     * - drag, ustype — зависят от типа и серии; не у всех объектов (ветки GeometryComponent / VisualBaseComponent — в конце файла)
     * - sta — одно имя в NIS, но смысл и допустимые коды не едины для всех типов: у большинства видимых виджетов это
     *   стиль заливки/фона (в инструкции xstr для области текста: 0 crop, 1 solid, 2 image, 3 none — NIS); у Variable —
     *   выбор режима str/num на этапе дизайна (Editor Guide); у Waveform и др. — своя таблица (см. Attribute Pane / NIS по типу).
     * - vscope (RO): область имени в проекте Nextion (NIS: `b[id].…` vs `p[page].b[id].…`),
     *   не «видимость на экране» (для показа/скрытия — en, vis, aph и т.д.); см. `nex::prop::Prop::Vscope` в nexPropLiterals.hpp
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
            Variable        = 52, //type, id, objname, vscope, sta(Number, String), val (txt, txt_maxl) - ДОЛЖНЫ БЫТЬ ДВА РАЗНЫХ ТИПА ОБЪЕКТА
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
            Hotspot         = 109, //type, id, objname, vscope, x, y, w, h
            Picture         = 112, //type, id, objname, vscope, drag, aph, effect, pic, x, y, w, h
            CropPicture     = 113, //type, id, objname, vscope, drag, aph, effect, cpic, x, y, w, h
            Text            = 116, //type, id, objname, vscope, drag, aph, effect, sta (cropimage, color, image, transparent), key, font, bco (pic, picc), pco, xcen, ycen, pw, txt, txt_maxl, isbr,spax, spay, x, y, w, h
            PageComponent   = 121, //type, id, vscope, effect, up, down, left, right, sta(no background, color, image), bco(pic) - when sta != no background, x, y, w, h
            Gauge           = 122, //type, id, objname, vscope, drag, aph, effect, sta(cropimage, color, image, transparent), bco(picc, pic) - when sta != transparent, val, format, up, down, left, pco, pco2, hig, vvs0, vvs1, vvs2, x, y, w, h
        };

        const char* const name;
        const Page& page;
        /** Вид этого экземпляра (тот же код, что атрибут `type` объекта на панели). */
        const Type type;

        Component(Page& owner, const char* objectName, Type componentType, uint8_t id = 0) noexcept;

        constexpr uint8_t ID() const noexcept { return _ID; }
        virtual void onTouch(const msg::TouchCompEvent& e);

    protected:
        uint8_t _ID;
    };

    /*
     * ---------------------------------------------------------------------------
     * Дерево наследования C++ (план по enum Type — строки комментариев у значений enum = эталон атрибутов NIS).
     * Узел: только дельта к непосредственному родителю (без повторения type, id, vscope, objname и полей предков).
     * RO — как в enum; у PageComponent в редакторе нет objname.
     *
     * Ветки без font в enum вынесены отдельно от ветки с font:
     *   StaBcoComponent — в базе sta (режимы по типу), bco; без font. Атрибут pco как «цвет шрифта» (глифы) не на этом узле.
     *   FontedStaComponent — дельта + font; здесь же единый базовый `pco` (цвет шрифта NIS). У потомков FontedSta в дельтах
     *   `pco` не повторяем; оставляем только отличные от базового имена: pco1, pco2, pco3, … (отдельные атрибуты в панели).
     *   Под StaBco (без FontedSta): QR, DrawableColored — в enum нет font; у листьев свои дельты (в т.ч. pco0…3, pco(ppic) — не смешивать с pco шрифта).
     *   SelectionComponent — у Checkbox/Radio в enum нет sta и font; у ToggleSwitch есть font (см. enum), sta нет.
     *
     *   PageComponent                          // effect; up; down; left; right; sta; bco(pic при sta≠no background)
     *   Component                              // type(RO), id(RO), vscope(RO); objname(RO) — если есть у типа
     *   │
     *   ├── Timer                              // tim, en
     *   ├── Variable                           // sta(Number|String); val|txt; txt_maxl (строковый режим)
     *   ├── Audio                              // from(RO); vid; en; loop; tim; stim(RO)
     *   ├── TouchCap                           // val(RO)
     *   ├── FileStream                         // val; qty(RO); en(RO); open, read, write, close, find
     *   │
     *   └── GeometryComponent                  // x, y, w, h — компоненты с прямоугольником на странице
     *       ├── Hotspot                        // —
     *       │
     *       └── VisualComponent                // drag, aph, effect
     *           ├── ExternalPicture            // path
     *           ├── MediaComponent             // vid; en; loop; dis; tim; stim(RO); qty(RO)
     *           │   ├── Gmov                   // —
     *           │   └── Video                  // from(RO); vid(path) при from=external
     *           │
     *           └── BGComponent<STYLE = CROP, COLOR, IMAGE, TRANSPARENT>  //templated structure bg: bg.color / bg.image / bg.cropimage / bg
     *               │
     *               ├── QRCode<COLOR>          // pco; dis; txt; txt_maxl
     *               ├── Picture<IMAGE>
     *               ├── CropPicture<CROP_IMAGE>
     *               │
     *               ├── DrawableColoredComponent 
     *               │   ├── Waveform<chNum>         // {gdc; gdw; gdh;} -> grid.color, grid.width, grid.height; pco0…pco3-> ch[0..3].color (number depends from chNum); dis; wid; hig; {add, addt - add values to form.}
     *               │   ├── ProgressBar<COLOR, IMAGE>  // val; dis(sta=color); pco(ppic)
     *               │   ├── Slider<CROP, COLOR, IMAGE> // wid; hig; bco1(pic1,picc1); pco; val -> value; {maxval; minval;} -> value.min; value.max ch
     *               │   └── Gauge                      // bco(picc,pic); val -> value; format; up; down; left;
     *               │                                  // pco; pco2; hig; vvs0; vvs1; vvs2
     *               │
     *               ├── PrintableComponent          // {font; pco; spax} -> font.id; font.color; font.spacing
     *               │   ├── DataFileRecordComponent // txt; txt_maxl; left; ch; dir; val; txt(RO); qty(RO); dis;
     *               │   │   │                       // maxval_y; maxval_x; val_x; val_y; bco2; pco2
     *               │   │   │                       // (таблица/файлы — буфер txt как у Textual, ветка не через TextualComponent)
     *               │   │   │
     *               │   │   ├── DataRecord       //  path; lenth(RO); maxval(RO); val(RO); length; format; mode;
     *               │   │   │                    // order; hig; gdc; gdw; gdh; bco1; pco1; xcen
     *               │   │   └── FileBrowser      // spay; filter; pco2; psta(RO); pic1; pic2; vvs2; buffsize(RO); fwarning(RO);
     *               │   │
     *               │   ├── ListSelectTextComponent // path <path_m>; text<txt_maxl>, val; ch; dis; hig
     *               │   │   │
     *               │   │   ├── ComboBox       //  ycen; up; pco3; bco1; pco1; dir; qty; vvs0; bco2; pco2;
     *               │   │   │                  // down; mode; wid; vvs1; xcen
     *               │   │   └── TextSelect     // pco2; pco1(line); txt(RO);
     *               │   │
     *               │   └── MultilineComponent // spay; isbr; ycen; xcen;
     *               │       │
     *               │       ├── TextComponent      // txt <txt_maxl>
     *               │       │   ├── SLText         // left; ch; val_y<maxval_y>; ycen = delete
     *               │       │   ├── Text           // key; pw
     *               │       │   ├── ScrollText     // key; dir; dis; tim; en
     *               │       │   │
     *               │       │   └── ButtonLikeComponent // bco2(pic2,picc2); pco2
     *               │       │        ├── Button
     *               │       │        └── DualStateButton // val;
     *               │       │
     *               │       └── NumericComponent // key; val; format;
     *               │           ├── Number           // length
     *               │           └── XFloat           // vvs0; vvs1;
     *               │
     *               └── SelectionComponent<COLOR>  // pco; val — в enum у Checkbox/Radio нет sta и font
     *                   ├── Checkbox               // —
     *                   ├── Radio                  // —
     *                   └── ToggleSwitch           // bco2; pco2; pco1(font color); font; dis; txt(txt_maxl=24)
     *
     * Пояснения:
     *
     * - Timer, Variable, Audio, TouchCap, FileStream — прямые наследники Component (в панели нет x,y,w,h).
     * - GeometryComponent: всё с x,y,w,h; первый лист — Hotspot (только геометрия); PageComponent — без drag/aph/effect.
     *
     * - VisualBaseComponent: под Geometry — один узел (в NIS drag, aph, effect); потомки: картинки, Media, StaBco (QR,
     *   DrawableColored, FontedSta), Selection; дельта drag/aph/effect не повторяется у детей.
     * - Picture / CropPicture / ExternalPicture — не StaBco (набор атрибутов в enum другой). QR — под StaBco, sta none|has,
     *   не путать с растром; обязательный txt как данные кода, pic условно.
     *
     * - ListSelectComponent: ComboBox и TextSelect — выбор из строк path, val, ch, dis, spax, hig; различие в листьях
     *   (раскрывающийся список vs строка с подсветкой выбора, pco* / txt(RO) у TextSelect).
     *
     * - FontedStaComponent: единственное место в ветке для базового `pco` (цвет шрифта); у потомков в дельтах не дублируется,
     *   кроме отдельных имён pco1, pco2, pco3 в NIS (другие роли, не «ещё раз pco»).
     * - DataFileRecordComponent: прямой потомок FontedSta (рядом с Textual, не внутри него); txt/txt_maxl + общие поля
     *   скролла/ячеек для DataRecord и FileBrowser; DataRecord — path и график; FileBrowser — filter, spay, pic*, предупреждения.
     *
     * - SLText — прямой потомок Textual (не MultilineText); в NIS без атрибута key; xcen, left, ch, maxval_y, val_y, isbr, spax, spay.
     * - MultilineTextComponent: spax; spay; isbr; xcen; ycen — общее для Text, ScrollText, ButtonLike;
     *   у Number/XFloat spay остаётся в дельте листа под NumericText.
     *
     * - Checkbox/Radio — Selection, не FontedSta (в enum нет sta и font); ToggleSwitch — Selection + дельта с font и txt (enum).
     *
     * - DataRecord: в enum дублируется val — сверять RO/RW с редактором; общие поля см. DataFileRecordComponent.
     * - Классы C++ не для всех листьев — см. nexWidgets.hpp.
     *
     * --- Архитектурное дерево (имена; атрибуты — сумма дельт от Component до листа) ---
     *
     * object → timer | variable | audio | touchcap | filestream
     *
     *       → geometry (x,y,w,h) → hotspot | page | visual_base (drag, aph, effect)
     *
     *       → visual_base → picture | croppicture | externalpicture | media
     *                   → sta_bco ( qrcode | drawable_colored | fonted_sta → DataFileRecord…; textual…; NumericText… )
     *                   → selection (checkbox | radio | toggleswitch)
     */

} // namespace nex
