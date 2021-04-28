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


#ifndef GCERESOURCE_H
#define GCERESOURCE_H

#include "condor_common.h"
#include "condor_daemon_core.h"
#include <map>

#include "gcejob.h"
#include "baseresource.h"
#include "gahp-client.h"

#define GCE_RESOURCE_NAME "gce"

class GCEJob;

class GCEResource : public BaseResource
{
public:
	void Reconfig();

	static std::string & HashName( const char *resource_name,
								 const char *project,
								 const char *zone,
								 const char *auth_file,
								 const char *account );

	static GCEResource* FindOrCreateResource( const char *resource_name,
											  const char *project,
											  const char *zone,
											  const char *auth_file,
											  const char *account );

	GahpClient *gahp;
	GahpClient *status_gahp;

	GCEResource( const char *resource_name,
				 const char *project,
				 const char *zone,
				 const char *auth_file,
				 const char *account );

	~GCEResource();

	static std::map <std::string, GCEResource *> ResourcesByName;

	const char *ResourceType();

	const char *GetHashName();

	void PublishResourceAd( ClassAd *resource_ad );

	bool hadAuthFailure() const { return m_hadAuthFailure; }

	std::string authFailureMessage;

	BatchStatusResult StartBatchStatus();
	BatchStatusResult FinishBatchStatus();
	GahpClient * BatchGahp() { return status_gahp; }

	HashTable< std::string, GCEJob * > jobsByInstanceID;

private:
	void DoPing(unsigned & ping_delay,
				bool & ping_complete,
				bool & ping_succeeded  );

	char *m_auth_file;
	char *m_account;
	char *m_project;
	char *m_zone;

	bool m_hadAuthFailure;
};

#endif
