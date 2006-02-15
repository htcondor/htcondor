/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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

#include "simplelist.h"
#include "condor_string.h"  /* for strnewp() */
#include "string_list.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

#if defined(BUILD_HELPER)
#include "helper.h"
#endif

//---------------------------------------------------------------------------
void touch (const char * filename) {
    int fd = open(filename, O_RDWR | O_CREAT, 0600);
    if (fd == -1) {
        debug_error( 1, DEBUG_QUIET, "Error: can't open %s\n", filename );
    }
    close (fd);
}

//---------------------------------------------------------------------------
Dag::Dag( /* const */ StringList &dagFiles, char *condorLogName,
		  const int maxJobsSubmitted,
		  const int maxPreScripts, const int maxPostScripts,
		  int allow_events, const char* dapLogName, bool allowLogError,
		  bool useDagDir, int maxIdleJobProcs, bool retrySubmitFirst,
		  bool retryNodeFirst, const char *condorRmExe,
		  const char *storkRmExe) :
    _maxPreScripts        (maxPreScripts),
    _maxPostScripts       (maxPostScripts),
	DAG_ERROR_CONDOR_SUBMIT_FAILED (-1001),
	DAG_ERROR_CONDOR_JOB_ABORTED (-1002),
	DAG_ERROR_DAGMAN_HELPER_COMMAND_FAILED (-1101),
	_condorLogName		  (NULL),
    _condorLogInitialized (false),
    _dapLogName           (NULL),
    _dapLogInitialized    (false),             //<--DAP
    _numNodesDone          (0),
    _numNodesFailed        (0),
    _numJobsSubmitted     (0),
    _maxJobsSubmitted     (maxJobsSubmitted),
	_numIdleJobProcs		  (0),
	_maxIdleJobProcs		  (maxIdleJobProcs),
	_allowLogError		  (allowLogError),
	m_retrySubmitFirst	  (retrySubmitFirst),
	m_retryNodeFirst	  (retryNodeFirst),
	_condorRmExe		  (condorRmExe),
	_storkRmExe			  (storkRmExe),
	_preRunNodeCount	  (0),
	_postRunNodeCount	  (0),
	_checkCondorEvents    (allow_events),
	_checkStorkEvents     (allow_events)
{
	ASSERT( dagFiles.number() >= 1 );

	_condorLogName = strnewp( condorLogName );
	ASSERT( _condorLogName );

	if( dapLogName ) {
		_dapLogName = strnewp( dapLogName );
		ASSERT( _dapLogName );
	}

	PrintDagFiles( dagFiles );

	FindLogFiles( dagFiles, useDagDir );

	ASSERT( TotalLogFileCount() > 0 ) ;

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
	_nextSubmitTime = 0;
	_nextSubmitDelay = 1;
	_recovery = false;

	return;
}

