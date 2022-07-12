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


/*

  TODO:

  * Really handle multiple procs per cluster, throughout the module
  * Be smarter about dealing with things in the future
  * Have a real scheduling algorithm
  * How do we interact w/ MAX_JOBS_RUNNING and all that stuff when
    we're negotiating for resource requests and not really jobs?
  * Flocking

*/

#include "condor_common.h"
#include "condor_daemon_core.h"
#include "dedicated_scheduler.h"
#include "scheduler.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_query.h"
#include "condor_adtypes.h"
#include "condor_state.h"
#include "string_list.h"
#include "condor_attributes.h"
#include "proc.h"
#include "exit.h"
#include "dc_startd.h"
#include "qmgmt.h"
#include "condor_qmgr.h"
#include "schedd_negotiate.h"

#include <vector>
#include <algorithm>

extern Scheduler scheduler;
extern DedicatedScheduler dedicated_scheduler;
extern char* Name;

extern void mark_job_running(PROC_ID*);
extern void mark_job_stopped(PROC_ID*);
extern int Runnable(PROC_ID*);

/*
  Stash this value as a static variable to this whole file, since we
  use it again and again.  Everytime we call handleDedicatedJobs(), we
  update the value
*/
static time_t now;

const int DedicatedScheduler::MPIShadowSockTimeout = 60;

void removeFromList(SimpleList<PROC_ID> *, CAList *);

//////////////////////////////////////////////////////////////
//  AllocationNode
//////////////////////////////////////////////////////////////

AllocationNode::AllocationNode( int cluster_id, int n_procs )
{
	cluster = cluster_id;
	num_procs = n_procs;
	claim_id = NULL;
	status = A_NEW;
	num_resources = 0;

	jobs = new ExtArray< ClassAd* >(num_procs);
	matches = new ExtArray< MRecArray* >(num_procs);
	jobs->fill(NULL);
	matches->fill(NULL);
	is_reconnect = false;
}


AllocationNode::~AllocationNode()
{
	int i;
	for( i=0; i<num_procs; i++ ) {
		if( (*matches)[i] ) {
			delete (*matches)[i];
		}
	}
	delete jobs;
	delete matches;
	free(claim_id);
}


void
AllocationNode::setClaimId( const char* new_id )
{
	free(claim_id);
	claim_id = strdup( new_id );
}


void
AllocationNode::display( void ) const
{
	const int level = D_FULLDEBUG;
	if( ! IsFulldebug(D_FULLDEBUG) ) {
		return;
	}
	dprintf( level, "Allocation for job %d.0, nprocs: %d\n",
			 cluster, num_procs );
	int p, n, num_nodes;
	MRecArray* ma;
	match_rec* mrec;
	char buf[256];
	for( p=0; p<num_procs; p++ ) {
		ma = (*matches)[p];
		if( ! ma ) {
			return;
		}
		num_nodes = ma->getlast();
		for( n=0; n<=num_nodes; n++ ) {
			mrec = (*ma)[n];
			if( mrec ) {
				snprintf( buf, 256, "%d.%d.%d: ", cluster, p, n );
				displayResource( mrec->my_match_ad, buf, level );
			}
		}
	}
}


bool satisfies(ClassAd* job, ClassAd* candidate) {
	// Make sure the job requirements are satisfied with this resource.
    bool satisfied_req = true;
	if (!job || EvalBool(ATTR_REQUIREMENTS, job, candidate, satisfied_req) == 0) {
		// If it's undefined, treat it as false.
		satisfied_req = false;
	}
    // if reqs weren't satisfied, it's an immediate failure
    if (!satisfied_req) return false;


    // Concurrency limit checking
    // This is relevant for reused claim candidates
    bool satisfied_lim = true;
    std::string resource_state;
    candidate->LookupString(ATTR_STATE, resource_state);
    lower_case(resource_state);
    if ((resource_state == "claimed") && param_boolean("CLAIM_RECYCLING_CONSIDER_LIMITS", true)) {
        dprintf(D_FULLDEBUG, "Entering ded-schedd concurrency limit check...\n");
		std::string jobLimits, resourceLimits;
		job->LookupString(ATTR_CONCURRENCY_LIMITS, jobLimits);
		candidate->LookupString(ATTR_CONCURRENCY_LIMITS, resourceLimits);
		lower_case(jobLimits);
		lower_case(resourceLimits);

        dprintf(D_FULLDEBUG, "job limit: \"%s\"\n", jobLimits.c_str());
        dprintf(D_FULLDEBUG, "candidate limit: \"%s\"\n", resourceLimits.c_str());

        // a claimed resource with limits that are equal is considered a match
		if (jobLimits == resourceLimits) {
			dprintf(D_FULLDEBUG, "ConcurrencyLimits match, can reuse claim\n");
		} else {
			dprintf(D_FULLDEBUG, "ConcurrencyLimits do not match, cannot reuse claim\n");
            satisfied_lim = false;
		}
        dprintf(D_FULLDEBUG, "Leaving ded-schedd concurrency limit check...\n");
	}


    // We already satisfied requirements; did we also satisfy concurrency limits?
    return satisfied_lim;
}


bool is_partitionable(const ClassAd* slot) {
    bool part = false;
    if (!slot->LookupBool(ATTR_SLOT_PARTITIONABLE, part)) part = false;
    return part;
}

bool is_dynamic(ClassAd* slot) {
    bool dyn = false;
    if (!slot->LookupBool(ATTR_SLOT_DYNAMIC, dyn)) dyn = false;
    return dyn;
}

bool is_claimed(ClassAd* slot) {
    std::string state;
    if (!slot->LookupString(ATTR_STATE, state)) return false;
    lower_case(state);
    return state == "claimed";
}

bool is_idle(ClassAd* slot) {
    std::string activ;
    if (!slot->LookupString(ATTR_ACTIVITY, activ)) return false;
    lower_case(activ);
    return activ == "idle";
}


//////////////////////////////////////////////////////////////
//  ResList
// This is a list of resources, sorted by rank, then clusterid
//////////////////////////////////////////////////////////////

ResList::ResList()
{
	num_matches = 0;
}

ResList::~ResList()
{
}



// Run through the list of jobs, assumed to be procs of the
// same cluster, and try to match machine classads from
// this list.  For each match, append the machine ClassAd
// to the "candidates" list, and the job that match to
// the parallel canidates_jobs list
bool
ResList::satisfyJobs( CAList *jobs, 
					  CAList* candidates,
					  CAList* candidates_jobs,
					  bool sort /* = false */ )
{
    // Address the case of empty resource list up front
    if (this->Number() <= 0) return false;

    dprintf(D_FULLDEBUG, "satisfyJobs: testing %d jobs against %d slots\n", (int)jobs->Number(), (int)this->Number());

	jobs->Rewind();

		// Pull the first job off the list, and use its RANK to rank
		// the machines in this ResList

		// Maybe we should re-sort for each job in this cluster, but that
		// seems like a lot of work, and the RANKs should usually be the
		// same.

    if( sort) {

		ClassAd *rankAd = NULL;
		rankAd = jobs->Next();
		jobs->Rewind();

			// rankAd shouldn't ever be null, but just in case
		if (rankAd != NULL) {
			this->sortByRank(rankAd);
		}
	}

		// Foreach job in the given list
	while (ClassAd *job = jobs->Next()) {
        int cluster=0;
        int proc=0;
        job->LookupInteger(ATTR_CLUSTER_ID, cluster);
        job->LookupInteger(ATTR_PROC_ID, proc);
        dprintf(D_FULLDEBUG, "satisfyJobs: finding resources for %d.%d\n", cluster, proc);
		this->Rewind();
		while (ClassAd* candidate = this->Next()) {
			if (satisfies(job, candidate)) {
                // There's a match
                std::string slotname;
                candidate->LookupString(ATTR_NAME, slotname);
                dprintf(D_FULLDEBUG, "satisfyJobs:     %d.%d satisfied with slot %s\n", cluster, proc, slotname.c_str());

				candidates->Insert( candidate );
				candidates_jobs->Insert( job );

					// Give the match to the candidate, it is the caller's
					// responsibility to put it back if they don't want to 
					// use it.
				this->DeleteCurrent();

					// This job is matched, don't use it again.
				jobs->DeleteCurrent();

				if( jobs->Number() == 0) {
						// No more jobs to match, our work is done.
                    dprintf(D_FULLDEBUG, "satisfyJobs: jobs were satisfied\n");
					return true;
				}
				break; // try to match the next job
			}
		}
	}
	return false;
}


struct rankSortRec {
	ClassAd *machineAd;
	float rank;
};

	// Given a job classAd, sort the machines in this reslist
	// according to the job's RANK
void
ResList::sortByRank(ClassAd *rankAd) {

	this->Rewind();

	struct rankSortRec *array = new struct rankSortRec[this->Number()];
	ASSERT(array);
	int index = 0;
	ClassAd *machine = NULL;

		// Foreach machine in this list,
	while ((machine = this->Next())) {
			// If RANK undefined, default value is small
		float rank = 0.0;
		EvalFloat(ATTR_RANK, rankAd, machine, rank);

			// and stick this machine and its rank in our array...
		array[index].machineAd = machine;
		array[index].rank      = rank;
		index++;

			// Remove it from this list, we'll return them in RANK order
		this->DeleteCurrent();
	}

		// and sort it
	std::sort(array, array + index, ResList::machineSortByRank);

		// Now, rebuild our list in order
	for (int i = 0; i < index; i++) {
		this->Insert(array[i].machineAd);
	}

	delete [] array;
}

/* static */ bool
ResList::machineSortByRank(const struct rankSortRec &lhs, const struct rankSortRec &rhs) {
	return lhs.rank > rhs.rank;
}

void
ResList::selectGroup( CAList *group,
					  const char   *groupName) {
	this->Rewind();
	ClassAd *machine;

		// For each machine in the whole list
	while ((machine = this->Next())) {
		char *thisGroupName = 0;
		machine->LookupString(ATTR_PARALLEL_SCHEDULING_GROUP, &thisGroupName);

			// If it has a groupname, and its the same as the param
		if (thisGroupName && 
			(strcmp(thisGroupName, groupName) == 0)) {
			group->Append(machine); // Not transfering ownership!
		}

		if (thisGroupName) {
			free(thisGroupName);
		}
	}
}

void
ResList::display( int debug_level )
{
	if( Number() == 0) {
		dprintf( debug_level, " ************ empty ************ \n");
		return;
	}

	ClassAd* res;
	Rewind();
	while( (res = Next()) ) {
		displayResource( res, "   ", debug_level );
	}
}
//////////////////////////////////////////////////////////////
//  CandidateList
//////////////////////////////////////////////////////////////

CandidateList::CandidateList() {}
CandidateList::~CandidateList() {}

void 
CandidateList::appendResources( ResList *res )
{
	Rewind();
	while (ClassAd *c = Next()) {
		res->Append(c);
		DeleteCurrent();
	}
}

void 
CandidateList::markScheduled()
{
	Rewind();
	while (ClassAd *c = Next()) {
		std::string buf;
		match_rec *mrec = dedicated_scheduler.getMrec(c, buf);
		if( ! mrec) {
			EXCEPT(" no match for %s, but listed as available", buf.c_str());
		}
		mrec->scheduled = true;
	}
}


//////////////////////////////////////////////////////////////
//  DedicatedScheduler
//////////////////////////////////////////////////////////////

DedicatedScheduler::DedicatedScheduler()
{
	idle_clusters = NULL;
	resources = NULL;

	idle_resources = NULL;
	serial_resources = NULL;
	limbo_resources = NULL;
	unclaimed_resources = NULL;
	busy_resources = NULL;

	total_cores = 0;
	hdjt_tid = -1;
	sanity_tid = -1;
	rid = -1;

	allocations = new HashTable < int, AllocationNode*>( hashFuncInt );

	pending_preemptions = NULL;

	all_matches = new HashTable < std::string, match_rec*>( hashFunction );
	all_matches_by_id = new HashTable < std::string, match_rec*>( hashFunction );

	num_matches = 0;

	unused_timeout = 0;

	ds_owner = NULL;
	ds_name = NULL;

	startdQueryTime = 0;
	split_match_count = 0;
}


DedicatedScheduler::~DedicatedScheduler()
{
	if(	idle_clusters ) { delete idle_clusters; }
	if(	resources ) { delete resources; }

	if (idle_resources) {delete idle_resources;}
	if (serial_resources) {delete serial_resources;}
	if (limbo_resources) {delete limbo_resources;}
	if (unclaimed_resources) {delete unclaimed_resources;}
	if (busy_resources) { delete busy_resources;}

	if( ds_owner ) { free(ds_owner); }
	if( ds_name ) { free(ds_name); }

	if( daemonCore && hdjt_tid != -1 ) {
		daemonCore->Cancel_Timer( hdjt_tid );
	}
	if( daemonCore && sanity_tid != -1 ) {
		daemonCore->Cancel_Timer( sanity_tid );
	}

        // for the stored claim records
	AllocationNode* foo;
	match_rec* tmp;
    allocations->startIterations();
    while( allocations->iterate( foo ) ) {
        delete foo;
	}
    delete allocations;

		// First, delete the hashtable where we hash based on
		// ClaimId strings.  Don't actually delete the match
		// records themselves, since we'll do those to the main
		// records stored in all_matches.
	delete all_matches_by_id;

		// Now, we can clear out the actually match records, too. 
	all_matches->startIterations();
    while( all_matches->iterate( tmp ) ) {
        delete tmp;
	}
	delete all_matches;

		// Clear out the resource_requests queue
	clearResourceRequests();  	// Delete classads in the queue
}


int
DedicatedScheduler::initialize( void )
{
	char buf[256];
	char *tmp, *tmpname;
	

		// First, figure out what ds_owner and ds_name should be.
	if( ! Name ) {
		EXCEPT( "DedicatedScheduler::initialize(): Name is NULL" ); 
	}
	tmpname = strdup( Name );
	if( (tmp = strchr(tmpname, '@')) ) {
			// There's an '@', so use everything in front of it to
			// uniquely identify this dedicated scheduler on this
			// host.  ATTR_OWNER is such a yucky attribute anyway, we
			// care much more about ATTR_USER and ATTR_SCHEDULER. 
		*tmp = '\0';
		snprintf( buf, 256, "DedicatedScheduler@%s", tmpname );
	} else {
			// No '@'... we're alone on this host, and can just use a 
			// simple string...
		snprintf( buf, 256, "DedicatedScheduler" );
	}
	free( tmpname );
	ds_owner = strdup( buf );

	snprintf( buf, 256, "DedicatedScheduler@%s", Name );
	ds_name = strdup( buf );

		// Call our reconfig() method, since any config file stuff we
		// care about should be read at start-up, too.
	this->reconfig();

		// Next, fill in the dummy job ad we're going to send to 
		// startds for claiming them.
	SetMyTypeName( dummy_job, JOB_ADTYPE );
	SetTargetTypeName( dummy_job, STARTD_ADTYPE );
	dummy_job.Assign( ATTR_REQUIREMENTS, true );
	dummy_job.Assign( ATTR_OWNER, ds_owner );
	dummy_job.Assign( ATTR_USER, ds_name );
	dummy_job.Assign( ATTR_SCHEDULER, ds_name );
	dummy_job.Assign( ATTR_JOB_UNIVERSE, CONDOR_UNIVERSE_MPI );
	dummy_job.Assign( ATTR_JOB_STATUS, IDLE );
	dummy_job.Assign( ATTR_IMAGE_SIZE, 10 ); // WANT_SUSPEND often needs this
	dummy_job.Assign( ATTR_REQUEST_MEMORY, 10 ); // Dynamic slots needs this
	dummy_job.Assign( ATTR_REQUEST_DISK, 10 ); //     and this
	config_fill_ad( &dummy_job );

		// Next, register our special MPI shadow reaper
	rid = daemonCore->
		Register_Reaper( "MPI reaper", 
						 (ReaperHandlercpp)&DedicatedScheduler::reaper,
						 "DedicatedScheduler::reaper", this );
	if( rid <= 0 ) {
			// This is lame, but Register_Reaper returns FALSE on
			// failure, even though it seems like reaper id 0 is
			// valid... who knows.
		EXCEPT( "Can't register daemonCore reaper!" );
	}

		// Now, register a handler for the special command that the
		// MPI shadow sends us if it needs to get information about
		// the matches for a given cluster.proc
	daemonCore->
		Register_Command( GIVE_MATCHES, "GIVE_MATCHES", 
						  (CommandHandlercpp)&DedicatedScheduler::giveMatches,
						  "DedicatedScheduler::giveMatches", this, 
						  DAEMON );

	return TRUE;
}


int
DedicatedScheduler::reconfig( void )
{
	static int old_unused_timeout = 0;

	unused_timeout = param_integer( "UNUSED_CLAIM_TIMEOUT", 600 );

	if( old_unused_timeout && (old_unused_timeout != unused_timeout) ) {
			// We've got a new value.  We should re-check our sanity. 
		dprintf( D_FULLDEBUG, "New value for UNUSED_CLAIM_TIMEOUT (%d)\n", 
				 unused_timeout );
		checkSanity();
	}
	old_unused_timeout = unused_timeout;

	return TRUE;
}


int
DedicatedScheduler::shutdown_fast( void )
{
		// TODO: any other cleanup?
	match_rec* mrec;
	all_matches->startIterations();
    while( all_matches->iterate( mrec ) ) {
		releaseClaim( mrec );
	}
	return TRUE;
}


int
DedicatedScheduler::shutdown_graceful( void )
{
		// TODO: any other cleanup?
	match_rec* mrec;
	all_matches->startIterations();
    while( all_matches->iterate( mrec ) ) {
		releaseClaim( mrec );
	}
	return TRUE;
}

bool
DedicatedScheddNegotiate::scheduler_getJobAd( PROC_ID job_id, ClassAd &job_ad )
{
	ClassAd *ad = GetJobAd( job_id.cluster, job_id.proc );
	if( !ad ) {
		return false;
	}

	ClassAd *generic_ad = dedicated_scheduler.makeGenericAdFromJobAd( ad );
	FreeJobAd( ad );

	if( !generic_ad ) {
		return false;
	}

	job_ad = *generic_ad;
	delete generic_ad;

	return true;
}


