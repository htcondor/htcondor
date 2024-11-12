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
#include <math.h>
#include <float.h>
#include <set>
#include "condor_state.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_attributes.h"
#include "condor_classad.h"
#include "condor_query.h"
#include "daemon.h"
#include "dc_startd.h"
#include "daemon_types.h"
#include "dc_collector.h"
#include "get_daemon_name.h"
#include "condor_claimid_parser.h"
#include "misc_utils.h"
#include "NegotiationUtils.h"
#include "condor_daemon_core.h"
#include "selector.h"
#include "consumption_policy.h"
#include "subsystem_info.h"
#include "authentication.h"

#include <vector>
#include <string>
#include <deque>

#if defined(WANT_CONTRIB) && defined(WITH_MANAGEMENT) && defined(UNIX)
#include "NegotiatorPlugin.h"
#endif

#include "matchmaker.h"

static int jobsInSlot(ClassAd &job, ClassAd &offer);

// possible outcomes of negotiating with a schedd
enum { MM_ERROR, MM_DONE, MM_RESUME };

// possible outcomes of a matchmaking attempt
enum { _MM_ERROR, MM_NO_MATCH, MM_GOOD_MATCH, MM_BAD_MATCH };

typedef int (*lessThanFunc)(ClassAd*, ClassAd*, void*);

char const *RESOURCES_IN_USE_BY_USER_FN_NAME = "ResourcesInUseByUser";
char const *RESOURCES_IN_USE_BY_USERS_GROUP_FN_NAME = "ResourcesInUseByUsersGroup";

GCC_DIAG_OFF(float-equal)

class NegotiationCycleStats
{
public:
	NegotiationCycleStats();

	time_t start_time;
    time_t end_time;

	time_t duration;
    time_t duration_phase1;
    time_t duration_phase2;
    time_t duration_phase3;
    time_t duration_phase4;

    double cpu_time;
    double phase1_cpu_time;
    double phase2_cpu_time;
    double phase3_cpu_time;
    double phase4_cpu_time;

    time_t prefetch_duration;
    double prefetch_cpu_time;

    int total_slots;
    int trimmed_slots;
    int candidate_slots;

    int slot_share_iterations;

    int num_idle_jobs;
    int num_jobs_considered;

	int matches;
	int rejections;

    int pies;
    int pie_spins;

    // set of unique active schedd, id by sinful strings:
    std::set<std::string> active_schedds;

    // active submitters
	std::set<std::string> active_submitters;

    std::set<std::string> submitters_share_limit;
	std::set<std::string> submitters_out_of_time;
	std::set<std::string> submitters_failed;
	std::set<std::string> schedds_out_of_time;
};

NegotiationCycleStats::NegotiationCycleStats():
    start_time(time(nullptr)),
    end_time(start_time),
	duration(0),
    duration_phase1(0),
    duration_phase2(0),
    duration_phase3(0),
    duration_phase4(0),
    cpu_time(0.0),
    phase1_cpu_time(0.0),
    phase2_cpu_time(0.0),
    phase3_cpu_time(0.0),
    phase4_cpu_time(0.0),
    prefetch_duration(0),
    prefetch_cpu_time(0.0),
    total_slots(0),
    trimmed_slots(0),
    candidate_slots(0),
    slot_share_iterations(0),
    num_idle_jobs(0),
    num_jobs_considered(0),
	matches(0),
	rejections(0),
    pies(0),
    pie_spins(0),
    active_schedds(),
    active_submitters(),
    submitters_share_limit(),
    submitters_out_of_time(),
    submitters_failed(),
    schedds_out_of_time()
{
}


static std::string MachineAdID(ClassAd * ad)
{
	ASSERT(ad);
	std::string addr;
	std::string name;

	// We should always be passed an ad with an ATTR_NAME.
	ASSERT(ad->LookupString(ATTR_NAME, name));
	if(!ad->LookupString(ATTR_STARTD_IP_ADDR, addr)) {
		addr = "<No Address>";
	}

	std::string ID(addr);
	ID += ' ';
	ID += name;
	return ID;
}

static Matchmaker *matchmaker_for_classad_func;

static
bool ResourcesInUseByUser_classad_func( const char * /*name*/,
										 const classad::ArgumentList &arg_list,
										 classad::EvalState &state, classad::Value &result )
{
	classad::Value arg0;
	std::string user;

	ASSERT( matchmaker_for_classad_func );

	// Must have one argument
	if ( arg_list.size() != 1 ) {
		result.SetErrorValue();
		return( true );
	}

	// Evaluate argument
	if( !arg_list[0]->Evaluate( state, arg0 ) ) {
		result.SetErrorValue();
		return false;
	}

	// If argument isn't a string, then the result is an error.
	if( !arg0.IsStringValue( user ) ) {
		result.SetErrorValue();
		return true;
	}

	float usage = matchmaker_for_classad_func->getAccountant().GetWeightedResourcesUsed(user);

	result.SetRealValue( usage );
	return true;
}

static
bool ResourcesInUseByUsersGroup_classad_func( const char * /*name*/,
												const classad::ArgumentList &arg_list,
												classad::EvalState &state, classad::Value &result )
{
	classad::Value arg0;
	std::string user;

	ASSERT( matchmaker_for_classad_func );

	// Must have one argument
	if ( arg_list.size() != 1 ) {
		result.SetErrorValue();
		return( true );
	}

	// Evaluate argument
	if( !arg_list[0]->Evaluate( state, arg0 ) ) {
		result.SetErrorValue();
		return false;
	}

	// If argument isn't a string, then the result is an error.
	if( !arg0.IsStringValue( user ) ) {
		result.SetErrorValue();
		return true;
	}

	double group_quota = 0;
	double group_usage = 0;
	std::string group_name;
	if( !matchmaker_for_classad_func->getGroupInfoFromUserId(user.c_str(),group_name,group_quota,group_usage) ) {
		result.SetErrorValue();
		return true;
	}

	result.SetRealValue( group_usage );
	return true;
}

bool dslotLookup( const classad::ClassAd *ad, const char *name, int idx, classad::Value &value )
{
	if ( ad == NULL || name == NULL || idx < 0 ) {
		return false;
	}
	std::string attr_name = "Child";
	attr_name += name;
	// lookup or evaluate Child<name>
	// set value to idx-th entry of resulting ExprList
	const classad::ExprTree *expr_tree = ad->Lookup( attr_name );
	if ( expr_tree == NULL || expr_tree->GetKind() != classad::ExprTree::EXPR_LIST_NODE ) {
		return false;
	}
	std::vector<classad::ExprTree*> expr_list;
	((const classad::ExprList*)expr_tree)->GetComponents( expr_list );
	if ( (unsigned)idx >= expr_list.size() ) {
		return false;
	}
	if (dynamic_cast<classad::Literal *>(expr_list[idx]) == nullptr) {
		return false;
	}
	((classad::Literal*)expr_list[idx])->GetValue( value );
	return true;
}

bool dslotLookupString( const classad::ClassAd *ad, const char *name, int idx, std::string &value )
{
	classad::Value val;
	if ( !dslotLookup( ad, name, idx, val ) ) {
		return false;
	}
	return val.IsStringValue( value );
}

bool dslotLookupInteger( const classad::ClassAd *ad, const char *name, int idx, int &value )
{
	classad::Value val;
	if ( !dslotLookup( ad, name, idx, val ) ) {
		return false;
	}
	return val.IsNumber( value );
}

bool dslotLookupFloat( const classad::ClassAd *ad, const char *name, int idx, double &value )
{
	classad::Value val;
	if ( !dslotLookup( ad, name, idx, val ) ) {
		return false;
	}
	return val.IsNumber( value );
}

struct rankSorter {
	Matchmaker *mm;
	bool operator()(ClassAd *left, ClassAd *right) {
		double lhscandidateRankValue, lhscandidatePreJobRankValue, lhscandidatePostJobRankValue, lhscandidatePreemptRankValue;
		double rhscandidateRankValue, rhscandidatePreJobRankValue, rhscandidatePostJobRankValue, rhscandidatePreemptRankValue;

		ClassAd dummyRequest;

		mm->calculateRanks(dummyRequest, left, Matchmaker::NO_PREEMPTION, lhscandidateRankValue, lhscandidatePreJobRankValue, lhscandidatePostJobRankValue, lhscandidatePreemptRankValue);
		mm->calculateRanks(dummyRequest, right, Matchmaker::NO_PREEMPTION, rhscandidateRankValue, rhscandidatePreJobRankValue, rhscandidatePostJobRankValue, rhscandidatePreemptRankValue);

		if (lhscandidatePreJobRankValue < rhscandidatePreJobRankValue) {
			return false;
		}

		if (lhscandidatePreJobRankValue > rhscandidatePreJobRankValue) {
			return true;
		}

		// We are intentially skipping the job rank, as we assume it is constant
		if (lhscandidatePostJobRankValue < rhscandidatePostJobRankValue) {
			return false;
		}

		if (lhscandidatePostJobRankValue > rhscandidatePostJobRankValue) {
			return true;
		}

		return left < right;
	};
};

struct submitterLessThan {
	Matchmaker *mm;
	submitterLessThan(Matchmaker *mm) : mm(mm) {}

	bool operator()(ClassAd *ad1, ClassAd *ad2) {

		std::string subname1;
		std::string subname2;

		// nameless submitters are filtered elsewhere
		ad1->LookupString(ATTR_NAME, subname1);
		ad2->LookupString(ATTR_NAME, subname2);
		double prio1 = mm->accountant.GetPriority(subname1);
		double prio2 = mm->accountant.GetPriority(subname2);

		// primary sort on submitter priority
		if (prio1 < prio2) return true;
		if (prio1 > prio2) return false;

		double sr1 = DBL_MAX;
		double sr2 = DBL_MAX;

		if (!ad1->LookupFloat("SubmitterStarvation", sr1)) sr1 = DBL_MAX;
		if (!ad2->LookupFloat("SubmitterStarvation", sr2)) sr2 = DBL_MAX;

		// secondary sort on job prio, if want_globaljobprio is true (see gt #3218)
		if ( mm->want_globaljobprio ) {
			int p1 = INT_MIN;	// no priority should be treated as lowest priority
			int p2 = INT_MIN;
			ad1->LookupInteger(ATTR_JOB_PRIO,p1);
			ad2->LookupInteger(ATTR_JOB_PRIO,p2);
			if (p1 > p2) return true;	// note: higher job prio is "better"
			if (p1 < p2) return false;
		}

		// tertiary sort on submitter starvation
		if (sr1 < sr2) return true;
		if (sr1 > sr2) return false;

		int ts1=0;
		int ts2=0;
		ad1->LookupInteger(ATTR_LAST_HEARD_FROM, ts1);
		ad2->LookupInteger(ATTR_LAST_HEARD_FROM, ts2);

		// when submitters have same name from different schedd, their priorities
		// and starvation ratios will be equal: fallback is to order them randomly
		// to prevent long-term starvation of any one submitter
		return (ts1 % 1009) < (ts2 % 1009);
	}
};

// Return the cpu user time for the current process in seconds.
// TODO Should we include the system time as well?
static double
get_rusage_utime()
{
#if defined(WIN32)
	UINT64 ntCreate=0, ntExit=0, ntSys=0, ntUser=0; // nanotime. tick rate of 100 nanosec.
	ASSERT( GetProcessTimes( GetCurrentProcess(),
							 (FILETIME*)&ntCreate, (FILETIME*)&ntExit,
							 (FILETIME*)&ntSys, (FILETIME*)&ntUser ) );

	return (double)ntUser / (double)(1000*1000*10); // convert to seconds
#else
	struct rusage usage;
	ASSERT( getrusage( RUSAGE_SELF, &usage ) == 0 );
	return usage.ru_utime.tv_sec + ( usage.ru_utime.tv_usec / 1'000'000.0 );
#endif
}

Matchmaker::
Matchmaker ()
   : strSlotConstraint(NULL)
   , SlotPoolsizeConstraint(NULL)
{
	char buf[64];

	NegotiatorName = NULL;
	NegotiatorNameInConfig = false;

	AccountantHost  = NULL;
	PreemptionReq = NULL;
	PreemptionRank = NULL;
	NegotiatorPreJobRank = NULL;
	NegotiatorPostJobRank = NULL;
	sockCache = NULL;

	snprintf (buf, sizeof(buf), "MY.%s > MY.%s", ATTR_RANK, ATTR_CURRENT_RANK);
	ParseClassAdRvalExpr (buf, rankCondStd);

	snprintf (buf, sizeof(buf), "MY.%s >= MY.%s", ATTR_RANK, ATTR_CURRENT_RANK);
	ParseClassAdRvalExpr (buf, rankCondPrioPreempt);

	negotiation_timerID = -1;
	GotRescheduleCmd=false;
	job_attr_references = NULL;
	
	stashedAds = new AdHash(hashFunction);

	MatchList = NULL;
	cachedAutoCluster = -1;
	cachedName = NULL;
	cachedAddr = NULL;

	want_globaljobprio = false;
	want_matchlist_caching = false;
	PublishCrossSlotPrios = false;
	ConsiderPreemption = true;
	ConsiderEarlyPreemption = false;
	MatchWorkingCmSlots = false;
	want_nonblocking_startd_contact = true;

	startedLastCycleTime = 0;
	completedLastCycleTime = (time_t) 0;

	publicAd = NULL;

	update_collector_tid = -1;

	update_interval = 5*MINUTE;

	groupQuotasHash = NULL;

	prevLHF = 0;
	Collectors = 0;

	memset(negotiation_cycle_stats,0,sizeof(negotiation_cycle_stats));
	num_negotiation_cycle_stats = 0;

    hgq_root_group = NULL;
    accept_surplus = false;
    autoregroup = false;

    cp_resources = false;

	rejForNetwork = 0;
	rejForNetworkShare = 0;
	rejPreemptForPrio = 0;
	rejPreemptForPolicy = 0;
	rejPreemptForRank = 0;
	rejForSubmitterLimit = 0;
	rejForConcurrencyLimit = 0;
	rejForSubmitterCeiling = 0;

	cachedPrio = 0;
	cachedOnlyForStartdRank = false;

		// just assign default values
	want_inform_startd = true;
	preemption_req_unstable = true;
	preemption_rank_unstable = true;
	NegotiatorTimeout = 30;
 	NegotiatorInterval = 60;
	NegotiatorMinInterval = 5;
 	MaxTimePerSubmitter = 31536000;
	MaxTimePerSchedd = 31536000;
 	MaxTimePerSpin = 31536000;
	MaxTimePerCycle = 31536000;

	ASSERT( matchmaker_for_classad_func == NULL );
	matchmaker_for_classad_func = this;
	std::string name;
	name = RESOURCES_IN_USE_BY_USER_FN_NAME;
	classad::FunctionCall::RegisterFunction( name,
											 ResourcesInUseByUser_classad_func );
	name = RESOURCES_IN_USE_BY_USERS_GROUP_FN_NAME;
	classad::FunctionCall::RegisterFunction( name,
											 ResourcesInUseByUsersGroup_classad_func );
	slotWeightStr = 0;
	m_staticRanks = false;
	m_dryrun = false;
}

Matchmaker::
~Matchmaker()
{
	if (AccountantHost) free (AccountantHost);
	AccountantHost = NULL;
	if (job_attr_references) free (job_attr_references);
	job_attr_references = NULL;
	delete rankCondStd;
	delete rankCondPrioPreempt;
	delete PreemptionReq;
	delete PreemptionRank;
	delete NegotiatorPreJobRank;
	delete NegotiatorPostJobRank;
	delete sockCache;
	if (MatchList) {
		delete MatchList;
	}
	if ( cachedName ) free(cachedName);
	if ( cachedAddr ) free(cachedAddr);

	free(NegotiatorName);
	if (publicAd) delete publicAd;
    if (SlotPoolsizeConstraint) delete SlotPoolsizeConstraint;
	if (groupQuotasHash) delete groupQuotasHash;
	if (stashedAds) delete stashedAds;
    if (strSlotConstraint) free(strSlotConstraint), strSlotConstraint = NULL;

	int i;
	for(i=0;i<MAX_NEGOTIATION_CYCLE_STATS;i++) {
		delete negotiation_cycle_stats[i];
	}

    if (NULL != hgq_root_group) delete hgq_root_group;

	matchmaker_for_classad_func = NULL;
}


void Matchmaker::
initialize (const char *neg_name)
{
	// If -name or -local-name was given on the command line, use
	// that for the negotiator's name.
	// Otherwise, reinitialize() will set the name based on the
	// config file (or use the default).
	if ( neg_name == NULL ) {
		neg_name = get_mySubSystem()->getLocalName();
	}
	if ( neg_name != NULL ) {
		NegotiatorName = build_valid_daemon_name(neg_name);
	}

	// read in params
	reinitialize ();

    //Alternative permissions for RESCHEDULE Command
    std::vector<DCpermission> alt_reschedulePerms{ADVERTISE_SCHEDD_PERM, ADMINISTRATOR};
    // register commands
    daemonCore->Register_Command (RESCHEDULE, "Reschedule",
            (CommandHandlercpp) &Matchmaker::RESCHEDULE_commandHandler,
             "RESCHEDULE_commandHandler", (Service*) this, DAEMON,
             false, 0, &alt_reschedulePerms);
    daemonCore->Register_Command (RESET_ALL_USAGE, "ResetAllUsage",
            (CommandHandlercpp) &Matchmaker::RESET_ALL_USAGE_commandHandler,
			"RESET_ALL_USAGE_commandHandler", this, ADMINISTRATOR);
    daemonCore->Register_Command (RESET_USAGE, "ResetUsage",
            (CommandHandlercpp) &Matchmaker::RESET_USAGE_commandHandler,
			"RESET_USAGE_commandHandler", this, ADMINISTRATOR);
    daemonCore->Register_Command (DELETE_USER, "DeleteUser",
            (CommandHandlercpp) &Matchmaker::DELETE_USER_commandHandler,
			"DELETE_USER_commandHandler", this, ADMINISTRATOR);
    daemonCore->Register_Command (SET_PRIORITYFACTOR, "SetPriorityFactor",
            (CommandHandlercpp) &Matchmaker::SET_PRIORITYFACTOR_commandHandler,
			"SET_PRIORITYFACTOR_commandHandler", this, WRITE);
    daemonCore->Register_Command (SET_PRIORITY, "SetPriority",
            (CommandHandlercpp) &Matchmaker::SET_PRIORITY_commandHandler,
			"SET_PRIORITY_commandHandler", this, ADMINISTRATOR);
    daemonCore->Register_Command (SET_CEILING, "SetCeiling",
            (CommandHandlercpp) &Matchmaker::SET_CEILING_or_FLOOR_commandHandler,
			"SET_CEILING_commandHandler", this, ADMINISTRATOR);
    daemonCore->Register_Command (SET_FLOOR, "SetFloor",
            (CommandHandlercpp) &Matchmaker::SET_CEILING_or_FLOOR_commandHandler,
			"SET_FLOOR_commandHandler", this, ADMINISTRATOR);
    daemonCore->Register_Command (SET_ACCUMUSAGE, "SetAccumUsage",
            (CommandHandlercpp) &Matchmaker::SET_ACCUMUSAGE_commandHandler,
			"SET_ACCUMUSAGE_commandHandler", this, ADMINISTRATOR);
    daemonCore->Register_Command (SET_BEGINTIME, "SetBeginUsageTime",
            (CommandHandlercpp) &Matchmaker::SET_BEGINTIME_commandHandler,
			"SET_BEGINTIME_commandHandler", this, ADMINISTRATOR);
    daemonCore->Register_Command (SET_LASTTIME, "SetLastUsageTime",
            (CommandHandlercpp) &Matchmaker::SET_LASTTIME_commandHandler,
			"SET_LASTTIME_commandHandler", this, ADMINISTRATOR);
    daemonCore->Register_Command (GET_PRIORITY, "GetPriority",
		(CommandHandlercpp) &Matchmaker::GET_PRIORITY_commandHandler,
			"GET_PRIORITY_commandHandler", this, READ);
    daemonCore->Register_Command (GET_PRIORITY_ROLLUP, "GetPriorityRollup",
		(CommandHandlercpp) &Matchmaker::GET_PRIORITY_ROLLUP_commandHandler,
			"GET_PRIORITY_ROLLUP_commandHandler", this, READ);
	// CRUFT: The original command int for GET_PRIORITY_ROLLUP conflicted
	//   with DRAIN_JOBS. In 7.9.6, we assigned a new command int to
	//   GET_PRIORITY_ROLLUP. Recognize the old int here for now...
    daemonCore->Register_Command (GET_PRIORITY_ROLLUP_OLD, "GetPriorityRollup",
		(CommandHandlercpp) &Matchmaker::GET_PRIORITY_ROLLUP_commandHandler,
			"GET_PRIORITY_ROLLUP_commandHandler", this, READ);
    daemonCore->Register_Command (GET_RESLIST, "GetResList",
		(CommandHandlercpp) &Matchmaker::GET_RESLIST_commandHandler,
			"GET_RESLIST_commandHandler", this, READ);
    daemonCore->Register_Command (QUERY_NEGOTIATOR_ADS, "QUERY_NEGOTIATOR_ADS",
		(CommandHandlercpp) &Matchmaker::QUERY_ADS_commandHandler,
			"QUERY_ADS_commandHandler", this, READ);
    daemonCore->Register_Command (QUERY_ACCOUNTING_ADS, "QUERY_ACCOUNTING_ADS",
		(CommandHandlercpp) &Matchmaker::QUERY_ADS_commandHandler,
			"QUERY_ADS_commandHandler", this, READ);

	// Set a timer to renegotiate.
    negotiation_timerID = daemonCore->Register_Timer (0,  NegotiatorInterval,
			(TimerHandlercpp) &Matchmaker::negotiationTime,
			"Time to negotiate", this);

	update_collector_tid = daemonCore->Register_Timer (
			0, update_interval,
			(TimerHandlercpp) &Matchmaker::updateCollector,
			"Update Collector", this );


#if defined(WANT_CONTRIB) && defined(WITH_MANAGEMENT) && defined(UNIX)
	NegotiatorPluginManager::Load();
	NegotiatorPluginManager::Initialize();
#endif
}


int Matchmaker::
reinitialize ()
{
	// NOTE: reinitialize() is also called on startup

	char *tmp;
	static bool first_time = true;

    // (re)build the HGQ group tree from configuration
    // need to do this prior to initializing the accountant
	delete hgq_root_group;
    hgq_root_group = GroupEntry::hgq_construct_tree(hgq_groups, this->autoregroup, this->accept_surplus);

    // Initialize accountant params
    accountant.Initialize(hgq_root_group);

	if ( NegotiatorNameInConfig || NegotiatorName == NULL ) {
		char *tmp = param( "NEGOTIATOR_NAME" );
		free( NegotiatorName );
		if ( tmp ) {
			NegotiatorName = build_valid_daemon_name( tmp );
			free( tmp );
			NegotiatorNameInConfig = true;
		} else {
			NegotiatorName = default_daemon_name();
		}
	}

	init_public_ad();

	// get timeout values

 	NegotiatorInterval = param_integer("NEGOTIATOR_INTERVAL",60);

	NegotiatorMinInterval = param_integer("NEGOTIATOR_MIN_INTERVAL",5);

	NegotiatorTimeout = param_integer("NEGOTIATOR_TIMEOUT",30);

	// up to 1 year per negotiation cycle
	MaxTimePerCycle = param_integer("NEGOTIATOR_MAX_TIME_PER_CYCLE",1200);

	// up to 1 year per submitter by default
	MaxTimePerSubmitter = param_integer("NEGOTIATOR_MAX_TIME_PER_SUBMITTER",60);

	// up to 1 year per schedd by default
	MaxTimePerSchedd = param_integer("NEGOTIATOR_MAX_TIME_PER_SCHEDD",120);

	// up to 1 year per spin by default
	MaxTimePerSpin = param_integer("NEGOTIATOR_MAX_TIME_PER_PIESPIN",120);

	// deal with a possibly resized socket cache, or create the socket
	// cache if this is the first time we got here.
	//
	// we call the resize method which:
	// - does nothing if the size is the same
	// - preserves the old sockets if the size has grown
	// - does nothing (except dprintf into the log) if the size has shrunk.
	//
	// the user must call condor_restart to actually shrink the sockCache.

	int socket_cache_size = param_integer("NEGOTIATOR_SOCKET_CACHE_SIZE",DEFAULT_SOCKET_CACHE_SIZE,1);
	if( socket_cache_size ) {
		dprintf (D_ALWAYS,"NEGOTIATOR_SOCKET_CACHE_SIZE = %d\n", socket_cache_size);
	}
	if (sockCache) {
		sockCache->resize(socket_cache_size);
	} else {
		sockCache = new SocketCache(socket_cache_size);
	}

	// get PreemptionReq expression
	if (PreemptionReq) delete PreemptionReq;
	PreemptionReq = NULL;
	tmp = param("PREEMPTION_REQUIREMENTS");
	if( tmp ) {
		if( ParseClassAdRvalExpr(tmp, PreemptionReq) ) {
			EXCEPT ("Error parsing PREEMPTION_REQUIREMENTS expression: %s",
					tmp);
		}
		dprintf (D_ALWAYS,"PREEMPTION_REQUIREMENTS = %s\n", tmp);
		free( tmp );
		tmp = NULL;
	} else {
		dprintf (D_ALWAYS,"PREEMPTION_REQUIREMENTS = None\n");
	}

	NegotiatorMatchExprs.clear();
	tmp = param("NEGOTIATOR_MATCH_EXPRS");
	if( tmp ) {
		for (const auto& expr_name: StringTokenIterator(tmp)) {
			// Now read in the values of the macros in the list.
			char* expr_value = param(expr_name.c_str());
			if (!expr_value ) {
				dprintf(D_ALWAYS, "Warning: NEGOTIATOR_MATCH_EXPRS references a macro '%s' which is not defined in the configuration file.\n", expr_name.c_str());
				continue;
			}
			// Now change the names of the ExprNames so they have the prefix
			// "MatchExpr" that is expected by the schedd.
			std::string new_name;
			if (!expr_name.starts_with(ATTR_NEGOTIATOR_MATCH_EXPR)) {
				new_name = ATTR_NEGOTIATOR_MATCH_EXPR;
			}
			new_name += expr_name;
			NegotiatorMatchExprs.emplace(new_name, expr_value);
			free(expr_value);
		}
		free( tmp );
		tmp = NULL;
	}

	dprintf (D_ALWAYS,"ACCOUNTANT_HOST = %s\n", AccountantHost ?
			AccountantHost : "None (local)");
	dprintf (D_ALWAYS,"NEGOTIATOR_INTERVAL = %lld sec\n",(long long)NegotiatorInterval);
	dprintf (D_ALWAYS,"NEGOTIATOR_MIN_INTERVAL = %lld sec\n",(long long)NegotiatorMinInterval);
	dprintf (D_ALWAYS,"NEGOTIATOR_TIMEOUT = %d sec\n",NegotiatorTimeout);
	dprintf (D_ALWAYS,"MAX_TIME_PER_CYCLE = %d sec\n",MaxTimePerCycle);
	dprintf (D_ALWAYS,"MAX_TIME_PER_SUBMITTER = %d sec\n",MaxTimePerSubmitter);
	dprintf (D_ALWAYS,"MAX_TIME_PER_SCHEDD = %d sec\n",MaxTimePerSchedd);
	dprintf (D_ALWAYS,"MAX_TIME_PER_PIESPIN = %d sec\n",MaxTimePerSpin);

	if (PreemptionRank) {
		delete PreemptionRank;
		PreemptionRank = NULL;
	}
	tmp = param("PREEMPTION_RANK");
	if( tmp ) {
		if( ParseClassAdRvalExpr(tmp, PreemptionRank) ) {
			EXCEPT ("Error parsing PREEMPTION_RANK expression: %s", tmp);
		}
	}

	dprintf (D_ALWAYS,"PREEMPTION_RANK = %s\n", (tmp?tmp:"None"));

	if( tmp ) free( tmp );

	if (NegotiatorPreJobRank) delete NegotiatorPreJobRank;
	NegotiatorPreJobRank = NULL;
	tmp = param("NEGOTIATOR_PRE_JOB_RANK");
	if( tmp ) {
		if( ParseClassAdRvalExpr(tmp, NegotiatorPreJobRank) ) {
			EXCEPT ("Error parsing NEGOTIATOR_PRE_JOB_RANK expression: %s", tmp);
		}
	}

	dprintf (D_ALWAYS,"NEGOTIATOR_PRE_JOB_RANK = %s\n", (tmp?tmp:"None"));

	if( tmp ) free( tmp );

	if (NegotiatorPostJobRank) delete NegotiatorPostJobRank;
	NegotiatorPostJobRank = NULL;
	tmp = param("NEGOTIATOR_POST_JOB_RANK");
	if( tmp ) {
		if( ParseClassAdRvalExpr(tmp, NegotiatorPostJobRank) ) {
			EXCEPT ("Error parsing NEGOTIATOR_POST_JOB_RANK expression: %s", tmp);
		}
	}

	dprintf (D_ALWAYS,"NEGOTIATOR_POST_JOB_RANK = %s\n", (tmp?tmp:"None"));
	
	if( tmp ) free( tmp );


		// how often we update the collector, fool
 	update_interval = param_integer ("NEGOTIATOR_UPDATE_INTERVAL",
									 5*MINUTE);



	char *preferred_collector = param ("COLLECTOR_HOST_FOR_NEGOTIATOR");
	if ( preferred_collector ) {
		CollectorList* collectors = daemonCore->getCollectorList();
		collectors->resortLocal( preferred_collector );
		free( preferred_collector );
	}

	want_globaljobprio = param_boolean("USE_GLOBAL_JOB_PRIOS",false);
	want_matchlist_caching = param_boolean("NEGOTIATOR_MATCHLIST_CACHING",true);
	PublishCrossSlotPrios = param_boolean("NEGOTIATOR_CROSS_SLOT_PRIOS", false);
	ConsiderPreemption = param_boolean("NEGOTIATOR_CONSIDER_PREEMPTION",true);
	ConsiderEarlyPreemption = param_boolean("NEGOTIATOR_CONSIDER_EARLY_PREEMPTION",false);
	if( ConsiderEarlyPreemption && !ConsiderPreemption ) {
		dprintf(D_ALWAYS,"WARNING: NEGOTIATOR_CONSIDER_EARLY_PREEMPTION=true will be ignored, because NEGOTIATOR_CONSIDER_PREEMPTION=false\n");
	}
	MatchWorkingCmSlots = param_boolean("MATCH_WORKING_CM_SLOTS", false);
	want_inform_startd = param_boolean("NEGOTIATOR_INFORM_STARTD", false);
	want_nonblocking_startd_contact = param_boolean("NEGOTIATOR_USE_NONBLOCKING_STARTD_CONTACT",true);

	// we should figure these out automatically someday ....
	preemption_req_unstable = ! (param_boolean("PREEMPTION_REQUIREMENTS_STABLE",true)) ;
	preemption_rank_unstable = ! (param_boolean("PREEMPTION_RANK_STABLE",true)) ;

    // load the constraint for slots that will be available for matchmaking.
    // used for sharding or as an alternative to GROUP_DYNAMIC_MACH_CONSTRAINT
    // or NEGOTIATOR_SLOT_POOLSIZE_CONSTRAINT when you DONT ever want to negotiate on
    // slots that don't match the constraint.
    if (strSlotConstraint) free(strSlotConstraint);
    strSlotConstraint = param ("NEGOTIATOR_SLOT_CONSTRAINT");
    if (strSlotConstraint) {
       dprintf (D_FULLDEBUG, "%s = %s\n", "NEGOTIATOR_SLOT_CONSTRAINT",
                strSlotConstraint);
       // do a test parse of the constraint before we try and use it.
       ExprTree *SlotConstraint = NULL;
       if (ParseClassAdRvalExpr(strSlotConstraint, SlotConstraint)) {
          EXCEPT("Error parsing NEGOTIATOR_SLOT_CONSTRAINT expresion: %s",
                  strSlotConstraint);
       }
       delete SlotConstraint;
    }

    // load the constraint for calculating the poolsize for matchmaking
    // used to ignore some slots for calculating the poolsize, but not
    // for matchmaking.
    //
	if (SlotPoolsizeConstraint) delete SlotPoolsizeConstraint;
	SlotPoolsizeConstraint = NULL;
    const char * attr = "NEGOTIATOR_SLOT_POOLSIZE_CONSTRAINT";
	tmp = param(attr);
    if ( ! tmp) {
       attr = "GROUP_DYNAMIC_MACH_CONSTRAINT";
       tmp = param(attr);
       if (tmp) dprintf(D_ALWAYS, "%s is obsolete, use NEGOTIATOR_SLOT_POOLSIZE_CONSTRAINT instead\n", attr);
    }
	if( tmp ) {
        dprintf(D_FULLDEBUG, "%s = %s\n", attr, tmp);
		if( ParseClassAdRvalExpr(tmp, SlotPoolsizeConstraint) ) {
			dprintf(D_ALWAYS, "Error parsing %s expression: %s\n", attr, tmp);
            SlotPoolsizeConstraint = NULL;
		}
        free (tmp);
	}

	m_SubmitterConstraintStr.clear();
	param(m_SubmitterConstraintStr, "NEGOTIATOR_SUBMITTER_CONSTRAINT");
	if (!m_SubmitterConstraintStr.empty()) {
		dprintf (D_FULLDEBUG, "%s = %s\n", "NEGOTIATOR_SUBMITTER_CONSTRAINT",
		         m_SubmitterConstraintStr.c_str());
		// do a test parse of the constraint before we try and use it.
		ExprTree *SlotConstraint = NULL;
		if (ParseClassAdRvalExpr(m_SubmitterConstraintStr.c_str(), SlotConstraint)) {
			EXCEPT("Error parsing NEGOTIATOR_SUBMITTER_CONSTRAINT expresion: %s",
			       m_SubmitterConstraintStr.c_str());
		}
		delete SlotConstraint;
	}

	m_JobConstraintStr.clear();
	param(m_JobConstraintStr, "NEGOTIATOR_JOB_CONSTRAINT");

	num_negotiation_cycle_stats = param_integer("NEGOTIATION_CYCLE_STATS_LENGTH",3,0,MAX_NEGOTIATION_CYCLE_STATS);
	ASSERT( num_negotiation_cycle_stats <= MAX_NEGOTIATION_CYCLE_STATS );

	m_staticRanks = param_boolean("NEGOTIATOR_IGNORE_JOB_RANKS", false);

	if( first_time ) {
		first_time = false;
	} else {
			// be sure to try to publish a new negotiator ad on reconfig
		updateCollector();
	}

	if (slotWeightStr) free(slotWeightStr);

	slotWeightStr = param("SLOT_WEIGHT");
	if (!slotWeightStr) {
		slotWeightStr = strdup("Cpus");
	}


	// done
	return TRUE;
}

