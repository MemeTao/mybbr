#ifndef BBR_COMMON_CIRCULAR_BUFFER_H_
#define BBR_COMMON_CIRCULAR_BUFFER_H_

#include <cstdint>
#include <vector>
#include <list>

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
        ValueType value;
    };
public:
    CircularBuffer(size_t buffer_size = 1024)
    {

    }

    template <class...Args>
    void emplace(uint64_t key, Args&&...args)
    {
        size_t pos = hash(key);
        while (beyond_allocated(pos)) {
            allocate_trunk(1);
        }
        auto iter = trunk(pos);
        auto idx = index(pos);
        iter->emplace(idx, std::forward<Args>(args)...);
    }

    ElemType* get(uint64_t key);

    void erase(uint64_t key);
    //[form, to]
    void erase(uint64_t from, uint64_t to);

private:
    size_t hash(uint64_t key) //distance to elem_begin_
    {
        return key - key_flag;
    }

    bool beyond_allocated(size_t pos)
    {
        size_t allocated_elems = elems_.size() *
                elems_[0].size() - elem_begin_;
        return pos > allocated_elems;
    }

    void allocate_trunk(size_t cnt)
    {
        for (size_t i = 0; i < cnt; i++) {
            elems_.push_back(std::vector<ElemType>{});
            elems_.back().resize(elems_[0].size());
        }
    }

    typename std::list<std::vector<ElemType>>::iterator
    tunk(size_t pos)
    {
        auto iter = elems_.begin();
        const auto left = elems_[0].size() -
                elem_begin_;
        if (pos < left) {
            return iter;
        }
        auto increment = (pos-left) / elems_[0].size() + 1;
        while(increment-- > 0) {
            iter ++;
        }
        return iter;
    }

    size_t index(size_t pos) {
        const auto left = elems_[0].size() -
                elem_begin_;
        if(pos < left) {
            return pos + elem_begin_;
        }
        return (pos - left) % elems_[0].size();
    }
private:
    //[begin, end)
    size_t elem_begin_ = 0;
    size_t elem_end_ = 0;

    uint64_t key_flag = 0;

    size_t trunk_begin_ = 0;
    size_t trunk_end_ = 0;

    std::list<std::vector<ElemType>> elems_;
};
}
}
#endif