bool
DedicatedScheddNegotiate::scheduler_getRequestConstraints(PROC_ID /*job_id*/, ClassAd &/*request_ad*/, int * match_max)
{
	// No constraints?
	if (match_max) { *match_max = INT_MAX; }
	return true;
}


bool
DedicatedScheddNegotiate::scheduler_skipJob(JobQueueJob *jobad, ClassAd * /*match_ad*/, bool &skip_all, const char * &because) // match ad may be null
{
	because = "got enough matches";
	skip_all = false;
	if( !jobad ) {
		because = "job was removed";
		return true;
	}

	if (!m_jobs) {
		// This is a fast claim of a split dynamic resource
		dedicated_scheduler.incrementSplitMatchCount();
		if (dedicated_scheduler.getSplitMatchCount() > dedicated_scheduler.getResourceRequestSize()) {
                        dprintf(D_FULLDEBUG, "Skipping further matches: (split match count) %d > %d (resource request size)\n", dedicated_scheduler.getSplitMatchCount(), dedicated_scheduler.getResourceRequestSize());
			FreeJobAd( jobad );
			return true;
		}
	}
	FreeJobAd( jobad );
	return false;
}

bool
DedicatedScheddNegotiate::scheduler_handleMatch(PROC_ID job_id,char const *claim_id, char const *extra_claims, ClassAd &match_ad, char const *slot_name)
{
	ASSERT( claim_id );
	ASSERT( slot_name );

	dprintf(D_FULLDEBUG,
			"DedicatedScheduler: Received match for job %d.%d: %s\n",
			job_id.cluster, job_id.proc, slot_name);

	bool skip_all = false;
	const char * because = "";
	if( scheduler_skipJob(GetJobAd(job_id), &match_ad, skip_all, because) ) {
		dprintf(D_FULLDEBUG,
				"DedicatedScheduler: job %d.%d %s.\n",
				job_id.cluster,job_id.proc,
				because);
			// TODO: see if one of the other parallel jobs matches this machine
		return false;
	}

	Daemon startd(&match_ad,DT_STARTD,NULL);
	if( !startd.addr() ) {
		dprintf( D_ALWAYS, "Can't find address of startd in match ad:\n" );
		dPrintAd(D_ALWAYS, match_ad);
		return false;
	}

	match_rec *mrec = dedicated_scheduler.AddMrec(claim_id,startd.addr(),slot_name,job_id,&match_ad,getRemotePool());
	if( !mrec ) {
		return false;
	}

	ContactStartdArgs *args = new ContactStartdArgs( claim_id, extra_claims, startd.addr(), true );

	if( !scheduler.enqueueStartdContact(args) ) {
		delete args;
		dedicated_scheduler.DelMrec( mrec );
		return false;
	}

	return true;
}

void
DedicatedScheddNegotiate::scheduler_handleJobRejected(PROC_ID job_id,char const *reason)
{
	ASSERT( reason );

	dprintf(D_FULLDEBUG, "Job %d.%d rejected: %s\n",
			job_id.cluster, job_id.proc, reason);

	if ( job_id.cluster < 0 || job_id.proc < 0 ) {
		// If we asked the negotiator for matches for jobs that can no
		// longer use them, the negotiation code uses a job id of -1.-1.
		return;
	}

	SetAttributeString(
		job_id.cluster, job_id.proc,
		ATTR_LAST_REJ_MATCH_REASON,	reason, NONDURABLE);

	SetAttributeInt(
		job_id.cluster, job_id.proc,
		ATTR_LAST_REJ_MATCH_TIME, (int)time(0), NONDURABLE);
}

void
DedicatedScheddNegotiate::scheduler_handleNegotiationFinished( Sock *sock )
{
	char const *remote_pool = getRemotePool();

	dprintf(D_ALWAYS,"Finished negotiating for %s%s%s: %d matched, %d rejected\n",
			getMatchUser(),
			remote_pool ? " in pool " : "",
			remote_pool ? remote_pool : " in local pool",
			getNumJobsMatched(), getNumJobsRejected() );

	int rval =
		daemonCore->Register_Socket(
			sock, "<Negotiator Socket>", 
			(SocketHandlercpp)&Scheduler::negotiatorSocketHandler,
			"<Negotiator Command>", &scheduler, ALLOW);

	if( rval >= 0 ) {
			// do not delete this sock until we get called back
		sock->incRefCount();
	}
}

int
DedicatedScheduler::negotiate( int command, Sock* sock, char const* remote_pool )
{
		// At this point, we've already read the command int, the
		// owner to negotiate for, and an eom off the wire.  
		// Now, we've just got to handle the per-job negotiation
		// protocol itself.

	ResourceRequestList *requests = new ResourceRequestList;
	int next_cluster = 0;
	std::list<PROC_ID>::iterator id;

	for( id = resource_requests.begin();
		 id != resource_requests.end();
		 id++ )
	{
		ResourceRequestCluster *cluster = new ResourceRequestCluster( ++next_cluster );
		requests->push_back( cluster );
		cluster->addJob( *(id) );
	}

	classy_counted_ptr<DedicatedScheddNegotiate> neg_handler =
		new DedicatedScheddNegotiate(
			command,
			requests,
			owner(),
			remote_pool
		);

		// handle the rest of the negotiation protocol asynchronously
	neg_handler->negotiate(sock);

	return KEEP_STREAM;
}

void
DedicatedScheduler::handleDedicatedJobTimer( int seconds )
{
	if( hdjt_tid != -1 ) {
			// Nothing to do, we've already been here
		return;
	}
	hdjt_tid = daemonCore->
		Register_Timer( seconds, 0,
				(TimerHandlercpp)&DedicatedScheduler::callHandleDedicatedJobs,
						"callHandleDedicatedJobs", this );
	if( hdjt_tid == -1 ) {
		EXCEPT( "Can't register DC timer!" );
	}
	dprintf( D_FULLDEBUG, 
			 "Started timer (%d) to call handleDedicatedJobs() in %d secs\n",
			 hdjt_tid, seconds );
}


void
DedicatedScheduler::callHandleDedicatedJobs( void )
{
	hdjt_tid = -1;
	handleDedicatedJobs();
}


#ifdef WIN32
#pragma warning(suppress: 6262) // warning: function uses about 64k of stack
#endif
bool
DedicatedScheduler::releaseClaim( match_rec* m_rec )
{
    ReliSock rsock;

	if( ! m_rec ) {
        dprintf( D_ALWAYS, "ERROR in releaseClaim(): NULL m_rec\n" ); 
		return false;
	}


	DCStartd d( m_rec->peer );

    rsock.timeout(2);
	if (!rsock.connect( m_rec->peer)) {
        dprintf( D_ALWAYS, "ERROR in releaseClaim(): canot connect to startd %s\n", m_rec->peer); 
		return false;
	}

	rsock.encode();
    d.startCommand( RELEASE_CLAIM, &rsock);
	rsock.put( m_rec->claimId() );
	rsock.end_of_message();

	if( IsFulldebug(D_FULLDEBUG) ) { 
		char name_buf[256];
		name_buf[0] = '\0';
		m_rec->my_match_ad->LookupString( ATTR_NAME, name_buf, sizeof(name_buf) );
		dprintf( D_FULLDEBUG, "DedicatedScheduler: releasing claim on %s\n", 
				 name_buf );
	}

	DelMrec( m_rec );
	return true;
}


bool
DedicatedScheduler::deactivateClaim( match_rec* m_rec )
{
	ReliSock sock;
	ClassAd responseAd;

	dprintf( D_FULLDEBUG, "DedicatedScheduler::deactivateClaim\n");

	if( ! m_rec ) {
        dprintf( D_ALWAYS, "ERROR in deactivateClaim(): NULL m_rec\n" ); 
		return false;
	}

    sock.timeout( STARTD_CONTACT_TIMEOUT );

	if( !sock.connect(m_rec->peer, 0) ) {
        dprintf( D_ALWAYS, "ERROR in deactivateClaim(): "
				 "Couldn't connect to startd.\n" );
		return false;
	}

	DCStartd d( m_rec->peer );
	if (!d.startCommand(DEACTIVATE_CLAIM, &sock)) {
        	dprintf( D_ALWAYS, "ERROR in deactivateClaim(): "
				 "Can't start command to startd\n" );
		return false;
	}

	sock.encode();

	if( !sock.put(m_rec->claimId()) ) {
        	dprintf( D_ALWAYS, "ERROR in deactivateClaim(): "
				 "Can't code ClaimId (%s)\n", m_rec->publicClaimId() );
		return false;
	}
	if( !sock.end_of_message() ) {
        	dprintf( D_ALWAYS, "ERROR in deactivateClaim(): "
				 "Can't send EOM\n" );
		return false;
	}
	
	// Wait for response from the startd to avoid misleading errors.
	sock.decode();
	// Ignore decode/getClassAd errors - Failure to receive the response classad
	// should "not be critical in any way" (see comment in startd/command.cpp 
	//  - deactivate_claim()).
	getClassAd(&sock, responseAd);

		// Clear out this match rec, since it's no longer allocated to
		// a given MPI job.
	deallocMatchRec( m_rec );

	return true;
}


void
DedicatedScheduler::sendAlives( void )
{
	match_rec	*mrec;
	int		  	numsent=0;
	int now = (int)time(0);
	bool starter_handles_alives = param_boolean("STARTER_HANDLES_ALIVES",true);

	BeginTransaction();

	all_matches->startIterations();
	while( all_matches->iterate(mrec) == 1 ) {
		if( mrec->m_startd_sends_alives == false &&
			( mrec->status == M_ACTIVE || mrec->status == M_CLAIMED ) ) {
			if( sendAlive( mrec ) ) {
				numsent++;
			}
		}

		if (mrec->m_startd_sends_alives && (mrec->status == M_ACTIVE)) {
				// in receive_startd_update, we've updated the lease time only in the job ad
				// actually write it to the job log here in one big transaction.
			int renew_time = 0;
			if ( starter_handles_alives && 
				 mrec->shadowRec && mrec->shadowRec->pid > 0 ) 
			{
				// If we're trusting the existance of the shadow to 
				// keep the claim alive (because of kernel sockopt keepalives),
				// set ATTR_LAST_JOB_LEASE_RENEWAL to the current time.
				renew_time = now;
			} else {
				GetAttributeInt(mrec->cluster,mrec->proc, ATTR_LAST_JOB_LEASE_RENEWAL,&renew_time);
			}
			SetAttributeInt( mrec->cluster, mrec->proc, ATTR_LAST_JOB_LEASE_RENEWAL, renew_time ); 
		}
	}

	CommitTransactionOrDieTrying();

	if( numsent ) {
		dprintf( D_PROTOCOL, "## 6. (Done sending alive messages to "
				 "%d dedicated startds)\n", numsent );
	}
}

int
DedicatedScheduler::reaper( int pid, int status )
{
	shadow_rec*		srec;
	int q_status;  // status of this job in the queue

	dprintf( D_ALWAYS, "In DedicatedScheduler::reaper pid %d has status %d\n", pid, status);

		// No matter what happens, now that a shadow has exited, we
		// want to call handleDedicatedJobs() in a few seconds to see
		// if we can do anything else useful at this point.
	handleDedicatedJobTimer( 2 );

	srec = scheduler.FindSrecByPid( pid );
	if( !srec ) {
			// Can't find the shadow record!  This is bad.
		dprintf( D_ALWAYS, "ERROR: Can't find shadow record for pid %d!\n" 
				 "\t\tAborting DedicatedScheduler::reaper()\n", pid );
		return FALSE;
	}

	if ( daemonCore->Was_Not_Responding(pid) ) {
			// this shadow was killed by daemon core because it was hung.
			// make the schedd treat this like a Shadow Exception so job
			// just goes back into the queue as idle, but if it happens
			// to many times we relinquish the match.
		dprintf( D_ALWAYS, 
				 "Shadow pid %d successfully killed because the shadow was hung.\n",
				 pid );
		status = JOB_EXCEPTION;
	}

	if( GetAttributeInt(srec->job_id.cluster, srec->job_id.proc, 
						ATTR_JOB_STATUS, &q_status) < 0 ) {
		dprintf(D_ALWAYS, "Job (%d.%d) for reaped shadow has already been removed.\n",
				srec->job_id.cluster, srec->job_id.proc );
	}

	if( WIFEXITED(status) ) {			
		dprintf( D_ALWAYS, "Shadow pid %d exited with status %d\n",
				 pid, WEXITSTATUS(status) );

		switch( WEXITSTATUS(status) ) {
		case JOB_NO_MEM:
			scheduler.swap_space_exhausted();
		case JOB_EXEC_FAILED:
			break;
		case JOB_CKPTED:
		case JOB_SHOULD_REQUEUE:
		case JOB_NOT_STARTED:
			if (!srec->removed) {
				shutdownMpiJob( srec , true);
			}
			break;
		case JOB_SHADOW_USAGE:
			EXCEPT("shadow exited with incorrect usage!");
			break;
		case JOB_BAD_STATUS:
			EXCEPT("shadow exited because job status != RUNNING");
			break;
		case JOB_SHOULD_REMOVE:
			dprintf( D_ALWAYS, "Removing job %d.%d\n",
					 srec->job_id.cluster, srec->job_id.proc );
				// set this flag in our shadow record so we treat this
				// just like a condor_rm
			srec->removed = true;
					// no break, fall through and do the action
					//@fallthrough@
		case JOB_NO_CKPT_FILE:
		case JOB_KILLED:
		case JOB_COREDUMPED:
			if( q_status != HELD ) {
				set_job_status( srec->job_id.cluster, srec->job_id.proc,  
								REMOVED );
			}
			break;
		case JOB_EXITED:
			if( q_status != HELD ) {
				set_job_status( srec->job_id.cluster,
								srec->job_id.proc, COMPLETED );
			}
			shutdownMpiJob( srec );
			break;
		case JOB_SHOULD_HOLD:
			if ( q_status != HELD && q_status != REMOVED ) {
				dprintf( D_ALWAYS, "Putting job %d.%d on hold\n",
						 srec->job_id.cluster, srec->job_id.proc );
				set_job_status( srec->job_id.cluster, srec->job_id.proc,
								HELD );
			}
			shutdownMpiJob( srec );
			break;
		case DPRINTF_ERROR:
			dprintf( D_ALWAYS, 
					 "ERROR: Shadow had fatal error writing to its log file.\n" );
				// We don't want to break, we want to fall through 
				// and treat this like a shadow exception for now.
				//@fallthrough@
		case JOB_EXCEPTION:
			if ( WEXITSTATUS(status) == JOB_EXCEPTION ){
				dprintf( D_ALWAYS,
						 "ERROR: Shadow exited with exception code!\n");
			}
				// We don't want to break, we want to fall through 
				// and treat this like a shadow exception for now.
				//@fallthrough@
		default:
				/* the default case is now a shadow exception in case ANYTHING
				   goes wrong with the shadow exit status */
			if ( (WEXITSTATUS(status) != DPRINTF_ERROR) &&
				 (WEXITSTATUS(status) != JOB_EXCEPTION) )
				{
					dprintf( D_ALWAYS,
							 "ERROR: Shadow exited with unknown value!\n");
				}
				// record that we had an exception.  This function will
				// relinquish the match if we get too many
				// exceptions 
			if( !srec->removed ) {
				shutdownMpiJob( srec );
					// GGT  -- I think release_claim will fix this
					//scheduler.HadException( srec->match );
			}
			break;
		}
	} else if( WIFSIGNALED(status) ) {
		dprintf( D_ALWAYS, "Shadow pid %d died with %s\n",
				 pid, daemonCore->GetExceptionString(status) );

		dprintf( D_ALWAYS, "Shadow exited prematurely, "
				 "shutting down MPI computation\n" );

			// If the shadow died with a signal, it's in bad shape,
			// and probably didn't clean up anything.  So, try to
			// shutdown the computation so we don't leave jobs lying
			// around... 
		shutdownMpiJob( srec );
	}

		// Regardless of what happened, we need to remove the
		// allocation for this shadow.
	removeAllocation( srec );

		// Tell the Scheduler class this shadow pid is gone...
	scheduler.delete_shadow_rec( pid );

	return TRUE;
}


/* 
   This command handler reads a cluster int and ClaimId off the
   wire, and replies with the number of matches for that job, followed
   by the ClaimIds for each match, and an EOF.
*/

