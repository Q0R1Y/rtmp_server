#include <rss_core_stream.hpp>

#include <rss_core_log.hpp>
#include <rss_core_error.hpp>

RssStream::RssStream()
{
	p = bytes = NULL;
	size = 0;
}

RssStream::~RssStream()
{
}

int RssStream::initialize(char* _bytes, int _size)
{
	int ret = ERROR_SUCCESS;

	if (!_bytes)
	{
		ret = ERROR_SYSTEM_STREAM_INIT;
		rss_error("stream param bytes must not be NULL. ret=%d", ret);
		return ret;
	}

	if (_size <= 0)
	{
		ret = ERROR_SYSTEM_STREAM_INIT;
		rss_error("stream param size must be positive. ret=%d", ret);
		return ret;
	}

	size = _size;
	p = bytes = _bytes;

	return ret;
}

void RssStream::reset()
{
	p = bytes;
}

bool RssStream::empty()
{
	return !p || !bytes || (p >= bytes + size);
}

bool RssStream::require(int required_size)
{
	return !empty() && (required_size <= bytes + size - p);
}

void RssStream::skip(int size)
{
	p += size;
}

int RssStream::pos()
{
	if (empty())
	{
		return 0;
	}

	return p - bytes;
}

int8_t RssStream::read_1bytes()
{
	rss_assert(require(1));

	return (int8_t)*p++;
}

int16_t RssStream::read_2bytes()
{
	rss_assert(require(2));

	int16_t value;
	pp = (char*)&value;
	pp[1] = *p++;
	pp[0] = *p++;

	return value;
}

int32_t RssStream::read_4bytes()
{
	rss_assert(require(4));

	int32_t value;
	pp = (char*)&value;
	pp[3] = *p++;
	pp[2] = *p++;
	pp[1] = *p++;
	pp[0] = *p++;

	return value;
}

int64_t RssStream::read_8bytes()
{
	rss_assert(require(8));

	int64_t value;
	pp = (char*)&value;
	pp[7] = *p++;
	pp[6] = *p++;
	pp[5] = *p++;
	pp[4] = *p++;
	pp[3] = *p++;
	pp[2] = *p++;
	pp[1] = *p++;
	pp[0] = *p++;

	return value;
}

std::string RssStream::read_string(int len)
{
	rss_assert(require(len));

	std::string value;
	value.append(p, len);

	p += len;

	return value;
}

void RssStream::write_1bytes(int8_t value)
{
	rss_assert(require(1));

	*p++ = value;
}

void RssStream::write_2bytes(int16_t value)
{
	rss_assert(require(2));

	pp = (char*)&value;
	*p++ = pp[1];
	*p++ = pp[0];
}

void RssStream::write_4bytes(int32_t value)
{
	rss_assert(require(4));

	pp = (char*)&value;
	*p++ = pp[3];
	*p++ = pp[2];
	*p++ = pp[1];
	*p++ = pp[0];
}

void RssStream::write_8bytes(int64_t value)
{
	rss_assert(require(8));

	pp = (char*)&value;
	*p++ = pp[7];
	*p++ = pp[6];
	*p++ = pp[5];
	*p++ = pp[4];
	*p++ = pp[3];
	*p++ = pp[2];
	*p++ = pp[1];
	*p++ = pp[0];
}

void RssStream::write_string(std::string value)
{
	rss_assert(require(value.length()));

	memcpy(p, value.data(), value.length());
	p += value.length();
}

