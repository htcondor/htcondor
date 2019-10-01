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
#include "condor_state.h"
#include "condor_api.h"
#include "status_types.h"
#include "totals.h"
#include "get_daemon_name.h"
#include "daemon.h"
#include "dc_collector.h"
#include "extArray.h"
#include "sig_install.h"
#include "string_list.h"
#include "match_prefix.h"    // is_arg_colon_prefix
#include "print_wrapped_text.h"
#include "error_utils.h"
#include "condor_distribution.h"
#include "condor_version.h"
#include "natural_cmp.h"
#include "classad/jsonSource.h"
#include "classad_helpers.h"
#include "prettyPrint.h"
#include "setflags.h"
#include "adcluster.h"

#include <vector>
#include <sstream>
#include <iostream>

// if enabled, use natural_cmp for numerical sorting of hosts/slots
#define STRVCMP (naturalSort ? natural_cmp : strcmp)

// the condor q strategy will be to ingest ads and print them into MyRowOfValues structures
// then insert those into a map which is indexed (and thus ordered) by job id.
// Once all of the jobs have arrived we linkup the JobRowOfData structures by job id and dag id.
// Finally, then we adjust column widths and formats based on the actual data and finally print.

// This structure hold the rendered fields from a single job ad, it will be inserted in to a map by job id
// and also linked up using the various pointers
class StatusRowOfData {
public:
	MyRowOfValues rov;
	unsigned int ordinal;  // id assigned to preserve the original order
	unsigned int flags;    // SROD_* flags used while processing
	ClassAd * ad;
	class StatusRowOfData * next_slot; // used at runtime to linkup slots for a machine.
	StatusRowOfData(unsigned int _ord)
		: ordinal(_ord), flags(0), ad(NULL)
		, next_slot(NULL)
	{}

	~StatusRowOfData() { if (ad) delete ad; ad = NULL; }

	bool isValid(int index) {
		if ( ! rov.is_valid(index)) return false;
		return rov.Column(index) != NULL;
	}

	template <class t>
	bool getNumber(int index, t & val) {
		val = 0;
		if ( ! rov.is_valid(index)) return false;
		classad::Value * pval = rov.Column(index);
		if ( ! pval) return false;
		return pval->IsNumber(val);
	}

	template <class s>
	bool getString(int index, s & val) {
		if ( ! rov.is_valid(index)) return false;
		classad::Value * pval = rov.Column(index);
		if ( ! pval) return false;
		return pval->IsStringValue(val);
	}
	bool getString(int index, char * buf, int cch) {
		if ( ! rov.is_valid(index)) return false;
		classad::Value * pval = rov.Column(index);
		if ( ! pval) return false;
		return pval->IsStringValue(buf, cch);
	}
};

struct STRVCMPStr {
   inline bool operator( )( const std::string &s1, const std::string &s2 ) const {
       return( natural_cmp( s1.c_str( ), s2.c_str( ) ) < 0 );
	}
};

typedef std::map<std::string, StatusRowOfData, STRVCMPStr> ROD_MAP_BY_KEY;

#define SROD_COOKED  0x0001   // set when the row data has been cooked (i.e. folded into)
#define SROD_SKIP    0x0002   // this row should be skipped entirely
#define SROD_FOLDED  0x0004   // data from this row has been folded into another row
#define SROD_PRINTED 0x0008   // Set during printing so we can know what has already been printed

#define SROD_PARTITIONABLE_SLOT 0x1000 // is a partitionable slot
#define SROD_BUSY_SLOT          0x2000 // is an busy slot (Claimed/Matched/Preempting)
#define SROD_UNAVAIL_SLOT       0x4000 // is an unavailable slot (Owner/Drained/Delete?)
#define SROD_EXHAUSTED_SLOT     0x8000 // is a partitionable slot with no cores remaining.

class ClassadSortSpecs {
public:
	ClassadSortSpecs() {}
	~ClassadSortSpecs() {
		for (size_t ii = 0; ii < key_exprs.size(); ++ii) {
			if (key_exprs[ii]) { delete key_exprs[ii]; }
			key_exprs[ii] = NULL;
		}
	}

	bool empty() { return key_args.empty(); }

	// returns true on success, false if the arg did not parse as an attribute or classad expression
	bool Add(const char * arg) {
		ExprTree* expr = NULL;
		if (ParseClassAdRvalExpr(arg, expr)) {
			return false;
		}
		key_exprs.push_back(expr);
		key_args.push_back(arg);
		return true;
	}
	// make sure that the primary key is the same as arg, inserting arg as the first key if needed
	bool ForcePrimaryKey(const char * arg) {
		if ( ! key_args.size() || key_args[0].empty() || MATCH != strcasecmp(key_args[0].c_str(), arg)) {
			ExprTree* expr = NULL;
			if (ParseClassAdRvalExpr(arg, expr)) {
				return false;
			}
			key_exprs.insert(key_exprs.begin(), expr);
			key_args.insert(key_args.begin(), arg);
		}
		return true;
	}

	void RenderKey(std::string & key, unsigned int ord, ClassAd * ad);
	void AddToProjection(classad::References & proj);
	// for debugging, dump the sort expressions
	void dump(std::string & out, const char * sep);

protected:
	std::vector<std::string> key_args;
	std::vector<classad::ExprTree*> key_exprs;
};


// global variables
std::vector<GroupByKeyInfo> group_by_keys; // TJ 8.1.5 for future use, ignored for now.
bool explicit_format = false;
bool disable_user_print_files = false; // allow command line to defeat use of default user print files.
const char		*DEFAULT= "<default>";
DCCollector* pool = NULL;
int			sdo_mode = SDO_NotSet;
int			summarySize = -1;
bool        expert = false;
const char * mode_constraint = NULL; // constraint set by mode
int			result_limit = 0; // max number of results we want back.
int			diagnose = 0;
const char* diagnostics_ads_file = NULL; // filename to write diagnostics query ads to, from -diagnose:<filename>
char*		direct = NULL;
char*       statistics = NULL;
const char*	genericType = NULL;
CondorQuery *query;
char		buffer[1024];
char		*myName;
ClassadSortSpecs sortSpecs;
bool			noSort = false; // set to true to disable sorting entirely
bool			naturalSort = true;

classad::References projList;
StringList dashAttributes; // Attributes specifically requested via the -attributes argument

bool       dash_group_by; // the "group-by" command line arguments was specified.
AdCluster<std::string> ad_groups; // does the actually grouping 
std::string get_ad_name_string(ClassAd &ad) { std::string name; ad.LookupString(ATTR_NAME, name); return name; }


char *			target = NULL;
bool			print_attrs_in_hash_order = false;


// Global mode flags.
bool			javaMode = false;
bool			vmMode = false;
bool			absentMode = false;
bool			offlineMode = false;
bool			mergeMode = false;
bool			annexMode = false;
bool			compactMode = false;

// Merge-mode globals.
const char * rightFileName = NULL;
ClassAdFileParseType::ParseType rightFileFormat = ClassAdFileParseType::Parse_long;

const char * leftFileName = NULL;
ClassAdFileParseType::ParseType leftFileFormat = ClassAdFileParseType::Parse_long;

typedef struct {
	std::set< ClassAd * > * leftSet;
	struct _process_ads_info * both_ai;
	struct _process_ads_info * right_ai;
} merge_ads_info;

// The main pretty printer.
PrettyPrinter mainPP( PP_NOTSET, PP_NOTSET, STD_HEADFOOT );

// function declarations
void usage 		();
void firstPass  (int, char *[]);
void secondPass (int, char *[]);

// prototype for CollectorList:query, CondorQuery::processAds,
// and CondorQ::fetchQueueFromHostAndProcess callbacks.
// callback should return false to take ownership of the ad
typedef bool (*FNPROCESS_ADS_CALLBACK)(void* pv, ClassAd * ad);
static bool read_classad_file(const char *filename, ClassAdFileParseType::ParseType ads_file_format, FNPROCESS_ADS_CALLBACK callback, void* pv, const char * constr, int limit);

void ClassadSortSpecs::AddToProjection(classad::References & proj)
{
	ClassAd ad;
	for (size_t ii = 0; ii < key_exprs.size(); ++ii) {
		classad::ExprTree * expr = key_exprs[ii];
		if (expr) { ad.GetExternalReferences(expr, proj, true); }
	}
}

void ClassadSortSpecs::dump(std::string & out, const char * sep)
{
	ClassAd ad;
	for (size_t ii = 0; ii < key_exprs.size(); ++ii) {
		classad::ExprTree * expr = key_exprs[ii];
		if (expr) { ExprTreeToString(expr, out); }
		out += sep;
	}
}

void ClassadSortSpecs::RenderKey(std::string & key, unsigned int ord, ClassAd * ad)
{
	for (size_t ii = 0; ii < key_exprs.size(); ++ii) {
		classad::Value val;
		classad::ExprTree * expr = key_exprs[ii];
		if (expr && ad->EvaluateExpr(expr, val)) {
			std::string fld;
			switch (val.GetType()) {
			case classad::Value::REAL_VALUE: {
				// render real by treating the bits of the double as integer bits. this works because
				// IEEE 754 floats compare the same when the bits are compared as they do
				// when compared as doubles because the exponent is the upper bits.
				union { long long lval; double dval; };
				val.IsRealValue(dval);
				formatstr(fld, "%lld", lval);
				}
				break;
			case classad::Value::INTEGER_VALUE:
			case classad::Value::BOOLEAN_VALUE: {
				long long lval;
				val.IsNumber(lval);
				formatstr(fld, "%lld", lval);
				}
				break;
			case classad::Value::STRING_VALUE:
				val.IsStringValue(fld);
				break;
			default: {
				classad::ClassAdUnParser unp;
				unp.Unparse(fld, val);
				}
				break;
			}
			key += fld;
		}
		key += "\n";
	}

	// append the ordinal as the key of last resort
	formatstr_cat(key, "%08X", ord);
}

#if 0
static void make_status_key(std::string & key, unsigned int ord, ClassAd* ad)
{
	std::string fld;
	if ( ! ad->LookupString(ATTR_MACHINE, key)) key = "";
	key += "\n";

	if ( ! ad->LookupString(ATTR_NAME, fld)) fld = "";
	key += fld;
	key += "\n";

	//if ( ! ad->LookupString(ATTR_OPSYS, fld)) fld = "";
	//if ( ! ad->LookupString(ATTR_ARCH, fld)) fld = "";

	// append the ordinal as the key of last resort
	formatstr_cat(key, "\n%08X", ord);
}
#endif

static State LookupStartdState(ClassAd* ad)
{
	char state[32];
	if (!ad->LookupString (ATTR_STATE, state, sizeof(state))) return no_state;
	return string_to_state (state);
}

// arguments passed to the process_ads_callback
struct _process_ads_info {
   ROD_MAP_BY_KEY * pmap;
   TrackTotals *  totals;
   unsigned int  ordinal;
   int           columns;
   FILE *        hfDiag; // write raw ads to this file for diagnostic purposes
   unsigned int  diag_flags;
   PrettyPrinter * pp;
};

static bool store_ads_callback( void * arg, ClassAd * ad ) {
	std::set< ClassAd * > * mySet = reinterpret_cast< std::set< ClassAd * > *>( arg );
	mySet->insert( ad );

	// Do _not_ delete the ad.
	return false;
}

