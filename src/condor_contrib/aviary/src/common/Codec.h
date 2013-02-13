/***************************************************************
 *
 * Copyright (C) 2009-2011 Red Hat, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

#ifndef _CODEC_H
#define _CODEC_H

// c++ includes
#include <map>
#include <vector>

// condor includes
#include <compat_classad.h>

using namespace std;
using namespace compat_classad;

namespace aviary {
namespace codec {

// universal err struct
struct AviaryStatus
{
	// keep these aligned with whatever public API
	// in XSD, JSON, etc.
    enum StatusType
    {
		// don't use "OK"
        A_OK = 0,
        FAIL = 1,
        NO_MATCH = 2,
        STRING_TYPE = 3,
		INVALID_OFFSET = 4,
		UNIMPLEMENTED = 5,
		UNAVAILABLE = 6
    };

    StatusType type;
    string text;
};

class AviaryAttribute
{
public:
    enum AttributeType
    {
        EXPR_TYPE = 0,
        INTEGER_TYPE = 1,
        FLOAT_TYPE = 2,
        STRING_TYPE = 3
    };
    AviaryAttribute ( AttributeType _type, const char* _value): m_type(_type), m_value(_value) {};
    ~AviaryAttribute() { /**/ };

    AttributeType getType() const { return m_type; }
    const char * getValue() const { return m_value.c_str(); }

private:
    AttributeType m_type;
    std::string m_value;
};

typedef map<std::string, AviaryAttribute*> AttributeMapType;
typedef AttributeMapType::const_iterator AttributeMapIterator;

// TODO: revisit
// singleton class for encoding/decoding from/to transport-specific types
// to a simple type to be used in the impl;
// intent is to sit above ClassAds but still interact with them;
// base is suitable for SOAP, but QMF might need subclassing
// with it's !!descriptors idiom
class Codec {
public:
    virtual bool addAttributeToMap(ClassAd& ad, const char* name, AttributeMapType& _map) = 0;
    virtual bool classAdToMap(ClassAd &ad, AttributeMapType &_map) = 0;
    virtual bool mapToClassAd(AttributeMapType &_map, ClassAd &ad, std::string& text) = 0;

};

// TODO: defer until linking issues sorted
//class CodecFactory {
// public:
//     virtual Codec* createCodec() = 0;
//
// };
//
// class DefaultCodecFactory: public CodecFactory {
// public:
//     virtual Codec* createCodec();
// protected:
//     Codec* m_codec;
// };

class BaseCodec: public Codec {
public:
    //friend class DefaultCodecFactory;
    virtual bool addAttributeToMap(ClassAd& ad, const char* name, AttributeMapType& _map);
    virtual bool classAdToMap(ClassAd &ad, AttributeMapType &_map);
	virtual bool mapToClassAd(AttributeMapType &_map, ClassAd &ad, std::string& text);

//protected:
    BaseCodec();
    ~BaseCodec();

private:
    BaseCodec (const BaseCodec &);
    BaseCodec& operator= (const BaseCodec &);

};

}}

#endif    // _CODEC_H
