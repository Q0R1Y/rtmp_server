#include <rss_core_rtmp.hpp>

#include <rss_core_log.hpp>
#include <rss_core_error.hpp>
#include <rss_core_socket.hpp>
#include <rss_core_protocol.hpp>
#include <rss_core_auto_free.hpp>
#include <rss_core_amf0.hpp>

/**
* the signature for packets to client.
*/
#define RTMP_SIG_FMS_VER "3,5,3,888"
#define RTMP_SIG_AMF0_VER 0
#define RTMP_SIG_CLIENT_ID "ASAICiss"

/**
* onStatus consts.
*/
#define StatusLevel "level"
#define StatusCode "code"
#define StatusDescription "description"
#define StatusDetails "details"
#define StatusClientId "clientid"
// status value
#define StatusLevelStatus "status"
// code value
#define StatusCodeConnectSuccess "NetConnection.Connect.Success"
#define StatusCodeStreamReset "NetStream.Play.Reset"
#define StatusCodeStreamStart "NetStream.Play.Start"
#define StatusCodePublishStart "NetStream.Publish.Start"
#define StatusCodeDataStart "NetStream.Data.Start"
#define StatusCodeUnpublishSuccess "NetStream.Unpublish.Success"

// FMLE
#define RTMP_AMF0_COMMAND_ON_FC_PUBLISH		"onFCPublish"
#define RTMP_AMF0_COMMAND_ON_FC_UNPUBLISH	"onFCUnpublish"

// default stream id for response the createStream request.
#define RSS_DEFAULT_SID 1

RssRequest::RssRequest()
{
	objectEncoding = RTMP_SIG_AMF0_VER;
}

RssRequest::~RssRequest()
{
}

int RssRequest::discovery_app()
{
	int ret = ERROR_SUCCESS;

	size_t pos = std::string::npos;
	std::string url = tcUrl;

	if ((pos = url.find("://")) != std::string::npos)
	{
		schema = url.substr(0, pos);
		url = url.substr(schema.length() + 3);
		rss_verbose("discovery schema=%s", schema.c_str());
	}

	if ((pos = url.find("/")) != std::string::npos)
	{
		vhost = url.substr(0, pos);
		url = url.substr(vhost.length() + 1);
		rss_verbose("discovery vhost=%s", vhost.c_str());
	}

	port = "1935";
	if ((pos = vhost.find(":")) != std::string::npos)
	{
		port = vhost.substr(pos + 1);
		vhost = vhost.substr(0, pos);
		rss_verbose("discovery vhost=%s, port=%s", vhost.c_str(), port.c_str());
	}

	app = url;
	rss_info("discovery app success. schema=%s, vhost=%s, port=%s, app=%s",
	         schema.c_str(), vhost.c_str(), port.c_str(), app.c_str());

	if (schema.empty() || vhost.empty() || port.empty() || app.empty())
	{
		ret = ERROR_RTMP_REQ_TCURL;
		rss_error("discovery tcUrl failed. "
		          "tcUrl=%s, schema=%s, vhost=%s, port=%s, app=%s, ret=%d",
		          tcUrl.c_str(), schema.c_str(), vhost.c_str(), port.c_str(), app.c_str(), ret);
		return ret;
	}

	return ret;
}

std::string RssRequest::get_stream_url()
{
	std::string url = "";

	//url += vhost;

	url += "/";
	url += app;
	url += "/";
	url += stream;

	return url;
}

RssResponse::RssResponse()
{
	stream_id = RSS_DEFAULT_SID;
}

RssResponse::~RssResponse()
{
}

RssRtmp::RssRtmp(st_netfd_t client_stfd)
{
	protocol = new RssProtocol(client_stfd);
	stfd = client_stfd;
}

RssRtmp::~RssRtmp()
{
	rss_freep(protocol);
}

void RssRtmp::set_recv_timeout(int timeout_ms)
{
	return protocol->set_recv_timeout(timeout_ms);
}

void RssRtmp::set_send_timeout(int timeout_ms)
{
	return protocol->set_send_timeout(timeout_ms);
}

int RssRtmp::recv_message(RssCommonMessage** pmsg)
{
	return protocol->recv_message(pmsg);
}

int RssRtmp::send_message(IRssMessage* msg)
{
	return protocol->send_message(msg);
}

