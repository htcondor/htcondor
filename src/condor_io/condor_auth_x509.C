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

#if !defined(SKIP_AUTHENTICATION) && defined(X509_AUTHENTICATION)

#include "condor_auth_x509.h"
#include "sslutils.h"
#include "environ.h"
#include "condor_config.h"

extern DLL_IMPORT_MAGIC char **environ;

//----------------------------------------------------------------------
// Implementation
//----------------------------------------------------------------------

Condor_Auth_X509 :: Condor_Auth_X509(ReliSock * sock)
    : Condor_Auth_Base (sock, CAUTH_X509),
      context_handle   (GSS_C_NO_CONTEXT),
      credential_handle(GSS_C_NO_CREDENTIAL),
      ret_flags        (0)
{
    char buffer[1024];
    char *pbuf;
    
    memset(buffer, 0, 1024);
    
    if (!mySock_->isClient() || isDaemon()){
        erase_env();

        pbuf = param( STR_X509_DIRECTORY );
        if (pbuf) {
            sprintf( buffer, "%s=%s/certdir", STR_X509_CERT_DIR, pbuf);
            putenv( strdup( buffer ) );
	    
            sprintf( buffer, "%s=%s/usercert.pem", STR_X509_USER_CERT, pbuf);
            putenv( strdup ( buffer ) );
            
            sprintf(buffer,"%s=%s/private/userkey.pem",STR_X509_USER_KEY,pbuf);
            putenv( strdup ( buffer  ) );
            
            sprintf(buffer,"%s=%s/condor_ssl.cnf", STR_SSLEAY_CONF, pbuf);
            putenv( strdup ( buffer ) );
            
            free(pbuf);
        }

        pbuf = param( STR_X509_MAPFILE );
        if (pbuf) {
            sprintf( buffer, "%s=%s", STR_X509_MAPFILE, pbuf);
            putenv( strdup (buffer) );
            free(pbuf);
        }
    }	
    else{
        pbuf = getenv(STR_X509_DIRECTORY );   
        
        if (pbuf) {
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

	    //free( pbuf );
        }
    }
    
    pbuf = param("CONDOR_GATEKEEPER") ; 
    
    if (pbuf){
        sprintf(buffer ,"CONDOR_GATEKEEPER=%s",pbuf);
        putenv( buffer );
        free(pbuf);
    }
}

Condor_Auth_X509 ::  ~Condor_Auth_X509()
{
    // Delete context handle if exist

    if (context_handle) {
        OM_uint32 minor_status = 0;
        gss_delete_sec_context(&minor_status,&context_handle,GSS_C_NO_BUFFER);
    }

    if (credential_handle != GSS_C_NO_CREDENTIAL) {
        OM_uint32 major_status = 0; 
        gss_release_cred(&major_status, &credential_handle);
    }
}

int Condor_Auth_X509 :: authenticate(const char * remoteHost)
{
    int status = 1;
    
    //don't just return TRUE if isAuthenticated() == TRUE, since 
    //we should BALANCE calls of Authenticate() on client/server side
    //just like end_of_message() calls must balance!
    
    if ( !authenticate_self_gss() ) {
        dprintf( D_ALWAYS, "authenticate: user creds not established\n" );
        status = 0;
    }
    else {
        //temporarily change timeout to 5 minutes so the user can type passwd
        //MUST do this even on server side, since client side might call
        //authenticate_self_gss() while server is waiting on authenticate!!
        int time = mySock_->timeout(60 * 5); 
        
        switch ( mySock_->isClient() ) {
        case 1: 
            status = authenticate_client_gss();
            break;
        default: 
            status = authenticate_server_gss();
            break;
        }
        mySock_->timeout(time); //put it back to what it was before
    }
    
    return( status );
}

int Condor_Auth_X509 :: wrap(char*  data_in, 
                             int    length_in, 
                             char*& data_out, 
                             int&   length_out)
{
    OM_uint32 major_status;
    OM_uint32 minor_status;

    gss_buffer_desc input_token_desc  = GSS_C_EMPTY_BUFFER;
    gss_buffer_t    input_token       = &input_token_desc;
    gss_buffer_desc output_token_desc = GSS_C_EMPTY_BUFFER;
    gss_buffer_t    output_token      = &output_token_desc;
    
    if (!isValid())
        return GSS_S_FAILURE;	
    
    input_token->value  = (void *)data_in;
    input_token->length = length_in;
    
    major_status = gss_wrap(&minor_status,
                            context_handle,
                            0,
                            GSS_C_QOP_DEFAULT,
                            input_token,
                            NULL,
                            output_token);
    
    data_out = (char*)output_token -> value;
    length_out = output_token -> length;

    dprintf(D_ALWAYS,"Encrypting using X.509 %s --> %x\n", data_in, data_out);
    
    return major_status;
}
    
