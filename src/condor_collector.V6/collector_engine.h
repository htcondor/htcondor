#ifndef __COLLECTOR_ENGINE_H__
#define __COLLECTOR_ENGINE_H__

#include "condor_classad.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

#include "condor_collector.h"
#include "HashTable.h"
#include "hashkey.h"

// pointer values for representing master states
extern  ClassAd *RECENTLY_DOWN;
extern  ClassAd *DONE_REPORTING;
extern  ClassAd *LONG_GONE;
extern  ClassAd *THRESHOLD;

class CollectorEngine : public Service
{
  public:
	CollectorEngine();
	~CollectorEngine();

	// maximum time a client can take to communicate with the collector
	int setClientTimeout (int = 20);

	// interval to clean out ads
	int scheduleHousekeeper (int = 300);

	// interval to check for down masters
	int scheduleDownMasterCheck (int = 10800);

	// want collector to log messages?  Default: yes
	void toggleLogging (void);

	// want startd private ads?  Default: no
	void wantStartdPrivateAds (bool);

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
	CollectorHashTable MasterAds;

	// the lesser tables
	enum {LESSER_TABLE_SIZE = 32};
	CollectorHashTable CkptServerAds;
	CollectorHashTable GatewayAds;

	// relevent variables from the config file
	int	clientTimeout; 
	int	machineUpdateInterval;
	int	masterCheckInterval;

	// should we log?
	bool log;

	// do we want private ads?
	bool pvtAds;

	void checkMasterStatus (ClassAd *);
	int masterCheck ();
	int  masterCheckTimerID;
	int  housekeeper ();
	int  housekeeperTimerID;
	void cleanHashTable (CollectorHashTable &, time_t,
				bool (*) (HashKey &, ClassAd *,sockaddr_in *));
};

#ifndef WIN32
// export a utility function
int email (char *, char * = NULL);
#endif  // of ifndef WIN32

#endif // __COLLECTOR_ENGINE_H__
