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

#ifndef GRIDMANAGER_H
#define GRIDMANAGER_H

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "user_log.c++.h"
#include "classad_hashtable.h"
#include "list.h"

#include "proxymanager.h"
#include "globusresource.h"
#include "globusjob.h"
#include "gahp-client.h"


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
extern char *ScheddJobConstraint;
extern char *GridmanagerScratchDir;

extern GahpClient GahpMain;

extern char *gassServerUrl;

extern HashTable <HashKey, GlobusJob *> JobsByContact;

// initialization
void Init();
void Register();

// maintainence
void Reconfig();
	
bool InitializeGahp( const char *proxy_filename );

bool addScheddUpdateAction( GlobusJob *job, int actions, int request_id = 0 );
void removeScheddUpdateAction( GlobusJob *job );
void rehashJobContact( GlobusJob *job, const char *old_contact,
					   const char *new_contact );
char *globusJobId( const char *contact );

void gramCallbackHandler( void *user_arg, char *job_contact, int state,
						  int errorcode );

UserLog *InitializeUserLog( ClassAd *job_ad );
bool WriteExecuteEventToUserLog( ClassAd *job_ad );
bool WriteAbortEventToUserLog( ClassAd *job_ad );
bool WriteTerminateEventToUserLog( GlobusJob *job_ad );
bool WriteEvictEventToUserLog( ClassAd *job_ad );
bool WriteHoldEventToUserLog( ClassAd *job_ad );
bool WriteGlobusSubmitEventToUserLog( ClassAd *job_ad );
bool WriteGlobusSubmitFailedEventToUserLog( ClassAd *job_ad,
											int failure_code );
bool WriteGlobusResourceUpEventToUserLog( ClassAd *job_ad );
bool WriteGlobusResourceDownEventToUserLog( ClassAd *job_ad );


#endif
