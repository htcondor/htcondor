#include "dag.h"
#include "debug.h"
#include "simplelist.h"
#include "submit.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>

//---------------------------------------------------------------------------
Dag::Dag() :
  _condorLogName(NULL),
  _condorLogInitialized(false),
  _condorLogSize(0),
  _lockFileName(NULL),
  _numJobsDone(0) {}

//-------------------------------------------------------------------------
bool Dag::Init (const char *condorLogName, const char *lockFileName) {
  if (_condorLogName != NULL) delete [] _condorLogName;
  _condorLogName = new char [strlen(condorLogName) + 1];
  if (_condorLogName == NULL) return false;
  strcpy(_condorLogName, condorLogName);
 
  if (_lockFileName != NULL) delete [] _lockFileName;
  _lockFileName = new char [strlen(lockFileName) + 1];
  if (_lockFileName == NULL) return false;
  strcpy(_lockFileName, lockFileName);

  return true;
}

//-------------------------------------------------------------------------
Dag::~Dag() {
  unlink(_lockFileName);  // remove the file being used as semaphore
  delete [] _condorLogName;
}

//-------------------------------------------------------------------------
Job * Dag::GetJob (const JobID_t jobID) const {
  ListIterator<Job> iList (_jobs);
  Job * job;
  iList.ToBeforeFirst();
  while ((job = iList.Next()) != NULL) {
    if (job->GetJobID() == jobID) return job;
  }
  return NULL;
}

//---------------------------------------------------------------------------
Job * Dag::GetJob (const char * jobName) const {
  ListIterator<Job> iList (_jobs);
  Job * job;
  while ((job = iList.Next())) {
    if (strcmp(job->GetJobName(), jobName) == 0) return job;
  }
  return NULL;
}

//---------------------------------------------------------------------------
Job * Dag::GetJob (const CondorID_t condorID) const {
  ListIterator<Job> iList (_jobs);
  Job * job;
  while ((job = iList.Next())) if (job->_CondorID == condorID) return job;
  return NULL;
}

//-------------------------------------------------------------------------
bool Dag::AddDependency (Job * parent, Job * child) {
  assert (parent != NULL);
  assert (child  != NULL);

  if (!parent->Add (Job::Q_OUTGOING,  child->GetJobID())) return false;
  if (!child->Add  (Job::Q_INCOMING, parent->GetJobID())) return false;
  if (!child->Add  (Job::Q_WAITING,  parent->GetJobID())) return false;
  return true;
}