static bool process_ads_callback(void * pv,  ClassAd* ad)
{
	bool done_with_ad = true;
	struct _process_ads_info * pi = (struct _process_ads_info *)pv;
	ROD_MAP_BY_KEY * pmap = pi->pmap;
	TrackTotals *    totals = pi->totals;
	PrettyPrinter &  thePP = * pi->pp;
	ASSERT( pi->pp != NULL );

	std::string key;
	unsigned int ord = pi->ordinal++;
	sortSpecs.RenderKey(key, ord, ad);
	//make_status_key(key, ord, ad);

	if (dash_group_by) {
		std::string id(key), attrs;
		int acid = ad_groups.getClusterid(*ad, true, &attrs);
		for (size_t ix = id.find_first_of("\n",0); ix != std::string::npos; ix = id.find_first_of("\n",ix)) { id.replace(ix,1,"/"); }
		fprintf(stderr, "ingested '%s', got clusterid=%d attrs=%s\n", id.c_str(), acid, attrs.c_str());
	}

	// if diagnose flag is passed, unpack the key and ad and print them to the diagnostics file
	if (pi->hfDiag) {
		if (pi->diag_flags & 1) {
			fprintf(pi->hfDiag, "#Key:");
			size_t ib = 0;
			do {
				size_t ix = key.find_first_of("\n", ib);
				size_t cb = ix != std::string::npos ?  ix-ib: std::string::npos;
				fprintf(pi->hfDiag, " / %s", key.substr(ib, cb).c_str());
				ib = ix+1;
				if (ix == std::string::npos) break;
			} while (ib != std::string::npos);
			fputc('\n', pi->hfDiag);
		}

		if (pi->diag_flags & 2) {
			fPrintAd(pi->hfDiag, *ad);
			fputc('\n', pi->hfDiag);
		}

		return true; // done processing this ad
	}

	std::pair<ROD_MAP_BY_KEY::iterator,bool> pp = pmap->insert(std::pair<std::string, StatusRowOfData>(key, ord));
	if( ! pp.second ) {
		fprintf( stderr, "Error: Two results with the same key.\n" );
		done_with_ad = true;
	} else {

		// we can do normal totals now. but compact mode totals we have to do after checking the slot type
		if (totals && ( ! pi->columns || ! compactMode)) { totals->update(ad); }

		if (pi->columns) {
			StatusRowOfData & srod = pp.first->second;

			srod.rov.SetMaxCols(pi->columns);
			thePP.pm.render(srod.rov, ad);

			bool fPslot = false;
			if (ad->LookupBool(ATTR_SLOT_PARTITIONABLE, fPslot) && fPslot) {
				srod.flags |= SROD_PARTITIONABLE_SLOT;
				double cpus = 0;
				if (ad->LookupFloat(ATTR_CPUS, cpus)) {
					if (cpus < 0.1) srod.flags |= SROD_EXHAUSTED_SLOT;
				}
			} // else
			{
				State state = LookupStartdState(ad);
				if (state == matched_state || state == claimed_state || state == preempting_state)
					srod.flags |= SROD_BUSY_SLOT;
				else if (state == drained_state || state == owner_state || state == shutdown_state || state == delete_state)
					srod.flags |= SROD_UNAVAIL_SLOT;
			}
			if (totals && compactMode) {
				char keybuf[64] = " ";
				const char * subtot_key = keybuf;
				switch(thePP.ppTotalStyle) {
					case PP_SUBMITTER_NORMAL: srod.getString(0, keybuf, sizeof(keybuf)); break;
					case PP_SCHEDD_NORMAL:
					case PP_SCHEDD_DATA:
					case PP_SCHEDD_RUN: subtot_key = NULL; break;
					case PP_STARTD_STATE: subtot_key = NULL; break; /* use activity as key */
					default: srod.getString(thePP.startdCompact_ixCol_Platform, keybuf, sizeof(keybuf)); break;
				}
				if (subtot_key) {
					int len = strlen(subtot_key);
					thePP.max_totals_subkey = MAX(thePP.max_totals_subkey, len);
				}
				totals->update(ad, TOTALS_OPTION_ROLLUP_PARTITIONABLE | TOTALS_OPTION_IGNORE_DYNAMIC, subtot_key);
				if ((srod.flags & (SROD_PARTITIONABLE_SLOT | SROD_EXHAUSTED_SLOT)) == SROD_PARTITIONABLE_SLOT) {
					totals->update(ad, 0, subtot_key);
				}
			}

			// for compact mode, we are about to throw about child state and activity attributes
			// so roll them up now
			if (compactMode && fPslot && thePP.startdCompact_ixCol_ActCode >= 0) {
				State consensus_state = no_state;
				Activity consensus_activity = no_act;

				const bool preempting_wins = true;

				char tmp[32];
				classad::Value lval, val;
				const classad::ExprList* plst = NULL;
				// roll up child states into a consensus child state
				if (ad->EvaluateAttr("Child" ATTR_STATE, lval) && lval.IsListValue(plst)) {
					for (classad::ExprList::const_iterator it = plst->begin(); it != plst->end(); ++it) {
						const classad::ExprTree * pexpr = *it;
						if (pexpr->Evaluate(val) && val.IsStringValue(tmp,sizeof(tmp))) {
							State st = string_to_state(tmp);
							if (st >= no_state && st < _state_threshold_) {
								if (consensus_state != st) {
									if (consensus_state == no_state) consensus_state = st;
									else {
										if (preempting_wins && st == preempting_state) {
											consensus_state = st; break;
										} else {
											consensus_state = _state_threshold_;
										}
									}
								}
							}
						}
					}
				}

				// roll up child activity into a consensus child state
				if (ad->EvaluateAttr("Child" ATTR_ACTIVITY, lval) && lval.IsListValue(plst)) {
					for (classad::ExprList::const_iterator it = plst->begin(); it != plst->end(); ++it) {
						const classad::ExprTree * pexpr = *it;
						if (pexpr->Evaluate(val) && val.IsStringValue(tmp,sizeof(tmp))) {
							Activity ac = string_to_activity(tmp);
							if (ac >= no_act && ac < _act_threshold_) {
								if (consensus_activity != ac) {
									if (consensus_activity == no_act) consensus_activity = ac;
									else {
										if (preempting_wins && ac == vacating_act) {
											consensus_activity = ac; break;
										} else {
											consensus_activity = _act_threshold_;
										}
									}
								}
							}
						}
					}
				}

				// roll concensus state into parent slot state.
				srod.getString(thePP.startdCompact_ixCol_ActCode, tmp, 4);
				digest_state_and_activity(tmp+4, consensus_state, consensus_activity);
				char bsc = tmp[4], bac = tmp[4+1];
				char asc = tmp[0], aac = tmp[1];
				if ((asc == 'U' && aac == 'i') && (srod.flags & SROD_EXHAUSTED_SLOT)) {
					// For exhausted partitionable slots that are Unclaimed/Idle, just use the concensus state
					asc = bsc; aac = bac;
				} else if (asc == 'D' && bsc == 'C') {
					// if partitionable slot is stated Drained and the children are claimed,
					// show the overall state as Claimed/Retiring
					asc = bsc;
				}
				if (preempting_wins) {
					if (bsc == 'P') asc = bsc;
					if (bac == 'v') aac = bac;
				}
				if (consensus_state != no_state && asc != bsc) asc = '*';
				if (consensus_activity != no_act && aac != bac) aac = '*';
				if (tmp[0] != asc || tmp[1] != aac) {
					tmp[0] = asc; tmp[1] = aac; tmp[2] = 0;
					srod.rov.Column(thePP.startdCompact_ixCol_ActCode)->SetStringValue(tmp);
				}

			}

			done_with_ad = true;
		} else {
			pp.first->second.ad = ad;
			done_with_ad = false;
		}
	}

	return done_with_ad; // return false to indicate we took ownership of the passed in ad.
}

// return true if the strings are non-empty and match up to the first \n
// or match exactly if there is no \n
bool same_primary_key(const std::string & aa, const std::string & bb)
{
	size_t cb = aa.size();
	if ( ! cb) return false;
	for (size_t ix = 0; ix < cb; ++ix) {
		char ch = aa[ix];
		if (bb[ix] != ch) return false;
		if (ch == '\n') return true;
	}
	return bb.size() == cb;
}

std::set< ClassAd * > markedAds;
static void mark( ClassAd * ad ) {
	markedAds.insert( ad );
}

static bool marked( ClassAd * ad ) {
	return markedAds.find( ad ) != markedAds.end();
}

static bool canJoin( ClassAd * left, ClassAd * right ) {
	// FIXME: for now, just use the default key.
	std::string leftKey;
	unsigned int ordinal = 0;
	sortSpecs.RenderKey( leftKey, ordinal, left );

	std::string rightKey;
	sortSpecs.RenderKey( rightKey, ordinal, right );

	return leftKey == rightKey;
}

static bool merge_ads_callback( void * arg, ClassAd * ad ) {
	merge_ads_info * mai = (merge_ads_info *)arg;
	std::set< ClassAd * > & leftSet = * mai->leftSet;
	_process_ads_info * both_ai = mai->both_ai;
	_process_ads_info * right_ai = mai->right_ai;

	// Partially determine which ROD_MAP_BY_KEY we shove this ad in.
	// (We'll do one last pass over leftSet before we starting printing;
	// mark()ed ads will be put in the both ROD, the others in the
	// right ROD.
	bool wasMatched = false;
	for( auto i = leftSet.begin(); i != leftSet.end(); ++i ) {
		if( canJoin( * i, ad ) ) {
			mark( * i );
			wasMatched = true;
		}
	}

	if( wasMatched ) {
		return process_ads_callback( both_ai, ad );
	} else {
		return process_ads_callback( right_ai, ad );
	}
}

// fold slot bb into aa assuming startdCompact format
void fold_slot_result(StatusRowOfData & aa, StatusRowOfData * pbb, PrettyPrinter & thePP)
{
	if (aa.rov.empty()) return;

	// If the destination slot is not partitionable and hasn't already been cooked, some setup work is needed.
	if ( ! (aa.flags & SROD_PARTITIONABLE_SLOT) && ! (aa.flags & SROD_COOKED)) {

		// The MaxMem column will be undefined or error, set it equal to Memory
		if (thePP.startdCompact_ixCol_FreeMem >= 0 && thePP.startdCompact_ixCol_MaxSlotMem >= 0) {
			double amem;
			aa.getNumber(thePP.startdCompact_ixCol_FreeMem, amem);
			aa.rov.Column(thePP.startdCompact_ixCol_MaxSlotMem)->SetRealValue(amem);
			aa.rov.set_col_valid(thePP.startdCompact_ixCol_MaxSlotMem, true);
		}

		// The FreeMem and FreeCpus columns should be set to 0 if the slot is busy (or unavailable?)
		if (aa.flags & SROD_BUSY_SLOT) {
			if (thePP.startdCompact_ixCol_FreeCpus >= 0) aa.rov.Column(thePP.startdCompact_ixCol_FreeCpus)->SetIntegerValue(0);
			if (thePP.startdCompact_ixCol_FreeMem >= 0) aa.rov.Column(thePP.startdCompact_ixCol_FreeMem)->SetRealValue(0.0);
		}

		// The Slots column should be set to 1
		if (thePP.startdCompact_ixCol_Slots >= 0) {
			aa.rov.Column(thePP.startdCompact_ixCol_Slots)->SetIntegerValue(1);
			aa.rov.set_col_valid(thePP.startdCompact_ixCol_Slots, true);
		}

		aa.flags |= SROD_COOKED;
	}

	if ( ! pbb)
		return;

	StatusRowOfData & bb = *pbb;

	// If the source slot is partitionable, we fold differently than if it is static
	bool partitionable = (bb.flags & SROD_PARTITIONABLE_SLOT) != 0;

	// calculate the memory size of the largest slot
	double amem = 0.0;
	double bmem = 0.0;

	if (thePP.startdCompact_ixCol_MaxSlotMem >= 0) {
		aa.getNumber(thePP.startdCompact_ixCol_MaxSlotMem, amem);
		bb.getNumber(partitionable ? thePP.startdCompact_ixCol_MaxSlotMem : thePP.startdCompact_ixCol_FreeMem, bmem);
		double maxslotmem = MAX(amem,bmem);
		aa.rov.Column(thePP.startdCompact_ixCol_MaxSlotMem)->SetRealValue(maxslotmem);
	}

	// Add FreeMem and FreeCpus from bb into aa if slot bb is not busy
	if (partitionable || !(bb.flags & SROD_BUSY_SLOT)) {
		if (thePP.startdCompact_ixCol_FreeMem >= 0) {
			aa.getNumber(thePP.startdCompact_ixCol_FreeMem, amem);
			aa.rov.Column(thePP.startdCompact_ixCol_FreeMem)->SetRealValue(bmem + amem);
		}

		int acpus, bcpus;
		if (thePP.startdCompact_ixCol_FreeCpus >= 0) {
			aa.getNumber(thePP.startdCompact_ixCol_FreeCpus, acpus);
			bb.getNumber(thePP.startdCompact_ixCol_FreeCpus, bcpus);
			aa.rov.Column(thePP.startdCompact_ixCol_FreeCpus)->SetIntegerValue(acpus + bcpus);
		}
	}

	// Increment the aa Slots column
	int aslots, bslots = 1;
	if (thePP.startdCompact_ixCol_Slots >= 0) {
		aa.getNumber(thePP.startdCompact_ixCol_Slots, aslots);
		if (partitionable) bb.getNumber(thePP.startdCompact_ixCol_Slots, bslots);
		aa.rov.Column(thePP.startdCompact_ixCol_Slots)->SetIntegerValue(aslots + bslots);
	}

	// Sum the number of job starts
	double astarts, bstarts;
	if (thePP.startdCompact_ixCol_JobStarts >= 0) {
		aa.getNumber(thePP.startdCompact_ixCol_JobStarts, astarts);
		bb.getNumber(thePP.startdCompact_ixCol_JobStarts, bstarts);
		aa.rov.Column(thePP.startdCompact_ixCol_JobStarts)->SetRealValue(astarts + bstarts);
	}

	// merge the state/activity (for static slots, partitionable merge happens elsewhere)
	if (thePP.startdCompact_ixCol_ActCode >= 0) {
		char ast[4] = {0,0,0,0}, bst[4] = {0,0,0,0};
		aa.getString(thePP.startdCompact_ixCol_ActCode, ast, sizeof(ast));
		bb.getString(thePP.startdCompact_ixCol_ActCode, bst, sizeof(bst));
		char asc = ast[0], bsc = bst[0], aac = ast[1], bac = bst[1];
		if (asc != bsc) asc = '*';
		if (aac != bac) aac = '*';
		if (ast[0] != asc || ast[1] != aac) {
			ast[0] = asc; ast[1] = aac;
			aa.rov.Column(thePP.startdCompact_ixCol_ActCode)->SetStringValue(ast);
		}
	}
}

