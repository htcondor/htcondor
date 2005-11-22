/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#ifndef __COLLECTOR_ENGINE_H__
#define __COLLECTOR_ENGINE_H__

#include "condor_classad.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

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

	// interval to check for down masters
	int scheduleDownMasterCheck (int = 10800);

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

  private:
	// the greater tables
	enum {GREATER_TABLE_SIZE = 1024};
	CollectorHashTable StartdAds;
	CollectorHashTable StartdPrivateAds;
#if WANT_QUILL
	CollectorHashTable QuillAds;
#endif /* WANT_QUILL */
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
	CollectorHashTable NegotiatorAds;
	CollectorHashTable HadAds;

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
				bool (*) (AdNameHashKey &, ClassAd *,sockaddr_in *));
	ClassAd* updateClassAd(CollectorHashTable&,const char*, const char *,
						   ClassAd*,AdNameHashKey&, const MyString &, int &, 
						   const sockaddr_in * );

	// Statistics
	CollectorStats	*collectorStats;

  public:
	// pointer values for representing master states
	static ClassAd* RECENTLY_DOWN;
	static ClassAd* DONE_REPORTING;
	static ClassAd* LONG_GONE;
	static ClassAd* THRESHOLD;

};


#endif // __COLLECTOR_ENGINE_H__
