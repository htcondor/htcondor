/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
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
