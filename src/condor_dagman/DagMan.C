#ifndef _DAGMAN_
#define _DAGMAN_

#include "DagMan.h"
#include "util.h" // util.h contains some useful functions
#include <iostream.h>
#include <sys/types.h>
#include <sys/stat.h>

extern int DEFAULT_SLEEP;
extern int DagLog_fd;

static int CondorIDCompare(Condor_id a, Condor_id b) {
  // returns 0 if the 2 Condor_id structs have same value

  int result = IntCompare(a.cluster, b.cluster);
  if (result == 0) result = IntCompare(a.proc,    b.proc);
  if (result == 0) result = IntCompare(a.subproc, b.subproc);
  
  return result;
}

DagMan::DagMan() {
  _condorLogName  = NULL;
  _condorLogSize  = 0;
  _dagLogName     = NULL;
  _dagmanLockFile = NULL;

  _condorLogInitialized = false;

  _nJobs         = 0;
  _nJobsWaiting  = 0;
  _nJobsReady    = 0;
  _nJobsRunning  = 0;
  _nJobsDone     = 0;
  
  CondorJobs = new List<NodeInfo>;
  NodeIDCounter = 0;

  // the following is used to avoid 'broken pipe' messages
  install_sig_handler( SIGPIPE, SIG_IGN );
}

int DagMan::Init(const char *condorLog, 
                 const char *dagLog,
                 const char *dagmanLockFile,
                 const char *username) {
  char *jobname;
  
  delete [] _condorLogName;
  delete [] _dagLogName;
  
  _condorLogName = new char [strlen(condorLog) + 1];
  strcpy(_condorLogName, condorLog);
 
  _dagLogName = new char [strlen(dagLog) + 1];
  strcpy(_dagLogName, dagLog);

  _dagmanLockFile = new char [strlen(dagmanLockFile) +1];
  strcpy(_dagmanLockFile, dagmanLockFile);

  // Initialize the DAG log
  // _dagLog is of type UserLog, which is defined in condor_c++_util
    
  unlink(_dagLogName);
  _dagLog.initialize(username, _dagLogName, 0, getpid(), 0);

  return OK;
}

DagMan::~DagMan() {
  printf("Inside the DagMan() destructor .. \n");

  // remove the file being used as semaphore
  unlink(_dagmanLockFile);
  unlink (_dagLogName);
  
  delete CondorJobs;
  
  delete [] _condorLogName;
  delete [] _dagLogName;
}


int DagMan::GetNodePtr(NodeID id, NodeInfo *&job) {
  int status;

  //  fprintf(stderr, "Looking for NodeID %d\n", id);

  if (CondorJobs->IsEmpty()) return NOT_OK; // List is empty

  // find NodeInformation structure from list

  CondorJobs->Rewind();
  
  while ( (job = CondorJobs->Next()) ) {
    NodeID CurrentJobID;

    status = job->GetNodeID(CurrentJobID);
    if (status != OK) return NOT_OK;

    //  fprintf(stderr, "ListNodeID : %d\n", CurrentJobID);

    if (CurrentJobID == id) return OK;
  }

  return NOT_OK;
}

int
DagMan::AddNode(NodeInfo &NewJob, NodeID &id) {
  //  printf("Inside DagMan::AddNode()\n");

  int status;

  NodeIDCounter++;

  // add some verification here, so that 2 nodes are not the same

  NewJob.SetNodeID(NodeIDCounter);
  id = NodeIDCounter;

#ifdef DEBUG
  NewJob.Dump();
#endif

  CondorJobs->Append(NewJob);

#ifdef DEBUG
  PrintNodeList();
  fprintf(stderr, "dagMan has Added Node %d\n", id);
#endif

  _nJobs++;
  _nJobsWaiting++;

  return OK;
}

int DagMan::AddDependency(NodeID parent, NodeID child) {
  int status;
  NodeInfo *ParentJob, *ChildJob;

  status = GetNodePtr(parent, ParentJob);
  if (status != OK) return status;      // Node Information Not Found

  status = GetNodePtr(child, ChildJob);
  if (status != OK) return status; // Node Information Not Found

  // add node to Parent's outgoing list (read-only)

  status = ParentJob->AddOutgoing(child);
  
  if (status != OK) return status; // something wrong when adding dependency

  // add node to child's incoming list (read-only)

  status = ChildJob->AddIncoming(parent);

  if (status != OK) return status; // something wrong when adding dependency

  // since child is waiting on parent to finish, add parent to child's
  // waiting list. This will be updated as jobs are submitted and completed

  status = ChildJob->AddWaiting(parent);

  if (status != OK) return status; // something wrong when adding dependency
  
  return OK;
}

