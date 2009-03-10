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


#include "condor_common.h"
#include "condor_classad.h"
#include "condor_debug.h"
#include "condor_daemon_core.h"
#include "condor_attributes.h"
#include "condor_syscall_mode.h"
#include "exit.h"
#include "vanilla_proc.h"
#include "starter.h"
#include "syscall_numbers.h"
#include "dynuser.h"
#include "condor_config.h"
#include "domain_tools.h"

#ifdef WIN32
#include "executable_scripts.WINDOWS.h"
extern dynuser* myDynuser;
#endif

extern CStarter *Starter;

int
VanillaProc::StartJob()
{
	dprintf(D_FULLDEBUG,"in VanillaProc::StartJob()\n");

	// vanilla jobs, unlike standard jobs, are allowed to run 
	// shell scripts (or as is the case on NT, batch files).  so
	// edit the ad so we start up a shell, pass the executable as
	// an argument to the shell, if we are asked to run a .bat file.
#ifdef WIN32

	char systemshell[MAX_PATH];    
	const char* jobtmp			= Starter->jic->origJobName();
	int			joblen			= strlen(jobtmp);
	const char	*extension		= joblen > 5 ? &(jobtmp[joblen-4]) : NULL;
	bool		allow_scripts	= param_boolean ( 
		"ALLOW_SCRIPTS_AS_EXECUTABLES", true );

	/** Since we will always look-up the file extension in the 
		file transfer object, we take a copy of it here--if it
		is available, that is. */
	JobAd->Assign ( 
		ATTR_JOB_CMDEXT, 
		extension ? extension : "" );
	
	if ( extension && 
			( (stricmp(".bat",extension) == 0) || 
			  (stricmp(".cmd",extension) == 0) ) ) {
		// executable name ends in .bat or .cmd

		// pull out pathname to executable and save
		MyString JobName;
		if ( JobAd->LookupString( ATTR_JOB_CMD, JobName ) != 1 ) {
			// really strange it is not there.  fall back
			// on the origJobName.
			JobName = jobtmp;
		}

		// now  change executable to be cmd.exe
		::GetSystemDirectory(systemshell,MAX_PATH);
		MyString tmp;
		tmp.sprintf("%s\\cmd.exe",systemshell);
		JobAd->Assign(ATTR_JOB_CMD,tmp.Value());

		// now change arguments to include name of program cmd.exe 
		// should run
		// also pass /Q and /C arguments to cmd.exe, to tell it we do not
		// want an interactive shell -- just run the command and exit

		ArgList args;
		MyString arg_errors;

			// Since we are adding to the argument list, we may need to deal
			// with platform-specific arg syntax in the user's args in order
			// to successfully merge them with the additional args.
		args.SetArgV1SyntaxToCurrentPlatform();

		args.AppendArg("/Q");
		args.AppendArg("/C");
			// If we transferred the job, it may have been
			// renamed to condor_exec.exe even though it is a batch file.
			// We will have to rename it back to a .bat before it will run.
		if ( stricmp(CONDOR_EXEC,condor_basename(JobName.Value()))==0 ) {
				// we also must rename file condor_exec.exe to condor_exec.bat
			args.AppendArg("condor_exec.bat");				
			rename(CONDOR_EXEC,"condor_exec.bat");
		} else {
				// Wasn't renamed
			args.AppendArg(JobName);
		}

		if(!args.AppendArgsFromClassAd(JobAd,&arg_errors) ||
		   !args.InsertArgsIntoClassAd(JobAd,NULL,&arg_errors)) {
			dprintf(D_ALWAYS,"ERROR: failed to get args from job ad: %s\n",
					arg_errors.Value());
			return FALSE;
		}


		if(DebugFlags & D_FULLDEBUG) {
			MyString args_desc;
			args.GetArgsStringForDisplay(&args_desc);
			dprintf(D_FULLDEBUG,
					"Executable is a batch file, so running %s\\cmd.exe %s\n",
					systemshell,args_desc.Value());
		}

	}	// end of if executable name ends in .bat
	else if ( extension && allow_scripts &&
			( MATCH != strcasecmp ( ".exe", extension ) && 
			  MATCH != strcasecmp ( ".com", extension ) ) ) {

		CHAR		interpreter[MAX_PATH+1];
		ArgList		arguments;
		MyString	filename,
					jobname, 
					error,
					description;
		BOOL		ok;
		
		/** since we do not actually know how long the extension of
			the file is, we'll need to hunt down the '.' in the path,
			if it exists */
		extension = strrchr ( jobtmp, '.' );

		if ( !extension ) {

			dprintf ( 
				D_ALWAYS, 
				"VanillaProc::StartJob(): Failed to extract "
				"the file's extension.\n" );

			/** don't fail here, since we want executables to run as
				usual.  That is, some condor jobs submit executables
				that do not have the '.exe' extension, but are, 
				nonetheless, executable binaries.  For instance, a
				submit script may contain: executable$(OPSYS) */

		} else {

			/** Take another snapshot of the extension, since it may
				be the case that it is more than three letters as is
				assumed above. */
			JobAd->Assign ( 
				ATTR_JOB_CMDEXT, 
				extension ? extension : "" );

			/** try and find the executable associated with this 
				file extension */
			ok = GetExecutableAndArgumentTemplateByExtention ( 
				extension, 
				interpreter );

			if ( !ok ) {

				dprintf ( 
					D_ALWAYS, 
					"VanillaProc::StartJob(): Failed to find an "
					"executable for extension *%s\n",
					extension );

				/** don't fail here either, for the same reasons we 
					outline above, save a small modification to the 
					executable's name: executable.$(OPSYS) */

			} else {

				/** pull out the path to the executable */
				if ( 1 != JobAd->LookupString ( 
					ATTR_JOB_CMD, 
					jobname ) ) {
					
					/** fall back on Starter->jic->origJobName() */
					jobname = jobtmp;

				}

				dprintf (
					D_FULLDEBUG,
					"Job command: %s\n",
					jobname.Value () );

				/** change executable to be the interpreter associated
					with the file type. */
				JobAd->Assign ( 
					ATTR_JOB_CMD, 
					interpreter );

				/** Since we are adding to the argument list, we
					may need to deal with platform-specific argument
					syntax in the user's arguments in order to
					successfully merge them with the additional 
					arguments. */
				arguments.SetArgV1SyntaxToCurrentPlatform ();

				/** If we transferred the job, it may have been
					renamed to condor_exec.exe even though it is
					not an executable. Here we rename it back to
					a the correct extension before it will run. */
				if ( MATCH == strcasecmp ( 
						CONDOR_EXEC, 
						condor_basename ( jobname.Value () ) ) ) {
					filename.sprintf ( "condor_exec%s", extension );
					rename ( CONDOR_EXEC, filename.Value () );					
				} else {
					filename = jobname;
				}
				arguments.AppendArg ( filename );

				/** We've moved the script to argv[1], so we need to 
					add	the remaining arguments to positions argv[2]..
					argv[/n/]. */
				if ( !arguments.AppendArgsFromClassAd ( JobAd, &error ) ||
					 !arguments.InsertArgsIntoClassAd ( JobAd, NULL, 
					&error ) ) {

					dprintf (
						D_ALWAYS,
						"ERROR: failed to get args from job ad: %s\n",
						error.Value () );

					return FALSE;

				}
				
				/** Since we know already that we do not want this
					file returned to us, we explicitly add it to an
					exception list which will stop the file transfer
					mechanism from considering it for transfer to
					submitter */
				Starter->jic->removeFromOutputFiles (
					filename.Value () );
				
				if ( DebugFlags & D_FULLDEBUG ) {

					arguments.GetArgsStringForDisplay ( 
						&description );

					dprintf (
						D_FULLDEBUG,
						"Executable is a *%s script, so running %s %s\n",
						extension,
						interpreter,
						description.Value () );

				}

			}
			
		}
		
	}
#endif

	// set up a FamilyInfo structure to tell OsProc to register a family
	// with the ProcD in its call to DaemonCore::Create_Process
	//
	FamilyInfo fi;

	// take snapshots at no more than 15 seconds in between, by default
	//
	fi.max_snapshot_interval = param_integer("PID_SNAPSHOT_INTERVAL", 15);

	char const *dedicated_account = Starter->jic->getExecuteAccountIsDedicated();
	if (dedicated_account) {
			// using login-based family tracking
		fi.login = dedicated_account;
			// The following message is documented in the manual as the
			// way to tell whether the dedicated execution account
			// configuration is being used.
		dprintf(D_ALWAYS,
		        "Tracking process family by login \"%s\"\n",
		        fi.login);
	}

#if defined(LINUX)
	// on Linux, we also have the ability to track processes via
	// a phony supplementary group ID
	//
	gid_t tracking_gid;
	if (param_boolean("USE_GID_PROCESS_TRACKING", false)) {
		if (!can_switch_ids() &&
		    (Starter->condorPrivSepHelper() == NULL))
		{
			EXCEPT("USE_GID_PROCESS_TRACKING enabled, but can't modify "
			           "the group list of our children unless running as "
			           "root or using PrivSep");
		}
		fi.group_ptr = &tracking_gid;
	}
#endif

	// have OsProc start the job
	//
	return OsProc::StartJob(&fi);
}


