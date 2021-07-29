/***************************************************************
 *
 * Copyright (C) 1990-2011, Condor Team, Computer Sciences Department,
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

#ifndef _STARTD_RESOURCE_H
#define _STARTD_RESOURCE_H

#include "ResAttributes.h"
#include "ResState.h"
#include "Starter.h"
#include "claim.h"
#include "Reqexp.h"
#include "LoadQueue.h"
#include "cod_mgr.h"
#include "IdDispenser.h"

#include <set>

class SlotType
{
public:
	typedef std::map<std::string, std::string, classad::CaseIgnLTStr> slottype_param_map_t;

	const char * type_param(const char * name);
	const char * Shares() { return shares.empty() ? NULL : shares.c_str(); }

	static const char * type_param(CpuAttributes* p_attr, const char * name);
	static const char * type_param(int type_id, const char * name);
	static bool type_param_boolean(CpuAttributes* p_attr, const char * name, bool def_value);
	static bool type_param_boolean(int type_id, const char * name, bool def_value);
	static long long type_param_long(CpuAttributes* p_attr, const char * name, long long def_value);
	static char * param(CpuAttributes* p_attr, const char * name);
	static const char * param(std::string& out, CpuAttributes* p_attr, const char * name);
	static void init_types(int max_type_id, bool first_init);

private:
	std::string shares; // share info from SLOT_TYPE_n attribute
	slottype_param_map_t params;
	static std::vector<SlotType> types;
	static bool insert_param(void*, HASHITER & it);
	void clear() { shares.clear(); params.clear(); }
};

class Resource : public Service
{
public:
	Resource( CpuAttributes*, int id, bool multiple_slots, Resource* _parent = NULL);
	~Resource();

		// override param by slot_type
	char * param(const char * name);
	const char * param(std::string& out, const char * name);
	const char * param(std::string& out, const char * name, const char * def);

		// Public methods that can be called from command handlers
	int		retire_claim( bool reversible=false );	// Gracefully finish job and release claim
	void	void_retire_claim( ) { (void)retire_claim(false); }
	void	void_retire_claim( bool reversible ) { (void)retire_claim(reversible); }
	int		release_claim( void );	// Send softkill to starter; release claim
	void	void_release_claim( void ) { (void)release_claim(); }
	int		kill_claim( void );		// Quickly kill starter and release claim
	void	void_kill_claim( void ) { (void)kill_claim(); }
	int		got_alive( void );		// You got a keep alive command
	int 	periodic_checkpoint( void );	// Do a periodic checkpoint
	void	void_periodic_checkpoint( void ) { (void)periodic_checkpoint(); }
	int 	suspend_claim(); // suspend the claim
	int 	continue_claim(); // continue the claim

		// Multi-shadow wants to run more processes.  Send a SIGHUP to
		// the starter
	int		request_new_proc( void );

		// Gracefully kill starter but keep claim	
	int		deactivate_claim( void );	

		// Quickly kill starter but keep claim
	int		deactivate_claim_forcibly( void );

		// Tell the starter to put the job on hold
	void hold_job(bool soft);

		// True if no more jobs will be accepted on the current claim.
	bool curClaimIsClosing();

		// True if this slot is draining
	bool isDraining();

		// Remove the given claim from this Resource
	void	removeClaim( Claim* );
	void	remove_pre( void );	// If r_pre is set, refuse and delete it.
	void	invalidateAllClaimIDs();

		// Shutdown methods that deal w/ opportunistic *and* COD claims
		// reversible: if true, claim may unretire
	void	shutdownAllClaims( bool graceful, bool reversible=false );
	void	releaseAllClaims( void );
	void	releaseAllClaimsReversibly( void );
	void	killAllClaims( void );

	void	dropAdInLogFile( void );

        void	setBadputCausedByDraining();
        bool	getBadputCausedByDraining() { return r_cur->getBadputCausedByDraining();}

        void	setBadputCausedByPreemption() { if( r_cur ) r_cur->setBadputCausedByPreemption();}
        bool	getBadputCausedByPreemption() { return r_cur->getBadputCausedByPreemption();}

        // Enable/Disable claims for hibernation
    void    disable ();
    void    enable ();

    // Resource state methods
	void	set_destination_state( State s ) { r_state->set_destination(s);};
	State	destination_state( void ) {return r_state->destination();};
	void	change_state( State s ) {r_state->change(s);};
	void	change_state( Activity a) {r_state->change(a);};
	void	change_state( State s , Activity a ) {r_state->change(s, a);};
	State		state( void ) const    {return r_state->state();};
	Activity	activity( void ) const {return r_state->activity();};

	// refresh ad and evaluate state change policy
	void	eval_state(void);

		// does this resource need polling frequency for compute/eval?
	bool	needsPolling( void );
	bool	hasOppClaim( void );
	bool	hasAnyClaim( void );
	bool	isDeactivating( void )	{return r_cur->isDeactivating();};
	bool	isSuspendedForCOD( void ) {return r_suspended_for_cod;};
	void	hackLoadForCOD( void );

	void	suspendForCOD( void );
	void	resumeForCOD( void );

		// Methods for computing and publishing resource attributes 

		// called when creating a d-slot
	void	initial_compute(Resource * pslot);
		// called only by initialize_resource, when a slot is created
	void	initial_compute() { 
		r_reqexp->config();
		r_attr->compute_virt_mem();
		r_attr->compute_disk();
	}
		// called only by resmgr::compute()
	void	compute_unshared();
		// called only by resmgr::compute()
	void	compute_shared() {
		r_attr->compute_virt_mem();
	}
	// called by resmgr::compute()  and by main_init()
	void	compute_evaluated() {
		// Evaluate the CpuBusy expression and compute CpuBusyTime
		// and CpuIsBusy.
		compute_cpu_busy();
	}
	void    display_total_disk() {
		dprintf(D_FULLDEBUG, "Total execute space: %llu\n", r_attr->get_total_disk());
	}

	void	publish_static(ClassAd* ad);
	void	publish_dynamic(ClassAd* ad, bool for_update);
			// publish to internal classad
	void	publish_dynamic() { publish_dynamic(r_classad, false); }

	void	refresh_sandbox_ad(ClassAd* ad);

	// publish a full ad for update or writing to disk etc
	// do NOT pass r_classad into this function!!
	typedef enum _purpose { for_update, for_req_claim, for_cod, for_workfetch, for_query, for_snap } Purpose;
	void    publish_single_slot_ad(ClassAd & ad, time_t cur_time, Purpose perp);

	void    publish_private( ClassAd *ad );
    void	publishDeathTime( ClassAd* cap );
	void	publishSlotAttrs( ClassAd* cap, bool as_literal, bool only_valid_values = true );
	void	publishDynamicChildSummaries( ClassAd *cap);

	struct ResourceLess {
		bool operator()(Resource *lhs, Resource *rhs) const {
			return strcmp(lhs->r_name, rhs->r_name) < 0;
		}
	};
	static void rollupAttrs(std::string &value, std::set<Resource *,ResourceLess> & children, const std::string &name);
	void	rollupChildAttrs(std::string & value, const std::string &name) {
		rollupAttrs(value, m_children, name);
	}

		// Load Average related methods
	float	condor_load( void ) {return r_attr->condor_load();};
	float	compute_condor_usage( void );
	float	owner_load( void ) {return r_attr->owner_load();};
	void	set_owner_load( float val ) {r_attr->set_owner_load(val);};
	void	compute_cpu_busy( void );
	time_t	cpu_busy_time( void );

	void	display_load(amask_t how_much) {
		// for updates, we want to log this on normal, all other times, we log at VERBOSE 
		r_attr->display(IS_UPDATE(how_much) ? 0 : D_VERBOSE);
	}

		// dprintf() functions add the CPU id to the header of each
		// message for SMP startds (single CPU machines get no special
		// header and it works just like regular dprintf())
	void	dprintf( int, const char*, ... ) const;
	void	dprintf_va( int, const char*, va_list ) const;

		// Helper functions to log that we're ignoring a command that
		// came in while we were in an unexpected state, or while
		// we're shutting down.
	void	log_ignore( int cmd, State s );
	void	log_ignore( int cmd, State s, Activity a );
	void	log_shutdown_ignore( int cmd );

		// Return a pointer to the Claim object whose starter has the
		// given pid.  
	Claim*	findClaimByPid( pid_t starter_pid );

		// Return a pointer to the Claim object with the given ClaimId
	Claim*	findClaimById( const char* id );

		// Return a pointer to the Claim object with the given GlobalJobId
	Claim*	findClaimByGlobalJobId( const char* id );

	bool	claimIsActive( void ); 

	Claim*	newCODClaim( int lease_duration );


		/**
		   Try to finish accepting a pending claim request.
		   @return true if we accepted and began a claim, false if not.
		*/
	bool	acceptClaimRequest();

		// Called when the starter of one of our claims exits
	void	starterExited( Claim* cur_claim );

		// Since the preempting state is so weird, and when we want to
		// leave it, we need to decide where we want to go, and we
		// have to do lots of funky twiddling with our claim objects,
		// we put all the actions and logic in one place that gets
		// called whenever we're finally ready to leave the preempting
		// state. 
	void	leave_preempting_state( void );

		// Methods to initialize and refresh the resource classads.
	void	init_classad( void );

	#if 0 // not curently used
	void	refresh_classad_static() {
		if (r_config_classad) this->publish_static(r_config_classad);
	}
	#endif

	void	refresh_classad_for_update() { // low freqency refresh, used only by ResMgr::compute_dynamic
		if (r_classad) this->publish_dynamic(r_classad, true);
	}
	void	refresh_classad_for_policy() { // high frequency refresh, used only by ResMgr::compute_dynamic
		if (r_classad) this->publish_dynamic(r_classad, false);
	}
	void	refresh_classad_resources(); // called when the resource bag of a slot has changed (p-slot or coalesced slot)
	void	refresh_classad_evaluated();
	void	refresh_classad_slot_attrs(); // refresh cross-slot attrs into r_classad
	void	refresh_draining_attrs();    // specialized refresh for changes caused by draining
	void	refresh_startd_cron_attrs(); // got startd cron updates, refresh now
	void	reconfig( void );
	void	publish_slot_config_overrides(ClassAd * cad);

	void	update( void );		// Schedule to update the central manager.
	void	do_update( void );			// Actually update the CM
	void    process_update_ad(ClassAd & ad, int snapshot=0); // change the update ad before we send it 
    int     update_with_ack( void );    // Actually update the CM and wait for an ACK
	void	final_update( void );		// Send a final update to the CM
									    // with Requirements = False.

		// Invoked by the benchmark manager at specific events
	int     benchmarks_started( void );
	int	    benchmarks_finished( void );

 		// Helper functions to evaluate resource expressions
	int     wants_hold( void );         // Default's to FALSE on undefined
	int		wants_vacate( void );		// EXCEPT's on undefined
	int		wants_suspend( void );		// EXCEPT's on undefined
	int		wants_pckpt( void );		// Defaults to FALSE on undefined
	int		eval_kill( void );			// EXCEPT's on undefined
	int		eval_preempt( void );		// EXCEPT's on undefined
	int		eval_suspend( void );		// EXCEPT's on undefined
	int		eval_continue( void );		// EXCEPT's on undefined
	int		eval_is_owner( void );		// EXCEPT's on undefined
	int		eval_start( void );			// returns -1 on undefined
	int		eval_cpu_busy( void );		// returns FALSE on undefined
	bool	willingToRun( ClassAd* request_ad );
	float	compute_rank( ClassAd* req_classad );

