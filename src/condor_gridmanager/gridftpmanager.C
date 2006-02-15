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
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_qmgr.h"
#include "util_lib_proto.h"
#include "setenv.h"
#include "env.h"
#include "get_port_range.h"
#include "basename.h"
#include "filename_tools.h"
#include "classad_helpers.h"
#include "directory.h"

#include "gridftpmanager.h"

/* GridftpServer:
 *   Invariant: Both m_urlBase and m_requestedUrlBase may not both be
 *     defined at the same time.
 */

#define QMGMT_TIMEOUT 15

#define CHECK_SERVER_INTERVAL	60
#define SERVER_JOB_LEASE		900
#define SUBMIT_ATTEMPT_INTERVAL	60

#define STATUS_UNSUBMITTED	1
#define STATUS_IDLE			2
#define STATUS_ACTIVE		3
#define STATUS_DONE			4

#define HASH_TABLE_SIZE			50

HashTable <HashKey, GridftpServer *>
    GridftpServer::m_serversByProxy( HASH_TABLE_SIZE,
									 hashFunction );

bool GridftpServer::m_initialScanDone = false;
int GridftpServer::m_updateLeasesTid = TIMER_UNSET;
bool GridftpServer::m_configRead = false;
char *GridftpServer::m_configUrlBase = NULL;

bool WriteSubmitEventToUserLog( ClassAd *job_ad );

GridftpServer::GridftpServer( Proxy *proxy )
{
	m_urlBase = NULL;
	m_requestedUrlBase = NULL;
	m_canRequestUrlBase = true;
	m_refCount = 0;
	m_jobId.cluster = 0;
	m_jobId.proc = 0;
	m_proxy = proxy->subject->master_proxy;
	m_userLog = NULL;
	m_outputFile = NULL;
	m_errorFile = NULL;
	m_proxyFile = NULL;
	m_proxyExpiration = 0;
	m_errorMessage = "";
	m_lastSubmitAttempt = 0;
	m_checkServerTid = daemonCore->Register_Timer( 0,
								(TimerHandlercpp)&GridftpServer::CheckServer,
								"GridftpServer::CheckServer", (Service*)this );
	AcquireProxy( m_proxy, m_checkServerTid );

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
	if ( m_checkServerTid != TIMER_UNSET ) {
		daemonCore->Cancel_Timer( m_checkServerTid );
	}
}

GridftpServer *GridftpServer::FindOrCreateServer( Proxy *proxy )
{
	int rc;
	GridftpServer *server = NULL;

	rc = m_serversByProxy.lookup( HashKey( proxy->subject->subject_name ),
								  server );
	if ( rc != 0 ) {
		server = new GridftpServer( proxy );
		ASSERT(server);
		m_serversByProxy.insert( HashKey( proxy->subject->subject_name ),
								 server );
	} else {
		ASSERT(server);
	}

	return server;
}

void GridftpServer::Reconfig()
{
	if ( !m_configRead ) {

			// These are config settings that we only want to read once,
			// at startup.
		m_configUrlBase = param( "GRIDFTP_URL_BASE" );

		m_configRead = true;
	}

}

void GridftpServer::RegisterClient( int notify_tid, const char *req_url_base )
{
	m_refCount++;
	if ( notify_tid > 0 &&
		 m_registeredClients.IsMember( notify_tid ) == false ) {
		m_registeredClients.Append( notify_tid );
	}
	if ( m_canRequestUrlBase && m_requestedUrlBase == NULL && req_url_base ) {
		m_requestedUrlBase = strdup( req_url_base );
	}
}

void GridftpServer::UnregisterClient( int notify_tid )
{
	m_refCount--;
	if ( notify_tid > 0 ) {
		m_registeredClients.Delete( notify_tid );
	}
}

bool GridftpServer::IsEmpty()
{
	return m_refCount == 0;
}

