#include <rss_core_amf0.hpp>

#include <utility>

#include <rss_core_log.hpp>
#include <rss_core_error.hpp>
#include <rss_core_stream.hpp>

// AMF0 marker
#define RTMP_AMF0_Number 					0x00
#define RTMP_AMF0_Boolean 					0x01
#define RTMP_AMF0_String 					0x02
#define RTMP_AMF0_Object 					0x03
#define RTMP_AMF0_MovieClip 				0x04 // reserved, not supported
#define RTMP_AMF0_Null 						0x05
#define RTMP_AMF0_Undefined 				0x06
#define RTMP_AMF0_Reference 				0x07
#define RTMP_AMF0_EcmaArray 				0x08
#define RTMP_AMF0_ObjectEnd 				0x09
#define RTMP_AMF0_StrictArray 				0x0A
#define RTMP_AMF0_Date 						0x0B
#define RTMP_AMF0_LongString 				0x0C
#define RTMP_AMF0_UnSupported 				0x0D
#define RTMP_AMF0_RecordSet 				0x0E // reserved, not supported
#define RTMP_AMF0_XmlDocument 				0x0F
#define RTMP_AMF0_TypedObject 				0x10
// AVM+ object is the AMF3 object.
#define RTMP_AMF0_AVMplusObject 			0x11
// origin array whos data takes the same form as LengthValueBytes
#define RTMP_AMF0_OriginStrictArray 		0x20

// User defined
#define RTMP_AMF0_Invalid 					0x3F

int rss_amf0_get_object_eof_size();
int rss_amf0_get_any_size(RssAmf0Any* value);
int rss_amf0_read_object_eof(RssStream* stream, RssAmf0ObjectEOF*&);
int rss_amf0_write_object_eof(RssStream* stream, RssAmf0ObjectEOF*);
int rss_amf0_read_any(RssStream* stream, RssAmf0Any*& value);
int rss_amf0_write_any(RssStream* stream, RssAmf0Any* value);

RssAmf0Any::RssAmf0Any()
{
	marker = RTMP_AMF0_Invalid;
}

RssAmf0Any::~RssAmf0Any()
{
}

bool RssAmf0Any::is_string()
{
	return marker == RTMP_AMF0_String;
}

bool RssAmf0Any::is_boolean()
{
	return marker == RTMP_AMF0_Boolean;
}

bool RssAmf0Any::is_number()
{
	return marker == RTMP_AMF0_Number;
}

bool RssAmf0Any::is_null()
{
	return marker == RTMP_AMF0_Null;
}

bool RssAmf0Any::is_undefined()
{
	return marker == RTMP_AMF0_Undefined;
}

bool RssAmf0Any::is_object()
{
	return marker == RTMP_AMF0_Object;
}

bool RssAmf0Any::is_ecma_array()
{
	return marker == RTMP_AMF0_EcmaArray;
}

bool RssAmf0Any::is_object_eof()
{
	return marker == RTMP_AMF0_ObjectEnd;
}

RssAmf0String::RssAmf0String(const char* _value)
{
	marker = RTMP_AMF0_String;
	if (_value)
	{
		value = _value;
	}
}

RssAmf0String::~RssAmf0String()
{
}

RssAmf0Boolean::RssAmf0Boolean(bool _value)
{
	marker = RTMP_AMF0_Boolean;
	value = _value;
}

RssAmf0Boolean::~RssAmf0Boolean()
{
}

RssAmf0Number::RssAmf0Number(double _value)
{
	marker = RTMP_AMF0_Number;
	value = _value;
}

RssAmf0Number::~RssAmf0Number()
{
}

RssAmf0Null::RssAmf0Null()
{
	marker = RTMP_AMF0_Null;
}

RssAmf0Null::~RssAmf0Null()
{
}

RssAmf0Undefined::RssAmf0Undefined()
{
	marker = RTMP_AMF0_Undefined;
}

RssAmf0Undefined::~RssAmf0Undefined()
{
}

RssAmf0ObjectEOF::RssAmf0ObjectEOF()
{
	marker = RTMP_AMF0_ObjectEnd;
	utf8_empty = 0x00;
}

RssAmf0ObjectEOF::~RssAmf0ObjectEOF()
{
}

RssUnSortedHashtable::RssUnSortedHashtable()
{
}

