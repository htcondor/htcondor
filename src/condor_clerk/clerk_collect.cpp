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
#include "classad_merge.h"
#include "classad_collection.h"


// some platforms (rhel6) don't have emplace yet.
//#define HAS_EMPLACE_HINT

#include "clerk_utils.h"
#include "clerk_collect.h"
#include <map>
#include "tokener.h"


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
};


typedef struct AdLogKey {
	long long uid1;
	long long uid2;
	// a LessThan operator suitable for inserting into a sorted map or set
	bool operator<(const AdLogKey& cp) const {
		int diff = this->uid1 - cp.uid1;
		if ( ! diff) diff = this->uid2 - cp.uid2;
		return diff < 0;
	}
	AdLogKey() : uid1(0), uid2(0) {}
	AdLogKey(long long a, long long b) : uid1(a), uid2(b) {}
	//AdLogKey(const PROC_ID & rhs) : uid1(rhs.cluster), uid2(rhs.proc) {}
	// constructing AdLogKey(NULL) ends up calling this constructor because there is no single int constructor - ClassAdLog depends on that...
	AdLogKey(const char * id_str) : uid1(0), uid2(0) { if (id_str) set(id_str); }
	void sprint(MyString &s) const;
	bool set(const char * id_str);
	static unsigned int hash(const AdLogKey &);
} AdLogKey;

unsigned int AdLogKey::hash(const AdLogKey & alk) {
	return (unsigned int)alk.uid1 ^ (unsigned int)alk.uid2;
}

void AdLogKey::sprint(MyString &s) const {
	s.formatstr("%llx:%llx", uid1, uid2);
}
bool AdLogKey::set(const char * id_str) {
	char *endptr = NULL;
	uid1 = strtoll(id_str, &endptr, 16);
	if (endptr && *endptr == ':') {
		uid2 = strtoll(endptr+1, &endptr, 16);
	} else {
		uid2 = 0;
	}
	return true;
}

inline bool operator==( const AdLogKey a, const AdLogKey b) { return a.uid1 == b.uid1 && a.uid2 == b.uid2; }

class PersistAd : public ClassAd {
public:
	PersistAd() {};
	virtual ~PersistAd() {};
};

typedef PersistAd* AdLogPayload;
typedef ClassAdLog<AdLogKey, const char*,PersistAd*> AdLogType;

// force instantiation of templates that we need.
// template class ClassAdLog<AdLogKey,const char*,PersistAd*>;

template <>
class ConstructClassAdLogTableEntry<PersistAd*> : public ConstructLogEntry
{
public:
	virtual compat_classad::ClassAd* New() const {
		return new PersistAd();
	};
	virtual void Delete(compat_classad::ClassAd*& ad) const {
		if ( ! ad) return;
		PersistAd * pad = (PersistAd*)ad;
		delete pad;
	}
};

class PersistLogHolder {
public:
	PersistLogHolder() : _log(NULL) {}
	virtual ~PersistLogHolder () { delete _log; _log = NULL; };
	const MyString & Filename() { return _filename; }
	void Load(const char * filename, int max_logs) {
		_filename = filename;
		_log = new AdLogType(filename, max_logs, new ConstructClassAdLogTableEntry<PersistAd*>());
	}
	bool Enabled() { return _log != NULL; }
	int SaveChanges(ClassAd &ad, const std::string & key);
	int MergeAd(ClassAd &ad, const std::string & key);
	int ReplaceAd(ClassAd &ad, const std::string & key);
	int RemoveAd(const std::string & key);
protected:
	MyString _filename;
	AdLogType * _log;
};

// To be compatible with the Collector, the offline log is an ordinary ClassAdCollection
//
class OfflineLogHolder {
public:
	OfflineLogHolder() : _ads(NULL) {}
	virtual ~OfflineLogHolder () { delete _ads; _ads = NULL;  }
	void Load(const char * filename, int max_logs) {
		_filename = filename;
		_ads = new ClassAdCollection (NULL, filename, max_logs);
	}
	const MyString & Filename() { return _filename; }
	bool Enabled() { return _ads != NULL; }
	// set default absent ads lifetime.
	void SetLifetime(time_t lifetime) { _lifetime = lifetime; }
	time_t Lifetime() { return _lifetime; }
	// iteration
	ClassAd * First() {
		if ( ! _ads) return NULL;
		_ads->StartIterateAllClassAds();
		return Next();
	}
	ClassAd * Next() {
		if ( ! _ads) return NULL;
		ClassAd * ad = NULL;
		if (_ads->IterateAllClassAds (ad)) { return new ClassAd(*ad); }
		return NULL;
	}
	//int Update(CollectStatus & status, bool force_offline);
	int ReplaceAd(ClassAd &ad, const std::string & key);
	int RemoveAd(const std::string & key);
protected:
	MyString _filename;
	ClassAdCollection *_ads;
	time_t _lifetime;  // absent lifetime
	const char * makeLogKey(const std::string & key, MyString & temp_key);
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
	bool         forwarding;
	bool         always_merge;
	char         persist_log_id; // if non-zero indicates which persist log to use.
} SettingsByAdType;

