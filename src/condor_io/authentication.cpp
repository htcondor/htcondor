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
#include "authentication.h"

#include "condor_auth.h"
#include "condor_auth_claim.h"
#include "condor_auth_anonymous.h"
#include "condor_auth_fs.h"
#include "condor_auth_munge.h"
#include "condor_auth_sspi.h"
#include "condor_auth_x509.h"
#include "condor_auth_ssl.h"
#include "condor_auth_kerberos.h"
#include "condor_auth_passwd.h"
#include "condor_secman.h"
#include "condor_environ.h"
#include "condor_ipverify.h"
#include "CondorError.h"
#include "globus_utils.h"



#if !defined(SKIP_AUTHENTICATION)
#   include "condor_debug.h"
#   include "condor_config.h"
#   include "string_list.h"
#include "MapFile.h"
#include "condor_daemon_core.h"
#endif /* !defined(SKIP_AUTHENTICATION) */


//----------------------------------------------------------------------

MapFile* Authentication::global_map_file = NULL;
bool Authentication::global_map_file_load_attempted = false;

char const *UNMAPPED_DOMAIN = "unmapped";
char const *MATCHSESSION_DOMAIN = "matchsession";
char const *UNAUTHENTICATED_FQU = "unauthenticated@unmapped";
char const *UNAUTHENTICATED_USER = "unauthenticated";
char const *EXECUTE_SIDE_MATCHSESSION_FQU = "execute-side@matchsession";
char const *SUBMIT_SIDE_MATCHSESSION_FQU = "submit-side@matchsession";
char const *CONDOR_CHILD_FQU = "condor@child";
char const *CONDOR_PARENT_FQU = "condor@parent";
char const *CONDOR_FAMILY_FQU = "condor@family";

Authentication::Authentication( ReliSock *sock )
	: m_auth(NULL),
	  m_key(NULL),
	  m_auth_timeout_time(0),
	  m_continue_handshake(false),
	  m_continue_auth(false)
{
// Do this regardless of the state of SKIP_AUTHENTICATION)
// even if SKIP_AUTHENTICATION is true, we call sock->Timeout later
	mySock              = sock;
	auth_status         = CAUTH_NONE;
	method_used         = NULL;
	authenticator_      = NULL;
}

Authentication::~Authentication()
{
#if !defined(SKIP_AUTHENTICATION)
	mySock = NULL;

	if (authenticator_)
	{
		delete authenticator_;
	}
	if (m_auth)
	{
		delete m_auth;
	}

	if (method_used) {
		free (method_used);
	}

#endif
}

int Authentication::authenticate( char *hostAddr, KeyInfo *& key, 
								  const char* auth_methods, CondorError* errstack, int timeout, bool non_blocking)
{
	m_key = &key;
	return authenticate(hostAddr, auth_methods, errstack, timeout, non_blocking);
}

int Authentication::authenticate( char *hostAddr, const char* auth_methods,
		CondorError* errstack, int timeout, bool non_blocking)
{
	int retval;
	int old_timeout=0;
	if (timeout>=0) {
		old_timeout=mySock->timeout(timeout);
	}

	retval = authenticate_inner(hostAddr,auth_methods,errstack,timeout,non_blocking);

	if (timeout>=0) {
		mySock->timeout(old_timeout);
	}

	return retval;
}

int Authentication::authenticate_inner( char *hostAddr, const char* auth_methods,
		CondorError* errstack, int timeout, bool non_blocking)
{
#if defined(SKIP_AUTHENTICATION)
	dprintf(D_ALWAYS, "Skipping....\n");
	/*
	errstack->push ( "AUTHENTICATE", AUTHENTICATE_ERR_NOT_BUILT,
			"this condor was built with SKIP_AUTHENTICATION");
	*/
	return 0;
#else
	m_host_addr = hostAddr ? hostAddr : "(unknown)";
	if (timeout > 0) {
		dprintf( D_SECURITY, "AUTHENTICATE: setting timeout for %s to %d.\n", m_host_addr.c_str(), timeout);
		m_auth_timeout_time = time(0) + timeout;
	} else {
		m_auth_timeout_time = 0;
	}

	if (IsDebugVerbose(D_SECURITY)) {
		if (m_host_addr.size()) {
			dprintf ( D_SECURITY, "AUTHENTICATE: in authenticate( addr == '%s', "
					"methods == '%s')\n", m_host_addr.c_str(), auth_methods);
		} else {
			dprintf ( D_SECURITY, "AUTHENTICATE: in authenticate( addr == NULL, "
					"methods == '%s')\n", auth_methods);
		}
	}

	m_methods_to_try = auth_methods;

	m_continue_handshake = false;
	m_continue_auth = false;
	auth_status = CAUTH_NONE;
	method_used = NULL;
	m_auth = NULL;

	return authenticate_continue(errstack, non_blocking);
#endif
}