RssUnSortedHashtable::~RssUnSortedHashtable()
{
	std::vector<RssObjectPropertyType>::iterator it;
	for (it = properties.begin(); it != properties.end(); ++it)
	{
		RssObjectPropertyType& elem = *it;
		RssAmf0Any* any = elem.second;
		rss_freep(any);
	}
	properties.clear();
}

int RssUnSortedHashtable::size()
{
	return (int)properties.size();
}

void RssUnSortedHashtable::clear()
{
	properties.clear();
}

std::string RssUnSortedHashtable::key_at(int index)
{
	rss_assert(index < size());
	RssObjectPropertyType& elem = properties[index];
	return elem.first;
}

RssAmf0Any* RssUnSortedHashtable::value_at(int index)
{
	rss_assert(index < size());
	RssObjectPropertyType& elem = properties[index];
	return elem.second;
}

void RssUnSortedHashtable::set(std::string key, RssAmf0Any* value)
{
	std::vector<RssObjectPropertyType>::iterator it;

	for (it = properties.begin(); it != properties.end(); ++it)
	{
		RssObjectPropertyType& elem = *it;
		std::string name = elem.first;
		RssAmf0Any* any = elem.second;

		if (key == name)
		{
			rss_freep(any);
			properties.erase(it);
			break;
		}
	}

	properties.push_back(std::make_pair(key, value));
}

RssAmf0Any* RssUnSortedHashtable::get_property(std::string name)
{
	std::vector<RssObjectPropertyType>::iterator it;

	for (it = properties.begin(); it != properties.end(); ++it)
	{
		RssObjectPropertyType& elem = *it;
		std::string key = elem.first;
		RssAmf0Any* any = elem.second;
		if (key == name)
		{
			return any;
		}
	}

	return NULL;
}

RssAmf0Any* RssUnSortedHashtable::ensure_property_string(std::string name)
{
	RssAmf0Any* prop = get_property(name);

	if (!prop)
	{
		return NULL;
	}

	if (!prop->is_string())
	{
		return NULL;
	}

	return prop;
}

RssAmf0Any* RssUnSortedHashtable::ensure_property_number(std::string name)
{
	RssAmf0Any* prop = get_property(name);

	if (!prop)
	{
		return NULL;
	}

	if (!prop->is_number())
	{
		return NULL;
	}

	return prop;
}

RssAmf0Object::RssAmf0Object()
{
	marker = RTMP_AMF0_Object;
}

RssAmf0Object::~RssAmf0Object()
{
}

int RssAmf0Object::size()
{
	return properties.size();
}

std::string RssAmf0Object::key_at(int index)
{
	return properties.key_at(index);
}

RssAmf0Any* RssAmf0Object::value_at(int index)
{
	return properties.value_at(index);
}

void RssAmf0Object::set(std::string key, RssAmf0Any* value)
{
	properties.set(key, value);
}

RssAmf0Any* RssAmf0Object::get_property(std::string name)
{
	return properties.get_property(name);
}

RssAmf0Any* RssAmf0Object::ensure_property_string(std::string name)
{
	return properties.ensure_property_string(name);
}

RssAmf0Any* RssAmf0Object::ensure_property_number(std::string name)
{
	return properties.ensure_property_number(name);
}

RssARssAmf0EcmaArray::RssARssAmf0EcmaArray()
{
	marker = RTMP_AMF0_EcmaArray;
}

RssARssAmf0EcmaArray::~RssARssAmf0EcmaArray()
{
}

int RssARssAmf0EcmaArray::size()
{
	return properties.size();
}

void RssARssAmf0EcmaArray::clear()
{
	properties.clear();
}

std::string RssARssAmf0EcmaArray::key_at(int index)
{
	return properties.key_at(index);
}

RssAmf0Any* RssARssAmf0EcmaArray::value_at(int index)
{
	return properties.value_at(index);
}

void RssARssAmf0EcmaArray::set(std::string key, RssAmf0Any* value)
{
	properties.set(key, value);
}

RssAmf0Any* RssARssAmf0EcmaArray::get_property(std::string name)
{
	return properties.get_property(name);
}

RssAmf0Any* RssARssAmf0EcmaArray::ensure_property_string(std::string name)
{
	return properties.ensure_property_string(name);
}