int RssRtmp::handshake()
{
	int ret = ERROR_SUCCESS;

	ssize_t nsize;
	RssSocket skt(stfd);

	char* c0c1 = new char[1537];
	RssAutoFree(char, c0c1, true);
	if ((ret = skt.read_fully(c0c1, 1537, &nsize)) != ERROR_SUCCESS)
	{
		rss_warn("read c0c1 failed. ret=%d", ret);
		return ret;
	}
	rss_verbose("read c0c1 success.");

	// plain text required.
	if (c0c1[0] != 0x03)
	{
		ret = ERROR_RTMP_PLAIN_REQUIRED;
		rss_warn("only support rtmp plain text. ret=%d", ret);
		return ret;
	}
	rss_verbose("check c0 success, required plain text.");

	char* s0s1s2 = new char[3073];
	RssAutoFree(char, s0s1s2, true);
	// plain text required.
	s0s1s2[0] = 0x03;
	if ((ret = skt.write(s0s1s2, 3073, &nsize)) != ERROR_SUCCESS)
	{
		rss_warn("send s0s1s2 failed. ret=%d", ret);
		return ret;
	}
	rss_verbose("send s0s1s2 success.");

	char* c2 = new char[1536];
	RssAutoFree(char, c2, true);
	if ((ret = skt.read_fully(c2, 1536, &nsize)) != ERROR_SUCCESS)
	{
		rss_warn("read c2 failed. ret=%d", ret);
		return ret;
	}
	rss_verbose("read c2 success.");

	rss_trace("handshake success.");

	return ret;
}

int RssRtmp::connect_app(RssRequest* req)
{
	int ret = ERROR_SUCCESS;

	RssCommonMessage* msg = NULL;
	RssConnectAppPacket* pkt = NULL;
	if ((ret = rss_rtmp_expect_message<RssConnectAppPacket>(protocol, &msg, &pkt)) != ERROR_SUCCESS)
	{
		rss_error("expect connect app message failed. ret=%d", ret);
		return ret;
	}
	RssAutoFree(RssCommonMessage, msg, false);
	rss_info("get connect app message");

	RssAmf0Any* prop = NULL;

	if ((prop = pkt->command_object->ensure_property_string("tcUrl")) == NULL)
	{
		ret = ERROR_RTMP_REQ_CONNECT;
		rss_error("invalid request, must specifies the tcUrl. ret=%d", ret);
		return ret;
	}
	req->tcUrl = rss_amf0_convert<RssAmf0String>(prop)->value;

	if ((prop = pkt->command_object->ensure_property_string("pageUrl")) != NULL)
	{
		req->pageUrl = rss_amf0_convert<RssAmf0String>(prop)->value;
	}

	if ((prop = pkt->command_object->ensure_property_string("swfUrl")) != NULL)
	{
		req->swfUrl = rss_amf0_convert<RssAmf0String>(prop)->value;
	}

	if ((prop = pkt->command_object->ensure_property_number("objectEncoding")) != NULL)
	{
		req->objectEncoding = rss_amf0_convert<RssAmf0Number>(prop)->value;
	}

	rss_info("get connect app message params success.");

	return req->discovery_app();
}

int RssRtmp::set_window_ack_size(int ack_size)
{
	int ret = ERROR_SUCCESS;

	RssCommonMessage* msg = new RssCommonMessage();
	RssSetWindowAckSizePacket* pkt = new RssSetWindowAckSizePacket();

	pkt->ackowledgement_window_size = ack_size;
	msg->set_packet(pkt, 0);

	if ((ret = protocol->send_message(msg)) != ERROR_SUCCESS)
	{
		rss_error("send ack size message failed. ret=%d", ret);
		return ret;
	}
	rss_info("send ack size message success. ack_size=%d", ack_size);

	return ret;
}

int RssRtmp::set_peer_bandwidth(int bandwidth, int type)
{
	int ret = ERROR_SUCCESS;

	RssCommonMessage* msg = new RssCommonMessage();
	RssSetPeerBandwidthPacket* pkt = new RssSetPeerBandwidthPacket();

	pkt->bandwidth = bandwidth;
	pkt->type = type;
	msg->set_packet(pkt, 0);

	if ((ret = protocol->send_message(msg)) != ERROR_SUCCESS)
	{
		rss_error("send set bandwidth message failed. ret=%d", ret);
		return ret;
	}
	rss_info("send set bandwidth message "
	         "success. bandwidth=%d, type=%d", bandwidth, type);

	return ret;
}

