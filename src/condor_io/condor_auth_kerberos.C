/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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

#if !defined(SKIP_AUTHENTICATION) && defined(KERBEROS_AUTHENTICATION)

#include "condor_auth_kerberos.h"
#include "condor_config.h"
#include "condor_string.h"
#include "string_list.h"
#include "CondorError.h"

extern char* mySubSystem;

const char STR_KERBEROS_SERVER_KEYTAB[]   = "KERBEROS_SERVER_KEYTAB";
const char STR_KERBEROS_SERVER_PRINCIPAL[]= "KERBEROS_SERVER_PRINCIPAL";
const char STR_KERBEROS_SERVER_USER[]     = "KERBEROS_SERVER_USER";
const char STR_KERBEROS_SERVER_SERVICE[]  = "KERBEROS_SERVER_SERVICE";
const char STR_KERBEROS_CLIENT_KEYTAB[]   = "KERBEROS_CLIENT_KEYTAB";
const char STR_KRB_FORMAT[]             = "FILE:%s/krb_condor_%s.stash";
const char STR_DEFAULT_CONDOR_SERVICE[] = "host";

#define KERBEROS_ABORT   -1
#define KERBEROS_DENY    0
#define KERBEROS_GRANT   1
#define KERBEROS_FORWARD 2
#define KERBEROS_MUTUAL  3
#define KERBEROS_PROCEED 4

HashTable<MyString, MyString> * Condor_Auth_Kerberos::RealmMap = 0;
//----------------------------------------------------------------------
// Kerberos Implementation
//----------------------------------------------------------------------

Condor_Auth_Kerberos :: Condor_Auth_Kerberos( ReliSock * sock )
    : Condor_Auth_Base ( sock, CAUTH_KERBEROS ),
      krb_context_     ( NULL ),
      auth_context_    ( NULL ),
      krb_principal_   ( NULL ),
      ccname_          ( NULL ),
      defaultStash_    ( NULL ),
      keytabName_      ( NULL ),
      sessionKey_      ( NULL ),
      server_          ( NULL ),
      creds_           ( NULL )
{
}

Condor_Auth_Kerberos :: ~Condor_Auth_Kerberos()
{
    if (krb_context_) {

        if (auth_context_) {
            krb5_auth_con_free(krb_context_, auth_context_);
        }

        if (krb_principal_) {
            krb5_free_principal(krb_context_, krb_principal_);
        }
        
        if (sessionKey_) {
            krb5_free_keyblock(krb_context_, sessionKey_);
        }
        
        if (server_) {
            krb5_free_principal(krb_context_, server_);
        }
        
        krb5_free_context(krb_context_);
    }
    
    if (defaultStash_) {
        free(defaultStash_);
        defaultStash_ = NULL;
    }

    if (ccname_) {
        free(ccname_);
        ccname_ = NULL;
    }
}

int Condor_Auth_Kerberos :: authenticate(const char * remoteHost, CondorError* errstack)
{
    int status = 0;
    char * principal = NULL;

	if ( mySock_->isClient() ) {
		// we are the client.
		// initialize everything if needed.
		if (init_kerberos_context() && init_server_info()) {
            
			if (isDaemon() ||
				((strcmp(mySubSystem, "TOOL") != 0) &&
				(strcmp(mySubSystem, "SUBMIT") != 0))) {
				status = init_daemon();
			} else {
				status = init_user();
			}
		} else {
			status = FALSE;
		}

		int message = (status == TRUE? KERBEROS_PROCEED : KERBEROS_ABORT);

		mySock_->encode();
		mySock_->code(message);
		mySock_->end_of_message();
            
		if (message == KERBEROS_PROCEED) {
			// We are ready to go
			status = authenticate_client_kerberos();
		} else {
			status = FALSE;
		}
	} else {
		// we are the server.
		int ready;
		mySock_->decode();
		if (!mySock_->code(ready) || !mySock_->end_of_message()) {
			status = FALSE;
		} else {
			if (ready == KERBEROS_PROCEED) {
				dprintf(D_SECURITY,"About to authenticate client using Kerberos\n" );
				// initialize everything if needed.
				if (init_kerberos_context() && init_server_info()) {
					status = authenticate_server_kerberos();
				} else {
					status = FALSE;
				}
			}
		}
	}

    return( status );
}

