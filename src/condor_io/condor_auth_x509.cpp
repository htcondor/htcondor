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

#if defined(HAVE_EXT_GLOBUS)

#include "condor_auth_x509.h"
#include "authentication.h"
#include "condor_config.h"
#include "CondorError.h"
#include "setenv.h"
#include "globus_utils.h"
#include "condor_gssapi_openssl.h"
#include "ipv6_hostname.h"
#include "condor_sinful.h"
#include "condor_attributes.h"

#if defined(HAVE_EXT_VOMS)
extern "C" {
	#include "voms/voms_apic.h"
}
#endif

#define USER_NAME_MAX 256

const char STR_DAEMON_NAME_FORMAT[]="$$(FULL_HOST_NAME)";
StringList * getDaemonList(char const *param_name,char const *fqh);


#ifdef WIN32
HashTable<std::string, std::string> * Condor_Auth_X509::GridMap = 0;
#endif

bool Condor_Auth_X509::m_globusActivated = false;

Condor_Auth_X509::GlobusMappingTable *Condor_Auth_X509::m_mapping = NULL;

//----------------------------------------------------------------------
// Implementation
//----------------------------------------------------------------------

Condor_Auth_X509 :: Condor_Auth_X509(ReliSock * sock)
    : Condor_Auth_Base (sock, CAUTH_GSI),
      credential_handle(GSS_C_NO_CREDENTIAL),
      context_handle   (GSS_C_NO_CONTEXT),
      m_gss_server_name(NULL),
      m_client_name(GSS_C_NO_NAME),
      token_status     (0),
      ret_flags        (0),
      m_state          (GetClientPre),
      m_status         (1)
{
#ifdef WIN32
	ParseMapFile();
#endif
	if ( !m_globusActivated ) {
		// The Globus callout module is a system-wide setting.  There are several
		// cases where a user may not want it to apply to Condor by default
		// (for example, if it causes crashes when mixed with Condor libs!).
		// Setting GSI_AUTHZ_CONF=/dev/null works for disabling the callouts.
		std::string gsi_authz_conf;
		if (param(gsi_authz_conf, "GSI_AUTHZ_CONF")) {
			if (setenv("GSI_AUTHZ_CONF", gsi_authz_conf.c_str(), 1)) {
				dprintf(D_ALWAYS, "Failed to set the GSI_AUTHZ_CONF environment variable.\n");
				EXCEPT("Failed to set the GSI_AUTHZ_CONF environment variable.");
			}
		}
		if ( activate_globus_gsi() < 0 ) {
			dprintf( D_ALWAYS, "Can't initialize GSI, authentication will fail: %s\n", x509_error_string() );
		} else {
			m_globusActivated = true;
		}
	}
}

Condor_Auth_X509 ::  ~Condor_Auth_X509()
{
	if ( !m_globusActivated ) {
		return;
	}
    // Delete context handle if exist

	OM_uint32 minor_status = 0;

    if (context_handle) {
        (*gss_delete_sec_context_ptr)(&minor_status,&context_handle,GSS_C_NO_BUFFER);
    }

    if (credential_handle != GSS_C_NO_CREDENTIAL) {
        (*gss_release_cred_ptr)(&minor_status, &credential_handle);
    }

	if( m_gss_server_name != NULL ) {
		(*gss_release_name_ptr)( &minor_status, &m_gss_server_name );
	}
	(*gss_release_name_ptr)(&minor_status, &m_client_name);
}

int Condor_Auth_X509 :: authenticate(const char * /* remoteHost */, CondorError* errstack, bool non_blocking)
{
    int status = 1;
    int reply = 0;
	token_status = 0;
	m_state = GetClientPre;

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
			if (!mySock_->code(status)) {
        		dprintf( D_SECURITY, "authenticate: and the remote side hung up on us.\n" );
			}
			mySock_->end_of_message();
		}
		else {
			// I am server, first wait for the other side
			mySock_->decode();
			if (!mySock_->code(reply)) {
        		dprintf( D_SECURITY, "authenticate: the client side hung up on us.\n" );
			}
			mySock_->end_of_message();

			if (reply == 1) { 
				// The other side was okay, tell them the bad news
				mySock_->encode();
				if (!mySock_->code(status)) {
					dprintf(D_SECURITY,"authenticate: the client hung up before authenticatiation\n");
				}
				mySock_->end_of_message();
			}
		}
    }
    else {
		// wait to see if the other side is okay
		if (mySock_->isClient()) {
			// Tell the other side, that I am fine, then wait for answer
			mySock_->encode();
			if (!mySock_->code(status)) {
				dprintf(D_SECURITY, "authenticate: the service hung up before authentication\n");
			}
			mySock_->end_of_message();

			mySock_->decode();
			if (!mySock_->code(reply)) {
				dprintf(D_SECURITY, "authenticate: the service hung up before authentication reply could be sent\n");
			}
			mySock_->end_of_message();
			if (reply == 0) {   // The other side failed, abort
				errstack->push("GSI", GSI_ERR_REMOTE_SIDE_FAILED,
						"Failed to authenticate because the remote (server) "
						"side was not able to acquire its credentials.");

				return 0;
			}
		}
		else {
			m_state = GetClientPre;
			CondorAuthX509Retval tmp_status = authenticate_server_pre(errstack, non_blocking);
			if ((tmp_status == Fail) || (tmp_status == WouldBlock)) {
				return static_cast<int>(tmp_status);
			}
		}

		int gsi_auth_timeout = param_integer("GSI_AUTHENTICATION_TIMEOUT",-1);
		int old_timeout=0;
		if (gsi_auth_timeout>=0) {
			old_timeout = mySock_->timeout(gsi_auth_timeout); 
		}
        
		if ( mySock_->isClient() ) {
			warn_on_gsi_usage();
			status = authenticate_client_gss(errstack);
		} else {
			CondorAuthX509Retval rc = authenticate_server_gss(errstack, non_blocking);
			if ( rc == Continue ) {
				rc = authenticate_server_gss_post( errstack, non_blocking );
			}
			status = static_cast<int>( rc );
		}

		if (gsi_auth_timeout>=0) {
			mySock_->timeout(old_timeout); //put it back to what it was before
		}
    }
    
    return( status );
}