int RssRtmp::response_connect_app(RssRequest* req)
{
	int ret = ERROR_SUCCESS;

	RssCommonMessage* msg = new RssCommonMessage();
	RssConnectAppResPacket* pkt = new RssConnectAppResPacket();

	pkt->props->set("fmsVer", new RssAmf0String("FMS/" RTMP_SIG_FMS_VER));
	pkt->props->set("capabilities", new RssAmf0Number(127));
	pkt->props->set("mode", new RssAmf0Number(1));

	pkt->info->set(StatusLevel, new RssAmf0String(StatusLevelStatus));
	pkt->info->set(StatusCode, new RssAmf0String(StatusCodeConnectSuccess));
	pkt->info->set(StatusDescription, new RssAmf0String("Connection succeeded"));
	pkt->info->set("objectEncoding", new RssAmf0Number(req->objectEncoding));
	RssARssAmf0EcmaArray* data = new RssARssAmf0EcmaArray();
	pkt->info->set("data", data);

	data->set("version", new RssAmf0String(RTMP_SIG_FMS_VER));
	data->set("server", new RssAmf0String(RTMP_SIG_RSS_NAME));
	data->set("rss_url", new RssAmf0String(RTMP_SIG_RSS_URL));
	data->set("rss_version", new RssAmf0String(RTMP_SIG_RSS_VERSION));

	msg->set_packet(pkt, 0);

	if ((ret = protocol->send_message(msg)) != ERROR_SUCCESS)
	{
		rss_error("send connect app response message failed. ret=%d", ret);
		return ret;
	}
	rss_info("send connect app response message success.");

	return ret;
}

int RssRtmp::on_bw_done()
{
	int ret = ERROR_SUCCESS;

	RssCommonMessage* msg = new RssCommonMessage();
	RssOnBWDonePacket* pkt = new RssOnBWDonePacket();

	msg->set_packet(pkt, 0);

	if ((ret = protocol->send_message(msg)) != ERROR_SUCCESS)
	{
		rss_error("send onBWDone message failed. ret=%d", ret);
		return ret;
	}
	rss_info("send onBWDone message success.");

	return ret;
}

int RssRtmp::identify_client(int stream_id, RssClientType& type, std::string& stream_name)
{
	type = RssClientUnknown;
	int ret = ERROR_SUCCESS;

	while (true)
	{
		RssCommonMessage* msg = NULL;
		if ((ret = protocol->recv_message(&msg)) != ERROR_SUCCESS)
		{
			rss_error("recv identify client message failed. ret=%d", ret);
			return ret;
		}

		RssAutoFree(RssCommonMessage, msg, false);

		if (!msg->header.is_amf0_command() && !msg->header.is_amf3_command())
		{
			rss_trace("identify ignore messages except "
			          "AMF0/AMF3 command message. type=%#x", msg->header.message_type);
			continue;
		}

		if ((ret = msg->decode_packet()) != ERROR_SUCCESS)
		{
			rss_error("identify decode message failed. ret=%d", ret);
			return ret;
		}

		RssPacket* pkt = msg->get_packet();
		if (dynamic_cast<RssCreateStreamPacket*>(pkt))
		{
			rss_info("identify client by create stream, play or flash publish.");
			return identify_create_stream_client(
			           dynamic_cast<RssCreateStreamPacket*>(pkt), stream_id, type, stream_name);
		}
		if (dynamic_cast<RssFMLEStartPacket*>(pkt))
		{
			rss_info("identify client by releaseStream, fmle publish.");
			return identify_fmle_publish_client(
			           dynamic_cast<RssFMLEStartPacket*>(pkt), type, stream_name);
		}

		rss_trace("ignore AMF0/AMF3 command message.");
	}

	return ret;
}

int RssRtmp::set_chunk_size(int chunk_size)
{
	int ret = ERROR_SUCCESS;

	RssCommonMessage* msg = new RssCommonMessage();
	RssSetChunkSizePacket* pkt = new RssSetChunkSizePacket();

	pkt->chunk_size = chunk_size;
	msg->set_packet(pkt, 0);

	if ((ret = protocol->send_message(msg)) != ERROR_SUCCESS)
	{
		rss_error("send set chunk size message failed. ret=%d", ret);
		return ret;
	}
	rss_info("send set chunk size message success. chunk_size=%d", chunk_size);

	return ret;
}