int Condor_Auth_Kerberos :: wrap(char*  input, 
                                 int    input_len, 
                                 char*& output, 
                                 int&   output_len)
{
    krb5_error_code code;
    krb5_data       in_data, test;
    krb5_enc_data   out_data;
    krb5_pointer    ivec;
    int             index, tmp;

    // Make the input buffer
    in_data.length = input_len;
    in_data.data = input;
    memcpy(in_data.data, input, input_len);
    out_data.ciphertext.data = 0;
    
    if (code = krb5_encrypt_data(krb_context_, sessionKey_, 0, &in_data, &out_data)) {
        output     = 0;
        output_len = 0;
        if (out_data.ciphertext.data) {    
            free(out_data.ciphertext.data);
        }
        dprintf(D_ALWAYS, "%s\n", error_message(code));
        return false;
    }
    
    output_len = sizeof(out_data.enctype) +
        sizeof(out_data.kvno)    + 
        sizeof(out_data.ciphertext.length) +
        out_data.ciphertext.length;
    
    output = (char *) malloc(output_len);
    index = 0;
    tmp = htonl(out_data.enctype);
    memcpy(output + index, &tmp, sizeof(out_data.enctype));
    index += sizeof(out_data.enctype);

    tmp = htonl(out_data.kvno);
    memcpy(output + index, &tmp, sizeof(out_data.kvno));
    index += sizeof(out_data.kvno);

    tmp = htonl(out_data.ciphertext.length);
    memcpy(output + index, &tmp, sizeof(out_data.ciphertext.length));
    index += sizeof(out_data.ciphertext.length);

    memcpy(output + index, out_data.ciphertext.data, out_data.ciphertext.length);

    if (out_data.ciphertext.data) {    
        free(out_data.ciphertext.data);
    }
    
    return TRUE;
}

int Condor_Auth_Kerberos :: unwrap(char*  input, 
                                   int    input_len, 
                                   char*& output, 
                                   int&   output_len)
{
    krb5_error_code code;
    krb5_data       out_data;
    krb5_enc_data   enc_data;
    int index = 0, tmp;
    out_data.data = 0;

    memcpy(&tmp, input, sizeof(enc_data.enctype));
    enc_data.enctype = ntohl(tmp);
    index += sizeof(enc_data.enctype);

    memcpy(&tmp, input + index, sizeof(enc_data.kvno));
    enc_data.kvno = ntohl(tmp);
    index += sizeof(enc_data.kvno);

    memcpy(&tmp, input + index, sizeof(enc_data.ciphertext.length));
    enc_data.ciphertext.length = ntohl(tmp);
    index += sizeof(enc_data.ciphertext.length);
    
    enc_data.ciphertext.data = input + index;
    
    if (code = krb5_decrypt_data(krb_context_, 
                                 sessionKey_,
                                 0, 
                                 &enc_data, 
                                 &out_data)) {
        output_len = 0;
        output = 0;
        dprintf(D_ALWAYS, "%s\n", error_message(code));
        if (out_data.data) {
            free(out_data.data);
        }
        return false;
    }
    //fprintf(stderr, "Decrypted\n");
    
    output_len = out_data.length;
    output = (char *) malloc(output_len);
    memcpy(output, out_data.data, output_len);

    if (out_data.data) {
        free(out_data.data);
    }
    return true;
}

//----------------------------------------------------------------------
// Authenticate to server as a daemon
//----------------------------------------------------------------------
int Condor_Auth_Kerberos :: init_daemon()
{
    int            code, rc = TRUE;
    priv_state     priv;
    krb5_keytab    keytab = 0;
    char *         daemonPrincipal = 0;
    creds_ = (krb5_creds *) malloc(sizeof(krb5_creds));
    memset(creds_, 0, sizeof(krb5_creds));
    keytabName_ = param(STR_KERBEROS_SERVER_KEYTAB);
    
    //------------------------------------------
    // Initialize the principal for daemon (properly :-)
    //------------------------------------------
    daemonPrincipal = param(STR_KERBEROS_SERVER_PRINCIPAL);
    
	if (daemonPrincipal) {
		if ((code = krb5_parse_name(krb_context_, daemonPrincipal, &krb_principal_))) {
			free(daemonPrincipal);
			goto error;
		}
	} else {
		daemonPrincipal = param(STR_KERBEROS_SERVER_SERVICE);
		if (!daemonPrincipal) {
			daemonPrincipal = strdup(STR_DEFAULT_CONDOR_SERVICE);
		}

    	if ((code = krb5_sname_to_principal(krb_context_, 
                                        	NULL, 
                                        	daemonPrincipal,
                                        	KRB5_NT_SRV_HST, 
                                        	&krb_principal_))) {
			free(daemonPrincipal);
			goto error;
		}
	}
	free(daemonPrincipal);
	daemonPrincipal = 0;

    //------------------------------------------
    // Copy the principal information 
    //------------------------------------------
    krb5_copy_principal(krb_context_, krb_principal_, &(creds_->client));
    krb5_copy_principal(krb_context_, server_, &(creds_->server));
    
    dprintf(D_SECURITY, "Trying to get credential\n");
    
    if (keytabName_) {
        code = krb5_kt_resolve(krb_context_, keytabName_, &keytab);
    }
    else {
        code = krb5_kt_default(krb_context_, &keytab);
    }
    
    priv = set_root_priv();   // Get the old privilige
    
    if (code = krb5_get_in_tkt_with_keytab(krb_context_,
                                           0,
                                           NULL,      //adderss
                                           NULL,
                                           NULL,
                                           keytab,
                                           0,
                                           creds_,
                                           0)) {
        set_priv(priv);
        goto error;
    }
    
    set_priv(priv);
    
    dprintf(D_SECURITY, "Success..........................\n");
    
    rc = TRUE;

    goto cleanup;
    
 error:
    
    dprintf(D_ALWAYS, "AUTH_ERROR: %s\n", error_message(code));

    rc = FALSE;
    
 cleanup:
    
    if (keytab) {
        krb5_kt_close(krb_context_, keytab);   
    }
    
    return rc;
}