const char *GridftpServer::GetUrlBase()
{
	if ( m_configUrlBase ) {
		return m_configUrlBase;
	} else {
		return m_urlBase;
	}
}

const char *GridftpServer::GetErrorMessage()
{
	if ( m_errorMessage.IsEmpty() ) {
		return NULL;
	} else {
		return m_errorMessage.Value();
	}
}

bool GridftpServer::UseSelfCred()
{
	return m_configUrlBase == NULL;
}

int GridftpServer::UpdateLeases()
{
	Qmgr_connection *schedd;
	bool error = false;
	GridftpServer *server;
	int lease_expire = time(NULL) + SERVER_JOB_LEASE;
	int rc;

	dprintf( D_FULLDEBUG, "GridftpServer: Updating job leases for gridftp "
			 "server jobs\n" );

	schedd = ConnectQ( ScheddAddr, QMGMT_TIMEOUT, false );
	if ( !schedd ) {
		dprintf( D_ALWAYS, "GridftpServer::UpdateLeases: "
				 "Failed to connect to schedd\n" );
		error = true;
		goto UpdateLeases_exit;
	}

	m_serversByProxy.startIterations();

	while ( m_serversByProxy.iterate( server ) != 0 ) {

		rc = SetAttributeInt( server->m_jobId.cluster, server->m_jobId.proc,
							  ATTR_TIMER_REMOVE_CHECK, lease_expire );
		if ( rc < 0 ) {
			dprintf( D_ALWAYS, "GridftpServer::UpdateLeases: "
					 "Failed to update lease for %d.%d\n",
					 server->m_jobId.cluster, server->m_jobId.proc );
		}

	}

	if ( !DisconnectQ( schedd ) ) {
		dprintf( D_ALWAYS, "GridftpServer::UpdateLeases: "
				 "Failed to commit transcation\n" );
		error = true;
	}

 UpdateLeases_exit:
	if ( error ) {
		daemonCore->Reset_Timer( m_updateLeasesTid, 10 );
	} else {
		daemonCore->Reset_Timer( m_updateLeasesTid, SERVER_JOB_LEASE / 3 );
	}

	return TRUE;
}

void GridftpServer::CheckServerSoon( int delta )
{
	if ( m_checkServerTid != TIMER_UNSET ) {
		daemonCore->Reset_Timer( m_checkServerTid, delta );
	}
}

int GridftpServer::CheckServer()
{
	bool existing_error = !m_errorMessage.IsEmpty();
 
	daemonCore->Reset_Timer( m_checkServerTid, CHECK_SERVER_INTERVAL );

	if ( m_configUrlBase ) {
			// If GRIDFTP_URL_BASE is set in the config file, then never
			// try to start our own gridftp servers.
		daemonCore->Reset_Timer( m_checkServerTid, TIMER_NEVER );
		return TRUE;
	}

	if ( !m_initialScanDone ) {
		if ( ScanSchedd() ) {
			m_initialScanDone = true;
		} else {
			return TRUE;
		}
	}

		// Once we have a gridftp server or are about to submit one, we
		// ignore client requests for a preferred port for the server to
		// bind to.
	m_canRequestUrlBase = false;

		// TODO maybe waite 5 minutes after having no jobs before shutting
		//   server down
	if ( IsEmpty() ) {
		RemoveJob();
		delete this;
		return TRUE;
	}

		// TODO wait for an explicit request from a job before
		//   starting a server?
	int job_status;
	job_status = CheckJobStatus();

		// TODO If a job exits for an unknown reason, put the stdout/err
		//   in the log
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
				int tid;
				m_registeredClients.Rewind();
				while ( m_registeredClients.Next( tid ) ) {
					daemonCore->Reset_Timer( tid, 0 );
				}
			} else {
					// Come back in a second to check the output file again
				CheckServerSoon( 1 );
			}
		} else {
			m_urlBase = m_requestedUrlBase;
			m_requestedUrlBase = NULL;

			int tid;
			m_registeredClients.Rewind();
			while ( m_registeredClients.Next( tid ) ) {
				daemonCore->Reset_Timer( tid, 0 );
			}
		}
	}

		// TODO if proxy refresh fails, wait some time between retries
	if ( job_status == STATUS_ACTIVE ) {
		CheckProxy();
	}

	if ( job_status == STATUS_UNSUBMITTED ) {
			// If we did a submit recently, wait a while before trying again
		int now = time(NULL);
		if ( now < m_lastSubmitAttempt + SUBMIT_ATTEMPT_INTERVAL ) {
			CheckServerSoon( m_lastSubmitAttempt + SUBMIT_ATTEMPT_INTERVAL
							 - now );
		} else {
				// If our submit was successful, come back into this
				// function real soon to see if the server is working
			if ( SubmitServerJob() ) {
				CheckServerSoon( 1 );
			}
			m_lastSubmitAttempt = now;
		}
	}

	if ( !existing_error && !m_errorMessage.IsEmpty() ) {
		int tid;
		m_registeredClients.Rewind();
		while ( m_registeredClients.Next( tid ) ) {
			daemonCore->Reset_Timer( tid, 0 );
		}
	}

	return TRUE;
}

