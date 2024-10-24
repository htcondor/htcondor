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


#ifndef _CONDOR_DC_SCHEDD_H
#define _CONDOR_DC_SCHEDD_H

#include "condor_common.h"
#include "condor_classad.h"
#include "condor_io.h"
#include "enum_utils.h"
#include "daemon.h"


typedef enum {
	AR_ERROR,
	AR_SUCCESS,
	AR_NOT_FOUND,
	AR_BAD_STATUS,
	AR_ALREADY_DONE,
	AR_PERMISSION_DENIED
} action_result_t;


// ATTR_ACTION_RESULT_TYPE should be one of these
typedef enum {
	AR_NONE = 0,	// don't care or don't yet know
	AR_LONG = 1,	// want info per job
	AR_TOTALS = 2	// want totals for each possible result
} action_result_type_t;

// Callback after an impersonation token command
//
typedef void ImpersonationTokenCallbackType(bool success, const std::string &token, const CondorError &err,
	void *misc_data);

#define ENABLE_RAW_JOB_QUERY 1
#ifdef ENABLE_RAW_JOB_QUERY
// forward ref to wire_ad_buffer.h
class WireClassadBuffer;
#endif

/** This is the Schedd-specific class derived from Daemon.  It
	implements some of the schedd's daemonCore command interface.
	For now, it implements the "ACT_ON_JOBS" command, which is used
	for the new condor_(rm|hold|release) tools.
*/
class DCSchedd : public Daemon {
public:

		/** Constructor.  Same as a Daemon object.
		  @param name The name (or sinful string) of the daemon, NULL
		              if you want local
		  @param pool The name of the pool, NULL if you want local
		*/
	DCSchedd( const char* const name = NULL, const char* pool = NULL );

		/** Constructor.  Same as a Daemon object.
		  @param ad   gets all the info out of this ad
		  @param pool The name of the pool, NULL if you want local
		*/
	DCSchedd( const ClassAd& ad, const char* pool = NULL );


		/// Destructor
	~DCSchedd();

		/** Hold all jobs that match the given constraint.
			Set ATTR_HOLD_REASON to the given reason.
			@param constraint What jobs to act on
			@param reason Why the action is being done
			@param reason_code The hold subcode
			@param result_type What kind of results you want
			@return ClassAd containing results of this action, or NULL
			if we couldn't get any results.  The caller must delete
			this ClassAd when they are done with the results.
		*/
	ClassAd* holdJobs( const char* constraint, const char* reason,
					   const char* reason_code,
					   CondorError * errstack,
					   action_result_type_t result_type = AR_TOTALS );

		/** Remove all jobs that match the given constraint.
			Set ATTR_REMOVE_REASON to the given reason.
			@param constraint What jobs to act on
			@param reason Why the action is being done
			@param result_type What kind of results you want
			@return ClassAd containing results of this action, or NULL
			if we couldn't get any results.  The caller must delete
			this ClassAd when they are done with the results.
		*/
	ClassAd* removeJobs( const char* constraint, const char* reason,
						 CondorError * errstack,
						 action_result_type_t result_type = AR_TOTALS );

		/** Force the local removal of jobs in the X state that match
			the given constraint, regardless of whether they've been
			successfully removed remotely.
			Set ATTR_REMOVE_REASON to the given reason.
			@param constraint What jobs to act on
			@param reason Why the action is being done
			@param result_type What kind of results you want
			@return ClassAd containing results of this action, or NULL
			if we couldn't get any results.  The caller must delete
			this ClassAd when they are done with the results.
		*/
	ClassAd* removeXJobs( const char* constraint, const char* reason,
						  CondorError * errstack,
						  action_result_type_t result_type = AR_TOTALS );

