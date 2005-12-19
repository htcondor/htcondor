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

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_qmgr.h"
#include "util_lib_proto.h"
#include "setenv.h"
#include "env.h"
#include "get_port_range.h"
#include "basename.h"

#include "gridftpmanager.h"

const char *ATTR_GRIDFTP_SERVER_JOB = "GridftpServerJob";

#define QMGMT_TIMEOUT 15

#define STATUS_UNSUBMITTED	1
#define STATUS_IDLE			2
#define STATUS_ACTIVE		3
#define STATUS_DONE			4

#define HASH_TABLE_SIZE			50

HashTable <HashKey, GridftpServer *>
    GridftpServer::m_serversByProxy( HASH_TABLE_SIZE,
									 hashFunction );

const int GridftpServer::STARTING = 0;
const int GridftpServer::ACTIVE = 1;
const int GridftpServer::FAILED = 2;

bool GridftpServer::m_initialized = false;
int GridftpServer::m_checkServersTid = TIMER_UNSET;
bool GridftpServer::m_initialScanDone = false;

ClassAd *CreateJobAd( const char *owner, int universe, const char *cmd );

GridftpServer::GridftpServer( Proxy *proxy, const char *req_url_base )
{
	m_urlBase = NULL;
	m_refCount = 0;
	m_jobId.cluster = 0;
	m_jobId.proc = 0;
	m_proxy = proxy->subject->master_proxy;
	m_userLog = NULL;
	m_outputFile = NULL;
	m_errorFile = NULL;
	m_proxyFile = NULL;
	m_proxyExpiration = 0;
	AcquireProxy( m_proxy, m_checkServersTid );
	if ( req_url_base ) {
		m_requestedUrlBase = strdup( req_url_base );
	} else {
		m_requestedUrlBase = NULL;
	}
}

GridftpServer::~GridftpServer()
{
	m_serversByProxy.remove( HashKey( m_proxy->subject->subject_name ) );
	ReleaseProxy( m_proxy );
	if ( m_urlBase ) {
		free( m_urlBase );
	}
	if ( m_requestedUrlBase ) {
		free( m_requestedUrlBase );
	}
	if ( m_userLog ) {
		free( m_userLog );
	}
	if ( m_outputFile ) {
		free( m_outputFile );
	}
	if ( m_errorFile ) {
		free( m_errorFile );
	}
	if ( m_proxyFile ) {
		free( m_proxyFile );
	}
}

GridftpServer *GridftpServer::FindOrCreateServer( Proxy *proxy,
												  const char *req_url_base )
{
	int rc;
	GridftpServer *server = NULL;

	if ( !m_initialized ) {
		m_checkServersTid = daemonCore->Register_Timer( 1,
										(TimerHandler)&CheckServers,
										"GridftpServer::CheckServers", NULL );
		m_initialized = true;
	}

	rc = m_serversByProxy.lookup( HashKey( proxy->subject->subject_name ),
								  server );
	if ( rc != 0 ) {
		server = new GridftpServer( proxy, req_url_base );
		ASSERT(server);
		m_serversByProxy.insert( HashKey( proxy->subject->subject_name ),
								 server );
	} else {
		ASSERT(server);
	}

	return server;
}

void GridftpServer::RegisterClient( GT4Job *job )
{
	m_refCount++;
	m_registeredJobs.Append( job );
}

void GridftpServer::UnregisterClient( GT4Job *job )
{
	m_refCount--;
	m_registeredJobs.Delete( job );
}

bool GridftpServer::IsEmpty()
{
	return m_refCount == 0;
}

const char *GridftpServer::GetUrlBase()
{
	return m_urlBase;
}

void GridftpServer::CheckServersSoon( int delta )
{
	if ( m_checkServersTid != TIMER_UNSET ) {
		daemonCore->Reset_Timer( m_checkServersTid, delta );
	}
}

