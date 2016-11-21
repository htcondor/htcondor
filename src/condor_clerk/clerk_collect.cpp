/***************************************************************
 *
 * Copyright (C) 2014, Condor Team, Computer Sciences Department,
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
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_commands.h"
#include "command_strings.h"
#include "condor_classad.h"
#include "condor_attributes.h"
#include "condor_adtypes.h"

// some platforms (rhel6) don't have emplace yet.
//#define HAS_EMPLACE_HINT

#include "clerk_utils.h"
#include "clerk_collect.h"
#include <map>
#include "tokener.h"


typedef std::map<std::string, AdCollectionEntry> AdCollection;
typedef std::pair<std::string, AdCollectionEntry> AdCollectionPair;
extern AdTypes CollectorCmdToAdType(int cmd);

#define ADTYPE_FMT "%03d"

const char * CollectOpName(int op)
{
	switch(op)
	{
	case COLLECT_OP_REPLACE: return "REPLACE";
	case COLLECT_OP_MERGE:   return "MERGE";
	case COLLECT_OP_EXPIRE:  return "EXPIRE";
	case COLLECT_OP_DELETE:  return "DELETE";
	}
	return "UNKNOWN_OP";
}

class SelfCollection {
public:
	SelfCollection() : typ(COLLECTOR_AD), ad(NULL) {}
	int update(ClassAd * cad);
	bool IsSelf(const std::string & ad_key) { return ad_key == key; }
private:
	AdTypes typ;
	ClassAd * ad; // this is not owned by this class, it points to the global daemon ad
	std::string key;
	//std::string name;
	//std::string addr;
};

// these are used with keyAttrUse mask
static const int kfName     = 0x01; // ATTR_NAME
static const int kfAddr     = 0x02; // ATTR_MY_ADDRESS
static const int kfHost     = 0x04; // extracts Host from ATTR_MY_ADDRESS when this is set with kfAddr
static const int kfMachine  = 0x08; // ATTR_MACHINE
static const int kfSchedd   = 0x10; // ATTR_SCHEDD_NAME
static const int kfHash     = 0x80; // ATTR_HASH_NAME

typedef struct _settings_by_adtype {
	const char * name;       // typename for the ads of this type
	const char * zzkeys;     // override of keys for ads of this type
	time_t       lifetime;   // lifetime to use for this adtype if the ad itself does not specify
	unsigned int keyAttrUse; // 'known' attributes that this adtype uses to make it's keys
	bool         bigTable;
	bool         spare1;
	bool         spare2;
	bool         spare3;
} SettingsByAdType;

int maxAdSettings = 200 + ((NUM_AD_TYPES/100)*100);
const int minDynamicAd = NUM_AD_TYPES + (0x20 - (NUM_AD_TYPES&0x1F)); // lowest AdTypes value for a dynamically configured ad
int topDynamicAd    = minDynamicAd; // hightest used AdTypes value for a dynamically configured ad
SettingsByAdType * AdSettings = NULL;
int maxOldAdSettings = 0;
SettingsByAdType * OldAdSettings = NULL;
NOCASE_STRING_TO_INT_MAP MyTypeToAdType;

struct {
	AdCollection Public;
	AdCollection Private;
	SelfCollection Self;
	ConstraintHolder AbsentReq;
	ConstraintHolder NotAbsent; // holds the expression 'Absent =!= true' so we can copy it
	time_t AbsentLifetime;  // absent lifetime
	bool HOUSEKEEPING_ON_INVALIDATE; // when true, invalidate by expression should be done by the housekeeper.
} Collections;

typedef struct StdAdInfoItem {
	bool bigTable;
	bool hasPrivate;
	bool spare;
	unsigned char kfUse;
} StdAdInfoItem;

#define bigtable 1
#define hasPeer  1
static StdAdInfoItem StandardAdInfo[] = {
	{0,       0      , 0      , kfName | kfSchedd },		// QUILL_AD,
	{bigtable,hasPeer, 0      , kfName | kfAddr | kfHost },	// STARTD_AD,
	{0,       0      , 0      , kfName | kfAddr | kfHost },	// SCHEDD_AD,
	{bigtable,0      , 0      , kfName | kfAddr | kfHost },	// MASTER_AD,
	{0,       0      , 0      , kfName },					// GATEWAY_AD,
	{0,       0      , 0      , kfMachine },				// CKPT_SRVR_AD,
	{bigtable,hasPeer, 0      , kfName | kfAddr | kfHost },	// STARTD_PVT_AD,
	{bigtable,0      , 0      , kfName | kfSchedd | kfAddr | kfHost },	// SUBMITTOR_AD,
	{0,       0      , 0      , kfName | kfMachine },		// COLLECTOR_AD,
	{0,       0      , 0      , kfName | kfAddr | kfHost },	// LICENSE_AD,
	{0,       0      , 0      , kfName },					// STORAGE_AD,
	{bigtable,0      , 0      , kfName },					// ANY_AD,
	{0,       0      , 0      , kfName },					// BOGUS_AD
	{0,       0      , 0      , kfName },					// CLUSTER_AD,
	{0,       0      , 0      , kfName },					// NEGOTIATOR_AD,
	{0,       0      , 0      , kfName },					// HAD_AD,
	{bigtable,0      , 0      , kfName },					// GENERIC_AD,
	{0,       0      , 0      , kfName },					// CREDD_AD,
	{0,       0      , 0      , kfName },					// DATABASE_AD,
	{0,       0      , 0      , kfName },					// DBMSD_AD,
	{0,       0      , 0      , kfName },					// TT_AD,
	{0,       0      , 0      , kfHash | kfSchedd },		// GRID_AD,
	{0,       0      , 0      , kfName },					// XFER_SERVICE_AD,
	{0,       0      , 0      , kfName },					// LEASE_MANAGER_AD,
	{0,       0      , 0      , kfName },					// DEFRAG_AD,
	{bigtable,0      , 0      , kfName },					// ACCOUNTING_AD,
};
#undef bigtable
#undef hasPeer

#if 0 // this is superceeded by the StandardAdInfo table above
switch (aty) {
	case STARTD_AD: //    key = (Name||Machine)+ipAddr(MyAddress)
	case STARTD_PVT_AD:
	case SCHEDD_AD:
	case MASTER_AD:
	case LICENSE_AD: // *only one licence ad at a time is allowed
	sat[aty].keyAttrUse = kfName | kfAddr | kfHost;
	break;
case SUBMITTOR_AD: // key = Name+ScheddName+ipAddr(MyAddress)
	sat[aty].keyAttrUse = kfName | kfSchedd | kfAddr | kfHost;
	break;
case QUILL_AD:     // key = Name+ScheddName
	sat[aty].keyAttrUse = kfName | kfSchedd;
	break;
case GRID_AD: //  key = HashName+Owner+(ScheddName||ScheddIpAddr)
	sat[aty].keyAttrUse = kfHash | kfSchedd;
	break;
case CKPT_SRVR_AD: // key = Machine
	sat[aty].keyAttrUse = kfMachine;
	break;
case COLLECTOR_AD: // key = (Name||Machine)
	sat[aty].keyAttrUse = kfName | kfMachine;
	break;
}
#endif

// Returns true if the table is large for this adtype
bool IsBigTable(AdTypes typ) {
	if (typ >= 0 && typ < (int)COUNTOF(StandardAdInfo)) {
		return StandardAdInfo[typ].bigTable;
	}
	return false;
}

// Returns the public AdType for this ad, if the AdType is public, returns itself
AdTypes has_public_adtype(AdTypes whichAds)
{
	if (whichAds == STARTD_PVT_AD) return STARTD_AD;
	return whichAds;
}

// Returns the private AdType associated with this ad. If The AdType is private, returns itself
// if there is no private AdType, returns NO_AD
AdTypes has_private_adtype(AdTypes whichAds)
{
	if (whichAds == STARTD_AD || whichAds == STARTD_PVT_AD) return STARTD_PVT_AD;
	return NO_AD;
}


void collect_config(int max_adtype)
{
	config_standard_ads(max_adtype);

	Collections.HOUSEKEEPING_ON_INVALIDATE = param_boolean("HOUSEKEEPING_ON_INVALIDATE", true);

	const int def_lifetime = 60 * 60 * 24 * 30; // default expire absent ads in a month
	Collections.AbsentLifetime = param_integer ("ABSENT_EXPIRE_ADS_AFTER", def_lifetime);
	if (0 == Collections.AbsentLifetime) Collections.AbsentLifetime = INT_MAX; // 0 means forever

	Collections.AbsentReq.set(param("ABSENT_REQUIREMENTS"));
	Collections.NotAbsent.set(strdup(ATTR_ABSENT " =!= true"));
	dprintf (D_ALWAYS,"ABSENT_REQUIREMENTS = %s\n", Collections.AbsentReq.c_str());
}

// setup the AdSettings table for the standard ad types
void config_standard_ads(int max_adtype)
{
	// allocate an array for the ad settings
	if (max_adtype > maxAdSettings || ! AdSettings) {
		if (OldAdSettings) free((char*)OldAdSettings);
		OldAdSettings = AdSettings;
		maxOldAdSettings = maxAdSettings;

		size_t cbAlloc = MAX(max_adtype, maxAdSettings) * sizeof(SettingsByAdType);
		char * ptr = (char*)malloc(cbAlloc);
		memset(ptr, 0, cbAlloc);
		AdSettings = (SettingsByAdType*)ptr;
		maxAdSettings = MAX(max_adtype, maxAdSettings);
	}

	// fill in the standard ad types
	SettingsByAdType * sat = AdSettings;
	for (int aty = 0; aty < NUM_AD_TYPES; ++aty) {
		if (OldAdSettings) { sat[aty] = OldAdSettings[aty]; } // preserve some settings
		sat[aty].name = AdTypeToString((AdTypes)aty);
		sat[aty].lifetime = 1;
		ASSERT(aty < (int)COUNTOF(StandardAdInfo));
		sat[aty].bigTable = StandardAdInfo[aty].bigTable;
		sat[aty].keyAttrUse = StandardAdInfo[aty].kfUse;
		MyTypeToAdType[sat[aty].name] = aty;
	}
	MyTypeToAdType["NO"] = NO_AD;

	// default some things for the dynamic ad types
	for (int aty = NUM_AD_TYPES; aty < maxAdSettings; ++aty) {
		if (OldAdSettings && (aty < maxOldAdSettings)) { sat[aty] = OldAdSettings[aty]; } // preserve some settings
		// default some things
		sat[aty].name = NULL;
		sat[aty].lifetime = 1; // 1 means expire on next housecleaning pass.
		sat[aty].keyAttrUse = kfName;
	}
}

AdTypes collect_register_adtype(const char * mytype)
{
	int whichAds = NO_AD;
	NOCASE_STRING_TO_INT_MAP::iterator found = MyTypeToAdType.find(mytype);
	if (found != MyTypeToAdType.end()) {
		whichAds = found->second;
	} else {
		SettingsByAdType * sat = AdSettings;
		whichAds = minDynamicAd;
		while (whichAds < maxAdSettings && sat[whichAds].name != NULL) ++whichAds;
	}
	// if we couldn't find a slot, return NO_AD
	if (whichAds < 0 || whichAds >= maxAdSettings) return NO_AD;

	// if the slot we found is one the the standard types, just quit.
	if (whichAds < NUM_AD_TYPES) return (AdTypes)whichAds;

	// add the mapping between name and integer
	MyTypeToAdType[mytype] = whichAds;

	// Found a slot, and it's not a standard type. init the table entry
	SettingsByAdType & mysat = AdSettings[whichAds];
	mysat.name = ClerkVars.apool.insert(mytype);
	mysat.lifetime = 1;
	mysat.keyAttrUse = kfName | kfHash;
	mysat.zzkeys = NULL;

	return (AdTypes)whichAds;
}

// these are used to help parse the keylist passed to collect_set_adtype
typedef struct {
	const char * key;
	int          value;
} AttrToInt;
typedef nocase_sorted_tokener_lookup_table<AttrToInt> AttrToIntTable;
static const AttrToInt StdKeyAttrsItems[] = {
	{ATTR_HASH_NAME, kfHash},
	{"HOST", kfHost},
	{"IP", kfHost},
	{ATTR_MACHINE, kfMachine},
	{ATTR_MY_ADDRESS, kfAddr},
	{ATTR_NAME, kfName},
	{ATTR_SCHEDD_NAME, kfSchedd},
};
static const AttrToIntTable StdKeyAttrs = SORTED_TOKENER_TABLE(StdKeyAttrsItems);

// customize standard ad types, and control the behavior of dynmic ad types
//
void collect_set_adtype(AdTypes whichAds, const char * keylist, double lifetime, bool big_table)
{
	if (whichAds < 0 || whichAds >= maxAdSettings) return;

	SettingsByAdType & mysat = AdSettings[whichAds];

	mysat.lifetime = (time_t)lifetime;
	mysat.bigTable = big_table;

	if (keylist) {
		StringTokenIterator attrs(keylist);
		// first, try see if all of the keys in the keylist are standard
		// also determine the size needed for a szz list of keys
		int cch = 2;
		unsigned int usemask = 0;
		const unsigned int kfNotFound = 0x80000000;
		for (const char * attr = attrs.first(); attr; attr = attrs.next()) {
			cch += strlen(attr) + 1;
			const AttrToInt * ki = StdKeyAttrs.lookup(attr);
			usemask |= (ki ? ki->value : kfNotFound);
		}

		bool has_private = (has_private_adtype(whichAds) != NO_AD);
		if (has_private && (usemask && kfNotFound)) {
			dprintf(D_ALWAYS, "Non-standard ReKey of %s ads unsupported because of private ads. key: %s\n", AdTypeName(whichAds), keylist);
			usemask = mysat.keyAttrUse; // just preserve the existing keyset.
		}

		// if all standard keys are used, set the key use mask
		if ( ! (usemask & kfNotFound)) {
			mysat.keyAttrUse = usemask;
			mysat.zzkeys = NULL;
		} else {
			// if any non-standard keys are used, set the key szz string
			char * p = ClerkVars.apool.consume(cch, 8);
			mysat.zzkeys = p;
			mysat.keyAttrUse = 0;
			for (const char * attr = attrs.first(); attr; attr = attrs.next()) {
				strcpy(p, attr);
				p += strlen(p)+1;
			}
			*p++ = 0;
		}
	}
}

void collect_finalize_ad_tables()
{
	PRAGMA_REMIND("cleanup and log final ad tables")
}

PRAGMA_REMIND("write housekeeping / remove expired ads.")

int default_lifetime(AdTypes whichAds)
{
	if (whichAds > 0 && whichAds < maxAdSettings) { return AdSettings[whichAds].lifetime; }
	return 1; // 1 means expire on next housecleaning pass.
}

// ad an absent clause to the the given filter if it does not already refer to ATTR_ABSENT
bool collect_add_absent_clause(ConstraintHolder & filter_if)
{
	bool bval;
	ExprTree * expr = filter_if.Expr();
	if (expr && ExprTreeIsLiteralBool(expr, bval)) {
		// a literal false matches nothing
		if ( ! bval) return false;
		// a literal true is really no expression at all.
		if (bval) filter_if.clear();
	}
	if ( ! Collections.AbsentReq.empty()) {
		return false; // did not ammend absent requirements
	}

	// if the input filter is nothing, just copy the NotAbsent expression
	if (filter_if.empty()) {
		filter_if.set(Collections.NotAbsent.Expr()->Copy());
		return true;
	}

	// If there is an input filter and ABSENT_REQUIREMENTS is defined
	// rewrite filter to filter-out absent ads also if
	// ATTR_ABSENT is not alrady referenced in the query.

	NOCASE_STRING_TO_INT_MAP attrs;
	attrs[ATTR_ABSENT] = 0;
	if ( ! has_specific_attr_refs(filter_if.Expr(), attrs, true)) {

		ExprTree * modified_expr = JoinExprTreeCopiesWithOp(
			classad::Operation::LOGICAL_AND_OP,
			filter_if.Expr(), Collections.NotAbsent.Expr());

		filter_if.set(modified_expr);

		dprintf(D_FULLDEBUG,"Query after modification: *%s*\n", filter_if.c_str());
		return true;
	}

	return false;
}

AdCollectionEntry::AdCollectionEntry(AdTypes t, int f, time_t u, time_t e, compat_classad::ClassAd* a)
	: adType(t)
	, flags(f)
	, updateTime(u)
	, expireTime(e)
	, ad(a)
{
	if ((this->expireTime == (time_t)-1) && ad) {
		int lifetime = 0;
		if ( ! ad->LookupInteger(ATTR_CLASSAD_LIFETIME, lifetime)) { lifetime = default_lifetime(t); }
		this->expireTime = this->updateTime + lifetime;
	}
}

int AdCollectionEntry::DeleteAd(AdTypes atype, int new_flags, time_t now, time_t expire_time, bool expire_it /*=false*/)
{
	this->adType = atype;
	this->flags = new_flags; // something more complex needed here?
	this->updateTime = now;
	this->expireTime = expire_time;

	if (expire_it) {
	} else if (this->ad) {
		delete this->ad; this->ad = NULL;
	}
	return 0;
}