bool
VanillaProc::PublishUpdateAd( ClassAd* ad )
{
	dprintf( D_FULLDEBUG, "In VanillaProc::PublishUpdateAd()\n" );

	ProcFamilyUsage* usage;
	ProcFamilyUsage cur_usage;
	if (m_proc_exited) {
		usage = &m_final_usage;
	}
	else {
		if (daemonCore->Get_Family_Usage(JobPid, cur_usage) == FALSE) {
			dprintf(D_ALWAYS, "error getting family usage in "
					"VanillaProc::PublishUpdateAd() for pid %d\n", JobPid);
			return false;
		}
		usage = &cur_usage;
	}

		// Publish the info we care about into the ad.
	char buf[200];
	sprintf( buf, "%s=%lu", ATTR_JOB_REMOTE_SYS_CPU, usage->sys_cpu_time );
	ad->InsertOrUpdate( buf );
	sprintf( buf, "%s=%lu", ATTR_JOB_REMOTE_USER_CPU, usage->user_cpu_time );
	ad->InsertOrUpdate( buf );
	sprintf( buf, "%s=%lu", ATTR_IMAGE_SIZE, usage->max_image_size );
	ad->InsertOrUpdate( buf );

		// Update our knowledge of how many processes the job has
	num_pids = usage->num_procs;

		// Now, call our parent class's version
	return OsProc::PublishUpdateAd( ad );
}