int Condor_Auth_Kerberos :: init_user()
{
    int             rc = TRUE;
    krb5_error_code code;
    krb5_ccache     ccache = (krb5_ccache) NULL;
    krb5_creds      mcreds;

    memset(&mcreds, 0, sizeof(mcreds));

    dprintf(D_SECURITY, "Acquiring credential for user\n");

    //------------------------------------------
    // First, try the default credential cache
    //------------------------------------------
    ccname_ = strdup(krb5_cc_default_name(krb_context_));
    
    if (code = krb5_cc_resolve(krb_context_, ccname_, &ccache)) {
        goto error;
    }
    
    //------------------------------------------
    // Get principal info
    //------------------------------------------
    if (code = krb5_cc_get_principal(krb_context_, ccache, &krb_principal_)) {
        goto error;
    }

    if (code = krb5_copy_principal(krb_context_,krb_principal_,&mcreds.client)){
        goto error;
    }
    
    if (code = krb5_copy_principal(krb_context_, server_, &mcreds.server)) {
        goto error;
    }
    
    if (code = krb5_get_credentials(krb_context_, 0, ccache, &mcreds, &creds_)) {
        goto error;
    }                                   
    
    dprintf(D_SECURITY, "Successfully located credential cache\n");
    
    rc = TRUE;
    goto cleanup;
    
 error:
    dprintf(D_ALWAYS, "%s\n", error_message(code));
    rc = FALSE;

 cleanup:

    krb5_free_cred_contents(krb_context_, &mcreds);

    if (ccache) {  // maybe should destroy this
        krb5_cc_close(krb_context_, ccache);
    }
    return rc;
}
    
int Condor_Auth_Kerberos :: authenticate_client_kerberos()
{
    krb5_error_code        code;
    krb5_flags             flags;
    krb5_data              request;
    krb5_error           * error = NULL;
    int                    reply, rc = FALSE;
    
    request.data = 0;
    request.length = 0;
    //------------------------------------------
    // Set up the flags
    //------------------------------------------
    flags = AP_OPTS_MUTUAL_REQUIRED | AP_OPTS_USE_SUBKEY;
    
    //------------------------------------------
    // Load local addresses
    //------------------------------------------
    if (creds_->addresses == NULL) {
        if (code = krb5_os_localaddr(krb_context_, &(creds_->addresses))) {
            goto error;
        }
    }
    
    //------------------------------------------
    // Let's create the KRB_AP_REQ message
    //------------------------------------------    
    if (code = krb5_mk_req_extended(krb_context_, 
                                    &auth_context_, 
                                    flags,
                                    0, 
                                    creds_, 
                                    &request)) {
        goto error;
    }
    
    // Send out the request
    if ((reply = send_request(&request)) != KERBEROS_MUTUAL) {
        dprintf(D_ALWAYS, "Could not authenticate!\n");
        return FALSE;
    }
    
    //------------------------------------------
    // Now, mutual authenticate
    //------------------------------------------
    reply = client_mutual_authenticate();

    switch (reply) 
        {
        case KERBEROS_DENY:
            dprintf(D_ALWAYS, "Authentication failed\n");
            return FALSE;
            break; // unreachable
        case KERBEROS_FORWARD:
            // We need to forward the credentials
            // We could do a fast forwarding (i.e stashing, if client/server
            // are located on the same machine. However, I want to keep the
            // forwarding mechanism clean, so, we use krb5_fwd_tgt_creds
            // regardless of where client/server are located
            
            // This is an implict GRANT
            //if (forward_tgt_creds(creds_, 0)) {
            //    dprintf(D_ALWAYS, "Unable to forward credentials\n");
            //return FALSE;  
            //            }
        case KERBEROS_GRANT:
            break; 
        default:
            dprintf(D_ALWAYS, "Response is invalid\n");
            break;
        }
    
    //------------------------------------------
    // Success, do some cleanup
    //------------------------------------------
    setRemoteAddress();
    
    //------------------------------------------
    // Store the session key for encryption
    //------------------------------------------
    if (code = krb5_copy_keyblock(krb_context_, &(creds_->keyblock), &sessionKey_)) {
        goto error;			  
    } 

    rc = TRUE;
    goto cleanup;
    
 error:
    dprintf(D_ALWAYS, "%s\n", error_message(code));
    // Abort
    mySock_->encode();
    reply = KERBEROS_ABORT;
    mySock_->code(reply);
    mySock_->end_of_message();

    rc = FALSE;
    
 cleanup:
    
    if (creds_) {
        krb5_free_creds(krb_context_, creds_);
    }
    
    if (request.data) {
        free(request.data);
    }
    
    return rc;
}
    
