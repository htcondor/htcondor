#include "condor_common.h"
#include "condor_debug.h"
#include "condor_q.h"
#include "condor_attributes.h"
#include "condor_adtypes.h"
#include "condor_qmgr.h"

// specify keyword lists; N.B.  The order should follow from the category
// enumerations in the .h file
static const char *intKeywords[] =
{
	ATTR_CLUSTER_ID,
	ATTR_PROC_ID,
	ATTR_JOB_STATUS,
	ATTR_JOB_UNIVERSE
};


static const char *strKeywords[] =
{
	ATTR_OWNER
};


static const char *fltKeywords[] = 
{
	""	// add one string to avoid compiler error
};

// scan function to walk the job queue
extern "C" getJobAdAndInsert (int, int);
int getJobAd(int cluster_id, int proc_id, ClassAd *new_ad);

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
		if (!ad->LookupString (ATTR_SCHEDD_IP_ADDR, scheddString))
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

	WalkJobQueue (getJobAdAndInsert);

	return Q_OK;
}


getJobAdAndInsert(int cluster_id, int proc_id)
{
	int rval;
    ClassAd *new_ad = new ClassAd;

	rval = getJobAd(cluster_id, proc_id, new_ad);

	if (rval != 0) {
		delete new_ad;
		return rval;
	}

	// iff the job ad matches the query, insert it into the list
	if ((*new_ad) >= (*__query))
		__list->Insert (new_ad);
	else
		delete new_ad;

    return 0;
}

getJobAd(int cluster_id, int proc_id, ClassAd *new_ad)
{
    char    buf[1000];
    char    buf2[1000];
    int     rval;

	// this code is largely Jim Pruyne's original qmgmt_test.c program  --RR
    rval = FirstAttribute(cluster_id, proc_id, buf);
    while( rval >= 0) {
        rval = GetAttributeExpr(cluster_id, proc_id, buf, buf2);
        if (rval >= 0) {
			if (!new_ad->Insert (buf2))
			{
				// failed to insert expr --- abort this ad
				return 1;
			}
            rval = NextAttribute(cluster_id, proc_id, buf);
        }
    }

	// insert types for the ad  ### Use from some canonical file?  --RR
	new_ad->SetMyTypeName (JOB_ADTYPE);
	new_ad->SetTargetTypeName (STARTD_ADTYPE);

	return 0;
}

/* queue printing routines moved here from proc_obj.C, which is being
   taken out of cplus_lib.a.  -Jim B. */

/*
  Format a date expressed in "UNIX time" into "month/day hour:minute".
*/
static char *
format_date( time_t date )
{
	static char	buf[ 12 ];
	struct tm	*tm;

	tm = localtime( &date );
	sprintf( buf, "%2d/%-2d %02d:%02d",
		(tm->tm_mon)+1, tm->tm_mday, tm->tm_hour, tm->tm_min
	);
	return buf;
}

/*
  Format a time value which is encoded as seconds since the UNIX
  "epoch".  We return a string in the format dd+hh:mm:ss, indicating
  days, hours, minutes, and seconds.  The string is in static data
  space, and will be overwritten by the next call to this function.
*/
static char	*
format_time( int tot_secs )
{
	int		days;
	int		hours;
	int		min;
	int		secs;
	static char	answer[25];

	days = tot_secs / DAY;
	tot_secs %= DAY;
	hours = tot_secs / HOUR;
	tot_secs %= HOUR;
	min = tot_secs / MINUTE;
	secs = tot_secs % MINUTE;

	(void)sprintf( answer, "%3d+%02d:%02d:%02d", days, hours, min, secs );
	return answer;
}

/*
  Encode a status from a PROC structure as a single letter suited for
  printing.
*/
static char
encode_status( int status )
{
	switch( status ) {
	  case UNEXPANDED:
		return 'U';
	  case IDLE:
		return 'I';
	  case RUNNING:
		return 'R';
	  case COMPLETED:
		return 'C';
	  case REMOVED:
		return 'X';
	  default:
		return ' ';
	}
}

/*
  Print a line of data for the "short" display of a PROC structure.  The
  "short" display is the one used by "condor_q".  N.B. the columns used
  by this routine must match those defined by the short_header routine
  defined above.
*/
void
short_print(
	int cluster,
	int proc,
	const char *owner,
	int date,
	int time,
	int status,
	int prio,
	int image_size,
	const char *cmd
	) {
	printf( "%4d.%-3d %-14s %-11s %-12s %-2c %-3d %-4.1f %-18s\n",
		cluster,
		proc,
		owner,
		format_date(date),
		format_time(time),
		encode_status(status),
		prio,
		image_size/1024.0,
		cmd
	);
}

