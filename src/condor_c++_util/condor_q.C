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
#include "condor_debug.h"
#include "condor_q.h"
#include "condor_attributes.h"
#include "condor_adtypes.h"
#include "condor_qmgr.h"
#include "format_time.h"
#include "condor_classad.h"
#include "condor_config.h"
#include "CondorError.h"

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
// 30-Dec-2001: These no longer seem needed--nothing refers to them, there
// doesn't seem to be a scan function.
//static ClassAdList *__list;
//static ClassAd     *__query;


CondorQ::
CondorQ ()
{
	connect_timeout = 20; 
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


bool CondorQ::
init()
{
	connect_timeout = param_integer( "Q_QUERY_TIMEOUT", connect_timeout );
	return true;
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
fetchQueue (ClassAdList &ca_list, ClassAd *ad, CondorError* errstack)
{
	Qmgr_connection *qmgr;
	ClassAd 		filterAd;
	int     		result;
	char    		scheddString [32];

	// make the query ad
	if ((result = query.makeQuery (filterAd)) != Q_OK)
		return result;

	// insert types into the query ad   ###
//	filterAd.SetMyTypeName ("Query");
//	filterAd.SetTargetTypeName ("Job");
	filterAd.InsertAttr( ATTR_MY_TYPE, (string)QUERY_ADTYPE );		// NAC
	filterAd.InsertAttr( ATTR_TARGET_TYPE, (string)JOB_ADTYPE ); 	// NAC	

	// connect to the Q manager
	init();  // needed to get default connect_timeout
	if (ad == 0)
	{
		// local case
		if( !(qmgr = ConnectQ( 0, connect_timeout, true, errstack)) ) {
			errstack->push("TEST", 0, "FOO");
			return Q_SCHEDD_COMMUNICATION_ERROR;
		}
	}
	else
	{
		// remote case to handle condor_globalq
//		if (!ad->LookupString (ATTR_SCHEDD_IP_ADDR, scheddString))
		if ( !ad->EvaluateAttrString( ATTR_SCHEDD_IP_ADDR, scheddString, 32 ) ) {	// NAC
			return Q_NO_SCHEDD_IP_ADDR;	
		}
		if( !(qmgr = ConnectQ( scheddString, connect_timeout, true, errstack)) ) {
			return Q_SCHEDD_COMMUNICATION_ERROR;
		}
	}

	// get the ads and filter them
	getAndFilterAds (filterAd, ca_list);

	DisconnectQ (qmgr);
	return Q_OK;
}

int CondorQ::
fetchQueueFromHost (ClassAdList &ca_list, char *host, CondorError* errstack)
{
	Qmgr_connection *qmgr;
	ClassAd 		filterAd;
	int     		result;

	// make the query ad
	if ((result = query.makeQuery (filterAd)) != Q_OK)
		return result;

	// insert types into the query ad   ###
//	filterAd.SetMyTypeName ("Query");
//	filterAd.SetTargetTypeName ("Job");
	filterAd.InsertAttr( ATTR_MY_TYPE, (string)QUERY_ADTYPE );		// NAC
	filterAd.InsertAttr( ATTR_TARGET_TYPE, (string)JOB_ADTYPE ); 	// NAC	


	/*
	 connect to the Q manager.
	 use a timeout of 20 seconds, and a read-only connection.
	 why 20 seconds?  because careful research by Derek has shown
	 that whenever one needs a periodic time value, 20 is always
	 optimal.  :^).
	*/
	init();  // needed to get default connect_timeout
	if( !(qmgr = ConnectQ( host, connect_timeout, true, errstack)) ) {
		return Q_SCHEDD_COMMUNICATION_ERROR;
	}
	// get the ads and filter them
	result = getAndFilterAds (filterAd, ca_list);

	DisconnectQ (qmgr);
	return result;
}

int CondorQ::
fetchQueueFromHostAndProcess ( char *host, process_function process_func, CondorError* errstack )
{
		// DEBUG NAC
//	printf("entered fetchQueueFromHostAndProcess\n");	// NAC

	Qmgr_connection *qmgr;
	ClassAd 		filterAd;
	int     		result;

	// make the query ad
	if ((result = query.makeQuery (filterAd)) != Q_OK)
		return result;

	// insert types into the query ad   ###
//	filterAd.SetMyTypeName ("Query");
//	filterAd.SetTargetTypeName ("Job");
	filterAd.InsertAttr( ATTR_MY_TYPE, (string)QUERY_ADTYPE );		// NAC
	filterAd.InsertAttr( ATTR_TARGET_TYPE, (string)JOB_ADTYPE ); 	// NAC	

	/*
	 connect to the Q manager.
	 use a timeout of 20 seconds, and a read-only connection.
	 why 20 seconds?  because careful research by Derek has shown
	 that whenever one needs a periodic time value, 20 is always
	 optimal.  :^).
	*/
	init();  // needed to get default connect_timeout
	if( !(qmgr = ConnectQ( host, connect_timeout, true, errstack)) ) {
		return Q_SCHEDD_COMMUNICATION_ERROR;
	}

	// get the ads and filter them
	result = getFilterAndProcessAds (filterAd, process_func);
	result = Q_OK; // NAC

	DisconnectQ (qmgr);

		// DEBUG NAC
//	printf("exiting fetchQueueFromHostAndProcess\n");	// NAC
	return result;
}

int CondorQ::
getFilterAndProcessAds( ClassAd &queryad, process_function process_func )
{
		// DEBUG NAC
//	printf("entered getFilterAndProcessAds\n");	// NAC

//	char		constraint[ATTRLIST_MAX_EXPRESSION]; /* yuk! */ 
	char		constraint[10240];	// NAC
	string		constraint_string; 	// NAC
	ExprTree	*tree;
	ClassAd		*ad;
	ClassAdUnParser	unp;			// NAC

//	constraint[0] = '\0';
	tree = queryad.Lookup(ATTR_REQUIREMENTS);
//	if (!tree) {
//		return Q_INVALID_QUERY;
//	}
//	tree->RArg()->PrintToStr(constraint);
	unp.Unparse( constraint_string, tree );  // NAC

		// DEBUG NAC
//	cout << "constraints_string = " << constraint_string << endl;	// NAC

	strncpy( constraint, constraint_string.c_str( ), 10240 ); // NAC

		// DEBUG NAC
//	cout << "constraint = " << constraint << endl;  // NAC


	if ((ad = GetNextJobByConstraint(constraint, 1)) != NULL) {
		// Process the data and insert it into the list
		if ( ( *process_func )( ad ) ) {
			delete(ad);
		}

		while((ad = GetNextJobByConstraint(constraint, 0)) != NULL) {
			// Process the data and insert it into the list

				// DEBUG NAC
//			cout << "about to call process_func" << endl;
			if ( ( *process_func )( ad ) ) {
				delete(ad);
			}
//			cout << "done process_func" << endl;
		}
	}

		// DEBUG NAC
//	cout << "done GetNextJobByConstraint" << endl;

	// here GetNextJobByConstraint returned NULL.  check if it was
	// because of the network or not.  if qmgmt had a problem with
	// the net, then errno is set to ETIMEDOUT, and we should fail.
	if ( errno == ETIMEDOUT ) {
		return Q_SCHEDD_COMMUNICATION_ERROR;
	}
		// DEBUG NAC
//	printf("exiting getFilterAndProcessAds\n");	// NAC

	return Q_OK;
}


int CondorQ::
getAndFilterAds (ClassAd &queryad, ClassAdList &ca_list)
{
//	char		constraint[ATTRLIST_MAX_EXPRESSION]; /* yuk! */
	char		constraint[10240];  // NAC
	string		constraint_string; 	// NAC
	ExprTree	*tree;
	ClassAd		*ad;
	ClassAdUnParser	unp;		// NAC

//	constraint[0] = '\0';
	tree = queryad.Lookup( ATTR_REQUIREMENTS );
//	if (!tree) {
//		return Q_INVALID_QUERY;
//	}
//	tree->RArg()->PrintToStr(constraint);
	unp.Unparse( constraint_string, tree );  // NAC
	strncpy( constraint, constraint_string.c_str( ), 10240 ); // NAC
	

	if ((ad = GetNextJobByConstraint(constraint, 1)) != NULL) {
		ca_list.Insert(ad);
		while((ad = GetNextJobByConstraint(constraint, 0)) != NULL) {
			ca_list.Insert(ad);
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

//	job1->LookupInteger(ATTR_CLUSTER_ID, cluster1);
//	job2->LookupInteger(ATTR_CLUSTER_ID, cluster2);
	job1->EvaluateAttrInt( ATTR_CLUSTER_ID, cluster1 );	// NAC
	job2->EvaluateAttrInt( ATTR_CLUSTER_ID, cluster2 );	// NAC
	if (cluster1 < cluster2) return 1;
	if (cluster1 > cluster2) return 0;
//	job1->LookupInteger(ATTR_PROC_ID, proc1);
//	job2->LookupInteger(ATTR_PROC_ID, proc2);
	job1->EvaluateAttrInt( ATTR_PROC_ID, proc1 );	// NAC
	job2->EvaluateAttrInt( ATTR_PROC_ID, proc2 );	// NAC
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
