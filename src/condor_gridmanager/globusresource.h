
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

	bool IsEmpty();
	bool IsDown();
	char *ResourceName();

	static void setProbeInterval( int new_interval )
		{ probeInterval = new_interval; }

 private:
	int DoPing();

	char *resourceName;
	bool resourceDown;
	int pingTimerId;
	List<GlobusJob> registeredJobs;
	List<GlobusJob> pingRequesters;
	static int probeInterval;

	GahpClient gahp;
};

#endif
