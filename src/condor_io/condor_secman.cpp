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
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_ver_info.h"
#include "condor_version.h"
#include "condor_environ.h"

#include "authentication.h"
#include "condor_string.h"
#include "condor_attributes.h"
#include "condor_adtypes.h"
#include "my_hostname.h"
#include "internet.h"
#include "HashTable.h"
#include "KeyCache.h"
#include "condor_daemon_core.h"
#include "condor_ipverify.h"
#include "condor_secman.h"
#include "classad_merge.h"
#include "daemon.h"
#include "daemon_core_sock_adapter.h"
#include "subsystem_info.h"
#include "setenv.h"
#include "ipv6_hostname.h"

extern bool global_dc_get_cookie(int &len, unsigned char* &data);

// special security session "hint" used to specify that a new
// security session should be used
char const *USE_TMP_SEC_SESSION = "USE_TMP_SEC_SESSION";

void SecMan::key_printf(int debug_levels, KeyInfo *k) {
	if (param_boolean("SEC_DEBUG_PRINT_KEYS", false)) {
		if (k) {
			char hexout[260];  // holds (at least) a 128 byte key.
			const unsigned char* dataptr = k->getKeyData();
			int   length  =  k->getKeyLength();

			for (int i = 0; (i < length) && (i < 24); i++) {
				sprintf (&hexout[i*2], "%02x", *dataptr++);
			}

			dprintf (debug_levels, "KEYPRINTF: [%i] %s\n", length, hexout);
		} else {
			dprintf (debug_levels, "KEYPRINTF: [NULL]\n");
		}
	}
}



const char SecMan::sec_feat_act_rev[][10] = {
	"UNDEFINED",
	"INVALID",
	"FAIL",
	"YES",
	"NO"
};


const char SecMan::sec_req_rev[][10] = {
	"UNDEFINED",
	"INVALID",
	"NEVER",
	"OPTIONAL",
	"PREFERRED",
	"REQUIRED"
};

KeyCache* SecMan::session_cache = NULL;
HashTable<MyString,MyString>* SecMan::command_map = NULL;
HashTable<MyString,classy_counted_ptr<SecManStartCommand> >* SecMan::tcp_auth_in_progress = NULL;
int SecMan::sec_man_ref_count = 0;
char* SecMan::_my_unique_id = 0;
char* SecMan::_my_parent_unique_id = 0;
bool SecMan::_should_check_env_for_unique_id = true;
IpVerify *SecMan::m_ipverify = NULL;

SecMan::sec_req
SecMan::sec_alpha_to_sec_req(char *b) {
	if (!b || !*b) {  
		// ... that is the question :)
		return SEC_REQ_INVALID;
	}

	switch (toupper(b[0])) {
		case 'R':  // required
		case 'Y':  // yes
		case 'T':  // true
			return SEC_REQ_REQUIRED;
		case 'P':  // preferred
			return SEC_REQ_PREFERRED;
		case 'O':  // optional
			return SEC_REQ_OPTIONAL;
		case 'F':  // false
		case 'N':  // never
			return SEC_REQ_NEVER;
	}

	return SEC_REQ_INVALID;
}


SecMan::sec_feat_act
SecMan::sec_lookup_feat_act( ClassAd &ad, const char* pname ) {

	char* res = NULL;
	ad.LookupString(pname, &res);

	if (res) {
		char buf[2];
		strncpy (buf, res, 1);
		buf[1] = 0;
		free (res);

		return sec_alpha_to_sec_feat_act(buf);
	}

	return SEC_FEAT_ACT_UNDEFINED;

}

SecMan::sec_feat_act
SecMan::sec_alpha_to_sec_feat_act(char *b) {
	if (!b || !*b) {  
		// ... that is the question :)
		return SEC_FEAT_ACT_INVALID;
	}

	switch (toupper(b[0])) {
		case 'F':  // enact
			return SEC_FEAT_ACT_FAIL;
		case 'Y':  // yes
			return SEC_FEAT_ACT_YES;
		case 'N':  // no
			return SEC_FEAT_ACT_NO;
	}

	return SEC_FEAT_ACT_INVALID;
}


/*
SecMan::sec_act
SecMan::sec_alpha_to_sec_act(char *b) {
	if (!b || !*b) {  
		// ... that is the question :)
		return SEC_ACT_INVALID;
	}

	switch (toupper(b[0])) {
		case 'A':  // ask
			return SEC_REQ_ASK;
		case 'E':  // enact
			return SEC_ENACT;
		case 'U':  // usekey
			return SEC_USEKEY;
		case 'N':  // none
			return SEC_NONE;
	}

	return SEC_INVALID;
}
*/

/*
SecMan::sec_act
SecMan::sec_lookup_act( ClassAd &ad, const char* pname ) {

	char* res = NULL;
	ad.LookupString(pname, &res);

	if (res) {
		char buf[2];
		strncpy (buf, res, 1);
		buf[1] = 0;
		free (res);

		return sec_alpha_to_sec_act(buf);
	}

	return SEC_UNDEFINED;
}
*/


SecMan::sec_req
SecMan::sec_lookup_req( ClassAd &ad, const char* pname ) {

	char* res = NULL;
	ad.LookupString(pname, &res);

	if (res) {
		char buf[2];
		strncpy (buf, res, 1);
		buf[1] = 0;
		free (res);

		return sec_alpha_to_sec_req(buf);
	}

	return SEC_REQ_UNDEFINED;
}

SecMan::sec_feat_act
SecMan::sec_req_to_feat_act (sec_req r) {
	if ( (r == SEC_REQ_REQUIRED) || (r == SEC_REQ_PREFERRED) ) {
		return SEC_FEAT_ACT_YES;
	} else {
		return SEC_FEAT_ACT_NO;
	}
}


bool
SecMan::sec_is_negotiable (sec_req r) {
	if ( (r == SEC_REQ_REQUIRED) || (r == SEC_REQ_NEVER) ) {
		return false;
	} else {
		return true;
	}
}


SecMan::sec_req
SecMan::sec_req_param( const char* fmt, DCpermission auth_level, sec_req def ) {
	char *config_value = getSecSetting( fmt, auth_level );

	if (config_value) {
		char buf[2];
		strncpy (buf, config_value, 1);
		buf[1] = 0;
		free (config_value);

		sec_req res = sec_alpha_to_sec_req(buf);

		if (res == SEC_REQ_UNDEFINED || res == SEC_REQ_INVALID) {
			MyString param_name;
			char *value = getSecSetting( fmt, auth_level, &param_name );
			if( res == SEC_REQ_INVALID ) {
				EXCEPT( "SECMAN: %s=%s is invalid!\n",
				        param_name.Value(), value ? value : "(null)" );
			}
			if( IsDebugVerbose(D_SECURITY) ) {
				dprintf (D_SECURITY,
				         "SECMAN: %s is undefined; using %s.\n",
				         param_name.Value(), SecMan::sec_req_rev[def]);
			}
			free(value);

			return def;
		}

		return res;
	}

	return def;
}

void SecMan::getAuthenticationMethods( DCpermission perm, MyString *result ) {
	ASSERT( result );

	char * p = getSecSetting ("SEC_%s_AUTHENTICATION_METHODS", perm);

	if (p) {
		*result = p;
		free (p);
	} else {
		*result = SecMan::getDefaultAuthenticationMethods();
	}
}

bool
SecMan::getIntSecSetting( int &result, const char* fmt, DCpermissionHierarchy const &auth_level, MyString *param_name /* = NULL */, char const *check_subsystem /* = NULL */ )
{
	return getSecSetting_implementation(&result,NULL,fmt,auth_level,param_name,check_subsystem);
}

char* 
SecMan::getSecSetting( const char* fmt, DCpermissionHierarchy const &auth_level, MyString *param_name /* = NULL */, char const *check_subsystem /* = NULL */ )
{
	char *result = NULL;
	getSecSetting_implementation(NULL,&result,fmt,auth_level,param_name,check_subsystem);
	return result;
}

bool
SecMan::getSecSetting_implementation( int *int_result,char **str_result, const char* fmt, DCpermissionHierarchy const &auth_level, MyString *param_name, char const *check_subsystem )
{
	DCpermission const *perms = auth_level.getConfigPerms();
	bool found;

		// Now march through the list of config settings to look for.  The
		// last one in the list will be DEFAULT_PERM, which we only use
		// if nothing else is found first.

	for( ; *perms != LAST_PERM; perms++ ) {
		MyString buf;
		if( check_subsystem ) {
				// First see if there is a specific config entry for the
				// specified condor subsystem.
			buf.formatstr( fmt, PermString(*perms) );
			buf.formatstr_cat("_%s",check_subsystem);
			if( int_result ) {
				found = param_integer( buf.Value(), *int_result, false, 0, false, 0, 0 );
			}
			else {
				*str_result = param( buf.Value() );
				found = *str_result;
			}
			if( found ) {
				if( param_name ) {
						// Caller wants to know the param name.
					param_name->append_to_list(buf);
				}
				return true;
			}
		}

		buf.formatstr( fmt, PermString(*perms) );
		if( int_result ) {
			found = param_integer( buf.Value(), *int_result, false, 0, false, 0, 0 );
		}
		else {
			*str_result = param( buf.Value() );
			found = *str_result;
		}
		if( found ) {
			if( param_name ) {
					// Caller wants to know the param name.
				param_name->append_to_list(buf);
			}
			return true;
		}
	}

	return false;
}



// params() for a bunch of stuff and sets up a class ad describing our security
// preferences/requirements.  returns true if the security policy is valid, and
// false otherwise.

// there are many ways to end up with and 'invalid' security policy.  here are a
// couple:
//
// 6.3-style negotiation is disabled and one of AUTH, ENC, or INTEG were required.
//
// either ENC or INTEG are supposed to happen but AUTH is NEVER (can't exchange
// private key if we don't authenticate)

