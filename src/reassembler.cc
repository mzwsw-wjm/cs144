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
    // cout << "unassembled index: " << unassembled_index_ << "\tdata: " << data << endl;
    cout << "unassembled index: " << unassembled_index_ << endl;

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
        // cout << "Processed data: " << data << endl;
        // Get the rear substring and merge the overlapped part
        auto iter = unassembled_substrings_.lower_bound(unassembled_index_);
        while (iter != unassembled_substrings_.end()) {
            auto & [rear_index, rear_data] = *iter;
            if (rear_index < unassembled_index_) {
                break;
            }
            cout << "Upper bound exists." << endl;
            // cout << "Rear Index: " << rear_index << "\tRear Data: " << rear_data << endl;
            cout << "Rear Index: " << rear_index << endl;
            int64_t overlapped_exists = unassembled_index_ + data.size() - rear_index;
            if (overlapped_exists <= 0) {
                break;
            }
            uint64_t rear_overlapped_length = 0;
            if (unassembled_index_ + data.size() <= rear_index + rear_data.size()) {
                rear_overlapped_length = unassembled_index_ + data.size() - rear_index;
            } else {
                rear_overlapped_length = rear_data.size();
            }
            cout << "Overlapped length of rear part and current part: " << rear_overlapped_length << endl;
            // cout << "Overlapped part: " << rear_data.substr(0, rear_overlapped_length) << endl;
            uint64_t next_rear = rear_index + rear_data.size();
            if (rear_overlapped_length == rear_data.size()) {
                unassembled_bytes_ -= rear_data.size();
                cout << "Substring erased. \tindex: " << rear_index << "\t rear_data: " 
                        << rear_data << "\t bytes_pending: " << bytes_pending() << endl;
                // cout << "Substring erased. \tindex: " << rear_index << "\t bytes_pending: " << bytes_pending() << endl;
                unassembled_substrings_.erase(rear_index);
            } else {
                // We don't combine current data and rear data. 
                // Erase the overlapped part in current data is more efficient.
                data.erase(data.end() - rear_overlapped_length, data.end());
            }
            // cout << "Current data: " << data << endl;
            iter = unassembled_substrings_.lower_bound(next_rear);
        }
    } else if (first_index > unassembled_index_) {
        auto rear_iter = unassembled_substrings_.upper_bound(new_index);
        if (rear_iter == unassembled_substrings_.end()) {
            cout << "No rear data" << endl;
        }
        while (rear_iter != unassembled_substrings_.end()) {
            auto & [rear_index, rear_data] = *rear_iter;
            if (rear_index < first_index) {
                break;
            }
            cout << "Rear index: " << rear_index << endl;
            // no overlap
            uint64_t overlapped_length = 0; 
            if (first_index + data.size() - 1 < rear_index) {
                break;
            } else {
                uint64_t next_rear = rear_data.size() + rear_index;
                if (first_index + data.size() <= rear_index + rear_data.size()) {
                    overlapped_length = first_index + data.size() - rear_index;
                } else {
                    overlapped_length = rear_data.size();
                }

                if (overlapped_length == rear_data.size()) {
                    unassembled_bytes_ -= rear_data.size();
                    unassembled_substrings_.erase(rear_index);
                } else {
                    data.erase(data.end() - overlapped_length, data.end());
                }
                rear_iter = unassembled_substrings_.lower_bound(next_rear);
            }
        }

        auto front_iter = unassembled_substrings_.upper_bound(new_index);
        if (front_iter != unassembled_substrings_.begin()) {
            front_iter--;
            const auto & [front_index, front_data] = *front_iter;
            cout << "Front index: " << front_index << endl;
            // no overlap
            if (front_index + front_data.size() - 1 < first_index) {
                ;
            } else {
                uint64_t overlapped_length = 0;
                if (front_index + front_data.size() <= first_index + data.size()) {
                    overlapped_length = front_index + front_data.size() - first_index;
                } else {
                    overlapped_length = data.size();
                }
                cout << "Overlapped length: " << overlapped_length << endl;
                if (overlapped_length == front_data.size()) {
                    unassembled_bytes_ -= front_data.size();
                    unassembled_substrings_.erase(front_index);
                } else {
                    data.erase(data.begin(), data.begin() + overlapped_length);
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

    cout << "Pending Bytes: " << bytes_pending() << endl;
    
    for (auto iter = unassembled_substrings_.begin(); iter != unassembled_substrings_.end(); /* nop */) {
        const auto & [sub_index, sub_data] = *iter;
        if (sub_index == unassembled_index_) {
            unassembled_index_ += sub_data.size();
            unassembled_bytes_ -= sub_data.size();
            cout << "[" << sub_index << "," << sub_index + sub_data.size() - 1 << "] pushed!" << endl;
            uint64_t prev_byte_pushed = output.bytes_pushed();
            output.push(sub_data);
            if (output.bytes_pushed() == prev_byte_pushed) {
                break; // No space left
            }
            unassembled_substrings_.erase(sub_index);
            iter = unassembled_substrings_.find(unassembled_index_);
            if (iter != unassembled_substrings_.end()) {
                cout << "Next iter_index: " << iter->first << endl;
            } else {
                cout << "No more will be pushed" << endl;
            }
        } else {
            break;
        }
    }
    cout << "Pending Bytes After: " << bytes_pending() << endl;
    cout << "Pushed bytes: " << output.bytes_pushed() << "\n" << endl;

    if(is_closed()) {
        output.close();
    }
}

uint64_t Reassembler::bytes_pending() const
{
    return unassembled_bytes_;
}