void reduce_slot_results(ROD_MAP_BY_KEY & rmap, PrettyPrinter & thePP )
{
	if (rmap.empty())
		return;

	ROD_MAP_BY_KEY::iterator it, itMachine = rmap.begin();
	it = itMachine;
	for (++it; it != rmap.end(); ++it) {
		if (same_primary_key(it->first, itMachine->first)) {
			fold_slot_result(itMachine->second, &it->second, thePP);
			it->second.flags |= SROD_FOLDED;
		} else {
			fold_slot_result(itMachine->second, NULL, thePP);
			itMachine = it;
		}
	}
}

void doNormalOutput( struct _process_ads_info & ai, AdTypes & adType );
void doMergeOutput( struct _process_ads_info & ai );

int
main (int argc, char *argv[])
{
#if !defined(WIN32)
	install_sig_handler(SIGPIPE, (SIG_HANDLER)SIG_IGN );
#endif

	// initialize to read from config file
	myDistro->Init( argc, argv );
	myName = argv[0];
	set_priv_initialize(); // allow uid switching if root
	config();
	dprintf_config_tool_on_error(0);

	// The arguments take two passes to process --- the first pass
	// figures out the mode, after which we can instantiate the required
	// query object.  We add implied constraints from the command line in
	// the second pass.
	firstPass (argc, argv);

	// if the mode has not been set, it is SDO_Startd, we set it here
	// but with default priority, so that any mode set in the first pass
	// takes precedence.
	// the actual adType to be queried is returned, note that this will
	// _not_ be STARTD_AD if another ad type was set in pass 1
	AdTypes adType = mainPP.setMode (SDO_Startd, 0, DEFAULT);
	ASSERT(sdo_mode != SDO_NotSet);

	// instantiate query object
	if (!(query = new CondorQuery (adType))) {
		dprintf_WriteOnErrorBuffer(stderr, true);
		fprintf (stderr, "Error:  Out of memory\n");
		exit (1);
	}
	// if a first-pass setMode set a mode_constraint, apply it now to the query object
	if (mode_constraint && ! explicit_format) {
		query->addANDConstraint(mode_constraint);
	}

	// set result limit if any is desired.
	if (result_limit > 0) {
		query->setResultLimit(result_limit);
	}

		// if there was a generic type specified
	if (genericType) {
		// tell the query object what the type we're querying is
		if (diagnose) { printf ("Setting generic ad type to %s\n", genericType); }
		query->setGenericQueryType(genericType);
	}

	// set the constraints implied by the mode
	if (sdo_mode == SDO_Startd_Avail && ! compactMode) {
		// -avail shows unclaimed slots
		sprintf (buffer, "%s == \"%s\" && Cpus > 0", ATTR_STATE, state_to_string(unclaimed_state));
		if (diagnose) { printf ("Adding OR constraint [%s]\n", buffer); }
		query->addORConstraint (buffer);
	}
	else if (sdo_mode == SDO_Startd_Claimed && ! compactMode) {
		// -claimed shows claimed slots
		sprintf (buffer, "%s == \"%s\"", ATTR_STATE, state_to_string(claimed_state));
		if (diagnose) { printf ("Adding OR constraint [%s]\n", buffer); }
		query->addORConstraint (buffer);
	}
	else if (sdo_mode == SDO_Startd_Cod) {
		// -run shows claimed slots
		sprintf (buffer, ATTR_NUM_COD_CLAIMS " > 0");
		if (diagnose) { printf ("Adding OR constraint [%s]\n", buffer); }
		query->addORConstraint (buffer);
	}

	if(javaMode) {
		sprintf( buffer, "%s == TRUE", ATTR_HAS_JAVA );
		if (diagnose) {
			printf ("Adding constraint [%s]\n", buffer);
		}
		query->addANDConstraint (buffer);

		projList.insert(ATTR_HAS_JAVA);
		projList.insert(ATTR_JAVA_MFLOPS);
		projList.insert(ATTR_JAVA_VENDOR);
		projList.insert(ATTR_JAVA_VERSION);
	}

	if(offlineMode) {
		query->addANDConstraint( "size( OfflineUniverses ) != 0" );
		projList.insert( "OfflineUniverses" );
	}

	if(absentMode) {
	    sprintf( buffer, "%s == TRUE", ATTR_ABSENT );
	    if (diagnose) {
	        printf( "Adding constraint %s\n", buffer );
	    }
	    query->addANDConstraint( buffer );

	    projList.insert( ATTR_ABSENT );
	    projList.insert( ATTR_LAST_HEARD_FROM );
	    projList.insert( ATTR_CLASSAD_LIFETIME );
	}

	if(vmMode) {
		sprintf( buffer, "%s == TRUE", ATTR_HAS_VM);
		if (diagnose) {
			printf ("Adding constraint [%s]\n", buffer);
		}
		query->addANDConstraint (buffer);

		projList.insert(ATTR_VM_TYPE);
		projList.insert(ATTR_VM_MEMORY);
		projList.insert(ATTR_VM_NETWORKING);
		projList.insert(ATTR_VM_NETWORKING_TYPES);
		projList.insert(ATTR_VM_HARDWARE_VT);
		projList.insert(ATTR_VM_AVAIL_NUM);
		projList.insert(ATTR_VM_ALL_GUEST_MACS);
		projList.insert(ATTR_VM_ALL_GUEST_IPS);
		projList.insert(ATTR_VM_GUEST_MAC);
		projList.insert(ATTR_VM_GUEST_IP);
	}

	if (compactMode && ! (vmMode || javaMode)) {
		if (sdo_mode == SDO_Startd_Avail) {
			// State==Unclaimed picks up partitionable and unclaimed static, Cpus > 0 picks up only partitionable that have free memory
			sprintf(buffer, "State == \"%s\" && Cpus > 0 && Memory > 0", state_to_string(unclaimed_state));
		} else if (sdo_mode == SDO_Startd_Claimed) {
			// State==Claimed picks up static slots, NumDynamicSlots picks up partitionable slots that are partly claimed.
			sprintf(buffer, "(State == \"%s\" && DynamicSlot =!= true) || (NumDynamicSlots isnt undefined && NumDynamicSlots > 0)", state_to_string(claimed_state));
		} else {
			sprintf(buffer, "PartitionableSlot =?= true || DynamicSlot =!= true");
		}
		if (diagnose) {
			printf ("Adding constraint [%s]\n", buffer);
		}
		query->addANDConstraint (buffer);
		projList.insert(ATTR_ARCH);
		projList.insert(ATTR_OPSYS_AND_VER);
		projList.insert(ATTR_OPSYS_NAME);
		projList.insert(ATTR_SLOT_DYNAMIC);
		projList.insert(ATTR_SLOT_PARTITIONABLE);
		projList.insert(ATTR_STATE);
		projList.insert(ATTR_ACTIVITY);
		//projList.insert(ATTR_MACHINE_RESOURCES);
		//projList.insert(ATTR_WITHIN_RESOURCE_LIMITS); // this will force all partitionable resource values to be fetched.
		projList.insert("ChildState"); // this is needed to do the summary rollup
		projList.insert("ChildActivity"); // this is needed to do the summary rollup
		//pmHeadFoot = (printmask_headerfooter_t)(pmHeadFoot | HF_NOSUMMARY);
	}

	// second pass:  add regular parameters and constraints
	if (diagnose) {
		printf ("----------\n");
	}

	secondPass (argc, argv);

	if (sortSpecs.empty() && ! noSort) {
		// set a default sort of Machine/Name
		sortSpecs.Add(ATTR_MACHINE);
		sortSpecs.Add(ATTR_NAME);
	}
	if (compactMode && !mergeMode) {
		// compact mode reqires machine to be the primary sort key,
		// but merge mode requires that we respect the sort key.
		// FIXME: We should be using a merge key for merging, probably,
		// anyway, maybe even just as the argument to -merge.
		sortSpecs.ForcePrimaryKey(ATTR_MACHINE);
	}
	sortSpecs.AddToProjection(projList);

	// initialize the totals object
	if (mainPP.ppStyle == PP_CUSTOM && mainPP.using_print_format) {
		if (mainPP.pmHeadFoot & HF_NOSUMMARY) mainPP.ppTotalStyle = PP_CUSTOM;
	} else {
		mainPP.ppTotalStyle = mainPP.ppStyle;
	}
	TrackTotals	rightTotals(mainPP.ppTotalStyle);

	// In merge mode, the 'both' ads are printed normally.
	TrackTotals bothTotals(mainPP.ppTotalStyle);
	TrackTotals leftTotals(mainPP.ppTotalStyle);

	// in order to totals, the projection MUST have certain attributes
	if (mainPP.wantOnlyTotals || ((mainPP.ppTotalStyle != PP_CUSTOM) && ! projList.empty())) {
		switch (mainPP.ppTotalStyle) {
			case PP_STARTD_SERVER:
				projList.insert(ATTR_MEMORY);
				projList.insert(ATTR_DISK);
				// fall through
			case PP_STARTD_RUN:
				projList.insert(ATTR_LOAD_AVG);
				projList.insert(ATTR_MIPS);
				projList.insert(ATTR_KFLOPS);
				// fall through
			case PP_STARTD_NORMAL:
			case PP_STARTD_COD:
				if (mainPP.ppTotalStyle == PP_STARTD_COD) {
					projList.insert(ATTR_CLAIM_STATE);
					projList.insert(ATTR_COD_CLAIMS);
				}
				projList.insert(ATTR_STATE); // Norm, state, server
				projList.insert(ATTR_ARCH);  // for key
				projList.insert(ATTR_OPSYS); // for key
				break;

			case PP_STARTD_STATE:
				projList.insert(ATTR_STATE);
				projList.insert(ATTR_ACTIVITY); // for key
				break;

			case PP_SUBMITTER_NORMAL:
				projList.insert(ATTR_NAME); // for key
				projList.insert(ATTR_RUNNING_JOBS);
				projList.insert(ATTR_IDLE_JOBS);
				projList.insert(ATTR_HELD_JOBS);
				break;

			case PP_SCHEDD_NORMAL: // no key
				projList.insert(ATTR_TOTAL_RUNNING_JOBS);
				projList.insert(ATTR_TOTAL_IDLE_JOBS);
				projList.insert(ATTR_TOTAL_HELD_JOBS);
				break;

			case PP_CKPT_SRVR_NORMAL:
				projList.insert(ATTR_DISK);
				break;

			default:
				break;
		}
	}

	// fetch the query
	QueryResult q;

	if (PP_IS_LONGish(mainPP.ppStyle)) {
		// Remove everything from the projection list if we're displaying
		// the "long form" of the ads.
		projList.clear();
		mainPP.using_print_format = false;
		mainPP.pm.clearFormats();

		// but if -attributes was supplied, show only those attributes
		if ( ! dashAttributes.isEmpty()) {
			const char * s;
			dashAttributes.rewind();
			while ((s = dashAttributes.next())) {
				projList.insert(s);
			}
		}
		mainPP.pmHeadFoot = HF_BARE;
	}

	// Setup the pretty printer for the given mode.
	if ( ! PP_IS_LONGish(mainPP.ppStyle) && mainPP.ppStyle != PP_CUSTOM && ! explicit_format) {
		const char * constr = NULL;
		mainPP.prettyPrintInitMask(projList, constr, disable_user_print_files);
		if (constr) { query->addANDConstraint(constr); }
	}

	// if diagnose was requested, just print the query ad
	if (diagnose) {

		FILE* fout = stderr;
		if (disable_user_print_files) {
			std::string temp;
			fprintf(fout, "# for cmd:");
			for (int ii = 0; ii < argc; ++ii) {
				const char * pcolon;
				if (is_dash_arg_colon_prefix(argv[ii], "diagnose", &pcolon, 3)) continue;
				if (is_dash_arg_colon_prefix(argv[ii], "print-format", &pcolon, 2)) { ++ii; continue; }
				fprintf(fout, " %s", argv[ii]);
			}
			fprintf(fout, "\n# %s\n", paramNameFromPPMode(temp));

			PrintMaskMakeSettings pmms;
			//pmms.aggregate =
			const char *adtypeName = mainPP.adtypeNameFromPPMode();
			if (adtypeName) { pmms.select_from = mainPP.adtypeNameFromPPMode(); }
			pmms.headfoot = mainPP.pmHeadFoot;
			List<const char> * pheadings = NULL;
			if ( ! mainPP.pm.has_headings()) {
				if (mainPP.pm_head.Length() > 0) pheadings = &mainPP.pm_head;
			}
			MyString requirements;
			if (Q_OK == query->getRequirements(requirements) && ! requirements.empty()) {
				ConstraintHolder constrRaw(requirements.StrDup());
				ExprTree * tree = constrRaw.Expr();
				ConstraintHolder constrReduced(SkipExprParens(tree)->Copy());
				pmms.where_expression = constrReduced.c_str();
			}
			const CustomFormatFnTable * pFnTable = getCondorStatusPrintFormats();

			temp.clear();
			temp.reserve(4096);
			PrintPrintMask(temp, *pFnTable, mainPP.pm, pheadings, pmms, group_by_keys, NULL);
			fprintf(fout, "%s\n", temp.c_str());
			//exit (1);
		}

		fprintf(fout, "diagnose: ");
		for (int ii = 0; ii < argc; ++ii) { fprintf(fout, "%s ", argv[ii]); }
		fprintf(fout, "\n----------\n");

		// print diagnostic information about inferred internal state
		mainPP.dumpPPMode(fout);
		fprintf(fout, "Totals: %s\n", getPPStyleStr(mainPP.ppTotalStyle));
		fprintf(fout, "Opts: HF=%x\n", mainPP.pmHeadFoot);

		std::string style_text;
		style_text.reserve(8000);
		style_text = "";

		sortSpecs.dump(style_text, " ] [ ");
		fprintf(fout, "Sort: [ %s<ord> ]\n", style_text.c_str());

		style_text = "";
		List<const char> * pheadings = NULL;
		if ( ! mainPP.pm.has_headings()) {
			if (mainPP.pm_head.Length() > 0) pheadings = &mainPP.pm_head;
		}
		mainPP.pm.dump(style_text, getCondorStatusPrintFormats(), pheadings);
		fprintf(fout, "\nPrintMask:\n%s\n", style_text.c_str());

		ClassAd queryAd;
		q = query->getQueryAd (queryAd);
		fPrintAd (fout, queryAd);

		// print projection
		if (projList.empty()) {
			fprintf(fout, "Projection: <NULL>\n");
		} else {
			const char * prefix = "\n  ";
			std::string buf(prefix);
			fprintf(fout, "Projection:%s\n", print_attrs(buf, true, projList, prefix));
		}

		fprintf (fout, "\n\n");
		fprintf (stdout, "Result of making query ad was:  %d\n", q);
		if ( ! diagnostics_ads_file) exit (1);
	}

	if ( ! projList.empty()) {
		#if 0 // for debugging
		std::string attrs;
		fprintf(stdout, "Projection is [%s]\n", print_attrs(attrs, false, projList, " "));
		#endif
		query->setDesiredAttrs(projList);
	}

	// Address (host:port) is taken from requested pool, if given.
	const char* addr = (NULL != pool) ? pool->addr() : NULL;
	Daemon* requested_daemon = pool;

	// If we're in "direct" mode, then we attempt to locate the daemon
	// associated with the requested subsystem (here encoded by value of mode)
	// In this case the host:port of pool (if given) denotes which
	// pool is being consulted
	if( direct ) {
		Daemon *d = NULL;
		switch (adType) {
		case MASTER_AD: d = new Daemon( DT_MASTER, direct, addr ); break;
		case STARTD_AD: d = new Daemon( DT_STARTD, direct, addr ); break;
		case SCHEDD_AD:
		case SUBMITTOR_AD: d = new Daemon( DT_SCHEDD, direct, addr ); break;
		case NEGOTIATOR_AD:
		case ACCOUNTING_AD: d = new Daemon( DT_NEGOTIATOR, direct, addr ); break;
		default: // These have to go to the collector, there is no 'direct'
			break;
		}

		// Here is where we actually override 'addr', if we can obtain
		// address of the requested daemon/subsys.  If it can't be
		// located, then fail with error msg.
		// 'd' will be null (unset) if mode is one of above that must go to
		// collector (MODE_ANY_NORMAL, MODE_COLLECTOR_NORMAL, etc)
		if (NULL != d) {
			if( d->locate() ) {
				addr = d->addr();
				requested_daemon = d;
			} else {
				const char* id = d->idStr();
				if (NULL == id) id = d->name();
				dprintf_WriteOnErrorBuffer(stderr, true);
				if (NULL == id) id = "daemon";
				fprintf(stderr, "Error: Failed to locate %s\n", id);
				fprintf(stderr, "%s\n", d->error());
				exit( 1 );
			}
		}
	}

	if (dash_group_by) {
		if ( ! dashAttributes.isEmpty()) {
			ad_groups.setSigAttrs(dashAttributes.print_to_string(), true, true);
		} else {
			ad_groups.setSigAttrs("Cpus Memory GPUs IOHeavy START", false, true);
		}
		ad_groups.keepAdKeys(get_ad_name_string);
	}

	// This awful construction is forced on us by our List class
	// refusing to allow copying or assignment.
	PrettyPrinter * lpp = & mainPP;
	PrettyPrinter * bpp = & mainPP;
	PrettyPrinter * rpp = & mainPP;

	PrettyPrinter leftPP( PP_CUSTOM, PP_NOTSET, HF_NOSUMMARY );
	PrettyPrinter rightPP( PP_CUSTOM, PP_NOTSET, HF_NOSUMMARY );
	if( mergeMode && annexMode ) {
		const char * constraint;
		leftPP.ppSetAnnexInstanceCols( -1, constraint );
		rightPP.ppSetAnnexInstanceCols( -1, constraint );

		rpp = & rightPP;
		lpp = & leftPP;
	}

	CondorError errstack;
	ROD_MAP_BY_KEY right;
	struct _process_ads_info right_ai = {
		& right,
		(rpp->pmHeadFoot&HF_NOSUMMARY) ? NULL : & rightTotals,
		1, rpp->pm.ColCount(), NULL, 0, rpp
	};

	bool close_hfdiag = false;
	if (diagnose) {
		right_ai.diag_flags = 1 | 2;
		if (diagnostics_ads_file && diagnostics_ads_file[0] != '-') {
			right_ai.hfDiag = safe_fopen_wrapper_follow(diagnostics_ads_file, "w");
			if ( ! right_ai.hfDiag) {
				fprintf(stderr, "\nERROR: Failed to open file -diag output file (%s)\n", strerror(errno));
			}
			close_hfdiag = true;
		} else {
			right_ai.hfDiag = stdout;
			if (diagnostics_ads_file && diagnostics_ads_file[0] == '-' && diagnostics_ads_file[1] == '2') right_ai.hfDiag = stderr;
			close_hfdiag = false;
		}
		if ( ! right_ai.hfDiag) { exit(2); }
	}


	//
	// Read the left set of ads.
	//
	int leftLimit = -1;
	const char * leftConstraint = NULL;
	std::set< ClassAd * > leftSet;
	ClassAdFileParseType::ParseType leftParseType = ClassAdFileParseType::Parse_auto;
	if( mergeMode ) {
		read_classad_file( leftFileName, leftParseType, store_ads_callback,
			& leftSet, leftConstraint, leftLimit );
	}

	// Output storage.
	ROD_MAP_BY_KEY left;
	struct _process_ads_info left_ai = {
		& left,
		(lpp->pmHeadFoot&HF_NOSUMMARY) ? NULL : & leftTotals,
		1, lpp->pm.ColCount(), NULL, 0, lpp
	};

	ROD_MAP_BY_KEY both;
	struct _process_ads_info both_ai = {
		& both,
		(bpp->pmHeadFoot&HF_NOSUMMARY) ? NULL : & bothTotals,
		1, bpp->pm.ColCount(), NULL, 0, bpp
	};

	// Argument for the merge callback;
	merge_ads_info mai = { & leftSet, & both_ai, & right_ai };

	//
	// Read the right set of ads and do the merge in the same pass.
	//
	void * rightArg = & mai;
	FNPROCESS_ADS_CALLBACK rightCallback = merge_ads_callback;

	if( rightFileName != NULL ) {
		MyString req;
		q = query->getRequirements(req);
		const char * constraint = req.empty() ? NULL : req.c_str();
		if( read_classad_file( rightFileName, rightFileFormat, rightCallback, rightArg, constraint, result_limit ) ) {
			q = Q_OK;
		}
	} else if (NULL != addr) {
			// this case executes if pool was provided, or if in "direct" mode with
			// subsystem that corresponds to a daemon (above).
			// Here 'addr' represents either the host:port of requested pool, or
			// alternatively the host:port of daemon associated with requested subsystem (direct mode)
		q = query->processAds( rightCallback, rightArg, addr, & errstack );
	} else {
			// otherwise obtain list of collectors and submit query that way
		CollectorList * collectors = CollectorList::create();
		q = collectors->query( * query, rightCallback, rightArg, & errstack );
		delete collectors;
	}

	for( auto i = leftSet.begin(); i != leftSet.end(); ++i ) {
		if( marked( * i ) ) {
			// The ad is already in both if it was marked.
		} else {
			process_ads_callback( & left_ai, * i );
		}
	}

	if (diagnose) {
		if (close_hfdiag) {
			fclose(right_ai.hfDiag);
			right_ai.hfDiag = NULL;
		}
		exit(1);
	}


	// if any error was encountered during the query, report it and exit 
	if (Q_OK != q) {

		dprintf_WriteOnErrorBuffer(stderr, true);
			// we can always provide these messages:
		fprintf( stderr, "Error: %s\n", getStrQueryResult(q) );
		fprintf( stderr, "%s\n", errstack.getFullText(true).c_str() );

		if ((NULL != requested_daemon) && ((Q_NO_COLLECTOR_HOST == q) ||
			(requested_daemon->type() == DT_COLLECTOR)))
		{
				// Specific long message if connection to collector failed.
			const char* fullhost = requested_daemon->fullHostname();
			if (NULL == fullhost) fullhost = "<unknown_host>";
			const char* daddr = requested_daemon->addr();
			if (NULL == daddr) daddr = "<unknown>";
			char info[1000];
			sprintf(info, "%s (%s)", fullhost, daddr);
			printNoCollectorContact( stderr, info, !expert );
		} else if ((NULL != requested_daemon) && (Q_COMMUNICATION_ERROR == q)) {
				// more helpful message for failure to connect to some daemon/subsys
			const char* id = requested_daemon->idStr();
			if (NULL == id) id = requested_daemon->name();
			if (NULL == id) id = "daemon";
			const char* daddr = requested_daemon->addr();
			if (NULL == daddr) daddr = "<unknown>";
			fprintf(stderr, "Error: Failed to contact %s at %s\n", id, daddr);
		}

		// fail
		exit (1);
	}

	if(! mergeMode) {
		doNormalOutput( right_ai, adType );
	// This conditional stolen from PrettyPrinter::prettyPrintHeadings().
	} else if( (!mainPP.using_print_format) && mainPP.ppStyle == PP_JSON ) {
			fputs( "{\n\"query-only\": ", stdout );
			if( right_ai.pmap->size() > 0 ) {
				doMergeOutput( right_ai );
			} else {
				fputs( "[ ]", stdout );
			}
			fputs( ",\n\"both\": ", stdout );
			if( both_ai.pmap->size() > 0 ) {
				doNormalOutput( both_ai, adType );
			} else {
				fputs( "[ ]", stdout );
			}
			fputs( ",\n\"file-only\": ", stdout );
			if( left_ai.pmap->size() > 0 ) {
				doMergeOutput( left_ai );
			} else {
				fputs( "[ ]", stdout );
			}
			fputs( "\n}\n", stdout );
	} else {
		if( right_ai.pmap->size() > 0 ) {
			if(! annexMode) { fprintf( stdout, "The following ads were found only in the query:\n" ); }
			doMergeOutput( right_ai );
			if( both_ai.pmap->size() > 0 ) {
				fprintf( stdout, "\n" );
			}
		}

		if( both_ai.pmap->size() > 0 ) {
			if(! annexMode) { fprintf( stdout, "The following ads were found in both the query and in '%s':\n", leftFileName ); }
			doNormalOutput( both_ai, adType );
			if( left_ai.pmap->size() > 0 ) {
				fprintf( stdout, "\n" );
			}
		}

		if( left_ai.pmap->size() > 0 ) {
			if(! annexMode) { fprintf( stdout, "The following ads were found only in '%s':\n", leftFileName ); }
			doMergeOutput( left_ai );
		}
	}

	if (dash_group_by) {
		std::string output;
		output.reserve(16372);
		StringList * whitelist = NULL;

		AdAggregationResults<std::string> groups(ad_groups);
		groups.rewind();
		ClassAd * ad;
		while ((ad = groups.next()) != NULL) {
			output.clear();
			classad::References attrs;
			sGetAdAttrs(attrs, *ad, false, whitelist);
			sPrintAdAttrs(output, *ad, attrs);
			output += "\n";
			fputs(output.c_str(), stdout);
		}
	}

	delete query;
	return 0;
}

