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
#include "daemon.h"
#include "dc_collector.h"
#include "classad_helpers.h"
#include "../condor_procapi/procapi.h" // for getting cpu time & process memory

static	const char	*fixSubmittorName( const char*, int );
static	ExprTree	*stdRankCondition;
static	ExprTree	*preemptRankCondition;
static	ExprTree	*preemptPrioCondition;
static	ExprTree	*preemptionReq;

std::vector<PrioEntry> prioTable;

// produce two match columns, one for undrained slots and one drained slots
// 
class DrainedAnalysisMatchDescriminator : public AnalysisMatchDescriminator {
public:
	virtual int column_width() { return 19; }              // 1234567890123456789
	virtual const char * header1(std::string & , const char * ) {
	                                               return "  Slots      If    "; }
	virtual const char * header2(std::string & ) { return " Matched   Drained "; }
	virtual const char * bar(std::string & )     { return "--------- ---------"; }
	virtual void clear(int ary[2], ClassAd * /*ad*/) { ary[1] = ary[0] = 0; }
	virtual int add(int ary[2], ClassAd* ad) {
		if (ad->GetChainedParentAd()) {
			ary[1] += 1; return ary[1];
		} else {
			ary[0] += 1; return ary[0];
		}
	}
	virtual int count(int ary[2]) { return ary[0] + ary[1]; }
	virtual const char * print(std::string & buf, int ary[2]) { 
		formatstr(buf,"%9d %9d",ary[0],ary[1]);
		return buf.c_str();
	}
	virtual const char * print(std::string & buf, const char * val) {
		formatstr(buf,"%19s", val); return buf.c_str();
	}
};


static int slot_sequence_id = 0;

// callback function for adding a slot to an IdToClassaAdMap.
static bool AddSlotToClassAdCollection(void * pv, ClassAd* ad) {
	RelatedClassads & rmap = *(RelatedClassads*)pv;

	std::string machine;
	ad->LookupString(ATTR_MACHINE,machine);
	longest_slot_machine_name = MAX(longest_slot_machine_name, (int)machine.size());

	// the sort kill will be "<machine>/<slot>[.<dslot>]/<name>/<sequence>"
	std::string key(machine);
	int slot_id = 0, dslot_id = 0;
	bool is_pslot = false;
	ad->LookupInteger(ATTR_SLOT_ID, slot_id);
	formatstr_cat(key, "/%03d", slot_id);
	if (ad->LookupInteger(ATTR_DSLOT_ID, dslot_id)) {
		formatstr_cat(key,".%03d", dslot_id);
		++rmap.numDynamic;
	} else if (ad->LookupBool(ATTR_SLOT_PARTITIONABLE, is_pslot)) {
		if (is_pslot) { ++rmap.numOverlayCandidates; }
		else { ++rmap.numStatic; }
	}
	std::string name;
	if (ad->LookupString(ATTR_NAME, name)) {
		longest_slot_name = MAX(longest_slot_name, (int)name.size());
		key += "/";
		key += name;
	}
	++slot_sequence_id;
	formatstr_cat(key, "/%010d", slot_sequence_id);

	// take ownership of the ad, and put it in the allAds collection
	// this gives is an ad index, which we can put into the adsByKey map
	auto & rad = rmap.allAds.emplace_back();
	rad.ad.reset(ad);
	rad.index = (int)rmap.allAds.size() - 1;
	rad.id = slot_id;
	rad.sub_id = dslot_id;
	auto pp = rmap.adsByKey.insert(std::pair<std::string, int>(key,rad.index));
	if ( ! pp.second) {
		fprintf( stderr, "Error: Two results with the same key : %s.\n", key.c_str() );
	}

	// assign a group_id, we group slots by machine.
	// TODO: use a better key than ATTR_MACHINE name to group by machine
	// ideal would be a collector supplied connection id
	// for all of the ads that arrive on the same socket.
	std::string group_key(machine);
	int group_id = rmap.numGroups;
	auto gp = rmap.groupMap.insert(std::pair<std::string, int>(group_key,group_id));
	if ( ! gp.second) {
		// groupMap already has an entry for this group
		group_id = gp.first->second;
	} else {
		rmap.numGroups += 1;
	}
	rad.group_id = group_id;

	if (capture_raw_fp) {
		// spill the key and ad to the capture file
		fprintf(capture_raw_fp, "#slotad group=%d key=%s\n", group_id, key.c_str());
		dump_long_to_fp(capture_raw_fp, ad);
	}

	return false; // return false to indicate we took ownership of the ad.
}


