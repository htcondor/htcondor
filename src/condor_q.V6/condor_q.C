#include "condor_common.h"
#include "condor_debug.h"
#include "condor_q.h"
#include "condor_attributes.h"
#include "condor_qmgr.h"

// specify keyword lists; N.B.  The order shoudl follow from the category
// enumerations in the .h file
static const char *intKeywords[] =
{
	ATTR_CLUSTER_ID,
	ATTR_PROC_ID,
	"Status",              // ### This should be ATTR_JOB_STATUS
	"Universe"             // ### This should be ATTR_JOB_UNIVERSE
};


static const char *strKeywords[] =
{
	ATTR_OWNER
};


static const char *fltKeywords[] = 
{
};

// scan function to walk the job queue
extern "C" getJobAd (int, int);

// need this global variable to hold information reqd by the scan function
static ClassAdList *__list;
static ClassAd     *__query;


CondorQ::
CondorQ ()
{
	query.setNumIntegerCats (CQ_INT_THRESHOLD);
	query.setNumStringCats (CQ_STR_THRESHOLD);
	query.setNumFloatCats (CQ_FLT_THRESHOLD);
	query.setIntegerKwList ((char **) intKeywords);
	query.setStringKwList ((char **) strKeywords);
	query.setFloatKwList ((char **) fltKeywords);
}


CondorQ::
~CondorQ ()
{
}


int CondorQ::
add (CondorQIntCategories cat, int value)
{
	return query.addInteger (cat, value);
}


int CondorQ::
add (CondorQStrCategories cat, char *value)
{  
	return query.addString (cat, value);
}


int CondorQ::
add (CondorQFltCategories cat, float value)
{
	return query.addFloat (cat, value);
}


int CondorQ::
add (char *value)
{
	return query.addCustom (value);
}

int CondorQ::
fetchQueue (ClassAdList &list, ClassAd *ad)
{
	Qmgr_connection *qmgr;
	ClassAd 		filterAd;
	int     		result;
	char    		scheddString [32];

	// make the query ad
	if ((result = query.makeQuery (filterAd)) != Q_OK)
		return result;

	// insert types into the query ad   ###
	filterAd.SetMyTypeName ("Query");
	filterAd.SetTargetTypeName ("Job");

	// connect to the Q manager
	if (ad == 0)
	{
		// local case
		if (!(qmgr = ConnectQ (0)))
			return Q_SCHEDD_COMMUNICATION_ERROR;
	}
	else
	{
		// remote case to handle condor_globalq
		if (!ad->LookupString ("SCHEDD_IP_ADDR", scheddString))
			return Q_NO_SCHEDD_IP_ADDR;

		if (!(qmgr = ConnectQ (scheddString)))
			return Q_SCHEDD_COMMUNICATION_ERROR;
	}

	// get the ads and filter them
	getAndFilterAds (filterAd, list);

	DisconnectQ (qmgr);
	return Q_OK;
}

int CondorQ::
fetchQueueFromHost (ClassAdList &list, char *host)
{
	Qmgr_connection *qmgr;
	ClassAd 		filterAd;
	int     		result;
	char    		scheddString [32];

	// make the query ad
	if ((result = query.makeQuery (filterAd)) != Q_OK)
		return result;

	// insert types into the query ad   ###
	filterAd.SetMyTypeName ("Query");
	filterAd.SetTargetTypeName ("Job");

	// connect to the Q manager
	if (!(qmgr = ConnectQ (host)))
		return Q_SCHEDD_COMMUNICATION_ERROR;

	// get the ads and filter them
	getAndFilterAds (filterAd, list);

	DisconnectQ (qmgr);
	return Q_OK;
}

int CondorQ::
getAndFilterAds (ClassAd &ad, ClassAdList &list)
{
	__list = &list;
	__query = &ad;

	WalkJobQueue (getJobAd);

	return Q_OK;
}


getJobAd(int cluster_id, int proc_id)
{
    char    buf[1000];
    char    buf2[1000];
    int     rval;
    ClassAd *new_ad = new ClassAd;

	// this code is largely Jim Pruyne's original qmgmt_test.c program  --RR
    rval = FirstAttribute(cluster_id, proc_id, buf);
    while( rval >= 0) {
        rval = GetAttributeExpr(cluster_id, proc_id, buf, buf2);
        if (rval >= 0) {
			if (!new_ad->Insert (buf2))
			{
				// failed to insert expr --- abort this ad
				delete new_ad;
				return 1;
			}
            rval = NextAttribute(cluster_id, proc_id, buf);
        }
    }

	// insert types for the ad  ### Use from some canonical file?  --RR
	new_ad->SetMyTypeName ("Job");
	new_ad->SetTargetTypeName ("Machine");

	// iff the job ad matches the query, insert it into the list
	if ((*new_ad) >= (*__query))
		__list->Insert (new_ad);
	else
		delete new_ad;

    return 0;
}


