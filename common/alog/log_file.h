#ifndef COMMON_ALOG_LOG_FILE_H_
#define COMMON_ALOG_LOG_FILE_H_
#include <string>
#include <map>
#include <vector>
#include <memory>

namespace rtc
{
namespace common
{

//使用标准IO库，不使用系统调用
//因为是单线程,用write_unlocked替换write()
/*FIXME: 日志文件中途如果被删除
 * */
class AppendFile{
public:
    AppendFile(std::string& name);
    ~AppendFile();
    void append(const char* logline,const size_t size); 
    int flush();
    void close();
    off_t written_bytes() const { return written_bytes_;}
private:
    FILE* pfile_;
    char  buffer_[64*1024];  //64K:用户态标准IO库的缓冲区
    off_t written_bytes_;
};

class LogFile{
public:
    LogFile(const std::string& file_path,uint64_t roll_size);
    ~LogFile();
    void append(const char* content,size_t size);
    void flush();
    void roll_file();
private:
    std::string file_path_;
    uint64_t    roll_size_;
    std::unique_ptr<AppendFile> file_;
};

}
}
#endif
