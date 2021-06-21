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
#include "subsystem_info.h"
#include "setenv.h"
#include "ipv6_hostname.h"
#include "condor_auth_passwd.h"
#include "condor_auth_ssl.h"

#include <sstream>
#include <algorithm>
#include <string>

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

KeyCache SecMan::m_default_session_cache;
std::map<std::string,KeyCache*> *SecMan::m_tagged_session_cache = NULL;
std::string SecMan::m_tag;
std::string SecMan::m_token;
std::map<DCpermission, std::string> SecMan::m_tag_methods;
std::string SecMan::m_tag_token_owner;
KeyCache *SecMan::session_cache = &SecMan::m_default_session_cache;
std::string SecMan::m_pool_password;
HashTable<std::string,std::string> SecMan::command_map(hashFunction);
HashTable<std::string,classy_counted_ptr<SecManStartCommand> > SecMan::tcp_auth_in_progress(hashFunction);
int SecMan::sec_man_ref_count = 0;
std::set<std::string> SecMan::m_not_my_family;
char* SecMan::_my_unique_id = 0;
char* SecMan::_my_parent_unique_id = 0;
bool SecMan::_should_check_env_for_unique_id = true;
IpVerify *SecMan::m_ipverify = NULL;
classad::References SecMan::m_resume_proj;

// Forward dec'l; this was previously a SecMan method but not hidden here to discourage
// its use; in all cases, an external caller should use SecMan::getAuthenticationMethods
// instead.
static std::string getDefaultAuthenticationMethods(DCpermission perm);

void
SecMan::setTag(const std::string &tag) {

	if (tag != m_tag) {
		m_tag_token_owner = "";
		m_tag_methods.clear();
	}

        m_tag = tag;
	if (tag.size() == 0) {
		session_cache = &m_default_session_cache;
		return;
	}
	if (m_tagged_session_cache == NULL) {
		m_tagged_session_cache = new std::map<std::string, KeyCache*>();
	}
	std::map<std::string, KeyCache*>::const_iterator iter = m_tagged_session_cache->find(tag);
	KeyCache *tmp = NULL;
	if (iter == m_tagged_session_cache->end()) {
		tmp = new KeyCache();
		m_tagged_session_cache->insert(std::make_pair(tag, tmp));
	} else {
		tmp = iter->second;
	}
	session_cache = tmp;
}


void
SecMan::setTagAuthenticationMethods(DCpermission perm, const std::vector<std::string> &methods)
{
	std::stringstream ss;
	bool first = true;
	for (const auto &method : methods) {
		if (first) first = false;
		else ss << ",";
		ss << method;
	}
	m_tag_methods[perm] = ss.str();
}


const std::string
SecMan::getTagAuthenticationMethods(DCpermission perm)
{
	const auto iter = m_tag_methods.find(perm);
	if (iter == m_tag_methods.end()) {
		return "";
	}
	return iter->second;
}


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
SecMan::sec_lookup_feat_act( const ClassAd &ad, const char* pname ) {

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
SecMan::sec_lookup_act( const ClassAd &ad, const char* pname ) {

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
SecMan::sec_lookup_req( const ClassAd &ad, const char* pname ) {

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
				EXCEPT( "SECMAN: %s=%s is invalid!",
				        param_name.c_str(), value ? value : "(null)" );
			}
			if( IsDebugVerbose(D_SECURITY) ) {
				dprintf (D_SECURITY,
				         "SECMAN: %s is undefined; using %s.\n",
				         param_name.c_str(), SecMan::sec_req_rev[def]);
			}
			free(value);

			return def;
		}

		return res;
	}

	return def;
}


	// Determine the valid authentication methods for the current process.  Order of
	// preference is:
	// 1. Methods explicitly set in the current security tag (typically a developer
	//    override of settings).
	// 2. The setting in SEC_<perm>_AUTHENTICATION_METHODS.
	// 3. The default parameters for the permission level.
	// Additionally, (2) and (3) are filtered to ensure they are valid.
