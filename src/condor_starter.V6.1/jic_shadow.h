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


#if !defined(_CONDOR_JIC_SHADOW_H)
#define _CONDOR_JIC_SHADOW_H

#include "job_info_communicator.h"
#include "condor_ver_info.h"
#include "file_transfer.h"
#include "io_proxy.h"

#include "condor_daemon_client.h"

#include "file_transfer_constants.h"
#include "file_transfer_functions.h"

/** The base class of JobInfoCommunicator that knows how to talk to a
	remote condor_shadow.  this is where we deal with sending any
	shadow RSCs, FileTransfer, etc.
*/

class JICShadow : public JobInfoCommunicator
{
public:
		/// Constructor
	JICShadow( const char* shadow_name );

		/// Destructor
	~JICShadow();

		// // // // // // // // // // // //
		// Initialization
		// // // // // // // // // // // //

		/// Initialize ourselves
	bool init( void );

		/// Read anything relevent from the config file
	void config( void );

		/// If needed, transfer files.
	void setupJobEnvironment( void );
	void setupJobEnvironment_part2(void);

	void newSetupJobEnvironment( void );

	bool streamInput();
	bool streamOutput();
	bool streamError();

		// // // // // // // // // // // //
		// Information about the job 
		// // // // // // // // // // // //

		/// Total bytes sent by this job 
	uint64_t bytesSent( void );

		/// Total bytes received by this job 
	uint64_t bytesReceived( void );

		/** Since the logic for getting the std filenames out of the
			job ad and munging them are identical for all 3, just use
			a helper to avoid duplicating code.  If we're not
			transfering files, we may need to use an alternate
			attribute (see comment for initStdFiles() for more
			details). 
			@param attr_name The ClassAd attribute name to lookup
			@return a strdup() allocated string for the filename we 
			        care about, or NULL if it's not in the job ad.
		*/
	char* getJobStdFile( const char* attr_name );

		// // // // // // // // // // // //
		// Job Actions
		// // // // // // // // // // // //

	virtual bool holdJob( const char*, int hold_reason_code, int hold_reason_subcode );

		/** 
		 * JICShadow does not need to implement these 
		 * at this time
		 **/ 
	virtual bool removeJob( const char* ) { return ( false ); }
	virtual bool terminateJob( const char* ) { return ( false ); }
	virtual bool requeueJob( const char* ) { return ( false ); }

		// // // // // // // // // // // //
		// Job execution and state changes
		// // // // // // // // // // // //

		/** The starter has been asked to suspend.  Suspend file
			transfer activity and notify the shadow.
		*/
	void Suspend( void );

		/** The starter has been asked to continue.  Resume file
			transfer activity and notify the shadow.
		*/
	void Continue( void );

		/** All the jobs are done. We need to send an update ad to the
		 *  shadow before starting the transfer of output files.
		 */
	bool allJobsDone( void );

		/** Once all the jobs are done, and after the optional
			HOOK_JOB_EXIT has returned, initiate the file transfer.
			If you call this, and transient_failure is not true, you
			MUST use transferOutputMopUp() afterwards to handle
			problems the file transfer may have had.
		*/
	bool transferOutput( bool &transient_failure );
	bool realTransferOutput( bool &transient_failure );
	bool nullTransferOutput( bool &transient_failure );

		/** After transferOutput returns, we need to handle what happens
			if the transfer actually failed. This call is separate from the
			one above since it allows us to talk to the shadow in the time
			between the failure of the transfer and the disconnection of the
			shadow when the starter notifies it of the file transfer error.
			This call will put the job on hold and cause the shadow to
			disconnect from the starter if something went wrong. If 
			the file transfer went right, then it is a noop.
		*/
	bool transferOutputMopUp( void );

		/** The last job this starter is controlling has been
   			completely cleaned up.  We don't care, since we just wait
			for the shadow to tell the startd to tell us to go away. 
		*/
	void allJobsGone( void );

		/** The starter has been asked to shutdown fast.  Disable file
			transfer, since we don't want that on fast shutdowns.
			Also, set a flag so we know we were asked to vacate. 
		 */
	void gotShutdownFast( void );

		/** Someone is attempting to reconnect to this job.
		 */
	int reconnect( ReliSock* s, ClassAd* ad );

		/// Disconnect from the shadow - jic virtual method handler
	void disconnect();

		// // // // // // // // // // // //
		// Notfication to the shadow
		// // // // // // // // // // // //

		/** Send RSC to the shadow to tell it the job is about to
			spawn.
		 */
	void notifyJobPreSpawn( void );

	void notifyExecutionExit( void );
	bool notifyGenericEvent( const ClassAd & event, int & rv );

	virtual bool genericRequestGuidance( const ClassAd & request, GuidanceResult & rv, ClassAd & guidance );

