#ifndef RSS_CORE_PROTOCOL_HPP
#define RSS_CORE_PROTOCOL_HPP

/*
#include <rss_core_protocol.hpp>
*/

#include <rss_core.hpp>

#include <map>
#include <string>

#include <st.h>

#include <rss_core_log.hpp>
#include <rss_core_error.hpp>

class RssSocket;
class RssBuffer;
class RssPacket;
class RssStream;
class RssCommonMessage;
class RssChunkStream;
class RssAmf0Object;
class RssAmf0Null;
class RssAmf0Undefined;
class IRssMessage;

// convert class name to string.
#define CLASS_NAME_STRING(className) #className

/**
* max rtmp header size:
* 	1bytes basic header,
* 	11bytes message header,
* 	4bytes timestamp header,
* that is, 1+11+4=16bytes.
*/
#define RTMP_MAX_FMT0_HEADER_SIZE 16
/**
* max rtmp header size:
* 	1bytes basic header,
* 	4bytes timestamp header,
* that is, 1+4=5bytes.
*/
#define RTMP_MAX_FMT3_HEADER_SIZE 5

/**
* the protocol provides the rtmp-message-protocol services,
* to recv RTMP message from RTMP chunk stream,
* and to send out RTMP message over RTMP chunk stream.
*/
class RssProtocol
{
// peer in/out
private:
	st_netfd_t stfd;
	RssSocket* skt;
	char* pp;
// peer in
private:
	std::map<int, RssChunkStream*> chunk_streams;
	RssBuffer* buffer;
	int32_t in_chunk_size;
// peer out
private:
	char out_header_fmt0[RTMP_MAX_FMT0_HEADER_SIZE];
	char out_header_fmt3[RTMP_MAX_FMT3_HEADER_SIZE];
	int32_t out_chunk_size;
public:
	RssProtocol(st_netfd_t client_stfd);
	virtual ~RssProtocol();
public:
	/**
	* set the timeout in ms.
	* if timeout, recv/send message return ERROR_SOCKET_TIMEOUT.
	*/
	virtual void set_recv_timeout(int timeout_ms);
	virtual void set_send_timeout(int timeout_ms);
	/**
	* recv a message with raw/undecoded payload from peer.
	* the payload is not decoded, use rss_rtmp_expect_message<T> if requires
	* specifies message.
	* @pmsg, user must free it. NULL if not success.
	* @remark, only when success, user can use and must free the pmsg.
	*/
	virtual int recv_message(RssCommonMessage** pmsg);
	/**
	* send out message with encoded payload to peer.
	* use the message encode method to encode to payload,
	* then sendout over socket.
	* @msg this method will free it whatever return value.
	*/
	virtual int send_message(IRssMessage* msg);
private:
	/**
	* when recv message, update the context.
	*/
	virtual int on_recv_message(RssCommonMessage* msg);
	/**
	* when message sentout, update the context.
	*/
	virtual int on_send_message(IRssMessage* msg);
	/**
	* try to recv interlaced message from peer,
	* return error if error occur and nerver set the pmsg,
	* return success and pmsg set to NULL if no entire message got,
	* return success and pmsg set to entire message if got one.
	*/
	virtual int recv_interlaced_message(RssCommonMessage** pmsg);
	/**
	* read the chunk basic header(fmt, cid) from chunk stream.
	* user can discovery a RssChunkStream by cid.
	* @bh_size return the chunk basic header size, to remove the used bytes when finished.
	*/
	virtual int read_basic_header(char& fmt, int& cid, int& bh_size);
	/**
	* read the chunk message header(timestamp, payload_length, message_type, stream_id)
	* from chunk stream and save to RssChunkStream.
	* @mh_size return the chunk message header size, to remove the used bytes when finished.
	*/
	virtual int read_message_header(RssChunkStream* chunk, char fmt, int bh_size, int& mh_size);
	/**
	* read the chunk payload, remove the used bytes in buffer,
	* if got entire message, set the pmsg.
	* @payload_size read size in this roundtrip, generally a chunk size or left message size.
	*/
	virtual int read_message_payload(RssChunkStream* chunk, int bh_size, int mh_size, int& payload_size, RssCommonMessage** pmsg);
};