int
DedicatedScheduler::giveMatches( int, Stream* stream )
{
	int cluster = -1;
	char *id = NULL, *sinful = NULL;
	MRecArray* matches;
	int i, p, last;

	dprintf( D_FULLDEBUG, "Entering DedicatedScheduler::giveMatches()\n" );

	stream->decode();
	if( ! stream->code(cluster) ) {
		dprintf( D_ALWAYS, "ERROR in DedicatedScheduler::giveMatches: "
				 "can't read cluster - aborting\n" );
			// TODO: other cleanup?
		return FALSE;
	}
	if( ! stream->code(id) ) {
		dprintf( D_ALWAYS, "ERROR in DedicatedScheduler::giveMatches: "
				 "can't read ClaimId - aborting\n" );
			// TODO: other cleanup?
		return FALSE;
	}
	
	if( ! stream->end_of_message() ) {
		dprintf( D_ALWAYS, "ERROR in DedicatedScheduler::giveMatches: "
				 "can't read end_of_message - aborting\n" );
			// TODO: other cleanup?
		return FALSE;
	}
		// Now that we have a job id, try to find this job in our
		// table of matches, and make sure the ClaimId is good
	AllocationNode* alloc;
	if( allocations->lookup(cluster, alloc) < 0 ) {
		dprintf( D_ALWAYS, "ERROR in DedicatedScheduler::giveMatches: "
				 "can't find cluster %d in allocation table - aborting\n", 
				 cluster ); 
			// TODO: other cleanup?
		free( id );
		return FALSE;
	}

		// Next, see if the ClaimId we got matches what we have in
		// our table.

	if( strcmp(alloc->claim_id, id) ) {
			// No match, abort
		ClaimIdParser alloc_claim_id_p(alloc->claim_id);
		ClaimIdParser id_p(id);
		dprintf( D_ALWAYS, "ERROR in DedicatedScheduler::giveMatches: "
				 "incorrect ClaimId (%s) given for cluster %d "
				 "- aborting (expecting: %s)\n", id_p.publicClaimId(),
				 cluster, alloc_claim_id_p.publicClaimId() );
			// TODO: other cleanup?
		free( id );
		return FALSE;
	}		

		// Now that we're done checking it, we must free the
		// ClaimId string we got back from CEDAR, since that
		// allocated memory for us
	free( id );
	id = NULL;

		/*
		  If we got here, we successfully read the job id, found the
		  job in our table, and verified the correct ClaimId for
		  the match.  So, we're good to stuff all the required info
		  onto the wire.

		  We stuff in the following form:

		  int num_procs (job "classes", for different requirements, etc)
		  for( p=0, p<num_procs, p++ ) {
		      ClassAd job_ad[p]
		      int num_matches[p]
		      for( m=0, m<num_matches, m++ ) {
		          char* sinful_string_of_startd[p][m]
		          char* ClaimId[p][m]
		      }
		  }
		  EOM
		*/

	stream->encode();

	dprintf( D_FULLDEBUG, "Pushing %d proc(s) for cluster %d\n",
			 alloc->num_procs, cluster );
	if( ! stream->code(alloc->num_procs) ) {
		dprintf( D_ALWAYS, "ERROR in giveMatches: can't send num procs\n" );
		return FALSE;
	}

	for( p=0; p<alloc->num_procs; p++ ) {
		matches = (*alloc->matches)[p];
		last = matches->getlast() + 1; 
		dprintf( D_FULLDEBUG, "In proc %d, num_matches: %d\n", p, last );
		if( ! stream->code(last) ) {
			dprintf( D_ALWAYS, "ERROR in giveMatches: can't send "
					 "number of matches (%d) for proc %d\n", last, p );
			return FALSE;
		}			

		for( i=0; i<last; i++ ) {
			ClassAd *job_ad;
			sinful = (*matches)[i]->peer;
			if( ! stream->code(sinful) ) {
				dprintf( D_ALWAYS, "ERROR in giveMatches: can't send "
						 "address (%s) for match %d of proc %d\n", 
						 sinful, i, p );
				return FALSE;
			}				
			if( ! stream->put( (*matches)[i]->claimId() ) ) {
				dprintf( D_ALWAYS, "ERROR in giveMatches: can't send "
						 "ClaimId for match %d of proc %d\n", i, p );
				return FALSE;
			}				
			//job_ad = new ClassAd( *((*alloc->jobs)[p]) );
			job_ad = dollarDollarExpand(cluster,p, (*alloc->jobs)[p], (*matches)[i]->my_match_ad, true);
			if( ! job_ad) {
				dprintf( D_ALWAYS, "ERROR in giveMatches: "
						 "dollarDollarExpand fails for proc %d\n", p );
				return FALSE;
			}
			if( ! putClassAd(stream, *job_ad) ) {
				dprintf( D_ALWAYS, "ERROR in giveMatches: "
						 "can't send job classad for proc %d\n", p );
				delete job_ad;
				return FALSE;
			}
			delete job_ad;
		}
	}
	if( ! stream->end_of_message() ) {
		dprintf( D_ALWAYS, "ERROR in giveMatches: can't send "
				 "end of message\n" );
		return FALSE;
	}
	return TRUE;
}


void
DedicatedScheduler::clearDedicatedClusters( void )
{
	if( ! idle_clusters ) {
			// we're done
		return;
	}
	delete idle_clusters;
	idle_clusters = NULL;
}


void
DedicatedScheduler::addDedicatedCluster( int cluster )
{
	if( ! idle_clusters ) {
		idle_clusters = new ExtArray<int>;
		idle_clusters->fill(0);
	}
	int next = idle_clusters->getlast() + 1;
	(*idle_clusters)[next] = cluster;
	dprintf( D_FULLDEBUG, "Found idle MPI cluster %d\n", cluster );
}


bool
DedicatedScheduler::hasDedicatedClusters( void )
{
	if( ! idle_clusters ) {
		return false;
	}
	if( idle_clusters->getlast() < 0 ) {
		return false;
	}
	return true;
}


// Go through our list of idle clusters we're supposed to deal with,
// get pointers to all the classads, put it in a big array, and sort
// that array based on QDate.
bool
DedicatedScheduler::sortJobs( void )
{
	ClassAd *job;
	int i, last_cluster, next_cluster, cluster, status;
	ExtArray<int>* verified_clusters;
	
	if( ! idle_clusters ) {
			// No dedicated jobs found, we're done.
		dprintf( D_FULLDEBUG, 
				 "DedicatedScheduler::sortJobs: no jobs found\n" );
		return false;
	}

		// First, verify that all the clusters in our array are still
		// valid jobs, that are still, in fact idle.  If we find any
		// that are no longer jobs or no longer idle, remove them from
		// the array.
	last_cluster = idle_clusters->getlast() + 1;
	if( ! last_cluster ) {
			// No dedicated jobs found, we're done.
		dprintf( D_FULLDEBUG, 
				 "DedicatedScheduler::sortJobs: no jobs found\n" );
		return false;
	}		
	verified_clusters= new ExtArray<int>( last_cluster );
	verified_clusters->fill(0);
	next_cluster = 0;


	// Ded scheduler usually runs jobs in strict FIFO.  If remote submitting, though,
	// jobs are put on hold until spooling finishes.  If a later-submitting job finished
	// spooling first, it will run before a held jobs still spooling, breaking FIFO.  Some
	// sites don't like this.  Enforce strict FIFO in this case:

	int cluster_causing_skippage = INT_MAX;
	bool waitForSpoolers = false;
	waitForSpoolers = param_boolean("DEDICATED_SCHEDULER_WAIT_FOR_SPOOLER", false);

	if (waitForSpoolers) {
		std::string fifoConstraint;
		// "JobUniverse == PARALLEL_UNIVERSE && JobStatus == HELD && HoldReasonCode == HOLD_SpoolingInput

		formatstr(fifoConstraint, "%s == %d && %s == %d && %s == %d", ATTR_JOB_UNIVERSE, CONDOR_UNIVERSE_PARALLEL, 
																ATTR_JOB_STATUS, HELD, 
																ATTR_HOLD_REASON_CODE, CONDOR_HOLD_CODE::SpoolingInput);
		ClassAd *spoolingInJob = NULL;
		bool firstTime = true;
		while ((spoolingInJob = GetNextJobByConstraint(fifoConstraint.c_str(), firstTime))) {
			firstTime = false;
			int spooling_cluster_id = INT_MAX;
			spoolingInJob->LookupInteger(ATTR_CLUSTER_ID, spooling_cluster_id);
			if (spooling_cluster_id < cluster_causing_skippage) {
				cluster_causing_skippage = spooling_cluster_id;
			}
		}
	}

	for( i=0; i<last_cluster; i++ ) {
		cluster = (*idle_clusters)[i];
		job = GetJobAd( cluster, 0 );
		if( ! job ) {
				// This cluster is no longer in the job queue, don't
				// freak out, just move onto the next cluster.
			dprintf( D_FULLDEBUG, "Cluster %d is in idle_clusters, "
					 "but no longer in job queue, removing\n", 
					 cluster );
			continue;
		}
			// If we found the job ad, make sure it's still idle
		if (job->LookupInteger(ATTR_JOB_STATUS, status) == false) {
			dprintf( D_ALWAYS, "Job %d.0 has no %s attribute.  Ignoring\n",  
					 cluster, ATTR_JOB_STATUS );
			continue;

		}

		if (cluster > cluster_causing_skippage) {
			dprintf(D_ALWAYS, "Job %d.0 is spooling input files, will block all %d.0 until spooling completes\n", cluster_causing_skippage, cluster);
			continue;
		}

		if( status != IDLE ) {
			dprintf( D_FULLDEBUG, "Job %d.0 has non idle status (%d).  "
					 "Ignoring\n", cluster, status );
			continue;
		}

		int hosts = 0;
		job->LookupInteger(ATTR_CURRENT_HOSTS, hosts);
		if (hosts > 0) {
			dprintf(D_FULLDEBUG, " job %d is in process of starting, though marked idle, skipping\n",
					cluster);
			continue;
		}

			// If we made it this far, this cluster has been verified,
			// so put it in our array.
		(*verified_clusters)[next_cluster++] = cluster;

			// While we're at it, make sure ATTR_SCHEDULER is right for all procs
		ClassAd *jobIter = job;
		int proc = 0;
		do {
			setScheduler( jobIter );
			proc++;
			jobIter = GetJobAd(cluster, proc);
		} while (jobIter != NULL);
	}

		// No matter what, we want to remove our old array and start
		// using the verified one...
	delete idle_clusters;
	idle_clusters = verified_clusters;

	if( ! next_cluster ) {
		dprintf( D_FULLDEBUG, "Found no idle dedicated job(s)\n" );		
		return false;
	}

	dprintf( D_FULLDEBUG, "Found %d idle dedicated job(s)\n",
			 next_cluster );

		// Now, sort them by prio and qdate
	qsort( &(*idle_clusters)[0], next_cluster, sizeof(int), 
		   clusterSortByPrioAndDate ); 

		// Show the world what we've got
	listDedicatedJobs( D_FULLDEBUG );
	return true;
}
	

int
DedicatedScheduler::handleDedicatedJobs( void )
{
	dprintf( D_FULLDEBUG, "Starting "
			 "DedicatedScheduler::handleDedicatedJobs\n" );

// 	now = (int)time(0);
		// Just for debugging, set now to 0 to make everything easier
		// to parse when looking at log files.
	    // NOTE: this _is_ intended to be checked into CVS.  It may
	    // as well remain this way until the scheduler is improved
	    // to actually care about this value in any meaningful way.
	now = 0;

		// This will gather up pointers to all the job classads we
		// care about, and sort them by QDate.
	if( ! sortJobs() ) {
			// No jobs, we're done.
		dprintf( D_FULLDEBUG, "No idle dedicated jobs, "
				 "handleDedicatedJobs() returning\n" );
		checkSanity();
		return TRUE;
	}

		// See what all the possible resources we *could* get are.  
	if( ! getDedicatedResourceInfo() ) { 
		dprintf( D_ALWAYS, "ERROR: Can't get resources, "
				 "aborting handleDedicatedJobs\n" );
		return FALSE;
	}

		// Sort them, so we know if we can use them or not.
	sortResources();

		// Figure out what we want to do, based on what we have now.  
	if( ! computeSchedule() ) { 
		dprintf( D_ALWAYS, "ERROR: Can't compute schedule, "
				 "aborting handleDedicatedJobs\n" );
		return FALSE;
	}

		// Try to spawn any jobs that we can currently service. 
	if( ! spawnJobs() ) { 
		dprintf( D_ALWAYS, "ERROR: Can't spawn jobs, "
				 "aborting handleDedicatedJobs\n" );
		return FALSE;
	}

		// If we have any resources we need to aquire, ask for them. 
	if( ! requestResources() ) {
		dprintf( D_ALWAYS, "ERROR: Can't request resources, "
				 "aborting handleDedicatedJobs\n" );
		return FALSE;
	}

		// Preempt any machines we selected in computeSchedule
	if( ! preemptResources() ) {
		dprintf( D_ALWAYS, "ERROR: Can't preempt resources, "
				 "aborting handleDedicatedJobs\n" );
		return FALSE;
	}

		// See if we should release claims we're not using.
	checkSanity();
	
		// Free memory allocated above
	clearResources();

	dprintf( D_FULLDEBUG, 
			 "Finished DedicatedScheduler::handleDedicatedJobs\n" );
	return TRUE;
}


void
DedicatedScheduler::listDedicatedJobs( int debug_level )
{
	int cluster, proc;
	std::string owner_str;

	if( ! idle_clusters ) {
		dprintf( debug_level, "DedicatedScheduler: No dedicated jobs\n" );
		return;
	}
	dprintf( debug_level, "DedicatedScheduler: Listing all dedicated "
			 "jobs - \n" );
	int i, last = idle_clusters->getlast();
	for( i=0; i<=last; i++ ) {
		cluster = (*idle_clusters)[i];
		proc = 0;
		owner_str = "";
		GetAttributeString( cluster, proc, ATTR_OWNER, owner_str ); 
		dprintf( debug_level, "Dedicated job: %d.%d %s\n", cluster,
				 proc, owner_str.c_str() );
	}
}


bool
DedicatedScheduler::getDedicatedResourceInfo( void )
{
	StringList config_list;
	CondorQuery	query(STARTD_AD);

	time_t b4 = time(0);
	std::string constraint;

		// Now, clear out any old list we might have for the resources
		// themselves. 
	clearResources();
	
		// Make a new list to hold our resource classads.
	resources = new ClassAdList;

    formatstr(constraint, "DedicatedScheduler == \"%s\"", name());
	query.addORConstraint( constraint.c_str() );

		// This should fill in resources with all the classads we care
		// about
	CollectorList *collectors = daemonCore->getCollectorList();
	if (collectors->query (query, *resources) == Q_OK) {
		startdQueryTime = time(0) - b4;
		dprintf( D_ALWAYS, "Found %d potential dedicated resources in %ld seconds\n",
				 resources->Length(),startdQueryTime);

		resources->Rewind();
		while (ClassAd *m = resources->Next()) {
			int cpus = 0;
			m->LookupInteger(ATTR_CPUS, cpus);
			total_cores += cpus;
		}
		resources->Rewind();

		return true;
	}

	dprintf (D_ALWAYS, "Unable to run query %s\n",  constraint.c_str());
	return false;
}


void duplicate_partitionable_res(ResList*& resources, std::map<std::string, match_rec*> &pending_matches) {
    // This is a way to account for partitionable slots offering 
    // multiple cpus, that makes it easy to use slots fungably and also
    // avoids the need to make pervasive changes to memory 
    // management logic for resource ads.
    ResList* dup_res = new ResList;
    resources->Rewind();
    while (ClassAd* res = resources->Next()) {
        if (!is_partitionable(res)) {
            dup_res->Append(res);
            continue;
        }
        std::string resname;
        res->LookupString(ATTR_NAME, resname);
        int ncpus=0;
        res->LookupInteger(ATTR_CPUS, ncpus);
        int ntotalcpus=0;
        res->LookupInteger(ATTR_TOTAL_SLOT_CPUS, ntotalcpus);
        if (ntotalcpus < ncpus) ntotalcpus = ncpus;
        // Increase ncpus by the number of matches associated to this
        // dynamic slot that haven't appeared in the idle slot list yet.
        // This will prevent a successful match to a set of partitionable
        // slots to turn into an unsuccessful match that will prevent further
        // partitionable slots to be requested from the dynamic slot
        // leftovers.
        int npend=0;
        std::string pname;
        std::map<std::string, match_rec*>::const_iterator mr;
        std::map<std::string, match_rec*>::const_iterator mre = pending_matches.end();
        for (mr = pending_matches.begin(); mr != mre; ++mr) {
            mr->second->my_match_ad->LookupString( ATTR_NAME, pname );
            if (pname == resname) npend++;
        }
        int ndupl = ncpus + npend;
        if (ndupl > ntotalcpus) ndupl = ntotalcpus;

        dprintf(D_FULLDEBUG, "Duplicate x%d (%d/%d) partitionable res %s\n", ndupl, ncpus, npend, resname.c_str());
        for (int j = 0;  j < ndupl;  ++j) dup_res->Append(res);
    }

    delete resources;
    resources = dup_res;
}