bool
SecMan::FillInSecurityPolicyAd( DCpermission auth_level, ClassAd* ad, 
								bool raw_protocol,
								bool use_tmp_sec_session,
								bool force_authentication )
{
	if( ! ad ) {
		EXCEPT( "SecMan::FillInSecurityPolicyAd called with NULL ad!" );
	}

	// get values from config file, trying each auth level in the
	// list in turn.  The final level in the list will be "DEFAULT".
	// if that fails, the default value (OPTIONAL) is used.

	sec_req sec_authentication = force_authentication ? SEC_REQ_REQUIRED :
		sec_req_param("SEC_%s_AUTHENTICATION", auth_level, SEC_REQ_OPTIONAL);

	sec_req sec_encryption = sec_req_param(
		"SEC_%s_ENCRYPTION", auth_level, SEC_REQ_OPTIONAL);

	sec_req sec_integrity = sec_req_param(
		 "SEC_%s_INTEGRITY", auth_level, SEC_REQ_OPTIONAL);


	// regarding SEC_NEGOTIATE values:
	// REQUIRED- outgoing will always negotiate, and incoming must
	//           be negotiated as well.
	// PREFERRED- outgoing will try to negotiate but fall back to
	//            6.2 method if necessary.  incoming will allow
	//            negotiated & unnegotiated commands.
	// OPTIONAL- outgoing will be 6.2 style.  incoming will allow
	//           negotiated and unnegotiated commands.
	// NEVER- everything will be 6.2 style

	// as of 6.5.0, the default is PREFERRED

	sec_req sec_negotiation = sec_req_param ("SEC_%s_NEGOTIATION", auth_level, SEC_REQ_PREFERRED);

	if( raw_protocol ) {
		sec_negotiation = SEC_REQ_NEVER;
		sec_authentication = SEC_REQ_NEVER;
		sec_encryption = SEC_REQ_NEVER;
		sec_integrity = SEC_REQ_NEVER;
	}


	if (!ReconcileSecurityDependency (sec_authentication, sec_encryption) ||
		!ReconcileSecurityDependency (sec_authentication, sec_integrity) ||
	    !ReconcileSecurityDependency (sec_negotiation, sec_authentication) ||
	    !ReconcileSecurityDependency (sec_negotiation, sec_encryption) ||
		!ReconcileSecurityDependency (sec_negotiation, sec_integrity)) {

		// houston, we have a problem.  
		dprintf (D_SECURITY, "SECMAN: failure! can't resolve security policy:\n");
		dprintf (D_SECURITY, "SECMAN:   SEC_NEGOTIATION=\"%s\"\n",
				SecMan::sec_req_rev[sec_negotiation]);
		dprintf (D_SECURITY, "SECMAN:   SEC_AUTHENTICATION=\"%s\"\n",
				SecMan::sec_req_rev[sec_authentication]);
		dprintf (D_SECURITY, "SECMAN:   SEC_ENCRYPTION=\"%s\"\n", 
				SecMan::sec_req_rev[sec_encryption]);
		dprintf (D_SECURITY, "SECMAN:   SEC_INTEGRITY=\"%s\"\n", 
				SecMan::sec_req_rev[sec_integrity]);
		return false;
	}

	// for those of you reading this code, a 'paramer'
	// is a thing that param()s.
	char *paramer;

	// auth methods
	paramer = SecMan::getSecSetting ("SEC_%s_AUTHENTICATION_METHODS", auth_level);
	if (paramer == NULL) {
		paramer = strdup(SecMan::getDefaultAuthenticationMethods().Value());
	}

	if (paramer) {
		ad->Assign (ATTR_SEC_AUTHENTICATION_METHODS, paramer);
		free(paramer);
		paramer = NULL;
	} else {
		if( sec_authentication == SEC_REQ_REQUIRED ) {
			dprintf( D_SECURITY, "SECMAN: no auth methods, "
					 "but a feature was required! failing...\n" );
			return false;
		} else {
			// disable auth, which disables crypto and integrity.
			// if any of these were required, auth would be required
			// too after calling ReconcileSecurityDependency.
			dprintf( D_SECURITY, "SECMAN: no auth methods, "
			 	"disabling authentication, crypto, and integrity.\n" );
			sec_authentication = SEC_REQ_NEVER;
			sec_encryption = SEC_REQ_NEVER;
			sec_integrity = SEC_REQ_NEVER;
		}
	}



	// crypto methods
	paramer = SecMan::getSecSetting("SEC_%s_CRYPTO_METHODS", auth_level);
	if (!paramer) {
		paramer = strdup(SecMan::getDefaultCryptoMethods().Value());
	}

	if (paramer) {
		ad->Assign (ATTR_SEC_CRYPTO_METHODS, paramer);
		free(paramer);
		paramer = NULL;
	} else {
		if( sec_encryption == SEC_REQ_REQUIRED || 
			sec_integrity == SEC_REQ_REQUIRED ) {
			dprintf( D_SECURITY, "SECMAN: no crypto methods, "
					 "but it was required! failing...\n" );
		} else {
			dprintf( D_SECURITY, "SECMAN: no crypto methods, "
					 "disabling crypto.\n" );
			sec_encryption = SEC_REQ_NEVER;
			sec_integrity = SEC_REQ_NEVER;
		}
	}


	ad->Assign( ATTR_SEC_NEGOTIATION, SecMan::sec_req_rev[sec_negotiation] );

	ad->Assign ( ATTR_SEC_AUTHENTICATION, SecMan::sec_req_rev[sec_authentication] );

	ad->Assign (ATTR_SEC_ENCRYPTION, SecMan::sec_req_rev[sec_encryption] );

	ad->Assign ( ATTR_SEC_INTEGRITY, SecMan::sec_req_rev[sec_integrity] );

	ad->Assign ( ATTR_SEC_ENACT, "NO" );


	// subsystem
	ad->Assign ( ATTR_SEC_SUBSYSTEM, get_mySubSystem()->getName() );

    char * parent_id = my_parent_unique_id();
    if (parent_id) {
		ad->Assign ( ATTR_SEC_PARENT_UNIQUE_ID, parent_id );
	}

	// pid
	int    mypid = 0;
#ifdef WIN32
	mypid = ::GetCurrentProcessId();
#else
	mypid = ::getpid();
#endif
	ad->Assign ( ATTR_SEC_SERVER_PID, mypid );

	// key duration
	// ZKM TODO HACK
	// need to check kerb expiry.

	// first try the form SEC_<subsys>_<authlev>_SESSION_DURATION
	// if that does not exist, fall back to old form of
	// SEC_<authlev>_SESSION_DURATION.
	int session_duration;

		// set default session duration
	if ( get_mySubSystem()->isType(SUBSYSTEM_TYPE_TOOL) ||
		 get_mySubSystem()->isType(SUBSYSTEM_TYPE_SUBMIT) ) {
			// default for tools is 1 minute.
		session_duration = 60;
	} else {
			// default for daemons is one day.

			// Note that pre 6.6 condors have bugs with re-negotiation
			// of security sessions, so we used to set this to 100 days.
			// That caused memory bloating for dynamic pools.  

		session_duration = 86400;
	}

	char fmt[128];
	sprintf(fmt, "SEC_%s_%%s_SESSION_DURATION", get_mySubSystem()->getName() );
	if( !SecMan::getIntSecSetting(session_duration, fmt, auth_level) ) {
		SecMan::getIntSecSetting(session_duration, "SEC_%s_SESSION_DURATION", auth_level);
	}

	if( use_tmp_sec_session ) {
		// expire this session soon
		session_duration = 60;
	}

		// For historical reasons, session duration is inserted as a string
		// in the ClassAd
	MyString session_duration_buf;
	session_duration_buf.formatstr("%d",session_duration);
	ad->Assign ( ATTR_SEC_SESSION_DURATION, session_duration_buf );

	int session_lease = 3600;
	SecMan::getIntSecSetting(session_lease, "SEC_%s_SESSION_LEASE", auth_level);
	ad->Assign( ATTR_SEC_SESSION_LEASE, session_lease );

	return true;
}

bool
SecMan::ReconcileSecurityDependency (sec_req &a, sec_req &b) {
	if (a == SEC_REQ_NEVER) {
		if (b == SEC_REQ_REQUIRED) {
			return false;
		} else {
			b = SEC_REQ_NEVER;
		}
	}

	if (b > a) {
		a = b;
	}
	return true;
}


SecMan::sec_feat_act
SecMan::ReconcileSecurityAttribute(const char* attr,
								   ClassAd &cli_ad, ClassAd &srv_ad,
								   bool *required ) {

	// extract the values from the classads

	// pointers to string values
	char* cli_buf = NULL;
	char* srv_buf = NULL;

	// enums of the values
	sec_req cli_req;
	sec_req srv_req;


	// get the attribute from each
	cli_ad.LookupString(attr, &cli_buf);
	srv_ad.LookupString(attr, &srv_buf);

	// convert it to an enum
	cli_req = sec_alpha_to_sec_req(cli_buf);
	srv_req = sec_alpha_to_sec_req(srv_buf);

	// free the buffers
	if (cli_buf) {
		free (cli_buf);
	}

	if (srv_buf) {
		free (srv_buf);
	}

	if( required ) {
		// if either party requires this feature, indicate that
		*required = (cli_req == SEC_REQ_REQUIRED || srv_req == SEC_REQ_REQUIRED);
	}

	// this policy is moderately complicated.  make sure you know
	// the implications if you monkey with the below code.  -zach

	// basically, there is a chart of the desired input and output.
	// you can implement this in a number of different ways that are
	// all logically equivelant.  make a karnough map if you really
	// want to find the minimal solution :)

	// right now, the results are symmetric across client and server
	// (i.e. client and server can be switched with the same result)
	// i've marked the redundant rules with (*)

	// Client  Server  Result
	// R       R       YES
	// R       P       YES
	// R       O       YES
	// R       N       FAIL

	// P       R       YES  (*)
	// P       P       YES
	// P       O       YES
	// P       N       NO

	// O       R       YES  (*)
	// O       P       YES  (*)
	// O       O       NO
	// O       N       NO

	// N       R       FAIL (*)
	// N       P       NO   (*)
	// N       O       NO   (*)
	// N       N       NO


	if (cli_req == SEC_REQ_REQUIRED) {
		if (srv_req == SEC_REQ_NEVER) {
			return SEC_FEAT_ACT_FAIL;
		} else {
			return SEC_FEAT_ACT_YES;
		}
	}

	if (cli_req == SEC_REQ_PREFERRED) {
		if (srv_req == SEC_REQ_NEVER) {
			return SEC_FEAT_ACT_NO;
		} else {
			return SEC_FEAT_ACT_YES;
		}
	}

	if (cli_req == SEC_REQ_OPTIONAL) {
		if (srv_req == SEC_REQ_REQUIRED || srv_req == SEC_REQ_PREFERRED) {
			return SEC_FEAT_ACT_YES;
		} else {
			return SEC_FEAT_ACT_NO;
		}
	}

	if (cli_req == SEC_REQ_NEVER) {
		if (srv_req == SEC_REQ_REQUIRED) {
			return SEC_FEAT_ACT_FAIL;
		} else {
			return SEC_FEAT_ACT_NO;
		}
	}

	// cli_req is not in {REQUIRED, PREFERRED, OPTIONAL, NEVER} for some reason.
	return SEC_FEAT_ACT_FAIL;
}


ClassAd *
SecMan::ReconcileSecurityPolicyAds(ClassAd &cli_ad, ClassAd &srv_ad) {

	// figure out what to do
	sec_feat_act authentication_action;
	sec_feat_act encryption_action;
	sec_feat_act integrity_action;
	bool auth_required = false;


	authentication_action = ReconcileSecurityAttribute(
								ATTR_SEC_AUTHENTICATION,
								cli_ad, srv_ad, &auth_required );

	encryption_action = ReconcileSecurityAttribute(
								ATTR_SEC_ENCRYPTION,
								cli_ad, srv_ad );

	integrity_action = ReconcileSecurityAttribute(
								ATTR_SEC_INTEGRITY,
								cli_ad, srv_ad );

	if ( (authentication_action == SEC_FEAT_ACT_FAIL) ||
	     (encryption_action == SEC_FEAT_ACT_FAIL) ||
	     (integrity_action == SEC_FEAT_ACT_FAIL) ) {

		// one or more decisions could not be agreed upon, so
		// we fail.

		return NULL;
	}

	// make a classad with the results
	ClassAd * action_ad = new ClassAd();

	char buf[1024];

	sprintf (buf, "%s=\"%s\"", ATTR_SEC_AUTHENTICATION, SecMan::sec_feat_act_rev[authentication_action]);
	action_ad->Insert(buf);

	if( authentication_action == SecMan::SEC_FEAT_ACT_YES ) {
			// record whether the authentication is required or not, so
			// both parties know what to expect if authentication fails
		if( !auth_required ) {
				// this is assumed to be true if not set
			action_ad->Assign(ATTR_SEC_AUTH_REQUIRED,false);
		}
	}

	sprintf (buf, "%s=\"%s\"", ATTR_SEC_ENCRYPTION, SecMan::sec_feat_act_rev[encryption_action]);
	action_ad->Insert(buf);

	sprintf (buf, "%s=\"%s\"", ATTR_SEC_INTEGRITY, SecMan::sec_feat_act_rev[integrity_action]);
	action_ad->Insert(buf);


	char* cli_methods = NULL;
	char* srv_methods = NULL;
	if (cli_ad.LookupString( ATTR_SEC_AUTHENTICATION_METHODS, &cli_methods) &&
		srv_ad.LookupString( ATTR_SEC_AUTHENTICATION_METHODS, &srv_methods)) {

		// send the list for 6.5.0 and higher
		MyString the_methods = ReconcileMethodLists( cli_methods, srv_methods );
		sprintf (buf, "%s=\"%s\"", ATTR_SEC_AUTHENTICATION_METHODS_LIST, the_methods.Value());
		action_ad->Insert(buf);

		// send the single method for pre 6.5.0
		StringList  tmpmethodlist( the_methods.Value() );
		char* first;
		tmpmethodlist.rewind();
		first = tmpmethodlist.next();
		if (first) {
			sprintf (buf, "%s=\"%s\"", ATTR_SEC_AUTHENTICATION_METHODS, first);
			action_ad->Insert(buf);
		}
	}

	if (cli_methods) {
        free(cli_methods);
    }
	if (srv_methods) {
        free(srv_methods);
    }

	cli_methods = NULL;
	srv_methods = NULL;
	if (cli_ad.LookupString( ATTR_SEC_CRYPTO_METHODS, &cli_methods) &&
		srv_ad.LookupString( ATTR_SEC_CRYPTO_METHODS, &srv_methods)) {

		MyString the_methods = ReconcileMethodLists( cli_methods, srv_methods );
		sprintf (buf, "%s=\"%s\"", ATTR_SEC_CRYPTO_METHODS, the_methods.Value());
		action_ad->Insert(buf);
	}

	if (cli_methods) {
        free( cli_methods );
    }
	if (srv_methods) {
        free( srv_methods );
    }

	// reconcile the session expiration.  it is the SHORTER of
	// client's and server's value.

	int cli_duration = 0;
	int srv_duration = 0;

	char *dur = NULL;
	cli_ad.LookupString(ATTR_SEC_SESSION_DURATION, &dur);
	if (dur) {
		cli_duration = atoi(dur);
		free(dur);
	}

	dur = NULL;
	srv_ad.LookupString(ATTR_SEC_SESSION_DURATION, &dur);
	if (dur) {
		srv_duration = atoi(dur);
		free(dur);
	}

	sprintf (buf, "%s=\"%i\"", ATTR_SEC_SESSION_DURATION,
			(cli_duration < srv_duration) ? cli_duration : srv_duration );
	action_ad->Insert(buf);


		// Session lease time (max unused time) is the shorter of the
		// server's and client's values.  Note that a value of 0 means
		// no lease.  For compatibility with older versions that do not
		// support leases, we choose no lease if either side does not
		// specify one.
	int cli_lease = 0;
	int srv_lease = 0;

	if( cli_ad.LookupInteger(ATTR_SEC_SESSION_LEASE, cli_lease) &&
		srv_ad.LookupInteger(ATTR_SEC_SESSION_LEASE, srv_lease) )
	{
		if( cli_lease == 0 ) {
			cli_lease = srv_lease;
		}
		if( srv_lease == 0 ) {
			srv_lease = cli_lease;
		}
		action_ad->Assign( ATTR_SEC_SESSION_LEASE,
						   cli_lease < srv_lease ? cli_lease : srv_lease );
	}


	sprintf (buf, "%s=\"YES\"", ATTR_SEC_ENACT);
	action_ad->Insert(buf);

	return action_ad;

}

