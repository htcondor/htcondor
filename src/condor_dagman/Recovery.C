
#ifndef _DAGMAN_RECOVERY_LOG_
#define _DAGMAN_RECOVERY_LOG_

#include "DagMan.h"
#include "log.h"
#include <iostream.h>

extern DagMan *dagman;

LogSetCondorID::LogSetCondorID(int cluster, int proc, int subproc, int Job)
{
  op_type = DAGMAN_SetCondorID;
  //  body_size = sizeof(SubmittedJobID) + sizeof(CondorCluster) +
  //  sizeof(CondorProc) + sizeof(CondorSubproc);

  CondorCluster = cluster;
  CondorProc = proc;
  CondorSubproc = subproc;

  SubmittedJobID = Job;
}

int 
LogSetCondorID::Play(void *data_structure)
{
  NodeInfo *UpdateThisNode = (NodeInfo *)data_structure;

  int status;

  status = UpdateThisNode->UpdateCondorInfo(CondorCluster, 
					    CondorProc, 
					    CondorSubproc);
  if (status != OK)
    {
      return 0;
    }
  
  status = UpdateThisNode->SetNodeStatus(RUNNING);
  if (status != OK)
    {
      return 0;
    }

  //  dagman->IncrJobsRunning();

  return 1;
}

int
LogSetCondorID::WriteBody(int fd)
{
  char value[20];

  sprintf(value, "%d %d %d %d", 
	  SubmittedJobID,
	  CondorCluster,
	  CondorProc, 
	  CondorSubproc);

  return ( write(fd, value, strlen(value)) );
}

int
LogSetCondorID::ReadBody(int fd)
{
  char *value;
  int rval;
  
  rval = readline(fd, value);
  
  if (rval < 0)
    {
      delete value;
      return (-1);
    }
#ifdef DEBUG
  cout << "LogSetCondorID::ReadBody - read -> " << value << endl;
#endif

  sscanf(value, "%d %d %d %d",
	 &SubmittedJobID,
	 &CondorCluster,
	 &CondorProc,
	 &CondorSubproc);

  delete value;
  // return NodeID of the log entry
  return (SubmittedJobID);
}

LogTerminateJob::LogTerminateJob(int JobID)
{
  op_type = DAGMAN_TerminateJob;
  //  body_size = sizeof(SubmittedJobID);

  SubmittedJobID = JobID;
}

int 
LogTerminateJob::Play(void *data_structure)
{
  int *NodeID = (int *)data_structure;
  
  int status;

#ifdef DEBUG
  cout << "Inside LogTerminateJob::Play() - id value is " << *NodeID << endl;
#endif

  status = dagman->TerminateJob(*NodeID);
  if (status != OK)
    {
      return 0;
    }

  return 1;
}

int
LogTerminateJob::WriteBody(int fd)
{
  char value[4];

  sprintf(value, "%d", SubmittedJobID);

  return ( write(fd, value, strlen(value)) );  
}

int
LogTerminateJob::ReadBody(int fd)
{
  int rval;
  char *value;

  rval = readline(fd, value);
  if (rval < 0)
    {
      delete value;
      return -1;
    }
#ifdef DEBUG
  cout << "LogTerminateJob::ReadBody - read -> " << value << endl;
#endif

  sscanf(value, "%d", &SubmittedJobID);

  delete value;
  // return NodeID for the recovery log entry
  return SubmittedJobID;
}

LogSubmitJob::LogSubmitJob(int JobID)
{
  op_type = DAGMAN_SubmitJob;
  //  body_size = sizeof(SubmittedJobID);

  SubmittedJobID = JobID;
}

int 
LogSubmitJob::Play(void *data_structure)
{

#ifdef DEBUG
  cout << "Inside LogSubmitJob::Play()" << endl;
#endif

  // this function does nothing

  // this is because when we are in recovery mode, we do NOT
  // want DagMan to resubmit the job to condor. In normal mode,
  // DagMan calls condor_submit after logging the Submit action
  // in the recovery log

  return 1;
}