void
DedicatedScheduler::sortResources( void )
{
	idle_resources = new ResList;
	serial_resources = new ResList;
	unclaimed_resources = new ResList;
	limbo_resources = new ResList;
	busy_resources = new ResList;

	scheduling_groups.clearAll();

	resources->Rewind();
	while (ClassAd* res = resources->Next()) {
		addToSchedulingGroup(res);

        // new dynamic slots may also show up here, which need to have their
        // match_rec moved from the pending table into the all_matches table
        std::string resname;
        res->LookupString(ATTR_NAME, resname);
        if (is_dynamic(res)) {
            match_rec* dmrec = NULL;
            if (all_matches->lookup(resname, dmrec) < 0) {
                dprintf(D_FULLDEBUG, "New dynamic slot %s\n", resname.c_str());
                if (!(is_claimed(res) && is_idle(res))) {
					// Not actually unexpected -- this is a claimed dynamic slot, claimed by a serial job
                    //dprintf(D_ALWAYS, "WARNING: unexpected claim/activity state for new dynamic slot %s -- ignoring this resource\n", resname.Value());
                    continue;
                }
                std::string pub_claim_id;
                res->LookupString(ATTR_PUBLIC_CLAIM_ID, pub_claim_id);
                std::map<std::string, std::string>::iterator f(pending_claims.find(pub_claim_id));
                if (f != pending_claims.end()) {
                    char const* claim_id = f->second.c_str();
                    std::map<std::string, match_rec*>::iterator c(pending_matches.find(claim_id));
                    if (c != pending_matches.end()) {
                        dmrec = c->second;
                        ASSERT( all_matches->insert(resname, dmrec) == 0 );
                        ASSERT( all_matches_by_id->insert(claim_id, dmrec) == 0 );
                        dmrec->setStatus(M_CLAIMED);
                        pending_matches.erase(c);
                    } else {
                        dprintf(D_ALWAYS, "WARNING: match rec not found for new dynamic slot %s -- ignoring this resource\n", resname.c_str());
                        continue;
                    }
                    pending_claims.erase(f);
                } else {
                    dprintf(D_ALWAYS, "WARNING: claim id not found for new dynamic slot %s -- ignoring this resource\n", resname.c_str());
                    continue;
                }
            }
        }

        // getMrec from the dec sched -- won't have matches for non dedicated jobs
        match_rec* mrec = NULL;
	std::string buf;
        if( ! (mrec = getMrec(res, buf)) ) {
			// We don't have a match_rec for this resource yet, so
			// put it in our unclaimed_resources list
			unclaimed_resources->Append( res );
			continue;
		}

			// If we made it this far, this resource is already
			// claimed by us, so we can add it to the appropriate
			// list of available resources.  

			// First, we want to update my_match_ad which we're
			// storing in the match_rec, since that's now got
			// (probably) stale info, and we always want to use the
			// most recent.

		// Carry any negotiator match attrs over from the existing match ad. 
		// Otherwise these will be lost and dollar-dollar expansion will fail.
		size_t len = strlen(ATTR_NEGOTIATOR_MATCH_EXPR);
		for ( auto itr = mrec->my_match_ad->begin(); itr != mrec->my_match_ad->end(); itr++ ) {
			if( !strncmp(itr->first.c_str(),ATTR_NEGOTIATOR_MATCH_EXPR,len) ) {
				ExprTree *oexpr = itr->second;
				if( !oexpr ) {
					continue;
				}
				ExprTree *nexpr = res->LookupExpr(itr->first);
				if (!nexpr) {
					const char *oexprStr = ExprTreeToString(oexpr);
					res->AssignExpr(itr->first, oexprStr);

					dprintf( D_FULLDEBUG, "%s: Negotiator match attribute %s==%s carried over from existing match record.\n", 
					         resname.c_str(), itr->first.c_str(), oexprStr);
				}
			}
		}
		delete( mrec->my_match_ad );
		mrec->my_match_ad = new ClassAd( *res );

			// If it is active, or on its way to becoming active,
			// mark it as busy
		if( mrec->status == M_ACTIVE ){
			busy_resources->Append( res );
			continue;
		}

		if( mrec->status == M_CLAIMED ) {
			idle_resources->Append( res );
			continue;
		}

		if( mrec->status == M_STARTD_CONTACT_LIMBO ) {
			limbo_resources->Append( res );
			continue;
		}
		if (mrec->status == M_UNCLAIMED) {
			// not quite sure how we got here, or what to do about it
			unclaimed_resources->Append( res );
			continue;
		}
		EXCEPT("DedicatedScheduler got unknown status for match %d", mrec->status);
	}

    duplicate_partitionable_res(unclaimed_resources, pending_matches);

	// If we are configured to steal claimed/idle matches from the serial
	// scheduler, do so here

	if (param_boolean("DEDICATED_SCHEDULER_USE_SERIAL_CLAIMS", false)) {
		match_rec *mr = NULL;
		std::string id;
		scheduler.matches->startIterations();
		while (scheduler.matches->iterate(id, mr) == 1) {
			if (mr->status == M_CLAIMED) {
				// this match rec is claimed/idle, steal it for the ded sched
				mr->needs_release_claim = false;
				scheduler.unlinkMrec(mr);
				mr->is_dedicated = true; // it is now!
				mr->cluster = -1; // dissociate from previous job
				ClassAd *resource = new ClassAd(*mr->my_match_ad);
				dPrintAd(D_ALWAYS, *resource);
				
				serial_resources->Append(resource);
				char *slot_name = NULL;
				resource->LookupString(ATTR_NAME, &slot_name);
				ASSERT( all_matches->insert(slot_name, mr) == 0 );
				ASSERT( all_matches_by_id->insert(mr->claimId(), mr) == 0 );
				free(slot_name);
			}
		}
	}

	if( IsFulldebug(D_FULLDEBUG) ) {
		dprintf(D_FULLDEBUG, "idle resource list\n");
		idle_resources->display( D_FULLDEBUG );

		dprintf(D_FULLDEBUG, "limbo resource list\n");
		limbo_resources->display( D_FULLDEBUG );

		dprintf(D_FULLDEBUG, "serial c/i resource list\n");
		serial_resources->display( D_FULLDEBUG );

		dprintf(D_FULLDEBUG, "unclaimed resource list\n");
		unclaimed_resources->display( D_FULLDEBUG );

		dprintf(D_FULLDEBUG, "busy resource list\n");
		busy_resources->display( D_FULLDEBUG );
	}
}


void
DedicatedScheduler::clearResources( void )
{
		// Now that we're done, free up the memory we allocated, so we
		// don't use up these resources when we don't need them.  If
		// we bail out early, we won't leak, since we delete all of
		// these things again before we need to create them the next
		// time around.
   
	if (idle_resources) {
		delete idle_resources;
		idle_resources = NULL;
	}

	if (serial_resources) {
		serial_resources->Rewind();
		ClassAd *serialMach = NULL;
		while ((serialMach = serial_resources->Next())) {
			char *slot_name = NULL;
			serialMach->LookupString(ATTR_NAME, &slot_name);
			match_rec *mr = NULL;
			if (all_matches->lookup(slot_name, mr) != 0) {
				mr->needs_release_claim = false;
				DelMrec(mr);
			}
			free(slot_name);
		}
		delete serial_resources;
		serial_resources = NULL;
	}

	if (limbo_resources) {
		delete limbo_resources;
		limbo_resources = NULL;
	}

	if (unclaimed_resources) {
		delete unclaimed_resources;
		unclaimed_resources = NULL;
	}

	if (busy_resources) {
		delete busy_resources;
		busy_resources = NULL;
	}

	if( resources ) {
		delete resources;
		resources = NULL;
	}
	total_cores = 0;
}


void
DedicatedScheduler::addToSchedulingGroup(ClassAd *r) {
	char *group = 0;
	r->LookupString(ATTR_PARALLEL_SCHEDULING_GROUP, &group);

	// If this startd is a member of a scheduling group..

	if (group) {
		if (!scheduling_groups.contains(group)) {
			// add it to our list of groups, if it isn't already there
			scheduling_groups.append(group); // doesn't transfer ownership
		}
		free(group);
	}
}

void
DedicatedScheduler::listDedicatedResources( int debug_level,
											ClassAdList* resource_list )
{
	ClassAd* ad;

	if( ! resource_list ) {
		dprintf( debug_level, "DedicatedScheduler: "
				 "No dedicated resources\n" );
		return;
	}
	dprintf( debug_level, "DedicatedScheduler: Listing all "
			 "possible dedicated resources - \n" );
	resource_list->Rewind();
	while( (ad = resource_list->Next()) ) {
		displayResource( ad, "   ", debug_level );
	}
}


bool
DedicatedScheduler::spawnJobs( void )
{
	AllocationNode* allocation;
	match_rec* mrec;
	shadow_rec* srec;
	int univ;
	int i, p, n;
	PROC_ID id;

	if( ! allocations ) {
			// Nothing to do
		return true;
	}

	allocations->startIterations();
	while( allocations->iterate(allocation) ) {
		if( allocation->status != A_NEW ) {
			continue;
		}
		id.cluster = allocation->cluster;
		id.proc = 0;
		MRecArray* cur_matches = (*allocation->matches)[0];
		if( ! cur_matches ) {
			EXCEPT( "spawnJobs(): allocation node has no matches!" );
		}
		mrec = (*cur_matches)[0];
		if( ! mrec ) {
			EXCEPT( "spawnJobs(): allocation node has NULL first match!" );
		}

		dprintf( D_FULLDEBUG, "DedicateScheduler::spawnJobs() - "
				 "job=%d.%d on %s\n", id.cluster, id.proc, mrec->peer );

		GetAttributeInt( id.cluster, id.proc, ATTR_JOB_UNIVERSE, &univ );

			// need to skip following line if it is a reconnect job already
		if (! allocation->is_reconnect) {
			addReconnectAttributes( allocation);
			scheduler.stats.JobsRestartReconnectsAttempting += 1;
		}

			/*
			  it's important we're setting ATTR_CURRENT_HOSTS now so
			  that we don't consider this job idle any more, even
			  though the status will be idle until the shadow actually
			  starts running.  this will prevent us from trying to
			  schedule it again, negotiate for other resources to run
			  it on, or to make another shadow record and add it to
			  the RunnableJobQueue, etc.
			*/

		int total_nodes = 0;
		int procIndex = 0;
		for( procIndex = 0; procIndex < allocation->num_procs; procIndex++) {
			total_nodes += ((*allocation->matches)[procIndex])->getlast() + 1;
		}

			// In each proc's classad, set CurrentHosts to be the
			// total number of nodes for all procs.
		for( procIndex = 0; procIndex < allocation->num_procs; procIndex++) {
			SetAttributeInt( id.cluster, procIndex, ATTR_CURRENT_HOSTS,
							 total_nodes );
		}

			// add job to run queue, though the shadow pid is still 0,
			// since there's not really a shadow just yet.
		srec = scheduler.add_shadow_rec( 0, &id, univ, mrec, -1 );

		srec->is_reconnect = allocation->is_reconnect;

			// add this shadow rec to the scheduler's queue so that
			// it'll honor the JOB_START_DELAY, call the
			// aboutToSpawnJobHandler() hook, and so eventually, we'll
			// spawn the real shadow.
		scheduler.addRunnableJob( srec );

			/*
			  now that this allocation has a shadow record, we need to
			  do some book-keeping to keep our data structures sane.
			  in particular, we want to mark this allocation as
			  running (and store the claim ID for node 0 as this
			  allocation's claim ID), we want to set all the match
			  records to M_ACTIVE, we want to have all the match
			  records point to this shadow record.  we do all this so
			  that we consider all of these resources busy, even
			  though the shadow's not running yet, since we don't want
			  to give any of these resources out to another job while
			  we're waiting for the JOB_START_DELAY, and/or the
			  aboutToSpawnJobHandler() hook to complete.
			*/
		allocation->status = A_RUNNING;
		allocation->setClaimId( mrec->claimId() );

			// We must set all the match recs to point at this srec.
		for( p=0; p<allocation->num_procs; p++ ) {
			n = ((*allocation->matches)[p])->getlast();
			for( i=0; i<=n; i++ ) {
				(*(*allocation->matches)[p])[i]->shadowRec = srec;
				(*(*allocation->matches)[p])[i]->setStatus( M_ACTIVE );
			}
		}
	}
	return true;
}

void
DedicatedScheduler::addReconnectAttributes(AllocationNode *allocation)
{
		// foreach proc in this cluster...

		StringList allRemoteHosts;

		for( int p=0; p<allocation->num_procs; p++ ) {

			StringList claims;
			StringList public_claims;
			StringList remoteHosts;

			int n = ((*allocation->matches)[p])->getlast();

				// Foreach node within this proc...
			for( int i=0; i<=n; i++ ) {
					// Grab the claim from the mrec
				char const *claim = (*(*allocation->matches)[p])[i]->claimId();
				char const *publicClaim = (*(*allocation->matches)[p])[i]->publicClaimId();

				std::string claim_buf;
				if( strchr(claim,',') ) {
						// the claimid contains a comma, which is the delimiter
						// in the list of claimids, so we must escape it
					claim_buf = claim;
					ASSERT( !strstr(claim_buf.c_str(),"$(COMMA)") );
					replace_str(claim_buf, ",","$(COMMA)");
					claim = claim_buf.c_str();
				}

				claims.append(claim);
				public_claims.append(publicClaim);


				char *hosts = matchToHost( (*(*allocation->matches)[p])[i], allocation->cluster, p);
				remoteHosts.append(hosts);
				free(hosts);
			}

			allRemoteHosts.create_union(remoteHosts, false);

			char *claims_str = claims.print_to_string();
			if ( claims_str ) {
				SetPrivateAttributeString(allocation->cluster, p, ATTR_CLAIM_IDS, claims_str);
				free(claims_str);
				claims_str = NULL;
			}

				// For debugging purposes, store a user-visible version of
				// the claim ids in the ClassAd as well.
			char *public_claims_str = public_claims.print_to_string();
			if ( public_claims_str ) {
				SetAttributeString(allocation->cluster, p, ATTR_PUBLIC_CLAIM_IDS, public_claims_str);
				free(public_claims_str);
				public_claims_str = NULL;
			}

			char *hosts_str = remoteHosts.print_to_string();
			if ( hosts_str ) {
				SetAttributeString(allocation->cluster, p, ATTR_REMOTE_HOSTS, hosts_str);
				free(hosts_str);
				hosts_str = NULL;
			}
		}

		char *all_hosts_str = allRemoteHosts.print_to_string();
		ASSERT( all_hosts_str );

		for (int pNo = 0; pNo < allocation->num_procs; pNo++) {
				SetAttributeString(allocation->cluster, pNo, ATTR_ALL_REMOTE_HOSTS, all_hosts_str);
		}
		free(all_hosts_str);
}

char *
DedicatedScheduler::matchToHost(match_rec *mrec, int /*cluster*/, int /*proc*/) {

	if( mrec->my_match_ad ) {
		char* tmp = NULL;
		mrec->my_match_ad->LookupString(ATTR_NAME, &tmp );
		return tmp;
	}
	return NULL;
}

bool
DedicatedScheduler::shadowSpawned( shadow_rec* srec )
{
	if( ! srec ) {
		return false;
	}

	split_match_count = 0;

	int i; 
	PROC_ID id;
	id.cluster = srec->job_id.cluster;

		// Now that we have a job id, try to find this job in our
		// table of matches, and make sure the ClaimId is good
	AllocationNode* allocation;
	if( allocations->lookup(id.cluster, allocation) < 0 ) {
		dprintf( D_ALWAYS, "ERROR in DedicatedScheduler::shadowSpawned(): "
				 "can't find cluster %d in allocation table - aborting\n", 
				 id.cluster ); 
			// TODO: other cleanup?
		return false;
	}

	if (! allocation->is_reconnect) {
		for( i=0; i < allocation->num_procs; i++ ) {
			id.proc = i;
			mark_job_running( &id );
		}
		return true;
	}

	return true;
}


