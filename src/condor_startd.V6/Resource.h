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

#define USE_STARTD_LATCHES 1

class SlotType
{
public:
	typedef std::map<std::string, std::string, classad::CaseIgnLTStr> slottype_param_map_t;

	const char * type_param(const char * name);
	const char * Shares() { return shares.empty() ? NULL : shares.c_str(); }

	static const char * type_param(CpuAttributes* p_attr, const char * name);
	static const char * type_param(unsigned int type_id, const char * name);
	static bool type_param_boolean(CpuAttributes* p_attr, const char * name, bool def_value);
	static bool type_param_boolean(unsigned int type_id, const char * name, bool def_value);
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

#ifdef USE_STARTD_LATCHES
class AttrLatch
{
public:
	AttrLatch() = default;
	AttrLatch(const char * name) : attr(name) {}

	bool operator<(const AttrLatch& that) const {
		return strcasecmp(this->attr.c_str(), that.attr.c_str()) < 0;
	}

	const char * repl(std::string & buf) const {
		formatstr_cat(buf, "%s:%d%d,%lld,%lld", attr.c_str(), is_defined, publish_last_value, (long long)last_value, (long long)change_time);
		return buf.c_str();
	}

	std::string attr;
	time_t      change_time=0;
	int64_t     last_value=0;
	bool        is_defined=false;
	bool        publish_last_value=false;
};

struct AttrLatchLTStr {
	inline bool operator( )( const AttrLatch &al, const AttrLatch &bl ) const {
		return strcasecmp(al.attr.c_str(), bl.attr.c_str()) < 0;
	}
};
#endif

class Resource : public Service
{
public:
	Resource( CpuAttributes*, int id, Resource* _parent = NULL, bool take_parent_claim = false);
	~Resource();

		// override param by slot_type
	char * param(const char * name);
	const char * param(std::string& out, const char * name);
	const char * param(std::string& out, const char * name, const char * def);

		// Public methods that can be called from command handlers
	int		retire_claim( bool reversible=false );	// Gracefully finish job and release claim
	int		release_claim( void );	// Send softkill to starter; release claim
	int		kill_claim( void );		// Quickly kill starter and release claim
	int		got_alive( void );		// You got a keep alive command
	int 	periodic_checkpoint( void );	// Do a periodic checkpoint
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

	void	dropAdInLogFile( void );

	void	setBadputCausedByDraining() { if (r_cur) r_cur->setBadputCausedByDraining(); }
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

	// called by ResState when the state or activity changes in a notable way
	// this gives us a chance to refresh the slot classad and possibly trigger a collector update
	void state_or_activity_changed(time_t now, bool state_changed, bool activity_changed);

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
		// called only by resmgr::compute()
	void	compute_unshared();

	// called by resmgr::compute_and_refresh(rip) and by resmgr::compute_dynamic()
	// always called after refresh_classad_dynamic()
	void	compute_evaluated();

	void    display_total_disk() {
		dprintf(D_FULLDEBUG, "Total execute space: %llu\n", r_attr->get_total_disk());
	}

	void	publish_static(ClassAd* ad);
	void	publish_dynamic(ClassAd* ad);
			// publish to internal classad
	void	refresh_classad_dynamic() { publish_dynamic(r_classad); }

	void	refresh_sandbox_ad(ClassAd* ad);

	// publish a full ad for update or writing to disk etc
	// do NOT pass r_classad into this function!!
	typedef enum _purpose { for_update, for_req_claim, for_cod, for_workfetch, for_query, for_snap } Purpose;
	void    publish_single_slot_ad(ClassAd & ad, time_t last_heard_from, Purpose perp);

	void    publish_private( ClassAd *ad );
	void	publish_SlotAttrs( ClassAd* cap, bool as_literal, bool only_valid_values = true );
	void	publishDynamicChildSummaries( ClassAd *cap);
	void	publish_evaluated(ClassAd * cap);

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
	double	condor_load( void ) {return r_attr->condor_load();};
	double	compute_condor_usage( void );
	double	owner_load( void ) {return r_attr->owner_load();};
	void	set_owner_load( double val ) {r_attr->set_owner_load(val);};
#ifdef USE_STARTD_LATCHES  // more generic mechanism for CpuBusy
	std::vector<AttrLatch> latches;
	void init_latches();
	void reconfig_latches();
	void evaluate_latches();
	void publish_latches(ClassAd* cap, time_t now);
	void dump_latches(int dpf_level);
#else
	void	compute_cpu_busy( void );
	time_t	cpu_busy_time(time_t now);
#endif

	void	display_load() { r_attr->display_load(0); }
	void	display_load_as_D_VERBOSE() { r_attr->display_load(D_VERBOSE); }

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
	void	init_classad();

	// called when the resource bag of a slot has changed (p-slot or coalesced slot)
	void	refresh_classad_resources(const ResBag & inUse) {
		if (r_classad) {
			// Put in cpu-specific attributes (A_STATIC, A_UPDATE, A_TIMEOUT)
			publish_static_resources(r_config_classad, inUse);
			r_attr->publish_dynamic(r_classad);
		}
	}

