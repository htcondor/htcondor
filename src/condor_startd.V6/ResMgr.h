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

#include "simplelist.h"
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

#ifdef LINUX
#include "VolumeManager.h"
#endif

typedef float (Resource::*ResourceFloatMember)();
typedef void (Resource::*ResourceMaskMember)(amask_t);
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
	bool	reconfig_resources( void );

private:
	void	compute_dead( amask_t );
public:
	void	compute_static();
	void	compute_dynamic(bool for_update, Resource * rip=NULL);
	void	publish_static(ClassAd* cp) { Starter::publish(cp); }
	void	publish_dynamic(ClassAd*);
	void	publishSlotAttrs( ClassAd* cap );

	void	assign_load( void );
	void	assign_keyboard( void );

	bool 	needsPolling( void );
	bool 	hasAnyClaim( void );
	bool	is_smp( void ) { return( num_cpus() > 1 ); }
	int		num_cpus( void ) const { return m_attr->num_cpus(); }
	int		num_real_cpus( void ) const { return m_attr->num_real_cpus(); }
	int		numSlots( void ) const { return nresources; }

	int		send_update( int, ClassAd*, ClassAd*, bool nonblocking );
	void	final_update( void );
	
		// Evaluate the state of all resources.
	void	eval_all( void );

		// Evaluate and send updates for all resources.
	void	eval_and_update_all( void );

		// The first one is special, since we already computed
		// everything and we don't need to recompute anything.
	void	update_all( void );	
	// These two functions walk through the array of rip pointers and
	// call the specified function on each resource.  The first takes
	// functions that take a rip as an arg.  The second takes Resource
	// member functions that take no args.  The third takes a Resource
	// member function that takes an amask_t as its only arg.
	void	walk( VoidResourceMember );
	void	walk( ResourceMaskMember, amask_t );

	// This function walks through the array of rip pointers, calls
	// the specified function on each one, sums the resulting return
	// values, and returns the total.
	float	sum( ResourceFloatMember );

	// Sort our Resource pointer array with the given comparison
	// function.  
	void resource_sort( ComparisonFunc );

	// Manipulate the supplemental Class Ad list
	int		adlist_register( StartdNamedClassAd *ad );
	StartdNamedClassAd* adlist_find( const char *name );
	int		adlist_replace( const char *name, ClassAd *ad) { return extra_ads.Replace( name, ad ); }
	int		adlist_replace( const char *name, ClassAd *ad, bool report_diff, const char *prefix);
	int		adlist_delete( const char *name ) { return extra_ads.Delete( name ); }
	int		adlist_delete( StartdCronJob * job ) { return extra_ads.DeleteJob( job ); }
	int		adlist_clear( StartdCronJob * job )  { return extra_ads.ClearJob( job ); } // delete child ads, and clear the base job ad
	int		adlist_publish( unsigned r_id, ClassAd *resAd, amask_t mask, const char * r_id_str );
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

	void		addResource( Resource* );
	bool		removeResource( Resource* );

	void		deleteResource( Resource* );

		//Make a list of the ClassAds from each slot we represent.
	void		makeAdList( ClassAdList& ads, ClassAd & queryAd );

		// count the number of resources owned by this user
	int			claims_for_this_user(const char * user);

	void		markShutdown() { is_shutting_down = true; };
	bool		isShuttingDown() const { return is_shutting_down; };

	VMUniverseMgr m_vmuniverse_mgr;

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

	time_t	now( void ) const { return cur_time; };

	StartdStats startd_stats;

    class Stats {
	public:
       stats_recent_counter_timer Compute;
       stats_recent_counter_timer WalkEvalState;
       stats_recent_counter_timer WalkUpdate;
       stats_recent_counter_timer WalkOther;
       stats_recent_counter_timer Drain;

       // TJ: for now these stats will be registered in the DC pool.
       void Init(void);
       double BeginRuntime(stats_recent_counter_timer & Probe);
       double EndRuntime(stats_recent_counter_timer & Probe, double timeBegin);
       double BeginWalk(VoidResourceMember memberfunc);
       double EndWalk(VoidResourceMember memberfunc, double timeBegin);
    } stats;

	void FillExecuteDirsList( class StringList *list );

	int nextId( void ) { return id_disp->next(); };

		// returns true if specified slot is draining
	bool isSlotDraining(Resource *rip) const;

		// return number of seconds after which we want
		// to transition to fast eviction of jobs
	int gracefulDrainingTimeRemaining(Resource *rip);

	int gracefulDrainingTimeRemaining();

		// return true if all slots are in drained state
	bool drainingIsComplete(Resource *rip);

		// return true if draining was canceled by this function
	bool considerResumingAfterDraining();

		// how_fast: DRAIN_GRACEFUL, DRAIN_QUICK, DRAIN_FAST
	bool startDraining(int how_fast,const std::string & reason, int on_completion,ExprTree *check_expr,ExprTree *start_expr,std::string &new_request_id,std::string &error_msg,int &error_code);

	bool cancelDraining(std::string request_id,std::string &error_msg,int &error_code);

	void publish_draining_attrs(Resource *rip, ClassAd *cap);

	void compute_draining_attrs();

		// badput is in seconds
	void addToDrainingBadput( int badput );

	bool typeNumCmp( const int* a, const int* b ) const;

	void calculateAffinityMask(Resource *rip);

	void checkForDrainCompletion();
	int getMaxJobRetirementTimeOverride() const { return max_job_retirement_time_override; }
	void resetMaxJobRetirementTime() { max_job_retirement_time_override = -1; }
	void setLastDrainStopTime() {
		last_drain_stop_time = time(NULL);
		stats.Drain.Add( last_drain_stop_time - last_drain_start_time );
	}