int rss_amf0_read_utf8(RssStream* stream, std::string& value)
{
	int ret = ERROR_SUCCESS;

	// len
	if (!stream->require(2))
	{
		ret = ERROR_RTMP_AMF0_DECODE;
		rss_error("amf0 read string length failed. ret=%d", ret);
		return ret;
	}
	int16_t len = stream->read_2bytes();
	rss_verbose("amf0 read string length success. len=%d", len);

	// empty string
	if (len <= 0)
	{
		rss_verbose("amf0 read empty string. ret=%d", ret);
		return ret;
	}

	// data
	if (!stream->require(len))
	{
		ret = ERROR_RTMP_AMF0_DECODE;
		rss_error("amf0 read string data failed. ret=%d", ret);
		return ret;
	}
	std::string str = stream->read_string(len);

	// support utf8-1 only
	// 1.3.1 Strings and UTF-8
	// UTF8-1 = %x00-7F
	for (int i = 0; i < len; i++)
	{
		char ch = *(str.data() + i);
		if ((ch & 0x80) != 0)
		{
			ret = ERROR_RTMP_AMF0_DECODE;
			rss_error("ignored. only support utf8-1, 0x00-0x7F, actual is %#x. ret=%d", (int)ch, ret);
			ret = ERROR_SUCCESS;
		}
	}

	value = str;
	rss_verbose("amf0 read string data success. str=%s", str.c_str());

	return ret;
}
int rss_amf0_write_utf8(RssStream* stream, std::string value)
{
	int ret = ERROR_SUCCESS;

	// len
	if (!stream->require(2))
	{
		ret = ERROR_RTMP_AMF0_ENCODE;
		rss_error("amf0 write string length failed. ret=%d", ret);
		return ret;
	}
	stream->write_2bytes(value.length());
	rss_verbose("amf0 write string length success. len=%d", (int)value.length());

	// empty string
	if (value.length() <= 0)
	{
		rss_verbose("amf0 write empty string. ret=%d", ret);
		return ret;
	}

	// data
	if (!stream->require(value.length()))
	{
		ret = ERROR_RTMP_AMF0_ENCODE;
		rss_error("amf0 write string data failed. ret=%d", ret);
		return ret;
	}
	stream->write_string(value);
	rss_verbose("amf0 write string data success. str=%s", value.c_str());

	return ret;
}

int rss_amf0_read_string(RssStream* stream, std::string& value)
{
	int ret = ERROR_SUCCESS;

	// marker
	if (!stream->require(1))
	{
		ret = ERROR_RTMP_AMF0_DECODE;
		rss_error("amf0 read string marker failed. ret=%d", ret);
		return ret;
	}

	char marker = stream->read_1bytes();
	if (marker != RTMP_AMF0_String)
	{
		ret = ERROR_RTMP_AMF0_DECODE;
		rss_error("amf0 check string marker failed. "
		          "marker=%#x, required=%#x, ret=%d", marker, RTMP_AMF0_String, ret);
		return ret;
	}
	rss_verbose("amf0 read string marker success");

	return rss_amf0_read_utf8(stream, value);
}

int rss_amf0_write_string(RssStream* stream, std::string value)
{
	int ret = ERROR_SUCCESS;

	// marker
	if (!stream->require(1))
	{
		ret = ERROR_RTMP_AMF0_ENCODE;
		rss_error("amf0 write string marker failed. ret=%d", ret);
		return ret;
	}

	stream->write_1bytes(RTMP_AMF0_String);
	rss_verbose("amf0 write string marker success");

	return rss_amf0_write_utf8(stream, value);
}

int rss_amf0_read_boolean(RssStream* stream, bool& value)
{
	int ret = ERROR_SUCCESS;

	// marker
	if (!stream->require(1))
	{
		ret = ERROR_RTMP_AMF0_DECODE;
		rss_error("amf0 read bool marker failed. ret=%d", ret);
		return ret;
	}

	char marker = stream->read_1bytes();
	if (marker != RTMP_AMF0_Boolean)
	{
		ret = ERROR_RTMP_AMF0_DECODE;
		rss_error("amf0 check bool marker failed. "
		          "marker=%#x, required=%#x, ret=%d", marker, RTMP_AMF0_Boolean, ret);
		return ret;
	}
	rss_verbose("amf0 read bool marker success");

	// value
	if (!stream->require(1))
	{
		ret = ERROR_RTMP_AMF0_DECODE;
		rss_error("amf0 read bool value failed. ret=%d", ret);
		return ret;
	}

	if (stream->read_1bytes() == 0)
	{
		value = false;
	}
	else
	{
		value = true;
	}

	rss_verbose("amf0 read bool value success. value=%d", value);

	return ret;
}
int rss_amf0_write_boolean(RssStream* stream, bool value)
{
	int ret = ERROR_SUCCESS;

	// marker
	if (!stream->require(1))
	{
		ret = ERROR_RTMP_AMF0_ENCODE;
		rss_error("amf0 write bool marker failed. ret=%d", ret);
		return ret;
	}
	stream->write_1bytes(RTMP_AMF0_Boolean);
	rss_verbose("amf0 write bool marker success");

	// value
	if (!stream->require(1))
	{
		ret = ERROR_RTMP_AMF0_ENCODE;
		rss_error("amf0 write bool value failed. ret=%d", ret);
		return ret;
	}

	if (value)
	{
		stream->write_1bytes(0x01);
	}
	else
	{
		stream->write_1bytes(0x00);
	}

	rss_verbose("amf0 write bool value success. value=%d", value);

	return ret;
}

