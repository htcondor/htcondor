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

#include "condor_common.h"

#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_config.h"
#include "globus_utils.h"
#include "grid_universe.h"
#include "directory.h"
#include "MyString.h"
#include "condor_ver_info.h"
#include "condor_attributes.h"

// Initialize static data members
const int GridUniverseLogic::job_added_delay = 3;
const int GridUniverseLogic::job_removed_delay = 2;
GridUniverseLogic::GmanPidTable_t * GridUniverseLogic::gman_pid_table = NULL;
int GridUniverseLogic::rid = -1;
static const char scratch_prefix[] = "condor_g_scratch.";
static int make_tmp_dir = -1;  // -1 = unknown, 0 = no tmp dir, 1 = mkdir

// globals
GridUniverseLogic* _gridlogic = NULL;


// hash function for Owner names
static int 
gman_pid_table_hash (const MyString &str, int numBuckets)
{
	return (str.Hash() % numBuckets);
}


GridUniverseLogic::GridUniverseLogic() 
{
	// This class should only be instantiated once
	ASSERT( gman_pid_table == NULL );

	// Make our hashtable
	gman_pid_table = new GmanPidTable_t(10,&gman_pid_table_hash);

	// Register a reaper for this grid managers
	rid = daemonCore->Register_Reaper("GManager",
		(ReaperHandler) &GridUniverseLogic::GManagerReaper,"GManagerReaper");

	// This class should register a reaper after the regular schedd reaper
	ASSERT( rid > 1 );

	return;
}