/**
* 4.1. Message Header
*/
struct RssMessageHeader
{
	/**
	* One byte field to represent the message type. A range of type IDs
	* (1-7) are reserved for protocol control messages.
	*/
	int8_t message_type;
	/**
	* Three-byte field that represents the size of the payload in bytes.
	* It is set in big-endian format.
	*/
	int32_t payload_length;
	/**
	* Three-byte field that contains a timestamp delta of the message.
	* The 4 bytes are packed in the big-endian order.
	*/
	int32_t timestamp_delta;
	/**
	* Three-byte field that identifies the stream of the message. These
	* bytes are set in big-endian format.
	*/
	int32_t stream_id;

	/**
	* Four-byte field that contains a timestamp of the message.
	* The 4 bytes are packed in the big-endian order.
	*/
	int32_t timestamp;

	RssMessageHeader();
	virtual ~RssMessageHeader();

	bool is_audio();
	bool is_video();
	bool is_amf0_command();
	bool is_amf0_data();
	bool is_amf3_command();
	bool is_amf3_data();
	bool is_window_ackledgement_size();
	bool is_set_chunk_size();
};

/**
* incoming chunk stream maybe interlaced,
* use the chunk stream to cache the input RTMP chunk streams.
*/
class RssChunkStream
{
public:
	/**
	* represents the basic header fmt,
	* which used to identify the variant message header type.
	*/
	char fmt;
	/**
	* represents the basic header cid,
	* which is the chunk stream id.
	*/
	int cid;
	/**
	* cached message header
	*/
	RssMessageHeader header;
	/**
	* whether the chunk message header has extended timestamp.
	*/
	bool extended_timestamp;
	/**
	* partially read message.
	*/
	RssCommonMessage* msg;
	/**
	* decoded msg count, to identify whether the chunk stream is fresh.
	*/
	int64_t msg_count;
public:
	RssChunkStream(int _cid);
	virtual ~RssChunkStream();
};

/**
* message to output.
*/
class IRssMessage
{
// 4.1. Message Header
public:
	RssMessageHeader header;
// 4.2. Message Payload
public:
	/**
	* The other part which is the payload is the actual data that is
	* contained in the message. For example, it could be some audio samples
	* or compressed video data. The payload format and interpretation are
	* beyond the scope of this document.
	*/
	int32_t size;
	int8_t* payload;
public:
	IRssMessage();
	virtual ~IRssMessage();
public:
	/**
	* whether message canbe decoded.
	* only update the context when message canbe decoded.
	*/
	virtual bool can_decode() = 0;
	/**
	* encode functions.
	*/
public:
	/**
	* get the perfered cid(chunk stream id) which sendout over.
	*/
	virtual int get_perfer_cid() = 0;
	/**
	* encode the packet to message payload bytes.
	* @remark there exists empty packet, so maybe the payload is NULL.
	*/
	virtual int encode_packet() = 0;
};

/**
* common RTMP message defines in rtmp.part2.Message-Formats.pdf.
* cannbe parse and decode.
*/
class RssCommonMessage : public IRssMessage
{
private:
	typedef IRssMessage super;
// decoded message payload.
private:
	RssStream* stream;
	RssPacket* packet;
public:
	RssCommonMessage();
	virtual ~RssCommonMessage();
public:
	virtual bool can_decode();
	/**
	* decode functions.
	*/
public:
	/**
	* decode packet from message payload.
	*/
	virtual int decode_packet();
	/**
	* get the decoded packet which decoded by decode_packet().
	* @remark, user never free the pkt, the message will auto free it.
	*/
	virtual RssPacket* get_packet();
	/**
	* encode functions.
	*/
public:
	/**
	* get the perfered cid(chunk stream id) which sendout over.
	*/
	virtual int get_perfer_cid();
	/**
	* set the encoded packet to encode_packet() to payload.
	* @stream_id, the id of stream which is created by createStream.
	* @remark, user never free the pkt, the message will auto free it.
	*/
	virtual void set_packet(RssPacket* pkt, int stream_id);
	/**
	* encode the packet to message payload bytes.
	* @remark there exists empty packet, so maybe the payload is NULL.
	*/
	virtual int encode_packet();
};

