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

#include "condor_daemon_core.h"
#include "condor_config.h"
#include "globus_utils.h"
#include "grid_universe.h"
#include "directory.h"
#include "condor_ver_info.h"
#include "condor_attributes.h"
#include "exit.h"
#include "condor_uid.h"
#include "condor_email.h"
#include "shared_port_endpoint.h"

// Initialize static data members
const int GridUniverseLogic::job_added_delay = 3;
const int GridUniverseLogic::job_removed_delay = 2;
GridUniverseLogic::GmanPidTable_t * GridUniverseLogic::gman_pid_table = nullptr;
int GridUniverseLogic::rid = -1;
static const char scratch_prefix[] = "condor_g_scratch.";

// globals
GridUniverseLogic* _gridlogic = nullptr;


GridUniverseLogic::GridUniverseLogic() 
{
	// This class should only be instantiated once
	ASSERT( gman_pid_table == nullptr );

	// Make our hashtable
	gman_pid_table = new GmanPidTable_t;

	// Register a reaper for this grid managers
	rid = daemonCore->Register_Reaper("GManager",
		 &GridUniverseLogic::GManagerReaper,"GManagerReaper");

	// This class should register a reaper after the regular schedd reaper
	ASSERT( rid > 1 );

	return;
}

GridUniverseLogic::~GridUniverseLogic()
{
	if ( gman_pid_table) {
		for (const auto &[name, node]: *gman_pid_table) {
			if (daemonCore) {
				if ( node->add_timer_id >= 0 ) {
					daemonCore->Cancel_Timer(node->add_timer_id);
				}
				if ( node->remove_timer_id >= 0 ) {
					daemonCore->Cancel_Timer(node->remove_timer_id);
				}
				if ( node->pid > 0 ) {
					daemonCore->Send_Signal( node->pid, SIGQUIT );
				}
			}
			delete node;
		}
		delete gman_pid_table;
		rid = 0;
	}
	gman_pid_table = nullptr;
	return;
}

void 
GridUniverseLogic::JobCountUpdate(const char* user, const char* osname,
	   	const char* attr_value, const char* attr_name, int cluster, int proc, 
		int num_globus_jobs, int num_globus_unmanaged_jobs)
{
	// Quick sanity checks - this should never be...
	ASSERT( num_globus_jobs >= num_globus_unmanaged_jobs );
	ASSERT( num_globus_jobs >= 0 );
	ASSERT( num_globus_unmanaged_jobs >= 0 );

	// if there are unsubmitted jobs, the gridmanager apparently
	// does not know they are in the queue. so tell it some jobs
	// were added.
	if ( num_globus_unmanaged_jobs > 0 ) {
		JobAdded(user, osname, attr_value, attr_name, cluster, proc);
		return;
	}

	// now that we've taken care of unsubmitted globus jobs, see if there 
	// are any globus jobs at all.  if there are, make certain that there
	// is a grid manager watching over the jobs and start one if there isn't.
	if ( num_globus_jobs > 0 ) {
		StartOrFindGManager(user, osname, attr_value, attr_name, cluster, proc);
		return;
	}

	// if made it here, there is nothing we have to do
	return;
}


void 
GridUniverseLogic::JobAdded(const char* user, const char* osname,
	   	const char* attr_value, const char* attr_name, int cluster, int proc)
{
	gman_node_t* node = nullptr;

	node = StartOrFindGManager(user, osname, attr_value, attr_name, cluster, proc);

	if (!node) {
		// if we cannot find nor start a gridmanager, there's
		// nobody to notify.
		return;
	}

	// start timer to signal gridmanager if we haven't already
	if ( node->add_timer_id == -1 ) {  // == -1 means no timer set
		node->add_timer_id = daemonCore->Register_Timer(job_added_delay,
			GridUniverseLogic::SendAddSignal,
			"GridUniverseLogic::SendAddSignal");
		daemonCore->Register_DataPtr(node);
	}
	

	return;
}

