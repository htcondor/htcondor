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
         const int maxJobsSubmitted, const int maxScriptsRunning ) :
	_maxScriptsRunning    (maxScriptsRunning),
    _condorLogInitialized (false),
    _condorLogSize        (0),
    _lockFileName         (NULL),
    _numJobsDone          (0),
    _numJobsFailed        (0),
    _numJobsSubmitted     (0),
    _maxJobsSubmitted     (maxJobsSubmitted)
{
    _condorLogName = strnewp (condorLogName);
    _lockFileName  = strnewp (lockFileName);

	_scriptQ = new ScriptQ( this );
	_submitQ = new Queue<Job*>;

	if( _scriptQ == NULL || _submitQ == NULL ) {
		EXCEPT( "ERROR: out of memory (%s() in %s:%d)!\n",
				__FUNCTION__, __FILE__, __LINE__ );
	}
}

//-------------------------------------------------------------------------
Dag::~Dag() {
    unlink(_lockFileName);  // remove the file being used as semaphore
    delete [] _condorLogName;
    delete [] _lockFileName;
	delete _scriptQ;
	delete _submitQ;
}

//-------------------------------------------------------------------------
bool Dag::Bootstrap (bool recovery) {
    Job* job;
    ListIterator<Job> jobs (_jobs);

    // update dependencies for pre-completed jobs (jobs marked DONE in
    // the DAG input file)
	jobs.ToBeforeFirst();
    while( jobs.Next( job ) ) {
        if( job->_Status == Job::STATUS_DONE ) {
			TerminateJob( job, true );
		}
    }
	debug_println( DEBUG_VERBOSE, "Number of pre-completed jobs: %d",
                   NumJobsDone() );
    
    // Add a virtual dependency for jobs that have no parent.  In
    // other words, pretend that there is an invisible "God" job that
    // has the orphan jobs as its children.  The God job will have the
    // value (Job *) NULL.
	TQI* god = new TQI;
	if( god == NULL ) {
		debug_error( 1, DEBUG_SILENT,
					 "ERROR: out of memory (%s() in %s:%d)!\n",
					 __FUNCTION__, __FILE__, __LINE__ );
	}
	jobs.ToBeforeFirst();
	while( jobs.Next( job ) ) {
		if( job->IsEmpty( Job::Q_WAITING ) ) {
			god->children.Append( job->GetJobID() );
		}
	}

	// if there are no such jobs, remove the god job
	if( god->children.IsEmpty() ) {
		delete god;
	}

	_termQ.Append( god );

    if (recovery) {
        debug_println (DEBUG_NORMAL, "Running in RECOVERY mode...");
        if (!ProcessLogEvents (recovery)) return false;
    }

	if( DEBUG_LEVEL( DEBUG_VERBOSE ) ) {
		PrintJobList();
		Print_TermQ();
	}	

    return SubmitReadyJobs();
}