int AdCollectionEntry::UpdateAd(AdTypes atype, int new_flags, time_t now, time_t expire_time, compat_classad::ClassAd* new_ad, bool merge_it /*=false*/)
{
	this->adType = atype;
	this->flags = new_flags; // something more complex needed here?
	this->updateTime = now;
	this->expireTime = expire_time;
	//int lifetime = 0;
	//if ( ! new_ad->LookupInteger(ATTR_CLASSAD_LIFETIME, lifetime)) { lifetime = 1; }
	//this->expireTime = this->updateTime + lifetime;

	if (merge_it) {
		if (this->ad != new_ad) {
			this->ad->Update(*new_ad);
		}
	} else if (this->ad != new_ad) {
		delete this->ad; this->ad = NULL;
		this->ad = new_ad;
	}
	return 0;
}

// remove this ad from the Persistent log
// returns true if the ad was removed, or if there is no persist log
bool AdCollectionEntry::PersistentRemoveAd(AdTypes /*adtype*/, const std::string & /*key*/)
{
	PRAGMA_REMIND("write add persist remove")
	return true;
}

// Write this ad to the persist log
// returns true if the ad was saved, or if there is no persist log
bool AdCollectionEntry::PersistentStoreAd(AdTypes /*adtype*/, const std::string & /*key*/)
{
	PRAGMA_REMIND("write add persist store")
	return false;
}

