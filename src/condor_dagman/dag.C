//
// Local DAGMan includes
//
#include "condor_common.h"
#include "dag.h"
#include "debug.h"
#include "submit.h"
#include "util.h"

#include "simplelist.h"     /* should be replaced by STL library */
#include "condor_string.h"  /* for strnewp() */
#include "../condor_daemon_core.V6/condor_daemon_core.h"

//---------------------------------------------------------------------------
void TQI::Print () const {
    printf ("Job: ");
    job_print(parent);
    printf (", Children: ");
    children.Display(cout);
}

//---------------------------------------------------------------------------
Dag::Dag(const char *condorLogName, const char *lockFileName,
         const int numJobsRunningMax) :
    _condorLogInitialized (false),
    _condorLogSize        (0),
    _lockFileName         (NULL),
    _termQLock            (false),
    _numJobsDone          (0),
    _numJobsFailed        (0),
    _numJobsRunning       (0),
    _numJobsRunningMax    (numJobsRunningMax)
{
    _condorLogName = strnewp (condorLogName);
    _lockFileName  = strnewp (lockFileName);

	// register reaper functions for PRE & POST script completion
	preScriptReaperId =
		daemonCore->Register_Reaper( "PRE Script Reaper",
									 (ReaperHandlercpp)&Dag::PreScriptReaper,
									 "Dag::PreScriptReaper", this );
	postScriptReaperId =
		daemonCore->Register_Reaper( "POST Script Reaper",
									 (ReaperHandlercpp)&Dag::PostScriptReaper,
									 "Dag::PostScriptReaper", this );

	// initialize hash table for mapping PRE/POST script pids to Job*
	jobScriptPidTable = new HashTable<int,Job*>( 499, &hashFuncInt );
}

//-------------------------------------------------------------------------
Dag::~Dag() {
    unlink(_lockFileName);  // remove the file being used as semaphore
    delete [] _condorLogName;
    delete [] _lockFileName;
	delete jobScriptPidTable;
	// should we un-register reapers here?
}