class SecManStartCommand: Service, public ClassyCountedPtr {
 public:
	SecManStartCommand (
		int cmd,Sock *sock,bool raw_protocol,
		CondorError *errstack,int subcmd,StartCommandCallbackType *callback_fn,
		void *misc_data,bool nonblocking,char const *cmd_description,char const *sec_session_id_hint,SecMan *sec_man):

		m_cmd(cmd),
		m_subcmd(subcmd),
		m_sock(sock),
		m_raw_protocol(raw_protocol),
		m_errstack(errstack),
		m_callback_fn(callback_fn),
		m_misc_data(misc_data),
		m_nonblocking(nonblocking),
		m_pending_socket_registered(false),
		m_sec_man(*sec_man),
		m_use_tmp_sec_session(false)
	{
		m_sec_session_id_hint = sec_session_id_hint ? sec_session_id_hint : "";
		if( m_sec_session_id_hint == USE_TMP_SEC_SESSION ) {
			m_use_tmp_sec_session = true;
		}
		m_already_tried_TCP_auth = false;
		if( !m_errstack ) {
			m_errstack = &m_internal_errstack;
		}
		m_is_tcp = (m_sock->type() == Stream::reli_sock);
		m_have_session = false;
		m_new_session = false;
		m_state = SendAuthInfo;
		m_enc_key = NULL;
		m_private_key = NULL;
		if(cmd_description) {
			m_cmd_description = cmd_description;
		}
		else {
			cmd_description = getCommandString(m_cmd);
			if(cmd_description) {
				m_cmd_description = cmd_description;
			}
			else {
				m_cmd_description.formatstr("command %d",m_cmd);
			}
		}
		m_already_logged_startcommand = false;
		m_negotiation = SecMan::SEC_REQ_UNDEFINED;

		m_sock_had_no_deadline = false;
	}

	~SecManStartCommand() {
		if( m_pending_socket_registered ) {
			m_pending_socket_registered = false;
			daemonCoreSockAdapter.decrementPendingSockets();
		}
		if( m_private_key ) {
			delete m_private_key;
			m_private_key = NULL;
		}
			// The callback function _must_ have been called
			// (and set to NULL) by now.
		ASSERT( !m_callback_fn );
	}

		// This function starts a command, as specified by the data
		// passed into the constructor.  The real work is done in
		// startCommand_inner(), but this function exists to guarantee
		// correct cleanup (e.g. calling callback etc.)
	StartCommandResult startCommand();

	void incrementPendingSockets() {
			// This is called to let daemonCore know that we are holding
			// onto a socket which is waiting for some callback other than
			// the obvious Register_Socket() callback, which handles this
			// case automatically.
		if( !m_pending_socket_registered ) {
			m_pending_socket_registered = true;
			daemonCoreSockAdapter.incrementPendingSockets();
		}
	}

	int operator== (const SecManStartCommand &other) {
			// We do not care about a deep comparison.
			// This is just to make the compiler happy on
			// SimpleList< classy_counted_ptr< SecManStartCommand > >
		return this == &other;
	}

 private:
	int m_cmd;
	int m_subcmd;
	MyString m_cmd_description;
	Sock *m_sock;
	bool m_raw_protocol;
	CondorError* m_errstack; // caller's errstack, if any, o.w. internal
	CondorError m_internal_errstack;
	StartCommandCallbackType *m_callback_fn;
	void *m_misc_data;
	bool m_nonblocking;
	bool m_pending_socket_registered;
	SecMan m_sec_man; // We create a copy of the original sec_man, so we
	                  // don't have to worry about longevity of it.  It's
	                  // all static data anyway, so this is no big deal.

	MyString m_session_key; // "addr,<cmd>"
	bool m_already_tried_TCP_auth;
	SimpleList<classy_counted_ptr<SecManStartCommand> > m_waiting_for_tcp_auth;
	classy_counted_ptr<SecManStartCommand> m_tcp_auth_command;

	bool m_is_tcp;
	bool m_have_session;
	bool m_new_session;
	bool m_use_tmp_sec_session;
	bool m_already_logged_startcommand;
	bool m_sock_had_no_deadline;
	ClassAd m_auth_info;
	SecMan::sec_req m_negotiation;
	MyString m_remote_version;
	KeyCacheEntry *m_enc_key;
	KeyInfo* m_private_key;
	MyString m_sec_session_id_hint;

	enum StartCommandState {
		SendAuthInfo,
		ReceiveAuthInfo,
		Authenticate,
		ReceivePostAuthInfo,
	} m_state;

		// All functions that call _inner() functions must
		// pass the result of the _inner() function to doCallback().
		// This ensures that when SecManStartCommand is done, the
		// registered callback function will be called in all cases.
	StartCommandResult doCallback( StartCommandResult result );

		// This does most of the work of the startCommand() protocol.
		// It is called from within a wrapper function that guarantees
		// correct cleanup (e.g. callback function being called etc.)
	StartCommandResult startCommand_inner();

		// When trying to start a UDP command, this is called to first
		// get a security session via TCP.  It will continue with the
		// rest of the startCommand protocol once the TCP auth attempt
		// is completed.
	StartCommandResult DoTCPAuth_inner();

		// These functions are called at successive stages in the protocol.
	StartCommandResult sendAuthInfo_inner();
	StartCommandResult receiveAuthInfo_inner();
	StartCommandResult authenticate_inner();
	StartCommandResult receivePostAuthInfo_inner();

		// This is called when the TCP auth attempt completes.
	static void TCPAuthCallback(bool success,Sock *sock,CondorError *errstack,void *misc_data);

		// This is the _inner() function for TCPAuthCallback().
	StartCommandResult TCPAuthCallback_inner( bool auth_succeeded, Sock *tcp_auth_sock );

		// This is called when we were waiting for another
		// instance to finish a TCP auth session.
	void ResumeAfterTCPAuth(bool auth_succeeded);

		// This is called when we want to wait for a
		// non-blocking socket operation to complete.
	StartCommandResult WaitForSocketCallback();

		// This is where we get called back when a
		// non-blocking socket operation finishes.
	int SocketCallback( Stream *stream );
};

StartCommandResult
SecMan::startCommand( int cmd, Sock* sock, bool raw_protocol, CondorError* errstack, int subcmd, StartCommandCallbackType *callback_fn, void *misc_data, bool nonblocking,char const *cmd_description,char const *sec_session_id_hint)
{
	// This function is simply a convenient wrapper around the
	// SecManStartCommand class, which does the actual work.

	// If this is nonblocking, we must create the following on the heap.
	// The blocking case could avoid use of the heap, but for simplicity,
	// we just do the same in both cases.

	classy_counted_ptr<SecManStartCommand> sc = new SecManStartCommand(cmd,sock,raw_protocol,errstack,subcmd,callback_fn,misc_data,nonblocking,cmd_description,sec_session_id_hint,this);

	ASSERT(sc.get());

	return sc->startCommand();
}

StartCommandResult
SecManStartCommand::doCallback( StartCommandResult result )
{
		// StartCommandContinue is for internal use and should never be the
		// final status returned to the caller.
	ASSERT( result != StartCommandContinue );

	if( result == StartCommandSucceeded ) {
			// Check our policy to make sure the server we have just
			// connected to is authorized.

		char const *server_fqu = m_sock->getFullyQualifiedUser();

		if( IsDebugVerbose(D_SECURITY) ) {
			dprintf(D_SECURITY,
			        "Authorizing server '%s/%s'.\n",
			        server_fqu ? server_fqu : "*",
					m_sock->peer_ip_str() );
		}

		MyString deny_reason;

		int authorized = m_sec_man.Verify(
			CLIENT_PERM,
			m_sock->peer_addr(),
			server_fqu,
			NULL,
			&deny_reason );

		if( authorized != USER_AUTH_SUCCESS ) {
			m_errstack->pushf("SECMAN", SECMAN_ERR_CLIENT_AUTH_FAILED,
			         "DENIED authorization of server '%s/%s' (I am acting as "
			         "the client): reason: %s.",
					 server_fqu ? server_fqu : "*",
					 m_sock->peer_ip_str(), deny_reason.Value() );
			result = StartCommandFailed;
		}
	}

	if( result == StartCommandFailed && m_errstack == &m_internal_errstack ) {
			// caller did not provide an errstack, so print out the
			// internal one

		dprintf(D_ALWAYS, "ERROR: %s\n", m_internal_errstack.getFullText().c_str());
	}

	if(result != StartCommandInProgress) {
		if( m_sock_had_no_deadline ) {
				// There was no deadline on this socket, so restore
				// it to that state.
			m_sock->set_deadline( 0 );
		}
	}

	if(result == StartCommandInProgress) {
			// Do nothing.
			// We will (MUST) be called again in the future
			// once the final result is known.

		if(!m_callback_fn) {
				// Caller wants us to go ahead and get a session key,
				// but caller will try sending the UDP command later,
				// rather than dealing with a callback.
			result = StartCommandWouldBlock;
		}
	}
	else if(m_callback_fn) {
		bool success = result == StartCommandSucceeded;
		CondorError *cb_errstack = m_errstack == &m_internal_errstack ?
		                           NULL : m_errstack;
		(*m_callback_fn)(success,m_sock,cb_errstack,m_misc_data);

		m_callback_fn = NULL;
		m_misc_data = NULL;

			// Caller is responsible for deallocating the following
			// in the callback, so do not point to them anymore.
		m_errstack = &m_internal_errstack;
		m_sock = NULL;

			// We just called back with the success/failure code.
			// Therefore, we simply return success to indicate that we
			// successfully called back.
		result = StartCommandSucceeded;
	}

	if( result == StartCommandWouldBlock ) {
			// It is caller's responsibility to delete socket when we
			// return StartCommandWouldBlock, so ensure that we never
			// reference the socket again from now on.
		m_sock = NULL;
	}

	return result;
}

StartCommandResult
SecManStartCommand::startCommand()
{
	// NOTE: if there is a callback function, we _must_ guarantee that it is
	// eventually called in all code paths leading from here.

	// prevent *this from being deleted while this function is executing
	classy_counted_ptr<SecManStartCommand> self = this;

	return doCallback( startCommand_inner() );
}

StartCommandResult
SecManStartCommand::startCommand_inner()
{
	// NOTE: like all _inner() functions, the caller of this function
	// must ensure that the m_callback_fn is called (if there is one).

	ASSERT(m_sock);
	ASSERT(m_errstack);

	dprintf ( D_SECURITY, "SECMAN: %scommand %i %s to %s from %s port %i (%s%s).\n",
			  m_already_logged_startcommand ? "resuming " : "",
			  m_cmd,
			  m_cmd_description.Value(),
			  m_sock->peer_description(),
			  m_is_tcp ? "TCP" : "UDP",
			  m_sock->get_port(),
			  m_nonblocking ?  "non-blocking" : "blocking",
			  m_raw_protocol ? ", raw" : "");

	m_already_logged_startcommand = true;


	if( m_sock->deadline_expired() ) {
		MyString msg;
		msg.formatstr("deadline for %s %s has expired.",
					m_is_tcp && !m_sock->is_connected() ?
					"connection to" : "security handshake with",
					m_sock->peer_description());
		dprintf(D_SECURITY,"SECMAN: %s\n", msg.Value());
		m_errstack->pushf("SECMAN", SECMAN_ERR_CONNECT_FAILED,
						  "%s", msg.Value());

		return StartCommandFailed;
	}
	else if( m_nonblocking && m_sock->is_connect_pending() ) {
		dprintf(D_SECURITY,"SECMAN: waiting for TCP connection to %s.\n",
				m_sock->peer_description());
		return WaitForSocketCallback();
	}
	else if( m_is_tcp && !m_sock->is_connected()) {
		MyString msg;
		msg.formatstr("TCP connection to %s failed.",
					m_sock->peer_description());
		dprintf(D_SECURITY,"SECMAN: %s\n", msg.Value());
		m_errstack->pushf("SECMAN", SECMAN_ERR_CONNECT_FAILED,
						  "%s", msg.Value());

		return StartCommandFailed;
	}

		// The following loop executes each part of the protocol in the
		// correct order.  In non-blocking mode, one of the functions
		// may return StartCommandInProgress, in which case we return
		// and wait to be called back to resume where we left off.

	StartCommandResult result = StartCommandSucceeded;
	do {
		switch( m_state ) {
		case SendAuthInfo:
			result = sendAuthInfo_inner();
			break;
		case ReceiveAuthInfo:
			result = receiveAuthInfo_inner();
			break;
		case Authenticate:
			result = authenticate_inner();
			break;
		case ReceivePostAuthInfo:
			result = receivePostAuthInfo_inner();
			break;
		default:
			EXCEPT("Unexpected state in SecManStartCommand: %d\n",m_state);
		}
	} while( result == StartCommandContinue );

	return result;
}

bool
SecMan::LookupNonExpiredSession(char const *session_id, KeyCacheEntry *&session_key)
{
	if(!session_cache->lookup(session_id,session_key)) {
		return false;
	}

		// check the expiration.
	time_t cutoff_time = time(0);
	time_t expiration = session_key->expiration();
	if (expiration && expiration <= cutoff_time) {
		session_cache->expire(session_key);
		session_key = NULL;
		return false;
	}
	return true;
}