int DagMan::SetCondorLogFile(char *logfile) {
  int status;

  delete [] _condorLogName;
  _condorLogName = new char[strlen(logfile) + 1];
  if (_condorLogName == NULL) return NOT_OK; // Not enough Memory

  strcpy(_condorLogName, logfile);    

  return OK;
}

int DagMan::SetDagLogFile(char *logfile) {
  int status;

  delete [] _dagLogName;
  _dagLogName = new char[strlen(logfile) + 1];

  if (_dagLogName == NULL) return NOT_OK; // Not enough memory
  strcpy(_dagLogName, logfile);    

  return OK;
}

int DagMan::RunReadyJobs(int &nJobsSubmitted) {
  // nJobsSubmitted returns the number of jobs submitted by this 
  // calling of the function
  
  // Jobs are submitted when the NodeInfo waiting list is empty

  int status;

  NodeInfo *JobPointer;

  nJobsSubmitted = 0;
  
  CondorJobs->Rewind();

  while( (JobPointer = CondorJobs->Next()) ) {
    JobStatus NodeStatus;
    JobPointer->GetNodeStatus(NodeStatus);

    if ( (JobPointer->IsEmpty("waiting")) && (NodeStatus == READY) ) {
      // job ready to be submitted
      NodeID RunJobID;
      status = JobPointer->GetNodeID(RunJobID);
      if (status != OK) return status;

      SubmitJob(RunJobID);
      nJobsSubmitted++;
    }
  }

  return OK;
}

