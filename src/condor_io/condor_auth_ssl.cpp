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

#if !defined(SKIP_AUTHENTICATION) && defined(HAVE_EXT_OPENSSL)
#define ouch(x) dprintf(D_SECURITY,"SSL Auth: %s",x)
#include "authentication.h"
#include "condor_auth_ssl.h"
#include "condor_config.h"
#include "condor_environ.h"
#include "CondorError.h"
#include "openssl/rand.h"
#include "condor_netdb.h"

#if defined(DLOPEN_SECURITY_LIBS)
#include <dlfcn.h>
#include "condor_auth_kerberos.h"
#endif

#if defined(HAVE_EXT_SCITOKENS)
#include <scitokens/scitokens.h>
#endif

// Symbols from libssl
#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
static long (*SSL_CTX_ctrl_ptr)(SSL_CTX *, int, long, void *) = NULL;
#endif
static void (*SSL_CTX_free_ptr)(SSL_CTX *) = NULL;
static int (*SSL_CTX_load_verify_locations_ptr)(SSL_CTX *, const char *, const char *) = NULL;
#if OPENSSL_VERSION_NUMBER < 0x10000000L
static SSL_CTX *(*SSL_CTX_new_ptr)(SSL_METHOD *) = NULL;
#else
static SSL_CTX *(*SSL_CTX_new_ptr)(const SSL_METHOD *) = NULL;
#endif
static int (*SSL_CTX_set_cipher_list_ptr)(SSL_CTX *, const char *) = NULL;
static void (*SSL_CTX_set_verify_ptr)(SSL_CTX *, int, int (*)(int, X509_STORE_CTX *)) = NULL;
static void (*SSL_CTX_set_verify_depth_ptr)(SSL_CTX *, int) = NULL;
static int (*SSL_CTX_use_PrivateKey_file_ptr)(SSL_CTX *, const char *, int) = NULL;
static int (*SSL_CTX_use_certificate_chain_file_ptr)(SSL_CTX *, const char *) = NULL;
static int (*SSL_accept_ptr)(SSL *) = NULL;
static int (*SSL_connect_ptr)(SSL *) = NULL;
static void (*SSL_free_ptr)(SSL *) = NULL;
static int (*SSL_get_error_ptr)(const SSL *, int) = NULL;
static X509 *(*SSL_get_peer_certificate_ptr)(const SSL *) = NULL;
static long (*SSL_get_verify_result_ptr)(const SSL *) = NULL;
#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
static int (*SSL_library_init_ptr)() = NULL;
static void (*SSL_load_error_strings_ptr)() = NULL;
#else
static int (*OPENSSL_init_ssl_ptr)(uint64_t, const OPENSSL_INIT_SETTINGS *) = NULL;
#endif
static SSL *(*SSL_new_ptr)(SSL_CTX *) = NULL;
static int (*SSL_read_ptr)(SSL *, void *, int) = NULL;
static void (*SSL_set_bio_ptr)(SSL *, BIO *, BIO *) = NULL;
static int (*SSL_write_ptr)(SSL *, const void *, int) = NULL;
#if OPENSSL_VERSION_NUMBER < 0x10000000L
static SSL_METHOD *(*SSL_method_ptr)() = NULL;
#else
static const SSL_METHOD *(*SSL_method_ptr)() = NULL;
#endif