bool
DedicatedScheduler::computeSchedule( void )
{
		// Initialization
		//int proc, cluster, max_hosts;
	int cluster = -1, max_hosts;
	ClassAd *job = NULL, *ad;

	CandidateList *idle_candidates = NULL;
	CAList *idle_candidates_jobs = NULL;

	CandidateList *serial_candidates = NULL;
	CAList *serial_candidates_jobs = NULL;

	CandidateList *limbo_candidates = NULL;
	CAList *limbo_candidates_jobs = NULL;

	CandidateList *unclaimed_candidates = NULL;
	CAList *unclaimed_candidates_jobs = NULL;

	int *nodes_per_proc = NULL;
	match_rec* mrec;
	int i, l;

		//----------------------------------------------------------
		// First, we need to do some clean-up, so we create a fresh
		// schedule from scratch, given the current state of things. 
		//----------------------------------------------------------

		// Clear out the "scheduled" flag in all of our match_recs
		// that aren't already allocated to a job, so we can set them
		// correctly as we create the new schedule.
	all_matches->startIterations();
    while( all_matches->iterate( mrec ) ) {
		if( ! mrec->allocated ) {
			mrec->scheduled = false;
		}
    }

		// Now, we want to remove any stale resource requests we might
		// still have.  If we decided we want to negotiate for some
		// resources the last time around, and those still haven't
		// been serviced, we want to get rid of them at this point.
		// If nothing's changed, we'll make the same decisions this
		// time through, and end up with the same set of requests.  If
		// things have changed, we don't want to act on our old
		// decisions... 
	clearResourceRequests();


	if( pending_preemptions) {
		delete pending_preemptions;
	}
	pending_preemptions = new CAList;

		//----------------------------------------------------------
		// Clean-up is done, actually compute a schedule...
		//----------------------------------------------------------

		// For each job, try to satisfy it as soon as possible.
	CAList *jobs = NULL;
	l = idle_clusters->getlast();
	for( i=0; i<=l; i++ ) {

			// This is the main data structure for handling multiple
			// procs It just contains one non-unique job ClassAd for
			// each node.  So, if there a two procs, with one and two
			// nodes, respectively, jobs will have three entries, and
			// the last two will be the same job ad.
		if (jobs) {
			delete jobs;
		}
		jobs = new CAList;

			// put each job ad per proc in this cluster
			// into jobs
		int nprocs = 0;
		max_hosts = 0;
		bool give_up = false;
		while ( (job = GetJobAd( (*idle_clusters)[i], nprocs))) {
			int hosts = 0;

			if( ! job->LookupInteger(ATTR_CLUSTER_ID, cluster) ) {
					// ATTR_CLUSTER_ID is undefined!
				give_up = true;
				break;
			}

			if( !job->LookupInteger(ATTR_MAX_HOSTS, hosts) ) {
				give_up = true;
				break;
			}
			max_hosts += hosts;

			int proc_id;
			if( !job->LookupInteger(ATTR_PROC_ID, proc_id) ) {
				give_up = true;
				break;
			}

			// If this job is waiting for a reconnect, don't even try here
			jobsToReconnect.Rewind();
			PROC_ID reconId;
			while (jobsToReconnect.Next(reconId)) {
				if ((reconId.cluster == cluster) &&
				    (reconId.proc    == proc_id)) {
					dprintf(D_FULLDEBUG, "skipping %d.%d because it is waiting to reconnect\n", cluster, proc_id);
					give_up = true;
					break;
				}
			}
			dprintf( D_FULLDEBUG, 
					 "Trying to find %d resource(s) for dedicated job %d.%d\n",
					 hosts, cluster, proc_id );

			if (hosts > total_cores) {
				dprintf(D_ALWAYS, "Skipping job %d.%d because it requests more nodes (%d) than exist in the pool (%d)\n", cluster, proc_id, hosts, total_cores);
				nprocs++;
				give_up = true;
				continue;
			}
			for( int job_num = 0 ; job_num < hosts; job_num++) {
				jobs->Append(job);
			}
			nprocs++;
		}	
			
		if (give_up) {
			continue;
		}

			// These are the potential resources for the job
		idle_candidates = new CandidateList;

		    // A parallel list for the jobs that match each above
		idle_candidates_jobs = new CAList;
		
		bool want_groups = false;;
		job = jobs->Head();
		jobs->Rewind();
		job->LookupBool(ATTR_WANT_PARALLEL_SCHEDULING_GROUPS, want_groups);

		if (want_groups) {
			bool foundMatch = false;
			bool psgIsPreferred = false; 
			psgIsPreferred = job->LookupBool(ATTR_PREFER_PARALLEL_SCHEDULING_GROUP, psgIsPreferred);

			// if job had a previous match, set PSG to true, so it can now match any PSG
			if (job->LookupExpr(ATTR_MATCHED_PSG)) {
				std::string psgString;
				if (!job->LookupString(ATTR_MATCHED_PSG, psgString)) {
					// Make sure the Matched_PSG attribute is not a string, this was the
					// old way of doing things, and we'll skip that case.

					// Assume here it is an expression of the form ParallelSchedulingGroup == "somegroup"
					// and set it to true now, to match any group
					job->Assign(ATTR_MATCHED_PSG, true);
				}
			}

			foundMatch = satisfyJobWithGroups(jobs, cluster, nprocs);
			
				// If we found a matching set of machines, or PSG is a hard requirement, we're
				// done now.  Otherwise, keep looking.
			if (foundMatch || !psgIsPreferred) {
 				// we're done with these, safe to delete
				delete idle_candidates;
				idle_candidates = NULL;

				delete idle_candidates_jobs;
				idle_candidates_jobs = NULL;
				continue; // on to the next job
			}
		}

			// First, try to satisfy the requirements of this cluster
			// by going after machine resources that are idle &
			// claimed by us
		if( idle_resources->satisfyJobs(jobs, idle_candidates,
										idle_candidates_jobs, true) )
		{
			printSatisfaction( cluster, idle_candidates, NULL, NULL, NULL, NULL );
			createAllocations( idle_candidates, idle_candidates_jobs,
							   cluster, nprocs, false );
				
 				// we're done with these, safe to delete
			delete idle_candidates;
			idle_candidates = NULL;

			delete idle_candidates_jobs;
			idle_candidates_jobs = NULL;
			continue;				// Go onto the next job.

		}

			// Now, try to satisfy the remaining requirements of this cluster
			// by going after machine resources "stolen" from the serial
			// scheduler that are claimed/idle
		if (serial_resources) {
			serial_candidates = new CandidateList;
			serial_candidates_jobs = new CAList;

			if( serial_resources->satisfyJobs(jobs, serial_candidates,
											serial_candidates_jobs, true) )
			{
				printSatisfaction( cluster, idle_candidates, serial_candidates, NULL, NULL , NULL);
				createAllocations( serial_candidates, serial_candidates_jobs,
								   cluster, nprocs, false );
					
					// we're done with these, safe to delete
				delete idle_candidates;
				idle_candidates = NULL;

				delete idle_candidates_jobs;
				idle_candidates_jobs = NULL;

				delete serial_candidates;
				serial_candidates = NULL;
		
				delete serial_candidates_jobs;
				serial_candidates_jobs = NULL;
				continue;				// Go onto the next job.

			}
		}


			// if there are unclaimed resources, try to find
			// candidates for them

			// If we're here, we couldn't schedule the job right now.

			// If satisfyJob() failed, it might have partially
			// satisfied the job.  If so, the idle_candidates list will
			// have some things in it.  If so, we need to see if we
			// could satisfy the job with a combination of unclaimed
			// and already-claimed resources.  If that's the case,
			// we'll need to hold onto the old

			// Now, see if we could satisfy it with resources we
			// don't yet have claimed.
		if( limbo_resources ) {
			limbo_candidates = new CandidateList;
			limbo_candidates_jobs = new CAList;

			if( limbo_resources->
				satisfyJobs(jobs, limbo_candidates,
							limbo_candidates_jobs) )
			{
					// Could satisfy with idle and/or limbo
				printSatisfaction( cluster, idle_candidates, serial_candidates,
								   limbo_candidates, NULL, NULL );

					// Mark any idle resources we are going to use as
					// scheduled.
				if( idle_candidates ) {
					idle_candidates->markScheduled();
					delete idle_candidates;
					idle_candidates = NULL;

					delete idle_candidates_jobs;
					idle_candidates_jobs = NULL;
				}

					// Mark any serial resources we are going to use as
					// scheduled.
				if( serial_candidates ) {
					serial_candidates->markScheduled();
					delete serial_candidates;
					serial_candidates = NULL;

					delete serial_candidates_jobs;
					serial_candidates_jobs = NULL;
				}

				    // and the limbo resources too
				limbo_candidates->markScheduled();

				delete limbo_candidates;
				limbo_candidates = NULL;
					
				delete limbo_candidates_jobs;
				limbo_candidates_jobs = NULL;
					// OK, we will have enough with idle + limbo,
					// but we need to wait till the limbo ones
					// come back.
				continue;
			}
		}
		
		if( unclaimed_resources) {
			unclaimed_candidates = new CandidateList;
			unclaimed_candidates_jobs = new CAList;

			if( unclaimed_resources->
				satisfyJobs(jobs, unclaimed_candidates,
							unclaimed_candidates_jobs) )
			{
					// Could satisfy with idle and/or limbo and/or unclaimed

					// Cool, we could run it if we requested more
					// resources.  Go through the candidates, generate
					// resources requests for each one, remove them from
					// the unclaimed_resources list, and go onto the next
					// job.
				printSatisfaction( cluster, idle_candidates, serial_candidates,
								   limbo_candidates,
								   unclaimed_candidates, NULL );

				unclaimed_candidates->Rewind();
				unclaimed_candidates_jobs->Rewind();
				while( (ad = unclaimed_candidates->Next()) ) {
						// Make a resource request out of this job
					ClassAd *ajob = unclaimed_candidates_jobs->Next();
					generateRequest( ajob );
				}
				
				if( idle_candidates ) {
					idle_candidates->markScheduled();
					delete idle_candidates;
					idle_candidates = NULL;
					delete idle_candidates_jobs;
					idle_candidates_jobs = NULL;
				}
			
					// Mark any serial resources we are going to use as
					// scheduled.
				if( serial_candidates ) {
					serial_candidates->markScheduled();
					delete serial_candidates;
					serial_candidates = NULL;

					delete serial_candidates_jobs;
					serial_candidates_jobs = NULL;
				}


				if( limbo_candidates ) {
					limbo_candidates->markScheduled();
					delete limbo_candidates;
					limbo_candidates = NULL;
					delete limbo_candidates_jobs;
					limbo_candidates_jobs = NULL;
				}
				
				delete unclaimed_candidates;
				unclaimed_candidates = NULL;
				delete unclaimed_candidates_jobs;
				unclaimed_candidates_jobs = NULL;
				continue;
			}	
		}	

			// preempt case
			// create our preemptable list, sort it, make a ResList out of it, 
			// and try the equivalent of satisfyJobs yet again.  Note that this
			// requires us to compare with the sorted list of candidates, so we
			// can't call satisfy jobs exactly.

		ExprTree *preemption_rank = NULL;  
		ExprTree *preemption_req  = NULL;  

		char *param1 = param("SCHEDD_PREEMPTION_REQUIREMENTS");
		char *param2 = param("SCHEDD_PREEMPTION_RANK");

			// If we've got both, build an expr tree to parse them
			// If either are missing, the schedd will never preempt
			// running jobs

		if( (param1 != NULL) && (param2 != NULL)) {
			ParseClassAdRvalExpr(param1, preemption_req);
			ParseClassAdRvalExpr(param2, preemption_rank);
		}
		if( param1 ) {
			free(param1);
		}

		if( param2 ) {
			free(param2);
		}

			// If both SCHEDD_PREEMPTION_REQUIREMENTS and ..._RANK is
			// set, then try to satisfy the job by preempting running
			// resources
		if( (preemption_req != NULL) && (preemption_rank != NULL) ) {
			CAList *preempt_candidates = new CAList;
			int nodes;
			int proc;

			if (nodes_per_proc) {
				delete [] nodes_per_proc;
			}
			nodes_per_proc = new int[nprocs];
			for (int ni = 0; ni < nprocs; ni++) {
				nodes_per_proc[ni] = 0;
			}

			jobs->Rewind();
			while( (job = jobs->Next()) ) {
				job->LookupInteger(ATTR_PROC_ID, proc);
				nodes_per_proc[proc]++;
			}

			struct PreemptCandidateNode* preempt_candidate_array = NULL;
			jobs->Rewind();
			while( (job = jobs->Next()) ) {
				job->LookupInteger(ATTR_PROC_ID, proc);
				nodes = nodes_per_proc[proc];
				
					// We may not need all of this array, this is
					// worst-case allocation we will fill and sort a
					// num_candidates number of entries
				int len = busy_resources->Length();
				preempt_candidate_array = new struct PreemptCandidateNode[len];
				int num_candidates = 0;

				busy_resources->Rewind();
				while (ClassAd *machine = busy_resources->Next()) {
					classad::Value result;
					bool requirement = false;

						// See if this machine has a true
						// SCHEDD_PREEMPTION_REQUIREMENT
					requirement = EvalExprTree( preemption_req, machine, job,
												result );
					if (requirement) {
						bool val;
						if (result.IsBooleanValue(val)) {
							requirement = val;
						}
					}

						// If it does
					if (requirement) {
						double rank = 0.0;

							// Evaluate its SCHEDD_PREEMPTION_RANK in
							// the context of this job
						int rval;
						rval = EvalExprTree( preemption_rank, machine, job,
											 result );
						if( !rval || !result.IsNumber(rank) ) {
								// The result better be a number
							const char *s = ExprTreeToString( preemption_rank );
							char *m = NULL;
							machine->LookupString( ATTR_NAME, &m );
							dprintf( D_ALWAYS, "SCHEDD_PREEMPTION_RANK (%s) "
									 "did not evaluate to float on job %d "
									 "for machine %s\n", s, cluster, m );
							free(m);
							continue;
						}
						preempt_candidate_array[num_candidates].rank = rank;
						preempt_candidate_array[num_candidates].cluster_id = cluster;
						preempt_candidate_array[num_candidates].machine_ad = machine;
						num_candidates++;
					}
				}

					// Now we've got an array from 0 to num_candidates
					// of PreemptCandidateNodes sort by rank,
					// cluster_id;
				std::sort( preempt_candidate_array, preempt_candidate_array + num_candidates, RankSorter );

				int num_preemptions = 0;
				for( int cand = 0; cand < num_candidates; cand++) {
                    if (satisfies(job, preempt_candidate_array[cand].machine_ad)) {
                        // And we found a victim to preempt
						preempt_candidates->Append(preempt_candidate_array[cand].machine_ad);
						num_preemptions++;
						jobs->DeleteCurrent();
						job = jobs->Next();
						nodes--;

							// Found all the machines we needed!
						if (nodes == 0) {
							delete [] preempt_candidate_array;
							preempt_candidate_array = NULL;
							break;
						}
					}
				}

				delete [] preempt_candidate_array;
				preempt_candidate_array = NULL;

				if( jobs->Number() == 0) {
					break;
				}
			}

			if( jobs->Number() == 0) {
					// We got every single thing we need
				printSatisfaction( cluster, idle_candidates, serial_candidates,
								   limbo_candidates, unclaimed_candidates,
								   preempt_candidates );

				preempt_candidates->Rewind();
				while( ClassAd *mach = preempt_candidates->Next()) {
					busy_resources->Delete(mach);
					pending_preemptions->Append(mach);
				}

				if( idle_candidates ) {
					idle_candidates->markScheduled();
					delete idle_candidates;
					idle_candidates = NULL;
					delete idle_candidates_jobs;
					idle_candidates_jobs = NULL;
				}
			
				if( serial_candidates ) {
					serial_candidates->markScheduled();
					delete serial_candidates;
					serial_candidates = NULL;

					delete serial_candidates_jobs;
					serial_candidates_jobs = NULL;
				}


				if( limbo_candidates ) {
					limbo_candidates->markScheduled();
					delete limbo_candidates;
					limbo_candidates = NULL;
					delete limbo_candidates_jobs;
					limbo_candidates_jobs = NULL;
				}
				
				if( unclaimed_candidates ) {
					delete unclaimed_candidates;
					unclaimed_candidates = NULL;
					delete unclaimed_candidates_jobs;
					unclaimed_candidates_jobs = NULL;
				}

				delete preempt_candidates;

				if (preemption_rank) {
					delete preemption_rank;
					preemption_rank = NULL;
				}
	
				if (preemption_req) {
					delete preemption_req;
					preemption_req = NULL;
				}
	
				continue;
			}
			
			delete preempt_candidates;
		} // if (preemption_req & _rank) != NULL
			// We are done with these now
		if (preemption_rank) {
			delete preemption_rank;
			preemption_rank = NULL;
		}
	
		if (preemption_req) {
			delete preemption_req;
			preemption_req = NULL;
		}
	

			// Ok, we're in pretty bad shape.  We've got nothing
			// available now, and we've got nothing unclaimed.  Do a
			// final check to see if we could *ever* schedule this
			// job, given all the dedicated resources we know about.
			// If not, give up on it (for now), and move onto other
			// jobs.  That way, we don't starve jobs that could run,
			// just b/c someone messed up and submitted a job that's
			// too big to run.  However, if we *could* schedule this
			// job in the future, stop here, and stick to strict FIFO,
			// so that we don't starve the big job at the front of the
			// queue.  


			// clear out jobs
		jobs->Rewind();
		while( (jobs->Next()) ) {
			jobs->DeleteCurrent();
		}

		int current_proc = 0;
		while ( (job = GetJobAd( (*idle_clusters)[i], current_proc))) {
			int hosts;
			job->LookupInteger(ATTR_MAX_HOSTS, hosts);

			for( int job_num = 0 ; job_num < hosts; job_num++) {
				jobs->Append(job);
			}
			current_proc++;
		}

		
		// Support best fit scheduling by setting this parameter
		// This may cause starvation of large jobs, so use cautiously!

		bool fifo = param_boolean("DEDICATED_SCHEDULER_USE_FIFO", true);

		if( fifo && isPossibleToSatisfy(jobs, max_hosts) ) {
				// Yes, we could run it.  Stop here.
			dprintf( D_FULLDEBUG, "Could satisfy job %d in the future, "
					 "done computing schedule\n", cluster );

			if( idle_candidates ) {
				delete idle_candidates;
				idle_candidates = NULL;
				delete idle_candidates_jobs;
				idle_candidates_jobs = NULL;
			}
			
			if( limbo_candidates ) {
				delete limbo_candidates;
				limbo_candidates = NULL;
				delete limbo_candidates_jobs;
				limbo_candidates_jobs = NULL;
			}
				
			if( serial_candidates ) {
				delete serial_candidates;
				serial_candidates = NULL;
	
				delete serial_candidates_jobs;
				serial_candidates_jobs = NULL;
			}

			// and the limbo resources too
			if( unclaimed_candidates ) {
				delete unclaimed_candidates;
				unclaimed_candidates = NULL;
				delete unclaimed_candidates_jobs;
				unclaimed_candidates_jobs = NULL;
			}

			delete jobs;
			if( nodes_per_proc ) {
					delete [] nodes_per_proc;
					nodes_per_proc = NULL;
			}
			return true;
		} else {
			dprintf( D_FULLDEBUG, "Can't satisfy job %d with all possible "
					 "resources... trying next job\n", cluster );

				// By now, we've perhaps got candidates which will never
				// match, so give them back to our resources
			if( idle_candidates ) {
				idle_candidates->appendResources(idle_resources);
				delete idle_candidates;
				idle_candidates = NULL;
				delete idle_candidates_jobs;
				idle_candidates_jobs = NULL;
			}
			
			if( serial_candidates ) {
				delete serial_candidates;
				serial_candidates = NULL;
	
				delete serial_candidates_jobs;
				serial_candidates_jobs = NULL;
			}

			// and the limbo resources too
			if( limbo_candidates ) {
				limbo_candidates->appendResources(limbo_resources);
				delete limbo_candidates;
				limbo_candidates = NULL;
				delete limbo_candidates_jobs;
				limbo_candidates_jobs = NULL;
			}
				
			if( unclaimed_candidates ) {
				unclaimed_candidates->appendResources(unclaimed_resources);
				delete unclaimed_candidates;
				unclaimed_candidates = NULL;
				delete unclaimed_candidates_jobs;
				unclaimed_candidates_jobs = NULL;
			}
			continue;
		}
	}
	delete jobs;
	if( nodes_per_proc ) {
			delete [] nodes_per_proc;
	}
	return true;
}


void
DedicatedScheduler::createAllocations( CAList *idle_candidates,
									   CAList *idle_candidates_jobs,
									   int cluster, int nprocs,
									   bool is_reconnect)
{
	AllocationNode *alloc;
	MRecArray* matches=NULL;

	alloc = new AllocationNode( cluster, nprocs );
	alloc->num_resources = idle_candidates->Number();

	alloc->is_reconnect = is_reconnect;

		// Walk through idle_candidates, the list should
		// be sorted by proc.  Put each job into
		// the correct jobs and match ExtArry in our AllocationNode

	ClassAd *machine = NULL;
	ClassAd *job     = NULL;

	idle_candidates->Rewind();
	idle_candidates_jobs->Rewind();

		// Assume all procs start at 0, and are monotonically increasing
	int last_proc = -1;
	int node = 0;

		// Foreach machine we've matched
	while( (machine = idle_candidates->Next()) ) {
		match_rec *mrec;
		std::string buf;

			// Get the job for this machine
		job = idle_candidates_jobs->Next();

		    // And the proc for the job
		int proc = -1;
		job->LookupInteger(ATTR_PROC_ID, proc);
		if (proc == -1) {
			EXCEPT("illegal value for proc: %d in dedicated cluster id %d\n", proc, cluster);
		}

			// Get the match record
		if( ! (mrec = getMrec(machine, buf)) ) {
 			EXCEPT( "no match for %s in all_matches table, yet " 
 					"allocated to dedicated job %d.0!",
 					buf.c_str(), cluster ); 
		}
			// and mark it scheduled & allocated
		mrec->scheduled = true;
		mrec->allocated = true;
		mrec->cluster   = cluster;
		mrec->proc      = proc;

			// We're now at a new proc
		if( proc != last_proc) {
			last_proc = proc;
			node = 0;

				// create a new MRecArray
			matches = new MRecArray();
			ASSERT(matches != NULL);
			matches->fill(NULL);
			
				// And stick it into the AllocationNode
			(*alloc->matches)[proc] = matches;
			(*alloc->jobs)[proc] = job;
		}

			// And put the mrec into the matches for this node in the proc
		(*matches)[node] = mrec;
		node++;
	}
	
	ASSERT( allocations->insert( cluster, alloc ) == 0 );

		// Show world what we did
	alloc->display();

}