//-------------------------------------------------------------------------
bool Dag::AddDependency (Job * parent, Job * child) {
    assert (parent != NULL);
    assert (child  != NULL);
    
    if (!parent->Add (Job::Q_CHILDREN,  child->GetJobID())) return false;
    if (!child->Add  (Job::Q_PARENTS, parent->GetJobID())) return false;
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
bool Dag::DetectLogGrowth () {
    if (!_condorLogInitialized) {
        _condorLog.initialize(_condorLogName);
        _condorLogInitialized = true;
    }
    
    int fd = _condorLog.getfd();
    assert (fd != 0);
    struct stat buf;
    
    if (fstat (fd, & buf) == -1) debug_perror (2, DEBUG_QUIET, _condorLogName);
    
    int oldSize = _condorLogSize;
    _condorLogSize = buf.st_size;
    
    bool growth = (buf.st_size > oldSize);
    debug_printf( DEBUG_DEBUG_4, "%s\n",
				  growth ? "Log GREW!" : "No log growth..." );
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
                  Job * job = GetJob (condorID);
                  
                  if (DEBUG_LEVEL(DEBUG_VERBOSE)) {
                    job_print(job, true);
                    putchar ('\n');
                  }
                  
                  if (job == NULL) {
                      debug_println (DEBUG_QUIET,
                                     "Unknown terminated job found in log\n");
                      done   = true;
                      result = false;
                      break;
                  }

				  if( !recovery ) {
					  _numJobsSubmitted--;
					  assert( _numJobsSubmitted >= 0 );
				  }

                  JobTerminatedEvent * termEvent = (JobTerminatedEvent*) e;

                  if (! (termEvent->normal &&
                                termEvent->returnValue == 0)) {
                      job->_Status = Job::STATUS_ERROR;
					  _numJobsFailed++;
					  if( termEvent->normal ) {
						  job->retval = termEvent->returnValue;
						  sprintf( job->error_text, "Job terminated with "
								   "status %d.", termEvent->returnValue );
						  debug_printf( DEBUG_QUIET, "Job %s terminated with "
										"status %d.\n", job->GetJobName(),
										termEvent->returnValue );
					  } else {
						  job->retval = (0 - termEvent->signalNumber);
						  sprintf( job->error_text, "Job terminated with "
								   "signal %d", termEvent->signalNumber );
						  debug_printf( DEBUG_QUIET, "Job %s terminated with "
										"signal %d.\n", job->GetJobName(),
										termEvent->signalNumber );
					  }

					  if( run_post_on_failure == FALSE ) {
						  break;
					  }
                  }
				  else {
					  job->retval = 0;
					  debug_printf( DEBUG_NORMAL,
									"Job %s completed successfully.\n",
									job->GetJobName() );
				  }

                  // if a POST script is specified for the job, run it
                  if (job->_scriptPost != NULL) {
					  debug_printf( DEBUG_NORMAL,
									"Running POST script of Job %s...\n",
									job->GetJobName() );
					  // let the script know the job's exit status
                      job->_scriptPost->_retValJob = termEvent->normal
                          ? termEvent->returnValue : -1;

					  job->_Status = Job::STATUS_POSTRUN;
					  _scriptQ->Run( job->_scriptPost );
					  done = true;
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

						  // is this really what we want to do?  why
						  // not let the rest of the DAG run as far as
						  // possible?  --pfc
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
                
				// find the first job we submitted that hasn't yet
				// been matched with a submit event, and match it with
				// this one
				Job* job = GetSubmittedJob( recovery );
				if( job == NULL ) {
					if( DEBUG_LEVEL( DEBUG_QUIET ) ) {
						printf( "Unknown submitted job found in log\n" );
					}
					done = true;
					result = false;
					break;
				}

				// _submitQ is redundant for now, but a nice sanity-check
				Job* double_check_job;
				if( _submitQ->dequeue( double_check_job ) == -1 ) {
					if( DEBUG_LEVEL( DEBUG_QUIET ) ) {
						printf( "Unknown submitted job found in log\n" );
					}
					done = true;
					result = false;
					break;
				}
				if( job != double_check_job ) {
					debug_printf( DEBUG_DEBUG_1,
								  "GetSubmittedJob() bug: Job %s != Job %s\n",
								  job->GetJobName(),
								  double_check_job->GetJobName() );
					assert( job == double_check_job );
				}

				job->_CondorID = condorID;
				if( DEBUG_LEVEL( DEBUG_VERBOSE ) ) {
					job_print( job, true );
					printf( "\n" );
				}
                
                if (DEBUG_LEVEL(DEBUG_DEBUG_1)) Print_TermQ();
				done = true;
                break;
            }
            break;
        }
		if( e != NULL ) {
			// event allocated earlier by _condorLog.readEvent()
			delete e;
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

	// if a PRE script exists, run that first -- the PRE script's
	// reaper function will submit the actual job to Condor if/when
	// the script exits successfully

    if (job->_scriptPre != NULL) {
		debug_printf( DEBUG_NORMAL, "Running PRE Script of Job %s...\n",
					  job->GetJobName() );
		job->_Status = Job::STATUS_PRERUN;
		_scriptQ->Run( job->_scriptPre );
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
		// when would this ever happen? why not assert()?  --pfc (2000-12-01)
		if( DEBUG_LEVEL( DEBUG_VERBOSE ) ) {
			printf( "Warning: trying to submit unready job " );
			job->Print( true );
			printf( "\n" );
		}
		return false;
	}

	debug_printf( DEBUG_NORMAL, "Submitting Job %s", job->GetJobName() );
  
    CondorID condorID(0,0,0);
    if (!submit_submit (job->GetCmdFile(), condorID)) {
        job->_Status = Job::STATUS_ERROR;
        _numJobsFailed++;
		sprintf( job->error_text, "Job submit failed" );
        return true;
    }

	// append job to the submit queue so we can match it with its
	// submit event once the latter appears in the Condor job log
	_submitQ->enqueue( job );

    job->_Status = Job::STATUS_SUBMITTED;
	_numJobsSubmitted++;

	if( DEBUG_LEVEL( DEBUG_VERBOSE ) ) {
		printf( " " );
		condorID.Print();
	}
	debug_printf( DEBUG_NORMAL, "...\n" );

    return true;
}

//---------------------------------------------------------------------------
int
Dag::PreScriptReaper( Job* job, int status )
{
	assert( job != NULL );
	assert( job->_Status == Job::STATUS_PRERUN );

	if( WIFSIGNALED( status ) ) {
		debug_printf( DEBUG_QUIET, "PRE Script of Job %s died on %s\n",
					  job->GetJobName(),
					  daemonCore->GetExceptionString(status) );
		job->_Status = Job::STATUS_ERROR;
		_numJobsFailed++;
		sprintf( job->error_text, "PRE Script died on %s",
				 daemonCore->GetExceptionString(status) );
	}
	else if ( WEXITSTATUS( status ) != 0 ) {
		debug_printf( DEBUG_QUIET, "PRE Script of Job %s failed with status "
					  "%d\n", job->GetJobName(), WEXITSTATUS(status) );
		job->_Status = Job::STATUS_ERROR;
		_numJobsFailed++;
		sprintf( job->error_text, "PRE Script failed with status %d",
				 WEXITSTATUS(status) );
	}
	else {
		debug_printf( DEBUG_QUIET, "PRE Script of Job %s completed "
					  "successfully.\n", job->GetJobName() );
		job->_Status = Job::STATUS_READY;
		SubmitCondor( job );
	}
	return true;
}

//---------------------------------------------------------------------------
int
Dag::PostScriptReaper( Job* job, int status )
{
	assert( job != NULL );
	assert( job->_Status == Job::STATUS_POSTRUN );

	if( WIFSIGNALED( status ) ) {
        debug_printf( DEBUG_QUIET, "POST Script of Job %s died on %s\n",
                      job->GetJobName(),
                      daemonCore->GetExceptionString(status) );
		job->_Status = Job::STATUS_ERROR;
		_numJobsFailed++;
        sprintf( job->error_text, "POST Script died on %s",
                 daemonCore->GetExceptionString(status) );
	}
	else if ( WEXITSTATUS( status ) != 0 ) {
        debug_printf( DEBUG_QUIET, "POST Script of Job %s failed with status "
                      "%d\n", job->GetJobName(), WEXITSTATUS(status) );
        job->_Status = Job::STATUS_ERROR;
        _numJobsFailed++;
        sprintf( job->error_text, "POST Script failed with status %d",
                 WEXITSTATUS(status) );
	}
	else {
		if( DEBUG_LEVEL(DEBUG_QUIET) ) {
			printf( "POST Script of Job %s completed successfully.\n",
					job->GetJobName() );
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
			// restore STATUS_ERROR if the job had failed
			job->_Status = Job::STATUS_ERROR;
		}

		if( DEBUG_LEVEL( DEBUG_DEBUG_1 ) )
			Print_TermQ();
	}
	return true;
}

//---------------------------------------------------------------------------
void Dag::PrintJobList() const {
    Job * job;
    ListIterator<Job> iList (_jobs);
    while ((job = iList.Next()) != NULL) {
        printf ("---------------------------------------");
        job->Dump();
        putchar ('\n');
    }
    printf ("---------------------------------------");
    printf ("\t<END>\n");
}

void
Dag::PrintJobList( Job::status_t status ) const
{
    Job* job;
    ListIterator<Job> iList( _jobs );
    while( ( job = iList.Next() ) != NULL ) {
		if( job->_Status == status ) {
			printf( "---------------------------------------" );
			job->Dump();
			printf( "\n" );
		}
    }
    printf( "---------------------------------------\n" );
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
			assert( job->_scriptPre->_pid != 0 );
            killCmdLen += sprintf( &killCmd[killCmdLen], " %d",
								   job->_scriptPre->_pid );
            killNum++;

			if( killCmdLen >= (ARG_MAX - 20) ) {
				util_popen( killCmd );
				killNum = 0;
			}
        }
		// if job has its POST script running, hard kill it
        else if( job->_Status == Job::STATUS_POSTRUN ) {
			assert( job->_scriptPost->_pid != 0 );
            killCmdLen += sprintf( &killCmd[killCmdLen], " %d",
								   job->_scriptPost->_pid );
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
        SimpleList<JobID_t> & _queue = job->GetQueueRef(Job::Q_CHILDREN);
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
void
Dag::TerminateJob( Job* job, bool bootstrap )
{
    assert (job != NULL);
    assert (job->_Status == Job::STATUS_DONE);

    //
    // Report termination to all child jobs by removing parent's ID from
    // each child's waiting queue.
    //
    SimpleList<JobID_t> & qp = job->GetQueueRef(Job::Q_CHILDREN);
    SimpleListIterator<JobID_t> iList (qp);
    JobID_t childID;
    while (iList.Next(childID)) {
        Job * child = GetJob(childID);
        assert (child != NULL);
        child->Remove(Job::Q_WAITING, job->GetJobID());
    }
    _numJobsDone++;
    assert (_numJobsDone <= _jobs.Number());

	// add job to the termination queue if it has any children
	if( !bootstrap && !job->IsEmpty( Job::Q_CHILDREN ) ) {
		TQI* tqi = new TQI( job, qp );
		if( tqi == NULL ) {
			debug_error( 1, DEBUG_SILENT, "\nERROR: out of memory "
						 "(%s() in %s:%d)!\n",
						 __FUNCTION__, __FILE__, __LINE__ );
		}
		_termQ.Append(tqi);
	}
}

//---------------------------------------------------------------------------
Job * Dag::GetSubmittedJob (bool recovery) {

    Job * job_found = NULL;

    // The following loop scans the entire termination queue (_termQ)
    // It has two purposes.  First it looks for a submittable child of
    // a terminated job.  If no terminated job has such a child, then
    // the loop ends, and this function will return NULL (no job
    // found).
	//
	// [This function is called only after we've just seen a new
	// submit event in the log, and assumes that that submit event
	// must correspond to the first unsubmitted child found in the
	// termination queue, since that's the first one we would have
	// submitted.  With the advent of asynchronous PRE/POST scripts
	// this is no longer a safe assumption, and can result in buggy
	// behavior... this needs to be fixed.  --pfc]
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
    _termQ.Rewind();
    while (_termQ.Next(tqi)) {
        assert (!tqi->children.IsEmpty());
        JobID_t match_ID;  // The current Job ID being examined
        JobID_t found_ID;  // The ID of the original child found

        bool found_on_this_line = false;  // for debugging

        tqi->children.Rewind();
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

				// there shouldn't be duplicate children of the same job
                break; 
            }
        }
    }
    if (recovery && job_found != NULL) {
        job_found->_Status = Job::STATUS_SUBMITTED;
		_numJobsSubmitted++;
		assert( _numJobsSubmitted == 0 || 
				(_numJobsSubmitted <= _maxJobsSubmitted) );
    }
    return job_found;
}

