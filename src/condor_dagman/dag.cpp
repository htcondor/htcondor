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
// of condor_submit.
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
#include "dagman_submit.h"
#include "util.h"
#include "dagman_main.h"
#include "dagman_multi_dag.h"
#include "write_user_log.h"
#include "simplelist.h"
#include "condor_string.h"  /* for strnewp() */
#include "condor_daemon_core.h"
#include <set>
#include "dagman_metrics.h"
#include "enum_utils.h"
#include <iostream>

using namespace std;

const CondorID Dag::_defaultCondorId;

const int Dag::DAG_ERROR_CONDOR_SUBMIT_FAILED = -1001;
const int Dag::DAG_ERROR_CONDOR_JOB_ABORTED = -1002;
const int Dag::DAG_ERROR_LOG_MONITOR_ERROR = -1003;
const int Dag::DAG_ERROR_JOB_SKIPPED = -1004;

// NOTE: this must be kept in sync with the DagStatus enum
const char * Dag::_dag_status_names[] = {
    "DAG_STATUS_OK",
    "DAG_STATUS_ERROR",
    "DAG_STATUS_NODE_FAILED",
    "DAG_STATUS_ABORT",
    "DAG_STATUS_RM",
    "DAG_STATUS_CYCLE",
    "DAG_STATUS_HALTED"
};

const char *Dag::ALL_NODES = "ALL_NODES";

//---------------------------------------------------------------------------
void touch (const char * filename) {
    int fd = safe_open_wrapper_follow(filename, O_RDWR | O_CREAT, 0600);
    if (fd == -1) {
        debug_error( 1, DEBUG_QUIET, "Error: can't open %s\n", filename );
    }
    close (fd);
}