int DagMan::ProcessLogEvents(bool RecoveryMode, NodeID GetCondorJobID) {
  int status;
  
  // defined in condor_event.h
  ULogEvent *e;
  ULogEventOutcome outcome;

  bool done;
  bool found;

  if (!_condorLogInitialized) {
    _condorLog.initialize(_condorLogName);
    _condorLogInitialized = true;
  }
  
  done = false;

  NodeInfo *job;
  bool FoundNode = FALSE;
  Condor_id CondorID;
  Condor_id condorID;
  NodeID jobID;   


  while (!done) {
    outcome = _condorLog.readEvent(e);
      
    switch (outcome) {

     case ULOG_OK:

#ifdef DEBUG
      printf("Read an ULOG_OK event. Condor ID %d %d %d\n", 
             e->cluster,
             e->proc,
             e->subproc);
#endif	  

      FoundNode = false;
	  
      switch(e->eventNumber) {
       case ULOG_EXECUTABLE_ERROR:
       case ULOG_CHECKPOINTED:
       case ULOG_JOB_EVICTED:
       case ULOG_IMAGE_SIZE:
       case ULOG_SHADOW_EXCEPTION:

        // [j] Need to handle these cases later ...
        break;

       case ULOG_JOB_TERMINATED:
	  
        condorID.cluster = e->cluster;
        condorID.proc = e->proc;
        condorID.subproc = e->subproc;
	      
        // search through Node Information List to find the
        // job's Node
	      
        CondorJobs->Rewind();
	      
        while ( (!FoundNode) && (job = CondorJobs->Next()) ) {
          job->GetCondorInfo(CondorID.cluster, 
                             CondorID.proc,
                             CondorID.subproc);
		  
          if (!CondorIDCompare(CondorID, condorID)) {
            FoundNode = TRUE;
            break;
          }
        } 
	      
        if (FoundNode) {
          job->GetNodeID(jobID);
		  
          // Uncomment the following code to remove
          // the DAG_LOG facility

          /*
		     
            status = TerminateJob(jobID);
		  
            if (status != OK) {
            // What do we do in this case?
            // keep going or stop?
		      
            return status;
            }
          */

          // write TERM message to recovery log

          // To remove the DAG_LOG facility, comment the following
          // up until the *** 

          if (DagLog_fd < 0) {
            fprintf(stderr, "DagMan::ProcessLogEvent() : \
Failed to open DagMan Recovery Log %s\n", _dagLogName);
          } else {
            // Write Event SubmitJob to DagMan Recovery Log
#ifdef VERBOSE
            fprintf(stderr, "Logging TERMINATE message for Node %d\n",jobID);
#endif
            LogTerminateJob *log;
            log = new LogTerminateJob(jobID);
		      
            int temp;
            temp = log->Write(DagLog_fd);
            log->Play((void *)&jobID);
            fsync(DagLog_fd);
            delete log;
#ifdef DEBUG		      
            fprintf(stderr, "Wrote %d items\n", temp);
#endif
          }

          // ***
#ifdef DEBUG		  
          printf("Received TERM event for job %d (%d.%d.%d)\n",
                 jobID, e->cluster, e->proc, e->subproc);
#endif	
        }
	      
        if (!FoundNode) {
          fprintf(stderr,
                  "WARNING: TERM event for unknown job: %d.%d.%d\n",
                  e->cluster, e->proc, e->subproc);
          return NOT_OK;
        }
	      
        break;
	      
       case ULOG_SUBMIT:

        if (RecoveryMode) {
          // search Node Information List for next eligible
          // Node. This will get updated with the CondorID info
		  
          CondorJobs->Rewind();
		  
          while( (!FoundNode) && (job = CondorJobs->Next()) ) {
            JobStatus NodeStatus;
            job->GetNodeStatus(NodeStatus);
            job->GetNodeID(jobID);
		      
            if ( (job->IsEmpty("waiting")) && 
                 (NodeStatus == READY) ) {
              // To remove the DAG_LOG facility, 
              // uncomment the following

              /*
                // update Condor ID
                job->UpdateCondorInfo(e->cluster,
                e->proc,
                e->subproc);
				  
              */

              // To remove the DAG_LOG facility, 
              // comment the folowing up until the 
              // *** mark

              // write SET_CONDORID message to recovery log

              if (DagLog_fd < 0) {
                fprintf(stderr, "DagMan::ProcessLogEvent() : \
Failed to open DagMan Recovery Log %s\n", _dagLogName);
              } else {
                // Write Event SubmitJob to DagMan Recovery Log
#ifdef VERBOSE
                fprintf(stderr, "Logging SET_CONDORID message for Node %d\n",jobID);
#endif
                LogSetCondorID *log;
                log = new LogSetCondorID(e->cluster,
                                         e->proc,
                                         e->subproc,
                                         jobID);
			      
                int temp;
                temp = log->Write(DagLog_fd);
                log->Play((void *)job);
                fsync(DagLog_fd);
                delete log;
#ifdef DEBUG		      
                fprintf(stderr, "Wrote %d items\n", temp);
#endif
			    
              }
			  
              // ***

              job->SetNodeStatus(SUBMITTED);
              _nJobsWaiting--;
              _nJobsReady++;
              FoundNode = true;
            }
          }
		  
          if (!FoundNode) {
            fprintf(stderr,
                    "SUBMIT event for NOTREADY job: %d.%d.%d\n",
                    e->cluster, e->proc, e->subproc);
            return NOT_OK;
          }
        } else if (GetCondorJobID != 0) {
          status = GetNodePtr(GetCondorJobID, job);
          if (status != OK) {
            // Couldn't get Pointer to NodeInfo
            return status;
          }
		    
          status = job->UpdateCondorInfo(e->cluster, 
                                         e->proc, 
                                         e->subproc);

          // Stop checking the log

          // this method will break if the submit hasnt
          // been logged in the condor_log

          done = true;
        }
	    
        break;
	      
       case ULOG_EXECUTE:
	      
        condorID.cluster = e->cluster;
        condorID.proc = e->proc;
        condorID.subproc = e->subproc;
	      
        // search through Node Information List to find the
        // job's Node
	      
        CondorJobs->Rewind();
	      
        while ( (!FoundNode) && (job = CondorJobs->Next()) ) {
          job->GetCondorInfo(CondorID.cluster, 
                             CondorID.proc,
                             CondorID.subproc);
		  
          if (!CondorIDCompare(CondorID, condorID)) {
            FoundNode = TRUE;
            break;
          }
        } 
	      
        if (FoundNode) {
          job->SetNodeStatus(RUNNING);
		  
#ifdef DEBUG
          job->GetNodeID(jobID);
          printf("job %d (%d.%d.%d) is running\n",
                 jobID, e->cluster, e->proc, e->subproc);
#endif

          _nJobsRunning++;
          _nJobsWaiting--;
#ifdef DEBUG
          job->Dump();
#endif

        }
	      
        if (!FoundNode) {
          fprintf(stderr,
                  "Detecting Execution for unknown job: %d.%d.%d\n",
                  e->cluster, e->proc, e->subproc);
		  
          return NOT_OK;
        }
        break;
	      
      }
	  
      break;
	  
     case ULOG_NO_EVENT:
#if DEBUG
      printf ("ULOG_NO_EVENT\n");
#endif
      // printf("No more events in condor log\n");
      done = true;
      break;
	  
     case ULOG_RD_ERROR:
#if DEBUG
      printf ("ULOG_RD_ERROR\n");
#endif
     case ULOG_UNK_ERROR:
#if DEBUG
      printf ("ULOG_UNK_ERROR\n");
#endif
      if (_condorLog.synchronize() != 1) {
        fprintf(stderr,
                "WARNING: attempt to synchronize condor log failed\n");
      }
      break;

    }
  }
  
  return OK;
}

