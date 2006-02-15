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

#include "condor_common.h"
#include "local_user_log.h"
#include "job_info_communicator.h"
#include "condor_attributes.h"
#include "condor_uid.h"
#include "basename.h"
#include "exit.h"


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
LocalUserLog::init( const char* filename, bool is_xml, 
					int cluster, int proc, int subproc )
{
	if( ! jic->userPrivInitialized() ) { 
		EXCEPT( "LocalUserLog::init() "
				"called before user priv is initialized!" );
	}
	priv_state priv;
	priv = set_user_priv();

	if( ! u_log.initialize(filename, cluster, proc, subproc) ) {
		dprintf( D_ALWAYS, 
				 "Failed to initialize Starter's UserLog, aborting\n" );
		set_priv( priv );
		return false;
	}

	set_priv( priv );

	if( is_xml ) {
		u_log.setUseXML( true );
	} else {
		u_log.setUseXML( false );
	}
	
	dprintf( D_FULLDEBUG, "Starter's UserLog: %s\n", filename );
	is_initialized = true;
	should_log = true;
	return true;
}


bool
LocalUserLog::initFromJobAd( ClassAd* ad, const char* path_attr,
							 const char* xml_attr )
{
    char tmp[_POSIX_PATH_MAX], logfilename[_POSIX_PATH_MAX];
	int use_xml = FALSE;
	const char* iwd = jic->jobIWD();
	int cluster = jic->jobCluster();
	int proc = jic->jobProc();
	int subproc = jic->jobSubproc();

    if( ! ad->LookupString(path_attr, tmp) ) {
        dprintf( D_FULLDEBUG, "No %s found in job ClassAd\n", path_attr );
		return initNoLogging();
	}

	if( fullpath(tmp) ) {
			// we have a full pathname in the job ad.  however, if the
			// job is using a different iwd (namely, filetransfer is
			// being used), we want to just stick it in the local iwd
			// for the job, instead.
		if( jic->iwdIsChanged() ) {
			const char* base = condor_basename(tmp);
			sprintf(logfilename, "%s/%s", iwd, base);
		} else {			
			strcpy(logfilename, tmp);
		}
	} else {
			// no full path, so, use the job's iwd...
		sprintf(logfilename, "%s/%s", iwd, tmp);
	}

	ad->LookupBool( xml_attr, use_xml );

	return init( logfilename, (bool)use_xml, cluster, proc, subproc );
}


bool
LocalUserLog::initNoLogging( void )
{
	dprintf( D_FULLDEBUG, "Starter will not write a local UserLog\n" );
	is_initialized = true;
	should_log = false;
	return true;
}



bool
LocalUserLog::logExecute( ClassAd* ad )
{
	if( ! is_initialized ) {
		EXCEPT( "LocalUserLog::logExecute() called before init()" );
	}
	if( ! should_log ) {
		return true;
	}

	ExecuteEvent event;
	strcpy( event.executeHost, daemonCore->InfoCommandSinfulString() ); 

	if( !u_log.writeEvent(&event) ) {
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

	if( !u_log.writeEvent(&event) ) {
        dprintf( D_ALWAYS, "Unable to log ULOG_JOB_SUSPENDED event\n" );
		return false;
    }
	return true;
}


bool
LocalUserLog::logContinue( ClassAd* ad )
{
	if( ! is_initialized ) {
		EXCEPT( "LocalUserLog::logContinue() called before init()" );
	}
	if( ! should_log ) {
		return true;
	}

	JobUnsuspendedEvent event;

	if( !u_log.writeEvent(&event) ) {
        dprintf( D_ALWAYS, "Unable to log ULOG_JOB_UNSUSPENDED event\n" );
		return false;
    }
	return true;
}


bool
LocalUserLog::logStarterError( const char* err_msg, bool critical )
{
	if( ! is_initialized ) {
		EXCEPT( "LocalUserLog::logStarterError() called before init()" );
	}
	if( ! should_log ) {
		return true;
	}

	RemoteErrorEvent event;
	event.setErrorText( err_msg );
	event.setDaemonName( "starter" );
	event.setExecuteHost( daemonCore->InfoCommandSinfulString() );
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
    case JOB_NOT_CKPTED:
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

	int int_value = 0;
	bool exited_by_signal = false;
	if( ad->LookupBool(ATTR_ON_EXIT_BY_SIGNAL, int_value) ) {
        if( int_value ) {
            exited_by_signal = true;
        } 
    } else {
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

	if( !u_log.writeEvent(&event) ) {
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
    
	if( !u_log.writeEvent(&event) ) {
        dprintf( D_ALWAYS, "Unable to log ULOG_JOB_EVICTED event\n" );
		return false;
    }
	return true;
}




struct rusage
LocalUserLog::getRusageFromAd( ClassAd* ad )
{
    struct rusage local_rusage;
    memset( &local_rusage, 0, sizeof(struct rusage) );

	float sys_time = 0;
	float user_time = 0;

	if( ad->LookupFloat(ATTR_JOB_REMOTE_SYS_CPU, sys_time) ) {
        local_rusage.ru_stime.tv_sec = (int) sys_time; 
    }
    if( ad->LookupFloat(ATTR_JOB_REMOTE_USER_CPU, user_time) ) {
        local_rusage.ru_utime.tv_sec = (int) user_time; 
    }

	return local_rusage;
}

