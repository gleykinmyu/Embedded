#pragma once
// Adapted from GyverJoy by AlexGyver (MIT), see ../LICENSE.
#include <stdint.h>
#include <algorithm>
namespace joystick {
enum class Curve : uint8_t { Linear, Square, Cubic };
class Axis {
    uint16_t max_, zero_, dead_=0;
    Curve curve_=Curve::Linear;
    bool inverted_=false;
    int16_t value_=0;
public:
    explicit Axis(uint16_t max=4095):max_(max?max:1),zero_(max_/2){}
    void calibrate(uint16_t zero){zero_=std::min(zero,max_);}
    void deadzone(uint16_t width){dead_=width/2;}
    void exponent(Curve curve){curve_=curve;}
    void invert(bool enabled){inverted_=enabled;}
    int16_t value() const{return value_;}
    int16_t compute(uint16_t adc){
        int32_t d=int32_t(std::min(adc,max_))-zero_;
        int32_t magnitude=d<0?-d:d;
        int32_t span=d<0?zero_:max_-zero_;
        int32_t n=0;
        if(magnitude>dead_ && span>dead_) n=(magnitude-dead_)*32767/(span-dead_);
        if(curve_==Curve::Square) n=int64_t(n)*n/32767;
        else if(curve_==Curve::Cubic) n=int64_t(n)*n*n/(32767LL*32767);
        value_=((d<0)!=inverted_)?-n:n;
        return value_;
    }
};
}
