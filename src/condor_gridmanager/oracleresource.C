/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
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


#include "condor_common.h"
#include "condor_config.h"
#include "string_list.h"

#include "oracleresource.h"
#include "gridmanager.h"

#define MIN_PROBE_JOBS_INTERVAL		2
#define MAX_PROBE_JOBS_INTERVAL		10

template class List<OracleJob>;
template class Item<OracleJob>;
template class List<OciSession>;
template class Item<OciSession>;
template class HashTable<HashKey, OciSession *>;
template class HashBucket<HashKey, OciSession *>;
template class HashTable<HashKey, OracleJob *>;
template class HashBucket<HashKey, OracleJob *>;

#define HASH_TABLE_SIZE			500

template class HashTable<HashKey, OciServer *>;
template class HashBucket<HashKey, OciServer *>;

HashTable <HashKey, OciServer *> ServersByName( HASH_TABLE_SIZE,
												hashFunction );

OciSession *GetOciSession( const char *db_name, const char *db_username,
						   const char *db_password )
{
	int rc;
	OciServer *server;
	OciSession *session;

	if ( db_name == NULL || db_username == NULL || db_password == NULL ) {
		return NULL;
	}

	rc = ServersByName.lookup( HashKey( db_name ), server );

	if ( rc != 0 ) {
		server = new OciServer( db_name );
		ASSERT(server);
		if ( server->Initialize() != OCI_SUCCESS ) {
			delete server;
			return NULL;
		}
		ServersByName.insert( HashKey( db_name ), server );
	} else {
		ASSERT(server);
	}

	session = server->FindSession( db_username );
	if ( session == NULL ) {
		session = new OciSession( server, db_username, db_password );
		ASSERT(session);
		if ( session->Initialize() != OCI_SUCCESS ) {
			delete session;
			return NULL;
		}
	} else {
		ASSERT(session);
	}

	return session;
}

OciSession::OciSession( OciServer *oci_server, const char *db_username,
						const char *db_password )
{
	initDone = false;
	server = oci_server;
	username = strdup( db_username );
	password = strdup( db_password );
	ociSvcCtxHndl = NULL;
	ociSessionHndl = NULL;
	ociTransHndl = NULL;
	ociErrorHndl = NULL;
	sessionOpen = false;
	registeredJobs = new List<OracleJob>();
	lastProbeJobsTime = 0;

	server->RegisterSession( this, username );

	probeJobsTid = daemonCore->Register_Timer( 0,
								(TimerHandlercpp)&OciSession::doProbeJobs,
								"doProbeJobs", (Service*) this );
}

OciSession::~OciSession()
{
	daemonCore->Cancel_Timer( probeJobsTid );
	if ( registeredJobs != NULL ) {
		delete registeredJobs;
	}
	if ( server != NULL ) {
		server->UnregisterSession( this, username );
	}
	if ( ociTransHndl != NULL ) {
		OCIHandleFree( ociTransHndl, OCI_HTYPE_TRANS );
	}
	if ( ociSessionHndl != NULL ) {
		OCIHandleFree( ociSessionHndl, OCI_HTYPE_SESSION );
	}
	if ( ociSvcCtxHndl != NULL ) {
		OCIHandleFree( ociSvcCtxHndl, OCI_HTYPE_SVCCTX );
	}
	if ( ociErrorHndl != NULL ) {
		OCIHandleFree( ociErrorHndl, OCI_HTYPE_ERROR );
	}
	if ( username != NULL ) {
		free( username );
	}
	if ( password != NULL ) {
		free( username );
	}
}

