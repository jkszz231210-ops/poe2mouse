#pragma once
#include "input_core.hpp"
#include <cstdint>
#include <cmath>
#include <utility>
#include <algorithm>
namespace pm {
struct PadReport {
    std::uint16_t buttons{};
    std::uint8_t lt{},rt{};
    std::int16_t lx{},ly{},rx{},ry{};
    friend bool operator==(const PadReport&,const PadReport&)=default;
};
static_assert(sizeof(PadReport)==12);
inline PadReport merge_pad_reports(PadReport a,PadReport b) noexcept {
    auto axis=[](std::int16_t x,std::int16_t y) {
        return static_cast<std::int16_t>(std::clamp(static_cast<int>(x)+static_cast<int>(y),-32768,32767));
    };
    a.buttons=static_cast<std::uint16_t>(a.buttons|b.buttons);
    a.lt=std::max(a.lt,b.lt); a.rt=std::max(a.rt,b.rt);
    a.lx=axis(a.lx,b.lx); a.ly=axis(a.ly,b.ly);
    a.rx=axis(a.rx,b.rx); a.ry=axis(a.ry,b.ry);
    return a;
}
enum class Control : unsigned {
    none,lup,ldown,lleft,lright,rup,rdown,rleft,rright,
    a,b,x,y,lb,rb,lt,rt,dup,ddown,dleft,dright,start,back,lthumb,rthumb,count
};
inline constexpr std::array<std::string_view,static_cast<unsigned>(Control::count)> control_names{
    "none","lup","ldown","lleft","lright","rup","rdown","rleft","rright",
    "a","b","x","y","lb","rb","lt","rt","dup","ddown","dleft","dright","start","back","lthumb","rthumb"};
using PadMap=std::array<std::array<Control,action_count>,2>;
inline bool read_pad_map(std::istream& in,PadMap& output,std::string& error) {
    PadMap next{}; bool schema=false; unsigned number=0; std::string line;
    std::array<std::array<bool,action_count>,2> seen{};
    auto fail=[&](std::string_view why){error="line "+std::to_string(number)+": "+std::string(why);return false;};
    while(std::getline(in,line)) {
        ++number;
        if(number>256||line.size()>256) return fail("map limit exceeded");
        std::istringstream row(line); std::string first,action,control,extra;
        if(!(row>>first)||first.starts_with('#')) continue;
        if(!schema) {
            if(first!="schema"||!(row>>action)||action!="1"||(row>>extra)) return fail("expected schema 1");
            schema=true;continue;
        }
        const unsigned p=first=="p1"?0u:first=="p2"?1u:2u;
        if(p>1||!(row>>action>>control)||(row>>extra)) return fail("expected player action control");
        std::size_t a=0,c=0;
        while(a<names.size()&&names[a]!=action) ++a;
        while(c<control_names.size()&&control_names[c]!=control) ++c;
        if(a==names.size()||c==control_names.size()||solo(static_cast<Action>(a))||seen[p][a]) return fail("invalid or duplicate mapping");
        seen[p][a]=true;next[p][a]=static_cast<Control>(c);
    }
    if(!schema||in.bad()||(!in.eof()&&in.fail())) return fail("invalid map");
    output=next;error.clear();return true;
}
// External device bridge: a physical trigger resolves to exactly one role/action.
// Shared or solo sandbox mappings cannot be armed in this bridge.
inline bool independent_profile(const Profile& profile,const PadMap& map,std::string& error) {
    bool any=false;
    for(const auto& layer:profile.layers) for(const auto b:layer) {
        if(b.target==Target::none) continue;
        const auto a=static_cast<std::size_t>(b.action);
        if((b.target!=Target::p1&&b.target!=Target::p2)||a>=action_count||solo(b.action)) {
            error="shared or invalid binding is not supported by device bridge";return false;
        }
        const auto p=b.target==Target::p1?0u:1u;
        const auto c=static_cast<unsigned>(map[p][a]);
        if(c==0||c>=static_cast<unsigned>(Control::count)) {error="binding has no controller mapping";return false;}
        any=true;
    }
    if(!any) {error="empty profile";return false;}
    error.clear();return true;
}
inline PadReport make_pad_report(const Engine::State::value_type& state,const PadMap::value_type& map) noexcept {
    std::array<bool,static_cast<unsigned>(Control::count)> active{};
    for(std::size_t a=0;a<action_count;++a) {
        const auto c=static_cast<unsigned>(map[a]);
        if(state[a]&&c>0&&c<active.size()) active[c]=true;
    }
    auto on=[&](Control c){return active[static_cast<unsigned>(c)];};
    auto axis=[&](Control pos,Control neg){return static_cast<int>(on(pos))-static_cast<int>(on(neg));};
    auto stick=[](int x,int y,std::int16_t& ox,std::int16_t& oy){
        const double length=std::hypot(static_cast<double>(x),static_cast<double>(y));
        if(length==0) return;
        ox=static_cast<std::int16_t>(std::lround(32767.0*x/length));
        oy=static_cast<std::int16_t>(std::lround(32767.0*y/length));
    };
    PadReport r{};
    stick(axis(Control::lright,Control::lleft),axis(Control::lup,Control::ldown),r.lx,r.ly);
    stick(axis(Control::rright,Control::rleft),axis(Control::rup,Control::rdown),r.rx,r.ry);
    constexpr std::array<std::pair<Control,unsigned>,14> buttons{{
        {Control::dup,0x0001},{Control::ddown,0x0002},{Control::dleft,0x0004},{Control::dright,0x0008},
        {Control::start,0x0010},{Control::back,0x0020},{Control::lthumb,0x0040},{Control::rthumb,0x0080},
        {Control::lb,0x0100},{Control::rb,0x0200},{Control::a,0x1000},{Control::b,0x2000},
        {Control::x,0x4000},{Control::y,0x8000}}};
    for(const auto& [c,mask]:buttons) if(on(c)) r.buttons=static_cast<std::uint16_t>(r.buttons|mask);
    if(on(Control::lt)) r.lt=255;
    if(on(Control::rt)) r.rt=255;
    return r;
}
}
