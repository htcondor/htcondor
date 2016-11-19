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

#include "clerk_utils.h"
#include "clerk_collect.h"
#include <map>

#define AC_ENT_PRIVATE 0x0001
#define AC_ENT_OFFLINE 0x0002
#define AC_ENT_EXPIRED 0x0004

typedef struct AdCollectionEntry {
	AdTypes                  adType;
	int                      flags;  // zero or more of AC_ENT_* flags
	time_t                   updateTime; // value of ATTR_LAST_HEARD_FROM
	time_t                   expireTime; // value of ATTR_LAST_HEARD_FROM + ATTR_CLASSAD_LIFETIME
	compat_classad::ClassAd* ad;
	AdCollectionEntry() : adType(NO_AD), flags(0), updateTime(0), expireTime(0), ad(NULL) {}
	AdCollectionEntry(AdTypes t, int f, time_t u, time_t e, compat_classad::ClassAd* a);
	int ReplaceAd(AdTypes atype, int new_flags, time_t now, time_t expire_time, compat_classad::ClassAd* new_ad);
} AdCollectionEntry;

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
	bool IsSelf(const std::string & ad_name) { return ad_name == name; }
private:
	AdTypes typ;
	ClassAd * ad; // this is not owned by this class, it points to the global daemon ad
	std::string name;
	std::string addr;
};

struct {
	AdCollection Public;
	AdCollection Private;
	SelfCollection Self;
	ConstraintHolder AbsentReq;
} Collections;

AdCollectionEntry::AdCollectionEntry(AdTypes t, int f, time_t u, time_t e, compat_classad::ClassAd* a)
	: adType(t)
	, flags(f)
	, updateTime(u)
	, expireTime(e)
	, ad(a)
{
	if ((this->expireTime == (time_t)-1) && ad) {
		int lifetime = 0;
		if ( ! ad->LookupInteger(ATTR_CLASSAD_LIFETIME, lifetime)) { lifetime = 1; }
		this->expireTime = this->updateTime + lifetime;
	}
}

int AdCollectionEntry::ReplaceAd(AdTypes atype, int new_flags, time_t now, time_t expire_time, compat_classad::ClassAd* new_ad)
{
	if (this->ad != new_ad) {
		delete this->ad; this->ad = NULL;
	}
	this->adType = atype;
	this->flags = new_flags; // something more complex needed here?
	this->updateTime = now;
	this->expireTime = expire_time;
	//int lifetime = 0;
	//if ( ! new_ad->LookupInteger(ATTR_CLASSAD_LIFETIME, lifetime)) { lifetime = 1; }
	//this->expireTime = this->updateTime + lifetime;
	this->ad = new_ad;
	return 0;
}

void collect_config()
{
	Collections.AbsentReq.set(param("ABSENT_REQUIREMENTS"));
	dprintf (D_ALWAYS,"ABSENT_REQUIREMENTS = %s\n", Collections.AbsentReq.c_str());
}