int Authentication::authenticate_continue( CondorError* errstack, bool non_blocking )
{
	// Check for continuations;
	int firm = -1;
	bool do_handshake = true;
	if (m_continue_handshake) {
		firm = handshake_continue(m_methods_to_try, non_blocking);
		if ( firm == -2 ) {
			dprintf(D_SECURITY, "AUTHENTICATE: handshake would still block\n");
			return 2;
		}
		m_continue_handshake = false;
		do_handshake = false;
	}

	int auth_rc = 0;
	bool do_authenticate = true;
	if (m_continue_auth) {
		auth_rc = m_auth->authenticate_continue(errstack, non_blocking);
		if (auth_rc == 2 ) {
			dprintf(D_SECURITY, "AUTHENTICATE: auth would still block\n");
			return 2;
		}
		m_continue_auth = false;
		do_authenticate = false;
		goto authenticate;
	}
 
	m_auth = NULL;
	while (auth_status == CAUTH_NONE ) {
		if (m_auth_timeout_time>0 && m_auth_timeout_time <= time(0)) {
			dprintf(D_SECURITY, "AUTHENTICATE: exceeded deadline %ld\n", m_auth_timeout_time);
			errstack->pushf( "AUTHENTICATE", AUTHENTICATE_ERR_TIMEOUT, "exceeded %ld deadline during authentication", m_auth_timeout_time );
			break;
		}
		if (IsDebugVerbose(D_SECURITY)) {
			dprintf(D_SECURITY, "AUTHENTICATE: can still try these methods: %s\n", m_methods_to_try.c_str());
		}

		if (do_handshake) {
			firm = handshake(m_methods_to_try, non_blocking);
		}
		do_handshake = true;

		if ( firm == -2 ) {
			dprintf(D_SECURITY, "AUTHENTICATE: handshake would block\n");
			m_continue_handshake = true;
			return 2;
		}
		if ( firm < 0 ) {
			dprintf(D_ALWAYS, "AUTHENTICATE: handshake failed!\n");
			errstack->push( "AUTHENTICATE", AUTHENTICATE_ERR_HANDSHAKE_FAILED, "Failure performing handshake" );
			break;
		}

		m_method_name = "";
		switch ( firm ) {
#if defined(HAVE_EXT_GLOBUS)
			case CAUTH_GSI:
				m_auth = new Condor_Auth_X509(mySock);
				m_method_name = "GSI";
				break;
#endif /* HAVE_EXT_GLOBUS */

#ifdef HAVE_EXT_OPENSSL
			case CAUTH_SSL:
				m_auth = new Condor_Auth_SSL(mySock, 0, false);
				m_method_name = "SSL";
				break;
#ifdef HAVE_EXT_SCITOKENS
			case CAUTH_SCITOKENS:
				m_auth = new Condor_Auth_SSL(mySock, 0, true);
				m_method_name = "SCITOKENS";
				break;
#endif
#endif

#if defined(HAVE_EXT_KRB5) 
			case CAUTH_KERBEROS:
				m_auth = new Condor_Auth_Kerberos(mySock);
				m_method_name = "KERBEROS";
				break;
#endif

#ifdef HAVE_EXT_OPENSSL  // 3DES is the prerequisite for passwd auth
			case CAUTH_PASSWORD:
				m_auth = new Condor_Auth_Passwd(mySock, 1);
				m_method_name = "PASSWORD";
				break;
			case CAUTH_TOKEN: {
					// Make a copy of the pointer as a Condor_Auth_Passwd to avoid
					// repetitive static_cast<> below.
				auto tmp_auth = new Condor_Auth_Passwd(mySock, 2);
				m_auth = tmp_auth;
				const classad::ClassAd *policy = mySock->getPolicyAd();
				if (policy) {
					std::string issuer;
					if (policy->EvaluateAttrString(ATTR_SEC_TRUST_DOMAIN, issuer)) {
						tmp_auth->set_remote_issuer(issuer);
					}
					std::string key_str;
					if (policy->EvaluateAttrString(ATTR_SEC_ISSUER_KEYS, key_str)) {
						StringList key_list(key_str.c_str());
						const char *key;
						key_list.rewind();
						std::vector<std::string> keys;
						while ( (key = key_list.next()) ) {
							keys.push_back(key);
						}
						tmp_auth->set_remote_keys(keys);
					}
				}
                                m_method_name = "TOKEN";
                                break;
			}
#endif
 
#if defined(WIN32)
			case CAUTH_NTSSPI:
				m_auth = new Condor_Auth_SSPI(mySock);
				m_method_name = "NTSSPI";
				break;
#else
			case CAUTH_FILESYSTEM:
				m_auth = new Condor_Auth_FS(mySock);
				m_method_name = "FS";
				break;
			case CAUTH_FILESYSTEM_REMOTE:
				m_auth = new Condor_Auth_FS(mySock, 1);
				m_method_name = "FS_REMOTE";
				break;

#endif /* !defined(WIN32) */

#if defined(HAVE_EXT_MUNGE)
			case CAUTH_MUNGE:
				m_auth = new Condor_Auth_MUNGE(mySock);
				m_method_name = "MUNGE";
				break;
#endif

			case CAUTH_CLAIMTOBE:
				m_auth = new Condor_Auth_Claim(mySock);
				m_method_name = "CLAIMTOBE";
				break;
 
			case CAUTH_ANONYMOUS:
				m_auth = new Condor_Auth_Anonymous(mySock);
				m_method_name = "ANONYMOUS";
				break;
 
			case CAUTH_NONE:
				dprintf(D_SECURITY|D_FULLDEBUG,"AUTHENTICATE: no available authentication methods succeeded!\n");
				errstack->push("AUTHENTICATE", AUTHENTICATE_ERR_OUT_OF_METHODS,
						"Failed to authenticate with any method");
				return 0;

			default:
				dprintf(D_ALWAYS,"AUTHENTICATE: unsupported method: %i, failing.\n", firm);
				errstack->pushf("AUTHENTICATE", AUTHENTICATE_ERR_OUT_OF_METHODS,
						"Failure.  Unsupported method: %i", firm);
				return 0;
		}


		if (IsDebugVerbose(D_SECURITY)) {
			dprintf(D_SECURITY, "AUTHENTICATE: will try to use %d (%s)\n", firm,
					(m_method_name.size()?m_method_name.c_str():"?!?") );
		}

		// We have just picked a new method, so we definitely want to
		// make this value true.  We log a message only if we changed
		// it.
		if(!do_authenticate) {
			do_authenticate = true;
			if (IsDebugVerbose(D_SECURITY)) {
				dprintf(D_SECURITY, "AUTHENTICATE: forcing do_authenticate to true.\n");
			}
		}

		//------------------------------------------
		// Now authenticate
		//------------------------------------------
authenticate:
		// Re-check deadline as handshake could have taken awhile.
		if (m_auth_timeout_time>0 && m_auth_timeout_time <= time(0)) {
			dprintf(D_SECURITY, "AUTHENTICATE: exceeded deadline %ld\n", m_auth_timeout_time);
			errstack->pushf( "AUTHENTICATE", AUTHENTICATE_ERR_TIMEOUT, "exceeded %ld deadline during authentication", m_auth_timeout_time );
			break;
		}

		if (IsDebugVerbose(D_SECURITY)) {
			dprintf (D_SECURITY, "AUTHENTICATE: do_authenticate is %i.\n", do_authenticate);
		}

		if (do_authenticate) {
			auth_rc = m_auth->authenticate(m_host_addr.c_str(), errstack, non_blocking);
			if (auth_rc == 2) {
				m_continue_auth = true;
				return 2;
			}
		}

			// check to see if the auth IP is the same as the socket IP
		if( auth_rc ) {
			char const *sockip = mySock->peer_ip_str();
			char const *authip = m_auth->getRemoteHost() ;

			auth_rc = !sockip || !authip || !strcmp(sockip,authip);

			if (!auth_rc && !param_boolean( "DISABLE_AUTHENTICATION_IP_CHECK", false)) {
				errstack->pushf("AUTHENTICATE", AUTHENTICATE_ERR_METHOD_FAILED,
								"authenticated remote host does not match connection address (%s vs %s)", authip, sockip );
				dprintf (D_ALWAYS, "AUTHENTICATE: ERROR: authenticated remot ehost does not match connection address (%s vs %s); configure DISABLE_AUTHENTICATION_IP_CHECK=TRUE if this check should be skipped\n",authip,sockip);
			}
		}

		if( !auth_rc ) {
			delete m_auth;
			m_auth = NULL;

			errstack->pushf("AUTHENTICATE", AUTHENTICATE_ERR_METHOD_FAILED,
					"Failed to authenticate using %s", m_method_name.c_str() );

			//if authentication failed, try again after removing from client tries
			if ( mySock->isClient() ) {
				// need to remove this item from the MyString!!  perhaps there is a
				// better way to do this...  anyways, 'firm' is equal to the bit value
				// of a particular method, so we'll just convert each item in the list
				// and keep it if it's not that particular bit.
				StringList meth_iter( m_methods_to_try.c_str() );
				meth_iter.rewind();
				MyString new_list;
				char *tmp = NULL;
				while( (tmp = meth_iter.next()) ) {
					int that_bit = SecMan::getAuthBitmask( tmp );

					// keep if this isn't the failed method.
					if (firm != that_bit) {
						// and of course, keep the comma's correct.
						if (new_list.Length() > 0) {
							new_list += ",";
						}
						new_list += tmp;
					}
				}

				// trust the copy constructor. :)
				m_methods_to_try = new_list;
			}

			dprintf(D_SECURITY,"AUTHENTICATE: method %d (%s) failed.\n", firm,
					(m_method_name.size()?m_method_name.c_str():"?!?"));
		} else {
			// authentication succeeded.  store the object (we may call upon
			// its wrapper functions) and set the auth_status of this sock to
			// the bitmask method we used and the method_used to the string
			// name.  (string name is obtained above because there is currently
			// no bitmask -> string map)
			authenticator_ = m_auth;
			m_auth = NULL;
			auth_status = authenticator_->getMode();
			if (m_method_name.size()) {
				method_used = strdup(m_method_name.c_str());
			} else {
				method_used = NULL;
			}
		}
	}
	return authenticate_finish(errstack);
}

