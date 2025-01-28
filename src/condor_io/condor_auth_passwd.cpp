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
#include "CondorError.h"

#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>

#ifdef EVP_PKEY_HKDF
#include <openssl/kdf.h>
#endif

#include <sqlite3.h>

#include "condor_auth.h"
#include "authentication.h"
#include "CryptKey.h"
#include "store_cred.h"
#include "my_username.h"
#include "condor_config.h"
#include "classad/source.h"
#include "condor_attributes.h"
#include "condor_base64.h"
#include "condor_regex.h"
#include "directory.h"
#include "subsystem_info.h"
#include "secure_file.h"
#include "condor_secman.h"
#include "compat_classad_util.h"
#include "classad/exprTree.h"
#include "fcloser.h"

#include "condor_auth_passwd.h"

/********************
 * This file implements both POOL and IDTOKENS authentication methods.
 * Both of these authentication methods utilize symmetric encryption, as a shared
 * secret exists at both the client and the server.
 * For the POOL, the pool password is the shared secret.
 * For IDTOKENS (which is a JWT), the token signature is the shared secret; the client
 * sends over the token header and data (but NOT the signature) to the server,
 * and the server then recomputes the signature using the specified secret signing key.
 * Next the AKEP2 protocol is used to prove that both sides posses the same
 * shared secret, and to establish a session key.
 *
 * Several comments in this file refer to specific steps in the AKEP2 protocol,
 * which is as follows:
 *
 *  Authenticated Key Exchange Protocol 2 (AKEP2)
 *
 *  SUMMARY: A and B exchange 3 messages to derive a session key W.
 *
 *  RESULT: mutual entity authentication, and implicit key authentication of W.
 *
 *  1. Setup: A and B share long-term symmetric keys K, K' (these should differ but need not
 *  be independent). hK is a MAC (keyed hash function) used for entity authentication. h'K' is
 *  a pseudorandom permutation or keyed one-way function used for key derivation.
 *
 *  2. Protocol messages. Define T = (B, A, rA, rB).
 *     A -> B : rA                  (1)
 *     A <- B : T, hK(T)            (2)
 *     A -> B : (A, rB), hK(A, rB)  (3)
 *     W = h'K'(rB)
 *
 *  3. Protocol actions. Perform the following steps for each shared key required.
 *     (a) A selects and sends to B a random number rA.
 * 	   (b) B selects a random number rB and sends to A the values (B,A,rA,rB), along
 * 	       with a MAC over these quantities generated using h with key K.
 * 	   (c) Upon receiving message (2), A checks the identities are proper, that
 * 	       the rA received matches that in (1), and verifies the MAC.
 * 	   (d) A then sends to B the values (A,rB), along with a MAC thereon.
 * 	   (e) Upon receiving (3), B verifies that the MAC is correct, and that the
 * 	       received value rB matches that sent earlier.
 *     (f) Both A and B compute the session key as W = h'K'(rB).
 *
********************/

bool Condor_Auth_Passwd::m_should_search_for_tokens = true;
bool Condor_Auth_Passwd::m_tokens_avail = false;


// The GCC_DIAG_OFF() disables warnings so that we can build on our
// -Werror platforms.
//
// Older Clang compilers on macOS define __cpp_attributes but not
//   __has_cpp_attribute.
// LibreSSL advertises itself as OpenSSL 2.0.0
//   (OPENSSL_VERSION_NUMBER 0x20000000L), but doesn't have some
//   functions introduced in OpenSSL 1.1.0.
// OpenSSL 0.9.8 (used on older macOS versions) doesn't have
//    RSA_verify_PKCS1_PSS_mgf1() or RSA_padding_add_PKCS1_PSS_mgf1().
//    But since jwt calls them using the same value for the Hash and
//    mgf1Hash arguments, we can use the non-mgf1 versions of these
//    functions, which are available.

#if defined(LIBRESSL_VERSION_NUMBER)
#define OPENSSL10
#endif

#if ! defined WIN32
#if defined(__cpp_attributes) && !defined(__has_cpp_attribute)
#undef __cpp_attributes
#endif
#endif

#if OPENSSL_VERSION_NUMBER < 0x10000000L
#include <openssl/rsa.h>
static int RSA_verify_PKCS1_PSS_mgf1(RSA *rsa, const unsigned char *mHash,
        const EVP_MD *Hash, const EVP_MD *mgf1Hash,
        const unsigned char *EM, int sLen)
{ return RSA_verify_PKCS1_PSS(rsa, mHash, Hash, EM, sLen); }
static int RSA_padding_add_PKCS1_PSS_mgf1(RSA *rsa, unsigned char *EM,
        const unsigned char *mHash,
        const EVP_MD *Hash, const EVP_MD *mgf1Hash, int sLen)
{ return RSA_padding_add_PKCS1_PSS(rsa, EM, mHash, Hash, sLen); }
#endif

GCC_DIAG_OFF(float-equal)
GCC_DIAG_OFF(cast-qual)
#include "jwt-cpp/jwt.h"
GCC_DIAG_ON(float-equal)
GCC_DIAG_ON(cast-qual)

#include <stdio.h>

namespace {

bool checkToken(const std::string &line,
	const std::string &issuer,
	const std::set<std::string> &server_key_ids,
	const std::string &tokenfilename,
	std::string &username,
	std::string &token,
	std::string &signature)
{
	try {
		auto decoded_jwt = jwt::decode(line);
		if (!decoded_jwt.has_key_id()) {
			dprintf(D_SECURITY, "Decoded JWT has no key ID; skipping.\n");
			return false;
		}
		const std::string &tmp_key_id = decoded_jwt.get_key_id();
		if (!server_key_ids.empty() && (server_key_ids.find(tmp_key_id) == server_key_ids.end())) {
			dprintf(D_SECURITY, "Ignoring token as it was signed with key %s (not known to the server).\n", tmp_key_id.c_str());
			return false;
		}
		dprintf(D_SECURITY|D_VERBOSE, "JWT object was signed with server key %s (out of %zu possible keys)\n", tmp_key_id.c_str(), server_key_ids.size());
		const std::string &tmp_issuer = decoded_jwt.get_issuer();
		if (!issuer.empty() && issuer != tmp_issuer) {
			dprintf(D_SECURITY, "Ignoring token as it is from trust domain %s (server trust domain is %s).\n", tmp_issuer.c_str(), issuer.c_str());
			return false;
		}
		if (decoded_jwt.has_subject()) {
			username = decoded_jwt.get_subject();
		} else if (decoded_jwt.has_id()) {
			dprintf(D_SECURITY|D_VERBOSE, "JWT is a capability\n");
			username = decoded_jwt.get_id();
		} else {
			dprintf(D_ERROR, "JWT is missing a subject claim.\n");
			return false;
		}
		token = decoded_jwt.get_header_base64() + "." + decoded_jwt.get_payload_base64();
		signature = decoded_jwt.get_signature();
	} catch (...) {
		if (!tokenfilename.empty()) {
			dprintf(D_SECURITY, "Failed to decode JWT in keyfile '%s'; ignoring.\n", tokenfilename.c_str());
		} else {
			dprintf(D_ALWAYS, "Failed to decode provided JWT; ignoring.\n");
		}
		return false;
	}
	return true;
}

bool findToken(const std::string &tokenfilename,
	const std::string &issuer,
	const std::set<std::string> &server_key_ids,
	std::string &username,
	std::string &token,
	std::string &signature)
{
	dprintf(D_SECURITY, "IDTOKENS: Examining %s for valid tokens from issuer %s.\n", tokenfilename.c_str(), issuer.c_str());

	bool rv = false;
	char* data = nullptr;
	size_t len = 0;
	if (!read_secure_file(tokenfilename.c_str(), (void**)&data, &len, true, SECURE_FILE_VERIFY_ALL)) {
		return false;
	}
	for (auto& line: StringTokenIterator(data, len, "\n")) {
		if (line.empty() || line[0] == '#') continue;
		bool good_token = checkToken(line, issuer, server_key_ids, tokenfilename, username, token, signature);
		if (good_token) {
			rv = true;
			break;
		}
	}
	free(data);
	return rv;
}

bool
findTokens(const std::string &issuer,
        const std::set<std::string> &server_key_ids,
        const std::string &owner,
        std::string &username,
        std::string &token,
        std::string &signature)
{
	const std::string &token_contents = SecMan::getToken();
	if (!token_contents.empty() &&
		checkToken(token_contents, issuer, server_key_ids, "", username, token, signature))
	{
		return true;
	}

		// If we are supposed to retrieve a token on behalf of a user, then
		// owner will be set and we will use PRIV_USER.  In all other cases,
		// if we are a daemon we should read the token as PRIV_ROOT.
	TemporaryPrivSentry tps( !owner.empty() );
	auto subsys = get_mySubSystem();
	if (!owner.empty()) {
		if (!init_user_ids(owner.c_str(), NULL)) {
			dprintf(D_ERROR, "findTokens(%s): Failed to switch to user priv\n", owner.c_str());
			return false;
		}
		set_user_priv();
	} else if (subsys->isDaemon()) {
		set_priv(PRIV_ROOT);
	}

	// Note we reuse the exclude regexp from the configuration subsys.
	std::string dirpath;
	if (!owner.empty() || !param(dirpath, "SEC_TOKEN_DIRECTORY")) {
		std::string file_location;
			// Only utilize a "user_file" if the owner is set.
		if (!find_user_file(file_location, "tokens.d", false, !owner.empty())) {
			if (!owner.empty()) {
				dprintf(D_SECURITY|D_VERBOSE, "findTokens(%s): Unable to find any tokens for owner.\n", owner.c_str());
				return false;
			}
			param(dirpath, "SEC_TOKEN_SYSTEM_DIRECTORY");
		} else {
			dirpath = file_location;
		}
	}
	dprintf(D_SECURITY|D_VERBOSE, "Looking for tokens in directory %s for issuer %s\n", dirpath.c_str(), issuer.c_str());

	int _errcode, _erroffset;
	std::string excludeRegex;
		// We simply fail invalid regex as the config subsys should have EXCEPT'd
		// in this case.
	if (!param(excludeRegex, "LOCAL_CONFIG_DIR_EXCLUDE_REGEXP")) {
		dprintf(D_SECURITY|D_VERBOSE, "LOCAL_CONFIG_DIR_EXCLUDE_REGEXP is unset");
		return false;
	}
	Regex excludeFilesRegex;
	if (!excludeFilesRegex.compile(excludeRegex, &_errcode, &_erroffset)) {
		dprintf(D_FULLDEBUG, "LOCAL_CONFIG_DIR_EXCLUDE_REGEXP "
			"config parameter is not a valid "
			"regular expression.  Value: %s,  Error Code: %d",
			excludeRegex.c_str(), _errcode);
		return false;
	}
	if(!excludeFilesRegex.isInitialized() ) {
		dprintf(D_SECURITY|D_VERBOSE, "Failed to initialize exclude files regex.");
		return false;
	}

	Directory dir(dirpath.c_str());
	if (!dir.Rewind()) {
		dprintf(D_SECURITY, "Cannot open %s: %s (errno=%d)\n",
			dirpath.c_str(), strerror(errno), errno);
			return false;
	}

	const char *file;
	std::vector<std::string> tokens;
	std::string subsys_token_file;
	std::string subsys_agt_name = subsys->getName();
	subsys_agt_name += "_auto_generated_token";

	while ( (file = dir.Next()) ) {
		if (dir.IsDirectory()) {
			continue;
		}
		if(!excludeFilesRegex.match(file)) {
			tokens.emplace_back(dir.GetFullPath());
			if (!strcasecmp(file, subsys_agt_name.c_str())) {
				subsys_token_file = dir.GetFullPath();
			}
		} else {
			dprintf(D_SECURITY, "Ignoring token file "
				"based on LOCAL_CONFIG_DIR_EXCLUDE_REGEXP: "
				"'%s'\n", dir.GetFullPath());
		}
	}
	std::sort(tokens.begin(), tokens.end());

	if (!subsys_token_file.empty() && findToken(subsys_token_file, issuer,
		server_key_ids, username, token, signature))
	{
		//dprintf(D_SECURITY, "token %s sig %s\n", token.c_str(), signature.c_str());
		//dprintf(D_SECURITY | D_BACKTRACE, "found subsys token for user %s in file %s\n",
		//	username.c_str(), subsys_token_file.c_str());
		return true;
	}

	for (const auto &token_filename : tokens) {
		if (findToken(token_filename, issuer, server_key_ids,
			username, token, signature))
		{
			//dprintf(D_SECURITY, "token %s sig %s\n", token.c_str(), signature.c_str());
			//dprintf(D_SECURITY | D_BACKTRACE, "found token for user %s in file %s\n",
			//	username.c_str(), token_filename.c_str());
			return true;
		}
	}

	return false;
}

}

// HKDF_Extract and HKDF_Expand functions taken from OpenSSL 1.1.0 implementation.
// Licensed under the OpenSSL license.
//
// These implementations should be removed once OpenSSL 1.1.0 is available on all
// platforms.
#ifndef EVP_PKEY_HKDF
static unsigned char *HKDF_Extract(const EVP_MD *evp_md,
                                   const unsigned char *salt, size_t salt_len,
                                   const unsigned char *key, size_t key_len,
                                   unsigned char *prk, size_t *prk_len)
{
    unsigned int tmp_len;

    if (!HMAC(evp_md, salt, salt_len, key, key_len, prk, &tmp_len))
        return NULL;

    *prk_len = tmp_len;
    return prk;
}

// In OpenSSL 0.9.8, several of the HMAC functions returned void.
// This macro lets us fake a successful return code when using these
// crusty versions of OpenSSL, but use the real return code when using
// newer versions.
#if OPENSSL_VERSION_NUMBER < 0x10000000L
#define FAKE_RC(x) ((x), 1)
#else
#define FAKE_RC(x) x
#endif

