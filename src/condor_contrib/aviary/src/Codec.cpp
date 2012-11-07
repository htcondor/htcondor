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

// condor includes
#include "condor_common.h"
#include "condor_qmgr.h"

// local includes
#include "Codec.h"
#include "AviaryUtils.h"

using namespace compat_classad;
using namespace aviary::codec;
using namespace aviary::util;

// TODO: defer until linking issues sorted
//Codec*
//DefaultCodecFactory::createCodec() {
//    if (!m_codec) {
//        m_codec = new BaseCodec;
//    }
//    return m_codec;
//}

BaseCodec::BaseCodec()
{
    //
}

BaseCodec::~BaseCodec()
{
    //
}


bool
BaseCodec::addAttributeToMap (ClassAd& ad, const char* name, AttributeMapType& _map)
{
    ExprTree *expr;

    // All these extra lookups are horrible. They have to
    // be there because the ClassAd may have multiple
    // copies of the same attribute name! This means that
    // the last attribute with a given name will set the
    // value, but the last attribute tends to be the
    // attribute with the oldest (wrong) value. How
    // annoying is that!
    if (!(expr = ad.Lookup(name))) {
        dprintf(D_FULLDEBUG, "Warning: failed to lookup attribute '%s' from ad\n", name);
        return false;
    }

    classad::Value value;
    ad.EvaluateExpr(expr,value);
	std::string key = name;
    switch (value.GetType()) {
        // seems this covers expressions also
        case classad::Value::ERROR_VALUE:
        case classad::Value::UNDEFINED_VALUE:
        case classad::Value::BOOLEAN_VALUE:
            _map[key] = new AviaryAttribute(AviaryAttribute::EXPR_TYPE,trimQuotes(ExprTreeToString(expr)).c_str());
            break;
        case classad::Value::INTEGER_VALUE:
        {
            int i;
            value.IsIntegerValue (i);
            string i_str;
            formatstr(i_str,"%d",i);
            _map[key] = new AviaryAttribute(AviaryAttribute::INTEGER_TYPE,i_str.c_str());
            break;
        }
        case classad::Value::REAL_VALUE:
        {
            double d;
            value.IsRealValue(d);
            string d_str;
            formatstr(d_str,"%f",d);
            _map[key] = new AviaryAttribute(AviaryAttribute::FLOAT_TYPE,d_str.c_str());
            break;
        }
        case classad::Value::STRING_VALUE:
        default:
            _map[key] = new AviaryAttribute(AviaryAttribute::STRING_TYPE,trimQuotes(ExprTreeToString(expr)).c_str());
    }

    return true;
}

bool
BaseCodec::classAdToMap(ClassAd& ad, AttributeMapType& _map)
{
    ExprTree *expr;
	const char *name;
	ad.ResetExpr();
	_map.clear();
	while (ad.NextExpr(name,expr)) {
		if (!addAttributeToMap(ad, name, _map)) {
                    return false;
		}
	}

// //debug
//        if (IsFulldebug(D_FULLDEBUG)) {
//            ad.dPrint(D_FULLDEBUG|D_NOHEADER);
//        }

    return true;
}


bool
BaseCodec::mapToClassAd(AttributeMapType& _map, ClassAd& ad, string& text)
{

    for (AttributeMapIterator entry = _map.begin(); _map.end() != entry; entry++) {
        const char* name = entry->first.c_str();

		if (isKeyword(name)) {
			text = "Reserved ClassAd keyword cannot be an attribute name: "+ entry->first;
			return false;
		}

        AviaryAttribute* value = entry->second;

        switch (value->getType()) {
            case AviaryAttribute::INTEGER_TYPE:
                ad.Assign(name, atoi(value->getValue()));
                break;
            case AviaryAttribute::FLOAT_TYPE:
                ad.Assign(name, atof(value->getValue()));
                break;
            case AviaryAttribute::STRING_TYPE:
                ad.Assign(name, value->getValue());
                break;
            case AviaryAttribute::EXPR_TYPE:
                ad.AssignExpr(name, value->getValue());
                break;
            default:
                dprintf(D_FULLDEBUG, "Warning: Unknown/unsupported type in map for attribute '%s'\n", name);
        }
    }

//     // debug
//     if (IsFulldebug(D_FULLDEBUG)) {
//           ad.dPrint(D_FULLDEBUG|D_NOHEADER);
//     }

    return true;
}
