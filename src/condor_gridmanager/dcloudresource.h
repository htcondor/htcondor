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

  
#ifndef DCLOUDRESOURCE_H
#define DCLOUDRESOURCE_H
    
#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

#include "dcloudjob.h"
#include "baseresource.h"
#include "gahp-client.h"

#define DCLOUD_RESOURCE_NAME "deltacloud"
  
class DCloudJob;

class DCloudResource : public BaseResource
{
public:
	void Reconfig();
	
	static const char *HashName( const char *resource_name, 
								 const char *username, 
								 const char *password );
	
	static DCloudResource* FindOrCreateResource( const char *resource_name, 
												 const char *username, 
												 const char *password );

	GahpClient *gahp;
	GahpClient *status_gahp;

	DCloudResource(const char *resource_name, 
				   const char *username, 
				   const char *password );
	
	~DCloudResource();	

	static HashTable <HashKey, DCloudResource *> ResourcesByName;

	const char *ResourceType();

	const char *GetHashName();

	void PublishResourceAd( ClassAd *resource_ad );

	BatchStatusResult StartBatchStatus();
	BatchStatusResult FinishBatchStatus();
	GahpClient * BatchGahp();

private:
	void DoPing(unsigned & ping_delay,
				bool & ping_complete,
				bool & ping_succeeded  );
	
	char* m_username;
	char* m_password;
};    
  
#endif