bool AdCollectionEntry::CheckAbsent(AdTypes adtype, const std::string & key)
{
	if (Collections.AbsentReq.empty()) return false;
	if ( ! ad) return false;

	/* If the ad is alraedy has ABSENT=True and it is expiring, then
	   let it be deleted as in this case it already sat around absent
	   for the absent lifetime. */
	bool already_absent = false;
	ad->LookupBool(ATTR_ABSENT,already_absent);
	if (already_absent) {
		PersistentRemoveAd(adtype, key);
		return false; // return false tells collector to delete this ad
	}

	// Test is ad against the absent requirements expression, and
	// mark the ad absent if true
	if (EvalBool(ad, Collections.AbsentReq.Expr()))
	{
		time_t lifetime = Collections.AbsentLifetime;

		ad->Assign (ATTR_ABSENT, true);
		ad->Assign (ATTR_CLASSAD_LIFETIME, lifetime);

		time_t now = time(NULL);
		ad->Assign(ATTR_LAST_HEARD_FROM, now);
		ad->Assign (ATTR_MY_CURRENT_TIME, now);

		PersistentStoreAd(adtype, key);

		// if we marked this ad as absent, we want to keep it in the collector
		return true;	// return true tells the collector to KEEP this ad
	}

	return false; // return false tells collector to delete this ad
}




