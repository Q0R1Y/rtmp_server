#ifndef RSS_CORE_BUFFER_HPP
#define RSS_CORE_BUFFER_HPP

#include <rss_core.hpp>

#include <vector>

class RssSocket;

/**
* the buffer provices bytes cache for protocol. generally, 
* protocol recv data from socket, put into buffer, decode to RTMP message.
* protocol encode RTMP message to bytes, put into buffer, send to socket.
*/
class RssBuffer
{
private:
	std::vector<char> data;
public:
	RssBuffer();
	virtual ~RssBuffer();
public:
	virtual int size();
	virtual char* bytes();
	virtual void erase(int size);
private:
	virtual void append(char* bytes, int size);
public:
	virtual int ensure_buffer_bytes(RssSocket* skt, int required_size);
};

#endif