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

#ifndef __COLLECTOR_ENGINE_H__
#define __COLLECTOR_ENGINE_H__

#include "condor_classad.h"

#include "collector_stats.h"
#include "hashkey.h"

struct CollectorRecord
{
	CollectorRecord(ClassAd* public_ad, ClassAd* pvt_ad)
		: m_publicAd(public_ad), m_pvtAd(pvt_ad) { m_pvtAd->ChainToAd(m_publicAd); }
	~CollectorRecord() { delete m_publicAd; delete m_pvtAd; }
	void ReplaceAds(ClassAd* public_ad, ClassAd* pvt_ad)
	{ delete m_publicAd; delete m_pvtAd; m_publicAd=public_ad; m_pvtAd=pvt_ad; m_pvtAd->ChainToAd(m_publicAd); }

	ClassAd* m_publicAd;
	ClassAd* m_pvtAd;
};

// type for the hash tables ...
typedef HashTable <AdNameHashKey, CollectorRecord *> CollectorHashTable;
typedef HashTable <istring, CollectorHashTable *> GenericAdHashTable;

class CollectorEngine : public Service
{
  public:
	CollectorEngine( CollectorStats *stats = NULL );
	~CollectorEngine();

	// maximum time a client can take to communicate with the collector
	int setClientTimeout (int = 20);

	// interval to clean out ads
	int scheduleHousekeeper (int = 300);
	int invokeHousekeeper (AdTypes);
	int invalidateAds(CollectorHashTable *table, const char *targetType, ClassAd &);

	// perform the collect operation of the given command
	CollectorRecord *collect (int, Sock *, const condor_sockaddr&, int &);
	CollectorRecord *collect (int, ClassAd *, const condor_sockaddr&, int &, Sock* = NULL);

	// lookup classad in the specified table with the given hashkey
	CollectorRecord *lookup (AdTypes, AdNameHashKey &);

	/**
	* remove () - attempts to construct a hashkey from a query
    * to remove in O(1) for INVALIDATE* vs. O(n). The query must contain
    * TARGET.Name && TARGET.MyAddress
	* @param t_AddType - the add type used to determine the hashtable to search
    * @param c_query - classad query 
	* @param query_contains_hash_key - will be set to true if hash key for a
	*                                 specific ad was specified in the query ad
    * @return - the number of records removed.
	*/
	//int remove (AdTypes t_AddType, const ClassAd & c_query, bool *hash_key_specified);
	int remove(CollectorHashTable * table, AdNameHashKey &hk, AdTypes t_AddType);

    /**
     * expire() - as remove(), above, except that it expires the ad.
     *
     */
	//int expire (AdTypes t_AddType, const ClassAd & c_query, bool *hash_key_specified);
	int expire(CollectorHashTable * hTable, AdNameHashKey & hKey);

	// remove classad in the specified table with the given hashkey
	int remove (AdTypes, AdNameHashKey &);

	// walk specified hash table with the given visit procedure
	//int walkHashTable (AdTypes, int (*)(CollectorRecord *));

	CollectorHashTable * getHashTable(AdTypes adType) {
		CollectorHashTable *table = nullptr;
		HashFunc func;
		if (LookupByAdType(adType, table, func)) { return table; }
		return nullptr;
	}

	CollectorHashTable * getGenericHashTable(const istring & mytype) {
		CollectorHashTable *table = nullptr;
		if (GenericAds.lookup(mytype, table) == 0) { return table; }
		return nullptr;	
	}

	std::vector<CollectorHashTable *> getAnyHashTables(const char * mytype = nullptr);

	// templated version of the above that uses a callable for the walk function
	template <typename Func>
	int walkHashTable(CollectorHashTable & table, Func fn) {
		// walk the hash table
		CollectorRecord *record = nullptr;
		table.startIterations();
		while (table.iterate(record)) {
			// call scan function for each ad
			if (!fn(record)) break;
		}
		return 1;
	}


