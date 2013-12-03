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
#include "dagman_recursive_submit.h"
#include "dagman_metrics.h"

using namespace std;

const CondorID Dag::_defaultCondorId;

const int Dag::DAG_ERROR_CONDOR_SUBMIT_FAILED = -1001;
const int Dag::DAG_ERROR_CONDOR_JOB_ABORTED = -1002;
const int Dag::DAG_ERROR_LOG_MONITOR_ERROR = -1003;
const int Dag::DAG_ERROR_JOB_SKIPPED = -1004;

//---------------------------------------------------------------------------
void touch (const char * filename) {
    int fd = safe_open_wrapper_follow(filename, O_RDWR | O_CREAT, 0600);
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
		  const char *defaultNodeLog, bool generateSubdagSubmits,
		  SubmitDagDeepOptions *submitDagDeepOpts, bool isSplice,
		  const MyString &spliceScope ) :
    _maxPreScripts        (maxPreScripts),
    _maxPostScripts       (maxPostScripts),
	MAX_SIGNAL			  (64),
	_splices              (200, hashFuncMyString, rejectDuplicateKeys),
	_dagFiles             (dagFiles),
	_useDagDir            (useDagDir),
	_final_job (0),
	_nodeNameHash		  (NODE_HASH_SIZE, MyStringHash, rejectDuplicateKeys),
	_nodeIDHash			  (NODE_HASH_SIZE, hashFuncInt, rejectDuplicateKeys),
	_condorIDHash		  (NODE_HASH_SIZE, hashFuncInt, rejectDuplicateKeys),
	_storkIDHash		  (NODE_HASH_SIZE, hashFuncInt, rejectDuplicateKeys),
	_noopIDHash			  (NODE_HASH_SIZE, hashFuncInt, rejectDuplicateKeys),
    _numNodesDone         (0),
    _numNodesFailed       (0),
    _numJobsSubmitted     (0),
    _maxJobsSubmitted     (maxJobsSubmitted),
	_numIdleJobProcs		  (0),
	_maxIdleJobProcs		  (maxIdleJobProcs),
	_numHeldJobProcs	  (0),
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
	_generateSubdagSubmits (generateSubdagSubmits),
	_submitDagDeepOpts	  (submitDagDeepOpts),
	_isSplice			  (isSplice),
	_spliceScope		  (spliceScope),
	_recoveryMaxfakeID	  (0),
	_maxJobHolds		  (0),
	_reject			  (false),
	_alwaysRunPost		  (true),
	_defaultPriority	  (0),
	_use_default_node_log  (true),
	_metrics			  (NULL)
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

	_statusFileName = NULL;
	_statusFileOutdated = true;
	_minStatusUpdateTime = 0;
	_lastStatusUpdateTimestamp = 0;

	_nextSubmitTime = 0;
	_nextSubmitDelay = 1;
	_recovery = false;
	_abortOnScarySubmit = true;
	_configFile = NULL;
	_runningFinalNode = false;
		
		// Don't print any waiting node reports until we're done with
		// recovery mode.
	_pendingReportInterval = -1;
	_lastPendingNodePrintTime = 0;
	_lastEventTime = 0;

	_dagIsHalted = false;
	_dagFiles.rewind();
	_haltFile = HaltFileName( _dagFiles.next() );
	_dagStatus = DAG_STATUS_OK;
	if( submitDagDeepOpts ) {
		_use_default_node_log = submitDagDeepOpts->always_use_node_log;
	}
	_nfsLogIsError = param_boolean( "DAGMAN_LOG_ON_NFS_IS_ERROR", true );
	if(_nfsLogIsError) {
		bool userlog_locking = param_boolean( "ENABLE_USERLOG_LOCKING", true );
		if(userlog_locking) {
			bool locks_on_local = param_boolean( "CREATE_LOCKS_ON_LOCAL_DISK", true);
			if(locks_on_local) {
				dprintf( D_ALWAYS, "Ignoring value of DAGMAN_LOG_ON_NFS_IS_ERROR.\n");
				_nfsLogIsError = false;
			}
		}
	}

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

	delete[] _statusFileName;

	delete _metrics;

    return;
}

//-------------------------------------------------------------------------
void
Dag::CreateMetrics( const char *primaryDagFile, int rescueDagNum )
{
	_metrics = new DagmanMetrics( this, primaryDagFile, rescueDagNum );
}

