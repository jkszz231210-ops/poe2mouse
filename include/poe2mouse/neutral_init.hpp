#pragma once
#include "pad_output.hpp"
namespace pm {
// Explicit setup diagnostic, never used by the realtime input path.
// Always attempt neutral restoration, even if the priming write/read fails.
template<class Send,class Wait>
bool initialize_neutral(Send send,Wait wait) {
    PadReport sample{};
    sample.lx=1024;
    const bool prime_write_ok=send(sample);
    const bool observed=prime_write_ok&&wait(sample);
    const bool reset=send(PadReport{});
    const bool neutral=reset&&wait(PadReport{});
    return observed&&neutral;
}
}
