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


#ifndef BASESHADOW_H
#define BASESHADOW_H

#include "condor_common.h"
#include "condor_daemon_core.h"
#include "condor_classad.h"
#include "shadow_user_policy.h"
#include "write_user_log.h"
#include "exit.h"
#include "internet.h"
#include <qmgr_job_updater.h>
#include "condor_update_style.h"
#include "file_transfer.h"

/* Forward declaration to prevent loops... */
class RemoteResource;

/** This is the base class for the various incarnations of the Shadow.<p>

	If you want to change something related to the shadow, 
	make sure you know how these classes interact.  If it's a general
	shadow thing, you very well may want to add it to this class.  
	However, if it is specific to one remote resource, you want to
	look at RemoteResource.  If it is single, MPI
	specific, make the change in that derived class. <p>

	More to come...<p>

	This class has some pure virtual functions, so it's an abstract
	class.  You therefore can't instantiate it; you must instantiate
	one of its dervide classes.<p>

	Based heavily on code by Todd Tannenbaum.
	@see RemoteResource
	@author Mike Yoder
*/
class BaseShadow : public Service 
{
 public:
		/// Default constructor
	BaseShadow();

		/// Destructor, it's virtual
	virtual ~BaseShadow();

		/** This is the basic initialization function.

			It does the following:
			<ul>
			 <li>Puts the args into class data members
			 <li>Stores the classAd, checks its info, and pulls
			     everything we care about out of the ad and into class
			     data members.
			 <li>calls config()
			 <li>calls initUserLog()
			 <li>registers handleJobRemoval on SIGUSR1
			</ul>
			It should be called right after the constructor.
			@param job_ad The ClassAd for this job.
			@param schedd_addr The sinful string of the schedd
		*/
	void baseInit( ClassAd *job_ad, const char* schedd_addr, const char *xfer_queue_contact_info );

		/** Everyone must make an init with a bunch of parameters.<p>
			This function is <b>pure virtual</b>.
		 */
	virtual void init( ClassAd* job_ad, const char* schedd_addr, const char *xfer_queue_contact_info ) = 0;

		/** Shadow should spawn a new starter for this job.
			This function is <b>pure virtual</b>.
		 */
	virtual void spawn( void ) = 0;

	bool waitingToUpdateSchedd() const { return m_cleanup_retry_tid != -1; }

		/** Shadow should attempt to reconnect to a disconnected
			starter that might still be running for this job.  
			This function is <b>pure virtual</b>.
		 */
	virtual void reconnect( void ) = 0;

		/** Does the shadow support reconnect for this job? 
		 */
	virtual bool supportsReconnect( void ) = 0;

		/** Given our config parameters, figure out how long we should
			delay until our next attempt to reconnect.
		*/
	int nextReconnectDelay( int attempts ) const;

		/**	Called by any part of the shadow that finally decides the
			reconnect has completely failed, we should give up, try
			one last time to release the claim, write a UserLog event
			about it, and exit with a special status. 
			@param reason Why we gave up (for UserLog, dprintf, etc)
		*/
	PREFAST_NORETURN
	void reconnectFailed( const char* reason ); 

	virtual bool shouldAttemptReconnect(RemoteResource *) { return true;};

		/** Here, we param for lots of stuff in the config file.
		*/
	virtual void config();

		/** Everyone should be able to shut down.<p>
			@param reason The reason the job exited (JOB_BLAH_BLAH)
		 */
	virtual void shutDown( int reason, const char* reason_str, int reason_code=0, int reason_subcode=0 );
		/** Forces the starter to shutdown fast as well.<p>
			@param reason The reason the job exited (JOB_BLAH_BLAH)
		 */
	virtual void shutDownFast( int reason, const char* reason_str, int reason_code=0, int reason_subcode=0 );

		/** Graceful shutdown method.  This is virtual so each kind of
			shadow can do the right thing.
		 */
	virtual void gracefulShutDown( void ) = 0;

		/** Get name of resource job is running on.  This is for
			informational purposes only.  It may be empty if there
			is no appropriate answer (and function will return false).
		*/
	virtual bool getMachineName( std::string &machineName );