void
Matchmaker::SetupMatchSecurity(std::vector<ClassAd *> &submitterAds)
{
	if (!param_boolean("SEC_ENABLE_MATCH_PASSWORD_AUTHENTICATION", true)) {
		return;
	}
	dprintf(D_SECURITY, "Will look for match security sessions.\n");

	std::set<std::pair<std::string, std::string>> capabilities;
	SecMan *secman = daemonCore->getSecMan();
	for (classad::ClassAd *ad: submitterAds) {
		std::string capability;
		std::string sinful;
		std::string version;
		if (!ad->EvaluateAttrString(ATTR_CAPABILITY, capability) ||
			!ad->EvaluateAttrString(ATTR_MY_ADDRESS, sinful))
		{
			dprintf(D_SECURITY, "No capability present for ad from %s.\n", sinful.c_str());
			dPrintAd(D_SECURITY, *ad);
			continue;
		}
		if (capabilities.find(std::make_pair(sinful, capability)) != capabilities.end()) {
			// Already saw a submitter ad from this schedd with this capability
			continue;
		}
		capabilities.insert(std::make_pair(sinful, capability));
		// CRUFT: 8.9.3 schedds send a capability that doesn't include
		//   the encryption algorithm (attribute CryptoMethods), and the
		//   default algorithm changed from 3DES to BLOWFISH in 8.9.4.
		//   So we'll insert a CryptoMethods attribute into the session
		//   policy for capabilities from 8.9.3 schedds.
		bool old_schedd = false;
		if (ad->EvaluateAttrString(ATTR_VERSION, version)) {
			CondorVersionInfo vi(version.c_str());
			if (!vi.built_since_version(8, 9, 4)) {
				old_schedd = true;
			}
		}
		ClaimIdParser cidp(capability.c_str());
		dprintf(D_FULLDEBUG, "Creating a new session for capability %s\n", cidp.publicClaimId());
		const char *session_info = cidp.secSessionInfo();
		std::string info_str;
		if ( old_schedd && session_info ) {
			dprintf(D_FULLDEBUG, "Adding CryptoMethods=\"3DES\" to session policy of 8.9.3 schedd\n");
			info_str = session_info;
			info_str.insert(1, "CryptoMethods=\"3DES\";");
			session_info = info_str.c_str();
		}
		secman->CreateNonNegotiatedSecuritySession(
			CLIENT_PERM,
			cidp.secSessionId(),
			cidp.secSessionKey(),
			session_info,
			AUTH_METHOD_MATCH,
			SUBMIT_SIDE_MATCHSESSION_FQU,
			sinful.c_str(),
			1200,
			nullptr, false
		);

	}
}

int Matchmaker::
RESCHEDULE_commandHandler (int, Stream *strm)
{
	// read the required data off the wire
	if (!strm->end_of_message())
	{
		dprintf (D_ALWAYS, "Could not read eom\n");
		return FALSE;
	}

	if (GotRescheduleCmd) return TRUE;
	GotRescheduleCmd=true;
	daemonCore->Reset_Timer(negotiation_timerID,0,
							NegotiatorInterval);
	return TRUE;
}


int Matchmaker::
RESET_ALL_USAGE_commandHandler (int, Stream *strm)
{
	// read the required data off the wire
	if (!strm->end_of_message())
	{
		dprintf (D_ALWAYS, "Could not read eom\n");
		return FALSE;
	}

	// reset usage
	dprintf (D_ALWAYS,"Resetting the usage of all users\n");
	accountant.ResetAllUsage();
	
	return TRUE;
}

int Matchmaker::
DELETE_USER_commandHandler (int, Stream *strm)
{
    std::string submitter;

	// read the required data off the wire
	if (!strm->get(submitter) 	||
		!strm->end_of_message())
	{
		dprintf (D_ALWAYS, "Could not read accountant record name\n");
		return FALSE;
	}

	// reset usage
	dprintf (D_ALWAYS,"Deleting accountanting record of %s\n", submitter.c_str());
	accountant.DeleteRecord(submitter);
	
	return TRUE;
}

int Matchmaker::
RESET_USAGE_commandHandler (int, Stream *strm)
{
    std::string submitter;

	// read the required data off the wire
	if (!strm->get(submitter) 	||
		!strm->end_of_message())
	{
		dprintf (D_ALWAYS, "Could not read submitter name\n");
		return FALSE;
	}

	// reset usage
	dprintf(D_ALWAYS, "Resetting the usage of %s\n", submitter.c_str());
	accountant.ResetAccumulatedUsage(submitter);
	
	return TRUE;
}

namespace {

bool returnPrioFactor(Stream *strm, CondorError &errstack)
{
	auto version = strm->get_peer_version();
	if (version && version->built_since_version(8, 9, 9)) {
		classad::ClassAd ad;
		if (errstack.empty()) {
			ad.InsertAttr(ATTR_ERROR_CODE, 0);
		} else {
			dprintf(D_ALWAYS, "Failed to set priority factor: %s\n", errstack.message());
			ad.InsertAttr(ATTR_ERROR_CODE, errstack.code());
			ad.InsertAttr(ATTR_ERROR_STRING, errstack.message());
		}
		strm->encode();
		if (!putClassAd(strm, ad) || strm->end_of_message()) {
			dprintf(D_ALWAYS, "Failed to send response for SET_PRIORITY_FACTOR command.\n");
			return false;
		}
	} else if (!errstack.empty()) {
		dprintf(D_ALWAYS, "Failed to set priority factor: %s\n", errstack.message());
	}
	return errstack.empty();
}

}

int Matchmaker::
SET_PRIORITYFACTOR_commandHandler (int, Stream *strm)
{
	double	priority;
	std::string submitter;

	// read the required data off the wire
	if (!strm->get(submitter) 	||
		!strm->get(priority) 	||
		!strm->end_of_message())
	{
		dprintf (D_ALWAYS, "Could not read submitter name and priority factor\n");
		return false;
	}

	const char *peer_identity = static_cast<Sock*>(strm)->getFullyQualifiedUser();

	// If we have admin, we can always set the priority.  Otherwise, we need to
	// check our group mapping.
	bool has_admin = static_cast<Sock*>(strm)->isAuthorizationInBoundingSet("ADMINISTRATOR") &&
		daemonCore->Verify("set prio factor", ADMINISTRATOR,
		static_cast<ReliSock*>(strm)->peer_addr(),
		peer_identity);

	CondorError errstack;
	bool authorized = has_admin;
	if (!has_admin) {

		dprintf(D_FULLDEBUG, "Will check if the authenticated user %s is authorized to edit submitter %s prio factor.\n",
			peer_identity ? peer_identity : "(unknown)", submitter.c_str());
		std::string map_names;
		if (!param(map_names, "NEGOTIATOR_CLASSAD_USER_MAP_NAMES")) {
			errstack.pushf("NEGOTIATOR", 1, "Not an administrator and authorization maps (NEGOTIATOR_CLASSAD_USER_MAP_NAMES) is not set.");
			return returnPrioFactor(strm, errstack);
		}
		std::vector<std::string> map_names_list = split(map_names);
		if (!contains(map_names_list, "PRIORITY_FACTOR_AUTHORIZATION")) {
			errstack.pushf("NEGOTIATOR", 2, "Not an administrator and PRIORITY_FACTOR_AUTHORIZATION not a configured map file.");
			return returnPrioFactor(strm, errstack);
		}

		if (!peer_identity) {
			errstack.pushf("NEGOTIATOR", 3, "Not an administrator and client is not authenticated.");
			return returnPrioFactor(strm, errstack);
		}

		std::string map_output;
		if (user_map_do_mapping("PRIORITY_FACTOR_AUTHORIZATION", peer_identity, map_output)) {
			for (const auto& item: StringTokenIterator(map_output, ",")) {
				if (submitter.starts_with(item)) {
					authorized = true;
				}
			}
		}
	}
	if (!authorized) {
		errstack.pushf("NEGOTIATOR", 4, "Client %s requested to set the priority factor of %s but is not authorized.",
			peer_identity, submitter.c_str());
		return returnPrioFactor(strm, errstack);
	}

	// set the priority
	dprintf(D_ALWAYS,"Setting the priority factor of %s to %f\n", submitter.c_str(), priority);
	accountant.SetPriorityFactor(submitter, priority);
	
	return returnPrioFactor(strm, errstack);
}

int Matchmaker::
SET_CEILING_or_FLOOR_commandHandler (int command, Stream *stream) {
	int	value;
    std::string submitter;

	// read the required data off the wire
	if (!stream->get(submitter) 	||
		!stream->get(value) 	||
		!stream->end_of_message())
	{
		dprintf (D_ALWAYS, "SET_CEILING_or_FLOOR: Could not read submitter name and ceiling/floor\n");
		return FALSE;
	}

	// set the ceiling
	if (command == SET_CEILING) {
		dprintf(D_ALWAYS,"Setting the ceiling of %s to %d\n", submitter.c_str(), value);
		accountant.SetCeiling(submitter, value);
	}
	
	// set the floor
	if (command == SET_FLOOR) {
		dprintf(D_ALWAYS,"Setting the floor of %s to %d\n", submitter.c_str(), value);
		accountant.SetFloor(submitter, value);
	}
	return TRUE;
}


int Matchmaker::
SET_PRIORITY_commandHandler (int, Stream *strm)
{
	double	priority;
    std::string submitter;

	// read the required data off the wire
	if (!strm->get(submitter) 	||
		!strm->get(priority) 	||
		!strm->end_of_message())
	{
		dprintf (D_ALWAYS, "Could not read submitter name and priority\n");
		return FALSE;
	}

	// set the priority
	dprintf(D_ALWAYS,"Setting the priority of %s to %f\n",submitter.c_str(),priority);
	accountant.SetPriority(submitter, priority);
	
	return TRUE;
}

int Matchmaker::
SET_ACCUMUSAGE_commandHandler (int, Stream *strm)
{
	double	accumUsage;
    std::string submitter;

	// read the required data off the wire
	if (!strm->get(submitter) 	||
		!strm->get(accumUsage) 	||
		!strm->end_of_message())
	{
		dprintf (D_ALWAYS, "Could not read submitter name and accumulatedUsage\n");
		return FALSE;
	}

	// set the priority
	dprintf(D_ALWAYS,"Setting the accumulated usage of %s to %f\n", submitter.c_str(), accumUsage);
	accountant.SetAccumUsage(submitter, accumUsage);
	
	return TRUE;
}

int Matchmaker::
SET_BEGINTIME_commandHandler (int, Stream *strm)
{
	int	beginTime;
    std::string submitter;

	// read the required data off the wire
	if (!strm->get(submitter) 	||
		!strm->get(beginTime) 	||
		!strm->end_of_message())
	{
		dprintf (D_ALWAYS, "Could not read submitter name and begin usage time\n");
		return FALSE;
	}

	// set the priority
	dprintf(D_ALWAYS, "Setting the begin usage time of %s to %d\n", submitter.c_str(), beginTime);
	accountant.SetBeginTime(submitter, beginTime);
	
	return TRUE;
}

int Matchmaker::
SET_LASTTIME_commandHandler (int, Stream *strm)
{
	int	lastTime;
    std::string submitter;

	// read the required data off the wire
	if (!strm->get(submitter) 	||
		!strm->get(lastTime) 	||
		!strm->end_of_message())
	{
		dprintf (D_ALWAYS, "Could not read submitter name and last usage time\n");
		return FALSE;
	}

	// set the priority
	dprintf(D_ALWAYS,"Setting the last usage time of %s to %d\n", submitter.c_str(), lastTime);
	accountant.SetLastTime(submitter, lastTime);
	
	return TRUE;
}


int Matchmaker::
GET_PRIORITY_commandHandler (int, Stream *strm)
{
	// read the required data off the wire
	if (!strm->end_of_message())
	{
		dprintf (D_ALWAYS, "GET_PRIORITY: Could not read eom\n");
		return FALSE;
	}

	// get the priority
	dprintf (D_ALWAYS,"Getting state information from the accountant\n");
	ClassAd* ad=accountant.ReportState();
	
	if (!putClassAd(strm, *ad, PUT_CLASSAD_NO_TYPES) ||
	    !strm->end_of_message())
	{
		dprintf (D_ALWAYS, "Could not send priority information\n");
		delete ad;
		return FALSE;
	}

	delete ad;

	return TRUE;
}


int Matchmaker::
GET_PRIORITY_ROLLUP_commandHandler(int, Stream *strm) {
    // read the required data off the wire
    if (!strm->end_of_message()) {
        dprintf (D_ALWAYS, "GET_PRIORITY_ROLLUP: Could not read eom\n");
        return FALSE;
    }

    // get the priority
    dprintf(D_ALWAYS, "Getting state information from the accountant\n");
    ClassAd* ad = accountant.ReportState(true);

    if (!putClassAd(strm, *ad, PUT_CLASSAD_NO_TYPES) ||
        !strm->end_of_message()) {
        dprintf (D_ALWAYS, "Could not send priority information\n");
        delete ad;
        return FALSE;
	}

    delete ad;
    return TRUE;
}


int Matchmaker::
GET_RESLIST_commandHandler (int, Stream *strm)
{
    std::string submitter;

    // read the required data off the wire
    if (!strm->get(submitter)     ||
        !strm->end_of_message())
    {
        dprintf (D_ALWAYS, "Could not read submitter name\n");
        return FALSE;
    }

    // reset usage
    dprintf(D_ALWAYS, "Getting resource list of %s\n", submitter.c_str());

	// get the priority
	ClassAd* ad=accountant.ReportState(submitter);
	dprintf (D_ALWAYS,"Getting state information from the accountant\n");
	
	if (!putClassAd(strm, *ad, PUT_CLASSAD_NO_TYPES) ||
	    !strm->end_of_message())
	{
		dprintf (D_ALWAYS, "Could not send resource list\n");
		delete ad;
		return FALSE;
	}

	delete ad;

	return TRUE;
}

int Matchmaker::
QUERY_ADS_commandHandler (int cmd, Stream *strm)
{
	bool query_negotiator_ad =  (cmd == QUERY_NEGOTIATOR_ADS);
	bool query_accounting_ads = (cmd == QUERY_ACCOUNTING_ADS);

	ClassAd queryAd;
	ClassAd *ad;
	ClassAdList ads;
	int more = 1, num_ads = 0;
	dprintf( D_FULLDEBUG, "In QUERY_ADS_commandHandler cmd=%d\n", cmd );

	strm->decode();
	strm->timeout(15);
	if( !getClassAd(strm, queryAd) || !strm->end_of_message()) {
		dprintf( D_ALWAYS, "Failed to receive query on TCP: aborting\n" );
		return FALSE;
	}

		// Construct a list of all our ClassAds that match the query
	if (query_negotiator_ad) {
		std::string stats_config;
		queryAd.LookupString("STATISTICS_TO_PUBLISH", stats_config);
		if ( ! publicAd) { init_public_ad(); }
		if (publicAd) {
			ad = new ClassAd(*publicAd);
			publishNegotiationCycleStats(ad);
			daemonCore->dc_stats.Publish(*ad, stats_config.c_str());
			daemonCore->monitor_data.ExportData(ad);
			ads.Insert(ad);
		}
	}
	if (query_accounting_ads) {
		this->accountant.ReportState(queryAd, ads);
	}

	classad::References proj;
	std::string projection;
	if (queryAd.LookupString(ATTR_PROJECTION, projection) && ! projection.empty()) {
		StringTokenIterator list(projection);
		const std::string * attr;
		while ((attr = list.next_string())) { proj.insert(*attr); }
	}

		// Now, return the ClassAds that match.
	strm->encode();
	ads.Open();
	while( (ad = ads.Next()) ) {
		if( !strm->code(more) || !putClassAd(strm, *ad, PUT_CLASSAD_NO_PRIVATE, proj.empty() ? NULL : &proj) ) {
			dprintf (D_ALWAYS, 
						"Error sending query result to client -- aborting\n");
			return FALSE;
		}
		num_ads++;
	}

		// Finally, close up shop.  We have to send NO_MORE.
	more = 0;
	if( !strm->code(more) || !strm->end_of_message() ) {
		dprintf( D_ALWAYS, "Error sending EndOfResponse (0) to client\n" );
		return FALSE;
	}
	dprintf( D_FULLDEBUG, "Sent %d ads in response to query\n", num_ads ); 
	return TRUE;
}


char *
Matchmaker::
compute_significant_attrs(std::vector<ClassAd *> & startdAds)
{
	char *result = NULL;

	// Figure out list of all external attribute references in all startd ads
	//
	// When getting references across many ads, it's faster to call the
	// base ClassAd method GetExternalReferences(), building up a merged
	// set of full reference names and then call TrimReferenceNames()
	// on that.
	dprintf(D_FULLDEBUG,"Entering compute_significant_attrs()\n");
	ClassAd *sample_startd_ad = nullptr;
	classad::References external_references;
	for (ClassAd *startd_ad: startdAds) {
		if ( !sample_startd_ad ) {
			sample_startd_ad = new ClassAd(*startd_ad);
		}
		classad::ClassAd::const_iterator attr_it;
		for ( attr_it = startd_ad->begin(); attr_it != startd_ad->end(); attr_it++ ) {
			// ignore list type values when computing external refs.
			// this prevents Child* and AvailableGPUs slot attributes from polluting the sig attrs
			if (attr_it->second->GetKind() == classad::ExprTree::EXPR_LIST_NODE) {
				continue;
			}
			startd_ad->GetExternalReferences( attr_it->second, external_references, true );
		}
	}	// while startd_ad

	// Now add external attributes references from negotiator policy exprs; at
	// this point, we only have to worry about PREEMPTION_REQUIREMENTS.
	// PREEMPTION_REQUIREMENTS is evaluated in the context of a machine ad
	// followed by a job ad.  So to help figure out the external (job) attributes
	// that are significant, we take a sample startd ad and add any startd_job_exprs
	// to it.
	if (!sample_startd_ad) {	// if no startd ads, just return.
		return NULL;	// if no startd ads, there are no sig attrs
	}
	//bool has_startd_job_attrs = false;
	auto_free_ptr startd_job_attrs(param("STARTD_JOB_ATTRS"));
	if ( ! startd_job_attrs.empty()) { // add in startd_job_attrs
		StringTokenIterator attrs(startd_job_attrs);
		for (const char * attr = attrs.first(); attr; attr = attrs.next()) {
			sample_startd_ad->Assign(attr, true);
			//has_startd_job_attrs = true;
		}
	}
	startd_job_attrs.set(param("STARTD_JOB_EXPRS"));
	if ( ! startd_job_attrs.empty()) { // add in (obsolete) startd_job_exprs
		StringTokenIterator attrs(startd_job_attrs);
		for (const char * attr = attrs.first(); attr; attr = attrs.next()) {
			sample_startd_ad->Assign(attr, true);
		}
		//if (has_startd_job_attrs) { dprintf(D_FULLDEBUG, "Warning: both STARTD_JOB_ATTRS and STARTD_JOB_EXPRS specified, for now these will be merged, but you should use only STARTD_JOB_ATTRS\n"); }
	}
	// Now add in the job attrs required by HTCondor.
	startd_job_attrs.set(param("SYSTEM_STARTD_JOB_ATTRS"));
	if ( ! startd_job_attrs.empty()) { // add in startd_job_attrs
		StringTokenIterator attrs(startd_job_attrs);
		for (const char * attr = attrs.first(); attr; attr = attrs.next()) {
			sample_startd_ad->Assign(attr, true);
		}
	}

	char *tmp=param("PREEMPTION_REQUIREMENTS");
	if ( tmp && PreemptionReq ) {	// add references from preemption_requirements
		const char* preempt_req_name = "preempt_req__";	// any name will do
		sample_startd_ad->AssignExpr(preempt_req_name,tmp);
		ExprTree *expr = sample_startd_ad->Lookup(preempt_req_name);
		if ( expr != NULL ) {
			sample_startd_ad->GetExternalReferences(expr,external_references,true);
		}
	}
	free(tmp);

	tmp=param("PREEMPTION_RANK");
	if ( tmp && PreemptionRank) {
		const char* preempt_rank_name = "preempt_rank__";	// any name will do
		sample_startd_ad->AssignExpr(preempt_rank_name,tmp);
		ExprTree *expr = sample_startd_ad->Lookup(preempt_rank_name);
		if ( expr != NULL ) {
			sample_startd_ad->GetExternalReferences(expr,external_references,true);
		}
	}
	free(tmp);

	tmp=param("NEGOTIATOR_PRE_JOB_RANK");
	if ( tmp && NegotiatorPreJobRank) {
		const char* pre_job_rank_name = "pre_job_rank__";	// any name will do
		sample_startd_ad->AssignExpr(pre_job_rank_name,tmp);
		ExprTree *expr = sample_startd_ad->Lookup(pre_job_rank_name);
		if ( expr != NULL ) {
			sample_startd_ad->GetExternalReferences(expr,external_references,true);
		}
	}
	free(tmp);

	tmp=param("NEGOTIATOR_POST_JOB_RANK");
	if ( tmp && NegotiatorPostJobRank) {
		const char* post_job_rank_name = "post_job_rank__";	// any name will do
		sample_startd_ad->AssignExpr(post_job_rank_name,tmp);
		ExprTree *expr = sample_startd_ad->Lookup(post_job_rank_name);
		if ( expr != NULL ) {
			sample_startd_ad->GetExternalReferences(expr,external_references,true);
		}
	}
	free(tmp);


	if (sample_startd_ad) {
		delete sample_startd_ad;
		sample_startd_ad = NULL;
	}

	// We also need to include references in NEGOTIATOR_JOB_CONSTRAINT
	if ( !m_JobConstraintStr.empty() ) {
		ClassAd empty_ad;
		empty_ad.AssignExpr("__JobConstraint",m_JobConstraintStr.c_str());
		ExprTree *expr = empty_ad.Lookup("__JobConstraint");
		if ( expr ) {
			empty_ad.GetExternalReferences(expr,external_references,true);
		}
	}

	// Simplify the attribute references
	TrimReferenceNames( external_references, true );

		// Always get rid of the follow attrs:
		//    CurrentTime - for obvious reasons
		//    RemoteUserPrio - not needed since we negotiate per user
		//    SubmittorPrio - not needed since we negotiate per user
	external_references.erase(ATTR_CURRENT_TIME);
	external_references.erase(ATTR_REMOTE_USER_PRIO);
	external_references.erase(ATTR_REMOTE_USER_RESOURCES_IN_USE);
	external_references.erase(ATTR_REMOTE_GROUP_RESOURCES_IN_USE);
	external_references.erase(ATTR_SUBMITTOR_PRIO);
	external_references.erase(ATTR_SUBMITTER_USER_PRIO);
	external_references.erase(ATTR_SUBMITTER_USER_RESOURCES_IN_USE);
	external_references.erase(ATTR_SUBMITTER_GROUP_RESOURCES_IN_USE);

	classad::References::iterator it;
	std::string list_str;
	for ( it = external_references.begin(); it != external_references.end(); it++ ) {
		if ( !list_str.empty() ) {
			list_str += ',';
		}
		list_str += *it;
	}

	result = strdup( list_str.c_str() );
	dprintf(D_FULLDEBUG,"Leaving compute_significant_attrs() - result=%s\n",
					result ? result : "(none)" );
	return result;
}


bool Matchmaker::
getGroupInfoFromUserId(const char* user, std::string& groupName, double& groupQuota, double& groupUsage)
{
	ASSERT(groupQuotasHash);

    groupName = "";
	groupQuota = 0.0;
	groupUsage = 0.0;

	if (!user) return false;

    GroupEntry* group = GroupEntry::GetAssignedGroup(hgq_root_group, user);

    // if group quotas not in effect, return here for backward compatability
    if (hgq_groups.size() <= 1) return false;

    groupName = group->name;

	if (groupQuotasHash->lookup(groupName, groupQuota) == -1) {
		// hash lookup failed, must not be a group name
		return false;
	}

	groupUsage = accountant.GetWeightedResourcesUsed(groupName);

	return true;
}

double starvation_ratio(double usage, double allocated) {
    return (allocated > 0) ? (usage / allocated) : FLT_MAX;
}

int count_effective_slots(std::vector<ClassAd *>& startdAds, ExprTree* constraint) {
	int sum = 0;

	for (ClassAd *ad: startdAds) {
        // only count ads satisfying constraint, if given
        if ((nullptr != constraint) && !EvalExprBool(ad, constraint)) {
            continue;
        }

        bool part = false;
        if (!ad->LookupBool(ATTR_SLOT_PARTITIONABLE, part)) part = false;

        int slots = 1;
        if (part) {
            // effective slots for a partitionable slot is number of cpus
            ad->LookupInteger(ATTR_CPUS, slots);
        }

        sum += slots;
	}

	return sum;
}


static int 
CountMatches(const std::vector<ClassAd *> &ads, classad::ExprTree* constraint) {
	// Check for null constraint.
	if (constraint == nullptr) {
		return 0;
	}

	int matchCount  = 0;
	for (ClassAd *ad: ads) {
		matchCount += EvalExprBool(ad, constraint);
	}
	return matchCount;
}

