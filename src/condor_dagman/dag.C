//
// Local DAGMan includes
//
#include "dag.h"
#include "debug.h"
#include "submit.h"

#include "simplelist.h"     /* should be replaced by STL library */
#include "condor_string.h"  /* for strnewp() */

//---------------------------------------------------------------------------
void TQI::Print () const {
    printf ("Job: ");
    job_print(parent);
    printf (", Children: ");
    cout << children;
}

//---------------------------------------------------------------------------
Dag::Dag(const char *condorLogName, const char *lockFileName,
         const int numJobsRunningMax) :
    _condorLogInitialized (false),
    _condorLogSize        (0),
    _lockFileName         (NULL),
    _termQLock            (false),
    _numJobsDone          (0),
    _numJobsRunning       (0),
    _numJobsRunningMax    (numJobsRunningMax)
{
    _condorLogName = strnewp (condorLogName);
    _lockFileName  = strnewp (lockFileName);
}

//-------------------------------------------------------------------------
Dag::~Dag() {
    unlink(_lockFileName);  // remove the file being used as semaphore
    delete [] _condorLogName;
    delete [] _lockFileName;
}

//-------------------------------------------------------------------------
bool Dag::Bootstrap (bool recovery) {
    //--------------------------------------------------
    // Add a virtual dependency for jobs that have no
    // parent.  In other words, pretend that there is
    // an invisible God job that has the orphan jobs
    // as its children.  (Ignor the phyllisophical ramifications).
    // The God job will be the value (Job *) NULL, which is
    // fitting, since the existance of god is an unsolvable concept
    //--------------------------------------------------

    assert (!_termQLock);
    _termQLock = true;

    {
        TQI * god = new TQI;   // Null parent and empty children list
        ListIterator<Job> iList (_jobs);
        Job * job;
        while (iList.Next(job)) {
            if (job->IsEmpty(Job::Q_INCOMING))
                god->children.Append(job->GetJobID());
        }
        _termQ.Append (god);
    }

    _termQLock = false;
    
    //--------------------------------------------------
    // Update dependencies for pre-terminated jobs
    // (jobs marks DONE in the dag input file)
    //--------------------------------------------------
    Job * job;
    ListIterator<Job> iList (_jobs);
    while ((job = iList.Next()) != NULL) {
        if (job->_Status == Job::STATUS_DONE) TerminateJob(job);
    }
    
    debug_println (DEBUG_VERBOSE, "Number of Pre-completed Jobs: %d",
                   NumJobsDone());
    
    if (recovery) {
        debug_println (DEBUG_NORMAL, "Running in RECOVERY mode...");
        if (!ProcessLogEvents (recovery)) return false;
    }
    return SubmitReadyJobs();
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
Job * Dag::GetJob (const JobID_t jobID) const {
    ListIterator<Job> iList (_jobs);
    Job * job;
    iList.ToBeforeFirst();
    while ((job = iList.Next()) != NULL) {
        if (job->GetJobID() == jobID) return job;
    }
    return NULL;
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
    assert (fd != 0);
    struct stat buf;
    sleep (checkInterval);
    
    if (fstat (fd, & buf) == -1) debug_perror (2, DEBUG_QUIET, _condorLogName);
    
    int oldSize = _condorLogSize;
    _condorLogSize = buf.st_size;
    
    bool growth = (buf.st_size > oldSize);
    debug_printf (DEBUG_DEBUG_4, "%s\n", growth ? "GREW!" : "No growth");
    return growth;
}

//-------------------------------------------------------------------------
bool Dag::ProcessLogEvents (bool recovery) {
    
    if (!_condorLogInitialized) {
        _condorLogInitialized = _condorLog.initialize(_condorLogName);
    }
    
    bool done = false;  // Keep scaning until ULOG_NO_EVENT
    bool result = true;
    static unsigned int log_unk_count;

    while (!done) {
        
        ULogEvent * e;  // refer to condor_event.h
        ULogEventOutcome outcome = _condorLog.readEvent(e);
        
        CondorID condorID;
        if (e != NULL) condorID = CondorID (e->cluster, e->proc, e->subproc);
        
        debug_printf (DEBUG_VERBOSE, " Log outcome: %s",
                      ULogEventOutcomeNames[outcome]);
        
        if (outcome != ULOG_UNK_ERROR) log_unk_count = 0;

        switch (outcome) {
            
            //----------------------------------------------------------------
          case ULOG_NO_EVENT:      
            debug_printf (DEBUG_VERBOSE, "\n");
            done = true;
            break;
            //----------------------------------------------------------------
          case ULOG_RD_ERROR:
            debug_printf (DEBUG_QUIET, "  ERROR: failure to read log\n");
            done   = true;
            result = false;
            break;
            //----------------------------------------------------------------
          case ULOG_UNK_ERROR:
            log_unk_count++;
            if (recovery || log_unk_count >= 5) {
                debug_printf (DEBUG_QUIET, "  ERROR: Unknown log error");
                result = false;
            }
            debug_printf (DEBUG_VERBOSE, "\n");
            done   = true;
            break;
            //----------------------------------------------------------------
          case ULOG_OK:
            
            if (DEBUG_LEVEL(DEBUG_VERBOSE)) {
                putchar (' ');
                condorID.Print();
                putchar (' ');
            }
            
            if (DEBUG_LEVEL(DEBUG_VERBOSE)) {
                printf ("  Event: %s ", ULogEventNumberNames[e->eventNumber]);
                printf ("for Job ");
            }
            
            switch(e->eventNumber) {
                
                //--------------------------------------------------
              case ULOG_EXECUTABLE_ERROR:
              case ULOG_JOB_ABORTED:
              {
                  Job * job = GetJob (condorID);
                  
                  if (DEBUG_LEVEL(DEBUG_VERBOSE)) job_print (job,true);
                  
                  // If this is one of our jobs, then we must inform the user
                  // that UNDO is not yet handled
                  if (job != NULL) {
                      if (DEBUG_LEVEL(DEBUG_QUIET)) {
                          printf ("\n------------------------------------\n");
                          job->Print(true);
                          printf (" resulted in %s.\n"
                                  "This version of Dagman does not support "
                                  "job resubmition, so this DAG must be "
                                  "aborted.\n"
                                  "Version 2 of Dagman will support job UNDO "
                                  "so that an erroneous job can be "
                                  "resubmitted\n"
                                  "while the dag is still running.\n",
                                  ULogEventNumberNames[e->eventNumber]);
                      }
                      done   = true;
                      result = false;
                  }
              }
              break;
              
              case ULOG_CHECKPOINTED:
              case ULOG_JOB_EVICTED:
              case ULOG_IMAGE_SIZE:
              case ULOG_SHADOW_EXCEPTION:
              case ULOG_GENERIC:
              case ULOG_EXECUTE:
                if (DEBUG_LEVEL(DEBUG_VERBOSE)) {
                    Job * job = GetJob (condorID);
                    job_print(job,true);
                    putchar ('\n');
                }
                break;
                
                //--------------------------------------------------
              case ULOG_JOB_TERMINATED:
              {
                  Job * job = GetJob (condorID);
                  
                  if (DEBUG_LEVEL(DEBUG_VERBOSE)) {
                    job_print(job, true);
                    putchar ('\n');
                  }
                  
                  if (job == NULL) {
                      done   = true;
                      result = false;
                  } else {
                      job->_Status = Job::STATUS_DONE;
                      TerminateJob(job);
                      if (DEBUG_LEVEL(DEBUG_DEBUG_1)) Print_TermQ();
                      if (!recovery) {
                          debug_printf (DEBUG_VERBOSE, "\n");
                          if (SubmitReadyJobs() == false) {
                              done   = true;
                              result = false;
                          }
                      }
                  }
              }
              break;
              
              //--------------------------------------------------
              case ULOG_SUBMIT:
                
                // search Job List for next eligible
                // Job. This will get updated with the CondorID info
                Job * job = GetSubmittedJob(recovery);
                
                if (job == NULL) {
                    done = true;
                    result = false;
                } else job->_CondorID = condorID;
                
                if (DEBUG_LEVEL(DEBUG_VERBOSE)) {
                  job_print (job, true);
                  putchar ('\n');
                }

                if (DEBUG_LEVEL(DEBUG_DEBUG_1)) Print_TermQ();
                break;
            }
            break;
        }
    }
    if (DEBUG_LEVEL(DEBUG_VERBOSE) && recovery) {
        printf ("    -----------------------\n");
        printf ("       Recovery Complete\n");
        printf ("    -----------------------\n");
    }
    return result;
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
Job * Dag::GetJob (const CondorID condorID) const {
    ListIterator<Job> iList (_jobs);
    Job * job;
    while ((job = iList.Next())) if (job->_CondorID == condorID) return job;
    return NULL;
}

//-------------------------------------------------------------------------
//bool Dag::Submit (JobID_t jobID) {
//    Job * job = GetJob (jobID);
//    assert (job != NULL);
//    return Submit(job);
//}

//-------------------------------------------------------------------------
bool Dag::Submit (Job * job) {
    assert (job != NULL);

    if (DEBUG_LEVEL(DEBUG_VERBOSE)) {
        printf ("Submitting JOB ");
        job->Print();
    }
  
    CondorID condorID(0,0,0);
    submit_submit (job->GetCmdFile(), condorID);
    job->_Status = Job::STATUS_SUBMITTED;
    _numJobsRunning++;
    assert (_numJobsRunningMax >= 0 || _numJobsRunning <= _numJobsRunningMax);

    if (DEBUG_LEVEL(DEBUG_VERBOSE)) {
        printf (", ");
        condorID.Print();
        putchar('\n');
    }

    return true;
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
void Dag::Print_TermQ () const {
    printf ("Termination Queue:");
    if (_termQ.IsEmpty()) {
        printf (" <empty>\n");
        return;
    } else putchar('\n');

    ListIterator<TQI> q_iter (_termQ);
    TQI * tqi;
    while (q_iter.Next(tqi)) {
        printf ("  ");
        tqi->Print();
        putchar ('\n');
    }
}


//---------------------------------------------------------------------------
void Dag::run_popen (char * cmd) const {
    debug_println (DEBUG_VERBOSE, "Running: %s", cmd);
    FILE *fp = popen (cmd, "r");
    int r;
    if (fp == NULL || (r = pclose(fp)) != 0) {
        if (DEBUG_LEVEL(DEBUG_NORMAL)) {
            printf ("WARNING: failure: %s", cmd);
            if (fp != NULL) printf ("returned %d", r);
            putchar('\n');
        }
    }
}

//---------------------------------------------------------------------------
void Dag::RemoveRunningJobs () const {
    char cmd     [ARG_MAX];

    unsigned int len  = 0;  // Number of character in cmd so far
    unsigned int jobs = 0;  // Number of jobs appended to cmd so far

    ListIterator<Job> iList(_jobs);
    Job * job;
    while (iList.Next(job)) {

        if (jobs == 0) {
            len = 0;
            len += sprintf (cmd, "condor_rm");
        }

        if (job->_Status == Job::STATUS_SUBMITTED) {
            // Should be snprintf(), but doesn't exist on all platforms
            len += sprintf (&cmd[len], " %d", job->_CondorID._cluster);
            jobs++;
        }

        if (jobs > 0 && len >= ARG_MAX - 10) {
            run_popen (cmd);
            jobs = 0;
        }
    }

    if (jobs > 0) {
        run_popen (cmd);
        jobs = 0;
    }
}

//===========================================================================
// Private Meathods
//===========================================================================

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
    _numJobsRunning--;
    assert (_numJobsRunning >= 0);
    assert (_numJobsDone <= _jobs.Number());

    //
    // Add job and its dependants to the termination queue
    //
    TQI * tqi = new TQI (job, qp);
    assert (!_termQLock);
    if (!job->IsEmpty(Job::Q_OUTGOING)) _termQ.Append(tqi);
}

//---------------------------------------------------------------------------
Job * Dag::GetSubmittedJob (bool recovery) {

    assert (!_termQLock);
    _termQLock = true;

    _termQ.Rewind();

    Job * job_found = NULL;

    // The following loop scans the entire termination queue (_termQ)
    // It has two purposes.  First it looks for a submittable child
    // of the first terminated job.  If the first terminated job has
    // no such child, then the loop ends, and this function will return
    // NULL (no job found).
    //
    // If such a child job is found, it is removed from its parent's list
    // of unsubmitted jobs.  If that causes the parent's child list to
    // become empty, then the parent itself is removed from the termination
    // queue.
    //
    // The rest of the termination queue is scanned for duplicates of the
    // earlier found child job.  Those duplicates are removed exactly the
    // same way the original child job was removed.

    bool found = false;  // Flag signally the discovery of a submitted child
    TQI * tqi;           // The current termination queue item being scanned
    while (_termQ.Next(tqi)) {
        assert (!tqi->children.IsEmpty());
        tqi->children.Rewind();
        JobID_t match_ID;  // The current Job ID being examined
        JobID_t found_ID;  // The ID of the original child found

        bool found_on_this_line = false;  // for debugging

        while (tqi->children.Next(match_ID)) {
            bool kill = false;  // Flag whether this child should be removed
            if (found) {
                if (match_ID == found_ID) {
                    assert (!found_on_this_line);
                    found_on_this_line = true;
                    kill = true;
                }
            } else {
                Job * job = GetJob(match_ID);
                assert (job != NULL);
                if (job->_Status == (recovery ?
                                     Job::STATUS_READY :
                                     Job::STATUS_SUBMITTED)) {
                    found_ID           = match_ID;
                    found              = true;
                    kill               = true;
                    job_found          = job;
                    found_on_this_line = true;
                }
            }

            if (kill) {
                tqi->children.DeleteCurrent();
                if (tqi->children.IsEmpty()) _termQ.DeleteCurrent();
                //break; // There shouldn't be duplicate children for this job
            }
        }

        // Note that found must become true during the first terminated job
        // for the scan of the rest of the termination queue (_termQ)
        // to continue
        if (!found) break;
    }
    _termQLock = false;
    if (recovery && job_found != NULL) {
        job_found->_Status = Job::STATUS_SUBMITTED;
    }
    return job_found;
}

//-------------------------------------------------------------------------
bool Dag::SubmitReadyJobs () {
    if (_termQ.IsEmpty()) return true;
    assert (!_termQLock);
    _termQLock = true;

    _termQ.Rewind();
    TQI * tqi = _termQ.Next();
    assert (tqi != NULL);
    assert (!tqi->children.IsEmpty());
    tqi->children.Rewind();
    JobID_t jobID;
    while (tqi->children.Next(jobID) &&
           (_numJobsRunningMax == 0 || _numJobsRunning < _numJobsRunningMax)) {
        Job * job = GetJob(jobID);
        assert (job != NULL);
        if (job->CanSubmit()) {
            if (!Submit (job)) {
                if (DEBUG_LEVEL(DEBUG_QUIET)) {
                    printf ("Failed to submit job ");
                    job->Print(true);
                    putchar('\n');
                }
                return false;
            }
        }
    }
    _termQLock = false;
    return true;
}
