/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

//
// Local DAGMan includes
//
#include "condor_common.h"
#include "dag.h"
#include "debug.h"
#include "submit.h"
#include "util.h"
#include "dagman_main.h"

#include "simplelist.h"
#include "condor_string.h"  /* for strnewp() */
#include "../condor_daemon_core.V6/condor_daemon_core.h"

#if defined(BUILD_HELPER)
#include "helper.h"
#endif

//---------------------------------------------------------------------------
Dag::Dag( StringList &condorLogFiles, const int maxJobsSubmitted,
		  const int maxPreScripts, const int maxPostScripts,
		  const char* dapLogName ) :
    _maxPreScripts        (maxPreScripts),
    _maxPostScripts       (maxPostScripts),
    _condorLogFiles       (condorLogFiles),
    _condorLogInitialized (false),
    _dapLogName           (NULL),
    _dapLogInitialized    (false),             //<--DAP
    _dapLogSize           (0),                 //<--DAP
    _numJobsDone          (0),
    _numJobsFailed        (0),
    _numJobsSubmitted     (0),
    _maxJobsSubmitted     (maxJobsSubmitted)
{

	ASSERT( condorLogFiles.number() > 0 || dapLogName );
	if( dapLogName ) {
		_dapLogName = strnewp( dapLogName );
		ASSERT( _dapLogName );
	}

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
	DFS_ORDER = 0;

	_dot_file_name         = NULL;
	_dot_include_file_name = NULL;
	_update_dot_file       = false;
	_overwrite_dot_file    = true;
	_dot_file_name_suffix  = 0;

	return;
}

//-------------------------------------------------------------------------
Dag::~Dag() {
		// remember kids, delete is safe *even* if ptr == NULL...
    // NOTE: we cast this to char* because older MS compilers
    // (contrary to the ISO C++ spec) won't allow you to delete a
    // const.  This has apparently been fixed in Visual C++ .NET, but
    // as of 6/2004 we don't yet use that.  For details, see:
    // http://support.microsoft.com/support/kb/articles/Q131/3/22.asp
    delete[] (char*) _dapLogName;

    // delete all jobs in _jobs
    Job *job = NULL;
    _jobs.Rewind();
    while( (job = _jobs.Next()) ) {
      ASSERT( job != NULL );
      delete job;
      _jobs.DeleteCurrent();
    }

    delete _preScriptQ;
    delete _postScriptQ;
    delete _submitQ;
    delete _readyQ;

	delete[] _dot_file_name;
	delete[] _dot_include_file_name;
    
    return;
}