StartCommandResult
SecManStartCommand::sendAuthInfo_inner()
{

	// find out if we have a session id to use for this command

	MyString sid;

	sid = m_sec_session_id_hint;
	if( sid.Value()[0] && !m_raw_protocol && !m_use_tmp_sec_session ) {
		m_have_session = m_sec_man.LookupNonExpiredSession(sid.Value(), m_enc_key);
		if( m_have_session ) {
			dprintf(D_SECURITY,"Using requested session %s.\n",sid.Value());
		}
		else {
			dprintf(D_SECURITY,"Ignoring requested session, because it does not exist: %s\n",sid.Value());
		}
	}

	m_session_key.formatstr ("{%s,<%i>}", m_sock->get_connect_addr(), m_cmd);
	bool found_map_ent = false;
	if( !m_have_session && !m_raw_protocol && !m_use_tmp_sec_session ) {
		found_map_ent = (m_sec_man.command_map->lookup(m_session_key, sid) == 0);
	}
	if (found_map_ent) {
		dprintf (D_SECURITY, "SECMAN: using session %s for %s.\n", sid.Value(), m_session_key.Value());
		// we have the session id, now get the session from the cache
		m_have_session = m_sec_man.LookupNonExpiredSession(sid.Value(), m_enc_key);

		if(!m_have_session) {
			// the session is no longer in the cache... might as well
			// delete this mapping to it.  (we could delete them all, but
			// it requires iterating through the hash table)
			if (m_sec_man.command_map->remove(m_session_key.Value()) == 0) {
				dprintf (D_SECURITY, "SECMAN: session id %s not found, removed %s from map.\n", sid.Value(), m_session_key.Value());
			} else {
				dprintf (D_SECURITY, "SECMAN: session id %s not found and failed to removed %s from map!\n", sid.Value(), m_session_key.Value());
			}
		}
	}

	// if we have a private key, we will use the same security policy that
	// was decided on when the key was issued.
	// otherwise, get our security policy and work it out with the server.
	if (m_have_session) {
		MergeClassAds( &m_auth_info, m_enc_key->policy(), true );

		if (IsDebugVerbose(D_SECURITY)) {
			dprintf (D_SECURITY, "SECMAN: found cached session id %s for %s.\n",
					m_enc_key->id(), m_session_key.Value());
			m_sec_man.key_printf(D_SECURITY, m_enc_key->key());
			m_auth_info.dPrint( D_SECURITY );
		}

			// Ideally, we would only increment our lease expiration time after
			// verifying that the server renewed the lease on its side.  However,
			// there is no ACK in the protocol, so we just do it here.  Worst case,
			// we may end up trying to use a session that the server threw out,
			// which has the same error-handling as a restart of the server.
		m_enc_key->renewLease();

		m_new_session = false;
	} else {
		if( !m_sec_man.FillInSecurityPolicyAd(
				CLIENT_PERM, &m_auth_info,
				m_raw_protocol,	m_use_tmp_sec_session) )
		{
				// security policy was invalid.  bummer.
			dprintf( D_ALWAYS, 
					 "SECMAN: ERROR: The security policy is invalid.\n" );
			m_errstack->push("SECMAN", SECMAN_ERR_INVALID_POLICY,
				"Configuration Problem: The security policy is invalid." );
			return StartCommandFailed;
		}

		if (IsDebugVerbose(D_SECURITY)) {
			if( m_use_tmp_sec_session ) {
				dprintf (D_SECURITY, "SECMAN: using temporary security session for %s.\n", m_session_key.Value() );
			}
			else {
				dprintf (D_SECURITY, "SECMAN: no cached key for %s.\n", m_session_key.Value());
			}
		}

		// no sessions in udp
		if (m_is_tcp) {
			// for now, always open a session for tcp.
			m_new_session = true;
			m_auth_info.Assign(ATTR_SEC_NEW_SESSION,"YES");
		}
	}

	
	if (IsDebugVerbose(D_SECURITY)) {
		dprintf (D_SECURITY, "SECMAN: Security Policy:\n");
		m_auth_info.dPrint( D_SECURITY );
	}


	// find out our negotiation policy.
	m_negotiation = m_sec_man.sec_lookup_req( m_auth_info, ATTR_SEC_NEGOTIATION );
	if (m_negotiation == SecMan::SEC_REQ_UNDEFINED) {
		// this code never executes, as a default was already stuck into the
		// classad.
		m_negotiation = SecMan::SEC_REQ_PREFERRED;
		dprintf(D_SECURITY, "SECMAN: missing negotiation attribute, assuming PREFERRED.\n");
	}

	SecMan::sec_feat_act negotiate = m_sec_man.sec_req_to_feat_act(m_negotiation);
	if (negotiate == SecMan::SEC_FEAT_ACT_NO) {
		// old way:
		// code the int and be done.  there is no easy way to try the
		// new way if the old way fails, since it will fail outside
		// the scope of this function.

		if (IsDebugVerbose(D_SECURITY)) {
			dprintf(D_SECURITY, "SECMAN: not negotiating, just sending command (%i)\n", m_cmd);
		}

		// just code the command and be done
		m_sock->encode();
		if(!m_sock->code(m_cmd)) {
			m_errstack->pushf( "SECMAN", SECMAN_ERR_COMMUNICATIONS_ERROR,
							   "Failed to send raw command to %s.",
							   m_sock->peer_description());
			return StartCommandFailed;
		}

		// we must _NOT_ do an end_of_message() here!  Ques?  See Todd or Zach 9/01

		return StartCommandSucceeded;
	}

	// once we have reached this point:
	// we are going to send them a DC_SEC_NEGOTIATE command followed
	// by an auth_info ClassAd.  auth_info tells the server what to
	// do, which is usually to negotiate security options.  in this
	// case, the server will then send back a ClassAd telling us what
	// to do.
	// auth_info could also be a one-way "quickie" authentication,
	// using the security key.  this cannot be done unless there IS
	// a security key of course.

	// so, we know we will do it the new way, but there's a few
	// different kinds of classad we may send.



	// now take action.

	// if we've made it here, we need to talk with the other side
	// to either tell them what to do or ask what they want to do.

	if (IsDebugVerbose(D_SECURITY)) {
		dprintf ( D_SECURITY, "SECMAN: negotiating security for command %i.\n", m_cmd);
	}


	// special case for UDP: we can't actually authenticate via UDP.
	// so, we send a DC_AUTHENTICATE via TCP.  this will get us authenticated
	// and get us a key, which is what needs to happen.

	// however, we cannot do this if the UDP message is addressed to
	// our own daemoncore process, as we are NOT multithreaded and
	// cannot hold a TCP conversation with ourself. so, in this case,
	// we set a cookie in daemoncore and put the cookie in the classad
	// as proof that the message came from ourself.

	Sinful destsinful( m_sock->get_connect_addr() );
	Sinful oursinful( global_dc_sinful() );
	bool using_cookie = false;

	if (oursinful.addressPointsToMe(destsinful)) {
		// use a cookie.
		int len = 0;
		unsigned char* randomjunk = NULL;

		global_dc_get_cookie (len, randomjunk);
		
		m_auth_info.Assign(ATTR_SEC_COOKIE,randomjunk);
		dprintf (D_SECURITY, "SECMAN: %s=\"%s\"\n", ATTR_SEC_COOKIE,randomjunk);

		free(randomjunk);
		randomjunk = NULL;

		using_cookie = true;

	} else if ((!m_have_session) && (!m_is_tcp) && (!m_already_tried_TCP_auth)) {
		// can't use a cookie, so try to start a session.
		return DoTCPAuth_inner();
	} else if ((!m_have_session) && (!m_is_tcp) && m_already_tried_TCP_auth) {
			// there still is no session.
			//
			// this means when i sent them the NOP, no session was started.
			// maybe it means their security policy doesn't negotiate.
			// we'll send them this packet either way... if they don't like
			// it, they won't listen.
		if (IsDebugVerbose(D_SECURITY)) {
			dprintf ( D_SECURITY, "SECMAN: UDP has no session to use!\n");
		}

		ASSERT (m_enc_key == NULL);
	}

	// extract the version attribute current in the classad - it is
	// the version of the remote side.
	if(m_auth_info.LookupString( ATTR_SEC_REMOTE_VERSION, m_remote_version )) {
		CondorVersionInfo ver_info(m_remote_version.Value());
		m_sock->set_peer_version(&ver_info);
	}

	// fill in our version
	m_auth_info.Assign(ATTR_SEC_REMOTE_VERSION,CondorVersion());

	// fill in return address, if we are a daemon
	char const* dcss = global_dc_sinful();
	if (dcss) {
		m_auth_info.Assign(ATTR_SEC_SERVER_COMMAND_SOCK, dcss);
	}

	// fill in command
	m_auth_info.Assign(ATTR_SEC_COMMAND, m_cmd);

	if (m_cmd == DC_AUTHENTICATE) {
		// fill in sub-command
		m_auth_info.Assign(ATTR_SEC_AUTH_COMMAND, m_subcmd);
	}


	/*
	if (send_replay_info) {
		// in some future version,
		// fill in the following:
		//   ServerTime
		//   PID
		//   Counter
		//   MyEndpoint
	}
	*/


	if (!using_cookie && !m_is_tcp) {

		// udp works only with an already established session (gotten from a
		// tcp connection).  if there's no session, there's no way to enable
		// any features.  it could just be that this is a 6.2 daemon we are
		// talking to.  in that case, send the command the old way, as long
		// as that is permitted

		dprintf ( D_SECURITY, "SECMAN: UDP, m_have_session == %i\n",
				(m_have_session?1:0) );

		if (m_have_session) {
			// UDP w/ session
			if (IsDebugVerbose(D_SECURITY)) {
				dprintf ( D_SECURITY, "SECMAN: UDP has session %s.\n", m_enc_key->id());
			}

			SecMan::sec_feat_act will_authenticate = m_sec_man.sec_lookup_feat_act( m_auth_info, ATTR_SEC_AUTHENTICATION );
			SecMan::sec_feat_act will_enable_enc   = m_sec_man.sec_lookup_feat_act( m_auth_info, ATTR_SEC_ENCRYPTION );
			SecMan::sec_feat_act will_enable_mac   = m_sec_man.sec_lookup_feat_act( m_auth_info, ATTR_SEC_INTEGRITY );

			if (will_authenticate == SecMan::SEC_FEAT_ACT_UNDEFINED || 
				will_authenticate == SecMan::SEC_FEAT_ACT_INVALID || 
				will_enable_enc == SecMan::SEC_FEAT_ACT_UNDEFINED || 
				will_enable_enc == SecMan::SEC_FEAT_ACT_INVALID || 
				will_enable_mac == SecMan::SEC_FEAT_ACT_UNDEFINED || 
				will_enable_mac == SecMan::SEC_FEAT_ACT_INVALID ) {

				// suck.

				dprintf ( D_ALWAYS, "SECMAN: action attribute missing from classad\n");
				m_auth_info.dPrint( D_SECURITY );
				m_errstack->push( "SECMAN", SECMAN_ERR_ATTRIBUTE_MISSING,
						"Protocol Error: Action attribute missing.");
				return StartCommandFailed;
			}


			KeyInfo* ki  = NULL;
			if (m_enc_key->key()) {
				ki  = new KeyInfo(*(m_enc_key->key()));
			}
			
			if (will_enable_mac == SecMan::SEC_FEAT_ACT_YES) {

				if (!ki) {
					dprintf ( D_ALWAYS, "SECMAN: enable_mac has no key to use, failing...\n");
					m_errstack->push( "SECMAN", SECMAN_ERR_NO_KEY,
							"Failed to establish a crypto key." );
					return StartCommandFailed;
				}

				if (IsDebugVerbose(D_SECURITY)) {
					dprintf (D_SECURITY, "SECMAN: about to enable message authenticator.\n");
					m_sec_man.key_printf(D_SECURITY, ki);
				}

				// prepare the buffer to pass in udp header
				MyString key_id = m_enc_key->id();

				// stick our command socket sinful string in there
				char const* dcsss = global_dc_sinful();
				if (dcsss) {
					key_id += ",";
					key_id += dcsss;
				}

				m_sock->encode();
				m_sock->set_MD_mode(MD_ALWAYS_ON, ki, key_id.Value());

				dprintf ( D_SECURITY, "SECMAN: successfully enabled message authenticator!\n");
			} // if (will_enable_mac)

			bool turn_encryption_on = will_enable_enc == SecMan::SEC_FEAT_ACT_YES;
			if (turn_encryption_on && !ki) {
				dprintf ( D_ALWAYS, "SECMAN: enable_enc no key to use, failing...\n");
				m_errstack->push( "SECMAN", SECMAN_ERR_NO_KEY,
						"Failed to establish a crypto key." );
				return StartCommandFailed;
			}

			if( ki ) {
				if (IsDebugVerbose(D_SECURITY)) {
					dprintf (D_SECURITY, "SECMAN: about to enable encryption.\n");
					m_sec_man.key_printf(D_SECURITY, ki);
				}

					// prepare the buffer to pass in udp header
				MyString key_id = m_enc_key->id();

					// stick our command socket sinful string in there
				char const* dcsss = global_dc_sinful();
				if (dcsss) {
					key_id += ",";
					key_id += dcsss;
				}


				m_sock->encode();
				m_sock->set_crypto_key(turn_encryption_on, ki, key_id.Value());

				dprintf ( D_SECURITY,
						  "SECMAN: successfully enabled encryption%s.\n",
						  turn_encryption_on ? "" : " (but encryption mode is off by default for this packet)");

				delete ki;
			}
		} else {
			// UDP the old way...  who knows if they get it, we'll just assume they do.
			m_sock->encode();
			if( !m_sock->code(m_cmd) ) {
				m_errstack->pushf( "SECMAN", SECMAN_ERR_COMMUNICATIONS_ERROR,
								   "Failed to send raw UDP command to %s.",
								   m_sock->peer_description() );
				return StartCommandFailed;
			}
			return StartCommandSucceeded;
		}
	}


	// now send the actual DC_AUTHENTICATE command
	if (IsDebugVerbose(D_SECURITY)) {
		dprintf ( D_SECURITY, "SECMAN: sending DC_AUTHENTICATE command\n");
	}
	int authcmd = DC_AUTHENTICATE;
    m_sock->encode();
	if (! m_sock->code(authcmd)) {
		dprintf ( D_ALWAYS, "SECMAN: failed to send DC_AUTHENTICATE\n");
		m_errstack->push( "SECMAN", SECMAN_ERR_COMMUNICATIONS_ERROR,
						"Failed to send DC_AUTHENTICATE message." );
		return StartCommandFailed;
	}


	if (IsDebugVerbose(D_SECURITY)) {
		dprintf ( D_SECURITY, "SECMAN: sending following classad:\n");
		m_auth_info.dPrint ( D_SECURITY );
	}

	// send the classad
	if (! m_auth_info.put(*m_sock)) {
		dprintf ( D_ALWAYS, "SECMAN: failed to send auth_info\n");
		m_errstack->push( "SECMAN", SECMAN_ERR_COMMUNICATIONS_ERROR,
						"Failed to send auth_info." );
		return StartCommandFailed;
	}


	if (m_is_tcp) {

		if (! m_sock->end_of_message()) {
			dprintf ( D_ALWAYS, "SECMAN: failed to end classad message\n");
			m_errstack->push( "SECMAN", SECMAN_ERR_COMMUNICATIONS_ERROR,
						"Failed to end classad message." );
			return StartCommandFailed;
		}
	}

	m_state = ReceiveAuthInfo;
	return StartCommandContinue;
}