/**
* shared ptr message.
* for audio/video/data message that need less memory copy.
* and only for output.
*/
class RssSharedPtrMessage : public IRssMessage
{
private:
	typedef IRssMessage super;
private:
	struct RssSharedPtr
	{
		char* payload;
		int size;
		int perfer_cid;
		int shared_count;

		RssSharedPtr();
		virtual ~RssSharedPtr();
	};
	RssSharedPtr* ptr;
public:
	RssSharedPtrMessage();
	virtual ~RssSharedPtrMessage();
public:
	virtual bool can_decode();
public:
	/**
	* set the shared payload.
	*/
	virtual int initialize(IRssMessage* msg, char* payload, int size);
	virtual RssSharedPtrMessage* copy();
public:
	/**
	* get the perfered cid(chunk stream id) which sendout over.
	*/
	virtual int get_perfer_cid();
	/**
	* ignored.
	* for shared message, nothing should be done.
	* use initialize() to set the data.
	*/
	virtual int encode_packet();
};

/**
* the decoded message payload.
*/
class RssPacket
{
protected:
	/**
	* subpacket must override to provide the right class name.
	*/
	virtual const char* get_class_name()
	{
		return CLASS_NAME_STRING(RssPacket);
	}
public:
	RssPacket();
	virtual ~RssPacket();
	/**
	* decode functions.
	*/
public:
	/**
	* subpacket must override to decode packet from stream.
	* @remark never invoke the super.decode, it always failed.
	*/
	virtual int decode(RssStream* stream);
	/**
	* encode functions.
	*/
public:
	virtual int get_perfer_cid();
	virtual int get_payload_length();
public:
	/**
	* subpacket must override to provide the right message type.
	*/
	virtual int get_message_type();
	/**
	* the subpacket can override this encode,
	* for example, video and audio will directly set the payload withou memory copy,
	* other packet which need to serialize/encode to bytes by override the
	* get_size and encode_packet.
	*/
	virtual int encode(int& size, char*& payload);
protected:
	/**
	* subpacket can override to calc the packet size.
	*/
	virtual int get_size();
	/**
	* subpacket can override to encode the payload to stream.
	* @remark never invoke the super.encode_packet, it always failed.
	*/
	virtual int encode_packet(RssStream* stream);
};

/**
* 4.1.1. connect
* The client sends the connect command to the server to request
* connection to a server application instance.
*/
class RssConnectAppPacket : public RssPacket
{
private:
	typedef RssPacket super;
protected:
	virtual const char* get_class_name()
	{
		return CLASS_NAME_STRING(RssConnectAppPacket);
	}
public:
	std::string command_name;
	double transaction_id;
	RssAmf0Object* command_object;
public:
	RssConnectAppPacket();
	virtual ~RssConnectAppPacket();
public:
	virtual int decode(RssStream* stream);
};
/**
* response for RssConnectAppPacket.
*/
class RssConnectAppResPacket : public RssPacket
{
private:
	typedef RssPacket super;
protected:
	virtual const char* get_class_name()
	{
		return CLASS_NAME_STRING(RssConnectAppResPacket);
	}
public:
	std::string command_name;
	double transaction_id;
	RssAmf0Object* props;
	RssAmf0Object* info;
public:
	RssConnectAppResPacket();
	virtual ~RssConnectAppResPacket();
public:
	virtual int get_perfer_cid();
public:
	virtual int get_message_type();
protected:
	virtual int get_size();
	virtual int encode_packet(RssStream* stream);
};

/**
* 4.1.3. createStream
* The client sends this command to the server to create a logical
* channel for message communication The publishing of audio, video, and
* metadata is carried out over stream channel created using the
* createStream command.
*/
class RssCreateStreamPacket : public RssPacket
{
private:
	typedef RssPacket super;
protected:
	virtual const char* get_class_name()
	{
		return CLASS_NAME_STRING(RssCreateStreamPacket);
	}
public:
	std::string command_name;
	double transaction_id;
	RssAmf0Null* command_object;
public:
	RssCreateStreamPacket();
	virtual ~RssCreateStreamPacket();
public:
	virtual int decode(RssStream* stream);
};
/**
* response for RssCreateStreamPacket.
*/
class RssCreateStreamResPacket : public RssPacket
{
private:
	typedef RssPacket super;
protected:
	virtual const char* get_class_name()
	{
		return CLASS_NAME_STRING(RssCreateStreamResPacket);
	}
public:
	std::string command_name;
	double transaction_id;
	RssAmf0Null* command_object;
	double stream_id;
public:
	RssCreateStreamResPacket(double _transaction_id, double _stream_id);
	virtual ~RssCreateStreamResPacket();
public:
	virtual int get_perfer_cid();
public:
	virtual int get_message_type();
protected:
	virtual int get_size();
	virtual int encode_packet(RssStream* stream);
};

