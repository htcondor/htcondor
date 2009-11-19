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
#include "condor_daemon_core.h"

#include "condor_collector.h"
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

	// want collector to log messages?  Default: yes
	void toggleLogging (void);

	// perform the collect operation of the given command
	ClassAd *collect (int, Sock *, sockaddr_in *, int &);
	ClassAd *collect (int, ClassAd *, sockaddr_in *, int &, Sock* = NULL);

	// lookup classad in the specified table with the given hashkey
	ClassAd *lookup (AdTypes, AdNameHashKey &);

	// remove classad in the specified table with the given hashkey
	int remove (AdTypes, AdNameHashKey &);

	// walk specified hash table with the given visit procedure
	int walkHashTable (AdTypes, int (*)(ClassAd *));

	// Publish stats into the collector's ClassAd
	int publishStats( ClassAd *ad );

		// returns true on success; false on failure (and sets error_desc)
	bool setCollectorRequirements( char const *str, MyString &error_desc );

  private:
	// the greater tables
	enum {GREATER_TABLE_SIZE = 1024};
	CollectorHashTable StartdAds;
	CollectorHashTable StartdPrivateAds;
#ifdef HAVE_EXT_POSTGRESQL
	CollectorHashTable QuillAds;
#endif /* HAVE_EXT_POSTGRESQL */
	CollectorHashTable ScheddAds;
	CollectorHashTable SubmittorAds;
	CollectorHashTable LicenseAds;
	CollectorHashTable MasterAds;
	CollectorHashTable StorageAds;
	CollectorHashTable XferServiceAds;

	// the lesser tables
	enum {LESSER_TABLE_SIZE = 32};
	CollectorHashTable CkptServerAds;
	CollectorHashTable GatewayAds;
	CollectorHashTable CollectorAds;
	CollectorHashTable NegotiatorAds;
	CollectorHashTable HadAds;
	CollectorHashTable GridAds;
	CollectorHashTable LeaseManagerAds;
	
	// table for "generic" ad types
	GenericAdHashTable GenericAds;

	// for walking through the generic hash tables
	static int (*genericTableScanFunction)(ClassAd *);
	static int genericTableWalker(CollectorHashTable *cht);
	int walkGenericTables(int (*scanFunction)(ClassAd *));

	// relevant variables from the config file
	int	clientTimeout; 
	int	machineUpdateInterval;

	// should we log?
	bool log;

	void  housekeeper ();
	int  housekeeperTimerID;
	void cleanHashTable (CollectorHashTable &, time_t,
				bool (*) (AdNameHashKey &, ClassAd *,sockaddr_in *));
	ClassAd* updateClassAd(CollectorHashTable&,const char*, const char *,
						   ClassAd*,AdNameHashKey&, const MyString &, int &, 
						   const sockaddr_in * );

	ClassAd * mergeClassAd (CollectorHashTable &hashTable,
							const char *adType,
							const char *label,
							ClassAd *new_ad,
							AdNameHashKey &hk,
							const MyString &hashString,
							int  &insert,
							const sockaddr_in * /*from*/ );

	// support for dynamically created tables
	CollectorHashTable *findOrCreateTable(MyString &str);

	bool ValidateClassAd(int command,ClassAd *clientAd,Sock *sock);

	// Statistics
	CollectorStats	*collectorStats;

	ClassAd *m_collector_requirements;
};


#endif // __COLLECTOR_ENGINE_H__
