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
#if !defined(_QMGMT_H)
#define _QMGMT_H

#include "condor_classad.h"
#include "condor_io.h"

#define NEW_BORN	1
#define DEATHS_DOOR	2


void PrintQ();
class Service;

#if defined(__cplusplus)
extern "C" {
#endif
void InitJobQueue(const char *job_queue_name,int max_historical_logs);
void InitQmgmt();
void SetMaxHistoricalLogs(int max_historical_logs);
void CleanJobQueue();
void DestroyJobQueue( void );
int handle_q(Service *, int, Stream *sock);
bool setQSock( ReliSock* rsock );
void unsetQSock( void );
void BeginTransaction();
void CommitTransaction();
void AbortTransaction();
void dirtyJobQueue( void );
bool isQueueSuperUser( const char* user );
bool OwnerCheck( ClassAd *ad, const char *test_owner );
bool Reschedule();

int get_myproxy_password_handler(Service *, int, Stream *sock);
#if defined(__cplusplus)
}
#endif

#if defined(__cplusplus)
bool OwnerCheck(int,int);
bool OwnerCheck(ClassAd *, const char *);
#endif

#endif /* _QMGMT_H */
