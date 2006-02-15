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
#include "condor_common.h"
#include "authentication.h"
#include "condor_auth_x509.h"
#include "globus_utils.h"

#if !defined(SKIP_AUTHENTICATION)
#   include "condor_debug.h"
#   include "condor_string.h"
#   include "condor_config.h"
#   include "internet.h"
#   include "condor_uid.h"
#   include "my_username.h"
#endif /* !defined(SKIP_AUTHENTICATION) */
#include "credential.h"
extern DLL_IMPORT_MAGIC char **environ;

#ifdef X509_AUTHENTICATION

#define MAX_CRED_NAME 256

X509_Credential :: X509_Credential(){}

X509_Credential :: X509_Credential(char *name)
{
	X509_Credential(SYSTEM, name);
}	


X509_Credential :: X509_Credential(Process_Type type,char* name)
{
	int temp;
	if (name)
		crd_name=strdup(name);
	my_type = type;
}

int X509_Credential :: SetupEnv()
{
	char buffer[MAX_BUF_LENGTH];
	char *pbuf;

	if (my_type == SYSTEM){
		
	    erase_env();
		
    	pbuf = param( "X509_DIRECTORY" );
	   	sprintf( buffer, "%s=%s/certdir", STR_X509_CERT_DIR, pbuf );
	   	putenv( strdup( buffer ) );
		
		if (!pbuf) return -1;	
		
    	sprintf( buffer, "%s=%s/usercert.pem", STR_X509_USER_CERT, pbuf );
	   	putenv( strdup ( buffer ) );
		crd_name = strdup( buffer );

	    sprintf(buffer,"%s=%s/private/userkey.pem",STR_X509_USER_KEY, pbuf );
		putenv( strdup ( buffer  ) );
		
	    sprintf(buffer,"%s=%s/condor_ssl.cnf", STR_SSLEAY_CONF, pbuf );
    	putenv( strdup ( buffer ) );
	}
	else {
		pbuf = getenv(STR_X509_DIRECTORY );
		if (!pbuf)
			return -1;
		if (!getenv(STR_X509_CERT_DIR )){	
	    	sprintf( buffer, "%s=%s/certdir", STR_X509_CERT_DIR, pbuf );
	    	putenv(  strdup( buffer ) );
		}
	
		if (!getenv( STR_X509_USER_CERT )){	
    		sprintf( buffer, "%s=%s/usercert.pem", STR_X509_USER_CERT, pbuf );
	    	putenv( strdup ( buffer )  );
		}
    	
		if (!getenv( STR_X509_USER_KEY )){	
		    sprintf(buffer,"%s=%s/private/userkey.pem",STR_X509_USER_KEY, pbuf );
	   		putenv( strdup ( buffer )  );
		}
    
		if (!getenv(STR_SSLEAY_CONF)){	
		    sprintf(buffer,"%s=%s/condor_ssl.cnf", STR_SSLEAY_CONF, pbuf );
    		putenv( strdup ( buffer ) );
	  	}
		if (pbuf) 
			free( pbuf );
	}
  
	pbuf =  param("CONDOR_GATEKEEPER") ; 
	if (pbuf){
		sprintf(buffer ,"CONDOR_GATEKEEPER=%s",pbuf);
		putenv( strdup( buffer ) );
		free( pbuf );
	}
	else return -1;
	return 0;

		
}	

	
int X509_Credential :: forward_credential(ReliSock *sock)
{
	
	OM_uint32						  major_status = 0;
	OM_uint32						  minor_status = 0;
	struct stat buf;
	char *proxy_buffer;//read proxy file into buffer 
	int proxy_length;
	FILE *fp;
	size_t state;


	if (crd_name && !stat(crd_name,&buf)){
		fp = fopen(crd_name,"r");
		if (fp){
			proxy_length = buf.st_size;
			proxy_buffer = (char*) malloc(proxy_length);
			if (fread(proxy_buffer,proxy_length,1,fp) == 1){

				int time = sock->timeout(60 * 5); 	
				sock -> encode();
				if (sock -> encrypt(TRUE) == -1){
					dprintf(D_ALWAYS,"X509_Credential::forward_credential:
					   cannot set encrypt mode\n");
						return -1;
				}
				
				
				state = sock -> code( proxy_length );



				if ( state ) {
					if ( !(state = sock->code_bytes( proxy_buffer,  proxy_length ) ) ) {
						dprintf( D_ALWAYS, "failure sending data (%d bytes) over sock\n",proxy_length);
						sock -> end_of_message();
						sock -> timeout(time);	
						sock -> encrypt(FALSE);
						return -1;
					}
				}
				else {
					dprintf( D_ALWAYS, "failure sending size (%d) over sock\n", proxy_length );
					sock -> end_of_message();
					sock -> timeout(time);	
					sock -> encrypt(FALSE);
					return -1;
				}

				sock -> end_of_message();
				sock -> timeout(time);	
				sock -> encrypt(FALSE);
			}
			free(proxy_buffer);
			fclose(fp);
			return 0;
		}
		else 
			return -1;
	}
	else return -1;
}

int X509_Credential :: receive_credential(ReliSock *sock, char* name)
{
	OM_uint32 major_status,minor_status;
	int token_status;
	char *proxy_buffer,proxy_file[MAX_CRED_NAME];
	int proxy_length=0;
	FILE* fp;
	size_t stat;
	

	int time = sock->timeout(60 * 5); 	
	
	if (sock -> encrypt(TRUE) == -1){
		dprintf(D_ALWAYS,"X509_Credential :: receive_credential:
			cannot set encrypt mode on socket \n");
		return -1;
	}

	sock -> decode();
	stat = sock->code( proxy_length );


	if ( stat ){
		proxy_buffer =(char*) malloc(proxy_length);
		if (proxy_buffer){
			
			
			sock -> code_bytes(proxy_buffer, proxy_length);
		}
		else 
			stat = FALSE;
	}
	
	sock -> end_of_message();	
	sock -> timeout(time);	
	sock -> encrypt(FALSE);
	
	if (stat == FALSE ){
		dprintf(D_ALWAYS,"X509_Credential :: receive_credential :
			   Unable to recv packet\n");
		   	   
		return -1;
	}

		
   	char * where = strstr(name, "/CN=");
    where+=4;
	strcpy(proxy_file,param("SPOOL"));
	strcat(proxy_file,"/");
	strcat(proxy_file,where);
	strcat(proxy_file,".x509");
	fp = fopen(proxy_file,"w");
	if (fwrite(proxy_buffer,proxy_length,1,fp) == 1){
		fclose(fp);
		return 0;
	}
	else {
		dprintf(D_ALWAYS,"proxy_file could not be written\n");
		return -1;
	}

}


void  X509_Credential :: erase_env()
{
	int i,j;
	char *temp=NULL,*temp1=NULL;
	for (i=0;environ[i] != NULL;i++) {

		temp1 = (char*)strdup(environ[i]);
		if (!temp1) {
			return;
		}
		temp = (char*)strtok(temp1,"=");
		if (temp && !strcmp(temp, "X509_USER_PROXY" )) {
			break;
		}
	}
	for (j = i;environ[j] != NULL;j++) {
		environ[j] = environ[j+1];
	}
	return;
}

bool X509_Credential :: isvalid()
{
	int result = check_x509_proxy(crd_name);
	if (!result ) {
		return TRUE;
	} else {
		return FALSE;
	}
}

#endif