int maxAdSettings = 200 + ((NUM_AD_TYPES/100)*100);
const int minDynamicAd = NUM_AD_TYPES + (0x20 - (NUM_AD_TYPES&0x1F)); // lowest AdTypes value for a dynamically configured ad
int topDynamicAd    = minDynamicAd; // hightest used AdTypes value for a dynamically configured ad
SettingsByAdType * AdSettings = NULL;
int maxOldAdSettings = 0;
SettingsByAdType * OldAdSettings = NULL;
NOCASE_STRING_TO_INT_MAP MyTypeToAdType; // map MyType to adType so we can preserve dynamic adtype ids across reconfig

struct {
	AdCollection Public;
	AdCollection Private;
	SelfCollection Self;
	OfflineLogHolder Offline;
	PersistLogHolder Persist;
	ConstraintHolder AbsentReq; // requirements for turning ads into absent ads.
	ConstraintHolder NotAbsent; // holds the expression 'Absent =!= true' so we can copy it
	classad::References ForwardingWatchList; // agressively forward when attrs in this list change
	time_t AbsentLifetime;
	bool HOUSEKEEPING_ON_INVALIDATE; // when true, invalidate by expression should be done by the housekeeper.
} Collections;

typedef struct StdAdInfoItem {
	bool bigTable;
	bool hasPrivate;
	bool forwarding;
	unsigned char kfUse;
} StdAdInfoItem;

#define bigtable 1
#define hasPeer  1
#define forward  1
static StdAdInfoItem StandardAdInfoX[] = {
	{0       , 0      , 0      , kfName | kfSchedd },		// QUILL_AD,
	{bigtable, hasPeer, forward, kfName | kfAddr | kfHost },// STARTD_AD,
	{0       , 0      , 0      , kfName | kfAddr | kfHost },// SCHEDD_AD,
	{bigtable, 0      , 0      , kfName | kfAddr | kfHost },// MASTER_AD,
	{0       , 0      , 0      , kfName },					// GATEWAY_AD,
	{0       , 0      , 0      , kfMachine },				// CKPT_SRVR_AD,
	{bigtable, hasPeer, forward, kfName | kfAddr | kfHost },// STARTD_PVT_AD,
	{bigtable, 0      , forward, kfName | kfSchedd | kfAddr | kfHost },	// SUBMITTOR_AD,
	{0       , 0      , 0      , kfName | kfMachine },		// COLLECTOR_AD,
	{0       , 0      , 0      , kfName | kfAddr | kfHost },// LICENSE_AD,
	{0       , 0      , 0      , kfName },					// STORAGE_AD,
	{bigtable, 0      , 0      , kfName },					// ANY_AD,
	{0       , 0      , 0      , kfName },					// BOGUS_AD
	{0       , 0      , 0      , kfName },					// CLUSTER_AD,
	{0       , 0      , 0      , kfName },					// NEGOTIATOR_AD,
	{0       , 0      , 0      , kfName },					// HAD_AD,
	{bigtable, 0      , 0      , kfName },					// GENERIC_AD,
	{0       , 0      , 0      , kfName },					// CREDD_AD,
	{0       , 0      , 0      , kfName },					// DATABASE_AD,
	{0       , 0      , 0      , kfName },					// DBMSD_AD,
	{0       , 0      , 0      , kfName },					// TT_AD,
	{0       , 0      , 0      , kfHash | kfSchedd },		// GRID_AD,
	{0       , 0      , 0      , kfName },					// XFER_SERVICE_AD,
	{0       , 0      , 0      , kfName },					// LEASE_MANAGER_AD,
	{0       , 0      , 0      , kfName },					// DEFRAG_AD,
	{bigtable, 0      , 0      , kfName },					// ACCOUNTING_AD,
};
#undef bigtable
#undef hasPeer
#undef forward

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
	if (typ >= 0 && typ < maxAdSettings) {
		return AdSettings[typ].bigTable;
	}
	return false;
}

bool IsForwarding(AdTypes typ) {
	if (typ >= 0 && typ < maxAdSettings) {
		return AdSettings[typ].forwarding;
	}
	return false;
}

bool CanOffline(AdTypes typ) {
	if (typ >= 0 && typ < maxAdSettings) {
		return AdSettings[typ].persist_log_id == 1;
	}
	return false;
}

