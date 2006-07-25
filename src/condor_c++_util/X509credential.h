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
#ifndef __X509CREDENTIAL_H__
#define __X509CREDENTIAL_H__
#include "credential.h"

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
