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
