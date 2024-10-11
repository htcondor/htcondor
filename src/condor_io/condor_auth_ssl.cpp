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

#define ouch(x) dprintf(D_SECURITY,"SSL Auth: %s",x)
#include "authentication.h"
#include "condor_auth_ssl.h"
#include "condor_config.h"
#include "condor_environ.h"
#include "CondorError.h"
#include "openssl/rand.h"
#include "condor_netdb.h"
#include "condor_sinful.h"
#include "condor_secman.h"
#include "condor_scitokens.h"
#include "ca_utils.h"
#include "fcloser.h"
#include "globus_utils.h"

#if defined(DLOPEN_SECURITY_LIBS)
#include <dlfcn.h>
#include "condor_auth_kerberos.h"
#endif

#include "condor_attributes.h"
#include "secure_file.h"
#include "subsystem_info.h"

#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/err.h>
#ifdef WIN32
#include <openssl/applink.c>
#endif

#include <algorithm>
#include <sstream>
#include <iomanip>
#include <string.h>

// remove when we have C++ 23 everywhere
#include "zip_view.hpp"

#include "condor_daemon_core.h"

GCC_DIAG_OFF(float-equal)
GCC_DIAG_OFF(cast-qual)
#include "jwt-cpp/jwt.h"
GCC_DIAG_ON(float-equal)
GCC_DIAG_ON(cast-qual)

namespace {

bool hostname_match(const char *match_pattern, const char *hostname)
{
    char match_copy[256], host_copy[256];
    char *tok1, *tok2;
    char *run1, *run2;
    int idx;

    // Basic input sanity checks.
    if ((match_pattern == NULL || hostname == NULL) ||
       ((strlen(match_pattern) > 255) || strlen(hostname) > 255))
        return false;

    // All matching on hostnames must be case insensitive.
    for (idx = 0; match_pattern[idx]; idx++) {
        match_copy[idx] = tolower(match_pattern[idx]);
    }
    match_copy[idx] = '\0';
    for (idx = 0; hostname[idx]; idx++) {
        host_copy[idx] = tolower(hostname[idx]);
    }

    host_copy[idx] = '\0';

	// This is the same trick as done in condor_startd.V6/ResAttributes.cpp; as
	// noted there, it's not the exact same function but "close enough"
#ifdef WIN32
#define strtok_r strtok_s
#endif

    // Split the strings by '.' character, iterate through each sub-component.
    run1 = nullptr;
    run2 = nullptr;
    for (tok1 = strtok_r(match_copy, ".", &run1), tok2 = strtok_r(host_copy, ".", &run2);
         tok1 && tok2;
         tok1 = strtok_r(NULL, ".", &run1), tok2 = strtok_r(NULL, ".", &run2))
    {
        // Match non-wildcard bits
        while (*tok1 && *tok2 && *tok1 == *tok2) {
            if (*tok2 == '*')
               return false;
            if (*tok1 == '*')
                break;
            tok1++;
            tok2++;
        }

        /**
         * At this point, one of the following must be true:
         * - We hit a wildcard.  In this case, we accept the match as
         *   long as there is no non-wildcard after it.
         * - We hit a character that doesn't match.
         * - We hit the of at least one string.
         */
        if (*tok1 == '*') {
            tok1++;
            // Non-wildcard after wildcard -- not acceptable.
            if (*tok1 != '\0')
                return false;
        }
        // Only accept the match if both components are at their end.
        else if (*tok1 || *tok2) {
            return false;
        }
    }
    return !tok1 && !tok2;
}

int g_last_verify_error_index = -1;

}

// Symbols from libssl
#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
static decltype(&SSL_CTX_ctrl) SSL_CTX_ctrl_ptr = nullptr;
#else
static decltype(&SSL_CTX_set_options) SSL_CTX_set_options_ptr = nullptr;
#endif
static decltype(&SSL_peek) SSL_peek_ptr = nullptr;
static decltype(&SSL_CTX_free) SSL_CTX_free_ptr = nullptr;
static decltype(&SSL_CTX_load_verify_locations) SSL_CTX_load_verify_locations_ptr = nullptr;
static decltype(&SSL_CTX_set_default_verify_paths) SSL_CTX_set_default_verify_paths_ptr = nullptr;
static decltype(&SSL_CTX_new) SSL_CTX_new_ptr = nullptr;
static decltype(&SSL_CTX_set_cipher_list) SSL_CTX_set_cipher_list_ptr = nullptr;
static decltype(&SSL_CTX_set_verify) SSL_CTX_set_verify_ptr = nullptr;
static decltype(&SSL_CTX_use_PrivateKey_file) SSL_CTX_use_PrivateKey_file_ptr = nullptr;
static decltype(&SSL_CTX_use_certificate_chain_file) SSL_CTX_use_certificate_chain_file_ptr = nullptr;
static decltype(&SSL_accept) SSL_accept_ptr = nullptr;
static decltype(&SSL_connect) SSL_connect_ptr = nullptr;
static decltype(&SSL_free) SSL_free_ptr = nullptr;
static decltype(&SSL_get_error) SSL_get_error_ptr = nullptr;
static decltype(&SSL_get_peer_certificate) SSL_get_peer_certificate_ptr = nullptr;
static decltype(&SSL_get_verify_result) SSL_get_verify_result_ptr = nullptr;
#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
static decltype(&SSL_library_init) SSL_library_init_ptr = nullptr;
static decltype(&SSL_load_error_strings) SSL_load_error_strings_ptr = nullptr;
#else
static decltype(&OPENSSL_init_ssl) OPENSSL_init_ssl_ptr = nullptr;
#endif
static decltype(&SSL_new) SSL_new_ptr = nullptr;
static decltype(&SSL_read) SSL_read_ptr = nullptr;
static decltype(&SSL_set_bio) SSL_set_bio_ptr = nullptr;
static decltype(&SSL_write) SSL_write_ptr = nullptr;
#if OPENSSL_VERSION_NUMBER < 0x10100000L
static decltype(&SSLv23_method) SSL_method_ptr = nullptr;
#else
static decltype(&TLS_method) SSL_method_ptr = nullptr;
#endif
static decltype(&SSL_CTX_set1_param) SSL_CTX_set1_param_ptr = nullptr;
static decltype(&SSL_get_current_cipher) SSL_get_current_cipher_ptr = nullptr;
static decltype(&SSL_CIPHER_get_name) SSL_CIPHER_get_name_ptr = nullptr;
static decltype(&SSL_get_ex_data_X509_STORE_CTX_idx) SSL_get_ex_data_X509_STORE_CTX_idx_ptr = nullptr;
static decltype(&SSL_get_ex_data) SSL_get_ex_data_ptr = nullptr;
static decltype(&SSL_set_ex_data) SSL_set_ex_data_ptr = nullptr;
#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
static decltype(&SSL_get_peer_cert_chain) SSL_get_peer_cert_chain_ptr = nullptr;
#else
static decltype(&SSL_get0_verified_chain) SSL_get0_verified_chain_ptr = nullptr;
#endif

bool Condor_Auth_SSL::m_initTried = false;
bool Condor_Auth_SSL::m_initSuccess = false;
bool Condor_Auth_SSL::m_should_search_for_cert = true;
bool Condor_Auth_SSL::m_cert_avail = false;

int Condor_Auth_SSL::m_pluginReaperId = -1;
std::map<int, Condor_Auth_SSL*> Condor_Auth_SSL::m_pluginPidTable;

Condor_Auth_SSL::AuthState::~AuthState() {
	if (m_ctx) {(*SSL_CTX_free_ptr)(m_ctx); m_ctx = nullptr;}
	if (m_ssl) {
		(*SSL_free_ptr)(m_ssl); m_ssl = nullptr;
	} else {
		if (m_conn_in) {BIO_free( m_conn_in );}
		if (m_conn_out) {BIO_free( m_conn_out );}
	}
}

Condor_Auth_SSL :: Condor_Auth_SSL(ReliSock * sock, int /* remote */, bool scitokens_mode)
    : Condor_Auth_Base    ( sock, scitokens_mode ? CAUTH_SCITOKENS : CAUTH_SSL ),
	m_scitokens_mode(scitokens_mode),
	m_pluginRC(0)
{
	m_crypto = NULL;
	m_crypto_state = NULL;
	ASSERT( Initialize() == true );
}

Condor_Auth_SSL :: ~Condor_Auth_SSL()
{
#if OPENSSL_VERSION_NUMBER < 0x10000000L
    ERR_remove_state( 0 );
#elif OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
    ERR_remove_thread_state( 0 );
#endif
	if(m_crypto) delete(m_crypto);
	if(m_crypto_state) delete(m_crypto_state);
	if (m_pluginState && m_pluginState->m_pid > 0) {
		m_pluginPidTable[m_pluginState->m_pid] = nullptr;
	}
}

