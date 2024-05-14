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


#include "condor_common.h"
#include "local_user_log.h"
#include "job_info_communicator.h"
#include "condor_attributes.h"
#include "condor_uid.h"
#include "basename.h"
#include "exit.h"
#include "condor_config.h"


LocalUserLog::LocalUserLog( JobInfoCommunicator* my_jic )
{
	jic = my_jic;
	is_initialized = false;
	should_log = false;
}


LocalUserLog::~LocalUserLog()
{
		// don't delete the jic, since it's not really ours
}

bool
LocalUserLog::initFromJobAd( ClassAd* ad, bool starter_ulog )
{
	if( ! jic->userPrivInitialized() ) {
		EXCEPT( "LocalUserLog::initFromJobAd() "
				"called before user priv is initialized!" );
	}

	if ( ! starter_ulog ) {
		// This is the primary user log for the local universe.
		// Use WriteUserLog's full initialization logic.
		if ( !u_log.initialize(*ad) ) {
			dprintf( D_ALWAYS,
				"Failed to initialize Starter's UserLog, aborting\n" );
			return false;
		}
		is_initialized = true;
		should_log = u_log.willWrite();
		return true;
	}

	std::string tmp;
	std::string logfilename;
	bool use_xml = false;
	const char* iwd = jic->jobIWD();
	int cluster = jic->jobCluster();
	int proc = jic->jobProc();
	int subproc = jic->jobSubproc();

	// Look for starter-specific user log file
	if( ! ad->LookupString(ATTR_STARTER_ULOG_FILE, tmp) ) {
			// The fact that this attribute is not found in the ClassAd
			// indicates we do not want logging to a log file specified
			// in the submit file.
			// These semantics are defined in JICShadow::init.
			// We still need to check below for a DAGMan-specified
			// workflow log file for local universe!!
		dprintf( D_FULLDEBUG, "No %s found in job ClassAd\n", ATTR_STARTER_ULOG_FILE );
		is_initialized = true;
		should_log = false;
		return true;
	}

	dprintf( D_FULLDEBUG, "LocalUserLog::initFromJobAd: tmp = %s\n",
			tmp.c_str());
	if( fullpath (tmp.c_str() ) ) {
		// we have a full pathname in the job ad.  however, if the
		// job is using a different iwd (namely, filetransfer is
		// being used), we want to just stick it in the local iwd
		// for the job, instead.
		if( jic->iwdIsChanged() ) {
			const char* base = condor_basename(tmp.c_str());
			formatstr(logfilename, "%s/%s", iwd, base);
		} else {
			logfilename = tmp;
		}
	} else {
			// no full path, so, use the job's iwd...
		formatstr(logfilename, "%s/%s", iwd, tmp.c_str());
	}

	dprintf( D_FULLDEBUG, "LocalUserLog::initFromJobAd: UserLog "
		"file %s\n", logfilename.c_str());

	ad->LookupBool( ATTR_STARTER_ULOG_USE_XML, use_xml );

	TemporaryPrivSentry temp_priv(PRIV_USER);

	// TODO Do we need to ensure that u_log will switch to user priv
	//   when writing and closing the log file?
	bool ret = u_log.initialize(logfilename.c_str(), cluster, proc, subproc);
	if( ! ret ) {
		dprintf( D_ALWAYS,
				 "Failed to initialize Starter's UserLog, aborting\n" );
		return false;
	}

	u_log.setUseCLASSAD( use_xml ? ULogEvent::formatOpt::XML : 0 );
	is_initialized = true;
	should_log = true;
	return true;
}

bool
LocalUserLog::logExecute( ClassAd*  ad  )
{
	if( ! is_initialized ) {
		EXCEPT( "LocalUserLog::logExecute() called before init()" );
	}
	if( ! should_log ) {
		return true;
	}

	ExecuteEvent event;
	event.setExecuteHost( daemonCore->InfoCommandSinfulString() ); 

	if( !u_log.writeEvent(&event,ad) ) {
        dprintf( D_ALWAYS, "Unable to log ULOG_EXECUTE event: "
                 "can't write to UserLog!\n" );
		return false;
    }
	return true;
}


