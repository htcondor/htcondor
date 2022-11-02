/***************************************************************
 *
 * Copyright (C) 1990-2011, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
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
#include "condor_query.h"
#include "condor_attributes.h"
#include "condor_config.h"
#include "condor_io.h"
#include "condor_adtypes.h"
#include "condor_debug.h"
#include "condor_classad.h"
#include "internet.h"
#include "daemon.h"
#include "dc_collector.h"
#include "condor_arglist.h"

// The order and number of the elements of the following arrays *are*
// important.  (They follow the structure of the enumerations supplied
// in the header file condor_query.h)
const char *ScheddStringKeywords [] = 
{
	ATTR_NAME 
};

const char *ScheddIntegerKeywords [] = 
{
	ATTR_NUM_USERS,
	ATTR_IDLE_JOBS,
	ATTR_RUNNING_JOBS
};

const char *ScheddFloatKeywords [] = 
{
	""		// add null string to avoid compiler error
};

const char *StartdStringKeywords [] = 
{
	ATTR_NAME,
	ATTR_MACHINE,
	ATTR_ARCH,
	ATTR_OPSYS
};

const char *StartdIntegerKeywords [] = 
{
	ATTR_MEMORY,
	ATTR_DISK
};

const char *StartdFloatKeywords [] =
{
	""		// add null string to avoid compiler error
};

// for getJobStatusString (see condor_util_lib/proc.c)
const char *GridManagerStringKeywords [] = 
{
    ATTR_NAME,
    "HashName",
    ATTR_SCHEDD_NAME,
    ATTR_OWNER,
    
    // ATTR_GRID_RESOURCE_TYPE
};

const char *GridManagerIntegerKeywords [] = 
{
    "NumJobs",
    "JobLimit",
    "SubmitLimit",
    "SubmitsInProgress",
    "SubmitsQueued",
    "SubmitsAllowed",
    "SubmitsWanted"
};

const char *GridManagerFloatKeywords [] =
{
    ""		// add null string to avoid compiler error
};

// normal ctor
CondorQuery::
CondorQuery (AdTypes qType)
{
	genericQueryType = NULL;
	resultLimit = 0;
	queryType = qType;
	switch (qType)
	{
	  case STARTD_AD:
		query.setNumStringCats (STARTD_STRING_THRESHOLD);
		query.setNumIntegerCats(STARTD_INT_THRESHOLD);
		query.setNumFloatCats  (STARTD_FLOAT_THRESHOLD);
		query.setIntegerKwList (const_cast<char **>(StartdIntegerKeywords));
		query.setStringKwList (const_cast<char **>(StartdStringKeywords));
		query.setFloatKwList (const_cast<char **>(StartdFloatKeywords));
		command = QUERY_STARTD_ADS;
		break;

	  case STARTD_PVT_AD:
		query.setNumStringCats (STARTD_STRING_THRESHOLD);
		query.setNumIntegerCats(STARTD_INT_THRESHOLD);
		query.setNumFloatCats (STARTD_FLOAT_THRESHOLD);
		query.setIntegerKwList (const_cast<char **>(StartdIntegerKeywords));
		query.setStringKwList (const_cast<char **>(StartdStringKeywords));
		query.setFloatKwList (const_cast<char **>(StartdFloatKeywords));
		command = QUERY_STARTD_PVT_ADS;
		break;

	  case SCHEDD_AD:
		query.setNumStringCats (SCHEDD_STRING_THRESHOLD);
		query.setNumIntegerCats(SCHEDD_INT_THRESHOLD);
		query.setNumFloatCats (SCHEDD_FLOAT_THRESHOLD);
		query.setIntegerKwList (const_cast<char **>(ScheddIntegerKeywords));
		query.setStringKwList (const_cast<char **>(ScheddStringKeywords));
		query.setFloatKwList (const_cast<char **>(ScheddFloatKeywords));
		command = QUERY_SCHEDD_ADS;
		break;

	  case SUBMITTOR_AD:
		query.setNumStringCats (SCHEDD_STRING_THRESHOLD);
		query.setNumIntegerCats(SCHEDD_INT_THRESHOLD);
		query.setNumFloatCats (SCHEDD_FLOAT_THRESHOLD);
		query.setIntegerKwList (const_cast<char **>(ScheddIntegerKeywords));
		query.setStringKwList (const_cast<char **>(ScheddStringKeywords));
		query.setFloatKwList (const_cast<char **>(ScheddFloatKeywords));
		command = QUERY_SUBMITTOR_ADS;
		break;

      case GRID_AD:
        query.setNumStringCats (GRID_STRING_THRESHOLD);
		query.setNumIntegerCats(GRID_INT_THRESHOLD);
		query.setNumFloatCats (GRID_FLOAT_THRESHOLD);
		query.setIntegerKwList (const_cast<char **>(GridManagerIntegerKeywords));
		query.setStringKwList (const_cast<char **>(GridManagerStringKeywords));
		query.setFloatKwList (const_cast<char **>(GridManagerFloatKeywords));
		command = QUERY_GRID_ADS;
		break;

	  case LICENSE_AD:
		query.setNumStringCats (0);
		query.setNumIntegerCats(0);
		query.setNumFloatCats  (0);
		command = QUERY_LICENSE_ADS;
		break;

	  case MASTER_AD:
		query.setNumStringCats (0);
		query.setNumIntegerCats(0);
		query.setNumFloatCats  (0);
		command = QUERY_MASTER_ADS;
		break;

	  case CKPT_SRVR_AD:
		query.setNumStringCats (0);
		query.setNumIntegerCats(0);
		query.setNumFloatCats  (0);
		command = QUERY_CKPT_SRVR_ADS;
		break;

	  case DEFRAG_AD:
		query.setNumStringCats (0);
		query.setNumIntegerCats(0);
		query.setNumFloatCats  (0);
		command = QUERY_ANY_ADS;
		break;

	  case COLLECTOR_AD:
		query.setNumStringCats (0);
		query.setNumIntegerCats(0);
		query.setNumFloatCats  (0);
		command = QUERY_COLLECTOR_ADS;
		break;

	  case NEGOTIATOR_AD:
		query.setNumStringCats (0);
		query.setNumIntegerCats(0);
		query.setNumFloatCats  (0);
		command = QUERY_NEGOTIATOR_ADS;
		break;

	  case HAD_AD:
		query.setNumStringCats (0);
		query.setNumIntegerCats(0);
		query.setNumFloatCats  (0);
		command = QUERY_HAD_ADS;
		break;

	  case STORAGE_AD:
		query.setNumStringCats (0);
		query.setNumIntegerCats(0);
		query.setNumFloatCats  (0);
		command = QUERY_STORAGE_ADS;
		break;

	  case CREDD_AD:
		query.setNumStringCats (0);
		query.setNumIntegerCats(0);
		query.setNumFloatCats  (0);
		command = QUERY_ANY_ADS;
		break;

	  case GENERIC_AD:
		query.setNumStringCats (0);
		query.setNumIntegerCats(0);
		query.setNumFloatCats  (0);
		command = QUERY_GENERIC_ADS;
		break;

	  case ANY_AD:
		query.setNumStringCats (0);
		query.setNumIntegerCats(0);
		query.setNumFloatCats  (0);
		command = QUERY_ANY_ADS;
		break;

	  case DATABASE_AD:
		query.setNumStringCats (0);
		query.setNumIntegerCats(0);
		query.setNumFloatCats  (0);
		command = QUERY_ANY_ADS;
		break;

	  case TT_AD:
		query.setNumStringCats (0);
		query.setNumIntegerCats(0);
		query.setNumFloatCats  (0);
		command = QUERY_ANY_ADS;
		break;

	  case ACCOUNTING_AD:
		query.setNumStringCats (0);
		query.setNumIntegerCats(0);
		query.setNumFloatCats  (0);
		command = QUERY_ACCOUNTING_ADS;
		break;

	  default:
		command = -1;
		queryType = (AdTypes) -1;
	}
}


// copy ctor; makes deep copy
CondorQuery::
CondorQuery (const CondorQuery & /* from */)
{
		// Unimplemented!
	EXCEPT( "CondorQuery copy constructor called, but unimplemented!" );
}