int Condor_Auth_Kerberos :: authenticate_server_kerberos()
{
    krb5_error_code   code;
    krb5_flags        flags = 0;
    krb5_data         request, reply;
    priv_state        priv;
    krb5_keytab       keytab = 0;
    int               time, message, rc = FALSE;
    krb5_ticket *     ticket = NULL;

    request.data = 0;
    reply.data   = 0;
    
    keytabName_ = param(STR_KERBEROS_SERVER_KEYTAB);

    //------------------------------------------
    // Getting keytab info
    //------------------------------------------
    if (keytabName_) {
        code = krb5_kt_resolve(krb_context_, keytabName_, &keytab);
    }
    else {
        code = krb5_kt_default(krb_context_, &keytab);
    }
    
    if (code) {
        dprintf(D_ALWAYS, "Kerberos server authentication error:%s\n", error_message(code));
        goto error;
    }
    
    //------------------------------------------
    // Get te KRB_AP_REQ message
    //------------------------------------------
    if(read_request(&request) == FALSE) {
        dprintf(D_ALWAYS, "Server is unable to read request\n");
        goto error;
    }
    
    dprintf(D_ALWAYS, "Reading request object\n");

    priv = set_root_priv();   // Get the old privilige
    
    if (code = krb5_rd_req(krb_context_,
                           &auth_context_,
                           &request,
                           krb_principal_,
                           keytab,
                           &flags,
                           &ticket)) {
        set_priv(priv);   // Reset
        dprintf(D_ALWAYS, "Kerberos server authentication error:%s\n", error_message(code));
        goto error;
    }
    set_priv(priv);   // Reset
    
    //------------------------------------------
    // See if mutual authentication is required
    //------------------------------------------
    if (flags & AP_OPTS_MUTUAL_REQUIRED) {
        if (code = krb5_mk_rep(krb_context_, auth_context_, &reply)) {
            dprintf(D_ALWAYS, "Kerberos server authentication error:%s\n", error_message(code));
            goto error;
        }

        mySock_->encode();
        message = KERBEROS_MUTUAL;
        if (!mySock_->code(message) || !mySock_->end_of_message()) {
            goto error;
        }

        // send the message
        if (send_request(&reply) != KERBEROS_GRANT) {
            goto cleanup;
        }
    }
    
    //------------------------------------------
    // extract client addresses
    //------------------------------------------
    if (ticket->enc_part2->caddrs) {
        struct in_addr in;
        memcpy(&(in.s_addr), ticket->enc_part2->caddrs[0]->contents, sizeof(in_addr));
        
        setRemoteHost(inet_ntoa(in));
    
        dprintf(D_SECURITY, "Client address is %s\n", getRemoteHost());
    }    

    // First, map the name, this has to take place before receive_tgt_creds!
    if (!map_kerberos_name(&(ticket->enc_part2->client))) {
        dprintf(D_SECURITY, "Unable to map Kerberos name\n");
        goto error;
    }

    // copy the session key
    if (code = krb5_copy_keyblock(krb_context_, 
                                  ticket->enc_part2->session, 
                                  &sessionKey_)){
        dprintf(D_SECURITY, "Kerberos server authentication error:%s\n", error_message(code));
        goto error;
    }
    
    // Next, see if we need client to forward the credential as well
    if (receive_tgt_creds(ticket)) {
        goto cleanup;
    }
    
    //------------------------------------------
    // We are now authenticated!
    //------------------------------------------
    dprintf(D_SECURITY, "User %s is now authenticated!\n", getRemoteUser());
    
    rc = TRUE;
    
    goto cleanup;
    
 error:
    message = KERBEROS_DENY;
    
    mySock_->encode();
    if ((!mySock_->code(message)) || (!mySock_->end_of_message())) {
        dprintf(D_ALWAYS, "Failed to send response message!\n");
    }
    
 cleanup:
    
    //------------------------------------------
    // Free up some stuff
    //------------------------------------------
    if (ticket) {
        krb5_free_ticket(krb_context_, ticket);
    }
    
    if (keytab) {
        krb5_kt_close(krb_context_, keytab);
    }
    //------------------------------------------
    // Free it for now, in the future, we might 
    // need this for future secure transctions.
    //------------------------------------------
    if (request.data) {
        free(request.data);
    }
    
    if (reply.data) {
        free(reply.data);
    }

    return rc;
}