PRAGMA_REMIND("move this optimized version of getHostFromAddr to the right place")
// Scan a const string containing the host address and return
// a const char * that points to the start of the host name, and a length for the hostname.
// This code accepts either ipv4 or ipv6 sinful strings. of the format
// <uniq@host:port?options>  where host is n.n.n.n for ipv4 and [n:n:n::n] for ipv6
//  and uniq@ and ?options may not exist.
//
const char * findHostInAddr(const char * addr, /*_out_*/ int * hostlen) {
	bool at = false;  // set to true when we see an @ while parsing.
	int  colon = ':'; // set to end of hostname char, either : or ]
	const char * pb = addr; // pb will be set to beginning of hostname
	// setup for either ipv4 or ipv6 parsing.
	if (*pb == '<') { // < for sinful
		++pb;
		if (*pb == '[') {  // [ for ipv6
			++pb;
			colon = ']';
		}
	}
	const char * p = pb;
	while (*p) {
		char ch = *p;
		// if there's an '@' sign, then the actual hostname starts after it.
		if ( ! at && (ch == '@')) { pb = ++p; at = true; continue; }
		if (ch == colon || ch == '>' || ch == '?') { // we also check for > & ? incase the port is missing
			*hostlen = (int)(p - pb);
			return pb; 
		}
		++p;
	}
	*hostlen = (int)(p - pb);
	return pb;
}

