#include "condor_common.h"
#include <iostream.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>

#include "condor_query.h"
#include "condor_attributes.h"
#include "condor_collector.h"
#include "condor_config.h"
#include "condor_network.h"
#include "reli_sock.h"
#include "condor_parser.h"
#include "condor_adtypes.h"


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

	  case SCHEDD_AD:
		query.setNumStringCats (SCHEDD_STRING_THRESHOLD);
		query.setNumIntegerCats(SCHEDD_INT_THRESHOLD);
		query.setNumFloatCats  (SCHEDD_FLOAT_THRESHOLD);
		query.setIntegerKwList ((char **)ScheddIntegerKeywords);
		query.setStringKwList  ((char **)ScheddStringKeywords);
		query.setFloatKwList   ((char **)ScheddFloatKeywords);
		command = QUERY_SCHEDD_ADS;
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
clearCustomConstraints (void)
{
	query.clearCustom ();
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
addConstraint (const char *value)
{
	return (QueryResult) query.addCustom ((char *) value);
}


// fetch all ads from the collector that satisfy the constraints
QueryResult CondorQuery::
fetchAds (ClassAdList &adList, const char *poolName)
{
    char        *pool;
    ReliSock    sock; 
	int			more;
    QueryResult result;
    ClassAd     queryAd, *ad;

	// use current pool's collector if not specified
	if (poolName == NULL || poolName[0] == '\0')
	{
		if ((pool = param ("COLLECTOR_HOST")) == NULL)  {
			return Q_NO_COLLECTOR_HOST;
		}
	}
	else {
		// pool specified
		pool = (char *) poolName;
	}

	// make the query ad
	result = (QueryResult) query.makeQuery (queryAd);
	if (result != Q_OK) return result;

	// fix types
	queryAd.SetMyTypeName (QUERY_ADTYPE);
	switch (queryType) {
	  case STARTD_AD:
		queryAd.SetTargetTypeName (STARTD_ADTYPE);
		break;

	  case SCHEDD_AD:
		queryAd.SetTargetTypeName (SCHEDD_ADTYPE);
		break;

	  case MASTER_AD:
		queryAd.SetTargetTypeName (MASTER_ADTYPE);
		break;

	  case CKPT_SRVR_AD:
		queryAd.SetTargetTypeName (CKPT_SRVR_ADTYPE);
		break;

	  default:
		return Q_INVALID_QUERY;
	}

	// contact collector
	if (!sock.connect(pool, COLLECTOR_COMM_PORT)) {
        return Q_COMMUNICATION_ERROR;
    }

	// ship query
	sock.encode();
	if (!sock.code (command) || !queryAd.put (sock) || !sock.eom()) {
		return Q_COMMUNICATION_ERROR;
	}
	
	// get result
	sock.decode ();
	more = 1;
	while (more)
	{
		if (!sock.code (more)) {
			return Q_COMMUNICATION_ERROR;
		}
		if (more) {
			ad = new ClassAd;
			if (!ad->get (sock)) {
				return Q_COMMUNICATION_ERROR;
			}
			adList.Insert (ad);
		}
	}

	// finalize
	sock.close ();
	
	return (Q_OK);
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
	while (candidate = (ClassAd *) in.Next())
    {
        // if a match occurs
		if ((*candidate) >= (queryAd)) out.Insert (candidate);
    }
    in.Close ();
    
	return Q_OK;
}