		/** Notify the shadow that the job exited. This will not only
			update the job ad with the termination information of the job,
			but also write terminate events into the job log, and do the
			rest of the machinery for when a job finishes and is likely to
			leave the queue.

			@param exit_status The exit status from wait()
			@param reason The Condor-defined exit reason
			@param user_proc The UserProc that was running the job
		*/
	bool notifyJobExit( int exit_status, int reason,
						UserProc* user_proc );

		/** The sutble distinction between notifyJobTermination() and
			notifyJobExit() is that notifyJobTermination() will only update the
			U_TERMINATE attributes in the job ad on the submit machine based
			upon the exit_status and reason and do nothing else! An example
			use is when a job coredumps and due to this causes the
			file transfer to fail since some files might not exist if
			directly specified in transfer_output_files. This allows the
			job ad to contain the exit status of the job while it is on hold.
			Basically this is used to preserve the termination of the job in
			the face of Condor then messing stuff up terribly around it.
		*/
	int notifyJobTermination( UserProc* user_proc );

		/** Tell the shadow to log an error event in the user log.
			@param err_msg A desription of the error
			@param critical True if the shadow should exit
			@param hold_reason_code If non-zero, put job on hold with this code
			@param hold_reason_subcode The sub-code to record
		 */
	bool notifyStarterError( const char* err_msg, bool critical,
	                         int hold_reason_code, int hold_reason_subcode);


		/**
		   Send a periodic update ClassAd to the shadow.

		   @param update_ad Update ad to use if you've already got the info
		   @return true if success, false if failure
		*/
	virtual bool periodicJobUpdate(ClassAd* update_ad = NULL);


		// // // // // // // // // // // //
		// Misc utilities
		// // // // // // // // // // // //

		/** Check to see if we're configured to use file transfer or
			not.  If so, see if transfer_output_files is set.  If so,
			append the given filename to the list, so that we try to
			transfer it back, too.  If we're not using file transfer,
			the only way a remote shadow would get this file back is
			if we're on a shared filesystem, in which case, we don't
			have to do anything special, anyway.
			@param filename File to add to the job's output list 
		*/
	void addToOutputFiles( const char* filename );

		/** Make sure the given filename will be excluded from the
			list of files that the job sends back to the submitter.
			If the file has already been added to the output list,
			it is removed and added to the exception list; otherwise,
			it is added to the exception list directly.
			@param filename File to remove from the job's output list 
		*/
	void removeFromOutputFiles( const char* filename );

		/** Send modified files in a job working directory 
		    to shadow by using file transfer
		*/
	bool uploadWorkingFiles(void);

		/** Send checkpoint files to shadow */
	bool uploadCheckpointFiles(int checkpointNumber);

		/* Update Job ClassAd with checkpoint info and log it */
	void updateCkptInfo(void);

		/* Record an attribute to update */
	bool recordDelayedUpdate( const std::string &name, const classad::ExprTree &expr );

		/* Return an attribute from the combination of the delayed ad and the starter */
	std::unique_ptr<classad::ExprTree> getDelayedUpdate( const std::string &name );

	virtual bool wroteChirpConfig() { return m_wrote_chirp_config; }
	virtual const std::string chirpConfigFilename() { return m_chirp_config_filename; }

private:

	int handleFileTransferCommand( Stream * s );
	FileTransferFunctions::GoAheadState gas;
    void _remove_files_from_output();

	void updateShadowWithPluginResults( const char * which );

	void recordSandboxContents( const char * filename );

		// // // // // // // // // // // //
		// Private helper methods
		// // // // // // // // // // // //

		/** Publish information into the given classad for updates to
			the shadow
			@param ad ClassAd pointer to publish into
			@return true if success, false if failure
		*/
	bool publishUpdateAd( ClassAd* ad );

	bool publishJobExitAd( ClassAd* ad );

	// Should only be called from publish[Update|JobExit]Ad().
	bool publishStartdUpdates( ClassAd* ad );

		/** Send an update ClassAd to the shadow.
			@param update_ad Update ad to use if you've already got the info 
			@return true if success, false if failure
		*/
	bool updateShadow( ClassAd* update_ad );

		/** Send an update ClassAd to the startd.
			@param ad Update ad
		 */
	void updateStartd( ClassAd *ad, bool final_update );

		/** Read all the relevent attributes out of the job ad and
			decide if we need to transfer files.  If so, instantiate a
			FileTransfer object, start the transfer, and return true.
			If we don't have to transfer anything, return false.
			@return true if transfer was begun, false if not
		*/
	bool beginFileTransfer( void );
	bool beginNullFileTransfer( void );
	bool beginRealFileTransfer( void );

		/// Callback for when the FileTransfer object is done or has status
	int transferStatusCallback(FileTransfer * ftrans) {
		if (ftrans->GetInfo().type == FileTransfer::TransferType::DownloadFilesType) {
			return transferInputStatus(ftrans);
		}
		return 1;
	}
	int transferInputStatus(FileTransfer *);