void
Matchmaker::negotiationTime( int /* timerID */ )
{
	ClassAdList_DeleteAdsAndMatchList allAds(this); //contains ads from collector
	std::vector<ClassAd *> startdAds; // ptrs to startd ads in allAds
    ClaimIdHash claimIds;
	std::set<std::string> accountingNames; // set of active submitter names to publish
	std::vector<ClassAd*> submitterAds; // ptrs to submitter ads in allAds

	ranksMap.clear();
	m_slotNameToAdMap.clear();

	/**
		Check if we just finished a cycle less than NEGOTIATOR_CYCLE_DELAY
		seconds ago.  If we did, reset our timer so at least
		NEGOTIATOR_CYCLE_DELAY seconds will elapse between cycles.  We do
		this to help ensure all the startds have had time to update the
		collector after the last negotiation cycle (otherwise, we might match
		the same resource twice).  Note: we must do this check _before_ we
		reset GotRescheduledCmd to false to prevent postponing a new
		cycle indefinitely.
	**/
	time_t elapsed = time(NULL) - completedLastCycleTime;
	time_t cycle_delay = param_integer("NEGOTIATOR_CYCLE_DELAY",20,0);
	if ( elapsed < cycle_delay ) {
		daemonCore->Reset_Timer(negotiation_timerID,
							cycle_delay - elapsed,
							NegotiatorInterval);
		dprintf(D_FULLDEBUG,
			"New cycle requested but just finished one -- delaying %lld secs\n",
			(long long)cycle_delay - elapsed);
		return;
	}

	elapsed = time(NULL) - startedLastCycleTime;
	if ( elapsed < NegotiatorMinInterval ) {
		daemonCore->Reset_Timer(negotiation_timerID,
							NegotiatorMinInterval - elapsed,
							NegotiatorInterval);
		dprintf(D_FULLDEBUG,
			"New cycle requested but last one started too recently -- delaying %lld secs\n",
			(long long)NegotiatorMinInterval - elapsed);
		return;
	}

    if (param_boolean("NEGOTIATOR_READ_CONFIG_BEFORE_CYCLE", false)) {
        // All things being equal, it would be preferable to invoke a full neg reconfig here
        // instead of just config(), however frequent reconfigs apparently create new nonblocking
        // sockets to the collector that the collector waits in vain for, which ties it up, thus
        // also blocking other daemons trying to talk to the collector, and so forth.  That seems
        // like it should be fixed as well.
        dprintf(D_ALWAYS, "Re-reading config.\n");
        config();
    }

	dprintf( D_ALWAYS, "---------- Started Negotiation Cycle ----------\n" );

	time_t start_time = time(NULL);

	GotRescheduleCmd=false;  // Reset the reschedule cmd flag

	// We need to nuke our MatchList from the previous negotiation cycle,
	// since a different set of machines may now be available.
	if (MatchList) delete MatchList;
	MatchList = NULL;

	ScheddsTimeInCycle.clear();

	// ----- Get all required ads from the collector
    time_t start_time_phase1 = time(NULL);
	double start_usage_phase1 = get_rusage_utime();
	dprintf( D_ALWAYS, "Phase 1:  Obtaining ads from collector ...\n" );
	if( !obtainAdsFromCollector( allAds, startdAds, submitterAds, accountingNames,
		claimIds ) )
	{
		dprintf( D_ALWAYS, "Aborting negotiation cycle\n" );
		// should send email here
		return;
	}

    // From here we are committed to the main negotiator cycle, which is non
    // reentrant wrt reconfig. Set any reconfig to delay until end of this cycle
    // to protect HGQ structures and also to prevent blocking of other commands
    daemonCore->SetDelayReconfig(true);

		// allocate stat object here, now that we know we are not going
		// to abort the cycle
	StartNewNegotiationCycleStat();
	negotiation_cycle_stats[0]->start_time = start_time;

	// Save this for future use.
	int cTotalSlots = startdAds.size();
    negotiation_cycle_stats[0]->total_slots = cTotalSlots;

	double minSlotWeight = 0;
	double untrimmedSlotWeightTotal = sumSlotWeights(startdAds,&minSlotWeight,NULL);
	
	// Register a lookup function that passes through the list of all ads.
	// ClassAdLookupRegister( lookup_global, &allAds );

	dprintf( D_ALWAYS, "Phase 2:  Performing accounting ...\n" );
	// Compute the significant attributes to pass to the schedd, so
	// the schedd can do autoclustering to speed up the negotiation cycles.

    // Transition Phase 1 --> Phase 2
    time_t start_time_phase2 = time(NULL);
	double start_usage_phase2 = get_rusage_utime();
    negotiation_cycle_stats[0]->duration_phase1 += start_time_phase2 - start_time_phase1;
	negotiation_cycle_stats[0]->phase1_cpu_time += start_usage_phase2 - start_usage_phase1;

	if ( job_attr_references ) {
		free(job_attr_references);
	}
	job_attr_references = compute_significant_attrs(startdAds);

	// ----- Recalculate priorities for schedds
	accountant.UpdatePriorities();
	accountant.CheckMatches( startdAds );

	if ( !groupQuotasHash ) {
		groupQuotasHash = new groupQuotasHashType(hashFunction);
		ASSERT(groupQuotasHash);
    }

	int cPoolsize = 0;
    double weightedPoolsize = 0;
    int effectivePoolsize = 0;
    // Restrict number of slots available for determining quotas
    if (SlotPoolsizeConstraint != nullptr) {
        cPoolsize = CountMatches(startdAds, SlotPoolsizeConstraint);
        if (cPoolsize > 0) {
            dprintf(D_ALWAYS,"NEGOTIATOR_SLOT_POOLSIZE_CONSTRAINT constraint reduces slot count from %d to %d\n", cTotalSlots, cPoolsize);
            weightedPoolsize = (accountant.UsingWeightedSlots()) ? sumSlotWeights(startdAds, NULL, SlotPoolsizeConstraint) : cPoolsize;
            effectivePoolsize = count_effective_slots(startdAds, SlotPoolsizeConstraint);
        } else {
            dprintf(D_ALWAYS, "WARNING: 0 out of %d slots match NEGOTIATOR_SLOT_POOLSIZE_CONSTRAINT\n", cTotalSlots);
        }
    } else {
        cPoolsize = cTotalSlots;
        weightedPoolsize = (accountant.UsingWeightedSlots()) ? untrimmedSlotWeightTotal : (double)cTotalSlots;
        effectivePoolsize = count_effective_slots(startdAds, NULL);
    }

	// Trim out ads that we should not bother considering
	// during matchmaking now.  (e.g. when NEGOTIATOR_CONSIDER_PREEMPTION=False)
	// note: we cannot trim out the Unclaimed ads before we call CheckMatches,
	// otherwise CheckMatches will do the wrong thing (because it will not see
	// any of the claimed machines!).

	trimStartdAds(startdAds);

	if (m_staticRanks) {
		dprintf(D_FULLDEBUG, "About to sort machine ads by rank\n");
		struct rankSorter sorter;
		sorter.mm = this;
		std::sort(startdAds.begin(), startdAds.end(), sorter);
		dprintf(D_FULLDEBUG, "Done sorting machine ads by rank\n");
	}

    negotiation_cycle_stats[0]->trimmed_slots = startdAds.size();
    negotiation_cycle_stats[0]->candidate_slots = startdAds.size();

		// We insert NegotiatorMatchExprXXX attributes into the
		// "matched ad".  In the negotiator, this means the machine ad.
		// The schedd will later propogate these attributes into the
		// matched job ad that is sent to the startd.  So in different
		// matching contexts, the negotiator match exprs are in different
		// ads, but they should always be in at least one.
	insertNegotiatorMatchExprs( startdAds );

	// insert RemoteUserPrio and related attributes so they are
	// available during matchmaking
	addRemoteUserPrios( startdAds );

	SetupMatchSecurity(submitterAds);

    if (hgq_groups.size() <= 1) {
        // If there is only one group (the root group) we are in traditional non-HGQ mode.
        // It seems cleanest to take the traditional case separately for maximum backward-compatible behavior.
        // A possible future change would be to unify this into the HGQ code-path, as a "root-group-only" case.

		// If there are any submitters who have a floor defined, and their current usage is below
		// their floor, negotiator for just those, and only up to their floor.
		std::vector<ClassAd *> submittersBelowFloor;
		findBelowFloorSubmitters(submitterAds,submittersBelowFloor);
		if (submittersBelowFloor.size() > 0) {
			dprintf(D_FULLDEBUG, "   %zu submitters have a floor defined and are below it, running a floor round for them\n",
					submittersBelowFloor.size());
			negotiateWithGroup(true /*isFloorRound*/, cPoolsize, weightedPoolsize, minSlotWeight, startdAds, claimIds, submittersBelowFloor);
			dprintf(D_FULLDEBUG, "   Floor round finished, commencing with full negotiator round\n");
		}

        negotiateWithGroup(false /*isFloorRound*/, cPoolsize, weightedPoolsize, minSlotWeight, startdAds, claimIds, submitterAds);
    } else {
        // Otherwise we are in HGQ mode, so begin HGQ computations

        negotiation_cycle_stats[0]->candidate_slots = cPoolsize;

        // assign slot quotas based on the config-quotas
        double hgq_total_quota = (accountant.UsingWeightedSlots()) ? weightedPoolsize : effectivePoolsize;
        dprintf(D_ALWAYS, "group quotas: assigning group quotas from %g available%s slots\n",
                hgq_total_quota,
                (accountant.UsingWeightedSlots()) ? " weighted" : "");

		GroupEntry::hgq_prepare_for_matchmaking(hgq_total_quota, hgq_root_group, hgq_groups, accountant, submitterAds);

		auto callback = [&](GroupEntry *g, int slots) -> void {
				const char *name = g->name.c_str();
				if (autoregroup && (g == hgq_root_group)) {
					name = nullptr;
				}

				std::vector<ClassAd*> submittersBelowFloor;
				findBelowFloorSubmitters(*(g->submitterAds), submittersBelowFloor);
				if (submittersBelowFloor.size() > 0) {
					dprintf(D_FULLDEBUG, "   %zu submitters have a floor defined and are below it, running a floor round for them\n",
							submittersBelowFloor.size());
					negotiateWithGroup(true /*isFloorRound*/, cPoolsize, 
							weightedPoolsize, 
							minSlotWeight, 
							startdAds, 
							claimIds, 
							submittersBelowFloor,
							slots,
							name);

				}
				dprintf(D_FULLDEBUG, "   Floor round finished, commencing with full negotiator round\n");
				negotiateWithGroup(false, cPoolsize,
								weightedPoolsize,
								minSlotWeight,
								startdAds,
								claimIds,
								*(g->submitterAds),
								slots,
								name);
		};

		GroupEntry::hgq_negotiate_with_all_groups(
						hgq_root_group, 
						hgq_groups,
						groupQuotasHash,
						hgq_total_quota,
						accountant,
						callback,
						accept_surplus);

    }

    // Leave this in as an easter egg for dev/testing purposes.
    // Like NEG_SLEEP, but this one is not dependent on getting into the
    // negotiation loops to take effect.
    int insert_duration = param_integer("INSERT_NEGOTIATOR_CYCLE_TEST_DURATION", 0);
    if (insert_duration > 0) {
        dprintf(D_ALWAYS, "begin sleep: %d seconds\n", insert_duration);
        sleep(insert_duration);
        dprintf(D_ALWAYS, "end sleep: %d seconds\n", insert_duration);
    }

    // ----- Done with the negotiation cycle
    dprintf( D_ALWAYS, "---------- Finished Negotiation Cycle ----------\n" );

	startedLastCycleTime = start_time;
    completedLastCycleTime = time(NULL);

    negotiation_cycle_stats[0]->end_time = completedLastCycleTime;

    // Phase 2 is time to do "all of the above" since end of phase 1, less the time we spent in phase 3 and phase 4
    // (phase 3 and 4 occur inside of negotiateWithGroup(), which may be called in multiple places, inside looping)
    negotiation_cycle_stats[0]->duration_phase2 = completedLastCycleTime - start_time_phase2;
    negotiation_cycle_stats[0]->duration_phase2 -= negotiation_cycle_stats[0]->duration_phase3;
    negotiation_cycle_stats[0]->duration_phase2 -= negotiation_cycle_stats[0]->duration_phase4;

    negotiation_cycle_stats[0]->duration = completedLastCycleTime - negotiation_cycle_stats[0]->start_time;

	double end_cycle_usage = get_rusage_utime();
	negotiation_cycle_stats[0]->phase2_cpu_time = end_cycle_usage - start_usage_phase2;
	negotiation_cycle_stats[0]->phase2_cpu_time -= negotiation_cycle_stats[0]->phase3_cpu_time;
	negotiation_cycle_stats[0]->phase2_cpu_time -= negotiation_cycle_stats[0]->phase4_cpu_time;
	negotiation_cycle_stats[0]->cpu_time = end_cycle_usage - start_usage_phase1;

    // if we got any reconfig requests during the cycle it is safe to service them now:
    if (daemonCore->GetNeedReconfig()) {
        daemonCore->SetNeedReconfig(false);
        dprintf(D_FULLDEBUG,"Running delayed reconfig\n");
        dc_reconfig();
    }
    daemonCore->SetDelayReconfig(false);

	if (param_boolean("NEGOTIATOR_UPDATE_AFTER_CYCLE", false)) {
		updateCollector();
	}

	if (param_boolean("NEGOTIATOR_ADVERTISE_ACCOUNTING", true)) {
		forwardAccountingData(accountingNames);
	}

    // reduce negotiator delay drift
    daemonCore->Reset_Timer(negotiation_timerID,
                            std::max(cycle_delay,  NegotiatorInterval - negotiation_cycle_stats[0]->duration),
                            NegotiatorInterval);
}

// Make an accounting ad per active submitter, and send them
// to the collector.
void
Matchmaker::forwardAccountingData(std::set<std::string> &names) {
		std::set<std::string>::iterator it;
		
		if (nullptr == daemonCore->getCollectorList()) {
			dprintf(D_ALWAYS, "Not updating collector with accounting information, as no collectors are found\n");
			return;
		}
	
		dprintf(D_FULLDEBUG, "Updating collector with accounting information\n");
			// for all of the names of active submitters
		for (it = names.begin(); it != names.end(); it++) {
			std::string name = *it;
			std::string key("Customer.");  // hashkey is "Customer" followed by name
			key += name;

			ClassAd *accountingAd = accountant.GetClassAd(key);
			if (accountingAd) {

				ClassAd updateAd(*accountingAd); // copy all fields from Accountant Ad


				updateAd.Assign(ATTR_NAME, name); // the hash key
				updateAd.Assign(ATTR_NEGOTIATOR_NAME, NegotiatorName);
				updateAd.Assign("Priority", accountant.GetPriority(name));
				updateAd.Assign("Ceiling", accountant.GetCeiling(name));
				updateAd.Assign("Floor", accountant.GetFloor(name));

				bool isGroup;

				GroupEntry *ge = GroupEntry::GetAssignedGroup(hgq_root_group, name, isGroup);
				std::string groupName(ge->name);

				updateAd.Assign("IsAccountingGroup", isGroup);
				updateAd.Assign("AccountingGroup", groupName);

				updateAd.Assign(ATTR_LAST_UPDATE, accountant.GetLastUpdateTime());

				SetMyTypeName(updateAd, ACCOUNTING_ADTYPE);

				DCCollectorAdSequences seq; // Don't need them, interface requires them
				int resUsed = -1;

				// If flocking submitters aren't running here, ResourcesUsed
				// will be zero.  Don't include those submitters.

				if (updateAd.LookupInteger("ResourcesUsed", resUsed)) {
					daemonCore->sendUpdates(UPDATE_ACCOUNTING_AD, &updateAd, NULL, false);
				}
			}
		}
		forwardGroupAccounting(hgq_root_group);
		dprintf(D_FULLDEBUG, "Done Updating collector with accounting information\n");
}

void
Matchmaker::forwardGroupAccounting(GroupEntry* group) {

	ClassAd accountingAd;
	SetMyTypeName(accountingAd, ACCOUNTING_ADTYPE);
	accountingAd.Assign(ATTR_LAST_UPDATE, accountant.GetLastUpdateTime());
	accountingAd.Assign(ATTR_NEGOTIATOR_NAME, NegotiatorName);


	std::string CustomerName = group->name;

	ClassAd *CustomerAd = accountant.GetClassAd(std::string("Customer.") + CustomerName);

    if (CustomerAd == NULL) {
        dprintf(D_ALWAYS, "WARNING: Expected AcctLog entry \"%s\" to exist.\n", CustomerName.c_str());
        return;
    }

    bool isGroup=false;
    GroupEntry* cgrp = GroupEntry::GetAssignedGroup(hgq_root_group, CustomerName, isGroup);

	if (!cgrp) {
        return;
	}

    std::string cgname;
    if (isGroup) {
        cgname = (cgrp->parent != NULL) ? cgrp->parent->name : cgrp->name;
    } else {
        dprintf(D_ALWAYS, "WARNING: Expected \"%s\" to be a defined group in the accountant", CustomerName.c_str());
        return;
    }

    accountingAd.Assign(ATTR_NAME, CustomerName);
    accountingAd.Assign("IsAccountingGroup", isGroup);
    accountingAd.Assign("AccountingGroup", cgname);

    double Priority = accountant.GetPriority(CustomerName);
    accountingAd.Assign("Priority", Priority);

    double PriorityFactor = 0;
    if (CustomerAd->LookupFloat("PriorityFactor",PriorityFactor)==0) {
		PriorityFactor=0;
	}

    accountingAd.Assign("PriorityFactor", PriorityFactor);

    if (cgrp) {
        accountingAd.Assign("EffectiveQuota", cgrp->quota);
        accountingAd.Assign("ConfigQuota", cgrp->config_quota);
        accountingAd.Assign("SubtreeQuota", cgrp->subtree_quota);
        accountingAd.Assign("GroupSortKey", cgrp->sort_key);

        const char * policy = "no";
        if (cgrp->autoregroup) policy = "regroup";
        else if (cgrp->accept_surplus) policy = "byquota";
        accountingAd.Assign("SurplusPolicy", policy);

        accountingAd.Assign("Requested", cgrp->currently_requested);
    }

    int ResourcesUsed = 0;
    if (CustomerAd->LookupInteger("ResourcesUsed", ResourcesUsed)==0) ResourcesUsed=0;
    accountingAd.Assign("ResourcesUsed", ResourcesUsed);

    double WeightedResourcesUsed = 0;
    if (CustomerAd->LookupFloat("WeightedResourcesUsed",WeightedResourcesUsed)==0) WeightedResourcesUsed=0;
    accountingAd.Assign("WeightedResourcesUsed", WeightedResourcesUsed);

    double AccumulatedUsage = 0;
    if (CustomerAd->LookupFloat("AccumulatedUsage",AccumulatedUsage)==0) AccumulatedUsage=0;
    accountingAd.Assign("AccumulatedUsage", AccumulatedUsage);

    double WeightedAccumulatedUsage = 0;
    if (CustomerAd->LookupFloat("WeightedAccumulatedUsage",WeightedAccumulatedUsage)==0) WeightedAccumulatedUsage=0;
    accountingAd.Assign("WeightedAccumulatedUsage", WeightedAccumulatedUsage);

    int BeginUsageTime = 0;
    if (CustomerAd->LookupInteger("BeginUsageTime",BeginUsageTime)==0) BeginUsageTime=0;
    accountingAd.Assign("BeginUsageTime", BeginUsageTime);

    int LastUsageTime = 0;
    if (CustomerAd->LookupInteger("LastUsageTime",LastUsageTime)==0) LastUsageTime=0;
    accountingAd.Assign("LastUsageTime", LastUsageTime);

	// And send the ad to the collector
	DCCollectorAdSequences seq; // Don't need them, interface requires them
	daemonCore->sendUpdates(UPDATE_ACCOUNTING_AD, &accountingAd, NULL, false);

    // Populate group's children recursively, if it has any
    for (std::vector<GroupEntry*>::iterator j(group->children.begin());  j != group->children.end();  ++j) {
        forwardGroupAccounting(*j);
    }
}

void filter_submitters_no_idle(std::vector<ClassAd *>& submitterAds) {
	auto isIdle = [](ClassAd *ad) -> bool {
        int idle = 0;
        ad->LookupInteger(ATTR_IDLE_JOBS, idle);

        if (idle <= 0) {
            std::string submitterName;
            ad->LookupString(ATTR_NAME, submitterName);
            dprintf(D_FULLDEBUG, "Ignoring submitter %s with no idle jobs\n", submitterName.c_str());
			return true;
        }
		return false;
    };
	auto it = std::remove_if(submitterAds.begin(), submitterAds.end(), isIdle);
	submitterAds.erase(it, submitterAds.end());
}

/*
 consolidate_globaljobprio_submitter_ads()
 Scan through submitterAds looking for globaljobprio submitter ads, consolidating
 them into a minimal set of submitter ads that contain JOBPRIO_MIN and
 JOBPRIO_MAX attributes to reflect job priority ranges.
 Return true on success and/or want_globaljobprio should be true,
 false if there is a data structure inconsistency and/or want_globaljobprio should be false.
*/
bool Matchmaker::
consolidate_globaljobprio_submitter_ads(std::vector<ClassAd *>& submitterAds) const
{
	// nothing to do if unless want_globaljobprio is true...
	if (!want_globaljobprio) {
		return false;  // keep want_globajobprio false
	}

	ClassAd *prev_ad = NULL;
	std::string curr_name, curr_addr, prev_name, prev_addr;
	int min_prio=INT_MAX, max_prio=INT_MIN; // initialize to shut gcc up, the loop always sets before using.

	auto it = submitterAds.begin();
	while (it != submitterAds.end()) {
		ClassAd *curr_ad = *it;

		// skip this submitter if we cannot identify its origin
		if (!curr_ad->LookupString(ATTR_NAME,curr_name)) {
			it++;
			continue;
		}
		if (!curr_ad->LookupString(ATTR_SCHEDD_IP_ADDR,curr_addr)) {
			it++;
			continue;
		}

		// In obtainAdsFromCollector() inserted an ATTR_JOB_PRIO attribute; if
		// it is not there, then the value of want_globaljobprio must have changed
		// or something. In any event, if we cannot find what we need, don't honor
		// the request for USE_GLOBAL_JOB_PRIOS for this negotiation cycle.
		int curr_prio=0;
		if (!curr_ad->LookupInteger(ATTR_JOB_PRIO,curr_prio)) {
			dprintf(D_ALWAYS,
				"WARNING: internal inconsistancy, ignoring USE_GLOBAL_JOB_PRIOS=True until next reconfig\n");
			return false;
		}

		// If this ad has no ATTR_JOB_PRIO_ARRAY, then we don't want to assign
		// any JOBPRIO_MIN or MAX, as this must be a schedd that does not (or cannot)
		// play the global job prios game.  So just continue along.
		if ( !curr_ad->Lookup(ATTR_JOB_PRIO_ARRAY)) {
			it++;
			continue;
		}

		// If this ad is not from the same user and schedd previously
		// seen, insert JOBPRIO_MIX and MAX attributes, update our notion
		// of "previously seen", and continue along.
		if ( curr_name != prev_name || curr_addr != prev_addr ) {
			curr_ad->Assign("JOBPRIO_MIN",curr_prio);
			curr_ad->Assign("JOBPRIO_MAX",curr_prio);
			prev_ad = curr_ad;
			prev_name = curr_name;
			prev_addr = curr_addr;
			max_prio = min_prio = curr_prio;
			it++;
			continue;
		}

		// Some sanity assertions here.
		ASSERT(prev_ad);
		ASSERT(curr_ad);

		// Here is the meat: consolidate this submitter ad into the
		// previous one, if we can...
		// update the previous ad to negotiate for this priority as well
		if (curr_prio < min_prio) {
			prev_ad->Assign("JOBPRIO_MIN",curr_prio);
			min_prio = curr_prio;
		}
		if (curr_prio > max_prio) {
			prev_ad->Assign("JOBPRIO_MAX",curr_prio);
			max_prio = curr_prio;
		}
		// and now may as well delete the curr_ad, since negotiation will
		// be handled by the first ad for this user/schedd_addr
		it = submitterAds.erase(it);
	}	// end of while iterate through submitterAds

	return true;
}

int Matchmaker::
negotiateWithGroup ( bool isFloorRound,
					 int untrimmed_num_startds,
					 double untrimmedSlotWeightTotal,
					 double minSlotWeight,
					 std::vector<ClassAd *>& startdAds,
					 ClaimIdHash& claimIds,
					 std::vector<ClassAd *>& submitterAds,
					 double groupQuota, const char* groupName)
{
	std::string    submitterName;
	std::string    scheddName;
	std::string    scheddAddr;
	int			result;
	int			numStartdAds;
	double      slotWeightTotal;
	double		maxPrioValue;
	double		maxAbsPrioValue;
	double		normalFactor;
	double		normalAbsFactor;
	double		submitterPrio;
	double		submitterPrioFactor;
	double		submitterShare = 0.0;
	double		submitterAbsShare = 0.0;
	double		pieLeft;
	double 		pieLeftOrig;
	size_t		submitterAdsCountOrig;
	int			totalTime;
	int			totalTimeSchedd;
	int			num_idle_jobs;

    time_t duration_phase3 = 0;
    time_t start_time_phase4 = time(NULL);
	double phase3_cpu_time = 0.0;
	double start_usage_phase4 = get_rusage_utime();
	time_t start_time_prefetch = 0;
	double start_usage_prefetch = 0.0;

	negotiation_cycle_stats[0]->pies++;

	double scheddUsed=0;
	int spin_pie=0;
	do {
		spin_pie++;
		negotiation_cycle_stats[0]->pie_spins++;

        // On the first spin of the pie we tell the negotiate function to ignore the
        // submitterLimit w/ respect to jobs which are strictly preferred by resource
        // offers (via startd rank).  However, if preemption is not being considered,
        // we respect submitter limits on all iterations.
        const bool ignore_submitter_limit = ((spin_pie == 1) && ConsiderPreemption);

        double groupusage = (NULL != groupName) ? accountant.GetWeightedResourcesUsed(groupName) : 0.0;
        if (!ignore_submitter_limit && (NULL != groupName) && (groupusage >= groupQuota)) {
            // If we've met the group quota, and if we are paying attention to submitter limits, halt now
            dprintf(D_ALWAYS, "Group %s is using its quota %g - halting negotiation\n", groupName, groupQuota);
            break;
        }
			// invalidate the MatchList cache, because even if it is valid
			// for the next user+auto_cluster being considered, we might
			// have thrown out matches due to SlotWeight being too high
			// given the schedd limit computed in the previous pie spin
		DeleteMatchList();

        // filter submitters with no idle jobs to avoid unneeded computations and log output
        if (!ConsiderPreemption) {
            filter_submitters_no_idle(submitterAds);
        }

		calculateNormalizationFactor( submitterAds, maxPrioValue, normalFactor,
									  maxAbsPrioValue, normalAbsFactor);
		numStartdAds = untrimmed_num_startds;
			// If operating on a group with a quota, consider the size of
			// the "pie" to be limited to the groupQuota, so each user in
			// the group gets a reasonable sized slice.
		slotWeightTotal = untrimmedSlotWeightTotal;
		if ( slotWeightTotal > groupQuota ) {
			slotWeightTotal = groupQuota;
		}

		calculatePieLeft(
			submitterAds,
			groupName,
			groupQuota,
			groupusage,
			maxPrioValue,
			maxAbsPrioValue,
			normalFactor,
			normalAbsFactor,
			slotWeightTotal,
				/* result parameters: */
			pieLeft);

		if (!ConsiderPreemption && (pieLeft <= 0)) {
			dprintf(D_ALWAYS,
				"Halting negotiation: no slots available to match (preemption disabled,%zu trimmed slots,pieLeft=%.3f)\n",
				startdAds.size(),pieLeft);
			break;
		}

        if (1 == spin_pie) {
            // Sort the schedd list in decreasing priority order
            // This only needs to be done once: do it on the 1st spin, prior to
            // iterating over submitter ads so they negotiate in sorted order.
            // The sort ordering function makes use of a submitter starvation
            // attribute that is computed in calculatePieLeft, above.
			// The sort order function also makes use of job priority information
			// if want_globaljobprio is true.
            time_t start_time_phase3 = time(NULL);
			double start_usage_phase3 = get_rusage_utime();
            dprintf(D_ALWAYS, "Phase 3:  Sorting submitter ads by priority ...\n");

			std::sort(submitterAds.begin(), submitterAds.end(), submitterLessThan(this));

			// Now that the submitter ad list (submitterAds) is sorted, we can
			// scan through it looking for globaljobprio submitter ads, consolidating
			// them into a minimal set of submitter ads that contain JOBPRIO_MIN and
			// JOBPRIO_MAX attributes to reflect job priority ranges.
			want_globaljobprio = consolidate_globaljobprio_submitter_ads(submitterAds);

            duration_phase3 += time(nullptr) - start_time_phase3;
			phase3_cpu_time += get_rusage_utime() - start_usage_phase3;
        }

		start_time_prefetch = time(NULL);
		start_usage_prefetch = get_rusage_utime();

		prefetchResourceRequestLists(submitterAds);

		negotiation_cycle_stats[0]->prefetch_duration = time(NULL) - start_time_prefetch;
		negotiation_cycle_stats[0]->prefetch_cpu_time += get_rusage_utime() - start_usage_prefetch;

		pieLeftOrig = pieLeft;
		submitterAdsCountOrig = submitterAds.size();

		// ----- Negotiate with the schedds in the sorted list
		dprintf( D_ALWAYS, "Phase 4.%d:  Negotiating with schedds ...\n",
			spin_pie );
		dprintf (D_FULLDEBUG, "    numSlots = %d (after trimming=%zu)\n", numStartdAds,startdAds.size());
		dprintf (D_FULLDEBUG, "    slotWeightTotal = %f\n", slotWeightTotal);
		dprintf (D_FULLDEBUG, "    minSlotWeight = %f\n", minSlotWeight);
		dprintf (D_FULLDEBUG, "    pieLeft = %.3f\n", pieLeft);
		dprintf (D_FULLDEBUG, "    NormalFactor = %f\n", normalFactor);
		dprintf (D_FULLDEBUG, "    MaxPrioValue = %f\n", maxPrioValue);
		dprintf (D_FULLDEBUG, "    NumSubmitterAds = %zu\n", submitterAds.size());

		auto submitterIter = submitterAds.begin();
		while (submitterIter != submitterAds.end()) {
			ClassAd		*submitter_ad = *submitterIter;
			bool removeMe = false;
			if (!ignore_submitter_limit && (NULL != groupName) && (accountant.GetWeightedResourcesUsed(groupName) >= groupQuota)) {
				// If we met group quota, and if we're respecting submitter limits, halt.
				// (output message at top of outer loop above)
				break;
			}
			// get the name of the submitter and address of the schedd-daemon it came from
			if( !submitter_ad->LookupString( ATTR_NAME, submitterName ) ||
				!submitter_ad->LookupString( ATTR_SCHEDD_NAME, scheddName ) ||
				!submitter_ad->LookupString( ATTR_SCHEDD_IP_ADDR, scheddAddr ) )
			{
				dprintf (D_ALWAYS,"  Error!  Could not get %s, %s and %s from ad\n",
						 ATTR_NAME, ATTR_SCHEDD_NAME, ATTR_SCHEDD_IP_ADDR);
				dprintf( D_ALWAYS, "  Ignoring this submitter and continuing\n" );
				submitterIter = submitterAds.erase(submitterIter);
				continue;
			}

			num_idle_jobs = 0;
			submitter_ad->LookupInteger(ATTR_IDLE_JOBS,num_idle_jobs);
			if ( num_idle_jobs < 0 ) {
				num_idle_jobs = 0;
			}

			totalTime = 0;
			submitter_ad->LookupInteger(ATTR_TOTAL_TIME_IN_CYCLE,totalTime);
			if ( totalTime < 0 ) {
				totalTime = 0;
			}

			totalTimeSchedd = ScheddsTimeInCycle[scheddAddr.c_str()];

			if (( num_idle_jobs > 0 ) && (totalTime < MaxTimePerSubmitter) &&
				(totalTimeSchedd < MaxTimePerSchedd)) {
				dprintf(D_ALWAYS,"  Negotiating with %s at %s\n",
					submitterName.c_str(), scheddAddr.c_str());
				dprintf(D_ALWAYS, "%d seconds so far for this submitter\n", totalTime);
				dprintf(D_ALWAYS, "%d seconds so far for this schedd\n", totalTimeSchedd);
			}

			double submitterLimit = 0.0;
            double submitterLimitUnclaimed = 0.0;
			double submitterUsage = 0.0;

			calculateSubmitterLimit(
				submitterName,
				groupName,
				groupQuota,
				groupusage,
				maxPrioValue,
				maxAbsPrioValue,
				normalFactor,
				normalAbsFactor,
				slotWeightTotal,
				isFloorRound,
					/* result parameters: */
				submitterLimit,
                submitterLimitUnclaimed,
				submitterUsage,
				submitterShare,
				submitterAbsShare,
				submitterPrio,
				submitterPrioFactor);

				if (spin_pie == 1) {
					std::string key("Customer.");  // hashkey is "Customer" followed by name
					key += submitterName;

					// Save away the submitter share on the first pie spin to put in
					// the accounting ad to publish to the AccountingAd.
					ClassAd *accountingAd = accountant.GetClassAd(key);
					if (accountingAd) {
						accountingAd->Assign("SubmitterShare", submitterShare);
						accountingAd->Assign("SubmitterLimit", submitterShare * slotWeightTotal);
					}
				}

			double submitterLimitStarved = 0;
			if( submitterLimit > pieLeft ) {
				// Somebody must have taken more than their fair share,
				// so this schedd gets starved.  This assumes that
				// none of the pie dished out so far was just shuffled
				// around between the users in the current group.
				// If that is not true, a subsequent spin of the pie
				// will dish out some more.
				submitterLimitStarved = submitterLimit - pieLeft;
				submitterLimit = pieLeft;
			}

			int submitterCeiling = accountant.GetCeiling(submitterName);
			// -1 means no limit
			if (submitterCeiling < 0) {
					submitterCeiling = INT_MAX;
			} else {
					submitterCeiling -= (int) accountant.GetWeightedResourcesUsed(submitterName);
			}

			// But now if we are below 0, we've used more than our current ceiling, set to 0
			if (submitterCeiling < 0) {
				submitterCeiling = 0;
			}
			// So by here, submitterCeiling is "ceiling left"  maybe, "headroom" 

			if ( num_idle_jobs > 0 ) {
				dprintf (D_FULLDEBUG, "  Calculating submitter limit with the "
					"following parameters\n");
				dprintf (D_FULLDEBUG, "    SubmitterPrio       = %f\n",
					submitterPrio);
				dprintf (D_FULLDEBUG, "    SubmitterPrioFactor = %f\n",
					 submitterPrioFactor);
				dprintf (D_FULLDEBUG, "    submitterShare      = %f\n",
					submitterShare);
				dprintf (D_FULLDEBUG, "    submitterAbsShare   = %f\n",
					submitterAbsShare);
				std::string starvation;
				if( submitterLimitStarved > 0 ) {
					formatstr(starvation, " (starved %f)",submitterLimitStarved);
				}
				dprintf (D_FULLDEBUG, "    submitterLimit    = %f%s\n",
					submitterLimit, starvation.c_str());
				dprintf (D_FULLDEBUG, "    submitterCeiling remaining   = %d\n",
					submitterCeiling);
				dprintf (D_FULLDEBUG, "    submitterUsage    = %f\n",
					submitterUsage);
			}

			// initialize reasons for match failure; do this now
			// in case we never actually call negotiate() below.
			rejForNetwork = 0;
			rejForNetworkShare = 0;
			rejForConcurrencyLimit = 0;
			rejPreemptForPrio = 0;
			rejPreemptForPolicy = 0;
			rejPreemptForRank = 0;
			rejForSubmitterLimit = 0;
			rejForSubmitterCeiling = 0;
			rejectedConcurrencyLimits.clear();

			// Optimizations:
			// If number of idle jobs = 0, don't waste time with negotiate.
			// Likewise, if limit is 0, don't waste time with negotiate EXCEPT
			// on the first spin of the pie (spin_pie==1), we must
			// still negotiate because on the first spin we tell the negotiate
			// function to ignore the submitterLimit w/ respect to jobs which
			// are strictly preferred by resource offers (via startd rank).
			// Also, don't bother negotiating if MaxTime(s) to negotiate exceeded.
			time_t startTime = time(NULL);
			time_t remainingTimeForThisCycle = MaxTimePerCycle -
						(startTime - negotiation_cycle_stats[0]->start_time);
			time_t remainingTimeForThisSubmitter = MaxTimePerSubmitter - totalTime;
			time_t remainingTimeForThisSchedd = MaxTimePerSchedd - totalTimeSchedd;
			if ( num_idle_jobs == 0 ) {
				dprintf(D_FULLDEBUG,
					"  Negotiating with %s skipped because no idle jobs\n",
					submitterName.c_str());
				result = MM_DONE;
			} else if (remainingTimeForThisSubmitter <= 0) {
				dprintf(D_ALWAYS,
					"  Negotiation with %s skipped because of time limits:\n",
					submitterName.c_str());
				dprintf(D_ALWAYS,
					"  %d seconds spent on this user, MAX_TIME_PER_USER is %d secs\n ",
					totalTime, MaxTimePerSubmitter);
				negotiation_cycle_stats[0]->submitters_out_of_time.insert(submitterName);
				result = MM_DONE;
			} else if (remainingTimeForThisSchedd <= 0) {
				dprintf(D_ALWAYS,
					"  Negotiation with %s skipped because of time limits:\n",
					submitterName.c_str());
				dprintf(D_ALWAYS,
					"  %d seconds spent on this schedd (%s), MAX_TIME_PER_SCHEDD is %d secs\n ",
					totalTimeSchedd, scheddName.c_str(), MaxTimePerSchedd);
				negotiation_cycle_stats[0]->schedds_out_of_time.insert(scheddName.c_str());
				result = MM_DONE;
			} else if (remainingTimeForThisCycle <= 0) {
				dprintf(D_ALWAYS,
					"  Negotiation with %s skipped because MAX_TIME_PER_CYCLE of %d secs exceeded\n",
					submitterName.c_str(),MaxTimePerCycle);
				result = MM_DONE;
			} else if ((submitterLimit < minSlotWeight || pieLeft < minSlotWeight) && (spin_pie > 1)) {
				dprintf(D_ALWAYS,
					"  Negotiation with %s skipped as pieLeft < minSlotWeight\n",
					submitterName.c_str());
				result = MM_RESUME;
			} else if ((submitterCeiling == 0) || (submitterCeiling < minSlotWeight)) {
				dprintf(D_ALWAYS,
					"  Negotiation with %s skipped because submitterCeiling remaining is %d\n",
					submitterName.c_str(), submitterCeiling);
				result = MM_DONE;
			} else {
				int numMatched = 0;
				time_t deadline = startTime +
					MIN(MaxTimePerSpin, MIN(remainingTimeForThisCycle, MIN(remainingTimeForThisSubmitter, remainingTimeForThisSchedd)));
                if (negotiation_cycle_stats[0]->active_submitters.count(submitterName) <= 0) {
                    negotiation_cycle_stats[0]->num_idle_jobs += num_idle_jobs;
                }
				negotiation_cycle_stats[0]->active_submitters.insert(submitterName);
				negotiation_cycle_stats[0]->active_schedds.insert(scheddAddr.c_str());
				result=negotiate(groupName, submitterName.c_str(), submitter_ad, submitterPrio,
                              submitterLimit, submitterLimitUnclaimed, submitterCeiling,
							  startdAds, claimIds,
							  ignore_submitter_limit,
							  deadline, numMatched, pieLeft);
				updateNegCycleEndTime(startTime, submitter_ad);
			}

			switch (result)
			{
				case MM_RESUME:
					// the schedd hit its resource limit.  must resume
					// negotiations in next spin
					scheddUsed += accountant.GetWeightedResourcesUsed(submitterName);
                    negotiation_cycle_stats[0]->submitters_share_limit.insert(submitterName.c_str());
					dprintf(D_FULLDEBUG, "  This submitter hit its submitterLimit.\n");
					break;
				case MM_DONE:
					if (rejForNetworkShare) {
							// We negotiated for all jobs, but some
							// jobs were rejected because this user
							// exceeded her fair-share of network
							// resources.  Resume negotiations for
							// this user in next spin.
					} else {
							// the schedd got all the resources it
							// wanted. delete this schedd ad.
						dprintf(D_FULLDEBUG,"  Submitter %s got all it wants; removing it.\n", submitterName.c_str());
                        scheddUsed += accountant.GetWeightedResourcesUsed(submitterName);
                        dprintf( D_FULLDEBUG, " resources used by %s are %f\n",submitterName.c_str(),
                                 accountant.GetWeightedResourcesUsed(submitterName));
						removeMe = true;
					}
					break;
				case MM_ERROR:
				default:
					dprintf(D_ALWAYS,"  Error: Ignoring submitter for this cycle\n" );
					sockCache->invalidateSock( scheddAddr.c_str() );
	
					scheddUsed += accountant.GetWeightedResourcesUsed(submitterName);
					dprintf( D_FULLDEBUG, " resources used by %s are %f\n",submitterName.c_str(),
						    accountant.GetWeightedResourcesUsed(submitterName));
					negotiation_cycle_stats[0]->submitters_failed.insert(submitterName.c_str());
					removeMe = true;
			}
			if (removeMe) {
				submitterIter = submitterAds.erase(submitterIter);
			} else {
				submitterIter++;
			}
		}
		dprintf( D_FULLDEBUG, " resources used scheddUsed= %f\n",scheddUsed);

	} while ( ( pieLeft < pieLeftOrig || submitterAds.size() < submitterAdsCountOrig )
			  && (submitterAds.size() > 0)
			  && (startdAds.size() > 0)
		   	  && !isFloorRound);

	dprintf( D_ALWAYS, " negotiateWithGroup resources used submitterAds length %zu \n", submitterAds.size());

    negotiation_cycle_stats[0]->duration_phase3 += duration_phase3;
    negotiation_cycle_stats[0]->duration_phase4 += (time(NULL) - start_time_phase4) - duration_phase3;

    negotiation_cycle_stats[0]->phase3_cpu_time += phase3_cpu_time;
    negotiation_cycle_stats[0]->phase4_cpu_time += (get_rusage_utime() - start_usage_phase4) - phase3_cpu_time;

	return TRUE;
}


