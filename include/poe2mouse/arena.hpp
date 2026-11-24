#pragma once
#include "input_core.hpp"
#include <algorithm>
#include <cmath>
namespace pm {
struct Point { double x{},y{}; };
class Arena {
public:
    static constexpr double width=460, height=190;
    static constexpr double radius=9;
    void reset() noexcept { positions_={Point{80,75},Point{80,115}}; }
    Arena() noexcept { reset(); }
    [[nodiscard]] const auto& positions() const noexcept { return positions_; }
    [[nodiscard]] static bool moving(const Engine::State& s) noexcept {
        for (const auto& p:s)
            if ((p[0]>0)!=(p[1]>0) || (p[2]>0)!=(p[3]>0)) return true;
        return false;
    }
    void step(const Engine::State& s,double dt) noexcept {
        if (!std::isfinite(dt) || dt<=0) return;
        dt=std::min(dt,0.05);
        for (std::size_t p=0;p<2;++p) {
            const auto& a=s[p];
            double dx=static_cast<double>(a[3]>0)-static_cast<double>(a[2]>0);
            double dy=static_cast<double>(a[1]>0)-static_cast<double>(a[0]>0);
            const double norm=std::hypot(dx,dy);
            if (norm==0) continue;
            const double distance=(p==0?110.0:90.0)*dt/norm;
            auto& pos=positions_[p];
            const auto blocked=[](double x,double y) {
                return x>205-radius && x<255+radius && y>42-radius && y<148+radius;
            };
            const auto x=std::clamp(pos.x+dx*distance,radius,width-radius);
            const auto y=std::clamp(pos.y+dy*distance,radius,height-radius);
            if(!blocked(x,pos.y)) pos.x=x;
            if(!blocked(pos.x,y)) pos.y=y;
        }
    }
private:
    std::array<Point,2> positions_{};
};
}
