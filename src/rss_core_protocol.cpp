#include <rss_core_protocol.hpp>

#include <rss_core_log.hpp>
#include <rss_core_amf0.hpp>
#include <rss_core_error.hpp>
#include <rss_core_socket.hpp>
#include <rss_core_buffer.hpp>
#include <rss_core_stream.hpp>
#include <rss_core_auto_free.hpp>

/****************************************************************************
*****************************************************************************
****************************************************************************/
/**
5. Protocol Control Messages
RTMP reserves message type IDs 1-7 for protocol control messages.
These messages contain information needed by the RTM Chunk Stream
protocol or RTMP itself. Protocol messages with IDs 1 & 2 are
reserved for usage with RTM Chunk Stream protocol. Protocol messages
with IDs 3-6 are reserved for usage of RTMP. Protocol message with ID
7 is used between edge server and origin server.
*/
#define RTMP_MSG_SetChunkSize 				0x01
#define RTMP_MSG_AbortMessage 				0x02
#define RTMP_MSG_Acknowledgement 			0x03
#define RTMP_MSG_UserControlMessage 		0x04
#define RTMP_MSG_WindowAcknowledgementSize 	0x05
#define RTMP_MSG_SetPeerBandwidth 			0x06
#define RTMP_MSG_EdgeAndOriginServerCommand 0x07
/**
3. Types of messages
The server and the client send messages over the network to
communicate with each other. The messages can be of any type which
includes audio messages, video messages, command messages, shared
object messages, data messages, and user control messages.
3.1. Command message
Command messages carry the AMF-encoded commands between the client
and the server. These messages have been assigned message type value
of 20 for AMF0 encoding and message type value of 17 for AMF3
encoding. These messages are sent to perform some operations like
connect, createStream, publish, play, pause on the peer. Command
messages like onstatus, result etc. are used to inform the sender
about the status of the requested commands. A command message
consists of command name, transaction ID, and command object that
contains related parameters. A client or a server can request Remote
Procedure Calls (RPC) over streams that are communicated using the
command messages to the peer.
*/
#define RTMP_MSG_AMF3CommandMessage 		17 // 0x11
#define RTMP_MSG_AMF0CommandMessage 		20 // 0x14
/**
3.2. Data message
The client or the server sends this message to send Metadata or any
user data to the peer. Metadata includes details about the
data(audio, video etc.) like creation time, duration, theme and so
on. These messages have been assigned message type value of 18 for
AMF0 and message type value of 15 for AMF3.        
*/
#define RTMP_MSG_AMF0DataMessage 			18 // 0x12
#define RTMP_MSG_AMF3DataMessage 			15 // 0x0F
/**
3.3. Shared object message
A shared object is a Flash object (a collection of name value pairs)
that are in synchronization across multiple clients, instances, and
so on. The message types kMsgContainer=19 for AMF0 and
kMsgContainerEx=16 for AMF3 are reserved for shared object events.
Each message can contain multiple events.
*/
#define RTMP_MSG_AMF3SharedObject 			16 // 0x10
#define RTMP_MSG_AMF0SharedObject 			19 // 0x13
/**
3.4. Audio message
The client or the server sends this message to send audio data to the
peer. The message type value of 8 is reserved for audio messages.
*/
#define RTMP_MSG_AudioMessage 				8 // 0x08
/* *
3.5. Video message
The client or the server sends this message to send video data to the
peer. The message type value of 9 is reserved for video messages.
These messages are large and can delay the sending of other type of
messages. To avoid such a situation, the video message is assigned
the lowest priority.
*/
#define RTMP_MSG_VideoMessage 				9 // 0x09
/**
3.6. Aggregate message
An aggregate message is a single message that contains a list of submessages.
The message type value of 22 is reserved for aggregate
messages.
*/
#define RTMP_MSG_AggregateMessage 			22 // 0x16

/****************************************************************************
*****************************************************************************
****************************************************************************/
/**
* 6.1.2. Chunk Message Header
* There are four different formats for the chunk message header,
* selected by the "fmt" field in the chunk basic header.
*/
// 6.1.2.1. Type 0
// Chunks of Type 0 are 11 bytes long. This type MUST be used at the
// start of a chunk stream, and whenever the stream timestamp goes
// backward (e.g., because of a backward seek).
#define RTMP_FMT_TYPE0 						0
// 6.1.2.2. Type 1
// Chunks of Type 1 are 7 bytes long. The message stream ID is not
// included; this chunk takes the same stream ID as the preceding chunk.
// Streams with variable-sized messages (for example, many video
// formats) SHOULD use this format for the first chunk of each new
// message after the first.
#define RTMP_FMT_TYPE1 						1
// 6.1.2.3. Type 2
// Chunks of Type 2 are 3 bytes long. Neither the stream ID nor the
// message length is included; this chunk has the same stream ID and
// message length as the preceding chunk. Streams with constant-sized
// messages (for example, some audio and data formats) SHOULD use this
// format for the first chunk of each message after the first.
#define RTMP_FMT_TYPE2 						2
// 6.1.2.4. Type 3
// Chunks of Type 3 have no header. Stream ID, message length and
// timestamp delta are not present; chunks of this type take values from
// the preceding chunk. When a single message is split into chunks, all
// chunks of a message except the first one, SHOULD use this type. Refer
// to example 2 in section 6.2.2. Stream consisting of messages of
// exactly the same size, stream ID and spacing in time SHOULD use this
// type for all chunks after chunk of Type 2. Refer to example 1 in
// section 6.2.1. If the delta between the first message and the second
// message is same as the time stamp of first message, then chunk of
// type 3 would immediately follow the chunk of type 0 as there is no
// need for a chunk of type 2 to register the delta. If Type 3 chunk
// follows a Type 0 chunk, then timestamp delta for this Type 3 chunk is
// the same as the timestamp of Type 0 chunk.
#define RTMP_FMT_TYPE3 						3

/****************************************************************************
*****************************************************************************
****************************************************************************/
/**
* 6. Chunking
* The chunk size is configurable. It can be set using a control
* message(Set Chunk Size) as described in section 7.1. The maximum
* chunk size can be 65536 bytes and minimum 128 bytes. Larger values
* reduce CPU usage, but also commit to larger writes that can delay
* other content on lower bandwidth connections. Smaller chunks are not
* good for high-bit rate streaming. Chunk size is maintained
* independently for each direction.
*/
#define RTMP_DEFAULT_CHUNK_SIZE 			128
#define RTMP_MIN_CHUNK_SIZE 				2

/**
* 6.1. Chunk Format
* Extended timestamp: 0 or 4 bytes
* This field MUST be sent when the normal timsestamp is set to
* 0xffffff, it MUST NOT be sent if the normal timestamp is set to
* anything else. So for values less than 0xffffff the normal
* timestamp field SHOULD be used in which case the extended timestamp
* MUST NOT be present. For values greater than or equal to 0xffffff
* the normal timestamp field MUST NOT be used and MUST be set to
* 0xffffff and the extended timestamp MUST be sent.
*/
#define RTMP_EXTENDED_TIMESTAMP 			0xFFFFFF

/****************************************************************************
*****************************************************************************
****************************************************************************/
/**
* amf0 command message, command name macros
*/
#define RTMP_AMF0_COMMAND_CONNECT			"connect"
#define RTMP_AMF0_COMMAND_CREATE_STREAM		"createStream"
#define RTMP_AMF0_COMMAND_PLAY				"play"
#define RTMP_AMF0_COMMAND_ON_BW_DONE		"onBWDone"
#define RTMP_AMF0_COMMAND_ON_STATUS			"onStatus"
#define RTMP_AMF0_COMMAND_RESULT			"_result"
#define RTMP_AMF0_COMMAND_RELEASE_STREAM	"releaseStream"
#define RTMP_AMF0_COMMAND_FC_PUBLISH		"FCPublish"
#define RTMP_AMF0_COMMAND_UNPUBLISH			"FCUnpublish"
#define RTMP_AMF0_COMMAND_PUBLISH			"publish"
#define RTMP_AMF0_DATA_SAMPLE_ACCESS		"|RtmpSampleAccess"
#define RTMP_AMF0_DATA_SET_DATAFRAME		"@setDataFrame"
#define RTMP_AMF0_DATA_ON_METADATA			"onMetaData"

/****************************************************************************
*****************************************************************************
****************************************************************************/
/**
* the chunk stream id used for some under-layer message,
* for example, the PC(protocol control) message.
*/
#define RTMP_CID_ProtocolControl 0x02
/**
* the AMF0/AMF3 command message, invoke method and return the result, over NetConnection.
* generally use 0x03.
*/
#define RTMP_CID_OverConnection 0x03
/**
* the AMF0/AMF3 command message, invoke method and return the result, over NetConnection, 
* the midst state(we guess).
* rarely used, e.g. onStatus(NetStream.Play.Reset).
*/
#define RTMP_CID_OverConnection2 0x04
/**
* the stream message(amf0/amf3), over NetStream.
* generally use 0x05.
*/
#define RTMP_CID_OverStream 0x05
/**
* the stream message(amf0/amf3), over NetStream, the midst state(we guess).
* rarely used, e.g. play("mp4:mystram.f4v")
*/
#define RTMP_CID_OverStream2 0x08
/**
* the stream message(video), over NetStream
* generally use 0x06.
*/
#define RTMP_CID_Video 0x06
/**
* the stream message(audio), over NetStream.
* generally use 0x07.
*/
#define RTMP_CID_Audio 0x07

/****************************************************************************
*****************************************************************************
****************************************************************************/

RssProtocol::RssProtocol(st_netfd_t client_stfd)
{
	stfd = client_stfd;
	buffer = new RssBuffer();
	skt = new RssSocket(stfd);
	
	in_chunk_size = out_chunk_size = RTMP_DEFAULT_CHUNK_SIZE;
}

