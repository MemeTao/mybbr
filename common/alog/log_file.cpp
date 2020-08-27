#include <rtc/common/alog/log_file.h>
#include <stdio.h>
#include <stdlib.h>
#include <cassert>
#include <algorithm>

namespace rtc{
namespace common
{
    
AppendFile::AppendFile(std::string& name)
#ifdef __unix__
    :pfile_(::fopen(name.c_str(),"ae")),//e: glibc extension
#endif
#if defined (_WIN32) || defined(WIND32)
    :pfile_(::fopen(name.c_str(),"a")),
#endif
    written_bytes_(0)
{
    assert(pfile_ != nullptr);
#ifdef __unix__
    ::setbuffer(pfile_, buffer_, sizeof buffer_);
#endif
#if defined (_WIN32) || defined(WIND32)
    //windows下好像没有setbuffer
    ::setvbuf(pfile_,buffer_,_IOFBF,sizeof buffer_);
#endif
}

AppendFile::~AppendFile()
{
    if(pfile_){
        ::fclose(pfile_);
        pfile_ = nullptr;
    }
}

void AppendFile::close()
{
    if(pfile_){
        ::fclose(pfile_);
        pfile_ = nullptr;
    }
}
void AppendFile::append(const char* logline,const size_t len)
{
    //size_t n = ::fwrite_unlocked(logline,1,len,pfile_);
    size_t n = ::fwrite(logline,1,len,pfile_);
    size_t remain = len - n;
    while (remain > 0 && n)
    {
        //size_t x = ::fwrite_unlocked(logline + n,1,remain,pfile_);
        size_t x = ::fwrite(logline + n,1,remain,pfile_);
        if (x == 0)
        {
            int err = ::ferror(pfile_);
            if (err)
                break;
        }
        n += x;
        remain = len - n; // remain -= x
    }
    written_bytes_ += static_cast<off_t>(len);
}
int AppendFile::flush()
{
    //return ::fflush_unlocked(pfile_);
    return ::fflush(pfile_);
}

LogFile::LogFile(const std::string& file_path,uint64_t roll_size)
    :file_path_(file_path),
    roll_size_(roll_size)
{
    roll_file();
}

LogFile::~LogFile()
{
    flush();
}

void LogFile::append(const char* content,size_t size)
{
    file_->append(content,size);
    if( static_cast<uint64_t>(file_->written_bytes()) > roll_size_)
    {
        roll_file();
    }
}

void LogFile::flush()
{
    int ret = file_->flush();
    (void) ret;
}

void LogFile::roll_file()
{
    std::string m_file_name = file_path_.empty() ? "rtc.log" : file_path_ + "_rtc.log";
    std::string s_file_name = file_path_.empty() ? "rtc.log" : file_path_ + "_rtc.log.bk";
    if(file_){
        file_->close();
    }
    remove(s_file_name.c_str());
    rename(m_file_name.c_str(), s_file_name.c_str());

    file_.reset(new AppendFile(m_file_name));
}

}  //namesapce
}