int Authentication::authenticate_finish(CondorError *errstack)
{
	//if none of the methods succeeded, we fall thru to default "none" from above
	int retval = ( auth_status != CAUTH_NONE );
	if (IsDebugVerbose(D_SECURITY)) {
		dprintf(D_SECURITY, "AUTHENTICATE: auth_status == %i (%s)\n", auth_status,
				(method_used?method_used:"?!?") );
	}
	dprintf(D_SECURITY, "Authentication was a %s.\n", retval == 1 ? "Success" : "FAILURE" );


	// at this point, all methods have set the raw authenticated name available
	// via getAuthenticatedName().

	if(authenticator_) {
		dprintf (D_SECURITY, "ZKM: setting default map to %s\n",
				 authenticator_->getRemoteFQU()?authenticator_->getRemoteFQU():"(null)");
	}

	// check to see if CERTIFICATE_MAPFILE was defined.  if so, use it.  if
	// not, do nothing.  the user and domain have been filled in by the
	// authentication method itself, so just leave that alone.
	bool use_mapfile = param_defined("CERTIFICATE_MAPFILE");

	// if successful so far, invoke the security MapFile.  the output of that
	// is the "canonical user".  if that has an '@' sign, split it up on the
	// last '@' and set the user and domain.  if there is more than one '@',
	// the user will contain the leftovers after the split and the domain
	// always has none.
	if (retval && use_mapfile) {
		const char * name_to_map = authenticator_->getAuthenticatedName();
		if (name_to_map) {
			dprintf (D_SECURITY, "ZKM: name to map is '%s'\n", name_to_map);
			dprintf (D_SECURITY, "ZKM: pre-map: current user is '%s'\n",
					authenticator_->getRemoteUser()?authenticator_->getRemoteUser():"(null)");
			dprintf (D_SECURITY, "ZKM: pre-map: current domain is '%s'\n",
					authenticator_->getRemoteDomain()?authenticator_->getRemoteDomain():"(null)");
			map_authentication_name_to_canonical_name(auth_status, method_used, name_to_map);
		} else {
			dprintf (D_SECURITY, "ZKM: name to map is null, not mapping.\n");
		}
#if defined(HAVE_EXT_GLOBUS)
	} else if (auth_status == CAUTH_GSI ) {
		// Fall back to using the globus mapping mechanism.  GSI is a bit unique in that
		// it may be horribly expensive - or cause a SEGFAULT - to do an authorization callout.
		// Hence, we delay it until after we apply a mapfile or, here, have no map file.
		// nameGssToLocal calls setRemoteFoo directly.
		const char * name_to_map = authenticator_->getAuthenticatedName();
		if (name_to_map) {
			int retval = ((Condor_Auth_X509*)authenticator_)->nameGssToLocal(name_to_map);
			dprintf(D_SECURITY, "nameGssToLocal returned %s\n", retval ? "success" : "failure");
		} else {
			dprintf (D_SECURITY, "ZKM: name to map is null, not calling GSI authorization.\n");
		}
#endif
	}
	// for now, let's be a bit more verbose and print this to D_SECURITY.
	// yeah, probably all of the log lines that start with ZKM: should be
	// updated.  oh, i wish there were a D_ZKM, but alas, we're out of bits.
	if( authenticator_ ) {
		dprintf (D_SECURITY, "ZKM: post-map: current user is '%s'\n",
				 authenticator_->getRemoteUser()?authenticator_->getRemoteUser():"(null)");
		dprintf (D_SECURITY, "ZKM: post-map: current domain is '%s'\n",
				 authenticator_->getRemoteDomain()?authenticator_->getRemoteDomain():"(null)");
		dprintf (D_SECURITY, "ZKM: post-map: current FQU is '%s'\n",
				 authenticator_->getRemoteFQU()?authenticator_->getRemoteFQU():"(null)");
	}

	mySock->allow_one_empty_message();

#if !defined(SKIP_AUTHENTICATION)
	if (retval && retval != 2 && m_key != NULL) {        // will always try to exchange key!
		// This is a hack for now, when we have only one authenticate method
		// this will be gone
		mySock->allow_empty_message_flag = FALSE;
		retval = exchangeKey(*m_key);
		if ( !retval ) {
			errstack->push("AUTHENTICATE",AUTHENTICATE_ERR_KEYEXCHANGE_FAILED,
			"Failed to securely exchange session key");
		}
		dprintf(D_SECURITY, "AUTHENTICATE: Result of end of authenticate is %d.\n", retval);
		mySock->allow_one_empty_message();
	}
#endif

	return ( retval );
}

