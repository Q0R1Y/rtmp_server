#ifndef RSS_CORE_AMF0_HPP
#define RSS_CORE_AMF0_HPP

/*
#include <rss_core_amf0.hpp>
*/

#include <rss_core.hpp>

#include <string>
#include <vector>

class RssStream;
class RssAmf0Object;

/**
* any amf0 value.
* 2.1 Types Overview
* value-type = number-type | boolean-type | string-type | object-type
* 		| null-marker | undefined-marker | reference-type | ecma-array-type
* 		| strict-array-type | date-type | long-string-type | xml-document-type
* 		| typed-object-type
*/
struct RssAmf0Any
{
	char marker;

	RssAmf0Any();
	virtual ~RssAmf0Any();

	virtual bool is_string();
	virtual bool is_boolean();
	virtual bool is_number();
	virtual bool is_null();
	virtual bool is_undefined();
	virtual bool is_object();
	virtual bool is_object_eof();
	virtual bool is_ecma_array();
};

/**
* read amf0 string from stream.
* 2.4 String Type
* string-type = string-marker UTF-8
* @return default value is empty string.
*/
struct RssAmf0String : public RssAmf0Any
{
	std::string value;

	RssAmf0String(const char* _value = NULL);
	virtual ~RssAmf0String();
};

/**
* read amf0 boolean from stream.
* 2.4 String Type
* boolean-type = boolean-marker U8
* 		0 is false, <> 0 is true
* @return default value is false.
*/
struct RssAmf0Boolean : public RssAmf0Any
{
	bool value;

	RssAmf0Boolean(bool _value = false);
	virtual ~RssAmf0Boolean();
};

/**
* read amf0 number from stream.
* 2.2 Number Type
* number-type = number-marker DOUBLE
* @return default value is 0.
*/
struct RssAmf0Number : public RssAmf0Any
{
	double value;

	RssAmf0Number(double _value = 0.0);
	virtual ~RssAmf0Number();
};

/**
* read amf0 null from stream.
* 2.7 null Type
* null-type = null-marker
*/
struct RssAmf0Null : public RssAmf0Any
{
	RssAmf0Null();
	virtual ~RssAmf0Null();
};

/**
* read amf0 undefined from stream.
* 2.8 undefined Type
* undefined-type = undefined-marker
*/
struct RssAmf0Undefined : public RssAmf0Any
{
	RssAmf0Undefined();
	virtual ~RssAmf0Undefined();
};

/**
* 2.11 Object End Type
* object-end-type = UTF-8-empty object-end-marker
* 0x00 0x00 0x09
*/
struct RssAmf0ObjectEOF : public RssAmf0Any
{
	int16_t utf8_empty;

	RssAmf0ObjectEOF();
	virtual ~RssAmf0ObjectEOF();
};

/**
* to ensure in inserted order.
* for the FMLE will crash when AMF0Object is not ordered by inserted,
* if ordered in map, the string compare order, the FMLE will creash when
* get the response of connect app.
*/
struct RssUnSortedHashtable
{
private:
	typedef std::pair<std::string, RssAmf0Any*> RssObjectPropertyType;
	std::vector<RssObjectPropertyType> properties;
public:
	RssUnSortedHashtable();
	virtual ~RssUnSortedHashtable();

	virtual int size();
	virtual void clear();
	virtual std::string key_at(int index);
	virtual RssAmf0Any* value_at(int index);
	virtual void set(std::string key, RssAmf0Any* value);

	virtual RssAmf0Any* get_property(std::string name);
	virtual RssAmf0Any* ensure_property_string(std::string name);
	virtual RssAmf0Any* ensure_property_number(std::string name);
};

/**
* 2.5 Object Type
* anonymous-object-type = object-marker *(object-property)
* object-property = (UTF-8 value-type) | (UTF-8-empty object-end-marker)
*/
struct RssAmf0Object : public RssAmf0Any
{
private:
	RssUnSortedHashtable properties;
public:
	RssAmf0ObjectEOF eof;

	RssAmf0Object();
	virtual ~RssAmf0Object();

	virtual int size();
	virtual std::string key_at(int index);
	virtual RssAmf0Any* value_at(int index);
	virtual void set(std::string key, RssAmf0Any* value);

	virtual RssAmf0Any* get_property(std::string name);
	virtual RssAmf0Any* ensure_property_string(std::string name);
	virtual RssAmf0Any* ensure_property_number(std::string name);
};

