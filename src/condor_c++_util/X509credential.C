#include "X509credential.h"
#include "condor_common.h"
#include "condor_debug.h"
#include "globus_utils.h"
#include "classad_distribution.h"

X509Credential::X509Credential (const classad::ClassAd& class_ad) : Credential (class_ad) {
	std::string val;

	type = X509_CREDENTIAL_TYPE;

	if (class_ad.EvaluateAttrString (CREDATTR_MYPROXY_HOST, val)) {
		myproxy_server_host=val.c_str();
	}

	if (class_ad.EvaluateAttrString (CREDATTR_MYPROXY_DN, val)) {
		myproxy_server_dn=val.c_str();
	}

	if (class_ad.EvaluateAttrString (CREDATTR_MYPROXY_PASSWORD, val)) {
		myproxy_server_password=val.c_str();
	}

	if (class_ad.EvaluateAttrString (CREDATTR_MYPROXY_CRED_NAME, val)) {
		myproxy_credential_name=val.c_str();
	}

	if (class_ad.EvaluateAttrString (CREDATTR_MYPROXY_USER, val)) {
		myproxy_user=val.c_str();
	}

	class_ad.EvaluateAttrInt (CREDATTR_EXPIRATION_TIME, expiration_time);
}

X509Credential::X509Credential () {
	type = X509_CREDENTIAL_TYPE;
}


X509Credential::~X509Credential() {
}

classad::ClassAd * 
X509Credential::GetMetadata() {
	classad::ClassAd * class_ad = Credential::GetMetadata();


	class_ad->InsertAttr (CREDATTR_MYPROXY_HOST, 
						 myproxy_server_host.Value());
	class_ad->InsertAttr (CREDATTR_MYPROXY_DN, 
						 myproxy_server_dn.Value());
	class_ad->InsertAttr (CREDATTR_MYPROXY_PASSWORD, 
						 myproxy_server_password.Value());
	class_ad->InsertAttr (CREDATTR_MYPROXY_CRED_NAME, 
						 myproxy_credential_name.Value());
	class_ad->InsertAttr (CREDATTR_MYPROXY_USER,
						 myproxy_user.Value());

	class_ad->InsertAttr (CREDATTR_EXPIRATION_TIME,
						 expiration_time);
	return class_ad;
}

const char *
X509Credential::GetMyProxyServerDN() {
	return myproxy_server_dn.Value();
}


void
X509Credential::SetMyProxyServerDN(const char * dn) {
	myproxy_server_dn = dn?dn:"";
}


const char *
X509Credential::GetRefreshPassword() {
	return myproxy_server_password.Value();
}


void
X509Credential::SetRefreshPassword(const char * pwd) {
  myproxy_server_password = pwd?pwd:"";
}

const char *
X509Credential::GetMyProxyServerHost() {
	return myproxy_server_host.Value();
}

void
X509Credential::SetMyProxyServerHost(const char * host) {
	myproxy_server_host = host?host:"";
}

const char *
X509Credential::GetCredentialName() {
	return myproxy_credential_name.Value();
}

void
X509Credential::SetCredentialName(const char * name) {
	myproxy_credential_name = name?name:"";
}


const char *
X509Credential::GetMyProxyUser() {
	return myproxy_user.Value();
}

void
X509Credential::SetMyProxyUser(const char * name) {
	myproxy_user = name?name:"";
}


time_t 
X509Credential::GetRealExpirationTime() {
	return expiration_time;

/*  time_t exp_time = x509_proxy_expiration_time(file_name);
	return exp_time;*/
}

void
X509Credential::SetRealExpirationTime (time_t t) {
	expiration_time = t;
}

void
X509Credential::display( int debugflag )
{
	time_t RealExpirationTime = GetRealExpirationTime();
	dprintf( debugflag, "X509Credential:\nexpires: %s",
			ctime( &RealExpirationTime ) );
	dprintf( debugflag, "MyProxyServerDN: '%s'\n",
			GetMyProxyServerDN() );
	dprintf( debugflag, "MyProxyServerHost: %s\n",
			GetMyProxyServerHost() );
	dprintf( debugflag, "CredentialName: %s MyProxyUser: %s\n",
			GetCredentialName(), GetMyProxyUser() );
}