//-------------------------------------------------------------------------
void
Dag::ReportMetrics( int exitCode )
{
	_metrics->Report( exitCode, _dagStatus );
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
				  NumNodesDone( true ) );
    
    if (recovery) {
		_recovery = true;

        debug_printf( DEBUG_NORMAL, "Running in RECOVERY mode... "
					">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n" );
		_jobstateLog.WriteRecoveryStarted();
		_jobstateLog.InitializeRecovery();

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
							_nfsLogIsError, recovery, _defaultNodeLog, _use_default_node_log ) ) {
					debug_cache_stop_caching();
					_jobstateLog.WriteRecoveryFailure();
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
				_jobstateLog.WriteRecoveryFailure();
				return false;
			}
		}
		if( StorkLogFileCount() > 0 ) {
			if( !ProcessLogEvents( DAPLOG, recovery ) ) {
				_recovery = false;
				debug_cache_stop_caching();
				_jobstateLog.WriteRecoveryFailure();
				return false;
			}
		}

		// all jobs stuck in STATUS_POSTRUN need their scripts run
		jobs.ToBeforeFirst();
		while( jobs.Next( job ) ) {
			if( job->GetStatus() == Job::STATUS_POSTRUN ) {
				if ( !RunPostScript( job, _alwaysRunPost, 0, false ) ) {
					debug_cache_stop_caching();
					_jobstateLog.WriteRecoveryFailure();
					return false;
				}
			}
		}

		set_fake_condorID( _recoveryMaxfakeID );

		debug_cache_stop_caching();

		_jobstateLog.WriteRecoveryFinished();
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

		// For performance reasons, we don't want to flush the jobstate
		// log file in recovery mode.  Outside of recovery mode, though
		// we want to do that so that the jobstate log file stays
		// current with the status of the workflow.
	if ( !_recovery ) {
		_jobstateLog.Flush();
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
			PrintEvent( DEBUG_VERBOSE, event, job, recovery );
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

				// Log this event if necessary.
			if ( job ) {
				_jobstateLog.WriteEvent( event, job );
			}

			job->SetLastEventTime( event );

				// Note: this is a bit conservative -- some events (e.g.,
				// ImageSizeUpdate) don't actually outdate the status file.
				// If we need to, we could move this down to the cases
				// where it's strictly necessary.
			_statusFileOutdated = true;

			switch(event->eventNumber) {

			case ULOG_EXECUTABLE_ERROR:
					// Don't do anything here, because we seem to always
					// also get an ABORTED event when we get an
					// EXECUTABLE_ERROR event.  (Not doing anything
					// here fixes Gnats PR 697.)
				break;

			case ULOG_JOB_ABORTED:
#if !defined(DISABLE_NODE_TIME_METRICS)
				job->TermAbortMetrics( event->proc, event->eventTime,
							_metrics );
#endif
					// Make sure we don't count finished jobs as idle.
				ProcessNotIdleEvent(job);
				ProcessAbortEvent(event, job, recovery);
				break;
              
			case ULOG_JOB_TERMINATED:
#if !defined(DISABLE_NODE_TIME_METRICS)
				job->TermAbortMetrics( event->proc, event->eventTime,
							_metrics );
#endif
					// Make sure we don't count finished jobs as idle.
				ProcessNotIdleEvent(job);
				ProcessTerminatedEvent(event, job, recovery);
				break;

			case ULOG_POST_SCRIPT_TERMINATED:
				ProcessPostTermEvent(event, job, recovery);
				break;

			case ULOG_SUBMIT:
				ProcessSubmitEvent(job, recovery, submitEventIsSane);
				ProcessIsIdleEvent(job);
				break;

			case ULOG_JOB_RECONNECT_FAILED:
			case ULOG_JOB_EVICTED:
			case ULOG_JOB_SUSPENDED:
			case ULOG_SHADOW_EXCEPTION:
				ProcessIsIdleEvent(job);
				break;

			case ULOG_JOB_HELD:
				ProcessHeldEvent(job, event);
				ProcessIsIdleEvent(job);
				break;

			case ULOG_JOB_UNSUSPENDED:
				ProcessNotIdleEvent(job);
				break;

			case ULOG_EXECUTE:
#if !defined(DISABLE_NODE_TIME_METRICS)
				job->ExecMetrics( event->proc, event->eventTime,
							_metrics );
#endif
				ProcessNotIdleEvent(job);
				break;

			case ULOG_JOB_RELEASED:
				ProcessReleasedEvent(job, event);
				break;

			case ULOG_PRESKIP:
				TerminateJob( job, recovery );
				job->UnmonitorLogFile( _condorLogRdr, _storkLogRdr );
				if(!recovery) {
					--_preRunNodeCount;
				}
					// TEMP -- we probably need to write something to the
					// jobstate.log file here, but JOB_SUCCESS is not the
					// right thing.  wenger 2011-09-01
				// _jobstateLog.WriteJobSuccessOrFailure( job );
				break;

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

			// This code is here because if a held job is removed, we
			// don't get a released event for that job.  This may not
			// work exactly right if some procs of a cluster are held
			// and some are not.  wenger 2010-08-26
		if ( job->_jobProcsOnHold > 0 && job->Release( event->proc ) ) {
			_numHeldJobProcs--;
		}

			// Only change the node status, error info,
			// etc., if we haven't already gotten an error
			// from another job proc in this job cluster
		if ( job->GetStatus() != Job::STATUS_ERROR ) {
			job->TerminateFailure();
			snprintf( job->error_text, JOB_ERROR_TEXT_MAXLEN,
				  "Condor reported %s event for job proc (%d.%d.%d)",
				  ULogEventNumberNames[event->eventNumber],
				  event->cluster, event->proc, event->subproc );
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
				debug_printf( DEBUG_QUIET, "Node %s job proc (%d.%d.%d) "
						"failed with status %d.\n", job->GetJobName(),
						event->cluster, event->proc, event->subproc,
						termEvent->returnValue );
			} else {
				debug_printf( DEBUG_QUIET, "Node %s job proc (%d.%d.%d) "
						"failed with signal %d.\n", job->GetJobName(),
						event->cluster, event->proc, event->subproc,
						termEvent->signalNumber );
			}

				// Only change the node status, error info, etc.,
				// if we haven't already gotten an error on this node.
			if ( job->GetStatus() != Job::STATUS_ERROR ) {
				if( termEvent->normal ) {
					snprintf( job->error_text, JOB_ERROR_TEXT_MAXLEN,
							"Job proc (%d.%d.%d) failed with status %d",
							termEvent->cluster, termEvent->proc,
							termEvent->subproc, termEvent->returnValue );
					job->retval = termEvent->returnValue;
					if ( job->_scriptPost != NULL) {
							// let the script know the job's exit status
						job->_scriptPost->_retValJob = job->retval;
					}
				} else {
					snprintf( job->error_text, JOB_ERROR_TEXT_MAXLEN,
							"Job proc (%d.%d.%d) failed with signal %d",
							termEvent->cluster, termEvent->proc,
							termEvent->subproc, termEvent->signalNumber );
					job->retval = (0 - termEvent->signalNumber);
					if ( job->_scriptPost != NULL) {
							// let the script know the job's exit status
						job->_scriptPost->_retValJob = job->retval;
					}
				}

				job->TerminateFailure();
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
			if ( job->GetStatus() != Job::STATUS_ERROR ) {
				job->retval = 0;
				if ( job->_scriptPost != NULL) {
						// let the script know the job's exit status
					job->_scriptPost->_retValJob = job->retval;
				}
			} else {
				debug_printf( DEBUG_NORMAL, "Node %s job proc (%d.%d.%d) "
					"completed successfully, but status is STATUS_ERROR\n",
					job->GetJobName(), termEvent->cluster, termEvent->proc,
					termEvent->subproc );
			}
			debug_printf( DEBUG_NORMAL,
							"Node %s job proc (%d.%d.%d) completed "
							"successfully.\n", job->GetJobName(),
							termEvent->cluster, termEvent->proc,
							termEvent->subproc );
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
		constraint.formatstr( "%s =?= %d && %s =?= %d",
					ATTR_DAGMAN_JOB_ID, _DAGManJobId->_cluster,
					ATTR_CLUSTER_ID, node->GetCluster() );
		args.AppendArg( constraint.Value() );
		break;

	case Job::TYPE_STORK:
		args.AppendArg( _storkRmExe );
		args.AppendArg( node->GetCluster() );
		break;

	default:
		EXCEPT( "Illegal job (%d) type for node %s", node->JobType(),
					node->GetJobName() );
		break;
	}
	
	MyString display;
	args.GetArgsStringForDisplay( &display );
	debug_printf( DEBUG_VERBOSE, "Executing: %s\n", display.Value() );
	if ( util_popen( args ) != 0 ) {
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

			// Log job success or failure if necessary.
		_jobstateLog.WriteJobSuccessOrFailure( job );
	}

	//
	// Note: structure here should be cleaned up, but I'm leaving it for
	// now to make sure parallel universe support is complete for 6.7.17.
	// wenger 2006-02-15.
	//

	if ( failed && job->_scriptPost == NULL ) {
		if( job->GetRetries() < job->GetRetryMax() ) {
			RestartNode( job, recovery );
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
				_metrics->NodeFinished( job->GetDagFile() != NULL, false );
				if ( _dagStatus == DAG_STATUS_OK ) {
					_dagStatus = DAG_STATUS_NODE_FAILED;
				}
			}
		}
		return;
	}

	if ( job->_queuedNodeJobProcs == 0 ) {
			// All procs for this job are done.
		debug_printf( DEBUG_NORMAL, "Node %s job completed\n",
				job->GetJobName() );

			// if a POST script is specified for the job, run it
		if(job->_scriptPost != NULL) {
			if ( recovery ) {
				job->SetStatus( Job::STATUS_POSTRUN );
				_postRunNodeCount++;
				(void)job->MonitorLogFile( _condorLogRdr, _storkLogRdr,
						_nfsLogIsError, _recovery, _defaultNodeLog, _use_default_node_log );
			} else {
				(void)RunPostScript( job, _alwaysRunPost, 0 );
			}
		} else if( job->GetStatus() != Job::STATUS_ERROR ) {
			// no POST script was specified, so update DAG with
			// node's successful completion if the node succeeded.
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
			job->TerminateFailure();

			int mainJobRetval = job->retval;

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

				debug_printf( DEBUG_QUIET,
					"POST for Node %s returned %d\n",
					job->GetJobName(),termEvent->returnValue);
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

				// Log post script success or failure if necessary.
			_jobstateLog.WriteScriptSuccessOrFailure( job, true );

				//
				// Deal with retries.
				//
			if( job->GetRetries() < job->GetRetryMax() ) {
				RestartNode( job, recovery );
			} else {
					// no more retries -- node failed
				_numNodesFailed++;
				_metrics->NodeFinished( job->GetDagFile() != NULL, false );
				if ( _dagStatus == DAG_STATUS_OK ) {
					_dagStatus = DAG_STATUS_NODE_FAILED;
				}

				MyString	errMsg;

				if( mainJobRetval > 0 ) {
					errMsg.formatstr( "Job exited with status %d and ",
								mainJobRetval);
				}
				else if( mainJobRetval < 0  &&
							mainJobRetval >= -MAX_SIGNAL ) {
					errMsg.formatstr( "Job died on signal %d and ",
								0 - mainJobRetval);
				}
				else {
					errMsg.formatstr( "Job failed due to DAGMAN error %d and ",
								mainJobRetval);
				}

				if ( termEvent->normal ) {
					errMsg.formatstr_cat( "POST Script failed with status %d",
								termEvent->returnValue );
				} else {
					errMsg.formatstr_cat( "POST Script died on signal %d",
								termEvent->signalNumber );
				}

				if( job->GetRetryMax() > 0 ) {
						// add # of retries to error_text
					errMsg.formatstr_cat( " (after %d node retries)",
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
				// Log post script success or failure if necessary.
			_jobstateLog.WriteScriptSuccessOrFailure( job, true );
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

		// If we're in recovery mode, we need to keep track of the
		// maximum subprocID for NOOP jobs, so we can start out at
		// the next value instead of zero.
	if ( recovery ) {
		if ( JobIsNoop( job->GetID() ) ) {
			_recoveryMaxfakeID = MAX( _recoveryMaxfakeID,
						GetIndexID( job->GetID() ) );
		}
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

		job->SetStatus( Job::STATUS_SUBMITTED );
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

	if ( !job->GetIsIdle() &&
				( job->GetStatus() == Job::STATUS_SUBMITTED ) ) {
		job->SetIsIdle(true);
		_numIdleJobProcs++;
	}

	// Do some consistency checks here.
	if ( _numIdleJobProcs > 0 && NumJobsSubmitted() < 1 ) {
        debug_printf( DEBUG_NORMAL,
					"Warning:  DAGMan thinks there are %d idle job procs, even though there are no jobs in the queue!  Setting idle count to 0 so DAG continues.\n",
					_numIdleJobProcs );
		check_warning_strictness( DAG_STRICT_2 );
		_numIdleJobProcs = 0;
	}

	debug_printf( DEBUG_VERBOSE, "Number of idle job procs: %d\n",
				_numIdleJobProcs);
}

//---------------------------------------------------------------------------
void
Dag::ProcessNotIdleEvent(Job *job) {

	if ( !job ) {
		return;
	}

	if ( job->GetIsIdle() &&
				( job->GetStatus() == Job::STATUS_SUBMITTED ) ) {
		job->SetIsIdle(false);
		_numIdleJobProcs--;
	}

		// Do some consistency checks here.
	if ( _numIdleJobProcs < 0 ) {
        debug_printf( DEBUG_NORMAL,
					"WARNING: DAGMan thinks there are %d idle job procs!\n",
					_numIdleJobProcs );
	}

	if ( _numIdleJobProcs > 0 && NumJobsSubmitted() < 1 ) {
        debug_printf( DEBUG_NORMAL,
					"Warning:  DAGMan thinks there are %d idle job procs, even though there are no jobs in the queue!  Setting idle count to 0 so DAG continues.\n",
					_numIdleJobProcs );
		check_warning_strictness( DAG_STRICT_2 );
		_numIdleJobProcs = 0;
	}

	debug_printf( DEBUG_VERBOSE, "Number of idle job procs: %d\n",
				_numIdleJobProcs);
}

//---------------------------------------------------------------------------
// We need the event here so we can tell which process has been held for
// multi-process jobs.
void
Dag::ProcessHeldEvent(Job *job, const ULogEvent *event) {

	if ( !job ) {
		return;
	}
	ASSERT( event );

	if( job->Hold( event->proc ) ) {
		_numHeldJobProcs++;
		if ( _maxJobHolds > 0 && job->_timesHeld >= _maxJobHolds ) {
			debug_printf( DEBUG_VERBOSE, "Total hold count for job %d (node %s) "
						"has reached DAGMAN_MAX_JOB_HOLDS (%d); all job "
						"proc(s) for this node will now be removed\n",
						event->cluster, job->GetJobName(), _maxJobHolds );
			RemoveBatchJob( job );
		}
	}
}

//---------------------------------------------------------------------------
void
Dag::ProcessReleasedEvent(Job *job,const ULogEvent* event) {

	ASSERT( event );
	if ( !job ) {
		return;
	}
	if( job->Release( event->proc ) ) {
		_numHeldJobProcs--;
	}
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
Job * Dag::FindNodeByEventID ( int logsource, const CondorID condorID ) const {
	if ( condorID._cluster == -1 ) {
		return NULL;
	}

	Job *	node = NULL;
	bool isNoop = JobIsNoop( condorID );
	int id = GetIndexID( condorID );
	if ( GetEventIDHash( isNoop, logsource )->lookup(id, node) != 0 ) {
			// Note: eventually get rid of the "(might be because of
			// node retries)" message here, and have code that explicitly
			// figures out whether the node was not found because of a
			// retry.  (See gittrac #1957 and #1961.)
    	debug_printf( DEBUG_VERBOSE,
					"ERROR: node for condor ID %d.%d.%d not found! "
					"(might be because of node retries)\n",
					condorID._cluster, condorID._proc, condorID._subproc);
		node = NULL;
	}

	if ( node ) {
		if ( condorID._cluster != node->GetCluster() ) {
			if ( node->GetCluster() != _defaultCondorId._cluster ) {
			 	EXCEPT( "Searched for node for cluster %d; got %d!!",
						 	condorID._cluster,
						 	node->GetCluster() );
			} else {
					// Note: we can get here if we get an aborted event
					// after a terminated event for the same job (see
					// gittrac #744 and the
					// job_dagman_abnormal_term_recovery_retries test).
				debug_printf( DEBUG_QUIET, "Warning: searched for node for "
							"cluster %d; got %d!!\n", condorID._cluster,
							node->GetCluster() );
				check_warning_strictness( DAG_STRICT_3 );
			}
		}
		ASSERT( isNoop == node->GetNoop() );
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
		node->SetStatus( Job::STATUS_PRERUN );
		_preRunNodeCount++;
		_preScriptQ->Run( node->_scriptPre );
		return true;
    }
	// no PRE script exists or is done, so add job to the queue of ready jobs
	node->FixPriority(*this);
	if ( isRetry && m_retryNodeFirst ) {
		_readyQ->Prepend( node, -node->_nodePriority );
	} else {
		if(node->_hasNodePriority){
			Job::NodeVar *var = new Job::NodeVar();
			var->_name = "priority";
			var->_value = node->_nodePriority;
			node->varsFromDag->Append( var );
		}
		if ( _submitDepthFirst ) {
			_readyQ->Prepend( node, -node->_nodePriority );
		} else {
			_readyQ->Append( node, -node->_nodePriority );
		}
	}
	return TRUE;
}

//-------------------------------------------------------------------------
bool
Dag::StartFinalNode()
{
	if ( _final_job && _final_job->GetStatus() == Job::STATUS_NOT_READY ) {
		debug_printf( DEBUG_QUIET, "Starting final node...\n" );

			// Clear out the ready queue so "regular" jobs don't run
			// when we run the final node (this is really just needed
			// for the DAG abort and DAG halt cases).
		Job* job;
		_readyQ->Rewind();
		while ( _readyQ->Next( job ) ) {
			if ( !job->GetFinal() ) {
				debug_printf( DEBUG_DEBUG_1,
							"Removing node %s from ready queue\n",
							job->GetJobName() );
				_readyQ->DeleteCurrent();
			}
		}

			// Now start up the final node.
		_final_job->SetStatus( Job::STATUS_READY );
		if ( StartNode( _final_job, false ) ) {
			_runningFinalNode = true;
			return true;
		}
	}

	return false;
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

		// Check whether the held file exists -- if so, we don't submit
		// any jobs or run scripts.
	bool prevDagIsHalted = _dagIsHalted;
	_dagIsHalted = ( access( _haltFile.Value() , F_OK ) == 0 );

	if ( _dagIsHalted ) {
		debug_printf( DEBUG_QUIET,
					"DAG is halted because halt file %s exists\n",
					_haltFile.Value() );
		if ( _runningFinalNode ) {
			debug_printf( DEBUG_QUIET,
						"Continuing to allow final node to run\n" );
		} else {
        	return numSubmitsThisCycle;
		}
	}
	if ( prevDagIsHalted ) {
		if ( !_dagIsHalted ) {
			debug_printf( DEBUG_QUIET,
						"DAG going from halted to running state\n" );
		}
			// If going from the halted to the not halted state, we need
			// to fire up any PRE scripts that were deferred while we were
			// halted.
		_preScriptQ->RunAllWaitingScripts();
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
					catThrottle->isSet() &&
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

				// Okay, we're just going to skip the sleep if we're
				// using the single workflow log file.  I think we
				// shouldn't worry about being smart in any other cases.
				// wenger 2013-01-24
			if( !_use_default_node_log && dm.submit_delay == 0 && !didLogSleep ) {
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

			} else if ( submit_result == SUBMIT_RESULT_FAILED || submit_result == SUBMIT_RESULT_NO_SUBMIT ) {
				ProcessFailedSubmit( job, dm.max_submit_attempts );
				break; // break out of while loop
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

	_preRunNodeCount--;

	bool preScriptFailed = false;
	if ( WIFSIGNALED( status ) ) {
			// if script was killed by a signal
		preScriptFailed = true;
		debug_printf( DEBUG_QUIET, "PRE Script of Job %s died on %s\n",
					  job->GetJobName(),
					  daemonCore->GetExceptionString(status) );
		snprintf( job->error_text, JOB_ERROR_TEXT_MAXLEN,
				"PRE Script died on %s",
				daemonCore->GetExceptionString(status) );
		job->retval = ( 0 - WTERMSIG(status ) );
	} else if ( WEXITSTATUS( status ) != 0 ) {
			// if script returned failure
		preScriptFailed = true;
		debug_printf( DEBUG_QUIET,
					  "PRE Script of Job %s failed with status %d\n",
					  job->GetJobName(), WEXITSTATUS(status) );
		snprintf( job->error_text, JOB_ERROR_TEXT_MAXLEN,
				"PRE Script failed with status %d",
				WEXITSTATUS(status) );
		job->retval = WEXITSTATUS( status );
	} else {
			// if script succeeded
		if( job->_scriptPost != NULL ) {
			job->_scriptPost->_retValScript = 0;
		}
		job->retval = 0;
	}

	_jobstateLog.WriteScriptSuccessOrFailure( job, false );

	if ( preScriptFailed ) {

			// Check for PRE_SKIP.
		if ( job->HasPreSkip() && job->GetPreSkip() == job->retval ) {
				// The PRE script exited with a non-zero status, but
				// because that status matches the PRE_SKIP value,
				// we're skipping the node job and the POST script
				// and considering the node successful.
			debug_printf( DEBUG_NORMAL, "PRE_SKIP return "
					"value %d indicates we are done (successfully) with node %s\n",
					job->retval, job->GetJobName() );

				// Mark the node as a skipped node.
			CondorID id;
				// This might be the first time we watch the file, so we
				// monitor it.
			if ( !job->MonitorLogFile( _condorLogRdr, _storkLogRdr, _nfsLogIsError,
					_recovery, _defaultNodeLog, _use_default_node_log ) ) {
				return 0;
			}
				
			if(!writePreSkipEvent( id, job, job->GetJobName(),
					job->GetDirectory(), _use_default_node_log ? DefaultNodeLog():
					job->GetLogFile() , !_use_default_node_log
					&& job->GetLogFileIsXml() ) ) {
				debug_printf( DEBUG_NORMAL, "Failed to write PRE_SKIP event for node %s\n",
					job->GetJobName() );
				main_shutdown_rescue( EXIT_ERROR, DAG_STATUS_ERROR );
			}
			++_preRunNodeCount; // We are not running this again, but we want
				// to keep the tables correct.  The log reading code will take 
				// care of this in a moment.
			job->_scriptPre->_done = TRUE; // So the pre script is not run again.
			job->retval = 0; // Job _is_ successful!
		}

			// Check for POST script.
		else if ( _alwaysRunPost && job->_scriptPost != NULL ) {
				// PRE script Failed.  The return code is in retval member.
			job->_scriptPost->_retValScript = job->retval;
			job->_scriptPost->_retValJob = DAG_ERROR_JOB_SKIPPED;
			RunPostScript( job, _alwaysRunPost, job->retval );
		}

			// Check for retries.
		else if( job->GetRetries() < job->GetRetryMax() ) {
			job->TerminateFailure();
			// Note: don't update count in metrics here because we're
			// retrying!
			RestartNode( job, false );
		}

			// None of the above apply -- the node has failed.
		else {
			job->TerminateFailure();
			_numNodesFailed++;
			_metrics->NodeFinished( job->GetDagFile() != NULL, false );
			if ( _dagStatus == DAG_STATUS_OK ) {
				_dagStatus = DAG_STATUS_NODE_FAILED;
			}
			if( job->GetRetryMax() > 0 ) {
				// add # of retries to error_text
				char *tmp = strnewp( job->error_text );
				snprintf( job->error_text, JOB_ERROR_TEXT_MAXLEN,
						"%s (after %d node retries)", tmp,
						job->GetRetries() );
				delete [] tmp;   
			}
		}

	} else {
		debug_printf( DEBUG_NORMAL, "PRE Script of Node %s completed "
				"successfully.\n", job->GetJobName() );
		job->retval = 0; // for safety on retries
		job->SetStatus( Job::STATUS_READY );
		if ( _submitDepthFirst ) {
			_readyQ->Prepend( job, -job->_nodePriority );
		} else {
			_readyQ->Append( job, -job->_nodePriority );
		}
	}

	CheckForDagAbort(job, "PRE script");
	// if dag abort happened, we never return here!
	return true;
}

//---------------------------------------------------------------------------
// This is the only way a POST script runs.

bool Dag::RunPostScript( Job *job, bool ignore_status, int status,
			bool incrementRunCount )
{
	// A little defensive programming. Just check that we are
	// allowed to run this script.  Because callers can be a little
	// sloppy...
	if( ( !ignore_status && status != 0 ) || !job->_scriptPost ) {
		return false;
	}
	// a POST script is specified for the job, so run it
	// We are told to ignore the result of the PRE script
	job->SetStatus( Job::STATUS_POSTRUN );
	if ( !job->MonitorLogFile( _condorLogRdr, _storkLogRdr,
			_nfsLogIsError, _recovery, _defaultNodeLog, _use_default_node_log ) ) {
		debug_printf(DEBUG_QUIET, "Unable to monitor user logfile for node %s\n",
			job->GetJobName() );
		debug_printf(DEBUG_QUIET, "Not running the POST script\n" );
		return false;
	}
	if ( incrementRunCount ) {
		_postRunNodeCount++;
	}
	_postScriptQ->Run( job->_scriptPost );

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
	return PostScriptSubReaper( job, status );
}

int 
Dag::PostScriptSubReaper( Job* job, int status )
{
	if ( job->GetStatus() != Job::STATUS_POSTRUN ) {
		EXCEPT( "Node %s is not in POSTRUN state",
			job->GetJobName() );
	}

	PostScriptTerminatedEvent e;
	
	e.dagNodeName = strnewp( job->GetJobName() );

	if( WIFSIGNALED( status ) ) {
		debug_printf( DEBUG_QUIET, "POST script died on signal %d\n", status );
		e.normal = false;
		e.signalNumber = status;
	} else {
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
		e.cluster = job->GetCluster();
		e.proc = job->GetProc();
		e.subproc = job->GetSubProc();
		ProcessPostTermEvent(&e, job, _recovery);
	} else {

		e.cluster = job->GetCluster();
		e.proc = job->GetProc();
		e.subproc = job->GetSubProc();
		WriteUserLog ulog;
			// Disabling the global log (EventLog) fixes the main problem
			// in gittrac #934 (if you can't write to the global log the
			// write to the user log also fails, and DAGMan hangs
			// waiting for the event that wasn't written).
		ulog.setEnableGlobalLog( false );
		ulog.setUseXML( !_use_default_node_log && job->GetLogFileIsXml() );
			// For NOOP jobs, we need the proc and subproc values;
			// for "real" jobs, they are not significant.
		int procID = job->GetNoop() ? job->GetProc() : 0;
		int subprocID = job->GetNoop() ? job->GetSubProc() : 0;
		const char* s = _use_default_node_log ? DefaultNodeLog() :
			job->GetLogFile();
		if( !s ) { 
				// User did not specify a log
				// We specify one for him
				// Default log is never in XML format
			s = DefaultNodeLog();
			ulog.setUseXML( false );
		}
		debug_printf( DEBUG_QUIET, "Initializing logfile %s, %d, %d, %d\n",
			s, job->GetCluster(), procID, subprocID );
		ulog.initialize( std::vector<const char*>(1,s), job->GetCluster(),
			procID, subprocID, NULL );

		for(int write_attempts = 0;;++write_attempts) {
			if( !ulog.writeEvent( &e ) ) {
				if( write_attempts >= 2 ) {
					debug_printf( DEBUG_QUIET,
							"Unable to log ULOG_POST_SCRIPT_TERMINATED event\n" );
					// Exit here, because otherwise we'll wait forever to see
					// the event that we just failed to write (see gittrac #934).
					// wenger 2009-11-12.
					main_shutdown_rescue( EXIT_ERROR, DAG_STATUS_ERROR );
				}
			} else {
				break;
			}
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
		Job* job = 0;
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
bool
Dag::FinishedRunning( bool includeFinalNode ) const
{
	if ( includeFinalNode && _final_job && !_runningFinalNode ) {
			// Make sure we don't incorrectly return true if we get called
			// just before a final node is started...
		return false;
	}

	return NumJobsSubmitted() == 0 && NumNodesReady() == 0 &&
				ScriptRunNodeCount() == 0;
}

//---------------------------------------------------------------------------
bool
Dag::DoneSuccess( bool includeFinalNode ) const
{
	if ( NumNodesDone( includeFinalNode ) == NumNodes( includeFinalNode ) ) {
		return true;
	} else if ( includeFinalNode && _final_job &&
				_final_job->GetStatus() == Job::STATUS_DONE ) {
			// Final node can override the overall DAG status.
		return true;
	}

	return false;
}

//---------------------------------------------------------------------------
bool
Dag::DoneFailed( bool includeFinalNode ) const
{
	if ( !FinishedRunning( includeFinalNode ) ) {
			// Note: if final node is running we should get to here...
		return false;
	} else if ( includeFinalNode && _final_job &&
				_final_job->GetStatus() == Job::STATUS_DONE ) {
			// Success of final node overrides failure of any other node.
		return false;
	}

	return NumNodesFailed() > 0;
}

//---------------------------------------------------------------------------
int
Dag::NumNodes( bool includeFinal ) const
{
	int result = _jobs.Number();
	if ( !includeFinal && HasFinalNode() ) {
		result--;
	}

	return result;
}

//---------------------------------------------------------------------------
int
Dag::NumNodesDone( bool includeFinal ) const
{
	int result = _numNodesDone;
	if ( !includeFinal && HasFinalNode() &&
				_final_job->GetStatus() == Job::STATUS_DONE ) {
		result--;
	}

	return result;
}

//---------------------------------------------------------------------------
// Note: the Condor part of this method essentially duplicates functionality
// that is now in schedd.cpp.  We are keeping this here for now in case
// someone needs to run a 7.5.6 DAGMan with an older schedd.
// wenger 2011-01-26
void Dag::RemoveRunningJobs ( const Dagman &dm, bool bForce) const {

	debug_printf( DEBUG_NORMAL, "Removing any/all submitted Condor/"
				"Stork jobs...\n");

	ArgList args;

		// first, remove all Condor jobs submitted by this DAGMan
		// Make sure we have at least one Condor (not Stork) job before
		// we call condor_rm...
	bool	haveCondorJob = bForce;
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

		constraint.formatstr( "%s =?= %d", ATTR_DAGMAN_JOB_ID,
					dm.DAGManJobId._cluster );
		args.AppendArg( constraint.Value() );
		if ( util_popen( args ) != 0 ) {
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
			args.AppendArg( job->GetCluster() );
			if ( util_popen( args ) != 0 ) {
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
void Dag::Rescue ( const char * dagFile, bool multiDags,
			int maxRescueDagNum, bool overwrite, bool parseFailed,
			bool isPartial ) /* const */
{
	MyString rescueDagFile;
	if ( parseFailed ) {
		rescueDagFile = dagFile;
		rescueDagFile += ".parse_failed";
	} else {
		int nextRescue = FindLastRescueDagNum( dagFile, multiDags,
					maxRescueDagNum ) + 1;
		if ( overwrite && nextRescue > 1 ) {
			nextRescue--;
		}
		if ( nextRescue > maxRescueDagNum ) {
			nextRescue = maxRescueDagNum;
		}
		rescueDagFile = RescueDagName( dagFile, multiDags, nextRescue );
	}

		// Note: there could possibly be a race condition here if two
		// DAGMans are running on the same DAG at the same time.  That
		// should be avoided by the lock file, though, so I'm not doing
		// anything about it right now.  wenger 2007-02-27

	WriteRescue( rescueDagFile.Value(), dagFile, parseFailed, isPartial );
}

static const char *RESCUE_DAG_VERSION = "2.0.1";

//-----------------------------------------------------------------------------
void Dag::WriteRescue (const char * rescue_file, const char * dagFile,
			bool parseFailed, bool isPartial) /* const */
{
	debug_printf( DEBUG_NORMAL, "Writing Rescue DAG to %s...\n",
				rescue_file );

    FILE *fp = safe_fopen_wrapper_follow(rescue_file, "w");
    if (fp == NULL) {
        debug_printf( DEBUG_QUIET, "Could not open %s for writing.\n",
					  rescue_file);
        return;
    }

	bool reset_retries_upon_rescue =
		param_boolean( "DAGMAN_RESET_RETRIES_UPON_RESCUE", true );


	if ( parseFailed ) {
    	fprintf(fp, "# \"Rescue\" DAG file, created after failure parsing\n");
    	fprintf(fp, "#   the %s DAG file\n", dagFile);
	} else {
    	fprintf(fp, "# Rescue DAG file, created after running\n");
    	fprintf(fp, "#   the %s DAG file\n", dagFile);
	}

	time_t timestamp;
	(void)time( &timestamp );
	const struct tm *tm;
	tm = gmtime( &timestamp );
	fprintf( fp, "# Created %d/%d/%d %02d:%02d:%02d UTC\n", tm->tm_mon + 1,
				tm->tm_mday, tm->tm_year + 1900, tm->tm_hour, tm->tm_min,
				tm->tm_sec );
	fprintf( fp, "# Rescue DAG version: %s (%s)\n", RESCUE_DAG_VERSION,
				isPartial ? "partial" : "full" );

    fprintf(fp, "#\n");
    fprintf(fp, "# Total number of Nodes: %d\n", NumNodes( true ));
    fprintf(fp, "# Nodes premarked DONE: %d\n", _numNodesDone);
    fprintf(fp, "# Nodes that failed: %d\n", _numNodesFailed);

    //
    // Print the names of failed Jobs
    //
    fprintf(fp, "#   ");
    ListIterator<Job> it (_jobs);
    Job * job;
    while (it.Next(job)) {
        if (job->GetStatus() == Job::STATUS_ERROR) {
            fprintf(fp, "%s,", job->GetJobName());
        }
    }
    fprintf(fp, "<ENDLIST>\n\n");

	//
	// REJECT tells DAGMan to reject this DAG if we try to run it
	// (which we shouldn't).
	//
	if ( parseFailed && !isPartial ) {
		fprintf(fp, "REJECT\n\n");
	}

	//
	// Print the CONFIG file, if any.
	//
	if ( _configFile && !isPartial ) {
    	fprintf( fp, "CONFIG %s\n\n", _configFile );
		
	}

	//
	// Print the node status file, if any.
	//
	if ( _statusFileName && !isPartial ) {
		fprintf( fp, "NODE_STATUS_FILE %s\n\n", _statusFileName );
	}

	//
	// Print the jobstate.log file, if any.
	//
	if ( _jobstateLog.LogFile() && !isPartial ) {
		fprintf( fp, "JOBSTATE_LOG %s\n\n", _jobstateLog.LogFile() );
	}

    //
    // Print per-node information.
    //
    it.ToBeforeFirst();
    while (it.Next(job)) {
		WriteNodeToRescue( fp, job, reset_retries_upon_rescue, isPartial );
    }

    //
    // Print Dependency Section
    //
	if ( !isPartial ) {
    	fprintf(fp, "\n");
    	it.ToBeforeFirst();
    	while (it.Next(job)) {

        	set<JobID_t> & _queue = job->GetQueueRef(Job::Q_CHILDREN);
        	if (!_queue.empty()) {
            	fprintf(fp, "PARENT %s CHILD", job->GetJobName());

				set<JobID_t>::const_iterator qit;
				for (qit = _queue.begin(); qit != _queue.end(); qit++) {
                	Job * child = FindNodeByNodeID( *qit );
                	ASSERT( child != NULL );
                	fprintf(fp, " %s", child->GetJobName());
				}
            	fprintf(fp, "\n");
        	}
    	}
	}

	//
	// Print "throttle by node category" settings.
	//
	if ( !isPartial ) {
		_catThrottles.PrintThrottles( fp );
	}

    fclose( fp );
}

//-----------------------------------------------------------------------------
void
Dag::WriteNodeToRescue( FILE *fp, Job *node, bool reset_retries_upon_rescue,
			bool isPartial )
{
		// Print the JOB/DATA line.
	const char *keyword = "";
	if ( node->GetFinal() ) {
		keyword = "FINAL";
	} else if ( node->JobType() == Job::TYPE_CONDOR ) {
		keyword = node->GetDagFile() ? "SUBDAG EXTERNAL" : "JOB";
	} else if( node->JobType() == Job::TYPE_STORK ) {
		keyword = "DATA";
	} else {
		EXCEPT( "Illegal node type (%d)\n", node->JobType() );
	}

	if ( !isPartial ) {
		fprintf( fp, "\n%s %s %s ", keyword, node->GetJobName(),
					node->GetDagFile() ? node->GetDagFile() :
					node->GetCmdFile() );
		if ( strcmp( node->GetDirectory(), "" ) ) {
			fprintf( fp, "DIR %s ", node->GetDirectory() );
		}
		if ( node->GetNoop() ) {
			fprintf( fp, "NOOP " );
		}
		fprintf( fp, "\n" );

			// Print the SCRIPT PRE line, if any.
		if ( node->_scriptPre != NULL ) {
			fprintf( fp, "SCRIPT PRE  %s %s\n", 
			node->GetJobName(), node->_scriptPre->GetCmd() );
		}

			// Print the PRE_SKIP line, if any.
		if ( node->HasPreSkip() != 0 ) {
			fprintf( fp, "PRE_SKIP %s %d\n", node->GetJobName(), node->GetPreSkip() );
		}

			// Print the SCRIPT POST line, if any.
		if ( node->_scriptPost != NULL ) {
			fprintf( fp, "SCRIPT POST %s %s\n", 
						node->GetJobName(), node->_scriptPost->GetCmd() );
		}

			// Print the VARS line, if any.
		if ( !node->varsFromDag->IsEmpty() ) {
			fprintf( fp, "VARS %s", node->GetJobName() );
	
			ListIterator<Job::NodeVar> vars( *node->varsFromDag );
			vars.ToBeforeFirst();
			Job::NodeVar *nodeVar;
			while ( ( nodeVar = vars.Next() ) ) {
				fprintf(fp, " %s=\"", nodeVar->_name.Value());
					// now we print the value, but we have to re-escape certain characters
				for( int i = 0; i < nodeVar->_value.Length(); i++ ) {
					char c = (nodeVar->_value)[i];
					if ( c == '\"' ) {
						fprintf( fp, "\\\"" );
					} else if (c == '\\') {
						fprintf( fp, "\\\\" );
					} else {
						fprintf( fp, "%c", c );
					}
				}
				fprintf( fp, "\"" );
			}
			fprintf( fp, "\n" );
		}

			// Print the ABORT-DAG-ON line, if any.
		if ( node->have_abort_dag_val ) {
			fprintf( fp, "ABORT-DAG-ON %s %d", node->GetJobName(),
						node->abort_dag_val );
			if ( node->have_abort_dag_return_val ) {
				fprintf( fp, " RETURN %d", node->abort_dag_return_val );
			}
			fprintf( fp, "\n" );
		}

			// Print the PRIORITY line, if any.
			// Note: when gittrac #2167 gets merged, we need to think
			// about how this code will interact with that code.
			// wenger/nwp 2011-08-24
		if ( node->_hasNodePriority ) {
			fprintf( fp, "PRIORITY %s %d\n", node->GetJobName(),
						node->_nodePriority );
		}

			// Print the CATEGORY line, if any.
		if ( node->GetThrottleInfo() ) {
			fprintf( fp, "CATEGORY %s %s\n", node->GetJobName(),
						node->GetThrottleInfo()->_category->Value() );
		}
	}

		// Never mark a FINAL node as done.
		// Also avoid a possible race condition where the job
		// has been skipped but is not yet marked as DONE.
	if ( node->GetStatus() == Job::STATUS_DONE && !node->GetFinal() ) {
		fprintf(fp, "DONE %s\n", node->GetJobName() );
	}

		// Print the RETRY line, if any.
	if( node->retry_max > 0 ) {
		int retriesLeft = (node->retry_max - node->retries);

		if ( node->GetStatus() == Job::STATUS_ERROR
					&& node->retries < node->retry_max 
					&& node->have_retry_abort_val
					&& node->retval == node->retry_abort_val ) {
			fprintf( fp, "# %d of %d retries performed; remaining attempts "
						"aborted after node returned %d\n", 
						node->retries, node->retry_max, node->retval );
		} else {
			if ( !reset_retries_upon_rescue ) {
				fprintf( fp,
							"# %d of %d retries already performed; %d remaining\n",
							node->retries, node->retry_max, retriesLeft );
			}
		}

		ASSERT( node->retries <= node->retry_max );
		if ( !reset_retries_upon_rescue ) {
			fprintf( fp, "RETRY %s %d", node->GetJobName(), retriesLeft );
		} else {
			fprintf( fp, "RETRY %s %d", node->GetJobName(), node->retry_max );
		}
		if ( node->have_retry_abort_val ) {
			fprintf( fp, " UNLESS-EXIT %d", node->retry_abort_val );
		}
		fprintf( fp, "\n" );
	}
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

	if ( job == _final_job ) {
		_runningFinalNode = false;
	}

	job->TerminateSuccess(); // marks job as STATUS_DONE
	if ( job->GetStatus() != Job::STATUS_DONE ) {
		EXCEPT( "Node %s is not in DONE state", job->GetJobName() );
	}

		// this is a little ugly, but since this function can be
		// called multiple times for the same job, we need to be
		// careful not to double-count...
	if( job->countedAsDone == false ) {
		_numNodesDone++;
		_metrics->NodeFinished( job->GetDagFile() != NULL, true );
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
								_nfsLogIsError, recovery, _defaultNodeLog, _use_default_node_log);
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
PrintEvent( debug_level_t level, const ULogEvent* event, Job* node,
			bool recovery )
{
	ASSERT( event );

	const char *recovStr = recovery ? " [recovery mode]" : "";

	if( node ) {
	    debug_printf( level, "Event: %s for %s Node %s (%d.%d.%d)%s\n",
					  event->eventName(), node->JobTypeString(),
					  node->GetJobName(), event->cluster, event->proc,
					  event->subproc, recovStr );
	} else {
        debug_printf( level, "Event: %s for unknown Node (%d.%d.%d): "
					  "ignoring...%s\n", event->eventName(),
					  event->cluster, event->proc,
					  event->subproc, recovStr );
	}
}

//-------------------------------------------------------------------------
void
Dag::RestartNode( Job *node, bool recovery )
{
    ASSERT( node != NULL );
	if ( node->GetStatus() != Job::STATUS_ERROR ) {
		EXCEPT( "Node %s is not in ERROR state", node->GetJobName() );
	}
    if( node->have_retry_abort_val && node->retval == node->retry_abort_val ) {
        debug_printf( DEBUG_NORMAL, "Aborting further retries of node %s "
                      "(last attempt returned %d)\n",
                      node->GetJobName(), node->retval);
        _numNodesFailed++;
		_metrics->NodeFinished( node->GetDagFile() != NULL, false );
		if ( _dagStatus == DAG_STATUS_OK ) {
			_dagStatus = DAG_STATUS_NODE_FAILED;
		}
        return;
    }
	node->SetStatus( Job::STATUS_READY );
	node->retries++;
	ASSERT( node->GetRetries() <= node->GetRetryMax() );
	if( node->_scriptPre ) {
		// undo PRE script completion
		node->_scriptPre->_done = false;
	}
	strcpy( node->error_text, "" );
	node->ResetJobstateSequenceNum();

	if( !recovery ) {
		debug_printf( DEBUG_VERBOSE, "Retrying node %s (retry #%d of %d)...\n",
					node->GetJobName(), node->GetRetries(),
					node->GetRetryMax() );
		StartNode( node, true );
	} else {
		debug_printf( DEBUG_VERBOSE, "Looking for retry of node %s (retry "
					"#%d of %d)...\n", node->GetJobName(), node->GetRetries(),
					node->GetRetryMax() );

			// Remove the "old" Condor ID from the ID->node hash table
			// here to fix gittrac #1957.
			// Note: the if checking against the default condor ID
			// should *always* be true here, but checking just to be safe.
		if ( !(node->GetID() == _defaultCondorId) ) {
			int logsource = node->JobType() == Job::TYPE_CONDOR ? CONDORLOG :
						DAPLOG;
			int id = GetIndexID( node->GetID() );
			if ( GetEventIDHash( node->GetNoop(), logsource )->remove( id )
						!= 0 ) {
				EXCEPT( "Event ID hash table error!" );
			}
		}

		// Doing this fixes gittrac #481 (recovery fails on a DAG that
		// has retried nodes).  (See SubmitNodeJob() for where this
		// gets done during "normal" running.)
		node->SetCondorID( _defaultCondorId );
		(void)node->MonitorLogFile( _condorLogRdr, _storkLogRdr,
					_nfsLogIsError, recovery, _defaultNodeLog, _use_default_node_log );
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
			main_shutdown_rescue( job->abort_dag_return_val,
						job->abort_dag_return_val != 0 ? DAG_STATUS_ABORT :
						DAG_STATUS_OK );
		} else {
			main_shutdown_rescue( job->retval, DAG_STATUS_ABORT );
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
Dag::SetDotFileName(const char *dot_file_name)
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
Dag::SetDotIncludeFileName(const char *include_file_name)
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

		tolerant_unlink(temp_dot_file_name.Value());
		temp_dot_file = safe_fopen_wrapper_follow(temp_dot_file_name.Value(), "w");
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
			tolerant_unlink(current_dot_file_name.Value());
			if ( rename(temp_dot_file_name.Value(),
						current_dot_file_name.Value()) != 0 ) {
				debug_printf( DEBUG_NORMAL,
					  		"Warning: can't rename temporary dot "
					  		"file (%s) to permanent file (%s): %s\n",
					  		temp_dot_file_name.Value(),
							current_dot_file_name.Value(),
					  		strerror( errno ) );
				check_warning_strictness( DAG_STRICT_1 );
			}
		}
	}
	return;
}

//===========================================================================
// Methods for node status files.
//===========================================================================

/** Set the filename of the node status file.
	@param the filename to which to dump node status information
	@param the minimum interval, in seconds, at which to update the
		status file (0 means no limit)
*/
void 
Dag::SetNodeStatusFileName( const char *statusFileName,
			int minUpdateTime )
{
	if ( _statusFileName != NULL ) {
		debug_printf( DEBUG_NORMAL, "Attempt to set NODE_STATUS_FILE "
					"to %s does not override existing value of %s\n",
					statusFileName, _statusFileName );
		return;
	}
	_statusFileName = strnewp( statusFileName );
	_minStatusUpdateTime = minUpdateTime;
}

//-------------------------------------------------------------------------
/** Dump the node status.
	@param whether the DAG has just been held
	@param whether the DAG has just been removed
*/
void
Dag::DumpNodeStatus( bool held, bool removed )
{
		//
		// Decide whether to update the file.
		//
	if ( _statusFileName == NULL ) {
		return;
	}
	
	if ( !_statusFileOutdated && !held && !removed ) {
		debug_printf( DEBUG_DEBUG_1, "Node status file not updated "
					"because it is not yet outdated\n" );
		return;
	}
	
	time_t startTime = time( NULL );
	bool tooSoon = (_minStatusUpdateTime > 0) &&
				((startTime - _lastStatusUpdateTimestamp) <
				_minStatusUpdateTime);
	if ( tooSoon && !held && !removed && !FinishedRunning( true ) ) {
		debug_printf( DEBUG_DEBUG_1, "Node status file not updated "
					"because min. status update time has not yet passed\n" );
		return;
	}

		//
		// If we made it to here, we want to actually update the
		// file.  We do that by actually writing to a temporary file,
		// and then renaming that to the "real" file, so that the
		// "real" file is always complete.
		//
	debug_printf( DEBUG_DEBUG_1, "Updating node status file\n" );

	MyString tmpStatusFile( _statusFileName );
	tmpStatusFile += ".tmp";
		// Note: it's not an error if this fails (file may not
		// exist).
	tolerant_unlink( tmpStatusFile.Value() );

	FILE *outfile = safe_fopen_wrapper_follow( tmpStatusFile.Value(), "w" );
	if ( outfile == NULL ) {
		debug_printf( DEBUG_NORMAL,
					  "Warning: can't create node status file '%s': %s\n", 
					  tmpStatusFile.Value(), strerror( errno ) );
		check_warning_strictness( DAG_STRICT_1 );
		return;
	}

		//
		// Print header.
		//
	char *timeStr = ctime( &startTime );
	char *newline = strchr(timeStr, '\n');
	if (newline != NULL) {
		*newline = 0;
	}
	fprintf( outfile, "BEGIN %lu (%s)\n",
				(unsigned long)startTime, timeStr );
	fprintf( outfile, "Status of nodes of DAG(s): " );
	char *dagFile;
	_dagFiles.rewind();
	while ( (dagFile = _dagFiles.next()) ) {
		fprintf( outfile, "%s ", dagFile );
	}
	fprintf( outfile, "\n\n" );

		//
		// Print status of all nodes.
		//
	ListIterator<Job> it ( _jobs );
	Job *node;
	while ( it.Next( node ) ) {
		const char *statusStr = Job::status_t_names[node->GetStatus()];
		const char *nodeNote = "";
		if ( node->GetStatus() == Job::STATUS_READY ) {
			if ( !node->CanSubmit() ) {
				// See Job::_job_type_names for other strings.
				statusStr = "STATUS_UNREADY  ";
			}
		} else if ( node->GetStatus() == Job::STATUS_SUBMITTED ) {
			nodeNote = node->GetIsIdle() ? "idle" : "not_idle";
			// Note: add info here about whether the job(s) are
			// held, once that code is integrated.
		} else if ( node->GetStatus() == Job::STATUS_ERROR ) {
			nodeNote = node->error_text;
		}
		fprintf( outfile, "JOB %s %s (%s)\n", node->GetJobName(),
					statusStr, nodeNote );
	}

		//
		// Print node counts.
		//
	fprintf( outfile, "\n" );
	fprintf( outfile, "Nodes total: %d\n", NumNodes( true ) );
	fprintf( outfile, "Nodes done: %d\n", NumNodesDone( true ) );
	fprintf( outfile, "Nodes pre: %d\n", PreRunNodeCount() );
	fprintf( outfile, "Nodes queued: %d\n", NumJobsSubmitted() );
	fprintf( outfile, "Nodes post: %d\n", PostRunNodeCount() );
	fprintf( outfile, "Nodes ready: %d\n", NumNodesReady() );
	int unready = NumNodes( true )  - (NumNodesDone( true ) +
				PreRunNodeCount() + NumJobsSubmitted() + PostRunNodeCount() +
				NumNodesReady() + NumNodesFailed()  );
	fprintf( outfile, "Nodes un-ready: %d\n", unready );
	fprintf( outfile, "Nodes failed: %d\n", NumNodesFailed() );

		//
		// Print overall DAG status.
		//
	Job::status_t dagStatus = Job::STATUS_SUBMITTED;
	const char *statusNote = "";
	if ( DoneSuccess( true ) ) {
		dagStatus = Job::STATUS_DONE;
		statusNote = "success";
	} else if ( DoneFailed( true ) ) {
		dagStatus = Job::STATUS_ERROR;
		statusNote = "failed";
	} else if ( DoneCycle( true ) ) {
		dagStatus = Job::STATUS_ERROR;
		statusNote = "cycle";
	} else if ( held ) {
		statusNote = "held";
	} else if ( removed ) {
		dagStatus = Job::STATUS_ERROR;
		statusNote = "removed";
	}
	fprintf( outfile, "\nDAG status: %s (%s)\n",
				Job::status_t_names[dagStatus], statusNote );

		//
		// Print footer.
		//
	time_t endTime = time( NULL );

	fprintf( outfile, "Next scheduled update: " );
	if ( FinishedRunning( true ) || removed ) {
		fprintf( outfile, "none\n" );
	} else {
		time_t nextTime = endTime + _minStatusUpdateTime;
		timeStr = ctime( &nextTime );
		newline = strchr(timeStr, '\n');
		if (newline != NULL) {
			*newline = 0;
		}
		fprintf( outfile, "%lu (%s)\n", (unsigned long)nextTime, timeStr );
	}

	timeStr = ctime( &endTime );
	newline = strchr(timeStr, '\n');
	if (newline != NULL) {
		*newline = 0;
	}
	fprintf( outfile, "END %lu (%s)\n",
				(unsigned long)endTime, timeStr );

	fclose( outfile );

		//
		// Now rename the temporary file to the "real" file.
		//
	if ( rename( tmpStatusFile.Value(), _statusFileName ) != 0 ) {
		debug_printf( DEBUG_NORMAL,
					  "Warning: can't rename temporary node status "
					  "file (%s) to permanent file (%s): %s\n",
					  tmpStatusFile.Value(), _statusFileName,
					  strerror( errno ) );
		check_warning_strictness( DAG_STRICT_1 );
		return;
	}

	_statusFileOutdated = false;
	_lastStatusUpdateTimestamp = startTime;
}

//-------------------------------------------------------------------------
void
Dag::SetReject( const MyString &location )
{
	if ( _firstRejectLoc == "" ) {
		_firstRejectLoc = location;
	}
	_reject = true;
}

//-------------------------------------------------------------------------
bool
Dag::GetReject( MyString &firstLocation )
{
	firstLocation = _firstRejectLoc;
	return _reject;
}

//===========================================================================

/** Set the filename of the jobstate.log file.
	@param the filename to which to write the jobstate log
*/
void 
Dag::SetJobstateLogFileName( const char *logFileName )
{
	if ( _jobstateLog.LogFile() != NULL ) {
		debug_printf( DEBUG_NORMAL, "Attempt to set JOBSTATE_LOG "
					"to %s does not override existing value of %s\n",
					logFileName, _jobstateLog.LogFile() );
		return;
	}
	_jobstateLog.SetLogFile( logFileName );
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
		check_warning_strictness( DAG_STRICT_3 );
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
		check_warning_strictness( DAG_STRICT_3 );
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
						node->GetJobName(), node->GetCluster(),
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
void
Dag::CheckThrottleCats()
{
	ThrottleByCategory::ThrottleInfo *info;
	_catThrottles.StartIterations();
	while ( _catThrottles.Iterate( info ) ) {
		debug_printf( DEBUG_DEBUG_1, "Category %s has %d jobs, "
					"throttle setting of %d\n", info->_category->Value(),
					info->_totalJobs, info->_maxJobs );
		ASSERT( info->_totalJobs >= 0 );
		if ( info->_totalJobs < 1 ) {
			debug_printf( DEBUG_NORMAL, "Warning: category %s has no "
						"assigned nodes, so the throttle setting (%d) "
						"will have no effect\n", info->_category->Value(),
						info->_maxJobs );
			check_warning_strictness( DAG_STRICT_2 );
		}

		if ( !info->isSet() ) {
			debug_printf( DEBUG_NORMAL, "Warning: category %s has no "
						"throttle value set\n", info->_category->Value() );
			check_warning_strictness( DAG_STRICT_2 );
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

	include_file = safe_fopen_wrapper_follow(_dot_include_file_name, "r");
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
		fclose(include_file);
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

			dot_file_name.formatstr("%s.%d", _dot_file_name, _dot_file_name_suffix);
			fp = safe_fopen_wrapper_follow(dot_file_name.Value(), "r");
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

		// Final node status is set to STATUS_NOT_READY here, so it
		// won't get run even though it has no parents; its status
		// will get changed when it should be run.
	if ( job.GetFinal() ) {
		if ( _final_job ) {
        	debug_printf( DEBUG_QUIET, "Error: DAG already has a final "
						"node %s; attempting to add final node %s\n",
						_final_job->GetJobName(), job.GetJobName() );
			return false;
		}
		job.SetStatus( Job::STATUS_NOT_READY );
		_final_job = &job;
	}
	return _jobs.Append(&job);
}

#if 0
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
		whynot.formatstr( "is active (%s)", node->GetStatusName() );
		return false;
	}
	if( !node->IsEmpty( Job::Q_CHILDREN ) ||
		!node->IsEmpty( Job::Q_PARENTS ) ) {
		whynot.formatstr( "dependencies exist" );
		return false;
	}

		// now we know it's okay to remove, and can do the deed...

	Job* candidate = NULL;
	bool removed = false;

	if( node->GetStatus() == Job::STATUS_DONE ) {
		_numNodesDone--;
		ASSERT( _numNodesDone >= 0 );
		// Need to update metrics here!!!
	}
	else if( node->GetStatus() == Job::STATUS_ERROR ) {
		_numNodesFailed--;
		ASSERT( _numNodesFailed >= 0 );
		if ( _numNodesFailed == 0 && _dagStatus == DAG_STATUS_NODE_FAILED ) {
			_dagStatus = DAG_STATUS_OK;
		}
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
		whynot.formatstr( "node in unexpected state (%s)",
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
#endif

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

	if ( event->eventNumber != ULOG_SUBMIT &&
				event->eventNumber != ULOG_PRESKIP ) {
		
	  node = FindNodeByEventID( logsource, condorID );
	  if( node ) {
	    return node;
	  }
	}

		// if the job ID wasn't familiar and we didn't find a node
		// above, there are at least four possibilites:
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
		//
		// 4) it's a pre skip event, which is handled similarly to
		// a submit event.
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
					node->SetCondorID( condorID );

						// Insert this node into the CondorID->node hash
						// table if we don't already have it (e.g., recovery
						// mode).  (In "normal" mode we should have already
						// inserted it when we did the condor_submit.)
					Job *tmpNode = NULL;
					bool isNoop = JobIsNoop( condorID );
					ASSERT( isNoop == node->GetNoop() );
					int id = GetIndexID( condorID );
					HashTable<int, Job *> *ht =
								GetEventIDHash( isNoop, logsource );
					if ( ht->lookup(id, tmpNode) != 0 ) {
							// Node not found.
						int insertResult = ht->insert( id, node );
						ASSERT( insertResult == 0 );
					} else {
							// Node was found.
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
	
	if( event->eventNumber == ULOG_PRESKIP ) {
		const PreSkipEvent* skip_event = (const PreSkipEvent*)event;
		char nodeName[1024] = "";
		if( !skip_event->skipEventLogNotes ) { 
			debug_printf( DEBUG_NORMAL, "No DAG Node indicated in a PRE_SKIP event\n" );	
			node = NULL;
		} else if( sscanf( skip_event->skipEventLogNotes, "DAG Node: %1023s",
				nodeName ) == 1) {
			node = FindNodeByName( nodeName );
			if( node ) {
				node->SetCondorID( condorID );
					// Insert this node into the CondorID->node hash
					// table.
				Job *tmpNode = NULL;
				bool isNoop = JobIsNoop( condorID );
				int id = GetIndexID( condorID );
				HashTable<int, Job *> *ht =
							GetEventIDHash( isNoop, logsource );
				if ( ht->lookup(id, tmpNode) != 0 ) {
						// Node not found.
					int insertResult = ht->insert( id, node );
					ASSERT( insertResult == 0 );
				} else {
						// Node was found.
					ASSERT( tmpNode == node );
				}
			}
		} else {
			debug_printf( DEBUG_QUIET, "ERROR: 'DAG Node:' not found "
						"in skip event notes: <%s>\n",
						skip_event->skipEventLogNotes );
		}
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
	if( node->GetID() == _defaultCondorId ) {
			// we no longer have the submit command stdout to check against
		return true;
	}

	if( condorID._cluster == node->GetCluster() ) {
		return true;
	}

	MyString message;
	message.formatstr( "ERROR: node %s: job ID in userlog submit event (%d.%d.%d) "
				"doesn't match ID reported earlier by submit command "
				"(%d.%d.%d)!", 
				node->GetJobName(), condorID._cluster, condorID._proc,
				condorID._subproc, node->GetCluster(), node->GetProc(),
				node->GetSubProc() );

	if ( _abortOnScarySubmit ) {
		debug_printf( DEBUG_QUIET, "%s  Aborting DAG; set "
					"DAGMAN_ABORT_ON_SCARY_SUBMIT to false if you are "
					"*sure* this shouldn't cause an abort.\n",
					message.Value() );
		main_shutdown_rescue( EXIT_ERROR, DAG_STATUS_ERROR );
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
Dag::GetEventIDHash(bool isNoop, int jobType)
{
	if ( isNoop ) {
		return &_noopIDHash;
	}

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
Dag::GetEventIDHash(bool isNoop, int jobType) const
{
	if ( isNoop ) {
		return &_noopIDHash;
	}

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

// A RAII class to swap out the priorities below, then restore them
// when we are done.
class priority_swapper {
public:
	priority_swapper(bool nodepriority, int newprio, int& oldprio);
	~priority_swapper();
private:
	priority_swapper(); // Not implemented
	bool swapped;
	int& oldp;
	int oldp_value;
};

priority_swapper::priority_swapper(bool nodepriority, int newprio, int& oldprio) :
	swapped(false), oldp(oldprio), oldp_value(oldprio)
{
	if( nodepriority && newprio > oldprio ) {
		swapped = true;
		oldp = newprio;
	}
}

priority_swapper::~priority_swapper()
{
	if( swapped ) {
		oldp = oldp_value;
	}
}

Dag::submit_result_t
Dag::SubmitNodeJob( const Dagman &dm, Job *node, CondorID &condorID )
{
	submit_result_t result = SUBMIT_RESULT_NO_SUBMIT;

		// Resetting the Condor ID here fixes PR 799.  wenger 2007-01-24.
	if ( node->GetCluster() != _defaultCondorId._cluster ) {
		ASSERT( JobIsNoop( condorID ) == node->GetNoop() );
		int id = GetIndexID( node->GetID() );
		int removeResult = GetEventIDHash( node->GetNoop(),
					node->JobType() )->remove( id );
		ASSERT( removeResult == 0 );
	}
	node->SetCondorID( _defaultCondorId );

		// sleep for a specified time before submitting
	if( dm.submit_delay != 0 ) {
		debug_printf( DEBUG_VERBOSE, "Sleeping for %d s "
					  "(DAGMAN_SUBMIT_DELAY) to throttle submissions...\n",
					  dm.submit_delay );
		sleep( dm.submit_delay );
	}

		// Do condor_submit_dag -no_submit if this is a nested DAG node
		// and lazy submit file generation is enabled (this must be
		// done before we try to monitor the log file).
   	if ( node->JobType() == Job::TYPE_CONDOR && !node->GetNoop() &&
				node->GetDagFile() != NULL && _generateSubdagSubmits ) {
		bool isRetry = node->GetRetries() > 0;
		priority_swapper ps( node->_hasNodePriority, node->_nodePriority, _submitDagDeepOpts->priority);
		if ( runSubmitDag( *_submitDagDeepOpts, node->GetDagFile(),
					node->GetDirectory(), isRetry ) != 0 ) {
			++node->_submitTries;
			debug_printf( DEBUG_QUIET,
						"ERROR: condor_submit_dag -no_submit failed "
						"for node %s.\n", node->GetJobName() );
				// Hmm -- should this be a node failure, since it probably
				// won't work on retry?  wenger 2010-03-26
			return SUBMIT_RESULT_NO_SUBMIT;
		}
	}

	if ( !node->MonitorLogFile( _condorLogRdr, _storkLogRdr, _nfsLogIsError,
			_recovery, _defaultNodeLog, _use_default_node_log ) ) {
		debug_printf( DEBUG_QUIET, "ERROR: Failed to monitor log for node %s.\n",
			node->GetJobName() );
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
				!node->CheckForLogFile( _use_default_node_log ) ) {
		debug_printf( DEBUG_NORMAL, "ERROR: No 'log =' value found in "
					"submit file %s for node %s\n", node->GetCmdFile(),
					node->GetJobName() );
		node->TerminateFailure();
		snprintf( node->error_text, JOB_ERROR_TEXT_MAXLEN,
					"No 'log =' value found in submit file %s",
					node->GetCmdFile() );
	  	_numNodesFailed++;
		_metrics->NodeFinished( node->GetDagFile() != NULL, false );
		if ( _dagStatus == DAG_STATUS_OK ) {
			_dagStatus = DAG_STATUS_NODE_FAILED;
		}
		result = SUBMIT_RESULT_NO_SUBMIT;

	} else {
		debug_printf( DEBUG_NORMAL, "Submitting %s Node %s job(s)...\n",
				  	node->JobTypeString(), node->GetJobName() );

    	MyString cmd_file = node->GetCmdFile();
		bool submit_success = false;

    	if( node->JobType() == Job::TYPE_CONDOR ) {
	  		node->_submitTries++;
			if ( node->GetNoop() ) {
      			submit_success = fake_condor_submit( condorID, 0,
							node->GetJobName(), node->GetDirectory(),
							_use_default_node_log ? DefaultNodeLog():
								node->GetLogFile() ,
							!_use_default_node_log && node->GetLogFileIsXml() );
			} else {
				const char *logFile = node->UsingDefaultLog() ?
							DefaultNodeLog() : NULL;
					// Note: assigning the ParentListString() return value
					// to a variable here, instead of just passing it directly
					// to condor_submit(), fixes a memory leak(!).
					// wenger 2008-12-18
				MyString parents = ParentListString( node );
      			submit_success = condor_submit( dm, cmd_file.Value(), condorID,
							node->GetJobName(), parents,
							node->varsFromDag, node->GetRetries(),
							node->GetDirectory(), DefaultNodeLog(),
							_use_default_node_log && node->UseDefaultLog(),
							logFile, ProhibitMultiJobs(),
							node->NumChildren() > 0 && dm._claim_hold_time > 0);
			}
    	} else if( node->JobType() == Job::TYPE_STORK ) {
	  		node->_submitTries++;
			if ( node->GetNoop() ) {
      			submit_success = fake_condor_submit( condorID, 0,
							node->GetJobName(), node->GetDirectory(),
							_use_default_node_log ? DefaultNodeLog() :
								node->GetLogFile(),
							!_use_default_node_log && node->GetLogFileIsXml() );

			} else {
      			submit_success = stork_submit( dm, cmd_file.Value(), condorID,
				   		node->GetJobName(), node->GetDirectory() );
			}
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

    node->SetStatus( Job::STATUS_SUBMITTED );

		// Note: I assume we're incrementing this here instead of when
		// we see the submit events so that we don't accidentally exceed
		// maxjobs (now really maxnodes) if it takes a while to see
		// the submit events.  wenger 2006-02-10.
	UpdateJobCounts( node, 1 );
    
        // stash the job ID reported by the submit command, to compare
        // with what we see in the userlog later as a sanity-check
        // (note: this sanity-check is not possible during recovery,
        // since we won't have seen the submit command stdout...)

	node->SetCondorID( condorID );
	ASSERT( JobIsNoop( node->GetID() ) == node->GetNoop() );
	int id = GetIndexID( node->GetID() );
	int insertResult = GetEventIDHash( node->GetNoop(), node->JobType() )->
				insert( id, node );
	ASSERT( insertResult == 0 );

	debug_printf( DEBUG_VERBOSE, "\tassigned %s ID (%d.%d.%d)\n",
				  node->JobTypeString(), condorID._cluster, condorID._proc,
				  condorID._subproc );
}

//---------------------------------------------------------------------------
void
Dag::ProcessFailedSubmit( Job *node, int max_submit_attempts )
{
	// This function should never be called when the Dag object is being used
	// to parse a splice.
	ASSERT( _isSplice == false );

	_jobstateLog.WriteSubmitFailure( node );

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
		if( node->_scriptPost ) {
			node->_scriptPost->_retValJob = DAG_ERROR_CONDOR_SUBMIT_FAILED;
			(void)RunPostScript( node, _alwaysRunPost, 0 );
		} else {
			node->TerminateFailure();
			_numNodesFailed++;
			_metrics->NodeFinished( node->GetDagFile() != NULL, false );
			if ( _dagStatus == DAG_STATUS_OK ) {
				_dagStatus = DAG_STATUS_NODE_FAILED;
			}
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

	debug_printf(DEBUG_DEBUG_1, "Entering: Dag::PrefixAllNodeNames()"
		" with prefix %s\n",prefix.Value());

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
	return new OwnedMaterials(nodes, &_catThrottles, _reject,
				_firstRejectLoc);
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
		AssumeOwnershipofNodes(key, om);
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
// Grab all of the nodes out of the splice, and place them into here.
// If the splice, after parsing of the dag file representing 'here', still
// have true initial or final nodes, then those must move over the the
// recorded inital and final nodes for 'here'.
void
Dag::AssumeOwnershipofNodes(const MyString &spliceName, OwnedMaterials *om)
{
	Job *job = NULL;
	int i;
	MyString key;
	JobID_t key_id;

	ExtArray<Job*> *nodes = om->nodes;

	// 0. Take ownership of the categories

	// Merge categories from the splice into this DAG object.  If the
	// same category exists in both (whether global or non-global) the
	// higher-level value overrides the lower-level value.

	// Note: by the time we get to here, all category names have already
	// been prefixed with the proper scope.
	om->throttles->StartIterations();
	ThrottleByCategory::ThrottleInfo *spliceThrottle;
	while ( om->throttles->Iterate( spliceThrottle ) ) {
		ThrottleByCategory::ThrottleInfo *mainThrottle =
					_catThrottles.GetThrottleInfo(
					spliceThrottle->_category );
		if ( mainThrottle && mainThrottle->isSet() &&
					mainThrottle->_maxJobs != spliceThrottle->_maxJobs ) {
			debug_printf( DEBUG_NORMAL, "Warning: higher-level (%s) "
						"maxjobs value of %d for category %s overrides "
						"splice %s value of %d\n", _spliceScope.Value(),
						mainThrottle->_maxJobs,
						mainThrottle->_category->Value(),
						spliceName.Value(), spliceThrottle->_maxJobs );
			check_warning_strictness( DAG_STRICT_2 );
		} else {
			_catThrottles.SetThrottle( spliceThrottle->_category,
						spliceThrottle->_maxJobs );
		}
	}

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
		spliceThrottle = tmpNode->GetThrottleInfo();
		if ( spliceThrottle != NULL ) {
				// Now re-set the category in the node, so that the
				// category info points to the upper DAG rather than the
				// splice DAG.
			tmpNode->SetCategory( spliceThrottle->_category->Value(),
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

	// 4. Copy any reject info from the splice.
	if ( om->_reject ) {
		SetReject( om->_firstRejectLoc );
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

//---------------------------------------------------------------------------
// Iterate over the jobs and set the default priority
void Dag::SetDefaultPriorities()
{
	if(GetDefaultPriority() != 0) {
		Job* job;
		_jobs.Rewind();
		while( (job = _jobs.Next()) != NULL ) {
			// If the DAG file has already assigned a priority
			// Leave this job alone for now
			if( !job->_hasNodePriority ) {
				job->_hasNodePriority = true;
				job->_nodePriority = GetDefaultPriority();
			} else if( GetDefaultPriority() > job->_nodePriority ) {
				job->_nodePriority = GetDefaultPriority();
			}
		}
	}
}