int OciSession::Initialize()
{
	int rc;

	if ( initDone ) {
		return OCI_SUCCESS;
	}

	if ( ociErrorHndl == NULL ) {
		rc = OCIHandleAlloc( GlobalOciEnvHndl, (dvoid**)&ociErrorHndl,
							 OCI_HTYPE_ERROR, 0, NULL );
		if ( rc != OCI_SUCCESS ) {
			return rc;
		}
	}

	if ( ociSessionHndl == NULL ) {
		rc = OCIHandleAlloc( GlobalOciEnvHndl, (dvoid**)&ociSessionHndl,
							 OCI_HTYPE_SESSION, 0, NULL );
		if ( rc != OCI_SUCCESS ) {
			return rc;
		}
	}

	if ( ociSvcCtxHndl == NULL ) {
		rc = OCIHandleAlloc( GlobalOciEnvHndl, (dvoid**)&ociSvcCtxHndl,
							 OCI_HTYPE_SVCCTX, 0, NULL );
		if ( rc != OCI_SUCCESS ) {
			return rc;
		}
	}

	if ( ociTransHndl == NULL ) {
		rc = OCIHandleAlloc( GlobalOciEnvHndl, (dvoid**)&ociTransHndl,
							 OCI_HTYPE_TRANS, 0, NULL );
		if ( rc != OCI_SUCCESS ) {
			return rc;
		}
	}

	rc = OCIAttrSet( ociSessionHndl, OCI_HTYPE_SESSION, username,
					 strlen(username), OCI_ATTR_USERNAME, ociErrorHndl );
	if ( rc != OCI_SUCCESS ) {
		return rc;
	}

	rc = OCIAttrSet( ociSessionHndl, OCI_HTYPE_SESSION, password,
					 strlen(password), OCI_ATTR_PASSWORD, ociErrorHndl );
	if ( rc != OCI_SUCCESS ) {
		return rc;
	}

	rc = OCIAttrSet( ociSvcCtxHndl, OCI_HTYPE_SVCCTX, ociSessionHndl, 0,
					 OCI_ATTR_SESSION, ociErrorHndl );
	if ( rc != OCI_SUCCESS ) {
		return rc;
	}

	rc = OCIAttrSet( ociSvcCtxHndl, OCI_HTYPE_SVCCTX, ociTransHndl, 0,
					 OCI_ATTR_TRANS, ociErrorHndl );
	if ( rc != OCI_SUCCESS ) {
		return rc;
	}

	initDone = true;

	return OCI_SUCCESS;
}

void OciSession::ServerConnectionClosing()
{
	int rc;

		// TODO: what about any open transactions?

	rc = CloseSession();
	if ( rc != OCI_SUCCESS ) {
		EXCEPT( "OCISessionEnd() failed!\n" );
	}

	return;
}

void OciSession::RegisterJob( OracleJob *job )
{
	registeredJobs->Append( job );
}

void OciSession::UnregisterJob( OracleJob *job )
{

	registeredJobs->Delete( job );

	if ( registeredJobs->IsEmpty() ) {
		this->~OciSession();
	}
}

int OciSession::AcquireSession( OracleJob *job, OCISvcCtx *&svc_hndl,
								OCIError *&err_hndl )
{
	int rc;

	if ( sessionOpen == false ) {
		rc = OpenSession( err_hndl );
		if ( rc != OCI_SUCCESS ) {
			return rc;
		}
	}

	svc_hndl = ociSvcCtxHndl;

	return OCI_SUCCESS;
}

int OciSession::ReleaseSession( OracleJob *job )
{
	return OCI_SUCCESS;
}

void OciSession::RequestProbeJobs()
{
	if ( lastProbeJobsTime + MIN_PROBE_JOBS_INTERVAL <= time(NULL) ) {
		daemonCore->Reset_Timer( probeJobsTid, 0 );
	} else {
		daemonCore->Reset_Timer( probeJobsTid,
								 lastProbeJobsTime + MIN_PROBE_JOBS_INTERVAL );
	}
}

