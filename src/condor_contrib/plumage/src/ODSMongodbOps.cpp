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
#include "condor_debug.h"
#include "compat_classad_util.h"
#include "classad/literals.h"
#include "stl_string_utils.h"

// local includes
#include "ODSMongodbOps.h"
#include "ODSUtils.h"

using namespace std;
using namespace mongo;
using namespace plumage::etl;
using namespace plumage::util;

#define  LAST_DB_ERROR()   string last_err = m_db_conn->getLastError(); \
    if (!last_err.empty()) { \
        dprintf(D_ALWAYS,"mongodb getLastError: %s\n",last_err.c_str()); \
        return false; \
    }

ODSMongodbOps::ODSMongodbOps(const string& db_name)
{
    m_db_name = db_name;
}

ODSMongodbOps::~ODSMongodbOps()
{
    delete m_db_conn;
}

bool ODSMongodbOps::init(const string& db_location) {
    try {
        m_db_conn = new DBClientConnection(true);
        m_db_conn->connect(db_location);
    }
    catch (UserException e) {
        dprintf(D_ALWAYS,"ODSMongodbOps::init couldn't connect to db '%s'\n",db_location.c_str());
        return false;
    }
    dprintf(D_FULLDEBUG,"ODSMongodbOps::init successfully connected to db '%s'\n",db_location.c_str());
    return true;
}

bool
ODSMongodbOps::updateAttr (BSONObjBuilder& key, const char* attr_name, const char* attr_value, const classad::Value* attr_type)
{
    // parse the type
    ExprTree *expr=NULL;
    classad::Value value;
    
    if (!attr_type) {
        if ( ParseClassAdRvalExpr ( attr_value, expr ) ) {
            dprintf ( D_ALWAYS, "error: parsing '%s=%s', skipping\n", attr_name, attr_value );
            return false;
        }
    }
    else {
        value.CopyFrom(*attr_type);
    }
    
    classad::Literal* literal = static_cast<classad::Literal*>(expr);
    literal->GetValue(value);
    BSONObjBuilder builder;
    switch ( value.GetType() )
    {
        case classad::Value::INTEGER_VALUE:
            int i;
            value.IsIntegerValue(i);
            builder.append( attr_name, i );
            break;
        case classad::Value::REAL_VALUE:
            double d;
            value.IsRealValue(d);
            builder.append( attr_name, d );
            break;
        case classad::Value::BOOLEAN_VALUE:
            bool b;
            value.IsBooleanValue(b);
            builder.append( attr_name, b );
            break;
        case classad::Value::STRING_VALUE:
        default:
            builder.append( attr_name, trimQuotes(attr_value) );
            break;
    }
    delete expr; expr = NULL;
    
    try {
        m_db_conn->update(m_db_name, Query(key.obj()) ,BSON( "$set" << builder.obj()),FALSE, TRUE);
    }
    catch(DBException& e) {
        dprintf(D_ALWAYS,"ODSMongodbOps::updateAttr caught DBException: %s\n", e.toString().c_str());
        return false;
    }

    LAST_DB_ERROR();

    return true;
}

bool
ODSMongodbOps::deleteAttr (BSONObjBuilder& key, const char* attr_name)
{
    try {
        m_db_conn->update(m_db_name, Query(key.obj()) ,BSON("$unset" << BSON(attr_name << 1)),FALSE,FALSE);
    }
    catch(DBException& e) {
        dprintf(D_ALWAYS,"ODSMongodbOps::deleteAttr caught DBException: %s\n", e.toString().c_str());
        return false;
    }

    LAST_DB_ERROR();

    return true;
}

bool
ODSMongodbOps::createAd(BSONObjBuilder& key) {
    
    try {
        m_db_conn->insert(m_db_name, key.obj());
    }
    catch(DBException& e) {
        dprintf(D_ALWAYS,"ODSMongodbOps::createAd caught DBException: %s\n", e.toString().c_str());
        return false;
    }

    LAST_DB_ERROR();
    
    return true;
}

bool
ODSMongodbOps::deleteAd(BSONObjBuilder& key) {
    
    try {
        m_db_conn->remove(m_db_name, Query(key.obj()), TRUE);
    }
    catch(DBException& e) {
        dprintf(D_ALWAYS,"ODSMongodbOps::deleteAd caught DBException: %s\n", e.toString().c_str());
        return false;
    }

    LAST_DB_ERROR();
    
    return true;
}

bool
ODSMongodbOps::updateAd(BSONObjBuilder& key, ClassAd* ad)
{
    ExprTree *expr;
    const char *name;
    ad->ResetExpr();
    
    BSONObjBuilder bob;
    while (ad->NextExpr(name,expr)) {       

        if (!(expr = ad->Lookup(name))) {
                dprintf(D_FULLDEBUG, "Warning: failed to lookup attribute '%s'\n", name);
                continue;
            }
            
        classad::Value value;
        ad->EvaluateExpr(expr,value);
        switch (value.GetType()) {
            case classad::Value::INTEGER_VALUE:
            {
                int i = 0;
                ad->LookupInteger(name,i);
                bob.append(name,i);
                break;
            }
            case classad::Value::REAL_VALUE:
            {
                float f = 0.0;
                ad->LookupFloat(name,f);
                bob.append(name,f);
                break;
            }
            case classad::Value::BOOLEAN_VALUE:
                bool b;
                ad->LookupBool(name,b);
                bob.append(name,b);
                break;
            case classad::Value::STRING_VALUE:
            default:
                bob.append(name,trimQuotes(ExprTreeToString(expr)));
        }
    }

    try {
        m_db_conn->update(m_db_name, Query(key.obj()), bob.obj(), TRUE, FALSE);
    }
    catch(DBException& e) {
        dprintf(D_ALWAYS,"ODSMongodbOps::updateAd caught DBException: %s\n", e.toString().c_str());
        return false;
    }

    LAST_DB_ERROR();

    return true;
}

bool
ODSMongodbOps::createRecord(BSONObjBuilder& bob)
{
    try {
        m_db_conn->insert(m_db_name, bob.obj());
    }
    catch(DBException& e) {
        dprintf(D_ALWAYS,"ODSMongodbOps::createRecord caught DBException: %s\n", e.toString().c_str());
        return false;
    }

    LAST_DB_ERROR();

    return true;
}

bool
ODSMongodbOps::readRecord(BSONObjBuilder& bob)
{
    try {
        m_db_conn->query(m_db_name, bob.obj());
    }
    catch(DBException& e) {
        dprintf(D_ALWAYS,"ODSMongodbOps::readRecord caught DBException: %s\n", e.toString().c_str());
        return false;
    }

    LAST_DB_ERROR();

    return true;
}

bool
ODSMongodbOps::addIndex(mongo::BSONObj bo)
{
    try {
        m_db_conn->ensureIndex(m_db_name,bo.getOwned());
    }
    catch(DBException& e) {
        dprintf(D_ALWAYS,"ODSMongodbOps::addIndex caught DBException: %s\n", e.toString().c_str());
        return false;
    }

    LAST_DB_ERROR();

    return true;
}