//-------------------------------------------------------------------------
bool Dag::ProcessLogEvents(bool recover, Job * getJob) {

  assert (!recover || getJob == NULL);

  if (DEBUG_LEVEL(DEBUG_DEBUG_2)) {
    printf ("%s: recover=%s, getJob: ", __FUNCTION__,
            recover ? "true" : "false");
    if (getJob == NULL) printf ("NULL");
    else getJob->Print();
    putchar ('\n');
  }

  if (!_condorLogInitialized) {
    _condorLog.initialize(_condorLogName);
    _condorLogInitialized = true;
  }
  
  bool done = false;  // Keep scaning until ULOG_NO_EVENT
  bool result = true;

  while (!done) {
    
    ULogEvent * e;  // refer to condor_event.h
    ULogEventOutcome outcome = _condorLog.readEvent(e);
    Job * job = NULL;

    CondorID_t condorID;
    if (e != NULL) {
      condorID.Set (e->cluster, e->proc, e->subproc);
      job = GetJob (condorID);
    }

    abort();
    
    debug_printf (DEBUG_VERBOSE, "    Log outcome: ");

    switch (outcome) {
      
     case ULOG_OK:
      
      if (DEBUG_LEVEL(DEBUG_VERBOSE)) {
        printf ("ULOG_OK");
        condorID.Print();
        putchar (' ');
      }
	  
      debug_printf (DEBUG_VERBOSE, "  Event: %s",
                    ULogEventNumberNames[e->eventNumber]);

      switch(e->eventNumber) {
       case ULOG_EXECUTABLE_ERROR:
        done = true;
        result = false;
        break;

       case ULOG_CHECKPOINTED:
       case ULOG_JOB_EVICTED:
       case ULOG_IMAGE_SIZE:
       case ULOG_SHADOW_EXCEPTION:
        break;
        
        //--------------------------------------------------
       case ULOG_JOB_TERMINATED:
        debug_printf (DEBUG_VERBOSE, "for Job ");
          if (job == NULL) {
            if (DEBUG_LEVEL(DEBUG_VERBOSE)) {
              printf ("(UNKNOWN) ");
              condorID.Print();
            }
            done = true;
            result = false;  // should we could going?
          } else {
            if (DEBUG_LEVEL(DEBUG_VERBOSE)) job->Print(true);
            job->_Status = Job::STATUS_DONE;
            TerminateJob(job);
          }
        break;
        
        //--------------------------------------------------
       case ULOG_SUBMIT:
        
        debug_printf (DEBUG_VERBOSE, "SUBMIT         for Job ");

        if (recover) {
          // search Job List for next eligible
          // Job. This will get updated with the CondorID info
		  
          Job * job = GetReadyJob();
          
          if (job == NULL) {
            if (DEBUG_LEVEL(DEBUG_VERBOSE)) {
              printf ("(UNKNOWN) ");
              condorID.Print();
            }
            done = true;
            result = false;
          } else {
            job->_CondorID = condorID;
            job->_Status   = Job::STATUS_SUBMITTED;
            if (DEBUG_LEVEL(DEBUG_VERBOSE)) job->Print(true);
          }
        } else if (getJob != NULL) {
          getJob->_CondorID = condorID;
          if (DEBUG_LEVEL(DEBUG_VERBOSE)) getJob->Print(true);
        } else {
          if (DEBUG_LEVEL(DEBUG_VERBOSE)) {
            printf ("(UNEXPECTED) ");
            Job * job = GetJob (condorID);
            if (job == NULL) {
              printf ("(UNKNOWN) ");
              condorID.Print();
            } else job->Print();
          }
          done = true;
          result = false;
        }
        break;
        
       //--------------------------------------------------
       case ULOG_EXECUTE:
        debug_printf (DEBUG_VERBOSE, "EXECUTE        for Job ");
        {
          Job * job = GetJob (condorID);
          if (job == NULL) {
            if (DEBUG_LEVEL(DEBUG_VERBOSE)) {
              printf ("(UNKNOWN) ");
              condorID.Print();
            }
            done = true;
            result = false;
          } else {
            if (DEBUG_LEVEL(DEBUG_VERBOSE)) job->Print(true);
            job->_Status = Job::STATUS_RUNNING;
          }
        }
        break;
        
        //--------------------------------------------------
       case ULOG_GENERIC:
        debug_printf (DEBUG_VERBOSE, "ULOG_GENERIC not handled: "
                      "%s(%d)", __FILE__, __LINE__);
        break;

        //--------------------------------------------------
       case ULOG_JOB_ABORTED:
        debug_printf (DEBUG_VERBOSE, "ULOG_JOB_ABORTED not handled: "
                      "%s(%d)", __FILE__, __LINE__);
        break;
      }
      break;
	  
      //--------------------------------------------------
     case ULOG_NO_EVENT:
      debug_printf (DEBUG_VERBOSE, "ULOG_NO_EVENT");
      done = true;
      break;
	  
       //--------------------------------------------------
     case ULOG_RD_ERROR:
      debug_printf (DEBUG_VERBOSE, "ULOG_RD_ERROR");
      break;

       //--------------------------------------------------
     case ULOG_UNK_ERROR:
      debug_printf (DEBUG_VERBOSE, "ULOG_UNK_ERROR");

      if (_condorLog.synchronize() != 1) {
        debug_println (DEBUG_NORMAL,
                       "WARNING: attempt to synchronize condor log failed");
      }
      break;
    }
    putchar('\n');
  }
  if (DEBUG_LEVEL(DEBUG_VERBOSE) && recover) {
    printf ("    -----------------------\n");
    printf ("       Recovery Complete\n");
    printf ("    -----------------------\n");
  }
  return result;
}
  