// This function is used to remove the allocation associated with the
// given shadow and perform any other clean-up related to the shadow
// exiting. 
void
DedicatedScheduler::removeAllocation( shadow_rec* srec )
{
	AllocationNode* alloc;
	MRecArray* matches;
	int i, n, m;

	if( ! srec ) {
		EXCEPT( "DedicatedScheduler::removeAllocation: srec is NULL!" );
	}

	dprintf( D_FULLDEBUG, "DedicatedScheduler::removeAllocation, "
			 "cluster %d\n", srec->job_id.cluster );

	if( allocations->lookup( srec->job_id.cluster, alloc ) < 0 ) {
		EXCEPT( "DedicatedScheduler::removeAllocation(): can't find " 
				"allocation node for cluster %d! Aborting", 
				srec->job_id.cluster ); 
	}

		// First, mark all the match records as no longer allocated to
		// our MPI job.
	for( i=0; i<alloc->num_procs; i++ ) {
		matches = (*alloc->matches)[i];
		n = matches->getlast();
		for( m=0 ; m <= n ; m++ ) {
			deallocMatchRec( (*matches)[m] );
		}
	}

		// This "allocation" is no longer valid.  So, delete the
		// allocation node from our table.
	allocations->remove( srec->job_id.cluster );

		// Finally, delete the object itself so we don't leak it. 
	delete alloc;
}


bool
DedicatedScheduler::satisfyJobWithGroups(CAList *jobs, int cluster, int nprocs) {
	dprintf(D_ALWAYS, "Trying to satisfy job with group scheduling\n");

	if (scheduling_groups.number() == 0) {
		dprintf(D_ALWAYS, "Job requested parallel scheduling groups, but no groups found\n");
		return false; 
	}

	scheduling_groups.rewind();
	char *groupName = 0;

		// Build a res list with one machine per scheduling group
		// for RANKing purposes
	ResList exampleSchedulingGroup;
	scheduling_groups.rewind();
	while ((groupName = scheduling_groups.next())) {
		ClassAd *machine;
		idle_resources->Rewind();
		while ((machine = idle_resources->Next())) {
			char *machineGroupName = 0;
			machine->LookupString(ATTR_PARALLEL_SCHEDULING_GROUP, &machineGroupName);

			bool foundOne = false;
				// if the group name in the machine name == this one, add it to the list
			if (machineGroupName && (strcmp(machineGroupName, groupName) == 0)) {
				foundOne = true;
				exampleSchedulingGroup.Append(machine);
			}
			if (machineGroupName) free(machineGroupName);
			if (foundOne) break; // just add one per group
		}
	}

	jobs->Rewind();
	ClassAd *jobAd = jobs->Next();
	jobs->Rewind();

		// Now sort the list of scheduling group example ads by this machine's rank
	if (jobAd != NULL) {
		exampleSchedulingGroup.sortByRank(jobAd);
	}
	exampleSchedulingGroup.Rewind();

	ClassAd *machineAd;
	exampleSchedulingGroup.Rewind();

		// For each of our scheduling groups...
	while ((machineAd = exampleSchedulingGroup.Next())) {
		std::string groupStr;
		machineAd->LookupString(ATTR_PARALLEL_SCHEDULING_GROUP, groupStr);

		dprintf(D_ALWAYS, "Attempting to find enough idle machines in group %s to run job.\n", groupStr.c_str());

			// From all the idle machines, select just those machines that are in this group
		ResList group; 
		idle_resources->selectGroup(&group, groupStr.c_str());

			// And try to match the jobs in the cluster to the machine just in this group
		CandidateList candidate_machines;
		CAList candidate_jobs;

		CAList allJobs; // copy jobs to allJobs, so satisfyJobs can mutate it
		jobs->Rewind();
		ClassAd *j = 0;
		while ((j = jobs->Next())) {
		    allJobs.Append(j);
		}

		if (group.satisfyJobs(&allJobs, &candidate_machines, &candidate_jobs)) {

				// Remove the allocated machines from the idle list
			candidate_machines.Rewind();
			ClassAd *cm = 0;
			while ((cm = candidate_machines.Next())) {
				idle_resources->Delete(cm);
			}
			
				// This group satisfies the request, so create the allocations
			printSatisfaction( cluster, &candidate_machines, NULL, NULL, NULL, NULL );
			createAllocations( &candidate_machines, &candidate_jobs,
							   cluster, nprocs, false );

				// We successfully allocated machines, our work here is done
			dprintf(D_FULLDEBUG, "Found matching ParallelSchedulingGroup for job\n");
			return true;
		}
	}

		// We couldn't allocate from the claimed/idle machines, try the
		// unclaimed ones as well.
	scheduling_groups.rewind();
	groupName = 0;

		// For each of our scheduling groups...
	while ((groupName = scheduling_groups.next())) {
		dprintf(D_ALWAYS, "Attempting to find enough idle or unclaimed machines in group %s to run job.\n", groupName);

		ResList idle_group; 
		ResList unclaimed_group; 
		CandidateList idle_candidate_machines;
		CAList idle_candidate_jobs;
		CandidateList unclaimed_candidate_machines;
		CAList unclaimed_candidate_jobs;

			// copy the idle machines into idle_group
		idle_resources->selectGroup(&idle_group, groupName);
		unclaimed_resources->selectGroup(&unclaimed_group, groupName); // and the unclaimed ones, too
		
			// copy jobs
		CAList allJobs; // copy jobs to allJobs, so satisfyJobs can mutate it
		jobs->Rewind();
		ClassAd *j = 0;
		while ((j = jobs->Next())) {
		    allJobs.Append(j);
		}

			// This might match some, but not all jobs and machines,
			// but fills in the candidate lists of the partial matches as a side effect
		(void) idle_group.satisfyJobs(&allJobs, &idle_candidate_machines, &idle_candidate_jobs);

		if (unclaimed_group.satisfyJobs(&allJobs, &unclaimed_candidate_machines, &unclaimed_candidate_jobs)) {
				// idle + unclaimed could satsify this request

				// Remove the allocated machines from the idle list
			idle_candidate_machines.Rewind();
			ClassAd *cm = 0;
			while ((cm = idle_candidate_machines.Next())) {
				idle_resources->Delete(cm);
			}
			
				// Remote the unclaimed machines from the unclaimed list
			unclaimed_candidate_machines.Rewind();
			cm = 0;
			while ((cm = unclaimed_candidate_machines.Next())) {
				unclaimed_resources->Delete(cm);
			}

				// Mark the unclaimed ones as scheduled
			idle_candidate_machines.markScheduled();

				// And claim the unclaimed ones
			unclaimed_candidate_machines.Rewind();
			unclaimed_candidate_jobs.Rewind();
			ClassAd *um;

				// Loop over the machines, as there might be fewer unclaimed
				// machines than idle jobs
			while( (um = unclaimed_candidate_machines.Next()) ) {
						// Make a resource request out of this job

					// Make sure it matches this PSG
				ClassAd *aJob = unclaimed_candidate_jobs.Next();
				
				ExprTree *previousPSG = NULL;
				previousPSG = aJob->LookupExpr(ATTR_MATCHED_PSG);
				
				if (!previousPSG) {
					// We've haven't already munged the Requirements, do it just this once
					ExprTree *requirements = aJob->LookupExpr(ATTR_REQUIREMENTS);
					const char *rhs = ExprTreeToString(requirements);
					std::string newRequirements = std::string("(my.Matched_PSG) && ")  + rhs;
					aJob->AssignExpr(ATTR_REQUIREMENTS, newRequirements.c_str());
				}
				std::string psgString;
				if (!aJob->LookupString(ATTR_MATCHED_PSG, psgString)) {
					std::string psgExpr;
					formatstr(psgExpr, "ParallelSchedulingGroup =?= \"%s\"", groupName);
					aJob->AssignExpr(ATTR_MATCHED_PSG, psgExpr.c_str());
				} else {
					// The old way, keep for backward compatibility of running jobs
					aJob->Assign(ATTR_MATCHED_PSG, groupName);
				}
				generateRequest(aJob);
			}
				

				// This group satisfies the request, so try to claim the unclaimed ones
			printSatisfaction( cluster, &idle_candidate_machines, NULL, NULL, &unclaimed_candidate_machines, NULL );

				// We successfully allocated machines, our work here is done
			return true;
		}
	}

		// Could not schedule this job.
	dprintf(D_FULLDEBUG, "Could not find matching ParallelSchedulingGroup for job\n");
	return false;
}

// This function is used to deactivate all the claims used by a
// given shadow.
void
DedicatedScheduler::shutdownMpiJob( shadow_rec* srec , bool kill /* = false */)
{
	AllocationNode* alloc;

	if( ! srec ) {
		EXCEPT( "DedicatedScheduler::shutdownMpiJob: srec is NULL!" );
	}

	dprintf( D_FULLDEBUG, "DedicatedScheduler::shutdownMpiJob, cluster %d\n", 
			 srec->job_id.cluster );

	if( allocations->lookup( srec->job_id.cluster, alloc ) < 0 ) {
		EXCEPT( "DedicatedScheduler::shutdownMpiJob(): can't find " 
				"allocation node for cluster %d! Aborting", 
				srec->job_id.cluster ); 
	}
	alloc->status = A_DYING;
	for (int i=0; i<alloc->num_procs; i++ ) {
        MRecArray* matches = (*alloc->matches)[i];
        int n = matches->getlast();
        std::vector<match_rec*> delmr;
        // Save match_rec pointers into a vector, because deactivation of claims 
        // alters the MRecArray object (*matches) destructively:
        for (int j = 0;  j <= n;  ++j) delmr.push_back((*matches)[j]);
        for (std::vector<match_rec*>::iterator mr(delmr.begin());  mr != delmr.end();  ++mr) {
            if (kill) {
                dprintf( D_ALWAYS, "Dedicated job abnormally ended, releasing claim\n");
                releaseClaim(*mr);
            } else {
                deactivateClaim(*mr);
            }
		}
	}
}

static void update_negotiator_attrs_for_partitionable_slots(ClassAd* match_ad)
{
	typedef std::map<std::string, std::string> negotiator_attr_cache_entry_t;
	typedef std::map<std::string, negotiator_attr_cache_entry_t> negotiator_attr_cache_t;
	static negotiator_attr_cache_t negotiator_attr_cache;

	std::string partitionable_slot_name;
	match_ad->LookupString(ATTR_NAME, partitionable_slot_name);
	if (partitionable_slot_name.length() == 0) return; 
	bool negotiator_attr_found = false;
	negotiator_attr_cache_t::iterator cit = negotiator_attr_cache.find(partitionable_slot_name);

	size_t len = strlen(ATTR_NEGOTIATOR_MATCH_EXPR);
	for ( auto itr = match_ad->begin(); itr != match_ad->end(); itr++ ) {
		if( !strncmp(itr->first.c_str(),ATTR_NEGOTIATOR_MATCH_EXPR,len) ) {
			ExprTree *expr = itr->second;
			if( !expr ) {
				continue;
			}
			if (!negotiator_attr_found) {
				// New set of attributes. Reset cache for this partitionable slot.
				if (cit != negotiator_attr_cache.end()) cit->second.clear();
				negotiator_attr_found = true;
			}
			std::string exprs(ExprTreeToString(expr));
			if (cit != negotiator_attr_cache.end()) {
				cit->second[itr->first] = exprs;
			} else {
				negotiator_attr_cache_entry_t nmap;
				nmap.insert(negotiator_attr_cache_entry_t::value_type(itr->first,exprs));
				negotiator_attr_cache.insert(negotiator_attr_cache_t::value_type(partitionable_slot_name, nmap));
			}
		}
	}
	if (!negotiator_attr_found && (cit != negotiator_attr_cache.end())) {
		// No negotiator attr found. Insert the cached ones, if any.
		const negotiator_attr_cache_entry_t &atm=cit->second;
		negotiator_attr_cache_entry_t::const_iterator mit;
		negotiator_attr_cache_entry_t::const_iterator mend = atm.end();
		for (mit = atm.begin(); mit!=mend; ++mit) {

			match_ad->AssignExpr(mit->first, mit->second.c_str());
			dprintf( D_FULLDEBUG, "%s: Negotiator match attribute %s==%s carried over from previous partitionable slot match record.\n", 
				partitionable_slot_name.c_str(),
				mit->first.c_str(), mit->second.c_str());
		}
	}
}

match_rec *
DedicatedScheduler::AddMrec(
	char const* claim_id,
	char const* startd_addr,
	char const* slot_name,
	PROC_ID job_id,
	const ClassAd* match_ad,
	char const *remote_pool
)
{
		// The dedicated scheduler expects the job id to not be set
		// until this match is associated with a job.
	PROC_ID empty_job_id;
	empty_job_id.cluster = -1;
	empty_job_id.proc = -1;

		// Now, create a match_rec for this resource
		// Note, we want to claim this startd as the
		// "DedicatedScheduler" owner, which is why we call
		// owner() here...
	match_rec *mrec = new match_rec( claim_id, startd_addr, &empty_job_id,
									 match_ad,owner(),remote_pool,true);

	match_rec *existing_mrec;
	if( all_matches->lookup(slot_name, existing_mrec) == 0) {
			// Already have this match
		dprintf(D_ALWAYS, "DedicatedScheduler: negotiator sent match for %s, but we've already got it, ignoring\n", slot_name);
		return nullptr;
	}

	// Next, insert this match_rec into our hashtables
    ClassAd* job = GetJobAd(job_id.cluster, job_id.proc);
	ASSERT( job );
    pending_requests[claim_id] = new ClassAd(*job);

	// Collapse the chained ad attributes into this copied ad,
	// just in case the job is removed while the request is still pending.
	ChainCollapse(*pending_requests[claim_id]);

    // PartitionableSlot in match_ad can never be 'true' as match_ad was
    // tweaked by ScheddNegotiate::fixupPartitionableSlot. If we want 
    // not to store just one 'dynamic' slot per host into all_matches 
    // and leave behind all extra created dynamic slots 
    // we need to use/fill pending_matches. Try checking for
    // SlotTypeID == PARTITIONABLE_SLOT, as this is left unchanged by
    // ScheddNegotiate::fixupPartitionableSlot.
    // if (is_partitionable(match_ad)) {
    int slot_type_id = 0;
    match_ad->LookupInteger(ATTR_SLOT_TYPE_ID, slot_type_id);
    if (slot_type_id == 1) { // Cannot include Resource.h from here.
        update_negotiator_attrs_for_partitionable_slots(mrec->my_match_ad);
        pending_matches[claim_id] = mrec;
        pending_claims[mrec->publicClaimId()] = claim_id;
    } else {
        ASSERT( all_matches->insert(slot_name, mrec) == 0 );
        ASSERT( all_matches_by_id->insert(mrec->claimId(), mrec) == 0 );
    }

	removeRequest( job_id );

	return mrec;
}


bool
DedicatedScheduler::DelMrec( match_rec* rec )
{
	if( ! rec ) {
		dprintf( D_ALWAYS, "Null parameter to DelMrec() -- "
				 "match not deleted\n" );
		return false;
	}
	return DelMrec( rec->claimId() );
}


bool
DedicatedScheduler::DelMrec( char const* id )
{
	match_rec* rec = NULL;

	char name_buf[256];
	name_buf[0] = '\0';

	if( ! id ) {
		dprintf( D_ALWAYS, "Null parameter to DelMrec() -- "
				 "match not deleted\n" );
		return false;
	}
	// Check pending_matches
	std::map<std::string,match_rec*>::iterator it;
	if((it = pending_matches.find(id)) != pending_matches.end()){
		rec = it->second;	
		dprintf( D_FULLDEBUG, "Found record for claim %s in pending matches\n",id);
		pending_matches.erase(it);
		std::map<std::string,ClassAd*>::iterator rit;
		if((rit = pending_requests.find(rec->publicClaimId())) != pending_requests.end()){
			if(rit->second){
				delete rit->second;
				pending_requests.erase(rit);
			}
		}
		std::map<std::string,std::string>::iterator cit;
		if((cit = pending_claims.find(rec->publicClaimId())) != pending_claims.end()){
			pending_claims.erase(cit);
		}
		delete rec;
		return true;
	}
		// First, delete it from our table hashed on ClaimId. 
	if( all_matches_by_id->lookup(id, rec) < 0 ) {
		ClaimIdParser cid(id);
		dprintf( D_FULLDEBUG, "mrec for \"%s\" not found -- " 
				 "match not deleted (but perhaps it was deleted previously)\n", cid.publicClaimId() );
		return false;
	}

	ASSERT( rec->is_dedicated );

	if (all_matches_by_id->remove(id) < 0) {
		dprintf(D_ALWAYS, "DelMrec::all_matches_by_id->remove < 0\n");	
	}
		// Now that we have the mrec again, we have to see if this
		// match record is stored in our table of allocation nodes,
		// and if so, we need to remove it from there, so we don't
		// have dangling pointers, etc.  We can look it up w/ the
		// cluster from the mrec.
	AllocationNode* alloc;
	if( allocations->lookup(rec->cluster, alloc) < 0 ) {
			// Cool, this match wasn't allocated to anyone, so we
			// don't have to worry about it.  If the match isn't
			// allocated to anyone, the cluster better be -1.
		ASSERT( rec->cluster == -1 );
	} else {
			// Bummer, this match was allocated to one of our jobs.
			// We don't have to worry about shutting it down, since
			// the shadow takes care of that.  However, we need to
			// delete this match record from the matches in the
			// allocation node, or we're going to have a dangling
			// pointer in there.

		bool found_it = false;
		for( int proc_index = 0; proc_index < alloc->num_procs; proc_index++) {
			MRecArray* rec_array = (*alloc->matches)[proc_index];
			int i, last = rec_array->getlast();

			for( i=0; i<= last; i++ ) {
					// In case you were wondering, this works just fine if
					// the mrec we care about is in the last position.
					// The first assignment will be a no-op, but no harm
					// is done, and the thing that matters is that we NULL
					// out the entry in the array and truncate it so that
					// getlast() will return the right value.
				if( (*rec_array)[i] == rec ) {
					found_it = true;
					(*rec_array)[i] = (*rec_array)[last];
					(*rec_array)[last] = NULL;
						// We want to decrement last so we break out of
						// this for loop before checking the element we
						// NULL'ed out.  Otherwise, the truncate below
						// will just get undone when we inspect the last
						// element. 
					last--;
						// Truncate our array so we realize the match is
						// gone, and don't consider it in the future.
					rec_array->truncate(last);
				}
			}
		}
		if( ! found_it ) {
				// This sucks.  We think this match record belongs to
				// a cluster that we have an allocation node for, but
				// we couldn't find the match record in the allocation
				// node.  This must be a programmer error...
			dprintf( D_ALWAYS, "ERROR deleting match record for cluster %d\n",
					 rec->cluster );  
			dprintf( D_ALWAYS, "Allocation node for this cluster doesn't "
					 "include the match rec\n" );
			EXCEPT( "Can't delete m_rec from allocation node for cluster %d",
					rec->cluster );
		}
	}

		// Now, we can delete it from the main table hashed on name.
	rec->my_match_ad->LookupString( ATTR_NAME, name_buf, sizeof(name_buf) );
	all_matches->remove(name_buf);

		// If this match record is associated with a shadow record,
		// clear out the match record from that shadow record to avoid
		// a dangling pointer.
	if( rec->shadowRec ) {
		rec->shadowRec->match = NULL;
	}

		// Finally, delete the match rec itself.
	delete rec;

	dprintf( D_FULLDEBUG, "Deleted match rec for %s\n", name_buf );

	return true;
}