int Condor_Auth_X509 :: unwrap(char*  data_in, 
                               int    length_in, 
                               char*& data_out, 
                               int&   length_out)
{
    OM_uint32 major_status;
    OM_uint32 minor_status;
    
    gss_buffer_desc input_token_desc  = GSS_C_EMPTY_BUFFER;
    gss_buffer_t    input_token       = &input_token_desc;
    gss_buffer_desc output_token_desc = GSS_C_EMPTY_BUFFER;
    gss_buffer_t    output_token      = &output_token_desc;
    
    if (!isValid()) {
        return GSS_S_FAILURE;
    }
    
    input_token -> value = (void *)data_in;
    input_token -> length = length_in;
    
    major_status = gss_unwrap(&minor_status,
                              context_handle,
                              input_token,
                              output_token,
                              NULL,
                              NULL);
    
    
    data_out = (char*)output_token -> value;
    length_out = output_token -> length;

    dprintf(D_ALWAYS,"Decrypting using X.509 %x --> %s\n", data_in, data_out);
 
    return major_status;
}

int Condor_Auth_X509 :: endTime() const
{
    OM_uint32 major_status;
	OM_uint32 minor_status;
	OM_uint32 time_rec;
	
	major_status = gss_context_time(&minor_status ,
                                    context_handle ,
                                    &time_rec);

	if (!(major_status == GSS_S_COMPLETE)) {
        return -1;
	}

    return time_rec;
}

int Condor_Auth_X509 :: isValid() const
{
    return (endTime() != -1);
}	

void Condor_Auth_X509 :: print_log(OM_uint32 major_status,
                                   OM_uint32 minor_status,
                                   int       token_status, 
                                   char *    comment)
{
    char* buffer;
    globus_gss_assist_display_status_str(&buffer,
                                         comment,
                                         major_status,
                                         minor_status,
                                         token_status);
    if (buffer){
        dprintf(D_ALWAYS,"%s\n",buffer);
        free(buffer);
    }
}

int Condor_Auth_X509::get_user_x509name(char *proxy_file, char* name)
{

    proxy_cred_desc *     pcd = NULL;
    struct stat           stx; 
	/* initialize SSLeay and the error strings */
    ERR_load_prxyerr_strings(0);
    SSLeay_add_ssl_algorithms();

    pcd = proxy_cred_desc_new(); // Added. But not sure if it's correct. Hao

    if (!pcd) {
        dprintf(D_ALWAYS,"ERROR: unable to initialize globus\n");
        return 1;
    }

    /* Load proxy */
    if (!proxy_file) {
        proxy_get_filenames(pcd, 1, NULL, NULL, &proxy_file, NULL, NULL);
    }

    // potential memory leak below. Should change the code! Hao
    if (!proxy_file)
    {
        dprintf(D_ALWAYS,"ERROR: unable to determine proxy file name\n");
        return 1;
    }

    if (stat(proxy_file,&stx) != 0) {
    	return 1;
    }

    if (proxy_load_user_cert(pcd, proxy_file, NULL, NULL))
    {
    	dprintf(D_ALWAYS,"ERROR: unable to load proxy");
	    return 1;
    }

    name=X509_NAME_oneline(X509_get_subject_name(pcd->ucert),NULL,0);

    proxy_cred_desc_free(pcd);  

    return 0;
}

void  Condor_Auth_X509 :: erase_env()
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

int Condor_Auth_X509::nameGssToLocal(char * GSSClientname) 
{
    //this might need to change with SSLK5 stuff
    //just extract username from /CN=<username>@<domain,etc>
    OM_uint32 major_status;
    char *local;

    if ( (major_status = globus_gss_assist_gridmap(GSSClientname, &local)) 
         != GSS_S_COMPLETE) {
        dprintf(D_SECURITY, "Unable to map X509 user name %s to local id.\n", GSSClientname);
        return 0;
    }

    // we found a map, let's check it now
    // split it into user@domain
    char * tmp = strchr(local, '@');
    if (tmp == NULL) {
        dprintf(D_SECURITY, "X509 certificate map is invalid. Expect user@domain. Instead, the user id is %s\n", local);
        return 0;
    }
    
    int len  = strlen(local);
    int len2 = len - strlen(tmp);
    char * user = (char *) malloc(len2 + 1);
    memset(user, 0, len2 + 1);
    strncpy(user, local, len2);
    setRemoteUser  (user);
    setRemoteDomain(tmp+1);
    free(user);
    free(local);
    return 1;
}