StartCommandResult
SecManStartCommand::receiveAuthInfo_inner()
{
	if (m_is_tcp) {
		if (m_sec_man.sec_lookup_feat_act(m_auth_info, ATTR_SEC_ENACT) != SecMan::SEC_FEAT_ACT_YES) {

			// if we asked them what to do, get their response

			if( m_nonblocking && !m_sock->readReady() ) {
				return WaitForSocketCallback();
			}

			ClassAd auth_response;
			m_sock->decode();

			if (!auth_response.initFromStream(*m_sock) ||
				!m_sock->end_of_message() ) {

				// if we get here, it means the serve accepted our connection
				// but dropped it after we sent the DC_AUTHENTICATE.

				// We used to conclude from this that our peer must be
				// too old to understand DC_AUTHENTICATE, so we would
				// reconnect and try sending a raw command (unless our
				// config required negotiation).  However, this is no
				// longer considered worth doing, because the most
				// likely case is not that our peer doesn't understand
				// but that something else caused us to fail to get a
				// response. Trying to reconnect can just make things
				// worse in this case.

				dprintf ( D_ALWAYS, "SECMAN: no classad from server, failing\n");
				m_errstack->push( "SECMAN", SECMAN_ERR_COMMUNICATIONS_ERROR,
						"Failed to end classad message." );
				return StartCommandFailed;
			}


			if (IsDebugVerbose(D_SECURITY)) {
				dprintf ( D_SECURITY, "SECMAN: server responded with:\n");
				auth_response.dPrint( D_SECURITY );
			}

				// Get rid of our sinful address in what will become
				// the session policy ad.  It's there because we sent
				// it to our peer, but no need to keep it around in
				// our copy of the session.  In fact, if we did, this
				// would cause performance problems in the KeyCache
				// index, which would contain a lot of (useless) index
				// entries for our own sinful address.
			m_auth_info.Delete( ATTR_SEC_SERVER_COMMAND_SOCK );
				// Ditto for our pid info.
			m_auth_info.Delete( ATTR_SEC_SERVER_PID );
			m_auth_info.Delete( ATTR_SEC_PARENT_UNIQUE_ID );

			// it makes a difference if the version is empty, so we must
			// explicitly delete it before we copy it.
			m_auth_info.Delete(ATTR_SEC_REMOTE_VERSION);
			m_sec_man.sec_copy_attribute( m_auth_info, auth_response, ATTR_SEC_REMOTE_VERSION );
			m_auth_info.LookupString(ATTR_SEC_REMOTE_VERSION,m_remote_version);
			if( !m_remote_version.IsEmpty() ) {
				CondorVersionInfo ver_info(m_remote_version.Value());
				m_sock->set_peer_version(&ver_info);
			}
			m_sec_man.sec_copy_attribute( m_auth_info, auth_response, ATTR_SEC_ENACT );
			m_sec_man.sec_copy_attribute( m_auth_info, auth_response, ATTR_SEC_AUTHENTICATION_METHODS_LIST );
			m_sec_man.sec_copy_attribute( m_auth_info, auth_response, ATTR_SEC_AUTHENTICATION_METHODS );
			m_sec_man.sec_copy_attribute( m_auth_info, auth_response, ATTR_SEC_CRYPTO_METHODS );
			m_sec_man.sec_copy_attribute( m_auth_info, auth_response, ATTR_SEC_AUTHENTICATION );
			m_sec_man.sec_copy_attribute( m_auth_info, auth_response, ATTR_SEC_AUTH_REQUIRED );
			m_sec_man.sec_copy_attribute( m_auth_info, auth_response, ATTR_SEC_ENCRYPTION );
			m_sec_man.sec_copy_attribute( m_auth_info, auth_response, ATTR_SEC_INTEGRITY );
			m_sec_man.sec_copy_attribute( m_auth_info, auth_response, ATTR_SEC_SESSION_DURATION );
			m_sec_man.sec_copy_attribute( m_auth_info, auth_response, ATTR_SEC_SESSION_LEASE );

			m_auth_info.Delete(ATTR_SEC_NEW_SESSION);

			m_auth_info.Assign(ATTR_SEC_USE_SESSION, "YES");

			m_sock->encode();

		}

	}

	m_state = Authenticate;
	return StartCommandContinue;
}

StartCommandResult
SecManStartCommand::authenticate_inner()
{
	if( m_is_tcp ) {
		SecMan::sec_feat_act will_authenticate = m_sec_man.sec_lookup_feat_act( m_auth_info, ATTR_SEC_AUTHENTICATION );
		SecMan::sec_feat_act will_enable_enc   = m_sec_man.sec_lookup_feat_act( m_auth_info, ATTR_SEC_ENCRYPTION );
		SecMan::sec_feat_act will_enable_mac   = m_sec_man.sec_lookup_feat_act( m_auth_info, ATTR_SEC_INTEGRITY );

		if (will_authenticate == SecMan::SEC_FEAT_ACT_UNDEFINED || 
			will_authenticate == SecMan::SEC_FEAT_ACT_INVALID || 
			will_enable_enc == SecMan::SEC_FEAT_ACT_UNDEFINED || 
			will_enable_enc == SecMan::SEC_FEAT_ACT_INVALID || 
			will_enable_mac == SecMan::SEC_FEAT_ACT_UNDEFINED || 
			will_enable_mac == SecMan::SEC_FEAT_ACT_INVALID ) {

			// missing some essential info.

			dprintf ( D_SECURITY, "SECMAN: action attribute missing from classad, failing!\n");
			m_auth_info.dPrint( D_SECURITY );
			m_errstack->push( "SECMAN", SECMAN_ERR_ATTRIBUTE_MISSING,
						"Protocol Error: Action attribute missing.");
			return StartCommandFailed;
		}

		// protocol fix:
		//
		// up to and including 6.6.0, will_authenticate would be set to true
		// if we are resuming a session that was authenticated.  this is not
		// necessary.
		//
		// so, as of 6.6.1, if we are resuming a session (as determined
		// by the expression (!m_new_session), AND the other side is 6.6.1
		// or higher, we will force will_authenticate to SEC_FEAT_ACT_NO.
		//
		// we can tell easily if the other side is 6.6.1 or higher by the
		// mere presence of the version, since that is when it was added.

		if ( will_authenticate == SecMan::SEC_FEAT_ACT_YES ) {
			if ((!m_new_session)) {
				if( !m_remote_version.IsEmpty() ) {
					dprintf( D_SECURITY, "SECMAN: resume, other side is %s, NOT reauthenticating.\n", m_remote_version.Value() );
					will_authenticate = SecMan::SEC_FEAT_ACT_NO;
				} else {
					dprintf( D_SECURITY, "SECMAN: resume, other side is pre 6.6.1, reauthenticating.\n");
				}
			} else {
				dprintf( D_SECURITY, "SECMAN: new session, doing initial authentication.\n" );
			}
		}

		

		// at this point, we know exactly what needs to happen.  if we asked
		// the other side, their choice is in will_authenticate.  if we
		// didn't ask, then our choice is in will_authenticate.



		if (will_authenticate == SecMan::SEC_FEAT_ACT_YES) {

			ASSERT (m_sock->type() == Stream::reli_sock);

			if (IsDebugVerbose(D_SECURITY)) {
				dprintf ( D_SECURITY, "SECMAN: authenticating RIGHT NOW.\n");
			}
			char * auth_methods = NULL;
			m_auth_info.LookupString( ATTR_SEC_AUTHENTICATION_METHODS_LIST, &auth_methods );
			if (auth_methods) {
				if (IsDebugVerbose(D_SECURITY)) {
					dprintf (D_SECURITY, "SECMAN: AuthMethodsList: %s\n", auth_methods);
				}
			} else {
				// lookup the 6.4 attribute name
				m_auth_info.LookupString( ATTR_SEC_AUTHENTICATION_METHODS, &auth_methods );
				if (IsDebugVerbose(D_SECURITY)) {
					dprintf (D_SECURITY, "SECMAN: AuthMethods: %s\n", auth_methods);
				}
			}

			if (!auth_methods) {
				// there's no auth methods.
				dprintf ( D_ALWAYS, "SECMAN: no auth method!, failing.\n");
				m_errstack->push( "SECMAN", SECMAN_ERR_ATTRIBUTE_MISSING,
						"Protocol Error: No auth methods.");
				return StartCommandFailed;
			} else {
				dprintf ( D_SECURITY, "SECMAN: Auth methods: %s\n", auth_methods);
			}

			int auth_timeout = m_sec_man.getSecTimeout( CLIENT_PERM );
			bool auth_success = m_sock->authenticate(m_private_key, auth_methods, m_errstack,auth_timeout);

			if (auth_methods) {  
				free(auth_methods);
			}

			if( !auth_success ) {
				bool auth_required = true;
				m_auth_info.LookupBool(ATTR_SEC_AUTH_REQUIRED,auth_required);

				if( auth_required ) {
					dprintf( D_ALWAYS,
							 "SECMAN: required authentication with %s failed, so aborting command %s.\n",
							 m_sock->peer_description(),
							 m_cmd_description.Value());
					return StartCommandFailed;
				}
				dprintf( D_SECURITY|D_FULLDEBUG,
						 "SECMAN: authentication with %s failed but was not required, so continuing.\n",
						 m_sock->peer_description() );
			}
		} else {
			// !m_new_session is equivalent to use_session in this client.
			if (!m_new_session) {
				// we are using this key
				if (m_enc_key && m_enc_key->key()) {
					m_private_key = new KeyInfo(*(m_enc_key->key()));
				} else {
					ASSERT (m_private_key == NULL);
				}
			}
		}

		
		if (will_enable_mac == SecMan::SEC_FEAT_ACT_YES) {

			if (!m_private_key) {
				dprintf ( D_ALWAYS, "SECMAN: enable_mac has no key to use, failing...\n");
				m_errstack->push ("SECMAN", SECMAN_ERR_NO_KEY,
							"Failed to establish a crypto key." );
				return StartCommandFailed;
			}

			if (IsDebugVerbose(D_SECURITY)) {
				dprintf (D_SECURITY, "SECMAN: about to enable message authenticator.\n");
				m_sec_man.key_printf(D_SECURITY, m_private_key);
			}

			m_sock->encode();
			m_sock->set_MD_mode(MD_ALWAYS_ON, m_private_key);

			dprintf ( D_SECURITY, "SECMAN: successfully enabled message authenticator!\n");
		} else {
			// we aren't going to enable MD5.  but we should still set the secret key
			// in case we decide to turn it on later.
			m_sock->encode();
			m_sock->set_MD_mode(MD_OFF, m_private_key);
		}

		if (will_enable_enc == SecMan::SEC_FEAT_ACT_YES) {

			if (!m_private_key) {
				dprintf ( D_ALWAYS, "SECMAN: enable_enc no key to use, failing...\n");
				m_errstack->push ("SECMAN", SECMAN_ERR_NO_KEY,
							"Failed to establish a crypto key." );
				return StartCommandFailed;
			}

			if (IsDebugVerbose(D_SECURITY)) {
				dprintf (D_SECURITY, "SECMAN: about to enable encryption.\n");
				m_sec_man.key_printf(D_SECURITY, m_private_key);
			}

			m_sock->encode();
			m_sock->set_crypto_key(true, m_private_key);

			dprintf ( D_SECURITY, "SECMAN: successfully enabled encryption!\n");
		} else {
			// we aren't going to enable encryption for everything.  but we should
			// still have a secret key ready to go in case someone decides to turn
			// it on later.
			m_sock->encode();
			m_sock->set_crypto_key(false, m_private_key);
		}
		
	}

	m_state = ReceivePostAuthInfo;
	return StartCommandContinue;
}

