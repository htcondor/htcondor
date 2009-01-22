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

  
#ifndef AMAZONRESOURCE_H
#define AMAZONRESOURCE_H
    
#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

#include "amazonjob.h"
#include "baseresource.h"
#include "gahp-client.h"

#define AMAZON_RESOURCE_NAME "amazon"
  
class AmazonJob;
class AmazonResource;

class AmazonResource : public BaseResource
{
public:
	void Reconfig();
	
	static const char *HashName( const char * resource_name, 
								 const char * public_key_file, 
								 const char * private_key_file );
	
	static AmazonResource* FindOrCreateResource( const char * resource_name, 
												 const char * public_key_file, 
												 const char * private_key_file );

	GahpClient *gahp;

	AmazonResource(const char * resource_name, 
				   const char * public_key_file, 
				   const char * private_key_file );
	
	~AmazonResource();	

	static HashTable <HashKey, AmazonResource *> ResourcesByName;

	const char *ResourceType();

	const char *GetHashName();

	void PublishResourceAd( ClassAd *resource_ad );

private:
	void DoPing(time_t & ping_delay, 
				bool & ping_complete, 
				bool & ping_succeeded  );
	
	char* m_public_key_file;
	char* m_private_key_file;	
};    
  
#endif