int GridftpServer::CheckServers()
{
	GridftpServer *server;

		// TODO interval should be a const, or even configurable
	daemonCore->Reset_Timer( m_checkServersTid, 60 );

	if ( !m_initialScanDone ) {
		if ( ScanSchedd() ) {
			m_initialScanDone = true;
		} else {
			return TRUE;
		}
	}

	m_serversByProxy.startIterations();

	while ( m_serversByProxy.iterate( server ) != 0 ) {

		bool delete_server = false;
		server->CheckServer( delete_server );
		if ( delete_server ) {
			delete server;
		}
	}

	return TRUE;
}

void GridftpServer::CheckServer( bool &delete_me )
{
	delete_me = false;

		// TODO maybe waite 5 minutes after having no jobs before shutting
		//   server down
	if ( IsEmpty() ) {
		RemoveJob();
		delete_me = true;
		return;
	}

	int job_status;
	job_status = CheckJobStatus();

	if ( job_status == STATUS_DONE ) {
		if ( m_requestedUrlBase && CheckPortError() ) {
			free( m_requestedUrlBase );
			m_requestedUrlBase = NULL;
		} else if ( m_urlBase ) {
			m_requestedUrlBase = m_urlBase;
			m_urlBase = NULL;
		}
		
		RemoveJob();
		m_jobId.cluster = 0;
		if ( m_outputFile ) {
			free( m_outputFile );
			m_outputFile = NULL;
		}
		if ( m_errorFile ) {
			free( m_errorFile );
			m_errorFile = NULL;
		}
		if ( m_userLog ) {
			free( m_userLog );
			m_userLog = NULL;
		}
		if ( m_proxyFile ) {
			free( m_proxyFile );
			m_proxyFile = NULL;
		}
	}

	if ( job_status == STATUS_ACTIVE && m_urlBase == NULL ) {
		if ( m_requestedUrlBase == NULL ) {
			if ( ReadUrlBase() ) {
				GT4Job *job;
				m_registeredJobs.Rewind();
				while ( m_registeredJobs.Next( job ) ) {
					job->NewGridftpUrlBase( m_urlBase );
				}
			}
		} else {
			m_urlBase = m_requestedUrlBase;
			m_requestedUrlBase = NULL;

			GT4Job *job;
			m_registeredJobs.Rewind();
			while ( m_registeredJobs.Next( job ) ) {
				job->NewGridftpUrlBase( m_urlBase );
			}
		}
	}

	if ( job_status == ACTIVE ) {
		CheckProxy();
	}

	if ( job_status == STATUS_UNSUBMITTED ) {
		SubmitServerJob();
	}

}

bool GridftpServer::ScanSchedd()
{
	Qmgr_connection *schedd;
	bool error = false;
	MyString expr;
	ClassAd *next_ad;

	schedd = ConnectQ( ScheddAddr, QMGMT_TIMEOUT, true );
	if ( !schedd ) {
		dprintf( D_ALWAYS, "GridftpServer::ScanSchedd: "
				 "Failed to connect to schedd\n" );
		return false;
	}

	expr.sprintf( "%s == \"%s\" && %s =?= True", ATTR_OWNER, Owner,
				  ATTR_GRIDFTP_SERVER_JOB );

		// TODO check that this didn't return NULL due to a connection
		//   failure
	next_ad = GetNextJobByConstraint( expr.Value(), 1 );
	while ( next_ad != NULL ) {
		MyString buff;
		GridftpServer *server;

			// If we have a Server object for this proxy subect with
			// no job id, attach it to this job. If we have multiple
			// jobs for the same proxy subject, we'll end up picking
			// the first one, ignoring the requested url base (which
			// may match one of the later jobs). Tough luck.
		next_ad->LookupString( ATTR_X509_USER_PROXY_SUBJECT, buff );
		if ( m_serversByProxy.lookup( HashKey( buff.Value() ), server ) ) {

			MyString error_str;
			Proxy *proxy = AcquireProxy( next_ad, error_str );
			server = new GridftpServer( proxy, NULL );
			ASSERT(server);
			m_serversByProxy.insert( HashKey( proxy->subject->subject_name ),
									 server );
			ReleaseProxy( proxy );
		}

		if ( server->m_jobId.cluster == 0 ) {
			next_ad->LookupInteger( ATTR_CLUSTER_ID,
									server->m_jobId.cluster );
			next_ad->LookupInteger( ATTR_PROC_ID,
									server->m_jobId.proc );
		}

		delete next_ad;
			// TODO check that this didn't return NULL due to a connection
			//   failure
		next_ad = GetNextJobByConstraint( expr.Value(), 0 );
	}

 contact_schedd_disconnect:
	DisconnectQ( schedd );

	return !error;
}