#ifdef LINUX
	VolumeManager *getVolumeManager() const {return m_volume_mgr.get();}
#endif //LINUX

private:
	static void token_request_callback(bool success, void *miscdata);

	Resource**	resources;		// Array of pointers to Resource objects
	int			nresources;		// Size of the array

	IdDispenser* id_disp;
	bool 		is_shutting_down;

	int		num_updates;
	int		up_tid;		// DaemonCore timer id for update timer
	int		poll_tid;	// DaemonCore timer id for polling timer
	int		m_cred_sweep_tid;	// DaemonCore timer id for polling timer
	time_t	startTime;		// Time that we started
	time_t	cur_time;		// current time

	StringList**	type_strings;	// Array of StringLists that
		// define the resource types specified in the config file.  
	int*		type_nums;		// Number of each type.
	int*		new_type_nums;	// New numbers of each type.
	int			max_types;		// Maximum # of types.

		// Data members we need to handle dynamic reconfig:
	SimpleList<CpuAttributes*>		alloc_list;		
	SimpleList<Resource*>			destroy_list;

	// List of Supplemental ClassAds to publish
	StartdNamedClassAdList			extra_ads;

		/* 
		  See if the destroy_list is empty.  If so, allocate whatever
		  we need to allocate.
		*/
	bool processAllocList( void );

	int last_in_use;	// Timestamp of the last time we were in use.

		/* 
		   Function to deal with checking if we're in use, or, if not,
		   if it's been too long since we've been used and if we need
		   to "pull the plug" and exit (based on the
		   STARTD_NOCLAIM_SHUTDOWN parameter).
		*/
	void check_use( void );

	void sweep_timer_handler( void ) const;

	void try_token_request();

		// State of the in-flight token request; for now, we only allow
		// one at a time.
	std::string m_token_request_id;
	std::string m_token_client_id;
	Daemon *m_token_daemon{nullptr};

#if HAVE_BACKFILL
	bool backfillConfig( void );
	bool m_backfill_shutdown_pending;
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
	void checkHibernate(void);
	int	 allHibernating( std::string &state_str ) const;
	int  startHibernateTimer();
	void resetHibernateTimer();
	void cancelHibernateTimer();
#endif /* HAVE_HIBERNATION */

	std::string drain_reason;
	bool draining;
	bool draining_is_graceful;
	unsigned char on_completion_of_draining;
	int draining_id;
	time_t last_drain_start_time;
	time_t last_drain_stop_time;
	int expected_graceful_draining_completion;
	int expected_quick_draining_completion;
	int expected_graceful_draining_badput;
	int expected_quick_draining_badput;
	int total_draining_badput;
	int total_draining_unclaimed;
	int max_job_retirement_time_override;

#ifdef LINUX
	std::unique_ptr<VolumeManager> m_volume_mgr;
#endif

	DCTokenRequester m_token_requester;
	std::unique_ptr<htcondor::DataReuseDirectory> m_reuse_dir;
};


// Comparison function for sorting resources:

// natural order is slotid / slot_sub_id
int naturalSlotOrderCmp(const void*, const void*);

// Sort on State, with Owner state resources coming first, etc.
int ownerStateCmp( const void*, const void* );

// Sort on State, with Claimed state resources coming first.  Break
// ties with the value of the Rank expression for Claimed resources.
int claimedRankCmp( const void*, const void* );

/*
  Sort resource so their in the right order to give out a new COD
  Claim.  We give out COD claims in the following order:  
  1) the Resource with the least # of existing COD claims (to ensure
     round-robin across resources
  2) in case of a tie, the Resource in the best state (owner or
     unclaimed, not claimed)
  3) in case of a tie, the Claimed resource with the lowest value of
     machine Rank for its claim
*/
int newCODClaimCmp( const void*, const void* );

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
					ClassAdUnParser::UnparseAux(rhs, val, classad::Value::NumberFactor::NO_FACTOR);
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
