#ifndef __COLLECTOR_ENGINE_H__
#define __COLLECTOR_ENGINE_H__

#include "condor_classad.h"
#include "condor_daemon_core.h"
#include "condor_timer_manager.h"

#include "condor_collector.h"
#include "HashTable.h"
#include "hashkey.h"

class CollectorEngine : public Service
{
  public:
	CollectorEngine(TimerManager *);
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
#if defined(USE_XDR)
	ClassAd *collect (int, XDR *, sockaddr_in *, int &);
#endif
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
	const int GREATER_TABLE_SIZE = 1024;
	CollectorHashTable StartdAds;
	CollectorHashTable StartdPrivateAds;
	CollectorHashTable ScheddAds;
	CollectorHashTable MasterAds;

	// the lesser tables
	const int LESSER_TABLE_SIZE = 32;
	CollectorHashTable CkptServerAds;
	CollectorHashTable GatewayAds;

	// relevent variables from the config file
	int	clientTimeout; 
	int	machineUpdateInterval;
	int	masterCheckInterval;

	// communication socket
	int clientSocket;

	// check if time to perform cleanups
	bool timeToClean;
	bool timeToCheckMasters;

	// should we log?
	bool log;

	// do we want private ads?
	bool pvtAds;

	// timer services
	TimerManager *timer;
	friend int engine_clientTimeoutHandler(Service *);// client takes too long
	friend int engine_housekeepingHandler (Service *);// clean out old classads
	friend int engine_masterCheckHandler  (Service *);// check status of masters
	
	void checkMasterStatus (ClassAd *);
	void masterCheck (void);
	int  masterCheckTimerID;
	void housekeeper (void);
	int  housekeeperTimerID;
	void cleanHashTable (CollectorHashTable &, time_t,
				bool (*) (HashKey &, ClassAd *,sockaddr_in *));
};

// export a utility function
int email (char *, char * = NULL);

#endif // __COLLECTOR_ENGINE_H__