bool GridftpServer::SubmitServerJob()
{
	char *cmd;
	ClassAd *job_ad = NULL;
	char mapfile[1024] = "";
	FILE *mapfile_fp = NULL;
	Qmgr_connection *schedd = NULL;
	int cluster_id = -1;
	int proc_id = -1;
	ExprTree *tree = NULL;
	CondorError errstack;
	Env env_obj;
	char *job_env;
	int low_port, high_port;
	const char *value;

	DCSchedd dc_schedd( ScheddAddr );
	if ( dc_schedd.error() || !dc_schedd.locate() ) {
		dprintf( D_ALWAYS, "Can't find our schedd!?\n" );
		return false;
	}

	cmd = param( "GRIDFTP_SERVER" );
		// TODO react better to cmd being undefined
	ASSERT( cmd );
	job_ad = CreateJobAd( Owner, CONDOR_UNIVERSE_SCHEDULER, cmd );
	free( cmd );

	job_ad->Assign( ATTR_JOB_STATUS, HELD );
	job_ad->Assign( ATTR_HOLD_REASON, "Spooling input data files" );
	job_ad->Assign( ATTR_X509_USER_PROXY, m_proxy->proxy_filename );
	job_ad->Assign( ATTR_X509_USER_PROXY_SUBJECT,
					m_proxy->subject->subject_name );
	job_ad->Assign( ATTR_JOB_OUTPUT, "/tmp/out" );
	job_ad->Assign( ATTR_JOB_ERROR, "/tmp/err" );
	job_ad->Assign( ATTR_ULOG_FILE, "/tmp/log" );
	job_ad->Assign( ATTR_JOB_LEAVE_IN_QUEUE, true );

	snprintf( mapfile, sizeof(mapfile), "%s/grid-mapfile",
			  GridmanagerScratchDir );
	mapfile_fp = fopen( mapfile, "w" );
	if ( mapfile_fp == NULL ) {
		dprintf( D_ALWAYS, "Failed to open %s for write, errno=%d\n", mapfile,
				 errno );
		goto error_exit;
	}

	if ( fprintf( mapfile_fp, "\"%s\" %s\n", m_proxy->subject->subject_name,
				  Owner ) < 0 ) {
		dprintf( D_ALWAYS, "Failed to write to %s, errno=%d\n", mapfile,
				 errno );
		goto error_exit;
	}

	if ( fclose( mapfile_fp ) < 0 ) {
		dprintf( D_ALWAYS, "Failed to close %s, errno=%d\n", mapfile, errno );
		mapfile_fp = NULL;
		goto error_exit;
	}
	mapfile_fp = NULL;

	job_ad->Assign( ATTR_TRANSFER_INPUT_FILES, mapfile );

	env_obj.Put( "GRIDMAP", condor_basename( mapfile ) );

	if ( get_port_range( &low_port, &high_port ) == TRUE ) {
		MyString buff;
		buff.sprintf( "%d.%d", low_port, high_port );
		env_obj.Put( "GLOBUS_TCP_PORT_RANGE", buff.Value() );
	}

	value = GetEnv( "X509_CERT_DIR" );
	if ( value ) {
		env_obj.Put( "X509_CERT_DIR", value );
	}

	job_env = env_obj.getDelimitedString();
	job_ad->Assign( ATTR_JOB_ENVIRONMENT, job_env );
	delete [] job_env;
		// TODO what about LD_LIBRARY_PATH?

		// TODO Set job policy expressions to remove jobs that have been
		//   unattended too long
	if ( m_requestedUrlBase ) {
		MyString buff;
		const char *ptr = strrchr( m_requestedUrlBase, ':' );
		ASSERT( ptr );
		buff.sprintf( "-p %d", atoi( ptr+1 ) );
		job_ad->Assign( ATTR_JOB_ARGUMENTS, buff.Value() );
	}

	schedd = ConnectQ( ScheddAddr, QMGMT_TIMEOUT, true );
	if ( !schedd ) {
		dprintf( D_ALWAYS, "GridftpServer::SubmitServerJob: "
				 "Failed to connect to schedd\n" );
		goto error_exit;
	}

	if ( (cluster_id = NewCluster()) >= 0 ) {
		proc_id = NewProc( cluster_id );
	}

	if ( cluster_id == -2 || proc_id == -2 ) {
		dprintf( D_ALWAYS, "Number of submitted jobs would exceed "
				 "MAX_JOBS_SUBMITTED\n" );
	}
	if ( cluster_id < 0 ) {
		dprintf( D_ALWAYS, "Unable to create a new job cluster\n" );
		goto error_exit;
	} else if ( proc_id < 0 ) {
		dprintf( D_ALWAYS, "Unable to create a new job proc\n" );
		goto error_exit;
	}

	job_ad->Assign( ATTR_CLUSTER_ID, cluster_id );
	job_ad->Assign( ATTR_PROC_ID, proc_id );

	// Set all the classad attribute on the remote classad
	job_ad->ResetExpr();
	while( (tree = job_ad->NextExpr()) ) {
		ExprTree *lhs;
		ExprTree *rhs;
		char *lhstr, *rhstr;

		lhs = NULL, rhs = NULL;
		rhs = NULL, rhstr = NULL;

		if( (lhs = tree->LArg()) ) { lhs->PrintToNewStr (&lhstr); }
		if( (rhs = tree->RArg()) ) { rhs->PrintToNewStr (&rhstr); }
		if( !lhs || !rhs || !lhstr || !rhstr) {
			dprintf( D_ALWAYS, "Failed to unparse job attribute\n" );
			goto error_exit;
		} else if ( SetAttribute( cluster_id, proc_id, lhstr, rhstr ) == -1 ) {
			dprintf( D_ALWAYS, "Failed to SetAttribute %s=%s for job %d.%d\n",
					 lhstr, rhstr, cluster_id, proc_id );
			free( lhstr );
			free( rhstr );
			goto error_exit;
		}

		free( lhstr );
		free( rhstr );
	}

	if ( CloseConnection() < 0 ) {
		dprintf( D_ALWAYS, "Failed to commit job submission\n" );
		goto error_exit;
	}

	BeginTransaction();
	if ( errno == ETIMEDOUT ) {
		dprintf( D_ALWAYS, "Failed to query submitted job\n" );
		goto error_exit;
	}

	m_outputFile = NULL;
	m_errorFile = NULL;
	m_userLog = NULL;
	m_proxyFile = NULL;
	if ( GetAttributeStringNew( cluster_id, proc_id, ATTR_JOB_OUTPUT, 
								&m_outputFile ) < 0 ||
		 GetAttributeStringNew( cluster_id, proc_id, ATTR_JOB_ERROR, 
								&m_errorFile ) < 0 ||
		 GetAttributeStringNew( cluster_id, proc_id, ATTR_ULOG_FILE, 
								&m_userLog ) < 0 ||
		 GetAttributeStringNew( cluster_id, proc_id, ATTR_X509_USER_PROXY, 
								&m_proxyFile ) < 0 ) {
		dprintf( D_ALWAYS, "Failed to read job attributes\n" );
		goto error_exit;
	}

	DisconnectQ( schedd );
	schedd = NULL;

	if ( !dc_schedd.spoolJobFiles( 1, &job_ad, &errstack ) ) {
		dprintf( D_ALWAYS, "Failed to stage in files: %s\n",
				 errstack.getFullText() );
		goto error_exit;
	}

	m_jobId.cluster = cluster_id;
	m_jobId.proc = proc_id;
	m_proxyExpiration = m_proxy->expiration_time;

	if ( unlink( mapfile ) < 0 ) {
		dprintf( D_ALWAYS, "Failed to unlink %s, errno=%d\n", mapfile, errno );
	}

	delete job_ad;

	return true;

 error_exit:
	if ( job_ad ) {
		delete job_ad;
	}
	if ( schedd ) {
		DisconnectQ( schedd, false );
	}
	if ( mapfile_fp ) {
		fclose( mapfile_fp );
	}
	if ( *mapfile ) {
		unlink( mapfile );
	}
	if ( m_outputFile ) {
		free( m_outputFile );
		m_outputFile = NULL;
	}
	if ( m_errorFile ) {
		free( m_errorFile );
		m_errorFile = NULL;
	}
	if ( m_userLog ) {
		free( m_userLog );
		m_userLog = NULL;
	}
	if ( m_proxyFile ) {
		free( m_proxyFile );
		m_proxyFile = NULL;
	}
	return false;
}

