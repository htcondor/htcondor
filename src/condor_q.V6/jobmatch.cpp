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
#include "condor_config.h"
#include "condor_accountant.h"
#include "condor_classad.h"
#include "condor_debug.h"
#include "condor_query.h"
#include "condor_q.h"
#include "condor_io.h"
#include "condor_string.h"
#include "condor_attributes.h"
#include "match_prefix.h"
#include "queue_internal.h"
#if 0
#include "ad_printmask.h"
#include "internet.h"
#include "sig_install.h"
#include "format_time.h"
#endif
#include "daemon.h"
#include "dc_collector.h"
#if 0
#include "basename.h"
#include "metric_units.h"
#include "globus_utils.h"
#include "error_utils.h"
#include "print_wrapped_text.h"
#include "condor_distribution.h"
#include "string_list.h"
#include "condor_version.h"
#include "subsystem_info.h"
#include "condor_open.h"
#include "condor_sockaddr.h"
#include "condor_id.h"
#include "userlog_to_classads.h"
#include "ipv6_hostname.h"
#include <map>
#include <vector>
//#include "pool_allocator.h"
#include "expr_analyze.h"
#include "classad/classadCache.h" // for CachedExprEnvelope stats
#include "classad_helpers.h"
#include "console-utils.h"
#endif
#include "classad_helpers.h"
#include "../condor_procapi/procapi.h" // for getting cpu time & process memory

static	const char	*fixSubmittorName( const char*, int );
static	ExprTree	*stdRankCondition;
static	ExprTree	*preemptRankCondition;
static	ExprTree	*preemptPrioCondition;
static	ExprTree	*preemptionReq;

std::vector<PrioEntry> prioTable;

#ifdef INCLUDE_ANALYSIS_SUGGESTIONS
const int SHORT_BUFFER_SIZE = 8192;
#endif
const int LONG_BUFFER_SIZE = 16384;	
char return_buff[LONG_BUFFER_SIZE * 100];


// Sort Machine ads by Machine name first, then by slotid, then then by slot name
int SlotSort(ClassAd *ad1, ClassAd *ad2, void *  /*data*/)
{
	std::string name1, name2;
	if ( ! ad1->LookupString(ATTR_MACHINE, name1))
		return 1;
	if ( ! ad2->LookupString(ATTR_MACHINE, name2))
		return 0;
	
	// opportunisticly capture the longest slot machine name.
	longest_slot_machine_name = MAX(longest_slot_machine_name, (int)name1.size());
	longest_slot_machine_name = MAX(longest_slot_machine_name, (int)name2.size());

	int cmp = name1.compare(name2);
	if (cmp < 0) return 1;
	if (cmp > 0) return 0;

	int slot1=0, slot2=0;
	ad1->LookupInteger(ATTR_SLOT_ID, slot1);
	ad2->LookupInteger(ATTR_SLOT_ID, slot2);
	if (slot1 < slot2) return 1;
	if (slot1 > slot2) return 0;

	// PRAGMA_REMIND("TJ: revisit this code to compare dynamic slot id once that exists");
	// for now, the only way to get the dynamic slot id is to use the slot name.
	name1.clear(); name2.clear();
	if ( ! ad1->LookupString(ATTR_NAME, name1))
		return 1;
	if ( ! ad2->LookupString(ATTR_NAME, name2))
		return 0;
	// opportunisticly capture the longest slot name.
	longest_slot_name = MAX(longest_slot_name, (int)name1.size());
	longest_slot_name = MAX(longest_slot_name, (int)name2.size());
	cmp = name1.compare(name2);
	if (cmp < 0) return 1;
	return 0;
}

static int slot_sequence_id = 0;

// callback function for adding a slot to an IdToClassaAdMap.
static bool AddSlotToClassAdCollection(void * pv, ClassAd* ad) {
	KeyToClassaAdMap * pmap = (KeyToClassaAdMap*)pv;

	std::string key;
	ad->LookupString(ATTR_MACHINE,key);
	longest_slot_machine_name = MAX(longest_slot_machine_name, (int)key.size());

	int slot_id = 0;
	ad->LookupInteger(ATTR_SLOT_ID, slot_id);
	formatstr_cat(key, "/%03d", slot_id);
	std::string name;
	if (ad->LookupString(ATTR_NAME, name)) {
		longest_slot_name = MAX(longest_slot_name, (int)name.size());
		key += "/";
		key += name;
	}
	++slot_sequence_id;
	formatstr_cat(key, "/%010d", slot_sequence_id);

	auto pp = pmap->insert(std::pair<std::string, UniqueClassAdPtr>(key,UniqueClassAdPtr()));
	if ( ! pp.second) {
		fprintf( stderr, "Error: Two results with the same key.\n" );
		// return true to indicate that the caller still owns the ad.
		return true;
	} else {
		// give the ad pointer to the map, it will now be responsible for freeing it.
		pp.first->second.reset(ad);
	}

	return false; // return false to indicate we took ownership of the ad.
}


static bool is_slot_name(const char * psz)
{
	// if string contains only legal characters for a slotname, then assume it's a slotname.
	// legal characters are a-zA-Z@_\.\-
	while (*psz) {
		if (*psz != '-' && *psz != '.' && *psz != '@' && *psz != '_' && ! isalnum(*psz))
			return false;
		++psz;
	}
	return true;
}

// these are used for profing...
static double tmBefore = 0.0;
static size_t cbBefore = 0;

void cleanupAnalysis()
{
	startdAds.clear();
}

int setupAnalysis(
	CollectorList * Collectors,
	const char *machineads_file, ClassAdFileParseType::ParseType machineads_file_format,
	const char *user_slot_constraint)
{
	CondorQuery	query(STARTD_AD);
	int			rval;
	char		buffer[64];
	char		*preq;
	
	if (verbose || dash_profile) {
		fprintf(stderr, "Fetching Machine ads...");
	}

	tmBefore = 0.0;
	cbBefore = 0;
	if (dash_profile) { cbBefore = ProcAPI::getBasicUsage(getpid(), &tmBefore); }

	// if there is a slot constraint, that's allowed to be a simple machine/slot name
	// in which case we build up the actual constraint expression automatically.
	std::string mconst; // holds the final machine/slot constraint expression 
	if (user_slot_constraint) {
		mconst = user_slot_constraint;
		if (is_slot_name(user_slot_constraint)) {
			formatstr(mconst, "(" ATTR_NAME "==\"%s\") || (" ATTR_MACHINE "==\"%s\")", user_slot_constraint, user_slot_constraint);
			single_machine = true;
			single_machine_label = user_slot_constraint;
		}
	}

	// fetch startd ads
	if (machineads_file != NULL) {
		ConstraintHolder constr(mconst.empty() ? NULL : strdup(mconst.c_str()));
		CondorClassAdFileParseHelper parse_helper("\n", machineads_file_format);
		if ( ! iter_ads_from_file(machineads_file, AddSlotToClassAdCollection, &startdAds, parse_helper, constr.Expr())) {
			exit (1);
		}
	} else {
		if (user_slot_constraint) {
			if (single_machine) {
				// if the constraint is really a machine/slot name, then we want to or it in (in case someday we support multiple machines)
				query.addORConstraint (mconst.c_str());
			} else {
				query.addANDConstraint (mconst.c_str());
			}
		}
		rval = Collectors->query (query, AddSlotToClassAdCollection, &startdAds);
		if( rval != Q_OK ) {
			fprintf( stderr , "Error:  Could not fetch startd ads\n" );
			exit( 1 );
		}
	}

	if (dash_profile) {
		profile_print(cbBefore, tmBefore, startdAds.size());
	} else if (verbose) {
		fprintf(stderr, " %d ads.\n", (int)startdAds.size());
	}


	// setup condition expressions
    snprintf( buffer, sizeof(buffer), "MY.%s > MY.%s", ATTR_RANK, ATTR_CURRENT_RANK );
    ParseClassAdRvalExpr( buffer, stdRankCondition );

    snprintf( buffer, sizeof(buffer), "MY.%s >= MY.%s", ATTR_RANK, ATTR_CURRENT_RANK );
    ParseClassAdRvalExpr( buffer, preemptRankCondition );

	snprintf( buffer, sizeof(buffer), "MY.%s > TARGET.%s + %f", ATTR_REMOTE_USER_PRIO,
			ATTR_SUBMITTOR_PRIO, PriorityDelta );
	ParseClassAdRvalExpr( buffer, preemptPrioCondition ) ;

	// setup preemption requirements expression
	if( !( preq = param( "PREEMPTION_REQUIREMENTS" ) ) ) {
		fprintf( stderr, "\nWarning:  No PREEMPTION_REQUIREMENTS expression in"
					" config file --- assuming FALSE\n\n" );
		ParseClassAdRvalExpr( "FALSE", preemptionReq );
	} else {
		if( ParseClassAdRvalExpr( preq , preemptionReq ) ) {
			fprintf( stderr, "\nError:  Failed parse of "
				"PREEMPTION_REQUIREMENTS expression: \n\t%s\n", preq );
			exit( 1 );
		}
		free( preq );
	}

	return startdAds.size();
}