void 
GridUniverseLogic::JobRemoved(const char* user, const char* osname,
	   	const char* attr_value, const char* attr_name,int cluster, int proc)
{
	gman_node_t* node = nullptr;

	node = StartOrFindGManager(user, osname, attr_value, attr_name, cluster, proc);

	if (!node) {
		// if we cannot find nor start a gridmanager, there's
		// nobody to notify.
		return;
	}

	// start timer to signal gridmanager if we haven't already
	if ( node->remove_timer_id == -1 ) {  // == -1 means no timer set
		node->remove_timer_id = daemonCore->Register_Timer(job_removed_delay,
			GridUniverseLogic::SendRemoveSignal,
			"GridUniverseLogic::SendRemoveSignal");
		daemonCore->Register_DataPtr(node);
	}

	return;
}

void
GridUniverseLogic::SendAddSignal(int /* tid */)
{
	// This method is called via a DC Timer set in JobAdded method

	// First get our gridmanager node
	auto * node = (gman_node_t *)daemonCore->GetDataPtr();
	ASSERT(node);

	// Record in the node that there is no outstanding timer set anymore
	node->add_timer_id = -1;

	// Signal the gridmanager
	if ( node->pid ) {
		daemonCore->Send_Signal(node->pid,GRIDMAN_ADD_JOBS);
	}
}

void
GridUniverseLogic::SendRemoveSignal(int /* tid */)
{
	// This method is called via a DC Timer set in JobRemoved method

	// First get our gridmanager node
	auto * node = (gman_node_t *)daemonCore->GetDataPtr();
	ASSERT(node);

	// Record in the node that there is no outstanding timer set anymore
	node->remove_timer_id = -1;

	// Signal the gridmanager
	if ( node->pid ) {
		daemonCore->Send_Signal(node->pid,GRIDMAN_REMOVE_JOBS);
	}
}

void
GridUniverseLogic::signal_all(int sig)
{
	// Iterate through our entire table and send the desired sig

	if (gman_pid_table) {
		for (const auto &[name, tmpnode]: *gman_pid_table) {
			if (tmpnode->pid) {
				daemonCore->Send_Signal(tmpnode->pid,sig);
			}
		}
	}
}


// Note: caller must deallocate return value w/ delete []
const char *
GridUniverseLogic::scratchFilePath(gman_node_t *gman_node, std::string & path)
{
	std::string filename;
	formatstr(filename,"%s%p.%d",scratch_prefix,
					gman_node,daemonCore->getpid());
	auto_free_ptr prefix(temp_dir_path());
	ASSERT(prefix);
	return dircat(prefix,filename.c_str(),path);
}