		/** Release all jobs that match the given constraint.
			Set ATTR_RELEASE_REASON to the given reason.
			@param constraint What jobs to act on
			@param reason Why the action is being done
			@param result_type What kind of results you want
			@return ClassAd containing results of this action, or NULL
			if we couldn't get any results.  The caller must delete
			this ClassAd when they are done with the results.
		*/
	ClassAd* releaseJobs( const char* constraint, const char* reason,
						  CondorError * errstack,
						  action_result_type_t result_type = AR_TOTALS );

		/** Hold all jobs specified in the given list.  The list
			should contain a comma-seperated list of cluster.proc job
			ids.  Also, set ATTR_HOLD_REASON to the given reason.
			@param constraint What jobs to act on
			@param reason Why the action is being done
			@param result_type What kind of results you want
			@return ClassAd containing results of this action, or NULL
			if we couldn't get any results.  The caller must delete
			this ClassAd when they are done with the results.
		*/
	ClassAd* holdJobs( const std::vector<std::string>& ids, const char* reason,
					   const char* reason_code,
					   CondorError * errstack,
					   action_result_type_t result_type = AR_LONG );

		/** Remove all jobs specified in the given list.  The
			list should contain a comma-seperated list of cluster.proc
			job ids.  Also, set ATTR_REMOVE_REASON to the given
			reason.
			@param constraint What jobs to act on
			@param reason Why the action is being done
			@param result_type What kind of results you want
			@return ClassAd containing results of this action, or NULL
			if we couldn't get any results.  The caller must delete
			this ClassAd when they are done with the results.
		*/
	ClassAd* removeJobs( const std::vector<std::string>& ids, const char* reason,
						 CondorError * errstack,
						 action_result_type_t result_type = AR_LONG );

		/** Force the local removal of jobs in the X state specified
			in the given list, regardless of whether they've
			been successfully removed remotely.  The list should
			contain a comma-seperated list of cluster.proc job ids.
			Also, set ATTR_REMOVE_REASON to the given reason.
			@param constraint What jobs to act on
			@param reason Why the action is being done
			@param result_type What kind of results you want
			@return ClassAd containing results of this action, or NULL
			if we couldn't get any results.  The caller must delete
			this ClassAd when they are done with the results.
		*/
	ClassAd* removeXJobs( const std::vector<std::string>& ids, const char* reason,
						  CondorError * errstack,
						  action_result_type_t result_type = AR_LONG );

		/** Release all jobs specified in the given list.  The
			list should contain a comma-seperated list of cluster.proc
			job ids.  Also, set ATTR_RELEASE_REASON to the given
			reason.
			@param constraint What jobs to act on
			@param reason Why the action is being done
			@param result_type What kind of results you want
			@return ClassAd containing results of this action, or NULL
			if we couldn't get any results.  The caller must delete
			this ClassAd when they are done with the results.
		*/
	ClassAd* releaseJobs( const std::vector<std::string>& ids, const char* reason,
						  CondorError * errstack,
						  action_result_type_t result_type = AR_LONG );


		/** Vacate all jobs specified in the given list.  The list
			should contain a comma-seperated list of cluster.proc job
			ids.
			@param ids What jobs to act on
			@param vacate_type Graceful or fast vacate?
			@param result_type What kind of results you want
			@return ClassAd containing results of this action, or NULL
			if we couldn't get any results.  The caller must delete
			this ClassAd when they are done with the results.
		*/
	ClassAd* vacateJobs( const std::vector<std::string>& ids, VacateType vacate_type,
						 CondorError * errstack,
						 action_result_type_t result_type = AR_LONG );

		/** Vacate all jobs that match the given constraint.
			@param constraint What jobs to act on
			@param vacate_type Graceful or fast vacate?
			@param result_type What kind of results you want
			@return ClassAd containing results of this action, or NULL
			if we couldn't get any results.  The caller must delete
			this ClassAd when they are done with the results.
		*/
	ClassAd* vacateJobs( const char* constraint, VacateType vacate_type,
						 CondorError * errstack,
						 action_result_type_t result_type = AR_TOTALS );