int Matchmaker::
trimStartdAds(std::vector<ClassAd *> &startdAds)
{
	/*
		Throw out startd ads have no business being
		visible to the matchmaking engine, but were fetched from the
		collector because perhaps the accountant needs to see them.
		This method is called after accounting completes, but before
		matchmaking begins.
	*/

	int removed = 0;

	removed += trimStartdAds_PreemptionLogic(startdAds);
	removed += trimStartdAds_ShutdownLogic(startdAds);
	removed += trimStartdAds_ClaimedPslotLogic(startdAds);

	return removed;
}

int Matchmaker::
trimStartdAds_ShutdownLogic(std::vector<ClassAd *> &startdAds)
{
	int threshold = 0;
	size_t removed = 0;
	const time_t now = time(nullptr);

	/*
		Trim out any startd ads that have a DaemonShutdown attribute that evaluates
		to True threshold seconds in the future.  The idea here is we don't want to
		match with startds that are real close to shutting down, since likely doing so
		will just be a waste of time.
	*/

	// Get our threshold from the config file; note that NEGOTIATOR_TRIM_SHUTDOWN_THRESHOLD
	// can be an int OR a classad expression that will get evaluated against the
	// negotiator ad.  This may be handy to express the threshold as a function of
	// the negotiator cycle time.
	param_integer("NEGOTIATOR_TRIM_SHUTDOWN_THRESHOLD",threshold,true,0,false,INT_MIN,INT_MAX,publicAd);

	// A threshold of 0 (or less) means don't trim anything, in which case we have no
	// work to do.
	if ( threshold <= 0 ) {
		// Nothing to do
		return removed;
	}

	// Predicate to decide if we should remove slot ad because
	// it is about to shutdown, so it would be unwise to send jobs
	// there.
	auto isShuttingdown = [now,threshold](ClassAd *ad) {
		bool shutdown = false;
		ExprTree *shutdown_expr = ad->Lookup(ATTR_DAEMON_SHUTDOWN);
		ExprTree *shutdownfast_expr = ad->Lookup(ATTR_DAEMON_SHUTDOWN_FAST);
		if (shutdown_expr || shutdownfast_expr ) {
			// Set CurrentTime to be threshold seconds into the
			// future.  Use ATTR_MY_CURRENT_TIME if it exists in
			// the ad to avoid issues due to clock skew between the
			// startd and the negotiator.
			time_t myCurrentTime = now;
			ad->LookupInteger(ATTR_MY_CURRENT_TIME,myCurrentTime);
			ExprTree *old_currtime = ad->Remove(ATTR_CURRENT_TIME);
			ad->Assign(ATTR_CURRENT_TIME,myCurrentTime + threshold); // change time

			// Now that CurrentTime is set into the future, evaluate
			// if the Shutdown expression(s)
			if (shutdown_expr) {
				ad->LookupBool(ATTR_DAEMON_SHUTDOWN, shutdown);
			}
			if (shutdownfast_expr) {
				ad->LookupBool(ATTR_DAEMON_SHUTDOWN_FAST, shutdown);
			}

			// Put CurrentTime back to how we found it, ie = time()
			if ( old_currtime ) {
				ad->Insert(ATTR_CURRENT_TIME, old_currtime);
			}
		}
		return shutdown;
	};

	auto lastGood = std::remove_if(startdAds.begin(), startdAds.end(), isShuttingdown);
	removed = std::distance(lastGood, startdAds.end());
	startdAds.erase(lastGood, startdAds.end());

	dprintf(D_FULLDEBUG,
				"Trimmed out %zu startd ads due to NEGOTIATOR_TRIM_SHUTDOWN_THRESHOLD=%d\n",
				removed,threshold);
	
	return removed;
}

int Matchmaker::
trimStartdAds_PreemptionLogic(std::vector<ClassAd *> &startdAds) const
{
	int removed = 0;

		// If we are not considering preemption, we can save time
		// (and also make the spinning pie algorithm more correct) by
		// getting rid of ads that are not in the Unclaimed state.
	
	if ( ConsiderPreemption ) {
		if( ConsiderEarlyPreemption ) {
				// we need to keep all the ads.
			return 0;
		}

			// Remove ads with retirement time, because we are not
			// considering early preemption
		auto startdit = startdAds.begin();
		while (startdit != startdAds.end()) {
			ClassAd *ad = *startdit;
			int retirement_remaining;
			if(ad->LookupInteger(ATTR_RETIREMENT_TIME_REMAINING, retirement_remaining) &&
			   retirement_remaining > 0 )
			{
				if( IsDebugLevel(D_FULLDEBUG) ) {
					std::string name,user;
					ad->LookupString(ATTR_NAME,name);
					ad->LookupString(ATTR_REMOTE_USER,user);
					dprintf(D_FULLDEBUG,"Trimming %s, because %s still has %ds of retirement time.\n",
							name.c_str(), user.c_str(), retirement_remaining);
				}
				startdit = startdAds.erase(startdit);
				removed++;
			} else {
				startdit++;
			}
		}

		if ( removed > 0 ) {
			dprintf(D_ALWAYS,
					"Trimmed out %d startd ads due to NEGOTIATOR_CONSIDER_EARLY_PREEMPTION=False\n",
					removed);
		}
		return removed;
	}

	const std::string claimed_state_str = state_to_string(claimed_state);
	const std::string preempting_state_str = state_to_string(preempting_state);

	auto isClaimedOrPreempting = [&claimed_state_str,&preempting_state_str](ClassAd *ad) {
		std::string curState;
		bool is_pslot = false;
		if (ad->LookupString(ATTR_STATE, curState)) {
			if ((curState == claimed_state_str) || (curState == preempting_state_str)) {
				// Ignore Claimed pslots, they aren't eligible for preemption
				ad->LookupBool(ATTR_SLOT_PARTITIONABLE, is_pslot);
				if (is_pslot) {
					return false;
				}
				return true;
			}
		}
		return false;
	};

	auto lastGood = std::remove_if(startdAds.begin(), startdAds.end(), isClaimedOrPreempting);
	removed = std::distance(lastGood, startdAds.end());
	startdAds.erase(lastGood, startdAds.end());

	dprintf(D_FULLDEBUG,
			"Trimmed out %d startd ads due to NEGOTIATOR_CONSIDER_PREEMPTION=False\n",
			removed);
	return removed;
}


int Matchmaker::
trimStartdAds_ClaimedPslotLogic(std::vector<ClassAd *> &startdAds)
{
	size_t removed = 0;
	std::string cur_state;
	const char* claimed_state_str = state_to_string(claimed_state);

	/*
	 * Trim out claimed partitionable slots when any of the following
	 * is true:
	 * 1) WantMatching is false in the ad
	 * 2) WorkingCM is set in the ad and MatchWorkingCmSlots is false
	*/

	auto isClaimedPslot = [&](ClassAd *ad) {
		bool is_pslot = false;
		ad->LookupBool(ATTR_SLOT_PARTITIONABLE, is_pslot);
		if (is_pslot) {
			ad->LookupString(ATTR_STATE, cur_state);
			if (strcmp(cur_state.c_str(), claimed_state_str) != 0) {
				return false;;
			}
			std::string working_cm;
			bool want_matching = true;
			ad->LookupString("WorkingCM", working_cm);
			ad->LookupBool(ATTR_WANT_MATCHING, want_matching);
			if (want_matching && (MatchWorkingCmSlots || working_cm.empty())) {
				return false;;
			}
			return true;
		}
		return false;
	};
	
	auto lastGood = std::remove_if(startdAds.begin(), startdAds.end(), isClaimedPslot);
	removed = std::distance(lastGood, startdAds.end());
	startdAds.erase(lastGood, startdAds.end());

	dprintf(D_FULLDEBUG,
	        "Trimmed out %zu startd ads that are claimed pslots that don't want to be matched by us\n",
	        removed);

	return removed;
}

double Matchmaker::
sumSlotWeights(std::vector<ClassAd *> &startdAds, double* minSlotWeight, ExprTree* constraint)
{
	double sum = 0.0;

	if( minSlotWeight ) {
		*minSlotWeight = DBL_MAX;
	}

	for (ClassAd *ad: startdAds) {
        // only count ads satisfying constraint, if given
        if ((nullptr != constraint) && !EvalExprBool(ad, constraint)) {
            continue;
        }

		double slotWeight = accountant.GetSlotWeight(ad);

		// The negotiator will always stop matching if it thinks it's
		// filled up the pool, so for offline ads representing a type
		// of resource, we need to multiply its weight accordingly.
		//
		// The offline ad MUST specify WantAdRevaluate if you want to
		// see information about more than one matching autocluster.
		int multiplier = 1;
		ad->LookupInteger( "GhostMultiplier", multiplier );
		slotWeight *= multiplier;

		sum+=slotWeight;
		if (minSlotWeight && (slotWeight < *minSlotWeight)) {
			*minSlotWeight = slotWeight;
		}
	}

	return sum;
}


bool
Matchmaker::TransformSubmitterAd(classad::ClassAd &ad)
{
	std::string map_names;
	if (!param(map_names, "NEGOTIATOR_CLASSAD_USER_MAP_NAMES")) {
		return true;
	}
	std::vector<std::string> map_names_list = split(map_names);
	if (!contains(map_names_list, "GROUP_PREFIX")) {
		return true;
	}

	std::string submitter_name;
	if (!ad.EvaluateAttrString(ATTR_NAME, submitter_name)) {
		return true;
	}
	auto last_at = submitter_name.find_last_of('@');
	if (last_at == std::string::npos) {
		return true;
	}
	auto accounting_domain = submitter_name.substr(last_at + 1);

	std::vector<classad::ExprTree*> args;
	args.emplace_back(classad::Literal::MakeString("GROUP_PREFIX"));
	args.emplace_back(classad::Literal::MakeString(accounting_domain));
	std::unique_ptr<ExprTree> fnCall(classad::FunctionCall::MakeFunctionCall("userMap", args));

	classad::Value val;
	fnCall->SetParentScope(&ad);
	if (!fnCall->Evaluate(val)) {
		return true;
	}
	fnCall->SetParentScope(nullptr);

	std::string new_prefix;
	if (!val.IsStringValue(new_prefix)) {
		return true;
	}

	std::string name;
	if (!ad.EvaluateAttrString(ATTR_NAME, name)) {
		dprintf(D_FULLDEBUG, "Prefix cannot evaluate to a string.\n");
		return false;
	}
	if (!ad.InsertAttr("OriginalName", name)) {
		return false;
	}
	std::string new_name = new_prefix + "." + name;
	dprintf(D_FULLDEBUG, "Negotiator maps submitter %s to %s.\n", name.c_str(), new_name.c_str());
	return ad.InsertAttr("Name", new_name);
}

bool Matchmaker::
obtainAdsFromCollector (
						ClassAdList &allAds,
						std::vector<ClassAd *> &startdAds,
						std::vector<ClassAd *> &submitterAds,
						std::set<std::string>  &submitterNames,
						ClaimIdHash &claimIds )
{
	CondorQuery privateQuery(STARTD_PVT_AD);
	QueryResult result;
	ClassAd *ad, *oldAd;
	MapEntry *oldAdEntry;
	int newSequence, oldSequence;
	bool reevaluate_ad;
	char    *remoteHost = NULL;
	CollectorList* collects = daemonCore->getCollectorList();

    cp_resources = false;

    // build a query for Scheduler, Submitter and (constrained) machine ads
    //
#if 1
	CondorQuery publicQuery(QUERY_MULTIPLE_PVT_ADS);
	publicQuery.addExtraAttribute(ATTR_SEND_PRIVATE_ATTRIBUTES, "true");

	if (!m_SubmitterConstraintStr.empty()) {
		publicQuery.addORConstraint(m_SubmitterConstraintStr.c_str());
	}
	publicQuery.convertToMulti(SUBMITTER_ADTYPE, true, false, false);

	if (strSlotConstraint && strSlotConstraint[0]) {
		publicQuery.addORConstraint(strSlotConstraint);
	}
	if (!ConsiderPreemption) {
		const char *projectionString =
			"ifThenElse((State == \"Claimed\"&&PartitionableSlot=!=true),\"Name MyType State Activity StartdIpAddr AccountingGroup Owner RemoteUser Requirements SlotWeight ConcurrencyLimits\",\"\") ";
		publicQuery.setDesiredAttrsExpr(projectionString);
		dprintf(D_ALWAYS, "Not considering preemption, therefore constraining idle machines with %s\n", projectionString);
	}
	publicQuery.convertToMulti(STARTD_SLOT_ADTYPE, true, true, false);
	// TODO: add this to the query and get rid of separate query for private ads?
	// publicQuery.convertToMulti(STARTD_PVT_ADTYPE, true, true, false);

#else
	static const char * submitter_ad_constraint = "MyType == \"" SUBMITTER_ADTYPE "\"";
	static const char * slot_ad_constraint = "MyType == \"" STARTD_SLOT_ADTYPE "\" || MyType == \"" STARTD_OLD_ADTYPE "\"";

	CondorQuery publicQuery(ANY_AD);
	std::string constraint;
	if (!m_SubmitterConstraintStr.empty()) {
		formatstr(constraint, "(%s) && (%s)", submitter_ad_constraint, m_SubmitterConstraintStr.c_str());
		publicQuery.addORConstraint(constraint.c_str());
	} else {
		publicQuery.addORConstraint(submitter_ad_constraint);
	}
    if (strSlotConstraint && strSlotConstraint[0]) {
        formatstr(constraint, "(%s) && (%s)", slot_ad_constraint, strSlotConstraint);
        publicQuery.addORConstraint(constraint.c_str());
    } else {
        publicQuery.addORConstraint(slot_ad_constraint);
    }

	privateQuery.addExtraAttribute(ATTR_SEND_PRIVATE_ATTRIBUTES, "true");
	publicQuery.addExtraAttribute(ATTR_SEND_PRIVATE_ATTRIBUTES, "true");

	// If preemption is disabled, we only need a handful of attrs from claimed ads.
	// Ask for that projection.

	if (!ConsiderPreemption) {
		const char *projectionString =
			"ifThenElse((State == \"Claimed\"&&PartitionableSlot=!=true),\"Name MyType State Activity StartdIpAddr AccountingGroup Owner RemoteUser Requirements SlotWeight ConcurrencyLimits\",\"\") ";
		publicQuery.setDesiredAttrsExpr(projectionString);

		dprintf(D_ALWAYS, "Not considering preemption, therefore constraining idle machines with %s\n", projectionString);
	}
#endif

	dprintf(D_ALWAYS,"  Getting startd private ads ...\n");
	ClassAdList startdPvtAdList;
	result = collects->query (privateQuery, startdPvtAdList);
	if( result!=Q_OK ) {
		dprintf(D_ALWAYS, "Couldn't fetch ads: %s\n", getStrQueryResult(result));
		return false;
	}

    CondorError errstack;
	dprintf(D_ALWAYS, "  Getting Scheduler, Submitter and Machine ads ...\n");
	result = collects->query (publicQuery, allAds, &errstack);
	if( result!=Q_OK ) {
		dprintf(D_ALWAYS, "Couldn't fetch ads: %s\n",
           errstack.code() ? errstack.getFullText(false).c_str() : getStrQueryResult(result)
           );
		return false;
	}

	dprintf(D_ALWAYS, "  Sorting %d ads ...\n",allAds.MyLength());

	allAds.Open();
	while( (ad=allAds.Next()) ) {

		// Insert each ad into the appropriate list.
		// After we insert it into a list, do not delete the ad...

		// If the collector included the admin capability, delete it.
		// We don't want it.
		ad->Delete(ATTR_REMOTE_ADMIN_CAPABILITY);

		// let's see if we've already got it - first lookup the sequence
		// number from the new ad, then let's look and see if we've already
		// got something for this one.		
		const char * mytype = GetMyTypeName(*ad);
		if(MATCH == strcmp(mytype,STARTD_SLOT_ADTYPE) || MATCH == strcmp(mytype,STARTD_OLD_ADTYPE)) {

			// first, let's make sure that will want to actually use this
			// ad, and if we can use it (old startds had no seq. number)
			reevaluate_ad = false;
			ad->LookupBool(ATTR_WANT_AD_REVAULATE, reevaluate_ad);
			newSequence = -1;	
			ad->LookupInteger(ATTR_UPDATE_SEQUENCE_NUMBER, newSequence);

			if(!ad->LookupString(ATTR_NAME, &remoteHost)) {
				dprintf(D_FULLDEBUG,"Rejecting unnamed startd ad.\n");
				continue;
			}

			// Next, let's transform the ad. The first thing we might
			// do is replace the Requirements attribute with whatever
			// we find in NegotiatorRequirements
			ExprTree  *negReqTree, *reqTree;
			const char *subReqs;
			subReqs = NULL;
			negReqTree = reqTree = NULL;
			negReqTree = ad->LookupExpr(ATTR_NEGOTIATOR_REQUIREMENTS);
			if ( negReqTree != NULL ) {

				// Save the old requirements expression
				reqTree = ad->LookupExpr(ATTR_REQUIREMENTS);
				if( reqTree != NULL ) {
					// Now, put the old requirements back into the ad
					// (note: ExprTreeToString uses a static buffer, so do not
					//        deallocate the buffer it returns)
					std::string attrn = "Saved";
					attrn += ATTR_REQUIREMENTS;
					subReqs = ExprTreeToString(reqTree);
					ad->AssignExpr(attrn, subReqs);
				}
		
				// Get the requirements expression we're going to
				// subsititute in, and convert it to a string...
				// Sadly, this might be the best interface :(
				subReqs = ExprTreeToString(negReqTree);
				ad->AssignExpr(ATTR_REQUIREMENTS, subReqs);
			}

			if( reevaluate_ad && newSequence != -1 ) {
				oldAd = NULL;
				oldAdEntry = NULL;

				std::string adID = MachineAdID(ad);
				stashedAds->lookup( adID, oldAdEntry);
				// if we find it...
				oldSequence = -1;
				if( oldAdEntry ) {
					oldSequence = oldAdEntry->sequenceNum;
					oldAd = oldAdEntry->oldAd;
				}

					// Find classad expression that decides if
					// new ad should replace old ad
				char *exprStr = param("STARTD_AD_REEVAL_EXPR");
				if (!exprStr) {
						// This matches the "old" semantic.
					exprStr = strdup("target.UpdateSequenceNumber > my.UpdateSequenceNumber");
				}

				ExprTree *expr = NULL;
				::ParseClassAdRvalExpr(exprStr, expr); // expr will be null on error

				bool replace = true;
				if (expr == NULL) {
					// error evaluating expression
					dprintf(D_ALWAYS, "Can't compile STARTD_AD_REEVAL_EXPR %s, treating as TRUE\n", exprStr);
					replace = true;
				} else {

						// Expression is valid, now evaluate it
						// old ad is "my", new one is "target"
					classad::Value er;
					int evalRet = EvalExprToBool(expr, oldAd, ad, er);
					if( !evalRet || !er.IsBooleanValueEquiv(replace) ) {
							// Something went wrong
						dprintf(D_ALWAYS, "Can't evaluate STARTD_AD_REEVAL_EXPR %s as a bool, treating as TRUE\n", exprStr);
						replace = true;
					}

						// But, if oldAd was null (i.e.. the first time), always replace
					if (!oldAd) {
						replace = true;
					}
				}

				free(exprStr);
				delete expr ;

					//if(newSequence > oldSequence) {
				if (replace) {
					if(oldSequence >= 0) {
						delete(oldAdEntry->oldAd);
						delete(oldAdEntry->remoteHost);
						delete(oldAdEntry);
						stashedAds->remove(adID);
					}
					MapEntry *me = new MapEntry;
					me->sequenceNum = newSequence;
					me->remoteHost = strdup(remoteHost);
					me->oldAd = new ClassAd(*ad);
					stashedAds->insert(adID, me);
				} else {
					/*
					  We have a stashed copy of this ad, and it's the
					  the same or a more recent ad, and we
					  we don't want to use the one in allAds. We determine
					  if an ad is more recent by evaluating an expression
					  from the config file that decides "newness".  By default,
					  this is just based on the sequence number.  However,
					  we need to make sure that the "stashed" ad gets into
					  allAds for this negotiation cycle, but we don't want
					  to get stuck in a loop evaluating the, so we remove
					  the sequence number before we put it into allAds - this
					  way, when we encounter it a few iteration later we
					  won't reconsider it
					*/

					allAds.Delete(ad);
					ad = new ClassAd(*(oldAdEntry->oldAd));
					ad->Delete(ATTR_UPDATE_SEQUENCE_NUMBER);
					allAds.Insert(ad);
				}
			}

            if (!cp_resources && cp_supports_policy(*ad)) {
                // we need to know if we will be encountering resource ads that
                // advertise a consumption policy
                cp_resources = true;
            }

			// Reset the per-negotiation-cycle match count, in case the
			// negotiation cycle is faster than the startd update interval.
			ad->Assign("MachineMatchCount", 0);
			ad->Delete("OfflineMatches");

			// If startd didn't set a slot weight expression, add in our own
			double slot_weight;
			if (!ad->LookupFloat(ATTR_SLOT_WEIGHT, slot_weight)) {
				ad->AssignExpr(ATTR_SLOT_WEIGHT, slotWeightStr);
			}

			OptimizeMachineAdForMatchmaking( ad );

			startdAds.emplace_back(ad);
		} else if( !strcmp(GetMyTypeName(*ad),SUBMITTER_ADTYPE) ) {

            std::string subname;
			std::string schedd_addr;
            if (!ad->LookupString(ATTR_NAME, subname) ||
                !ad->LookupString(ATTR_SCHEDD_IP_ADDR, schedd_addr)) {

                dprintf(D_ALWAYS, "WARNING: ignoring submitter ad with no name and/or address\n");
                continue;
            }
			if ( !IsValidSubmitterName( subname.c_str() ) ) {
				dprintf( D_ALWAYS, "WARNING: ignoring submitter ad with invalid name: %s\n", subname.c_str() );
				continue;
			}

			if (!TransformSubmitterAd(*ad)) {
				dprintf( D_ALWAYS, "WARNING: Failed to transform the submitter ad with name; %s\n", subname.c_str() );
				continue;
			}

            int numidle=0;
            ad->LookupInteger(ATTR_IDLE_JOBS, numidle);
            int numrunning=0;
            ad->LookupInteger(ATTR_RUNNING_JOBS, numrunning);
            int requested = numrunning + numidle;

            // This will avoid some wasted effort in negotiation looping
            if (requested <= 0) {
                dprintf(D_FULLDEBUG,
					"Ignoring submitter %s from schedd at %s with no requested jobs\n",
					subname.c_str(), schedd_addr.c_str());
                continue;
            }

			submitterNames.insert(subname);

    		ad->Assign(ATTR_TOTAL_TIME_IN_CYCLE, 0);

			ScheddsTimeInCycle[schedd_addr] = 0;

			// Now all that is left is to insert the submitter ad
			// into our list. However, if want_globaljobprio is true,
			// we insert a submitter ad for each job priority in the submitter
			// ad's job_prio_array attribute.  See gittrac #3218.
			if ( want_globaljobprio ) {
				std::string jobprioarray;

				if (!ad->LookupString(ATTR_JOB_PRIO_ARRAY,jobprioarray)) {
					// By design, if negotiator has want_globaljobprio and a schedd
					// does not give us a job prio array, behave as if this SubmitterAd had a
					// JobPrioArray attribute with a single value w/ the worst job priority
					jobprioarray = std::to_string( INT_MIN );
				}

				// Insert a group of submitter ads with one ATTR_JOB_PRIO value
				// taken from the list in ATTR_JOB_PRIO_ARRAY.
				for (const auto& prio: StringTokenIterator(jobprioarray)) {
					ClassAd *adCopy = new ClassAd( *ad );
					ASSERT(adCopy);
					adCopy->Assign(ATTR_JOB_PRIO, atoi(prio.c_str()));
					submitterAds.push_back(adCopy);
				}
			} else {
				// want_globaljobprio is false, so just insert the submitter
				// ad into our list as-is
				submitterAds.push_back(ad);
			}
		}
        free(remoteHost);
        remoteHost = NULL;
	}
	allAds.Close();

	// In the processing of allAds above, if want_globaljobprio is true,
	// we may have created additional submitter ads and inserted them
	// into submitterAds on the fly.
	// As ads in submitterAds are not deleted when submitterAds is destroyed,
	// we must be certain to insert these ads into allAds so it gets deleted.
	// To accomplish this, we simply iterate through submitterAds and insert all
	// ads found into submitterAds. No worries about duplicates since the Insert()
	// method checks for duplicates already.
	if (want_globaljobprio) {
		for (auto ad: submitterAds) {
			allAds.Insert(ad);
		}
	}

	// Map slot names to slot Classads, used by pslotMultiMatch() to
	// quickly find a given dslot ad.
	if (param_boolean("ALLOW_PSLOT_PREEMPTION", false))  {
		std::string name;
		for (ClassAd *ad: startdAds) {
			if (ad->LookupString(ATTR_NAME,name)) {
				m_slotNameToAdMap[name] = ad;
			}
		}
	}

	MakeClaimIdHash(startdPvtAdList,claimIds);

	dprintf(D_ALWAYS, "Got ads: %d public and %zu private\n",
	        allAds.MyLength(),claimIds.size());

	dprintf(D_ALWAYS, "Public ads include %zu submitter, %zu startd\n",
		submitterAds.size(), startdAds.size() );

	return true;
}

void
Matchmaker::OptimizeMachineAdForMatchmaking(ClassAd *ad)
{
		// The machine ad will be passed as the RIGHT ad during
		// matchmaking (i.e. in the call to IsAMatch()), so
		// optimize it accordingly.
	std::string error_msg;
	if( !classad::MatchClassAd::OptimizeRightAdForMatchmaking( ad, &error_msg ) ) {
		std::string name;
		ad->LookupString(ATTR_NAME,name);
		dprintf(D_ALWAYS,
				"Failed to optimize machine ad %s for matchmaking: %s\n",	
			name.c_str(),
				error_msg.c_str());
	}
}

void
Matchmaker::OptimizeJobAdForMatchmaking(ClassAd *ad)
{
		// The job ad will be passed as the LEFT ad during
		// matchmaking (i.e. in the call to IsAMatch()), so
		// optimize it accordingly.
	std::string error_msg;
	if( !classad::MatchClassAd::OptimizeLeftAdForMatchmaking( ad, &error_msg ) ) {
		int cluster_id=-1,proc_id=-1;
		ad->LookupInteger(ATTR_CLUSTER_ID,cluster_id);
		ad->LookupInteger(ATTR_PROC_ID,proc_id);
		dprintf(D_ALWAYS,
				"Failed to optimize job ad %d.%d for matchmaking: %s\n",	
				cluster_id,
				proc_id,
				error_msg.c_str());
	}
}

std::map<std::string, std::vector<std::string> > childClaimHash;

