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
#include "condor_api.h"
#include "condor_classad.h"
#include "condor_query.h"
#include "daemon.h"
#include "dc_startd.h"
#include "daemon_types.h"
#include "dc_collector.h"
#include "condor_string.h"  // for strlwr() and friends
#include "get_daemon_name.h"
#include "condor_netdb.h"
#include "condor_claimid_parser.h"
#include "misc_utils.h"
#include "ConcurrencyLimitUtils.h"
#include "MyString.h"
#include "condor_daemon_core.h"

#include "consumption_policy.h"

#include <vector>
#include <string>
#include <deque>

#if defined(WANT_CONTRIB) && defined(WITH_MANAGEMENT)
#if defined(HAVE_DLOPEN)
#include "NegotiatorPlugin.h"
#endif
#endif

// the comparison function must be declared before the declaration of the
// matchmaker class in order to preserve its static-ness.  (otherwise, it
// is forced to be extern.)

static int comparisonFunction (AttrList *, AttrList *, void *);
#include "matchmaker.h"


/* This extracts the machine name from the global job ID user@machine.name#timestamp#cluster.proc*/
static int get_scheddname_from_gjid(const char * globaljobid, char * scheddname );

// possible outcomes of negotiating with a schedd
enum { MM_ERROR, MM_DONE, MM_RESUME };

// possible outcomes of a matchmaking attempt
enum { _MM_ERROR, MM_NO_MATCH, MM_GOOD_MATCH, MM_BAD_MATCH };

typedef int (*lessThanFunc)(AttrList*, AttrList*, void*);

MyString SlotWeightAttr = ATTR_SLOT_WEIGHT;

char const *RESOURCES_IN_USE_BY_USER_FN_NAME = "ResourcesInUseByUser";
char const *RESOURCES_IN_USE_BY_USERS_GROUP_FN_NAME = "ResourcesInUseByUsersGroup";

GCC_DIAG_OFF(float-equal)

class NegotiationCycleStats
{
public:
	NegotiationCycleStats();

	time_t start_time;
    time_t end_time;

	int duration;
    int duration_phase1;
    int duration_phase2;
    int duration_phase3;
    int duration_phase4;

    int total_slots;
    int trimmed_slots;
    int candidate_slots;

    int slot_share_iterations;

    int num_idle_jobs;
    int num_jobs_considered;

	int matches;
	int rejections;

    // set of unique active schedd, id by sinful strings:
    std::set<std::string> active_schedds;

    // active submitters
	std::set<std::string> active_submitters;

    std::set<std::string> submitters_share_limit;
	std::set<std::string> submitters_out_of_time;
	std::set<std::string> submitters_failed;
};

NegotiationCycleStats::NegotiationCycleStats():
    start_time(time(NULL)),
    end_time(start_time),
	duration(0),
    duration_phase1(0),
    duration_phase2(0),
    duration_phase3(0),
    duration_phase4(0),
    total_slots(0),
    trimmed_slots(0),
    candidate_slots(0),
    slot_share_iterations(0),
    num_idle_jobs(0),
    num_jobs_considered(0),
	matches(0),
	rejections(0),
    active_schedds(),
    active_submitters(),
    submitters_share_limit(),
    submitters_out_of_time(),
    submitters_failed()
{
}


static MyString MachineAdID(ClassAd * ad)
{
	ASSERT(ad);
	MyString addr;
	MyString name;

	// We should always be passed an ad with an ATTR_NAME.
	ASSERT(ad->LookupString(ATTR_NAME, name));
	if(!ad->LookupString(ATTR_STARTD_IP_ADDR, addr)) {
		addr = "<No Address>";
	}

	MyString ID(addr);
	ID += " ";
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

	float usage = matchmaker_for_classad_func->getAccountant().GetWeightedResourcesUsed(user.c_str());

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

	float group_quota = 0;
	float group_usage = 0;
    string group_name;
	if( !matchmaker_for_classad_func->getGroupInfoFromUserId(user.c_str(),group_name,group_quota,group_usage) ) {
		result.SetErrorValue();
		return true;
	}

	result.SetRealValue( group_usage );
	return true;
}

Matchmaker::
Matchmaker ()
   : strSlotConstraint(NULL)
   , SlotPoolsizeConstraint(NULL)
{
	char buf[64];

	NegotiatorName = NULL;

	AccountantHost  = NULL;
	PreemptionReq = NULL;
	PreemptionRank = NULL;
	NegotiatorPreJobRank = NULL;
	NegotiatorPostJobRank = NULL;
	sockCache = NULL;

	sprintf (buf, "MY.%s > MY.%s", ATTR_RANK, ATTR_CURRENT_RANK);
	ParseClassAdRvalExpr (buf, rankCondStd);

	sprintf (buf, "MY.%s >= MY.%s", ATTR_RANK, ATTR_CURRENT_RANK);
	ParseClassAdRvalExpr (buf, rankCondPrioPreempt);

	negotiation_timerID = -1;
	GotRescheduleCmd=false;
	job_attr_references = NULL;
	
	stashedAds = new AdHash(1000, HashFunc);

	MatchList = NULL;
	cachedAutoCluster = -1;
	cachedName = NULL;
	cachedAddr = NULL;

	want_globaljobprio = false;
	want_matchlist_caching = false;
	ConsiderPreemption = true;
	ConsiderEarlyPreemption = false;
	want_nonblocking_startd_contact = true;

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
    allow_quota_oversub = false;

    cp_resources = false;

	rejForNetwork = 0;
	rejForNetworkShare = 0;
	rejPreemptForPrio = 0;
	rejPreemptForPolicy = 0;
	rejPreemptForRank = 0;
	rejForSubmitterLimit = 0;
	rejForConcurrencyLimit = 0;

	cachedPrio = 0;
	cachedOnlyForStartdRank = false;

		// just assign default values
	want_inform_startd = true;
	preemption_req_unstable = true;
	preemption_rank_unstable = true;
	NegotiatorTimeout = 30;
 	NegotiatorInterval = 60;
 	MaxTimePerSubmitter = 31536000;
 	MaxTimePerSpin = 31536000;

	ASSERT( matchmaker_for_classad_func == NULL );
	matchmaker_for_classad_func = this;
	std::string name;
	name = RESOURCES_IN_USE_BY_USER_FN_NAME;
	classad::FunctionCall::RegisterFunction( name,
											 ResourcesInUseByUser_classad_func );
	name = RESOURCES_IN_USE_BY_USERS_GROUP_FN_NAME;
	classad::FunctionCall::RegisterFunction( name,
											 ResourcesInUseByUsersGroup_classad_func );
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

	if (NegotiatorName) free (NegotiatorName);
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
initialize ()
{
	// read in params
	reinitialize ();

    // register commands
    daemonCore->Register_Command (RESCHEDULE, "Reschedule", 
            (CommandHandlercpp) &Matchmaker::RESCHEDULE_commandHandler, 
			"RESCHEDULE_commandHandler", (Service*) this, DAEMON);
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
			"SET_PRIORITYFACTOR_commandHandler", this, ADMINISTRATOR);
    daemonCore->Register_Command (SET_PRIORITY, "SetPriority",
            (CommandHandlercpp) &Matchmaker::SET_PRIORITY_commandHandler, 
			"SET_PRIORITY_commandHandler", this, ADMINISTRATOR);
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

	// Set a timer to renegotiate.
    negotiation_timerID = daemonCore->Register_Timer (0,  NegotiatorInterval,
			(TimerHandlercpp) &Matchmaker::negotiationTime, 
			"Time to negotiate", this);

	update_collector_tid = daemonCore->Register_Timer (
			0, update_interval,
			(TimerHandlercpp) &Matchmaker::updateCollector,
			"Update Collector", this );


#if defined(WANT_CONTRIB) && defined(WITH_MANAGEMENT)
#if defined(HAVE_DLOPEN)
	NegotiatorPluginManager::Load();
	NegotiatorPluginManager::Initialize();
#endif
#endif
}


int Matchmaker::
reinitialize ()
{
	char *tmp;
	static bool first_time = true;

    // (re)build the HGQ group tree from configuration
    // need to do this prior to initializing the accountant
    hgq_construct_tree();

    // Initialize accountant params
    accountant.Initialize(hgq_root_group);

	init_public_ad();

	// get timeout values

 	NegotiatorInterval = param_integer("NEGOTIATOR_INTERVAL",60);

	NegotiatorTimeout = param_integer("NEGOTIATOR_TIMEOUT",30);

	// up to 1 year per submitter by default
 	MaxTimePerSubmitter = param_integer("NEGOTIATOR_MAX_TIME_PER_SUBMITTER",31536000);

	// up to 1 year per spin by default
 	MaxTimePerSpin = param_integer("NEGOTIATOR_MAX_TIME_PER_PIESPIN",31536000);

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
#if defined(ADD_TARGET_SCOPING)
		if(PreemptionReq){
			ExprTree *tmp_expr = AddTargetRefs( PreemptionReq, TargetJobAttrs );
			delete PreemptionReq;
			PreemptionReq = tmp_expr;
		}
#endif
		dprintf (D_ALWAYS,"PREEMPTION_REQUIREMENTS = %s\n", tmp);
		free( tmp );
		tmp = NULL;
	} else {
		dprintf (D_ALWAYS,"PREEMPTION_REQUIREMENTS = None\n");
	}

	NegotiatorMatchExprNames.clearAll();
	NegotiatorMatchExprValues.clearAll();
	tmp = param("NEGOTIATOR_MATCH_EXPRS");
	if( tmp ) {
		NegotiatorMatchExprNames.initializeFromString( tmp );
		free( tmp );
		tmp = NULL;

			// Now read in the values of the macros in the list.
		NegotiatorMatchExprNames.rewind();
		char const *expr_name;
		while( (expr_name=NegotiatorMatchExprNames.next()) ) {
			char *expr_value = param( expr_name );
			if( !expr_value ) {
				dprintf(D_ALWAYS,"Warning: NEGOTIATOR_MATCH_EXPRS references a macro '%s' which is not defined in the configuration file.\n",expr_name);
				NegotiatorMatchExprNames.deleteCurrent();
				continue;
			}
			NegotiatorMatchExprValues.append( expr_value );
			free( expr_value );
		}

			// Now change the names of the ExprNames so they have the prefix
			// "MatchExpr" that is expected by the schedd.
		size_t prefix_len = strlen(ATTR_NEGOTIATOR_MATCH_EXPR);
		NegotiatorMatchExprNames.rewind();
		while( (expr_name=NegotiatorMatchExprNames.next()) ) {
			if( strncmp(expr_name,ATTR_NEGOTIATOR_MATCH_EXPR,prefix_len) != 0 ) {
				MyString new_name = ATTR_NEGOTIATOR_MATCH_EXPR;
				new_name += expr_name;
				NegotiatorMatchExprNames.insert(new_name.Value());
				NegotiatorMatchExprNames.deleteCurrent();
			}
		}
	}

	dprintf (D_ALWAYS,"ACCOUNTANT_HOST = %s\n", AccountantHost ? 
			AccountantHost : "None (local)");
	dprintf (D_ALWAYS,"NEGOTIATOR_INTERVAL = %d sec\n",NegotiatorInterval);
	dprintf (D_ALWAYS,"NEGOTIATOR_TIMEOUT = %d sec\n",NegotiatorTimeout);
	dprintf (D_ALWAYS,"MAX_TIME_PER_SUBMITTER = %d sec\n",MaxTimePerSubmitter);
	dprintf (D_ALWAYS,"MAX_TIME_PER_PIESPIN = %d sec\n",MaxTimePerSpin);

	if( tmp ) free( tmp );

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
#if defined(ADD_TARGET_SCOPING)
		if(PreemptionRank){
			tmp_expr = AddTargetRefs( PreemptionRank, TargetJobAttrs );
			delete PreemptionRank;
		}
		PreemptionRank = tmp_expr;