/**
* FMLE start publish: ReleaseStream/PublishStream
*/
class RssFMLEStartPacket : public RssPacket
{
private:
	typedef RssPacket super;
protected:
	virtual const char* get_class_name()
	{
		return CLASS_NAME_STRING(RssFMLEStartPacket);
	}
public:
	std::string command_name;
	double transaction_id;
	RssAmf0Null* command_object;
	std::string stream_name;
public:
	RssFMLEStartPacket();
	virtual ~RssFMLEStartPacket();
public:
	virtual int decode(RssStream* stream);
};
/**
* response for RssFMLEStartPacket.
*/
class RssFMLEStartResPacket : public RssPacket
{
private:
	typedef RssPacket super;
protected:
	virtual const char* get_class_name()
	{
		return CLASS_NAME_STRING(RssFMLEStartResPacket);
	}
public:
	std::string command_name;
	double transaction_id;
	RssAmf0Null* command_object;
	RssAmf0Undefined* args;
public:
	RssFMLEStartResPacket(double _transaction_id);
	virtual ~RssFMLEStartResPacket();
public:
	virtual int get_perfer_cid();
public:
	virtual int get_message_type();
protected:
	virtual int get_size();
	virtual int encode_packet(RssStream* stream);
};

/**
* FMLE/flash publish
* 4.2.6. Publish
* The client sends the publish command to publish a named stream to the
* server. Using this name, any client can play this stream and receive
* the published audio, video, and data messages.
*/
class RssPublishPacket : public RssPacket
{
private:
	typedef RssPacket super;
protected:
	virtual const char* get_class_name()
	{
		return CLASS_NAME_STRING(RssPublishPacket);
	}
public:
	std::string command_name;
	double transaction_id;
	RssAmf0Null* command_object;
	std::string stream_name;
	std::string type;
public:
	RssPublishPacket();
	virtual ~RssPublishPacket();
public:
	virtual int decode(RssStream* stream);
};

/**
* 4.2.1. play
* The client sends this command to the server to play a stream.
*/
class RssPlayPacket : public RssPacket
{
private:
	typedef RssPacket super;
protected:
	virtual const char* get_class_name()
	{
		return CLASS_NAME_STRING(RssPlayPacket);
	}
public:
	std::string command_name;
	double transaction_id;
	RssAmf0Null* command_object;
	std::string stream_name;
	double start;
	double duration;
	bool reset;
public:
	RssPlayPacket();
	virtual ~RssPlayPacket();
public:
	virtual int decode(RssStream* stream);
};
/**
* response for RssPlayPacket.
* @remark, user must set the stream_id in header.
*/
class RssPlayResPacket : public RssPacket
{
private:
	typedef RssPacket super;
protected:
	virtual const char* get_class_name()
	{
		return CLASS_NAME_STRING(RssPlayResPacket);
	}
public:
	std::string command_name;
	double transaction_id;
	RssAmf0Null* command_object;
	RssAmf0Object* desc;
public:
	RssPlayResPacket();
	virtual ~RssPlayResPacket();
public:
	virtual int get_perfer_cid();
public:
	virtual int get_message_type();
protected:
	virtual int get_size();
	virtual int encode_packet(RssStream* stream);
};

/**
* when bandwidth test done, notice client.
*/
class RssOnBWDonePacket : public RssPacket
{
private:
	typedef RssPacket super;
protected:
	virtual const char* get_class_name()
	{
		return CLASS_NAME_STRING(RssOnBWDonePacket);
	}
public:
	std::string command_name;
	double transaction_id;
	RssAmf0Null* args;
public:
	RssOnBWDonePacket();
	virtual ~RssOnBWDonePacket();
public:
	virtual int get_perfer_cid();
public:
	virtual int get_message_type();
protected:
	virtual int get_size();
	virtual int encode_packet(RssStream* stream);
};