//cannot make this an AuthSock method, since gss_assist method expects
//three parms, methods have hidden "this" parm first. Couldn't figure out
//a way around this, so made AuthSock have a member of type AuthComms
//to pass in to this method to manage buffer space.  //mju
int authsock_get(void *arg, void **bufp, size_t *sizep)
{
    /* globus code which calls this function expects 0/-1 return vals */
    
    //authsock must "hold onto" GSS state, pass in struct with comms stuff
    GSSComms *comms = (GSSComms *) arg;
    ReliSock *sock = comms->sock;
    size_t stat;
    
    sock->decode();
    
    //read size of data to read
    stat = sock->code( *((int *)sizep) );
    
    *bufp = malloc( *((int *)sizep) );
    if ( !*bufp ) {
        dprintf( D_ALWAYS, "malloc failure authsock_get\n" );
        stat = FALSE;
    }
    
    //if successfully read size and malloced, read data
    if ( stat ) {
        sock->code_bytes( *bufp, *((int *)sizep) );
    }
    
    sock->end_of_message();
    
    //check to ensure comms were successful
    if ( !stat ) {
        dprintf( D_ALWAYS, "authsock_get (read from socket) failure\n" );
        return -1;
    }
    return 0;
}

int authsock_put(void *arg,  void *buf, size_t size)
{
    //param is just a AS*
    ReliSock *sock = (ReliSock *) arg;
    int stat;
    
    sock->encode();
    
    //send size of data to send
    stat = sock->code( (int &)size );
    
    
    //if successful, send the data
    if ( stat ) {
        if ( !(stat = sock->code_bytes( buf, ((int) size )) ) ) {
            dprintf( D_ALWAYS, "failure sending data (%d bytes) over sock\n",size);
        }
    }
    else {
        dprintf( D_ALWAYS, "failure sending size (%d) over sock\n", size );
    }
    
    sock->end_of_message();
    
    //ensure data send was successful
    if ( !stat ) {
        dprintf( D_ALWAYS, "authsock_put (write to socket) failure\n" );
        return -1;
    }
    return 0;
}

