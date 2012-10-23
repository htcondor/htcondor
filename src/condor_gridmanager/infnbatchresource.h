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

  
#ifndef INFNBATCHRESOURCE_H
#define INFNBATCHRESOURCE_H
    
#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

#include "infnbatchjob.h"
#include "baseresource.h"
#include "gahp-client.h"

class INFNBatchJob;

class INFNBatchResource : public BaseResource
{
public:
	void Reconfig();

	static const char *HashName( const char * batch_type, 
								 const char * resource_name );

	static INFNBatchResource* FindOrCreateResource( const char * batch_type, 
													const char * resource_name );

	GahpClient *gahp;

	INFNBatchResource( const char * batch_type,
					   const char * resource_name );

	~INFNBatchResource();

	static HashTable <HashKey, INFNBatchResource *> ResourcesByName;

	const char *ResourceType();

	const char *GetHashName();

	void PublishResourceAd( ClassAd *resource_ad );

	bool GahpIsRemote() { return m_gahpIsRemote; };
	const char *RemoteHostname() { return m_remoteHostname.c_str(); };

private:
	void DoPing(time_t & ping_delay, 
				bool & ping_complete, 
				bool & ping_succeeded  );

	std::string m_batchType;
	bool m_gahpIsRemote;
	std::string m_remoteHostname;
};    
  
#endif
