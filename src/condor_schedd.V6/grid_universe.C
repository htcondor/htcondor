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

#include "condor_common.h"

#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_config.h"
#include "globus_utils.h"
#include "grid_universe.h"

// Initialize static data members
const int GridUniverseLogic::job_added_delay = 3;
const int GridUniverseLogic::job_removed_delay = 2;
GridUniverseLogic::GmanPidTable_t * GridUniverseLogic::gman_pid_table = NULL;
int GridUniverseLogic::rid = -1;

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
	if ( gman_pid_table ) {
		gman_node_t * node;
		gman_pid_table->startIterations();
		while (gman_pid_table->iterate( node ) == 1 ) {
			if ( node->add_timer_id >= 0 ) {
				daemonCore->Cancel_Timer(node->add_timer_id);
			}
			if ( node->remove_timer_id >= 0 ) {
				daemonCore->Cancel_Timer(node->remove_timer_id);
			}
			if ( node->pid > 0 ) {
				daemonCore->Send_Signal(node->pid,DC_SIGQUIT);
			}
			delete node;
		}
		delete gman_pid_table;
		rid = 0;
	}
	gman_pid_table = NULL;
	return;
}



void 
GridUniverseLogic::JobCountUpdate(const char* owner, int num_globus_jobs,
			int num_globus_unsubmitted_jobs)
{
	// Quick sanity checks - this should never be...
	ASSERT( num_globus_jobs >= num_globus_unsubmitted_jobs );
	ASSERT( num_globus_jobs >= 0 );
	ASSERT( num_globus_unsubmitted_jobs >= 0 );

	// if there are unsubmitted jobs, the gridmanager apparently
	// does not know they are in the queue. so tell it some jobs
	// were added.
	if ( num_globus_unsubmitted_jobs > 0 ) {
		JobAdded(owner);
		return;
	}

	// now that we've taken care of unsubmitted globus jobs, see if there 
	// are any globus jobs at all.  if there are, make certain that there
	// is a grid manager watching over the jobs and start one if there isn't.
	if ( num_globus_jobs > 0 ) {
		StartOrFindGManager(owner);
		return;
	}

	// if made it here, there is nothing we have to do
	return;
}


void 
GridUniverseLogic::JobAdded(const char* owner)
{
	gman_node_t* node;

	node = StartOrFindGManager(owner);

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
GridUniverseLogic::JobRemoved(const char* owner)
{
	gman_node_t* node;

	node = StartOrFindGManager(owner);

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
	// Reclaim memory from the node itself
	delete gman_node;

	return 0;
}


GridUniverseLogic::gman_node_t *
GridUniverseLogic::lookupGmanByOwner(const char* owner)
{
	gman_node_t* result = NULL;
	MyString owner_key(owner);

	if (!gman_pid_table) {
		// destructor has already been called; we are probably
		// closing down.
		return NULL;
	}

	gman_pid_table->lookup(owner_key,result);

	return result;
}

GridUniverseLogic::gman_node_t *
GridUniverseLogic::StartOrFindGManager(const char* owner)
{
	gman_node_t* gman_node;
	int pid;

	if ( (gman_node=lookupGmanByOwner(owner)) ) {
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

	dprintf( D_FULLDEBUG, "Starting condor_gmanager for owner %s\n",
			owner);

	char *gman_binary;
	gman_binary = param("GRIDMANAGER");
	if ( !gman_binary ) {
		dprintf(D_ALWAYS,"ERROR - GRIDMANAGER not defined in config file\n");
		return NULL;
	}

	char *gman_args;
	gman_args = param("GRIDMANAGER_ARGS");
	char gman_final_args[_POSIX_ARG_MAX];
	if (gman_args) {
		sprintf(gman_final_args,"condor_gridmanager -f %s",gman_args);
		free(gman_args);
	} else {
		sprintf(gman_final_args,"condor_gridmanager -f");
	}
	dprintf(D_FULLDEBUG,"Execing %s\n",gman_final_args);

	init_user_ids(owner);

	pid = daemonCore->Create_Process( 
		gman_binary,			// Program to exec
		gman_final_args,		// Command-line args
		PRIV_USER_FINAL,		// Run as the Owner
		rid						// Reaper ID
		);

	if ( pid <= 0 ) {
		dprintf ( D_ALWAYS, "StartOrFindGManager: Create_Process problems!\n" );
		return NULL;
	}

	// If we made it here, we happily started up a new gridmanager process

	dprintf( D_ALWAYS, "Started condor_gmanager for owner %s pid=%d\n",
			owner,pid);

	// Make a new gman_node entry for our hashtable & insert it
	gman_node = new gman_node_t;
	gman_node->pid = pid;
	MyString owner_key(owner);
	ASSERT( gman_pid_table->insert(owner,gman_node) == 0 );

	// All done
	return gman_node;
}
