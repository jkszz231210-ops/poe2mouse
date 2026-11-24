#pragma once
#include <algorithm>
#include <cmath>
namespace pm {
struct Axis { double x{},y{}; };
struct Curve { double deadzone=0.08, minimum=0.15, exponent=1.0, sensitivity=1.0; };
// Pure radial response calibration. Inputs are normalized to [-1,1].
// No OS input, pointer capture, game coordinates or automatic targeting.
inline Axis radial_curve(Axis v,Curve c) noexcept {
    if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(c.deadzone)||
       !std::isfinite(c.minimum)||!std::isfinite(c.exponent)||!std::isfinite(c.sensitivity)) return {};
    if(c.deadzone<0||c.deadzone>=1||c.minimum<0||c.minimum>1||
       c.exponent<=0||c.sensitivity<=0) return {};
    v.x=std::clamp(v.x,-1.0,1.0);v.y=std::clamp(v.y,-1.0,1.0);
    const double n=std::hypot(v.x,v.y);
    if(n==0) return {};
    const double magnitude=std::min(n*c.sensitivity,1.0);
    if(magnitude<=c.deadzone) return {};
    const double normalized=(magnitude-c.deadzone)/(1-c.deadzone);
    const double out=c.minimum+(1-c.minimum)*std::pow(normalized,c.exponent);
    return {v.x/n*out,v.y/n*out};
}
}
