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
#include "condor_debug.h"
#include "condor_q.h"
#include "condor_attributes.h"
#include "condor_adtypes.h"
#include "condor_qmgr.h"
#include "format_time.h"
#include "condor_config.h"
#include "CondorError.h"
#include "condor_classad_util.h"
#include "quill_enums.h"

#if HAVE_ORACLE
#undef ATTR_VERSION
#include "oracledatabase.h"
#endif

#ifdef WANT_QUILL
#include "pgsqldatabase.h"
#include "jobqueuesnapshot.h"

static ClassAd* getDBNextJobByConstraint(const char* constraint, JobQueueSnapshot  *jqSnapshot);

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
	schedd[0] = '\0';
	scheddBirthdate = 0;
}


CondorQ::
~CondorQ ()
{
	free(clusterarray);
	free(procarray);
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
					clusterprocarraysize * 2 * sizeof(int));
		   procarray = (int *) realloc(procarray, 
					clusterprocarraysize * 2 * sizeof(int));
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
		strncpy(owner, value, MAXOWNERLEN - 1);
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
addSchedd (char *value)
{  
	strncpy(schedd, value, MAXSCHEDDLEN - 1);
	return 0;
}

int CondorQ::
addScheddBirthdate (time_t value)
{  
	scheddBirthdate = value;
	return 0;
}


int CondorQ::
fetchQueue (ClassAdList &list, StringList &attrs, ClassAd *ad, CondorError* errstack)
{
	Qmgr_connection *qmgr;
	ClassAd 		filterAd;
	int     		result;
	char    		scheddString [32];

	bool useFastPath = false;

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
		useFastPath = true;
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
	getAndFilterAds (filterAd, attrs, list, useFastPath);

	DisconnectQ (qmgr);
	return Q_OK;
}

int CondorQ::
fetchQueueFromHost (ClassAdList &list, StringList &attrs, const char *host, char const *schedd_version, CondorError* errstack)
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

	bool useFastPath = false;
	if( schedd_version && *schedd_version ) {
		CondorVersionInfo v(schedd_version);
		useFastPath = v.built_since_version(6,9,3);
	}

	// get the ads and filter them
	result = getAndFilterAds (filterAd, attrs, list, useFastPath);

	DisconnectQ (qmgr);
	return result;
}

int CondorQ::
fetchQueueFromDB (ClassAdList &list, char *&lastUpdate, char *dbconn, CondorError*  /*errstack*/)
{
#ifdef WANT_QUILL
	ClassAd 		filterAd;
	int     		result;
	JobQueueSnapshot	*jqSnapshot;
	const char 		*constraint;
	ClassAd        *ad;
	QuillErrCode   rv;

	jqSnapshot = new JobQueueSnapshot(dbconn);

	rv = jqSnapshot->startIterateAllClassAds(clusterarray,
						 numclusters,
						 procarray,
						 numprocs,
						 schedd,
						 FALSE,
						 scheddBirthdate,
						 lastUpdate);

	if (rv == QUILL_FAILURE) {
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
	tree = filterAd.LookupExpr(ATTR_REQUIREMENTS);
	if (!tree) {
		delete jqSnapshot;
	  return Q_INVALID_QUERY;
	}

	constraint = ExprTreeToString(tree);

	ad = getDBNextJobByConstraint(constraint, jqSnapshot);

	while (ad != (ClassAd *) 0) {
		ad->ChainCollapse();
		list.Insert(ad);
		ad = getDBNextJobByConstraint(constraint, jqSnapshot);
	}	

	delete jqSnapshot;
#endif /* WANT_QUILL */
	return Q_OK;
}

int CondorQ::
fetchQueueFromHostAndProcess ( const char *host, StringList &attrs, process_function process_func, bool useFastPath, CondorError* errstack)
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
	result = getFilterAndProcessAds (filterAd, attrs, process_func, useFastPath);

	DisconnectQ (qmgr);
	return result;
}

