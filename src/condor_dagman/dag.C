/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-2001 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
 ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

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
Dag::Dag( const char* condorLogName, const int maxJobsSubmitted,
		  const int maxPreScripts, const int maxPostScripts ) :
	_maxPreScripts        (maxPreScripts),
	_maxPostScripts       (maxPostScripts),
    _condorLogInitialized (false),
    _condorLogSize        (0),
    _numJobsDone          (0),
    _numJobsFailed        (0),
    _numJobsSubmitted     (0),
    _maxJobsSubmitted     (maxJobsSubmitted)
{
    _condorLogName = strnewp (condorLogName);

 	_readyQ = new SimpleList<Job*>;
	_preScriptQ = new ScriptQ( this );
	_postScriptQ = new ScriptQ( this );
	_submitQ = new Queue<Job*>;

	if( !_readyQ || !_submitQ || !_preScriptQ || !_postScriptQ ) {
		EXCEPT( "ERROR: out of memory (%s:%d)!\n", __FILE__, __LINE__ );
	}

	debug_printf( DEBUG_DEBUG_4, "_maxJobsSubmitted = %d, "
				  "_maxPreScripts = %d, _maxPostScripts = %d\n",
				  _maxJobsSubmitted, _maxPreScripts, _maxPostScripts );
}

//-------------------------------------------------------------------------
Dag::~Dag() {
    delete [] _condorLogName;
	delete _preScriptQ;
	delete _postScriptQ;
	delete _submitQ;
	delete _readyQ;
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
    
    if (recovery) {
        debug_println (DEBUG_NORMAL, "Running in RECOVERY mode...");
        if (!ProcessLogEvents (recovery)) return false;

		// all jobs stuck in STATUS_POSTRUN need their scripts run
		jobs.ToBeforeFirst();
		while( jobs.Next( job ) ) {
			if( job->_Status == Job::STATUS_POSTRUN ) {
				_postScriptQ->Run( job->_scriptPost );
			}
		}
    }

	if( DEBUG_LEVEL( DEBUG_DEBUG_2 ) ) {
		PrintJobList();
		PrintReadyQ( DEBUG_DEBUG_2 );
	}	

	jobs.ToBeforeFirst();
	while( jobs.Next( job ) ) {
		if( job->_Status == Job::STATUS_READY &&
			job->IsEmpty( Job::Q_WAITING ) ) {
			Submit( job );
		}
	}

	return true;
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
    static int log_unk_count = 0;
	static int ulog_rd_error_count = 0;

    while (!done) {
        
        ULogEvent * e;  // refer to condor_event.h
        ULogEventOutcome outcome = _condorLog.readEvent(e);
        
        CondorID condorID;
        if (e != NULL) condorID = CondorID (e->cluster, e->proc, e->subproc);
        
        debug_printf( DEBUG_DEBUG_2, "Log outcome: %s\n",
                      ULogEventOutcomeNames[outcome] );
        
        if (outcome != ULOG_UNK_ERROR) log_unk_count = 0;

        switch (outcome) {
            
            //----------------------------------------------------------------
          case ULOG_NO_EVENT:      
            done = true;
            break;
            //----------------------------------------------------------------
          case ULOG_RD_ERROR:
			if( ++ulog_rd_error_count >= 10 ) {
				debug_printf( DEBUG_QUIET, "ERROR: repeated (%d) failures to "
							  "read job log; quitting...\n",
							  ulog_rd_error_count );
				result = false;
			}
			else {
				debug_printf( DEBUG_NORMAL, "ERROR: failure to read job log\n"
							  "\tA log event may be corrupt.  DAGMan will "
							  "skip the event and try to\n\tcontinue, but "
							  "information may have been lost.  If DAGMan "
							  "exits\n\tunfinished, but reports no failed "
							  "jobs, re-submit the rescue file\n\tto complete "
							  "the DAG.\n" );
			}
			done = true;
            break;
            //----------------------------------------------------------------
          case ULOG_UNK_ERROR:
            log_unk_count++;
            if (recovery || log_unk_count >= 5) {
				debug_printf( DEBUG_QUIET, "ERROR: repeated (%d) unknown log "
							  "errors; quitting...\n", log_unk_count );
                result = false;
            }
			debug_printf( DEBUG_NORMAL, "ERROR: unknown log error\n" );
            done   = true;
            break;
            //----------------------------------------------------------------
          case ULOG_OK:
            
			debug_printf( DEBUG_VERBOSE, "Event: %s for ",
						  ULogEventNumberNames[e->eventNumber] );

			Job* job = GetJob( condorID );

            switch(e->eventNumber) {
                
              case ULOG_EXECUTABLE_ERROR:
              case ULOG_JOB_ABORTED:

				  PrintEvent( DEBUG_VERBOSE, job );
				  if( !job ) {
					  break;
				  }

                  // inform the user that resubmit is not yet handled
				  debug_printf( DEBUG_QUIET,
								"This version of Dagman does not support "
								"job resubmition, so this node will be "
								"marked as \"failed\".\n" );
				  job->_Status = Job::STATUS_ERROR;
				  _numJobsFailed++;
				  sprintf( job->error_text, "Condor reported %s event",
						   ULogEventNumberNames[e->eventNumber] );

 				  if( job->_scriptPost == NULL ||
					  run_post_on_failure == FALSE ) {
					  break;
				  }
				  // if a POST script is specified for the job, run it
				  job->_Status = Job::STATUS_POSTRUN;
				  // there's no easy way to represent these errors as
				  // return values or signals, so just say SIGKILL
				  job->_scriptPost->_retValJob = -9;
				  if( !recovery ) {
					  _postScriptQ->Run( job->_scriptPost );
				  }

				  break;
              
              case ULOG_JOB_TERMINATED:
			  {	
				  PrintEvent( DEBUG_VERBOSE, job );
                  if( !job ) {
                      break;
                  }

				  _numJobsSubmitted--;
				  assert( _numJobsSubmitted >= 0 );

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
 
					  if( job->_scriptPost == NULL ||
						  run_post_on_failure == FALSE ) {
						  break;
					  }
                  }
				  else {
					  assert( termEvent->returnValue == 0 );
					  job->retval = 0;
					  debug_printf( DEBUG_NORMAL,
									"Job %s completed successfully.\n",
									job->GetJobName() );
				  }

                  // if a POST script is specified for the job, run it
                  if (job->_scriptPost != NULL) {
					  job->_Status = Job::STATUS_POSTRUN;
					  // let the script know the job's exit status
					  job->_scriptPost->_retValJob = termEvent->normal
						  ? termEvent->returnValue : -1;
					  if( !recovery ) {
						  _postScriptQ->Run( job->_scriptPost );
					  }
				  }
				  // no POST script was specified, so update DAG with
				  // job's successful completion
				  else {
					  job->_Status = Job::STATUS_DONE;
					  TerminateJob( job, recovery );
				  }
				  SubmitReadyJobs();
				  PrintReadyQ( DEBUG_DEBUG_2 );

				  break;
			  }

			case ULOG_POST_SCRIPT_TERMINATED:
			{
				PrintEvent( DEBUG_VERBOSE, job );
				if( !job ) {
					break;
				}

				if( !recovery ) {
					// ignore this event since the reaper has already
					// handled it
					break;
				}

				PostScriptTerminatedEvent *termEvent =
					(PostScriptTerminatedEvent*) e;
					
				if( !termEvent->normal ) {
					job->_Status = Job::STATUS_ERROR;
					// if the Condor job failed earlier, we already
					// incremented _numJobsFailed, and we want
					// error_text to report the job failure rather
					// than the POST script failure, so do this only
					// if the job succeeded and this POST script is
					// what's causing the node to fail...
					if( job->retval == 0 ) {
						_numJobsFailed++;
						sprintf( job->error_text,
								 "POST Script died on signal %d",
								 termEvent->signalNumber );
					}
				} else if( termEvent->returnValue != 0 ) {
					job->_Status = Job::STATUS_ERROR;
					// see previous comment...
					if( job->retval == 0 ) {
						_numJobsFailed++;
						sprintf( job->error_text,
								 "POST Script failed with status %d",
								 termEvent->returnValue );
					}
				} else {
					if( job->retval == 0 ) {
						// update DAG dependencies given our
						// successful completion
						job->_Status = Job::STATUS_DONE;
						TerminateJob( job );
					}
					else {
						// restore STATUS_ERROR from the job failure
						job->_Status = Job::STATUS_ERROR;
					}
				}
				break;
			}
              case ULOG_SUBMIT:
			  {
				SubmitEvent* submit_event = (SubmitEvent*) e;
				char job_name[1024];
				if( sscanf( submit_event->submitEventLogNotes,
							"DAG Node: %1023s", job_name ) != 1 ) {
					debug_printf( DEBUG_NORMAL, "WARNING: no DAG Node info "
								  "found in userlog (submit event)\n" );
				}
				job = GetJob( job_name );
				PrintEvent( DEBUG_VERBOSE, job );
				if( !job ) {
					break;
				}
				if( job ) {
					job->_CondorID = condorID;
				}

				if( recovery ) {
					job->_Status = Job::STATUS_SUBMITTED;
					_numJobsSubmitted++;
					break;
				}

					// as a sanity check, compare the job from the
					// submit event to the job we expected to see from
					// our submit queue
 					Job* expectedJob = NULL;
					if( _submitQ->dequeue( expectedJob ) == -1 ) {
						debug_printf( DEBUG_QUIET,
									  "Unrecognized submit event (for job "
									  "\"%s\") found in log (none expected)\n",
									  job->GetJobName() );
						break;
					}
					else if( job != expectedJob ) {
						assert( expectedJob != NULL );
						debug_printf( DEBUG_QUIET,
                                      "Unexpected submit event (for job "
                                      "\"%s\") found in log; job \"%s\" "
									  "was expected.\n",
									  job->GetJobName(),
									  expectedJob->GetJobName() );
						// put expectedJob back onto submit queue
						_submitQ->enqueue( expectedJob );
						break;
					}

					PrintReadyQ( DEBUG_DEBUG_2 );
					break;
			  }
			case ULOG_CHECKPOINTED:
			case ULOG_JOB_EVICTED:
			case ULOG_IMAGE_SIZE:
			case ULOG_JOB_SUSPENDED:
			case ULOG_JOB_UNSUSPENDED:
			case ULOG_JOB_HELD:
			case ULOG_JOB_RELEASED:
			case ULOG_NODE_EXECUTE:
			case ULOG_NODE_TERMINATED:
			case ULOG_SHADOW_EXCEPTION:
			case ULOG_GENERIC:
			case ULOG_EXECUTE:
			default:

                PrintEvent( DEBUG_VERBOSE, job );
                break;
			}
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
	if( !jobName ) {
		return NULL;
	}
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
	assert( job->CanSubmit() );

	// if a PRE script exists and hasn't already been run, run that
	// first -- the PRE script's reaper function will submit the
	// actual job to Condor if/when the script exits successfully

    if( job->_scriptPre && job->_scriptPre->_done == FALSE ) {
		job->_Status = Job::STATUS_PRERUN;
		_preScriptQ->Run( job->_scriptPre );
		return true;
    }
	// no PRE script exists or is done, so add job to the queue of ready jobs
	_readyQ->Append( job );
	SubmitReadyJobs();
	return TRUE;
}

// returns number of jobs submitted
int
Dag::SubmitReadyJobs()
{
//	PrintReadyQ( DEBUG_DEBUG_4 );
	// no jobs ready to submit
    if( _readyQ->IsEmpty() ) {
        return 0;
    }
    // max jobs already submitted
    if( _maxJobsSubmitted && _numJobsSubmitted >= _maxJobsSubmitted ) {
        debug_printf( DEBUG_DEBUG_1,
                      "Max jobs (%d) already running; "
					  "deferring submission of %d ready job%s.\n",
                      _maxJobsSubmitted, _readyQ->Number(),
					  _readyQ->Number() == 1 ? "" : "s" );
        return 0;
    }

	// remove & submit first job from ready queue
	Job* job;
	_readyQ->Rewind();
	_readyQ->Next( job );
	_readyQ->DeleteCurrent();
	assert( job != NULL );
	assert( job->_Status == Job::STATUS_READY );

	debug_printf( DEBUG_VERBOSE, "Submitting Job %s ...\n",
				  job->GetJobName() );
  
    CondorID condorID(0,0,0);
    if( ! submit_submit( job->GetCmdFile(), condorID, job->GetJobName() ) ) {
        job->_Status = Job::STATUS_ERROR;
        _numJobsFailed++;
		sprintf( job->error_text, "Job submit failed" );
		// the problem might be specific to that job, so keep submitting...
        return SubmitReadyJobs();
    }

	// append job to the submit queue so we can match it with its
	// submit event once the latter appears in the Condor job log
	_submitQ->enqueue( job );

    job->_Status = Job::STATUS_SUBMITTED;
	_numJobsSubmitted++;

	if( DEBUG_LEVEL( DEBUG_VERBOSE ) ) {
		printf( "\tassigned Condor ID " );
		condorID.Print();
		debug_printf( DEBUG_VERBOSE, "\n" );		
	}

    return SubmitReadyJobs() + 1;
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
		_readyQ->Append( job );
		SubmitReadyJobs();
	}
	return true;
}