RssProtocol::~RssProtocol()
{
	std::map<int, RssChunkStream*>::iterator it;
	
	for (it = chunk_streams.begin(); it != chunk_streams.end(); ++it) {
		RssChunkStream* stream = it->second;
		rss_freep(stream);
	}

	chunk_streams.clear();
	
	rss_freep(buffer);
	rss_freep(skt);
}

void RssProtocol::set_recv_timeout(int timeout_ms)
{
	return skt->set_recv_timeout(timeout_ms);
}

void RssProtocol::set_send_timeout(int timeout_ms)
{
	return skt->set_send_timeout(timeout_ms);
}

int RssProtocol::recv_message(RssCommonMessage** pmsg)
{
	*pmsg = NULL;
	
	int ret = ERROR_SUCCESS;
	
	while (true) {
		RssCommonMessage* msg = NULL;
		
		if ((ret = recv_interlaced_message(&msg)) != ERROR_SUCCESS) {
			if (ret != ERROR_SOCKET_TIMEOUT) {
				rss_error("recv interlaced message failed. ret=%d", ret);
			}
			return ret;
		}
		rss_verbose("entire msg received");
		
		if (!msg) {
			continue;
		}
		
		if (msg->size <= 0 || msg->header.payload_length <= 0) {
			rss_trace("ignore empty message(type=%d, size=%d, time=%d, sid=%d).",
				msg->header.message_type, msg->header.payload_length,
				msg->header.timestamp, msg->header.stream_id);
			rss_freep(msg);
			continue;
		}
		
		if ((ret = on_recv_message(msg)) != ERROR_SUCCESS) {
			rss_error("hook the received msg failed. ret=%d", ret);
			rss_freep(msg);
			return ret;
		}
		
		rss_verbose("get a msg with raw/undecoded payload");
		*pmsg = msg;
		break;
	}
	
	return ret;
}

int RssProtocol::send_message(IRssMessage* msg)
{
	int ret = ERROR_SUCCESS;
	
	// free msg whatever return value.
	RssAutoFree(IRssMessage, msg, false);
	
	if ((ret = msg->encode_packet()) != ERROR_SUCCESS) {
		rss_error("encode packet to message payload failed. ret=%d", ret);
		return ret;
	}
	rss_info("encode packet to message payload success");

	// p set to current write position,
	// it's ok when payload is NULL and size is 0.
	char* p = (char*)msg->payload;
	
	// always write the header event payload is empty.
	do {
		// generate the header.
		char* pheader = NULL;
		int header_size = 0;
		
		if (p == (char*)msg->payload) {
			// write new chunk stream header, fmt is 0
			pheader = out_header_fmt0;
			*pheader++ = 0x00 | (msg->get_perfer_cid() & 0x3F);
			
		    // chunk message header, 11 bytes
		    // timestamp, 3bytes, big-endian
			if (msg->header.timestamp >= RTMP_EXTENDED_TIMESTAMP) {
		        *pheader++ = 0xFF;
		        *pheader++ = 0xFF;
		        *pheader++ = 0xFF;
			} else {
		        pp = (char*)&msg->header.timestamp; 
		        *pheader++ = pp[2];
		        *pheader++ = pp[1];
		        *pheader++ = pp[0];
			}
			
		    // message_length, 3bytes, big-endian
		    pp = (char*)&msg->header.payload_length;
		    *pheader++ = pp[2];
		    *pheader++ = pp[1];
		    *pheader++ = pp[0];
		    
		    // message_type, 1bytes
		    *pheader++ = msg->header.message_type;
		    
		    // message_length, 3bytes, little-endian
		    pp = (char*)&msg->header.stream_id;
		    *pheader++ = pp[0];
		    *pheader++ = pp[1];
		    *pheader++ = pp[2];
		    *pheader++ = pp[3];
		    
		    // chunk extended timestamp header, 0 or 4 bytes, big-endian
		    if(msg->header.timestamp >= RTMP_EXTENDED_TIMESTAMP){
		        pp = (char*)&msg->header.timestamp; 
		        *pheader++ = pp[3];
		        *pheader++ = pp[2];
		        *pheader++ = pp[1];
		        *pheader++ = pp[0];
		    }
			
			header_size = pheader - out_header_fmt0;
			pheader = out_header_fmt0;
		} else {
			// write no message header chunk stream, fmt is 3
			pheader = out_header_fmt3;
			*pheader++ = 0xC0 | (msg->get_perfer_cid() & 0x3F);
		    
		    // chunk extended timestamp header, 0 or 4 bytes, big-endian
		    if(msg->header.timestamp >= RTMP_EXTENDED_TIMESTAMP){
		        pp = (char*)&msg->header.timestamp; 
		        *pheader++ = pp[3];
		        *pheader++ = pp[2];
		        *pheader++ = pp[1];
		        *pheader++ = pp[0];
		    }
			
			header_size = pheader - out_header_fmt3;
			pheader = out_header_fmt3;
		}
		
		// sendout header and payload by writev.
		// decrease the sys invoke count to get higher performance.
		int payload_size = msg->size - (p - (char*)msg->payload);
		payload_size = rss_min(payload_size, out_chunk_size);
		
		// send by writev
		iovec iov[2];
		iov[0].iov_base = pheader;
		iov[0].iov_len = header_size;
		iov[1].iov_base = p;
		iov[1].iov_len = payload_size;
		
		ssize_t nwrite;
		if ((ret = skt->writev(iov, 2, &nwrite)) != ERROR_SUCCESS) {
			rss_error("send with writev failed. ret=%d", ret);
			return ret;
		}
		
		// consume sendout bytes when not empty packet.
		if (msg->payload && msg->size > 0) {
			p += payload_size;
		}
	} while (p < (char*)msg->payload + msg->size);
	
	if ((ret = on_send_message(msg)) != ERROR_SUCCESS) {
		rss_error("hook the send message failed. ret=%d", ret);
		return ret;
	}
	
	return ret;
}

int RssProtocol::on_recv_message(RssCommonMessage* msg)
{
	int ret = ERROR_SUCCESS;
	
	rss_assert(msg != NULL);
	
	switch (msg->header.message_type) {
		case RTMP_MSG_SetChunkSize:
		case RTMP_MSG_WindowAcknowledgementSize:
			if ((ret = msg->decode_packet()) != ERROR_SUCCESS) {
				rss_error("decode packet from message payload failed. ret=%d", ret);
				return ret;
			}
			rss_verbose("decode packet from message payload success.");
			break;
	}
	
	switch (msg->header.message_type) {
		case RTMP_MSG_WindowAcknowledgementSize: {
			RssSetWindowAckSizePacket* pkt = dynamic_cast<RssSetWindowAckSizePacket*>(msg->get_packet());
			rss_assert(pkt != NULL);
			// TODO: take effect.
			rss_trace("set ack window size to %d", pkt->ackowledgement_window_size);
			break;
		}
		case RTMP_MSG_SetChunkSize: {
			RssSetChunkSizePacket* pkt = dynamic_cast<RssSetChunkSizePacket*>(msg->get_packet());
			rss_assert(pkt != NULL);
			
			in_chunk_size = pkt->chunk_size;
			
			rss_trace("set input chunk size to %d", pkt->chunk_size);
			break;
		}
	}
	
	return ret;
}

int RssProtocol::on_send_message(IRssMessage* msg)
{
	int ret = ERROR_SUCCESS;
	
	if (!msg->can_decode()) {
		rss_verbose("ignore the un-decodable message.");
		return ret;
	}
	
	RssCommonMessage* common_msg = dynamic_cast<RssCommonMessage*>(msg);
	if (!msg) {
		rss_verbose("ignore the shared ptr message.");
		return ret;
	}
	
	rss_assert(common_msg != NULL);
	
	switch (common_msg->header.message_type) {
		case RTMP_MSG_SetChunkSize: {
			RssSetChunkSizePacket* pkt = dynamic_cast<RssSetChunkSizePacket*>(common_msg->get_packet());
			rss_assert(pkt != NULL);
			
			out_chunk_size = pkt->chunk_size;
			
			rss_trace("set output chunk size to %d", pkt->chunk_size);
			break;
		}
	}
	
	return ret;
}