		/// Do the RSC to get the job classad from the shadow
	bool getJobAdFromShadow( void );

		/// Get the machine classad from the given stream
	bool receiveMachineAd( Stream *stream );

		/// Get the job execution overlay classad from the given stream
	bool receiveExecutionOverlayAd(Stream* stream);

		/** Initialize information about the shadow's version and
			sinful string from the given ClassAd.  At startup, we just
			pass the job ad, since that should have everything in it.
			But on reconnect, we call this with the request ad.  Also,
			try to initialize our ShadowVersion object.  If there's no
			shadow version, we leave our ShadowVersion NULL.  If we
			know the version, we instantiate a CondorVersionInfo
			object so we can perform checks on the version in the
			various places in the starter where we need to know this
			for compatibility.
		*/
	void initShadowInfo( ClassAd* ad );

		/** Register some important information about ourself that the
			shadow might need.
			@return true on success, false on failure
		*/
	virtual	bool registerStarterInfo( void );
	
		/** All the attributes the shadow cares about that we send via
			a ClassAd is handled in this method, so that we can share
			the code between registerStarterInfo() and when we're
			replying to accept an attempted reconnect.
		*/ 
	void publishStarterInfo( ClassAd* ad );

		/** Initialize the priv_state code with the appropriate user
			for this job.  This function deals with all the logic for
			checking UID_DOMAIN compatibility, SOFT_UID_DOMAIN
			support, and so on.
			@return true on success, false on failure
		*/
	bool initUserPriv( void );

		/** Initialize our version of important information for this
			job which the starter will want to know.  This should
			init the following: orig_job_name, job_input_name, 
			job_output_name, job_error_name, and job_iwd.
			@return true on success, false on failure */
	bool initJobInfo( void );

		/** Initialize the file-transfer-related properties of this
			job from the ClassAd.  This procedure is somewhat
			complicated, and involves some backwards compatibility
			cruft, too.  All of the code paths share some common code,
			too.  So, everything is split up into a few helper
			functions to maximize clarity, keep the length of
			individual functions reasonable, and to avoid code
			duplication.  

			initFileTransfer() is responsible for looking up
			ATTR_SHOULD_TRANSFER_FILES, which specifies the
			file transfer behavior.  It decides what
			to do based on what it says, and call the appropriate
			helper method, either initNoFileTransfer() or
			initWithFileTransfer().  If it's defined to "IF_NEEDED",
			we compare the FileSystemDomain of the job with our local
			value, using the sameFSDomain() helper, and if there's a
			match, we don't setup file transfer.

			@return true on success, false if there are fatal errors
		*/
	bool initFileTransfer( void );

		/** If we know for sure we do NOT want file transfer, we call
			this method.  It sets the appropriate flags to turn off
			file transfer code in the starter, and uses the original
			value of ATTR_JOB_IWD to initialize the job's IWD and the
			standard files (STDIN, STDOUT, and STDERR), using the
			initStdFiles() method.
			@return true on success, false if there are fatal errors
		*/
	bool initNoFileTransfer( void );

		/** If we think we want file transfer, we call this method.
			It tries to find the ATTR_WHEN_TO_TRANSFER_OUTPUT
			attribute and uses that to figure out if we should send
			back output files when we're evicted, or only if the job
			exits on its own. Once we know that we're
			doing a file transfer, we initialize the job's IWD to the
			starter's temporary execute directory, and can then call
			the initStdFiles() method to initalize STDIN, STDOUT, and
			STDERR.
			@return true on success, false if there are fatal errors
		*/			
	bool initWithFileTransfer( void );

		/** This method is used whether or not we're doing a file
			transfer to initialize the valid full paths to use for
			STDIN, STDOUT, and STDERR.  The "job_iwd" data member of
			this object must be filled in before this can be called.  
			For the output files, if they contain full pathnames,
			condor_submit now stores the original values in alternate
			attribute names and puts a temporary value in the real
			attributes so that things work for file transfer.
			However, if we're not transfering files, we need to now
			use these alternate names, since that's where the user
			really wants the output, and we want to access them
			directly.  So, for STDOUT, and STDERR, we also pass in the
			alternate attribute names to the underlying helper method,
			getJobStdFile(). 
			@return at this time, this method always returns true
		*/
	bool initStdFiles( void );

		/// If the job ad says so, initialize our IO proxy
	bool initIOProxy( void );

		// If we are supposed to specially create a security session
		// for file transfer and reconnect, do it.
	void initMatchSecuritySession();

		/// If the job ad says so, acquire user credentials
	bool initUserCredentials();