/**
* 2.10 ECMA Array Type
* ecma-array-type = associative-count *(object-property)
* associative-count = U32
* object-property = (UTF-8 value-type) | (UTF-8-empty object-end-marker)
*/
struct RssARssAmf0EcmaArray : public RssAmf0Any
{
private:
	RssUnSortedHashtable properties;
public:
	int32_t count;
	RssAmf0ObjectEOF eof;

	RssARssAmf0EcmaArray();
	virtual ~RssARssAmf0EcmaArray();

	virtual int size();
	virtual void clear();
	virtual std::string key_at(int index);
	virtual RssAmf0Any* value_at(int index);
	virtual void set(std::string key, RssAmf0Any* value);

	virtual RssAmf0Any* get_property(std::string name);
	virtual RssAmf0Any* ensure_property_string(std::string name);
};

/**
* read amf0 utf8 string from stream.
* 1.3.1 Strings and UTF-8
* UTF-8 = U16 *(UTF8-char)
* UTF8-char = UTF8-1 | UTF8-2 | UTF8-3 | UTF8-4
* UTF8-1 = %x00-7F
* @remark only support UTF8-1 char.
*/
extern int rss_amf0_read_utf8(RssStream* stream, std::string& value);
extern int rss_amf0_write_utf8(RssStream* stream, std::string value);

/**
* read amf0 string from stream.
* 2.4 String Type
* string-type = string-marker UTF-8
*/
extern int rss_amf0_read_string(RssStream* stream, std::string& value);
extern int rss_amf0_write_string(RssStream* stream, std::string value);

/**
* read amf0 boolean from stream.
* 2.4 String Type
* boolean-type = boolean-marker U8
* 		0 is false, <> 0 is true
*/
extern int rss_amf0_read_boolean(RssStream* stream, bool& value);
extern int rss_amf0_write_boolean(RssStream* stream, bool value);

/**
* read amf0 number from stream.
* 2.2 Number Type
* number-type = number-marker DOUBLE
*/
extern int rss_amf0_read_number(RssStream* stream, double& value);
extern int rss_amf0_write_number(RssStream* stream, double value);

/**
* read amf0 null from stream.
* 2.7 null Type
* null-type = null-marker
*/
extern int rss_amf0_read_null(RssStream* stream);
extern int rss_amf0_write_null(RssStream* stream);

/**
* read amf0 undefined from stream.
* 2.8 undefined Type
* undefined-type = undefined-marker
*/
extern int rss_amf0_read_undefined(RssStream* stream);
extern int rss_amf0_write_undefined(RssStream* stream);

extern int rss_amf0_read_any(RssStream* stream, RssAmf0Any*& value);

/**
* read amf0 object from stream.
* 2.5 Object Type
* anonymous-object-type = object-marker *(object-property)
* object-property = (UTF-8 value-type) | (UTF-8-empty object-end-marker)
*/
extern int rss_amf0_read_object(RssStream* stream, RssAmf0Object*& value);
extern int rss_amf0_write_object(RssStream* stream, RssAmf0Object* value);

/**
* read amf0 object from stream.
* 2.10 ECMA Array Type
* ecma-array-type = associative-count *(object-property)
* associative-count = U32
* object-property = (UTF-8 value-type) | (UTF-8-empty object-end-marker)
*/
extern int rss_amf0_read_ecma_array(RssStream* stream, RssARssAmf0EcmaArray*& value);
extern int rss_amf0_write_ecma_array(RssStream* stream, RssARssAmf0EcmaArray* value);

/**
* get amf0 objects size.
*/
extern int rss_amf0_get_utf8_size(std::string value);
extern int rss_amf0_get_string_size(std::string value);
extern int rss_amf0_get_number_size();
extern int rss_amf0_get_null_size();
extern int rss_amf0_get_undefined_size();
extern int rss_amf0_get_boolean_size();
extern int rss_amf0_get_object_size(RssAmf0Object* obj);
extern int rss_amf0_get_ecma_array_size(RssARssAmf0EcmaArray* arr);

/**
* convert the any to specified object.
* @return T*, the converted object. never NULL.
* @remark, user must ensure the current object type,
* 		or the covert will cause assert failed.
*/
template<class T>
T* rss_amf0_convert(RssAmf0Any* any)
{
	T* p = dynamic_cast<T*>(any);
	rss_assert(p != NULL);
	return p;
}

#endif