bool check_absent(AdCollectionEntry & entry, const std::string & key)
{
	if (Collections.AbsentReq.empty()) return false;

	if (entry.ad) return false;
	ClassAd & ad = *entry.ad;

	/* If the ad is alraedy has ABSENT=True and it is expiring, then
	   let it be deleted as in this case it already sat around absent
	   for the absent lifetime. */
	bool already_absent = false;
	ad.LookupBool(ATTR_ABSENT,already_absent);
	if (already_absent) {
		MyString s;
		if ( ! key.empty()) {
#if 1
			PRAGMA_REMIND("add persist mechanism for offline ads")
#else
			persistentRemoveAd(key.c_str());
#endif
		}
		return false; // return false tells collector to delete this ad
	}

	// Test is ad against the absent requirements expression, and
	// mark the ad absent if true
	bool val;
	classad::Value result;
	if (EvalExprTree(Collections.AbsentReq.Expr(), &ad, NULL, result) && result.IsBooleanValue(val) && val) 
	{
		time_t lifetime, now;

		const int def_lifetime = 60 * 60 * 24 * 30; // default expire absent ads in a month
		lifetime = param_integer ("ABSENT_EXPIRE_ADS_AFTER", def_lifetime);
		if (0 == lifetime) lifetime = INT_MAX; // 0 means forever

		ad.Assign (ATTR_ABSENT, true);
		ad.Assign (ATTR_CLASSAD_LIFETIME, lifetime);
		now = time(NULL);
		ad.Assign(ATTR_LAST_HEARD_FROM, now);
		ad.Assign (ATTR_MY_CURRENT_TIME, now);
#if 1
		PRAGMA_REMIND("add persist mechanism for offline ads")
#else
		persistentStoreAd(NULL,ad);
#endif
		// if we marked this ad as absent, we want to keep it in the collector
		return true;	// return true tells the collector to KEEP this ad
	}

	return false;	// return false tells collector to delete this ad
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

int SelfCollection::update(ClassAd * cad)
{
	ad = cad;
	if (ad) {
		ad->LookupString(ATTR_NAME, name);
		ad->LookupString(ATTR_MY_ADDRESS, addr);
	} else {
		name.clear();
		addr.clear();
	}
	return 0;
}

int collect_self(ClassAd * ad)
{
	return Collections.Self.update(ad);
}

// delete or exprire ads that match the delete_if constraint
int collect_delete(int operation, AdTypes whichAds, ConstraintHolder & delete_if)
{
	dprintf(D_FULLDEBUG, "collector::collect_delete(%s, %s) expr: %s\n",
		CollectOpName(operation), AdTypeName(whichAds), delete_if.c_str());

	if (whichAds == NO_AD)
		return 0;

	std::map<std::string, AdCollectionEntry>::iterator begin, end;
	;

	if (whichAds != ANY_AD) {
		AdTypes key_aty = (whichAds == STARTD_PVT_AD) ? STARTD_AD : whichAds; // private ads are keyed the same as startd ads.
		std::string key;
		formatstr(key, ADTYPE_FMT "/", key_aty);
		begin = Collections.Public.lower_bound(key);
	} else {
		begin = Collections.Public.begin();
	}
	end = Collections.Public.end();

	int cRemoved = 0;

	classad::Value result;
	std::map<std::string, AdCollectionEntry>::iterator it, jt;
	for (it = begin; it != end; it = jt) {
		jt = it; ++jt;

		if ((whichAds != ANY_AD) && (whichAds != it->second.adType)) {
			break;
		}

		bool matches = false;
		if (EvalExprTree(delete_if.Expr(), it->second.ad, NULL, result) && 
			result.IsBooleanValue(matches) && matches) {
			Collections.Public.erase(it);
			++cRemoved;
		}
	}

	return cRemoved;
}


int collect(int operation, AdTypes whichAds, ClassAd * ad, ClassAd * adPvt)
{
	dprintf(D_FULLDEBUG, "collector::collect(%s, %s, %p, %p) %d\n",
		CollectOpName(operation), AdTypeName(whichAds), ad, adPvt, whichAds);

	bool key_required = operation == COLLECT_OP_REPLACE || operation == COLLECT_OP_MERGE;
	int has_key_fields = 0;
	int uses_key_fields = 0;
	const int kfName     = 0x01;
	const int kfAddr     = 0x02;
	const int kfMachine  = 0x04;
	const int kfSchedd   = 0x08;
	const int kfHash     = 0x10;

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
			uses_key_fields = kfName | kfAddr;
			if (ad->LookupString(ATTR_MY_ADDRESS, addr)) {
				has_key_fields |= kfAddr;
			}
			use_host_of_addr = true;
			break;

		case SUBMITTOR_AD: // key = Name+ScheddName+ipAddr(MyAddress)
		case QUILL_AD:     // key = Name+ScheddName
			uses_key_fields = kfName | kfSchedd | kfAddr;
			if (ad->LookupString(ATTR_MY_ADDRESS, addr)) {
				has_key_fields |= kfAddr;
			}
			if (ad->LookupString(ATTR_SCHEDD_NAME, owner)) {
				has_key_fields |= kfSchedd;
			}
			break;

		case GRID_AD: //  key = HashName+Owner+(ScheddName||ScheddIpAddr)
			uses_key_fields = kfHash | kfSchedd;
			if ( ! ad->LookupString(ATTR_SCHEDD_NAME, owner)) {
				has_key_fields |= kfSchedd;
			}
			if (ad->LookupString(ATTR_HASH_NAME, name)) {
				has_key_fields |= kfHash;
			}
			break;

		case CKPT_SRVR_AD: // key = Machine
			uses_key_fields = kfMachine;
			if (ad->LookupString(ATTR_MACHINE, name)) {
				has_key_fields |= kfMachine;
			}
			break;

		case COLLECTOR_AD: // key = (Name||Machine)
			// don't allow an ad with the same key as our self ad to be inserted 
			// into the collection, Instead just return success.
			uses_key_fields = kfName | kfMachine;
			if (Collections.Self.IsSelf(name)) {
				return 0;
			}
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
			uses_key_fields = kfName;
		default:
			break;
	}

	if (key_required && (has_key_fields & uses_key_fields) != uses_key_fields) {
		// problem getting the attributues we will need to make a key
		// treat this as a malformed ad.
		return -3;
	}


	// force the startd private ad to have the same name & addr attributes as the public ad.
	if (whichAds == STARTD_AD || whichAds == STARTD_PVT_AD) {

		if (adPvt) {
				// Queries of private ads depend on the following:
			SetMyTypeName(*adPvt, STARTD_ADTYPE);

				// Negotiator matches up private ad with public ad by
				// using the following.
			adPvt->InsertAttr(ATTR_NAME, name);
			adPvt->InsertAttr(ATTR_MY_ADDRESS, addr);
		}
	}

	// if key is not required, and the ad doesn't have the key fields, this can still be a valid
	// invalidate. Handle those now...
	if ( ! key_required && (has_key_fields & uses_key_fields) != uses_key_fields) {
		PRAGMA_REMIND("write code to handle invalidate by constraint")
		return 0;
	}

	// build up the key
	std::string key;
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

	bool expire_ads = COLLECT_OP_EXPIRE == operation;
	if (expire_ads || (COLLECT_OP_DELETE == operation)) {
		bool absent_it = false;  // mark absent rather than removing
		AdCollection::iterator it;
		it = Collections.Public.find(key);
		if (it != Collections.Public.end()) {
			if (expire_ads) {
				absent_it = check_absent(it->second, key);
			}
			if ( ! absent_it) {
				dprintf(D_STATUS, "Removing %s ad key='%s'\n",  AdTypeToString(whichAds), key.c_str());
				it->second.ReplaceAd(whichAds, 0, 0, 0, NULL);
				Collections.Public.erase(it);
			}
		}

		it = Collections.Private.find(key);
		if (it != Collections.Private.end()) {
			dprintf(D_STATUS, "%s Private %s ad key='%s'\n", absent_it ? "Absenting" : "Removing", AdTypeToString(whichAds), key.c_str());
			if (absent_it) {
			} else {
				it->second.ReplaceAd(whichAds, 0, 0, 0, NULL);
				Collections.Private.erase(it);
			}
		}
		return 0;
	}

	int lifetime = 0;
	if ( ! ad->LookupInteger( ATTR_CLASSAD_LIFETIME, lifetime)) {
		lifetime = 1; // expire on next housecleaning pass.
	}
	time_t now;
	time(&now);
	time_t expire_time = now + lifetime;

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
			dprintf(D_STATUS, "Updating %s ad key='%s'\n",  AdTypeToString(pvtType), key.c_str());
			lb->second.ReplaceAd(pvtType, AC_ENT_PRIVATE, now, expire_time, adPvt);
		} else {
			// inserting an ad
			dprintf(D_STATUS, "Inserting %s ad key='%s'\n",  AdTypeToString(pvtType), key.c_str());
			#if 1 // emplace is supported
			Collections.Private.emplace_hint(lb, key, AdCollectionEntry(pvtType, AC_ENT_PRIVATE, now, expire_time, adPvt));
			#else
			Collections.Private.insert(lb, AdCollectionPair(key, AdCollectionEntry(pvtType, AC_ENT_PRIVATE, now, expire_time, adPvt)));
			#endif
		}
	}

	lb = Collections.Public.lower_bound(key);
	if (lb != Collections.Public.end() && !(Collections.Public.key_comp()(key, lb->first))) {
		// updating an ad
		dprintf(D_STATUS, "Updating %s ad key='%s'\n",  AdTypeToString(whichAds), key.c_str());
		lb->second.ReplaceAd(whichAds, 0, now, expire_time, ad);
	} else {
		// inserting an ad
		dprintf(D_STATUS, "Inserting %s ad key='%s'\n",  AdTypeToString(whichAds), key.c_str());
		#if 1 // emplace is supported
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
			#if 1 // emplace is supported
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
		#if 1 // if emplace is supported
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
	dprintf(D_STATUS | D_VERBOSE, "Creating iterator for %s ads [%d]\n",  AdTypeToString(adtype), adtype);
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
		}
	}
}

