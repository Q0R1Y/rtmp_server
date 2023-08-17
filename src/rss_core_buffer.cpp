#include <rss_core_buffer.hpp>

#include <rss_core_error.hpp>
#include <rss_core_socket.hpp>

#define SOCKET_READ_SIZE 4096

RssBuffer::RssBuffer()
{
}

RssBuffer::~RssBuffer()
{
}

int RssBuffer::size()
{
	return (int)data.size();
}

char* RssBuffer::bytes()
{
	return &data.at(0);
}

void RssBuffer::erase(int size)
{
	data.erase(data.begin(), data.begin() + size);
}

void RssBuffer::append(char* bytes, int size)
{
	std::vector<char> vec(bytes, bytes + size);
	
	data.insert(data.end(), vec.begin(), vec.end());
}

int RssBuffer::ensure_buffer_bytes(RssSocket* skt, int required_size)
{
	int ret = ERROR_SUCCESS;

	rss_assert(required_size >= 0);

	while (size() < required_size) {
		char buffer[SOCKET_READ_SIZE];
		
		ssize_t nread;
		if ((ret = skt->read(buffer, SOCKET_READ_SIZE, &nread)) != ERROR_SUCCESS) {
			return ret;
		}
		
		rss_assert((int)nread > 0);
		append(buffer, (int)nread);
	}
	
	return ret;
}