StartCommandResult
SecManStartCommand::receivePostAuthInfo_inner()
{
	if( m_is_tcp ) {
		if (m_new_session) {
			// There is no pending data to send, so the following is a no-op.
			// Why is it being done?  Perhaps to ensure clean state of the
			// socket?
			m_sock->encode();
			m_sock->end_of_message();

			if( m_nonblocking && !m_sock->readReady() ) {
				return WaitForSocketCallback();
			}

			// receive a classAd containing info about new session
			ClassAd post_auth_info;
			m_sock->decode();
			if (!post_auth_info.initFromStream(*m_sock) || !m_sock->end_of_message()) {
				dprintf (D_ALWAYS, "SECMAN: could not receive session info, failing!\n");
				m_errstack->push ("SECMAN", SECMAN_ERR_COMMUNICATIONS_ERROR,
							"could not receive post_auth_info." );
				return StartCommandFailed;
			} else {
				if (IsDebugVerbose(D_SECURITY)) {
					dprintf (D_SECURITY, "SECMAN: received post-auth classad:\n");
					post_auth_info.dPrint (D_SECURITY);
				}
			}

			// bring in the session ID
			m_sec_man.sec_copy_attribute( m_auth_info, post_auth_info, ATTR_SEC_SID );

			// other attributes
			// Copy "User" to "MyRemoteUserName", since this is _not_ the name
			// of our peer; it is our name as determined by our peer.
			// We reserve "User" for the name of our peer so that an
			// incoming/outgoing security session can be reversed and
			// used as a full-duplex session if desired.
			m_sec_man.sec_copy_attribute( m_auth_info, ATTR_SEC_MY_REMOTE_USER_NAME, post_auth_info, ATTR_SEC_USER );
			m_sec_man.sec_copy_attribute( m_auth_info, post_auth_info, ATTR_SEC_VALID_COMMANDS );

			if( m_sock->getFullyQualifiedUser() ) {
				m_auth_info.Assign( ATTR_SEC_USER, m_sock->getFullyQualifiedUser() );
			}
			else {
					// we did not authenticate peer, so this attribute
					// should not be defined
				ASSERT( !m_auth_info.LookupExpr( ATTR_SEC_USER ) );
			}
			m_sec_man.sec_copy_attribute( m_auth_info, post_auth_info, ATTR_SEC_TRIED_AUTHENTICATION );

			if (IsDebugVerbose(D_SECURITY)) {
				dprintf (D_SECURITY, "SECMAN: policy to be cached:\n");
				m_auth_info.dPrint(D_SECURITY);
			}

			char *sesid = NULL;
			m_auth_info.LookupString(ATTR_SEC_SID, &sesid);
			if (sesid == NULL) {
				dprintf (D_ALWAYS, "SECMAN: session id is NULL, failing\n");
				m_errstack->push( "SECMAN", SECMAN_ERR_ATTRIBUTE_MISSING,
						"Failed to lookup session id.");
				return StartCommandFailed;
			}

			char *cmd_list = NULL;
			m_auth_info.LookupString(ATTR_SEC_VALID_COMMANDS, &cmd_list);
			if (cmd_list == NULL) {
				dprintf (D_ALWAYS, "SECMAN: valid commands is NULL, failing\n");
				m_errstack->push( "SECMAN", SECMAN_ERR_ATTRIBUTE_MISSING,
						"Protocol Failure: Unable to lookup valid commands.");
				delete sesid;
				return StartCommandFailed;
			}


			ASSERT (m_enc_key == NULL);


			// extract the session duration
			char *dur = NULL;
			m_auth_info.LookupString(ATTR_SEC_SESSION_DURATION, &dur);

			int expiration_time = 0;
			time_t now = time(0);
			if( dur ) {
				expiration_time = now + atoi(dur);
			}

			int session_lease = 0;
			m_auth_info.LookupInteger(ATTR_SEC_SESSION_LEASE, session_lease );

				// This makes a copy of the policy ad, so we don't
				// have to. 
			condor_sockaddr peer_addr = m_sock->peer_addr();
			KeyCacheEntry tmp_key( sesid, &peer_addr, m_private_key,
								   &m_auth_info, expiration_time,
								   session_lease ); 
			dprintf (D_SECURITY, "SECMAN: added session %s to cache for %s seconds (%ds lease).\n", sesid, dur, session_lease);

            if (dur) {
                free(dur);
				dur = NULL;
            }

			// stick the key in the cache
			m_sec_man.session_cache->insert(tmp_key);


			// now add entrys which map all the {<sinful_string>,<command>} pairs
			// to the same key id (which is in the variable sesid)

			StringList coms(cmd_list);
			char *p;

			coms.rewind();
			while ( (p = coms.next()) ) {
				MyString keybuf;
				keybuf.formatstr ("{%s,<%s>}", m_sock->get_connect_addr(), p);

				// NOTE: HashTable returns ZERO on SUCCESS!!!
				if (m_sec_man.command_map->insert(keybuf, sesid) == 0) {
					// success
					if (IsDebugVerbose(D_SECURITY)) {
						dprintf (D_SECURITY, "SECMAN: command %s mapped to session %s.\n", keybuf.Value(), sesid);
					}
				} else {
					dprintf (D_ALWAYS, "SECMAN: command %s NOT mapped (insert failed!)\n", keybuf.Value());
				}
			}
			
			free( sesid );
            free( cmd_list );

		} // if (m_new_session)

	} // if (m_is_tcp)

	if( !m_new_session && m_have_session ) {
		char *fqu = NULL;
		if( m_auth_info.LookupString(ATTR_SEC_USER,&fqu) && fqu ) {
			if( IsDebugVerbose(D_SECURITY) ) {
				dprintf( D_SECURITY, "Getting authenticated user from cached session: %s\n", fqu );
			}
			m_sock->setFullyQualifiedUser( fqu );
			free( fqu );
		}

		bool tried_authentication = false;
		m_auth_info.LookupBool(ATTR_SEC_TRIED_AUTHENTICATION,tried_authentication);
		m_sock->setTriedAuthentication(tried_authentication);
	}

	m_sock->encode();
	m_sock->allow_one_empty_message();
	dprintf ( D_SECURITY, "SECMAN: startCommand succeeded.\n");

	return StartCommandSucceeded;
}

StartCommandResult
SecManStartCommand::DoTCPAuth_inner()
{
	ASSERT( !m_already_tried_TCP_auth );
	m_already_tried_TCP_auth = true;

	if(m_nonblocking) {
			// Make daemonCore aware that we are holding onto this
			// UDP socket while waiting for other events to complete.
		incrementPendingSockets();

			// Check if there is already a non-blocking TCP auth in progress
		classy_counted_ptr<SecManStartCommand> sc;
		if(m_sec_man.tcp_auth_in_progress->lookup(m_session_key,sc) == 0) {
				// Rather than starting yet another TCP session for
				// this session key, simply add ourselves to the list
				// of things waiting for the pending session to be
				// ready for use.

			if(m_nonblocking && !m_callback_fn) {
					// Caller wanted us to get a session key but did
					// not want to bother about handling a
					// callback. Since somebody else is already
					// getting a session, we are done.
				return StartCommandWouldBlock;
			}

			sc->m_waiting_for_tcp_auth.Append(this);

			if(IsDebugVerbose(D_SECURITY)) {
				dprintf(D_SECURITY,
						"SECMAN: waiting for pending session %s to be ready\n",
						m_session_key.Value());
			}

			return StartCommandInProgress;
		}
	}

	if (IsDebugVerbose(D_SECURITY)) {
		dprintf ( D_SECURITY, "SECMAN: need to start a session via TCP\n");
	}

		// we'll have to authenticate via TCP
	ReliSock *tcp_auth_sock = new ReliSock;

	ASSERT(tcp_auth_sock);

		// timeout on individual socket operations
	int TCP_SOCK_TIMEOUT = param_integer("SEC_TCP_SESSION_TIMEOUT", 20);
	tcp_auth_sock->timeout(TCP_SOCK_TIMEOUT);

		// we already know the address - condor uses the same TCP port as it does UDP port.
	MyString tcp_addr = m_sock->get_connect_addr();
	if (!tcp_auth_sock->connect(tcp_addr.Value(),0,m_nonblocking)) {
		dprintf ( D_SECURITY, "SECMAN: couldn't connect via TCP to %s, failing...\n", tcp_addr.Value());
		m_errstack->pushf("SECMAN", SECMAN_ERR_CONNECT_FAILED,
						  "TCP auth connection to %s failed.", tcp_addr.Value());
		delete tcp_auth_sock;
		return StartCommandFailed;
	}

		// Make note that this operation to do the TCP
		// auth operation is in progress, so others
		// wanting the same session key can wait for it.
	SecMan::tcp_auth_in_progress->insert(m_session_key,this);

	m_tcp_auth_command = new SecManStartCommand(
		DC_AUTHENTICATE,
		tcp_auth_sock,
		m_raw_protocol,
		m_errstack,
		m_cmd,
		m_nonblocking ? SecManStartCommand::TCPAuthCallback : NULL,
		m_nonblocking ? this : NULL,
		m_nonblocking,
		m_cmd_description.Value(),
		m_sec_session_id_hint.Value(),
		&m_sec_man);

	StartCommandResult auth_result = m_tcp_auth_command->startCommand();

	if( !m_nonblocking ) {
			// We did not pass a callback function to the TCP
			// startCommand(), because we need to pass back the final
			// result to the caller directly.

		return TCPAuthCallback_inner(
			auth_result == StartCommandSucceeded,
			tcp_auth_sock );
	}

	return StartCommandInProgress;
}

void
SecManStartCommand::TCPAuthCallback(bool success,Sock *sock,CondorError * /*errstack*/,void * misc_data)
{
	classy_counted_ptr<SecManStartCommand> self = (SecManStartCommand *)misc_data;

	self->doCallback( self->TCPAuthCallback_inner(success,sock) );
}

StartCommandResult
SecManStartCommand::TCPAuthCallback_inner( bool auth_succeeded, Sock *tcp_auth_sock )
{
	m_tcp_auth_command = NULL;

		// close the TCP socket, the rest will be UDP.
	tcp_auth_sock->end_of_message();
	tcp_auth_sock->close();
	delete tcp_auth_sock;
	tcp_auth_sock = NULL;

	StartCommandResult rc;

	if(m_nonblocking && !m_callback_fn) {
		// Caller wanted us to get a session key but did not
		// want to bother about handling a callback.  Therefore,
		// we are done.  No need to start the command again.
		rc = StartCommandWouldBlock;

		// NOTE: m_sock is expected to be NULL, because caller may
		// have deleted it by now.
		ASSERT( m_sock == NULL );
	}
	else if( !auth_succeeded ) {
		dprintf ( D_SECURITY,
				  "SECMAN: unable to create security session to %s via TCP, "
		          "failing.\n", m_sock->get_sinful_peer() );
		m_errstack->pushf("SECMAN", SECMAN_ERR_NO_SESSION,
		                 "Failed to create security session to %s with TCP.",
		                 m_sock->get_sinful_peer());
		rc = StartCommandFailed;
	}
	else {
		if( (IsDebugVerbose(D_SECURITY)) ) {
			dprintf ( D_SECURITY,
					  "SECMAN: succesfully created security session to %s via "
					  "TCP!\n", m_sock->get_sinful_peer() );
		}
		rc = startCommand_inner();
	}

		// Remove ourselves from SecMan's list of pending TCP auth sessions.
	classy_counted_ptr<SecManStartCommand> sc;
	if( SecMan::tcp_auth_in_progress->lookup(m_session_key,sc) == 0 &&
	    sc.get() == this )
	{
		ASSERT(SecMan::tcp_auth_in_progress->remove(m_session_key) == 0);
	}

		// Iterate through the list of objects waiting for our TCP auth session
		// to be done.
	m_waiting_for_tcp_auth.Rewind();
	while( m_waiting_for_tcp_auth.Next(sc) ) {
		sc->ResumeAfterTCPAuth(auth_succeeded);
	}
	m_waiting_for_tcp_auth.Clear();

	return rc;
}