int OciSession::doProbeJobs()
{
	OCIStmt *stmt_hndl = NULL;
	OCIDateTime *job_this_date = NULL;
	sb2 this_date_indp;
	sb2 failures_indp;
	OCIDefine *define1_hndl = NULL;
	OCIDefine *define2_hndl = NULL;
	OCIDefine *define3_hndl = NULL;
	OCIDefine *define4_hndl = NULL;
	int rc;
	bool trans_open = false;
	MyString stmt;
	int failures;
	char broken_str[2];
	int job_id;
	char job_id_str[16];
	HashTable <HashKey, OracleJob *> *jobsByRemoteId = NULL;
	OracleJob *job = NULL;

	lastProbeJobsTime = time(NULL);
	daemonCore->Reset_Timer( probeJobsTid, MAX_PROBE_JOBS_INTERVAL );

	if ( !initDone || registeredJobs->IsEmpty() ) {
		return TRUE;
	}

	jobsByRemoteId = new HashTable<HashKey,OracleJob*>( HASH_TABLE_SIZE,
														hashFunction );
	registeredJobs->Rewind();
	while ( registeredJobs->Next( job ) ) {
		if ( job->remoteJobId != NULL ) {
			jobsByRemoteId->insert( HashKey( job->remoteJobId ), job );
		}
	}

	rc = OpenSession( ociErrorHndl );
	if ( rc != OCI_SUCCESS ) {
		dprintf( D_ALWAYS, "OpenSession failed!\n" );
		print_error( rc, ociErrorHndl );
		goto doProbeJobs_error_exit;
	}

	rc = OCITransStart( ociSvcCtxHndl, ociErrorHndl, 60, OCI_TRANS_NEW );
	if ( rc != OCI_SUCCESS ) {
		dprintf( D_ALWAYS, "OCITransStart failed\n" );
		print_error( rc, ociErrorHndl );
		goto doProbeJobs_error_exit;
	}
	trans_open = true;

	rc = OCIHandleAlloc( GlobalOciEnvHndl, (dvoid**)&stmt_hndl, OCI_HTYPE_STMT,
						 0, NULL );
	if ( rc != OCI_SUCCESS ) {
		dprintf( D_ALWAYS, "OCIHandleAlloc failed\n" );
		print_error( rc, NULL );
		goto doProbeJobs_error_exit;
	}

	rc = OCIDescriptorAlloc( GlobalOciEnvHndl, (dvoid **)&job_this_date,
							 OCI_DTYPE_DATE, 0, 0 );
	if ( rc != OCI_SUCCESS ) {
		dprintf( D_ALWAYS, "OCIDescriptorAlloc failed\n" );
		print_error( rc, NULL );
		goto doProbeJobs_error_exit;
	}

	stmt.sprintf( "SELECT job, failures, this_date, broken FROM user_jobs" );

	rc = OCIStmtPrepare( stmt_hndl, ociErrorHndl, (const OraText *)stmt.Value(),
						 strlen(stmt.Value()), OCI_NTV_SYNTAX, OCI_DEFAULT );
	if ( rc != OCI_SUCCESS ) {
		dprintf( D_ALWAYS, "OCIStmtPrepare failed\n" );
		print_error( rc, ociErrorHndl );
		goto doProbeJobs_error_exit;
	}

	rc = OCIStmtExecute( ociSvcCtxHndl, stmt_hndl, ociErrorHndl, 0, 0, NULL,
						 NULL, OCI_DEFAULT );
	if ( rc != OCI_SUCCESS ) {
		dprintf( D_ALWAYS, "OCIStmtExecute failed\n" );
		print_error( rc, ociErrorHndl );
		goto doProbeJobs_error_exit;
	}

	rc = OCIDefineByPos( stmt_hndl, &define1_hndl, ociErrorHndl, 1, &job_id,
						 sizeof(job_id), SQLT_INT, NULL, NULL, NULL,
						 OCI_DEFAULT );
	if ( rc != OCI_SUCCESS ) {
		dprintf( D_ALWAYS, "OCIDefineByPos failed\n" );
		print_error( rc, ociErrorHndl );
		goto doProbeJobs_error_exit;
	}

	rc = OCIDefineByPos( stmt_hndl, &define2_hndl, ociErrorHndl, 2, &failures,
						 sizeof(failures), SQLT_INT, &failures_indp, NULL,
						 NULL, OCI_DEFAULT );
	if ( rc != OCI_SUCCESS ) {
		dprintf( D_ALWAYS, "OCIDefineByPos failed\n" );
		print_error( rc, ociErrorHndl );
		goto doProbeJobs_error_exit;
	}

	rc = OCIDefineByPos( stmt_hndl, &define3_hndl, ociErrorHndl, 3,
						 &job_this_date, sizeof(job_this_date), SQLT_DATE,
						 &this_date_indp, NULL, NULL, OCI_DEFAULT);
	if ( rc != OCI_SUCCESS ) {
		dprintf( D_ALWAYS, "OCIDefineByPos failed\n" );
		print_error( rc, ociErrorHndl );
		goto doProbeJobs_error_exit;
	}

	rc = OCIDefineByPos( stmt_hndl, &define4_hndl, ociErrorHndl, 4,
						 broken_str, sizeof(broken_str), SQLT_STR, NULL, NULL,
						 NULL, OCI_DEFAULT);
	if ( rc != OCI_SUCCESS ) {
		dprintf( D_ALWAYS, "OCIDefineByPos failed\n" );
		print_error( rc, ociErrorHndl );
		goto doProbeJobs_error_exit;
	}

	rc = OCIStmtFetch( stmt_hndl, ociErrorHndl, 1, OCI_FETCH_NEXT,
					   OCI_DEFAULT );
	while ( rc != OCI_NO_DATA ) {
		if ( rc != OCI_SUCCESS ) {
			dprintf( D_ALWAYS, "OCIStmtFetch failed\n" );
			print_error( rc, ociErrorHndl );
			goto doProbeJobs_error_exit;
		}

		sprintf( job_id_str, "%d", job_id );
		rc = jobsByRemoteId->lookup( HashKey( job_id_str ), job );
		if ( rc == 0 && job != NULL ) {

			int job_state;
			if ( broken_str[0] == 'Y' ) {
				if ( failures_indp == -1 || failures == 0 ) {
					job_state = ORACLE_JOB_SUBMIT;
				} else {
					job_state = ORACLE_JOB_BROKEN;
				}
			} else if ( this_date_indp >= 0 ) {
				job_state = ORACLE_JOB_ACTIVE;
			} else {
				job_state = ORACLE_JOB_IDLE;
			}

			job->UpdateRemoteState( job_state );

			jobsByRemoteId->remove( HashKey( job_id_str ) );
		}

		rc = OCIStmtFetch( stmt_hndl, ociErrorHndl, 1, OCI_FETCH_NEXT,
						   OCI_DEFAULT );
	}

	rc = OCITransCommit( ociSvcCtxHndl, ociErrorHndl, 0 );
	if ( rc != OCI_SUCCESS ) {
		dprintf( D_ALWAYS, "OCITransCommit failed\n" );
		print_error( rc, ociErrorHndl );
		goto doProbeJobs_error_exit;
	}
	trans_open = false;

	if ( job_this_date != NULL ) {
		rc = OCIDescriptorFree( job_this_date, OCI_DTYPE_DATE );
		if ( rc != OCI_SUCCESS ) {
			dprintf( D_ALWAYS, "OCIDescriptorFree failed, ignoring\n" );
			print_error( rc, NULL );
		}
	}
	job_this_date = NULL;

	rc = OCIHandleFree( stmt_hndl, OCI_HTYPE_STMT );
	if ( rc != OCI_SUCCESS ) {
		dprintf( D_ALWAYS, "OCIHandleFree failed, ignoring\n" );
		print_error( rc, NULL );
	}
	stmt_hndl = NULL;

	jobsByRemoteId->startIterations();

	while ( jobsByRemoteId->iterate( job ) != 0 ) {
		job->UpdateRemoteState( ORACLE_JOB_UNQUEUED );
	}

	delete jobsByRemoteId;

	return TRUE;

 doProbeJobs_error_exit:
	if ( job_this_date != NULL ) {
		OCIDescriptorFree( job_this_date, OCI_DTYPE_DATE );
	}
	if ( stmt_hndl != NULL ) {
		OCIHandleFree( stmt_hndl, OCI_HTYPE_STMT );
	}
	if ( trans_open ) {
		OCITransRollback( ociSvcCtxHndl, ociErrorHndl, OCI_DEFAULT );
	}
	if ( jobsByRemoteId != NULL ) {
		delete jobsByRemoteId;
	}
	return TRUE;
}