static unsigned char *HKDF_Expand(const EVP_MD *evp_md,
                                  const unsigned char *prk, size_t prk_len,
                                  const unsigned char *info, size_t info_len,
                                  unsigned char *okm, size_t okm_len)
{
    HMAC_CTX hmac;

    unsigned int i;

    unsigned char prev[EVP_MAX_MD_SIZE];

    size_t done_len = 0, dig_len = EVP_MD_size(evp_md);

    size_t n = okm_len / dig_len;
    if (okm_len % dig_len)
        n++;

    if (n > 255 || okm == NULL)
        return NULL;

    HMAC_CTX_init(&hmac);

    if (!FAKE_RC(HMAC_Init_ex(&hmac, prk, prk_len, evp_md, NULL)))
        goto err;

    for (i = 1; i <= n; i++) {
        size_t copy_len;
        const unsigned char ctr = i;

        if (i > 1) {
            if (!FAKE_RC(HMAC_Init_ex(&hmac, NULL, 0, NULL, NULL)))
                goto err;

            if (!FAKE_RC(HMAC_Update(&hmac, prev, dig_len)))
                goto err;
        }

        if (!FAKE_RC(HMAC_Update(&hmac, info, info_len)))
            goto err;

        if (!FAKE_RC(HMAC_Update(&hmac, &ctr, 1)))
            goto err;

        if (!FAKE_RC(HMAC_Final(&hmac, prev, NULL)))
            goto err;

        copy_len = (done_len + dig_len > okm_len) ?
                       okm_len - done_len :
                       dig_len;

        memcpy(okm + done_len, prev, copy_len);

        done_len += copy_len;
    }

    HMAC_CTX_cleanup(&hmac);
    return okm;

 err:
    HMAC_CTX_cleanup(&hmac);
    return NULL;
}
#endif


Condor_Auth_Passwd :: Condor_Auth_Passwd(ReliSock * sock, int version)
    : Condor_Auth_Base(sock, version == 1 ? CAUTH_PASSWORD : CAUTH_TOKEN),
    m_crypto(NULL),
    m_crypto_state(NULL),
	m_client_status(0),
	m_server_status(0),
	m_ret_value(0),
	m_sk({0,0,0,0,0,0}),
    m_version(version),
    m_k(NULL),
    m_k_prime(NULL),
    m_k_len(0),
    m_k_prime_len(0),
	m_state(ServerRec1)
{
	if (m_version == 2) {
		std::string revocation_param;
		classad::ExprTree *expr = nullptr;
		if (!param(revocation_param, "SEC_TOKEN_REVOCATION_EXPR")) {
			param(revocation_param, "SEC_TOKEN_BLACKLIST_EXPR");
		}
		if (!revocation_param.empty() &&
			!ParseClassAdRvalExpr(revocation_param.c_str(), expr))
		{
			m_token_revocation_expr.reset(expr);
		}
	}
}

Condor_Auth_Passwd :: ~Condor_Auth_Passwd()
{
    if(m_crypto) delete(m_crypto);
    if(m_crypto_state) delete(m_crypto_state);
    if (m_k) free(m_k);
    if (m_k_prime) free(m_k_prime);
}

char *
Condor_Auth_Passwd::fetchTokenSharedKey(const std::string &token, int & len)
{
	len = 0;
	std::string key_id;
	try {
			// Append a '.' to the token; we only send the <header>.<payload>, while
			// a valid token is <header>.<payload>.<signature>.  The resulting string
			// does not have a valid signature, of course... but we'll generate that
			// later.
		auto decoded_jwt = jwt::decode(token + ".");
		if (!decoded_jwt.has_key_id()) {
			dprintf(D_SECURITY, "Client JWT is missing a key ID.\n");
			return nullptr;
		}
		key_id = decoded_jwt.get_key_id();
	} catch (...) {
		dprintf(D_SECURITY, "Failed to decode JWT for determining the signing key.\n");
		return nullptr;
	}
	if (key_id.empty()) {
		// At one point, we considered allowing an empty key ID (which would imply POOL);
		// we decided against this as explicit is better than implicit.
		dprintf(D_SECURITY, "Client JWT has empty key ID\n");
		return nullptr;
	}
	std::string shared_key;
	CondorError err;
	if (!getTokenSigningKey(key_id, shared_key, &err)) {
		dprintf(D_SECURITY, "Failed to fetch key named %s: %s\n", key_id.c_str(), err.getFullText().c_str());
		return nullptr;
	}
	len = (int)shared_key.size();
	char * buf = (char*)malloc(len);
	memcpy(buf, shared_key.data(), len);
	return buf;
}

char *
Condor_Auth_Passwd::fetchPoolSharedKey(int & len)
{
	len = 0;
	std::string shared_key;
	CondorError err;
	if (!getTokenSigningKey("", shared_key, &err)) {
		dprintf(D_SECURITY, "Failed to fetch POOL key: %s\n", err.getFullText().c_str());
		return nullptr;
	}
	len = (int)shared_key.size();
	char * buf = (char*)malloc(len);
	memcpy(buf, shared_key.data(), len);
	return buf;
}

char* Condor_Auth_Passwd::fetchPoolPassword(int & len)
{
	len = 0;
	// TODO: use correct domain for windows?
	auto_free_ptr pwd(getStoredPassword(POOL_PASSWORD_USERNAME, getLocalDomain()));
	if ( ! pwd) {
		dprintf(D_SECURITY, "Failed to fetch pool password\n");
		return NULL;
	}
	len = (int)strlen(pwd.ptr()) * 2;
	char * buf = (char*)malloc(len+1);
	strcpy(buf, pwd.ptr());
	strcat(buf, pwd.ptr());
	buf[len] = 0;
	return buf;
}


char *
Condor_Auth_Passwd::fetchLogin()
{
	// return malloc-ed string "user@domain" that represents who we are.
	//
	// If we are a client of the password v2 protocol, we may have a token instead.
	if (m_version == 2 && mySock_->isClient()) {
		std::string username;
		std::string token;
		std::string signature;

		auto found_token = findTokens(m_server_issuer,
					m_server_keys,
					SecMan::getTagCredentialOwner(),
					username,
					token,
					signature);

		if (!found_token && SecMan::getTagCredentialOwner().empty()) {
			// Check to see if we have access to the master key and generate a token accordingly.
			std::string issuer;
			param(issuer, "TRUST_DOMAIN");
			if (m_server_issuer == issuer && !m_server_keys.empty()) {
				CondorError err;
				std::string match_key;
				for (auto const &server_key : m_server_keys) {
					if (hasTokenSigningKey(server_key, &err)) {
						match_key = server_key;
						break;
					}
					if (!err.empty()) {
						dprintf(D_SECURITY, "Failed to read token signing key %s: %s\n", server_key.c_str(), err.getFullText().c_str());
					}
				}
				if (!match_key.empty()) {
					CondorError err;
					std::vector<std::string> authz_list;
					int lifetime = 60;
					if (mySock_->get_peer_version()->built_since_version(23, 9, 0)) {
						username = CONDOR_PASSWORD_FQU;
					} else {
						username = POOL_PASSWORD_USERNAME "@";
					}
					std::string local_token;
						// Note we don't log the token generation here as it is an ephemeral token
						// used server-side to complete the secret generation process.
					if (!Condor_Auth_Passwd::generate_token(username, match_key,
							authz_list, lifetime, false, local_token, 0, &err))
					{
						dprintf(D_SECURITY, "Failed to generate a token: %s\n",
							err.getFullText().c_str());
					}
					else {
						try {
							auto decoded_jwt = jwt::decode(local_token);
							signature = decoded_jwt.get_signature();
							token = decoded_jwt.get_header_base64() + "." + decoded_jwt.get_payload_base64();
							found_token = true;
						} catch (...) {
							dprintf(D_SECURITY, "Generated pool_password token is malformed\n");
						}
					}
				} else {
					dprintf(D_SECURITY, "No compatible security key found.\n");
				}
			}
			if (!found_token) {
				dprintf(D_SECURITY, "TOKEN: No token found.\n");
				return nullptr;
			}
		}

		size_t key_size = AUTH_PW_KEY_LEN + token.size();
		auto seed_ka = (unsigned char *)malloc(key_size);
		auto seed_kb = (unsigned char *)malloc(key_size);
		auto ka = (unsigned char *)malloc(key_strength_bytes());
		auto kb = (unsigned char *)malloc(key_strength_bytes());
		if (!seed_ka || !seed_kb || !ka || !kb) {
			dprintf(D_ALWAYS, "TOKEN: Failed to allocate memory buffers.\n");
			if (seed_ka) free(seed_ka);
			if (seed_kb) free(seed_kb);
			if (ka) free(ka);
			if (kb) free(kb);
			return nullptr;
		}
		memcpy(seed_ka + AUTH_PW_KEY_LEN, token.c_str(), token.size());
		memcpy(seed_kb + AUTH_PW_KEY_LEN, token.c_str(), token.size());

		setup_seed(seed_ka, seed_kb);
		if (hkdf(reinterpret_cast<const unsigned char *>(signature.c_str()), signature.size(),
			&seed_ka[0], key_size,
			(const unsigned char *)"master ka", 9,
			&ka[0],
			key_strength_bytes_v2()))
		{
			dprintf(D_SECURITY, "TOKEN: Failed to generate master key K\n");
			free(ka);
			free(kb);
			free(seed_ka);
			free(seed_kb);
			return nullptr;
		}
		if (hkdf(reinterpret_cast<const unsigned char *>(signature.c_str()), signature.size(),
			&seed_kb[0], key_size,
			(const unsigned char *)"master kb", 9,
			&kb[0],
			key_strength_bytes_v2()))
		{
			dprintf(D_SECURITY, "TOKEN: Failed to generate master key K'\n");
			free(ka);
			free(kb);
			free(seed_ka);
			free(seed_kb);
			return nullptr;
		}

		m_k_len = 0;
		free(m_k); m_k = nullptr;
		if (!(m_k = reinterpret_cast<unsigned char *>(malloc(key_strength_bytes_v2())))) {
			dprintf(D_SECURITY, "TOKEN: Failed to allocate new copy of K\n");
			free(ka);
			free(kb);
			free(seed_ka);
			free(seed_kb);
			return nullptr;
		}
		memcpy(m_k, &ka[0], key_strength_bytes_v2());
		m_k_len = key_strength_bytes_v2();
		m_k_prime_len = 0;
		free(m_k_prime); m_k_prime = nullptr;
		if (!(m_k_prime = reinterpret_cast<unsigned char *>(malloc(key_strength_bytes_v2())))) {
			dprintf(D_SECURITY, "TOKEN: Failed to allocate new copy of K'\n");
			free(ka);
			free(kb);
			free(seed_ka);
			free(seed_kb);
			return nullptr;
		}
		memcpy(m_k_prime, &kb[0], key_strength_bytes_v2());
		m_k_prime_len = key_strength_bytes_v2();
		m_keyfile_token = token;
		free(ka);
		free(kb);
		free(seed_ka);
		free(seed_kb);
		return strdup(username.c_str());
	}

	std::string login;
	
		// decide the login name we will try to authenticate with.
	if (mySock_->get_peer_version()->built_since_version(23, 9, 0)) {
		login = CONDOR_PASSWORD_FQU;
	} else {
		// Older peers expect 'condor_pool@...'
		formatstr(login,"%s@%s",POOL_PASSWORD_USERNAME,getLocalDomain());
	}

	return strdup( login.c_str() );
}

bool
Condor_Auth_Passwd::setupCrypto(const unsigned char* key, const int keylen)
{
		// get rid of any old crypto object
	if ( m_crypto ) delete m_crypto;
	m_crypto = NULL;
	if ( m_crypto_state ) delete m_crypto_state;
	m_crypto_state = NULL;

	if ( !key || !keylen ) {
		// cannot setup anything without a key
		return false;
	}

		// This could be 3des -- maybe we should use "best crypto" indirection.
	KeyInfo thekey(key, keylen, CONDOR_3DES, 0);
	m_crypto = new Condor_Crypt_3des();
	if ( m_crypto ) {
		m_crypto_state = new Condor_Crypto_State(CONDOR_3DES,thekey);
		// if this failed, clean up other mem
		if ( !m_crypto_state ) {
			delete m_crypto;
			m_crypto = NULL;
		}
	}

	return m_crypto ? true : false;
}

bool
Condor_Auth_Passwd::encrypt(const unsigned char* input,
					int input_len, unsigned char* & output, int& output_len)
{
	return encrypt_or_decrypt(true,input,input_len,output,output_len);
}

bool
Condor_Auth_Passwd::decrypt(const unsigned char* input, int input_len,
							unsigned char* & output, int& output_len)
{
	return encrypt_or_decrypt(false,input,input_len,output,output_len);
}

bool
Condor_Auth_Passwd::encrypt_or_decrypt(bool want_encrypt, 
									   const unsigned char* input,
									   int input_len, 
									   unsigned char* &output, 
									   int &output_len)
{
	bool result;
	
		// clean up any old buffers that perhaps were left over
	if ( output ) free(output);
	output = NULL;
	output_len = 0;
	
		// check some intput params
	if (!input || input_len < 1) {
		return false;
	}
	
		// make certain we got a crypto object
	if (!m_crypto || !m_crypto_state) {
		return false;
	}

		// do the work
	m_crypto_state->reset();
	if (want_encrypt) {
		result = m_crypto->encrypt(m_crypto_state, input,input_len,output,output_len);
	} else {
		result = m_crypto->decrypt(m_crypto_state, input,input_len,output,output_len);
	}
	
		// mark output_len as zero upon failure
	if (!result) {
		output_len = 0;
	}

		// an output_len of zero means failure; cleanup and return
	if ( output_len == 0 ) {
		if ( output ) free(output);
		output = NULL;
		return false;
	} 
	
		// if we made it here, we're golden!
	return true;
}

int 
Condor_Auth_Passwd::wrap(const char *   input,
						 int      input_len, 
						 char*&   output, 
						 int&     output_len)
{
	bool result;
	const unsigned char* in = (const unsigned char*)input;
	unsigned char* out = (unsigned char*)output;
	result = encrypt(in,input_len,out,output_len);
	
	output = (char *)out;
	
	return result ? TRUE : FALSE;
}

int 
Condor_Auth_Passwd::unwrap(const char *   input,
						   int      input_len, 
						   char*&   output, 
						   int&     output_len)
{
	bool result;
	const unsigned char* in = (const unsigned char*)input;
	unsigned char* out = (unsigned char*)output;
	
	result = decrypt(in,input_len,out,output_len);
	
	output = (char *)out;
	
	return result ? TRUE : FALSE;
}

