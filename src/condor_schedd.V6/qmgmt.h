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
void InitJobQueue(const char *job_queue_name);
void InitQmgmt();
void CleanJobQueue();
int handle_q(Service *, int, Stream *sock);
int GetJobList(const char *constraint, ClassAdList &list);
#if defined(__cplusplus)
}
#endif

#endif /* _QMGMT_H */