int 
GridUniverseLogic::GManagerReaper(int pid, int exit_status)
{
	gman_node_t* gman_node = nullptr;
	std::string user_key;

	// Iterate through our table to find the node w/ this pid
	// Someday we should perhaps also hash on the pid, but we
	// don't expect gridmanagers to exit very often, and there
	// are not that many of them.

	if (gman_pid_table) {
		for (const auto &[key, tmpnode]: *gman_pid_table) {
			if (tmpnode->pid == pid ) {
				// found it!
				user_key = key;
				gman_node = tmpnode;
				break;
			}
		}
	}

	std::string user_safe;
	std::string exit_reason;
	if(gman_node) { user_safe = user_key; }
	else { user_safe = "Unknown"; }
	if ( WIFEXITED( exit_status ) ) {
		formatstr( exit_reason, "with return code %d",
							 WEXITSTATUS( exit_status ) );
	} else {
		formatstr( exit_reason, "due to %s",
							 daemonCore->GetExceptionString( exit_status ) );
	}
	dprintf(D_ALWAYS, "condor_gridmanager (PID %d, user %s) exited %s.\n",
			pid, user_safe.c_str(), exit_reason.c_str() );
	if(WIFEXITED(exit_status) && WEXITSTATUS(exit_status) == DPRINTF_ERROR) {
		const char *condorUserName = get_condor_username();

		dprintf(D_ALWAYS, 
			"The gridmanager had a problem writing its log. "
			"Check the permissions of the file specified by GRIDMANAGER_LOG; "
			"it needs to be writable by Condor.\n");

			/* send email to the admin about this, but only
			 * every six hours - enough to not be ignored, but
			 * not enough to be a pest.  If only my children were
			 * so helpful and polite.  Ah, well, we can always dream...
			 */
		static time_t last_email_re_gridmanlog = 0;
		if ( time(nullptr) - last_email_re_gridmanlog > 6 * 60 * 60 ) {
			last_email_re_gridmanlog = time(nullptr);
			FILE *email = email_admin_open("Unable to launch grid universe jobs.");
			if ( email ) {
				fprintf(email,
					"The condor_gridmanager had an error writing its log file.  Check the  \n"
					"permissions/ownership of the file specified by the GRIDMANAGER_LOG setting in \n"
					"the condor_config file.  This file needs to be writable as user %s to enable\n"
					"the condor_gridmanager daemon to write to it. \n\n"
					"Until this problem is fixed, grid universe jobs submitted from this machine cannot "
					"be launched.\n", condorUserName ? condorUserName : "*unknown*" );
				email_close(email);
			} else {
					// Error sending an email message
				dprintf(D_ALWAYS,"ERROR: Cannot send email to the admin\n");
			}
		}	
	}	// end if(WIFEXITED(exit_status) && WEXITSTATUS(exit_status) == DPRINTF_ERROR)

	if (!gman_node) {
		// nothing more to do, so return
		return 0;
	}

	// Cancel any timers before removing the node!!
	if (gman_node->add_timer_id != -1) {
		daemonCore->Cancel_Timer(gman_node->add_timer_id);
	}
	if (gman_node->remove_timer_id != -1) {
		daemonCore->Cancel_Timer(gman_node->remove_timer_id);
	}
	// Remove node from our hash table
	gman_pid_table->erase(user_key);
	// Remove any scratch directory used by this gridmanager
	std::string scratchdirbuf;
	const char *scratchdir = scratchFilePath(gman_node, scratchdirbuf);
	ASSERT(scratchdir);
	std::string tmp;
	if ( IsDirectory(scratchdir) && 
		 init_user_ids(name_of_user(gman_node->user, tmp), domain_of_user(gman_node->user, "")) )
	{
		priv_state saved_priv = set_user_priv();
			// Must put this in braces so the Directory object
			// destructor is called, which will free the iterator
			// handle.  If we didn't do this, the below rmdir 
			// would fail.
		{
			Directory tmp( scratchdir );
			tmp.Remove_Entire_Directory();
		}
		if ( rmdir(scratchdir) == 0 ) {
			dprintf(D_FULLDEBUG,"Removed scratch dir %s\n",scratchdir);
		} else {
			dprintf(D_FULLDEBUG,"Failed to remove scratch dir %s\n",
					scratchdir);
		}
		set_priv(saved_priv);
		uninit_user_ids();
	}

	// Reclaim memory from the node itself
	delete gman_node;

	return 0;
}


GridUniverseLogic::gman_node_t *
GridUniverseLogic::lookupGmanByOwner(const char* user, const char* attr_value,
					int cluster, int proc)
{
	gman_node_t* result = nullptr;
	std::string user_key(user);
	if(attr_value){
		user_key += attr_value;
	}
	if (cluster) {
		formatstr_cat( user_key, "-%d.%d", cluster, proc );
	}

	if (!gman_pid_table) {
		// destructor has already been called; we are probably
		// closing down.
		return nullptr;
	}

	if (gman_pid_table->contains(user_key)) {
		result = (*gman_pid_table)[user_key];
	}

	return result;
}

int
GridUniverseLogic::FindGManagerPid(const char* user,
					const char* attr_value,	
					int cluster, int proc)
{
	gman_node_t* gman_node = nullptr;

	if ( attr_value && strlen(attr_value)==0 ) {
		attr_value = nullptr;
	}

	if ( (gman_node=lookupGmanByOwner(user, attr_value, cluster, proc)) ) {
		return gman_node->pid;
	}
	else {
		return -1;
	}
}

