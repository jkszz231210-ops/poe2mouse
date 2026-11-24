#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <ostream>
#include <string_view>

namespace pm {
enum class Event : unsigned {
    startup_ok, startup_profile_failed, profile_loaded, profile_invalid,
    profile_write_failed, profile_exported, profile_imported, focus_reset,
    timer_failed, arena_reset, count
};
inline constexpr std::array<std::string_view,static_cast<std::size_t>(Event::count)> event_names{
    "startup_ok","startup_profile_failed","profile_loaded","profile_invalid",
    "profile_write_failed","profile_exported","profile_imported","focus_reset",
    "timer_failed","arena_reset"
};
// Bounded in-memory operational events only. No key codes, text, paths or input history.
class Diagnostics {
public:
    static constexpr std::size_t capacity=64;
    void record(Event e) noexcept {
        const auto index=static_cast<std::size_t>(e);
        if(index>=event_names.size()) return;
        ++counts_[index];
        recent_[next_]=e;
        next_=(next_+1)%capacity;
        if(size_<capacity) ++size_;
    }
    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    void write(std::ostream& out) const {
        out << "event_counts:\n";
        for(std::size_t i=0;i<counts_.size();++i)
            out << "  " << event_names[i] << ": " << counts_[i] << '\n';
        out << "recent_operational_events (oldest first):\n";
        const auto first=(next_+capacity-size_)%capacity;
        for(std::size_t i=0;i<size_;++i)
            out << "  " << event_names[static_cast<std::size_t>(recent_[(first+i)%capacity])] << '\n';
    }
private:
    std::array<Event,capacity> recent_{};
    std::array<std::uint64_t,event_names.size()> counts_{};
    std::size_t next_{},size_{};
};
}
