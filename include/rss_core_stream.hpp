#ifndef RSS_CORE_STREAM_HPP
#define RSS_CORE_STREAM_HPP

/*
#include <rss_core_stream.hpp>
*/

#include <rss_core.hpp>

#include <sys/types.h>
#include <string>

class RssStream
{
private:
	char* p;
	char* pp;
	char* bytes;
	int size;
public:
	RssStream();
	virtual ~RssStream();
public:
	/**
	* initialize the stream from bytes.
	* @_bytes, must not be NULL, or return error.
	* @_size, must be positive, or return error.
	* @remark, stream never free the _bytes, user must free it.
	*/
	virtual int initialize(char* _bytes, int _size);
	/**
	* reset the position to beginning.
	*/
	virtual void reset();
	/**
	* whether stream is empty.
	* if empty, never read or write.
	*/
	virtual bool empty();
	/**
	* whether required size is ok.
	* @return true if stream can read/write specified required_size bytes.
	*/
	virtual bool require(int required_size);
	/**
	* to skip some size.
	* @size can be any value. positive to forward; nagetive to backward.
	*/
	virtual void skip(int size);
	/**
	* tell the current pos.
	*/
	virtual int pos();
public:
	/**
	* get 1bytes char from stream.
	*/
	virtual int8_t read_1bytes();
	/**
	* get 2bytes int from stream.
	*/
	virtual int16_t read_2bytes();
	/**
	* get 4bytes int from stream.
	*/
	virtual int32_t read_4bytes();
	/**
	* get 8bytes int from stream.
	*/
	virtual int64_t read_8bytes();
	/**
	* get string from stream, length specifies by param len.
	*/
	virtual std::string read_string(int len);
public:
	/**
	* write 1bytes char to stream.
	*/
	virtual void write_1bytes(int8_t value);
	/**
	* write 2bytes int to stream.
	*/
	virtual void write_2bytes(int16_t value);
	/**
	* write 4bytes int to stream.
	*/
	virtual void write_4bytes(int32_t value);
	/**
	* write 8bytes int to stream.
	*/
	virtual void write_8bytes(int64_t value);
	/**
	* write string to stream
	*/
	virtual void write_string(std::string value);
};

#endif