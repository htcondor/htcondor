#include "X509credential.h"
#include "condor_common.h"
#include <stdio.h>
#include <stdlib.h>
#include "globus_utils.h"

X509Credential::X509Credential (const ClassAd& classad) : Credential (classad) {
  SetIntAttribute ("Type", X509_CREDENTIAL_TYPE);
}

X509Credential::X509Credential () {
    SetIntAttribute ("Type", X509_CREDENTIAL_TYPE);
}

X509Credential::X509Credential (const char * metadata) : Credential() {
    SetIntAttribute ("Type", X509_CREDENTIAL_TYPE);
}

X509Credential::X509Credential (const char * metadata, const void * buff, int buff_size) : Credential() {
  data = NULL;
  this->SetData(buff, buff_size);
  SetIntAttribute ("Type", X509_CREDENTIAL_TYPE);
}

X509Credential::~X509Credential() {
}

const char *
X509Credential::GetMyProxyServerDN() {
  char * buff = GetStringAttribute ("MyProxyServerDN");
  myproxy_server_dn = buff;
  free (buff);
  return myproxy_server_dn.GetCStr();
}


void
X509Credential::SetMyProxyServerDN(const char * dn) {
  myproxy_server_dn = dn;
  SetStringAttribute ("MyProxyServerDN", dn);
}


const char *
X509Credential::GetRefreshPassword() {
  char * buff = GetStringAttribute ("MyProxyPassword");
  myproxy_server_password = buff;
  free (buff);
  return myproxy_server_password.GetCStr();
}


void
X509Credential::SetRefreshPassword(const char * pwd) {
  myproxy_server_password = pwd;
  SetStringAttribute ("MyProxyPassword", pwd);
}

const char *
X509Credential::GetMyProxyServerHost() {
  char * buff = GetStringAttribute ("MyProxyServerHost");
  myproxy_server_host = buff;
  free (buff);
  return myproxy_server_host.GetCStr();
}

void
X509Credential::SetMyProxyServerHost(const char * host) {
  myproxy_server_host = host;
  SetStringAttribute ("MyProxyServerHost", host);
}

const char *
X509Credential::GetCredentialName() {
  char * buff = GetStringAttribute ("MyProxyCredentialName");
  myproxy_credential_name = buff;
  free (buff);
  return myproxy_credential_name.GetCStr();
}

void
X509Credential::SetCredentialName(const char * name) {
  myproxy_credential_name = name;
  SetStringAttribute ("MyProxyCredentialName", name);
}


const char *
X509Credential::GetMyProxyUser() {
  char * buff = GetStringAttribute ("MyProxyUser");
  myproxy_user = buff;
  free (buff);
  return myproxy_user.GetCStr();
}

void
X509Credential::SetMyProxyUser(const char * name) {
  myproxy_user = name;
  SetStringAttribute ("MyProxyUser", name);
}

/*
int
X509Credential::StoreData() {
  if (!data) {
    return FALSE;
  }

  const char * file_name = GetStorageName();

  int fd = open (file_name, O_WRONLY);
  if (fd == -1)
    return FALSE;

  int data_size = GetDataSize();

  write (fd, data, data_size);

  close (fd);

  printf ("Wrote data %d bytes\n", data_size);
  return TRUE;
}
*/

const char *
X509Credential::GetStorageName() {
  char * buff = GetStringAttribute ("StorageName");
  storage_name = buff;
  free (buff);
  return storage_name.GetCStr();
}

void
X509Credential::SetStorageName(const char * name) {
  storage_name = name;
  SetStringAttribute ("StorageName", name);
}

/*
int
X509Credential::LoadData() {
  const char * file_name = GetStorageName();
  int fd = open (file_name, O_RDONLY);
  if (fd == -1) {
    fprintf (stderr, "Can't open %s\n", file_name);
    return FALSE;
  }
  char buff [100000];
  int data_size = read (fd, buff, 100000);
  buff[data_size]='\0';

  close (fd);

  if (data_size <= 0)
    return FALSE;

  data = malloc (data_size);

  memcpy (data, buff, data_size);
  SetDataSize (data_size);

  return TRUE;
}
*/

time_t 
X509Credential::GetRealExpirationTime() {
  const char * file_name = GetStorageName();
  time_t exp_time = x509_proxy_expiration_time(file_name);
  return exp_time;
}

