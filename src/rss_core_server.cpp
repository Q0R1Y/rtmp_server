#include <rss_core_server.hpp>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <algorithm>

#include <st.h>

#include <rss_core_log.hpp>
#include <rss_core_error.hpp>
#include <rss_core_client.hpp>

#define SERVER_LISTEN_BACKLOG 10

// global value, ensure the report interval,
// it will be changed when clients increase.
#define RSS_CONST_REPORT_INTERVAL_MS 3000

RssServer::RssServer()
{
	rss_report_interval_ms = RSS_CONST_REPORT_INTERVAL_MS;
}

RssServer::~RssServer()
{
	for (std::vector<RssConnection*>::iterator it = conns.begin(); it != conns.end(); ++it) {
		RssConnection* conn = *it;
		rss_freep(conn);
	}
	conns.clear();
}

int RssServer::initialize()
{
	int ret = ERROR_SUCCESS;
    
    // use linux epoll.
    if (st_set_eventsys(ST_EVENTSYS_ALT) == -1) {
        ret = ERROR_ST_SET_EPOLL;
        rss_error("st_set_eventsys use linux epoll failed. ret=%d", ret);
        return ret;
    }
    rss_verbose("st_set_eventsys use linux epoll success");
    
    if(st_init() != 0){
        ret = ERROR_ST_INITIALIZE;
        rss_error("st_init failed. ret=%d", ret);
        return ret;
    }
    rss_verbose("st_init success");
	
	// set current log id.
	log_context->generate_id();
	rss_info("log set id success");
	
	return ret;
}

int RssServer::listen(int port)
{
	int ret = ERROR_SUCCESS;
	
	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        ret = ERROR_SOCKET_CREATE;
        rss_error("create linux socket error. ret=%d", ret);
        return ret;
	}
	rss_verbose("create linux socket success. fd=%d", fd);
    
    int reuse_socket = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse_socket, sizeof(int)) == -1) {
        ret = ERROR_SOCKET_SETREUSE;
        rss_error("setsockopt reuse-addr error. ret=%d", ret);
        return ret;
    }
    rss_verbose("setsockopt reuse-addr success. fd=%d", fd);
    
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(fd, (const sockaddr*)&addr, sizeof(sockaddr_in)) == -1) {
        ret = ERROR_SOCKET_BIND;
        rss_error("bind socket error. ret=%d", ret);
        return ret;
    }
    rss_verbose("bind socket success. fd=%d", fd);
    
    if (::listen(fd, SERVER_LISTEN_BACKLOG) == -1) {
        ret = ERROR_SOCKET_LISTEN;
        rss_error("listen socket error. ret=%d", ret);
        return ret;
    }
    rss_verbose("listen socket success. fd=%d", fd);
    
    if ((stfd = st_netfd_open_socket(fd)) == NULL){
        ret = ERROR_ST_OPEN_SOCKET;
        rss_error("st_netfd_open_socket open socket failed. ret=%d", ret);
        return ret;
    }
    rss_verbose("st open socket success. fd=%d", fd);
    
    if (st_thread_create(listen_thread, this, 0, 0) == NULL) {
        ret = ERROR_ST_CREATE_LISTEN_THREAD;
        rss_error("st_thread_create listen thread error. ret=%d", ret);
        return ret;
    }
    rss_verbose("create st listen thread success.");
    
    rss_trace("server started, listen at port=%d, fd=%d", port, fd);
	
	return ret;
}

int RssServer::cycle()
{
	int ret = ERROR_SUCCESS;
	// TODO: canbe a api thread.
	st_thread_exit(NULL);
	return ret;
}

void RssServer::remove(RssConnection* conn)
{
	std::vector<RssConnection*>::iterator it = std::find(conns.begin(), conns.end(), conn);
	
	if (it != conns.end()) {
		conns.erase(it);
	}
	
	rss_info("conn removed. conns=%d", (int)conns.size());
	
	// all connections are created by server,
	// so we free it here.
	rss_freep(conn);
}

bool RssServer::can_report(int64_t& reported, int64_t time)
{
	if (rss_report_interval_ms <= 0) {
		return false;
	}
	
	if (time - reported < rss_report_interval_ms) {
		return false;
	}
	
	reported = time;
	return true;
}

int RssServer::accept_client(st_netfd_t client_stfd)
{
	int ret = ERROR_SUCCESS;
	
	RssConnection* conn = new RssClient(this, client_stfd);
	
	// directly enqueue, the cycle thread will remove the client.
	conns.push_back(conn);
	rss_verbose("add conn to vector. conns=%d", (int)conns.size());

	// ensure the report interval is consts
	rss_report_interval_ms = RSS_CONST_REPORT_INTERVAL_MS * (int)conns.size();
	
	// cycle will start process thread and when finished remove the client.
	if ((ret = conn->start()) != ERROR_SUCCESS) {
		return ret;
	}
	rss_verbose("conn start finished. ret=%d", ret);
    
	return ret;
}

void RssServer::listen_cycle()
{
	int ret = ERROR_SUCCESS;
	
	log_context->generate_id();
	rss_trace("listen cycle start.");
	
	while (true) {
	    st_netfd_t client_stfd = st_accept(stfd, NULL, NULL, ST_UTIME_NO_TIMEOUT);
	    
	    if(client_stfd == NULL){
	        // ignore error.
	        rss_warn("ignore accept thread stoppped for accept client error");
	        continue;
	    }
	    rss_verbose("get a client. fd=%d", st_netfd_fileno(client_stfd));
    	
    	if ((ret = accept_client(client_stfd)) != ERROR_SUCCESS) {
    		rss_warn("accept client error. ret=%d", ret);
			continue;
    	}
    	
    	rss_verbose("accept client finished. conns=%d, ret=%d", (int)conns.size(), ret);
	}
}

void* RssServer::listen_thread(void* arg)
{
	RssServer* server = (RssServer*)arg;
	rss_assert(server != NULL);
	
	server->listen_cycle();
	
	return NULL;
}

