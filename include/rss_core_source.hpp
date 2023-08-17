#ifndef RSS_CORE_SOURCE_HPP
#define RSS_CORE_SOURCE_HPP

/*
#include <rss_core_source.hpp>
*/

#include <rss_core.hpp>

#include <map>
#include <vector>
#include <string>

class RssSource;
class RssCommonMessage;
class RssOnMetaDataPacket;
class RssSharedPtrMessage;

/**
* the consumer for RssSource, that is a play client.
*/
class RssConsumer
{
private:
	RssSource* source;
	std::vector<RssSharedPtrMessage*> msgs;
public:
	RssConsumer(RssSource* _source);
	virtual ~RssConsumer();
public:
	/**
	* enqueue an shared ptr message.
	*/
	virtual int enqueue(RssSharedPtrMessage* msg);
	/**
	* get packets in consumer queue.
	* @pmsgs RssMessages*[], output the prt array.
	* @count the count in array.
	* @max_count the max count to dequeue, 0 to dequeue all.
	*/
	virtual int get_packets(int max_count, RssSharedPtrMessage**& pmsgs, int& count);
};

/**
* live streaming source.
*/
class RssSource
{
private:
	static std::map<std::string, RssSource*> pool;
public:
	/**
	* find stream by vhost/app/stream.
	* @stream_url the stream url, for example, myserver.xxx.com/app/stream
	* @return the matched source, never be NULL.
	* @remark stream_url should without port and schema.
	*/
	static RssSource* find(std::string stream_url);
private:
	std::string stream_url;
	std::vector<RssConsumer*> consumers;
private:
	RssSharedPtrMessage* cache_metadata;
	// the cached video sequence header.
	RssSharedPtrMessage* cache_sh_video;
	// the cached audio sequence header.
	RssSharedPtrMessage* cache_sh_audio;
public:
	RssSource(std::string _stream_url);
	virtual ~RssSource();
public:
	virtual int on_meta_data(RssCommonMessage* msg, RssOnMetaDataPacket* metadata);
	virtual int on_audio(RssCommonMessage* audio);
	virtual int on_video(RssCommonMessage* video);
public:
	virtual int create_consumer(RssConsumer*& consumer);
	virtual void on_consumer_destroy(RssConsumer* consumer);
};

#endif