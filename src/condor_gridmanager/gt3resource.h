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

#ifndef GT3RESOURCE_H
#define GT3RESOURCE_H

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

#include "proxymanager.h"
#include "gt3job.h"
#include "baseresource.h"
#include "gahp-client.h"


class GT3Job;
class GT3Resource;

class GT3Resource : public BaseResource
{
 protected:
	GT3Resource( const char *resource_name, const char *proxy_subject );
	~GT3Resource();

 public:
	bool Init();
	void Reconfig();
	void RegisterJob( GT3Job *job, bool already_submitted );
	void UnregisterJob( GT3Job *job );
	void RequestPing( GT3Job *job );
	bool RequestSubmit( GT3Job *job );
	void SubmitComplete( GT3Job *job );
	void CancelSubmit( GT3Job *job );

	bool IsEmpty();
	bool IsDown();

	time_t getLastStatusChangeTime() { return lastStatusChange; }

	static const char *CanonicalName( const char *name );
	static const char *HashName( const char *resource_name,
								 const char *proxy_subject );

	static GT3Resource *FindOrCreateResource( const char *resource_name,
											  const char *proxy_subject );

	static void setProbeInterval( int new_interval )
		{ probeInterval = new_interval; }

	static void setProbeDelay( int new_delay )
		{ probeDelay = new_delay; }

	static void setGahpCallTimeout( int new_timeout )
		{ gahpCallTimeout = new_timeout; }

	// This should be private, but GT3Job references it directly for now
	static HashTable <HashKey, GT3Resource *> ResourcesByName;

 private:
	int DoPing();

	bool initialized;

	char *proxySubject;
	bool resourceDown;
	bool firstPingDone;
	int pingTimerId;
	time_t lastPing;
	time_t lastStatusChange;
	List<GT3Job> registeredJobs;
	List<GT3Job> pingRequesters;
	// jobs that are currently executing a submit
	List<GT3Job> submitsInProgress;
	// jobs that want to submit but can't due to submitLimit
	List<GT3Job> submitsQueued;
	// jobs allowed to submit under jobLimit
	List<GT3Job> submitsAllowed;
	// jobs that want to submit but can't due to jobLimit
	List<GT3Job> submitsWanted;
	static int probeInterval;
	static int probeDelay;
	int submitLimit;		// max number of submit actions
	int jobLimit;			// max number of submitted jobs
	static int gahpCallTimeout;

	GahpClient *gahp;
};

#endif