int OciSession::OpenSession( OCIError *&err_hndl )
{
	int rc;
	OCIServer *server_hndl;

	if ( sessionOpen ) {
		return OCI_SUCCESS;
	}

	rc = server->SessionActive( this, server_hndl, err_hndl );
	if ( rc != OCI_SUCCESS ) {
		return rc;
	}

	rc = OCIAttrSet( ociSvcCtxHndl, OCI_HTYPE_SVCCTX, server_hndl, 0,
					 OCI_ATTR_SERVER, ociErrorHndl );
	if ( rc != OCI_SUCCESS ) {
		if ( rc == OCI_ERROR ) {
			err_hndl = ociErrorHndl;
		}
		server->SessionInactive( this );
		return rc;
	}

	rc = OCISessionBegin( ociSvcCtxHndl, ociErrorHndl, ociSessionHndl,
						  OCI_CRED_RDBMS, OCI_DEFAULT );
	if ( rc != OCI_SUCCESS ) {
		if ( rc == OCI_ERROR ) {
			err_hndl = ociErrorHndl;
		}
		server->SessionInactive( this );	
		return rc;
	}

	sessionOpen = true;

	return OCI_SUCCESS;
}

int OciSession::CloseSession()
{
	int rc;

	if ( sessionOpen == false ) {
		return OCI_SUCCESS;
	}

		// TODO: what about any open transactions?

	rc = OCISessionEnd( ociSvcCtxHndl, ociErrorHndl, NULL, OCI_DEFAULT );
	if ( rc != OCI_SUCCESS ) {
		return rc;
	}

	sessionOpen = false;

	return OCI_SUCCESS;
}

OciServer::OciServer( const char *db_name )
	: sessionsByUsername( HASH_TABLE_SIZE, hashFunction )
{
	initDone = false;
	dbName = strdup( db_name );
	ociServerHndl = NULL;
	ociErrorHndl = NULL;
	connectionOpen = false;
	idleDisconnectTid = TIMER_UNSET;
	activeSessions = new List<OciSession>();
}