void
SecManStartCommand::ResumeAfterTCPAuth(bool auth_succeeded)
{
		// We get here when we needed a session, and some other
		// instance of SecManStartCommand() was already in the process
		// of getting one that we could use.  When that object
		// finished getting the session, it called us here.

	if( IsDebugVerbose(D_SECURITY) ) {
		dprintf(D_SECURITY,"SECMAN: done waiting for TCP auth to %s (%s)\n",
		        m_sock->get_sinful_peer(),
				auth_succeeded ? "succeeded" : "failed");
	}
	if(!auth_succeeded) {
		m_errstack->pushf("SECMAN", SECMAN_ERR_NO_SESSION,
						  "Was waiting for TCP auth session to %s, "
						  "but it failed.",
						  m_sock->get_sinful_peer());
	}

	StartCommandResult rc;
	if(auth_succeeded) {
		rc = startCommand_inner();
	}
	else {
		rc = StartCommandFailed;
	}

	doCallback( rc );
}

StartCommandResult
SecManStartCommand::WaitForSocketCallback()
{

	if( m_sock->get_deadline() == 0 ) {
			// Set a deadline for completion of this non-blocking operation
			// and any that follow until StartCommand is done with it.
			// One reason to do this here rather than once at the beginning
			// of StartCommand is because m_sock->get_deadline() may return
			// non-zero while the non-blocking connect is happening and then
			// revert to 0 later.  Best to always check before Register_Socket.
		int TCP_SESSION_DEADLINE = param_integer("SEC_TCP_SESSION_DEADLINE",120);
		m_sock->set_deadline_timeout(TCP_SESSION_DEADLINE);
		m_sock_had_no_deadline = true; // so we restore deadline to 0 when done
	}

	MyString req_description;
	req_description.formatstr("SecManStartCommand::WaitForSocketCallback %s",
							m_cmd_description.Value());
	int reg_rc = daemonCoreSockAdapter.Register_Socket(
		m_sock,
		m_sock->peer_description(),
		(SocketHandlercpp)&SecManStartCommand::SocketCallback,
		req_description.Value(),
		this,
		ALLOW);

	if(reg_rc < 0) {
		MyString msg;
		msg.formatstr("StartCommand to %s failed because "
					"Register_Socket returned %d.",
					m_sock->get_sinful_peer(),
					reg_rc);
		dprintf(D_SECURITY, "SECMAN: %s\n", msg.Value());
		m_errstack->pushf("SECMAN", SECMAN_ERR_CONNECT_FAILED,
						  "%s", msg.Value());

		return StartCommandFailed;
	}

		// Do not allow ourselves to be deleted until after
		// SocketCallback is called.
	incRefCount();

	return StartCommandInProgress;
}

int
SecManStartCommand::SocketCallback( Stream *stream )
{
	daemonCoreSockAdapter.Cancel_Socket( stream );

		// NOTE: startCommand_inner() is responsible for checking
		// if our deadline had expired.
	doCallback( startCommand_inner() );

		// get rid of ref counted when callback was registered
	decRefCount();

	return KEEP_STREAM;
}


// Given a sinful string, clear out any associated sessions (incoming or outgoing)
void
SecMan::invalidateHost(const char * sin)
{
	StringList *keyids = session_cache->getKeysForPeerAddress(sin);
	if( !keyids ) {
		return;
	}

	keyids->rewind();
	char const *keyid;
	while( (keyid=keyids->next()) ) {
		if (IsDebugVerbose(D_SECURITY)) {
			dprintf (D_SECURITY, "KEYCACHE: removing session %s for %s\n", keyid, sin);
		}
		invalidateKey(keyid);
	}
	delete keyids;
}

void
SecMan::invalidateByParentAndPid(const char * parent, int pid) {
	StringList *keyids = session_cache->getKeysForProcess(parent,pid);
	if( !keyids ) {
		return;
	}

	keyids->rewind();
	char const *keyid;
	while( (keyid=keyids->next()) ) {
		if (IsDebugVerbose(D_SECURITY)) {
			dprintf (D_SECURITY, "KEYCACHE: removing session %s for %s pid %d\n", keyid, parent, pid);
		}
		invalidateKey(keyid);
	}
	delete keyids;
}

bool SecMan :: invalidateKey(const char * key_id)
{
    bool removed = true;
    KeyCacheEntry * keyEntry = NULL;

    // What if session_cache is NULL but command_cache is not?
	if (session_cache) {

        session_cache->lookup(key_id, keyEntry);

		if( keyEntry && keyEntry->expiration() <= time(NULL) ) {
			dprintf( D_SECURITY,
					 "DC_INVALIDATE_KEY: security session %s %s expired.\n",
					 key_id, keyEntry->expirationType() );
		}

        remove_commands(keyEntry);

        // Now, remove session id
		if (session_cache->remove(key_id)) {
			dprintf ( D_SECURITY, 
                      "DC_INVALIDATE_KEY: removed key id %s.\n", 
                      key_id);
		} else {
			dprintf ( D_SECURITY, 
                      "DC_INVALIDATE_KEY: ignoring request to invalidate non-existant key %s.\n", 
                      key_id);
		}
	} else {
		dprintf ( D_ALWAYS, 
                  "DC_INVALIDATE_KEY: did not remove %s, no KeyCache exists!\n", 
                  key_id);
	}

    return removed;
}

void SecMan :: remove_commands(KeyCacheEntry * keyEntry)
{
    if (keyEntry) {
        char * commands = NULL;
        keyEntry->policy()->LookupString(ATTR_SEC_VALID_COMMANDS, &commands);
		MyString addr;
		if (keyEntry->addr())
			addr = keyEntry->addr()->to_sinful();
    
        // Remove all commands from the command map
        if (commands) {
            char keybuf[128];
            StringList cmd_list(commands);
            free(commands);
        
            if (command_map) {
                cmd_list.rewind();
                char * cmd = NULL;
                while ( (cmd = cmd_list.next()) ) {
                    memset(keybuf, 0, 128);
                    sprintf (keybuf, "{%s,<%s>}", addr.Value(), cmd);
                    command_map->remove(keybuf);
                }
            }
        }
    }
}

int
SecMan::sec_char_to_auth_method( char* method ) {
    if (!strcasecmp( method, "SSL" )  ) {
        return CAUTH_SSL;
    } else if (!strcasecmp( method, "GSI" )  ) {
		return CAUTH_GSI;
	} else if ( !strcasecmp( method, "NTSSPI" ) ) {
		return CAUTH_NTSSPI;
	} else if ( !strcasecmp( method, "PASSWORD" ) ) {
		return CAUTH_PASSWORD;
	} else if ( !strcasecmp( method, "FS" ) ) {
		return CAUTH_FILESYSTEM;
	} else if ( !strcasecmp( method, "FS_REMOTE" ) ) {
		return CAUTH_FILESYSTEM_REMOTE;
	} else if ( !strcasecmp( method, "KERBEROS" ) ) {
		return CAUTH_KERBEROS;
	} else if ( !strcasecmp( method, "CLAIMTOBE" ) ) {
		return CAUTH_CLAIMTOBE;
	} else if ( !strcasecmp( method, "ANONYMOUS" ) ) {
		return CAUTH_ANONYMOUS;
	}
	return 0;
}


int
SecMan::getAuthBitmask ( const char * methods ) {

	if (!methods || !*methods) {
		return 0;
	}

	StringList server( methods );
	char *tmp = NULL;
	int retval = 0;

	server.rewind();
	while ( (tmp = server.next()) ) {
		retval |= sec_char_to_auth_method(tmp);
	}

	return retval;
}



MyString
SecMan::ReconcileMethodLists( char * cli_methods, char * srv_methods ) {

	// algorithm:
	// step through the server's methods in order.  if the method is
	// present in the clients list, then append it to the results.
	// the output will be a list of methods supported by both, in the
	// order that the server prefers.

	StringList server_methods( srv_methods );
	StringList client_methods( cli_methods );
	char *sm = NULL;
	char *cm = NULL;

	MyString results;
	int match = 0;

	// server methods, one at a time
	server_methods.rewind();
	while ( (sm = server_methods.next()) ) {
		client_methods.rewind();
		while ( (cm = client_methods.next()) ) {
			if (!strcasecmp(sm, cm)) {
				// add a comma if it isn't the first match
				if (match) {
					results += ",";
				} else {
					match = 1;
				}

				// and of course, append the common method
				results += cm;
			}
		}
	}

	return results;
}


SecMan::SecMan(int nbuckets)
{
	if ( NULL == m_ipverify ) {
		m_ipverify = new IpVerify( );
	}
	// session_cache is a static member... we only
	// want to construct it ONCE.
	if (session_cache == NULL) {
		session_cache = new KeyCache(nbuckets);
	}
	if (command_map == NULL) {
		command_map = new HashTable<MyString,MyString>(nbuckets, MyStringHash, updateDuplicateKeys);
	}
	if (tcp_auth_in_progress == NULL) {
		tcp_auth_in_progress = new HashTable<MyString,classy_counted_ptr<SecManStartCommand> >(256, MyStringHash, rejectDuplicateKeys);
	}
	sec_man_ref_count++;
}


SecMan::SecMan(const SecMan & /* copy */) {
	// session_cache is static.  if there's a copy, it
	// should already have been constructed.
	ASSERT (session_cache);
	ASSERT (command_map);
	ASSERT (tcp_auth_in_progress);
	sec_man_ref_count++;
}

const SecMan & SecMan::operator=(const SecMan & /* copy */) {
	// session_cache is static.  if there's a copy, it
	// should already have been constructed.
	ASSERT (session_cache);
	ASSERT (command_map);
	return *this;
}


SecMan::~SecMan() {
	// session_cache is static.  if we are in a destructor,
	// then these should already have been constructed.
	ASSERT (session_cache);
	ASSERT (command_map);

	sec_man_ref_count--;

	/*
	// if that was the last one to go, we could delete the objects
	if (sec_man_ref_count == 0) {
		delete session_cache;
		session_cache = NULL;

		delete command_map;
		command_map = NULL;

		delete tcp_auth_in_progress;
		tcp_aut_in_progress = NULL;
	}
	*/
}

void
SecMan::reconfig()
{
	m_ipverify->reconfig();
	Authentication::reconfigMapFile();
}

IpVerify *
SecMan::getIpVerify()
{
	return m_ipverify;
}

int
SecMan::Verify(DCpermission perm, const condor_sockaddr& addr, const char * fqu, MyString *allow_reason, MyString *deny_reason )
{
	IpVerify *ipverify = getIpVerify();
	ASSERT( ipverify );
	return ipverify->Verify(perm,addr,fqu,allow_reason,deny_reason);
}


bool
SecMan::sec_copy_attribute( ClassAd &dest, ClassAd &source, const char* attr ) {
	ExprTree *e = source.LookupExpr(attr);
	if (e) {
		ExprTree *cp = e->Copy();
		dest.Insert(attr,cp,false);
		return true;
	} else {
		return false;
	}
}

bool
SecMan::sec_copy_attribute( ClassAd &dest, const char *to_attr, ClassAd &source, const char *from_attr ) {
	ExprTree *e = source.LookupExpr(from_attr);
	if (!e) {
		return false;
	}

	e = e->Copy();
	bool retval = dest.Insert(to_attr, e, false) != 0;
	return retval;
}


void
SecMan::invalidateAllCache() {
	delete session_cache;
	session_cache = new KeyCache(209);

	delete command_map;
	command_map = new HashTable<MyString,MyString>(209, MyStringHash, updateDuplicateKeys);
}

void 
SecMan :: invalidateExpiredCache()
{
    // Go through all cache and invalide the ones that are expired
    StringList * list = session_cache->getExpiredKeys();

    // The current session cache, command map does not allow
    // easy random access based on host direcly. Therefore,
    // we need to decide which list to be the outerloop
    // In this case, I assume the command map will be bigger
    // so, outloop will be command map, inner loop will be host

    list->rewind();
    char * p;
    while ( (p = list->next()) ) {
        invalidateKey(p);
    }
    delete list;
}

/*

			// a failure here signals that the cache may be invalid.
			// delete this entry from table and force normal auth.
			KeyCacheEntry * ek = NULL;
			if (session_cache->lookup(keybuf, ek) == 0) {
				delete ek;
			} else {
				dprintf (D_SECURITY, "SECMAN: unable to delete KeyCacheEntry.\n");
			}
			session_cache->remove(keybuf);
			m_have_session = false;

			// close this connection and start a new one
			if (!sock->close()) {
				dprintf ( D_ALWAYS, "SECMAN: could not close socket to %s\n",
						sin_to_string(sock->peer_addr()));
				return false;
			}

			KeyInfo* nullp = 0;
			if (!sock->set_crypto_key(false, nullp)) {
				dprintf ( D_ALWAYS, "SECMAN: could not re-init crypto!\n");
				return false;
			}
			if (!sock->set_MD_mode(MD_OFF, nullp)) {
				dprintf ( D_ALWAYS, "SECMAN: could not re-init MD5!\n");
				return false;
			}
			if (!sock->connect(sock->get_connect_addr(), 0)) {
				dprintf ( D_ALWAYS, "SECMAN: could not reconnect to %s.\n",
						sin_to_string(sock->peer_addr()));
				return false;
			}

			goto choose_action;
*/


MyString SecMan::getDefaultAuthenticationMethods() {
	MyString methods;
#if defined(WIN32)
	// default windows method
	methods = "NTSSPI";
#else
	// default unix method
	methods = "FS";
#endif

#if defined(HAVE_EXT_KRB5) 
	methods += ",KERBEROS";
#endif

#if defined(HAVE_EXT_GLOBUS)
	methods += ",GSI";
#endif

	return methods;
}



