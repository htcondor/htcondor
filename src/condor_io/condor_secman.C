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
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_ver_info.h"

#include "authentication.h"
#include "condor_string.h"
#include "condor_attributes.h"
#include "condor_adtypes.h"
#include "get_daemon_addr.h"
#include "get_full_hostname.h"
#include "my_hostname.h"
#include "internet.h"
#include "HashTable.h"
#include "KeyCache.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_secman.h"

extern char* mySubSystem;
extern char* global_dc_sinful();

#define SECURITY_HACK_ENABLE
void zz1printf(KeyInfo *k) {
	if (k) {
		char hexout[260];  // holds (at least) a 128 byte key.
		const unsigned char* dataptr = k->getKeyData();
		int   length  =  k->getKeyLength();

		for (int i = 0; (i < length) && (i < 24); i++) {
			sprintf (&hexout[i*2], "%02x", *dataptr++);
		}

		dprintf (D_SECURITY, "KEYCACHE: [%i] %s\n", length, hexout);
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
int SecMan::sec_man_ref_count = 0;

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
SecMan::sec_param( char* pname, sec_req def) {
	char *paramer = param(pname);
	if (paramer) {
		char buf[2];
		strncpy (buf, paramer, 1);
		free (paramer);

		sec_req res = sec_alpha_to_sec_req(buf);

		if (res == SEC_REQ_UNDEFINED || res == SEC_REQ_INVALID) {
			dprintf (D_ALWAYS,
					"SECMAN: %s is invalid, using default: %s!\n",
					pname, SecMan::sec_req_rev[def] );
			return def;
		}

		return res;
	}

	dprintf (D_SECURITY, "SECMAN: no %s defined, assuming %s\n",
				pname, SecMan::sec_req_rev[def]);
	return def;
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

ClassAd *
SecMan::CreateSecurityPolicyAd(const char *auth_level, bool other_side_can_negotiate) {

	char buf[256];

	// if auth_level is empty, use the default
	char def[] = "DEFAULT";
	if (auth_level == NULL || *auth_level == 0) {
		auth_level = def;
	}

	// get values from config file
	// first lookup SEC_<AUTHLEVEL>_BLAH.  if that fails,
	// lookup SEC_DEFAULT_BLAH instead.  if that fails, the
	// default value (OPTIONAL) is used.

	sprintf (buf, "SEC_%s_AUTHENTICATION", auth_level);
	sec_req sec_authentication = sec_param(buf);
	if (sec_authentication == SEC_REQ_UNDEFINED) {
		sec_authentication = sec_param("SEC_DEFAULT_AUTHENTICATION");
		if (sec_authentication == SEC_REQ_UNDEFINED) {
			sec_authentication = SEC_REQ_OPTIONAL;
		}
	}


	sprintf (buf, "SEC_%s_ENCRYPTION", auth_level);
	sec_req sec_encryption = sec_param(buf);
	if (sec_encryption == SEC_REQ_UNDEFINED) {
		sec_encryption = sec_param("SEC_DEFAULT_ENCRYPTION");
		if (sec_encryption == SEC_REQ_UNDEFINED) {
			sec_encryption = SEC_REQ_OPTIONAL;
		}
	}


	sprintf (buf, "SEC_%s_INTEGRITY", auth_level);
	sec_req sec_integrity = sec_param(buf);
	if (sec_integrity == SEC_REQ_UNDEFINED) {
		sec_integrity = sec_param("SEC_DEFAULT_INTEGRITY");
		if (sec_integrity == SEC_REQ_UNDEFINED) {
			sec_integrity = SEC_REQ_OPTIONAL;
		}
	}


	// regarding SEC_NEGOTIATE values:
	// REQUIRED- outgoing will always negotiate, and incoming must
	//           be negotiated as well.
	// PREFERRED- outgoing will try to negotiate but fall back to
	//            6.2 method if necessary.  incoming will allow
	//            negotiated & unnegotiated commands.
	// OPTIONAL- outgoing will be 6.2 style.  incoming will allow
	//           negotiated and unnegotiated commands.
	// NEVER- everything will be 6.2 style

	// the default is OPTIONAL

	sprintf (buf, "SEC_%s_NEGOTIATION", auth_level);
	sec_req sec_negotiation = sec_param(buf);
	if (sec_negotiation == SEC_REQ_UNDEFINED) {
		sec_negotiation = sec_param("SEC_DEFAULT_NEGOTIATION");
		if (sec_negotiation == SEC_REQ_UNDEFINED) {
			sec_negotiation = SEC_REQ_OPTIONAL;
		}
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
		return NULL;
	}

	// if we require negotiation and we know the other side can't speak
	// security negotiation, may as well fail now (as opposed to later)
	if (sec_negotiation == SEC_REQ_REQUIRED && other_side_can_negotiate == FALSE) {
		dprintf (D_SECURITY, "SECMAN: failure! SEC_NEGOTIATION "
				"is REQUIRED and other daemon is pre 6.3.2.\n");
		return NULL;
	}


	ClassAd * ad = new ClassAd();


	// for those of you reading this code, a 'paramer'
	// is a thing that param()s.
	char *paramer;

	// auth methods
	sprintf(buf, "SEC_%s_AUTHENTICATION_METHODS", auth_level);
	paramer = param(buf);
	if (paramer == NULL) {
		dprintf ( D_SECURITY, "SECMAN: param(\"%s\") == NULL\n", buf);
		paramer = param("SEC_DEFAULT_AUTHENTICATION_METHODS");
		if (paramer == NULL) {
#if defined(WIN32)
			// default windows method
			paramer = strdup("NTSSPI");
#else
			// default unix method
			paramer = strdup("FS");
#endif
			dprintf ( D_SECURITY, "SECMAN: param(\"SEC_DEFAULT_AUTHENTICATION_METHODS\") == NULL, using \"%s\"\n", paramer);
		} else {
			dprintf ( D_SECURITY, "SECMAN: param(\"SEC_DEFAULT_AUTHENTICATION_METHODS\") == %s\n", paramer );
		}
	} else {
		dprintf ( D_SECURITY, "SECMAN: param(\"%s\") == %s\n", buf, paramer );
	}

	if (paramer) {
		sprintf(buf, "%s=\"%s\"", ATTR_SEC_AUTHENTICATION_METHODS, paramer);
		free(paramer);

		ad->Insert(buf);
		dprintf ( D_SECURITY, "SECMAN: inserted '%s'\n", buf);
	}


	// crypto methods
	sprintf(buf, "SEC_%s_CRYPTO_METHODS", auth_level);
	paramer = param(buf);
	if (paramer == NULL) {
		dprintf ( D_SECURITY, "SECMAN: param(\"%s\") == NULL\n", buf);
		paramer = param("SEC_DEFAULT_CRYPTO_METHODS");
		if (paramer) {
			dprintf ( D_SECURITY, "SECMAN: param(\"SEC_DEFAULT_CRYPTO_METHODS\") == %s\n", paramer );
		} else {
			dprintf ( D_SECURITY, "SECMAN: param(\"SEC_DEFAULT_CRYPTO_METHODS\") == NULL.\n");
		}
	} else {
		dprintf ( D_SECURITY, "SECMAN: param(\"%s\") == %s\n", buf, paramer );
	}

	if (paramer) {
		sprintf(buf, "%s=\"%s\"", ATTR_SEC_CRYPTO_METHODS, paramer);
		free(paramer);

		ad->Insert(buf);
		dprintf ( D_SECURITY, "SECMAN: inserted '%s'\n", buf);
	} else {
		if (sec_encryption == SEC_REQ_REQUIRED || sec_integrity == SEC_REQ_REQUIRED) {
			dprintf ( D_SECURITY, "SECMAN: no crypto methods, but it was required! failing...\n");
		} else {
			dprintf ( D_SECURITY, "SECMAN: no crypto methods, disabling crypto.\n");
			sec_encryption = SEC_REQ_NEVER;
			sec_integrity = SEC_REQ_NEVER;
		}
	}


	// for now, we always want the top bracket of code.
	if ( TRUE ||
		 sec_is_negotiable(sec_authentication) || 
 		 sec_is_negotiable(sec_encryption) || 
 		 sec_is_negotiable(sec_integrity) ) {

		sprintf (buf, "%s=\"%s\"", ATTR_SEC_NEGOTIATION, SecMan::sec_req_rev[sec_negotiation]);
		ad->Insert(buf);

		sprintf (buf, "%s=\"%s\"", ATTR_SEC_AUTHENTICATION, SecMan::sec_req_rev[sec_authentication]);
		ad->Insert(buf);

		sprintf (buf, "%s=\"%s\"", ATTR_SEC_ENCRYPTION, SecMan::sec_req_rev[sec_encryption]);
		ad->Insert(buf);

		sprintf (buf, "%s=\"%s\"", ATTR_SEC_INTEGRITY, SecMan::sec_req_rev[sec_integrity]);
		ad->Insert(buf);

		sprintf (buf, "%s=\"%s\"", ATTR_SEC_ENACT, "NO");
		ad->Insert(buf);
	} else {
		sprintf (buf, "%s=\"%s\"", ATTR_SEC_NEGOTIATION, SecMan::sec_feat_act_rev[sec_req_to_feat_act(sec_negotiation)]);
		ad->Insert(buf);

		sprintf (buf, "%s=\"%s\"", ATTR_SEC_AUTHENTICATION, SecMan::sec_feat_act_rev[sec_req_to_feat_act(sec_authentication)]);
		ad->Insert(buf);

		sprintf (buf, "%s=\"%s\"", ATTR_SEC_ENCRYPTION, SecMan::sec_feat_act_rev[sec_req_to_feat_act(sec_encryption)]);
		ad->Insert(buf);

		sprintf (buf, "%s=\"%s\"", ATTR_SEC_INTEGRITY, SecMan::sec_feat_act_rev[sec_req_to_feat_act(sec_integrity)]);
		ad->Insert(buf);
		
		sprintf (buf, "%s=\"%s\"", ATTR_SEC_ENACT, "YES");
		ad->Insert(buf);
	}


	// subsystem
	sprintf(buf, "%s=\"%s\"", ATTR_SEC_SUBSYSTEM, mySubSystem);
	ad->Insert(buf);


	// key duration
	// ZKM TODO HACK
	// need to check kerb expiry.
	sprintf(buf, "SEC_%s_SESSION_DURATION", auth_level);
	paramer = param(buf);
	if (!paramer) {
		paramer = param("SEC_DEFAULT_SESSION_DURATION");
	}

	if (paramer) {
		sprintf(buf, "%s=\"%s\"", ATTR_SEC_SESSION_DURATION, paramer);
		delete paramer;

		ad->Insert(buf);
		dprintf ( D_SECURITY, "SECMAN: inserted '%s'\n", buf);
	} else {
		// default: 4 hours
		sprintf(buf, "%s=\"14400\"", ATTR_SEC_SESSION_DURATION);

		ad->Insert(buf);
		dprintf ( D_SECURITY, "SECMAN: inserted '%s'\n", buf);
	}

	return ad;
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

/* defunct
bool
SecMan::ReconcileSecurityDependencyOld (sec_req &a, sec_req &b) {
	sec_req tmp_base = a;
	sec_req tmp_opt  = b;

	if (tmp_opt == REQUIRED) {
		if (tmp_base == NEVER) {
			// invalid combo
			return false;
		}

		// if the option is required and the
		// base doesn't care, then the base
		// is required too.
		tmp_base = REQUIRED;
	}

	if (tmp_opt == PREFERRED) {
		if (tmp_base == NEVER) {
			// the option is preferred, but not
			// possible since the base is never.
			tmp_opt = NEVER;
		} else if (tmp_base == OPTIONAL) {
			// but if the base doesn't care, it
			// too will prefer the option
			tmp_base = PREFERRED;
		}
	}

	if (tmp_opt == OPTIONAL) {
		if (tmp_base == NEVER) {
			// the option will not work,
			// the base won't allow it.
			tmp_opt = NEVER;
		}
		// base is already optional or higher (PREF, REQ)
	}

	// if (tmp_opt == NEVER) then nobody cares

	// set the return params
	a = tmp_base;
	b = tmp_opt;
	return true;
}
*/


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

	char buf[128];

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

		char* the_method = ReconcileMethodLists( cli_methods, srv_methods );
		if (the_method) {
			sprintf (buf, "%s=\"%s\"", ATTR_SEC_AUTHENTICATION_METHODS, the_method);
			action_ad->Insert(buf);
			free( the_method );
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

		char *the_method = ReconcileMethodLists( cli_methods, srv_methods );
		if (the_method) {
			sprintf (buf, "%s=\"%s\"", ATTR_SEC_CRYPTO_METHODS, the_method);
			action_ad->Insert(buf);
			free( the_method );
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

	sprintf (buf, "%s=\"%i\"", ATTR_SEC_SESSION_DURATION,
			(cli_duration < srv_duration) ? cli_duration : srv_duration );
	action_ad->Insert(buf);


	sprintf (buf, "%s=\"YES\"", ATTR_SEC_ENACT);
	action_ad->Insert(buf);

	return action_ad;

}


bool
SecMan::startCommand( int cmd, Sock* sock, bool can_negotiate, int subCmd)
{

	// basic sanity check
	if( ! sock ) {
		dprintf ( D_ALWAYS, "startCommand() called with a NULL Sock*, failing." );
		return false;
	} else {
		dprintf ( D_SECURITY, "SECMAN: starting %i to %s on %s port %i.\n", cmd, sin_to_string(sock->endpoint()), (sock->type() == Stream::safe_sock) ? "UDP" : "TCP", sock->get_port());
	}


	// get this value handy
	bool is_tcp = (sock->type() == Stream::reli_sock);
	

	// need a temp buffer handy throughout.
	char buf[256];

	// this one is a temp for storing key ids
	char keybuf[128];

	bool have_session = false;
	bool new_session = false;


	// find out if we have a session id to use for this command
	KeyCacheEntry *enc_key = NULL;

	MyString sid;
	sprintf (keybuf, "{%s,<%i>}", sin_to_string(sock->endpoint()), cmd);
	bool found_map_ent = (command_map->lookup(keybuf, sid) == 0);
	if (found_map_ent) {
		dprintf (D_SECURITY, "SECMAN: %s uses session id %s.\n", keybuf, sid.Value());
		// we have the session id, now get the session from the cache
		have_session = session_cache->lookup(sid.Value(), enc_key);
		if (!have_session) {
			// the session is no longer in the cache... might as well
			// delete this mapping to it.  (we could delete them all, but
			// it requires iterating through the hash table)
			if (command_map->remove(keybuf) == 0) {
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
	ClassAd *auth_info = NULL;

	// if we have a private key, we will use the same security policy that
	// was decided on when the key was issued.
	// otherwise, get our security policy and work it out with the server.
	if (have_session) {
		auth_info = new ClassAd(*enc_key->policy());

		dprintf (D_SECURITY, "SECMAN: found cached session id %s for %s.\n",
				enc_key->id(), keybuf);
#ifdef SECURITY_HACK_ENABLE
		zz1printf(enc_key->key());
#endif
		auth_info->dPrint( D_SECURITY );

		new_session = false;
	} else {
		auth_info = CreateSecurityPolicyAd("CLIENT", can_negotiate);

		if ( !auth_info ) {
			// security policy was invalid.  bummer.
			dprintf (D_SECURITY, "SECMAN: security policy invalid.\n");
			return false;
		}

		dprintf (D_SECURITY, "SECMAN: no cached key for %s.\n", keybuf);

		// no sessions in udp
		if (is_tcp) {
			// for now, always open a session for tcp.
			new_session = true;
			sprintf (buf, "%s=\"YES\"", ATTR_SEC_NEW_SESSION);
			auth_info->Insert(buf);
		}
	}

	
	dprintf (D_SECURITY, "SECMAN: Security Policy:\n");
	auth_info->dPrint( D_SECURITY );


	// find out our negotiation policy.
	sec_req negotiation = sec_lookup_req( *auth_info, ATTR_SEC_NEGOTIATION );
	if (negotiation == SEC_REQ_UNDEFINED) {
		negotiation = SEC_REQ_OPTIONAL;
		dprintf(D_SECURITY, "SECMAN: missing negotiation attribute, assuming OPTIONAL.\n");
	}

	sec_feat_act negotiate = sec_req_to_feat_act(negotiation);
	if (negotiate == SEC_FEAT_ACT_NO) {
		// old way:
		// code the int and be done.  there is no easy way to try the
		// new way if the old way fails, since it will fail outside
		// the scope of this function.

		dprintf(D_SECURITY, "SECMAN: sending unauthenticated command (%i)\n", cmd);

		// just code the command and be done
		sock->encode();
		sock->code(cmd);

		// we must _NOT_ do an eom() here!  Ques?  See Todd or Zach 9/01

		// TODO ZKM HACK
		// make a note that this command was done old-style.  perhaps it is time
		// to get some decent error propagation up in here...

        // I think there is a memory leak, hence the code below. Hao
        if (auth_info) {
            delete auth_info;
        }
		return true;
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

	dprintf ( D_SECURITY, "SECMAN: negotiating security for command %i.\n", cmd);


	// special case for UDP: we can't actually authenticate via UDP.
	// so, we send a DC_AUTHENTICATE via TCP.  this will get us authenticated
	// and get us a key, which is what needs to happen.

	if ((!have_session) && (!is_tcp)) {

		dprintf ( D_SECURITY, "SECMAN: need to start a session via TCP\n");

		// we'll have to authenticate via TCP
		ReliSock tcp_auth_sock;

		// the timeout
		const int TCP_SOCK_TIMEOUT = 20;

		// we already know the address - condor uses the same TCP port as it does UDP port.
		sprintf (buf, sin_to_string(sock->endpoint()));
		if (!tcp_auth_sock.connect(buf)) {
			dprintf ( D_SECURITY, "SECMAN: couldn't connect via TCP to %s, failing...\n", buf);
			return false;
		}

		dprintf ( D_SECURITY, "SECMAN: setting timout to %i seconds.\n", TCP_SOCK_TIMEOUT);
		tcp_auth_sock.timeout(TCP_SOCK_TIMEOUT);

		bool succ = startCommand ( DC_AUTHENTICATE, &tcp_auth_sock, true, cmd);

		dprintf ( D_SECURITY, "SECMAN: sending eom() and closing TCP sock.\n");

		// close the TCP socket, the rest will be UDP.
		tcp_auth_sock.eom();
		tcp_auth_sock.close();

		if (!succ) {
			dprintf ( D_SECURITY, "SECMAN: unable to start session via TCP, failing.\n");
			return false;
		} else {
			dprintf ( D_SECURITY, "SECMAN: succesfully sent NOP via TCP!\n");
			// check if there's a key now...  what about now, is there
			// a key now?  (you see what i'm saying.... :)
			ASSERT (enc_key == NULL);

			// need to use the command map to get the sid!!!

			// sid, and keybuf are declared above
			sid = "";
			sprintf (keybuf, "{%s,<%i>}", sin_to_string(sock->endpoint()), cmd);

			bool found_map_ent = (command_map->lookup(keybuf, sid) == 0);
			if (found_map_ent) {
				dprintf (D_SECURITY, "SECMAN: %s uses session id %s.\n", keybuf, sid.Value());
				// we have the session id, now get the session from the cache
				have_session = session_cache->lookup(sid.Value(), enc_key);
				if (!have_session) {
					// the session is no longer in the cache... might as well
					// delete this mapping to it.  (we could delete them all, but
					// it requires iterating through the hash table)
					if (command_map->remove(keybuf) == 0) {
						dprintf (D_SECURITY, "SECMAN: session id %s not found, removed %s from map.\n", sid.Value(), keybuf);
					} else {
						dprintf (D_SECURITY, "SECMAN: session id %s not found and failed to removed %s from map!\n", sid.Value(), keybuf);
					}
				}
			} else {
				have_session = false;
			}


			if (have_session) {
				// i got a key...  let's use it!
				dprintf ( D_SECURITY, "SECMAN: SEC_UDP obtained key id %s!\n", enc_key->id());

				ASSERT (auth_info);

				delete auth_info;
				auth_info = new ClassAd(*enc_key->policy());

			} else {
				// there still is no key.
				//
				// this means when i sent them the NOP, no key was exchanged.  maybe
				// it means their security policy doesn't use any crypto.  we'll send
				// them this packet either way... if they don't like it, they won't
				// listen.
				dprintf ( D_SECURITY, "SECMAN: SEC_UDP has no key to use!\n");

				ASSERT (enc_key == NULL);
				ASSERT (have_session == false);
			}
		}
	}


	// fill in command
	sprintf(buf, "%s=%i", ATTR_SEC_COMMAND, cmd);
	auth_info->Insert(buf);
	dprintf ( D_SECURITY, "SECMAN: inserted '%s'\n", buf);


	if (cmd == DC_AUTHENTICATE) {
		// fill in sub-command
		sprintf(buf, "%s=%i", ATTR_SEC_AUTH_COMMAND, subCmd);
		auth_info->Insert(buf);
		dprintf ( D_SECURITY, "SECMAN: inserted '%s'\n", buf);
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


	bool retval = true;

	if (!is_tcp) {

		// there has to be a session to do anything.
		// in the future, if no authenticate or crypto is required, we 
		// could just send a sessionless UDP message with the command.

		if (!enc_key) {
			dprintf ( D_SECURITY, "SECMAN: UDP has no session, failing!\n");
			return false;
		}

		// UDP
		dprintf ( D_SECURITY, "SECMAN: UDP has session %s.\n", enc_key->id());

		sec_feat_act will_authenticate = sec_lookup_feat_act( *auth_info, ATTR_SEC_AUTHENTICATION );
		sec_feat_act will_enable_enc   = sec_lookup_feat_act( *auth_info, ATTR_SEC_ENCRYPTION );
		sec_feat_act will_enable_mac   = sec_lookup_feat_act( *auth_info, ATTR_SEC_INTEGRITY );

		if (will_authenticate == SEC_FEAT_ACT_UNDEFINED || 
			will_authenticate == SEC_FEAT_ACT_INVALID || 
			will_enable_enc == SEC_FEAT_ACT_UNDEFINED || 
			will_enable_enc == SEC_FEAT_ACT_INVALID || 
			will_enable_mac == SEC_FEAT_ACT_UNDEFINED || 
			will_enable_mac == SEC_FEAT_ACT_INVALID ) {

			// suck.

			dprintf ( D_SECURITY, "SECMAN: action attribute missing from classad\n");
			auth_info->dPrint( D_SECURITY );
			return false;
		}


		KeyInfo* ki  = NULL;
		if (enc_key->key()) {
			ki  = new KeyInfo(*(enc_key->key()));
		}
		
		if (will_enable_mac == SEC_FEAT_ACT_YES) {

			if (!ki) {
				dprintf ( D_SECURITY, "SECMAN: enable_mac has no key to use, failing...\n");
				return false;
			}

			dprintf (D_SECURITY, "SECMAN: about to enable message authenticator.\n");
#ifdef SECURITY_HACK_ENABLE
			zz1printf(ki);
#endif

			// prepare the buffer to pass in udp header
			sprintf(buf, "%s", enc_key->id());

			// stick our command socket sinful string in there
			char* dcss = global_dc_sinful();
			if (dcss) {
				strcat (buf, ",");
				strcat (buf, dcss);
			}

			sock->encode();
			sock->set_MD_mode(MD_ALWAYS_ON, ki, buf);

			dprintf ( D_SECURITY, "SECMAN: successfully enabled message authenticator!\n");
			retval = true;
		} // if (will_enable_mac)

		if (will_enable_enc == SEC_FEAT_ACT_YES) {

			if (!ki) {
				dprintf ( D_SECURITY, "SECMAN: enable_enc no key to use, failing...\n");
				return false;
			}

			dprintf (D_SECURITY, "SECMAN: about to enable encryption.\n");
#ifdef SECURITY_HACK_ENABLE
			zz1printf(ki);
#endif

			// prepare the buffer to pass in udp header
			sprintf(buf, "%s", enc_key->id());

			// stick our command socket sinful string in there
			char* dcss = global_dc_sinful();
			if (dcss) {
				strcat (buf, ",");
				strcat (buf, dcss);
			}


			sock->encode();
			sock->set_crypto_key(ki, buf);

			dprintf ( D_SECURITY, "SECMAN: successfully enabled encryption!\n");
			retval = true;
		} // if (will_enable_enc)

		if (ki) {
			delete ki;
		}
	}


	// now send the actual DC_AUTHENTICATE command
	dprintf ( D_SECURITY, "SECMAN: sending DC_AUTHENTICATE command\n");
	int authcmd = DC_AUTHENTICATE;
    sock->encode();
	if (! sock->code(authcmd)) {
		dprintf ( D_ALWAYS, "SECMAN: failed to send DC_AUTHENTICATE\n");
		return false;
	}


	// send the classad
	dprintf ( D_SECURITY, "SECMAN: sending following classad:\n");
	auth_info->dPrint ( D_SECURITY );

	if (! auth_info->put(*sock)) {
		dprintf ( D_ALWAYS, "SECMAN: failed to send auth_info\n");
		return false;
	}


	if (is_tcp) {

		if (! sock->end_of_message()) {
			dprintf ( D_ALWAYS, "SECMAN: failed to end classad message\n");
			return false;
		}

		if (sec_lookup_feat_act(*auth_info, ATTR_SEC_ENACT) != SEC_FEAT_ACT_YES) {

			// if we asked them what to do, get their response
			ASSERT (is_tcp);

			ClassAd auth_response;
			sock->decode();

			if (!auth_response.initFromStream(*sock) ||
				!sock->end_of_message() ) {

				dprintf ( D_SECURITY, "SECMAN: server did not respond, failing\n");
				return false;
			}


			dprintf ( D_SECURITY, "SECMAN: server responded with:\n");
			auth_response.dPrint( D_SECURITY );

			sec_copy_attribute( *auth_info, auth_response, ATTR_SEC_VERSION );
			sec_copy_attribute( *auth_info, auth_response, ATTR_SEC_ENACT );
			sec_copy_attribute( *auth_info, auth_response, ATTR_SEC_AUTHENTICATION_METHODS );
			sec_copy_attribute( *auth_info, auth_response, ATTR_SEC_CRYPTO_METHODS );
			sec_copy_attribute( *auth_info, auth_response, ATTR_SEC_AUTHENTICATION );
			sec_copy_attribute( *auth_info, auth_response, ATTR_SEC_ENCRYPTION );
			sec_copy_attribute( *auth_info, auth_response, ATTR_SEC_INTEGRITY );
			// sec_copy_attribute( *auth_info, auth_response, ATTR_SEC_VALID_COMMANDS );
			// sec_copy_attribute( *auth_info, auth_response, ATTR_SEC_USER );
			// sec_copy_attribute( *auth_info, auth_response, ATTR_SEC_SID );

			auth_info->Delete(ATTR_SEC_NEW_SESSION);

			sprintf(buf, "%s=\"YES\"", ATTR_SEC_USE_SESSION);
			auth_info->Insert(buf);

			sock->encode();

		}

		sec_feat_act will_authenticate = sec_lookup_feat_act( *auth_info, ATTR_SEC_AUTHENTICATION );
		sec_feat_act will_enable_enc   = sec_lookup_feat_act( *auth_info, ATTR_SEC_ENCRYPTION );
		sec_feat_act will_enable_mac   = sec_lookup_feat_act( *auth_info, ATTR_SEC_INTEGRITY );

		if (will_authenticate == SEC_FEAT_ACT_UNDEFINED || 
			will_authenticate == SEC_FEAT_ACT_INVALID || 
			will_enable_enc == SEC_FEAT_ACT_UNDEFINED || 
			will_enable_enc == SEC_FEAT_ACT_INVALID || 
			will_enable_mac == SEC_FEAT_ACT_UNDEFINED || 
			will_enable_mac == SEC_FEAT_ACT_INVALID ) {

			// missing some essential info.

			dprintf ( D_SECURITY, "SECMAN: action attribute missing from classad\n");
			auth_info->dPrint( D_SECURITY );
            
            // There might be a memory leak with auth_info, hence the code below. Hao.
            delete auth_info;
			return false;
		}

		

		// at this point, we know exactly what needs to happen.  if we asked
		// the other side, their choice is in will_authenticate.  if we
		// didn't ask, then our choice is in will_authenticate.



		// the private key, if there is one.
		KeyInfo* ki  = NULL;

		if (will_authenticate == SEC_FEAT_ACT_YES) {

			ASSERT (sock->type() == Stream::reli_sock);

			dprintf ( D_SECURITY, "SECMAN: authenticating RIGHT NOW.\n");
			char * auth_method = NULL;
			auth_info->LookupString( ATTR_SEC_AUTHENTICATION_METHODS, &auth_method );
			if (!auth_method) {
				// there's no auth method.
				dprintf ( D_SECURITY, "SECMAN: no auth method!, failing.\n");
                // Make sure auth_info is deleted
                delete auth_info;
				return false;
			}

			if (!sock->authenticate(ki, sec_char_to_auth_method(auth_method))) {
				dprintf ( D_SECURITY, "SECMAN: authenticate failed!\n");
				retval = false;
			}
            if (auth_method) {  
                free(auth_method);
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

		
		if (will_enable_mac == SEC_FEAT_ACT_YES) {

			if (!ki) {
				dprintf ( D_SECURITY, "SECMAN: enable_mac has no key to use, failing...\n");
                delete auth_info;
				return false;
			}

			dprintf (D_SECURITY, "SECMAN: about to enable message authenticator.\n");
#ifdef SECURITY_HACK_ENABLE
				zz1printf(ki);
#endif

			sock->encode();
			sock->set_MD_mode(MD_ALWAYS_ON, ki);

			dprintf ( D_SECURITY, "SECMAN: successfully enabled message authenticator!\n");
			retval = true;
		} // if (will_enable_mac)

		if (will_enable_enc == SEC_FEAT_ACT_YES) {

			if (!ki) {
				dprintf ( D_SECURITY, "SECMAN: enable_enc no key to use, failing...\n");
				return false;
			}

			dprintf (D_SECURITY, "SECMAN: about to enable encryption.\n");
#ifdef SECURITY_HACK_ENABLE
				zz1printf(ki);
#endif

			sock->encode();
			sock->set_crypto_key(ki);

			dprintf ( D_SECURITY, "SECMAN: successfully enabled encryption!\n");
			retval = true;
		} // if (will_enable_enc)

		
		if (new_session) {
			// receive a classAd containing info such as: well, nothing yet
			sock->encode();
			sock->eom();

			ClassAd post_auth_info;
			sock->decode();
			if (!post_auth_info.initFromStream(*sock) || !sock->eom()) {
				dprintf (D_ALWAYS, "SECMAN: could not receive p.a. ClassAd.\n");
				return false;
			} else {
				dprintf (D_SECURITY, "SECMAN: received post-auth classad:\n");
				post_auth_info.dPrint (D_SECURITY);
			}

			// bring in the session ID
			sec_copy_attribute( *auth_info, post_auth_info, ATTR_SEC_SID );

			// other attributes
			sec_copy_attribute( *auth_info, post_auth_info, ATTR_SEC_USER );
			sec_copy_attribute( *auth_info, post_auth_info, ATTR_SEC_VALID_COMMANDS );

			dprintf (D_SECURITY, "SECMAN: policy to be cached:\n");
			auth_info->dPrint(D_SECURITY);

			char *sid = NULL;
			auth_info->LookupString(ATTR_SEC_SID, &sid);
			if (sid == NULL) {
				delete auth_info;
				return FALSE;
			}

			char *cmd_list = NULL;
			auth_info->LookupString(ATTR_SEC_VALID_COMMANDS, &cmd_list);
			if (cmd_list == NULL) {
				delete sid;
				delete auth_info;
				return FALSE;
			}


			ASSERT (enc_key == NULL);


			// extract the session duration
			char *dur = NULL;
			auth_info->LookupString(ATTR_SEC_SESSION_DURATION, &dur);

			int expiration_time = time(0) + atoi(dur);

            if (dur) {
                free(dur);
            }

			KeyCacheEntry tmp_key( sid, sock->endpoint(), ki, auth_info, expiration_time);
			dprintf (D_SECURITY, "SECMAN: added session %s to cache for %i seconds.\n", sid, expiration_time);

			// stick the key in the cache
			session_cache->insert(tmp_key);


			// now add entrys which map all the {<sinful_string>,<command>} pairs
			// to the same key id (which is in the variable sid)

			StringList coms(cmd_list);
			char *p;

			coms.rewind();
			while (p = coms.next() ) {
				sprintf (keybuf, "{%s,<%s>}", sin_to_string(sock->endpoint()), p);

				// NOTE: HashTable returns ZERO on SUCCESS!!!
				if (command_map->insert(keybuf, sid) == 0) {
					// success
					dprintf (D_SECURITY, "SECMAN: command %s mapped to session %s.\n", keybuf, sid);
				} else {
					// perhaps there is an old entry under the same name.  we should
					// delete the old one and insert the new one.

					// NOTE: HashTable's remove returns ZERO on SUCCESS!!!
					if (command_map->remove(keybuf) == 0) {
						// now let's try to insert again (zero on success)
						if (command_map->insert(keybuf, sid) == 0) {
							dprintf (D_SECURITY, "SECMAN: command %s remapped to session %s!\n", keybuf, sid);
						} else {
							dprintf (D_SECURITY, "SECMAN: command %s NOT mapped (insert failed!)\n", keybuf, sid);
						}
					} else {
						dprintf (D_SECURITY, "SECMAN: command %s NOT mapped (remove failed!)\n", keybuf, sid);
					}
				}
			}
			
			free( sid );
            free( cmd_list );

			retval = true;

		} // if (new_session)

		// clean up
		if (ki) {
			delete ki;
		}

	} // if (is_tcp)

	// clean up
	if (auth_info) {
		delete auth_info;
	}

	if (retval) {
		dprintf ( D_SECURITY, "SECMAN: setting sock->encode()\n");
		dprintf ( D_SECURITY, "SECMAN: Success.\n");
		sock->encode();
		sock->allow_one_empty_message();
	} else {
		dprintf ( D_SECURITY, "SECMAN: startCommand failed.\n");
	}

	return retval;

}

bool SecMan :: invalidateHost(const char * sin)
{
    bool removed = true;

    if (sin) {
        KeyCacheEntry * keyEntry = NULL;
        MyString  addr(sin), id;

        if (session_cache) {
            session_cache->key_table->startIterations();
            while (session_cache->key_table->iterate(id, keyEntry)) {
                char * comp = sin_to_string(keyEntry->addr());
                if (addr == MyString(comp)) {
                    remove_commands(keyEntry);
                    session_cache->key_table->remove(id);
                }
            }
        }
    }
    else {
        // sock is NULL!
    }

    return removed;
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
                while (cmd = cmd_list.next() ) {
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
	if ( !stricmp( method, "GSS_AUTHENTICATION" ) ||
		 !stricmp( method, "X509" ) ) {
		return CAUTH_X509;
	} else if ( !stricmp( method, "NTSSPI" ) ) {
		return CAUTH_NTSSPI;
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
SecMan::getAuthBitmask ( char * methods ) {

	if (methods) {
		dprintf ( D_SECURITY, "GETAUTHBITMASK: in getAuthBitmask('%s')\n", methods);
	} else {
		dprintf ( D_SECURITY, "GETAUTHBITMASK: getAuthBitmask( NULL ) called!\n");
		return 0;
	}

	StringList server( methods );
	char *tmp = NULL;
	int retval = 0;

	server.rewind();
	while ( tmp = server.next() ) {
		retval |= sec_char_to_auth_method(tmp);
	}

	return retval;
}



char*
SecMan::ReconcileMethodLists( char * cli_methods, char * srv_methods ) {

	// algorithm:
	// step through the server's types in order.  the first
	// one the client supports will be the one.

	StringList server_methods( srv_methods );
	StringList client_methods( cli_methods );
	char *sm = NULL;
	char *cm = NULL;

	server_methods.rewind();
	while ( sm = server_methods.next() ) {
		client_methods.rewind();
		while (cm = client_methods.next() ) {
			if (stricmp(sm, cm) == 0) {
				return strdup(cm);
			}
		}
	}

	return NULL;
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
	sec_man_ref_count++;
}


SecMan::SecMan(const SecMan &copy) {
	// session_cache is static.  if there's a copy, it
	// should already have been constructed.
	ASSERT (session_cache);
	ASSERT (command_map);
	sec_man_ref_count++;
}

const SecMan & SecMan::operator=(const SecMan &copy) {
	// session_cache is static.  if there's a copy, it
	// should already have been constructed.
	ASSERT (session_cache);
	ASSERT (command_map);
	return *this;
}


SecMan::~SecMan() {
	ASSERT (session_cache);
	ASSERT (command_map);

	// don't delete session_cache - it is static!!!
	sec_man_ref_count--;
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
    while (p = list->next()) {
        invalidateKey(p);
    }
    delete list;
}

/*

			dprintf ( D_SECURITY, "SECMAN: cached key invalid (%s), removing.\n", sid);
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
			if (!sock->set_crypto_key(nullp)) {
				dprintf ( D_ALWAYS, "SECMAN: could not re-init crypto!\n");
				return false;
			}
			if (!sock->set_MD_mode(MD_OFF, nullp)) {
				dprintf ( D_ALWAYS, "SECMAN: could not re-init crypto!\n");
				return false;
			}
			if (!sock->connect(sin_to_string(sock->endpoint()), 0)) {
				dprintf ( D_ALWAYS, "SECMAN: could not reconnect to %s.\n",
						sin_to_string(sock->endpoint()));
				return false;
			}

			goto choose_action;
*/
