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

#include "condor_common.h"

#include "mongo/client/dbclient.h"

// condor includes
#include "condor_attributes.h"
#include "condor_exprtype.h"
#include "stl_string_utils.h"

// local includes
#include "ODSJobLogConsumer.h"
#include "ODSHistoryUtils.h"

using namespace std;
using namespace mongo;
using namespace bson;
using namespace plumage::etl;

#define IS_JOB(key) ((key) && '0' != (key)[0])

const char DB_NAME[] = "condor.jobs";
mongo::DBClientConnection* db_client = NULL;

template <class T>
bool from_string ( T& t,
                   const string& s,
                   ios_base& ( *f ) ( ios_base& ) )
{
    istringstream iss ( s );
    return ! ( iss >> f >> t ).fail();
}

template <class T>
string to_string ( T t, ios_base & ( *f ) ( ios_base& ) )
{
    ostringstream oss;
    oss << f << t;
    return oss.str();
}

// cleans up the quoted values from the job log reader
static string trimQuotes(const char* str) {
    string val = str;

    size_t endpos = val.find_last_not_of("\\\"");
    if( string::npos != endpos ) {
        val = val.substr( 0, endpos+1 );
    }
    size_t startpos = val.find_first_not_of("\\\"");
    if( string::npos != startpos ) {
        val = val.substr( startpos );
    }

    return val;
}

ODSJobLogConsumer::ODSJobLogConsumer(const string& url): m_reader(NULL)
{ 
    db_client = new mongo::DBClientConnection;
    db_client->connect(url);
}

ODSJobLogConsumer::~ODSJobLogConsumer()
{
    if (db_client) delete db_client;
}

void
ODSJobLogConsumer::Reset()
{
	dprintf(D_FULLDEBUG, "ODSJobLogConsumer::Reset()\n");

	// reload our history
	initHistoryFiles();
}

bool
ODSJobLogConsumer::NewClassAd(const char *_key,
									const char */*type*/,
									const char */*target*/)
{

	dprintf(D_FULLDEBUG, "ODSJobLogConsumer::NewClassAd processing _key='%s'\n", _key);

	// ignore the marker
	if (!IS_JOB(_key)) {
        return true;
    }

	PROC_ID proc = getProcByString(_key);
    BSONObj p = BSONObjBuilder().append(ATTR_CLUSTER_ID, proc.cluster).append(ATTR_PROC_ID, proc.proc).obj();
    db_client->insert(DB_NAME,p);

	return true;
}

bool
ODSJobLogConsumer::DestroyClassAd(const char *_key)
{

	// ignore the marker
	if (strcmp(_key,"0.0") == 0) {
	  return true;
	}

   dprintf ( D_FULLDEBUG, "ODSJobLogConsumer::DestroyClassAd - key '%s'\n", _key);

   // TODO: doubt that we want to destory job record
    PROC_ID proc = getProcByString(_key);
    db_client->remove(DB_NAME,QUERY(ATTR_CLUSTER_ID << (int)proc.cluster << ATTR_PROC_ID << (int)proc.proc),TRUE);
    return true;
}

bool
ODSJobLogConsumer::SetAttribute(const char *_key,
									  const char *_name,
									  const char *_value)
{

	// ignore the marker
	if (strcmp(_key,"0.0") == 0) {
	  return true;
	}

	if (0 == strcmp(_name,ATTR_NEXT_CLUSTER_NUM) ) {
		return true;
	}
	
	// need to ignore these since they were set by NewClassAd and confuse the indexing
	if ((0 == strcmp(_name,ATTR_CLUSTER_ID)) || (0 == strcmp(_name,ATTR_PROC_ID))) {
        return true;
    }

    PROC_ID proc = getProcByString(_key);
    dprintf ( D_FULLDEBUG, "ODSJobLogConsumer::SetAttribute - key '%d.%d'; '%s=%s'\n", proc.cluster,proc.proc,_name,_value);
    
    // parse the type
    ExprTree *expr;
    if ( ParseClassAdRvalExpr ( _value, expr ) )
    {
        dprintf ( D_ALWAYS, "error: parsing %s[%s] = %s, skipping\n", _key, _name, _value );
        return false;
    }
    // add this value to the classad
    classad::Value value;
    expr->Evaluate(value);
    BSONObjBuilder builder;
    switch ( value.GetType() )
    {
        case classad::Value::INTEGER_VALUE:
            int i;
            from_string<int> ( i, string ( _value ), dec );
            builder.append( _name, i );
            break;
        case classad::Value::REAL_VALUE:
            float f;
            from_string<float> ( f, string ( _value ), dec );
            builder.append( _name, f );
            break;
        case classad::Value::STRING_VALUE:
        default:
            builder.append( _name, trimQuotes(_value) );
            break;
    }
    delete expr; expr = NULL;
    
    db_client->update(DB_NAME, BSON( ATTR_CLUSTER_ID << (int)proc.cluster),BSON( "$set" << builder.obj()),FALSE, TRUE);
 
	return true;
}

bool
ODSJobLogConsumer::DeleteAttribute(const char *_key,
										 const char *_name)
{
	// ignore the marker
	if (strcmp(_key,"0.0") == 0) {
	  return true;
	}

    PROC_ID proc = getProcByString(_key);
    db_client->update(DB_NAME,QUERY(ATTR_CLUSTER_ID << (int)proc.cluster << ATTR_PROC_ID << (int)proc.proc),BSON("$unset" << _name),FALSE,FALSE);

    return true;
}