// dtor
CondorQuery::
~CondorQuery ()
{	
	if(genericQueryType) {
		free(genericQueryType);
	}
}

// assignment
CondorQuery &CondorQuery::
operator= (const CondorQuery &)
{
		// Unimplemented!
	EXCEPT( "CondorQuery operator= called, but unimplemented!" );
	return *this;
}


// clear particular string category
QueryResult CondorQuery::
clearStringConstraints (const int i)
{
	return (QueryResult) query.clearString (i);
}


// clear particular integer category
QueryResult CondorQuery::
clearIntegerConstraints (const int i)
{
	return (QueryResult) query.clearInteger (i);
}

// clear particular float category
QueryResult CondorQuery::
clearFloatConstraints (const int i)
{
	return (QueryResult) query.clearFloat (i);
}


void CondorQuery::
clearORCustomConstraints (void)
{
	query.clearCustomOR ();
}


void CondorQuery::
clearANDCustomConstraints (void)
{
	query.clearCustomAND ();
}


// add a string constraint
QueryResult CondorQuery::
addConstraint (const int cat, const char *value)
{
	return (QueryResult) query.addString (cat, value);
}


// add an integer constraint
QueryResult CondorQuery::
addConstraint (const int cat, const int value)
{
	return (QueryResult) query.addInteger (cat, value);
}