	/** Suspend all jobs specified in the given list.  The list
			should contain a comma-seperated list of cluster.proc job
			ids.
			@param ids What jobs to act on
			@param vacate_type Graceful or fast vacate?
			@param result_type What kind of results you want
			@return ClassAd containing results of this action, or NULL
			if we couldn't get any results.  The caller must delete
			this ClassAd when they are done with the results.
		*/
	ClassAd* suspendJobs( const std::vector<std::string>& ids, const char* reason,
						 CondorError * errstack,
						 action_result_type_t result_type = AR_LONG );

	/** Suspend all jobs that match the given constraint.
			@param constraint What jobs to act on
			@param reason Why the action is being done
			@param result_type What kind of results you want
			@return ClassAd containing results of this action, or NULL
			if we couldn't get any results.  The caller must delete
			this ClassAd when they are done with the results.
		*/
	ClassAd* suspendJobs( const char* constraint, const char* reason,
						  CondorError * errstack,
						  action_result_type_t result_type = AR_TOTALS );

	/** Continue all jobs specified in the given list.  The list
			should contain a comma-seperated list of cluster.proc job
			ids.
			@param ids What jobs to act on
			@param vacate_type Graceful or fast vacate?
			@param result_type What kind of results you want
			@return ClassAd containing results of this action, or NULL
			if we couldn't get any results.  The caller must delete
			this ClassAd when they are done with the results.
		*/
	ClassAd* continueJobs( const std::vector<std::string>& ids, const char* reason,
						 CondorError * errstack,
						 action_result_type_t result_type = AR_LONG );

		/** Continue all jobs that match the given constraint.
			@param constraint What jobs to act on
			@param reason Why the action is being done
			@param result_type What kind of results you want
			@return ClassAd containing results of this action, or NULL
			if we couldn't get any results.  The caller must delete
			this ClassAd when they are done with the results.
		*/
	ClassAd* continueJobs( const char* constraint, const char* reason,
						  CondorError * errstack,
						  action_result_type_t result_type = AR_TOTALS );

	/** Clear dirty attributes for a list of job ids
			@param ids What jobs to act on
			@param result_type What kind of results you want
		*/
	ClassAd* clearDirtyAttrs( const std::vector<std::string>& ids, CondorError * errstack,
						action_result_type_t result_type = AR_TOTALS );

	/** export jobs to an external job_queue and put mark them as MANAGED by Lumberjack
	    @param ids or constraint What jobs to act on
	    @param export_dir what directory to export the jobs to. <export_dir>/job_queue.log will be the exported log
	    @param new_spool_dir what value to use when rewriting paths into the SPOOL directory
	*/
	ClassAd* exportJobs( const std::vector<std::string>& ids, const char * export_dir, const char * new_spool_dir, CondorError * errstack);
	ClassAd* exportJobs( const char * constraint, const char * export_dir, const char * new_spool_dir, CondorError * errstack);

	/** import the results from a previously exported job_queue.log managed by Lumberjack
	    @param import_dir directory containing the exported job_queue.log and job files to be imported
	*/
	ClassAd* importExportedJobResults(const char * import_dir, CondorError * errstack);

	/** unexport jobs that were previously exported via exportJobs().
	    @param ids or constraint What jobs to act on
	*/
	ClassAd* unexportJobs( const std::vector<std::string>& ids, CondorError * errstack);
	ClassAd* unexportJobs( const char * constraint, CondorError * errstack);

	/** Vacate the victim and schedule the beneficiary on its slot.  Hard-
		kills the job.  The caller must authenticate as a queue user or
		the owner of both jobs; the victim must be running and the beneficiary
		must be idle.  Returns true iff it received a valid reply.
			@param beneficiary The job the schedule on the vacated slot.
			@param reply The reply from the schedd (unchanged if none).
			@param errorMessage The error message (unchanged if none).
			@param victims A pointer to an array victim jobs.
			@param victimCount The size of the array.
			@param flags Reserved.
		*/
	bool reassignSlot( PROC_ID beneficiary, ClassAd & reply, std::string & errorMessage, PROC_ID * victims, unsigned victimCount, int flags = 0 );