GridUniverseLogic::~GridUniverseLogic()
{
	if ( gman_pid_table) {
		gman_node_t * node;
		gman_pid_table->startIterations();
		while (gman_pid_table->iterate( node ) == 1 ) {
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
	gman_pid_table = NULL;
	return;
}

bool
GridUniverseLogic::want_scratch_dir()
{
	return group_per_subject();
}

bool
GridUniverseLogic::group_per_subject()
{
	char *gman_binary;

	if ( make_tmp_dir == -1 ) {
		gman_binary = param("GRIDMANAGER");
		if (!gman_binary) {
			return false;
		}

		// Now figure out if the gridmanager installed on this
		// machine wants a tmp dir or not.  Do this by checking the version.
		char ver[128];
		ver[0] = '\0';
		CondorVersionInfo::get_version_from_file(gman_binary,ver,sizeof(ver));
		dprintf(D_FULLDEBUG,"Version of gridmanager is %s\n",ver);
		CondorVersionInfo vi(ver);
		// If gridmanager is ver 6.5.1 or newer, it wants a tmp dir.
		if ( vi.built_since_version(6,5,1) ) {
				// give it a tmp dir
			make_tmp_dir = 1;
		} else {
				// old gridmanager --- no tmp dir
			make_tmp_dir = 0;
		}
		free(gman_binary);
	}

	if ( make_tmp_dir == 1 ) {
		return true;
	}

	if ( make_tmp_dir == 0 ) {
		return false;
	}

	// if we made it here, make_tmp_dir is messed up
	EXCEPT("group_per_subject() failed sanity check (%d)\n",make_tmp_dir);
	return false;
}

void 
GridUniverseLogic::JobCountUpdate(const char* owner, const char* domain,
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
		JobAdded(owner, domain, attr_value, attr_name, cluster, proc);
		return;
	}

	// now that we've taken care of unsubmitted globus jobs, see if there 
	// are any globus jobs at all.  if there are, make certain that there
	// is a grid manager watching over the jobs and start one if there isn't.
	if ( num_globus_jobs > 0 ) {
		StartOrFindGManager(owner, domain, attr_value, attr_name, cluster, proc);
		return;
	}

	// if made it here, there is nothing we have to do
	return;
}


void 
GridUniverseLogic::JobAdded(const char* owner, const char* domain,
	   	const char* attr_value, const char* attr_name, int cluster, int proc)
{
	gman_node_t* node;

	node = StartOrFindGManager(owner, domain, attr_value, attr_name, cluster, proc);

	if (!node) {
		// if we cannot find nor start a gridmanager, there's
		// nobody to notify.
		return;
	}

	// start timer to signal gridmanager if we haven't already
	if ( node->add_timer_id == -1 ) {  // == -1 means no timer set
		node->add_timer_id = daemonCore->Register_Timer(job_added_delay,
			(TimerHandler) &GridUniverseLogic::SendAddSignal,
			"GridUniverseLogic::SendAddSignal");
		daemonCore->Register_DataPtr(node);
	}
	

	return;
}

void 
GridUniverseLogic::JobRemoved(const char* owner, const char* domain,
	   	const char* attr_value, const char* attr_name,int cluster, int proc)
{
	gman_node_t* node;

	node = StartOrFindGManager(owner, domain, attr_value, attr_name, cluster, proc);

	if (!node) {
		// if we cannot find nor start a gridmanager, there's
		// nobody to notify.
		return;
	}

	// start timer to signal gridmanager if we haven't already
	if ( node->add_timer_id == -1 ) {  // == -1 means no timer set
		node->remove_timer_id = daemonCore->Register_Timer(job_removed_delay,
			(TimerHandler) &GridUniverseLogic::SendRemoveSignal,
			"GridUniverseLogic::SendRemoveSignal");
		daemonCore->Register_DataPtr(node);
	}

	return;
}

int
GridUniverseLogic::SendAddSignal(Service *)
{
	// This method is called via a DC Timer set in JobAdded method

	// First get our gridmanager node
	gman_node_t * node = (gman_node_t *)daemonCore->GetDataPtr();
	ASSERT(node);

	// Record in the node that there is no outstanding timer set anymore
	node->add_timer_id = -1;

	// Signal the gridmanager
	if ( node->pid ) {
		daemonCore->Send_Signal(node->pid,GRIDMAN_ADD_JOBS);
	}

	return 0;
}

int
GridUniverseLogic::SendRemoveSignal(Service *)
{
	// This method is called via a DC Timer set in JobRemoved method

	// First get our gridmanager node
	gman_node_t * node = (gman_node_t *)daemonCore->GetDataPtr();
	ASSERT(node);

	// Record in the node that there is no outstanding timer set anymore
	node->remove_timer_id = -1;

	// Signal the gridmanager
	if ( node->pid ) {
		daemonCore->Send_Signal(node->pid,GRIDMAN_REMOVE_JOBS);
	}

	return 0;
}

void
GridUniverseLogic::signal_all(int sig)
{
	// Iterate through our entire table and send the desired sig

	if (gman_pid_table) {
		gman_node_t* tmpnode;
		gman_pid_table->startIterations();
		while ( gman_pid_table->iterate(tmpnode) ) {
			if (tmpnode->pid) {
				daemonCore->Send_Signal(tmpnode->pid,sig);
			}
		}
	}
}


// Note: caller must deallocate return value w/ delete []
char *
GridUniverseLogic::scratchFilePath(gman_node_t *gman_node)
{
	MyString filename;
	filename.sprintf("%s%p.%d",scratch_prefix,
					gman_node,daemonCore->getpid());
	char *prefix = temp_dir_path();
	ASSERT(prefix);
		// note: dircat allocates with new char[]
	char *finalpath = dircat(prefix,filename.Value());
	free(prefix);
	return finalpath;
}


int 
GridUniverseLogic::GManagerReaper(Service *,int pid, int exit_status)
{
	gman_node_t* gman_node = NULL;
	MyString owner;

	// Iterate through our table to find the node w/ this pid
	// Someday we should perhaps also hash on the pid, but we
	// don't expect gridmanagers to exit very often, and there
	// are not that many of them.

	if (gman_pid_table) {
		gman_node_t* tmpnode;
		gman_pid_table->startIterations();
		while ( gman_pid_table->iterate(owner,tmpnode) ) {
			if (tmpnode->pid == pid ) {
				// found it!
				gman_node = tmpnode;
				break;
			}
		}
	}

	if (!gman_node) {
		dprintf(D_ALWAYS,
			"condor_gridmanager exited pid=%d status=%d owner=Unknown\n",
			pid,exit_status);
		// nothing more to do, so return
		return 0;
	}

	dprintf(D_ALWAYS,"condor_gridmanager exited pid=%d status=%d owner=%s\n",
			pid,exit_status,owner.Value());

	// Cancel any timers before removing the node!!
	if (gman_node->add_timer_id != -1) {
		daemonCore->Cancel_Timer(gman_node->add_timer_id);
	}
	if (gman_node->remove_timer_id != -1) {
		daemonCore->Cancel_Timer(gman_node->remove_timer_id);
	}
	// Remove node from our hash table
	gman_pid_table->remove(owner);
	// Remove any scratch directory used by this gridmanager
	if ( want_scratch_dir() ) {
		char *scratchdir = scratchFilePath(gman_node);
		ASSERT(scratchdir);
		if ( IsDirectory(scratchdir) && 
			init_user_ids(gman_node->owner, gman_node->domain) ) 
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
		delete [] scratchdir;
	}
	// Reclaim memory from the node itself
	delete gman_node;

	return 0;
}


GridUniverseLogic::gman_node_t *
GridUniverseLogic::lookupGmanByOwner(const char* owner, const char* attr_value,
					int cluster, int proc)
{
	gman_node_t* result = NULL;
	MyString owner_key(owner);
	if(attr_value){
		MyString attr_key(attr_value);
		owner_key += attr_key;
	}
	if (cluster) {
		char tmp[100];
		sprintf(tmp,"-%d.%d",cluster,proc);
		MyString job_key(tmp);
		owner_key += job_key;
	}

	if (!gman_pid_table) {
		// destructor has already been called; we are probably
		// closing down.
		return NULL;
	}

	gman_pid_table->lookup(owner_key,result);

	return result;
}

GridUniverseLogic::gman_node_t *
GridUniverseLogic::StartOrFindGManager(const char* owner, const char* domain,
	   	const char* attr_value, const char* attr_name, int cluster, int proc)
{
	gman_node_t* gman_node;
	int pid;

	if ( (gman_node=lookupGmanByOwner(owner, attr_value, cluster, proc)) ) {
		// found it
		return gman_node;
	}

	// not found.  fire one up!  we want to run the GManager as the user.

	// but first, make certain we are not shutting down...
	if (!gman_pid_table) {
		// destructor has already been called; we are probably
		// closing down.
		return NULL;
	}


#ifndef WIN32
	if (stricmp(owner, "root") == 0 ) {
		dprintf(D_ALWAYS, "Tried to start condor_gmanager as root.\n");
		return NULL;
	}
#endif

	dprintf( D_FULLDEBUG, "Starting condor_gmanager for owner %s (%d.%d)\n",
			owner, cluster, proc);

	char *gman_binary;
	gman_binary = param("GRIDMANAGER");
	if ( !gman_binary ) {
		dprintf(D_ALWAYS,"ERROR - GRIDMANAGER not defined in config file\n");
		return NULL;
	}

	ArgList args;
	MyString error_msg;

	args.AppendArg("condor_gridmanager");
	args.AppendArg("-f");

	char *gman_args = param("GRIDMANAGER_ARGS");

	if(!args.AppendArgsV1or2Input(gman_args,&error_msg)) {
		dprintf( D_ALWAYS, "ERROR: failed to parse gridmanager args: %s\n",
				 error_msg.Value());
		free(gman_args);
		return NULL;
	}
	free(gman_args);

	if (cluster) {
		dprintf( D_ALWAYS, "ERROR - gridman_per_job not supported!\n" );
		return NULL;
	}
	if ( !group_per_subject() ) {
			// pre-6.5.1 gridmanager: waaaaaay too old!
		dprintf( D_ALWAYS, "ERROR - gridmanager way too old!\n" );
		return NULL;
	} else {
		// new way - pass a constraint
		if ( !owner ) {
			dprintf(D_ALWAYS,"ERROR - missing owner field\n");
			return NULL;
		}
		MyString constraint;
		MyString full_owner_name(owner);
		const char *owner_or_user;
		if ( domain && domain[0] ) {
			owner_or_user = ATTR_USER;
			full_owner_name += "@";
			full_owner_name += domain;
		} else {
			owner_or_user = ATTR_OWNER;
		}
		if ( !attr_name  ) {
			constraint.sprintf("(%s=?=\"%s\"&&%s==%d)",
				owner_or_user,full_owner_name.Value(),
				ATTR_JOB_UNIVERSE,CONDOR_UNIVERSE_GRID);
		} else {
			constraint.sprintf("(%s=?=\"%s\"&&%s=?=\"%s\")",
				owner_or_user,full_owner_name.Value(),
				attr_name,attr_value);
		}
		args.AppendArg("-C");
		args.AppendArg(constraint.Value());
	}

	if (!init_user_ids(owner, domain)) {
		dprintf(D_ALWAYS,"ERROR - init_user_ids() failed in GRIDMANAGER\n");
		return NULL;
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
		const char *f;
		char *dot;
		int fname_pid;
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

		if (prefix != NULL) {
			free(prefix);
			prefix = NULL;
		}

	}	// end of once-per-schedd invocation block

	// If gridmanager wants a tmp dir, create one and append proper
	// command-line arguments to tell where it is.
	if ( want_scratch_dir() ) {
		bool failed = false;
		gman_node = new gman_node_t;
		char *finalpath = scratchFilePath(gman_node);
		priv_state saved_priv = set_user_priv();
		if ( (mkdir(finalpath,0700)) < 0 ) {
			// mkdir failed.  
			dprintf(D_ALWAYS,"ERROR - mkdir(%s,0700) failed in GRIDMANAGER\n",
				finalpath);
			failed = true;
		}
		set_priv(saved_priv);
		args.AppendArg("-S");	// -S = "ScratchDir" argument
		args.AppendArg(finalpath);
		delete [] finalpath;
		if ( failed ) {
			// we already did dprintf reason to the log...
			if (gman_node) delete gman_node;
			return NULL;
		}
	}

	if(DebugFlags & D_FULLDEBUG) {
		MyString args_string;
		args.GetArgsStringForDisplay(&args_string);
		dprintf(D_FULLDEBUG,"Really Execing %s\n",args_string.Value());
	}

	pid = daemonCore->Create_Process( 
		gman_binary,			// Program to exec
		args,					// Command-line args
		PRIV_USER_FINAL,		// Run as the Owner
		rid						// Reaper ID
		);

	free(gman_binary);

	if ( pid <= 0 ) {
		dprintf ( D_ALWAYS, "StartOrFindGManager: Create_Process problems!\n" );
		if (gman_node) delete gman_node;
		return NULL;
	}

	// If we made it here, we happily started up a new gridmanager process

	dprintf( D_ALWAYS, "Started condor_gmanager for owner %s pid=%d\n",
			owner,pid);

	// Make a new gman_node entry for our hashtable & insert it
	if ( !gman_node ) {
		gman_node = new gman_node_t;
	}
	gman_node->pid = pid;
	gman_node->owner[0] = '\0';
	gman_node->domain[0] = '\0';
	if ( owner ) {
		strcpy(gman_node->owner,owner);
	}
	if ( domain ) {
		strcpy(gman_node->domain,domain);
	}
	MyString owner_key(owner);
	if(attr_value){
		MyString attr_key(attr_value);
		owner_key += attr_key;
	}
	if (cluster) {
		char tmp[100];
		sprintf(tmp,"-%d.%d",cluster,proc);
		MyString job_key(tmp);
		owner_key += job_key;
	}

	ASSERT( gman_pid_table->insert(owner_key,gman_node) == 0 );

	// start timer to signal gridmanager if we haven't already
	if ( gman_node->add_timer_id == -1 ) {  // == -1 means no timer set
		gman_node->add_timer_id = daemonCore->Register_Timer(job_added_delay,
			(TimerHandler) &GridUniverseLogic::SendAddSignal,
			"GridUniverseLogic::SendAddSignal");
		daemonCore->Register_DataPtr(gman_node);
	}

	// All done
	return gman_node;
}