		/** Put this job on hold, if requested, notify the user about
			it.  This function does _not_ exit.  Use holdJobAndExit()
			instead to exit with appropriate status so that the
			schedd actually puts the job on hold.
			This uses the virtual cleanUp() method to take care of any
			universe-specific code before we exit.
			@param reason String describing why the job is held
		*/
	virtual void holdJob( const char* reason, int hold_reason_code, int hold_reason_subcode );

		/** Put this job on hold, if requested, notify the user about
			it and exit with the appropriate status so that the
			schedd actually puts the job on hold.
			This uses the virtual cleanUp() method to take care of any
			universe-specific code before we exit.
			@param reason String describing why the job is held
		*/
	void holdJobAndExit( const char* reason, int hold_reason_code, int hold_reason_subcode );

		/** Remove the job from the queue, if requested, notify the
			user about it, and exit with the appropriate status so
			that the schedd actually removes the job.<p>
			This uses the virtual cleanUp() method to take care of any
			universe-specific code before we exit.
			@param reason String describing why the job is removed
		*/
	virtual void removeJob( const char* reason );

		/** The job exited, but we want to put it back in the job
			queue so it will run again.  If requested, notify the user about
			it, and exit with the appropriate status so that the
			schedd doesn't remove the job from the queue.<p>
			This uses the virtual cleanUp() method to take care of any
			universe-specific code before we exit.
			@param reason String describing why the job is requeued
		*/
	void requeueJob( const char* reason );

		/** The job exited, and it's ready to leave the queue.  If
			requested, notify the user about it, log a terminate
			event, update the job queue, etc.<p>
			This uses the virtual cleanUp() method to take care of any
			universe-specific code before we exit.
			The optional style parameter dictates if the job terminated of
			its own accord or if the shadow had been born with a 
			previously terminated job classad.
		*/
	void terminateJob( update_style_t kind = US_NORMAL );

		/** The job exited, but it _isn't_ ready to leave the queue.

			For example, the job segfaulted and didn't produce all of the
			output files specifically labeled in transfer_output_files, so
			this causes file transfer to fail which puts the job on hold
			(as of 7.4.4). However, the user would like to write periodic or
			exit policies about the job and for that we need to know exactly
			how the job exited. So this call places how the job exited into
			the jobad, but doesn't write any events about it.
		*/
	void mockTerminateJob( std::string exit_reason, bool exited_by_signal,
		int exit_code, int exit_signal, bool core_dumped );

		/** Set a timer to call terminateJob() so we retry
		    our attempts to update the job queue etc.
		*/
	void retryJobCleanup();

		/// DaemonCore timer handler to actually do the retry.
	void retryJobCleanupHandler( int timerID = -1 );

		/** The job exited but it's not ready to leave the queue.  
			We still want to log an evict event, possibly email the
			user, etc.  We want to update the job queue, but not with
			anything about how the job was killed.<p>
			This uses the virtual cleanUp() method to take care of any
			universe-specific code before we exit.
		*/
	void evictJob( int exit_reason, const char* reason_str, int reason_code=0, int reason_subcode=0 );
		/** It's possible to the shadow to initiate eviction, and in
			some cases that means we need to wait around for the starter
			to tell us what happened.
		*/
	virtual void exitAfterEvictingJob( int reason ) { DC_Exit( reason ); }
	virtual bool exitDelayed( int & /*reason*/ ) { return false; }

		/** The total number of bytes sent over the network on
			behalf of this job.
			Each shadow class should override this function and
			do whatever it needs to do.
			The default implementation in the base just returns
			zero bytes.
			@return number of bytes sent over the network.
		*/
	virtual uint64_t bytesSent() { return 0; }

		/** The total number of bytes received over the network on
			behalf of this job.
			Each shadow class should override this function and
			do whatever it needs to do.
			The default implementation in the base just returns
			zero bytes.
			@return number of bytes sent over the network.
		*/
	virtual uint64_t bytesReceived() { return 0; }

	virtual void getFileTransferStats(ClassAd &upload_file_stats, ClassAd &download_file_stats) = 0;
	ClassAd* updateFileTransferStats(const ClassAd& old_stats, const ClassAd &new_stats);
	virtual void getFileTransferStatus(FileTransferStatus &upload_status,FileTransferStatus &download_status) = 0;

