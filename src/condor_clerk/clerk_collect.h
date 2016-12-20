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

#ifndef _CLERK_COLLECT_H_
#define _CLERK_COLLECT_H_

//turning this off because it currently crashes the clerk.
//#define WANT_UPDATE_HISTORY 1

// call this to reconfig the collection code
void collect_config(int max_adtype);

// this is called by collect_config, and also early in daemon init
// to setup the tables for the known AdTypes
// and reserve space for dynamic types to be added
void config_standard_ads(int max_adtype);

// this is called at the end of Config so that some things
// about the ad tables can be finalized.
void collect_finalize_ad_tables();

// register a new AdType or lookup the AdType for an existing type name
AdTypes collect_register_adtype(const char * mytype);
// just lookup the AdType for an existing type, returns NO_AD if the name does not match anything.
AdTypes collect_lookup_mytype(const char * mytype);

// adjust settings for an AdType, either new or dynamic
void collect_set_adtype(AdTypes whichAds, const char * keylist, double lifetime, unsigned int flags, const char * persist);
#define COLL_F_FORK           0x01
#define COLL_F_FORWARD        0x02
#define COLL_F_INJECT_AUTH    0x04
#define COLL_F_ALWAYS_MERGE   0x08
// these flag bits indicate which things collect_set_adtype should set.
#define COLL_SET_FORK         (COLL_F_FORK << 16)
#define COLL_SET_FORWARD      (COLL_F_FORWARD << 16)
#define COLL_SET_INJECT_AUTH  (COLL_F_INJECT_AUTH << 16)
#define COLL_SET_ALWAYS_MERGE (COLL_F_ALWAYS_MERGE << 16)
#define COLL_SET_LIFETIME     (0x10 << 16)

// adjust which AdTypes will be forwarded.
void collect_set_forwarding_adtypes(classad::References & viewAdTypes);

void offline_ads_load(const char * logfile);
void offline_ads_populate();

// Returns the private AdType associated with this ad. If The AdType is private, returns itself
// if there is no private AdType, returns NO_AD
AdTypes has_private_adtype(AdTypes whichAds);

// Returns the public AdType for this ad, if the AdType is public, returns itself
AdTypes has_public_adtype(AdTypes whichAds);

// Returns true if the table is large for this adtype
bool IsBigTable(AdTypes whichAds);
// Returns true if we should be inject ATTR_AUTHENTICATED_IDENTITY into the ad
bool ShouldInjectAuthId(AdTypes whichAds);
// Returns name of the adype or null for types in the table that have no name, and Invalid for types not in the table.
const char * NameOf(AdTypes typ);

const char * CollectOpName(int op);

template<typename T> class CollectionIterFromCondorList : public CollectionIterator<T*>
{
public:
	List<T> * lst;
	CollectionIterFromCondorList(List<T>* l) : lst(l) {}
	virtual ~CollectionIterFromCondorList() {};
	virtual bool IsEmpty() const { return lst->IsEmpty(); }
	virtual void Rewind() { lst->Rewind(); }
	virtual T* Next () { return lst->Next(); }
	virtual T* Current() const { return lst->Current(); }
	virtual bool AtEnd() const { return lst->AtEnd(); }
};

class CollectionIterFromClassadList : public CollectionIterator<ClassAd*>
{
public:
	ClassAdList * lst;
	ClassAd * cur;
	CollectionIterFromClassadList(ClassAdList* l) : lst(l), cur(NULL) {}
	virtual ~CollectionIterFromClassadList() {};
	virtual bool IsEmpty() const { return lst->Length() <= 0; }
	virtual void Rewind() { lst->Rewind(); }
	virtual ClassAd* Next () { cur = lst->Next(); return cur; }
	virtual ClassAd* Current() const { return cur; }
	virtual bool AtEnd() const { return cur != NULL; }
};

CollectionIterator<ClassAd*>* collect_get_iter(AdTypes whichAds);
void collect_free_iter(CollectionIterator<ClassAd*> * it);
bool collect_add_absent_clause(ConstraintHolder & filter_if);

