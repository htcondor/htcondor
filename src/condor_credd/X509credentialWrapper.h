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