void doMergeOutput( struct _process_ads_info & ai ) {
	ROD_MAP_BY_KEY & admap = * ai.pmap;
	ASSERT( ai.pmap != NULL );
	PrettyPrinter & pp = * ai.pp;
	ASSERT( ai.pp != NULL );

	bool any_ads = ! admap.empty();
	ppOption pps = pp.prettyPrintHeadings( any_ads );

	bool is_piped = false;
	int display_width = pp.getDisplayWidth( & is_piped );
	std::string line;
	line.reserve( is_piped ? 1024 : display_width );

	// for XML output, print the xml header even if there are no ads.
	if (PP_XML == pps) {
		line.clear();
		AddClassAdXMLFileHeader(line);
		fputs(line.c_str(), stdout); // xml string already ends in a newline.
	}

	int output_index = 0;
	for( auto it = admap.begin(); it != admap.end(); ++it ) {
		if( ai.columns ) {
			line.clear();
			pp.pm.display( line, it->second.rov );
			fputs( line.c_str(), stdout );
		} else {
			StringList * whitelist = dashAttributes.isEmpty() ? NULL : & dashAttributes;
			pp.prettyPrintAd( pps, it->second.ad, output_index, whitelist, print_attrs_in_hash_order );
			if (it->second.ad) ++output_index;
		}
		it->second.flags |= SROD_PRINTED; // for debugging, keep track of what we already printed.
	}

	// for XML output, print the xml footer even if there are no ads.
	if (PP_XML == pps) {
		line.clear();
		AddClassAdXMLFileFooter(line);
		fputs(line.c_str(), stdout);
		// PRAGMA_REMIND("tj: XML output used to have an extra trailing newline, do we need to preserve that?")
	} else if (output_index > 0 && (PP_JSON == pps || PP_NEWCLASSAD == pps)) {
		// if we wrote any ads IN JSON or new classad format, then we need a closing list operator
		line = (PP_JSON == pps) ? "]\n" : "}\n";
		fputs(line.c_str(), stdout);
	}

	TrackTotals * totals = ai.totals;
	if (any_ads && !(pp.pmHeadFoot&HF_NOSUMMARY) && totals && totals->haveTotals()) {
		fputc('\n', stdout);
		bool auto_width = (pp.ppTotalStyle == PP_SUBMITTER_NORMAL);
		int totals_key_width = (pp.wide_display || auto_width) ? -1 : MAX(14, pp.max_totals_subkey);
		totals->displayTotals(stdout, totals_key_width);
	}
}

