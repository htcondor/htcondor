/***************************************************************
 *
 * Copyright (C) 1990-2021, Condor Team, Computer Sciences Department,
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


#ifndef ARCRESOURCE_H
#define ARCRESOURCE_H

#include "condor_common.h"
#include "condor_daemon_core.h"
#include <map>

#include "baseresource.h"
#include "gahp-client.h"

class ArcJob;
class ArcResource;

class ArcResource : public BaseResource
{
 public:

	ArcResource( const char *resource_name, int gahp_id,
	             const Proxy *proxy, const std::string& token_file );
	~ArcResource();

	const char *ResourceType();
	void Reconfig();

	static std::string & HashName( const char *resource_name,
	                               int gahp_id,
	                               const char *proxy_subject,
	                               const std::string& token_file );
	static ArcResource *FindOrCreateResource( const char *resource_name,
	                                          int gahp_id,
	                                          const Proxy *proxy,
	                                          const std::string& token_file );

	const char *GetHashName();

	void PublishResourceAd( ClassAd *resource_ad );

	char *proxySubject;
	char *proxyFQAN;
	std::string m_tokenFile;
	GahpClient *gahp;
	int m_gahpId;

	static std::map <std::string, ArcResource *> ResourcesByName;

 private:
	void DoPing(time_t& ping_delay, bool& ping_complete,
				 bool& ping_succeeded  );

	void DoJobStatus(int timerID);

	bool m_jobStatusActive;
	int m_jobStatusTid;
	GahpClient *m_statusGahp;
};

#endif