int RssProtocol::recv_interlaced_message(RssCommonMessage** pmsg)
{
	int ret = ERROR_SUCCESS;
	
	// chunk stream basic header.
	char fmt = 0;
	int cid = 0;
	int bh_size = 0;
	if ((ret = read_basic_header(fmt, cid, bh_size)) != ERROR_SUCCESS) {
		if (ret != ERROR_SOCKET_TIMEOUT) {
			rss_error("read basic header failed. ret=%d", ret);
		}
		return ret;
	}
	rss_info("read basic header success. fmt=%d, cid=%d, bh_size=%d", fmt, cid, bh_size);
	
	// get the cached chunk stream.
	RssChunkStream* chunk = NULL;
	
	if (chunk_streams.find(cid) == chunk_streams.end()) {
		chunk = chunk_streams[cid] = new RssChunkStream(cid);
		rss_info("cache new chunk stream: fmt=%d, cid=%d", fmt, cid);
	} else {
		chunk = chunk_streams[cid];
		rss_info("cached chunk stream: fmt=%d, cid=%d, size=%d, message(type=%d, size=%d, time=%d, sid=%d)",
			chunk->fmt, chunk->cid, (chunk->msg? chunk->msg->size : 0), chunk->header.message_type, chunk->header.payload_length,
			chunk->header.timestamp, chunk->header.stream_id);
	}

	// chunk stream message header
	int mh_size = 0;
	if ((ret = read_message_header(chunk, fmt, bh_size, mh_size)) != ERROR_SUCCESS) {
		if (ret != ERROR_SOCKET_TIMEOUT) {
			rss_error("read message header failed. ret=%d", ret);
		}
		return ret;
	}
	rss_info("read message header success. "
			"fmt=%d, mh_size=%d, ext_time=%d, size=%d, message(type=%d, size=%d, time=%d, sid=%d)", 
			fmt, mh_size, chunk->extended_timestamp, (chunk->msg? chunk->msg->size : 0), chunk->header.message_type, 
			chunk->header.payload_length, chunk->header.timestamp, chunk->header.stream_id);
	
	// read msg payload from chunk stream.
	RssCommonMessage* msg = NULL;
	int payload_size = 0;
	if ((ret = read_message_payload(chunk, bh_size, mh_size, payload_size, &msg)) != ERROR_SUCCESS) {
		if (ret != ERROR_SOCKET_TIMEOUT) {
			rss_error("read message payload failed. ret=%d", ret);
		}
		return ret;
	}
	
	// not got an entire RTMP message, try next chunk.
	if (!msg) {
		rss_info("get partial message success. chunk_payload_size=%d, size=%d, message(type=%d, size=%d, time=%d, sid=%d)",
				payload_size, (msg? msg->size : (chunk->msg? chunk->msg->size : 0)), chunk->header.message_type, chunk->header.payload_length,
				chunk->header.timestamp, chunk->header.stream_id);
		return ret;
	}
	
	*pmsg = msg;
	rss_info("get entire message success. chunk_payload_size=%d, size=%d, message(type=%d, size=%d, time=%d, sid=%d)",
			payload_size, (msg? msg->size : (chunk->msg? chunk->msg->size : 0)), chunk->header.message_type, chunk->header.payload_length,
			chunk->header.timestamp, chunk->header.stream_id);
			
	return ret;
}

int RssProtocol::read_basic_header(char& fmt, int& cid, int& bh_size)
{
	int ret = ERROR_SUCCESS;
	
	int required_size = 1;
	if ((ret = buffer->ensure_buffer_bytes(skt, required_size)) != ERROR_SUCCESS) {
		if (ret != ERROR_SOCKET_TIMEOUT) {
			rss_error("read 1bytes basic header failed. required_size=%d, ret=%d", required_size, ret);
		}
		return ret;
	}
	
	char* p = buffer->bytes();
	
    fmt = (*p >> 6) & 0x03;
    cid = *p & 0x3f;
    bh_size = 1;
    
    if (cid > 1) {
		rss_verbose("%dbytes basic header parsed. fmt=%d, cid=%d", bh_size, fmt, cid);
        return ret;
    }

	if (cid == 0) {
		required_size = 2;
		if ((ret = buffer->ensure_buffer_bytes(skt, required_size)) != ERROR_SUCCESS) {
			if (ret != ERROR_SOCKET_TIMEOUT) {
				rss_error("read 2bytes basic header failed. required_size=%d, ret=%d", required_size, ret);
			}
			return ret;
		}
		
		cid = 64;
		cid += *(++p);
    	bh_size = 2;
		rss_verbose("%dbytes basic header parsed. fmt=%d, cid=%d", bh_size, fmt, cid);
	} else if (cid == 1) {
		required_size = 3;
		if ((ret = buffer->ensure_buffer_bytes(skt, 3)) != ERROR_SUCCESS) {
			if (ret != ERROR_SOCKET_TIMEOUT) {
				rss_error("read 3bytes basic header failed. required_size=%d, ret=%d", required_size, ret);
			}
			return ret;
		}
		
		cid = 64;
		cid += *(++p);
		cid += *(++p) * 256;
    	bh_size = 3;
		rss_verbose("%dbytes basic header parsed. fmt=%d, cid=%d", bh_size, fmt, cid);
	} else {
		rss_error("invalid path, impossible basic header.");
		rss_assert(false);
	}
	
	return ret;
}

int RssProtocol::read_message_header(RssChunkStream* chunk, char fmt, int bh_size, int& mh_size)
{
	int ret = ERROR_SUCCESS;
	
	/**
	* we should not assert anything about fmt, for the first packet.
	* (when first packet, the chunk->msg is NULL).
	* the fmt maybe 0/1/2/3, the FMLE will send a 0xC4 for some audio packet.
	* the previous packet is:
	* 	04 			// fmt=0, cid=4
	* 	00 00 1a 	// timestamp=26
	*	00 00 9d 	// payload_length=157
	* 	08 			// message_type=8(audio)
	* 	01 00 00 00 // stream_id=1
	* the current packet maybe:
	* 	c4 			// fmt=3, cid=4
	* it's ok, for the packet is audio, and timestamp delta is 26.
	* the current packet must be parsed as:
	* 	fmt=0, cid=4
	* 	timestamp=26+26=52
	* 	payload_length=157
	* 	message_type=8(audio)
	* 	stream_id=1
	* so we must update the timestamp even fmt=3 for first packet.
	*/
	// fresh packet used to update the timestamp even fmt=3 for first packet.
	bool is_fresh_packet = !chunk->msg;
	
	// but, we can ensure that when a chunk stream is fresh, 
	// the fmt must be 0, a new stream.
	if (chunk->msg_count == 0 && fmt != RTMP_FMT_TYPE0) {
		ret = ERROR_RTMP_CHUNK_START;
		rss_error("chunk stream is fresh, "
			"fmt must be %d, actual is %d. ret=%d", RTMP_FMT_TYPE0, fmt, ret);
		return ret;
	}

	// when exists cache msg, means got an partial message,
	// the fmt must not be type0 which means new message.
	if (chunk->msg && fmt == RTMP_FMT_TYPE0) {
		ret = ERROR_RTMP_CHUNK_START;
		rss_error("chunk stream exists, "
			"fmt must not be %d, actual is %d. ret=%d", RTMP_FMT_TYPE0, fmt, ret);
		return ret;
	}
	
	// create msg when new chunk stream start
	if (!chunk->msg) {
		chunk->msg = new RssCommonMessage();
		rss_verbose("create message for new chunk, fmt=%d, cid=%d", fmt, chunk->cid);
	}

	// read message header from socket to buffer.
	static char mh_sizes[] = {11, 7, 3, 0};
	mh_size = mh_sizes[(int)fmt];
	rss_verbose("calc chunk message header size. fmt=%d, mh_size=%d", fmt, mh_size);
	
	int required_size = bh_size + mh_size;
	if ((ret = buffer->ensure_buffer_bytes(skt, required_size)) != ERROR_SUCCESS) {
		if (ret != ERROR_SOCKET_TIMEOUT) {
			rss_error("read %dbytes message header failed. required_size=%d, ret=%d", mh_size, required_size, ret);
		}
		return ret;
	}
	char* p = buffer->bytes() + bh_size;
	
	// parse the message header.
	// see also: ngx_rtmp_recv
	if (fmt <= RTMP_FMT_TYPE2) {
        char* pp = (char*)&chunk->header.timestamp_delta;
        pp[2] = *p++;
        pp[1] = *p++;
        pp[0] = *p++;
        pp[3] = 0;
        
        if (fmt == RTMP_FMT_TYPE0) {
			// 6.1.2.1. Type 0
			// For a type-0 chunk, the absolute timestamp of the message is sent
			// here.
            chunk->header.timestamp = chunk->header.timestamp_delta;
        } else {
            // 6.1.2.2. Type 1
            // 6.1.2.3. Type 2
            // For a type-1 or type-2 chunk, the difference between the previous
            // chunk's timestamp and the current chunk's timestamp is sent here.
            chunk->header.timestamp += chunk->header.timestamp_delta;
        }
        
        // fmt: 0
        // timestamp: 3 bytes
        // If the timestamp is greater than or equal to 16777215
        // (hexadecimal 0x00ffffff), this value MUST be 16777215, and the
        // ‘extended timestamp header’ MUST be present. Otherwise, this value
        // SHOULD be the entire timestamp.
        //
        // fmt: 1 or 2
        // timestamp delta: 3 bytes
        // If the delta is greater than or equal to 16777215 (hexadecimal
        // 0x00ffffff), this value MUST be 16777215, and the ‘extended
        // timestamp header’ MUST be present. Otherwise, this value SHOULD be
        // the entire delta.
        chunk->extended_timestamp = (chunk->header.timestamp_delta >= RTMP_EXTENDED_TIMESTAMP);
        if (chunk->extended_timestamp) {
			chunk->header.timestamp = RTMP_EXTENDED_TIMESTAMP;
        }
        
        if (fmt <= RTMP_FMT_TYPE1) {
            pp = (char*)&chunk->header.payload_length;
            pp[2] = *p++;
            pp[1] = *p++;
            pp[0] = *p++;
            pp[3] = 0;
            
            chunk->header.message_type = *p++;
            
            if (fmt == 0) {
                pp = (char*)&chunk->header.stream_id;
                pp[0] = *p++;
                pp[1] = *p++;
                pp[2] = *p++;
                pp[3] = *p++;
				rss_verbose("header read completed. fmt=%d, mh_size=%d, ext_time=%d, time=%d, payload=%d, type=%d, sid=%d", 
					fmt, mh_size, chunk->extended_timestamp, chunk->header.timestamp, chunk->header.payload_length, 
					chunk->header.message_type, chunk->header.stream_id);
			} else {
				rss_verbose("header read completed. fmt=%d, mh_size=%d, ext_time=%d, time=%d, payload=%d, type=%d", 
					fmt, mh_size, chunk->extended_timestamp, chunk->header.timestamp, chunk->header.payload_length, 
					chunk->header.message_type);
			}
		} else {
			rss_verbose("header read completed. fmt=%d, mh_size=%d, ext_time=%d, time=%d", 
				fmt, mh_size, chunk->extended_timestamp, chunk->header.timestamp);
		}
	} else {
		// update the timestamp even fmt=3 for first stream
		if (is_fresh_packet && !chunk->extended_timestamp) {
			chunk->header.timestamp += chunk->header.timestamp_delta;
		}
		rss_verbose("header read completed. fmt=%d, size=%d, ext_time=%d", 
			fmt, mh_size, chunk->extended_timestamp);
	}
	
	if (chunk->extended_timestamp) {
		mh_size += 4;
		required_size = bh_size + mh_size;
		rss_verbose("read header ext time. fmt=%d, ext_time=%d, mh_size=%d", fmt, chunk->extended_timestamp, mh_size);
		if ((ret = buffer->ensure_buffer_bytes(skt, required_size)) != ERROR_SUCCESS) {
			if (ret != ERROR_SOCKET_TIMEOUT) {
				rss_error("read %dbytes message header failed. required_size=%d, ret=%d", mh_size, required_size, ret);
			}
			return ret;
		}

        char* pp = (char*)&chunk->header.timestamp;
        pp[3] = *p++;
        pp[2] = *p++;
        pp[1] = *p++;
        pp[0] = *p++;
		rss_verbose("header read ext_time completed. time=%d", chunk->header.timestamp);
	}
	
	// valid message
	if (chunk->header.payload_length < 0) {
		ret = ERROR_RTMP_MSG_INVLIAD_SIZE;
		rss_error("RTMP message size must not be negative. size=%d, ret=%d", 
			chunk->header.payload_length, ret);
		return ret;
	}
	
	// copy header to msg
	chunk->msg->header = chunk->header;
	
	// increase the msg count, the chunk stream can accept fmt=1/2/3 message now.
	chunk->msg_count++;
	
	return ret;
}