int DagMan::SubmitJob(NodeID id) {
  int status;
  int condorstatus;
  
  NodeInfo *job;
  char *cmdfile;
  char *jobname;
  int cluster, proc, subproc;
  
  char *condorError;
  
  status = GetNodePtr(id, job);
  if (status != OK) return status;
  
  status = job->GetJobName(jobname);
  if (status != OK) return status;
  
#ifdef VERBOSE    
  printf("Submitting job %s to condor\n", jobname);
  fflush(stdout);
#endif    
  
  status = job->GetCmdFile(cmdfile);
  
  if (status != OK) return status;

  // To remove the DAG_LOG facility, comment the 
  // following code until the
  // ***

  // write JOB SUBMIT message to recovery log

  if (DagLog_fd < 0) {
    fprintf(stderr, "DagMan::SubmitJob() : \
Failed to open DagMan Recovery Log %s\n", _dagLogName);
  } else {
    // Write Event SubmitJob to DagMan Recovery Log
#ifdef VERBOSE
    fprintf(stderr, "Logging CONDOR_SUBMIT message for Node %d\n", id);
#endif
    LogSubmitJob *log;
    log = new LogSubmitJob(id);
      
    int temp;
    temp = log->Write(DagLog_fd);
    fsync(DagLog_fd);
    delete log;
#ifdef DEBUG		      
    fprintf(stderr, "Wrote %d items\n", temp);
#endif
  }
  // ***
  
  condorstatus = CondorSubmit(cmdfile, condorError,
                              cluster, proc, subproc);
  
  if (condorstatus != 0) {
    // Error in submitting the condor job

    return NOT_OK;
  } else {

    // To remove the DAG_LOG facility, comment out the
    // following code up until the
    // ***

    // write JOB_SUBMIT_DONE message to recovery log

    if (DagLog_fd < 0) {
      fprintf(stderr, "DagMan::SubmitJob() : \
Failed to open DagMan Recovery Log %s\n", _dagLogName);
    } else {
      // Write Event SubmitJob to DagMan Recovery Log
#ifdef VERBOSE
      fprintf(stderr, "Logging SUBMIT_OK message for Node %d\n",id);
#endif
      LogSubmitJobOK *log;
      log = new LogSubmitJobOK(id);
	  
      int temp;
      temp = log->Write(DagLog_fd);
      fsync(DagLog_fd);
      delete log;
#ifdef DEBUG		      
      fprintf(stderr, "Wrote %d items\n", temp);
#endif
    }
    // ***

  }
  
  // we have successfully submitted Condor Job
  
  // old routine
  //    status = job->UpdateCondorInfo(cluster, proc, subproc);
  
  // [j] we should call a select on the Condor File before 
  // [j] doing this to check if there's something to read
  //sleep(DEFAULT_SLEEP);
  
  
  // check the condor log & retrieve the cluster, proc & subproc
  status = ProcessLogEvents(false, id);
  
  if (status != OK) {
    // Something wrong when reading the logfile

#ifdef DEBUG
    cout << "Something is wrong with retrieving the CondorID" << endl;
#endif
    return NOT_OK;
  }
  
  status = job->SetNodeStatus(SUBMITTED);
  
  if (status != OK)
    {
      // STATE not found in Node Attribute
      return NOT_OK;
    }
  
  _nJobsWaiting--;
  _nJobsReady++;
  
#ifdef DEBUG
  
  printf("Job %s has been assigned Condor ID %d.%d.%d\n", 
         jobname, cluster, proc, subproc);
  fflush(stdout);
  
  // print out NodeInformation
  cout << "Updated Node!!!" << endl;
  job->Dump();
#endif
  
  return OK;
}

