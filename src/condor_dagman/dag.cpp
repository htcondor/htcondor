/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


//
// Terminology note:
// We are calling a *node* the combination of pre-script, job, and
// post-script.
// We are calling a *job* essentially what results from one invocation
// of condor_submit or stork_submit.
// We are calling a *job proc* an individual batch system process within
// a cluster.
// So nodes-to-jobs is 1-to-1; jobs to job procs is 1-to-n.
// wenger 2006-02-14.

//
// Local DAGMan includes
//
#include "condor_common.h"
#include "condor_attributes.h"
#include "dag.h"
#include "debug.h"
#include "submit.h"
#include "util.h"
#include "dagman_main.h"
#include "dagman_multi_dag.h"
#include "write_user_log.h"
#include "simplelist.h"
#include "condor_string.h"  /* for strnewp() */
#include "string_list.h"
#include "condor_daemon_core.h"
#include "extArray.h"
#include "HashTable.h"
#include <set>

using namespace std;

const CondorID Dag::_defaultCondorId;

const int Dag::DAG_ERROR_CONDOR_SUBMIT_FAILED = -1001;
const int Dag::DAG_ERROR_CONDOR_JOB_ABORTED = -1002;
const int Dag::DAG_ERROR_LOG_MONITOR_ERROR = -1003;

//---------------------------------------------------------------------------
void touch (const char * filename) {
    int fd = safe_open_wrapper(filename, O_RDWR | O_CREAT, 0600);
    if (fd == -1) {
        debug_error( 1, DEBUG_QUIET, "Error: can't open %s\n", filename );
    }
    close (fd);
}

static const int NODE_HASH_SIZE = 10007; // prime, allow for big DAG...

