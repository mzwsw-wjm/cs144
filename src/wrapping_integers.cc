#include "wrapping_integers.hh"
#include <algorithm>

using namespace std;

Wrap32 Wrap32::wrap(uint64_t n /* absolute seqno */, Wrap32 zero_point /* ISN */)
{
    //Convert absolute seqno → seqno.
    return Wrap32 { zero_point + static_cast<uint32_t>(n) };
}

uint64_t Wrap32::unwrap(Wrap32 zero_point, uint64_t checkpoint) const
{
    //Convert seqno → absolute seqno.
    auto seq = static_cast<uint64_t>(raw_value_ - zero_point.raw_value_);
    const uint64_t offset = static_cast<uint64_t>(1) << 32;
    if(checkpoint <= seq) { return seq; }
    uint64_t p_checkpoint = checkpoint / offset;
    seq = seq + p_checkpoint * offset;
    if(checkpoint <= seq) {
        auto up_seq = seq - offset;
        if(checkpoint - up_seq < seq - checkpoint) { seq = up_seq; }
    }
    else {
        auto up_seq = seq + offset;
        if(up_seq > seq && up_seq-checkpoint < checkpoint-seq) { seq = up_seq; }
    }
    return seq;
}
