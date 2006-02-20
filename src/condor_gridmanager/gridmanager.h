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

#ifndef GRIDMANAGER_H
#define GRIDMANAGER_H

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "user_log.c++.h"
#include "classad_hashtable.h"
#include "list.h"
#include "daemon.h"
#include "dc_schedd.h"

#include "basejob.h"

// Special value for a daemon-core timer id which indicates that there
// is no timer currently registered (for variables holding a timer id) or
// that no timer should be signalled when one normally would be (for
// functions that take as an argument a timer id to be signalled when
// something happens).
#define TIMER_UNSET -1

extern char *ScheddAddr;
extern char *ScheddJobConstraint;
extern char *GridmanagerScratchDir;
extern char *Owner;

// initialization
void Init();
void Register();

// maintainence
void Reconfig();

bool requestScheddUpdate( BaseJob *job );
bool requestScheddVacate( BaseJob *job, action_result_t &result );
bool requestJobStatus( BaseJob *job, int &job_status );
bool requestJobStatus( PROC_ID job_id, int tid, int &job_status );
void requestScheddUpdateNotification( int timer_id );


#endif
