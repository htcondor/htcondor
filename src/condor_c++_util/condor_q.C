/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#include "condor_common.h"
#include "condor_debug.h"
#include "condor_q.h"
#include "condor_attributes.h"
#include "condor_adtypes.h"
#include "condor_qmgr.h"
#include "format_time.h"

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
addOR (char *value)
{
	return query.addCustomOR (value);
}

int CondorQ::
addAND (char *value)
{
	return query.addCustomAND (value);
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
		if (!(qmgr = ConnectQ (0,20,true)))
			return Q_SCHEDD_COMMUNICATION_ERROR;
	}
	else
	{
		// remote case to handle condor_globalq
		if (!ad->LookupString (ATTR_SCHEDD_IP_ADDR, scheddString))
			return Q_NO_SCHEDD_IP_ADDR;

		if (!(qmgr = ConnectQ (scheddString,20,true)))
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

	/*
	 connect to the Q manager.
	 use a timeout of 20 seconds, and a read-only connection.
	 why 20 seconds?  because careful research by Derek has shown
	 that whenever one needs a periodic time value, 20 is always
	 optimal.  :^).
	*/
	if (!(qmgr = ConnectQ (host,20,true)))
		return Q_SCHEDD_COMMUNICATION_ERROR;

	// get the ads and filter them
	result = getAndFilterAds (filterAd, list);

	DisconnectQ (qmgr);
	return result;
}

int CondorQ::
fetchQueueFromHostAndProcess ( char *host, process_function process_func )
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

	/*
	 connect to the Q manager.
	 use a timeout of 20 seconds, and a read-only connection.
	 why 20 seconds?  because careful research by Derek has shown
	 that whenever one needs a periodic time value, 20 is always
	 optimal.  :^).
	*/
	if (!(qmgr = ConnectQ (host,20,true)))
		return Q_SCHEDD_COMMUNICATION_ERROR;

	// get the ads and filter them
	result = getFilterAndProcessAds (filterAd, process_func);

	DisconnectQ (qmgr);
	return result;
}

int CondorQ::
getFilterAndProcessAds( ClassAd &queryad, process_function process_func )
{
	char		constraint[ATTRLIST_MAX_EXPRESSION]; /* yuk! */ 
	ExprTree	*tree;
	ClassAd		*ad;

	constraint[0] = '\0';
	tree = queryad.Lookup(ATTR_REQUIREMENTS);
	if (!tree) {
		return Q_INVALID_QUERY;
	}

	tree->RArg()->PrintToStr(constraint);

	if ((ad = GetNextJobByConstraint(constraint, 1)) != NULL) {
		// Process the data and insert it into the list
		if ( ( *process_func )( ad ) ) {
			delete(ad);
		}

		while((ad = GetNextJobByConstraint(constraint, 0)) != NULL) {
			// Process the data and insert it into the list
			if ( ( *process_func )( ad ) ) {
				delete(ad);
			}
		}
	}

	// here GetNextJobByConstraint returned NULL.  check if it was
	// because of the network or not.  if qmgmt had a problem with
	// the net, then errno is set to ETIMEDOUT, and we should fail.
	if ( errno == ETIMEDOUT ) {
		return Q_SCHEDD_COMMUNICATION_ERROR;
	}

	return Q_OK;
}


int CondorQ::
getAndFilterAds (ClassAd &queryad, ClassAdList &list)
{
	char		constraint[ATTRLIST_MAX_EXPRESSION]; /* yuk! */
	ExprTree	*tree;
	ClassAd		*ad;

	constraint[0] = '\0';
	tree = queryad.Lookup(ATTR_REQUIREMENTS);
	if (!tree) {
		return Q_INVALID_QUERY;
	}
	tree->RArg()->PrintToStr(constraint);

	if ((ad = GetNextJobByConstraint(constraint, 1)) != NULL) {
		list.Insert(ad);
		while((ad = GetNextJobByConstraint(constraint, 0)) != NULL) {
			list.Insert(ad);
		}
	}

	// here GetNextJobByConstraint returned NULL.  check if it was
	// because of the network or not.  if qmgmt had a problem with
	// the net, then errno is set to ETIMEDOUT, and we should fail.
	if ( errno == ETIMEDOUT ) {
		return Q_SCHEDD_COMMUNICATION_ERROR;
	}

	return Q_OK;
}


int JobSort(ClassAd *job1, ClassAd *job2, void *data)
{
	int cluster1=0, cluster2=0, proc1=0, proc2=0;

	job1->LookupInteger(ATTR_CLUSTER_ID, cluster1);
	job2->LookupInteger(ATTR_CLUSTER_ID, cluster2);
	if (cluster1 < cluster2) return 1;
	if (cluster1 > cluster2) return 0;
	job1->LookupInteger(ATTR_PROC_ID, proc1);
	job2->LookupInteger(ATTR_PROC_ID, proc2);
	if (proc1 < proc2) return 1;
	else return 0;
}

/*
  Encode a status from a PROC structure as a single letter suited for
  printing.
*/
const char
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
	  case HELD:
		return 'H';
	  case SUBMISSION_ERR:
		return 'E';
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
	printf( "%4d.%-3d %-14s %-11s %-12s %-2c %-3d %-4.1f %-18.18s\n",
		cluster,
		proc,
		owner,
		format_date((time_t)date),
		format_time(time),
		encode_status(status),
		prio,
		image_size/1024.0,
		cmd
	);
}