int RssProtocol::read_message_payload(RssChunkStream* chunk, int bh_size, int mh_size, int& payload_size, RssCommonMessage** pmsg)
{
	int ret = ERROR_SUCCESS;
	
	// empty message
	if (chunk->header.payload_length == 0) {
		// need erase the header in buffer.
		buffer->erase(bh_size + mh_size);
		
		rss_trace("get an empty RTMP "
				"message(type=%d, size=%d, time=%d, sid=%d)", chunk->header.message_type, 
				chunk->header.payload_length, chunk->header.timestamp, chunk->header.stream_id);
		
		*pmsg = chunk->msg;
		chunk->msg = NULL;
				
		return ret;
	}
	rss_assert(chunk->header.payload_length > 0);
	
	// the chunk payload size.
	payload_size = chunk->header.payload_length - chunk->msg->size;
	payload_size = rss_min(payload_size, in_chunk_size);
	rss_verbose("chunk payload size is %d, message_size=%d, received_size=%d, in_chunk_size=%d", 
		payload_size, chunk->header.payload_length, chunk->msg->size, in_chunk_size);

	// create msg payload if not initialized
	if (!chunk->msg->payload) {
		chunk->msg->payload = new int8_t[chunk->header.payload_length];
		memset(chunk->msg->payload, 0, chunk->header.payload_length);
		rss_verbose("create empty payload for RTMP message. size=%d", chunk->header.payload_length);
	}
	
	// read payload to buffer
	int required_size = bh_size + mh_size + payload_size;
	if ((ret = buffer->ensure_buffer_bytes(skt, required_size)) != ERROR_SUCCESS) {
		if (ret != ERROR_SOCKET_TIMEOUT) {
			rss_error("read payload failed. required_size=%d, ret=%d", required_size, ret);
		}
		return ret;
	}
	memcpy(chunk->msg->payload + chunk->msg->size, buffer->bytes() + bh_size + mh_size, payload_size);
	buffer->erase(bh_size + mh_size + payload_size);
	chunk->msg->size += payload_size;
	
	rss_verbose("chunk payload read completed. bh_size=%d, mh_size=%d, payload_size=%d", bh_size, mh_size, payload_size);
	
	// got entire RTMP message?
	if (chunk->header.payload_length == chunk->msg->size) {
		*pmsg = chunk->msg;
		chunk->msg = NULL;
		rss_verbose("get entire RTMP message(type=%d, size=%d, time=%d, sid=%d)", 
				chunk->header.message_type, chunk->header.payload_length, 
				chunk->header.timestamp, chunk->header.stream_id);
		return ret;
	}
	
	rss_verbose("get partial RTMP message(type=%d, size=%d, time=%d, sid=%d), partial size=%d", 
			chunk->header.message_type, chunk->header.payload_length, 
			chunk->header.timestamp, chunk->header.stream_id,
			chunk->msg->size);
	
	return ret;
}

RssMessageHeader::RssMessageHeader()
{
	message_type = 0;
	payload_length = 0;
	timestamp_delta = 0;
	stream_id = 0;
	
	timestamp = 0;
}

RssMessageHeader::~RssMessageHeader()
{
}

bool RssMessageHeader::is_audio()
{
	return message_type == RTMP_MSG_AudioMessage;
}

bool RssMessageHeader::is_video()
{
	return message_type == RTMP_MSG_VideoMessage;
}

bool RssMessageHeader::is_amf0_command()
{
	return message_type == RTMP_MSG_AMF0CommandMessage;
}

bool RssMessageHeader::is_amf0_data()
{
	return message_type == RTMP_MSG_AMF0DataMessage;
}

bool RssMessageHeader::is_amf3_command()
{
	return message_type == RTMP_MSG_AMF3CommandMessage;
}

bool RssMessageHeader::is_amf3_data()
{
	return message_type == RTMP_MSG_AMF3DataMessage;
}

bool RssMessageHeader::is_window_ackledgement_size()
{
	return message_type == RTMP_MSG_WindowAcknowledgementSize;
}

bool RssMessageHeader::is_set_chunk_size()
{
	return message_type == RTMP_MSG_SetChunkSize;
}

RssChunkStream::RssChunkStream(int _cid)
{
	fmt = 0;
	cid = _cid;
	extended_timestamp = false;
	msg = NULL;
	msg_count = 0;
}

RssChunkStream::~RssChunkStream()
{
	rss_freep(msg);
}

IRssMessage::IRssMessage()
{
	payload = NULL;
	size = 0;
}

IRssMessage::~IRssMessage()
{	
}

RssCommonMessage::RssCommonMessage()
{
	stream = NULL;
	packet = NULL;
}

RssCommonMessage::~RssCommonMessage()
{	
	// we must directly free the ptrs,
	// nevery use the virtual functions to delete,
	// for in the destructor, the virtual functions is disabled.
	
	rss_freepa(payload);
	rss_freep(packet);
	rss_freep(stream);
}

bool RssCommonMessage::can_decode()
{
	return true;
}

int RssCommonMessage::decode_packet()
{
	int ret = ERROR_SUCCESS;
	
	rss_assert(payload != NULL);
	rss_assert(size > 0);
	
	if (packet) {
		rss_verbose("msg already decoded");
		return ret;
	}
	
	if (!stream) {
		rss_verbose("create decode stream for message.");
		stream = new RssStream();
	}
	
	// initialize the decode stream for all message,
	// it's ok for the initialize if fast and without memory copy.
	if ((ret = stream->initialize((char*)payload, size)) != ERROR_SUCCESS) {
		rss_error("initialize stream failed. ret=%d", ret);
		return ret;
	}
	rss_verbose("decode stream initialized success");
	
	// decode specified packet type
	if (header.is_amf0_command() || header.is_amf3_command() || header.is_amf0_data() || header.is_amf3_data()) {
		rss_verbose("start to decode AMF0/AMF3 command message.");
		
		// skip 1bytes to decode the amf3 command.
		if (header.is_amf3_command() && stream->require(1)) {
			rss_verbose("skip 1bytes to decode AMF3 command");
			stream->skip(1);
		}
		
		// amf0 command message.
		// need to read the command name.
		std::string command;
		if ((ret = rss_amf0_read_string(stream, command)) != ERROR_SUCCESS) {
			rss_error("decode AMF0/AMF3 command name failed. ret=%d", ret);
			return ret;
		}
		rss_verbose("AMF0/AMF3 command message, command_name=%s", command.c_str());
		
		// reset to zero(amf3 to 1) to restart decode.
		stream->reset();
		if (header.is_amf3_command()) {
			stream->skip(1);
		}
		
		// decode command object.
		if (command == RTMP_AMF0_COMMAND_CONNECT) {
			rss_info("decode the AMF0/AMF3 command(connect vhost/app message).");
			packet = new RssConnectAppPacket();
			return packet->decode(stream);
		} else if(command == RTMP_AMF0_COMMAND_CREATE_STREAM) {
			rss_info("decode the AMF0/AMF3 command(createStream message).");
			packet = new RssCreateStreamPacket();
			return packet->decode(stream);
		} else if(command == RTMP_AMF0_COMMAND_PLAY) {
			rss_info("decode the AMF0/AMF3 command(paly message).");
			packet = new RssPlayPacket();
			return packet->decode(stream);
		} else if(command == RTMP_AMF0_COMMAND_RELEASE_STREAM) {
			rss_info("decode the AMF0/AMF3 command(FMLE releaseStream message).");
			packet = new RssFMLEStartPacket();
			return packet->decode(stream);
		} else if(command == RTMP_AMF0_COMMAND_FC_PUBLISH) {
			rss_info("decode the AMF0/AMF3 command(FMLE FCPublish message).");
			packet = new RssFMLEStartPacket();
			return packet->decode(stream);
		} else if(command == RTMP_AMF0_COMMAND_PUBLISH) {
			rss_info("decode the AMF0/AMF3 command(publish message).");
			packet = new RssPublishPacket();
			return packet->decode(stream);
		} else if(command == RTMP_AMF0_COMMAND_UNPUBLISH) {
			rss_info("decode the AMF0/AMF3 command(unpublish message).");
			packet = new RssFMLEStartPacket();
			return packet->decode(stream);
		} else if(command == RTMP_AMF0_DATA_SET_DATAFRAME || command == RTMP_AMF0_DATA_ON_METADATA) {
			rss_info("decode the AMF0/AMF3 data(onMetaData message).");
			packet = new RssOnMetaDataPacket();
			return packet->decode(stream);
		}
		
		// default packet to drop message.
		rss_trace("drop the AMF0/AMF3 command message, command_name=%s", command.c_str());
		packet = new RssPacket();
		return ret;
	} else if(header.is_window_ackledgement_size()) {
		rss_verbose("start to decode set ack window size message.");
		packet = new RssSetWindowAckSizePacket();
		return packet->decode(stream);
	} else if(header.is_set_chunk_size()) {
		rss_verbose("start to decode set chunk size message.");
		packet = new RssSetChunkSizePacket();
		return packet->decode(stream);
	} else {
		// default packet to drop message.
		rss_trace("drop the unknown message, type=%d", header.message_type);
		packet = new RssPacket();
	}
	
	return ret;
}