void
doNormalOutput( struct _process_ads_info & ai, AdTypes & adType ) {
	TrackTotals * totals = ai.totals;
	ROD_MAP_BY_KEY & admap = * ai.pmap;

	if (compactMode) {
		switch (adType) {
		case STARTD_AD: {
			if( mainPP.wantOnlyTotals ) {
				fprintf(stderr, "Warning: ignoring -compact option because -total option is also set\n");
			} else {
				reduce_slot_results(admap, mainPP);
			}
		} break;
		default: break;
		}
	}

	bool any_ads = ! admap.empty();
	ppOption pps = mainPP.prettyPrintHeadings( any_ads );

	bool is_piped = false;
	int display_width = mainPP.getDisplayWidth(&is_piped);
	std::string line; line.reserve(is_piped ? 1024 : display_width);

	// for XML output, print the xml header even if there are no ads.
	if (PP_XML == pps) {
		line.clear();
		AddClassAdXMLFileHeader(line);
		fputs(line.c_str(), stdout); // xml string already ends in a newline.
	}

	int output_index = 0;
	for (ROD_MAP_BY_KEY::iterator it = admap.begin(); it != admap.end(); ++it) {
		if (it->second.flags & (SROD_FOLDED | SROD_SKIP))
			continue;
		if (ai.columns) {
			line.clear();
			mainPP.pm.display(line, it->second.rov);
			fputs(line.c_str(), stdout);
		} else {
			StringList * whitelist = dashAttributes.isEmpty() ? NULL : &dashAttributes;
			mainPP.prettyPrintAd(pps, it->second.ad, output_index, whitelist, print_attrs_in_hash_order);
			if (it->second.ad) ++output_index;
		}
		it->second.flags |= SROD_PRINTED; // for debugging, keep track of what we already printed.
	}

	// for XML output, print the xml footer even if there are no ads.
	if (PP_XML == pps) {
		line.clear();
		AddClassAdXMLFileFooter(line);
		fputs(line.c_str(), stdout);
		// PRAGMA_REMIND("tj: XML output used to have an extra trailing newline, do we need to preserve that?")
	} else if (output_index > 0 && (PP_JSON == pps || PP_NEWCLASSAD == pps)) {
		// if we wrote any ads IN JSON or new classad format, then we need a closing list operator
		line = (PP_JSON == pps) ? "]\n" : "}\n";
		fputs(line.c_str(), stdout);
	}

	// if totals are required, display totals
	if (any_ads && !(mainPP.pmHeadFoot&HF_NOSUMMARY) && totals && totals->haveTotals()) {
		fputc('\n', stdout);
		bool auto_width = (mainPP.ppTotalStyle == PP_SUBMITTER_NORMAL);
		int totals_key_width = (mainPP.wide_display || auto_width) ? -1 : MAX(14, mainPP.max_totals_subkey);
		totals->displayTotals(stdout, totals_key_width);
	}
}


int PrettyPrinter::set_status_print_mask_from_stream (
	const char * streamid,
	bool is_filename,
	const char ** pconstraint)
{
	PrintMaskMakeSettings pmopt;
	std::string messages;

	pmopt.headfoot = pmHeadFoot;

	SimpleInputStream * pstream = NULL;
	*pconstraint = NULL;

	FILE *file = NULL;
	if (MATCH == strcmp("-", streamid)) {
		pstream = new SimpleFileInputStream(stdin, false);
	} else if (is_filename) {
		file = safe_fopen_wrapper_follow(streamid, "r");
		if (file == NULL) {
			fprintf(stderr, "Can't open select file: %s\n", streamid);
			return -1;
		}
		pstream = new SimpleFileInputStream(file, true);
	} else {
		pstream = new StringLiteralInputStream(streamid);
	}
	ASSERT(pstream);

	//PRAGMA_REMIND("tj: fix to handle summary formatting.")
	int err = SetAttrListPrintMaskFromStream(
					*pstream,
					*getCondorStatusPrintFormats(),
					pm,
					pmopt,
					group_by_keys,
					NULL,
					messages);
	delete pstream; pstream = NULL;
	if ( ! err) {
		if (pmopt.aggregate != PR_NO_AGGREGATION) {
			fprintf(stderr, "print-format aggregation not supported\n");
			return -1;
		}

		if ( ! pmopt.where_expression.empty()) {
			*pconstraint = pm.store(pmopt.where_expression.c_str());
		}
		// copy attrs in the global projection
		for (classad::References::const_iterator it = pmopt.attrs.begin(); it != pmopt.attrs.end(); ++it) {
			projList.insert(*it);
		}
	}
	if ( ! messages.empty()) { fprintf(stderr, "%s", messages.c_str()); }
	return err;
}