bool Condor_Auth_SSL::Initialize()
{
	if ( m_initTried ) {
		return m_initSuccess;
	}

#if defined(DLOPEN_SECURITY_LIBS)
	void *dl_hdl;

	dlerror();

	if (
#if defined(HAVE_EXT_KRB5)
		Condor_Auth_Kerberos::Initialize() == false ||
#endif
		 (dl_hdl = dlopen(LIBSSL_SO, RTLD_LAZY)) == NULL ||
#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
		 !(SSL_CTX_ctrl_ptr = reinterpret_cast<decltype(SSL_CTX_ctrl_ptr)>(dlsym(dl_hdl, "SSL_CTX_ctrl"))) ||
#else
		 !(SSL_CTX_set_options_ptr = reinterpret_cast<decltype(SSL_CTX_set_options_ptr)>(dlsym(dl_hdl, "SSL_CTX_set_options"))) ||
#endif
		 !(SSL_peek_ptr = reinterpret_cast<decltype(SSL_peek_ptr)>(dlsym(dl_hdl, "SSL_peek"))) ||
		 !(SSL_CTX_free_ptr = reinterpret_cast<decltype(SSL_CTX_free_ptr)>(dlsym(dl_hdl, "SSL_CTX_free"))) ||
		 !(SSL_CTX_load_verify_locations_ptr = reinterpret_cast<decltype(SSL_CTX_load_verify_locations_ptr)>(dlsym(dl_hdl, "SSL_CTX_load_verify_locations"))) ||
		 !(SSL_CTX_set_default_verify_paths_ptr = reinterpret_cast<decltype(SSL_CTX_set_default_verify_paths_ptr)>(dlsym(dl_hdl, "SSL_CTX_set_default_verify_paths"))) ||
		 !(SSL_CTX_new_ptr = reinterpret_cast<decltype(SSL_CTX_new_ptr)>(dlsym(dl_hdl, "SSL_CTX_new"))) ||
		 !(SSL_CTX_set_cipher_list_ptr = reinterpret_cast<decltype(SSL_CTX_set_cipher_list_ptr)>(dlsym(dl_hdl, "SSL_CTX_set_cipher_list"))) ||
		 !(SSL_CTX_set_verify_ptr = reinterpret_cast<decltype(SSL_CTX_set_verify_ptr)>(dlsym(dl_hdl, "SSL_CTX_set_verify"))) ||
		 !(SSL_CTX_use_PrivateKey_file_ptr = reinterpret_cast<decltype(SSL_CTX_use_PrivateKey_file_ptr)>(dlsym(dl_hdl, "SSL_CTX_use_PrivateKey_file"))) ||
		 !(SSL_CTX_use_certificate_chain_file_ptr = reinterpret_cast<decltype(SSL_CTX_use_certificate_chain_file_ptr)>(dlsym(dl_hdl, "SSL_CTX_use_certificate_chain_file"))) ||
		 !(SSL_accept_ptr = reinterpret_cast<decltype(SSL_accept_ptr)>(dlsym(dl_hdl, "SSL_accept"))) ||
		 !(SSL_connect_ptr = reinterpret_cast<decltype(SSL_connect_ptr)>(dlsym(dl_hdl, "SSL_connect"))) ||
		 !(SSL_free_ptr = reinterpret_cast<decltype(SSL_free_ptr)>(dlsym(dl_hdl, "SSL_free"))) ||
		 !(SSL_get_error_ptr = reinterpret_cast<decltype(SSL_get_error_ptr)>(dlsym(dl_hdl, "SSL_get_error"))) ||
#if OPENSSL_VERSION_NUMBER < 0x30000000L || defined(LIBRESSL_VERSION_NUMBER)
		 !(SSL_get_peer_certificate_ptr = reinterpret_cast<decltype(SSL_get_peer_certificate_ptr)>(dlsym(dl_hdl, "SSL_get_peer_certificate"))) ||
#else
		 !(SSL_get_peer_certificate_ptr = reinterpret_cast<decltype(SSL_get_peer_certificate_ptr)>(dlsym(dl_hdl, "SSL_get1_peer_certificate"))) ||
#endif
		 !(SSL_get_verify_result_ptr = reinterpret_cast<decltype(SSL_get_verify_result_ptr)>(dlsym(dl_hdl, "SSL_get_verify_result"))) ||
#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
		 !(SSL_library_init_ptr = reinterpret_cast<decltype(SSL_library_init_ptr)>(dlsym(dl_hdl, "SSL_library_init"))) ||
		 !(SSL_load_error_strings_ptr = reinterpret_cast<decltype(SSL_load_error_strings_ptr)>(dlsym(dl_hdl, "SSL_load_error_strings"))) ||
#else
		 !(OPENSSL_init_ssl_ptr = reinterpret_cast<decltype(OPENSSL_init_ssl_ptr)>(dlsym(dl_hdl, "OPENSSL_init_ssl"))) ||
#endif
		 !(SSL_new_ptr = reinterpret_cast<decltype(SSL_new_ptr)>(dlsym(dl_hdl, "SSL_new"))) ||
		 !(SSL_read_ptr = reinterpret_cast<decltype(SSL_read_ptr)>(dlsym(dl_hdl, "SSL_read"))) ||
		 !(SSL_set_bio_ptr = reinterpret_cast<decltype(SSL_set_bio_ptr)>(dlsym(dl_hdl, "SSL_set_bio"))) ||
		 !(SSL_write_ptr = reinterpret_cast<decltype(SSL_write_ptr)>(dlsym(dl_hdl, "SSL_write"))) ||
		 !(SSL_CTX_set1_param_ptr = reinterpret_cast<decltype(SSL_CTX_set1_param_ptr)>(dlsym(dl_hdl, "SSL_CTX_set1_param"))) ||
		 !(SSL_get_current_cipher_ptr = reinterpret_cast<decltype(SSL_get_current_cipher_ptr)>(dlsym(dl_hdl, "SSL_get_current_cipher"))) ||
		 !(SSL_CIPHER_get_name_ptr = reinterpret_cast<decltype(SSL_CIPHER_get_name_ptr)>(dlsym(dl_hdl, "SSL_CIPHER_get_name"))) ||
		 !(SSL_get_ex_data_X509_STORE_CTX_idx_ptr = reinterpret_cast<decltype(SSL_get_ex_data_X509_STORE_CTX_idx_ptr)>(dlsym(dl_hdl, "SSL_get_ex_data_X509_STORE_CTX_idx"))) ||
		 !(SSL_get_ex_data_ptr = reinterpret_cast<decltype(SSL_get_ex_data_ptr)>(dlsym(dl_hdl, "SSL_get_ex_data"))) ||
		 !(SSL_set_ex_data_ptr = reinterpret_cast<decltype(SSL_set_ex_data_ptr)>(dlsym(dl_hdl, "SSL_set_ex_data"))) ||
#if OPENSSL_VERSION_NUMBER < 0x10100000L
		 !(SSL_get_peer_cert_chain_ptr = reinterpret_cast<decltype(SSL_get_peer_cert_chain_ptr)>(dlsym(dl_hdl, "SSL_get_peer_cert_chain"))) ||
		 !(SSL_method_ptr = reinterpret_cast<decltype(SSL_method_ptr)>(dlsym(dl_hdl, "SSLv23_method")))
#else
		 !(SSL_get0_verified_chain_ptr = reinterpret_cast<decltype(SSL_get0_verified_chain_ptr)>(dlsym(dl_hdl, "SSL_get0_verified_chain"))) ||
		 !(SSL_method_ptr = reinterpret_cast<decltype(SSL_method_ptr)>(dlsym(dl_hdl, "TLS_method")))
#endif
		 ) {

		// Error in the dlopen/sym calls, return failure.
		// If dlerror() returns NULL, then assume the failure was in
		// Condor_Auth_Kerberos::Initialize(), which already printed
		// an error message.
		 const char *err_msg = dlerror();
		if ( err_msg ) {
			dprintf( D_ALWAYS, "Failed to open OpenSSL library: %s\n", err_msg );
		}
		m_initSuccess = false;
	} else {
		m_initSuccess = true;
	}

#else
#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
	SSL_CTX_ctrl_ptr = SSL_CTX_ctrl;
#else
	SSL_CTX_set_options_ptr = SSL_CTX_set_options;
#endif
	SSL_peek_ptr = SSL_peek;
	SSL_CTX_free_ptr = SSL_CTX_free;
	SSL_CTX_load_verify_locations_ptr = SSL_CTX_load_verify_locations;
	SSL_CTX_set_default_verify_paths_ptr = SSL_CTX_set_default_verify_paths;
	SSL_CTX_new_ptr = SSL_CTX_new;
	SSL_CTX_set_cipher_list_ptr = SSL_CTX_set_cipher_list;
	SSL_CTX_set_verify_ptr = SSL_CTX_set_verify;
	SSL_CTX_use_PrivateKey_file_ptr = SSL_CTX_use_PrivateKey_file;
	SSL_CTX_use_certificate_chain_file_ptr = SSL_CTX_use_certificate_chain_file;
	SSL_accept_ptr = SSL_accept;
	SSL_connect_ptr = SSL_connect;
	SSL_free_ptr = SSL_free;
	SSL_get_error_ptr = SSL_get_error;
	SSL_get_peer_certificate_ptr = SSL_get_peer_certificate;
	SSL_get_verify_result_ptr = SSL_get_verify_result;
#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
	SSL_library_init_ptr = SSL_library_init;
	SSL_load_error_strings_ptr = SSL_load_error_strings;
#else
	OPENSSL_init_ssl_ptr = OPENSSL_init_ssl;
#endif
	SSL_new_ptr = SSL_new;
	SSL_read_ptr = SSL_read;
	SSL_set_bio_ptr = SSL_set_bio;
	SSL_write_ptr = SSL_write;
#if OPENSSL_VERSION_NUMBER < 0x10100000L
	SSL_method_ptr = SSLv23_method;
#else
	SSL_method_ptr = TLS_method;
#endif
	SSL_CTX_set1_param_ptr = SSL_CTX_set1_param;
	SSL_get_current_cipher_ptr = SSL_get_current_cipher;
	SSL_CIPHER_get_name_ptr = SSL_CIPHER_get_name;
	SSL_get_ex_data_X509_STORE_CTX_idx_ptr = SSL_get_ex_data_X509_STORE_CTX_idx;
	SSL_get_ex_data_ptr = SSL_get_ex_data;
	SSL_set_ex_data_ptr = SSL_set_ex_data;
#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
	SSL_get_peer_cert_chain_ptr = SSL_get_peer_cert_chain;
#else
	SSL_get0_verified_chain_ptr = SSL_get0_verified_chain;
#endif

	m_initSuccess = true;
#endif

	m_initTried = true;
	return m_initSuccess;
}


int
Condor_Auth_SSL::authenticate_continue(CondorError *errstack, bool non_blocking)
{
	if (!m_auth_state) {
		ouch("Trying to continue authentication after failure!\n");
		return static_cast<int>(CondorAuthSSLRetval::Fail);
	}
	switch (m_auth_state->m_phase) {
	case Phase::Startup:
		ouch("authenticate_continue called when authentication is in wrong state.\n");
		return static_cast<int>(CondorAuthSSLRetval::Fail);
	case Phase::PreConnect:
		return static_cast<int>(authenticate_server_pre(errstack, non_blocking));
	case Phase::Connect:
		return static_cast<int>(authenticate_server_connect(errstack, non_blocking));
	case Phase::KeyExchange:
		return static_cast<int>(authenticate_server_key(errstack, non_blocking));
	case Phase::SciToken:
		return static_cast<int>(authenticate_server_scitoken(errstack, non_blocking));
	};
	return static_cast<int>(CondorAuthSSLRetval::Fail);
}

