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

#ifndef ORACLERESOURCE_H
#define ORACLERESOURCE_H

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

#include "oci.h"

#include "oraclejob.h"


class OracleJob;
class OciSession;
class OciServer;

OciSession *GetOciSession( const char *db_name, const char *db_username,
						   const char *db_password );

class OciSession : public Service
{
 public:
	OciSession( OciServer *oci_server, const char *db_username,
				const char *db_password );
	~OciSession();

	int Initialize();

	void ServerConnectionClosing();

	void RegisterJob( OracleJob *job );
	void UnregisterJob( OracleJob *job );

	int AcquireSession( OracleJob *job, OCISvcCtx *&svc_hndl,
						OCIError *&err_hndl );
	int ReleaseSession( OracleJob *job );

	void RequestProbeJobs();

 private:
	int OpenSession( OCIError *&err_hndl );
	int CloseSession();

	int doProbeJobs();

	bool initDone;
	OciServer *server;
	char *username;
	char *password;
	OCISvcCtx *ociSvcCtxHndl;
	OCISession *ociSessionHndl;
	OCITrans *ociTransHndl;
	OCIError *ociErrorHndl;
		// We use a pointer to a List here because we want to put
		// OciSessions into Lists, which require operator=() to be defined,
		// but List itself doesn't have a usable operator=().
	List<OracleJob> *registeredJobs;
	bool sessionOpen;
	int lastProbeJobsTime;
	int probeJobsTid;
};

class OciServer : public Service
{
 public:
	OciServer( const char *db_name );
	~OciServer();

	int Initialize();

	void RegisterSession( OciSession *session, const char *username );
	void UnregisterSession( OciSession *session, const char *username );
	OciSession *FindSession( const char *username );

	int SessionActive( OciSession *session, OCIServer *&svr_hndl,
					   OCIError *&err_hndl );
	void SessionInactive( OciSession *session );

 private:
	int IdleDisconnectHandler();

	int ServerConnect();
	int ServerDisconnect();

	bool initDone;
	char *dbName;
	HashTable<HashKey, OciSession *> sessionsByUsername;
	List<OciSession> *activeSessions;
	OCIServer *ociServerHndl;
	OCIError *ociErrorHndl;
	bool connectionOpen;
	int idleDisconnectTid;
};

#endif
