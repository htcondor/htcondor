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

#ifndef CONDOR_SECMAN_H
#define CONDOR_SECMAN_H

#include "condor_common.h"
// #include "condor_debug.h"
// #include "condor_config.h"
// #include "condor_ver_info.h"

// #include "daemon.h"
// #include "condor_string.h"
// #include "condor_attributes.h"
// #include "condor_adtypes.h"
// #include "condor_query.h"
// #include "get_daemon_addr.h"
// #include "get_full_hostname.h"
// #include "my_hostname.h"
// #include "internet.h"
// #include "HashTable.h"
#include "KeyCache.h"
// #include "../condor_daemon_core.V6/condor_daemon_core.h"



class SecMan {

public:

	enum sec_req {
		SEC_REQ_UNDEFINED = 0, SEC_REQ_INVALID = 1,
		SEC_REQ_NEVER = 2, SEC_REQ_OPTIONAL = 3,
		SEC_REQ_PREFERRED = 4, SEC_REQ_REQUIRED = 5
	};

	enum sec_feat_act {
		SEC_FEAT_ACT_UNDEFINED = 0, SEC_FEAT_ACT_INVALID = 1,
		SEC_FEAT_ACT_FAIL = 2,
		SEC_FEAT_ACT_YES = 3, SEC_FEAT_ACT_NO = 4
	};


	static char* sec_feat_act_rev[];
	static char* sec_req_rev[];

	static KeyCache                      * session_cache;
	static HashTable<MyString, MyString> * command_map;
	static int sec_man_ref_count;


	SecMan(int numbuckets = 209);
	SecMan(const SecMan &);
	~SecMan();
	const SecMan & operator=(const SecMan &);


	bool 					startCommand( int cmd, Sock* sock, bool can_neg = true, int subcmd = 0);

    //------------------------------------------
    // invalidate cache
    //------------------------------------------
	void 					invalidateAllCache();
	bool  					invalidateKey(const char * keyid);
	bool  					invalidateHost(const char * sin);
    void                    invalidateExpiredCache();

	ClassAd *				CreateSecurityPolicyAd(const char *auth_level, bool otherside_can_neg = true);
	ClassAd * 				ReconcileSecurityPolicyAds(ClassAd &cli_ad, ClassAd &srv_ad);
	bool 					ReconcileSecurityDependency (sec_req &a, sec_req &b);
	SecMan::sec_feat_act	ReconcileSecurityAttribute(const char* attr, ClassAd &cli_ad, ClassAd &srv_ad);
	char* 					ReconcileMethodLists( char * cli_methods, char * srv_methods );

	int 					getAuthBitmask ( char * methods );


	SecMan::sec_req 		sec_alpha_to_sec_req(char *b);
	SecMan::sec_feat_act 	sec_alpha_to_sec_feat_act(char *b);
	SecMan::sec_req 		sec_lookup_req( ClassAd &ad, const char* pname );
	SecMan::sec_feat_act 	sec_lookup_feat_act( ClassAd &ad, const char* pname );

	SecMan::sec_req 		sec_param( char* pname, sec_req def = SEC_REQ_UNDEFINED );

	bool 					sec_is_negotiable (sec_req r);
	SecMan::sec_feat_act 	sec_req_to_feat_act (sec_req r);

	int 					sec_char_to_auth_method( char* method );

	bool 					sec_copy_attribute( ClassAd &dest, ClassAd &source, const char* attr );

 private:
    void                    remove_commands(KeyCacheEntry * keyEntry);

};


#endif
