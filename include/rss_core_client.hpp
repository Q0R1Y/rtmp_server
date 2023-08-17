#ifndef RSS_CORE_CLIENT_HPP
#define RSS_CORE_CLIENT_HPP

/*
#include <rss_core_client.hpp>
*/

#include <rss_core.hpp>

#include <rss_core_conn.hpp>

class RssRtmp;
class RssRequest;
class RssResponse;
class RssSource;

/**
* the client provides the main logic control for RTMP clients.
*/
class RssClient : public RssConnection
{
private:
	char* ip;
	RssRequest* req;
	RssResponse* res;
	RssRtmp* rtmp;
public:
	RssClient(RssServer* rss_server, st_netfd_t client_stfd);
	virtual ~RssClient();
protected:
	virtual int do_cycle();
private:
	virtual int streaming_play(RssSource* source);
	virtual int streaming_publish(RssSource* source);
	virtual int get_peer_ip();
};

#endif