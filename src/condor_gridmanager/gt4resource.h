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

#ifndef GT4RESOURCE_H
#define GT4RESOURCE_H

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

#include "proxymanager.h"
#include "gt4job.h"
#include "baseresource.h"
#include "gahp-client.h"


class GT4Job;
class GT4Resource;

////////////from gridmanager.C
extern HashTable <HashKey, GT4Resource *> GT4ResourcesByName;
//////////////////////////////

class GT4Resource : public BaseResource
{
 protected:
	GT4Resource( const char *resource_name, const char *proxy_subject );
	~GT4Resource();

 public:
	bool Init();
	void Reconfig();
	void RegisterJob( GT4Job *job, bool already_submitted );
	void UnregisterJob( GT4Job *job );
	void RequestPing( GT4Job *job );
	bool RequestSubmit( GT4Job *job );
	void SubmitComplete( GT4Job *job );
	void CancelSubmit( GT4Job *job );

	bool IsEmpty();
	bool IsDown();

	time_t getLastStatusChangeTime() { return lastStatusChange; }

	static const char *CanonicalName( const char *name );
	static const char *HashName( const char *resource_name,
								 const char *proxy_subject );

	static GT4Resource *FindOrCreateResource( const char *resource_name,
											  const char *proxy_subject );

	static void setProbeInterval( int new_interval )
		{ probeInterval = new_interval; }

	static void setProbeDelay( int new_delay )
		{ probeDelay = new_delay; }

	static void setGahpCallTimeout( int new_timeout )
		{ gahpCallTimeout = new_timeout; }

	// This should be private, but GT4Job references it directly for now
	static HashTable <HashKey, GT4Resource *> ResourcesByName;

 private:
	int DoPing();

	bool initialized;

	char *proxySubject;
	bool resourceDown;
	bool firstPingDone;
	int pingTimerId;
	time_t lastPing;
	time_t lastStatusChange;
	List<GT4Job> registeredJobs;
	List<GT4Job> pingRequesters;
	// jobs that are currently executing a submit
	List<GT4Job> submitsInProgress;
	// jobs that want to submit but can't due to submitLimit
	List<GT4Job> submitsQueued;
	// jobs allowed to submit under jobLimit
	List<GT4Job> submitsAllowed;
	// jobs that want to submit but can't due to jobLimit
	List<GT4Job> submitsWanted;
	static int probeInterval;
	static int probeDelay;
	int submitLimit;		// max number of submit actions
	int jobLimit;			// max number of submitted jobs
	static int gahpCallTimeout;

	GahpClient *gahp;
};

#endif
