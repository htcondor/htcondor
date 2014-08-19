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

struct {
	AdCollection Public;
	AdCollection Private;
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

int collect(int cmd, ClassAd * ad, ClassAd * adPvt)
{
	AdTypes whichAds = CollectorCmdToAdType(cmd);
	dprintf(D_FULLDEBUG, "collector::collect(%s, %p, %p) whichAds=%d\n", getCommandStringSafe(cmd), ad, adPvt, whichAds);

	// fetch attributes needed to build the hashkey.
	std::string name, owner, addr;
	if ( ! ad->LookupString(ATTR_NAME, name)) {
		// problem getting the attributues we will need to make a key
		// treat this as a malformed ad.
		return -3;
	}

	bool use_host_of_addr = false;
	switch (whichAds) {
		case STARTD_AD: //    key = (Name||Machine)+ipAddr(MyAddress)
		case STARTD_PVT_AD:
		case SCHEDD_AD:
		case MASTER_AD:
		case LICENSE_AD: // *only one licence ad at a time is allowed
			if ( ! ad->LookupString(ATTR_MY_ADDRESS, addr)) {
				return -3;
			}
			use_host_of_addr = true;
			break;

		case SUBMITTOR_AD: // key = Name+ScheddName+ipAddr(MyAddress)
		case QUILL_AD:     // key = Name+ScheddName
			if ( ! ad->LookupString(ATTR_SCHEDD_NAME, owner)) {
				return -3;
			}
			break;

		case GRID_AD: //  key = HashName+Owner+(ScheddName||ScheddIpAddr)
			if ( ! ad->LookupString(ATTR_HASH_NAME, name)) {
				return -3;
			}
			if ( ! ad->LookupString(ATTR_SCHEDD_NAME, owner)) {
				return -3;
			}
			break;

		case CKPT_SRVR_AD: // key = Machine
			if ( ! ad->LookupString(ATTR_MACHINE, name)) {
				return -3;
			}
			break;

		// case COLLECTOR_AD: // key = (Name||Machine)
		// STORAGE_AD:    key = Name
		// NEGOTIATOR_AD: key = Name
		// HAD_AD:		  key = Name
		// XFER_SERVICE_AD: key = Name
		// LEASE_MANAGER_AD: Key = Name
		// GENERIC_AD:    key = Name
		default:
			break;
	}

	// force the startd private ad to have the same name & addr attributes as the public ad.
	if (cmd == UPDATE_STARTD_AD || cmd == UPDATE_STARTD_AD_WITH_ACK) {

		if (adPvt) {
				// Queries of private ads depend on the following:
			SetMyTypeName(*adPvt, STARTD_ADTYPE);

				// Negotiator matches up private ad with public ad by
				// using the following.
			adPvt->InsertAttr(ATTR_NAME, name);
			adPvt->InsertAttr(ATTR_MY_ADDRESS, addr);
		}
	}

	// build up the key
	std::string key;
	key.reserve(6+name.size()+owner.size()+addr.size());
	formatstr(key, "%d/%s", whichAds, name.c_str());
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
			#if 0 // emplace is supported
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
		#if 0 // emplace is supported
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
	dprintf(D_STATUS | D_VERBOSE, "Creating collection iterator for %s ads\n",  AdTypeToString(adtype));
	if (adtype != NO_AD) { adtype = ANY_AD; }
	if (adtype != ANY_AD) {
		PRAGMA_REMIND("find the begin() and end() for this ad type");
		std::string key;
		formatstr(key, "%d", adtype);
		begin = lst->lower_bound(key);
		formatstr(key, "%d", adtype+1);
		end = lst->lower_bound(key);
	}
	dprintf(D_STATUS | D_VERBOSE, "Will be iterating from key '%s' to just before key '%s'\n",
		(begin != end) ? begin->first.c_str() : "<begin>",
		(end != lst->end()) ? end->first.c_str() : "<end>"
		);
}

CollectionIterator<ClassAd*>* collect_get_iter(AdTypes whichAds)
{
	return new CollectionIterFromCollection (&Collections.Public, whichAds);
}

void collect_free_iter(CollectionIterator<ClassAd*>* it)
{
	CollectionIterFromCollection * cit = reinterpret_cast<CollectionIterFromCollection*>(it);
	delete cit;
}

