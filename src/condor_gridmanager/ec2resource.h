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

  
#ifndef EC2RESOURCE_H
#define EC2RESOURCE_H
    
#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

#include "ec2job.h"
#include "baseresource.h"
#include "gahp-client.h"

#define EC2_RESOURCE_NAME "ec2"
  
class EC2Job;
class EC2Resource;

class EC2Resource : public BaseResource
{
public:
	void Reconfig();
	
	static const char *HashName( const char * resource_name, 
								 const char * public_key_file, 
								 const char * private_key_file );
	
	static EC2Resource* FindOrCreateResource( const char * resource_name, 
											  const char * public_key_file, 
											  const char * private_key_file );

	GahpClient *gahp;
	GahpClient *status_gahp;

	EC2Resource(const char * resource_name, 
				const char * public_key_file, 
				const char * private_key_file );
	
	~EC2Resource();	

	static HashTable <HashKey, EC2Resource *> ResourcesByName;

	const char *ResourceType();

	const char *GetHashName();

	void PublishResourceAd( ClassAd *resource_ad );

    bool hadAuthFailure() { return m_hadAuthFailure; }

	bool ServerTypeQueried( EC2Job *job = NULL );
	bool ClientTokenWorks( EC2Job *job = NULL );
	bool ShuttingDownTrusted( EC2Job *job = NULL );

    std::string authFailureMessage;

	std::string m_serverType;

    BatchStatusResult StartBatchStatus();
    BatchStatusResult FinishBatchStatus();
    GahpClient * BatchGahp() { return status_gahp; }

    HashTable< HashKey, EC2Job * > jobsByInstanceID;
    HashTable< HashKey, EC2Job * > spotJobsByRequestID;
    
private:
	void DoPing(unsigned & ping_delay,
				bool & ping_complete,
				bool & ping_succeeded  );
	
	char* m_public_key_file;
	char* m_private_key_file;

	bool m_hadAuthFailure;
	bool m_checkSpotNext;
};    
  
#endif