static bool read_classad_file(const char *filename, ClassAdFileParseType::ParseType ads_file_format, FNPROCESS_ADS_CALLBACK callback, void* pv, const char * constr, int limit)
{
	bool success = false;
	if (ads_file_format < ClassAdFileParseType::Parse_long || ads_file_format > ClassAdFileParseType::Parse_auto) {
		fprintf(stderr, "unsupported ClassAd input format %d\n", ads_file_format);
		return false;
	}

	FILE* file = NULL;
	bool close_file = false;
	if (MATCH == strcmp(filename,"-")) {
		file = stdin;
		close_file = false;
	} else {
		file = safe_fopen_wrapper_follow(filename, "r");
		close_file = true;
	}

	if (file == NULL) {
		fprintf(stderr, "Can't open file of ClassAds: %s\n", filename);
		return false;
	} else {
		ConstraintHolder constraint;
		if (constr) {
			constraint.set(strdup(constr));
			int err = 0;
			constraint.Expr(&err);
			if (err) {
				fprintf(stderr, "Invalid constraint expression : %s\n", constr);
				return false;
			}
		}

		CondorClassAdFileIterator adIter;
		if ( ! adIter.begin(file, close_file, ads_file_format)) {
			fprintf(stderr, "Unexpected error reading file of ClassAds: %s\n", filename);
			if (close_file) { fclose(file); file = NULL; }
			return false;
		} else {
			int index = 0;
			if (limit <= 0) limit = INT_MAX;
			success = true;
			ClassAd * classad;
			while ((classad = adIter.next(constraint.Expr()))) {
				if (callback(pv, classad)) {
					delete classad; // delete unless the callback took ownership.
				}
				if (++index >= limit) break;
			}
		}
		file = NULL;
	}
	return success;
}


void
usage ()
{
	fprintf (stderr,"Usage: %s [help-opt] [query-opt] [custom-opts] [display-opts] [name ...]\n", myName);

	fprintf (stderr,"    where [help-opt] is one of\n"
		"\t-help\t\t\tPrint this screen and exit\n"
		"\t-version\t\tPrint HTCondor version and exit\n"
		"\t-diagnose\t\tPrint out query ad without performing query\n"
		);

	fprintf (stderr,"\n    and [query-opt] is one of\n"
		"\t-absent\t\t\tPrint information about absent resources\n"
		"\t-annex-name <name>\tPrint information about the named annex's slots\n"
		"\t-annex-slots\t\tPrint information about any annex's slots\n"
		"\t-avail\t\t\tPrint information about available resources\n"
		"\t-ckptsrvr\t\tDisplay checkpoint server attributes\n"
		"\t-claimed\t\tPrint information about claimed resources\n"
		"\t-cod\t\t\tDisplay Computing On Demand (COD) jobs\n"
		"\t-collector\t\tDisplay collector daemon attributes\n"
		"\t-data\t\t\tDisplay data transfer information\n"
		"\t-debug\t\t\tDisplay debugging info to console\n"
		"\t-defrag\t\t\tDisplay status of defrag daemon\n"
		"\t-direct <host>\t\tGet attributes directly from the given daemon\n"
		"\t-java\t\t\tDisplay Java-capable hosts\n"
		"\t-vm\t\t\tDisplay VM-capable hosts\n"
		"\t-license\t\tDisplay attributes of licenses\n"
		"\t-master\t\t\tDisplay daemon master attributes\n"
		"\t-pool <name>\t\tGet information from collector <name>\n"
		"\t-ads[:<form>] <file>\tGet information from <file> in <form> format.\n"
		"\t           where <form> choice is one of:\n"
		"\t       long    The traditional -long form. This is the default\n"
		"\t       xml     XML form, the same as -xml\n"
		"\t       json    JSON classad form, the same as -json\n"
		"\t       new     'new' classad form without newlines\n"
		"\t       auto    Guess the format from reading the input stream\n"
		"\t-grid\t\t\tDisplay grid resources\n"
		"\t-run\t\t\tDisplay running job stats\n"
		"\t-schedd\t\t\tDisplay attributes of schedds\n"
		"\t-server\t\t\tDisplay important attributes of resources\n"
		"\t-startd\t\t\tDisplay resource attributes\n"
		"\t-generic\t\tDisplay attributes of 'generic' ads\n"
		"\t-subsystem <type>\tDisplay classads of the given type\n"
		"\t-negotiator\t\tDisplay negotiator attributes\n"
		"\t-storage\t\tDisplay network storage resources\n"
		"\t-any\t\t\tDisplay any resources\n"
		"\t-state\t\t\tDisplay state of resources\n"
		"\t-submitters\t\tDisplay information about request submitters\n"
//		"\t-world\t\t\tDisplay all pools reporting to UW collector\n"
		);

	fprintf (stderr, "\n    and [custom-opts ...] are one or more of\n"
		"\t-constraint <const>\tAdd constraint on classads\n"
		"\t-compact\t\tShow compact form, rolling up slots into a single line\n"
		"\t-statistics <set>:<n>\tDisplay statistics for <set> at level <n>\n"
		"\t\t\t\tsee STATISTICS_TO_PUBLISH for valid <set> and level values\n"
		"\t\t\t\tuse with -direct queries to STARTD and SCHEDD daemons\n"
		"\t-target <file>\t\tUse target classad with -format or -af evaluation\n"
		"\n    and [display-opts] are one or more of\n"
		"\t-long[:<form>]\t\tDisplay entire classads in format <form>. See -ads\n"
		"\t-limit <n>\t\tDisplay no more than <n> classads.\n"
		"\t-sort <expr>\t\tSort ClassAds by expressions. 'no' disables sorting\n"
		"\t-natural[:off]\t\tUse natural sort order in default output (default=on)\n"
		"\t-total\t\t\tDisplay totals only\n"
		"\t-expert\t\t\tDisplay shorter error messages\n"
		"\t-wide[:<width>]\t\tDon't truncate data to fit in 80 columns.\n"
		"\t\t\t\tTruncates to console width or <width> argument if specified.\n"
		"\t-xml\t\t\tDisplay entire classads, but in XML\n"
		"\t-json\t\t\tDisplay entire classads, but in JSON\n"
		"\t-attributes X,Y,...\tAttributes to show in -xml or -long \n"
		"\t-format <fmt> <attr>\tDisplay <attr> values with formatting\n"
		"\t-autoformat[:lhVr,tng] <attr> [<attr2> [...]]\n"
		"\t-af[:lhVr,tng] <attr> [attr2 [...]]\n"
		"\t    Print attr(s) with automatic formatting\n"
		"\t    the [lhVr,tng] options modify the formatting\n"
		//"\t        j   display Job id\n"
		"\t        l   attribute labels\n"
		"\t        h   attribute column headings\n"
		"\t        V   %%V formatting (string values are quoted)\n"
		"\t        r   %%r formatting (raw/unparsed values)\n"
		"\t        ,   comma after each value\n"
		"\t        t   tab before each value (default is space)\n"
		"\t        n   newline after each value\n"
		"\t        g   newline between ClassAds, no space before values\n"
		"\t    use -af:h to get tabular values with headings\n"
		"\t    use -af:lrng to get -long equivalent format\n"
		"\t-merge <file>\t\tCompare ads in file and query by sort key\n"
		"\t-print-format <file>\tUse <file> to set display attributes and formatting\n"
		"\t\t\t\t(experimental, see htcondor-wiki for more information)\n"
		);
}