bool
LocalUserLog::logSuspend( ClassAd* ad )
{
	if( ! is_initialized ) {
		EXCEPT( "LocalUserLog::logSuspend() called before init()" );
	}
	if( ! should_log ) {
		return true;
	}

	JobSuspendedEvent event;

	int num = 0;
	if( ! ad->LookupInteger(ATTR_NUM_PIDS, num) ) {
		dprintf( D_ALWAYS, "LocalUserLog::logSuspend() "
				 "ERROR: %s not defined in update ad, assuming 1\n", 
				 ATTR_NUM_PIDS );
		num = 1;
	}
    event.num_pids = num;

	if( !u_log.writeEvent(&event,ad) ) {
        dprintf( D_ALWAYS, "Unable to log ULOG_JOB_SUSPENDED event\n" );
		return false;
    }
	return true;
}


bool
LocalUserLog::logContinue( ClassAd*  ad  )
{
	if( ! is_initialized ) {
		EXCEPT( "LocalUserLog::logContinue() called before init()" );
	}
	if( ! should_log ) {
		return true;
	}

	JobUnsuspendedEvent event;

	if( !u_log.writeEvent(&event,ad) ) {
        dprintf( D_ALWAYS, "Unable to log ULOG_JOB_UNSUSPENDED event\n" );
		return false;
    }
	return true;
}


bool
LocalUserLog::logCheckpoint( ClassAd*  /* ad */ )
{
	if( ! is_initialized ) {
		EXCEPT( "LocalUserLog::logCheckpoint() called before init()" );
	}
	if( ! should_log ) {
		return true;
	}

	CheckpointedEvent event;

	if( !u_log.writeEvent(&event) ) {
        dprintf( D_ALWAYS, "Unable to log ULOG_CHECKPOINTED event\n" );
		return false;
    }
	return true;
}


bool
LocalUserLog::logStarterError( const char* err_msg, bool critical )
{
	if( ! is_initialized ) {
			// This can happen if we hit an error talking to the shadow
			// before we get the job ad.  Just ignore it.
		return false;
	}
	if( ! should_log ) {
		return true;
	}

	RemoteErrorEvent event;
	event.error_str = err_msg;
	event.daemon_name = "starter";
	event.execute_host = daemonCore->InfoCommandSinfulString();
	event.setCriticalError( critical );

	if( !u_log.writeEvent(&event) ) {
        dprintf( D_ALWAYS, "Unable to log ULOG_REMOTE_ERROR event\n" );
		return false;
    }
	return true;
}


bool
LocalUserLog::logJobExit( ClassAd* ad, int exit_reason ) 
{
	if( ! should_log ) {
		return true;
	}
	switch( exit_reason ) {
    case JOB_EXITED:
    case JOB_COREDUMPED:
		return logTerminate( ad );
		break;
    case JOB_CKPTED:
		return logEvict( ad, true );
		break;
    case JOB_SHOULD_REQUEUE:
    case JOB_KILLED:
		return logEvict( ad, false );
        break;
    default:
		dprintf( D_ALWAYS, "Job exited with unknown reason (%d)!\n",exit_reason); 
    }
	return false;
}


bool
LocalUserLog::logTerminate( ClassAd* ad )
{
	if( ! is_initialized ) {
		EXCEPT( "LocalUserLog::logTerminate() called before init()" );
	}
	if( ! should_log ) {
		return true;
	}

	JobTerminatedEvent event;
	
		//
		// This will fill out the event with the appropriate
		// information we need about how the job ended
		//
	int int_value = 0;
	bool exited_by_signal = false;
	if( ! ad->LookupBool(ATTR_ON_EXIT_BY_SIGNAL, exited_by_signal) ) {
		EXCEPT( "in LocalUserLog::logTerminate() "
				"ERROR: ClassAd does not define %s!",
				ATTR_ON_EXIT_BY_SIGNAL );
	}

    if( exited_by_signal ) {
        event.normal = false;
		if( ad->LookupInteger(ATTR_ON_EXIT_SIGNAL, int_value) ) {
			event.signalNumber = int_value;
		} else {
			EXCEPT( "in LocalUserLog::logTerminate() "
					"ERROR: ClassAd does not define %s!",
					ATTR_ON_EXIT_SIGNAL );
		}
    } else {
        event.normal = true;
		if( ad->LookupInteger(ATTR_ON_EXIT_CODE, int_value) ) {
			event.returnValue = int_value;
		} else {
			EXCEPT( "in LocalUserLog::logTerminate() "
					"ERROR: ClassAd does not define %s!",
					ATTR_ON_EXIT_CODE );
		}
    }

    struct rusage run_local_rusage;
	run_local_rusage = getRusageFromAd( ad );
	event.run_local_rusage = run_local_rusage;
        // remote rusage should be blank

	event.recvd_bytes = jic->bytesReceived();
    event.sent_bytes = jic->bytesSent();

		// TODO corefile name?!?!

	if( !u_log.writeEvent(&event,ad) ) {
        dprintf( D_ALWAYS, "Unable to log ULOG_JOB_CONTINUED event\n" );
		return false;
    }
	return true;
}


