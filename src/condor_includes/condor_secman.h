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


	SecMan(int numbuckets = 209);  // years of careful research... HA HA HA HA
	SecMan(const SecMan &);
	~SecMan();
	const SecMan & operator=(const SecMan &);


	bool 					startCommand( int cmd, Sock* sock, bool &can_neg, CondorError* errstack, int subcmd = 0);

    //------------------------------------------
    // invalidate cache
    //------------------------------------------
	void 					invalidateAllCache();
	bool  					invalidateKey(const char * keyid);
	bool  					invalidateHost(const char * sin);
    void                    invalidateExpiredCache();

	void					send_invalidate_packet ( char* sinful, char* sessid );

	bool	FillInSecurityPolicyAd( const char *auth_level, ClassAd* ad,
									bool otherside_can_neg = true);
	ClassAd * 				ReconcileSecurityPolicyAds(ClassAd &cli_ad, ClassAd &srv_ad);
	bool 					ReconcileSecurityDependency (sec_req &a, sec_req &b);
	SecMan::sec_feat_act	ReconcileSecurityAttribute(const char* attr, ClassAd &cli_ad, ClassAd &srv_ad);
	MyString			ReconcileMethodLists( char * cli_methods, char * srv_methods );


	static  void			key_printf(int debug_levels, KeyInfo *k);

	static	int 			getAuthBitmask ( const char * methods );
	static	char* 			getSecSetting( const char* fmt, const char* authorization_level );
	static	MyString 		getDefaultAuthenticationMethods();
	static	MyString 		getDefaultCryptoMethods();
	static	SecMan::sec_req 		sec_alpha_to_sec_req(char *b);
	static	SecMan::sec_feat_act 	sec_alpha_to_sec_feat_act(char *b);
	static	SecMan::sec_req 		sec_lookup_req( ClassAd &ad, const char* pname );
	static	SecMan::sec_feat_act 	sec_lookup_feat_act( ClassAd &ad, const char* pname );

	SecMan::sec_req 		sec_param( char* pname, sec_req def = SEC_REQ_UNDEFINED );
	bool 					sec_is_negotiable (sec_req r);
	SecMan::sec_feat_act 	sec_req_to_feat_act (sec_req r);

	static	int 			sec_char_to_auth_method( char* method );

	bool 					sec_copy_attribute( ClassAd &dest, ClassAd &source, const char* attr );

 private:
    void                    remove_commands(KeyCacheEntry * keyEntry);

};


#endif