int RssRtmp::start_play(int stream_id)
{
	int ret = ERROR_SUCCESS;

	// StreamBegin
	if (true)
	{
		RssCommonMessage* msg = new RssCommonMessage();
		RssPCUC4BytesPacket* pkt = new RssPCUC4BytesPacket();

		pkt->event_type = SrcPCUCStreamBegin;
		pkt->event_data = stream_id;
		msg->set_packet(pkt, 0);

		if ((ret = protocol->send_message(msg)) != ERROR_SUCCESS)
		{
			rss_error("send PCUC(StreamBegin) message failed. ret=%d", ret);
			return ret;
		}
		rss_info("send PCUC(StreamBegin) message success.");
	}

	// onStatus(NetStream.Play.Reset)
	if (true)
	{
		RssCommonMessage* msg = new RssCommonMessage();
		RssOnStatusCallPacket* pkt = new RssOnStatusCallPacket();

		pkt->data->set(StatusLevel, new RssAmf0String(StatusLevelStatus));
		pkt->data->set(StatusCode, new RssAmf0String(StatusCodeStreamReset));
		pkt->data->set(StatusDescription, new RssAmf0String("Playing and resetting stream."));
		pkt->data->set(StatusDetails, new RssAmf0String("stream"));
		pkt->data->set(StatusClientId, new RssAmf0String(RTMP_SIG_CLIENT_ID));

		msg->set_packet(pkt, stream_id);

		if ((ret = protocol->send_message(msg)) != ERROR_SUCCESS)
		{
			rss_error("send onStatus(NetStream.Play.Reset) message failed. ret=%d", ret);
			return ret;
		}
		rss_info("send onStatus(NetStream.Play.Reset) message success.");
	}

	// onStatus(NetStream.Play.Start)
	if (true)
	{
		RssCommonMessage* msg = new RssCommonMessage();
		RssOnStatusCallPacket* pkt = new RssOnStatusCallPacket();

		pkt->data->set(StatusLevel, new RssAmf0String(StatusLevelStatus));
		pkt->data->set(StatusCode, new RssAmf0String(StatusCodeStreamStart));
		pkt->data->set(StatusDescription, new RssAmf0String("Started playing stream."));
		pkt->data->set(StatusDetails, new RssAmf0String("stream"));
		pkt->data->set(StatusClientId, new RssAmf0String(RTMP_SIG_CLIENT_ID));

		msg->set_packet(pkt, stream_id);

		if ((ret = protocol->send_message(msg)) != ERROR_SUCCESS)
		{
			rss_error("send onStatus(NetStream.Play.Reset) message failed. ret=%d", ret);
			return ret;
		}
		rss_info("send onStatus(NetStream.Play.Reset) message success.");
	}

	// |RtmpSampleAccess(false, false)
	if (true)
	{
		RssCommonMessage* msg = new RssCommonMessage();
		RssSampleAccessPacket* pkt = new RssSampleAccessPacket();

		msg->set_packet(pkt, stream_id);

		if ((ret = protocol->send_message(msg)) != ERROR_SUCCESS)
		{
			rss_error("send |RtmpSampleAccess(false, false) message failed. ret=%d", ret);
			return ret;
		}
		rss_info("send |RtmpSampleAccess(false, false) message success.");
	}

	// onStatus(NetStream.Data.Start)
	if (true)
	{
		RssCommonMessage* msg = new RssCommonMessage();
		RssOnStatusDataPacket* pkt = new RssOnStatusDataPacket();

		pkt->data->set(StatusCode, new RssAmf0String(StatusCodeDataStart));

		msg->set_packet(pkt, stream_id);

		if ((ret = protocol->send_message(msg)) != ERROR_SUCCESS)
		{
			rss_error("send onStatus(NetStream.Data.Start) message failed. ret=%d", ret);
			return ret;
		}
		rss_info("send onStatus(NetStream.Data.Start) message success.");
	}

	rss_info("start play success.");

	return ret;
}

