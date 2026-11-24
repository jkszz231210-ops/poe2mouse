#pragma once
#include "pad_output.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <algorithm>
namespace pm {
struct PhysicalPadState {
    std::uint16_t buttons{};
    std::uint8_t lt{},rt{};
    std::int16_t lx{},ly{},rx{},ry{};
    friend bool operator==(const PhysicalPadState&,const PhysicalPadState&)=default;
};
inline std::array<std::int16_t,2> radial_deadzone(std::int16_t x,std::int16_t y,unsigned deadzone) noexcept {
    const double dx=static_cast<double>(x),dy=static_cast<double>(y);
    const double length=std::hypot(dx,dy);
    if(!std::isfinite(length)||length<=static_cast<double>(deadzone)||length==0.0) return {};
    const double magnitude=std::min(1.0,(length-static_cast<double>(deadzone))/(32767.0-static_cast<double>(deadzone)));
    const double scale=magnitude*32767.0/length;
    return {static_cast<std::int16_t>(std::clamp(std::lround(dx*scale),-32768l,32767l)),
            static_cast<std::int16_t>(std::clamp(std::lround(dy*scale),-32768l,32767l))};
}
class HybridController {
public:
    bool update(const PhysicalPadState& sample,std::uint16_t role_toggle) noexcept {
        const auto previous=reports_;
        const bool toggle_down=role_toggle!=0&&(sample.buttons&role_toggle)!=0;
        const bool role_changed=toggle_down&&!toggle_held_;
        if(role_changed) active_player_^=1u;
        toggle_held_=toggle_down;
        const auto left=radial_deadzone(sample.lx,sample.ly,7849);
        const auto right=radial_deadzone(sample.rx,sample.ry,8689);
        PadReport shared{}; shared.lx=left[0];shared.ly=left[1];
        PadReport focused{};
        focused.buttons=static_cast<std::uint16_t>(sample.buttons&static_cast<std::uint16_t>(~role_toggle));
        focused.lt=sample.lt;focused.rt=sample.rt;focused.rx=right[0];focused.ry=right[1];
        reports_={shared,shared};
        reports_[active_player_]=merge_pad_reports(shared,focused);
        return role_changed||reports_!=previous;
    }
    void reset_inputs() noexcept {toggle_held_=false;reports_={};}
    void reset_role() noexcept {active_player_=0;reset_inputs();}
    [[nodiscard]] unsigned active_player() const noexcept {return active_player_;}
    [[nodiscard]] const std::array<PadReport,2>& reports() const noexcept {return reports_;}
private:
    unsigned active_player_{};
    bool toggle_held_{};
    std::array<PadReport,2> reports_{};
};
}