bool GridftpServer::ScanSchedd()
{
	Qmgr_connection *schedd;
	bool error = false;
	MyString expr;
	ClassAd *next_ad;

	dprintf( D_FULLDEBUG, "GridftpServer: Scanning schedd for previously "
			 "submitted gridftp server jobs\n" );

	if ( m_updateLeasesTid == TIMER_UNSET ) {
		m_updateLeasesTid = daemonCore->Register_Timer( 0,
							(TimerHandler)&GridftpServer::UpdateLeases,
							"GridftpServer::UpdateLeases", NULL );
	}

	schedd = ConnectQ( ScheddAddr, QMGMT_TIMEOUT, true );
	if ( !schedd ) {
		dprintf( D_ALWAYS, "GridftpServer::ScanSchedd: "
				 "Failed to connect to schedd\n" );
		return false;
	}

		// TODO ignore jobs that aren't IDLE or RUNNING
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
			server = new GridftpServer( proxy );
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
			if ( server->m_requestedUrlBase ) {
				free( server->m_requestedUrlBase );
				server->m_requestedUrlBase = NULL;
			}
			MyString value;
			next_ad->LookupString( ATTR_JOB_OUTPUT, value );
			server->m_outputFile = strdup( value.Value() );
			next_ad->LookupString( ATTR_JOB_ERROR, value );
			server->m_errorFile = strdup( value.Value() );
			next_ad->LookupString( ATTR_ULOG_FILE, value );
			server->m_userLog = strdup( value.Value() );
			next_ad->LookupString( ATTR_X509_USER_PROXY, value );
			server->m_proxyFile = strdup( value.Value() );
			if ( next_ad->LookupString( ATTR_REQUESTED_GRIDFTP_URL_BASE,
										value ) ) {
				server->m_requestedUrlBase = strdup( value.Value() );
			}
				// TODO check expiration time on proxy?
		}

		delete next_ad;
			// TODO check that this didn't return NULL due to a connection
			//   failure
		next_ad = GetNextJobByConstraint( expr.Value(), 0 );
	}

	DisconnectQ( schedd );

	if ( !error ) {
		GridftpServer *server;
		m_serversByProxy.startIterations();

		while ( m_serversByProxy.iterate( server ) != 0 ) {
			server->CheckServerSoon();
		}
	}

	return !error;
}