int
LogSubmitJob::WriteBody(int fd)
{
  char value[4];

  sprintf(value, "%d", SubmittedJobID);

  return ( write(fd, value, strlen(value)) );  
}

int
LogSubmitJob::ReadBody(int fd)
{
  int rval;
  char *value;

  rval = readline(fd, value);
  if (rval < 0)
    {
      delete value;
      return -1;
    }

  sscanf(value, "%d", &SubmittedJobID);

#ifdef DEBUG
  cout << "LogSubmitJob::ReadBody - " << SubmittedJobID << endl;
#endif

  delete value;
  //  body_size = sizeof(SubmittedJobID);

  // return NodeID for recovery log entry
  return SubmittedJobID;
}

LogSubmitJobOK::LogSubmitJobOK(int JobID)
{
  op_type = DAGMAN_SubmitJobOK;
  //  body_size = sizeof(SubmittedJobID);

  SubmittedJobID = JobID;
}

int 
LogSubmitJobOK::Play(void *data_structure)
{

#ifdef DEBUG
  cout << "Inside LogSubmitJob::Play()" << endl;
#endif

  // this function does nothing

  // The only purpose of LogSubmitJobOK is to make sure that
  // the condor_submit has not failed

  return 1;
}

int
LogSubmitJobOK::WriteBody(int fd)
{
  char value[4];

  sprintf(value, "%d", SubmittedJobID);

  return ( write(fd, value, strlen(value)) );  
}

int
LogSubmitJobOK::ReadBody(int fd)
{
  int rval;
  char *value;

  rval = readline(fd, value);
  if (rval < 0)
    {
      delete value;
      return -1;
    }

  sscanf(value, "%d", &SubmittedJobID);

#ifdef DEBUG
  cout << "LogSubmitJobOK::ReadBody - " << SubmittedJobID << endl;
#endif

  delete value;
  //  body_size = sizeof(SubmittedJobID);

  // return NodeID for recovery log entry
  return SubmittedJobID;
}

LogRecord *
InstantiateLogEntry(int fd, int type)
{

#ifdef DEBUG
  cout << "Inside InstantiateLogEntry" << endl;
#endif

  LogRecord *log_rec;

  switch(type)
    {
    case DAGMAN_SubmitJob:
#ifdef DEBUG
      cout << "DAGMAN_SubmitJob" << endl;
#endif
      log_rec = new LogSubmitJob(-1);
      break;
    case DAGMAN_SetCondorID:
#ifdef DEBUG
      cout << "DAGMAN_SetCondorID" << endl;
#endif
      log_rec = new LogSetCondorID(-1, -1, -1, -1);
      break;
    case DAGMAN_TerminateJob:
#ifdef DEBUG
      cout << "DAGMAN_TerminateJob" << endl;
#endif      
      log_rec = new LogTerminateJob(-1);
      break;
    default:
#ifdef DEBUG
      cout << "InstantiateLogEntry - returning default case" << endl;
#endif
      return 0;
      break;
    }

  //  log_rec->ReadBody(fd);
  return log_rec;
}

LogUndoJob::LogUndoJob(int JobID)
{
  op_type = DAGMAN_UndoJob;
  //  body_size = sizeof(SubmittedJobID);

  SubmittedJobID = JobID;
}

int 
LogUndoJob::Play(void *data_structure)
{
#ifdef DEBUG
  cout << "Inside LogSubmitJob::Play()" << endl;
#endif
  // this function has not been implemented yet

  return 1;
}

int
LogUndoJob::WriteBody(int fd)
{
  char value[4];

  sprintf(value, "%d", SubmittedJobID);

  return ( write(fd, value, strlen(value)) );  
}

int
LogUndoJob::ReadBody(int fd)
{
  int rval;
  char *value;

  rval = readline(fd, value);
  if (rval < 0)
    {
      delete value;
      return -1;
    }

  sscanf(value, "%d", &SubmittedJobID);

#ifdef DEBUG
  cout << "LogSubmitJob::ReadBody - " << SubmittedJobID << endl;
#endif

  delete value;
  //  body_size = sizeof(SubmittedJobID);

  // return NodeID for recovery log entry
  return SubmittedJobID;
}


#endif

