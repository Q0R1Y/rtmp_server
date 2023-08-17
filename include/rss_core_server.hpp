#ifndef RSS_CORE_SERVER_HPP
#define RSS_CORE_SERVER_HPP

#include <rss_core.hpp>

#include <vector>

#include <st.h>

class RssConnection;
class RssServer
{
private:
	int fd;
	st_netfd_t stfd;
	std::vector<RssConnection*> conns;
	int rss_report_interval_ms;
public:
	RssServer();
	virtual ~RssServer();
public:
	virtual int initialize();
	virtual int listen(int port);
	virtual int cycle();
	virtual void remove(RssConnection* conn);
	virtual bool can_report(int64_t& reported, int64_t time);
private:
	virtual int accept_client(st_netfd_t client_stfd);
	virtual void listen_cycle();
	static void* listen_thread(void* arg);
};

#endif