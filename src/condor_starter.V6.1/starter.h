/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
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

#if !defined(_CONDOR_STARTER_H)
#define _CONDOR_STARTER_H

#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "list.h"
#include "os_proc.h"
#include "user_proc.h"
#include "io_proxy.h"
#include "NTsenders.h"
#include "condor_ver_info.h"
#include "file_transfer.h"
#include "condor_daemon_client.h"

extern ReliSock *syscall_sock;

/* thse are the remote system calls that the starter uses */

/** The starter class.  Basically, this class does some initialization
	stuff and manages a set of UserProc instances, each of which 
	represent a running job.

	@see UserProc
 */
class CStarter : public Service
{
public:
		/// Constructor
	CStarter();
		/// Destructor
	virtual ~CStarter();

		/** This is called at the end of main_init().  It calls Config(), 
			registers a bunch of signals, registers a reaper, makes 
			the starter's working dir and moves there, does a 
			register_machine_info remote syscall, sets resource 
			limits, then calls StartJob()
		    @param peer The sumbitting machine (sinful string)
		*/
	virtual bool Init(char peer[]);

		/** Params for "EXECUTE", "UID_DOMAIN", and "FILESYSTEM_DOMAIN".
		 */
	virtual void Config();

		/** Walk through list of jobs, call ShutDownGraceful on each.
			@return 1 if no jobs running, 0 otherwise 
		*/
	virtual int ShutdownGraceful(int);

		/** Walk through list of jobs, call ShutDownFast on each.
			@return 1 if no jobs running, 0 otherwise 
		*/
	virtual int ShutdownFast(int);

		/** For now, make a VanillaProc class instance and call
			StartJob on it.  Append it to the JobList.
		*/
	virtual bool StartJob();

		/** Call Suspend() on all elements in JobList */
	virtual int Suspend(int);

		/** Call Continue() on all elements in JobList */
	virtual int Continue(int);

		/** To do */
	virtual int PeriodicCkpt(int);

		/** Handles the exiting of user jobs.  If we're shutting down
			and there are no jobs left alive, we exit ourselves.
			@param pid The pid that died.
			@param exit_status The exit status of the dead pid
		*/
	virtual int Reaper(int pid, int exit_status);

		/** Return the Working dir */
	const char *GetWorkingDir() const { return WorkingDir; }

		/** Return a pointer to the version object for the Shadow  */
	CondorVersionInfo* GetShadowVersion() const { return ShadowVersion; }

	DCShadow* shadow;

	char* GetOrigJobName() { return &OrigJobName[0]; };

		/** Check to see if we're configured to transfer files or not.
			If so, see if transfer_output_files is set.  If so, append
			the given filename to the list, so that we try to transfer
			it back, too.  This is used when we find a core file to
			make sure we always transfer it.
			@param filename File to append to the transfer output list 
		*/
	void addToTransferOutputFiles( const char* filename );

		/// Start a timer for the periodic update to the shadow
	void startUpdateTimer( void );

protected:
	List<UserProc> JobList;
	List<UserProc> CleanedUpJobList;

private:

		// // // // // // // 
		// Private Methods
		// // // // // // // 

		/** Send an update ClassAd to the shadow.  The "insure_update"
			just means do we make sure the update gets there.  It has
			nothing to do with the "insure" memory analysis tool.
			@param insure_update Should we insure the update gets there?
			@return true if success, false if failure
		*/
	bool UpdateShadow( bool insure_update = false );

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
	int PeriodicShadowUpdate( void );

		/** Publish all attributes we care about for updating the
			shadow into the given ClassAd. 
			@param ad pointer to the classad to publish into
			@return true if success, false if failure
		*/
	bool PublishUpdateAd( ClassAd* ad );

		/** Initialize the priv_state code with the appropriate user
			for this job.  This function deals with all the logic for
			checking UID_DOMAIN compatibility, SOFT_UID_DOMAIN
			support, and so on.
			@return true on success, false on failure
		*/
	bool InitUserPriv( void );

		/** Initialize information about the Shadow from the job ad.
			Grab the shadow's sinful string and store that in
			ShadowAddr.  Also, try to initialize our ShadowVersion
			object.  If there's no shadow version, we leave our
			ShadowVersion NULL.  If we know the version, we
			instantiate a CondorVersionInfo object so we can perform
			checks on the version in the various places in the starter
			where we need to know this for compatibility.
		*/
	void InitShadowInfo( void );

		/** Compare our own UIDDomain vs. the submitting host.  We
			check in the given job ClassAd for ATTR_UID_DOMAIN.
			@return true if they match, false if not
		*/
	bool SameUidDomain( void );

		/** Perform an RSC to the shadow to register some important
			information about ourself that the shadow needs.  This
			checks the version of the shadow and sends the appropriate
			RSC to be backwards compatible, etc
			@return true on success, false on failure
		*/
	bool RegisterStarterInfo( void );

		/** Read all the relevent attributes out of the job ad and
			decide if we need to transfer files.  If so, instantiate a
			FileTransfer object, start the transfer, and return true.
			If we don't have to transfer anything, return false.
			@return true if transfer was begun, false if not
		*/
	bool BeginFileTransfer( void );

		/// Callback for when the FileTransfer object is done
    int TransferCompleted(FileTransfer *);


		// // // // // // // //
		// Private Data Members
		// // // // // // // //

		/** The version of the shadow if known; otherwise NULL */
	CondorVersionInfo* ShadowVersion;

	char *Execute;
	char *UIDDomain;
	char *FSDomain;
	char WorkingDir[_POSIX_PATH_MAX]; // The iwd given to the job
	char ExecuteDir[_POSIX_PATH_MAX]; // The scratch dir created for the job
	int ShuttingDown;
	IOProxy io_proxy;

	ClassAd* jobAd;
	int jobUniverse;

	// the real job executable name (after ATTR_JOB_CMD
	// is switched to condor_exec).
	char OrigJobName[_POSIX_PATH_MAX];

	FileTransfer *filetrans;

	// if true, transfer files at vacate time (in addtion to job exit)
	bool transfer_at_vacate;

		/// if true, we were asked to shutdown
	bool requested_exit;

		/// timer id for periodically sending info on job to Shadow
	int shadowupdate_tid;

};

#endif