static bool is_slot_name(const char * psz)
{
	// if string contains non legal characters for a slotname, then it can't be a slot name
	// legal characters are a-zA-Z@_\.\-, but most of this are also legal for an attribute ref
	int dot_count = 0;
	while (*psz) {
		if (*psz != '-' && *psz != '.' && *psz != '@' && *psz != '_' && ! isalnum(*psz))
			return false;
		if (*psz == '.') ++dot_count;
		++psz;
	}
	// if it passes the test for no illegal characters for a slot name
	// assume it's a slotname only if it has an @ or more than one .
	return strchr(psz, '@') || dot_count > 1;
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
	const char *slot_constraint,
	const char *slotname)
{
	static bool extra_verbose = false; // set when debugging to get extra verbosity
	CondorQuery	query(STARTD_AD);
	int			rval;
	char		buffer[64];
	char		*preq;
	
	if (verbose || dash_profile) {
		fprintf(stderr, "Fetching Slot ads...");
		//fprintf(stderr, "\tn=%s sc=%s\n", slotname?slotname:"<null>", slot_constraint?slot_constraint:"<null>");
	}

	tmBefore = 0.0;
	cbBefore = 0;
	if (dash_profile) { cbBefore = ProcAPI::getBasicUsage(getpid(), &tmBefore); }

	// if there is a slot constraint, that's allowed to be a simple machine/slot name
	// in which case we build up the actual constraint expression automatically.
	if (slot_constraint) {
		if ( ! slotname && is_slot_name(slot_constraint)) {
			slotname = slot_constraint;
			slot_constraint = nullptr;
		}
	}
	if (slot_constraint) { query.addANDConstraint (slot_constraint); }

	// turn a slotname into a constraint on the name or machine
	std::string mconst;
	bool likely_single_slot = false;
	if (slotname) {
		if ( ! slot_constraint) {
			single_machine = true;
			single_machine_label = slotname;
		}
		// a STARTD name *can* have an @ in it, but it's unlikely
		likely_single_slot = strchr(slotname, '@') != nullptr;
		formatstr(mconst, "(" ATTR_NAME "==\"%s\" || " ATTR_MACHINE "==\"%s\")", slotname, slotname);
		query.addANDConstraint(mconst.c_str());
	}


	// fetch startd ads
	if (machineads_file != NULL) {
		query.getRequirements(mconst);
		ConstraintHolder constr; constr.parse(mconst.c_str());
		if (verbose) { fprintf(stderr, "from file with constraint: %s\n", constr.c_str()); }
		CondorClassAdFileParseHelper parse_helper("\n", machineads_file_format);
		if ( ! iter_ads_from_file(machineads_file, AddSlotToClassAdCollection, &startdAds, parse_helper, constr.Expr())) {
			exit (1);
		}
	} else {
		if ( ! likely_single_slot) {
			// examine the constraint (if any) and add a clause excluding DynamicSlots
			// if the constraint is not already likely to select a single slot
			// and does not already appear to constrain by slot type
			ClassAd qad;
			query.getQueryAd(qad);
			ExprTree * req = qad.Lookup(ATTR_REQUIREMENTS);
			classad::References refs;
			if (req) qad.GetExternalReferences(req, refs, false);
			if ( ! refs.contains(ATTR_SLOT_PARTITIONABLE) &&
				 ! refs.contains(ATTR_SLOT_DYNAMIC) &&
				 ! refs.contains(ATTR_SLOT_TYPE) &&
				 ! refs.contains(ATTR_DSLOT_ID)
				) {
				query.addANDConstraint("!(DynamicSlot?:false)");
				if (verbose) fprintf( stderr, "(excluding dynamic slots)...");
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
		if (extra_verbose) {
			fprintf(stderr, "\n");
			for (auto & slotad : startdAds.allAds) {
				fprintf(stderr, "\t%04d.%d.%03d [%04d]\n", slotad.group_id, slotad.id, slotad.sub_id, slotad.index);
			}
		}
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

void setupUserpriosForAnalysis(DCCollector* pool, FILE* capture_fp, const char *userprios_file)
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
		cPrios = fetchSubmittorPriosFromNegotiator(pool, capture_fp, prioTable);
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
	for (auto & slotad : startdAds.allAds) {
		ClassAd *ad = slotad.get();
		if (ad->LookupString( ATTR_REMOTE_USER , remoteUser)) {
			int index;
			if ((index = findSubmittor(remoteUser)) != -1) {
				ad->Assign(ATTR_REMOTE_USER_PRIO, prioTable[index].prio);
			}
		}
	}
}


int fetchSubmittorPriosFromNegotiator(DCCollector* pool, FILE * /*capture_fp*/, std::vector<PrioEntry> & prios)
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

	bool do_analyze_req = doJobRunAnalysis(
			job, schedd, job_status,
			mode, ac,
			withUserprio ? &prio : NULL,
			verbose ? &mach : NULL);

	std::string buffer;
	if (do_analyze_req || always_analyze_req) {
		if (do_analyze_req && ! always_analyze_req) details |= detail_analyze_each_sub_expr;
		fputs(doJobMatchAnalysisToBuffer(buffer, job, details, mode), stdout);
		fputs("\n",stdout);
		buffer.clear();
	}

	if (ac.totalSlots > 0) {
		appendJobMatchTotalsToBuffer(buffer, job, ac,
			withUserprio ? &prio : NULL,
			verbose ? &mach : NULL);
	}

	buffer += "\n";
	appendBasicJobRunAnalysisToBuffer(buffer, job, job_status);

	fputs(buffer.c_str(), stdout);
}

void anaMachines::append_to_list(entry ety, RelatedClassads::AnnotatedClassAd & machine)
{
	if (lists.empty()) { lists.resize(WillingIfDrained+1); }
	lists[ety].push_back(machine.index);
}

// append (up to limit) items by name to the output string
int anaMachines::print_list(std::string & out, entry ety, const char * label, RelatedClassads & adlist, int limit /*=10*/)
{
	if (lists.empty() || lists[ety].empty()) return 0;
	out += label;
	int num = 0;
	for (auto & index : lists[ety]) {
		if ( ! limit) { out += ", ..."; break; }
		std::string name;
		adlist.allAds[index].ad->LookupString(ATTR_NAME, name);
		out += num ? ", " : " ";
		out += name;
		--limit; ++num;
	}
	return (int) lists[ety].size();
}

// hack to avoid refactoring
int anaMachines::print_fail_list(std::string & out, entry ety) {
	return print_list(out, ety, ":", startdAds);
}

static bool checkOffer(
	ClassAd * request,
	ClassAd * offer,
	anaCounters & ac,
	anaMachines * pmat,
	RelatedClassads::AnnotatedClassAd & machine)
{
	// ClassAd * offer = machine.ad.get();
	auto & groupCounts = startdAds.groupIdCount[machine.group_id];
	// 1. Request satisfied? 
	if ( ! IsAConstraintMatch(request, offer)) {
		groupCounts.forward_match = 1;
		ac.fReqConstraint++;
		if (pmat) pmat->append_to_list(anaMachines::ReqConstraint, machine);
		return false;
	}
	ac.job_matches_slot++;

	// 2. Offer satisfied? 
	if ( ! IsAConstraintMatch(offer, request)) {
		groupCounts.reverse_match = 1;
		ac.fOffConstraint++;
		if (pmat) pmat->append_to_list(anaMachines::OffConstraint, machine);
		return false;
	}
	groupCounts.both_match = 1;
	ac.both_match++;

	bool offline = false;
	if (offer->LookupBool(ATTR_OFFLINE, offline) && offline) {
		groupCounts.offline = 1;
		ac.fOffline++;
		if (pmat) pmat->append_to_list(anaMachines::Offline, machine);
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

// construct an AvailableGPUs expression that references all GPUs property ads
bool make_all_gpus_available_expr(ClassAd *slot, std::string & expr_str)
{
	std::string gpu_ids;
	std::set<std::string> offline_ids;
	if (slot->LookupString("OfflineGPUs", gpu_ids)) {
		for (auto & gpuid : StringTokenIterator(gpu_ids)) { offline_ids.insert(gpuid); }
	}
	if ( ! slot->LookupString("AssignedGPUs", gpu_ids)) {
		return false;
	}

	expr_str = "{";

	for (auto & gpuid : StringTokenIterator(gpu_ids)) {
		if (offline_ids.contains(gpuid)) continue;
		// munge gpuid into a gpus property ad attribute name
		// this is the same algorithm used by the STARTD
		std::string prop_attr = "GPUs_" + gpuid;
		cleanStringForUseAsAttr(prop_attr, '_');
		if (slot->Lookup(prop_attr)) {
			if (expr_str.size() > 1) expr_str += ", ";
			expr_str += prop_attr;
		}
	}

	expr_str += "}";
	return true;
}

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
	for (auto & tag : attrs) {
		std::string tot("TotalSlot"); tot += tag;
		classad::ExprTree * expr = par.ParseExpression(tot, true);
		if (expr) { over->Insert(tag, expr); }

		// special case for GPU properties, if not all gpus are currently available
		// add an AvailableGPUs attribute to the overlay
		if (YourStringNoCase("gpus") == tag) {
			classad::Value val;
			long long num_gpus_avail = -1;
			long long tot_slot_gpus = -1;
			if (slot->LookupInteger(tot, tot_slot_gpus) &&
				slot->EvaluateExpr("size(AvailableGpus)", val) &&
				val.IsNumber(num_gpus_avail) &&
				num_gpus_avail < tot_slot_gpus) {
				// some gpus that should be available are not, it's worth making an overlay of AvailableGPUs
				std::string expr_str;
				if (make_all_gpus_available_expr(slot, expr_str)) {
					over->AssignExpr("AvailableGPUs", expr_str.c_str());
				}
			}
		}
	}
	if (over->size() <= 0) {
		delete over;
		over = NULL;
	}
	return over;
}

// class to automatically unchain an ad and then delete it
class autoOverlayAd {
	ClassAd * pad{nullptr};
public:
	~autoOverlayAd() {
		if (pad) {
			pad->Unchain();
			delete pad;
		}
		pad = NULL;
	}
	//ClassAd * take_ad() { ClassAd * ret = pad; pad = nullptr; return ret; }
	ClassAd * ad() { return pad; }
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
	time_t  cool_down = 0;
	time_t  server_time = time(nullptr);
	std::string owner;
	std::string user;

	// prepare to use the groupIdCount to keep track of unique groups we have tried to match
	startdAds.groupIdCount.clear();
	startdAds.groupIdCount.resize(startdAds.numGroups);

	job_status = "";

	ac.clear();

	if ( ! request->LookupString(ATTR_OWNER , owner)) {
		job_status = "Nothing here.";
		return 0;
	}
	request->LookupString(ATTR_USER, user);

	request->LookupInteger(ATTR_JOB_STATUS, jobState);
	request->LookupInteger(ATTR_JOB_COOL_DOWN_EXPIRATION, cool_down);
	request->LookupInteger(ATTR_SERVER_TIME, server_time);
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
	if (jobState == IDLE && cool_down > server_time) {
		job_status = "Job is in cool down.";
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
			ac.totalSlots++;
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
			job_status += match_result;
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
	static bool overlay_only_when_it_fits = true; // flip this in the debugger to keep all overlays

	// Compare jobs to machines, we use the sorted ads iteration here.
	// If we end up creating an overlay, it will *Not* be added to the
	// sorted ads collection, but it will be appended to the allAds collection.
	//
	for (auto & [key,index] : startdAds.adsByKey) {
		auto & slotad = startdAds.allAds[index];
		ClassAd	*offer = slotad.get();

		bool is_pslot = false;
		bool is_dslot = false;
		if ( ! offer->LookupBool("PartitionableSlot", is_pslot)) {
			is_pslot = false;
			if ( ! offer->LookupBool("DynamicSlot", is_dslot)) is_dslot = false;
		}

		// update counts by group, where in this case slots are grouped by STARTD
		auto & groupCounts = startdAds.groupIdCount[slotad.group_id];
		if ( ! groupCounts.tot) {
			// this is the first time i have seen this group/machine
			ac.totalUniqueMachines++;
		}
		++groupCounts.tot;
		if (is_pslot) groupCounts.major++;

		// ignore dslots if the match mode is Pslot (and static) only
		if (is_dslot && (mode == anaMatchModePslot)) {
			ac.totalDSlotsSkipped++;
			continue;
		}

		auto & machine = slotad;
		groupCounts.considered = 1;

		bool within_limits = true; // within limits of original slot ad
		autoOverlayAd overlay; // in case we need a pslot overlay ad, this will automatically delete it when it goes out of scope
		if (is_pslot && (mode >= anaMatchModePslot)) {
			ExprTree * expr = offer->Lookup(ATTR_WITHIN_RESOURCE_LIMITS);
			classad::Value val;
			if (expr && EvalExprToBool(expr, offer, request, val) && val.IsBooleanValueEquiv(within_limits) && ! within_limits) {
				// if the slot doesn't fit within the pslot, try applying an overlay ad that restores the pslot to full health
				ClassAd * pov = make_pslot_overlay_ad(offer);
				if (pov) {
					std::string adbuf, slotname;
					offer->LookupString(ATTR_NAME, slotname);
					dprintf(D_ZKM, "creating drain simulation overlay for pslot %s :\n%s",
						slotname.c_str(), formatAd(adbuf, *pov, "\t", nullptr, false));

					// does the job fit if we drain first?
					bool keep_overlay = true;
					if (overlay_only_when_it_fits) {
						pov->ChainToAd(offer);
						bool within_drain = within_limits;
						if (EvalExprToBool(expr, pov, request, val) && val.IsBooleanValueEquiv(within_drain) && ! within_drain) {
							// if the slot still doesn't fit, discard the overlay
							dprintf(D_ZKM, "discarding drain simulation overlay for pslot %s\n",slotname.c_str());
							keep_overlay = false;
						}
						pov->Unchain();
					}

					// append the overlay ad to the all ads collection
					// we use the sub_id field to link the original ad to the overlay and vv.
					if (keep_overlay) {
						groupCounts.drain_sim = 1;

						// save off the overlay ad
						auto & rad = startdAds.allAds.emplace_back();
						rad.ad.reset(pov);
						rad.index = (int)startdAds.allAds.size() - 1;
						rad.id = machine.id;
						rad.sub_id = -slotad.index; // hacky link of orig and overlay
						slotad.sub_id = -rad.index; // using the sub_id field
						rad.group_id = machine.group_id;
						startdAds.numOverlayAds += 1;

						pov->ChainToAd(offer);
						offer = pov;
					}
				}
			}

			if (within_limits) groupCounts.within_limits = 1;
		}

		bool backfill = false;
		if (offer->LookupBool("BackfillSlot", backfill) && backfill) {
			groupCounts.backfill = 1;
		}

		// 0.  info from machine
		ac.totalSlots++;

		// check to see if the job matches the slot and slot matches the job
		// if the 'failed request constraint' counter goes up, job_match_analysis is suggested
		int fReqConstraint = ac.fReqConstraint;
		if ( ! checkOffer(request, offer, ac, pmat, machine)) {
			match_analysis_needed += ac.fReqConstraint - fReqConstraint;
			continue;
		}

		// job and slot match each other

		// 3. Is there a remote user?
		std::string remoteUser;
		if ( ! offer->LookupString(ATTR_REMOTE_USER, remoteUser)) {
			if (within_limits) {
				ac.available_now++;
				if (pmat) pmat->append_to_list(anaMachines::Willing, machine);
			} else {
				ac.available_if_drained++;
				if (pmat) pmat->append_to_list(anaMachines::WillingIfDrained, machine);
			}
			continue;
		}

		// if we get to here, there is a remote user, if we don't have access to user priorities
		// we can't decide whether we should be able to preempt other users, so we are done.
		ac.slotsRunningJobs++;
		if ( ! prio || (prio->ixSubmittor < 0)) {
			if (user == remoteUser) {
				ac.slotsRunningYourJobs++;
			} else if (pmat) {
				pmat->append_to_list(anaMachines::PreemptPrio, machine); // borrow preempt prio list
			}
			continue;
		}

		// machines running your jobs will never preempt for your job.
		if (user == remoteUser) {
			ac.slotsRunningYourJobs++;

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
						if (pmat) pmat->append_to_list(anaMachines::PreemptReqTest, machine);
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
						if (within_limits) {
							ac.available_now++;
							if (pmat) pmat->append_to_list(anaMachines::Willing, machine);
						} else {
							ac.available_if_drained++;
							if (pmat) pmat->append_to_list(anaMachines::WillingIfDrained, machine);
						}
					}
				} else {
					if (within_limits) {
						ac.available_now++;
						if (pmat) pmat->append_to_list(anaMachines::Willing, machine);
					} else {
						ac.available_if_drained++;
						if (pmat) pmat->append_to_list(anaMachines::WillingIfDrained, machine);
					}
				}
			}
		} else {
			ac.fPreemptPrioCond++;
			if (pmat) pmat->append_to_list(anaMachines::PreemptPrio, machine);
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

const char * appendBasicJobRunAnalysisToBuffer(std::string &out, ClassAd *job, std::string &job_status)
{
	int		cluster, proc, autocluster;
	job->LookupInteger( ATTR_CLUSTER_ID, cluster );
	job->LookupInteger( ATTR_PROC_ID, proc );
	job->LookupInteger( ATTR_AUTO_CLUSTER_ID, autocluster );

	if ( ! job_status.empty()) {
		formatstr_cat(out, "\n%03d.%03d:  " , cluster, proc);
		out += job_status;
		out += "\n\n";
	}

	time_t last_match_time=0, last_rej_match_time=0;
	job->LookupInteger(ATTR_LAST_MATCH_TIME, last_match_time);
	job->LookupInteger(ATTR_LAST_REJ_MATCH_TIME, last_rej_match_time);

	if (last_match_time) {
		out += "Last successful match: ";
		out += ctime(&last_match_time); chomp(out);
		out += "\n";
	} else if (last_rej_match_time) {
		out += "No successful match recorded.\n";
	}
	if (last_rej_match_time > last_match_time) {
		out += "Last failed match: ";
		out += ctime(&last_rej_match_time); chomp(out);
		std::string rej_negotiator;
		if (job->LookupString(ATTR_LAST_REJ_MATCH_NEGOTIATOR, rej_negotiator) && ! rej_negotiator.empty()) {
			out += " from ";
			out += rej_negotiator;
		}
		out += "\n";
		std::string rej_reason;
		if (job->LookupString(ATTR_LAST_REJ_MATCH_REASON, rej_reason)) {
			out += "Reason for last match failure: ";
			out += rej_reason;
			out += "\n";
		}
	} else {
		// check for rejections in the autocluster (24.X or later schedds can report this)
		auto found = autoclusterRejAds.find(autocluster);
		if (found != autoclusterRejAds.end()) {
			auto & rejAd = found->second;
			last_rej_match_time = 0;
			rejAd.LookupInteger(ATTR_LAST_REJ_MATCH_TIME, last_rej_match_time);
			if (last_rej_match_time > last_match_time) {
				out += "Last failed match for this autocluster: ";
				out += ctime(&last_rej_match_time); chomp(out);
				std::string rej_negotiator;
				if (rejAd.LookupString(ATTR_LAST_REJ_MATCH_NEGOTIATOR, rej_negotiator) && ! rej_negotiator.empty()) {
					out += " from ";
					out += rej_negotiator;
				}
				out += "\n";
				std::string rej_reason;
				if (rejAd.LookupString(ATTR_LAST_REJ_MATCH_REASON, rej_reason)) {
					out += "\tReason: ";
					out += rej_reason;
					out += "\n";
				}
			}

			if (rejAd.LookupInteger(ATTR_LAST_MATCH_TIME, last_match_time)) {
				out += "Last successful match for this autocluster: ";
				out += ctime(&last_match_time); chomp(out);
				out += "\n";
			}
			int num_running=0, num_completed=0;
			if (rejAd.LookupInteger("NumRunning", num_running) || rejAd.LookupInteger("NumCompleted", num_completed)) {
				formatstr_cat(out, "This autocluster currently has %d running", num_running);
				if (num_completed) { formatstr_cat(out, " and %d completed", num_completed); }
				out += " jobs.\n";
			}
		}
	}

	return out.c_str();
}

const char * appendJobMatchTotalsToBuffer(std::string & out, ClassAd *job, anaCounters & ac, anaPrio * prio, anaMachines * pmat)
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
		"\n%03d.%03d:  Run analysis summary %s.  Of %d slots on %d machines,\n",
		cluster, proc, with_prio_tag, ac.totalSlots, ac.totalUniqueMachines);

	formatstr_cat(out, "  %5d slots are rejected by your job's requirements", ac.fReqConstraint);
	if (pmat) { pmat->print_fail_list(out, anaMachines::ReqConstraint); }
	out += "\n";
	formatstr_cat(out, "  %5d slots reject your job because of their own requirements", ac.fOffConstraint);
	if (pmat) { pmat->print_fail_list(out, anaMachines::OffConstraint); }
	out += "\n";

	if (prio) {
		formatstr_cat(out, "  %5d slots match and are already running one of your jobs\n", ac.slotsRunningYourJobs);

		formatstr_cat(out, "  %5d slots match but are serving users with a better priority in the pool", ac.fPreemptPrioCond);
		if (prio->niceUser) { out += "(*)"; }
		if (pmat) { pmat->print_fail_list(out, anaMachines::PreemptPrio); }
		out += "\n";

		if (ac.fRankCond) {
			formatstr_cat(out, "  %5d slots match but current work outranks your job", ac.fRankCond);
			if (pmat) { pmat->print_fail_list(out, anaMachines::RankCond); }
			out += "\n";
		}

		formatstr_cat(out, "  %5d slots match but will not currently preempt their existing job", ac.fPreemptReqTest);
		if (pmat) { pmat->print_fail_list(out, anaMachines::PreemptReqTest); }
		out += "\n";

		formatstr_cat(out, "  %5d slots match but are currently offline\n", ac.fOffline);
		if (pmat) { pmat->print_fail_list(out, anaMachines::Offline); }
		out += "\n";

		formatstr_cat(out, "  %5d slots match and are willing to run your job", ac.available_now);
		if (pmat) { pmat->print_list(out, anaMachines::Willing, ":", startdAds); }
		out += "\n";
		if (ac.available_if_drained) {
			formatstr_cat(out, "  %5d slots would match if drained", ac.available_if_drained);
			if (pmat) { pmat->print_list(out, anaMachines::WillingIfDrained, ":", startdAds); }
			out += "\n";
		}

		if (prio->niceUser) {
			out += "\n\t(*)  Since this is a \"nice-user\" job, it has a\n"
				"\t     very low priority and is unlikely to preempt other jobs.\n";
		}
	} else {
		if (ac.slotsRunningYourJobs) {
			formatstr_cat(out, "  %5d slots match and are already running your jobs\n", ac.slotsRunningYourJobs);
		}

		if (ac.slotsRunningJobs) {
			formatstr_cat(out, "  %5d slots match but are serving other users",
			ac.slotsRunningJobs - ac.slotsRunningYourJobs);
			if (pmat) { pmat->print_fail_list(out, anaMachines::PreemptPrio); }
			out += "\n";
		}

		if (ac.fRankCond) {
			formatstr_cat(out, "  %5d slots match but current work outranks your job", ac.fRankCond);
			if (pmat) { pmat->print_fail_list(out, anaMachines::RankCond); }
			out += "\n";
		}

		if (ac.fOffline > 0) {
			formatstr_cat(out, "  %5d slots match but are currently offline", ac.fOffline);
			if (pmat) { pmat->print_fail_list(out, anaMachines::Offline); }
			out += "\n";
		}

		formatstr_cat(out, "  %5d slots match and are willing to run your job", ac.available_now);
		if (pmat) { pmat->print_list(out, anaMachines::Willing, ":", startdAds); }
		out += "\n";
		if (ac.available_if_drained) {
			formatstr_cat(out, "  %5d slots would match if drained", ac.available_if_drained);
			if (pmat) { pmat->print_list(out, anaMachines::WillingIfDrained, ":", startdAds); }
			out += "\n";
		}
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

const char * doJobMatchAnalysisToBuffer(std::string & return_buf, ClassAd *request, int details, int mode)
{
	bool	analEachReqClause = (details & detail_analyze_each_sub_expr) != 0;
	bool	showJobAttrs = analEachReqClause && ! (details & detail_dont_show_job_attrs);
	bool	rawReferencedValues = true; // show raw (not evaluated) referenced attribs.
	int cSlots = (int)startdAds.size();
	bool skip_dslots = cSlots > 1 && (mode == anaMatchModePslot);

	JOB_ID_KEY jid;
	request->LookupInteger(ATTR_CLUSTER_ID, jid.cluster);
	request->LookupInteger(ATTR_PROC_ID, jid.proc);
	char request_id[33];
	snprintf(request_id, sizeof(request_id), "%d.%03d", jid.cluster, jid.proc);

	{
		// first analyze the Requirements expression against the startd ads.
		//
		std::string pretty_req = "";
		const bool useNewPrettyReq = true;

		if (useNewPrettyReq) {
			classad::ExprTree* tree = request->LookupExpr(ATTR_REQUIREMENTS);
			if (tree) {
				int console_width = getDisplayWidth();
				formatstr_cat(return_buf, "The Requirements expression for job %s is\n\n    ", request_id);
				const int indent = 4;
				PrettyPrintExprTree(tree, pretty_req, indent, console_width);
				return_buf += pretty_req;
				return_buf += "\n\n";
				pretty_req.clear();
			}
		}

		classad::References inline_attrs; // don't show this as 'referenced' attrs, because we display them differently.

		// print the requirements again, this time as one line per analysis step
		ExprTree * exprReq = request->Lookup(ATTR_REQUIREMENTS);
		if (exprReq) {
			anaFormattingOptions fmt = { widescreen ? getDisplayWidth() : 80, details, "Requirements", "Job", "Slot", nullptr };
			PrintNumberedExprTreeClauses(pretty_req, request, exprReq, inline_attrs, fmt);
			return_buf += pretty_req;
			return_buf += "\n";
			pretty_req.clear();
		}

		// then capture the values of MY attributes referenced by the expression
		// also capture the value of TARGET attributes if there is only a single ad.
		inline_attrs.insert(ATTR_REQUIREMENTS);
		if (showJobAttrs) {
			std::string attrib_values;
			formatstr(attrib_values, "Job %s defines the following attributes:\n\n", request_id);
			classad::References trefs;
			AddReferencedAttribsToBuffer(request, ATTR_REQUIREMENTS, inline_attrs, trefs, rawReferencedValues, "    ", attrib_values);
			return_buf += attrib_values;
			attrib_values.clear();

			// if there is only a single machine, or a single slot
			// print TARGET. attributes as well.
			// We want to print these in slot order
			if (single_machine || cSlots == 1) {
				for (auto & [key,index] : startdAds.adsByKey) {
					auto & slotad = startdAds.allAds[index];
					// skip dslots unless there is a single slot to analyze
					if (skip_dslots && slotad.sub_id > 0)
						continue;
					ClassAd * ptarget = slotad.get();
					std::string name;
					attrib_values.clear();
					if (AddTargetAttribsToBuffer(trefs, request, ptarget, false, "    ", attrib_values, name)) {
						return_buf += "\n";
						return_buf += name;
						return_buf += " has the following attributes:\n\n";
						return_buf += attrib_values;
					}

					// if there is a drain sim overlay, print that as well
					// The slotad.sub_id holds the negative of the ad index to the overlay ad
					if (slotad.sub_id < 0 && (int)startdAds.allAds.size() > -slotad.sub_id) {
						auto & drainad = startdAds.allAds[-slotad.sub_id];
						ptarget = drainad.get();
						name.clear();
						attrib_values.clear();
						if (AddTargetAttribsToBuffer(trefs, request, ptarget, false, "    ", attrib_values, name)) {
							return_buf += "\n";
							return_buf += name;
							return_buf += " has the following attributes when drained:\n\n";
							return_buf += attrib_values;
						}
					}
				}
			}
			return_buf += "\n";
		}

		// Count matches for each requirments clause (the -better in -better-analyze)
		if (analEachReqClause) {
			std::string subexpr_detail;
			// if we have any drained overlay ads, analyze with two match columns
			DrainedAnalysisMatchDescriminator matches;
			anaFormattingOptions fmt = { widescreen ? getDisplayWidth() : 80, details, "Requirements", "Job", "Slot", nullptr };
			bool include_overlay_ads = false;
			if (startdAds.numOverlayAds > 0) {
				fmt.match_descrim = &matches;
				include_overlay_ads = true;
			}

			// build a vector of ads we want to count matches for
			std::vector<ClassAd*> ads;
			ads.reserve(startdAds.size());
			for (auto & slotad : startdAds.allAds) {
				if (skip_dslots && slotad.sub_id > 0)
					continue;
				if ( ! include_overlay_ads && slotad.ad->GetChainedParentAd())
					continue;
				ads.push_back(slotad.get());
			}
			dprintf(D_ZKM, "doing detailed match analysis of %d ads\n", (int)ads.size());

			AnalyzeRequirementsForEachTarget(request, ATTR_REQUIREMENTS, inline_attrs, ads, subexpr_detail, fmt);
			formatstr_cat(return_buf, "The Requirements expression for job %s reduces to these conditions:\n\n", request_id);
			return_buf += subexpr_detail;
		}

	}


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
	std::string return_buf; return_buf.reserve(65536);
	fputs(doSlotRunAnalysisToBuffer(return_buf, slot, clusters, console_width, analyze_detail_level, tabular), stdout);
}

const char * doSlotRunAnalysisToBuffer(std::string & return_buf, ClassAd *slot, JobClusterMap & clusters, int console_width, int analyze_detail_level, bool tabular)
{
	bool analStartExpr = /*(better_analyze == 2) ||*/ (analyze_detail_level > 0);
	bool showSlotAttrs = ! (analyze_detail_level & detail_dont_show_job_attrs);
	bool rawReferencedValues = true;
	anaFormattingOptions fmt = { console_width, analyze_detail_level, "START", "Slot", "Cluster", nullptr };

	std::string slotname = "";
	slot->LookupString(ATTR_NAME , slotname);

	bool offline = false;
	if (slot->LookupBool(ATTR_OFFLINE, offline) && offline) {
		formatstr(return_buf, "%-24.24s  is offline\n", slotname.c_str());
		return return_buf.c_str();
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
	int cDrainReqConst = 0;
	int cDrainOffConst = 0;
	int cDrainBothMatch = 0;

	autoOverlayAd overlay;

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
			} else if (is_pslot && ! overlay.ad()) {
				ExprTree * expr = slot->Lookup(ATTR_WITHIN_RESOURCE_LIMITS);
				classad::Value val;
				bool within_limits = false;
				if (expr && EvalExprToBool(expr, slot, job, val) && val.IsBooleanValueEquiv(within_limits) && ! within_limits) {
					// if the slot doesn't fit within the pslot, try applying an overlay ad that restores the pslot to full health
					ClassAd * pov = make_pslot_overlay_ad(slot);
					if (pov) { overlay.chain(pov, slot); }
				}
			}

			// 1. Request satisfied?
			if (IsAConstraintMatch(job, slot)) {
				cReqConstraint += cJobsToInc;
				if (offer_match) {
					cBothMatch += cJobsToInc;
				}
			}

			// If there is drain sim overlay, try also with the overlay
			if (overlay.ad()) {
				offer_match = IsAConstraintMatch(overlay.ad(), job);
				if (offer_match) {
					cDrainOffConst += cJobsToInc;
				}
				if (IsAConstraintMatch(job, overlay.ad())) {
					cDrainReqConst += cJobsToInc;
					if (offer_match) {
						cDrainBothMatch += cJobsToInc;
					}
				}
			}
		}
	}

	if ( ! tabular && analStartExpr) {

		formatstr_cat(return_buf, "\n-- Slot: %s : Analyzing matches for %d Jobs in %d autoclusters\n", 
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
				return_buf += "\nThe Requirements expression for this slot is\n\n    ";
				return_buf += pretty_req;
				return_buf += "\n\n";
				// uncomment this line to print out Machine requirements only when it changes.
				//prev_pretty_req = pretty_req;
			}
			pretty_req.clear();

			tree = slot->LookupExpr(ATTR_REQUIREMENTS);
			if (tree) {
				return_buf += PrintNumberedExprTreeClauses(pretty_req, slot, tree, inline_attrs, fmt);
				return_buf += "\n";
			}

			// then capture the values of MY attributes referenced by the expression
			// also capture the value of TARGET attributes if there is only a single ad.
			if (showSlotAttrs) {
				return_buf += "This slot defines the following attributes:\n\n";
				classad::References trefs;
				AddReferencedAttribsToBuffer(slot, ATTR_REQUIREMENTS, inline_attrs, trefs, rawReferencedValues, "    ", return_buf);

				if (overlay.ad()) {
					return_buf += "When drained, this slot has the following attributes:\n\n";
					classad::References trefs2;
					AddReferencedAttribsToBuffer(overlay.ad(), ATTR_REQUIREMENTS, inline_attrs, trefs2, false, "    ", return_buf);
				}

				if (jobs.size() == 1) {
					for (size_t ixj = 0; ixj < jobs.size(); ++ixj) {
						ClassAd * job = jobs[ixj];
						if ( ! job) continue;
						std::string name, attrib_values;
						if (AddTargetAttribsToBuffer(trefs, slot, job, false, "    ", attrib_values, name)) {
							return_buf += "\n";
							return_buf += name;
							return_buf += " has the following attributes:\n\n";
							return_buf += attrib_values;
						}
					}
				}
				return_buf += "\nThe Requirements expression for this slot reduces to these conditions:\n\n";
			}
		}

		if ( ! (analyze_detail_level & detail_inline_std_slot_exprs)) {
			inline_attrs.clear();
			inline_attrs.insert(ATTR_START);
			inline_attrs.insert(ATTR_WITHIN_RESOURCE_LIMITS);
		}

		AnalyzeRequirementsForEachTarget(slot, ATTR_REQUIREMENTS, inline_attrs, jobs, pretty_req, fmt);
		return_buf += pretty_req;

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
		return_buf += pretty_req;
		if (overlay.ad()) {
			formatstr(pretty_req,
				"%5d would match both slot and job requirments if the slot was drained.\n"
				"%5d would match the requirements of this slot when drained.\n"
				"%5d would have job requirements that match this slot when drained\n",
				cDrainBothMatch,
				cDrainOffConst,
				cDrainReqConst);
			return_buf += pretty_req;
		}
		pretty_req.clear();

	} else {
		char fmt[sizeof("%-nnn.nnns %-4s %12d %12d %10.2f\n")+1];
		int name_width = MAX(longest_slot_machine_name+7, longest_slot_name);
		snprintf(fmt, sizeof(fmt), "%%-%d.%ds", MAX(name_width, 16), MAX(name_width, 16));
		strcat(fmt, " %-4s %12d %12d %10.2f\n");
		formatstr(return_buf, fmt, slotname.c_str(), slot_type,
				cOffConstraint, cReqConstraint, 
				cTotalJobs ? (100.0 * cBothMatch / cTotalJobs) : 0.0);
	}

	return return_buf.c_str();
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

