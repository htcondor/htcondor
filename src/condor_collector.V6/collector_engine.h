/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#ifndef __COLLECTOR_ENGINE_H__
#define __COLLECTOR_ENGINE_H__

#include "condor_classad.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

#include "condor_collector.h"
#include "hashkey.h"

class CollectorEngine : public Service
{
  public:
	CollectorEngine();
	~CollectorEngine();

	// maximum time a client can take to communicate with the collector
	int setClientTimeout (int = 20);

	// interval to clean out ads
	int scheduleHousekeeper (int = 300);
	int invokeHousekeeper (AdTypes);

	// interval to check for down masters
	int scheduleDownMasterCheck (int = 10800);

	// want collector to log messages?  Default: yes
	void toggleLogging (void);

	// perform the collect operation of the given command
	ClassAd *collect (int, Sock *, sockaddr_in *, int &);
	ClassAd *collect (int, ClassAd *, sockaddr_in *, int &, Sock* = NULL);

	// lookup classad in the specified table with the given hashkey
	ClassAd *lookup (AdTypes, HashKey &);

	// remove classad in the specified table with the given hashkey
	int remove (AdTypes, HashKey &);

	// walk specified hash table with the given visit procedure
	int walkHashTable (AdTypes, int (*)(ClassAd *));

  private:
	// the greater tables
	enum {GREATER_TABLE_SIZE = 1024};
	CollectorHashTable StartdAds;
	CollectorHashTable StartdPrivateAds;
	CollectorHashTable ScheddAds;
	CollectorHashTable SubmittorAds;
	CollectorHashTable LicenseAds;
	CollectorHashTable MasterAds;
	CollectorHashTable StorageAds;

	// the lesser tables
	enum {LESSER_TABLE_SIZE = 32};
	CollectorHashTable CkptServerAds;
	CollectorHashTable GatewayAds;
	CollectorHashTable CollectorAds;

	// relevant variables from the config file
	int	clientTimeout; 
	int	machineUpdateInterval;
	int	masterCheckInterval;

	// should we log?
	bool log;

	void checkMasterStatus (ClassAd *);
	int  masterCheck ();
	int  masterCheckTimerID;
	int  housekeeper ();
	int  housekeeperTimerID;
	void cleanHashTable (CollectorHashTable &, time_t,
				bool (*) (HashKey &, ClassAd *,sockaddr_in *));
	ClassAd* updateClassAd(CollectorHashTable&,char*,ClassAd*,HashKey&,
	                       char*, int & );

  public:
	// pointer values for representing master states
	static ClassAd* RECENTLY_DOWN;
	static ClassAd* DONE_REPORTING;
	static ClassAd* LONG_GONE;
	static ClassAd* THRESHOLD;

};


#endif // __COLLECTOR_ENGINE_H__
