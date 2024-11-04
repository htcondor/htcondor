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

/*
    This file defines the ResMgr class, the "resource manager".  This
	object contains an array of pointers to Resource objects, one for
	each resource defined the config file.  

	Written on 10/9/97 by Derek Wright <wright@cs.wisc.edu>
*/
#ifndef _CONDOR_RESMGR_H
#define _CONDOR_RESMGR_H

#include "startd_named_classad_list.h"

#include "IdDispenser.h"

#include "Resource.h"
#include "claim.h"
#include "vmuniverse_mgr.h"
#include "ad_transforms.h"

#if HAVE_HIBERNATION
#  include "hibernation_manager.h"
#endif

#if HAVE_JOB_HOOKS
#  include "StartdHookMgr.h"
#endif /* HAVE_JOB_HOOKS */

#if HAVE_BACKFILL
#  include "backfill_mgr.h"
#  if HAVE_BOINC
#     include "boinc_mgr.h"
#  endif /* HAVE_BOINC */
#endif /* HAVE_BACKFILL */

#include "generic_stats.h"

#ifndef NUM_ELEMENTS
#define NUM_ELEMENTS(_ary)   (sizeof(_ary) / sizeof((_ary)[0]))
#endif

#include "VolumeManager.h"

typedef double (Resource::*ResourceFloatMember)();
typedef void (Resource::*VoidResourceMember)();
typedef int (*ComparisonFunc)(const void *, const void *);

namespace htcondor {
class DataReuseDirectory;
}

// Statistics to publish global to the startd
class StartdStats {
public:
	StartdStats() : init_time(0), lifetime(0), last_update_time(0), recent_lifetime(0), recent_tick_time(0) { Init();}

	stats_entry_recent<int>	total_preemptions;
	stats_entry_recent<int>	total_rank_preemptions;
	stats_entry_recent<int>	total_user_prio_preemptions;
	stats_entry_recent<int>	total_job_starts;
	stats_entry_recent<int>	total_claim_requests;
	stats_entry_recent<int>	total_activation_requests;
	stats_entry_recent<int> total_new_dslot_unwilling;
	stats_entry_recent<Probe> job_busy_time;
	stats_entry_recent<Probe> job_duration;

	time_t init_time;
	time_t lifetime;
	time_t last_update_time;
	time_t recent_lifetime;
	time_t recent_tick_time;

	StatisticsPool pool;

	void Init() {
		const int recent_window = 60 * 20;
		const int window_quantum = 60 * 4;

		pool.AddProbe("JobPreemptions", &total_preemptions);
		pool.AddProbe("JobRankPreemptions", &total_rank_preemptions);
		pool.AddProbe("JobUserPrioPreemptions", &total_user_prio_preemptions);
		pool.AddProbe("JobStarts", &total_job_starts);
		pool.AddProbe("ClaimRequests", &total_claim_requests);
		pool.AddProbe("ActivationRequests", &total_activation_requests);
		pool.AddProbe("NewDSlotNotMatch", &total_new_dslot_unwilling);

		// publish two Miron probes, showing only XXXCount if count is zero, and
		// also XXXMin, XXXMax and XXXAvg if count is non-zero
		const int flags = stats_entry_recent<Probe>::PubValueAndRecent | ProbeDetailMode_CAMM | IF_NONZERO | IF_VERBOSEPUB;
		pool.AddProbe("JobBusyTime", &job_busy_time, NULL, flags);
		pool.AddProbe("JobDuration", &job_duration, NULL, flags);

		// for probes to be published if they are in the whitelist
		std::string strWhitelist;
		if (param(strWhitelist, "STATISTICS_TO_PUBLISH_LIST")) {
			pool.SetVerbosities(strWhitelist.c_str(), 0, true);
		}


		pool.SetRecentMax(recent_window, window_quantum);
	}

	void Publish(ClassAd &ad, int flags) const {
		pool.Publish(ad, flags);
	}

