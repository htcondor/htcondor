
#ifndef GLOBUSRESOURCE_H
#define GLOBUSRESOURCE_H

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

#include "gahp-client.h"
#include "globusjob.h"

class GlobusJob;

class GlobusResource : public Service
{
 public:

	GlobusResource( char *resource_name );
	~GlobusResource();

	void RegisterJob( GlobusJob *job );
	void UnregisterJob( GlobusJob *job );
	void RequestPing( GlobusJob *job );
	bool RequestSubmit( GlobusJob *job );
	bool CancelSubmit( GlobusJob *job );

	bool IsEmpty();
	bool IsDown();
	char *ResourceName();

	static void setProbeInterval( int new_interval )
		{ probeInterval = new_interval; }

	static void setProbeDelay( int new_delay )
		{ probeDelay = new_delay; }

	static void setSubmitLimit( int new_limit )
		{ submitLimit = new_limit; }

 private:
	int DoPing();

	char *resourceName;
	bool resourceDown;
	bool firstPingDone;
	int pingTimerId;
	time_t lastPing;
	List<GlobusJob> registeredJobs;
	List<GlobusJob> pingRequesters;
	List<GlobusJob> submitsInProgress;
	List<GlobusJob> submitsQueued;
	static int probeInterval;
	static int probeDelay;
	static int submitLimit;

	GahpClient gahp;
};

#endif
