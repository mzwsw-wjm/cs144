#include "tcp_receiver.hh"

using namespace std;

void TCPReceiver::receive(TCPSenderMessage message, Reassembler &reassembler, Writer &inbound_stream)
{
    if (!set_syn_) {
        if (!message.SYN) {
            return; // drop all data if SYN isn't received
        }
        isn_ = message.seqno; // FIN occupied one seqno
        set_syn_ = true;
    }

    uint64_t checkpoint = inbound_stream.bytes_pushed() + 1;
    uint64_t abs_seqno = message.seqno.unwrap(isn_, checkpoint);
    // unwrap function starts from isn_, which occupies one seqno.
    // We calculate one index more so we need to minus it.
    // But if SYN exists in this message, compensation is needed.
    uint64_t stream_index = abs_seqno - 1 + message.SYN;
    reassembler.insert(stream_index, message.payload.release(), message.FIN, inbound_stream);
}

TCPReceiverMessage TCPReceiver::send(const Writer &inbound_stream) const
{
    TCPReceiverMessage recv_msg {};

    uint16_t window_size
        = inbound_stream.available_capacity() > UINT16_MAX ? UINT16_MAX : inbound_stream.available_capacity();
    if (!set_syn_) {
        return {std::optional<Wrap32> {}, window_size};
    }
    // add one ISN(SYN) length
    uint64_t abs_ackno_offset = inbound_stream.bytes_pushed() + 1;
    if (inbound_stream.is_closed()) {
        abs_ackno_offset++; // add one FIN
    }
    recv_msg.ackno = isn_ + abs_ackno_offset;
    recv_msg.window_size = window_size;

    return recv_msg;
}