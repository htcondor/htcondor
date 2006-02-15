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
#ifndef __X509CREDENTIALWRAPPER_H__
#define __X509CREDENTIALWRAPPER_H__
#include "X509credential.h"
#include "classad_distribution.h"

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