void
Matchmaker::MakeClaimIdHash(ClassAdList &startdPvtAdList, ClaimIdHash &claimIds)
{
	ClassAd *ad;
	startdPvtAdList.Open();

	bool pslotPreempt = param_boolean("ALLOW_PSLOT_PREEMPTION", false);
	childClaimHash.clear();

	while( (ad = startdPvtAdList.Next()) ) {
		std::string name;
		std::string ip_addr;
		std::string claim_id;
		std::string claimlist;

		if( !ad->LookupString(ATTR_NAME, name) ) {
			continue;
		}
		if( !ad->LookupString(ATTR_MY_ADDRESS, ip_addr) )
		{
			continue;
		}
			// As of 7.1.3, we look up CLAIM_ID first and CAPABILITY
			// second.  Someday CAPABILITY can be phased out.
		if( !ad->LookupString(ATTR_CLAIM_ID, claim_id) &&
			!ad->LookupString(ATTR_CAPABILITY, claim_id) &&
            !ad->LookupString(ATTR_CLAIM_ID_LIST, claimlist))
		{
			continue;
		}

			// hash key is name + ip_addr
		std::string key = name;
        key += ip_addr;
        ClaimIdHash::iterator f(claimIds.find(key));
        if (f == claimIds.end()) {
            claimIds[key];
            f = claimIds.find(key);
        } else {
            dprintf(D_ALWAYS, "Warning: duplicate key %s detected while loading private claim table, overwriting previous entry\n", key.c_str());
            f->second.clear();
        }

        // Use the new claim-list if it is present, otherwise use traditional claim id (not both)
        if (ad->LookupString(ATTR_CLAIM_ID_LIST, claimlist)) {
			for (const auto& id: StringTokenIterator(claimlist)) {
                f->second.insert(id);
            }
        } else {
            f->second.insert(claim_id);
        }
	
		if (pslotPreempt) {
				// Only expected for pslots
			std::string childClaims;
					// Grab the classad vector of ids
			int numKids = 0;
			int kids_set = ad->LookupInteger(ATTR_NUM_DYNAMIC_SLOTS,numKids);
			std::vector<std::string> claims;

				// foreach entry in that vector
			for (int kid = 0; kid < numKids; kid++) {
				std::string child_claim = "";
				// The startd sets this attribute under the name
				// ATTR_CHILD_CLAIM_IDS.
				// dslotLookupString() prepends "Child" to the given name,
				// so we use ATTR_CLAIM_IDS for this call.
				if ( dslotLookupString( ad, ATTR_CLAIM_IDS, kid, child_claim ) ) {
					claims.push_back( child_claim );
				} else {
					dprintf( D_FULLDEBUG, "Ignoring pslot with missing child claim ids\n" );
					kids_set = FALSE;
					break;
				}
			}

				// Put the newly-made vector of claims in the hash
				// if we got claim ids for all of the child dslots
			if ( kids_set ) {
				childClaimHash[key] = claims;
			}
		}
	}
	startdPvtAdList.Close();
}


static
bool getScheddAddr(const ClassAd &ad, std::string &scheddAddr)
{
        if (!ad.EvaluateAttrString(ATTR_SCHEDD_IP_ADDR, scheddAddr))
        {
                return false;
        }
	return true;
}

static
bool getSubmitter(const ClassAd &ad, std::string &submitter)
{
	if (!ad.EvaluateAttrString(ATTR_NAME, submitter))
	{
		return false;
	}
	return true;
}


static
bool makeSubmitterScheddHash(const ClassAd &ad, std::string &hash)
{
	std::string scheddAddr, submitterName;
	if (!getScheddAddr(ad, scheddAddr) || !getSubmitter(ad, submitterName))
	{
		return false;
	}
	std::string ss {submitterName};
    ss += ',';
	ss += scheddAddr;
	int jobprio = 0;
	ad.EvaluateAttrInt("JOBPRIO_MIN", jobprio);
	formatstr_cat(ss, ", %d", jobprio);
	ad.EvaluateAttrInt("JOBPRIO_MAX", jobprio);
	formatstr_cat(ss, ", %d", jobprio);
	hash = ss;
	return true;
}


typedef std::deque<ClassAd*> ScheddWork;
typedef classad_shared_ptr<ScheddWork> ScheddWorkPtr;
typedef std::map<std::string, ScheddWorkPtr> ScheddWorkMap;
typedef classad_shared_ptr<ResourceRequestList> RRLPtr;
typedef std::map<std::string, std::pair<ClassAd*, RRLPtr> > CurrentWorkMap;

static bool
assignWork(const ScheddWorkMap &workMap, CurrentWorkMap &curWork, ScheddWork &negotiations)
{
	negotiations.clear();
	unsigned workAssigned = 0;
	for (const auto & schedd_it : workMap)
	{
		if (!schedd_it.second.get()) {continue;} // null pointer.

		CurrentWorkMap::iterator cur_it = curWork.find(schedd_it.first);
		if (cur_it != curWork.end()) {continue;} // Already work for this schedd.

		ScheddWork & workRef = *schedd_it.second;
		if (workRef.empty()) {continue;} // No work for this schedd.

		RRLPtr emptyPtr;
		std::pair<ClassAd*, RRLPtr> work( workRef.front(), emptyPtr );
		curWork.insert(cur_it, std::make_pair(schedd_it.first, work));
		negotiations.push_back(workRef.front());
		workRef.pop_front();
		workAssigned++;
	}
	dprintf(D_FULLDEBUG, "Assigned %u units of work for prefetching.\n", workAssigned);
	return workAssigned;
}


void
Matchmaker::prefetchResourceRequestLists(std::vector<ClassAd *> &submitterAds)
{
	if (!param_boolean("NEGOTIATOR_PREFETCH_REQUESTS", true))
	{
		return;
	}

	ReliSock *sock;

	m_cachedRRLs.clear();
	ScheddWorkMap scheddWorkQueues;
	unsigned todoPrefetches = 0;
	for (ClassAd *submitterAd: submitterAds) {
		std::string scheddAddr;
		if (!getScheddAddr(*submitterAd, scheddAddr))
		{
			continue;
		}
		ScheddWorkMap::iterator iter = scheddWorkQueues.find(scheddAddr);
		if (iter == scheddWorkQueues.end())
		{
			ScheddWorkPtr work_ptr(new ScheddWork());
			iter = scheddWorkQueues.insert(iter, std::make_pair(scheddAddr, work_ptr));
		}
		iter->second->push_back(submitterAd);
		todoPrefetches++;
	}
	dprintf(D_ALWAYS, "Starting prefetch round; %u potential prefetches to do.\n", todoPrefetches);

	// Make sure our socket cache is big enough for all our current schedds.
	if (static_cast<unsigned>(sockCache->size()) < scheddWorkQueues.size())
	{
		sockCache->resize(scheddWorkQueues.size()+1);
	}
	
	CurrentWorkMap currentWork;
	ScheddWork negotiations;
	typedef std::map<int, std::pair<ClassAd*, RRLPtr> > FDToRRLMap;
	FDToRRLMap fdToRRL;
	unsigned attemptedPrefetches = 0, successfulPrefetches = 0;
	double startTime = _condor_debug_get_time_double();
	int prefetchTimeout = param_integer("NEGOTIATOR_PREFETCH_REQUESTS_TIMEOUT", NegotiatorTimeout);
	int prefetchCycle = param_integer("NEGOTIATOR_PREFETCH_REQUESTS_MAX_TIME");
	double deadline = (prefetchCycle > 0) ? (startTime + prefetchCycle) : -1;

	Selector selector;
	while (assignWork(scheddWorkQueues, currentWork, negotiations) || !currentWork.empty())
	{
		dprintf(D_FULLDEBUG, "Starting prefetch loop.\n");
		// Start a bunch of negotiations
		for (ScheddWork::const_iterator it=negotiations.begin(); it!=negotiations.end(); it++)
		{
			std::string submitter; getSubmitter(**it, submitter);
			std::string scheddAddr; getScheddAddr(**it, scheddAddr);
			dprintf(D_ALWAYS, "Starting prefetch negotiation for %s.\n", submitter.c_str());
			classad_shared_ptr<ResourceRequestList> rrl;
			attemptedPrefetches++;
			bool success = false;
			if (startNegotiateProtocol(submitter, **it, sock, rrl))
			{
				switch (rrl->tryRetrieve(sock))
				{
				case ResourceRequestList::RRL_DONE:
				case ResourceRequestList::RRL_NO_MORE_JOBS:
				{
					dprintf(D_FULLDEBUG, "Prefetch negotiation immediately finished.\n");
					if (rrl->needsEndNegotiateNow()) {endNegotiate(scheddAddr);}
					std::string hash; makeSubmitterScheddHash(**it, hash);
					m_cachedRRLs[hash] = rrl;
					CurrentWorkMap::iterator iter = currentWork.find(scheddAddr);
					if (iter != currentWork.end()) {currentWork.erase(iter);}
					else {dprintf(D_ALWAYS, "ERROR: Did prefetch work, but couldn't find it in the internal TODO list\n");}
					success = true;
					successfulPrefetches++;
					break;
				}
				case ResourceRequestList::RRL_ERROR:
					success = false;
					break;
				case ResourceRequestList::RRL_CONTINUE:
					dprintf(D_FULLDEBUG, "Prefetch negotiation would block.\n");
					currentWork[scheddAddr] = std::make_pair(*it, rrl);
					success = true;
					break;
				}
			}
			if (!success)
			{
				dprintf(D_ALWAYS, "Failed to prefetch resource request lists for %s(%s).\n", submitter.c_str(), scheddAddr.c_str());
				scheddWorkQueues[scheddAddr]->clear();
				CurrentWorkMap::iterator iter = currentWork.find(scheddAddr);
				if (iter != currentWork.end()) {currentWork.erase(iter);}
			}
		}

		if ((deadline >= 0) && (_condor_debug_get_time_double() > deadline))
		{
			dprintf(D_ALWAYS, "Prefetch cycle hit deadline of %d; skipping remaining submitters.\n", prefetchCycle);
			break;
		}

		// Non-blocking reads of RRLs
		selector.reset();
		selector.set_timeout(prefetchTimeout);

			// Put together the selector.
		unsigned workCount = 0;
		fdToRRL.clear();
		for (CurrentWorkMap::const_iterator it=currentWork.begin(); it!=currentWork.end(); it++)
		{
			ReliSock *sock = sockCache->findReliSock(it->first);
			if (!sock) {continue;}
			int fd = sock->get_file_desc();
			selector.add_fd(fd, Selector::IO_READ);
			fdToRRL[fd] = it->second;
			workCount++;
		}
		if (!workCount) {continue;}
		dprintf(D_FULLDEBUG, "Waiting on the results of %u negotiation sessions.\n", workCount);
		selector.execute();
		if (selector.timed_out() || selector.failed())
		{
			for (FDToRRLMap::const_iterator it = fdToRRL.begin(); it != fdToRRL.end(); it++)
			{
				std::string scheddAddr; getScheddAddr(*(it->second.first), scheddAddr);
				scheddWorkQueues[scheddAddr]->clear();
				ReliSock *sock = sockCache->findReliSock(scheddAddr);
				if (!sock) {continue;}
				CurrentWorkMap::iterator iter = currentWork.find(scheddAddr);
				if (iter != currentWork.end()) {currentWork.erase(iter);}
				endNegotiate(scheddAddr);
				sockCache->invalidateSock(scheddAddr.c_str());
				if (selector.timed_out()) {dprintf(D_ALWAYS, "Timeout when prefetching from %s; will skip this schedd for the remainder of prefetch cycle.\n", scheddAddr.c_str());}
				else {dprintf(D_ALWAYS, "Failure when waiting on results of negotiations sessions (%s, errno=%d).\n", strerror(selector.select_errno()), selector.select_errno());}
			}
		}
		else
			// Try getting the RRL for all ready sockets.
		for (FDToRRLMap::const_iterator it = fdToRRL.begin(); it != fdToRRL.end(); it++)
		{
			if (!selector.fd_ready(it->first, Selector::IO_READ)) {continue;}
			ResourceRequestList &rrl = *(it->second.second);
			std::string scheddAddr; getScheddAddr(*(it->second.first), scheddAddr);
			ReliSock *sock = sockCache->findReliSock(scheddAddr);
			if (!sock) {continue;}
			switch (rrl.tryRetrieve(sock)) {
			case ResourceRequestList::RRL_DONE:
			case ResourceRequestList::RRL_NO_MORE_JOBS:
			{
					// Successfully prefetched a RRL; cache it in the negotiator.
				if (rrl.needsEndNegotiateNow()) {endNegotiate(scheddAddr);}
				std::string hash; makeSubmitterScheddHash(*(it->second.first), hash);
				m_cachedRRLs[hash] = it->second.second;
				CurrentWorkMap::iterator iter = currentWork.find(scheddAddr);
				if (iter != currentWork.end()) {currentWork.erase(iter);}
				successfulPrefetches++;
				break;
			}
			case ResourceRequestList::RRL_ERROR:
			{
					// Do not attempt further prefetching with this schedd.
				scheddWorkQueues[scheddAddr]->clear();
				CurrentWorkMap::iterator iter = currentWork.find(scheddAddr);
				if (iter != currentWork.end()) {currentWork.erase(iter);}
				dprintf(D_ALWAYS, "Error when prefetching from %s; will skip this schedd for the remainder of prefetch cycle.\n", scheddAddr.c_str());
				break;
			}
			case ResourceRequestList::RRL_CONTINUE:
				// Nothing to do.
				break;
			}
		}

		if ((deadline >= 0) && (_condor_debug_get_time_double() > deadline))
		{
			dprintf(D_ALWAYS, "Prefetch cycle hit deadline of %d; skipping remaining submitters.\n", prefetchCycle);
			break;
		}
	}
	unsigned timedOutPrefetches = 0;
	for (CurrentWorkMap::const_iterator it=currentWork.begin(); it!=currentWork.end(); it++)
	{
		timedOutPrefetches++;
		dprintf(D_ALWAYS, "At end of the prefetch cycle, still waiting on response from %s; giving up and invalidating socket.\n", it->first.c_str());
		endNegotiate(it->first);
		sockCache->invalidateSock(it->first);
	}
	dprintf(D_ALWAYS, "Prefetch summary: %u attempted, %u successful.\n", attemptedPrefetches, successfulPrefetches);
	if (timedOutPrefetches)
	{
		dprintf(D_ALWAYS, "There were %u prefetches in progress when timeout limit was reached.\n", timedOutPrefetches);
	}
}


void
Matchmaker::endNegotiate(const std::string &scheddAddr)
{
	ReliSock *sock = sockCache->findReliSock(scheddAddr);
	if (!sock)
	{
		dprintf(D_ALWAYS, "    Asked to stop negotiation for %s but no active connection.\n", scheddAddr.c_str());
		return;
	}
	sock->encode();
	if (!sock->put (END_NEGOTIATE) || !sock->end_of_message())
	{
		dprintf(D_ALWAYS, "    Could not send END_NEGOTIATE/eom\n");
		sockCache->invalidateSock(scheddAddr.c_str());
	}
	dprintf(D_FULLDEBUG, "    Send END_NEGOTIATE to remote schedd\n");
}


classad_shared_ptr<ResourceRequestList>
Matchmaker::startNegotiate(const std::string &submitter, const ClassAd &submitterAd, ReliSock *&sock)
{
	RRLPtr request_list;
	std::string hash; makeSubmitterScheddHash(submitterAd, hash);
	RRLHash::iterator iter = m_cachedRRLs.find(hash);
	if (iter != m_cachedRRLs.end())
	{
		dprintf(D_FULLDEBUG, "Using resource request list from cache.\n");
		request_list = iter->second;
		m_cachedRRLs.erase(iter);
	}

	if (!startNegotiateProtocol(submitter, submitterAd, sock, request_list))
	{
		dprintf(D_FULLDEBUG, "Failed to start negotiation; ignoring cached request list.\n");
		request_list.reset();
	}

	return request_list;
}


bool
Matchmaker::startNegotiateProtocol(const std::string &submitter, const ClassAd &submitterAd, ReliSock *&sock, RRLPtr &request_list)
{
	// fetch the verison of the schedd, so we can take advantage of
	// protocol improvements in newer versions while still being
	// backwards compatible.
	std::string schedd_version_string;
	// from the version of the schedd, figure out the version of the negotiate
	// protocol supported.
	int schedd_negotiate_protocol_version = 0;
	if (submitterAd.EvaluateAttrString(ATTR_VERSION, schedd_version_string) && !schedd_version_string.empty())
	{
		CondorVersionInfo scheddVersion(schedd_version_string.c_str());
		if (scheddVersion.built_since_version(8,3,0))
		{
			// resource request lists supported...
			schedd_negotiate_protocol_version = 1;
		}
	}

	// Because of CCB, we may end up contacting a different
	// address than scheddAddr!  This is used for logging (to identify
	// the schedd) and to uniquely identify the host in the socketCache.
	// Do not attempt direct connections to this sinful string!
	std::string scheddAddr;
	if (!getScheddAddr(submitterAd, scheddAddr))
	{
		dprintf(D_ALWAYS, "Matchmaker::negotiate: Internal error: Missing IP address for schedd %s.  Please contact the Condor developers.\n", submitter.c_str());
		return false;
	}

	// Used for log messages to identify the schedd.
	// Not for other uses, as it may change!
	std::string schedd_id;
	formatstr(schedd_id, "%s (%s)", submitter.c_str(), scheddAddr.c_str());

	std::string submitter_tag;
	int negotiate_cmd = NEGOTIATE; // 7.5.4+
	if (!submitterAd.EvaluateAttrString(ATTR_SUBMITTER_TAG, submitter_tag))
	{
		dprintf(D_ERROR, "Matchmaker::negotiate: Submitter ad for %s is missing %s",
		        schedd_id.c_str(), ATTR_SUBMITTER_TAG);
		return false;
	}

	// 0.  connect to the schedd --- ask the cache for a connection
	sock = sockCache->findReliSock(scheddAddr);
	if (!sock)
	{
		dprintf(D_FULLDEBUG, "Socket to %s not in cache, creating one\n",
				 schedd_id.c_str());
			// not in the cache already, create a new connection and
			// add it to the cache.  We want to use a Daemon object to
			// send the first command so we setup a security session.

		if (IsDebugLevel(D_COMMAND))
		{
			dprintf(D_COMMAND, "Matchmaker::negotiate(%s,...) making connection to %s\n", getCommandStringSafe(negotiate_cmd), scheddAddr.c_str());
		}

		std::string capability;
		std::string sess_id;
		if (submitterAd.LookupString(ATTR_CAPABILITY, capability)) {
			ClaimIdParser cidp(capability.c_str());
			sess_id = cidp.secSessionId();
		}

		Daemon schedd(&submitterAd, DT_SCHEDD, 0);
		sock = schedd.reliSock(NegotiatorTimeout);
		if (!sock)
		{
			dprintf(D_ALWAYS, "    Failed to connect to %s\n", schedd_id.c_str());
			return false;
		}
		if (!schedd.startCommand(negotiate_cmd, sock, NegotiatorTimeout, nullptr, nullptr, false, sess_id.c_str())) {
			dprintf(D_ALWAYS, "    Failed to send NEGOTIATE command to %s\n",
					 schedd_id.c_str());
			delete sock;
			return false;
		}
			// finally, add it to the cache for later...
		sockCache->addReliSock(scheddAddr, sock);
	}
	else
	{
		dprintf(D_FULLDEBUG, "Socket to %s already in cache, reusing\n",
				 schedd_id.c_str());
			// this address is already in our socket cache.  since
			// we've already got a TCP connection, we do *NOT* want to
			// use a Daemon::startCommand() to create a new security
			// session, we just want to encode the command
			// int on the socket...
		sock->encode();
		if (!sock->put(negotiate_cmd)) {
			dprintf(D_ALWAYS, "    Failed to send NEGOTIATE command to %s\n",
					 schedd_id.c_str());
			sockCache->invalidateSock(scheddAddr);
			return false;
		}
	}

	sock->encode();
	// Keeping body of old if-statement to avoid re-indenting
	{
		// Here we create a negotiation ClassAd to pass parameters to the
		// schedd's negotiation method.
		ClassAd negotiate_ad;
		int jmin, jmax;
		// Tell the schedd to limit negotiation to this owner
		std::string orig_submitter;
		if (submitterAd.EvaluateAttrString("OriginalName", orig_submitter)) {
			negotiate_ad.InsertAttr(ATTR_OWNER, orig_submitter);
		} else {
			negotiate_ad.InsertAttr(ATTR_OWNER, submitter);
		}
		// Tell the schedd to limit negotiation to this job priority range
		if (want_globaljobprio && submitterAd.LookupInteger("JOBPRIO_MIN", jmin))
		{
			if (!submitterAd.LookupInteger("JOBPRIO_MAX", jmax))
			{
				EXCEPT("SubmitterAd with JOBPRIO_MIN attr, but no JOBPRIO_MAX");
			}
			negotiate_ad.Assign("JOBPRIO_MIN", jmin);
			negotiate_ad.Assign("JOBPRIO_MAX", jmax);
			dprintf (D_ALWAYS | D_MATCH,
				"    USE_GLOBAL_JOB_PRIOS limit to jobprios between %d and %d\n",
				jmin, jmax);
		}
		// Tell the schedd we're only interested in some of its jobs
		if ( !m_JobConstraintStr.empty() ) {
			negotiate_ad.AssignExpr(ATTR_NEGOTIATOR_JOB_CONSTRAINT,
			                        m_JobConstraintStr.c_str());
		}
		// Tell the schedd what sigificant attributes we found in the startd ads
		negotiate_ad.InsertAttr(ATTR_AUTO_CLUSTER_ATTRS, job_attr_references ? job_attr_references : "");
		// Tell the schedd a submitter tag value (used for flocking levels)
		negotiate_ad.InsertAttr(ATTR_SUBMITTER_TAG, submitter_tag.c_str());

		negotiate_ad.Assign(ATTR_MATCH_CLAIMED_PSLOTS, true);

		if (!putClassAd(sock, negotiate_ad))
		{
			dprintf(D_ALWAYS, "    Failed to send negotiation header to %s\n",
					 schedd_id.c_str());
			sockCache->invalidateSock(scheddAddr);
			return false;
		}
	}

	if (!sock->end_of_message())
	{
		dprintf(D_ALWAYS, "    Failed to send submitterName/eom to %s\n",
			schedd_id.c_str());
		sockCache->invalidateSock(scheddAddr);
		return false;
	}
	dprintf(D_FULLDEBUG, "Started NEGOTIATE with remote schedd; protocol version %d.\n", schedd_negotiate_protocol_version);

	if (!request_list.get())
	{
		request_list.reset(new ResourceRequestList(schedd_negotiate_protocol_version));
	}
	return true;
}

int Matchmaker::
negotiate(char const* groupName, char const *submitterName, const ClassAd *submitterAd, double priority,
		   double submitterLimit, double submitterLimitUnclaimed, int submitterCeiling,
		   std::vector<ClassAd *> &startdAds, ClaimIdHash &claimIds,
		   bool ignore_schedd_limit, time_t deadline,
		   int& numMatched, double &pieLeft)
{
	ReliSock	*sock;
	int			cluster, proc, autocluster;
	int			result;
	time_t		currentTime;
	time_t		beginTime = time(NULL);
	ClassAd		request;
	ClassAd*    offer = NULL;
	bool		only_consider_startd_rank = false;
	bool		display_overlimit = true;
	bool		limited_by_submitterLimit = false;
	std::string remoteUser;
	double limitUsed = 0.0;
	double limitUsedUnclaimed = 0.0;

	numMatched = 0;

	classad_shared_ptr<ResourceRequestList> request_list = startNegotiate(submitterName, *submitterAd, sock);
	if (!request_list.get()) {return MM_ERROR;}

	std::string scheddName;
	if (!submitterAd->LookupString(ATTR_SCHEDD_NAME, scheddName)) {
		dprintf(D_ALWAYS, "Matchmaker::negotiate: Internal error: Missing schedd name for submitter %s.\n", submitterName);
		return MM_ERROR;
	}
	std::string scheddAddr;
	if (!getScheddAddr(*submitterAd, scheddAddr))
	{
		dprintf (D_ALWAYS, "Matchmaker::negotiate: Internal error: Missing IP address for schedd %s.  Please contact the Condor developers.\n", submitterName);
		return MM_ERROR;
	}
	// Used for log messages to identify the schedd.
	// Not for other uses, as it may change!
	std::string schedd_id;
	formatstr(schedd_id, "%s (%s)", submitterName, scheddAddr.c_str());
	
	int schedd_will_match = 1; // number of extra jobs schedd will put into a partitionable slot

	// 2.  negotiation loop with schedd
	for (numMatched=0;true;numMatched++)
	{
		// Service any interactive commands on our command socket.
		// This keeps condor_userprio hanging to a minimum when
		// we are involved in a lot of schedd negotiating.
		// It also performs the important function of draining out
		// any reschedule requests queued up on our command socket, so
		// we do not negotiate over & over unnecesarily.

		daemonCore->ServiceCommandSocket();

		currentTime = time(NULL);

		if (currentTime >= deadline) {
			dprintf (D_ALWAYS, 	
			"    Reached deadline for %s after %d sec... stopping\n"
			"       MAX_TIME_PER_SUBMITTER = %d sec, MAX_TIME_PER_SCHEDD = %d sec, MAX_TIME_PER_CYCLE = %d sec, MAX_TIME_PER_PIESPIN = %d sec\n",
				schedd_id.c_str(), (int)(currentTime - beginTime),
				MaxTimePerSubmitter, MaxTimePerSchedd, MaxTimePerCycle,
				MaxTimePerSpin);
			break;	// get out of the infinite for loop & stop negotiating
		}


		// Handle the case if we are over the submitterLimit
		if( limitUsed >= submitterLimit ) {
			if( ignore_schedd_limit ) {
				only_consider_startd_rank = true;
				if( display_overlimit ) {
					display_overlimit = false;
					dprintf(D_FULLDEBUG,
							"    Over submitter resource limit (%f, used %f) ... "
							"only consider startd ranks\n", submitterLimit,limitUsed);
				}
			} else {
				dprintf (D_ALWAYS, 	
						 "    Reached submitter resource limit: %f ... stopping\n", limitUsed);
				break;	// get out of the infinite for loop & stop negotiating
			}
		} else {
			only_consider_startd_rank = false;
		}


		if (limitUsed >= submitterCeiling) {
			dprintf(D_ALWAYS, "  This submitter has hit the ceiling (got %f new slots this cycle), stopping negotiation\n", limitUsed);
			break; // stop negotiating
		}

		// 2a.  ask for job information
		if ( !request_list->getRequest(request,cluster,proc,autocluster,sock, schedd_will_match) ) {
			// Failed to get a request.  Check to see if it is because
			// of an error talking to the schedd.
			if ( request_list->hadError() ) {
				// note: error message already dprintf-ed
				sockCache->invalidateSock(scheddAddr);
				return MM_ERROR;
			}
			if (request_list->needsEndNegotiate())
			{
				endNegotiate(scheddAddr);
				schedd_will_match = 1;
			}
			// Failed to get a request, and no error occured.
			// If we have negotiated above our submitterLimit, we have only
			// considered matching if the offer strictly prefers the request.
			// So in this case, return MM_RESUME since there still may be
			// jobs which the schedd wants scheduled but have not been considered
			// as candidates for no preemption or user priority preemption.
			// Also, if we were limited by submitterLimit, resume
			// in the next spin of the pie, because our limit might
			// increase.
			if( limitUsed >= submitterLimit || limited_by_submitterLimit ) {
				return MM_RESUME;
			} else {
				return MM_DONE;
			}
		}
		// end of asking for job information - we now have a request
	

        negotiation_cycle_stats[0]->num_jobs_considered += 1;

        // information regarding the negotiating group context:
		std::string negGroupName = (groupName != NULL) ? groupName : hgq_root_group->name.c_str();
        request.Assign(ATTR_SUBMITTER_NEGOTIATING_GROUP, negGroupName);
        request.Assign(ATTR_SUBMITTER_AUTOREGROUP, (autoregroup && (negGroupName == hgq_root_group->name)));

		// insert the submitter user priority attributes into the request ad
		// first insert old-style ATTR_SUBMITTOR_PRIO
		request.Assign(ATTR_SUBMITTOR_PRIO , priority );
		// next insert new-style ATTR_SUBMITTER_USER_PRIO
		request.Assign(ATTR_SUBMITTER_USER_PRIO , priority );
		// next insert the submitter user usage attributes into the request
		request.Assign(ATTR_SUBMITTER_USER_RESOURCES_IN_USE,
					   accountant.GetWeightedResourcesUsed ( submitterName ));
		std::string temp_groupName;
		double temp_groupQuota, temp_groupUsage;
		if (getGroupInfoFromUserId(submitterName, temp_groupName, temp_groupQuota, temp_groupUsage)) {
			// this is a group, so enter group usage info
            request.Assign(ATTR_SUBMITTER_GROUP,temp_groupName);
			request.Assign(ATTR_SUBMITTER_GROUP_RESOURCES_IN_USE,temp_groupUsage);
			request.Assign(ATTR_SUBMITTER_GROUP_QUOTA,temp_groupQuota);
		}

        // when resource ads with consumption policies are in play, optimizing
        // the Requirements attribute can break the augmented consumption policy logic
        // that overrides RequestXXX attributes with corresponding values supplied by
        // the consumption policy
        if (!cp_resources) {
            OptimizeJobAdForMatchmaking( &request );
        }

		if( IsDebugLevel( D_JOB ) ) {
			dprintf(D_JOB,"Searching for a matching machine for the following job ad:\n");
			dPrintAd(D_JOB, request);
		}

		// 2e.  find a compatible offer for the request --- keep attempting
		//		to find matches until we can successfully (1) find a match,
		//		AND (2) notify the startd; so quit if we got a MM_GOOD_MATCH,
		//		or if MM_NO_MATCH could be found
		result = MM_BAD_MATCH;
		while (result == MM_BAD_MATCH)
		{
            remoteUser = "";
			// 2e(i).  find a compatible offer
			offer=matchmakingAlgorithm(submitterName, scheddAddr.c_str(), scheddName.c_str(), request,
                                             startdAds, priority,
                                             limitUsed, limitUsedUnclaimed,
                                             submitterLimit, submitterLimitUnclaimed,
											 pieLeft,
											 only_consider_startd_rank);

			if( !offer )
			{
				// lookup want_match_diagnostics in request
				// 0 = no match diagnostics
				// 1 = match diagnostics string
				// 2 = match diagnostics string w/ autocluster + jobid
				int want_match_diagnostics = 0;
				request.LookupInteger(ATTR_WANT_MATCH_DIAGNOSTICS,want_match_diagnostics);
				std::string diagnostic_message;
				// no match found
				dprintf(D_ALWAYS|D_MATCH, "      Rejected %d.%d %s %s: ",
						cluster, proc, submitterName, scheddAddr.c_str());

				negotiation_cycle_stats[0]->rejections++;

				if( rejForSubmitterLimit ) {
                    negotiation_cycle_stats[0]->submitters_share_limit.insert(submitterName);
					limited_by_submitterLimit = true;
				}
				if (rejForNetwork) {
					diagnostic_message = "insufficient bandwidth";
					dprintf(D_ALWAYS|D_MATCH|D_NOHEADER, "%s\n",
							diagnostic_message.c_str());
				} else {
					if (rejForNetworkShare) {
						diagnostic_message = "network share exceeded";
					} else if (rejForConcurrencyLimit) {
						std::string ss;
						std::set<std::string>::const_iterator it = rejectedConcurrencyLimits.begin();
						while (true) {
							ss +=  *it;
							it++;
							if (it == rejectedConcurrencyLimits.end()) {break;}
							else {ss += ", ";}
						}
						diagnostic_message = std::string("concurrency limit ") + ss + " reached";
					} else if (rejPreemptForPolicy) {
						diagnostic_message =
							"PREEMPTION_REQUIREMENTS == False";
					} else if (rejPreemptForPrio) {
						diagnostic_message = "insufficient priority";
					} else if (rejForSubmitterLimit) {
                        diagnostic_message = "submitter limit exceeded";
					} else {
						diagnostic_message = "no match found";
					}
					dprintf(D_ALWAYS|D_MATCH|D_NOHEADER, "%s\n",
							diagnostic_message.c_str());
				}
				// add in autocluster and job id info if requested
				if ( want_match_diagnostics == 2 ) {
					std::string diagnostic_jobinfo;
					formatstr(diagnostic_jobinfo," |%d|%d.%d|",autocluster,cluster,proc);
					diagnostic_message += diagnostic_jobinfo;
				}
				sock->encode();
				if ((want_match_diagnostics) ?
					(!sock->put(REJECTED_WITH_REASON) ||
					 !sock->put(diagnostic_message) ||
					 !sock->end_of_message()) :
					(!sock->put(REJECTED) || !sock->end_of_message()))
					{
						dprintf (D_ALWAYS, "      Could not send rejection\n");
						sock->end_of_message ();
						sockCache->invalidateSock(scheddAddr.c_str());
						
						return MM_ERROR;
					}
				result = MM_NO_MATCH;
				continue;
			}

			if ((offer->LookupString(ATTR_PREEMPTING_ACCOUNTING_GROUP, remoteUser)==1) ||
				(offer->LookupString(ATTR_PREEMPTING_USER, remoteUser)==1) ||
				(offer->LookupString(ATTR_ACCOUNTING_GROUP, remoteUser)==1) ||
			    (offer->LookupString(ATTR_REMOTE_USER, remoteUser)==1))
			{
                char	*remoteHost = NULL;
                double	remotePriority;

				offer->LookupString(ATTR_NAME, &remoteHost);
				remotePriority = accountant.GetPriority (remoteUser);


				double newStartdRank;
				double oldStartdRank = 0.0;
				if(! EvalFloat(ATTR_RANK, offer, &request, newStartdRank)) {
					newStartdRank = 0.0;
				}
				offer->LookupFloat(ATTR_CURRENT_RANK, oldStartdRank);

				// got a candidate preemption --- print a helpful message
				dprintf( D_ALWAYS, "      Preempting %s (user prio=%.2f, startd rank=%.2f) on %s "
						 "for %s (user prio=%.2f, startd rank=%.2f)\n", remoteUser.c_str(),
						 remotePriority, oldStartdRank, remoteHost, submitterName,
						 priority, newStartdRank );
                free(remoteHost);
                remoteHost = NULL;
			}

			// 2e(ii).  perform the matchmaking protocol
			result = matchmakingProtocol (request, offer, claimIds, sock,
					submitterName, scheddAddr.c_str());

			// 2e(iii). if the matchmaking protocol failed, do not consider the
			//			startd again for this negotiation cycle.
			if (result == MM_BAD_MATCH) {
				auto it = std::find(startdAds.begin(), startdAds.end(), offer);
				if (it != startdAds.end()) {
					startdAds.erase(it);
				}
			}
			// 2e(iv).  if the matchmaking protocol failed to talk to the
			//			schedd, invalidate the connection and return
			if (result == MM_ERROR)
			{
				sockCache->invalidateSock (scheddAddr.c_str());
				return MM_ERROR;
			}
		}

		// 2f.  if MM_NO_MATCH was found for the request, get another request
		if (result == MM_NO_MATCH)
		{
			numMatched--;		// haven't used any resources this cycle

			request_list->noMatchFound(); // do not reuse any cached requests
			schedd_will_match = 1;

            if (rejForSubmitterLimit && !ConsiderPreemption && !accountant.UsingWeightedSlots()) {
                // If we aren't considering preemption and slots are unweighted, then we can
                // be done with this submitter when it hits its submitter limit
                dprintf (D_ALWAYS, "    Hit submitter limit: done negotiating\n");
                // stop negotiation and return MM_RESUME
                // we don't want to return with MM_DONE because
                // we didn't get NO_MORE_JOBS: there are jobs that could match
                // in later cycles with a quota redistribution
                break;
            }

            // Otherwise continue trying with this submitter
			continue;
		}

        double match_cost = 0;
        if (offer->LookupFloat(CP_MATCH_COST, match_cost)) {
            // If CP_MATCH_COST attribute is present, this match involved a consumption policy.
            offer->Delete(CP_MATCH_COST);

            // In this mode we don't remove offers, because the goal is to allow
            // other jobs/requests to match against them and consume resources, if possible
            //
            // A potential future RFE here would be to support an option for choosing "breadth-first"
            // or "depth-first" slot utilization.  If breadth-first was chosen, then the slot
            // could be shuffled to the back.  It might even be possible to allow a slot-specific
            // policy choice for this behavior.
        } else {
    		bool reevaluate_ad = false;
    		offer->LookupBool(ATTR_WANT_AD_REVAULATE, reevaluate_ad);
    		if (reevaluate_ad) {
    			reeval(offer);
        		// Shuffle this resource to the end of the list.  This way, if
        		// two resources with the same RANK match, we'll hand them out
        		// in a round-robin way
				auto pos = std::ranges::find(startdAds, offer);
				std::rotate(pos, pos + 1, startdAds.end());
    		} else  {
                // 2g.  Delete ad from list so that it will not be considered again in
		        // this negotiation cycle
				startdAds.erase(std::ranges::find(startdAds,offer));
    		}
            // traditional match cost is just slot weight expression
            match_cost = accountant.GetSlotWeight(offer);
        }
        dprintf(D_FULLDEBUG, "Match completed, match cost= %g\n", match_cost);

		if (param_boolean("NEGOTIATOR_DEPTH_FIRST", false)) {
			schedd_will_match = jobsInSlot(request, *offer);
		}

		limitUsed += match_cost;
        if (remoteUser == "") limitUsedUnclaimed += match_cost;
		pieLeft -= match_cost;
		negotiation_cycle_stats[0]->matches++;
	}


	// break off negotiations
	endNegotiate(scheddAddr);

	// ... and continue negotiating with others
	return MM_RESUME;
}