RssPacket* RssCommonMessage::get_packet()
{
	if (!packet) {
		rss_error("the payload is raw/undecoded, invoke decode_packet to decode it.");
	}
	rss_assert(packet != NULL);
	
	return packet;
}

int RssCommonMessage::get_perfer_cid()
{
	if (!packet) {
		return RTMP_CID_ProtocolControl;
	}
	
	// we donot use the complex basic header,
	// ensure the basic header is 1bytes.
	if (packet->get_perfer_cid() < 2) {
		return packet->get_perfer_cid();
	}
	
	return packet->get_perfer_cid();
}

void RssCommonMessage::set_packet(RssPacket* pkt, int stream_id)
{
	rss_freep(packet);

	packet = pkt;
	
	header.message_type = packet->get_message_type();
	header.payload_length = packet->get_payload_length();
	header.stream_id = stream_id;
}

int RssCommonMessage::encode_packet()
{
	int ret = ERROR_SUCCESS;
	
	if (packet == NULL) {
		rss_warn("packet is empty, send out empty message.");
		return ret;
	}
	// realloc the payload.
	size = 0;
	rss_freepa(payload);
	
	return packet->encode(size, (char*&)payload);
}

RssSharedPtrMessage::RssSharedPtr::RssSharedPtr()
{
	payload = NULL;
	size = 0;
	perfer_cid = 0;
	shared_count = 0;
}

RssSharedPtrMessage::RssSharedPtr::~RssSharedPtr()
{
	rss_freepa(payload);
}

RssSharedPtrMessage::RssSharedPtrMessage()
{
	ptr = NULL;
}

RssSharedPtrMessage::~RssSharedPtrMessage()
{
	if (ptr) {
		if (ptr->shared_count == 0) {
			rss_freep(ptr);
		} else {
			ptr->shared_count--;
		}
	}
}

bool RssSharedPtrMessage::can_decode()
{
	return false;
}

int RssSharedPtrMessage::initialize(IRssMessage* msg, char* payload, int size)
{
	int ret = ERROR_SUCCESS;
	
	rss_assert(msg != NULL);
	if (ptr) {
		ret = ERROR_SYSTEM_ASSERT_FAILED;
		rss_error("should not set the payload twice. ret=%d", ret);
		rss_assert(false);
		
		return ret;
	}
	
	header = msg->header;
	header.payload_length = size;
	
	ptr = new RssSharedPtr();
	ptr->payload = payload;
	ptr->size = size;
	
	if (msg->header.is_video()) {
		ptr->perfer_cid = RTMP_CID_Video;
	} else if (msg->header.is_audio()) {
		ptr->perfer_cid = RTMP_CID_Audio;
	} else {
		ptr->perfer_cid = RTMP_CID_OverConnection2;
	}
	
	super::payload = (int8_t*)ptr->payload;
	super::size = ptr->size;
	
	return ret;
}

RssSharedPtrMessage* RssSharedPtrMessage::copy()
{
	if (!ptr) {
		rss_error("invoke initialize to initialize the ptr.");
		rss_assert(false);
		return NULL;
	}
	
	RssSharedPtrMessage* copy = new RssSharedPtrMessage();
	
	copy->header = header;
	
	copy->ptr = ptr;
	ptr->shared_count++;
	
	copy->payload = (int8_t*)ptr->payload;
	copy->size = ptr->size;
	
	return copy;
}

int RssSharedPtrMessage::get_perfer_cid()
{
	if (!ptr) {
		return 0;
	}
	
	return ptr->perfer_cid;
}

int RssSharedPtrMessage::encode_packet()
{
	rss_verbose("shared message ignore the encode method.");
	return ERROR_SUCCESS;
}

RssPacket::RssPacket()
{
}

RssPacket::~RssPacket()
{
}

int RssPacket::decode(RssStream* stream)
{
	int ret = ERROR_SUCCESS;
	
	rss_assert(stream != NULL);

	ret = ERROR_SYSTEM_PACKET_INVALID;
	rss_error("current packet is not support to decode. "
		"paket=%s, ret=%d", get_class_name(), ret);
	
	return ret;
}

int RssPacket::get_perfer_cid()
{
	return 0;
}

int RssPacket::get_message_type()
{
	return 0;
}

int RssPacket::get_payload_length()
{
	return get_size();
}

int RssPacket::encode(int& psize, char*& ppayload)
{
	int ret = ERROR_SUCCESS;
	
	int size = get_size();
	char* payload = NULL;
	
	RssStream stream;
	
	if (size > 0) {
		payload = new char[size];
		
		if ((ret = stream.initialize(payload, size)) != ERROR_SUCCESS) {
			rss_error("initialize the stream failed. ret=%d", ret);
			rss_freepa(payload);
			return ret;
		}
	}
	
	if ((ret = encode_packet(&stream)) != ERROR_SUCCESS) {
		rss_error("encode the packet failed. ret=%d", ret);
		rss_freepa(payload);
		return ret;
	}
	
	psize = size;
	ppayload = payload;
	rss_verbose("encode the packet success. size=%d", size);
	
	return ret;
}

int RssPacket::get_size()
{
	return 0;
}

int RssPacket::encode_packet(RssStream* stream)
{
	int ret = ERROR_SUCCESS;
	
	rss_assert(stream != NULL);

	ret = ERROR_SYSTEM_PACKET_INVALID;
	rss_error("current packet is not support to encode. "
		"paket=%s, ret=%d", get_class_name(), ret);
	
	return ret;
}

RssConnectAppPacket::RssConnectAppPacket()
{
	command_name = RTMP_AMF0_COMMAND_CONNECT;
	transaction_id = 1;
	command_object = NULL;
}

RssConnectAppPacket::~RssConnectAppPacket()
{
	rss_freep(command_object);
}

int RssConnectAppPacket::decode(RssStream* stream)
{
	int ret = ERROR_SUCCESS;

	if ((ret = rss_amf0_read_string(stream, command_name)) != ERROR_SUCCESS) {
		rss_error("amf0 decode connect command_name failed. ret=%d", ret);
		return ret;
	}
	if (command_name.empty() || command_name != RTMP_AMF0_COMMAND_CONNECT) {
		ret = ERROR_RTMP_AMF0_DECODE;
		rss_error("amf0 decode connect command_name failed. "
			"command_name=%s, ret=%d", command_name.c_str(), ret);
		return ret;
	}
	
	if ((ret = rss_amf0_read_number(stream, transaction_id)) != ERROR_SUCCESS) {
		rss_error("amf0 decode connect transaction_id failed. ret=%d", ret);
		return ret;
	}
	if (transaction_id != 1.0) {
		ret = ERROR_RTMP_AMF0_DECODE;
		rss_error("amf0 decode connect transaction_id failed. "
			"required=%.1f, actual=%.1f, ret=%d", 1.0, transaction_id, ret);
		return ret;
	}
	
	if ((ret = rss_amf0_read_object(stream, command_object)) != ERROR_SUCCESS) {
		rss_error("amf0 decode connect command_object failed. ret=%d", ret);
		return ret;
	}
	if (command_object == NULL) {
		ret = ERROR_RTMP_AMF0_DECODE;
		rss_error("amf0 decode connect command_object failed. ret=%d", ret);
		return ret;
	}
	
	rss_info("amf0 decode connect packet success");
	
	return ret;
}

RssConnectAppResPacket::RssConnectAppResPacket()
{
	command_name = RTMP_AMF0_COMMAND_RESULT;
	transaction_id = 1;
	props = new RssAmf0Object();
	info = new RssAmf0Object();
}

RssConnectAppResPacket::~RssConnectAppResPacket()
{
	rss_freep(props);
	rss_freep(info);
}

int RssConnectAppResPacket::get_perfer_cid()
{
	return RTMP_CID_OverConnection;
}

int RssConnectAppResPacket::get_message_type()
{
	return RTMP_MSG_AMF0CommandMessage;
}

int RssConnectAppResPacket::get_size()
{
	return rss_amf0_get_string_size(command_name) + rss_amf0_get_number_size()
		+ rss_amf0_get_object_size(props)+ rss_amf0_get_object_size(info);
}

