#pragma once
namespace pm {
// Four XInput slots. Reject unrelated disconnects, ambiguous additions and invalid masks.
constexpr int new_slot(unsigned before,unsigned after) noexcept {
    if(before>15u||after>15u||(before&after)!=before) return -1;
    const unsigned added=after&~before;
    for(int i=0;i<4;++i) if(added==(1u<<i)) return i;
    return -1;
}
}