/* Utility function to create a generic job ad. The caller can then fill
 * in the relevant details.
 */
ClassAd *CreateJobAd( const char *owner, int universe, const char *cmd )
{
	ClassAd *job_ad = new ClassAd();

	job_ad->SetMyTypeName(JOB_ADTYPE);
	job_ad->SetTargetTypeName(STARTD_ADTYPE);

	job_ad->Assign( ATTR_OWNER, owner );
	job_ad->Assign( ATTR_JOB_UNIVERSE, universe );
	job_ad->Assign( ATTR_JOB_CMD, cmd );

	job_ad->Assign( ATTR_Q_DATE, (int)time(NULL) );
	job_ad->Assign( ATTR_COMPLETION_DATE, 0 );

	job_ad->Assign( ATTR_JOB_REMOTE_WALL_CLOCK, (float)0.0 );
	job_ad->Assign( ATTR_JOB_LOCAL_USER_CPU, (float)0.0 );
	job_ad->Assign( ATTR_JOB_LOCAL_SYS_CPU, (float)0.0 );
	job_ad->Assign( ATTR_JOB_REMOTE_USER_CPU, (float)0.0 );
	job_ad->Assign( ATTR_JOB_REMOTE_SYS_CPU, (float)0.0 );

		// Are these ones really necessary?
	job_ad->Assign( ATTR_JOB_EXIT_STATUS, 0 );
	job_ad->Assign( ATTR_ON_EXIT_BY_SIGNAL, false );

	job_ad->Assign( ATTR_NUM_CKPTS, 0 );
	job_ad->Assign( ATTR_NUM_RESTARTS, 0 );
	job_ad->Assign( ATTR_NUM_SYSTEM_HOLDS, 0 );
	job_ad->Assign( ATTR_JOB_COMMITTED_TIME, 0 );
	job_ad->Assign( ATTR_TOTAL_SUSPENSIONS, 0 );
	job_ad->Assign( ATTR_LAST_SUSPENSION_TIME, 0 );
	job_ad->Assign( ATTR_CUMULATIVE_SUSPENSION_TIME, 0 );

	job_ad->Assign( ATTR_JOB_ROOT_DIR, "/" );

	job_ad->Assign( ATTR_MIN_HOSTS, 1 );
	job_ad->Assign( ATTR_MAX_HOSTS, 1 );
	job_ad->Assign( ATTR_CURRENT_HOSTS, 1 );

	job_ad->Assign( ATTR_WANT_REMOTE_SYSCALLS, false );
	job_ad->Assign( ATTR_WANT_CHECKPOINT, false );

	job_ad->Assign( ATTR_JOB_STATUS, IDLE );
	job_ad->Assign( ATTR_ENTERED_CURRENT_STATUS, (int)time(NULL) );

	job_ad->Assign( ATTR_JOB_PRIO, 0 );

	job_ad->Assign( ATTR_JOB_ENVIRONMENT, "" );

	job_ad->Assign( ATTR_JOB_NOTIFICATION, NOTIFY_NEVER );

	job_ad->Assign( ATTR_KILL_SIG, "SIGTERM" );

	job_ad->Assign( ATTR_IMAGE_SIZE, 0 );

	job_ad->Assign( ATTR_JOB_INPUT, NULL_FILE );
	job_ad->Assign( ATTR_JOB_OUTPUT, NULL_FILE );
	job_ad->Assign( ATTR_JOB_ERROR, NULL_FILE );

	job_ad->Assign( ATTR_TRANSFER_INPUT, false );
//	job_ad->Assign( ATTR_TRANSFER_OUTPUT, false );
//	job_ad->Assign( ATTR_TRANSFER_ERROR, false );
//	job_ad->Assign( ATTR_TRANSFER_EXECUTABLE, false );

	job_ad->Assign( ATTR_BUFFER_SIZE, 512*1024 );
	job_ad->Assign( ATTR_BUFFER_SIZE, 32*1024 );

	job_ad->Assign( ATTR_SHOULD_TRANSFER_FILES, true );
	job_ad->Assign( ATTR_TRANSFER_FILES, "ONEXIT" );
	job_ad->Assign( ATTR_WHEN_TO_TRANSFER_OUTPUT, FTO_ON_EXIT );

	job_ad->Assign( ATTR_REQUIREMENTS, true );

	job_ad->Assign( ATTR_PERIODIC_HOLD_CHECK, false );
	job_ad->Assign( ATTR_PERIODIC_REMOVE_CHECK, false );
	job_ad->Assign( ATTR_PERIODIC_RELEASE_CHECK, false );

	job_ad->Assign( ATTR_ON_EXIT_HOLD_CHECK, false );
	job_ad->Assign( ATTR_ON_EXIT_REMOVE_CHECK, true );

	job_ad->Assign( ATTR_JOB_ARGUMENTS, "" );

	job_ad->Assign( ATTR_JOB_LEAVE_IN_QUEUE, false );

	return job_ad;
}

