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
#include "ODSMongodbRW.h"

using namespace compat_classad;
using namespace mongo;
using namespace plumage::etl;

ODSMongodbWriter::ODSMongodbWriter()
{
    //
}

ODSMongodbWriter::~ODSMongodbWriter()
{
    //
}


bool
ODSMongodbWriter::writeAttribute (ClassAd& ad, const char* name)
{
    ExprTree *expr;

    if (!(expr = ad.Lookup(name))) {
        dprintf(D_FULLDEBUG, "Warning: failed to lookup attribute '%s' from ad\n", name);
        return false;
    }

    classad::Value value;
    ad.EvaluateExpr(expr,value);
	std::string key = name;
    switch (value.GetType()) {
        // seems this covers expressions also
        case classad::Value::BOOLEAN_VALUE:
            _map[key] = new AviaryAttribute(AviaryAttribute::EXPR_TYPE,trimQuotes(ExprTreeToString(expr)).c_str());
            break;
        case classad::Value::INTEGER_VALUE:
            _map[key] = new AviaryAttribute(AviaryAttribute::INTEGER_TYPE,ExprTreeToString(expr));
            break;
        case classad::Value::REAL_VALUE:
            _map[key] = new AviaryAttribute(AviaryAttribute::FLOAT_TYPE,ExprTreeToString(expr));
            break;
        case classad::Value::STRING_VALUE:
        default:
            _map[key] = new AviaryAttribute(AviaryAttribute::STRING_TYPE,trimQuotes(ExprTreeToString(expr)).c_str());
    }

    return true;
}

bool
ODSMongodbWriter::writeClassAd(const ClassAd* ad)
{
    ExprTree *expr;
	const char *name;
	ad->ResetExpr();
	while (ad.NextExpr(name,expr)) {
		if (!addAttributeToMap(ad, name, _map)) {
                    return false;
		}
	}

// //debug
//        if (DebugFlags & D_FULLDEBUG) {
//            ad.dPrint(D_FULLDEBUG|D_NOHEADER);
//        }

    return true;
}