int DagMan::ReportNodeTermination(NodeID parentID) {
  int status;
  
  NodeInfo *pNode, *childNode;
  NodeID Node;
  
  char *childState;
  char *oldState;

  //  JobStatus currentState;

  status = GetNodePtr(parentID, pNode);
  if (status != OK) return NOT_OK;       // Couldn't get Node Pointer
  
  pNode->outgoing.Rewind();
  
  while (pNode->outgoing.Next(Node)) {
    status = GetNodePtr(Node, childNode);
    if (status != OK) {
      // Remove Node Pointer
      return NOT_OK;
    }
      
    status = childNode->RemoveWaiting(parentID);
      
    // Check if we can run this Node

    JobStatus currentState;
    childNode->GetNodeStatus(currentState);
      
    if ( childNode->IsEmpty("waiting") && (currentState != DONE) ) {
      status = childNode->SetNodeStatus(READY);
      if (status != OK) {
        // couldn't update STATUS of NodeInfo
        return NOT_OK;
      }
    }
  }

  return OK;
}

int DagMan::TerminateJob(NodeID id) {
  NodeInfo *job;
  int status;
  
  JobStatus oldState;
  char *jobname;
  
  //
  // Get a pointer into the job list
  //
  status = GetNodePtr(id, job);
  if (status != OK) return NOT_OK;       // Couldn't get Node Pointer
  
  //
  // Make sure job is currently running
  //
  
  status = job->GetNodeStatus(oldState);
  
  if (status != OK) return NOT_OK;      // Couldn't retrieve NodeStatus
  
  if (oldState != RUNNING) {
    if (oldState != DONE) {
#ifdef DEBUG
      fprintf(stderr, "WARNING: TERM event for a non-running job: %d\n",
              id);
      fprintf(stderr, "  The event will be ignored\n");
#endif
      return OK;
    }
  }
  
  status = job->GetJobName(jobname);
  
  if (status != OK) {
    // Couldn't retrieve JobName
    return NOT_OK;
  }
  
#ifdef VERBOSE
  printf("Job %s has terminated\n", jobname);
  fflush(stdout);
#endif
    
  //
  // Set job state to Done
  //

  status = job->SetNodeStatus(DONE);
  if (status != OK) {
    // Couldn't set NodeStatus
    return NOT_OK;
  }

  _nJobsRunning--;
  _nJobsDone++;
    
  //
  // Report termination to all child jobs
  //
  status = ReportNodeTermination(id);

  if (status != OK) {
    // Error in function ReportNodeTermination()

    return NOT_OK;
  }

  return OK;
}

int DagMan::NumJobs() {  return _nJobs; }

int DagMan::NumJobsWaiting() {  return _nJobsWaiting; }

int DagMan::NumJobsReady() {   return _nJobsReady; }

int DagMan::NumJobsRunning() {   return _nJobsRunning; }

int DagMan::NumJobsCompleted() {   return _nJobsDone; }

void DagMan::Dump() {
  printf("DagMan summary\n");
  printf("\t%d jobs. %d Waiting, %d Submitted, %d running, %d done\n",
         _nJobs, _nJobsWaiting, _nJobsReady, _nJobsRunning, _nJobsDone);
}

//-------------------------------------------------------------------------
bool DagMan::DetectLogGrowth (int checkInterval) {
  if (!_condorLogInitialized) {
    _condorLog.initialize(_condorLogName);
    _condorLogInitialized = true;
  }

  int fd = _condorLog.getfd();


  // fprintf (stderr, "fd == %d\t", fd);
  struct stat buf;
  sleep (checkInterval);
  int error = fstat (fd, & buf);
  if (error == -1) {
    perror ("Dagman Condor Logfile");
    exit(1);
  }
  int oldSize = _condorLogSize;
  _condorLogSize = buf.st_size;
  // fprintf (stderr, "buf.st_size == %d\t", buf.st_size);
  return (buf.st_size > oldSize);
}