bool
Condor_Auth_Passwd::setup_shared_keys(struct sk_buf *sk, const std::string &init_text)
{
	if ( sk->shared_key == NULL || sk->len <= 0) {
		return false;
	}

		// These were generated randomly at coding time (see
		// setup_seed).  They are used as hash keys to create the two
		// keys K and K' (referred to here as ka and kb,
		// respectively).  We derive these ka and kb by hmacing the
		// shared key with these two seed keys.
		//
    size_t key_size = AUTH_PW_KEY_LEN + (m_version == 1 ? 0 : init_text.size());
    unsigned char *seed_ka = (unsigned char *)malloc(key_size);
    unsigned char *seed_kb = (unsigned char *)malloc(key_size);
    
		// These are the keys K and K' referred to in the AKEP2
		// description.
    unsigned char *ka = (unsigned char *)malloc(key_strength_bytes());
    unsigned char *kb = (unsigned char *)malloc(key_strength_bytes());

    unsigned int ka_len = key_strength_bytes();
    unsigned int kb_len = key_strength_bytes();

		// If any are NULL, free the others...
    if( !seed_ka || !seed_kb || !ka || !kb ) {
		if(seed_ka) free(seed_ka);
		if(seed_kb) free(seed_kb);
		if(ka) free(ka);
		if(kb) free(kb);
        dprintf(D_SECURITY, "Can't authenticate: malloc error.\n");
        return false;
    }
    
		// Fill in the data for the seed keys.
    setup_seed(seed_ka, seed_kb);

		// Copy the text seed into the key.
	if (m_version == 2) {
		memcpy(seed_ka + AUTH_PW_KEY_LEN, init_text.c_str(), init_text.size());
		memcpy(seed_kb + AUTH_PW_KEY_LEN, init_text.c_str(), init_text.size());
	}

		// Generate the shared keys K and K'
	if (m_version == 1) {
		hmac((unsigned char *)sk->shared_key, sk->len,
			 seed_ka, key_size,
			 ka, &ka_len );

		hmac((unsigned char *)sk->shared_key, sk->len,
			 seed_kb, key_size,
			 kb, &kb_len );
	} else {
		// Re-derive the signing key
		std::vector<unsigned char> jwt_key; jwt_key.resize(key_strength_bytes_v2(), 0);
		if (hkdf((unsigned char *)sk->shared_key, sk->len,
			reinterpret_cast<const unsigned char *>("htcondor"), 8,
			(const unsigned char *)"master jwt", 10,
			&jwt_key[0], key_strength_bytes_v2()))
		{
			free(seed_ka);
			free(seed_kb);
			free(ka);
			free(kb);
			return false;
		}
		std::string jwt_key_str(reinterpret_cast<char *>(&jwt_key[0]), key_strength_bytes_v2());

		// Verify known keys and Sign the JWT.
		std::string token = init_text + ".";
		std::string signature;
		try {
			auto jwt = jwt::decode(token);

			int max_age = -1;
			auto now = std::chrono::system_clock::now();
			if (jwt.has_issued_at() && (max_age = param_integer("SEC_TOKEN_MAX_AGE", -1))) {
				auto iat = jwt.get_issued_at();
				auto age = std::chrono::duration_cast<std::chrono::seconds>(now - iat).count();
				if ((max_age != -1) && age > max_age) {
					dprintf(D_SECURITY, "User token age (%ld) is greater than max age (%d); rejecting\n", (long)age, max_age);
					free(ka);
					free(kb);
					free(seed_ka);
					free(seed_kb);
					return false;
				}
			}
			if (jwt.has_expires_at()) {
				auto expiry = jwt.get_expires_at();
				auto expired_for = std::chrono::duration_cast<std::chrono::seconds>(now - expiry).count();
				if (expired_for > 0) {
					dprintf(D_SECURITY, "User token has been expired for %ld seconds.\n", (long)expired_for);
					free(ka);
					free(kb);
					free(seed_ka);
					free(seed_kb);
					return false;
				}
			}
			dprintf(D_AUDIT, mySock_->getUniqueId(),
				"Remote entity presented valid token with payload %s.\n", jwt.get_payload().c_str());

			if (isTokenRevoked(jwt)) {
				dprintf(D_SECURITY, "User token with payload %s has been revoked.\n", jwt.get_payload().c_str());
				free(ka);
				free(kb);
				free(seed_ka);
				free(seed_kb);
				return false;
			}

			const std::string& algo = jwt.get_algorithm();
			std::error_code ec;
			if (algo == "HS256") {
				auto signer = jwt::algorithm::hs256{jwt_key_str};
				signature = signer.sign(init_text, ec);
			} else if (algo == "HS384") {
				auto signer = jwt::algorithm::hs384{jwt_key_str};
				signature = signer.sign(init_text, ec);
			} else if (algo == "HS512") {
				auto signer = jwt::algorithm::hs512{jwt_key_str};
				signature = signer.sign(init_text, ec);
			}
		} catch (...) {
			dprintf(D_SECURITY, "Failed to deserialize JWT.\n");
			return false;
		}

		// Derive K and K' from the JWT's signature.
		if (hkdf(reinterpret_cast<const unsigned char *>(signature.c_str()), signature.size(),
			seed_ka, key_size,
			(const unsigned char *)"master ka", 9,
			ka, AUTH_PW_KEY_STRENGTH) ||
		hkdf(reinterpret_cast<const unsigned char *>(signature.c_str()), signature.size(),
			seed_kb, key_size,
			(const unsigned char *)"master kb", 9,
			kb, AUTH_PW_KEY_STRENGTH))
		{
			free(seed_ka);
			free(seed_kb);
			free(ka);
			free(kb);
			dprintf(D_SECURITY, "Can't authenticate: HKDF error.\n");
			return false;
		}
	}

	free(seed_ka);
	free(seed_kb);
	sk->ka = ka;
    sk->kb = kb;
    sk->ka_len = ka_len;
    sk->kb_len = kb_len;

    return true;
}


bool
Condor_Auth_Passwd::isTokenRevoked(const jwt::decoded_jwt<jwt::traits::kazuho_picojson> &jwt)
{
	if (!m_token_revocation_expr) {
		return false;
	}
	classad::ClassAd ad;
	auto claims = jwt.get_payload_claims();
	for (const auto &pair : claims) {
		bool inserted = true;
		const auto &claim = pair.second;
		switch (claim.get_type()) {
		//case jwt::json::type::null:
		//	inserted = ad.InsertLiteral(pair.first, classad::Literal::MakeUndefined());
		//	break;
		case jwt::json::type::boolean:
			inserted = ad.InsertAttr(pair.first, pair.second.as_bool());
			break;
		case jwt::json::type::integer:
			inserted = ad.InsertAttr(pair.first, pair.second.as_int());
			break;
		case jwt::json::type::number:
			inserted = ad.InsertAttr(pair.first, pair.second.as_number());
			break;
		case jwt::json::type::string:
			inserted = ad.InsertAttr(pair.first, pair.second.as_string());
			break;
		// TODO: these are not currently supported
		case jwt::json::type::array: // fallthrough
		case jwt::json::type::object: // fallthrough
		default:
			break;
		}

			// If, somehow, we can't build the ad, be paranoid,
			// and assume revoked. "abundance of caution"
		if (!inserted) {
			return true;
		}
	}

	classad::EvalState state;
	state.SetScopes(&ad);
	classad::Value val;
	bool revoked = true;
		// Out of an abundance of caution, if we fail to evaluate the
		// expression or it doesn't evaluate to something boolean-like,
		// we consider the token potentially suspect.
	if (!m_token_revocation_expr->Evaluate(state, val) ||
		!val.IsBooleanValueEquiv(revoked)) {
		return true;
	}
	return revoked;
}