//----------------------------------------------------------------------
// Mututal authentication
//----------------------------------------------------------------------
int Condor_Auth_Kerberos :: client_mutual_authenticate()
{
    krb5_ap_rep_enc_part * rep = NULL;
    krb5_error_code        code;
    krb5_data              request;
    int reply            = KERBEROS_DENY;
    int message;
    
    if (read_request(&request) == FALSE) {
        return KERBEROS_DENY;
    }
    
    if (code = krb5_rd_rep(krb_context_, auth_context_, &request, &rep)) {
        goto error;
    }
    
    if (rep) {
        krb5_free_ap_rep_enc_part(krb_context_, rep);
    }
    
    message = KERBEROS_GRANT;
    mySock_->encode();
    if (!(mySock_->code(message)) || !(mySock_->end_of_message())) {
        return KERBEROS_DENY;
    }
    
    mySock_->decode();
    if (!(mySock_->code(reply)) || !(mySock_->end_of_message())) {
        return KERBEROS_DENY;
    }
    
    free(request.data);
    
    return reply;
 error:
    free(request.data);
    
    dprintf(D_ALWAYS, "%s\n", error_message(code));
    return KERBEROS_DENY;
}

//----------------------------------------------------------------------
// Initialze some general structures for kerberos
//----------------------------------------------------------------------
int Condor_Auth_Kerberos :: init_kerberos_context()
{
    krb5_error_code code = 0;
    krb5_address  ** localAddr  = NULL;
    krb5_address  ** remoteAddr = NULL;

    // kerberos context_
    if (krb_context_ == NULL) {
        if (code = krb5_init_context(&krb_context_)) {
            goto error;
        }
    }

    if (code = krb5_auth_con_init(krb_context_, &auth_context_)) {
        goto error;
    }

    if (code = krb5_auth_con_setflags(krb_context_, 
                                      auth_context_, 
                                      KRB5_AUTH_CONTEXT_DO_SEQUENCE)) {
        goto error;
    }
        
    if (code = krb5_auth_con_genaddrs(krb_context_, 
                                      auth_context_, 
                                      mySock_->get_file_desc(),
                                      KRB5_AUTH_CONTEXT_GENERATE_LOCAL_FULL_ADDR|
                                      KRB5_AUTH_CONTEXT_GENERATE_REMOTE_FULL_ADDR
                                      )) {
        goto error;
    }

    if (code = krb5_auth_con_getaddrs(krb_context_, 
                                      auth_context_,
                                      localAddr, 
                                      remoteAddr)) {
        goto error;
    }

    // stash location
    defaultStash_ = param(STR_CONDOR_CACHE_DIR);

    if (defaultStash_ == NULL) {
        defaultStash_ = strdup(STR_DEFAULT_CONDOR_SPOOL);
    }
    
    return TRUE;
 error:
    dprintf(D_ALWAYS, "Unable to initialize kerberos: %s\n",error_message(code));
    return FALSE;
}

int Condor_Auth_Kerberos :: map_kerberos_name(krb5_principal * princ_to_map)
{
    krb5_error_code code;
    char *          client = NULL;
    int             rc = FALSE;

    //------------------------------------------
    // Decode the client name
    //------------------------------------------    
    if (code = krb5_unparse_name(krb_context_, 
                                 *princ_to_map, 
                                 &client)){
    	dprintf(D_ALWAYS, "%s\n", error_message(code));
		return FALSE;
    } 
    else {
		dprintf( D_SECURITY, "ZKM: krb5_unparse_name: %s\n", client );

		char * user = 0;
		char * at_sign = strchr(client, '@');

		// first, see if the principal up to the @ sign is the same as
		// STR_KERBEROS_SERVER_PRINCIPAL
		char * server_princ = param(STR_KERBEROS_SERVER_PRINCIPAL);
		if (server_princ) {
			dprintf ( D_SECURITY, "ZKM: param server princ: %s\n", server_princ );
			if (strcmp(client, server_princ) == 0) {
				user = param(STR_KERBEROS_SERVER_USER);
				if (user) {
					dprintf ( D_SECURITY, "ZKM: mapped to user: %s\n", user );
				}
			}
		}

		if (!user) {
			dprintf ( D_SECURITY, "ZKM: no user yet determined, will grab up to slash\n" );
			char * tmp;
			if ((tmp = strchr( client, '/')) == NULL) {
				tmp = at_sign;
			}
			int user_len = tmp - client;
			user = (char*) malloc( user_len + 1 );
			strncpy ( user, client, user_len );
			user[user_len] = '\0';
			dprintf ( D_SECURITY, "ZKM: picked user: %s\n", user );
		}

		char * service = 0;
		service = param(STR_KERBEROS_SERVER_SERVICE);
		if (!service) {
			service = strdup(STR_DEFAULT_CONDOR_SERVICE);
		}
		// hack for now - map the "host" user to the condor user
		if ((strcmp(user, service) == 0)) {
			free(user);
			user = param(STR_KERBEROS_SERVER_USER);
			if (!user) {
				user = strdup(STR_DEFAULT_CONDOR_USER);
			}
			dprintf ( D_SECURITY, "ZKM: remapping '%s' to '%s'\n", service, user );
		}
 		setRemoteUser(user);
		free(user);
		user = 0;
		free(service);
		service = 0;

		if (!map_domain_name(at_sign+1)) {
			return FALSE;
		}

		dprintf(D_SECURITY, "Client is %s@%s\n", getRemoteUser(), getRemoteDomain());
	}

	return TRUE;
}