std::string
SecMan::getAuthenticationMethods(DCpermission perm) {

	auto methods = getTagAuthenticationMethods(perm);
	if (!methods.empty()) {
		return methods;
	}

	std::unique_ptr<char, decltype(&free)> config_methods(getSecSetting ("SEC_%s_AUTHENTICATION_METHODS", perm), free);

	if (!config_methods) {
		methods = getDefaultAuthenticationMethods(perm);
	} else {
		methods = std::string(config_methods.get());
	}

	return filterAuthenticationMethods(perm, methods);
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
				found = param_integer( buf.c_str(), *int_result, false, 0, false, 0, 0 );
			}
			else {
				*str_result = param( buf.c_str() );
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
			found = param_integer( buf.c_str(), *int_result, false, 0, false, 0, 0 );
		}
		else {
			*str_result = param( buf.c_str() );
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


void
SecMan::UpdateAuthenticationMetadata(ClassAd &ad)
{
	// We need the trust domain for TOKEN auto-request, but auto-request
	// happens if and only if TOKEN isn't in the method list (indicating
	// that we didn't have one).
	std::string issuer;
	if (param(issuer, "TRUST_DOMAIN")) {
		issuer = issuer.substr(0, issuer.find_first_of(", \t"));
		ad.InsertAttr(ATTR_SEC_TRUST_DOMAIN, issuer);
	}

	std::string method_list_str;
	if (!ad.EvaluateAttrString(ATTR_SEC_AUTHENTICATION_METHODS, method_list_str)) {
		return;
	}
	StringList  method_list( method_list_str.c_str() );
	const char *method;

	method_list.rewind();
	while ( (method = method_list.next()) ) {
		if (!strcmp(method, "TOKEN") || !strcmp(method, "TOKENS") ||
			!strcmp(method, "IDTOKEN") || !strcmp(method, "IDTOKENS"))
		{
			Condor_Auth_Passwd::preauth_metadata(ad);
		}
	}
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

	// auth methods
	auto methods = getAuthenticationMethods(auth_level);
	if (!methods.empty()) {
		ad->Assign (ATTR_SEC_AUTHENTICATION_METHODS, methods.c_str());

		// Some methods may need to insert additional metadata into this ad
		// in order for the client & server to determine if they can be used.
		UpdateAuthenticationMetadata(*ad);
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
	auto crypto_method_raw = SecMan::getSecSetting("SEC_%s_CRYPTO_METHODS", auth_level);
	std::string crypto_method = crypto_method_raw ? crypto_method_raw : SecMan::getDefaultCryptoMethods();
	free(crypto_method_raw);

	crypto_method = SecMan::filterCryptoMethods(crypto_method);

	if (!crypto_method.empty()) {
		ad->Assign (ATTR_SEC_CRYPTO_METHODS, crypto_method);
	} else {
		if( sec_encryption == SEC_REQ_REQUIRED || 
			sec_integrity == SEC_REQ_REQUIRED ) {
			dprintf( D_SECURITY, "SECMAN: no crypto methods, "
					 "but it was required! failing...\n" );
			return false;
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
	ad->Assign ( ATTR_SEC_SESSION_DURATION, std::to_string(session_duration) );

	int session_lease = 3600;
	SecMan::getIntSecSetting(session_lease, "SEC_%s_SESSION_LEASE", auth_level);
	ad->Assign( ATTR_SEC_SESSION_LEASE, session_lease );

	return true;
}

// Fill'ing in the security policy ad is very expensive, sometimes requiring
// 400 param calls.  Usually, we call it with the same parameters over and over,
// so keep track of the last set of parameters, and cache the last one returned.

bool
SecMan::FillInSecurityPolicyAdFromCache(DCpermission auth_level, ClassAd* &ad, 
								bool raw_protocol,
								bool use_tmp_sec_session,
								bool force_authentication )
{
	if ((m_cached_auth_level == auth_level) &&
		(m_cached_raw_protocol == raw_protocol) &&
		(m_cached_use_tmp_sec_session == use_tmp_sec_session) &&
		(m_cached_force_authentication == force_authentication)) {

			// A Hit!
		if (m_cached_return_value) {
			ad = &m_cached_policy_ad;

		}
		return m_cached_return_value;
	}	

		// Miss...
	m_cached_auth_level = auth_level;
	m_cached_raw_protocol = raw_protocol;
	m_cached_use_tmp_sec_session = use_tmp_sec_session;
	m_cached_force_authentication = force_authentication;
	
	m_cached_policy_ad.Clear(); 
	m_cached_return_value = FillInSecurityPolicyAd(auth_level, &m_cached_policy_ad, raw_protocol, use_tmp_sec_session, force_authentication);
	ad = & m_cached_policy_ad;
	return m_cached_return_value;
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
								   const ClassAd &cli_ad, const ClassAd &srv_ad,
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
SecMan::ReconcileSecurityPolicyAds(const ClassAd &cli_ad, const ClassAd &srv_ad) {

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

	action_ad->Assign(ATTR_SEC_AUTHENTICATION, SecMan::sec_feat_act_rev[authentication_action]);

	if( authentication_action == SecMan::SEC_FEAT_ACT_YES ) {
			// record whether the authentication is required or not, so
			// both parties know what to expect if authentication fails
		if( !auth_required ) {
				// this is assumed to be true if not set
			action_ad->Assign(ATTR_SEC_AUTH_REQUIRED,false);
		}
	}

	action_ad->Assign(ATTR_SEC_ENCRYPTION, SecMan::sec_feat_act_rev[encryption_action]);

	action_ad->Assign(ATTR_SEC_INTEGRITY, SecMan::sec_feat_act_rev[integrity_action]);


	char* cli_methods = NULL;
	char* srv_methods = NULL;
	if (cli_ad.LookupString( ATTR_SEC_AUTHENTICATION_METHODS, &cli_methods) &&
		srv_ad.LookupString( ATTR_SEC_AUTHENTICATION_METHODS, &srv_methods)) {

		// send the list for 6.5.0 and higher
		std::string the_methods = ReconcileMethodLists( cli_methods, srv_methods );
		action_ad->Assign(ATTR_SEC_AUTHENTICATION_METHODS_LIST, the_methods);

		// send the single method for pre 6.5.0
		StringList  tmpmethodlist( the_methods.c_str() );
		char* first;
		tmpmethodlist.rewind();
		first = tmpmethodlist.next();
		if (first) {
			action_ad->Assign(ATTR_SEC_AUTHENTICATION_METHODS, first);
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

		std::string the_methods = ReconcileMethodLists( cli_methods, srv_methods );
		action_ad->Assign(ATTR_SEC_CRYPTO_METHODS, the_methods);
		action_ad->Assign(ATTR_SEC_CRYPTO_METHODS_LIST, the_methods);
			// AES-GCM will always internally encrypt and do integrity-checking,
			// regardless of what was negotiated.  Might as well say that in the
			// ad so the user is aware!
		if (SEC_FEAT_ACT_YES == authentication_action && "AES" == the_methods.substr(0, the_methods.find(','))) {
			action_ad->Assign(ATTR_SEC_ENCRYPTION, "YES");
			action_ad->Assign(ATTR_SEC_INTEGRITY, "YES");
		}
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

	action_ad->Assign(ATTR_SEC_SESSION_DURATION, std::to_string((cli_duration < srv_duration) ? cli_duration : srv_duration));


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


	action_ad->Assign(ATTR_SEC_ENACT, "YES");

	UpdateAuthenticationMetadata(*action_ad);
	std::string trust_domain;
	if (srv_ad.EvaluateAttrString(ATTR_SEC_TRUST_DOMAIN, trust_domain)) {
		action_ad->InsertAttr(ATTR_SEC_TRUST_DOMAIN, trust_domain);
	}
	std::string issuer_keys;
	if (srv_ad.EvaluateAttrString(ATTR_SEC_ISSUER_KEYS, issuer_keys)) {
		action_ad->InsertAttr(ATTR_SEC_ISSUER_KEYS, issuer_keys);
	}

	return action_ad;

}

class SecManStartCommand: Service, public ClassyCountedPtr {
 public:
	SecManStartCommand (
		int cmd,Sock *sock,bool raw_protocol,
		CondorError *errstack,int subcmd,StartCommandCallbackType *callback_fn,
		void *misc_data,bool nonblocking,char const *cmd_description,char const *sec_session_id_hint,
		const std::string &owner, const std::vector<std::string> &methods, SecMan *sec_man):

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
		m_use_tmp_sec_session(false),
		m_owner(owner),
		m_methods(methods)
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
				formatstr(m_cmd_description,"command %d",m_cmd);
			}
		}
		m_already_logged_startcommand = false;
		m_negotiation = SecMan::SEC_REQ_UNDEFINED;

		m_sock_had_no_deadline = false;
	}

	~SecManStartCommand() {
		if( m_private_key ) {
			delete m_private_key;
			m_private_key = NULL;
		}

			// we may be called after the daemonCore object
			// has been destructed and nulled out in a 
			// shutdown.  If so, just return now.

		if (!daemonCore) {
			return;
		}

		if( m_pending_socket_registered ) {
			m_pending_socket_registered = false;
			daemonCore->decrementPendingSockets();
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
			daemonCore->incrementPendingSockets();
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
	std::string m_cmd_description;
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

	std::string m_session_key; // "addr,<cmd>"
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
	std::string m_remote_version;
	KeyCacheEntry *m_enc_key;
	KeyInfo* m_private_key;
	std::string m_sec_session_id_hint;
	std::string m_owner;
	std::vector<std::string> m_methods;

	enum StartCommandState {
		SendAuthInfo,
		ReceiveAuthInfo,
		Authenticate,
		AuthenticateContinue,
		AuthenticateFinish,
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
	StartCommandResult authenticate_inner_continue();
	StartCommandResult authenticate_inner_finish();
	StartCommandResult receivePostAuthInfo_inner();

		// This is called when the TCP auth attempt completes.
	static void TCPAuthCallback(bool success,Sock *sock,CondorError *errstack, const std::string &trust_domain, bool should_try_token_request, void *misc_data);

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
SecMan::startCommand(const StartCommandRequest &req)
{
	// This function is simply a convenient wrapper around the
	// SecManStartCommand class, which does the actual work.

	// Initialize IpVerify, if necessary, before starting any network
	// communications.
	m_ipverify->Init();

	// If this is nonblocking, we must create the following on the heap.
	// The blocking case could avoid use of the heap, but for simplicity,
	// we just do the same in both cases.

	classy_counted_ptr<SecManStartCommand> sc = new SecManStartCommand(
		req.m_cmd,
		req.m_sock,
		req.m_raw_protocol,
		req.m_errstack,
		req.m_subcmd,
		req.m_callback_fn,
		req.m_misc_data,
		req.m_nonblocking,
		req.m_cmd_description,
		req.m_sec_session_id,
		req.m_owner,
		req.m_methods,
		this);

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

		std::string allow_reason;
		std::string deny_reason;

		int authorized = m_sec_man.Verify(
			CLIENT_PERM,
			m_sock->peer_addr(),
			server_fqu,
			allow_reason,
			deny_reason );

		if( authorized != USER_AUTH_SUCCESS ) {
			m_errstack->pushf("SECMAN", SECMAN_ERR_CLIENT_AUTH_FAILED,
			         "DENIED authorization of server '%s/%s' (I am acting as "
			         "the client): reason: %s.",
					 server_fqu ? server_fqu : "*",
					 m_sock->peer_ip_str(), deny_reason.c_str() );
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
		(*m_callback_fn)(success,m_sock,cb_errstack, m_sock->getTrustDomain(),
			m_sock->shouldTryTokenRequest(), m_misc_data);

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
	std::string old_owner;
		// Reset tag on function exit.
	std::shared_ptr<int> x(nullptr,
		[&](int *){ if (!m_owner.empty()) SecMan::setTag(old_owner); });
	if (!m_owner.empty()) {
		old_owner = SecMan::getTag();
		SecMan::setTag(m_owner);
		if (!m_methods.empty()) {
			SecMan::setTagAuthenticationMethods(CLIENT_PERM, m_methods);
		}
		SecMan::setTagCredentialOwner(m_owner);
	}

	ASSERT(m_sock);
	ASSERT(m_errstack);

	dprintf ( D_SECURITY, "SECMAN: %scommand %i %s to %s from %s port %i (%s%s).\n",
			  m_already_logged_startcommand ? "resuming " : "",
			  m_cmd,
			  m_cmd_description.c_str(),
			  m_sock->peer_description(),
			  m_is_tcp ? "TCP" : "UDP",
			  m_sock->get_port(),
			  m_nonblocking ?  "non-blocking" : "blocking",
			  m_raw_protocol ? ", raw" : "");

	m_already_logged_startcommand = true;


	if( m_sock->deadline_expired() ) {
		std::string msg;
		formatstr(msg, "deadline for %s %s has expired.",
					m_is_tcp && !m_sock->is_connected() ?
					"connection to" : "security handshake with",
					m_sock->peer_description());
		dprintf(D_SECURITY,"SECMAN: %s\n", msg.c_str());
		m_errstack->pushf("SECMAN", SECMAN_ERR_CONNECT_FAILED,
						  "%s", msg.c_str());

		return StartCommandFailed;
	}
	else if( m_nonblocking && m_sock->is_connect_pending() ) {
		dprintf(D_SECURITY,"SECMAN: waiting for TCP connection to %s.\n",
				m_sock->peer_description());
		return WaitForSocketCallback();
	}
	else if( m_is_tcp && !m_sock->is_connected()) {
		std::string msg;
		formatstr(msg, "TCP connection to %s failed.",
					m_sock->peer_description());
		dprintf(D_SECURITY,"SECMAN: %s\n", msg.c_str());
		m_errstack->pushf("SECMAN", SECMAN_ERR_CONNECT_FAILED,
						  "%s", msg.c_str());

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
		case AuthenticateContinue:
			result = authenticate_inner_continue();
			break;
		case AuthenticateFinish:
			result = authenticate_inner_finish();
			break;
		case ReceivePostAuthInfo:
			result = receivePostAuthInfo_inner();
			break;
		default:
			EXCEPT("Unexpected state in SecManStartCommand: %d",m_state);
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
	Sinful destsinful( m_sock->get_connect_addr() );
	Sinful oursinful( global_dc_sinful() );

	// find out if we have a session id to use for this command

	std::string sid;

	sid = m_sec_session_id_hint;
	if( sid.c_str()[0] && !m_raw_protocol && !m_use_tmp_sec_session ) {
		m_have_session = m_sec_man.LookupNonExpiredSession(sid.c_str(), m_enc_key);
		if( m_have_session ) {
			dprintf(D_SECURITY,"Using requested session %s.\n",sid.c_str());
		}
		else {
			dprintf(D_SECURITY,"Ignoring requested session, because it does not exist: %s\n",sid.c_str());
		}
	}

	const std::string &tag = SecMan::getTag();
	if (tag.size()) {
		formatstr(m_session_key, "{%s,%s,<%i>}", tag.c_str(), m_sock->get_connect_addr(), m_cmd);
	} else {
		formatstr(m_session_key, "{%s,<%i>}", m_sock->get_connect_addr(), m_cmd);
	}
	bool found_map_ent = false;
	if( !m_have_session && !m_raw_protocol && !m_use_tmp_sec_session ) {
		found_map_ent = (m_sec_man.command_map.lookup(m_session_key, sid) == 0);
	}
	if (found_map_ent) {
		dprintf (D_SECURITY, "SECMAN: using session %s for %s.\n", sid.c_str(), m_session_key.c_str());
		// we have the session id, now get the session from the cache
		m_have_session = m_sec_man.LookupNonExpiredSession(sid.c_str(), m_enc_key);

		if(!m_have_session) {
			// the session is no longer in the cache... might as well
			// delete this mapping to it.  (we could delete them all, but
			// it requires iterating through the hash table)
			if (m_sec_man.command_map.remove(m_session_key.c_str()) == 0) {
				dprintf (D_SECURITY, "SECMAN: session id %s not found, removed %s from map.\n", sid.c_str(), m_session_key.c_str());
			} else {
				dprintf (D_SECURITY, "SECMAN: session id %s not found and failed to removed %s from map!\n", sid.c_str(), m_session_key.c_str());
			}
		}
	}
	if( !m_have_session && !m_raw_protocol && !m_use_tmp_sec_session && daemonCore && !daemonCore->m_family_session_id.empty() ) {
		// We have a process family security session.
		// Try using it if the following are all true:
		// 1) Peer is on a local network interface
		// 2) If we're using shared port, peer is using same command port as us
		// 3) Peer isn't in m_not_my_family
		if ( m_sock->peer_is_local() && (oursinful.getSharedPortID() == NULL || oursinful.getPortNum() == destsinful.getPortNum()) && m_sec_man.m_not_my_family.count(m_sock->get_connect_addr()) == 0 ) {
			dprintf(D_SECURITY, "Trying family security session for local peer\n");
			m_have_session = m_sec_man.LookupNonExpiredSession(daemonCore->m_family_session_id.c_str(), m_enc_key);
			ASSERT(m_have_session);
		}
	}

	// if we have a private key, we will use the same security policy that
	// was decided on when the key was issued.
	// otherwise, get our security policy and work it out with the server.
	if (m_have_session) {
		MergeClassAds( &m_auth_info, m_enc_key->policy(), true );

		if (IsDebugVerbose(D_SECURITY)) {
			dprintf (D_SECURITY, "SECMAN: found cached session id %s for %s.\n",
					m_enc_key->id(), m_session_key.c_str());
			m_sec_man.key_printf(D_SECURITY, m_enc_key->key());
			dPrintAd( D_SECURITY, m_auth_info );
		}

		// Ensure that CryptoMethods in the resume session ad that we
		// send to the server matches the method and key we plan to use
		// for this connection.
		// Some sessions support a set of possible methods/keys, and
		// some have no method/key.
		if (m_enc_key->key()) {
			Protocol crypto_type = m_enc_key->key()->getProtocol();
			const char *crypto_name = SecMan::getCryptProtocolEnumToName(crypto_type);
			if (crypto_name && crypto_name[0]) {
				m_auth_info.Assign(ATTR_SEC_CRYPTO_METHODS, crypto_name);
			}
		} else {
			m_auth_info.Delete(ATTR_SEC_CRYPTO_METHODS);
		}

			// Ideally, we would only increment our lease expiration time after
			// verifying that the server renewed the lease on its side.  However,
			// there is no ACK in the protocol, so we just do it here.  Worst case,
			// we may end up trying to use a session that the server threw out,
			// which has the same error-handling as a restart of the server.
		m_enc_key->renewLease();

		// tweak the session if it's UDP
		if(!m_is_tcp) {
			// There's no AES support for UDP, so fall back to
			// BLOWFISH (default) or 3DES if specified.  3DES would
			// be preferred in FIPS mode.
			std::string fallback_method_str = "BLOWFISH";
			if (param_boolean("FIPS", false)) {
				fallback_method_str = "3DES";
			}
			dprintf(D_SECURITY|D_VERBOSE, "SESSION: fallback crypto method would be %s.\n", fallback_method_str.c_str());

			dprintf(D_SECURITY, "SESSION: for outgoing UDP, forcing %s, no MD5\n", fallback_method_str.c_str());
			m_auth_info.Assign(ATTR_SEC_CRYPTO_METHODS, fallback_method_str.c_str());
			// Integrity not needed since these packets are assumed
			// not to be leaving the local host.  Leaving it on
			// might use MD5 and make us non-FIPS compliant.
			m_auth_info.Assign(ATTR_SEC_INTEGRITY, "NO");
		}

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
				dprintf (D_SECURITY, "SECMAN: using temporary security session for %s.\n", m_session_key.c_str() );
			}
			else {
				dprintf (D_SECURITY, "SECMAN: no cached key for %s.\n", m_session_key.c_str());
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
		dPrintAd( D_SECURITY, m_auth_info );
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
		CondorVersionInfo ver_info(m_remote_version.c_str());
		m_sock->set_peer_version(&ver_info);
	}

	// fill in our version
	m_auth_info.Assign(ATTR_SEC_REMOTE_VERSION,CondorVersion());

	// fill in return address, if we are a daemon
	char const* dcss = global_dc_sinful();
	if (dcss) {
		m_auth_info.Assign(ATTR_SEC_SERVER_COMMAND_SOCK, dcss);
	}

	// Tell the server the sinful string we used to contact it
	m_auth_info.Assign(ATTR_SEC_CONNECT_SINFUL, m_sock->get_connect_addr());

	// fill in command
	m_auth_info.Assign(ATTR_SEC_COMMAND, m_cmd);

	if ((m_cmd == DC_AUTHENTICATE) || (m_cmd == DC_SEC_QUERY)) {
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
				dPrintAd( D_SECURITY, m_auth_info );
				m_errstack->push( "SECMAN", SECMAN_ERR_ATTRIBUTE_MISSING,
						"Protocol Error: Action attribute missing.");
				return StartCommandFailed;
			}


			KeyInfo* ki  = NULL;
			if (m_enc_key->key()) {
				KeyInfo* key_to_use;
				KeyInfo* fallback_key;

				// There's no AES support for UDP, so fall back to
				// BLOWFISH (default) or 3DES if specified.  3DES would
				// be preferred in FIPS mode.
				std::string fallback_method_str = "BLOWFISH";
				Protocol fallback_method = CONDOR_BLOWFISH;
				if (param_boolean("FIPS", false)) {
					fallback_method_str = "3DES";
					fallback_method = CONDOR_3DES;
				}
				dprintf(D_SECURITY|D_VERBOSE, "SESSION: fallback crypto method would be %s.\n", fallback_method_str.c_str());

				key_to_use = m_enc_key->key();
				fallback_key = m_enc_key->key(fallback_method);

				dprintf(D_SECURITY|D_VERBOSE, "UDP: client normal key (proto %i): %p\n", key_to_use->getProtocol(), key_to_use);
				dprintf(D_SECURITY|D_VERBOSE, "UDP: client fallback key (proto %i): %p\n", (fallback_key ? fallback_key->getProtocol() : 0), fallback_key);
				dprintf(D_SECURITY|D_VERBOSE, "UDP: client m_is_tcp: %i\n", m_is_tcp);

				// if UDP, and we were going to use AES, use fallback instead (if it exists)
				if(!m_is_tcp && (key_to_use->getProtocol() == CONDOR_AESGCM)) {
					if(fallback_key) {
						dprintf(D_SECURITY, "UDP: SWITCHING CRYPTO FROM AES TO %s.\n", fallback_method_str.c_str());
						key_to_use = fallback_key;
					} else {
						// Fail now since this isn't going to work.
						dprintf(D_ALWAYS, "UDP: ERROR: AES not supported for UDP.\n");
						m_errstack->push( "SECMAN", SECMAN_ERR_NO_KEY, "AES not supported for UDP" );
						return StartCommandFailed;
					}
				}
				ki  = new KeyInfo(*(key_to_use));
			}

			if (will_enable_mac == SecMan::SEC_FEAT_ACT_YES) {

				if (!ki) {
					dprintf ( D_ALWAYS, "SECMAN: enable_mac has no key to use, failing...\n");
					m_errstack->push( "SECMAN", SECMAN_ERR_NO_KEY,
							"Failed to establish a crypto key." );
					return StartCommandFailed;
				}

				if (IsDebugVerbose(D_SECURITY)) {
					dprintf (D_SECURITY, "SECMAN: about to enable message authenticator with key type %i\n",
								ki->getProtocol());
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

				// if the encryption method is AES, we don't actually want to enable the MAC
				// here as that instantiates an MD5 object which will cause FIPS to blow up.
				if(ki->getProtocol() != CONDOR_AESGCM) {
					m_sock->set_MD_mode(MD_ALWAYS_ON, ki, key_id.Value());
				} else {
					dprintf(D_SECURITY|D_VERBOSE, "SECMAN: because protocal is AES, not using other MAC.\n");
					m_sock->set_MD_mode(MD_OFF, ki, key_id.Value());
				}

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
				m_sock->set_crypto_key(turn_encryption_on, ki, key_id.c_str());

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
		dPrintAd ( D_SECURITY, m_auth_info );
	}

	// send the classad
	// if we are resuming, use the projection so we send the minimum number of attributes
	if (! putClassAd(m_sock, m_auth_info, 0, m_have_session ? SecMan::getResumeProj() : NULL)) {
		dprintf ( D_ALWAYS, "SECMAN: failed to send auth_info (resume was %i)\n", m_have_session);
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

			if (!getClassAd(m_sock, auth_response) ||
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
				dPrintAd( D_SECURITY, auth_response );
			}

				// Record the remote trust domain, if present
			std::string trust_domain;
			if (auth_response.EvaluateAttrString(ATTR_SEC_TRUST_DOMAIN, trust_domain)) {
				m_sock->setTrustDomain(trust_domain);
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
			if( !m_remote_version.empty() ) {
				CondorVersionInfo ver_info(m_remote_version.c_str());
				m_sock->set_peer_version(&ver_info);
			}
			m_sec_man.sec_copy_attribute( m_auth_info, auth_response, ATTR_SEC_ENACT );
			m_sec_man.sec_copy_attribute( m_auth_info, auth_response, ATTR_SEC_AUTHENTICATION_METHODS_LIST );
			m_sec_man.sec_copy_attribute( m_auth_info, auth_response, ATTR_SEC_AUTHENTICATION_METHODS );
			m_sec_man.sec_copy_attribute( m_auth_info, auth_response, ATTR_SEC_CRYPTO_METHODS );
			m_sec_man.sec_copy_attribute( m_auth_info, auth_response, ATTR_SEC_CRYPTO_METHODS_LIST );
			m_sec_man.sec_copy_attribute( m_auth_info, auth_response, ATTR_SEC_AUTHENTICATION );
			m_sec_man.sec_copy_attribute( m_auth_info, auth_response, ATTR_SEC_AUTH_REQUIRED );
			m_sec_man.sec_copy_attribute( m_auth_info, auth_response, ATTR_SEC_ENCRYPTION );
			m_sec_man.sec_copy_attribute( m_auth_info, auth_response, ATTR_SEC_INTEGRITY );
			m_sec_man.sec_copy_attribute( m_auth_info, auth_response, ATTR_SEC_SESSION_DURATION );
			m_sec_man.sec_copy_attribute( m_auth_info, auth_response, ATTR_SEC_SESSION_LEASE );

			m_sec_man.sec_copy_attribute( m_auth_info, auth_response, ATTR_SEC_ISSUER_KEYS);
			m_sec_man.sec_copy_attribute( m_auth_info, auth_response, ATTR_SEC_TRUST_DOMAIN);
			m_sec_man.sec_copy_attribute( m_auth_info, auth_response, ATTR_SEC_LIMIT_AUTHORIZATION);

			m_auth_info.Delete(ATTR_SEC_NEW_SESSION);

			m_auth_info.Assign(ATTR_SEC_USE_SESSION, "YES");

			std::string encryption;
			if (auth_response.EvaluateAttrString(ATTR_SEC_ENCRYPTION, encryption) && encryption == "YES") {
				std::string crypto_method_list;
				if (!auth_response.EvaluateAttrString(ATTR_SEC_CRYPTO_METHODS, crypto_method_list) ||
					crypto_method_list.empty())
				{
					dprintf(D_ALWAYS, "SECMAN: Remote server requires encryption but provided no crypto method to use.\n");
					m_errstack->push( "SECMAN", SECMAN_ERR_INVALID_POLICY, "Remote server requires encryption but provided no crypto method to use; potentially there were no mutually-compatible methods enabled between client and server." );
					return StartCommandFailed;
				}
				std::string crypto_method = crypto_method_list.substr(0, crypto_method_list.find(','));
				if (SecMan::filterCryptoMethods(crypto_method).empty()) {
					dprintf(D_ALWAYS, "SECMAN: Remote server suggested a crypto method (%s) we don't support.\n", crypto_method.c_str());
					m_errstack->pushf( "SECMAN", SECMAN_ERR_INVALID_POLICY, "Remote server suggested a crypto method (%s) we don't support", crypto_method.c_str());
					return StartCommandFailed;
				}
			}

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
			dPrintAd( D_SECURITY, m_auth_info );
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
				if( !m_remote_version.empty() ) {
					dprintf( D_SECURITY, "SECMAN: resume, other side is %s, NOT reauthenticating.\n", m_remote_version.c_str() );
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

			m_sock->setPolicyAd(m_auth_info);
			int auth_timeout = m_sec_man.getSecTimeout( CLIENT_PERM );
			int auth_result = m_sock->authenticate(m_private_key, auth_methods, m_errstack, auth_timeout, m_nonblocking, NULL);

			if (auth_methods) {
				free(auth_methods);
			}

			if( auth_result == 2 ) {
				m_state = AuthenticateContinue;
				return WaitForSocketCallback();
			} else if( !auth_result ) {
				bool auth_required = true;
				m_auth_info.LookupBool(ATTR_SEC_AUTH_REQUIRED,auth_required);

				if( auth_required ) {
					dprintf( D_ALWAYS,
							"SECMAN: required authentication with %s failed, so aborting command %s.\n",
							m_sock->peer_description(),
							m_cmd_description.c_str());
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
	}
        m_state = AuthenticateFinish;
        return StartCommandContinue;

}

StartCommandResult
SecManStartCommand::authenticate_inner_continue()
{
	int auth_result = m_sock->authenticate_continue(m_errstack, true, NULL);

	if( auth_result == 2 ) {
		return WaitForSocketCallback();
	} else if( !auth_result ) {
		bool auth_required = true;
		m_auth_info.LookupBool(ATTR_SEC_AUTH_REQUIRED,auth_required);

		if( auth_required ) {
			dprintf( D_ALWAYS,
				"SECMAN: required authentication with %s failed, so aborting command %s.\n",
                                                         m_sock->peer_description(),
                                                         m_cmd_description.c_str());
			return StartCommandFailed;
		}
		dprintf( D_SECURITY|D_FULLDEBUG,
			"SECMAN: authentication with %s failed but was not required, so continuing.\n",
			m_sock->peer_description() );
	}

	m_state = AuthenticateFinish;
	return StartCommandContinue;
}

StartCommandResult
SecManStartCommand::authenticate_inner_finish()
{
	if( m_is_tcp ) {
		SecMan::sec_feat_act will_enable_enc   = m_sec_man.sec_lookup_feat_act( m_auth_info, ATTR_SEC_ENCRYPTION );
		SecMan::sec_feat_act will_enable_mac   = m_sec_man.sec_lookup_feat_act( m_auth_info, ATTR_SEC_INTEGRITY );

			// We've successfully authenticated; clear out any prior authentication errors
			// so they don't affect future messages in the stack.
		m_errstack->clear();

		// We must configure encryption before integrity on the socket
		// when using AES over TCP. If we do integrity first, then ReliSock
		// will initialize an MD5 context and then tear it down when it
		// learns it's doing AES. This breaks FIPS compliance.
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

		if (will_enable_mac == SecMan::SEC_FEAT_ACT_YES) {

			if (!m_private_key) {
				dprintf ( D_ALWAYS, "SECMAN: enable_mac has no key to use, failing...\n");
				m_errstack->push ("SECMAN", SECMAN_ERR_NO_KEY,
							"Failed to establish a crypto key." );
				return StartCommandFailed;
			}

			if (IsDebugVerbose(D_SECURITY)) {
				dprintf (D_SECURITY, "SECMAN: about to enable message authenticator with key type %i\n",
							m_private_key->getProtocol());
				m_sec_man.key_printf(D_SECURITY, m_private_key);
			}

			m_sock->encode();

			// if the encryption method is AES, we don't actually want to enable the MAC
			// here as that instantiates an MD5 object which will cause FIPS to blow up.
			if(m_private_key->getProtocol() != CONDOR_AESGCM) {
				m_sock->set_MD_mode(MD_ALWAYS_ON, m_private_key);
			} else {
				dprintf(D_SECURITY|D_VERBOSE, "SECMAN: because protocal is AES, not using other MAC.\n");
				m_sock->set_MD_mode(MD_OFF, m_private_key);
			}

			dprintf ( D_SECURITY, "SECMAN: successfully enabled message authenticator!\n");
		} else {
			// we aren't going to enable hasing.  but we should still set the secret key
			// in case we decide to turn it on later.
			m_sock->encode();
			m_sock->set_MD_mode(MD_OFF, m_private_key);
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

			// receive a classAd containing info about new session.
			ClassAd post_auth_info;
			m_sock->decode();
			if (!getClassAd(m_sock, post_auth_info) || !m_sock->end_of_message()) {
				std::string errmsg;
				formatstr(errmsg, "Failed to received post-auth ClassAd");
				dprintf (D_ALWAYS, "SECMAN: FAILED: %s\n", errmsg.c_str());
				m_errstack->push ("SECMAN", SECMAN_ERR_COMMUNICATIONS_ERROR, errmsg.c_str());
				return StartCommandFailed;
			} else {
				if (IsDebugVerbose(D_SECURITY)) {
					dprintf (D_SECURITY, "SECMAN: received post-auth classad:\n");
					dPrintAd (D_SECURITY, post_auth_info);
				}
			}

				// Mark the session as having state-tracking enabled.
				// When combined with AES-GCM, this will cause the ReliSock to
				// store the state of the encryption in the session cache.
			if (!m_auth_info.InsertAttr("TrackState", true)) {
				dprintf(D_SECURITY, "SECMAN: Failed to enable state tracking.\n");
				return StartCommandFailed;
			}

			// Starting in 8.3.6, there is a ReturnCode attribute which tells us if the
			// other side authorized, denied, or was unaware of the command we sent.
			//
			// Inspect the return code in the reponse ad to see what we should do.
			// If it is blank, we must assume success, since that was what 8.3.5 and
			// earlier would send.
			std::string response_rc;
			post_auth_info.LookupString(ATTR_SEC_RETURN_CODE,response_rc);
			if((response_rc != "") && (response_rc != "AUTHORIZED")) {
				// gather some additional data for useful error reporting
				std::string response_user;
				MyString response_method = m_sock->getAuthenticationMethodUsed();
				post_auth_info.LookupString(ATTR_SEC_USER,response_user);

				// push error message on the stack and print to the log
				std::string errmsg;
				if (response_method == "") {
					response_method = "(no authentication)";
					formatstr( errmsg, "Received \"%s\" from server for user %s using no authentication method, which may imply host-based security.  Our address was '%s', and server's address was '%s'.  Check your ALLOW settings and IP protocols.",
						response_rc.c_str(), response_user.c_str(),
						m_sock->my_addr().to_ip_string().c_str(),
						m_sock->peer_addr().to_ip_string().c_str()
						);
				} else {
					m_sock->setShouldTryTokenRequest(true);
					formatstr(errmsg, "Received \"%s\" from server for user %s using method %s.",
						response_rc.c_str(), response_user.c_str(), response_method.c_str());
				}
				dprintf (D_ALWAYS, "SECMAN: FAILED: %s\n", errmsg.c_str());
				m_errstack->push ("SECMAN", SECMAN_ERR_AUTHORIZATION_FAILED, errmsg.c_str());
				return StartCommandFailed;
			} else {
				// Hey, it worked!  Make sure we don't request a new token.
				m_sock->setShouldTryTokenRequest(false);
			}

			// if we made it here, we're going to proceed as if the server authorized
			// our command and we will cache this session for future use.  we know this
			// for sure in 8.3.6 and beyond, but for 8.3.5 and earlier we must assume
			// success because this information was not transmitted explicitly.

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

			// update the ad with the auth method actually used
			if( m_sock->getAuthenticationMethodUsed() ) {
				m_auth_info.Assign( ATTR_SEC_AUTHENTICATION_METHODS, m_sock->getAuthenticationMethodUsed() );
			}

			// update the ad with the crypto method actually used
			if( m_sock->getCryptoMethodUsed() ) {
				m_auth_info.Assign( ATTR_SEC_CRYPTO_METHODS, m_sock->getCryptoMethodUsed() );
			} else {
				m_auth_info.Delete( ATTR_SEC_CRYPTO_METHODS );
			}

			if (IsDebugVerbose(D_SECURITY)) {
				dprintf (D_SECURITY, "SECMAN: policy to be cached:\n");
				dPrintAd(D_SECURITY, m_auth_info);
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
				free(sesid);
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

			std::vector<KeyInfo*> keyvec;
			dprintf(D_SECURITY|D_VERBOSE, "SESSION: client checking key type: %i\n", (m_private_key ? m_private_key->getProtocol() : -1));
			if (m_private_key) {
				// put the normal key into the vector
				keyvec.push_back(new KeyInfo(*m_private_key));

				// now see if we want to (and are allowed) to add a BLOWFISH key in addition to AES
				if (m_private_key->getProtocol() == CONDOR_AESGCM) {
					// There's no AES support for UDP, so fall back to
					// BLOWFISH (default) or 3DES if specified.  3DES would
					// be preferred in FIPS mode.
					std::string fallback_method_str = "BLOWFISH";
					Protocol fallback_method = CONDOR_BLOWFISH;
					if (param_boolean("FIPS", false)) {
						fallback_method_str = "3DES";
						fallback_method = CONDOR_3DES;
					}
					dprintf(D_SECURITY|D_VERBOSE, "SESSION: fallback crypto method would be %s.\n", fallback_method_str.c_str());

					std::string all_methods;
					if (m_auth_info.LookupString(ATTR_SEC_CRYPTO_METHODS_LIST, all_methods)) {
						dprintf(D_SECURITY|D_VERBOSE, "SESSION: found list: %s.\n", all_methods.c_str());
						StringList sl(all_methods.c_str());
						if (sl.contains_anycase(fallback_method_str.c_str())) {
							keyvec.push_back(new KeyInfo(m_private_key->getKeyData(), 24, fallback_method, 0));
							dprintf(D_SECURITY, "SESSION: client duplicated AES to %s key for UDP.\n", fallback_method_str.c_str());
						} else {
							dprintf(D_SECURITY, "SESSION: %s not allowed.  UDP will not work.\n", fallback_method_str.c_str());
						}
					} else {
						dprintf(D_ALWAYS, "SESSION: no crypto methods list\n");
					}
				}
			}

				// This makes a copy of the policy ad, so we don't
				// have to. 
			condor_sockaddr peer_addr = m_sock->peer_addr();
			KeyCacheEntry tmp_key( sesid, &peer_addr, keyvec,
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
				std::string keybuf;
				const std::string &tag = SecMan::getTag();
				if (tag.size()) {
					formatstr (keybuf, "{%s,%s,<%s>}", tag.c_str(), m_sock->get_connect_addr(), p);
				} else {
					formatstr (keybuf, "{%s,<%s>}", m_sock->get_connect_addr(), p);
				}

				// NOTE: HashTable returns ZERO on SUCCESS!!!
				if (m_sec_man.command_map.insert(keybuf, sesid, true) == 0) {
					// success
					if (IsDebugVerbose(D_SECURITY)) {
						dprintf (D_SECURITY, "SECMAN: command %s mapped to session %s.\n", keybuf.c_str(), sesid);
					}
				} else {
					dprintf (D_ALWAYS, "SECMAN: command %s NOT mapped (insert failed!)\n", keybuf.c_str());
				}
			}
			
			m_sock->setSessionID(sesid);
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
		if(m_sec_man.tcp_auth_in_progress.lookup(m_session_key,sc) == 0) {
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
						m_session_key.c_str());
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
	if (!tcp_auth_sock->connect(tcp_addr.c_str(),0,m_nonblocking)) {
		dprintf ( D_SECURITY, "SECMAN: couldn't connect via TCP to %s, failing...\n", tcp_addr.c_str());
		m_errstack->pushf("SECMAN", SECMAN_ERR_CONNECT_FAILED,
						  "TCP auth connection to %s failed.", tcp_addr.c_str());
		delete tcp_auth_sock;
		return StartCommandFailed;
	}

		// Make note that this operation to do the TCP
		// auth operation is in progress, so others
		// wanting the same session key can wait for it.
	SecMan::tcp_auth_in_progress.insert(m_session_key,this);

	m_tcp_auth_command = new SecManStartCommand(
		DC_AUTHENTICATE,
		tcp_auth_sock,
		m_raw_protocol,
		m_errstack,
		m_cmd,
		m_nonblocking ? SecManStartCommand::TCPAuthCallback : NULL,
		m_nonblocking ? this : NULL,
		m_nonblocking,
		m_cmd_description.c_str(),
		m_sec_session_id_hint.c_str(),
		m_owner,
		m_methods,
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
SecManStartCommand::TCPAuthCallback(bool success,Sock *sock,CondorError * /*errstack*/, const std::string & /* trust_domain */, bool /* should_try_token_request */, void * misc_data)
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
	if( SecMan::tcp_auth_in_progress.lookup(m_session_key,sc) == 0 &&
	    sc.get() == this )
	{
		ASSERT(SecMan::tcp_auth_in_progress.remove(m_session_key) == 0);
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

	std::string req_description;
	formatstr(req_description, "SecManStartCommand::WaitForSocketCallback %s",
							m_cmd_description.c_str());
	int reg_rc = daemonCore->Register_Socket(
		m_sock,
		m_sock->peer_description(),
		(SocketHandlercpp)&SecManStartCommand::SocketCallback,
		req_description.c_str(),
		this,
		ALLOW);

	if(reg_rc < 0) {
		std::string msg;
		formatstr(msg, "StartCommand to %s failed because "
					"Register_Socket returned %d.",
					m_sock->get_sinful_peer(),
					reg_rc);
		dprintf(D_SECURITY, "SECMAN: %s\n", msg.c_str());
		m_errstack->pushf("SECMAN", SECMAN_ERR_CONNECT_FAILED,
						  "%s", msg.c_str());

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
	daemonCore->Cancel_Socket( stream );

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

	int r = session_cache->lookup(key_id, keyEntry);
	if (!r) {
		dprintf( D_SECURITY,
				 "DC_INVALIDATE_KEY: security session %s not found in cache.\n",
				 key_id);
	}

	if ( keyEntry && keyEntry->expiration() <= time(NULL) && keyEntry->expiration() > 0 ) {
		dprintf( D_SECURITY,
				 "DC_INVALIDATE_KEY: security session %s %s expired.\n",
				 key_id, keyEntry->expirationType() );
	}

	remove_commands(keyEntry);

	// Now, remove session id
	if (daemonCore && !strcmp(daemonCore->m_family_session_id.c_str(), key_id) ) {
		dprintf ( D_SECURITY, "DC_INVALIDATE_KEY: ignoring request to invalidate family security key.\n" );
	} else
	if (session_cache->remove(key_id)) {
		dprintf ( D_SECURITY, 
				  "DC_INVALIDATE_KEY: removed key id %s.\n", 
				  key_id);
	} else {
		dprintf ( D_SECURITY, 
				  "DC_INVALIDATE_KEY: ignoring request to invalidate non-existant key %s.\n", 
				  key_id);
	}

    return removed;
}

void SecMan :: remove_commands(KeyCacheEntry * keyEntry)
{
    if (keyEntry) {
        char * commands = NULL;
        keyEntry->policy()->LookupString(ATTR_SEC_VALID_COMMANDS, &commands);
		std::string addr;
		if (keyEntry->addr())
			addr = keyEntry->addr()->to_sinful();
    
        // Remove all commands from the command map
        if (commands) {
            char keybuf[128];
            StringList cmd_list(commands);
            free(commands);
        
            cmd_list.rewind();
            char * cmd = NULL;
            while ( (cmd = cmd_list.next()) ) {
                memset(keybuf, 0, 128);
                sprintf (keybuf, "{%s,<%s>}", addr.c_str(), cmd);
                command_map.remove(keybuf);
            }
        }
    }
}

int
SecMan::sec_char_to_auth_method( const char* method ) {
    if (!strcasecmp( method, "SSL" )  ) {
        return CAUTH_SSL;
    } else if (!strcasecmp( method, "GSI" )  ) {
		return CAUTH_GSI;
	} else if ( !strcasecmp( method, "NTSSPI" ) ) {
		return CAUTH_NTSSPI;
	} else if ( !strcasecmp( method, "PASSWORD" ) ) {
		return CAUTH_PASSWORD;
	} else if ( !strcasecmp( method, "TOKENS" ) ) {
		return CAUTH_TOKEN;
	} else if ( !strcasecmp( method, "TOKEN" ) ) {
		return CAUTH_TOKEN;
	} else if ( !strcasecmp( method, "IDTOKENS" ) ) {
		return CAUTH_TOKEN;
	} else if ( !strcasecmp( method, "IDTOKEN" ) ) {
		return CAUTH_TOKEN;
	} else if ( !strcasecmp( method, "SCITOKENS" ) ) {
		return CAUTH_SCITOKENS;
	} else if ( !strcasecmp( method, "SCITOKEN" ) ) {
		return CAUTH_SCITOKENS;
	} else if ( !strcasecmp( method, "FS" ) ) {
		return CAUTH_FILESYSTEM;
	} else if ( !strcasecmp( method, "FS_REMOTE" ) ) {
		return CAUTH_FILESYSTEM_REMOTE;
	} else if ( !strcasecmp( method, "KERBEROS" ) ) {
		return CAUTH_KERBEROS;
	} else if ( !strcasecmp( method, "CLAIMTOBE" ) ) {
		return CAUTH_CLAIMTOBE;
	} else if ( !strcasecmp( method, "MUNGE" ) ) {
		return CAUTH_MUNGE;
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



std::string
SecMan::ReconcileMethodLists( char * cli_methods, char * srv_methods ) {

	// algorithm:
	// step through the server's methods in order.  if the method is
	// present in the clients list, then append it to the results.
	// the output will be a list of methods supported by both, in the
	// order that the server prefers.

	StringList server_methods( srv_methods );
	StringList client_methods( cli_methods );
	const char *sm = NULL;
	const char *cm = NULL;

	std::string results;
	int match = 0;

	// server methods, one at a time
	server_methods.rewind();
	while ( (sm = server_methods.next()) ) {
		client_methods.rewind();
		if (!strcasecmp("TOKENS", sm) || !strcasecmp("IDTOKENS", sm) || !strcasecmp("IDTOKEN", sm)) {
			sm = "TOKEN";
		}
		while ( (cm = client_methods.next()) ) {
			if (!strcasecmp("TOKENS", cm) || !strcasecmp("IDTOKENS", cm) || !strcasecmp("IDTOKEN", cm)) {
				cm = "TOKEN";
			}
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


SecMan::SecMan() :
	m_cached_auth_level(UNSET_PERM),
	m_cached_raw_protocol(false),
	m_cached_use_tmp_sec_session(false),
	m_cached_force_authentication(false),
	m_cached_return_value(-1) {

	// the list of ClassAd attributes we need to resume a session
	if (m_resume_proj.empty()) {
		m_resume_proj.insert(ATTR_SEC_USE_SESSION);
		m_resume_proj.insert(ATTR_SEC_SID);
		m_resume_proj.insert(ATTR_SEC_COMMAND);
		m_resume_proj.insert(ATTR_SEC_AUTH_COMMAND);
		m_resume_proj.insert(ATTR_SEC_SERVER_COMMAND_SOCK);
		m_resume_proj.insert(ATTR_SEC_CONNECT_SINFUL);
		m_resume_proj.insert(ATTR_SEC_COOKIE);
		m_resume_proj.insert(ATTR_SEC_CRYPTO_METHODS);
	}

	if ( NULL == m_ipverify ) {
		m_ipverify = new IpVerify( );
	}
	sec_man_ref_count++;
}


SecMan::SecMan(const SecMan & rhs/* copy */) : 
	m_cached_auth_level(rhs.m_cached_auth_level), 
	m_cached_raw_protocol(rhs.m_cached_raw_protocol), 
	m_cached_use_tmp_sec_session(rhs.m_cached_use_tmp_sec_session), 
	m_cached_force_authentication(rhs.m_cached_force_authentication),
	m_cached_return_value(rhs.m_cached_return_value) {

	sec_man_ref_count++;
}

const SecMan & SecMan::operator=(const SecMan & /* copy */) {
	return *this;
}

SecMan &
SecMan::operator=(SecMan && rhs)  noexcept {
	this->m_cached_auth_level   = rhs.m_cached_auth_level;
	this->m_cached_raw_protocol = rhs.m_cached_raw_protocol;
	this->m_cached_use_tmp_sec_session = rhs.m_cached_use_tmp_sec_session;
	this->m_cached_force_authentication = rhs.m_cached_force_authentication;
	this->m_cached_policy_ad = std::move(rhs.m_cached_policy_ad);
	this->m_cached_return_value = rhs.m_cached_return_value;
	return *this;
}



SecMan::~SecMan() {
	sec_man_ref_count--;
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
SecMan::Verify(DCpermission perm, const condor_sockaddr& addr, const char * fqu, std::string &allow_reason, std::string &deny_reason )
{
	IpVerify *ipverify = getIpVerify();
	ASSERT( ipverify );
	return ipverify->Verify(perm,addr,fqu,allow_reason,deny_reason);
}


bool
SecMan::sec_copy_attribute( classad::ClassAd &dest, const ClassAd &source, const char* attr ) {
	ExprTree *e = source.LookupExpr(attr);
	if (e) {
		ExprTree *cp = e->Copy();
		dest.Insert(attr,cp);
		return true;
	} else {
		return false;
	}
}

bool
SecMan::sec_copy_attribute( ClassAd &dest, const char *to_attr, const ClassAd &source, const char *from_attr ) {
	ExprTree *e = source.LookupExpr(from_attr);
	if (!e) {
		return false;
	}

	e = e->Copy();
	bool retval = dest.Insert(to_attr, e) != 0;
	return retval;
}


void
SecMan::invalidateAllCache() {
	session_cache->clear();

	command_map.clear();
}

void 
SecMan::invalidateOneExpiredCache(KeyCache *cache)
{
    // Go through all cache and invalide the ones that are expired
    StringList * list = cache->getExpiredKeys();

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

void
SecMan::invalidateExpiredCache()
{
	invalidateOneExpiredCache(&m_default_session_cache);
	if (!m_tagged_session_cache) {return;}
	std::map<std::string,KeyCache*>::iterator session_cache_iter;
	for (session_cache_iter = m_tagged_session_cache->begin();
		session_cache_iter != m_tagged_session_cache->end();
		session_cache_iter++)
	{
		if (session_cache_iter->second) {
			invalidateOneExpiredCache(session_cache_iter->second);
		}
	}
}

std::string SecMan::filterCryptoMethods(const std::string &input_methods)
{
	StringList meth_iter(input_methods.c_str());
	meth_iter.rewind();
	const char *tmp = nullptr;
	bool first = true;
	std::string result;
	while ((tmp = meth_iter.next())) {
		if (strcmp(tmp, "AES") && strcmp(tmp, "3DES") && strcmp(tmp, "TRIPLEDES") && strcmp(tmp, "BLOWFISH")) {
			continue;
		}
		if (first) {first = false;}
		else {result += ",";}
		result += tmp;
	}
	return result;
}

	// Iterate through all the methods in a list;
	// - Canonicalize the name of the method (IDTOKENS -> TOKENS)
	// - Remove any invalid names.
	// - Remove any valid names not supported by this build.
	// - Remove any methods that cannot work in the current setup (e.g.,
	//   SSL auth for daemons if there is no host certificate).
	// Issues warnings as appropriate to D_SECURITY.
std::string SecMan::filterAuthenticationMethods(DCpermission perm, const std::string &input_methods)
{
	StringList meth_iter(input_methods.c_str());
	meth_iter.rewind();
	const char *tmp = NULL;
	bool first = true;
	std::string result;
	dprintf(D_FULLDEBUG|D_SECURITY, "Filtering authentication methods (%s) prior to offering them remotely.\n",
		input_methods.c_str());
	while ((tmp = meth_iter.next())) {
		int method = sec_char_to_auth_method(tmp);
		switch (method) {
			case CAUTH_TOKEN: {
				if (!Condor_Auth_Passwd::should_try_auth()) {
					continue;
				}
				dprintf(D_FULLDEBUG|D_SECURITY, "Will try IDTOKENS auth.\n");
					// For wire compatibility with older versions, we
					// actually say 'TOKEN' instead of the canonical 'TOKENS'.
				tmp = "TOKEN";
				break;
			}
#ifndef WIN32
			case CAUTH_NTSSPI: {
				dprintf(D_SECURITY, "Ignoring NTSSPI method because it is not"
					" available to this build of HTCondor.\n");
				continue;
			}
#endif
#ifndef HAVE_EXT_MUNGE
			case CAUTH_MUNGE: {
				dprintf(D_SECURITY, "Ignoring MUNGE method because it is not"
					" available to this build of HTCondor.\n");
				continue;
			}
#endif
#ifndef HAVE_EXT_GLOBUS
			case CAUTH_GSI: {
				dprintf(D_SECURITY, "Ignoring GSI method because it is not"
					" available to this build of HTCondor.\n");
				continue;
			}
#endif
#ifndef HAVE_EXT_KRB5
			case CAUTH_KERBEROS: {
				dprintf(D_SECURITY, "Ignoring KERBEROS method because it is not"
					" available to this build of HTCondor.\n");
				continue;
			}
#endif
			case CAUTH_SCITOKENS: // fallthrough
#ifdef HAVE_EXT_SCITOKENS
			{
					// Ensure we use the canonical 'SCITOKENS' on the wire
					// for compatibility with older HTCondor versions.
				tmp = "SCITOKENS";
				break;
			}
#else
			{
				dprintf(D_SECURITY, "Ignoring SCITOKENS method because it is not"
					" available to this build of HTCondor.\n");
				continue;
			}
#endif
			case CAUTH_SSL: {
					// Client auth doesn't require a SSL cert, so
					// we always will try this
				if (CLIENT_PERM == perm) {break;}
				if (!Condor_Auth_SSL::should_try_auth()) {
					dprintf(D_SECURITY|D_FULLDEBUG, "Not trying SSL auth; server is not ready.\n");
					continue;
				}
				break;
			}
			case 0: {
				dprintf(D_SECURITY, "Requested configured authentication method "
					"%s not known or supported by HTCondor.\n", tmp);
				continue;
			}
			// As additional filters are made, we can add them here.
			default:
				break;
		};
		if (first) {first = false;}
		else {result += ",";}
		result += tmp;
	}
	return result;
}


	// Given a known permission level, determine the default authentication
	// methods we should use (as a string list) if the sysadmin provides
	// no input.
	//
	// NOTE: this does *not* filter the authentication methods; some might
	// not be valid for the current build of HTCondor
static std::string
getDefaultAuthenticationMethods(DCpermission perm) {
	std::string methods;
#if defined(WIN32)
	// default windows method
	methods = "NTSSPI";
#else
	// default unix method
	methods = "FS";
#endif

	methods += ",TOKEN";

#if defined(HAVE_EXT_KRB5)
	methods += ",KERBEROS";
#endif

#if defined(HAVE_EXT_GLOBUS)
	methods += ",GSI";
#endif

#if defined(HAVE_EXT_SCITOKENS)
	methods += ",SCITOKENS";
#endif

	// SSL is last as this may cause the client to be anonymous.
	methods += ",SSL";

	if (perm == READ) {
		methods += ",CLAIMTOBE";
	} else if (perm == CLIENT_PERM) {
		methods += ",CLAIMTOBE";
	}

	return methods;
}


std::string SecMan::getDefaultCryptoMethods() {
	return "AES,BLOWFISH,3DES";
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

		std::string tid;
        formatstr( tid, "%s:%i:%i", get_local_hostname().c_str(), mypid, 
					 (int)time(0));

        _my_unique_id = strdup(tid.c_str());
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

		if (value.length()) {
			set_parent_unique_id(value.c_str());
		}
	}

	return _my_parent_unique_id;
}

int
SecMan::authenticate_sock(Sock *s,DCpermission perm, CondorError* errstack)
{
	auto methods = getAuthenticationMethods( perm );
	ASSERT(s);
	int auth_timeout = getSecTimeout(perm);
	return s->authenticate( methods.c_str(), errstack, auth_timeout, false );
}

int
SecMan::authenticate_sock(Sock *s,KeyInfo *&ki, DCpermission perm, CondorError* errstack)
{
	auto methods = getAuthenticationMethods( perm );
	ASSERT(s);
	int auth_timeout = getSecTimeout(perm);
	return s->authenticate( ki, methods.c_str(), errstack, auth_timeout, false, NULL );
}

int
SecMan::getSecTimeout(DCpermission perm)
{
	int auth_timeout = -1;
	getIntSecSetting(auth_timeout,"SEC_%s_AUTHENTICATION_TIMEOUT",perm);
	return auth_timeout;
}

Protocol
SecMan::getCryptProtocolNameToEnum(char const *name) {
	if (!name) return CONDOR_NO_PROTOCOL;

	StringList list(name);
	list.rewind();
	char *tmp;
	while ((tmp = list.next())) {
		dprintf(D_NETWORK|D_VERBOSE, "Considering crypto protocol %s.\n", tmp);
		if (!strcasecmp(tmp, "BLOWFISH")) {
			dprintf(D_NETWORK|D_VERBOSE, "Decided on crypto protocol %s.\n", tmp);
			return CONDOR_BLOWFISH;
		} else if (!strcasecmp(tmp, "3DES") || !strcasecmp(tmp, "TRIPLEDES")) {
			dprintf(D_NETWORK|D_VERBOSE, "Decided on crypto protocol %s.\n", tmp);
			return CONDOR_3DES;
		} else if (!strcasecmp(tmp, "AES")) {
			dprintf(D_NETWORK|D_VERBOSE, "Decided on crypto protocol %s.\n", tmp);
			return CONDOR_AESGCM;
		}
	}
	dprintf(D_NETWORK, "Could not decide on crypto protocol from list %s, return CONDOR_NO_PROTOCOL.\n", name);
	return CONDOR_NO_PROTOCOL;
}

const char *
SecMan::getCryptProtocolEnumToName(Protocol proto)
{
	switch(proto) {
	case CONDOR_NO_PROTOCOL:
		return "";
	case CONDOR_BLOWFISH:
		return "BLOWFISH";
	case CONDOR_3DES:
		return "3DES";
	case CONDOR_AESGCM:
		return "AES";
	default:
		return "";
	}
}

std::string
SecMan::getPreferredOldCryptProtocol(const std::string &name)
{
	std::string answer;
	StringList list(name.c_str());
	list.rewind();
	char *tmp;
	while ((tmp = list.next())) {
		dprintf(D_NETWORK|D_VERBOSE, "Considering crypto protocol %s.\n", tmp);
		if (!strcasecmp(tmp, "BLOWFISH")) {
			dprintf(D_NETWORK|D_VERBOSE, "Decided on crypto protocol %s.\n", tmp);
			return "BLOWFISH";
		} else if (!strcasecmp(tmp, "3DES") || !strcasecmp(tmp, "TRIPLEDES")) {
			dprintf(D_NETWORK|D_VERBOSE, "Decided on crypto protocol %s.\n", tmp);
			return "3DES";
		} else if (!strcasecmp(tmp, "AES")) {
			dprintf(D_NETWORK|D_VERBOSE, "Decided on crypto protocol %s.\n", tmp);
			answer = tmp;
		}
	}
	if (answer.empty()) {
		dprintf(D_NETWORK, "Could not decide on crypto protocol from list %s, return CONDOR_NO_PROTOCOL.\n", name.c_str());
	} else {
		dprintf(D_NETWORK|D_VERBOSE, "Decided on crypto protocol %s.\n", answer.c_str());
	}
	return answer;
}

bool
SecMan::CreateNonNegotiatedSecuritySession(DCpermission auth_level, char const *sesid,char const *private_key,char const *exported_session_info,const char *auth_method,char const *peer_fqu, char const *peer_sinful, int duration, classad::ClassAd *policy_input, bool allow_multiple_methods)
{
	if (policy_input) {
		dprintf(D_SECURITY|D_VERBOSE, "NONNEGOTIATEDSESSION: policy_input ad is:\n");
		dPrintAd(D_SECURITY|D_VERBOSE, *policy_input);
	} else {
		dprintf(D_SECURITY|D_VERBOSE, "NONNEGOTIATEDSESSION: policy_input ad is NULL\n");
	}

	ClassAd policy;
	if (policy_input) {
		policy.CopyFrom(*policy_input);
	}

	ASSERT(sesid);

	condor_sockaddr peer_addr;
	if(peer_sinful && !peer_addr.from_sinful(peer_sinful)) {
		dprintf(D_ALWAYS,"SECMAN: failed to create non-negotiated security session %s because "
				"sock_sockaddr::from_sinful(%s) failed\n",sesid,peer_sinful);
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
		dprintf(D_ALWAYS,"SECMAN: failed to create non-negotiated security session %s because "
				"ReconcileSecurityPolicyAds() failed.\n",sesid);
		return false;
	}
	sec_copy_attribute(policy,*auth_info,ATTR_SEC_AUTHENTICATION);
	sec_copy_attribute(policy,*auth_info,ATTR_SEC_INTEGRITY);
	sec_copy_attribute(policy,*auth_info,ATTR_SEC_ENCRYPTION);
	sec_copy_attribute(policy,*auth_info,ATTR_SEC_CRYPTO_METHODS);

	delete auth_info;
	auth_info = NULL;

	if( !ImportSecSessionInfo(exported_session_info,policy) ) {
		return false;
	}

	std::string crypto_methods;
	policy.LookupString(ATTR_SEC_CRYPTO_METHODS, crypto_methods);

	// preserve full list
	policy.Assign(ATTR_SEC_CRYPTO_METHODS_LIST, crypto_methods);

	// ZKM FIXME TODO:
	// why would we disallow multiple methods?
	allow_multiple_methods = true;

	// remove all but the first crypto method, if multiple methods
	// aren't allowed
	if (!allow_multiple_methods) {
		size_t pos = crypto_methods.find(',');
		if( pos != std::string::npos ) {
			crypto_methods.erase(pos);
			policy.Assign(ATTR_SEC_CRYPTO_METHODS, crypto_methods);
		}
	}

	policy.Assign(ATTR_SEC_USE_SESSION, "YES");
	policy.Assign(ATTR_SEC_SID, sesid);
	policy.Assign(ATTR_SEC_ENACT, "YES");

	if( auth_method ) {
		policy.Assign(ATTR_SEC_AUTHENTICATION_METHODS, auth_method);

	}
	if( peer_fqu ) {
		policy.Assign(ATTR_SEC_AUTHENTICATION, SecMan::sec_feat_act_rev[SEC_FEAT_ACT_NO]);
		policy.Assign(ATTR_SEC_TRIED_AUTHENTICATION,true);
		policy.Assign(ATTR_SEC_USER,peer_fqu);
	}

		// extract the session duration from the (imported) policy
	int expiration_time = 0;

	if( policy.LookupInteger(ATTR_SEC_SESSION_EXPIRES,expiration_time) ) {
		duration = expiration_time ? expiration_time - time(NULL) : 0;
		if( duration < 0 ) {
			dprintf(D_ALWAYS,"SECMAN: failed to create non-negotiated security session %s because duration = %d\n",sesid,duration);
			return false;
		}
	}
	else if( duration > 0 ) {
		expiration_time = time(NULL) + duration;
			// store this in the policy so that when we export session info,
			// it is there
		policy.Assign(ATTR_SEC_SESSION_EXPIRES,expiration_time);
	}

	// TODO What happens if we don't support any of the methods in the list?
	//   The old code creates a KeyInfo with CONDOR_NO_PROTOCOL.
	std::vector<KeyInfo*> keys_list;
	Tokenize(crypto_methods);
	const char *next_crypto;
	while ((next_crypto = GetNextToken(",", true))) {
		Protocol crypt_protocol = getCryptProtocolNameToEnum(next_crypto);

		unsigned char* keybuf;
		if (crypt_protocol != CONDOR_AESGCM) {
			if(param_boolean("FIPS", false)) {
				// if operating in FIPS mode, we can't use
				// oneWayHashKey since that is MD5.
				keybuf = Condor_Crypt_Base::hkdf(reinterpret_cast<const unsigned char *>(private_key), strlen(private_key), 24);
				dprintf(D_SECURITY, "SECMAN: in FIPS mode, used used hkdf for key protocol %i.\n", crypt_protocol);
			} else {
				keybuf = Condor_Crypt_Base::oneWayHashKey(private_key);
			}
		} else {
			keybuf = Condor_Crypt_Base::hkdf(reinterpret_cast<const unsigned char *>(private_key), strlen(private_key), 32);
		}
		if(!keybuf) {
			dprintf(D_ALWAYS,"SECMAN: failed to create non-negotiated security session %s because"
				" key generation failed.\n",sesid);
			return false;
		}
		KeyInfo *keyinfo;
		if (crypt_protocol == CONDOR_AESGCM) {
			keyinfo = new KeyInfo(keybuf, 32, crypt_protocol, 0);
		} else {
			// should this be MAC_SIZE?
			keyinfo = new KeyInfo(keybuf,MAC_SIZE,crypt_protocol, 0);
		}
		keys_list.push_back(keyinfo);
		free( keybuf );
		keybuf = NULL;
	}

	KeyCacheEntry key(sesid,peer_sinful ? &peer_addr : NULL,keys_list,&policy,expiration_time,0);

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
			ClassAd *existing_policy = existing ? existing->policy() : NULL;
			if( existing_policy ) {
				dprintf(D_SECURITY,"SECMAN: not creating new session, found existing session %s\n", sesid);
				dPrintAd(D_SECURITY | D_FULLDEBUG, *existing_policy);
			} else {
				dprintf(D_ALWAYS, "SECMAN: failed to create session %s.\n", sesid);
			}
			return false;
		}
	}

	dprintf(D_SECURITY, "SECMAN: created non-negotiated security session %s for %d %sseconds."
			"\n", sesid, duration, expiration_time == 0 ? "(inf) " : "");

	// now add entrys which map all the {<sinful_string>,<command>} pairs
	// to the same key id (which is in the variable sesid)
	dprintf(D_SECURITY, "SECMAN: now creating non-negotiated command mappings\n");

	std::string valid_coms;
	policy.LookupString(ATTR_SEC_VALID_COMMANDS, valid_coms);
	StringList coms(valid_coms.c_str());
	char *p;

	coms.rewind();
	while ( (p = coms.next()) ) {
		std::string keybuf;
		const std::string &tag = SecMan::getTag();
		if (tag.size()) {
			formatstr (keybuf, "{%s,%s,<%s>}", tag.c_str(), peer_sinful, p);
		} else {
			formatstr (keybuf, "{%s,<%s>}", peer_sinful, p);
		}

		// NOTE: HashTable returns ZERO on SUCCESS!!!
		if (command_map.insert(keybuf, sesid, true) == 0) {
			// success
			if (IsDebugVerbose(D_SECURITY)) {
				dprintf (D_SECURITY, "SECMAN: command %s mapped to session %s.\n", keybuf.c_str(), sesid);
			}
		} else {
			dprintf (D_ALWAYS, "SECMAN: command %s NOT mapped (insert failed!)\n", keybuf.c_str());
		}
	}

	if( IsDebugVerbose(D_SECURITY) ) {
		if( exported_session_info ) {
			dprintf(D_SECURITY,"Imported session attributes: %s\n",
					exported_session_info);
		}
		dprintf(D_SECURITY,"Caching non-negotiated security session ad:\n");
		dPrintAd(D_SECURITY, policy);
	}

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

	std::string buf = session_info+1;

		// verify that the string is contained in []'s
	if( session_info[0]!='[' || buf[buf.length()-1]!=']' ) {
		dprintf( D_ALWAYS, "ImportSecSessionInfo: invalid session info: %s\n",
				session_info );
		return false;
	}

		// get rid of final ']'
	buf.erase(buf.length()-1);

	StringList lines(buf.c_str(),";");
	lines.rewind();

	char const *line;
	ClassAd imp_policy;
	while( (line=lines.next()) ) {
		if( !imp_policy.Insert(line) ) {
			dprintf( D_ALWAYS, "ImportSecSessionInfo: invalid imported session info: '%s' in %s\n", line, session_info );
			return false;
		}
	}

	dprintf(D_SECURITY|D_VERBOSE, "IMPORT: Importing session attributes from ad:\n");
	dPrintAd(D_SECURITY|D_VERBOSE, imp_policy);

		// We could have just blindly inserted everything into our policy,
		// but for safety, we explicitly copy over specific attributes
		// from imp_policy to our policy.

	sec_copy_attribute(policy,imp_policy,ATTR_SEC_INTEGRITY);
	sec_copy_attribute(policy,imp_policy,ATTR_SEC_ENCRYPTION);
	sec_copy_attribute(policy,imp_policy,ATTR_SEC_CRYPTO_METHODS);
	sec_copy_attribute(policy,imp_policy,ATTR_SEC_SESSION_EXPIRES);
	sec_copy_attribute(policy,imp_policy,ATTR_SEC_VALID_COMMANDS);

	// If the import policy has CryptoMethodsList, that should override
	// CryptoMethods
	sec_copy_attribute(policy, ATTR_SEC_CRYPTO_METHODS, imp_policy, ATTR_SEC_CRYPTO_METHODS_LIST);

		// See comments in ExportSecSessionInfo; use a different delimiter as ','
		// is a significant character and not allowed in session IDs.
	std::string crypto_methods;
	if (policy.LookupString(ATTR_SEC_CRYPTO_METHODS, crypto_methods)) {
		std::replace(crypto_methods.begin(), crypto_methods.end(), '.', ',');
		policy.Assign(ATTR_SEC_CRYPTO_METHODS, crypto_methods.c_str());
	}

	// we need to convert the short version (e.g. "8.9.7") into a proper version string
	std::string short_version;
	if (imp_policy.LookupString(ATTR_SEC_SHORT_VERSION, short_version)) {
		int maj, min, sub;
		char *pos = NULL;
		maj = strtol(short_version.c_str(), &pos, 10);
		min = (pos[0] == '.') ? strtol(pos+1, &pos, 10) : 0;
		sub = (pos[0] == '.') ? strtol(pos+1, &pos, 10) : 0;

		CondorVersionInfo cvi(maj,min,sub, "ExportedSessionInfo");
		std::string full_version = cvi.get_version_stdstring();
		policy.Assign(ATTR_SEC_REMOTE_VERSION, full_version.c_str());
		dprintf (D_SECURITY|D_VERBOSE, "IMPORT: Version components are %i:%i:%i, set Version to %s\n", maj, min, sub, full_version.c_str());
	}

	return true;
}

bool
SecMan::getSessionPolicy(const char *session_id, classad::ClassAd &policy_ad)
{
	KeyCacheEntry *session_key = NULL;
	if (!session_cache->lookup(session_id, session_key)) {return false;}
	ClassAd *policy = session_key->policy();
	if (!policy) {return false;}

	sec_copy_attribute(policy_ad, *policy, ATTR_X509_USER_PROXY_SUBJECT);
	sec_copy_attribute(policy_ad, *policy, ATTR_X509_USER_PROXY_EXPIRATION);
	sec_copy_attribute(policy_ad, *policy, ATTR_X509_USER_PROXY_EMAIL);
	sec_copy_attribute(policy_ad, *policy, ATTR_X509_USER_PROXY_VONAME);
	sec_copy_attribute(policy_ad, *policy, ATTR_X509_USER_PROXY_FIRST_FQAN);
	sec_copy_attribute(policy_ad, *policy, ATTR_X509_USER_PROXY_FQAN);
	sec_copy_attribute(policy_ad, *policy, ATTR_TOKEN_SUBJECT);
	sec_copy_attribute(policy_ad, *policy, ATTR_TOKEN_ISSUER);
	sec_copy_attribute(policy_ad, *policy, ATTR_TOKEN_GROUPS);
	sec_copy_attribute(policy_ad, *policy, ATTR_TOKEN_SCOPES);
	sec_copy_attribute(policy_ad, *policy, ATTR_TOKEN_ID);
	sec_copy_attribute(policy_ad, *policy, ATTR_REMOTE_POOL);
	sec_copy_attribute(policy_ad, *policy, "ScheddSession");
	return true;
}

bool
SecMan::getSessionStringAttribute(const char *session_id, const char *attr_name, std::string &attr_value)
{
	KeyCacheEntry *session_key = NULL;
	if (!session_cache->lookup(session_id, session_key)) {return false;}
	ClassAd *policy = session_key->policy();
	if (!policy) {return false;}

	return policy->LookupString(attr_name,attr_value) ? true : false;
}

bool
SecMan::ExportSecSessionInfo(char const *session_id,std::string &session_info) {
    MyString ms;
    bool rv = ExportSecSessionInfo(session_id, ms);
    if(! ms.empty()) { session_info = ms; }
    return rv;
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

	dprintf(D_SECURITY|D_VERBOSE, "EXPORT: Exporting session attributes from ad:\n");
	dPrintAd(D_SECURITY|D_VERBOSE, *policy);

	ClassAd exp_policy;
	sec_copy_attribute(exp_policy,*policy,ATTR_SEC_INTEGRITY);
	sec_copy_attribute(exp_policy,*policy,ATTR_SEC_ENCRYPTION);
	sec_copy_attribute(exp_policy,*policy,ATTR_SEC_SESSION_EXPIRES);
	sec_copy_attribute(exp_policy,*policy,ATTR_SEC_VALID_COMMANDS);

	std::string crypto_methods;
	policy->LookupString(ATTR_SEC_CRYPTO_METHODS, crypto_methods);
	size_t pos = crypto_methods.find(',');
	if( pos != std::string::npos ) {
		std::string preferred = getPreferredOldCryptProtocol(crypto_methods);
		if (preferred.empty()) {
			preferred = crypto_methods.substr(0, pos);
		}
		exp_policy.Assign(ATTR_SEC_CRYPTO_METHODS, preferred);

		// ',' is not a permissible character in claim IDs.
		// Convert it to a different delimiter...
		std::replace(crypto_methods.begin(), crypto_methods.end(), ',', '.');
		exp_policy.Assign(ATTR_SEC_CRYPTO_METHODS_LIST, crypto_methods);
	} else if (!crypto_methods.empty()) {
		exp_policy.Assign(ATTR_SEC_CRYPTO_METHODS, crypto_methods);
	}

	// we want to export "RemoteVersion" but the spaces in the full version
	// string screw up parsing when importing.  so we have to extract just
	// the numeric version (e.g. "8.9.7")
	std::string full_version;
	if (policy->LookupString(ATTR_SEC_REMOTE_VERSION, full_version)) {
		CondorVersionInfo cvi(full_version.c_str());
		std::string short_version;
		short_version = std::to_string(cvi.getMajorVer());
		short_version += ".";
		short_version += std::to_string(cvi.getMinorVer());
		short_version += ".";
		short_version += std::to_string(cvi.getSubMinorVer());
		dprintf (D_SECURITY|D_VERBOSE, "EXPORT: Setting short version to %s\n", short_version.c_str());
		exp_policy.Assign(ATTR_SEC_SHORT_VERSION, short_version.c_str());
	}

	session_info += "[";
	for ( auto itr = exp_policy.begin(); itr != exp_policy.end(); itr++ ) {
			// In the following, we attempt to avoid any spaces in the
			// result string.  However, no code should depend on this.
		session_info += itr->first;
		session_info += "=";

        const char *line = ExprTreeToString(itr->second);

			// none of the ClassAd values should ever contain ';'
			// that makes things easier in ImportSecSessionInfo()
		ASSERT( strchr(line,';') == NULL );

		session_info += line;
		session_info += ";";
    }
	session_info += "]";

	dprintf(D_SECURITY,"SECMAN: exporting session info for %s: %s\n",
			session_id, session_info.c_str());
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