void setupUserpriosForAnalysis(DCCollector* pool, const char *userprios_file)
{
	std::string		remoteUser;

	// fetch user priorities, and propagate the user priority values into the machine ad's
	// we fetch user priorities either directly from the negotiator, or from a file
	// treat a userprios_file of "" as a signal to ignore user priority.
	//
	int cPrios = 0;
	if (verbose || dash_profile) {
		fprintf(stderr, "Fetching user priorities...");
	}

	if (userprios_file != NULL) {
		cPrios = read_userprio_file(userprios_file, prioTable);
		if (cPrios < 0) {
			//PRAGMA_REMIND("TJ: don't exit here, just analyze without userprio information")
			exit (1);
		}
	} else {
		// fetch submittor prios
		cPrios = fetchSubmittorPriosFromNegotiator(pool, prioTable);
		if (cPrios < 0) {
			//PRAGMA_REMIND("TJ: don't exit here, just analyze without userprio information")
			exit(1);
		}
	}

	if (dash_profile) {
		profile_print(cbBefore, tmBefore, cPrios);
	}
	else if (verbose) {
		fprintf(stderr, " %d submitters\n", cPrios);
	}
	/* TJ: do we really want to do this??
	if (cPrios <= 0) {
		printf( "Warning:  Found no submitters\n" );
	}
	*/


	// populate startd ads with remote user prios
	for (auto it = startdAds.begin(); it != startdAds.end(); ++it) {
		ClassAd *ad = it->second.get();
		if (ad->LookupString( ATTR_REMOTE_USER , remoteUser)) {
			int index;
			if ((index = findSubmittor(remoteUser)) != -1) {
				ad->Assign(ATTR_REMOTE_USER_PRIO, prioTable[index].prio);
			}
		}
	}
}


int fetchSubmittorPriosFromNegotiator(DCCollector* pool, std::vector<PrioEntry> & prios)
{
	ClassAd	al;
	char  	attrName[32], attrPrio[32];
  	double 	priority;

	Daemon	negotiator( DT_NEGOTIATOR, NULL, pool ? pool->addr() : NULL );

	// connect to negotiator
	Sock* sock;

	if (!(sock = negotiator.startCommand( GET_PRIORITY, Stream::reli_sock, 0))) {
		fprintf( stderr, "Error: Could not connect to negotiator (%s)\n",
				 negotiator.fullHostname() );
		return -1;
	}

	sock->end_of_message();
	sock->decode();
	if( !getClassAdNoTypes(sock, al) || !sock->end_of_message() ) {
		fprintf( stderr, 
				 "Error:  Could not get priorities from negotiator (%s)\n",
				 negotiator.fullHostname() );
		return -1;
	}
	sock->close();
	delete sock;


	int cPrios = 0;
	std::string name;
	while( true ) {
		snprintf( attrName, sizeof(attrName), "Name%d", cPrios+1 );
		snprintf( attrPrio, sizeof(attrPrio), "Priority%d", cPrios+1 );

		if( !al.LookupString( attrName, name) || 
			!al.LookupFloat( attrPrio, priority))
			break;

		prios.emplace_back(PrioEntry{name,static_cast<float>(priority)});
		++cPrios;
	}

	return cPrios;
}

// parse lines of the form "attrNNNN = value" and return attr, NNNN and value as separate fields.
static int parse_userprio_line(const char * line, std::string & attr, std::string & value)
{
	int id = 0;

	// skip over the attr part
	const char * p = line;
	while (*p && isspace(*p)) ++p;

	// parse prefixNNN and set id=NNN and attr=prefix
	const char * pattr = p;
	while (*p) {
		if (isdigit(*p)) {
			attr.assign(pattr, p-pattr);
			id = atoi(p);
			while (isdigit(*p)) ++p;
			break;
		} else if  (isspace(*p)) {
			break;
		}
		++p;
	}
	if (id <= 0)
		return -1;

	// parse = with or without whitespace.
	while (isspace(*p)) ++p;
	if (*p != '=')
		return -1;
	++p;
	while (isspace(*p)) ++p;

	// parse value, remove "" from around strings 
	if (*p == '"')
		value.assign(p+1,strlen(p)-2);
	else
		value = p;
	return id;
}

int read_userprio_file(const char *filename, std::vector<PrioEntry> & prios)
{
	int cPrios = 0;

	FILE *file = safe_fopen_wrapper_follow(filename, "r");
	if (file == NULL) {
		fprintf(stderr, "Can't open file of user priorities: %s\n", filename);
		return -1;
	} else {
		int lineno = 0;
		while (const char * line = getline_trim(file, lineno)) {
			std::string attr, value;
			int id = parse_userprio_line(line, attr, value);
			if (id <= 0) {
				// there are valid attributes that we want to ignore that return -1 from
				// this call, so just skip them
				continue;
			}

			if (attr == "Priority") {
				float priority = atof(value.c_str());
				if (id >= (int) prios.size()) {
					prios.resize(1 + id * 2);
				}
				prios[id].prio = priority;
				cPrios = MAX(cPrios, id);
			} else if (attr == "Name") {
				if (id >= (int) prios.size()) {
					prios.resize(1 + id * 2);
				}
				prios[id].name = value;
				cPrios = MAX(cPrios, id);
			}
		}
		fclose(file);
	}
	return cPrios;
}

void printJobRunAnalysis(ClassAd *job, DaemonAllowLocateFull *schedd, int details, bool withUserprio, int PslotMode /*=2*/)
{
	bool always_analyze_req = (details & detail_always_analyze_req) != 0;

	int mode = anaMatchModePslot;
	if (always_analyze_req) {
		mode = PslotMode ? PslotMode : anaMatchModeAlways;
	}

	anaPrio prio;
	anaCounters ac;
	anaMachines mach;
	std::string job_status;

	ac.clear();
	mach.limit = 100; // maximum length (in chars) of each machine list

	bool do_analyze_req = doJobRunAnalysis(
			job, schedd, job_status,
			mode, ac,
			withUserprio ? &prio : NULL,
			verbose ? &mach : NULL);

	std::string buffer;
	if (do_analyze_req || always_analyze_req) {
		fputs(doJobMatchAnalysisToBuffer(buffer, job, details), stdout);
		fputs("\n",stdout);
		buffer.clear();
	}

	appendJobRunAnalysisToBuffer(buffer, job, job_status);
	if (ac.totalMachines > 0) {
		appendJobRunAnalysisToBuffer(buffer, job, ac,
			withUserprio ? &prio : NULL,
			verbose ? &mach : NULL);
	}
	buffer += "\n";
	fputs(buffer.c_str(), stdout);
}

#if 1
void anaMachines::append_to_fail_list(entry ety, const char * machine) {
	std::string &list = alist[ety];
	if ( ! limit || list.size() < limit) {
		if (list.size() > 1) { list += ", "; }
		list += machine;
	} else if (list.size() > limit) {
		if (limit > 50) {
			list.erase(limit-3);
			list += "...";
		} else {
			list.erase(limit);
		}
	}
}