OciServer::~OciServer()
{
	if ( idleDisconnectTid != TIMER_UNSET ) {
		daemonCore->Cancel_Timer( idleDisconnectTid );
	}
	if ( connectionOpen ) {
		ServerDisconnect();
	}
	if ( activeSessions != NULL ) {
		delete activeSessions;
	}
	if ( dbName ) {
		free( dbName );
	}
	if ( ociServerHndl != NULL ) {
		OCIHandleFree( ociServerHndl, OCI_HTYPE_SERVER );
	}
	if ( ociErrorHndl != NULL ) {
		OCIHandleFree( ociErrorHndl, OCI_HTYPE_ERROR );
	}
}

int OciServer::Initialize()
{
	int rc;

	if ( initDone ) {
		return OCI_SUCCESS;
	}

	if ( ociErrorHndl == NULL ) {
		rc = OCIHandleAlloc( GlobalOciEnvHndl, (dvoid**)&ociErrorHndl,
							 OCI_HTYPE_ERROR, 0, NULL );
		if ( rc != OCI_SUCCESS ) {
			return rc;
		}
	}

	if ( ociServerHndl == NULL ) {
		rc = OCIHandleAlloc( GlobalOciEnvHndl, (dvoid**)&ociServerHndl,
							 OCI_HTYPE_SERVER, 0, NULL );
		if ( rc != OCI_SUCCESS ) {
			return rc;
		}
	}

	initDone = true;

	return OCI_SUCCESS;
}

void OciServer::RegisterSession( OciSession *session, const char *username )
{
	OciSession *old_session;

	ASSERT(sessionsByUsername.lookup( HashKey(username), old_session ) == -1);

	sessionsByUsername.insert( HashKey(username), session );
}

void OciServer::UnregisterSession( OciSession *session, const char *username )
{
	if ( activeSessions->Delete( session ) ) {
		SessionInactive( session );
	}
	sessionsByUsername.remove( username );

	if ( sessionsByUsername.getNumElements() == 0 ) {
		this->~OciServer();
	}
}

OciSession *OciServer::FindSession( const char *username )
{
	OciSession *session = NULL;

	sessionsByUsername.lookup( HashKey( username ), session );

	return session;
}

int OciServer::SessionActive( OciSession *session, OCIServer *&svr_hndl,
							  OCIError *&err_hndl )
{
	int rc;

	rc = ServerConnect();
	if ( rc != OCI_SUCCESS ) {
		if ( rc == OCI_ERROR ) {
			err_hndl = ociErrorHndl;
		}
		return rc;
	}

		// TODO check if session is already in list?
	activeSessions->Append( session );

	if ( idleDisconnectTid != TIMER_UNSET ) {
		daemonCore->Cancel_Timer( idleDisconnectTid );
		idleDisconnectTid = TIMER_UNSET;
	}

	svr_hndl = ociServerHndl;

	return OCI_SUCCESS;
}

void OciServer::SessionInactive( OciSession *session )
{
	activeSessions->Delete( session );

	if ( activeSessions->IsEmpty() ) {
		idleDisconnectTid = daemonCore->Register_Timer( 5,
							(TimerHandlercpp)&OciServer::IdleDisconnectHandler,
							"OciServer::IdleDisconnectHandler", this );
	}
}

int OciServer::IdleDisconnectHandler()
{
	int rc;
	OciSession *session;

	idleDisconnectTid = TIMER_UNSET;

	if ( activeSessions->IsEmpty() == false ) {
		// There are active sesssions, abort the disconnect.
		return 0;
	}

	// Notify all of our sessions about we're about to close the connection
	// to the server.
	sessionsByUsername.startIterations();

	while ( sessionsByUsername.iterate( session ) != 0 ) {
		session->ServerConnectionClosing();
	}

	rc = ServerDisconnect();
	if ( rc != OCI_SUCCESS ) {
		// TODO can we do better?
		EXCEPT( "OCIServerDetach failed!\n" );
	}

	return 0;
}

int OciServer::ServerConnect()
{
	int rc;

	if ( connectionOpen ) {
		return OCI_SUCCESS;
	}

	rc = OCIServerAttach( ociServerHndl, ociErrorHndl, (OraText *)dbName, strlen(dbName),
						  OCI_DEFAULT );
	if ( rc != OCI_SUCCESS ) {
		return rc;
	}

	connectionOpen = true;

	return OCI_SUCCESS;
}

int OciServer::ServerDisconnect()
{
	int rc;

	if ( connectionOpen == false ) {
		return OCI_SUCCESS;
	}

	rc = OCIServerDetach( ociServerHndl, ociErrorHndl, OCI_DEFAULT );
	if ( rc != OCI_SUCCESS ) {
		return rc;
	}

	connectionOpen = false;

	return OCI_SUCCESS;
}
