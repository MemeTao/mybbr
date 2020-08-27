#include <rtc/common/alog/logstream.h>
#include <cstring>
#include <algorithm>

namespace rtc
{
namespace common
{

LogStream::~LogStream()
{
    ;
}

LogStream& LogStream::operator<<(const std::string& content)
{
    return (*this)<<content.c_str();
}

LogStream& LogStream::operator<<(const std::vector<std::string>& contents)
{
    for(auto& it : contents)
    {
        (*this) << it;
    }
    return *this;
}

LogStream& LogStream::operator<<(const char* content)
{
    if(content != nullptr)
    {
        buffer_.append(content, strlen(content));
    }
    else
    {
        buffer_.append("nullptr", sizeof("nullptr"));
    }
    return *this;
}

template<typename T>
inline void LogStream::formatInteger(T v)
{
    (*this) << std::to_string(v);
}

LogStream& LogStream::operator<<(uint8_t v)
{
    formatInteger(v);
    return *this;
}

LogStream& LogStream::operator<<(short v)
{
    formatInteger(v);
    return *this;
}
LogStream& LogStream::operator<<(unsigned short v)
{
    formatInteger(v);
    return *this;
}
LogStream& LogStream::operator<<(int v)
{
    formatInteger(v);
    return *this;
}
LogStream& LogStream::operator<<(unsigned int v)
{
    formatInteger(v);
    return *this;
}
LogStream& LogStream::operator<<(unsigned long v)
{ 
    formatInteger(v);
    return *this;
}
LogStream& LogStream::operator<<(long v)
{ 
    formatInteger(v);
    return *this;
}
LogStream& LogStream::operator<<(long long v)
{
    formatInteger(v);
    return *this;
}
LogStream& LogStream::operator<<(unsigned long long v)
{
    formatInteger(v);
    return *this;
}
LogStream& LogStream::operator<<(double v)
{
    //std::to_string(v);
    //保留一定的位数
    *this<<std::to_string(v);
    return *this;
}
}
}
