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

void set_resource_limits();
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

		/** Compare our own UIDDomain vs. the submitting host.  We
			check in the given job ClassAd for ATTR_UID_DOMAIN.
			@param jobAd ClassAd of the job we're trying to run
			@return true if they match, false if not
		*/
	bool SameUidDomain( ClassAd* jobAd );

		/** Initialize the priv_state code with the appropriate user
			for this job.  This function deals with all the logic for
			checking UID_DOMAIN compatibility, SOFT_UID_DOMAIN
			support, and so on.
			@param jobAd ClassAd of the job we're trying to run
			@return true on success, false on failure
		*/
	bool InitUserPriv( ClassAd* jobAd );

		/** Initialize our ShadowVersion object with the version of
			the shadow we get out of the job ad.  If there's no shadow
			version, we leave our ShadowVersion NULL.  If we know the
			version, we instantiate a CondorVersionInfo object so we
			can perform checks on the version in the various places in
			the starter where we need to know this for compatibility.
			@param jobAd ClassAd of the job we're trying to run
		*/
	void InitShadowVersion( ClassAd* jobAd );

protected:
	List<UserProc> JobList;

private:
	char *InitiatingHost;
		/** The version of the shadow if known; otherwise NULL */
	CondorVersionInfo* ShadowVersion;
	char *Execute;
	char *UIDDomain;
	char *FSDomain;
	char WorkingDir[_POSIX_PATH_MAX]; // The iwd given to the job
	char ExecuteDir[_POSIX_PATH_MAX]; // The scratch dir created for the job
	int Key;
	int ShuttingDown;
	IOProxy io_proxy;

	int printAdToFile(ClassAd *ad, char* JobHistoryFileName);

};

#endif