#if 1
bool collect_make_key(AdTypes whichAds, const ClassAd* ad, ClassAd *adPvt, std::string & key)
{
	int uses_key_fields = 0;
	const char * pzz = NULL;
	int key_length = 100;
	if (whichAds > 0 && whichAds < maxAdSettings) {
		uses_key_fields = AdSettings[whichAds].keyAttrUse;
		pzz = AdSettings[whichAds].zzkeys;
	}

	bool private_inject_keys = (adPvt != NULL) && (has_private_adtype(whichAds) != NO_AD);
	if (private_inject_keys) {
		// Queries of private ads depend on having the keys match the public ad
		// TJ: do they really??
		SetMyTypeName(*adPvt, AdTypeName(whichAds));
	}

	key.reserve(key_length);
	formatstr(key, ADTYPE_FMT, whichAds);

	// build the key from the pzz list
	if (pzz) {
		classad::Value val;
		classad::ClassAdUnParser unparser;
		unparser.SetOldClassAd(true, true);

		ASSERT( ! private_inject_keys); // this code can't handle private key injection

		unsigned int found_attrs = 0;
		unsigned int attr_bit = 1;
		for (const char * p = pzz; (p && *p); p += strlen(p)+1) {
			key += "/";
			if (ad->EvaluateAttr(p, val) && ! val.IsExceptional()) {
				found_attrs |= attr_bit;
				// use strings without quotes, unparse other literals
				const char * strval;
				if (val.IsStringValue(strval)) {
					key += strval;
				} else {
					classad::abstime_t ticks; double secs;
					if (val.IsAbsoluteTimeValue(ticks)) { val.SetIntegerValue(ticks.secs); }
					else if (val.IsRelativeTimeValue(secs)) { val.SetRealValue(secs); }
					unparser.Unparse(key, val);
				}
				if (private_inject_keys) { 
					classad::ExprTree* plit = classad::Literal::MakeLiteral(val);
					adPvt->Insert(p, plit, false);
				}
			}
			attr_bit <<= 1;
		}

		// non of the attrs we need to build the key were found.
		return (found_attrs != 0);
	}

	// use standard key fields
	std::string val;

	int has_key_fields = 0;
	if ( ! uses_key_fields)
		uses_key_fields = kfName | kfHash;

	if (uses_key_fields & kfHash) {
		if (ad->LookupString(ATTR_HASH_NAME, val)) {
			has_key_fields |= kfHash | kfName;
			key += "/";
			key += val;
			if (private_inject_keys) { adPvt->Assign(ATTR_HASH_NAME, val); }
		}
	} else if (uses_key_fields & kfName) {
		if (ad->LookupString(ATTR_NAME, val)) {
			has_key_fields |= kfName;
			key += "/";
			key += val;
			if (private_inject_keys) { adPvt->Assign(ATTR_NAME, val); }
		}
	}

	if (uses_key_fields & kfMachine) {
		if (ad->LookupString(ATTR_MACHINE, val)) {
			has_key_fields |= kfMachine;
			key += "/";
			key += val;
			if (private_inject_keys) { adPvt->Assign(ATTR_MACHINE, val); }
		}
	}

	if (uses_key_fields & kfSchedd) {
		if (ad->LookupString(ATTR_SCHEDD_NAME, val)) {
			has_key_fields |= kfSchedd;
			key += "/";
			key += val;
			if (private_inject_keys) { adPvt->Assign(ATTR_SCHEDD_NAME, val); }
		}
	}

	if (uses_key_fields & (kfAddr|kfHost)) {
		if (ad->LookupString(ATTR_MY_ADDRESS, val)) {
			has_key_fields |= kfAddr;
			key += "/";
			if (private_inject_keys) { adPvt->Assign(ATTR_MY_ADDRESS, val); }
			if (uses_key_fields & kfHost) {
				// append the hostname/ip part of the address to the name in order to make the key for this item
				int cbhost = 0;
				const char * phost = findHostInAddr(val.c_str(), &cbhost);
				if (phost && cbhost) {
					has_key_fields |= kfHost;
					key.append(phost, cbhost);
				}
			} else {
				key.append(val);
			}
		}
	}

	if ((has_key_fields & uses_key_fields) != uses_key_fields) {
		// problem getting the attributues we will need to make a key
		// treat this as a malformed ad.
		return false;
	}

	return true;
}
#else
bool collect_make_key(AdTypes whichAds, const ClassAd* ad, ClassAd *adPvt, std::string & key)
{
	int uses_key_fields = 0;
	const char * pzz = NULL;
	if (whichAds > 0 && whichAds < maxAdSettings) {
		uses_key_fields = AdSettings[whichAds].keyAttrUse;
		pzz = AdSettings[whichAds].zzkeys;
	}
	if ( ! uses_key_fields) uses_key_fields = kfName;


	int has_key_fields = 0;
	// fetch attributes needed to build the hashkey.
	std::string name, owner, addr;
	if (ad->LookupString(ATTR_NAME, name)) {
		has_key_fields |= kfName;
	}

	bool use_host_of_addr = false;
	switch (whichAds) {
		case STARTD_AD: //    key = (Name||Machine)+ipAddr(MyAddress)
		case STARTD_PVT_AD:
		case SCHEDD_AD:
		case MASTER_AD:
		case LICENSE_AD: // *only one licence ad at a time is allowed
			if (ad->LookupString(ATTR_MY_ADDRESS, addr)) {
				has_key_fields |= kfAddr;
			}
			use_host_of_addr = true;
			break;

		case SUBMITTOR_AD: // key = Name+ScheddName+ipAddr(MyAddress)
		case QUILL_AD:     // key = Name+ScheddName
			if (ad->LookupString(ATTR_MY_ADDRESS, addr)) {
				has_key_fields |= kfAddr;
			}
			if (ad->LookupString(ATTR_SCHEDD_NAME, owner)) {
				has_key_fields |= kfSchedd;
			}
			break;

		case GRID_AD: //  key = HashName+Owner+(ScheddName||ScheddIpAddr)
			if ( ! ad->LookupString(ATTR_SCHEDD_NAME, owner)) {
				has_key_fields |= kfSchedd;
			}
			if (ad->LookupString(ATTR_HASH_NAME, name)) {
				has_key_fields |= kfHash;
			}
			break;

		case CKPT_SRVR_AD: // key = Machine
			if (ad->LookupString(ATTR_MACHINE, name)) {
				has_key_fields |= kfMachine;
			}
			break;

		case COLLECTOR_AD: // key = (Name||Machine)
			if (ad->LookupString(ATTR_MACHINE, name)) {
				has_key_fields |= kfMachine;
			}
			break;

		// STORAGE_AD:    key = Name
		// ACCOUNTING_AD: key = Name
		// NEGOTIATOR_AD: key = Name
		// HAD_AD:		  key = Name
		// XFER_SERVICE_AD: key = Name
		// LEASE_MANAGER_AD: Key = Name
		// GENERIC_AD:    key = Name
		default:
			break;
	}

	if ((has_key_fields & uses_key_fields) != uses_key_fields) {
		// problem getting the attributues we will need to make a key
		// treat this as a malformed ad.
		return false;
	}

	// HACK! force the startd private ad to have the same name & addr attributes as the public ad.
	if (has_private_adtype(whichAds) != NO_AD) {

		if (adPvt && (whichAds == STARTD_AD)) {
				// Queries of private ads depend on the following:
			SetMyTypeName(*adPvt, STARTD_ADTYPE);

				// Negotiator matches up private ad with public ad by
				// using the following.
			adPvt->InsertAttr(ATTR_NAME, name);
			adPvt->InsertAttr(ATTR_MY_ADDRESS, addr);
		}
	}

	// build up the key
	key.reserve(6+name.size()+owner.size()+addr.size());
	formatstr(key, ADTYPE_FMT "/%s", whichAds, name.c_str());
	if ( ! owner.empty()) {
		key.append("/");
		key.append(owner);
	}
	if ( ! addr.empty()) {
		key.append("/");
		if (use_host_of_addr) {
			// append the hostname/ip part of the address to the name in order to make the key for this item
			int cbhost = 0;
			const char * phost = findHostInAddr(addr.c_str(), &cbhost);
			if (phost && cbhost) {
				key.append(phost, cbhost);
			}
		} else {
			key.append(addr);
		}
	}

	return true;
}
#endif

int SelfCollection::update(ClassAd * cad)
{
	ad = cad;
	if (ad) {
		//ad->LookupString(ATTR_NAME, name);
		//ad->LookupString(ATTR_MY_ADDRESS, addr);
		collect_make_key(COLLECTOR_AD, cad, NULL, key);
	} else {
		//name.clear();
		//addr.clear();
		key.clear();
	}
	return 0;
}

int collect_self(ClassAd * ad)
{
	return Collections.Self.update(ad);
}


static AdCollection::iterator adtype_begins(AdTypes whichAds, AdCollection & lst)
{
	if (whichAds < 0 || whichAds == ANY_AD)
		return lst.begin();

	char key[10];
	sprintf(key, ADTYPE_FMT "/", whichAds);
	return lst.lower_bound(key);
}