void Matchmaker::
updateNegCycleEndTime(time_t startTime, ClassAd *submitter) {
	std::string schedd_addr;
	time_t endTime;
	int oldTotalTime;

	endTime = time(nullptr);
	submitter->LookupInteger(ATTR_TOTAL_TIME_IN_CYCLE, oldTotalTime);
	submitter->Assign(ATTR_TOTAL_TIME_IN_CYCLE,
	                  (oldTotalTime + (endTime - startTime)));

	if ( submitter->LookupString( ATTR_SCHEDD_IP_ADDR, schedd_addr ) ) {
		ScheddsTimeInCycle[schedd_addr] += endTime - startTime;
	}
}

double Matchmaker::
EvalNegotiatorMatchRank(char const *expr_name,ExprTree *expr,
                        ClassAd &request,ClassAd *resource)
{
	classad::Value result;
	double rank = -(DBL_MAX);

	if(expr && EvalExprToNumber(expr,resource,&request,result)) {
		double val;
		if( result.IsNumber(val) ) {
			rank = (float)val;
		} else {
			dprintf(D_ALWAYS, "Failed to evaluate %s "
			                  "expression to a float.\n",expr_name);
		}
	} else if(expr) {
		dprintf(D_ALWAYS, "Failed to evaluate %s "
		                  "expression.\n",expr_name);
	}
	return rank;
}

bool Matchmaker::
SubmitterLimitPermits(ClassAd* request, ClassAd* candidate, double used, double allowed, double /*pieLeft*/) {
    double match_cost = 0;

    if (cp_supports_policy(*candidate)) {
        // deduct assets in test-mode only, for purpose of getting match cost
        match_cost = cp_deduct_assets(*request, *candidate, true);
    } else {
        match_cost = accountant.GetSlotWeight(candidate);
    }

    if ((used + match_cost) <= allowed) {
        return true;
    }

    if ((used <= 0) && (allowed > 0)) {

		// Allow user to round up once per pie spin in order to avoid
		// "crumbs" being left behind that couldn't be taken by anyone
		// because they were split between too many users.  Only allow
		// this if there is enough total pie left to dish out this
		// resource in this round.  ("pie_left" is somewhat of a
		// fiction, since users in the current group may be stealing
		// pie from each other as well as other sources, but
		// subsequent spins of the pie should deal with that
		// inaccuracy.)

		return true;
	}
	return false;
}

bool
Matchmaker::
rejectForConcurrencyLimits(std::string &limits)
{
	std::transform(limits.begin(), limits.end(), limits.begin(), ::tolower);
	if (lastRejectedConcurrencyString == limits) {
		//dprintf(D_FULLDEBUG, "Rejecting job due to concurrency limits %s (see original rejection message).\n", limits.c_str());
		return true;
	}

	for (const auto& limit: StringTokenIterator(limits)) {
		double increment;
		char* limit_cpy = strdup(limit.c_str());
		// This mutates the first argument, so we need to copy it.
		if ( !ParseConcurrencyLimit(limit_cpy, increment) ) {
			dprintf( D_FULLDEBUG, "Ignoring invalid concurrency limit '%s'\n",
					 limit.c_str() );
			free(limit_cpy);
			continue;
		}

		double count = accountant.GetLimit(limit_cpy);

		double max = accountant.GetLimitMax(limit_cpy);

		dprintf(D_FULLDEBUG,
			"Concurrency Limit: %s is %f of max %f\n",
			limit_cpy, count, max);

		if (count < 0) {
			dprintf(D_ALWAYS, "ERROR: Concurrency Limit %s is %f (below 0)\n",
				limit_cpy, count);
			free(limit_cpy);
			return true;
		}

		if (count + increment > max) {
			dprintf(D_FULLDEBUG,
				"Concurrency Limit %s is %f, requesting %f, "
				"but cannot exceed %f\n",
				limit_cpy, count, increment, max);

			rejForConcurrencyLimit++;
			rejectedConcurrencyLimits.insert(limit_cpy);
			lastRejectedConcurrencyString = limits;
			free(limit_cpy);
			return true;
		}
		free(limit_cpy);
	}
	return false;
}

//
// getSinfulStringProtocolBools() short-circuits based on the comparison
// in Matchmaker::matchmakingAlgorithm(); it is not a general-purpose function.
//
// If isIPv4 or isIPv6 is true, then the function will return early if
// the given protocol is found in the sinful string. This optimizes cases
// where we only care whether at least one of the protocols of interest
// is present.

void
getSinfulStringProtocolBools( bool isIPv4, bool isIPv6,
		const char * sinfulString, bool & v4, bool & v6 ) {
	// Compare the primary addresses.
	if( sinfulString[1] == '[' ) {
		v6 = true;
		if( isIPv6 ) { return; }
	}

	if( isdigit( sinfulString[1] ) ) {
		v4 = true;
		if( isIPv4 ) { return; }
	}

	// Look for the addrs list.
	const char * addr = strstr( sinfulString, "addrs=" );
	if( ! addr ) { return; }
	addr += 6;

	const char * currentAddr = addr;
	while( currentAddr[0] != '\0' ) {
		if( currentAddr[0] == '[' ) {
			v6 = true;
			if( isIPv6 ) { return; }
		}

		if( isdigit( currentAddr[0] ) ) {
			v4 = true;
			if( isIPv4 ) { return; }
		}

		size_t span = strcspn( currentAddr, "+>&;" );
		currentAddr += span;
		if( currentAddr[0] != '+' ) { return; }
		++currentAddr;
	}
}


/*
Warning: scheddAddr may not be the actual address we'll use to contact the
schedd, thanks to CCB.  It _is_ suitable for use as a unique identifier, for
display to the user, or for calls to sockCache->invalidateSock.
*/
ClassAd *Matchmaker::
matchmakingAlgorithm(const char *submitterName, const char *scheddAddr, const char* scheddName, ClassAd &request,
					 std::vector<ClassAd *> &startdAds,
					 double preemptPrio,
					 double limitUsed, double limitUsedUnclaimed,
                     double submitterLimit, double submitterLimitUnclaimed,
					 double pieLeft,
					 bool only_for_startdrank)
{
		// to store values pertaining to a particular candidate offer
	double			candidateRankValue;
	double			candidatePreJobRankValue;
	double			candidatePostJobRankValue;
	double			candidatePreemptRankValue;
	PreemptState	candidatePreemptState;
	std::string			candidateDslotClaims;
		// to store the best candidate so far
	ClassAd 		*bestSoFar = NULL;	
	ClassAd 		*cached_bestSoFar = NULL;	
	double			bestRankValue = -(FLT_MAX);
	double			bestPreJobRankValue = -(FLT_MAX);
	double			bestPostJobRankValue = -(FLT_MAX);
	double			bestPreemptRankValue = -(FLT_MAX);
	PreemptState	bestPreemptState = (PreemptState)-1;
	std::string			bestDslotClaims;
	bool			newBestFound;
		// to store results of evaluations
	std::string remoteUser;
	classad::Value	result;
	bool			val;
		// request attributes
	int				requestAutoCluster = -1;

	dprintf(D_FULLDEBUG, "matchmakingAlgorithm: limit %f used %f pieLeft %f\n", submitterLimit, limitUsed, pieLeft);

		// Check resource constraints requested by request
	rejForConcurrencyLimit = 0;
	lastRejectedConcurrencyString = "";
	std::string limits;
	bool evaluate_limits_with_match = true;
	if (request.LookupString(ATTR_CONCURRENCY_LIMITS, limits)) {
		evaluate_limits_with_match = false;
		if (rejectForConcurrencyLimits(limits)) {
			return NULL;
		}
	} else if (!request.Lookup(ATTR_CONCURRENCY_LIMITS)) {
		evaluate_limits_with_match = false;
	}
	rejectedConcurrencyLimits.clear();

	request.LookupInteger(ATTR_AUTO_CLUSTER_ID, requestAutoCluster);

		// If this incoming job is from the same user, same schedd,
		// and is in the same autocluster, and we have a MatchList cache,
		// then we can just pop off
		// the top entry in our MatchList if we have one.  The
		// MatchList is essentially just a sorted cache of the machine
		// ads that match jobs of this type (i.e. same autocluster).
	if ( MatchList &&
		 cachedAutoCluster != -1 &&
		 cachedAutoCluster == requestAutoCluster &&
		 cachedPrio == preemptPrio &&
		 cachedOnlyForStartdRank == only_for_startdrank &&
		 strcmp(cachedName,submitterName)==0 &&
		 strcmp(cachedAddr,scheddAddr)==0 &&
		 MatchList->cache_still_valid(request,PreemptionReq,PreemptionRank,
					preemption_req_unstable,preemption_rank_unstable) )
	{
		// we can use cached information.  pop off the best
		// candidate from our sorted list.
		while( (cached_bestSoFar = MatchList->pop_candidate(candidateDslotClaims)) ) {
			if (evaluate_limits_with_match) {
				std::string limits;
				if (EvalString(ATTR_CONCURRENCY_LIMITS, &request, cached_bestSoFar, limits)) {
					if (rejectForConcurrencyLimits(limits)) {
						continue;
					}
				}
			}
			int t = 0;
			cached_bestSoFar->LookupInteger(ATTR_PREEMPT_STATE_, t);
			PreemptState pstate = PreemptState(t);
			if ((pstate != NO_PREEMPTION) && SubmitterLimitPermits(&request, cached_bestSoFar, limitUsed, submitterLimit, pieLeft)) {
				break;
			} else if (SubmitterLimitPermits(&request, cached_bestSoFar, limitUsedUnclaimed, submitterLimitUnclaimed, pieLeft)) {
				break;
			}
			MatchList->increment_rejForSubmitterLimit();
		}
		dprintf(D_FULLDEBUG,"Attempting to use cached MatchList: %s (MatchList length: %d, Autocluster: %d, Submitter Name: %s, Schedd Address: %s)\n",
			cached_bestSoFar?"Succeeded.":"Failed",
			MatchList->length(),
			requestAutoCluster,
			submitterName,
			scheddAddr
			);
		if ( ! cached_bestSoFar ) {
				// if we don't have a candidate, fill in
				// all the rejection reason counts.
			MatchList->get_diagnostics(
				rejForNetwork,
				rejForNetworkShare,
				rejForConcurrencyLimit,
				rejPreemptForPrio,
				rejPreemptForPolicy,
				rejPreemptForRank,
				rejForSubmitterLimit,
				rejForSubmitterCeiling);
		}
		if ( cached_bestSoFar && !candidateDslotClaims.empty() ) {
			cached_bestSoFar->Assign("PreemptDslotClaims", candidateDslotClaims);
		}
			//  TODO  - compare results, reserve net bandwidth
		return cached_bestSoFar;
	}

		// Delete our old MatchList, since we know that if we made it here
		// we no longer are dealing with a job from the same autocluster.
		// (someday we will store it in case we see another job with
		// the same autocluster, but we aren't that smart yet...)
	DeleteMatchList();

		// Create a new MatchList cache if desired via config file,
		// and the job ad contains autocluster info,
		// and there are machines potentially available to consider.		
	if ( want_matchlist_caching &&		// desired via config file
		 requestAutoCluster != -1 &&	// job ad contains autocluster info
		 startdAds.size() > 0 )		// machines available
	{
		MatchList = new MatchListType( startdAds.size() );
		cachedAutoCluster = requestAutoCluster;
		cachedPrio = preemptPrio;
		cachedOnlyForStartdRank = only_for_startdrank;
		cachedName = strdup(submitterName);
		cachedAddr = strdup(scheddAddr);
	}


	// initialize reasons for match failure
	rejForNetwork = 0;
	rejForNetworkShare = 0;
	rejPreemptForPrio = 0;
	rejPreemptForPolicy = 0;
	rejPreemptForRank = 0;
	rejForSubmitterLimit = 0;

	bool allow_pslot_preemption = param_boolean("ALLOW_PSLOT_PREEMPTION", false);
	double allocatedWeight = 0.0;
		// Set up for parallel matchmaking, if enabled
	std::vector<ClassAd *> par_candidates;
	std::vector<ClassAd *> par_matches;

	int num_threads =  param_integer("NEGOTIATOR_NUM_THREADS", 1);
	if (num_threads > 1) {
		par_candidates = startdAds;
		ParallelIsAMatch(&request, par_candidates, par_matches, num_threads, false);
	}

	// scan the offer ads
	std::string machineAddr;
	std::string pslot_claimer;
	bool is_pslot;

	bool isIPv4 = false;
	bool isIPv6 = false;
	getSinfulStringProtocolBools( false, false, scheddAddr, isIPv4, isIPv6 );

	for (ClassAd *candidate: startdAds) {
		bool v4 = false;
		bool v6 = false;
		candidate->LookupString( "MyAddress", machineAddr );
		// This short-circuits based on the subsequent evaluation, so
		// be sure only to change them in tandem.
		getSinfulStringProtocolBools( isIPv4, isIPv6, machineAddr.c_str(),
			v4, v6 );
		if(! ((isIPv4 && v4) || (isIPv6 && v6))) { continue; }

		if( IsDebugVerbose(D_MACHINE) ) {
			dprintf(D_MACHINE,"Testing whether the job matches with the following machine ad:\n");
			dPrintAd(D_MACHINE, *candidate);
		}

		if ( allow_pslot_preemption ) {
			bool is_dslot = false;
			candidate->LookupBool( ATTR_SLOT_DYNAMIC, is_dslot );
			if ( is_dslot ) {
				bool rollup = false;
				candidate->LookupBool( ATTR_PSLOT_ROLLUP_INFORMATION, rollup );
				if ( rollup ) {
					continue;
				}
			}
		}

		int cluster_id=-1,proc_id=-1;
		std::string machine_name;
		if( IsDebugLevel( D_MACHINE ) ) {
			request.LookupInteger(ATTR_CLUSTER_ID,cluster_id);
			request.LookupInteger(ATTR_PROC_ID,proc_id);
			candidate->LookupString(ATTR_NAME,machine_name);
		}

		is_pslot = false;
		candidate->LookupBool(ATTR_SLOT_PARTITIONABLE, is_pslot);
		if (is_pslot && candidate->LookupString(ATTR_REMOTE_SCHEDD_NAME, pslot_claimer)) {
			if (pslot_claimer != scheddName) {
				dprintf(D_MACHINE, "Job %d.%d is not from the schedd that has pslot %s claimed (%s)\n", cluster_id, proc_id, machine_name.c_str(), pslot_claimer.c_str());
				continue;
			}
		}

        consumption_map_t consumption;
        bool has_cp = cp_supports_policy(*candidate);
        bool cp_sufficient = true;
        if (has_cp) {
            // replace RequestXxx attributes (temporarily) with values derived from
            // the consumption policy, so that Requirements expressions evaluate in a
            // manner consistent with the check on CP resources
            cp_override_requested(request, *candidate, consumption);
            cp_sufficient = cp_sufficient_assets(*candidate, consumption);
        }

		// The candidate offer and request must match.
        // When candidate supports a consumption policy, then resources
        // requested via consumption policy must also be available from
        // the resource
		bool is_a_match = false;
		if (num_threads > 1) {
			is_a_match = cp_sufficient &&
				(par_matches.end() !=
					std::find(par_matches.begin(), par_matches.end(), candidate));
		} else {
			is_a_match = cp_sufficient && IsAMatch(&request, candidate);
		}

        if (has_cp) {
            // put original values back for RequestXxx attributes
            cp_restore_requested(request, consumption);
        }

		candidatePreemptState = NO_PREEMPTION;

		candidateDslotClaims.clear();
		if (!is_a_match && ConsiderPreemption) {
			bool jobWantsMultiMatch = false;
			request.LookupBool(ATTR_WANT_PSLOT_PREEMPTION, jobWantsMultiMatch);
			if (allow_pslot_preemption && jobWantsMultiMatch) {
				// Note: after call to pslotMultiMatch(), iff is_a_match == True,
				// then candidatePreemptState will be updated as well as candidateDslotClaims
				is_a_match = pslotMultiMatch(&request, candidate,submitterName,
					only_for_startdrank, candidateDslotClaims, candidatePreemptState);
			}
		}

		dprintf(D_MACHINE,"Job %d.%d %s match with %s.\n",
		        cluster_id,
		        proc_id,
		        is_a_match ? "does" : "does not",
		        machine_name.c_str());

		if( !is_a_match ) {
				// they don't match; continue
			continue;
		}

		remoteUser.clear();
			// If there is already a preempting user, we need to preempt that user.
			// Otherwise, we need to preempt the user who is running the job.

			// But don't bother with all these lookups if preemption is disabled.
		if (ConsiderPreemption && (candidatePreemptState == NO_PREEMPTION)) {
			if (!candidate->LookupString(ATTR_PREEMPTING_ACCOUNTING_GROUP, remoteUser)) {
				if (!candidate->LookupString(ATTR_PREEMPTING_USER, remoteUser)) {
					if (!candidate->LookupString(ATTR_ACCOUNTING_GROUP, remoteUser)) {
						candidate->LookupString(ATTR_REMOTE_USER, remoteUser);
					}
				}
			}
		}

		// if only_for_startdrank flag is true, check if the offer strictly
		// prefers this request (if we have not already done so, such as in
		// pslotMultimatch).  Since this is the only case we care about
		// when the only_for_startdrank flag is set, if the offer does
		// not prefer it, just continue with the next offer ad....  we can
		// skip all the below logic about preempt for user-priority, etc.
		if ( only_for_startdrank &&
			 candidatePreemptState == NO_PREEMPTION  // have we not already considered preemption?
		   )
		{
			if ( remoteUser.empty() ) {
					// offer does not have a remote user, thus we cannot eval
					// startd rank yet because it does not make sense (the
					// startd has nothing to compare against).
					// So try the next offer...
				dprintf(D_MACHINE,
						"Ignoring %s because it is unclaimed and we are currently "
						"only considering startd rank preemption for job %d.%d.\n",
						machine_name.c_str(), cluster_id, proc_id);
				continue;
			}
			if ( !(EvalExprToBool(rankCondStd, candidate, &request, result) &&
				   result.IsBooleanValue(val) && val) ) {
					// offer does not strictly prefer this request.
					// try the next offer since only_for_statdrank flag is set

				dprintf(D_MACHINE,
						"Job %d.%d does not have higher startd rank than existing job on %s.\n",
						cluster_id, proc_id, machine_name.c_str());
				continue;
			}
			// If we made it here, we have a candidate which strictly prefers
			// this request.  Set the candidatePreemptState properly so that
			// we consider PREEMPTION_RANK down below as we should.
			candidatePreemptState = RANK_PREEMPTION;
		}

		// If there is a remote user, consider preemption if we have not already
		// done so (such as in pslotMultiMatch).
		// Note: we skip this if only_for_startdrank is true since we already
		//       tested above for the only condition we care about.
		if ( (!remoteUser.empty()) &&      // is there a remote user?
			 (!only_for_startdrank) &&
			 (candidatePreemptState == NO_PREEMPTION) // have we not already considered preemption?
		   )
		{
			if( EvalExprToBool(rankCondStd, candidate, &request, result) &&
				result.IsBooleanValue(val) && val ) {
					// offer strictly prefers this request to the one
					// currently being serviced; preempt for rank
				candidatePreemptState = RANK_PREEMPTION;
			} else if (remoteUser != submitterName) {
					// RemoteUser is not the same as the submitting user (or is the
					// same user, but submitting into different groups), so
					// perhaps we can preempt this machine *but* we need to check
					// on two things first
				candidatePreemptState = PRIO_PREEMPTION;
					// (1) we need to make sure that PreemptionReq's hold (i.e.,
					// if the PreemptionReq expression isn't true, dont preempt)
				if (PreemptionReq &&
					!(EvalExprToBool(PreemptionReq,candidate,&request,result) &&
					  result.IsBooleanValue(val) && val) ) {
					rejPreemptForPolicy++;
					dprintf(D_MACHINE,
							"PREEMPTION_REQUIREMENTS prevents job %d.%d from claiming %s.\n",
							cluster_id, proc_id, machine_name.c_str());
					continue;
				}
					// (2) we need to make sure that the machine ranks the job
					// at least as well as the one it is currently running
					// (i.e., rankCondPrioPreempt holds)
				if(!(EvalExprToBool(rankCondPrioPreempt,candidate,&request,result)&&
					 result.IsBooleanValue(val) && val ) ) {
						// machine doesn't like this job as much -- find another
					rejPreemptForRank++;
					dprintf(D_MACHINE,
							"Job %d.%d has lower startd rank than existing job on %s.\n",
							cluster_id, proc_id, machine_name.c_str());
					continue;
				}
			} else {
					// slot is being used by the same user as the job submitter *and* offer doesn't prefer
					// request --- find another machine
				dprintf(D_MACHINE,
						"Job %d.%d is from the same user as the existing job on %s and is not preferred by startd rank\n",
						cluster_id, proc_id, machine_name.c_str());
				continue;
			}
		}

		/* Check that the submitter has suffient user priority to be matched with
		   yet another machine. HOWEVER, do NOT perform this submitter limit
		   check if we are negotiating only for startd rank, since startd rank
		   preemptions should be allowed regardless of user priorities.
	    */
        if ((candidatePreemptState == PRIO_PREEMPTION) && !SubmitterLimitPermits(&request, candidate, limitUsed, submitterLimit, pieLeft)) {
            rejForSubmitterLimit++;
            continue;
        } else if ((candidatePreemptState == NO_PREEMPTION) && !SubmitterLimitPermits(&request, candidate, limitUsedUnclaimed, submitterLimitUnclaimed, pieLeft)) {
            rejForSubmitterLimit++;
            continue;
        }

		if (evaluate_limits_with_match) {
			std::string limits;
			if (EvalString(ATTR_CONCURRENCY_LIMITS, &request, candidate, limits) && rejectForConcurrencyLimits(limits)) {
				continue;
			}
		}

		calculateRanks(request, candidate, candidatePreemptState, candidateRankValue, candidatePreJobRankValue, candidatePostJobRankValue, candidatePreemptRankValue);

		if ( MatchList ) {
			MatchList->add_candidate(
					candidate,
					candidateRankValue,
					candidatePreJobRankValue,
					candidatePostJobRankValue,
					candidatePreemptRankValue,
					candidatePreemptState,
					candidateDslotClaims
					);
		}

		// NOTE!!!   IF YOU CHANGE THE LOGIC OF THE BELOW LEXICOGRAPHIC
		// SORT, YOU MUST ALSO CHANGE THE LOGIC IN METHOD
   		//     Matchmaker::MatchListType::sort_compare() !!!
		// THIS STATE OF AFFAIRS IS TEMPORARY.  ONCE WE ARE CONVINVED
		// THAT THE MatchList LOGIC IS WORKING PROPERLY, AND AUTOCLUSTERS
		// ARE AUTOMATIC, THEN THE MatchList SORTING WILL ALWAYS BE USED
		// AND THE LEXICOGRAPHIC SORT BELOW WILL BE REMOVED.
		// - Todd Tannenbaum <tannenba@cs.wisc.edu> 10/2004
		// ----------------------------------------------------------
		// the quality of a match is determined by a lexicographic sort on
		// the following values, but more is better for each component
		//  1. negotiator pre job rank
		//  1. job rank of offer
		//  2. negotiator post job rank
		//	3. preemption state (2=no preempt, 1=rank-preempt, 0=prio-preempt)
		//  4. preemption rank (if preempting)

		newBestFound = false;
		if(candidatePreJobRankValue < bestPreJobRankValue);
		else if(candidatePreJobRankValue > bestPreJobRankValue) {
			newBestFound = true;
		}
		else if(candidateRankValue < bestRankValue);
		else if(candidateRankValue > bestRankValue) {
			newBestFound = true;
		}
		else if(candidatePostJobRankValue < bestPostJobRankValue);
		else if(candidatePostJobRankValue > bestPostJobRankValue) {
			newBestFound = true;
		}
		else if(candidatePreemptState < bestPreemptState);
		else if(candidatePreemptState > bestPreemptState) {
			newBestFound = true;
		}
		//NOTE: if NO_PREEMPTION, PreemptRank is a constant
		else if(candidatePreemptRankValue < bestPreemptRankValue);
		else if(candidatePreemptRankValue > bestPreemptRankValue) {
			newBestFound = true;
		}

		if( newBestFound || !bestSoFar ) {
			bestSoFar = candidate;
			bestPreJobRankValue = candidatePreJobRankValue;
			bestRankValue = candidateRankValue;
			bestPostJobRankValue = candidatePostJobRankValue;
			bestPreemptState = candidatePreemptState;
			bestPreemptRankValue = candidatePreemptRankValue;
			bestDslotClaims = candidateDslotClaims;
		}

		if (m_staticRanks) {
			double weight = 1.0;
			candidate->LookupFloat(ATTR_SLOT_WEIGHT, weight);
			allocatedWeight += weight;
			if (allocatedWeight > submitterLimit) {
				break;
			}
		}
	}

	if ( MatchList ) {
		MatchList->set_diagnostics(
			rejForNetwork,
			rejForNetworkShare,
		    rejForConcurrencyLimit,
			rejPreemptForPrio,
			rejPreemptForPolicy,
		   	rejPreemptForRank,
			rejForSubmitterLimit,
			rejForSubmitterCeiling);

			// only bother sorting if there is more than one entry
		if ( MatchList->length() > 1 ) {
			dprintf(D_FULLDEBUG,"Start of sorting MatchList (len=%d)\n",
				MatchList->length());
			MatchList->sort();
			dprintf(D_FULLDEBUG,"Finished sorting MatchList\n");
		}
		// Pop top candidate off the list to hand out as best match
		bestSoFar = MatchList->pop_candidate(bestDslotClaims);
	}

	if ( bestSoFar && !bestDslotClaims.empty() ) {
		bestSoFar->Assign( "PreemptDslotClaims", bestDslotClaims );
	}
	// this is the best match
	return bestSoFar;
}

void Matchmaker::
insertNegotiatorMatchExprs( std::vector<ClassAd *> &cal )
{
	for (ClassAd *ad: cal) {
		insertNegotiatorMatchExprs( ad );
	}
}