void Authentication::reconfigMapFile()
{
	global_map_file_load_attempted = false;
}


#if !defined(SKIP_AUTHENTICATION)
// takes the type (as defined in handshake bitmask, CAUTH_*) and result of authentication,
// and maps it to the cannonical condor name.
//
void Authentication::map_authentication_name_to_canonical_name(int authentication_type, const char* method_string, const char* authentication_name) {

	// make sure the mapfile is loaded.  it's a static global variable.
	if (global_map_file_load_attempted == false) {
		if (global_map_file) {
			delete global_map_file;
			global_map_file = NULL;
		}

		dprintf (D_SECURITY, "ZKM: Parsing map file.\n");
		auto_free_ptr credential_mapfile(param("CERTIFICATE_MAPFILE"));
		if ( ! credential_mapfile) {
			dprintf(D_SECURITY, "ZKM: No CERTIFICATE_MAPFILE defined\n");
		} else {
			global_map_file = new MapFile();

			// prior to 8.5.8 all keys in CERTIFICATE_MAPFILE were assumed to be regexes
			// this is both slower, and less secure, so it is now possible to opt in to a syntax where
			// keys are assumed to be literals (i.e. hashable keys) unless the start and end with /
			bool assume_hash = param_boolean("CERTIFICATE_MAPFILE_ASSUME_HASH_KEYS", false);

			int line;
			if (0 != (line = global_map_file->ParseCanonicalizationFile(credential_mapfile.ptr(), assume_hash))) {
				dprintf(D_SECURITY, "ZKM: Error parsing %s at line %d", credential_mapfile.ptr(), line);
				delete global_map_file;
				global_map_file = NULL;
			}
		}
		global_map_file_load_attempted = true;
	} else {
		dprintf (D_SECURITY, "ZKM: map file already loaded.\n");
	}

	dprintf (D_SECURITY, "ZKM: attempting to map '%s'\n", authentication_name);

	// this will hold what we pass to the mapping function
	MyString auth_name_to_map = authentication_name;

	bool included_voms = false;

#if defined(HAVE_EXT_GLOBUS)
	// if GSI, try first with the FQAN (dn plus voms attrs)
	if (authentication_type == CAUTH_GSI) {
		const char *fqan = ((Condor_Auth_X509*)authenticator_)->getFQAN();
		if (fqan && fqan[0]) {
			dprintf (D_SECURITY, "ZKM: GSI was used, and FQAN is present.\n");
			auth_name_to_map = fqan;
			included_voms = true;
		}
	}
#endif

	if (global_map_file) {
		MyString canonical_user;

		dprintf (D_SECURITY, "ZKM: 1: attempting to map '%s'\n", auth_name_to_map.Value());
		bool mapret = global_map_file->GetCanonicalization(method_string, auth_name_to_map.Value(), canonical_user);
		dprintf (D_SECURITY, "ZKM: 2: mapret: %i included_voms: %i canonical_user: %s\n", mapret, included_voms, canonical_user.Value());

		// if it did not find a user, and we included voms attrs, try again without voms
		if (mapret && included_voms) {
			dprintf (D_SECURITY, "ZKM: now attempting to map '%s'\n", authentication_name);
			mapret = global_map_file->GetCanonicalization(method_string, authentication_name, canonical_user);
			dprintf (D_SECURITY, "ZKM: now 2: mapret: %i included_voms: %i canonical_user: %s\n", mapret, included_voms, canonical_user.Value());
		}

		if (!mapret) {
			// returns true on failure?
			dprintf (D_FULLDEBUG, "ZKM: successful mapping to %s\n", canonical_user.Value());

			// there is a switch for GSI to use the default globus function for this, in
			// case there is some custom globus mapping add-on, or the admin just wants
			// to use the grid-mapfile in use by other globus software.
			//
			// if they don't opt for globus to map, just fall through to the condor
			// mapfile.
			//
			if ((authentication_type == CAUTH_GSI) && (canonical_user == "GSS_ASSIST_GRIDMAP")) {
#if defined(HAVE_EXT_GLOBUS)

				// nameGssToLocal calls setRemoteFoo directly.
				int retval = ((Condor_Auth_X509*)authenticator_)->nameGssToLocal( authentication_name );

				if (retval) {
					dprintf (D_SECURITY, "Globus-based mapping was successful.\n");
				} else {
					dprintf (D_SECURITY, "Globus-based mapping failed; will use gsi@unmapped.\n");
				}
#else
				dprintf(D_ALWAYS, "ZKM: GSI not compiled, but was used?!!\n");
#endif
				return;
			} else {

				dprintf (D_SECURITY, "ZKM: found user %s, splitting.\n", canonical_user.Value());

				MyString user;
				MyString domain;

				// this sets user and domain
				split_canonical_name( canonical_user, user, domain);

				authenticator_->setRemoteUser( user.Value() );
				authenticator_->setRemoteDomain( domain.Value() );

				// we're done.
				return;
			}
		} else {
			dprintf (D_FULLDEBUG, "ZKM: did not find user %s.\n", canonical_user.Value());
		}
	} else if (authentication_type == CAUTH_GSI) {
        // See notes above around the nameGssToLocal call about why we invoke GSI authorization here.
        // Theoretically, it should be impossible to hit this case - the invoking function thought
		// we had a mapfile, but we couldnt create one.  This just covers weird corner cases.

#if defined(HAVE_EXT_GLOBUS)
		int retval = ((Condor_Auth_X509*)authenticator_)->nameGssToLocal(authentication_name);
		dprintf(D_SECURITY, "nameGssToLocal returned %s\n", retval ? "success" : "failure");
#else
		dprintf(D_ALWAYS, "ZKM: GSI not compiled, so can't call nameGssToLocal!!\n");
#endif
	} else {
		dprintf (D_FULLDEBUG, "ZKM: global_map_file not present!\n");
	}
}