#if HAVE_BACKFILL
	int		eval_start_backfill( void ); 
	int		eval_evict_backfill( void ); 
	bool	start_backfill( void );
	bool	softkill_backfill( void );
	bool	hardkill_backfill( void );
#endif /* HAVE_BACKFILL */

#if HAVE_JOB_HOOKS
	bool	isCurrentlyFetching( void ) { return m_currently_fetching; }
	void	tryFetchWork( void );
	void	createOrUpdateFetchClaim( ClassAd* job_ad, float rank = 0 );
	bool	spawnFetchedWork( void );
	void	terminateFetchedWork( void );
	void	startedFetch( void );
	void	fetchCompleted( void );
		/**
		   Find the keyword to use for the given resource/slot.
		   @return Hook keyword to use, or NULL if none.
		*/
	char* getHookKeyword();
#endif /* HAVE_JOB_HOOKS */

#if HAVE_HIBERNATION
	bool	evaluateHibernate( std::string &state ) const;
#endif /* HAVE_HIBERNATION */

	int     evalMaxVacateTime();
	bool    claimWorklifeExpired();
	bool    retirementExpired();
	int     evalRetirementRemaining();
	int		mayUnretire( void );
	bool    inRetirement( void );
	int		hasPreemptingClaim( void );
	int     preemptWasTrue( void ) const; //PREEMPT was true in current claim
	void    preemptIsTrue();              //records that PREEMPT was true

		// Data members
	ResState*		r_state;	// Startd state object, contains state and activity
	ClassAd*		r_config_classad; // Static/Base Resource classad (contains everything in config file)
	ClassAd*		r_classad;  // Chained child of r_config_classad, cleaned out and rebuild frequently, publish writes into this one
	Claim*			r_cur;		// Info about the current claim
	Claim*			r_pre;		// Info about the possibly preempting claim
	Claim*			r_pre_pre;	// Info about the preempting preempting claim

    // store multiple claims (currently > 1 for consumption policies)
    struct claimset_less {
        bool operator()(Claim* a, Claim* b) const {
            return strcmp(a->id(), b->id()) < 0;
        }
    };
    typedef std::set<Claim*, claimset_less> claims_t;
    claims_t        r_claims;
    bool            r_has_cp;

	CODMgr*			r_cod_mgr;	// Object to manage COD claims
	Reqexp*			r_reqexp;   // Object for the requirements expression
	CpuAttributes*	r_attr;		// Attributes of this resource
	LoadQueue*		r_load_queue;  // Holds 1 minute avg % cpu usage
	char*			r_name;		// Name of this resource
	int				r_id;		// CPU id of this resource (int form)
	int				r_sub_id;	// Sub id of this resource (int form)
	char*			r_id_str;	// CPU id of this resource (string form)
	char*			r_pair_name; // Name of the resource paired with this one, NULL is no pair (the default), may contain "#type" during the slot building process
	int             prevLHF;
	bool 			m_bUserSuspended;
	bool			r_no_collector_updates;

	int				type( void ) { return r_attr->type(); };

	char const *executeDir() { return r_attr->executeDir(); }

		// Types: STANDARD is the slot everyone knows and loves;
		// PARTITIONABLE is a slot that never runs jobs itself,
		// instead it spawns off DYNAMIC slots as claims arrive;
		// DYNAMIC is a slot that only runs jobs, it is created by a
		// PARTITIONABLE slot
	enum ResourceFeature {
		STANDARD_SLOT,
		PARTITIONABLE_SLOT,
		DYNAMIC_SLOT
	};

	void	set_feature( ResourceFeature feature ) { m_resource_feature = feature; }
	ResourceFeature	get_feature( void ) { return m_resource_feature; }

	bool is_partitionable_slot() const { return m_resource_feature == PARTITIONABLE_SLOT; }
	bool is_dynamic_slot() const { return m_resource_feature == DYNAMIC_SLOT; }

	void set_parent( Resource* rip );
    Resource* get_parent() { return m_parent; }

	std::string makeChildClaimIds();
	void add_dynamic_child(Resource *rip) { m_children.insert(rip); }
	void remove_dynamic_child(Resource *rip) {m_children.erase(rip); }

	static bool swap_claims(Resource* ripa, Resource* ripb);

	std::list<int> *get_affinity_set() { return &m_affinity_mask;}

	bool wasAcceptedWhileDraining() const { return m_acceptedWhileDraining; }
	void setAcceptedWhileDraining() { m_acceptedWhileDraining = isDraining(); }
