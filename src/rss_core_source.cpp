#include <rss_core_source.hpp>

#include <algorithm>

#include <rss_core_log.hpp>
#include <rss_core_protocol.hpp>
#include <rss_core_auto_free.hpp>
#include <rss_core_amf0.hpp>

std::map<std::string, RssSource*> RssSource::pool;

RssSource* RssSource::find(std::string stream_url)
{
	if (pool.find(stream_url) == pool.end())
	{
		pool[stream_url] = new RssSource(stream_url);
		rss_verbose("create new source for url=%s", stream_url.c_str());
	}

	return pool[stream_url];
}

RssConsumer::RssConsumer(RssSource* _source)
{
	source = _source;
}

RssConsumer::~RssConsumer()
{
	std::vector<RssSharedPtrMessage*>::iterator it;
	for (it = msgs.begin(); it != msgs.end(); ++it)
	{
		RssSharedPtrMessage* msg = *it;
		rss_freep(msg);
	}
	msgs.clear();

	source->on_consumer_destroy(this);
}

int RssConsumer::enqueue(RssSharedPtrMessage* msg)
{
	int ret = ERROR_SUCCESS;
	msgs.push_back(msg);
	return ret;
}

int RssConsumer::get_packets(int max_count, RssSharedPtrMessage**& pmsgs, int& count)
{
	int ret = ERROR_SUCCESS;

	if (msgs.empty())
	{
		return ret;
	}

	if (max_count == 0)
	{
		count = (int)msgs.size();
	}
	else
	{
		count = rss_min(max_count, (int)msgs.size());
	}

	pmsgs = new RssSharedPtrMessage*[count];

	for (int i = 0; i < count; i++)
	{
		pmsgs[i] = msgs[i];
	}

	if (count == (int)msgs.size())
	{
		msgs.clear();
	}
	else
	{
		msgs.erase(msgs.begin(), msgs.begin() + count);
	}

	return ret;
}

RssSource::RssSource(std::string _stream_url)
{
	stream_url = _stream_url;
	cache_metadata = NULL;
	cache_sh_video = NULL;
	cache_sh_audio = NULL;
}

RssSource::~RssSource()
{
	std::vector<RssConsumer*>::iterator it;
	for (it = consumers.begin(); it != consumers.end(); ++it)
	{
		RssConsumer* consumer = *it;
		rss_freep(consumer);
	}
	consumers.clear();

	rss_freep(cache_metadata);
	rss_freep(cache_sh_video);
	rss_freep(cache_sh_audio);
}

int RssSource::on_meta_data(RssCommonMessage* msg, RssOnMetaDataPacket* metadata)
{
	int ret = ERROR_SUCCESS;

	metadata->metadata->set("server",
	                        new RssAmf0String(RTMP_SIG_RSS_NAME""RTMP_SIG_RSS_VERSION));

	// encode the metadata to payload
	int size = metadata->get_payload_length();
	if (size <= 0)
	{
		rss_warn("ignore the invalid metadata. size=%d", size);
		return ret;
	}
	rss_verbose("get metadata size success.");

	char* payload = new char[size];
	memset(payload, 0, size);
	if ((ret = metadata->encode(size, payload)) != ERROR_SUCCESS)
	{
		rss_error("encode metadata error. ret=%d", ret);
		rss_freepa(payload);
		return ret;
	}
	rss_verbose("encode metadata success.");

	// create a shared ptr message.
	rss_freep(cache_metadata);
	cache_metadata = new RssSharedPtrMessage();

	// dump message to shared ptr message.
	if ((ret = cache_metadata->initialize(msg, payload, size)) != ERROR_SUCCESS)
	{
		rss_error("initialize the cache metadata failed. ret=%d", ret);
		return ret;
	}
	rss_verbose("initialize shared ptr metadata success.");

	// copy to all consumer
	std::vector<RssConsumer*>::iterator it;
	for (it = consumers.begin(); it != consumers.end(); ++it)
	{
		RssConsumer* consumer = *it;
		if ((ret = consumer->enqueue(cache_metadata->copy())) != ERROR_SUCCESS)
		{
			rss_error("dispatch the metadata failed. ret=%d", ret);
			return ret;
		}
	}
	rss_trace("dispatch metadata success.");

	return ret;
}