//-------------------------------------------------------------------------
int Dag::SubmitReadyJobs () {
  int submitted = 0;
  ListIterator<Job> iList (_jobs);
  Job * job;
  while ((job = iList.Next())) {
    if (job->CanSubmit()) {
      Submit (job);
      submitted++;
    }
  }
  return submitted;
}

//-------------------------------------------------------------------------
bool Dag::Submit (JobID_t jobID) {
  Job * job = GetJob (jobID);
  if (job == NULL) return false;
  return Submit (job);
}

//-------------------------------------------------------------------------
bool Dag::Submit (Job * job) {
  assert (job != NULL);

  if (DEBUG_LEVEL(DEBUG_VERBOSE)) {
    printf ("Submitting JOB ");
    job->Print();
    putchar ('\n');
  }
  
  submit_submit (job->GetCmdFile());
  
  // Condor Job has been submitted.  condor_submit logs the submit event
  // in the Condor log before exiting, so the event containing the job's
  // condor ID should be there.
  
  if (!ProcessLogEvents(false, job)) {
    debug_println (DEBUG_DEBUG_1, "Condor ID for job %d could not "
                   "be found in condor log", job->GetJobID());
    return false;
  } else {
    job->_Status = Job::STATUS_SUBMITTED;
    return true;
  }
}

//-------------------------------------------------------------------------
bool Dag::DetectLogGrowth (int checkInterval) {
  
  debug_printf (DEBUG_DEBUG_4, "%s: checkInterval=%d -- ", __FUNCTION__,
                checkInterval);

  if (!_condorLogInitialized) {
    _condorLog.initialize(_condorLogName);
    _condorLogInitialized = true;
  }

  int fd = _condorLog.getfd();
  struct stat buf;
  sleep (checkInterval);

  if (fstat (fd, & buf) == -1) debug_perror (1, DEBUG_QUIET, _condorLogName);

  int oldSize = _condorLogSize;
  _condorLogSize = buf.st_size;

  bool growth = (buf.st_size > oldSize);
  debug_printf (DEBUG_DEBUG_4, "%s\n", growth ? "GREW!" : "No growth");
  return growth;
}

//---------------------------------------------------------------------------
void Dag::PrintJobList() const {
  Job * job;
  printf ("Dag Job List:\n");

  ListIterator<Job> iList (_jobs);
  while ((job = iList.Next()) != NULL) {
    printf ("---------------------------------------");
    job->Dump();
    putchar ('\n');
  }
  printf ("---------------------------------------");
  printf ("\t<END>\n");
}

//---------------------------------------------------------------------------
void Dag::TerminateFinishedJobs() {
  Job * job;
  ListIterator<Job> iList (_jobs);
  while ((job = iList.Next()) != NULL) {
    if (job->_Status == Job::STATUS_DONE) TerminateJob(job);
  }
}

//-------------------------------------------------------------------------
void Dag::TerminateJob (Job * job) {
  assert (job != NULL);
  assert (job->_Status == Job::STATUS_DONE);

  //
  // Report termination to all child jobs by removing parent's ID from
  // each child's waiting queue.
  //
  SimpleList<JobID_t> & qp = job->GetQueueRef(Job::Q_OUTGOING);
  SimpleListIterator<JobID_t> iList (qp);
  JobID_t childID;
  while (iList.Next(childID)) {
    Job * child = GetJob(childID);
    assert (child != NULL);
    child->Remove(Job::Q_WAITING, job->GetJobID());
  }
  _numJobsDone++;
  assert (_numJobsDone <= _jobs.Number());
}

//---------------------------------------------------------------------------
Job * Dag::GetReadyJob () const {
  ListIterator<Job> iList (_jobs);
  Job * job;
  while ((job = iList.Next())) if (job->CanSubmit()) return job;
  return NULL;
}
