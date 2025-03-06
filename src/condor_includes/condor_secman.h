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
#include "KeyCache.h"
#include "classy_counted_ptr.h"
#include "reli_sock.h"
#include <map>

// For AES (and newer).
#define SEC_SESSION_KEY_LENGTH_V9  32

// For BLOWFISH and 3DES
#define SEC_SESSION_KEY_LENGTH_OLD 24

typedef void StartCommandCallbackType(bool success, Sock *sock, CondorError *errstack, const std::string &trust_domain, bool should_try_token_request, void *misc_data);

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


	static const char sec_feat_act_rev[][10];
	static const char sec_req_rev[][10];

	// A pointer to the current session cache.
	static KeyCache                     *session_cache;
	// The default session cache - used when there are no tags
	static KeyCache                      m_default_session_cache;
	// Alternate session caches.
	static std::map<std::string,KeyCache> m_tagged_session_cache;
        static std::string m_tag;
	// Alternate tag methods
	static std::map<DCpermission, std::string> m_tag_methods;
	static std::string m_tag_token_owner;
	static std::map<std::string, std::string> command_map;
	static int sec_man_ref_count;
	static std::set<std::string> m_not_my_family;

	// Manage the pool password
	static std::string m_pool_password;

		// Manage the in-memory token override
	static std::string m_token;

		// The following is indexed by session index name ( "addr,<cmd>" )
	static HashTable<std::string, classy_counted_ptr<SecManStartCommand> > tcp_auth_in_progress;

	SecMan();
	SecMan(const SecMan &);
	~SecMan();
	const SecMan & operator=(const SecMan &);
	SecMan &operator=(SecMan &&) noexcept ;

		// A struct to order all the startCommand parameters below (as opposed
		// to having 10 parameters to a single function).
		//
		// Mostly a duplicate of the internal "SecManStartCommand" class, except
		// simpler and stripped down.
	struct StartCommandRequest {

		StartCommandRequest() {}
		StartCommandRequest(const StartCommandRequest &) = delete;

		int m_cmd{-1};
		Sock *m_sock{nullptr};
		bool m_raw_protocol{false};
		bool m_resume_response{true};
		CondorError *m_errstack{nullptr};
		int m_subcmd{-1};
		StartCommandCallbackType *m_callback_fn{nullptr};
		void *m_misc_data{nullptr};
		bool m_nonblocking{false};
		const char *m_cmd_description{nullptr};
		const char *m_sec_session_id{nullptr};
			// Do the start command on behalf of a specific owner;
			// empty tag is the default (`condor` for daemons...).
		std::string m_owner;
			// If m_owner is set, then we can also specify the authentication
			// methods to use for that owner.
		std::vector<std::string> m_methods;
	};

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
	StartCommandResult startCommand(const StartCommandRequest &req);

		// Authenticate a socket using whatever authentication methods
		// have been configured for the specified perm level.
	static int authenticate_sock(Sock *s,DCpermission perm, CondorError* errstack);
	static int authenticate_sock(Sock *s,KeyInfo *&ki, DCpermission perm, CondorError* errstack);

	bool getSessionPolicy(const char *sess_id, classad::ClassAd &policy);

	bool getSessionStringAttribute(const char *sess_id, const char *attr_name, std::string &attr_value);

    //------------------------------------------
    // invalidate cache
    //------------------------------------------
	void 					invalidateAllCache();
	bool  					invalidateKey(const char * keyid);
    void                    invalidateExpiredCache();

	// Setup `tag`ing mechanism - provide a way for a unique set of session caches.
	// This is useful when CEDAR needs to impersonate several logical users within the
	// same process but does not want the session cache for user A to be reused by user B.
	static void setTag(const std::string &);
        static const std::string &getTag() {return m_tag;}

	// Get and set a pool password - the SecMan controls this as it is a convenient global.
	// This is useful when a pool password needs to be managed programmatically - and not
	// read from disk.
	//
	// An empty pool password is invalid and indicates one should disable the feature.
	static void setPoolPassword(const std::string &pool) {m_pool_password = pool;}
	// An empty pool indicates this is not used.
	static const std::string &getPoolPassword() {return m_pool_password;}

		// Get and set the in-memory token.  See comments for similar approach in
		// setPoolPassword
	static void setToken(const std::string &token) {m_token = token;}
	static const std::string &getToken() {return m_token;}

	// Setup the current authentication methods for a tag; these are considered overrides
	// and are cleared when the tag is changed.
	static void setTagAuthenticationMethods(DCpermission perm, const std::vector<std::string> &methods);
	static const std::string getTagAuthenticationMethods(DCpermission perm);

	// Setup the tag credential owner name; this is considered an override and cleared when the
	// tag is changed.
	//
	// When non-empty, the authentication method should proceed as-if the daemon was running as
	// the specified owner.  While `tag` is an opaque string, this is interpreted as a username.
	static void setTagCredentialOwner(const std::string &owner) {m_tag_token_owner = owner;}
	static const std::string &getTagCredentialOwner() {return m_tag_token_owner;}

	bool	FillInSecurityPolicyAd( DCpermission auth_level,
									ClassAd* ad,
									bool raw_protocol=false,
									bool use_tmp_sec_session=false,
									bool force_authentication=false);

	bool	FillInSecurityPolicyAdFromCache( DCpermission auth_level,
									ClassAd* &ad,
									bool raw_protocol=false,
									bool use_tmp_sec_session=false,
									bool force_authentication=false);

	ClassAd * 				ReconcileSecurityPolicyAds(const ClassAd &cli_ad, const ClassAd &srv_ad);
	bool 					ReconcileSecurityDependency (sec_req &a, sec_req &b);
	SecMan::sec_feat_act	ReconcileSecurityAttribute(const char* attr, const ClassAd &cli_ad, const ClassAd &srv_ad, bool *required = nullptr, const char* attr_alt = nullptr);
	std::string			ReconcileMethodLists( const char * cli_methods, const char * srv_methods );


	static  void			key_printf(int debug_levels, KeyInfo *k);

	static	int 			getAuthBitmask ( const char * methods );
	static  std::string		getAuthenticationMethods( DCpermission perm );

	static	std::string		getDefaultCryptoMethods();
		// Given a list of crypto methods, return a list of those that are supported
		// by this version of HTCondor.  Prevents clients and servers from suggesting
		// a crypto method that isn't supported by the code.
	static  std::string		filterCryptoMethods(const std::string &);
	static	SecMan::sec_req 		sec_alpha_to_sec_req(const char *b);
	static	SecMan::sec_feat_act 	sec_alpha_to_sec_feat_act(char *b);
	static	SecMan::sec_req 		sec_lookup_req( const ClassAd &ad, const char* pname );
	static	SecMan::sec_feat_act 	sec_lookup_feat_act( const ClassAd &ad, const char* pname );

		// For each auth level in config hierarchy starting at auth_Level, look up config value
		// and return first one found.  Optionally, set param_name to the
		// name of the config parameter that was found.  If check_subsystem
		// is specified, look first for param specific to specified
		// subsystem.
		// Caller should free the returned string.
	static char*            getSecSetting( const char* fmt, DCpermission auth_level, std::string *param_name=NULL, char const *check_subsystem=NULL );
		// like getSecSetting but returns an integer value
	static bool getIntSecSetting( int &result, const char* fmt, DCpermission auth_level, std::string *param_name = NULL, char const *check_subsystem = NULL );

		// for each auth level in the hierarchy, look up config value,
		// and parse it as REQUIRED, OPTIONAL, etc.
	sec_req         sec_req_param( const char* fmt, DCpermission auth_level, sec_req def );


	static int              getSecTimeout( DCpermission );
	bool 					sec_is_negotiable (sec_req r);
	SecMan::sec_feat_act 	sec_req_to_feat_act (sec_req r);

	static	int 			sec_char_to_auth_method( const char* method );

	bool 					sec_copy_attribute( classad::ClassAd &dest, const ClassAd &source, const char* attr );

	bool 					sec_copy_attribute( ClassAd &dest, const char *to_attr, const ClassAd &source, const char *from_attr );

	bool		set_parent_unique_id(const char *v);
	char*		my_parent_unique_id();
	char*		my_unique_id();

	void reconfig();
	static IpVerify *getIpVerify();
	static int Verify(DCpermission perm, const condor_sockaddr& addr, const char * fqu, std::string &allow_reason, std::string &deny_reason );

		// Check to see if the authentication that was performed on the socket
		// (if any at all!) is sufficient to have the socket authorized at a given
		// permission level.
	bool IsAuthenticationSufficient(DCpermission perm, const Sock &sock, CondorError &err);

	static classad::References* getResumeProj() { return &m_resume_proj; };

		// Generate a EC P256 keypair for key exchange
	static std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> GenerateKeyExchange(CondorError *errstack);

		// Given the local key and a base64-encoded EC P256 peerkey, generate a key appropriate
		// for use as a symmetric key.  The resulting key is passed through HKDF.
	static bool FinishKeyExchange(std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> mykey, const char *encoded_peerkey, unsigned char *outkey, size_t outlen, CondorError *errstack);

		// Given a keypair, encode the pubkey to base64-encoded DER.
	static bool EncodePubkey(const EVP_PKEY *pkey, std::string &encoded_pkey, CondorError *errstack);

		// Create a security session from scratch (without doing any
		// security negotation with the peer).  The session id and
		// key will presumably be communicated to the peer using some
		// other mechanism.
		// Setting duration=0 means the session never expires.  (In this case
		// it should be explicitly deleted with invalidateKey() when it
		// is no longer needed.)
		//
		// If additional attributes should be copied into the session policy,
		// these can be copied into the policy parameter.
		// new_session=true: brand new session
		// new_session=false: imported session from elsewhere
	bool CreateNonNegotiatedSecuritySession(DCpermission auth_level, char const *sesid, char const *private_key,
		char const *exported_session_info, const char *auth_method, char const *peer_fqu, char const *peer_sinful, time_t duration,
		classad::ClassAd *policy, bool new_session);

		// Get security session info to send to our peer so that peer
		// can create pre-built security session compatible with ours.
		// This basically serializes selected attributes of the session.
	bool ExportSecSessionInfo(char const *session_id,std::string &session_info);

		// This can be used, for example, to expire a non-negotiated session
		// that was originally created with no expiration time.
	bool SetSessionExpiration(char const *session_id,time_t expiration_time);

		// This is used to mark a session as being in a state where it is
		// just hanging around for a short period in case some pending
		// communication is still in flight (not essential communication,
		// but stuff that would be nice to handle without generating scary
		// error messages).  It is understood that if this session has
		// the same session id as a newly requested non-negotiated security
		// session, the lingering session will simply be replaced.
	bool SetSessionLingerFlag(char const *session_id);

		// Given a list of crypto methods, return the first valid protocol name.
	static Protocol getCryptProtocolNameToEnum(char const *name);
	static const char *getCryptProtocolEnumToName(Protocol proto);

	static std::string getPreferredOldCryptProtocol(const std::string &name);

 private:
	void invalidateOneExpiredCache(KeyCache *session_cache);
	static  std::string		filterAuthenticationMethods(DCpermission perm, const std::string &input_methods);

    void                    remove_commands(KeyCacheEntry * keyEntry);

	static char*		_my_unique_id;
	static char*		_my_parent_unique_id;
	static bool			_should_check_env_for_unique_id;

	static IpVerify *m_ipverify;
	static classad::References m_resume_proj;

	friend class SecManStartCommand;

	bool LookupNonExpiredSession(char const *session_id, KeyCacheEntry *&session_key);

		// This is used internally to take the serialized representation
		// of the session attributes produced by ExportSecSessionInfo
		// and apply them while creating a session.
	bool ImportSecSessionInfo(char const *session_info,ClassAd &policy);

		// Once the authentication methods are known, fill in metadata from
		// the relevant subclass; this may allow the remote client to skip a
		// authentication which has no chance to succeed.
	void UpdateAuthenticationMetadata(ClassAd &ad);

	// Attributes for cached Security Policy Ad
	DCpermission m_cached_auth_level;
	bool m_cached_raw_protocol;
	bool m_cached_use_tmp_sec_session;
	bool m_cached_force_authentication;
	ClassAd m_cached_policy_ad;
	bool m_cached_return_value;

};

#endif
