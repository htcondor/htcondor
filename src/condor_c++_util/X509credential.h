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
