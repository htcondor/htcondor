/***************************************************************
 *
 * Copyright (C) 1990-2016, Condor Team, Computer Sciences Department,
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
#include "condor_classad.h"
#include "quill_enums.h"
#include "dc_schedd.h"

#ifdef HAVE_EXT_POSTGRESQL
#include "pgsqldatabase.h"
#include "jobqueuesnapshot.h"

static ClassAd* getDBNextJobByConstraint(const char* constraint, JobQueueSnapshot  *jqSnapshot);

#endif /* HAVE_EXT_POSTGRESQL */

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
CondorQ( void )
{
	connect_timeout = 20; 
	query.setNumIntegerCats (CQ_INT_THRESHOLD);
	query.setNumStringCats (CQ_STR_THRESHOLD);
	query.setNumFloatCats (CQ_FLT_THRESHOLD);
	query.setIntegerKwList (const_cast<char **>(intKeywords));
	query.setStringKwList (const_cast<char **>(strKeywords));
	query.setFloatKwList (const_cast<char **>(fltKeywords));

	clusterprocarraysize = 128;
	clusterarray = (int *) malloc(clusterprocarraysize * sizeof(int));
	procarray = (int *) malloc(clusterprocarraysize * sizeof(int));
	ASSERT( clusterarray != NULL && procarray != NULL );
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
		   void * pvc = realloc(clusterarray, 
					clusterprocarraysize * 2 * sizeof(int));
		   void * pvp = realloc(procarray, 
					clusterprocarraysize * 2 * sizeof(int));
		   ASSERT( pvc != NULL && pvp != NULL );
		   clusterarray = (int *) pvc;
		   procarray = (int *) pvp;
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
add (CondorQStrCategories cat, const char *value)
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
addOR (const char *value)
{
	return query.addCustomOR (value);
}

int CondorQ::
addAND (const char *value)
{
	return query.addCustomAND (value);
}

int CondorQ::
addSchedd (const char *value)
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
	ExprTree		*tree;
	int     		result;
	char    		scheddString [32];
	const char 		*constraint;

	int useFastPath = 0;

	// make the query ad
	if ((result = query.makeQuery (tree)) != Q_OK)
		return result;
	constraint = ExprTreeToString( tree );
	delete tree;

	// connect to the Q manager
	init();  // needed to get default connect_timeout
	if (ad == 0)
	{
		// local case
		if( !(qmgr = ConnectQ( 0, connect_timeout, true, errstack)) ) {
			errstack->push("TEST", 0, "FOO");
			return Q_SCHEDD_COMMUNICATION_ERROR;
		}
		useFastPath = 2;
	}
	else
	{
		// remote case to handle condor_globalq
		if (!ad->LookupString (ATTR_SCHEDD_IP_ADDR, scheddString, sizeof(scheddString)))
			return Q_NO_SCHEDD_IP_ADDR;

		if( !(qmgr = ConnectQ( scheddString, connect_timeout, true, errstack)) )
			return Q_SCHEDD_COMMUNICATION_ERROR;

	}

	// get the ads and filter them
	getAndFilterAds (constraint, attrs, -1, list, useFastPath);

	DisconnectQ (qmgr);
	return Q_OK;
}

int CondorQ::
fetchQueueFromHost (ClassAdList &list, StringList &attrs, const char *host, char const *schedd_version, CondorError* errstack)
{
	Qmgr_connection *qmgr;
	ExprTree		*tree;
	const char		*constraint;
	int     		result;

	// make the query ad
	if ((result = query.makeQuery (tree)) != Q_OK)
		return result;
	constraint = ExprTreeToString( tree );
	delete tree;

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

	int useFastPath = 0;
	if( schedd_version && *schedd_version ) {
		CondorVersionInfo v(schedd_version);
		useFastPath = v.built_since_version(6,9,3) ? 1 : 0;
		if (v.built_since_version(8, 1, 5)) {
			useFastPath = 2;
		}
	}

	// get the ads and filter them
	result = getAndFilterAds (constraint, attrs, -1, list, useFastPath);

	DisconnectQ (qmgr);
	return result;
}

