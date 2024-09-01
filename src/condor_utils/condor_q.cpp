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
#include "dc_schedd.h"
#include "my_username.h"

// specify keyword lists; N.B.  The order should follow from the category
// enumerations in the .h file

static const char *strKeywordsOld[] =
{
	ATTR_OWNER,
	"(" ATTR_ACCOUNTING_GROUP " is undefined ? " ATTR_OWNER " : " ATTR_ACCOUNTING_GROUP ")"
};

static const char *strKeywordsModern[] =
{
	ATTR_OWNER,
	"(" ATTR_ACCOUNTING_GROUP "?:" ATTR_OWNER ")"
};

CondorQ::
CondorQ(void)
{
	connect_timeout = 20;

	owner[0] = '\0';
	schedd[0] = '\0';
	scheddBirthdate = 0;
	useDefaultingOperator(false);
	requestservertime = false;
}

void CondorQ::useDefaultingOperator(bool enable)
{
	defaulting_operator = enable;
}

CondorQ::~CondorQ () {}

bool CondorQ::
init()
{
	connect_timeout = param_integer( "Q_QUERY_TIMEOUT", connect_timeout );
	return true;
}

int CondorQ::
add (CondorQStrCategories cat, const char *value)
{
	const char * attr = nullptr;
	switch (cat) {
	case CQ_OWNER:
	case CQ_SUBMITTER:
		strncpy(owner, value, MAXOWNERLEN - 1);
		if (defaulting_operator) {
			attr = strKeywordsModern[cat];
		} else {
			attr = strKeywordsOld[cat];
		}
		break;
	default:
		break;
	}
	if (!attr) {
		return Q_INVALID_CATEGORY;
	}

	// TODO: delay rendering of the expression until we need to know the final query expr
	// so we can take the version of the schedd into account?  (8.6.0 or later to use modern keywords)
	std::string expr;
	QuoteAdStringValue(value, expr);
	expr.insert(0,"==");
	expr.insert(0,attr);

	return query.addCustomOR (expr.c_str());
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
fetchQueue (ClassAdList &list, const std::vector<std::string> &attrs, ClassAd *ad, CondorError* errstack)
{
	Qmgr_connection *qmgr;
	ExprTree		*tree;
	int     		result;
	std::string scheddString, constraintString;
	const char 		*constraint;

	int useFastPath = 0;

	// make the query ad
	if ((result = query.makeQuery (tree)) != Q_OK)
		return result;
	constraint = ExprTreeToString( tree, constraintString );
	delete tree;

	// connect to the Q manager
	init();  // needed to get default connect_timeout
	if (ad == 0)
	{
		// local case
		DCSchedd schedd(nullptr);
		if( !(qmgr = ConnectQ( schedd, connect_timeout, true, errstack)) ) {
			errstack->push("TEST", 0, "FOO");
			return Q_SCHEDD_COMMUNICATION_ERROR;
		}
		useFastPath = 2;
	}
	else
	{
		// remote case to handle condor_globalq
		if (!ad->LookupString (ATTR_SCHEDD_IP_ADDR, scheddString))
			return Q_NO_SCHEDD_IP_ADDR;

		DCSchedd schedd(scheddString.c_str());
		if( !(qmgr = ConnectQ( schedd, connect_timeout, true, errstack)) )
			return Q_SCHEDD_COMMUNICATION_ERROR;

	}

	// get the ads and filter them
	getAndFilterAds (constraint, attrs, -1, list, useFastPath);

	DisconnectQ (qmgr);
	return Q_OK;
}

int CondorQ::
fetchQueueFromHost (ClassAdList &list, const std::vector<std::string> &attrs, const char *host, char const *schedd_version, CondorError* errstack)
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
	DCSchedd schedd(host);
	if( !(qmgr = ConnectQ( schedd, connect_timeout, true, errstack)) )
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
CondorQ::fetchQueueFromHostAndProcess ( const char *host,
										const std::vector<std::string> &attrs,
										int fetch_opts,
										int match_limit,
										condor_q_process_func process_func,
										void * process_func_data,
										int useFastPath,
										CondorError* errstack,
										ClassAd ** psummary_ad)
{
	Qmgr_connection *qmgr;
	ExprTree		*tree;
	int     		result;

	// make the query ad
	if (useFastPath > 1) {
		return fetchQueueFromHostAndProcessV2(host, attrs, fetch_opts, match_limit, process_func, process_func_data, connect_timeout, useFastPath, errstack, psummary_ad);
	}

	if (fetch_opts != fetch_Jobs) {
		return Q_UNSUPPORTED_OPTION_ERROR;
	}

	if ((result = query.makeQuery (tree)) != Q_OK)
		return result;
	ConstraintHolder constraint(tree);

	/*
	 connect to the Q manager.
	 use a timeout of 20 seconds, and a read-only connection.
	 why 20 seconds?  because careful research by Derek has shown
	 that whenever one needs a periodic time value, 20 is always
	 optimal.  :^).
	*/
	init();  // needed to get default connect_timeout
	DCSchedd schedd(host);
	if( !(qmgr = ConnectQ( schedd, connect_timeout, true, errstack)) ) {
		return Q_SCHEDD_COMMUNICATION_ERROR;
	}

	// get the ads and filter them
	result = getFilterAndProcessAds (constraint.c_str(), attrs, match_limit, process_func, process_func_data, useFastPath);

	DisconnectQ (qmgr);
	return result;
}

int
CondorQ::initQueryAd(
	ClassAd & request_ad,
	const std::vector<std::string> & attrs,
	int fetch_opts,
	int match_limit)
{
	std::string constraint;
	int rval = query.makeQuery(constraint);
	if (rval != Q_OK) return rval;

	// TODO: can we get rid of this? I don't think the schedd requires a trival constraint anymore.
	if (constraint.empty()) constraint = "TRUE";

	std::string projection(join(attrs,"\n"));

	auto_free_ptr owner;
	if (fetch_opts & fetch_MyJobs) { owner.set(my_username()); }

	return DCSchedd::makeJobsQueryAd(request_ad, constraint.c_str(), projection.c_str(), fetch_opts, match_limit, owner, requestservertime);
}

int
CondorQ::fetchQueueFromHostAndProcessV2(const char *host,
	const std::vector<std::string> &attrs,
	int fetch_opts,
	int match_limit,
	condor_q_process_func process_func,
	void * process_func_data,
	int connect_timeout,
	int useFastPath,
	CondorError *errstack,
	ClassAd ** psummary_ad)
{
	bool want_authentication = (fetch_opts & fetch_MyJobs) != 0;

	ClassAd request_ad;
	int rval = initQueryAd(request_ad, attrs, fetch_opts, match_limit);
	if (rval != Q_OK) {
		return rval;
	}

	DCSchedd schedd(host);
	int cmd = QUERY_JOB_ADS;
	if (want_authentication && (useFastPath > 2)) {
		bool can_auth = schedd.canUseQueryWithAuth();
		if (can_auth) {
			cmd = QUERY_JOB_ADS_WITH_AUTH;
		} else {
			dprintf (D_ALWAYS, "detected that authentication will not happen.  falling back to QUERY_JOB_ADS without authentication.\n");
		}
	}

	return schedd.queryJobs(cmd, request_ad, process_func, process_func_data, connect_timeout, errstack, psummary_ad);
}

#if 0 // moved to DCSChedd object
int CondorQ::fetchQueueV2(
	DCSchedd & schedd,
	int cmd,
	ClassAd & request_ad,
	condor_q_process_func process_func,
	void * process_func_data,
	int connect_timeout,
	CondorError *errstack,
	ClassAd ** psummary_ad)
{
	Sock* sock;
	if (!(sock = schedd.startCommand(cmd, Stream::reli_sock, connect_timeout, errstack))) return Q_SCHEDD_COMMUNICATION_ERROR;

	classad_shared_ptr<Sock> sock_sentry(sock);

	if (!putClassAd(sock, request_ad) || !sock->end_of_message()) return Q_SCHEDD_COMMUNICATION_ERROR;
	dprintf(D_FULLDEBUG, "Sent Query classad to schedd\n");

	ClassAd *ad = NULL;	// job ad result
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
			if (psummary_ad && rval == 0) {
				std::string val;
				if (ad->LookupString(ATTR_MY_TYPE, val) && val == "Summary") {
					ad->Delete(ATTR_OWNER); // remove the bogus owner attribute
					*psummary_ad = ad; // return the final ad, because it has summary information
					ad = NULL; // so we don't delete it below.
				}
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
#endif

#if 0
int
CondorQ::fetchQueueFromDBAndProcess ( const char *dbconn,
									  char *&lastUpdate,
									  condor_q_process_func process_func,
									  void * process_func_data,
									  CondorError*  /*errstack*/ )
{
	(void) dbconn;
	(void) lastUpdate;
	(void) process_func;
	(void) process_func_data;

	return Q_OK;
}

void
CondorQ::rawDBQuery(const char *dbconn, CondorQQueryType qType)
{
	(void) dbconn;
	(void) qType;
}
#endif

int
CondorQ::getFilterAndProcessAds( const char *constraint,
								 const std::vector<std::string> &attrs,
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
		std::string attrs_str = join(attrs,"\n");
		GetAllJobsByConstraint_Start(constraint, attrs_str.c_str());

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
						  const std::vector<std::string> &attrs,
						  int match_limit,
						  ClassAdList &list,
						  int useAllJobs)
{
	if (useAllJobs == 1) {
		std::string attrs_str = join(attrs, "\n");
		GetAllJobsByConstraint(constraint, attrs_str.c_str(), list);

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
	static const char encode[JOB_STATUS_MAX+1] = {
		  0,	 //zero
		'I',	 //IDLE					1
		'R',	 //RUNNING				2
		'X',	 //REMOVED				3
		'C',	 //COMPLETED			4
		'H',	 //HELD					5
		'>',	 //TRANSFERRING_OUTPUT	6
		'S',	 //SUSPENDED			7
		'F',	 //JOB_STATUS_FAILED	8
		'B',	 //JOB_STATUS_BLOCKED	9
	};

	if (status < JOB_STATUS_MIN || status > JOB_STATUS_MAX) {
		return ' ';
	}
	return encode[status];
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
