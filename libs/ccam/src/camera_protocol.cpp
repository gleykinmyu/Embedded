#include "protocol/camera_protocol.hpp"

#include <cstring>

namespace ccam {

void camFormatSubcode(char out[3], uint8_t subcode)
{
    static const char hex[] = "0123456789ABCDEF";
    out[0] = hex[(subcode >> 4) & 0x0F];
    out[1] = hex[subcode & 0x0F];
    out[2] = '\0';
}

static Status beginFrame(CameraFrame& frame)
{
    frame.clear();
    return frame.appendByte(kStx);
}

static Status endFrame(CameraFrame& frame)
{
    return frame.appendByte(kEtx);
}

Status camBuildTrigger(CameraFrame& frame, const char* cmd3)
{
    if (cmd3 == nullptr || std::strlen(cmd3) != 3) {
        return Status::Param;
    }
    Status st = beginFrame(frame);
    if (st != Status::Ok) {
        return st;
    }
    st = frame.appendStr(cmd3);
    if (st != Status::Ok) {
        return st;
    }
    return endFrame(frame);
}

Status camBuildSet(CameraFrame& frame, const char* cmd3, const char* data)
{
    if (cmd3 == nullptr || data == nullptr || std::strlen(cmd3) != 3) {
        return Status::Param;
    }
    Status st = beginFrame(frame);
    if (st != Status::Ok) {
        return st;
    }
    st = frame.appendStr(cmd3);
    if (st != Status::Ok) {
        return st;
    }
    st = frame.appendByte(static_cast<uint8_t>(kCmdSep));
    if (st != Status::Ok) {
        return st;
    }
    st = frame.appendStr(data);
    if (st != Status::Ok) {
        return st;
    }
    return endFrame(frame);
}

Status camBuildQuery(CameraFrame& frame, const char* cmd3)
{
    return camBuildTrigger(frame, cmd3);
}

Status camBuildScene(CameraFrame& frame, uint8_t scene_index)
{
    char data[4];
    data[0] = static_cast<char>('0' + (scene_index % 10));
    data[1] = '\0';

    Status st = beginFrame(frame);
    if (st != Status::Ok) {
        return st;
    }
    st = frame.appendStr("XSF");
    if (st != Status::Ok) {
        return st;
    }
    st = frame.appendByte(static_cast<uint8_t>(kCmdSep));
    if (st != Status::Ok) {
        return st;
    }
    st = frame.appendStr(data);
    if (st != Status::Ok) {
        return st;
    }
    return endFrame(frame);
}

Status camBuildMonitor(CameraFrame& frame, const char* dcmd3, const char* data1)
{
    if (dcmd3 == nullptr || data1 == nullptr || std::strlen(dcmd3) != 3) {
        return Status::Param;
    }
    Status st = beginFrame(frame);
    if (st != Status::Ok) {
        return st;
    }
    st = frame.appendStr(dcmd3);
    if (st != Status::Ok) {
        return st;
    }
    st = frame.appendByte(static_cast<uint8_t>(kCmdSep));
    if (st != Status::Ok) {
        return st;
    }
    st = frame.appendStr(data1);
    if (st != Status::Ok) {
        return st;
    }
    return endFrame(frame);
}

Status camBuildOsdSet(CameraFrame& frame, uint8_t subcode, const char* data)
{
    char sub[3];
    camFormatSubcode(sub, subcode);

    Status st = beginFrame(frame);
    if (st != Status::Ok) {
        return st;
    }
    st = frame.appendStr("OSD");
    if (st != Status::Ok) {
        return st;
    }
    st = frame.appendByte(static_cast<uint8_t>(kCmdSep));
    if (st != Status::Ok) {
        return st;
    }
    st = frame.appendStr(sub);
    if (st != Status::Ok) {
        return st;
    }
    st = frame.appendByte(static_cast<uint8_t>(kCmdSep));
    if (st != Status::Ok) {
        return st;
    }
    st = frame.appendStr(data);
    if (st != Status::Ok) {
        return st;
    }
    return endFrame(frame);
}

Status camBuildOsdQuery(CameraFrame& frame, uint8_t subcode)
{
    char sub[3];
    camFormatSubcode(sub, subcode);

    Status st = beginFrame(frame);
    if (st != Status::Ok) {
        return st;
    }
    st = frame.appendStr("QSD");
    if (st != Status::Ok) {
        return st;
    }
    st = frame.appendByte(static_cast<uint8_t>(kCmdSep));
    if (st != Status::Ok) {
        return st;
    }
    st = frame.appendStr(sub);
    if (st != Status::Ok) {
        return st;
    }
    return endFrame(frame);
}

Status camBuildMenuSet(CameraFrame& frame, const char* prefix, uint8_t subcode, const char* data)
{
    char sub[3];
    if (prefix == nullptr || data == nullptr || std::strlen(prefix) != 3) {
        return Status::Param;
    }
    camFormatSubcode(sub, subcode);

    Status st = beginFrame(frame);
    if (st != Status::Ok) {
        return st;
    }
    st = frame.appendStr(prefix);
    if (st != Status::Ok) {
        return st;
    }
    st = frame.appendByte(static_cast<uint8_t>(kCmdSep));
    if (st != Status::Ok) {
        return st;
    }
    st = frame.appendStr(sub);
    if (st != Status::Ok) {
        return st;
    }
    st = frame.appendByte(static_cast<uint8_t>(kCmdSep));
    if (st != Status::Ok) {
        return st;
    }
    st = frame.appendStr(data);
    if (st != Status::Ok) {
        return st;
    }
    return endFrame(frame);
}

Status camBuildMenuQuery(CameraFrame& frame, const char* prefix, uint8_t subcode)
{
    char sub[3];
    if (prefix == nullptr || std::strlen(prefix) != 3) {
        return Status::Param;
    }
    camFormatSubcode(sub, subcode);

    Status st = beginFrame(frame);
    if (st != Status::Ok) {
        return st;
    }
    st = frame.appendStr(prefix);
    if (st != Status::Ok) {
        return st;
    }
    st = frame.appendByte(static_cast<uint8_t>(kCmdSep));
    if (st != Status::Ok) {
        return st;
    }
    st = frame.appendStr(sub);
    if (st != Status::Ok) {
        return st;
    }
    return endFrame(frame);
}

Status camBuildContact(CameraFrame& frame, const char* hcmd3)
{
    return camBuildTrigger(frame, hcmd3);
}

} // namespace ccam