int
CondorQ::fetchQueueFromDB (ClassAdList &list,
						   char *&lastUpdate,
						   const char *dbconn,
						   CondorError*  /*errstack*/)
{
#ifndef HAVE_EXT_POSTGRESQL
	(void) list;
	(void) lastUpdate;
	(void) dbconn;
#else
	int     		result;
	JobQueueSnapshot	*jqSnapshot;
	const char 		*constraint;
	ClassAd        *ad;
	QuillErrCode   rv;
	ExprTree *tree;

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
	if ((result = query.makeQuery (tree)) != Q_OK) {
		delete jqSnapshot;
		return result;
	}

	constraint = ExprTreeToString(tree);
	delete tree;

	ad = getDBNextJobByConstraint(constraint, jqSnapshot);

	while (ad != (ClassAd *) 0) {
		ad->ChainCollapse();
		list.Insert(ad);
		ad = getDBNextJobByConstraint(constraint, jqSnapshot);
	}	

	delete jqSnapshot;
#endif /* HAVE_EXT_POSTGRESQL */

	return Q_OK;
}

int
CondorQ::fetchQueueFromHostAndProcess ( const char *host,
										StringList &attrs,
										int fetch_opts,
										int match_limit,
										condor_q_process_func process_func,
										void * process_func_data,
										int useFastPath,
										CondorError* errstack)
{
	Qmgr_connection *qmgr;
	ExprTree		*tree;
	char 			*constraint;
	int     		result;

	// make the query ad
	if ((result = query.makeQuery (tree)) != Q_OK)
		return result;
	constraint = strdup( ExprTreeToString( tree ) );
	delete tree;

	if (useFastPath == 2) {
		int result = fetchQueueFromHostAndProcessV2(host, constraint, attrs, fetch_opts, match_limit, process_func, process_func_data, connect_timeout, errstack);
		free( constraint);
		return result;
	}

	if (fetch_opts != fetch_Default) {
		free( constraint );
		return Q_UNSUPPORTED_OPTION_ERROR;
	}

	/*
	 connect to the Q manager.
	 use a timeout of 20 seconds, and a read-only connection.
	 why 20 seconds?  because careful research by Derek has shown
	 that whenever one needs a periodic time value, 20 is always
	 optimal.  :^).
	*/
	init();  // needed to get default connect_timeout
	if( !(qmgr = ConnectQ( host, connect_timeout, true, errstack)) ) {
		free( constraint );
		return Q_SCHEDD_COMMUNICATION_ERROR;
	}

	// get the ads and filter them
	result = getFilterAndProcessAds (constraint, attrs, match_limit, process_func, process_func_data, useFastPath);

	DisconnectQ (qmgr);
	free( constraint );
	return result;
}