	virtual int getExitReason( void ) = 0;

	virtual bool claimIsClosing( void ) = 0;

	static void CommitSuspensionTime(ClassAd *jobAd);

		/** Initializes the user log.  'Nuff said. 
		 */
	void initUserLog();

		/** Change to the 'Iwd' directory.  Send email if problem.
			@return 0 on success, -1 on failure.
		*/
	int cdToIwd();

		/** Remove this job.  This function is <b>pure virtual</b>.

		 */
	virtual int handleJobRemoval(int sig) = 0;

	/**
	 * Handle the situation where the job is to be suspended
	 */
	virtual int JobSuspend(int sig)=0;
	
	/**
	 * Handle the situation where the job is to be continued.
	 */
	virtual int JobResume(int sig)=0;
	
		/** Update this job.
		 */
	int handleUpdateJobAd(int sig);

		/// Returns the jobAd for this job
	ClassAd *getJobAd() { return jobAd; }
		/// Returns this job's cluster number
	int getCluster() const { return cluster; }
		/// Returns this job's proc number
	int getProc() const { return proc; }
		/// Returns this job's GlobalJobId string
	const char* getGlobalJobId() { return gjid; }
		/// Returns the schedd address
	char *getScheddAddr() { return scheddAddr; }
        /// Returns the current working dir for the job
    char const *getIwd() { return iwd.c_str(); }
        /// Returns the owner of the job - found in the job ad
    char const *getJobOwner() { return user_owner.c_str(); }
		/// Returns true if job requests graceful removal
	bool jobWantsGracefulRemoval();

		/// Called by EXCEPT handler to log to user log
	static void log_except(const char *msg);

	//set by pseudo_ulog() to suppress "Shadow exception!"
	bool exception_already_logged;

		/// Used by static and global functions to access shadow object
	static BaseShadow* myshadow_ptr;

		/** Method to process an update from the starter with info 
			about the job.  This is shared by both the daemoncore
			command handler and the CONDOR_register_job_info RSC. 
		 */
	virtual int updateFromStarterClassAd(ClassAd* update_ad) = 0;

	virtual int64_t getImageSize( int64_t & memory_usage, int64_t & rss, int64_t & pss ) = 0;

	virtual int64_t getDiskUsage( void ) = 0;

	virtual struct rusage getRUsage( void ) = 0;

	virtual bool setMpiMasterInfo( char* str ) = 0;

		/** Connect to the job queue and update all relevent
			attributes of the job class ad.  This checks our job
			classad to find any dirty attributes, and compares them
			against the lists of attribute names we care about.  
			@param type What kind of update we want to do
			@return true on success, false on failure
		*/
	bool updateJobInQueue( update_t type );

	// Called by updateJobInQueue() to generate file transfer events.
	virtual void recordFileTransferStateChanges( ClassAd * jobAd, ClassAd * ftAd ) = 0;

		/** Connect to the job queue and update one attribute */
	virtual bool updateJobAttr( const char *name, const char *expr, bool log=false );

		/** Connect to the job queue and update one integer attribute */
	virtual bool updateJobAttr( const char *name, int64_t value, bool log=false );

		/** Watch a job attribute for future updates */
	virtual void watchJobAttr( const std::string & );

		/** Do whatever cleanup (like killing starter(s)) that's
			required before the shadow can exit.
		*/
	virtual void cleanUp( bool graceful=false ) = 0;

		/** Did this shadow's job exit by a signal or not?  This is
			virtual since each kind of shadow will need to implement a
			different method to decide this. */
	virtual bool exitedBySignal( void ) = 0;

		/** If this shadow's job exited by a signal, what was it?  If
			not, return -1.  This is virtual since shadow will need to
			implement a different method to supply this
			information. */ 
	virtual int exitSignal( void ) = 0;

		/** If this shadow's job exited normally, what was the return
			code?  If it was killed by a signal, return -1.  This is
			virtual since shadow will need to implement a different
			method to supply this information. */
	virtual int exitCode( void ) = 0;

	WriteUserLog uLog;

	void evalPeriodicUserPolicy( void );

	const char* getCoreName( void );