void Authentication::split_canonical_name(char const *can_name,char **user,char **domain) {
		// This version of the function exists to avoid use of MyString
		// in ReliSock, because that gets linked into std univ jobs.
		// This function is stubbed out in cedar_no_ckpt.C.
	MyString my_user,my_domain;
	split_canonical_name(can_name,my_user,my_domain);
	*user = strdup(my_user.Value());
	*domain = strdup(my_domain.Value());
}

void Authentication::split_canonical_name(MyString can_name, MyString& user, MyString& domain ) {

    char local_user[256];
 
	// local storage so we can modify it.
	strncpy (local_user, can_name.Value(), 255);
	local_user[255] = 0;

    // split it into user@domain
    char* tmp = strchr(local_user, '@');
    if (tmp == NULL) {
        user = local_user;
        char * uid_domain = param("UID_DOMAIN");
        if (uid_domain) {
            domain = uid_domain;
            free(uid_domain);
        } else {
            dprintf(D_SECURITY, "AUTHENTICATION: UID_DOMAIN not defined.\n");
        }
    } else {
        // tmp is pointing to '@' in local_user[]
        *tmp = 0;
        user = local_user;
        domain = (tmp+1);
    }
}
#endif //SKIP_AUTHENTICATION


int Authentication::isAuthenticated() const
{
#if defined(SKIP_AUTHENTICATION)
    return 0;
#else
    return( auth_status != CAUTH_NONE );
#endif
}