int Condor_Auth_X509::authenticate_self_gss()
{
    OM_uint32 major_status;
    OM_uint32 minor_status;
    char comment[1024];
    if ( credential_handle != GSS_C_NO_CREDENTIAL ) { // user already auth'd 
        dprintf( D_FULLDEBUG, "This process has a valid certificate & key\n" );
        return TRUE;
    }
    
    // ensure all env vars are in place, acquire cred will fail otherwise 
    
    //use gss-assist to verify user (not connection)
    //this method will prompt for password if private key is encrypted!
    int time = mySock_->timeout(60 * 5);  //allow user 5 min to type passwd
    
    priv_state priv;
    
    if (!mySock_->isClient() || isDaemon()) {
        priv = set_root_priv();
    }
    
    major_status = globus_gss_assist_acquire_cred(&minor_status,
                                                  GSS_C_BOTH, 
                                                  &credential_handle);
    if (major_status != GSS_S_COMPLETE) {
        major_status = globus_gss_assist_acquire_cred(&minor_status,
                                                      GSS_C_BOTH,
                                                      &credential_handle);
    }

    if (!mySock_->isClient() || isDaemon()) {
        set_priv(priv);
    }
    
    mySock_->timeout(time); //put it back to what it was before
    
    if (major_status != GSS_S_COMPLETE)
	{
        sprintf(comment,"authenticate_self_gss: acquiring self 
				credentials failed \n");
        print_log( major_status,minor_status,0,comment); 
        credential_handle = GSS_C_NO_CREDENTIAL; 
        return FALSE;
	}
    
    dprintf( D_FULLDEBUG, "This process has a valid certificate & key\n" );
    return TRUE;
}

int Condor_Auth_X509::authenticate_client_gss()
{
    OM_uint32	major_status = 0;
    OM_uint32	minor_status = 0;
    GSSComms    authComms;
    int         status = 0;

    authComms.sock   = mySock_;
    authComms.buffer = NULL;
    authComms.size   = 0;
    
    priv_state priv;
    
    char *gateKeeper = param( "CONDOR_GATEKEEPER" );
    
    if ( !gateKeeper ) {
        dprintf( D_ALWAYS, "env var CONDOR_GATEKEEPER not set\n" );
        return FALSE;
    }
    
    //nameGssToLocal(gateKeeper);
    
    if (isDaemon()) {
        priv = set_root_priv();
    }
    
    major_status = globus_gss_assist_init_sec_context(&minor_status,
                                                      credential_handle,
                                                      &context_handle,
                                                      gateKeeper,
                                                      GSS_C_MUTUAL_FLAG,
                                                      &ret_flags, 
                                                      &token_status,
                                                      authsock_get, 
                                                      (void *) &authComms,
                                                      authsock_put, 
                                                      (void *) mySock_
                                                      );
    
    if (isDaemon()) {
        set_priv(priv);
    }
    
    if (major_status != GSS_S_COMPLETE)	{
        print_log(major_status,minor_status,token_status,
                  "Condor_Auth_X509 Failure on client side");
        free(gateKeeper);
        return FALSE;
    }
    
    dprintf(D_SECURITY, "valid GSS connection established to %s\n", gateKeeper);
    free(gateKeeper);

    //char      * proxy = NULL;
    //proxy = getenv(STR_X509_USER_PROXY);
    //my_credential = new X509_Credential(USER, proxy);
    //free(proxy);
    
    //if (!my_credential -> forward_credential(mySock_))
    //	dprintf(D_ALWAYS,"Successfully forwarded credentials\n");
    //delete my_credential;

    // Now, wait for final signal
    mySock_->decode();
    if (!mySock_->code(status) || !mySock_->end_of_message()) {
        dprintf(D_SECURITY, "Unable to receive final confirmation for GSS Authentication!\n");
        return FALSE;
    }
    if (status == 0) {
        dprintf(D_SECURITY, "Servier is unable to authorize my user name. Check the GRIDMAP file on the server side.\n");
        return FALSE;
    }
    
    
    return TRUE;
}

int Condor_Auth_X509::authenticate_server_gss()
{
    char *    GSSClientname;
    int	      token_status = 0;
    int       status = 0;
    OM_uint32 major_status = 0;
    OM_uint32 minor_status = 0;
    GSSComms  authComms;
    
    authComms.sock   = mySock_;
    authComms.buffer = NULL;
    authComms.size   = 0;
    
    priv_state priv;
    
    priv = set_root_priv();
    
    major_status = globus_gss_assist_accept_sec_context(&minor_status,
                                                        &context_handle, 
                                                        credential_handle,
                                                        &GSSClientname,
                                                        &ret_flags, NULL,
                                                        /* don't need user_to_user */
                                                        &token_status,
                                                        NULL,     /* don't delegate credential */
                                                        authsock_get, 
                                                        (void *) &authComms,
                                                        authsock_put, 
                                                        (void *) mySock_
                                                        );
    
    set_priv(priv);
    
    if ( (major_status != GSS_S_COMPLETE)) {
        print_log(major_status,minor_status,token_status,
                  "X509 GSS authentication failure on server side" );
        return FALSE;
    }
    
    // Try to map DN to local name (in the format of name@domain)
    if ( (status = nameGssToLocal(GSSClientname) ) == 0) {
        dprintf(D_SECURITY, "Could not map user's DN to local name.\n");
    }
    else {
        dprintf(D_SECURITY,"Valid GSS connection established to %s\n", 
                GSSClientname);
    }
        
    mySock_->encode();
    if (!mySock_->code(status) || !mySock_->end_of_message()) {
        dprintf(D_SECURITY, "Unable to send final confirmation\n");
        status = 0;
    }
    
    //my_credential = new X509_Credential(SYSTEM,NULL); 
    //if (my_credential -> 
    //		receive_credential(mySock_,GSSClientname)==0){
    //	dprintf(D_ALWAYS,"successfully received credentials\n");	
    //}
    //else {
    //  dprintf(D_ALWAYS,"unable to receive credentials\n");
    //}
    
    //delete my_credential;
    
    if (GSSClientname) {
        free(GSSClientname);
    }
    
    return (status == 0) ? FALSE : TRUE;
}

#endif