// returns < 0   if the callback returns one of the Abort values
//         >= 0  on success, value is the number of matches (i.e. callbacks that did not return NoMatch)
int collect_walk (
	AdTypes whichAds,
	ConstraintHolder & call_if,
	WalkPostOp (*callback)(void* pv, AdCollectionEntry& entry),
	void*pv)
{
	dprintf(D_FULLDEBUG, "collect_walk(%s, %p) expr: %s\n", AdTypeName(whichAds), pv, call_if.c_str());
	if (whichAds == NO_AD)
		return 0;

	AdTypes key_aty = has_public_adtype(whichAds); // make sure we use the public adtype, this is how private ads are keyed.
	AdCollection::iterator begin = adtype_begins(key_aty, Collections.Public);
	bool single_adtype = (whichAds < 0 || whichAds != ANY_AD);
	bool private_also = ! single_adtype || has_private_adtype(whichAds) != NO_AD;

	int cMatched = 0;
	int cPvtMatched = 0;

	ExprTree *expr = call_if.Expr();
	AdCollection::iterator it, jt;
	for (it = begin; it != Collections.Public.end(); it = jt) {
		jt = it; ++jt;

		// if deleting only a specific adtype, we can stop as soon as we see an ad of the wrong type
		if (single_adtype && (whichAds != it->second.adType)) {
			break;
		}

		if ( ! expr || EvalBool(it->second.ad, expr)) {
			WalkPostOp post = callback(pv, it->second);
			if (post < 0) {
				// this is one of the abort returns
				dprintf(D_ALWAYS, "collect_walk(%s, %p) ABORTED with %d expr: %s\n", AdTypeName(whichAds), pv, post, call_if.c_str());
				return post;
			}
			if ( ! (post & Walk_Op_NoMatch)) {
				++cMatched;
			}
			if (post & Walk_Op_DeleteItem) {
				if (private_also && has_private_adtype(it->second.adType) != NO_AD) {
					AdCollection::iterator pt = Collections.Private.find(it->first);
					if (pt != Collections.Private.end()) {
						pt->second.DeleteAd(whichAds, 0, 0, 0, false);
						Collections.Private.erase(pt);
						++cPvtMatched;
					}
				}
				ASSERT( ! it->second.ad); // the ad had better have been deleted already!
				Collections.Public.erase(it);
			}
			if (post & Walk_Op_Break) {
				break;
			}
		}
	}

	dprintf(D_FULLDEBUG, "collect_walk(%s, %p) matched %d ads, and %d Private ads\n", AdTypeName(whichAds), pv, cMatched, cPvtMatched);
	return cMatched;
}

int collect_delete(int operation, AdTypes whichAds, const std::string & key)
{
	dprintf(D_FULLDEBUG, "collect_delete(%s, %s) key: %s\n",
		CollectOpName(operation), AdTypeName(whichAds), key.c_str());

	if (whichAds == NO_AD)
		return 0;

	bool expire_ads = (operation == COLLECT_OP_EXPIRE);
	bool is_absent = false;
	int cMatches = 0;

	AdCollection::iterator it;
	it = Collections.Public.find(key);
	if (it != Collections.Public.end()) {
		++cMatches;
		if (expire_ads) {
			is_absent = it->second.CheckAbsent(whichAds, key);
		}
		if (IsDebugCategory(D_STATUS)) {
			const char * opname = is_absent ? "**Absenting" : (expire_ads) ? "**Expiring" : "**Removing";
			dprintf(D_STATUS, "%s %s ad key='%s'\n", opname, AdTypeName(whichAds), key.c_str());
		}
		if ( ! is_absent) {
			it->second.DeleteAd(whichAds, 0, 0, 0, expire_ads);
			if ( ! expire_ads) Collections.Public.erase(it);
		}
	}

	if ( ! is_absent) {
		it = Collections.Private.find(key);
		if (it != Collections.Private.end()) {
			++cMatches;
			AdTypes pvtType = (whichAds == STARTD_AD) ? STARTD_PVT_AD : whichAds;
			if (IsDebugCategory(D_STATUS)) {
				const char * opname = expire_ads ? "**Expiring" : "**Removing";
				dprintf(D_STATUS, "%s %s ad key='%s'\n", opname, AdTypeName(pvtType), key.c_str());
			}
			it->second.DeleteAd(whichAds, 0, 0, 0, expire_ads);
			if ( ! expire_ads) Collections.Private.erase(it);
		}
	}

	return cMatches;
}

// delete or exprire ads that match the delete_if constraint
int collect_delete(int operation, AdTypes whichAds, ConstraintHolder & delete_if)
{
	dprintf(D_FULLDEBUG, "collect_delete(%s, %s) expr: %s\n",
		CollectOpName(operation), AdTypeName(whichAds), delete_if.c_str());

	if (whichAds == NO_AD)
		return 0;

	AdTypes key_aty = has_public_adtype(whichAds); // make sure we use the public adtype, this is how private ads are keyed.
	AdCollection::iterator begin = adtype_begins(key_aty, Collections.Public);

	bool expire_ads = (operation == COLLECT_OP_EXPIRE);
	bool single_adtype = (whichAds < 0 || whichAds != ANY_AD);
	bool private_also = ! single_adtype || has_private_adtype(whichAds) != NO_AD;
	int cRemoved = 0;
	int cPvtRemoved = 0;

	AdCollection::iterator it, jt;
	for (it = begin; it != Collections.Public.end(); it = jt) {
		jt = it; ++jt;

		// if deleting only a specific adtype, we can stop as soon as we see an ad of the wrong type
		if (single_adtype && (whichAds != it->second.adType)) {
			break;
		}

		if (EvalBool(it->second.ad, delete_if.Expr())) {
			if (private_also) {
				AdCollection::iterator pt = Collections.Private.find(it->first);
				if (pt != Collections.Private.end()) {
					pt->second.DeleteAd(whichAds, 0, 0, 0, expire_ads);
					if ( ! expire_ads) Collections.Private.erase(pt);
					++cPvtRemoved;
				}
			}
			it->second.DeleteAd(whichAds, 0, 0, 0, expire_ads);
			if ( ! expire_ads) Collections.Public.erase(it);
			++cRemoved;
		}
	}

	PRAGMA_REMIND("honor HOUSEKEEPING_ON_INVALIDATE")

	return cRemoved;
}