//---------------------------------------------------------------------------
Dag::Dag( /* const */ std::list<std::string> &dagFiles,
		  const int maxJobsSubmitted,
		  const int maxPreScripts, const int maxPostScripts,
		  const int maxHoldScripts,
		  bool useDagDir, int maxIdleJobProcs, bool retrySubmitFirst,
		  bool retryNodeFirst, const char *condorRmExe,
		  const CondorID *DAGManJobID,
		  bool prohibitMultiJobs, bool submitDepthFirst,
		  const char *defaultNodeLog, bool generateSubdagSubmits,
		  SubmitDagDeepOptions *submitDagDeepOpts, bool isSplice,
		  DCSchedd *schedd, const MyString &spliceScope ) :
    _maxPreScripts        (maxPreScripts),
    _maxPostScripts       (maxPostScripts),
	_maxHoldScripts       (maxHoldScripts),
	MAX_SIGNAL			  (64),
	_splices              ({}),
	_dagFiles             (dagFiles),
	_useDagDir            (useDagDir),
	_final_job (0),
	_nodeNameHash		  ({}),
	_nodeIDHash			  ({}),
	_condorIDHash		  ({}),
	_noopIDHash			  ({}),
    _numNodesDone         (0),
    _numNodesFailed       (0),
    _numJobsSubmitted     (0),
    _maxJobsSubmitted     (maxJobsSubmitted),
	_numIdleJobProcs		  (0),
	_maxIdleJobProcs		  (maxIdleJobProcs),
	m_retrySubmitFirst	  (retrySubmitFirst),
	m_retryNodeFirst	  (retryNodeFirst),
	_condorRmExe		  (condorRmExe),
	_DAGManJobId		  (DAGManJobID),
	_preRunNodeCount	  (0),
	_postRunNodeCount	  (0),
	_holdRunNodeCount	  (0),
	_checkCondorEvents    (),
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
	_reject				  (false),
	_alwaysRunPost		  (true),
	_dry_run			  (0),
	_dagPriority		  (0),
	_metrics			  (NULL),
	_schedd				  (schedd)
{
	debug_printf( DEBUG_DEBUG_1, "Dag(%s)::Dag()\n", _spliceScope.c_str() );

	// If this dag is a splice, then it may have been specified with a DIR
	// directive. If so, then this records what it was so we can later
	// propogate this information to all contained nodes in the DAG--effectively
	// giving all nodes in the DAG a DIR definition with this directory
	// as a prefix to what is already there (except in the case of absolute
	// paths, in which case nothing is done).
	m_directory = ".";

	// for the toplevel dag, emit the dag files we ended up using.
	if (_isSplice == false) {
		ASSERT( dagFiles.size() >= 1 );
		PrintDagFiles( dagFiles );
	}

 	_readyQ = new PrioritySimpleList<Job*>;
	_submitQ = new std::queue<Job*>;
	if( !_readyQ || !_submitQ ) {
		EXCEPT( "ERROR: out of memory (%s:%d)!", __FILE__, __LINE__ );
	}

	/* The ScriptQ object allocates daemoncore reapers, which are a
		regulated and precious resource. Since we *know* we will never need
		this object when we are parsing a splice, we don't allocate it.
		In the other codes that expect this pointer to be valid, there is
		either an ASSERT or other checks to ensure it is valid. */
	if (_isSplice == false) {
		_preScriptQ = new ScriptQ( this );
		_postScriptQ = new ScriptQ( this );
		_holdScriptQ = new ScriptQ( this );
		if( !_preScriptQ || !_postScriptQ ) {
			EXCEPT( "ERROR: out of memory (%s:%d)!", __FILE__, __LINE__ );
		}
	} else {
		_preScriptQ = NULL;
		_postScriptQ = NULL;
		_holdScriptQ = NULL;
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
	_alwaysUpdateStatus = false;
	_lastStatusUpdateTimestamp = 0;

	_nextSubmitTime = 0;
	_nextSubmitDelay = 1;
	_recovery = false;
	_abortOnScarySubmit = true;
	_configFile = NULL;
	_finalNodeRun = false;
		
		// Don't print any waiting node reports until we're done with
		// recovery mode.
	_pendingReportInterval = -1;
	_lastPendingNodePrintTime = 0;
	_lastEventTime = 0;

	_dagIsHalted = false;
	_dagIsAborted = false;
	_haltFile = _dagmanUtils.HaltFileName( _dagFiles.front() );
	_dagStatus = DAG_STATUS_OK;

	_graph_width = 0;
	_graph_height = 0;

	return;
}

//-------------------------------------------------------------------------
Dag::~Dag()
{
	debug_printf( DEBUG_DEBUG_1, "Dag(%s)::~Dag()\n", _spliceScope.c_str() );

	if ( _condorLogRdr.activeLogFileCount() > 0 ) {
		(void) UnmonitorLogFile();
	}

	// Delete all job objects in _jobs
	for (auto *job : _jobs) {
		delete job;
	}

	// And remove them from the vector
	_jobs.clear();

    delete _preScriptQ;
    delete _postScriptQ;
	delete _holdScriptQ;
    delete _submitQ;
    delete _readyQ;

	free(_dot_file_name);
	free(_dot_include_file_name);

	free(_statusFileName);

	delete _metrics;

	DeletePinList( _pinIns );
	DeletePinList( _pinOuts );

	delete _provisionerClassad;
	_provisionerClassad = NULL;

    return;
}

//-------------------------------------------------------------------------
void
Dag::RunWaitingScripts()
{
	_preScriptQ->RunWaitingScripts();
	_postScriptQ->RunWaitingScripts();
	_holdScriptQ->RunWaitingScripts();
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
	// For some failure modes this can get called before  we created the metrics object 
	if ( ! _metrics)
		return;
	bool report_graph_metrics = param_boolean( "DAGMAN_REPORT_GRAPH_METRICS", false );
	if ( report_graph_metrics == true ) {
		if (_dagStatus != DagStatus::DAG_STATUS_CYCLE ) {
			_metrics->GatherGraphMetrics( this );
		}
	}
	(void)_metrics->Report( exitCode, _dagStatus );
}

//-------------------------------------------------------------------------
bool Dag::Bootstrap (bool recovery)
{
	// This function should never be called on a dag object which is acting
	// like a splice.
	ASSERT( _isSplice == false );

    // update dependencies for pre-completed jobs (jobs marked DONE in
    // the DAG input file)
    for (auto it = _jobs.begin(); it != _jobs.end(); it++) {
		if( (*it)->GetStatus() == Job::STATUS_DONE ) {
			TerminateJob(*it, false, true);
		}
    }
    debug_printf( DEBUG_VERBOSE, "Number of pre-completed nodes: %d\n",
				  NumNodesDone( true ) );
    
	_recovery = recovery;

	(void) MonitorLogFile();

    if (recovery) {
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

		if( CondorLogFileCount() > 0 ) {
			if( !ProcessLogEvents( recovery ) ) {
				_recovery = false;
				debug_cache_stop_caching();
				_jobstateLog.WriteRecoveryFailure();
				return false;
			}
		}

		// all jobs stuck in STATUS_POSTRUN need their scripts run
		for (auto it = _jobs.begin(); it != _jobs.end(); it++) {
			if( (*it)->GetStatus() == Job::STATUS_POSTRUN ) {
				if ( !RunPostScript( *it, _alwaysRunPost, 0, false ) ) {
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
    
		// If we have a provisioner job, submit that before any other jobs
	if( HasProvisionerNode() ) {
		StartProvisionerNode();
	}
		// Note: we're bypassing the ready queue here...
	else {
		for (auto it = _jobs.begin(); it != _jobs.end(); it++) {
			if( (*it)->CanSubmit() ) {
				StartNode( *it, false );
			}
		}
	}

    return true;
}

#ifdef DEAD_CODE
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
#endif

//-------------------------------------------------------------------------
Job * Dag::FindNodeByNodeID (const JobID_t jobID) const {
	Job *	job = NULL;
	auto findResult = _nodeIDHash.find( jobID );
	if ( findResult == _nodeIDHash.end() ) {
    	debug_printf( DEBUG_NORMAL, "ERROR: job %d not found!\n", jobID);
		dprintf(D_ALWAYS | D_BACKTRACE, "caller info");
	}
	else {
		job = (*findResult).second;
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
ReadUserLog::FileStatus
Dag::GetCondorLogStatus () {

	ReadUserLog::FileStatus status = _condorLogRdr.GetLogStatus();

		//
		// Print the nodes we're waiting for if the time threshold
		// since the last event has been exceeded, and we also haven't
		// printed a report in that time.
		//
	time_t		currentTime;
	time( &currentTime );

	if ( status == ReadUserLog::LOG_STATUS_GROWN ) _lastEventTime = currentTime;
	time_t		elapsedEventTime = currentTime - _lastEventTime;
	time_t		elapsedPrintTime = currentTime - _lastPendingNodePrintTime;

	if ( (int)elapsedEventTime >= _pendingReportInterval &&
				(int)elapsedPrintTime >= _pendingReportInterval ) {
		debug_printf( DEBUG_NORMAL, "%d seconds since last log event\n",
					(int)elapsedEventTime );
		PrintPendingNodes();

		_lastPendingNodePrintTime = currentTime;
	}

    return status;
}

//-------------------------------------------------------------------------
// Developer's Note: returning false tells main_timer to abort the DAG
bool Dag::ProcessLogEvents (bool recovery) {

	debug_printf( DEBUG_VERBOSE, "Currently monitoring %d HTCondor "
				"log file(s)\n", _condorLogRdr.activeLogFileCount() );

	bool done = false;  // Keep scanning until ULOG_NO_EVENT
	bool result = true;

	while (!done) {
		ULogEvent* e = NULL;
		ULogEventOutcome outcome = ULOG_NO_EVENT;

		outcome = _condorLogRdr.readEvent(e);

		bool tmpResult = ProcessOneEvent( outcome, e, recovery,
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
		const char *name = "HTCondor";
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
bool Dag::ProcessOneEvent (ULogEventOutcome outcome,
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
			Job *job = LogEventNodeLookup( event,
						submitEventIsSane );
			PrintEvent( DEBUG_VERBOSE, event, job, recovery );
			if( !job ) {
					// event is for a job outside this DAG; ignore it
				break;
			}
			if( !EventSanityCheck( event, job, &result ) ) {
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
					// Make sure we don't count finished jobs as idle.
				ProcessNotIdleEvent( job, event->proc );
				ProcessAbortEvent(event, job, recovery);
				break;
              
			case ULOG_JOB_TERMINATED:
					// Make sure we don't count finished jobs as idle.
				ProcessNotIdleEvent( job, event->proc );
				ProcessTerminatedEvent(event, job, recovery);
				break;

			case ULOG_POST_SCRIPT_TERMINATED:
				ProcessPostTermEvent(event, job, recovery);
				break;

			case ULOG_SUBMIT:
				ProcessSubmitEvent(job, recovery, submitEventIsSane);
				ProcessIsIdleEvent( job, event->proc );
				break;

			case ULOG_JOB_RECONNECT_FAILED:
			case ULOG_JOB_EVICTED:
			case ULOG_JOB_SUSPENDED:
			case ULOG_SHADOW_EXCEPTION:
				ProcessIsIdleEvent( job, event->proc );
				break;

			case ULOG_JOB_HELD:
				ProcessHeldEvent(job, event);
				ProcessIsIdleEvent(job, event->proc);
				break;

			case ULOG_JOB_UNSUSPENDED:
				ProcessNotIdleEvent( job, event->proc );
				break;

			case ULOG_EXECUTE:
				ProcessNotIdleEvent( job, event->proc );
				break;

			case ULOG_JOB_RELEASED:
				ProcessReleasedEvent(job, event);
				break;

			case ULOG_PRESKIP:
				TerminateJob( job, recovery );
				if(!recovery) {
					--_preRunNodeCount;
				}
					// TEMP -- we probably need to write something to the
					// jobstate.log file here, but JOB_SUCCESS is not the
					// right thing.  wenger 2011-09-01
				// _jobstateLog.WriteJobSuccessOrFailure( job );
				break;

			case ULOG_CLUSTER_SUBMIT:
				ProcessClusterSubmitEvent(job);
				break;

			case ULOG_CLUSTER_REMOVE:
				ProcessClusterRemoveEvent(job, recovery);
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

  // NOTE: while there are known HTCondor bugs leading to a "double"
  // terminate-then-abort event pair for the same job proc abortion,
  // this method will no longer actually see the second (abort) event,
  // because the event-checking code will suppress it.  Therefore, this
  // code no longer has to worry about it.  Note, though, that we can
  // still see a combination of terminated and aborted events for the
  // same *job* (not job proc).

	if ( job ) {

		job->SetProcEvent( event->proc, ABORT_TERM_MASK );

		DecrementProcCount( job );

			// This code is here because if a held job is removed, we
			// don't get a released event for that job.  This may not
			// work exactly right if some procs of a cluster are held
			// and some are not.  wenger 2010-08-26
		if ( job->_jobProcsOnHold > 0 ) {
			job->Release( event->proc );
		}

			// Only change the node status, error info,
			// etc., if we haven't already gotten an error
			// from another job proc in this job cluster
		if ( job->GetStatus() != Job::STATUS_ERROR ) {
			job->TerminateFailure();
			job->error_text.formatstr (
				  "HTCondor reported %s event for job proc (%d.%d.%d)",
				  event->eventName(),
				  event->cluster, event->proc, event->subproc );
			job->retval = DAG_ERROR_CONDOR_JOB_ABORTED;
			
			// It seems like we should be checking _numSubmittedProcs here, but
			// that breaks a test in Windows. Keep an eye on this in case we
			// have trouble recovering from aborted jobs using late materialization.
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

		job->SetProcEvent( event->proc, ABORT_TERM_MASK );

		DecrementProcCount( job );

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
					job->error_text.formatstr(
							"Job proc (%d.%d.%d) failed with status %d",
							termEvent->cluster, termEvent->proc,
							termEvent->subproc, termEvent->returnValue );
					job->retval = termEvent->returnValue;
					if ( job->_scriptPost != NULL) {
							// let the script know the job's exit status
						job->_scriptPost->_retValJob = job->retval;
					}
				} else {
					job->error_text.formatstr(
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

		ProcessJobProcEnd( job, recovery, failed );

		if( job->_scriptPost == NULL ) {
			bool abort = CheckForDagAbort(job, "job");
			// if dag abort happened, we never return here!
			if( abort ) {
				return;
			}
		}

		PrintReadyQ( DEBUG_DEBUG_2 );

		return;
	}
}

//---------------------------------------------------------------------------
void
Dag::RemoveBatchJob(Job *node) {

	ArgList args;
	MyString constraint;

	args.AppendArg( _condorRmExe );
	args.AppendArg(node->GetCluster());
	args.AppendArg( "-const" );

		// Adding this DAGMan's cluster ID as a constraint to
		// be extra-careful to avoid removing someone else's
		// job.
	constraint.formatstr(ATTR_DAGMAN_JOB_ID "==%d", _DAGManJobId->_cluster);
	args.AppendArg( constraint.c_str() );
	
	MyString display;
	args.GetArgsStringForDisplay( &display );
	debug_printf( DEBUG_VERBOSE, "Executing: %s\n", display.c_str() );
	if ( _dagmanUtils.popen( args ) != 0 ) {
			// Note: error here can't be fatal because there's a
			// race condition where you could do a condor_rm on
			// a job that already terminated.  wenger 2006-02-08.
		debug_printf( DEBUG_NORMAL, "Error removing DAG node jobs\n");
	}
}

//---------------------------------------------------------------------------
// Note:  if job is the final node of the DAG, should we set _dagStatus
// in here according to whether job succeeded or failed?  wenger 2014-03-18
void
Dag::ProcessJobProcEnd(Job *job, bool recovery, bool failed) {

	// This function should never be called when the dag object is
	// being used to parse a splice.
	ASSERT ( _isSplice == false );

	if ( job->_queuedNodeJobProcs == 0 && !job->is_cluster  ) {
			// Log job success or failure if necessary.
		_jobstateLog.WriteJobSuccessOrFailure( job );
	}

	//
	// Note: structure here should be cleaned up, but I'm leaving it for
	// now to make sure parallel universe support is complete for 6.7.17.
	// wenger 2006-02-15.
	//
	bool putFailedJobsOnHold = param_boolean("DAGMAN_PUT_FAILED_JOBS_ON_HOLD", false);
	if ( failed && job->_scriptPost == NULL ) {
		if ( job->DoRetry() ) {
			// If this is a cluster job with multiple procs, do not restart it now.
			// That should happen in ProcessClusterRemoveEvent().
			if ( !job->is_cluster ) {
				RestartNode( job, recovery );
			}
		} else if ( putFailedJobsOnHold ) {
			job->SetHold( true );
			// Increase the job's retry max, so it will try again after the
			// retry count gets increased in the RestartNode() function.
			// We might want to limit this to avoid livelock.
			job->SetRetryMax( job->GetRetryMax() + 1 );
			RestartNode( job, recovery );
		} else {
				// no more retries -- job failed
			if( job->GetRetryMax() > 0 ) {
					// add # of retries to error_text
				job->error_text.formatstr_cat( " (after %d node retries)",
						job->GetRetries() );
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

	// If this is *not* a multi-proc cluster job, and no more procs are
	// outstanding, start shutting things down now.
	// Multi-proc cluster jobs get shut down in ProcessClusterRemoveEvent().
	if ( job->_queuedNodeJobProcs == 0 && !job->is_cluster ) {
			// All procs for this job are done.
			debug_printf( DEBUG_NORMAL, "Node %s job completed\n",
				job->GetJobName() );

			// if a POST script is specified for the job, run it
		if(job->_scriptPost != NULL) {
			if ( recovery ) {
				job->SetStatus( Job::STATUS_POSTRUN );
				_postRunNodeCount++;
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

		MyString header;
		header.formatstr( "POST Script of node %s ", job->GetJobName() );
		if( !(termEvent->normal && termEvent->returnValue == 0) ) {
				// POST script failed or was killed by a signal
			job->TerminateFailure();

			int mainJobRetval = job->retval;

			MyString errStr;

			if( termEvent->normal ) {
					// Normal termination -- POST script failed
				errStr.formatstr( "failed with status %d",
							termEvent->returnValue );
				debug_printf( DEBUG_NORMAL, "%s%s\n", header.c_str(),
							errStr.c_str() );
				debug_printf( DEBUG_QUIET,
					"POST for Node %s returned %d\n",
					job->GetJobName(),termEvent->returnValue);

				job->retval = termEvent->returnValue;
			} else {
					// Abnormal termination -- POST script killed by signal
				errStr.formatstr( "died on signal %d",
							termEvent->signalNumber );
				debug_printf( DEBUG_NORMAL,
							"%s%s\n", header.c_str(), errStr.c_str() );

				job->retval = (0 - termEvent->signalNumber);
			}

			job->error_text.formatstr(
						"POST script %s", errStr.c_str() );

				// Log post script success or failure if necessary.
			_jobstateLog.WriteScriptSuccessOrFailure( job, ScriptType::POST );

				//
				// Deal with retries.
				//
			if ( job->DoRetry() ) {
				RestartNode( job, recovery );
			} else {
					// no more retries -- node failed
				_numNodesFailed++;
				_metrics->NodeFinished( job->GetDagFile() != NULL, false );
				if ( _dagStatus == DAG_STATUS_OK ) {
					_dagStatus = DAG_STATUS_NODE_FAILED;
				}

				if( mainJobRetval > 0 ) {
					job->error_text.formatstr( "Job exited with status %d and ",
								mainJobRetval );
				}
				else if( mainJobRetval < 0  &&
							mainJobRetval >= -MAX_SIGNAL ) {
					job->error_text.formatstr( "Job died on signal %d and ",
								0 - mainJobRetval );
				}
				else {
					job->error_text.formatstr( "Job failed due to DAGMAN error %d and ",
								mainJobRetval );
				}

				if ( termEvent->normal ) {
					job->error_text.formatstr_cat( "POST Script failed with status %d",
								termEvent->returnValue );
				} else {
					job->error_text.formatstr_cat( "POST Script died on signal %d",
								termEvent->signalNumber );
				}

				if ( job->GetRetryMax() > 0 ) {
						// add # of retries to error_text
					job->error_text.formatstr_cat( " (after %d node retries)",
							job->GetRetries() );
				}
			}

		} else {
				// POST script succeeded.
			ASSERT( termEvent->returnValue == 0 );
			debug_printf( DEBUG_NORMAL,
						"%scompleted successfully.\n", header.c_str() );
			job->retval = 0;
				// Log post script success or failure if necessary.
			_jobstateLog.WriteScriptSuccessOrFailure( job, ScriptType::POST );
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

	if ( job->IsWaiting() ) {
		debug_printf( DEBUG_QUIET, "Error: DAG semantics violated!  "
					"Node %s was submitted but has unfinished parents!\n",
					job->GetJobName() );
		const char *dagFile = _dagFiles.front().c_str();
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
		// HTCondor ID, we won't decrement the counts at that time.
		//
	if ( submitEventIsSane || job->GetStatus() != Job::STATUS_SUBMITTED ) {
		job->_queuedNodeJobProcs++;
		job->_numSubmittedProcs++;
	}

		// Note:  in non-recovery mode, we increment _numJobsSubmitted
		// in ProcessSuccessfulSubmit().
	if ( recovery ) {
		if ( submitEventIsSane || job->GetStatus() != Job::STATUS_SUBMITTED ) {
				// Only increment the submitted job count on
				// the *first* proc of a job.
			if( job->_numSubmittedProcs == 1 ) {
				UpdateJobCounts( job, 1 );
			}
		}

		job->SetStatus( Job::STATUS_SUBMITTED );
		return;
	}

		// If we only have one log, compare the order of submit events in the
		// log to the order in which we submitted the jobs -- but if we
		// have >1 userlog we can't count on any order, so we can't sanity-check
		// in this way.
		// What happens if we have more than one log? Will the DAG run
		// correctly without hitting this code block?
	if ( TotalLogFileCount() == 1 ) {

			// Only perform sanity check on the first proc in a job cluster.
		if( job->_numSubmittedProcs == 1 ) {

				// as a sanity check, compare the job from the
				// submit event to the job we expected to see from
				// our submit queue
			Job* expectedJob = NULL;
			if ( _submitQ->empty() ) {
				debug_printf( DEBUG_QUIET,
						"Unrecognized submit event (for job "
						"\"%s\") found in log (none expected)\n",
						job->GetJobName() );
				return;
			}
			expectedJob = _submitQ->front();
			_submitQ->pop();
			if ( job != expectedJob ) {
				ASSERT( expectedJob != NULL );
				debug_printf( DEBUG_QUIET,
						"Unexpected submit event (for job "
						"\"%s\") found in log; job \"%s\" "
						"was expected.\n",
						job->GetJobName(),
						expectedJob->GetJobName() );
				// put expectedJob back onto submit queue
				_submitQ->push( expectedJob );
				return;
			}
		}
	}

		// If this is a provisioner node, initialize the classad object
		// that communicates with the schedd.
	if ( job->GetType() == NodeType::PROVISIONER ) {
		CondorID provisionerId = CondorID( job->GetCluster(), job->GetProc(), 0 );
		_provisionerClassad = new ProvisionerClassad( provisionerId, _schedd );
	}

	PrintReadyQ( DEBUG_DEBUG_2 );
}

//---------------------------------------------------------------------------
void
Dag::ProcessIsIdleEvent( Job *job, int proc ) {

	if ( !job ) {
		return;
	}

		// Note:  we need to make sure here that the job proc isn't already
		// idle so we don't count it twice if, for example, we get a hold
		// event for a job that's already idle.
	if ( !job->GetProcIsIdle( proc ) &&
				( job->GetStatus() == Job::STATUS_SUBMITTED ) ) {
		job->SetProcIsIdle( proc, true );
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
Dag::ProcessNotIdleEvent( Job *job, int proc ) {

	if ( !job ) {
		return;
	}

	if ( job->GetProcIsIdle( proc ) &&
				( ( job->GetStatus() == Job::STATUS_SUBMITTED ) ||
				( job->GetStatus() == Job::STATUS_ERROR ) ) ) {
		job->SetProcIsIdle( proc, false );
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

	job->SetProcEvent( proc, EXEC_MASK );

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

	const JobHeldEvent *heldEvent = (const JobHeldEvent *)event;
	const char *reason = heldEvent->getReason() ?
				heldEvent->getReason() : "(unknown)";
	debug_printf( DEBUG_VERBOSE, "  Hold reason: %s\n", reason );

	if( job->Hold( event->proc ) ) {
		if ( _maxJobHolds > 0 && job->_jobProcsOnHold >= _maxJobHolds ) {
			debug_printf( DEBUG_VERBOSE, "Total hold count for job %d (node %s) "
						"has reached DAGMAN_MAX_JOB_HOLDS (%d); all job "
						"proc(s) for this node will now be removed\n",
						event->cluster, job->GetJobName(), _maxJobHolds );
			RemoveBatchJob( job );
		}
	}

	if( job->_scriptHold != NULL ) {
		// TODO: Does running a HOLD script warrant a separate helper function?
		RunHoldScript( job, true );
	}
}

//---------------------------------------------------------------------------
void
Dag::ProcessReleasedEvent(Job *job,const ULogEvent* event) {

	ASSERT( event );
	if ( !job ) {
		return;
	}
	job->Release( event->proc );
}

//---------------------------------------------------------------------------
void
Dag::ProcessClusterSubmitEvent(Job *job) {

	if ( !job ) {
		return;
	}
	job->is_cluster = true;
}

//---------------------------------------------------------------------------
void
Dag::ProcessClusterRemoveEvent(Job *job, bool recovery) {

	if ( !job ) {
		return;
	}

	// Make sure the job is a multi-proc cluster, and has no more queued procs. 
	// Otherwise something is wrong.
	if ( job->_queuedNodeJobProcs == 0 && job->is_cluster ) {
		// All procs for this job are done.
		debug_printf( DEBUG_NORMAL, "Node %s job completed\n",
			job->GetJobName() );
		// If a post script is defined for this job, that will run after we
		// receive the ClusterRemove event. In that case, run the post script
		// now and don't call TerminateJob(); that will get called later by
		// ProcessPostTermEvent().
		if( job->_scriptPost != NULL ) {
			if ( recovery ) {
				job->SetStatus( Job::STATUS_POSTRUN );
				_postRunNodeCount++;
			} else {
				(void)RunPostScript( job, _alwaysRunPost, 0 );
			}
		} else if( job->GetStatus() != Job::STATUS_ERROR ) {
			// no POST script was specified, so update DAG with
			// node's successful completion if the node succeeded.
			TerminateJob( job, recovery );
		}
	}
	else {
		debug_printf(DEBUG_NORMAL, "ERROR: ProcessClusterRemoveEvent() called"
			" for job %s although %d procs still queued.\n", 
			job->GetJobName(), job->_queuedNodeJobProcs);
	}

	// If this cluster did not complete successfully, restart it.
	if( job->GetStatus() == Job::STATUS_ERROR ) {
		if ( job->DoRetry() ) {
			RestartNode( job, recovery );
		}
	}

	// Cleanup the job and write succcess/failure to the log. 
	// For non-cluster jobs, this is done in DecrementProcCount
	_jobstateLog.WriteJobSuccessOrFailure( job );
	UpdateJobCounts( job, -1 );
	job->Cleanup();
}

//---------------------------------------------------------------------------
Job * Dag::FindNodeByName (const char * jobName) const {
	if( !jobName ) {
		return NULL;
	}

	Job *	job = NULL;
	auto findResult = _nodeNameHash.find( jobName );
	if ( findResult == _nodeNameHash.end() ) {
		debug_printf( DEBUG_VERBOSE, "ERROR: job %s not found!\n", jobName);
	}
	else {
		job = ( *findResult ).second;
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
Job *
Dag::FindAllNodesByName( const char* nodeName,
			const char *finalSkipMsg, const char *file, int line ) const
{
	bool skipFinalNode = true;
	Job *node = NULL;
	if ( nodeName ) {
		if ( strcasecmp( nodeName, ALL_NODES ) ) {
				// Looking for a specific node.
			_allNodesIt = _jobs.end(); 
				// Specific node lookups should not skip the final node.
				// TODO: Some commands (RETRY) should skip the final node,
				// but others (SCRIPT) should not. Find a better way to do this.
			skipFinalNode = false;
			node =  FindNodeByName( nodeName );
		} else {
				// First call when looking for ALL_NODES.
			_allNodesIt = _jobs.begin();
			node = *_allNodesIt++;
		}

	} else {
			// Second or subsequent call when looking for ALL_NODES.
		if ( _allNodesIt != _jobs.end() ) {
			node =  *_allNodesIt++;
		} else {
			node =  NULL;
		}
	}

		// We want to skip final nodes if we're in ALL_NODES mode.
	if ( node && node->GetType() == NodeType::FINAL && skipFinalNode ) {
		debug_printf( DEBUG_NORMAL, finalSkipMsg, node->GetJobName(),
					file, line );
			// There can only be one FINAL node. 
			// If there are more _jobs left in the container, advance the iterator.
		if ( _allNodesIt != _jobs.end() ) {
			node = *_allNodesIt++;
		}
			// If no more jobs left, return node = NULL
		else {
			node = NULL;
		}
	}

	return node;
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
	Job *job = NULL;
	auto findResult = _nodeNameHash.find( nodeName );
	if ( findResult == _nodeNameHash.end() ) {
		return false;
	}
	else {
		job = ( *findResult ).second;
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
Job * Dag::FindNodeByEventID ( const CondorID condorID ) const {
	if ( condorID._cluster == -1 ) {
		return NULL;
	}

	Job *	node = NULL;
	bool isNoop = JobIsNoop( condorID );
	int id = GetIndexID( condorID );
	auto findResult = GetEventIDHash( isNoop )->find( id );
	if ( findResult == GetEventIDHash( isNoop )->end() ) {
			// Note: eventually get rid of the "(might be because of
			// node retries)" message here, and have code that explicitly
			// figures out whether the node was not found because of a
			// retry.  (See gittrac #1957 and #1961.)
    	debug_printf( DEBUG_VERBOSE,
					"ERROR: node for condor ID %d.%d.%d not found! "
					"(might be because of node retries)\n",
					condorID._cluster, condorID._proc, condorID._subproc);
	}
	else {
		node = (*findResult).second;
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
}

//-------------------------------------------------------------------------
void
Dag::PrintDagFiles( /* const */ std::list<std::string> &dagFiles )
{
	if ( dagFiles.size() > 1 ) {
		debug_printf( DEBUG_VERBOSE, "All DAG files:\n");
		int thisDagNum = 0;
		for ( auto it = dagFiles.begin(); it != dagFiles.end(); ++it ) {
			debug_printf( DEBUG_VERBOSE, "  %s (DAG #%d)\n", it->c_str(),
						thisDagNum++);
		}
	}
}

//-------------------------------------------------------------------------
bool
Dag::StartNode( Job *node, bool isRetry )
{
	ASSERT( !_finalNodeRun );
    ASSERT( node != NULL );

	if ( !node->CanSubmit() ) {
		EXCEPT( "Node %s not ready to submit!", node->GetJobName() );
	}

	// We should never be calling this function when the dag is being used
	// as a splicing dag.
	ASSERT( _isSplice == false );

	// if a PRE script exists and hasn't already been run, run that
	// first -- the PRE script's reaper function will submit the
	// actual job to HTCondor if/when the script exits successfully
    if( node->_scriptPre && node->_scriptPre->_done == FALSE ) {
		node->SetStatus( Job::STATUS_PRERUN );
		_preRunNodeCount++;
		_preScriptQ->Run( node->_scriptPre );
		return true;
    }

	// no PRE script exists or is done, so add job to the queue of ready jobs
	if ( isRetry && m_retryNodeFirst ) {
		_readyQ->Prepend( node, -node->_effectivePriority );
	} else {
		if ( _submitDepthFirst ) {
			_readyQ->Prepend( node, -node->_effectivePriority );
		} else {
			_readyQ->Append( node, -node->_effectivePriority );
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
			// Note:  maybe we should change nodes in the prerun state
			// to not ready here, to be more consistent.  But I'm not
			// dealing with that for now.  wenger 2014-03-17
		Job* job;
		_readyQ->Rewind();
		while ( _readyQ->Next( job ) ) {
			if ( !(job->GetType() == NodeType::FINAL) ) {
				debug_printf( DEBUG_DEBUG_1,
							"Removing node %s from ready queue\n",
							job->GetJobName() );
				_readyQ->DeleteCurrent();
				job->SetStatus( Job::STATUS_NOT_READY );
			}
		}

			// Now start up the final node.
		_final_job->SetStatus( Job::STATUS_READY );
		if ( StartNode( _final_job, false ) ) {
			_finalNodeRun = true;
			return true;
		}
	}

	return false;
}

//-------------------------------------------------------------------------
bool
Dag::StartProvisionerNode()
{
	if ( _provisioner_node && _provisioner_node->GetStatus() == Job::STATUS_READY ) {
		debug_printf( DEBUG_QUIET, "Starting provisioner node...\n" );
		if ( StartNode( _provisioner_node, false ) ) {
			return true;
		}
	}

	return false;
}

//-------------------------------------------------------------------------
int
Dag::GetProvisionerJobAdState()
{
	int provisionerState = -1;
	if (_provisionerClassad) {
		provisionerState = _provisionerClassad->GetProvisionerState();
	}
	return provisionerState;
}

//-------------------------------------------------------------------------
// returns number of jobs submitted
int
Dag::SubmitReadyJobs(const Dagman &dm)
{
	debug_printf( DEBUG_DEBUG_1, "Dag::SubmitReadyJobs()\n" );
	time_t cycleStart = time( NULL );

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
	_dagIsHalted = ( access( _haltFile.c_str() , F_OK ) == 0 );

	if ( _dagIsHalted ) {
		debug_printf( DEBUG_QUIET,
					"DAG is halted because halt file %s exists\n",
					_haltFile.c_str() );
		if ( _finalNodeRun ) {
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
		_preScriptQ->RunWaitingScripts();
	}

		// Check if we are waiting for a provisioner node to become ready
	if ( HasProvisionerNode() && !_provisioner_ready ) {
			// If we just moved into a provisioned state, we can start
			// submitting the other jobs in the dag
		if ( GetProvisionerJobAdState() == ProvisionerState::PROVISIONING_COMPLETE ) {
			_provisioner_ready = true;
			for (auto it = _jobs.begin(); it != _jobs.end(); it++) {
				if( (*it)->CanSubmit() ) {
					StartNode( *it, false );
				}
			}
		}
	}

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

			// Check whether this submit cycle is taking too long (only if we
			// are not in aggressive submit mode)
		if( !dm.aggressive_submit ) {
			time_t now = time( NULL );
			time_t elapsed = now - cycleStart;
			if ( elapsed > dm.m_user_log_scan_interval ) {
				debug_printf( DEBUG_QUIET,
					"Warning: Submit cycle elapsed time (%d s) has exceeded "
					"log scan interval (%d s); bailing out of submit loop\n",
					(int)elapsed, dm.m_user_log_scan_interval );
				break; // break out of while loop
			}
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
						job->GetJobName(), catThrottle->_category->c_str(),
						catThrottle->_maxJobs );
			deferredJobs.Prepend( job, -job->_effectivePriority );
			_catThrottleDeferredCount++;
		} else if (_dry_run) {
			// Don't actually submit the job. Just terminate it right away
			debug_printf(DEBUG_NORMAL, "Processing job: %s\n", job->GetJobName());
			TerminateJob(job, false, false);
			numSubmitsThisCycle++;
		} else {

				// Note:  I'm not sure why we don't just use the default
				// constructor here.  wenger 2015-09-25
			CondorID condorID( 0, 0, 0 );
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
				EXCEPT( "Illegal submit_result_t value: %d", submit_result );
			}
		}
	}

	// if we didn't actually invoke condor_submit, and we submitted any jobs
	// we should now send a reschedule command
	if (numSubmitsThisCycle > 0 && !_dry_run)
	{
		send_reschedule(dm);
	}

		// Put any deferred jobs back into the ready queue for next time.
	deferredJobs.Rewind();
	Job *job;
	while ( deferredJobs.Next( job ) ) {
		debug_printf( DEBUG_DEBUG_1,
					"Returning deferred node %s to the ready queue\n",
					job->GetJobName() );
		_readyQ->Prepend( job, -job->_effectivePriority );
	}

	return numSubmitsThisCycle;
}

//---------------------------------------------------------------------------
int
Dag::PreScriptReaper( Job *job, int status )
{
	ASSERT( job != NULL );
	if ( job->GetStatus() != Job::STATUS_PRERUN ) {
		EXCEPT( "Error: node %s is not in PRERUN state", job->GetJobName() );
	}

	_preRunNodeCount--;

	bool preScriptFailed = false;
	if ( WIFSIGNALED( status ) ) {
			// if script was killed by a signal
		preScriptFailed = true;
		debug_printf( DEBUG_QUIET, "PRE Script of node %s died on %s\n",
					  job->GetJobName(),
					  daemonCore->GetExceptionString(status) );
		job->error_text.formatstr(
				"PRE Script died on %s",
				daemonCore->GetExceptionString(status) );
		job->retval = ( 0 - WTERMSIG(status ) );
	} else if ( WEXITSTATUS( status ) != 0 ) {
			// if script returned failure
		preScriptFailed = true;
		debug_printf( DEBUG_QUIET,
					  "PRE Script of Job %s failed with status %d\n",
					  job->GetJobName(), WEXITSTATUS(status) );
		job->error_text.formatstr(
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

	_jobstateLog.WriteScriptSuccessOrFailure( job, ScriptType::PRE );

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
			if ( !writePreSkipEvent( id, job, job->GetJobName(),
					job->GetDirectory(), DefaultNodeLog() ) ) {
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
		else if ( job->DoRetry() ) {
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
			if ( job->GetRetryMax() > 0 ) {
					// add # of retries to error_text
				job->error_text.formatstr_cat( " (after %d node retries)",
						job->GetRetries() );
			}
		}

	} else {
		debug_printf( DEBUG_NORMAL, "PRE Script of node %s completed "
				"successfully.\n", job->GetJobName() );
		job->retval = 0; // for safety on retries
		job->SetStatus( Job::STATUS_READY );
		if ( _submitDepthFirst ) {
			_readyQ->Prepend( job, -job->_effectivePriority );
		} else {
			_readyQ->Append( job, -job->_effectivePriority );
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
Dag::PostScriptReaper( Job *job, int status )
{
	ASSERT( job != NULL );

	if ( job->GetStatus() != Job::STATUS_POSTRUN ) {
		EXCEPT( "Node %s is not in POSTRUN state",
			job->GetJobName() );
	}

	PostScriptTerminatedEvent event;
	
	event.dagNodeName = strnewp( job->GetJobName() );

	if( WIFSIGNALED( status ) ) {
		debug_printf( DEBUG_QUIET, "POST script died on signal %d\n", status );
		event.normal = false;
		event.signalNumber = status;
	} else {
		event.normal = true;
		event.returnValue = WEXITSTATUS( status );
	}

		// For NOOP jobs, we need the proc and subproc values;
		// for "real" jobs, they are not significant.
	event.cluster = job->GetCluster();
	event.proc = job->GetNoop() ? job->GetProc() : 0;
	event.subproc = job->GetNoop() ? job->GetSubProc() : 0;

	WriteUserLog ulog;
	ulog.setUseCLASSAD( 0 );

	debug_printf( DEBUG_QUIET,
				"Initializing user log writer for %s, (%d.%d.%d)\n",
				DefaultNodeLog(), event.cluster, event.proc, event.subproc );
	ulog.initialize( DefaultNodeLog(), event.cluster, event.proc,
				event.subproc );

	for(int write_attempts = 0;;++write_attempts) {
		if( !ulog.writeEvent( &event ) ) {
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
	return true;
}

//---------------------------------------------------------------------------
// Run a HOLD script.

bool Dag::RunHoldScript( Job *job, bool incrementRunCount )
{
	// Make sure we are allowed to run this script.
	if( !job->_scriptHold ) {
		return false;
	}
	// Run the HOLD script. This is a best-effort attempt that does not change
	// the node status.
	if ( incrementRunCount ) {
		_holdRunNodeCount++;
	}
	_holdScriptQ->Run( job->_scriptHold );

	return true;
}

int
Dag::HoldScriptReaper( Job *job )
{
	ASSERT( job != NULL );
	_holdRunNodeCount--;

	return true;
}

//---------------------------------------------------------------------------
void Dag::PrintJobList() const {
	for (auto it = _jobs.begin(); it != _jobs.end(); it++) {
        (*it)->Dump( this );
    }
    dprintf( D_ALWAYS, "---------------------------------------\t<END>\n" );
}

//---------------------------------------------------------------------------
void
Dag::PrintJobList( Job::status_t status ) const
{
	for (auto it = _jobs.begin(); it != _jobs.end(); it++) {
		if( (*it)->GetStatus() == status ) {
			(*it)->Dump( this );
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
	if ( includeFinalNode && _final_job && !_finalNodeRun ) {
			// Make sure we don't incorrectly return true if we get called
			// just before a final node is started...  (There is a race
			// condition here otherwise, because all of the "regular"
			// nodes can be done, and the final node not changed to
			// ready yet.)
		return false;
	} else if ( IsHalted() ) {
			// Note that we're checking for scripts actually running here,
			// not the number of nodes in the PRERUN or POSTRUN state --
			// if we're halted, we don't start any new PRE scripts.
		return NumJobsSubmitted() == 0 && NumScriptsRunning() == 0;
	}

	return NumJobsSubmitted() == 0 && NumNodesReady() == 0 &&
				ScriptRunNodeCount() == 0;
}

//---------------------------------------------------------------------------
bool
Dag::DoneSuccess( bool includeFinalNode ) const
{
	if ( !FinishedRunning( includeFinalNode ) ) {
			// Note: if final node is running we should get to here...
		return false;
	} else if ( NumNodesDone( includeFinalNode ) ==
				NumNodes( includeFinalNode ) ) {
			// This is the normal case.
		return true;
	} else if ( includeFinalNode && _final_job &&
				_final_job->GetStatus() == Job::STATUS_DONE ) {
			// Final node can override the overall DAG status.
		return true;
	} else if ( _dagIsAborted && _dagStatus == DAG_STATUS_OK ) {
			// Abort-dag-on can override the overall DAG status.
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
	} else if ( _dagIsAborted && _dagStatus == DAG_STATUS_OK ) {
			// Abort-dag-on can override the overall DAG status.
		return false;
	} else if ( IsHalted() ) {
		return true;
	}

	return NumNodesFailed() > 0;
}

//---------------------------------------------------------------------------
bool
Dag::DoneCycle( bool includeFinalNode) const
{
	return FinishedRunning( includeFinalNode ) &&
				!DoneSuccess( includeFinalNode ) &&
				NumNodesFailed() == 0 &&
				!IsHalted() && !_dagIsAborted;
}

//---------------------------------------------------------------------------
int
Dag::NumNodes( bool includeFinal ) const
{
	int result = _jobs.size();
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
// Note: the HTCondor part of this method essentially duplicates functionality
// that is now in schedd.cpp.  We are keeping this here for now in case
// someone needs to run a 7.5.6 DAGMan with an older schedd.
// wenger 2011-01-26
// Note 2: We need to keep this indefinitely for the ABORT-DAG-ON case,
// where we need to condor_rm any running node jobs, and the schedd
// won't do it for us.  wenger 2014-10-29.
void Dag::RemoveRunningJobs ( const CondorID &dmJobId, bool removeCondorJobs,
			bool bForce )
{
	if ( bForce ) removeCondorJobs = true;

	// Hmm -- should we check here if we have jobs queued? wenger 2014-12-17
	bool	haveCondorJob = true;

	ArgList args;

	if ( removeCondorJobs && haveCondorJob ) {
		debug_printf( DEBUG_NORMAL, "Removing any/all submitted "
					"HTCondor jobs...\n" );

		MyString constraint;

		args.Clear();
		args.AppendArg( _condorRmExe );
		args.AppendArg( "-const" );

		// NOTE: having whitespace in the constraint argument will cause quoting problems on windows
		constraint.formatstr(ATTR_DAGMAN_JOB_ID "==%d", dmJobId._cluster );
		args.AppendArg( constraint.c_str() );
		if ( _dagmanUtils.popen( args ) != 0 ) {
			debug_printf( DEBUG_NORMAL, "Error removing DAGMan jobs\n");
		}
	}

	return;
}

//---------------------------------------------------------------------------
void Dag::RemoveRunningScripts ( ) const {

    for (auto it = _jobs.begin(); it != _jobs.end(); it++) {
		ASSERT( *it != NULL );

		// if node is running a PRE script, hard kill it
        if( (*it)->GetStatus() == Job::STATUS_PRERUN ) {
			if ( !(*it)->_scriptPre ) {
				EXCEPT( "Node %s has no PRE script!", (*it)->GetJobName() );
			}
			if ( (*it)->_scriptPre->_pid != 0 ) {
				debug_printf( DEBUG_DEBUG_1, "Killing PRE script %d\n",
							(*it)->_scriptPre->_pid );
				if (daemonCore->Shutdown_Fast((*it)->_scriptPre->_pid) == FALSE) {
					debug_printf(DEBUG_QUIET,
								"WARNING: shutdown_fast() failed on pid %d: %s\n",
								(*it)->_scriptPre->_pid, strerror(errno));
				}
			}
        }
		// if node is running a POST script, hard kill it
        else if( (*it)->GetStatus() == Job::STATUS_POSTRUN ) {
			if ( !(*it)->_scriptPost ) {
				EXCEPT( "Node %s has no POST script!", (*it)->GetJobName() );
			}
			if ( (*it)->_scriptPost->_pid != 0 ) {
				debug_printf( DEBUG_DEBUG_1, "Killing POST script %d\n",
							(*it)->_scriptPost->_pid );
				if(daemonCore->Shutdown_Fast((*it)->_scriptPost->_pid) == FALSE) {
					debug_printf(DEBUG_QUIET,
								"WARNING: shutdown_fast() failed on pid %d: %s\n",
								(*it)->_scriptPost->_pid, strerror( errno ));
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
		int nextRescue = _dagmanUtils.FindLastRescueDagNum( dagFile, multiDags,
					maxRescueDagNum ) + 1;
		if ( overwrite && nextRescue > 1 ) {
			nextRescue--;
		}
		if ( nextRescue > maxRescueDagNum ) {
			nextRescue = maxRescueDagNum;
		}
		rescueDagFile = _dagmanUtils.RescueDagName( dagFile, multiDags, nextRescue );
	}

		// Note: there could possibly be a race condition here if two
		// DAGMans are running on the same DAG at the same time.  That
		// should be avoided by the lock file, though, so I'm not doing
		// anything about it right now.  wenger 2007-02-27

	WriteRescue( rescueDagFile.c_str(), dagFile, parseFailed, isPartial );
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
    for (auto it = _jobs.begin(); it != _jobs.end(); it++) {
        if ((*it)->GetStatus() == Job::STATUS_ERROR) {
            fprintf(fp, "%s,", (*it)->GetJobName());
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
	for (auto it = _jobs.begin(); it != _jobs.end(); it++) {
		WriteNodeToRescue( fp, *(it), reset_retries_upon_rescue, isPartial );
    }

    //
    // Print Dependency Section
    //
	if ( !isPartial ) {
    	fprintf(fp, "\n");
		for (auto it = _jobs.begin(); it != _jobs.end(); it++) {

#ifdef DEAD_CODE
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
#else
			if ( ! (*it)->NoChildren()) {
				fprintf(fp, "PARENT %s CHILD ", (*it)->GetJobName());

				(*it)->VisitChildren(*this,
					[](Dag&, Job*, Job* child, void* pv) -> int {
						fprintf((FILE*)pv, " %s", child->GetJobName());
						return 1;
					}, fp);

				fprintf(fp, "\n");
			}
#endif
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
	if ( node->GetType() == NodeType::FINAL ) {
		keyword = "FINAL";
	} else {
		keyword = node->GetDagFile() ? "SUBDAG EXTERNAL" : "JOB";
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
			WriteScriptToRescue( fp, node->_scriptPre );
		}

			// Print the PRE_SKIP line, if any.
		if ( node->HasPreSkip() != 0 ) {
			fprintf( fp, "PRE_SKIP %s %d\n", node->GetJobName(), node->GetPreSkip() );
		}

			// Print the SCRIPT POST line, if any.
		if ( node->_scriptPost != NULL ) {
			WriteScriptToRescue( fp, node->_scriptPost );
		}

			// Print the VARS line, if any.
#ifdef DEAD_CODE
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
#else
		if (node->HasVars()) {
			std::string vars;
			vars.reserve(500);
			vars = "";
			node->PrintVars(vars);
			fprintf(fp, "VARS %s", node->GetJobName());
			fprintf(fp, "%s\n", vars.c_str());
		}
#endif

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
		if ( node->_explicitPriority != 0 ) {
			fprintf( fp, "PRIORITY %s %d\n", node->GetJobName(),
						node->_explicitPriority );
		}

			// Print the CATEGORY line, if any.
		if ( node->GetThrottleInfo() ) {
			fprintf( fp, "CATEGORY %s %s\n", node->GetJobName(),
						node->GetThrottleInfo()->_category->c_str() );
		}
	}

		// Never mark a FINAL node as done.
		// Also avoid a possible race condition where the job
		// has been skipped but is not yet marked as DONE.
	if ( node->GetStatus() == Job::STATUS_DONE && !( node->GetType() == NodeType::FINAL ) ) {
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

//-----------------------------------------------------------------------------
void
Dag::WriteScriptToRescue( FILE *fp, Script *script )
{
	const char *type;
	switch( script->_type ) {
		case ScriptType::PRE: type = "PRE"; break;
		case ScriptType::POST: type = "POST"; break;
		case ScriptType::HOLD: type = "HOLD"; break;
	}
	fprintf( fp, "SCRIPT " );
	if ( script->_deferStatus != SCRIPT_DEFER_STATUS_NONE ) {
		fprintf( fp, "DEFER %d %d ", script->_deferStatus,
					(int)script->_deferTime );
	}
	fprintf( fp, "%s %s %s\n", type, script->GetNode()->GetJobName(),
				script->GetCmd() );
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
		_metrics->NodeFinished( job->GetDagFile() != NULL, true );
		job->countedAsDone = true;
		ASSERT( (unsigned int)_numNodesDone <= _jobs.size() );
	} else {
#ifdef MEMORY_HOG
		// fall through to update children again. this is safe
#else
		// do not update children again - the children have only a completion counter.
		// not a parent list, so we can only call ParentComplete() once per child.
		return;
#endif
	}

    //
    // Report termination to all child jobs by removing parent's ID from
    // each child's waiting queue.
    //
#ifdef DEAD_CODE
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
				if ( !recovery ) {
						// If child has no more parents in its waiting queue,
						// submit it.
					StartNode( child, false );
				}
			}
		}
	}
#else
	if (bootstrap || recovery) {
		// notify children of parent completion, but don't start any nodes
		job->NotifyChildren(*this, NULL);
	} else {
		// notify children of parent completion, and start any nodes that are no longer idle
		job->NotifyChildren(*this,
			[](Dag& dag, Job* child) -> bool {
				// this is invoked after child->ParentComplete(job) returns true
				if (child->GetStatus() == Job::STATUS_READY) {
					return dag.StartNode(child, false);
				} else {
					return false;
				}
			});
	}
#endif
}

//-------------------------------------------------------------------------
void Dag::
PrintEvent( debug_level_t level, const ULogEvent* event, Job* node,
			bool recovery )
{
	ASSERT( event );

	const char *recovStr = recovery ? " [recovery mode]" : "";

	MyString timestr;
		// Be sure to pass GetEventTime() here, because we want the
		// event time to always be output has a human-readable string,
		// even if dprintf() is configured to print timestamps.
	time_to_str( event->GetEventclock(), timestr );
		// String from time_to_str has trailing blank (needed for other
		// places in the code).
	timestr.trim();

	if( node ) {
	    debug_printf( level, "Event: %s for %s Node %s (%d.%d.%d) {%s}%s\n",
					  event->eventName(), node->JobTypeString(),
					  node->GetJobName(), event->cluster, event->proc,
					  event->subproc, timestr.c_str(), recovStr );
	} else {
        debug_printf( level, "Event: %s for unknown Node (%d.%d.%d) {%s}: "
					  "ignoring...%s\n", event->eventName(),
					  event->cluster, event->proc,
					  event->subproc, timestr.c_str(), recovStr );
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

    if ( _finalNodeRun || ( node->have_retry_abort_val &&
				node->retval == node->retry_abort_val ) ) {
		const char *finalRun = _finalNodeRun ?
					"because final node is running " : "";
        debug_printf( DEBUG_NORMAL, "Aborting further retries of node %s "
                      "%s(last attempt returned %d)\n",
                      node->GetJobName(), finalRun, node->retval);
        _numNodesFailed++;
		_metrics->NodeFinished( node->GetDagFile() != NULL, false );
		if ( _dagStatus == DAG_STATUS_OK ) {
			_dagStatus = DAG_STATUS_NODE_FAILED;
		}
        return;
    }
	node->SetStatus( Job::STATUS_READY );
	node->retries++;
	node->_numSubmittedProcs = 0;
	ASSERT( node->GetRetries() <= node->GetRetryMax() );
	if( node->_scriptPre ) {
		// undo PRE script completion
		node->_scriptPre->_done = false;
	}
	node->error_text = "";
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
			int id = GetIndexID( node->GetID() );
			if ( GetEventIDHash( node->GetNoop() )->erase( id )
						!= 1 ) {
				EXCEPT( "Event ID hash table error!" );
			}
		}

		// Doing this fixes gittrac #481 (recovery fails on a DAG that
		// has retried nodes).  (See SubmitNodeJob() for where this
		// gets done during "normal" running.)
		node->SetCondorID( _defaultCondorId );
	}
}

//-------------------------------------------------------------------------
// Number the nodes according to DFS order 
void 
Dag::DFSVisit (Job * job, int depth)
{
	//Check whether job has been numbered already
	if (job==NULL || job->_visited)
		return;
	
	//Remember that the job has been numbered	
	job->_visited = true;

	// the height of a node is the minumum recursion depth at which we first visited it
	// the height of the graph is the maximum of the node depths
	_graph_height = MAX(_graph_height, depth);

	// with width is the sum of nodes at a given depth, the overall width is the max of those
	_graph_widths[depth] += 1;
	_graph_width = MAX(_graph_width, _graph_widths[depth] += 1);
	
#ifdef DEAD_CODE
	//Get the children of current job	
	set<JobID_t> & children = job->GetQueueRef(Job::Q_CHILDREN);
	set<JobID_t>::const_iterator child_itr;

	for (child_itr = children.begin(); child_itr != children.end(); child_itr++)
	{
		Job * child = FindNodeByNodeID( *child_itr );
		DFSVisit (child);
	}
#else
	// visit the children of the current job, marking their first visitation order
	if ( ! job->NoChildren()) {
		int plus_one = depth + 1;
		while ((int)_graph_widths.size() <= plus_one) { _graph_widths.push_back(0); }

		job->VisitChildren(*this, [](Dag& dag, Job* /*parent*/, Job* child, void* pv) -> int {
				dag.DFSVisit(child, *(int*)pv);
				return 1;
			},
			&plus_one);
	}
#endif

	DFS_ORDER++;
	job->_dfsOrder = DFS_ORDER;
}		

//-------------------------------------------------------------------------
// Detects cycle and warns user about it
bool 
Dag::isCycle ()
{
	bool cycle = false; 

	//Start DFS numbering from zero, although not necessary
	DFS_ORDER = 0; 
	// as a side effect of DFS cycle detection, will we determine width and height of the graph
	_graph_width = 0;
	_graph_height = 0;
	_graph_widths.clear();
	_graph_widths.push_back(0);

	//Visit all jobs in DAG and number them	
	for (auto it = _jobs.begin(); it != _jobs.end(); it++) {
		if ( !(*it)->_visited && (*it)->NoParents())
			DFSVisit (*it, 0);
	}

	//Detect cycle
	for (auto it = _jobs.begin(); it != _jobs.end(); it++) {
#ifdef DEAD_CODE
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
#else
		if ((*it)->VisitChildren(*this,
			[](Dag&, Job* parent, Job* child, void*) -> int {
				if (child->_dfsOrder >= parent->_dfsOrder) {
		#ifdef REPORT_CYCLE
					debug_printf(DEBUG_QUIET,
						"Cycle in the graph possibly involving jobs %s and %s\n",
						parent->GetJobName(), child->GetJobName());
		#endif
					return 1; // increment the cycle count
				}
				return 0;
			}, NULL)) {
			// the return value of VisitChildren is the number of children with _dfsOrder
			// greater that that of their parents.  If *any* have this, then we have a cycle.
			cycle = true;
		}
#endif
	}
	return cycle;
}

//-------------------------------------------------------------------------
bool
Dag::CheckForDagAbort(Job *job, const char *type)
{
	if ( job->DoAbort() ) {
		debug_printf( DEBUG_QUIET, "Aborting DAG because we got "
				"the ABORT exit value from a %s\n", type);
		_dagIsAborted = true;

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

#if 0
//-------------------------------------------------------------------------
const MyString
Dag::ParentListString( Job *node, size_t max_items, const char delim ) const
{
	MyString parents_str;

	set<JobID_t> &parent_list = node->GetQueueRef( Job::Q_PARENTS );
	if (parent_list.size() < max_items) {
		for (auto pit = parent_list.begin(); pit != parent_list.end(); pit++) {
			Job* parent = FindNodeByNodeID( *pit );
			const char* parent_name = parent->GetJobName();
			ASSERT( parent_name );
			if( ! parents_str.IsEmpty() ) {
				parents_str += delim;
			}
			parents_str += parent_name;
		}
		}
	return parents_str;
}
#endif

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
	free(_dot_file_name);
	_dot_file_name = strdup(dot_file_name);
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
	free(_dot_include_file_name);
	_dot_include_file_name = strdup(include_file_name);
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

		_dagmanUtils.tolerant_unlink(temp_dot_file_name.c_str());
		temp_dot_file = safe_fopen_wrapper_follow(temp_dot_file_name.c_str(), "w");
		if (temp_dot_file == NULL) {
			debug_dprintf(D_ALWAYS, DEBUG_NORMAL,
						  "Can't create dot file '%s'\n", 
						  temp_dot_file_name.c_str());
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
				// Note:  we do tolerant_unlink because renaming over an
				// existing file fails on Windows.
			_dagmanUtils.tolerant_unlink(current_dot_file_name.c_str());
			if ( rename(temp_dot_file_name.c_str(),
						current_dot_file_name.c_str()) != 0 ) {
				debug_printf( DEBUG_NORMAL,
					  		"Warning: can't rename temporary dot "
					  		"file (%s) to permanent file (%s): %s\n",
					  		temp_dot_file_name.c_str(),
							current_dot_file_name.c_str(),
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
			int minUpdateTime, bool alwaysUpdate )
{
	if ( _statusFileName != NULL ) {
		debug_printf( DEBUG_NORMAL, "Warning: Attempt to set NODE_STATUS_FILE "
					"to %s does not override existing value of %s\n",
					statusFileName, _statusFileName );
		check_warning_strictness( DAG_STRICT_3 );
		return;
	}
	_statusFileName = strdup( statusFileName );
	_minStatusUpdateTime = minUpdateTime;
	_alwaysUpdateStatus = alwaysUpdate;
}

//-------------------------------------------------------------------------
/** Dump the node status.
	@param whether the DAG has just been held
	@param whether the DAG has just been removed
*/
// Note:  We might eventually want to change this to actually creating
// classad objects and calling the unparser on them.  (See gittrac
// #4316.)  wenger 2014-04-16
void
Dag::DumpNodeStatus( bool held, bool removed )
{
		//
		// Decide whether to update the file.
		//
	if ( _statusFileName == NULL ) {
		return;
	}
	
	if ( !_alwaysUpdateStatus && !_statusFileOutdated && !held && !removed ) {
		debug_printf( DEBUG_DEBUG_1, "Node status file not updated "
					"because it is not yet outdated\n" );
		return;
	}
	
	time_t startTime = time( NULL );
	bool tooSoon = (_minStatusUpdateTime > 0) &&
				((startTime - _lastStatusUpdateTimestamp) <
				_minStatusUpdateTime);
	if ( tooSoon && !held && !removed && !FinishedRunning( true ) &&
				!_dagIsAborted ) {
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
	_dagmanUtils.tolerant_unlink( tmpStatusFile.c_str() );

	FILE *outfile = safe_fopen_wrapper_follow( tmpStatusFile.c_str(), "w" );
	if ( outfile == NULL ) {
		debug_printf( DEBUG_NORMAL,
					  "Warning: can't create node status file '%s': %s\n", 
					  tmpStatusFile.c_str(), strerror( errno ) );
		check_warning_strictness( DAG_STRICT_1 );
		return;
	}

	fprintf( outfile, "[\n" );
	fprintf( outfile, "  Type = \"DagStatus\";\n" );

		//
		// Print DAG file list.
		//
	fprintf( outfile, "  DagFiles = {\n" );
	const char *separator = "";
	for ( auto it = _dagFiles.begin(); it != _dagFiles.end(); ++it ) {
		fprintf( outfile, "%s    %s", separator,
					EscapeClassadString( it->c_str() ) );
		separator = ",\n";
	}
	fprintf( outfile, "\n  };\n" );

		//
		// Print timestamp.
		//
	MyString timeStr = ctime( &startTime );
	timeStr.chomp();
	fprintf( outfile, "  Timestamp = %lu; /* %s */\n",
				(unsigned long)startTime,
				EscapeClassadString( timeStr.c_str() ) );

		// If markNodesError is true, this means that we want to mark
		// nodes in the PRERUN, SUBMITTED, and POSTRUN states as being
		// in the ERROR state.  This is because if, for example, we're
		// condor_rm'ed, we won't see the ABORTED events for node jobs
		// that get removed, but we're *assuming* they will get removed.
		// (See gittrac #4686.)  We only want to do this the very last
		// time we dump out the node status file, to make sure that we
		// don't erroneously mark a running final node (if we have one)
		// as finished unsuccessfully.
	bool markNodesError = false;

		//
		// Print overall DAG status.
		//
	Job::status_t dagJobStatus = Job::STATUS_SUBMITTED;
	const char *statusNote = "";

	if ( DoneSuccess( true ) ) {
		dagJobStatus = Job::STATUS_DONE;
		statusNote = "success";
			// In case we have a combination of abort-dag-on and final
			// node, and we get here before seeing ABORTED events for
			// node jobs that got removed.
		markNodesError = true;

	} else if ( DoneFailed( true ) ) {
		dagJobStatus = Job::STATUS_ERROR;
		if ( _dagStatus == DAG_STATUS_ABORT ) {
			statusNote = "aborted";
			markNodesError = true;
		} else {
			statusNote = "failed";
				// In case we're halted and nodes are waiting for
				// deferred PRE or POST scripts.
			markNodesError = true;
		}

	} else if ( DoneCycle( true ) ) {
		dagJobStatus = Job::STATUS_ERROR;
		statusNote = "cycle";

	} else if ( held ) {
		statusNote = "held";

	} else if ( removed ) {
		if ( HasFinalNode() && !FinishedRunning( true ) ) {
			dagJobStatus = Job::STATUS_SUBMITTED;
			statusNote = "removed";
		} else {
			dagJobStatus = Job::STATUS_ERROR;
			statusNote = "removed";
			markNodesError = true;
		}
	} else if ( _dagIsAborted ) {
		if ( HasFinalNode() && !FinishedRunning( true ) ) {
			dagJobStatus = Job::STATUS_SUBMITTED;
			statusNote = "aborted";
		} else {
			dagJobStatus = Job::STATUS_ERROR;
			statusNote = "aborted";
			markNodesError = true;
		}
	}

	MyString statusStr = Job::status_t_names[dagJobStatus];
	statusStr.trim();
	statusStr += " (";
	statusStr += statusNote;
	statusStr += ")";
	fprintf( outfile, "  DagStatus = %d; /* %s */\n", dagJobStatus,
				EscapeClassadString( statusStr.c_str() ) );

	int nodesPre = PreRunNodeCount();
	int nodesQueued = NumJobsSubmitted();
	int nodesPost = PostRunNodeCount();
	int nodesFailed = NumNodesFailed();
	int nodesHeld = NumHeldJobProcs();
	int nodesIdle = NumIdleJobProcs();
	if ( markNodesError ) {
			// Adjust state counts to reflect "pending" removes of node
			// jobs, etc.
		nodesFailed += nodesPre;
		nodesPre = 0;
		nodesFailed += nodesQueued;
		nodesQueued = 0;
		nodesFailed += nodesPost;
		nodesPost = 0;
		nodesHeld = 0;
		nodesIdle = 0;
	}
	fprintf( outfile, "  NodesTotal = %d;\n", NumNodes( true ) );
	fprintf( outfile, "  NodesDone = %d;\n", NumNodesDone( true ) );
	fprintf( outfile, "  NodesPre = %d;\n", nodesPre );
	fprintf( outfile, "  NodesQueued = %d;\n", nodesQueued );
	fprintf( outfile, "  NodesPost = %d;\n", nodesPost );
	fprintf( outfile, "  NodesReady = %d;\n", NumNodesReady() );
	fprintf( outfile, "  NodesUnready = %d;\n",NumNodesUnready( true ) );
	fprintf( outfile, "  NodesFailed = %d;\n", nodesFailed );
	fprintf( outfile, "  JobProcsHeld = %d;\n", nodesHeld );
	fprintf( outfile, "  JobProcsIdle = %d; /* includes held */\n", nodesIdle );
	fprintf( outfile, "]\n" );

		//
		// Print status of all nodes.
		//
	for (auto it = _jobs.begin(); it != _jobs.end(); it++) {
		fprintf( outfile, "[\n" );
		fprintf( outfile, "  Type = \"NodeStatus\";\n" );

		int jobProcsQueued = (*it)->_queuedNodeJobProcs;
		int jobProcsHeld = (*it)->_jobProcsOnHold;

		Job::status_t status = (*it)->GetStatus();
		const char *nodeNote = "";
		if ( status == Job::STATUS_READY ) {
				// Note:  Job::STATUS_READY only means that the job is
				// ready to submit if it doesn't have any unfinished
				// parents.
			if ( !(*it)->CanSubmit() ) {
				status = Job::STATUS_NOT_READY;
			}

		} else if ( status == Job::STATUS_SUBMITTED ) {
			if ( markNodesError ) {
				status = Job::STATUS_ERROR;
				nodeNote = "Was STATUS_SUBMITTED";
				jobProcsQueued = 0;
				jobProcsHeld = 0;
			} else {
					// This isn't really the right thing to do for multi-
					// proc nodes, but I want to get in a fix for
					// gittrac #5333 today...  wenger 2015-11-05
				nodeNote = (*it)->GetProcIsIdle( 0 ) ? "idle" : "not_idle";
				// Note: add info here about whether the job(s) are
				// held, once that code is integrated.
			}

		} else if ( status == Job::STATUS_ERROR ) {
			nodeNote = (*it)->error_text.c_str();

		} else if ( status == Job::STATUS_PRERUN ) {
			if ( markNodesError ) {
				status = Job::STATUS_ERROR;
				nodeNote = "Was STATUS_PRERUN";
			}

		} else if ( status == Job::STATUS_POSTRUN ) {
			if ( markNodesError ) {
				status = Job::STATUS_ERROR;
				nodeNote = "Was STATUS_POSTRUN";
			}
		}

		fprintf( outfile, "  Node = %s;\n",
					EscapeClassadString((*it)->GetJobName() ) );
		statusStr = Job::status_t_names[status];
		statusStr.trim();
		fprintf( outfile, "  NodeStatus = %d; /* %s */\n", status,
					EscapeClassadString( statusStr.c_str() ) );
		// fprintf( outfile, "  /* HTCondorStatus = xxx; */\n" );
		fprintf( outfile, "  StatusDetails = %s;\n",
					EscapeClassadString( nodeNote ) );
		fprintf( outfile, "  RetryCount = %d;\n", (*it)->GetRetries() );
		// fprintf( outfile, "  /* JobProcsTotal = xxx; */\n" );
		fprintf( outfile, "  JobProcsQueued = %d;\n", jobProcsQueued );
		// fprintf( outfile, "  /* JobProcsRunning = xxx; */\n" );
		// fprintf( outfile, "  /* JobProcsIdle = xxx; */\n" );
		fprintf( outfile, "  JobProcsHeld = %d;\n", jobProcsHeld );

		fprintf( outfile, "]\n" );
	}

		//
		// Print end information.
		//
	fprintf( outfile, "[\n" );
	fprintf( outfile, "  Type = \"StatusEnd\";\n" );

	time_t endTime = time( NULL );
	timeStr = ctime( &endTime );
	timeStr.chomp();
	fprintf( outfile, "  EndTime = %lu; /* %s */\n",
				(unsigned long)endTime,
				EscapeClassadString( timeStr.c_str() ) );

	time_t nextTime;
	if ( FinishedRunning( true ) || removed ) {
		nextTime = 0;
		timeStr = "none";
	} else {
		nextTime = endTime + _minStatusUpdateTime;
		timeStr = ctime( &nextTime );
		timeStr.chomp();
	}
	fprintf( outfile, "  NextUpdate = %lu; /* %s */\n",
				(unsigned long)nextTime,
				EscapeClassadString( timeStr.c_str() ) );
	fprintf( outfile, "]\n" );

	fclose( outfile );

		//
		// Now rename the temporary file to the "real" file.
		// Note:  we do tolerant_unlink because renaming over an
		// existing file fails on Windows.
		//
	MyString statusFileName( _statusFileName );
#if 0 // For testing, to enable manual checking of intermediate states...
	static int statusFileCount = 0;
	statusFileName += ++statusFileCount;
	debug_printf( DEBUG_QUIET, "Writing node status file %s\n",
				statusFileName.Value() );
#endif
	_dagmanUtils.tolerant_unlink( statusFileName.c_str() );
	if ( rename( tmpStatusFile.c_str(), statusFileName.c_str() ) != 0 ) {
		debug_printf( DEBUG_NORMAL,
					  "Warning: can't rename temporary node status "
					  "file (%s) to permanent file (%s): %s\n",
					  tmpStatusFile.c_str(), statusFileName.c_str(),
					  strerror( errno ) );
		check_warning_strictness( DAG_STRICT_1 );
		return;
	}

	_statusFileOutdated = false;
	_lastStatusUpdateTimestamp = startTime;
}

//-------------------------------------------------------------------------
const char *
Dag::EscapeClassadString( const char* strIn )
{
	static classad::Value tmpValue;
	static std::string tmpStr; // must be static so we can return c_str()
	static classad::ClassAdUnParser unparse;

	tmpValue.SetStringValue( strIn );
	tmpStr = "";
	unparse.Unparse( tmpStr, tmpValue );

	return tmpStr.c_str();
}

//---------------------------------------------------------------------------
bool
Dag::MonitorLogFile()
{
	debug_printf( DEBUG_DEBUG_2,
				"Attempting to monitor log file <%s>\n", _defaultNodeLog );

	CondorError errstack;
	if ( !_condorLogRdr.monitorLogFile( _defaultNodeLog, !_recovery,
				errstack ) ) {
		errstack.pushf( "DAGMan::Job", DAGMAN_ERR_LOG_FILE,
					"ERROR: Unable to monitor log file <%s>\n",
					_defaultNodeLog );
		debug_printf( DEBUG_QUIET, "%s\n", errstack.getFullText().c_str() );
		EXCEPT( "Fatal log file monitoring error!" );
		return false;
	}

	return true;
}

//---------------------------------------------------------------------------
bool
Dag::UnmonitorLogFile()
{
	debug_printf( DEBUG_DEBUG_2, "Unmonitoring log file <%s>\n",
				_defaultNodeLog );

	CondorError errstack;
	bool result = _condorLogRdr.unmonitorLogFile( _defaultNodeLog,
				errstack );
	if ( !result ) {
		errstack.pushf( "DAGMan::Job", DAGMAN_ERR_LOG_FILE,
					"ERROR: Unable to unmonitor log file <%s>\n",
					_defaultNodeLog );
		debug_printf( DEBUG_QUIET, "%s\n", errstack.getFullText().c_str() );
		EXCEPT( "Fatal log file monitoring error!" );
	}

	return result;
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

//-------------------------------------------------------------------------
void 
Dag::SetMaxJobsSubmitted(int newMax) {

	bool isChanged = (newMax != _maxJobsSubmitted);
	bool removeJobsAfterLimitChange = param_boolean("DAGMAN_REMOVE_JOBS_AFTER_LIMIT_CHANGE", false);

	// Update our internal max jobs count
	_maxJobsSubmitted = newMax;

	// If maxJobs is set to 0, that means no maximum limit. Exit now.
	if (_maxJobsSubmitted == 0) return;

	// Optionally remove jobs to meet the new limit, starting with most recent
	if (isChanged && removeJobsAfterLimitChange) {
		int submittedJobsCount = 0;
		for (auto it = _jobs.begin(); it != _jobs.end(); it++) {
			if ((*it)->GetStatus() == Job::STATUS_SUBMITTED) {
				submittedJobsCount++;
				if (submittedJobsCount > _maxJobsSubmitted) {
					(*it)->retry_max++;
					RemoveBatchJob(*it);
				}
			}
		}
	}
}

//===========================================================================

/** Set the filename of the jobstate.log file.
	@param the filename to which to write the jobstate log
*/
void 
Dag::SetJobstateLogFileName( const char *logFileName )
{
	if ( _jobstateLog.LogFile() != NULL ) {
		debug_printf( DEBUG_NORMAL, "Warning: Attempt to set JOBSTATE_LOG "
					"to %s does not override existing value of %s\n",
					logFileName, _jobstateLog.LogFile() );
		check_warning_strictness( DAG_STRICT_3 );
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
		debug_printf( DEBUG_QUIET, "Error checking HTCondor job events: %s\n",
				jobError.c_str() );
		ASSERT( false );
	} else if ( result == CheckEvents::EVENT_BAD_EVENT ||
				result == CheckEvents::EVENT_WARNING ) {
		debug_printf( DEBUG_NORMAL, "Warning checking HTCondor job events: %s\n",
				jobError.c_str() );
		check_warning_strictness( DAG_STRICT_3 );
	} else {
		debug_printf( DEBUG_DEBUG_1, "All HTCondor job events okay\n");
	}
}

//-------------------------------------------------------------------------
int
Dag::NumHeldJobProcs()
{
	int numHeldProcs = 0;
	for (auto it = _jobs.begin(); it != _jobs.end(); it++) {
		numHeldProcs += (*it)->_jobProcsOnHold;
	}
	return numHeldProcs;
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
					"of -MaxPre limit (%d) or DEFER\n",
					_preScriptQ->GetScriptDeferredCount(),
					_maxPreScripts );
	}

	if( _postScriptQ->GetScriptDeferredCount() > 0 || force ) {
		debug_printf( level, "Note: %d total POST script deferrals because "
					"of -MaxPost limit (%d) or DEFER\n",
					_postScriptQ->GetScriptDeferredCount(),
					_maxPostScripts );
	}

	if( _holdScriptQ->GetScriptDeferredCount() > 0 || force ) {
		debug_printf( level, "Note: %d total HOLD script deferrals because "
					"of -MaxHold limit (%d) or DEFER\n",
					_holdScriptQ->GetScriptDeferredCount(),
					_maxHoldScripts );
	}
}

//---------------------------------------------------------------------------
void
Dag::PrintPendingNodes() const
{
	dprintf( D_ALWAYS, "Pending DAG nodes:\n" );

    for (auto it = _jobs.begin(); it != _jobs.end(); it++) {
		switch ((*it)->GetStatus() ) {
		case Job::STATUS_PRERUN:
		case Job::STATUS_SUBMITTED:
		case Job::STATUS_POSTRUN:
			dprintf( D_ALWAYS, "  Node %s, HTCondor ID %d, status %s\n",
						(*it)->GetJobName(), (*it)->GetCluster(),
						(*it)->GetStatusName() );
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
	for ( auto throttle: *_catThrottles.GetThrottles() ) {
		ThrottleByCategory::ThrottleInfo *info = throttle.second;
		debug_printf( DEBUG_DEBUG_1, "Category %s has %d jobs, "
					"throttle setting of %d\n", info->_category->c_str(),
					info->_totalJobs, info->_maxJobs );
		ASSERT( info->_totalJobs >= 0 );
		if ( info->_totalJobs < 1 ) {
			debug_printf( DEBUG_NORMAL, "Warning: category %s has no "
						"assigned nodes, so the throttle setting (%d) "
						"will have no effect\n", info->_category->c_str(),
						info->_maxJobs );
			check_warning_strictness( DAG_STRICT_2 );
		}

		if ( !info->isSet() ) {
			debug_printf( DEBUG_NORMAL, "Warning: category %s has no "
						"throttle value set\n", info->_category->c_str() );
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
//           people that are familiar with HTCondor job states. 
// Scope:    Private
//
//-------------------------------------------------------------------------
void 
Dag::DumpDotFileNodes(FILE *temp_dot_file)
{
	for (auto it = _jobs.begin(); it != _jobs.end(); it++) {
		const char *node_name;
		
		node_name = (*it)->GetJobName();
		switch ((*it)->GetStatus()) {
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
	for (auto it = _jobs.begin(); it != _jobs.end(); it++) {
#ifdef DEAD_CODE
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
#else
		if ((*it)->GetJobName()) {
			(*it)->VisitChildren(*this,
				[](Dag&, Job* parent, Job* child, void* pv) -> int {
					FILE* fp = (FILE*)pv;
					const char * child_name = child->GetJobName();
					if (child_name) {
						fprintf(fp, "    \"%s\" -> \"%s\";\n", parent->GetJobName(), child_name);
					}
					return 1;
				},
				temp_dot_file);
		}
#endif
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
			fp = safe_fopen_wrapper_follow(dot_file_name.c_str(), "r");
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
	auto insertJobResult = _nodeNameHash.insert( std::make_pair( job.GetJobName(), &job ) );
	ASSERT( insertJobResult.second == true );
	auto insertIdResult = _nodeIDHash.insert( std::make_pair( job.GetJobID(), &job ) );
	ASSERT( insertIdResult.second == true );

		// Final node status is set to STATUS_NOT_READY here, so it
		// won't get run even though it has no parents; its status
		// will get changed when it should be run.
	if ( ( job.GetType() == NodeType::FINAL ) ) {
		if ( _final_job ) {
        	debug_printf( DEBUG_QUIET, "Error: DAG already has a final "
						"node %s; attempting to add final node %s\n",
						_final_job->GetJobName(), job.GetJobName() );
			return false;
		}
		job.SetStatus( Job::STATUS_NOT_READY );
		_final_job = &job;
	}

	if ( ( job.GetType() == NodeType::PROVISIONER ) ) {
		if ( _provisioner_node ) {
			debug_printf( DEBUG_QUIET, "Error: DAG already has a provisioner "
						"node %s; attempting to add provisioner node %s\n",
						_provisioner_node->GetJobName(), job.GetJobName() );
			return false;
		}
		_provisioner_node = &job;
	}

	_jobs.push_back(&job);
	return true;
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

#ifdef DEAD_CODE
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
#endif

//---------------------------------------------------------------------------
Job*
Dag::LogEventNodeLookup( const ULogEvent* event,
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
		
	  node = FindNodeByEventID( condorID );
	  if( node ) {
	    return node;
	  }
	}

		// if the job ID wasn't familiar and we didn't find a node
		// above, there are at least four possibilites:
		//	
		// 1) it's the submit event for a node we just submitted, and
		// we don't yet know the job ID; in this case, we look up the
		// node name in the event body. Alternately if we are in recovery
		// mode we don't yet know the job ID.
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
		//
		// 5) it's a cluster submit event, which is handled similarly to
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
					bool isNoop = JobIsNoop( condorID );
					ASSERT( isNoop == node->GetNoop() );
					int id = GetIndexID( condorID );
					std::map<int, Job *> *ht =
								GetEventIDHash( isNoop );
					auto findResult = ht->find( id );
					if ( findResult == ht->end() ) {
							// Node not found.
						auto insertResult = ht->insert( std::make_pair( id, node ) );
						ASSERT( insertResult.second == true );
					} else {
							// Node was found.
						ASSERT( (*findResult).second == node );
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
				bool isNoop = JobIsNoop( condorID );
				int id = GetIndexID( condorID );
				std::map<int, Job *> *ht =
							GetEventIDHash( isNoop );
				auto findResult = ht->find( id );
				if ( findResult == ht->end() ) {
						// Node not found.
					auto insertResult = ht->insert( std::make_pair( id, node ) );
					ASSERT( insertResult.second == true );
				} else {
						// Node was found.
					ASSERT( (*findResult).second == node );
				}
			}
		} else {
			debug_printf( DEBUG_QUIET, "ERROR: 'DAG Node:' not found "
						"in skip event notes: <%s>\n",
						skip_event->skipEventLogNotes );
		}
		return node;
	}

	if( event->eventNumber == ULOG_CLUSTER_SUBMIT ) {
		const ClusterSubmitEvent* cluster_submit_event = (const ClusterSubmitEvent*)event;
		if ( cluster_submit_event->submitEventLogNotes ) {
			char nodeName[1024] = "";
			if ( sscanf( cluster_submit_event->submitEventLogNotes,
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
					bool isNoop = JobIsNoop( condorID );
					ASSERT( isNoop == node->GetNoop() );
					int id = GetIndexID( condorID );
					std::map<int, Job *> *ht =
								GetEventIDHash( isNoop );
					auto findResult = ht->find( id );
						// std::map::find() returns an iterator pointing to the desired element, or end() if not found
					if ( findResult == ht->end() ) {
							// Node not found.
						auto insertResult = ht->insert( std::make_pair( id, node ) );
							// std::map::insert() returns a pair, second element is the success bool
						ASSERT( insertResult.second == true );
					} else {
							// Node was found.
						ASSERT( (*findResult).second == node );
					}
				}
			} else {
				debug_printf( DEBUG_QUIET, "ERROR: 'DAG Node:' not found "
							"in cluster submit event notes: <%s>\n",
							cluster_submit_event->submitEventLogNotes );
			}
		}
		return node;
	}
	return node;
}


//---------------------------------------------------------------------------
// Checks whether this is a "good" event ("bad" events include HTCondor
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
Dag::EventSanityCheck( const ULogEvent* event,
			const Job* node, bool* result )
{
	ASSERT( event );
	ASSERT( node );

	MyString eventError;
	CheckEvents::check_event_result_t checkResult = CheckEvents::EVENT_OKAY;

	checkResult = _checkCondorEvents.CheckAnEvent( event, eventError );

	if( checkResult == CheckEvents::EVENT_OKAY ) {
		debug_printf( DEBUG_DEBUG_1, "Event is okay\n" );
		return true;
	}

	debug_printf( DEBUG_NORMAL, "%s\n", eventError.c_str() );
	//debug_printf( DEBUG_NORMAL, "WARNING: bad event here may indicate a "
				  //"serious bug in HTCondor -- beware!\n" );

	if( checkResult == CheckEvents::EVENT_WARNING ) {
		debug_printf( DEBUG_NORMAL, "BAD EVENT is warning only\n");
		return true;
	}

	if( checkResult == CheckEvents::EVENT_BAD_EVENT ) {
		debug_printf( DEBUG_NORMAL, "Continuing with DAG in spite of bad "
					  "event (%s) because of allow_events setting\n",
					  eventError.c_str() );
	
			// Don't do any further processing of this event,
			// because it can goof us up (e.g., decrement count
			// of running jobs when we shouldn't).

		return false;
	}

	if( checkResult == CheckEvents::EVENT_ERROR ) {
		debug_printf( DEBUG_QUIET, "ERROR: aborting DAG because of bad event "
					  "(%s)\n", eventError.c_str() );
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
Dag::SanityCheckSubmitEvent( const CondorID condorID, const Job* node ) const
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
					message.c_str() );
		main_shutdown_rescue( EXIT_ERROR, DAG_STATUS_ERROR );
		return true;
	} else {
		debug_printf( DEBUG_QUIET,
					"%s  Trusting the userlog for now (because of "
					"DAGMAN_ABORT_ON_SCARY_SUBMIT setting), but this is "
					"scary!\n", message.c_str() );
	}
	return false;
}

//---------------------------------------------------------------------------
std::map<int, Job *> *
Dag::GetEventIDHash(bool isNoop)
{
	if ( isNoop ) {
		return &_noopIDHash;
	}

	return &_condorIDHash;
}

//---------------------------------------------------------------------------
const std::map<int, Job *> *
Dag::GetEventIDHash(bool isNoop) const
{
	if ( isNoop ) {
		return &_noopIDHash;
	}

	return &_condorIDHash;
}

// NOTE: dag addnode/removenode/adddep/removedep methods don't
// necessarily insure internal consistency...that's currently up to
// the higher level calling them to get right...
//---------------------------------------------------------------------------

Dag::submit_result_t
Dag::SubmitNodeJob( const Dagman &dm, Job *node, CondorID &condorID )
{
	submit_result_t result = SUBMIT_RESULT_NO_SUBMIT;
	bool use_condor_submit = param_boolean("DAGMAN_USE_CONDOR_SUBMIT", true);
	// If a submit description is already set, override the DAGMAN_USE_CONDOR_SUBMIT knob
	if (node->GetSubmitDesc()) { use_condor_submit = false; }

		// Resetting the HTCondor ID here fixes PR 799.  wenger 2007-01-24.
	if ( node->GetCluster() != _defaultCondorId._cluster ) {
			// Remove the "previous" HTCondor ID for this node from
			// the ID->node hash table.
		int id = GetIndexID( node->GetID() );
		int removeResult = GetEventIDHash( node->GetNoop() )->erase( id );
			// std::map::erase() returns the number of elements erased
		ASSERT( removeResult == 1 );
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
   	if ( !node->GetNoop() &&
				node->GetDagFile() != NULL && _generateSubdagSubmits ) {
		bool isRetry = node->GetRetries() > 0;
		if ( _dagmanUtils.runSubmitDag( *_submitDagDeepOpts, node->GetDagFile(),
					node->GetDirectory(), node->_effectivePriority,
					isRetry ) != 0 ) {
			++node->_submitTries;
			debug_printf( DEBUG_QUIET,
						"ERROR: condor_submit_dag -no_submit failed "
						"for node %s.\n", node->GetJobName() );
				// Hmm -- should this be a node failure, since it probably
				// won't work on retry?  wenger 2010-03-26
			return SUBMIT_RESULT_NO_SUBMIT;
		}
	}

	debug_printf( DEBUG_NORMAL, "Submitting %s Node %s job(s)...\n",
			  	node->JobTypeString(), node->GetJobName() );

	bool submit_success = false;

 	node->_submitTries++;
	if ( node->GetNoop() ) {
   		submit_success = fake_condor_submit( condorID, 0,
					node->GetJobName(), node->GetDirectory(),
					_defaultNodeLog );
	} else if (use_condor_submit) {
#if 0
		// Note: assigning the ParentListString() return value
		// to a variable here, instead of just passing it directly
		// to condor_submit(), fixes a memory leak(!).
		// wenger 2008-12-18
		MyString parents = ParentListString( node );
#else
		std::string parents("");
		if ( ! node->NoParents()) {
			parents.reserve(2048);
			node->PrintParents(parents, 2000, this, ",");
		}
#endif

			// This is to allow specifying a top-level batch name of
			// " " to *not* override batch names specified at a lower
			// level.
		const char *batchName;
		std::string batchId;
		if ( !node->GetDagFile() && dm._batchName == " " ) {
			batchName = "";
		} else {
			batchName = dm._batchName.c_str();
		}
		if ( !node->GetDagFile() && dm._batchId == " " ) {
			batchId = "";
		} else {
			batchId = dm._batchId.c_str();
		}

		submit_success = condor_submit( dm, node->GetCmdFile(), condorID,
					node->GetJobName(), parents.c_str(),
					node, node->_effectivePriority,
					node->GetRetries(),
					node->GetDirectory(), _defaultNodeLog,
					( ! node->NoChildren()) && dm._claim_hold_time > 0,
					batchName, batchId );
	} else {
#ifdef DEAD_CODE
		MyString parents = ParentListString(node);
		if (parents.empty() && ! node->NoParents()) {
			debug_printf(DEBUG_NORMAL, "Warning: node %s has too many parents "
				"to list in its classad; leaving its DAGParentNodeNames "
				"attribute undefined\n", node->GetJobName());
			check_warning_strictness(DAG_STRICT_3);
		}
#else
		std::string parents("");
		if ( ! node->NoParents()) {
			parents.reserve(2048);
			node->PrintParents(parents, 2000, this, ",");
		}
#endif

		// This is to allow specifying a top-level batch name of
		// " " to *not* override batch names specified at a lower
		// level.
		const char *batchName;
		const char *batchId;
		if (!node->GetDagFile() && dm._batchName == " ") {
			batchName = "";
		} else {
			batchName = dm._batchName.c_str();
		}
				if ( !node->GetDagFile() && dm._batchId == " " ) {
			batchId = "";
		} else {
			batchId = dm._batchId.c_str();
		}

		submit_success = direct_condor_submit(dm, node,
			_defaultNodeLog, parents.c_str(), batchName, batchId, condorID);
	}

	result = submit_success ? SUBMIT_RESULT_OK : SUBMIT_RESULT_FAILED;

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
    // submit event once the latter appears in the HTCondor job log
	_submitQ->push( node );

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
	auto result = GetEventIDHash( node->GetNoop() )->insert( std::make_pair( id, node ) );
		// std::map::insert() returns a pair, second element is the success bool
	ASSERT( result.second == true );

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

    // Flag the status file as outdated so it gets updated soon.
    _statusFileOutdated = true;

	// Set the times to wait twice as long as last time.
	int thisSubmitDelay = _nextSubmitDelay;
	_nextSubmitTime = time(NULL) + thisSubmitDelay;
	_nextSubmitDelay *= 2;

	if ( _dagStatus == DagStatus::DAG_STATUS_RM && node->GetType() != NodeType::FINAL ) {
		max_submit_attempts = min( max_submit_attempts, 2 );
	}

	if ( node->_submitTries >= max_submit_attempts ) {
			// We're out of submit attempts, treat this as a submit failure.

			// To match the existing behavior, we're resetting the times
			// here.
		_nextSubmitTime = 0;
		_nextSubmitDelay = 1;

		debug_printf( DEBUG_QUIET, "Job submit failed after %d tr%s.\n",
				node->_submitTries, node->_submitTries == 1 ? "y" : "ies" );

		node->error_text.formatstr( "Job submit failed" );

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
			_readyQ->Prepend(node, -node->_effectivePriority);
		} else {
			_readyQ->Append(node, -node->_effectivePriority);
		}
	}
}

//---------------------------------------------------------------------------
void
Dag::DecrementProcCount( Job *node )
{
	node->_queuedNodeJobProcs--;
	ASSERT( node->_queuedNodeJobProcs >= 0 );

	if( !node->is_cluster && node->_queuedNodeJobProcs == 0 ) {
		UpdateJobCounts( node, -1 );
		node->Cleanup();
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
Dag::PropagateDirectoryToAllNodes(void)
{
	MyString key;

	if (m_directory == ".") {
		return;
	}

	// Propagate the directory setting to all nodes in the DAG.
	for (auto it = _jobs.begin(); it != _jobs.end(); it++) {
		(*it)->PrefixDirectory(m_directory);
	}

	// I wipe out m_directory here. If this gets called multiple
	// times for a specific DAG, it'll prefix multiple times, and that is most
	// likely wrong.

	m_directory = ".";
}

//-------------------------------------------------------------------------
bool
Dag::SetPinInOut( bool isPinIn, const char *nodeName, int pinNum )
{
	debug_printf( DEBUG_DEBUG_1, "Dag(%s)::SetPinInOut(%d, %s, %d)\n",
				_spliceScope.c_str(), isPinIn, nodeName, pinNum );

	ASSERT( pinNum > 0 );

	Job *node = FindNodeByName( nodeName );
	if ( !node ) {
		debug_printf( DEBUG_QUIET, "ERROR: node %s not found!\n", nodeName );
		return false;
	}

	bool result = false;
	if ( isPinIn ) {
		result = SetPinInOut( _pinIns, node, pinNum );
	} else {
		result = SetPinInOut( _pinOuts, node, pinNum );
	}

	return result;
}

//---------------------------------------------------------------------------
bool
Dag::SetPinInOut( PinList &pinList, Job *node, int pinNum )
{
	--pinNum; // Pin numbers start with 1
	if ( pinNum >= static_cast<int>( pinList.size() ) ) {
		pinList.resize( pinNum+1, NULL );
	}
	PinNodes *pn = pinList[pinNum];
	if ( !pn ) {
		pinList[pinNum] = new PinNodes();
		pn = pinList[pinNum];
	}
	pn->push_back( node );

	return true;
}

//---------------------------------------------------------------------------
const Dag::PinNodes *
Dag::GetPinInOut( bool isPinIn, int pinNum ) const
{
	debug_printf( DEBUG_DEBUG_1, "Dag(%s)::GetPinInOut(%d, %d)\n",
				_spliceScope.c_str(), isPinIn, pinNum );

	ASSERT( pinNum > 0 );

	const PinNodes *pn;
	if ( isPinIn ) {
		pn = GetPinInOut( _pinIns, "in", pinNum );
	} else {
		pn = GetPinInOut( _pinOuts, "out", pinNum );
	}

	return pn;
}

//---------------------------------------------------------------------------
const Dag::PinNodes *
Dag::GetPinInOut( const PinList &pinList, const char *inOutStr,
			int pinNum )
{
	--pinNum; // Pin numbers start with 1
	if ( pinNum >= static_cast<int>( pinList.size() ) ) {
		debug_printf( DEBUG_QUIET,
					"ERROR: pin %s number %d specified; max is %d\n",
					inOutStr, pinNum+1, static_cast<int>( pinList.size() ) );
		return NULL;
	} else {
		return pinList[pinNum];
	}
}

//---------------------------------------------------------------------------
int
Dag::GetPinCount( bool isPinIn )
{
	if ( isPinIn ) {
		return (int)_pinIns.size();
	} else {
		return (int)_pinOuts.size();
	}
}

//---------------------------------------------------------------------------
bool
Dag::ConnectSplices( Dag *parentSplice, Dag *childSplice )
{
	debug_printf( DEBUG_DEBUG_1, "Dag::ConnectSplices(%s, %s)\n",
				parentSplice->_spliceScope.c_str(),
				childSplice->_spliceScope.c_str() );

	MyString parentName = parentSplice->_spliceScope;
		// Trim trailing '+' from parentName.
	int last = parentName.length() - 1;
	ASSERT( last >= 0 );
	if ( parentName[last] == '+' ) {
		parentName.truncate( last );
	}

	MyString childName = childSplice->_spliceScope;
		// Trim trailing '+' from childName.
	last = childName.length() - 1;
	ASSERT( last >= 0 );
	if ( childName[last] == '+' ) {
		childName.truncate( last );
	}

		// Make sure the parent and child splices have pin_ins/pin_outs
		// as appropriate, and that the number of pin_ins and pin_outs
		// matches.
	int pinOutCount = parentSplice->GetPinCount( false );
	if ( pinOutCount <= 0 ) {
		debug_printf( DEBUG_QUIET,
					"ERROR: parent splice %s has 0 pin_outs\n",
					parentName.c_str() );
		return false;
	}

	int pinInCount = childSplice->GetPinCount( true );
	if ( pinInCount <= 0 ) {
		debug_printf( DEBUG_QUIET,
					"ERROR: child splice %s has 0 pin_ins\n",
					childName.c_str() );
		return false;
	}

	if ( pinOutCount != pinInCount ) {
		debug_printf( DEBUG_QUIET,
					"ERROR: pin_in/out mismatch:  parent splice %s has %d pin_outs; child splice %s has %d pin_ins\n",
					parentName.c_str(), pinOutCount,
					childName.c_str(), pinInCount );
		return false;
	}

#ifdef DEAD_CODE
#else
	MyString failReason;
#endif

		// Go thru the pin_in/pin_out lists, and add parent/child
		// dependencies between splices as appropriate.  (Note that
		// we will catch any missing pin_in/pin_out numbers here.)
	for (int pinNum = 1; pinNum <= pinOutCount; ++pinNum ) {
		const PinNodes *parentPNs = parentSplice->GetPinInOut( false, pinNum );
		if ( !parentPNs ) {
			debug_printf( DEBUG_QUIET,
						"ERROR: parent splice %s has no node for pin_out %d\n",
						parentName.c_str(), pinNum );
			return false;
		}

		const PinNodes *childPNs = childSplice->GetPinInOut( true, pinNum );
		if ( !childPNs ) {
			debug_printf( DEBUG_QUIET,
						"ERROR: child splice %s has no node for pin_in %d\n",
						childName.c_str(), pinNum );
			return false;
		}

		for ( int parentNodeNum = 0;
					parentNodeNum < static_cast<int>( parentPNs->size() );
					++parentNodeNum ) {
			Job *parentNode = parentPNs->at(parentNodeNum);
			for ( int childNodeNum = 0;
						childNodeNum < static_cast<int>( childPNs->size() );
						++childNodeNum ) {
				Job *childNode = childPNs->at(childNodeNum);

			#ifdef DEAD_CODE
				if ( !AddDependency( parentNode, childNode ) ) {
			#else
				std::forward_list<Job*> lst = { childNode };
				if ( ! parentNode->AddChildren(lst, failReason)) {
			#endif
					debug_printf( DEBUG_QUIET,
								"ERROR: unable to add parent/child dependency for pin %d\n", pinNum );
		
					return false;
				}
			}
		}
	}

		// Check for "orphan" nodes in the child splice -- nodes that
		// don't have either a parent within the splice or a pin_in
		// connection.
	for (auto it = childSplice->_jobs.begin(); it != childSplice->_jobs.end(); it++) {
		if ((*it)->NoParents()) {
			debug_printf( DEBUG_QUIET,
						"ERROR: child splice node %s has no parents after making pin connections; add pin_in or parent\n",
						(*it)->GetJobName() );
			return false;
		}
	}

	return true;
}

//---------------------------------------------------------------------------
void
Dag::DeletePinList( PinList &pinList )
{
	for ( int pinNum = 0; pinNum < static_cast<int>( pinList.size() );
				++pinNum ) {
		PinNodes *pn = pinList[pinNum];
		delete pn;
	}
}

//---------------------------------------------------------------------------
void
Dag::PrefixAllNodeNames(const MyString &prefix)
{
	MyString key;

	debug_printf(DEBUG_DEBUG_1, "Entering: Dag::PrefixAllNodeNames()"
		" with prefix %s\n",prefix.c_str());

	for (auto it = _jobs.begin(); it != _jobs.end(); it++) {
		(*it)->PrefixName(prefix);
	}

	// Here we must reindex the hash view with the prefixed name.
	// also fix my node name hash view of the jobs

	// First, wipe out the index.
	auto it = _nodeNameHash.begin();
	while (it != _nodeNameHash.end()) {
		it = _nodeNameHash.erase(it);
	}

	// Then, reindex all the jobs keyed by their new name
	for (auto it = _jobs.begin(); it != _jobs.end(); it++) {
		key = (*it)->GetJobName();
		auto insertResult = _nodeNameHash.insert(std::make_pair(key, *it));
		if (insertResult.second != true) {
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
bool 
Dag::InsertSplice(MyString spliceName, Dag *splice_dag)
{
	auto insertResult = _splices.insert(std::make_pair(spliceName, splice_dag));
	return insertResult.second;
}

//---------------------------------------------------------------------------
Dag*
Dag::LookupSplice(MyString name)
{
	auto findResult = _splices.find(name);
	if (findResult == _splices.end()) {
		return nullptr;
	}
	return (*findResult).second;
}

//---------------------------------------------------------------------------
// This represents not the actual initial nodes of the dag just after
// the file containing the dag had been parsed.
// You must NOT free the returned array or the contained pointers.
std::vector<Job*>*
Dag::InitialRecordedNodes(void)
{
	return &_splice_initial_nodes;
}

//---------------------------------------------------------------------------
// This represents not the actual final nodes of the dag just after
// the file containing the dag had been parsed.
// You must NOT free the returned array or the contained pointers.
std::vector<Job*>*
Dag::FinalRecordedNodes(void)
{
	return &_splice_terminal_nodes;
}


//---------------------------------------------------------------------------
// After we parse the dag, let's remember which ones were the initial and 
// terminal nodes in the dag (in case this dag was used as a splice).
void
Dag::RecordInitialAndTerminalNodes(void)
{
	for (auto it = _jobs.begin(); it != _jobs.end(); it++) {

		// record the initial nodes
		if ((*it)->NoParents()) {
			_splice_initial_nodes.push_back(*it);
		}

		// record the final nodes
		if ((*it)->NoChildren()) {
			_splice_terminal_nodes.push_back(*it);
		}
	}
}

//---------------------------------------------------------------------------
// Make a copy of the _jobs array and return it. This will also remove the
// nodes out of _jobs so the destructor doesn't delete them.
OwnedMaterials*
Dag::RelinquishNodeOwnership(void)
{
	MyString key;

	std::vector<Job*> *nodes = new std::vector<Job*>();

	// 1. Copy the jobs
	auto it = _jobs.begin();
	while (it != _jobs.end()) {
		nodes->push_back(*it);
		it = _jobs.erase(it);
	}

	// shove it into a packet and give it back
	return new OwnedMaterials(nodes, &_catThrottles, _reject,
				_firstRejectLoc);
}


//---------------------------------------------------------------------------
OwnedMaterials*
Dag::LiftSplices(SpliceLayer layer)
{
	//PrintJobList();
	MyString key;
	OwnedMaterials *om = NULL;

	// if this splice contains no other splices, then relinquish the nodes I own
	if (layer == DESCENDENTS && _splices.size() == 0) {
		return RelinquishNodeOwnership();
	}

	// recurse down the splice tree moving everything up into myself.
	for (auto& splice: _splices) {
		debug_printf(DEBUG_DEBUG_1, "Lifting splice %s\n", splice.first.Value());
		om = splice.second->LiftSplices(DESCENDENTS);
		// this function moves what it needs out of the returned object
		AssumeOwnershipofNodes(splice.first, om);
		delete om;
	}

	// Now delete all of them.
	auto it = _splices.begin();
	while(it != _splices.end()) {
		it = _splices.erase(it);
	}
	ASSERT( _splices.size() == 0 );

	// and prefix them if there was a DIR for the dag.
	PropagateDirectoryToAllNodes();

	// base case is above.
	return NULL;
}

void
Dag::AdjustEdges()
{
	for (auto it = _jobs.begin(); it != _jobs.end(); it++) {
		(*it)->BeginAdjustEdges(this);
	}
	for (auto it = _jobs.begin(); it != _jobs.end(); it++) {
		(*it)->AdjustEdges(this);
	}
	for (auto it = _jobs.begin(); it != _jobs.end(); it++) {
		(*it)->FinalizeAdjustEdges(this);
	}
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
	unsigned int i;
	MyString key;
	JobID_t key_id;

	std::vector<Job*> *nodes = om->nodes;

	// 0. Take ownership of the categories

	// Merge categories from the splice into this DAG object.  If the
	// same category exists in both (whether global or non-global) the
	// higher-level value overrides the lower-level value.

	// Note: by the time we get to here, all category names have already
	// been prefixed with the proper scope.
	ThrottleByCategory::ThrottleInfo *spliceThrottle;
	for ( auto throttle: *om->throttles->GetThrottles() ) {
		spliceThrottle = throttle.second;
		ThrottleByCategory::ThrottleInfo *mainThrottle =
					_catThrottles.GetThrottleInfo(
					spliceThrottle->_category );
		if ( mainThrottle && mainThrottle->isSet() &&
					mainThrottle->_maxJobs != spliceThrottle->_maxJobs ) {
			debug_printf( DEBUG_NORMAL, "Warning: higher-level (%s) "
						"maxjobs value of %d for category %s overrides "
						"splice %s value of %d\n", _spliceScope.c_str(),
						mainThrottle->_maxJobs,
						mainThrottle->_category->c_str(),
						spliceName.c_str(), spliceThrottle->_maxJobs );
			check_warning_strictness( DAG_STRICT_2 );
		} else {
			_catThrottles.SetThrottle( spliceThrottle->_category,
						spliceThrottle->_maxJobs );
		}
	}

	// 1. Take ownership of the nodes

	// 1a. If there are any actual initial/final nodes, then ensure to record
	// it into the recorded initial and final nodes for this node.
	for (i = 0; i < nodes->size(); i++) {
		if ((*nodes)[i]->NoParents()) {
			_splice_initial_nodes.push_back((*nodes)[i]);
			continue;
		}
		if ((*nodes)[i]->NoChildren()) {
			_splice_terminal_nodes.push_back((*nodes)[i]);
		}
	}

	// 1. Take ownership of the nodes
	// 1b. Re-set the node categories (if any) so they point to the
	// ThrottleByCategory object in *this* DAG rather than the splice
	// DAG (which will be deleted soon).
	for ( i = 0; i < nodes->size(); i++ ) {
		Job *tmpNode = (*nodes)[i];
		spliceThrottle = tmpNode->GetThrottleInfo();
		if ( spliceThrottle != NULL ) {
				// Now re-set the category in the node, so that the
				// category info points to the upper DAG rather than the
				// splice DAG.
			tmpNode->SetCategory( spliceThrottle->_category->c_str(),
						_catThrottles );
		}
	}

	// 1c. Copy the nodes into _jobs.
	for (i = 0; i < nodes->size(); i++) {
		_jobs.push_back((*nodes)[i]);
	}

	// 2. Update our name hash to include the new nodes.
	for (i = 0; i < nodes->size(); i++) {
		key = (*nodes)[i]->GetJobName();

		debug_printf(DEBUG_DEBUG_1, "Creating view hash fixup for: job %s\n", 
			key.c_str());

		auto insertResult = _nodeNameHash.insert(std::make_pair(key, (*nodes)[i]));
		if (insertResult.second == false) {
			debug_printf(DEBUG_QUIET, 
				"Found name collision while taking ownership of node: %s\n",
				key.c_str());
			debug_printf(DEBUG_QUIET, "Trying to insert key %s, node:\n",
				key.c_str());
			(*nodes)[i]->Dump( this );
			debug_printf(DEBUG_QUIET, "but it collided with key %s, node:\n", 
				key.Value());
			auto findResult = _nodeNameHash.find(key);
			if (findResult != _nodeNameHash.end()) {
				job = (*findResult).second;
				job->Dump( this );
			} else {
				debug_error(1, DEBUG_QUIET, "What? This is impossible!\n");
			}

			debug_error(1, DEBUG_QUIET, "Aborting.\n");
		}
	}

	// 3. Update our node id hash to include the new nodes.
	for (i = 0; i < nodes->size(); i++) {
		key_id = (*nodes)[i]->GetJobID();
		auto insertResult = _nodeIDHash.insert(std::make_pair(key_id, (*nodes)[i])) ;
		if (insertResult.second != true) {
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

#ifdef DEAD_CODE // now done by $() substitution in submit or submit_utils
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
#endif

//---------------------------------------------------------------------------
// Iterate over the jobs and set the effective priorities for the nodes.
void Dag::SetNodePriorities()
{
	if ( GetDagPriority() != 0 ) {
		for (auto it = _jobs.begin(); it != _jobs.end(); it++) {
			(*it)->_effectivePriority += GetDagPriority();
		}
	}
}
