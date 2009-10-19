/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
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
#include "condor_collector.h"
#include "condor_config.h"
#include "condor_network.h"
#include "condor_io.h"
#include "condor_parser.h"
#include "condor_adtypes.h"
#include "condor_debug.h"
#include "condor_classad_util.h"
#include "internet.h"
#include "daemon.h"
#include "dc_collector.h"

#define XDR_ASSERT(x) {if (!(x)) return Q_COMMUNICATION_ERROR;}

char *new_strdup (const char *);

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
	queryType = qType;
	switch (qType)
	{
	  case STARTD_AD:
		query.setNumStringCats (STARTD_STRING_THRESHOLD);
		query.setNumIntegerCats(STARTD_INT_THRESHOLD);
		query.setNumFloatCats  (STARTD_FLOAT_THRESHOLD);
		query.setIntegerKwList ((char **)StartdIntegerKeywords);
		query.setStringKwList  ((char **)StartdStringKeywords);
		query.setFloatKwList   ((char **)StartdFloatKeywords);
		command = QUERY_STARTD_ADS;
		break;

	  case STARTD_PVT_AD:
		query.setNumStringCats (STARTD_STRING_THRESHOLD);
		query.setNumIntegerCats(STARTD_INT_THRESHOLD);
		query.setNumFloatCats  (STARTD_FLOAT_THRESHOLD);
		query.setIntegerKwList ((char **)StartdIntegerKeywords);
		query.setStringKwList  ((char **)StartdStringKeywords);
		query.setFloatKwList   ((char **)StartdFloatKeywords);
		command = QUERY_STARTD_PVT_ADS;
		break;

#ifdef WANT_QUILL
	  case QUILL_AD:
		query.setNumStringCats (0);
		query.setNumIntegerCats(0);
		query.setNumFloatCats  (0);
		command = QUERY_QUILL_ADS;
		break;
#endif /* WANT_QUILL */

	  case SCHEDD_AD:
		query.setNumStringCats (SCHEDD_STRING_THRESHOLD);
		query.setNumIntegerCats(SCHEDD_INT_THRESHOLD);
		query.setNumFloatCats  (SCHEDD_FLOAT_THRESHOLD);
		query.setIntegerKwList ((char **)ScheddIntegerKeywords);
		query.setStringKwList  ((char **)ScheddStringKeywords);
		query.setFloatKwList   ((char **)ScheddFloatKeywords);
		command = QUERY_SCHEDD_ADS;
		break;

	  case SUBMITTOR_AD:
		query.setNumStringCats (SCHEDD_STRING_THRESHOLD);
		query.setNumIntegerCats(SCHEDD_INT_THRESHOLD);
		query.setNumFloatCats  (SCHEDD_FLOAT_THRESHOLD);
		query.setIntegerKwList ((char **)ScheddIntegerKeywords);
		query.setStringKwList  ((char **)ScheddStringKeywords);
		query.setFloatKwList   ((char **)ScheddFloatKeywords);
		command = QUERY_SUBMITTOR_ADS;
		break;

      case GRID_AD:
        query.setNumStringCats (GRID_STRING_THRESHOLD);
		query.setNumIntegerCats(GRID_INT_THRESHOLD);
		query.setNumFloatCats  (GRID_FLOAT_THRESHOLD);
		query.setIntegerKwList ((char **)GridManagerIntegerKeywords);
		query.setStringKwList  ((char **)GridManagerStringKeywords);
		query.setFloatKwList   ((char **)GridManagerFloatKeywords);
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

	  case XFER_SERVICE_AD:
		query.setNumStringCats (0);
		query.setNumIntegerCats(0);
		query.setNumFloatCats  (0);
		command = QUERY_XFER_SERVICE_ADS;
		break;

	  case LEASE_MANAGER_AD:
		query.setNumStringCats (0);
		query.setNumIntegerCats(0);
		query.setNumFloatCats  (0);
		command = QUERY_LEASE_MANAGER_ADS;
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

	  case DBMSD_AD:
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


// fetch all ads from the collector that satisfy the constraints
QueryResult CondorQuery::
fetchAds (ClassAdList &adList, const char *poolName, CondorError* errstack)
{
	Sock*    sock; 
	int                     more;
	QueryResult result;
	ClassAd     queryAd(extraAttrs), *ad;

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
	result = (QueryResult) query.makeQuery (queryAd);
	if (result != Q_OK) return result;

	// fix types
	queryAd.SetMyTypeName (QUERY_ADTYPE);
	switch (queryType) {
#ifdef WANT_QUILL
	  case QUILL_AD:
		queryAd.SetTargetTypeName (QUILL_ADTYPE);
		break;
#endif /* WANT_QUILL */

	  case STARTD_AD:
	  case STARTD_PVT_AD:
		queryAd.SetTargetTypeName (STARTD_ADTYPE);
		break;

	  case SCHEDD_AD:
	  case SUBMITTOR_AD:
		// CRUFT: Before 7.3.2, submitter ads had a MyType of
		//   "Scheduler". The only way to tell the difference
		//   was that submitter ads didn't have ATTR_NUM_USERS.
		//   Newer collectors will coerce the TargetType to
		//   "Submitter" for queries of submitter ads.
		queryAd.SetTargetTypeName (SCHEDD_ADTYPE);
		break;

	  case LICENSE_AD:
		queryAd.SetTargetTypeName (LICENSE_ADTYPE);
		break;

	  case MASTER_AD:
		queryAd.SetTargetTypeName (MASTER_ADTYPE);
		break;

	  case CKPT_SRVR_AD:
		queryAd.SetTargetTypeName (CKPT_SRVR_ADTYPE);
		break;

	  case COLLECTOR_AD:
		queryAd.SetTargetTypeName (COLLECTOR_ADTYPE);
		break;

	  case NEGOTIATOR_AD:
		queryAd.SetTargetTypeName (NEGOTIATOR_ADTYPE);
		break;

	  case STORAGE_AD:
		queryAd.SetTargetTypeName (STORAGE_ADTYPE);
		break;

	  case CREDD_AD:
		queryAd.SetTargetTypeName (CREDD_ADTYPE);
		break;

	  case GENERIC_AD:
		  // For now, at least, there is no separate QUERY_GENERIC_ADS
		  // command, so send the ANY_ADTYPE
		queryAd.SetTargetTypeName (GENERIC_ADTYPE);
		break;

	  case XFER_SERVICE_AD:
		queryAd.SetTargetTypeName (XFER_SERVICE_ADTYPE);
		break;

	  case LEASE_MANAGER_AD:
		queryAd.SetTargetTypeName (LEASE_MANAGER_ADTYPE);
		break;

	  case ANY_AD:
		queryAd.SetTargetTypeName (ANY_ADTYPE);
		break;

	  case DATABASE_AD:
		queryAd.SetTargetTypeName (DATABASE_ADTYPE);
		break;

	  case DBMSD_AD:
		queryAd.SetTargetTypeName (DBMSD_ADTYPE);
		break;

	  case TT_AD:
		queryAd.SetTargetTypeName (TT_ADTYPE);
		break;

      case GRID_AD:
        queryAd.SetTargetTypeName (GRID_ADTYPE);
        break;

	  case HAD_AD:
		queryAd.SetTargetTypeName (HAD_ADTYPE);
		break;

	  default:
		return Q_INVALID_QUERY;
	}

	if( DebugFlags & D_HOSTNAME ) {
		dprintf( D_HOSTNAME, "Querying collector %s (%s) with classad:\n", 
				 my_collector.addr(), my_collector.fullHostname() );
		queryAd.dPrint( D_HOSTNAME );
		dprintf( D_HOSTNAME, " --- End of Query ClassAd ---\n" );
	}


	int mytimeout = param_integer ("QUERY_TIMEOUT",60); 
	if (!(sock = my_collector.startCommand(command, Stream::reli_sock, mytimeout, errstack)) ||
	    !queryAd.put (*sock) || !sock->end_of_message()) {

		if (sock) {
			delete sock;
		}
		return Q_COMMUNICATION_ERROR;
	}
	
	// get result
	sock->decode ();
	more = 1;
	while (more)
	{
		if (!sock->code (more)) {
			sock->end_of_message();
			delete sock;
			return Q_COMMUNICATION_ERROR;
		}
		if (more) {
			ad = new ClassAd;
			if( !ad->initFromStream(*sock) ) {
				sock->end_of_message();
				delete ad;
				delete sock;
				return Q_COMMUNICATION_ERROR;
			}
			adList.Insert (ad);
		}
	}
	sock->end_of_message();

	// finalize
	sock->close();
	delete sock;
	
	return (Q_OK);
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

	result = (QueryResult) query.makeQuery (queryAd);
	if (result != Q_OK) return result;

	// fix types
	queryAd.SetMyTypeName (QUERY_ADTYPE);
	switch (queryType) {
	  case STARTD_AD:
	  case STARTD_PVT_AD:
		queryAd.SetTargetTypeName (STARTD_ADTYPE);
		break;

	  case SCHEDD_AD:
	  case SUBMITTOR_AD:
		queryAd.SetTargetTypeName (SCHEDD_ADTYPE);
		break;

	  case LICENSE_AD:
		queryAd.SetTargetTypeName (LICENSE_ADTYPE);
		break;

	  case MASTER_AD:
		queryAd.SetTargetTypeName (MASTER_ADTYPE);
		break;

	  case CKPT_SRVR_AD:
		queryAd.SetTargetTypeName (CKPT_SRVR_ADTYPE);
		break;

	  case COLLECTOR_AD:
		queryAd.SetTargetTypeName (COLLECTOR_ADTYPE);
		break;

	  case NEGOTIATOR_AD:
		queryAd.SetTargetTypeName (NEGOTIATOR_ADTYPE);
		break;

      case GRID_AD:
        queryAd.SetTargetTypeName (GRID_ADTYPE);
        break;

	  case GENERIC_AD:
		queryAd.SetTargetTypeName (GENERIC_ADTYPE);

	  case ANY_AD:
		queryAd.SetTargetTypeName (genericQueryType);

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
	result = (QueryResult) query.makeQuery (queryAd);
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

int 
CondorQuery::addExtraAttribute(const char *attr) {
	return extraAttrs.Insert(attr);
}


char *getStrQueryResult(QueryResult q)
{
	switch (q)
	{
    	case Q_OK:					return "ok";
    	case Q_INVALID_CATEGORY:	return "invalid category";
    	case Q_MEMORY_ERROR:		return "memory error";
    	case Q_PARSE_ERROR:			return "parse error";
	    case Q_COMMUNICATION_ERROR:	return "communication error";
	    case Q_INVALID_QUERY:		return "invalid query";
	    case Q_NO_COLLECTOR_HOST:	return "can't find collector";
		default:
			return "unknown error";
	}
}