//---------------------------------------------------------------------------
int
Dag::PostScriptReaper( Job* job, int status )
{
	assert( job != NULL );
	assert( job->_Status == Job::STATUS_POSTRUN );

	PostScriptTerminatedEvent e;

	debug_printf( DEBUG_NORMAL, "POST Script of Job %s ", job->GetJobName() );

	if( WIFSIGNALED( status ) ) {
        debug_printf( DEBUG_NORMAL, "died on %s\n",
                      daemonCore->GetExceptionString(status) );
		job->_Status = Job::STATUS_ERROR;
		// if the job itself failed, we already incremented
		// _numJobsFailed, and we want error_text to say job failure,
		// not the POST script's failure, so do this only if the job
		// succeeded and this POST script is causing the node to fail
		if( job->retval == 0 ) {
			_numJobsFailed++;
			sprintf( job->error_text, "POST Script died on %s",
					 daemonCore->GetExceptionString(status) );
		}
		e.normal = false;
		e.signalNumber = status;
	}
	else if ( WEXITSTATUS( status ) != 0 ) {
        debug_printf( DEBUG_NORMAL, "failed with status %d\n",
					  WEXITSTATUS(status) );
        job->_Status = Job::STATUS_ERROR;
		// see previous comment...
		if( job->retval == 0 ) {
			_numJobsFailed++;
			sprintf( job->error_text, "POST Script failed with status %d",
					 WEXITSTATUS(status) );
		}
		e.normal = true;
		e.returnValue = WEXITSTATUS(status);
	}
	else {
		debug_printf( DEBUG_NORMAL, "completed successfully.\n" );
		if( job->retval == 0 ) {
			// update DAG dependencies given our successful completion
			job->_Status = Job::STATUS_DONE;
			TerminateJob( job );
		}
		else {
			// restore STATUS_ERROR if the job had failed
			job->_Status = Job::STATUS_ERROR;
		}
		e.normal = true;
		e.returnValue = WEXITSTATUS(status);

		PrintReadyQ( DEBUG_DEBUG_2 );
	}

	UserLog ulog;
	ulog.initialize( _condorLogName, job->_CondorID._cluster,
					 job->_CondorID._proc, job->_CondorID._subproc );
	if( ! ulog.writeEvent( &e ) ) {
		debug_printf( DEBUG_NORMAL,
					  "Unable to log ULOG_POST_SCRIPT_TERMINATED event\n" );
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

void
Dag::PrintReadyQ( debug_level_t level ) const {
	debug_printf( level, "Ready Queue: " );
	if( _readyQ->IsEmpty() ) {
		debug_printf( level, "<empty>\n" );
		return;
	}
	_readyQ->Rewind();
	Job* job;
	_readyQ->Next( job );
	if( job ) {
		debug_printf( level, "%s", job->GetJobName() );
	}
	while( _readyQ->Next( job ) ) {
		debug_printf( level, ", %s", job->GetJobName() );
	}
	debug_printf( level, "\n" );
}

//---------------------------------------------------------------------------
void Dag::RemoveRunningJobs () const {
    char cmd[ARG_MAX];

    ListIterator<Job> iList(_jobs);
    Job * job;
    while (iList.Next(job)) {
		// if the job has been submitted, condor_rm it
        if (job->_Status == Job::STATUS_SUBMITTED) {
			sprintf( cmd, "condor_rm %d", job->_CondorID._cluster );
			util_popen( cmd );
        }
		// if node is running a PRE script, hard kill it
        else if( job->_Status == Job::STATUS_PRERUN ) {
			assert( job->_scriptPre->_pid != 0 );
			sprintf( cmd, "kill -9 %d", job->_scriptPre->_pid );
			util_popen( cmd );
        }
		// if node is running a POST script, hard kill it
        else if( job->_Status == Job::STATUS_POSTRUN ) {
			assert( job->_scriptPost->_pid != 0 );
			sprintf( cmd, "kill -9 %d", job->_scriptPost->_pid );
			util_popen( cmd );
        }
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
		// if child has no more parents in its waiting queue, submit it
		if( child->_Status == Job::STATUS_READY &&
			child->IsEmpty( Job::Q_WAITING ) && bootstrap == FALSE ) {
			Submit( child );
		}
    }
    _numJobsDone++;
    assert (_numJobsDone <= _jobs.Number());
}

void Dag::
PrintEvent( debug_level_t level, Job* job )
{
	if( job ) {
		debug_printf( level, "Job %s (%d.%d.%d)\n",
					  job->GetJobName(), job->_CondorID._cluster,
					  job->_CondorID._proc, job->_CondorID._subproc );
	} else {
		debug_printf( level, "Unknown Job (%d.%d.%d)\n",
					  job->_CondorID._cluster, job->_CondorID._proc,
					  job->_CondorID._subproc );
	}
}