bool GridftpServer::SubmitServerJob()
{
	char *server_path = NULL;
	char *wrapper_path = NULL;
	ClassAd *job_ad = NULL;
	char mapfile[1024] = "";
	FILE *mapfile_fp = NULL;
	Qmgr_connection *schedd = NULL;
	int cluster_id = -1;
	int proc_id = -1;
	ExprTree *tree = NULL;
	CondorError errstack;
	Env env_obj;
	int low_port, high_port;
	const char *value;
	MyString userlog;
	MyString buff;
	CondorVersionInfo ver_info;
	ArgList args_list;

	dprintf( D_FULLDEBUG, "GridftpServer: Submitting job for proxy '%s'\n",
			 m_proxy->subject->subject_name );
	if ( m_requestedUrlBase ) {
		dprintf( D_FULLDEBUG, "   reusing URL %s\n", m_requestedUrlBase );
	}
	DCSchedd dc_schedd( ScheddAddr );
	if ( dc_schedd.error() || !dc_schedd.locate() ) {
		dprintf( D_ALWAYS, "GridftpServer::SubmitServerJob: Can't find "
				 "our schedd!?\n" );
		return false;
	}

	server_path = param( "GRIDFTP_SERVER" );
	if ( !server_path ) {
		dprintf( D_ALWAYS, "GridftpServer::SubmitServerJob: No gridftp "
				 "server configured\n" );
		m_errorMessage.sprintf( "No gridftp server configured" );
		return false;
	} else {
		StatInfo si( server_path );
		if ( si.Error() || !si.IsExecutable() ) {
			dprintf( D_ALWAYS, "GridftpServer::SubmitServerJob: "
					 "GRIDFTP_SERVER file %s is invalid\n",
					 server_path );
			m_errorMessage.sprintf( "GRIDFTP_SERVER file is invalid" );
			goto error_exit;
		}
	}

	wrapper_path = param( "GRIDFTP_SERVER_WRAPPER" );
	if ( wrapper_path ) {
		StatInfo si( wrapper_path );
		if ( si.Error() || !si.IsExecutable() ) {
			dprintf( D_ALWAYS, "GridftpServer::SubmitServerJob: "
					 "GRIDFTP_SERVER_WRAPPER file %s is invalid\n",
					 server_path );
			m_errorMessage.sprintf( "GRIDFTP_SERVER_WRAPPER file is invalid" );
			goto error_exit;
		}
	}

	job_ad = CreateJobAd( Owner, CONDOR_UNIVERSE_SCHEDULER,
						  wrapper_path ? wrapper_path : server_path );

	job_ad->Assign( ATTR_JOB_STATUS, HELD );
	job_ad->Assign( ATTR_HOLD_REASON, "Spooling input data files" );
	job_ad->Assign( ATTR_X509_USER_PROXY, m_proxy->proxy_filename );
	job_ad->Assign( ATTR_X509_USER_PROXY_SUBJECT,
					m_proxy->subject->subject_name );
	job_ad->Assign( ATTR_JOB_OUTPUT, "/tmp/out" );
	job_ad->Assign( ATTR_JOB_ERROR, "/tmp/err" );
	userlog.sprintf( "%s/gridftp.log", GridmanagerScratchDir );
	job_ad->Assign( ATTR_ULOG_FILE, userlog.Value() );

	job_ad->Assign( ATTR_TIMER_REMOVE_CHECK,
					(int)time(NULL) + SERVER_JOB_LEASE );
	buff.sprintf( "%s > CurrentTime", ATTR_TIMER_REMOVE_CHECK );
	job_ad->AssignExpr( ATTR_JOB_LEAVE_IN_QUEUE, buff.Value() );

	snprintf( mapfile, sizeof(mapfile), "%s/grid-mapfile",
			  GridmanagerScratchDir );
	mapfile_fp = fopen( mapfile, "w" );
	if ( mapfile_fp == NULL ) {
		dprintf( D_ALWAYS, "GridftpServer::SubmitServerJob: Failed to open "
				 "%s for write, errno=%d\n", mapfile, errno );
		goto error_exit;
	}

	if ( fprintf( mapfile_fp, "\"%s\" %s\n", m_proxy->subject->subject_name,
				  Owner ) < 0 ) {
		dprintf( D_ALWAYS, "GridftpServer::SubmitServerJob: Failed to write "
				 "to %s, errno=%d\n", mapfile, errno );
		goto error_exit;
	}

	if ( fclose( mapfile_fp ) < 0 ) {
		dprintf( D_ALWAYS, "GridftpServer::SubmitServerJob:: Failed to "
				 "close %s, errno=%d\n", mapfile, errno );
		mapfile_fp = NULL;
		goto error_exit;
	}
	mapfile_fp = NULL;

	job_ad->Assign( ATTR_TRANSFER_INPUT_FILES, mapfile );

		// TODO The gridftp server doesn't like having a relative path
		//   to the grid-mapfile. We need a wrapper script to set the
		//   absolute path.
	env_obj.SetEnv( "GRIDMAP", condor_basename( mapfile ) );

		// if the condor_config has set any of the incoming or
		// outgoing LOWPORT/HIGHPORT settings, pass them through
		// to globus via the appropriate environment variables.
	if ( get_port_range( FALSE, &low_port, &high_port ) == TRUE ) {
		MyString buff;
		buff.sprintf( "%d.%d", low_port, high_port );
		env_obj.SetEnv( "GLOBUS_TCP_PORT_RANGE", buff.Value() );
	}
	if ( get_port_range( TRUE, &low_port, &high_port ) == TRUE ) {
		MyString buff;
		buff.sprintf( "%d.%d", low_port, high_port );
		env_obj.SetEnv( "GLOBUS_TCP_SOURCE_RANGE", buff.Value() );
	}

		// TODO Should check config parameter GSI_DAEMON_TRUSTED_CA_DIR
	value = GetEnv( "X509_CERT_DIR" );
	if ( value ) {
		env_obj.SetEnv( "X509_CERT_DIR", value );
	}

	if ( !env_obj.InsertEnvIntoClassAd( job_ad, &buff ) ) {
		dprintf( D_ALWAYS, "GridftpServer::SubmitServerJob: Env: %s\n",
				 buff.Value() );
		goto error_exit;
	}		
		// TODO what about LD_LIBRARY_PATH?

		// Set up the arguments to the job
	if ( wrapper_path ) {
		args_list.AppendArg( server_path );
	}

	if ( m_requestedUrlBase ) {
		char url_scheme[_POSIX_PATH_MAX];
		char url_host[_POSIX_PATH_MAX];
		char url_path[_POSIX_PATH_MAX];
		int url_port;
		filename_url_parse( m_requestedUrlBase, url_scheme, url_host,
							&url_port, url_path );
		if ( strcasecmp( "gsiftp", url_scheme ) ) {
				// The requested URL base is malformed, discard it
			free( m_requestedUrlBase );
			m_requestedUrlBase = NULL;
		} else {
			if ( url_port == -1 ) {
				url_port = 2811;
			}
			args_list.AppendArg( "-p" );
			buff.sprintf( "%d ", url_port );
			args_list.AppendArg( buff.Value() );

			job_ad->Assign( ATTR_REQUESTED_GRIDFTP_URL_BASE,
							m_requestedUrlBase );
		}
	}

	if ( !args_list.InsertArgsIntoClassAd( job_ad, &ver_info, &buff ) ) {
		dprintf( D_ALWAYS, "GridftpServer::SubmitServerJob: ArgList: %s\n",
				 buff.Value() );
		goto error_exit;
	}

	job_ad->Assign( ATTR_GRIDFTP_SERVER_JOB, true );

	schedd = ConnectQ( ScheddAddr, QMGMT_TIMEOUT, false );
	if ( !schedd ) {
		dprintf( D_ALWAYS, "GridftpServer::SubmitServerJob: "
				 "Failed to connect to schedd\n" );
		goto error_exit;
	}

	if ( (cluster_id = NewCluster()) >= 0 ) {
		proc_id = NewProc( cluster_id );
	}

	if ( cluster_id == -2 || proc_id == -2 ) {
		dprintf( D_ALWAYS, "GridftpServer::SubmitServerJob: Number of "
				 "submitted jobs would exceed MAX_JOBS_SUBMITTED\n" );
	}
	if ( cluster_id < 0 ) {
		dprintf( D_ALWAYS, "GridftpServer::SubmitServerJob: Unable to "
				 "create a new job cluster\n" );
		goto error_exit;
	} else if ( proc_id < 0 ) {
		dprintf( D_ALWAYS, "GridftpServer::SubmitServerJob: Unable to "
				 "create a new job proc\n" );
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
			dprintf( D_ALWAYS, "GridftpServer::SubmitServerJob: Failed to "
					 "unparse job attribute\n" );
			goto error_exit;
		} else if ( SetAttribute( cluster_id, proc_id, lhstr, rhstr ) == -1 ) {
			dprintf( D_ALWAYS, "GridftpServer::SubmitServerJob: Failed to "
					 "SetAttribute %s=%s for job %d.%d\n",
					 lhstr, rhstr, cluster_id, proc_id );
			free( lhstr );
			free( rhstr );
			goto error_exit;
		}

		free( lhstr );
		free( rhstr );
	}

	if ( CloseConnection() < 0 ) {
		dprintf( D_ALWAYS, "GridftpServer::SubmitServerJob: Failed to "
				 "commit job submission\n" );
		goto error_exit;
	}

	DisconnectQ( schedd );
	schedd = NULL;

	WriteSubmitEventToUserLog( job_ad );

	if ( !dc_schedd.spoolJobFiles( 1, &job_ad, &errstack ) ) {
		dprintf( D_ALWAYS, "GridftpServer::SubmitServerJob: Failed to "
				 "stage in files: %s\n", errstack.getFullText() );
		goto error_exit;
	}

	m_outputFile = NULL;
	m_errorFile = NULL;
	m_userLog = NULL;
	m_proxyFile = NULL;
	while ( m_userLog == NULL ) {

		schedd = ConnectQ( ScheddAddr, QMGMT_TIMEOUT, true );
		if ( !schedd ) {
			dprintf( D_ALWAYS, "GridftpServer::SubmitServerJob: "
					 "Failed to connect to schedd (2nd time)\n" );
			goto error_exit;
		}

			// TODO Shouldn't block waiting for the schedd to finish the
			//   stage-in process. Instead, we should go back into the main
			//   event loop and come back here in a second.
		int val;
		if ( GetAttributeInt( cluster_id, proc_id, ATTR_STAGE_IN_FINISH,
							  &val ) == 0 ) {
			if ( GetAttributeStringNew( cluster_id, proc_id, ATTR_JOB_OUTPUT, 
										&m_outputFile ) < 0 ||
				 GetAttributeStringNew( cluster_id, proc_id, ATTR_JOB_ERROR, 
										&m_errorFile ) < 0 ||
				 GetAttributeStringNew( cluster_id, proc_id, ATTR_ULOG_FILE, 
										&m_userLog ) < 0 ||
				 GetAttributeStringNew( cluster_id, proc_id,
										ATTR_X509_USER_PROXY,
										&m_proxyFile ) < 0 ) {
				dprintf( D_ALWAYS, "GridftpServer::SubmitServerJob: Failed "
						 "to read job attributes\n" );
				goto error_exit;
			}
		}

		DisconnectQ( schedd );
		schedd = NULL;

		sleep( 1 );
	}

	m_jobId.cluster = cluster_id;
	m_jobId.proc = proc_id;
	m_proxyExpiration = m_proxy->expiration_time;

	if ( unlink( mapfile ) < 0 ) {
		dprintf( D_ALWAYS, "GridftpServer::SubmitServerJob: Failed to "
				 "unlink %s, errno=%d\n", mapfile, errno );
	}

	delete job_ad;
	unlink( userlog.Value() );

	return true;

 error_exit:
	free( server_path );
	free( wrapper_path );
	if ( userlog.Length() > 0 ) {
		unlink( userlog.Value() );
	}
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