int Condor_Auth_SSL::authenticate(const char * /* remoteHost */, CondorError* errstack,
	bool non_blocking)
{
	if (!m_auth_state) {
		m_auth_state.reset(new AuthState);
	}

    if( mySock_->isClient() ) {
		m_host_alias = "";
        if( init_OpenSSL( ) != AUTH_SSL_A_OK ) {
            ouch( "Error initializing OpenSSL for authentication\n" );
            m_auth_state->m_client_status = AUTH_SSL_ERROR;
        }
        if( !(m_auth_state->m_ctx = setup_ssl_ctx( false )) ) {
            ouch( "Error initializing client security context\n" );
            m_auth_state->m_client_status = AUTH_SSL_ERROR;
        }

		{
			char const *connect_addr = mySock_->get_connect_addr();
			if (connect_addr) {
				Sinful s(connect_addr);
				char const *alias = s.getAlias();
				if (alias) {
					dprintf(D_SECURITY|D_FULLDEBUG,"SSL client host check: using host alias %s for peer %s\n", alias,
						mySock_->peer_ip_str());
					m_host_alias = alias;
				}
			}
		}

		std::string scitoken;
		if (m_scitokens_mode) {
			if (m_scitokens_file.empty()) {
					// Try to use the token from the environment.
				scitoken = htcondor::discover_token();

				if (scitoken.empty()) {
					ouch( "No SciToken file provided\n" );
					m_auth_state->m_client_status = AUTH_SSL_ERROR;
				}
			} else {
				std::unique_ptr<FILE,fcloser> f(
					safe_fopen_no_create( m_scitokens_file.c_str(), "r" ));

				if (f.get() == nullptr) {
					dprintf(D_ALWAYS, "Failed to open scitoken file '%s': %d (%s)\n",
						m_scitokens_file.c_str(), errno, strerror(errno));
					m_auth_state->m_client_status = AUTH_SSL_ERROR;
				} else {
					for (std::string line; readLine(line, f.get(), false); ) {
						// Strip out whitespace and ignore comments.
						trim(line);
						if (line.empty() || line[0] == '#') { continue; }
						scitoken = line;
						ouch( "Found a SciToken to use for authentication.\n" );
						break;
					}
				}
			}
		}

        if( !(m_auth_state->m_conn_in = BIO_new( BIO_s_mem( ) )) 
            || !(m_auth_state->m_conn_out = BIO_new( BIO_s_mem( ) )) ) {
            ouch( "Error creating buffer for SSL authentication\n" );
            m_auth_state->m_client_status = AUTH_SSL_ERROR;
        }
        if( !(m_auth_state->m_ssl = SSL_new_ptr( m_auth_state->m_ctx )) ) {
            ouch( "Error creating SSL context\n" );
            m_auth_state->m_client_status = AUTH_SSL_ERROR;
        } else {
            SSL_set_bio_ptr( m_auth_state->m_ssl, m_auth_state->m_conn_in,
				m_auth_state->m_conn_out );
            if (g_last_verify_error_index >= 0) {
                SSL_set_ex_data_ptr( m_auth_state->m_ssl, g_last_verify_error_index, &m_last_verify_error);
            }
        }
        m_auth_state->m_server_status = client_share_status( m_auth_state->m_client_status );
        if( m_auth_state->m_server_status != AUTH_SSL_A_OK ||
		m_auth_state->m_client_status != AUTH_SSL_A_OK ) {
            ouch( "SSL Authentication fails, terminating\n" );
            return static_cast<int>(CondorAuthSSLRetval::Fail);
        }

        m_auth_state->m_done = 0;
        m_auth_state->m_round_ctr = 0;

        while( !m_auth_state->m_done ) {
            if( m_auth_state->m_client_status != AUTH_SSL_HOLDING ) {
                ouch("Trying to connect.\n");
                m_auth_state->m_ssl_status = SSL_connect_ptr( m_auth_state->m_ssl );
                dprintf(D_SECURITY|D_VERBOSE, "Tried to connect: %d\n", m_auth_state->m_ssl_status);
            }
            if( m_auth_state->m_ssl_status < 1 ) {
                m_auth_state->m_client_status = AUTH_SSL_QUITTING;
                m_auth_state->m_done = 1;
                //ouch( "Error performing SSL authentication\n" );
                m_auth_state->m_err = SSL_get_error_ptr( m_auth_state->m_ssl, m_auth_state->m_ssl_status );
                switch( m_auth_state->m_err ) {
                case SSL_ERROR_ZERO_RETURN:
                    ouch("SSL: connection has been closed.\n");
                    break;
                case SSL_ERROR_WANT_READ:
                    ouch("SSL: trying to continue reading.\n");
                    m_auth_state->m_client_status = AUTH_SSL_RECEIVING;
                    m_auth_state->m_done = 0;
                    break;
                case SSL_ERROR_WANT_WRITE:
                    ouch("SSL: trying to continue writing.\n");
                    m_auth_state->m_client_status = AUTH_SSL_SENDING;
                    m_auth_state->m_done = 0;
                    break;
                case SSL_ERROR_WANT_CONNECT:
                case SSL_ERROR_WANT_ACCEPT:
                    ouch("SSL: error want connect/accept.\n");
                    break;
                case SSL_ERROR_WANT_X509_LOOKUP:
                    ouch("SSL: X509_LOOKUP: callback incomplete.\n" );
                    break;
                case SSL_ERROR_SYSCALL:
                    ouch("SSL: Syscall.\n" );
                    break;
                case SSL_ERROR_SSL:
                    dprintf(D_SECURITY, "SSL: library failure: %s\n", ERR_error_string(ERR_get_error(), NULL));
                    break;
                default:
                    ouch("SSL: unknown error?\n" );
                    break;
                }
            } else {
                m_auth_state->m_client_status = AUTH_SSL_HOLDING;
            }
            m_auth_state->m_round_ctr++;
            dprintf(D_SECURITY|D_VERBOSE,"Round %d.\n", m_auth_state->m_round_ctr);
            if(m_auth_state->m_round_ctr % 2 == 1) {
                if(AUTH_SSL_ERROR == client_send_message(
                       m_auth_state->m_client_status, m_auth_state->m_buffer,
						m_auth_state->m_conn_in, m_auth_state->m_conn_out )) {
                   m_auth_state->m_server_status = AUTH_SSL_QUITTING;
                }
            } else {
                m_auth_state->m_server_status = client_receive_message(
                    m_auth_state->m_client_status, m_auth_state->m_buffer,
			m_auth_state->m_conn_in, m_auth_state->m_conn_out );
            }
            dprintf(D_SECURITY|D_VERBOSE,"Status (c: %d, s: %d)\n", m_auth_state->m_client_status,
			m_auth_state->m_server_status);
            /* server_status = client_exchange_messages( client_status,
             *  buffer, conn_in,
             *  conn_out );
             */
            if( m_auth_state->m_server_status == AUTH_SSL_ERROR ) {
                m_auth_state->m_server_status = AUTH_SSL_QUITTING;
            }
            if( m_auth_state->m_server_status == AUTH_SSL_HOLDING
                && m_auth_state->m_client_status == AUTH_SSL_HOLDING ) {
                m_auth_state->m_done = 1;
            }
            if( m_auth_state->m_client_status == AUTH_SSL_QUITTING
                || m_auth_state->m_server_status == AUTH_SSL_QUITTING ) {
                ouch( "SSL Authentication failed\n" );
                return static_cast<int>(CondorAuthSSLRetval::Fail);
            }
        }
        dprintf(D_SECURITY|D_VERBOSE,"Client trying post connection check.\n");
        dprintf(D_SECURITY|D_VERBOSE, "Cipher used: %s.\n",
            SSL_CIPHER_get_name_ptr((*SSL_get_current_cipher_ptr)(m_auth_state->m_ssl)));

        if((m_auth_state->m_err = post_connection_check(
                m_auth_state->m_ssl, AUTH_SSL_ROLE_CLIENT )) != X509_V_OK ) {
            ouch( "Error on check of peer certificate\n" );
            snprintf(m_auth_state->m_err_buf, 500, "%s\n",
                     X509_verify_cert_error_string( m_auth_state->m_err ));
            ouch( m_auth_state->m_err_buf );
            m_auth_state->m_client_status = AUTH_SSL_QUITTING;
        } else {
            m_auth_state->m_client_status = AUTH_SSL_A_OK;
        }

        dprintf(D_SECURITY|D_VERBOSE,"Client performs one last exchange of messages.\n");

        if( m_auth_state->m_client_status == AUTH_SSL_QUITTING
            || m_auth_state->m_server_status == AUTH_SSL_QUITTING ) {
            ouch( "SSL Authentication failed\n" );
			// Read and ignore the session key from the server.
			// Then tell it we're bailing, if it still thinks
			// everything is ok.
			int dummy;
			if (CondorAuthSSLRetval::Success != receive_message(false,
				m_auth_state->m_server_status, dummy,
				m_auth_state->m_buffer))
			{
				m_auth_state->m_server_status = AUTH_SSL_QUITTING;
			}
			if (m_auth_state->m_server_status != AUTH_SSL_QUITTING) {
				send_message(AUTH_SSL_QUITTING, m_auth_state->m_buffer, 0);
			}
            return static_cast<int>(CondorAuthSSLRetval::Fail);
        }

        m_auth_state->m_client_status = m_auth_state->m_server_status = AUTH_SSL_RECEIVING;
        m_auth_state->m_done = 0;
        m_auth_state->m_round_ctr = 0;
			//unsigned char session_key[AUTH_SSL_SESSION_KEY_LEN];
        while(!m_auth_state->m_done) {
            dprintf(D_SECURITY|D_VERBOSE,"Reading round %d.\n",++m_auth_state->m_round_ctr);
            if(m_auth_state->m_round_ctr > 256) {
                ouch("Too many rounds exchanging key: quitting.\n");
                m_auth_state->m_done = 1;
                m_auth_state->m_client_status = AUTH_SSL_QUITTING;
                break;
            }
            if( m_auth_state->m_client_status != AUTH_SSL_HOLDING) {
                m_auth_state->m_ssl_status = SSL_read_ptr(m_auth_state->m_ssl, 
									  m_auth_state->m_session_key, AUTH_SSL_SESSION_KEY_LEN);
            }
            if(m_auth_state->m_ssl_status < 1) {
                m_auth_state->m_err = SSL_get_error_ptr( m_auth_state->m_ssl,
					m_auth_state->m_ssl_status);
                switch( m_auth_state->m_err ) {
                case SSL_ERROR_WANT_READ:
                case SSL_ERROR_WANT_WRITE:
                    ouch("SSL: continue read/write.\n");
                    m_auth_state->m_done = 0;
                    m_auth_state->m_client_status = AUTH_SSL_RECEIVING;
                    break;
                default:
                    m_auth_state->m_client_status = AUTH_SSL_QUITTING;
                    m_auth_state->m_done = 1;
                    ouch("SSL: error on write.  Can't proceed.\n");
                    break;
                }
            } else {
                dprintf(D_SECURITY|D_VERBOSE,"SSL read has succeeded.\n");
                m_auth_state->m_client_status = AUTH_SSL_HOLDING;
            }
            if(m_auth_state->m_round_ctr % 2 == 1) {
                m_auth_state->m_server_status = client_receive_message(
                    m_auth_state->m_client_status, m_auth_state->m_buffer,
					m_auth_state->m_conn_in, m_auth_state->m_conn_out );
            } else {
                if(AUTH_SSL_ERROR == client_send_message(
                       m_auth_state->m_client_status, m_auth_state->m_buffer,
						m_auth_state->m_conn_in, m_auth_state->m_conn_out )) {
                    m_auth_state->m_server_status = AUTH_SSL_QUITTING;
                }
            }
            dprintf(D_SECURITY|D_VERBOSE, "Status: c: %d, s: %d\n", m_auth_state->m_client_status,
				m_auth_state->m_server_status);
            if(m_auth_state->m_server_status == AUTH_SSL_HOLDING
               && m_auth_state->m_client_status == AUTH_SSL_HOLDING) {
                m_auth_state->m_done = 1;
            }
            if(m_auth_state->m_server_status == AUTH_SSL_QUITTING) {
                m_auth_state->m_done = 1;
            }
        }
        if( m_auth_state->m_server_status == AUTH_SSL_QUITTING
            || m_auth_state->m_client_status == AUTH_SSL_QUITTING ) {
            ouch( "SSL Authentication failed at session key exchange.\n" );
            return static_cast<int>(CondorAuthSSLRetval::Fail);
        }
        //dprintf(D_SECURITY, "Got session key: '%s'.\n", session_key);
        setup_crypto( m_auth_state->m_session_key, AUTH_SSL_SESSION_KEY_LEN );

		if (m_scitokens_mode) {
			m_auth_state->m_client_status = m_auth_state->m_server_status = AUTH_SSL_RECEIVING;
			m_auth_state->m_done = 0;
			m_auth_state->m_round_ctr = 0;
			uint32_t network_size = htonl(scitoken.size());
			std::vector<unsigned char> network_bytes(sizeof(network_size) + scitoken.size(), 0);
			memcpy(&network_bytes[0], static_cast<void*>(&network_size), sizeof(network_size));
			memcpy(&network_bytes[0] + sizeof(network_size), scitoken.c_str(), scitoken.size());
			while(!m_auth_state->m_done) {
				dprintf(D_SECURITY|D_VERBOSE,"Writing SciToken round %d.\n",++m_auth_state->m_round_ctr);

				// Abort if the exchange has gone on for too long.
				if(m_auth_state->m_round_ctr > 256) {
					ouch("Too many rounds exchanging key: quitting.\n");
					m_auth_state->m_done = 1;
					m_auth_state->m_client_status = AUTH_SSL_QUITTING;
					break;
				}

				if( m_auth_state->m_client_status != AUTH_SSL_HOLDING) {
					m_auth_state->m_ssl_status = SSL_write_ptr(m_auth_state->m_ssl,
						&network_bytes[0], sizeof(network_size) + scitoken.size());
				}
				if(m_auth_state->m_ssl_status < 1) {
					m_auth_state->m_err = SSL_get_error_ptr( m_auth_state->m_ssl,
						m_auth_state->m_ssl_status);
					switch( m_auth_state->m_err ) {
						case SSL_ERROR_WANT_READ:
						case SSL_ERROR_WANT_WRITE:
						ouch("SSL: continue read/write.\n");
						m_auth_state->m_done = 0;
						m_auth_state->m_client_status = AUTH_SSL_RECEIVING;
						break;
						default:
							m_auth_state->m_client_status = AUTH_SSL_QUITTING;
							m_auth_state->m_done = 1;
							ouch("SSL: error on write.  Can't proceed.\n");
							break;
					}
				} else {
					dprintf(D_SECURITY|D_VERBOSE,"SSL write is successful.\n");
					m_auth_state->m_client_status = AUTH_SSL_HOLDING;
				}
				if(m_auth_state->m_round_ctr % 2 == 0) {
					m_auth_state->m_server_status = client_receive_message(
						m_auth_state->m_client_status, m_auth_state->m_buffer,
						m_auth_state->m_conn_in, m_auth_state->m_conn_out );
				} else {
					if(AUTH_SSL_ERROR == client_send_message(
						m_auth_state->m_client_status, m_auth_state->m_buffer,
						m_auth_state->m_conn_in, m_auth_state->m_conn_out ))
					{
						m_auth_state->m_server_status = AUTH_SSL_QUITTING;
					}
				}
				dprintf(D_SECURITY|D_VERBOSE, "SciToken exchange status: c: %d, s: %d\n", m_auth_state->m_client_status,
					m_auth_state->m_server_status);
				if(m_auth_state->m_server_status == AUTH_SSL_HOLDING
					&& m_auth_state->m_client_status == AUTH_SSL_HOLDING)
				{
					m_auth_state->m_done = 1;
				}
				if(m_auth_state->m_server_status == AUTH_SSL_QUITTING) {
					m_auth_state->m_done = 1;
				}
			}
			if( m_auth_state->m_server_status == AUTH_SSL_QUITTING )
			{
				ouch ("Server has rejected our token!\n");
				return static_cast<int>(CondorAuthSSLRetval::Fail);
			}
			if (m_auth_state->m_client_status == AUTH_SSL_QUITTING)
			{
				ouch( "SciToken Authentication while client was sending the token.\n" );
				return static_cast<int>(CondorAuthSSLRetval::Fail);
			}
		}
    } else { // Server
        
        if( init_OpenSSL(  ) != AUTH_SSL_A_OK ) {
            ouch( "Error initializing OpenSSL for authentication\n" );
            m_auth_state->m_server_status = AUTH_SSL_ERROR;
        }
        if( !(m_auth_state->m_ctx = setup_ssl_ctx( true )) ) {
            ouch( "Error initializing server security context\n" );
            m_auth_state->m_server_status = AUTH_SSL_ERROR;
        }
        if( !(m_auth_state->m_conn_in = BIO_new( BIO_s_mem( ) ))
            || !(m_auth_state->m_conn_out = BIO_new( BIO_s_mem( ) )) ) {
            ouch( "Error creating buffer for SSL authentication\n" );
            m_auth_state->m_server_status = AUTH_SSL_ERROR;
        }
        if (!(m_auth_state->m_ssl = SSL_new_ptr(m_auth_state->m_ctx))) {
            ouch("Error creating SSL context\n");
            m_auth_state->m_server_status = AUTH_SSL_ERROR;
        } else {
            // SSL_set_accept_state(ssl); // Do I really have to do this?
            SSL_set_bio_ptr(m_auth_state->m_ssl, m_auth_state->m_conn_in,
				m_auth_state->m_conn_out);
        }
		if( send_status( m_auth_state->m_server_status ) == AUTH_SSL_ERROR ) {
			return static_cast<int>(CondorAuthSSLRetval::Fail);
		}
		CondorAuthSSLRetval tmp_status = authenticate_server_pre(errstack, non_blocking);
		if (tmp_status == CondorAuthSSLRetval::Fail) {
			return static_cast<int>(authenticate_fail());
		}

		return static_cast<int>(tmp_status);
	}
	return static_cast<int>(authenticate_finish(errstack, non_blocking));
}


Condor_Auth_SSL::CondorAuthSSLRetval
Condor_Auth_SSL::authenticate_server_pre(CondorError *errstack, bool non_blocking) {
		m_auth_state->m_phase = Phase::PreConnect;
		auto retval = receive_status( non_blocking, m_auth_state->m_client_status );
		if (retval != CondorAuthSSLRetval::Success) {
			return retval == CondorAuthSSLRetval::Fail ? authenticate_fail() : retval;
		}

        if( m_auth_state->m_client_status != AUTH_SSL_A_OK
            || m_auth_state->m_server_status != AUTH_SSL_A_OK ) {
            dprintf(D_SECURITY, "SSL Auth: SSL Authentication fails; client status is %d; server status is %d; terminating\n", m_auth_state->m_client_status, m_auth_state->m_server_status );
            return authenticate_fail();
        }
        m_auth_state->m_done = 0;
        m_auth_state->m_round_ctr = 0;
		return authenticate_server_connect(errstack, non_blocking);
}