bool
VanillaProc::JobReaper(int pid, int status)
{
	dprintf(D_FULLDEBUG,"in VanillaProc::JobReaper()\n");

	if (pid == JobPid) {
			// Make sure that nothing was left behind.
		daemonCore->Kill_Family(JobPid);

			// Record final usage stats for this process family, since
			// once the reaper returns, the family is no longer
			// registered with DaemonCore and we'll never be able to
			// get this information again.
		if (daemonCore->Get_Family_Usage(JobPid, m_final_usage) == FALSE) {
			dprintf(D_ALWAYS, "error getting family usage for pid %d in "
					"VanillaProc::JobReaper()\n", JobPid);
		}
	}

		// This will reset num_pids for us, too.
	return OsProc::JobReaper( pid, status );
}


void
VanillaProc::Suspend()
{
	dprintf(D_FULLDEBUG,"in VanillaProc::Suspend()\n");
	
	// suspend the user job
	if (JobPid != -1) {
		if (daemonCore->Suspend_Family(JobPid) == FALSE) {
			dprintf(D_ALWAYS,
			        "error suspending family in VanillaProc::Suspend()\n");
		}
	}
	
	// set our flag
	is_suspended = true;
}

void
VanillaProc::Continue()
{
	dprintf(D_FULLDEBUG,"in VanillaProc::Continue()\n");
	
	// resume user job
	if (JobPid != -1) {
		if (daemonCore->Continue_Family(JobPid) == FALSE) {
			dprintf(D_ALWAYS,
			        "error continuing family in VanillaProc::Continue()\n");
		}
	}

	// set our flag
	is_suspended = false;
}

bool
VanillaProc::ShutdownGraceful()
{
	dprintf(D_FULLDEBUG,"in VanillaProc::ShutdownGraceful()\n");
	
	if ( JobPid == -1 ) {
		// there is no process family yet, probably because we are still
		// transferring files.  just return true to say we're all done,
		// and that way the starter class will simply delete us and the
		// FileTransfer destructor will clean up.
		return true;
	}

	// WE USED TO.....
	//
	// take a snapshot before we softkill the parent job process.
	// this helps ensure that if the parent exits without killing
	// the kids, our JobExit() handler will get em all.
	//
	// TODO: should we make an explicit call to the procd here to tell
	// it to take a snapshot???

	// now softkill the parent job process.  this is exactly what
	// OsProc::ShutdownGraceful does, so call it.
	//
	return OsProc::ShutdownGraceful();	
}

bool
VanillaProc::ShutdownFast()
{
	dprintf(D_FULLDEBUG,"in VanillaProc::ShutdownFast()\n");
	
	if ( JobPid == -1 ) {
		// there is no process family yet, probably because we are still
		// transferring files.  just return true to say we're all done,
		// and that way the starter class will simply delete us and the
		// FileTransfer destructor will clean up.
		return true;
	}

	// We purposely do not do a SIGCONT here, since there is no sense
	// in potentially swapping the job back into memory if our next
	// step is to hard kill it.
	requested_exit = true;

	// this used to be the only place where we would clean up the process
	// family. this, however, wouldn't properly clean up local universe jobs
	// so a call to Kill_Family has been added to JobReaper(). i'm not sure
	// that this call is still needed, but am unwilling to remove it on the
	// eve of Condor 7
	//   -gquinn, 2007-11-14
	daemonCore->Kill_Family(JobPid);

	return false;	// shutdown is pending, so return false
}
