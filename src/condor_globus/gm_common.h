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
