/**
 * @file camera_protocol.hpp
 * @brief Паттерны кадров Camera Control Protocol (ConvertibleProtocol.pdf v3.05, стр. 2–27).
 */

#pragma once

#include "transport/frame.hpp"
#include "transport/types.hpp"

#include <cstddef>
#include <cstdint>

namespace ccam {

/** Паттерны команд камеры (PDF стр. 2–3). 9–11 в PDF упомянуты, формат не описан. */
enum class CamPattern : uint8_t {
    Trigger = 1,   ///< [STX]O??S[ETX] или Oxx без data (pattern 1/2)
    SetOData = 2,  ///< [STX]O??:data[ETX]
    Scene = 3,     ///< [STX]XSF:n[ETX]
    Monitor = 4,   ///< [STX]D??:n[ETX]
    OsdMenu = 5,   ///< [STX]OSD:nn:data[ETX] / QSA:nn / OSE / OSG / OSI / OSJ
    Query = 6,     ///< [STX]Q??[ETX]
    QueryOsd = 7,  ///< [STX]QSD:nn[ETX] → OSD:nn:data
    Contact = 8,   ///< [STX]H??[ETX] — lens I/F, contact P/T
};

/** Запись каталога команд (см. camera_commands*.hpp). */
struct CamCommandEntry {
    const char* name;
    const char* control;  ///< nullptr если только query
    const char* query;    ///< nullptr если только control
    CamPattern pattern;
    const char* notes;
};

struct CamCommandTable {
    const CamCommandEntry* entries;
    size_t count;
};

/** Сборка кадра по паттерну (без отправки). */
Status camBuildTrigger(CameraFrame& frame, const char* cmd3);
Status camBuildSet(CameraFrame& frame, const char* cmd3, const char* data);
Status camBuildQuery(CameraFrame& frame, const char* cmd3);
Status camBuildScene(CameraFrame& frame, uint8_t scene_index);
Status camBuildMonitor(CameraFrame& frame, const char* dcmd3, const char* data1);
Status camBuildOsdSet(CameraFrame& frame, uint8_t subcode, const char* data);
Status camBuildOsdQuery(CameraFrame& frame, uint8_t subcode);
/** prefix: "OSA", "OSE", "OSG", "OSI", "OSJ" — subcode 00..FF hex ASCII */
Status camBuildMenuSet(CameraFrame& frame, const char* prefix, uint8_t subcode, const char* data);
Status camBuildMenuQuery(CameraFrame& frame, const char* prefix, uint8_t subcode);
Status camBuildContact(CameraFrame& frame, const char* hcmd3);

/** Форматирование subcode в 2 hex-символа (00..FF). */
void camFormatSubcode(char out[3], uint8_t subcode);

} // namespace ccam
