/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


#include "condor_common.h"

#if !defined(SKIP_AUTHENTICATION) && defined(HAVE_EXT_GLOBUS)

#include "condor_auth_x509.h"
#include "authentication.h"
#include "condor_config.h"
#include "condor_string.h"
#include "CondorError.h"
#include "setenv.h"
#include "globus_utils.h"
#include "condor_gssapi_openssl.h"

#if defined(HAVE_EXT_VOMS)
extern "C" {
	#include "glite/security/voms/voms_apic.h"
}
#endif


const char STR_DAEMON_NAME_FORMAT[]="$$(FULL_HOST_NAME)";
StringList * getDaemonList(ReliSock * sock);


#ifdef WIN32
HashTable<MyString, MyString> * Condor_Auth_X509::GridMap = 0;
#endif

//----------------------------------------------------------------------
// Implementation
//----------------------------------------------------------------------

Condor_Auth_X509 :: Condor_Auth_X509(ReliSock * sock)
    : Condor_Auth_Base (sock, CAUTH_GSI),
      credential_handle(GSS_C_NO_CREDENTIAL),
      context_handle   (GSS_C_NO_CONTEXT),
      token_status     (0),
      ret_flags        (0)
{
#ifdef WIN32
	ParseMapFile();
#endif
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

int Condor_Auth_X509 :: authenticate(const char * /* remoteHost */, CondorError* errstack)
{
    int status = 1;
    int reply = 0;

    //don't just return TRUE if isAuthenticated() == TRUE, since 
    //we should BALANCE calls of Authenticate() on client/server side
    //just like end_of_message() calls must balance!
    
    if ( !authenticate_self_gss(errstack) ) {
        dprintf( D_SECURITY, "authenticate: user creds not established\n" );
        status = 0;
		// If I failed, notify the other side.
		if (mySock_->isClient()) {
			// Tell the other side, abort
			mySock_->encode();
			mySock_->code(status);
			mySock_->end_of_message();
		}
		else {
			// I am server, first wait for the other side
			mySock_->decode();
			mySock_->code(reply);
			mySock_->end_of_message();

			if (reply == 1) { 
				// The other side was okay, tell them the bad news
				mySock_->encode();
				mySock_->code(status);
				mySock_->end_of_message();
			}
		}
    }
    else {
		// wait to see if the other side is okay
		if (mySock_->isClient()) {
			// Tell the other side, that I am fine, then wait for answer
			mySock_->encode();
			mySock_->code(status);
			mySock_->end_of_message();

			mySock_->decode();
			mySock_->code(reply);
			mySock_->end_of_message();
			if (reply == 0) {   // The other side failed, abort
				errstack->push("GSI", GSI_ERR_REMOTE_SIDE_FAILED,
						"Failed to authenticate because the remote (server) "
						"side was not able to acquire its credentials.");

				return 0;
			}
		}
		else {
			// I am server, first wait for the other side
			mySock_->decode();
			mySock_->code(reply);
			mySock_->end_of_message();
			
			if (reply) {
				mySock_->encode();
				mySock_->code(status);
				mySock_->end_of_message();
			}
			else {
				errstack->push("GSI", GSI_ERR_REMOTE_SIDE_FAILED,
						"Failed to authenticate because the remote (client) "
						"side was not able to acquire its credentials.");
				return 0;  // The other side failed, abort
			}
		}

		int gsi_auth_timeout = param_integer("GSI_AUTHENTICATION_TIMEOUT",-1);
        int old_timeout=0;
		if (gsi_auth_timeout>=0) {
			old_timeout = mySock_->timeout(gsi_auth_timeout); 
		}
        
        switch ( mySock_->isClient() ) {
        case 1: 
            status = authenticate_client_gss(errstack);
            break;
        default: 
            status = authenticate_server_gss(errstack);
            break;
        }

		if (gsi_auth_timeout>=0) {
			mySock_->timeout(old_timeout); //put it back to what it was before
		}
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
        return FALSE;	
    
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

	// return TRUE on success
    return (major_status == GSS_S_COMPLETE);
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
        return FALSE;
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

	// return TRUE on success
    return (major_status == GSS_S_COMPLETE);
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
                                   int       token_stat, 
                                   char *    comment)
{
    char* buffer;
    globus_gss_assist_display_status_str(&buffer,
                                         comment,
                                         major_status,
                                         minor_status,
                                         token_stat);
    if (buffer){
        dprintf(D_ALWAYS,"%s\n",buffer);
        free(buffer);
    }
}

void  Condor_Auth_X509 :: erase_env()
{
	UnsetEnv( "X509_USER_PROXY" );
}

/* these next two functions are the implementation of a globus function that
   does not exist on the windows side.
   
   the return value and method of returning the dup'ed string are the same way
   globus behaves.
 
   if globus includes this function in a future version of GSI, delete
   these functions and use theirs.
*/
#ifdef WIN32
int Condor_Auth_X509::ParseMapFile() {
	FILE *fd;
	char * buffer;
	char * filename = param( STR_GSI_MAPFILE );

	if (!filename) {
		dprintf( D_ALWAYS, "GSI: GRIDMAP not set in condor_config, failing!\n");
		return FALSE;
	}

	if ( !(fd = safe_fopen_wrapper(  filename, "r" ) ) ) {
		dprintf( D_ALWAYS, "GSI: unable to open map file %s, errno %d\n", 
			filename, errno );
		free(filename);
		return FALSE;
	} else {

		dprintf (D_SECURITY, "GSI: parsing mapfile: %s\n", filename);
    
		if (GridMap) {
			delete GridMap;
			GridMap = NULL;
		}

		assert (GridMap == NULL);
		GridMap = new Grid_Map_t(293, MyStringHash);

		// parse the file
		while (buffer = getline(fd)) {

			// one_line will be a copy.  user_name will point to the username
			// within.  subject_name will point to the null-truncated subject
			// name within.  one_line should be free()ed.
			char *user_name = NULL;
			char *subject_name = NULL;
			char *one_line = (char*)malloc(1 + strlen(buffer));

			if (!one_line) {
				EXCEPT("out of mem.");
			}
			strcpy (one_line, buffer);

			// subject name is in quotes
			char *tmp;
			tmp = index(one_line, '"');
			if (tmp) {
				subject_name = tmp + 1;

				tmp = index(subject_name, '"');
				if (tmp) {
					// truncate the subject name
					*tmp = 0;

					// now, let's find the username
					tmp++;
					while (*tmp && (*tmp == ' ' || *tmp == '\t')) {
						tmp++;
					}
					if (*tmp) {
						// we found something.  the rest of the line
						// becomes username.
						user_name = tmp;
					} else {
						dprintf ( D_ALWAYS, "GSI: bad map (%s), no username: %s\n", filename, buffer);
					}

				} else {
					dprintf ( D_ALWAYS, "GSI: bad map (%s), missing quote after subject name: %s\n", filename, buffer);
				}
			} else {
				dprintf ( D_ALWAYS, "GSI: bad map (%s), need quotes around subject name: %s\n", filename, buffer);
			}


			// user_name is set only if we parsed the whole line
			if (user_name) {
				GridMap->insert( subject_name, user_name );
				dprintf ( D_SECURITY, "GSI: mapped %s to %s.\n",
						subject_name, user_name );
			}

			free (one_line);
		}

		fclose(fd);

		free(filename);
		return TRUE;
	}
}

/*
   this function must return the same values as globus!!!!

   also, it must allocate memory the same:  create a new string and
   write address into '*to'.
 
*/
int Condor_Auth_X509::condor_gss_assist_gridmap(const char * from, char ** to) {

	if (GridMap == 0) {
		ParseMapFile();
	}

	if (GridMap) {
		MyString f(from), t;
		if (GridMap->lookup(f, t) != -1) {
			if (DebugFlags & D_FULLDEBUG) {
				dprintf (D_SECURITY, "GSI: subject %s is mapped to user %s.\n", 
					f.Value(), t.Value());
			}

			*to = strdup(t.Value());
			return GSS_S_COMPLETE;
		} else {
			// if the map exists, they must be listed.  and they're NOT!
			return !GSS_S_COMPLETE;
		}
	}

	return !GSS_S_COMPLETE;
}
#endif


// ZKM
//
// the following function needs cleanup.  the mapping function should exist in
// this object, but it should just return the GSSClientname and not do the
// splitting and setRemoteUser() here.  see authentication.C
// map_authentication_name_to_canonical_name()

int Condor_Auth_X509::nameGssToLocal(const char * GSSClientname) 
{
	//this might need to change with SSLK5 stuff
	//just extract username from /CN=<username>@<domain,etc>
	OM_uint32 major_status;
	char *tmp_user = NULL;
	char local_user[256];

// windows gsi does not currently include this function.  we use it on
// unix, but implement our own on windows for now.
#ifdef WIN32
	major_status = condor_gss_assist_gridmap(GSSClientname, &tmp_user);
#else
	major_status = globus_gss_assist_gridmap((char*)GSSClientname, &tmp_user);
#endif

	if (tmp_user) {
		strcpy( local_user, tmp_user );
		free(tmp_user);
		tmp_user = NULL;
	}

	if ( major_status != GSS_S_COMPLETE) {
		setRemoteUser("gsi");
		setRemoteDomain( UNMAPPED_DOMAIN );
		return 0;
	}

	MyString user;
	MyString domain;
	Authentication::split_canonical_name( local_user, user, domain );
    
	setRemoteUser  (user.Value());
	setRemoteDomain(domain.Value());
	setAuthenticatedName(GSSClientname);
	return 1;
}

StringList * getDaemonList(ReliSock * sock)
{
    // Now, we substitu all $$FULL_HOST_NAME with actual host name, then
    // build a string list, then do a search to see if the target is 
    // in the list
    char * daemonNames = param( "GSI_DAEMON_NAME" );
    char * fqh         = sin_to_hostname(sock->peer_addr(), NULL);
    char * entry       = NULL;

	if (!daemonNames) {
		daemonNames = strdup("*");
	}

    StringList * original_names = new StringList(daemonNames, ",");
    StringList * expanded_names = new StringList(NULL, ",");

    original_names->rewind();
    while ( (entry=original_names->next())) {
        char *buf = NULL;
        char *tmp = strstr( entry, STR_DAEMON_NAME_FORMAT );
        if (tmp != NULL) { // we found the macro, now expand it
            char * rest = tmp + strlen(STR_DAEMON_NAME_FORMAT);
            int totalLen = strlen(entry) + strlen(fqh);

            // We have our macor, expand it into our host name
            buf = (char *) malloc(totalLen);
            memset(buf, 0, totalLen);

            // First, copy the part up to $$
            strncpy(buf, entry, strlen(entry) - strlen(tmp));
            tmp = buf + strlen(buf);

            // Next, copy the expanded host name
            strcpy(tmp, fqh);

            int remain = strlen(rest);
            // Now, copy the rest of the string if there is any
            if (remain != 0) {
                tmp = tmp + strlen(fqh);
                strcpy(tmp, rest);
            }

            expanded_names->insert(buf);
            free(buf);
        }
        else {
            // It's not using macro, let's just attach it to the list
            expanded_names->insert(entry);
        }
    }
    delete original_names;
    free(daemonNames);
    
    return expanded_names;
}

char * Condor_Auth_X509::get_server_info()
{
    OM_uint32	major_status = 0;
    OM_uint32	minor_status = 0;            
    OM_uint32   lifetime, flags;
    gss_name_t  target = NULL;
    gss_OID     mech, name_type;
    gss_buffer_desc name_buf;
    char *      server = NULL;
    
    // Now, we do some authorization work 
    major_status = gss_inquire_context(&minor_status,
                                       context_handle,
                                       NULL,    
                                       &target,
                                       &lifetime,
                                       &mech, 
                                       &flags, 
                                       NULL,
                                       NULL);
    if (major_status != GSS_S_COMPLETE) {
        dprintf(D_SECURITY, "Unable to obtain target principal name\n");
        return NULL;
    }
    else {
        major_status = gss_display_name(&minor_status,
                                             target,
                                             &name_buf,
                                             &name_type);
		gss_release_name( &minor_status, &target );
		if( major_status != GSS_S_COMPLETE) {
            dprintf(D_SECURITY, "Unable to convert target principal name\n");
            return NULL;
        }
        else {
            server = new char[name_buf.length+1];
            memset(server, 0, name_buf.length+1);
            memcpy(server, name_buf.value, name_buf.length);
			gss_release_buffer( &minor_status, &name_buf );
        }
    }
    return server;
}   

int Condor_Auth_X509::authenticate_self_gss(CondorError* errstack)
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
    
    //if ((!mySock_->isClient() && {
    if (isDaemon()) {
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

    //if (!mySock_->isClient() || isDaemon()) {
    if (isDaemon()) {
        set_priv(priv);
    }
    
    mySock_->timeout(time); //put it back to what it was before
    
    if (major_status != GSS_S_COMPLETE)
	{
		if (major_status == 851968 && minor_status == 20) {
			errstack->pushf("GSI", GSI_ERR_NO_VALID_PROXY,
				"Failed to authenticate.  Globus is reporting error (%i:%i).  "
				"This indicates that you do not have a valid user proxy.  "
				"Run grid-proxy-init.", major_status, minor_status);
		} else if (major_status == 851968 && minor_status == 12) {
			errstack->pushf("GSI", GSI_ERR_NO_VALID_PROXY,
				"Failed to authenticate.  Globus is reporting error (%i:%i).  "
				"This indicates that your user proxy has expired.  "
				"Run grid-proxy-init.", major_status, minor_status);
		} else {
			errstack->pushf("GSI", GSI_ERR_ACQUIRING_SELF_CREDINTIAL_FAILED,
				"Failed to authenticate.  Globus is reporting error (%i:%i).  There is probably a problem with your credentials.  (Did you run grid-proxy-init?)", major_status, minor_status);
		}

        sprintf(comment,"authenticate_self_gss: acquiring self credentials failed. Please check your Condor configuration file if this is a server process. Or the user environment variable if this is a user process. \n");
        print_log( major_status,minor_status,0,comment); 
        credential_handle = GSS_C_NO_CREDENTIAL; 
        return FALSE;
	}
    
    dprintf( D_FULLDEBUG, "This process has a valid certificate & key\n" );
    return TRUE;
}

int Condor_Auth_X509::authenticate_client_gss(CondorError* errstack)
{
    OM_uint32	major_status = 0;
    OM_uint32	minor_status = 0;
    int         status = 0;

    priv_state priv;
    
    if (isDaemon()) {
        priv = set_root_priv();
    }
    
    major_status = globus_gss_assist_init_sec_context(&minor_status,
                                                      credential_handle,
                                                      &context_handle,
                                                      "GSI-NO-TARGET",
                                                      GSS_C_MUTUAL_FLAG,
                                                      &ret_flags, 
                                                      &token_status,
                                                      relisock_gsi_get, 
                                                      (void *) mySock_,
                                                      relisock_gsi_put, 
                                                      (void *) mySock_
                                                      );
    
    if (isDaemon()) {
        set_priv(priv);
    }

    if (major_status != GSS_S_COMPLETE)	{
		if (major_status == 655360 && minor_status == 6) {
			errstack->pushf("GSI", GSI_ERR_AUTHENTICATION_FAILED,
				"Failed to authenticate.  Globus is reporting error (%i:%i).  "
				"This indicates that it was unable to find the issuer "
				"certificate for your credential", major_status, minor_status);
		} else if (major_status == 655360 && minor_status == 9) {
			errstack->pushf("GSI", GSI_ERR_AUTHENTICATION_FAILED,
				"Failed to authenticate.  Globus is reporting error (%i:%i).  "
				"This indicates that it was unable to verify the server's "
				"credential", major_status, minor_status);
		} else if (major_status == 655360 && minor_status == 11) {
			errstack->pushf("GSI", GSI_ERR_AUTHENTICATION_FAILED,
				"Failed to authenticate.  Globus is reporting error (%i:%i).  "
				"This indicates that it was unable verify the server's "
				"credentials because a signing policy file was not found or "
				"could not be read.", major_status, minor_status);
		} else {
			errstack->pushf("GSI", GSI_ERR_AUTHENTICATION_FAILED,
				"Failed to authenticate.  Globus is reporting error (%i:%i)",
				major_status, minor_status);
		}
        print_log(major_status,minor_status,token_status,
                  "Condor GSI authentication failure");
        // Following four lines of code is added to temporarily
        // resolve a bug (I belive so) in Globus's GSI code.
        // basically, if client calls init_sec_context with
        // mutual authentication and it returns with a mismatched
        // target principal, init_sec_context will return without
        // sending the server any token. The sever, therefore,
        // hangs on waiting for the token (or until the timeout
        // occurs). This code will force the server to break out
        // the loop.
        status = 0;
        mySock_->encode();
        mySock_->code(status);
        mySock_->end_of_message();
    }
    else {
        // Now, wait for final signal
        mySock_->decode();
        if (!mySock_->code(status) || !mySock_->end_of_message()) {
			errstack->push("GSI", GSI_ERR_COMMUNICATIONS_ERROR,
					"Failed to authenticate with server.  Unable to receive server status");
            dprintf(D_SECURITY, "Unable to receive final confirmation for GSI Authentication!\n");
        }
        if (status == 0) {
			errstack->push("GSI", GSI_ERR_AUTHENTICATION_FAILED,
				"Failed to get authorization from server.  Either the server "
				"does not trust your certificate, or you are not in the server's "
				"authorization file (grid-mapfile)");
            dprintf(D_SECURITY, "Server is unable to authorize my user name. Check the GRIDMAP file on the server side.\n");
            goto clear; 
        }

        char * server = get_server_info();

		// store the raw subject name for later mapping
		setAuthenticatedName(server);

        // Try to map DN to local name (in the format of name@domain)
        if ( !nameGssToLocal(server) ) {
			errstack->pushf("GSI", GSI_ERR_AUTHENTICATION_FAILED,
				"Failed to gss_assist_gridmap %s to a local user.  "
				"Check the grid-mapfile.", server );
			dprintf(D_SECURITY, "gss_assist_gridmap does not contain an entry for %s\n", server );
			setRemoteUser("gsi");
        }
        else {
            dprintf(D_SECURITY,"gss_assist_gridmap contains an entry for %s\n", 
                    server);
        }

		// extract and store VOMS attributes
		if (param_boolean("USE_VOMS_ATTRIBUTES", true)) {

			// get the voms attributes from the peer
			globus_gsi_cred_handle_t peer_cred = context_handle->peer_cred_handle->cred_handle;

			char * voms_fqan = NULL;
			int voms_err = extract_VOMS_info(peer_cred, 1, NULL, NULL, &voms_fqan);
			if (!voms_err) {
				setFQAN(voms_fqan);
				free(voms_fqan);
			} else {
				// complain!
				dprintf(D_SECURITY, "ZKM: VOMS FQAN not present (error %i), ignoring.\n", voms_err);
			}
		}

        StringList * daemonNames = getDaemonList(mySock_);

        // Now, let's see if the name is in the list, I am not using
        // anycase here, so if the host name and what we are looking for
        // are in different cases, then we will run into problems.
        status = daemonNames->contains_withwildcard(server) == TRUE? 1 : 0;

        if (status) {
            dprintf(D_SECURITY, "valid GSS connection established to %s\n", server);            
        }
        else {
			errstack->pushf("GSI", GSI_ERR_UNAUTHORIZED_SERVER,
					"Failed to authenticate because the subject '%s' is not currently trusted by you.  "
					"If it should be, add it to GSI_DAEMON_NAME in the condor_config, "
					"or use the environment variable override (check the manual).", server);
            dprintf(D_SECURITY, "The server %s is not specified in the GSI_DAEMON_NAME parameter\n", server);
        }

        mySock_->encode();
        if (!mySock_->code(status) || !mySock_->end_of_message()) {
			errstack->push("GSI", GSI_ERR_COMMUNICATIONS_ERROR,
					"Failed to authenticate with server.  Unable to send status");
            dprintf(D_SECURITY, "Unable to mutually authenticate with server!\n");
            status = 0;
        }

        delete [] server;
        delete daemonNames;
    }
 clear:
    return (status == 0) ? FALSE : TRUE;
}

int Condor_Auth_X509::authenticate_server_gss(CondorError* errstack)
{
    char *    GSSClientname;
    int       status = 0;
    OM_uint32 major_status = 0;
    OM_uint32 minor_status = 0;

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
                                                        relisock_gsi_get, 
                                                        (void *) mySock_,
                                                        relisock_gsi_put, 
                                                        (void *) mySock_
                                                        );
    
    set_priv(priv);
    
    if ( (major_status != GSS_S_COMPLETE)) {
		if (major_status == 655360) {
			errstack->pushf("GSI", GSI_ERR_AUTHENTICATION_FAILED,
				"COMMON Failed to authenticate (%i:%i)", major_status, minor_status);
		} else {
			errstack->pushf("GSI", GSI_ERR_AUTHENTICATION_FAILED,
				"Failed to authenticate.  Globus is reporting error (%i:%i)",
				major_status, minor_status);
		}
        print_log(major_status,minor_status,token_status, 
                  "Condor GSI authentication failure" );
    }
    else {
		// store the raw subject name for later mapping
		setAuthenticatedName(GSSClientname);
		if (param_boolean("USE_VOMS_ATTRIBUTES", true)) {

			// get the voms attributes from the peer
			globus_gsi_cred_handle_t peer_cred = context_handle->peer_cred_handle->cred_handle;

			char * voms_fqan = NULL;
			int voms_err = extract_VOMS_info(peer_cred, 1, NULL, NULL, &voms_fqan);
			if (!voms_err) {
				setFQAN(voms_fqan);
				free(voms_fqan);
			} else {
				// complain!
				dprintf(D_SECURITY, "ZKM: VOMS FQAN not present (error %i), ignoring.\n", voms_err);
			}
		}

        // Try to map DN to local name (in the format of name@domain)
        if ( (status = nameGssToLocal(GSSClientname) ) == 0) {
			errstack->pushf("GSI", GSI_ERR_AUTHENTICATION_FAILED,
				"Failed to gss_assist_gridmap %s to a local user.  "
				"Check the grid-mapfile.", GSSClientname);
			dprintf(D_SECURITY, "gss_assist_gridmap does not contain an entry for %s\n", GSSClientname);
        }
        else {
            dprintf(D_SECURITY,"gss_assist_gridmap contains an entry for %s\n", 
                    GSSClientname);
        }

		// XXX FIXME ZKM
		// i am making failure to be mapped a non-fatal error at this point.
		status = 1;

        mySock_->encode();
        if (!mySock_->code(status) || !mySock_->end_of_message()) {
			errstack->push("GSI", GSI_ERR_COMMUNICATIONS_ERROR,
				"Failed to authenticate with client.  Unable to send status");
            dprintf(D_SECURITY, "Unable to send final confirmation\n");
            status = 0;
        }

        if (status != 0) {
            // Now, see if client likes me or not
            mySock_->decode();
            if (!mySock_->code(status) || !mySock_->end_of_message()) {
				errstack->push("GSI", GSI_ERR_COMMUNICATIONS_ERROR,
					"Failed to authenticate with client.  Unable to receive status");
                dprintf(D_SECURITY, "Unable to receive client confirmation.\n");
                status = 0;
            }
            else {
                if (status == 0) {
					errstack->push("GSI", GSI_ERR_COMMUNICATIONS_ERROR,
						"Failed to authenticate with client.  Client does not trust our certificate.  "
						"You may want to check the GSI_DAEMON_NAME in the condor_config");
                    dprintf(D_SECURITY, "Client rejected my certificate. Please check the GSI_DAEMON_NAME parameter in Condor's config file.\n");
                }
            }
        }
        
        if (GSSClientname) {
            free(GSSClientname);
        }
    }
    return (status == 0) ? FALSE : TRUE;
}

void Condor_Auth_X509::setFQAN(MyString fqan) {
	dprintf (D_FULLDEBUG, "ZKM: setting FQAN: %s\n", fqan.Value());
	m_fqan = fqan;
}

MyString Condor_Auth_X509::getFQAN() {
	return m_fqan;
}

#endif