Condor_Auth_SSL::CondorAuthSSLRetval
Condor_Auth_SSL::authenticate_server_connect(CondorError *errstack, bool non_blocking) {
		m_auth_state->m_phase = Phase::Connect;
        while( !m_auth_state->m_done ) {
            if( m_auth_state->m_server_status != AUTH_SSL_HOLDING ) {
                ouch("Trying to accept.\n");
                m_auth_state->m_ssl_status = SSL_accept_ptr( m_auth_state->m_ssl );
                dprintf(D_SECURITY|D_VERBOSE, "Accept returned %d.\n", m_auth_state->m_ssl_status);
            }
            if( m_auth_state->m_ssl_status < 1 ) {
                m_auth_state->m_server_status = AUTH_SSL_QUITTING;
                m_auth_state->m_done = 1;
                m_auth_state->m_err = SSL_get_error_ptr( m_auth_state->m_ssl,
					m_auth_state->m_ssl_status );
                switch( m_auth_state->m_err ) {
                case SSL_ERROR_ZERO_RETURN:
                    ouch("SSL: connection has been closed.\n");
                    break;
                case SSL_ERROR_WANT_READ:
                    ouch("SSL: trying to continue reading.\n");
                    m_auth_state->m_server_status = AUTH_SSL_RECEIVING;
                    m_auth_state->m_done = 0;
                    break;
                case SSL_ERROR_WANT_WRITE:
                    ouch("SSL: trying to continue writing.\n");
                    m_auth_state->m_server_status = AUTH_SSL_SENDING;
                    m_auth_state->m_done = 0;
                    break;
                case SSL_ERROR_WANT_CONNECT:
                case SSL_ERROR_WANT_ACCEPT:
                    ouch("SSL: error want connect/accept.\n");
                    break;
                case SSL_ERROR_WANT_X509_LOOKUP:
                    ouch("SSL: X509_LOOKUP: callback incomplete.\n" );
                    break;
                case SSL_ERROR_SYSCALL:
                    ouch("SSL: Syscall.\n" );
                    break;
                case SSL_ERROR_SSL:
                    dprintf(D_SECURITY, "SSL: library failure: %s\n", ERR_error_string(ERR_get_error(), NULL));
                    break;
                default:
                    ouch("SSL: unknown error?\n" );
                    break;
                }
            } else {
                m_auth_state->m_server_status = AUTH_SSL_HOLDING;
            }
            dprintf(D_SECURITY|D_VERBOSE,"Round %d.\n", m_auth_state->m_round_ctr);
            if(m_auth_state->m_round_ctr % 2 == 0) {
                auto retval = server_receive_message(non_blocking,
					m_auth_state->m_server_status, m_auth_state->m_buffer,
					m_auth_state->m_conn_in, m_auth_state->m_conn_out,
					m_auth_state->m_client_status );
				if (retval != CondorAuthSSLRetval::Success) {
					return retval == CondorAuthSSLRetval::Fail ? authenticate_fail() : retval;
				}
            } else {
                if(AUTH_SSL_ERROR == server_send_message(
                       m_auth_state->m_server_status, m_auth_state->m_buffer,
						m_auth_state->m_conn_in, m_auth_state->m_conn_out )) {
                    m_auth_state->m_client_status = AUTH_SSL_QUITTING;
                }
            }
            m_auth_state->m_round_ctr++;
            dprintf(D_SECURITY|D_VERBOSE,"Status (c: %d, s: %d)\n", m_auth_state->m_client_status,
				m_auth_state->m_server_status);            
            /*
             * client_status = server_exchange_messages( server_status,
             * buffer,
             * conn_in, conn_out );
             */
            if (m_auth_state->m_client_status == AUTH_SSL_ERROR) {
                m_auth_state->m_client_status = AUTH_SSL_QUITTING;
            }
            if( m_auth_state->m_client_status == AUTH_SSL_HOLDING
                && m_auth_state->m_server_status == AUTH_SSL_HOLDING ) {
                m_auth_state->m_done = 1;
            }
            if( m_auth_state->m_client_status == AUTH_SSL_QUITTING
                || m_auth_state->m_server_status == AUTH_SSL_QUITTING ) {
                ouch( "SSL Authentication failed\n" );
                return authenticate_fail();
            }
        }
        dprintf(D_SECURITY|D_VERBOSE, "Server trying post connection check.\n");
        if ((m_auth_state->m_err = post_connection_check(m_auth_state->m_ssl,
				AUTH_SSL_ROLE_SERVER)) != X509_V_OK) {
            ouch( "Error on check of peer certificate\n" );

            char errbuf[500];
            snprintf(errbuf, 500, "%s\n", X509_verify_cert_error_string(m_auth_state->m_err));
            ouch( errbuf );
            ouch( "Error checking SSL object after connection\n" );
            m_auth_state->m_server_status = AUTH_SSL_QUITTING;
        } else {
            m_auth_state->m_server_status = AUTH_SSL_A_OK;
        }

        if( m_auth_state->m_server_status == AUTH_SSL_QUITTING
            || m_auth_state->m_client_status == AUTH_SSL_QUITTING ) {
            ouch( "SSL Authentication failed\n" );
			// Tell the client that we're bailing
			send_message(AUTH_SSL_QUITTING, m_auth_state->m_buffer, 0);
            return authenticate_fail();
        }
        if(!RAND_bytes(m_auth_state->m_session_key, AUTH_SSL_SESSION_KEY_LEN)) {
            ouch("Couldn't generate session key.\n");
            m_auth_state->m_server_status = AUTH_SSL_QUITTING;
			// Tell the client that we're bailing
			send_message(AUTH_SSL_QUITTING, m_auth_state->m_buffer, 0);
			return authenticate_fail();
        }
        //dprintf(D_SECURITY,"Generated session key: '%s'\n", session_key);

        m_auth_state->m_client_status = m_auth_state->m_server_status = AUTH_SSL_RECEIVING;
        m_auth_state->m_done = 0;
        m_auth_state->m_round_ctr = 0;

		return authenticate_server_key(errstack, non_blocking);
}


Condor_Auth_SSL::CondorAuthSSLRetval
Condor_Auth_SSL::authenticate_server_key(CondorError *errstack, bool non_blocking)
{
		m_auth_state->m_phase = Phase::KeyExchange;
        while(!m_auth_state->m_done) {
            dprintf(D_SECURITY|D_VERBOSE,"Writing round %d.\n", m_auth_state->m_round_ctr);
            if(m_auth_state->m_round_ctr > 256) {
                ouch("Too many rounds exchanging key: quitting.\n");
                m_auth_state->m_done = 1;
                m_auth_state->m_server_status = AUTH_SSL_QUITTING;
                break;
            }
            if( m_auth_state->m_server_status != AUTH_SSL_HOLDING ) {
                m_auth_state->m_ssl_status = SSL_write_ptr(m_auth_state->m_ssl, 
									   m_auth_state->m_session_key, AUTH_SSL_SESSION_KEY_LEN);
            }
            if(m_auth_state->m_ssl_status < 1) {
                m_auth_state->m_err = SSL_get_error_ptr( m_auth_state->m_ssl,
					m_auth_state->m_ssl_status);
                switch( m_auth_state->m_err ) {
                case SSL_ERROR_WANT_READ:
                case SSL_ERROR_WANT_WRITE:
                    dprintf(D_SECURITY|D_VERBOSE, "SSL: continue read/write.\n");
                    m_auth_state->m_done = 0;
                    m_auth_state->m_server_status = AUTH_SSL_RECEIVING;
                    break;
                default:
                    m_auth_state->m_server_status = AUTH_SSL_QUITTING;
                    m_auth_state->m_done = 1;
                    ouch("SSL: error on write.  Can't proceed.\n");
                    break;
                }
            } else {
                dprintf(D_SECURITY|D_VERBOSE, "SSL write has succeeded.\n");
                if(m_auth_state->m_client_status == AUTH_SSL_HOLDING) {
                    m_auth_state->m_done = 1;
                }
                m_auth_state->m_server_status = AUTH_SSL_HOLDING;
            }
            if(m_auth_state->m_round_ctr % 2 == 0) {
                if(AUTH_SSL_ERROR == server_send_message(
                       m_auth_state->m_server_status, m_auth_state->m_buffer,
						m_auth_state->m_conn_in, m_auth_state->m_conn_out )) {
                    m_auth_state->m_client_status = AUTH_SSL_QUITTING;
                }
            } else {
                auto retval = server_receive_message(non_blocking,
                    m_auth_state->m_server_status, m_auth_state->m_buffer,
					m_auth_state->m_conn_in, m_auth_state->m_conn_out,
					m_auth_state->m_client_status );
				if (retval != CondorAuthSSLRetval::Success) {
					return retval == CondorAuthSSLRetval::Fail ? authenticate_fail() : retval;
				}
            }
			m_auth_state->m_round_ctr++;
            dprintf(D_SECURITY|D_VERBOSE, "Status: c: %d, s: %d\n", m_auth_state->m_client_status,
				m_auth_state->m_server_status);
            if(m_auth_state->m_server_status == AUTH_SSL_HOLDING
               && m_auth_state->m_client_status == AUTH_SSL_HOLDING) {
                m_auth_state->m_done = 1;
            }
            if(m_auth_state->m_client_status == AUTH_SSL_QUITTING) {
                m_auth_state->m_done = 1;
            }
        }
        if( m_auth_state->m_server_status == AUTH_SSL_QUITTING
            || m_auth_state->m_client_status == AUTH_SSL_QUITTING ) {
            ouch( "SSL Authentication failed at key exchange.\n" );
            return authenticate_fail();
        }
        setup_crypto( m_auth_state->m_session_key, AUTH_SSL_SESSION_KEY_LEN );
		if (m_scitokens_mode) {
			m_auth_state->m_client_status = m_auth_state->m_server_status = AUTH_SSL_RECEIVING;
			m_auth_state->m_done = 0;
			m_auth_state->m_round_ctr = 0;

			return authenticate_server_scitoken(errstack, non_blocking);
		}
		return authenticate_finish(errstack, non_blocking);
}


Condor_Auth_SSL::CondorAuthSSLRetval
Condor_Auth_SSL::authenticate_server_scitoken(CondorError *errstack, bool non_blocking)
{
	m_auth_state->m_phase = Phase::SciToken;
	std::vector<char> token_contents;
	while(!m_auth_state->m_done) {
		dprintf(D_SECURITY|D_VERBOSE,"Reading SciTokens round %d.\n", m_auth_state->m_round_ctr);
		if(m_auth_state->m_round_ctr > 256) {
			ouch("Too many rounds exchanging SciToken: quitting.\n");
			m_auth_state->m_done = 1;
			m_auth_state->m_server_status = AUTH_SSL_QUITTING;
			break;
		}
		if( m_auth_state->m_server_status != AUTH_SSL_HOLDING ) {
			if (m_auth_state->m_token_length == -1) {
				uint32_t token_length = 0;
				m_auth_state->m_ssl_status = SSL_peek_ptr(m_auth_state->m_ssl,
					static_cast<void*>(&token_length), sizeof(token_length));
				if (m_auth_state->m_ssl_status >= 1) {
					m_auth_state->m_token_length = ntohl(token_length);
					dprintf(D_SECURITY|D_FULLDEBUG, "Peeked at the sent token; "
						"%u bytes long; SSL status %d.\n",
						m_auth_state->m_token_length, m_auth_state->m_ssl_status);
				}
			}
			if (m_auth_state->m_token_length == 0) {
				ouch("Received zero-length scitoken: quitting.\n");
				m_auth_state->m_done = 1;
				m_auth_state->m_server_status = AUTH_SSL_QUITTING;
				break;
			}
			if (m_auth_state->m_token_length > 0) {
				token_contents.resize(m_auth_state->m_token_length + sizeof(uint32_t), 0);
				m_auth_state->m_ssl_status = SSL_read_ptr(m_auth_state->m_ssl,
					static_cast<void*>(token_contents.data()),
					m_auth_state->m_token_length + sizeof(uint32_t));
			}
		}
		if(m_auth_state->m_ssl_status < 1) {
			m_auth_state->m_err = SSL_get_error_ptr( m_auth_state->m_ssl,
				m_auth_state->m_ssl_status);
			switch( m_auth_state->m_err ) {
			case SSL_ERROR_WANT_READ:
			case SSL_ERROR_WANT_WRITE:
				dprintf(D_SECURITY|D_VERBOSE, "SciToken: continue read/write.\n");
				m_auth_state->m_done = 0;
				m_auth_state->m_server_status = AUTH_SSL_RECEIVING;
				break;
			default:
				m_auth_state->m_server_status = AUTH_SSL_QUITTING;
				m_auth_state->m_done = 1;
				dprintf(D_SECURITY, "SciToken: error on read (%ld).  Can't proceed.\n",
					m_auth_state->m_err);
				break;
			}
		} else {
			dprintf(D_SECURITY|D_VERBOSE, "SciToken SSL read is successful.\n");
			m_client_scitoken =
				std::string(&token_contents[sizeof(uint32_t)], m_auth_state->m_token_length);
			if(m_auth_state->m_client_status == AUTH_SSL_HOLDING) {
				m_auth_state->m_done = 1;
			}
			if (server_verify_scitoken(errstack)) {
				m_auth_state->m_server_status = AUTH_SSL_HOLDING;
			} else {
				m_auth_state->m_server_status = AUTH_SSL_QUITTING;
			}


			// We don't currently go back and try another authentication method
			// if authorization fails, so check for a succesful mapping here.
			//
			// We can't delay this info authentication_finish() because we
			// need to be able to tell the client that the authN failed.
			if (m_auth_state->m_server_status == AUTH_SSL_HOLDING) {
				std::string canonical_user;
				Authentication::load_map_file();
				auto global_map_file = Authentication::getGlobalMapFile();
				bool mapFailed = true;
				bool pluginsDefined = param_defined("SEC_SCITOKENS_PLUGIN_NAMES");
				if( global_map_file != NULL ) {
					mapFailed = global_map_file->GetCanonicalization( "SCITOKENS", m_scitokens_auth_name, canonical_user );
				}
				if (!global_map_file && pluginsDefined) {
					dprintf(D_SECURITY|D_VERBOSE, "No map file, but SCITOKENS plugins defined, assuming authorization will succeed\n");
				} else if( mapFailed ) {
					dprintf(D_ERROR, "Failed to map SCITOKENS authenticated identity '%s', failing authentication to give another authentication method a go.\n", m_scitokens_auth_name.c_str() );

					m_auth_state->m_server_status = AUTH_SSL_QUITTING;
				} else {
					dprintf(D_SECURITY|D_VERBOSE, "Mapped SCITOKENS authenticated identity '%s' to %s, assuming authorization will succeed.\n", m_scitokens_auth_name.c_str(), canonical_user.c_str() );
				}
			}
		}
		if(m_auth_state->m_round_ctr % 2 == 1) {
			if(AUTH_SSL_ERROR == server_send_message(
				m_auth_state->m_server_status, m_auth_state->m_buffer,
				m_auth_state->m_conn_in, m_auth_state->m_conn_out ))
			{
				m_auth_state->m_client_status = AUTH_SSL_QUITTING;
			}
		} else {
			auto retval = server_receive_message(non_blocking,
				m_auth_state->m_server_status, m_auth_state->m_buffer,
				m_auth_state->m_conn_in, m_auth_state->m_conn_out,
				m_auth_state->m_client_status );
			if (retval != CondorAuthSSLRetval::Success) {
				return retval == CondorAuthSSLRetval::Fail ? authenticate_fail() : retval;
			}
		}
		m_auth_state->m_round_ctr++;
		dprintf(D_SECURITY|D_VERBOSE, "SciToken exchange server status: c: %d, s: %d\n", m_auth_state->m_client_status,
				m_auth_state->m_server_status);
		if(m_auth_state->m_server_status == AUTH_SSL_HOLDING
			&& m_auth_state->m_client_status == AUTH_SSL_HOLDING)
		{
			m_auth_state->m_done = 1;
		}
		if(m_auth_state->m_client_status == AUTH_SSL_QUITTING) {
			m_auth_state->m_done = 1;
		}
	}
	if( m_auth_state->m_server_status == AUTH_SSL_QUITTING
		|| m_auth_state->m_client_status == AUTH_SSL_QUITTING )
	{
		ouch( "SciToken Authentication failed at token exchange.\n" );
		return authenticate_fail();
	}
	return authenticate_finish(errstack, non_blocking);
}