bool
LocalUserLog::logEvict( ClassAd* ad, bool checkpointed )
{
	if( ! is_initialized ) {
		EXCEPT( "LocalUserLog::logEvict() called before init()" );
	}
	if( ! should_log ) {
		return true;
	}

    JobEvictedEvent event;

    struct rusage run_local_rusage;
	run_local_rusage = getRusageFromAd( ad );
	event.run_local_rusage = run_local_rusage;
        // remote rusage should be blank

    event.checkpointed = checkpointed;
    
	event.recvd_bytes = jic->bytesReceived();
    event.sent_bytes = jic->bytesSent();
    
	if( !u_log.writeEvent(&event,ad) ) {
        dprintf( D_ALWAYS, "Unable to log ULOG_JOB_EVICTED event\n" );
		return false;
    }
	return true;
}

/**
 * Writes an EVICT event to the user log with the requeue flag
 * set to true. We will stuff information about why the job
 * exited into the event object, such as the exit signal.
 * 
 * @param ad - the job to update the user log for
 * @param checkpointed - whether the job was checkpointed or not
 **/
bool
LocalUserLog::logRequeueEvent( ClassAd* ad, bool checkpointed )
{
	struct rusage run_local_rusage;
	run_local_rusage = this->getRusageFromAd( ad );

	JobEvictedEvent event;
	event.terminate_and_requeued = true;

		//
		// This will fill out the event with the appropriate
		// information we need about how the job ended
		// Copied from logTerminate()
		// It would be nice to have a method that could populate all 
		// this data for us regardless of the log event type, but alas
		// the objects are not uniform (signal_number vs signalNumber)
		//
	int int_value = 0;
	bool exited_by_signal = false;
	if( ! ad->LookupBool(ATTR_ON_EXIT_BY_SIGNAL, exited_by_signal) ) {
		EXCEPT( "in LocalUserLog::logTerminate() "
				"ERROR: ClassAd does not define %s!",
				ATTR_ON_EXIT_BY_SIGNAL );
	}

    if( exited_by_signal ) {
        event.normal = false;
		if( ad->LookupInteger(ATTR_ON_EXIT_SIGNAL, int_value) ) {
			event.signal_number = int_value;
		} else {
			EXCEPT( "in LocalUserLog::logTerminate() "
					"ERROR: ClassAd does not define %s!",
					ATTR_ON_EXIT_SIGNAL );
		}
    } else {
        event.normal = true;
		if( ad->LookupInteger(ATTR_ON_EXIT_CODE, int_value) ) {
			event.return_value = int_value;
		} else {
			EXCEPT( "in LocalUserLog::logTerminate() "
					"ERROR: ClassAd does not define %s!",
					ATTR_ON_EXIT_CODE );
		}
    }
    
    	//
    	// Grab the exit reason out of the ad
    	//
	std::string reasonstr;
	ad->LookupString( ATTR_REQUEUE_REASON, reasonstr );
	event.setReason(reasonstr);
    
	event.run_local_rusage = run_local_rusage;
    event.checkpointed = checkpointed;
	event.recvd_bytes = jic->bytesReceived();
    event.sent_bytes = jic->bytesSent();
    
	if ( ! this->u_log.writeEvent(&event,ad) ) {
        dprintf( D_ALWAYS, "Unable to log ULOG_JOB_EVICTED event\n" );
		return ( false );
    }
	return ( true );	
}

struct rusage
LocalUserLog::getRusageFromAd( ClassAd* ad )
{
    struct rusage local_rusage;
    memset( &local_rusage, 0, sizeof(struct rusage) );

	double sys_time = 0;
	double user_time = 0;

	if( ad->LookupFloat(ATTR_JOB_REMOTE_SYS_CPU, sys_time) ) {
        local_rusage.ru_stime.tv_sec = (time_t) sys_time;
    }
    if( ad->LookupFloat(ATTR_JOB_REMOTE_USER_CPU, user_time) ) {
        local_rusage.ru_utime.tv_sec = (time_t) user_time;
    }

	return local_rusage;
}