//-------------------------------------------------------------------------
bool DagMan::WaitForLogEvents() {
  fd_set fdset;
  int nReady;
  int DEFAULT_SLEEP = 2;
  int fd;
  
  if (!_condorLogInitialized) {
    _condorLog.initialize(_condorLogName);
    _condorLogInitialized = true;
  }
  
  //
  // Wait for new events to be written into the condor log
  //
  fd = _condorLog.getfd();
  while (fd < 0 || fd >= FD_SETSIZE) {
    //
    // Wake up periodically to see if fd becomes valid
    //
    fprintf(stderr, "WARNING: invalid condor log fd: %d\n", fd);
    fprintf(stderr,
            "Sleeping for %d seconds to see if fd becomes valid...",
            DEFAULT_SLEEP);
    fflush(stdout);
    sleep(DEFAULT_SLEEP);
    printf("done\n");
    fflush(stdout);
    fd = _condorLog.getfd();
  }

  do {
    FD_ZERO(&fdset);
    FD_SET(fd, &fdset);
    // fprintf(stderr, "Calling select...");
    fflush(stderr);

    // this is not doing anything but returning 1 ...

    // fprintf (stderr, "nReady = "); fflush(stderr);  // COLBY
    // nReady = select(fd + 1, &fdset, NULL, NULL, NULL);
    // fprintf (stderr, "%d\n", nReady); fflush(stderr);  // COLBY

    sleep(2);
    fflush(stderr);
  } while (nReady != 1);
  
  return true;
  
}

bool DagMan::NodeLookUp(char *JobName, NodeID &JobID) {
  NodeInfo *CurrentJob;
  int status;
  
  CondorJobs->Rewind();
  
  while ( (CurrentJob = CondorJobs->Next()) ) {
    char *CurrentJobName;
      
    status = CurrentJob->GetJobName(CurrentJobName);
    if (status != OK) return FALSE;
      
    if (!strcmp(CurrentJobName, JobName)) {
      status = CurrentJob->GetNodeID(JobID);
      if (status != OK) {
        return FALSE;
      }
      return TRUE;
    }
  }

  return FALSE;
}

void DagMan::PrintNodeList() {
  NodeInfo *CurrentJob;

  printf("Printing the list of Jobs and Job Attributes ... \n");

  if (CondorJobs->IsEmpty()) {
    printf("\tThe List is Empty\n");
  } else {
    CondorJobs->Rewind();

    while ( (CurrentJob = CondorJobs->Next()) ) {
      CurrentJob->Dump();
    }
  }

  printf("Exiting NodeList::PrintNodeList()\n");

}

void DagMan::CheckForFinishedJobs() {
  NodeInfo *CheckingThisJob;
  JobStatus status;
  NodeID CurrentJobID;

#ifdef DEBUG
  fprintf(stderr, "Checking for jobs which are already done ... \n");
#endif

  CondorJobs->Rewind();

  int current_index = 0, last_index = 0;

  while ( (CheckingThisJob = CondorJobs->Next()) ) {
    current_index++;

    CheckingThisJob->GetNodeStatus(status);

#ifdef DEBUG
    CheckingThisJob->Dump();
#endif

    // This is ugly code. The reason for current_index and last_index
    // is that the LIST template uses a single private variable for the
    // current position. However, when we call TerminateJob, the 
    // function manipulates the CondorJobs list, and throws the
    // current position pointer out of whack

    if ( (status == DONE) && (current_index > last_index) ) {
      last_index = current_index;
      CheckingThisJob->GetNodeID(CurrentJobID);
	  
      _nJobsRunning++;
      CheckingThisJob->SetNodeStatus(RUNNING);
      TerminateJob(CurrentJobID);
      CondorJobs->Rewind();
      current_index = 0;
    }
  }
}


void  DagMan::Recover_DagMan() {
  ProcessLogEvents(true);

#ifdef DEBUG
  PrintNodeList();
  Dump();  
#endif

}

#endif 