/**
* onStatus command, AMF0 Call
* @remark, user must set the stream_id by RssMessage.set_packet().
*/
class RssOnStatusCallPacket : public RssPacket
{
private:
	typedef RssPacket super;
protected:
	virtual const char* get_class_name()
	{
		return CLASS_NAME_STRING(RssOnStatusCallPacket);
	}
public:
	std::string command_name;
	double transaction_id;
	RssAmf0Null* args;
	RssAmf0Object* data;
public:
	RssOnStatusCallPacket();
	virtual ~RssOnStatusCallPacket();
public:
	virtual int get_perfer_cid();
public:
	virtual int get_message_type();
protected:
	virtual int get_size();
	virtual int encode_packet(RssStream* stream);
};

/**
* onStatus data, AMF0 Data
* @remark, user must set the stream_id by RssMessage.set_packet().
*/
class RssOnStatusDataPacket : public RssPacket
{
private:
	typedef RssPacket super;
protected:
	virtual const char* get_class_name()
	{
		return CLASS_NAME_STRING(RssOnStatusDataPacket);
	}
public:
	std::string command_name;
	RssAmf0Object* data;
public:
	RssOnStatusDataPacket();
	virtual ~RssOnStatusDataPacket();
public:
	virtual int get_perfer_cid();
public:
	virtual int get_message_type();
protected:
	virtual int get_size();
	virtual int encode_packet(RssStream* stream);
};

/**
* AMF0Data RtmpSampleAccess
* @remark, user must set the stream_id by RssMessage.set_packet().
*/
class RssSampleAccessPacket : public RssPacket
{
private:
	typedef RssPacket super;
protected:
	virtual const char* get_class_name()
	{
		return CLASS_NAME_STRING(RssSampleAccessPacket);
	}
public:
	std::string command_name;
	bool video_sample_access;
	bool audio_sample_access;
public:
	RssSampleAccessPacket();
	virtual ~RssSampleAccessPacket();
public:
	virtual int get_perfer_cid();
public:
	virtual int get_message_type();
protected:
	virtual int get_size();
	virtual int encode_packet(RssStream* stream);
};

/**
* the stream metadata.
* FMLE: @setDataFrame
* others: onMetaData
*/
class RssOnMetaDataPacket : public RssPacket
{
private:
	typedef RssPacket super;
protected:
	virtual const char* get_class_name()
	{
		return CLASS_NAME_STRING(RssOnMetaDataPacket);
	}
public:
	std::string name;
	RssAmf0Object* metadata;
public:
	RssOnMetaDataPacket();
	virtual ~RssOnMetaDataPacket();
public:
	virtual int decode(RssStream* stream);
public:
	virtual int get_perfer_cid();
public:
	virtual int get_message_type();
protected:
	virtual int get_size();
	virtual int encode_packet(RssStream* stream);
};

/**
* 5.5. Window Acknowledgement Size (5)
* The client or the server sends this message to inform the peer which
* window size to use when sending acknowledgment.
*/
class RssSetWindowAckSizePacket : public RssPacket
{
private:
	typedef RssPacket super;
protected:
	virtual const char* get_class_name()
	{
		return CLASS_NAME_STRING(RssSetWindowAckSizePacket);
	}
public:
	int32_t ackowledgement_window_size;
public:
	RssSetWindowAckSizePacket();
	virtual ~RssSetWindowAckSizePacket();
public:
	virtual int decode(RssStream* stream);
public:
	virtual int get_perfer_cid();
public:
	virtual int get_message_type();
protected:
	virtual int get_size();
	virtual int encode_packet(RssStream* stream);
};

/**
* 7.1. Set Chunk Size
* Protocol control message 1, Set Chunk Size, is used to notify the
* peer about the new maximum chunk size.
*/
class RssSetChunkSizePacket : public RssPacket
{
private:
	typedef RssPacket super;
protected:
	virtual const char* get_class_name()
	{
		return CLASS_NAME_STRING(RssSetChunkSizePacket);
	}
public:
	int32_t chunk_size;
public:
	RssSetChunkSizePacket();
	virtual ~RssSetChunkSizePacket();
public:
	virtual int decode(RssStream* stream);
public:
	virtual int get_perfer_cid();
public:
	virtual int get_message_type();
protected:
	virtual int get_size();
	virtual int encode_packet(RssStream* stream);
};

