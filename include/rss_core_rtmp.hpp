#ifndef RSS_CORE_RTMP_HPP
#define RSS_CORE_RTMP_HPP

/*
#include <rss_core_rtmp.hpp>
*/

#include <rss_core.hpp>

#include <string>

#include <st.h>

class RssProtocol;
class IRssMessage;
class RssCommonMessage;
class RssCreateStreamPacket;
class RssFMLEStartPacket;

/**
* the original request from client.
*/
struct RssRequest
{
	std::string tcUrl;
	std::string pageUrl;
	std::string swfUrl;
	double objectEncoding;

	std::string schema;
	std::string vhost;
	std::string port;
	std::string app;
	std::string stream;

	RssRequest();
	virtual ~RssRequest();

	/**
	* disconvery vhost/app from tcUrl.
	*/
	virtual int discovery_app();
	virtual std::string get_stream_url();
};

/**
* the response to client.
*/
struct RssResponse
{
	int stream_id;

	RssResponse();
	virtual ~RssResponse();
};

/**
* the rtmp client type.
*/
enum RssClientType
{
	RssClientUnknown,
	RssClientPlay,
	RssClientPublish,
};

/**
* the rtmp provices rtmp-command-protocol services,
* a high level protocol, media stream oriented services,
* such as connect to vhost/app, play stream, get audio/video data.
*/
class RssRtmp
{
private:
	RssProtocol* protocol;
	st_netfd_t stfd;
public:
	RssRtmp(st_netfd_t client_stfd);
	virtual ~RssRtmp();
public:
	virtual void set_recv_timeout(int timeout_ms);
	virtual void set_send_timeout(int timeout_ms);
	virtual int recv_message(RssCommonMessage** pmsg);
	virtual int send_message(IRssMessage* msg);
public:
	virtual int handshake();
	virtual int connect_app(RssRequest* req);
	virtual int set_window_ack_size(int ack_size);
	/**
	* @type: The sender can mark this message hard (0), soft (1), or dynamic (2)
	* using the Limit type field.
	*/
	virtual int set_peer_bandwidth(int bandwidth, int type);
	virtual int response_connect_app(RssRequest* req);
	virtual int on_bw_done();
	/**
	* recv some message to identify the client.
	* @stream_id, client will createStream to play or publish by flash,
	* 		the stream_id used to response the createStream request.
	* @type, output the client type.
	*/
	virtual int identify_client(int stream_id, RssClientType& type, std::string& stream_name);
	/**
	* set the chunk size when client type identified.
	*/
	virtual int set_chunk_size(int chunk_size);
	/**
	* when client type is play, response with packets:
	* StreamBegin,
	* onStatus(NetStream.Play.Reset), onStatus(NetStream.Play.Start).,
	* |RtmpSampleAccess(false, false),
	* onStatus(NetStream.Data.Start).
	*/
	virtual int start_play(int stream_id);
	/**
	* when client type is publish, response with packets:
	* releaseStream response
	* FCPublish
	* FCPublish response
	* createStream response
	* onFCPublish(NetStream.Publish.Start)
	* onStatus(NetStream.Publish.Start)
	*/
	virtual int start_publish(int stream_id);
	/**
	* process the FMLE unpublish event.
	* @unpublish_tid the unpublish request transaction id.
	*/
	virtual int fmle_unpublish(int stream_id, double unpublish_tid);
private:
	virtual int identify_create_stream_client(RssCreateStreamPacket* req, int stream_id, RssClientType& type, std::string& stream_name);
	virtual int identify_fmle_publish_client(RssFMLEStartPacket* req, RssClientType& type, std::string& stream_name);
};

#endif