// TODO: Deal w/ flocking!
void
DedicatedScheduler::publishRequestAd( void )
{
	ClassAd ad;

	dprintf( D_FULLDEBUG, "In DedicatedScheduler::publishRequestAd()\n" );

	SetMyTypeName(ad, SUBMITTER_ADTYPE);
	SetTargetTypeName(ad, STARTD_ADTYPE);

        // Publish all DaemonCore-specific attributes, which also handles
        // SCHEDD_ATTRS for us.
    daemonCore->publish(&ad);

	ad.Assign(ATTR_SCHEDD_IP_ADDR, daemonCore->publicNetworkIpAddr() );

		// Tell negotiator to send us the startd ad
		// As of 7.1.3, the negotiator no longer pays attention to this
		// attribute; it _always_ sends the resource request ad.
		// For backward compatibility with older negotiators, we still set it.
	ad.Assign( ATTR_WANT_RESOURCE_AD, true );

	ad.Assign( ATTR_SUBMITTER_TAG, HOME_POOL_SUBMITTER_TAG );

	ad.Assign( ATTR_SCHEDD_NAME, Name );

	ad.Assign( ATTR_NAME, name() );

		// Finally, we need to tell it how many "jobs" we want to
		// negotiate.  These are really how many resource requests
		// we've got. 
	ad.Assign( ATTR_IDLE_JOBS, (long long)resource_requests.size() );
	
		// TODO: Eventually, we could try to publish this info as
		// well, so condor_status -sub and friends could tell people
		// something useful about the DedicatedScheduler...
	ad.Assign( ATTR_RUNNING_JOBS, 0 );
	
	ad.Assign( ATTR_HELD_JOBS, 0 );

	ad.Assign( ATTR_FLOCKED_JOBS, 0 );

	dprintf(D_ALWAYS, "Adding submitter %s to the submitter map for default pool.\n", name());
	scheduler.SubmitterMap.AddSubmitter("", name(), time(NULL));

		// Now, we can actually send this off to the CM.
	daemonCore->sendUpdates( UPDATE_SUBMITTOR_AD, &ad, NULL, true );
}


void
DedicatedScheduler::generateRequest( ClassAd* job )
{
	PROC_ID id;

	job->LookupInteger(ATTR_CLUSTER_ID, id.cluster);
	job->LookupInteger(ATTR_PROC_ID, id.proc);

	resource_requests.push_back(id);
	return;
}

void
DedicatedScheduler::removeRequest( PROC_ID job_id )
{
		// Remove the first instance of the specified job id.

	std::list<PROC_ID>::iterator id;
	for(id = resource_requests.begin();
		id != resource_requests.end();
		id++)
	{
		if( *(id) == job_id ) {
			resource_requests.erase( id );
                        if (split_match_count > 0) split_match_count--;
			break;
		}
	}
}

   
		// We send to the negotiatior something almost like
		// but not quite, a job ad.  To make it, we copy
		// and hack up the existing job ad, turning it into
		// a generic resource request.
ClassAd *
DedicatedScheduler::makeGenericAdFromJobAd(ClassAd *job)
{
		// First, make a copy of the job ad, as is, and use that as the
		// basis for our resource request.
	ClassAd* req = new ClassAd( *job );

		// Now, insert some attributes we need
	req->Assign( ATTR_SCHEDULER, name() );

		// Patch up existing attributes with the values we want. 
	req->Assign( ATTR_MIN_HOSTS, 1 );

	req->Assign( ATTR_MAX_HOSTS, 1 );

	req->Assign( ATTR_CURRENT_HOSTS, 0 );

    ExprTree* expr = job->LookupExpr( ATTR_REQUIREMENTS );

		// ATTR_REQUIREMENTS better be there!
	ASSERT( expr );

		// We just want the right side of the assignment, which is
		// just the value (without the "Requirements = " part)
	const char *rhs = ExprTreeToString( expr );

		// We had better have a string for the expression!
	ASSERT( rhs );

		// Construct the new requirements expression by adding a
		// clause that says we need to run on a machine that's going
		// to accept us as the dedicated scheduler, and isn't already
		// running one of our jobs.

		// TODO: In the future, we don't want to require this, we just
		// want to rank it, and require that the minimum claim time is
		// >= the duration of the job...

	std::string buf;
	formatstr( buf, "(Target.DedicatedScheduler == \"%s\") && "
				 "(Target.RemoteOwner =!= \"%s\") && (%s)", 
				 name(), name(), rhs );
	req->AssignExpr( ATTR_REQUIREMENTS, buf.c_str() );

	return req;
}


void
DedicatedScheduler::clearResourceRequests( void )
{
        // If a new set of resource requests is going to be generated
        // make sure that scheduler_skipJob doesn'think that enough matches
        // were found.
        split_match_count = pending_matches.size();
	resource_requests.clear();
}


bool
DedicatedScheduler::requestResources( void )
{
	if( resource_requests.size() > 0 ) {
			// If we've got things we want to grab, publish a ClassAd
			// to ask to negotiate for them...
		displayResourceRequests();
		publishRequestAd();
		scheduler.needReschedule();
	} else {
			// We just want to publish another add to let the
			// negotiator know we're satisfied.
		publishRequestAd();
	}		
	return true;
}


bool
DedicatedScheduler::preemptResources() {
	if( pending_preemptions->Length() > 0) {
		pending_preemptions->Rewind();
		while( ClassAd *machine = pending_preemptions->Next()) {
			std::string buf;
			match_rec *mrec = getMrec(machine, buf);
			if( mrec) {
				if( deactivateClaim(mrec)) {
					char *s = NULL;
					machine->LookupString(ATTR_NAME, &s);
					dprintf( D_ALWAYS, "Preempted job on %s\n", s);
					free(s);
				}
			}
			pending_preemptions->DeleteCurrent();
		}
	}
	delete pending_preemptions;
	pending_preemptions = NULL;
	return true;
}

void
DedicatedScheduler::displayResourceRequests( void )
{
	dprintf( D_FULLDEBUG,
			 "Waiting to negotiate for %lu dedicated resource request(s)\n",
			 (unsigned long)resource_requests.size() );
}


void
DedicatedScheduler::printSatisfaction( int cluster, CAList* idle, CAList *serial, 
									   CAList* limbo, CAList* unclaimed, 
									   CAList* busy )
{
	std::string msg;
	formatstr( msg, "Satisfied job %d with ", cluster );
	bool had_one = false;
	if( idle && idle->Length() ) {
		msg += std::to_string( idle->Length() );
		msg += " idle";
		had_one = true;
	}
	if( limbo && limbo->Length() ) {
		if( had_one ) {
			msg += ", ";
		}
		msg += std::to_string( limbo->Length() );
		msg += " limbo";
		had_one = true;
	}
	if( serial && serial->Length() ) {
		if( had_one ) {
			msg += ", ";
		}
		msg += std::to_string( serial->Length() );
		msg += " serial";
		had_one = true;
	}
	if( unclaimed && unclaimed->Length() ) {
		if( had_one ) {
			msg += ", ";
		}
		msg += std::to_string( unclaimed->Length() );
		msg += " unclaimed";
		had_one = true;
	}
	if( busy && busy->Length() ) {
		if( had_one ) {
			msg += ", ";
		}
		msg += std::to_string( busy->Length() );
		msg += " busy";
		had_one = true;
	}
	msg += " resources";
	dprintf( D_FULLDEBUG, "%s\n", msg.c_str() );

	if( unclaimed && unclaimed->Length() ) {
		dprintf( D_FULLDEBUG, "Generating %d resource requests for job %d\n", 
				 unclaimed->Length(), cluster  );
	}

	if( busy && busy->Length() ) {
		dprintf( D_FULLDEBUG, "Preempting %d resources for job %d\n", 
				 busy->Length(), cluster  );
	}
}


bool
DedicatedScheduler::setScheduler( ClassAd* job_ad )
{
	int cluster;
	int proc;

	if( ! job_ad->LookupInteger(ATTR_CLUSTER_ID, cluster) ) {
		return false;
	}
	if( ! job_ad->LookupInteger(ATTR_PROC_ID, proc) ) {
		return false;
	}

	while( SetAttributeString(cluster, proc, ATTR_SCHEDULER,
							  ds_name, NONDURABLE) ==  0 ) {
		proc++;
	}
	return true;
}


// Make sure we're not holding onto any resources we're not using for
// longer than the unused_timeout.
void
DedicatedScheduler::checkSanity( void )
{
	if( ! unused_timeout ) {
			// Without a value for the timeout, there's nothing to
			// do (yet).
		return;
	}

	dprintf( D_FULLDEBUG, "Entering DedicatedScheduler::checkSanity()\n" );

		// Maximum unused time for all claims that aren't already over
		// the config-file-specified limit.
	int max_unused_time = 0;
	int tmp;
	match_rec* mrec;

	all_matches->startIterations();
    while( all_matches->iterate( mrec ) ) {
		tmp = getUnusedTime( mrec );
		if( tmp >= unused_timeout ) {
			char namebuf[1024];
			namebuf[0] = '\0';
			mrec->my_match_ad->LookupString( ATTR_NAME, namebuf, sizeof(namebuf) );
			dprintf( D_ALWAYS, "Resource %s has been unused for %d seconds, "
					 "limit is %d, releasing\n", namebuf, tmp,
					 unused_timeout );
			releaseClaim( mrec );
			continue;
		}
		if( tmp > max_unused_time ) {
			max_unused_time = tmp;
		}
	}

	if( max_unused_time ) {
			// we've got some unused claims.  we need to make sure
			// we're going to wake up in time to get rid of them, if
			// they're still unused.
		tmp = unused_timeout - max_unused_time;
		ASSERT( tmp > 0 );
		if( sanity_tid == -1 ) {
				// This must be the first time we've called
				// checkSanity(), so we actually have to register a
				// timer, instead of just resetting it.
			sanity_tid = daemonCore->Register_Timer( tmp, 0,
  				         (TimerHandlercpp)&DedicatedScheduler::checkSanity,
						 "checkSanity", this );
			if( sanity_tid == -1 ) {
				EXCEPT( "Can't register DC timer!" );
			}
		} else {
				// We've already got a timer.  Whether we got here b/c
				// the timer went off, or b/c we just called
				// checkSanity() ourselves, we can just reset the
				// timer for the new value and everything will work.
			daemonCore->Reset_Timer( sanity_tid, tmp );
		}
		dprintf( D_FULLDEBUG, "Timer (%d) will call checkSanity() "
				 "again in %d seconds\n", sanity_tid, tmp );
	} else {
			// We have no unused claims, so we don't need to call
			// checkSanity() again ourselves.  It'll get called when
			// something we care about changes (like a shadow exiting,
			// etc).  So, if we've got a timer now, just reset it to
			// go off in a Long Time so we don't have to worry about
			// managing the sanity_tid in funny ways.  If we never had
			// a timer in the first place, don't set it now.
		if( sanity_tid != -1 ) {
			daemonCore->Reset_Timer( sanity_tid, 100000000 );
		}
	}
}


int
DedicatedScheduler::getUnusedTime( match_rec* mrec )
{
	if( mrec->allocated ) {
			// We're either using this mrec now, or planning to in the
			// near future, so say it's being used.
		return 0;
	}
	switch( mrec->status ) {
	case M_UNCLAIMED:
	case M_STARTD_CONTACT_LIMBO:
    case M_ACTIVE:
		return 0;
		break;
	case M_CLAIMED:
			// This is the case we're really interested in.  We're
			// claimed, but not active (a.k.a "Claimed/Idle").  We
			// need to see how long we've been like this.
		return( (int)time(0) - mrec->entered_current_status );
		break;
	default:
		EXCEPT( "Unknown status in match rec %p (%d)", mrec, mrec->status );
	}
	return 0;
}


match_rec*
DedicatedScheduler::getMrec( ClassAd* ad, std::string& buf )
{
	match_rec* mrec;

	if( ! ad->LookupString(ATTR_NAME, buf) ) {
		dprintf( D_ALWAYS, "ERROR in DedicatedScheduler::getMrec(): "
				 "No %s in ClassAd!\n", ATTR_NAME );
		return NULL;
	}
	if( all_matches->lookup(buf, mrec) < 0 ) {
		return NULL;
	}
	return mrec;
}


bool
DedicatedScheduler::isPossibleToSatisfy( CAList* jobs, int max_hosts ) 
{
	ClassAd* candidate;
	StringList names;
	char name_buf[512];
	match_rec* mrec;
	
	dprintf( D_FULLDEBUG, 
			 "Trying to satisfy job with all possible resources\n" );

	CAList candidate_resources;
	resources->Rewind();

		// Copy resources to candidate_resources
	ClassAd *machine;
	while( (machine = resources->Next() )) {
		candidate_resources.Append(machine);
	}
	candidate_resources.Rewind();

	ClassAd *job;
	jobs->Rewind();
	int matchCount = 0;
	while( (job = jobs->Next()) ) {
		candidate_resources.Rewind();
		while( (candidate = candidate_resources.Next()) ) {
            // Make sure the job requirements are satisfied with this resource.
            if (satisfies(job, candidate)) {
				candidate_resources.DeleteCurrent();
				matchCount++;
				name_buf[0] = '\0';
				candidate->LookupString( ATTR_NAME, name_buf, sizeof(name_buf) );
				names.append( name_buf );
				jobs->DeleteCurrent();

				if( matchCount == max_hosts ) {
					// We've found all we need for this job.
					// Set the scheduled flag on any match records we used
					// for satisfying this job so we don't release them
					// prematurely. 
					names.rewind();
					char* machineName;
					while( (machineName = names.next()) ) {
						if( all_matches->lookup(machineName, mrec) >= 0 ) {
							mrec->scheduled = true;
						}
					}
					return true;
				}
				break;
			}
		}
	}
	return false;
}

void
DedicatedScheduler::holdAllDedicatedJobs( void ) 
{
	static bool should_notify_admin = true;
	int i, last_cluster, cluster;

	if( ! idle_clusters ) {
			// No dedicated jobs found, we're done.
		dprintf( D_FULLDEBUG,
				 "DedicatedScheduler::holdAllDedicatedJobs: "
				 "no jobs found\n" );
		return;
	}

	last_cluster = idle_clusters->getlast() + 1;
	if( ! last_cluster ) {
			// No dedicated jobs found, we're done.
		dprintf( D_FULLDEBUG,
				 "DedicatedScheduler::holdAllDedicatedJobs: "
				 "no jobs found\n" );
		return;
	}		

	for( i=0; i<last_cluster; i++ ) {
		cluster = (*idle_clusters)[i];
		holdJob( cluster, 0, 
		         "No condor_shadow installed that supports MPI jobs",
		         CONDOR_HOLD_CODE::NoCompatibleShadow, 0, false,
		         false, should_notify_admin );
		if( should_notify_admin ) {
				// only send email to the admin once per lifetime of
				// the schedd, so we don't swamp them w/ email...
			should_notify_admin = false;
		}
	}
}

/*
 * If we restart the schedd, and there are running jobs in the queue,
 * this method gets called once for each proc of each running job.
 * Each allocation needs to know all the procs for a job, so here,
 * we just save up all the PROC_IDs that can be reconnected, and
 * register a timer to call checkReconnectQueue when done
 */

static int reconnect_tid = -1;

bool
DedicatedScheduler::enqueueReconnectJob( PROC_ID job) {

	 
	if( ! jobsToReconnect.Append(job) ) {
		dprintf( D_ALWAYS, "Failed to enqueue job id (%d.%d)\n",
				 job.cluster, job.proc );
		return false;
	}
	dprintf( D_FULLDEBUG,
			 "Enqueued dedicated job %d.%d to spawn shadow for reconnect\n",
			 job.cluster, job.proc );

		/*
		  If we haven't already done so, register a timer to go off
		  in zero seconds to call checkContactQueue().  This will
		  start the process of claiming the startds *after* we have
		  completed negotiating and returned control to daemonCore. 
		*/
	if( reconnect_tid == -1 ) {
		reconnect_tid = daemonCore->Register_Timer( 0,
			  (TimerHandlercpp)&DedicatedScheduler::checkReconnectQueue,
			   "checkReconnectQueue", this );
	}
	if( reconnect_tid == -1 ) {
			// Error registering timer!
		EXCEPT( "Can't register daemonCore timer for DedicatedScheduler::checkReconnectQueue!" );
	}
	return true;
}

/*
 * By the time we get here, all the procs of all the cluster
 * that might need to be reconnected are in the jobsToReconnect list
 *
 */