		/** Get starter connection info for a running job.
			@param jobid What job to act on
			@param subproc For parallel jobs, which one (-1 if not specified)
			@param session_info Security session parameters (e.g. encryption)
			@param errstack Record of errors encountered
			@param starter_addr Contact address of starter
			@param starter_claim_id Security session info to use
			@param slot_name Example: slot1@somehost.edu
			@param error_msg Message for user describing why it failed
			@param retry_is_sensible True if failed and sensible to retry
			@param job_status Set iff return is false; the status of the job.
			@param hold_reason Set iff job is held; the job's hold reason.
			@return true on success; false on failure
		*/
	bool getJobConnectInfo( PROC_ID jobid,
							int subproc,
							char const *session_info,
							int timeout,
							CondorError *errstack,
							std::string &starter_addr,
							std::string &starter_claim_id,
							std::string &starter_version,
							std::string &slot_name,
							std::string &error_msg,
							bool &retry_is_sensible,
							int &job_status,
							std::string &hold_reason);


		/** Request the schedd to initiate a negoitation cycle.
			The request is sent via a SafeSock (UDP datagram).
			@return true on success, false on failure.
		*/
	bool reschedule();

	bool spoolJobFiles(int JobAdsArrayLen, ClassAd* JobAdsArray[], CondorError * errstack);

	bool receiveJobSandbox(const char* constraint, CondorError * errstack, int * numdone = 0);


	bool updateGSIcredential(const int cluster, const int proc,
		const char* path_to_proxy_file, CondorError * errstack);

		// expiration_time: 0 if none; o.w. time to expire delegated proxy
		// result_expiration_time: if non-NULL, gets set to the delegated
		//                         proxy expiration time, which may be sooner
		//                         than requested if source proxy expires
	bool delegateGSIcredential(const int cluster, const int proc,
							   const char* path_to_proxy_file, time_t expiration_time,
							   time_t *result_expiration_time,
							   CondorError * errstack);

		// Caller should delete new_job_ad when done with it.
		// Returns false on error (see error_msg)
		// If no new job found, returns true with *new_job_ad=NULL
	bool recycleShadow( int previous_job_exit_reason, ClassAd **new_job_ad, std::string & error_msg );


		/*
		 * Retrieve a token with someone else's identity from a remote schedd,
		 * based on an existing session.
		 */
	bool requestImpersonationTokenAsync(const std::string &identity,
		const std::vector<std::string> &authz_bounding_set, int lifetime,
		ImpersonationTokenCallbackType callback, void *misc_data, CondorError &err);


		/*
		 * Methods to query job ads, cluster ads, jobset ads, and autocluster ads.
		 * also dynamic autocluster ads (aka group-by)
		 */

	static const char* makeProjectionFromAttrs(classad::References & attrs, std::string & proj) {
		for (const auto & attr : attrs) { if (!proj.empty()) proj += "\n"; proj += attr; }
		if (proj.empty()) return nullptr;
		return proj.c_str();
	}

	static int makeJobsQueryAd (
		ClassAd & request_ad,
		const char *constraint,
		const char *projection,
		int fetch_opts,     // flags from QueryFetchOpts enum in condor_q.h
		int match_limit = -1,
		const char * owner = nullptr, // needed if fetch_opts includes MyJobs flag
		bool send_server_time = false);

	// check config to see if settings allow us to use QUERY_JOB_ADS_WITH_AUTH
	// TODO: someday also check Schedd locate ad to see if it will allow that?? (right now we assume the schedd config is the same as ours)
	bool canUseQueryWithAuth(); // returns true if we think the schedd and handle QUERY_JOB_ADS_WITH_AUTH