bool
Condor_Auth_SSL::server_verify_scitoken(CondorError* errstack)
{
	std::string issuer, subject;
	long long expiry;
	std::vector<std::string> bounding_set;
	std::vector<std::string> groups, scopes;
	std::string jti;
	if (!htcondor::validate_scitoken(m_client_scitoken, issuer, subject, expiry,
		bounding_set, groups, scopes, jti, mySock_->getUniqueId(), *errstack))
	{
		dprintf(D_SECURITY, "SCITOKENS error: %s\n", errstack->message(0));
		return false;
	}
	classad::ClassAd ad;
	if (!groups.empty()) {
		std::string ss = join(groups, ",");
		ad.InsertAttr(ATTR_TOKEN_GROUPS, ss);
	}
		// These are *not* the same as authz; the authz are filtered (and stripped)
		// for the prefix condor:/
	if (!scopes.empty()) {
		std::string ss = join(scopes, ",");
		ad.InsertAttr(ATTR_TOKEN_SCOPES, ss);
	}
	if (!jti.empty()) {
		ad.InsertAttr(ATTR_TOKEN_ID, jti);
	}
	ad.InsertAttr(ATTR_TOKEN_ISSUER, issuer);
	ad.InsertAttr(ATTR_TOKEN_SUBJECT, subject);
	if (!bounding_set.empty()) {
		std::string ss = join(bounding_set, ",");
		for (const auto &auth : bounding_set) {
			dprintf(D_SECURITY|D_FULLDEBUG,
				"Found SciToken condor authorization: %s\n",
				auth.c_str());
		}
		ad.InsertAttr(ATTR_SEC_LIMIT_AUTHORIZATION, ss);
	}
	mySock_->setPolicyAd(ad);
	m_scitokens_auth_name = issuer + "," + subject;
	return true;
}

Condor_Auth_SSL::CondorAuthSSLRetval
Condor_Auth_SSL::authenticate_finish(CondorError * /*errstack*/, bool /*non_blocking*/)
{
	Condor_Auth_SSL::CondorAuthSSLRetval retval = CondorAuthSSLRetval::Success;
	setRemoteDomain( UNMAPPED_DOMAIN );
	if (m_scitokens_mode) {
		setRemoteUser("scitokens");
		setAuthenticatedName( m_scitokens_auth_name.c_str() );
	} else {
		std::string subjectname = get_peer_identity(m_auth_state->m_ssl);
		if (subjectname.empty()) {
			setRemoteUser("unauthenticated");
			setAuthenticatedName("unauthenticated");
		} else {
			setRemoteUser("ssl");
			setAuthenticatedName(subjectname.c_str());
		}
	}

	dprintf(D_SECURITY,"SSL authentication succeeded to %s\n", getAuthenticatedName());
	m_auth_state.reset();
	return retval;
}


Condor_Auth_SSL::CondorAuthSSLRetval
Condor_Auth_SSL::authenticate_fail()
{
	m_auth_state.reset();
	return CondorAuthSSLRetval::Fail;
}


int Condor_Auth_SSL::isValid() const
{
	if ( m_crypto && m_crypto_state ) {
		return TRUE;
	} else {
		return FALSE;
	}
}

bool
Condor_Auth_SSL::setup_crypto(unsigned char* key, const int keylen)
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
Condor_Auth_SSL::encrypt(const unsigned char* input,
					int input_len, unsigned char* & output, int& output_len)
{
	return encrypt_or_decrypt(true,input,input_len,output,output_len);
}

bool
Condor_Auth_SSL::decrypt(const unsigned char* input, int input_len,
							unsigned char* & output, int& output_len)
{
	return encrypt_or_decrypt(false,input,input_len,output,output_len);
}