int Condor_Auth_Kerberos :: map_domain_name(const char * domain)
{
    int rc = TRUE;
    int code;
    const char * localDomain;
    if (RealmMap == 0) {
        init_realm_mapping();
        // it's okay if it returns false
    }

    // two cases, if domain is the same as the current uid domain,
    // then we are okay, other wise, see if we have a map
    if (RealmMap) {
        MyString from(domain), to;
        if (RealmMap->lookup(from, to) != -1) {
			if (DebugFlags & D_FULLDEBUG) {
				dprintf (D_SECURITY, "KERBEROS: mapping realm %s to domain %s.\n", 
					from.Value(), to.Value());
			}
            setRemoteDomain(to.Value());
            return TRUE;
        } else {
			// if the map exists, they must be listed.  and they're NOT!
			return FALSE;
		}
    }

    // if there is no map, we just allow realm -> domain.
	if (DebugFlags & D_FULLDEBUG) {
		if (DebugFlags & D_FULLDEBUG) {
			dprintf (D_SECURITY, "KERBEROS: mapping realm %s to domain %s.\n", 
				domain, domain);
            setRemoteDomain(domain);
		}
	}
	return TRUE;

}

static int compute_string_hash(const MyString& str, int numBuckets)
{
	return ( str.Hash() % numBuckets );
}

int Condor_Auth_Kerberos :: init_realm_mapping()
{
    int lc = 0;
    FILE *fd;
    char * buffer;
    char * filename = param( "KERBEROS_MAP_FILE" );
    StringList from, to;

    if (RealmMap) {
        delete RealmMap;
		RealmMap = NULL;
    }

    if ( !(fd = fopen(  filename, "r" ) ) ) {
        dprintf( D_SECURITY, "unable to open map file %s, errno %d\n", 
                 filename, errno );
		free(filename);
        RealmMap = NULL;
		return FALSE;
    } else {
    
    	while (buffer = getline(fd)) {
        	char * token;
        	token = strtok(buffer, "= ");
        	if(token) {
				char *tmpf = strdup(token);

				token = strtok(NULL, "= ");
				if(token) {
					to.append(token);
					from.append(tmpf);
					lc++;
				} else {
					dprintf (D_ALWAYS, "KERBEROS: bad map (%s), no domain after '=': %s\n",
						filename, buffer);
				}

				free (tmpf);
			} else {
				dprintf (D_ALWAYS, "KERBEROS: bad map (%s), missing '=' separator: %s\n",
					filename, buffer);
			}
		}

		assert (RealmMap == NULL);
		RealmMap = new Realm_Map_t(lc, compute_string_hash);
		from.rewind();
		to.rewind();
		char *f, * t;
		while ( (f = from.next()) ) {
			t = to.next();
			RealmMap->insert(MyString(f), MyString(t));
			from.deleteCurrent();
			to.deleteCurrent();
		}
		fclose(fd);

		free(filename);
		return TRUE;
	}
}
   
int Condor_Auth_Kerberos :: send_request(krb5_data * request)
{
    int reply = KERBEROS_DENY;
    int message = KERBEROS_PROCEED;
    //------------------------------------------
    // Send the AP_REQ object
    //------------------------------------------
    mySock_->encode();
    
    if (!mySock_->code(message) || !mySock_->code(request->length)) {
        dprintf(D_SECURITY, "Faile to send request length\n");
        return reply;
    }
    
    if (!(mySock_->put_bytes(request->data, request->length)) ||
        !(mySock_->end_of_message())) {
        dprintf(D_SECURITY, "Faile to send request data\n");
        return reply;
    }

    //------------------------------------------
    // Next, wait for response
    //------------------------------------------
    mySock_->decode();
    
    if ((!mySock_->code(reply)) || (!mySock_->end_of_message())) {
        dprintf(D_SECURITY, "Failed to receive response from server\n");
        return KERBEROS_DENY;
    }// Resturn buffer size
    
    return reply;
}