	void Tick(time_t now) {
		const int my_window =  60 * 20;
		const int my_quantum =  60 * 4;

		if (!now) now = time(NULL);

		int advance = generic_stats_Tick(
			now,
			my_window,
			my_quantum,
			this->init_time,
			this->last_update_time,
			this->recent_tick_time,
			this->lifetime,
			this->recent_lifetime);
		if (advance) {
			pool.Advance(advance);
		}
	}
};

class ResMgr : public Service
{
public:
	ResMgr();
	~ResMgr();

	void	init_socks( void );
	void	init_resources( void );
	void	reconfig_resources( void );

	void	compute_static();
	// recompute and refresh dynamic attrs for all slots before update or policy evaluation
	void	compute_dynamic(bool for_update);
	// recompute and re-publish dynamic attrs on d-slot create or on any slot Activation
	void	compute_and_refresh(Resource * rip);
	void	publish_static(ClassAd* cp) { Starter::publish(cp); }
	// publish statistics, hibernation, STARTD_CRON and TTL
	void	publish_resmgr_dynamic(ClassAd* ad, bool daemon_ad=false);
	void	publishSlotAttrs( ClassAd* cap );

	void	assign_load_to_slots();
	void    assign_idle_to_slots();
	void    got_cmd_xevent(); // called when a command_x_event arrives from a local socket

	bool 	needsPolling( void );
	bool 	hasAnyClaim( void );
	bool	is_smp( void ) { return( num_cpus() > 1 ); }
	int		num_cpus( void ) const { return m_attr->num_cpus(); }
	int		num_real_cpus( void ) const { return m_attr->num_real_cpus(); }
	//Resource * Slot(int ix) const { return slots[ix]; }
	int		numSlots( void ) const { return (int)slots.size(); }

	int		send_update( int, ClassAd*, ClassAd*, bool nonblocking );
	void	final_update( void );
	
		// Evaluate the state of all resources.
	void	eval_all( int timerID = -1 );

		// Evaluate and send updates for all resources.
	void	eval_and_update_all( int timerID = -1 );

		// The first one is special, since we already computed
		// everything and we don't need to recompute anything.
	void	update_all( int timerID = -1 );

		// Evaluate and send updates for dirty resources, and clear update dirty bits
	void	send_updates_and_clear_dirty( int timerID = -1 );

	void vacate_all(bool fast) {
		if (fast) { walk( [](Resource* rip) { rip->kill_claim(); } ); }
		else { walk( [](Resource* rip) { rip->retire_claim(false); } ); }
	}

	void checkpoint_all() { walk( [](Resource* rip) { rip->periodic_checkpoint(); } ); }

	// called from the ~Resource destructor when the deleted slot has a parent
	// this gives a chance to trigger updates of backfill p-slots
	void res_conflict_change(Resource* pslot, bool dslot_deleted) {
		// when we delete a non-backfill d-slot the backfill pslot has more
		// resources to report so we want to trigger and update
		// TODO: find a better way to handle this?
		if ( ! pslot->r_backfill_slot && dslot_deleted) {
			walk ( [](Resource * rip) {
						if (rip->r_backfill_slot && rip->can_create_dslot()) {
							rip->update_needed(Resource::WhyFor::wf_refreshRes);
						}
					} );
		}
	}

	// called from StartdCronJob::Publish after one or more adlist ads with an ad_name prefix are updated
	// this gives a chance to refresh the startd cron ads right away
	// TODO: TJ, figure out is this necessary?
	void adlist_updated(const char * /*ad_name*/, bool update_collector) {
		// TODO: be more selective about what we refresh here?
		walk(&Resource::refresh_startd_cron_attrs);
		if (update_collector) rip_update_needed(1<<Resource::WhyFor::wf_cronRequest);
	}

private:
	int in_walk = 0;

	// This function walks through the array of rip pointers and
	// calls the specified Resource:: member function on each resource.
	// It can call Resource::member functions that take no args
	void	walk( VoidResourceMember );

	// This function walks through the array of rip pointers, calls
	// the specified function on each one, sums the resulting return
	// values, and returns the total.
	double	sum( ResourceFloatMember );

