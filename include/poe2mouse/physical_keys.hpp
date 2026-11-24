#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace pm {
// Only current held physical inputs. No history, allocation, or persistent device identifiers.
class PhysicalKeys {
public:
    enum class Change { ignored, pressed, released, overflow };
    static constexpr std::size_t capacity=256;
    void reset() noexcept { held_={}; counts_={}; }
    Change input(std::uintptr_t device,std::uint32_t physical,unsigned logical,bool down) noexcept {
        if(logical==0||logical>=counts_.size()) return Change::ignored;
        Entry* free=nullptr;
        for(auto& e:held_) {
            if(!e.active) {if(!free) free=&e;continue;}
            if(e.device!=device||e.physical!=physical) continue;
            if(down) return Change::ignored;
            const unsigned old=e.logical;
            e={};
            --counts_[old];
            return counts_[old]==0?Change::released:Change::ignored;
        }
        if(!down) return Change::ignored;
        if(!free) return Change::overflow;
        *free={device,physical,logical,true};
        return ++counts_[logical]==1?Change::pressed:Change::ignored;
    }
private:
    struct Entry {std::uintptr_t device{};std::uint32_t physical{};unsigned logical{};bool active{};};
    std::array<Entry,capacity> held_{};
    std::array<unsigned,256> counts_{};
};
}
