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
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "../condor_daemon_core.V6/condor_ipverify.h"
#include "condor_secman.h"
#include "classad_merge.h"
#include "daemon.h"
#include "daemon_core_sock_adapter.h"
#include "setenv.h"

extern char* mySubSystem;
extern bool global_dc_get_cookie(int &len, unsigned char* &data);

template class HashTable<MyString, class SecManStartCommand *>;
template class SimpleList<class SecManStartCommand *>;

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



char* SecMan::sec_feat_act_rev[] = {
	"UNDEFINED",
	"INVALID",
	"FAIL",
	"YES",
	"NO"
};


char* SecMan::sec_req_rev[] = {
	"UNDEFINED",
	"INVALID",
	"NEVER",
	"OPTIONAL",
	"PREFERRED",
	"REQUIRED"
};

KeyCache* SecMan::session_cache = NULL;
HashTable<MyString,MyString>* SecMan::command_map = NULL;
HashTable<MyString,class SecManStartCommand *>* SecMan::tcp_auth_in_progress = NULL;
int SecMan::sec_man_ref_count = 0;
char* SecMan::_my_unique_id = 0;
char* SecMan::_my_parent_unique_id = 0;
bool SecMan::_should_check_env_for_unique_id = true;

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
			if( DebugFlags & D_FULLDEBUG ) {
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

char* 
SecMan::getSecSetting( const char* fmt, DCpermissionHierarchy const &auth_level, MyString *param_name /* = NULL */, char const *check_subsystem /* = NULL */ ) {
	DCpermission const *perms = auth_level.getConfigPerms();
	char *result;

		// Now march through the list of config settings to look for.  The
		// last one in the list will be DEFAULT_PERM, which we only use
		// if nothing else is found first.

	for( ; *perms != LAST_PERM; perms++ ) {
		MyString buf;
		if( check_subsystem ) {
				// First see if there is a specific config entry for the
				// specified condor subsystem.
			buf.sprintf( fmt, PermString(*perms) );
			buf.sprintf_cat("_%s",check_subsystem);
			result = param( buf.Value() );
			if( result ) {
				if( param_name ) {
						// Caller wants to know the param name.
					param_name->append_to_list(buf);
				}
				return result;
			}
		}

		buf.sprintf( fmt, PermString(*perms) );
		result = param( buf.Value() );
		if( result ) {
			if( param_name ) {
					// Caller wants to know the param name.
				param_name->append_to_list(buf);
			}
			return result;
		}
	}

	return NULL;
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
								bool other_side_can_negotiate )
{
	if( ! ad ) {
		EXCEPT( "SecMan::FillInSecurityPolicyAd called with NULL ad!" );
	}

	// get values from config file, trying each auth level in the
	// list in turn.  The final level in the list will be "DEFAULT".
	// if that fails, the default value (OPTIONAL) is used.

	sec_req sec_authentication = sec_req_param(
		"SEC_%s_AUTHENTICATION", auth_level, SEC_REQ_OPTIONAL);

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

	// if we require negotiation and we know the other side can't speak
	// security negotiation, may as well fail now (as opposed to later)
	if( sec_negotiation == SEC_REQ_REQUIRED && 
		other_side_can_negotiate == FALSE ) {
		dprintf (D_SECURITY, "SECMAN: failure! SEC_NEGOTIATION "
				"is REQUIRED and other daemon is pre 6.3.3.\n");
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
	ad->Assign ( ATTR_SEC_SUBSYSTEM, mySubSystem );

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
	char fmt[128];
	sprintf(fmt, "SEC_%s_%%s_SESSION_DURATION", mySubSystem);
	paramer = SecMan::getSecSetting(fmt, auth_level);
	if (!paramer) {
		paramer = SecMan::getSecSetting("SEC_%s_SESSION_DURATION", auth_level);
	}

	if (paramer) {
		// take whichever value we found and put it in the ad.
		ad->Assign ( ATTR_SEC_SESSION_DURATION, paramer );
		free( paramer );
		paramer = NULL;
	} else {
		// no value defined, use defaults.
		if (strcmp(mySubSystem, "TOOL") == 0) {
			// default for tools is 1 minute.
			ad->Assign ( ATTR_SEC_SESSION_DURATION, "60" );
		} else if (strcmp(mySubSystem, "SUBMIT") == 0) {
			// default for submit is 1 hour.  yeah, that's a long submit
			// but you never know with file transfer and all.
			ad->Assign ( ATTR_SEC_SESSION_DURATION, "3600" );
		} else {
			// default for daemons is one day.
			ad->Assign ( ATTR_SEC_SESSION_DURATION, "86400" );
		}
	}

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
									ClassAd &cli_ad, ClassAd &srv_ad) {

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


	authentication_action = ReconcileSecurityAttribute(
								ATTR_SEC_AUTHENTICATION,
								cli_ad, srv_ad );

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


	sprintf (buf, "%s=\"YES\"", ATTR_SEC_ENACT);
	action_ad->Insert(buf);

	return action_ad;

}


class SecManStartCommand: Service {
 public:
	SecManStartCommand (
		int cmd,Sock *sock,bool can_negotiate,
		CondorError *errstack,int subcmd,StartCommandCallbackType *callback_fn,
		void *misc_data,bool nonblocking,SecMan *sec_man):

		m_cmd(cmd),
		m_subcmd(subcmd),
		m_sock(sock),
		m_can_negotiate(can_negotiate),
		m_errstack(errstack),
		m_callback_fn(callback_fn),
		m_misc_data(misc_data),
		m_nonblocking(nonblocking),
		m_pending_socket_registered(false),
		m_sec_man(*sec_man)
	{
		m_refcount = 1;
		m_already_tried_TCP_auth = false;
	}

		// This function starts a command, as specified by the data
		// passed into the constructor.  The real work is done in
		// startCommand_inner(), but this function exits to guarantee
		// correct cleanup (e.g. calling callback etc.)
	StartCommandResult startCommand();

	void IncRefCount() {
		m_refcount++;
	}
	void DecRefCount() {
		ASSERT(m_refcount > 0);
		if(--m_refcount == 0) {
			delete this;
		}
	}
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

	~SecManStartCommand() {
		if( m_pending_socket_registered ) {
			m_pending_socket_registered = false;
			daemonCoreSockAdapter.decrementPendingSockets();
		}
		if(m_nonblocking && !m_callback_fn) {
			DestructCallerData();
		}
	}

		// This deletes sock and errstack.  It is called when in
		// non-blocking mode when there is no callback function.
	void DestructCallerData();

 private:
	int m_cmd;
	int m_subcmd;
	Sock *m_sock;
	bool m_can_negotiate;
	CondorError* m_errstack;
	StartCommandCallbackType *m_callback_fn;
	void *m_misc_data;
	bool m_nonblocking;
	bool m_pending_socket_registered;
	SecMan m_sec_man; // We create a copy of the original sec_man, so we
	                  // don't have to worry about longevity of it.  It's
	                  // all static data anyway, so this is no big deal.

	MyString m_session_key; // "addr,<cmd>"
	bool m_already_tried_TCP_auth;
	SimpleList<SecManStartCommand *> m_waiting_for_tcp_auth;

	int m_refcount;

		// This does most of the work of the startCommand() protocol.
		// It is called from within a wrapper function that guarantees
		// correct cleanup (e.g. callback function being called etc.)
	StartCommandResult startCommand_inner();

		// This is called when the TCP auth socket is connected.
		// This function guarantees correct cleanup.
	int TCPAuthConnected( Stream *stream );

		// This is called to do the TCP auth command.
		// It is called from within a wrapper function that guarantees
		// correct cleanup (e.g. callback function being called etc.)
	StartCommandResult TCPAuthConnected_inner( Sock *tcp_auth_sock, bool *auth_succeeded=NULL );

		// This is called when we were waiting for another
		// instance to finish a TCP auth session.  This
		// function guarantees correct cleanup.
	void ResumeAfterTCPAuth(bool auth_succeeded);

		// This is called either after finishing our own TCP auth
		// session, or after waiting for another instance to finish a
		// TCP auth session that we can use.  It is called from within
		// a wrapper function that guarantees correct cleanup
		// (e.g. callback function being called etc.)
	StartCommandResult ResumeAfterTCPAuth_inner();
};

StartCommandResult
SecMan::startCommand( int cmd, Sock* sock, bool can_neg, CondorError* errstack, int subcmd, StartCommandCallbackType *callback_fn, void *misc_data, bool nonblocking)
{
	// This function is simply a convenient wrapper around the
	// SecManStartCommand class, which does the actual work.

	// If this is nonblocking, we must create the following on the heap.
	// The blocking case could avoid use of the heap, but for simplicity,
	// we just do the same in both cases.

	SecManStartCommand *sc = new SecManStartCommand(cmd,sock,can_neg,errstack,subcmd,callback_fn,misc_data,nonblocking,this);

	ASSERT(sc);

	StartCommandResult rc = sc->startCommand();

		// We are done with this instance of the StartCommand class,
		// but someone else might be using it (i.e. a non-blocking
		// operation may be in progress), so just release our reference
		// to it, rather than deleting it.
	sc->DecRefCount();

	return rc;
}


StartCommandResult
SecManStartCommand::startCommand()
{
	// NOTE: if there is a callback function, we _must_ guarantee that it is
	// eventually called in all code paths.

	// never deallocate this class until we release this reference
	IncRefCount();

	StartCommandResult rc = startCommand_inner();

	if(rc == StartCommandInProgress) {
		// do nothing
	}
	else if(m_callback_fn) {
		bool success = rc == StartCommandSucceeded;
		(*m_callback_fn)(success,m_sock,m_errstack,m_misc_data);

		// We just called back with the success/failure code.  Therefore,
		// we simply return success to indicate that we successfully
		// called back.
		rc = StartCommandSucceeded;
	}

	// we are done with this; if everyone else is too, then destruct
	DecRefCount();

	return rc;
}

StartCommandResult
SecManStartCommand::startCommand_inner()
{

	// NOTE: the caller of this function must ensure that
	// the m_callback_fn is called (if there is one).

	ASSERT(m_sock);
	ASSERT(m_errstack);

	dprintf ( D_SECURITY, "SECMAN: %scommand %i to %s on %s port %i (%s).\n",
			  m_already_tried_TCP_auth ? "resuming " : "",
			  m_cmd,
			  sin_to_string(m_sock->endpoint()),
			  (m_sock->type() == Stream::safe_sock) ? "UDP" : "TCP",
			  m_sock->get_port(),
			  m_nonblocking ?  "non-blocking" : "blocking");


	// get this value handy
	bool is_tcp = (m_sock->type() == Stream::reli_sock);


	// need a temp buffer handy throughout.
	char buf[1024];

	// this one is a temp for storing key ids
	char keybuf[128];

	bool have_session = false;
	bool new_session = false;


	// find out if we have a session id to use for this command
	KeyCacheEntry *enc_key = NULL;

	MyString sid;
	sprintf (keybuf, "{%s,<%i>}", sin_to_string(m_sock->endpoint()), m_cmd);
	m_session_key = keybuf;
	bool found_map_ent = (m_sec_man.command_map->lookup(keybuf, sid) == 0);
	if (found_map_ent) {
		dprintf (D_SECURITY, "SECMAN: using session %s for %s.\n", sid.Value(), keybuf);
		// we have the session id, now get the session from the cache
		have_session = m_sec_man.session_cache->lookup(sid.Value(), enc_key);

		if (have_session) {
			// check the expiration.
			time_t cutoff_time = time(0);
			if (enc_key->expiration() && enc_key->expiration() <= cutoff_time) {
				m_sec_man.session_cache->expire(enc_key);
				have_session = false;
				enc_key = NULL;
			}
		} else {
			// the session is no longer in the cache... might as well
			// delete this mapping to it.  (we could delete them all, but
			// it requires iterating through the hash table)
			if (m_sec_man.command_map->remove(keybuf) == 0) {
				dprintf (D_SECURITY, "SECMAN: session id %s not found, removed %s from map.\n", sid.Value(), keybuf);
			} else {
				dprintf (D_SECURITY, "SECMAN: session id %s not found and failed to removed %s from map!\n", sid.Value(), keybuf);
			}
		}
	} else {
		have_session = false;
		enc_key = NULL;
	}


	// this classad will hold our security policy
	ClassAd auth_info;

	// if we have a private key, we will use the same security policy that
	// was decided on when the key was issued.
	// otherwise, get our security policy and work it out with the server.
	if (have_session) {
		MergeClassAds( &auth_info, enc_key->policy(), true );

		if (DebugFlags & D_FULLDEBUG) {
			dprintf (D_SECURITY, "SECMAN: found cached session id %s for %s.\n",
					enc_key->id(), keybuf);
			m_sec_man.key_printf(D_SECURITY, enc_key->key());
			auth_info.dPrint( D_SECURITY );
		}

		new_session = false;
	} else {
		if( !m_sec_man.FillInSecurityPolicyAd( CLIENT_PERM, &auth_info,
									 m_can_negotiate) ) {
				// security policy was invalid.  bummer.
			dprintf( D_ALWAYS, 
					 "SECMAN: ERROR: The security policy is invalid.\n" );
			m_errstack->push("SECMAN", SECMAN_ERR_INVALID_POLICY,
				"Configuration Problem: The security policy is invalid.\n" );
			return StartCommandFailed;
		}

		if (DebugFlags & D_FULLDEBUG) {
			dprintf (D_SECURITY, "SECMAN: no cached key for %s.\n", keybuf);
		}

		// no sessions in udp
		if (is_tcp) {
			// for now, always open a session for tcp.
			new_session = true;
			sprintf (buf, "%s=\"YES\"", ATTR_SEC_NEW_SESSION);
			auth_info.Insert(buf);
		}
	}

	
	if (DebugFlags & D_FULLDEBUG) {
		dprintf (D_SECURITY, "SECMAN: Security Policy:\n");
		auth_info.dPrint( D_SECURITY );
	}


	// find out our negotiation policy.
	SecMan::sec_req negotiation = m_sec_man.sec_lookup_req( auth_info, ATTR_SEC_NEGOTIATION );
	if (negotiation == SecMan::SEC_REQ_UNDEFINED) {
		// this code never executes, as a default was already stuck into the
		// classad.
		negotiation = SecMan::SEC_REQ_PREFERRED;
		dprintf(D_SECURITY, "SECMAN: missing negotiation attribute, assuming PREFERRED.\n");
	}

	SecMan::sec_feat_act negotiate = m_sec_man.sec_req_to_feat_act(negotiation);
	if (negotiate == SecMan::SEC_FEAT_ACT_NO) {
		// old way:
		// code the int and be done.  there is no easy way to try the
		// new way if the old way fails, since it will fail outside
		// the scope of this function.

		if (DebugFlags & D_FULLDEBUG) {
			dprintf(D_SECURITY, "SECMAN: not negotiating, just sending command (%i)\n", m_cmd);
		}

		// just code the command and be done
		m_sock->encode();
		m_sock->code(m_cmd);

		// we must _NOT_ do an eom() here!  Ques?  See Todd or Zach 9/01

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

	if (DebugFlags & D_FULLDEBUG) {
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

	MyString destsinful = sin_to_string(m_sock->endpoint());
	MyString oursinful = global_dc_sinful();
	bool using_cookie = false;

	if (destsinful == oursinful) {
		// use a cookie.
		int len = 0;
		unsigned char* randomjunk = NULL;

		global_dc_get_cookie (len, randomjunk);
		
		sprintf (buf, "%s=\"%s\"", ATTR_SEC_COOKIE, randomjunk);
		free(randomjunk);
		randomjunk = NULL;

		auth_info.Insert(buf);
		dprintf (D_SECURITY, "SECMAN: %s\n", buf);

		using_cookie = true;

	} else if ((!have_session) && (!is_tcp) && (!m_already_tried_TCP_auth)) {
		// can't use a cookie, so try to start a session.

		if(m_nonblocking) {
				// Check if there is already a non-blocking TCP auth in progress
			SecManStartCommand *sc = NULL;
			if(m_sec_man.tcp_auth_in_progress->lookup(m_session_key,sc) == 0) {
					// Rather than starting yet another TCP session for this session key,
					// simply add ourselves to the list of things waiting for the
					// pending session to be ready for use.

					// Do not allow ourselves to be deleted until after
					// ResumeAfterTCPAuth() is called.
				IncRefCount();

					// Make daemonCore aware that we are holding onto this
					// UDP socket while waiting for other events to complete.
				incrementPendingSockets();

				sc->m_waiting_for_tcp_auth.Append(this);

				if(DebugFlags & D_FULLDEBUG) {
					dprintf( D_SECURITY, "SECMAN: waiting for pending session %s to be ready\n",
							 m_session_key.Value());
				}

				if(!m_callback_fn) {
						// Caller wants us to go ahead and get a session key,
						// but caller will try sending the UDP command later,
						// rather than dealing with a callback.
					return StartCommandWouldBlock;
				}
				return StartCommandInProgress;
			}
		}

		if (DebugFlags & D_FULLDEBUG) {
			dprintf ( D_SECURITY, "SECMAN: need to start a session via TCP\n");
		}

		// we'll have to authenticate via TCP
		ReliSock *tcp_auth_sock = new ReliSock;

		ASSERT(tcp_auth_sock);

		// the timeout
		int TCP_SOCK_TIMEOUT = param_integer("SEC_TCP_SESSION_TIMEOUT", 20);
		if (DebugFlags & D_FULLDEBUG) {
			dprintf ( D_SECURITY, "SECMAN: setting timeout to %i seconds.\n", TCP_SOCK_TIMEOUT);
		}
		tcp_auth_sock->timeout(TCP_SOCK_TIMEOUT);

		// we already know the address - condor uses the same TCP port as it does UDP port.
		sprintf (buf, sin_to_string(m_sock->endpoint()));
		if (!tcp_auth_sock->connect(buf,0,m_nonblocking)) {
			dprintf ( D_SECURITY, "SECMAN: couldn't connect via TCP to %s, failing...\n", buf);
			m_errstack->pushf("SECMAN", SECMAN_ERR_CONNECT_FAILED,
					"TCP auth connection to %s failed\n", buf);
			delete tcp_auth_sock;
			return StartCommandFailed;
		}

		if(m_nonblocking && tcp_auth_sock->is_connect_pending()) {

			int reg_rc = daemonCoreSockAdapter.Register_Socket(
				tcp_auth_sock,
				"<StartCommand TCP Auth Socket>",
				(SocketHandlercpp)&SecManStartCommand::TCPAuthConnected,
				tcp_auth_sock->get_sinful_peer(),
				this,
				ALLOW);

			if(reg_rc < 0) {
				MyString msg;
				msg.sprintf("TCP auth connection to %s failed because "
				            "Register_Socket returned %d",
				            m_sock->get_sinful_peer(),
				            reg_rc);
				dprintf(D_SECURITY, "SECMAN: %s\n", msg.Value());
				m_errstack->pushf("SECMAN", SECMAN_ERR_CONNECT_FAILED,
				                  "%s\n", msg.Value());

				delete tcp_auth_sock;
				return StartCommandFailed;
			}

			ASSERT(daemonCoreSockAdapter.Register_DataPtr( this ));

			// Do not allow ourselves to be deleted until after
			// ConnectCallbackFn is called.
			IncRefCount();

				// Make daemonCore aware that we are holding onto this
				// UDP socket while waiting for the (registered) TCP
				// socket to connect etc.
			incrementPendingSockets();

				// Make note that this operation to do the TCP
				// auth operation is in progress, so others
				// wanting the same session key can wait for it.
			SecMan::tcp_auth_in_progress->insert(m_session_key,this);

			if(!m_callback_fn) {
					// Caller wants us to go ahead and get a session key,
					// but caller will try sending the UDP command later,
					// rather than dealing with a callback.
				return StartCommandWouldBlock;
			}
			return StartCommandInProgress;
		}

		return TCPAuthConnected_inner( tcp_auth_sock );
	} else if ((!have_session) && (!is_tcp) && m_already_tried_TCP_auth) {
			// there still is no session.
			//
			// this means when i sent them the NOP, no session was started.
			// maybe it means their security policy doesn't negotiate.
			// we'll send them this packet either way... if they don't like
			// it, they won't listen.
		if (DebugFlags & D_FULLDEBUG) {
			dprintf ( D_SECURITY, "SECMAN: UDP has no session to use!\n");
		}

		ASSERT (enc_key == NULL);
	}

	// extract the version attribute current in the classad - it is
	// the version of the remote side.
	MyString remote_version;
	char * rvtmp = NULL;
	auth_info.LookupString ( ATTR_SEC_REMOTE_VERSION, &rvtmp );
	if (rvtmp) {
		remote_version = rvtmp;
		free(rvtmp);
	} else {
		remote_version = "unknown";
	}

	// fill in our version
	sprintf(buf, "%s=\"%s\"", ATTR_SEC_REMOTE_VERSION, CondorVersion());
	auth_info.InsertOrUpdate(buf);

	// fill in return address, if we are a daemon
	char* dcss = global_dc_sinful();
	if (dcss) {
		sprintf(buf, "%s=\"%s\"", ATTR_SEC_SERVER_COMMAND_SOCK, dcss);
		auth_info.Insert(buf);
	}

	// fill in command
	sprintf(buf, "%s=%i", ATTR_SEC_COMMAND, m_cmd);
	auth_info.Insert(buf);

	if (m_cmd == DC_AUTHENTICATE) {
		// fill in sub-command
		sprintf(buf, "%s=%i", ATTR_SEC_AUTH_COMMAND, m_subcmd);
		auth_info.Insert(buf);
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


	if (!using_cookie && !is_tcp) {

		// udp works only with an already established session (gotten from a
		// tcp connection).  if there's no session, there's no way to enable
		// any features.  it could just be that this is a 6.2 daemon we are
		// talking to.  in that case, send the command the old way, as long
		// as that is permitted

		dprintf ( D_SECURITY, "SECMAN: UDP, have_session == %i, can_neg == %i\n",
				(have_session?1:0), (m_can_negotiate?1:0));

		if (have_session) {
			// UDP w/ session
			if (DebugFlags & D_FULLDEBUG) {
				dprintf ( D_SECURITY, "SECMAN: UDP has session %s.\n", enc_key->id());
			}

			SecMan::sec_feat_act will_authenticate = m_sec_man.sec_lookup_feat_act( auth_info, ATTR_SEC_AUTHENTICATION );
			SecMan::sec_feat_act will_enable_enc   = m_sec_man.sec_lookup_feat_act( auth_info, ATTR_SEC_ENCRYPTION );
			SecMan::sec_feat_act will_enable_mac   = m_sec_man.sec_lookup_feat_act( auth_info, ATTR_SEC_INTEGRITY );

			if (will_authenticate == SecMan::SEC_FEAT_ACT_UNDEFINED || 
				will_authenticate == SecMan::SEC_FEAT_ACT_INVALID || 
				will_enable_enc == SecMan::SEC_FEAT_ACT_UNDEFINED || 
				will_enable_enc == SecMan::SEC_FEAT_ACT_INVALID || 
				will_enable_mac == SecMan::SEC_FEAT_ACT_UNDEFINED || 
				will_enable_mac == SecMan::SEC_FEAT_ACT_INVALID ) {

				// suck.

				dprintf ( D_ALWAYS, "SECMAN: action attribute missing from classad\n");
				auth_info.dPrint( D_SECURITY );
				m_errstack->push( "SECMAN", SECMAN_ERR_ATTRIBUTE_MISSING,
						"Protocol Error: Action attribute missing");
				return StartCommandFailed;
			}


			KeyInfo* ki  = NULL;
			if (enc_key->key()) {
				ki  = new KeyInfo(*(enc_key->key()));
			}
			
			if (will_enable_mac == SecMan::SEC_FEAT_ACT_YES) {

				if (!ki) {
					dprintf ( D_ALWAYS, "SECMAN: enable_mac has no key to use, failing...\n");
					m_errstack->push( "SECMAN", SECMAN_ERR_NO_KEY,
							"Failed to establish a crypto key" );
					return StartCommandFailed;
				}

				if (DebugFlags & D_FULLDEBUG) {
					dprintf (D_SECURITY, "SECMAN: about to enable message authenticator.\n");
					m_sec_man.key_printf(D_SECURITY, ki);
				}

				// prepare the buffer to pass in udp header
				sprintf(buf, "%s", enc_key->id());

				// stick our command socket sinful string in there
				char* dcsss = global_dc_sinful();
				if (dcsss) {
					strcat (buf, ",");
					strcat (buf, dcsss);
				}

				m_sock->encode();
				m_sock->set_MD_mode(MD_ALWAYS_ON, ki, buf);

				dprintf ( D_SECURITY, "SECMAN: successfully enabled message authenticator!\n");
			} // if (will_enable_mac)

			if (will_enable_enc == SecMan::SEC_FEAT_ACT_YES) {

				if (!ki) {
					dprintf ( D_ALWAYS, "SECMAN: enable_enc no key to use, failing...\n");
					m_errstack->push( "SECMAN", SECMAN_ERR_NO_KEY,
							"Failed to establish a crypto key" );
					return StartCommandFailed;
				}

				if (DebugFlags & D_FULLDEBUG) {
					dprintf (D_SECURITY, "SECMAN: about to enable encryption.\n");
					m_sec_man.key_printf(D_SECURITY, ki);
				}

				// prepare the buffer to pass in udp header
				sprintf(buf, "%s", enc_key->id());

				// stick our command socket sinful string in there
				char* dcsss = global_dc_sinful();
				if (dcsss) {
					strcat (buf, ",");
					strcat (buf, dcsss);
				}


				m_sock->encode();
				m_sock->set_crypto_key(true, ki, buf);

				dprintf ( D_SECURITY, "SECMAN: successfully enabled encryption!\n");
			} // if (will_enable_enc)

			if (ki) {
				delete ki;
			}
		} else {
			// UDP the old way...  who knows if they get it, we'll just assume they do.
			m_sock->encode();
			m_sock->code(m_cmd);
			return StartCommandSucceeded;
		}
	}


	// now send the actual DC_AUTHENTICATE command
	if (DebugFlags & D_FULLDEBUG) {
		dprintf ( D_SECURITY, "SECMAN: sending DC_AUTHENTICATE command\n");
	}
	int authcmd = DC_AUTHENTICATE;
    m_sock->encode();
	if (! m_sock->code(authcmd)) {
		dprintf ( D_ALWAYS, "SECMAN: failed to send DC_AUTHENTICATE\n");
		m_errstack->push( "SECMAN", SECMAN_ERR_COMMUNICATIONS_ERROR,
						"Failed to send DC_AUTHENTICATE message" );
		return StartCommandFailed;
	}


	if (DebugFlags & D_FULLDEBUG) {
		dprintf ( D_SECURITY, "SECMAN: sending following classad:\n");
		auth_info.dPrint ( D_SECURITY );
	}

	// send the classad
	if (! auth_info.put(*m_sock)) {
		dprintf ( D_ALWAYS, "SECMAN: failed to send auth_info\n");
		m_errstack->push( "SECMAN", SECMAN_ERR_COMMUNICATIONS_ERROR,
						"Failed to send auth_info" );
		return StartCommandFailed;
	}


	if (is_tcp) {

		if (! m_sock->end_of_message()) {
			dprintf ( D_ALWAYS, "SECMAN: failed to end classad message\n");
			m_errstack->push( "SECMAN", SECMAN_ERR_COMMUNICATIONS_ERROR,
						"Failed to end classad message" );
			return StartCommandFailed;
		}

		if (m_sec_man.sec_lookup_feat_act(auth_info, ATTR_SEC_ENACT) != SecMan::SEC_FEAT_ACT_YES) {

			// if we asked them what to do, get their response
			ASSERT (is_tcp);

			ClassAd auth_response;
			m_sock->decode();

			if (!auth_response.initFromStream(*m_sock) ||
				!m_sock->end_of_message() ) {

				// if we get here, it means the serve accepted our connection
				// but dropped it after we sent the DC_AUTHENTICATE.  it probably
				// doesn't understand that command, so let's attempt to send it
				// the old way, IF negotiation wasn't REQUIRED.

				// set this input/output parameter to reflect
				m_can_negotiate = false;

				if (negotiation == SecMan::SEC_REQ_REQUIRED) {
					dprintf ( D_ALWAYS, "SECMAN: no classad from server, failing\n");
					m_errstack->push( "SECMAN", SECMAN_ERR_COMMUNICATIONS_ERROR,
						"Failed to end classad message" );
					return StartCommandFailed;
				}

				// we'll try to send it the old way.
				dprintf (D_SECURITY, "SECMAN: negotiation failed.  trying the old way...\n");

				// this is kind of ugly:  close and reconnect the socket.
				// seems to work though! :)

				char addrbuf[128];
				sprintf (addrbuf, sin_to_string(m_sock->endpoint()));
				m_sock->close();
				//sleep( 1 );

				if (!m_sock->connect(addrbuf)) {
					dprintf ( D_SECURITY, "SECMAN: couldn't connect via TCP to %s, failing...\n", addrbuf);
					m_errstack->pushf( "SECMAN", SECMAN_ERR_CONNECT_FAILED,
						"TCP connection to %s failed\n", addrbuf);
					return StartCommandFailed;
				}

				m_sock->encode();
				m_sock->code(m_cmd);
				return StartCommandSucceeded;
			}


			if (DebugFlags & D_FULLDEBUG) {
				dprintf ( D_SECURITY, "SECMAN: server responded with:\n");
				auth_response.dPrint( D_SECURITY );
			}

			// it makes a difference if the version is empty, so we must
			// explicitly delete it before we copy it.
			auth_info.Delete(ATTR_SEC_REMOTE_VERSION);
			m_sec_man.sec_copy_attribute( auth_info, auth_response, ATTR_SEC_REMOTE_VERSION );
			m_sec_man.sec_copy_attribute( auth_info, auth_response, ATTR_SEC_ENACT );
			m_sec_man.sec_copy_attribute( auth_info, auth_response, ATTR_SEC_AUTHENTICATION_METHODS_LIST );
			m_sec_man.sec_copy_attribute( auth_info, auth_response, ATTR_SEC_AUTHENTICATION_METHODS );
			m_sec_man.sec_copy_attribute( auth_info, auth_response, ATTR_SEC_CRYPTO_METHODS );
			m_sec_man.sec_copy_attribute( auth_info, auth_response, ATTR_SEC_AUTHENTICATION );
			m_sec_man.sec_copy_attribute( auth_info, auth_response, ATTR_SEC_ENCRYPTION );
			m_sec_man.sec_copy_attribute( auth_info, auth_response, ATTR_SEC_INTEGRITY );
			m_sec_man.sec_copy_attribute( auth_info, auth_response, ATTR_SEC_SESSION_DURATION );
			// sec_copy_attribute( auth_info, auth_response, ATTR_SEC_VALID_COMMANDS );
			// sec_copy_attribute( auth_info, auth_response, ATTR_SEC_USER );
			// sec_copy_attribute( auth_info, auth_response, ATTR_SEC_SID );

			auth_info.Delete(ATTR_SEC_NEW_SESSION);

			sprintf(buf, "%s=\"YES\"", ATTR_SEC_USE_SESSION);
			auth_info.Insert(buf);

			m_sock->encode();

		}

		SecMan::sec_feat_act will_authenticate = m_sec_man.sec_lookup_feat_act( auth_info, ATTR_SEC_AUTHENTICATION );
		SecMan::sec_feat_act will_enable_enc   = m_sec_man.sec_lookup_feat_act( auth_info, ATTR_SEC_ENCRYPTION );
		SecMan::sec_feat_act will_enable_mac   = m_sec_man.sec_lookup_feat_act( auth_info, ATTR_SEC_INTEGRITY );

		if (will_authenticate == SecMan::SEC_FEAT_ACT_UNDEFINED || 
			will_authenticate == SecMan::SEC_FEAT_ACT_INVALID || 
			will_enable_enc == SecMan::SEC_FEAT_ACT_UNDEFINED || 
			will_enable_enc == SecMan::SEC_FEAT_ACT_INVALID || 
			will_enable_mac == SecMan::SEC_FEAT_ACT_UNDEFINED || 
			will_enable_mac == SecMan::SEC_FEAT_ACT_INVALID ) {

			// missing some essential info.

			dprintf ( D_SECURITY, "SECMAN: action attribute missing from classad, failing!\n");
			auth_info.dPrint( D_SECURITY );
			m_errstack->push( "SECMAN", SECMAN_ERR_ATTRIBUTE_MISSING,
						"Protocol Error: Action attribute missing");
			return StartCommandFailed;
		}

		// protocol fix:
		//
		// up to and including 6.6.0, will_authenticate would be set to true
		// if we are resuming a session that was authenticated.  this is not
		// necessary.
		//
		// so, as of 6.6.1, if we are resuming a session (as determined
		// by the expression (!new_session), AND the other side is 6.6.1
		// or higher, we will force will_authenticate to SEC_FEAT_ACT_NO.
		//
		// we can tell easily if the other side is 6.6.1 or higher by the
		// mere presence of the version, since that is when it was added.

		if ((will_authenticate == SecMan::SEC_FEAT_ACT_YES)) {
			if ((!new_session)) {
				if (remote_version != "unknown") {
					dprintf( D_SECURITY, "SECMAN: resume, other side is %s, NOT reauthenticating.\n", remote_version.Value() );
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



		// the private key, if there is one.
		KeyInfo* ki  = NULL;

		if (will_authenticate == SecMan::SEC_FEAT_ACT_YES) {

			ASSERT (m_sock->type() == Stream::reli_sock);

			if (DebugFlags & D_FULLDEBUG) {
				dprintf ( D_SECURITY, "SECMAN: authenticating RIGHT NOW.\n");
			}
			char * auth_methods = NULL;
			auth_info.LookupString( ATTR_SEC_AUTHENTICATION_METHODS_LIST, &auth_methods );
			if (auth_methods) {
				if (DebugFlags & D_FULLDEBUG) {
					dprintf (D_SECURITY, "SECMAN: AuthMethodsList: %s\n", auth_methods);
				}
			} else {
				// lookup the 6.4 attribute name
				auth_info.LookupString( ATTR_SEC_AUTHENTICATION_METHODS, &auth_methods );
				if (DebugFlags & D_FULLDEBUG) {
					dprintf (D_SECURITY, "SECMAN: AuthMethods: %s\n", auth_methods);
				}
			}

			if (!auth_methods) {
				// there's no auth methods.
				dprintf ( D_ALWAYS, "SECMAN: no auth method!, failing.\n");
				m_errstack->push( "SECMAN", SECMAN_ERR_ATTRIBUTE_MISSING,
						"Protocol Error: No auth methods");
				return StartCommandFailed;
			} else {
				dprintf ( D_SECURITY, "SECMAN: Auth methods: %s\n", auth_methods);
			}

			if (!m_sock->authenticate(ki, auth_methods, m_errstack)) {
				if(ki) {
					delete ki;
				}
            	if (auth_methods) {  
                	free(auth_methods);
            	}
				return StartCommandFailed;
			}
            if (auth_methods) {  
                free(auth_methods);
            }
		} else {
			// !new_session is equivilant to use_session in this client.
			if (!new_session) {
				// we are using this key
				if (enc_key && enc_key->key()) {
					ki = new KeyInfo(*(enc_key->key()));
				} else {
					ASSERT (ki == NULL);
				}
			}
		}

		
		if (will_enable_mac == SecMan::SEC_FEAT_ACT_YES) {

			if (!ki) {
				dprintf ( D_ALWAYS, "SECMAN: enable_mac has no key to use, failing...\n");
				m_errstack->push ("SECMAN", SECMAN_ERR_NO_KEY,
							"Failed to establish a crypto key" );
				return StartCommandFailed;
			}

			if (DebugFlags & D_FULLDEBUG) {
				dprintf (D_SECURITY, "SECMAN: about to enable message authenticator.\n");
				m_sec_man.key_printf(D_SECURITY, ki);
			}

			m_sock->encode();
			m_sock->set_MD_mode(MD_ALWAYS_ON, ki);

			dprintf ( D_SECURITY, "SECMAN: successfully enabled message authenticator!\n");
		} else {
			// we aren't going to enable MD5.  but we should still set the secret key
			// in case we decide to turn it on later.
			m_sock->encode();
			m_sock->set_MD_mode(MD_OFF, ki);
		}

		if (will_enable_enc == SecMan::SEC_FEAT_ACT_YES) {

			if (!ki) {
				dprintf ( D_ALWAYS, "SECMAN: enable_enc no key to use, failing...\n");
				m_errstack->push ("SECMAN", SECMAN_ERR_NO_KEY,
							"Failed to establish a crypto key" );
				return StartCommandFailed;
			}

			if (DebugFlags & D_FULLDEBUG) {
				dprintf (D_SECURITY, "SECMAN: about to enable encryption.\n");
				m_sec_man.key_printf(D_SECURITY, ki);
			}

			m_sock->encode();
			m_sock->set_crypto_key(true, ki);

			dprintf ( D_SECURITY, "SECMAN: successfully enabled encryption!\n");
		} else {
			// we aren't going to enable encryption for everything.  but we should
			// still have a secret key ready to go in case someone decides to turn
			// it on later.
			m_sock->encode();
			m_sock->set_crypto_key(false, ki);
		}
		
		if (new_session) {
			// receive a classAd containing info such as: well, nothing yet
			m_sock->encode();
			m_sock->eom();

			ClassAd post_auth_info;
			m_sock->decode();
			if (!post_auth_info.initFromStream(*m_sock) || !m_sock->eom()) {
				dprintf (D_ALWAYS, "SECMAN: could not receive session info, failing!\n");
				m_errstack->push ("SECMAN", SECMAN_ERR_COMMUNICATIONS_ERROR,
							"could not receive post_auth_info" );
				return StartCommandFailed;
			} else {
				if (DebugFlags & D_FULLDEBUG) {
					dprintf (D_SECURITY, "SECMAN: received post-auth classad:\n");
					post_auth_info.dPrint (D_SECURITY);
				}
			}

			// bring in the session ID
			m_sec_man.sec_copy_attribute( auth_info, post_auth_info, ATTR_SEC_SID );

			// other attributes
			m_sec_man.sec_copy_attribute( auth_info, post_auth_info, ATTR_SEC_USER );
			m_sec_man.sec_copy_attribute( auth_info, post_auth_info, ATTR_SEC_VALID_COMMANDS );

			if (DebugFlags & D_FULLDEBUG) {
				dprintf (D_SECURITY, "SECMAN: policy to be cached:\n");
				auth_info.dPrint(D_SECURITY);
			}

			char *sesid = NULL;
			auth_info.LookupString(ATTR_SEC_SID, &sesid);
			if (sesid == NULL) {
				dprintf (D_ALWAYS, "SECMAN: session id is NULL, failing\n");
				m_errstack->push( "SECMAN", SECMAN_ERR_ATTRIBUTE_MISSING,
						"Failed to lookup session id");
				return StartCommandFailed;
			}

			char *cmd_list = NULL;
			auth_info.LookupString(ATTR_SEC_VALID_COMMANDS, &cmd_list);
			if (cmd_list == NULL) {
				dprintf (D_ALWAYS, "SECMAN: valid commands is NULL, failing\n");
				m_errstack->push( "SECMAN", SECMAN_ERR_ATTRIBUTE_MISSING,
						"Protocol Failure: Unable to lookup valid commands");
				delete sesid;
				return StartCommandFailed;
			}


			ASSERT (enc_key == NULL);


			// extract the session duration
			char *dur = NULL;
			auth_info.LookupString(ATTR_SEC_SESSION_DURATION, &dur);

			int expiration_time = time(0) + atoi(dur);

				// This makes a copy of the policy ad, so we don't
				// have to. 
			KeyCacheEntry tmp_key( sesid, m_sock->endpoint(), ki,
								   &auth_info, expiration_time ); 
			dprintf (D_SECURITY, "SECMAN: added session %s to cache for %s seconds.\n", sesid, dur);

            if (dur) {
                free(dur);
            }

			// stick the key in the cache
			m_sec_man.session_cache->insert(tmp_key);


			// now add entrys which map all the {<sinful_string>,<command>} pairs
			// to the same key id (which is in the variable sesid)

			StringList coms(cmd_list);
			char *p;

			coms.rewind();
			while ( (p = coms.next()) ) {
				sprintf (keybuf, "{%s,<%s>}", sin_to_string(m_sock->endpoint()), p);

				// NOTE: HashTable returns ZERO on SUCCESS!!!
				if (m_sec_man.command_map->insert(keybuf, sesid) == 0) {
					// success
					if (DebugFlags & D_FULLDEBUG) {
						dprintf (D_SECURITY, "SECMAN: command %s mapped to session %s.\n", keybuf, sesid);
					}
				} else {
					// perhaps there is an old entry under the same name.  we should
					// delete the old one and insert the new one.

					// NOTE: HashTable's remove returns ZERO on SUCCESS!!!
					if (m_sec_man.command_map->remove(keybuf) == 0) {
						// now let's try to insert again (zero on success)
						if (m_sec_man.command_map->insert(keybuf, sesid) == 0) {
							if (DebugFlags & D_FULLDEBUG) {
								dprintf (D_SECURITY,
										"SECMAN: command %s remapped to "
										"session %s!\n",
										keybuf, sesid);
							}
						} else {
							if (DebugFlags & D_FULLDEBUG) {
								dprintf (D_SECURITY, "SECMAN: command %s NOT mapped (insert failed!)\n", keybuf);
							}
						}
					} else {
						if (DebugFlags & D_FULLDEBUG) {
							dprintf (D_SECURITY, "SECMAN: command %s NOT mapped (remove failed!)\n", keybuf);
						}
					}
				}
			}
			
			free( sesid );
            free( cmd_list );

		} // if (new_session)

		// clean up
		if (ki) {
			delete ki;
		}

	} // if (is_tcp)

	m_sock->encode();
	m_sock->allow_one_empty_message();
	dprintf ( D_SECURITY, "SECMAN: startCommand succeeded.\n");

	return StartCommandSucceeded;
}

int
SecManStartCommand::TCPAuthConnected( Stream *stream )
{
	Sock *sock = (Sock *)stream;

	daemonCoreSockAdapter.Cancel_Socket( stream );

	if(DebugFlags & D_FULLDEBUG) {
		dprintf(D_SECURITY,"Non-blocking connection for TCP authentication to %s finished (connected=%d)\n",sock->get_sinful_peer(),sock->is_connected());
	}

	bool auth_succeeded = false;
	StartCommandResult rc = TCPAuthConnected_inner(sock,&auth_succeeded);

	if(m_callback_fn) {
		bool success = rc == StartCommandSucceeded;
		(*m_callback_fn)(success,m_sock,m_errstack,m_misc_data);

			// Caller is responsible for deallocating the following:
		m_errstack = NULL;
		m_sock = NULL;
	}
	else {
		if ( DebugFlags & D_FULLDEBUG) {
			dprintf( D_SECURITY,
				"SECMAN: caller did not want a callback, "
				"so aborting startCommand(%d) to %s\n",
				 m_cmd,
				 m_sock->get_sinful_peer());
		}

		// The original caller should have left one remaining
		// reference count in order to assure this object would
		// persist, but now we are done, so get rid of it.
		DecRefCount();
	}

		// Remove ourselves from SecMan's list of pending TCP auth sessions.
	SecManStartCommand *sc = NULL;
	if(SecMan::tcp_auth_in_progress->lookup(m_session_key,sc) == 0 && sc == this) {
		ASSERT(SecMan::tcp_auth_in_progress->remove(m_session_key) == 0);
	}

		// Iterate through the list of objects waiting for our TCP auth session
		// to be done.
	m_waiting_for_tcp_auth.Rewind();
	while( m_waiting_for_tcp_auth.Next(sc) ) {
		if(!auth_succeeded) {
			sc->m_errstack->pushf("SECMAN", SECMAN_ERR_NO_SESSION,
			                      "Was waiting for TCP auth session to %s, "
			                      "but it failed.",
			                      sc->m_sock->get_sinful_peer());
		}
		sc->ResumeAfterTCPAuth(auth_succeeded);

			// This instance had IncRefCount() called when getting
			// added to this list.  Now we are done with the
			// reference.

		sc->DecRefCount();
	}
	m_waiting_for_tcp_auth.Clear();

	// We did IncRefCount() when registering this callback.
	// Now we are done with the reference.
	DecRefCount();

	return KEEP_STREAM;
}

StartCommandResult
SecManStartCommand::ResumeAfterTCPAuth_inner()
{
	// Now try again to initiate the UDP command.  This time,
	// we should have a session key cached as a consequence
	// of the TCP exchange we just did, but just in case
	// we don't have one, disable the code path that landed us
	// here so we don't get into an infinite loop.
	m_already_tried_TCP_auth = true;

	if(m_nonblocking && !m_callback_fn) {
		// Caller wanted us to get a session key but did not
		// want to bother about handling a callback.  Therefore,
		// we are done.  No need to start the command again.
		return StartCommandSucceeded;
	}

	return startCommand_inner();
}

void
SecManStartCommand::ResumeAfterTCPAuth(bool auth_succeeded)
{
		// We get here when we needed a session, and some other
		// instance of SecManStartCommand() was already in the process
		// of getting one that we could use.  When that object
		// finished getting the session, it called us here.

	StartCommandResult rc = StartCommandFailed;

	if(DebugFlags & D_FULLDEBUG) {
		dprintf(D_SECURITY,"SECMAN: done waiting for TCP auth to %s\n",
		        m_sock->get_sinful_peer());
	}

	if(auth_succeeded) {
		rc = ResumeAfterTCPAuth_inner();
	}

	if(m_callback_fn) {
		bool success = rc == StartCommandSucceeded;
		(*m_callback_fn)(success,m_sock,m_errstack,m_misc_data);

			// Caller is responsible for deallocating the following:
		m_errstack = NULL;
		m_sock = NULL;
	}
	else {
		if ( DebugFlags & D_FULLDEBUG) {
			dprintf( D_SECURITY,
				"SECMAN: caller did not want a callback, "
				"so aborting startCommand(%d) to %s\n",
				 m_cmd,
				 m_sock->get_sinful_peer());
		}

		// The original caller should have left one remaining
		// reference count in order to assure this object would
		// persist, but now we are done, so get rid of it.
		DecRefCount();
	}
}


StartCommandResult
SecManStartCommand::TCPAuthConnected_inner( Sock *tcp_auth_sock, bool *auth_succeeded )
{

	// We are now connected to the TCP authentication socket.
	// We got here because we wanted to start a UDP command,
	// but we didn't have a session key.  We now get a session
	// key and then resume with the UDP command.

	// NOTE: the caller of this function must ensure that
	// the m_callback_fn is called (if there is one).

	if(auth_succeeded) {
		*auth_succeeded = false; // set default result
	}

	if(!tcp_auth_sock->is_connected()) {
		dprintf(D_SECURITY,"SECMAN: failed in non-blocking TCP connect to %s",
				tcp_auth_sock->get_sinful_peer());
		m_errstack->pushf("SECMAN", SECMAN_ERR_CONNECT_FAILED,
		                "TCP connection to %s failed\n",
		                tcp_auth_sock->get_sinful_peer());

		delete tcp_auth_sock;
		return StartCommandFailed;
	}

	// For now, we do the TCP command as a blocking call,
	// because it happens to use blocking read/writes anyway.

	const bool auth_nonblocking = false;
	// Since we're using it in blocking mode, no need for putting
	// SecManStartCommand on the heap.
	SecManStartCommand sc(
		DC_AUTHENTICATE,
		tcp_auth_sock,
		m_can_negotiate,
		m_errstack,
		m_cmd,
		NULL,
		NULL,
		auth_nonblocking,
		&m_sec_man);

	StartCommandResult auth_result = sc.startCommand();
	if(auth_succeeded) {
		*auth_succeeded = auth_result == StartCommandSucceeded;
	}

	// in case we discovered anything new about security negotiation
	m_can_negotiate = sc.m_can_negotiate;

	if (DebugFlags & D_FULLDEBUG) {
		dprintf ( D_SECURITY, "SECMAN: sending eom() and closing TCP sock.\n");
	}

		// close the TCP socket, the rest will be UDP.
	tcp_auth_sock->eom();
	tcp_auth_sock->close();
	delete tcp_auth_sock;
	tcp_auth_sock = NULL;


	if (auth_result != StartCommandSucceeded) {
		dprintf ( D_SECURITY,"SECMAN: unable to start session to %s via TCP, "
		          "failing.\n",m_sock->get_sinful_peer());
		m_errstack->pushf("SECMAN", SECMAN_ERR_NO_SESSION,
		                 "Failed to start a session to %s with TCP",
		                 m_sock->get_sinful_peer());
		return StartCommandFailed;
	}
	if (DebugFlags & D_FULLDEBUG) {
		dprintf ( D_SECURITY, "SECMAN: succesfully sent NOP via TCP!\n");
	}

	return ResumeAfterTCPAuth_inner();
}

void
SecManStartCommand::DestructCallerData()
{
	if(m_sock) {
		delete m_sock;
		m_sock = NULL;
	}
	if(m_errstack) {
		delete m_errstack;
		m_errstack = NULL;
	}
}


// Given a sinful string, clear out any associated sessions (incoming or outgoing)
bool SecMan :: invalidateHost(const char * sin)
{
    bool removed = true;

    if (sin && sin[0]) {
        KeyCacheEntry * keyEntry = NULL;
        MyString  addr(sin), id;

        if (session_cache) {
            session_cache->key_table->startIterations();
            while (session_cache->key_table->iterate(id, keyEntry)) {
				char * remote_sinful = sin_to_string(keyEntry->addr());
					// if this is an outgoing session, we need to check against the keyEntry->addr()
				if (remote_sinful && remote_sinful[0] &&  (addr == MyString(remote_sinful))) {
					if (DebugFlags & D_FULLDEBUG) {
						dprintf (D_SECURITY, "KEYCACHE: removing session %s for %s\n", id.Value(), remote_sinful);
					}
					remove_commands(keyEntry);	// removing mapping from DC command int to session entry
					session_cache->remove(id.Value());	// remove session entry
				} else {
						// if it did not match, it might be an incoming session, so check against the
						// ServerCommandSock that is in the cached policy classad.
					char * local_sinful = NULL;
					keyEntry->policy()->LookupString( ATTR_SEC_SERVER_COMMAND_SOCK, &local_sinful);
					if (local_sinful && local_sinful[0] && (addr == MyString(local_sinful))) {
						if (DebugFlags & D_FULLDEBUG) {
							dprintf (D_SECURITY, "KEYCACHEX: removing session %s for %s\n", id.Value(), local_sinful);
						}
						// remove_commands shouldn't be necessary for incoming connections
						// remove_commands(keyEntry);
						session_cache->remove(id.Value());
					}
					if (local_sinful) {
						free(local_sinful);
					}
				}
            }
        }
    }
    else {
        // sock is NULL!
    }

    return removed;
}

bool SecMan :: invalidateByParentAndPid(const char * parent, int pid) {
	if (parent && parent[0]) {

    	KeyCacheEntry * keyEntry = NULL;
        MyString  id;

		if (session_cache) {
			session_cache->key_table->startIterations();
			while (session_cache->key_table->iterate(id, keyEntry)) {

				char * parent_unique_id = NULL;
				int    tpid = 0;

				keyEntry->policy()->LookupString( ATTR_SEC_PARENT_UNIQUE_ID, &parent_unique_id);
				keyEntry->policy()->LookupInteger( ATTR_SEC_SERVER_PID, tpid);

				if (parent_unique_id && parent_unique_id[0] && (strcmp(parent, parent_unique_id) == 0) && (pid == tpid)) {
					if (DebugFlags & D_FULLDEBUG) {
						dprintf (D_SECURITY, "KEYCACHE: removing session %s for %s\n", id.Value(), parent_unique_id);
					}
					// remove_commands shouldn't be necessary for incoming connections
					// remove_commands(keyEntry);
					session_cache->remove(id.Value());
				}
				if (parent_unique_id) {
					free(parent_unique_id);
					parent_unique_id = 0;
				}
			}
        }
	}
	return true;
}

bool SecMan :: invalidateKey(const char * key_id)
{
    bool removed = true;
    KeyCacheEntry * keyEntry = NULL;

    // What if session_cache is NULL but command_cache is not?
	if (session_cache) {

        session_cache->lookup(key_id, keyEntry);

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
        char * addr = strdup(sin_to_string(keyEntry->addr()));
    
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
                    sprintf (keybuf, "{%s,<%s>}", addr, cmd);
                    command_map->remove(keybuf);
                }
            }
        }
        free(addr);
    }
}

int
SecMan::sec_char_to_auth_method( char* method ) {
    if (!stricmp( method, "SSL" )  ) {
        return CAUTH_SSL;
    } else if (!stricmp( method, "GSI" )  ) {
		return CAUTH_GSI;
	} else if ( !stricmp( method, "NTSSPI" ) ) {
		return CAUTH_NTSSPI;
	} else if ( !stricmp( method, "PASSWORD" ) ) {
		return CAUTH_PASSWORD;
	} else if ( !stricmp( method, "FS" ) ) {
		return CAUTH_FILESYSTEM;
	} else if ( !stricmp( method, "FS_REMOTE" ) ) {
		return CAUTH_FILESYSTEM_REMOTE;
	} else if ( !stricmp( method, "KERBEROS" ) ) {
		return CAUTH_KERBEROS;
	} else if ( !stricmp( method, "CLAIMTOBE" ) ) {
		return CAUTH_CLAIMTOBE;
	} else if ( !stricmp( method, "ANONYMOUS" ) ) {
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
			if (!stricmp(sm, cm)) {
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


SecMan::SecMan(int nbuckets) {
	// session_cache is a static member... we only
	// want to construct it ONCE.
	if (session_cache == NULL) {
		session_cache = new KeyCache(nbuckets);
	}
	if (command_map == NULL) {
		command_map = new HashTable<MyString,MyString>(nbuckets, MyStringHash, rejectDuplicateKeys);
	}
	if (tcp_auth_in_progress == NULL) {
		tcp_auth_in_progress = new HashTable<MyString,class SecManStartCommand *>(256, MyStringHash, rejectDuplicateKeys);
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


bool
SecMan::sec_copy_attribute( ClassAd &dest, ClassAd &source, const char* attr ) {
	ExprTree *e = source.Lookup(attr);
	if (e) {
		ExprTree *cp = e->DeepCopy();
		dest.Insert(cp);
		return true;
	} else {
		return false;
	}
}



void
SecMan::invalidateAllCache() {
	delete session_cache;
	session_cache = new KeyCache(209);

	delete command_map;
	command_map = new HashTable<MyString,MyString>(209, MyStringHash, rejectDuplicateKeys);
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
			have_session = false;

			// close this connection and start a new one
			if (!sock->close()) {
				dprintf ( D_ALWAYS, "SECMAN: could not close socket to %s\n",
						sin_to_string(sock->endpoint()));
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
			if (!sock->connect(sin_to_string(sock->endpoint()), 0)) {
				dprintf ( D_ALWAYS, "SECMAN: could not reconnect to %s.\n",
						sin_to_string(sock->endpoint()));
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

void SecMan::send_invalidate_packet ( char* sinful, char* sessid ) {
	if ( sinful ) {
		SafeSock s;
		if (s.connect(sinful)) {
			s.encode();
			s.put(DC_INVALIDATE_KEY);
			s.code(sessid);
			s.eom();
			s.close();
			dprintf (D_SECURITY, "DC_AUTHENTICATE: sent DC_INVALIDATE %s to %s.\n",
				sessid, sinful);
		} else {
			dprintf (D_SECURITY, "DC_AUTHENTICATE: couldn't send DC_INVALIDATE %s to %s.\n",
				sessid, sinful);
		}
	} else {
		dprintf (D_SECURITY, "DC_AUTHENTICATE: couldn't invalidate session %s... don't know who it is from!\n", sessid);
	}
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
        tid.sprintf( "%s:%i:%i", my_hostname(), mypid, (int)time(0));

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
	return s->authenticate(methods.Value(),errstack);
}

int
SecMan::authenticate_sock(Sock *s,KeyInfo *&ki, DCpermission perm, CondorError* errstack)
{
	MyString methods;
	getAuthenticationMethods( perm, &methods );
	ASSERT(s);
	return s->authenticate(ki,methods.Value(),errstack);
}