int anaMachines::print_fail_list(std::string & out, entry ety) {
	std::string &list = alist[ety];
	if (list.empty()) return 0;
	out += " [";
	out += list;
	out += "]";
	return (int)list.size() + 3;
}
#else
static void append_to_fail_list(std::string & list, const char * entry, size_t limit)
{
	if ( ! limit || list.size() < limit) {
		if (list.size() > 1) { list += ", "; }
		list += entry;
	} else if (list.size() > limit) {
		if (limit > 50) {
			list.erase(limit-3);
			list += "...";
		} else {
			list.erase(limit);
		}
	}
}

static bool is_exhausted_partionable_slot(ClassAd* slotAd, ClassAd* jobAd)
{
	bool within = false, is_pslot = false;
	if (slotAd->LookupBool("PartitionableSlot", is_pslot) && is_pslot) {
		ExprTree * expr = slotAd->Lookup(ATTR_WITHIN_RESOURCE_LIMITS);
		classad::Value val;
		if (expr && EvalExprToBool(expr, slotAd, jobAd, val) && val.IsBooleanValueEquiv(within)) {
			return ! within;
		}
		return false;
	}
	return false;
}

#endif

static bool checkOffer(
	ClassAd * request,
	ClassAd * offer,
	anaCounters & ac,
	anaMachines * pmat,
	const char * slotname)
{
	// 1. Request satisfied? 
	if ( ! IsAConstraintMatch(request, offer)) {
		ac.fReqConstraint++;
		if (pmat) pmat->append_to_fail_list(anaMachines::ReqConstraint, slotname);
		return false;
	}
	ac.job_matches_slot++;

	// 2. Offer satisfied? 
	if ( ! IsAConstraintMatch(offer, request)) {
		ac.fOffConstraint++;
		if (pmat) pmat->append_to_fail_list(anaMachines::OffConstraint, slotname);
		return false;
	}
	ac.both_match++;

	bool offline = false;
	if (offer->LookupBool(ATTR_OFFLINE, offline) && offline) {
		ac.fOffline++;
		if (pmat) pmat->append_to_fail_list(anaMachines::Offline, slotname);
	}
	return true;
}

#if 0
static bool checkPremption (
	std::string & user,
	ClassAd * request,
	ClassAd * offer,
	anaCounters & ac,
	anaPrio * prio,
	anaMachines * pmat,
	const char * slotname)
{
		// 3. Is there a remote user?
		string remoteUser;
		if ( ! offer->LookupString(ATTR_REMOTE_USER, remoteUser)) {
			// no remote user
			if (EvalExprToBool(stdRankCondition, offer, request, eval_result) &&
				eval_result.IsBooleanValue(val) && val) {
				// both sides satisfied and no remote user
				//if( verbose ) strcat(return_buff, "Available\n");
				ac.available++;
				return true;
			} else {
				// no remote user and std rank condition failed
			  if (last_rej_match_time != 0) {
				ac.fRankCond++;
				if (pmat) pmat->append_to_fail_list(anaMachines::RankCond, slotname);
				//if( verbose ) strcat( return_buff, "Failed rank condition: MY.Rank > MY.CurrentRank\n");
				return true;
			  } else {
				ac.available++; // tj: is this correct?
				return true;
			  }
			}
		}

		// if we get to here, there is a remote user, if we don't have access to user priorities
		// we can't decide whether we should be able to preempt other users, so we are done.
		ac.machinesRunningJobs++;
		if ( ! prio || (prio->ixSubmittor < 0)) {
			if (user == remoteUser) {
				ac.machinesRunningUsersJobs++;
			} else if (pmat) {
				pmat->append_to_fail_list(anaMachines::PreemptPrio, slotname); // borrow preempt prio list
			}
			return true;
		}

		// machines running your jobs will never preempt for your job.
		if (user == remoteUser) {
			ac.machinesRunningUsersJobs++;

		// 4. Satisfies preemption priority condition?
		} else if (EvalExprToBool(preemptPrioCondition, offer, request, eval_result) &&
			eval_result.IsBooleanValue(val) && val) {

			// 5. Satisfies standard rank condition?
			if (EvalExprToBool(stdRankCondition, offer, request, eval_result) &&
				eval_result.IsBooleanValue(val) && val )  
			{
				//if( verbose ) strcat( return_buff, "Available\n");
				ac.available++;
				continue;
			} else {
				// 6.  Satisfies preemption rank condition?
				if (EvalExprToBool(preemptRankCondition, offer, request, eval_result) &&
					eval_result.IsBooleanValue(val) && val)
				{
					// 7.  Tripped on PREEMPTION_REQUIREMENTS?
					if (EvalExprToBool( preemptionReq, offer, request, eval_result) &&
						eval_result.IsBooleanValue(val) && !val) 
					{
						ac.fPreemptReqTest++;
						if (pmat) pmat->append_to_fail_list(anaMachines::PreemptReqTest, slotname.c_str(), verb_width);
						/*
						if( verbose ) {
							sprintf_cat( return_buff,
									"%sCan preempt %s, but failed "
									"PREEMPTION_REQUIREMENTS test\n",
									buffer,
									 remoteUser.c_str());
						}
						*/
						continue;
					} else {
						// not held
						/*
						if( verbose ) {
							sprintf_cat( return_buff,
								"Available (can preempt %s)\n", remoteUser.c_str());
						}
						*/
						ac.available++;
					}
				} else {
					// failed 6 and 5, but satisfies 4; so have priority
					// but not better or equally preferred than current
					// customer
					// NOTE: In practice this often indicates some
					// unknown problem.
				  if (last_rej_match_time != 0) {
					ac.fRankCond++;
				  }
				}
			}
		} else {
			ac.fPreemptPrioCond++;
			if (pmat) pmat->append_to_fail_list(anaMachines::PreemptPrio, slotname);
			/*
			if( verbose ) {
				sprintf_cat( return_buff, "Insufficient priority to preempt %s\n", remoteUser.c_str() );
			}
			*/
			continue;
		}
}
#endif

// Make an overlay ad from a pslot that sets Cpus=TotalSlotCpus, etc
ClassAd * make_pslot_overlay_ad(ClassAd *slot)
{
	classad::References attrs;
	std::string attrs_str;
	if ( ! slot->LookupString(ATTR_MACHINE_RESOURCES, attrs_str)) { attrs_str = "Cpus Memory Disk"; }
	add_attrs_from_string_tokens(attrs, attrs_str.c_str());

	classad::ClassAdParser par;
	par.SetOldClassAd( true );

	ClassAd * over = new ClassAd;
	for (auto it = attrs.begin(); it != attrs.end(); ++it) {
		std::string tot("TotalSlot"); tot += *it;
		classad::ExprTree * expr = par.ParseExpression(tot, true);
		if (expr) { over->Insert(*it, expr); }
	}
	if (over->size() <= 0) {
		delete over;
		over = NULL;
	}
	return over;
}

// class to automatically unchain an ad and then delete it
class autoOverlayAd {
	ClassAd * pad;
public:
	autoOverlayAd() : pad(NULL) {}
	~autoOverlayAd() {
		if (pad) {
			pad->Unchain();
			delete pad;
		}
		pad = NULL;
	}
	void chain(ClassAd * ad, ClassAd * parent) {
		pad = ad;
		ad->ChainToAd(parent);
	}
};

