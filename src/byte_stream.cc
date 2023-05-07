#include <stdexcept>

#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream(uint64_t capacity) :
    capacity_(capacity), pushed_len_(0), popped_len_(0), closed_(false), error_(false) {}

void Writer::push(string data)
{
    /* Throw away the incoming data */
    if (error_ || is_closed() || available_capacity() <= 0 || data.empty()) {
        return;
    }

    uint64_t len = std::min(data.length(), available_capacity());
    queue_.append(data.substr(0, len));
    pushed_len_ += len;
}

void Writer::close()
{
    closed_ = true;
}

void Writer::set_error()
{
    error_ = true;
}

bool Writer::is_closed() const
{
    return closed_;
}

uint64_t Writer::available_capacity() const
{
    return capacity_ - pushed_len_ + popped_len_;
}

uint64_t Writer::bytes_pushed() const
{
    return pushed_len_;
}

string_view Reader::peek() const
{
    return string_view(queue_.begin(), queue_.end());
}

bool Reader::is_finished() const
{
    return closed_ && queue_.empty();
}

bool Reader::has_error() const
{
    return error_;
}

void Reader::pop(uint64_t len)
{
    if (queue_.empty()) {
        return;
    } 
    
    len = std::min(len, queue_.size());
    queue_.erase(queue_.begin(), queue_.begin() + len);
    popped_len_ += len;
}

uint64_t Reader::bytes_buffered() const
{
    return pushed_len_ - popped_len_;
}

uint64_t Reader::bytes_popped() const
{
    return popped_len_;
}