bool GridftpServer::ReadUrlBase()
{
	if ( m_requestedUrlBase ) {
		dprintf( D_ALWAYS, "Reading URL base of job with requested port\n" );
		return false;
	}

	if ( m_jobId.cluster <= 0 || m_outputFile == NULL ) {
		dprintf( D_ALWAYS, "Reading URL base of unsubmitted job\n" );
		return false;
	}

	FILE *out = fopen( m_outputFile, "r" );
	if ( out == NULL ) {
		dprintf( D_ALWAYS, "Failed to open '%s': %d\n", m_outputFile, errno );
			// TODO should make job failed
		return false;
	}

	char buff[1024];
	if ( fscanf( out, "Server listening at %[^\n]\n", buff ) == 1 ) {
		MyString buff2;
		buff2.sprintf( "gsiftp://%s", buff );
		m_urlBase = strdup( buff2.Value() );
	}

	fclose( out );

	return (m_urlBase != NULL);
}

int GridftpServer::CheckJobStatus()
{

	if ( m_jobId.cluster <= 0 || m_userLog == NULL ) {
		dprintf( D_ALWAYS, "Checking status of unsubmitted job\n" );
		return STATUS_UNSUBMITTED;
	}

	ReadUserLog user_log;
	ULogEventOutcome rc;
	ULogEvent *event;

	if ( user_log.initialize( m_userLog ) == false ) {
		dprintf( D_ALWAYS, "Failed to initialize ReadUserLog\n" );
		return STATUS_DONE;
	}

	int status;
	bool status_known = false;

	for ( rc = user_log.readEvent( event ); rc == ULOG_OK; 
		  rc = user_log.readEvent( event ) ) {

		if ( event->cluster != m_jobId.cluster ||
			 event->proc != m_jobId.proc ) {
			continue;
		}

		switch( event->eventNumber ) {
		case ULOG_SUBMIT:
			status_known = true;
			status = STATUS_IDLE;
			break;
		case ULOG_EXECUTE:
			status_known = true;
			status = STATUS_ACTIVE;
			break;
		case ULOG_JOB_TERMINATED:
		case ULOG_JOB_ABORTED:
		case ULOG_JOB_HELD:
			status_known = true;
			status = STATUS_DONE;
			break;
		default:
				// Do nothing
			break;
		}

	}

	if ( rc == ULOG_RD_ERROR || rc == ULOG_UNK_ERROR ) {
		dprintf( D_ALWAYS, "Error reading user log\n" );
	}

	if ( !status_known ) {
		//dprintf( D_ALWAYS, "No status read from user log!\n" );
		EXCEPT( "No status read from user log!\n" );
	}

	return status;
}

