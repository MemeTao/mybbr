#ifndef COMMON_ALOG_LOGSTREAM_H_
#define COMMON_ALOG_LOGSTREAM_H_
#include <stdint.h>
#include <string>
#include <vector>
#include <cstring>
#include <algorithm>

namespace rtc{
namespace  common{

template<int SIZE>
class FixedBuffer{
public:
    FixedBuffer() : cur_ (data_) {}
    void append(const char* content,size_t len)
    {   
        size_t size = std::min(valid(),len);
        memcpy(cur_,content,size);
        cur_ += size;
    }
    const char* data() { return data_;}
    inline void clear(){ cur_ = data_;}
    inline size_t len(){ return  static_cast<size_t>(cur_-data_);}
    inline size_t valid(){ return static_cast<size_t>(end() - cur_);}
private:
    const char* end() { return data_ + sizeof(data_);}
    char data_[SIZE];
    char* cur_;
};

class LogStream{
    using Self   = LogStream;
public:
    static const uint32_t kSmallBuffer = 1024 * 4;
    static const uint32_t kLargeBuffer = kSmallBuffer * 1024;
    using Buffer = FixedBuffer<kSmallBuffer>;
    ~LogStream();

    template<typename T>
    Self& operator << (const T& v)
    {
        (*this) << v.to_log();
        return *this;
    }
    template<typename T>
    Self& operator <<(const std::vector<T>& vecs)
    {
        std::string logs;
        for(const auto& log : vecs){
            logs = logs + log.to_log();
            logs.push_back('\n');
        }
        (*this) << logs;
        return *this;
    }
    Self& operator <<(const std::vector<char>& vecs)
    {
        buffer_.append(vecs.data(),vecs.size());
        return *this;
    }
    Self& operator << (const char* content);
    Self& operator << (const std::string&);
    Self& operator << (const std::vector<std::string>&);
    Self& operator << (short);
    Self& operator << (unsigned short);
    Self& operator << (int);
    Self& operator << (unsigned int);
    Self& operator << (long);
    Self& operator << (unsigned long);
    Self& operator << (long long);
    Self& operator << (unsigned long long);
    Self& operator << (float v) 
    {
        *this << static_cast<double>(v);
        return *this;
    } 
    Self& operator << (double);
    Self& operator << (char v)
    {
        buffer_.append(&v,1);
        return *this;
    }
    Self& operator << (uint8_t v);
    Buffer& buffer(){ return buffer_;}
    template<typename T>
    inline void formatInteger(const T );
private:
    Buffer buffer_;
};

}
}
#endif