	// publish result of compute_evaluated into the internal ad
	void	refresh_classad_evaluated() {
		if (r_classad) publish_evaluated(r_classad);
	}

	void	refresh_classad_slot_attrs(); // refresh cross-slot attrs into r_classad
	void	refresh_draining_attrs();    // specialized refresh for changes caused by draining
	void	refresh_startd_cron_attrs(); // got startd cron updates, refresh now
	void	reconfig( void );
	void	publish_slot_config_overrides(ClassAd * cad);

	void	publish_static_resources(ClassAd * cad, const ResBag & inUse) {
		if (r_backfill_slot && can_create_dslot()) {
			// deduct in-use resources from the backfill p-slot
			// if there is more than one backfill p-slot, inuse resources will be overcounted.
			// TODO: spread out deductions across multiple backfill p-slots and/or static slots?
			r_attr->publish_static(cad, &inUse);
		} else {
			r_attr->publish_static(cad, nullptr);
		}
	}


	typedef enum _whyfor {
		wf_doUpdate,       //0	 the regular periodic update
		wf_stateChange,    //1
		wf_vmChange,       //2
		wf_hiberChange,    //3
		wf_removeClaim,    //4
		wf_cod,            //5
		wf_preemptingClaim,//6
		wf_dslotCreate,    //7
		wf_dslotDelete,    //8
		wf_refreshRes,     //9
		wf_cronRequest,    //10  STARTD_CRON job requested a collector update
		wf_daemonAd,       //11  need to refresh daemon ad, but not necessarily slot ads
	} WhyFor;
	void	update_needed( WhyFor why );// Schedule to update the central manager.
	void	update_walk_for_timer() { update_needed(wf_doUpdate); } // for use with Walk where arguments are not permitted
	void	get_update_ads(ClassAd & public_ad, ClassAd & private_ad);
	unsigned int update_is_needed() { return r_update_is_for; }
	void    process_update_ad(ClassAd & ad, int snapshot=0); // change the update ad before we send it 
    int     update_with_ack( void );    // Actually update the CM and wait for an ACK, used when hibernating.
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
#ifdef USE_STARTD_LATCHES  // more generic mechanism for CpuBusy
#else
	int		eval_cpu_busy( void );		// returns FALSE on undefined
#endif
	bool	willingToRun( ClassAd* request_ad );
	double	compute_rank( ClassAd* req_classad );
	const char * analyze_match(std::string & buf, ClassAd* request_ad, bool slot_requirements, bool job_requirements);

#if HAVE_BACKFILL
	int		eval_start_backfill( void ); 
	int		eval_evict_backfill( void ); 
	bool	start_backfill( void );
	bool	softkill_backfill( void );
	bool	hardkill_backfill( void );
#endif /* HAVE_BACKFILL */

#if HAVE_JOB_HOOKS
	bool	isCurrentlyFetching( void ) { return m_currently_fetching; }
	void	tryFetchWork( int timerID = -1 );
	void	createOrUpdateFetchClaim( ClassAd* job_ad, double rank = 0 );
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

	time_t  evalMaxVacateTime();
	bool    claimWorklifeExpired();
	bool    retirementExpired();
	time_t  evalRetirementRemaining();
	int		mayUnretire( void );
	bool    inRetirement( void );
	int		hasPreemptingClaim( void );
	int     preemptWasTrue( void ) const; //PREEMPT was true in current claim
	void    preemptIsTrue();              //records that PREEMPT was true
	const ExprTree * getDrainingExpr();

	// methods that manipulate the Requirements attributes via the Reqexp struct
	void 	publish_requirements(ClassAd*);
	bool	reqexp_restore(); // Restore the original requirements
	void	reqexp_unavail(const ExprTree * start_expr = nullptr);
	void	reqexp_config();

private:
	void	reqexp_set_state(reqexp_state rst);
public:

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
	bool            r_backfill_slot;
	bool            r_suspended_by_command;	// true when the claim was suspended by a SUSPEND_CLAIM command
	bool            r_no_collector_updates; // true for HIDDEN slots
	bool            r_acceptedWhileDraining;// true when the job was started while draining

	CODMgr*			r_cod_mgr;	// Object to manage COD claims
	CpuAttributes*	r_attr;		// Attributes of this resource
	LoadQueue*		r_load_queue;  // Holds 1 minute avg % cpu usage
	char*			r_name;		// Name of this resource
	char*			r_id_str;	// CPU id of this resource (string form)
	int				r_id;		// CPU id of this resource (int form)
	int				r_sub_id;	// Sub id of this resource (int form)

	unsigned int type_id( void ) { return r_attr->type_id(); };

	char const *executeDir() { return r_attr->executeDir(); }

		// Types: STANDARD is the slot everyone knows and loves;
		// PARTITIONABLE is a slot that never runs jobs itself,
		// instead it spawns off DYNAMIC slots as claims arrive;
		// DYNAMIC is a slot that only runs jobs, it is created by a
		// PARTITIONABLE slot
	enum ResourceFeature {
		STANDARD_SLOT,
		PARTITIONABLE_SLOT,
		DYNAMIC_SLOT,
		BROKEN_SLOT,   // temporary state between failure to create and delete of slot
	};

