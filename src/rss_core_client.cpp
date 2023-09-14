#include <rss_core_client.hpp>

#include <arpa/inet.h>

#include <rss_core_error.hpp>
#include <rss_core_log.hpp>
#include <rss_core_rtmp.hpp>
#include <rss_core_protocol.hpp>
#include <rss_core_auto_free.hpp>
#include <rss_core_source.hpp>
#include <rss_core_server.hpp>

#define RSS_PULSE_TIMEOUT_MS 100
#define RSS_SEND_TIMEOUT_MS 5000

RssClient::RssClient(RssServer* rss_server, st_netfd_t client_stfd)
	: RssConnection(rss_server, client_stfd)
{
	ip = NULL;
	req = new RssRequest();
	res = new RssResponse();
	rtmp = new RssRtmp(client_stfd);
}

RssClient::~RssClient()
{
	rss_freepa(ip);
	rss_freep(req);
	rss_freep(res);
	rss_freep(rtmp);
}

int RssClient::do_cycle()
{
	int ret = ERROR_SUCCESS;

	if ((ret = get_peer_ip()) != ERROR_SUCCESS)
	{
		rss_error("get peer ip failed. ret=%d", ret);
		return ret;
	}
	rss_verbose("get peer ip success. ip=%s", ip);

	rtmp->set_recv_timeout(RSS_SEND_TIMEOUT_MS);
	rtmp->set_send_timeout(RSS_SEND_TIMEOUT_MS);

	if ((ret = rtmp->handshake()) != ERROR_SUCCESS)
	{
		rss_error("rtmp handshake failed. ret=%d", ret);
		return ret;
	}
	rss_verbose("rtmp handshake success");

	if ((ret = rtmp->connect_app(req)) != ERROR_SUCCESS)
	{
		rss_error("rtmp connect vhost/app failed. ret=%d", ret);
		return ret;
	}
	rss_trace("rtmp connect app success. "
	          "tcUrl=%s, pageUrl=%s, swfUrl=%s, schema=%s, vhost=%s, port=%s, app=%s",
	          req->tcUrl.c_str(), req->pageUrl.c_str(), req->swfUrl.c_str(),
	          req->schema.c_str(), req->vhost.c_str(), req->port.c_str(),
	          req->app.c_str());

	if ((ret = rtmp->set_window_ack_size(2.5 * 1000 * 1000)) != ERROR_SUCCESS)
	{
		rss_error("set window acknowledgement size failed. ret=%d", ret);
		return ret;
	}
	rss_verbose("set window acknowledgement size success");

	if ((ret = rtmp->set_peer_bandwidth(2.5 * 1000 * 1000, 2)) != ERROR_SUCCESS)
	{
		rss_error("set peer bandwidth failed. ret=%d", ret);
		return ret;
	}
	rss_verbose("set peer bandwidth success");

	if ((ret = rtmp->response_connect_app(req)) != ERROR_SUCCESS)
	{
		rss_error("response connect app failed. ret=%d", ret);
		return ret;
	}
	rss_verbose("response connect app success");

	if ((ret = rtmp->on_bw_done()) != ERROR_SUCCESS)
	{
		rss_error("on_bw_done failed. ret=%d", ret);
		return ret;
	}
	rss_verbose("on_bw_done success");

	RssClientType type;
	if ((ret = rtmp->identify_client(res->stream_id, type, req->stream)) != ERROR_SUCCESS)
	{
		rss_error("identify client failed. ret=%d", ret);
		return ret;
	}
	rss_verbose("identify client success. type=%d, stream_name=%s", type, req->stream.c_str());

	// TODO: read from config.
	int chunk_size = 4096;
	if ((ret = rtmp->set_chunk_size(chunk_size)) != ERROR_SUCCESS)
	{
		rss_error("set chunk size failed. ret=%d", ret);
		return ret;
	}
	rss_verbose("set chunk size success");

	// find a source to publish.
	RssSource* source = RssSource::find(req->get_stream_url());
	rss_assert(source != NULL);
	rss_info("source found, url=%s", req->get_stream_url().c_str());

	switch (type)
	{
	case RssClientPlay:
	{
		rss_verbose("start to play stream %s.", req->stream.c_str());

		if ((ret = rtmp->start_play(res->stream_id)) != ERROR_SUCCESS)
		{
			rss_error("start to play stream failed. ret=%d", ret);
			return ret;
		}
		rss_info("start to play stream %s success", req->stream.c_str());
		return streaming_play(source);
	}
	case RssClientPublish:
	{
		rss_verbose("start to publish stream %s.", req->stream.c_str());

		if ((ret = rtmp->start_publish(res->stream_id)) != ERROR_SUCCESS)
		{
			rss_error("start to publish stream failed. ret=%d", ret);
			return ret;
		}
		rss_info("start to publish stream %s success", req->stream.c_str());
		return streaming_publish(source);
	}
	default:
	{
		ret = ERROR_SYSTEM_CLIENT_INVALID;
		rss_info("invalid client type=%d. ret=%d", type, ret);
		return ret;
	}
	}

	return ret;
}

