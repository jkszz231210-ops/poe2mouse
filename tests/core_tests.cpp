#include "poe2mouse/input_core.hpp"
#include "poe2mouse/arena.hpp"
#include "poe2mouse/pad_output.hpp"
#include "poe2mouse/slot_identity.hpp"
#include "poe2mouse/readback_monitor.hpp"
#include "poe2mouse/bridge_labels.hpp"
#include "poe2mouse/physical_keys.hpp"
#include "poe2mouse/axis_curve.hpp"
#include <limits>
#include "poe2mouse/diagnostics.hpp"
#include <iostream>
#include <cstdlib>
using namespace pm;
void check(bool ok, const char* message) {
    if (!ok) { std::cerr << message << '\n'; std::exit(1); }
}
int main() {
    Profile p{};
    std::string error;
    std::istringstream good("schema 1\n87 both up\n38 p2 up\n1 p1 attack\n2 p2 skill\n");
    check(read_profile(good,p,error),"valid profile");
    Engine e;
    e.set_profile(p);
    auto up = static_cast<unsigned>(Action::up);
    auto attack = static_cast<unsigned>(Action::attack);
    auto skill = static_cast<unsigned>(Action::skill);
    check(e.input(87,true),"press");
    check(e.state()[0][up]==1 && e.state()[1][up]==1,"sandbox dual state");
    check(!e.input(87,true) && e.state()[0][up]==1,"repeat dedup");
    e.input(38,true); e.input(87,false);
    check(e.state()[0][up]==0 && e.state()[1][up]==1,"overlap release");
    e.input(38,false);
    check(e.state()==Engine::State{},"release all");
    e.input(1,true); e.input(2,true);
    check(e.state()[0][attack]==1 && e.state()[1][skill]==1 && e.state()[1][attack]==0,"independent");
    e.reset();
    check(e.state()==Engine::State{},"focus reset");
    e.input(87,false);
    check(e.state()==Engine::State{},"late release");
    e.input(87,true); e.set_profile(p);
    check(e.state()==Engine::State{},"profile replacement reset");
    check(!e.input(300,true),"out of range");
    for (auto bad : {"schema 4\n", "87 both up\n", "schema 1\n87 both up\n87 p1 up\n",
                    "schema 1\n256 p1 up\n", "schema 1\n1 invalid up\n",
                    "schema 1\n1 p1 unknown\n", "schema 1\n-1 p1 up\n",
                    "schema 1\n1 p1 up extra\n"}) {
        std::istringstream invalid(bad);
        check(!read_profile(invalid,p,error) && !error.empty(),"invalid profile rejection");
        check(p[87].target==Target::both,"transactional preservation");
    }
    std::ostringstream serialized;
    check(write_profile(serialized,p),"serialize");
    Profile roundtrip{};
    std::istringstream again(serialized.str());
    check(read_profile(again,roundtrip,error),"read serialized profile");
    for(std::size_t i=0;i<p.size();++i)
        check(p[i].target==roundtrip[i].target && p[i].action==roundtrip[i].action,"round trip");
    Profile v2{};
    std::istringstream v2source("schema 2\n87 both up\n16 both solo_p1\n17 both solo_p2\n38 p2 up\n1 p1 attack\n");
    check(read_profile(v2source,v2,error),"schema 2 modifiers");
    e.set_profile(v2); e.input(87,true); e.input(16,true);
    check(e.state()[0][up]==1 && e.state()[1][up]==0,"held movement reroutes to P1");
    e.input(17,true);
    check(e.state()[0][up]==0 && e.state()[1][up]==0,"conflicting modifiers neutralize shared movement");
    e.input(38,true); e.input(1,true);
    check(e.state()[1][up]==1 && e.state()[0][attack]==1,"explicit actions remain independent");
    e.input(16,false);
    check(e.state()[0][up]==0 && e.state()[1][up]==2,"P2 shared plus explicit");
    e.input(17,false);
    check(e.state()[0][up]==1 && e.state()[1][up]==2,"restore shared movement");
    e.input(38,false); e.input(87,false); e.input(1,false);
    check(e.state()==Engine::State{},"release all after modifier changes");
    std::istringstream illegal("schema 2\n16 p1 solo_p1\n");
    check(!read_profile(illegal,v2,error),"reject invalid modifier target");
    e.set_profile(roundtrip);
    Profile v3{};
    std::istringstream v3source("schema 3\n81 p1 slot1\n81 p2 slot13 1\n81 p1 flask1 2\n81 p2 weapon_swap 3\n16 both solo_p1\n");
    check(read_profile(v3source,v3,error),"schema 3 chords");
    e.set_profile(v3);
    e.input(16,true); e.input(81,true);
    const auto slot13=static_cast<unsigned>(Action::slot13);
    const auto slot1=static_cast<unsigned>(Action::slot1);
    check(e.state()[1][slot13]==1&&e.state()[0][slot1]==0,"chord precedence");
    e.input(16,false);
    check(e.state()[1][slot13]==1,"held action stays latched");
    e.input(81,false);
    check(e.state()==Engine::State{},"release after modifier change");
    e.input(81,true); e.input(16,true);
    check(e.state()[0][slot1]==1&&e.state()[1][slot13]==0,"modifier after action does not switch skill");
    e.reset();
    e.input(16,true); e.input(17,true); e.input(81,true);
    check(e.state()[1][static_cast<unsigned>(Action::weapon_swap)]==1,"combined modifiers");
    e.reset();
    std::ostringstream v3serialized;
    check(write_profile(v3serialized,v3),"schema 3 serialize");
    std::istringstream v3again(v3serialized.str()); Profile v3round{};
    check(read_profile(v3again,v3round,error),"schema 3 round trip");
    check(v3round.layers[1][81].action==Action::slot13,"chord preserved");
    for(auto bad:{"schema 3\n81 p1 slot1 8\n","schema 3\n81 p1 slot1 1\n81 p2 slot2 1\n","schema 2\n81 p1 slot13\n","schema 3\n16 p1 slot1 1\n"}) {
        std::istringstream rejected(bad);
        check(!read_profile(rejected,v3,error),"invalid chord rejected");
        check(v3.layers[1][81].action==Action::slot13,"failed edit preserves prior profile");
    }
    e.set_profile(roundtrip);
    Arena arena;
    e.set_profile(roundtrip);
    e.input(87,true);
    const auto start=arena.positions();
    arena.step(e.state(),0.05);
    check(arena.positions()[0].y<start[0].y && arena.positions()[1].y<start[1].y,"both lab characters move");
    check(arena.positions()[0].y-start[0].y < arena.positions()[1].y-start[1].y,"different movement speeds");
    e.input(87,false);
    e.input(38,true);
    const auto before=arena.positions();
    arena.step(e.state(),0.05);
    check(arena.positions()[0].y==before[0].y && arena.positions()[1].y<before[1].y,"independent lab movement");
    e.reset();
    check(!Arena::moving(e.state()),"no timer required when neutral");
    arena.reset();
    Engine::State right{};
    right[0][static_cast<unsigned>(Action::right)]=1;
    for(int i=0;i<100;++i) arena.step(right,0.05);
    check(arena.positions()[0].x<=205-Arena::radius,"wall collision");
    Diagnostics d;
    d.record(Event::startup_ok);
    for(std::size_t i=0;i<1000;++i) d.record(Event::focus_reset);
    d.record(Event::profile_invalid);
    check(d.size()==Diagnostics::capacity,"diagnostic ring bounded");
    std::ostringstream report;
    d.write(report);
    const auto output=report.str();
    check(output.find("focus_reset: 1000")!=std::string::npos,"aggregate count retained");
    check(output.ends_with("  profile_invalid\n"),"recent event ordering");
    check(output.size()<4096,"bounded report size");
    check(output.find("  startup_ok\n")==std::string::npos,"oldest event evicted");
    const auto zero=radial_curve({0.01,0.01},{});
    check(zero.x==0&&zero.y==0,"radial deadzone");
    const auto diagonal=radial_curve({1,1},{});
    check(std::abs(std::hypot(diagonal.x,diagonal.y)-1)<1e-9,"diagonal bounded");
    const auto slow=radial_curve({0.2,0},{0.08,0.15,2,1});
    const auto fast=radial_curve({0.8,0},{0.08,0.15,2,1});
    check(slow.x>=0.15&&fast.x>slow.x&&fast.x<=1,"curve monotonic");
    const auto negative=radial_curve({-0.2,0},{0.08,0.15,2,1});
    check(std::abs(negative.x+slow.x)<1e-9,"curve symmetric");
    check(radial_curve({1,1},{1,0,1,1}).x==0,"invalid curve neutral");
    check(radial_curve({std::numeric_limits<double>::quiet_NaN(),0},{}).x==0,"invalid sample neutral");
    PadMap map{};
    std::istringstream mapping("schema 1\np1 up lup\np1 right lright\np1 slot1 a\np2 slot1 x\n");
    check(read_pad_map(mapping,map,error),"controller map");
    Profile independent{};
    std::istringstream bridge_profile("schema 3\n87 p1 up\n68 p1 right\n81 p1 slot1\n81 p2 slot1 1\n");
    check(read_profile(bridge_profile,independent,error)&&independent_profile(independent,map,error),"independent bridge config");
    check(!independent_profile(roundtrip,map,error),"reject sandbox broadcast profile");
    e.set_profile(independent);e.input(87,true);e.input(68,true);e.input(81,true);
    auto pr1=make_pad_report(e.state()[0],map[0]);
    check(pr1.ly==23170&&pr1.lx==23170&&pr1.buttons==0x1000,"normalized movement and independent button");
    check(make_pad_report(e.state()[1],map[1])==PadReport{},"other target neutral");
    e.input(81,false);e.input(16,true);e.input(81,true);e.input(16,false);
    check(make_pad_report(e.state()[1],map[1]).buttons==0x4000,"independent chord latched");
    e.input(81,false);check(make_pad_report(e.state()[1],map[1])==PadReport{},"chord release");
    e.reset();check(make_pad_report(e.state()[0],map[0])==PadReport{},"neutral on reset");
    std::istringstream duplicate_map("schema 1\np1 up lup\np1 up ldown\n");
    check(!read_pad_map(duplicate_map,map,error)&&map[0][0]==Control::lup,"transactional map rejection");
    PhysicalKeys keys;
    using Change=PhysicalKeys::Change;
    check(keys.input(1,42,16,true)==Change::pressed,"first shift");
    check(keys.input(1,42,16,true)==Change::ignored,"repeat physical shift");
    check(keys.input(1,54,16,true)==Change::ignored,"second shift same logical key");
    check(keys.input(1,42,16,false)==Change::ignored,"first shift release does not clear");
    check(keys.input(1,54,16,false)==Change::released,"last shift release");
    check(keys.input(1,87,87,true)==Change::pressed,"keyboard one");
    check(keys.input(2,87,87,true)==Change::ignored,"keyboard two");
    check(keys.input(1,87,87,false)==Change::ignored,"second keyboard retains logical key");
    check(keys.input(2,87,87,false)==Change::released,"second keyboard release");
    check(keys.input(2,87,87,false)==Change::ignored,"late release ignored");
    for(std::size_t i=0;i<PhysicalKeys::capacity;++i)
        check(keys.input(i,1,65,true)==(i==0?Change::pressed:Change::ignored),"bounded held state");
    check(keys.input(999,1,65,true)==Change::overflow,"capacity fail closed");
    keys.reset();
    check(keys.input(999,1,65,true)==Change::pressed,"tracker reset");
    PadMap full_map{};
    std::istringstream extended_map("schema 1\np1 menu start\np1 aim_right rright\np2 view back\n");
    check(read_pad_map(extended_map,full_map,error),"extended map parse");
    Engine::State manual{};
    manual[0][static_cast<unsigned>(Action::menu)]=1;
    manual[0][static_cast<unsigned>(Action::aim_right)]=1;
    auto extended=make_pad_report(manual[0],full_map[0]);
    check(extended.buttons==0x10&&extended.rx==32767&&extended.ry==0,"manual menu and right stick");
    check(make_pad_report(manual[1],full_map[1])==PadReport{},"second controller stays neutral");
    BridgeOptions opts{};
    std::istringstream options_input("schema 1\nnative_pause 121\n");
    check(read_bridge_options(options_input,opts,error)&&opts.pause_key==121,"native pause options");
    check(bridge_options_ok(independent,opts,error),"default pause no conflict");
    Profile edited=independent;
    check(rebind(edited,map,opts,87,0,90,0,{Target::p1,Action::up},error),"change physical key");
    check(edited[87].target==Target::none&&edited[90].action==Action::up,"old key cleared");
    check(!rebind(edited,map,opts,90,0,121,0,{Target::p1,Action::up},error),"reserved pause rejected");
    check(!rebind(edited,map,opts,90,0,81,0,{Target::p1,Action::up},error),"duplicate key rejected");
    check(edited[90].target==Target::p1,"failed edit retains state");
    e.set_profile(edited);e.input(90,true);
    check(make_pad_report(e.state()[0],map[0]).ly==32767,"new binding active");
    e.reset();check(make_pad_report(e.state()[0],map[0])==PadReport{},"pause neutral");
    check(new_slot(0,1)==0&&new_slot(1,3)==1,"sequential slot discovery");
    check(new_slot(2,10)==3&&new_slot(4,5)==0,"existing physical device slots");
    check(new_slot(0,3)==-1&&new_slot(1,2)==-1&&new_slot(1,1)==-1&&new_slot(16,17)==-1,"ambiguous or invalid discovery rejected");
    ReadbackMonitor live;
    check(live.sample(false)&&live.sample(false)&&live.sample(false),"tolerate transient sample lag");
    check(!live.sample(false),"persistent mismatch fails");
    check(live.sample(true)&&live.consecutive()==0,"matching sample clears streak");
    live.sample(false);live.reset();check(live.consecutive()==0,"pause clears streak");
    std::cout << "All core checks passed\n";
}
