#ifndef _BUILD_CONTEXT_H_
#define _BUILD_CONTEXT_H_

#include "context.h"

#define MAX_MACHINE_NUM  500
#define MAX_JOB_NUM 500

class Ptr
{
 public:
  Ptr() {}
  ~Ptr() { delete next; }
  Context *machine;
  class Ptr *next;
};

/*
class Job
{
 public:
  Job() {}
  ~Job() { delete machineInfo; }
  Context jobInfo;
  Ptr *machineInfo;
};
*/

void Build_machine_context(FILE *, ContextList *);
// void Build_job_context(FILE *, class Job *, int *);

#endif