int RssConnectAppResPacket::encode_packet(RssStream* stream)
{
	int ret = ERROR_SUCCESS;
	
	if ((ret = rss_amf0_write_string(stream, command_name)) != ERROR_SUCCESS) {
		rss_error("encode command_name failed. ret=%d", ret);
		return ret;
	}
	rss_verbose("encode command_name success.");
	
	if ((ret = rss_amf0_write_number(stream, transaction_id)) != ERROR_SUCCESS) {
		rss_error("encode transaction_id failed. ret=%d", ret);
		return ret;
	}
	rss_verbose("encode transaction_id success.");
	
	if ((ret = rss_amf0_write_object(stream, props)) != ERROR_SUCCESS) {
		rss_error("encode props failed. ret=%d", ret);
		return ret;
	}
	rss_verbose("encode props success.");
	
	if ((ret = rss_amf0_write_object(stream, info)) != ERROR_SUCCESS) {
		rss_error("encode info failed. ret=%d", ret);
		return ret;
	}
	rss_verbose("encode info success.");
	
	rss_info("encode connect app response packet success.");
	
	return ret;
}

RssCreateStreamPacket::RssCreateStreamPacket()
{
	command_name = RTMP_AMF0_COMMAND_CREATE_STREAM;
	transaction_id = 2;
	command_object = new RssAmf0Null();
}

RssCreateStreamPacket::~RssCreateStreamPacket()
{
	rss_freep(command_object);
}

int RssCreateStreamPacket::decode(RssStream* stream)
{
	int ret = ERROR_SUCCESS;

	if ((ret = rss_amf0_read_string(stream, command_name)) != ERROR_SUCCESS) {
		rss_error("amf0 decode createStream command_name failed. ret=%d", ret);
		return ret;
	}
	if (command_name.empty() || command_name != RTMP_AMF0_COMMAND_CREATE_STREAM) {
		ret = ERROR_RTMP_AMF0_DECODE;
		rss_error("amf0 decode createStream command_name failed. "
			"command_name=%s, ret=%d", command_name.c_str(), ret);
		return ret;
	}
	
	if ((ret = rss_amf0_read_number(stream, transaction_id)) != ERROR_SUCCESS) {
		rss_error("amf0 decode createStream transaction_id failed. ret=%d", ret);
		return ret;
	}
	
	if ((ret = rss_amf0_read_null(stream)) != ERROR_SUCCESS) {
		rss_error("amf0 decode createStream command_object failed. ret=%d", ret);
		return ret;
	}
	
	rss_info("amf0 decode createStream packet success");
	
	return ret;
}

RssCreateStreamResPacket::RssCreateStreamResPacket(double _transaction_id, double _stream_id)
{
	command_name = RTMP_AMF0_COMMAND_RESULT;
	transaction_id = _transaction_id;
	command_object = new RssAmf0Null();
	stream_id = _stream_id;
}

RssCreateStreamResPacket::~RssCreateStreamResPacket()
{
	rss_freep(command_object);
}

int RssCreateStreamResPacket::get_perfer_cid()
{
	return RTMP_CID_OverConnection;
}

int RssCreateStreamResPacket::get_message_type()
{
	return RTMP_MSG_AMF0CommandMessage;
}

int RssCreateStreamResPacket::get_size()
{
	return rss_amf0_get_string_size(command_name) + rss_amf0_get_number_size()
		+ rss_amf0_get_null_size() + rss_amf0_get_number_size();
}

int RssCreateStreamResPacket::encode_packet(RssStream* stream)
{
	int ret = ERROR_SUCCESS;
	
	if ((ret = rss_amf0_write_string(stream, command_name)) != ERROR_SUCCESS) {
		rss_error("encode command_name failed. ret=%d", ret);
		return ret;
	}
	rss_verbose("encode command_name success.");
	
	if ((ret = rss_amf0_write_number(stream, transaction_id)) != ERROR_SUCCESS) {
		rss_error("encode transaction_id failed. ret=%d", ret);
		return ret;
	}
	rss_verbose("encode transaction_id success.");
	
	if ((ret = rss_amf0_write_null(stream)) != ERROR_SUCCESS) {
		rss_error("encode command_object failed. ret=%d", ret);
		return ret;
	}
	rss_verbose("encode command_object success.");
	
	if ((ret = rss_amf0_write_number(stream, stream_id)) != ERROR_SUCCESS) {
		rss_error("encode stream_id failed. ret=%d", ret);
		return ret;
	}
	rss_verbose("encode stream_id success.");
	
	
	rss_info("encode createStream response packet success.");
	
	return ret;
}

RssFMLEStartPacket::RssFMLEStartPacket()
{
	command_name = RTMP_AMF0_COMMAND_CREATE_STREAM;
	transaction_id = 0;
	command_object = new RssAmf0Null();
}

RssFMLEStartPacket::~RssFMLEStartPacket()
{
	rss_freep(command_object);
}

int RssFMLEStartPacket::decode(RssStream* stream)
{
	int ret = ERROR_SUCCESS;

	if ((ret = rss_amf0_read_string(stream, command_name)) != ERROR_SUCCESS) {
		rss_error("amf0 decode FMLE start command_name failed. ret=%d", ret);
		return ret;
	}
	if (command_name.empty() 
		|| (command_name != RTMP_AMF0_COMMAND_RELEASE_STREAM 
		&& command_name != RTMP_AMF0_COMMAND_FC_PUBLISH
		&& command_name != RTMP_AMF0_COMMAND_UNPUBLISH)
	) {
		ret = ERROR_RTMP_AMF0_DECODE;
		rss_error("amf0 decode FMLE start command_name failed. "
			"command_name=%s, ret=%d", command_name.c_str(), ret);
		return ret;
	}
	
	if ((ret = rss_amf0_read_number(stream, transaction_id)) != ERROR_SUCCESS) {
		rss_error("amf0 decode FMLE start transaction_id failed. ret=%d", ret);
		return ret;
	}
	
	if ((ret = rss_amf0_read_null(stream)) != ERROR_SUCCESS) {
		rss_error("amf0 decode FMLE start command_object failed. ret=%d", ret);
		return ret;
	}
	
	if ((ret = rss_amf0_read_string(stream, stream_name)) != ERROR_SUCCESS) {
		rss_error("amf0 decode FMLE start stream_name failed. ret=%d", ret);
		return ret;
	}
	
	rss_info("amf0 decode FMLE start packet success");
	
	return ret;
}

RssFMLEStartResPacket::RssFMLEStartResPacket(double _transaction_id)
{
	command_name = RTMP_AMF0_COMMAND_RESULT;
	transaction_id = _transaction_id;
	command_object = new RssAmf0Null();
	args = new RssAmf0Undefined();
}

RssFMLEStartResPacket::~RssFMLEStartResPacket()
{
	rss_freep(command_object);
	rss_freep(args);
}

int RssFMLEStartResPacket::get_perfer_cid()
{
	return RTMP_CID_OverConnection;
}

int RssFMLEStartResPacket::get_message_type()
{
	return RTMP_MSG_AMF0CommandMessage;
}

int RssFMLEStartResPacket::get_size()
{
	return rss_amf0_get_string_size(command_name) + rss_amf0_get_number_size()
		+ rss_amf0_get_null_size() + rss_amf0_get_undefined_size();
}

int RssFMLEStartResPacket::encode_packet(RssStream* stream)
{
	int ret = ERROR_SUCCESS;
	
	if ((ret = rss_amf0_write_string(stream, command_name)) != ERROR_SUCCESS) {
		rss_error("encode command_name failed. ret=%d", ret);
		return ret;
	}
	rss_verbose("encode command_name success.");
	
	if ((ret = rss_amf0_write_number(stream, transaction_id)) != ERROR_SUCCESS) {
		rss_error("encode transaction_id failed. ret=%d", ret);
		return ret;
	}
	rss_verbose("encode transaction_id success.");
	
	if ((ret = rss_amf0_write_null(stream)) != ERROR_SUCCESS) {
		rss_error("encode command_object failed. ret=%d", ret);
		return ret;
	}
	rss_verbose("encode command_object success.");
	
	if ((ret = rss_amf0_write_undefined(stream)) != ERROR_SUCCESS) {
		rss_error("encode args failed. ret=%d", ret);
		return ret;
	}
	rss_verbose("encode args success.");
	
	
	rss_info("encode FMLE start response packet success.");
	
	return ret;
}

RssPublishPacket::RssPublishPacket()
{
	command_name = RTMP_AMF0_COMMAND_PUBLISH;
	transaction_id = 0;
	command_object = new RssAmf0Null();
	type = "live";
}

RssPublishPacket::~RssPublishPacket()
{
	rss_freep(command_object);
}

int RssPublishPacket::decode(RssStream* stream)
{
	int ret = ERROR_SUCCESS;

	if ((ret = rss_amf0_read_string(stream, command_name)) != ERROR_SUCCESS) {
		rss_error("amf0 decode publish command_name failed. ret=%d", ret);
		return ret;
	}
	if (command_name.empty() || command_name != RTMP_AMF0_COMMAND_PUBLISH) {
		ret = ERROR_RTMP_AMF0_DECODE;
		rss_error("amf0 decode publish command_name failed. "
			"command_name=%s, ret=%d", command_name.c_str(), ret);
		return ret;
	}
	
	if ((ret = rss_amf0_read_number(stream, transaction_id)) != ERROR_SUCCESS) {
		rss_error("amf0 decode publish transaction_id failed. ret=%d", ret);
		return ret;
	}
	
	if ((ret = rss_amf0_read_null(stream)) != ERROR_SUCCESS) {
		rss_error("amf0 decode publish command_object failed. ret=%d", ret);
		return ret;
	}
	
	if ((ret = rss_amf0_read_string(stream, stream_name)) != ERROR_SUCCESS) {
		rss_error("amf0 decode publish stream_name failed. ret=%d", ret);
		return ret;
	}
	
	if ((ret = rss_amf0_read_string(stream, type)) != ERROR_SUCCESS) {
		rss_error("amf0 decode publish type failed. ret=%d", ret);
		return ret;
	}
	
	rss_info("amf0 decode publish packet success");
	
	return ret;
}

