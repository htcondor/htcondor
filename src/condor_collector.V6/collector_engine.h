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

	// want collector to log messages?  Default: yes
	void toggleLogging (void);

	// perform the collect operation of the given command
	ClassAd *collect (int, XDR *, sockaddr_in *, int &);
	ClassAd *collect (int, Sock *, sockaddr_in *, int &);
	ClassAd *collect (int, ClassAd *, sockaddr_in *, int &);

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
	CollectorHashTable ScheddAds;
	CollectorHashTable MasterAds;

	// the lesser tables
	const int LESSER_TABLE_SIZE = 32;
	CollectorHashTable CkptServerAds;
	CollectorHashTable GatewayAds;

	// relevent variables from the config file
	int   clientTimeout; 
	int   machineUpdateInterval;

	// communication socket
	int   clientSocket;

	// check if time to clean out old classads
	bool  timeToClean;

	// should we log?
	bool  log;

	// timer services
	TimerManager *timer;
	friend int engine_clientTimeoutHandler(Service *);// client takes too long
	friend int engine_housekeepingHandler (Service *);// clean out old classads
	
	void housekeeper (void);
	int  housekeeperTimerID;
	void cleanHashTable (CollectorHashTable &, time_t,
				bool (*) (HashKey &, ClassAd *,sockaddr_in *));
};

#endif // __COLLECTOR_ENGINE_H__