//-------------------------------------------------------------------------
bool Dag::SubmitReadyJobs () {

	// if the termination queue is empty, return
	if( _termQ.IsEmpty() ) {
		debug_printf( DEBUG_DEBUG_1, "%s(): termination queue is empty\n",
					  __FUNCTION__ );
        return true;
    }

	// if we've already submitted max jobs, return
	if( _maxJobsSubmitted != 0 && _numJobsSubmitted >= _maxJobsSubmitted ) {
		assert( _numJobsSubmitted <= _maxJobsSubmitted );
		debug_printf( DEBUG_DEBUG_1,
					  "%s(): max scripts (%d) already running\n",
					  __FUNCTION__, _maxJobsSubmitted );
		return true;
	}

	// look at the children of each terminated job to see if any of
	// them are submittable, and if so, submit them

    TQI* tqi;
    _termQ.Rewind();
	while( (tqi = _termQ.Next()) ) {
		tqi->children.Rewind();
		JobID_t childID;
		while( tqi->children.Next( childID ) &&
			   ( (_maxJobsSubmitted == 0) ||
				 (_numJobsSubmitted < _maxJobsSubmitted)) ) {
			debug_printf( DEBUG_DEBUG_4,
						  "%s() examining JobID #%d ... ", __FUNCTION__,
						  childID );
			Job* child = GetJob( childID );
			assert( child != NULL );
			if( child->CanSubmit() ) {
				debug_printf( DEBUG_DEBUG_4, "submittable!\n" );
				if( !Submit( child ) ) {
					if( DEBUG_LEVEL( DEBUG_QUIET ) ) {
						printf( "Fatal error submitting job " );
						child->Print( true );
						printf( "\n" );
					}
					return false;
				}
			}
			else {
				debug_printf( DEBUG_DEBUG_4, "not submittable\n" );
			}
		}
	}
    return true;
}
