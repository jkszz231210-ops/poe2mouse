#include "poe2mouse/input_core.hpp"
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
    std::cout << "All core checks passed\n";
}