int Condor_Auth_X509 :: wrap(const char*  data_in,
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
    
    if (!m_globusActivated || !isValid())
        return FALSE;	
    
    input_token->value  = (void *)const_cast<char *>(data_in);
    input_token->length = length_in;
    
    major_status = (*gss_wrap_ptr)(&minor_status,
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
    
int Condor_Auth_X509 :: unwrap(const char*  data_in,
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
    
    if (!m_globusActivated || !isValid()) {
        return FALSE;
    }
    
    input_token -> value = (void *)const_cast<char *>(data_in);
    input_token -> length = length_in;
    
    major_status = (*gss_unwrap_ptr)(&minor_status,
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

	if ( !m_globusActivated ) {
		return -1;
	}

	major_status = (*gss_context_time_ptr)(&minor_status ,
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
                                   const char *    comment)
{
    if ( !m_globusActivated ) {
        return;
    }
    char* buffer;
    char *tmp = (char *)malloc(strlen(comment)+1);
    strcpy(tmp, comment);
    (*globus_gss_assist_display_status_str_ptr)(&buffer,
                                         tmp,
                                         major_status,
                                         minor_status,
                                         token_stat);
    free(tmp);
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

	if ( !(fd = safe_fopen_wrapper_follow(  filename, "r" ) ) ) {
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
		GridMap = new Grid_Map_t(293, hashFunction);

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
		std::string f(from), t;
		if (GridMap->lookup(f, t) != -1) {
			if (IsDebugVerbose(D_SECURITY)) {
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
	OM_uint32 major_status = GSS_S_COMPLETE;
	char *tmp_user = NULL;
	char local_user[USER_NAME_MAX] = "";

// windows gsi does not currently include this function.  we use it on
// unix, but implement our own on windows for now.
#ifdef WIN32
	major_status = condor_gss_assist_gridmap(GSSClientname, &tmp_user);
#else
// Switched the unix map function to _map_and_authorize, which allows access
// to the Globus callout infrastructure.

	if (m_mapping == NULL) {
		// Size of hash table is purposely initialized small to prevent this
		// from hogging memory.  This will, of course, grow at large sites.
		m_mapping = new GlobusMappingTable(hashFunction);
	}
	const char *auth_name_to_map;
	const char *fqan = getFQAN();
	if (fqan && fqan[0]) {
		auth_name_to_map = fqan;
	}
	else {
		auth_name_to_map = GSSClientname;
	}

	globus_mapping_entry_ptr value;
	time_t now = 0;
	time_t gsi_cache_expiry = param_integer("GSS_ASSIST_GRIDMAP_CACHE_EXPIRATION", 0);
	if (gsi_cache_expiry && (m_mapping->lookup(auth_name_to_map, value) == 0)) {
		now = time(NULL);
		if (now < value->expiry_time) {
			dprintf(D_SECURITY, "Using Globus mapping result from the cache.\n");
			if (value->name.size()) {
				tmp_user = strdup(value->name.c_str());
			}
			else {
				major_status = GSS_S_FAILURE;
			}
		} else {
			m_mapping->remove(auth_name_to_map);
		}
	}

	if ( !m_globusActivated ) {
		major_status = GSS_S_FAILURE;
	}

	if ((tmp_user == NULL) && (major_status == GSS_S_COMPLETE)) {
		char condor_str[] = "condor";
		major_status = (*globus_gss_assist_map_and_authorize_ptr)(
			context_handle,
			condor_str, // Requested service name
			NULL, // Requested user name; NULL for non-specified
			local_user,
			USER_NAME_MAX-1); // Leave one space at end of buffer, just-in-case
		// Defensive programming: to protect against buffer overruns in the
		// unknown globus mapping module, make sure we are at least nul-term'd
		local_user[USER_NAME_MAX-1] = '\0';

		// More defensive programming: There is a bug in LCMAPS, (which is possibly
		// called by a globus callout) that sometimes returns with the euid set to
		// root (!?!).  As a safeguard, We check for that here and return to the
		// condor euid.  This is done "outside" of the condor priv stack since this
		// is essentially undoing a side effect of the library call, not
		// intentionally changing priv state.
		if (geteuid() == 0) {
			dprintf(D_ALWAYS, "WARNING: globus returned with euid 0\n");
			// attempt to undo
			if (seteuid(get_condor_uid())) {
				// complain loudly, but continue
				dprintf(D_ALWAYS, "ERROR: something has gone terribly wrong: errno %i\n", errno);
			}
		}

		if (now == 0) { now = time(NULL); }
		value.reset(new globus_mapping_entry_t);
		value->expiry_time = now + gsi_cache_expiry;
		// The special name of "" indicates failed mapping.
		if (major_status == GSS_S_COMPLETE) {
			value->name = local_user;
		}
		m_mapping->insert(auth_name_to_map, value);
	}
#endif

	if (tmp_user) {
		strncpy( local_user, tmp_user, USER_NAME_MAX-1 );
		local_user[USER_NAME_MAX-1] = '\0';
		free(tmp_user);
		tmp_user = NULL;
	}

	if ( major_status != GSS_S_COMPLETE) {
		setRemoteUser("gsi");
		setRemoteDomain( UNMAPPED_DOMAIN );
		return 0;
	}

	std::string user;
	std::string domain;
	Authentication::split_canonical_name( local_user, user, domain );
    
	setRemoteUser  (user.c_str());
	setRemoteDomain(domain.c_str());
	setAuthenticatedName(GSSClientname);
	return 1;
}

StringList * getDaemonList(char const *param_name,char const *fqh)
{
    // Now, we substitu all $$FULL_HOST_NAME with actual host name, then
    // build a string list, then do a search to see if the target is 
    // in the list
    char * daemonNames = param( param_name );
    char * entry       = NULL;

	if (!daemonNames) {
		return NULL;
	}

    StringList * original_names = new StringList(daemonNames, ",");
    StringList * expanded_names = new StringList(NULL, ",");

    original_names->rewind();
    while ( (entry=original_names->next())) {
        char *buf = NULL;
        char *tmp = strstr( entry, STR_DAEMON_NAME_FORMAT );
        if (tmp != NULL) { // we found the macro, now expand it
            char * rest = tmp + strlen(STR_DAEMON_NAME_FORMAT);
            int totalLen = strlen(entry) + strlen(fqh) + 1;

            // We have our macro, expand it into our host name
            buf = (char *) malloc(totalLen);
            memset(buf, 0, totalLen);

            // First, copy the part up to $$
            strcpy(buf, entry);
            tmp = buf + strlen(entry) - strlen(tmp);

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

bool Condor_Auth_X509::get_server_info(std::string &name, std::string &cred)
{
    OM_uint32	major_status = 0;
    OM_uint32	minor_status = 0;            
    OM_uint32   lifetime, flags;
    gss_OID     mech, name_type;
    gss_buffer_desc name_buf;

    if ( !m_globusActivated ) {
        return false;
    }

    // Now, we do some authorization work 
    major_status = (*gss_inquire_context_ptr)(&minor_status,
                                       context_handle,
                                       NULL,    
                                       &m_gss_server_name,
                                       &lifetime,
                                       &mech, 
                                       &flags, 
                                       NULL,
                                       NULL);
    if (major_status != GSS_S_COMPLETE) {
        dprintf(D_SECURITY, "Unable to obtain target principal name\n");
        return false;
    }

	major_status = (*gss_display_name_ptr)(&minor_status,
									m_gss_server_name,
									&name_buf,
									&name_type);
	if( major_status != GSS_S_COMPLETE) {
		dprintf(D_SECURITY, "Unable to convert target principal name\n");
		return false;
	}
	name = std::string(static_cast<char*>(name_buf.value), name_buf.length);
	(*gss_release_buffer_ptr)( &minor_status, &name_buf );

	globus_gsi_cred_handle_t peer_cred = context_handle->peer_cred_handle->cred_handle;
	X509 *cert_raw = NULL;
	auto ret = (*globus_gsi_cred_get_cert_ptr)(peer_cred, &cert_raw);
	if (ret != GLOBUS_SUCCESS) {
		return false;
	}
	std::unique_ptr<X509, decltype(&X509_free)> cert(cert_raw, X509_free);

	std::unique_ptr<BIO, decltype(&BIO_free)> bio(BIO_new( BIO_s_mem() ), BIO_free);

	if (!PEM_write_bio_X509(bio.get(), cert.get())) {
		return false;
	}
	char *pem_raw;
	auto len = BIO_get_mem_data(bio.get(), &pem_raw);
	if (len) {
		cred = std::string(pem_raw, len);
	}

	return true;
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

    if ( !m_globusActivated ) {
        errstack->push("GSI", GSI_ERR_ACQUIRING_SELF_CREDINTIAL_FAILED,
                       "Failed to load Globus libraries.");
        return FALSE;
    }
    
    // ensure all env vars are in place, acquire cred will fail otherwise 
    
    //use gss-assist to verify user (not connection)
    //this method will prompt for password if private key is encrypted!
    int time = mySock_->timeout(60 * 5);  //allow user 5 min to type passwd
    
    priv_state priv = PRIV_UNKNOWN;
    
    //if ((!mySock_->isClient() && {
    if (isDaemon()) {
        priv = set_root_priv();
    }
    
    major_status = (*globus_gss_assist_acquire_cred_ptr)(&minor_status,
                                                  GSS_C_BOTH, 
                                                  &credential_handle);
    if (major_status != GSS_S_COMPLETE) {
        major_status = (*globus_gss_assist_acquire_cred_ptr)(&minor_status,
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
				"Failed to authenticate.  Globus is reporting error (%u:%u).  "
				"This indicates that you do not have a valid user proxy.  "
				"Run grid-proxy-init.", (unsigned)major_status, (unsigned)minor_status);
		} else if (major_status == 851968 && minor_status == 12) {
			errstack->pushf("GSI", GSI_ERR_NO_VALID_PROXY,
				"Failed to authenticate.  Globus is reporting error (%u:%u).  "
				"This indicates that your user proxy has expired.  "
				"Run grid-proxy-init.", (unsigned)major_status, (unsigned)minor_status);
		} else {
			errstack->pushf("GSI", GSI_ERR_ACQUIRING_SELF_CREDINTIAL_FAILED,
				"Failed to authenticate.  Globus is reporting error (%u:%u).  There is probably a problem with your credentials.  (Did you run grid-proxy-init?)", (unsigned)major_status, (unsigned)minor_status);
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

    if ( !m_globusActivated ) {
        errstack->push("GSI", GSI_ERR_AUTHENTICATION_FAILED,
                       "Failed to load Globus libraries.");
        return FALSE;
    }

    priv_state priv = PRIV_UNKNOWN;
    
    if (isDaemon()) {
        priv = set_root_priv();
    }
    
	// NOTE: If a target_str other than "GSI-NO-TARGET" is used, then
	//   this function may return failure while leaving the server
	//   waiting for data. The most common why for this to happen is
	//   if the server's subject name doesn't match the expected target.
	//   The anticipated action for the client is to close the
	//   connection, it seems. Otherwise, the two sides will be out
	//   of synch.
    char target_str[] = "GSI-NO-TARGET";
    major_status = (*globus_gss_assist_init_sec_context_ptr)(&minor_status,
                                                      credential_handle,
                                                      &context_handle,
                                                      target_str,
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
				"Failed to authenticate.  Globus is reporting error (%u:%u).  "
				"This indicates that it was unable to find the issuer "
				"certificate for your credential", (unsigned)major_status, (unsigned)minor_status);
		} else if (major_status == 655360 && minor_status == 9) {
			errstack->pushf("GSI", GSI_ERR_AUTHENTICATION_FAILED,
				"Failed to authenticate.  Globus is reporting error (%u:%u).  "
				"This indicates that it was unable to verify the server's "
				"credential", (unsigned)major_status, (unsigned)minor_status);
		} else if (major_status == 655360 && minor_status == 11) {
			errstack->pushf("GSI", GSI_ERR_AUTHENTICATION_FAILED,
				"Failed to authenticate.  Globus is reporting error (%u:%u).  "
				"This indicates that it was unable verify the server's "
				"credentials because a signing policy file was not found or "
				"could not be read.", (unsigned)major_status, (unsigned)minor_status);
		} else {
			errstack->pushf("GSI", GSI_ERR_AUTHENTICATION_FAILED,
				"Failed to authenticate.  Globus is reporting error (%u:%u)",
				(unsigned)major_status, (unsigned)minor_status);
		}
        print_log(major_status,minor_status,token_status,
                  "Condor GSI authentication failure");
		// The following code is a workaround for a long-standing GSI
		// bug. In gss_init_sec_context() (called by
		// globus_gss_assist_init_sec_context()), in some situations
		// where the SSL handshake fails, it drops the last packet of
		// data to be sent to the server before returning failure.
		// This leaves the server waiting for data that won't be sent.
		// So here, we send an empty packet of data to the server.
		// Our heuristic for when to do this is if the last packet
		// exchanged was a "large" one received by the client.
		// Once GSI is fixed, this code won't cause any problems, but
		// can then be removed.
		if (mySock_->is_decode() && relisock_gsi_get_last_size > 100) {
			status = 0;
			mySock_->encode();
			if (!mySock_->code(status)) {
				dprintf(D_ALWAYS, "Authenticate: failed to inform client of failure to authenticate\n");
			}
			mySock_->end_of_message();
		}
    }
    else {
        // Now, wait for final signal
        mySock_->decode();
        if (!mySock_->code(status) || !mySock_->end_of_message()) {
			status = 0;
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

		std::string server, cred;
		if (!get_server_info(server, cred)) {
			errstack->push("GSI", GSI_ERR_AUTHENTICATION_FAILED,
				"Authentication to remote server appeared to succeed but we were unable"
				" to extract the remote side's name");
			dprintf(D_SECURITY, "Failed to extract a DN or hostcert from the remote server connection");
			status = 0;
			goto clear;
		}

		// store the raw subject name for later mapping
		setAuthenticatedName(server.c_str());

		// Default to user name "gsi@unmapped".
		// Later on, if configured, we will invoke the callout in nameGssToLocal.
		setRemoteUser("gsi");
		setRemoteDomain( UNMAPPED_DOMAIN );

		// extract and store VOMS attributes
		if (param_boolean("USE_VOMS_ATTRIBUTES", false)) {

			// get the voms attributes from the peer
			globus_gsi_cred_handle_t peer_cred_handle = context_handle->peer_cred_handle->cred_handle;
			X509* peer_cert = NULL;
			STACK_OF(X509)* peer_chain = NULL;
			(*globus_gsi_cred_get_cert_ptr)(peer_cred_handle, &peer_cert);
			(*globus_gsi_cred_get_cert_chain_ptr)(peer_cred_handle, &peer_chain);
			ASSERT(peer_cert);

			char * voms_fqan = NULL;
			int voms_err = extract_VOMS_info(peer_cert, peer_chain, 1, NULL, NULL, &voms_fqan);
			if (!voms_err) {
				setFQAN(voms_fqan);
				free(voms_fqan);
			} else {
				// complain!
				dprintf(D_SECURITY, "VOMS: VOMS FQAN not present (error %i), ignoring.\n", voms_err);
			}
			X509_free(peer_cert);
			if (peer_chain) {
				sk_X509_pop_free(peer_chain, X509_free);
			}
		}

        std::string fqh = get_full_hostname(mySock_->peer_addr());
        StringList * daemonNames = getDaemonList("GSI_DAEMON_NAME",fqh.c_str());

        // Now, let's see if the name is in the list, I am not using
        // anycase here, so if the host name and what we are looking for
        // are in different cases, then we will run into problems.
		if( daemonNames ) {
			status = daemonNames->contains_withwildcard(server.c_str()) == TRUE? 1 : 0;

			if( !status ) {
				errstack->pushf("GSI", GSI_ERR_UNAUTHORIZED_SERVER,
								"Failed to authenticate because the subject '%s' is not currently trusted by you.  "
								"If it should be, add it to GSI_DAEMON_NAME or undefine GSI_DAEMON_NAME.", server.c_str());
				dprintf(D_SECURITY,
						"GSI_DAEMON_NAME is defined and the server %s is not specified in the GSI_DAEMON_NAME parameter\n",
						server.c_str());
			}
		}
		else {
			status = CheckServerName(fqh.c_str(),mySock_->peer_ip_str(),mySock_,errstack);
		}

        if (status) {
            dprintf(D_SECURITY, "valid GSS connection established to %s\n", server.c_str());
        }

        mySock_->encode();
        if (!mySock_->code(status) || !mySock_->end_of_message()) {
			errstack->push("GSI", GSI_ERR_COMMUNICATIONS_ERROR,
					"Failed to authenticate with server.  Unable to send status");
            dprintf(D_SECURITY, "Unable to mutually authenticate with server!\n");
            status = 0;
        }

		if (!cred.empty()) {
			classad::ClassAd ad;
			ad.InsertAttr(ATTR_SERVER_PUBLIC_CERT, cred);
			mySock_->setPolicyAd(ad);
		}

        delete daemonNames;
    }
 clear:
    return (status == 0) ? FALSE : TRUE;
}

bool Condor_Auth_X509::CheckServerName(char const *fqh,char const *ip,ReliSock *sock,CondorError *errstack)
{
	if( param_boolean("GSI_SKIP_HOST_CHECK",false) ) {
		return true;
	}

    if ( !m_globusActivated ) {
        errstack->push("GSI", GSI_ERR_DNS_CHECK_ERROR,
                       "Failed to load Globus libraries.");
        return false;
    }

	char const *server_dn = getAuthenticatedName();
	if( !server_dn ) {
		std::string msg;
		formatstr(msg,"Failed to find certificate DN for server on GSI connection to %s",ip);
		errstack->push("GSI", GSI_ERR_DNS_CHECK_ERROR, msg.c_str());
		return false;
	}

	std::string skip_check_pattern;
	if( param(skip_check_pattern,"GSI_SKIP_HOST_CHECK_CERT_REGEX") ) {
		Regex re;
		int errcode = 0, erroffset=0;
		std::string full_pattern;
		formatstr(full_pattern,"^(%s)$",skip_check_pattern.c_str());
		if( !re.compile(full_pattern.c_str(),&errcode,&erroffset) ) {
			dprintf(D_ALWAYS,"GSI_SKIP_HOST_CHECK_CERT_REGEX is not a valid regular expression: %s\n",skip_check_pattern.c_str());
			return false;
		}
		if( re.match(server_dn,NULL) ) {
			return true;
		}
	}

	ASSERT( errstack );
	ASSERT( m_gss_server_name );
	ASSERT( ip );

	char const *connect_addr = sock->get_connect_addr();
	std::string alias_buf;
	if( connect_addr ) {
		Sinful s(connect_addr);
		char const *alias = s.getAlias();
		if( alias ) {
			dprintf(D_FULLDEBUG,"GSI host check: using host alias %s for %s %s\n",alias,fqh,sock->peer_ip_str());
			alias_buf = alias;
			fqh = alias_buf.c_str();
		}
	}

	if( !fqh || !fqh[0] ) {
		std::string msg;
		formatstr(msg,"Failed to look up server host address for GSI connection to server with IP %s and DN %s.  Is DNS correctly configured?  This server name check can be bypassed by making GSI_SKIP_HOST_CHECK_CERT_REGEX match the DN, or by disabling all hostname checks by setting GSI_SKIP_HOST_CHECK=true or defining GSI_DAEMON_NAME.",ip,server_dn);
		errstack->push("GSI", GSI_ERR_DNS_CHECK_ERROR, msg.c_str());
		return false;
	}

	std::string connect_name;
	gss_buffer_desc gss_connect_name_buf;
	gss_name_t gss_connect_name;
	OM_uint32 major_status = 0;
	OM_uint32 minor_status = 0;

	formatstr(connect_name,"%s/%s",fqh,sock->peer_ip_str());

	gss_connect_name_buf.value = strdup(connect_name.c_str());
	gss_connect_name_buf.length = connect_name.size()+1;

	major_status = (*gss_import_name_ptr)(&minor_status,
								   &gss_connect_name_buf,
								   *gss_nt_host_ip_ptr,
								   &gss_connect_name);

	free( gss_connect_name_buf.value );

	if( major_status != GSS_S_COMPLETE ) {
		std::string comment;
		formatstr(comment,"Failed to create gss connection name data structure for %s.\n",connect_name.c_str());
		print_log( major_status, minor_status, 0, comment.c_str() );
		return false;
	}

	int name_equal = 0;
	major_status = (*gss_compare_name_ptr)( &minor_status,
									 m_gss_server_name,
									 gss_connect_name,
									 &name_equal );

	(*gss_release_name_ptr)( &major_status, &gss_connect_name );

	if( !name_equal ) {
		std::string msg;
		formatstr(msg,"We are trying to connect to a daemon with certificate DN (%s), but the host name in the certificate does not match any DNS name associated with the host to which we are connecting (host name is '%s', IP is '%s', Condor connection address is '%s').  Check that DNS is correctly configured.  If the certificate is for a DNS alias, configure HOST_ALIAS in the daemon's configuration.  If you wish to use a daemon certificate that does not match the daemon's host name, make GSI_SKIP_HOST_CHECK_CERT_REGEX match the DN, or disable all host name checks by setting GSI_SKIP_HOST_CHECK=true or by defining GSI_DAEMON_NAME.\n",
				server_dn,
				fqh,
				ip,
				connect_addr ? connect_addr : sock->peer_description() );
		errstack->push("GSI", GSI_ERR_DNS_CHECK_ERROR, msg.c_str());
	}
	return name_equal != 0;
}

int Condor_Auth_X509::authenticate_continue(CondorError* errstack, bool non_blocking)
{
	int gsi_auth_timeout = param_integer("GSI_AUTHENTICATION_TIMEOUT",-1);
	int old_timeout=0;
	if (gsi_auth_timeout>=0) {
		old_timeout = mySock_->timeout(gsi_auth_timeout); 
	}       

	CondorAuthX509Retval retval = Continue;
	while (retval == Continue)
	{
		switch (m_state)
		{
		case GetClientPre:
			retval = authenticate_server_pre(errstack, non_blocking);
			break;
		case GSSAuth:
			retval = authenticate_server_gss(errstack, non_blocking);
			break;
		case GetClientPost:
			retval = authenticate_server_gss_post(errstack, non_blocking);
			break;
		default:
			retval = Fail;
			break;
		}
	}

	if (gsi_auth_timeout>=0) {
		mySock_->timeout(old_timeout); //put it back to what it was before
	}

	return static_cast<int>(retval);
}

Condor_Auth_X509::CondorAuthX509Retval
Condor_Auth_X509::authenticate_server_pre(CondorError* errstack, bool non_blocking)
{

	if (non_blocking && !mySock_->readReady())
	{
		dprintf(D_NETWORK, "Returning to DC as read would block in authenticate_server_pre\n");
		return WouldBlock;
	}
	m_status = 1;
	int reply = 0;

	mySock_->decode();
	if (!mySock_->code(reply)) {
		errstack->push("GSI", GSI_ERR_REMOTE_SIDE_FAILED, 
			"Failed to auth because we could not communicate with remote side\n");
	
		return Fail;
	}
	mySock_->end_of_message();

	if (reply) {
		mySock_->encode();
		if (!mySock_->code(m_status)) {
			errstack->push("GSI", GSI_ERR_REMOTE_SIDE_FAILED, 
			"Failed to auth because we could not read reply from remote side\n");
			return Fail;
		}
		mySock_->end_of_message();
	}
	else {
		errstack->push("GSI", GSI_ERR_REMOTE_SIDE_FAILED,
			"Failed to authenticate because the remote (client) "
			"side was not able to acquire its credentials.");
			return Fail;  // The other side failed, abort
	}
	warn_on_gsi_usage();
	m_state = GSSAuth;
	return Continue;
}

Condor_Auth_X509::CondorAuthX509Retval
Condor_Auth_X509::authenticate_server_gss(CondorError* errstack, bool non_blocking)
{
	OM_uint32 major_status = GSS_S_COMPLETE;
	OM_uint32 minor_status = 0;
	OM_uint32 minor_status2 = 0;

	OM_uint32				time_req;
	gss_buffer_desc			output_token_desc = GSS_C_EMPTY_BUFFER;
	gss_buffer_t			output_token = &output_token_desc;
	gss_buffer_desc         input_token_desc;
	gss_buffer_t            input_token;

    if ( !m_globusActivated ) {
        errstack->push("GSI", GSI_ERR_AUTHENTICATION_FAILED,
                       "Failed to load Globus libraries.");
        return Fail;
    }

	m_state = GSSAuth;
	do
	{
		if (non_blocking && !mySock_->readReady())
		{
			dprintf(D_NETWORK, "Returning to DC as read would block.\n");
			return WouldBlock;
		}

		input_token_desc.length = 0;
		input_token_desc.value = NULL;
		input_token = &input_token_desc;

		if ((token_status = relisock_gsi_get(
			mySock_,
			&input_token->value,
			&input_token->length)) != 0)
		{
			major_status =
				GSS_S_DEFECTIVE_TOKEN | GSS_S_CALL_INACCESSIBLE_READ;
			break;
		}

		dprintf(D_NETWORK, "gss_assist_accept_sec_context(1):inlen:%lu\n", static_cast<unsigned long>(input_token->length));

		major_status = (*gss_accept_sec_context_ptr)(
			&minor_status,
			&context_handle,
			credential_handle,
			input_token,
			GSS_C_NO_CHANNEL_BINDINGS,
			&m_client_name,
			NULL,
			output_token,
			&ret_flags,
			&time_req,
			NULL);

		dprintf(D_NETWORK, "gss_assist_accept_sec_context(2):"
			"maj:%8.8x:min:%8.8x:ret:%8.8x "
			"outlen:%lu:context:%p\n",
			(unsigned int) major_status,
			(unsigned int) minor_status,
			(unsigned int) ret_flags,
			output_token->length,
			context_handle);

		if (output_token->length != 0)
		{
			if ((token_status = relisock_gsi_put(
				mySock_,
				output_token->value,
				output_token->length)) != 0)
			{
				major_status =
				GSS_S_DEFECTIVE_TOKEN | GSS_S_CALL_INACCESSIBLE_WRITE;
			}
			(*gss_release_buffer_ptr)(&minor_status2, output_token);
		}
		if (GSS_ERROR(major_status))
		{
			if (context_handle != GSS_C_NO_CONTEXT)
			{
				(*gss_delete_sec_context_ptr)(&minor_status2, &context_handle, GSS_C_NO_BUFFER);
			}
			break;
		}

		if (input_token->length >0)
		{
			free(input_token->value);
			input_token->length = 0;
		}
	}
	while (major_status & GSS_S_CONTINUE_NEEDED);

	if (input_token->length >0)
	{
		free(input_token->value);
		input_token->length = 0;
	}

    m_status = 0;
    if ( (major_status != GSS_S_COMPLETE)) {
		if (major_status == 655360) {
			errstack->pushf("GSI", GSI_ERR_AUTHENTICATION_FAILED,
				"COMMON Failed to authenticate (%u:%u)", (unsigned)major_status, (unsigned)minor_status);
		} else {
			errstack->pushf("GSI", GSI_ERR_AUTHENTICATION_FAILED,
				"Failed to authenticate.  Globus is reporting error (%u:%u)",
				(unsigned)major_status, (unsigned)minor_status);
		}
        print_log(major_status,minor_status,token_status, 
                  "Condor GSI authentication failure" );
    }
    else {

		gss_buffer_desc                     tmp_buffer_desc = GSS_C_EMPTY_BUFFER;
		gss_buffer_t                        tmp_buffer = &tmp_buffer_desc;
		char * gss_name = NULL;
		major_status = (*gss_display_name_ptr)(&minor_status,
			m_client_name,
			tmp_buffer,
			NULL);
		if (major_status == GSS_S_COMPLETE)
		{
			gss_name = (char *)malloc(tmp_buffer->length+1);
			if (gss_name)
			{
				memcpy(gss_name, tmp_buffer->value, tmp_buffer->length);
				gss_name[tmp_buffer->length] = '\0';
			}
			else
			{
				errstack->pushf("GSI", GSI_ERR_AUTHENTICATION_FAILED, "Unable to allocate buffer");
				major_status = GSS_S_FAILURE;
			}
		}
		else
		{
			errstack->pushf("GSI", GSI_ERR_AUTHENTICATION_FAILED, "Unable to determine remote client name.  Globus is reporting error (%u:%u)",
				(unsigned)major_status, (unsigned)minor_status);
		}
		(*gss_release_buffer_ptr)(&minor_status2, tmp_buffer);

		classad::ClassAd ad;

		// store the raw subject name for later mapping
		if (gss_name) {
			setAuthenticatedName(gss_name);
			ad.InsertAttr(ATTR_X509_USER_PROXY_SUBJECT, gss_name);
			free(gss_name);
		}
		setRemoteUser("gsi");
		setRemoteDomain( UNMAPPED_DOMAIN );

		// get handle to cred so we can extract other attributes
		globus_gsi_cred_handle_t peer_cred_handle = context_handle->peer_cred_handle->cred_handle;
		X509* peer_cert = NULL;
		STACK_OF(X509)* peer_chain = NULL;
		(*globus_gsi_cred_get_cert_ptr)(peer_cred_handle, &peer_cert);
		(*globus_gsi_cred_get_cert_chain_ptr)(peer_cred_handle, &peer_chain);
		ASSERT(peer_cert);

		time_t expiration = x509_proxy_expiration_time(peer_cert, peer_chain);
		if (expiration != -1) {
			ad.InsertAttr(ATTR_X509_USER_PROXY_EXPIRATION, expiration);
		}

		char *email_name = x509_proxy_email(peer_cert, peer_chain);
		if (email_name)
		{
			ad.InsertAttr(ATTR_X509_USER_PROXY_EMAIL, email_name);
			free(email_name);
		}

		if (param_boolean("USE_VOMS_ATTRIBUTES", false)) {

			// get the voms attributes from the peer

			char * voname = NULL;
			char * firstfqan = NULL;
			char * voms_fqan = NULL;
			int voms_err = extract_VOMS_info(peer_cert, peer_chain, 1, &voname, &firstfqan, &voms_fqan);
			if (!voms_err) {
				setFQAN(voms_fqan);
				if (voms_fqan) {ad.InsertAttr(ATTR_X509_USER_PROXY_FQAN, voms_fqan);}
				free(voms_fqan);
				if (firstfqan) {ad.InsertAttr(ATTR_X509_USER_PROXY_FIRST_FQAN, firstfqan);}
				free(firstfqan);
				if (voname) {ad.InsertAttr(ATTR_X509_USER_PROXY_VONAME, voname);}
				free(voname);
			} else {
				// complain!
				dprintf(D_SECURITY, "VOMS: VOMS FQAN not present (error %i), ignoring.\n", voms_err);
			}
		}
		mySock_->setPolicyAd(ad);

		X509_free(peer_cert);
		if (peer_chain) {
			sk_X509_pop_free(peer_chain, X509_free);
		}

		// XXX FIXME ZKM
		// i am making failure to be mapped a non-fatal error at this point.
		m_status = (major_status == GSS_S_COMPLETE);

        mySock_->encode();
        if (!mySock_->code(m_status) || !mySock_->end_of_message()) {
			errstack->push("GSI", GSI_ERR_COMMUNICATIONS_ERROR,
				"Failed to authenticate with client.  Unable to send status");
            dprintf(D_SECURITY, "Unable to send final confirmation\n");
            m_status = 0;
        }
	}

	m_state = GetClientPost;
	return (m_status == 0) ? Fail : Continue;
}

Condor_Auth_X509::CondorAuthX509Retval
Condor_Auth_X509::authenticate_server_gss_post(CondorError* errstack, bool non_blocking)
{
	dprintf(D_FULLDEBUG, "Finishing authenticate_server_gss_post with status=%d\n", m_status);
	if (m_status != 0) {
		if (non_blocking && !mySock_->readReady()) {
			dprintf(D_NETWORK, "Returning to DC because read would block in authenticate_server_gss_post\n");
			return WouldBlock;
		}
		// Now, see if client likes me or not
		mySock_->decode();
		if (!mySock_->code(m_status) || !mySock_->end_of_message()) {
			errstack->push("GSI", GSI_ERR_COMMUNICATIONS_ERROR,
				"Failed to authenticate with client.  Unable to receive status");
			dprintf(D_SECURITY, "Unable to receive client confirmation.\n");
			m_status = 0;
		}
		else {
			if (m_status == 0) {
				errstack->push("GSI", GSI_ERR_COMMUNICATIONS_ERROR,
					"Failed to authenticate with client.  Client does not trust our certificate.  "
					"You may want to check the GSI_DAEMON_NAME in the condor_config");
				dprintf(D_SECURITY, "Client rejected my certificate. Please check the GSI_DAEMON_NAME parameter in Condor's config file.\n");
			}
		}
	}
	return (m_status == 0) ? Fail : Success;
}

void Condor_Auth_X509::setFQAN(const char *fqan) {
	dprintf (D_FULLDEBUG, "X509: setting FQAN: %s\n", fqan ? fqan : "");
	m_fqan = fqan ? fqan : "";
}

const char *Condor_Auth_X509::getFQAN() {
	return m_fqan.empty() ? NULL : m_fqan.c_str();
}

#endif

