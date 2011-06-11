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

// always first
#include "condor_common.h"

// local includes
#include "ODSMongodbRW.h"

// other condor includes
#include "condor_qmgr.h"

using namespace std;
using namespace compat_classad;
using namespace mongo;
using namespace plumage::etl;

ODSMongodbWriter::ODSMongodbWriter(const string& db_name)
{
    m_db_name = db_name;
}

ODSMongodbWriter::~ODSMongodbWriter()
{
    //
}

bool ODSMongodbWriter::init(const string& db_location) {
    try {
        m_db_conn.connect(db_location);
    }
    catch (DBException e) {
        dprintf(D_ALWAYS,"ODSMongodbWriter::init couldn't connect to db '%s'\n",db_location.c_str());
        return false;
    }
    dprintf(D_FULLDEBUG,"ODSMongodbWriter::init successfully connected to db '%s'\n",db_location.c_str());
    return true;
}

bool
ODSMongodbWriter::writeAttribute (const string& key, const string& name, const classad::Value& type, const string& value)
{
//     ExprTree *expr;
// 
//     if (!(expr = ad.Lookup(name))) {
//         dprintf(D_FULLDEBUG, "Warning: failed to lookup attribute '%s' from ad\n", name);
//         return false;
//     }
// 
//     classad::Value value;
//     ad.EvaluateExpr(expr,value);
// 	string key = name;
//     switch (value.GetType()) {
//         // seems this covers expressions also
//         case classad::Value::BOOLEAN_VALUE:
//             _map[key] = new AviaryAttribute(AviaryAttribute::EXPR_TYPE,trimQuotes(ExprTreeToString(expr)).c_str());
//             break;
//         case classad::Value::INTEGER_VALUE:
//             _map[key] = new AviaryAttribute(AviaryAttribute::INTEGER_TYPE,ExprTreeToString(expr));
//             break;
//         case classad::Value::REAL_VALUE:
//             _map[key] = new AviaryAttribute(AviaryAttribute::FLOAT_TYPE,ExprTreeToString(expr));
//             break;
//         case classad::Value::STRING_VALUE:
//         default:
//             _map[key] = new AviaryAttribute(AviaryAttribute::STRING_TYPE,trimQuotes(ExprTreeToString(expr)).c_str());
//     }
// 
    return true;
}

bool
ODSMongodbWriter::writeClassAd(const string& key, ClassAd* ad)
{
    clock_t start, end;
    double elapsed;
    ExprTree *expr;
    const char *name;
    ad->ResetExpr();
    
    BSONObjBuilder b;
    start = clock();
    while (ad->NextExpr(name,expr)) {        
        b.append(name,ExprTreeToString(expr));
    }
    try {
        m_db_conn.insert(m_db_name, b.obj());
    }
    catch(DBException& e) {
        dprintf(D_ALWAYS,"ODSMongodbWriter::writeClassAd caught DBException: %s\n", e.toString().c_str());
        return false;
    }
    end = clock();
    elapsed = ((float) (end - start)) / CLOCKS_PER_SEC;
    string last_err = m_db_conn.getLastError();
    if (!last_err.empty()) {
        dprintf(D_ALWAYS,"mongodb getLastError: %s on key '%s'\n",last_err.c_str(), key.c_str());
        return false;
    }
    dprintf(D_FULLDEBUG,"mongodb ClassAd insert: %1.9f sec\n",elapsed);

    return true;
}
