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

		/// Destructor
	~DCSchedd();

		/** Hold all jobs that match the given constraint.
			Set ATTR_HOLD_REASON to the given reason.
			@param constraint What jobs to act on
			@param reason Why the action is being done
			@param result_type What kind of results you want
			@param notify_scheduler Should the schedd notify the
 			controlling scheduler for this job?
			@return ClassAd containing results of this action, or NULL
			if we couldn't get any results.  The caller must delete
			this ClassAd when they are done with the results.
		*/
	ClassAd* holdJobs( const char* constraint, const char* reason,
					   CondorError * errstack,
					   action_result_type_t result_type = AR_TOTALS,
					   bool notify_scheduler = true );

		/** Remove all jobs that match the given constraint.
			Set ATTR_REMOVE_REASON to the given reason.
			@param constraint What jobs to act on
			@param reason Why the action is being done
			@param result_type What kind of results you want
			@param notify_scheduler Should the schedd notify the
 			controlling scheduler for this job?
			@return ClassAd containing results of this action, or NULL
			if we couldn't get any results.  The caller must delete
			this ClassAd when they are done with the results.
		*/
	ClassAd* removeJobs( const char* constraint, const char* reason,
						 CondorError * errstack,
						 action_result_type_t result_type = AR_TOTALS,
						 bool notify_scheduler = true );

		/** Force the local removal of jobs in the X state that match
			the given constraint, regardless of whether they've been
			successfully removed remotely.
			Set ATTR_REMOVE_REASON to the given reason.
			@param constraint What jobs to act on
			@param reason Why the action is being done
			@param result_type What kind of results you want
			@param notify_scheduler Should the schedd notify the
 			controlling scheduler for this job?
			@return ClassAd containing results of this action, or NULL
			if we couldn't get any results.  The caller must delete
			this ClassAd when they are done with the results.
		*/
	ClassAd* removeXJobs( const char* constraint, const char* reason,
						  CondorError * errstack,
						  action_result_type_t result_type = AR_TOTALS,
						  bool notify_scheduler = true );

		/** Release all jobs that match the given constraint.
			Set ATTR_RELEASE_REASON to the given reason.
			@param constraint What jobs to act on
			@param reason Why the action is being done
			@param result_type What kind of results you want
			@param notify_scheduler Should the schedd notify the
 			controlling scheduler for this job?
			@return ClassAd containing results of this action, or NULL
			if we couldn't get any results.  The caller must delete
			this ClassAd when they are done with the results.
		*/
	ClassAd* releaseJobs( const char* constraint, const char* reason,
						  CondorError * errstack,
						  action_result_type_t result_type = AR_TOTALS,
						  bool notify_scheduler = true );

		/** Hold all jobs specified in the given StringList.  The list
			should contain a comma-seperated list of cluster.proc job
			ids.  Also, set ATTR_HOLD_REASON to the given reason. 
			@param constraint What jobs to act on
			@param reason Why the action is being done
			@param result_type What kind of results you want
			@param notify_scheduler Should the schedd notify the
 			controlling scheduler for this job?
			@return ClassAd containing results of this action, or NULL
			if we couldn't get any results.  The caller must delete
			this ClassAd when they are done with the results.
		*/
	ClassAd* holdJobs( StringList* ids, const char* reason,
					   CondorError * errstack,
					   action_result_type_t result_type = AR_LONG,
					   bool notify_scheduler = true );

		/** Remove all jobs specified in the given StringList.  The
			list should contain a comma-seperated list of cluster.proc
			job ids.  Also, set ATTR_REMOVE_REASON to the given
			reason. 
			@param constraint What jobs to act on
			@param reason Why the action is being done
			@param result_type What kind of results you want
			@param notify_scheduler Should the schedd notify the
 			controlling scheduler for this job?
			@return ClassAd containing results of this action, or NULL
			if we couldn't get any results.  The caller must delete
			this ClassAd when they are done with the results.
		*/
	ClassAd* removeJobs( StringList* ids, const char* reason,
						 CondorError * errstack,
						 action_result_type_t result_type = AR_LONG,
						 bool notify_scheduler = true );

		/** Force the local removal of jobs in the X state specified
			in the given StringList, regardless of whether they've
			been successfully removed remotely.  The list should
			contain a comma-seperated list of cluster.proc job ids.
			Also, set ATTR_REMOVE_REASON to the given reason.
			@param constraint What jobs to act on
			@param reason Why the action is being done
			@param result_type What kind of results you want
			@param notify_scheduler Should the schedd notify the
 			controlling scheduler for this job?
			@return ClassAd containing results of this action, or NULL
			if we couldn't get any results.  The caller must delete
			this ClassAd when they are done with the results.
		*/
	ClassAd* removeXJobs( StringList* ids, const char* reason,
						  CondorError * errstack,
						  action_result_type_t result_type = AR_LONG,
						  bool notify_scheduler = true );

		/** Release all jobs specified in the given StringList.  The
			list should contain a comma-seperated list of cluster.proc
			job ids.  Also, set ATTR_RELEASE_REASON to the given
			reason.
			@param constraint What jobs to act on
			@param reason Why the action is being done
			@param result_type What kind of results you want
			@param notify_scheduler Should the schedd notify the
 			controlling scheduler for this job?
			@return ClassAd containing results of this action, or NULL
			if we couldn't get any results.  The caller must delete
			this ClassAd when they are done with the results.
		*/
	ClassAd* releaseJobs( StringList* ids, const char* reason,
						  CondorError * errstack,
						  action_result_type_t result_type = AR_LONG,
						  bool notify_scheduler = true );


		/** Vacate all jobs specified in the given StringList.  The list
			should contain a comma-seperated list of cluster.proc job
			ids.
			@param ids What jobs to act on
			@param vacate_type Graceful or fast vacate?
			@param result_type What kind of results you want
			@param notify_scheduler Should the schedd notify the
 			controlling scheduler for this job?
			@return ClassAd containing results of this action, or NULL
			if we couldn't get any results.  The caller must delete
			this ClassAd when they are done with the results.
		*/
	ClassAd* vacateJobs( StringList* ids, VacateType vacate_type,
						 CondorError * errstack,
						 action_result_type_t result_type = AR_LONG,
						 bool notify_scheduler = true );

		/** Vacate all jobs that match the given constraint.
			@param constraint What jobs to act on
			@param vacate_type Graceful or fast vacate?
			@param result_type What kind of results you want
			@param notify_scheduler Should the schedd notify the
 			controlling scheduler for this job?
			@return ClassAd containing results of this action, or NULL
			if we couldn't get any results.  The caller must delete
			this ClassAd when they are done with the results.
		*/
	ClassAd* vacateJobs( const char* constraint, VacateType vacate_type,
						 CondorError * errstack,
						 action_result_type_t result_type = AR_TOTALS,
						 bool notify_scheduler = true );


		/** Request the schedd to initiate a negoitation cycle.
			The request is sent via a SafeSock (UDP datagram).
			@return true on success, false on failure.
		*/
	bool reschedule();

	bool spoolJobFiles(int JobAdsArrayLen, ClassAd* JobAdsArray[], CondorError * errstack);

	bool receiveJobSandbox(const char* constraint, CondorError * errstack);

	bool updateGSIcredential(const int cluster, const int proc, 
		const char* path_to_proxy_file, CondorError * errstack);

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
			@param action_str String describing the action
			@param constraint Constraint to operate on, or NULL
			@param ids StringList of ids to operate on, or NULL
			@param reason A string describing what we're doing
			@param reason_attr_name Attribute name for the reason
			@param result_type What kind of results you want
			@param notify_scheduler Should the schedd notify the
 			controlling scheduler for this job?
			@return ClassAd containing results of this action, or NULL
			if we couldn't get any results.  The caller must delete
			this ClassAd when they are done with the results.
		*/
	ClassAd* actOnJobs( JobAction action, const char* action_str, 
						const char* constraint, StringList* ids, 
						const char* reason, const char* reason_attr,
						action_result_type_t result_type,
						bool notify_scheduler, CondorError * errstack );

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

	int numError( void ) { return ar_error; };
	int numSuccess( void ) { return ar_success; };
	int numNotFound( void ) { return ar_not_found; };
	int numBadStatus( void ) { return ar_bad_status; };
	int numAlreadyDone( void ) { return ar_already_done; };
	int numPermissionDenied( void ) { return ar_permission_denied; }; 

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
