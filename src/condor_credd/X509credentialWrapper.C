#include "X509credentialWrapper.h"
#include "condor_common.h"

X509CredentialWrapper::X509CredentialWrapper (const classad::ClassAd& classad) : X509Credential (classad) {

  get_delegation_pid = 0;
  get_delegation_err_fd = -1;
  
}

X509CredentialWrapper::~X509CredentialWrapper() {
}