void Matchmaker::
calculateRanks(ClassAd &request,
               ClassAd *candidate,
               PreemptState candidatePreemptState,
               double &candidateRankValue,
               double &candidatePreJobRankValue,
               double &candidatePostJobRankValue,
               double &candidatePreemptRankValue
              )
{
	if (m_staticRanks) {
		RanksMapType::iterator it = ranksMap.find(candidate);

		// if we have it, return it
		if (it != ranksMap.end()) {
			struct JobRanks ranks = (*it).second;
			candidatePreJobRankValue = ranks.PreJobRankValue;
			candidatePostJobRankValue = ranks.PostJobRankValue;
			candidatePreemptRankValue = ranks.PreemptRankValue;
			candidateRankValue = 1.0; // Assume fixed
			return;
		}
	}

	candidatePreJobRankValue = EvalNegotiatorMatchRank(
		"NEGOTIATOR_PRE_JOB_RANK",NegotiatorPreJobRank,
		request, candidate);

	// calculate the request's rank of the candidate
	double tmp;
	if(!EvalFloat(ATTR_RANK, &request, candidate, tmp)) {
		tmp = 0.0;
	}
	candidateRankValue = tmp;

	candidatePostJobRankValue = EvalNegotiatorMatchRank(
		"NEGOTIATOR_POST_JOB_RANK",NegotiatorPostJobRank,
		request, candidate);

	candidatePreemptRankValue = -(FLT_MAX);
	if(candidatePreemptState != NO_PREEMPTION) {
		candidatePreemptRankValue = EvalNegotiatorMatchRank(
			"PREEMPTION_RANK",PreemptionRank,
			request, candidate);
	}

	if (m_staticRanks) {
		// only get here on cache miss
		struct JobRanks ranks;
		ranks.PreJobRankValue = candidatePreJobRankValue;
		ranks.PostJobRankValue = candidatePostJobRankValue;
		ranks.PreemptRankValue = candidatePreemptRankValue;
		ranksMap.insert(std::make_pair(candidate, ranks));
	}
}

	// NOTE NOTE: this assumes that p-slots are not being preempted.
bool Matchmaker::
returnPslotToMatchList(ClassAd &request, ClassAd *offer)
{
	if (!MatchList) {return false;}

	double candidateRankValue, candidatePreJobRankValue, candidatePostJobRankValue, candidatePreemptRankValue;
	calculateRanks(request, offer, NO_PREEMPTION, candidateRankValue, candidatePreJobRankValue, candidatePostJobRankValue, candidatePreemptRankValue);

	return MatchList->insert_candidate(
		offer,
		candidateRankValue,
		candidatePreJobRankValue,
		candidatePostJobRankValue,
		candidatePreemptRankValue,
		NO_PREEMPTION
	);
}

void Matchmaker::
insertNegotiatorMatchExprs(ClassAd *ad)
{
	ASSERT(ad);

	for (const auto& [expr_name, expr_value]: NegotiatorMatchExprs) {
		ad->AssignExpr(expr_name, expr_value.c_str());
	}
}

/*
Warning: scheddAddr may not be the actual address we'll use to contact the
schedd, thanks to CCB.  It _is_ suitable for use as a unique identifier, for
display to the user, or for calls to sockCache->invalidateSock.
*/
int Matchmaker::
matchmakingProtocol (ClassAd &request, ClassAd *offer,
						ClaimIdHash &claimIds, Sock *sock,
					    const char* submitterName, const char* scheddAddr)
{
	int  cluster = 0;
	int proc = 0;
	std::string startdAddr;
	std::string remoteUser;
	char accountingGroup[256];
	char remoteOwner[256];
	std::string startdName;
	bool send_failed;
	bool want_claiming = true;
	ExprTree *savedRequirements;
	int length;
	char *tmp;

	// these will succeed
	request.LookupInteger (ATTR_CLUSTER_ID, cluster);
	request.LookupInteger (ATTR_PROC_ID, proc);

	bool offline = false;
	offer->LookupBool(ATTR_OFFLINE,offline);
	if( offline ) {
		want_claiming = false;
		RegisterAttemptedOfflineMatch( &request, offer, scheddAddr );
	}
	else {
			// see if offer supports claiming or not
		offer->LookupBool(ATTR_WANT_CLAIMING,want_claiming);
	}

	// these should too, but may not
	if (!offer->LookupString (ATTR_STARTD_IP_ADDR, startdAddr)		||
		!offer->LookupString (ATTR_NAME, startdName))
	{
		// fatal error if we need claiming
		if ( want_claiming ) {
			dprintf (D_ALWAYS, "      Could not lookup %s and %s\n",
					ATTR_NAME, ATTR_STARTD_IP_ADDR);
			return MM_BAD_MATCH;
		}
	}

	// find the startd's claim id from the private ad
	// claim_id and all_claim_ids will have the primary claim id.
	// For pslot preemption, all_claim_ids will also have the claim ids
	// of the dslots being preempted.
	std::string claim_id;
	std::string all_claim_ids;
	std::string dslotDesc;
    ClaimIdHash::iterator claimset = claimIds.end();
	if (want_claiming) {
		std::string key = startdName;
        key += startdAddr;
        claimset = claimIds.find(key);
        if ((claimIds.end() == claimset) || (claimset->second.size() < 1)) {
            dprintf(D_ALWAYS,"      %s has no claim id\n", startdName.c_str());
            return MM_BAD_MATCH;
        }
		claim_id = *(claimset->second.begin());
		all_claim_ids = claim_id;

		// If there are extra preempting dslot claims, hand them out too
		std::string extraClaims;
		if (offer->LookupString("PreemptDslotClaims", extraClaims)) {
			all_claim_ids += " ";
			all_claim_ids += extraClaims;
			size_t numExtraClaims = std::count(extraClaims.begin(), extraClaims.end(), ' ');
			formatstr(dslotDesc, "%zu dslots", numExtraClaims);
			offer->Delete("PreemptDslotClaims");
		}

	} else {
		// Claiming is *not* desired
		claim_id = "null";
		all_claim_ids = claim_id;
	}

	classad::MatchClassAd::UnoptimizeAdForMatchmaking( offer );

	savedRequirements = NULL;
	length = strlen("Saved") + strlen(ATTR_REQUIREMENTS) + 2;
	tmp = (char *)malloc(length);
	ASSERT( tmp != NULL );
	snprintf(tmp, length, "Saved%s", ATTR_REQUIREMENTS);
	savedRequirements = offer->LookupExpr(tmp);
	free(tmp);
	if(savedRequirements != NULL) {
		const char *savedReqStr = ExprTreeToString(savedRequirements);
		offer->AssignExpr( ATTR_REQUIREMENTS, savedReqStr );
		dprintf( D_ALWAYS, "Inserting %s = %s into the ad\n",
				ATTR_REQUIREMENTS, savedReqStr ? savedReqStr : "" );
	}	

		// Stash the Concurrency Limits in the offer, they are part of
		// what's being provided to the request after all. The limits
		// will be available to the Accountant when the match is added
		// and also to the Schedd when considering to reuse a
		// claim. Both are key, first so the Accountant can properly
		// recreate its state on startup, and second so the Schedd has
		// the option of checking if a claim should be reused for a
		// job incase it has different limits. The second part is
		// because the limits are not in the Requirements.
		//
		// NOTE: Because the Concurrency Limits should be available to
		// the Schedd, they must be stashed before PERMISSION_AND_AD
		// is sent.
	std::string limits;
	if (EvalString(ATTR_CONCURRENCY_LIMITS, &request, offer, limits)) {
		lower_case(limits);
		offer->Assign(ATTR_MATCHED_CONCURRENCY_LIMITS, limits);
	} else {
		offer->Delete(ATTR_MATCHED_CONCURRENCY_LIMITS);
	}

    // these propagate into the slot ad in the schedd match rec, and from there eventually to the claim
    // structures in the startd:
    CopyAttribute(ATTR_REMOTE_GROUP, *offer, ATTR_SUBMITTER_GROUP, request);
    CopyAttribute(ATTR_REMOTE_NEGOTIATING_GROUP, *offer, ATTR_SUBMITTER_NEGOTIATING_GROUP, request);
    CopyAttribute(ATTR_REMOTE_AUTOREGROUP, *offer, ATTR_SUBMITTER_AUTOREGROUP);

	// insert cluster and proc from the request into the offer; this is
	// used by schedd_negotiate.cpp when resource request lists are being used
	offer->Assign(ATTR_RESOURCE_REQUEST_CLUSTER,cluster);
	offer->Assign(ATTR_RESOURCE_REQUEST_PROC,proc);

	// ---- real matchmaking protocol begins ----
	// 1.  contact the startd
	if (want_claiming && want_inform_startd) {
			// The following sends a message to the startd to inform it
			// of the match.  Although it is a UDP message, it still may
			// block, because if there is no cached security session,
			// a TCP connection is created.  Therefore, the following
			// handler supports the nonblocking interface to startCommand.

		NotifyStartdOfMatchHandler *h =
			new NotifyStartdOfMatchHandler(
				startdName.c_str(),startdAddr.c_str(),NegotiatorTimeout,
				claim_id.c_str(),want_nonblocking_startd_contact);

		if(!h->startCommand()) {
			return MM_BAD_MATCH;
		}
	}	// end of if want_claiming

	// 3.  send the match and all_claim_ids to the schedd
	sock->encode();
	send_failed = false;	

	dprintf(D_FULLDEBUG,
		"      Sending PERMISSION, claim id, startdAd to schedd\n");
	if (!sock->put(PERMISSION_AND_AD) ||
		!sock->put_secret(all_claim_ids.c_str()) ||
		!putClassAd(sock, *offer, PUT_CLASSAD_NO_PRIVATE)	||	// send startd ad to schedd
		!sock->end_of_message())
	{
			send_failed = true;
	}

	if ( send_failed )
	{
		ClaimIdParser cidp(claim_id.c_str());
		dprintf (D_ALWAYS, "      Could not send PERMISSION\n" );
		dprintf( D_FULLDEBUG, "      (Claim ID is \"%s\")\n", cidp.publicClaimId());
		sockCache->invalidateSock( scheddAddr );
		return MM_ERROR;
	}

	if (offer->LookupString(ATTR_REMOTE_USER, remoteOwner, sizeof(remoteOwner)) == 0) {
		strcpy(remoteOwner, "none");
	}
	if (offer->LookupString(ATTR_ACCOUNTING_GROUP, accountingGroup, sizeof(accountingGroup))) {
		formatstr(remoteUser,"%s (%s=%s)",
			remoteOwner,ATTR_ACCOUNTING_GROUP,accountingGroup);
	} else {
		remoteUser = remoteOwner;
	}
	
	if (dslotDesc.length() > 0) {
		remoteUser = dslotDesc;
	}

	if (offer->LookupString (ATTR_STARTD_IP_ADDR, startdAddr) == 0) {
		startdAddr = "<0.0.0.0:0>";
	}
	dprintf(D_ALWAYS|D_MATCH, "      Matched %d.%d %s %s preempting %s %s %s%s\n",
			cluster, proc, submitterName, scheddAddr, remoteUser.c_str(),
			startdAddr.c_str(), startdName.c_str(),
			offline ? " (offline)" : "");

    // At this point we're offering this match as good.
    // We don't offer a claim more than once per cycle, so remove it
    // from the set of available claims.
    if (claimset != claimIds.end()) {
        claimset->second.erase(claim_id);
    }

    if (cp_supports_policy(*offer)) {
        // Stash match cost here for the accountant.
        // At this point the match is fully vetted so we can also deduct
        // the resource assets.
        offer->Assign(CP_MATCH_COST, cp_deduct_assets(request, *offer));

		if (MatchList)
		{
			consumption_map_t consumption;
			cp_override_requested(request, *offer, consumption);
			bool is_a_match = cp_sufficient_assets(*offer, consumption) && IsAMatch(&request, offer);
			cp_restore_requested(request, consumption);
				// NOTE: returnPslotToMatchList only works for p-slots; assumes they are not preempted.
			if (is_a_match && !returnPslotToMatchList(request, offer))
			{
				dprintf(D_FULLDEBUG, "Unable to return still-valid offer to the match list.\n");
			}
		}
    }

    // 4. notifiy the accountant
	dprintf(D_FULLDEBUG,"      Notifying the accountant\n");
	accountant.AddMatch(submitterName, offer);

	// done
	dprintf (D_ALWAYS, "      Successfully matched with %s%s\n",
			 startdName.c_str(),
			 offline ? " (offline)" : "");
	return MM_GOOD_MATCH;
}

void
Matchmaker::calculateSubmitterLimit(
	const std::string &submitterName,
	char const *groupAccountingName,
	float groupQuota,
	float groupusage,
	double maxPrioValue,
	double maxAbsPrioValue,
	double normalFactor,
	double normalAbsFactor,
	double slotWeightTotal,
	bool isFloorRound,
		/* result parameters: */
	double &submitterLimit,
    double& submitterLimitUnclaimed,
	double &submitterUsage,
	double &submitterShare,
	double &submitterAbsShare,
	double &submitterPrio,
	double &submitterPrioFactor)
{
		// calculate the percentage of machines that this schedd can use
	submitterPrio = accountant.GetPriority ( submitterName );
	submitterUsage = accountant.GetWeightedResourcesUsed( submitterName );
	submitterShare = maxPrioValue/(submitterPrio*normalFactor);

	if ( param_boolean("NEGOTIATOR_IGNORE_USER_PRIORITIES",false) ) {
		submitterLimit = DBL_MAX;
	} else {
		submitterLimit = (submitterShare*slotWeightTotal)-submitterUsage;
	}
	if( submitterLimit < 0 ) {
		submitterLimit = 0.0;
	}

    submitterLimitUnclaimed = submitterLimit;
	if (groupAccountingName) {
		float maxAllowed = groupQuota - groupusage;
		dprintf(D_FULLDEBUG, "   maxAllowed= %g   groupQuota= %g   groupusage=  %g\n", maxAllowed, groupQuota, groupusage);
		if (maxAllowed < 0) maxAllowed = 0.0;
		if (submitterLimitUnclaimed > maxAllowed) {
			submitterLimitUnclaimed = maxAllowed;
		}
	}
    if (!ConsiderPreemption) submitterLimit = submitterLimitUnclaimed;

		// calculate this schedd's absolute fair-share for allocating
		// resources other than CPUs (like network capacity and licenses)
	submitterPrioFactor = accountant.GetPriorityFactor ( submitterName );
	submitterAbsShare =
		maxAbsPrioValue/(submitterPrioFactor*normalAbsFactor);

		// if we are in the "floor round", only give up to
		// the floor number of resources configured for this
		// submitter.  Might get more in subseqent rounds
		// but the floor round is to try to get everyone up
		// to their floor.
	if (isFloorRound) {
		double floor = accountant.GetFloor(submitterName);
		submitterLimit = std::min(floor, submitterLimit);
		dprintf(D_FULLDEBUG, "   Floor Round: %g\n", floor);
	}
}

void
Matchmaker::calculatePieLeft(
	std::vector<ClassAd *> &submitterAds,
	char const *groupAccountingName,
	float groupQuota,
	float groupusage,
	double maxPrioValue,
	double maxAbsPrioValue,
	double normalFactor,
	double normalAbsFactor,
	double slotWeightTotal,
		/* result parameters: */
	double &pieLeft)
{

		// Calculate sum of submitterLimits in this spin of the pie.
	pieLeft = 0;

	for (ClassAd *submitter: submitterAds) {
		double submitterShare = 0.0;
		double submitterAbsShare = 0.0;
		double submitterPrio = 0.0;
		double submitterPrioFactor = 0.0;
		std::string submitterName;
		double submitterLimit = 0.0;
        double submitterLimitUnclaimed = 0.0;
		double submitterUsage = 0.0;

		submitter->LookupString( ATTR_NAME, submitterName );

		calculateSubmitterLimit(
			submitterName,
			groupAccountingName,
			groupQuota,
			groupusage,
			maxPrioValue,
			maxAbsPrioValue,
			normalFactor,
			normalAbsFactor,
			slotWeightTotal,
			false, /* is floor round */
				/* result parameters: */
			submitterLimit,
            submitterLimitUnclaimed,
			submitterUsage,
			submitterShare,
			submitterAbsShare,
			submitterPrio,
			submitterPrioFactor);

        submitter->Assign("SubmitterStarvation", starvation_ratio(submitterUsage, submitterUsage+submitterLimit));
			
		pieLeft += submitterLimit;
	}
}

void Matchmaker::
calculateNormalizationFactor (std::vector<ClassAd *> &submitterAds,
							  double &max, double &normalFactor,
							  double &maxAbs, double &normalAbsFactor)
{
	// find the maximum of the priority values (i.e., lowest priority)
	max = maxAbs = DBL_MIN;
	for (ClassAd* ad: submitterAds) {
		// this will succeed (comes from collector)
		std::string subname;
		ad->LookupString(ATTR_NAME, subname);
		double prio = accountant.GetPriority(subname);
		if (prio > max) max = prio;
		double prioFactor = accountant.GetPriorityFactor(subname);
		if (prioFactor > maxAbs) maxAbs = prioFactor;
	}

	// calculate the normalization factor, i.e., sum of the (max/scheddprio)
	// also, do not factor in ads with the same ATTR_NAME more than once -
	// ads with the same ATTR_NAME signify the same user submitting from multiple
	// machines.
	std::set<std::string> names;
	normalFactor = 0.0;
	normalAbsFactor = 0.0;
	for (ClassAd* ad: submitterAds) {
		std::string subname;
		ad->LookupString(ATTR_NAME, subname);
        std::pair<std::set<std::string>::iterator, bool> r = names.insert(subname);
        // Only count each submitter once
        if (!r.second) continue;

		double prio = accountant.GetPriority(subname);
		normalFactor += max/prio;
		double prioFactor = accountant.GetPriorityFactor(subname);
		normalAbsFactor += maxAbs/prioFactor;
	}
}


void Matchmaker::
addRemoteUserPrios(std::vector<ClassAd *> &cal )
{
	if (!ConsiderPreemption) {
			// Hueristic - no need to take the time to populate ad with
			// accounting information if no preemption is to be considered.
		return;
	}

	for (ClassAd *ad: cal) {
		addRemoteUserPrios(ad);
	}
}

void Matchmaker::
addRemoteUserPrios( ClassAd	*ad )
{	
	std::string	remoteUser;
	std::string buffer,buffer1,buffer2,buffer3;
	std::string slot_prefix;
	std::string expr;
	std::string expr_buffer;
	double	prio;
	int     total_slots, i;
	double     preemptingRank;
	double temp_groupQuota, temp_groupUsage;
	std::string temp_groupName;

		// If there is a preempting user, use that for computing remote user prio.
		// Otherwise, use the current user.
	if( ad->LookupString( ATTR_PREEMPTING_ACCOUNTING_GROUP , remoteUser ) ||
		ad->LookupString( ATTR_PREEMPTING_USER , remoteUser ) ||
		ad->LookupString( ATTR_ACCOUNTING_GROUP , remoteUser ) ||
		ad->LookupString( ATTR_REMOTE_USER , remoteUser ) )
	{
		prio = (float) accountant.GetPriority( remoteUser );
		ad->Assign(ATTR_REMOTE_USER_PRIO, prio);
		formatstr(expr, "%s(%s)", RESOURCES_IN_USE_BY_USER_FN_NAME, QuoteAdStringValue(remoteUser.c_str(),expr_buffer));
		ad->AssignExpr(ATTR_REMOTE_USER_RESOURCES_IN_USE,expr.c_str());
		if (getGroupInfoFromUserId(remoteUser.c_str(), temp_groupName, temp_groupQuota, temp_groupUsage)) {
			// this is a group, so enter group usage info
            ad->Assign(ATTR_REMOTE_GROUP, temp_groupName);
			formatstr(expr, "%s(%s)", RESOURCES_IN_USE_BY_USERS_GROUP_FN_NAME, QuoteAdStringValue(remoteUser.c_str(),expr_buffer));
			ad->AssignExpr(ATTR_REMOTE_GROUP_RESOURCES_IN_USE,expr.c_str());
			ad->Assign(ATTR_REMOTE_GROUP_QUOTA,temp_groupQuota);
		}
	}
	if( ad->LookupFloat( ATTR_PREEMPTING_RANK, preemptingRank ) ) {
			// There is already a preempting claim (waiting for the previous
			// claim to retire), so set current rank to the preempting
			// rank, since any new preemption must trump the
			// current preempter.
		ad->Assign(ATTR_CURRENT_RANK, preemptingRank);
	}
		
	char* resource_prefix = param("STARTD_RESOURCE_PREFIX");
	if (!resource_prefix) {
		resource_prefix = strdup("slot");
	}
	total_slots = 0;
	if (!ad->LookupInteger(ATTR_TOTAL_SLOTS, total_slots)) {
		total_slots = 0;
	}
		// The for-loop below publishes accounting information about each slot
		// into each other slot.  This is relatively computationally expensive,
		// especially for startds that manage a lot of slots, and 99% of the world
		// doesn't care.  So we only do this if knob
		// NEGOTIATOR_CROSS_SLOT_PRIOS is explicitly set to True.
		// This won't fire if total_slots is still 0...
	for(i = 1; PublishCrossSlotPrios && i <= total_slots; i++) {
		formatstr(slot_prefix, "%s%d_", resource_prefix, i);
		buffer = slot_prefix + ATTR_PREEMPTING_ACCOUNTING_GROUP;
		buffer1 = slot_prefix + ATTR_PREEMPTING_USER;
		buffer2 = slot_prefix + ATTR_ACCOUNTING_GROUP;
		buffer3 = slot_prefix + ATTR_REMOTE_USER;
			// If there is a preempting user, use that for computing remote user prio.
		if( ad->LookupString( buffer, remoteUser ) ||
			ad->LookupString( buffer1, remoteUser ) ||
			ad->LookupString( buffer2, remoteUser ) ||
			ad->LookupString( buffer3, remoteUser ) )
		{
				// If there is a user on that VM, stick that user's priority
				// information into the ad	
			prio = (float) accountant.GetPriority( remoteUser );
			buffer = slot_prefix + ATTR_REMOTE_USER_PRIO;
			ad->Assign(buffer, prio);
			buffer = slot_prefix + ATTR_REMOTE_USER_RESOURCES_IN_USE;
			formatstr(expr, "%s(%s)", RESOURCES_IN_USE_BY_USER_FN_NAME, QuoteAdStringValue(remoteUser.c_str(),expr_buffer));
			ad->AssignExpr(buffer, expr.c_str());
			if (getGroupInfoFromUserId(remoteUser.c_str(), temp_groupName, temp_groupQuota, temp_groupUsage)) {
				// this is a group, so enter group usage info
				buffer = slot_prefix + ATTR_REMOTE_GROUP;
				ad->Assign( buffer, temp_groupName );
				buffer = slot_prefix + ATTR_REMOTE_GROUP_RESOURCES_IN_USE;
				formatstr(expr, "%s(%s)", RESOURCES_IN_USE_BY_USERS_GROUP_FN_NAME, QuoteAdStringValue(remoteUser.c_str(),expr_buffer));
				ad->AssignExpr( buffer, expr.c_str() );
				buffer = slot_prefix + ATTR_REMOTE_GROUP_QUOTA;
				ad->Assign( buffer, temp_groupQuota );
			}
		}	
	}
	free( resource_prefix );
}

void
Matchmaker::findBelowFloorSubmitters(std::vector<ClassAd *> &submitterAds, std::vector<ClassAd *> &submittersBelowFloor) {
	for (ClassAd *submitterAd : submitterAds) {
		std::string submitterName;
		submitterAd->LookupString(ATTR_NAME, submitterName);
		double floor = accountant.GetFloor(submitterName);
		double usage = accountant.GetWeightedResourcesUsed(submitterName);

		// If any floor is defined, and we're below it, we get into the
		// special floor-only round
		if ((floor > 0.0) && (usage < floor)) {
			submittersBelowFloor.push_back(submitterAd);
		}
	}
	return;
}

void Matchmaker::
reeval(ClassAd *ad)
{
	int cur_matches;
	MapEntry *oldAdEntry = NULL;
	
	cur_matches = 0;
	ad->LookupInteger("CurMatches", cur_matches);

	std::string adID = MachineAdID(ad);
	stashedAds->lookup( adID, oldAdEntry);
		
	cur_matches++;
	ad->Assign("CurMatches", cur_matches);
	if(oldAdEntry) {
		delete(oldAdEntry->oldAd);
		oldAdEntry->oldAd = new ClassAd(*ad);
	}
}

Matchmaker::MatchListType::
MatchListType(int maxlen)
{
	ASSERT(maxlen > 0);
	AdListArray = new AdListEntry[maxlen];
	ASSERT(AdListArray);
	adListMaxLen = maxlen;
	already_sorted = false;
	adListLen = 0;
	adListHead = 0;
	m_rejForNetwork = 0;
	m_rejForNetworkShare = 0;
	m_rejForConcurrencyLimit = 0;
	m_rejPreemptForPrio = 0;
	m_rejPreemptForPolicy = 0;
	m_rejPreemptForRank = 0;
	m_rejForSubmitterLimit = 0;
	m_rejForSubmitterCeiling = 0;
	m_submitterLimit = 0.0f;
}

Matchmaker::MatchListType::
~MatchListType()
{
	if (AdListArray) {
		delete [] AdListArray;
	}
}


#if 0
Matchmaker::AdListEntry* Matchmaker::MatchListType::
peek_candidate()
{
	ClassAd* candidate = NULL;
	int temp_adListHead = adListHead;

	while ( temp_adListHead < adListLen && !candidate ) {
		candidate = AdListArray[temp_adListHead].ad;
		temp_adListHead++;
	}

	if ( candidate ) {
		temp_adListHead--;
		ASSERT( temp_adListHead >= 0 );
		return AdListArray[temp_adListHead];
	} else {
		return NULL;
	}
}
#endif

ClassAd* Matchmaker::MatchListType::
pop_candidate(std::string &dslot_claims)
{
	ClassAd* candidate = NULL;

	while ( adListHead < adListLen && !candidate ) {
		candidate = AdListArray[adListHead].ad;
		if ( candidate ) {
			dslot_claims = AdListArray[adListHead].DslotClaims;
		}
		adListHead++;
	}

	return candidate;
}

// This method assumes the ad being inserted was just popped from the
// top of the list. Specicifically, we assume there is room at the top
// of the list for insertion, the list is sorted, and the ad being
// inserted is likely to sort toward the top of the list.
bool Matchmaker::MatchListType::
insert_candidate(ClassAd * candidate,
	double candidateRankValue,
	double candidatePreJobRankValue,
	double candidatePostJobRankValue,
	double candidatePreemptRankValue,
	PreemptState candidatePreemptState)
{
	if (adListHead == 0) {return false;}
	adListHead--;
	AdListEntry new_entry;
	new_entry.ad = candidate;
	new_entry.RankValue = candidateRankValue;
	new_entry.PreJobRankValue = candidatePreJobRankValue;
	new_entry.PostJobRankValue = candidatePostJobRankValue;
	new_entry.PreemptRankValue = candidatePreemptRankValue;
	new_entry.PreemptStateValue = candidatePreemptState;
	new_entry.DslotClaims.clear();

		// Hand-rolled insertion sort; as the list was previously sorted,
		// we know this will be O(n).
	int insert_idx = adListHead;
	while ( insert_idx < adListLen - 1 )
	{
		if ( sort_compare( new_entry, AdListArray[insert_idx + 1])) {
			AdListArray[insert_idx] = AdListArray[insert_idx + 1];
			insert_idx++;
		} else {
			break;
		}
	}
	AdListArray[insert_idx] = new_entry;

	return true;
}

bool Matchmaker::MatchListType::
cache_still_valid(ClassAd &request, ExprTree *preemption_req, ExprTree *preemption_rank,
				  bool preemption_req_unstable, bool preemption_rank_unstable)
{
	AdListEntry* next_entry = NULL;

	if ( !preemption_req_unstable && !preemption_rank_unstable ) {
		return true;
	}

	// Set next_entry to be a "peek" at the next entry on
	// our cached match list, i.e. don't actually pop it off our list.
	{
		ClassAd* candidate = NULL;
		int temp_adListHead = adListHead;

		while ( temp_adListHead < adListLen && !candidate ) {
			candidate = AdListArray[temp_adListHead].ad;
			temp_adListHead++;
		}

		if ( candidate ) {
			temp_adListHead--;
			ASSERT( temp_adListHead >= 0 );
			next_entry =  &AdListArray[temp_adListHead];
		} else {
			next_entry = NULL;
		}
	}

	if ( preemption_req_unstable )
	{
		if ( !next_entry ) {
			return false;
		}
		
		if ( next_entry->PreemptStateValue == PRIO_PREEMPTION ) {
			classad::Value result;
			bool val;
			if (preemption_req &&
				!(EvalExprToBool(preemption_req,next_entry->ad,&request,result) &&
				  result.IsBooleanValue(val) && val) )
			{
				dprintf(D_FULLDEBUG,
					"Cache invalidated due to preemption_requirements\n");
				return false;
			}
		}
	}

	if ( next_entry && preemption_rank_unstable )
	{		
		if( next_entry->PreemptStateValue != NO_PREEMPTION) {
			double candidatePreemptRankValue = -(FLT_MAX);
			candidatePreemptRankValue = EvalNegotiatorMatchRank(
					"PREEMPTION_RANK",preemption_rank,request,next_entry->ad);
			if ( candidatePreemptRankValue != next_entry->PreemptRankValue ) {
				// ranks don't match ....  now what?
				// ideally we would just want to resort the cache, but for now
				// we do the safest thing - just invalidate the cache.
				dprintf(D_FULLDEBUG,
					"Cache invalidated due to preemption_rank\n");
				return false;
				
			}
		}
	}

	return true;
}


void Matchmaker::MatchListType::
get_diagnostics(int & rejForNetwork,
					int & rejForNetworkShare,
					int & rejForConcurrencyLimit,
					int & rejPreemptForPrio,
					int & rejPreemptForPolicy,
				    int & rejPreemptForRank,
				    int & rejForSubmitterLimit,
				    int & rejForSubmitterCeiling) const
{
	rejForNetwork = m_rejForNetwork;
	rejForNetworkShare = m_rejForNetworkShare;
	rejForConcurrencyLimit = m_rejForConcurrencyLimit;
	rejPreemptForPrio = m_rejPreemptForPrio;
	rejPreemptForPolicy = m_rejPreemptForPolicy;
	rejPreemptForRank = m_rejPreemptForRank;
	rejForSubmitterLimit = m_rejForSubmitterLimit;
	rejForSubmitterCeiling = m_rejForSubmitterCeiling;
}

void Matchmaker::MatchListType::
set_diagnostics(int rejForNetwork,
					int rejForNetworkShare,
					int rejForConcurrencyLimit,
					int rejPreemptForPrio,
					int rejPreemptForPolicy,
				    int rejPreemptForRank,
				    int rejForSubmitterLimit,
				    int rejForSubmitterCeiling)
{
	m_rejForNetwork = rejForNetwork;
	m_rejForNetworkShare = rejForNetworkShare;
	m_rejForConcurrencyLimit = rejForConcurrencyLimit;
	m_rejPreemptForPrio = rejPreemptForPrio;
	m_rejPreemptForPolicy = rejPreemptForPolicy;
	m_rejPreemptForRank = rejPreemptForRank;
	m_rejForSubmitterLimit = rejForSubmitterLimit;
	m_rejForSubmitterCeiling = rejForSubmitterCeiling;
}

void Matchmaker::MatchListType::
add_candidate(ClassAd * candidate,
					double candidateRankValue,
					double candidatePreJobRankValue,
					double candidatePostJobRankValue,
					double candidatePreemptRankValue,
					PreemptState candidatePreemptState,
					const std::string &candidateDslotClaims)
{
	ASSERT(AdListArray);
	ASSERT(adListLen < adListMaxLen);  // don't write off end of array!

	AdListArray[adListLen].ad = candidate;
	AdListArray[adListLen].RankValue = candidateRankValue;
	AdListArray[adListLen].PreJobRankValue = candidatePreJobRankValue;
	AdListArray[adListLen].PostJobRankValue = candidatePostJobRankValue;
	AdListArray[adListLen].PreemptRankValue = candidatePreemptRankValue;
	AdListArray[adListLen].PreemptStateValue = candidatePreemptState;
	AdListArray[adListLen].DslotClaims = candidateDslotClaims;

    // This hack allows me to avoid mucking with the pseudo-que-like semantics of MatchListType,
    // which ought to be replaced with something cleaner like std::deque<AdListEntry>
    if (NULL != AdListArray[adListLen].ad) {
        AdListArray[adListLen].ad->Assign(ATTR_PREEMPT_STATE_, int(candidatePreemptState));
    }

	adListLen++;
}


void Matchmaker::DeleteMatchList()
{
	// Delete our MatchList
	if( MatchList ) {
		delete MatchList;
		MatchList = NULL;
	}
	cachedAutoCluster = -1;
	if ( cachedName ) {
		free(cachedName);
		cachedName = NULL;
	}
	if ( cachedAddr ) {
		free(cachedAddr);
		cachedAddr = NULL;
	}

	// And anytime we clear out our MatchList, we also want to restore
	// any pslot ads that got mutated as part of pslot preemption back to their
	// original state (i.e. restore them back to how we got them from the collector).
	for (auto i = unmutatedSlotAds.begin(); i != unmutatedSlotAds.end(); i++) {
		(i->first)->Update(*(i->second));  // restore backup ad (i.second) attrs into machine ad (i.first)
		delete i->second;  // deallocate backup ad (i.second)
	}
	unmutatedSlotAds.clear();
}