void Authentication::unAuthenticate()
{
#if !defined(SKIP_AUTHENTICATION)
    auth_status = CAUTH_NONE;
	if (authenticator_) {
    	delete authenticator_;
    	authenticator_ = 0;
	}
	if (method_used) {
		free (method_used);
		method_used = 0;
	}
#endif
}


char* Authentication::getMethodUsed() {
#if !defined(SKIP_AUTHENTICATION)
	return method_used;
#else
	return NULL;
#endif
}
/*
void Authentication::setAuthAny()
{
#if !defined(SKIP_AUTHENTICATION)
    canUseFlags = CAUTH_ANY;
#endif
}
*/

const char* Authentication::getAuthenticatedName()
{
#if defined(SKIP_AUTHENTICATION)
	return NULL;
#else
	if ( authenticator_ ) {
		return authenticator_->getAuthenticatedName();
	} else {
		return NULL;
	}
#endif
}

const char* Authentication::getFQAuthenticatedName()
{
#if defined(SKIP_AUTHENTICATION)
	return NULL;
#else
	if ( authenticator_ ) {
#if defined(HAVE_EXT_GLOBUS)
		if(strcasecmp("GSI", method_used) == 0) {
	        const char *fqan = ((Condor_Auth_X509*)authenticator_)->getFQAN();	
			if(fqan) {
				return fqan;
			}
		}
#endif // defined(HAVE_EXT_GLOBUS)
		return authenticator_->getAuthenticatedName();
	} else {
		return NULL;
	}
#endif // defined(SKIP_AUTHENTICATION)
}

int Authentication::setOwner( const char *owner ) 
{
#if defined(SKIP_AUTHENTICATION)
	return 0;
#else
	if ( authenticator_ ) {
            authenticator_->setRemoteUser(owner);
            return 1;
	}
        else {
            return 0;
        }
#endif /* SKIP_AUTHENTICATION */
}

//----------------------------------------------------------------------
// getRemoteAddress: after authentication, return the remote address
//----------------------------------------------------------------------
const char * Authentication :: getRemoteAddress() const
{
#if defined(SKIP_AUTHENTICATION)
    return NULL;
#else
    // If we are not using Kerberos
    if (authenticator_) {
        return authenticator_->getRemoteHost();
    }
    else {
        return NULL;
    }
#endif
}

//----------------------------------------------------------------------
// getDomain:: return domain information
//----------------------------------------------------------------------
const char * Authentication :: getDomain() const
{
#if defined(SKIP_AUTHENTICATION)
    return NULL;
#else
    if (authenticator_) {
        return authenticator_->getRemoteDomain();
    }
    else {
        return NULL;
    }
#endif
}

const char * Authentication::getOwner() const
{
#if defined(SKIP_AUTHENTICATION)
    return NULL;
#else
    // Since we never use getOwner() like it allocates memory
    // anywhere in the code, it shouldn't actually allocate
    // memory.  We can always just return claimToBe, since it'll
    // either be NULL or the string we want, which is the old
    // semantics.  -Derek Wright 3/12/99
	const char *owner;
    if (authenticator_) {
        owner = authenticator_->getRemoteUser();
    }
    else {
        owner = NULL;
    }

	// If we're authenticated, we should always have a valid owner
	if ( isAuthenticated() ) {
		if ( NULL == owner ) {
			EXCEPT( "Socket is authenticated, but has no owner!!" );
		}
	}
	return owner;
#endif  
}               

const char * Authentication::getFullyQualifiedUser() const
{
#if defined(SKIP_AUTHENTICATION)
    return NULL;
#else
    // Since we never use getOwner() like it allocates memory
    // anywhere in the code, it shouldn't actually allocate
    // memory.  We can always just return claimToBe, since it'll
    // either be NULL or the string we want, which is the old
    // semantics.  -Derek Wright 3/12/99
    if (authenticator_) {
        return authenticator_->getRemoteFQU();
    }
    else {
        return NULL;
    }
#endif  
}           