	// helper for assign_load_to_slots
	double distribute_load(Resource * rip, double load);

public:
	// Manipulate the supplemental Class Ad list
	int		adlist_register( StartdNamedClassAd *ad );
	StartdNamedClassAd* adlist_find( const char *name );
	int		adlist_replace( const char *name, ClassAd *ad) { return extra_ads.Replace( name, ad ); }
	int		adlist_delete( const char *name ) { return extra_ads.Delete( name ); }
	int		adlist_delete( StartdCronJob * job ) { return extra_ads.DeleteJob( job ); }
	int		adlist_clear( StartdCronJob * job )  { return extra_ads.ClearJob( job ); } // delete child ads, and clear the base job ad
	int		adlist_publish( unsigned r_id, ClassAd *resAd, const char * r_id_str) {
		return extra_ads.Publish(resAd, r_id, r_id_str);
	}
	void	adlist_reset_monitors( unsigned r_id, ClassAd * forWhom );
	void	adlist_unset_monitors( unsigned r_id, ClassAd * forWhom );

	// Methods to control various timers
	void	check_polling( void );	// See if we need to poll frequently
	int		start_sweep_timer(void); // Timer for sweeping SEC_CREDENTIAL_DIRECTORY
	int		start_update_timer(void); // Timer for updating the CM(s)
	int		start_poll_timer( void ); // Timer for polling the resources
	void	cancel_poll_timer( void );
	void	reset_timers( void );	// Reset the period on our timers,
									// in case the config has changed.

#if defined(WIN32)
	void reset_credd_test_throttle() { m_attr->reset_credd_test_throttle(); }
#endif

	Claim*		getClaimByPid( pid_t );	// Find Claim by pid of starter
	Claim*		getClaimById( const char* id );	// Find Claim by ClaimId
	Claim*		getClaimByGlobalJobId( const char* id );
	Claim*		getClaimByGlobalJobIdAndId( const char *claimId,
											const char *job_id);

	Resource*	findRipForNewCOD( ClassAd* ad );
	Resource*	get_by_cur_id(const char* id);	// Find rip by ClaimId of r_cur
	Resource*	get_by_any_id(const char* id, bool move_cp_claim = false);	// Find rip by any claim id
	Resource*	get_by_name(const char*);		// Find rip by r_name
	Resource*	get_by_name_prefix(const char*); // Find rip by slot prefix part of r_name (everything before the @)
	Resource*	get_by_slot_id(int);	// Find rip by r_id
	State		state( void );			// Return the machine state


	void report_updates( void ) const;	// Log updates w/ dprintf()

	MachAttributes*	m_attr;		// Machine-wide attribute object
	
	ClassAd*	totals_classad;
	ClassAd*	config_classad;
	ClassAd*	extras_classad;

	void		init_config_classad( void );
	void		updateExtrasClassAd( ClassAd * cap );
	void		publish_daemon_ad(ClassAd & ad);
	void		final_update_daemon_ad();

	void		addResource( Resource* );
	bool		removeResource( Resource* );

	void		deleteResource( Resource* );

		//Make a list of the ClassAds from each slot we represent.
	void		makeAdList( ClassAdList& ads, ClassAd & queryAd );

		// count the number of resources owned by this user
	int			claims_for_this_user(const std::string &user);

	void		markShutdown() { is_shutting_down = true; };
	bool		isShuttingDown() const { return is_shutting_down; };

	void directAttachToSchedd();
	time_t m_lastDirectAttachToSchedd;

	VMUniverseMgr m_vmuniverse_mgr;
	bool AllocVM(pid_t starter_pid, ClassAd & vm_classad, Resource* rip);

	AdTransforms m_execution_xfm;

#if HAVE_BACKFILL
	BackfillMgr* m_backfill_mgr;
	void backfillMgrDone();
#endif /* HAVE_BACKFILL */

#if HAVE_JOB_HOOKS
	StartdHookMgr* m_hook_mgr;
	void startdHookMgrDone();
#endif /* HAVE_JOB_HOOKS */

#if HAVE_HIBERNATION
	HibernationManager const& getHibernationManager () const;
	void updateHibernateConfiguration ();
    int disableResources ( const std::string &state );
	bool hibernating () const;
#endif /* HAVE_HIBERNATION */