bool Matchmaker::MatchListType::
sort_compare(const AdListEntry &Elem1, const AdListEntry &Elem2)
{
	const double candidateRankValue = Elem1.RankValue;
	const double candidatePreJobRankValue = Elem1.PreJobRankValue;
	const double candidatePostJobRankValue = Elem1.PostJobRankValue;
	const double candidatePreemptRankValue = Elem1.PreemptRankValue;
	const PreemptState candidatePreemptState = Elem1.PreemptStateValue;

	const double bestRankValue = Elem2.RankValue;
	const double bestPreJobRankValue = Elem2.PreJobRankValue;
	const double bestPostJobRankValue = Elem2.PostJobRankValue;
	const double bestPreemptRankValue = Elem2.PreemptRankValue;
	const PreemptState bestPreemptState = Elem2.PreemptStateValue;

	if ( candidateRankValue == bestRankValue &&
		 candidatePreJobRankValue == bestPreJobRankValue &&
		 candidatePostJobRankValue == bestPostJobRankValue &&
		 candidatePreemptRankValue == bestPreemptRankValue &&
		 candidatePreemptState == bestPreemptState )
	{
		return false;
	}

	// the quality of a match is determined by a lexicographic sort on
	// the following values, but more is better for each component
	//  1. negotiator pre job rank
	//  1. job rank of offer
	//  2. negotiator post job rank
	//	3. preemption state (2=no preempt, 1=rank-preempt, 0=prio-preempt)
	//  4. preemption rank (if preempting)

	bool newBestFound = false;

	if(candidatePreJobRankValue < bestPreJobRankValue);
	else if(candidatePreJobRankValue > bestPreJobRankValue) {
		newBestFound = true;
	}
	else if(candidateRankValue < bestRankValue);
	else if(candidateRankValue > bestRankValue) {
		newBestFound = true;
	}
	else if(candidatePostJobRankValue < bestPostJobRankValue);
	else if(candidatePostJobRankValue > bestPostJobRankValue) {
		newBestFound = true;
	}
	else if(candidatePreemptState < bestPreemptState);
	else if(candidatePreemptState > bestPreemptState) {
		newBestFound = true;
	}
	//NOTE: if NO_PREEMPTION, PreemptRank is a constant
	else if(candidatePreemptRankValue < bestPreemptRankValue);
	else if(candidatePreemptRankValue > bestPreemptRankValue) {
		newBestFound = true;
	}

	if ( newBestFound ) {
		// candidate is better: candidate is elem1, and std::sort man page
		// says return true is elem1 is better than elem2
		return true;
	} else {
		return false;
	}
}
			
void Matchmaker::MatchListType::
sort()
{
	// Should only be called ONCE.  If we call for a sort more than
	// once, this code has a bad logic errror, so ASSERT it.
	ASSERT(already_sorted == false);

	// Note: since we must use static members, sort() is
	// _NOT_ thread safe!!!
	std::sort(AdListArray,AdListArray + adListLen,sort_compare);

	already_sorted = true;
}


void Matchmaker::
init_public_ad()
{
	if( publicAd ) delete( publicAd );
	publicAd = new ClassAd();

	SetMyTypeName(*publicAd, NEGOTIATOR_ADTYPE);

	publicAd->Assign(ATTR_NAME, NegotiatorName );

	publicAd->Assign(ATTR_NEGOTIATOR_IP_ADDR,daemonCore->InfoCommandSinfulString());

#if !defined(WIN32)
	publicAd->Assign(ATTR_REAL_UID, (int)getuid());
#endif

        // Publish all DaemonCore-specific attributes, which also handles
        // NEGOTIATOR_ATTRS for us.
    daemonCore->publish(publicAd);
}

void
Matchmaker::updateCollector( int /* timerID */ ) {
	dprintf(D_FULLDEBUG, "enter Matchmaker::updateCollector\n");

		// in case our address changes, re-initialize public ad every time
	init_public_ad();

	if( publicAd ) {
		publishNegotiationCycleStats( publicAd );

        daemonCore->dc_stats.Publish(*publicAd);
		daemonCore->monitor_data.ExportData(publicAd);

#if defined(WANT_CONTRIB) && defined(WITH_MANAGEMENT) && defined(UNIX)
		NegotiatorPluginManager::Update(*publicAd);
#endif
		daemonCore->sendUpdates(UPDATE_NEGOTIATOR_AD, publicAd, NULL, true);
	}

			// Reset the timer so we don't do another period update until
	daemonCore->Reset_Timer( update_collector_tid, update_interval, update_interval );

	dprintf( D_FULLDEBUG, "exit Matchmaker::UpdateCollector\n" );
}


void
Matchmaker::invalidateNegotiatorAd( void )
{
	ClassAd cmd_ad;
	std::string line;

	if( !NegotiatorName ) {
		return;
	}

		// Set the correct types
	SetMyTypeName( cmd_ad, QUERY_ADTYPE );
	cmd_ad.Assign(ATTR_TARGET_TYPE, NEGOTIATOR_ADTYPE);

	formatstr( line, "TARGET.%s == \"%s\"",
				  ATTR_NAME,
				  NegotiatorName );
	cmd_ad.AssignExpr( ATTR_REQUIREMENTS, line.c_str() );
	cmd_ad.Assign( ATTR_NAME, NegotiatorName );

	daemonCore->sendUpdates( INVALIDATE_NEGOTIATOR_ADS, &cmd_ad, NULL, false );
}

void Matchmaker::RegisterAttemptedOfflineMatch( ClassAd *job_ad, ClassAd *startd_ad, const char *schedd_addr )
{
	if( IsFulldebug(D_FULLDEBUG) ) {
		std::string name;
		startd_ad->LookupString(ATTR_NAME,name);
		std::string owner;
		job_ad->LookupString(ATTR_OWNER,owner);
		dprintf(D_FULLDEBUG,"Registering attempt to match offline machine %s by %s.\n",name.c_str(),owner.c_str());
	}

	ClassAd update_ad;

		// Copy some stuff from the startd ad into the update ad so
		// the collector can identify what ad to merge our update
		// into.
	CopyAttribute(ATTR_NAME, update_ad, *startd_ad);
	CopyAttribute(ATTR_STARTD_IP_ADDR, update_ad, *startd_ad);

	time_t now = time(nullptr);
	update_ad.Assign(ATTR_MACHINE_LAST_MATCH_TIME,now);


	// How many times did we match this machine this negotiation cycle?
	int matchCount = 0;
	startd_ad->LookupInteger("MachineMatchCount", matchCount);

	// 1 is an undercount, and ATTR_RESOURCE_REQUEST_COUNT is an overcount
	// unless no other machine matches, but it's probably the right number
	// assuming the pool is full (which is the case we care about).
	int requestCount = 1;
	job_ad->LookupInteger(ATTR_RESOURCE_REQUEST_COUNT, requestCount);
	matchCount += requestCount;

	startd_ad->Assign(       "MachineMatchCount", matchCount);
	update_ad.Assign(        "MachineMatchCount", matchCount);


	// For each matching resource request, record its originating schedd and
	// autocluster ID (so that the external scaler can look it up) and the
	// requestCount (so that the external scaler knows how much to care).

	ClassAd * record = new ClassAd();
	record->Assign( "Scheduler", schedd_addr );

	int autoclusterID = -1;
	job_ad->LookupInteger( ATTR_AUTO_CLUSTER_ID, autoclusterID );
	record->Assign( "AutoclusterID", autoclusterID );

	record->Assign( "RequestCount", requestCount );

	// This is clumsy because of the ClassAd memory-managment model; see
	// classad/classad/classad.h's commented-out EvalauteAttrList().

	//
	// `record` is heap-allocated and _not_ freed because the ExprList
	// will gain ownership of it (and thus try to free it when it goes
	// out of scope).  The ExprList is heap-allocated so that the
	// shared ptr can deallocate it.  We make a copy for each ad because
	// we can't pass a shared pointer in and ClassAds assume they own
	// everything.
	//

	classad::Value result;
	classad_shared_ptr<classad::ExprList> list( new classad::ExprList() );
	if( startd_ad->EvaluateAttr( "OfflineMatches", result ) ) {
		result.IsSListValue(list);
	}
	list->push_back(record);

	classad::ExprList * copyA = (classad::ExprList *)list->Copy();
	startd_ad->Insert( "OfflineMatches", copyA );

	classad::ExprList * copyB = (classad::ExprList *)list->Copy();
	update_ad.Insert( "OfflineMatches", copyB );


	classy_counted_ptr<ClassAdMsg> msg = new ClassAdMsg(MERGE_STARTD_AD,update_ad);
	classy_counted_ptr<DCCollector> collector = new DCCollector();

	if( !collector->useTCPForUpdates() ) {
		msg->setStreamType( Stream::safe_sock );
	}

	collector->sendMsg( msg.get() );

		// also insert slotX_LastMatchTime into the slot1 ad so that
		// the match info about all slots is available in one place
	std::string name;
	std::string slot1_name;
	int slot_id = -1;
	startd_ad->LookupString(ATTR_NAME,name);
	startd_ad->LookupInteger(ATTR_SLOT_ID,slot_id);

		// Undocumented feature in case we ever need it:
		// If OfflinePrimarySlotName is defined, it specifies which
		// slot should collect all the slotX_LastMatchTime attributes.
	if( !startd_ad->LookupString("OfflinePrimarySlotName",slot1_name) ) {
			// no primary slot name specified, so use slot1

		const char *at = strchr(name.c_str(),'@');
		if( at ) {
				// in case the slot prefix is something other than "slot"
				// figure out the prefix
			int prefix_len = strcspn(name.c_str(),"0123456789");
			if( prefix_len < at - name.c_str() ) {
				formatstr(slot1_name,"%.*s1%s",prefix_len,name.c_str(),at);
			}
		}
	}

	if( !slot1_name.empty() && slot_id >= 0 ) {
		ClassAd slot1_update_ad;

		slot1_update_ad.Assign(ATTR_NAME,slot1_name);
		CopyAttribute(ATTR_STARTD_IP_ADDR,slot1_update_ad,*startd_ad);
		std::string slotX_last_match_time;
		formatstr(slotX_last_match_time,"slot%d_%s",slot_id,ATTR_MACHINE_LAST_MATCH_TIME);
		slot1_update_ad.Assign(slotX_last_match_time,now);

		classy_counted_ptr<ClassAdMsg> lmsg = \
			new ClassAdMsg(MERGE_STARTD_AD, slot1_update_ad);

		if( !collector->useTCPForUpdates() ) {
			lmsg->setStreamType( Stream::safe_sock );
		}

		collector->sendMsg( lmsg.get() );
	}
}

void Matchmaker::StartNewNegotiationCycleStat()
{
	int i;

	delete negotiation_cycle_stats[MAX_NEGOTIATION_CYCLE_STATS-1];

	for(i=MAX_NEGOTIATION_CYCLE_STATS-1;i>0;i--) {
		negotiation_cycle_stats[i] = negotiation_cycle_stats[i-1];
	}

	negotiation_cycle_stats[0] = new NegotiationCycleStats();
	ASSERT( negotiation_cycle_stats[0] );

		// to save memory, only keep stats within the configured visible window
	for(i=num_negotiation_cycle_stats;i<MAX_NEGOTIATION_CYCLE_STATS;i++) {
		if( i == 0 ) {
				// always have a 0th entry in the list so we can mindlessly
				// update it without checking every time.
			continue;
		}
		delete negotiation_cycle_stats[i];
		negotiation_cycle_stats[i] = NULL;
	}
}

static void
DelAttrN( ClassAd *ad, char const *attr, int n )
{
	std::string attrn;
	formatstr(attrn,"%s%d",attr,n);
	ad->Delete( attrn );
}

static void
SetAttrN( ClassAd *ad, char const *attr, int n, int value )
{
	std::string attrn;
	formatstr(attrn,"%s%d",attr,n);
	ad->Assign(attrn,value);
}

static void
SetAttrN( ClassAd *ad, char const *attr, int n, time_t value )
{
	std::string attrn;
	formatstr(attrn,"%s%d",attr,n);
	ad->Assign(attrn,value);
}

static void
SetAttrN( ClassAd *ad, char const *attr, int n, double value )
{
	std::string attrn;
	formatstr(attrn,"%s%d",attr,n);
	ad->Assign(attrn,value);
}

static void
SetAttrN( ClassAd *ad, char const *attr, int n, std::set<std::string> &string_list )
{
	std::string attrn;
	formatstr(attrn,"%s%d",attr,n);

	std::string value;
	std::set<std::string>::iterator it;
	for(it = string_list.begin();
		it != string_list.end();
		it++)
	{
		if( !value.empty() ) {
			value += ", ";
		}
		value += *it;
	}

	ad->Assign(attrn,value);
}


void
Matchmaker::publishNegotiationCycleStats( ClassAd *ad )
{
	char const* attrs[] = {
        ATTR_LAST_NEGOTIATION_CYCLE_TIME,
        ATTR_LAST_NEGOTIATION_CYCLE_END,
        ATTR_LAST_NEGOTIATION_CYCLE_PERIOD,
        ATTR_LAST_NEGOTIATION_CYCLE_DURATION,
        ATTR_LAST_NEGOTIATION_CYCLE_DURATION_PHASE1,
        ATTR_LAST_NEGOTIATION_CYCLE_DURATION_PHASE2,
        ATTR_LAST_NEGOTIATION_CYCLE_DURATION_PHASE3,
        ATTR_LAST_NEGOTIATION_CYCLE_DURATION_PHASE4,
        ATTR_LAST_NEGOTIATION_CYCLE_TOTAL_SLOTS,
        ATTR_LAST_NEGOTIATION_CYCLE_TRIMMED_SLOTS,
        ATTR_LAST_NEGOTIATION_CYCLE_CANDIDATE_SLOTS,
        ATTR_LAST_NEGOTIATION_CYCLE_SLOT_SHARE_ITER,
        ATTR_LAST_NEGOTIATION_CYCLE_NUM_SCHEDULERS,
        ATTR_LAST_NEGOTIATION_CYCLE_NUM_IDLE_JOBS,
        ATTR_LAST_NEGOTIATION_CYCLE_NUM_JOBS_CONSIDERED,
        ATTR_LAST_NEGOTIATION_CYCLE_MATCHES,
        ATTR_LAST_NEGOTIATION_CYCLE_REJECTIONS,
        ATTR_LAST_NEGOTIATION_CYCLE_PIES,
        ATTR_LAST_NEGOTIATION_CYCLE_PIE_SPINS,
        ATTR_LAST_NEGOTIATION_CYCLE_PREFETCH_DURATION,
        ATTR_LAST_NEGOTIATION_CYCLE_PREFETCH_CPU_TIME,
        ATTR_LAST_NEGOTIATION_CYCLE_CPU_TIME,
        ATTR_LAST_NEGOTIATION_CYCLE_PHASE1_CPU_TIME,
        ATTR_LAST_NEGOTIATION_CYCLE_PHASE2_CPU_TIME,
        ATTR_LAST_NEGOTIATION_CYCLE_PHASE3_CPU_TIME,
        ATTR_LAST_NEGOTIATION_CYCLE_PHASE4_CPU_TIME,
        ATTR_LAST_NEGOTIATION_CYCLE_SCHEDDS_OUT_OF_TIME,
        ATTR_LAST_NEGOTIATION_CYCLE_SUBMITTERS_FAILED,
        ATTR_LAST_NEGOTIATION_CYCLE_SUBMITTERS_OUT_OF_TIME,
        ATTR_LAST_NEGOTIATION_CYCLE_SUBMITTERS_SHARE_LIMIT,
        ATTR_LAST_NEGOTIATION_CYCLE_ACTIVE_SUBMITTER_COUNT,
        ATTR_LAST_NEGOTIATION_CYCLE_MATCH_RATE,
        ATTR_LAST_NEGOTIATION_CYCLE_MATCH_RATE_SUSTAINED
    };
    const int nattrs = sizeof(attrs)/sizeof(*attrs);

		// clear out all negotiation cycle attributes in the ad
	for (int i=0; i<MAX_NEGOTIATION_CYCLE_STATS; i++) {
		for (int a=0; a<nattrs; a++) {
			DelAttrN( ad, attrs[a], i );
		}
	}

	for (int i=0; i<num_negotiation_cycle_stats; i++) {
		NegotiationCycleStats* s = negotiation_cycle_stats[i];
		if (s == NULL) continue;

        time_t period = 0;
        if (((1+i) < num_negotiation_cycle_stats) && (negotiation_cycle_stats[1+i] != NULL))
            period = s->end_time - negotiation_cycle_stats[1+i]->end_time;

		SetAttrN( ad, ATTR_LAST_NEGOTIATION_CYCLE_TIME, i, s->start_time);
		SetAttrN( ad, ATTR_LAST_NEGOTIATION_CYCLE_END, i, s->end_time);
		SetAttrN( ad, ATTR_LAST_NEGOTIATION_CYCLE_PERIOD, i, (int)period);
		SetAttrN( ad, ATTR_LAST_NEGOTIATION_CYCLE_DURATION, i, (int)s->duration);
		SetAttrN( ad, ATTR_LAST_NEGOTIATION_CYCLE_DURATION_PHASE1, i, (int)s->duration_phase1);
		SetAttrN( ad, ATTR_LAST_NEGOTIATION_CYCLE_DURATION_PHASE2, i, (int)s->duration_phase2);
		SetAttrN( ad, ATTR_LAST_NEGOTIATION_CYCLE_DURATION_PHASE3, i, (int)s->duration_phase3);
		SetAttrN( ad, ATTR_LAST_NEGOTIATION_CYCLE_DURATION_PHASE4, i, (int)s->duration_phase4);
		SetAttrN( ad, ATTR_LAST_NEGOTIATION_CYCLE_TOTAL_SLOTS, i, (int)s->total_slots);
		SetAttrN( ad, ATTR_LAST_NEGOTIATION_CYCLE_TRIMMED_SLOTS, i, (int)s->trimmed_slots);
        SetAttrN( ad, ATTR_LAST_NEGOTIATION_CYCLE_CANDIDATE_SLOTS, i, (int)s->candidate_slots);
        SetAttrN( ad, ATTR_LAST_NEGOTIATION_CYCLE_SLOT_SHARE_ITER, i, (int)s->slot_share_iterations);
		SetAttrN( ad, ATTR_LAST_NEGOTIATION_CYCLE_NUM_SCHEDULERS, i, (int)s->active_schedds.size());
		SetAttrN( ad, ATTR_LAST_NEGOTIATION_CYCLE_NUM_IDLE_JOBS, i, (int)s->num_idle_jobs);
		SetAttrN( ad, ATTR_LAST_NEGOTIATION_CYCLE_NUM_JOBS_CONSIDERED, i, (int)s->num_jobs_considered);
		SetAttrN( ad, ATTR_LAST_NEGOTIATION_CYCLE_MATCHES, i, (int)s->matches);
		SetAttrN( ad, ATTR_LAST_NEGOTIATION_CYCLE_REJECTIONS, i, (int)s->rejections);
		SetAttrN( ad, ATTR_LAST_NEGOTIATION_CYCLE_MATCH_RATE, i, (s->duration > 0) ? (double)(s->matches)/double(s->duration) : double(0.0));
		SetAttrN( ad, ATTR_LAST_NEGOTIATION_CYCLE_MATCH_RATE_SUSTAINED, i, (period > 0) ? (double)(s->matches)/double(period) : double(0.0));
		SetAttrN( ad, ATTR_LAST_NEGOTIATION_CYCLE_ACTIVE_SUBMITTER_COUNT, i, (int)s->active_submitters.size());
		SetAttrN( ad, ATTR_LAST_NEGOTIATION_CYCLE_PIES, i, s->pies );
		SetAttrN( ad, ATTR_LAST_NEGOTIATION_CYCLE_PIE_SPINS, i, s->pie_spins );
		SetAttrN( ad, ATTR_LAST_NEGOTIATION_CYCLE_PREFETCH_DURATION, i, s->prefetch_duration );
		// TODO Should we truncate these to integer values?
		SetAttrN( ad, ATTR_LAST_NEGOTIATION_CYCLE_PREFETCH_CPU_TIME, i, s->prefetch_cpu_time );
		SetAttrN( ad, ATTR_LAST_NEGOTIATION_CYCLE_CPU_TIME, i, s->phase1_cpu_time );
		SetAttrN( ad, ATTR_LAST_NEGOTIATION_CYCLE_PHASE1_CPU_TIME, i, s->phase1_cpu_time );
		SetAttrN( ad, ATTR_LAST_NEGOTIATION_CYCLE_PHASE2_CPU_TIME, i, s->phase2_cpu_time );
		SetAttrN( ad, ATTR_LAST_NEGOTIATION_CYCLE_PHASE3_CPU_TIME, i, s->phase3_cpu_time );
		SetAttrN( ad, ATTR_LAST_NEGOTIATION_CYCLE_PHASE4_CPU_TIME, i, s->phase4_cpu_time );
		SetAttrN( ad, ATTR_LAST_NEGOTIATION_CYCLE_SCHEDDS_OUT_OF_TIME, i, s->schedds_out_of_time);
		SetAttrN( ad, ATTR_LAST_NEGOTIATION_CYCLE_SUBMITTERS_FAILED, i, s->submitters_failed);
		SetAttrN( ad, ATTR_LAST_NEGOTIATION_CYCLE_SUBMITTERS_OUT_OF_TIME, i, s->submitters_out_of_time);
        SetAttrN( ad, ATTR_LAST_NEGOTIATION_CYCLE_SUBMITTERS_SHARE_LIMIT, i, s->submitters_share_limit);
	}
}

bool rankPairCompare(std::pair<int,double> lhs, std::pair<int,double> rhs) {
	return lhs.second < rhs.second;
}

	// Return true is this partitionable slot would match the
	// job with preempted resources from a dynamic slot.
	// Only consider startd RANK for now.
bool
Matchmaker::pslotMultiMatch(ClassAd *job, ClassAd *machine, const char *submitterName,
                            bool only_startd_rank, std::string &dslot_claims, PreemptState &candidatePreemptState)
{
	bool isPartitionable = false;
	PreemptState saved_candidatePreemptState = candidatePreemptState;

	machine->LookupBool(ATTR_SLOT_PARTITIONABLE, isPartitionable);

	// This whole deal is only for partitionable slots
	if (!isPartitionable) {
		return false;
	}

	double newRank; // The startd rank of the potential job

	if (!EvalFloat(ATTR_RANK, machine, job, newRank)) {
		newRank = 0.0;
	}

	// How many active dslots does this pslot currently have?
	int numDslots = 0;
	machine->LookupInteger(ATTR_NUM_DYNAMIC_SLOTS, numDslots);

	if (numDslots < 1) {
		return false;
	}

	std::string name, ipaddr;
	machine->LookupString(ATTR_NAME, name);
	machine->LookupString(ATTR_MY_ADDRESS, ipaddr);

		// Lookup the vector of claim ids for this startd
	std::string hash_key = name + ipaddr;
	std::vector<std::string> &child_claims = childClaimHash[hash_key];
	if ( numDslots != (int)child_claims.size() ) {
		dprintf( D_FULLDEBUG, "Wrong number of dslot claim ids for %s, ignoring for pslot preemption\n", name.c_str() );
		return false;
	}

		// Copy the childCurrentRanks list attributes into vector
		// Skip dslots that aren't eligible for matching
	std::vector<std::pair<int,double> > ranks;
	for ( int i = 0; i < numDslots; i++ ) {
		double currentRank = 0.0; // global default startd rank
		int retire_time = 0;
		std::string state = "";
		dslotLookupFloat( machine, ATTR_CURRENT_RANK, i, currentRank );
		dslotLookupInteger( machine, ATTR_RETIREMENT_TIME_REMAINING, i,
							retire_time );
		dslotLookupString( machine, ATTR_STATE, i, state );
		// TODO In the future, condition this on ConsiderEarlyPreemption
		if ( retire_time > 0 ) {
			continue;
		}
		if ( !strcmp( state.c_str(), state_to_string( preempting_state ) ) ) {
			continue;
		}
		if ( child_claims[i] == "" ) {
			continue;
		}
		ranks.emplace_back(i, currentRank );
	}

		// Early opt-out -- if no dslots are eligible for matching, bail out now
	if ( ranks.size() == 0 ) {
		return false;
	}

		// Sort all dslots by their current rank
	std::sort(ranks.begin(), ranks.end(), rankPairCompare);

	std::vector<std::string> attrs;
	std::string attrs_str;
	if ( machine->LookupString( ATTR_MACHINE_RESOURCES, attrs_str ) ) {
		attrs = split(attrs_str, " ");
	} else {
		attrs.emplace_back("cpus");
		attrs.emplace_back("memory");
		attrs.emplace_back("disk");
	}

		// Backup all the attributes in the machine ad we may end up mutating
	ExprTree* expr;
	ClassAd* backupAd = new ClassAd();
	for (const auto& attr: attrs) {
		if ( (expr = machine->Lookup(attr)) ) {
			expr = expr->Copy();
			backupAd->Insert(attr, expr);
		}
	}
	backupAd->AssignExpr(ATTR_REMOTE_USER,"UNDEFINED");

		// In rank order, see if by preempting one more dslot would cause pslot to match
	std::list<int> usableDSlots;
	for (unsigned int slot = 0; slot < ranks.size() && ranks[slot].second <= newRank; slot++) {
		int dSlot = ranks[slot].first; // dslot index in childXXX list

			// if ranks are the same, consider preemption just based on user prio iff
			// 1) userprio of preempting user > exiting user + delta
			// 2) preemption requirements match
		if (ranks[slot].second == newRank) {

			// If only considering startd rank, ignore this slot, cause ranks are equal.
			if (only_startd_rank) {
				continue;
			}

			// See if PREEMPTION_REQUIREMENTS holds when evaluated in the context
			// of the dslot we are considering and the job

				// First fetch the dslot ad we are considering
			std::string dslotName;
			ClassAd * dslotCandidateAd = NULL;
			if ( !dslotLookupString( machine, ATTR_NAME, dSlot, dslotName ) ) {
				// couldn't parse or evaluate, give up on this dslot
				continue;
			} else {
				// using the dslotName, find the dslot ad in our map
				auto it = m_slotNameToAdMap.find(dslotName);
				if (it == m_slotNameToAdMap.end()) {
					// dslot ad not found ???, give up on this dslot
					continue;
				} else {
					dslotCandidateAd = it->second;
				}
			}

				// Figure out the full remoteUser (including any group names).  If it is the
				// same as the submitterName, then ignore this slot.
			std::string remoteUser;
			if (!dslotCandidateAd->LookupString(ATTR_PREEMPTING_ACCOUNTING_GROUP, remoteUser)) {
				if (!dslotCandidateAd->LookupString(ATTR_PREEMPTING_USER, remoteUser)) {
					if (!dslotCandidateAd->LookupString(ATTR_ACCOUNTING_GROUP, remoteUser)) {
						dslotCandidateAd->LookupString(ATTR_REMOTE_USER, remoteUser);
					}
				}
			}
			if ( remoteUser == submitterName ) {
				continue;
			}


				// if PreemptionRequirements evals to true, below
				// will be true
			classad::Value result;
			result.SetBooleanValue(false);

				// Evalute preemption req into result
			EvalExprToBool(PreemptionReq,dslotCandidateAd,job,result);

			bool shouldPreempt = false;
			if (!result.IsBooleanValue(shouldPreempt) || (shouldPreempt == false)) {
				// didn't eval to boolean or eval'ed to false.  Ignore this slot
				continue;
			}
			
			// Finally, if we made it here, this dslot is a candidate for prio preemption,
			// fall through and try to merge its resources into the pslot to see
			// if it is enough for a match.
			candidatePreemptState = PRIO_PREEMPTION;

		} else {

			// Finally, if we made it here, this dslot is a candidate for startd rank preemption,
			// fall through and try to merge its resources into the pslot to see
			// if it is enough for a match.
			// Note that if candidatePreemptState was already set to PRIO_PREMPTION, it meant
			// some other dslot required prio preemption to use, so keep that value.
			if ( candidatePreemptState != PRIO_PREEMPTION ) {
				candidatePreemptState = RANK_PREEMPTION;
			}

			// Finally, if we made it here, this slot is a candidate for
			// preemption, fall through and try to merge its resources into
			// the pslot to match and preempt this one.
		}
		usableDSlots.push_back(slot);

			// for each splitable resource, get it from the dslot, and add to pslot
		for (const auto& attr: attrs) {
			double b4 = 0.0;
			double realValue = 0.0;

			if (machine->LookupFloat(attr, b4)) {
					// The value exists in the parent
				b4 = floor(b4);
				classad::Value result;
				if ( !dslotLookup( machine, attr.c_str(), dSlot, result ) ) {
					result.SetUndefinedValue();
				}

				long long longValue;
				if (result.IsIntegerValue(longValue)) {
					machine->Assign(attr, (long long) (b4 + longValue));
				} else if (result.IsRealValue(realValue)) {
					machine->Assign(attr, (b4 + realValue));
				} else {
					// TODO: deal with slot resources that are not ints or reals, e.g. non-fungibles
					dprintf(D_ALWAYS, "Lookup of %s failed to evalute to integer or real\n", attr.c_str());	
				}
			}
		}

		// Now, check if it is a match

		// Since we modified the machine resource counts, we need to
		// unoptimize the machine ad, in case the original values were
		// propagated into the requirments or rank expressions.
		classad::MatchClassAd::UnoptimizeAdForMatchmaking(machine);

		if (IsAMatch(job, machine)) {
			dprintf(D_FULLDEBUG, "Matched pslot %s by %s preempting %d dynamic slots\n",
				name.c_str(),
				candidatePreemptState == PRIO_PREEMPTION ? "priority" : "startd rank",
				(int)usableDSlots.size());
			dslot_claims.clear();

			auto i = usableDSlots.begin();
			for( ; i != usableDSlots.end(); ++i ) {
				int child = *i;
				dslot_claims += child_claims[ranks[child].first];
				dslot_claims += " ";
				// TODO Move this clearing of claim ids to
				//   matchmakingProcotol(), after the match is successfully
				//   sent to the schedd. That is where the claim id of the
				//   pslot is cleared.
				child_claims[ranks[child].first] = "";
			}

			// Put a bogus RemoteUser attribute into the pslot
			// ad so all the legacy policy statements (like NEGOTIATOR_PRE_JOB_RANK) understand that
			// this match is causing preemption.  This bogus RemoteUser attribute will be reset to UNDEFINED
			// when we restore the backupAd info when we call DeleteMatchList().
			machine->Assign(ATTR_REMOTE_USER,"various_dSlot_users");

			// Stash away all the attributes we mutated in the slot ad so we can restore it
			// when/if we purge the match list in DeleteMatchList().
			unmutatedSlotAds.emplace_back(machine, backupAd );

			// Note we do not want to delete backupAd when returning here, since we handed off this
			// pointer to unmutatedSlotAds above; it will be deleted in DeleteMatchList().
			return true;
		}
	}

	// If we made it here, we failed to match this pSlot.  So restore the pslot ad back to
	// its original state before we mutated it, delete our backupAd after using it,
	// restore original value of candidatePreemptState (cuz we should only modify this if
	// we are returning true), and return false.
	machine->Update(*backupAd);
	delete backupAd;
	candidatePreemptState = saved_candidatePreemptState;
	return false;
}

static int jobsInSlot(ClassAd &request, ClassAd &offer) {
	int requestCpus = 1;
	int requestMemory = 1;
	int availCpus = 1;
	int availMemory = 1;

	offer.LookupInteger(ATTR_CPUS, availCpus);
	offer.LookupInteger(ATTR_MEMORY, availMemory);
	EvalInteger(ATTR_REQUEST_CPUS, &request, &offer, requestCpus);
	EvalInteger(ATTR_REQUEST_MEMORY, &request, &offer, requestMemory);

		// Eventually should support fractional Cpus...
	if (requestCpus < 1) requestCpus = 1;
	if (requestMemory < 1) requestMemory = 1;

	return MIN( availCpus / requestCpus,
	            availMemory / requestMemory );
}

GCC_DIAG_ON(float-equal)