int RssRtmp::start_publish(int stream_id)
{
	int ret = ERROR_SUCCESS;

	// FCPublish
	double fc_publish_tid = 0;
	if (true)
	{
		RssCommonMessage* msg = NULL;
		RssFMLEStartPacket* pkt = NULL;
		if ((ret = rss_rtmp_expect_message<RssFMLEStartPacket>(protocol, &msg, &pkt)) != ERROR_SUCCESS)
		{
			rss_error("recv FCPublish message failed. ret=%d", ret);
			return ret;
		}
		rss_info("recv FCPublish request message success.");

		RssAutoFree(RssCommonMessage, msg, false);
		fc_publish_tid = pkt->transaction_id;
	}
	// FCPublish response
	if (true)
	{
		RssCommonMessage* msg = new RssCommonMessage();
		RssFMLEStartResPacket* pkt = new RssFMLEStartResPacket(fc_publish_tid);

		msg->set_packet(pkt, 0);

		if ((ret = protocol->send_message(msg)) != ERROR_SUCCESS)
		{
			rss_error("send FCPublish response message failed. ret=%d", ret);
			return ret;
		}
		rss_info("send FCPublish response message success.");
	}

	// createStream
	double create_stream_tid = 0;
	if (true)
	{
		RssCommonMessage* msg = NULL;
		RssCreateStreamPacket* pkt = NULL;
		if ((ret = rss_rtmp_expect_message<RssCreateStreamPacket>(protocol, &msg, &pkt)) != ERROR_SUCCESS)
		{
			rss_error("recv createStream message failed. ret=%d", ret);
			return ret;
		}
		rss_info("recv createStream request message success.");

		RssAutoFree(RssCommonMessage, msg, false);
		create_stream_tid = pkt->transaction_id;
	}
	// createStream response
	if (true)
	{
		RssCommonMessage* msg = new RssCommonMessage();
		RssCreateStreamResPacket* pkt = new RssCreateStreamResPacket(create_stream_tid, stream_id);

		msg->set_packet(pkt, 0);

		if ((ret = protocol->send_message(msg)) != ERROR_SUCCESS)
		{
			rss_error("send createStream response message failed. ret=%d", ret);
			return ret;
		}
		rss_info("send createStream response message success.");
	}

	// publish
	if (true)
	{
		RssCommonMessage* msg = NULL;
		RssPublishPacket* pkt = NULL;
		if ((ret = rss_rtmp_expect_message<RssPublishPacket>(protocol, &msg, &pkt)) != ERROR_SUCCESS)
		{
			rss_error("recv publish message failed. ret=%d", ret);
			return ret;
		}
		rss_info("recv publish request message success.");

		RssAutoFree(RssCommonMessage, msg, false);
	}
	// publish response onFCPublish(NetStream.Publish.Start)
	if (true)
	{
		RssCommonMessage* msg = new RssCommonMessage();
		RssOnStatusCallPacket* pkt = new RssOnStatusCallPacket();

		pkt->command_name = RTMP_AMF0_COMMAND_ON_FC_PUBLISH;
		pkt->data->set(StatusCode, new RssAmf0String(StatusCodePublishStart));
		pkt->data->set(StatusDescription, new RssAmf0String("Started publishing stream."));

		msg->set_packet(pkt, stream_id);

		if ((ret = protocol->send_message(msg)) != ERROR_SUCCESS)
		{
			rss_error("send onFCPublish(NetStream.Publish.Start) message failed. ret=%d", ret);
			return ret;
		}
		rss_info("send onFCPublish(NetStream.Publish.Start) message success.");
	}
	// publish response onStatus(NetStream.Publish.Start)
	if (true)
	{
		RssCommonMessage* msg = new RssCommonMessage();
		RssOnStatusCallPacket* pkt = new RssOnStatusCallPacket();

		pkt->data->set(StatusLevel, new RssAmf0String(StatusLevelStatus));
		pkt->data->set(StatusCode, new RssAmf0String(StatusCodePublishStart));
		pkt->data->set(StatusDescription, new RssAmf0String("Started publishing stream."));
		pkt->data->set(StatusClientId, new RssAmf0String(RTMP_SIG_CLIENT_ID));

		msg->set_packet(pkt, stream_id);

		if ((ret = protocol->send_message(msg)) != ERROR_SUCCESS)
		{
			rss_error("send onStatus(NetStream.Publish.Start) message failed. ret=%d", ret);
			return ret;
		}
		rss_info("send onStatus(NetStream.Publish.Start) message success.");
	}

	return ret;
}