	virtual void resourceBeganExecution( RemoteResource* rr ) = 0;

	virtual void resourceDisconnected( RemoteResource* rr ) = 0;

	virtual void resourceReconnected( RemoteResource* rr ) = 0;

		/** Start a timer to do periodic updates of the job queue for
			this job.
		*/
	void startQueueUpdateTimer( void );

	void publishShadowAttrs( ClassAd* ad );

	virtual void logDisconnectedEvent( const char* reason ) = 0;

	char const *getTransferQueueContactInfo() {return m_xfer_queue_contact_info.c_str();}

		/** True if attemping a reconnect from startup, i.e. if
			reconnecting based upon command-line flag -reconnect. 
			Used to determine if shadow exits with RECONNECT_FAILED
			or just with JOB_SHOULD_REQUEUE. */
	bool attemptingReconnectAtStartup;

	bool isDataflowJob = false;

	void logDataflowJobSkippedEvent();

 protected:

		/** Note that this is the base, "unexpanded" ClassAd for the job.
			If we're a regular shadow, this pointer gets copied into the
			remoteresource.  If we're an MPI job we expand it based on
			something like $(NODE) into each remoteresource. */
	ClassAd *jobAd;

	QmgrJobUpdater *job_updater;

		/// Are we trying to remove this job (condor_rm)?
	bool remove_requested;

	ShadowUserPolicy shadow_user_policy;

	double prev_run_bytes_sent;
	double prev_run_bytes_recvd;

	bool began_execution;

	void emailHoldEvent( const char* reason );

	void emailRemoveEvent( const char* reason );

	void logRequeueEvent( const char* reason );
	
	void removeJobPre( const char* reason ); 
	
		// virtual void emailRequeueEvent( const char* reason );

	void logTerminateEvent( int exitReason, update_style_t kind = US_NORMAL );

	void logEvictEvent( int exitReason, const std::string &reasonStr, int reasonCode, int reasonSubCode );

	virtual void logReconnectedEvent( void ) = 0;

	virtual void logReconnectFailedEvent( const char* reason ) = 0;

	virtual void emailTerminateEvent( int exitReason, update_style_t kind = US_NORMAL ) = 0;

	void startdClaimedCB(DCMsgCallback *cb);
	bool m_lazy_queue_update;

	ClassAd m_prev_run_upload_file_stats;
	ClassAd m_prev_run_download_file_stats;

 private:

	// private methods

		/** See if there's enough swap space for this shadow, and if  
			not, exit with JOB_NO_MEM.
		*/
	void checkSwap( void );

		/** Improve Hold/EvictReason string and codes before sending this info
			to the job classad in the schedd. */
	std::string improveReasonAttributes(const char* orig_reason_str, int  &reason_code, int &reason_subcode);

	// config file parameters
	int reconnect_ceiling;
	double reconnect_e_factor;
	bool m_RunAsNobody;

	// job parameters
	int cluster;
	int proc;
	char* gjid;
	std::string user_owner;
	std::string domain;
	std::string iwd;
	char *scheddAddr;
	char *core_file_name;
	std::string m_xfer_queue_contact_info;

		/// Timer id for the job cleanup retry handler.
	int m_cleanup_retry_tid;

		/// Number of times we have retried job cleanup.
	int m_num_cleanup_retries;

		/// Maximum number of times we will retry job cleanup.
	int m_max_cleanup_retries;

		/// How long to delay between attempts to retry job cleanup.
	int m_cleanup_retry_delay;

		// Insist on a fast shutdown of the starter?
	bool m_force_fast_starter_shutdown;

		// Has CommittedTime in the job ad been updated to reflect
		// job termination?
	bool m_committed_time_finalized;

		// This makes this class un-copy-able:
	BaseShadow( const BaseShadow& );
	BaseShadow& operator = ( const BaseShadow& );
};

extern void dumpClassad( const char*, ClassAd*, int );

// Register the shadow "exit status" for the previous job
// and restart this shadow with a new job.
// Returns false if no new job found.
extern bool recycleShadow(int previous_job_exit_reason);

// fix the update ad from the starter to work around starter bugs.
extern void fix_update_ad(ClassAd & update_ad);

extern BaseShadow *Shadow;

#endif

