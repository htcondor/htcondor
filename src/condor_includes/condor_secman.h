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
// #include "condor_daemon_core.h"
#include "classy_counted_ptr.h"


typedef void StartCommandCallbackType(bool success,Sock *sock,CondorError *errstack,void *misc_data);

extern char const *USE_TMP_SEC_SESSION;

typedef enum {
	StartCommandFailed = 0,
	StartCommandSucceeded = 1,
	StartCommandWouldBlock = 2,
	StartCommandInProgress = 3,
	StartCommandContinue = 4, // used internally by SecManStartCommand
}StartCommandResult;

/*
 Meaning of StartCommandWouldBlock:
  Caller wants to send a non-blocking UDP message, but callback fn is NULL,
  and we need to do a TCP key session exchange.  As a side-effect, the
  TCP exchange will be initiated, but nothing further is done.  The caller
  is expected to try again later (because by then the session key may
  be ready for use).
*/

class SecManStartCommand;

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

		// The following is indexed by session index name ( "addr,<cmd>" )
	static HashTable<MyString, classy_counted_ptr<SecManStartCommand> > *tcp_auth_in_progress;

	SecMan(int numbuckets = 209);  // years of careful research... HA HA HA HA
	SecMan(const SecMan &);
	~SecMan();
	const SecMan & operator=(const SecMan &);

		// Prepare a socket for sending a CEDAR command.  This takes
		// care of security negotiation and authentication.
		// (If raw_protocol=true, then no security negotiation or
		// anything is done.  The command is just sent directly.)
		// If callback_fn is NULL, it is caller's responsibility to
		// delete sock and errstack after this call, even if nonblocking.
		// When nonblocking with no callback_fn, this function will return
		// StartCommandWouldBlock if any blocking operations are required.
		// As a side-effect, in the case of UDP sockets, this will
		// spawn off a non-blocking attempt to create a security
		// session so that in the future, a UDP command could succeed
		// without StartCommandWouldBlock.
	StartCommandResult startCommand( int cmd, Sock* sock, bool peer_can_negotiate, bool raw_protocol, CondorError* errstack, int subcmd, StartCommandCallbackType *callback_fn, void *misc_data, bool nonblocking,char const *cmd_description,char const *sec_session_id);

		// Authenticate a socket using whatever authentication methods
		// have been configured for the specified perm level.
	static int authenticate_sock(Sock *s,DCpermission perm, CondorError* errstack);
	static int authenticate_sock(Sock *s,KeyInfo *&ki, DCpermission perm, CondorError* errstack);


    //------------------------------------------
    // invalidate cache
    //------------------------------------------
	void 					invalidateAllCache();
	bool  					invalidateKey(const char * keyid);
	void  					invalidateHost(const char * sin);
    void                    invalidateExpiredCache();
	void					invalidateByParentAndPid(const char * parent, int pid);


	bool	FillInSecurityPolicyAd( DCpermission auth_level,
									ClassAd* ad, bool peer_can_neg=true,
									bool raw_protocol=false,
									bool use_tmp_sec_session=false,
									bool force_authentication=false);
	ClassAd * 				ReconcileSecurityPolicyAds(ClassAd &cli_ad, ClassAd &srv_ad);
	bool 					ReconcileSecurityDependency (sec_req &a, sec_req &b);
	SecMan::sec_feat_act	ReconcileSecurityAttribute(const char* attr, ClassAd &cli_ad, ClassAd &srv_ad);
	MyString			ReconcileMethodLists( char * cli_methods, char * srv_methods );


	static  void			key_printf(int debug_levels, KeyInfo *k);

	static	int 			getAuthBitmask ( const char * methods );
	static void             getAuthenticationMethods( DCpermission perm, MyString *result );

	static	MyString 		getDefaultAuthenticationMethods();
	static	MyString 		getDefaultCryptoMethods();
	static	SecMan::sec_req 		sec_alpha_to_sec_req(char *b);
	static	SecMan::sec_feat_act 	sec_alpha_to_sec_feat_act(char *b);
	static	SecMan::sec_req 		sec_lookup_req( ClassAd &ad, const char* pname );
	static	SecMan::sec_feat_act 	sec_lookup_feat_act( ClassAd &ad, const char* pname );

		// For each auth level in config hierarchy, look up config value
		// and return first one found.  Optionally, set param_name to the
		// name of the config parameter that was found.  If check_subsystem
		// is specified, look first for param specific to specified
		// subsystem.
		// Caller should free the returned string.
	static char*            getSecSetting( const char* fmt, DCpermissionHierarchy const &auth_level, MyString *param_name=NULL, char const *check_subsystem=NULL );

		// for each auth level in the hierarchy, look up config value,
		// and parse it as REQUIRED, OPTIONAL, etc.
	sec_req         sec_req_param( const char* fmt, DCpermission auth_level, sec_req def );


	static int              getSecTimeout( DCpermission );
	bool 					sec_is_negotiable (sec_req r);
	SecMan::sec_feat_act 	sec_req_to_feat_act (sec_req r);

	static	int 			sec_char_to_auth_method( char* method );

	bool 					sec_copy_attribute( ClassAd &dest, ClassAd &source, const char* attr );

	bool 					sec_copy_attribute( ClassAd &dest, const char *to_attr, ClassAd &source, const char *from_attr );

	bool		set_parent_unique_id(const char *v);
	char*		my_parent_unique_id();
	char*		my_unique_id();

	void reconfig();
	static IpVerify *getIpVerify();
	static int Verify(DCpermission perm, const struct sockaddr_in *sin, const char * fqu, MyString *allow_reason=NULL, MyString *deny_reason=NULL );

		// Create a security session from scratch (without doing any
		// security negotation with the peer).  The session id and
		// key will presumably be communicated to the peer using some
		// other mechanism.
		// Setting duration=0 means the session never expires.  (In this case
		// it should be explicitly deleted with invalidateKey() when it
		// is no longer needed.)
	bool CreateNonNegotiatedSecuritySession(DCpermission auth_level, char const *sesid,char const *private_key,char const *exported_session_info,char const *peer_fqu,char const *peer_sinful, int duration);

		// Get security session info to send to our peer so that peer
		// can create pre-built security session compatible with ours.
		// This basically serializes selected attributes of the session.
	bool ExportSecSessionInfo(char const *session_id,MyString &session_info);

		// This can be used, for example, to expire a non-negotiated session
		// that was originally created with no expiration time.
	bool SetSessionExpiration(char const *session_id,time_t expiration_time);

 private:
    void                    remove_commands(KeyCacheEntry * keyEntry);

	static char*		_my_unique_id;
	static char*		_my_parent_unique_id;
	static bool			_should_check_env_for_unique_id;

	static IpVerify m_ipverify;

	friend class SecManStartCommand;

	bool LookupNonExpiredSession(char const *session_id, KeyCacheEntry *&session_key);

		// This is used internally to take the serialized representation
		// of the session attributes produced by ExportSecSessionInfo
		// and apply them while creating a session.
	bool ImportSecSessionInfo(char const *session_info,ClassAd &policy);
};

#endif
