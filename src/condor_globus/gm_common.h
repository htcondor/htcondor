#ifndef GM_COMMON_H
#define GM_COMMON_H

#include "condor_common.h"
#include "list.h"


extern char *gramCallbackContact;
extern char *gassServerUrl;

// Job attributes that need to be updated on the schedd
#define JOB_STATE		1
#define JOB_CONTACT		2
#define JOB_REMOVED		4

#define JOB_UE_UPDATE_STATE			1
#define JOB_UE_UPDATE_CONTACT		2
#define JOB_UE_REMOVE_JOB			3
#define JOB_UE_HOLD_JOB				4
#define JOB_UE_ULOG_EXECUTE			5
#define JOB_UE_ULOG_TERMINATE		6
#define JOB_UE_EMAIL				7
#define JOB_UE_CALLBACK				8

class GlobusJob;

struct JobUpdateEvent {
	GlobusJob *job;
	int event;
	int subtype;
};

extern List<JobUpdateEvent> JobUpdateEventQueue;

void addJobUpdateEvent( GlobusJob *job, int event, int subtype = 0 );
void setUpdate();

#endif
