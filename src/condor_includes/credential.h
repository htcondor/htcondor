#ifndef CREDENTIAL_H
#define CREDENTIAL_H

#ifdef GSS_AUTHENTICATION

#define SYSTEM 1
#define USER 2

#include "reli_sock.h"
#include "globus_gss_assist.h"
#include "sslutils.h"

enum Cred_status {
	ENV_NOT_SET = 1,
	CRED_NOT_SET,
	CRED_EXPIRED,
	CRED_SUCCESS
};

#define MAX_BUF_LENGTH 1024



typedef int CREDENTIAL_TYPE; 
typedef int Process_Type;

class X509_Credential{
	private :
		
		char *crd_name; //path to the credential file
		Process_Type my_type; //SYSTEM or USER
		Cred_status c_stat; //Valid or if failed why ?
		
	public :
			
		X509_Credential();
		X509_Credential(Process_Type,char*);
		X509_Credential(char*);
		int forward_credential(ReliSock*);
		int receive_credential(ReliSock*,char*);
		bool isvalid();
		int SetupEnv();
 		void erase_env();
		int check_x509_proxy( char* );
};


#endif

#endif /* CREDENTIAL_H */