int collect(int operation, AdTypes whichAds, ClassAd * ad, ClassAd * adPvt)
{
	dprintf(D_FULLDEBUG, "collector::collect(%s, %s, %p, %p) %d\n",
		CollectOpName(operation), AdTypeName(whichAds), ad, adPvt, whichAds);

	// make a key for this ad.
	std::string key;
	int got_key = collect_make_key(whichAds, ad, adPvt, key);

	if (whichAds == COLLECTOR_AD) {
		// don't allow an ad with the same key as our self ad to be inserted 
		// into the collection, Instead just return success.
		if (Collections.Self.IsSelf(key)) {
			return 0;
		}
	}

	// update operations require a valid key. If we didn't get one have have to abort.
	bool key_required = operation == COLLECT_OP_REPLACE || operation == COLLECT_OP_MERGE;
	if (key_required && ! got_key) {
		return -3; // ad is malformed, just ignore it
	}

	// invalidate/expire operations can be keyed or constrained
	// handle the constrained expire now.
	bool expire_ads = COLLECT_OP_EXPIRE == operation;
	if (expire_ads || (COLLECT_OP_DELETE == operation)) {

		// handle the constrained delete/expire
		if ( ! got_key) {
			ExprTree * filter = ad->LookupExpr(ATTR_REQUIREMENTS);
			if ( ! filter) {
				dprintf (D_ALWAYS, "Invalidation missing " ATTR_REQUIREMENTS "\n");
				return 0;
			}
			ConstraintHolder delete_if(filter->Copy());

			// An empty adType means don't check the MyType of the ads.
			// This means either the command indicates we're only checking
			// one type of ad, or the query's TargetType is "Any" (match
			// all ad types).
			if (whichAds == GENERIC_AD || whichAds == ANY_AD) {
				std::string target;
				ad->LookupString(ATTR_TARGET_TYPE, target);
				if (MATCH == strcasecmp(target.c_str(), "any")) {
					whichAds = ANY_AD;
				}
			}

			return collect_delete(operation, whichAds, delete_if);
		}

		// handle the keyed delete/expire
		return collect_delete(operation, whichAds, key);
	}

	int lifetime = 0;
	if ( ! ad->LookupInteger( ATTR_CLASSAD_LIFETIME, lifetime)) {
		lifetime = default_lifetime(whichAds);
	}
	time_t now;
	time(&now);
	time_t expire_time = now + lifetime;

	bool merge_ads = (operation == COLLECT_OP_MERGE);

#if 0 // use map::insert to probe for existence
	if (adPvt) {
		AdTypes pvtType = (whichAds == STARTD_AD) ? STARTD_PVT_AD : whichAds;
		std::pair<AdCollection::iterator,bool> found = Collections.Private.insert(AdCollectionPair(key, AdCollectionEntry()));
		if (found.second) { /*dummy item was inserted*/ }
		found.first->second.ReplaceAd(pvtType, AC_ENT_PRIVATE, now, expire_time, adPvt);
	}

	// this either inserts an empty entry, or it finds and existing entry, found.second is a bool that tell us which
	std::pair<AdCollection::iterator,bool> found = Collections.Public.insert(AdCollectionPair(key, AdCollectionEntry()));
	if (found.second) { /*dummy item was inserted*/ }
	found.first->second.ReplaceAd(whichAds, 0, now, expire_time, ad);
#elif 1 // use map::lower_bound to find and hint at insertion point.
	AdCollection::iterator lb;
	if (adPvt) {
		AdTypes pvtType = (whichAds == STARTD_AD) ? STARTD_PVT_AD : whichAds;
		lb = Collections.Private.lower_bound(key);
		if (lb != Collections.Private.end() && !(Collections.Private.key_comp()(key, lb->first))) {
			// updating an ad
			dprintf(D_STATUS, "%s %s ad key='%s'\n", merge_ads ? "**Merging" : "**Replacing", AdTypeName(pvtType), key.c_str());
			lb->second.UpdateAd(pvtType, AC_ENT_PRIVATE, now, expire_time, adPvt, merge_ads);
		} else {
			// inserting an ad
			dprintf(D_STATUS, "**Inserting %s ad key='%s'\n",  AdTypeName(pvtType), key.c_str());
			#ifdef HAS_EMPLACE_HINT // emplace is supported
			Collections.Private.emplace_hint(lb, key, AdCollectionEntry(pvtType, AC_ENT_PRIVATE, now, expire_time, adPvt));
			#else
			Collections.Private.insert(lb, AdCollectionPair(key, AdCollectionEntry(pvtType, AC_ENT_PRIVATE, now, expire_time, adPvt)));
			#endif
		}
	}

	lb = Collections.Public.lower_bound(key);
	if (lb != Collections.Public.end() && !(Collections.Public.key_comp()(key, lb->first))) {
		// updating an ad
		dprintf(D_STATUS, "%s %s ad key='%s'\n", merge_ads ? "**Merging" : "**Replacing", AdTypeName(whichAds), key.c_str());
		lb->second.UpdateAd(whichAds, 0, now, expire_time, ad, merge_ads);
	} else {
		// inserting an ad
		dprintf(D_STATUS, "**Inserting %s ad key='%s'\n",  AdTypeName(whichAds), key.c_str());
		#ifdef HAS_EMPLACE_HINT // emplace is supported
		Collections.Public.emplace_hint(lb, key, AdCollectionEntry(whichAds, 0, now, expire_time, ad));
		#else
		Collections.Public.insert(lb, AdCollectionPair(key, AdCollectionEntry(whichAds, 0, now, expire_time, ad)));
		#endif
	}
#else
	AdCollection::iterator found;
	if (adPvt) {
		AdTypes pvtType = (whichAds == STARTD_AD) ? STARTD_PVT_AD : whichAds;
		found = Collections.Private.find(key);
		if (found != Collections.Private.end()) {
			// updating an ad.
			ASSERT(found->second.adType == pvtType);
			found->second.ReplaceAd(pvtType, AC_ENT_PRIVATE, now, expire_time, adPvt);
		} else {
			// inserting the ad
			#ifdef HAS_EMPLACE_HINT // emplace is supported
			Collections.Private.emplace(key, AdCollectionEntry(pvtType, AC_ENT_PRIVATE, now, expire_time, adPvt));
			#else
			std::pair<AdCollection::iterator,bool> ret = Collections.Private.insert(AdCollectionPair(key, AdCollectionEntry()));
			ASSERT(ret.second);
			found = ret.first;
			found->second.ReplaceAd(pvtType, AC_ENT_PRIVATE, now, expire_time, adPvt);
			#endif
		}
	}

	found = Collections.Public.find(key);
	if (found != Collections.Public.end()) {
		// updating an ad.
		ASSERT(found->second.adType == whichAds);
		found->second.ReplaceAd(whichAds, 0, now, expire_time, ad);
	} else {
		// inserting the ad
		#ifdef HAS_EMPLACE_HINT // if emplace is supported
		Collections.Public.emplace(key, AdCollectionEntry(whichAds, 0, now, expire_time, ad));
		#else
		std::pair<AdCollection::iterator,bool> ret = Collections.Public.insert(AdCollectionPair(key, AdCollectionEntry()));
		ASSERT(ret.second);
		found = ret.first;
		found->second.ReplaceAd(whichAds, 0, now, expire_time, ad);
		#endif
	}
#endif

	return 0;
}