private:
	ResourceFeature m_resource_feature;

	Resource*	m_parent;

	// Only partitionable slots have children
	std::set<Resource *, ResourceLess> m_children;

	IdDispenser* m_id_dispenser;

	int			update_tid;	// DaemonCore timer id for update delay

	int		r_cpu_busy;
	time_t	r_cpu_busy_start_time;
	time_t	r_last_compute_condor_load;
	bool	r_suspended_for_cod;
	bool	r_hack_load_for_cod;
	int		r_cod_load_hack_tid;
	void	beginCODLoadHack( void );
	float	r_pre_cod_total_load;
	float	r_pre_cod_condor_load;
	void 	startTimerToEndCODLoadHack( void );
	void	endCODLoadHack( void );
	int		eval_expr( const char* expr_name, bool fatal, bool check_vanilla );

	std::string m_execute_dir;
	std::string m_execute_partition_id;

#if HAVE_JOB_HOOKS
	time_t	m_last_fetch_work_spawned;
	time_t	m_last_fetch_work_completed;
	bool	m_currently_fetching;
	int		m_next_fetch_work_delay;
	int		m_next_fetch_work_tid;
	int		evalNextFetchWorkDelay( void );
	void	createFetchClaim( ClassAd* job_ad, float rank = 0 );
	void	resetFetchWorkTimer( int next_fetch = 0 );
	char*	m_hook_keyword;
	bool	m_hook_keyword_initialized;
#endif /* HAVE_JOB_HOOKS */

	std::list<int> m_affinity_mask;

	bool	m_acceptedWhileDraining;
};


/* Initialize resource for this claim/job

Arguments

- rip - Input: Resource this job has been matched to.

- req_classad - Input: The ClassAd for the job to run

- leftover_claim - Output: If a partitionable slot was carved up,
  this will hold the claim to the leftovers.  Otherwise, it will be
  unchanged.

Return

Returns the Resource the job will actually be running on.  It does not need to
be deleted.  The returned Resource might be different than the Resource passed
in!  In particular, if the passed in Resource is a partitionable slot, we will
carve out a new dynamic slot for his job.

The job may be rejected, in which case the returned Resource will be null.
*/
Resource * initialize_resource(Resource * rip, ClassAd * req_classad, Claim* &leftover_claim);

#endif /* _STARTD_RESOURCE_H */