// check to see if matchmaking analysis makes sense, and (optionally) fill out analysis counters
// this function can count matching/non-matching machines, but
// does NOT do matchmaking (requirements) analysis.
// it returns the number of slots that the job matches and that match the job.
// on return it fills out anaCounters, anaPrio and anaMachines if those have been provided.
bool doJobRunAnalysis (
	ClassAd *request,
	DaemonAllowLocateFull* schedd,
	std::string &job_status,
	int mode,          // always do match analysis, even when the job is held,running etc.
	anaCounters & ac,  // output: if non-null, counts machines with various match/reject categories
	anaPrio * prio,     // output: if non-null, filled with priority information for the submittor
	anaMachines * pmat) // output: if non-null, filled with partial lists if machines that match/reject
{
	classad::Value	eval_result;
	bool	val;
	int		universe = CONDOR_UNIVERSE_MIN;
	int		jobState;
	std::string owner;
	std::string user;
	std::string slotname;

	job_status = "";

	ac.clear();

	if ( ! request->LookupString(ATTR_OWNER , owner)) {
		job_status = "Nothing here.";
		return 0;
	}
	request->LookupString(ATTR_USER, user);

	int last_rej_match_time=0;
	request->LookupInteger(ATTR_LAST_REJ_MATCH_TIME, last_rej_match_time);

	request->LookupInteger(ATTR_JOB_STATUS, jobState);
	if (jobState == RUNNING || jobState == TRANSFERRING_OUTPUT || jobState == SUSPENDED) {
		job_status = "Job is running.";
	}
	if (jobState == HELD) {
		job_status = "Job is held.";
		std::string hold_reason;
		request->LookupString( ATTR_HOLD_REASON, hold_reason );
		if( hold_reason.length() ) {
			job_status += "\n\nHold reason: ";
			job_status += hold_reason;
		}
	}
	if (jobState == REMOVED) {
		job_status = "Job is removed.";
	}
	if (jobState == COMPLETED) {
		job_status = "Job is completed.";
	}

	// if we already figured out the job status, and we haven't been asked to analyze requirements anyway
	// we are done.
	if ( ! job_status.empty() && ! mode) {
		return false; // no need for match analysis
	}

	request->LookupInteger(ATTR_JOB_UNIVERSE, universe);
	if (universe == CONDOR_UNIVERSE_LOCAL || universe == CONDOR_UNIVERSE_SCHEDULER) {

		std::string match_result;
		ClassAd *scheddAd = schedd ? schedd->daemonAd() : NULL;
		if ( ! scheddAd) {
			match_result = "WARNING: A schedd ClassAd is needed to do analysis for scheduler or Local universe jobs.\n";
		} else {
			ac.totalMachines++;
			ac.job_matches_slot++;

			//PRAGMA_REMIND("should job requirements be checked against schedd ad?")

			char const *requirements_attr = (universe == CONDOR_UNIVERSE_LOCAL)
				? ATTR_START_LOCAL_UNIVERSE 
				: ATTR_START_SCHEDULER_UNIVERSE;
			bool can_start = false;
			if ( ! EvalBool(requirements_attr, scheddAd, request, can_start)) {
				formatstr_cat(match_result, "This schedd's %s policy failed to evalute for this job.\n",requirements_attr);
			} else {
				if (can_start) { ac.both_match++; } else { ac.fOffConstraint++; }
				formatstr_cat(match_result, "This schedd's %s evalutes to %s for this job.\n",requirements_attr, can_start ? "true" : "false" );
			}
		}

		if ( ! match_result.empty()) {
			if ( ! job_status.empty()) { job_status += "\n\n"; }
			job_status += match_result.c_str();
		}

		return false;
	}

	// setup submitter info
	if (prio) {
		if ( ! request->LookupInteger(ATTR_NICE_USER_deprecated, prio->niceUser)) { prio->niceUser = 0; }
		prio->ixSubmittor = findSubmittor(fixSubmittorName(user.c_str(), prio->niceUser));
		if (prio->ixSubmittor >= 0) {
			request->Assign(ATTR_SUBMITTOR_PRIO, prioTable[prio->ixSubmittor].prio);
		}
	}

	int match_analysis_needed = 0; // keep track of the total number of slots that this job will not run on.

	// compare jobs to machines
	// 
	for (auto it = startdAds.begin(); it != startdAds.end(); ++it) {
		ClassAd	*offer = it->second.get();

		bool is_pslot = false;
		bool is_dslot = false;
		if ( ! offer->LookupBool("PartitionableSlot", is_pslot)) {
			is_pslot = false;
			if ( ! offer->LookupBool("DynamicSlot", is_dslot)) is_dslot = false;
		}

		// ignore dslots
		if (is_dslot && (mode == anaMatchModePslot))
			continue;

		slotname = "undefined";
		offer->LookupString(ATTR_NAME, slotname);

		autoOverlayAd overlay; // in case we need a pslot overlay ad, this will automatically delete it when it goes out of scope
		if (is_pslot && (mode >= anaMatchModePslot)) {
			ExprTree * expr = offer->Lookup(ATTR_WITHIN_RESOURCE_LIMITS);
			classad::Value val;
			bool within = false;
			if (expr && EvalExprToBool(expr, offer, request, val) && val.IsBooleanValueEquiv(within) && ! within) {
				// if the slot doesn't fit within the pslot, try applying an overlay ad that restores the pslot to full health
				ClassAd * pov = make_pslot_overlay_ad(offer);
				if (pov) {
					overlay.chain(pov, offer);
					offer = pov; // match using the pslot overlay
				}
			}
		}

		// 0.  info from machine
		ac.totalMachines++;

		// check to see if the job matches the slot and slot matches the job
		// if the 'failed request constraint' counter goes up, job_match_analysis is suggested
		int fReqConstraint = ac.fReqConstraint;
		if ( ! checkOffer(request, offer, ac, pmat, slotname.c_str())) {
			match_analysis_needed += ac.fReqConstraint - fReqConstraint;
			continue;
		}

		// 3. Is there a remote user?
		std::string remoteUser;
		if ( ! offer->LookupString(ATTR_REMOTE_USER, remoteUser)) {
			ac.available++; // tj: is this correct?
			continue;
		}

		// if we get to here, there is a remote user, if we don't have access to user priorities
		// we can't decide whether we should be able to preempt other users, so we are done.
		ac.machinesRunningJobs++;
		if ( ! prio || (prio->ixSubmittor < 0)) {
			if (user == remoteUser) {
				ac.machinesRunningUsersJobs++;
			} else if (pmat) {
				pmat->append_to_fail_list(anaMachines::PreemptPrio, slotname.c_str()); // borrow preempt prio list
			}
			continue;
		}

		// machines running your jobs will never preempt for your job.
		if (user == remoteUser) {
			ac.machinesRunningUsersJobs++;

		// 4. Satisfies preemption priority condition?
		} else if (EvalExprToBool(preemptPrioCondition, offer, request, eval_result) &&
			eval_result.IsBooleanValue(val) && val) {

			{
				// 6.  Satisfies preemption rank condition?
				if (EvalExprToBool(preemptRankCondition, offer, request, eval_result) &&
					eval_result.IsBooleanValue(val) && val)
				{
					// 7.  Tripped on PREEMPTION_REQUIREMENTS?
					if (EvalExprToBool( preemptionReq, offer, request, eval_result) &&
						eval_result.IsBooleanValue(val) && !val) 
					{
						ac.fPreemptReqTest++;
						if (pmat) pmat->append_to_fail_list(anaMachines::PreemptReqTest, slotname.c_str());
						/*
						if( verbose ) {
							sprintf_cat( return_buff,
									"%sCan preempt %s, but failed "
									"PREEMPTION_REQUIREMENTS test\n",
									buffer,
									 remoteUser.c_str());
						}
						*/
						continue;
					} else {
						// not held
						/*
						if( verbose ) {
							sprintf_cat( return_buff,
								"Available (can preempt %s)\n", remoteUser.c_str());
						}
						*/
						ac.available++;
					}
				} else {
					ac.available++;
				}
			}
		} else {
			ac.fPreemptPrioCond++;
			if (pmat) pmat->append_to_fail_list(anaMachines::PreemptPrio, slotname.c_str());
			/*
			if( verbose ) {
				sprintf_cat( return_buff, "Insufficient priority to preempt %s\n", remoteUser.c_str() );
			}
			*/
			continue;
		}
	}

	return match_analysis_needed > 0;
}

