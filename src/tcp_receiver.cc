#include "tcp_receiver.hh"
#include "tcp_receiver_message.hh"
#include "wrapping_integers.hh"
#include <cstdint>
#include <optional>

using namespace std;

void TCPReceiver::receive(TCPSenderMessage message, Reassembler &reassembler, Writer &inbound_stream)
{
    if(!set_syn_) {
        if(!message.SYN) { return; }
        set_syn_ = true;
        isn_ = message.seqno;
        reassembler.insert(message.seqno.unwrap(isn_, inbound_stream.bytes_pushed()+1), message.payload.release(), message.FIN, inbound_stream);
    }
    if(set_syn_ && !message.SYN) {
        reassembler.insert(message.seqno.unwrap(isn_, inbound_stream.bytes_pushed()+1)-1, message.payload.release(), message.FIN, inbound_stream);
    }
}

TCPReceiverMessage TCPReceiver::send(const Writer &inbound_stream) const
{
    TCPReceiverMessage ret{};
    auto num = inbound_stream.bytes_pushed()+1;
    if(inbound_stream.is_closed()) { num++; }
    if(!set_syn_){ ret.ackno = nullopt; }
    else { ret.ackno = make_optional<Wrap32>(Wrap32::wrap(num, isn_)); }
    ret.window_size = (inbound_stream.available_capacity() < UINT16_MAX) ? inbound_stream.available_capacity() : UINT16_MAX;
    return ret;
}