const char * NameOf(AdTypes typ) {
	if (typ >= 0 && typ < maxAdSettings) {
		return AdSettings[typ].name ? AdSettings[typ].name : "null";
	}
	return "Invalid";
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
	int lifetime = param_integer ("ABSENT_EXPIRE_ADS_AFTER", def_lifetime);
	if (0 == lifetime) lifetime = INT_MAX; // 0 means forever
	Collections.AbsentLifetime = lifetime;

	Collections.AbsentReq.set(param("ABSENT_REQUIREMENTS"));
	Collections.NotAbsent.set(strdup(ATTR_ABSENT " =!= true"));
	dprintf (D_ALWAYS,"ABSENT_REQUIREMENTS = %s\n", Collections.AbsentReq.c_str());

	lifetime = param_integer("OFFLINE_EXPIRE_ADS_AFTER", INT_MAX);
	Collections.Offline.SetLifetime(lifetime);

	Collections.ForwardingWatchList.clear();
	param_and_insert_attrs("COLLECTOR_FORWARD_WATCH_LIST", Collections.ForwardingWatchList);
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
		sat[aty].always_merge = false;
		ASSERT(aty < (int)COUNTOF(StandardAdInfoX));
		sat[aty].bigTable = StandardAdInfoX[aty].bigTable;
		sat[aty].forwarding = StandardAdInfoX[aty].forwarding;
		sat[aty].keyAttrUse = StandardAdInfoX[aty].kfUse;
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
		sat[aty].forwarding = false;
		sat[aty].always_merge = false;
	}
}

char collect_register_persist_log(AdTypes /*whichAds*/, const char * persist_log)
{
	char log_id = 0;

	if (Collections.Offline.Filename() == persist_log) {
		log_id = 1;
	} else if (Collections.Persist.Filename() == persist_log) {
		log_id = 2;
	} else if (Collections.Persist.Filename().empty()) {
		Collections.Persist.Load(persist_log, 2);
		log_id = 2;
	}

	return log_id;
}

// initialize the offline ads log, and load the offline ads.
// note: we expect this to happend only on startup. not on reconfig.
void offline_ads_load(const char * logfile)
{
	Collections.Offline.Load(logfile, 2);
}

// populate the ads collection from the offline ads
// this can happen both and startup and on reconfig
void offline_ads_populate()
{
	if ( ! Collections.Offline.Enabled())
		return;

	for (ClassAd * ad = Collections.Offline.First(); ad; ad = Collections.Offline.Next()) {
		CollectStatus status;
		int rval = collect(COLLECT_OP_REPLACE | COLLECT_OP_NO_OFFLINE, STARTD_AD, ad, NULL, status);
		if (rval < 0) {
			if (rval == -3)
			{
				/* this happens when we get a classad for which a hash key could
				not been made. This occurs when certain attributes are needed
				for the particular catagory the ad is destined for, but they
				are not present in the ad. */
				dprintf (D_ALWAYS, "Received malformed ad from Absent log. Ignoring.\n");
			}

			delete ad;
		}
	}
}