//-------------------------------------------------------------------------
bool Dag::Bootstrap (const Dagman &dm, bool recovery) {
    Job* job;
    ListIterator<Job> jobs (_jobs);

    // update dependencies for pre-completed jobs (jobs marked DONE in
    // the DAG input file)
    jobs.ToBeforeFirst();
    while( jobs.Next( job ) ) {
		if( job->GetStatus() == Job::STATUS_DONE ) {
			TerminateJob( job, true );
		}
    }
    debug_printf( DEBUG_VERBOSE, "Number of pre-completed jobs: %d\n",
				  NumJobsDone() );
    
    if (recovery) {
        debug_printf( DEBUG_NORMAL, "Running in RECOVERY mode...\n" );

		if( _condorLogFiles.number() > 0 ) {
			if( !ProcessLogEvents( dm, CONDORLOG, recovery ) ) {
				return false;
			}
		}
		if( _dapLogName ) {
			if( !ProcessLogEvents( dm, DAPLOG, recovery ) ) {
				return false;
			}
		}

		// all jobs stuck in STATUS_POSTRUN need their scripts run
		jobs.ToBeforeFirst();
		while( jobs.Next( job ) ) {
			if( job->GetStatus() == Job::STATUS_POSTRUN ) {
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
		if( job->GetStatus() == Job::STATUS_READY &&
			job->IsEmpty( Job::Q_WAITING ) ) {
			StartNode( job );
		}
    }
    
    return true;
}

//-------------------------------------------------------------------------
bool
Dag::AddDependency( Job* parent, Job* child )
{
	if( !parent || !child ) {
		return false;
	}
	if( !parent->AddChild( child ) ) {
		return false;
	}
	if( !child->AddParent( parent ) ) {
			// reverse earlier successful AddChild() so we don't end
			// up with asymetric dependencies
		if( !parent->RemoveChild( child ) ) {
				// the DAG state is FUBAR, so we should bail...
			ASSERT( false );
		}
		return false;
	}
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
bool
Dag::DetectCondorLogGrowth () {

	if( _condorLogFiles.number() <= 0 ) {
		debug_printf( DEBUG_DEBUG_1, "WARNING: DetectCondorLogGrowth() called "
					  "but no Condor log defined\n" );
		return false;
	}

    if (!_condorLogInitialized) {
		_condorLogInitialized = _condorLog.initialize( _condorLogFiles );
		if( !_condorLogInitialized ) {
				// this can be normal before we've actually submitted
				// any jobs and the log doesn't yet exist, but is
				// likely a problem if it persists...
			debug_printf( DEBUG_VERBOSE, "ERROR: failed to initialize condor "
						  "job log -- ignore unless error repeats\n");
			return false;
		}
    }

	bool growth = _condorLog.detectLogGrowth();
    debug_printf( DEBUG_DEBUG_4, "%s\n",
				  growth ? "Log GREW!" : "No log growth..." );
    return growth;
}

//-------------------------------------------------------------------------
bool Dag::DetectDaPLogGrowth () {
	if( !_dapLogName ) {
		debug_printf( DEBUG_DEBUG_1, "WARNING: DetectDaPLogGrowth() called "
					  "but no dap log defined\n" );
		return false;
	}
	if( !_dapLogInitialized ) {
		_dapLogInitialized = _dapLog.initialize( _dapLogName );
		if( !_dapLogInitialized ) {
				// this can be normal before we've actually submitted
				// any jobs and the log doesn't yet exist, but is
				// likely a problem if it persists...
			debug_printf( DEBUG_VERBOSE, "ERROR: failed to initialize dap "
						  "job log (%s) -- ignore unless error repeats\n",
						  _dapLogName );
			return false;
		}
	}

    int fd = _dapLog.getfd();
    ASSERT( fd );
    struct stat buf;
    
    if( fstat( fd, &buf ) == -1 ) {
		debug_printf( DEBUG_QUIET, "ERROR: can't stat dap log (%s): %s\n",
					  _dapLogName, strerror (errno ) );
		return false;
    }
    
    int oldSize = _dapLogSize;
    _dapLogSize = buf.st_size;
    
    bool growth = (buf.st_size > oldSize);
    debug_printf( DEBUG_DEBUG_4, "%s\n",
				  growth ? "Log GREW!" : "No log growth..." );
    return growth;
}

//-------------------------------------------------------------------------
// Developer's Note: returning false tells main_timer to abort the DAG
bool Dag::ProcessLogEvents (const Dagman & dm, int logsource, bool recovery) {

 if (logsource == CONDORLOG){
    if (!_condorLogInitialized) {
      _condorLogInitialized = _condorLog.initialize(_condorLogFiles);
    }
  }
  else if (logsource == DAPLOG){
    if (!_dapLogInitialized) {
      _dapLogInitialized = _dapLog.initialize(_dapLogName);
    }
  }

 
 bool done = false;  // Keep scanning until ULOG_NO_EVENT
 bool result = true;
 static int log_unk_count = 0;
 static int ulog_rd_error_count = 0;

 while (!done) {

        ULogEvent* e = NULL;
        ULogEventOutcome outcome;

	if (logsource == CONDORLOG){
	  outcome = _condorLog.readEvent(e);
	}
	else if (logsource == DAPLOG){
	  outcome = _dapLog.readEvent(e);
	}

	Job* job = NULL;
	const char* eventName = NULL;
        
        CondorID condorID;
        if (e != NULL) condorID = CondorID (e->cluster, e->proc, e->subproc);
        
        debug_printf( DEBUG_DEBUG_4, "Log outcome: %s\n",
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
            
			ASSERT( e != NULL );

			job = GetJob( condorID );
			eventName = ULogEventNumberNames[e->eventNumber];

			if( e->eventNumber != ULOG_SUBMIT ) {
				PrintEvent( DEBUG_VERBOSE, eventName, job, condorID );
			}
				// submit events are printed specially below, since we
				// want to match them with their DAG nodes first...

				// This variable is used to turn off the new event checking
				// code for duplicate terminate/abort events.  DAGMan
				// tolerates them, but the CheckEvents class does not.
			bool	checkThisEvent = true;

            switch(e->eventNumber) {

              case ULOG_EXECUTABLE_ERROR:
              case ULOG_JOB_ABORTED:

				  if( !job ) {
					  break;
				  }

				  if( job->_Status == Job::STATUS_ERROR ) {
						  // sometimes condor prints *both* a
						  // termination and an abort event for a job;
						  // in such cases, we need to make sure not
						  // to process both...
					  debug_printf( DEBUG_NORMAL,
									"WARNING: Job %s already marked %s; "
									"ignoring job aborted event...\n",
									job->GetJobName(), job->GetStatusName() );
					  checkThisEvent = false;
					  break;
				  }

                  _numJobsSubmitted--;
                  ASSERT( _numJobsSubmitted >= 0 );

				  job->_Status = Job::STATUS_ERROR;

				  if( job->GetRetries() < job->GetRetryMax() ) {
					  RestartNode( job, recovery );
					  break;
				  }

				  sprintf( job->error_text, "Condor reported %s event",
						   ULogEventNumberNames[e->eventNumber] );
				  if( job->GetRetryMax() > 0 ) {
					  // add # of retries to error_text
					  char *tmp = strnewp( job->error_text );
					  sprintf( job->error_text, "%s (after %d node retries)",
							   tmp, job->GetRetries() );
					  delete tmp;
				  }
 				  if( job->_scriptPost == NULL || 
					  run_post_on_failure == FALSE ) {
					  _numJobsFailed++;
					  break;
				  }
				  // a POST script is specified for the job, so run it
				  job->_Status = Job::STATUS_POSTRUN;
				  // there's no easy way to represent these errors as
				  // return values or signals, so just say SIGABRT
				  job->_scriptPost->_retValJob = -6;
				  if( !recovery ) {
					  _postScriptQ->Run( job->_scriptPost );
				  }

				  break;
              
              case ULOG_JOB_TERMINATED:
			  {	
                  if( !job ) {
                      break;
                  }

				  _numJobsSubmitted--;
				  ASSERT( _numJobsSubmitted >= 0 );

                  JobTerminatedEvent * termEvent = (JobTerminatedEvent*) e;

                  if( !(termEvent->normal && termEvent->returnValue == 0) ) {
					  // job failed or was killed by a signal
					  if( termEvent->normal ) {
						  job->retval = termEvent->returnValue;
						  sprintf( job->error_text, "Job failed with "
								   "status %d", termEvent->returnValue );
						  debug_printf( DEBUG_QUIET, "Job %s failed with "
										"status %d.\n", job->GetJobName(),
										termEvent->returnValue );
					  }
					  else {
						  job->retval = (0 - termEvent->signalNumber);
						  sprintf( job->error_text, "Job failed with "
								   "signal %d", termEvent->signalNumber );
						  debug_printf( DEBUG_QUIET, "Job %s failed with "
										"signal %d.\n", job->GetJobName(),
										termEvent->signalNumber );
					  }

                      job->_Status = Job::STATUS_ERROR;

					  if( job->GetRetries() < job->GetRetryMax() ) {
						  RestartNode( job, recovery );
						  break;
					  }
					  else {
						  // no more retries -- job failed
						  if( job->GetRetryMax() > 0 ) {
							  // add # of retries to error_text
							  char *tmp = strnewp( job->error_text );
							  sprintf( job->error_text,
									   "%s (after %d node retries)", tmp,
									   job->GetRetries() );
							  delete tmp;   
						  }
						  if( job->_scriptPost == NULL ||
							  run_post_on_failure == FALSE ) {
							  _numJobsFailed++;
							  break;
						  }
					  }
                  }
				  else {
					  // job succeeded
					  ASSERT( termEvent->returnValue == 0 );
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
					  TerminateJob( job, recovery );
				  }
				  PrintReadyQ( DEBUG_DEBUG_2 );

				  break;
			  }

			case ULOG_POST_SCRIPT_TERMINATED:
			{
				if( !job ) {
					break;
				}
				ASSERT( job->GetStatus() == Job::STATUS_POSTRUN );

				PostScriptTerminatedEvent *termEvent =
					(PostScriptTerminatedEvent*) e;
					
				debug_printf( DEBUG_NORMAL, "POST Script of Job %s ",
							  job->GetJobName() );

				if( !termEvent->normal ) {
					// POST script failed due to a signal
					debug_dprintf( D_ALWAYS | D_NOHEADER, DEBUG_NORMAL,
								   "died on signal %d\n",
								   termEvent->signalNumber );

                    job->_Status = Job::STATUS_ERROR;

					if( job->GetRetries() < job->GetRetryMax() ) {
						RestartNode( job, recovery );
                    }
                    else {
						// no more retries -- node failed
						_numJobsFailed++;
						if( job->retval == 0 ) {
							// job had succeeded but POST failed
							sprintf( job->error_text,
									 "POST Script died on signal %d",
									 termEvent->signalNumber );
						}
						else {
							// job failed prior to POST failing
                            sprintf( job->error_text, 
                                     "Job failed with status %d and "
									 "POST Script died on signal %d",
									 job->retval, termEvent->signalNumber );
						}
						if( job->GetRetryMax() > 0 ) {
							// add # of retries to error_text
							char *tmp = strnewp( job->error_text );
							sprintf( job->error_text,
									 "%s (after %d node retries)", tmp,
									 job->GetRetries() );
							delete tmp;   
						}
					}
				}
				else if( termEvent->returnValue != 0 ) {
					// POST script returned non-zero
					debug_dprintf( D_ALWAYS | D_NOHEADER, DEBUG_NORMAL,
								   "failed with status %d\n",
								   termEvent->returnValue );

                    job->_Status = Job::STATUS_ERROR;

					if( job->GetRetries() < job->GetRetryMax() ) {
						RestartNode( job, recovery );
					}
					else {
						// no more retries -- node failed
						_numJobsFailed++;
						if( job->retval == 0 ) {
					   		// job had succeeded but POST failed
							sprintf( job->error_text,
									 "POST Script failed with status %d",
									 termEvent->returnValue );
						}
						else {
						   	// job failed prior to POST failing
							sprintf( job->error_text,
                                     "Job failed with status %d and "
									 "POST Script failed with status %d",
									 job->retval, termEvent->returnValue );
						}
						if( job->GetRetryMax() > 0 ) {
							// add # of retries to error_text
                            char *tmp = strnewp( job->error_text );
                            sprintf( job->error_text,
                                     "%s (after %d node retries)", tmp,
                                     job->GetRetries() );
							delete tmp;
						}
					}
				}
				else {
					// POST script succeeded
					debug_dprintf( D_ALWAYS | D_NOHEADER, DEBUG_NORMAL,
								   "completed successfully.\n" );
					TerminateJob( job, recovery );
				}
				PrintReadyQ( DEBUG_DEBUG_4 );
				break;
			}
              case ULOG_SUBMIT:
			  {
				SubmitEvent* submit_event = (SubmitEvent*) e;
				if( submit_event->submitEventLogNotes ) {
					char job_name[1024] = "";
					if( sscanf( submit_event->submitEventLogNotes,
								"DAG Node: %1023s", job_name ) == 1 ) {
						job = GetJob( job_name );
						if( job ) {
							if( !recovery ) {
									// as a sanity-check, compare the
									// job ID in the userlog with the
									// one that appeared earlier in
									// the submit command's stdout
                                if( condorID.Compare( job->_CondorID ) != 0 ) {
                                    debug_printf( DEBUG_QUIET,
												  "ERROR: job %s: job ID in userlog submit event (%d.%d) doesn't match ID reported earlier by submit command (%d.%d)!  Trusting the userlog for now., but this is scary!\n",
                                                  job_name,
                                                  condorID._cluster,
                                                  condorID._proc,
                                                  job->_CondorID._cluster,
                                                  job->_CondorID._proc );
                                }
							}
							job->_CondorID = condorID;
						}
					}
				}
				PrintEvent( DEBUG_VERBOSE, eventName, job, condorID );
				if( !job ) {
					break;
				}

				if( recovery ) {
					job->_Status = Job::STATUS_SUBMITTED;
					_numJobsSubmitted++;
					break;
				}

				// if we only have one log, compare
				// the order of submit events in the
				// log to the order in which we
				// submitted the jobs -- but if we
				// have >1 userlog we can't count on
				// any order, so we can't sanity-check
				// in this way

				if( _condorLogFiles.number() == 1 ) {

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
						ASSERT( expectedJob != NULL );
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
                break;
			}

			if ( dm.doEventChecks && checkThisEvent ) {
				MyString	eventError;
				if ( !_ce.CheckAnEvent(e, eventError) ) {
    				debug_printf( DEBUG_VERBOSE, "%s\n",
							eventError.Value() );
					result = false;
				} else {
    				debug_printf( DEBUG_DEBUG_1, "Event okay\n");
				}
			}
        }
		if( e != NULL ) {
			// event allocated earlier by _condorLog.readEvent()
			delete e;
		}
    }
    if (DEBUG_LEVEL(DEBUG_VERBOSE) && recovery) {
        debug_printf( DEBUG_QUIET, "    -----------------------\n");
        debug_printf( DEBUG_QUIET, "       Recovery Complete\n");
        debug_printf( DEBUG_QUIET, "    -----------------------\n");
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
		if( strcasecmp( job->GetJobName(), jobName ) == 0 ) {
			return job;
		}
    }
    return NULL;
}


bool
Dag::NodeExists( const char* nodeName ) const
{
  if( !nodeName ) {
    return false;
  }
  ListIterator<Job> nodeList( _jobs );
  Job *node;
  while( (node = nodeList.Next()) ) {
    if( strcasecmp( node->GetJobName(), nodeName ) == 0 ) {
      return true;
    }
  }
  return false;
}


//---------------------------------------------------------------------------
Job * Dag::GetJob (const CondorID condorID) const {
    ListIterator<Job> iList (_jobs);
    Job * job;
    while ((job = iList.Next())) if (job->_CondorID == condorID) return job;
    return NULL;
}

//-------------------------------------------------------------------------
bool
Dag::StartNode( Job *node )
{
    ASSERT( node != NULL );
	ASSERT( node->CanSubmit() );

	// if a PRE script exists and hasn't already been run, run that
	// first -- the PRE script's reaper function will submit the
	// actual job to Condor if/when the script exits successfully

    if( node->_scriptPre && node->_scriptPre->_done == FALSE ) {
		node->_Status = Job::STATUS_PRERUN;
		_preScriptQ->Run( node->_scriptPre );
		return true;
    }
	// no PRE script exists or is done, so add job to the queue of ready jobs
	_readyQ->Append( node );
	return TRUE;
}

// returns number of jobs submitted
int
Dag::SubmitReadyJobs(const Dagman &dm)
{
#if defined(BUILD_HELPER)
	Helper helperObj;
#endif

	int numSubmitsThisCycle = 0;
	while( numSubmitsThisCycle < dm.max_submits_per_interval ) {

//	PrintReadyQ( DEBUG_DEBUG_4 );
	// no jobs ready to submit
    if( _readyQ->IsEmpty() ) {
        return numSubmitsThisCycle;
    }
    // max jobs already submitted
    if( _maxJobsSubmitted && _numJobsSubmitted >= _maxJobsSubmitted ) {
        debug_printf( DEBUG_DEBUG_1,
                      "Max jobs (%d) already running; "
					  "deferring submission of %d ready job%s.\n",
                      _maxJobsSubmitted, _readyQ->Number(),
					  _readyQ->Number() == 1 ? "" : "s" );
        return numSubmitsThisCycle;
    }

	// remove & submit first job from ready queue
	Job* job;
	_readyQ->Rewind();
	_readyQ->Next( job );
	_readyQ->DeleteCurrent();
	ASSERT( job != NULL );
	ASSERT( job->GetStatus() == Job::STATUS_READY );

	if( job->NumParents() > 0 && dm.submit_delay == 0 ) {
			// if we don't already have a submit_delay, sleep for one
			// second here, so we can be sure that this job's submit
			// event will be unambiguously later than the termination
			// events of its parents, given that userlog timestamps
			// only have a resolution of one second.  (Because of the
			// new feature allowing distinct userlogs for each job, we
			// can't just rely on the physical order in a single log
			// file.)

			// TODO: as an optimization, check if, for all parents,
			// the logfile is the same as the child -- if yes, we can
			// skip the sleep...
		sleep( 1 );
	}

		// sleep for a specified time before submitting
	if( dm.submit_delay ) {
		debug_printf( DEBUG_VERBOSE, "Sleeping for %d s "
					  "(DAGMAN_SUBMIT_DELAY) to throttle submissions...\n",
					  dm.submit_delay );
		sleep( dm.submit_delay );
	}

	debug_printf( DEBUG_VERBOSE, "Submitting %s Job %s ...\n",
				  job->JobTypeString(), job->GetJobName() );

    CondorID condorID(0,0,0);
    MyString cmd_file = job->GetCmdFile();

#if defined(BUILD_HELPER)
    char *helper = param( "DAGMAN_HELPER_COMMAND" );
    if( helper ) {
      debug_printf( DEBUG_VERBOSE, "  passing original submit file (%s) "
		    "to helper (%s)\n", cmd_file.Value(), helper );
      cmd_file = helperObj.resolve( cmd_file.Value() ).c_str();
      if( cmd_file.Length() == 0 ) {
	debug_printf( DEBUG_QUIET, "ERROR: helper (%s) "
		      "failed for Job %s: submit aborted\n", helper,
		      job->GetJobName() );
	// NOTE: this failure does not currently observe the "retry"
	// feature (for better or worse)
        job->_Status = Job::STATUS_ERROR;
        _numJobsFailed++;
	sprintf( job->error_text, "helper (%s) failed",
		 helper );
	free( helper );
	helper = NULL;
	// the problem might be specific to that job, so keep submitting...
	continue;  // while( numSubmitsThisCycle < max_submits_per_interval )
      }
      debug_printf( DEBUG_VERBOSE,
		    "  using new submit file (%s) from helper\n",
		    cmd_file.Value(), job->GetJobName() );
      free( helper );
      helper = NULL;
    }
#endif //BUILD_HELPER

    if( job->JobType() == Job::TYPE_CONDOR ) {
      if( ! submit_submit( dm, cmd_file.Value(), condorID, job->GetJobName(),
                           job->varNamesFromDag, job->varValsFromDag ) ) {
	// NOTE: this failure does not observe the "retry" feature
	// (for better or worse)
	job->_Status = Job::STATUS_ERROR;
	_numJobsFailed++;
	sprintf( job->error_text, "Job submit failed" );
	// the problem might be specific to that job, so keep submitting...
	continue;  // while( numSubmitsThisCycle < max_submits_per_interval )
      }
    } //job ==  condor_job
    
    else if( job->JobType() == Job::TYPE_STORK ) {
      if( ! dap_submit( dm, cmd_file.Value(), condorID, job->GetJobName() ) ) {
	// NOTE: this failure does not observe the "retry" feature
	// (for better or worse)
	job->_Status = Job::STATUS_ERROR;
	_numJobsFailed++;
	sprintf( job->error_text, "Job submit failed" );
	// the problem might be specific to that job, so keep submitting...
	continue;  // while( numSubmitsThisCycle < max_submits_per_interval )
      }
    }

    // append job to the submit queue so we can match it with its
    // submit event once the latter appears in the Condor job log
    if( _submitQ->enqueue( job ) == -1 ) {
		debug_printf( DEBUG_QUIET, "ERROR: _submitQ->enqueue() failed!\n" );
	}

    job->_Status = Job::STATUS_SUBMITTED;
    _numJobsSubmitted++;
    
        // stash the job ID reported by the submit command, to compare
        // with what we see in the userlog later as a sanity-check
        // (note: this sanity-check is not possible during recovery,
        // since we won't have seen the submit command stdout...)
	job->_CondorID = condorID;

	debug_printf( DEBUG_VERBOSE, "\tassigned %s ID (%d.%d)\n",
				  job->JobTypeString(), condorID._cluster, condorID._proc );
    
    numSubmitsThisCycle++;
	}
	return numSubmitsThisCycle;
}

//---------------------------------------------------------------------------
int
Dag::PreScriptReaper( const char* nodeName, int status )
{
	ASSERT( nodeName != NULL );
	Job* job = GetJob( nodeName );
	ASSERT( job != NULL );
	ASSERT( job->GetStatus() == Job::STATUS_PRERUN );

	if( WIFSIGNALED( status ) || WEXITSTATUS( status ) != 0 ) {
		// if script returned failure or was killed by a signal
		if( WIFSIGNALED( status ) ) {
			// if script was killed by a signal
			debug_printf( DEBUG_QUIET, "PRE Script of Job %s died on %s\n",
						  job->GetJobName(),
						  daemonCore->GetExceptionString(status) );
			sprintf( job->error_text, "PRE Script died on %s",
					 daemonCore->GetExceptionString(status) );
            job->retval = ( 0 - WTERMSIG(status ) );
		}
		else if( WEXITSTATUS( status ) != 0 ) {
			// if script returned failure
			debug_printf( DEBUG_QUIET,
						  "PRE Script of Job %s failed with status %d\n",
						  job->GetJobName(), WEXITSTATUS(status) );
			sprintf( job->error_text, "PRE Script failed with status %d",
					 WEXITSTATUS(status) );
            job->retval = WEXITSTATUS( status );
		}

        job->_Status = Job::STATUS_ERROR;

		if( job->GetRetries() < job->GetRetryMax() ) {
			RestartNode( job, false );
		}
		else {
			_numJobsFailed++;
			if( job->GetRetryMax() > 0 ) {
				// add # of retries to error_text
				char *tmp = strnewp( job->error_text );
				sprintf( job->error_text,
						 "%s (after %d node retries)", tmp,
						 job->GetRetries() );
				delete tmp;   
			}
		}
	}
	else {
		debug_printf( DEBUG_QUIET, "PRE Script of Job %s completed "
					  "successfully.\n", job->GetJobName() );
		job->_Status = Job::STATUS_READY;
		_readyQ->Append( job );
	}
	return true;
}

// Note that the actual handling of the post script's exit status is
// done not when the reaper is called, but in ProcessLogEvents when
// the log event (written by the reaper) is seen...
int
Dag::PostScriptReaper( const char* nodeName, int status )
{
	ASSERT( nodeName != NULL );
	Job* job = GetJob( nodeName );
	ASSERT( job != NULL );
	ASSERT( job->GetStatus() == Job::STATUS_POSTRUN );

	PostScriptTerminatedEvent e;
	UserLog ulog;

	if( WIFSIGNALED( status ) ) {
		e.normal = false;
		e.signalNumber = status;
	}
	else {
		e.normal = true;
		e.returnValue = WEXITSTATUS( status );
	}

	ulog.initialize( job->_logFile, job->_CondorID._cluster,
					 job->_CondorID._proc, job->_CondorID._subproc );

	if( !ulog.writeEvent( &e ) ) {
		debug_printf( DEBUG_QUIET,
					  "Unable to log ULOG_POST_SCRIPT_TERMINATED event\n" );
	}
	return true;
}

//---------------------------------------------------------------------------
void Dag::PrintJobList() const {
    Job * job;
    ListIterator<Job> iList (_jobs);
    while ((job = iList.Next()) != NULL) {
        job->Dump();
    }
    dprintf( D_ALWAYS, "---------------------------------------\t<END>\n" );
}

void
Dag::PrintJobList( Job::status_t status ) const
{
    Job* job;
    ListIterator<Job> iList( _jobs );
    while( ( job = iList.Next() ) != NULL ) {
		if( job->GetStatus() == status ) {
			job->Dump();
		}
    }
    dprintf( D_ALWAYS, "---------------------------------------\t<END>\n" );
}

void
Dag::PrintReadyQ( debug_level_t level ) const {
	if( DEBUG_LEVEL( level ) ) {
		dprintf( D_ALWAYS, "Ready Queue: " );
		if( _readyQ->IsEmpty() ) {
			dprintf( D_ALWAYS | D_NOHEADER, "<empty>\n" );
			return;
		}
		_readyQ->Rewind();
		Job* job;
		_readyQ->Next( job );
		if( job ) {
			dprintf( D_ALWAYS | D_NOHEADER, "%s", job->GetJobName() );
		}
		while( _readyQ->Next( job ) ) {
			dprintf( D_ALWAYS | D_NOHEADER, ", %s", job->GetJobName() );
		}
		dprintf( D_ALWAYS | D_NOHEADER, "\n" );
	}
}

//---------------------------------------------------------------------------
void Dag::RemoveRunningJobs () const {
    char cmd[ARG_MAX];

		// first, remove all Condor jobs submitted by this DAGMan
	debug_printf( DEBUG_NORMAL, "Removing any/all submitted Condor jobs...\n",
				  cmd );
	snprintf( cmd, ARG_MAX, "condor_rm -const \'%s == \"%s\"\'",
			  DAGManJobIdAttrName, DAGManJobId );
	debug_printf( DEBUG_VERBOSE, "Executing: %s\n", cmd );
	util_popen( cmd );
		// TODO: we need to check for failures here

    ListIterator<Job> iList(_jobs);
    Job * job;
    while (iList.Next(job)) {
		ASSERT( job != NULL );
			// if node has a Stork job that is presently submitted,
			// remove it individually (this is necessary because
			// DAGMan's job ID can't currently be inserted into the
			// Stork job ad, and thus we can't do a "dap_rm -const..." 
			// like we do with Condor; this should be fixed)
		if( job->JobType() == Job::TYPE_STORK &&
			job->GetStatus() == Job::STATUS_SUBMITTED ) {
			snprintf( cmd, ARG_MAX, "dap_rm %d", job->_CondorID._cluster );
			debug_printf( DEBUG_VERBOSE, "Executing: %s\n", cmd );
			util_popen( cmd );
				// TODO: we need to check for failures here
        }
		// if node is running a PRE script, hard kill it
        else if( job->GetStatus() == Job::STATUS_PRERUN ) {
			ASSERT( job->_scriptPre );
			ASSERT( job->_scriptPre->_pid != 0 );
			if (daemonCore->Shutdown_Fast(job->_scriptPre->_pid) == FALSE) {
				debug_printf(DEBUG_QUIET,
				             "WARNING: shutdown_fast() failed on pid %d: %s\n",
				             job->_scriptPre->_pid, strerror(errno));
			}
        }
		// if node is running a POST script, hard kill it
        else if( job->GetStatus() == Job::STATUS_POSTRUN ) {
			ASSERT( job->_scriptPost );
			ASSERT( job->_scriptPost->_pid != 0 );
			if(daemonCore->Shutdown_Fast(job->_scriptPost->_pid) == FALSE) {
				debug_printf(DEBUG_QUIET,
				             "WARNING: shutdown_fast() failed on pid %d: %s\n",
				             job->_scriptPost->_pid, strerror( errno ));
			}
        }
	}
	return;
}

//-----------------------------------------------------------------------------
void Dag::Rescue (const char * rescue_file, const char * datafile) const {
    FILE *fp = fopen(rescue_file, "w");
    if (fp == NULL) {
        debug_printf( DEBUG_QUIET, "Could not open %s for writing.\n",
					  rescue_file);
        return;
    }

    fprintf (fp, "# Rescue DAG file, created after running\n");
    fprintf (fp, "#   the %s DAG file\n", datafile);
    fprintf (fp, "#\n");
    fprintf (fp, "# Total number of Nodes: %d\n", NumJobs());
    fprintf (fp, "# Nodes premarked DONE: %d\n", _numJobsDone);
    fprintf (fp, "# Nodes that failed: %d\n", _numJobsFailed);

    //
    // Print the names of failed Jobs
    //
    fprintf (fp, "#   ");
    ListIterator<Job> it (_jobs);
    Job * job;
    while (it.Next(job)) {
        if (job->GetStatus() == Job::STATUS_ERROR) {
            fprintf (fp, "%s,", job->GetJobName());
        }
    }
    fprintf (fp, "<ENDLIST>\n\n");

    //
    // Print JOBS and SCRIPTS
    //
    it.ToBeforeFirst();
    while (it.Next(job)) {
        if( job->JobType() == Job::TYPE_CONDOR ) {
            fprintf (fp, "JOB %s %s %s\n", 
                     job->GetJobName(), job->GetCmdFile(),
                     job->_Status == Job::STATUS_DONE ? "DONE" : "");
        }
        else if( job->JobType() == Job::TYPE_STORK ) {
            fprintf (fp, "DAP %s %s %s\n", 
                     job->GetJobName(), job->GetCmdFile(),
                     job->_Status == Job::STATUS_DONE ? "DONE" : "");
        }
        if (job->_scriptPre != NULL) {
            fprintf (fp, "SCRIPT PRE  %s %s\n", 
                     job->GetJobName(), job->_scriptPre->GetCmd());
        }
        if (job->_scriptPost != NULL) {
            fprintf (fp, "SCRIPT POST %s %s\n", 
                     job->GetJobName(), job->_scriptPost->GetCmd());
        }
        if( job->retry_max > 0 ) {
            int retries = (job->retry_max - job->retries);

            if (   job->_Status == Job::STATUS_ERROR
                && job->retries < job->retry_max 
                && job->have_retry_abort_val
                && job->retval == job->retry_abort_val ) {
                fprintf(fp, "# %d of %d retries performed; remaining attempts "
                        "aborted after node returned %d\n", 
                        job->retries, job->retry_max, job->retval );
            } else {
                fprintf( fp,
                         "# %d of %d retries already performed; %d remaining\n",
                         job->retries, job->retry_max, retries );
            }
            
            ASSERT( job->retries <= job->retry_max );
            // print (job->retry_max - job->retries) so that
            // job->retries isn't reset upon recovery
            fprintf( fp, "RETRY %s %d", job->GetJobName(), retries );
            if( job->have_retry_abort_val ) {
                fprintf( fp, " UNLESS-EXIT %d", job->retry_abort_val );
            }
            fprintf( fp, "\n" );
            fprintf( fp, "\n" );
        }
        fprintf( fp, "\n" );

        if(!job->varNamesFromDag->IsEmpty()) {
            fprintf(fp, "VARS %s", job->GetJobName());
	
            ListIterator<MyString> names(*job->varNamesFromDag);
            ListIterator<MyString> vals(*job->varValsFromDag);
            names.ToBeforeFirst();
            vals.ToBeforeFirst();
            MyString *strName, *strVal;
            while((strName = names.Next()) && (strVal = vals.Next())) {
                fprintf(fp, " %s=\"", strName->Value());
                // now we print the value, but we have to re-escape certain characters
                for(int i = 0; i < strVal->Length(); i++) {
                    char c = (*strVal)[i];
                    if(c == '\"')
                        fprintf(fp, "\\\"");
                    else if(c == '\\')
                        fprintf(fp, "\\\\");
                    else
                        fprintf(fp, "%c", c);
                }
                fprintf(fp, "\"");
            }
            fprintf(fp, "\n");
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
                ASSERT( child != NULL );
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
Dag::TerminateJob( Job* job, bool recovery )
{
    ASSERT( job != NULL );

	job->TerminateSuccess();
    ASSERT( job->GetStatus() == Job::STATUS_DONE );

    //
    // Report termination to all child jobs by removing parent's ID from
    // each child's waiting queue.
    //
    SimpleList<JobID_t> & qp = job->GetQueueRef(Job::Q_CHILDREN);
    SimpleListIterator<JobID_t> iList (qp);
    JobID_t childID;
    while (iList.Next(childID)) {
        Job * child = GetJob(childID);
        ASSERT( child != NULL );
        child->Remove(Job::Q_WAITING, job->GetJobID());
		// if child has no more parents in its waiting queue, submit it
		if( child->GetStatus() == Job::STATUS_READY &&
			child->IsEmpty( Job::Q_WAITING ) && recovery == FALSE ) {
			StartNode( child );
		}
    }
		// this is a little ugly, but since this function can be
		// called multiple times for the same job, we need to be
		// careful not to double-count...
	if( job->countedAsDone == false ) {
		_numJobsDone++;
		job->countedAsDone = true;
		ASSERT( _numJobsDone <= _jobs.Number() );
	}
}

void Dag::
PrintEvent( debug_level_t level, const char* eventName, Job* job,
			CondorID condorID )
{
	if( job ) {
	    debug_printf( level, "Event: %s for %s Job %s (%d.%d)\n", eventName,
					  job->JobTypeString(), job->GetJobName(),
					  job->_CondorID._cluster, job->_CondorID._proc );
	} else {
        debug_printf( level, "Event: %s for Unknown Job (%d.%d): "
					  "ignoring...\n", eventName, condorID._cluster,
					  condorID._proc );
	}
	return;
}

void
Dag::RestartNode( Job *node, bool recovery )
{
    ASSERT( node != NULL );
    ASSERT( node->_Status == Job::STATUS_ERROR );
    if( node->have_retry_abort_val && node->retval == node->retry_abort_val ) {
        debug_printf( DEBUG_NORMAL, "Aborting further retries of node %s "
                      "(last attempt returned %d)\n",
                      node->GetJobName(), node->retval);
        _numJobsFailed++;
        return;
    }
	node->_Status = Job::STATUS_READY;
	node->retries++;
	ASSERT( node->GetRetries() <= node->GetRetryMax() );
	if( node->_scriptPre ) {
		// undo PRE script completion
		node->_scriptPre->_done = false;
	}
	strcpy( node->error_text, "" );
	debug_printf( DEBUG_VERBOSE, "Retrying node %s (retry #%d of %d)...\n",
				  node->GetJobName(), node->GetRetries(),
				  node->GetRetryMax() );
	if( !recovery ) {
		StartNode( node );
	}
}


// Number the nodes according to DFS order 
void 
Dag::DFSVisit (Job * job)
{
	//Check whether job has been numbered already
	if (job==NULL || job->_visited)
		return;
	
	//Remember that the job has been numbered	
	job->_visited = true; 
	
	//Get the children of current job	
	SimpleList <JobID_t> & children = job->GetQueueRef(Job::Q_CHILDREN);
	SimpleListIterator <JobID_t> child_itr (children); 
	
	JobID_t childID;
	child_itr.ToBeforeFirst();
	
	while (child_itr.Next(childID))
	{
		Job * child = GetJob (childID);
		DFSVisit (child);
	}

	DFS_ORDER++;
	job->_dfsOrder = DFS_ORDER;
}		

// Detects cycle and warns user about it
bool 
Dag::isCycle ()
{
	bool cycle = false; 
	Job * job;
	JobID_t childID;
	ListIterator <Job> joblist (_jobs);
	SimpleListIterator <JobID_t> child_list;

	//Start DFS numbering from zero, although not necessary
	DFS_ORDER = 0; 
	
	//Visit all jobs in DAG and number them	
	joblist.ToBeforeFirst();	
	while (joblist.Next(job))
	{
  		if (!job->_visited &&
			job->GetQueueRef(Job::Q_PARENTS).Number()==0)
			DFSVisit (job);	
	}	

	//Detect cycle
	joblist.ToBeforeFirst();	
	while (joblist.Next(job))
	{
		child_list.Initialize(job->GetQueueRef(Job::Q_CHILDREN));
		child_list.ToBeforeFirst();
		while (child_list.Next(childID))
		{
			Job * child = GetJob (childID);

			//No child's DFS order should be smaller than parent's
			if (child->_dfsOrder >= job->_dfsOrder) {
#ifdef REPORT_CYCLE	
				debug_printf (DEBUG_QUIET, 
							  "Cycle in the graph possibly involving jobs %s and %s\n",
							  job->GetJobName(), child->GetJobName());
#endif 			
				cycle = true;
			}
		}		
	}
	return cycle;
}

//===========================================================================
// Methods for Dot Files, both public and private
//===========================================================================


//-------------------------------------------------------------------------
// 
// Function: SetDotFileName
// Purpose:  Sets the base name of the dot file name. If we aren't 
//           overwriting the file, it will have a number appended to it.
// Scope:    Public
//
//-------------------------------------------------------------------------
void 
Dag::SetDotFileName(char *dot_file_name)
{
	delete[] _dot_file_name;
	_dot_file_name = strnewp(dot_file_name);
	return;
}

//-------------------------------------------------------------------------
// 
// Function: SetDotIncludeFileName
// Purpose:  Sets the name of a file that we'll take the contents from and
//           place them into the dot file. The idea is that if someone wants
//           to set some parameters for the graph but doesn't want to manually
//           edit the file (perhaps a higher-level program is watching the 
//           dot file) then we'll get the parameters from here.
// Scope:    Public
//
//-------------------------------------------------------------------------
void
Dag::SetDotIncludeFileName(char *include_file_name)
{
	delete[] _dot_include_file_name;
	_dot_include_file_name = strnewp(include_file_name);
	return;
}

//-------------------------------------------------------------------------
// 
// Function: DumpDotFile
// Purpose:  Called whenever the dot file should be created. 
//           This writes to the _dot_file_name, although the name may have
//           a numeric suffix if we're not overwriting files. 
// Scope:    Public
//
//-------------------------------------------------------------------------
void 
Dag::DumpDotFile(void)
{
	if (_dot_file_name != NULL) {
		MyString  current_dot_file_name;
		MyString  temp_dot_file_name;
		FILE      *temp_dot_file;

		ChooseDotFileName(current_dot_file_name);

		temp_dot_file_name = current_dot_file_name + ".temp";

		unlink(temp_dot_file_name.Value());
		temp_dot_file = fopen(temp_dot_file_name.Value(), "w");
		if (temp_dot_file == NULL) {
			debug_dprintf(D_ALWAYS, DEBUG_NORMAL,
						  "Can't create dot file '%s'\n", 
						  temp_dot_file_name.Value());
		} else {
			time_t current_time;
			char   *time_string;
			char   *newline;

			time(&current_time);
			time_string = ctime(&current_time);
			newline = strchr(time_string, '\n');
			if (newline != NULL) {
				*newline = 0;
			}
			
			fprintf(temp_dot_file, "digraph DAG {\n");
			fprintf(temp_dot_file, "    label=\"DAGMan Job %s status at %s\";\n\n",
					DAGManJobId, time_string);

			IncludeExtraDotCommands(temp_dot_file);

			// We sweep the job list twice, just to make a nice
			// looking DOT file. 
			// The first sweep sets the appearance of each node.
			// The second sweep describes all of the arcs.

			DumpDotFileNodes(temp_dot_file);
			DumpDotFileArcs(temp_dot_file);

			fprintf(temp_dot_file, "}\n");
			fclose(temp_dot_file);
			unlink(current_dot_file_name.Value());
			rename(temp_dot_file_name.Value(), current_dot_file_name.Value());
		}
	}
	return;
}

//-------------------------------------------------------------------------
// 
// Function: CheckAllJobs
// Purpose:  Called after the DAG has finished to check for any
//           inconsistency in the job events we've gotten.
// Scope:    Public
//
//-------------------------------------------------------------------------
void
Dag::CheckAllJobs(const Dagman &dm)
{
	if ( dm.doEventChecks ) {
		MyString	jobError;
		if ( !_ce.CheckAllJobs(jobError) ) {
			debug_printf( DEBUG_VERBOSE, "Error checking job events: %s\n",
					jobError.Value() );
			ASSERT( false );
		} else {
			debug_printf( DEBUG_DEBUG_1, "All job events okay\n");
		}
	}
}

//-------------------------------------------------------------------------
// 
// Function: IncludeExtraDotCommands
// Purpose:  Helper function for DumpDotFile. Reads the _dot_include_file_name 
//           file and places everything in it into the dot file that is being 
//           written. 
// Scope:    Private
//
//-------------------------------------------------------------------------
void
Dag::IncludeExtraDotCommands(
	FILE *dot_file)
{
	FILE *include_file;

	include_file = fopen(_dot_include_file_name, "r");
	if (include_file == NULL) {
        debug_printf(DEBUG_NORMAL, "Can't open %s\n", _dot_include_file_name);
	} else {
		char line[100];
		fprintf(dot_file, "// Beginning of commands included from %s.\n", 
				_dot_include_file_name);
		while (fgets(line, 100, include_file) != NULL) {
			fputs(line, dot_file);
		}
		fprintf(dot_file, "// End of commands included from %s.\n\n", 
				_dot_include_file_name);
	}
	return;
}

//-------------------------------------------------------------------------
// 
// Function: DumpDotFileNodes
// Purpose:  Helper function for DumpDotFile. Prints one line for each 
//           node in the graph. This line describes how the node should
//           be drawn, and it's based on the state of the node. In particular,
//           we draw different shapes for each node type, but also add
//           a short description of the state, to make it easily readable by
//           people that are familiar with Condor job states. 
// Scope:    Private
//
//-------------------------------------------------------------------------
void 
Dag::DumpDotFileNodes(FILE *temp_dot_file)
{
	Job                 *node;
	ListIterator <Job>  joblist (_jobs);
	
	joblist.ToBeforeFirst();	
	while (joblist.Next(node)) {
		const char *node_name;
		
		node_name = node->GetJobName();
		switch (node->GetStatus()) {
		case Job::STATUS_READY:
			fprintf(temp_dot_file, 
				   "    %s [shape=ellipse label=\"%s (I)\"];\n",
				   node_name, node_name);
			break;
		case Job::STATUS_PRERUN:
			fprintf(temp_dot_file, 
				   "    %s [shape=ellipse label=\"%s (Pre)\" style=dotted];\n",
				   node_name, node_name);
			break;
		case Job::STATUS_SUBMITTED:
			fprintf(temp_dot_file, 
				   "    %s [shape=ellipse label=\"%s (R)\" peripheries=2];\n",
				   node_name, node_name);
			break;
		case Job::STATUS_POSTRUN:
			fprintf(temp_dot_file, 
				   "    %s [shape=ellipse label=\"%s (Post)\" style=dotted];\n",
				   node_name, node_name);
			break;
		case Job::STATUS_DONE:
			fprintf(temp_dot_file, 
				   "    %s [shape=ellipse label=\"%s (Done)\" style=bold];\n",
				   node_name, node_name);
			break;
		case Job::STATUS_ERROR:
			fprintf(temp_dot_file, 
				   "    %s [shape=box label=\"%s (E)\"];\n",
				   node_name, node_name);
			break;
		default:
			fprintf(temp_dot_file, 
				   "    %s [shape=ellipse label=\"%s (I)\"];\n",
				   node_name, node_name);
			break;
		}
	}

	fprintf(temp_dot_file, "\n");

	return;
}

//-------------------------------------------------------------------------
// 
// Function: DumpDotFileArcs
// Purpose:  Helper function for DumpDotFile. This prints one line for each
//           arc that is in the DAG.
// Scope:    Private
//
//-------------------------------------------------------------------------
void 
Dag::DumpDotFileArcs(FILE *temp_dot_file)
{
	Job                          *parent;
	ListIterator <Job>           joblist (_jobs);

	joblist.ToBeforeFirst();	
	while (joblist.Next(parent)) {
		Job        *child;
		JobID_t                      childID;
		SimpleListIterator <JobID_t> child_list;
		const char                   *parent_name;
		const char                   *child_name;
		
		parent_name = parent->GetJobName();
		
		child_list.Initialize(parent->GetQueueRef(Job::Q_CHILDREN));
		child_list.ToBeforeFirst();
		while (child_list.Next(childID)) {
			
			child = GetJob (childID);
			
			child_name  = child->GetJobName();
			if (parent_name != NULL && child_name != NULL) {
				fprintf(temp_dot_file, "    %s -> %s;\n",
						parent_name, child_name);
			}
		}
	}
	
	return;
}

//-------------------------------------------------------------------------
// 
// Function: ChooseDotFileName
// Purpose:  If we're overwriting the dot file, the name is exactly as
//           the user specified it. If we're not overwriting, we need to 
//           choose a unique name. We choose one of the form name.number, 
//           where the number is the first unused we find. If the user 
//           creates a bunch of dot files (foo.0-foo.100) and deletes
//           some in the middle (foo.50), then this could confuse the user
//           because we'll fill in the middle. We don't worry about it though.
// Scope:    Private
//
//-------------------------------------------------------------------------
void 
Dag::ChooseDotFileName(MyString &dot_file_name)
{
	if (_overwrite_dot_file) {
		dot_file_name = _dot_file_name;
	} else {
		bool found_unused_file = false;

		while (!found_unused_file) {
			FILE *fp;

			dot_file_name.sprintf("%s.%d", _dot_file_name, _dot_file_name_suffix);
			fp = fopen(dot_file_name.Value(), "r");
			if (fp != NULL) {
				fclose(fp);
				_dot_file_name_suffix++;
			} else {
				found_unused_file = true;
			}
		}
		_dot_file_name_suffix++;
	}
	return;
}

bool Dag::Add( Job& job )
{
	return _jobs.Append(job);
}


bool
Dag::RemoveNode( const char *name, MyString &whynot )
{
	if( !name ) {
		whynot = "name == NULL";
		return false;
	}
	Job *node = GetJob( name );
	if( !node ) {
		whynot = "does not exist in DAG";
		return false;
	}
	if( node->IsActive() ) {
		whynot.sprintf( "is active (%s)", node->GetStatusName() );
		return false;
	}
	if( !node->IsEmpty( Job::Q_CHILDREN ) ||
		!node->IsEmpty( Job::Q_PARENTS ) ) {
		whynot.sprintf( "dependencies exist" );
		return false;
	}

		// now we know it's okay to remove, and can do the deed...

	Job* candidate = NULL;
	bool removed = false;

	if( node->GetStatus() == Job::STATUS_DONE ) {
		_numJobsDone--;
		ASSERT( _numJobsDone >= 0 );
	}
	else if( node->GetStatus() == Job::STATUS_ERROR ) {
		_numJobsFailed--;
		ASSERT( _numJobsFailed >= 0 );
	}
	else if( node->GetStatus() == Job::STATUS_READY ) {
		ASSERT( _readyQ );

		if( _readyQ->IsMember( node ) ) {

				// remove node from ready queue

			debug_printf( DEBUG_VERBOSE, "=== Ready Queue (Before) ===" );
			PrintReadyQ( DEBUG_VERBOSE );

			removed = false;
			_readyQ->Rewind();
			while( _readyQ->Next( candidate ) ) {
				ASSERT( candidate );
				if( candidate == node ) {
					_readyQ->DeleteCurrent();
					removed = true;
					continue;
				}
			}
			ASSERT( removed );
			ASSERT( !_readyQ->IsMember( node ) );
			debug_printf( DEBUG_VERBOSE, "=== Ready Queue (After) ===" );
			PrintReadyQ( DEBUG_VERBOSE );
		}
	}
	else {
			// this should never happen, unless we add a new state and
			// fail to handle it above...
		debug_printf( DEBUG_QUIET, "ERROR: node %s in unexpected state (%s)\n",
					  node->GetJobName(), node->GetStatusName() );
		whynot.sprintf( "node in unexpected state (%s)",
						node->GetStatusName() );
		return false;
	}

		// remove node from the DAG
	removed = false;
	_jobs.Rewind();
	while( _jobs.Next( candidate ) ) {
		ASSERT( candidate );
        if( candidate == node ) {
			_jobs.DeleteCurrent();
			removed = true;
			continue;
		}
	}
		// we know the node is in _jobs (since we looked it up via
		// GetJob() above), and DeleteCurrent() can't fail (!), so
		// there should be no way for us to get through the above loop
		// without having seen & removed the node... also, we can't
		// safely just return false here and let a higher level handle
		// the error, since the node is already half-removed and thus
		// the DAG state is FUBAR...
	ASSERT( removed == true );

	whynot = "n/a";
	return true;
}


bool
Dag::RemoveDependency( Job *parent, Job *child )
{
	MyString whynot;
	return RemoveDependency( parent, child, whynot );
}

bool
Dag::RemoveDependency( Job *parent, Job *child, MyString &whynot )
{
	if( !parent ) {
		whynot = "parent == NULL";
		return false;
	}
	if( !child ) {
		whynot = "child == NULL";
		return false;
	}

	if( !parent->HasChild( child ) ) {
		whynot = "no such dependency";
		return false;
	}
	ASSERT( child->HasParent( parent ) );

		// remove the child from the parent's children...
	if( !parent->RemoveChild( child, whynot ) ) {
		return false;
	}
	ASSERT( parent->HasChild( child ) == false );

		// remove the parent from the child's parents...
	if( !child->RemoveParent( parent, whynot ) ) {
			// the dependencies are now asymetric and thus the DAG
			// state is FUBAR, so we should bail...
		ASSERT( false );
		return false;
	}
	ASSERT( child->HasParent( parent ) == false );

	whynot = "n/a";
    return true;
}


// NOTE: dag addnode/removenode/adddep/removedep methods don't
// necessarily insure internal consistency...that's currently up to
// the higher level calling them to get right...
