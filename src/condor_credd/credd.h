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

#ifndef __CREDD_H__
#define __CREDD_H__
#include "condor_common.h"
#include "sock.h"
#include "condor_daemon_core.h"
#include "stream.h"
#include "simplelist.h"
#include "credential.h"
#include "X509credentialWrapper.h"

void CheckCredentials ();

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
