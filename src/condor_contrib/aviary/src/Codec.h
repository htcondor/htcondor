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

class Attribute
{
public:
    enum AttributeType
    {
        EXPR_TYPE = 0,
        INTEGER_TYPE = 1,
        FLOAT_TYPE = 2,
        STRING_TYPE = 3
    };
    Attribute ( AttributeType _type, const char* _value): m_type(_type), m_value(_value) {};
    ~Attribute();

    AttributeType getType() const { return m_type; }
    const char * getValue() const { return m_value; }

private:
    AttributeType m_type;
    const char * m_value;
};

typedef map<const char*, Attribute*> AttributeMapType;
typedef vector<Attribute*> AttributeVectorType;
typedef AttributeMapType::const_iterator AttributeMapIterator;
typedef AttributeVectorType::const_iterator AttributeVectorIterator;

// singleton class for encoding/decoding from/to transport-specific types
// to a simple type to be used in the impl;
// intent is to sit above ClassAds but still interact with them;
// base is suitable for SOAP, but QMF might need subclassing
// with it's !!descriptors idiom
class Codec {
public:
    virtual bool addAttributeToMap(ClassAd& ad, const char* name, AttributeMapType& _map) = 0;
    virtual bool classAdToMap(ClassAd &ad, AttributeMapType &_map) = 0;
    virtual bool mapToClassAd(AttributeMapType &_map, ClassAd &ad) = 0;
    virtual bool procIdToMap(int clusterId, int procId, AttributeMapType& _map) = 0;

};

class BaseCodec: public Codec {
public:
    bool addAttributeToMap(ClassAd& ad, const char* name, AttributeMapType& _map);
    bool classAdToMap(ClassAd &ad, AttributeMapType &_map);
    bool mapToClassAd(AttributeMapType &_map, ClassAd &ad);
    bool procIdToMap(int clusterId, int procId, AttributeMapType& _map);

protected:
    BaseCodec();
    ~BaseCodec();

private:
    BaseCodec (const BaseCodec &);
    BaseCodec& operator= (const BaseCodec &);

};

class CodecFactory {
public:
    virtual Codec* createCodec() = 0;

protected:
    Codec* m_codec;
};

template <class Codec>
class DefaultCodecFactory: public CodecFactory {
public:
    virtual Codec* createCodec();

};

}}

#endif    // _CODEC_H
