#include <rss_core_conn.hpp>

#include <rss_core_log.hpp>
#include <rss_core_error.hpp>
#include <rss_core_server.hpp>

RssConnection::RssConnection(RssServer* rss_server, st_netfd_t client_stfd)
{
	server = rss_server;
	stfd = client_stfd;
}

RssConnection::~RssConnection()
{
	if (stfd)
	{
		st_netfd_close(stfd);
		stfd = NULL;
	}
}

int RssConnection::start()
{
	int ret = ERROR_SUCCESS;

	if (st_thread_create(cycle_thread, this, 0, 0) == NULL)
	{
		ret = ERROR_ST_CREATE_CYCLE_THREAD;
		rss_error("st_thread_create conn cycle thread error. ret=%d", ret);
		return ret;
	}
	rss_verbose("create st conn cycle thread success.");

	return ret;
}

void RssConnection::cycle()
{
	int ret = ERROR_SUCCESS;

	log_context->generate_id();
	ret = do_cycle();

	// if socket io error, set to closed.
	if (ret == ERROR_SOCKET_READ || ret == ERROR_SOCKET_READ_FULLY || ret == ERROR_SOCKET_WRITE)
	{
		ret = ERROR_SOCKET_CLOSED;
	}

	// success.
	if (ret == ERROR_SUCCESS)
	{
		rss_trace("client process normally finished. ret=%d", ret);
	}

	// client close peer.
	if (ret == ERROR_SOCKET_CLOSED)
	{
		rss_trace("client disconnect peer. ret=%d", ret);
	}

	server->remove(this);
}

void* RssConnection::cycle_thread(void* arg)
{
	RssConnection* conn = (RssConnection*)arg;
	rss_assert(conn != NULL);

	conn->cycle();

	return NULL;
}