int rss_amf0_read_number(RssStream* stream, double& value)
{
	int ret = ERROR_SUCCESS;

	// marker
	if (!stream->require(1))
	{
		ret = ERROR_RTMP_AMF0_DECODE;
		rss_error("amf0 read number marker failed. ret=%d", ret);
		return ret;
	}

	char marker = stream->read_1bytes();
	if (marker != RTMP_AMF0_Number)
	{
		ret = ERROR_RTMP_AMF0_DECODE;
		rss_error("amf0 check number marker failed. "
		          "marker=%#x, required=%#x, ret=%d", marker, RTMP_AMF0_Number, ret);
		return ret;
	}
	rss_verbose("amf0 read number marker success");

	// value
	if (!stream->require(8))
	{
		ret = ERROR_RTMP_AMF0_DECODE;
		rss_error("amf0 read number value failed. ret=%d", ret);
		return ret;
	}

	int64_t temp = stream->read_8bytes();
	memcpy(&value, &temp, 8);

	rss_verbose("amf0 read number value success. value=%.2f", value);

	return ret;
}
int rss_amf0_write_number(RssStream* stream, double value)
{
	int ret = ERROR_SUCCESS;

	// marker
	if (!stream->require(1))
	{
		ret = ERROR_RTMP_AMF0_ENCODE;
		rss_error("amf0 write number marker failed. ret=%d", ret);
		return ret;
	}

	stream->write_1bytes(RTMP_AMF0_Number);
	rss_verbose("amf0 write number marker success");

	// value
	if (!stream->require(8))
	{
		ret = ERROR_RTMP_AMF0_ENCODE;
		rss_error("amf0 write number value failed. ret=%d", ret);
		return ret;
	}

	int64_t temp = 0x00;
	memcpy(&temp, &value, 8);
	stream->write_8bytes(temp);

	rss_verbose("amf0 write number value success. value=%.2f", value);

	return ret;
}

int rss_amf0_read_null(RssStream* stream)
{
	int ret = ERROR_SUCCESS;

	// marker
	if (!stream->require(1))
	{
		ret = ERROR_RTMP_AMF0_DECODE;
		rss_error("amf0 read null marker failed. ret=%d", ret);
		return ret;
	}

	char marker = stream->read_1bytes();
	if (marker != RTMP_AMF0_Null)
	{
		ret = ERROR_RTMP_AMF0_DECODE;
		rss_error("amf0 check null marker failed. "
		          "marker=%#x, required=%#x, ret=%d", marker, RTMP_AMF0_Null, ret);
		return ret;
	}
	rss_verbose("amf0 read null success");

	return ret;
}
int rss_amf0_write_null(RssStream* stream)
{
	int ret = ERROR_SUCCESS;

	// marker
	if (!stream->require(1))
	{
		ret = ERROR_RTMP_AMF0_ENCODE;
		rss_error("amf0 write null marker failed. ret=%d", ret);
		return ret;
	}

	stream->write_1bytes(RTMP_AMF0_Null);
	rss_verbose("amf0 write null marker success");

	return ret;
}

