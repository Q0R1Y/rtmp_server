#ifndef RSS_CORE_LOG_HPP
#define RSS_CORE_LOG_HPP

/*
#include <rss_core_log.hpp>
*/

#include <rss_core.hpp>

#include <stdio.h>

#include <errno.h>
#include <string.h>

// the context for multiple clients.
class ILogContext
{
public:
	ILogContext();
	virtual ~ILogContext();
public:
	virtual void generate_id() = 0;
	virtual int get_id() = 0;
public:
	virtual const char* format_time() = 0;
};

// user must implements the LogContext and define a global instance.
extern ILogContext* log_context;

// donot print method
#if 0
	#define rss_verbose(msg, ...) printf("[%s][%d][verbs] ", log_context->format_time(), log_context->get_id());printf(msg, ##__VA_ARGS__);printf("\n")
	#define rss_info(msg, ...)    printf("[%s][%d][infos] ", log_context->format_time(), log_context->get_id());printf(msg, ##__VA_ARGS__);printf("\n")
	#define rss_trace(msg, ...)   printf("[%s][%d][trace] ", log_context->format_time(), log_context->get_id());printf(msg, ##__VA_ARGS__);printf("\n")
	#define rss_warn(msg, ...)    printf("[%s][%d][warns] ", log_context->format_time(), log_context->get_id());printf(msg, ##__VA_ARGS__);printf(" errno=%d(%s)", errno, strerror(errno));printf("\n")
	#define rss_error(msg, ...)   printf("[%s][%d][error] ", log_context->format_time(), log_context->get_id());printf(msg, ##__VA_ARGS__);printf(" errno=%d(%s)", errno, strerror(errno));printf("\n")
// use __FUNCTION__ to print c method
#elif 1
	#define rss_verbose(msg, ...) printf("[%s][%d][verbs][%s] ", log_context->format_time(), log_context->get_id(), __FUNCTION__);printf(msg, ##__VA_ARGS__);printf("\n")
	#define rss_info(msg, ...)    printf("[%s][%d][infos][%s] ", log_context->format_time(), log_context->get_id(), __FUNCTION__);printf(msg, ##__VA_ARGS__);printf("\n")
	#define rss_trace(msg, ...)   printf("[%s][%d][trace][%s] ", log_context->format_time(), log_context->get_id(), __FUNCTION__);printf(msg, ##__VA_ARGS__);printf("\n")
	#define rss_warn(msg, ...)    printf("[%s][%d][warns][%s] ", log_context->format_time(), log_context->get_id(), __FUNCTION__);printf(msg, ##__VA_ARGS__);printf(" errno=%d(%s)", errno, strerror(errno));printf("\n")
	#define rss_error(msg, ...)   printf("[%s][%d][error][%s] ", log_context->format_time(), log_context->get_id(), __FUNCTION__);printf(msg, ##__VA_ARGS__);printf(" errno=%d(%s)", errno, strerror(errno));printf("\n")
// use __PRETTY_FUNCTION__ to print c++ class:method
#else
	#define rss_verbose(msg, ...) printf("[%s][%d][verbs][%s] ", log_context->format_time(), log_context->get_id(), __PRETTY_FUNCTION__);printf(msg, ##__VA_ARGS__);printf("\n")
	#define rss_info(msg, ...)    printf("[%s][%d][infos][%s] ", log_context->format_time(), log_context->get_id(), __PRETTY_FUNCTION__);printf(msg, ##__VA_ARGS__);printf("\n")
	#define rss_trace(msg, ...)   printf("[%s][%d][trace][%s] ", log_context->format_time(), log_context->get_id(), __PRETTY_FUNCTION__);printf(msg, ##__VA_ARGS__);printf("\n")
	#define rss_warn(msg, ...)    printf("[%s][%d][warns][%s] ", log_context->format_time(), log_context->get_id(), __PRETTY_FUNCTION__);printf(msg, ##__VA_ARGS__);printf(" errno=%d(%s)", errno, strerror(errno));printf("\n")
	#define rss_error(msg, ...)   printf("[%s][%d][error][%s] ", log_context->format_time(), log_context->get_id(), __PRETTY_FUNCTION__);printf(msg, ##__VA_ARGS__);printf(" errno=%d(%s)", errno, strerror(errno));printf("\n")
#endif

#if 1
	#undef rss_verbose
	#define rss_verbose(msg, ...) (void)0
#endif
#if 0
	#undef rss_info
	#define rss_info(msg, ...) (void)0
#endif

#endif
