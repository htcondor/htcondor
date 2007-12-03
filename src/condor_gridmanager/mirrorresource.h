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


#ifndef MIRRORRESOURCE_H
#define MIRRORRESOURCE_H

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

#include "baseresource.h"
#include "gahp-client.h"

#define ACQUIRE_DONE		0
#define ACQUIRE_QUEUED		1
#define ACQUIRE_FAILED		2

class MirrorJob;
class MirrorResource;

class MirrorResource : public BaseResource
{
 public:

	MirrorResource( const char *resource_name );
	~MirrorResource();

	void Reconfig();
	void RegisterJob( MirrorJob *job, const char *submitter_id );

	int DoScheddPoll();

	static MirrorResource *FindOrCreateResource( const char *resource_name );
	static void setPollInterval( int new_interval )
		{ scheddPollInterval = new_interval; }

	static int scheddPollInterval;

	StringList submitter_ids;
	MyString submitter_constraint;
	int scheddPollTid;
	char *mirrorScheddName;
	bool scheddUpdateActive;
	bool scheddStatusActive;

 private:
	static HashTable <HashKey, MirrorResource *> ResourcesByName;

	GahpClient *gahp;
};

#endif
