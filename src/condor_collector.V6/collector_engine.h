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
	int invalidateAds(AdTypes, ClassAd &);

	// perform the collect operation of the given command
	ClassAd *collect (int, Sock *, const condor_sockaddr&, int &);
	ClassAd *collect (int, ClassAd *, const condor_sockaddr&, int &, Sock* = NULL);

	// lookup classad in the specified table with the given hashkey
	ClassAd *lookup (AdTypes, AdNameHashKey &);

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
	int remove (AdTypes t_AddType, const ClassAd & c_query, bool *hash_key_specified);

    /**
     * expire() - as remove(), above, except that it expires the ad.
     *
     */
	int expire (AdTypes t_AddType, const ClassAd & c_query, bool *hash_key_specified);

	// remove classad in the specified table with the given hashkey
	int remove (AdTypes, AdNameHashKey &);

	// walk specified hash table with the given visit procedure
	int walkHashTable (AdTypes, int (*)(ClassAd *));

	// Walk through a specific (non-generic, non-ANY) table using a lambda
	template<typename T>
	int walkConcreteTable(AdTypes adType, T scanFunction) {
		if (ANY_AD == adType || GENERIC_AD == adType) {
			dprintf(D_ALWAYS, "Generic ad requested from walkConcreteTable.\n");
			return 0;
		}

		CollectorHashTable *table;
		CollectorEngine::HashFunc func;
		if (!LookupByAdType(adType, table, func)) {
			dprintf (D_ALWAYS, "Unknown type %d\n", adType);
			return 0;
		}

			// walk the hash table
		ClassAd *ad;
		table->startIterations();
		while (table->iterate(ad)) {
				// call scan function for each ad
			if (!scanFunction(ad)) {break;}
		}

		return 1;
	}


	// register the collector's own ad pointer, and check to see if a given ad is that ad.
	// this is used to allow us to recognise the collector ad during iteration and automatically
	// insert fresh stats into it when it is fetched.
	void identifySelfAd(ClassAd * ad);
	bool isSelfAd(void * ad) { return __self_ad__ != NULL && __self_ad__ == ad; }

	// Publish stats into the collector's ClassAd
	//int publishStats( ClassAd *ad );

		// returns true on success; false on failure (and sets error_desc)
	bool setCollectorRequirements( char const *str, std::string &error_desc );

  private:
	typedef bool (*HashFunc) (AdNameHashKey &, const ClassAd *);

	bool LookupByAdType(AdTypes, CollectorHashTable *&, HashFunc &);
 
	// the greater tables

	/**
	* TODO<tstclair>: Eval notes and refactor when time permits.
	* consider using std::map<AdTypes,CollectorHashTable>
	* possibly create a new class with some queries and stats within it.
	* this seems to be a sloppy encapsulation issue.
	*/

	CollectorHashTable StartdAds;
	CollectorHashTable StartdPrivateAds;
	CollectorHashTable ScheddAds;
	CollectorHashTable SubmittorAds;
	CollectorHashTable LicenseAds;
	CollectorHashTable MasterAds;
	CollectorHashTable StorageAds;
	CollectorHashTable AccountingAds;

	// the lesser tables
	CollectorHashTable CkptServerAds;
	CollectorHashTable GatewayAds;
	CollectorHashTable CollectorAds;
	CollectorHashTable NegotiatorAds;
	CollectorHashTable HadAds;
	CollectorHashTable GridAds;
	
	// table for "generic" ad types
	GenericAdHashTable GenericAds;

	// for walking through the generic hash tables
	static int (*genericTableScanFunction)(ClassAd *);
	static int genericTableWalker(CollectorHashTable *cht);
	int walkGenericTables(int (*scanFunction)(ClassAd *));

	// relevant variables from the config file
	int	clientTimeout;
	int	machineUpdateInterval;

	void  housekeeper ();
	int  housekeeperTimerID;
	void cleanHashTable (CollectorHashTable &, time_t, HashFunc) const;
	ClassAd* updateClassAd(CollectorHashTable&,const char*, const char *,
						   ClassAd*,AdNameHashKey&, const std::string &, int &,
						   const condor_sockaddr& );

	ClassAd * mergeClassAd (CollectorHashTable &hashTable,
							const char *adType,
							const char *label,
							ClassAd *new_ad,
							AdNameHashKey &hk,
							const std::string &hashString,
							int  &insert,
							const condor_sockaddr& /*from*/ );

	// support for dynamically created tables
	CollectorHashTable *findOrCreateTable(const std::string &str);

	bool ValidateClassAd(int command,ClassAd *clientAd,Sock *sock);

	void* __self_ad__; // contains address of last Ad for this collector added to the hashtable, do NOT free from here
					   // this pointer is only used to recognise this collector's ad during a condor_status query
					   // so it's harmless if this pointer is out of date.

	// Statistics
	CollectorStats	*collectorStats;

	ClassAd *m_collector_requirements;

	bool m_forwardFilteringEnabled;
	StringList m_forwardWatchList;
	int m_forwardInterval;
public: // so that the config code can set it.
	bool m_allowOnlyOneNegotiator; // prior to 8.5.8, this was hard-coded to be true.
	int  m_get_ad_options; // new for 8.7.0, may be temporary
};


#endif // __COLLECTOR_ENGINE_H__
