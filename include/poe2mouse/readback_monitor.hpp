#pragma once
#include "pad_output.hpp"
#include <cstdint>
namespace pm {
// Compare sampled current device state, not a record of user inputs.
// Short transient mismatches are allowed while Windows delivers a new report.
class ReadbackMonitor {
public:
    void reset() noexcept { consecutive_=0; }
    bool sample(bool matches) noexcept {
        if(matches) consecutive_=0;
        else if(consecutive_<4) ++consecutive_;
        return consecutive_<4;
    }
    unsigned consecutive() const noexcept { return consecutive_; }
private:
    unsigned consecutive_{};
};
}