RssPlayPacket::RssPlayPacket()
{
	command_name = RTMP_AMF0_COMMAND_PLAY;
	transaction_id = 0;
	command_object = new RssAmf0Null();

	start = -2;
	duration = -1;
	reset = true;
}

RssPlayPacket::~RssPlayPacket()
{
	rss_freep(command_object);
}

int RssPlayPacket::decode(RssStream* stream)
{
	int ret = ERROR_SUCCESS;

	if ((ret = rss_amf0_read_string(stream, command_name)) != ERROR_SUCCESS) {
		rss_error("amf0 decode play command_name failed. ret=%d", ret);
		return ret;
	}
	if (command_name.empty() || command_name != RTMP_AMF0_COMMAND_PLAY) {
		ret = ERROR_RTMP_AMF0_DECODE;
		rss_error("amf0 decode play command_name failed. "
			"command_name=%s, ret=%d", command_name.c_str(), ret);
		return ret;
	}
	
	if ((ret = rss_amf0_read_number(stream, transaction_id)) != ERROR_SUCCESS) {
		rss_error("amf0 decode play transaction_id failed. ret=%d", ret);
		return ret;
	}
	
	if ((ret = rss_amf0_read_null(stream)) != ERROR_SUCCESS) {
		rss_error("amf0 decode play command_object failed. ret=%d", ret);
		return ret;
	}
	
	if ((ret = rss_amf0_read_string(stream, stream_name)) != ERROR_SUCCESS) {
		rss_error("amf0 decode play stream_name failed. ret=%d", ret);
		return ret;
	}
	
	if (!stream->empty() && (ret = rss_amf0_read_number(stream, start)) != ERROR_SUCCESS) {
		rss_error("amf0 decode play start failed. ret=%d", ret);
		return ret;
	}
	if (!stream->empty() && (ret = rss_amf0_read_number(stream, duration)) != ERROR_SUCCESS) {
		rss_error("amf0 decode play duration failed. ret=%d", ret);
		return ret;
	}
	if (!stream->empty() && (ret = rss_amf0_read_boolean(stream, reset)) != ERROR_SUCCESS) {
		rss_error("amf0 decode play reset failed. ret=%d", ret);
		return ret;
	}
	
	rss_info("amf0 decode play packet success");
	
	return ret;
}

RssPlayResPacket::RssPlayResPacket()
{
	command_name = RTMP_AMF0_COMMAND_RESULT;
	transaction_id = 0;
	command_object = new RssAmf0Null();
	desc = new RssAmf0Object();
}

RssPlayResPacket::~RssPlayResPacket()
{
	rss_freep(command_object);
	rss_freep(desc);
}

int RssPlayResPacket::get_perfer_cid()
{
	return RTMP_CID_OverStream;
}

int RssPlayResPacket::get_message_type()
{
	return RTMP_MSG_AMF0CommandMessage;
}

int RssPlayResPacket::get_size()
{
	return rss_amf0_get_string_size(command_name) + rss_amf0_get_number_size()
		+ rss_amf0_get_null_size() + rss_amf0_get_object_size(desc);
}

int RssPlayResPacket::encode_packet(RssStream* stream)
{
	int ret = ERROR_SUCCESS;
	
	if ((ret = rss_amf0_write_string(stream, command_name)) != ERROR_SUCCESS) {
		rss_error("encode command_name failed. ret=%d", ret);
		return ret;
	}
	rss_verbose("encode command_name success.");
	
	if ((ret = rss_amf0_write_number(stream, transaction_id)) != ERROR_SUCCESS) {
		rss_error("encode transaction_id failed. ret=%d", ret);
		return ret;
	}
	rss_verbose("encode transaction_id success.");
	
	if ((ret = rss_amf0_write_null(stream)) != ERROR_SUCCESS) {
		rss_error("encode command_object failed. ret=%d", ret);
		return ret;
	}
	rss_verbose("encode command_object success.");
	
	if ((ret = rss_amf0_write_object(stream, desc)) != ERROR_SUCCESS) {
		rss_error("encode desc failed. ret=%d", ret);
		return ret;
	}
	rss_verbose("encode desc success.");
	
	
	rss_info("encode play response packet success.");
	
	return ret;
}

RssOnBWDonePacket::RssOnBWDonePacket()
{
	command_name = RTMP_AMF0_COMMAND_ON_BW_DONE;
	transaction_id = 0;
	args = new RssAmf0Null();
}

RssOnBWDonePacket::~RssOnBWDonePacket()
{
	rss_freep(args);
}

int RssOnBWDonePacket::get_perfer_cid()
{
	return RTMP_CID_OverConnection;
}

int RssOnBWDonePacket::get_message_type()
{
	return RTMP_MSG_AMF0CommandMessage;
}

int RssOnBWDonePacket::get_size()
{
	return rss_amf0_get_string_size(command_name) + rss_amf0_get_number_size()
		+ rss_amf0_get_null_size();
}

int RssOnBWDonePacket::encode_packet(RssStream* stream)
{
	int ret = ERROR_SUCCESS;
	
	if ((ret = rss_amf0_write_string(stream, command_name)) != ERROR_SUCCESS) {
		rss_error("encode command_name failed. ret=%d", ret);
		return ret;
	}
	rss_verbose("encode command_name success.");
	
	if ((ret = rss_amf0_write_number(stream, transaction_id)) != ERROR_SUCCESS) {
		rss_error("encode transaction_id failed. ret=%d", ret);
		return ret;
	}
	rss_verbose("encode transaction_id success.");
	
	if ((ret = rss_amf0_write_null(stream)) != ERROR_SUCCESS) {
		rss_error("encode args failed. ret=%d", ret);
		return ret;
	}
	rss_verbose("encode args success.");
	
	rss_info("encode onBWDone packet success.");
	
	return ret;
}

RssOnStatusCallPacket::RssOnStatusCallPacket()
{
	command_name = RTMP_AMF0_COMMAND_ON_STATUS;
	transaction_id = 0;
	args = new RssAmf0Null();
	data = new RssAmf0Object();
}

RssOnStatusCallPacket::~RssOnStatusCallPacket()
{
	rss_freep(args);
	rss_freep(data);
}

int RssOnStatusCallPacket::get_perfer_cid()
{
	return RTMP_CID_OverStream;
}

int RssOnStatusCallPacket::get_message_type()
{
	return RTMP_MSG_AMF0CommandMessage;
}

int RssOnStatusCallPacket::get_size()
{
	return rss_amf0_get_string_size(command_name) + rss_amf0_get_number_size()
		+ rss_amf0_get_null_size() + rss_amf0_get_object_size(data);
}

int RssOnStatusCallPacket::encode_packet(RssStream* stream)
{
	int ret = ERROR_SUCCESS;
	
	if ((ret = rss_amf0_write_string(stream, command_name)) != ERROR_SUCCESS) {
		rss_error("encode command_name failed. ret=%d", ret);
		return ret;
	}
	rss_verbose("encode command_name success.");
	
	if ((ret = rss_amf0_write_number(stream, transaction_id)) != ERROR_SUCCESS) {
		rss_error("encode transaction_id failed. ret=%d", ret);
		return ret;
	}
	rss_verbose("encode transaction_id success.");
	
	if ((ret = rss_amf0_write_null(stream)) != ERROR_SUCCESS) {
		rss_error("encode args failed. ret=%d", ret);
		return ret;
	}
	rss_verbose("encode args success.");;
	
	if ((ret = rss_amf0_write_object(stream, data)) != ERROR_SUCCESS) {
		rss_error("encode data failed. ret=%d", ret);
		return ret;
	}
	rss_verbose("encode data success.");
	
	rss_info("encode onStatus(Call) packet success.");
	
	return ret;
}

RssOnStatusDataPacket::RssOnStatusDataPacket()
{
	command_name = RTMP_AMF0_COMMAND_ON_STATUS;
	data = new RssAmf0Object();
}

RssOnStatusDataPacket::~RssOnStatusDataPacket()
{
	rss_freep(data);
}

int RssOnStatusDataPacket::get_perfer_cid()
{
	return RTMP_CID_OverStream;
}

int RssOnStatusDataPacket::get_message_type()
{
	return RTMP_MSG_AMF0DataMessage;
}

int RssOnStatusDataPacket::get_size()
{
	return rss_amf0_get_string_size(command_name) + rss_amf0_get_object_size(data);
}

int RssOnStatusDataPacket::encode_packet(RssStream* stream)
{
	int ret = ERROR_SUCCESS;
	
	if ((ret = rss_amf0_write_string(stream, command_name)) != ERROR_SUCCESS) {
		rss_error("encode command_name failed. ret=%d", ret);
		return ret;
	}
	rss_verbose("encode command_name success.");
	
	if ((ret = rss_amf0_write_object(stream, data)) != ERROR_SUCCESS) {
		rss_error("encode data failed. ret=%d", ret);
		return ret;
	}
	rss_verbose("encode data success.");
	
	rss_info("encode onStatus(Data) packet success.");
	
	return ret;
}

RssSampleAccessPacket::RssSampleAccessPacket()
{
	command_name = RTMP_AMF0_DATA_SAMPLE_ACCESS;
	video_sample_access = false;
	audio_sample_access = false;
}

RssSampleAccessPacket::~RssSampleAccessPacket()
{
}

int RssSampleAccessPacket::get_perfer_cid()
{
	return RTMP_CID_OverStream;
}