int rss_amf0_read_undefined(RssStream* stream)
{
	int ret = ERROR_SUCCESS;

	// marker
	if (!stream->require(1))
	{
		ret = ERROR_RTMP_AMF0_DECODE;
		rss_error("amf0 read undefined marker failed. ret=%d", ret);
		return ret;
	}

	char marker = stream->read_1bytes();
	if (marker != RTMP_AMF0_Undefined)
	{
		ret = ERROR_RTMP_AMF0_DECODE;
		rss_error("amf0 check undefined marker failed. "
		          "marker=%#x, required=%#x, ret=%d", marker, RTMP_AMF0_Undefined, ret);
		return ret;
	}
	rss_verbose("amf0 read undefined success");

	return ret;
}
int rss_amf0_write_undefined(RssStream* stream)
{
	int ret = ERROR_SUCCESS;

	// marker
	if (!stream->require(1))
	{
		ret = ERROR_RTMP_AMF0_ENCODE;
		rss_error("amf0 write undefined marker failed. ret=%d", ret);
		return ret;
	}

	stream->write_1bytes(RTMP_AMF0_Undefined);
	rss_verbose("amf0 write undefined marker success");

	return ret;
}

int rss_amf0_read_any(RssStream* stream, RssAmf0Any*& value)
{
	int ret = ERROR_SUCCESS;

	// marker
	if (!stream->require(1))
	{
		ret = ERROR_RTMP_AMF0_DECODE;
		rss_error("amf0 read any marker failed. ret=%d", ret);
		return ret;
	}

	char marker = stream->read_1bytes();
	rss_verbose("amf0 any marker success");

	// backward the 1byte marker.
	stream->skip(-1);

	switch (marker)
	{
	case RTMP_AMF0_String:
	{
		std::string data;
		if ((ret = rss_amf0_read_string(stream, data)) != ERROR_SUCCESS)
		{
			return ret;
		}
		value = new RssAmf0String();
		rss_amf0_convert<RssAmf0String>(value)->value = data;
		return ret;
	}
	case RTMP_AMF0_Boolean:
	{
		bool data;
		if ((ret = rss_amf0_read_boolean(stream, data)) != ERROR_SUCCESS)
		{
			return ret;
		}
		value = new RssAmf0Boolean();
		rss_amf0_convert<RssAmf0Boolean>(value)->value = data;
		return ret;
	}
	case RTMP_AMF0_Number:
	{
		double data;
		if ((ret = rss_amf0_read_number(stream, data)) != ERROR_SUCCESS)
		{
			return ret;
		}
		value = new RssAmf0Number();
		rss_amf0_convert<RssAmf0Number>(value)->value = data;
		return ret;
	}
	case RTMP_AMF0_Null:
	{
		value = new RssAmf0Null();
		return ret;
	}
	case RTMP_AMF0_Undefined:
	{
		value = new RssAmf0Undefined();
		return ret;
	}
	case RTMP_AMF0_ObjectEnd:
	{
		RssAmf0ObjectEOF* p = NULL;
		if ((ret = rss_amf0_read_object_eof(stream, p)) != ERROR_SUCCESS)
		{
			return ret;
		}
		value = p;
		return ret;
	}
	case RTMP_AMF0_Object:
	{
		RssAmf0Object* p = NULL;
		if ((ret = rss_amf0_read_object(stream, p)) != ERROR_SUCCESS)
		{
			return ret;
		}
		value = p;
		return ret;
	}
	case RTMP_AMF0_EcmaArray:
	{
		RssARssAmf0EcmaArray* p = NULL;
		if ((ret = rss_amf0_read_ecma_array(stream, p)) != ERROR_SUCCESS)
		{
			return ret;
		}
		value = p;
		return ret;
	}
	case RTMP_AMF0_Invalid:
	default:
	{
		ret = ERROR_RTMP_AMF0_INVALID;
		rss_error("invalid amf0 message type. marker=%#x, ret=%d", marker, ret);
		return ret;
	}
	}

	return ret;
}
int rss_amf0_write_any(RssStream* stream, RssAmf0Any* value)
{
	int ret = ERROR_SUCCESS;

	rss_assert(value != NULL);

	switch (value->marker)
	{
	case RTMP_AMF0_String:
	{
		std::string data = rss_amf0_convert<RssAmf0String>(value)->value;
		return rss_amf0_write_string(stream, data);
	}
	case RTMP_AMF0_Boolean:
	{
		bool data = rss_amf0_convert<RssAmf0Boolean>(value)->value;
		return rss_amf0_write_boolean(stream, data);
	}
	case RTMP_AMF0_Number:
	{
		double data = rss_amf0_convert<RssAmf0Number>(value)->value;
		return rss_amf0_write_number(stream, data);
	}
	case RTMP_AMF0_Null:
	{
		return rss_amf0_write_null(stream);
	}
	case RTMP_AMF0_Undefined:
	{
		return rss_amf0_write_undefined(stream);
	}
	case RTMP_AMF0_ObjectEnd:
	{
		RssAmf0ObjectEOF* p = rss_amf0_convert<RssAmf0ObjectEOF>(value);
		return rss_amf0_write_object_eof(stream, p);
	}
	case RTMP_AMF0_Object:
	{
		RssAmf0Object* p = rss_amf0_convert<RssAmf0Object>(value);
		return rss_amf0_write_object(stream, p);
	}
	case RTMP_AMF0_EcmaArray:
	{
		RssARssAmf0EcmaArray* p = rss_amf0_convert<RssARssAmf0EcmaArray>(value);
		return rss_amf0_write_ecma_array(stream, p);
	}
	case RTMP_AMF0_Invalid:
	default:
	{
		ret = ERROR_RTMP_AMF0_INVALID;
		rss_error("invalid amf0 message type. marker=%#x, ret=%d", value->marker, ret);
		return ret;
	}
	}

	return ret;
}
int rss_amf0_get_any_size(RssAmf0Any* value)
{
	if (!value)
	{
		return 0;
	}

	int size = 0;

	switch (value->marker)
	{
	case RTMP_AMF0_String:
	{
		RssAmf0String* p = rss_amf0_convert<RssAmf0String>(value);
		size += rss_amf0_get_string_size(p->value);
		break;
	}
	case RTMP_AMF0_Boolean:
	{
		size += rss_amf0_get_boolean_size();
		break;
	}
	case RTMP_AMF0_Number:
	{
		size += rss_amf0_get_number_size();
		break;
	}
	case RTMP_AMF0_Null:
	{
		size += rss_amf0_get_null_size();
		break;
	}
	case RTMP_AMF0_Undefined:
	{
		size += rss_amf0_get_undefined_size();
		break;
	}
	case RTMP_AMF0_ObjectEnd:
	{
		size += rss_amf0_get_object_eof_size();
		break;
	}
	case RTMP_AMF0_Object:
	{
		RssAmf0Object* p = rss_amf0_convert<RssAmf0Object>(value);
		size += rss_amf0_get_object_size(p);
		break;
	}
	case RTMP_AMF0_EcmaArray:
	{
		RssARssAmf0EcmaArray* p = rss_amf0_convert<RssARssAmf0EcmaArray>(value);
		size += rss_amf0_get_ecma_array_size(p);
		break;
	}
	default:
	{
		// TOOD: other AMF0 types.
		rss_warn("ignore unkown AMF0 type size.");
		break;
	}
	}

	return size;
}