GridUniverseLogic::gman_node_t *
GridUniverseLogic::StartOrFindGManager(const char* user, const char* osname,
	   	const char* attr_value, const char* attr_name, int cluster, int proc)
{
	gman_node_t* gman_node = nullptr;
	int pid = 0;

		// If attr_value is an empty string, convert to NULL since code
		// after this point expects that.
	if ( attr_value && strlen(attr_value)==0 ) {
		attr_value = nullptr;
		attr_name = nullptr;
	}

	if ( !user || !osname ) {
		dprintf(D_ALWAYS,"ERROR - missing user/osname field\n");
		return nullptr;
	}

	if ( (gman_node=lookupGmanByOwner(user, attr_value, cluster, proc)) ) {
		// found it
		return gman_node;
	}

	// not found.  fire one up!  we want to run the GManager as the user.

	// but first, make certain we are not shutting down...
	if (!gman_pid_table) {
		// destructor has already been called; we are probably
		// closing down.
		return nullptr;
	}


#ifndef WIN32
	if (strcasecmp(user, "root") == 0 ) {
		dprintf(D_ALWAYS, "Tried to start condor_gmanager as root.\n");
		return nullptr;
	}
#endif

	dprintf( D_FULLDEBUG, "Starting condor_gmanager for user %s (%d.%d)\n",
			user, cluster, proc);

	char *gman_binary = nullptr;
	gman_binary = param("GRIDMANAGER");
	if ( !gman_binary ) {
		dprintf(D_ALWAYS,"ERROR - GRIDMANAGER not defined in config file\n");
		return nullptr;
	}

	ArgList args;
	std::string error_msg;

	args.AppendArg("condor_gridmanager");
	args.AppendArg("-f");

	if ( attr_value && *attr_value && param_boolean( "GRIDMANAGER_LOG_APPEND_SELECTION_EXPR", false ) ) {
		const std::string filename_filter = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-_";
		std::string log_suffix = attr_value;
		size_t pos = 0;
		while( (pos = log_suffix.find_first_not_of( filename_filter, pos )) != std::string::npos ) {
			log_suffix[pos] = '_';
		}
		args.AppendArg("-a");
		args.AppendArg(log_suffix.c_str());
	}

	char *gman_args = param("GRIDMANAGER_ARGS");

	if(!args.AppendArgsV1RawOrV2Quoted(gman_args,error_msg)) {
		dprintf( D_ALWAYS, "ERROR: failed to parse gridmanager args: %s\n",
				 error_msg.c_str());
		free(gman_binary);
		free(gman_args);
		return nullptr;
	}
	free(gman_args);

	std::string constraint;
	if ( !attr_name  ) {
		formatstr(constraint, "(%s=?=\"%s\"&&%s==%d)",
						   ATTR_USER, user,
						   ATTR_JOB_UNIVERSE, CONDOR_UNIVERSE_GRID);
	} else {
		formatstr(constraint, R"((%s=?="%s"&&%s=?="%s"&&%s==%d))",
						   ATTR_USER, user,
						   attr_name, attr_value,
						   ATTR_JOB_UNIVERSE, CONDOR_UNIVERSE_GRID);

		args.AppendArg("-A");
		args.AppendArg(attr_value);
	}
	args.AppendArg("-C");
	args.AppendArg(constraint);

	dprintf(D_ALWAYS, "Launching gridmanager with -o %s -u %s\n", osname, user);

	args.AppendArg("-o");
	args.AppendArg(osname);
	args.AppendArg("-u");
	args.AppendArg(user);

	std::string tmp;
	if (!init_user_ids(name_of_user(osname, tmp), domain_of_user(osname, ""))) {
		dprintf(D_ALWAYS,"ERROR - init_user_ids(%s) failed in GRIDMANAGER\n", osname);
		free(gman_binary);
		return nullptr;
	}

	static bool first_time_through = true;
	if ( first_time_through ) {
		// Note: Because first_time_through is static, this block runs only 
		// once per schedd invocation.
		first_time_through = false;

		// Clean up any old / abandoned scratch dirs.
		dprintf(D_FULLDEBUG,"Checking for old gridmanager scratch dirs\n");
		char *prefix = temp_dir_path();
		ASSERT(prefix);
		Directory tmp( prefix, PRIV_USER );
		const char *f = nullptr;
		char const *dot = nullptr;
		int fname_pid = 0;
		int mypid = daemonCore->getpid();
		int scratch_pre_len = strlen(scratch_prefix);
		while ( (f=tmp.Next()) ) {
				// skip regular files -- we only need to inspect subdirs
			if ( !tmp.IsDirectory() ) {
				continue;
			}
				// skip if it does not start with our prefix
			if ( strncmp(scratch_prefix,f,scratch_pre_len) ) {
				continue;
			}
				// skip if does not end w/ a pid
			dot = strrchr(f,'.');
			if ( !dot ) {
				continue;
			}
				// skip if this pid is still alive and not ours
			dot++;	// skip over period
			fname_pid = atoi(dot);
			if ( fname_pid != mypid && daemonCore->Is_Pid_Alive(fname_pid) ) {
					continue;
			}
				// if we made it here, blow away this subdir
			if ( tmp.Remove_Current_File() ) {
				dprintf(D_ALWAYS,"Removed old scratch dir %s\n",
				tmp.GetFullPath());
			}
		}	// end of while for cleanup of old scratch dirs

		dprintf(D_FULLDEBUG,"Done checking for old scratch dirs\n");			

		free(prefix);

	}	// end of once-per-schedd invocation block

	// Create a temp dir for the gridmanager and append proper
	// command-line arguments to tell where it is.
	bool failed = false;
	gman_node = new gman_node_t;
	std::string pathbuf;
	const char *finalpath = scratchFilePath(gman_node, pathbuf);
	priv_state saved_priv = set_user_priv();
	if ( (mkdir(finalpath,0700)) < 0 ) {
		// mkdir failed.  
		dprintf(D_ALWAYS,"ERROR - mkdir(%s,0700) failed in GRIDMANAGER, errno=%d (%s)\n",
				finalpath, errno, strerror(errno));
		failed = true;
	}
	set_priv(saved_priv);
	uninit_user_ids();
	args.AppendArg("-S");	// -S = "ScratchDir" argument
	args.AppendArg(finalpath);
	if ( failed ) {
		// we already did dprintf reason to the log...
		free(gman_binary);
		delete gman_node;
		return nullptr;
	}

	if(IsFulldebug(D_FULLDEBUG)) {
		std::string args_string;
		args.GetArgsStringForDisplay(args_string);
		dprintf(D_FULLDEBUG,"Really Execing %s\n",args_string.c_str());
	}

	std::string daemon_sock = SharedPortEndpoint::GenerateEndpointName( "gridmanager" );
	pid = daemonCore->Create_Process( 
		gman_binary,			// Program to exec
		args,					// Command-line args
		PRIV_ROOT,				// Run as root, so it can switch to
		                        //   PRIV_CONDOR
		rid,					// Reaper ID
		TRUE, TRUE, nullptr, nullptr, nullptr, nullptr,
		nullptr, nullptr, 0, nullptr, 0, nullptr, nullptr,
		daemon_sock.c_str()
		);

	free(gman_binary);

	if ( pid <= 0 ) {
		dprintf ( D_ALWAYS, "StartOrFindGManager: Create_Process problems!\n" );
		if (gman_node) delete gman_node;
		return nullptr;
	}

	// If we made it here, we happily started up a new gridmanager process

	dprintf( D_ALWAYS, "Started condor_gmanager for user %s pid=%d\n",
			user,pid);

	// Make a new gman_node entry for our hashtable & insert it
	if ( !gman_node ) {
		gman_node = new gman_node_t;
	}
	gman_node->pid = pid;
	gman_node->user[0] = '\0';
	if ( user ) {
		strcpy(gman_node->user, user);
	}
	std::string user_key(user);
	if(attr_value){
		user_key += attr_value;
	}
	if (cluster) {
		formatstr_cat( user_key, "-%d.%d", cluster, proc );
	}

	gman_pid_table->emplace(user_key,gman_node);

	// start timer to signal gridmanager if we haven't already
	if ( gman_node->add_timer_id == -1 ) {  // == -1 means no timer set
		gman_node->add_timer_id = daemonCore->Register_Timer(job_added_delay,
			GridUniverseLogic::SendAddSignal,
			"GridUniverseLogic::SendAddSignal");
		daemonCore->Register_DataPtr(gman_node);
	}

	// All done
	return gman_node;
}