void
DedicatedScheduler::checkReconnectQueue( void ) {
	dprintf(D_FULLDEBUG, "In DedicatedScheduler::checkReconnectQueue\n");

	PROC_ID id;

	CondorQuery query(STARTD_AD);
	ClassAdList result;
	ClassAdList ads;
	std::string constraint;

	SimpleList<PROC_ID> jobsToReconnectLater = jobsToReconnect;

	jobsToReconnect.Rewind();
	while( jobsToReconnect.Next(id) ) {
			// there's a pending registration in the queue:
		dprintf( D_FULLDEBUG, "In checkReconnectQueue(), job: %d.%d\n", 
				 id.cluster, id.proc );
	
		ClassAd *job = GetJobAd(id.cluster, id.proc);
		
		if (job == NULL) {
			dprintf(D_ALWAYS, "Job %d.%d missing from queue?\n", id.cluster, id.proc);
			continue;
		}

		char *remote_hosts = NULL;
		GetAttributeStringNew(id.cluster, id.proc, ATTR_REMOTE_HOSTS, &remote_hosts);

		StringList hosts(remote_hosts);
		free(remote_hosts);

			// Foreach host in the stringlist, build up a query to find the machine
			// ad from the collector
		hosts.rewind();
		char *host;
		while ( (host = hosts.next()) ) {
			constraint  = ATTR_NAME;
			constraint += "==\"";
			constraint += host;
			constraint += "\"";
			query.addORConstraint(constraint.c_str()); 
		}
	}

		// Now we have the big constraint with all the machines in it,
		// send it away to the Collector...
	CollectorList* collectors = daemonCore->getCollectorList();
	if (collectors->query(query, ads) != Q_OK) {
		dprintf(D_ALWAYS, "DedicatedScheduler could not query Collector\n");
	}

	CAList machines;
	CAList jobs;
	ClassAd *machine;

	ads.Open();
	while ((machine = ads.Next()) ) {
		char buf[256];
		machine->LookupString(ATTR_NAME, buf, sizeof(buf));
		dprintf(D_ALWAYS, "DedicatedScheduler found machine %s for possibly reconnection for job (%d.%d)\n", 
				buf, id.cluster, id.proc);
		machines.Append(machine);
	}

		// a "Job" in this list is really a proc.  There may be several procs
		// per job.  We need to create one allocation (potentially with 
		// multiple procs per job...
	CAList jobsToAllocate;
	CAList machinesToAllocate;

	bool firstTime = true;
	int nprocs = 0;

	PROC_ID last_id;
	last_id.cluster = last_id.proc = -1;

		// OK, we now have all the matched machines
	jobsToReconnect.Rewind();
	while( jobsToReconnect.Next(id) ) {
	
		dprintf(D_FULLDEBUG, "Trying to find machines for job (%d.%d)\n",
			id.cluster, id.proc);

			// we've rolled over to a new job with procid 0
			// create the allocation for what we've built up.
		if (! firstTime && id.proc == 0) {
			dprintf(D_ALWAYS, "DedicatedScheduler creating Allocations for reconnected job (%d.*)\n", last_id.cluster);

			// We're going to try to start this reconnect job, so remove it
			// from the reconnectLater list
			if (machinesToAllocate.Number() > 0) {
				removeFromList(&jobsToReconnectLater, &jobsToAllocate);

				createAllocations(&machinesToAllocate, &jobsToAllocate, 
							  last_id.cluster, nprocs, true);
			}
		
			nprocs = 0;
			jobsToAllocate.Rewind();
			while ( jobsToAllocate.Next() ) {
				jobsToAllocate.DeleteCurrent();
			}
			machinesToAllocate.Rewind();
			while (machinesToAllocate.Next() ) {
				machinesToAllocate.DeleteCurrent();
			}
		}

		firstTime = false;
		last_id = id;
		nprocs++;

		ClassAd *job = GetJobAd(id.cluster, id.proc);
			// Foreach node of each job
			// 1.) create mrec
			// 2.) add to all_matches, and all_matches_by_name
			// 3.) Call createAllocations to do the rest  

		char *remote_hosts = NULL;
		GetAttributeStringNew(id.cluster, id.proc, ATTR_REMOTE_HOSTS, &remote_hosts);

		std::string claims;
		GetPrivateAttributeString(id.cluster, id.proc, ATTR_CLAIM_IDS, claims);

		StringList escapedClaimList(claims.c_str(),",");
		StringList claimList;
		StringList hosts(remote_hosts);

		char *host;
		char *claim;

		escapedClaimList.rewind();
		while( (claim = escapedClaimList.next()) ) {
			std::string buf = claim;
			replace_str(buf, "$(COMMA)",",");
			claimList.append(buf.c_str());
		}

			// Foreach host in the stringlist, find matching machine by name
		hosts.rewind();
		claimList.rewind();

		while ( (host = hosts.next()) ) {

			claim = claimList.next();
			if( !claim ) {
				dprintf(D_ALWAYS,"Dedicated Scheduler:: failed to reconnect "
						"job %d.%d to %s, because claimid is missing: "
						"(hosts=%s,claims=%s).\n",
						id.cluster, id.proc,
						host, 
						remote_hosts ? remote_hosts : "(null)",
						claims.c_str());
				dPrintAd(D_ALWAYS, *job);
					// we will break out of the loop below
			}

			machines.Rewind();

			ClassAd *machineAd = NULL;
			while ( (machineAd = machines.Next())) {
					// Now lookup machine here...
				char *mach_name=NULL;
				machineAd->LookupString( ATTR_NAME, &mach_name);

				dprintf( D_FULLDEBUG, "Trying to match %s to %s\n", mach_name, host);
				if (strcmp(mach_name, host) == 0) {
					machines.DeleteCurrent();
					free(mach_name);
					break;
				}
				free(mach_name);
			}

			
			char *sinful=NULL;
			if( machineAd ) {
				Daemon startd(machineAd,DT_STARTD,NULL);
				if( !startd.addr() ) {
					dprintf( D_ALWAYS, "Can't find address of startd in ad:\n" );
					dPrintAd(D_ALWAYS, *machineAd);
						// we will break out of the loop below
				}
				else {
					sinful = strdup(startd.addr());
				}
			}

			if (machineAd == NULL) {
				dprintf( D_ALWAYS, "Dedicated Scheduler:: couldn't find machine %s to reconnect to\n", host);
					// we will break out of the loop below
			}

			if (machineAd == NULL || sinful == NULL || claim == NULL) {
					// Uh oh...
				machinesToAllocate.Rewind();
				while( machinesToAllocate.Next() ) {
					machinesToAllocate.DeleteCurrent();
				}
				jobsToAllocate.Rewind();
				while( jobsToAllocate.Next() ) {
					jobsToAllocate.DeleteCurrent();
				}
				free(sinful);
				sinful = NULL;
				continue;
			}

			dprintf(D_FULLDEBUG, "Dedicated Scheduler:: reconnect target address is %s; claim is %s\n", sinful, claim);

			match_rec *mrec = 
				new match_rec(claim, sinful, &id,
						  machineAd, owner(), NULL, true);

			mrec->setStatus(M_CLAIMED);

			ASSERT( all_matches->insert(host, mrec) == 0 );
			ASSERT( all_matches_by_id->insert(mrec->claimId(), mrec) == 0 );

			jobsToAllocate.Append(job);

			machinesToAllocate.Append(machineAd);
			free(sinful);
			sinful = NULL;
		}
		free(remote_hosts);
	}

		// Last time through, create the last bit of allocations, if there are any
	if (machinesToAllocate.Number() > 0) {
		dprintf(D_ALWAYS, "DedicatedScheduler creating Allocations for reconnected job (%d.*)\n", id.cluster);
		// We're going to try to start this reconnect job, so remove it
		// from the reconnectLater list
		removeFromList(&jobsToReconnectLater, &jobsToAllocate);

		createAllocations(&machinesToAllocate, &jobsToAllocate, 
					  id.cluster, nprocs, true);
		
	}
	spawnJobs();

	jobsToReconnect = jobsToReconnectLater;

	if (jobsToReconnect.Number() > 0) {
		reconnect_tid = daemonCore->Register_Timer( 60,
			  (TimerHandlercpp)&DedicatedScheduler::checkReconnectQueue,
			   "checkReconnectQueue", this );
		if( reconnect_tid == -1 ) {
				// Error registering timer!
			EXCEPT( "Can't register daemonCore timer for DedicatedScheduler::checkReconnectQueue!" );
		}
	}
}	

match_rec *      
DedicatedScheduler::FindMRecByJobID(PROC_ID job_id) {
	AllocationNode* alloc;
	if( allocations->lookup(job_id.cluster, alloc) < 0 ) {
		return NULL;
	}

	if (!alloc) {
		return NULL;
	}

	MRecArray* cur_matches = (*alloc->matches)[0];
	return ((*cur_matches)[0]);

	
}

match_rec*
DedicatedScheduler::FindMrecByClaimID(char const* claim_id) {
	match_rec* rec = NULL;

    // look in the traditional place first
    if (all_matches_by_id->lookup(claim_id, rec) >= 0) return rec;

    // otherwise, may be from a pending dynamic slot request, so check here:
    std::map<std::string, match_rec*>::iterator f(pending_matches.find(claim_id));
    if (f == pending_matches.end()) return NULL;
    rec = f->second;

    return rec;
}


//////////////////////////////////////////////////////////////
//  Utility functions
//////////////////////////////////////////////////////////////

// Set removal.  Remove each jobsToAllocate entry from jobsToReconnectLater
void
removeFromList(SimpleList<PROC_ID> *jobsToReconnectLater, CAList *jobsToAllocate) {
	jobsToAllocate->Rewind();
	ClassAd *job;
	while ((job = jobsToAllocate->Next())) {
		PROC_ID id;
		job->LookupInteger(ATTR_CLUSTER_ID, id.cluster);
		job->LookupInteger(ATTR_PROC_ID, id.proc);

		PROC_ID id2;
		jobsToReconnectLater->Rewind();
		while ((jobsToReconnectLater->Next(id2))) {
			if ((id.cluster == id2.cluster) &&
			    (id.proc    == id2.proc)) {
					jobsToReconnectLater->DeleteCurrent();
					break;
			}
		}
	}
}
 
// Given a pointer to a resource ClassAd, return the epoc time that
// describes when the resource will next be available.
time_t
findAvailTime( match_rec* mrec )
{
	char state[128];
	int duration, current, begin, universe, avail;
	State s;
	Activity act;
	ClassAd* resource = mrec->my_match_ad;

		// First, see if we've already allocated this node.  We want
		// to check this first, not the resource state in the classad,
		// since we might have stale classad info, but we always have
		// accurate info in the match record about allocations.
	if( mrec->allocated ) {
			// TODO: Once we're smarter about the future, we need
			// to be smarter about what to do in this case, too.
		return now + 300;
	}

		// First, switch on the status of the mrec
	switch( mrec->status ) {
	case M_UNCLAIMED:
	case M_STARTD_CONTACT_LIMBO:
			// Not yet claimed, so not yet available. 
			// TODO: Be smarter here.
		return now + 500;
	case M_CLAIMED:
			// Should be Claimed/Idle.  This is the ideal situation.
			// However, we might have stale classad info, so just say
			// we're done, instead of allowing the stale info to throw 
			// us off.
		return now;
    case M_ACTIVE:
			// Actually claimed by us, but already running a job.
			// Break out and let the state and other attributes
			// determine when we're available.
		break;
	default:
		EXCEPT( "Unknown status in match rec %p (%d)", mrec, mrec->status );
	}

	resource->LookupString( ATTR_STATE, state, sizeof(state) );
	s = string_to_state( state );
	switch( s ) {

	case unclaimed_state:
	case owner_state:
	case matched_state:
			// Shouldn't really be here, since we're checking the mrec
			// status above.  However, we might have stale classad
			// info and/or an incorrect mrec status.  So, say we'll be
			// available in 30 seconds, and let things work themselves
			// out when we have more fresh info.
		return now + 30;
		break;

	case claimed_state:
			// In the claimed_state, activity matters
		resource->LookupString( ATTR_ACTIVITY, state, sizeof(state) );
		act = string_to_activity( state );
		if( act == idle_act ) {
				// We're available now
			return now;
		} 
		resource->LookupInteger( ATTR_JOB_UNIVERSE, universe );
		if( ! resource->LookupInteger(ATTR_JOB_DURATION, duration) ) { 
				// Not defined, assume it's available now
				// TODO: Allow admin to provide a value for how much
				// it should "cost" to kill a job?
			if( universe == CONDOR_UNIVERSE_VANILLA ) {
				return now + 15;
			}
			return now + 60;
		} 

			// We found a duration, now just see when we started
			// running, compute the difference, and add that to the
			// current time
		if( ! resource->LookupInteger(ATTR_LAST_HEARD_FROM, current) ) { 
			dprintf( D_ALWAYS, "ERROR: no %s in resource ClassAd!\n", 
					 ATTR_LAST_HEARD_FROM );
			return now;
		}
		if( ! resource->LookupInteger(ATTR_ENTERED_CURRENT_ACTIVITY, 
									  begin) ) { 
			dprintf( D_ALWAYS, "ERROR: no %s in resource ClassAd!\n", 
					 ATTR_ENTERED_CURRENT_ACTIVITY );
			return now;
		}
		avail = now + (duration - (current - begin));
		if( avail < now ) {
			return now;
		} else {
			return avail;
		}
		break;

	case preempting_state:
			// TODO: Allow admins to tune this
		return now + 60;
		break;
	default:
			// Unknown state!
		dprintf( D_ALWAYS, 
				 "ERROR: unknown state (%d) for resource ClassAd!\n",
				 (int)s );
		break;
	}
	return now;
}


// Comparison function for sorting jobs (given cluster id) by QDate 
int
clusterSortByPrioAndDate( const void *ptr1, const void* ptr2 )
{
	int cluster1 = *((const int*)ptr1);
	int cluster2 = *((const int*)ptr2);
	int c1_qdate, c2_qdate;	
	int c1_prio=0, c1_preprio1=0, c1_preprio2=0, c1_postprio1=0, c1_postprio2=0;
	int c2_prio=0, c2_preprio1=0, c2_preprio2=0, c2_postprio1=0, c2_postprio2=0;

	if ((GetAttributeInt(cluster1, 0, ATTR_Q_DATE, &c1_qdate) < 0) || 
	        (GetAttributeInt(cluster2, 0, ATTR_Q_DATE, &c2_qdate) < 0) ||
	        (GetAttributeInt(cluster1, 0, ATTR_JOB_PRIO, &c1_prio) < 0) ||
	        (GetAttributeInt(cluster2, 0, ATTR_JOB_PRIO, &c2_prio) < 0)) {
		
		return -1;
	}

	GetAttributeInt(cluster1, 0, ATTR_PRE_JOB_PRIO1, &c1_preprio1);
	GetAttributeInt(cluster2, 0, ATTR_PRE_JOB_PRIO1, &c2_preprio1);
	if (c1_preprio1 < c2_preprio1) {
		return 1;
	}
	if (c1_preprio1 > c2_preprio1) {
		return -1;
	}

	GetAttributeInt(cluster1, 0, ATTR_PRE_JOB_PRIO2, &c1_preprio2);
	GetAttributeInt(cluster2, 0, ATTR_PRE_JOB_PRIO2, &c2_preprio2);
	if (c1_preprio2 < c2_preprio2) {
		return 1;
	}
	if (c1_preprio2 > c2_preprio2) {
		return -1;
	}
	
	if (c1_prio < c2_prio) {
		return 1;
	}

	if (c1_prio > c2_prio) {
		return -1;
	}

	GetAttributeInt(cluster1, 0, ATTR_POST_JOB_PRIO1, &c1_postprio1);
	GetAttributeInt(cluster2, 0, ATTR_POST_JOB_PRIO1, &c2_postprio1);
	if (c1_postprio1 < c2_postprio1) {
		return 1;
	}
	if (c1_postprio1 > c2_postprio1) {
		return -1;
	}

	GetAttributeInt(cluster1, 0, ATTR_POST_JOB_PRIO2, &c1_postprio2);
	GetAttributeInt(cluster2, 0, ATTR_POST_JOB_PRIO2, &c2_postprio2);
	if (c1_postprio2 < c2_postprio2) {
		return 1;
	}
	if (c1_postprio2 > c2_postprio2) {
		return -1;
	}
	
	return (c1_qdate - c2_qdate);
}


void
displayResource( ClassAd* ad, const char* str, int debug_level )
{
	char arch[128], opsys[128], name[128];
	ad->LookupString( ATTR_NAME, name, sizeof(name) );
	ad->LookupString( ATTR_OPSYS, opsys, sizeof(opsys) );
	ad->LookupString( ATTR_ARCH, arch, sizeof(arch) );
	dprintf( debug_level, "%s%s\t%s\t%s\n", str, opsys, arch, name );
}


void
displayRequest( ClassAd* ad, char* str, int debug_level )
{
	ExprTree* expr;
	expr = ad->LookupExpr( ATTR_REQUIREMENTS );
	dprintf( debug_level, "%s%s = %s\n", str, ATTR_REQUIREMENTS,
			 ExprTreeToString( expr ) );
}


void
deallocMatchRec( match_rec* mrec )
{
	dprintf( D_ALWAYS, "DedicatedScheduler::deallocMatchRec\n");
		// We might call this with a NULL mrec, so don't seg fault.
	if( ! mrec ) {
		return;
	}
	mrec->allocated = false;
	mrec->scheduled = false;
	mrec->cluster = -1;
	mrec->proc = -1;
	mrec->shadowRec = NULL;
	mrec->num_exceptions = 0;
		// Status is no longer active, but we're still claimed
	mrec->setStatus( M_CLAIMED );
}

// Comparision function for sorting machines
bool
RankSorter(const PreemptCandidateNode &n1, const PreemptCandidateNode &n2) {
	if (n1.rank < n2.rank) {
		return true;
	}

	if (n1.rank > n2.rank) {
		return false;
	}

	return n1.cluster_id - n2.cluster_id < 0;
}

ClassAd *
DedicatedScheduler::GetMatchRequestAd( match_rec* qmrec ) {
    if (NULL == qmrec) {
        dprintf(D_ALWAYS, "DedicatedScheduler::GetMatchRequestAd -- qmrec was NULL\n");
        return NULL;
    }

    std::map<std::string, ClassAd*>::iterator f(pending_requests.find(qmrec->claimId()));
    if (f == pending_requests.end()) {
        dprintf(D_ALWAYS, "DedicatedScheduler::GetMatchRequestAd -- failed to find job assigned to claim\n");
        return NULL;
    }

    ClassAd* job = f->second;

    pending_requests.erase(f);

    if (NULL == job) {
        dprintf(D_ALWAYS, "DedicatedScheduler::GetMatchRequestAd -- job assigned to claim was NULL\n"); 
        return NULL;
    }

    return job;
}

