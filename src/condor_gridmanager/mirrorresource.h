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

#ifndef MIRRORRESOURCE_H
#define MIRRORRESOURCE_H

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

#include "mirrorjob.h"
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
