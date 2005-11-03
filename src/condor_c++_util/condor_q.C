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
#include "condor_config.h"
#include "CondorError.h"
#include "condor_classad_util.h"

#if WANT_QUILL

#include "jobqueuesnapshot.h"

static ClassAd* getDBNextJobByConstraint(const char* constraint, JobQueueSnapshot  *jqSnapshot);
static int execQuery(PGconn *connection, const char* sql, PGresult*& result);

#endif /* WANT_QUILL */

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

	clusterprocarraysize = 128;
	clusterarray = (int *) malloc(clusterprocarraysize * sizeof(int));
	procarray = (int *) malloc(clusterprocarraysize * sizeof(int));
	int i;
	for(i=0; i < clusterprocarraysize; i++) { 
		clusterarray[i] = -1;
		procarray[i] = -1;
	}
	numclusters = 0;
	numprocs = 0;

	owner[0] = '\0';
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
addDBConstraint (CondorQIntCategories cat, int value) 
{
	int i;

		// remember the cluster and proc values so that they can be pushed down to DB query
	switch (cat) {
	case CQ_CLUSTER_ID:
		clusterarray[numclusters] = value;
		numclusters++;
		if(numclusters == clusterprocarraysize-1) {
		   clusterarray = (int *) realloc(clusterarray, 
					clusterprocarraysize * 2);
		   procarray = (int *) realloc(procarray, 
					clusterprocarraysize * 2);
		   for(i=clusterprocarraysize; 
				i < clusterprocarraysize * 2; i++) {
		      clusterarray[i] = -1;
		      procarray[i] = -1;
		   }
		   clusterprocarraysize *= 2;
		} 
		break;
	case CQ_PROC_ID:
		// we want to store the procs at the same index as its 
		// corresponding cluster so we simply use the numclusters 
		// value.  numclusters is already incremented above as the 
		// cluster value appears before proc in the user's 
		// constraint string, so we store it at numclusters-1
		procarray[numclusters-1] = value;
		numprocs++;
		break;
	default:
		break;
	}
	return 1;
}

int CondorQ::
add (CondorQIntCategories cat, int value)
{
	return query.addInteger (cat, value);
}


int CondorQ::
add (CondorQStrCategories cat, char *value)
{  
	switch (cat) {
	case CQ_OWNER:
		strncpy(owner, value, MAXOWNERLEN);
		break;
	default:
		break;
	}

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
fetchQueue (ClassAdList &list, ClassAd *ad, CondorError* errstack)
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
		if (!ad->LookupString (ATTR_SCHEDD_IP_ADDR, scheddString))
			return Q_NO_SCHEDD_IP_ADDR;

		if( !(qmgr = ConnectQ( scheddString, connect_timeout, true, errstack)) )
			return Q_SCHEDD_COMMUNICATION_ERROR;
	}

	// get the ads and filter them
	getAndFilterAds (filterAd, list);

	DisconnectQ (qmgr);
	return Q_OK;
}

int CondorQ::
fetchQueueFromHost (ClassAdList &list, char *host, CondorError* errstack)
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
	init();  // needed to get default connect_timeout
	if( !(qmgr = ConnectQ( host, connect_timeout, true, errstack)) )
		return Q_SCHEDD_COMMUNICATION_ERROR;

	// get the ads and filter them
	result = getAndFilterAds (filterAd, list);

	DisconnectQ (qmgr);
	return result;
}

int CondorQ::
fetchQueueFromDB (ClassAdList &list, char *dbconn, CondorError* errstack)
{
#if WANT_QUILL
	ClassAd 		filterAd;
	int     		result;
	JobQueueSnapshot	*jqSnapshot;
	char            constraint[ATTRLIST_MAX_EXPRESSION];
	ClassAd        *ad;
	QuillErrCode   rv;

	jqSnapshot = new JobQueueSnapshot(dbconn);

	rv = jqSnapshot->startIterateAllClassAds(clusterarray,
						 numclusters,
						 procarray,
						 numprocs,
						 owner,
						 FALSE);

	if (rv == FAILURE) {
		delete jqSnapshot;
		return Q_COMMUNICATION_ERROR;
	} else if (rv == JOB_QUEUE_EMPTY) {
		delete jqSnapshot;
		return Q_OK;
	}

	// make the query ad
	if ((result = query.makeQuery (filterAd)) != Q_OK) {
		delete jqSnapshot;
		return result;
	}

	// insert types into the query ad   ###
	filterAd.SetMyTypeName ("Query");
	filterAd.SetTargetTypeName ("Job");

	ExprTree *tree;
	tree = filterAd.Lookup(ATTR_REQUIREMENTS);
	if (!tree) {
		delete jqSnapshot;
	  return Q_INVALID_QUERY;
	}

	constraint[0] = '\0';
	tree->RArg()->PrintToStr(constraint);

	ad = getDBNextJobByConstraint(constraint, jqSnapshot);

	while (ad != (ClassAd *) 0) {
		list.Insert(ad);
		ad = getDBNextJobByConstraint(constraint, jqSnapshot);
	}	

	delete jqSnapshot;
#endif /* WANT_QUILL */
	return Q_OK;
}

