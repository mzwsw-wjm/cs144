#include "buffer.hh"
#include "tcp_config.hh"
#include "tcp_sender.hh"
#include "tcp_sender_message.hh"

#include <cstdint>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <utility>

using namespace std;

/* TCPSender constructor (uses a random ISN if none given) */
TCPSender::TCPSender(uint64_t initial_RTO_ms, optional<Wrap32> fixed_isn) 
    : isn_(fixed_isn.value_or(Wrap32(static_cast<uint32_t>(random_device()())))), initial_RTO_ms_(initial_RTO_ms)
{}

uint64_t TCPSender::sequence_numbers_in_flight() const { return outstanding_seqno_; }

uint64_t TCPSender::consecutive_retransmissions() const { return consecutive_retransmission_times_; }

optional<TCPSenderMessage> TCPSender::maybe_send()
{
    if(!segments_out_.empty() && set_syn_) {
        auto element = segments_out_.front();
        segments_out_.pop();
        return make_optional<TCPSenderMessage>(element);
    }
    return nullopt;
}

void TCPSender::push(Reader &outbound_stream)
{
    auto window_in_push = (window_size_) ? window_size_ : 1;
    while(window_in_push > outstanding_seqno_) {
        TCPSenderMessage message;
        if(!set_syn_) { message.SYN = set_syn_ = true; }
        message.seqno = get_next_seqno();
        auto payload_size = min(window_in_push-outstanding_seqno_-message.SYN, TCPConfig::MAX_PAYLOAD_SIZE);
        /*
        因为这里用了string_view，所以必须在Buffer之后才能调pop
        而只有pop之后才能去判断FIN，所以这几行代码有固定的顺序
        */
        auto payload = outbound_stream.peek().substr(0, payload_size);
        message.payload = Buffer(string(payload));
        outbound_stream.pop(payload_size);
        //substr未必真能返回n个元素，所以具体数目根据size函数确定
        if(!set_fin_ && outbound_stream.is_finished() && payload.size() + outstanding_seqno_ + message.SYN < window_in_push) { message.FIN = set_fin_ = true; }
        if(message.sequence_length() == 0) { break; }
        if(outstanding_seg_.empty()) {
            timer_ = 0;
            RTO_timeout_ = initial_RTO_ms_;
        }
        segments_out_.push(message);
        outstanding_seqno_ += message.sequence_length();
        outstanding_seg_.insert(make_pair(next_abs_seqno_, message));
        next_abs_seqno_ += message.sequence_length();
        if(message.FIN) { break; }
    }
}

TCPSenderMessage TCPSender::send_empty_message() const
{
    TCPSenderMessage message;
    message.seqno = get_next_seqno();
    return message;
}

void TCPSender::receive(const TCPReceiverMessage &msg)
{
    window_size_ = msg.window_size;
    if(msg.ackno.has_value()) {
        auto abs_index = msg.ackno.value().unwrap(isn_, next_abs_seqno_);
        if(abs_index > next_abs_seqno_) { return; }
        for(auto it = outstanding_seg_.begin(); it != outstanding_seg_.end(); ) {
            if(it->first + it->second.sequence_length() <= abs_index) {
                RTO_timeout_ = initial_RTO_ms_;
                consecutive_retransmission_times_ = 0;
                outstanding_seqno_ -= it->second.sequence_length();
                outstanding_seg_.erase(it++);
                if(!outstanding_seg_.empty()) { timer_ = 0; }
            }
            else { break; }
        }
    }
}

void TCPSender::tick(const size_t ms_since_last_tick)
{
    timer_ += ms_since_last_tick;
    if(timer_ >= RTO_timeout_) {
        auto i = outstanding_seg_.begin();
        if(i != outstanding_seg_.end()) { segments_out_.push(i->second); }
        if(window_size_ > 0) {
            RTO_timeout_ *= 2;
            consecutive_retransmission_times_++;
        }
        timer_ = 0;
    }
}
