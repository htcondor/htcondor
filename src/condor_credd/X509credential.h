#ifndef __X509CREDENTIAL_H__
#define __X509CREDENTIAL_H__
#include "credential.h"

// The X509credential object is for client-side use of X509 credentials.
class X509Credential : public Credential {
public:

  X509Credential ();
  X509Credential (const classad::ClassAd&);
  X509Credential (const char * metadata);
  X509Credential (const char * metadata, const void * buff, int size);
  virtual ~X509Credential();

  virtual time_t GetRealExpirationTime();


  const char * GetMyProxyServerDN();
  void SetMyProxyServerDN(const char*);

  const char * GetRefreshPassword();
  void SetRefreshPassword (const char *);

  const char * GetMyProxyServerHost();
  void SetMyProxyServerHost (const char * host);

  const char * GetCredentialName();
  void SetCredentialName (const char * name);

  void SetStorageName (const char *);
  const char * GetStorageName();

  void SetMyProxyUser (const char *);
  const char * GetMyProxyUser();

  virtual const char * GetTypeString() { return "X509proxy"; }

 protected:
  MyString storage_name;
  MyString myproxy_server_host;
  MyString myproxy_server_dn;
  MyString myproxy_server_password;
  MyString myproxy_credential_name;
  MyString myproxy_user;

};

#endif