	void	set_feature( ResourceFeature feature ) { m_resource_feature = feature; }
	ResourceFeature	get_feature( void ) { return m_resource_feature; }

	bool is_partitionable_slot() const { return m_resource_feature == PARTITIONABLE_SLOT; }
	bool is_dynamic_slot() const { return m_resource_feature == DYNAMIC_SLOT; }
	bool is_broken_slot() const { return m_resource_feature == BROKEN_SLOT; }
	bool can_create_dslot() const { return is_partitionable_slot(); } // for now, only p-slots are splitable

	void set_parent( Resource* rip );
	Resource* get_parent() { return m_parent; }
	void clear_parent(); // disconnect from parent (called on deletion of dynamic slots)

	std::string makeChildClaimIds();
	void add_dynamic_child(Resource *rip) { m_children.insert(rip); }
	void remove_dynamic_child(Resource *rip) {m_children.erase(rip); }

	std::list<int> *get_affinity_set() { return &m_affinity_mask;}

	// partially evaluate a Require<tag> expression against the request ad
	// returning the flattenAndInline'd expression as a string
	bool fix_require_tag_expr(const ExprTree * expr, ClassAd * request_ad, std::string & require);

	void set_res_conflict(const std::string & conflict) { m_res_conflict = conflict; }
	bool has_nft_conflicts(MachAttributes* ma) { return ma->has_nft_conflicts(r_id, r_sub_id); }

	bool wasAcceptedWhileDraining() const { return r_acceptedWhileDraining; }
	void setAcceptedWhileDraining() { r_acceptedWhileDraining = isDraining(); }
private:
	Reqexp          r_reqexp;   // Object for the requirments expression
	ResourceFeature m_resource_feature;
	Resource*       m_parent;

	// Only partitionable slots have children
	std::set<Resource *, ResourceLess> m_children;
	// only non-partitionable backfill slots have resource conflicts
	std::string m_res_conflict;

	IdDispenser* m_id_dispenser;

	// bulk updates use a single timer in the ResMgr for updates
	// and a timestamp of the oldest requested time for the update
	// along with a bitmask of the reason for the update
	time_t r_update_is_due = 0;		// 0 for update is not due, otherwise the oldest time it was requested
	unsigned int r_update_is_for = 0; // mask of WhyFor bits giving reason for update

#ifdef USE_STARTD_LATCHES  // more generic mechanism for CpuBusy
#else
	int		r_cpu_busy;
	time_t	r_cpu_busy_start_time; // time when cpu changed from busy to non-busy or visa versa
#endif
	time_t	r_last_compute_condor_load {0}; 
	bool	r_suspended_for_cod = false;
	bool	r_hack_load_for_cod = false;
	int		r_cod_load_hack_tid = false;
	void	beginCODLoadHack( void );
	double	r_pre_cod_total_load = 0.0;
	double	r_pre_cod_condor_load = 0.0;
	void 	startTimerToEndCODLoadHack();
	void	endCODLoadHack( int timerID = -1 );
	int		eval_expr( const char* expr_name, bool fatal, bool check_vanilla );

	std::string m_execute_dir;
	std::string m_execute_partition_id;

#if HAVE_JOB_HOOKS
	time_t	m_last_fetch_work_spawned = 0;
	time_t	m_last_fetch_work_completed = 0;
	bool	m_currently_fetching;
	int		m_next_fetch_work_delay;
	int		m_next_fetch_work_tid;
	int		evalNextFetchWorkDelay( void );
	void	createFetchClaim( ClassAd* job_ad, double rank = 0 );
	void	resetFetchWorkTimer( int next_fetch = 0 );
	char*	m_hook_keyword;
	bool	m_hook_keyword_initialized;
#endif /* HAVE_JOB_HOOKS */

	std::list<int> m_affinity_mask;
};


/* carve off resources for this claim/job

Arguments

- rip - Input: Resource this job has been matched to.

- req_classad - Input: The ClassAd for the job to run

Return

Returns the Resource the job will actually be running on.  It does not need to
be deleted.  The returned Resource might be different than the Resource passed
in!  In particular, if the passed in Resource is a partitionable slot, we will
carve out a new dynamic slot for his job.

Note: this function was formerly called initialize_resource, it can still be used that
way, but it just returned the first argument when the first arg was not a p-slot which
made the code confusing, consequently it is recommended that this function be called
only if rip->can_create_dslot() is true.

The job may be rejected, in which case the returned Resource will be null.
*/
Resource * create_dslot(Resource * rip, ClassAd * req_classad, bool take_parent_claim);

/*
Create multiple dynamic slots for a single request ad
*/
std::vector<Resource*> create_dslots(Resource* rip, ClassAd * req_classad, int num_dslots, bool take_parent_claim);

#endif /* _STARTD_RESOURCE_H */