int CondorQ::
fetchQueueFromHostAndProcess ( char *host, process_function process_func, CondorError* errstack )
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
	init();  // needed to get default connect_timeout
	if( !(qmgr = ConnectQ( host, connect_timeout, true, errstack)) )
		return Q_SCHEDD_COMMUNICATION_ERROR;

	// get the ads and filter them
	result = getFilterAndProcessAds (filterAd, process_func);

	DisconnectQ (qmgr);
	return result;
}

int CondorQ::
fetchQueueFromDBAndProcess ( char *dbconn, process_function process_func, CondorError* errstack )
{
#if WANT_QUILL
	ClassAd 		filterAd;
	int     		result;
	JobQueueSnapshot	*jqSnapshot;
	char            constraint[ATTRLIST_MAX_EXPRESSION] = "";
	ClassAd        *ad;
	QuillErrCode             rv;

	ASSERT(process_func);

	jqSnapshot = new JobQueueSnapshot(dbconn);

	rv = jqSnapshot->startIterateAllClassAds(clusterarray,
						 numclusters,
						 procarray,
						 numprocs,
						 owner,
						 FALSE);

	if (rv == FAILURE) {
		delete jqSnapshot;
		return Q_COMMUNICATION_ERROR;
	}
	else if (rv == JOB_QUEUE_EMPTY) {
		delete jqSnapshot;
		return Q_OK;
	}	

	// make the query ad
	if ((result = query.makeQuery (filterAd)) != Q_OK) {
		delete jqSnapshot;
		return result;
	}

	// insert types into the query ad   ###
	filterAd.SetMyTypeName ("Query");
	filterAd.SetTargetTypeName ("Job");

	ExprTree *tree;
	tree = filterAd.Lookup(ATTR_REQUIREMENTS);
	if (!tree) {
		delete jqSnapshot;
	  return Q_INVALID_QUERY;
	}

	constraint[0] = '\0';
	tree->RArg()->PrintToStr(constraint);

	ad = getDBNextJobByConstraint(constraint, jqSnapshot);
	
	while (ad != (ClassAd *) 0) {
			// Process the data and insert it into the list
		if ((*process_func) (ad) ) {
			ad->clear();
			delete ad;
		}
		
		ad = getDBNextJobByConstraint(constraint, jqSnapshot);
	}	

	if(ad) {
		ad->clear();
		delete ad;
	}
	delete jqSnapshot;
#endif /* WANT_QUILL */

	return Q_OK;
}

void CondorQ::rawDBQuery(char *dbconn, CondorQQueryType qType) {
#if WANT_QUILL

	PGconn        *connection;
	char          *rowvalue;
	PGresult	  *resultset;
	int           ntuples;
	SQLQuery      sqlquery;

	if ((connection = PQconnectdb(dbconn)) == NULL)
	{
		fprintf(stderr, "\n-- Failed to connect to the database\n");
		return;
	}

	switch (qType) {
	case AVG_TIME_IN_QUEUE:

		sqlquery.setQuery(QUEUE_AVG_TIME, NULL);

	   	ntuples = execQuery(connection, sqlquery.getQuery(), resultset);

			/* we expect exact one row out of the query */
		if (ntuples != 1) {
			fprintf(stderr, "\n-- Failed to execute the query\n");
			return;
		}
		
		rowvalue = PQgetvalue(resultset, 0, 0);
		if(strcmp(rowvalue,"") == 0) {
			printf("\nJob queue is curently empty\n");
		} else {
			printf("\nAverage time in queue for uncompleted jobs (in hh:mm:ss)\n");
			printf("%s\n", rowvalue);		 
		}
		
		break;
	default:
		fprintf(stderr, "Error: type of query not supported\n");
		return;
		break;
	}

	if(resultset) {
		PQclear(resultset);
	}
	if(connection) {
		PQfinish(connection);
	}
#endif /* WANT_QUILL */
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

#if WANT_QUILL

ClassAd* getDBNextJobByConstraint(const char* constraint, JobQueueSnapshot	*jqSnapshot)
{
	ClassAd *ad;
	
	while(jqSnapshot->iterateAllClassAds(ad) != DONE_JOBS_CURSOR) {
		if ((!constraint || !constraint[0] || EvalBool(ad, constraint))) {
			return ad;		      
		}
		
		if (ad != (ClassAd *) 0) {
			ad->clear();
			delete ad;
			ad = (ClassAd *) 0;
		}
	}

	return (ClassAd *) 0;
}

/* When the query is successfully executed, result is set and return value
   is >= 0, otherwise return value is -1
*/
static int execQuery(PGconn *connection, const char* sql, PGresult*& result)
{
	if ((result = PQexec(connection, sql)) == NULL) {
		return -1; 
	}
	
	else if (PQresultStatus(result) != PGRES_TUPLES_OK) {
		PQclear(result);
		result = NULL;
		return -1;
	}

	return PQntuples(result);			
}

#endif /* WANT_QUILL */