void GridftpServer::CheckProxy()
{
	if ( m_jobId.cluster <= 0 || m_proxyFile == NULL ) {
		dprintf( D_ALWAYS, "Checking proxy of unsubmitted job\n" );
		return;
	}

	if ( m_proxyExpiration < m_proxy->expiration_time ) {
		int rc;
		MyString tmp_file;

		tmp_file.sprintf( "%s.tmp", m_proxyFile );

		rc = copy_file( m_proxy->proxy_filename, tmp_file.Value() );
		if ( rc != 0 ) {
			return;
		}

		rc = rotate_file( tmp_file.Value(), m_proxyFile );
		if ( rc != 0 ) {
			unlink( tmp_file.Value() );
			return;
		}

		m_proxyExpiration = m_proxy->expiration_time;
	}
}

bool GridftpServer::CheckPortError()
{
	if ( m_jobId.cluster <= 0 || m_errorFile == NULL ) {
		dprintf( D_ALWAYS, "Checking port-in-use error of unsubmitted job\n" );
		return false;
	}

	FILE *err = fopen( m_errorFile, "r" );
	if ( err == NULL ) {
		dprintf( D_ALWAYS, "Failed to open '%s': %d\n", m_errorFile, errno );
		return false;
	}

	bool port_in_use = false;

	char buff[1024];
	if ( fgets( buff, sizeof(buff), err ) && 
		 !strcmp( buff, "globus_xio: globus_l_xio_tcp_create_listener failed.\n" ) &&
		 fgets( buff, sizeof(buff), err ) &&
		 !strcmp( buff, "globus_xio: globus_l_xio_tcp_bind failed.\n" ) &&
		 fgets( buff, sizeof(buff), err ) &&
		 !strcmp( buff, "globus_xio: System error in bind: Permission denied\n" ) &&
		 fgets( buff, sizeof(buff), err ) &&
		 !strcmp( buff, "globus_xio: A system call failed: Permission denied\n" ) ) {
		port_in_use = true;
	}

	fclose( err );

	return port_in_use;
}

