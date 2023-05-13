#include "reassembler.hh"
#include <iostream>
#include <math.h>
#include <assert.h>

using namespace std;

bool Reassembler::is_closed() const {
    return closed_ && bytes_pending() == 0;
}

void Reassembler::insert(uint64_t first_index, string data, bool is_last_substring, Writer &output)
{
    if (is_last_substring) {
        closed_ = true;
    }
    
    cout << "available_capacity: " << output.available_capacity() << endl;
    cout << "Substring index ["<< first_index << "," << first_index + data.length() - 1 << "]" << endl;
    cout << "unassembled index: " << unassembled_index_ << "\tdata: " << data << endl;

    // Remember index_ points to where the current byte located at.
    // 1. Unacceptable index: first_index overwhelms the capability range. 
    // 2. All overlapped: The end index of the substring is smaller than current index_.
    // 3. data is empty.
    // 4. No available space.
    if (first_index >= unassembled_index_ + output.available_capacity() || 
        first_index + data.length() - 1 < unassembled_index_ ||
        data.empty() || output.available_capacity() == 0) {
        if(is_closed()) {
            output.close();
        }
        return;
    }
    std::cout << "pass the first checker" << std::endl;

    uint64_t cap = output.available_capacity();
    uint64_t new_index = first_index;
    if (first_index <= unassembled_index_) {
        new_index = unassembled_index_;
        uint64_t overlapped_length = unassembled_index_ - first_index;
        data = std::move(data.substr(overlapped_length, min(data.size() - overlapped_length, cap)));
        cout << "Processed data: " << data << endl;
        // Get the rear substring and merge the overlapped part
        auto iter = unassembled_substrings_.upper_bound(unassembled_index_);
        while (iter != unassembled_substrings_.end()) {
            const auto & [rear_index, rear_data] = *iter;
            cout << "Upper bound exists." << endl;
            cout << "Rear Index: " << rear_index << "\tRear Data: " << rear_data << endl;
            int64_t rear_overlapped_length = unassembled_index_ + data.size() - rear_index;
            cout << "Overlapped length of rear part and current part: " << rear_overlapped_length << endl;
            if (rear_overlapped_length <= 0) {
                break;
            }
            unassembled_substrings_.erase(rear_index);
            data.append(rear_data.substr(rear_overlapped_length));
            cout << "Current data: " << data << endl;
            iter++;
        }
    } else if (first_index > unassembled_index_) {
        auto rear_iter = unassembled_substrings_.upper_bound(new_index);
        while (rear_iter != unassembled_substrings_.end()) {
            const auto & [rear_index, rear_data] = *rear_iter;
            // no overlap
            if (first_index + data.size() - 1 < rear_index) {
                break;
            } else {
                uint64_t overlapped_length = first_index + data.size() - rear_index;
                unassembled_substrings_.erase(rear_index);
                data.append(rear_data.substr(overlapped_length));
            }
            rear_iter++;
        }


        auto front_iter = unassembled_substrings_.lower_bound(new_index);
        while (front_iter != unassembled_substrings_.begin()) {
            const auto & [front_index, front_data] = *front_iter;
            // no overlap
            if (front_index + front_data.size() - 1 < front_index) {
                break;
            } else {
                int64_t overlapped_length = front_index + front_data.size() - first_index;
                assert(overlapped_length >= 0);
                unassembled_substrings_.erase(front_index);
                data = std::move(std::move(front_data) + std::move(data.substr(overlapped_length)));
            }
            front_iter--;
        }
    }
    unassembled_bytes_ += data.size();
    unassembled_substrings_[new_index] = std::move(data);

    cout << "Pending Bytes: " << bytes_pending() << endl;
    
    for (auto iter = unassembled_substrings_.begin(); iter != unassembled_substrings_.end();) {
        const auto & [sub_index, sub_data] = *iter;
        if (sub_index == unassembled_index_) {
            unassembled_index_ += sub_data.size();
            unassembled_bytes_ -= sub_data.size();
            output.push(sub_data);
            unassembled_substrings_.erase(sub_index);
            iter = unassembled_substrings_.find(unassembled_index_);
        } else {
            break;
        }
    }

    if(is_closed()) {
        output.close();
    }
}

uint64_t Reassembler::bytes_pending() const
{
    return unassembled_bytes_;
}