AdTypes collect_lookup_mytype(const char * mytype)
{
	int whichAds = NO_AD;
	NOCASE_STRING_TO_INT_MAP::iterator found = MyTypeToAdType.find(mytype);
	if (found != MyTypeToAdType.end()) {
		whichAds = found->second;
	} else if (MATCH == strcasecmp("any", mytype)) {
		whichAds = ANY_AD;
	}
	return (AdTypes)whichAds;
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
void collect_set_adtype(AdTypes whichAds, const char * keylist, double lifetime, unsigned int flags, const char * persist)
{
	if (whichAds < 0 || whichAds >= maxAdSettings) return;

	SettingsByAdType & mysat = AdSettings[whichAds];

	bool big_table    = (flags & COLL_F_FORK) != 0;
	bool forward      = (flags & COLL_F_FORWARD) != 0;
	bool always_merge = (flags & COLL_F_ALWAYS_MERGE) != 0;

	if (flags & COLL_SET_LIFETIME) mysat.lifetime = (time_t)lifetime;
	if (flags & COLL_SET_FORK) mysat.bigTable = big_table;
	if (flags & COLL_SET_FORWARD) mysat.forwarding = forward;
	if (flags & COLL_SET_ALWAYS_MERGE) mysat.always_merge = always_merge;

	mysat.persist_log_id = 0;
	if (persist) {
		mysat.persist_log_id = collect_register_persist_log(whichAds, persist);
	} else if (whichAds == STARTD_AD) {
		PRAGMA_REMIND("for now, defaulting STARTD_AD to use the offline log if it is enabled.")
		if (Collections.Offline.Enabled()) {
			AdSettings[STARTD_AD].persist_log_id = 1;
		}
	}

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

// set forwarding flags based on the given adTypes
void collect_set_forwarding_adtypes(classad::References & viewAdTypes)
{
	for (int ii = 0; ii < maxAdSettings; ++ii) {
		const char * name = AdSettings[ii].name;
		if ( ! name) name = AdTypeName((AdTypes)ii);
		if ( ! name) continue;
		AdSettings[ii].forwarding = viewAdTypes.find(name) != viewAdTypes.end();
	}
}

/*
typedef struct _settings_by_adtype {
	const char * name;       // typename for the ads of this type
	const char * zzkeys;     // override of keys for ads of this type
	time_t       lifetime;   // lifetime to use for this adtype if the ad itself does not specify
	unsigned int keyAttrUse; // 'known' attributes that this adtype uses to make it's keys
	bool         bigTable;
	bool         forwarding;
	bool         always_merge;
	char         persist_log_id; // if non-zero indicates which persist log to use.
} SettingsByAdType;

*/

void collect_finalize_ad_tables()
{
	std::string buffer;
	dprintf(D_FULLDEBUG, "Ad tables\n");
	dprintf(D_FULLDEBUG, "_ID_ _____NAME_____ FLAGS LIFETIME KEYS\n");
	for (int ii = 0; ii < maxAdSettings; ++ii) {
		const SettingsByAdType &as = AdSettings[ii];
		if ( ! as.name) continue;
		buffer.clear();
		formatstr(buffer, "[%2d] %-14s %c%c%c%d  ", ii, as.name, as.bigTable?'B':' ', as.forwarding?'F':' ', as.always_merge?'M':' ', as.persist_log_id);
		if (as.lifetime < 10000000 && as.lifetime > -10000000) {
			formatstr_cat(buffer, "%8u", (unsigned int)as.lifetime);
		} else {
			buffer += "INFINITE";
		}
		formatstr_cat(buffer, " 0x%04X", as.keyAttrUse);
		if (as.zzkeys) {
			for (const char * p = as.zzkeys; (p && *p); p += strlen(p)+1) {
				buffer += " ";
				buffer += p;
			}
		}
		dprintf(D_FULLDEBUG, "%s\n", buffer.c_str());
	}
}

int default_lifetime(AdTypes whichAds)
{
	if (whichAds > 0 && whichAds < maxAdSettings) { return AdSettings[whichAds].lifetime; }
	return 1; // 1 means expire on next housecleaning pass.
}

bool should_forward(AdTypes whichAds)
{
	if (whichAds > 0 && whichAds < maxAdSettings) { return AdSettings[whichAds].forwarding; }
	return 1; // 1 means expire on next housecleaning pass.
}

bool always_merge(AdTypes whichAds)
{
	if (whichAds > 0 && whichAds < maxAdSettings) { return AdSettings[whichAds].always_merge; }
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
	if (Collections.AbsentReq.empty()) {
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

int PersistLogHolder::ReplaceAd(ClassAd &ad, const std::string & ad_key)
{
	MyString s;

	if ( ! _log) return 0;

	// Convert the input key to one compatible with the ClassAdLog
	PRAGMA_REMIND("make a persist key from the ad_key")
	AdLogKey key(ad_key.c_str());
	MyString keystr;
	key.sprint(keystr);
	const char * pkey = keystr.Value();

	_log->BeginTransaction ();

	/* replace duplicate ads */

	PersistAd *old_ad = NULL;
	if (_log->table.lookup(key, old_ad)) {
		_log->AppendLog(new LogDestroyClassAd(pkey, _log->GetTableEntryMaker()));
		dprintf(D_FULLDEBUG, "PersistLogHolder::ReplaceAd: Replacing existing ad.\n");
	}

	// add the new ad

	const char * mytype = GetMyTypeName(ad);
	const char * targettype = GetTargetTypeName(ad);
	_log->AppendLog(new LogNewClassAd(keystr.c_str(), mytype, targettype, _log->GetTableEntryMaker()));

	classad::ClassAdUnParser unparser;
	unparser.SetOldClassAd( true, true );
	std::string value;

	const char *name;
	ExprTree* expr;
	ad.ResetExpr();
	while (ad.NextExpr(name, expr)) {
		value.clear();
		unparser.Unparse(value, expr);
		_log->AppendLog(new LogSetAttribute(pkey, name, value.c_str()));
	}

	_log->CommitTransaction ();

	dprintf(D_ALWAYS,"Added ad to persistent store key=%s\n", pkey);

	return 0;
}

int PersistLogHolder::RemoveAd(const std::string & ad_key)
{
	AdLogKey key(ad_key.c_str());
	MyString keystr;
	key.sprint(keystr);
	const char * pkey = keystr.Value();

	PersistAd *old_ad = NULL;
	if (_log->table.lookup(key, old_ad)) {
		_log->AppendLog(new LogDestroyClassAd(pkey, _log->GetTableEntryMaker()));
		dprintf(D_FULLDEBUG, "PersistLogHolder::RemoveAd: Replacing existing ad.\n");
	}
	return 0;
}

int PersistLogHolder::SaveChanges (ClassAd &ad, const std::string & ad_key)
{
	if ( ! _log) return 0;

	PRAGMA_REMIND("make a persist key from the ad_key")
	AdLogKey key(ad_key.c_str());
	MyString keystr;
	key.sprint(keystr);

	_log->BeginTransaction ();

	PersistAd *old_ad = NULL;
	classad::ClassAdUnParser unparser;
	unparser.SetOldClassAd( true, true );

	if ( !_log->table.lookup(key, old_ad)) {
		const char * mytype = GetMyTypeName(ad);
		const char * targettype = GetTargetTypeName(ad);
		_log->AppendLog(new LogNewClassAd(keystr.c_str(), mytype, targettype, _log->GetTableEntryMaker()));
	}

	std::string new_val;
	std::string old_val;

	ad.ResetExpr();
	ExprTree *expr;
	const char *attr_name;
	while (ad.NextExpr(attr_name, expr)) {

		ASSERT( attr_name && expr );

		new_val.clear();
		unparser.Unparse( new_val, expr );

		if (old_ad) {
			old_val.clear();
			expr = old_ad->LookupExpr( attr_name );
			if( expr ) {
				unparser.Unparse( old_val, expr );
				if( new_val == old_val ) {
					continue;
				}
			}

				// filter out stuff we never want to mess with
			if( !strcasecmp(attr_name,ATTR_MY_TYPE) ||
				!strcasecmp(attr_name,ATTR_TARGET_TYPE) ||
				!strcasecmp(attr_name,ATTR_AUTHENTICATED_IDENTITY) )
			{
				continue;
			}
		}

		_log->AppendLog(new LogSetAttribute(keystr.Value(), attr_name, new_val.c_str()));
	}

	_log->CommitTransaction ();
	return 0;
}

int PersistLogHolder::MergeAd (ClassAd &ad, const std::string & ad_key)
{
	if (!_log) return 0;

	PRAGMA_REMIND("make a persist key from the ad_key")
	AdLogKey key(ad_key.c_str());
	MyString keystr;
	key.sprint(keystr);

	classad::ClassAdUnParser unparser;
	unparser.SetOldClassAd( true, true );

	PersistAd *old_ad = NULL;
	if ( !_log->table.lookup(key, old_ad)) {
		PRAGMA_REMIND("report not found for merge?")
		return 0;
	}

	_log->BeginTransaction ();

	ad.ResetExpr();
	ExprTree *expr;
	const char *attr_name;
	while (ad.NextExpr(attr_name, expr)) {
		std::string new_val;
		std::string old_val;

		ASSERT( attr_name && expr );

		unparser.Unparse( new_val, expr );

		expr = old_ad->LookupExpr( attr_name );
		if( expr ) {
			unparser.Unparse( old_val, expr );
			if( new_val == old_val ) {
				continue;
			}
		}

			// filter out stuff we never want to mess with
		if( !strcasecmp(attr_name,ATTR_MY_TYPE) ||
			!strcasecmp(attr_name,ATTR_TARGET_TYPE) ||
			!strcasecmp(attr_name,ATTR_AUTHENTICATED_IDENTITY) )
		{
			continue;
		}

		_log->AppendLog(new LogSetAttribute(keystr.Value(), attr_name, new_val.c_str()));
	}

	_log->CommitTransaction ();
	return 0;
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

time_t AdCollectionEntry::ForwardBeforeTime(time_t now)
{
	time_t rval = INT_MAX;  // forward never
	if (should_forward(adType)) {
		// forward dirty watch attrs now.
		if (this->dirty & AC_DIRTY_WATCH) {
			rval = this->forwardTime + 5;
		// forward normal updates before the ad expires.
		} else if (this->dirty) {
			time_t lifetime = this->expireTime - this->updateTime;
			rval = this->forwardTime + lifetime/3;
		}
	}
	dprintf(D_FULLDEBUG, "Checking ForwardBeforeTime adType=%s dirty=0x%x returns %lld, now=%lld lastforward=%lld\n",
		NameOf(adType), this->dirty, (long long)rval, (long long)now, (long long)this->forwardTime);
#if 1
	PRAGMA_REMIND("Add delayed forwarding??")
	// for now, always force forwarding to happen right away.
	if (rval != INT_MAX) rval = now - 1;
#endif
	return rval;
}

void AdCollectionEntry::Forwarded(time_t now)
{
	this->dirty = 0;
	this->forwardTime = now;
}


int AdCollectionEntry::UpdateAd(AdTypes atype, int new_flags, time_t now, time_t expire_time, compat_classad::ClassAd* new_ad, bool merge_it /*=false*/)
{
	if (merge_it) {
		this->flags |= new_flags; // something more complex needed here?
		this->dirty |= AC_DIRTY_MERGE;
	} else {
		this->adType = atype;
		this->flags = new_flags; // something more complex needed here?
		this->updateTime = now;
		this->expireTime = expire_time;

		this->dirty |= AC_DIRTY_TIME;
		if (should_forward(adType) && ! (this->dirty & AC_DIRTY_WATCH)) {
			// check for changes in the watched attrs
			classad::Value old_val, new_val;
			for (classad::References::iterator it = Collections.ForwardingWatchList.begin(); it != Collections.ForwardingWatchList.end(); ++it) {
				// NOTE: this treats attribute-not-present and attribute-evaluates-to-UNDEFINED as equivalent
				if (this->ad->EvaluateAttr(*it, old_val) && new_ad->EvaluateAttr(*it, new_val) && ! new_val.SameAs(old_val)) {
					this->dirty |= AC_DIRTY_WATCH;
					break;
				};
			}
		}
	}

	if (merge_it) {
		if (this->ad != new_ad) {
			static const char * const dont_merge_these[] = { ATTR_AUTHENTICATED_IDENTITY, ATTR_MY_TYPE, ATTR_TARGET_TYPE };
			AttrNameSet dont_merge_attrs(dont_merge_these, dont_merge_these+(sizeof(dont_merge_these)/sizeof(const char *)));
			MergeClassAdsIgnoring(this->ad, new_ad, dont_merge_attrs, false);
		}
	} else if (this->ad != new_ad) {
		delete this->ad; this->ad = NULL;
		this->ad = new_ad;
	}
	return 0;
}

// remove this ad from the Persistent log
// returns true if the ad was removed, or if there is no persist log
bool AdCollectionEntry::PersistentRemoveAd(AdTypes adtype, const std::string & key)
{
	if (adtype < 0 || adType >= maxAdSettings)
		return false;

	int log_id = AdSettings[adtype].persist_log_id;
	if ( ! log_id)
		return false;

	if (log_id == 1) {
		Collections.Offline.RemoveAd(key);
		return true;
	} else if (log_id == 2) {
		Collections.Persist.RemoveAd(key);
		return true;
	}
	return false;
}

// Write this ad to the persist log
// returns true if the ad was saved, or if there is no persist log
bool AdCollectionEntry::PersistentStoreAd(AdTypes adtype, const std::string & key)
{
	if (adtype < 0 || adType >= maxAdSettings)
		return false;

	int log_id = AdSettings[adtype].persist_log_id;
	if ( ! log_id)
		return false;

	if (log_id == 1) {
		Collections.Offline.ReplaceAd(*ad, key);
		return true;
	} else if (log_id == 2) {
		Collections.Persist.ReplaceAd(*ad, key);
	}
	return true;
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

		this->updateTime = now;
		this->expireTime = now + lifetime;

		PersistentStoreAd(adtype, key);

		// if we marked this ad as absent, we want to keep it in the collector
		return true;	// return true tells the collector to KEEP this ad
	}

	return false; // return false tells collector to delete this ad
}

bool AdCollectionEntry::CheckOffline(AdTypes whichAds, const std::string key, bool force_offline)
{
	if ( ! CanOffline(whichAds) || ! ad)
		return false;

	int offline = false;
	bool offline_explicit = false;
	if( ad->EvalBool( ATTR_OFFLINE, NULL, offline ) ) {
		offline_explicit = true;
	}

	if (force_offline && ! offline_explicit) {

		// implicit offlining.
		// Rewrite the ad if it is going offline
		offline = true;
		if (whichAds == STARTD_AD) {
			ConvertStartdAdToOffline(*ad);
			dprintf (D_FULLDEBUG, "Machine ad lifetime: %d\n", (int)Collections.Offline.Lifetime() );
		}

			/* record the new values as specified above */
		ad->Assign (ATTR_OFFLINE, (bool)offline);
		if (Collections.Offline.Lifetime() > 0) {
			ad->Assign(ATTR_CLASSAD_LIFETIME, Collections.Offline.Lifetime());
			this->expireTime = this->updateTime + Collections.Offline.Lifetime();
		}
	}

	if (offline > 0) {
		PersistentStoreAd(whichAds, key);
	} else {
		PersistentRemoveAd(whichAds, key);
	}
	return offline > 0;
}

WalkPostOp housekeep_tick(void * pv, AdCollection::iterator & it)
{
	time_t now = *(time_t*)pv;
	const std::string & key = it->first;
	AdCollectionEntry & entry = it->second;

	WalkPostOp retval = Walk_Op_Continue;

	if (entry.expireTime <= now) {

		if (entry.updateTime == 0) {
			dprintf (D_ALWAYS,"**Removing invalidated ad key='%s'\n", key.c_str());
			entry.DeleteAd(entry.adType, 0, 0, 0, false);
			retval = Walk_Op_DeleteItem; // let the walk loop know to erase the item (and delete the private ad if needed)
		} else {
			/* let the off-line plug-in know we are about to expire this ad, so it can
				potentially mark the ad absent. if CheckAbsent() returns false, then delete
				the ad as planned; if it return true, it was likely marked as absent,
				so then this ad should NOT be deleted. */
			bool is_absent = entry.CheckAbsent(entry.adType, key);
			if (is_absent) {
				// don't delete this ad yet.
				dprintf(D_STATUS, "**Absenting %s ad key='%s'\n", AdTypeName(entry.adType), key.c_str());
				retval = Walk_Op_Continue;
			} else {
				dprintf (D_ALWAYS,"**Removing stale ad key='%s'\n", key.c_str());
				entry.DeleteAd(entry.adType, 0, 0, 0, false);
				retval = Walk_Op_DeleteItem; // let the walk loop know to erase the item (and delete the private ad if needed)
			}
		}
	}

	return retval;
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

#if 0
bool OLD_collect_make_key(AdTypes whichAds, const ClassAd* ad, ClassAd *adPvt, std::string & key)
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
#endif // OLD_collect_make_key

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
	WalkPostOp (*callback)(void* pv, AdCollection::iterator & it),
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
			WalkPostOp post = callback(pv, it);
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

// called by the housekeeper to expire ads
//
int collect_clean(time_t now)
{
	ConstraintHolder call_if;
	return collect_walk(ANY_AD, call_if, housekeep_tick, &now);
}

int collect_delete(int op_and_flags, AdTypes whichAds, const std::string & key)
{
	int operation = op_and_flags & 0xFF;
	dprintf(D_FULLDEBUG, "collect_delete(%s, %s) key: %s\n",
		CollectOpName(operation), AdTypeName(whichAds), key.c_str());

	if (whichAds == NO_AD)
		return 0;

	//bool consider_offline =  Collections.Offline.Enabled() && CanOffline(whichAds);
	//if (op_and_flags & COLLECT_OP_NO_OFFLINE) consider_offline = false;

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

	// do the same to Private ads (if there are any)
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
int collect_delete(int op_and_flags, AdTypes whichAds, ConstraintHolder & delete_if)
{
	int operation = op_and_flags & 0xFF;
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


int collect(int op_and_flags, AdTypes whichAds, ClassAd * ad, ClassAd * adPvt, CollectStatus & status)
{
	int operation = op_and_flags & 0xFF;
	dprintf(D_FULLDEBUG, "collector::collect(%s, %s, %p, %p) %d\n",
		CollectOpName(operation), AdTypeName(whichAds), ad, adPvt, whichAds);

	status.reset();

	// make a key for this ad.
	int got_key = collect_make_key(whichAds, ad, adPvt, status.key);

	if (whichAds == COLLECTOR_AD) {
		// don't allow an ad with the same key as our self ad to be inserted 
		// into the collection, Instead just return success.
		if (Collections.Self.IsSelf(status.key)) {
			return 0;
		}
	}

	// update operations require a valid key. If we didn't get one have have to abort.
	bool key_required = operation == COLLECT_OP_REPLACE || operation == COLLECT_OP_MERGE;
	if (key_required && ! got_key) {
		return -3; // ad is malformed, just ignore it
	}

	// if the ad type forwards, for now presume that we should forward this ad.
	// we will do the forward filtering/rate limiting check in UpdateAd
	status.should_forward = should_forward(whichAds);

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
				if (ad->LookupString(ATTR_TARGET_TYPE, target)) {
					whichAds = collect_lookup_mytype(target.c_str());
				}
			}

			return collect_delete(op_and_flags, whichAds, delete_if);
		}

		// handle the keyed delete/expire
		return collect_delete(op_and_flags, whichAds, status.key);
	}

	int lifetime = 0;
	if ( ! ad->LookupInteger( ATTR_CLASSAD_LIFETIME, lifetime)) {
		lifetime = default_lifetime(whichAds);
	}
	time_t now;
	time(&now);
	time_t expire_time = now + lifetime;

	bool merge_ads = (operation == COLLECT_OP_MERGE) || always_merge(whichAds);

#if 1 // use map::lower_bound to find and hint at insertion point.
	AdCollection::iterator lb;
	if (adPvt) {
		AdTypes pvtType = (whichAds == STARTD_AD) ? STARTD_PVT_AD : whichAds;
		lb = Collections.Private.lower_bound(status.key);
		if (lb != Collections.Private.end() && !(Collections.Private.key_comp()(status.key, lb->first))) {
			// updating an ad
			dprintf(D_STATUS, "%s %s ad key='%s'\n", merge_ads ? "**Merging" : "**Replacing", AdTypeName(pvtType), status.key.c_str());
			lb->second.UpdateAd(pvtType, AC_ENT_PRIVATE, now, expire_time, adPvt, merge_ads);
		} else if (merge_ads) {
			dprintf(D_STATUS, "**NOT Merging %s ad key='%s' was not found\n", AdTypeName(pvtType), status.key.c_str());
		} else {
			// inserting an ad
			dprintf(D_STATUS, "**Inserting %s ad key='%s'\n",  AdTypeName(pvtType), status.key.c_str());
			#ifdef HAS_EMPLACE_HINT // emplace is supported
			Collections.Private.emplace_hint(lb, status.key, AdCollectionEntry(pvtType, AC_ENT_PRIVATE, now, expire_time, adPvt));
			#else
			Collections.Private.insert(lb, AdCollectionPair(status.key, AdCollectionEntry(pvtType, AC_ENT_PRIVATE, now, expire_time, adPvt)));
			#endif
		}
	}

	lb = Collections.Public.lower_bound(status.key);
	if (lb != Collections.Public.end() && !(Collections.Public.key_comp()(status.key, lb->first))) {
		// updating an ad
		dprintf(D_STATUS, "%s %s ad key='%s'\n", merge_ads ? "**Merging" : "**Replacing", AdTypeName(whichAds), status.key.c_str());
		lb->second.UpdateAd(whichAds, 0, now, expire_time, ad, merge_ads);
		status.entry = &lb->second;
	} else if (merge_ads) {
		dprintf(D_STATUS, "**NOT Merging %s ad key='%s' was not found\n", AdTypeName(whichAds), status.key.c_str());
	} else {
		// inserting an ad
		dprintf(D_STATUS, "**Inserting %s ad key='%s'\n",  AdTypeName(whichAds), status.key.c_str());
		#ifdef HAS_EMPLACE_HINT // emplace is supported
		Collections.Public.emplace_hint(lb, status.key, AdCollectionEntry(whichAds, 0, now, expire_time, ad));
		#else
		Collections.Public.insert(lb, AdCollectionPair(status.key, AdCollectionEntry(whichAds, 0, now, expire_time, ad)));
		#endif
		status.entry = &Collections.Public[status.key];
	}
#else
	AdCollection::iterator found;
	if (adPvt) {
		AdTypes pvtType = (whichAds == STARTD_AD) ? STARTD_PVT_AD : whichAds;
		found = Collections.Private.find(status.key);
		if (found != Collections.Private.end()) {
			// updating an ad.
			ASSERT(found->second.adType == pvtType);
			found->second.ReplaceAd(pvtType, AC_ENT_PRIVATE, now, expire_time, adPvt);
		} else if (merge_ads) {
		} else {
			// inserting the ad
			#ifdef HAS_EMPLACE_HINT // emplace is supported
			Collections.Private.emplace(status.key, AdCollectionEntry(pvtType, AC_ENT_PRIVATE, now, expire_time, adPvt));
			#else
			std::pair<AdCollection::iterator,bool> ret = Collections.Private.insert(AdCollectionPair(status.key, AdCollectionEntry()));
			ASSERT(ret.second);
			found = ret.first;
			found->second.ReplaceAd(pvtType, AC_ENT_PRIVATE, now, expire_time, adPvt);
			#endif
		}
	}

	found = Collections.Public.find(status.key);
	if (found != Collections.Public.end()) {
		// updating an ad.
		ASSERT(found->second.adType == whichAds);
		found->second.ReplaceAd(whichAds, 0, now, expire_time, ad);
		status.entry = &found->second;
	} else if (merge_ads) {
	} else {
		// inserting the ad
		#ifdef HAS_EMPLACE_HINT // if emplace is supported
		Collections.Public.emplace(status.key, AdCollectionEntry(whichAds, 0, now, expire_time, ad));
		status.entry = &Collections.Public[status.key];
		#else
		std::pair<AdCollection::iterator,bool> ret = Collections.Public.insert(AdCollectionPair(status.key, AdCollectionEntry()));
		ASSERT(ret.second);
		found = ret.first;
		found->second.ReplaceAd(whichAds, 0, now, expire_time, ad);
		status.entry = &found->second;
		#endif
	}
#endif

	// handle updating of offline ads
	if ( ! (op_and_flags & COLLECT_OP_NO_OFFLINE) &&
		Collections.Offline.Enabled() &&
		CanOffline(whichAds) && status.entry) {
		status.entry->CheckOffline(whichAds, status.key, op_and_flags & COLLECT_OP_OFFLINE);
	}

	return 0;
}

const char * OfflineLogHolder::makeLogKey(const std::string & key_in, MyString & temp_key)
{
	if (key_in.find_first_of(" \t\r\n")) {
		temp_key = key_in;
		temp_key.compressSpaces();
		return temp_key.Value();
	}
	return key_in.c_str();
}

int OfflineLogHolder::ReplaceAd(ClassAd & ad, const std::string & key_in)
{
	MyString s;

	if (!_ads) return false;

	// Convert the input key to one compatible with the ClassAdLog
	const char * pkey = makeLogKey(key_in, s);

	_ads->BeginTransaction ();

	/* replace duplicate ads */
	ClassAd *p = NULL;
	if ( _ads->LookupClassAd ( pkey, p ) ) {

		if ( !_ads->DestroyClassAd ( pkey ) ) {
			dprintf (
				D_FULLDEBUG,
				"OfflineLogHolder::StoreAd: "
				"failed remove existing off-line ad from the persistent store.\n" );

			_ads->AbortTransaction ();
			return false;
		}

		dprintf(D_FULLDEBUG, 
			"OfflineLogHolder::StoreAd: "
			"Replacing existing offline ad.\n");
	}

	/* try to add the new ad */
	if ( !_ads->NewClassAd (pkey, &ad)) {

		dprintf (
			D_FULLDEBUG,
			"OfflineLogHolder::StoreAd: "
			"failed add off-line ad to the persistent "
			"store.\n" );

		_ads->AbortTransaction ();
		return false;
	}

	_ads->CommitTransaction ();

	dprintf(D_ALWAYS,"Added ad to persistent store key=%s\n", pkey);

	return true;
}

int OfflineLogHolder::RemoveAd(const std::string & key_in)
{
	ClassAd *p;

	if (!_ads) return false;

	// Convert the input key to one compatible with the ClassAdLog
	MyString s;
	const char * pkey = makeLogKey(key_in, s);

	/* can't remove ads that do not exist */
	if ( !_ads->LookupClassAd ( pkey, p ) ) {
		return false;

	}

	/* try to remove the ad */
	if ( !_ads->DestroyClassAd ( pkey ) ) {

		dprintf (
			D_FULLDEBUG,
			"OfflineLogHolder::RemoveAd: "
			"failed remove off-line ad from the persistent "
			"store.\n" );
		return false;

	}

	dprintf(D_ALWAYS,"Removed ad from persistent store key=%s\n", pkey);

	return true;
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