		/** Compare our own UIDDomain vs. where the job came from.  We
			check in the job ClassAd for ATTR_UID_DOMAIN and compare
			it to info we have about the shadow and the local machine.
			This is used to help initialize the priv-state code.
			@return true if they match, false if not
		*/
	bool sameUidDomain( void );

		/** Compare our own FileSystemDomain vs. where the job came from.
			We check in the job ClassAd for ATTR_FILESYSTEM_DOMAIN, and
			compare that to our locally defined value.  This is used
			to help figure out if we should do file-transfer or not.
			@return true if they match, false if not
		*/
	bool sameFSDomain( void );

		// Are we using file transfer.  Overridden from parent classes version.
	bool usingFileTransfer( void );

		// The shadow is feeding us a new proxy. Override from parent
	bool updateX509Proxy(int cmd, ReliSock * s);

	// leftover from gl_exec? this does nothing now..
	//void setX509ProxyExpirationTimer();

		// The proxy is about to expire, do something!
	void proxyExpiring();

	bool refreshSandboxCredentialsKRB();
	void refreshSandboxCredentialsKRB_from_timer( int /* timerID */ ) { (void)refreshSandboxCredentialsKRB(); }
	bool refreshSandboxCredentialsOAuth();
	void refreshSandboxCredentialsOAuth_from_timer( int /* timerID */ ) { (void)refreshSandboxCredentialsOAuth(); }
	void verifyXferProgressing(int /*timerid*/);

	bool shadowDisconnected() const { return syscall_sock_lost_time > 0; };

		// // // // // // // //
		// Private Data Members
		// // // // // // // //

	DCShadow* shadow;

		/** The version of the shadow if known; otherwise NULL */
	CondorVersionInfo* shadow_version;

		/// timer id of the credential checking timer
	int m_refresh_sandbox_creds_tid;
	time_t m_sandbox_creds_last_update;

		/// timer id of the proxy expiration timer
	int m_proxy_expiration_tid;

		/// hostname (or whatever the startd gave us) of our shadow 
	char* m_shadow_name;

	classad::ClassAd m_delayed_updates;
	std::vector<std::string> m_delayed_update_attrs;
	IOProxy io_proxy;

	FileTransfer *filetrans;
	bool m_ft_rval;
	FileTransfer::FileTransferInfo m_ft_info;
	bool m_did_transfer;


		// specially made security sessions if we are doing
		// SEC_ENABLE_MATCH_PASSWORD_AUTHENTICATION
	char *m_filetrans_sec_session;
	char *m_reconnect_sec_session;

	/// if true, transfer files at vacate time (in addtion to job exit)
	// but only if we get to the point of starting the job
	bool transfer_at_vacate;
	bool m_job_setup_done;

	bool wants_file_transfer;
	bool wants_x509_proxy;

	char* uid_domain;
	char* fs_domain;
	bool trust_uid_domain;
	bool trust_local_uid_domain;

	std::string m_chirp_config_filename;
	bool m_wrote_chirp_config;

		/** A flag to keep track of the case where we were trying to
			cleanup our job but we discovered that we were
			disconnected from the shadow.  This way, if there's a
			successful reconnect, we know we can try to clean up the
			job again...
		*/
	bool job_cleanup_disconnected;

		/** A flag to keep track of if the syscall_sock is registered
			with daemonCore for read.  We do this so we notice asap
			when this socket is closed, ie due to failed TCP keep alives.
		*/
	bool syscall_sock_registered;
		/** handler for read callbacks on syscall sock, to notice if it closes */
	int syscall_sock_handler(Stream *s);
		/** epoch time when the syscall_sock got closed (shadow disappeared) */
	time_t syscall_sock_lost_time;
		/** timer id of timer to invoke job_lease_expired() when syscall_sock closed */
	int syscall_sock_lost_tid;
		/** invoked when job lease expired - exits w/ well known status */
	void job_lease_expired( int timerID = -1 ) const;
		/** must be invoked whenever our syscall_sock is reconnected */
	void syscall_sock_reconnect();
		/** must be invoked whenever we notice our syscall_sock is borked */
	void syscall_sock_disconnect();

	Stream *m_job_startd_update_sock;

		/** A list of output files that have been dynamically added
		    (e.g. a core file dumped by the job)
		*/
	std::vector<std::string> m_added_output_files;

		/** A list of files that should NOT be transfered back to the
			job submitter. (e.g. the job's executable itself)
		*/
	std::vector<std::string> m_removed_output_files;

		/** A list of attributes to copy from the update ad to the job
			ad every time we update the shadow.
		*/
	bool m_job_update_attrs_set;
	std::vector<std::string> m_job_update_attrs;

	time_t file_xfer_last_alive_time = 0;
	int    file_xfer_last_alive_tid = 0;

	// Glorious hack.
	bool transferredFailureFiles = false;
};


#endif /* _CONDOR_JIC_SHADOW_H */