bool GridftpServer::ReadUrlBase()
{
	if ( m_requestedUrlBase ) {
		dprintf( D_ALWAYS, "GridftpServer::ReadUrlBase: Reading URL base "
				 "of job with requested port\n" );
		return false;
	}

	if ( m_jobId.cluster <= 0 || m_outputFile == NULL ) {
		dprintf( D_ALWAYS, "GridftpServer::ReadUrlBase: Reading URL base "
				 "of unsubmitted job\n" );
		return false;
	}

	FILE *out = fopen( m_outputFile, "r" );
	if ( out == NULL ) {
		dprintf( D_ALWAYS, "GridftpServer::ReadUrlBase: Failed to open "
				 "'%s': %d\n", m_outputFile, errno );
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
		return STATUS_UNSUBMITTED;
	}

	ReadUserLog user_log;
	ULogEventOutcome rc;
	ULogEvent *event;

	if ( user_log.initialize( m_userLog ) == false ) {
		dprintf( D_ALWAYS, "GridftpServer::CheckJobStatus: Failed to "
				 "initialize ReadUserLog(%s)\n", m_userLog );
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
		dprintf( D_ALWAYS, "GridftpServer::CheckJobStatus: Error reading "
				 "user log\n" );
	}

	if ( !status_known ) {
		//dprintf( D_ALWAYS, "GridftpServer::ReadUrlBase: No status read from user log!\n" );
		// TODO Shouldn't EXCEPT here, because user log can disappear
		//   along with job
		EXCEPT( "GridftpServer::CheckJobStatus: No status read from user log!\n" );
	}

	return status;
}

