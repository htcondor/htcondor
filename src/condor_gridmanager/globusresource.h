
#ifndef GLOBUSRESOURCE_H
#define GLOBUSRESOURCE_H

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

#include "proxymanager.h"
#include "globusjob.h"
#include "gahp-client.h"


class GlobusJob;

class GlobusResource : public Service
{
 public:

	GlobusResource( const char *resource_name );
	~GlobusResource();

	void Reconfig();
	void RegisterJob( GlobusJob *job, bool already_submitted );
	void UnregisterJob( GlobusJob *job );
	void RequestPing( GlobusJob *job );
	bool RequestSubmit( GlobusJob *job );
	void SubmitComplete( GlobusJob *job );
	void CancelSubmit( GlobusJob *job );

	bool IsEmpty();
	bool IsDown();
	char *ResourceName();

	time_t getLastStatusChangeTime() { return lastStatusChange; }

	static const char *CanonicalName( const char *name );

	static void setProbeInterval( int new_interval )
		{ probeInterval = new_interval; }

	static void setProbeDelay( int new_delay )
		{ probeDelay = new_delay; }

	static void setGahpCallTimeout( int new_timeout )
		{ gahpCallTimeout = new_timeout; }

 private:
	int DoPing();

	char *resourceName;
	bool resourceDown;
	bool firstPingDone;
	int pingTimerId;
	time_t lastPing;
	time_t lastStatusChange;
	Proxy *myProxy;
	List<GlobusJob> registeredJobs;
	List<GlobusJob> pingRequesters;
	// jobs that are currently executing a submit
	List<GlobusJob> submitsInProgress;
	// jobs that want to submit but can't due to submitLimit
	List<GlobusJob> submitsQueued;
	// jobs allowed to submit under jobLimit
	List<GlobusJob> submitsAllowed;
	// jobs that want to submit but can't due to jobLimit
	List<GlobusJob> submitsWanted;
	static int probeInterval;
	static int probeDelay;
	int submitLimit;		// max number of submit actions
	int jobLimit;			// max number of submitted jobs
	static int gahpCallTimeout;

	GahpClient gahp;
};

#endif
