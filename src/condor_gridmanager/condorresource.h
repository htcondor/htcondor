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


#ifndef CONDORRESOURCE_H
#define CONDORRESOURCE_H

#include "condor_common.h"
#include "condor_daemon_core.h"

#include "baseresource.h"
#include "gahp-client.h"

#define ACQUIRE_DONE		0
#define ACQUIRE_QUEUED		1
#define ACQUIRE_FAILED		2

class CondorJob;
class CondorResource;

class CondorResource : public BaseResource
{
 public:

	CondorResource( const char *resource_name, const char *pool_name,
					const Proxy *proxy );
	~CondorResource();

	const char *ResourceType();
	void Reconfig();
	void CondorRegisterJob( CondorJob *job, const char *submitter_id );
	void UnregisterJob( BaseJob *job );

	void DoScheddPoll();

	static const char *HashName( const char *resource_name,
								 const char *pool_name,
								 const char *proxy_subject );
	static CondorResource *FindOrCreateResource( const char *resource_name,
												 const char *pool_name,
												 const Proxy *proxy );

	StringList submitter_ids;
	std::string submitter_constraint;
	int scheddPollTid;
	char *scheddName;
	char *poolName;
	bool scheddStatusActive;
	char *proxySubject;
	char *proxyFQAN;

	static HashTable <HashKey, CondorResource *> ResourcesByName;

		// Used by DoPollSchedd() to share poll results across all
		// CondorResources of the same remote schedd
	struct ScheddPollInfo {
		time_t m_lastPoll;
		bool m_pollActive;
		List<CondorJob> m_submittedJobs;
	};
	static HashTable <HashKey, ScheddPollInfo *> PollInfoByName;

	const char *GetHashName();

	void PublishResourceAd( ClassAd *resource_ad );

 private:
	void DoPing( time_t& ping_delay, bool& ping_complete,
				 bool& ping_succeeded  );

	void DoUpdateLeases( time_t& update_delay, bool& update_complete, 
						 SimpleList<PROC_ID>& update_succeeded );

	GahpClient *gahp;
	GahpClient *ping_gahp;
	GahpClient *lease_gahp;
};

#endif
