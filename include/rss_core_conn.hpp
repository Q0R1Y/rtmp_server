#ifndef RSS_CORE_CONN_HPP
#define RSS_CORE_CONN_HPP

/*
#include <rss_core_conn.hpp>
*/

#include <rss_core.hpp>

#include <st.h>

class RssServer;
class RssConnection
{
protected:
	RssServer* server;
	st_netfd_t stfd;
public:
	RssConnection(RssServer* rss_server, st_netfd_t client_stfd);
	virtual ~RssConnection();
public:
	virtual int start();
protected:
	virtual int do_cycle() = 0;
private:
	virtual void cycle();
	static void* cycle_thread(void* arg);
};

#endif