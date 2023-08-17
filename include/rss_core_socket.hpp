#ifndef RSS_CORE_SOCKET_HPP
#define RSS_CORE_SOCKET_HPP

/*
#include <rss_core_socket.hpp>
*/

#include <rss_core.hpp>

#include <st.h>

/**
* the socket provides TCP socket over st,
* that is, the sync socket mechanism.
*/
class RssSocket
{
private:
	int64_t recv_timeout;
	int64_t send_timeout;
    st_netfd_t stfd;
public:
    RssSocket(st_netfd_t client_stfd);
    virtual ~RssSocket();
public:
	virtual void set_recv_timeout(int timeout_ms);
	virtual void set_send_timeout(int timeout_ms);
    virtual int read(const void* buf, size_t size, ssize_t* nread);
    virtual int read_fully(const void* buf, size_t size, ssize_t* nread);
    virtual int write(const void* buf, size_t size, ssize_t* nwrite);
    virtual int writev(const iovec *iov, int iov_size, ssize_t* nwrite);
};

#endif