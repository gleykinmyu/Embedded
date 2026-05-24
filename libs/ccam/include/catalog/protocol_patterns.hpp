/**
 * @file protocol_patterns.hpp
 * @brief Паттерны кадров Convertible Camera Protocol (PDF стр. 2–3).
 */

#pragma once

#include <cstdint>

namespace ccam::catalog {

/** Тип кадра камеры [STX]…[ETX]. */
enum class CameraPattern : uint8_t {
    Trigger = 1,   ///< [STX]O?S[ETX] — операция без data (OWS, OAS)
    SetO = 2,      ///< [STX]Oxx:data[ETX]
    Scene = 3,     ///< [STX]XSF:n[ETX]
    Monitor = 4,   ///< [STX]Dxx:n[ETX]
    OsdSet = 5,    ///< [STX]OSD:nn:data[ETX] (2 hex + data 2 byte)
    Query = 6,     ///< [STX]Qxx[ETX]
    OsdQuery = 7,  ///< [STX]QSD:nn[ETX] → OSD:nn:data
    LensH = 8,     ///< [STX]Hxx[ETX] — lens I/F card
    VideoMenuSet = 9,    ///< [STX]OSA:nn:data[ETX]
    VideoMenuQuery = 10, ///< [STX]QSA:nn[ETX]
    DcBoardSet = 11,     ///< [STX]OSE:nn:data[ETX]
    DcBoardQuery = 12,   ///< [STX]QSE:nn[ETX]
    ExtendedSetG = 13,   ///< [STX]OSG:nn:data[ETX]
    ExtendedQueryG = 14, ///< [STX]QSG:nn[ETX]
    ExtendedSetI = 15,   ///< [STX]OSI:…[ETX]
    ExtendedQueryI = 16, ///< [STX]QSI:…[ETX]
    ExtendedSetJ = 17,   ///< [STX]OSJ:…[ETX] — HE/UE series
    ExtendedQueryJ = 18, ///< [STX]QSJ:…[ETX]
};

/** Тип кадра P/T #…CR (PDF стр. 28+). */
enum class PtPattern : uint8_t {
    HashCmd,       ///< #XY[data]CR — основной формат
    HashQuery,     ///< запрос с ответом (#PTV, #RER, #QSV…)
    HashExtended,  ///< #APS, #RPC, #LC — несколько hex-полей
};

} // namespace ccam::catalog