int
CondorQ::fetchQueueFromHostAndProcessV2(const char *host,
					const char *constraint,
					StringList &attrs,
					int fetch_opts,
					int match_limit,
					condor_q_process_func process_func,
					void * process_func_data,
					int connect_timeout,
					CondorError *errstack)
{
	classad::ClassAdParser parser;
	classad::ExprTree *expr = NULL;
	parser.ParseExpression(constraint, expr);
	if (!expr) return Q_INVALID_REQUIREMENTS;

	classad::ClassAd request_ad;  // query ad to send to schedd
	ClassAd *ad = NULL;	// job ad result

	request_ad.Insert(ATTR_REQUIREMENTS, expr);

	char *projection = attrs.print_to_delimed_string(",");
	if (projection) {
		request_ad.InsertAttr(ATTR_PROJECTION, projection);
		free(projection);
	}

	if (fetch_opts == fetch_DefaultAutoCluster) {
		request_ad.InsertAttr("QueryDefaultAutocluster", true);
		request_ad.InsertAttr("MaxReturnedJobIds", 2); // TODO: make this settable by caller of this function.
	} else if (fetch_opts == fetch_GroupBy) {
		request_ad.InsertAttr("ProjectionIsGroupBy", true);
		request_ad.InsertAttr("MaxReturnedJobIds", 2); // TODO: make this settable by caller of this function.
	}

	if (match_limit >= 0)
	{
		request_ad.InsertAttr(ATTR_LIMIT_RESULTS, match_limit);
	}

	DCSchedd schedd(host);
	Sock* sock;
	if (!(sock = schedd.startCommand(QUERY_JOB_ADS, Stream::reli_sock, connect_timeout, errstack))) return Q_SCHEDD_COMMUNICATION_ERROR;

	classad_shared_ptr<Sock> sock_sentry(sock);

	if (!putClassAd(sock, request_ad) || !sock->end_of_message()) return Q_SCHEDD_COMMUNICATION_ERROR;
	dprintf(D_FULLDEBUG, "Sent classad to schedd\n");

	int rval = 0;
	do {
		ad = new ClassAd();
		if ( ! getClassAd(sock, *ad) || ! sock->end_of_message()) {
			rval = Q_SCHEDD_COMMUNICATION_ERROR;
			break;
		}
		dprintf(D_FULLDEBUG, "Got classad from schedd.\n");
		long long intVal;
		if (ad->EvaluateAttrInt(ATTR_OWNER, intVal) && (intVal == 0))
		{ // Last ad.
			sock->close();
			dprintf(D_FULLDEBUG, "Ad was last one from schedd.\n");
			std::string errorMsg;
			if (ad->EvaluateAttrInt(ATTR_ERROR_CODE, intVal) && intVal && ad->EvaluateAttrString(ATTR_ERROR_STRING, errorMsg))
			{
				if (errstack) errstack->push("TOOL", intVal, errorMsg.c_str());
				rval = Q_REMOTE_ERROR;
			}
			break;
		}
		// Note: According to condor_q.h, process_func() will return false if taking
		// ownership of ad, so only delete if it returns true, else set to NULL
		// so we don't delete it here.  Either way, next set ad to NULL since either
		// it has been deleted or will be deleted later by process_func().
		if (process_func(process_func_data, ad)) {
			delete ad;
		}
		ad = NULL;
	} while (true);

	// Make sure ad is not leaked no matter how we break out of the above loop.
	delete ad;

	return rval;
}

int
CondorQ::fetchQueueFromDBAndProcess ( const char *dbconn,
									  char *&lastUpdate,
									  condor_q_process_func process_func,
									  void * process_func_data,
									  CondorError*  /*errstack*/ )
{
#ifndef HAVE_EXT_POSTGRESQL
	(void) dbconn;
	(void) lastUpdate;
	(void) process_func;
	(void) process_func_data;
#else
	int     		result;
	JobQueueSnapshot	*jqSnapshot;
	const char           *constraint;
	ClassAd        *ad;
	QuillErrCode             rv;
	ExprTree *tree;

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
	if ((result = query.makeQuery (tree)) != Q_OK) {
		delete jqSnapshot;
		return result;
	}

	constraint = ExprTreeToString(tree);
	delete tree;

	ad = getDBNextJobByConstraint(constraint, jqSnapshot);
	
	while (ad != (ClassAd *) 0) {
			// Process the data and insert it into the list
		if ((*process_func) (process_func_data, ad) ) {
			ad->Clear();
			delete ad;
		}
		
		ad = getDBNextJobByConstraint(constraint, jqSnapshot);
	}	

	delete jqSnapshot;
#endif /* HAVE_EXT_POSTGRESQL */

	return Q_OK;
}