int RssRtmp::fmle_unpublish(int stream_id, double unpublish_tid)
{
	int ret = ERROR_SUCCESS;

	// publish response onFCUnpublish(NetStream.unpublish.Success)
	if (true)
	{
		RssCommonMessage* msg = new RssCommonMessage();
		RssOnStatusCallPacket* pkt = new RssOnStatusCallPacket();

		pkt->command_name = RTMP_AMF0_COMMAND_ON_FC_UNPUBLISH;
		pkt->data->set(StatusCode, new RssAmf0String(StatusCodeUnpublishSuccess));
		pkt->data->set(StatusDescription, new RssAmf0String("Stop publishing stream."));

		msg->set_packet(pkt, stream_id);

		if ((ret = protocol->send_message(msg)) != ERROR_SUCCESS)
		{
			rss_error("send onFCUnpublish(NetStream.unpublish.Success) message failed. ret=%d", ret);
			return ret;
		}
		rss_info("send onFCUnpublish(NetStream.unpublish.Success) message success.");
	}
	// FCUnpublish response
	if (true)
	{
		RssCommonMessage* msg = new RssCommonMessage();
		RssFMLEStartResPacket* pkt = new RssFMLEStartResPacket(unpublish_tid);

		msg->set_packet(pkt, stream_id);

		if ((ret = protocol->send_message(msg)) != ERROR_SUCCESS)
		{
			rss_error("send FCUnpublish response message failed. ret=%d", ret);
			return ret;
		}
		rss_info("send FCUnpublish response message success.");
	}
	// publish response onStatus(NetStream.Unpublish.Success)
	if (true)
	{
		RssCommonMessage* msg = new RssCommonMessage();
		RssOnStatusCallPacket* pkt = new RssOnStatusCallPacket();

		pkt->data->set(StatusLevel, new RssAmf0String(StatusLevelStatus));
		pkt->data->set(StatusCode, new RssAmf0String(StatusCodeUnpublishSuccess));
		pkt->data->set(StatusDescription, new RssAmf0String("Stream is now unpublished"));
		pkt->data->set(StatusClientId, new RssAmf0String(RTMP_SIG_CLIENT_ID));

		msg->set_packet(pkt, stream_id);

		if ((ret = protocol->send_message(msg)) != ERROR_SUCCESS)
		{
			rss_error("send onStatus(NetStream.Unpublish.Success) message failed. ret=%d", ret);
			return ret;
		}
		rss_info("send onStatus(NetStream.Unpublish.Success) message success.");
	}

	rss_info("FMLE unpublish success.");

	return ret;
}

int RssRtmp::identify_create_stream_client(RssCreateStreamPacket* req, int stream_id, RssClientType& type, std::string& stream_name)
{
	int ret = ERROR_SUCCESS;

	if (true)
	{
		RssCommonMessage* msg = new RssCommonMessage();
		RssCreateStreamResPacket* pkt = new RssCreateStreamResPacket(req->transaction_id, stream_id);

		msg->set_packet(pkt, 0);

		if ((ret = protocol->send_message(msg)) != ERROR_SUCCESS)
		{
			rss_error("send createStream response message failed. ret=%d", ret);
			return ret;
		}
		rss_info("send createStream response message success.");
	}

	while (true)
	{
		RssCommonMessage* msg = NULL;
		if ((ret = protocol->recv_message(&msg)) != ERROR_SUCCESS)
		{
			rss_error("recv identify client message failed. ret=%d", ret);
			return ret;
		}

		RssAutoFree(RssCommonMessage, msg, false);

		if (!msg->header.is_amf0_command() && !msg->header.is_amf3_command())
		{
			rss_trace("identify ignore messages except "
			          "AMF0/AMF3 command message. type=%#x", msg->header.message_type);
			continue;
		}

		if ((ret = msg->decode_packet()) != ERROR_SUCCESS)
		{
			rss_error("identify decode message failed. ret=%d", ret);
			return ret;
		}

		RssPacket* pkt = msg->get_packet();
		if (dynamic_cast<RssPlayPacket*>(pkt))
		{
			RssPlayPacket* play = dynamic_cast<RssPlayPacket*>(pkt);
			type = RssClientPlay;
			stream_name = play->stream_name;
			rss_trace("identity client type=play, stream_name=%s", stream_name.c_str());
			return ret;
		}

		rss_trace("ignore AMF0/AMF3 command message.");
	}

	return ret;
}

int RssRtmp::identify_fmle_publish_client(RssFMLEStartPacket* req, RssClientType& type, std::string& stream_name)
{
	int ret = ERROR_SUCCESS;

	type = RssClientPublish;
	stream_name = req->stream_name;

	// releaseStream response
	if (true)
	{
		RssCommonMessage* msg = new RssCommonMessage();
		RssFMLEStartResPacket* pkt = new RssFMLEStartResPacket(req->transaction_id);

		msg->set_packet(pkt, 0);

		if ((ret = protocol->send_message(msg)) != ERROR_SUCCESS)
		{
			rss_error("send releaseStream response message failed. ret=%d", ret);
			return ret;
		}
		rss_info("send releaseStream response message success.");
	}

	return ret;
}