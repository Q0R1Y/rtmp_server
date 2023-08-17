#include <rss_core_log.hpp>

#include <string.h>
#include <sys/time.h>

#include <string>
#include <map>

#include <st.h>

ILogContext::ILogContext()
{
}

ILogContext::~ILogContext()
{
}

class LogContext : public ILogContext
{
private:
	class DateTime
	{
	private:
	    // %d-%02d-%02d %02d:%02d:%02d.%03d
	    #define DATE_LEN 24
	    char time_data[DATE_LEN];
	public:
	    DateTime();
	    virtual ~DateTime();
	public:
	    virtual const char* format_time();
	};
private:
    DateTime time;
    std::map<st_thread_t, int> cache;
public:
    LogContext();
    virtual ~LogContext();
public:
    virtual void generate_id();
    virtual int get_id();
public:
    virtual const char* format_time();
};

ILogContext* log_context = new LogContext();

LogContext::DateTime::DateTime()
{
    memset(time_data, 0, DATE_LEN);
}

LogContext::DateTime::~DateTime()
{
}

const char* LogContext::DateTime::format_time()
{
    // clock time
    timeval tv;
    if (gettimeofday(&tv, NULL) == -1) {
        return "";
    }
    // to calendar time
    struct tm* tm;
    if ((tm = localtime(&tv.tv_sec)) == NULL) {
        return "";
    }
    
    // log header, the time/pid/level of log
    // reserved 1bytes for the new line.
    snprintf(time_data, DATE_LEN, "%d-%02d-%02d %02d:%02d:%02d.%03d", 
        1900 + tm->tm_year, 1 + tm->tm_mon, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, 
        (int)(tv.tv_usec / 1000));
        
    return time_data;
}

LogContext::LogContext()
{
}

LogContext::~LogContext()
{
}

void LogContext::generate_id()
{
	static int id = 1;
    cache[st_thread_self()] = id++;
}

int LogContext::get_id()
{
    return cache[st_thread_self()];
}

const char* LogContext::format_time()
{
    return time.format_time();
}

