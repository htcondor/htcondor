
#ifndef GLOBUSRESOURCE_H
#define GLOBUSRESOURCE_H

#include "condor_common.h"

#include "globusjob.h"

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

 private:
	int DoPing();

	char *resoruceName;
	bool resourceDown;
	int pingTimerId;
	List<GlobusJob> registeredJobs;
	List<GlobusJob> pingRequesters;
};

#endif
