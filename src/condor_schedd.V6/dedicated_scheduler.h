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

#include "condor_classad.h"
#include "list.h"
#include "scheduler.h"


enum AllocStatus { A_NEW, A_RUNNING, A_DYING };

class CAList : public List<ClassAd> {};
class MRecArray : public ExtArray<match_rec*> {};

class AllocationNode {
 public:
	AllocationNode( int cluster_id, int n_procs );
	~AllocationNode();

		// Methods
	void addResource( ClassAd* r, int proc );
	void setCapability( const char* new_capab );

		// Data
	int status;
	char* capability;	// The capability for the first match in the cluster 
	int cluster;		// cluster id of the job(s) for this allocation
	int num_procs;			// How many procs are in the cluster
	ExtArray< ClassAd* >* jobs;		// Both arrays are indexed by proc
	ExtArray< MRecArray* >* matches;
	int num_resources;		// How many total resources have been allocated
};		


class ResTimeNode {
 public:
	ResTimeNode( time_t t );
	~ResTimeNode();
	
		// Can we satisfy the given job with this ResTimeNode?  No
		// matter what we return, num_matches is reset to the number
		// of matches we found at this time, and the candidates list
		// includes a copy of each resource ad we matched with.
	bool satisfyJob( ClassAd* jobAd, int max_hosts,
					 CAList* candidates );

	time_t time;
	CAList* res_list;
	int num_matches;
};


class AvailTimeList : public List<ResTimeNode> {
 public:
	~AvailTimeList();
	void display( int debug_level );
	void addResource( match_rec* mrec );
	bool hasAvailResources( void );
	void removeResource( ClassAd* resource, ResTimeNode* rtn );
};


class DedicatedScheduler : public Service {
 public:
	DedicatedScheduler();
	~DedicatedScheduler();

		// Called at start-up to initialize this class.  This does the
		// work of finding which dedicated resources we control, and
		// starting the process of claiming them.
	int initialize( void );
	int shutdown_fast( void );
	int shutdown_graceful( void );
	int	reconfig( void );

		// Called everytime we want to process the job queue and
		// schedule/spawn MPI jobs.
	int handleDedicatedJobs( void );

		// Let the outside world know what we're doing... how exactly
		// this should work is still unclear
	void displaySchedule( void );

	void listDedicatedJobs( int debug_level );
	void listDedicatedResources( int debug_level, ClassAdList* resources );

		// Used for claiming/releasing startds we control
	bool requestClaim( ClassAd* r );
	int	startdContactSockHandler( Stream* sock );
	bool releaseClaim( match_rec* m_rec, bool use_tcp = true );
	bool deactivateClaim( match_rec* m_rec );
	void sendAlives( void );

		// Reaper for the MPI shadow
	int reaper( int pid, int status );

	int giveMatches( int cmd, Stream* stream );

		// These are public, since the Scheduler class needs to call
		// them from vacate_service and possibly other places, too.
	bool DelMrec( char* capability );
	bool DelMrec( match_rec* rec );

	char* name( void ) { return ds_name; };
	char* owner( void ) { return ds_owner; };

 private:
		// This gets a list of all dedicated resources we control.
		// This is called at the begining of each handleDedicatedJobs
		// cycle.
	bool getDedicatedResourceInfo( void );

		// This one should be seperated out, and most easy to change.
	bool computeSchedule( void );

		// This scans the whole job queue, and makes a list of all
		// Dedicated universe job ads, computes meta info (totals by
		// user, etc).  This is called at the begining of each
		// handleDeicatedJobs cycle.
	bool getDedicatedJobs( void );

		// This does the work of acting on a schedule, once that's
		// been decided.  
	bool spawnJobs( void );

	void sortResources( void );

        // Used to give matches to the mpi shadow when it asks for
		// them. 
    int giveMPIMatches( Service*, int cmd, Stream* stream );

		// Remove all startds from a given match.
	void nukeMpi ( shadow_rec* srec );

		// // // // // // 
		// Data members 
		// // // // // // 

		// Stuff for interacting w/ DaemonCore
	int		scheduling_interval;		// How often do we schedule?
	int		scheduling_tid;				// DC timer id
	int		rid;						// DC reaper id

	ExtArray<ClassAd*>*	idle_jobs;		// Only dedicated jobs
	ClassAdList*		resources;		// All dedicated resources

		// All resources, sorted by the time they'll next be available 
	AvailTimeList*			avail_time_list;	

        // hashed on cluster, all our allocations
    HashTable <int, AllocationNode*>* allocations;

		// hashed on capability, each claim we have
	HashTable <HashKey, match_rec*>* all_matches;

		// hashed on capability, each claim we've allocated to a job
	HashTable <HashKey, match_rec*>* allocated_matches;

	int		num_matches;	// Total number of matches in all_matches 

    static const int MPIShadowSockTimeout;

	ClassAd		dummy_job;		// Dummy ad used for claiming startds 

	char* ds_name;		// Name of this dedicated scheduler
	char* ds_owner;		// "Owner" to identify this dedicated scheduler 

};


// ////////////////////////////////////////////////////////////
//   Utility functions
// ////////////////////////////////////////////////////////////

// Find when a given resource will next be available
time_t findAvailTime( match_rec* mrec );

// Comparison function for sorting job ads by QDate
int jobSortByDate( const void* ptr1, const void* ptr2 );

// Print out
void displayResource( ClassAd* ad, char* str, int debug_level );

char* makeCapability( ClassAd* ad, char* addr = NULL );
