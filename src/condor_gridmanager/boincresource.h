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


#ifndef BOINCRESOURCE_H
#define BOINCRESOURCE_H

#include "condor_common.h"
#include "condor_daemon_core.h"

#include "baseresource.h"
#include "gahp-client.h"


class BoincJob;

struct BoincBatch;

class BoincResource : public BaseResource
{
 protected:
	BoincResource( const char *resource_name );
	~BoincResource();

 public:
	bool Init();
	const char *ResourceType();
	void Reconfig();
	void RegisterJob( BaseJob *job );
	void UnregisterJob( BaseJob *job );

	const char *GetHashName();

	void PublishResourceAd( ClassAd *resource_ad );

	bool JoinBatch( std::string &batch_name, std::string &error_str );
	int Submit( /* ... */ );

	static const char *CanonicalName( const char *name );
	static const char *HashName( const char *resource_name );

	static BoincResource *FindOrCreateResource( const char *resource_name );

	static void setGahpCallTimeout( int new_timeout )
		{ gahpCallTimeout = new_timeout; }

	// This should be private, but BoincJob references it directly for now
	static HashTable <HashKey, BoincResource *> ResourcesByName;

 private:
	void DoPing( time_t& ping_delay, bool& ping_complete,
				 bool& ping_succeeded );

	bool initialized;

	char *serviceUri;
	static int gahpCallTimeout;
	GahpClient *gahp; // For pings.
	GahpClient *status_gahp;
	GahpClient *m_leaseGahp;

protected:

	BatchStatusResult StartBatchStatus();
	BatchStatusResult FinishBatchStatus();
	GahpClient * BatchGahp();

	void DoUpdateSharedLease( time_t& update_delay, bool& update_complete, 
							  bool& update_succeeded );
};

#endif