bool GridftpServer::RemoveJob()
{
	if ( m_jobId.cluster <= 0 ) {
		dprintf( D_ALWAYS, "Removing unsubmitted job\n" );
		return false;
	}

	DCSchedd dc_schedd ( ScheddAddr );
	if ( dc_schedd.error() || !dc_schedd.locate() ) {
		dprintf( D_ALWAYS, "Can't find our schedd!?\n" );
		return false;
	}

	bool success = true;

	StringList id_list;
	char job_id_buff[30];
	sprintf (job_id_buff, "%d.%d", m_jobId.cluster, m_jobId.proc );
	id_list.append( job_id_buff );

	CondorError errstack;
	ClassAd *result_ad = NULL;

	result_ad = dc_schedd.removeJobs( &id_list, "by gridmanager", &errstack );

	if (!result_ad) {
		dprintf( D_ALWAYS, "Error connecting to schedd: %s",
				 errstack.getFullText() );
		return false;
	} else {
		int result;
		sprintf( job_id_buff, "job_%d_%d", m_jobId.cluster, m_jobId.proc );
		if ( result_ad->LookupInteger( job_id_buff, result ) ) {
			switch ( result ) {
			case AR_ERROR:
				dprintf( D_ALWAYS, "removeJobs failed: General Error\n" );
				success = false;
				break;
			case AR_SUCCESS:
				success = true;;
				break;
			case AR_NOT_FOUND:
				success = true;
				break;
			case AR_BAD_STATUS:
				dprintf( D_ALWAYS, "removeJobs failed: Bad job status\n" );
				success = false;
				break;
			case AR_ALREADY_DONE:
				success = true;
				break;
			case AR_PERMISSION_DENIED:
				dprintf( D_ALWAYS, "removeJobs failed: Permission denied\n" );
				success = false;
				break;
			default:
				dprintf( D_ALWAYS, "removeJobs failed: Unknown Result");
				success = false;
			}

		} else {
			dprintf( D_ALWAYS, "removeJobs failed: Unable to get result\n" );
			success = false;
		}

		delete result_ad;

		return success;
	}
}