//-------------------------------------------------------------------------
bool Dag::Bootstrap (bool recovery) {
    // Add a virtual dependency for jobs that have no parent.  In
    // other words, pretend that there is an invisible "God" job that
    // has the orphan jobs as its children.  The God job will have the
    // value (Job *) NULL.

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
    // (jobs marked DONE in the dag input file)
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
// Developer's Note: returning false tells main_timer to abort the DAG
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

		debug_printf (DEBUG_VERBOSE,
					  "\t_numJobsRunning = %d, _numJobsFailed = %d\n",
					  _numJobsRunning, _numJobsFailed );

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
                printf( "\n  Event: %s for Job ",
						ULogEventNumberNames[e->eventNumber] );
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
                                  "aborted.\n",
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
				  int pid;
                  Job * job = GetJob (condorID);
                  
                  if (DEBUG_LEVEL(DEBUG_VERBOSE)) {
                    job_print(job, true);
                    putchar ('\n');
                  }
                  
                  if (job == NULL) {
                      debug_println (DEBUG_QUIET,
                                     "Unknown terminated job found in log");
                      done   = true;
                      result = false;
                      break;
                  }

                  JobTerminatedEvent * termEvent = (JobTerminatedEvent*) e;

                  if (! (termEvent->normal &&
                                termEvent->returnValue == 0)) {
                      job->_Status = Job::STATUS_ERROR;
                      if (DEBUG_LEVEL(DEBUG_QUIET)) {
                          printf ("Job ");
                          job_print(job,true);
                          printf (" terminated with ");
                          if (termEvent->normal) {
                              printf ("status %d.", termEvent->returnValue);
							  job->retval = termEvent->returnValue;
                          } else {
                              printf ("signal %d.", termEvent->signalNumber);
							  job->retval = (0 - termEvent->signalNumber);
                          }
                          putchar ('\n');
                      }
					  _numJobsFailed++;
					  if( run_post_on_failure == FALSE ) {
						  break;
					  }
                  }
				  else {
					  job->retval = 0;
				  }

                  // if a POST script is specified for the job, run it
                  if (job->_scriptPost != NULL) {
					  if( DEBUG_LEVEL( DEBUG_VERBOSE ) ) {
						  printf( "Running POST Script of Job " );
						  job->Print();
						  printf( "\n" );
					  }
                      job->_scriptPost->_retValJob = termEvent->normal
                          ? termEvent->returnValue : -1;
					  job->_Status = Job::STATUS_POSTRUN;
					  pid =
						  job->_scriptPost->BackgroundRun(postScriptReaperId);
					  assert( pid > 0 );
					  job->_scriptPid = pid;
					  jobScriptPidTable->insert( pid, job );
					  break;
				  }

				  // no POST script was specified, so update DAG
				  // dependencies given our successful completion

				  job->_Status = Job::STATUS_DONE;
				  TerminateJob( job );

				  if( !recovery ) {
					  debug_printf( DEBUG_VERBOSE, "\n" );
					  // submit any waiting child jobs
					  if( SubmitReadyJobs() == false ) {
						  // if submit fails for any reason, abort
						  done   = true;
						  result = false;
					  }
				  }

				  if( DEBUG_LEVEL( DEBUG_DEBUG_1 ) )
					  Print_TermQ();
			  }
			  break;
              
              //--------------------------------------------------
              case ULOG_SUBMIT:
                
                // search Job List for next eligible
                // Job. This will get updated with the CondorID info
                Job * job = GetSubmittedJob(recovery);
                
                if (job == NULL) {
                    debug_println (DEBUG_QUIET,
                                   "Unknown submitted job found in log");
                    done = true;
                    result = false;
                    break;
                } else {
                    job->_CondorID = condorID;
                    if (DEBUG_LEVEL(DEBUG_VERBOSE)) {
                        job_print (job, true);
                        putchar ('\n');
                    }
                    if (!recovery) {
                        debug_printf (DEBUG_VERBOSE, "\n");

						// why do we submit more jobs here? i.e., why
						// would having just seen a submit event
						// prompt us to submit more jobs?  does only
						// one job get submitted at a time elsewhere,
						// making this part of an attempt at
						// throttling job submission or something? i'm
						// confused...  --pfc (2000-11-30)

                        if (SubmitReadyJobs() == false) {
                            done   = true;
                            result = false;
                        }
                    }
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
bool Dag::Submit (Job * job) {
    assert (job != NULL);

	_numJobsRunning++;
	assert( _numJobsRunning >= 0 );

	// if a PRE script exists, spawn it, and set up a reaper to submit
	// the job to Condor if/when the PRE script exits successfully
    if (job->_scriptPre != NULL) {
		if( DEBUG_LEVEL( DEBUG_VERBOSE ) ) {
			printf( "Running PRE Script of Job " );
			job->Print();
			printf( "\n" );
		}
		job->_Status = Job::STATUS_PRERUN;
		int pid = job->_scriptPre->BackgroundRun( preScriptReaperId );
		assert( pid > 0 );
		job->_scriptPid = pid;
		jobScriptPidTable->insert( pid, job );
		return true;
    }
	// no PRE script exists, so submit the job to condor right away
	return SubmitCondor( job );
}

//-------------------------------------------------------------------------
bool
Dag::SubmitCondor( Job* job )
{
    assert( job != NULL );
    assert( job->_Status == Job::STATUS_READY );

	if( ! job->CanSubmit() ) {
		// when would this ever happen?  --pfc (2000-12-01)
		if( DEBUG_LEVEL( DEBUG_VERBOSE ) ) {
			printf( "Warning: trying to submit unready job " );
			job->Print( true );
			printf( "\n" );
		}
		return false;
	}

    if (DEBUG_LEVEL(DEBUG_VERBOSE)) {
        printf ("Submitting JOB ");
        job->Print();
    }
  
    CondorID condorID(0,0,0);
    if (!submit_submit (job->GetCmdFile(), condorID)) {
        job->_Status = Job::STATUS_ERROR;
        _numJobsFailed++;
        return true;
    }

    job->_Status = Job::STATUS_SUBMITTED;

    if (DEBUG_LEVEL(DEBUG_VERBOSE)) {
        printf (", ");
        condorID.Print();
		printf( "\n" );
    }

    return true;
}

//---------------------------------------------------------------------------
int
Dag::PreScriptReaper( int pid, int status )
{
	Job* job;

	// get the Job* that corresponds to this pid
	jobScriptPidTable->lookup( pid, job );
	jobScriptPidTable->remove( pid );

	assert( job != NULL );
	assert( job->_Status == Job::STATUS_PRERUN );

	job->_scriptPid = 0;	// reset

	if( WIFSIGNALED( status ) ) {
		if( DEBUG_LEVEL(DEBUG_QUIET) ) {
			printf( "PRE Script of Job " );
			job->Print();
			printf( " died on %s\n", daemonCore->GetExceptionString(status) );
		}
		job->_Status = Job::STATUS_ERROR;
		_numJobsRunning--;
		_numJobsFailed++;
		return true;
	}
	else if ( WEXITSTATUS( status ) != 0 ) {
		if( DEBUG_LEVEL( DEBUG_QUIET ) ) {
			printf( "PRE Script of Job " );
			job->Print();
			printf( " failed with status %d\n", WEXITSTATUS(status) );
		}
		job->_Status = Job::STATUS_ERROR;
		_numJobsRunning--;
		_numJobsFailed++;
		return true;
	}

	if( DEBUG_LEVEL(DEBUG_QUIET) ) {
		printf( "PRE Script of Job " );
		job->Print();
		printf( " completed successfully\n" );
	}
	job->_Status = Job::STATUS_READY;
	SubmitCondor( job );
	return true;
}

//---------------------------------------------------------------------------
int
Dag::PostScriptReaper( int pid, int status )
{
	Job* job;

	// get the Job* that corresponds to this pid
	jobScriptPidTable->lookup( pid, job );
	jobScriptPidTable->remove( pid );

	assert( job != NULL );
	assert( job->_Status == Job::STATUS_POSTRUN );

	job->_scriptPid = 0;	// reset

	if( WIFSIGNALED( status ) ) {
		if( DEBUG_LEVEL(DEBUG_QUIET) ) {
			printf( "POST Script of Job " );
			job->Print();
			printf( " died on %s\n", daemonCore->GetExceptionString(status) );
		}
		job->_Status = Job::STATUS_ERROR;
		_numJobsRunning--;
		_numJobsFailed++;
		return true;
	}
	else if ( WEXITSTATUS( status ) != 0 ) {
		if( DEBUG_LEVEL( DEBUG_QUIET ) ) {
			printf( "POST Script of Job " );
			job->Print();
			printf( " failed with status %d\n", WEXITSTATUS(status) );
		}
		job->_Status = Job::STATUS_ERROR;
		_numJobsRunning--;
		_numJobsFailed++;
		return true;
	}

	if( DEBUG_LEVEL(DEBUG_QUIET) ) {
		printf( "POST Script of Job " );
		job->Print();
		printf( " completed successfully\n" );
	}

	if( job->retval == 0 ) {
		// update DAG dependencies given our successful completion
		job->_Status = Job::STATUS_DONE;
		TerminateJob( job );
		
		// submit any waiting child jobs
		if( SubmitReadyJobs() == false ) {
			// if submit fails for any reason, abort
			// TODO: abort the DAG somehow
		}
	}
	else {
		job->_Status = Job::STATUS_ERROR;
	}

	if( DEBUG_LEVEL( DEBUG_DEBUG_1 ) )
		Print_TermQ();

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
void Dag::RemoveRunningJobs () const {
    char rmCmd[ARG_MAX];
	char killCmd[ARG_MAX];

    unsigned int rmCmdLen = 0;	  // number of chars in rmCmd so far
    unsigned int rmNum = 0;		  // number of jobs appended to rmCmd so far
    unsigned int killCmdLen = 0;  // number of chars in killCmd so far
    unsigned int killNum = 0;     // number of jobs appended to killCmd so far

    ListIterator<Job> iList(_jobs);
    Job * job;
    while (iList.Next(job)) {
        if( rmNum == 0 ) {
            rmCmdLen = sprintf( rmCmd, "condor_rm" );
        }
        if( killNum == 0 ) {
            killCmdLen = sprintf( killCmd, "kill -9" );
        }

		// if the job has been submitted, condor_rm it
        if (job->_Status == Job::STATUS_SUBMITTED) {
            // Should be snprintf(), but doesn't exist on all platforms
            rmCmdLen += sprintf( &rmCmd[rmCmdLen], " %d",
								 job->_CondorID._cluster );
            rmNum++;

			if( rmCmdLen >= (ARG_MAX - 20) ) {
				util_popen( rmCmd );
				rmNum = 0;
			}
        }
		// if job has its PRE script running, hard kill it
        else if( job->_Status == Job::STATUS_PRERUN ) {
			assert( job->_scriptPid != 0 );
            killCmdLen += sprintf( &killCmd[killCmdLen], " %d",
								   job->_scriptPid );
            killNum++;

			if( killCmdLen >= (ARG_MAX - 20) ) {
				util_popen( killCmd );
				killNum = 0;
			}
        }
	}

    if( rmNum > 0) {
        util_popen( rmCmd );
    }
    if( killNum > 0) {
        util_popen( killCmd );
    }
	return;
}

//-----------------------------------------------------------------------------
void Dag::Rescue (const char * rescue_file, const char * datafile) const {
    FILE *fp = fopen(rescue_file, "w");
    if (fp == NULL) {
        debug_println (DEBUG_QUIET,
                       "Could not open %s for writing.", rescue_file);
        return;
    }

    fprintf (fp, "### DAGMan 6.1.1\n");
    fprintf (fp, "# Rescue DAG file, created after running\n");
    fprintf (fp, "#   the %s DAG file\n", datafile);
    fprintf (fp, "#\n");
    fprintf (fp, "# Total number of jobs: %d\n", NumJobs());
    fprintf (fp, "# Jobs premarked DONE: %d\n", _numJobsDone);
    fprintf (fp, "# Jobs that failed: %d\n", _numJobsFailed);

    //
    // Print the names of failed Jobs
    //
    fprintf (fp, "#   ");
    ListIterator<Job> it (_jobs);
    Job * job;
    while (it.Next(job)) {
        if (job->_Status == Job::STATUS_ERROR) {
            fprintf (fp, "%s,", job->GetJobName());
        }
    }
    fprintf (fp, "<ENDLIST>\n\n");

    //
    // Print JOBS and SCRIPTS
    //
    it.ToBeforeFirst();
    while (it.Next(job)) {
        fprintf (fp, "JOB %s %s %s\n", job->GetJobName(), job->GetCmdFile(),
                 job->_Status == Job::STATUS_DONE ? "DONE" : "");
        if (job->_scriptPre != NULL) {
            fprintf (fp, "SCRIPT PRE  %s %s\n", job->GetJobName(),
                     job->_scriptPre->GetCmd());
        }
        if (job->_scriptPost != NULL) {
            fprintf (fp, "SCRIPT POST %s %s\n", job->GetJobName(),
                     job->_scriptPost->GetCmd());
        }
    }

    //
    // Print Dependency Section
    //
    fprintf (fp, "\n");
    it.ToBeforeFirst();
    while (it.Next(job)) {
        SimpleList<JobID_t> & _queue = job->GetQueueRef(Job::Q_OUTGOING);
        if (!_queue.IsEmpty()) {
            fprintf (fp, "PARENT %s CHILD", job->GetJobName());
            
            SimpleListIterator<JobID_t> jobit (_queue);
            JobID_t jobID;
            while (jobit.Next(jobID)) {
                Job * child = GetJob(jobID);
                assert (child != NULL);
                fprintf (fp, " %s", child->GetJobName());
            }
            fprintf (fp, "\n");
        }
    }

    fclose(fp);
}


//===========================================================================
// Private Methods
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
                    printf ("Fatal error submitting job ");
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