const char * appendJobRunAnalysisToBuffer(std::string &out, ClassAd *job, std::string &job_status)
{
	int		cluster, proc;
	job->LookupInteger( ATTR_CLUSTER_ID, cluster );
	job->LookupInteger( ATTR_PROC_ID, proc );

	if ( ! job_status.empty()) {
		formatstr_cat(out, "\n%03d.%03d:  " , cluster, proc);
		out += job_status;
		out += "\n\n";
	}

	int last_match_time=0, last_rej_match_time=0;
	job->LookupInteger(ATTR_LAST_MATCH_TIME, last_match_time);
	job->LookupInteger(ATTR_LAST_REJ_MATCH_TIME, last_rej_match_time);

	if (last_match_time) {
		time_t t = (time_t)last_match_time;
		out += "Last successful match: ";
		out += ctime(&t);
		out += "\n";
	} else if (last_rej_match_time) {
		out += "No successful match recorded.\n";
	}
	if (last_rej_match_time > last_match_time) {
		time_t t = (time_t)last_rej_match_time;
		out += "Last failed match: ";
		out += ctime(&t);
		out += "\n";
		std::string rej_reason;
		if (job->LookupString(ATTR_LAST_REJ_MATCH_REASON, rej_reason)) {
			out += "Reason for last match failure: ";
			out += rej_reason;
			out += "\n";
		}
	}

	return out.c_str();
}

const char * appendJobRunAnalysisToBuffer(std::string & out, ClassAd *job, anaCounters & ac, anaPrio * prio, anaMachines * pmat)
{

	int		cluster, proc;
	job->LookupInteger( ATTR_CLUSTER_ID, cluster );
	job->LookupInteger( ATTR_PROC_ID, proc );

	if (prio) {
		formatstr_cat(out,
			 "Submittor %s has a priority of %.3f\n", prioTable[prio->ixSubmittor].name.c_str(), prioTable[prio->ixSubmittor].prio);
	}

	const char * with_prio_tag = prio ? "considering user priority" : "ignoring user priority";

	formatstr_cat(out,
		 "\n%03d.%03d:  Run analysis summary %s.  Of %d machines,\n",
		cluster, proc, with_prio_tag, ac.totalMachines);

	formatstr_cat(out, "  %5d are rejected by your job's requirements", ac.fReqConstraint);
	if (pmat) { pmat->print_fail_list(out, anaMachines::ReqConstraint); }
	out += "\n";
	formatstr_cat(out, "  %5d reject your job because of their own requirements", ac.fOffConstraint);
	if (pmat) { pmat->print_fail_list(out, anaMachines::OffConstraint); }
	out += "\n";

	if (ac.exhausted_partionable) {
		formatstr_cat(out, "  %5d are exhausted partitionable slots", ac.exhausted_partionable);
		if (pmat) { pmat->print_fail_list(out, anaMachines::Exhausted); }
		out += "\n";
	}

	if (prio) {
		formatstr_cat(out, "  %5d match and are already running one of your jobs\n", ac.machinesRunningUsersJobs);

		formatstr_cat(out, "  %5d match but are serving users with a better priority in the pool", ac.fPreemptPrioCond);
		if (prio->niceUser) { out += "(*)"; }
		if (pmat) { pmat->print_fail_list(out, anaMachines::PreemptPrio); }
		out += "\n";

		if (ac.fRankCond) {
			formatstr_cat(out, "  %5d match but current work outranks your job", ac.fRankCond);
			if (pmat) { pmat->print_fail_list(out, anaMachines::RankCond); }
			out += "\n";
		}

		formatstr_cat(out, "  %5d match but will not currently preempt their existing job", ac.fPreemptReqTest);
		if (pmat) { pmat->print_fail_list(out, anaMachines::PreemptReqTest); }
		out += "\n";

		formatstr_cat(out, "  %5d match but are currently offline\n", ac.fOffline);
		if (pmat) { pmat->print_fail_list(out, anaMachines::Offline); }
		out += "\n";

		formatstr_cat(out, "  %5d are able to run your job\n", ac.available);

		if (prio->niceUser) {
			out += "\n\t(*)  Since this is a \"nice-user\" job, it has a\n"
			       "\t     very low priority and is unlikely to preempt other jobs.\n";
		}
	} else {
		formatstr_cat(out, "  %5d match and are already running your jobs\n", ac.machinesRunningUsersJobs);

		formatstr_cat(out, "  %5d match but are serving other users",
			ac.machinesRunningJobs - ac.machinesRunningUsersJobs);
		if (pmat) { pmat->print_fail_list(out, anaMachines::PreemptPrio); }
		out += "\n";

		if (ac.fRankCond) {
			formatstr_cat(out, "  %5d match but current work outranks your job", ac.fRankCond);
			if (pmat) { pmat->print_fail_list(out, anaMachines::RankCond); }
			out += "\n";
		}

		if (ac.fOffline > 0) {
			formatstr_cat(out, "  %5d match but are currently offline", ac.fOffline);
			if (pmat) { pmat->print_fail_list(out, anaMachines::Offline); }
			out += "\n";
		}

		formatstr_cat(out, "  %5d are able to run your job\n", ac.available);
	}

	if ( ! ac.job_matches_slot || ! ac.both_match) {
		out += "\nWARNING:  Be advised:\n";
		if ( ! ac.job_matches_slot) {
			out += "   No machines matched the jobs's constraints\n";
		} else {
			out +=
				"   Job did not match any machines's constraints\n"
				"   To see why, pick a machine that you think should match and add\n"
				"     -reverse -machine <name>\n"
				"   to your query.\n";
		}
	}

	return out.c_str();
}

