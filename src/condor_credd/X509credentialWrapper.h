#ifndef __X509CREDENTIALWRAPPER_H__
#define __X509CREDENTIALWRAPPER_H__
#include "X509credential.h"

class X509CredentialWrapper : public X509Credential {
public:

  X509CredentialWrapper (const classad::ClassAd&);
  virtual ~X509CredentialWrapper();

  // get-delegation-invocation details
  time_t get_delegation_proc_start_time;
  int get_delegation_pid;
  int get_delegation_err_fd;
  char  * get_delegation_err_filename;
  int get_delegation_password_pipe[2]; // to send pwd via stdin

  
 protected:
  
};

#endif