void GridftpServer::CheckProxy()
{
	if ( m_jobId.cluster <= 0 || m_proxyFile == NULL ) {
		dprintf( D_ALWAYS, "GridftpServer::CheckProxy: Checking proxy of unsubmitted job\n" );
		return;
	}

		// TODO Should probably use the schedd's proxy refresh call
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
		dprintf( D_ALWAYS, "GridftpServer::CheckPortError: Checking "
				 "port-in-use error of unsubmitted job\n" );
		return false;
	}

	FILE *err = fopen( m_errorFile, "r" );
	if ( err == NULL ) {
		dprintf( D_ALWAYS, "GridftpServer::CheckPortError: Failed to "
				 "open '%s': %d\n", m_errorFile, errno );
		return false;
	}

	bool port_in_use = false;

	char buff[1024];
#if 0
	if ( fgets( buff, sizeof(buff), err ) == NULL ) {
		dprintf(D_FULLDEBUG,"    0: '%s'\n",buff);
		port_in_use = false;
	} else if ( fgets( buff, sizeof(buff), err ) == NULL ||
		 strcmp( buff, "globus_xio: globus_l_xio_tcp_create_listener failed.\n" ) ) {
		dprintf(D_FULLDEBUG,"    1: '%s'\n",buff);
		port_in_use = false;
	} else if ( fgets( buff, sizeof(buff), err ) == NULL ||
				strcmp( buff, "globus_xio: globus_l_xio_tcp_bind failed.\n" ) ) {
		dprintf(D_FULLDEBUG,"    2: '%s'\n",buff);
		port_in_use = false;
	} else if ( fgets( buff, sizeof(buff), err ) == NULL ||
				strcmp( buff, "globus_xio: System error in bind: Address already in use\n" ) ) {
		dprintf(D_FULLDEBUG,"    3: '%s'\n",buff);
		port_in_use = false;
	} else if ( fgets( buff, sizeof(buff), err ) == NULL ||
				strcmp( buff, "globus_xio: A system call failed: Address already in use\n" ) ) {
		dprintf(D_FULLDEBUG,"    4: '%s'\n",buff);
		port_in_use = false;
	} else {
		dprintf(D_FULLDEBUG,"    port in use\n");
		port_in_use = true;
	}
