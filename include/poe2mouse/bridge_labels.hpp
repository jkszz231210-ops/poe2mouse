#pragma once
#include "poe2mouse/pad_output.hpp"
#include <string>
namespace pm {
inline constexpr std::array<std::wstring_view,action_count> action_zh{
 L"向上移动",L"向下移动",L"向左移动",L"向右移动",L"攻击（逻辑）",L"技能（逻辑）",L"闪避（待标定）",L"仅P1（沙盒）",L"仅P2（沙盒）",
 L"测试技能槽1",L"测试技能槽2",L"测试技能槽3",L"测试技能槽4",L"测试技能槽5",L"测试技能槽6",L"测试技能槽7",L"测试技能槽8",L"测试技能槽9",L"测试技能槽10",L"测试技能槽11",L"测试技能槽12",L"测试技能槽13",
 L"药剂1（待标定）",L"药剂2（待标定）",L"切换武器（待标定）",L"交互（待标定）",
 L"手动瞄准上",L"手动瞄准下",L"手动瞄准左",L"手动瞄准右",L"菜单导航上",L"菜单导航下",L"菜单导航左",L"菜单导航右",L"菜单键",L"视图键",L"按下左摇杆",L"按下右摇杆"};
inline constexpr std::array<std::wstring_view,static_cast<unsigned>(Control::count)> control_zh{
 L"未映射",L"左摇杆↑",L"左摇杆↓",L"左摇杆←",L"左摇杆→",L"右摇杆↑",L"右摇杆↓",L"右摇杆←",L"右摇杆→",
 L"A 键",L"B 键",L"X 键",L"Y 键",L"LB 左肩键",L"RB 右肩键",L"LT 左扳机",L"RT 右扳机",L"十字键↑",L"十字键↓",L"十字键←",L"十字键→",L"Start 菜单键",L"Back 视图键",L"L3 左摇杆按下",L"R3 右摇杆按下"};
struct BridgeOptions {unsigned pause_key=121;};
inline bool read_bridge_options(std::istream& in,BridgeOptions& out,std::string& error) {
    BridgeOptions next{};bool schema=false,seen=false;unsigned lines=0;std::string line;
    while(std::getline(in,line)) {
        if(++lines>32||line.size()>128) {error="options size";return false;}
        std::istringstream row(line);std::string a,b,extra;
        if(!(row>>a)||a.starts_with('#')) continue;
        if(!schema) {if(a!="schema"||!(row>>b)||b!="1"||(row>>extra)) {error="options schema";return false;}schema=true;continue;}
        unsigned k{};
        if(seen||a!="native_pause"||!(row>>k)||k<112||k>135||(row>>extra)) {error="native_pause must be F1-F24 virtual key 112-135";return false;}
        next.pause_key=k;seen=true;
    }
    if(!schema||!seen||in.bad()||(!in.eof()&&in.fail())) {error="missing native_pause";return false;}
    out=next;error.clear();return true;
}
inline bool bridge_options_ok(const Profile& p,BridgeOptions o,std::string& error) {
    if(o.pause_key<112||o.pause_key>135) {error="invalid native pause key";return false;}
    for(const auto& layer:p.layers) if(layer[o.pause_key].target!=Target::none) {error="native pause key conflicts with a mapping";return false;}
    return true;
}
inline bool rebind(Profile& profile,const PadMap& map,BridgeOptions o,unsigned old_key,unsigned old_mask,unsigned key,unsigned mask,Binding b,std::string& error) {
    if(key==0||key>=key_count||mask>=layer_count||(mask&&mod_bit(key))||key==o.pause_key||
       (b.target!=Target::p1&&b.target!=Target::p2)) {error="invalid or reserved key";return false;}
    Profile next=profile;
    if(old_key>0&&old_key<key_count&&old_mask<layer_count) next.layers[old_mask][old_key]={};
    if(next.layers[mask][key].target!=Target::none) {error="key and modifiers already bound";return false;}
    next.layers[mask][key]=b;
    if(!independent_profile(next,map,error)||!bridge_options_ok(next,o,error)) return false;
    profile=next;return true;
}
}