int RssSampleAccessPacket::get_message_type()
{
	return RTMP_MSG_AMF0DataMessage;
}

int RssSampleAccessPacket::get_size()
{
	return rss_amf0_get_string_size(command_name)
		+ rss_amf0_get_boolean_size() + rss_amf0_get_boolean_size();
}

int RssSampleAccessPacket::encode_packet(RssStream* stream)
{
	int ret = ERROR_SUCCESS;
	
	if ((ret = rss_amf0_write_string(stream, command_name)) != ERROR_SUCCESS) {
		rss_error("encode command_name failed. ret=%d", ret);
		return ret;
	}
	rss_verbose("encode command_name success.");
	
	if ((ret = rss_amf0_write_boolean(stream, video_sample_access)) != ERROR_SUCCESS) {
		rss_error("encode video_sample_access failed. ret=%d", ret);
		return ret;
	}
	rss_verbose("encode video_sample_access success.");
	
	if ((ret = rss_amf0_write_boolean(stream, audio_sample_access)) != ERROR_SUCCESS) {
		rss_error("encode audio_sample_access failed. ret=%d", ret);
		return ret;
	}
	rss_verbose("encode audio_sample_access success.");;
	
	rss_info("encode |RtmpSampleAccess packet success.");
	
	return ret;
}

RssOnMetaDataPacket::RssOnMetaDataPacket()
{
	name = RTMP_AMF0_DATA_ON_METADATA;
	metadata = new RssAmf0Object();
}

RssOnMetaDataPacket::~RssOnMetaDataPacket()
{
	rss_freep(metadata);
}

int RssOnMetaDataPacket::decode(RssStream* stream)
{
	int ret = ERROR_SUCCESS;
	
	if ((ret = rss_amf0_read_string(stream, name)) != ERROR_SUCCESS) {
		rss_error("decode metadata name failed. ret=%d", ret);
		return ret;
	}

	// ignore the @setDataFrame
	if (name == RTMP_AMF0_DATA_SET_DATAFRAME) {
		if ((ret = rss_amf0_read_string(stream, name)) != ERROR_SUCCESS) {
			rss_error("decode metadata name failed. ret=%d", ret);
			return ret;
		}
	}
	
	rss_verbose("decode metadata name success. name=%s", name.c_str());
	
	// the metadata maybe object or ecma array
	RssAmf0Any* any = NULL;
	if ((ret = rss_amf0_read_any(stream, any)) != ERROR_SUCCESS) {
		rss_error("decode metadata metadata failed. ret=%d", ret);
		return ret;
	}
	
	if (any->is_object()) {
		rss_freep(metadata);
		metadata = rss_amf0_convert<RssAmf0Object>(any);
		rss_info("decode metadata object success");
		return ret;
	}
	
	RssARssAmf0EcmaArray* arr = dynamic_cast<RssARssAmf0EcmaArray*>(any);
	if (!arr) {
		ret = ERROR_RTMP_AMF0_DECODE;
		rss_error("decode metadata array failed. ret=%d", ret);
		rss_freep(any);
		return ret;
	}
	
	for (int i = 0; i < arr->size(); i++) {
		metadata->set(arr->key_at(i), arr->value_at(i));
	}
	arr->clear();
	rss_info("decode metadata array success");
	
	return ret;
}

int RssOnMetaDataPacket::get_perfer_cid()
{
	return RTMP_CID_OverConnection2;
}

int RssOnMetaDataPacket::get_message_type()
{
	return RTMP_MSG_AMF0DataMessage;
}

int RssOnMetaDataPacket::get_size()
{
	return rss_amf0_get_string_size(name) + rss_amf0_get_object_size(metadata);
}

int RssOnMetaDataPacket::encode_packet(RssStream* stream)
{
	int ret = ERROR_SUCCESS;
	
	if ((ret = rss_amf0_write_string(stream, name)) != ERROR_SUCCESS) {
		rss_error("encode name failed. ret=%d", ret);
		return ret;
	}
	rss_verbose("encode name success.");
	
	if ((ret = rss_amf0_write_object(stream, metadata)) != ERROR_SUCCESS) {
		rss_error("encode metadata failed. ret=%d", ret);
		return ret;
	}
	rss_verbose("encode metadata success.");
	
	rss_info("encode onMetaData packet success.");
	return ret;
}

RssSetWindowAckSizePacket::RssSetWindowAckSizePacket()
{
	ackowledgement_window_size = 0;
}

RssSetWindowAckSizePacket::~RssSetWindowAckSizePacket()
{
}

int RssSetWindowAckSizePacket::decode(RssStream* stream)
{
	int ret = ERROR_SUCCESS;
	
	if (!stream->require(4)) {
		ret = ERROR_RTMP_MESSAGE_DECODE;
		rss_error("decode ack window size failed. ret=%d", ret);
		return ret;
	}
	
	ackowledgement_window_size = stream->read_4bytes();
	rss_info("decode ack window size success");
	
	return ret;
}

int RssSetWindowAckSizePacket::get_perfer_cid()
{
	return RTMP_CID_ProtocolControl;
}

int RssSetWindowAckSizePacket::get_message_type()
{
	return RTMP_MSG_WindowAcknowledgementSize;
}

int RssSetWindowAckSizePacket::get_size()
{
	return 4;
}

int RssSetWindowAckSizePacket::encode_packet(RssStream* stream)
{
	int ret = ERROR_SUCCESS;
	
	if (!stream->require(4)) {
		ret = ERROR_RTMP_MESSAGE_ENCODE;
		rss_error("encode ack size packet failed. ret=%d", ret);
		return ret;
	}
	
	stream->write_4bytes(ackowledgement_window_size);
	
	rss_verbose("encode ack size packet "
		"success. ack_size=%d", ackowledgement_window_size);
	
	return ret;
}

RssSetChunkSizePacket::RssSetChunkSizePacket()
{
	chunk_size = RTMP_DEFAULT_CHUNK_SIZE;
}

RssSetChunkSizePacket::~RssSetChunkSizePacket()
{
}

int RssSetChunkSizePacket::decode(RssStream* stream)
{
	int ret = ERROR_SUCCESS;
	
	if (!stream->require(4)) {
		ret = ERROR_RTMP_MESSAGE_DECODE;
		rss_error("decode chunk size failed. ret=%d", ret);
		return ret;
	}
	
	chunk_size = stream->read_4bytes();
	rss_info("decode chunk size success. chunk_size=%d", chunk_size);
	
	if (chunk_size < RTMP_MIN_CHUNK_SIZE) {
		ret = ERROR_RTMP_CHUNK_SIZE;
		rss_error("invalid chunk size. min=%d, actual=%d, ret=%d", 
			ERROR_RTMP_CHUNK_SIZE, chunk_size, ret);
		return ret;
	}
	
	return ret;
}

int RssSetChunkSizePacket::get_perfer_cid()
{
	return RTMP_CID_ProtocolControl;
}

int RssSetChunkSizePacket::get_message_type()
{
	return RTMP_MSG_SetChunkSize;
}

int RssSetChunkSizePacket::get_size()
{
	return 4;
}

int RssSetChunkSizePacket::encode_packet(RssStream* stream)
{
	int ret = ERROR_SUCCESS;
	
	if (!stream->require(4)) {
		ret = ERROR_RTMP_MESSAGE_ENCODE;
		rss_error("encode chunk packet failed. ret=%d", ret);
		return ret;
	}
	
	stream->write_4bytes(chunk_size);
	
	rss_verbose("encode chunk packet success. ack_size=%d", chunk_size);
	
	return ret;
}

RssSetPeerBandwidthPacket::RssSetPeerBandwidthPacket()
{
	bandwidth = 0;
	type = 2;
}

RssSetPeerBandwidthPacket::~RssSetPeerBandwidthPacket()
{
}

int RssSetPeerBandwidthPacket::get_perfer_cid()
{
	return RTMP_CID_ProtocolControl;
}

int RssSetPeerBandwidthPacket::get_message_type()
{
	return RTMP_MSG_SetPeerBandwidth;
}

int RssSetPeerBandwidthPacket::get_size()
{
	return 5;
}

int RssSetPeerBandwidthPacket::encode_packet(RssStream* stream)
{
	int ret = ERROR_SUCCESS;
	
	if (!stream->require(5)) {
		ret = ERROR_RTMP_MESSAGE_ENCODE;
		rss_error("encode set bandwidth packet failed. ret=%d", ret);
		return ret;
	}
	
	stream->write_4bytes(bandwidth);
	stream->write_1bytes(type);
	
	rss_verbose("encode set bandwidth packet "
		"success. bandwidth=%d, type=%d", bandwidth, type);
	
	return ret;
}

RssPCUC4BytesPacket::RssPCUC4BytesPacket()
{
	event_type = 0;
	event_data = 0;
}

RssPCUC4BytesPacket::~RssPCUC4BytesPacket()
{
}

int RssPCUC4BytesPacket::get_perfer_cid()
{
	return RTMP_CID_ProtocolControl;
}

int RssPCUC4BytesPacket::get_message_type()
{
	return RTMP_MSG_UserControlMessage;
}

int RssPCUC4BytesPacket::get_size()
{
	return 2 + 4;
}

int RssPCUC4BytesPacket::encode_packet(RssStream* stream)
{
	int ret = ERROR_SUCCESS;
	
	if (!stream->require(6)) {
		ret = ERROR_RTMP_MESSAGE_ENCODE;
		rss_error("encode set bandwidth packet failed. ret=%d", ret);
		return ret;
	}
	
	stream->write_2bytes(event_type);
	stream->write_4bytes(event_data);
	
	rss_verbose("encode PCUC packet success. "
		"event_type=%d, event_data=%d", event_type, event_data);
	
	return ret;
}

