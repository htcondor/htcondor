/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
#ifndef GM_COMMON_H
#define GM_COMMON_H

#include "condor_common.h"
#include "list.h"


extern char *gramCallbackContact;
extern char *gassServerUrl;

#define JOB_UE_SUBMITTED		1
#define JOB_UE_RUNNING			2
#define JOB_UE_FAILED			3
#define JOB_UE_DONE				4
#define JOB_UE_CANCELED			5
#define JOB_UE_STATE_CHANGE		6
#define JOB_UE_JM_RESTARTED		7
#define JOB_UE_RESUBMIT			8	// We are returning the job to an
									// unsubmitted state following a failed
									// submission/execution. Erase knowledge
									// of the old submission from the schedd,
									// then resubmit it.
#define JOB_UE_NOT_SUBMITTED	9	// We were told by a restarted jobmanager
									// that the previous jobmanager hadn't
									// actually submitted the job.


class GlobusJob;

struct JobUpdateEvent {
	GlobusJob *job;
	int event;
	int subtype;
};

extern List<JobUpdateEvent> JobUpdateEventQueue;

void addJobUpdateEvent( GlobusJob *job, int event, int subtype = 0 );
void setUpdate();
void rehashJobContact( GlobusJob *job, const char *old_contact,
					   const char *new_contact );

#endif