#endif

	dprintf (D_ALWAYS,"PREEMPTION_RANK = %s\n", (tmp?tmp:"None"));

	if( tmp ) free( tmp );

	if (NegotiatorPreJobRank) delete NegotiatorPreJobRank;
	NegotiatorPreJobRank = NULL;
	tmp = param("NEGOTIATOR_PRE_JOB_RANK");
	if( tmp ) {
		if( ParseClassAdRvalExpr(tmp, NegotiatorPreJobRank) ) {
			EXCEPT ("Error parsing NEGOTIATOR_PRE_JOB_RANK expression: %s", tmp);
		}
#if defined(ADD_TARGET_SCOPING)
		if(NegotiatorPreJobRank){
			tmp_expr = AddTargetRefs( NegotiatorPreJobRank, TargetJobAttrs );
			delete NegotiatorPreJobRank;
		}
		NegotiatorPreJobRank = tmp_expr;
#endif
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
#if defined(ADD_TARGET_SCOPING)
		if(NegotiatorPostJobRank){
			tmp_expr = AddTargetRefs( NegotiatorPostJobRank, TargetJobAttrs );
			delete NegotiatorPostJobRank;
		}
		NegotiatorPostJobRank = tmp_expr;
#endif
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
	ConsiderPreemption = param_boolean("NEGOTIATOR_CONSIDER_PREEMPTION",true);
	ConsiderEarlyPreemption = param_boolean("NEGOTIATOR_CONSIDER_EARLY_PREEMPTION",false);
	if( ConsiderEarlyPreemption && !ConsiderPreemption ) {
		dprintf(D_ALWAYS,"WARNING: NEGOTIATOR_CONSIDER_EARLY_PREEMPTION=true will be ignored, because NEGOTIATOR_CONSIDER_PREEMPTION=false\n");
	}
	want_inform_startd = param_boolean("NEGOTIATOR_INFORM_STARTD", true);
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

	num_negotiation_cycle_stats = param_integer("NEGOTIATION_CYCLE_STATS_LENGTH",3,0,MAX_NEGOTIATION_CYCLE_STATS);
	ASSERT( num_negotiation_cycle_stats <= MAX_NEGOTIATION_CYCLE_STATS );

	if( first_time ) {
		first_time = false;
	} else { 
			// be sure to try to publish a new negotiator ad on reconfig
		updateCollector();
	}


	// done
	return TRUE;
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


int Matchmaker::
SET_PRIORITYFACTOR_commandHandler (int, Stream *strm)
{
	float	priority;
    std::string submitter;

	// read the required data off the wire
	if (!strm->get(submitter) 	|| 
		!strm->get(priority) 	|| 
		!strm->end_of_message())
	{
		dprintf (D_ALWAYS, "Could not read submitter name and priority factor\n");
		return FALSE;
	}

	// set the priority
	dprintf(D_ALWAYS,"Setting the priority factor of %s to %f\n", submitter.c_str(), priority);
	accountant.SetPriorityFactor(submitter, priority);
	
	return TRUE;
}


int Matchmaker::
SET_PRIORITY_commandHandler (int, Stream *strm)
{
	float	priority;
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
	float	accumUsage;
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
	AttrList* ad=accountant.ReportState();
	
	if (!putClassAdNoTypes(strm, *ad) ||
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
    AttrList* ad = accountant.ReportState(true);

    if (!putClassAdNoTypes(strm, *ad) ||
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
	AttrList* ad=accountant.ReportState(submitter);
	dprintf (D_ALWAYS,"Getting state information from the accountant\n");
	
	if (!putClassAdNoTypes(strm, *ad) ||
	    !strm->end_of_message())
	{
		dprintf (D_ALWAYS, "Could not send resource list\n");
		delete ad;
		return FALSE;
	}

	delete ad;

	return TRUE;
}


char *
Matchmaker::
compute_significant_attrs(ClassAdListDoesNotDeleteAds & startdAds)
{
	char *result = NULL;

	// Figure out list of all external attribute references in all startd ads
	dprintf(D_FULLDEBUG,"Entering compute_significant_attrs()\n");
	ClassAd *startd_ad = NULL;
	ClassAd *sample_startd_ad = NULL;
	startdAds.Open ();
	StringList internal_references;	// not used...
	StringList external_references;	// this is what we want to compute. 
	while ((startd_ad = startdAds.Next ())) { // iterate through all startd ads
		if ( !sample_startd_ad ) {
			sample_startd_ad = new ClassAd(*startd_ad);
		}
			// Make a stringlist of all attribute names in this startd ad.
		StringList AttrsToExpand;
		startd_ad->ResetName();
		const char *attr_name = startd_ad->NextNameOriginal();
		while ( attr_name ) {
			AttrsToExpand.append(attr_name);
			attr_name = startd_ad->NextNameOriginal();
		}
			// Get list of external references for all attributes.  Note that 
			// it is _not_ sufficient to just get references via requirements
			// and rank.  Don't understand why? Ask Todd <tannenba@cs.wisc.edu>
		AttrsToExpand.rewind();
		while ( (attr_name = AttrsToExpand.next()) ) {
			startd_ad->GetReferences(attr_name,internal_references,
					external_references);
		}	// while attr_name
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
	char *startd_job_exprs = param("STARTD_JOB_EXPRS");
	if ( startd_job_exprs ) {	// add in startd_job_exprs
		StringList exprs(startd_job_exprs);
		exprs.rewind();
		char *v = NULL;
		while ( (v=exprs.next()) ) {
			sample_startd_ad->Assign(v,true);
		}
		free(startd_job_exprs);
	}
	char *tmp=param("PREEMPTION_REQUIREMENTS");
	if ( tmp && PreemptionReq ) {	// add references from preemption_requirements
		const char* preempt_req_name = "preempt_req__";	// any name will do
		sample_startd_ad->AssignExpr(preempt_req_name,tmp);
		sample_startd_ad->GetReferences(preempt_req_name,internal_references,
					external_references);
	}
	free(tmp);
	if (sample_startd_ad) {
		delete sample_startd_ad;
		sample_startd_ad = NULL;
	}
		// Always get rid of the follow attrs:
		//    CurrentTime - for obvious reasons
		//    RemoteUserPrio - not needed since we negotiate per user
		//    SubmittorPrio - not needed since we negotiate per user
	external_references.remove_anycase(ATTR_CURRENT_TIME);
	external_references.remove_anycase(ATTR_REMOTE_USER_PRIO);
	external_references.remove_anycase(ATTR_REMOTE_USER_RESOURCES_IN_USE);
	external_references.remove_anycase(ATTR_REMOTE_GROUP_RESOURCES_IN_USE);
	external_references.remove_anycase(ATTR_SUBMITTOR_PRIO);
	external_references.remove_anycase(ATTR_SUBMITTER_USER_PRIO);
	external_references.remove_anycase(ATTR_SUBMITTER_USER_RESOURCES_IN_USE);
	external_references.remove_anycase(ATTR_SUBMITTER_GROUP_RESOURCES_IN_USE);
		// Note: print_to_string mallocs memory on the heap
	result = external_references.print_to_string();
	dprintf(D_FULLDEBUG,"Leaving compute_significant_attrs() - result=%s\n",
					result ? result : "(none)" );
	return result;
}


bool Matchmaker::
getGroupInfoFromUserId(const char* user, string& groupName, float& groupQuota, float& groupUsage)
{
	ASSERT(groupQuotasHash);

    groupName = "";
	groupQuota = 0.0;
	groupUsage = 0.0;

	if (!user) return false;

    GroupEntry* group = accountant.GetAssignedGroup(user);

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

void round_for_precision(double& x) {
    double ref = x;
    x = floor(0.5 + x);
    double err = fabs(x-ref);
    // This error threshold is pretty ad-hoc.  It would be ideal to try and figure out
    // bounds on precision error accumulation based on size of HGQ tree.
    if (err > 0.00001) {
        // If precision errors are not small, I am suspicious.
        dprintf(D_ALWAYS, "group quotas: WARNING: encountered precision error of %g\n", err);
    }
}


double starvation_ratio(double usage, double allocated) {
    return (allocated > 0) ? (usage / allocated) : FLT_MAX;
}

struct group_order {
    bool autoregroup;
    GroupEntry* root_group;

    group_order(bool arg, GroupEntry* rg): autoregroup(arg), root_group(rg) {
        if (autoregroup) {
            dprintf(D_ALWAYS, "group quotas: autoregroup mode: forcing group %s to negotiate last\n", root_group->name.c_str());
        }
    }

    bool operator()(const GroupEntry* a, const GroupEntry* b) const {
        if (autoregroup) {
            // root is never before anybody:
            if (a == root_group) return false;
            // a != root, and b = root, so a has to be before b:
            if (b == root_group) return true;
        }
        return a->sort_key < b->sort_key;
    }

    private:
    // I don't want anybody defaulting this obj by accident
    group_order(){}
};


int count_effective_slots(ClassAdListDoesNotDeleteAds& startdAds, ExprTree* constraint) {
	int sum = 0;

	startdAds.Open();
	while(ClassAd* ad = startdAds.Next()) {
        // only count ads satisfying constraint, if given
        if ((NULL != constraint) && !EvalBool(ad, constraint)) {
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


void Matchmaker::
negotiationTime ()
{
	ClassAdList allAds; //contains ads from collector
	ClassAdListDoesNotDeleteAds startdAds; // ptrs to startd ads in allAds
        //ClaimIdHash claimIds(MyStringHash);
    ClaimIdHash claimIds;
	ClassAdListDoesNotDeleteAds scheddAds; // ptrs to schedd ads in allAds

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
	int elapsed = time(NULL) - completedLastCycleTime;
	int cycle_delay = param_integer("NEGOTIATOR_CYCLE_DELAY",20,0);
	if ( elapsed < cycle_delay ) {
		daemonCore->Reset_Timer(negotiation_timerID,
							cycle_delay - elapsed,
							NegotiatorInterval);
		dprintf(D_FULLDEBUG,
			"New cycle requested but just finished one -- delaying %u secs\n",
			cycle_delay - elapsed);
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

	// ----- Get all required ads from the collector
    time_t start_time_phase1 = time(NULL);
	dprintf( D_ALWAYS, "Phase 1:  Obtaining ads from collector ...\n" );
	if( !obtainAdsFromCollector( allAds, startdAds, scheddAds,
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
	int cTotalSlots = startdAds.MyLength();
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
    negotiation_cycle_stats[0]->duration_phase1 += start_time_phase2 - start_time_phase1;

	if ( job_attr_references ) {
		free(job_attr_references);
	}
	job_attr_references = compute_significant_attrs(startdAds);

	// ----- Recalculate priorities for schedds
	accountant.UpdatePriorities();
	accountant.CheckMatches( startdAds );

	if ( !groupQuotasHash ) {
		groupQuotasHash = new groupQuotasHashType(100,HashFunc);
		ASSERT(groupQuotasHash);
    }

	int cPoolsize = 0;
    double weightedPoolsize = 0;
    int effectivePoolsize = 0;
    // Restrict number of slots available for determining quotas
    if (SlotPoolsizeConstraint != NULL) {
        cPoolsize = startdAds.CountMatches(SlotPoolsizeConstraint);
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
    negotiation_cycle_stats[0]->trimmed_slots = startdAds.MyLength();
    negotiation_cycle_stats[0]->candidate_slots = startdAds.MyLength();

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

    if (hgq_groups.size() <= 1) {
        // If there is only one group (the root group) we are in traditional non-HGQ mode.
        // It seems cleanest to take the traditional case separately for maximum backward-compatible behavior.
        // A possible future change would be to unify this into the HGQ code-path, as a "root-group-only" case. 
        negotiateWithGroup(cPoolsize, weightedPoolsize, minSlotWeight, startdAds, claimIds, scheddAds);
    } else {
        // Otherwise we are in HGQ mode, so begin HGQ computations

        negotiation_cycle_stats[0]->candidate_slots = cPoolsize;

        // Fill in latest usage/prio info for the groups.
        // While we're at it, reset fields prior to reloading from submitter ads.
        for (vector<GroupEntry*>::iterator j(hgq_groups.begin());  j != hgq_groups.end();  ++j) {
            GroupEntry* group = *j;

            group->quota = 0;
            group->requested = 0;
            group->currently_requested = 0;
            group->allocated = 0;
            group->subtree_quota = 0;
            group->subtree_requested = 0;
            if (NULL == group->submitterAds) group->submitterAds = new ClassAdListDoesNotDeleteAds;
            group->submitterAds->Open();
            while (ClassAd* ad = group->submitterAds->Next()) {
                group->submitterAds->Remove(ad);
            }
            group->submitterAds->Close();

            group->usage = accountant.GetWeightedResourcesUsed(group->name.c_str());
            group->priority = accountant.GetPriority(group->name.c_str());
        }


        // cycle through the submitter ads, and load them into the appropriate group node in the tree
        dprintf(D_ALWAYS, "group quotas: assigning %d submitters to accounting groups\n", int(scheddAds.MyLength()));
        scheddAds.Open();
        while (ClassAd* ad = scheddAds.Next()) {
            MyString tname;
            if (!ad->LookupString(ATTR_NAME, tname)) {
                dprintf(D_ALWAYS, "group quotas: WARNING: ignoring submitter ad with no name\n");
                continue;
            }
            // this holds the submitter name, which includes group, if present
            const string subname(tname.Value());

            // is there a username separator?
            string::size_type pos = subname.find_last_of('@');
            if (pos==string::npos) {
                dprintf(D_ALWAYS, "group quotas: WARNING: ignoring submitter with badly-formed name \"%s\"\n", subname.c_str());
                continue;
            }

            GroupEntry* group = accountant.GetAssignedGroup(subname.c_str());

            // attach the submitter ad to the assigned group
            group->submitterAds->Insert(ad);

            // Accumulate the submitter jobs submitted against this group
            // To do: investigate getting these values directly from schedds.  The
            // collector info can be a bit stale, direct from schedd might be improvement.
            int numidle=0;
            ad->LookupInteger(ATTR_IDLE_JOBS, numidle);
            int numrunning=0;
            ad->LookupInteger(ATTR_RUNNING_JOBS, numrunning);

			// The HGQ codes uses number of idle jobs to determine how to allocate
			// surplus.  This should really be weighted demand when slot weights
			// and paritionable slot are in use.  The schedd can tell us the cpu-weighed
			// demand in ATTR_WEIGHTED_IDLE_JOBS.  If this knob is set, use it.

			if (param_boolean("NEGOTIATOR_USE_WEIGHTED_DEMAND", false)) {
				int weightedIdle = numidle;
				int weightedRunning = numrunning;

				ad->LookupInteger(ATTR_WEIGHTED_IDLE_JOBS, weightedIdle);
				ad->LookupInteger(ATTR_WEIGHTED_RUNNING_JOBS, weightedRunning);

            	group->requested += weightedRunning + weightedIdle;
			} else {
            	group->requested += numrunning + numidle;
			}
			group->currently_requested = group->requested;
        }

        // Any groups with autoregroup are allowed to also negotiate in root group ("none")
        if (autoregroup) {
            unsigned long n = 0;
            for (vector<GroupEntry*>::iterator j(hgq_groups.begin());  j != hgq_groups.end();  ++j) {
                GroupEntry* group = *j;
                if (group == hgq_root_group) continue;
                if (!group->autoregroup) continue;
                group->submitterAds->Open();
                while (ClassAd* ad = group->submitterAds->Next()) {
                    hgq_root_group->submitterAds->Insert(ad);
                }
                group->submitterAds->Close();
                ++n;
            }
            dprintf(D_ALWAYS, "group quotas: autoregroup mode: appended %lu submitters to group %s negotiation\n", n, hgq_root_group->name.c_str());
        }

        // assign slot quotas based on the config-quotas
        double hgq_total_quota = (accountant.UsingWeightedSlots()) ? weightedPoolsize : effectivePoolsize;
        dprintf(D_ALWAYS, "group quotas: assigning group quotas from %g available%s slots\n",
                hgq_total_quota, 
                (accountant.UsingWeightedSlots()) ? " weighted" : "");
        hgq_assign_quotas(hgq_root_group, hgq_total_quota);

        for (vector<GroupEntry*>::iterator j(hgq_groups.begin());  j != hgq_groups.end();  ++j) {
            GroupEntry* group = *j;
            dprintf(D_FULLDEBUG, "group quotas: group= %s  cquota= %g  static= %d  accept= %d  quota= %g  req= %g  usage= %g\n",
                    group->name.c_str(), group->config_quota, int(group->static_quota), int(group->accept_surplus), group->quota, 
                    group->requested, group->usage);
        }

        // A user/admin can set this to > 1, to allow the algorithm an opportunity to re-distribute
        // slots that were not used due to rejection.
        int maxrounds = 0;
        if (param_defined("GROUP_QUOTA_MAX_ALLOCATION_ROUNDS")) {
            maxrounds = param_integer("GROUP_QUOTA_MAX_ALLOCATION_ROUNDS", 3, 1, INT_MAX);
        } else {
            // backward compatability
            maxrounds = param_integer("HFS_MAX_ALLOCATION_ROUNDS", 3, 1, INT_MAX);
        }

        // The allocation of slots may occur multiple times, if rejections
        // prevent some allocations from being filled.
        int iter = 0;
        while (true) {
            if (iter >= maxrounds) {
                dprintf(D_ALWAYS, "group quotas: halting allocation rounds after %d iterations\n", iter);
                break;
            }

            iter += 1;
            dprintf(D_ALWAYS, "group quotas: allocation round %d\n", iter);
            negotiation_cycle_stats[0]->slot_share_iterations += 1;

            // make sure working values are reset for this iteration
            groupQuotasHash->clear();
            for (vector<GroupEntry*>::iterator j(hgq_groups.begin());  j != hgq_groups.end();  ++j) {
                GroupEntry* group = *j;
                group->allocated = 0;
                group->subtree_requested = 0;
                group->rr = false;
            }

            // Allocate group slot quotas to satisfy group job requests
            double surplus_quota = hgq_fairshare(hgq_root_group);

            // This step is not relevant in a weighted-slot scenario, where slots may
            // have a floating-point cost != 1.
            if (!accountant.UsingWeightedSlots()) {
                // Recover any fractional slot remainders from fairshare algorithm, 
                // and distribute them using round robin.
                surplus_quota += hgq_recover_remainders(hgq_root_group);
            }

            if (autoregroup) {
                dprintf(D_ALWAYS, "group quotas: autoregroup mode: allocating %g to group %s\n", hgq_total_quota, hgq_root_group->name.c_str());
                hgq_root_group->quota = hgq_total_quota;
                hgq_root_group->allocated = hgq_total_quota;
            }

            double maxdelta = 0;
            double requested_total = 0;
            double allocated_total = 0;
            unsigned long served_groups = 0;
            unsigned long unserved_groups = 0;
            for (vector<GroupEntry*>::iterator j(hgq_groups.begin());  j != hgq_groups.end();  ++j) {
                GroupEntry* group = *j;
                dprintf(D_FULLDEBUG, "group quotas: group= %s  quota= %g  requested= %g  allocated= %g  unallocated= %g\n",
                        group->name.c_str(), group->quota, group->requested+group->allocated, group->allocated, group->requested);
                groupQuotasHash->insert(MyString(group->name.c_str()), group->quota);
                requested_total += group->requested;
                allocated_total += group->allocated;
                if (group->allocated > 0) served_groups += 1;
                else if (group->requested > 0) unserved_groups += 1;
                double target = (accept_surplus) ? group->allocated : group->quota;
                maxdelta = std::max(maxdelta, std::max(0.0, target - group->usage));
            }

            dprintf(D_ALWAYS, "group quotas: groups= %lu  requesting= %lu  served= %lu  unserved= %lu  slots= %g  requested= %g  allocated= %g  surplus= %g\n", 
                    static_cast<long unsigned int>(hgq_groups.size()), served_groups+unserved_groups, served_groups, unserved_groups, double(effectivePoolsize), requested_total+allocated_total, allocated_total, surplus_quota);

            // The loop below can add a lot of work (and log output) to the negotiation.  I'm going to
            // default its behavior to execute once, and just negotiate for everything at once.  If a
            // user is concerned about the "overlapping effective pool" problem, they can decrease this 
            // increment so that round robin happens, and competing groups will not starve one another.
            double ninc = 0;
            if (param_defined("GROUP_QUOTA_ROUND_ROBIN_RATE")) {
                ninc = param_double("GROUP_QUOTA_ROUND_ROBIN_RATE", DBL_MAX, 1.0, DBL_MAX);
            } else {
                // backward compatability 
                ninc = param_double("HFS_ROUND_ROBIN_RATE", DBL_MAX, 1.0, DBL_MAX);
            }

            // fill in sorting classad attributes for configurable sorting
            for (vector<GroupEntry*>::iterator j(hgq_groups.begin());  j != hgq_groups.end();  ++j) {
                GroupEntry* group = *j;
                ClassAd* ad = group->sort_ad;
                ad->Assign(ATTR_GROUP_QUOTA, group->quota);
                ad->Assign(ATTR_GROUP_RESOURCES_ALLOCATED, group->allocated);
                ad->Assign(ATTR_GROUP_RESOURCES_IN_USE, accountant.GetWeightedResourcesUsed(group->name));
                // Do this after all attributes are filled in
                float v = 0;
                if (!ad->EvalFloat(ATTR_SORT_EXPR, NULL, v)) {
                    v = FLT_MAX;
                    string e;
                    ad->LookupString(ATTR_SORT_EXPR_STRING, e);
                    dprintf(D_ALWAYS, "WARNING: sort expression \"%s\" failed to evaluate to floating point for group %s - defaulting to %g\n",
                            e.c_str(), group->name.c_str(), v);
                }
                group->sort_key = v;
            }

            // present accounting groups for negotiation in "starvation order":
            vector<GroupEntry*> negotiating_groups(hgq_groups);
            std::sort(negotiating_groups.begin(), negotiating_groups.end(), group_order(autoregroup, hgq_root_group));

            // This loop implements "weighted round-robin" behavior to gracefully handle case of multiple groups competing
            // for same subset of available slots.  It gives greatest weight to groups with the greatest difference 
            // between allocated and their current usage
            double n = 0;
            while (true) {
                // Up our fraction of the full deltas.  Note that maxdelta may be zero, but we still
                // want to negotiate at least once regardless, so loop halting check is at the end.
                n = std::min(n+ninc, maxdelta);
                dprintf(D_FULLDEBUG, "group quotas: entering RR iteration n= %g\n", n);

                // Do the negotiations
                for (vector<GroupEntry*>::iterator j(negotiating_groups.begin());  j != negotiating_groups.end();  ++j) {
                    GroupEntry* group = *j;

                    dprintf(D_FULLDEBUG, "Group %s - sortkey= %g\n", group->name.c_str(), group->sort_key);

                    if (group->allocated <= 0) {
                        dprintf(D_ALWAYS, "Group %s - skipping, zero slots allocated\n", group->name.c_str());
                        continue;
                    }

                    if ((group->usage >= group->allocated) && !ConsiderPreemption) {
                        dprintf(D_ALWAYS, "Group %s - skipping, at or over quota (usage=%g) (quota=%g)\n", group->name.c_str(), group->usage, group->allocated);
                        continue;
                    }
		    
                    if (group->submitterAds->MyLength() <= 0) {
                        dprintf(D_ALWAYS, "Group %s - skipping, no submitters (usage=%g)\n", group->name.c_str(), group->usage);
                        continue;
                    }
		    
                    dprintf(D_ALWAYS, "Group %s - BEGIN NEGOTIATION\n", group->name.c_str());

                    // if allocating surplus, use allocated, otherwise just use the group's quota directly
                    double target = (accept_surplus) ? group->allocated : group->quota;

                    double delta = std::max(0.0, target - group->usage);
                    // If delta > 0, we know maxdelta also > 0.  Otherwise, it means we actually are using more than
                    // we just got allocated, so just negotiate for what we were allocated.
                    double slots = (delta > 0) ? group->usage + (delta * (n / maxdelta)) : target;
                    // Defensive -- do not exceed allocated slots
                    slots = std::min(slots, target);
                    if (!accountant.UsingWeightedSlots()) {
                        slots = floor(slots);
                    }
					
					if (param_boolean("NEGOTIATOR_STRICT_ENFORCE_QUOTA", true)) {
						dprintf(D_FULLDEBUG, "NEGOTIATOR_STRICT_ENFORCE_QUOTA is true, current proposed allocation for %s is %g\n", group->name.c_str(), slots);
						calculate_subtree_usage(hgq_root_group); // usage changes with every negotiation
						GroupEntry *limitingGroup = group;

						double my_new_allocation = slots - group->usage; // resources above what we already have
						if (my_new_allocation < 0) {
							continue; // shouldn't get here
						}

						while (limitingGroup != NULL) {
							if (limitingGroup->accept_surplus == false) {
								// This is the extra available at this node
								double subtree_available = -1;
								if (limitingGroup->static_quota) {
									subtree_available = limitingGroup->config_quota - limitingGroup->subtree_usage;
								} else {
									subtree_available = limitingGroup->subtree_quota - limitingGroup->subtree_usage;
								}
								if (subtree_available < 0) subtree_available = 0;
								dprintf(D_FULLDEBUG, "\tmy_new_allocation is %g subtree_available is %g\n", my_new_allocation, subtree_available);
								if (my_new_allocation > subtree_available) {
									dprintf(D_ALWAYS, "Group %s with accept_surplus=false has total usage = %g and config quota of %g -- constraining allocation in subgroup %s to %g\n",
											limitingGroup->name.c_str(), limitingGroup->subtree_usage, limitingGroup->config_quota, group->name.c_str(), subtree_available + group->usage);
		
									my_new_allocation = subtree_available; // cap new allocation to the available
								}
							}
							limitingGroup = limitingGroup->parent;
						}
						slots = my_new_allocation + group->usage; // negotiation units are absolute quota, not new
					}

                    if (autoregroup && (group == hgq_root_group)) {
                        // note that in autoregroup mode, root group is guaranteed to be last group to negotiate
                        dprintf(D_ALWAYS, "group quotas: autoregroup mode: negotiating with autoregroup for %s\n", group->name.c_str());
                        negotiateWithGroup(cPoolsize, weightedPoolsize, minSlotWeight,
                                           startdAds, claimIds, *(group->submitterAds),
                                           slots, NULL);
                    } else {
                        negotiateWithGroup(cPoolsize, weightedPoolsize, minSlotWeight,
                                           startdAds, claimIds, *(group->submitterAds), 
                                           slots, group->name.c_str());
                    }
                }

                // Halt when we have negotiated with full deltas
                if (n >= maxdelta) break;
            }

            // After round robin, assess where we are relative to HGQ allocation goals
            double usage_total = 0;
            for (vector<GroupEntry*>::iterator j(hgq_groups.begin());  j != hgq_groups.end();  ++j) {
                GroupEntry* group = *j;

                double usage = accountant.GetWeightedResourcesUsed(group->name.c_str());

                group->usage = usage;
                dprintf(D_FULLDEBUG, "group quotas: Group %s  allocated= %g  usage= %g\n", group->name.c_str(), group->allocated, group->usage);

                // I do not want to give credit for usage above what was allocated here.
                usage_total += std::min(group->usage, group->allocated);

                if (group->usage < group->allocated) {
                    // If we failed to match all the allocated slots for any reason, then take what we
                    // got and allow other groups a chance at the rest on next iteration
                    dprintf(D_FULLDEBUG, "group quotas: Group %s - resetting requested to %g\n", group->name.c_str(), group->usage);
                    group->requested = group->usage;
                } else {
                    // otherwise restore requested to its original state for next iteration
                    group->requested += group->allocated;
                }
            }

            dprintf(D_ALWAYS, "Round %d totals: allocated= %g  usage= %g\n", iter, allocated_total, usage_total);

            // If we negotiated successfully for all slots, we're finished
            if (usage_total >= allocated_total) break;
        }

        // For the purposes of RR consistency I want to update these after all allocation rounds are completed.
        for (vector<GroupEntry*>::iterator j(hgq_groups.begin());  j != hgq_groups.end();  ++j) {
            GroupEntry* group = *j;
            // If we were served by RR this cycle, then update timestamp of most recent round-robin.  
            // I also update when requested is zero because I want to favor groups that have been actually
            // waiting for an allocation the longest.
            if (group->rr || (group->requested <= 0))  group->rr_time = negotiation_cycle_stats[0]->start_time;
        }
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

    completedLastCycleTime = time(NULL);

    negotiation_cycle_stats[0]->end_time = completedLastCycleTime;

    // Phase 2 is time to do "all of the above" since end of phase 1, less the time we spent in phase 3 and phase 4
    // (phase 3 and 4 occur inside of negotiateWithGroup(), which may be called in multiple places, inside looping)
    negotiation_cycle_stats[0]->duration_phase2 = completedLastCycleTime - start_time_phase2;
    negotiation_cycle_stats[0]->duration_phase2 -= negotiation_cycle_stats[0]->duration_phase3;
    negotiation_cycle_stats[0]->duration_phase2 -= negotiation_cycle_stats[0]->duration_phase4;

    negotiation_cycle_stats[0]->duration = completedLastCycleTime - negotiation_cycle_stats[0]->start_time;

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

    // reduce negotiator delay drift
    daemonCore->Reset_Timer(negotiation_timerID, 
                            std::max(cycle_delay,  NegotiatorInterval - negotiation_cycle_stats[0]->duration), 
                            NegotiatorInterval);
}


void Matchmaker::hgq_construct_tree() {
	// need to construct group structure
	// groups is list of group names
    // in form group.subgroup group.subgroup.subgroup etc
	char* groupnames = param("GROUP_NAMES");

	// Populate the group array, which contains an entry for each group.
    hgq_root_name = "<none>";
	vector<string> groups;
    if (NULL != groupnames) {
        StringList group_name_list;
        group_name_list.initializeFromString(groupnames);
        group_name_list.rewind();
        while (char* g = group_name_list.next()) {
            const string gname(g);

            // Best to sanity-check this as early as possible.  This will also
            // be useful if we ever decided to allow users to name the root group
            if (gname == hgq_root_name) {
                dprintf(D_ALWAYS, "group quotas: ERROR: group name \"%s\" is reserved for root group -- ignoring this group\n", gname.c_str());
                continue;
            }

            // store the group name
            groups.push_back(gname);
        }

        free(groupnames);
        groupnames = NULL;
    }

    // This is convenient for making sure a parent group always appears before its children
    std::sort(groups.begin(), groups.end(), Accountant::ci_less());

    // our root group always exists -- all configured HGQ groups are implicitly 
    // children / descendents of the root
    if (NULL != hgq_root_group) delete hgq_root_group;
    hgq_root_group = new GroupEntry;
	hgq_root_group->name = hgq_root_name;
    hgq_root_group->accept_surplus = true;

    group_entry_map.clear();
    group_entry_map[hgq_root_name] = hgq_root_group;

    allow_quota_oversub = param_boolean("NEGOTIATOR_ALLOW_QUOTA_OVERSUBSCRIPTION", false);

    accept_surplus = false;
    autoregroup = false;
    const bool default_accept_surplus = param_boolean("GROUP_ACCEPT_SURPLUS", false);
    const bool default_autoregroup = param_boolean("GROUP_AUTOREGROUP", false);
    if (default_autoregroup) autoregroup = true;
    if (default_accept_surplus) accept_surplus = true;

    // build the tree structure from our group path info
    for (unsigned long j = 0;  j < groups.size();  ++j) {
        string gname = groups[j];

        // parse the group name into a path of sub-group names
        vector<string> gpath;
        parse_group_name(gname, gpath);

        // insert the path of the current group into the tree structure
        GroupEntry* group = hgq_root_group;
        bool missing_parent = false;
        for (unsigned long k = 0;  k < gpath.size()-1;  ++k) {
            // chmap is mostly a structure to avoid n^2 behavior in groups with many children
            map<string, GroupEntry::size_type, Accountant::ci_less>::iterator f(group->chmap.find(gpath[k]));
            if (f == group->chmap.end()) {
                dprintf(D_ALWAYS, "group quotas: WARNING: ignoring group name %s with missing parent %s\n", gname.c_str(), gpath[k].c_str());
                missing_parent = true;
                break;
            }
            group = group->children[f->second];
        }
        if (missing_parent) continue;

        if (group->chmap.count(gpath.back()) > 0) {
            // duplicate group -- ignore
            dprintf(D_ALWAYS, "group quotas: WARNING: ignoring duplicate group name %s\n", gname.c_str());
            continue;
        }

        // enter the new group
        group->children.push_back(new GroupEntry);
        group->chmap[gpath.back()] = group->children.size()-1;
        group_entry_map[gname] = group->children.back();
        group->children.back()->parent = group;
        group = group->children.back();

        // "group" now refers to our current group in the list.
        // Fill in entry values from config.
        group->name = gname;

        // group quota setting 
        MyString vname;
        vname.formatstr("GROUP_QUOTA_%s", gname.c_str());
        double quota = param_double(vname.Value(), -1.0, 0, INT_MAX);
        if (quota >= 0) {
            group->config_quota = quota;
            group->static_quota = true;
        } else {
            vname.formatstr("GROUP_QUOTA_DYNAMIC_%s", gname.c_str());
            quota = param_double(vname.Value(), -1.0, 0.0, 1.0);
            if (quota >= 0) {
                group->config_quota = quota;
                group->static_quota = false;
            } else {
                dprintf(D_ALWAYS, "group quotas: WARNING: no quota specified for group \"%s\", defaulting to zero\n", gname.c_str());
                group->config_quota = 0.0;
                group->static_quota = false;
            }
        }

        // defensive sanity checking
        if (group->config_quota < 0) {
            dprintf(D_ALWAYS, "group quotas: ERROR: negative quota (%g) defaulting to zero\n", double(group->config_quota));
            group->config_quota = 0;
        }

        // accept surplus
	    vname.formatstr("GROUP_ACCEPT_SURPLUS_%s", gname.c_str());
        group->accept_surplus = param_boolean(vname.Value(), default_accept_surplus);
	    vname.formatstr("GROUP_AUTOREGROUP_%s", gname.c_str());
        group->autoregroup = param_boolean(vname.Value(), default_autoregroup);
        if (group->autoregroup) autoregroup = true;
        if (group->accept_surplus) accept_surplus = true;
    }

    // Set the root group's autoregroup state to match the effective global value for autoregroup
    // we do this for the benefit of the accountant, it also can be use to remove some special cases
    // in the negotiator loops.
    hgq_root_group->autoregroup = autoregroup;

    // With the tree structure in place, we can make a list of groups in breadth-first order
    // For more convenient iteration over the structure
    hgq_groups.clear();
    std::deque<GroupEntry*> grpq;
    grpq.push_back(hgq_root_group);
    while (!grpq.empty()) {
        GroupEntry* group = grpq.front();
        grpq.pop_front();
        hgq_groups.push_back(group);
        for (vector<GroupEntry*>::iterator j(group->children.begin());  j != group->children.end();  ++j) {
            grpq.push_back(*j);
        }
    }

    string group_sort_expr;
    if (!param(group_sort_expr, "GROUP_SORT_EXPR")) {
        // Should never fail! Default provided via param-info
        EXCEPT("Failed to obtain value for GROUP_SORT_EXPR\n");
    }
    ExprTree* test_sort_expr = NULL;
    if (ParseClassAdRvalExpr(group_sort_expr.c_str(), test_sort_expr)) {
        EXCEPT("Failed to parse GROUP_SORT_EXPR = %s\n", group_sort_expr.c_str());
    }
    delete test_sort_expr;
    for (vector<GroupEntry*>::iterator j(hgq_groups.begin());  j != hgq_groups.end();  ++j) {
        GroupEntry* group = *j;
        group->sort_ad->Assign(ATTR_ACCOUNTING_GROUP, group->name);
        // group-specific values might be supported in the future:
        group->sort_ad->AssignExpr(ATTR_SORT_EXPR, group_sort_expr.c_str());
        group->sort_ad->Assign(ATTR_SORT_EXPR_STRING, group_sort_expr);
    }
}


void Matchmaker::hgq_assign_quotas(GroupEntry* group, double quota) {
    dprintf(D_FULLDEBUG, "group quotas: subtree %s receiving quota= %g\n", group->name.c_str(), quota);

    // if quota is zero, we can leave this subtree with default quotas of zero
    if (quota <= 0) return;

    // incoming quota is quota for subtree
    group->subtree_quota = quota;

    // compute the sum of any static quotas of any children
    double sqsum = 0;
    double dqsum = 0;
    for (unsigned long j = 0;  j < group->children.size();  ++j) {
        GroupEntry* child = group->children[j];
        if (child->static_quota) {
            sqsum += child->config_quota;
        } else {
            dqsum += child->config_quota;
        }
    }

    // static quotas get first dibs on any available quota
    // total static quota assignable is bounded by quota coming from above
    double sqa = (allow_quota_oversub) ? sqsum : std::min(sqsum, quota);

    // children with dynamic quotas get allocated from the remainder 
    double dqa = std::max(0.0, quota - sqa);

    dprintf(D_FULLDEBUG, "group quotas: group %s, allocated %g for static children, %g for dynamic children\n", group->name.c_str(), sqa, dqa);

    // Prevent (0/0) in the case of all static quotas == 0.
    // In this case, all quotas will still be correctly assigned zero.
    double Zs = (sqsum > 0) ? sqsum : 1;

    // If dqsum exceeds 1, then dynamic quota values get scaled so that they sum to 1
    double Zd = std::max(dqsum, double(1));

    // quota assigned to all children 
    double chq = 0;
    for (unsigned long j = 0;  j < group->children.size();  ++j) {
        GroupEntry* child = group->children[j];
        // Each child with a static quota gets its proportion of the total of static quota assignable.
        // Each child with dynamic quota gets the dynamic quota assignable weighted by its configured dynamic quota value
        double q = (child->static_quota) ? (child->config_quota * (sqa / Zs)) : (child->config_quota * (dqa / Zd));
        if (q < 0) q = 0;

        if (child->static_quota && (q < child->config_quota)) {
            dprintf(D_ALWAYS, "group quotas: WARNING: static quota for group %s rescaled from %g to %g\n", child->name.c_str(), child->config_quota, q);
        } else if (Zd - 1 > 0.0001) {
            dprintf(D_ALWAYS, "group quotas: WARNING: dynamic quota for group %s rescaled from %g to %g\n", child->name.c_str(), child->config_quota, child->config_quota / Zd);
        }

        hgq_assign_quotas(child, q);
        chq += q;
    }

    // Current group gets anything remaining after assigning to any children
    // If there are no children (a leaf) then this group gets all the quota
    group->quota = (allow_quota_oversub) ? quota : (quota - chq);
    if (group->quota < 0) group->quota = 0;
    dprintf(D_FULLDEBUG, "group quotas: group %s assigned quota= %g\n", group->name.c_str(), group->quota);
}


double Matchmaker::hgq_fairshare(GroupEntry* group) {
    dprintf(D_FULLDEBUG, "group quotas: fairshare (1): group= %s  quota= %g  requested= %g\n", 
            group->name.c_str(), group->quota, group->requested);

    // Allocate whichever is smallest: the requested slots or group quota.
    group->allocated = std::min(group->requested, group->quota);

    // update requested values
    group->requested -= group->allocated;
    group->subtree_requested = group->requested;

    // surplus quota for this group
    double surplus = group->quota - group->allocated;

    dprintf(D_FULLDEBUG, "group quotas: fairshare (2): group= %s  quota= %g  allocated= %g  requested= %g\n", 
            group->name.c_str(), group->quota, group->allocated, group->requested);

    // If this is a leaf group, we're finished: return the surplus
    if (group->children.empty()) return surplus;

    // This is an internal group: perform fairshare recursively on children
    for (unsigned long j = 0;  j < group->children.size();  ++j) {
        GroupEntry* child = group->children[j];
        surplus += hgq_fairshare(child);
        if (child->accept_surplus) {
            group->subtree_requested += child->subtree_requested;
        }
    }

    // allocate any available surplus to current node and subtree
    surplus = hgq_allocate_surplus(group, surplus);

    dprintf(D_FULLDEBUG, "group quotas: fairshare (3): group= %s  surplus= %g  subtree_requested= %g\n", 
            group->name.c_str(), surplus, group->subtree_requested);

    // return any remaining surplus up the tree
    return surplus;
}


void hgq_allocate_surplus_loop(bool by_quota, 
                               vector<GroupEntry*>& groups, vector<double>& allocated, vector<double>& subtree_requested, 
                               double& surplus, double& requested) {
    int iter = 0;
    while (surplus > 0) {
        iter += 1;

        dprintf(D_FULLDEBUG, "group quotas: allocate-surplus-loop: by_quota= %d  iteration= %d  requested= %g  surplus= %g\n", 
                int(by_quota), iter, requested, surplus);

        // Compute the normalizer for outstanding groups
        double Z = 0;
        for (unsigned long j = 0;  j < groups.size();  ++j) {
            GroupEntry* grp = groups[j];
            if (subtree_requested[j] > 0)  Z += (by_quota) ? grp->subtree_quota : 1.0;
        }

        if (Z <= 0) {
            dprintf(D_FULLDEBUG, "group quotas: allocate-surplus-loop: no further outstanding groups at iteration %d - halting.\n", iter);
            break;
        }

        // allocations
        bool never_gt = true;
        double sumalloc = 0;
        for (unsigned long j = 0;  j < groups.size();  ++j) {
            GroupEntry* grp = groups[j];
            if (subtree_requested[j] > 0) {
                double N = (by_quota) ? grp->subtree_quota : 1.0;
                double a = surplus * (N / Z);
                if (a > subtree_requested[j]) {
                    a = subtree_requested[j];
                    never_gt = false;
                }
                allocated[j] += a;
                subtree_requested[j] -= a;
                sumalloc += a;
            }
        }

        surplus -= sumalloc;
        requested -= sumalloc;

        // Compensate for numeric precision jitter
        // This is part of the convergence guarantee: on each iteration, one of two things happens:
        // either never_gt becomes true, in which case all surplus was allocated, or >= 1 group had its
        // requested drop to zero.  This will move us toward Z becoming zero, which will halt the loop.
        // Note, that in "by-quota" mode, Z can become zero with surplus remaining, which is fine -- it means
        // groups with quota > 0 did not use all the surplus, and any groups with zero quota have the option
        // to use it in "non-by-quota" mode.
        if (never_gt || (surplus < 0)) {
            if (fabs(surplus) > 0.00001) {
                dprintf(D_ALWAYS, "group quotas: allocate-surplus-loop: WARNING: rounding surplus= %g to zero\n", surplus);
            }
            surplus = 0;
        }
    }
}


double Matchmaker::hgq_allocate_surplus(GroupEntry* group, double surplus) {
    dprintf(D_FULLDEBUG, "group quotas: allocate-surplus (1): group= %s  surplus= %g  subtree-requested= %g\n", group->name.c_str(), surplus, group->subtree_requested);

    // Nothing to allocate
    if (surplus <= 0) return 0;

    // If entire subtree requests nothing, halt now
    if (group->subtree_requested <= 0) return surplus;

    // Surplus allocation policy is that a group shares surplus on equal footing with its children.
    // So we load children and their parent (current group) into a single vector for treatment.
    // Convention will be that current group (subtree root) is last element.
    vector<GroupEntry*> groups(group->children);
    groups.push_back(group);

    // This vector will accumulate allocations.
    // We will proceed with recursive allocations after allocations at this level
    // are completed.  This keeps recursive calls to a minimum.
    vector<double> allocated(groups.size(), 0);

    // Temporarily hacking current group to behave like a child that accepts surplus 
    // avoids some special cases below.  Somewhere I just made a kitten cry.
    bool save_accept_surplus = group->accept_surplus;
    group->accept_surplus = true;
    double save_subtree_quota = group->subtree_quota;
    group->subtree_quota = group->quota;
    double requested = group->subtree_requested;
    group->subtree_requested = group->requested;

    if (surplus >= requested) {
        // In this scenario we have enough surplus to satisfy all requests.
        // Cornucopia! Give everybody what they asked for.

        dprintf(D_FULLDEBUG, "group quotas: allocate-surplus (2a): direct allocation, group= %s  requested= %g  surplus= %g\n",
                group->name.c_str(), requested, surplus);

        for (unsigned long j = 0;  j < groups.size();  ++j) {
            GroupEntry* grp = groups[j];
            if (grp->accept_surplus && (grp->subtree_requested > 0)) {
                allocated[j] = grp->subtree_requested;
            }
        }

        surplus -= requested;
        requested = 0;
    } else {
        // In this scenario there are more requests than there is surplus.
        // Here groups have to compete based on their quotas.

        dprintf(D_FULLDEBUG, "group quotas: allocate-surplus (2b): quota-based allocation, group= %s  requested= %g  surplus= %g\n", 
                group->name.c_str(), requested, surplus);

        vector<double> subtree_requested(groups.size(), 0);
        for (unsigned long j = 0;  j < groups.size();  ++j) {
            GroupEntry* grp = groups[j];
            // By conditioning on accept_surplus here, I don't have to check it below
            if (grp->accept_surplus && (grp->subtree_requested > 0)) {
                subtree_requested[j] = grp->subtree_requested;
            }
        }

        // In this loop we allocate to groups with quota > 0
        hgq_allocate_surplus_loop(true, groups, allocated, subtree_requested, surplus, requested);

        // Any quota left can be allocated to groups with zero quota
        hgq_allocate_surplus_loop(false, groups, allocated, subtree_requested, surplus, requested);

        // There should be no surplus left after the above two rounds
        if (surplus > 0) {
            dprintf(D_ALWAYS, "group quotas: allocate-surplus WARNING: nonzero surplus %g after allocation\n", surplus);
        }
    }

    // We have computed allocations for groups, with results cached in 'allocated'
    // Now we can perform the actual allocations.  Only actual children should
    // be allocated recursively here
    for (unsigned long j = 0;  j < (groups.size()-1);  ++j) {
        if (allocated[j] > 0) {
            double s = hgq_allocate_surplus(groups[j], allocated[j]);
            if (fabs(s) > 0.00001) {
                dprintf(D_ALWAYS, "group quotas: WARNING: allocate-surplus (3): surplus= %g\n", s);
            }
        }
    }

    // Here is logic for allocating current group
    group->allocated += allocated.back();
    group->requested -= allocated.back();

    dprintf(D_FULLDEBUG, "group quotas: allocate-surplus (4): group %s allocated surplus= %g  allocated= %g  requested= %g\n",
            group->name.c_str(), allocated.back(), group->allocated, group->requested);

    // restore proper group settings
    group->subtree_requested = requested;
    group->accept_surplus = save_accept_surplus;
    group->subtree_quota = save_subtree_quota;

    return surplus;
}


double Matchmaker::hgq_recover_remainders(GroupEntry* group) {
    dprintf(D_FULLDEBUG, "group quotas: recover-remainders (1): group= %s  allocated= %g  requested= %g\n", 
            group->name.c_str(), group->allocated, group->requested);

    // recover fractional remainder, which becomes surplus
    double surplus = group->allocated - floor(group->allocated);
    group->allocated -= surplus;
    group->requested += surplus;

    // These should be integer values now, so I get to round to correct any precision errs
    round_for_precision(group->allocated);
    round_for_precision(group->requested);

    group->subtree_requested = group->requested;
    group->subtree_rr_time = (group->requested > 0) ? group->rr_time : DBL_MAX;

    dprintf(D_FULLDEBUG, "group quotas: recover-remainders (2): group= %s  allocated= %g  requested= %g  surplus= %g\n", 
            group->name.c_str(), group->allocated, group->requested, surplus);

    // If this is a leaf group, we're finished: return the surplus
    if (group->children.empty()) return surplus;

    // This is an internal group: perform recovery recursively on children
    for (unsigned long j = 0;  j < group->children.size();  ++j) {
        GroupEntry* child = group->children[j];
        surplus += hgq_recover_remainders(child);
        if (child->accept_surplus) {
            group->subtree_requested += child->subtree_requested;
            if (child->subtree_requested > 0)
                group->subtree_rr_time = std::min(group->subtree_rr_time, child->subtree_rr_time);
        }
    }

    // allocate any available surplus to current node and subtree
    surplus = hgq_round_robin(group, surplus);

    dprintf(D_FULLDEBUG, "group quotas: recover-remainder (3): group= %s  surplus= %g  subtree_requested= %g\n", 
            group->name.c_str(), surplus, group->subtree_requested);

    // return any remaining surplus up the tree
    return surplus;
}


double Matchmaker::hgq_round_robin(GroupEntry* group, double surplus) {
    dprintf(D_FULLDEBUG, "group quotas: round-robin (1): group= %s  surplus= %g  subtree-requested= %g\n", group->name.c_str(), surplus, group->subtree_requested);

    // Sanity check -- I expect these to be integer values by the time I get here.
    if (group->subtree_requested != floor(group->subtree_requested)) {
        dprintf(D_ALWAYS, "group quotas: WARNING: forcing group %s requested= %g to integer value %g\n", 
                group->name.c_str(), group->subtree_requested, floor(group->subtree_requested));
        group->subtree_requested = floor(group->subtree_requested);
    }

    // Nothing to do if subtree had no requests
    if (group->subtree_requested <= 0) return surplus;

    // round robin has nothing to do without at least one whole slot
    if (surplus < 1) return surplus;

    // Surplus allocation policy is that a group shares surplus on equal footing with its children.
    // So we load children and their parent (current group) into a single vector for treatment.
    // Convention will be that current group (subtree root) is last element.
    vector<GroupEntry*> groups(group->children);
    groups.push_back(group);

    // This vector will accumulate allocations.
    // We will proceed with recursive allocations after allocations at this level
    // are completed.  This keeps recursive calls to a minimum.
    vector<double> allocated(groups.size(), 0);

    // Temporarily hacking current group to behave like a child that accepts surplus 
    // avoids some special cases below.  Somewhere I just made a kitten cry.  Even more.
    bool save_accept_surplus = group->accept_surplus;
    group->accept_surplus = true;
    double save_subtree_quota = group->subtree_quota;
    group->subtree_quota = group->quota;
    double save_subtree_rr_time = group->subtree_rr_time;
    group->subtree_rr_time = group->rr_time;
    double requested = group->subtree_requested;
    group->subtree_requested = group->requested;

    double outstanding = 0;
    vector<double> subtree_requested(groups.size(), 0);
    for (unsigned long j = 0;  j < groups.size();  ++j) {
        GroupEntry* grp = groups[j];
        if (grp->accept_surplus && (grp->subtree_requested > 0)) {
            subtree_requested[j] = grp->subtree_requested;
            outstanding += 1;
        }
    }

    // indexes allow indirect sorting
    vector<unsigned long> idx(groups.size());
    for (unsigned long j = 0;  j < idx.size();  ++j) idx[j] = j;

    // order the groups to determine who gets first cut
    ord_by_rr_time ord;
    ord.data = &groups;
    std::sort(idx.begin(), idx.end(), ord);

    while ((surplus >= 1) && (requested > 0)) {
        // max we can fairly allocate per group this round:
        double amax = std::max(double(1), floor(surplus / outstanding));

        dprintf(D_FULLDEBUG, "group quotas: round-robin (2): pass: surplus= %g  requested= %g  outstanding= %g  amax= %g\n", 
                surplus, requested, outstanding, amax);

        outstanding = 0;
        double sumalloc = 0;
        for (unsigned long jj = 0;  jj < groups.size();  ++jj) {
            unsigned long j = idx[jj];
            GroupEntry* grp = groups[j];
            if (grp->accept_surplus && (subtree_requested[j] > 0)) {
                double a = std::min(subtree_requested[j], amax);
                allocated[j] += a;
                subtree_requested[j] -= a;
                sumalloc += a;
                surplus -= a;
                requested -= a;
                grp->rr = true;
                if (subtree_requested[j] > 0) outstanding += 1;
                if (surplus < amax) break;
            }
        }

        // a bit of defensive sanity checking -- should not be possible:
        if (sumalloc < 1) {
            dprintf(D_ALWAYS, "group quotas: round-robin (3): WARNING: round robin failed to allocate >= 1 slot this round - halting\n");
            break;
        }
    }

    // We have computed allocations for groups, with results cached in 'allocated'
    // Now we can perform the actual allocations.  Only actual children should
    // be allocated recursively here
    for (unsigned long j = 0;  j < (groups.size()-1);  ++j) {
        if (allocated[j] > 0) {
            double s = hgq_round_robin(groups[j], allocated[j]);

            // This algorithm does not allocate more than a child has requested.
            // Also, this algorithm is designed to allocate every requested slot,
            // up to the given surplus.  Therefore, I expect these calls to return
            // zero.   If they don't, something is haywire.
            if (s > 0) {
                dprintf(D_ALWAYS, "group quotas: round-robin (4):  WARNING: nonzero surplus %g returned from round robin for group %s\n", 
                        s, groups[j]->name.c_str());
            }
        }
    }

    // Here is logic for allocating current group
    group->allocated += allocated.back();
    group->requested -= allocated.back();

    dprintf(D_FULLDEBUG, "group quotas: round-robin (5): group %s allocated surplus= %g  allocated= %g  requested= %g\n",
            group->name.c_str(), allocated.back(), group->allocated, group->requested);

    // restore proper group settings
    group->subtree_requested = requested;
    group->accept_surplus = save_accept_surplus;
    group->subtree_quota = save_subtree_quota;
    group->subtree_rr_time = save_subtree_rr_time;

    return surplus;
}


GroupEntry::GroupEntry():
    name(),
    config_quota(0),
    static_quota(false),
    accept_surplus(false),
    autoregroup(false),
    usage(0),
    submitterAds(NULL),
    quota(0),
    requested(0),
    currently_requested(0),
    allocated(0),
    subtree_quota(0),
    subtree_requested(0),
    subtree_usage(0),
    rr(false),
    rr_time(0),
    subtree_rr_time(0),
    parent(NULL),
    children(),
    chmap(),
    sort_ad(new ClassAd())
{
}


GroupEntry::~GroupEntry() {
    for (unsigned long j=0;  j < children.size();  ++j) {
        if (children[j] != NULL) {
            delete children[j];
        }
    }

    if (NULL != submitterAds) {
        submitterAds->Open();
        while (ClassAd* ad = submitterAds->Next()) {
            submitterAds->Remove(ad);
        }
        submitterAds->Close();

        delete submitterAds;
    }

    if (NULL != sort_ad) delete sort_ad;
}


void filter_submitters_no_idle(ClassAdListDoesNotDeleteAds& submitterAds) {
	submitterAds.Open();
	while (ClassAd* ad = submitterAds.Next()) {
        int idle = 0;
        ad->LookupInteger(ATTR_IDLE_JOBS, idle);

        if (idle <= 0) {
            std::string submitterName;
            ad->LookupString(ATTR_NAME, submitterName);
            dprintf(D_FULLDEBUG, "Ignoring submitter %s with no idle jobs\n", submitterName.c_str());
            submitterAds.Remove(ad);
        }
    }
}

/*
 consolidate_globaljobprio_submitter_ads()
 Scan through scheddAds looking for globaljobprio submitter ads, consolidating
 them into a minimal set of submitter ads that contain JOBPRIO_MIN and
 JOBPRIO_MAX attributes to reflect job priority ranges.
 Return true on success and/or want_globaljobprio should be true,
 false if there is a data structure inconsistency and/or want_globaljobprio should be false.
*/
bool Matchmaker::
consolidate_globaljobprio_submitter_ads(ClassAdListDoesNotDeleteAds& scheddAds)
{
	// nothing to do if unless want_globaljobprio is true...
	if (!want_globaljobprio) {
		return false;  // keep want_globajobprio false
	}

	ClassAd *curr_ad = NULL;
	ClassAd *prev_ad = NULL;
	MyString curr_name, curr_addr, prev_name, prev_addr;
	int min_prio=INT_MAX, max_prio=INT_MIN; // initialize to shut gcc up, the loop always sets before using.

	scheddAds.Open();
	while ( (curr_ad = scheddAds.Next()) )
	{
		// skip this submitter if we cannot identify its origin
		if (!curr_ad->LookupString(ATTR_NAME,curr_name)) continue;
		if (!curr_ad->LookupString(ATTR_SCHEDD_IP_ADDR,curr_addr)) continue;

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
		if ( !curr_ad->Lookup(ATTR_JOB_PRIO_ARRAY) ) continue;

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
		scheddAds.Remove(curr_ad);
	}	// end of while iterate through scheddAds

	return true;
}

int Matchmaker::
negotiateWithGroup ( int untrimmed_num_startds,
					 double untrimmedSlotWeightTotal,
					 double minSlotWeight,
					 ClassAdListDoesNotDeleteAds& startdAds,
					 ClaimIdHash& claimIds, 
					 ClassAdListDoesNotDeleteAds& scheddAds, 
					 float groupQuota, const char* groupName)
{
	ClassAd		*schedd;
	MyString    scheddName;
	MyString    scheddAddr;
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
	int         scheddAdsCountOrig;
	int			totalTime;
	int			num_idle_jobs;

    int duration_phase3 = 0;
    time_t start_time_phase4 = time(NULL);

	double scheddUsed=0;
	int spin_pie=0;
	do {
		spin_pie++;

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
            filter_submitters_no_idle(scheddAds);
        }

		calculateNormalizationFactor( scheddAds, maxPrioValue, normalFactor,
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
			scheddAds,
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

        if (!ConsiderPreemption && (pieLeft <= 0)) break;

        if (1 == spin_pie) {
            // Sort the schedd list in decreasing priority order
            // This only needs to be done once: do it on the 1st spin, prior to 
            // iterating over submitter ads so they negotiate in sorted order.
            // The sort ordering function makes use of a submitter starvation
            // attribute that is computed in calculatePieLeft, above.
			// The sort order function also makes use of job priority information
			// if want_globaljobprio is true.
            time_t start_time_phase3 = time(NULL);
            dprintf(D_ALWAYS, "Phase 3:  Sorting submitter ads by priority ...\n");
            scheddAds.Sort((lessThanFunc)comparisonFunction, this);

			// Now that the submitter ad list (scheddAds) is sorted, we can
			// scan through it looking for globaljobprio submitter ads, consolidating
			// them into a minimal set of submitter ads that contain JOBPRIO_MIN and
			// JOBPRIO_MAX attributes to reflect job priority ranges.
			want_globaljobprio = consolidate_globaljobprio_submitter_ads(scheddAds);

            duration_phase3 += time(NULL) - start_time_phase3;
        }

		pieLeftOrig = pieLeft;
		scheddAdsCountOrig = scheddAds.MyLength();

		// ----- Negotiate with the schedds in the sorted list
		dprintf( D_ALWAYS, "Phase 4.%d:  Negotiating with schedds ...\n",
			spin_pie );
		dprintf (D_FULLDEBUG, "    numSlots = %d\n", numStartdAds);
		dprintf (D_FULLDEBUG, "    slotWeightTotal = %f\n", slotWeightTotal);
		dprintf (D_FULLDEBUG, "    pieLeft = %.3f\n", pieLeft);
		dprintf (D_FULLDEBUG, "    NormalFactor = %f\n", normalFactor);
		dprintf (D_FULLDEBUG, "    MaxPrioValue = %f\n", maxPrioValue);
		dprintf (D_FULLDEBUG, "    NumSubmitterAds = %d\n", scheddAds.MyLength());
		scheddAds.Open();
        // These are submitter ads, not the actual schedd daemon ads.
        // "schedd" seems to be used interchangeably with "submitter" here
		while( (schedd = scheddAds.Next()) )
		{
            if (!ignore_submitter_limit && (NULL != groupName) && (accountant.GetWeightedResourcesUsed(groupName) >= groupQuota)) {
                // If we met group quota, and if we're respecting submitter limits, halt.
                // (output message at top of outer loop above)
                break;
            }
			// get the name of the submitter and address of the schedd-daemon it came from
			if( !schedd->LookupString( ATTR_NAME, scheddName ) ||
				!schedd->LookupString( ATTR_SCHEDD_IP_ADDR, scheddAddr ) )
			{
				dprintf (D_ALWAYS,"  Error!  Could not get %s and %s from ad\n",
							ATTR_NAME, ATTR_SCHEDD_IP_ADDR);
				dprintf( D_ALWAYS, "  Ignoring this schedd and continuing\n" );
				scheddAds.Remove( schedd );
				continue;
			}

			num_idle_jobs = 0;
			schedd->LookupInteger(ATTR_IDLE_JOBS,num_idle_jobs);
			if ( num_idle_jobs < 0 ) {
				num_idle_jobs = 0;
			}

			totalTime = 0;
			schedd->LookupInteger(ATTR_TOTAL_TIME_IN_CYCLE,totalTime);
			if ( totalTime < 0 ) {
				totalTime = 0;
			}

			if (( num_idle_jobs > 0 ) && (totalTime < MaxTimePerSubmitter) ) {
				dprintf(D_ALWAYS,"  Negotiating with %s at %s\n",
					scheddName.Value(), scheddAddr.Value());
				dprintf(D_ALWAYS, "%d seconds so far\n", totalTime);
			}

			// store the verison of the schedd, so we can take advantage of
			// protocol improvements in newer versions while still being
			// backwards compatible.
			char *schedd_ver_string = NULL;
			schedd->LookupString(ATTR_VERSION, &schedd_ver_string);
			ASSERT(schedd_ver_string);
			CondorVersionInfo	scheddVersion(schedd_ver_string);
			free(schedd_ver_string);
			schedd_ver_string = NULL;

			double submitterLimit = 0.0;
            double submitterLimitUnclaimed = 0.0;
			double submitterUsage = 0.0;

			calculateSubmitterLimit(
				scheddName.Value(),
				groupName,
				groupQuota,
				groupusage,
				maxPrioValue,
				maxAbsPrioValue,
				normalFactor,
				normalAbsFactor,
				slotWeightTotal,
					/* result parameters: */
				submitterLimit,
                submitterLimitUnclaimed,
				submitterUsage,
				submitterShare,
				submitterAbsShare,
				submitterPrio,
				submitterPrioFactor);

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
				MyString starvation;
				if( submitterLimitStarved > 0 ) {
					starvation.formatstr(" (starved %f)",submitterLimitStarved);
				}
				dprintf (D_FULLDEBUG, "    submitterLimit    = %f%s\n",
					submitterLimit, starvation.Value());
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
            rejectedConcurrencyLimit = "";

			// Optimizations: 
			// If number of idle jobs = 0, don't waste time with negotiate.
			// Likewise, if limit is 0, don't waste time with negotiate EXCEPT
			// on the first spin of the pie (spin_pie==1), we must
			// still negotiate because on the first spin we tell the negotiate
			// function to ignore the submitterLimit w/ respect to jobs which
			// are strictly preferred by resource offers (via startd rank).
			if ( num_idle_jobs == 0 ) {
				dprintf(D_FULLDEBUG,
					"  Negotiating with %s skipped because no idle jobs\n",
					scheddName.Value());
				result = MM_DONE;
			} else if (totalTime >= MaxTimePerSubmitter) {
				dprintf(D_ALWAYS,
					"  Negotiation with %s skipped because of time limits:\n",
					scheddName.Value());
				dprintf(D_ALWAYS,
					"  %d seconds spent, max allowed %d\n ",
					totalTime, MaxTimePerSubmitter);
				negotiation_cycle_stats[0]->submitters_out_of_time.insert(scheddName.Value());
				result = MM_DONE;
			} else {
				if ((submitterLimit < minSlotWeight || pieLeft < minSlotWeight) && (spin_pie > 1)) {
					result = MM_RESUME;
				} else {
					int numMatched = 0;
					int remainingTimeForThisSubmitter = MaxTimePerSubmitter - totalTime;
					time_t startTime = time(NULL);
					time_t deadline = startTime + MIN(MaxTimePerSpin, remainingTimeForThisSubmitter);
                    if (negotiation_cycle_stats[0]->active_submitters.count(scheddName.Value()) <= 0) {
                        negotiation_cycle_stats[0]->num_idle_jobs += num_idle_jobs;
                    }
					negotiation_cycle_stats[0]->active_submitters.insert(scheddName.Value());
					negotiation_cycle_stats[0]->active_schedds.insert(scheddAddr.Value());
					result=negotiate(groupName, scheddName.Value(), schedd, submitterPrio,
                                  submitterLimit, submitterLimitUnclaimed,
								  startdAds, claimIds, 
								  ignore_submitter_limit,
								  deadline, numMatched, pieLeft);
					updateNegCycleEndTime(startTime, schedd);
				}
			}

			switch (result)
			{
				case MM_RESUME:
					// the schedd hit its resource limit.  must resume 
					// negotiations in next spin
					scheddUsed += accountant.GetWeightedResourcesUsed(scheddName.Value());
                    negotiation_cycle_stats[0]->submitters_share_limit.insert(scheddName.Value());
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
						dprintf(D_FULLDEBUG,"  Submitter %s got all it wants; removing it.\n", scheddName.Value());
                        scheddUsed += accountant.GetWeightedResourcesUsed(scheddName.Value());
                        dprintf( D_FULLDEBUG, " resources used by %s are %f\n",scheddName.Value(),	
                                 accountant.GetWeightedResourcesUsed(scheddName.Value()));
						scheddAds.Remove( schedd);
					}
					break;
				case MM_ERROR:
				default:
					dprintf(D_ALWAYS,"  Error: Ignoring submitter for this cycle\n" );
					sockCache->invalidateSock( scheddAddr.Value() );
	
					scheddUsed += accountant.GetWeightedResourcesUsed(scheddName.Value());
					dprintf( D_FULLDEBUG, " resources used by %s are %f\n",scheddName.Value(),	
						    accountant.GetWeightedResourcesUsed(scheddName.Value()));
					scheddAds.Remove( schedd );
					negotiation_cycle_stats[0]->submitters_failed.insert(scheddName.Value());
			}
		}
		scheddAds.Close();
		dprintf( D_FULLDEBUG, " resources used scheddUsed= %f\n",scheddUsed);

	} while ( ( pieLeft < pieLeftOrig || scheddAds.MyLength() < scheddAdsCountOrig )
			  && (scheddAds.MyLength() > 0)
			  && (startdAds.MyLength() > 0) );

	dprintf( D_ALWAYS, " negotiateWithGroup resources used scheddAds length %d \n",scheddAds.MyLength());

    negotiation_cycle_stats[0]->duration_phase3 += duration_phase3;
    negotiation_cycle_stats[0]->duration_phase4 += (time(NULL) - start_time_phase4) - duration_phase3;

	return TRUE;
}

static int
comparisonFunction (AttrList *ad1, AttrList *ad2, void *m)
{
	Matchmaker* mm = (Matchmaker*)m;

    MyString subname1;
    MyString subname2;

    // nameless submitters are filtered elsewhere
	ad1->LookupString(ATTR_NAME, subname1);
	ad2->LookupString(ATTR_NAME, subname2);
	double prio1 = mm->accountant.GetPriority(subname1);
	double prio2 = mm->accountant.GetPriority(subname2);

    // primary sort on submitter priority
    if (prio1 < prio2) return true;
    if (prio1 > prio2) return false;

    float sr1 = FLT_MAX;
    float sr2 = FLT_MAX;

    if (!ad1->LookupFloat("SubmitterStarvation", sr1)) sr1 = FLT_MAX;
    if (!ad2->LookupFloat("SubmitterStarvation", sr2)) sr2 = FLT_MAX;

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

int Matchmaker::
trimStartdAds(ClassAdListDoesNotDeleteAds &startdAds)
{
	int removed = 0;
	ClassAd *ad = NULL;
	char curState[80];
	char const *claimed_state_str = state_to_string(claimed_state);
	char const *preempting_state_str = state_to_string(preempting_state);
	ASSERT(claimed_state_str && preempting_state_str);

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
		startdAds.Open();
		while( (ad=startdAds.Next()) ) {
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
				startdAds.Remove(ad);
				removed++;
			}
		}
		startdAds.Close();

		if ( removed > 0 ) {
			dprintf(D_FULLDEBUG,
					"Trimmed out %d startd ads due to NEGOTIATOR_CONSIDER_EARLY_PREEMPTION=False\n",
					removed);
		}
		return removed;
	}

	startdAds.Open();
	while( (ad=startdAds.Next()) ) {
		if(ad->LookupString(ATTR_STATE, curState, sizeof(curState))) {
			if ( strcmp(curState,claimed_state_str)==0
			     || strcmp(curState,preempting_state_str)==0)
			{
				startdAds.Remove(ad);
				removed++;
			}
		}
	}
	startdAds.Close();

	if ( removed > 0 ) {
		dprintf(D_FULLDEBUG,
				"Trimmed out %d startd ads due to NEGOTIATOR_CONSIDER_PREEMPTION=False\n",
				removed);
	}
	return removed;
}

double Matchmaker::
sumSlotWeights(ClassAdListDoesNotDeleteAds &startdAds, double* minSlotWeight, ExprTree* constraint)
{
	ClassAd *ad = NULL;
	double sum = 0.0;

	if( minSlotWeight ) {
		*minSlotWeight = DBL_MAX;
	}

	startdAds.Open();
	while( (ad=startdAds.Next()) ) {
        // only count ads satisfying constraint, if given
        if ((NULL != constraint) && !EvalBool(ad, constraint)) {
            continue;
        }

		float slotWeight = accountant.GetSlotWeight(ad);
		sum+=slotWeight;
		if (minSlotWeight && (slotWeight < *minSlotWeight)) {
			*minSlotWeight = slotWeight;
		}
	}

	return sum;
}

bool Matchmaker::
obtainAdsFromCollector (
						ClassAdList &allAds,
						ClassAdListDoesNotDeleteAds &startdAds, 
						ClassAdListDoesNotDeleteAds &scheddAds, 
						ClaimIdHash &claimIds )
{
	CondorQuery privateQuery(STARTD_PVT_AD);
	QueryResult result;
	ClassAd *ad, *oldAd;
	MapEntry *oldAdEntry;
	int newSequence, oldSequence, reevaluate_ad;
	char    *remoteHost = NULL;
	MyString buffer;
	CollectorList* collects = daemonCore->getCollectorList();

    cp_resources = false;

    // build a query for Scheduler, Submitter and (constrained) machine ads
    //
	CondorQuery publicQuery(ANY_AD);
    publicQuery.addORConstraint("(MyType == \"Scheduler\") || (MyType == \"Submitter\")");
    if (strSlotConstraint && strSlotConstraint[0]) {
        MyString machine;
        machine.formatstr("((MyType == \"Machine\") && (%s))", strSlotConstraint);
        publicQuery.addORConstraint(machine.Value());
    } else {
        publicQuery.addORConstraint("(MyType == \"Machine\")");
    }

	// If preemption is disabled, we only need a handful of attrs from claimed ads.
	// Ask for that projection.

	if (!ConsiderPreemption) {
		const char *projectionString =
			"ifThenElse(State == \"Claimed\",\"Name State Activity StartdIpAddr AccountingGroup Owner RemoteUser Requirements SlotWeight ConcurrencyLimits\",\"\") ";
		publicQuery.setDesiredAttrsExpr(projectionString);

		dprintf(D_ALWAYS, "Not considering preemption, therefore constraining idle machines with %s\n", projectionString);
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

		// let's see if we've already got it - first lookup the sequence 
		// number from the new ad, then let's look and see if we've already
		// got something for this one.		
		if(!strcmp(GetMyTypeName(*ad),STARTD_ADTYPE)) {

			// first, let's make sure that will want to actually use this
			// ad, and if we can use it (old startds had no seq. number)
			reevaluate_ad = false; 
			ad->LookupBool(ATTR_WANT_AD_REVAULATE, reevaluate_ad);
			newSequence = -1;	
			ad->LookupInteger(ATTR_UPDATE_SEQUENCE_NUMBER, newSequence);

			if(!ad->LookupString(ATTR_NAME, &remoteHost)) {
				dprintf(D_FULLDEBUG,"Rejecting unnamed startd ad.");
				continue;
			}

#if defined(ADD_TARGET_SCOPING)
			ad->AddTargetRefs( TargetJobAttrs );
#endif

			// Next, let's transform the ad. The first thing we might
			// do is replace the Requirements attribute with whatever
			// we find in NegotiatorRequirements
			ExprTree  *negReqTree, *reqTree;
			const char *subReqs;
			char *newReqs;
			subReqs = newReqs = NULL;
			negReqTree = reqTree = NULL;
			int length;
			negReqTree = ad->LookupExpr(ATTR_NEGOTIATOR_REQUIREMENTS);
			if ( negReqTree != NULL ) {

				// Save the old requirements expression
				reqTree = ad->LookupExpr(ATTR_REQUIREMENTS);
				if( reqTree != NULL ) {
				// Now, put the old requirements back into the ad
				// (note: ExprTreeToString uses a static buffer, so do not
				//        deallocate the buffer it returns)
				subReqs = ExprTreeToString(reqTree);
				length = strlen(subReqs) + strlen(ATTR_REQUIREMENTS) + 7;
				newReqs = (char *)malloc(length+16);
				ASSERT( newReqs != NULL );
				snprintf(newReqs, length+15, "Saved%s = %s", 
							ATTR_REQUIREMENTS, subReqs); 
				ad->Insert(newReqs);
				free(newReqs);
				}
		
				// Get the requirements expression we're going to 
				// subsititute in, and convert it to a string... 
				// Sadly, this might be the best interface :(
				subReqs = ExprTreeToString(negReqTree);
				length = strlen(subReqs) + strlen(ATTR_REQUIREMENTS);
				newReqs = (char *)malloc(length+16);
				ASSERT( newReqs != NULL );

				snprintf(newReqs, length+15, "%s = %s", ATTR_REQUIREMENTS, 
							subReqs); 
				ad->Insert(newReqs);

				free(newReqs);
				
			}

			if( reevaluate_ad && newSequence != -1 ) {
				oldAd = NULL;
				oldAdEntry = NULL;

				MyString adID = MachineAdID(ad);
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
					int evalRet = EvalExprTree(expr, oldAd, ad, er);

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

			OptimizeMachineAdForMatchmaking( ad );

			startdAds.Insert(ad);
		} else if( !strcmp(GetMyTypeName(*ad),SUBMITTER_ADTYPE) ) {

            MyString subname;
            if (!ad->LookupString(ATTR_NAME, subname)) {
                dprintf(D_ALWAYS, "WARNING: ignoring submitter ad with no name\n");
                continue;
            }

            int numidle=0;
            ad->LookupInteger(ATTR_IDLE_JOBS, numidle);
            int numrunning=0;
            ad->LookupInteger(ATTR_RUNNING_JOBS, numrunning);
            int requested = numrunning + numidle;

            // This will avoid some wasted effort in negotiation looping
            if (requested <= 0) {
                dprintf(D_FULLDEBUG, "Ignoring submitter %s with no requested jobs\n", subname.Value());
                continue;
            }

    		ad->Assign(ATTR_TOTAL_TIME_IN_CYCLE, 0);

			// Now all that is left is to insert the submitter ad
			// into our list. However, if want_globaljobprio is true,
			// we insert a submitter ad for each job priority in the submitter
			// ad's job_prio_array attribute.  See gittrac #3218.
			if ( want_globaljobprio ) {
				MyString jobprioarray;
				StringList jobprios;

				if (!ad->LookupString(ATTR_JOB_PRIO_ARRAY,jobprioarray)) {
					// By design, if negotiator has want_globaljobprio and a schedd
					// does not give us a job prio array, behave as if this SubmitterAd had a
					// JobPrioArray attribute with a single value w/ the worst job priority
					jobprioarray = INT_MIN;
				}

				jobprios.initializeFromString( jobprioarray.Value() );
				jobprios.rewind();
				char *prio = NULL;
				// Insert a group of submitter ads with one ATTR_JOB_PRIO value
				// taken from the list in ATTR_JOB_PRIO_ARRAY.
				while ( (prio = jobprios.next()) != NULL ) {
					ClassAd *adCopy = new ClassAd( *ad );
					ASSERT(adCopy);
					adCopy->Assign(ATTR_JOB_PRIO,atoi(prio));
					scheddAds.Insert(adCopy);
				}
			} else {
				// want_globaljobprio is false, so just insert the submitter
				// ad into our list as-is
				scheddAds.Insert(ad);
			}
		}
        free(remoteHost);
        remoteHost = NULL;
	}
	allAds.Close();

	// In the processing of allAds above, if want_globaljobprio is true,
	// we may have created additional submitter ads and inserted them
	// into scheddAds on the fly.
	// As ads in scheddAds are not deleted when scheddAds is destroyed,
	// we must be certain to insert these ads into allAds so it gets deleted.
	// To accomplish this, we simply iterate through scheddAds and insert all
	// ads found into scheddAds. No worries about duplicates since the Insert()
	// method checks for duplicates already.
	if (want_globaljobprio) {
		scheddAds.Open();
		while( (ad=scheddAds.Next()) ) {
			allAds.Insert(ad);
		}
	}

	dprintf(D_ALWAYS,"  Getting startd private ads ...\n");
	ClassAdList startdPvtAdList;
	result = collects->query (privateQuery, startdPvtAdList);
	if( result!=Q_OK ) {
		dprintf(D_ALWAYS, "Couldn't fetch ads: %s\n", getStrQueryResult(result));
		return false;
	}

	MakeClaimIdHash(startdPvtAdList,claimIds);

	dprintf(D_ALWAYS, "Got ads: %d public and %lu private\n",
	        allAds.MyLength(),claimIds.size());

	dprintf(D_ALWAYS, "Public ads include %d submitter, %d startd\n",
		scheddAds.MyLength(), startdAds.MyLength() );

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
		MyString name;
		ad->LookupString(ATTR_NAME,name);
		dprintf(D_ALWAYS,
				"Failed to optimize machine ad %s for matchmaking: %s\n",	
			name.Value(),
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

void
Matchmaker::MakeClaimIdHash(ClassAdList &startdPvtAdList, ClaimIdHash &claimIds)
{
	ClassAd *ad;
	startdPvtAdList.Open();
	while( (ad = startdPvtAdList.Next()) ) {
		MyString name;
		MyString ip_addr;
		string claim_id;
        string claimlist;

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
        string key = name;
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
            StringList idlist(claimlist.c_str());
            idlist.rewind();
            while (char* id = idlist.next()) {
                f->second.insert(id);
            }
        } else {
            f->second.insert(claim_id);
        }
	}
	startdPvtAdList.Close();
}

int Matchmaker::
negotiate(char const* groupName, char const *scheddName, const ClassAd *scheddAd, double priority,
		   double submitterLimit, double submitterLimitUnclaimed,
		   ClassAdListDoesNotDeleteAds &startdAds, ClaimIdHash &claimIds, 
		   bool ignore_schedd_limit, time_t deadline,
		   int& numMatched, double &pieLeft)
{
	ReliSock	*sock;
	int			reply;
	int			cluster, proc;
	int			result;
	time_t		currentTime;
	time_t		beginTime = time(NULL);
	ClassAd		request;
	ClassAd     cached_resource_request;
	int			resource_request_count = 0;  // how many resources desired in resource request
	int			resource_request_offers = 0; // how many resources offered on behalf of this request
	ClassAd*    offer = NULL;
	bool		only_consider_startd_rank = false;
	bool		display_overlimit = true;
	bool		limited_by_submitterLimit = false;
    string remoteUser;
    double limitUsed = 0.0;
    double limitUsedUnclaimed = 0.0;

	numMatched = 0;

	MyString submitter_tag;
	int negotiate_cmd = NEGOTIATE; // 7.5.4+
	if( !scheddAd->LookupString(ATTR_SUBMITTER_TAG,submitter_tag) ) {
			// schedd must be older than 7.5.4
		negotiate_cmd = NEGOTIATE_WITH_SIGATTRS;
	}

	// Because of CCB, we may end up contacting a different
	// address than scheddAddr!  This is used for logging (to identify
	// the schedd) and to uniquely identify the host in the socketCache.
	// Do not attempt direct connections to this sinful string!
	MyString scheddAddr;
	if( !scheddAd->LookupString( ATTR_SCHEDD_IP_ADDR, scheddAddr ) ) {
		dprintf( D_ALWAYS, "Matchmaker::negotiate: Internal error: Missing IP address for schedd %s.  Please contact the Condor developers.\n", scheddName);
		return MM_ERROR;
	}

	// Used for log messages to identify the schedd.
	// Not for other uses, as it may change!
	MyString schedd_id;
	schedd_id.formatstr("%s (%s)", scheddName, scheddAddr.Value());
	
	// 0.  connect to the schedd --- ask the cache for a connection
	sock = sockCache->findReliSock( scheddAddr.Value() );
	if( ! sock ) {
		dprintf( D_FULLDEBUG, "Socket to %s not in cache, creating one\n", 
				 schedd_id.Value() );
			// not in the cache already, create a new connection and
			// add it to the cache.  We want to use a Daemon object to
			// send the first command so we setup a security session. 
		Daemon schedd( scheddAd, DT_SCHEDD, 0 );
		sock = schedd.reliSock( NegotiatorTimeout );
		if( ! sock ) {
			dprintf( D_ALWAYS, "    Failed to connect to %s\n", schedd_id.Value() );
			return MM_ERROR;
		}
		if( ! schedd.startCommand(negotiate_cmd, sock, NegotiatorTimeout) ) {
			dprintf( D_ALWAYS, "    Failed to send NEGOTIATE command to %s\n",
					 schedd_id.Value() );
			delete sock;
			return MM_ERROR;
		}
			// finally, add it to the cache for later...
		sockCache->addReliSock( scheddAddr.Value(), sock );
	} else { 
		dprintf( D_FULLDEBUG, "Socket to %s already in cache, reusing\n", 
				 schedd_id.Value() );
			// this address is already in our socket cache.  since
			// we've already got a TCP connection, we do *NOT* want to
			// use a Daemon::startCommand() to create a new security
			// session, we just want to encode the command
			// int on the socket...
		sock->encode();
		if( ! sock->put(negotiate_cmd) ) {
			dprintf( D_ALWAYS, "    Failed to send NEGOTIATE command to %s\n",
					 schedd_id.Value() );
			sockCache->invalidateSock( scheddAddr.Value() );
			return MM_ERROR;
		}
	}

	sock->encode();
	if( negotiate_cmd == NEGOTIATE ) {
		// Here we create a negotiation ClassAd to pass parameters to the
		// schedd's negotiation method.
		ClassAd negotiate_ad;
		int jmin, jmax;
		// Tell the schedd to limit negotiation to this owner
		negotiate_ad.Assign(ATTR_OWNER,scheddName);
		// Tell the schedd to limit negotiation to this job priority range
		if ( want_globaljobprio && scheddAd->LookupInteger("JOBPRIO_MIN",jmin) ) {
			if (!scheddAd->LookupInteger("JOBPRIO_MAX",jmax)) {
				EXCEPT("SubmitterAd with JOBPRIO_MIN attr, but no JOBPRIO_MAX\n");
			}
			negotiate_ad.Assign("JOBPRIO_MIN",jmin);
			negotiate_ad.Assign("JOBPRIO_MAX",jmax);
			dprintf (D_ALWAYS | D_MATCH,
				"    USE_GLOBAL_JOB_PRIOS limit to jobprios between %d and %d\n",
				jmin, jmax);
		}
		// Tell the schedd what sigificant attributes we found in the startd ads
		negotiate_ad.Assign(ATTR_AUTO_CLUSTER_ATTRS,job_attr_references ? job_attr_references : "");
		// Tell the schedd a submitter tag value (used for flocking levels)
		negotiate_ad.Assign(ATTR_SUBMITTER_TAG,submitter_tag.Value());
		if( !putClassAd( sock, negotiate_ad ) ) {
			dprintf (D_ALWAYS, "    Failed to send negotiation header to %s\n",
					 schedd_id.Value() );
			sockCache->invalidateSock(scheddAddr.Value());
			return MM_ERROR;
		}
	}
	else if( negotiate_cmd == NEGOTIATE_WITH_SIGATTRS ) {
			// old protocol prior to 7.5.4
		if (!sock->put(scheddName))
		{
			dprintf (D_ALWAYS, "    Failed to send scheddName to %s\n",
					 schedd_id.Value() );
			sockCache->invalidateSock(scheddAddr.Value());
			return MM_ERROR;
		}
			// send the significant attributes
		if (!sock->put(job_attr_references)) 
		{
			dprintf (D_ALWAYS, "    Failed to send significant attrs to %s\n",
					 schedd_id.Value() );
			sockCache->invalidateSock(scheddAddr.Value());
			return MM_ERROR;
		}
	}
	else {
		EXCEPT("Unexpected negotiate_cmd=%d\n",negotiate_cmd);
	}
	if (!sock->end_of_message())
	{
		dprintf (D_ALWAYS, "    Failed to send scheddName/eom to %s\n",
			schedd_id.Value() );
		sockCache->invalidateSock(scheddAddr.Value());
		return MM_ERROR;
	}

	
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
			"    Reached spin deadline for %s after %d sec... stopping\n       MAX_TIME_PER_SUBMITTER = %d sec, MAX_TIME_PER_PIESPIN = %d sec\n",
				schedd_id.Value(), (int)(currentTime - beginTime),
				MaxTimePerSubmitter, MaxTimePerSpin);
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


		// 2a.  ask for job information
		if ( resource_request_count > ++resource_request_offers ) {
			// no need to go to the schedd to ask for another resource request,
			// since we have not yet handed out the number of requested matches
			// for the request we already have cached.
			request = cached_resource_request;
		} else {
			// need to go over the wire and ask the schedd for the request
			int sleepy = param_integer("NEG_SLEEP", 0);
				//  This sleep is useful for any testing that calls for a
				//  long negotiation cycle, please do not remove
				//  it. Examples of such testing are the async negotiation
				//  protocol w/ schedds and reconfig delaying until after
				//  a negotiation cycle. -matt 21 mar 2012
			if ( sleepy ) {
				dprintf(D_ALWAYS, "begin sleep: %d seconds\n", sleepy);
				sleep(sleepy); // TODD DEBUG - allow schedd to do other things
				dprintf(D_ALWAYS, "end sleep: %d seconds\n", sleepy);
			}
			dprintf (D_FULLDEBUG, "    Sending SEND_JOB_INFO/eom\n");
			sock->encode();
			if (!sock->put(SEND_JOB_INFO) || !sock->end_of_message())
			{
				dprintf (D_ALWAYS, "    Failed to send SEND_JOB_INFO/eom\n");
				sockCache->invalidateSock(scheddAddr.Value());
				return MM_ERROR;
			}

			// 2b.  the schedd may either reply with JOB_INFO or NO_MORE_JOBS
			dprintf (D_FULLDEBUG, "    Getting reply from schedd ...\n");
			sock->decode();
			if (!sock->get (reply))
			{
				dprintf (D_ALWAYS, "    Failed to get reply from schedd\n");
				sock->end_of_message ();
				sockCache->invalidateSock(scheddAddr.Value());
				return MM_ERROR;
			}

			// 2c.  if the schedd replied with NO_MORE_JOBS, cleanup and quit
			if (reply == NO_MORE_JOBS)
			{
				dprintf (D_ALWAYS, "    Got NO_MORE_JOBS;  done negotiating\n");
				sock->end_of_message ();
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
			else
			if (reply != JOB_INFO)
			{
				// something goofy
				dprintf(D_ALWAYS,"    Got illegal command %d from schedd\n",reply);
				sock->end_of_message ();
				sockCache->invalidateSock(scheddAddr.Value());
				return MM_ERROR;
			}

			// 2d.  get the request
			dprintf (D_FULLDEBUG,"    Got JOB_INFO command; getting classad/eom\n");
			if (!getClassAd(sock, request) || !sock->end_of_message())
			{
				dprintf(D_ALWAYS, "    JOB_INFO command not followed by ad/eom\n");
				sock->end_of_message();
				sockCache->invalidateSock(scheddAddr.Value());
				return MM_ERROR;
			}
			if (!request.LookupInteger (ATTR_CLUSTER_ID, cluster) ||
				!request.LookupInteger (ATTR_PROC_ID, proc))
			{
				dprintf (D_ALWAYS, "    Could not get %s and %s from request\n",
						ATTR_CLUSTER_ID, ATTR_PROC_ID);
				sockCache->invalidateSock( scheddAddr.Value() );
				return MM_ERROR;
			}
			resource_request_offers = 0;
			resource_request_count = 0;
			if ( param_boolean("USE_RESOURCE_REQUEST_COUNTS",true) ) {
				request.LookupInteger(ATTR_RESOURCE_REQUEST_COUNT,resource_request_count);
				if (resource_request_count > 0) {
					cached_resource_request = request;
				}
			}
		}	// end of going over wire to ask schedd for request

		dprintf(D_ALWAYS, "    Request %05d.%05d:  (request count %d of %d)\n", cluster, proc,
			resource_request_offers+1,resource_request_count);
        negotiation_cycle_stats[0]->num_jobs_considered += 1;

#if defined(ADD_TARGET_SCOPING)
		request.AddTargetRefs( TargetMachineAttrs );
#endif

        // information regarding the negotiating group context:
        string negGroupName = (groupName != NULL) ? groupName : hgq_root_group->name.c_str();
        request.Assign(ATTR_SUBMITTER_NEGOTIATING_GROUP, negGroupName);
        request.Assign(ATTR_SUBMITTER_AUTOREGROUP, (autoregroup && (negGroupName == hgq_root_group->name))); 

		// insert the submitter user priority attributes into the request ad
		// first insert old-style ATTR_SUBMITTOR_PRIO
		request.Assign(ATTR_SUBMITTOR_PRIO , (float)priority );  
		// next insert new-style ATTR_SUBMITTER_USER_PRIO
		request.Assign(ATTR_SUBMITTER_USER_PRIO , (float)priority );  
		// next insert the submitter user usage attributes into the request
		request.Assign(ATTR_SUBMITTER_USER_RESOURCES_IN_USE, 
					   accountant.GetWeightedResourcesUsed ( scheddName ));
        string temp_groupName;
		float temp_groupQuota, temp_groupUsage;
		if (getGroupInfoFromUserId(scheddName, temp_groupName, temp_groupQuota, temp_groupUsage)) {
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
			offer=matchmakingAlgorithm(scheddName, scheddAddr.Value(), request,
                                             startdAds, priority,
                                             limitUsed, limitUsedUnclaimed, 
                                             submitterLimit, submitterLimitUnclaimed,
											 pieLeft,
											 only_consider_startd_rank);

			if( !offer )
			{
				int want_match_diagnostics = 0;
				request.LookupBool (ATTR_WANT_MATCH_DIAGNOSTICS,
									want_match_diagnostics);
				string diagnostic_message;
				// no match found
				dprintf(D_ALWAYS|D_MATCH, "      Rejected %d.%d %s %s: ",
						cluster, proc, scheddName, scheddAddr.Value());

				negotiation_cycle_stats[0]->rejections++;

				if( rejForSubmitterLimit ) {
                    negotiation_cycle_stats[0]->submitters_share_limit.insert(scheddName);
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
						diagnostic_message = "concurrency limit " + rejectedConcurrencyLimit + " reached";
					} else if (rejPreemptForPolicy) {
						diagnostic_message =
							"PREEMPTION_REQUIREMENTS == False";
					} else if (rejPreemptForPrio) {
						diagnostic_message = "insufficient priority";
					} else if (rejForSubmitterLimit && !ignore_schedd_limit) {
                        diagnostic_message = "submitter limit exceeded";
					} else {
						diagnostic_message = "no match found";
					}
					dprintf(D_ALWAYS|D_MATCH|D_NOHEADER, "%s\n",
							diagnostic_message.c_str());
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
						sockCache->invalidateSock(scheddAddr.Value());
						
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


				float newStartdRank;
				float oldStartdRank = 0.0;
				if(! offer->EvalFloat(ATTR_RANK, &request, newStartdRank)) {
					newStartdRank = 0.0;
				}
				offer->LookupFloat(ATTR_CURRENT_RANK, oldStartdRank);

				// got a candidate preemption --- print a helpful message
				dprintf( D_ALWAYS, "      Preempting %s (user prio=%.2f, startd rank=%.2f) on %s "
						 "for %s (user prio=%.2f, startd rank=%.2f)\n", remoteUser.c_str(),
						 remotePriority, oldStartdRank, remoteHost, scheddName,
						 priority, newStartdRank );
                free(remoteHost);
                remoteHost = NULL;
			}

			// 2e(ii).  perform the matchmaking protocol
			result = matchmakingProtocol (request, offer, claimIds, sock, 
					scheddName, scheddAddr.Value());

			// 2e(iii). if the matchmaking protocol failed, do not consider the
			//			startd again for this negotiation cycle.
			if (result == MM_BAD_MATCH)
				startdAds.Remove (offer);

			// 2e(iv).  if the matchmaking protocol failed to talk to the 
			//			schedd, invalidate the connection and return
			if (result == MM_ERROR)
			{
				sockCache->invalidateSock (scheddAddr.Value());
				return MM_ERROR;
			}
		}

		// 2f.  if MM_NO_MATCH was found for the request, get another request
		if (result == MM_NO_MATCH) 
		{
			numMatched--;		// haven't used any resources this cycle

			resource_request_count = 0;	// do not reuse any cached request

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
    		int reevaluate_ad = false;
    		offer->LookupBool(ATTR_WANT_AD_REVAULATE, reevaluate_ad);
    		if (reevaluate_ad) {
    			reeval(offer);
        		// Shuffle this resource to the end of the list.  This way, if
        		// two resources with the same RANK match, we'll hand them out
        		// in a round-robin way
        		startdAds.Remove(offer);
        		startdAds.Insert(offer);
    		} else  {
                // 2g.  Delete ad from list so that it will not be considered again in 
		        // this negotiation cycle
    			startdAds.Remove(offer);
    		}
            // traditional match cost is just slot weight expression
            match_cost = accountant.GetSlotWeight(offer);
        }
        dprintf(D_FULLDEBUG, "Match completed, match cost= %g\n", match_cost);

		limitUsed += match_cost;
        if (remoteUser == "") limitUsedUnclaimed += match_cost;
		pieLeft -= match_cost;
		negotiation_cycle_stats[0]->matches++;
	}


	// break off negotiations
	sock->encode();
	if (!sock->put (END_NEGOTIATE) || !sock->end_of_message())
	{
		dprintf (D_ALWAYS, "    Could not send END_NEGOTIATE/eom\n");
        sockCache->invalidateSock(scheddAddr.Value());
	}

	// ... and continue negotiating with others
	return MM_RESUME;
}

void Matchmaker::
updateNegCycleEndTime(time_t startTime, ClassAd *submitter) {
	MyString buffer;
	time_t endTime;
	int oldTotalTime;

	endTime = time(NULL);
	submitter->LookupInteger(ATTR_TOTAL_TIME_IN_CYCLE, oldTotalTime);
	buffer.formatstr("%s = %ld", ATTR_TOTAL_TIME_IN_CYCLE, (oldTotalTime + 
					(endTime - startTime)) );
	submitter->Insert(buffer.Value());
}

float Matchmaker::
EvalNegotiatorMatchRank(char const *expr_name,ExprTree *expr,
                        ClassAd &request,ClassAd *resource)
{
	classad::Value result;
	float rank = -(FLT_MAX);

	if(expr && EvalExprTree(expr,resource,&request,result)) {
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
SubmitterLimitPermits(ClassAd* request, ClassAd* candidate, double used, double allowed, double pieLeft) {
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
    if ((used <= 0) && (allowed > 0) && (pieLeft >= 0.99*match_cost)) {

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


/*
Warning: scheddAddr may not be the actual address we'll use to contact the
schedd, thanks to CCB.  It _is_ suitable for use as a unique identifier, for
display to the user, or for calls to sockCache->invalidateSock.
*/
ClassAd *Matchmaker::
matchmakingAlgorithm(const char *scheddName, const char *scheddAddr, ClassAd &request,
					 ClassAdListDoesNotDeleteAds &startdAds,
					 double preemptPrio,
					 double limitUsed, double limitUsedUnclaimed,
                     double submitterLimit, double submitterLimitUnclaimed,
					 double pieLeft,
					 bool only_for_startdrank)
{
		// to store values pertaining to a particular candidate offer
	ClassAd 		*candidate;
	double			candidateRankValue;
	double			candidatePreJobRankValue;
	double			candidatePostJobRankValue;
	double			candidatePreemptRankValue;
	PreemptState	candidatePreemptState;
		// to store the best candidate so far
	ClassAd 		*bestSoFar = NULL;	
	ClassAd 		*cached_bestSoFar = NULL;	
	double			bestRankValue = -(FLT_MAX);
	double			bestPreJobRankValue = -(FLT_MAX);
	double			bestPostJobRankValue = -(FLT_MAX);
	double			bestPreemptRankValue = -(FLT_MAX);
	PreemptState	bestPreemptState = (PreemptState)-1;
	bool			newBestFound;
		// to store results of evaluations
	string remoteUser;
	classad::Value	result;
	bool			val;
	float			tmp;
		// request attributes
	int				requestAutoCluster = -1;

	dprintf(D_FULLDEBUG, "matchmakingAlgorithm: limit %f used %f pieLeft %f\n", submitterLimit, limitUsed, pieLeft);

		// Check resource constraints requested by request
	rejForConcurrencyLimit = 0;
    rejectedConcurrencyLimit = "";
	MyString limits;
	if (request.LookupString(ATTR_CONCURRENCY_LIMITS, limits)) {
		limits.lower_case();
		StringList list(limits.Value());
		char *limit;
		MyString str;
		list.rewind();
		while ((limit = list.next())) {
			double increment;

			ParseConcurrencyLimit(limit, increment);

			str = limit;
			double count = accountant.GetLimit(str);

			double max = accountant.GetLimitMax(str);

			dprintf(D_FULLDEBUG,
					"Concurrency Limit: %s is %f\n",
					limit, count);

			if (count < 0) {
 				EXCEPT("ERROR: Concurrency Limit %s is %f (below 0)",
					   limit, count);
			}

			if (count + increment > max) {
				dprintf(D_FULLDEBUG,
						"Concurrency Limit %s is %f, requesting %f, "
						"but cannot exceed %f\n",
						limit, count, increment, max);

				rejForConcurrencyLimit++;
                rejectedConcurrencyLimit = limit;
				return NULL;
			}
		}
	}

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
		 strcmp(cachedName,scheddName)==0 &&
		 strcmp(cachedAddr,scheddAddr)==0 &&
		 MatchList->cache_still_valid(request,PreemptionReq,PreemptionRank,
					preemption_req_unstable,preemption_rank_unstable) )
	{
		// we can use cached information.  pop off the best
		// candidate from our sorted list.
		while( (cached_bestSoFar = MatchList->pop_candidate()) ) {
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
		dprintf(D_FULLDEBUG,"Attempting to use cached MatchList: %s (MatchList length: %d, Autocluster: %d, Schedd Name: %s, Schedd Address: %s)\n",
			cached_bestSoFar?"Succeeded.":"Failed",
			MatchList->length(),
			requestAutoCluster,
			scheddName,
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
				rejForSubmitterLimit);
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
		 startdAds.Length() > 0 )		// machines available
	{
		MatchList = new MatchListType( startdAds.Length() );
		cachedAutoCluster = requestAutoCluster;
		cachedPrio = preemptPrio;
		cachedOnlyForStartdRank = only_for_startdrank;
		cachedName = strdup(scheddName);
		cachedAddr = strdup(scheddAddr);
	}


	// initialize reasons for match failure
	rejForNetwork = 0;
	rejForNetworkShare = 0;
	rejPreemptForPrio = 0;
	rejPreemptForPolicy = 0;
	rejPreemptForRank = 0;
	rejForSubmitterLimit = 0;

	// scan the offer ads
	startdAds.Open ();
	while ((candidate = startdAds.Next ())) {

		if( IsDebugVerbose(D_MACHINE) ) {
			dprintf(D_MACHINE,"Testing whether the job matches with the following machine ad:\n");
			dPrintAd(D_MACHINE, *candidate);
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
		bool is_a_match = cp_sufficient && IsAMatch(&request, candidate);

        if (has_cp) {
            // put original values back for RequestXxx attributes
            cp_restore_requested(request, consumption);
        }

		int cluster_id=-1,proc_id=-1;
		MyString machine_name;
		if( IsDebugLevel( D_MACHINE ) ) {
			request.LookupInteger(ATTR_CLUSTER_ID,cluster_id);
			request.LookupInteger(ATTR_PROC_ID,proc_id);
			candidate->LookupString(ATTR_NAME,machine_name);
			dprintf(D_MACHINE,"Job %d.%d %s match with %s.\n",
					cluster_id,
					proc_id,
					is_a_match ? "does" : "does not",
					machine_name.Value());
		}

		if( !is_a_match ) {
				// they don't match; continue
			continue;
		}

		candidatePreemptState = NO_PREEMPTION;

		remoteUser = "";
			// If there is already a preempting user, we need to preempt that user.
			// Otherwise, we need to preempt the user who is running the job.
		if (!candidate->LookupString(ATTR_PREEMPTING_ACCOUNTING_GROUP, remoteUser)) {
			if (!candidate->LookupString(ATTR_PREEMPTING_USER, remoteUser)) {
				if (!candidate->LookupString(ATTR_ACCOUNTING_GROUP, remoteUser)) {
					candidate->LookupString(ATTR_REMOTE_USER, remoteUser);
				}
			}
		}

		// if only_for_startdrank flag is true, check if the offer strictly
		// prefers this request.  Since this is the only case we care about
		// when the only_for_startdrank flag is set, if the offer does 
		// not prefer it, just continue with the next offer ad....  we can
		// skip all the below logic about preempt for user-priority, etc.
		if ( only_for_startdrank ) {
			if ( remoteUser == "" ) {
					// offer does not have a remote user, thus we cannot eval
					// startd rank yet because it does not make sense (the
					// startd has nothing to compare against).  
					// So try the next offer...
				dprintf(D_MACHINE,
						"Ignoring %s because it is unclaimed and we are currently "
						"only considering startd rank preemption for job %d.%d.\n",
						machine_name.Value(), cluster_id, proc_id);
				continue;
			}
			if ( !(EvalExprTree(rankCondStd, candidate, &request, result) && 
				   result.IsBooleanValue(val) && val) ) {
					// offer does not strictly prefer this request.
					// try the next offer since only_for_statdrank flag is set

				dprintf(D_MACHINE,
						"Job %d.%d does not have higher startd rank than existing job on %s.\n",
						cluster_id, proc_id, machine_name.Value());
				continue;
			}
			// If we made it here, we have a candidate which strictly prefers
			// this request.  Set the candidatePreemptState properly so that
			// we consider PREEMPTION_RANK down below as we should.
			candidatePreemptState = RANK_PREEMPTION;
		}

		// if there is a remote user, consider preemption ....
		// Note: we skip this if only_for_startdrank is true since we already
		//       tested above for the only condition we care about.
		if ( (remoteUser != "") &&
			 (!only_for_startdrank) ) {
			if( EvalExprTree(rankCondStd, candidate, &request, result) && 
				result.IsBooleanValue(val) && val ) {
					// offer strictly prefers this request to the one
					// currently being serviced; preempt for rank
				candidatePreemptState = RANK_PREEMPTION;
			} else if( accountant.GetPriority(remoteUser) >= preemptPrio +
				PriorityDelta ) {
					// RemoteUser on machine has *worse* priority than request
					// so we can preempt this machine *but* we need to check
					// on two things first
				candidatePreemptState = PRIO_PREEMPTION;
					// (1) we need to make sure that PreemptionReq's hold (i.e.,
					// if the PreemptionReq expression isn't true, dont preempt)
				if (PreemptionReq && 
					!(EvalExprTree(PreemptionReq,candidate,&request,result) &&
					  result.IsBooleanValue(val) && val) ) {
					rejPreemptForPolicy++;
					dprintf(D_MACHINE,
							"PREEMPTION_REQUIREMENTS prevents job %d.%d from claiming %s.\n",
							cluster_id, proc_id, machine_name.Value());
					continue;
				}
					// (2) we need to make sure that the machine ranks the job
					// at least as well as the one it is currently running 
					// (i.e., rankCondPrioPreempt holds)
				if(!(EvalExprTree(rankCondPrioPreempt,candidate,&request,result)&&
					 result.IsBooleanValue(val) && val ) ) {
						// machine doesn't like this job as much -- find another
					rejPreemptForRank++;
					dprintf(D_MACHINE,
							"Job %d.%d has lower startd rank than existing job on %s.\n",
							cluster_id, proc_id, machine_name.Value());
					continue;
				}
			} else {
					// don't have better priority *and* offer doesn't prefer
					// request --- find another machine
				if (remoteUser != scheddName) {
						// only set rejPreemptForPrio if we aren't trying to
						// preempt one of our own jobs!
					rejPreemptForPrio++;
				}
				dprintf(D_MACHINE,
						"Job %d.%d has insufficient priority to preempt existing job on %s.\n",
						cluster_id, proc_id, machine_name.Value());
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

		candidatePreJobRankValue = EvalNegotiatorMatchRank(
		  "NEGOTIATOR_PRE_JOB_RANK",NegotiatorPreJobRank,
		  request,candidate);

		// calculate the request's rank of the offer
		if(!request.EvalFloat(ATTR_RANK,candidate,tmp)) {
			tmp = 0.0;
		}
		candidateRankValue = tmp;

		candidatePostJobRankValue = EvalNegotiatorMatchRank(
		  "NEGOTIATOR_POST_JOB_RANK",NegotiatorPostJobRank,
		  request,candidate);

		candidatePreemptRankValue = -(FLT_MAX);
		if(candidatePreemptState != NO_PREEMPTION) {
			candidatePreemptRankValue = EvalNegotiatorMatchRank(
			  "PREEMPTION_RANK",PreemptionRank,
			  request,candidate);
		}

		if ( MatchList ) {
			MatchList->add_candidate(
					candidate,
					candidateRankValue,
					candidatePreJobRankValue,
					candidatePostJobRankValue,
					candidatePreemptRankValue,
					candidatePreemptState
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
		}
	}
	startdAds.Close ();

	if ( MatchList ) {
		MatchList->set_diagnostics(rejForNetwork, rejForNetworkShare, 
		    rejForConcurrencyLimit,
			rejPreemptForPrio, rejPreemptForPolicy, rejPreemptForRank,
			rejForSubmitterLimit);
			// only bother sorting if there is more than one entry
		if ( MatchList->length() > 1 ) {
			dprintf(D_FULLDEBUG,"Start of sorting MatchList (len=%d)\n",
				MatchList->length());
			MatchList->sort();
			dprintf(D_FULLDEBUG,"Finished sorting MatchList\n");
		}
		// Pop top candidate off the list to hand out as best match
		bestSoFar = MatchList->pop_candidate();
	}

	if(!bestSoFar)
	{
	/* Insert an entry into the rejects table only if no matches were found at all */
		insert_into_rejects(scheddName,request);
	}
	// this is the best match
	return bestSoFar;
}

class NotifyStartdOfMatchHandler {
public:
	MyString m_startdName;
	MyString m_startdAddr;
	int m_timeout;
	MyString m_claim_id;
	DCStartd m_startd;
	bool m_nonblocking;

	NotifyStartdOfMatchHandler(char const *startdName,char const *startdAddr,int timeout,char const *claim_id,bool nonblocking):
		
		m_startdName(startdName),
		m_startdAddr(startdAddr),
		m_timeout(timeout),
		m_claim_id(claim_id),
		m_startd(startdAddr),
		m_nonblocking(nonblocking) {}

	static void startCommandCallback(bool success,Sock *sock,CondorError * /*errstack*/,void *misc_data)
	{
		NotifyStartdOfMatchHandler *self = (NotifyStartdOfMatchHandler *)misc_data;
		ASSERT(misc_data);

		if(!success) {
			dprintf (D_ALWAYS,"      Failed to initiate socket to send MATCH_INFO to %s\n",
					 self->m_startdName.Value());
		}
		else {
			self->WriteMatchInfo(sock);
		}
		if(sock) {
			delete sock;
		}
		delete self;
	}

	bool WriteMatchInfo(Sock *sock)
	{
		ClaimIdParser idp( m_claim_id.Value() );
		ASSERT(sock);

		// pass the startd MATCH_INFO and claim id string
		dprintf (D_FULLDEBUG, "      Sending MATCH_INFO/claim id to %s\n",
		         m_startdName.Value());
		dprintf (D_FULLDEBUG, "      (Claim ID is \"%s\" )\n",
		         idp.publicClaimId() );

		if ( !sock->put_secret (m_claim_id.Value()) ||
			 !sock->end_of_message())
		{
			dprintf (D_ALWAYS,
			        "      Could not send MATCH_INFO/claim id to %s\n",
			        m_startdName.Value() );
			dprintf (D_FULLDEBUG,
			        "      (Claim ID is \"%s\")\n",
			        idp.publicClaimId() );
			return false;
		}
		return true;
	}

	bool startCommand()
	{
		dprintf (D_FULLDEBUG, "      Connecting to startd %s at %s\n", 
					m_startdName.Value(), m_startdAddr.Value()); 

		if(!m_nonblocking) {
			Stream::stream_type st = m_startd.hasUDPCommandPort() ? Stream::safe_sock : Stream::reli_sock;
			Sock *sock =  m_startd.startCommand(MATCH_INFO,st,m_timeout);
			bool result = false;
			if(!sock) {
				dprintf (D_ALWAYS,"      Failed to initiate socket (blocking mode) to send MATCH_INFO to %s\n",
						 m_startdName.Value());
			}
			else {
				result = WriteMatchInfo(sock);
			}
			if(sock) {
				delete sock;
			}
			delete this;
			return result;
		}

		Stream::stream_type st = m_startd.hasUDPCommandPort() ? Stream::safe_sock : Stream::reli_sock;
		m_startd.startCommand_nonblocking (
			MATCH_INFO,
			st,
			m_timeout,
			NULL,
			NotifyStartdOfMatchHandler::startCommandCallback,
			this);

			// Since this is nonblocking, we cannot give any immediate
			// feedback on whether the message to the startd succeeds.
		return true;
	}
};

void Matchmaker::
insertNegotiatorMatchExprs( ClassAdListDoesNotDeleteAds &cal )
{
	ClassAd *ad;
	cal.Open();
	while( ( ad = cal.Next() ) ) {
		insertNegotiatorMatchExprs( ad );
	}
	cal.Close();
}

void Matchmaker::
insertNegotiatorMatchExprs(ClassAd *ad)
{
	ASSERT(ad);

	NegotiatorMatchExprNames.rewind();
	NegotiatorMatchExprValues.rewind();
	char const *expr_name;
	while( (expr_name=NegotiatorMatchExprNames.next()) ) {
		char const *expr_value = NegotiatorMatchExprValues.next();
		ASSERT(expr_value);

		ad->AssignExpr(expr_name,expr_value);
	}
}

/*
Warning: scheddAddr may not be the actual address we'll use to contact the
schedd, thanks to CCB.  It _is_ suitable for use as a unique identifier, for
display to the user, or for calls to sockCache->invalidateSock.
*/
MSC_DISABLE_WARNING(6262) // warning: Function uses 60K of stack
int Matchmaker::
matchmakingProtocol (ClassAd &request, ClassAd *offer, 
						ClaimIdHash &claimIds, Sock *sock,
					    const char* scheddName, const char* scheddAddr)
{
	int  cluster, proc;
	MyString startdAddr;
	string remoteUser;
	char accountingGroup[256];
	char remoteOwner[256];
    MyString startdName;
	SafeSock startdSock;
	bool send_failed;
	int want_claiming = -1;
	ExprTree *savedRequirements;
	int length;
	char *tmp;

	// these will succeed
	request.LookupInteger (ATTR_CLUSTER_ID, cluster);
	request.LookupInteger (ATTR_PROC_ID, proc);

	int offline = false;
	offer->EvalBool(ATTR_OFFLINE,NULL,offline);
	if( offline ) {
		want_claiming = 0;
		RegisterAttemptedOfflineMatch( &request, offer );
	}
	else {
			// see if offer supports claiming or not
		offer->LookupBool(ATTR_WANT_CLAIMING,want_claiming);
	}

	// if offer says nothing, see if request says something
	if ( want_claiming == -1 ) {
		request.LookupBool(ATTR_WANT_CLAIMING,want_claiming);
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

	// find the startd's claim id from the private a
	char const *claim_id = NULL;
    string claim_id_buf;
    ClaimIdHash::iterator claimset = claimIds.end();
	if (want_claiming) {
        string key = startdName.Value();
        key += startdAddr.Value();
        claimset = claimIds.find(key);
        if ((claimIds.end() == claimset) || (claimset->second.size() < 1)) {
            dprintf(D_ALWAYS,"      %s has no claim id\n", startdName.Value());
            return MM_BAD_MATCH;
        }
        claim_id_buf = *(claimset->second.begin());
        claim_id = claim_id_buf.c_str();
	} else {
		// Claiming is *not* desired
		claim_id = "null";
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
	MyString limits;
	if (request.LookupString(ATTR_CONCURRENCY_LIMITS, limits)) {
		limits.lower_case();
		offer->Assign(ATTR_MATCHED_CONCURRENCY_LIMITS, limits);
	} else {
		offer->Delete(ATTR_MATCHED_CONCURRENCY_LIMITS);
	}

    // these propagate into the slot ad in the schedd match rec, and from there eventually to the claim
    // structures in the startd:
    offer->CopyAttribute(ATTR_REMOTE_GROUP, ATTR_SUBMITTER_GROUP, &request);
    offer->CopyAttribute(ATTR_REMOTE_NEGOTIATING_GROUP, ATTR_SUBMITTER_NEGOTIATING_GROUP, &request);
    offer->CopyAttribute(ATTR_REMOTE_AUTOREGROUP, ATTR_SUBMITTER_AUTOREGROUP, &request);

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
				startdName.Value(),startdAddr.Value(),NegotiatorTimeout,claim_id,want_nonblocking_startd_contact);

		if(!h->startCommand()) {
			return MM_BAD_MATCH;
		}
	}	// end of if want_claiming

	// 3.  send the match and claim_id to the schedd
	sock->encode();
	send_failed = false;	

	dprintf(D_FULLDEBUG,
		"      Sending PERMISSION, claim id, startdAd to schedd\n");
	if (!sock->put(PERMISSION_AND_AD) ||
		!sock->put_secret(claim_id) ||
		!putClassAd(sock, *offer)	||	// send startd ad to schedd
		!sock->end_of_message())
	{
			send_failed = true;
	}

	if ( send_failed )
	{
		ClaimIdParser cidp(claim_id);
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
	if (offer->LookupString (ATTR_STARTD_IP_ADDR, startdAddr) == 0) {
		startdAddr = "<0.0.0.0:0>";
	}
	dprintf(D_ALWAYS|D_MATCH, "      Matched %d.%d %s %s preempting %s %s %s%s\n",
			cluster, proc, scheddName, scheddAddr, remoteUser.c_str(),
			startdAddr.Value(), startdName.Value(),
			offline ? " (offline)" : "");

    // At this point we're offering this match as good.
    // We don't offer a claim more than once per cycle, so remove it
    // from the set of available claims.
    if (claimset != claimIds.end()) {
        claimset->second.erase(claim_id_buf);
    }

	/* CONDORDB Insert into matches table */
	insert_into_matches(scheddName, request, *offer);

    if (cp_supports_policy(*offer)) {
        // Stash match cost here for the accountant.
        // At this point the match is fully vetted so we can also deduct
        // the resource assets.
        offer->Assign(CP_MATCH_COST, cp_deduct_assets(request, *offer));
    }

    // 4. notifiy the accountant
	dprintf(D_FULLDEBUG,"      Notifying the accountant\n");
	accountant.AddMatch(scheddName, offer);

	// done
	dprintf (D_ALWAYS, "      Successfully matched with %s%s\n",
			 startdName.Value(),
			 offline ? " (offline)" : "");
	return MM_GOOD_MATCH;
}
MSC_RESTORE_WARNING(6262) // warning: Function uses 60K of stack

void
Matchmaker::calculateSubmitterLimit(
	char const *scheddName,
	char const *groupAccountingName,
	float groupQuota,
	float groupusage,
	double maxPrioValue,
	double maxAbsPrioValue,
	double normalFactor,
	double normalAbsFactor,
	double slotWeightTotal,
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
	submitterPrio = accountant.GetPriority ( scheddName );
	submitterUsage = accountant.GetWeightedResourcesUsed( scheddName );
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
	submitterPrioFactor = accountant.GetPriorityFactor ( scheddName );
	submitterAbsShare =
		maxAbsPrioValue/(submitterPrioFactor*normalAbsFactor);
}

void
Matchmaker::calculatePieLeft(
	ClassAdListDoesNotDeleteAds &scheddAds,
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
	ClassAd *schedd;

		// Calculate sum of submitterLimits in this spin of the pie.
	pieLeft = 0;

	scheddAds.Open();
	while ((schedd = scheddAds.Next()))
	{
		double submitterShare = 0.0;
		double submitterAbsShare = 0.0;
		double submitterPrio = 0.0;
		double submitterPrioFactor = 0.0;
		MyString scheddName;
		double submitterLimit = 0.0;
        double submitterLimitUnclaimed = 0.0;
		double submitterUsage = 0.0;

		schedd->LookupString( ATTR_NAME, scheddName );

		calculateSubmitterLimit(
			scheddName.Value(),
			groupAccountingName,
			groupQuota,
			groupusage,
			maxPrioValue,
			maxAbsPrioValue,
			normalFactor,
			normalAbsFactor,
			slotWeightTotal,
				/* result parameters: */
			submitterLimit,
            submitterLimitUnclaimed,
			submitterUsage,
			submitterShare,
			submitterAbsShare,
			submitterPrio,
			submitterPrioFactor);

        schedd->Assign("SubmitterStarvation", starvation_ratio(submitterUsage, submitterUsage+submitterLimit));
			
		pieLeft += submitterLimit;
	}
	scheddAds.Close();
}

void Matchmaker::
calculateNormalizationFactor (ClassAdListDoesNotDeleteAds &scheddAds,
							  double &max, double &normalFactor,
							  double &maxAbs, double &normalAbsFactor)
{
	// find the maximum of the priority values (i.e., lowest priority)
	max = maxAbs = DBL_MIN;
	scheddAds.Open();
	while (ClassAd* ad = scheddAds.Next()) {
		// this will succeed (comes from collector)
        MyString subname;
		ad->LookupString(ATTR_NAME, subname);
		double prio = accountant.GetPriority(subname);
		if (prio > max) max = prio;
		double prioFactor = accountant.GetPriorityFactor(subname);
		if (prioFactor > maxAbs) maxAbs = prioFactor;
	}
	scheddAds.Close();

	// calculate the normalization factor, i.e., sum of the (max/scheddprio)
	// also, do not factor in ads with the same ATTR_NAME more than once -
	// ads with the same ATTR_NAME signify the same user submitting from multiple
	// machines.
    set<MyString> names;
	normalFactor = 0.0;
	normalAbsFactor = 0.0;
	scheddAds.Open();
	while (ClassAd* ad = scheddAds.Next()) {
        MyString subname;
		ad->LookupString(ATTR_NAME, subname);
        std::pair<set<MyString>::iterator, bool> r = names.insert(subname);
        // Only count each submitter once
        if (!r.second) continue;

		double prio = accountant.GetPriority(subname);
		normalFactor += max/prio;
		double prioFactor = accountant.GetPriorityFactor(subname);
		normalAbsFactor += maxAbs/prioFactor;
	}
	scheddAds.Close();
}


void Matchmaker::
addRemoteUserPrios( ClassAdListDoesNotDeleteAds &cal )
{
	ClassAd *ad;
	cal.Open();
	while( ( ad = cal.Next() ) ) {
		addRemoteUserPrios(ad);
	}
	cal.Close();
}

void Matchmaker::
addRemoteUserPrios( ClassAd	*ad )
{	
	MyString	remoteUser;
	MyString	buffer,buffer1,buffer2,buffer3;
	MyString    slot_prefix;
	MyString    expr;
	string expr_buffer;
	float	prio;
	int     total_slots, i;
	float     preemptingRank;
	float temp_groupQuota, temp_groupUsage;
    string temp_groupName;

	if ( !ConsiderPreemption ) {
			// Hueristic - no need to take the time to populate ad with 
			// accounting information if no preemption is to be considered.
		return;
	}

		// If there is a preempting user, use that for computing remote user prio.
		// Otherwise, use the current user.
	if( ad->LookupString( ATTR_PREEMPTING_ACCOUNTING_GROUP , remoteUser ) ||
		ad->LookupString( ATTR_PREEMPTING_USER , remoteUser ) ||
		ad->LookupString( ATTR_ACCOUNTING_GROUP , remoteUser ) ||
		ad->LookupString( ATTR_REMOTE_USER , remoteUser ) ) 
	{
		prio = (float) accountant.GetPriority( remoteUser.Value() );
		ad->Assign(ATTR_REMOTE_USER_PRIO, prio);
		expr.formatstr("%s(\"%s\")",RESOURCES_IN_USE_BY_USER_FN_NAME,EscapeAdStringValue(remoteUser.Value(),expr_buffer));
		ad->AssignExpr(ATTR_REMOTE_USER_RESOURCES_IN_USE,expr.Value());
		if (getGroupInfoFromUserId(remoteUser.Value(), temp_groupName, temp_groupQuota, temp_groupUsage)) {
			// this is a group, so enter group usage info
            ad->Assign(ATTR_REMOTE_GROUP, temp_groupName);
			expr.formatstr("%s(\"%s\")",RESOURCES_IN_USE_BY_USERS_GROUP_FN_NAME,EscapeAdStringValue(remoteUser.Value(),expr_buffer));
			ad->AssignExpr(ATTR_REMOTE_GROUP_RESOURCES_IN_USE,expr.Value());
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
	if (!total_slots && (param_boolean("ALLOW_VM_CRUFT", false))) {
		if (!ad->LookupInteger(ATTR_TOTAL_VIRTUAL_MACHINES, total_slots)) {
			total_slots = 0;
		}
	}
		// This won't fire if total_slots is still 0...
	for(i = 1; i <= total_slots; i++) {
		slot_prefix.formatstr("%s%d_", resource_prefix, i);
		buffer.formatstr("%s%s", slot_prefix.Value(), ATTR_PREEMPTING_ACCOUNTING_GROUP);
		buffer1.formatstr("%s%s", slot_prefix.Value(), ATTR_PREEMPTING_USER);
		buffer2.formatstr("%s%s", slot_prefix.Value(), ATTR_ACCOUNTING_GROUP);
		buffer3.formatstr("%s%s", slot_prefix.Value(), ATTR_REMOTE_USER);
			// If there is a preempting user, use that for computing remote user prio.
		if( ad->LookupString( buffer.Value() , remoteUser ) ||
			ad->LookupString( buffer1.Value() , remoteUser ) ||
			ad->LookupString( buffer2.Value() , remoteUser ) ||
			ad->LookupString( buffer3.Value() , remoteUser ) ) 
		{
				// If there is a user on that VM, stick that user's priority
				// information into the ad	
			prio = (float) accountant.GetPriority( remoteUser.Value() );
			buffer.formatstr("%s%s", slot_prefix.Value(), 
					ATTR_REMOTE_USER_PRIO);
			ad->Assign(buffer.Value(),prio);
			buffer.formatstr("%s%s", slot_prefix.Value(), 
					ATTR_REMOTE_USER_RESOURCES_IN_USE);
			expr.formatstr("%s(\"%s\")",RESOURCES_IN_USE_BY_USER_FN_NAME,EscapeAdStringValue(remoteUser.Value(),expr_buffer));
			ad->AssignExpr(buffer.Value(),expr.Value());
			if (getGroupInfoFromUserId(remoteUser.Value(), temp_groupName, temp_groupQuota, temp_groupUsage)) {
				// this is a group, so enter group usage info
				buffer.formatstr("%s%s", slot_prefix.Value(), ATTR_REMOTE_GROUP);
				ad->Assign( buffer.Value(), temp_groupName );
				buffer.formatstr("%s%s", slot_prefix.Value(), ATTR_REMOTE_GROUP_RESOURCES_IN_USE);
				expr.formatstr("%s(\"%s\")",RESOURCES_IN_USE_BY_USERS_GROUP_FN_NAME,EscapeAdStringValue(remoteUser.Value(),expr_buffer));
				ad->AssignExpr( buffer.Value(), expr.Value() );
				buffer.formatstr("%s%s", slot_prefix.Value(), ATTR_REMOTE_GROUP_QUOTA);
				ad->Assign( buffer.Value(), temp_groupQuota );
			}
		}	
	}
	free( resource_prefix );
}

void Matchmaker::
reeval(ClassAd *ad) 
{
	int cur_matches;
	MapEntry *oldAdEntry = NULL;
	char    buffer[255];
	
	cur_matches = 0;
	ad->EvalInteger("CurMatches", NULL, cur_matches);

	MyString adID = MachineAdID(ad);
	stashedAds->lookup( adID, oldAdEntry);
		
	cur_matches++;
	snprintf(buffer, 255, "CurMatches = %d", cur_matches);
	ad->Insert(buffer);
	if(oldAdEntry) {
		delete(oldAdEntry->oldAd);
		oldAdEntry->oldAd = new ClassAd(*ad);
	}
}

unsigned int Matchmaker::HashFunc(const MyString &Key) {
	return Key.Hash();
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
pop_candidate()
{
	ClassAd* candidate = NULL;

	while ( adListHead < adListLen && !candidate ) {
		candidate = AdListArray[adListHead].ad;
		adListHead++;
	}

	return candidate;
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
				!(EvalExprTree(preemption_req,next_entry->ad,&request,result) &&
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
				    int & rejForSubmitterLimit)
{
	rejForNetwork = m_rejForNetwork;
	rejForNetworkShare = m_rejForNetworkShare;
	rejForConcurrencyLimit = m_rejForConcurrencyLimit;
	rejPreemptForPrio = m_rejPreemptForPrio;
	rejPreemptForPolicy = m_rejPreemptForPolicy;
	rejPreemptForRank = m_rejPreemptForRank;
	rejForSubmitterLimit = m_rejForSubmitterLimit;
}

void Matchmaker::MatchListType::
set_diagnostics(int rejForNetwork,
					int rejForNetworkShare,
					int rejForConcurrencyLimit,
					int rejPreemptForPrio,
					int rejPreemptForPolicy,
				    int rejPreemptForRank,
				    int rejForSubmitterLimit)
{
	m_rejForNetwork = rejForNetwork;
	m_rejForNetworkShare = rejForNetworkShare;
	m_rejForConcurrencyLimit = rejForConcurrencyLimit;
	m_rejPreemptForPrio = rejPreemptForPrio;
	m_rejPreemptForPolicy = rejPreemptForPolicy;
	m_rejPreemptForRank = rejPreemptForRank;
	m_rejForSubmitterLimit = rejForSubmitterLimit;
}

void Matchmaker::MatchListType::
add_candidate(ClassAd * candidate,
					double candidateRankValue,
					double candidatePreJobRankValue,
					double candidatePostJobRankValue,
					double candidatePreemptRankValue,
					PreemptState candidatePreemptState)
{
	ASSERT(AdListArray);
	ASSERT(adListLen < adListMaxLen);  // don't write off end of array!

	AdListArray[adListLen].ad = candidate;
	AdListArray[adListLen].RankValue = candidateRankValue;
	AdListArray[adListLen].PreJobRankValue = candidatePreJobRankValue;
	AdListArray[adListLen].PostJobRankValue = candidatePostJobRankValue;
	AdListArray[adListLen].PreemptRankValue = candidatePreemptRankValue;
	AdListArray[adListLen].PreemptStateValue = candidatePreemptState;

    // This hack allows me to avoid mucking with the pseudo-que-like semantics of MatchListType, 
    // which ought to be replaced with something cleaner like std::deque<AdListEntry>
    if (NULL != AdListArray[adListLen].ad) {
        AdListArray[adListLen].ad->Assign(ATTR_PREEMPT_STATE_, int(candidatePreemptState));
    }

	adListLen++;
}


void Matchmaker::DeleteMatchList()
{
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
}

int Matchmaker::MatchListType::
sort_compare(const void* elem1, const void* elem2)
{
	const AdListEntry* Elem1 = (const AdListEntry*) elem1;
	const AdListEntry* Elem2 = (const AdListEntry*) elem2;

	const double candidateRankValue = Elem1->RankValue;
	const double candidatePreJobRankValue = Elem1->PreJobRankValue;
	const double candidatePostJobRankValue = Elem1->PostJobRankValue;
	const double candidatePreemptRankValue = Elem1->PreemptRankValue;
	const PreemptState candidatePreemptState = Elem1->PreemptStateValue;

	const double bestRankValue = Elem2->RankValue;
	const double bestPreJobRankValue = Elem2->PreJobRankValue;
	const double bestPostJobRankValue = Elem2->PostJobRankValue;
	const double bestPreemptRankValue = Elem2->PreemptRankValue;
	const PreemptState bestPreemptState = Elem2->PreemptStateValue;

	if ( candidateRankValue == bestRankValue &&
		 candidatePreJobRankValue == bestPreJobRankValue &&
		 candidatePostJobRankValue == bestPostJobRankValue &&
		 candidatePreemptRankValue == bestPreemptRankValue &&
		 candidatePreemptState == bestPreemptState )
	{
		return 0;
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
		// candidate is better: candidate is elem1, and qsort man page
		// says return < 0 is elem1 is less than elem2
		return -1;
	} else {
		return 1;
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
	qsort(AdListArray,adListLen,sizeof(AdListEntry),sort_compare);

	already_sorted = true;
}


void Matchmaker::
init_public_ad()
{
	MyString line;

	if( publicAd ) delete( publicAd );
	publicAd = new ClassAd();

	SetMyTypeName(*publicAd, NEGOTIATOR_ADTYPE);
	SetTargetTypeName(*publicAd, "");

	if( !NegotiatorName ) {
		char* defaultName = NULL;
		defaultName = default_daemon_name();
		if( ! defaultName ) {
			EXCEPT( "default_daemon_name() returned NULL" );
		}
		NegotiatorName = strdup( defaultName );
		delete [] defaultName;
	}
	publicAd->Assign(ATTR_NAME, NegotiatorName );

	publicAd->Assign(ATTR_NEGOTIATOR_IP_ADDR,daemonCore->InfoCommandSinfulString());

#if !defined(WIN32)
	line.formatstr("%s = %d", ATTR_REAL_UID, (int)getuid() );
	publicAd->Insert(line.Value());
#endif

        // Publish all DaemonCore-specific attributes, which also handles
        // NEGOTIATOR_ATTRS for us.
    daemonCore->publish(publicAd);
}

void
Matchmaker::updateCollector() {
	dprintf(D_FULLDEBUG, "enter Matchmaker::updateCollector\n");

		// in case our address changes, re-initialize public ad every time
	init_public_ad();

	if( publicAd ) {
		publishNegotiationCycleStats( publicAd );

        daemonCore->dc_stats.Publish(*publicAd);
		daemonCore->monitor_data.ExportData(publicAd);

		// log classad into sql log so that it can be updated to DB
		FILESQL::daemonAdInsert(publicAd, "NegotiatorAd", FILEObj, prevLHF);	

#if defined(WANT_CONTRIB) && defined(WITH_MANAGEMENT)
#if defined(HAVE_DLOPEN)
		NegotiatorPluginManager::Update(*publicAd);
#endif
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
	MyString line;

	if( !NegotiatorName ) {
		return;
	}

		// Set the correct types
	SetMyTypeName( cmd_ad, QUERY_ADTYPE );
	SetTargetTypeName( cmd_ad, NEGOTIATOR_ADTYPE );

	line.formatstr( "%s = TARGET.%s == \"%s\"", ATTR_REQUIREMENTS,
				  ATTR_NAME,
				  NegotiatorName );
	cmd_ad.Insert( line.Value() );
	cmd_ad.Assign( ATTR_NAME, NegotiatorName );

	daemonCore->sendUpdates( INVALIDATE_NEGOTIATOR_ADS, &cmd_ad, NULL, false );
}

/* CONDORDB functions */
void Matchmaker::insert_into_rejects(char const *userName, ClassAd& job)
{
	int cluster, proc;
//	char startdname[80];
	char globaljobid[200];
	char scheddName[200];
	ClassAd tmpCl;
	ClassAd *tmpClP = &tmpCl;
	char tmp[512];

	time_t clock;

	(void)time(  (time_t *)&clock );

	job.LookupInteger (ATTR_CLUSTER_ID, cluster);
	job.LookupInteger (ATTR_PROC_ID, proc);
	job.LookupString( ATTR_GLOBAL_JOB_ID, globaljobid, sizeof(globaljobid)); 
	get_scheddname_from_gjid(globaljobid,scheddName);
//	machine.LookupString(ATTR_NAME, startdname);

	snprintf(tmp, 512, "reject_time = %d", (int)clock);
	tmpClP->Insert(tmp);
	
	tmpClP->Assign("username",userName);
		
	snprintf(tmp, 512, "scheddname = \"%s\"", scheddName);
	tmpClP->Insert(tmp);
	
	snprintf(tmp, 512, "cluster_id = %d", cluster);
	tmpClP->Insert(tmp);

	snprintf(tmp, 512, "proc_id = %d", proc);
	tmpClP->Insert(tmp);

	snprintf(tmp, 512, "GlobalJobId = \"%s\"", globaljobid);
	tmpClP->Insert(tmp);
	
	FILEObj->file_newEvent("Rejects", tmpClP);
}
void Matchmaker::insert_into_matches(char const * userName,ClassAd& request, ClassAd& offer)
{
	char startdname[80],remote_user[80];
	char globaljobid[200];
	float remote_prio;
	int cluster, proc;
	char scheddName[200];
	ClassAd tmpCl;
	ClassAd *tmpClP = &tmpCl;

	time_t clock;
	char tmp[512];

	(void)time(  (time_t *)&clock );

	request.LookupInteger (ATTR_CLUSTER_ID, cluster);
	request.LookupInteger (ATTR_PROC_ID, proc);
	request.LookupString( ATTR_GLOBAL_JOB_ID, globaljobid, sizeof(globaljobid)); 
	get_scheddname_from_gjid(globaljobid,scheddName);
	offer.LookupString( ATTR_NAME, startdname, sizeof(startdname)); 

	snprintf(tmp, 512, "match_time = %d", (int) clock);
	tmpClP->Insert(tmp);
	
	tmpClP->Assign("username",userName);
		
	snprintf(tmp, 512, "scheddname = \"%s\"", scheddName);
	tmpClP->Insert(tmp);
	
	snprintf(tmp, 512, "cluster_id = %d", cluster);
	tmpClP->Insert(tmp);

	snprintf(tmp, 512, "proc_id = %d", proc);
	tmpClP->Insert(tmp);

	snprintf(tmp, 512, "GlobalJobId = \"%s\"", globaljobid);
	tmpClP->Insert(tmp);

	snprintf(tmp, 512, "machine_id = \"%s\"", startdname);
	tmpClP->Insert(tmp);

	if(offer.LookupString( ATTR_REMOTE_USER, remote_user, sizeof(remote_user)) != 0)
	{
		remote_prio = (float) accountant.GetPriority(remote_user);

		snprintf(tmp, 512, "remote_user = \"%s\"", remote_user);
		tmpClP->Insert(tmp);

		snprintf(tmp, 512, "remote_priority = %f", remote_prio);
		tmpClP->Insert(tmp);
	}
	
	FILEObj->file_newEvent("Matches", tmpClP);
}
/* This extracts the machine name from the global job ID [user@]machine.name#timestamp#cluster.proc*/
static int get_scheddname_from_gjid(const char * globaljobid, char * scheddname )
{
	int i;

	scheddname[0] = '\0';

	for (i=0;
         globaljobid[i]!='\0' && globaljobid[i]!='#';i++)
		scheddname[i]=globaljobid[i];

	if(globaljobid[i] == '\0') 
	{
		scheddname[0] = '\0';
		return -1; /* Parse error, shouldn't happen */
	}
	else if(globaljobid[i]=='#')
	{
		scheddname[i]='\0';	
		return 1;
	}

	return -1;
}

void Matchmaker::RegisterAttemptedOfflineMatch( ClassAd *job_ad, ClassAd *startd_ad )
{
	if( IsFulldebug(D_FULLDEBUG) ) {
		MyString name;
		startd_ad->LookupString(ATTR_NAME,name);
		MyString owner;
		job_ad->LookupString(ATTR_OWNER,owner);
		dprintf(D_FULLDEBUG,"Registering attempt to match offline machine %s by %s.\n",name.Value(),owner.Value());
	}

	ClassAd update_ad;

		// Copy some stuff from the startd ad into the update ad so
		// the collector can identify what ad to merge our update
		// into.
	update_ad.CopyAttribute(ATTR_NAME,ATTR_NAME,startd_ad);
	update_ad.CopyAttribute(ATTR_STARTD_IP_ADDR,ATTR_STARTD_IP_ADDR,startd_ad);

	time_t now = time(NULL);
	update_ad.Assign(ATTR_MACHINE_LAST_MATCH_TIME,(int)now);

	classy_counted_ptr<ClassAdMsg> msg = new ClassAdMsg(MERGE_STARTD_AD,update_ad);
	classy_counted_ptr<DCCollector> collector = new DCCollector();

	if( !collector->useTCPForUpdates() ) {
		msg->setStreamType( Stream::safe_sock );
	}

	collector->sendMsg( msg.get() );

		// also insert slotX_LastMatchTime into the slot1 ad so that
		// the match info about all slots is available in one place
	MyString name;
	MyString slot1_name;
	int slot_id = -1;
	startd_ad->LookupString(ATTR_NAME,name);
	startd_ad->LookupInteger(ATTR_SLOT_ID,slot_id);

		// Undocumented feature in case we ever need it:
		// If OfflinePrimarySlotName is defined, it specifies which
		// slot should collect all the slotX_LastMatchTime attributes.
	if( !startd_ad->LookupString("OfflinePrimarySlotName",slot1_name) ) {
			// no primary slot name specified, so use slot1

		const char *at = strchr(name.Value(),'@');
		if( at ) {
				// in case the slot prefix is something other than "slot"
				// figure out the prefix
			int prefix_len = strcspn(name.Value(),"0123456789");
			if( prefix_len < at - name.Value() ) {
				slot1_name.formatstr("%.*s1%s",prefix_len,name.Value(),at);
			}
		}
	}

	if( !slot1_name.IsEmpty() && slot_id >= 0 ) {
		ClassAd slot1_update_ad;

		slot1_update_ad.Assign(ATTR_NAME,slot1_name);
		slot1_update_ad.CopyAttribute(ATTR_STARTD_IP_ADDR,ATTR_STARTD_IP_ADDR,startd_ad);
		MyString slotX_last_match_time;
		slotX_last_match_time.formatstr("slot%d_%s",slot_id,ATTR_MACHINE_LAST_MATCH_TIME);
		slot1_update_ad.Assign(slotX_last_match_time.Value(),(int)now);

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
	MyString attrn;
	attrn.formatstr("%s%d",attr,n);
	ad->Delete( attrn.Value() );
}

static void
SetAttrN( ClassAd *ad, char const *attr, int n, int value )
{
	MyString attrn;
	attrn.formatstr("%s%d",attr,n);
	ad->Assign(attrn.Value(),value);
}

static void
SetAttrN( ClassAd *ad, char const *attr, int n, double value )
{
	MyString attrn;
	attrn.formatstr("%s%d",attr,n);
	ad->Assign(attrn.Value(),value);
}

static void
SetAttrN( ClassAd *ad, char const *attr, int n, std::set<std::string> &string_list )
{
	MyString attrn;
	attrn.formatstr("%s%d",attr,n);

	MyString value;
	std::set<std::string>::iterator it;
	for(it = string_list.begin();
		it != string_list.end();
		it++)
	{
		if( !value.IsEmpty() ) {
			value += ", ";
		}
		value += it->c_str();
	}

	ad->Assign(attrn.Value(),value.Value());
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

        int period = 0;
        if (((1+i) < num_negotiation_cycle_stats) && (negotiation_cycle_stats[1+i] != NULL))
            period = s->end_time - negotiation_cycle_stats[1+i]->end_time;

		SetAttrN( ad, ATTR_LAST_NEGOTIATION_CYCLE_TIME, i, (int)s->start_time);
		SetAttrN( ad, ATTR_LAST_NEGOTIATION_CYCLE_END, i, (int)s->end_time);
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
		SetAttrN( ad, ATTR_LAST_NEGOTIATION_CYCLE_SUBMITTERS_FAILED, i, s->submitters_failed);
		SetAttrN( ad, ATTR_LAST_NEGOTIATION_CYCLE_SUBMITTERS_OUT_OF_TIME, i, s->submitters_out_of_time);
        SetAttrN( ad, ATTR_LAST_NEGOTIATION_CYCLE_SUBMITTERS_SHARE_LIMIT, i, s->submitters_share_limit);
	}
}

double 
Matchmaker::calculate_subtree_usage(GroupEntry *group) {
	double subtree_usage = 0.0;

    for (vector<GroupEntry*>::iterator i(group->children.begin());  i != group->children.end();  i++) {
		subtree_usage += calculate_subtree_usage(*i);
	}
	subtree_usage += accountant.GetWeightedResourcesUsed(group->name.c_str());

	group->subtree_usage = subtree_usage;;
	dprintf(D_ALWAYS, "subtree_usage at %s is %g\n", group->name.c_str(), subtree_usage);
	return subtree_usage;
}

GCC_DIAG_ON(float-equal)

