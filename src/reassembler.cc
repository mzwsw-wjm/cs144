#include "reassembler.hh"
#include <cassert>
#include <cmath>
#include <iostream>
#include <utility>

using namespace std;

bool Reassembler::is_closed() const { return closed_ && (bytes_pending() == 0); }

void Reassembler::insert(uint64_t first_index, string data, bool is_last_substring, Writer &output)
{
    if(is_last_substring) { closed_ = true; }

    //cout << "--------------------------" << endl;//debug
    //cout << "This time, the data is : " << data << endl;//debug

    //数据域
    auto start = first_index;
    auto end = first_index + data.size() - 1;
    //接受域
    auto left = unassembled_index_;
    auto right = unassembled_index_ + output.available_capacity() - 1;

    //非法情况
    //后两种情况主要是为了memory limit
    if(data.empty() || end<left || start>right || output.available_capacity()==0){ 
        //cout << "Is illegal." << endl;//debug
        if(is_last_substring) {
            output.close();
        }
        return; 
    }

    //合法情况，开始切数据往map里面扔,写入的操作被统一到插入之后
    //1: start left end right
    //2: start left right end
    //3: left start end right
    //4: left start right end

    auto first = max(start, left);
    auto last = min(end, right);
    auto elenum = last - first + 1;//有效数据字符数
    data = data.substr(first-start, elenum);

    //cout << "first is : " << first << endl;//debug
    //cout << "last is : " << last << endl;//debug
    //cout << "elenum is : " << elenum << endl;//debug
    //cout << "The final data is : " << data << endl;//debug

    //map有可能是空的,是空的就直接插进去就好了
    if(unassembled_substrings_.empty()) {
        //cout << "Is empty." << endl;//debug
        unassembled_bytes_ += data.size();//因为下一行移动了data，所以这两行的顺序不能颠倒
        unassembled_substrings_.insert(make_pair(first, move(data)));
    }

    //map非空
    /*
    前两种情况里start<left，也就一定小于map中的任意元素
    而这个时候我们可以把begin抽象成lower_bound，这样就
    将四种情况统一了
    */
    else {
    auto it = unassembled_substrings_.lower_bound(first);
    if(it == unassembled_substrings_.end()) {
        //cout << "It == end." << endl;//debug
        //即此段大于map中所有元素
        /*
        要考虑到虽然大于所有的index，但是每一个index其实是一段数据
        */
        auto front_it  = --it;
        auto front_first = front_it->first;
        auto front_last = front_first + front_it->second.size() - 1;
        /*
        这里有三种情况：
        last <= front_last           --> 不插入
        first > front_last+1         --> 完整插入
        first <= fornt_last < last   --> 局部插入
        */
        if(first > front_last+1) {//完整插入
            unassembled_bytes_ += data.size();
            unassembled_substrings_.insert(make_pair(first, move(data)));
            
        }
        if(first <= front_last+1 && front_last < last) {//局部插入
            data = data.substr(front_last-first+1, last-front_last);
            unassembled_bytes_ += data.size();
            front_it->second.append(data);
        }
    }
    else {
        /*
        从这里开始lower_bound算是找到了，于是也就可以
        继续向前寻找，问题在于it是不是begin
        */
        //cout << "It != end." << endl;//debug
        if(it != unassembled_substrings_.begin()) {
            //cout << "It != begin." << endl;//debug
            auto front_it  = --it;
            //cout << "front_it's element is : " << front_it->second << endl;//debug
            auto front_first = front_it->first;
            auto front_last = front_first + front_it->second.size() - 1;
            //cout << "front_first is : " << front_first << endl;//debug
            //cout << "front_last is : "  << front_last << endl;//debug
            /*
            跟上一个元素的关系有三种：
            1.不相关
            2.部分交叉
            3.全包含
            */
            if(first > front_last+1) {//紧邻也应该合并
                //cout << "The relation with former : 1."<< endl;//debug
                it++;
            }
            if(first <= front_last+1 && last > front_last) {
                /*
                在这一段我们需要对first，即插入数据进行修改
                是因为数据和之前已经插入map的数据发生了融合
                即便数据变长了，但是前半部分的元素已经都插入到
                map中过了，所以这种变长是被允许的。原来的数据被擦除
                相当于得到了一个新的待插入段
                */
                //cout << "The relation with former : 2."<< endl;//debug
                unassembled_bytes_ -= front_it ->second.size();
                data = front_it->second.append(data.substr(front_last-first+1, last-front_last));
                //cout << "With former element, the data is : " << data << endl;//debug
                
                unassembled_substrings_.erase(it++);
                first = front_first;
            }
            if(first <= front_last+1 && last <= front_last) {
                //cout << "The relation with former : 3."<< endl;//debug
                data = front_it->second;
                unassembled_bytes_ -= it->second.size();
                unassembled_substrings_.erase(it++);
                first = front_first;
                last = front_last;
            }
        }

        for(; it != unassembled_substrings_.end(); ) {
            //cout << "The for loop is called." << endl;//debug
            auto unassembled_first = it->first;
            auto unassembled_last = unassembled_first + it->second.size() - 1;
            //cout << "unassembled_first is : " << unassembled_first << endl;//debug
            //cout << "unassembled_last is : " << unassembled_last << endl;//debug
            if(last+1 < unassembled_first) {//紧邻也是某种连接
                //cout << "If condition 1." << endl;//debug
                unassembled_bytes_ += data.size();
                unassembled_substrings_.insert(make_pair(first, move(data)));
                break;
            }
            if(last >= unassembled_last) {
                //cout << "If condition 2." << endl;//debug
                unassembled_bytes_ -= it->second.size();
                data = data.substr(0,unassembled_first-first) + it->second + data.substr(unassembled_last-first+1,last-unassembled_last);
                unassembled_substrings_.erase(it++);
                //unassembled_substrings_.insert(make_pair(first, move(data)));
                //如果更新后的it指向了end，此时不得不进行插入
                if(it == unassembled_substrings_.end()) {
                    unassembled_bytes_ += data.size();
                    unassembled_substrings_.insert(make_pair(first, move(data)));
                }
            }
            else {
                //cout << "If condition 3." << endl;//debug
                unassembled_bytes_ -= it->second.size();
                /*
                如果last和unassembled_first相等，意味着该处的元素已经在data中了，不需要append
                last所指位置的元素并不包含，所以元素个数处无需＋1*/
                //data = data.append(it->second.substr(last-unassembled_first+1, unassembled_last-last));
                //尊重旧元素
                data = data.substr(0,unassembled_first-first).append(it->second);
                unassembled_bytes_ += data.size();
                unassembled_substrings_.erase(it++);
                unassembled_substrings_.insert(make_pair(first, move(data)));
                last = unassembled_last;
                break;
            }
        }
    }
    }
    //至此插入结束，开始写入
    //cout << "unassembled_index_ is : " << unassembled_index_ << endl;//debug
    auto p = unassembled_substrings_.find(unassembled_index_);
    if(p != unassembled_substrings_.end()) {
        //cout << "data : " << p->second << endl;//debug
        output.push(p->second);
        unassembled_index_ += p->second.size();
        unassembled_bytes_ -= p->second.size();
        unassembled_substrings_.erase(p);
    }
    //cout << "new unassembled_index_ is : " << unassembled_index_ << endl;//debug
    //写入完成，关闭流
    if(is_closed()) {
        output.close();
    }
}
uint64_t Reassembler::bytes_pending() const { return unassembled_bytes_; }