
#ifndef _CONDORSUBMIT_H_
#define _CONDORSUBMIT_H_

//
// Submits a job to condor.
//
// Returns 0 on success, non-zero on failure. On success, cluster, proc,
// and subproc will hold condor identifiers for the new job. On failure,
// errorString will contain a description of the error that occurred.
//
// errorString will point to static memory that is overwritten each
// time any function in this file is called.
//
//
int CondorSubmit(const char *cmdfile, char *&errorString,
		 int &cluster, int &proc, int &subproc);

#endif

