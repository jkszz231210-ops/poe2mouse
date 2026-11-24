#pragma once
#include <array>
#include <cstddef>
#include <istream>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>

namespace pm {
enum class Action : unsigned { up, down, left, right, attack, skill, dodge, count };
inline constexpr std::size_t action_count = static_cast<std::size_t>(Action::count);
inline constexpr std::array<std::string_view, action_count> names{
    "up", "down", "left", "right", "attack", "skill", "dodge"};
enum class Target : unsigned { none=0, p1=1, p2=2, both=3 };
struct Binding {
    Target target{Target::none};
    Action action{Action::up};
};
using Profile = std::array<Binding,256>;

// Transactional parser: a rejected profile never changes the previous profile.
// Format: first non-comment line "schema 1"; subsequent lines: key target action.
inline bool read_profile(std::istream& in, Profile& out, std::string& error) {
    Profile next{};
    bool schema = false;
    std::string line;
    unsigned number = 0;
    auto fail = [&](std::string_view why) {
        error = "line " + std::to_string(number) + ": " + std::string(why);
        return false;
    };
    while (std::getline(in,line)) {
        ++number;
        if (number > 1024 || line.size() > 256) return fail("profile limit exceeded");
        std::istringstream row(line);
        std::string first, extra;
        if (!(row >> first) || first.starts_with('#')) continue;
        if (!schema) {
            unsigned version{};
            if (first != "schema" || !(row >> version) || version != 1 || (row >> extra))
                return fail("unsupported schema");
            schema = true;
            continue;
        }
        unsigned key{};
        std::istringstream code(first);
        char trailing{};
        if (!(code >> key) || (code >> trailing) || key == 0 || key >= next.size())
            return fail("invalid key");
        std::string target, action;
        if (!(row >> target >> action) || (row >> extra)) return fail("expected key target action");
        Target t = target == "p1" ? Target::p1 : target == "p2" ? Target::p2 :
            target == "both" ? Target::both : Target::none;
        if (t == Target::none) return fail("invalid target");
        std::size_t a = 0;
        while (a < names.size() && names[a] != action) ++a;
        if (a == names.size()) return fail("invalid action");
        if (next[key].target != Target::none) return fail("duplicate key");
        next[key] = {t, static_cast<Action>(a)};
    }
    if (in.bad() || (!in.eof() && in.fail())) return fail("read error");
    if (!schema) return fail("missing schema");
    out = next;
    error.clear();
    return true;
}

// Canonical, portable serialization. No execution or embedded paths.
inline bool write_profile(std::ostream& out, const Profile& p) {
    out << "schema 1\n";
    for (std::size_t k=1; k<p.size(); ++k) {
        const auto b=p[k];
        if (b.target==Target::none) continue;
        const auto a=static_cast<std::size_t>(b.action);
        const char* target=b.target==Target::p1?"p1":b.target==Target::p2?"p2":b.target==Target::both?"both":nullptr;
        if (!target || a>=names.size()) return false;
        out << k << ' ' << target << ' ' << names[a] << '\n';
    }
    return static_cast<bool>(out);
}
class Engine {
public:
    using State = std::array<std::array<unsigned,action_count>,2>;
    void set_profile(const Profile& p) noexcept { reset(); profile_ = p; }
    void reset() noexcept { held_.fill(false); state_ = {}; }
    bool input(unsigned key, bool down) noexcept {
        if (key >= held_.size() || held_[key] == down) return false;
        held_[key] = down;
        const auto b = profile_[key];
        const auto a = static_cast<std::size_t>(b.action);
        const auto mask = static_cast<unsigned>(b.target);
        if (a >= action_count || mask > 3) return false;
        for (unsigned p=0; p<2; ++p) {
            if ((mask & (1u<<p)) == 0) continue;
            auto& count = state_[p][a];
            if (down) ++count;
            else if (count > 0) --count;
        }
        return mask != 0;
    }
    [[nodiscard]] const State& state() const noexcept { return state_; }
private:
    Profile profile_{};
    std::array<bool,256> held_{};
    State state_{};
};
} // namespace pm
