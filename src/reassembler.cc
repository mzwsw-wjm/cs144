#include "reassembler.hh"
#include <cassert>
#include <cmath>
#include <iostream>

using namespace std;

bool Reassembler::is_closed() const { return closed_ && bytes_pending() == 0; }

void Reassembler::insert(uint64_t first_index, string data, bool is_last_substring, Writer &output)
{
    if (is_last_substring) {
        closed_ = true;
    }

    // Remember index_ points to where the current byte located at.
    // 1. Unacceptable index: first_index overwhelms the capability range.
    // 2. All overlapped: The end index of the substring is smaller than current index_.
    // 3. data is empty.
    // 4. No available space.
    if (first_index >= unassembled_index_ + output.available_capacity() || /* Out of bound */
        first_index + data.length() - 1 < unassembled_index_ ||            /* Data have been transferred */
        data.empty() || output.available_capacity() == 0) {
        if (is_closed()) {
            output.close();
        }
        return;
    }

    const uint64_t cap = output.available_capacity();
    // new_index actually distinguish where the current data start, the start index
    uint64_t new_index = first_index;

    // Data needs to fit the capability limitation
    if (first_index <= unassembled_index_) {
        new_index = unassembled_index_;
        const uint64_t overlapped_length = unassembled_index_ - first_index;
        data = data.substr(overlapped_length, min(data.size() - overlapped_length, cap));
    } else {
        data = data.substr(0, min(data.size(), cap));
        if (first_index + data.size() - 1 > unassembled_index_ + cap - 1) {
            data = data.substr(0, unassembled_index_ + cap - first_index);
        }
    }
    // Get the rear substring and merge the overlapped part
    auto rear_iter = unassembled_substrings_.lower_bound(new_index);
    while (rear_iter != unassembled_substrings_.end()) {
        auto &[rear_index, rear_data] = *rear_iter;
        if (new_index + data.size() - 1 < rear_index) {
            break;
        } // No overlap conflict
        uint64_t rear_overlapped_length = 0;
        if (new_index + data.size() - 1 < rear_index + rear_data.size() - 1) {
            rear_overlapped_length = new_index + data.size() - rear_index;
        } else {
            rear_overlapped_length = rear_data.size();
        }
        // Prepare for next rear early, because the data may be erased afterwards.
        const uint64_t next_rear = rear_index + rear_data.size() - 1;
        if (rear_overlapped_length == rear_data.size()) {
            unassembled_bytes_ -= rear_data.size();
            unassembled_substrings_.erase(rear_index);
        } else {
            // We don't combine current data and rear data.
            // Erase the overlapped part in current data is more efficient.
            data.erase(data.end() - static_cast<int64_t>(rear_overlapped_length), data.end());
        }
        rear_iter = unassembled_substrings_.lower_bound(next_rear);
    }

    if (first_index > unassembled_index_) {
        auto front_iter = unassembled_substrings_.upper_bound(new_index);
        if (front_iter != unassembled_substrings_.begin()) {
            front_iter--;
            const auto &[front_index, front_data] = *front_iter;
            if (front_index + front_data.size() - 1 >= first_index) {
                uint64_t overlapped_length = 0;
                if (front_index + front_data.size() <= first_index + data.size()) {
                    overlapped_length = front_index + front_data.size() - first_index;
                } else {
                    overlapped_length = data.size();
                }
                if (overlapped_length == front_data.size()) {
                    unassembled_bytes_ -= front_data.size();
                    unassembled_substrings_.erase(front_index);
                } else {
                    data.erase(data.begin(), data.begin() + static_cast<int64_t>(overlapped_length));
                    // Don't forget to update the inserted location
                    new_index = first_index + overlapped_length;
                }
            }
        }
    }

    // If the processed data is empty, no need to insert it.
    if (data.size() > 0) {
        unassembled_bytes_ += data.size();
        unassembled_substrings_.insert(make_pair(new_index, std::move(data)));
    }

    for (auto iter = unassembled_substrings_.begin(); iter != unassembled_substrings_.end(); /* nop */) {
        auto &[sub_index, sub_data] = *iter;
        if (sub_index == unassembled_index_) {
            const uint64_t prev_bytes_pushed = output.bytes_pushed();
            output.push(sub_data);
            const uint64_t bytes_pushed = output.bytes_pushed();
            if (bytes_pushed != prev_bytes_pushed + sub_data.size()) {
                // Cannot push all data, we need to reserve the un-pushed part.
                const uint64_t pushed_length = bytes_pushed - prev_bytes_pushed;
                unassembled_index_ += pushed_length;
                unassembled_bytes_ -= pushed_length;
                unassembled_substrings_.insert(make_pair(unassembled_index_, sub_data.substr(pushed_length)));
                // Don't forget to remove the previous incompletely transferred data
                unassembled_substrings_.erase(sub_index);
                break;
            }
            unassembled_index_ += sub_data.size();
            unassembled_bytes_ -= sub_data.size();
            unassembled_substrings_.erase(sub_index);
            iter = unassembled_substrings_.find(unassembled_index_);
        } else {
            break; // No need to do more. Data has been discontinuous.
        }
    }

    if (is_closed()) {
        output.close();
    }
}

uint64_t Reassembler::bytes_pending() const { return unassembled_bytes_; }
