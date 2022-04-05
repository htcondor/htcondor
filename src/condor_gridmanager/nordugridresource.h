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


#ifndef NORDUGRIDRESOURCE_H
#define NORDUGRIDRESOURCE_H

#include "condor_common.h"
#include "condor_daemon_core.h"
#include <map>

#include "baseresource.h"
#include "gahp-client.h"

class NordugridJob;
class NordugridResource;

class NordugridResource : public BaseResource
{
 public:

	NordugridResource( const char *resource_name, const Proxy *proxy );
	~NordugridResource();

	const char *ResourceType();
	void Reconfig();

	static std::string & HashName( const char *resource_name,
								 const char *proxy_subject );
	static NordugridResource *FindOrCreateResource( const char *resource_name,
													const Proxy *proxy );

	const char *GetHashName();

	void PublishResourceAd( ClassAd *resource_ad );

	char *proxySubject;
	char *proxyFQAN;
	GahpClient *gahp;

	static std::map <std::string, NordugridResource *> ResourcesByName;

 private:
	void DoPing( unsigned& ping_delay, bool& ping_complete,
				 bool& ping_succeeded  );

	void DoJobStatus();

	bool m_jobStatusActive;
	int m_jobStatusTid;
	GahpClient *m_statusGahp;
};

#endif
