/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
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

// normal ctor
CondorQuery::
CondorQuery (AdTypes qType)
{
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

          case STORAGE_AD:
                query.setNumStringCats (0);
                query.setNumIntegerCats(0);
                query.setNumFloatCats  (0);
                command = QUERY_STORAGE_ADS;
                break;

          case ANY_AD:
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
CondorQuery (const CondorQuery &from)
{
}


// dtor
CondorQuery::
~CondorQuery ()
{	
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
	return (QueryResult) query.addString (cat, (char *) value);
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
	return (QueryResult) query.addCustomAND ((char *) value);
}


// add a custom constraint
QueryResult CondorQuery::
addORConstraint (const char *value)
{
	return (QueryResult) query.addCustomOR ((char *) value);
}


QueryResult CondorQuery::
fetchAds (ClassAdList &adList, DaemonList * daemon_list, CondorError* errstack) {
	if (!daemon_list) {
		return Q_NO_COLLECTOR_HOST;
	}

	if (daemon_list->IsEmpty())
		return Q_NO_COLLECTOR_HOST;

	Daemon * daemon;
	QueryResult result;

		// Try different daemons in the list
	daemon_list->rewind();
	while (daemon_list->next(daemon)) {
		if ((!daemon) || (!daemon->locate())) {
			dprintf (D_ALWAYS, "Unable to locate daemon %s\n", daemon->name());
			continue;
		}

		result  = fetchAds (
						   adList,
						   daemon->addr(),
						   errstack);

		if (result == Q_OK) {
			return result;
		}
	}

		// Return the last result
	return result; 
}

// fetch all ads from the collector that satisfy the constraints
QueryResult CondorQuery::
fetchAds (ClassAdList &adList, const char *poolName, CondorError* errstack)
{
	Sock*    sock; 
	int                     more;
	QueryResult result;
	ClassAd     queryAd, *ad;

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

	  case STORAGE_AD:
		queryAd.SetTargetTypeName (STORAGE_ADTYPE);
		break;

	  case ANY_AD:
		queryAd.SetTargetTypeName (ANY_ADTYPE);
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

	if (!(sock = my_collector.startCommand(command, Stream::reli_sock, 0, errstack)) ||
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
		if ((*candidate) >= (queryAd)) out.Insert (candidate);
    }
    in.Close ();
    
	return Q_OK;
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