const char * doJobMatchAnalysisToBuffer(std::string & return_buf, ClassAd *request, int details)
{
	bool	analEachReqClause = (details & detail_analyze_each_sub_expr) != 0;
	bool	showJobAttrs = analEachReqClause && ! (details & detail_dont_show_job_attrs);
#ifdef INCLUDE_ANALYSIS_SUGGESTIONS
	bool	useNewPrettyReq = true;
#endif
	bool	rawReferencedValues = true; // show raw (not evaluated) referenced attribs.

	JOB_ID_KEY jid;
	request->LookupInteger(ATTR_CLUSTER_ID, jid.cluster);
	request->LookupInteger(ATTR_PROC_ID, jid.proc);
	char request_id[33];
	snprintf(request_id, sizeof(request_id), "%d.%03d", jid.cluster, jid.proc);

	int cSlots = (int)startdAds.size();


	{
		// first analyze the Requirements expression against the startd ads.
		//
		std::string pretty_req = "";
#ifdef INCLUDE_ANALYSIS_SUGGESTIONS
		std::string suggest_buf = "";
		analyzer.GetErrors(true); // true to clear errors
		analyzer.AnalyzeJobReqToBuffer( request, startdAds, suggest_buf, pretty_req );
		if ((int)suggest_buf.size() > SHORT_BUFFER_SIZE)
		   suggest_buf.erase(SHORT_BUFFER_SIZE, string::npos);

		bool requirements_is_false = (suggest_buf == "Job ClassAd Requirements expression evaluates to false\n\n");

		if ( ! useNewPrettyReq) {
			return_buf += pretty_req;
		}
		pretty_req = "";
#else
		const bool useNewPrettyReq = true;
#endif

		if (useNewPrettyReq) {
			classad::ExprTree* tree = request->LookupExpr(ATTR_REQUIREMENTS);
			if (tree) {
				int console_width = getDisplayWidth();
				formatstr_cat(return_buf, "The Requirements expression for job %s is\n\n    ", request_id);
				const int indent = 4;
				PrettyPrintExprTree(tree, pretty_req, indent, console_width);
				return_buf += pretty_req;
				return_buf += "\n\n";
				pretty_req = "";
			}
		}

		// then capture the values of MY attributes refereced by the expression
		// also capture the value of TARGET attributes if there is only a single ad.
		classad::References inline_attrs; // don't show this as 'referenced' attrs, because we display them differently.
		inline_attrs.insert(ATTR_REQUIREMENTS);
		if (showJobAttrs) {
			std::string attrib_values;
			formatstr(attrib_values, "Job %s defines the following attributes:\n\n", request_id);
			classad::References trefs;
			AddReferencedAttribsToBuffer(request, ATTR_REQUIREMENTS, inline_attrs, trefs, rawReferencedValues, "    ", attrib_values);
			return_buf += attrib_values;
			attrib_values = "";

			if (single_machine || cSlots == 1) { 
				for (auto it = startdAds.begin(); it != startdAds.end(); ++it) {
					ClassAd * ptarget = it->second.get();
					std::string name;
					if (AddTargetAttribsToBuffer(trefs, request, ptarget, false, "    ", attrib_values, name)) {
						return_buf += "\n";
						return_buf += name;
						return_buf += " has the following attributes:\n\n";
						return_buf += attrib_values;
					}
				}
			}
			return_buf += "\n";
		}

		// TJ's experimental analysis (now with more anal)
#ifdef INCLUDE_ANALYSIS_SUGGESTIONS
		if (analEachReqClause || requirements_is_false) {
#else
		if (analEachReqClause) {
#endif
			std::string subexpr_detail;
			anaFormattingOptions fmt = { widescreen ? getDisplayWidth() : 80, details, "Requirements", "Job", "Slot" };

			// the common analyis code wants a vector of startd ads, not a map
			std::vector<ClassAd*> ads;
			ads.reserve(startdAds.size());
			for (auto it = startdAds.begin(); it != startdAds.end(); ++it) {
				ads.push_back(it->second.get());
			}

			AnalyzeRequirementsForEachTarget(request, ATTR_REQUIREMENTS, inline_attrs, ads, subexpr_detail, fmt);
			formatstr_cat(return_buf, "The Requirements expression for job %s reduces to these conditions:\n\n", request_id);
			return_buf += subexpr_detail;
		}

#ifdef INCLUDE_ANALYSIS_SUGGESTIONS
		// write the analysis/suggestions to the return buffer
		//
		return_buf += "\nSuggestions:\n\n";
		return_buf += suggest_buf;
#endif
	}


#ifdef INCLUDE_ANALYSIS_SUGGESTIONS
    {
        std::string buffer_string = "";
        char ana_buffer[SHORT_BUFFER_SIZE];
        if( ac.fOffConstraint > 0 ) {
            buffer_string = "";
            analyzer.GetErrors(true); // true to clear errors
            analyzer.AnalyzeJobAttrsToBuffer( request, startdAds, buffer_string );
            strncpy( ana_buffer, buffer_string.c_str( ), SHORT_BUFFER_SIZE);
            ana_buffer[SHORT_BUFFER_SIZE-1] = '\0';
            strcat( return_buff, ana_buffer );
        }
    }
#endif

	int universe = CONDOR_UNIVERSE_MIN;
	request->LookupInteger( ATTR_JOB_UNIVERSE, universe );
	bool uses_matchmaking = false;
	std::string resource;
	switch(universe) {
			// Known valid
		case CONDOR_UNIVERSE_JAVA:
		case CONDOR_UNIVERSE_VANILLA:
			break;

			// Unknown
		case CONDOR_UNIVERSE_PARALLEL:
		case CONDOR_UNIVERSE_VM:
			break;

			// Maybe
		case CONDOR_UNIVERSE_GRID:
			/* We may be able to detect when it's valid.  Check for existance
			 * of "$$(FOO)" style variables in the classad. */
			request->LookupString(ATTR_GRID_RESOURCE, resource);
			if ( strstr(resource.c_str(),"$$") ) {
				uses_matchmaking = true;
				break;
			}  
			if (!uses_matchmaking) {
				return_buf += "\nWARNING: Analysis is only meaningful for Grid universe jobs using matchmaking.\n";
			}
			break;

			// Specific known bad
		case CONDOR_UNIVERSE_MPI:
			return_buf += "\nWARNING: Analysis is meaningless for MPI universe jobs.\n";
			break;

			// Specific known bad
		case CONDOR_UNIVERSE_SCHEDULER:
			/* Note: May be valid (although requiring a different algorithm)
			 * starting some time in V6.7. */
			return_buf += "\nWARNING: Analysis is meaningless for Scheduler universe jobs.\n";
			break;

			// Unknown
			/* These _might_ be meaningful, especially if someone adds a 
			 * universe but fails to update this code. */
		//case CONDOR_UNIVERSE_PIPE:
		//case CONDOR_UNIVERSE_LINDA:
		//case CONDOR_UNIVERSE_MAX:
		//case CONDOR_UNIVERSE_MIN:
		//case CONDOR_UNIVERSE_PVM:
		//case CONDOR_UNIVERSE_PVMD:
		default:
			return_buf += "\nWARNING: Job universe unknown.  Analysis may not be meaningful.\n";
			break;
	}

	return return_buf.c_str();
}


void doSlotRunAnalysis(ClassAd *slot, JobClusterMap & clusters, int console_width, int analyze_detail_level, bool tabular)
{
	fputs(doSlotRunAnalysisToBuffer(slot, clusters, console_width, analyze_detail_level, tabular), stdout);
}

const char * doSlotRunAnalysisToBuffer(ClassAd *slot, JobClusterMap & clusters, int console_width, int analyze_detail_level, bool tabular)
{
	bool analStartExpr = /*(better_analyze == 2) ||*/ (analyze_detail_level > 0);
	bool showSlotAttrs = ! (analyze_detail_level & detail_dont_show_job_attrs);
	bool rawReferencedValues = true;
	anaFormattingOptions fmt = { console_width, analyze_detail_level, "START", "Slot", "Cluster" };

	return_buff[0] = 0;

	std::string slotname = "";
	slot->LookupString(ATTR_NAME , slotname);

	bool offline = false;
	if (slot->LookupBool(ATTR_OFFLINE, offline) && offline) {
		snprintf(return_buff, sizeof(return_buff), "%-24.24s  is offline\n", slotname.c_str());
		return return_buff;
	}

	const char * slot_type = "Stat";
	bool is_dslot = false, is_pslot = false;
	if (slot->LookupBool("DynamicSlot", is_dslot) && is_dslot) slot_type = "Dyn";
	else if (slot->LookupBool("PartitionableSlot", is_pslot) && is_pslot) slot_type = "Part";

	int cTotalJobs = 0;
	int cUniqueJobs = 0;
	int cReqConstraint = 0;
	int cOffConstraint = 0;
	int cBothMatch = 0;

	std::vector<ClassAd*> jobs;
	for (JobClusterMap::iterator it = clusters.begin(); it != clusters.end(); ++it) {
		int cJobsInCluster = (int)it->second.size();
		if (cJobsInCluster <= 0)
			continue;

		// for the the non-autocluster cluster, we have to eval these jobs individually
		int cJobsToEval = (it->first == -1) ? cJobsInCluster : 1;
		int cJobsToInc  = (it->first == -1) ? 1 : cJobsInCluster;

		cTotalJobs += cJobsInCluster;

		for (int ii = 0; ii < cJobsToEval; ++ii) {
			ClassAd *job = it->second[ii];

			jobs.push_back(job);
			cUniqueJobs += 1;

			// 2. Offer satisfied?
			bool offer_match = IsAConstraintMatch(slot, job);
			if (offer_match) {
				cOffConstraint += cJobsToInc;
			}

			// 1. Request satisfied?
			if (IsAConstraintMatch(job, slot)) {
				cReqConstraint += cJobsToInc;
				if (offer_match) {
					cBothMatch += cJobsToInc;
				}
			}
		}
	}

	if ( ! tabular && analStartExpr) {

		snprintf(return_buff, sizeof(return_buff), "\n-- Slot: %s : Analyzing matches for %d Jobs in %d autoclusters\n", 
				slotname.c_str(), cTotalJobs, cUniqueJobs);

		classad::References inline_attrs; // don't show this as 'referenced' attrs, because we display them differently.
		std::string pretty_req = "";
		static std::string prev_pretty_req;
		classad::ExprTree* tree = slot->LookupExpr(ATTR_REQUIREMENTS);
		if (tree) {
			PrettyPrintExprTree(tree, pretty_req, 4, console_width);
			inline_attrs.insert(ATTR_REQUIREMENTS);

			tree = slot->LookupExpr(ATTR_START);
			if (tree) {
				pretty_req += "\n\n  START is\n    ";
				PrettyPrintExprTree(tree, pretty_req, 4, console_width);
				inline_attrs.insert(ATTR_START);
			}
			tree = slot->LookupExpr(ATTR_WITHIN_RESOURCE_LIMITS);
			if (tree) {
				pretty_req += "\n\n  " ATTR_WITHIN_RESOURCE_LIMITS " is\n    ";
				PrettyPrintExprTree(tree, pretty_req, 4, console_width);
				inline_attrs.insert(ATTR_WITHIN_RESOURCE_LIMITS);
			}

			if (prev_pretty_req.empty() || prev_pretty_req != pretty_req) {
				strcat(return_buff, "\nThe Requirements expression for this slot is\n\n    ");
				strcat(return_buff, pretty_req.c_str());
				strcat(return_buff, "\n\n");
				// uncomment this line to print out Machine requirements only when it changes.
				//prev_pretty_req = pretty_req;
			}
			pretty_req = "";

			// then capture the values of MY attributes refereced by the expression
			// also capture the value of TARGET attributes if there is only a single ad.
			if (showSlotAttrs) {
				std::string attrib_values = "";
				attrib_values = "This slot defines the following attributes:\n\n";
				classad::References trefs;
				AddReferencedAttribsToBuffer(slot, ATTR_REQUIREMENTS, inline_attrs, trefs, rawReferencedValues, "    ", attrib_values);
				strcat(return_buff, attrib_values.c_str());
				attrib_values = "";

				if (jobs.size() == 1) {
					for (size_t ixj = 0; ixj < jobs.size(); ++ixj) {
						ClassAd * job = jobs[ixj];
						if ( ! job) continue;
						std::string name;
						if (AddTargetAttribsToBuffer(trefs, slot, job, false, "    ", attrib_values, name)) {
							strcat(return_buff,"\n");
							strcat(return_buff,name.c_str());
							strcat(return_buff," has the following attributes:\n\n");
							strcat(return_buff, attrib_values.c_str());
						}
					}
				}
				//strcat(return_buff, "\n");
				strcat(return_buff, "\nThe Requirements expression for this slot reduces to these conditions:\n\n");
			}
		}

		if ( ! (analyze_detail_level & detail_inline_std_slot_exprs)) {
			inline_attrs.clear();
			inline_attrs.insert(ATTR_START);
		}

		std::string subexpr_detail;
		AnalyzeRequirementsForEachTarget(slot, ATTR_REQUIREMENTS, inline_attrs, jobs, subexpr_detail, fmt);
		strncat(return_buff, subexpr_detail.c_str(), sizeof(return_buff) - strlen(return_buff) - 1);

		//formatstr(subexpr_detail, "%-5.5s %8d\n", "[ALL]", cOffConstraint);
		//strcat(return_buff, subexpr_detail.c_str());

		formatstr(pretty_req, "\n%s: Run analysis summary of %d jobs.\n"
			"%5d (%.2f %%) match both slot and job requirements.\n"
			"%5d match the requirements of this slot.\n"
			"%5d have job requirements that match this slot.\n",
			slotname.c_str(), cTotalJobs,
			cBothMatch, cTotalJobs ? (100.0 * cBothMatch / cTotalJobs) : 0.0,
			cOffConstraint, 
			cReqConstraint);
		strcat(return_buff, pretty_req.c_str());
		pretty_req = "";

	} else {
		char fmt[sizeof("%-nnn.nnns %-4s %12d %12d %10.2f\n")];
		int name_width = MAX(longest_slot_machine_name+7, longest_slot_name);
		snprintf(fmt, sizeof(fmt), "%%-%d.%ds", MAX(name_width, 16), MAX(name_width, 16));
		strcat(fmt, " %-4s %12d %12d %10.2f\n");
		snprintf(return_buff, sizeof(return_buff), fmt, slotname.c_str(), slot_type, 
				cOffConstraint, cReqConstraint, 
				cTotalJobs ? (100.0 * cBothMatch / cTotalJobs) : 0.0);
	}

#if 1
	//PRAGMA_REMIND("TJ: what does this do?")
#else
	if (better_analyze) {
		std::string ana_buffer = "";
		strcat(return_buff, ana_buffer.c_str());
	}
#endif

	return return_buff;
}


void buildJobClusterMap(IdToClassaAdMap & jobs, const char * attr, JobClusterMap & autoclusters)
{
	autoclusters.clear();

	for (auto it = jobs.begin(); it != jobs.end(); ++it) {
		ClassAd * job = it->second.get();

		int acid = -1;
		if (job->LookupInteger(attr, acid)) {
			autoclusters[acid].push_back(job);
		} else {
			// stick auto-clusterless jobs into the -1 slot.
			autoclusters[-1].push_back(job);
		}

	}
}


int findSubmittor( const std::string &name ) 
{
	size_t			last = prioTable.size();
	
	for(size_t i = 0 ; i < last ; i++ ) {
		if( prioTable[i].name == name ) return i;
	}

	return -1;
}


static const char*
fixSubmittorName( const char *name, int niceUser )
{
	static 	bool initialized = false;
	static	char * uid_domain = 0;
	static	char buffer[128];

	if( !initialized ) {
		uid_domain = param( "UID_DOMAIN" );
		if( !uid_domain ) {
			fprintf( stderr, "Error: UID_DOMAIN not found in config file\n" );
			exit( 1 );
		}
		initialized = true;
	}

    // potential buffer overflow! Hao
	if( strchr( name , '@' ) ) {
		snprintf( buffer, sizeof(buffer), "%s%s%s", 
					niceUser ? NiceUserName : "",
					niceUser ? "." : "",
					name );
	} else {
		snprintf( buffer, sizeof(buffer), "%s%s%s@%s", 
					niceUser ? NiceUserName : "",
					niceUser ? "." : "",
					name, uid_domain );
	}

	return buffer;
}



bool warnScheddGlobalLimits(DaemonAllowLocateFull *schedd,std::string &result_buf) {
	if( !schedd ) {
		return false;
	}
	bool has_warn = false;
	ClassAd *ad = schedd->daemonAd();
	if (ad) {
		bool exhausted = false;
		ad->LookupBool("SwapSpaceExhausted", exhausted);
		if (exhausted) {
			formatstr_cat(result_buf, "WARNING -- this schedd is not running jobs because it believes that doing so\n");
			formatstr_cat(result_buf, "           would exhaust swap space and cause thrashing.\n");
			formatstr_cat(result_buf, "           Set RESERVED_SWAP to 0 to tell the scheduler to skip this check\n");
			formatstr_cat(result_buf, "           Or add more swap space.\n");
			formatstr_cat(result_buf, "           The analysis code does not take this into consideration\n");
			has_warn = true;
		}

		int maxJobsRunning 	= -1;
		int totalRunningJobs= -1;

		ad->LookupInteger( ATTR_MAX_JOBS_RUNNING, maxJobsRunning);
		ad->LookupInteger( ATTR_TOTAL_RUNNING_JOBS, totalRunningJobs);

		if ((maxJobsRunning > -1) && (totalRunningJobs > -1) && 
			(maxJobsRunning == totalRunningJobs)) { 
			formatstr_cat(result_buf, "WARNING -- this schedd has hit the MAX_JOBS_RUNNING limit of %d\n", maxJobsRunning);
			formatstr_cat(result_buf, "       to run more concurrent jobs, raise this limit in the config file\n");
			formatstr_cat(result_buf, "       NOTE: the matchmaking analysis does not take the limit into consideration\n");
			has_warn = true;
		}
	}
	return has_warn;
}


#if 1
#else

// Given a list of jobs, do analysis for each job and print out the results.
//
bool print_jobs_analysis(
	IdToClassaAdMap & jobs,
	const char * source_label,
	DaemonAllowLocateFull * pschedd_daemon,
	int analyze_detail_level, bool analyze_with_userprio,
	bool reverse_analyze, bool summarize
	)
{
	// note: pschedd_daemon may be NULL.

	int cJobs = (int)jobs.size();
	int cSlots = (int)startdAds.size();

	// build a job autocluster map
	if (verbose) { fprintf(stderr, "\nBuilding autocluster map of %d Jobs...", cJobs); }
	JobClusterMap job_autoclusters;
	buildJobClusterMap(jobs, ATTR_AUTO_CLUSTER_ID, job_autoclusters);
	size_t cNoAuto = job_autoclusters[-1].size();
	size_t cAuto = job_autoclusters.size()-1; // -1 because size() counts the entry at [-1]
	if (verbose) { fprintf(stderr, "%d autoclusters and %d Jobs with no autocluster\n", (int)cAuto, (int)cNoAuto); }

	if (reverse_analyze) { 
		if (verbose && (cSlots > 1)) { fprintf(stderr, "Sorting %d Slots...", cSlots); }
		longest_slot_machine_name = 0;
		longest_slot_name = 0;
		if (cSlots == 1) {
			// if there is a single machine ad, then default to analysis
			if ( ! analyze_detail_level) analyze_detail_level = detail_analyze_each_sub_expr;
		} else {
			// for multiple ads, figure out the width needed to display machine and slot names.
			for (auto it = startdAds.begin(); it != startdAds.end(); ++it) {
				std::string name;
				if (it->second->LookupString(ATTR_MACHINE, name)) { longest_slot_machine_name = MAX(longest_slot_machine_name, (int)name.size()); }
				if (it->second->LookupString(ATTR_NAME, name)) { longest_slot_name = MAX(longest_slot_name, (int)name.size()); }
			}
		}
		//if (verbose) { fprintf(stderr, "Done, longest machine name %d, longest slot name %d\n", longest_slot_machine_name, longest_slot_name); }
	} else {
		if (analyze_detail_level & detail_better) {
			analyze_detail_level |= detail_analyze_each_sub_expr | detail_always_analyze_req;
		}
	}

	// print banner for this source of jobs.
	printf ("\n\n-- %s\n", source_label);

	if (reverse_analyze) {
		int console_width = widescreen ? getDisplayWidth() : 80;
		if (cSlots <= 0) {
			// print something out?
		} else {
			bool analStartExpr = /*(better_analyze == 2) || */(analyze_detail_level > 0);
			if (summarize || ! analStartExpr) {
				if (cJobs <= 1) {
					// PRAGMA_REMIND("this does the wrong thing if jobs collection is really an autocluster collection")
					for (auto it = jobs.begin(); it != jobs.end(); ++it) {
						ClassAd * job = it->second.get();
						int cluster_id = 0, proc_id = 0;
						job->LookupInteger(ATTR_CLUSTER_ID, cluster_id);
						job->LookupInteger(ATTR_PROC_ID, proc_id);
						printf("%d.%d: Analyzing matches for 1 job\n", cluster_id, proc_id);
					}
				} else {
					int cNoAutoT = (int)job_autoclusters[-1].size();
					int cAutoclustersT = (int)job_autoclusters.size()-1; // -1 because [-1] is not actually an autocluster
					if (verbose) {
						printf("Analyzing %d jobs in %d autoclusters\n", cJobs, cAutoclustersT+cNoAutoT);
					} else {
						printf("Analyzing matches for %d jobs\n", cJobs);
					}
				}
				std::string fmt;
				int name_width = MAX(longest_slot_machine_name+7, longest_slot_name);
				formatstr(fmt, "%%-%d.%ds", MAX(name_width, 16), MAX(name_width, 16));
				fmt += " %4s %12.12s %12.12s %10.10s\n";

				printf(fmt.c_str(), ""    , "Slot", "Slot's Req ", "  Job's Req ", "Both   ");
				printf(fmt.c_str(), "Name", "Type", "Matches Job", "Matches Slot", "Match %");
				printf(fmt.c_str(), "------------------------", "----", "------------", "------------", "----------");
			}
			for (auto it = startdAds.begin(); it != startdAds.end(); ++it) {
				doSlotRunAnalysis(it->second.get(), job_autoclusters, console_width, analyze_detail_level, summarize);
			}
		}
	} else {
		if (summarize) {
			if (single_machine) {
				printf("%s: Analyzing matches for %d slots\n", single_machine_label, cSlots);
			} else {
				printf("Analyzing matches for %d slots\n", cSlots);
			}
			const char * fmt = "%-13s %12s %12s %11s %11s %10s %9s %s\n";
			printf(fmt, "",              " Autocluster", "   Matches  ", "  Machine  ", "  Running  ", "  Serving ", "", "");
			printf(fmt, " JobId",        "Members/Idle", "Requirements", "Rejects Job", " Users Job ", "Other User", "Available", summarize_with_owner ? "Owner" : "");
			printf(fmt, "-------------", "------------", "------------", "-----------", "-----------", "----------", "---------", summarize_with_owner ? "-----" : "");
		}

		bool already_warned_schedd_limits = false;

		for (JobClusterMap::iterator it = job_autoclusters.begin(); it != job_autoclusters.end(); ++it) {
			int cJobsInCluster = (int)it->second.size();
			if (cJobsInCluster <= 0)
				continue;

			// for the the non-autocluster cluster, we have to eval these jobs individually
			int cJobsToEval = (it->first == -1) ? cJobsInCluster : 1;
			int cJobsToInc  = (it->first == -1) ? 1 : cJobsInCluster;
			for (int ii = 0; ii < cJobsToEval; ++ii) {
				ClassAd *job = it->second[ii];
				if (summarize) {
					char achJobId[32], achAutocluster[48], achRunning[32];
					int cluster_id = 0, proc_id = 0;
					job->LookupInteger(ATTR_CLUSTER_ID, cluster_id);
					job->LookupInteger(ATTR_PROC_ID, proc_id);
					snprintf(achJobId, sizeof(achJobId), "%d.%d", cluster_id, proc_id);

					string owner;
					if (summarize_with_owner) job->LookupString(ATTR_OWNER, owner);
					if (owner.empty()) owner = "";

					int cIdle = 0, cRunning = 0;
					for (int jj = 0; jj < cJobsToInc; ++jj) {
						ClassAd * jobT = it->second[jj];
						int jobState = 0;
						if (jobT->LookupInteger(ATTR_JOB_STATUS, jobState)) {
							if (IDLE == jobState) 
								++cIdle;
							else if (TRANSFERRING_OUTPUT == jobState || RUNNING == jobState || SUSPENDED == jobState)
								++cRunning;
						}
					}

					if (it->first >= 0) {
						if (verbose) {
							snprintf(achAutocluster, sizeof(achAutoCluster), "%d:%d/%d", it->first, cJobsToInc, cIdle);
						} else {
							snprintf(achAutocluster, sizeof(achAutoCluster), "%d/%d", cJobsToInc, cIdle);
						}
					} else {
						achAutocluster[0] = 0;
					}

					anaPrio prio;
					anaCounters ac;
					std::string job_status;
					doJobRunAnalysis(job, NULL, job_status, true, ac, &prio, NULL);
					const char * fmt = "%-13s %-12s %12d %11d %11s %10d %9d %s\n";

					achRunning[0] = 0;
					if (cRunning) { sprintf(achRunning, "%d/", cRunning); }
					sprintf(achRunning+strlen(achRunning), "%d", ac.machinesRunningUsersJobs);

					printf(fmt, achJobId, achAutocluster,
							ac.totalMachines - ac.fReqConstraint,
							ac.fOffConstraint,
							achRunning,
							ac.machinesRunningJobs - ac.machinesRunningUsersJobs,
							ac.available,
							owner.c_str());
#if 0 // this is debug code to analyze the memory usage of an expression in the job instead of the normal analyze behavior.
				} else if (analyze_memory_usage) {
					int num_skipped = 0;
					QuantizingAccumulator mem_use(0,0);
					const classad::ExprTree* tree = job->LookupExpr(analyze_memory_usage);
					if (tree) {
						AddExprTreeMemoryUse(tree, mem_use, num_skipped);
					}
					size_t raw_mem, quantized_mem, num_allocs;
					raw_mem = mem_use.Value(&quantized_mem, &num_allocs);

					printf("\nMemory usage for '%s' is %d bytes from %d allocations requesting %d bytes (%d expr nodes skipped)\n",
						analyze_memory_usage, (int)quantized_mem, (int)num_allocs, (int)raw_mem, num_skipped);
					return true;
#endif
				} else {
					if (pschedd_daemon && ! already_warned_schedd_limits) {
						std::string buf;
						if (warnScheddGlobalLimits(pschedd_daemon, buf)) {
							printf("%s", buf.c_str());
						}
						already_warned_schedd_limits = true;
					}
					printJobRunAnalysis(job, pschedd_daemon, analyze_detail_level, analyze_with_userprio);
				}
			}
		}
	}

	return true;
}

#endif

