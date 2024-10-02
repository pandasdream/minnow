#include "reassembler.hh"
#include <iostream>

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  if(is_last_substring) end_ = first_index + data.size();
  if(first_index + data.size() > start_) {
    if(first_index < start_) {
      data = data.substr(start_ - first_index);
      first_index = start_;
    }

    if(first_index < start_ + output_.writer().available_capacity()) {
      auto old_len = data.size();
      data = data.substr(0, start_ + output_.writer().available_capacity() - first_index);
      if(data.size() < old_len) is_last_substring = false;
    }
    // exceed the capacity
    else return;
    
    // insert into the list [first_index, first_index + data.size())
    auto it = sections_.begin();
    auto it1 = it;
    while(it != sections_.end()) {
      if(first_index < it->first) {
        break;
      }
      it1 = it;
      ++it;
    }

    if(it == sections_.begin() || it1->first + it1->second.size() < first_index) {
      while(it != sections_.end()) {
        if(first_index + data.size() < it->first) {
          break;
        }
        if(it->first + it->second.size() > first_index + data.size())
          data += it->second.substr(first_index + data.size() - it->first);
        it = sections_.erase(it);
      }
      sections_.insert(it, make_pair(first_index, data));
    }
    
    else {
      if(it1->first + it1->second.size() < first_index + data.size())
        it1->second += data.substr(it1->first + it1->second.size() - first_index);
      while(it != sections_.end()) {
        if(it->first <= it1->first + it1->second.size()) {
          if(it1->first + it1->second.size() < it->first + it->second.size())
            it1->second += it->second.substr(it1->first + it1->second.size() - it->first);
          it = sections_.erase(it);
        }
        else break;
      }
    }

    // can be pushed
    if(sections_.front().first == start_) {
      auto p = sections_.front();
      output_.writer().push(p.second);
      sections_.pop_front();
      start_ += p.second.size();
    }
  }
  if(end_ == start_) {
    output_.writer().close();
  }
}

uint64_t Reassembler::bytes_pending() const
{
  uint64_t bytes_num {};
  for(auto& p:sections_) {
    bytes_num += p.second.size();
  }
  return bytes_num;
}