int RssClient::streaming_play(RssSource* source)
{
	int ret = ERROR_SUCCESS;

	RssConsumer* consumer = NULL;
	if ((ret = source->create_consumer(consumer)) != ERROR_SUCCESS)
	{
		rss_error("create consumer failed. ret=%d", ret);
		return ret;
	}

	rss_assert(consumer != NULL);
	RssAutoFree(RssConsumer, consumer, false);
	rss_verbose("consumer created success.");

	rtmp->set_recv_timeout(RSS_PULSE_TIMEOUT_MS);

	int64_t report_time = 0;
	int64_t reported_time = 0;

	while (true)
	{
		report_time += RSS_PULSE_TIMEOUT_MS;

		// switch to other st-threads.
		st_usleep(0);

		// read from client.
		int ctl_msg_ret = ERROR_SUCCESS;
		if (true)
		{
			RssCommonMessage* msg = NULL;
			ctl_msg_ret = ret = rtmp->recv_message(&msg);

			rss_verbose("play loop recv message. ret=%d", ret);
			if (ret != ERROR_SUCCESS && ret != ERROR_SOCKET_TIMEOUT)
			{
				rss_error("recv client control message failed. ret=%d", ret);
				return ret;
			}
			if (ret == ERROR_SUCCESS && !msg)
			{
				rss_info("play loop got a message.");
				RssAutoFree(RssCommonMessage, msg, false);
				// TODO: process it.
			}
		}

		// get messages from consumer.
		RssSharedPtrMessage** msgs = NULL;
		int count = 0;
		if ((ret = consumer->get_packets(0, msgs, count)) != ERROR_SUCCESS)
		{
			rss_error("get messages from consumer failed. ret=%d", ret);
			return ret;
		}

		// reportable
		if (server->can_report(reported_time, report_time))
		{
			rss_trace("play report, time=%" PRId64 ", ctl_msg_ret=%d, msgs=%d", report_time, ctl_msg_ret, count);
		}

		if (count <= 0)
		{
			rss_verbose("no packets in queue.");
			continue;
		}
		RssAutoFree(RssSharedPtrMessage*, msgs, true);

		// sendout messages
		for (int i = 0; i < count; i++)
		{
			RssSharedPtrMessage* msg = msgs[i];

			// the send_message will free the msg,
			// so set the msgs[i] to NULL.
			msgs[i] = NULL;

			if ((ret = rtmp->send_message(msg)) != ERROR_SUCCESS)
			{
				rss_error("send message to client failed. ret=%d", ret);
				return ret;
			}
		}
	}

	return ret;
}

int RssClient::streaming_publish(RssSource* source)
{
	int ret = ERROR_SUCCESS;

	while (true)
	{
		// switch to other st-threads.
		st_usleep(0);

		RssCommonMessage* msg = NULL;
		if ((ret = rtmp->recv_message(&msg)) != ERROR_SUCCESS)
		{
			rss_error("recv identify client message failed. ret=%d", ret);
			return ret;
		}

		RssAutoFree(RssCommonMessage, msg, false);

		// process audio packet
		if (msg->header.is_audio() && ((ret = source->on_audio(msg)) != ERROR_SUCCESS))
		{
			rss_error("process audio message failed. ret=%d", ret);
			return ret;
		}
		// process video packet
		if (msg->header.is_video() && ((ret = source->on_video(msg)) != ERROR_SUCCESS))
		{
			rss_error("process video message failed. ret=%d", ret);
			return ret;
		}

		// process onMetaData
		if (msg->header.is_amf0_data() || msg->header.is_amf3_data())
		{
			if ((ret = msg->decode_packet()) != ERROR_SUCCESS)
			{
				rss_error("decode onMetaData message failed. ret=%d", ret);
				return ret;
			}

			RssPacket* pkt = msg->get_packet();
			if (dynamic_cast<RssOnMetaDataPacket*>(pkt))
			{
				RssOnMetaDataPacket* metadata = dynamic_cast<RssOnMetaDataPacket*>(pkt);
				if ((ret = source->on_meta_data(msg, metadata)) != ERROR_SUCCESS)
				{
					rss_error("process onMetaData message failed. ret=%d", ret);
					return ret;
				}
				rss_trace("process onMetaData message success.");
				continue;
			}

			rss_trace("ignore AMF0/AMF3 data message.");
			continue;
		}

		// process UnPublish event.
		if (msg->header.is_amf0_command() || msg->header.is_amf3_command())
		{
			if ((ret = msg->decode_packet()) != ERROR_SUCCESS)
			{
				rss_error("decode unpublish message failed. ret=%d", ret);
				return ret;
			}

			RssPacket* pkt = msg->get_packet();
			if (dynamic_cast<RssFMLEStartPacket*>(pkt))
			{
				RssFMLEStartPacket* unpublish = dynamic_cast<RssFMLEStartPacket*>(pkt);
				return rtmp->fmle_unpublish(res->stream_id, unpublish->transaction_id);
			}

			rss_trace("ignore AMF0/AMF3 command message.");
			continue;
		}
	}

	return ret;
}

int RssClient::get_peer_ip()
{
	int ret = ERROR_SUCCESS;

	int fd = st_netfd_fileno(stfd);

	// discovery client information
	sockaddr_in addr;
	socklen_t addrlen = sizeof(addr);
	if (getpeername(fd, (sockaddr*)&addr, &addrlen) == -1)
	{
		ret = ERROR_SOCKET_GET_PEER_NAME;
		rss_error("discovery client information failed. ret=%d", ret);
		return ret;
	}
	rss_verbose("get peer name success.");

	// ip v4 or v6
	char buf[INET6_ADDRSTRLEN];
	memset(buf, 0, sizeof(buf));

	if ((inet_ntop(addr.sin_family, &addr.sin_addr, buf, sizeof(buf))) == NULL)
	{
		ret = ERROR_SOCKET_GET_PEER_IP;
		rss_error("convert client information failed. ret=%d", ret);
		return ret;
	}
	rss_verbose("get peer ip of client ip=%s, fd=%d", buf, fd);

	ip = new char[strlen(buf) + 1];
	strcpy(ip, buf);

	rss_trace("get peer ip success. ip=%s, fd=%d", ip, fd);

	return ret;
}


