#ifndef __CREDD_H__
#define __CREDD_H__
#include "condor_common.h"
#include "sock.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "stream.h"
#include "simplelist.h"
#include "credential.h"
#include "X509credentialWrapper.h"


int CheckCredentials ();

int LoadCredentialList ();
int SaveCredentialList ();

int StoreData (const char *, const void *, const int);
int LoadData (const char *, void *&, int &);

int
store_cred_handler(Service * service, int i, Stream *socket);

int
get_cred_handler(Service * service, int i, Stream *socket);

int
rm_cred_handler(Service * service, int i, Stream *socket);

int
query_cred_handler(Service * service, int i, Stream *stream);
 

int 
RefreshProxyThruMyProxy(X509CredentialWrapper * proxy);

int 
MyProxyGetDelegationReaper(Service *, int exitPid, int exitStatus);

bool
isSuperUser( const char* user );

void
Init();


int
init_user_id_from_FQN (const char * _fqn);


#endif
