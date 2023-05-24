#include "wrapping_integers.hh"

using namespace std;

Wrap32 Wrap32::wrap(uint64_t n /* absolute seqno */, Wrap32 zero_point /* ISN */)
{
    return zero_point + static_cast<uint32_t>(n);
}

uint64_t Wrap32::unwrap(Wrap32 zero_point, uint64_t checkpoint) const
{
    uint32_t seqno_offset = this->raw_value_ - zero_point.raw_value_;
    /* checkpoint > offset we just need to find the nearest one */
    if (checkpoint > seqno_offset) {
        constexpr uint64_t UINT32_SIZE = 1l << 32;
        uint64_t abs_seqno_extra_part_offset = checkpoint - seqno_offset + (UINT32_SIZE >> 1);
        uint64_t UINT32_SIZE_num = abs_seqno_extra_part_offset / UINT32_SIZE;
        return UINT32_SIZE_num * UINT32_SIZE + seqno_offset;
    } else {
        return seqno_offset;
    }
}