int RssSource::on_audio(RssCommonMessage* audio)
{
	int ret = ERROR_SUCCESS;

	RssSharedPtrMessage* msg = new RssSharedPtrMessage();
	RssAutoFree(RssSharedPtrMessage, msg, false);
	if ((ret = msg->initialize(audio, (char*)audio->payload, audio->size)) != ERROR_SUCCESS)
	{
		rss_error("initialize the audio failed. ret=%d", ret);
		return ret;
	}
	rss_verbose("initialize shared ptr audio success.");

	// detach the original audio
	audio->payload = NULL;
	audio->size = 0;

	// copy to all consumer
	std::vector<RssConsumer*>::iterator it;
	for (it = consumers.begin(); it != consumers.end(); ++it)
	{
		RssConsumer* consumer = *it;
		if ((ret = consumer->enqueue(msg->copy())) != ERROR_SUCCESS)
		{
			rss_error("dispatch the audio failed. ret=%d", ret);
			return ret;
		}
	}
	rss_info("dispatch audio success.");

	if (!cache_sh_audio)
	{
		rss_freep(cache_sh_audio);
		cache_sh_audio = msg->copy();
	}

	return ret;
}

int RssSource::on_video(RssCommonMessage* video)
{
	int ret = ERROR_SUCCESS;

	RssSharedPtrMessage* msg = new RssSharedPtrMessage();
	RssAutoFree(RssSharedPtrMessage, msg, false);
	if ((ret = msg->initialize(video, (char*)video->payload, video->size)) != ERROR_SUCCESS)
	{
		rss_error("initialize the video failed. ret=%d", ret);
		return ret;
	}
	rss_verbose("initialize shared ptr video success.");

	// detach the original audio
	video->payload = NULL;
	video->size = 0;

	// copy to all consumer
	std::vector<RssConsumer*>::iterator it;
	for (it = consumers.begin(); it != consumers.end(); ++it)
	{
		RssConsumer* consumer = *it;
		if ((ret = consumer->enqueue(msg->copy())) != ERROR_SUCCESS)
		{
			rss_error("dispatch the video failed. ret=%d", ret);
			return ret;
		}
	}
	rss_info("dispatch video success.");

	if (!cache_sh_video)
	{
		rss_freep(cache_sh_video);
		cache_sh_video = msg->copy();
	}

	return ret;
}

int RssSource::create_consumer(RssConsumer*& consumer)
{
	int ret = ERROR_SUCCESS;

	consumer = new RssConsumer(this);
	consumers.push_back(consumer);

	if (cache_metadata && (ret = consumer->enqueue(cache_metadata->copy())) != ERROR_SUCCESS)
	{
		rss_error("dispatch metadata failed. ret=%d", ret);
		return ret;
	}
	rss_info("dispatch metadata success");

	if (cache_sh_video && (ret = consumer->enqueue(cache_sh_video->copy())) != ERROR_SUCCESS)
	{
		rss_error("dispatch video sequence header failed. ret=%d", ret);
		return ret;
	}
	rss_info("dispatch video sequence header success");

	if (cache_sh_audio && (ret = consumer->enqueue(cache_sh_audio->copy())) != ERROR_SUCCESS)
	{
		rss_error("dispatch audio sequence header failed. ret=%d", ret);
		return ret;
	}
	rss_info("dispatch audio sequence header success");

	return ret;
}

void RssSource::on_consumer_destroy(RssConsumer* consumer)
{
	std::vector<RssConsumer*>::iterator it;
	it = std::find(consumers.begin(), consumers.end(), consumer);
	if (it != consumers.end())
	{
		consumers.erase(it);
	}
	rss_info("handle consumer destroy success.");
}