	// Walk through a specific (non-generic, non-ANY) table using a lambda
	// this is used only by schedd_token_request.  
	template<typename T>
	int walkConcreteTable(AdTypes adType, T scanFunction) {
		if (ANY_AD == adType || GENERIC_AD == adType) {
			dprintf(D_ALWAYS, "Generic ad requested from walkConcreteTable.\n");
			return 0;
		}

		CollectorHashTable *table;
		CollectorEngine::HashFunc func;
		if (!LookupByAdType(adType, table, func)) {
			dprintf (D_ALWAYS, "Unknown type %ld\n", adType);
			return 0;
		}

			// walk the hash table
		CollectorRecord *record;
		table->startIterations();
		while (table->iterate(record)) {
				// call scan function for each ad
			if (!scanFunction(record)) {break;}
		}

		return 1;
	}


	// register the collector's own ad pointer, and check to see if a given ad is that ad.
	// this is used to allow us to recognise the collector ad during iteration and automatically
	// insert fresh stats into it when it is fetched.
	void identifySelfAd(CollectorRecord * record);
	bool isSelfAd(void * ad) { return __self_ad__ != NULL && __self_ad__ == ad; }

	// Publish stats into the collector's ClassAd
	//int publishStats( ClassAd *ad );

		// returns true on success; false on failure (and sets error_desc)
	bool setCollectorRequirements( char const *str, std::string &error_desc );

	typedef bool (*HashFunc) (AdNameHashKey &, const ClassAd *);

	bool LookupByAdType(AdTypes, CollectorHashTable *&, HashFunc &);

  private:

	// the greater tables

	CollectorHashTable StartdSlotAds;
	CollectorHashTable StartdPrivateAds;
	CollectorHashTable StartdDaemonAds;
	CollectorHashTable ScheddAds;
	CollectorHashTable SubmittorAds;
	CollectorHashTable LicenseAds;
	CollectorHashTable MasterAds;
	CollectorHashTable StorageAds;
	CollectorHashTable AccountingAds;

	// the lesser tables
	CollectorHashTable CkptServerAds;
	CollectorHashTable CollectorAds;
	CollectorHashTable NegotiatorAds;
	CollectorHashTable HadAds;
	CollectorHashTable GridAds;
	
	// table for "generic" ad types
	GenericAdHashTable GenericAds;

	// for walking through the generic hash tables
	static int (*genericTableScanFunction)(CollectorRecord *);
	static int genericTableWalker(CollectorHashTable *cht);
	int walkGenericTables(int (*scanFunction)(CollectorRecord *));

	// relevant variables from the config file
	int	clientTimeout;
	int	machineUpdateInterval;

	void  housekeeper ( int timerID = -1 );
	int  housekeeperTimerID;
	void cleanHashTable (CollectorHashTable &, time_t, HashFunc) const;
	CollectorRecord* updateClassAd(CollectorHashTable&,const char*, const char *, bool,
						   ClassAd*,AdNameHashKey&, const std::string &, int &,
						   const condor_sockaddr& );

	CollectorRecord* mergeClassAd (CollectorHashTable &hashTable,
							const char *adType,
							const char *label,
							ClassAd *new_ad,
							AdNameHashKey &hk,
							const std::string &hashString,
							int  &insert,
							const condor_sockaddr& /*from*/ );

	// support for dynamically created tables
	CollectorHashTable *findOrCreateTable(const istring &str);

	bool ValidateClassAd(int command,ClassAd *clientAd,Sock *sock);

	void* __self_ad__; // contains address of last Ad for this collector added to the hashtable, do NOT free from here
					   // this pointer is only used to recognise this collector's ad during a condor_status query
					   // so it's harmless if this pointer is out of date.

	// Statistics
	CollectorStats	*collectorStats;

	ClassAd *m_collector_requirements;

	bool m_forwardFilteringEnabled;
	std::vector<std::string> m_forwardWatchList;
	int m_forwardInterval;
public: // so that the config code can set it.
	bool m_allowOnlyOneNegotiator; // prior to 8.5.8, this was hard-coded to be true.
	int  m_get_ad_options; // new for 8.7.0, may be temporary
};

AdTypes AdTypeStringToWhichAds(const char* target_type);
AdTypes get_realish_startd_adtype(const char * mytype);

#endif // __COLLECTOR_ENGINE_H__
