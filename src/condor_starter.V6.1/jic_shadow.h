/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-2002 CONDOR Team, Computer Sciences Department, 
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

#if !defined(_CONDOR_JIC_SHADOW_H)
#define _CONDOR_JIC_SHADOW_H

#include "job_info_communicator.h"
#include "condor_ver_info.h"
#include "file_transfer.h"
#include "io_proxy.h"

#include "condor_daemon_client.h"

/** The base class of JobInfoCommunicator that knows how to talk to a
	remote condor_shadow.  this is where we deal with sending any
	shadow RSCs, FileTransfer, etc.
*/

class JICShadow : public JobInfoCommunicator
{
public:
		/// Constructor
	JICShadow( char* shadow_sinful );

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


		// // // // // // // // // // // //
		// Information about the job 
		// // // // // // // // // // // //

		/// Total bytes sent by this job 
	float bytesSent( void );

		/// Total bytes received by this job 
	float bytesReceived( void );


		// // // // // // // // // // // //
		// Job execution and state changes
		// // // // // // // // // // // //

		/** All jobs have been spawned by the starter.  Start a
			timer to update the shadow periodically.
		 */
	void allJobsSpawned( void );

		/** The starter has been asked to suspend.  Suspend file
			transfer activity and notify the shadow.
		*/
	void Suspend( void );

		/** The starter has been asked to continue.  Resume file
			transfer activity and notify the shadow.
		*/
	void Continue( void );

		/** The last job this starter is controlling has exited.  Stop
			the timer for updating the shadow, initiate the final file
			transfer, if needed.
		*/
	void allJobsDone( void );

		/** The last job this starter is controlling has been
   			completely cleaned up.  We don't care, since we just wait
			for the shadow to tell the startd to tell us to go away. 
		*/
	void allJobsGone( void ) {};

		/** The starter has been asked to shutdown fast.  Disable file
			transfer, since we don't want that on fast shutdowns.
			Also, set a flag so we know we were asked to vacate. 
		 */
	void gotShutdownFast( void );


		// // // // // // // // // // // //
		// Notfication to the shadow
		// // // // // // // // // // // //

		/** Send RSC to the shadow to tell it the job is about to
			spawn.
		 */
	void notifyJobPreSpawn( void );

		/** Notify the shadow that the job exited
			@param exit_status The exit status from wait()
			@param reason The Condor-defined exit reason
			@param user_proc The UserProc that was running the job
		*/
	bool notifyJobExit( int exit_status, int reason,
						UserProc* user_proc );

	bool notifyStarterError( const char* err_msg, bool critical );



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


private:

		// // // // // // // // // // // //
		// Private helper methods
		// // // // // // // // // // // //

		/** Publish information into the given classad for updates to
			the shadow
			@param ad ClassAd pointer to publish into
			@return true if success, false if failure
		*/ 
	bool publishUpdateAd( ClassAd* ad );

		/// Start a timer for the periodic update to the shadow
	void startUpdateTimer( void );

		/** Send an update ClassAd to the shadow.  The "insure_update"
			just means do we make sure the update gets there.  It has
			nothing to do with the "insure" memory analysis tool.
			@param update_ad Update ad to use if you've already got the info 
			@param insure_update Should we insure the update gets there?
			@return true if success, false if failure
		*/
	bool updateShadow( ClassAd* update_ad, bool insure_update = false );

		/** Function to be called periodically to update the shadow.
			We can't just register a timer to call UpdateShadow()
			directly, since DaemonCore isn't passing any args to timer
			handlers, and therefore, it doesn't know it's supposed to
			honor the default argument we specified above.  So, we use
			this seperate function to register for the periodic
			updates, and this ensures that we use the UDP version of
			UpdateShadow().  This returns an int just to keep
			DaemonCore happy about the types.
			@return TRUE on success, FALSE on failure
		*/
	int periodicShadowUpdate( void );

		/** Read all the relevent attributes out of the job ad and
			decide if we need to transfer files.  If so, instantiate a
			FileTransfer object, start the transfer, and return true.
			If we don't have to transfer anything, return false.
			@return true if transfer was begun, false if not
		*/
	bool beginFileTransfer( void );

		/// Callback for when the FileTransfer object is done
	int transferCompleted(FileTransfer *);

		/// Do the RSC to get the job classad from the shadow
	bool getJobAdFromShadow( void );

		/** Initialize information about the shadow's version and
			sinful string from the job ad.  Also, try to initialize
			our ShadowVersion object.  If there's no shadow version,
			we leave our ShadowVersion NULL.  If we know the version,
			we instantiate a CondorVersionInfo object so we can
			perform checks on the version in the various places in the
			starter where we need to know this for compatibility.
		*/
	void initShadowInfo( void );

		/** Register some important information about ourself that the
			shadow might need.
			@return true on success, false on failure
		*/
	virtual	bool registerStarterInfo( void );
	
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

		/** Since the logic for getting the std filenames out of the
			job ad and munging them are identical for all 3, just use
			a helper to avoid duplicating code...
			@param attr_name The ClassAd attribute name to lookup
			@return a strdup() allocated string for the filename we 
			        care about, or NULL if it's not in the job ad.
		*/
	char* getJobStdFile( const char* attr_name );

		/// If the job ad says so, initialize our IO proxy
	bool initIOProxy( void );

		/** Compare our own UIDDomain vs. where the job came from.
			We check in the given job ClassAd for ATTR_UID_DOMAIN, and
			compare that to info we have about our remote shadow.
			@return true if they match, false if not
		*/
	bool sameUidDomain( void );

		// // // // // // // //
		// Private Data Members
		// // // // // // // //

	DCShadow* shadow;

		/** The version of the shadow if known; otherwise NULL */
	CondorVersionInfo* shadow_version;

		/// "sinful string" of our shadow 
	char* shadow_addr;

	IOProxy io_proxy;

	FileTransfer *filetrans;

	/// if true, transfer files at vacate time (in addtion to job exit)
	bool transfer_at_vacate;

	bool wants_file_transfer;

		/// timer id for periodically sending info on job to Shadow
	int shadowupdate_tid;

	char* uid_domain;
	char* fs_domain;

};


#endif /* _CONDOR_JIC_SHADOW_H */