int rss_amf0_read_object_eof(RssStream* stream, RssAmf0ObjectEOF*& value)
{
	int ret = ERROR_SUCCESS;

	// auto skip -2 to read the object eof.
	rss_assert(stream->pos() >= 2);
	stream->skip(-2);

	// value
	if (!stream->require(2))
	{
		ret = ERROR_RTMP_AMF0_DECODE;
		rss_error("amf0 read object eof value failed. ret=%d", ret);
		return ret;
	}
	int16_t temp = stream->read_2bytes();
	if (temp != 0x00)
	{
		ret = ERROR_RTMP_AMF0_DECODE;
		rss_error("amf0 read object eof value check failed. "
		          "must be 0x00, actual is %#x, ret=%d", temp, ret);
		return ret;
	}

	// marker
	if (!stream->require(1))
	{
		ret = ERROR_RTMP_AMF0_DECODE;
		rss_error("amf0 read object eof marker failed. ret=%d", ret);
		return ret;
	}

	char marker = stream->read_1bytes();
	if (marker != RTMP_AMF0_ObjectEnd)
	{
		ret = ERROR_RTMP_AMF0_DECODE;
		rss_error("amf0 check object eof marker failed. "
		          "marker=%#x, required=%#x, ret=%d", marker, RTMP_AMF0_ObjectEnd, ret);
		return ret;
	}
	rss_verbose("amf0 read object eof marker success");

	value = new RssAmf0ObjectEOF();
	rss_verbose("amf0 read object eof success");

	return ret;
}
int rss_amf0_write_object_eof(RssStream* stream, RssAmf0ObjectEOF* value)
{
	int ret = ERROR_SUCCESS;

	rss_assert(value != NULL);

	// value
	if (!stream->require(2))
	{
		ret = ERROR_RTMP_AMF0_ENCODE;
		rss_error("amf0 write object eof value failed. ret=%d", ret);
		return ret;
	}
	stream->write_2bytes(0x00);
	rss_verbose("amf0 write object eof value success");

	// marker
	if (!stream->require(1))
	{
		ret = ERROR_RTMP_AMF0_ENCODE;
		rss_error("amf0 write object eof marker failed. ret=%d", ret);
		return ret;
	}

	stream->write_1bytes(RTMP_AMF0_ObjectEnd);

	rss_verbose("amf0 read object eof success");

	return ret;
}