// add a float constraint
QueryResult CondorQuery::
addConstraint (const int cat, const float value)
{
	return (QueryResult) query.addFloat (cat, value);
}


// add a custom constraint
QueryResult CondorQuery::
addANDConstraint (const char *value)
{
	return (QueryResult) query.addCustomAND (value);
}


// add a custom constraint
QueryResult CondorQuery::
addORConstraint (const char *value)
{
	return (QueryResult) query.addCustomOR (value);
}


// If this query is a DNS style, "just give me the ad so I can
// look up the address of the daemon", project out only those
// attributes needed for lookup.  This speeds up lookups
// for daemons like the schedd that may have thousands of
// superflous statistics in the ad.
bool
CondorQuery::setLocationLookup(const std::string &location, bool want_one_result /*=true*/)
{
	extraAttrs.InsertAttr(ATTR_LOCATION_QUERY, location);

	std::vector<std::string> attrs; attrs.reserve(7);
	attrs.push_back(ATTR_VERSION);
	attrs.push_back(ATTR_PLATFORM);
	attrs.push_back(ATTR_MY_ADDRESS);
	attrs.push_back(ATTR_ADDRESS_V1);
	attrs.push_back(ATTR_NAME);
	attrs.push_back(ATTR_MACHINE);
	attrs.push_back(ATTR_REMOTE_ADMIN_CAPABILITY);
	if (queryType == SCHEDD_AD)
	{
		attrs.push_back(ATTR_SCHEDD_IP_ADDR);
	}

	setDesiredAttrs(attrs);
	if (want_one_result) { setResultLimit(1); }
	return true;
}

// process ads from the collector, handing each to the callback
// callback will return 'false' if it took ownership of the ad.
QueryResult CondorQuery::
processAds (bool (*callback)(void*, ClassAd *), void* pv, const char * poolName, CondorError* errstack /*= NULL*/)
{
	Sock*    sock; 
	QueryResult result;
	ClassAd  queryAd(extraAttrs);

	if ( !poolName ) {
		return Q_NO_COLLECTOR_HOST;
	}

	// contact collector
	Daemon my_collector( DT_COLLECTOR, poolName, NULL );
	if( !my_collector.locate() ) {
			// We were passed a bogus poolName, abort gracefully
		return Q_NO_COLLECTOR_HOST;
	}


	// make the query ad
	result = getQueryAd (queryAd);
	if (result != Q_OK) return result;

	if (IsDebugLevel(D_HOSTNAME)) {
		dprintf( D_HOSTNAME, "Querying collector %s (%s) with classad:\n", 
				 my_collector.addr(), my_collector.fullHostname() );
		dPrintAd( D_HOSTNAME, queryAd );
		dprintf( D_HOSTNAME, " --- End of Query ClassAd ---\n" );
	}


	int mytimeout = param_integer ("QUERY_TIMEOUT",60); 
	if (!(sock = my_collector.startCommand(command, Stream::reli_sock, mytimeout, errstack)) ||
	    !putClassAd (sock, queryAd) || !sock->end_of_message()) {

		if (sock) {
			delete sock;
		}
		return Q_COMMUNICATION_ERROR;
	}

	// get result
	sock->decode ();
	int more = 1;
	while (more)
	{
		if (!sock->code (more)) {
			sock->end_of_message();
			delete sock;
			return Q_COMMUNICATION_ERROR;
		}
		if (more) {
			ClassAd * ad = new ClassAd;
			if( !getClassAd(sock, *ad) ) {
				sock->end_of_message();
				delete ad;
				delete sock;
				return Q_COMMUNICATION_ERROR;
			}
			if (callback(pv, ad)) {
				delete ad;
			}
		}
	}
	sock->end_of_message();

	// finalize
	sock->close();
	delete sock;

	return (Q_OK);
}

// callback used by fetchAds
static bool fetchAds_callback(void* pv, ClassAd * ad) { ClassAdList * padList = (ClassAdList *)pv; padList->Insert (ad); return false; }

// fetch all ads from the collector that satisfy the constraints
QueryResult CondorQuery::
fetchAds (ClassAdList &adList, const char *poolName, CondorError* errstack)
{
	return processAds(fetchAds_callback, &adList, poolName, errstack);
}

void CondorQuery::
setGenericQueryType(const char* genericType) {
	if(genericQueryType) {
		free(genericQueryType);
	}
	genericQueryType = strdup(genericType);
}