class Joinery {
public:
	Joinery(AdTypes adt) : targetAds(adt), join_key_ad(NULL) {};

	AdTypes targetAds;
	std::string lookupType; // name to use in the join_ad to refer to the key ad. i.e the lookup ad type name
	ClassAd* join_key_ad;
	ClassAd join_parent;
	ConstraintHolder foreach_size;
	//std::vector< std::pair<std::string, classad::ExprTree*> > keys;
};

// return a single ad (or null) using the keyAd to construct the lookup key
ClassAd * collect_lookup(AdTypes whichAds, const ClassAd * keyAd);
ClassAd * collect_lookup(const std::string & key);

// return the keys we get from apply the given join to the given index ad.
bool collect_make_keys(AdTypes whichAds, const ClassAd * keyAd, Joinery & join, std::vector<std::string> & keys);

typedef struct AdUpdateCounters {
	// counters of updates
	int initial;
	int total;
	int sequenced;
	int dropped;
	AdUpdateCounters() : initial(0), total(0), sequenced(0), dropped(0) {}
} AdUpdateCounters;

#define AC_ENT_PRIVATE 0x0001
#define AC_ENT_OFFLINE 0x0002
#define AC_ENT_EXPIRED 0x0004

#define AC_DIRTY_TIME  0x0001 // a full update was recieved
#define AC_DIRTY_MERGE 0x0001 // a merge update was recieved
#define AC_DIRTY_WATCH 0x0008 // one of the watch attributes was dirtied

// This structure is what is actually stored in the collector tables
// it is not a class, has no destructor, deep copy, or virtual methods on purpose
// a bit more care is required to use it correctly, but that lowers the memory/copying cost
// consider the additional memory/copy cost carefully before you turn this into a virtual class.
typedef struct AdCollectionEntry {
	AdTypes                  adType;
	int                      flags;  // zero or more of AC_ENT_* flags
	int                      dirty;  // flags indicate ad was changed sinced last housekeeping/forwarding
	int                      sequence; // last sequence number
	time_t                   dstartTime; // daemon start time, used to recognise when we see a restarted daemon.
	time_t                   updateTime; // value of ATTR_LAST_HEARD_FROM
	time_t                   expireTime; // value of ATTR_LAST_HEARD_FROM + ATTR_CLASSAD_LIFETIME
	time_t                   forwardTime; // value of ATTR_LAST_FORWARDED
	compat_classad::ClassAd* ad;
	AdUpdateCounters         updates;
#ifdef WANT_UPDATE_HISTORY
	BarrelShifter<unsigned int, 4> history;
#endif

	AdCollectionEntry() : adType(NO_AD), flags(0), dirty(0), sequence(0), dstartTime(0), updateTime(0), expireTime(0), forwardTime(0), ad(NULL) {}
	AdCollectionEntry(AdTypes t, int f, time_t u, time_t e, compat_classad::ClassAd* a);
	int UpdateAd(AdTypes adtype, int new_flags, time_t now, time_t expire_time, compat_classad::ClassAd* new_ad, bool merge_it=false);
	int DeleteAd(AdTypes adtype, int new_flags, time_t now, time_t expire_time, bool expire_it=false);
	bool CheckAbsent(AdTypes adtype, const std::string & key);
	bool CheckOffline(AdTypes adtype, const std::string key, bool force_offline);
	bool PersistentRemoveAd(AdTypes adtype, const std::string & key);
	bool PersistentStoreAd(AdTypes adtype, const std::string & key);
	time_t ForwardBeforeTime(time_t now);
	void Forwarded(time_t now);
} AdCollectionEntry;

typedef std::map<std::string, AdCollectionEntry> AdCollection;
typedef std::pair<std::string, AdCollectionEntry> AdCollectionPair;

// call this to register the collectors own ad
int collect_self(ClassAd * ad);