int rss_amf0_read_object(RssStream* stream, RssAmf0Object*& value)
{
	int ret = ERROR_SUCCESS;

	// marker
	if (!stream->require(1))
	{
		ret = ERROR_RTMP_AMF0_DECODE;
		rss_error("amf0 read object marker failed. ret=%d", ret);
		return ret;
	}

	char marker = stream->read_1bytes();
	if (marker != RTMP_AMF0_Object)
	{
		ret = ERROR_RTMP_AMF0_DECODE;
		rss_error("amf0 check object marker failed. "
		          "marker=%#x, required=%#x, ret=%d", marker, RTMP_AMF0_Object, ret);
		return ret;
	}
	rss_verbose("amf0 read object marker success");

	// value
	value = new RssAmf0Object();

	while (!stream->empty())
	{
		// property-name: utf8 string
		std::string property_name;
		if ((ret =rss_amf0_read_utf8(stream, property_name)) != ERROR_SUCCESS)
		{
			rss_error("amf0 object read property name failed. ret=%d", ret);
			return ret;
		}
		// property-value: any
		RssAmf0Any* property_value = NULL;
		if ((ret = rss_amf0_read_any(stream, property_value)) != ERROR_SUCCESS)
		{
			rss_error("amf0 object read property_value failed. "
			          "name=%s, ret=%d", property_name.c_str(), ret);
			return ret;
		}

		// AMF0 Object EOF.
		if (property_name.empty() || !property_value || property_value->is_object_eof())
		{
			if (property_value)
			{
				rss_freep(property_value);
			}
			rss_info("amf0 read object EOF.");
			break;
		}

		// add property
		value->set(property_name, property_value);
	}

	return ret;
}
int rss_amf0_write_object(RssStream* stream, RssAmf0Object* value)
{
	int ret = ERROR_SUCCESS;

	rss_assert(value != NULL);

	// marker
	if (!stream->require(1))
	{
		ret = ERROR_RTMP_AMF0_ENCODE;
		rss_error("amf0 write object marker failed. ret=%d", ret);
		return ret;
	}

	stream->write_1bytes(RTMP_AMF0_Object);
	rss_verbose("amf0 write object marker success");

	// value
	for (int i = 0; i < value->size(); i++)
	{
		std::string name = value->key_at(i);
		RssAmf0Any* any = value->value_at(i);

		if ((ret = rss_amf0_write_utf8(stream, name)) != ERROR_SUCCESS)
		{
			rss_error("write object property name failed. ret=%d", ret);
			return ret;
		}

		if ((ret = rss_amf0_write_any(stream, any)) != ERROR_SUCCESS)
		{
			rss_error("write object property value failed. ret=%d", ret);
			return ret;
		}

		rss_verbose("write amf0 property success. name=%s", name.c_str());
	}

	if ((ret = rss_amf0_write_object_eof(stream, &value->eof)) != ERROR_SUCCESS)
	{
		rss_error("write object eof failed. ret=%d", ret);
		return ret;
	}

	rss_verbose("write amf0 object success.");

	return ret;
}