	// returns Q_OK (0) on success
	// returns one of errors from query_result_type.h on failure (like Q_SCHEDD_COMMUNICATION_ERROR)
	int queryJobs (int cmd, // QUERY_JOB_ADS or QUERY_JOB_ADS_WITH_AUTH
		const ClassAd & query_ad,
		// return false to take ownership of the ad, true to allow the ad to be deleted after
		bool (*process_func)(void*, ClassAd *ad),
		void * process_func_data,
		int connect_timeout,
		CondorError *errstack,
		ClassAd ** psummary_ad);

#ifdef ENABLE_RAW_JOB_QUERY
	int queryJobs (int cmd, // QUERY_JOB_ADS or QUERY_JOB_ADS_WITH_AUTH
		const ClassAd & query_ad,
		bool (*process_func)(void*, class WireClassadBuffer & wab),
		void * process_func_data,
		int connect_timeout,
		CondorError *errstack,
		class WireClassadBuffer & wab); // used as per-job buffer and holds summary ad at the end
#endif

		/*
		 * methods for schedd UserRec records
		 */
	static int makeUsersQueryAd (
		classad::ClassAd & request_ad,
		const char * constraint,
		const char * projection,
		bool send_server_time = false,
		int match_limit = -1);

	static int makeUsersQueryAd (
		classad::ClassAd & request_ad,
		const char * constraint,
		classad::References & projection,
		int match_limit = -1);

	int queryUsers(
		const classad::ClassAd & query_ad,
		// return 0 to take ownership of the ad, non-zero to allow the ad to be deleted after, -1 aborts the loop
		int (*process_func)(void*, ClassAd *ad),
		void * process_func_data,
		int connect_timeout,
		CondorError *errstack,
		ClassAd ** psummary_ad);

	ClassAd * addUsers(
		const char * usernames[], // owner@uid_domain, owner@ntdomain for windows
		int num_usernames,
		CondorError *errstack);

	ClassAd * enableUsers(
		const char * usernames[], // owner@uid_domain, owner@ntdomain for windows
		int num_usernames,
		bool create_if,           // true if we want to create users that don't already exist
		CondorError *errstack);

	ClassAd * enableUsers(
		const char * constraint,  // expression
		CondorError *errstack);

	ClassAd * addOrEnableUsers(
		const ClassAd * userads[],   // ads must have ATTR_USER attribute, if create_if may have other attributes as well
		int num_usernames,
		bool create_if,           // true if we want to create users that don't already exist
		CondorError *errstack);

	ClassAd * disableUsers(
		const char * usernames[], // owner@uid_domain, owner@ntdomain for windows
		int num_usernames,
		const char * reason,
		CondorError *errstack);

	ClassAd * disableUsers(
		const char * constraint, // expression
		const char * reason,
		CondorError *errstack);

	ClassAd * updateUserAds(
		ClassAdList & user_ads,	 // ads must have ATTR_USER attribute at a minimum
		CondorError *errstack);

private:
		/** This method actually does all the brains for all versions
			of holdJobs(), removeJobs(), and releaseJobs().  This
			creates the ClassAd describing what we want to do, and
			speaks the network protocol with the schedd to do the
			work we need done.  This protocol is basically a 2 phase
			commit, where we send a ClassAd with what we want, the
			schedd sends a reply ClassAd with the results, and waits
			for us to send it an "OK".  If it never gets a reply, it
			assumes we were killed in the middle of the command, and
			aborts the transaction.
			@param action What action we're supposed to perform
			@param constraint Constraint to operate on, or NULL
			@param ids vector<string> of ids to operate on, or NULL
			@param reason A string describing what we're doing
			@param reason_attr_name Attribute name for the reason
			@param reason_code A string such as an error code
			@param reason_code_attr_name Attribute name for the reason_code
			@param result_type What kind of results you want
			@return ClassAd containing results of this action, or NULL
			if we couldn't get any results.  The caller must delete
			this ClassAd when they are done with the results.
		*/
	ClassAd* actOnJobs( JobAction action,
						const char* constraint, const std::vector<std::string>* ids,
						const char* reason, const char* reason_attr,
						const char* reason_code, const char* reason_code_attr,
						action_result_type_t result_type,
						CondorError * errstack );