int Condor_Auth_Kerberos :: init_server_info()
{
    // First, where to find cache
    //if (defaultStash_) {
    //  ccname_  = new char[MAXPATHLEN];
    //  sprintf(ccname_, STR_KRB_FORMAT, defaultStash_, my_username());
    //}
    //else {
    // Exception!
    //  dprintf(D_ALWAYS, "Unable to stash ticket -- STASH directory is not defined!\n");
    //}

    char * serverPrincipal = param(STR_KERBEROS_SERVER_PRINCIPAL);
    krb5_principal * server;

	if (mySock_->isClient()) {
		server = &server_;
	} else {
		server = &krb_principal_;
	}

    if (serverPrincipal) {
    	if (krb5_parse_name(krb_context_,serverPrincipal,server)) {
        	dprintf(D_SECURITY, "Failed to build server principal\n");
			free (serverPrincipal);
        	return 0;
		}
		free (serverPrincipal);
    } else {
		int  size;
		char *name = 0, *instance = 0;

		serverPrincipal = param(STR_KERBEROS_SERVER_SERVICE);
		if(!serverPrincipal) {
			serverPrincipal = strdup(STR_DEFAULT_CONDOR_SERVICE);
		}

		size = strlen(serverPrincipal);

		if ((instance = strchr( serverPrincipal, '/')) != NULL) {
			size = instance - serverPrincipal;
			instance += 1;
		}

		name = (char *) malloc(size + 1);
		memset(name, 0, size + 1);
		strncpy(name, serverPrincipal, size);

		if (mySock_->isClient()) {
			if (instance == 0) {
				struct hostent * hp;
				hp = gethostbyaddr((char *) &(mySock_->endpoint())->sin_addr, 
								   sizeof (struct in_addr),
								   mySock_->endpoint()->sin_family);
				instance = hp->h_name;
			}
		}

		//------------------------------------------
		// First, find out the principal mapping
		//------------------------------------------
		if (krb5_sname_to_principal(krb_context_,instance,name,KRB5_NT_SRV_HST,server)) {
			dprintf(D_SECURITY, "Failed to build server principal\n");
			free(name);
			free(serverPrincipal);
			return 0;
		}
		free(name);
		free(serverPrincipal);
	}

	if ( mySock_->isClient() && !map_kerberos_name( server ) ) {
		dprintf(D_SECURITY, "Failed to map principal to user\n");
		return 0;
	}

	char * tmp = 0;
    krb5_unparse_name(krb_context_, *server, &tmp);
	dprintf(D_SECURITY, "ZKM: Server principal is %s\n", tmp);
	free(tmp);

    return 1;
}


int Condor_Auth_Kerberos :: forward_tgt_creds(krb5_creds      * cred,
                                              krb5_ccache       ccache)
{
    krb5_error_code  code;
    krb5_data        request;
    int              message, rc = 1;
    struct hostent * hp;
    
    hp = gethostbyaddr((char *) &(mySock_->endpoint())->sin_addr, 		  
                       sizeof (struct in_addr), 
                       mySock_->endpoint()->sin_family);
    
    if (code = krb5_fwd_tgt_creds(krb_context_, 
                                  auth_context_,
                                  hp->h_name,
                                  cred->client, 
                                  cred->server,
                                  ccache, 
                                  KDC_OPT_FORWARDABLE,
                                  &request)) {
        goto error;
    }
    
    // Now, send it
    
    message = KERBEROS_FORWARD;
    mySock_->encode();
    if ((!mySock_->code(message)) || (!mySock_->end_of_message())) {
        dprintf(D_ALWAYS, "Failed to send response\n");
        goto cleanup;
    }
    
    rc = !(send_request(&request) == KERBEROS_GRANT);
    
    goto cleanup;
    
 error:
    dprintf(D_ALWAYS, "%s\n", error_message(code));
    
 cleanup:
    
    free(request.data);
    
    return rc;
}

