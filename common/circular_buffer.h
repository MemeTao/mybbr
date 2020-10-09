#ifndef BBR_COMMON_CIRCULAR_BUFFER_H_
#define BBR_COMMON_CIRCULAR_BUFFER_H_

#include <cstdint>
#include <vector>
#include <list>
#include <atomic>

namespace bbr
{
namespace common
{
template <typename ValueType>
class CircularBuffer
{
    const size_t kDefaultTrunkSize = 2;
    struct ElemType {
        bool dead = true;
        uint64_t key = 0;
        ValueType value;
    };
public:
    CircularBuffer(size_t buffer_size = 1024) {
        elems_.resize(kDefaultTrunkSize);
        auto iter = elems_.begin();
        while(iter != elems_.end()) {
            iter->resize(buffer_size);
            iter++;
        }
    }

    template <class...Args>
    void emplace(uint64_t key, Args&&...args) {
        size_t pos = hash(key);
        while (beyond_allocated(pos)) {
            allocate_trunk(1);
        }
        auto iter = trunk(pos);
        auto idx = index(pos);
        iter->emplace(iter->begin() + idx,
                ElemType{false,
            ValueType{std::forward<Args>(args)...}});
    }

    template <class...Args>
    void emplace(uint64_t key, const Args&...args) {
        if(rotate_.load()) {
            rotate();
        }
        size_t pos = hash(key);
        while (beyond_allocated(pos)) {
            allocate_trunk(1);
        }
        auto iter = trunk(pos);
        auto idx = index(pos);
        iter->emplace(iter->begin() + idx,
                ElemType{false, key, ValueType{args...}});
    }

    ValueType* get(uint64_t key) {
        size_t pos = hash(key);
        auto iter = trunk(pos);
        if(iter == elems_.end()){
            return nullptr;
        }
        auto idx = index(pos);
        return &(*iter)[idx].value;
    }

    void erase(uint64_t key) {
        size_t pos = hash(key);
        auto iter = trunk(pos);
        if(iter == elems_.end()) {
            return;
        }
        auto idx = index(pos);
        (*iter)[idx].dead = true;
    }

    //[form, to)
    void erase(uint64_t from, uint64_t to) {
        size_t pos = hash(from);
        auto iter = trunk(pos);
        if(iter == elems_.end()) {
            return;
        }
        auto idx = index(pos);
        while((*iter)[idx].key < to &&
                iter != elems_.end()) {
            (*iter)[idx].dead = true;
            idx++;
            if(idx == iter->size()) {
                idx = 0;
                ++ iter;
            }
        }
    }

private:
    size_t hash(uint64_t key) //distance to elem_begin_
    {
        return key - key_flag;
    }

    bool beyond_allocated(size_t pos) {
        size_t allocated_elems = elems_.size() *
                elems_.front().size() - elem_begin_;
        return pos > allocated_elems;
    }

    void allocate_trunk(size_t cnt) {
        for (size_t i = 0; i < cnt; i++) {
            elems_.push_back(std::vector<ElemType>{});
            elems_.back().resize(elems_.front().size());
        }
    }

    typename std::list<std::vector<ElemType>>::iterator
    trunk(size_t pos) {
        auto iter = elems_.begin();
        const auto left = elems_.front().size() -
                elem_begin_;
        if (pos < left) {
            return iter;
        }
        auto increment = (pos-left) / elems_.front().size() + 1;
        while(increment-- > 0 && iter != elems_.end()) {
            iter ++;
        }
        return iter;
    }

    size_t index(size_t pos) {
        const auto left = elems_.front().size() -
                elem_begin_;
        if(pos < left) {
            return pos + elem_begin_;
        }
        return (pos - left) % elems_.front().size();
    }

    void rotate() {
        if(elem_rotate_ <= elem_begin_) {
            return;
        }
        while(elem_rotate_ >= elems_.front().size()) {
            auto& elem_del = elems_.front();
            elems_.push_back(std::move(elem_del));
            elems_.pop_front();
            elem_rotate_ -= elems_.front().size();
        }
        elem_begin_ = index(elem_rotate_);
        key_flag = elems_.front()[elem_begin_].dead ? 0 :
                elems_.front()[elem_begin_].key;
        elem_rotate_ = 0;
    }

private:
    size_t elem_begin_ = 0;
    size_t elem_rotate_ = 0; //[begin, rotate_] will de deleted, 'begin' pointer to 'rotate_'
    uint64_t key_flag = 0;

    std::list<std::vector<ElemType>> elems_;

    std::atomic<bool> rotate_ = false;
};
}
}
#endif