int rss_amf0_read_ecma_array(RssStream* stream, RssARssAmf0EcmaArray*& value)
{
	int ret = ERROR_SUCCESS;

	// marker
	if (!stream->require(1))
	{
		ret = ERROR_RTMP_AMF0_DECODE;
		rss_error("amf0 read ecma_array marker failed. ret=%d", ret);
		return ret;
	}

	char marker = stream->read_1bytes();
	if (marker != RTMP_AMF0_EcmaArray)
	{
		ret = ERROR_RTMP_AMF0_DECODE;
		rss_error("amf0 check ecma_array marker failed. "
		          "marker=%#x, required=%#x, ret=%d", marker, RTMP_AMF0_Object, ret);
		return ret;
	}
	rss_verbose("amf0 read ecma_array marker success");

	// count
	if (!stream->require(4))
	{
		ret = ERROR_RTMP_AMF0_DECODE;
		rss_error("amf0 read ecma_array count failed. ret=%d", ret);
		return ret;
	}

	int32_t count = stream->read_4bytes();
	rss_verbose("amf0 read ecma_array count success. count=%d", count);

	// value
	value = new RssARssAmf0EcmaArray();
	value->count = count;

	while (!stream->empty())
	{
		// property-name: utf8 string
		std::string property_name;
		if ((ret =rss_amf0_read_utf8(stream, property_name)) != ERROR_SUCCESS)
		{
			rss_error("amf0 ecma_array read property name failed. ret=%d", ret);
			return ret;
		}
		// property-value: any
		RssAmf0Any* property_value = NULL;
		if ((ret = rss_amf0_read_any(stream, property_value)) != ERROR_SUCCESS)
		{
			rss_error("amf0 ecma_array read property_value failed. "
			          "name=%s, ret=%d", property_name.c_str(), ret);
			return ret;
		}

		// AMF0 Object EOF.
		if (property_name.empty() || !property_value || property_value->is_object_eof())
		{
			if (property_value)
			{
				rss_freep(property_value);
			}
			rss_info("amf0 read ecma_array EOF.");
			break;
		}

		// add property
		value->set(property_name, property_value);
	}

	return ret;
}
int rss_amf0_write_ecma_array(RssStream* stream, RssARssAmf0EcmaArray* value)
{
	int ret = ERROR_SUCCESS;

	rss_assert(value != NULL);

	// marker
	if (!stream->require(1))
	{
		ret = ERROR_RTMP_AMF0_ENCODE;
		rss_error("amf0 write ecma_array marker failed. ret=%d", ret);
		return ret;
	}

	stream->write_1bytes(RTMP_AMF0_EcmaArray);
	rss_verbose("amf0 write ecma_array marker success");

	// count
	if (!stream->require(4))
	{
		ret = ERROR_RTMP_AMF0_ENCODE;
		rss_error("amf0 write ecma_array count failed. ret=%d", ret);
		return ret;
	}

	stream->write_4bytes(value->count);
	rss_verbose("amf0 write ecma_array count success. count=%d", value->count);

	// value
	for (int i = 0; i < value->size(); i++)
	{
		std::string name = value->key_at(i);
		RssAmf0Any* any = value->value_at(i);

		if ((ret = rss_amf0_write_utf8(stream, name)) != ERROR_SUCCESS)
		{
			rss_error("write ecma_array property name failed. ret=%d", ret);
			return ret;
		}

		if ((ret = rss_amf0_write_any(stream, any)) != ERROR_SUCCESS)
		{
			rss_error("write ecma_array property value failed. ret=%d", ret);
			return ret;
		}

		rss_verbose("write amf0 property success. name=%s", name.c_str());
	}

	if ((ret = rss_amf0_write_object_eof(stream, &value->eof)) != ERROR_SUCCESS)
	{
		rss_error("write ecma_array eof failed. ret=%d", ret);
		return ret;
	}

	rss_verbose("write ecma_array object success.");

	return ret;
}

int rss_amf0_get_utf8_size(std::string value)
{
	return 2 + value.length();
}

int rss_amf0_get_string_size(std::string value)
{
	return 1 + rss_amf0_get_utf8_size(value);
}

int rss_amf0_get_number_size()
{
	return 1 + 8;
}

int rss_amf0_get_null_size()
{
	return 1;
}

int rss_amf0_get_undefined_size()
{
	return 1;
}

int rss_amf0_get_boolean_size()
{
	return 1 + 1;
}

int rss_amf0_get_object_size(RssAmf0Object* obj)
{
	if (!obj)
	{
		return 0;
	}

	int size = 1;

	for (int i = 0; i < obj->size(); i++)
	{
		std::string name = obj->key_at(i);
		RssAmf0Any* value = obj->value_at(i);

		size += rss_amf0_get_utf8_size(name);
		size += rss_amf0_get_any_size(value);
	}

	size += rss_amf0_get_object_eof_size();

	return size;
}

int rss_amf0_get_ecma_array_size(RssARssAmf0EcmaArray* arr)
{
	if (!arr)
	{
		return 0;
	}

	int size = 1 + 4;

	for (int i = 0; i < arr->size(); i++)
	{
		std::string name = arr->key_at(i);
		RssAmf0Any* value = arr->value_at(i);

		size += rss_amf0_get_utf8_size(name);
		size += rss_amf0_get_any_size(value);
	}

	size += rss_amf0_get_object_eof_size();

	return size;
}

int rss_amf0_get_object_eof_size()
{
	return 2 + 1;
}