//-------------------------------------------------------------------------
Dag::~Dag() {
		// remember kids, delete is safe *even* if ptr == NULL...

	delete[] _condorLogName;

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
void
Dag::InitializeDagFiles( char *lockFileName, bool deleteOldLogs )
{
		// if there is an older version of the log files,
		// we need to delete these.

		// pfc: why in the world would this be a necessary or good
		// thing?  apparently b/c old submit events from a
		// previous run of the same dag will contain the same dag
		// node names as those from current run, which will
		// completely f0rk the event-processing process (dagman
		// will see the events from the first run and think they
		// are from this one)...

		// instead of deleting the old logs, which seems evil,
		// maybe what we need is to add the submitting dagman
		// daemon's job id in each of the submit events in
		// addition to the dag node name we already write, so that
		// we can differentiate between identically-named nodes
		// from different dag instances.  (to take this to the
		// extreme, we might also want a unique schedd id in there
		// too...)  Or, we can do as Doug Thain suggests, and
		// have DAGMan keep its own log independant of
		// Condor -- but Miron hates that idea...

	if( deleteOldLogs ) {
	debug_printf( DEBUG_VERBOSE,
				  "Deleting any older versions of log files...\n" );

	MultiLogFiles::DeleteLogs( _condorLogFiles );
	MultiLogFiles::DeleteLogs( _storkLogFiles );

	if ( _condorLogName != NULL ) touch (_condorLogName);  //<-- DAP
	}
	touch (lockFileName);
}

//-------------------------------------------------------------------------
bool Dag::Bootstrap (bool recovery) {
    Job* job;
    ListIterator<Job> jobs (_jobs);

	_recovery = recovery;

    // update dependencies for pre-completed jobs (jobs marked DONE in
    // the DAG input file)
    jobs.ToBeforeFirst();
    while( jobs.Next( job ) ) {
		if( job->GetStatus() == Job::STATUS_DONE ) {
			TerminateJob( job, true );
		}
    }
    debug_printf( DEBUG_VERBOSE, "Number of pre-completed nodes: %d\n",
				  NumNodesDone() );
    
    if (recovery) {
        debug_printf( DEBUG_NORMAL, "Running in RECOVERY mode...\n" );

		if( _condorLogFiles.number() > 0 ) {
			if( !ProcessLogEvents( CONDORLOG, recovery ) ) {
				_recovery = false;
				return false;
			}
		}
		if( _storkLogFiles.number() > 0 ) {
			if( !ProcessLogEvents( DAPLOG, recovery ) ) {
				_recovery = false;
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
			StartNode( job, false );
		}
    }

	_recovery = false;
    
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
		_condorLogInitialized = _condorLogRdr.initialize( _condorLogFiles );
		if( !_condorLogInitialized ) {
				// this can be normal before we've actually submitted
				// any jobs and the log doesn't yet exist, but is
				// likely a problem if it persists...
			debug_printf( DEBUG_VERBOSE, "ERROR: failed to initialize condor "
						  "job log -- ignore unless error repeats\n");
			return false;
		}
    }

	bool growth = _condorLogRdr.detectLogGrowth();
    debug_printf( DEBUG_DEBUG_4, "%s\n",
				  growth ? "Log GREW!" : "No log growth..." );
    return growth;
}

//-------------------------------------------------------------------------
bool Dag::DetectDaPLogGrowth () {
	if( _storkLogFiles.number() <= 0 ) {
		debug_printf( DEBUG_DEBUG_1, "WARNING: DetectDaPLogGrowth() called "
					  "but no Stork log defined\n" );
		return false;
	}

    if (!_dapLogInitialized) {
		_dapLogInitialized = _storkLogRdr.initialize( _storkLogFiles );
		if( !_dapLogInitialized ) {
				// this can be normal before we've actually submitted
				// any jobs and the log doesn't yet exist, but is
				// likely a problem if it persists...
			debug_printf( DEBUG_VERBOSE, "ERROR: failed to initialize Stork "
						  "job log -- ignore unless error repeats\n");
			return false;
		}
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
		if ( !_condorLogInitialized ) {
			_condorLogInitialized = _condorLogRdr.initialize(_condorLogFiles);
		}
	} else if ( logsource == DAPLOG ) {
		if ( !_dapLogInitialized ) {
			_dapLogInitialized = _storkLogRdr.initialize(_storkLogFiles);
		}
	}

	bool done = false;  // Keep scanning until ULOG_NO_EVENT
	bool result = true;

	while (!done) {

		ULogEvent* e = NULL;
		ULogEventOutcome outcome;

		if ( logsource == CONDORLOG ) {
			outcome = _condorLogRdr.readEvent(e);
		} else if ( logsource == DAPLOG ){
			outcome = _storkLogRdr.readEvent(e);
		}

		//TEMP -- probably need to pass into here whether this is Condor or Stork
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
		debug_printf( DEBUG_QUIET, "    ------------------------------\n");
		debug_printf( DEBUG_QUIET, "       %s Recovery Complete\n", name);
		debug_printf( DEBUG_QUIET, "    ------------------------------\n");
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
			Job *job = LogEventNodeLookup( logsource, event, recovery );
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
			case ULOG_JOB_ABORTED:
				ProcessAbortEvent(event, job, recovery);
					// Make sure we don't count finished jobs as idle.
				ProcessNotIdleEvent(event, job);
				break;
              
			case ULOG_JOB_TERMINATED:
				ProcessTerminatedEvent(event, job, recovery);
					// Make sure we don't count finished jobs as idle.
				ProcessNotIdleEvent(event, job);
				break;

			case ULOG_POST_SCRIPT_TERMINATED:
				ProcessPostTermEvent(event, job, recovery);
				break;

			case ULOG_SUBMIT:
				ProcessSubmitEvent(event, job, recovery);
				ProcessIsIdleEvent(event, job);
				break;

			case ULOG_JOB_EVICTED:
			case ULOG_JOB_SUSPENDED:
			case ULOG_JOB_HELD:
				ProcessIsIdleEvent(event, job);
				break;

			case ULOG_EXECUTE:
				ProcessNotIdleEvent(event, job);
				break;

			case ULOG_JOB_UNSUSPENDED:
			case ULOG_JOB_RELEASED:
			case ULOG_CHECKPOINTED:
			case ULOG_IMAGE_SIZE:
			case ULOG_NODE_EXECUTE:
			case ULOG_NODE_TERMINATED:
			case ULOG_SHADOW_EXCEPTION:
			case ULOG_GENERIC:
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
		job->_queuedNodeJobProcs--;
		ASSERT( job->_queuedNodeJobProcs >= 0 );
		if( job->_queuedNodeJobProcs == 0 ) {
			_numJobsSubmitted--;
		}
		ASSERT( _numJobsSubmitted >= 0 );

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

		ProcessJobProcEnd( job, recovery, false, true );
	}
}

//---------------------------------------------------------------------------
void
Dag::ProcessTerminatedEvent(const ULogEvent *event, Job *job,
		bool recovery) {
	if( job ) {
		job->_queuedNodeJobProcs--;
		ASSERT( job->_queuedNodeJobProcs >= 0 );
		if( job->_queuedNodeJobProcs == 0 ) {
			_numJobsSubmitted--;
		}
		ASSERT( _numJobsSubmitted >= 0 );

		JobTerminatedEvent * termEvent = (JobTerminatedEvent*) event;


		bool isError = !(termEvent->normal && termEvent->returnValue == 0);

		if( isError ) {
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

			if( job->_scriptPost == NULL ) {
				CheckForDagAbort(job, job->retval, "node");
				// if dag abort happened, we never return here!
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

		ProcessJobProcEnd( job, recovery, isError, false );

		PrintReadyQ( DEBUG_DEBUG_2 );

		return;
	}
}

//---------------------------------------------------------------------------
void
Dag::RemoveBatchJob(Job *node) {

	char cmd[ARG_MAX];

	switch ( node->JobType() ) {
	case Job::TYPE_CONDOR:
       	snprintf( cmd, ARG_MAX, "%s %d", _condorRmExe,
					node->_CondorID._cluster);
		break;

	case Job::TYPE_STORK:
       	snprintf( cmd, ARG_MAX, "%s %d", _storkRmExe,
					node->_CondorID._cluster);
		break;

	default:
		ASSERT( false );
		break;
	}
	
	debug_printf( DEBUG_VERBOSE, "Executing: %s\n", cmd );
	if ( util_popen( cmd ) ) {
			// Note: error here can't be fatal because there's a
			// race condition where you could do a condor_rm on
			// a job that already terminated.  wenger 2006-02-08.
		debug_printf( DEBUG_VERBOSE, "Error removing DAG node jobs\n");
	}
}

//---------------------------------------------------------------------------
void
Dag::ProcessJobProcEnd(Job *job, bool recovery, bool isError, bool isAbort) {

	if ( isError || isAbort ) {
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
			// Note: "|| recovery" below is somewhat of a "quick and dirty"
			// fix to Gnats PR 357.  The first part of the assert can fail
			// in recovery mode because if any retries of a node failed
			// because the post script failed, they don't show up in the
			// user log.  Therefore, condor_dagman doesn't know abou them,
			// and can think the post script was run before all retries
			// were exhausted.  wenger 2004-11-23.
		ASSERT( job->GetStatus() == Job::STATUS_POSTRUN || recovery );

		_postRunNodeCount--;

		PostScriptTerminatedEvent *termEvent =
			(PostScriptTerminatedEvent*) event;

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
				CheckForDagAbort(job, termEvent->returnValue, "POST script");

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
				else if( mainJobRetval < 0  && mainJobRetval >= -64 ) {
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

		PrintReadyQ( DEBUG_DEBUG_4 );
	}
}

//---------------------------------------------------------------------------
void
Dag::ProcessSubmitEvent(const ULogEvent *event, Job *job, bool recovery) {

	if ( !job ) {
		return;
	}

	job->_queuedNodeJobProcs++;

		// Note:  in non-recovery mode, we increment _numJobsSubmitted
		// in SubmitReadyJobs().
	if ( recovery ) {
		job->_Status = Job::STATUS_SUBMITTED;

			// Only increment the submitted job count on
			// the *first* proc of a job.
		if( job->_queuedNodeJobProcs == 1 ) {
			_numJobsSubmitted++;
		}
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
Dag::ProcessIsIdleEvent(const ULogEvent *event, Job *job) {

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
Dag::ProcessNotIdleEvent(const ULogEvent *event, Job *job) {

	if ( !job ) {
		return;
	}

	if ( job->GetIsIdle() ) {
		job->SetIsIdle(false);
		_numIdleJobProcs--;
	}

	// Do some consistency checks here?

	debug_printf( DEBUG_VERBOSE, "Number of idle job procs: %d\n",
				_numIdleJobProcs);
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

//---------------------------------------------------------------------------
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
Job * Dag::GetJob (int logsource, const CondorID condorID) const {
	if ( condorID == CondorID(-1, -1, -1) ) {
		return NULL;
	}
    ListIterator<Job> iList (_jobs);
    Job * job;
    while ((job = iList.Next())) {
			// Note: we're comparing only on cluster ID
			// here so all procs in a single cluster get
			// mapped back to the corresponding node.
		if (job->_CondorID._cluster == condorID._cluster &&
					logsource == job->JobType()) return job;
	}
    return NULL;
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
void
Dag::FindLogFiles( /* const */ StringList &dagFiles, bool useDagDir )
{

	if ( _dapLogName ) {
		_storkLogFiles.append( _dapLogName );
	}

	MyString	msg;
	if ( !GetLogFiles( dagFiles, useDagDir, _condorLogFiles, _storkLogFiles,
				msg ) ) {
		debug_printf( DEBUG_VERBOSE,
				"Possible error when parsing DAG: %s ...\n", msg.Value());
		if ( _allowLogError ) {
			debug_printf( DEBUG_VERBOSE,
					"...continuing anyhow because of -AllowLogError "
					"flag (beware!)\n");
		} else {
			debug_printf( DEBUG_VERBOSE, "...exiting -- try again with "
					"the '-AllowLogError' flag if you *really* think "
					"this shouldn't be a fatal error\n");
			DC_Exit( 1 );
		}
	}
	
	debug_printf( DEBUG_VERBOSE, "All DAG node user log files:\n");
	if ( _condorLogFiles.number() > 0 ) {
		_condorLogFiles.rewind();
		const char *logfile;
		while( (logfile = _condorLogFiles.next()) ) {
			debug_printf( DEBUG_VERBOSE, "  %s (Condor)\n", logfile );
		}
	}

	if ( _storkLogFiles.number() > 0 ) {
		_storkLogFiles.rewind();
		const char *logfile;
		while( (logfile = _storkLogFiles.next()) ) {
			debug_printf( DEBUG_VERBOSE, "  %s (Stork)\n", logfile );
		}
	}
}

//-------------------------------------------------------------------------
bool
Dag::StartNode( Job *node, bool isRetry )
{
    ASSERT( node != NULL );
	ASSERT( node->CanSubmit() );

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
		_readyQ->Prepend( node );
	} else {
		_readyQ->Append( node );
	}
	return TRUE;
}

// returns number of jobs submitted
int
Dag::SubmitReadyJobs(const Dagman &dm)
{
#if defined(BUILD_HELPER)
	Helper helperObj;
#endif

		// Check whether we have to wait longer before submitting again
		// (if a previous submit attempt failed).
	if ( _nextSubmitTime && time(NULL) < _nextSubmitTime) {
		sleep(1);
        return 0;
	}


	int numSubmitsThisCycle = 0;
	while( numSubmitsThisCycle < dm.max_submits_per_interval ) {

//	PrintReadyQ( DEBUG_DEBUG_4 );

	// no jobs ready to submit
    if( _readyQ->IsEmpty() ) {
        return numSubmitsThisCycle;
    }

    // max jobs already submitted
    if( _maxJobsSubmitted && (_numJobsSubmitted >= _maxJobsSubmitted) ) {
        debug_printf( DEBUG_VERBOSE,
                      "Max jobs (%d) already running; "
					  "deferring submission of %d ready job%s.\n",
                      _maxJobsSubmitted, _readyQ->Number(),
					  _readyQ->Number() == 1 ? "" : "s" );
        return numSubmitsThisCycle;
    }
	if ( _maxIdleJobProcs && (_numIdleJobProcs >= _maxIdleJobProcs) ) {
        debug_printf( DEBUG_VERBOSE,
					  "Hit max number of idle DAG nodes (%d); "
					  "deferring submission of %d ready job%s.\n",
					  _maxIdleJobProcs, _readyQ->Number(),
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

	if( job->NumParents() > 0 && dm.submit_delay == 0 &&
				TotalLogFileCount() > 1 ) {
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
		debug_printf( DEBUG_VERBOSE,
					"Sleeping for one second for log file consistency\n" );
		sleep( 1 );
	}

		// sleep for a specified time before submitting
	if( dm.submit_delay ) {
		debug_printf( DEBUG_VERBOSE, "Sleeping for %d s "
					  "(DAGMAN_SUBMIT_DELAY) to throttle submissions...\n",
					  dm.submit_delay );
		sleep( dm.submit_delay );
	}

	debug_printf( DEBUG_VERBOSE, "Submitting %s Node %s job(s)...\n",
				  job->JobTypeString(), job->GetJobName() );

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
	snprintf( job->error_text, JOB_ERROR_TEXT_MAXLEN, "helper (%s) failed",
		 helper );
	free( helper );
	helper = NULL;

	// NOTE: this failure short-circuits the "retry" feature
	job->retries = job->GetRetryMax();

	if( job->_scriptPost ) {
	  // a POST script is specified for the job, so run it
	  job->_Status = Job::STATUS_POSTRUN;
	  _postRunNodeCount++;
	  job->retval = DAG_ERROR_DAGMAN_HELPER_COMMAND_FAILED;
	  job->_scriptPost->_retValJob = job->retval;
	  _postScriptQ->Run( job->_scriptPost );
	} else {
	  job->_Status = Job::STATUS_ERROR;
	  _numNodesFailed++;
	}
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

	bool submit_success;
    CondorID condorID(0,0,0);

    if( job->JobType() == Job::TYPE_CONDOR ) {
	  job->_submitTries++;
      submit_success =
		  condor_submit( dm, cmd_file.Value(), condorID, job->GetJobName(),
						 ParentListString( job ), job->varNamesFromDag,
						 job->varValsFromDag, job->GetDirectory() );
    } else if( job->JobType() == Job::TYPE_STORK ) {
	  job->_submitTries++;
      submit_success = stork_submit( dm, cmd_file.Value(), condorID,
				   job->GetJobName(), job->GetDirectory() );
    } else {
	    debug_printf( DEBUG_QUIET, "Illegal job type: %d\n", job->JobType() );
		ASSERT(false);
	}


    if( !submit_success ) {

	  	// Set the times to wait twice as long as last time.
	  int thisSubmitDelay = _nextSubmitDelay;
	  _nextSubmitTime = time(NULL) + thisSubmitDelay;
	  _nextSubmitDelay *= 2;

	  if ( job->_submitTries >= dm.max_submit_attempts ) {
		// We're out of submit attempts, treat this as a submit failure.

			// To match the existing behavior, we're resetting the times
			// here.
		_nextSubmitTime = 0;
		_nextSubmitDelay = 1;

		debug_printf( DEBUG_QUIET, "Job submit failed after %d tr%s.\n",
				job->_submitTries, job->_submitTries == 1 ? "y" : "ies" );

        snprintf( job->error_text, JOB_ERROR_TEXT_MAXLEN,
				"Job submit failed" );

        // NOTE: this failure short-circuits the "retry" feature
		// because it's already exhausted a number of retries
		// (maybe we should make sure dm.max_submit_attempts >
		// job->retries before assuming this is a good idea...)
        job->retries = job->GetRetryMax();

        if( job->_scriptPost ) {
	      // a POST script is specified for the job, so run it
	      job->_Status = Job::STATUS_POSTRUN;
		  _postRunNodeCount++;
	      job->_scriptPost->_retValJob = DAG_ERROR_CONDOR_SUBMIT_FAILED;
	      _postScriptQ->Run( job->_scriptPost );
        } else {
	      job->_Status = Job::STATUS_ERROR;
	      _numNodesFailed++;
        }

	  } else {
	      // We have more submit attempts left, put this job back into the
		  // ready queue.
		debug_printf( DEBUG_NORMAL, "Job submit try %d/%d failed, "
				"will try again in >= %d second%s.\n", job->_submitTries,
				dm.max_submit_attempts, thisSubmitDelay,
				thisSubmitDelay == 1 ? "" : "s" );

		if ( m_retrySubmitFirst ) {
		  _readyQ->Prepend(job);
		} else {
		  _readyQ->Append(job);
		}
	  }

	  return numSubmitsThisCycle;
    }
    
	  // Set the times to *not* wait before trying another submit, since
	  // the most recent submit worked.
	_nextSubmitTime = 0;
	_nextSubmitDelay = 1;

    // append job to the submit queue so we can match it with its
    // submit event once the latter appears in the Condor job log
    if( _submitQ->enqueue( job ) == -1 ) {
		debug_printf( DEBUG_QUIET, "ERROR: _submitQ->enqueue() failed!\n" );
	}

    job->_Status = Job::STATUS_SUBMITTED;

		// Note: I assume we're incrementing this here instead of when
		// we see the submit events so that we don't accidentally exceed
		// maxjobs (now really maxnodes) if it takes a while to see
		// the submit events.  wenger 2006-02-10.
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

			CheckForDagAbort(job, WEXITSTATUS( status ), "PRE script");
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
		debug_printf( DEBUG_QUIET, "PRE Script of Node %s completed "
					  "successfully.\n", job->GetJobName() );
		job->retval = 0; // for safetey on retries
		job->_Status = Job::STATUS_READY;
		_preRunNodeCount--;
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
		bool useXml = readLog.getIsXMLLog();

		UserLog ulog;
		ulog.setUseXML( useXml );
		ulog.initialize( job->_logFile, job->_CondorID._cluster,
					 	0, 0 );

		if( !ulog.writeEvent( &e ) ) {
			debug_printf( DEBUG_QUIET,
					  	"Unable to log ULOG_POST_SCRIPT_TERMINATED event\n" );
		}
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
void Dag::RemoveRunningJobs ( const Dagman &dm) const {

	debug_printf( DEBUG_NORMAL, "Removing any/all submitted Condor/"
				"Stork jobs...\n");

    char cmd[ARG_MAX];

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
		snprintf( cmd, ARG_MAX, "%s -const \'%s == %d\'",
			  dm.condorRmExe, ATTR_DAGMAN_JOB_ID,
			  dm.DAGManJobId._cluster );
		debug_printf( DEBUG_VERBOSE, "Executing: %s\n", cmd );
		if ( util_popen( cmd ) ) {
			debug_printf( DEBUG_VERBOSE, "Error removing DAGMan jobs\n");
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
			snprintf( cmd, ARG_MAX, "%s %d", dm.storkRmExe,
					  job->_CondorID._cluster );
			debug_printf( DEBUG_VERBOSE, "Executing: %s\n", cmd );
			if ( util_popen( cmd ) ) {
				debug_printf( DEBUG_VERBOSE, "Error removing Stork job\n");
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
			ASSERT( job->_scriptPre );
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
			ASSERT( job->_scriptPost );
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
void Dag::Rescue (const char * rescue_file, const char * datafile,
			bool useDagDir) const {
    FILE *fp = fopen(rescue_file, "w");
    if (fp == NULL) {
        debug_printf( DEBUG_QUIET, "Could not open %s for writing.\n",
					  rescue_file);
        return;
    }

	bool reset_retries_upon_rescue =
		param_boolean( "DAGMAN_RESET_RETRIES_UPON_RESCUE", true );

    fprintf (fp, "# Rescue DAG file, created after running\n");
    fprintf (fp, "#   the %s DAG file\n", datafile);
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
    // Print JOBS and SCRIPTS
    //
    it.ToBeforeFirst();
    while (it.Next(job)) {
        if( job->JobType() == Job::TYPE_CONDOR ) {
            fprintf (fp, "JOB %s %s ", job->GetJobName(), job->GetCmdFile());
			if ( useDagDir ) {
				fprintf(fp, "DIR %s ", job->GetDirectory());
			}
			fprintf (fp, "%s\n",
					job->_Status == Job::STATUS_DONE ? "DONE" : "");
        }
        else if( job->JobType() == Job::TYPE_STORK ) {
            fprintf (fp, "DATA %s %s ", job->GetJobName(), job->GetCmdFile());
			if ( useDagDir ) {
				fprintf(fp, "DIR %s ", job->GetDirectory());
			}
			fprintf (fp, "%s\n",
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
			StartNode( child, false );
		}
    }
		// this is a little ugly, but since this function can be
		// called multiple times for the same job, we need to be
		// careful not to double-count...
	if( job->countedAsDone == false ) {
		_numNodesDone++;
		job->countedAsDone = true;
		ASSERT( _numNodesDone <= _jobs.Number() );
	}
}

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

void
Dag::RestartNode( Job *node, bool recovery )
{
    ASSERT( node != NULL );
    ASSERT( node->_Status == Job::STATUS_ERROR );
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
	debug_printf( DEBUG_VERBOSE, "Retrying node %s (retry #%d of %d)...\n",
				  node->GetJobName(), node->GetRetries(),
				  node->GetRetryMax() );
	if( !recovery ) {
		StartNode( node, true );
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

void
Dag::CheckForDagAbort(Job *job, int exitVal, const char *type)
{
	if ( job->have_abort_dag_val &&
				exitVal == job->abort_dag_val ) {
		debug_printf( DEBUG_QUIET, "Aborting DAG because we got "
				"the ABORT exit value from a %s\n", type);
		main_shutdown_rescue( exitVal );
	}
}


const MyString
Dag::ParentListString( Job *node, const char delim ) const
{
	SimpleListIterator <JobID_t> parent_list;
	JobID_t parentID;
	Job* parent;
	const char* parent_name = NULL;
	MyString parents_str;

	parent_list.Initialize( node->GetQueueRef( Job::Q_PARENTS ) );
	parent_list.ToBeforeFirst();
	while( parent_list.Next( parentID ) ) {
		parent = GetJob( parentID );
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
		debug_printf( DEBUG_VERBOSE, "Error checking Condor job events: %s\n",
				jobError.Value() );
		ASSERT( false );
	} else if ( result == CheckEvents::EVENT_BAD_EVENT ) {
		debug_printf( DEBUG_VERBOSE, "Warning checking Condor job events: %s\n",
				jobError.Value() );
	} else {
		debug_printf( DEBUG_DEBUG_1, "All Condor job events okay\n");
	}

	result = _checkStorkEvents.CheckAllJobs(jobError);
	if ( result == CheckEvents::EVENT_ERROR ) {
		debug_printf( DEBUG_VERBOSE, "Error checking Stork job events: %s\n",
				jobError.Value() );
		ASSERT( false );
	} else if ( result == CheckEvents::EVENT_BAD_EVENT ) {
		debug_printf( DEBUG_VERBOSE, "Warning checking Stork job events: %s\n",
				jobError.Value() );
	} else {
		debug_printf( DEBUG_DEBUG_1, "All Stork job events okay\n");
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
        debug_printf(DEBUG_NORMAL, "Can't open dot include file %s\n",
					_dot_include_file_name);
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


Job*
Dag::LogEventNodeLookup( int logsource, const ULogEvent* event,
			bool recovery )
{
	ASSERT( event );
	Job *node = NULL;
	CondorID condorID( event->cluster, event->proc, event->subproc );

		// if this is a job whose submit event we've already seen, we
		// can simply use the job ID to look up the corresponding DAG
		// node, and we're done...

	if( event->eventNumber != ULOG_SUBMIT ) {
	  node = GetJob( logsource, condorID );
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
				node = GetJob( nodeName );
				if( node ) {
					SanityCheckSubmitEvent( condorID, node, recovery );
					node->_CondorID = condorID;
				}
			}
		}
		return node;
	}

	if( event->eventNumber == ULOG_POST_SCRIPT_TERMINATED &&
		event->cluster == -1 ) {
		const PostScriptTerminatedEvent* pst_event =
			(const PostScriptTerminatedEvent*)event;
		node = GetJob( pst_event->dagNodeName );
		return node;
	}

	return node;
}


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
	CheckEvents::check_event_result_t checkResult;

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
	debug_printf( DEBUG_NORMAL, "WARNING: bad event here may indicate a "
				  "serious bug in Condor -- beware!\n" );

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


// compares a submit event's job ID with the one that appeared earlier
// in the submit command's stdout (which we stashed in the Job object)

bool
Dag::SanityCheckSubmitEvent( const CondorID condorID, const Job* node,
							 const bool recovery )
{
	if( recovery ) {
			// we no longer have the submit command stdout to check against
		return true;
	}
	if( condorID._cluster == node->_CondorID._cluster ) {
		return true;
	}
	debug_printf( DEBUG_QUIET,
				  "ERROR: node %s: job ID in userlog submit event (%d.%d) "
				  "doesn't match ID reported earlier by submit command "
				  "(%d.%d)!  Trusting the userlog for now, but this is "
				  "scary!\n",
				  node->GetJobName(), condorID._cluster, condorID._proc,
				  node->_CondorID._cluster, node->_CondorID._proc );
	return false;
}


// NOTE: dag addnode/removenode/adddep/removedep methods don't
// necessarily insure internal consistency...that's currently up to
// the higher level calling them to get right...