/**
* 5.6. Set Peer Bandwidth (6)
* The client or the server sends this message to update the output
* bandwidth of the peer.
*/
class RssSetPeerBandwidthPacket : public RssPacket
{
private:
	typedef RssPacket super;
protected:
	virtual const char* get_class_name()
	{
		return CLASS_NAME_STRING(RssSetPeerBandwidthPacket);
	}
public:
	int32_t bandwidth;
	int8_t type;
public:
	RssSetPeerBandwidthPacket();
	virtual ~RssSetPeerBandwidthPacket();
public:
	virtual int get_perfer_cid();
public:
	virtual int get_message_type();
protected:
	virtual int get_size();
	virtual int encode_packet(RssStream* stream);
};

// 3.7. User Control message
enum SrcPCUCEventType
{
	// generally, 4bytes event-data
	SrcPCUCStreamBegin 		= 0x00,
	SrcPCUCStreamEOF 			= 0x01,
	SrcPCUCStreamDry 			= 0x02,
	SrcPCUCSetBufferLength 	= 0x03, // 8bytes event-data
	SrcPCUCStreamIsRecorded 	= 0x04,
	SrcPCUCPingRequest 		= 0x06,
	SrcPCUCPingResponse 		= 0x07,
};

/**
* for the EventData is 4bytes.
* Stream Begin(=0)			4-bytes stream ID
* Stream EOF(=1)			4-bytes stream ID
* StreamDry(=2)				4-bytes stream ID
* StreamIsRecorded(=4)		4-bytes stream ID
* PingRequest(=6)			4-bytes timestamp local server time
* PingResponse(=7)			4-bytes timestamp received ping request.
*
* 3.7. User Control message
* +------------------------------+-------------------------
* | Event Type ( 2- bytes ) | Event Data
* +------------------------------+-------------------------
* Figure 5 Pay load for the ‘User Control Message’.
*/
class RssPCUC4BytesPacket : public RssPacket
{
private:
	typedef RssPacket super;
protected:
	virtual const char* get_class_name()
	{
		return CLASS_NAME_STRING(RssPCUC4BytesPacket);
	}
public:
	int16_t event_type;
	int32_t event_data;
public:
	RssPCUC4BytesPacket();
	virtual ~RssPCUC4BytesPacket();
public:
	virtual int get_perfer_cid();
public:
	virtual int get_message_type();
protected:
	virtual int get_size();
	virtual int encode_packet(RssStream* stream);
};

/**
* expect a specified message, drop others util got specified one.
* @pmsg, user must free it. NULL if not success.
* @ppacket, store in the pmsg, user must never free it. NULL if not success.
* @remark, only when success, user can use and must free the pmsg/ppacket.
*/
template<class T>
int rss_rtmp_expect_message(RssProtocol* protocol, RssCommonMessage** pmsg, T** ppacket)
{
	*pmsg = NULL;
	*ppacket = NULL;

	int ret = ERROR_SUCCESS;

	while (true)
	{
		RssCommonMessage* msg = NULL;
		if ((ret = protocol->recv_message(&msg)) != ERROR_SUCCESS)
		{
			rss_error("recv message failed. ret=%d", ret);
			return ret;
		}
		rss_verbose("recv message success.");

		if ((ret = msg->decode_packet()) != ERROR_SUCCESS)
		{
			delete msg;
			rss_error("decode message failed. ret=%d", ret);
			return ret;
		}

		T* pkt = dynamic_cast<T*>(msg->get_packet());
		if (!pkt)
		{
			delete msg;
			rss_trace("drop message(type=%d, size=%d, time=%d, sid=%d).",
			          msg->header.message_type, msg->header.payload_length,
			          msg->header.timestamp, msg->header.stream_id);
			continue;
		}

		*pmsg = msg;
		*ppacket = pkt;
		break;
	}

	return ret;
}

#endif