bool Condor_Auth_SSL::m_initTried = false;
bool Condor_Auth_SSL::m_initSuccess = false;

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
    : Condor_Auth_Base    ( sock, CAUTH_SSL ),
	m_scitokens_mode(scitokens_mode)
{
	m_crypto = NULL;
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
		 !(SSL_CTX_ctrl_ptr = (long (*)(SSL_CTX *, int, long, void *))dlsym(dl_hdl, "SSL_CTX_ctrl")) ||
#endif
		 !(SSL_CTX_free_ptr = (void (*)(SSL_CTX *))dlsym(dl_hdl, "SSL_CTX_free")) ||
		 !(SSL_CTX_load_verify_locations_ptr = (int (*)(SSL_CTX *, const char *, const char *))dlsym(dl_hdl, "SSL_CTX_load_verify_locations")) ||
#if OPENSSL_VERSION_NUMBER < 0x10000000L
		 !(SSL_CTX_new_ptr = (SSL_CTX *(*)(SSL_METHOD *))dlsym(dl_hdl, "SSL_CTX_new")) ||
#else
		 !(SSL_CTX_new_ptr = (SSL_CTX *(*)(const SSL_METHOD *))dlsym(dl_hdl, "SSL_CTX_new")) ||
#endif
		 !(SSL_CTX_set_cipher_list_ptr = (int (*)(SSL_CTX *, const char *))dlsym(dl_hdl, "SSL_CTX_set_cipher_list")) ||
		 !(SSL_CTX_set_verify_ptr = (void (*)(SSL_CTX *, int, int (*)(int, X509_STORE_CTX *)))dlsym(dl_hdl, "SSL_CTX_set_verify")) ||
		 !(SSL_CTX_set_verify_depth_ptr = (void (*)(SSL_CTX *, int))dlsym(dl_hdl, "SSL_CTX_set_verify_depth")) ||
		 !(SSL_CTX_use_PrivateKey_file_ptr = (int (*)(SSL_CTX *, const char *, int))dlsym(dl_hdl, "SSL_CTX_use_PrivateKey_file")) ||
		 !(SSL_CTX_use_certificate_chain_file_ptr = (int (*)(SSL_CTX *, const char *))dlsym(dl_hdl, "SSL_CTX_use_certificate_chain_file")) ||
		 !(SSL_accept_ptr = (int (*)(SSL *))dlsym(dl_hdl, "SSL_accept")) ||
		 !(SSL_connect_ptr = (int (*)(SSL *))dlsym(dl_hdl, "SSL_connect")) ||
		 !(SSL_free_ptr = (void (*)(SSL *))dlsym(dl_hdl, "SSL_free")) ||
		 !(SSL_get_error_ptr = (int (*)(const SSL *, int))dlsym(dl_hdl, "SSL_get_error")) ||
		 !(SSL_get_peer_certificate_ptr = (X509 *(*)(const SSL *))dlsym(dl_hdl, "SSL_get_peer_certificate")) ||
		 !(SSL_get_verify_result_ptr = (long (*)(const SSL *))dlsym(dl_hdl, "SSL_get_verify_result")) ||
#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
		 !(SSL_library_init_ptr = (int (*)())dlsym(dl_hdl, "SSL_library_init")) ||
		 !(SSL_load_error_strings_ptr = (void (*)())dlsym(dl_hdl, "SSL_load_error_strings")) ||
#else
		 !(OPENSSL_init_ssl_ptr = (int (*)(uint64_t, const OPENSSL_INIT_SETTINGS *))dlsym(dl_hdl, "OPENSSL_init_ssl")) ||
#endif
		 !(SSL_new_ptr = (SSL *(*)(SSL_CTX *))dlsym(dl_hdl, "SSL_new")) ||
		 !(SSL_read_ptr = (int (*)(SSL *, void *, int))dlsym(dl_hdl, "SSL_read")) ||
		 !(SSL_set_bio_ptr = (void (*)(SSL *, BIO *, BIO *))dlsym(dl_hdl, "SSL_set_bio")) ||
		 !(SSL_write_ptr = (int (*)(SSL *, const void *, int))dlsym(dl_hdl, "SSL_write")) ||
#if OPENSSL_VERSION_NUMBER < 0x10000000L
		 !(SSL_method_ptr = (SSL_METHOD *(*)())dlsym(dl_hdl, "SSLv23_method"))
#elif OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
		 !(SSL_method_ptr = (const SSL_METHOD *(*)())dlsym(dl_hdl, "SSLv23_method"))
#else
		 !(SSL_method_ptr = (const SSL_METHOD *(*)())dlsym(dl_hdl, "TLS_method"))
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
#endif
	SSL_CTX_free_ptr = SSL_CTX_free;
	SSL_CTX_load_verify_locations_ptr = SSL_CTX_load_verify_locations;
	SSL_CTX_new_ptr = SSL_CTX_new;
	SSL_CTX_set_cipher_list_ptr = SSL_CTX_set_cipher_list;
	SSL_CTX_set_verify_ptr = SSL_CTX_set_verify;
	SSL_CTX_set_verify_depth_ptr = SSL_CTX_set_verify_depth;
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
	m_initSuccess = true;
#endif

	m_initTried = true;
	return m_initSuccess;
}


