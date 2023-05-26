#include <stdexcept>

#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream(uint64_t capacity) : capacity_(capacity) {}

void Writer::push(const std::string &data)
{
    /* Throw away the incoming data */
    if (error_ || is_closed() || data.empty()) {
        return;
    }

    const size_t len = min(data.length(), available_capacity());
    queue_.append(data.substr(0, len));
    pushed_len_ += len;
}

void Writer::close() { closed_ = true; }

void Writer::set_error() { error_ = true; }

bool Writer::is_closed() const { return closed_; }

uint64_t Writer::available_capacity() const { return capacity_ - pushed_len_ + popped_len_; }

uint64_t Writer::bytes_pushed() const { return pushed_len_; }

string_view Reader::peek() const { return {queue_}; }

bool Reader::is_finished() const { return closed_ && queue_.empty(); }

bool Reader::has_error() const { return error_; }

void Reader::pop(uint64_t len)
{
    if (queue_.empty()) {
        return;
    }

    len = std::min(len, queue_.size());
    queue_.erase(queue_.begin(), queue_.begin() + static_cast<int64_t>(len));
    popped_len_ += len;
}

uint64_t Reader::bytes_buffered() const { return pushed_len_ - popped_len_; }

uint64_t Reader::bytes_popped() const { return popped_len_; }