void
firstPass (int argc, char *argv[])
{
	int had_pool_error = 0;
	int had_direct_error = 0;
	int had_statistics_error = 0;
	//bool explicit_mode = false;
	const char * pcolon = NULL;

	// Process arguments:  there are dependencies between them
	// o -l/v and -serv are mutually exclusive
	// o -sub, -avail and -run are mutually exclusive
	// o -pool and -entity may be used at most once
	// o since -c can be processed only after the query has been instantiated,
	//   constraints are added on the second pass
	for (int i = 1; i < argc; i++) {
		if (is_dash_arg_prefix (argv[i], "avail", 2)) {
			mainPP.setMode (SDO_Startd_Avail, i, argv[i]);
		} else
		if (is_dash_arg_prefix (argv[i], "pool", 1)) {
			if( pool ) {
				delete pool;
				had_pool_error = 1;
			}
			i++;
			if( ! argv[i] ) {
				fprintf( stderr, "%s: -pool requires a hostname as an argument.\n",
						 myName );
				if (!expert) {
					printf("\n");
					print_wrapped_text("Extra Info: The hostname should be the central "
									   "manager of the Condor pool you wish to work with.",
									   stderr);
					printf("\n");
				}
				fprintf( stderr, "Use \"%s -help\" for details\n", myName );
				exit( 1 );
			}
			pool = new DCCollector( argv[i] );
			if( !pool->addr() ) {
				dprintf_WriteOnErrorBuffer(stderr, true);
				fprintf( stderr, "Error: %s\n", pool->error() );
				if (!expert) {
					printf("\n");
					print_wrapped_text("Extra Info: You specified a hostname for a pool "
									   "(the -pool argument). That should be the Internet "
									   "host name for the central manager of the pool, "
									   "but it does not seem to "
									   "be a valid hostname. (The DNS lookup failed.)",
									   stderr);
				}
				exit( 1 );
			}
		} else
		if (is_dash_arg_colon_prefix (argv[i], "ads", &pcolon, 2)) {
			if( !argv[i+1] ) {
				fprintf( stderr, "%s: -ads requires a filename argument\n",
						 myName );
				fprintf( stderr, "Use \"%s -help\" for details\n", myName );
				exit( 1 );
			}
			i += 1;
			rightFileName = argv[i];
			if (pcolon) {
				rightFileFormat = parseAdsFileFormat(++pcolon, ClassAdFileParseType::Parse_long);
			}
		} else
		if (is_dash_arg_prefix (argv[i], "format", 1)) {
			mainPP.setPPstyle (PP_CUSTOM, i, argv[i]);
			if( !argv[i+1] || !argv[i+2] ) {
				fprintf( stderr, "%s: -format requires two other arguments\n",
						 myName );
				fprintf( stderr, "Use \"%s -help\" for details\n", myName );
				exit( 1 );
			}
			i += 2;
			mainPP.pmHeadFoot = HF_BARE;
			explicit_format = true;
		} else
		if (*argv[i] == '-' &&
			(is_arg_colon_prefix(argv[i]+1, "autoformat", &pcolon, 5) || 
			 is_arg_colon_prefix(argv[i]+1, "af", &pcolon, 2)) ) {
				// make sure we have at least one more argument
			if ( !argv[i+1] || *(argv[i+1]) == '-') {
				fprintf( stderr, "Error: Argument %s requires "
						 "at last one attribute parameter\n", argv[i] );
				fprintf( stderr, "Use \"%s -help\" for details\n", myName );
				exit( 1 );
			}
			mainPP.pmHeadFoot = HF_NOSUMMARY;
			explicit_format = true;
			mainPP.setPPstyle (PP_CUSTOM, i, argv[i]);
			while (argv[i+1] && *(argv[i+1]) != '-') {
				++i;
			}
			// if autoformat list ends in a '-' without any characters after it, just eat the arg and keep going.
			MSC_SUPPRESS_WARNING(6011) // code analysis can't figure out that argc test protects us from de-refing a NULL in argv
			if (i+1 < argc && '-' == (argv[i+1])[0] && 0 == (argv[i+1])[1]) {
				++i;
			}
		} else
		if (is_dash_arg_colon_prefix(argv[i], "print-format", &pcolon, 2)) {
			if ( (i+1 >= argc)  || (*(argv[i+1]) == '-' && (argv[i+1])[1] != 0)) {
				fprintf( stderr, "Error: Argument -print-format requires a filename argument\n");
				exit( 1 );
			}
			// allow -pr ! to disable use of user-default print format files.
			if (MATCH == strcmp(argv[i+1], "!")) {
				disable_user_print_files = true;
			} else {
				explicit_format = true;
				mainPP.setPPstyle (PP_CUSTOM, i, argv[i]);
			}
			++i; // eat the next argument.
			// we can't fully parse the print format argument until the second pass, so we are done for now.
		} else
		if (is_dash_arg_colon_prefix (argv[i], "wide", &pcolon, 3)) {
			mainPP.wide_display = true; // when true, don't truncate field data
			if (pcolon) {
				mainPP.forced_display_width = atoi(++pcolon);
				if (mainPP.forced_display_width <= 100) mainPP.wide_display = false;
				mainPP.setPPwidth();
			}
		} else
		if (is_dash_arg_colon_prefix (argv[i], "natural", &pcolon, 3)) {
			naturalSort = true;
			if (pcolon) {
				if (MATCH == strcmp(++pcolon,"off")) {
					naturalSort = false;
				}
			}
		} else
		if (is_dash_arg_prefix (argv[i], "target", 4)) {
			if( !argv[i+1] ) {
				fprintf( stderr, "%s: -target requires one additional argument\n",
						 myName );
				fprintf( stderr, "Use \"%s -help\" for details\n", myName );
				exit( 1 );
			}
			i += 1;
			target = argv[i];
			FILE *targetFile = safe_fopen_wrapper_follow(target, "r");
			if (targetFile == NULL) {
				fprintf(stderr, "Cannot open file %s: errno: %d\n", target, errno);
				exit(1);
			}
			int iseof, iserror, empty;
			mainPP.targetAd = new ClassAd;
			InsertFromFile(targetFile, *mainPP.targetAd, "\n\n", iseof, iserror, empty);
			fclose(targetFile);
		} else
		if (is_dash_arg_prefix (argv[i], "constraint", 3)) {
			// can add constraints on second pass only
			i++;
			if( ! argv[i] ) {
				fprintf( stderr, "%s: -constraint requires another argument\n",
						 myName );
				fprintf( stderr, "Use \"%s -help\" for details\n", myName );
				exit( 1 );
			}
		} else
		if (is_dash_arg_prefix (argv[i], "annex-slots", 5)) {
			// can add constraints on second pass only
			if( argv[i + 1] && argv[i + 1][0] != '-' ) { ++i; }
			annexMode = true;
		} else
		if (is_dash_arg_prefix (argv[i], "annex-name", 10)) {
			// can add constraints on second pass only
			i++;
			if( ! argv[i] ) {
				fprintf( stderr, "%s: -annex-name requires an annex name\n",
						 myName );
				fprintf( stderr, "Use \"%s -help\" for details\n", myName );
				exit( 1 );
			}
			annexMode = true;
		} else
		if (is_dash_arg_prefix (argv[i], "direct", 3)) {
			if( direct ) {
				free( direct );
				had_direct_error = 1;
			}
			i++;
			if( ! argv[i] ) {
				fprintf( stderr, "%s: -direct requires another argument\n",
						 myName );
				fprintf( stderr, "Use \"%s -help\" for details\n", myName );
				exit( 1 );
			}
			direct = strdup( argv[i] );
		} else
		if (is_dash_arg_colon_prefix (argv[i], "diagnose", &pcolon, 3)) {
			diagnose = 1;
			if (pcolon) diagnostics_ads_file = ++pcolon;
		} else
		if (is_dash_arg_prefix (argv[i], "debug", 2)) {
			// dprintf to console
			dprintf_set_tool_debug("TOOL", 0);
		} else
		if (is_dash_arg_prefix (argv[i], "defrag", 3)) {
			mainPP.setMode (SDO_Defrag, i, argv[i]);
		} else
		if (is_dash_arg_prefix (argv[i], "help", 1)) {
			usage ();
			exit (0);
		} else
		if (is_dash_arg_prefix(argv[i], "limit", 2)) {
			if( !argv[i+1] ) {
				fprintf( stderr, "%s: -limit requires one additional argument\n", myName );
				fprintf( stderr, "Use \"%s -help\" for details\n", myName );
				exit( 1 );
			}
			++i;
			result_limit = atoi(argv[i]);
		} else
		if (is_dash_arg_colon_prefix (argv[i], "long", &pcolon, 1)) {
			ClassAdFileParseType::ParseType parse_type = ClassAdFileParseType::Parse_long;
			if (pcolon) {
				StringList opts(++pcolon);
				for (const char * opt = opts.first(); opt; opt = opts.next()) {
					if (YourString(opt) == "nosort") {
						// Note: -attributes quietly disables -long:nosort unless output is long form
						print_attrs_in_hash_order = true;
					} else {
						parse_type = parseAdsFileFormat(opt, parse_type);
					}
				}
			}
			switch (parse_type) {
				default:
				case ClassAdFileParseType::Parse_long: mainPP.setPPstyle (PP_LONG, i, argv[i]); break;
				case ClassAdFileParseType::Parse_xml: mainPP.setPPstyle (PP_XML, i, argv[i]); break;
				case ClassAdFileParseType::Parse_json: mainPP.setPPstyle (PP_JSON, i, argv[i]); break;
				case ClassAdFileParseType::Parse_new: mainPP.setPPstyle (PP_NEWCLASSAD, i, argv[i]); break;
			}
		} else
		if (is_dash_arg_prefix (argv[i],"xml", 1)){
			mainPP.setPPstyle (PP_XML, i, argv[i]);
		} else
		if (is_dash_arg_prefix (argv[i],"json", 2)){
			mainPP.setPPstyle (PP_JSON, i, argv[i]);
		} else
		if (is_dash_arg_prefix (argv[i],"group-by", 5)){
			dash_group_by = true;
		} else
		if (is_dash_arg_prefix (argv[i],"attributes", 2)){
			if( !argv[i+1] ) {
				fprintf( stderr, "%s: -attributes requires one additional argument\n",
						 myName );
				fprintf( stderr, "Use \"%s -help\" for details\n", myName );
				exit( 1 );
			}
			i++;
		} else
		if (is_dash_arg_prefix(argv[i], "claimed", 2)) {
			mainPP.setMode (SDO_Startd_Claimed, i, argv[i]);
		} else
		if (is_dash_arg_prefix(argv[i], "data", 2)) {
			if (sdo_mode >= SDO_Schedd && sdo_mode < SDO_Schedd_Run) {
				mainPP.resetMode (SDO_Schedd_Data, i, argv[i]);
			} else {
				mainPP.setMode (SDO_Schedd_Data, i, argv[i]);
			}
		} else
		if (is_dash_arg_prefix (argv[i], "run", 1)) {
			// NOTE: default for bare -run changed from SDO_Startd_Claimed to SDO_Schedd_Run in 8.5.7
			if (sdo_mode == SDO_Startd || sdo_mode == SDO_Startd_Claimed) {
				mainPP.resetMode (SDO_Startd_Claimed, i, argv[i]);
			} else {
				if (sdo_mode == SDO_Schedd) {
					mainPP.resetMode (SDO_Schedd_Run, i, argv[i]);
				} else {
					mainPP.setMode (SDO_Schedd_Run, i, argv[i]);
				}
			}
		} else
		if( is_dash_arg_prefix (argv[i], "cod", 3) ) {
			mainPP.setMode (SDO_Startd_Cod, i, argv[i]);
		} else
		if (is_dash_arg_prefix (argv[i], "java", 1)) {
			/*explicit_mode =*/ javaMode = true;
		} else
		if (is_dash_arg_prefix (argv[i], "absent", 2)) {
			/*explicit_mode =*/ absentMode = true;
		} else
		if (is_dash_arg_prefix (argv[i], "offline", 2)) {
			/*explicit_mode =*/ offlineMode = true;
		} else
		if (is_dash_arg_prefix (argv[i], "merge", 5)) {
			if( !argv[i+1] ) {
				fprintf( stderr, "%s: -merge requires a filename argument\n",
						 myName );
				fprintf( stderr, "Use \"%s -help\" for details\n", myName );
				exit( 1 );
			}
			i += 1;
			leftFileName = argv[i];
			mergeMode = true;
		} else
		if (is_dash_arg_prefix (argv[i], "vm", 2)) {
			/*explicit_mode =*/ vmMode = true;
		} else
		if (is_dash_arg_prefix (argv[i], "slots", 2)) {
			mainPP.setMode (SDO_Startd, i, argv[i]);
			compactMode = false;
		} else
		if (is_dash_arg_prefix (argv[i], "compact", 3)) {
			compactMode = true;
		} else
		if (is_dash_arg_prefix (argv[i], "nocompact", 5)) {
			compactMode = false;
		} else
		if (is_dash_arg_prefix (argv[i], "server", 2)) {
			//PRAGMA_REMIND("TJ: change to sdo_mode")
			mainPP.setPPstyle (PP_STARTD_SERVER, i, argv[i]);
		} else
		if (is_dash_arg_prefix (argv[i], "state", 4)) {
			//PRAGMA_REMIND("TJ: change to sdo_mode")
			mainPP.setPPstyle (PP_STARTD_STATE, i, argv[i]);
		} else
		if (is_dash_arg_prefix (argv[i], "statistics", 5)) {
			if( statistics ) {
				free( statistics );
				had_statistics_error = 1;
			}
			i++;
			if( ! argv[i] ) {
				fprintf( stderr, "%s: -statistics requires another argument\n",
						 myName );
				fprintf( stderr, "Use \"%s -help\" for details\n", myName );
				exit( 1 );
			}
			statistics = strdup( argv[i] );
		} else
		if (is_dash_arg_prefix (argv[i], "startd", 4)) {
			mainPP.setMode (SDO_Startd,i, argv[i]);
		} else
		if (is_dash_arg_prefix (argv[i], "schedd", 2)) {
			if (sdo_mode == SDO_Startd_Claimed) {
				mainPP.resetMode (SDO_Schedd_Run, i, argv[i]);
			} else if (sdo_mode >= SDO_Schedd && sdo_mode <= SDO_Schedd_Run) {
				// Do nothing.
			} else {
				mainPP.setMode (SDO_Schedd, i, argv[i]);
			}
		} else
		if (is_dash_arg_prefix (argv[i], "grid", 1)) {
			mainPP.setMode (SDO_Grid, i, argv[i]);
		} else
		if (is_dash_arg_prefix (argv[i], "subsystem", 4)) {
			i++;
			if( !argv[i] || *argv[i] == '-') {
				fprintf( stderr, "%s: -subsystem requires another argument\n",
						 myName );
				fprintf( stderr, "Use \"%s -help\" for details\n", myName );
				exit( 1 );
			}
			static const struct { const char * tag; int sm; } asub[] = {
				{"schedd", SDO_Schedd},
				{"submitters", SDO_Submitters},
				{"startd", SDO_Startd},
				{"defrag", SDO_Defrag},
				{"grid", SDO_Grid},
				{"accounting", SDO_Accounting},
				{"negotiator", SDO_Negotiator},
				{"master", SDO_Master},
				{"collector", SDO_Collector},
				{"generic", SDO_Generic},
				{"had", SDO_HAD},
			};
			int sm = SDO_NotSet;
			for (int ii = 0; ii < (int)COUNTOF(asub); ++ii) {
				if (is_arg_prefix(argv[i], asub[ii].tag, -1)) {
					sm = asub[ii].sm;
					break;
				}
			}
			if (sm != SDO_NotSet) {
				mainPP.setMode (sm, i, argv[i]);
			} else {
				genericType = argv[i];
				mainPP.setMode (SDO_Other, i, argv[i]);
			}
		} else
		if (is_dash_arg_prefix (argv[i], "license", 2)) {
			mainPP.setMode (SDO_License, i, argv[i]);
		} else
		if (is_dash_arg_prefix (argv[i], "storage", 3)) {
			mainPP.setMode (SDO_Storage, i, argv[i]);
		} else
		if (is_dash_arg_prefix (argv[i], "negotiator", 1)) {
			mainPP.setMode (SDO_Negotiator, i, argv[i]);
		} else
		if (is_dash_arg_prefix (argv[i], "generic", 2)) {
			mainPP.setMode (SDO_Generic, i, argv[i]);
		} else
		if (is_dash_arg_prefix (argv[i], "any", 2)) {
			mainPP.setMode (SDO_Any, i, argv[i]);
		} else
		if (is_dash_arg_prefix (argv[i], "sort", 2)) {
			i++;
			if( ! argv[i] ) {
				fprintf( stderr, "%s: -sort requires another argument\n",
						 myName );
				fprintf( stderr, "Use \"%s -help\" for details\n", myName );
				exit( 1 );
			}

			if (MATCH == strcasecmp(argv[i], "false") ||
				MATCH == strcasecmp(argv[i], "0") ||
				MATCH == strcasecmp(argv[i], "no") ||
				MATCH == strcasecmp(argv[i], "none"))
			{
				noSort = true;
				continue;
			}

			if ( ! sortSpecs.Add(argv[i])) {
				fprintf(stderr, "Error:  Parse error of: %s\n", argv[i]);
				exit(1);
			}
		} else
		if (is_dash_arg_prefix (argv[i], "submitters", 4)) {
			mainPP.setMode (SDO_Submitters, i, argv[i]);
		} else
		if (is_dash_arg_prefix (argv[i], "master", 1)) {
			mainPP.setMode (SDO_Master, i, argv[i]);
		} else
		if (is_dash_arg_prefix (argv[i], "collector", 3)) {
			mainPP.setMode (SDO_Collector, i, argv[i]);
		} else
		if (is_dash_arg_prefix (argv[i], "world", 1)) {
			mainPP.setMode (SDO_Collector, i, argv[i]);
		} else
		if (is_dash_arg_prefix (argv[i], "ckptsrvr", 2)) {
			mainPP.setMode (SDO_CkptSvr, i, argv[i]);
		} else
		if (is_dash_arg_prefix (argv[i], "total", 1)) {
			mainPP.wantOnlyTotals = true;
			mainPP.pmHeadFoot = (printmask_headerfooter_t)(HF_NOTITLE | HF_NOHEADER);
			explicit_format = true;
		} else
		if (is_dash_arg_prefix(argv[i], "expert", 1)) {
			expert = true;
		} else
		if (is_dash_arg_prefix(argv[i], "version", 3)) {
			printf( "%s\n%s\n", CondorVersion(), CondorPlatform() );
			exit(0);
		} else
		if (*argv[i] == '-') {
			fprintf (stderr, "Error:  Unknown option %s\n", argv[i]);
			usage ();
			exit (1);
		}
	}
	if( had_pool_error ) {
		fprintf( stderr,
				 "Warning:  Multiple -pool arguments given, using \"%s\"\n",
				 pool->name() );
	}
	if( had_direct_error ) {
		fprintf( stderr,
				 "Warning:  Multiple -direct arguments given, using \"%s\"\n",
				 direct );
	}
	if( had_statistics_error ) {
		fprintf( stderr,
				 "Warning:  Multiple -statistics arguments given, using \"%s\"\n",
				 statistics );
	}
}