int Authentication :: end_time()
{
    int endtime = 0;
#if !defined(SKIP_AUTHENTICATION)
    if (authenticator_) {
        endtime = authenticator_->endTime();
    }
#endif
    return endtime;
}

bool Authentication :: is_valid()
{
    bool valid = FALSE;
#if !defined(SKIP_AUTHENTICATION)
    if (authenticator_) {
        valid = authenticator_->isValid();
    }
#endif
    return valid;
}

int Authentication :: wrap(char*  input, 
			   int    input_len, 
			   char*& output,
			   int&   output_len)
{
#if defined(SKIP_AUTHENTICATION)
    return FALSE;
#else
    // Shouldn't we check the flag first?
    if (authenticator_) {
        return authenticator_->wrap(input, input_len, output, output_len);
    }
    else {
        return FALSE;
    }
#endif
}
	
int Authentication :: unwrap(char*  input, 
			     int    input_len, 
			     char*& output, 
			     int&   output_len)
{
#if defined(SKIP_AUTHENTICATION)
    return FALSE;
#else
    // Shouldn't we check the flag first?
    if (authenticator_) {
        return authenticator_->unwrap(input, input_len, output, output_len);
    }
    else {
        return FALSE;
    }
#endif
}


#if !defined(SKIP_AUTHENTICATION)

int Authentication::exchangeKey(KeyInfo *& key)
{
	dprintf(D_SECURITY, "AUTHENTICATE: Exchanging keys with remote side.\n");
    int retval = 1;
    int hasKey, keyLength, protocol, duration;
    int outputLen, inputLen;
    char * encryptedKey = 0, * decryptedKey = 0;

    if (mySock->isClient()) {
        mySock->decode();
        if (!mySock->code(hasKey)) {
			hasKey = 0;
			retval = 0;
			dprintf(D_SECURITY, "Authentication::exchangeKey server disconnected from us\n");
		}
        mySock->end_of_message();
        if (hasKey) {
            if (!mySock->code(keyLength) ||
                !mySock->code(protocol)  ||
                !mySock->code(duration)  ||
                !mySock->code(inputLen)) {
                return 0;
            }
            encryptedKey = (char *) malloc(inputLen);
            mySock->get_bytes(encryptedKey, inputLen);
            mySock->end_of_message();

            // Now, unwrap it.  
            if ( authenticator_->unwrap(encryptedKey,  inputLen, decryptedKey, outputLen) ) {
					// Success
				key = new KeyInfo((unsigned char *)decryptedKey, keyLength,(Protocol) protocol,duration);
			} else {
					// Failure!
				retval = 0;
				key = NULL;
			}
        }
        else {
            key = NULL;
        }
    }
    else {  // server sends the key!
        mySock->encode();
        if (key == 0) {
            hasKey = 0;
            if (!mySock->code(hasKey)) {
				dprintf(D_SECURITY, "Authentication::exchangeKey client hung up during key exchange\n");
            	mySock->end_of_message();
				return 0;
			}
            mySock->end_of_message();
            return 1;
        }
        else { // First, wrap it
            hasKey = 1;
            if (!mySock->code(hasKey) || !mySock->end_of_message()) {
                return 0;
            }
            keyLength = key->getKeyLength();
            protocol  = (int) key->getProtocol();
            duration  = key->getDuration();

            if (!authenticator_->wrap((const char *)key->getKeyData(), keyLength, encryptedKey, outputLen))
			{
				// failed to wrap key.
				return 0;
			}

            if (!mySock->code(keyLength) || 
                !mySock->code(protocol)  ||
                !mySock->code(duration)  ||
                !mySock->code(outputLen) ||
                !mySock->put_bytes(encryptedKey, outputLen) ||
                !mySock->end_of_message()) {
                free(encryptedKey);
                return 0;
            }
        }
    }

    if (encryptedKey) {
        free(encryptedKey);
    }

    if (decryptedKey) {
        free(decryptedKey);
    }

    return retval;
}


void Authentication::setAuthType( int state ) {
    auth_status = state;
}