#else
	if ( fgets( buff, sizeof(buff), err ) && // skip first line
		 fgets( buff, sizeof(buff), err ) && 
		 !strcmp( buff, "globus_xio: globus_l_xio_tcp_create_listener failed.\n" ) &&
		 fgets( buff, sizeof(buff), err ) &&
		 !strcmp( buff, "globus_xio: globus_l_xio_tcp_bind failed.\n" ) &&
		 fgets( buff, sizeof(buff), err ) &&
		 !strcmp( buff, "globus_xio: System error in bind: Address already in use\n" ) &&
		 fgets( buff, sizeof(buff), err ) &&
		 !strcmp( buff, "globus_xio: A system call failed: Address already in use\n" ) ) {
		port_in_use = true;
	}
#endif

	fclose( err );

	return port_in_use;
}

bool GridftpServer::RemoveJob()
{
	if ( m_jobId.cluster <= 0 ) {
		dprintf( D_ALWAYS, "GridftpServer::RemoveJob: Removing "
				 "unsubmitted job\n" );
		return false;
	}

	DCSchedd dc_schedd ( ScheddAddr );
	if ( dc_schedd.error() || !dc_schedd.locate() ) {
		dprintf( D_ALWAYS, "GridftpServer::RemoveJob: Can't find our "
				 "schedd!?\n" );
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
		dprintf( D_ALWAYS, "GridftpServer::RemoveJob: Error connecting "
				 "to schedd: %s", errstack.getFullText() );
		return false;
	} else {
		int result;
		sprintf( job_id_buff, "job_%d_%d", m_jobId.cluster, m_jobId.proc );
		if ( result_ad->LookupInteger( job_id_buff, result ) ) {
			switch ( result ) {
			case AR_ERROR:
				dprintf( D_ALWAYS, "GridftpServer::RemoveJob: removeJobs "
						 "failed: General Error\n" );
				success = false;
				break;
			case AR_SUCCESS:
				success = true;;
				break;
			case AR_NOT_FOUND:
				success = true;
				break;
			case AR_BAD_STATUS:
				dprintf( D_ALWAYS, "GridftpServer::RemoveJob: removeJobs "
						 "failed: Bad job status\n" );
				success = false;
				break;
			case AR_ALREADY_DONE:
				success = true;
				break;
			case AR_PERMISSION_DENIED:
				dprintf( D_ALWAYS, "GridftpServer::RemoveJob: removeJobs "
						 "failed: Permission denied\n" );
				success = false;
				break;
			default:
				dprintf( D_ALWAYS, "GridftpServer::RemoveJob: removeJobs "
						 "failed: Unknown Result");
				success = false;
			}

		} else {
			dprintf( D_ALWAYS, "GridftpServer::RemoveJob: removeJobs "
					 "failed: Unable to get result\n" );
			success = false;
		}

		delete result_ad;

		return success;
	}
}

bool
WriteSubmitEventToUserLog( ClassAd *job_ad )
{
	int cluster, proc;

	UserLog *ulog = InitializeUserLog( job_ad );
	if ( ulog == NULL ) {
		// User doesn't want a log
		return true;
	}

	job_ad->LookupInteger( ATTR_CLUSTER_ID, cluster );
	job_ad->LookupInteger( ATTR_PROC_ID, proc );

	dprintf( D_FULLDEBUG, 
			 "(%d.%d) Writing submit record to user logfile\n",
			 cluster, proc );

	SubmitEvent event;
	strcpy( event.submitHost, ScheddAddr );
	int rc = ulog->writeEvent(&event);
	delete ulog;

	if (!rc) {
		dprintf( D_ALWAYS,
				 "(%d.%d) Unable to log ULOG_SUBMIT event\n",
				 cluster, proc );
		return false;
	}

	return true;
}
