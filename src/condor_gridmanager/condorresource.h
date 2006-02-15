/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#ifndef CONDORRESOURCE_H
#define CONDORRESOURCE_H

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

#include "condorjob.h"
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
					const char *proxy_subject );
	~CondorResource();

	void Reconfig();
	void RegisterJob( CondorJob *job, const char *submitter_id );
	void UnregisterJob( CondorJob *job );

	int DoScheddPoll();

	static const char *HashName( const char *resource_name,
								 const char *pool_name,
								 const char *proxy_subject );
	static CondorResource *FindOrCreateResource( const char *resource_name,
												 const char *pool_name,
												 const char *proxy_subject );
	static void setPollInterval( int new_interval )
		{ scheddPollInterval = new_interval; }

	static int scheddPollInterval;

	StringList submitter_ids;
	MyString submitter_constraint;
	int scheddPollTid;
	char *scheddName;
	char *poolName;
	bool scheddStatusActive;
	char *proxySubject;

 private:
	void DoPing( time_t& ping_delay, bool& ping_complete,
				 bool& ping_succeeded  );

	void DoUpdateLeases( time_t& update_delay, bool& update_complete, 
						 SimpleList<PROC_ID>& update_succeeded );

	static HashTable <HashKey, CondorResource *> ResourcesByName;

	GahpClient *gahp;
	GahpClient *ping_gahp;
	GahpClient *lease_gahp;

		// Used by DoPollSchedd() to determine which jobs we expect to
		// see ads for. It is rebuilt on every poll.
	List<CondorJob> submittedJobs;
};

#endif