int Authentication::handshake(MyString my_methods, bool non_blocking) {

    int shouldUseMethod = 0;
    
    dprintf ( D_SECURITY, "HANDSHAKE: in handshake(my_methods = '%s')\n", my_methods.Value());

    if ( mySock->isClient() ) {

		// client

        dprintf (D_SECURITY, "HANDSHAKE: handshake() - i am the client\n");
        mySock->encode();
		int method_bitmask = SecMan::getAuthBitmask(my_methods.Value());
#if defined(HAVE_EXT_KRB5)
		if ( (method_bitmask & CAUTH_KERBEROS) && Condor_Auth_Kerberos::Initialize() == false )
#else
		if (method_bitmask & CAUTH_KERBEROS)
#endif
		{
			dprintf (D_SECURITY, "HANDSHAKE: excluding KERBEROS: %s\n", "Initialization failed");
			method_bitmask &= ~CAUTH_KERBEROS;
		}
#ifdef HAVE_EXT_OPENSSL
		if ( (method_bitmask & CAUTH_SSL) && Condor_Auth_SSL::Initialize() == false )
#else
		if (method_bitmask & CAUTH_SSL)
#endif
		{
			dprintf (D_SECURITY, "HANDSHAKE: excluding SSL: %s\n", "Initialization failed");
			method_bitmask &= ~CAUTH_SSL;
		}
		if ( (method_bitmask & CAUTH_GSI) && activate_globus_gsi() != 0 ) {
			dprintf (D_SECURITY, "HANDSHAKE: excluding GSI: %s\n", x509_error_string());
			method_bitmask &= ~CAUTH_GSI;
		}
#if defined(HAVE_EXT_MUNGE)
		if ( (method_bitmask & CAUTH_MUNGE) && Condor_Auth_MUNGE::Initialize() == false )
#else
		if (method_bitmask & CAUTH_MUNGE)
#endif
		{
			dprintf (D_SECURITY, "HANDSHAKE: excluding Munge: %s\n", "Initialization failed");
			method_bitmask &= ~CAUTH_MUNGE;
		}
        dprintf ( D_SECURITY, "HANDSHAKE: sending (methods == %i) to server\n", method_bitmask);
        if ( !mySock->code( method_bitmask ) || !mySock->end_of_message() ) {
            return -1;
        }

    	mySock->decode();
    	if ( !mySock->code( shouldUseMethod ) || !mySock->end_of_message() )  {
        	return -1;
    	}
    	dprintf ( D_SECURITY, "HANDSHAKE: server replied (method = %i)\n", shouldUseMethod);

    } else {

	return handshake_continue(my_methods, non_blocking);

    }

    return( shouldUseMethod );
}

int
Authentication::handshake_continue(MyString my_methods, bool non_blocking)
{
	//server
	if( non_blocking && !mySock->readReady() ) {
		return -2;
	}

	int shouldUseMethod = 0;
	int client_methods = 0;
	dprintf (D_SECURITY, "HANDSHAKE: handshake() - i am the server\n");
	mySock->decode();
	if ( !mySock->code( client_methods ) || !mySock->end_of_message() ) {
		return -1;
	}
	dprintf ( D_SECURITY, "HANDSHAKE: client sent (methods == %i)\n", client_methods);

	shouldUseMethod = selectAuthenticationType( my_methods, client_methods );
#if defined(HAVE_EXT_KRB5) 
	if ( (shouldUseMethod & CAUTH_KERBEROS) && Condor_Auth_Kerberos::Initialize() == false )
#else
	if (shouldUseMethod & CAUTH_KERBEROS)
#endif
	{
		dprintf (D_SECURITY, "HANDSHAKE: excluding KERBEROS: %s\n", "Initialization failed");
		shouldUseMethod &= ~CAUTH_KERBEROS;
	}
#ifdef HAVE_EXT_OPENSSL
	if ( (shouldUseMethod & CAUTH_SSL) && Condor_Auth_SSL::Initialize() == false )
#else
	if (shouldUseMethod & CAUTH_SSL)
#endif
	{
		dprintf (D_SECURITY, "HANDSHAKE: excluding SSL: %s\n", "Initialization failed");
		shouldUseMethod &= ~CAUTH_SSL;
	}
	if ( shouldUseMethod == CAUTH_GSI && activate_globus_gsi() != 0 ) {
		dprintf (D_SECURITY, "HANDSHAKE: excluding GSI: %s\n", x509_error_string());
		client_methods &= ~CAUTH_GSI;
		shouldUseMethod = selectAuthenticationType( my_methods, client_methods );
	}
#if defined(HAVE_EXT_MUNGE)
	if ( (shouldUseMethod & CAUTH_MUNGE) && Condor_Auth_MUNGE::Initialize() == false )
#else
	if (shouldUseMethod & CAUTH_MUNGE)
#endif
	{
		dprintf (D_SECURITY, "HANDSHAKE: excluding Munge: %s\n", "Initialization failed");
		shouldUseMethod &= ~CAUTH_MUNGE;
	}

	dprintf ( D_SECURITY, "HANDSHAKE: i picked (method == %i)\n", shouldUseMethod);


	mySock->encode();
	if ( !mySock->code( shouldUseMethod ) || !mySock->end_of_message() ) {
		return -1;
	}

	dprintf ( D_SECURITY, "HANDSHAKE: client received (method == %i)\n", shouldUseMethod);
	return shouldUseMethod;
}

int Authentication::selectAuthenticationType( MyString method_order, int remote_methods ) {

	// the first one in the list that is also in the bitmask is the one
	// that we pick.  so, iterate the list.

	StringList method_list( method_order.Value() );

	char * tmp = NULL;
	method_list.rewind();

	while (	(tmp = method_list.next()) ) {
		int that_bit = SecMan::getAuthBitmask( tmp );
		if ( remote_methods & that_bit ) {
			// we have a match.
			return that_bit;
		}
	}

	return 0;
}


#endif /* !defined(SKIP_AUTHENTICATION) */

