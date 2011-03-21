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

#ifndef __X509CREDENTIAL_H__
#define __X509CREDENTIAL_H__
#include "credential.unix.h"

#define CREDATTR_MYPROXY_HOST "MyproxyHost"
#define CREDATTR_MYPROXY_DN "MyproxyDN"
#define CREDATTR_MYPROXY_PASSWORD "MyproxyPassword"
#define CREDATTR_MYPROXY_CRED_NAME "MyproxyCredName"
#define CREDATTR_MYPROXY_USER "MyproxyUser"
#define CREDATTR_EXPIRATION_TIME "ExpirationTime"

// The X509credential object is for client-side use of X509 credentials.
class X509Credential : public Credential {
public:

  X509Credential ();
  X509Credential (const classad::ClassAd&);
  virtual ~X509Credential();

    
  time_t GetRealExpirationTime();
  void SetRealExpirationTime (time_t t);

  const char * GetMyProxyServerDN();
  void SetMyProxyServerDN(const char*);

  const char * GetRefreshPassword();
  void SetRefreshPassword (const char *);

  const char * GetMyProxyServerHost();
  void SetMyProxyServerHost (const char * host);

  const char * GetCredentialName();
  void SetCredentialName (const char * name);

  void SetMyProxyUser (const char *);
  const char * GetMyProxyUser();

  virtual const char * GetTypeString() { return "X509proxy"; }

  virtual classad::ClassAd * GetMetadata();

  virtual void display( int debugflag );

 protected:
  MyString myproxy_server_host;
  MyString myproxy_server_dn;
  MyString myproxy_server_password;
  MyString myproxy_credential_name;
  MyString myproxy_user;

  int expiration_time;
};

#endif
