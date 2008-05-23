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

#ifndef __X509CREDENTIALWRAPPER_H__
#define __X509CREDENTIALWRAPPER_H__
#include "X509credential.h"
#include "classad/classad_distribution.h"

#define CREDATTR_STORAGE_NAME "_StorageName"

class CredentialWrapper {
 public:
	CredentialWrapper();
	virtual ~CredentialWrapper();
	
	Credential * cred;

	virtual classad::ClassAd * GetMetadata();

	virtual const char * GetStorageName();
	virtual void SetStorageName (const char * name);

 protected:
	MyString storage_name;

};

// The X509credentialWrapper object is for server-side use of X509 credentials,
// and augments the X509credential objects with extra MyProxy get_delegation
// child process interfaces.
class X509CredentialWrapper : public CredentialWrapper {
public:

  X509CredentialWrapper (const classad::ClassAd&);
  virtual ~X509CredentialWrapper();

  // get-delegation-invocation details
#define GET_DELEGATION_START_TIME_NONE	0
  time_t get_delegation_proc_start_time;
#define GET_DELEGATION_PID_NONE	0
  int get_delegation_pid;
#define GET_DELEGATION_FD_NONE	(-1)
  int get_delegation_err_fd;
  char  * get_delegation_err_filename;
  char  * get_delegation_err_buff;
  int get_delegation_password_pipe[2]; // to send pwd via stdin
  void get_delegation_reset(void);

protected:
  void init(void);
  
};

#endif
