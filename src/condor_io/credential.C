/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#include "condor_common.h"
#include "authentication.h"
#include "environ.h"

#if !defined(SKIP_AUTHENTICATION)
#   include "condor_debug.h"
#   include "condor_string.h"
#   include "condor_config.h"
#   include "internet.h"
#   include "condor_uid.h"
#   include "my_username.h"
#   include "string_list.h"
#endif /* !defined(SKIP_AUTHENTICATION) */
#include "credential.h"
extern DLL_IMPORT_MAGIC char **environ;

#ifdef GSS_AUTHENTICATION

#define DEFAULT_MIN_TIME_LEFT 8*60*60;
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


 void  X509_Credential ::  erase_env()
 {
   int i,j;
   char *temp=NULL,*temp1=NULL;
   for (i=0;environ[i] != NULL;i++)
     {

       temp1 = (char*)strdup(environ[i]);
       if (!temp1)
	   	return;
       temp = (char*)strtok(temp1,"=");
       if (temp && !strcmp(temp, "X509_USER_PROXY" ))
	   		break;
    }
   for (j = i;environ[j] != NULL;j++)
	   environ[j] = environ[j+1];
   return;
 }

bool X509_Credential :: isvalid()
{
	int result = check_x509_proxy(crd_name);
	if (!result )
		return TRUE;
	else
		return FALSE;
}


int X509_Credential :: check_x509_proxy( char *proxy_file )
{
#if !defined(GSS_AUTHENTICATION)

	fprintf( stderr, "This version of Condor doesn't support X509 authentication!\n" );
	return 1;

#else

	char *min_time_left_param = NULL;
	int min_time_left;
	proxy_cred_desc *pcd = NULL;
	time_t time_after;
	time_t time_now;
	time_t time_diff;
	ASN1_UTCTIME *asn1_time = NULL;
	struct stat stx;

	/* initialize SSLeay and the error strings */
	ERR_load_prxyerr_strings(0);
	SSLeay_add_ssl_algorithms();

	/* Load proxy */
	if (!proxy_file) 
		proxy_get_filenames(1, NULL, NULL, &proxy_file, NULL, NULL);

	if (!proxy_file) {
		fprintf(stderr,"ERROR: unable to determine proxy file name\n");
		return 1;
	}

	if (stat(proxy_file,&stx) != 0) {
		fprintf(stderr, "ERROR: file %s not found\n",proxy_file);
		return 1;
	}

	pcd = proxy_cred_desc_new();
	if (!pcd) {
		fprintf(stderr,"ERROR: problem during internal initialization\n");
		return 1;
	}

	if (proxy_load_user_cert(pcd, proxy_file, NULL)) {
		fprintf(stderr,"ERROR: unable to load proxy");
		return 1;
	}

	if ((pcd->upkey = X509_get_pubkey(pcd->ucert)) == NULL) {
		fprintf(stderr,"ERROR: unable to load public key from proxy");
		return 1;
	}

	/* validity: set time_diff to time to expiration (in seconds) */
	asn1_time = ASN1_UTCTIME_new();
	X509_gmtime_adj(asn1_time,0);
	time_now = ASN1_UTCTIME_mktime(asn1_time);
	time_after = ASN1_UTCTIME_mktime(X509_get_notAfter(pcd->ucert));
	time_diff = time_after - time_now ;

	/* check validity */
	min_time_left_param = param( "CRED_MIN_TIME_LEFT" );

	if ( min_time_left_param != NULL ) {
		min_time_left = atoi( min_time_left_param );
	} else {
		min_time_left = DEFAULT_MIN_TIME_LEFT;
	}

	if ( time_diff < 0 ) {
		fprintf(stderr,"ERROR: proxy has expired\n");
		return 1;
	}

	if ( time_diff < min_time_left ) {
		dprintf(D_ALWAYS,"ERROR: proxy lifetime too short\n");
		return 1;
	}

	return 0;

#endif
}
#endif
