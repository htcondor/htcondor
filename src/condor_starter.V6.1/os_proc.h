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


#if !defined(_CONDOR_OS_PROC_H)
#define _CONDOR_OS_PROC_H

#include "user_proc.h"
#include "basename.h"

#if defined ( WIN32 )
#include "profile.WINDOWS.h"
#endif

/** This is a generic sort of "OS" process, the base for other types
	of jobs.

*/
class OsProc : public UserProc
{
public:
		/// Constructor
	OsProc( ClassAd* jobAd );

		/// Destructor
	virtual ~OsProc();

		/** Here we do things like set_user_ids(), get the executable, 
			get the args, env, cwd from the classad, open the input,
			output, error files for re-direction, and (finally) call
			daemonCore->Create_Process().
		 */
	virtual int StartJob() { return StartJob(NULL, NULL); };

	virtual bool canonicalizeJobPath(std::string &JobName, const char *iwd);

	int StartJob(FamilyInfo*, FilesystemRemap *);

		/** In this function, we determine if pid == our pid, and if so
			do a CONDOR_job_exit remote syscall.  
			@param pid The pid that exited.
			@param status Its status
		    @return True if our OsProc is no longer active, false if it is
		*/
	virtual bool JobReaper( int pid, int status );

		/** In this function, we determine what protocol to use to
			send the shadow a CONDOR_job_exit remote syscall, which
			will cause the job to leave the queue and the shadow to
			exit.  We can't send this until we're all done transfering
			files and cleaning up everything. 
		*/
	virtual bool JobExit( void );

		/** Publish all attributes we care about for updating the job
			controller into the given ClassAd.  This function is just
			virtual, not pure virtual, since OsProc and any derived
			classes should implement a version of this that publishes
			any info contained in each class, and each derived version
			should also call it's parent's version, too.
			@param ad pointer to the classad to publish into
			@return true if success, false if failure
		*/
	virtual bool PublishUpdateAd( ClassAd* ad );

	virtual void PublishToEnv( Env* proc_env );

		/// Send a SIGSTOP
	virtual void Suspend();

		/// Send a SIGCONT
	virtual void Continue();

		/// Send a SIGTERM
	virtual bool ShutdownGraceful();

		/// Send a SIGKILL
	virtual bool ShutdownFast();

		/// Evict for condor_rm (ATTR_REMOVE_KILL_SIG)
	virtual bool Remove();

		/// Evict for condor_hold (ATTR_HOLD_KILL_SIG)
	virtual bool Hold();

		/// rename a core file created by this process
	void checkCoreFile( void );
	bool renameCoreFile( const char* old_name, const char* new_name );

	int *makeCpuAffinityMask(int slotId);

	virtual bool SupportsPIDNamespace() { return true;}
	virtual bool ShouldConvertCmdToAbsolutePath() { return true;}

protected:

	ReliSock sshListener;
	void SetupSingularitySsh();
	int AcceptSingSshClient(Stream *stream);
	int SingEnterReaper( int pid, int status );

	bool is_suspended;
	bool is_checkpointed;

		/// Number of pids under this OsProc
	int num_pids;

	bool dumped_core;
	bool job_not_started;

	// From the ToE tag, for the post script's environment.
	int howCode;

private:

	bool m_using_priv_sep;
	bool cgroupActive;
	int singReaperId;

};

#endif