void
Condor_Auth_Passwd::setup_seed(unsigned char *ka, unsigned char *kb) 
{    
		// This is so ugly!
	ka[0] = 62;
    ka[1] = 74;
    ka[2] = 80;
    ka[3] = 32;
    ka[4] = 71;
    ka[5] = 213;
    ka[6] = 244;
    ka[7] = 229;
    ka[8] = 220;
    ka[9] = 124;
    ka[10] = 105;
    ka[11] = 187;
    ka[12] = 82;
    ka[13] = 16;
    ka[14] = 203;
    ka[15] = 182;
    ka[16] = 22;
    ka[17] = 122;
    ka[18] = 221;
    ka[19] = 128;
    ka[20] = 132;
    ka[21] = 247;
    ka[22] = 221;
    ka[23] = 158;
    ka[24] = 243;
    ka[25] = 173;
    ka[26] = 44;
    ka[27] = 202;
    ka[28] = 113;
    ka[29] = 210;
    ka[30] = 131;
    ka[31] = 221;
    ka[32] = 17;
    ka[33] = 74;
    ka[34] = 79;
    ka[35] = 187;
    ka[36] = 123;
    ka[37] = 30;
    ka[38] = 233;
    ka[39] = 10;
    ka[40] = 223;
    ka[41] = 168;
    ka[42] = 98;
    ka[43] = 196;
    ka[44] = 67;
    ka[45] = 4;
    ka[46] = 222;
    ka[47] = 84;
    ka[48] = 115;
    ka[49] = 163;
    ka[50] = 23;
    ka[51] = 47;
    ka[52] = 115;
    ka[53] = 92;
    ka[54] = 44;
    ka[55] = 187;
    ka[56] = 110;
    ka[57] = 119;
    ka[58] = 91;
    ka[59] = 93;
    ka[60] = 64;
    ka[61] = 211;
    ka[62] = 159;
    ka[63] = 172;
    ka[64] = 232;
    ka[65] = 115;
    ka[66] = 24;
    ka[67] = 37;
    ka[68] = 35;
    ka[69] = 249;
    ka[70] = 37;
    ka[71] = 43;
    ka[72] = 98;
    ka[73] = 59;
    ka[74] = 224;
    ka[75] = 212;
    ka[76] = 177;
    ka[77] = 103;
    ka[78] = 163;
    ka[79] = 168;
    ka[80] = 4;
    ka[81] = 12;
    ka[82] = 172;
    ka[83] = 254;
    ka[84] = 233;
    ka[85] = 238;
    ka[86] = 61;
    ka[87] = 160;
    ka[88] = 44;
    ka[89] = 10;
    ka[90] = 187;
    ka[91] = 244;
    ka[92] = 217;
    ka[93] = 216;
    ka[94] = 177;
    ka[95] = 31;
    ka[96] = 137;
    ka[97] = 0;
    ka[98] = 76;
    ka[99] = 148;
    ka[100] = 57;
    ka[101] = 35;
    ka[102] = 206;
    ka[103] = 93;
    ka[104] = 149;
    ka[105] = 8;
    ka[106] = 187;
    ka[107] = 63;
    ka[108] = 4;
    ka[109] = 188;
    ka[110] = 102;
    ka[111] = 163;
    ka[112] = 250;
    ka[113] = 32;
    ka[114] = 161;
    ka[115] = 58;
    ka[116] = 65;
    ka[117] = 108;
    ka[118] = 94;
    ka[119] = 111;
    ka[120] = 78;
    ka[121] = 13;
    ka[122] = 49;
    ka[123] = 135;
    ka[124] = 212;
    ka[125] = 95;
    ka[126] = 199;
    ka[127] = 131;
    ka[128] = 53;
    ka[129] = 197;
    ka[130] = 228;
    ka[131] = 133;
    ka[132] = 219;
    ka[133] = 44;
    ka[134] = 90;
    ka[135] = 55;
    ka[136] = 23;
    ka[137] = 151;
    ka[138] = 12;
    ka[139] = 194;
    ka[140] = 110;
    ka[141] = 123;
    ka[142] = 107;
    ka[143] = 157;
    ka[144] = 25;
    ka[145] = 101;
    ka[146] = 180;
    ka[147] = 122;
    ka[148] = 103;
    ka[149] = 223;
    ka[150] = 119;
    ka[151] = 163;
    ka[152] = 31;
    ka[153] = 34;
    ka[154] = 240;
    ka[155] = 138;
    ka[156] = 108;
    ka[157] = 11;
    ka[158] = 165;
    ka[159] = 112;
    ka[160] = 151;
    ka[161] = 162;
    ka[162] = 26;
    ka[163] = 156;
    ka[164] = 167;
    ka[165] = 198;
    ka[166] = 4;
    ka[167] = 36;
    ka[168] = 247;
    ka[169] = 39;
    ka[170] = 57;
    ka[171] = 171;
    ka[172] = 92;
    ka[173] = 185;
    ka[174] = 21;
    ka[175] = 164;
    ka[176] = 24;
    ka[177] = 91;
    ka[178] = 209;
    ka[179] = 9;
    ka[180] = 130;
    ka[181] = 142;
    ka[182] = 53;
    ka[183] = 228;
    ka[184] = 33;
    ka[185] = 8;
    ka[186] = 171;
    ka[187] = 133;
    ka[188] = 28;
    ka[189] = 8;
    ka[190] = 163;
    ka[191] = 223;
    ka[192] = 253;
    ka[193] = 224;
    ka[194] = 227;
    ka[195] = 176;
    ka[196] = 111;
    ka[197] = 61;
    ka[198] = 57;
    ka[199] = 56;
    ka[200] = 205;
    ka[201] = 173;
    ka[202] = 109;
    ka[203] = 246;
    ka[204] = 239;
    ka[205] = 154;
    ka[206] = 111;
    ka[207] = 109;
    ka[208] = 194;
    ka[209] = 203;
    ka[210] = 116;
    ka[211] = 240;
    ka[212] = 34;
    ka[213] = 133;
    ka[214] = 18;
    ka[215] = 235;
    ka[216] = 122;
    ka[217] = 61;
    ka[218] = 104;
    ka[219] = 35;
    ka[220] = 1;
    ka[221] = 6;
    ka[222] = 132;
    ka[223] = 176;
    ka[224] = 21;
    ka[225] = 193;
    ka[226] = 42;
    ka[227] = 195;
    ka[228] = 1;
    ka[229] = 76;
    ka[230] = 79;
    ka[231] = 159;
    ka[232] = 147;
    ka[233] = 142;
    ka[234] = 56;
    ka[235] = 77;
    ka[236] = 173;
    ka[237] = 30;
    ka[238] = 59;
    ka[239] = 215;
    ka[240] = 69;
    ka[241] = 255;
    ka[242] = 140;
    ka[243] = 20;
    ka[244] = 31;
    ka[245] = 215;
    ka[246] = 11;
    ka[247] = 70;
    ka[248] = 91;
    ka[249] = 168;
    ka[250] = 175;
    ka[251] = 93;
    ka[252] = 27;
    ka[253] = 152;
    ka[254] = 180;
    ka[255] = 177;
    kb[0] = 1;
    kb[1] = 0;
    kb[2] = 38;
    kb[3] = 173;
    kb[4] = 117;
    kb[5] = 223;
    kb[6] = 198;
    kb[7] = 193;
    kb[8] = 144;
    kb[9] = 165;
    kb[10] = 162;
    kb[11] = 102;
    kb[12] = 176;
    kb[13] = 209;
    kb[14] = 181;
    kb[15] = 216;
    kb[16] = 96;
    kb[17] = 247;
    kb[18] = 207;
    kb[19] = 163;
    kb[20] = 132;
    kb[21] = 103;
    kb[22] = 32;
    kb[23] = 85;
    kb[24] = 1;
    kb[25] = 205;
    kb[26] = 70;
    kb[27] = 13;
    kb[28] = 74;
    kb[29] = 136;
    kb[30] = 212;
    kb[31] = 115;
    kb[32] = 250;
    kb[33] = 82;
    kb[34] = 224;
    kb[35] = 179;
    kb[36] = 233;
    kb[37] = 20;
    kb[38] = 30;
    kb[39] = 51;
    kb[40] = 201;
    kb[41] = 125;
    kb[42] = 133;
    kb[43] = 30;
    kb[44] = 238;
    kb[45] = 45;
    kb[46] = 211;
    kb[47] = 54;
    kb[48] = 50;
    kb[49] = 243;
    kb[50] = 136;
    kb[51] = 103;
    kb[52] = 104;
    kb[53] = 239;
    kb[54] = 1;
    kb[55] = 14;
    kb[56] = 200;
    kb[57] = 223;
    kb[58] = 221;
    kb[59] = 102;
    kb[60] = 138;
    kb[61] = 222;
    kb[62] = 146;
    kb[63] = 213;
    kb[64] = 195;
    kb[65] = 67;
    kb[66] = 8;
    kb[67] = 187;
    kb[68] = 36;
    kb[69] = 56;
    kb[70] = 149;
    kb[71] = 216;
    kb[72] = 78;
    kb[73] = 215;
    kb[74] = 133;
    kb[75] = 226;
    kb[76] = 114;
    kb[77] = 104;
    kb[78] = 204;
    kb[79] = 94;
    kb[80] = 231;
    kb[81] = 86;
    kb[82] = 13;
    kb[83] = 228;
    kb[84] = 152;
    kb[85] = 40;
    kb[86] = 250;
    kb[87] = 183;
    kb[88] = 102;
    kb[89] = 194;
    kb[90] = 173;
    kb[91] = 140;
    kb[92] = 11;
    kb[93] = 44;
    kb[94] = 10;
    kb[95] = 251;
    kb[96] = 67;
    kb[97] = 92;
    kb[98] = 56;
    kb[99] = 45;
    kb[100] = 181;
    kb[101] = 210;
    kb[102] = 255;
    kb[103] = 54;
    kb[104] = 168;
    kb[105] = 174;
    kb[106] = 173;
    kb[107] = 88;
    kb[108] = 32;
    kb[109] = 71;
    kb[110] = 10;
    kb[111] = 154;
    kb[112] = 212;
    kb[113] = 93;
    kb[114] = 121;
    kb[115] = 133;
    kb[116] = 111;
    kb[117] = 94;
    kb[118] = 46;
    kb[119] = 206;
    kb[120] = 137;
    kb[121] = 75;
    kb[122] = 210;
    kb[123] = 80;
    kb[124] = 121;
    kb[125] = 41;
    kb[126] = 220;
    kb[127] = 242;
    kb[128] = 111;
    kb[129] = 125;
    kb[130] = 9;
    kb[131] = 240;
    kb[132] = 2;
    kb[133] = 143;
    kb[134] = 26;
    kb[135] = 196;
    kb[136] = 217;
    kb[137] = 113;
    kb[138] = 244;
    kb[139] = 130;
    kb[140] = 12;
    kb[141] = 95;
    kb[142] = 84;
    kb[143] = 113;
    kb[144] = 126;
    kb[145] = 157;
    kb[146] = 205;
    kb[147] = 171;
    kb[148] = 235;
    kb[149] = 33;
    kb[150] = 95;
    kb[151] = 97;
    kb[152] = 101;
    kb[153] = 93;
    kb[154] = 234;
    kb[155] = 212;
    kb[156] = 183;
    kb[157] = 44;
    kb[158] = 61;
    kb[159] = 59;
    kb[160] = 95;
    kb[161] = 102;
    kb[162] = 250;
    kb[163] = 75;
    kb[164] = 48;
    kb[165] = 184;
    kb[166] = 88;
    kb[167] = 136;
    kb[168] = 214;
    kb[169] = 47;
    kb[170] = 172;
    kb[171] = 212;
    kb[172] = 18;
    kb[173] = 156;
    kb[174] = 19;
    kb[175] = 4;
    kb[176] = 145;
    kb[177] = 159;
    kb[178] = 105;
    kb[179] = 173;
    kb[180] = 109;
    kb[181] = 140;
    kb[182] = 44;
    kb[183] = 67;
    kb[184] = 217;
    kb[185] = 206;
    kb[186] = 92;
    kb[187] = 219;
    kb[188] = 49;
    kb[189] = 212;
    kb[190] = 88;
    kb[191] = 3;
    kb[192] = 82;
    kb[193] = 199;
    kb[194] = 54;
    kb[195] = 43;
    kb[196] = 141;
    kb[197] = 128;
    kb[198] = 183;
    kb[199] = 239;
    kb[200] = 27;
    kb[201] = 186;
    kb[202] = 93;
    kb[203] = 103;
    kb[204] = 102;
    kb[205] = 96;
    kb[206] = 169;
    kb[207] = 68;
    kb[208] = 118;
    kb[209] = 69;
    kb[210] = 2;
    kb[211] = 249;
    kb[212] = 29;
    kb[213] = 29;
    kb[214] = 60;
    kb[215] = 84;
    kb[216] = 145;
    kb[217] = 12;
    kb[218] = 8;
    kb[219] = 139;
    kb[220] = 204;
    kb[221] = 183;
    kb[222] = 43;
    kb[223] = 17;
    kb[224] = 148;
    kb[225] = 138;
    kb[226] = 94;
    kb[227] = 26;
    kb[228] = 29;
    kb[229] = 205;
    kb[230] = 4;
    kb[231] = 54;
    kb[232] = 156;
    kb[233] = 23;
    kb[234] = 210;
    kb[235] = 152;
    kb[236] = 128;
    kb[237] = 76;
    kb[238] = 33;
    kb[239] = 110;
    kb[240] = 122;
    kb[241] = 38;
    kb[242] = 144;
    kb[243] = 184;
    kb[244] = 192;
    kb[245] = 233;
    kb[246] = 112;
    kb[247] = 54;
    kb[248] = 51;
    kb[249] = 0;
    kb[250] = 208;
    kb[251] = 146;
    kb[252] = 223;
    kb[253] = 36;
    kb[254] = 251;
    kb[255] = 140;
}

void
Condor_Auth_Passwd::hmac(unsigned char *sk, int sk_len,
          unsigned char *key, int key_len,
          unsigned char *result, unsigned int *result_len)
{
		// TODO: when stronger hashing functions are available, they
		// should be substituted.
    HMAC(EVP_sha1(), key, key_len, sk, sk_len, result, result_len);
}

int
Condor_Auth_Passwd::hkdf(const unsigned char *sk, size_t sk_len,
	const unsigned char *salt, size_t salt_len,
	const unsigned char *info, size_t info_len,
	unsigned char *result, size_t result_len)
{
#ifdef EVP_PKEY_HKDF
	// I had to fix two simple syntax errors in this code before
	// it would build.  I rather suspect it's never been tested.
	// See ticket #6962.
	EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, NULL);
	if (EVP_PKEY_derive_init(pctx) <= 0) {goto fail;}
	if (EVP_PKEY_CTX_set_hkdf_md(pctx, EVP_sha256()) <= 0) {goto fail;}
	if (EVP_PKEY_CTX_set1_hkdf_salt(pctx, salt, salt_len) <= 0) {goto fail;}
	if (EVP_PKEY_CTX_set1_hkdf_key(pctx, sk, sk_len) <= 0) {goto fail;}
	if (EVP_PKEY_CTX_add1_hkdf_info(pctx, info, info_len) <= 0) {goto fail;}
	if (EVP_PKEY_derive(pctx, result, &result_len) <= 0) {goto fail;}
	EVP_PKEY_CTX_free(pctx);
	return 0;

fail:
	EVP_PKEY_CTX_free(pctx);
	return -1;
#else
	// Implementation taken from OpenSSL 1.1.0; see license note at the definition
	// of extract / expand above.
	unsigned char prk[EVP_MAX_MD_SIZE];
	unsigned char *ret;
	size_t prk_len;

	if (!HKDF_Extract(EVP_sha256(), salt, salt_len, sk, sk_len, prk, &prk_len)) {
		return -1;
	}

	ret = HKDF_Expand(EVP_sha256(), prk, prk_len, info, info_len, result, result_len);
	OPENSSL_cleanse(prk, sizeof(prk));

	return ret ? 0 : -1;
#endif
}