void
secondPass (int argc, char *argv[])
{
	const char * pcolon = NULL;
	char *daemonname;
	for (int i = 1; i < argc; i++) {
		// omit parameters which qualify switches
		if( is_dash_arg_prefix(argv[i],"pool", 1) || is_dash_arg_prefix(argv[i],"direct", 3) ) {
			i++;
			continue;
		}
		if( is_dash_arg_prefix(argv[i],"subsystem", 4) ) {
			i++;
			continue;
		}
		if (is_dash_arg_prefix(argv[i], "limit", 2)) {
			++i;
			continue;
		}
		if (is_dash_arg_prefix (argv[i], "format", 1)) {
			mainPP.pm.registerFormatF (argv[i+1], argv[i+2], FormatOptionNoTruncate);

			classad::References attributes;
			ClassAd ad;
			if(!GetExprReferences(argv[i+2],ad,NULL,&attributes)){
				fprintf( stderr, "Error:  Parse error of: %s\n", argv[i+2]);
				exit(1);
			}

			projList.insert(attributes.begin(), attributes.end());

			if (diagnose) {
				printf ("Arg %d --- register format [%s] for [%s]\n",
						i, argv[i+1], argv[i+2]);
			}
			i += 2;
			continue;
		}
		if (*argv[i] == '-' &&
			(is_arg_colon_prefix(argv[i]+1, "autoformat", &pcolon, 5) || 
			 is_arg_colon_prefix(argv[i]+1, "af", &pcolon, 2)) ) {
				// make sure we have at least one more argument
			if ( !argv[i+1] || *(argv[i+1]) == '-') {
				fprintf( stderr, "Error: Argument %s requires "
						 "at last one attribute parameter\n", argv[i] );
				fprintf( stderr, "Use \"%s -help\" for details\n", myName );
				exit( 1 );
			}

			bool flabel = false;
			bool fCapV  = false;
			bool fRaw = false;
			bool fheadings = false;
			const char * prowpre = NULL;
			const char * pcolpre = " ";
			const char * pcolsux = NULL;
			if (pcolon) {
				++pcolon;
				while (*pcolon) {
					switch (*pcolon)
					{
						case ',': pcolsux = ","; break;
						case 'n': pcolsux = "\n"; break;
						case 'g': pcolpre = NULL; prowpre = "\n"; break;
						case 't': pcolpre = "\t"; break;
						case 'l': flabel = true; break;
						case 'V': fCapV = true; break;
						case 'r': case 'o': fRaw = true; break;
						case 'h': fheadings = true; break;
					}
					++pcolon;
				}
			}
			mainPP.pm.SetAutoSep(prowpre, pcolpre, pcolsux, "\n");

			while (argv[i+1] && *(argv[i+1]) != '-') {
				++i;
				ClassAd ad;
				classad::References attributes;
				if(!GetExprReferences(argv[i],ad,NULL,&attributes)){
					fprintf( stderr, "Error:  Parse error of: %s\n", argv[i]);
					exit(1);
				}

				projList.insert(attributes.begin(), attributes.end());

				MyString lbl = "";
				int wid = 0;
				int opts = FormatOptionNoTruncate;
				if (fheadings || mainPP.pm_head.Length() > 0) { 
					const char * hd = fheadings ? argv[i] : "(expr)";
					wid = 0 - (int)strlen(hd); 
					opts = FormatOptionAutoWidth | FormatOptionNoTruncate; 
					mainPP.pm_head.Append(hd);
				}
				else if (flabel) { lbl.formatstr("%s = ", argv[i]); wid = 0; opts = 0; }
				lbl += fRaw ? "%r" : (fCapV ? "%V" : "%v");
				if (diagnose) {
					printf ("Arg %d --- register format [%s] width=%d, opt=0x%x for [%s]\n",
							i, lbl.Value(), wid, opts,  argv[i]);
				}
				mainPP.pm.registerFormat(lbl.Value(), wid, opts, argv[i]);
			}
			// if autoformat list ends in a '-' without any characters after it, just eat the arg and keep going.
			MSC_SUPPRESS_WARNING(6011) // code analysis can't figure out that argc test protects us from de-refing a NULL in argv
			if (i+1 < argc && '-' == (argv[i+1])[0] && 0 == (argv[i+1])[1]) {
				++i;
			}
			continue;
		}
		if (is_dash_arg_colon_prefix(argv[i], "print-format", &pcolon, 2)) {
			if ( (i+1 >= argc)  || (*(argv[i+1]) == '-' && (argv[i+1])[1] != 0)) {
				fprintf( stderr, "Error: Argument -print-format requires a filename argument\n");
				exit( 1 );
			}
			// hack allow -pr ! to disable use of user-default print format files.
			if (MATCH == strcmp(argv[i+1], "!")) {
				++i;
				disable_user_print_files = true;
				continue;
			}
			mainPP.ppTotalStyle = mainPP.ppStyle;
			mainPP.setPPstyle (PP_CUSTOM, i, argv[i]);
			mainPP.setPPwidth();
			++i; // skip to the next argument.
			if (mainPP.set_status_print_mask_from_stream(argv[i], true, &mode_constraint) < 0) {
				fprintf(stderr, "Error: invalid select file %s\n", argv[i]);
				exit (1);
			}
			if (mode_constraint) {
				query->addANDConstraint(mode_constraint);
			}
			mainPP.using_print_format = true; // so we can hack totals.
			continue;
		}
		if (is_dash_arg_prefix (argv[i], "target", 4)) {
			i++;
			continue;
		}
		if (is_dash_arg_colon_prefix(argv[i], "ads", &pcolon, 2)) {
			++i;
			continue;
		}
		if (is_dash_arg_prefix (argv[i], "merge", 5)) {
			++i;
			continue;
		}
		if (is_dash_arg_prefix (argv[i], "group-by", 5)) {
			++i;
			continue;
		}
		if( is_dash_arg_prefix(argv[i], "sort", 2) ) {
			i++;
			if ( ! noSort) {
				sprintf( buffer, "%s =!= UNDEFINED", argv[i] );
				query->addANDConstraint( buffer );
			}
			continue;
		}

		if (is_dash_arg_prefix (argv[i], "statistics", 5)) {
			i += 2;
			sprintf(buffer,"STATISTICS_TO_PUBLISH = \"%s\"", statistics);
			if (diagnose) {
				printf ("[%s]\n", buffer);
			}
			query->addExtraAttribute(buffer);
			continue;
		}

		if (is_dash_arg_prefix (argv[i], "attributes", 2) ) {
			// parse attributes to be selected and split them along ","
			StringList more_attrs(argv[i+1],",");
			for (const char * s = more_attrs.first(); s; s = more_attrs.next()) {
				projList.insert(s);
				dashAttributes.append(s);
			}

			i++;
			continue;
		}



		// figure out what the other parameters should do
		if (*argv[i] != '-') {
			// display extra information for diagnosis
			if (diagnose) {
				printf ("Arg %d (%s) --- adding constraint", i, argv[i]);
			}

			const char * name = argv[i];
			daemonname = get_daemon_name(name);
			if( ! daemonname || ! daemonname[0]) {
				if ( (sdo_mode == SDO_Submitters) && strchr(argv[i],'@') ) {
					// For a submittor query, it is possible that the
					// hostname is really a UID_DOMAIN.  And there is
					// no requirement that UID_DOMAIN actually have
					// an inverse lookup in DNS...  so if get_daemon_name()
					// fails with a fully qualified submittor lookup, just
					// use what we are given and do not flag an error.
				} else {
					//PRAGMA_REMIND("TJ: change this to do the query rather than reporting an error?")
					dprintf_WriteOnErrorBuffer(stderr, true);
					fprintf( stderr, "%s: unknown host %s\n",
								 argv[0], get_host_part(argv[i]) );
					exit(1);
				}
			} else {
				name = daemonname;
			}

			if (sdo_mode == SDO_Startd_Claimed) {
				sprintf (buffer, ATTR_REMOTE_USER " == \"%s\"", argv[i]);
				if (diagnose) { printf ("[%s]\n", buffer); }
				query->addORConstraint (buffer);
			} else {
				sprintf(buffer, ATTR_NAME "==\"%s\" || " ATTR_MACHINE "==\"%s\"", name, name);
				if (diagnose) { printf ("[%s]\n", buffer); }
				query->addORConstraint (buffer);
			}
			free(daemonname);
			daemonname = NULL;
		} else if (is_dash_arg_prefix (argv[i], "constraint", 3)) {
			if (diagnose) { printf ("[%s]\n", argv[i+1]); }
			query->addANDConstraint (argv[i+1]);
			i++;
		} else if( is_dash_arg_prefix( argv[i], "annex-slots", 5 ) ||
		           is_dash_arg_prefix( argv[i], "annex-name", 10 ) ) {
			std::string constraint;
			if( argv[i + 1] && (argv[i + 1][0] != '-' || is_dash_arg_prefix( argv[i], "annex-name", 10 )) ) {
				formatstr( constraint, ATTR_ANNEX_NAME " =?= \"%s\"", argv[i + 1] );
				i++;
			} else {
				constraint = "IsAnnex";
			}
			if (diagnose) { printf ("[%s]\n", constraint.c_str()); }
			query->addANDConstraint (constraint.c_str());
		}
	}
}

