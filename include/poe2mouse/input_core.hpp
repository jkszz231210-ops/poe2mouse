#pragma once
#include <array>
#include <cstddef>
#include <istream>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>

namespace pm {
enum class Action : unsigned {
    up, down, left, right, attack, skill, dodge, solo_p1, solo_p2,
    slot1, slot2, slot3, slot4, slot5, slot6, slot7, slot8, slot9, slot10, slot11, slot12, slot13,
    flask1, flask2, weapon_swap, interact, count
};
inline constexpr std::size_t action_count=static_cast<std::size_t>(Action::count);
inline constexpr std::array<std::string_view,action_count> names{
    "up","down","left","right","attack","skill","dodge","solo_p1","solo_p2",
    "slot1","slot2","slot3","slot4","slot5","slot6","slot7","slot8","slot9","slot10","slot11","slot12","slot13",
    "flask1","flask2","weapon_swap","interact"};
enum class Target : unsigned { none=0,p1=1,p2=2,both=3 };
struct Binding { Target target{Target::none}; Action action{Action::up}; };
inline constexpr std::size_t key_count=256, layer_count=8;
struct Profile {
    std::array<std::array<Binding,key_count>,layer_count> layers{};
    Binding& operator[](std::size_t k) noexcept { return layers[0][k]; }
    const Binding& operator[](std::size_t k) const noexcept { return layers[0][k]; }
    [[nodiscard]] constexpr std::size_t size() const noexcept { return key_count; }
    auto begin() const noexcept { return layers[0].begin(); }
    auto end() const noexcept { return layers[0].end(); }
};
inline constexpr unsigned mod_bit(unsigned k) noexcept {
    return k==16?1u:k==17?2u:k==18?4u:0u; // logical Shift, Ctrl, Alt
}
inline constexpr bool solo(Action a) noexcept { return a==Action::solo_p1 || a==Action::solo_p2; }
// Schema 1/2: key target action. Schema 3: key target action [modifier mask].
// Masks 1=Shift, 2=Ctrl, 4=Alt; combine by sum. Plain fallback is explicit.
inline bool read_profile(std::istream& in,Profile& out,std::string& error) {
    Profile next{}; unsigned schema=0,number=0; std::string line;
    auto fail=[&](std::string_view why){error="line "+std::to_string(number)+": "+std::string(why);return false;};
    while(std::getline(in,line)) {
        ++number;
        if(number>4096 || line.size()>256) return fail("profile limit exceeded");
        std::istringstream row(line); std::string first,extra;
        if(!(row>>first) || first.starts_with('#')) continue;
        if(!schema) {
            unsigned version{};
            if(first!="schema" || !(row>>version) || version<1 || version>3 || (row>>extra)) return fail("unsupported schema");
            schema=version; continue;
        }
        unsigned key{}; std::istringstream code(first); char trailing{};
        if(!(code>>key)||(code>>trailing)||key==0||key>=key_count) return fail("invalid key");
        std::string target,action;
        if(!(row>>target>>action)) return fail("expected key target action");
        unsigned mask=0;
        if(row>>extra) {
            std::istringstream layer(extra);
            if(schema<3 || !(layer>>mask) || (layer>>trailing) || mask>=layer_count || (row>>extra)) return fail("invalid modifier mask");
        }
        Target t=target=="p1"?Target::p1:target=="p2"?Target::p2:target=="both"?Target::both:Target::none;
        if(t==Target::none) return fail("invalid target");
        std::size_t a=0; while(a<names.size()&&names[a]!=action) ++a;
        if(a==names.size()) return fail("invalid action");
        const auto act=static_cast<Action>(a);
        if(a>=static_cast<std::size_t>(Action::slot1)&&schema<3) return fail("new action requires schema 3");
        if(solo(act)&&(schema<2||t!=Target::both||mask!=0)) return fail("solo modifier requires plain both binding");
        if(mask&&mod_bit(key)) return fail("modifier keys cannot be chord triggers");
        if(next.layers[mask][key].target!=Target::none) return fail("duplicate key and modifiers");
        next.layers[mask][key]={t,act};
    }
    if(in.bad()||(!in.eof()&&in.fail())) return fail("read error");
    if(!schema) return fail("missing schema");
    out=next; error.clear(); return true;
}
inline bool write_profile(std::ostream& out,const Profile& p) {
    out<<"schema 3\n";
    for(std::size_t m=0;m<layer_count;++m) for(std::size_t k=1;k<key_count;++k) {
        const auto b=p.layers[m][k]; if(b.target==Target::none) continue;
        const auto a=static_cast<std::size_t>(b.action);
        const char* t=b.target==Target::p1?"p1":b.target==Target::p2?"p2":b.target==Target::both?"both":nullptr;
        if(!t||a>=names.size()||(solo(b.action)&&(m||b.target!=Target::both))||(m&&mod_bit(static_cast<unsigned>(k)))) return false;
        out<<k<<' '<<t<<' '<<names[a]; if(m) out<<' '<<m; out<<'\n';
    }
    return static_cast<bool>(out);
}
class Engine {
public:
    using State=std::array<std::array<unsigned,action_count>,2>;
    void set_profile(const Profile& p) noexcept { reset(); profile_=p; }
    void reset() noexcept { held_.fill(false); latched_={}; base_={}; shared_={}; state_={}; mods_=0; }
    bool input(unsigned key,bool down) noexcept {
        if(key>=key_count||held_[key]==down) return false;
        held_[key]=down;
        const auto bit=mod_bit(key);
        if(bit) { if(down) mods_|=bit; else mods_&=~bit; }
        if(down) {
            auto b=profile_.layers[bit?0:mods_][key];
            if(b.target==Target::none) b=profile_[key];
            latched_[key]=b;
        }
        const auto b=latched_[key];
        if(!down) latched_[key]={};
        const auto a=static_cast<std::size_t>(b.action),mask=static_cast<std::size_t>(b.target);
        if(a>=action_count||mask==0||mask>3) return false;
        const auto change=[down](unsigned& n){if(down) ++n;else if(n) --n;};
        if(mask==3&&a<4) change(shared_[a]);
        else for(unsigned p=0;p<2;++p) if(mask&(1u<<p)) change(base_[p][a]);
        state_=base_;
        const bool p1=base_[0][static_cast<unsigned>(Action::solo_p1)]!=0;
        const bool p2=base_[1][static_cast<unsigned>(Action::solo_p2)]!=0;
        for(unsigned p=0;p<2;++p) {
            const bool allow=(!p1&&!p2)||(p==0&&p1&&!p2)||(p==1&&p2&&!p1);
            if(allow) for(std::size_t i=0;i<shared_.size();++i) state_[p][i]+=shared_[i];
        }
        return true;
    }
    [[nodiscard]] const State& state() const noexcept { return state_; }
private:
    Profile profile_{}; std::array<bool,key_count> held_{};
    std::array<Binding,key_count> latched_{}; unsigned mods_{};
    State base_{},state_{}; std::array<unsigned,4> shared_{};
};
}