bool
Condor_Auth_SSL::encrypt_or_decrypt(bool want_encrypt, 
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
Condor_Auth_SSL::wrap(const char *   input,
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
Condor_Auth_SSL::unwrap(const char *   input,
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


/*
 * This works on Linux.
 */
/*int seed_pnrg(void)
{
    if (!RAND_load_file("/dev/urandom", 1024))
        return 0;
    return 1;
}*/

int Condor_Auth_SSL :: init_OpenSSL(void)
{
#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
    if (!SSL_library_init_ptr()) {
        return AUTH_SSL_ERROR;
    }
    SSL_load_error_strings_ptr();
#else
    if (!OPENSSL_init_ssl_ptr(OPENSSL_INIT_LOAD_SSL_STRINGS \
                               | OPENSSL_INIT_LOAD_CRYPTO_STRINGS, NULL)) {
        return AUTH_SSL_ERROR;
    }
#endif
    // seed_pnrg(); TODO: 
    return AUTH_SSL_A_OK;
}


std::string get_x509_encoded(X509 *cert)
{
	std::unique_ptr<BIO, decltype(&BIO_free)> b64( BIO_new(BIO_f_base64()), &BIO_free);
	BIO_set_flags(b64.get(), BIO_FLAGS_BASE64_NO_NL);
	if (!b64.get()) {return "";}

	decltype(b64) mem(BIO_new(BIO_s_mem()), &BIO_free);
	if (!mem.get()) {return "";}
	BIO_push(b64.get(), mem.get());

	if (1 != i2d_X509_bio(b64.get(), cert)) {
		dprintf(D_SECURITY, "Failed to base64 encode certificate.\n");
		return "";
	}
	// Ensure that the final base64 block is written to underlying memory.
	BIO_flush(b64.get());

	char* dt;
	auto len = BIO_get_mem_data(mem.get(), &dt);

	return std::string(dt, len);
}


int verify_callback(int ok, X509_STORE_CTX *store)
{
    char data[256];
 
    if (!ok) {
        X509 *cert = X509_STORE_CTX_get_current_cert(store);
        int  depth = X509_STORE_CTX_get_error_depth(store);
        int  err = X509_STORE_CTX_get_error(store);

        dprintf( D_SECURITY, "-Error with certificate at depth: %i\n", depth );
        X509_NAME_oneline( X509_get_issuer_name( cert ), data, 256 );
        dprintf( D_SECURITY, "  issuer   = %s\n", data );
        X509_NAME_oneline( X509_get_subject_name( cert ), data, 256 );
	std::string dn(data);
        dprintf( D_SECURITY, "  subject  = %s\n", data );
        dprintf( D_SECURITY, "  err %i:%s\n", err, X509_verify_cert_error_string( err ) );

		const SSL* ssl = (const SSL*)X509_STORE_CTX_get_ex_data(store, (*SSL_get_ex_data_X509_STORE_CTX_idx_ptr)());
		Condor_Auth_SSL::LastVerifyError *verify_ptr = (Condor_Auth_SSL::LastVerifyError *)(g_last_verify_error_index >= 0 ? SSL_get_ex_data_ptr(ssl, g_last_verify_error_index) : nullptr);
		if (verify_ptr) verify_ptr->m_skip_error = 0;

		if (verify_ptr && ((err == X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT) ||
			(err == X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN) ||
			(err == X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY) ||
			(err == X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE) ||
			(err == X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT)))
		{
			bool is_permitted;
			std::string method;
			std::string method_info;
			auto encoded_cert = get_x509_encoded(cert);
			bool is_ca_cert = (err == X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY) || (err == X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT) ||
				(err == X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN);

			const auto &host_alias = *(verify_ptr->m_host_alias);
			if (!encoded_cert.empty() &&
				htcondor::get_known_hosts_first_match(host_alias, is_permitted,
					method, method_info))
			{
				if (is_permitted && method == "SSL")
				{
					if (method_info == encoded_cert) {
						dprintf(D_SECURITY, "Skipping validation error as this is a known host.\n");
						verify_ptr->m_skip_error = err;
						verify_ptr->m_used_known_host = true;
						return 1;
					} else {
						// We aren't going to accept this - there's an earlier entry for this host;
						// however, we want to record that we saw it.
						dprintf(D_SECURITY, "Recording the SSL certificate in the known_hosts file.\n");
						htcondor::add_known_hosts(host_alias, false, "SSL", encoded_cert);
					}
				}
			} else if (!encoded_cert.empty()) {
				bool permitted = param_boolean("BOOTSTRAP_SSL_SERVER_TRUST", false);
				bool prompt_user = param_boolean("BOOTSTRAP_SSL_SERVER_TRUST_PROMPT_USER", true);
				dprintf(D_SECURITY, "Adding remote host as known host with trust set to %s"
					".\n", permitted ? "on" : "off");
				// Provide an opportunity for users to manually confirm the SSL fingerprint.
				if (!permitted && prompt_user && (get_mySubSystem()->isType(SUBSYSTEM_TYPE_TOOL) ||
					get_mySubSystem()->isType(SUBSYSTEM_TYPE_SUBMIT)) && isatty(0))
				{
					unsigned char md[EVP_MAX_MD_SIZE];
					auto digest = EVP_get_digestbyname("sha256");
					unsigned int len;
					if (1 != X509_digest(cert, digest, md, &len)) {
						dprintf(D_SECURITY, "Failed to create a digest of the provided X.509 certificate.\n");
						return ok;
					}
					std::stringstream ss;
					ss << std::hex << std::setw(2) << std::setfill('0');
					bool first = true;
					for (unsigned idx = 0; idx < len; idx++) {
						if (!first) ss << ":";
						ss << std::setw(2) << static_cast<int>(md[idx]);
						first = false;
					}
					std::string fingerprint = ss.str();

					permitted = htcondor::ask_cert_confirmation(host_alias, fingerprint, dn, is_ca_cert);
				}
				htcondor::add_known_hosts(host_alias, permitted, "SSL", encoded_cert);
				std::string method;
				if (permitted && htcondor::get_known_hosts_first_match(host_alias,
					permitted, method, encoded_cert) && method == "SSL")
				{
					dprintf(D_ALWAYS, "Skipping validation error as this is a known host.\n");
					verify_ptr->m_skip_error = err;
					verify_ptr->m_used_known_host = true;
					return 1;
				}
			}
		}
    }
 
    return ok;
}

int Condor_Auth_SSL :: send_status( int status )
{
    mySock_ ->encode( );
    if( !(mySock_ ->code( status ))
        || !(mySock_ ->end_of_message( )) ) {
        ouch( "Error communicating status\n" );
        return AUTH_SSL_ERROR;
    }
    return AUTH_SSL_A_OK;
}

Condor_Auth_SSL::CondorAuthSSLRetval
Condor_Auth_SSL :: receive_status( bool non_blocking, int &status )
{
	if (non_blocking && !mySock_->readReady()) {
		return CondorAuthSSLRetval::WouldBlock;
	}
    mySock_ ->decode( );
    if( !(mySock_ ->code( status ))
        || !(mySock_ ->end_of_message( )) ) {
        ouch( "Error communicating status\n" );
        return CondorAuthSSLRetval::Fail;
    }
    return CondorAuthSSLRetval::Success;
}
    

int Condor_Auth_SSL :: client_share_status( int client_status )
{
    int server_status;
	CondorAuthSSLRetval retval = receive_status( false,  server_status );
    if( retval != CondorAuthSSLRetval::Success ) {
        return static_cast<int>(retval);
    }
    if( send_status( client_status ) == AUTH_SSL_ERROR ) {
        return AUTH_SSL_ERROR;
    }
    return server_status;
}


int Condor_Auth_SSL :: send_message( int status, char *buf, int len )
{

    char *send = buf;

    dprintf(D_SECURITY|D_VERBOSE, "Send message (%d).\n", status );
    mySock_ ->encode( );
    if( !(mySock_ ->code( status ))
        || !(mySock_ ->code( len ))
        || !(len == (mySock_ ->put_bytes( send, len )))
        || !(mySock_ ->end_of_message( )) ) {
        ouch( "Error communicating with peer.\n" );
        return AUTH_SSL_ERROR;
    }
    return AUTH_SSL_A_OK;
}

Condor_Auth_SSL::CondorAuthSSLRetval
Condor_Auth_SSL :: receive_message( bool non_blocking, int &status, int &len, char *buf )
{
	if (non_blocking && !mySock_->readReady()) {
		dprintf(D_SECURITY|D_VERBOSE, "SSL Auth: Would block when trying to receive message\n");
		return CondorAuthSSLRetval::WouldBlock;
	}
    dprintf(D_SECURITY|D_VERBOSE, "SSL Auth: Receive message.\n");
    mySock_ ->decode( );
    if( !(mySock_ ->code( status ))
        || !(mySock_ ->code( len ))
        || !(len <= AUTH_SSL_BUF_SIZE)
        || !(len == (mySock_ ->get_bytes( buf, len )))
        || !(mySock_ ->end_of_message( )) ) {
        ouch( "Error communicating with peer.\n" );
        return CondorAuthSSLRetval::Fail;
    }
    dprintf(D_SECURITY|D_VERBOSE, "Received message (%d).\n", status );
    return CondorAuthSSLRetval::Success;
}

Condor_Auth_SSL::CondorAuthSSLRetval
Condor_Auth_SSL :: server_receive_message( bool non_blocking, int /* server_status */,
	char *buf, BIO *conn_in, BIO * /* conn_out */, int &client_status)
{
    int len;
    int rv;
    int written;
	auto retval = receive_message( non_blocking, client_status, len, buf );
    if( retval != CondorAuthSSLRetval::Success ) {
        return retval;
    }
//    if( client_status == AUTH_SSL_SENDING) {
        if( len > 0 ) {
            written = 0;
            while( written < len ) {
                rv =  BIO_write( conn_in, buf, len );
                if( rv <= 0 ) {
                    ouch( "Couldn't write connection data into bio\n" );
                    return CondorAuthSSLRetval::Fail;
                    break;
                }
                written += rv;
            }
        }
//    }
    return CondorAuthSSLRetval::Success;
}
int Condor_Auth_SSL :: server_send_message( int server_status, char *buf, BIO * /* conn_in */, BIO *conn_out )
{
    int len;
    // Read from server's conn_out into buffer to send to client.
    buf[0] = 0; // just in case we don't read anything
    len = BIO_read( conn_out, buf, AUTH_SSL_BUF_SIZE );
    if (len < 0) {
        len = 0;
    }
    if( send_message( server_status, buf, len ) == AUTH_SSL_ERROR ) {
        return AUTH_SSL_ERROR;
    }
    return AUTH_SSL_A_OK;
}


Condor_Auth_SSL::CondorAuthSSLRetval
Condor_Auth_SSL :: server_exchange_messages( bool non_blocking, int server_status, char *buf,
	BIO *conn_in, BIO *conn_out, int &client_status )
{
    dprintf(D_SECURITY|D_VERBOSE, "SSL Auth: Server exchange messages.\n");
    if(server_send_message( server_status, buf, conn_in, conn_out )
       == AUTH_SSL_ERROR ) {
        return CondorAuthSSLRetval::Fail;
    }
    return server_receive_message( non_blocking, server_status, buf, conn_in, conn_out,
		client_status );
}

int Condor_Auth_SSL :: client_send_message( int client_status, char *buf, BIO * /* conn_in */, BIO *conn_out )
{
    int len = 0;
    buf[0] = 0; // just in case we don't read anything
    len = BIO_read( conn_out, buf, AUTH_SSL_BUF_SIZE );
    if(len < 0) {
        len = 0;
    }
    if( send_message( client_status, buf, len ) == AUTH_SSL_ERROR ) {
        return AUTH_SSL_ERROR;
    }
    return AUTH_SSL_A_OK;
}
int Condor_Auth_SSL :: client_receive_message( int /* client_status */, char *buf, BIO *conn_in, BIO * /* conn_out */ )
{
    int server_status;
    int len = 0;
    int rv;
    int written;
	auto retval = receive_message( false, server_status, len, buf );
    if( retval != CondorAuthSSLRetval::Success ) {
        return static_cast<int>(retval);
    }

    if( len > 0 ) {
        written = 0;
        while( written < len ) {
            rv =  BIO_write( conn_in, buf, len );
            if( rv <= 0 ) {
                ouch( "Couldn't write connection data into bio\n" );
                return AUTH_SSL_ERROR;
                break;
            }
            written += rv;
        }
    }
    return server_status;   
}
int Condor_Auth_SSL :: client_exchange_messages( int client_status, char *buf, BIO *conn_in, BIO *conn_out )
{
    int server_status = AUTH_SSL_ERROR;
    dprintf(D_SECURITY|D_VERBOSE, "SSL Auth: Client exchange messages.\n");
    if(( server_status = client_receive_message(
             client_status, buf, conn_in, conn_out ))
       == AUTH_SSL_ERROR ) {
        return AUTH_SSL_ERROR;
    }
    if( client_send_message(
            client_status, buf, conn_in, conn_out )
        == AUTH_SSL_ERROR) {
        return AUTH_SSL_ERROR;
    }
    return server_status;
}


long Condor_Auth_SSL :: post_connection_check(SSL *ssl, int role )
{
	X509      *cert;
	std::string fqdn;
	bool success = false;
	ouch("post_connection_check.\n");

 
    /* Checking the return from SSL_get_peer_certificate here is not
     * strictly necessary.  With our example programs, it is not
     * possible for it to return NULL.  However, it is good form to
     * check the return since it can return NULL if the examples are
     * modified to enable anonymous ciphers or for the server to not
     * require a client certificate.
     * (Comment from book.  -Ian)
     */
	cert = SSL_get_peer_certificate_ptr(ssl);
	if( cert == NULL ) {
		if (mySock_->isClient()) {
			dprintf(D_SECURITY,"SSL_get_peer_certificate returned null.\n" );
			goto err_occured;
		} else if (!m_scitokens_mode && param_boolean("AUTH_SSL_REQUIRE_CLIENT_CERTIFICATE", false)) {
			dprintf(D_SECURITY,"SSL Auth: Anonymous client is not allowed.\n");
			goto err_occured;
		} else {
			dprintf(D_SECURITY, "SSL Auth: Anonymous client is allowed; not checking.\n");
			return X509_V_OK;
		}
	}
	dprintf(D_SECURITY|D_VERBOSE, "SSL_get_peer_certificate returned data.\n" );

    /* What follows is working code based on examples from the book
       "Network Programming with OpenSSL."  The point of this code
       is to determine that the certificate is a valid host certificate
       for the server side.  It's more complicated than simply extracting
       the CN and comparing it to the host name.

       -Ian
     */
       
	if(role == AUTH_SSL_ROLE_SERVER) {
		X509_free( cert );
		long rc = SSL_get_verify_result_ptr( ssl );
		if (rc == X509_V_OK && param_boolean("AUTH_SSL_REQUIRE_CLIENT_MAPPING", false)) {
			std::string peer_dn = get_peer_identity(m_auth_state->m_ssl);
			if (peer_dn.empty()) {
				dprintf(D_SECURITY, "Client has no SSL authenticated identity, failing authentication to give another authentication method a go.\n");
				return X509_V_ERR_APPLICATION_VERIFICATION;
			}
			std::string canonical_user;
			Authentication::load_map_file();
			auto global_map_file = Authentication::getGlobalMapFile();
			bool mapFailed = true;
			if (global_map_file != nullptr) {
				mapFailed = global_map_file->GetCanonicalization("SSL", peer_dn, canonical_user);
			}
			if (mapFailed) {
				dprintf(D_SECURITY, "Failed to map SSL authenticated identity '%s', failing authentication to give another authentication method a go.\n", peer_dn.c_str());
				return X509_V_ERR_APPLICATION_VERIFICATION;
			}
		}
		ouch("Server role: returning from post connection check.\n");

		return rc;
	} // else ROLE_CLIENT: check dns (arg 2) against CN and the SAN

	if (param_boolean("SSL_SKIP_HOST_CHECK", false)) {
		success = true;
		goto success;
	}

	// Client must know what host it is trying to talk to in order for us to verify the SAN / CN.
	fqdn = m_host_alias;
	if (fqdn.empty()) {
		dprintf(D_SECURITY, "No SSL host name specified.\n");
		goto err_occured;
	}
	{
			// First, look at the SAN; RFC 1035 limits each FQDN to 255 characters.
		char san_fqdn[256];

		auto gens = static_cast<GENERAL_NAMES *>(X509_get_ext_d2i(cert,
		NID_subject_alt_name, NULL, NULL));
		if (!gens) goto skip_san;

		for (int idx = 0; idx < sk_GENERAL_NAME_num(gens); idx++) {
			auto gen = sk_GENERAL_NAME_value(gens, idx);
			// We only consider alt names of type DNS
			if (gen->type != GEN_DNS) continue;
			auto cstr = gen->d.dNSName;
				// DNS names must be an IA5 string.
			if (ASN1_STRING_type(cstr) != V_ASN1_IA5STRING) continue;
			int san_fqdn_len = ASN1_STRING_length(cstr);
				// Suspiciously long FQDN; skip.
			if (san_fqdn_len > 255) continue;
				// ASN1_STRING_data is deprecated in newer SSLs due to an incorrect signature.
#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
			memcpy(san_fqdn, ASN1_STRING_data(cstr), san_fqdn_len);
#else
			memcpy(san_fqdn, ASN1_STRING_get0_data(cstr), san_fqdn_len);
#endif
			san_fqdn[san_fqdn_len] = '\0';
			if (strlen(san_fqdn) != static_cast<size_t>(san_fqdn_len)) // Avoid embedded null's.
				continue;
			if (hostname_match(san_fqdn, fqdn.c_str())) {
				dprintf(D_SECURITY, "SSL host check: host alias %s matches certificate SAN %s.\n",
					fqdn.c_str(), san_fqdn);
				success = true;
				break;
			} else {
				dprintf(D_SECURITY|D_FULLDEBUG,
					"SSL host check: host alias %s DOES NOT match certificate SAN %s.\n",
					fqdn.c_str(), san_fqdn);
			}
		}
		sk_GENERAL_NAME_pop_free(gens, GENERAL_NAME_free);
		if (!success) {
			dprintf(D_SECURITY|D_FULLDEBUG, "Certificate subjectAltName does not match hostname %s.\n",
				fqdn.c_str());
		}
	}

skip_san:
	{
		char cn_fqdn[256];
		X509_NAME *subj = nullptr;
		if (!success && (subj = X509_get_subject_name(cert)) &&
			X509_NAME_get_text_by_NID(subj, NID_commonName, cn_fqdn, 256) > 0)
		{
		        cn_fqdn[255] = '\0';
			dprintf(D_SECURITY|D_FULLDEBUG, "Common Name: '%s'; host: '%s'\n", cn_fqdn, fqdn.c_str());
			if (strcasecmp(cn_fqdn, fqdn.c_str()) != 0) {
				dprintf(D_SECURITY, "Certificate common name (CN), %s, does not match host %s.\n",
					cn_fqdn, fqdn.c_str());
				goto err_occured;
			} else {
				success = true;
			}
		} else if (!success) {
			dprintf(D_SECURITY|D_FULLDEBUG, "Unable to extract CN from certificate.\n");
			goto err_occured;
		}
	}

	if (mySock_->isClient()) {
		std::unique_ptr<BIO, decltype(&BIO_free)> bio(BIO_new( BIO_s_mem() ), BIO_free);

		if (!PEM_write_bio_X509(bio.get(), cert)) {
			dprintf(D_SECURITY, "Unable to convert server host cert to PEM format.\n");
			goto err_occured;
		}
		char *pem_raw;
		auto len = BIO_get_mem_data(bio.get(), &pem_raw);
		if (len) {
			classad::ClassAd ad;
			ad.InsertAttr(ATTR_SERVER_PUBLIC_CERT, pem_raw, len);
			mySock_->setPolicyAd(ad);
		}
	}

success:
	if (success) {
		dprintf(D_SECURITY|D_VERBOSE, "SSL Auth: Server checks out; returning SSL_get_verify_result.\n");
    
		X509_free(cert);

		auto verify_result = SSL_get_verify_result_ptr( ssl );

		if ((verify_result == X509_V_OK) && mySock_->isClient() && !m_host_alias.empty() &&
			!m_last_verify_error.m_used_known_host)
		{
			htcondor::add_known_hosts(m_host_alias, true, "SSL", "@trusted");
		}

		if (m_last_verify_error.m_skip_error == verify_result) return X509_V_OK;
		return verify_result;
	}

err_occured:
    if (cert)
        X509_free(cert);
    return X509_V_ERR_APPLICATION_VERIFICATION;
}

std::string Condor_Auth_SSL::get_peer_identity(SSL *ssl)
{
	char subjectname[1024] = "";
	X509 *peer = SSL_get_peer_certificate_ptr(ssl);
	if (peer) {
		BASIC_CONSTRAINTS *bs = nullptr;
		PROXY_CERT_INFO_EXTENSION *pci = (PROXY_CERT_INFO_EXTENSION *)X509_get_ext_d2i(peer, NID_proxyCertInfo, NULL, NULL);
		if (pci) {
			PROXY_CERT_INFO_EXTENSION_free(pci);
			STACK_OF(X509)* chain = nullptr;
#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
			chain = SSL_get_peer_cert_chain_ptr(ssl);
#else
			chain = SSL_get0_verified_chain_ptr(ssl);
#endif
			for (int n = 0; n < sk_X509_num(chain); n++) {
				X509* cert = sk_X509_value(chain, n);
				bs = (BASIC_CONSTRAINTS *)X509_get_ext_d2i(cert, NID_basic_constraints, NULL, NULL);
				pci = (PROXY_CERT_INFO_EXTENSION *)X509_get_ext_d2i(cert, NID_proxyCertInfo, NULL, NULL);
				if (!pci && !(bs && bs->ca)) {
					X509_NAME_oneline(X509_get_subject_name(cert), subjectname, sizeof(subjectname));
				}
				if (bs) {
					BASIC_CONSTRAINTS_free(bs);
				}
				if (pci) {
					PROXY_CERT_INFO_EXTENSION_free(pci);
				}
			}
			char* voms_fqan = nullptr;
			if (param_boolean("USE_VOMS_ATTRIBUTES", false) && param_boolean("AUTH_SSL_USE_VOMS_IDENTITY", true)) {
				int voms_err = extract_VOMS_info(peer, chain, 1, nullptr, nullptr, &voms_fqan);
				if (voms_err) {
					dprintf(D_SECURITY|D_FULLDEBUG, "VOMS FQAN not present (error %d), ignoring.\n", voms_err);
				}
			}
			if (voms_fqan) {
				strncpy(subjectname, voms_fqan, sizeof(subjectname));
				subjectname[sizeof(subjectname)-1] = '\0';
				free(voms_fqan);
				dprintf(D_SECURITY, "AUTHENTICATE: Peer's certificate is a proxy with VOMS attributes. Using identity '%s'\n", subjectname);
			} else {
				dprintf(D_SECURITY, "AUTHENTICATE: Peer's certificate is a proxy. Using identity '%s'\n", subjectname);
			}
		} else {
			X509_NAME_oneline(X509_get_subject_name(peer), subjectname, sizeof(subjectname));
		}
		X509_free(peer);
	}
	return subjectname;
}

SSL_CTX *Condor_Auth_SSL :: setup_ssl_ctx( bool is_server )
{
    SSL_CTX *ctx       = NULL;
    char *cafile       = NULL;
    char *cadir        = NULL;
    char *certfile     = NULL;
    char *keyfile      = NULL;
    char *cipherlist   = NULL;
    X509_VERIFY_PARAM *verify_param = nullptr;
    bool use_default_cas = true;
    bool i_need_cert   = is_server;
    bool allow_peer_proxy = false;
    const char *cafile_preferred = nullptr;
    std::string cafile_preferred_str;

		// Ensure the verification state is reset.
	m_last_verify_error.m_used_known_host = false;
	m_last_verify_error.m_skip_error = -1;
	m_last_verify_error.m_host_alias = &m_host_alias;

		// Not sure where we want to get these things from but this
		// will do for now.
	if( is_server ) {
		cafile     = param( AUTH_SSL_SERVER_CAFILE_STR );
		cadir      = param( AUTH_SSL_SERVER_CADIR_STR );
		certfile   = param( AUTH_SSL_SERVER_CERTFILE_STR );
		keyfile    = param( AUTH_SSL_SERVER_KEYFILE_STR );
		use_default_cas = param_boolean("AUTH_SSL_SERVER_USE_DEFAULT_CAS", true);
		allow_peer_proxy = param_boolean("AUTH_SSL_ALLOW_CLIENT_PROXY", false);
	} else {
		cafile     = param( AUTH_SSL_CLIENT_CAFILE_STR );
		cadir      = param( AUTH_SSL_CLIENT_CADIR_STR );
		if (m_scitokens_mode) {
			param( m_scitokens_file, "SCITOKENS_FILE" );
			// Only load the provided cert if we're not overriding the
			// default credential.  All non-default credential owners
			// will auth anonymously.
		} else if (SecMan::getTagCredentialOwner().empty()) {
			i_need_cert = param_boolean("AUTH_SSL_REQUIRE_CLIENT_CERTIFICATE", false);
			const char* proxy_path=nullptr;
			if (param_boolean("AUTH_SSL_USE_CLIENT_PROXY_ENV_VAR", false) && (proxy_path=getenv("X509_USER_PROXY"))) {
				certfile   = strdup(proxy_path);
				keyfile    = strdup(proxy_path);
			} else {
				certfile   = param( AUTH_SSL_CLIENT_CERTFILE_STR );
				keyfile    = param( AUTH_SSL_CLIENT_KEYFILE_STR );
			}
		}
		use_default_cas = param_boolean("AUTH_SSL_CLIENT_USE_DEFAULT_CAS", true);
	}		
	cipherlist = param( AUTH_SSL_CIPHERLIST_STR );
    if( cipherlist == NULL ) {
		cipherlist = strdup(AUTH_SSL_DEFAULT_CIPHERLIST);
    }
    if( i_need_cert && (!certfile || !keyfile) ) {
        ouch( "Please specify path to local certificate and key\n" );
        dprintf(D_SECURITY, "in config file : '%s' and '%s'.\n",
                is_server ? AUTH_SSL_SERVER_CERTFILE_STR : AUTH_SSL_CLIENT_CERTFILE_STR,
                is_server ? AUTH_SSL_SERVER_KEYFILE_STR : AUTH_SSL_CLIENT_KEYFILE_STR);
		ctx = NULL;
        goto setup_server_ctx_err;
    }
    if(cafile)     dprintf( D_SECURITY, "CAFILE:     '%s'\n", cafile     );
    if(cadir)      dprintf( D_SECURITY, "CADIR:      '%s'\n", cadir      );
    if(certfile)   dprintf( D_SECURITY, "CERTFILE:   '%s'\n", certfile   );
    if(keyfile)    dprintf( D_SECURITY, "KEYFILE:    '%s'\n", keyfile    );
    if(cipherlist) dprintf( D_SECURITY, "CIPHERLIST: '%s'\n", cipherlist );
    if(is_server)  dprintf( D_SECURITY, "ALLOW_PROXY: %d\n", (int)allow_peer_proxy );
    if (!m_scitokens_file.empty())
	dprintf( D_SECURITY, "SCITOKENSFILE:   '%s'\n", m_scitokens_file.c_str()   );
        
    ctx = SSL_CTX_new_ptr( (*SSL_method_ptr)(  ) );
	if(!ctx) {
		ouch( "Error creating new SSL context.\n");
		goto setup_server_ctx_err;
	}

#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
	SSL_CTX_ctrl_ptr( ctx, SSL_CTRL_OPTIONS, SSL_OP_NO_SSLv2, NULL );
	SSL_CTX_ctrl_ptr( ctx, SSL_CTRL_OPTIONS, SSL_OP_NO_SSLv3, NULL );
#if OPENSSL_VERSION_NUMBER >= 0x10000000L
	SSL_CTX_ctrl_ptr( ctx, SSL_CTRL_OPTIONS, SSL_OP_NO_TLSv1, NULL );
	SSL_CTX_ctrl_ptr( ctx, SSL_CTRL_OPTIONS, SSL_OP_NO_TLSv1_1, NULL );
#endif
#else
	SSL_CTX_set_options_ptr( ctx, SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1);
#endif

	if (allow_peer_proxy) {
		verify_param = X509_VERIFY_PARAM_new();
		if (!verify_param ||
			X509_VERIFY_PARAM_set_flags(verify_param, X509_V_FLAG_ALLOW_PROXY_CERTS) != 1 ||
			SSL_CTX_set1_param_ptr(ctx, verify_param) != 1)
		{
			ouch("Error configuring X509_VERIFY_PARAM\n");
			goto setup_server_ctx_err;
		}
	}

	// Only load the verify locations if they are explicitly specified;
	// otherwise, we will use the system default.

    if (cafile) {
        for (const auto& ca_str : StringTokenIterator(cafile, ",")) {
            int fd = open(ca_str.c_str(), O_RDONLY);
            if (fd < 0) {
                continue;
            }
            close(fd);
            cafile_preferred_str = ca_str;
            cafile_preferred = cafile_preferred_str.c_str();
        }
    }
    if( (cafile_preferred || cadir) && SSL_CTX_load_verify_locations_ptr( ctx, cafile_preferred, cadir ) != 1 ) {
        auto error_number = ERR_get_error();
        auto error_string = error_number ? ERR_error_string(error_number, nullptr) : "Unknown error";
        dprintf(D_SECURITY, "SSL Auth: Error loading CA file (%s) and/or directory (%s): %s\n",
            cafile_preferred, cadir, error_string);
        goto setup_server_ctx_err;
    }
    if (use_default_cas && SSL_CTX_set_default_verify_paths_ptr(ctx) != 1) {
        auto error_number = ERR_get_error();
        auto error_string = error_number ? ERR_error_string(error_number, nullptr) : "Unknown error";
        dprintf(D_SECURITY, "SSL Auth: Error loading default CA files: %s\n", error_string);
        goto setup_server_ctx_err;
    }
    {
        StringTokenIterator certfile_list(certfile ? certfile : "", ",");
        StringTokenIterator keyfile_list(keyfile ? keyfile : "", ",");
        const char *cert, *key;
        while ((cert = certfile_list.next()))
        {
            if ((key = keyfile_list.next()) == nullptr) {
                break;
            }
            TemporaryPrivSentry sentry(PRIV_ROOT);
            int fd = open(cert, O_RDONLY);
            if (fd < 0) {
                continue;
            }
            close(fd);
            fd = open(key, O_RDONLY);
            if (fd < 0) {
                continue;
            }
            close(fd);

            if( SSL_CTX_use_certificate_chain_file_ptr( ctx, cert ) != 1 ) {
                ouch( "Error loading certificate from file\n" );
                goto setup_server_ctx_err;
            }
            if( SSL_CTX_use_PrivateKey_file_ptr( ctx, key, SSL_FILETYPE_PEM) != 1 ) {
                ouch( "Error loading private key from file\n" );
                goto setup_server_ctx_err;
            }
        }
    }
	if (g_last_verify_error_index < 0)
		g_last_verify_error_index = CRYPTO_get_ex_new_index(CRYPTO_EX_INDEX_SSL, 0, const_cast<char *>("last verify error"), nullptr, nullptr, nullptr);

    (*SSL_CTX_set_verify_ptr)( ctx, SSL_VERIFY_PEER, verify_callback ); 
    if(SSL_CTX_set_cipher_list_ptr( ctx, cipherlist ) != 1 ) {
        ouch( "Error setting cipher list (no valid ciphers)\n" );
        goto setup_server_ctx_err;
    }

#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
		// Enable the automatic ephemeral ECDH setup.
		// This is automatic in OpenSSL 1.1.0+ and this value has been
		// removed there.
	SSL_CTX_ctrl_ptr( ctx, SSL_CTRL_SET_ECDH_AUTO, 1, NULL );
#endif

    if(cafile)          free(cafile);
    if(cadir)           free(cadir);
    if(certfile)        free(certfile);
    if(keyfile)         free(keyfile);
    if(cipherlist)      free(cipherlist);
    if(verify_param)    X509_VERIFY_PARAM_free(verify_param);
    return ctx;
  setup_server_ctx_err:
    if(cafile)          free(cafile);
    if(cadir)           free(cadir);
    if(certfile)        free(certfile);
    if(keyfile)         free(keyfile);
    if(cipherlist)      free(cipherlist);
    if(verify_param)    X509_VERIFY_PARAM_free(verify_param);
	if(ctx)		        SSL_CTX_free_ptr(ctx);
    return NULL;
}


bool
Condor_Auth_SSL::should_try_auth()
{
	if (!m_should_search_for_cert) {
		return m_cert_avail;
	}
	m_should_search_for_cert = false;
	m_cert_avail = false;

	std::string certfile, keyfile;
	if (!param(certfile, AUTH_SSL_SERVER_CERTFILE_STR)) {
		dprintf(D_SECURITY, "Not trying SSL auth because server certificate"
			" parameter (%s) is not set.\n", AUTH_SSL_SERVER_CERTFILE_STR);
		return false;
	}
	if (!param(keyfile, AUTH_SSL_SERVER_KEYFILE_STR)) {
		dprintf(D_SECURITY, "Not trying SSL auth because server key"
			" parameter (%s) is not set.\n", AUTH_SSL_SERVER_KEYFILE_STR);
		return false;
	}

	StringTokenIterator certfile_list(certfile, ",");
	StringTokenIterator keyfile_list(keyfile, ",");

	std::string last_error;
	for (const auto &[cert, key]: c9::zip(certfile_list, keyfile_list)) {
		TemporaryPrivSentry sentry(PRIV_ROOT);
		int fd = open(cert.c_str(), O_RDONLY);
		if (fd < 0) {
			formatstr(last_error, "Not trying SSL auth because server certificate"
				" (%s) is not readable by HTCondor: %s.\n", cert.c_str(), strerror(errno));
			continue;
		}
		close(fd);
		fd = open(key.c_str(), O_RDONLY);
		if (fd < 0) {
			formatstr(last_error, "Not trying SSL auth because server key"
			          " (%s) is not readable by HTCondor: %s.\n", key.c_str(), strerror(errno));
			continue;
		}
		close(fd);
		m_cert_avail = true;
		return true;
	}
	dprintf(D_SECURITY, "%s", last_error.c_str());
	return false;
}

int
Condor_Auth_SSL::StartScitokensPlugins(const std::string& input, std::string& result, CondorError* errstack)
{
	// TODO JEF should some of these be treated as errors?
	if (!m_scitokens_mode || m_client_scitoken.empty() || getRemoteUser() == nullptr) {
		m_pluginResult.clear();
		m_pluginRC = 1;
		return 1;
	}

	ASSERT(daemonCore);

	if (m_pluginReaperId == -1) {
		m_pluginReaperId = daemonCore->Register_Reaper(
				"Condor_Auth_SSL::PluginReaper()",
				&Condor_Auth_SSL::PluginReaper,
				"Condor_Auth_SSL::PluginReaper()");
	}

		// TODO Maybe be less harsh here?
	ASSERT(!m_pluginState);
	ASSERT(m_pluginRC != 2);

	m_pluginResult.clear();
	m_pluginErrstack.clear();
	m_pluginState.reset(new PluginState);

	if (input == "*") {
		// Use all plugins named in SEC_SCITOKENS_PLUGIN_NAMES
		std::string names;
		if (!param(names, "SEC_SCITOKENS_PLUGIN_NAMES") || names.empty()) {
			dprintf(D_ALWAYS, "SEC_SCITOKENS_PLUGIN_NAMES isn't defined\n");
			m_pluginState.reset();
			m_pluginRC = 1;
			return 1;
		}
		StringTokenIterator toker(names);
		const std::string* next;
		while ((next = toker.next_string())) {
			m_pluginState->m_names.push_back(*next);
		}
	} else {
		// TODO compare names to SEC_SCITOKENS_PLUGIN_NAMES?
		StringTokenIterator toker(input, ",");
		const std::string* next;
		while ((next = toker.next_string())) {
			m_pluginState->m_names.push_back(*next);
		}
	}

	auto jwt = jwt::decode(m_client_scitoken);
	m_pluginState->m_stdin = jwt.get_payload();

		// Merge current environment?
		// iterate over jwt
		// set BEARER_TOKEN_* variables in environment
	std::string token_value;
	std::string env_name;
	token_value = jwt.get_issuer();
	m_pluginState->m_env.SetEnv("BEARER_TOKEN_0_ISSUER", token_value);
	if (jwt.has_subject()) {
		token_value = jwt.get_subject();
		m_pluginState->m_env.SetEnv("BEARER_TOKEN_0_SUBJECT", token_value);
	}
	auto claims = jwt.get_payload_claims();
	for (const auto &pair : claims) {
		switch (pair.second.get_type()) {
		case jwt::json::type::string:
			if (pair.first == "iss") {
				m_pluginState->m_env.SetEnv("BEARER_TOKEN_0_ISSUER", pair.second.as_string());
			} else if (pair.first == "sub") {
				m_pluginState->m_env.SetEnv("BEARER_TOKEN_0_SUBJECT", pair.second.as_string());
			} else if (pair.first == "aud") {
				m_pluginState->m_env.SetEnv("BEARER_TOKEN_0_AUDIENCE", pair.second.as_string());
			} else if (pair.first == "scope") {
				// StringTokenIterator doesn't copy its input, so we need
				// to ensure the value is valid throughout the iteration
				// process.
				std::string value = pair.second.as_string();
				StringTokenIterator toker(value, " ");
				int idx = 0;
				const std::string* next;
				while ((next = toker.next_string())) {
					formatstr(env_name, "BEARER_TOKEN_0_SCOPE_%d", idx);
					m_pluginState->m_env.SetEnv(env_name, *next);
					idx++;
				}
			}
			formatstr(env_name, "BEARER_TOKEN_0_CLAIM_%s_0", pair.first.c_str());
			m_pluginState->m_env.SetEnv(env_name, pair.second.as_string());
			break;
		case jwt::json::type::array: {
			int idx = 0;
			bool is_groups = (pair.first == "wlcg.groups");
			const picojson::array& values = pair.second.as_array();
			for (const auto& next : values) {
				//if (next.get_type() != jwt::json::type::string) {
				//	continue;
				//}
				const std::string& next_str = next.get<std::string>();
				if (idx == 0 && pair.first == "aud") {
					m_pluginState->m_env.SetEnv("BEARER_TOKEN_0_AUDIENCE", next_str.c_str());
				}
				if (is_groups) {
					formatstr(env_name, "BEARER_TOKEN_0_GROUP_%d", idx);
					m_pluginState->m_env.SetEnv(env_name, next_str);
				}
				formatstr(env_name, "BEARER_TOKEN_0_CLAIM_%s_%d", pair.first.c_str(), idx);
				m_pluginState->m_env.SetEnv(env_name, next_str);
				idx++;
			}
			break;
		}
		default:
			// skip
			break;
		}
	}

	m_pluginRC = 2;

	return ContinueScitokensPlugins(result, errstack);
}

int
Condor_Auth_SSL::ContinueScitokensPlugins(std::string& result, CondorError* errstack)
{
	if (m_pluginRC != 2) {
		result = m_pluginResult;
		if (!m_pluginErrstack.empty()) {
			// This assumes we only put one entry in our local CondorError
			errstack->push(m_pluginErrstack.subsys(), m_pluginErrstack.code(), m_pluginErrstack.message());
		}
		return m_pluginRC;
	}

	std::string param_name;

	if (m_pluginState->m_pid > 0 && m_pluginState->m_exitStatus >= 0) {
		const char* plugin_name = m_pluginState->m_names[m_pluginState->m_idx].c_str();
		m_pluginState->m_pid = -1;
		dprintf(D_SECURITY|D_VERBOSE, "AUTHENTICATE: Plugin %s stdout:%s\n", plugin_name, m_pluginState->m_stdout.c_str());
		dprintf(D_SECURITY|D_VERBOSE, "AUTHENTICATE: Plugin %s stderr:%s\n", plugin_name, m_pluginState->m_stderr.c_str());
		if (WIFEXITED(m_pluginState->m_exitStatus) && WEXITSTATUS(m_pluginState->m_exitStatus) == 0) {
			dprintf(D_SECURITY|D_VERBOSE, "AUTHENTICATE: Plugin %s matched, extracting result\n", plugin_name);
			formatstr(param_name, "SEC_SCITOKENS_PLUGIN_%s_MAPPING", plugin_name);
			if (param(m_pluginResult, param_name.c_str())) {
				dprintf(D_SECURITY, "AUTHENTICATE: Mapped identity in config file for plugin %s: %s\n", plugin_name, m_pluginResult.c_str());
			} else {
				// look at plugin's output for identity
				// TODO recognize richer output format?
				StringTokenIterator toker(m_pluginState->m_stdout);
				const std::string* first = toker.next_string();
				if (first != nullptr) {
					m_pluginResult = *first;
					dprintf(D_SECURITY, "AUTHENTICATE: Mapped identity from plugin %s: %s\n", plugin_name, m_pluginResult.c_str());
				} else {
					dprintf(D_SECURITY, "AUTHENTICATE: Plugin %s didn't print mapped identity\n", plugin_name);
					errstack->pushf("AUTHENTICATE", AUTHENTICATE_ERR_PLUGIN_FAILED, "Plugin '%s' didn't print mapped identity", plugin_name);
					// TODO Consider calling addl plugins
					m_pluginRC = 0;
					goto cleanup;
				}
			}

			// TODO Consider restricted authz for exotic token types?

			result = m_pluginResult;
			m_pluginRC = 1;
		} else if (WIFEXITED(m_pluginState->m_exitStatus) && WEXITSTATUS(m_pluginState->m_exitStatus) == 1) {
			dprintf(D_SECURITY, "AUTHENTICATE: Plugin %s did not match\n", plugin_name);
			// TODO any other cleanup?
			m_pluginState->m_stdout.clear();
			m_pluginState->m_stderr.clear();
			m_pluginState->m_exitStatus = -1;
			m_pluginState->m_idx++;
		} else {
			dprintf(D_SECURITY, "AUTHENTICATE: Plugin %s exited with unexpected status %d\n", plugin_name, m_pluginState->m_exitStatus);
			errstack->pushf("AUTHENTICATE", AUTHENTICATE_ERR_PLUGIN_FAILED, "Plugin %s failed (bad exit status)", plugin_name);
			// TODO consider addl plugins?
			m_pluginRC = 0;
		}
	}

	if (m_pluginRC == 2 && m_pluginState->m_pid < 0) {
		if (m_pluginState->m_idx >= m_pluginState->m_names.size()) {
			dprintf(D_SECURITY, "No plugins matched, returning empty mapping\n");
			m_pluginRC = 1;
		}
	}

		// TODO add timeout for plugin
	if (m_pluginRC == 2 && m_pluginState->m_pid < 0) {
		const char* plugin_name = m_pluginState->m_names[m_pluginState->m_idx].c_str();
		dprintf(D_SECURITY|D_VERBOSE, "AUTHENTICATE: Trying plugin %s\n", plugin_name);
		std::string cmdline;
		formatstr(param_name, "SEC_SCITOKENS_PLUGIN_%s_COMMAND", plugin_name);
		if (!param(cmdline, param_name.c_str())) {
			dprintf(D_ALWAYS, "AUTHENTICATE: Plugin %s has no command configured\n", plugin_name);
			errstack->pushf("AUTHENTICATE", AUTHENTICATE_ERR_PLUGIN_FAILED, "Plugin %s failed (no command param)", plugin_name);
			m_pluginRC = 0;
			goto cleanup;
		}
		ArgList args;
		std::string errmsg;
		if (!args.AppendArgsV2Raw(cmdline.c_str(), errmsg)) {
			dprintf(D_ALWAYS, "AUTHENTICATE: Failed to parse command for plugin %s: %s\n", plugin_name, errmsg.c_str());
			errstack->pushf("AUTHENTICATE", AUTHENTICATE_ERR_PLUGIN_FAILED, "Plugin %s failed (invalid command param)", plugin_name);
			m_pluginRC = 0;
			goto cleanup;
		}

		int std_fds[3] = {DC_STD_FD_PIPE, DC_STD_FD_PIPE, DC_STD_FD_PIPE};

		// Tell DaemonCore to register the process family so we can
		// safely kill everything from the reaper.
		FamilyInfo fi;
		fi.max_snapshot_interval = param_integer("PID_SNAPSHOT_INTERVAL", 15);

		int pid = daemonCore->Create_Process(args.GetArg(0), args,
					PRIV_CONDOR_FINAL, m_pluginReaperId, FALSE,
					FALSE, &m_pluginState->m_env, nullptr, &fi, nullptr,
					std_fds);
		if (pid == 0) {
			dprintf(D_ALWAYS, "AUTHENTICATE: Failed to spawn plugin %s.\n", plugin_name);
			errstack->pushf("AUTHENTICATE", AUTHENTICATE_ERR_PLUGIN_FAILED, "Plugin %s failed (failed to spawn)", plugin_name);
			m_pluginRC = 0;
			goto cleanup;
		}
		m_pluginState->m_pid = pid;
		daemonCore->Write_Stdin_Pipe(pid, m_pluginState->m_stdin.c_str(),
		                             m_pluginState->m_stdin.length());

		dprintf(D_SECURITY, "AUTHENTICATE: Spawned plugin %s, pid=%d\n", plugin_name, pid);
		m_pluginPidTable[pid] = this;
	}

 cleanup:
	if (m_pluginRC != 2) {
		m_pluginState.reset();
	}
	return m_pluginRC;
}

void
Condor_Auth_SSL::CancelScitokensPlugins()
{
	if (!m_pluginState || m_pluginState->m_pid == -1) {
		return;
	}

	daemonCore->Kill_Family(m_pluginState->m_pid);

	m_pluginPidTable[m_pluginState->m_pid] = nullptr;
	m_pluginState.reset();
	m_pluginRC = 0;
}

int
Condor_Auth_SSL::PluginReaper(int exit_pid, int exit_status)
{
	dprintf(D_SECURITY, "SciTokens plugin pid %d exited with status %d\n",
	        exit_pid, exit_status);

	// First, make sure the plugin didn't leak any processes.
	daemonCore->Kill_Family(exit_pid);

	auto it = m_pluginPidTable.find(exit_pid);
	if (it == m_pluginPidTable.end()) {
			// no such pid
		dprintf(D_ALWAYS, "SciTokens plugin pid %d not found in table!\n", exit_pid);
		return TRUE;
	}

	if (it->second == nullptr) {
		dprintf(D_SECURITY, "SciTokens auth object was previously deleted, ignoring plugin\n");
	} else if (!it->second->m_pluginState) {
		dprintf(D_SECURITY, "SciTokens auth object has no plugin state, ignoring plugin\n");
	} else {
		std::string *output;
		std::string result;
		output = daemonCore->Read_Std_Pipe(exit_pid, 1);
		if (output) {
			it->second->m_pluginState->m_stdout = *output;
		}
		output = daemonCore->Read_Std_Pipe(exit_pid, 2);
		if (output) {
			it->second->m_pluginState->m_stderr = *output;
		}
		it->second->m_pluginState->m_exitStatus = exit_status;
		if (it->second->ContinueScitokensPlugins(result, &it->second->m_pluginErrstack) != 2) {
			dprintf(D_SECURITY, "SciTokens plugins done, triggering socket callback\n");
			daemonCore->CallSocketHandler(it->second->mySock_);
		}
	}

	m_pluginPidTable.erase(it);
	return TRUE;
}
