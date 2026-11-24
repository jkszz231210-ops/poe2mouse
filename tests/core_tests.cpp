#include "poe2mouse/input_core.hpp"
#include "poe2mouse/arena.hpp"
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
    for (auto bad : {"schema 2\n", "87 both up\n", "schema 1\n87 both up\n87 p1 up\n",
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
    std::cout << "All core checks passed\n";
}
