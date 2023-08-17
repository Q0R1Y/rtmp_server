#include <rss_core_socket.hpp>

#include <rss_core_error.hpp>

RssSocket::RssSocket(st_netfd_t client_stfd)
{
    stfd = client_stfd;
	recv_timeout = ST_UTIME_NO_TIMEOUT;
	send_timeout = ST_UTIME_NO_TIMEOUT;
}

RssSocket::~RssSocket()
{
}

void RssSocket::set_recv_timeout(int timeout_ms)
{
	recv_timeout = timeout_ms * 1000;
}

void RssSocket::set_send_timeout(int timeout_ms)
{
	send_timeout = timeout_ms * 1000;
}

int RssSocket::read(const void* buf, size_t size, ssize_t* nread)
{
    int ret = ERROR_SUCCESS;
    
    *nread = st_read(stfd, (void*)buf, size, recv_timeout);
    
    // On success a non-negative integer indicating the number of bytes actually read is returned 
    // (a value of 0 means the network connection is closed or end of file is reached).
    if (*nread <= 0) {
		if (errno == ETIME) {
			return ERROR_SOCKET_TIMEOUT;
		}
		
        if (*nread == 0) {
            errno = ECONNRESET;
        }
        
        ret = ERROR_SOCKET_READ;
    }
        
    return ret;
}

int RssSocket::read_fully(const void* buf, size_t size, ssize_t* nread)
{
    int ret = ERROR_SUCCESS;
    
    *nread = st_read_fully(stfd, (void*)buf, size, recv_timeout);
    
    // On success a non-negative integer indicating the number of bytes actually read is returned 
    // (a value less than nbyte means the network connection is closed or end of file is reached)
    if (*nread != (ssize_t)size) {
		if (errno == ETIME) {
			return ERROR_SOCKET_TIMEOUT;
		}
		
        if (*nread >= 0) {
            errno = ECONNRESET;
        }
        
        ret = ERROR_SOCKET_READ_FULLY;
    }
    
    return ret;
}

int RssSocket::write(const void* buf, size_t size, ssize_t* nwrite)
{
    int ret = ERROR_SUCCESS;
    
    *nwrite = st_write(stfd, (void*)buf, size, send_timeout);
    
    if (*nwrite <= 0) {
		if (errno == ETIME) {
			return ERROR_SOCKET_TIMEOUT;
		}
		
        ret = ERROR_SOCKET_WRITE;
    }
        
    return ret;
}

int RssSocket::writev(const iovec *iov, int iov_size, ssize_t* nwrite)
{
    int ret = ERROR_SUCCESS;
    
    *nwrite = st_writev(stfd, iov, iov_size, send_timeout);
    
    if (*nwrite <= 0) {
		if (errno == ETIME) {
			return ERROR_SOCKET_TIMEOUT;
		}
		
        ret = ERROR_SOCKET_WRITE;
    }
    
    return ret;
}