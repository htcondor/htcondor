
#ifndef GRIDMANAGER_H
#define GRIDMANAGER_H

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "user_log.c++.h"
#include "classad_hashtable.h"
#include "list.h"

#include "gahp-client.h"
#include "globusresource.h"
#include "globusjob.h"


#define UA_UPDATE_JOB_AD			0x0001
#define UA_DELETE_FROM_SCHEDD		0x0002
#define UA_LOG_SUBMIT_EVENT			0x0004
#define UA_LOG_EXECUTE_EVENT		0x0008
#define UA_LOG_SUBMIT_FAILED_EVENT	0x0010
#define UA_LOG_TERMINATE_EVENT		0x0020
#define UA_LOG_ABORT_EVENT			0x0040
#define UA_LOG_EVICT_EVENT			0x0080
#define UA_HOLD_JOB					0x0100
#define UA_FORGET_JOB				0x0200

extern char *gramCallbackContact;
extern char *ScheddAddr;
extern char *X509Proxy;
extern bool useDefaultProxy;

extern time_t Proxy_Expiration_Time;

extern GahpClient GahpMain;

extern char *gassServerUrl;

// initialization
void Init();
void Register();

// maintainence
void Reconfig();
	
bool addScheddUpdateAction( GlobusJob *job, int actions, int request_id = 0 );
void removeScheddUpdateAction( GlobusJob *job );
void rehashJobContact( GlobusJob *job, const char *old_contact,
					   const char *new_contact );

void gramCallbackHandler( void *user_arg, char *job_contact, int state,
						  int errorcode );

UserLog *InitializeUserLog( ClassAd *job_ad );
bool WriteExecuteEventToUserLog( ClassAd *job_ad );
bool WriteAbortEventToUserLog( ClassAd *job_ad );
bool WriteTerminateEventToUserLog( ClassAd *job_ad );
bool WriteEvictEventToUserLog( ClassAd *job_ad );
bool WriteHoldEventToUserLog( ClassAd *job_ad );
bool WriteGlobusSubmitEventToUserLog( ClassAd *job_ad );
bool WriteGlobusSubmitFailedEventToUserLog( ClassAd *job_ad,
											int failure_code );
bool WriteGlobusResourceUpEventToUserLog( ClassAd *job_ad );
bool WriteGlobusResourceDownEventToUserLog( ClassAd *job_ad );


#endif