	time_t	now(void) const { return cur_time; };
	time_t	time_to_live(void) const { return deathTime ? (cur_time - deathTime) : INT_MAX; }
	time_t	update_cur_time(bool update_ads=false) {
		time_t now = time(nullptr);
		if (now != cur_time) {
			cur_time = now;
			if (update_ads) refresh_cur_time(now);
		}
		return now;
	}
	void refresh_cur_time(time_t now) {
		walk([now](Resource * rip) { if (rip) rip->r_classad->Assign(ATTR_MY_CURRENT_TIME, now); } );
	}

	// called by Resource::update_needed the first time a resource is marked dirty after the last update
	// this function queues a global update timer and returns the time value needed for r_update_is_due.
	time_t rip_update_needed(unsigned int whyfor_bits);

	StartdStats startd_stats;

    class Stats {
	public:
       stats_recent_counter_timer Compute;
       stats_recent_counter_timer WalkEvalState;
       stats_recent_counter_timer WalkUpdate;
       stats_recent_counter_timer WalkOther;
       stats_recent_counter_timer Drain;
       stats_recent_counter_timer SendUpdates;

       // TJ: for now these stats will be registered in the DC pool.
       void Init(void);
       double BeginRuntime(stats_recent_counter_timer & Probe);
       double EndRuntime(stats_recent_counter_timer & Probe, double timeBegin);
       double BeginWalk(VoidResourceMember memberfunc);
       double EndWalk(VoidResourceMember memberfunc, double timeBegin);
    } stats;

	void FillExecuteDirsList( std::vector<std::string>& list );

	int nextId( void ) { return id_disp->next(); };

		// returns true if specified slot is draining
	bool isSlotDraining(Resource *rip) const;

		// return number of seconds after which we want
		// to transition to fast eviction of jobs
	time_t gracefulDrainingTimeRemaining(Resource *rip);

	time_t gracefulDrainingTimeRemaining();

		// return true if all slots are in drained state
	bool drainingIsComplete(Resource *rip);

		// return true if draining was canceled by this function
	bool considerResumingAfterDraining();

		// how_fast: DRAIN_GRACEFUL, DRAIN_QUICK, DRAIN_FAST
	bool startDraining(int how_fast,time_t deadline,const std::string & reason, int on_completion,ExprTree *check_expr,ExprTree *start_expr,std::string &new_request_id,std::string &error_msg,int &error_code);

	bool cancelDraining(std::string request_id,bool reconfig,std::string &error_msg,int &error_code);

	void publish_draining_attrs(Resource *rip, ClassAd *cap);

	void compute_draining_attrs();
	const ExprTree * get_draining_expr() { return draining_start_expr; }

		// badput is in seconds
	void addToDrainingBadput(time_t badput);

	bool typeNumCmp( const int* a, const int* b ) const;

	void calculateAffinityMask(Resource *rip);

	void checkForDrainCompletion();
	time_t getMaxJobRetirementTimeOverride() const { return max_job_retirement_time_override; }
	void resetMaxJobRetirementTime() { max_job_retirement_time_override = -1; }
	void setLastDrainStopTime() {
		last_drain_stop_time = time(NULL);
		stats.Drain.Add( last_drain_stop_time - last_drain_start_time );
	}

	bool compute_resource_conflicts();
	void printSlotAds(const char * slot_types) const;
	void refresh_classad_resources(Resource * rip) const {
		rip->refresh_classad_resources(primary_res_in_use);
	}
	void publish_static_slot_resources(Resource * rip, classad::ClassAd * cad) const {
		rip->publish_static_resources(cad, primary_res_in_use);
	}

	template <typename Func>
	void walk(Func fn) {
		if (++in_walk > 1) { dprintf(D_ZKM | D_BACKTRACE, "recursive <>ResMgr::walk\n"); }
		for (Resource* rip : slots) { if (rip) fn(rip); }
		if (--in_walk <= 0) { if ( ! _pending_removes.empty()) _complete_removes(); }
	}