void
CondorQ::rawDBQuery(const char *dbconn, CondorQQueryType qType)
{
#ifndef HAVE_EXT_POSTGRESQL
	(void) dbconn;
	(void) qType;
#else

	JobQueueDatabase *DBObj = NULL;
	const char    *rowvalue;
	int           ntuples;
	SQLQuery      sqlquery;
	char *tmp;
	dbtype dt;

	tmp = param("QUILL_DB_TYPE");
	if (tmp) {
		if (strcasecmp(tmp, "PGSQL") == 0) {
			dt = T_PGSQL;
		}
	} else {
		dt = T_PGSQL; // assume PGSQL by default
	}

	free(tmp);

	switch (dt) {				
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

		if(strcmp(rowvalue,"") == 0) // result from empty job queue in pgsql
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
#endif /* HAVE_EXT_POSTGRESQL */
}

int
CondorQ::getFilterAndProcessAds( const char *constraint,
								 StringList &attrs,
								 int match_limit,
								 condor_q_process_func process_func,
								 void * process_func_data,
								 bool useAll )
{
	int match_count = 0;
	ClassAd *ad = NULL;	// job ad result
	int rval = Q_OK; // return success by default, reset on error

	if (useAll) {
			// The fast case with the new protocol
		char *attrs_str = attrs.print_to_delimed_string();
		GetAllJobsByConstraint_Start(constraint, attrs_str);
		free(attrs_str);

		while( true ) {
			ad = new ClassAd();
			if (match_limit >= 0 && match_count >= match_limit)
				break;
			if( GetAllJobsByConstraint_Next( *ad ) != 0 ) {
				break;
			}
			++match_count;
			// Note: According to condor_q.h, process_func() will return false if taking
			// ownership of ad, so only delete if it returns true, else set to NULL
			// so we don't delete it here.  Either way, next set ad to NULL since either
			// it has been deleted or will be deleted later by process_func().
			if (process_func(process_func_data, ad)) {
				delete(ad);
			}
			ad = NULL;
		}
	} else {

		// slow case, using old protocol
		ad = GetNextJobByConstraint(constraint, 1);
		if (ad) {
			// Process the data and insert it into the list.
			// Note: According to condor_q.h, process_func() will return false if taking
			// ownership of ad, so only delete if it returns true, else set to NULL
			// so we don't delete it here.  Either way, next set ad to NULL since either
			// it has been deleted or will be deleted later by process_func().
			if (process_func(process_func_data, ad)) {
				delete ad;
			}
			ad = NULL;

			++match_count;

			while ((ad = GetNextJobByConstraint(constraint, 0)) != NULL) {
				if (match_limit >= 0 && match_count >= match_limit)
					break;
				// Process the data and insert it into the list.
				// See comment above re the return value of process_func.
				if (process_func(process_func_data, ad)) {
					delete ad;
				}
				ad = NULL;
			}
		}
	}

	// Make sure ad is not leaked no matter how we break out of the above loops.
	delete ad; 

	// here GetNextJobByConstraint returned NULL.  check if it was
	// because of the network or not.  if qmgmt had a problem with
	// the net, then errno is set to ETIMEDOUT, and we should fail.
	if ( errno == ETIMEDOUT ) {
		rval = Q_SCHEDD_COMMUNICATION_ERROR;
	}

	return rval;
}


int
CondorQ::getAndFilterAds (const char *constraint,
						  StringList &attrs,
						  int match_limit,
						  ClassAdList &list,
						  int useAllJobs)
{
	if (useAllJobs == 1) {
		char *attrs_str = attrs.print_to_delimed_string();
		GetAllJobsByConstraint(constraint, attrs_str, list);
		free(attrs_str);

	} else {
		ClassAd		*ad;
		int match_count = 0;
		if ((ad = GetNextJobByConstraint(constraint, 1)) != NULL) {
			list.Insert(ad);
			++match_count;
			while((ad = GetNextJobByConstraint(constraint, 0)) != NULL) {
				if (match_limit > 0 && match_count >= match_limit)
					break;
				list.Insert(ad);
				++match_count;
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
	  case TRANSFERRING_OUTPUT:
		return '>';
	  case SUSPENDED:
		return 'S';
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

#ifdef HAVE_EXT_POSTGRESQL

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

#endif /* HAVE_EXT_POSTGRESQL */