int Condor_Auth_Kerberos :: receive_tgt_creds(krb5_ticket * ticket)
{
    krb5_error_code  code;
    krb5_ccache      ccache;
    krb5_principal   client;
    krb5_data        request;
    krb5_creds **    creds;
    char             defaultCCName[MAXPATHLEN+1];
    int              message;
    // First find out who we are talking to.
    // In case of host or condor, we do not need to receive credential
    // This is really ugly
    /*
    //
    //if (!((strncmp(claimToBe, "host/"  , strlen("host/")) == 0) || 
    //      (strncmp(claimToBe, "condor/", strlen("condor/")) == 0))) {
    //    if (defaultStash_) {
    //        sprintf(defaultCCName, STR_KRB_FORMAT, defaultStash_, claimToBe);
    //        dprintf(D_ALWAYS, "%s\n", defaultCCName);
    //  
    //        // First, check to see if we have a stash ticket
    //        if (code = krb5_cc_resolve(krb_context_, defaultCCName, &ccache)) {
      goto error;
      }
      
      // A very weak assumption that client == ticket->enc_part2->client
      // But this is what I am going to do right now
      if (code = krb5_cc_get_principal(krb_context_, ccache, &client)) {
      // We need use to forward credential
      message = KERBEROS_FORWARD;
      
      mySock_->encode();
      if ((!mySock_->code(message)) || (!mySock_->end_of_message())) {
      dprintf(D_ALWAYS, "Failed to send response\n");
      return 1;
      }
      
      // Expecting KERBEROS_FORWARD as well
	mySock_->decode();
	
	if ((!mySock_->code(message)) || (!mySock_->end_of_message())) {
	  dprintf(D_ALWAYS, "Failed to receive response\n");
	  return 1;
          }
          
          if (message != KERBEROS_FORWARD) {
	  dprintf(D_ALWAYS, "Client did not send forwarding message!\n");
	  return 1;
          }
          else {
	  if (!mySock_->code(request.length)) {
          dprintf(D_ALWAYS, "Credential length is invalid!\n");
          return 1;
	  }
	  
	  request.data = (char *) malloc(request.length);
	  
	  if ((!mySock_->get_bytes(request.data, request.length)) ||
          (!mySock_->end_of_message())) {
          dprintf(D_ALWAYS, "Credential is invalid!\n");
          return 1;
	  }
	  
          // Technically spearking, we have the credential now
	  if (code = krb5_rd_cred(krb_context_, 
          auth_context_, 
          &request, 
          &creds, 
          NULL)) {
          goto error;
	  }
	  
	  // Now, try to store it
	  if (code = krb5_cc_initialize(krb_context_, 
          ccache, 
          ticket->enc_part2->client)) {
          goto error;
	  }
	  
	  if (code = krb5_cc_store_cred(krb_context_, ccache, *creds)) {
          goto error;
	  }
	  
	  // free the stuff
	  krb5_free_creds(krb_context_, *creds);
	  
	  free(request.data);
          }
          }
          
          krb5_cc_close(krb_context_, ccache);
          
          }
          else {
          dprintf(D_ALWAYS, "Stash location is not set!\n");
          goto error;
          }
          }
    */
    //------------------------------------------
    // Tell the other side the good news
    //------------------------------------------
    message = KERBEROS_GRANT;
  
    mySock_->encode();
    if ((!mySock_->code(message)) || (!mySock_->end_of_message())) {
        dprintf(D_ALWAYS, "Failed to send response\n");
        return 1;
    }
    
    return 0;  // Everything is fine
  
 error:
    dprintf(D_ALWAYS, "%s\n", error_message(code));
    //if (ccache) {
    //  krb5_cc_destroy(krb_context_, ccache);
    //}
    
    return 1;
}
    
int Condor_Auth_Kerberos :: read_request(krb5_data * request)
{
    int code = TRUE, message;
    
    mySock_->decode();
    
    if (!mySock_->code(message)) {
        return FALSE;
    }

    if (message == KERBEROS_PROCEED) {
        if (!mySock_->code(request->length)) {
            dprintf(D_ALWAYS, "Incorrect message 1!\n");
            code = FALSE;
        }
        else {
            request->data = (char *) malloc(request->length);
            
            if ((!mySock_->get_bytes(request->data, request->length)) ||
                (!mySock_->end_of_message())) {
                dprintf(D_ALWAYS, "Incorrect message 2!\n");
                code = FALSE;
            }
        }
    }
    else {
        mySock_->end_of_message();
        code = FALSE;
    }
    
    return code;
}
    
//----------------------------------------------------------------------
// getRemoteHost -- retrieve remote host's address
//----------------------------------------------------------------------    
void Condor_Auth_Kerberos :: setRemoteAddress()
{
    krb5_error_code  code;
    krb5_address  ** localAddr  = NULL;
    krb5_address  ** remoteAddr = NULL;
    
    // Get remote host's address first
    
    if (code = krb5_auth_con_getaddrs(krb_context_, 
                                      auth_context_, 
                                      localAddr, 
                                      remoteAddr)) {
        goto error;
    }
    
    if (remoteAddr) {
        struct in_addr in;
        memcpy(&(in.s_addr), (*remoteAddr)[0].contents, sizeof(in_addr));
        setRemoteHost(inet_ntoa(in));
    }
    
    if (localAddr) {
        krb5_free_addresses(krb_context_, localAddr);
    }
    
    if (remoteAddr) {
        krb5_free_addresses(krb_context_, remoteAddr);
    }
    
    dprintf(D_SECURITY, "Remote host is %s\n", getRemoteHost());

    return;

 error:
    dprintf(D_ALWAYS, "Unable to obtain remote address: ", error_message(code));
}

int Condor_Auth_Kerberos :: endTime() const
{
    if (creds_) {
        return creds_->times.endtime;
    }
    else {
        return -1;
    }
}

int Condor_Auth_Kerberos :: isValid() const
{
    return auth_context_ != NULL;  // This is incorrect!
}

#endif



