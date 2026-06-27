#pragma once

/**
 * Реестр NIS-имён атрибутов (`objname.lexeme`): `attr::Id` ↔ литерал для `AttrRef` и `attr::Num`/`String`.
 * Список — макрос `NEX_ATTR_LEXEME_LIST`; порядок по алфавиту NIS-имени.
 */

#include <cstddef>
#include <cstdint>

#include "../core/nexTypes.hpp"

namespace nex {
namespace attr {

/** Одна строка макроса: `ENTRY(CppId, "nis_lexeme")`. Порядок — по алфавиту NIS-имени. */
#define NEX_ATTR_LEXEME_LIST(ENTRY) \
    ENTRY(Aph, "aph") \
    ENTRY(Bco, "bco") \
    ENTRY(Bco1, "bco1") \
    ENTRY(Bco2, "bco2") \
    ENTRY(Borderc, "borderc") \
    ENTRY(Borderw, "borderw") \
    ENTRY(Bpic, "bpic") \
    ENTRY(Buffsize, "buffsize") \
    ENTRY(Ch, "ch") \
    ENTRY(Cpic, "cpic") \
    ENTRY(Dir, "dir") \
    ENTRY(Dis, "dis") \
    ENTRY(Down, "down") \
    ENTRY(Drag, "drag") \
    ENTRY(Effect, "effect") \
    ENTRY(En, "en") \
    ENTRY(Filter, "filter") \
    ENTRY(Font, "font") \
    ENTRY(Format, "format") \
    ENTRY(From, "from") \
    ENTRY(Fwarning, "fwarning") \
    ENTRY(Gdc, "gdc") \
    ENTRY(Gdh, "gdh") \
    ENTRY(Gdw, "gdw") \
    ENTRY(H, "h") \
    ENTRY(Hig, "hig") \
    ENTRY(Id, "id") \
    ENTRY(Isbr, "isbr") \
    ENTRY(Key, "key") \
    ENTRY(Left, "left") \
    ENTRY(Length, "length") \
    ENTRY(Lenth, "lenth") \
    ENTRY(Loop, "loop") \
    ENTRY(Maxval, "maxval") \
    ENTRY(MaxvalX, "maxval_x") \
    ENTRY(MaxvalY, "maxval_y") \
    ENTRY(Mode, "mode") \
    ENTRY(Order, "order") \
    ENTRY(Path, "path") \
    ENTRY(Pco, "pco") \
    ENTRY(Pco0, "pco0") \
    ENTRY(Pco1, "pco1") \
    ENTRY(Pco2, "pco2") \
    ENTRY(Pco3, "pco3") \
    ENTRY(Pic, "pic") \
    ENTRY(Pic1, "pic1") \
    ENTRY(Pic2, "pic2") \
    ENTRY(Picc, "picc") \
    ENTRY(Picc1, "picc1") \
    ENTRY(Picc2, "picc2") \
    ENTRY(Ppic, "ppic") \
    ENTRY(Psta, "psta") \
    ENTRY(Pw, "pw") \
    ENTRY(Qty, "qty") \
    ENTRY(Spax, "spax") \
    ENTRY(Spay, "spay") \
    ENTRY(Stim, "stim") \
    ENTRY(Tim, "tim") \
    ENTRY(Txt, "txt") \
    ENTRY(Up, "up") \
    ENTRY(Val, "val") \
    ENTRY(ValX, "val_x") \
    ENTRY(ValY, "val_y") \
    ENTRY(Vid, "vid") \
    ENTRY(Vvs0, "vvs0") \
    ENTRY(Vvs1, "vvs1") \
    ENTRY(Vvs2, "vvs2") \
    ENTRY(W, "w") \
    ENTRY(Wid, "wid") \
    ENTRY(X, "x") \
    ENTRY(Xcen, "xcen") \
    ENTRY(Y, "y") \
    ENTRY(Ycen, "ycen")

/** Tag атрибута NIS; значение = индекс в `kLiteral`. */
enum class Id : uint8_t {
#define NEX_LEX_ENUM(name, lit) name,
    NEX_ATTR_LEXEME_LIST(NEX_LEX_ENUM)
#undef NEX_LEX_ENUM
    Count
};

static_assert(static_cast<uint8_t>(Id::Count) <= 255u, "nex::attr::Id: overflow uint8_t");

/** Лексемы NIS во flash; индекс = `Id`. */
inline constexpr Literal kLiteral[static_cast<size_t>(Id::Count)] = {
#define NEX_LEX_LIT(name, lit) Literal{lit},
    NEX_ATTR_LEXEME_LIST(NEX_LEX_LIT)
#undef NEX_LEX_LIT
};

#undef NEX_ATTR_LEXEME_LIST

[[nodiscard]] inline constexpr bool isValid(Id id) noexcept
{
    return static_cast<uint8_t>(id) < static_cast<uint8_t>(Id::Count);
}

[[nodiscard]] inline constexpr const Literal& literal(Id id) noexcept
{
    return isValid(id) ? kLiteral[static_cast<uint8_t>(id)] : kEmptyLiteral;
}

} // namespace attr
} // namespace nex