MyString SecMan::getDefaultCryptoMethods() {
#ifdef HAVE_EXT_OPENSSL
	return "3DES,BLOWFISH";
#else
	return "";
#endif
}

char* SecMan::my_unique_id() {

    if (!_my_unique_id) {
        // first time we were called, construct the unique ID
        // in member variable _my_unique_id

        int    mypid = 0;

#ifdef WIN32
        mypid = ::GetCurrentProcessId();
#else
        mypid = ::getpid();
#endif

        MyString tid;
        tid.formatstr( "%s:%i:%i", get_local_hostname().Value(), mypid, 
					 (int)time(0));

        _my_unique_id = strdup(tid.Value());
    }

    return _my_unique_id;

}

bool SecMan::set_parent_unique_id(const char* value) {
	if (_my_parent_unique_id) {
		free (_my_parent_unique_id);
		_my_parent_unique_id = NULL;
	}

	// if the value is explicitly set using this method,
	// do not check the environment for it, even if we
	// set it to NULL
	_should_check_env_for_unique_id = false;

	if (value && value[0]) {
		_my_parent_unique_id = strdup(value);
	}

	return (_my_parent_unique_id != NULL);
}

char* SecMan::my_parent_unique_id() {
	if (_should_check_env_for_unique_id) {
		// we only check in the environment once
		_should_check_env_for_unique_id = false;

		// look in the env for ENV_PARENT_ID
		const char* envName = EnvGetName ( ENV_PARENT_ID );
		MyString value;
		GetEnv( envName, value );

		if (value.Length()) {
			set_parent_unique_id(value.Value());
		}
	}

	return _my_parent_unique_id;
}

int
SecMan::authenticate_sock(Sock *s,DCpermission perm, CondorError* errstack)
{
	MyString methods;
	getAuthenticationMethods( perm, &methods );
	ASSERT(s);
	int auth_timeout = getSecTimeout(perm);
	return s->authenticate(methods.Value(),errstack,auth_timeout);
}

int
SecMan::authenticate_sock(Sock *s,KeyInfo *&ki, DCpermission perm, CondorError* errstack)
{
	MyString methods;
	getAuthenticationMethods( perm, &methods );
	ASSERT(s);
	int auth_timeout = getSecTimeout(perm);
	return s->authenticate(ki,methods.Value(),errstack,auth_timeout);
}

int
SecMan::getSecTimeout(DCpermission perm)
{
	int auth_timeout = -1;
	getIntSecSetting(auth_timeout,"SEC_%s_AUTHENTICATION_TIMEOUT",perm);
	return auth_timeout;
}

Protocol CryptProtocolNameToEnum(char const *name) {
	switch (toupper(*name)) {
	case 'B': // blowfish
		return CONDOR_BLOWFISH;
	case '3': // 3des
	case 'T': // Tripledes
		return CONDOR_3DES;
	default:
		return CONDOR_NO_PROTOCOL;
	}
}

bool
SecMan::CreateNonNegotiatedSecuritySession(DCpermission auth_level, char const *sesid,char const *private_key,char const *exported_session_info,char const *peer_fqu, char const *peer_sinful, int duration)
{
	ClassAd policy;

	ASSERT(sesid);

	condor_sockaddr peer_addr;
	if(peer_sinful && !peer_addr.from_sinful(peer_sinful)) {
		dprintf(D_ALWAYS,"SECMAN: failed to create non-negotiated security session %s because"
				"string_to_sin(%s) failed\n",sesid,peer_sinful);
		return false;
	}

	FillInSecurityPolicyAd( auth_level, &policy, false );

		// Make sure security negotiation is turned on within this
		// security session.  If it is not, we will just use the raw
		// CEDAR command protocol, which defeats the whole purpose of
		// having a security session.
	policy.Assign(ATTR_SEC_NEGOTIATION,SecMan::sec_req_rev[SEC_REQ_REQUIRED]);

	ClassAd *auth_info = ReconcileSecurityPolicyAds(policy,policy);
	if(!auth_info) {
		dprintf(D_ALWAYS,"SECMAN: failed to create non-negotiated security session %s because"
				"ReconcileSecurityPolicyAds() failed.\n",sesid);
		return false;
	}
	sec_copy_attribute(policy,*auth_info,ATTR_SEC_AUTHENTICATION);
	sec_copy_attribute(policy,*auth_info,ATTR_SEC_INTEGRITY);
	sec_copy_attribute(policy,*auth_info,ATTR_SEC_ENCRYPTION);
	sec_copy_attribute(policy,*auth_info,ATTR_SEC_CRYPTO_METHODS);

		// remove all but the first crypto method
	MyString crypto_methods;
	policy.LookupString(ATTR_SEC_CRYPTO_METHODS,crypto_methods);
	if( crypto_methods.Length() ) {
		int pos = crypto_methods.FindChar(',');
		if( pos >= 0 ) {
			crypto_methods.setChar(pos,'\0');
			policy.Assign(ATTR_SEC_CRYPTO_METHODS,crypto_methods);
		}
	}

	delete auth_info;
	auth_info = NULL;

	if( !ImportSecSessionInfo(exported_session_info,policy) ) {
		return false;
	}

	policy.Assign(ATTR_SEC_USE_SESSION, "YES");
	policy.Assign(ATTR_SEC_SID, sesid);
	policy.Assign(ATTR_SEC_ENACT, "YES");

	if( peer_fqu ) {
		policy.Assign(ATTR_SEC_AUTHENTICATION, SecMan::sec_feat_act_rev[SEC_FEAT_ACT_NO]);
		policy.Assign(ATTR_SEC_TRIED_AUTHENTICATION,true);
		policy.Assign(ATTR_SEC_USER,peer_fqu);
	}


	MyString crypto_method;
	policy.LookupString(ATTR_SEC_CRYPTO_METHODS, crypto_method);

	Protocol crypt_protocol = CryptProtocolNameToEnum(crypto_method.Value());
	const int keylen = MAC_SIZE;
	unsigned char* keybuf = Condor_Crypt_Base::oneWayHashKey(private_key);
	if(!keybuf) {
		dprintf(D_ALWAYS,"SECMAN: failed to create non-negotiated security session %s because"
				" oneWayHashKey() failed.\n",sesid);
		return false;
	}
	KeyInfo *keyinfo = new KeyInfo(keybuf,keylen,crypt_protocol);
	free( keybuf );
	keybuf = NULL;

		// extract the session duration from the (imported) policy
	int expiration_time = 0;

	if( policy.LookupInteger(ATTR_SEC_SESSION_EXPIRES,expiration_time) ) {
		duration = expiration_time ? expiration_time - time(NULL) : 0;
		if( duration < 0 ) {
			dprintf(D_ALWAYS,"SECMAN: failed to create non-negotiated security session %s because duration = %d\n",sesid,duration);
			delete keyinfo;
			return false;
		}
	}
	else if( duration > 0 ) {
		expiration_time = time(NULL) + duration;
			// store this in the policy so that when we export session info,
			// it is there
		policy.Assign(ATTR_SEC_SESSION_EXPIRES,expiration_time);
	}

	KeyCacheEntry key(sesid,peer_sinful ? &peer_addr : NULL,keyinfo,&policy,expiration_time,0);

	if( !session_cache->insert(key) ) {
		KeyCacheEntry *existing = NULL;
		bool fixed = false;
		if( !session_cache->lookup(sesid,existing) ) {
			existing = NULL;
		}
		if( existing ) {
			if( !LookupNonExpiredSession(sesid,existing) ) {
					// the existing session must have expired, so try again
				existing = NULL;
				if( session_cache->insert(key) ) {
					fixed = true;
				}
			}
			else if( existing && existing->getLingerFlag() ) {
				dprintf(D_ALWAYS,"SECMAN: removing lingering non-negotiated security session %s because it conflicts with new request\n",sesid);
				session_cache->expire(existing);
				existing = NULL;
				if( session_cache->insert(key) ) {
					fixed = true;
				}
			}
		}

		if( !fixed ) {
			dprintf(D_ALWAYS, "SECMAN: failed to create session %s%s.\n",
					sesid,
					existing ? " (key already exists)" : "");
			ClassAd *existing_policy = existing ? existing->policy() : NULL;
			if( existing_policy ) {
				dprintf(D_ALWAYS,"SECMAN: existing session %s:\n", sesid);
				existing_policy->dPrint(D_SECURITY);
			}
			delete keyinfo;
			return false;
		}
	}

	dprintf(D_SECURITY, "SECMAN: created non-negotiated security session %s for %d %sseconds."
			"\n", sesid, duration, expiration_time == 0 ? "(inf) " : "");

	if( IsDebugVerbose(D_SECURITY) ) {
		if( exported_session_info ) {
			dprintf(D_SECURITY,"Imported session attributes: %s\n",
					exported_session_info);
		}
		dprintf(D_SECURITY,"Caching non-negotiated security session ad:\n");
		policy.dPrint(D_SECURITY);
	}

	delete keyinfo;
	return true;
}

bool
SecMan::ImportSecSessionInfo(char const *session_info,ClassAd &policy) {
		// expected format for session_info is the format produced by
		// ExportSecSessionInfo()
		// [param1=val1; param2=val2; ... ]
		// To keep things simple, no parameters or values may contain ';'

	if( !session_info || !*session_info) {
		return true; // no exported session info
	}

	MyString buf = session_info+1;

		// verify that the string is contained in []'s
	if( session_info[0]!='[' || buf[buf.Length()-1]!=']' ) {
		dprintf( D_ALWAYS, "ImportSecSessionInfo: invalid session info: %s\n",
				session_info );
		return false;
	}

		// get rid of final ']'
	buf.setChar(buf.Length()-1,'\0');

	StringList lines(buf.Value(),";");
	lines.rewind();

	char const *line;
	ClassAd imp_policy;
	while( (line=lines.next()) ) {
		if( !imp_policy.Insert(line) ) {
			dprintf( D_ALWAYS, "ImportSecSessionInfo: invalid imported session info: '%s' in %s\n", line, session_info );
			return false;
		}
	}

		// We could have just blindly inserted everything into our policy,
		// but for safety, we explicitly copy over specific attributes
		// from imp_policy to our policy.

	sec_copy_attribute(policy,imp_policy,ATTR_SEC_INTEGRITY);
	sec_copy_attribute(policy,imp_policy,ATTR_SEC_ENCRYPTION);
	sec_copy_attribute(policy,imp_policy,ATTR_SEC_CRYPTO_METHODS);
	sec_copy_attribute(policy,imp_policy,ATTR_SEC_SESSION_EXPIRES);

	return true;
}

bool
SecMan::ExportSecSessionInfo(char const *session_id,MyString &session_info) {
	ASSERT( session_id );

	KeyCacheEntry *session_key = NULL;
	if(!session_cache->lookup(session_id,session_key)) {
		dprintf(D_ALWAYS,"SECMAN: ExportSecSessionInfo failed to find "
				"session %s\n",session_id);
		return false;
	}
	ClassAd *policy = session_key->policy();
	ASSERT( policy );

	ClassAd exp_policy;
	sec_copy_attribute(exp_policy,*policy,ATTR_SEC_INTEGRITY);
	sec_copy_attribute(exp_policy,*policy,ATTR_SEC_ENCRYPTION);
	sec_copy_attribute(exp_policy,*policy,ATTR_SEC_CRYPTO_METHODS);
	sec_copy_attribute(exp_policy,*policy,ATTR_SEC_SESSION_EXPIRES);

	session_info += "[";
	exp_policy.ResetExpr();
	const char *name;
	ExprTree *elem;
	while( exp_policy.NextExpr(name, elem) ) {
			// In the following, we attempt to avoid any spaces in the
			// result string.  However, no code should depend on this.
		session_info += name;
		session_info += "=";

        const char *line = ExprTreeToString(elem);

			// none of the ClassAd values should ever contain ';'
			// that makes things easier in ImportSecSessionInfo()
		ASSERT( strchr(line,';') == NULL );

		session_info += line;
		session_info += ";";
    }
	session_info += "]";

	dprintf(D_SECURITY,"SECMAN: exporting session info for %s: %s\n",
			session_id, session_info.Value());
	return true;
}

bool
SecMan::SetSessionExpiration(char const *session_id,time_t expiration_time) {
	ASSERT( session_id );

	KeyCacheEntry *session_key = NULL;
	if(!session_cache->lookup(session_id,session_key)) {
		dprintf(D_ALWAYS,"SECMAN: SetSessionExpiration failed to find "
				"session %s\n",session_id);
		return false;
	}
	session_key->setExpiration(expiration_time);

	dprintf(D_SECURITY,"Set expiration time for security session %s to %ds\n",session_id,(int)(expiration_time-time(NULL)));

	return true;
}

bool
SecMan::SetSessionLingerFlag(char const *session_id) {
	ASSERT( session_id );

	KeyCacheEntry *session_key = NULL;
	if(!session_cache->lookup(session_id,session_key)) {
		dprintf(D_ALWAYS,"SECMAN: SetSessionLingerFlag failed to find "
				"session %s\n",session_id);
		return false;
	}
	session_key->setLingerFlag(true);

	return true;
}
