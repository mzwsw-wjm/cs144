#include "reassembler.hh"
#include <iostream>

using namespace std;

bool Reassembler::is_closed() const {
    return closed_ && bytes_pending() == 0;
}

void Reassembler::insert(uint64_t first_index, string data, bool is_last_substring, Writer &output)
{
    if (is_last_substring) {
        closed_ = true;
    }

    // Remember index_ points to where the current byte located at.
    // 1. Unacceptable index: first_index overwhelms the the capability range. 
    // 2. Disorder index: The end index of the substring is smaller than current index_.
    if (first_index >= index_ + output.available_capacity() || data.empty() || first_index + data.length() - 1 < index_) {
        if(is_closed()) {
            output.close();
        }
        return;
    }
    std::cout << "pass the first checker" << std::endl;
}

uint64_t Reassembler::bytes_pending() const
{
    return pending_buffer_.size();
}