	void releaseAllClaims()           { walk( [](Resource* rip) { rip->shutdownAllClaims(true); } ); }
	void releaseAllClaimsReversibly() { walk( [](Resource* rip) { rip->shutdownAllClaims(true, true); } ); }
	void killAllClaims()              { walk( [](Resource* rip) { rip->shutdownAllClaims(false); } ); }
	void initResourceAds()            { walk( [](Resource* rip) { rip->init_classad(); } ); }

	VolumeManager *getVolumeManager() const {return m_volume_mgr.get();}

private:
	static void token_request_callback(bool success, void *miscdata);

	// The main slot collection, this should normally be sorted by slot id / slot sub-id
	// but may not be for brief periods during slot creation
	std::vector<Resource*> slots;

	// The resources-in-use collections. this is recalculated each time
	// compute_resource_conflicts is called and is used by both the daemon-ad and by the
	// backfill slot advertising code
	ResBag primary_res_in_use;
	ResBag backfill_res_in_use;
	ResBag excess_backfill_res; // difference between size of backfill and primary bag when both are fully idle

	IdDispenser* id_disp;
	bool 		is_shutting_down;

	int		num_updates;
	int		up_tid;		// DaemonCore timer id for update timer (periodic repeating timer to trigger updates)
	int		poll_tid;	// DaemonCore timer id for polling timer
	int		m_cred_sweep_tid;	// DaemonCore timer id for polling timer
	int		send_updates_tid;   // DaemonCore timer for actually sending the updates (one-shot, short period timer)
	unsigned int send_updates_whyfor_mask;
	time_t	startTime;		// Time that we started
	time_t	cur_time;		// current time
	time_t	deathTime = 0;		// If non-zero, time we will SIGTERM

	std::vector<std::string> type_strings;	// Array of strings that
		// define the resource types specified in the config file.  
	std::vector<bool> bad_slot_types; // Array of slot types which had init time slot creation failures
	int*		type_nums;		// Number of each type.
	int*		new_type_nums;	// New numbers of each type.
	int			max_types;		// Maximum # of types.

	// private helper for deleting slot Resource* objects after a walk
	std::vector<Resource*>	_pending_removes;
	void _remove_and_delete_slot_res(Resource*);
	void _complete_removes();
	// called in init_resources after the configured slots have been created 
	void _post_init_resources();

	// search helper templates
	template <typename Ret, typename Filter>
	Ret get_by(Filter filter) const {
		for (Resource* rip : slots) {
			if ( ! rip) continue;
			Ret r = filter(rip);
			if (r) return r;
		}
		return NULL;
	}
	template <typename Ret, typename Member>
	Ret call_until(Member member) const {
		for (Resource* rip : slots) {
			if ( ! rip) continue;
			Ret r = (rip->*member)();
			if (r) return r;
		}
		return (Ret)0;
	}
	template <typename Ret, typename Member, typename Arg>
	Ret call_until(Member member, Arg arg) const {
		for (Resource* rip : slots) {
			if ( ! rip) continue;
			Ret r = (rip->*member)(arg);
			if (rip && r) return r;
		}
		return (Ret)0;
	}

	// List of Supplemental ClassAds to publish
	StartdNamedClassAdList			extra_ads;


	time_t last_in_use;	// Timestamp of the last time we were in use.

		/* 
		   Function to deal with checking if we're in use, or, if not,
		   if it's been too long since we've been used and if we need
		   to "pull the plug" and exit (based on the
		   STARTD_NOCLAIM_SHUTDOWN parameter).
		*/
	void check_use( void );

	void sweep_timer_handler( int timerID = -1 ) const;

	void try_token_request();

		// State of the in-flight token request; for now, we only allow
		// one at a time.
	std::string m_token_request_id;
	std::string m_token_client_id;
	Daemon *m_token_daemon{nullptr};
	DCTokenRequester m_token_requester;

#if HAVE_BACKFILL
	bool backfillMgrConfig( void );
	bool m_backfill_mgr_shutdown_pending;
#endif /* HAVE_BACKFILL */

#if HAVE_JOB_HOOKS
	bool fetchWorkConfig( void );
	bool m_startd_hook_shutdown_pending;
#endif /* HAVE_JOB_HOOKS */

#if HAVE_HIBERNATION
	NetworkAdapterBase  *m_netif;
	HibernationManager	*m_hibernation_manager;
	int					m_hibernate_tid;
	bool				m_hibernating;
	void checkHibernate( int timerID = -1 );
	int	 allHibernating( std::string &state_str ) const;
	int  startHibernateTimer();
	void resetHibernateTimer();
	void cancelHibernateTimer();
#endif /* HAVE_HIBERNATION */