// this class holds stats by collection
class CollectionStats {
public:
	CollectionStats() {};
	~CollectionStats() {}
	void resize(int max_adtype);
	void Update(AdCollectionEntry& entry, bool initial, bool is_merge);
protected:
	AdUpdateCounters all;
	std::vector<AdUpdateCounters> updates;
	static const std::string AttrSeqNum;
	static const std::string AttrDStartTime;
	static const std::string AttrTotal;
	static const std::string AttrSequenced;
	static const std::string AttrLost;
	static const std::string AttrHistory;
};

// this class is used return status information from a collect operation
class CollectStatus {
public:
	CollectStatus() : entry(NULL), should_forward(false) {}
	~CollectStatus() {}

	AdCollectionEntry * entry;
	std::string key;
	bool should_forward;

	void reset() { entry = NULL; key.clear(); should_forward = false; }
};

// the act of collection. 
int collect(int operation, AdTypes whichAds, ClassAd * ad, ClassAd * adPvt, CollectStatus & status);
// possible values for operation argument of collect
#define COLLECT_OP_REPLACE   0
#define COLLECT_OP_MERGE     1
#define COLLECT_OP_EXPIRE    2 // expire, but do not delete expired ads
#define COLLECT_OP_DELETE    3
#define COLLECT_OP_OFFLINE    0x100 // this can be OR'ed with COLLECT_OP_REPLACE to force an ad offline. (used by UPDATE_STARTD_AD_WITH_ACK)
#define COLLECT_OP_NO_OFFLINE 0x200 // this can be OR'ed with COLLECT_OP_REPLACE to disable updating of offline ads (used by offline_populate)

// delete or exprire ads that match the delete_if constraint
int collect_delete(int operation, AdTypes whichAds, ConstraintHolder & delete_if);
// delete or exprire ads that match the key
int collect_delete(int operation, AdTypes whichAds, const std::string & key);

// called by the housekeeper to expire ads
int collect_clean(time_t now);

// walk the public collection, calling the callback for ads that match the call_if expression
// the callback returns one of the WalkPostOp operations to control how iterations proceeds.
//
// if the callback returns a negative value, walk aborts and that value is returned
//
// if the callback returns a value with the Walk_Op_DeleteItem bit set, that entry will be deleted
// and the associated private ad will also be also be deleted.
//
// unless the callback returns a value with the Walk_Op_NoMatch bit set, the callback is counted.
//
// if the callback returns a value with the Walk_Op_Break bit set, the walk is ended
//
// returns < 0   if the callback returns one of the Abort values
//         >= 0  on success, value is the number of matches (i.e. callbacks that did not return NoMatch)
enum WalkPostOp {
	Walk_Op_Abort2   = -2, // stop iterating and return -2
	Walk_Op_Abort1   = -1, // stop iterating and return -1
	Walk_Op_Continue = 0, // keep iterating
	Walk_Op_Break    = 1, // stop iterating

	Walk_Op_NoMatch  = 0x10, // don't count this callback as a match
	Walk_Op_NoMatchBreak = (Walk_Op_NoMatch | Walk_Op_Break),

	Walk_Op_DeleteItem = 0x100, // remove this item from the collection
	Walk_Op_DeleteItemAndContinue = (Walk_Op_DeleteItem | Walk_Op_Continue),
	Walk_Op_DeleteItemAndBreak    = (Walk_Op_DeleteItem | Walk_Op_Break),
	Walk_Op_DeleteItemAndAbort    = (Walk_Op_DeleteItem | Walk_Op_Abort1),
};
int collect_walk(AdTypes adType, ConstraintHolder & call_if, WalkPostOp (*callback)(void* pv, AdCollection::iterator & it), void*pv);


// brute force totaling of various ads in the collection
// because we don't (yet) have to code to do this correctly as we go.
//
struct CollectionCounters {
	int submitterNumAds;
	int submitterRunningJobs;
	int submitterIdleJobs;
	int startdNumAds;

	int machinesTotal;
	int machinesUnclaimed;
	int machinesClaimed;
	int machinesOwner;

	int jobsByUniverse[CONDOR_UNIVERSE_MAX];

	void reset() { memset(this, 0, sizeof(*this)); }
};

void calc_collection_totals(struct CollectionCounters & counts);

#endif