int CondorQ::
fetchQueueFromDBAndProcess ( char *dbconn, char *&lastUpdate, process_function process_func, CondorError*  /*errstack*/ )
{
#ifdef WANT_QUILL
	ClassAd 		filterAd;
	int     		result;
	JobQueueSnapshot	*jqSnapshot;
	const char           *constraint;
	ClassAd        *ad;
	QuillErrCode             rv;

	ASSERT(process_func);

	jqSnapshot = new JobQueueSnapshot(dbconn);

	rv = jqSnapshot->startIterateAllClassAds(clusterarray,
						 numclusters,
						 procarray,
						 numprocs,
						schedd,
						 FALSE,
						scheddBirthdate,
						lastUpdate);

	if (rv == QUILL_FAILURE) {
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
	tree = filterAd.LookupExpr(ATTR_REQUIREMENTS);
	if (!tree) {
		delete jqSnapshot;
	  return Q_INVALID_QUERY;
	}

	constraint = ExprTreeToString(tree);

	ad = getDBNextJobByConstraint(constraint, jqSnapshot);
	
	while (ad != (ClassAd *) 0) {
			// Process the data and insert it into the list
		if ((*process_func) (ad) ) {
			ad->Clear();
			delete ad;
		}
		
		ad = getDBNextJobByConstraint(constraint, jqSnapshot);
	}	

	delete jqSnapshot;
#endif /* WANT_QUILL */

	return Q_OK;
}

void CondorQ::rawDBQuery(char *dbconn, CondorQQueryType qType) {
#ifdef WANT_QUILL

	JobQueueDatabase *DBObj = NULL;
	const char    *rowvalue;
	int           ntuples;
	SQLQuery      sqlquery;
	char *tmp;
	dbtype dt;

	tmp = param("QUILL_DB_TYPE");
	if (tmp) {
		if (strcasecmp(tmp, "ORACLE") == 0) {
			dt = T_ORACLE;
		} else if (strcasecmp(tmp, "PGSQL") == 0) {
			dt = T_PGSQL;
		}
	} else {
		dt = T_PGSQL; // assume PGSQL by default
	}

	free(tmp);

	switch (dt) {				
	case T_ORACLE:
#if HAVE_ORACLE
		DBObj = new ORACLEDatabase(dbconn);
#else
		EXCEPT("Oracle database requested, but no Oracle support compiled in this version of Condor!");
#endif
		break;
	case T_PGSQL:
		DBObj = new PGSQLDatabase(dbconn);
		break;
	default:
		break;;
	}

	if (!DBObj || (DBObj->connectDB() == QUILL_FAILURE))
	{
		fprintf(stderr, "\n-- Failed to connect to the database\n");
		return;
	}

	switch (qType) {
	case AVG_TIME_IN_QUEUE:

		sqlquery.setQuery(QUEUE_AVG_TIME, NULL);		
		sqlquery.prepareQuery();

		DBObj->execQuery(sqlquery.getQuery(), ntuples);

			/* we expect exact one row out of the query */
		if (ntuples != 1) {
			fprintf(stderr, "\n-- Failed to execute the query\n");
			return;
		}
		
		rowvalue = DBObj -> getValue(0, 0);

		if(strcmp(rowvalue,"") == 0 ||  // result from empty job queue in pgsql
		   strcmp(rowvalue, " ::") == 0) //result from empty jobqueue in oracle
			{ 
			printf("\nJob queue is curently empty\n");
		} else {
			printf("\nAverage time in queue for uncompleted jobs (in days hh:mm:ss)\n");
			printf("%s\n", rowvalue);		 
		}
		
		DBObj -> releaseQueryResult();
		break;
	default:
		fprintf(stderr, "Error: type of query not supported\n");
		return;
		break;
	}

	if(DBObj) {
		delete DBObj;
	}	
#endif /* WANT_QUILL */
}

int CondorQ::
getFilterAndProcessAds( ClassAd &queryad, StringList &attrs, process_function process_func, bool useAll )
{
	const char		*constraint;
	ExprTree	*tree;
	ClassAd		*ad;

	tree = queryad.LookupExpr(ATTR_REQUIREMENTS);
	if (!tree) {
		return Q_INVALID_QUERY;
	}

	constraint = ExprTreeToString(tree);

	if (useAll) {
	// The fast case with the new protocol
	ClassAdList list;
	char *attrs_str = attrs.print_to_delimed_string();
	GetAllJobsByConstraint(constraint, attrs_str, list);
	free(attrs_str);
	list.Rewind();
	while ((ad = list.Next())) {
		if ( ( *process_func )( ad ) ) {
			//delete(ad);
		}
	}
	} else {

	// slow case, using old protocol
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
getAndFilterAds (ClassAd &queryad, StringList &attrs, ClassAdList &list, bool useAllJobs)
{
	const char	*constraint;
	ExprTree	*tree;

	tree = queryad.LookupExpr(ATTR_REQUIREMENTS);
	if (!tree) {
		return Q_INVALID_QUERY;
	}
	constraint = ExprTreeToString(tree);

	if (useAllJobs) {
	char *attrs_str = attrs.print_to_delimed_string();
	GetAllJobsByConstraint(constraint, attrs_str, list);
	free(attrs_str);

	} else {
	ClassAd		*ad;
	if ((ad = GetNextJobByConstraint(constraint, 1)) != NULL) {
		list.Insert(ad);
		while((ad = GetNextJobByConstraint(constraint, 0)) != NULL) {
			list.Insert(ad);
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


int JobSort(ClassAd *job1, ClassAd *job2, void *  /*data*/)
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
char
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

#ifdef WANT_QUILL

ClassAd* getDBNextJobByConstraint(const char* constraint, JobQueueSnapshot	*jqSnapshot)
{
	ClassAd *ad;
	
	while(jqSnapshot->iterateAllClassAds(ad) != DONE_JOBS_CURSOR) {
		if ((!constraint || !constraint[0] || EvalBool(ad, constraint))) {
			return ad;		      
		}
		
		if (ad != (ClassAd *) 0) {
			ad->Clear();
			delete ad;
			ad = (ClassAd *) 0;
		}
	}

	return (ClassAd *) 0;
}

#endif /* WANT_QUILL */