	std::string drain_reason;
	ExprTree * draining_start_expr{nullptr}; // formerly globalDrainingStartExpr
	bool draining;
	bool draining_is_graceful;
	unsigned char on_completion_of_draining;
	int draining_id;
	time_t drain_deadline;
	time_t last_drain_start_time;
	time_t last_drain_stop_time;
	time_t expected_graceful_draining_completion;
	time_t expected_quick_draining_completion;
	time_t expected_graceful_draining_badput;
	time_t expected_quick_draining_badput;
	time_t total_draining_badput;
	time_t total_draining_unclaimed;
	time_t max_job_retirement_time_override;

	std::unique_ptr<VolumeManager> m_volume_mgr;
};


bool OtherSlotEval(const char * /*name*/,
	const classad::ArgumentList &arg_list,
	classad::EvalState &state,
	classad::Value &result);
bool ExprTreeIsSlotEval(classad::ExprTree * expr);
int ExprHasSlotEval(classad::ExprTree * expr);

namespace classad {

	/// This converts a ClassAd into a string representing the %ClassAd
	/// except for SlotEval function calls, those are evaluated and then unparsed
	class SlotEvalUnParser : public ClassAdUnParser
	{
	public:
		/// Constructor
		SlotEvalUnParser(const ClassAd * scope_ad) : ClassAdUnParser(), evs(), indirect(true) { evs.SetScopes(scope_ad); }

		/// Destructor
		virtual ~SlotEvalUnParser() {}

		/// Function unparser
		///   This uses the normal classad unparser unless it is a SlotEval function
		///   For a SlotEval function, the expression is evaluated against that slot
		///   and the result is unparsed and inserted into the output buffer
		virtual void UnparseAux(
			std::string &buffer,
			std::string &fnName,
			std::vector<ExprTree*>& args)
		{
			if (MATCH == strcasecmp(fnName.c_str(), "SlotEval")) {
				classad::Value val;
				std::string attr, rhs;
				if (!OtherSlotEval("SlotEval", args, evs, val)) {
					attr = "error";
				} else {
					ClassAdUnParser::UnparseAux(rhs, val);
					if (indirect) {
						classad::Value attrval;
						if(!OtherSlotEval("*", args, evs, attrval)||!attrval.IsStringValue(attr)) {
							attr = "error";
							rhs.clear();
						} else {
							if (ends_with(attr, "_expr_")) {
								formatstr_cat(attr, "%03d", (int)slot_attrs.size());
							}
							slot_attrs[attr] = rhs;
						}
					} else {
						attr = rhs;
					}
				}
				buffer += attr;
			} else {
				ClassAdUnParser::UnparseAux(buffer, fnName, args);
			}
		}

		// clear the indirect intermediate values
		void ClearSlotAttrs() { slot_attrs.clear(); }

		// assign the indirect intermediate values
		// into the given ad
		void AssignSlotAttrs(ClassAd & ad) {
			for (auto it = slot_attrs.begin(); it != slot_attrs.end(); ++it) {
				ad.AssignExpr(it->first, it->second.c_str());
			}
		}

		// toggle the direct vs. indirect replacement strategy
		bool setIndirectThroughAttr(bool ind) {
			bool tmp = indirect;
			indirect = ind;
			return tmp;
		}

	protected:
		EvalState evs;
		// when doing indirect replacement, intermediate attributes are added to this collection
		std::map<std::string, std::string, CaseIgnLTStr> slot_attrs;
		// when true, use indirect replacement of SlotEval function calls
		// when false, do direct replacement
		bool indirect;
	};
}



#endif /* _CONDOR_RESMGR_H */