int
Condor_Auth_SSL::authenticate_continue(CondorError *errstack, bool non_blocking)
{
	if (!m_auth_state.get()) {
		ouch("Trying to ontinue authentication after failure!\n");
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
	if (!m_auth_state.get()) {
		m_auth_state.reset(new AuthState);
	}

    if( mySock_->isClient() ) {
		std::string scitoken;
		if (m_scitokens_mode) {
			if (m_scitokens_file.empty()) {
				ouch( "No SciToken file provided\n" );
				m_auth_state->m_client_status = AUTH_SSL_ERROR;
			} else {
				FILE * f = safe_fopen_no_create( m_scitokens_file.c_str(), "r" );
				if (f == nullptr) {
					dprintf(D_ALWAYS, "Failed to open scitoken file '%s': %d (%s)\n",
						m_scitokens_file.c_str(), errno, strerror(errno));
					m_auth_state->m_client_status = AUTH_SSL_ERROR;
				} else {
					for (std::string line; readLine(line, f, false); ) {
						// Strip out whitespace and ignore comments.
						line.erase( line.length() - 1, 1 );
						line.erase(line.begin(),
						std::find_if(line.begin(), line.end(),
							[](int ch) {return !isspace(ch);}));
						if (line.empty() || line[0] == '#') { continue; }

						scitoken = line;
						ouch( "Found a SciToken to use for authentication" );
						break;
					}
				}
			}
		}
        if( init_OpenSSL( ) != AUTH_SSL_A_OK ) {
            ouch( "Error initializing OpenSSL for authentication\n" );
            m_auth_state->m_client_status = AUTH_SSL_ERROR;
        }
        if( !(m_auth_state->m_ctx = setup_ssl_ctx( false )) ) {
            ouch( "Error initializing client security context\n" );
            m_auth_state->m_client_status = AUTH_SSL_ERROR;
        }
        if( !(m_auth_state->m_conn_in = BIO_new( BIO_s_mem( ) )) 
            || !(m_auth_state->m_conn_out = BIO_new( BIO_s_mem( ) )) ) {
            ouch( "Error creating buffer for SSL authentication\n" );
            m_auth_state->m_client_status = AUTH_SSL_ERROR;
        }
        if( !(m_auth_state->m_ssl = (*SSL_new_ptr)( m_auth_state->m_ctx )) ) {
            ouch( "Error creating SSL context\n" );
            m_auth_state->m_client_status = AUTH_SSL_ERROR;
        } else {
            (*SSL_set_bio_ptr)( m_auth_state->m_ssl, m_auth_state->m_conn_in,
				m_auth_state->m_conn_out );
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
                m_auth_state->m_ssl_status = (*SSL_connect_ptr)( m_auth_state->m_ssl );
                dprintf(D_SECURITY, "Tried to connect: %d\n", m_auth_state->m_ssl_status);
            }
            if( m_auth_state->m_ssl_status < 1 ) {
                m_auth_state->m_client_status = AUTH_SSL_QUITTING;
                m_auth_state->m_done = 1;
                //ouch( "Error performing SSL authentication\n" );
                m_auth_state->m_err = (*SSL_get_error_ptr)( m_auth_state->m_ssl, m_auth_state->m_ssl_status );
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
                    ouch("SSL: library failure.  see error queue?\n");
                    break;
                default:
                    ouch("SSL: unknown error?\n" );
                    break;
                }
            } else {
                m_auth_state->m_client_status = AUTH_SSL_HOLDING;
            }
            m_auth_state->m_round_ctr++;
            dprintf(D_SECURITY,"Round %d.\n", m_auth_state->m_round_ctr);            
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
            dprintf(D_SECURITY,"Status (c: %d, s: %d)\n", m_auth_state->m_client_status,
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
        dprintf(D_SECURITY,"Client trying post connection check.\n");
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

        dprintf(D_SECURITY,"Client performs one last exchange of messages.\n");

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
            dprintf(D_SECURITY,"Reading round %d.\n",++m_auth_state->m_round_ctr);
            if(m_auth_state->m_round_ctr > 256) {
                ouch("Too many rounds exchanging key: quitting.\n");
                m_auth_state->m_done = 1;
                m_auth_state->m_client_status = AUTH_SSL_QUITTING;
                break;
            }
            if( m_auth_state->m_client_status != AUTH_SSL_HOLDING) {
                m_auth_state->m_ssl_status = (*SSL_read_ptr)(m_auth_state->m_ssl, 
									  m_auth_state->m_session_key, AUTH_SSL_SESSION_KEY_LEN);
            }
            if(m_auth_state->m_ssl_status < 1) {
                m_auth_state->m_err = (*SSL_get_error_ptr)( m_auth_state->m_ssl,
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
                dprintf(D_SECURITY,"SSL read has succeeded.\n");
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
            dprintf(D_SECURITY, "Status: c: %d, s: %d\n", m_auth_state->m_client_status,
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
			std::vector<unsigned char> network_bytes;
			network_bytes.reserve(sizeof(network_size) + scitoken.size());
			memcpy(&network_bytes[0], static_cast<void*>(&network_size), sizeof(network_size));
			memcpy(&network_bytes[0] + sizeof(network_size), scitoken.c_str(), scitoken.size());
			while(!m_auth_state->m_done) {
				dprintf(D_SECURITY,"Writing SciToken round %d.\n",++m_auth_state->m_round_ctr);

				// Abort if the exchange has gone on for too long.
				if(m_auth_state->m_round_ctr > 256) {
					ouch("Too many rounds exchanging key: quitting.\n");
					m_auth_state->m_done = 1;
					m_auth_state->m_client_status = AUTH_SSL_QUITTING;
					break;
				}

				if( m_auth_state->m_client_status != AUTH_SSL_HOLDING) {
					m_auth_state->m_ssl_status = (*SSL_write_ptr)(m_auth_state->m_ssl,
						&network_bytes[0], sizeof(network_size) + scitoken.size());
				}
				if(m_auth_state->m_ssl_status < 1) {
					m_auth_state->m_err = (*SSL_get_error_ptr)( m_auth_state->m_ssl,
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
					dprintf(D_SECURITY,"SSL write is successful.\n");
					m_auth_state->m_client_status = AUTH_SSL_HOLDING;
				}
				if(m_auth_state->m_round_ctr % 2 == 1) {
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
				dprintf(D_SECURITY, "SciToken exchange status: c: %d, s: %d\n", m_auth_state->m_client_status,
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
			if( m_auth_state->m_server_status == AUTH_SSL_QUITTING
				|| m_auth_state->m_client_status == AUTH_SSL_QUITTING )
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
        if (!(m_auth_state->m_ssl = (*SSL_new_ptr)(m_auth_state->m_ctx))) {
            ouch("Error creating SSL context\n");
            m_auth_state->m_server_status = AUTH_SSL_ERROR;
        } else {
            // SSL_set_accept_state(ssl); // Do I really have to do this?
            (*SSL_set_bio_ptr)(m_auth_state->m_ssl, m_auth_state->m_conn_in,
				m_auth_state->m_conn_out);
        }
		if( send_status( m_auth_state->m_server_status ) == AUTH_SSL_ERROR ) {
			return static_cast<int>(CondorAuthSSLRetval::Fail);
		}
		CondorAuthSSLRetval tmp_status = authenticate_server_pre(errstack, non_blocking);
		if (tmp_status == CondorAuthSSLRetval::Fail) {
			return static_cast<int>(authenticate_fail());
		} else if (tmp_status != CondorAuthSSLRetval::Success) {
			return static_cast<int>(tmp_status);
		}
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
                m_auth_state->m_ssl_status = (*SSL_accept_ptr)( m_auth_state->m_ssl );
                dprintf(D_SECURITY, "Accept returned %d.\n", m_auth_state->m_ssl_status);
            }
            if( m_auth_state->m_ssl_status < 1 ) {
                m_auth_state->m_server_status = AUTH_SSL_QUITTING;
                m_auth_state->m_done = 1;
                m_auth_state->m_err = (*SSL_get_error_ptr)( m_auth_state->m_ssl,
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
                    ouch("SSL: library failure.  see error queue?\n");
                    break;
                default:
                    ouch("SSL: unknown error?\n" );
                    break;
                }
            } else {
                m_auth_state->m_server_status = AUTH_SSL_HOLDING;
            }
            dprintf(D_SECURITY,"Round %d.\n", m_auth_state->m_round_ctr);
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
            dprintf(D_SECURITY,"Status (c: %d, s: %d)\n", m_auth_state->m_client_status,
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
        ouch("Server trying post connection check.\n");
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
            dprintf(D_SECURITY,"Writing round %d.\n", m_auth_state->m_round_ctr);
            if(m_auth_state->m_round_ctr > 256) {
                ouch("Too many rounds exchanging key: quitting.\n");
                m_auth_state->m_done = 1;
                m_auth_state->m_server_status = AUTH_SSL_QUITTING;
                break;
            }
            if( m_auth_state->m_server_status != AUTH_SSL_HOLDING ) {
                m_auth_state->m_ssl_status = (*SSL_write_ptr)(m_auth_state->m_ssl, 
									   m_auth_state->m_session_key, AUTH_SSL_SESSION_KEY_LEN);
            }
            if(m_auth_state->m_ssl_status < 1) {
                m_auth_state->m_err = (*SSL_get_error_ptr)( m_auth_state->m_ssl,
					m_auth_state->m_ssl_status);
                switch( m_auth_state->m_err ) {
                case SSL_ERROR_WANT_READ:
                case SSL_ERROR_WANT_WRITE:
                    ouch("SSL: continue read/write.\n");
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
                dprintf(D_SECURITY, "SSL write has succeeded.\n");
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
            dprintf(D_SECURITY, "Status: c: %d, s: %d\n", m_auth_state->m_client_status,
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
		dprintf(D_SECURITY,"Reading SciTokens round %d.\n", m_auth_state->m_round_ctr);
		if(m_auth_state->m_round_ctr > 256) {
			ouch("Too many rounds exchanging SciToken: quitting.\n");
			m_auth_state->m_done = 1;
			m_auth_state->m_server_status = AUTH_SSL_QUITTING;
			break;
		}
		uint32_t token_length = 0;
		if( m_auth_state->m_server_status != AUTH_SSL_HOLDING ) {
			if (m_auth_state->m_token_length == -1) {
				m_auth_state->m_ssl_status = (*SSL_read_ptr)(m_auth_state->m_ssl,
					static_cast<void*>(&token_length), sizeof(token_length));
			} else {
				token_contents.reserve(token_length);
				m_auth_state->m_ssl_status = (*SSL_read_ptr)(m_auth_state->m_ssl,
					static_cast<void*>(&token_contents), token_length);
			}
		}
		if(m_auth_state->m_ssl_status < 1) {
			m_auth_state->m_err = (*SSL_get_error_ptr)( m_auth_state->m_ssl,
				m_auth_state->m_ssl_status);
			switch( m_auth_state->m_err ) {
			case SSL_ERROR_WANT_READ:
			case SSL_ERROR_WANT_WRITE:
				ouch("SciToken: continue read/write.\n");
				m_auth_state->m_done = 0;
				m_auth_state->m_server_status = AUTH_SSL_RECEIVING;
				break;
			default:
				m_auth_state->m_server_status = AUTH_SSL_QUITTING;
				m_auth_state->m_done = 1;
				ouch("SciToken: error on read.  Can't proceed.\n");
				break;
			}
		} else {
			if (m_auth_state->m_token_length == -1) {
				dprintf(D_SECURITY|D_FULLDEBUG, "SciToken length SSL read is successful.\n");
				m_auth_state->m_token_length = ntohl(token_length);
					// We also expect the token itself in the same "round".
				continue;
			} else {
				dprintf(D_SECURITY, "SciToken SSL read is successful.\n");
				m_client_scitoken = std::string(&token_contents[0], token_length);
				if(m_auth_state->m_client_status == AUTH_SSL_HOLDING) {
					m_auth_state->m_done = 1;
				}
				m_auth_state->m_server_status = AUTH_SSL_HOLDING;
			}
		}
		if(m_auth_state->m_round_ctr % 2 == 0) {
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
		dprintf(D_SECURITY, "SciToken exchange server status: c: %d, s: %d\n", m_auth_state->m_client_status,
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


Condor_Auth_SSL::CondorAuthSSLRetval
Condor_Auth_SSL::authenticate_finish(CondorError * /*errstack*/, bool /*non_blocking*/)
{
    char subjectname[1024];
	Condor_Auth_SSL::CondorAuthSSLRetval retval = CondorAuthSSLRetval::Success;
	if (m_scitokens_mode) {
		if (mySock_->isClient()) {
			X509 *peer = (*SSL_get_peer_certificate_ptr)(m_auth_state->m_ssl);
			if (peer) {
				X509_NAME_oneline(X509_get_subject_name(peer), subjectname, 1024);
				setRemoteUser( "ssl" );
			} else {
				ouch("Server subject name is unavailable.");
				retval = CondorAuthSSLRetval::Fail;
			}
		} else {
			SciToken token = nullptr;
			char *err_msg = nullptr;
			char *issuer = nullptr;
			Enforcer enf = nullptr;
			Acl *acls = nullptr;
			std::vector<std::string> audiences;
			std::vector<const char *> audience_ptr;
			std::string audience_string;
			setRemoteUser("unauthenticated");
			setAuthenticatedName( "unauthenticated" );
			setRemoteDomain( UNMAPPED_DOMAIN );
			if (param(audience_string, "SCITOKENS_SERVER_AUDIENCE")) {
				StringList audience_list(audience_string.c_str());
				audience_list.rewind();
				char *aud;
				while ( (aud = audience_list.next()) ) {
					audiences.emplace_back(aud);
					audience_ptr.push_back(audiences.back().c_str());
				}
			}
			if (scitoken_deserialize(m_client_scitoken.c_str(), &token, nullptr, &err_msg)) {
				dprintf(D_SECURITY, "Failed to deserialize scitoken: %s\n",
					err_msg ? err_msg : "(unknown failure");
				free(err_msg);
				retval = CondorAuthSSLRetval::Fail;
			} else if (scitoken_get_claim_string(token, "iss", &issuer, &err_msg)) {
				dprintf(D_SECURITY, "Unable to retrieve token issuer: %s\n",
					err_msg ? err_msg : "(unknown failure");
				free(err_msg);
				scitoken_destroy(token);
				token = nullptr;
				retval = CondorAuthSSLRetval::Fail;
			} else if (!(enf = enforcer_create(issuer, &audience_ptr[0], &err_msg))) {
				dprintf(D_SECURITY, "Failed to create SciTokens enforcer: %s\n",
					err_msg ? err_msg : "(unknown failure");
				free(err_msg);
				scitoken_destroy(token);
				free(issuer);
				retval = CondorAuthSSLRetval::Fail;
			} else if (!enforcer_generate_acls(enf, token, &acls, &err_msg)) {
				dprintf(D_SECURITY, "Failed to verify token and generate ACLs: %s\n",
					err_msg ? err_msg : "(unknown failure");
				free(err_msg);
				scitoken_destroy(token);
				free(issuer);
				enforcer_destroy(enf);
				retval = CondorAuthSSLRetval::Fail;
			} else {
				int idx = 0;
				Acl acl = acls[idx++];
				while ( (acl.authz && acl.resource) ) {
					dprintf(D_SECURITY, "Found SciToken ACL: %s\n",
						err_msg ? err_msg : "(unknown failure");
					acl = acls[idx++];
				}
				setRemoteUser("scitokens");
				setAuthenticatedName( issuer );
			}
		}
	} else {
    	X509 *peer = (*SSL_get_peer_certificate_ptr)(m_auth_state->m_ssl);
		if (peer) {
			X509_NAME_oneline(X509_get_subject_name(peer), subjectname, 1024);
			setRemoteUser( "ssl" );
		} else {
			strcpy(subjectname, "unauthenticated");
			setRemoteUser( "unauthenticated" );
		}
		setAuthenticatedName( subjectname );
		setRemoteDomain( UNMAPPED_DOMAIN );
	}

    dprintf(D_SECURITY,"SSL authentication succeeded to %s\n", subjectname);
	m_auth_state.release();
    return retval;
}


Condor_Auth_SSL::CondorAuthSSLRetval
Condor_Auth_SSL::authenticate_fail()
{
	m_auth_state.release();
	return CondorAuthSSLRetval::Fail;
}


int Condor_Auth_SSL::isValid() const
{
	if ( m_crypto ) {
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

	if ( !key || !keylen ) {
		// cannot setup anything without a key
		return false;
	}

		// This could be 3des -- maybe we should use "best crypto" indirection.
	KeyInfo thekey(key,keylen,CONDOR_3DES);
	m_crypto = new Condor_Crypt_3des(thekey);
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
	if (!m_crypto) {
		return false;
	}

		// do the work
	m_crypto->resetState();
	if (want_encrypt) {
		result = m_crypto->encrypt(input,input_len,output,output_len);
	} else {
		result = m_crypto->decrypt(input,input_len,output,output_len);
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
	dprintf(D_SECURITY, "In wrap.\n");
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
	
	dprintf(D_SECURITY, "In unwrap.\n");
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
    if (!(*SSL_library_init_ptr)()) {
        return AUTH_SSL_ERROR;
    }
    (*SSL_load_error_strings_ptr)();
#else
    if (!(*OPENSSL_init_ssl_ptr)(OPENSSL_INIT_LOAD_SSL_STRINGS \
                               | OPENSSL_INIT_LOAD_CRYPTO_STRINGS, NULL)) {
        return AUTH_SSL_ERROR;
    }
#endif
    // seed_pnrg(); TODO: 
    return AUTH_SSL_A_OK;
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
        dprintf( D_SECURITY, "  subject  = %s\n", data );
        dprintf( D_SECURITY, "  err %i:%s\n", err, X509_verify_cert_error_string( err ) );
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

    dprintf(D_SECURITY, "Send message (%d).\n", status );
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
		ouch("Would block when trying to receive message\n");
		return CondorAuthSSLRetval::WouldBlock;
	}
    ouch("Receive message.\n");
    mySock_ ->decode( );
    if( !(mySock_ ->code( status ))
        || !(mySock_ ->code( len ))
        || !(len <= AUTH_SSL_BUF_SIZE)
        || !(len == (mySock_ ->get_bytes( buf, len )))
        || !(mySock_ ->end_of_message( )) ) {
        ouch( "Error communicating with peer.\n" );
        return CondorAuthSSLRetval::Fail;
    }
    dprintf(D_SECURITY,"Received message (%d).\n", status );
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
    ouch("Server exchange messages.\n");
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
    ouch("Client exchange messages.\n");
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


long Condor_Auth_SSL :: post_connection_check(SSL *ssl, int /* role */ )
{
	X509      *cert;
		/* These are removed, see below.
    X509_NAME *subj;
    char      data[256];
    int       extcount;
    int       ok = 0;
		*/
//    char      err_buf[500];
    ouch("post_connection_check.\n");
 
    /* Checking the return from SSL_get_peer_certificate here is not
     * strictly necessary.  With our example programs, it is not
     * possible for it to return NULL.  However, it is good form to
     * check the return since it can return NULL if the examples are
     * modified to enable anonymous ciphers or for the server to not
     * require a client certificate.
     * (Comment from book.  -Ian)
     */
    cert = (*SSL_get_peer_certificate_ptr)(ssl);
    if( cert == NULL ) {
        dprintf(D_SECURITY,"SSL_get_peer_certificate returned null.\n" );
        goto err_occured;
    }
    dprintf(D_SECURITY,"SSL_get_peer_certificate returned data.\n" );

    /* What follows is working code based on examples from the book
       "Network Programming with OpenSSL."  The point of this code
       is to determine that the certificate is a valid host certificate
       for the server side.  Zach says that this will be done by
       the caller of the authenticate method, so we just ignore it.
       However, since it's more complicated than simply extracting
       the CN and comparing it to the host name, I'm leaving it in
       here so we can refer to it if needed.

       TODO: Does globus do this?  Do we need to do it?

       TODO: The caller is probably going to want the DN, we should
       extract it here.
       
       -Ian
       
    if(role == AUTH_SSL_ROLE_SERVER) {
        X509_free( cert );
        ouch("Server role: returning from post connection check.\n");
        return (*SSL_get_verify_result_ptr)( ssl );
    } // else ROLE_CLIENT: check dns (arg 2) against cn
    if ((extcount = X509_get_ext_count(cert)) > 0) {
        int i;
        for (i = 0;  i < extcount;  i++) {
            const char              *extstr;
            X509_EXTENSION    *ext;
            ASN1_OBJECT *foo;

            ext = X509_get_ext(cert, i);
            foo = X509_EXTENSION_get_object(ext);
            int bar = OBJ_obj2nid(foo);
            extstr = OBJ_nid2sn(bar);
            
            if (!strcmp(extstr, "subjectAltName")) {
                int                  j;
                unsigned char        *data;
                STACK_OF(CONF_VALUE) *val;
                CONF_VALUE           *nval;
                X509V3_EXT_METHOD    *meth;
                void                 *ext_str = NULL;
 
                if (!(meth = X509V3_EXT_get(ext)))
                    break;
                data = ext->value->data;
#if (OPENSSL_VERSION_NUMBER > 0x00907000L)

                if (meth->it)
                    ext_str = ASN1_item_d2i(NULL, &data, ext->value->length,
                                            ASN1_ITEM_ptr(meth->it));
                else
                    ext_str = meth->d2i(NULL, &data, ext->value->length);
#else
                ext_str = meth->d2i(NULL, &data, ext->value->length);
#endif
                val = meth->i2v(meth, ext_str, NULL);
                for (j = 0;  j < sk_CONF_VALUE_num(val);  j++) {
                    nval = sk_CONF_VALUE_value(val, j);
                    if (!strcmp(nval->name, "DNS")
                        && !strcmp(nval->value, host)) {
                        ok = 1;
                        break;
                    }
                }
            }
            if (ok)
                break;
        }
    }
    
    if (!ok && (subj = X509_get_subject_name(cert)) &&
        X509_NAME_get_text_by_NID(subj, NID_commonName, data, 256) > 0)
    {
        data[255] = 0;
        dprintf(D_SECURITY, "Common Name: '%s'; host: '%s'\n", data, host);
        if (strcasecmp(data, host) != 0)
            goto err_occured;
    }
    if( cert != NULL ) {
        dprintf(D_SECURITY,"Client: Got non null cert.\n" );
    }
    */
    ouch("Returning SSL_get_verify_result.\n");
    
    X509_free(cert);
    return (*SSL_get_verify_result_ptr)(ssl);
 
err_occured:
    if (cert)
        X509_free(cert);
    return X509_V_ERR_APPLICATION_VERIFICATION;
}

SSL_CTX *Condor_Auth_SSL :: setup_ssl_ctx( bool is_server )
{
    SSL_CTX *ctx       = NULL;
    char *cafile       = NULL;
    char *cadir        = NULL;
    char *certfile     = NULL;
    char *keyfile      = NULL;
    char *cipherlist   = NULL;
    priv_state priv;

		// Not sure where we want to get these things from but this
		// will do for now.
	if( is_server ) {
		cafile     = param( AUTH_SSL_SERVER_CAFILE_STR );
		cadir      = param( AUTH_SSL_SERVER_CADIR_STR );
		certfile   = param( AUTH_SSL_SERVER_CERTFILE_STR );
		keyfile    = param( AUTH_SSL_SERVER_KEYFILE_STR );
	} else {
		cafile     = param( AUTH_SSL_CLIENT_CAFILE_STR );
		cadir      = param( AUTH_SSL_CLIENT_CADIR_STR );
		if (m_scitokens_mode) {
			param( m_scitokens_file, "SCITOKENS_FILE" );
		} else {
			certfile   = param( AUTH_SSL_CLIENT_CERTFILE_STR );
			keyfile    = param( AUTH_SSL_CLIENT_KEYFILE_STR );
		}
	}		
	cipherlist = param( AUTH_SSL_CIPHERLIST_STR );
    if( cipherlist == NULL ) {
		cipherlist = strdup(AUTH_SSL_DEFAULT_CIPHERLIST);
    }
    if( is_server && (!certfile || !keyfile) ) {
        ouch( "Please specify path to server certificate and key\n" );
        dprintf(D_SECURITY, "in config file : '%s' and '%s'.\n",
                AUTH_SSL_SERVER_CERTFILE_STR, AUTH_SSL_SERVER_KEYFILE_STR );
		ctx = NULL;
        goto setup_server_ctx_err;
    }
    if(cafile)     dprintf( D_SECURITY, "CAFILE:     '%s'\n", cafile     );
    if(cadir)      dprintf( D_SECURITY, "CADIR:      '%s'\n", cadir      );
    if(certfile)   dprintf( D_SECURITY, "CERTFILE:   '%s'\n", certfile   );
    if(keyfile)    dprintf( D_SECURITY, "KEYFILE:    '%s'\n", keyfile    );
    if(cipherlist) dprintf( D_SECURITY, "CIPHERLIST: '%s'\n", cipherlist );
    if (!m_scitokens_file.empty())
	dprintf( D_SECURITY, "SCITOKENSFILE:   '%s'\n", m_scitokens_file.c_str()   );
        
    ctx = (*SSL_CTX_new_ptr)( (*SSL_method_ptr)(  ) );
	if(!ctx) {
		ouch( "Error creating new SSL context.\n");
		goto setup_server_ctx_err;
	}

#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
	// disable SSLv2.  it has vulnerabilities.
	//SSL_CTX_set_options( ctx, SSL_OP_NO_SSLv2 );
	(*SSL_CTX_ctrl_ptr)( ctx, SSL_CTRL_OPTIONS, SSL_OP_NO_SSLv2, NULL );
#endif

    if( (*SSL_CTX_load_verify_locations_ptr)( ctx, cafile, cadir ) != 1 ) {
        dprintf(D_SECURITY, "SSL Auth: Error loading CA file (%s) and/or directory (%s) \n",
		 cafile, cadir);
	goto setup_server_ctx_err;
    }
    if( certfile && (*SSL_CTX_use_certificate_chain_file_ptr)( ctx, certfile ) != 1 ) {
        ouch( "Error loading certificate from file" );
        goto setup_server_ctx_err;
    }
    priv = set_root_priv();
    if( keyfile && (*SSL_CTX_use_PrivateKey_file_ptr)( ctx, keyfile, SSL_FILETYPE_PEM) != 1 ) {
        set_priv(priv);
        ouch( "Error loading private key from file" );
        goto setup_server_ctx_err;
    }
    set_priv(priv);
		// TODO where's this?
    (*SSL_CTX_set_verify_ptr)( ctx, SSL_VERIFY_PEER, verify_callback ); 
    (*SSL_CTX_set_verify_depth_ptr)( ctx, 4 ); // TODO arbitrary?
#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
    //SSL_CTX_set_options( ctx, SSL_OP_ALL|SSL_OP_NO_SSLv2 );
    (*SSL_CTX_ctrl_ptr)( ctx, SSL_CTRL_OPTIONS, SSL_OP_ALL|SSL_OP_NO_SSLv2, NULL );
#endif
    if((*SSL_CTX_set_cipher_list_ptr)( ctx, cipherlist ) != 1 ) {
        ouch( "Error setting cipher list (no valid ciphers)\n" );
        goto setup_server_ctx_err;
    }
    if(cafile)          free(cafile);
    if(cadir)           free(cadir);
    if(certfile)        free(certfile);
    if(keyfile)         free(keyfile);
    if(cipherlist)      free(cipherlist);
    return ctx;
  setup_server_ctx_err:
    if(cafile)          free(cafile);
    if(cadir)           free(cadir);
    if(certfile)        free(certfile);
    if(keyfile)         free(keyfile);
    if(cipherlist)      free(cipherlist);
	if(ctx)		        (*SSL_CTX_free_ptr)(ctx);
    return NULL;
}


 
#endif