// make a std collection iterate like a stringlist. (sigh)
class CollectionIterFromCollection : public CollectionIterator<ClassAd*>
{
public:
	std::map<std::string, AdCollectionEntry>::iterator begin;
	std::map<std::string, AdCollectionEntry>::iterator end;
	std::map<std::string, AdCollectionEntry>::iterator it;
	ClassAd * current;
	AdTypes adtype;
	CollectionIterFromCollection(std::map<std::string, AdCollectionEntry> * lst, AdTypes aty);
	virtual ~CollectionIterFromCollection() {};
	virtual bool IsEmpty() const { return begin == end; }
	virtual void Rewind() { it = begin; if (it == end) { current = NULL; } else { current = it->second.ad; } }
	virtual bool AtEnd() const { return it == end; }
	virtual ClassAd* Current() const { return current; }
	virtual ClassAd* Next () {
		if (it == end) {
			current = NULL;
		} else {
			current = it->second.ad;
			dprintf(D_STATUS | D_VERBOSE, "iter::Next() for adtype=%d current=%s\n", (int)adtype, it->first.c_str());
			ASSERT(adtype == ANY_AD || adtype == it->second.adType);
			++it;
		}
		return current;
	}
};

CollectionIterFromCollection::CollectionIterFromCollection(std::map<std::string, AdCollectionEntry> * lst, AdTypes aty)
	//: lst(p)
	: begin(lst->begin())
	, end(lst->end())
	, it(lst->begin())
	, current(NULL)
	, adtype(aty)
{
	dprintf(D_STATUS | D_VERBOSE, "Creating iterator for %s ads [%d]\n",  AdTypeName(adtype), adtype);
	if (adtype == NO_AD) { adtype = ANY_AD; }
	if (adtype != ANY_AD) {
		AdTypes key_aty = (adtype == STARTD_PVT_AD) ? STARTD_AD : adtype; // private ads are keyed the same as startd ads.
		std::string key;
		formatstr(key, ADTYPE_FMT "/", key_aty);
		begin = lst->lower_bound(key);
		formatstr(key, ADTYPE_FMT "/", key_aty+1);
		end = lst->lower_bound(key);
	}
	dprintf(D_STATUS | D_VERBOSE, "iter begin: '%s' end+1: '%s'\n",
		(begin != end) ? begin->first.c_str() : "<begin>",
		(end != lst->end()) ? end->first.c_str() : "<end>"
		);
}

CollectionIterator<ClassAd*>* collect_get_iter(AdTypes whichAds)
{
	if (whichAds == STARTD_PVT_AD) {
		return new CollectionIterFromCollection (&Collections.Private, STARTD_PVT_AD); // the private collection contains only STARTD_PVT_AD ads.
	} else {
		return new CollectionIterFromCollection (&Collections.Public, whichAds);
	}
}

void collect_free_iter(CollectionIterator<ClassAd*>* it)
{
	CollectionIterFromCollection * cit = reinterpret_cast<CollectionIterFromCollection*>(it);
	delete cit;
}

void calc_collection_totals(struct CollectionCounters & counts)
{
	counts.reset();
	std::map<std::string, AdCollectionEntry>::iterator it;
	for (it = Collections.Public.begin(); it != Collections.Public.end(); ++it) {
		if (it->second.adType > SUBMITTOR_AD) break; // no point in iterating the whole list.
		switch (it->second.adType) {

			case SUBMITTOR_AD: {
				counts.submitterNumAds++;
				int running=0, idle=0;
				ClassAd * ad = it->second.ad;
				if (ad && ad->LookupInteger(ATTR_RUNNING_JOBS, running) && ad->LookupInteger(ATTR_IDLE_JOBS, running)) {
					counts.submitterRunningJobs += running;
					counts.submitterIdleJobs += idle;
				}
			}
			break;

			case STARTD_AD: {
				counts.startdNumAds++;
				char state[10];
				ClassAd * ad = it->second.ad;
				if (ad && ad->LookupString(ATTR_STATE, state, COUNTOF(state))) {
					counts.machinesTotal++;
					switch (state[0]) {
					case 'C': counts.machinesClaimed++; break;
					case 'U': counts.machinesUnclaimed++; break;
					case 'O': counts.machinesOwner++; break;
					}

					int universe;
					if (ad->LookupInteger(ATTR_JOB_UNIVERSE, universe) && universe > 0 && universe < CONDOR_UNIVERSE_MAX) {
						counts.jobsByUniverse[universe] += 1;
					}
				}
			}
			break;

			default: // nothing to do here, just have a default to shut up an idiotic g++ warning.
			break;
		}
	}
}