bool
Condor_Auth_Passwd::lookup_capability(const std::string& jti, const std::string& key_id, std::string& subject, std::string& scope)
{
	std::string db_file;
	int rc = 0;
	sqlite3* db = nullptr;
	char *db_err_msg = nullptr;
	std::string stmt_str;

	if (!param(db_file, "TOKENS_DATABASE")) {
		return false;
	}

		// TODO consider adding a short busy wait
	rc = sqlite3_open_v2(db_file.c_str(), &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
	if (rc != SQLITE_OK) {
		dprintf(D_ERROR, "Failed to open tokens database file %s: %s\n", db_file.c_str(), sqlite3_errmsg(db));
		return false;
	}

	rc = sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS idtokens_minting (token_iss TEXT, token_kid TEXT, token_jti TEXT, token_iat INTEGER, token_exp INTEGER, token_sub TEXT, token_scope TEXT)", nullptr, nullptr, &db_err_msg);
	if (rc != SQLITE_OK) {
		dprintf(D_ERROR, "Failed to create tokens db table: %s\n", db_err_msg);
		sqlite3_close(db);
		free(db_err_msg);
		return false;
	}

	formatstr(stmt_str, "SELECT token_sub, token_scope FROM idtokens_minting WHERE token_kid='%s' AND token_jti='%s' ;", key_id.c_str(), jti.c_str());

	sqlite3_stmt* stmt = nullptr;
	rc = sqlite3_prepare_v2(db, stmt_str.c_str(), -1, &stmt, nullptr);
	if (rc != SQLITE_OK) {
		dprintf(D_ERROR, "sqlite3_prepare failed: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		sqlite3_close(db);
		free(db_err_msg);
		return false;
	}

	while ( (rc = sqlite3_step(stmt)) == SQLITE_ROW ) {
		subject = (const char*)sqlite3_column_text(stmt, 0);
		scope = (const char*)sqlite3_column_text(stmt, 1);
	}
	if (rc != SQLITE_DONE) {
		dprintf(D_ERROR, "sqlite3_step returned %d\n", rc);
		sqlite3_finalize(stmt);
		sqlite3_close(db);
		free(db_err_msg);
		return false;
	}

	sqlite3_finalize(stmt);

	sqlite3_close(db);
	free(db_err_msg);

	return true;
}

static bool
log_token_creation(const jwt::decoded_jwt<jwt::traits::kazuho_picojson> &jwt)
{
	int rc = 0;
	sqlite3* db = nullptr;
	std::string db_file;
	char *db_err_msg = nullptr;
	std::string stmt_str;

	std::string token_iss;
	std::string token_kid;
	std::string token_jti;
	long long token_iat = 0;
	long long token_exp = 0;
	std::string token_sub;
	std::string token_scope;

	if (!param(db_file, "TOKENS_DATABASE")) {
		return true;
	}

	dprintf(D_ALWAYS, "JEF log_token_creation()\n");
	rc = sqlite3_open_v2(db_file.c_str(), &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
	if (rc != SQLITE_OK) {
		dprintf(D_ERROR, "Failed to open database file %s: %s\n", db_file.c_str(), sqlite3_errmsg(db));
		goto done;
	}
	rc = sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS idtokens_minting (token_iss TEXT, token_kid TEXT, token_jti TEXT, token_iat INTEGER, token_exp INTEGER, token_sub TEXT, token_scope TEXT)", nullptr, nullptr, &db_err_msg);
	if (rc != SQLITE_OK) {
		dprintf(D_ERROR, "Failed to create db table: %s\n", db_err_msg);
		goto done;
	}

		// gather token data
	if (jwt.has_issuer()) {
		token_iss = jwt.get_issuer();
	}

	if (jwt.has_key_id()) {
		token_kid = jwt.get_key_id();
	}

	if (jwt.has_id()) {
		token_jti = jwt.get_id();
	}

	if (jwt.has_issued_at()) {
		auto datestamp = jwt.get_issued_at();
		token_iat = std::chrono::duration_cast<std::chrono::seconds>(datestamp.time_since_epoch()).count();
	}

	if (jwt.has_expires_at()) {
		auto datestamp = jwt.get_expires_at();
		token_exp = std::chrono::duration_cast<std::chrono::seconds>(datestamp.time_since_epoch()).count();
	}

	if (jwt.has_subject()) {
		token_sub = jwt.get_subject();
	}

	if (jwt.has_payload_claim("scope")) {
		token_scope = jwt.get_payload_claim("scope").as_string();
	}

	dprintf(D_ALWAYS, "JEF inserting iss='%s' kid='%s' jti='%s' iat=%lld exp=%lld sub='%s' scope='%s'\n",token_iss.c_str(),token_kid.c_str(), token_jti.c_str(), (long long)token_iat, (long long)token_exp, token_sub.c_str(), token_scope.c_str());
	formatstr(stmt_str, "INSERT INTO idtokens_minting (token_iss, token_kid, token_jti, token_iat, token_exp, token_sub, token_scope) VALUES ('%s', '%s', '%s', %lld, %lld, '%s', '%s');", token_iss.c_str(), token_kid.c_str(), token_jti.c_str(), token_iat, token_exp, token_sub.c_str(), token_scope.c_str());
	rc = sqlite3_exec(db, stmt_str.c_str(), nullptr, nullptr, &db_err_msg);
	if (rc != SQLITE_OK) {
		dprintf(D_ERROR, "Adding db entry failed: %s\n", db_err_msg);
		goto done;
	}

 done:
	sqlite3_close(db);
	free(db_err_msg);
	return rc == SQLITE_OK;
}

bool
Condor_Auth_Passwd::generate_token(const std::string & id,
	const std::string &key_id,
	const std::vector<std::string> &authz_list,
	long lifetime,
	bool capability,
	std::string &token,
	int ident,
	CondorError *err)
{
	std::string shared_key;
	if (!getTokenSigningKey(key_id, shared_key, err)) {
		return false;
	}

	std::vector<unsigned char> jwt_key;
	jwt_key.resize(key_strength_bytes_v2(), 0);
	if (hkdf(reinterpret_cast<const unsigned char *>(shared_key.data()), shared_key.size(),
		reinterpret_cast<const unsigned char *>("htcondor"), 8,
		(const unsigned char *)"master jwt", 10,
		&jwt_key[0],
		key_strength_bytes_v2()))
	{
		if (err) err->push("PASSWD", 1, "Failed to derive key for JWT signature");
		return false;
	}

	std::string issuer;
	if (!param(issuer, "TRUST_DOMAIN")) {
		if (err) err->push("PASSWD", 1, "Issuer namespace is not set");
		return false;
	}
	if (issuer.find_first_of(", \t") != std::string::npos) {
		if (err) err->push("PASSWD", 1, "Issuer namespace may not contain spaces or commas");
		return false;
	}

	auto iat = std::chrono::system_clock::now();
	time_t iat_unix = std::chrono::duration_cast<std::chrono::seconds>(iat.time_since_epoch()).count();
	time_t exp_unix = 0;
	std::string authz_set;
	auto_free_ptr jti(Condor_Crypt_Base::randomHexKey(16));
	if (!jti) {
		if (err) err->push("PASSWD", 1, "Failed to generate JTI");
		return false;
	}

	const char *key_id_str = key_id.empty() ? "POOL" : key_id.c_str();

	std::string jwt_key_str(reinterpret_cast<const char *>(jwt_key.data()), key_strength_bytes_v2());
	auto jwt_builder = std::move(jwt::create()
		.set_issuer(issuer)
		.set_issued_at(std::chrono::system_clock::now())
		.set_key_id(key_id_str));

	if (lifetime >= 0) {
		auto exp = std::chrono::system_clock::now() + std::chrono::seconds(lifetime);
		jwt_builder.set_expires_at(exp);
		exp_unix = std::chrono::duration_cast<std::chrono::seconds>(exp.time_since_epoch()).count();
	}
		// Set a unique JTI so we can identify the token we issued later on.
	jwt_builder.set_id(jti.ptr());

	if (!capability) {
		jwt_builder.set_subject(id);
		if (!authz_list.empty()) {
			authz_set = std::string("condor:/") + join(authz_list, " condor:/");
			jwt_builder.set_payload_claim("scope", jwt::claim(authz_set));
		}
	}

	try {
		auto jwt_token = jwt_builder.sign(jwt::algorithm::hs256(jwt_key_str));
		token = jwt_token;
		if (ident && IsDebugCategory( D_AUDIT )) {
			// Annoyingly, there's no way to get the payload from the jwt_builder object.
			auto decoded_jwt = jwt::decode(token);
			dprintf(D_AUDIT, ident, "Token Issued: %s\n", decoded_jwt.get_payload().c_str());
		}
	} catch (...) {
		return false;
	}

	//log_token_creation(decoded_jwt);
	std::string db_file;
	if (param(db_file, "TOKENS_DATABASE")) {
		int rc = 0;
		sqlite3* db = nullptr;
		char *db_err_msg = nullptr;
		std::string stmt_str;

		rc = sqlite3_open_v2(db_file.c_str(), &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
		if (rc != SQLITE_OK) {
			dprintf(D_ERROR, "Failed to open tokens database file %s: %s\n", db_file.c_str(), sqlite3_errmsg(db));
			return false;
		}
		rc = sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS idtokens_minting (token_iss TEXT, token_kid TEXT, token_jti TEXT, token_iat INTEGER, token_exp INTEGER, token_sub TEXT, token_scope TEXT)", nullptr, nullptr, &db_err_msg);
		if (rc != SQLITE_OK) {
			dprintf(D_ERROR, "Failed to create tokens db table: %s\n", db_err_msg);
			sqlite3_close(db);
			free(db_err_msg);
			return false;
		}

		formatstr(stmt_str, "INSERT INTO idtokens_minting (token_iss, token_kid, token_jti, token_iat, token_exp, token_sub, token_scope) VALUES ('%s', '%s', '%s', %lld, %lld, '%s', '%s');", issuer.c_str(), key_id_str, jti.ptr(), (long long)iat_unix, (long long)exp_unix, id.c_str(), authz_set.c_str());
		rc = sqlite3_exec(db, stmt_str.c_str(), nullptr, nullptr, &db_err_msg);
		if (rc != SQLITE_OK) {
			dprintf(D_ERROR, "Adding tokens db entry failed: %s\n", db_err_msg);
			sqlite3_close(db);
			free(db_err_msg);
			return false;
		}

		sqlite3_close(db);
		free(db_err_msg);
	} else if (capability) {
		dprintf(D_ERROR, "No tokens database, can't use capability token\n");
		return false;
	}

	return true;
}


void
Condor_Auth_Passwd::init_sk(struct sk_buf *sk) 
{       
    sk->shared_key = NULL;
	sk->len        = 0;
    sk->ka         = NULL;
	sk->ka_len     = 0;
    sk->kb         = NULL;
	sk->kb_len     = 0;
}

void
Condor_Auth_Passwd::destroy_sk(struct sk_buf *sk) 
{
    if(sk->shared_key) {
        memset(sk->shared_key, 0, sk->len);
        free(sk->shared_key);
    }
	if(sk->ka) {
		memset(sk->ka, 0, sk->ka_len);
		free(sk->ka);
		sk->ka_len = 0;
	}
	if(sk->kb) {
		memset(sk->kb, 0, sk->kb_len);
		free(sk->kb);
		sk->kb_len = 0;
	}
	init_sk(sk);
}

void
Condor_Auth_Passwd::init_t_buf(struct msg_t_buf *t) 
{
	t->a           = NULL;
	t->b           = NULL;
	t->ra          = NULL;
	t->rb          = NULL;
	t->hkt         = NULL;
	t->hkt_len     = 0;
	t->hk          = NULL;
	t->hk_len      = 0;
}
void
Condor_Auth_Passwd::destroy_t_buf(struct msg_t_buf *t) 
{
	if(t->a) {
		free(t->a);
		t->a = NULL;
	}
	if(t->b) {
		free(t->b);
		t->b = NULL;
	}
	if(t->ra) {
		free(t->ra);
		t->ra = NULL;
	}
	if(t->rb) {
		free(t->rb);
		t->rb = NULL;
	}
	if(t->hkt) {
		free(t->hkt);
		t->hkt = NULL;
	}
	if(t->hk) {
		free(t->hk);
		t->hk = NULL;
	}
	init_t_buf(t);
}

int
Condor_Auth_Passwd::authenticate(const char * /* remoteHost */, 
				 CondorError* /* errstack */,
				 bool /* non_blocking */)
{
	m_client_status = AUTH_PW_A_OK;
	m_server_status = AUTH_PW_A_OK;
	m_ret_value = -1;

		// Initialize these structures (with NULLs)
	init_t_buf(&m_t_client);
	init_t_buf(&m_t_server);
	init_sk(&m_sk);
	dprintf(D_SECURITY|D_VERBOSE, "PW.\n");

	if ( mySock_->isClient() ) {
			// ** client side authentication **

			// Get my name, password and setup the shared keys based
			// on this data.  The server will do the same when it
			// learns my name.
		dprintf(D_SECURITY|D_VERBOSE, "PW: getting name.\n");
		m_t_client.a = fetchLogin();
		if (!m_t_client.a) {
			dprintf(D_SECURITY, "PW: Failed to fetch a login name\n");
		}
		m_t_client.a_token = m_keyfile_token;

			// We complete the entire protocol even if there's an
			// error, but there's no point trying to actually do any
			// work.  This is protocol step (a).
		dprintf(D_SECURITY|D_VERBOSE, "PW: Generating ra.\n");

		if(m_client_status == AUTH_PW_A_OK) {
			m_t_client.ra = Condor_Crypt_Base::randomKey(AUTH_PW_KEY_LEN);
			if(!m_t_client.ra) {
				dprintf(D_SECURITY, "Malloc error in random key?\n");
				m_client_status = AUTH_PW_ERROR;
			}
		}

			// This differs from the protocol description in the book
			// only that the client also sends its name "A".  The
			// protocol doesn't mention how the peers know who they're
			// talking to.  This is also protocol step (a).
		dprintf(D_SECURITY|D_VERBOSE, "PW: Client sending.\n");
		m_client_status = client_send_one(m_client_status, &m_t_client);

		if(m_client_status == AUTH_PW_ABORT) {
			goto client_abort;
		}
			// This is protocol step (b).
		dprintf(D_SECURITY|D_VERBOSE, "PW: Client receiving.\n");
		m_server_status = client_receive(&m_client_status, &m_t_server);
		if(m_client_status == AUTH_PW_ABORT) {
			goto client_abort;
		}
		if ( m_server_status == AUTH_PW_ERROR ) {
			dprintf(D_SECURITY, "PW: Client received ERROR from server, propagating\n");
			m_client_status = AUTH_PW_ERROR;
		}

			// Now that we've received the server's name, we can go
			// ahead and setup the keys.
		if(m_client_status == AUTH_PW_A_OK && m_server_status == AUTH_PW_A_OK) {
			// If we have a pre-derived key, use that.
			if (m_k && m_k_prime) {
				dprintf(D_SECURITY|D_VERBOSE, "PW: Client using pre-derived key of length %zu.\n", m_k_len);
				m_sk.ka = m_k; m_k = NULL;
				m_sk.ka_len = m_k_len; m_k_len = 0;
				m_sk.kb = m_k_prime; m_k_prime = NULL;
				m_sk.kb_len = m_k_prime_len; m_k_prime_len = 0;
			} else {
				if (m_version == 2) {
					dprintf(D_SECURITY|D_VERBOSE, "PW: Client using pool shared key.\n");
					m_sk.shared_key = fetchPoolSharedKey(m_sk.len);
				} else {
					dprintf(D_SECURITY|D_VERBOSE, "PW: Client using pool password.\n");
					m_sk.shared_key = fetchPoolPassword(m_sk.len);
				}
				dprintf(D_SECURITY|D_VERBOSE, "PW: Client setting keys.\n");
				if(!setup_shared_keys(&m_sk, m_t_client.a_token)) {
					m_client_status = AUTH_PW_ERROR;
				}
			}
		}

			// This is protocol step (c).
		if(m_client_status == AUTH_PW_A_OK
		   && m_server_status == AUTH_PW_A_OK) {
			dprintf(D_SECURITY|D_VERBOSE, "PW: Client checking T.\n");
			m_client_status = client_check_t_validity(&m_t_client, &m_t_server, &m_sk);
		}

			// Are we copying the data into the m_t_client struct?
			// This is protocol step (d).  Server does (e).
		dprintf(D_SECURITY|D_VERBOSE, "PW: CLient sending two.\n");
		m_client_status = client_send_two(m_client_status, &m_t_client, &m_sk);
		if(m_client_status == AUTH_PW_ABORT) {
			goto client_abort;
		}

	client_abort:
			// This is protocol step (f).
		if(m_client_status == AUTH_PW_A_OK
		   && m_server_status == AUTH_PW_A_OK
		   && set_session_key(&m_t_client, &m_sk)) {
			dprintf(D_SECURITY|D_VERBOSE, "PW: CLient set session key.\n");
			m_ret_value = 1;
		} else {
			m_ret_value = 0;
		}
	}
	else {
		// enter state machine
		m_state = ServerRec1;
		return WouldBlock;
	}


	// code below here is client only, as server has gone into state machine.


		//m_ret_value is 1 for success, 0 for failure.
	if ( m_ret_value == 1 ) {
			// if all is good, set the remote user and domain names
		char *login, *domain;
		if ( mySock_->isClient() ) {
			login = m_t_server.b;	// server is remote to client
		} else {
			login = m_t_client.a; // client is remote to server
		}
		ASSERT(login);
		domain = strchr(login,'@');
		if (domain) {
			*domain='\0';
			domain++;
		}

		setRemoteUser(login);
		setRemoteDomain(domain);
	}

	destroy_t_buf(&m_t_client);
	destroy_t_buf(&m_t_server);
	destroy_sk(&m_sk);

		//return 1 for success, 0 for failure. Server should send
		//sucess/failure back to client so client can know what to
		//return.
	return m_ret_value;
}


int Condor_Auth_Passwd::authenticate_continue(CondorError* errstack, bool non_blocking)
{

//	int password_auth_timeout = param_integer("PASSWORD_AUTHENTICATION_TIMEOUT",-1);
//	int old_timeout=0;
//	if (password_auth_timeout>=0) {
//		old_timeout = mySock_->timeout(password_auth_timeout);
//	}

	dprintf(D_SECURITY|D_VERBOSE, "PASSWORD: entered authenticate_continue, state==%i\n", (int)m_state);

	CondorAuthPasswordRetval retval = Continue;
	while (retval == Continue)
	{
		switch (m_state)
		{
		case ServerRec1:
			retval = doServerRec1(errstack, non_blocking);
			break;
		case ServerRec2:
			retval = doServerRec2(errstack, non_blocking);
			break;
		default:
			retval = Fail;
			break;
		}
	}

//	if (password_auth_timeout>=0) {
//		mySock_->timeout(old_timeout); //put it back to what it was before
//	}

	dprintf(D_SECURITY|D_VERBOSE, "PASSWORD: leaving authenticate_continue, state==%i, return=%i\n", (int)m_state, (int)retval);
	return static_cast<int>(retval);
}

Condor_Auth_Passwd::CondorAuthPasswordRetval
Condor_Auth_Passwd::doServerRec1(CondorError* /*errstack*/, bool non_blocking) {

	if (non_blocking && !mySock_->readReady())
	{
		dprintf(D_NETWORK, "Returning to DC as read would block in PW::doServerRec1\n");
		return WouldBlock;
	}

		// ** server side authentication **

		// First we get the client's name and ra, protocol step
		// (a).
	dprintf(D_SECURITY|D_VERBOSE, "PW: Server receiving 1.\n");
	m_client_status = server_receive_one(&m_server_status, &m_t_client);
	if(m_client_status == AUTH_PW_ABORT || m_server_status == AUTH_PW_ABORT) {
		m_ret_value = 0;
		goto server_rec_1_abort;
	}

		// Then we do the key setup, and generate the random string.
	if(m_client_status == AUTH_PW_A_OK && m_server_status == AUTH_PW_A_OK) {
		m_t_server.b = fetchLogin();
		dprintf(D_SECURITY|D_VERBOSE, "PW: Server fetching password.\n");
			// In version 2, we always want to forcibly fetch the pool password.
			// However, the client ID might not actually be condor_pool@whatever;
			// hence, in this case we just use the server name twice (which is
			// mandated to be the pool username).
		if (m_t_client.a_token.empty()) {
			if (m_version == 2) {
				m_sk.shared_key = fetchPoolSharedKey(m_sk.len);
			} else {
				m_sk.shared_key = fetchPoolPassword(m_sk.len);
			}
		} else {
			m_sk.shared_key = fetchTokenSharedKey(m_t_client.a_token, m_sk.len);
		}
		// In version 1, the only thing that is ever sent in practice is 
		// condor_pool@whatever, and in that case, m_t_server.b == m_t_client.a
		// and we could simplify the line of code above to this:
		//     m_sk.shared_key = fetchPassword(m_t_server.b, m_t_client.a_token, m_t_server.b);

		if(!setup_shared_keys(&m_sk, m_t_client.a_token)) {
			m_server_status = AUTH_PW_ERROR;
		} else {
			dprintf(D_SECURITY|D_VERBOSE, "PW: Server generating rb.\n");
			//m_server_status = server_gen_rand_rb(&m_t_server);
			m_t_server.rb = Condor_Crypt_Base::randomKey(AUTH_PW_KEY_LEN);
			if(m_t_client.a) {
				m_t_server.a = strdup(m_t_client.a);
			} else {
				m_t_server.a = NULL;
			}
			m_t_server.ra = (unsigned char *)malloc(AUTH_PW_KEY_LEN);
			if(!m_t_server.ra || !m_t_server.rb) {
				dprintf(D_SECURITY, "Malloc error 1.\n");
				m_server_status = AUTH_PW_ERROR;
			} else {
				memcpy(m_t_server.ra, m_t_client.ra, AUTH_PW_KEY_LEN);
			}
		}
	} else if ( m_client_status == AUTH_PW_ERROR ) {
		dprintf(D_SECURITY, "PW: Server received ERROR from client, propagating\n");
		m_server_status = AUTH_PW_ERROR;
	}

		// Protocol message (2), step (b).
	dprintf(D_SECURITY|D_VERBOSE, "PW: Server sending.\n");
	m_server_status = server_send(m_server_status, &m_t_server, &m_sk);
	if(m_server_status == AUTH_PW_ABORT) {
		m_ret_value = 0;
		goto server_rec_1_abort;
	}

		// Protocol step (d)
	if(m_t_server.a) {
		m_t_client.a = strdup(m_t_server.a);
	} else {
		m_t_client.a = NULL;
	}
	if(m_server_status == AUTH_PW_A_OK) {
		m_t_client.rb = (unsigned char *)malloc(AUTH_PW_KEY_LEN);
		if(!m_t_client.rb) {
			dprintf(D_SECURITY, "Malloc_error.\n");
			m_server_status = AUTH_PW_ERROR;
		} else {
			memcpy(m_t_client.rb, m_t_server.rb, AUTH_PW_KEY_LEN);
		}
	} else {
		m_t_client.rb = NULL;
	}

	m_state = ServerRec2;
	return Continue;

server_rec_1_abort:
	destroy_t_buf(&m_t_client);
	destroy_t_buf(&m_t_server);
	destroy_sk(&m_sk);

		//return 0 for failure.
	return Fail;
}


Condor_Auth_Passwd::CondorAuthPasswordRetval
Condor_Auth_Passwd::doServerRec2(CondorError* /*errstack*/, bool non_blocking) {

	if (non_blocking && !mySock_->readReady())
	{
		return WouldBlock;
	}

	dprintf(D_SECURITY|D_VERBOSE, "PW: Server receiving 2.\n");
	m_client_status = server_receive_two(&m_server_status, &m_t_client);

	if(m_server_status == AUTH_PW_A_OK
	   && m_client_status == AUTH_PW_A_OK) {
			// Protocol step (e)
		dprintf(D_SECURITY|D_VERBOSE, "PW: Server checking hk.\n");
		m_server_status = server_check_hk_validity(&m_t_client, &m_t_server, &m_sk);
	}

			// protocol step (f)
	if(m_client_status == AUTH_PW_A_OK
	   && m_server_status == AUTH_PW_A_OK
	   && set_session_key(&m_t_server, &m_sk)) {
		dprintf(D_SECURITY|D_VERBOSE, "PW: Server set session key.\n");
		m_ret_value = 1;
	} else {
		m_ret_value = 0;
	}

	// the protocol has the client sending their claimed identity.  we'll log it
	// here, but it should not be trusted.
	dprintf (D_SECURITY | D_VERBOSE, "PW: client in mode %i and ID %s.\n", getMode(), m_t_client.a);

	// sanity check -- we shouldn't be in this code if not using these methods
	if ((getMode() != CAUTH_PASSWORD) && (getMode() != CAUTH_TOKEN)) {
		dprintf(D_ALWAYS, "PW: ERROR: in ServerRec2 in unknown mode %i.\n", getMode());
		m_ret_value = 0;  // signal failure
	}

	// we need to enforce that the id sent by the client matches what was
	// extracted from the token (for IDTOKENS) or is the "condor pool"
	// identity.
	//
	// the "expected_subject" string below will hold the expected value,
	// and it is set below in two different ways.
	//
	// for password, it's going to be the POOL_PASSWORD_USERNAME for older
	// clients and CONDOR_PASSWORD_FQU for newer ones.
	//
	// for token, it will be the subject extracted from the token.
	//
	// after everything is done / extracted, we will compare the "real"
	// subject with what the client initially sent and only succeed if they
	// match.
	std::string expected_subject;
	std::string identity;
	bool use_condor_pool = false;

	// for password, this is easy.
	// For older clients, we expect to see "condor_pool@<something>".
	// For newer clients, we expect to see "condor@password"
	if(m_version == 1) {
		if (mySock_->get_peer_version()->built_since_version(23, 9, 0)) {
			expected_subject = CONDOR_PASSWORD_FQU;
		} else {
			use_condor_pool = true;
			expected_subject = POOL_PASSWORD_USERNAME;
			expected_subject += "@";
			expected_subject += getLocalDomain();
		}
		identity = expected_subject;
	}

	// if the protocol was so far successful, process the token if it exists.
	if ( m_ret_value == 1 ) { //m_ret_value is 1 for success, 0 for failure.
		// we have a token, let's decode it
		if (!m_t_client.a_token.empty()) {
			std::vector<std::string> authz, scopes;
			time_t expiry = 0;
			std::string username, issuer, groups, jti;
			try {
				auto decoded_jwt = jwt::decode(m_t_client.a_token + ".");
				dprintf(D_SECURITY | D_VERBOSE, "PW: decoded JWT.\n");

				if (decoded_jwt.has_issuer()) {
					issuer = decoded_jwt.get_issuer();
				} else {
					throw 0;
				}
				if (decoded_jwt.has_id()) {
					jti = decoded_jwt.get_id();
				} else {
					throw 0;
				}
				std::string scopes_str;

				// extract the expected_subject
				if( ! decoded_jwt.has_subject() ) {
					dprintf(D_SECURITY|D_VERBOSE, "JWT has no subject, looking up in capability database\n");
					expected_subject = jti;
					std::string kid = decoded_jwt.get_key_id();
					if (!lookup_capability(jti, kid, identity, scopes_str)) {
						dprintf(D_ALWAYS, "JWT capability not found in database (jti=%s)\n", jti.c_str());
						throw 0; // skips rest of token handling
					}
				} else {
					expected_subject = decoded_jwt.get_subject();
					identity = expected_subject;
					if (decoded_jwt.has_payload_claim("scope")) {
						scopes_str = decoded_jwt.get_payload_claim("scope").as_string();
					}
				}

				// extract other useful information
				for (const auto& scope : StringTokenIterator(scopes_str)) {
					scopes.emplace_back(scope);
					if (strncmp(scope.c_str(), "condor:/", 8)) {
						continue;
					}
					authz.emplace_back(&scope[8]);
				}
				if (decoded_jwt.has_expires_at()) {
					auto token_expiry = decoded_jwt.get_expires_at();
					expiry = std::chrono::duration_cast<std::chrono::seconds>(token_expiry.time_since_epoch()).count();
				}
			} catch (...) {
				dprintf(D_SECURITY, "PW: Unable to parse final token.\n");
			}
			classad::ClassAd ad;
			if (!authz.empty()) {
				ad.InsertAttr(ATTR_SEC_LIMIT_AUTHORIZATION, join(authz, ","));
			}
			if (!scopes.empty()) {
				ad.InsertAttr(ATTR_TOKEN_SCOPES, join(scopes, ","));
			}
			if (identity.empty()) {
				// This should not be possible: the SciTokens library should fail such a token.
				dprintf(D_SECURITY, "Impossible token: token was validated with empty username.\n");
				m_ret_value = 0;
			} else {
				ad.InsertAttr(ATTR_TOKEN_SUBJECT, identity);
			}
			if (issuer.empty()) {
				// Again, not possible - can't validate without an issuer!
				dprintf(D_SECURITY, "Impossible token: token was validated with empty issuer.\n");
				m_ret_value = 0;
			} else {
				ad.InsertAttr(ATTR_TOKEN_ISSUER, issuer);
			}
			if (!jti.empty()) ad.InsertAttr(ATTR_TOKEN_ID, jti);
			if (expiry > 0) {
				ad.InsertAttr("TokenExpirationTime", expiry);
			}
			mySock_->setPolicyAd(ad);
		} else {
			// no token present.  that's expected for PASSWORD, but
			// that's a failure if it's IDTOKENS auth.
			if(getMode() == CAUTH_TOKEN) {
				// no actual token present when using IDTOKENS is a
				// failure.  set no ID and change the return code.
				dprintf(D_ALWAYS, "PW: ERROR: There was no token present!\n");
				m_ret_value = 0;
			}
		}
	}

	// if everything is still good, validate and set the authenticated information
	if(m_ret_value) {
		// for password... should the domain matter?  historically it has not,
		// since we just accepted what the client sent in the first place.  so
		// for password with older clients we only check up to the '@',
		// otherwise we check the whole thing.
		bool match = false;
		if (getMode() == CAUTH_PASSWORD && use_condor_pool) {
			match = !strncmp(m_t_client.a, expected_subject.c_str(), strlen(POOL_PASSWORD_USERNAME)+1);
		} else {
			match = !strcmp(m_t_client.a, expected_subject.c_str());
		}
		if (match) {
			char * login = strdup(identity.c_str());
			char * domain = strchr(login,'@');
			if (domain) {
				*domain='\0';
				domain++;
			}

			dprintf(D_SECURITY | D_VERBOSE, "PW: setting authenticated user (%s) and domain (%s)\n",
				login, domain ? domain : "NULL");
			setRemoteUser(login);
			setRemoteDomain(domain);
			// TODO FIXME ZKM: in the next devel release:
			// setAuthenticatedName(expected_subject.c_str());

			free(login);
		} else {
			dprintf(D_SECURITY, "PW: WARNING: client ID (%s) and expected ID (%s) do not match.  Failing.\n",
				m_t_client.a, expected_subject.c_str());
			m_ret_value = 0;
		}
	}

	destroy_t_buf(&m_t_client);
	destroy_t_buf(&m_t_server);
	destroy_sk(&m_sk);

	// an unfortunate issue here is that if we have failed, there is no
	// communication back to the client.  if we have reached this point the
	// client has already assumed success.  thus, the two sides will now be
	// out of sync as the client is waiting to exchange keys at this point.
	//
	// fortunately, the server is operating in non-blocking mode so this
	// will not interrupt operation.  the client will hang for 20 seconds
	// before timing out.
	//
	// nevertheless, we must fail for now.  perhaps down the road the
	// protocol could be expanded such that we could inform the client of
	// this status and the connection could recover to potentially try
	// other authentication methods, but now is not that time.

		//return 1 for success, 0 for failure.
	return (m_ret_value==1) ? Success : Fail;
}


bool 
Condor_Auth_Passwd::calculate_hk(struct msg_t_buf *t_buf, struct sk_buf *sk)
{
	unsigned char *buffer;
	int prefix_len, buffer_len;

	dprintf(D_SECURITY|D_VERBOSE, "In calculate_hk.\n");
	if(!t_buf->a || !t_buf->rb) {
		dprintf(D_SECURITY, "Can't hk hmac NULL.\n");
		return false;
	}

		// Create a buffer that contains the values to be hmaced.  The
		// buffer needs to be long enough to contain the client name,
		// it's trailing null, and the random string rb.
	prefix_len = strlen(t_buf->a);
	buffer_len = prefix_len+1+AUTH_PW_KEY_LEN;
	buffer = (unsigned char *)malloc(buffer_len);
	t_buf->hk = (unsigned char *)malloc(EVP_MAX_MD_SIZE);
	if(!buffer || !t_buf->hk) {
		dprintf(D_SECURITY,"Malloc error 2.\n");
		goto hk_error;
	}

	memset(buffer, 0, buffer_len);
		// Copy the data into the buffer.
	memcpy(buffer, t_buf->a, strlen(t_buf->a));
	memcpy(buffer+prefix_len+1, t_buf->rb, AUTH_PW_KEY_LEN);
	
		// Calculate the hmac using K as the key.
	hmac( buffer, buffer_len,
		  sk->ka, sk->ka_len,
		  t_buf->hk, &t_buf->hk_len);
	if(t_buf->hk_len < 1) {
		dprintf(D_SECURITY, "Error: hk hmac too short.\n");
		goto hk_error;
	}

	free(buffer);
	return true;
 hk_error:
	if(buffer) 
		free(buffer);
	if(t_buf->hk) {
		free(t_buf->hk);
		t_buf->hk = NULL;
	}
	return false;
}

int 
Condor_Auth_Passwd::client_send_two(int client_status, 
									struct msg_t_buf *t_client, 
									struct sk_buf *sk)
{
	char *send_a   = t_client->a;
	unsigned char *send_b   = t_client->rb;
	unsigned char *send_c   = NULL;
	int send_a_len = 0;
	int send_b_len = AUTH_PW_KEY_LEN;
	int send_c_len = 0;
	char nullstr[2];

	dprintf(D_SECURITY|D_VERBOSE, "In client_send_two.\n");

	nullstr[0] = 0;
	nullstr[1] = 0;

		// First we check for sanity in what we're sending.
	if(send_a == NULL) {
		client_status = AUTH_PW_ERROR;
		dprintf(D_SECURITY, "Client error: don't know my own name?\n");
	} else {
		send_a_len = strlen(send_a);
	} 
	if(send_b == NULL) {
		client_status = AUTH_PW_ERROR;
		dprintf(D_SECURITY, "Can't send null for random string.\n");
	}
	if(send_a_len == 0) {
		client_status = AUTH_PW_ERROR;
		dprintf(D_SECURITY, "Client error: I have no name?\n");
	}

		// If everything's OK so far, we calculate the hash.
	if( client_status == AUTH_PW_A_OK ) {
		if(!calculate_hk(t_client, sk)) {
			client_status = AUTH_PW_ERROR;
			dprintf(D_SECURITY, "Client can't calculate hk.\n");
		} else {
			dprintf(D_SECURITY|D_VERBOSE, "Client calculated hk.\n");
		}
	}

		// If there's an error, we don't send anything.
	if(client_status != AUTH_PW_A_OK) {
		send_a = nullstr;
		send_b = (unsigned char *)nullstr;
		send_c = (unsigned char *)nullstr;
		send_a_len = 0;
		send_b_len = 0;
		send_c_len = 0;
	} else {
		send_c = t_client->hk;
		send_c_len = t_client->hk_len;
	}

	dprintf(D_SECURITY|D_VERBOSE, "Client sending: %d(%s) %d %d\n",
			send_a_len, send_a, send_b_len, send_c_len);

	mySock_->encode();
	if( !mySock_->code(client_status)
		|| !mySock_->code(send_a_len)
		|| !mySock_->code(send_a) 
		|| !mySock_->code(send_b_len)
		|| !(send_b_len == mySock_->put_bytes(send_b, send_b_len))
		|| !mySock_->code(send_c_len)
		|| !(send_c_len == mySock_->put_bytes(send_c, send_c_len))
		|| !mySock_->end_of_message()) {
		dprintf(D_SECURITY, "Error sending to server (second message).  "
				"Aborting...\n");
		client_status = AUTH_PW_ABORT;
	}
	dprintf(D_SECURITY|D_VERBOSE, "Sent ok.\n");
	return client_status;
}

int Condor_Auth_Passwd::client_check_t_validity(msg_t_buf *t_client, 
												msg_t_buf *t_server, 
												sk_buf *sk) 
{

		// We received t_server from the server, want to compare it
		// against t_client.

		// First see if everything's there.
	if(!t_client->a || !t_client->ra
	   || (strlen(t_client->a) == 0) 
	   || !t_server->a || !t_server->b 
	   || (strlen(t_server->a) == 0) || (strlen(t_server->b) == 0)
	   || !t_server->ra || !t_server->rb 
	   || !t_server->hkt || (t_server->hkt_len == 0)) {
		dprintf(D_SECURITY, "Error: unexpected null.\n");
		return AUTH_PW_ERROR;
	}
		
		// Fill in the information supplied by the server.
	if(t_server->b) {
		t_client->b = strdup(t_server->b);
	} else {
		t_client->b = NULL;
	}
	if((t_client->rb = (unsigned char *)malloc(AUTH_PW_KEY_LEN))) {
		memcpy(t_client->rb, t_server->rb, AUTH_PW_KEY_LEN);
	} else {
		dprintf(D_SECURITY, "Malloc error 3.\n");
		return AUTH_PW_ABORT;
	}

		// Check that its data sent was the same as ours.
	if(strcmp(t_client->a, t_server->a)) {
		dprintf(D_SECURITY, "Error: server message T contains "
				"wrong client name.\n");
		return AUTH_PW_ERROR;
	}
	if(memcmp(t_client->ra, t_server->ra, AUTH_PW_KEY_LEN)) {
		dprintf(D_SECURITY, "Error: server message T contains "
				"different random string than what I sent.\n");
		return AUTH_PW_ERROR;
	}

		// Calculate the hash on our data.
	if(!calculate_hkt(t_client, sk)) {
		dprintf(D_SECURITY, "Error calculating hmac.\n");
		return AUTH_PW_ERROR;
	}

		// Compare the client's hash with that supplied by the server.
	if(memcmp(t_client->hkt, t_server->hkt, t_client->hkt_len)) {
		dprintf(D_SECURITY, "Hash supplied by server doesn't match "
				"that calculated by the client.\n");
		return AUTH_PW_ERROR;
	}

	return AUTH_PW_A_OK;
}

int Condor_Auth_Passwd::server_check_hk_validity(struct msg_t_buf *t_client, 
												 struct msg_t_buf *t_server, 
												 struct sk_buf *sk) 
{
		// Check for empty input.  This shouldn't happen, but...
	if(!t_client->a || !t_client->rb || !t_client->hk || !t_client->hk_len) {
		dprintf(D_SECURITY, "Error: unexpected NULL.\n");
		return AUTH_PW_ERROR;
	}

		// Check that everything's the same.
	if(strcmp(t_client->a, t_server->a)) {
		dprintf(D_SECURITY, 
				"Error: client message contains wrong server name.\n");
		return AUTH_PW_ERROR;
	}
	if(memcmp(t_client->rb, t_server->rb, AUTH_PW_KEY_LEN)) {
		dprintf(D_SECURITY, 
				"Error: client message contains wrong random rb.\n");
		return AUTH_PW_ERROR;
	}

		// Calculate the hash.
	if(!calculate_hk(t_server, sk)) {
		dprintf(D_SECURITY, "Error calculating hmac.\n");
		return AUTH_PW_ERROR;
	}
		// See that the hash is the same.
	if(t_server->hk_len != t_client->hk_len 
	   || memcmp(t_client->hk, t_server->hk, t_server->hk_len)) {
		dprintf(D_SECURITY, "Hash supplied by client doesn't match "
				"that calculated by the server.\n");
		return AUTH_PW_ERROR;
	}
	return AUTH_PW_A_OK;
}

int Condor_Auth_Passwd::server_receive_two(int *server_status, 
										   struct msg_t_buf *t_client) 
{
	int client_status = AUTH_PW_ERROR;
	char *a = NULL;
	int a_len = 0;
	unsigned char *rb = (unsigned char *)malloc(AUTH_PW_KEY_LEN);
	int rb_len = 0;
	unsigned char *hk = (unsigned char *)malloc(EVP_MAX_MD_SIZE);
	int hk_len = 0;
	
	if(!rb || !hk) {
		dprintf(D_SECURITY, "Malloc error 4.\n");
		*server_status = AUTH_PW_ABORT;
		client_status = AUTH_PW_ABORT;
		goto server_receive_two_abort;
	}
	memset(rb, 0, AUTH_PW_KEY_LEN);
	memset(hk, 0, EVP_MAX_MD_SIZE);

	if(*server_status == AUTH_PW_A_OK && (!t_client->a || !t_client->rb)) {
		dprintf(D_SECURITY, "Can't compare to null.\n");
		*server_status = client_status = AUTH_PW_ABORT;
		goto server_receive_two_abort;
	}
		// Get the data.
	mySock_->decode();
	if( !mySock_->code(client_status)
		|| !mySock_->code(a_len)
		|| !mySock_->code(a)
		|| !mySock_->code(rb_len)
		|| !(rb_len <= AUTH_PW_KEY_LEN)
		|| !(rb_len == mySock_->get_bytes(rb, rb_len))
		|| !mySock_->code(hk_len)
		|| !(hk_len <= EVP_MAX_MD_SIZE)
		|| !(hk_len == mySock_->get_bytes(hk, hk_len))
		|| !mySock_->end_of_message()) {
		dprintf(D_SECURITY, "Error communicating with client.  Aborting...\n");
		*server_status = AUTH_PW_ABORT;	
		client_status = AUTH_PW_ABORT;
		goto server_receive_two_abort;
	}

		// See if it's sane.
	if(client_status == AUTH_PW_A_OK && *server_status == AUTH_PW_A_OK) {
		if(rb_len == AUTH_PW_KEY_LEN 
		   && a && strlen(a) == strlen(t_client->a)
		   && a_len == (int)strlen(a)
		   && !strcmp(a, t_client->a) 
		   && !memcmp(rb, t_client->rb, AUTH_PW_KEY_LEN)) {
			
			t_client->hk = hk;
			t_client->hk_len = hk_len;
			free(a);
			if(rb) free(rb);
			return client_status;
		} else {
			dprintf(D_SECURITY, "Received inconsistent data.\n");
			*server_status = AUTH_PW_ERROR;
		}
	} else {
		dprintf(D_SECURITY, "Error from client.\n");
	}

	server_receive_two_abort:
		// Since we didn't move the data to the struct
	if(a) free(a);
	if(rb) free(rb);
	if(hk) free(hk);
	
	return client_status;
}

bool Condor_Auth_Passwd::calculate_hkt(msg_t_buf *t_buf, sk_buf *sk) 
{
	unsigned char *buffer;
	int prefix_len, buffer_len;

	if(t_buf->a && t_buf->b)
		dprintf(D_SECURITY|D_VERBOSE, "Calculating hkt '%s' (%lu), '%s' (%lu).\n",
			t_buf->a, (unsigned long)strlen(t_buf->a),
			t_buf->b, (unsigned long)strlen(t_buf->b));
		// Assemble the buffer to be hmac'd by concatentating T in
		// buffer.  Then call hmac with ka.
	if(!t_buf->a || !t_buf->b || !t_buf->ra || !t_buf->rb) {
		dprintf(D_SECURITY, "Can't hmac NULL.\n");
		return false;
	}

		// The length of this buffer is the length of a, space, b, one
		// null, and two times the key len (ra+rb).
	prefix_len = strlen(t_buf->a) + strlen(t_buf->b) + 1;
	buffer_len = prefix_len + 1 + 2 * AUTH_PW_KEY_LEN;
	buffer = (unsigned char *)malloc(buffer_len);
	t_buf->hkt = (unsigned char *)malloc(EVP_MAX_MD_SIZE);
	if(!buffer || !t_buf->hkt) {
		dprintf(D_SECURITY, "Malloc error 5.\n");
		goto hkt_error;
	}
	if(prefix_len != sprintf((char *)buffer, "%s %s", t_buf->a, t_buf->b)) {
		dprintf(D_SECURITY, "Error copying memory.\n");
		goto hkt_error;
	}

	memcpy(buffer+prefix_len+1, t_buf->ra, AUTH_PW_KEY_LEN);
	memcpy(buffer+prefix_len+1+AUTH_PW_KEY_LEN, t_buf->rb, AUTH_PW_KEY_LEN);

		// Calculate the hmac.
	hmac( buffer, buffer_len, 
		  sk->ka, sk->ka_len,
		  t_buf->hkt, &t_buf->hkt_len);
	if(t_buf->hkt_len < 1) {  // Maybe should be larger!
		dprintf(D_SECURITY, "Error: hmac returned zero length.\n");
		goto hkt_error;
	}
	free(buffer);
	// t_buf->hkt, allocated above, must be freed by caller if this function
	// returns true.
	return true;
 hkt_error:
	if(buffer)
		free(buffer);
	if(t_buf->hkt) {
		// make it very clear that t_buf->hkt is not valid.
		free(t_buf->hkt);
		t_buf->hkt = NULL;
		t_buf->hkt_len = 0;
	}
	return false;
}

int Condor_Auth_Passwd::server_send(int server_status, 
									struct msg_t_buf *t_server, 
									struct sk_buf *sk) 
{
	char *send_a = t_server->a;
	char *send_b = t_server->b;
	unsigned char *send_ra = t_server->ra;
	unsigned char *send_rb = t_server->rb;
	int send_a_len = 0;
	int send_b_len = 0;
	int send_ra_len = AUTH_PW_KEY_LEN;
	int send_rb_len = AUTH_PW_KEY_LEN;
	unsigned char *send_hkt = NULL;
	int send_hkt_len = 0;
	char nullstr[2];

	dprintf(D_SECURITY|D_VERBOSE, "In server_send: %d.\n", server_status);

	nullstr[0] = 0;
	nullstr[1] = 0;

		// Make sure everything's ok.
	if(server_status == AUTH_PW_A_OK) {
		if(!send_a || !send_b 
		   || !send_ra || !send_rb) {
			dprintf(D_SECURITY, "Error: NULL or zero length string in T!\n");
			server_status = AUTH_PW_ERROR;
		} else {
			send_a_len = strlen(send_a);
			send_b_len = strlen(send_b);
			if(!calculate_hkt(t_server, sk)) {
				server_status = AUTH_PW_ERROR;
			}
		}
	}

		// Set what will get sent.
	if(server_status !=AUTH_PW_A_OK) {
		send_a = nullstr;
		send_b = nullstr;
		send_ra = (unsigned char *)nullstr;
		send_rb = (unsigned char *)nullstr;
		send_a_len = 0;
		send_b_len = 0;
		send_ra_len = 0;
		send_rb_len = 0;
		send_hkt = (unsigned char *)nullstr;
		send_hkt_len = 0;
	} else {
		send_hkt = t_server->hkt;
		send_hkt_len = t_server->hkt_len;
	}
	dprintf(D_SECURITY|D_VERBOSE, "Server send '%s', '%s', %d %d %d\n", 
			send_a, send_b, send_ra_len, send_rb_len, send_hkt_len);

	mySock_->encode();
	if( !mySock_->code(server_status)
		|| !mySock_->code(send_a_len)
		|| !mySock_->code(send_a)
		|| !mySock_->code(send_b_len)
		|| !mySock_->code(send_b)
		|| !mySock_->code(send_ra_len)
		|| !(send_ra_len == mySock_->put_bytes(send_ra, send_ra_len))
		|| !mySock_->code(send_rb_len)
		|| !(send_rb_len == mySock_->put_bytes(send_rb, send_rb_len))
		|| !mySock_->code(send_hkt_len)
		|| !(send_hkt_len == mySock_->put_bytes(send_hkt, send_hkt_len))
		|| !mySock_->end_of_message()) {
		dprintf(D_SECURITY, "Error sending to client.  Aborting...\n");
		server_status = AUTH_PW_ABORT;
	}
	return server_status;
}

int Condor_Auth_Passwd :: client_receive(int *client_status, 
										 msg_t_buf *t_server) 
{
	int server_status  = AUTH_PW_ERROR;
	char *a            = (char *)malloc(AUTH_PW_MAX_NAME_LEN);
	int a_len          = 0;
	char *b            = (char *)malloc(AUTH_PW_MAX_NAME_LEN);
	int b_len          = 0;
	unsigned char *ra  = (unsigned char *)malloc(AUTH_PW_KEY_LEN);
	int ra_len         = 0;
	unsigned char *rb  = (unsigned char *)malloc(AUTH_PW_KEY_LEN);
	int rb_len         = 0;
	unsigned char *hkt = (unsigned char *)malloc(EVP_MAX_MD_SIZE);
	int hkt_len        = 0;

	if(!a || !b || !ra || !rb || !hkt) {
		dprintf(D_SECURITY, "Malloc error.  Aborting...\n");
		*client_status = AUTH_PW_ABORT;
		server_status = AUTH_PW_ABORT;
		goto client_receive_abort;
	}
	memset(ra, 0, AUTH_PW_KEY_LEN);
	memset(rb, 0, AUTH_PW_KEY_LEN);
	memset(hkt, 0, EVP_MAX_MD_SIZE);

		// Get the data
	mySock_->decode();
	if( !mySock_->code(server_status)
		|| !mySock_->code(a_len)
		|| !mySock_->get(a,AUTH_PW_MAX_NAME_LEN)
		|| !mySock_->code(b_len)
		|| !mySock_->get(b,AUTH_PW_MAX_NAME_LEN)
		|| !mySock_->code(ra_len)
		|| !(ra_len <= AUTH_PW_KEY_LEN)
		|| !(ra_len  == mySock_->get_bytes(ra, ra_len))
		|| !mySock_->code(rb_len)
		|| !(rb_len <= AUTH_PW_KEY_LEN)
		|| !(rb_len  == mySock_->get_bytes(rb, rb_len))
		|| !mySock_->code(hkt_len)
		|| !(hkt_len <= EVP_MAX_MD_SIZE)
		|| !(hkt_len == mySock_->get_bytes(hkt, hkt_len))
		|| !mySock_->end_of_message()) {
		dprintf(D_SECURITY, "Error communicating with server.  Aborting...\n");
		*client_status = AUTH_PW_ABORT;
		server_status = AUTH_PW_ABORT;
		goto client_receive_abort;
	}

		// Make sure the random strings are the right size.
	if(server_status == AUTH_PW_A_OK && (ra_len != AUTH_PW_KEY_LEN || rb_len != AUTH_PW_KEY_LEN)) {
		dprintf(D_SECURITY, "Incorrect protocol.\n");
		server_status = AUTH_PW_ERROR;
	}
		
		// Fill in the struct.
	if(server_status == AUTH_PW_A_OK) {
		t_server->a = a;
		t_server->b = b;
		t_server->ra = ra;
		dprintf(D_SECURITY|D_VERBOSE, "Wrote server ra.\n");
		t_server->rb =rb;
		t_server->hkt = hkt;
		t_server->hkt_len = hkt_len;
		return server_status;
	} else {
		dprintf(D_SECURITY, "Server sent status indicating not OK.\n");
	}

 client_receive_abort:
		// If we get here, there's been an error.  Avoid leaks...
	if(a) free(a);
	if(b) free(b);
	if(ra) free(ra);
	if(rb) free(rb);
	if(hkt) free(hkt);
	return server_status;
}

int Condor_Auth_Passwd::client_send_one(int client_status, msg_t_buf *t_client)
{
	char *send_a = 0;
	unsigned char *send_b = 0;
	
	if(t_client && t_client->a) send_a = t_client->a;
	if(t_client && t_client->ra) send_b = t_client->ra;
	int send_a_len=0;
	if(send_a) send_a_len = strlen(send_a);
	int send_b_len = AUTH_PW_KEY_LEN;
	char nullstr[2];

	nullstr[0] = 0;
	nullstr[1] = 0;
	if(client_status == AUTH_PW_A_OK 
	   && (!send_a || !send_b || !send_a_len)) {
		client_status = AUTH_PW_ERROR;
		dprintf(D_SECURITY,  "Client error: NULL in send?\n");
	}

		// If there's a problem, we don't send anything.
	if(client_status != AUTH_PW_A_OK) {
		send_a = nullstr;
		send_b = (unsigned char *)nullstr;
		send_a_len = 0;
		send_b_len = 0;
	}
	dprintf(D_SECURITY|D_VERBOSE, "Client sending: %d, %d(%s), %d\n", 
			client_status, send_a_len, send_a, send_b_len);

	mySock_->encode();
	std::string &init_token = t_client->a_token;
	if( !mySock_->code(client_status)
		|| !mySock_->code(send_a_len)
		|| !mySock_->code(send_a)
		|| !(m_version == 1 || mySock_->code(init_token))
		|| !mySock_->code(send_b_len)
		|| !(send_b_len == mySock_->put_bytes(send_b, send_b_len))
		|| !(mySock_->end_of_message())) {
		dprintf(D_SECURITY, "Error sending to server (first message).  "
				"Aborting...\n");
		client_status = AUTH_PW_ABORT;
	}
	return client_status;
}

int Condor_Auth_Passwd::server_receive_one(int *server_status, 
										   struct msg_t_buf *t_client)
{
	int client_status = AUTH_PW_ERROR;
	char *a           = NULL;
	int a_len         = 0;
	unsigned char *ra = (unsigned char *)malloc(AUTH_PW_KEY_LEN);
	int ra_len        = 0;
	std::string init_token;

	if(!ra) {
		dprintf(D_SECURITY, "Malloc error 6.\n");
		*server_status = AUTH_PW_ABORT;
		client_status = AUTH_PW_ABORT;
		goto server_receive_one_abort;
	}

	mySock_->decode();
	if( !mySock_->code(client_status)
		|| !mySock_->code(a_len)
		|| !mySock_->code(a) 
		|| !(m_version == 1 || mySock_->code(init_token))
		|| !mySock_->code(ra_len)
		|| !(ra_len <= AUTH_PW_KEY_LEN)
		|| !(ra_len == mySock_->get_bytes(ra, ra_len))
		|| !mySock_->end_of_message()) {

		dprintf(D_SECURITY, "Error communicating with client.  Aborting...\n");
		*server_status = AUTH_PW_ABORT;
		client_status = AUTH_PW_ABORT;
		goto server_receive_one_abort;
	}
	dprintf(D_SECURITY|D_VERBOSE, "Received: %d, %d(%s), %d\n", client_status, a_len, 
			a, ra_len);
		// If everything's ok, incorporate into data structure.
	if(client_status == AUTH_PW_A_OK 
	   && *server_status == AUTH_PW_A_OK) { 
		if(ra_len != AUTH_PW_KEY_LEN) {
			dprintf(D_SECURITY, "Bad length on received data: %d.\n", ra_len);
			*server_status = AUTH_PW_ERROR;
		}
	}
	if(client_status == AUTH_PW_A_OK && *server_status == AUTH_PW_A_OK) {
		t_client->a = a;
		t_client->ra = ra;
		t_client->a_token = init_token;
		return client_status;
	}
 server_receive_one_abort:
	if(a) free(a);
	if(ra) free(ra);
	return client_status;
}

int Condor_Auth_Passwd :: isValid() const
{
	if ( m_crypto && m_crypto_state) {
		return TRUE;
	} else {
		return FALSE;
	}
}

bool
Condor_Auth_Passwd::set_session_key(struct msg_t_buf *t_buf, struct sk_buf *sk)
{
	unsigned char *key = (unsigned char *)malloc(key_strength_bytes());
	unsigned int key_len = key_strength_bytes();
	
	dprintf(D_SECURITY|D_VERBOSE, "Setting session key.\n");

	if(!t_buf->rb || !sk->kb || !sk->kb_len || !key) {
			// shouldn't happen
		dprintf(D_SECURITY, "Unexpected NULL.\n");
		if (key) free(key);
		return false;
	}
	
	memset(key, 0, key_strength_bytes());

		// get rid of any old crypto object
	if ( m_crypto ) delete m_crypto;
	m_crypto = NULL;

	if ( m_crypto_state ) delete m_crypto_state;
	m_crypto_state = NULL;

		// Calculate W based on K'
	if (m_version == 1) {
		hmac( t_buf->rb, AUTH_PW_KEY_LEN,
			  sk->kb, sk->kb_len,
			  key, &key_len );
	} else {
		if (hkdf( t_buf->rb, AUTH_PW_KEY_LEN,
			(const unsigned char *)"session key", 11,
			(const unsigned char *)"htcondor", 8,
			key, key_strength_bytes()))
		{
			free(key);
			return false;
		}
	}

	dprintf(D_SECURITY|D_VERBOSE, "Key length: %d\n", key_len);
		// Fill the key structure.
	KeyInfo thekey(key, (int)key_len, CONDOR_3DES, 0);
	m_crypto = new Condor_Crypt_3des();
	if ( m_crypto ) {
		m_crypto_state = new Condor_Crypto_State(CONDOR_3DES,thekey);
		// if this failed, clean up other mem
		if ( !m_crypto_state ) {
			delete m_crypto;
			m_crypto = NULL;
		}
	}
	if ( key ) free(key);	// KeyInfo makes a copy of the key

	return m_crypto ? true : false;
}


int
Condor_Auth_Passwd::key_strength_bytes() const
{
	if (m_version == 1) {
		return EVP_MAX_MD_SIZE;
	} else {
		return key_strength_bytes_v2();
	}
}


bool
Condor_Auth_Passwd::preauth_metadata(classad::ClassAd &ad)
{
	dprintf(D_SECURITY|D_VERBOSE, "Inserting pre-auth metadata for TOKEN.\n");
	CondorError err;
	const std::string & keys = getCachedIssuerKeyNames(&err);
	if ( ! err.empty()) {
		dprintf(D_SECURITY, "Failed to determine available TOKEN keys: %s\n", err.getFullText().c_str());
		return false;
	}
	if ( ! keys.empty()) {
		ad.Assign(ATTR_SEC_ISSUER_KEYS, keys);
	}
	return true;
}

void
Condor_Auth_Passwd::create_pool_signing_key_if_needed()
{
	// This method is invoked by DaemonCore upon startup and a reconfig.

	// The collector will generate a token signing key by default.
	if( get_mySubSystem()->isType(SUBSYSTEM_TYPE_COLLECTOR) ) {
		std::string filepath;
		if(! param(filepath, "SEC_TOKEN_POOL_SIGNING_KEY_FILE")) {
			return;
		}

		create_signing_key( filepath, "POOL" );
	}

	const char * localName = get_mySubSystem()->getLocalName();
	if( localName && strcmp( localName, "AP_COLLECTOR" ) == 0 ) {
		std::string filepath;
		if(! param(filepath, "SEC_PASSWORD_DIRECTORY")) {
			return;
		}

		std::string tokenname;
		if(! param(tokenname, "SEC_TOKEN_AP_SIGNING_KEY_NAME")) {
			return;
		}

		filepath += "/" + tokenname;
		create_signing_key( filepath, "AP" );
	}
}

void
Condor_Auth_Passwd::create_signing_key( const std::string & filepath, const char * name ) {
		// Try to create the signing key file if it doesn't exist.
	int fd = -1;
	{
#ifdef WIN32
		const int open_flags = O_WRONLY | O_CREAT | O_EXCL | O_BINARY;
#else
		const int open_flags = O_WRONLY | O_CREAT | O_EXCL;
#endif
		mode_t access_mode = 0600;

		TemporaryPrivSentry sentry(PRIV_ROOT);
		fd = safe_open_wrapper_follow(filepath.c_str(), open_flags, access_mode);
	}
	if (fd < 0) {
		return;
	} else {
		close(fd);
	}

		// Generate a signing key.
	char rand_buffer[64];
	int r = RAND_bytes(reinterpret_cast<unsigned char *>(rand_buffer), sizeof(rand_buffer));
	ASSERT(r == 1)


		// Write out the signing key.
	if (TRUE == write_binary_password_file(filepath.c_str(), rand_buffer, sizeof(rand_buffer))) {
		dprintf(D_ALWAYS, "Created %s token signing key in file %s\n", name, filepath.c_str());
	}
	else {
		dprintf(D_ALWAYS, "WARNING: Failed to create %s token signing key in file %s\n", name, filepath.c_str());
	}
}


bool
Condor_Auth_Passwd::should_try_auth()
{
	bool has_named_creds = false;
	CondorError err;
	const std::string & keys = getCachedIssuerKeyNames(&err);
	if ( ! err.empty()) {
		dprintf(D_SECURITY, "Failed to determine available TOKEN keys: %s\n", err.getFullText().c_str());
		// return TRUE because we are probably a tool that does not have read access to the signing keys
		return true;
	}
	has_named_creds = ! keys.empty();
	if (has_named_creds) {
		dprintf(D_SECURITY|D_VERBOSE,
			"Can try token auth because we have at least one named credential.\n");
		return true;
	}

		// Did we perform the search recently?  If so,
		// we should just return the prior result.
	if (!m_should_search_for_tokens) {
		return m_tokens_avail;
	}
	m_should_search_for_tokens = false;

	std::string issuer;
	std::set<std::string> server_key_ids;
	std::string username, token, signature;

	m_tokens_avail = findTokens(issuer, server_key_ids, SecMan::getTagCredentialOwner(), username, token, signature);
	if (m_tokens_avail) {
		dprintf(D_SECURITY/*|D_VERBOSE*/,
			"Can try token auth because we have at least one token.\n");
	}
	return m_tokens_avail;
}


void
Condor_Auth_Passwd::set_remote_keys(const std::vector<std::string> &keys) {
	for (const auto &key : keys) {
		m_server_keys.insert(key);
	}
}