//---------------------------------------------------------------------------
Dag::Dag( /* const */ StringList &dagFiles,
		  const int maxJobsSubmitted,
		  const int maxPreScripts, const int maxPostScripts,
		  bool allowLogError,
		  bool useDagDir, int maxIdleJobProcs, bool retrySubmitFirst,
		  bool retryNodeFirst, const char *condorRmExe,
		  const char *storkRmExe, const CondorID *DAGManJobID,
		  bool prohibitMultiJobs, bool submitDepthFirst,
		  const char *defaultNodeLog, bool isSplice) :
    _maxPreScripts        (maxPreScripts),
    _maxPostScripts       (maxPostScripts),
	MAX_SIGNAL			  (64),
	_splices              (200, hashFuncMyString, rejectDuplicateKeys),
	_dagFiles             (dagFiles),
	_useDagDir            (useDagDir),
	_nodeNameHash		  (NODE_HASH_SIZE, MyStringHash, rejectDuplicateKeys),
	_nodeIDHash			  (NODE_HASH_SIZE, hashFuncInt, rejectDuplicateKeys),
	_condorIDHash		  (NODE_HASH_SIZE, hashFuncInt, rejectDuplicateKeys),
	_storkIDHash		  (NODE_HASH_SIZE, hashFuncInt, rejectDuplicateKeys),
    _numNodesDone         (0),
    _numNodesFailed       (0),
    _numJobsSubmitted     (0),
    _maxJobsSubmitted     (maxJobsSubmitted),
	_numIdleJobProcs		  (0),
	_maxIdleJobProcs		  (maxIdleJobProcs),
	_allowLogError		  (allowLogError),
	m_retrySubmitFirst	  (retrySubmitFirst),
	m_retryNodeFirst	  (retryNodeFirst),
	_condorRmExe		  (condorRmExe),
	_storkRmExe			  (storkRmExe),
	_DAGManJobId		  (DAGManJobID),
	_preRunNodeCount	  (0),
	_postRunNodeCount	  (0),
	_checkCondorEvents    (),
	_checkStorkEvents     (),
	_maxJobsDeferredCount (0),
	_maxIdleDeferredCount (0),
	_catThrottleDeferredCount (0),
	_prohibitMultiJobs	  (prohibitMultiJobs),
	_submitDepthFirst	  (submitDepthFirst),
	_defaultNodeLog		  (defaultNodeLog),
	_isSplice			  (isSplice)
{

	// If this dag is a splice, then it may have been specified with a DIR
	// directive. If so, then this records what it was so we can later
	// propogate this information to all contained nodes in the DAG--effectively
	// giving all nodes in the DAG a DIR definition with this directory
	// as a prefix to what is already there (except in the case of absolute
	// paths, in which case nothing is done).
	m_directory = ".";

	// for the toplevel dag, emit the dag files we ended up using.
	if (_isSplice == false) {
		ASSERT( dagFiles.number() >= 1 );
		PrintDagFiles( dagFiles );
	}

 	_readyQ = new PrioritySimpleList<Job*>;
	_submitQ = new Queue<Job*>;
	if( !_readyQ || !_submitQ ) {
		EXCEPT( "ERROR: out of memory (%s:%d)!\n", __FILE__, __LINE__ );
	}

	/* The ScriptQ object allocates daemoncore reapers, which are a
		regulated and precious resource. Since we *know* we will never need
		this object when we are parsing a splice, we don't allocate it.
		In the other codes that expect this pointer to be valid, there is
		either an ASSERT or other checks to ensure it is valid. */
	if (_isSplice == false) {
		_preScriptQ = new ScriptQ( this );
		_postScriptQ = new ScriptQ( this );
		if( !_preScriptQ || !_postScriptQ ) {
			EXCEPT( "ERROR: out of memory (%s:%d)!\n", __FILE__, __LINE__ );
		}
	} else {
		_preScriptQ = NULL;
		_postScriptQ = NULL;
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
	_nextSubmitTime = 0;
	_nextSubmitDelay = 1;
	_recovery = false;
	_abortOnScarySubmit = true;
	_configFile = NULL;
		
		// Don't print any waiting node reports until we're done with
		// recovery mode.
	_pendingReportInterval = -1;

	_nfsLogIsError = param_boolean( "DAGMAN_LOG_ON_NFS_IS_ERROR", true );

	return;
}

//-------------------------------------------------------------------------
Dag::~Dag() {
		// remember kids, delete is safe *even* if ptr == NULL...

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
bool Dag::Bootstrap (bool recovery)
{
    Job* job;
    ListIterator<Job> jobs (_jobs);

	// This function should never be called on a dag object which is acting
	// like a splice.
	ASSERT( _isSplice == false );

    // update dependencies for pre-completed jobs (jobs marked DONE in
    // the DAG input file)
    jobs.ToBeforeFirst();
    while( jobs.Next( job ) ) {
		if( job->GetStatus() == Job::STATUS_DONE ) {
			TerminateJob( job, false, true );
		}
    }
    debug_printf( DEBUG_VERBOSE, "Number of pre-completed nodes: %d\n",
				  NumNodesDone() );
    
    if (recovery) {
		_recovery = true;

        debug_printf( DEBUG_NORMAL, "Running in RECOVERY mode... "
					">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n" );

		// as we read the event log files, we emit lots of imformation into
		// the logfile. If this is on NFS, then we pay a *very* large price
		// for the open/close of each line in the log file.
		// Whether or not this caching is honored is dependant on if the
		// cache was originally enabled or not. If the cache is not
		// enabled, then debug_cache_start_caching() and 
		// debug_cache_stop_caching() are effectively noops.

		debug_cache_start_caching();

			// Here we're monitoring the log files of all ready nodes.
   		jobs.ToBeforeFirst();
   		while( jobs.Next( job ) ) {
			if ( job->CanSubmit() ) {
				if ( !job->MonitorLogFile( _condorLogRdr, _storkLogRdr,
							_nfsLogIsError, recovery, _defaultNodeLog ) ) {
					debug_cache_stop_caching();
					return false;
				}
			}
		}

			// Note: I just realized that this will almost certainly fail
			// on a combination of Condor and Stork events -- we probably
			// need a loop around the event processing.  wenger 2009-06-18.
		if( CondorLogFileCount() > 0 ) {
			if( !ProcessLogEvents( CONDORLOG, recovery ) ) {
				_recovery = false;
				debug_cache_stop_caching();
				return false;
			}
		}
		if( StorkLogFileCount() > 0 ) {
			if( !ProcessLogEvents( DAPLOG, recovery ) ) {
				_recovery = false;
				debug_cache_stop_caching();
				return false;
			}
		}

		// all jobs stuck in STATUS_POSTRUN need their scripts run
		jobs.ToBeforeFirst();
		while( jobs.Next( job ) ) {
			if( job->GetStatus() == Job::STATUS_POSTRUN ) {
				if ( !job->MonitorLogFile( _condorLogRdr, _storkLogRdr,
							_nfsLogIsError, _recovery, _defaultNodeLog ) ) {
					debug_cache_stop_caching();
					return false;
				}
				_postScriptQ->Run( job->_scriptPost );
			}
		}

		debug_cache_stop_caching();

        debug_printf( DEBUG_NORMAL, "...done with RECOVERY mode "
					"<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n" );
		print_status();

		_recovery = false;
    }
	
    if( DEBUG_LEVEL( DEBUG_DEBUG_2 ) ) {
		PrintJobList();
		PrintReadyQ( DEBUG_DEBUG_2 );
    }	
    
		// Note: we're bypassing the ready queue here...
    jobs.ToBeforeFirst();
    while( jobs.Next( job ) ) {
		if( job->GetStatus() == Job::STATUS_READY &&
			job->IsEmpty( Job::Q_WAITING ) ) {
			StartNode( job, false );
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
	debug_printf( DEBUG_DEBUG_2, "Dag::AddDependency(%s, %s)\n",
				parent->GetJobName(), child->GetJobName());
	if( !parent->AddChild( child ) ) {
		return false;
	}
	if( !child->AddParent( parent ) ) {
			// reverse earlier successful AddChild() so we don't end
			// up with asymetric dependencies
		if( !parent->RemoveChild( child ) ) {
				// the DAG state is FUBAR, so we should bail...
			EXCEPT( "Fatal error attempting to add dependency between "
						"%s (parent) and %s (child)",
						parent->GetJobName(), child->GetJobName() );
		}
		return false;
	}
    return true;
}

//-------------------------------------------------------------------------
Job * Dag::FindNodeByNodeID (const JobID_t jobID) const {
	Job *	job = NULL;
	if ( _nodeIDHash.lookup(jobID, job) != 0 ) {
    	debug_printf( DEBUG_NORMAL, "ERROR: job %d not found!\n", jobID);
		job = NULL;
	}

	if ( job ) {
		if ( jobID != job->GetJobID() ) {
			EXCEPT( "Searched for node ID %d; got %d!!", jobID,
						job->GetJobID() );
		}
	}

	return job;
}

//-------------------------------------------------------------------------
bool
Dag::DetectCondorLogGrowth () {

	if( CondorLogFileCount() <= 0 ) {
		return false;
	}

	bool growth = _condorLogRdr.detectLogGrowth();
    debug_printf( DEBUG_DEBUG_4, "%s\n",
				  growth ? "Log GREW!" : "No log growth..." );

		//
		// Print the nodes we're waiting for if the time threshold
		// since the last event has been exceeded, and we also haven't
		// printed a report in that time.
		//
	time_t		currentTime;
	time( &currentTime );

	if ( growth ) _lastEventTime = currentTime;
	time_t		elapsedEventTime = currentTime - _lastEventTime;
	time_t		elapsedPrintTime = currentTime - _lastPendingNodePrintTime;

	if ( (int)elapsedEventTime >= _pendingReportInterval &&
				(int)elapsedPrintTime >= _pendingReportInterval ) {
		debug_printf( DEBUG_NORMAL, "%d seconds since last log event\n",
					(int)elapsedEventTime );
		PrintPendingNodes();

		_lastPendingNodePrintTime = currentTime;
	}

    return growth;
}

//-------------------------------------------------------------------------
bool Dag::DetectDaPLogGrowth () {

	if( StorkLogFileCount() <= 0 ) {
		return false;
	}

	bool growth = _storkLogRdr.detectLogGrowth();
    debug_printf( DEBUG_DEBUG_4, "%s\n",
				  growth ? "Log GREW!" : "No log growth..." );
    return growth;
}

//-------------------------------------------------------------------------
// Developer's Note: returning false tells main_timer to abort the DAG
bool Dag::ProcessLogEvents (int logsource, bool recovery) {

	if ( logsource == CONDORLOG ) {
		debug_printf( DEBUG_VERBOSE, "Currently monitoring %d Condor "
					"log file(s)\n", _condorLogRdr.activeLogFileCount() );
	} else if ( logsource == DAPLOG ) {
		debug_printf( DEBUG_VERBOSE, "Currently monitoring %d Stork "
					"log file(s)\n", _storkLogRdr.activeLogFileCount() );
	}

	bool done = false;  // Keep scanning until ULOG_NO_EVENT
	bool result = true;

	while (!done) {
		ULogEvent* e = NULL;
		ULogEventOutcome outcome = ULOG_NO_EVENT;

		if ( logsource == CONDORLOG ) {
			outcome = _condorLogRdr.readEvent(e);
		} else if ( logsource == DAPLOG ){
			outcome = _storkLogRdr.readEvent(e);
		}

		bool tmpResult = ProcessOneEvent( logsource, outcome, e, recovery,
					done );
			// If ProcessOneEvent returns false, the result here must
			// be false.
		result = result && tmpResult;

		if( e != NULL ) {
			// event allocated earlier by _condorLogRdr.readEvent()
			delete e;
		}
	}

	if (DEBUG_LEVEL(DEBUG_VERBOSE) && recovery) {
		const char *name = (logsource == CONDORLOG) ? "Condor" : "Stork";
		debug_printf( DEBUG_NORMAL, "    ------------------------------\n");
		debug_printf( DEBUG_NORMAL, "       %s Recovery Complete\n", name);
		debug_printf( DEBUG_NORMAL, "    ------------------------------\n");
	}

	return result;
}

//---------------------------------------------------------------------------
// Developer's Note: returning false tells main_timer to abort the DAG
bool Dag::ProcessOneEvent (int logsource, ULogEventOutcome outcome,
		const ULogEvent *event, bool recovery, bool &done) {

	bool result = true;

	static int log_unk_count = 0;
	static int ulog_rd_error_count = 0;
	
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
		if ( ++ulog_rd_error_count >= 10 ) {
			debug_printf( DEBUG_QUIET, "ERROR: repeated (%d) failures to "
						"read job log; quitting...\n",
						ulog_rd_error_count );
			result = false;
		} else {
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
		{
			ASSERT( event != NULL );
			bool submitEventIsSane;
			Job *job = LogEventNodeLookup( logsource, event,
						submitEventIsSane );
			PrintEvent( DEBUG_VERBOSE, event, job );
			if( !job ) {
					// event is for a job outside this DAG; ignore it
				break;
			}
			if( !EventSanityCheck( logsource, event, job, &result ) ) {
					// this event is "impossible"; we will either
					// abort the DAG (if result was set to false) or
					// ignore it and hope for the best...
				break;
			} 

			switch(event->eventNumber) {

			case ULOG_EXECUTABLE_ERROR:
					// Don't do anything here, because we seem to always
					// also get an ABORTED event when we get an
					// EXECUTABLE_ERROR event.  (Not doing anything
					// here fixes Gnats PR 697.)
				break;

			case ULOG_JOB_ABORTED:
				ProcessAbortEvent(event, job, recovery);
					// Make sure we don't count finished jobs as idle.
				ProcessNotIdleEvent(job);
				break;
              
			case ULOG_JOB_TERMINATED:
				ProcessTerminatedEvent(event, job, recovery);
					// Make sure we don't count finished jobs as idle.
				ProcessNotIdleEvent(job);
				break;

			case ULOG_POST_SCRIPT_TERMINATED:
				ProcessPostTermEvent(event, job, recovery);
				break;

			case ULOG_SUBMIT:
				ProcessSubmitEvent(job, recovery, submitEventIsSane);
				ProcessIsIdleEvent(job);
				break;

			case ULOG_JOB_EVICTED:
			case ULOG_JOB_SUSPENDED:
			case ULOG_JOB_HELD:
			case ULOG_SHADOW_EXCEPTION:
				ProcessIsIdleEvent(job);
				break;

			case ULOG_EXECUTE:
				ProcessNotIdleEvent(job);
				break;

			case ULOG_JOB_UNSUSPENDED:
			case ULOG_JOB_RELEASED:
			case ULOG_CHECKPOINTED:
			case ULOG_IMAGE_SIZE:
			case ULOG_NODE_EXECUTE:
			case ULOG_NODE_TERMINATED:
			case ULOG_GENERIC:
			case ULOG_JOB_AD_INFORMATION:
			default:
				break;
			}
		}
		break;

	default:
		debug_printf( DEBUG_QUIET, "ERROR: illegal ULogEventOutcome "
					"value (%d)!!!\n", outcome);
		break;
	}

	return result;
}


//---------------------------------------------------------------------------
void
Dag::ProcessAbortEvent(const ULogEvent *event, Job *job,
		bool recovery) {

  // NOTE: while there are known Condor bugs leading to a "double"
  // terminate-then-abort event pair for the same job proc abortion,
  // this method will no longer actually see the second (abort) event,
  // because the event-checking code will suppress it.  Therefore, this
  // code no longer has to worry about it.  Note, though, that we can
  // still see a combination of terminated and aborted events for the
  // same *job* (not job proc).

	if ( job ) {
		DecrementJobCounts( job );

			// Only change the node status, error info,
			// etc., if we haven't already gotten an error
			// from another job proc in this job cluster
		if ( job->_Status != Job::STATUS_ERROR ) {
			job->_Status = Job::STATUS_ERROR;
			snprintf( job->error_text, JOB_ERROR_TEXT_MAXLEN,
				  "Condor reported %s event for job proc (%d.%d)",
				  ULogEventNumberNames[event->eventNumber],
				  event->cluster, event->proc );
			job->retval = DAG_ERROR_CONDOR_JOB_ABORTED;
			if ( job->_queuedNodeJobProcs > 0 ) {
			  // once one job proc fails, remove the whole cluster
				RemoveBatchJob( job );
			}
			if ( job->_scriptPost != NULL) {
					// let the script know the job's exit status
				job->_scriptPost->_retValJob = job->retval;
			}
		}

		ProcessJobProcEnd( job, recovery, true );
	}
}

//---------------------------------------------------------------------------
void
Dag::ProcessTerminatedEvent(const ULogEvent *event, Job *job,
		bool recovery) {
	if( job ) {
		DecrementJobCounts( job );

		const JobTerminatedEvent * termEvent =
					(const JobTerminatedEvent*) event;

		bool failed = !(termEvent->normal && termEvent->returnValue == 0);

		if( failed ) {
				// job failed or was killed by a signal

			if( termEvent->normal ) {
				debug_printf( DEBUG_QUIET, "Node %s job proc (%d.%d) "
						"failed with status %d.\n", job->GetJobName(),
						event->cluster, event->proc,
						termEvent->returnValue );
			} else {
				debug_printf( DEBUG_QUIET, "Node %s job proc (%d.%d) "
						"failed with signal %d.\n", job->GetJobName(),
						event->cluster, event->proc,
						termEvent->signalNumber );
			}

				// Only change the node status, error info, etc.,
				// if we haven't already gotten an error on this node.
			if ( job->_Status != Job::STATUS_ERROR ) {
				if( termEvent->normal ) {
					snprintf( job->error_text, JOB_ERROR_TEXT_MAXLEN,
							"Job proc (%d.%d) failed with status %d",
							termEvent->cluster, termEvent->proc,
							termEvent->returnValue );
					job->retval = termEvent->returnValue;
					if ( job->_scriptPost != NULL) {
							// let the script know the job's exit status
						job->_scriptPost->_retValJob = job->retval;
					}
				} else {
					snprintf( job->error_text, JOB_ERROR_TEXT_MAXLEN,
							"Job proc (%d.%d) failed with signal %d",
							termEvent->cluster, termEvent->proc,
							termEvent->signalNumber );
					job->retval = (0 - termEvent->signalNumber);
					if ( job->_scriptPost != NULL) {
							// let the script know the job's exit status
						job->_scriptPost->_retValJob = job->retval;
					}
				}

				job->_Status = Job::STATUS_ERROR;
				if ( job->_queuedNodeJobProcs > 0 ) {
				  // once one job proc fails, remove
				  // the whole cluster
					RemoveBatchJob( job );
				}
			}

		} else {
			// job succeeded
			ASSERT( termEvent->returnValue == 0 );

				// Only change the node status if we haven't already
				// gotten an error on this node.
			if ( job->_Status != Job::STATUS_ERROR ) {
				job->retval = 0;
				if ( job->_scriptPost != NULL) {
						// let the script know the job's exit status
					job->_scriptPost->_retValJob = job->retval;
				}
			}
			debug_printf( DEBUG_NORMAL,
							"Node %s job proc (%d.%d) completed "
							"successfully.\n", job->GetJobName(),
							termEvent->cluster, termEvent->proc );
		}

		if( job->_scriptPost == NULL ) {
			bool abort = CheckForDagAbort(job, "job");
			// if dag abort happened, we never return here!
			if( abort ) {
				return;
			}
		}

		ProcessJobProcEnd( job, recovery, failed );

		PrintReadyQ( DEBUG_DEBUG_2 );

		return;
	}
}

//---------------------------------------------------------------------------
void
Dag::RemoveBatchJob(Job *node) {

	ArgList args;
	MyString constraint;

	switch ( node->JobType() ) {
	case Job::TYPE_CONDOR:
		args.AppendArg( _condorRmExe );
		args.AppendArg( "-const" );

			// Adding this DAGMan's cluster ID as a constraint to
			// be extra-careful to avoid removing someone else's
			// job.
		constraint.sprintf( "%s == %d && %s == %d",
					ATTR_DAGMAN_JOB_ID, _DAGManJobId->_cluster,
					ATTR_CLUSTER_ID, node->_CondorID._cluster);
		args.AppendArg( constraint.Value() );
		break;

	case Job::TYPE_STORK:
		args.AppendArg( _storkRmExe );
		args.AppendArg( node->_CondorID._cluster );
		break;

	default:
		EXCEPT( "Illegal job (%d) type for node %s", node->JobType(),
					node->GetJobName() );
		break;
	}
	
	MyString display;
	args.GetArgsStringForDisplay( &display );
	debug_printf( DEBUG_VERBOSE, "Executing: %s\n", display.Value() );
	if ( util_popen( args ) ) {
			// Note: error here can't be fatal because there's a
			// race condition where you could do a condor_rm on
			// a job that already terminated.  wenger 2006-02-08.
		debug_printf( DEBUG_NORMAL, "Error removing DAG node jobs\n");
	}
}

//---------------------------------------------------------------------------
void
Dag::ProcessJobProcEnd(Job *job, bool recovery, bool failed) {

	// This function should never be called when the dag object is
	// being used to parse a splice.
	ASSERT ( _isSplice == false );

	if ( job->_queuedNodeJobProcs == 0 ) {
		(void)job->UnmonitorLogFile( _condorLogRdr, _storkLogRdr );
	}

	//
	// Note: structure here should be cleaned up, but I'm leaving it for
	// now to make sure parallel universe support is complete for 6.7.17.
	// wenger 2006-02-15.
	//

	if ( failed ) {
		if( job->_scriptPost != NULL ) {
			//
			// Fall thru and maybe run POST script below.
			//

		} else if( job->GetRetries() < job->GetRetryMax() ) {
			RestartNode( job, recovery );
			return;

		} else {
				// no more retries -- job failed
			if( job->GetRetryMax() > 0 ) {
					// add # of retries to error_text
				char *tmp = strnewp( job->error_text );
				snprintf( job->error_text, JOB_ERROR_TEXT_MAXLEN,
						"%s (after %d node retries)", tmp,
						job->GetRetries() );
				delete [] tmp;   
			}

			if ( job->_queuedNodeJobProcs == 0 ) {
				_numNodesFailed++;
			}

			return;
		}
	}

	if ( job->_queuedNodeJobProcs == 0 ) {
			// All procs for this job are done.
			debug_printf( DEBUG_NORMAL, "Node %s job completed\n",
						job->GetJobName() );

			// if a POST script is specified for the job, run it
		if (job->_scriptPost != NULL) {
			job->_Status = Job::STATUS_POSTRUN;
			_postRunNodeCount++;

			(void)job->MonitorLogFile( _condorLogRdr, _storkLogRdr,
						_nfsLogIsError, _recovery, _defaultNodeLog );
			if( !recovery ) {
				_postScriptQ->Run( job->_scriptPost );
			}
		}
			// no POST script was specified, so update DAG with
			// node's successful completion if the node succeeded.
		else if ( job->_Status != Job::STATUS_ERROR ) {
			TerminateJob( job, recovery );
		}
	}
}

//---------------------------------------------------------------------------
void
Dag::ProcessPostTermEvent(const ULogEvent *event, Job *job,
		bool recovery) {

	if( job ) {
			(void)job->UnmonitorLogFile( _condorLogRdr, _storkLogRdr );

			// Note: "|| recovery" below is somewhat of a "quick and dirty"
			// fix to Gnats PR 357.  The first part of the assert can fail
			// in recovery mode because if any retries of a node failed
			// because the post script failed, they don't show up in the
			// user log.  Therefore, condor_dagman doesn't know abou them,
			// and can think the post script was run before all retries
			// were exhausted.  wenger 2004-11-23.
		if ( job->GetStatus() == Job::STATUS_POSTRUN ) {
			_postRunNodeCount--;
		} else {
			ASSERT( recovery );
			// If we get here, it means that, in the run we're recovering,
			// the POST script for a node was run without any indication
			// in the log that the node was run.  This probably means that
			// all submit attempts on the node failed.  wenger 2007-04-09.
		}

		const PostScriptTerminatedEvent *termEvent =
			(const PostScriptTerminatedEvent*) event;

		debug_printf( DEBUG_NORMAL, "POST Script of Node %s ",
				job->GetJobName() );

		if( !(termEvent->normal && termEvent->returnValue == 0) ) {
				// POST script failed or was killed by a signal

			job->_Status = Job::STATUS_ERROR;

			int		mainJobRetval = job->retval;

			const	int ERR_BUF_LEN = 128;
			char	errBuf[ERR_BUF_LEN];

			if( termEvent->normal ) {
					// Normal termination -- POST script failed
				snprintf( errBuf, ERR_BUF_LEN, 
							"failed with status %d",
							termEvent->returnValue );
				debug_dprintf( D_ALWAYS | D_NOHEADER, DEBUG_NORMAL,
							"%s\n", errBuf );
				snprintf( job->error_text, JOB_ERROR_TEXT_MAXLEN,
							"POST script %s", errBuf );

				job->retval = termEvent->returnValue;

			} else {
					// Abnormal termination -- POST script killed by signal
				snprintf( errBuf, ERR_BUF_LEN,
							"died on signal %d",
							termEvent->signalNumber );
				debug_dprintf( D_ALWAYS | D_NOHEADER, DEBUG_NORMAL,
							"%s\n", errBuf );
				snprintf( job->error_text, JOB_ERROR_TEXT_MAXLEN,
							"POST Script %s", errBuf );

				job->retval = (0 - termEvent->signalNumber);
			}

				//
				// Deal with retries.
				//
			if( job->GetRetries() < job->GetRetryMax() ) {
				RestartNode( job, recovery );
			} else {
					// no more retries -- node failed
				_numNodesFailed++;

				MyString	errMsg;

				if( mainJobRetval > 0 ) {
					errMsg.sprintf( "Job exited with status %d and ",
								mainJobRetval);
				}
				else if( mainJobRetval < 0  &&
							mainJobRetval >= -MAX_SIGNAL ) {
					errMsg.sprintf( "Job died on signal %d and ",
								0 - mainJobRetval);
				}
				else {
					errMsg.sprintf( "Job failed due to DAGMAN error %d and ",
								mainJobRetval);
				}

				if ( termEvent->normal ) {
					errMsg.sprintf_cat( "POST Script failed with status %d",
								termEvent->returnValue );
				} else {
					errMsg.sprintf_cat( "POST Script died on signal %d",
								termEvent->signalNumber );
				}

				if( job->GetRetryMax() > 0 ) {
						// add # of retries to error_text
					errMsg.sprintf_cat( " (after %d node retries)",
							job->GetRetries() );
				}

				strncpy( job->error_text, errMsg.Value(),
							JOB_ERROR_TEXT_MAXLEN );
				job->error_text[JOB_ERROR_TEXT_MAXLEN - 1] = '\0';
			}

		} else {
				// POST script succeeded.
			ASSERT( termEvent->returnValue == 0 );
			debug_dprintf( D_ALWAYS | D_NOHEADER, DEBUG_NORMAL,
						"completed successfully.\n" );
			job->retval = 0;
			TerminateJob( job, recovery );
		}

		bool abort = CheckForDagAbort(job, "POST script");
		// if dag abort happened, we never return here!
		if( abort ) {
			return;
		}

		PrintReadyQ( DEBUG_DEBUG_4 );
	}
}

//---------------------------------------------------------------------------
void
Dag::ProcessSubmitEvent(Job *job, bool recovery, bool &submitEventIsSane) {

	if ( !job ) {
		return;
	}

	if ( !job->IsEmpty( Job::Q_WAITING ) ) {
		debug_printf( DEBUG_QUIET, "Error: DAG semantics violated!  "
					"Node %s was submitted but has unfinished parents!\n",
					job->GetJobName() );
		_dagFiles.rewind();
		char *dagFile = _dagFiles.next();
		debug_printf( DEBUG_QUIET, "This may indicate log file corruption; "
					"you may want to check the log files and re-run the "
					"DAG in recovery mode by giving the command "
					"'condor_submit %s.condor.sub'\n", dagFile );
		job->Dump( this );
			// We do NOT want to create a rescue DAG here, because it
			// would probably be invalid.
		DC_Exit( EXIT_ERROR );
	}

		//
		// If we got an "insane" submit event for a node that currently
		// has a job in the queue, we don't want to increment
		// the job proc count and jobs submitted counts here, because
		// if we get a terminated event corresponding to the "original"
		// Condor ID, we won't decrement the counts at that time.
		//
	if ( submitEventIsSane || job->GetStatus() != Job::STATUS_SUBMITTED ) {
		job->_queuedNodeJobProcs++;
	}

		// Note:  in non-recovery mode, we increment _numJobsSubmitted
		// in ProcessSuccessfulSubmit().
	if ( recovery ) {
		if ( submitEventIsSane || job->GetStatus() != Job::STATUS_SUBMITTED ) {
				// Only increment the submitted job count on
				// the *first* proc of a job.
			if( job->_queuedNodeJobProcs == 1 ) {
				UpdateJobCounts( job, 1 );
			}
		}

		job->_Status = Job::STATUS_SUBMITTED;
		return;
	}

		// if we only have one log, compare
		// the order of submit events in the
		// log to the order in which we
		// submitted the jobs -- but if we
		// have >1 userlog we can't count on
		// any order, so we can't sanity-check
		// in this way

	if ( TotalLogFileCount() == 1 && job->_queuedNodeJobProcs == 1 ) {

			// as a sanity check, compare the job from the
			// submit event to the job we expected to see from
			// our submit queue
 		Job* expectedJob = NULL;
		if ( _submitQ->dequeue( expectedJob ) == -1 ) {
			debug_printf( DEBUG_QUIET,
						"Unrecognized submit event (for job "
						"\"%s\") found in log (none expected)\n",
						job->GetJobName() );
			return;
		} else if ( job != expectedJob ) {
			ASSERT( expectedJob != NULL );
			debug_printf( DEBUG_QUIET,
						"Unexpected submit event (for job "
						"\"%s\") found in log; job \"%s\" "
						"was expected.\n",
						job->GetJobName(),
						expectedJob->GetJobName() );
				// put expectedJob back onto submit queue
			_submitQ->enqueue( expectedJob );

			return;
		}
	}

	PrintReadyQ( DEBUG_DEBUG_2 );
}

//---------------------------------------------------------------------------
void
Dag::ProcessIsIdleEvent(Job *job) {

	if ( !job ) {
		return;
	}

	if ( !job->GetIsIdle() ) {
		job->SetIsIdle(true);
		_numIdleJobProcs++;
	}

	// Do some consistency checks here?

	debug_printf( DEBUG_VERBOSE, "Number of idle job procs: %d\n",
				_numIdleJobProcs);
}

//---------------------------------------------------------------------------
void
Dag::ProcessNotIdleEvent(Job *job) {

	if ( !job ) {
		return;
	}

	if ( job->GetIsIdle() ) {
		job->SetIsIdle(false);
		_numIdleJobProcs--;
	}

		// Do some consistency checks here.
	if ( _numIdleJobProcs < 0 ) {
        debug_printf( DEBUG_NORMAL,
					"WARNING: DAGMan thinks there are %d idle job procs!\n",
					_numIdleJobProcs );
	}

	debug_printf( DEBUG_VERBOSE, "Number of idle job procs: %d\n",
				_numIdleJobProcs);
}

//---------------------------------------------------------------------------
Job * Dag::FindNodeByName (const char * jobName) const {
	if( !jobName ) {
		return NULL;
	}

	Job *	job = NULL;
	if ( _nodeNameHash.lookup(jobName, job) != 0 ) {
    	debug_printf( DEBUG_VERBOSE, "ERROR: job %s not found!\n", jobName);
		job = NULL;
	}

	if ( job ) {
		if ( strcmp( jobName, job->GetJobName() ) != 0 ) {
			EXCEPT( "Searched for node %s; got %s!!", jobName,
						job->GetJobName() );
		}
	}

	return job;
}

//---------------------------------------------------------------------------
bool
Dag::NodeExists( const char* nodeName ) const
{
  if( !nodeName ) {
    return false;
  }

	// Note:  we don't just call FindNodeByName() here because that would print
	// an error message if the node doesn't exist.
  Job *	job = NULL;
  if ( _nodeNameHash.lookup(nodeName, job) != 0 ) {
    return false;
  }

	if ( job ) {
		if ( strcmp( nodeName, job->GetJobName() ) != 0 ) {
			EXCEPT( "Searched for node %s; got %s!!", nodeName,
						job->GetJobName() );
		}
	}

  return job != NULL;
}

//---------------------------------------------------------------------------
Job * Dag::FindNodeByEventID (int logsource, const CondorID condorID) const {
	if ( condorID._cluster == -1 ) {
		return NULL;
	}

	Job *	node = NULL;
	if ( GetEventIDHash( logsource )->lookup(condorID._cluster, node) != 0 ) {
    	debug_printf( DEBUG_VERBOSE, "ERROR: node for cluster %d not found!\n",
					condorID._cluster);
		node = NULL;
	}

	if ( node ) {
		if ( condorID._cluster != node->_CondorID._cluster ) {
			if ( node->_CondorID._cluster != _defaultCondorId._cluster ) {
			 	EXCEPT( "Searched for node for cluster %d; got %d!!",
						 	condorID._cluster,
						 	node->_CondorID._cluster );
			} else {
					// Note: we can get here if we get an aborted event
					// after a terminated event for the same job (see
					// gittrac #744 and the
					// job_dagman_abnormal_term_recovery_retries test).
				debug_printf( DEBUG_QUIET, "Warning: searched for node for "
							"cluster %d; got %d!!\n", condorID._cluster,
							node->_CondorID._cluster );
			}
		}
	}

	return node;
}

//-------------------------------------------------------------------------
void
Dag::SetAllowEvents( int allowEvents)
{
	_checkCondorEvents.SetAllowEvents( allowEvents );
	_checkStorkEvents.SetAllowEvents( allowEvents );
}

//-------------------------------------------------------------------------
void
Dag::PrintDagFiles( /* const */ StringList &dagFiles )
{
	if ( dagFiles.number() > 1 ) {
		debug_printf( DEBUG_VERBOSE, "All DAG files:\n");
		dagFiles.rewind();
		char *dagFile;
		int thisDagNum = 0;
		while ( (dagFile = dagFiles.next()) != NULL ) {
			debug_printf( DEBUG_VERBOSE, "  %s (DAG #%d)\n", dagFile,
						thisDagNum++);
		}
	}
}

//-------------------------------------------------------------------------
bool
Dag::StartNode( Job *node, bool isRetry )
{
    ASSERT( node != NULL );
	if ( !node->CanSubmit() ) {
		EXCEPT( "Node %s not ready to submit!", node->GetJobName() );
	}

	// We should never be calling this function when the dag is being used
	// as a splicing dag.
	ASSERT( _isSplice == false );

	// if a PRE script exists and hasn't already been run, run that
	// first -- the PRE script's reaper function will submit the
	// actual job to Condor if/when the script exits successfully

    if( node->_scriptPre && node->_scriptPre->_done == FALSE ) {
		node->_Status = Job::STATUS_PRERUN;
		_preRunNodeCount++;
		_preScriptQ->Run( node->_scriptPre );
		return true;
    }
	// no PRE script exists or is done, so add job to the queue of ready jobs
	if ( isRetry && m_retryNodeFirst ) {
		_readyQ->Prepend( node, -node->_nodePriority );
	} else {
		if ( _submitDepthFirst ) {
			_readyQ->Prepend( node, -node->_nodePriority );
		} else {
			_readyQ->Append( node, -node->_nodePriority );
		}
	}
	return TRUE;
}

//-------------------------------------------------------------------------
// returns number of jobs submitted
int
Dag::SubmitReadyJobs(const Dagman &dm)
{
	debug_printf( DEBUG_DEBUG_1, "Dag::SubmitReadyJobs()\n" );

		// Jobs deferred by category throttles.
	PrioritySimpleList<Job*> deferredJobs;

	int numSubmitsThisCycle = 0;

		// Check whether we have to wait longer before submitting again
		// (if a previous submit attempt failed).
	if ( _nextSubmitTime && time(NULL) < _nextSubmitTime) {
		sleep(1);
        return numSubmitsThisCycle;
	}


	bool didLogSleep = false;
	while( numSubmitsThisCycle < dm.max_submits_per_interval ) {

//		PrintReadyQ( DEBUG_DEBUG_4 );

			// no jobs ready to submit
    	if( _readyQ->IsEmpty() ) {
			break; // break out of while loop
    	}

    		// max jobs already submitted
    	if( _maxJobsSubmitted && (_numJobsSubmitted >= _maxJobsSubmitted) ) {
        	debug_printf( DEBUG_DEBUG_1,
                      	"Max jobs (%d) already running; "
					  	"deferring submission of %d ready job%s.\n",
                      	_maxJobsSubmitted, _readyQ->Number(),
					  	_readyQ->Number() == 1 ? "" : "s" );
			_maxJobsDeferredCount += _readyQ->Number();
			break; // break out of while loop
    	}
		if ( _maxIdleJobProcs && (_numIdleJobProcs >= _maxIdleJobProcs) ) {
        	debug_printf( DEBUG_DEBUG_1,
					  	"Hit max number of idle DAG nodes (%d); "
					  	"deferring submission of %d ready job%s.\n",
					  	_maxIdleJobProcs, _readyQ->Number(),
					  	_readyQ->Number() == 1 ? "" : "s" );
			_maxIdleDeferredCount += _readyQ->Number();
			break; // break out of while loop
		}

			// remove & submit first job from ready queue
		Job* job;
		_readyQ->Rewind();
		_readyQ->Next( job );
		_readyQ->DeleteCurrent();
		ASSERT( job != NULL );

		debug_printf( DEBUG_DEBUG_1, "Got node %s from the ready queue\n",
				  	job->GetJobName() );

		if ( job->GetStatus() != Job::STATUS_READY ) {
			EXCEPT( "Job %s status is %s, not STATUS_READY",
						job->GetJobName(), job->GetStatusName() );
		}

			// Check for throttling by node category.
		ThrottleByCategory::ThrottleInfo *catThrottle = job->GetThrottleInfo();
		if ( catThrottle &&
					catThrottle->_maxJobs !=
					ThrottleByCategory::noThrottleSetting &&
					catThrottle->_currentJobs >= catThrottle->_maxJobs ) {
			debug_printf( DEBUG_DEBUG_1,
						"Node %s deferred by category throttle (%s, %d)\n",
						job->GetJobName(), catThrottle->_category->Value(),
						catThrottle->_maxJobs );
			deferredJobs.Prepend( job, -job->_nodePriority );
			_catThrottleDeferredCount++;
		} else {
				// The problem here is that we wouldn't need to sleep if
				// we only have one total log file *after* we monitor the
				// log file for the job we're going to submit.  I guess
				// we could be smarter and move this test somewhere else,
				// but I'm not going to deal with that right now.
				// wenger 2009-05-27
			if( dm.submit_delay == 0 && !didLogSleep ) {
					// if we don't already have a submit_delay, sleep for one
					// second here, so we can be sure that this job's submit
					// event will be unambiguously later than the termination
					// events of its parents, given that userlog timestamps
					// only have a resolution of one second.  (Because of the
					// new feature allowing distinct userlogs for each node, we
					// can't just rely on the physical order in a single log
					// file.)

					// We sleep at most once per submit cycle, because
					// we don't really have to worry about getting events
					// for "sibling" nodes in the exact order they occurred.
				debug_printf( DEBUG_VERBOSE,
							"Sleeping for one second for log "
							"file consistency\n" );
				sleep( 1 );
				didLogSleep = true;
			}

    		CondorID condorID(0,0,0);
			submit_result_t submit_result = SubmitNodeJob( dm, job, condorID );
	
				// Note: if instead of switch here so we can use break
				// to break out of while loop.
			if ( submit_result == SUBMIT_RESULT_OK ) {
				ProcessSuccessfulSubmit( job, condorID );
    			numSubmitsThisCycle++;

			} else if ( submit_result == SUBMIT_RESULT_FAILED ) {
				ProcessFailedSubmit( job, dm.max_submit_attempts );
				break; // break out of while loop

			} else if ( submit_result == SUBMIT_RESULT_NO_SUBMIT ) {
				// No op.

			} else {
				EXCEPT( "Illegal submit_result_t value: %d\n", submit_result );
			}
		}
	}

		// Put any deferred jobs back into the ready queue for next time.
	deferredJobs.Rewind();
	Job *job;
	while ( deferredJobs.Next( job ) ) {
		debug_printf( DEBUG_DEBUG_1,
					"Returning deferred node %s to the ready queue\n",
					job->GetJobName() );
		_readyQ->Prepend( job, -job->_nodePriority );
	}

	return numSubmitsThisCycle;
}

//---------------------------------------------------------------------------
int
Dag::PreScriptReaper( const char* nodeName, int status )
{
	ASSERT( nodeName != NULL );
	Job* job = FindNodeByName( nodeName );
	ASSERT( job != NULL );
	if ( job->GetStatus() != Job::STATUS_PRERUN ) {
		EXCEPT( "Error: node %s is not in PRERUN state", job->GetJobName() );
	}

	if( WIFSIGNALED( status ) || WEXITSTATUS( status ) != 0 ) {
		// if script returned failure or was killed by a signal
		if( WIFSIGNALED( status ) ) {
			// if script was killed by a signal
			debug_printf( DEBUG_QUIET, "PRE Script of Job %s died on %s\n",
						  job->GetJobName(),
						  daemonCore->GetExceptionString(status) );
			snprintf( job->error_text, JOB_ERROR_TEXT_MAXLEN,
					"PRE Script died on %s",
					daemonCore->GetExceptionString(status) );
            job->retval = ( 0 - WTERMSIG(status ) );
		}
		else if( WEXITSTATUS( status ) != 0 ) {
			// if script returned failure
			debug_printf( DEBUG_QUIET,
						  "PRE Script of Job %s failed with status %d\n",
						  job->GetJobName(), WEXITSTATUS(status) );
			snprintf( job->error_text, JOB_ERROR_TEXT_MAXLEN,
					"PRE Script failed with status %d",
					WEXITSTATUS(status) );
            job->retval = WEXITSTATUS( status );
		}

        job->_Status = Job::STATUS_ERROR;
		_preRunNodeCount--;

		if( job->GetRetries() < job->GetRetryMax() ) {
			RestartNode( job, false );
		}
		else {
			_numNodesFailed++;
			if( job->GetRetryMax() > 0 ) {
				// add # of retries to error_text
				char *tmp = strnewp( job->error_text );
				snprintf( job->error_text, JOB_ERROR_TEXT_MAXLEN,
						 "%s (after %d node retries)", tmp,
						 job->GetRetries() );
				delete [] tmp;   
			}
		}
	}
	else {
		debug_printf( DEBUG_NORMAL, "PRE Script of Node %s completed "
					  "successfully.\n", job->GetJobName() );
		job->retval = 0; // for safety on retries
		job->_Status = Job::STATUS_READY;
		_preRunNodeCount--;
		if ( _submitDepthFirst ) {
			_readyQ->Prepend( job, -job->_nodePriority );
		} else {
			_readyQ->Append( job, -job->_nodePriority );
		}
	}

	bool abort = CheckForDagAbort(job, "PRE script");
	// if dag abort happened, we never return here!
	if( abort ) {
		return true;
	}

	return true;
}

//---------------------------------------------------------------------------
// Note that the actual handling of the post script's exit status is
// done not when the reaper is called, but in ProcessLogEvents when
// the log event (written by the reaper) is seen...
int
Dag::PostScriptReaper( const char* nodeName, int status )
{
	ASSERT( nodeName != NULL );
	Job* job = FindNodeByName( nodeName );
	ASSERT( job != NULL );
	if ( job->GetStatus() != Job::STATUS_POSTRUN ) {
		EXCEPT( "Node %s is not in POSTRUN state", job->GetJobName() );
	}

	PostScriptTerminatedEvent e;
	
	e.dagNodeName = strnewp( nodeName );

	if( WIFSIGNALED( status ) ) {
		e.normal = false;
		e.signalNumber = status;
	}
	else {
		e.normal = true;
		e.returnValue = WEXITSTATUS( status );
	}

		// Note: after 6.7.15 is released, we'll be disabling the old-style
		// Stork logs, so we should probably go ahead and write the POST
		// script terminated events for Stork jobs here (although that
		// could cause some backwards-compatibility problems).  wenger
		// 2006-01-12.
	if ( job->JobType() == Job::TYPE_STORK ) {
			// Kludgey fix for Gnats PR 554 -- we are bypassing the whole
			// writing of the ULOG_POST_SCRIPT_TERMINATED event because
			// Stork doesn't use the UserLog code, and therefore presumably
			// doesn't have the correct file locking on the log file.
			// This means that in recovery mode we'll end up running this
			// POST script again even if we successfully ran it already.
			// wenger 2005-10-04.
		e.cluster = job->_CondorID._cluster;
		e.proc = job->_CondorID._proc;
		e.subproc = job->_CondorID._subproc;
		ProcessPostTermEvent(&e, job, _recovery);

	} else {
			// Determine whether the job's log is XML.  (Yes, it probably
			// would be better to just figure that out from the submit file,
			// but this is a quick way to do it.  wenger 2005-04-29.)
		ReadUserLog readLog( job->_logFile );
			// Check for isInitialized() here fixes gittrac #435 (DAGMan
			// core dumps if node has POST script and all submit attempts fail).
		bool useXml = readLog.isInitialized() ? readLog.getIsXMLLog() : false;

		WriteUserLog ulog;
			// Disabling the global log (EventLog) fixes the main problem
			// in gittrac #934 (if you can't write to the global log the
			// write to the user log also fails, and DAGMan hangs
			// waiting for the event that wasn't written).
		ulog.setEnableGlobalLog( false );
		ulog.setUseXML( useXml );
		ulog.initialize( job->_logFile, job->_CondorID._cluster,
					 	0, 0, NULL );

		if( !ulog.writeEvent( &e ) ) {
			debug_printf( DEBUG_QUIET,
					  	"Unable to log ULOG_POST_SCRIPT_TERMINATED event\n" );
			  // Exit here, because otherwise we'll wait forever to see
			  // the event that we just failed to write (see gittrac #934).
			  // wenger 2009-11-12.
			main_shutdown_rescue( EXIT_ERROR );
		}
	}
	return true;
}

//---------------------------------------------------------------------------
void Dag::PrintJobList() const {
    Job * job;
    ListIterator<Job> iList (_jobs);
    while ((job = iList.Next()) != NULL) {
        job->Dump( this );
    }
    dprintf( D_ALWAYS, "---------------------------------------\t<END>\n" );
}

//---------------------------------------------------------------------------
void
Dag::PrintJobList( Job::status_t status ) const
{
    Job* job;
    ListIterator<Job> iList( _jobs );
    while( ( job = iList.Next() ) != NULL ) {
		if( job->GetStatus() == status ) {
			job->Dump( this );
		}
    }
    dprintf( D_ALWAYS, "---------------------------------------\t<END>\n" );
}

//---------------------------------------------------------------------------
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
void Dag::RemoveRunningJobs ( const Dagman &dm) const {

	debug_printf( DEBUG_NORMAL, "Removing any/all submitted Condor/"
				"Stork jobs...\n");

	ArgList args;

		// first, remove all Condor jobs submitted by this DAGMan
		// Make sure we have at least one Condor (not Stork) job before
		// we call condor_rm...
	bool	haveCondorJob = false;
    ListIterator<Job> jobList(_jobs);
    Job * job;
    while (jobList.Next(job)) {
		ASSERT( job != NULL );
		if ( job->JobType() == Job::TYPE_CONDOR ) {
			haveCondorJob = true;
			break;
		}
	}

	if ( haveCondorJob ) {
		MyString constraint;

		args.Clear();
		args.AppendArg( _condorRmExe );
		args.AppendArg( "-const" );

		constraint.sprintf( "%s == %d", ATTR_DAGMAN_JOB_ID,
					dm.DAGManJobId._cluster );
		args.AppendArg( constraint.Value() );
		if ( util_popen( args ) ) {
			debug_printf( DEBUG_NORMAL, "Error removing DAGMan jobs\n");
		}
	}

		// Okay, now remove any Stork jobs.
    ListIterator<Job> iList(_jobs);
    while (iList.Next(job)) {
		ASSERT( job != NULL );
			// if node has a Stork job that is presently submitted,
			// remove it individually (this is necessary because
			// DAGMan's job ID can't currently be inserted into the
			// Stork job ad, and thus we can't do a "stork_rm -const..." 
			// like we do with Condor; this should be fixed)
		if( job->JobType() == Job::TYPE_STORK &&
			job->GetStatus() == Job::STATUS_SUBMITTED ) {
			args.Clear();
			args.AppendArg( dm.storkRmExe );
			args.AppendArg( job->_CondorID._cluster );
			if ( util_popen( args ) ) {
				debug_printf( DEBUG_NORMAL, "Error removing Stork job\n");
			}
        }
	}

	return;
}

//---------------------------------------------------------------------------
void Dag::RemoveRunningScripts ( ) const {
    ListIterator<Job> iList(_jobs);
    Job * job;
    while (iList.Next(job)) {
		ASSERT( job != NULL );

		// if node is running a PRE script, hard kill it
        if( job->GetStatus() == Job::STATUS_PRERUN ) {
			if ( !job->_scriptPre ) {
				EXCEPT( "Node %s has no PRE script!", job->GetJobName() );
			}
			if ( job->_scriptPre->_pid != 0 ) {
				debug_printf( DEBUG_DEBUG_1, "Killing PRE script %d\n",
							job->_scriptPre->_pid );
				if (daemonCore->Shutdown_Fast(job->_scriptPre->_pid) == FALSE) {
					debug_printf(DEBUG_QUIET,
				             	"WARNING: shutdown_fast() failed on pid %d: %s\n",
				             	job->_scriptPre->_pid, strerror(errno));
				}
			}
        }
		// if node is running a POST script, hard kill it
        else if( job->GetStatus() == Job::STATUS_POSTRUN ) {
			if ( !job->_scriptPost ) {
				EXCEPT( "Node %s has no POST script!", job->GetJobName() );
			}
			if ( job->_scriptPost->_pid != 0 ) {
				debug_printf( DEBUG_DEBUG_1, "Killing POST script %d\n",
							job->_scriptPost->_pid );
				if(daemonCore->Shutdown_Fast(job->_scriptPost->_pid) == FALSE) {
					debug_printf(DEBUG_QUIET,
				             	"WARNING: shutdown_fast() failed on pid %d: %s\n",
				             	job->_scriptPost->_pid, strerror( errno ));
				}
			}
        }
	}
	return;
}

//-----------------------------------------------------------------------------
void Dag::Rescue ( const char * datafile, bool multiDags,
			int maxRescueDagNum ) /* const */
{
	int nextRescue = FindLastRescueDagNum( datafile, multiDags,
				maxRescueDagNum ) + 1;
	if ( nextRescue > maxRescueDagNum ) nextRescue = maxRescueDagNum;
	MyString rescueDagFile = RescueDagName( datafile, multiDags, nextRescue );

		// Note: there could possibly be a race condition here if two
		// DAGMans are running on the same DAG at the same time.  That
		// should be avoided by the lock file, though, so I'm not doing
		// anything about it right now.  wenger 2007-02-27

	WriteRescue( rescueDagFile.Value(), datafile );
}

//-----------------------------------------------------------------------------
void Dag::WriteRescue (const char * rescue_file, const char * datafile)
			/* const */
{
	debug_printf( DEBUG_NORMAL, "Writing Rescue DAG to %s...\n",
				rescue_file );

    FILE *fp = safe_fopen_wrapper(rescue_file, "w");
    if (fp == NULL) {
        debug_printf( DEBUG_QUIET, "Could not open %s for writing.\n",
					  rescue_file);
        return;
    }

	bool reset_retries_upon_rescue =
		param_boolean( "DAGMAN_RESET_RETRIES_UPON_RESCUE", true );


    fprintf (fp, "# Rescue DAG file, created after running\n");
    fprintf (fp, "#   the %s DAG file\n", datafile);

	time_t timestamp;
	(void)time( &timestamp );
	const struct tm *tm;
	tm = gmtime( &timestamp );
	fprintf( fp, "# Created %d/%d/%d %02d:%02d:%02d UTC\n", tm->tm_mon + 1,
				tm->tm_mday, tm->tm_year + 1900, tm->tm_hour, tm->tm_min,
				tm->tm_sec );

    fprintf (fp, "#\n");
    fprintf (fp, "# Total number of Nodes: %d\n", NumNodes());
    fprintf (fp, "# Nodes premarked DONE: %d\n", _numNodesDone);
    fprintf (fp, "# Nodes that failed: %d\n", _numNodesFailed);

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
	// Print the CONFIG file, if any.
	//
	if ( _configFile ) {
    	fprintf ( fp, "CONFIG %s\n\n", _configFile );
		
	}

    //
    // Print per-node information.
    //
    it.ToBeforeFirst();
    while (it.Next(job)) {

			// Print the JOB/DATA line.
		const char *keyword = "";
        if( job->JobType() == Job::TYPE_CONDOR ) {
			keyword = job->GetDagFile() ? "SUBDAG EXTERNAL" : "JOB";
        } else if( job->JobType() == Job::TYPE_STORK ) {
			keyword = "DATA";
        } else {
			EXCEPT( "Illegal node type (%d)\n", job->JobType() );
		}
        fprintf (fp, "%s %s %s ", keyword, job->GetJobName(),
					job->GetDagFile() ? job->GetDagFile() :
					job->GetCmdFile());
		if ( strcmp( job->GetDirectory(), "" ) ) {
			fprintf(fp, "DIR %s ", job->GetDirectory());
		}
		fprintf (fp, "%s\n",
				job->_Status == Job::STATUS_DONE ? "DONE" : "");

			// Print the SCRIPT PRE line, if any.
        if (job->_scriptPre != NULL) {
            fprintf (fp, "SCRIPT PRE  %s %s\n", 
                     job->GetJobName(), job->_scriptPre->GetCmd());
        }

			// Print the SCRIPT POST line, if any.
        if (job->_scriptPost != NULL) {
            fprintf (fp, "SCRIPT POST %s %s\n", 
                     job->GetJobName(), job->_scriptPost->GetCmd());
        }

			// Print the RETRY line, if any.
        if( job->retry_max > 0 ) {
            int retriesLeft = (job->retry_max - job->retries);

            if (   job->_Status == Job::STATUS_ERROR
                && job->retries < job->retry_max 
                && job->have_retry_abort_val
                && job->retval == job->retry_abort_val ) {
                fprintf(fp, "# %d of %d retries performed; remaining attempts "
                        "aborted after node returned %d\n", 
                        job->retries, job->retry_max, job->retval );
            } else {
				if( !reset_retries_upon_rescue ) {
					fprintf( fp,
							 "# %d of %d retries already performed; %d remaining\n",
							 job->retries, job->retry_max, retriesLeft );
				}
            }
            
            ASSERT( job->retries <= job->retry_max );
			if( !reset_retries_upon_rescue ) {
				fprintf( fp, "RETRY %s %d", job->GetJobName(), retriesLeft );
			} else {
				fprintf( fp, "RETRY %s %d", job->GetJobName(), job->retry_max );
			}
            if( job->have_retry_abort_val ) {
                fprintf( fp, " UNLESS-EXIT %d", job->retry_abort_val );
            }
            fprintf( fp, "\n" );
        }

			// Print the VARS line, if any.
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
                    if(c == '\"') {
                        fprintf(fp, "\\\"");
                    } else if(c == '\\') {
                        fprintf(fp, "\\\\");
                    } else {
                        fprintf(fp, "%c", c);
					}
                }
                fprintf(fp, "\"");
            }
            fprintf(fp, "\n");
        }

			// Print the ABORT-DAG-ON line, if any.
        if ( job->have_abort_dag_val ) {
			fprintf( fp, "ABORT-DAG-ON %s %d", job->GetJobName(),
						job->abort_dag_val );
			if ( job->have_abort_dag_return_val ) {
				fprintf( fp, " RETURN %d", job->abort_dag_return_val );
			}
            fprintf(fp, "\n");
		}

			// Print the PRIORITY line, if any.
		if ( job->_hasNodePriority ) {
			fprintf( fp, "PRIORITY %s %d\n", job->GetJobName(),
						job->_nodePriority );
		}

			// Print the CATEGORY line, if any.
		if ( job->GetThrottleInfo() ) {
			fprintf( fp, "CATEGORY %s %s\n", job->GetJobName(),
						job->GetThrottleInfo()->_category->Value() );
		}

        fprintf( fp, "\n" );
    }

    //
    // Print Dependency Section
    //
    fprintf (fp, "\n");
    it.ToBeforeFirst();
    while (it.Next(job)) {

        set<JobID_t> & _queue = job->GetQueueRef(Job::Q_CHILDREN);
        if (!_queue.empty()) {
            fprintf (fp, "PARENT %s CHILD", job->GetJobName());

			set<JobID_t>::const_iterator qit;
			for (qit = _queue.begin(); qit != _queue.end(); qit++) {
                Job * child = FindNodeByNodeID( *qit );
                ASSERT( child != NULL );
                fprintf (fp, " %s", child->GetJobName());
			}
            fprintf (fp, "\n");
        }
    }

	//
	// Print "throttle by node category" settings.
	//
	_catThrottles.PrintThrottles( fp );

    fclose(fp);
}

//===========================================================================
// Private Methods
//===========================================================================

//-------------------------------------------------------------------------
void
Dag::TerminateJob( Job* job, bool recovery, bool bootstrap )
{
	ASSERT( !(recovery && bootstrap) );
    ASSERT( job != NULL );

	job->TerminateSuccess(); // marks job as STATUS_DONE
	if ( job->GetStatus() != Job::STATUS_DONE ) {
		EXCEPT( "Node %s is not in DONE state", job->GetJobName() );
	}

		// this is a little ugly, but since this function can be
		// called multiple times for the same job, we need to be
		// careful not to double-count...
	if( job->countedAsDone == false ) {
		_numNodesDone++;
		job->countedAsDone = true;
		ASSERT( _numNodesDone <= _jobs.Number() );
	}

    //
    // Report termination to all child jobs by removing parent's ID from
    // each child's waiting queue.
    //
    set<JobID_t> & qp = job->GetQueueRef(Job::Q_CHILDREN);

	set<JobID_t>::const_iterator qit;
	for (qit = qp.begin(); qit != qp.end(); qit++) {
        Job * child = FindNodeByNodeID( *qit );
        ASSERT( child != NULL );
        child->Remove(Job::Q_WAITING, job->GetJobID());
		if ( child->GetStatus() == Job::STATUS_READY &&
			child->IsEmpty( Job::Q_WAITING ) ) {

				// If we're bootstrapping, we don't want to do anything
				// here.
			if ( !bootstrap ) {
				if ( recovery ) {
						// We need to monitor the log file for the node that's
						// newly ready.
					(void)child->MonitorLogFile( _condorLogRdr, _storkLogRdr,
								_nfsLogIsError, recovery, _defaultNodeLog );
				} else {
						// If child has no more parents in its waiting queue,
						// submit it.
					StartNode( child, false );
				}
			}
		}
	}
}

//-------------------------------------------------------------------------
void Dag::
PrintEvent( debug_level_t level, const ULogEvent* event, Job* node )
{
	ASSERT( event );
	if( node ) {
	    debug_printf( level, "Event: %s for %s Node %s (%d.%d)\n",
					  event->eventName(), node->JobTypeString(),
					  node->GetJobName(), event->cluster, event->proc );
	} else {
        debug_printf( level, "Event: %s for unknown Node (%d.%d): "
					  "ignoring...\n", event->eventName(),
					  event->cluster, event->proc );
	}
	return;
}

//-------------------------------------------------------------------------
void
Dag::RestartNode( Job *node, bool recovery )
{
    ASSERT( node != NULL );
	if ( node->_Status != Job::STATUS_ERROR ) {
		EXCEPT( "Node %s is not in ERROR state", node->GetJobName() );
	}
    if( node->have_retry_abort_val && node->retval == node->retry_abort_val ) {
        debug_printf( DEBUG_NORMAL, "Aborting further retries of node %s "
                      "(last attempt returned %d)\n",
                      node->GetJobName(), node->retval);
        _numNodesFailed++;
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

	if( !recovery ) {
		debug_printf( DEBUG_VERBOSE, "Retrying node %s (retry #%d of %d)...\n",
					node->GetJobName(), node->GetRetries(),
					node->GetRetryMax() );
		StartNode( node, true );
	} else {
		debug_printf( DEBUG_VERBOSE, "Looking for retry of node %s (retry "
					"#%d of %d)...\n", node->GetJobName(), node->GetRetries(),
					node->GetRetryMax() );

		// Doing this fixes gittrac #481 (recovery fails on a DAG that
		// has retried nodes).  (See SubmitNodeJob() for where this
		// gets done during "normal" running.)
		node->_CondorID = _defaultCondorId;
		(void)node->MonitorLogFile( _condorLogRdr, _storkLogRdr,
					_nfsLogIsError, recovery, _defaultNodeLog );
	}
}


//-------------------------------------------------------------------------
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
	set<JobID_t> & children = job->GetQueueRef(Job::Q_CHILDREN);
	set<JobID_t>::const_iterator child_itr;

	for (child_itr = children.begin(); child_itr != children.end(); child_itr++)
	{
		Job * child = FindNodeByNodeID( *child_itr );
		DFSVisit (child);
	}
	
	DFS_ORDER++;
	job->_dfsOrder = DFS_ORDER;
}		

//-------------------------------------------------------------------------
// Detects cycle and warns user about it
bool 
Dag::isCycle ()
{
	bool cycle = false; 
	Job * job;
	ListIterator <Job> joblist (_jobs);
	SimpleListIterator <JobID_t> child_list;

	//Start DFS numbering from zero, although not necessary
	DFS_ORDER = 0; 
	
	//Visit all jobs in DAG and number them	
	joblist.ToBeforeFirst();	
	while (joblist.Next(job))
	{
  		if (!job->_visited &&
			job->GetQueueRef(Job::Q_PARENTS).size()==0)
			DFSVisit (job);	
	}	

	//Detect cycle
	joblist.ToBeforeFirst();	
	while (joblist.Next(job))
	{

		set<JobID_t> &cset = job->GetQueueRef(Job::Q_CHILDREN);
		set<JobID_t>::const_iterator cit;

		for(cit = cset.begin(); cit != cset.end(); cit++) {
			Job * child = FindNodeByNodeID( *cit );

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

//-------------------------------------------------------------------------
bool
Dag::CheckForDagAbort(Job *job, const char *type)
{
	if ( job->have_abort_dag_val &&
				job->retval == job->abort_dag_val ) {
		debug_printf( DEBUG_QUIET, "Aborting DAG because we got "
				"the ABORT exit value from a %s\n", type);
		if ( job->have_abort_dag_return_val ) {
			main_shutdown_rescue( job->abort_dag_return_val );
		} else {
			main_shutdown_rescue( job->retval );
		}
		return true;
	}

	return false;
}


//-------------------------------------------------------------------------
const MyString
Dag::ParentListString( Job *node, const char delim ) const
{
	Job* parent;
	const char* parent_name = NULL;
	MyString parents_str;

	set<JobID_t> &parent_list = node->GetQueueRef( Job::Q_PARENTS );
	set<JobID_t>::const_iterator pit;

	for (pit = parent_list.begin(); pit != parent_list.end(); pit++) {
		parent = FindNodeByNodeID( *pit );
		parent_name = parent->GetJobName();
		ASSERT( parent_name );
		if( ! parents_str.IsEmpty() ) {
			parents_str += delim;
		}
		parents_str += parent_name;
	}
	return parents_str;
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
		temp_dot_file = safe_fopen_wrapper(temp_dot_file_name.Value(), "w");
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
			fprintf( temp_dot_file,
					 "    label=\"DAGMan Job status at %s\";\n\n",
					 time_string);

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
Dag::CheckAllJobs()
{
	CheckEvents::check_event_result_t result;
	MyString	jobError;

	result = _checkCondorEvents.CheckAllJobs(jobError);
	if ( result == CheckEvents::EVENT_ERROR ) {
		debug_printf( DEBUG_QUIET, "Error checking Condor job events: %s\n",
				jobError.Value() );
		ASSERT( false );
	} else if ( result == CheckEvents::EVENT_BAD_EVENT ||
				result == CheckEvents::EVENT_WARNING ) {
		debug_printf( DEBUG_NORMAL, "Warning checking Condor job events: %s\n",
				jobError.Value() );
	} else {
		debug_printf( DEBUG_DEBUG_1, "All Condor job events okay\n");
	}

	result = _checkStorkEvents.CheckAllJobs(jobError);
	if ( result == CheckEvents::EVENT_ERROR ) {
		debug_printf( DEBUG_QUIET, "Error checking Stork job events: %s\n",
				jobError.Value() );
		ASSERT( false );
	} else if ( result == CheckEvents::EVENT_BAD_EVENT ||
				result == CheckEvents::EVENT_WARNING ) {
		debug_printf( DEBUG_NORMAL, "Warning checking Stork job events: %s\n",
				jobError.Value() );
	} else {
		debug_printf( DEBUG_DEBUG_1, "All Stork job events okay\n");
	}
}

//-------------------------------------------------------------------------
void
Dag::PrintDeferrals( debug_level_t level, bool force ) const
{
	// This function should never be called when this dag object is acting like
	// a splice. 
	ASSERT( _isSplice == false);

	if( _maxJobsDeferredCount > 0 || force ) {
		debug_printf( level, "Note: %d total job deferrals because "
					"of -MaxJobs limit (%d)\n", _maxJobsDeferredCount,
					_maxJobsSubmitted );
	}

	if( _maxIdleDeferredCount > 0 || force ) {
		debug_printf( level, "Note: %d total job deferrals because "
					"of -MaxIdle limit (%d)\n", _maxIdleDeferredCount,
					_maxIdleJobProcs );
	}

	if( _catThrottleDeferredCount > 0 || force ) {
		debug_printf( level, "Note: %d total job deferrals because "
					"of node category throttles\n", _catThrottleDeferredCount );
	}

	if( _preScriptQ->GetScriptDeferredCount() > 0 || force ) {
		debug_printf( level, "Note: %d total PRE script deferrals because "
					"of -MaxPre limit (%d)\n",
					_preScriptQ->GetScriptDeferredCount(),
					_maxPreScripts );
	}

	if( _postScriptQ->GetScriptDeferredCount() > 0 || force ) {
		debug_printf( level, "Note: %d total POST script deferrals because "
					"of -MaxPost limit (%d)\n",
					_postScriptQ->GetScriptDeferredCount(),
					_maxPostScripts );
	}
}

//---------------------------------------------------------------------------
void
Dag::PrintPendingNodes() const
{
	dprintf( D_ALWAYS, "Pending DAG nodes:\n" );

    Job* node;
    ListIterator<Job> iList( _jobs );
    while( ( node = iList.Next() ) != NULL ) {
		switch ( node->GetStatus() ) {
		case Job::STATUS_PRERUN:
		case Job::STATUS_SUBMITTED:
		case Job::STATUS_POSTRUN:
			dprintf( D_ALWAYS, "  Node %s, Condor ID %d, status %s\n",
						node->GetJobName(), node->_CondorID._cluster,
						node->GetStatusName() );
			break;

		default:
			// No op.
			break;
		}
    }
}

//---------------------------------------------------------------------------
void
Dag::SetPendingNodeReportInterval( int interval )
{
	_pendingReportInterval = interval;
	time( &_lastEventTime );
	time( &_lastPendingNodePrintTime );
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

	include_file = safe_fopen_wrapper(_dot_include_file_name, "r");
	if (include_file == NULL) {
		if (_dot_include_file_name != NULL) {
        	debug_printf(DEBUG_NORMAL, "Can't open dot include file %s\n",
					_dot_include_file_name);
		}
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
				"    \"%s\" [shape=ellipse label=\"%s (I)\"];\n",
				node_name, node_name);
			break;
		case Job::STATUS_PRERUN:
			fprintf(temp_dot_file, 
				"    \"%s\" [shape=ellipse label=\"%s (Pre)\" style=dotted];\n",
				node_name, node_name);
			break;
		case Job::STATUS_SUBMITTED:
			fprintf(temp_dot_file, 
				"    \"%s\" [shape=ellipse label=\"%s (R)\" peripheries=2];\n",
				node_name, node_name);
			break;
		case Job::STATUS_POSTRUN:
			fprintf(temp_dot_file, 
				"    \"%s\" [shape=ellipse label=\"%s (Post)\" style=dotted];\n",
				node_name, node_name);
			break;
		case Job::STATUS_DONE:
			fprintf(temp_dot_file, 
				"    \"%s\" [shape=ellipse label=\"%s (Done)\" style=bold];\n",
				node_name, node_name);
			break;
		case Job::STATUS_ERROR:
			fprintf(temp_dot_file, 
				"    \"%s\" [shape=box label=\"%s (E)\"];\n",
				node_name, node_name);
			break;
		default:
			fprintf(temp_dot_file, 
				"    \"%s\" [shape=ellipse label=\"%s (I)\"];\n",
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
		SimpleListIterator <JobID_t> child_list;
		const char                   *parent_name;
		const char                   *child_name;
		
		parent_name = parent->GetJobName();

		set<JobID_t> &cset = parent->GetQueueRef(Job::Q_CHILDREN);
		set<JobID_t>::const_iterator cit;

		for (cit = cset.begin(); cit != cset.end(); cit++) {
			child = FindNodeByNodeID( *cit );
			
			child_name  = child->GetJobName();
			if (parent_name != NULL && child_name != NULL) {
				fprintf(temp_dot_file, "    \"%s\" -> \"%s\";\n",
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
			fp = safe_fopen_wrapper(dot_file_name.Value(), "r");
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

//---------------------------------------------------------------------------
bool Dag::Add( Job& job )
{
	int insertResult = _nodeNameHash.insert( job.GetJobName(), &job );
	ASSERT( insertResult == 0 );

	insertResult = _nodeIDHash.insert( job.GetJobID(), &job );
	ASSERT( insertResult == 0 );

	return _jobs.Append(&job);
}


//---------------------------------------------------------------------------
bool
Dag::RemoveNode( const char *name, MyString &whynot )
{
	if( !name ) {
		whynot = "name == NULL";
		return false;
	}
	Job *node = FindNodeByName( name );
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
		_numNodesDone--;
		ASSERT( _numNodesDone >= 0 );
	}
	else if( node->GetStatus() == Job::STATUS_ERROR ) {
		_numNodesFailed--;
		ASSERT( _numNodesFailed >= 0 );
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


//---------------------------------------------------------------------------
bool
Dag::RemoveDependency( Job *parent, Job *child )
{
	MyString whynot;
	return RemoveDependency( parent, child, whynot );
}

//---------------------------------------------------------------------------
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


//---------------------------------------------------------------------------
Job*
Dag::LogEventNodeLookup( int logsource, const ULogEvent* event,
			bool &submitEventIsSane )
{
	ASSERT( event );
	submitEventIsSane = false;

	Job *node = NULL;
	CondorID condorID( event->cluster, event->proc, event->subproc );

		// if this is a job whose submit event we've already seen, we
		// can simply use the job ID to look up the corresponding DAG
		// node, and we're done...

	if( event->eventNumber != ULOG_SUBMIT ) {
	  node = FindNodeByEventID( logsource, condorID );
	  if( node ) {
	    return node;
	  }
	}

		// if the job ID wasn't familiar and we didn't find a node
		// above, there are three possibilites:
		//	
		// 1) it's the submit event for a node we just submitted, and
		// we don't yet know the job ID; in this case, we look up the
		// node name in the event body.
		//
		// 2) it's the POST script terminated event for a job whose
		// submission attempts all failed, leaving it with no valid
		// job ID at all (-1.-1.-1).  Here we also look up the node
		// name in the event body.
		//
		// 3) it's a job submitted by someone outside than this
		// DAGMan, and can/should be ignored (we return NULL).

	if( event->eventNumber == ULOG_SUBMIT ) {
		const SubmitEvent* submit_event = (const SubmitEvent*)event;
		if ( submit_event->submitEventLogNotes ) {
			char nodeName[1024] = "";
			if ( sscanf( submit_event->submitEventLogNotes,
						 "DAG Node: %1023s", nodeName ) == 1 ) {
				node = FindNodeByName( nodeName );
				if( node ) {
					submitEventIsSane = SanityCheckSubmitEvent( condorID,
								node );
					node->_CondorID = condorID;

						// Insert this node into the CondorID->node hash
						// table if we don't already have it (e.g., recovery
						// mode).  (In "normal" mode we should have already
						// inserted it when we did the condor_submit.)
					Job *tmpNode = NULL;
					if ( GetEventIDHash( logsource )->
								lookup(condorID._cluster, tmpNode) != 0 ) {
						int insertResult = GetEventIDHash( logsource )->
									insert( condorID._cluster, node );
						ASSERT( insertResult == 0 );
					} else {
						ASSERT( tmpNode == node );
					}
				}
			} else {
				debug_printf( DEBUG_QUIET, "ERROR: 'DAG Node:' not found "
							"in submit event notes: <%s>\n",
							submit_event->submitEventLogNotes );
			}
		}
		return node;
	}

	if( event->eventNumber == ULOG_POST_SCRIPT_TERMINATED &&
		event->cluster == -1 ) {
		const PostScriptTerminatedEvent* pst_event =
			(const PostScriptTerminatedEvent*)event;
		node = FindNodeByName( pst_event->dagNodeName );
		return node;
	}

	return node;
}


//---------------------------------------------------------------------------
// Checks whether this is a "good" event ("bad" events include Condor
// sometimes writing a terminated/aborted pair instead of just
// aborted, and the "submit once, run twice" bug).  Extra abort events
// we just ignore; other bad events will abort the DAG unless
// configured to continue.
//
// Note: we only check an event if we have a job object for it, so
// that we don't pay any attention to unrelated "garbage" events that
// may be in the log(s).
//
// Note: the event checking has pretty much been considered
// "diagnostic" until now; however, it turns out that if you get the
// terminated/abort event combo on a node that has retries left,
// DAGMan will assert if the event checking is turned off.  (See Gnats
// PR 467.)  wenger 2005-04-08.
//
// returns true if the event is sane and false if it is "impossible";
// (additionally sets *result=false if DAG should be aborted)

bool
Dag::EventSanityCheck( int logsource, const ULogEvent* event,
			const Job* node, bool* result )
{
	ASSERT( event );
	ASSERT( node );

	MyString eventError;
	CheckEvents::check_event_result_t checkResult = CheckEvents::EVENT_OKAY;

	if ( logsource == CONDORLOG ) {
		checkResult = _checkCondorEvents.CheckAnEvent( event, eventError );
	} else if ( logsource == DAPLOG ) {
		checkResult = _checkStorkEvents.CheckAnEvent( event, eventError );
	}

	if( checkResult == CheckEvents::EVENT_OKAY ) {
		debug_printf( DEBUG_DEBUG_1, "Event is okay\n" );
		return true;
	}

	debug_printf( DEBUG_NORMAL, "%s\n", eventError.Value() );
	//debug_printf( DEBUG_NORMAL, "WARNING: bad event here may indicate a "
				  //"serious bug in Condor -- beware!\n" );

	if( checkResult == CheckEvents::EVENT_WARNING ) {
		debug_printf( DEBUG_NORMAL, "BAD EVENT is warning only\n");
		return true;
	}

	if( checkResult == CheckEvents::EVENT_BAD_EVENT ) {
		debug_printf( DEBUG_NORMAL, "Continuing with DAG in spite of bad "
					  "event (%s) because of allow_events setting\n",
					  eventError.Value() );
	
			// Don't do any further processing of this event,
			// because it can goof us up (e.g., decrement count
			// of running jobs when we shouldn't).

		return false;
	}

	if( checkResult == CheckEvents::EVENT_ERROR ) {
		debug_printf( DEBUG_QUIET, "ERROR: aborting DAG because of bad event "
					  "(%s)\n", eventError.Value() );
			// set *result to indicate we should abort the DAG
		*result = false;
		return false;
	}

	debug_printf( DEBUG_QUIET, "Illegal CheckEvents::check_event_allow_t "
				  "value: %d\n", checkResult );
	return false;
}


//---------------------------------------------------------------------------
// compares a submit event's job ID with the one that appeared earlier
// in the submit command's stdout (which we stashed in the Job object)

bool
Dag::SanityCheckSubmitEvent( const CondorID condorID, const Job* node )
{
		// Changed this if from "if( recovery )" during work on PR 806 --
		// this is better because if you get two submit events for the
		// same node you'll still get a warning in recovery mode.
		// wenger 2007-01-31.
	if( node->_CondorID == _defaultCondorId ) {
			// we no longer have the submit command stdout to check against
		return true;
	}

	if( condorID._cluster == node->_CondorID._cluster ) {
		return true;
	}

	MyString message;
	message.sprintf( "ERROR: node %s: job ID in userlog submit event (%d.%d) "
				"doesn't match ID reported earlier by submit command "
				"(%d.%d)!", 
				node->GetJobName(), condorID._cluster, condorID._proc,
				node->_CondorID._cluster, node->_CondorID._proc );

	if ( _abortOnScarySubmit ) {
		debug_printf( DEBUG_QUIET, "%s  Aborting DAG; set "
					"DAGMAN_ABORT_ON_SCARY_SUBMIT to false if you are "
					"*sure* this shouldn't cause an abort.\n",
					message.Value() );
		main_shutdown_rescue( EXIT_ERROR );
		return true;
	} else {
		debug_printf( DEBUG_QUIET,
					"%s  Trusting the userlog for now (because of "
					"DAGMAN_ABORT_ON_SCARY_SUBMIT setting), but this is "
					"scary!\n", message.Value() );
	}
	return false;
}

//---------------------------------------------------------------------------
HashTable<int, Job *> *
Dag::GetEventIDHash(int jobType)
{
	switch (jobType) {
	case Job::TYPE_CONDOR:
		return &_condorIDHash;
		break;

	case Job::TYPE_STORK:
		return &_storkIDHash;
		break;

	default:
		EXCEPT( "Illegal job type (%d)\n", jobType );
		break;
	}

	return NULL;
}

//---------------------------------------------------------------------------
const HashTable<int, Job *> *
Dag::GetEventIDHash(int jobType) const
{
	switch (jobType) {
	case Job::TYPE_CONDOR:
		return &_condorIDHash;
		break;

	case Job::TYPE_STORK:
		return &_storkIDHash;
		break;

	default:
		EXCEPT( "Illegal job type (%d)\n", jobType );
		break;
	}

	return NULL;
}

// NOTE: dag addnode/removenode/adddep/removedep methods don't
// necessarily insure internal consistency...that's currently up to
// the higher level calling them to get right...


//---------------------------------------------------------------------------
Dag::submit_result_t
Dag::SubmitNodeJob( const Dagman &dm, Job *node, CondorID &condorID )
{
	submit_result_t result = SUBMIT_RESULT_NO_SUBMIT;

		// Resetting the Condor ID here fixes PR 799.  wenger 2007-01-24.
	if ( node->_CondorID._cluster != _defaultCondorId._cluster ) {
		int removeResult = GetEventIDHash( node->JobType() )->
					remove( node->_CondorID._cluster );
		ASSERT( removeResult == 0 );
	}
	node->_CondorID = _defaultCondorId;

		// sleep for a specified time before submitting
	if( dm.submit_delay != 0 ) {
		debug_printf( DEBUG_VERBOSE, "Sleeping for %d s "
					  "(DAGMAN_SUBMIT_DELAY) to throttle submissions...\n",
					  dm.submit_delay );
		sleep( dm.submit_delay );
	}

	if ( !node->MonitorLogFile( _condorLogRdr, _storkLogRdr, _nfsLogIsError,
				_recovery, _defaultNodeLog ) ) {
		return SUBMIT_RESULT_NO_SUBMIT;
	}

		// Note: we're checking for a missing log file spec here instead of
		// inside the submit code because we don't want to re-try the submit
		// if the log file spec is missing in the submit file.  wenger

		// We now only check for missing log files for Stork jobs because
		// of the default log file feature; that doesn't work for Stork
		// jobs because we can't specify the log file on the command
		// line.  wenger 2009-08-14
	if ( !_allowLogError && node->JobType() == Job::TYPE_STORK &&
				!node->CheckForLogFile() ) {
		debug_printf( DEBUG_NORMAL, "ERROR: No 'log =' value found in "
					"submit file %s for node %s\n", node->GetCmdFile(),
					node->GetJobName() );
		node->_Status = Job::STATUS_ERROR;
		snprintf( node->error_text, JOB_ERROR_TEXT_MAXLEN,
					"No 'log =' value found in submit file %s",
					node->GetCmdFile() );
	  	_numNodesFailed++;
		result = SUBMIT_RESULT_NO_SUBMIT;

	} else {
		debug_printf( DEBUG_NORMAL, "Submitting %s Node %s job(s)...\n",
				  	node->JobTypeString(), node->GetJobName() );

    	MyString cmd_file = node->GetCmdFile();
		bool submit_success = false;

    	if( node->JobType() == Job::TYPE_CONDOR ) {
	  		node->_submitTries++;
			const char *logFile = node->UsingDefaultLog() ?
						node->_logFile : NULL;
				// Note: assigning the ParentListString() return value
				// to a variable here, instead of just passing it directly
				// to condor_submit(), fixes a memory leak(!).
				// wenger 2008-12-18
			MyString parents = ParentListString( node );
      		submit_success = condor_submit( dm, cmd_file.Value(), condorID,
						node->GetJobName(), parents,
						node->varNamesFromDag, node->varValsFromDag,
						node->GetDirectory(), logFile );
    	} else if( node->JobType() == Job::TYPE_STORK ) {
	  		node->_submitTries++;
      		submit_success = stork_submit( dm, cmd_file.Value(), condorID,
				   	node->GetJobName(), node->GetDirectory() );
    	} else {
	    	debug_printf( DEBUG_QUIET, "Illegal job type: %d\n",
						node->JobType() );
			ASSERT(false);
		}

		result = submit_success ? SUBMIT_RESULT_OK : SUBMIT_RESULT_FAILED;
	}

	return result;
}

//---------------------------------------------------------------------------
void
Dag::ProcessSuccessfulSubmit( Job *node, const CondorID &condorID )
{
	  // Set the times to *not* wait before trying another submit, since
	  // the most recent submit worked.
	_nextSubmitTime = 0;
	_nextSubmitDelay = 1;

    // append node to the submit queue so we can match it with its
    // submit event once the latter appears in the Condor job log
    if( _submitQ->enqueue( node ) == -1 ) {
		debug_printf( DEBUG_QUIET, "ERROR: _submitQ->enqueue() failed!\n" );
	}

    node->_Status = Job::STATUS_SUBMITTED;

		// Note: I assume we're incrementing this here instead of when
		// we see the submit events so that we don't accidentally exceed
		// maxjobs (now really maxnodes) if it takes a while to see
		// the submit events.  wenger 2006-02-10.
	UpdateJobCounts( node, 1 );
    
        // stash the job ID reported by the submit command, to compare
        // with what we see in the userlog later as a sanity-check
        // (note: this sanity-check is not possible during recovery,
        // since we won't have seen the submit command stdout...)
	node->_CondorID = condorID;
	int insertResult = GetEventIDHash( node->JobType() )->
				insert( condorID._cluster, node );
	ASSERT( insertResult == 0 );

	debug_printf( DEBUG_VERBOSE, "\tassigned %s ID (%d.%d)\n",
				  node->JobTypeString(), condorID._cluster, condorID._proc );
}

//---------------------------------------------------------------------------
void
Dag::ProcessFailedSubmit( Job *node, int max_submit_attempts )
{
	// This function should never be called when the Dag ibject is being used
	// to parse a splice.
	ASSERT( _isSplice == false );

		// Set the times to wait twice as long as last time.
	int thisSubmitDelay = _nextSubmitDelay;
	_nextSubmitTime = time(NULL) + thisSubmitDelay;
	_nextSubmitDelay *= 2;

	(void)node->UnmonitorLogFile( _condorLogRdr, _storkLogRdr );

	if ( node->_submitTries >= max_submit_attempts ) {
			// We're out of submit attempts, treat this as a submit failure.

			// To match the existing behavior, we're resetting the times
			// here.
		_nextSubmitTime = 0;
		_nextSubmitDelay = 1;

		debug_printf( DEBUG_QUIET, "Job submit failed after %d tr%s.\n",
					node->_submitTries, node->_submitTries == 1 ? "y" : "ies" );

		snprintf( node->error_text, JOB_ERROR_TEXT_MAXLEN,
					"Job submit failed" );

			// NOTE: this failure short-circuits the "retry" feature
			// because it's already exhausted a number of retries
			// (maybe we should make sure max_submit_attempts >
			// node->retries before assuming this is a good idea...)
		debug_printf( DEBUG_NORMAL, "Shortcutting node %s retries because "
					"of submit failure(s)\n", node->GetJobName() );
		node->retries = node->GetRetryMax();

		if ( node->_scriptPost ) {
				// a POST script is specified for the node, so run it
			node->_Status = Job::STATUS_POSTRUN;
			_postRunNodeCount++;
			node->_scriptPost->_retValJob = DAG_ERROR_CONDOR_SUBMIT_FAILED;
			(void)node->MonitorLogFile( _condorLogRdr, _storkLogRdr,
						_nfsLogIsError, _recovery, _defaultNodeLog );
			_postScriptQ->Run( node->_scriptPost );
		} else {
			node->_Status = Job::STATUS_ERROR;
			_numNodesFailed++;
		}

	} else {
			// We have more submit attempts left, put this node back into the
			// ready queue.
		debug_printf( DEBUG_NORMAL, "Job submit try %d/%d failed, "
					"will try again in >= %d second%s.\n", node->_submitTries,
					max_submit_attempts, thisSubmitDelay,
					thisSubmitDelay == 1 ? "" : "s" );

		if ( m_retrySubmitFirst ) {
 			_readyQ->Prepend(node, -node->_nodePriority);
		} else {
			_readyQ->Append(node, -node->_nodePriority);
		}
	}
}

//---------------------------------------------------------------------------
void
Dag::DecrementJobCounts( Job *node )
{
	node->_queuedNodeJobProcs--;
	ASSERT( node->_queuedNodeJobProcs >= 0 );

	if( node->_queuedNodeJobProcs == 0 ) {
		UpdateJobCounts( node, -1 );
	}
}

//---------------------------------------------------------------------------
void
Dag::UpdateJobCounts( Job *node, int change )
{
    _numJobsSubmitted += change;
	ASSERT( _numJobsSubmitted >= 0 );

	if ( node->GetThrottleInfo() ) {
		node->GetThrottleInfo()->_currentJobs += change;
		ASSERT( node->GetThrottleInfo()->_currentJobs >= 0 );
	}
}


//---------------------------------------------------------------------------
void
Dag::SetDirectory(MyString &dir)
{
	m_directory = dir;
}

//---------------------------------------------------------------------------
void
Dag::SetDirectory(char *dir)
{
	m_directory = dir;
}

//---------------------------------------------------------------------------
void
Dag::PropogateDirectoryToAllNodes(void)
{
    Job *job = NULL;
	MyString key;

	if (m_directory == ".") {
		return;
	}

	// Propogate the directory setting to all nodes in the DAG.
	_jobs.Rewind();
	while( (job = _jobs.Next()) ) {
		ASSERT( job != NULL );
		job->PrefixDirectory(m_directory);
	}

	// I wipe out m_directory here. If this gets called multiple
	// times for a specific DAG, it'll prefix multiple times, and that is most
	// likely wrong.

	m_directory = ".";
}

//---------------------------------------------------------------------------
void
Dag::PrefixAllNodeNames(const MyString &prefix)
{
    Job *job = NULL;
	MyString key;

	debug_printf(DEBUG_DEBUG_1, "Entering: Dag::PrefixAllNodeNames()\n");

    _jobs.Rewind();
    while( (job = _jobs.Next()) ) {
      ASSERT( job != NULL );
	  job->PrefixName(prefix);
    }

	// Here we must reindex the hash view with the prefixed name.
	// also fix my node name hash view of the jobs

	// First, wipe out the index.
	_nodeNameHash.startIterations();
	while(_nodeNameHash.iterate(key,job)) {
		_nodeNameHash.remove(key);
	}

	// Then, reindex all the jobs keyed by their new name
    _jobs.Rewind();
    while( (job = _jobs.Next()) ) {
      ASSERT( job != NULL );
	  key = job->GetJobName();
		if (_nodeNameHash.insert(key, job) != 0) {
			// I'm reinserting everything newly, so this should never happen
			// unless two jobs have an identical name, which means another
			// part o the code failed to keep the constraint that all jobs have
			// unique names
			debug_error(1, DEBUG_QUIET, 
				"Dag::PrefixAllNodeNames(): This is an impossible error\n");
		}
    }

	debug_printf(DEBUG_DEBUG_1, "Leaving: Dag::PrefixAllNodeNames()\n");
}

//---------------------------------------------------------------------------
int 
Dag::InsertSplice(MyString spliceName, Dag *splice_dag)
{
	return _splices.insert(spliceName, splice_dag);
}

//---------------------------------------------------------------------------
int
Dag::LookupSplice(MyString name, Dag *&splice_dag)
{
	return _splices.lookup(name, splice_dag);
}

//---------------------------------------------------------------------------
// This represents not the actual initial nodes of the dag just after
// the file containing the dag had been parsed.
// You must NOT free the returned array or the contained pointers.
ExtArray<Job*>*
Dag::InitialRecordedNodes(void)
{
	return &_splice_initial_nodes;
}

//---------------------------------------------------------------------------
// This represents not the actual final nodes of the dag just after
// the file containing the dag had been parsed.
// You must NOT free the returned array or the contained pointers.
ExtArray<Job*>*
Dag::FinalRecordedNodes(void)
{
	return &_splice_final_nodes;
}


//---------------------------------------------------------------------------
// After we parse the dag, let's remember which ones were the initial and 
// final nodes in the dag (in case this dag was used as a splice).
void
Dag::RecordInitialAndFinalNodes(void)
{
	Job *job = NULL;

	_jobs.Rewind();
	while( (job = _jobs.Next()) ) {

		// record the initial nodes
		if (job->NumParents() == 0) {
			_splice_initial_nodes.add(job);
		}

		// record the final nodes
		if (job->NumChildren() == 0) {
			_splice_final_nodes.add(job);
		}
	}
}

//---------------------------------------------------------------------------
// Make a copy of the _jobs array and return it. This will also remove the
// nodes out of _jobs so the destructor doesn't delete them.
OwnedMaterials*
Dag::RelinquishNodeOwnership(void)
{
	Job *job = NULL;
	MyString key;

	ExtArray<Job*> *nodes = new ExtArray<Job*>();

	// 1. Copy the jobs
	_jobs.Rewind();
	while( (job = _jobs.Next()) ) {
		nodes->add(job);
		_jobs.DeleteCurrent();
	}

	// shove it into a packet and give it back
	return new OwnedMaterials(nodes);
}


//---------------------------------------------------------------------------
OwnedMaterials*
Dag::LiftSplices(SpliceLayer layer)
{
	Dag *splice = NULL;
	MyString key;
	OwnedMaterials *om = NULL;

	// if this splice contains no other splices, then relinquish the nodes I own
	if (layer == DESCENDENTS && _splices.getNumElements() == 0) {
		return RelinquishNodeOwnership();
	}

	// recurse down the splice tree moving everything up into myself.
	_splices.startIterations();
	while(_splices.iterate(key, splice)) {

		debug_printf(DEBUG_DEBUG_1, "Lifting splice %s\n", key.Value());
		om = splice->LiftSplices(DESCENDENTS);
		// this function moves what it needs out of the returned object
		AssumeOwnershipofNodes(om);
		delete om;
	}

	// Now delete all of them.
	_splices.startIterations();
	while(_splices.iterate(key, splice)) {
		_splices.remove(key);
		delete splice;
	}

	// and prefix them if there was a DIR for the dag.
	PropogateDirectoryToAllNodes();

	// base case is above.
	return NULL;
}

//---------------------------------------------------------------------------
void
Dag::LiftChildSplices(void)
{
	MyString key;
	Dag *splice = NULL;

	debug_printf(DEBUG_DEBUG_1, "Lifting child splices...\n");
	_splices.startIterations();
	while( _splices.iterate(key, splice) ) {
		debug_printf(DEBUG_DEBUG_1, "Lifting child splice: %s\n", key.Value());
		splice->LiftSplices(SELF);
	}
	debug_printf(DEBUG_DEBUG_1, "Done lifting child splices.\n");
}


//---------------------------------------------------------------------------
// Grab all of the nodes out of the splice, and place them into here.
// If the splice, after parsing of the dag file representing 'here', still
// have true initial or final nodes, then those must move over the the
// recorded inital and final nodes for 'here'.
void
Dag::AssumeOwnershipofNodes(OwnedMaterials *om)
{
	Job *job = NULL;
	int i;
	MyString key;
	JobID_t key_id;

	ExtArray<Job*> *nodes = om->nodes;

	// 1. Take ownership of the nodes

	// 1a. If there are any actual initial/final nodes, then ensure to record
	// it into the recorded initial and final nodes for this node.
	for (i = 0; i < nodes->length(); i++) {
		if ((*nodes)[i]->NumParents() == 0) {
			_splice_initial_nodes.add((*nodes)[i]);
			continue;
		}
		if ((*nodes)[i]->NumChildren() == 0) {
			_splice_final_nodes.add((*nodes)[i]);
		}
	}

	// 1b. Re-set the node categories (if any) so they point to the
	// ThrottleByCategory object in *this* DAG rather than the splice
	// DAG (which will be deleted soon).
	for ( i = 0; i < nodes->length(); i++ ) {
		Job *tmpNode = (*nodes)[i];
		ThrottleByCategory::ThrottleInfo *catThrottle =
					tmpNode->GetThrottleInfo();
		if ( catThrottle != NULL ) {

				// Copy the category throttle setting from the splice
				// DAG to the upper DAG (creates the category if we don't
				// already have it).
			_catThrottles.SetThrottle( catThrottle->_category,
						catThrottle->_maxJobs );

				// Now re-set the category in the node, so that the
				// category info points to the upper DAG rather than the
				// splice DAG.
			tmpNode->SetCategory( catThrottle->_category->Value(),
						_catThrottles );
		}
	}

	// 1c. Copy the nodes into _jobs.
	for (i = 0; i < nodes->length(); i++) {
		_jobs.Append((*nodes)[i]);
	}

	// 2. Update our name hash to include the new nodes.
	for (i = 0; i < nodes->length(); i++) {
		key = (*nodes)[i]->GetJobName();

		debug_printf(DEBUG_DEBUG_1, "Creating view hash fixup for: job %s\n", 
			key.Value());

		if (_nodeNameHash.insert(key, (*nodes)[i]) != 0) {
			debug_printf(DEBUG_QUIET, 
				"Found name collision while taking ownership of node: %s\n",
				key.Value());
			debug_printf(DEBUG_QUIET, "Trying to insert key %s, node:\n",
				key.Value());
			(*nodes)[i]->Dump( this );
			debug_printf(DEBUG_QUIET, "but it collided with key %s, node:\n", 
				key.Value());
			if (_nodeNameHash.lookup(key, job) == 0) {
				job->Dump( this );
			} else {
				debug_error(1, DEBUG_QUIET, "What? This is impossible!\n");
			}

			debug_error(1, DEBUG_QUIET, "Aborting.\n");
		}
	}

	// 3. Update our node id hash to include the new nodes.
	for (i = 0; i < nodes->length(); i++) {
		key_id = (*nodes)[i]->GetJobID();
		if (_nodeIDHash.insert(key_id, (*nodes)[i]) != 0) {
			debug_error(1, DEBUG_QUIET, 
				"Found job id collision while taking ownership of node: %s\n",
				(*nodes)[i]->GetJobName());
		}
	}
}


//---------------------------------------------------------------------------
// iterate over the whole dag and ask each job to interpolate the $(JOB) 
// macro in any vars that it may have.
void
Dag::ResolveVarsInterpolations(void)
{
	Job *job = NULL;

	_jobs.Rewind();
	while((job = _jobs.Next())) {
		job->ResolveVarsInterpolations();
	}
}