QueryResult CondorQuery::
getQueryAd (ClassAd &queryAd)
{
	QueryResult	result;
	ExprTree *tree;

	queryAd = extraAttrs;
	if (resultLimit > 0) { queryAd.Assign(ATTR_LIMIT_RESULTS, resultLimit); }

	result = (QueryResult) query.makeQuery (tree);
	if (result != Q_OK) return result;
	queryAd.Insert(ATTR_REQUIREMENTS, tree);

	// fix types
	SetMyTypeName (queryAd, QUERY_ADTYPE);
	switch (queryType) {

	  case DEFRAG_AD:
		SetTargetTypeName(queryAd, DEFRAG_ADTYPE);
		break;
	  case STARTD_AD:
	  case STARTD_PVT_AD:
		SetTargetTypeName (queryAd, STARTD_ADTYPE);
		break;

	  case SCHEDD_AD:
		SetTargetTypeName (queryAd, SCHEDD_ADTYPE);
		break;

	  case SUBMITTOR_AD:
		SetTargetTypeName (queryAd, SUBMITTER_ADTYPE);
		break;

	  case LICENSE_AD:
		SetTargetTypeName (queryAd, LICENSE_ADTYPE);
		break;

	  case MASTER_AD:
		SetTargetTypeName (queryAd, MASTER_ADTYPE);
		break;

	  case CKPT_SRVR_AD:
		SetTargetTypeName (queryAd, CKPT_SRVR_ADTYPE);
		break;

	  case COLLECTOR_AD:
		SetTargetTypeName (queryAd, COLLECTOR_ADTYPE);
		break;

	  case NEGOTIATOR_AD:
		SetTargetTypeName (queryAd, NEGOTIATOR_ADTYPE);
		break;

      case STORAGE_AD:
        SetTargetTypeName (queryAd, STORAGE_ADTYPE);
        break;

      case CREDD_AD:
        SetTargetTypeName (queryAd, CREDD_ADTYPE);
        break;

	  case GENERIC_AD:
		if ( genericQueryType ) {
			SetTargetTypeName (queryAd, genericQueryType);
		} else {
			SetTargetTypeName (queryAd, GENERIC_ADTYPE);
		}
		break;

	  case ANY_AD:
		SetTargetTypeName (queryAd, ANY_ADTYPE);
		break;

	  case DATABASE_AD:
		SetTargetTypeName (queryAd, DATABASE_ADTYPE);
		break;

	  case TT_AD:
		SetTargetTypeName (queryAd, TT_ADTYPE);
		break;

      case GRID_AD:
        SetTargetTypeName (queryAd, GRID_ADTYPE);
        break;

	  case HAD_AD:
		SetTargetTypeName (queryAd, HAD_ADTYPE);
		break;

	  case ACCOUNTING_AD:
		SetTargetTypeName(queryAd, ACCOUNTING_ADTYPE);
		break;
	  default:
		return Q_INVALID_QUERY;
	}

	return Q_OK;
}

	
QueryResult CondorQuery::
filterAds (ClassAdList &in, ClassAdList &out)
{
	ClassAd queryAd, *candidate;
	QueryResult	result;

	// make the query ad
	result = getQueryAd (queryAd);
	if (result != Q_OK) return result;

	in.Open();
	while( (candidate = (ClassAd *) in.Next()) )
    {
        // if a match occurs
		if (IsAHalfMatch(&queryAd, candidate)) out.Insert (candidate);
    }
    in.Close ();
    
	return Q_OK;
}


const char *
getStrQueryResult(QueryResult q)
{
	switch (q)
	{
    	case Q_OK:					return "ok";
    	case Q_INVALID_CATEGORY:	return "invalid category";
    	case Q_MEMORY_ERROR:		return "memory error";
	    case Q_PARSE_ERROR:			return "invalid constraint";
	    case Q_COMMUNICATION_ERROR:	return "communication error";
	    case Q_INVALID_QUERY:		return "invalid query";
	    case Q_NO_COLLECTOR_HOST:	return "can't find collector";
		default:
			return "unknown error";
	}
}

void
CondorQuery::setDesiredAttrs(char const * const *attrs)
{
	std::string val;
	::join_args(attrs,val);
	setDesiredAttrs(val.c_str());
}

void
CondorQuery::setDesiredAttrs(const classad::References &attrs)
{
	std::string str;
	str.reserve(attrs.size()*30); // make a guess at total string space needed.
	for (classad::References::const_iterator it = attrs.begin(); it != attrs.end(); ++it) {
		if ( ! str.empty()) str += " ";
		str += *it;
	}
	setDesiredAttrs(str.c_str());
}

void
CondorQuery::setDesiredAttrs(const std::vector<std::string> &attrs)
{
	std::string str;
	str.reserve(attrs.size()*30); // make a guess at total string space needed.
	::join(attrs, " ", str);
	setDesiredAttrs(str.c_str());
}

void
CondorQuery::setDesiredAttrsExpr(char const *expr)
{
	extraAttrs.AssignExpr(ATTR_PROJECTION,expr);
}