	/*
	* Create/Enable/Disable UserRec records in the schedd
	* based on an existing session.
	*/
	ClassAd* actOnUsers (
		int cmd, // ENABLE_USERREC or DISABLE_USERREC
		const ClassAd* userads[], // either pass array of ad pointers
		const char* usernames[], // or pass array of username pointers
		int num_usernames,
		bool create_if, // if true, treat ENABLE into as a CREATE
		const char * reason,
		CondorError *errstack,
		int connect_timeout = 20);

	void requestImpersonationTokenContinued(bool success, Sock *sock, CondorError *errstack,
		const std::string &trust_domain, bool should_try_token_request, void *misc_data);

	int requestImpersonationTokenFinish(Stream *stream);

	ClassAd* exportJobsWorker( const std::vector<std::string>* ids, const char * constraint, const char * export_dir, const char * new_spool_dir, CondorError * errstack);

	ClassAd* unexportJobsWorker( const std::vector<std::string>* ids, const char * constraint, CondorError * errstack);

		// I can't be copied (yet)
	DCSchedd( const DCSchedd& );
	DCSchedd& operator = ( const DCSchedd& );

};


/** This class is used to manage the results of any of the methods in
	the DCSchedd class which use the ACT_ON_JOBS protocol (remove,
	hold, and release).  It is also used by the schedd to construct
	this result ClassAd.  You instantiate a JobActionResults object,
	call the readResults() method and pass in whatever ClassAd you got
	back from DCSchedd::*Jobs().  Then you can call methods to query
	the results for specific jobs, or to access the totals, depending
	on what kind of results you requested when you sent the command.
*/
class JobActionResults {
public:

		/// Constructor
	JobActionResults( action_result_type_t res_type = AR_NONE );

		/// Destructor
	~JobActionResults();

		/** Publish all the results we know of into our result ad and
			return a pointer to the ad.  The caller should *NOT*
			delete this ad, since the memory is managed by this
			class.  The resulting ad is only valid while the
			JobActionResults class still exists.
		*/
	ClassAd* publishResults( void );

		/** Grab a copy the given ClassAd for our own use.  Also,
			parse through it to initialize our totals (if it's got any
			totals) and any other useful info that might be in a
			result ad.
		*/
	void readResults( ClassAd* ad );

		/** Record the result for the given job_id.  If our
			result_type is set to AR_LONG, we'll add a new attribute
			to our result ad for this job.  Otherwise, we'll just
			increment our total for this result.
		*/
	void record( PROC_ID job_id, action_result_t result );

	int numError( void ) const { return ar_error; };
	int numSuccess( void ) const { return ar_success; };
	int numNotFound( void ) const { return ar_not_found; };
	int numBadStatus( void ) const { return ar_bad_status; };
	int numAlreadyDone( void ) const { return ar_already_done; };
	int numPermissionDenied( void ) const { return ar_permission_denied; };

		/** Return the result code for the given job.
			@param job_id The job you care about
		*/
	action_result_t getResult( PROC_ID job_id );

		/** Allocate a string containing a human readable version of
			the result of the last action on the given job.
			@param job_id The job you care about
			@param str pointer to the string that will hold the newly
			allocated string.  You should *always* free() this string
			when you're done, since you'll have something regardless
			of the return value
			@return true if the result was success, false otherwise
		*/
	bool getResultString( PROC_ID job_id, char** str );

private:

	JobAction action;
	action_result_type_t result_type;

	ClassAd* result_ad;

		// Totals
	int ar_error;
	int ar_success;
	int ar_not_found;
	int ar_bad_status;
	int ar_already_done;
	int ar_permission_denied;

		// I can't be copied (yet)
	JobActionResults( const JobActionResults & );
	JobActionResults& operator = ( const JobActionResults & );

};


#endif /* _CONDOR_DC_SCHEDD_H */
