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

#if defined(HAVE_EXT_KRB5)

#include "condor_auth_kerberos.h"
#include "condor_config.h"
#include "condor_string.h"
#include "CondorError.h"
#include "condor_netdb.h"
#include "subsystem_info.h"
#include "ipv6_hostname.h"

#if defined(DLOPEN_SECURITY_LIBS)
#include <dlfcn.h>
#endif

const char STR_KERBEROS_SERVER_KEYTAB[]   = "KERBEROS_SERVER_KEYTAB";
const char STR_KERBEROS_SERVER_PRINCIPAL[]= "KERBEROS_SERVER_PRINCIPAL";
const char STR_KERBEROS_SERVER_USER[]     = "KERBEROS_SERVER_USER";
const char STR_KERBEROS_SERVER_SERVICE[]  = "KERBEROS_SERVER_SERVICE";
//const char STR_KERBEROS_CLIENT_KEYTAB[]   = "KERBEROS_CLIENT_KEYTAB";
//const char STR_KRB_FORMAT[]             = "FILE:%s/krb_condor_%s.stash";
const char STR_DEFAULT_CONDOR_SERVICE[] = "host";

#define KERBEROS_ABORT   -1
#define KERBEROS_DENY    0
#define KERBEROS_GRANT   1
#define KERBEROS_FORWARD 2
#define KERBEROS_MUTUAL  3
#define KERBEROS_PROCEED 4

std::map<std::string, std::string> * Condor_Auth_Kerberos::RealmMap = 0;
//----------------------------------------------------------------------
// Kerberos Implementation
//----------------------------------------------------------------------

// Symbols from the kerberos libraries
static decltype(&error_message) error_message_ptr = nullptr;
static decltype(&krb5_auth_con_free) krb5_auth_con_free_ptr = nullptr;
static decltype(&krb5_auth_con_genaddrs) krb5_auth_con_genaddrs_ptr = nullptr;
static decltype(&krb5_auth_con_getaddrs) krb5_auth_con_getaddrs_ptr = nullptr;
static decltype(&krb5_auth_con_init) krb5_auth_con_init_ptr = nullptr;
static decltype(&krb5_auth_con_setflags) krb5_auth_con_setflags_ptr = nullptr;
static decltype(&krb5_c_block_size) krb5_c_block_size_ptr = nullptr;
static decltype(&krb5_c_decrypt) krb5_c_decrypt_ptr = nullptr;
static decltype(&krb5_c_encrypt) krb5_c_encrypt_ptr = nullptr;
static decltype(&krb5_c_encrypt_length) krb5_c_encrypt_length_ptr = nullptr;
static decltype(&krb5_cc_close) krb5_cc_close_ptr = nullptr;
static decltype(&krb5_cc_default_name) krb5_cc_default_name_ptr = nullptr;
static decltype(&krb5_cc_get_principal) krb5_cc_get_principal_ptr = nullptr;
static decltype(&krb5_cc_resolve) krb5_cc_resolve_ptr = nullptr;
static decltype(&krb5_copy_keyblock) krb5_copy_keyblock_ptr = nullptr;
static decltype(&krb5_copy_principal) krb5_copy_principal_ptr = nullptr;
static decltype(&krb5_free_addresses) krb5_free_addresses_ptr = nullptr;
static decltype(&krb5_free_ap_rep_enc_part) krb5_free_ap_rep_enc_part_ptr = nullptr;
static decltype(&krb5_free_context) krb5_free_context_ptr = nullptr;
static decltype(&krb5_free_cred_contents) krb5_free_cred_contents_ptr = nullptr;
static decltype(&krb5_free_creds) krb5_free_creds_ptr = nullptr;
static decltype(&krb5_free_keyblock) krb5_free_keyblock_ptr = nullptr;
static decltype(&krb5_free_principal) krb5_free_principal_ptr = nullptr;
static decltype(&krb5_free_ticket) krb5_free_ticket_ptr = nullptr;
static decltype(&krb5_get_credentials) krb5_get_credentials_ptr = nullptr;
static decltype(&krb5_get_init_creds_keytab) krb5_get_init_creds_keytab_ptr = nullptr;
static decltype(&krb5_init_context) krb5_init_context_ptr = nullptr;
static decltype(&krb5_kt_close) krb5_kt_close_ptr = nullptr;
static decltype(&krb5_kt_default) krb5_kt_default_ptr = nullptr;
static decltype(&krb5_kt_default_name) krb5_kt_default_name_ptr = nullptr;
static decltype(&krb5_kt_resolve) krb5_kt_resolve_ptr = nullptr;
static decltype(&krb5_mk_rep) krb5_mk_rep_ptr = nullptr;
static decltype(&krb5_mk_req_extended) krb5_mk_req_extended_ptr = nullptr;
static decltype(&krb5_os_localaddr) krb5_os_localaddr_ptr = nullptr;
static decltype(&krb5_parse_name) krb5_parse_name_ptr = nullptr;
static decltype(&krb5_rd_rep) krb5_rd_rep_ptr = nullptr;
static decltype(&krb5_rd_req) krb5_rd_req_ptr = nullptr;
static decltype(&krb5_sname_to_principal) krb5_sname_to_principal_ptr = nullptr;
static decltype(&krb5_unparse_name) krb5_unparse_name_ptr = nullptr;

bool Condor_Auth_Kerberos::m_initTried = false;
bool Condor_Auth_Kerberos::m_initSuccess = false;

Condor_Auth_Kerberos :: Condor_Auth_Kerberos( ReliSock * sock )
    : Condor_Auth_Base ( sock, CAUTH_KERBEROS ),
	  m_state		   ( ServerReceiveClientReadiness),
	  ticket_          ( NULL),
      krb_context_     ( NULL ),
      auth_context_    ( NULL ),
      krb_principal_   ( NULL ),
      server_          ( NULL ),
      sessionKey_      ( NULL ),
      creds_           ( NULL ),
      ccname_          ( NULL ),
      defaultStash_    ( NULL ),
      keytabName_      ( NULL )
{
	ASSERT( Initialize() == true );
}

Condor_Auth_Kerberos :: ~Condor_Auth_Kerberos()
{
    if (krb_context_) {

        if (auth_context_) {
            (*krb5_auth_con_free_ptr)(krb_context_, auth_context_);
        }

        if (krb_principal_) {
            (*krb5_free_principal_ptr)(krb_context_, krb_principal_);
        }
        
        if (sessionKey_) {
            (*krb5_free_keyblock_ptr)(krb_context_, sessionKey_);
        }
        
        if (server_) {
            (*krb5_free_principal_ptr)(krb_context_, server_);
        }
        
        (*krb5_free_context_ptr)(krb_context_);
    }
    
    if (defaultStash_) {
        free(defaultStash_);
        defaultStash_ = NULL;
    }

    if (ccname_) {
        free(ccname_);
        ccname_ = NULL;
    }
}

bool Condor_Auth_Kerberos::Initialize()
{
	if ( m_initTried ) {
		return m_initSuccess;
	}

#if defined(DLOPEN_SECURITY_LIBS)
	void *dl_hdl;

	if ( (dl_hdl = dlopen(LIBCOM_ERR_SO, RTLD_LAZY)) == NULL ||
		 !(error_message_ptr = reinterpret_cast<decltype(error_message_ptr)>(dlsym(dl_hdl, "error_message"))) ||
		 (dl_hdl = dlopen(LIBKRB5SUPPORT_SO, RTLD_LAZY)) == NULL ||
		 (dl_hdl = dlopen(LIBK5CRYPTO_SO, RTLD_LAZY)) == NULL ||
		 (dl_hdl = dlopen(LIBKRB5_SO, RTLD_LAZY)) == NULL ||
		 (dl_hdl = dlopen(LIBGSSAPI_KRB5_SO, RTLD_LAZY)) == NULL ||
		 !(krb5_auth_con_free_ptr = reinterpret_cast<decltype(krb5_auth_con_free_ptr)>(dlsym(dl_hdl, "krb5_auth_con_free"))) ||
		 !(krb5_auth_con_genaddrs_ptr = reinterpret_cast<decltype(krb5_auth_con_genaddrs_ptr)>(dlsym(dl_hdl, "krb5_auth_con_genaddrs"))) ||
		 !(krb5_auth_con_getaddrs_ptr = reinterpret_cast<decltype(krb5_auth_con_getaddrs_ptr)>(dlsym(dl_hdl, "krb5_auth_con_getaddrs"))) ||
		 !(krb5_auth_con_init_ptr = reinterpret_cast<decltype(krb5_auth_con_init_ptr)>(dlsym(dl_hdl, "krb5_auth_con_init"))) ||
		 !(krb5_auth_con_setflags_ptr = reinterpret_cast<decltype(krb5_auth_con_setflags_ptr)>(dlsym(dl_hdl, "krb5_auth_con_setflags"))) ||
		 !(krb5_c_block_size_ptr = reinterpret_cast<decltype(krb5_c_block_size_ptr)>(dlsym(dl_hdl, "krb5_c_block_size"))) ||
		 !(krb5_c_decrypt_ptr = reinterpret_cast<decltype(krb5_c_decrypt_ptr)>(dlsym(dl_hdl, "krb5_c_decrypt"))) ||
		 !(krb5_c_encrypt_ptr = reinterpret_cast<decltype(krb5_c_encrypt_ptr)>(dlsym(dl_hdl, "krb5_c_encrypt"))) ||
		 !(krb5_c_encrypt_length_ptr = reinterpret_cast<decltype(krb5_c_encrypt_length_ptr)>(dlsym(dl_hdl, "krb5_c_encrypt_length"))) ||
		 !(krb5_cc_close_ptr = reinterpret_cast<decltype(krb5_cc_close_ptr)>(dlsym(dl_hdl, "krb5_cc_close"))) ||
		 !(krb5_cc_default_name_ptr = reinterpret_cast<decltype(krb5_cc_default_name_ptr)>(dlsym(dl_hdl, "krb5_cc_default_name"))) ||
		 !(krb5_cc_get_principal_ptr = reinterpret_cast<decltype(krb5_cc_get_principal_ptr)>(dlsym(dl_hdl, "krb5_cc_get_principal"))) ||
		 !(krb5_cc_resolve_ptr = reinterpret_cast<decltype(krb5_cc_resolve_ptr)>(dlsym(dl_hdl, "krb5_cc_resolve"))) ||
		 !(krb5_copy_keyblock_ptr = reinterpret_cast<decltype(krb5_copy_keyblock_ptr)>(dlsym(dl_hdl, "krb5_copy_keyblock"))) ||
		 !(krb5_copy_principal_ptr = reinterpret_cast<decltype(krb5_copy_principal_ptr)>(dlsym(dl_hdl, "krb5_copy_principal"))) ||
		 !(krb5_free_addresses_ptr = reinterpret_cast<decltype(krb5_free_addresses_ptr)>(dlsym(dl_hdl, "krb5_free_addresses"))) ||
		 !(krb5_free_ap_rep_enc_part_ptr = reinterpret_cast<decltype(krb5_free_ap_rep_enc_part_ptr)>(dlsym(dl_hdl, "krb5_free_ap_rep_enc_part"))) ||
		 !(krb5_free_context_ptr = reinterpret_cast<decltype(krb5_free_context_ptr)>(dlsym(dl_hdl, "krb5_free_context"))) ||
		 !(krb5_free_cred_contents_ptr = reinterpret_cast<decltype(krb5_free_cred_contents_ptr)>(dlsym(dl_hdl, "krb5_free_cred_contents"))) ||
		 !(krb5_free_creds_ptr = reinterpret_cast<decltype(krb5_free_creds_ptr)>(dlsym(dl_hdl, "krb5_free_creds"))) ||
		 !(krb5_free_keyblock_ptr = reinterpret_cast<decltype(krb5_free_keyblock_ptr)>(dlsym(dl_hdl, "krb5_free_keyblock"))) ||
		 !(krb5_free_principal_ptr = reinterpret_cast<decltype(krb5_free_principal_ptr)>(dlsym(dl_hdl, "krb5_free_principal"))) ||
		 !(krb5_free_ticket_ptr = reinterpret_cast<decltype(krb5_free_ticket_ptr)>(dlsym(dl_hdl, "krb5_free_ticket"))) ||
		 !(krb5_get_credentials_ptr = reinterpret_cast<decltype(krb5_get_credentials_ptr)>(dlsym(dl_hdl, "krb5_get_credentials"))) ||
#if defined(KRB5_RESPONDER_QUESTION_PASSWORD)
		 !(krb5_get_init_creds_keytab_ptr = reinterpret_cast<decltype(krb5_get_init_creds_keytab_ptr)>(dlsym(dl_hdl, "krb5_get_init_creds_keytab"))) ||
#else
		 !(krb5_get_init_creds_keytab_ptr = reinterpret_cast<decltype(krb5_get_init_creds_keytab_ptr)>(dlsym(dl_hdl, "krb5_get_init_creds_keytab"))) ||
#endif
		 !(krb5_init_context_ptr = reinterpret_cast<decltype(krb5_init_context_ptr)>(dlsym(dl_hdl, "krb5_init_context"))) ||
		 !(krb5_kt_close_ptr = reinterpret_cast<decltype(krb5_kt_close_ptr)>(dlsym(dl_hdl, "krb5_kt_close"))) ||
		 !(krb5_kt_default_ptr = reinterpret_cast<decltype(krb5_kt_default_ptr)>(dlsym(dl_hdl, "krb5_kt_default"))) ||
		 !(krb5_kt_default_name_ptr = reinterpret_cast<decltype(krb5_kt_default_name_ptr)>(dlsym(dl_hdl, "krb5_kt_default_name"))) ||
		 !(krb5_kt_resolve_ptr = reinterpret_cast<decltype(krb5_kt_resolve_ptr)>(dlsym(dl_hdl, "krb5_kt_resolve"))) ||
		 !(krb5_mk_rep_ptr = reinterpret_cast<decltype(krb5_mk_rep_ptr)>(dlsym(dl_hdl, "krb5_mk_rep"))) ||
		 !(krb5_mk_req_extended_ptr = reinterpret_cast<decltype(krb5_mk_req_extended_ptr)>(dlsym(dl_hdl, "krb5_mk_req_extended"))) ||
		 !(krb5_os_localaddr_ptr = reinterpret_cast<decltype(krb5_os_localaddr_ptr)>(dlsym(dl_hdl, "krb5_os_localaddr"))) ||
		 !(krb5_parse_name_ptr = reinterpret_cast<decltype(krb5_parse_name_ptr)>(dlsym(dl_hdl, "krb5_parse_name"))) ||
		 !(krb5_rd_rep_ptr = reinterpret_cast<decltype(krb5_rd_rep_ptr)>(dlsym(dl_hdl, "krb5_rd_rep"))) ||
		 !(krb5_rd_req_ptr = reinterpret_cast<decltype(krb5_rd_req_ptr)>(dlsym(dl_hdl, "krb5_rd_req"))) ||
		 !(krb5_sname_to_principal_ptr = reinterpret_cast<decltype(krb5_sname_to_principal_ptr)>(dlsym(dl_hdl, "krb5_sname_to_principal"))) ||
		 !(krb5_unparse_name_ptr = reinterpret_cast<decltype(krb5_unparse_name_ptr)>(dlsym(dl_hdl, "krb5_unparse_name")))
		 ) {

		// Error in the dlopen/sym calls, return failure.
		const char *err_msg = dlerror();
		dprintf( D_ALWAYS, "Failed to open Kerberos libraries: %s\n",
				 err_msg ? err_msg : "Unknown error" );
		m_initSuccess = false;
	} else {
		m_initSuccess = true;
	}
#else
	error_message_ptr = error_message;
	krb5_auth_con_free_ptr = krb5_auth_con_free;
	krb5_auth_con_genaddrs_ptr = krb5_auth_con_genaddrs;
	krb5_auth_con_getaddrs_ptr = krb5_auth_con_getaddrs;
	krb5_auth_con_init_ptr = krb5_auth_con_init;
	krb5_auth_con_setflags_ptr = krb5_auth_con_setflags;
	krb5_c_block_size_ptr = krb5_c_block_size;
	krb5_c_decrypt_ptr = krb5_c_decrypt;
	krb5_c_encrypt_ptr = krb5_c_encrypt;
	krb5_c_encrypt_length_ptr = krb5_c_encrypt_length;
	krb5_cc_close_ptr = krb5_cc_close;
	krb5_cc_default_name_ptr = krb5_cc_default_name;
	krb5_cc_get_principal_ptr = krb5_cc_get_principal;
	krb5_cc_resolve_ptr = krb5_cc_resolve;
	krb5_copy_keyblock_ptr = krb5_copy_keyblock;
	krb5_copy_principal_ptr = krb5_copy_principal;
	krb5_free_addresses_ptr = krb5_free_addresses;
	krb5_free_ap_rep_enc_part_ptr = krb5_free_ap_rep_enc_part;
	krb5_free_context_ptr = krb5_free_context;
	krb5_free_cred_contents_ptr = krb5_free_cred_contents;
	krb5_free_creds_ptr = krb5_free_creds;
	krb5_free_keyblock_ptr = krb5_free_keyblock;
	krb5_free_principal_ptr = krb5_free_principal;
	krb5_free_ticket_ptr = krb5_free_ticket;
	krb5_get_credentials_ptr = krb5_get_credentials;
	krb5_get_init_creds_keytab_ptr = krb5_get_init_creds_keytab;
	krb5_init_context_ptr = krb5_init_context;
	krb5_kt_close_ptr = krb5_kt_close;
	krb5_kt_default_ptr = krb5_kt_default;
	krb5_kt_default_name_ptr = krb5_kt_default_name;
	krb5_kt_resolve_ptr = krb5_kt_resolve;
	krb5_mk_rep_ptr = krb5_mk_rep;
	krb5_mk_req_extended_ptr = krb5_mk_req_extended;
	krb5_os_localaddr_ptr = krb5_os_localaddr;
	krb5_parse_name_ptr = krb5_parse_name;
	krb5_rd_rep_ptr = krb5_rd_rep;
	krb5_rd_req_ptr = krb5_rd_req;
	krb5_sname_to_principal_ptr = krb5_sname_to_principal;
	krb5_unparse_name_ptr = krb5_unparse_name;

	m_initSuccess = true;
#endif

	m_initTried = true;
	return m_initSuccess;
}

int Condor_Auth_Kerberos :: authenticate(const char * /* remoteHost */, CondorError* /* errstack */, bool /*non_blocking*/)
{
    int status = 0;

	if ( mySock_->isClient() ) {
		// we are the client.
		// initialize everything if needed.
		if (init_kerberos_context() && init_server_info()) {
            
			if (isDaemon() || ( get_mySubSystem()->isDaemon() && get_mySubSystem()->isTrusted() ) ) {
				status = init_daemon();
			} else {
				status = init_user();
			}
		} else {
			status = FALSE;
		}

		int message = (status == TRUE? KERBEROS_PROCEED : KERBEROS_ABORT);

		mySock_->encode();
		if (!mySock_->code(message) || !mySock_->end_of_message()) {
			status = FALSE;
		} else {
			if (message == KERBEROS_PROCEED) {
				// We are ready to go
				status = authenticate_client_kerberos();
			} else {
				status = FALSE;
			}
		}
	} else {
		// enter the server state machine
		m_state = ServerReceiveClientReadiness;
		status = WouldBlock;
	}

    return( status );
}


int Condor_Auth_Kerberos::authenticate_continue(CondorError* errstack, bool non_blocking)
{

	dprintf(D_SECURITY, "KERBEROS: entered authenticate_continue, state==%i\n", (int)m_state);

	CondorAuthKerberosRetval retval = Continue;
	while (retval == Continue)
	{
		switch (m_state)
		{
		case ServerReceiveClientReadiness:
			retval = doServerReceiveClientReadiness(errstack, non_blocking);
			break;
		case ServerAuthenticate:
			retval = doServerAuthenticate(errstack, non_blocking);
			break;
		case ServerReceiveClientSuccessCode:
			retval = doServerReceiveClientSuccessCode(errstack, non_blocking);
			break;
		default:
			retval = Fail;
			break;
		}
	}

	dprintf(D_SECURITY, "KERBEROS: leaving authenticate_continue, state==%i, return=%i\n", (int)m_state, (int)retval);
	return static_cast<int>(retval);
}



Condor_Auth_Kerberos::CondorAuthKerberosRetval
Condor_Auth_Kerberos::doServerReceiveClientReadiness(CondorError* /*errstack*/, bool non_blocking)
{

	if (non_blocking && !mySock_->readReady())
	{
		dprintf(D_NETWORK, "Returning to DC as read would block in KRB::doServerReceiveClientReadiness\n");
		return WouldBlock;
	}

	int status = authenticate_server_kerberos_0();
	if(status) {
		m_state = ServerAuthenticate;
		return Continue;
	}

	return Fail;
}


Condor_Auth_Kerberos::CondorAuthKerberosRetval
Condor_Auth_Kerberos::doServerAuthenticate(CondorError* /*errstack*/, bool non_blocking)
{
	if (non_blocking && !mySock_->readReady())
	{
		dprintf(D_NETWORK, "Returning to DC as read would block in KRB::doServerAuthenticate\n");
		return WouldBlock;
	}

	int status = authenticate_server_kerberos_1();
	if(status) {
		m_state = ServerReceiveClientSuccessCode;
		return Continue;
	}

	return Fail;
}

Condor_Auth_Kerberos::CondorAuthKerberosRetval
Condor_Auth_Kerberos::doServerReceiveClientSuccessCode(CondorError* /*errstack*/, bool non_blocking)
{
	if (non_blocking && !mySock_->readReady())
	{
		dprintf(D_NETWORK, "Returning to DC as read would block in KRB::doServerReceiveClientSuccessCode\n");
		return WouldBlock;
	}

	int status = authenticate_server_kerberos_2();
	if(status) {
		return Success;
	}

	return Fail;
}


int Condor_Auth_Kerberos :: wrap(const char*  input,
                                 int    input_len, 
                                 char*& output, 
                                 int&   output_len)
{
    krb5_error_code code;
    krb5_data       in_data;
    krb5_enc_data   out_data;
    int             index, tmp;

	size_t blocksize, encrypted_length;
	char* encrypted_data = 0;

	// make a blank initialization vector
	code = krb5_c_block_size_ptr(krb_context_, sessionKey_->enctype, &blocksize);
	if (code) {
		// err
	}

    // Make the input buffer
    in_data.data = const_cast<char*>(input);
    in_data.length = input_len;

    // Make the output buffer
    code = krb5_c_encrypt_length_ptr(krb_context_, sessionKey_->enctype, input_len, &encrypted_length);
	if(code) {
		// err
	}

	encrypted_data = (char*)malloc(encrypted_length);
	if (!encrypted_length) {
		// err
	}

    out_data.ciphertext.data = (char*)encrypted_data;
	out_data.ciphertext.length = encrypted_length;

    if ((code = krb5_c_encrypt_ptr(krb_context_, sessionKey_, 1024, /* key usage */
				0, &in_data, &out_data)) != 0) {			/* 0 = no ivec */
        output     = 0;
        output_len = 0;
        if (out_data.ciphertext.data) {    
            free(out_data.ciphertext.data);
        }
        dprintf( D_ALWAYS, "KERBEROS: %s\n", error_message_ptr(code) );
        return false;
    }
    
    output_len = sizeof(out_data.enctype) +
        sizeof(out_data.kvno)    + 
        sizeof(out_data.ciphertext.length) +
        out_data.ciphertext.length;
    
    output = (char *) malloc(output_len);
    index = 0;
    tmp = htonl(out_data.enctype);
    memcpy(output + index, &tmp, sizeof(out_data.enctype));
    index += sizeof(out_data.enctype);

    tmp = htonl(out_data.kvno);
    memcpy(output + index, &tmp, sizeof(out_data.kvno));
    index += sizeof(out_data.kvno);

    tmp = htonl(out_data.ciphertext.length);
    memcpy(output + index, &tmp, sizeof(out_data.ciphertext.length));
    index += sizeof(out_data.ciphertext.length);

    if (out_data.ciphertext.data) {    
	memcpy(output + index, out_data.ciphertext.data,
		out_data.ciphertext.length);
        free(out_data.ciphertext.data);
    }

    return TRUE;
}

int Condor_Auth_Kerberos :: unwrap(const char*  input,
                                   int    /* input_len */, 
                                   char*& output, 
                                   int& output_len)
{
    krb5_error_code code;
    krb5_data       out_data;
    krb5_enc_data   enc_data;
    int index = 0, tmp;
	size_t blocksize;
    out_data.data = 0;
    out_data.length = 0;

    memcpy(&tmp, input, sizeof(enc_data.enctype));
    enc_data.enctype = ntohl(tmp);
    index += sizeof(enc_data.enctype);

    memcpy(&tmp, input + index, sizeof(enc_data.kvno));
    enc_data.kvno = ntohl(tmp);
    index += sizeof(enc_data.kvno);

    memcpy(&tmp, input + index, sizeof(enc_data.ciphertext.length));
    enc_data.ciphertext.length = ntohl(tmp);
    index += sizeof(enc_data.ciphertext.length);

    enc_data.ciphertext.data = const_cast<char*>(input) + index;

	// DEBUG
	dprintf (D_FULLDEBUG, "KERBEROS: input.enctype (%i) and session.enctype (%i)\n",
			enc_data.enctype, sessionKey_->enctype);

	// make a blank initialization vector
	code = krb5_c_block_size_ptr(krb_context_, sessionKey_->enctype, &blocksize);
	if (code) {
		dprintf(D_ALWAYS, "AUTH_ERROR: %s\n", error_message_ptr(code));
	}

    out_data.length = enc_data.ciphertext.length;
	out_data.data = (char*)malloc(out_data.length);

	if ((code = krb5_c_decrypt_ptr(krb_context_, sessionKey_, 1024, /* key usage */
				0, &enc_data, &out_data))!=0) {			/* 0 = no ivec */

	//if (code = krb5_decrypt_data_ptr(krb_context_, sessionKey_, 0, &enc_data, &out_data)) {
        output_len = 0;
        output = 0;
        dprintf( D_ALWAYS, "KERBEROS: %s\n", error_message_ptr(code) );
        if (out_data.data) {
            free(out_data.data);
        }
        return false;
    }
    
    output_len = out_data.length;
    output = (char *) malloc(output_len);
    memcpy(output, out_data.data, output_len);

    if (out_data.data) {
        free(out_data.data);
    }
    return true;
}

//----------------------------------------------------------------------
// Authenticate to server as a daemon (i.e. using the keytab)
//----------------------------------------------------------------------
int Condor_Auth_Kerberos :: init_daemon()
{
    int            code, rc = TRUE;
    priv_state     priv;

    char *         daemonPrincipal = 0;

    krb5_keytab    keytab = 0;

	// init some member vars
    creds_ = (krb5_creds *) malloc(sizeof(krb5_creds));
    keytabName_ = param(STR_KERBEROS_SERVER_KEYTAB);

	// needed for extracting service name
	char * tmpsname = 0;
	std::string sname;

    memset(creds_, 0, sizeof(krb5_creds));

    //------------------------------------------
    // Initialize the principal for daemon (properly :-)
    //------------------------------------------
	// this means using a keytab to obtain a tgt.
	//
	// in krb5-1.2 and earlier this was done with krb5_get_in_tkt_keytab()
	//
	// in krb5-1.3 and
	// later this should be done with krb5_get_init_creds_keytab

	// select a server principal
    daemonPrincipal = param(STR_KERBEROS_SERVER_PRINCIPAL);

	if (daemonPrincipal) {
		// it was defined explicitly in the config file
		if ((code = krb5_parse_name_ptr(krb_context_, daemonPrincipal, &krb_principal_))) {
			free(daemonPrincipal);
			goto error;
		}
	} else {
		// not defined in config, let's construct one from a service name
		daemonPrincipal = param(STR_KERBEROS_SERVER_SERVICE);
		if (!daemonPrincipal) {
			// use the host credential if no service was specified
			daemonPrincipal = strdup(STR_DEFAULT_CONDOR_SERVICE);
		}

    	if ((code = krb5_sname_to_principal_ptr(krb_context_, 
                                        	NULL, 
                                        	daemonPrincipal,
                                        	KRB5_NT_SRV_HST, 
                                        	&krb_principal_))) {
			free(daemonPrincipal);
			goto error;
		}
	}
	free(daemonPrincipal);
	daemonPrincipal = 0;

   
	dprintf_krb5_principal( D_SECURITY, "init_daemon: client principal is '%s'\n", krb_principal_);

    if (keytabName_) {
    	dprintf(D_SECURITY, "init_daemon: Using keytab %s\n", keytabName_);
        code = krb5_kt_resolve_ptr(krb_context_, keytabName_, &keytab);
    } else {
		char defktname[_POSIX_PATH_MAX];
		krb5_kt_default_name_ptr(krb_context_, defktname, _POSIX_PATH_MAX);
    	dprintf(D_SECURITY, "init_daemon: Using default keytab %s\n", defktname);
        code = krb5_kt_default_ptr(krb_context_, &keytab);
    }
	if (code) {
		goto error;
	}

	// get the service name out of the member variable server_
	tmpsname = 0;
	code = krb5_unparse_name_ptr(krb_context_, server_, &tmpsname);
	if (code) {
		goto error;
	}

	// copy it into a mystring for stack cleanup purposes
	sname = tmpsname;
	free (tmpsname);

	dprintf(D_SECURITY, "init_daemon: Trying to get tgt credential for service %s\n", sname.c_str());

	priv = set_root_priv();   // Get the old privilige
	code = krb5_get_init_creds_keytab_ptr(krb_context_, creds_, krb_principal_, keytab, 0, const_cast<char*>(sname.c_str()), 0);
	set_priv(priv);
	if(code) {
		goto error;
	}

	dprintf_krb5_principal( D_SECURITY, "init_daemon: gic_kt creds_->client is '%s'\n", creds_->client );
	dprintf_krb5_principal( D_SECURITY, "init_daemon: gic_kt creds_->server is '%s'\n", creds_->server );

    dprintf(D_SECURITY, "Success..........................\n");
    
    rc = TRUE;

    goto cleanup;
    
 error:
    
    dprintf(D_ALWAYS, "AUTH_ERROR: %s\n", error_message_ptr(code));

    rc = FALSE;
    
 cleanup:
    
    if (keytab) {
        krb5_kt_close_ptr(krb_context_, keytab);
    }
    
    return rc;
}

int Condor_Auth_Kerberos :: init_user()
{
    int             rc = TRUE;
    krb5_error_code code;
    krb5_ccache     ccache = (krb5_ccache) NULL;
    krb5_creds      mcreds;

    memset(&mcreds, 0, sizeof(mcreds));

    dprintf(D_SECURITY, "Acquiring credential for user\n");

    //------------------------------------------
    // First, try the default credential cache
    //------------------------------------------
    ccname_ = strdup(krb5_cc_default_name_ptr(krb_context_));
    
    if ((code = krb5_cc_resolve_ptr(krb_context_, ccname_, &ccache))) {
        goto error;
    }
    
    //------------------------------------------
    // Get principal info
    //------------------------------------------
    if ((code = krb5_cc_get_principal_ptr(krb_context_, ccache, &krb_principal_))) {
        goto error;
    }

    if ((code = krb5_copy_principal_ptr(krb_context_,krb_principal_,&mcreds.client))){
        goto error;
    }
    
    if ((code = krb5_copy_principal_ptr(krb_context_, server_, &mcreds.server))) {
        goto error;
    }
    
   	dprintf_krb5_principal ( D_FULLDEBUG, "init_user: pre mcreds->client is '%s'\n", mcreds.client);
	dprintf_krb5_principal ( D_FULLDEBUG, "init_user: pre mcreds->server is '%s'\n", mcreds.server);
	if (creds_) {
		dprintf_krb5_principal ( D_FULLDEBUG, "init_user: pre creds_->client is '%s'\n", creds_->client);
		dprintf_krb5_principal ( D_FULLDEBUG, "init_user: pre creds_->server is '%s'\n", creds_->server);
	} else {
		dprintf ( D_FULLDEBUG, "init_user: pre creds_ is NULL\n");
	}		

    if ((code = krb5_get_credentials_ptr(krb_context_, 0, ccache, &mcreds, &creds_))) {
        goto error;
    }                                   

	dprintf_krb5_principal ( D_FULLDEBUG, "init_user: post mcreds->client is '%s'\n", mcreds.client);
	dprintf_krb5_principal ( D_FULLDEBUG, "init_user: post mcreds->server is '%s'\n", mcreds.server);
	if (creds_) {
		dprintf_krb5_principal ( D_FULLDEBUG, "init_user: post creds_->client is '%s'\n", creds_->client);
		dprintf_krb5_principal ( D_FULLDEBUG, "init_user: post creds_->server is '%s'\n", creds_->server);
	} else {
		dprintf ( D_FULLDEBUG, "init_user: post creds_ is NULL\n");
	}		
    
    dprintf(D_SECURITY, "Successfully located credential cache\n");
    
    rc = TRUE;
    goto cleanup;
    
 error:
    dprintf( D_ALWAYS, "KERBEROS: %s\n", error_message_ptr(code) );
    rc = FALSE;

 cleanup:

    krb5_free_cred_contents_ptr(krb_context_, &mcreds);

    if (ccache) {  // maybe should destroy this
        krb5_cc_close_ptr(krb_context_, ccache);
    }
    return rc;
}
    
int Condor_Auth_Kerberos :: authenticate_client_kerberos()
{
    krb5_error_code        code;
    krb5_flags             flags;
    krb5_data              request;
    int                    reply, rc = FALSE;
    
    request.data = 0;
    request.length = 0;
    //------------------------------------------
    // Set up the flags
    //------------------------------------------
    flags = AP_OPTS_MUTUAL_REQUIRED | AP_OPTS_USE_SUBKEY;
    
    //------------------------------------------
    // Load local addresses
    //------------------------------------------
	assert(creds_);
    if (creds_->addresses == NULL) {
		dprintf ( D_SECURITY, "KERBEROS: creds_->addresses == NULL\n");
        if ((code = krb5_os_localaddr_ptr(krb_context_, &(creds_->addresses)))) {
            goto error;
        }
    }
    
	dprintf_krb5_principal ( D_FULLDEBUG, "KERBEROS: creds_->client is '%s'\n", creds_->client);
	dprintf_krb5_principal ( D_FULLDEBUG, "KERBEROS: creds_->server is '%s'\n", creds_->server);
   
    //------------------------------------------
    // Let's create the KRB_AP_REQ message
    //------------------------------------------    
    if ((code = krb5_mk_req_extended_ptr(krb_context_, 
                                    &auth_context_, 
                                    flags,
                                    0, 
                                    creds_, 
                                    &request))) {
        goto error;
    }
    
    // Send out the request
    if ((reply = send_request_and_receive_reply(&request)) != KERBEROS_MUTUAL) {
        dprintf( D_ALWAYS, "KERBEROS: Could not authenticate!\n" );
        return FALSE;
    }
    
    //------------------------------------------
    // Now, mutual authenticate
    //------------------------------------------
    reply = client_mutual_authenticate();

    switch (reply) 
        {
        case KERBEROS_DENY:
            dprintf( D_ALWAYS, "KERBEROS: Authentication failed\n" );
            return FALSE;
            break; // unreachable
        case KERBEROS_FORWARD:
            // We need to forward the credentials
            // We could do a fast forwarding (i.e stashing, if client/server
            // are located on the same machine. However, I want to keep the
            // forwarding mechanism clean, so, we use krb5_fwd_tgt_creds
            // regardless of where client/server are located
            
            // This is an implict GRANT
        case KERBEROS_GRANT:
            break; 
        default:
            dprintf( D_ALWAYS, "KERBEROS: Response is invalid\n" );
            break;
        }
    
    //------------------------------------------
    // Success, do some cleanup
    //------------------------------------------
    setRemoteAddress();
    
    //------------------------------------------
    // Store the session key for encryption
    //------------------------------------------
    if ((code = krb5_copy_keyblock_ptr(krb_context_, &(creds_->keyblock), &sessionKey_))) {
        goto error;			  
    } 

    rc = TRUE;
    goto cleanup;
    
 error:
    dprintf( D_ALWAYS, "KERBEROS: %s\n", error_message_ptr(code) );
    // Abort
    mySock_->encode();
    reply = KERBEROS_ABORT;
    if (!mySock_->code(reply) || !mySock_->end_of_message()) {
        dprintf( D_ALWAYS, "KERBEROS: Failed to send ABORT message.\n");
    }

    rc = FALSE;
    
 cleanup:
    
   krb5_free_creds_ptr(krb_context_, creds_);
    
    if (request.data) {
        free(request.data);
    }
    
    return rc;
}



int Condor_Auth_Kerberos :: authenticate_server_kerberos_0()
{
	// assume failure
	CondorAuthKerberosRetval status = Fail;

	int ready;
	mySock_->decode();
	if (mySock_->code(ready) && mySock_->end_of_message()) {
		if (ready == KERBEROS_PROCEED) {
			dprintf(D_SECURITY,"About to authenticate client using Kerberos\n" );
			// initialize everything if needed.
			if (init_kerberos_context() && init_server_info()) {
				m_state = ServerAuthenticate;
				status = Continue;
			}
		}
	}

	return status;
}


int Condor_Auth_Kerberos :: authenticate_server_kerberos_1()
{
    krb5_error_code   code;
    krb5_flags        flags = 0;
    krb5_data         request, reply;
    priv_state        priv;
    krb5_keytab       keytab = 0;
    int               message;

    ticket_ = NULL;
    request.data = 0;
    reply.data   = 0;
    
    keytabName_ = param(STR_KERBEROS_SERVER_KEYTAB);

    //------------------------------------------
    // Getting keytab info
    //------------------------------------------
    if (keytabName_) {
        code = krb5_kt_resolve_ptr(krb_context_, keytabName_, &keytab);
    }
    else {
        code = krb5_kt_default_ptr(krb_context_, &keytab);
    }
    
    if (code) {
        dprintf( D_ALWAYS, "1: Kerberos server authentication error:%s\n",
				 error_message_ptr(code) );
        goto error;
    }
    
    //------------------------------------------
    // Get te KRB_AP_REQ message
    //------------------------------------------
    if(read_request(&request) == FALSE) {
        dprintf( D_ALWAYS, "KERBEROS: Server is unable to read request\n" );
        goto error;
    }
    
	dprintf( D_SECURITY, "Reading kerberos request object (krb5_rd_req)\n");

	dprintf_krb5_principal( D_FULLDEBUG, "KERBEROS: krb_principal_ is '%s'\n", krb_principal_);

 priv = set_root_priv();   // Get the old privilige
    
 if ((code = krb5_rd_req_ptr(krb_context_,
                           &auth_context_,
                           &request,
                           //krb_principal_,
						   NULL,
                           keytab,
                           &flags,
                           &ticket_))) {
        set_priv(priv);   // Reset
        dprintf( D_ALWAYS, "2: Kerberos server authentication error:%s\n",
				 error_message_ptr(code) );
        goto error;
    }
    set_priv(priv);   // Reset
    
	dprintf ( D_FULLDEBUG, "KERBEROS: krb5_rd_req done.\n");

    //------------------------------------------
    // Mutual authentication is always required
    //------------------------------------------
    if ((code = krb5_mk_rep_ptr(krb_context_, auth_context_, &reply))) {
        dprintf( D_ALWAYS, "3: Kerberos server authentication error:%s\n",
                           error_message_ptr(code) );
        goto error;
    }

    mySock_->encode();
    message = KERBEROS_MUTUAL;
    if (!mySock_->code(message) || !mySock_->end_of_message()) {
        goto error;
    }

    // send the message
    if (send_request(&reply) != KERBEROS_PROCEED) {
	// skip the error label and go straight to cleanup, as error tries to
	// send a message in the same "phase" of the wire protocol that we just
	// failed to use.
        goto cleanup;
    }


    // this phase has succeeded.  free things we no longer need.  leave all
    // state in ticket_ member variable.

    if (keytab) {
        krb5_kt_close_ptr(krb_context_, keytab);
    }

    if (request.data) {
        free(request.data);
    }

    if (reply.data) {
        free(reply.data);
    }

    m_state = ServerReceiveClientSuccessCode;
    return Continue;

 error:
    message = KERBEROS_DENY;
    
    mySock_->encode();
    if ((!mySock_->code(message)) || (!mySock_->end_of_message())) {
        dprintf( D_ALWAYS, "KERBEROS: Failed to send response message!\n" );
    }

 cleanup:
    //------------------------------------------
    // Free up anything we allocated since we are done (failed).
    //------------------------------------------
    if (ticket_) {
        krb5_free_ticket_ptr(krb_context_, ticket_);
    }

    if (keytab) {
        krb5_kt_close_ptr(krb_context_, keytab);
    }

    if (request.data) {
        free(request.data);
    }

    if (reply.data) {
        free(reply.data);
    }

    return Fail;
}


int Condor_Auth_Kerberos :: authenticate_server_kerberos_2()
{
    int message;
    int rc = FALSE;
    krb5_error_code   code;

    //------------------------------------------
    // Next, wait for response
    //------------------------------------------
    message = KERBEROS_DENY;
    mySock_->decode();
    if ((!mySock_->code(message)) || (!mySock_->end_of_message())) {
        dprintf(D_SECURITY, "KERBEROS: Failed to receive response from client\n");
        // ENTER FAILURE MODE
    }

    //------------------------------------------
    // extract client addresses
    //------------------------------------------
    if (ticket_->enc_part2->caddrs) {
        struct in_addr in;
        memcpy(&(in.s_addr), ticket_->enc_part2->caddrs[0]->contents, sizeof(in_addr));
        
        setRemoteHost(inet_ntoa(in));
    
        dprintf(D_SECURITY, "Client address is %s\n", getRemoteHost());
    }    

    // First, map the name
    if (!map_kerberos_name(&(ticket_->enc_part2->client))) {
        dprintf(D_SECURITY, "Unable to map Kerberos name\n");
        goto error;
    }

    // copy the session key
    if ((code = krb5_copy_keyblock_ptr(krb_context_, 
                                  ticket_->enc_part2->session,
                                  &sessionKey_))){
        dprintf(D_SECURITY, "4: Kerberos server authentication error:%s\n", error_message_ptr(code));
        goto error;
    }
    
    //------------------------------------------
    // Tell the other side the good news
    //------------------------------------------
    message = KERBEROS_GRANT;

    mySock_->encode();
    if ((!mySock_->code(message)) || (!mySock_->end_of_message())) {
        dprintf(D_ALWAYS, "Failed to send KERBEROS_GRANT response\n");
	// skip the error label and go straight to cleanup, as error tries to
	// send a message in the same "phase" of the wire protocol that we just
	// failed to use.
        goto cleanup;
    }
    
    //------------------------------------------
    // We are now authenticated!
    //------------------------------------------
    dprintf(D_SECURITY, "User %s is now authenticated!\n", getRemoteUser());
    
    rc = TRUE;
    
    goto cleanup;
    
 error:
    message = KERBEROS_DENY;
    
    mySock_->encode();
    if ((!mySock_->code(message)) || (!mySock_->end_of_message())) {
        dprintf( D_ALWAYS, "KERBEROS: Failed to send response message!\n" );
    }
    
 cleanup:
    
    //------------------------------------------
    // Free up some stuff
    //------------------------------------------
    krb5_free_ticket_ptr(krb_context_, ticket_);

    return rc;
}



//----------------------------------------------------------------------
// Mututal authentication
//----------------------------------------------------------------------
int Condor_Auth_Kerberos :: client_mutual_authenticate()
{
    krb5_ap_rep_enc_part * rep = NULL;
    krb5_error_code        code;
    krb5_data              request;
    int reply            = KERBEROS_DENY;
    int message;
    
    if (read_request(&request) == FALSE) {
        return KERBEROS_DENY;
    }
    
    if ((code = krb5_rd_rep_ptr(krb_context_, auth_context_, &request, &rep))) {
        goto error;
    }
    
    if (rep) {
        krb5_free_ap_rep_enc_part_ptr(krb_context_, rep);
    }
    
    message = KERBEROS_GRANT;
    mySock_->encode();
    if (!(mySock_->code(message)) || !(mySock_->end_of_message())) {
        return KERBEROS_DENY;
    }
    
    mySock_->decode();
    if (!(mySock_->code(reply)) || !(mySock_->end_of_message())) {
        return KERBEROS_DENY;
    }
    
    free(request.data);
    
    return reply;
 error:
    free(request.data);
    
    dprintf( D_ALWAYS, "KERBEROS: %s\n", error_message_ptr(code) );
    return KERBEROS_DENY;
}

//----------------------------------------------------------------------
// Initialze some general structures for kerberos
//----------------------------------------------------------------------
int Condor_Auth_Kerberos :: init_kerberos_context()
{
    krb5_error_code code = 0;

    // kerberos context_
    if (krb_context_ == NULL) {
        if ((code = krb5_init_context_ptr(&krb_context_))) {
            goto error;
        }
    }

    if ((code = krb5_auth_con_init_ptr(krb_context_, &auth_context_))) {
        goto error;
    }

    if ((code = krb5_auth_con_setflags_ptr(krb_context_, 
                                      auth_context_, 
                                      KRB5_AUTH_CONTEXT_DO_SEQUENCE))) {
        goto error;
    }
        
    if ((code = krb5_auth_con_genaddrs_ptr(krb_context_, 
                                      auth_context_, 
                                      mySock_->get_file_desc(),
                                      KRB5_AUTH_CONTEXT_GENERATE_LOCAL_FULL_ADDR|
                                      KRB5_AUTH_CONTEXT_GENERATE_REMOTE_FULL_ADDR
                                      ))) {
        goto error;
    }

#if !defined(DARWIN)
    // Passing nullptr causes a crash on macOS, though the docs say it's
    // allowed. This call should be a noop, but just in case it can fail
    // and that's a useful indicator of problems on other platforms, leave
    // it in place for them.
    if ((code = krb5_auth_con_getaddrs_ptr(krb_context_,
                                      auth_context_,
                                      nullptr,
                                      nullptr))) {
        goto error;
    }
#endif

    // stash location
    defaultStash_ = param(STR_CONDOR_CACHE_DIR);

    if (defaultStash_ == NULL) {
        defaultStash_ = strdup(STR_DEFAULT_CONDOR_SPOOL);
    }
    
    return TRUE;
 error:
    dprintf( D_ALWAYS, "Unable to initialize kerberos: %s\n",
			 error_message_ptr(code) );
    return FALSE;
}

int Condor_Auth_Kerberos :: map_kerberos_name(krb5_principal * princ_to_map)
{
    krb5_error_code code;
    char *          client = NULL;

    //------------------------------------------
    // Decode the client name
    //------------------------------------------    
    if ((code = krb5_unparse_name_ptr(krb_context_, 
                                 *princ_to_map, 
                                 &client))){
		dprintf(D_ALWAYS, "%s\n", error_message_ptr(code));
		return FALSE;
    } 
    else {
		dprintf( D_SECURITY, "KERBEROS: krb5_unparse_name: %s\n", client );

		char * user = 0;
		char * at_sign = strchr(client, '@');

		// first, see if the principal up to the @ sign is the same as
		// STR_KERBEROS_SERVER_PRINCIPAL
		char * server_princ = param(STR_KERBEROS_SERVER_PRINCIPAL);
		if (server_princ) {
			dprintf ( D_SECURITY, "KERBEROS: param server princ: %s\n", server_princ );
			if (strcmp(client, server_princ) == 0) {
				user = param(STR_KERBEROS_SERVER_USER);
				if (user) {
					dprintf ( D_SECURITY, "KERBEROS: mapped to user: %s\n", user );
				}
			}
		}

		if (!user) {
			dprintf ( D_SECURITY, "KERBEROS: no user yet determined, will grab up to slash\n" );
			char * tmp;
			if ((tmp = strchr( client, '/')) == NULL) {
				tmp = at_sign;
			}
			int user_len = tmp - client;
			user = (char*) malloc( user_len + 1 );
			ASSERT( user );
			strncpy ( user, client, user_len );
			user[user_len] = '\0';
			dprintf ( D_SECURITY, "KERBEROS: picked user: %s\n", user );
		}

		char * service = 0;
		service = param(STR_KERBEROS_SERVER_SERVICE);
		if (!service) {
			service = strdup(STR_DEFAULT_CONDOR_SERVICE);
		}
		// hack for now - map the "host" user to the condor user
		if ((strcmp(user, service) == 0)) {
			free(user);
			user = param(STR_KERBEROS_SERVER_USER);
			if (!user) {
				user = strdup(STR_DEFAULT_CONDOR_USER);
			}
			dprintf ( D_SECURITY, "KERBEROS: remapping '%s' to '%s'\n", service, user );
		}
 		setRemoteUser(user);
		setAuthenticatedName(client);
		free(user);
		user = 0;
		free(service);
		service = 0;
		free(server_princ);

		if (!map_domain_name(at_sign+1)) {
			return FALSE;
		}

		dprintf(D_SECURITY, "Client is %s@%s\n", getRemoteUser(), getRemoteDomain());
	}

	return TRUE;
}


int Condor_Auth_Kerberos :: map_domain_name(const char * domain)
{
    if (RealmMap == 0) {
        init_realm_mapping();
        // it's okay if it returns false
    }

    // two cases, if domain is the same as the current uid domain,
    // then we are okay, other wise, see if we have a map
    if (RealmMap) {
		auto itr = RealmMap->find(domain);
		if (itr != RealmMap->end()) {
			if (IsFulldebug(D_SECURITY)) {
				dprintf (D_SECURITY, "KERBEROS: mapping realm %s to domain %s.\n", 
					domain, itr->second.c_str());
			}
            setRemoteDomain(itr->second.c_str());
            return TRUE;
        } else {
			// if the map exists, they must be listed.  and they're NOT!
			return FALSE;
		}
    }

    // if there is no map, we just allow realm -> domain.
	if (IsDebugVerbose(D_SECURITY)) {
		dprintf (D_SECURITY, "KERBEROS: mapping realm %s to domain %s.\n", 
			domain, domain);
	}
	setRemoteDomain(domain);
	return TRUE;

}

int Condor_Auth_Kerberos :: init_realm_mapping()
{
    FILE *fd;
    char * buffer;
    char * filename = param( "KERBEROS_MAP_FILE" );

    if (RealmMap) {
        delete RealmMap;
		RealmMap = NULL;
    }

    if ( !(fd = safe_fopen_wrapper_follow(  filename, "r" ))  ) {
        dprintf( D_SECURITY, "unable to open map file %s, errno %d\n", 
                 filename, errno );
		free(filename);
        RealmMap = NULL;
		return FALSE;
    } else {

		RealmMap = new std::map<std::string, std::string>();

		int lineno = 0;
		while ((buffer = getline_trim(fd, lineno, GETLINE_TRIM_SIMPLE_CONTINUATION))) {
        	char * token;
        	token = strtok(buffer, "= ");
        	if(token) {
				std::string from = token;

				token = strtok(NULL, "= ");
				if(token) {
					(*RealmMap)[from] = token;
				} else {
					dprintf (D_ALWAYS, "KERBEROS: bad map (%s), no domain after '=': %s\n",
						filename, buffer);
				}

			} else {
				dprintf (D_ALWAYS, "KERBEROS: bad map (%s), missing '=' separator: %s\n",
					filename, buffer);
			}
		}

		fclose(fd);

		free(filename);
		return TRUE;
	}
}
   
int Condor_Auth_Kerberos :: send_request(krb5_data * request)
{
    int reply = KERBEROS_DENY;
    int message = KERBEROS_PROCEED;
    //------------------------------------------
    // Send the AP_REQ object
    //------------------------------------------
    mySock_->encode();
    
    if (!mySock_->code(message) || !mySock_->code(request->length)) {
        dprintf(D_SECURITY, "Faile to send request length\n");
        return reply;
    }
    
    if (!(mySock_->put_bytes(request->data, request->length)) ||
        !(mySock_->end_of_message())) {
        dprintf(D_SECURITY, "Faile to send request data\n");
        return reply;
    }
    return KERBEROS_PROCEED;
}

int Condor_Auth_Kerberos :: send_request_and_receive_reply(krb5_data * request)
{

    if(send_request(request) != KERBEROS_PROCEED) {
        return KERBEROS_DENY;
    }

    //------------------------------------------
    // Next, wait for response
    //------------------------------------------
    mySock_->decode();
    
    int reply = KERBEROS_DENY;
    if ((!mySock_->code(reply)) || (!mySock_->end_of_message())) {
        dprintf(D_SECURITY, "Failed to receive response from server\n");
        return KERBEROS_DENY;
    }// Resturn buffer size
    
    return reply;
}

int Condor_Auth_Kerberos :: init_server_info()
{
    // First, where to find cache
    //if (defaultStash_) {
    //  ccname_  = new char[MAXPATHLEN];
    //  sprintf(ccname_, STR_KRB_FORMAT, defaultStash_, my_username());
    //}
    //else {
    // Exception!
    //  dprintf(D_ALWAYS,"KERBEROS: Unable to stash ticket -- STASH directory is not defined!\n");
    //}

    int err = 0;

    // if we are client side, figure out remote server_ credentials
    if (mySock_->isClient()) {

		std::string remoteName = get_hostname(mySock_->peer_addr());

        char *service = param(STR_KERBEROS_SERVER_SERVICE);
        if (!service)
            service = strdup(STR_DEFAULT_CONDOR_SERVICE);

        err = krb5_sname_to_principal_ptr(krb_context_, remoteName.c_str(), service, KRB5_NT_SRV_HST, &server_);
        dprintf(D_SECURITY, "KERBEROS: get remote server principal for \"%s/%s\"%s\n",
                service, remoteName.c_str(), err ? " FAILED" : "");

        if (!err)
            err = !map_kerberos_name(&server_);

    } else { // we are the server itself

        char * principal = param(STR_KERBEROS_SERVER_PRINCIPAL);

        // if server principal is set then this overrides detection
        if (principal) {

            err = krb5_parse_name_ptr(krb_context_, principal, &krb_principal_);
            dprintf(D_SECURITY, "KERBEROS: set local server principal from %s = \"%s\"%s\n",
                    STR_KERBEROS_SERVER_PRINCIPAL, principal, err ? " FAILED" : "");

            free (principal);

        } else { // server side but no principal set, so get from service (note this is duplicated in init_daemon)

            char *service = param(STR_KERBEROS_SERVER_SERVICE);
            if (!service)
                service = strdup(STR_DEFAULT_CONDOR_SERVICE);

            err = krb5_sname_to_principal_ptr(krb_context_, NULL, service, KRB5_NT_SRV_HST, &krb_principal_);
            dprintf(D_SECURITY, "KERBEROS: get local server principal for \"%s\" %s\n",
                    service, err ? " FAILED" : "");

            free (service);
        }
    }

    if (IsDebugLevel(D_SECURITY) && !err) {
        char *tmp;
        if (!krb5_unparse_name_ptr(krb_context_, mySock_->isClient() ? krb_principal_ : server_, &tmp))
	    dprintf(D_SECURITY, "KERBEROS: the server principal is \"%s\"\n", tmp);
        free(tmp);
    }

    return !err;
}


int Condor_Auth_Kerberos :: read_request(krb5_data * request)
{
    int code = TRUE, message = 0;
    
    mySock_->decode();
    
    if (!mySock_->code(message)) {
        return FALSE;
    }

    if (message == KERBEROS_PROCEED) {
        if (!mySock_->code(request->length)) {
            dprintf(D_ALWAYS, "KERBEROS: Incorrect message 1!\n");
            code = FALSE;
        }
        else {
            request->data = (char *) malloc(request->length);
            
            if ((!mySock_->get_bytes(request->data, request->length)) ||
                (!mySock_->end_of_message())) {
                dprintf(D_ALWAYS, "KERBEROS: Incorrect message 2!\n");
                code = FALSE;
            }
        }
    }
    else {
        mySock_->end_of_message();
        code = FALSE;
    }
    
    return code;
}
    
//----------------------------------------------------------------------
// getRemoteHost -- retrieve remote host's address
//----------------------------------------------------------------------    
void Condor_Auth_Kerberos :: setRemoteAddress()
{
    krb5_error_code  code;

    // the krb5_free_addresses function takes a NULL-terminate array of addrs,
    // not just an address of a krb5_address*
    //
    // it (somewhat surprisingly) also calls free on the array itself, so
    // allocate it with malloc() and not on the stack!
    //
    // Some kerberos docs reference krb5_free_address(), which frees a
    // single krb5_address, but that doesn't appear to be available for us
    // to call.
    // macOS appears to require non-NULL values for both the local_addr and
    // remote_addr parameters of krb5_auth_con_getaddrs().
    //
    // Since krb5_free_addresses() expects a NULL-terminated array of
    // krb5_address*, allocate arrays of size 2, one for our single
    // krb5_address and one for terminating the array.
    krb5_address** localAddrs = (krb5_address**)calloc(2, sizeof(krb5_address*));
    krb5_address** remoteAddrs = (krb5_address**)calloc(2, sizeof(krb5_address*));

    // Get remote host's address first
    if ((code = krb5_auth_con_getaddrs_ptr(krb_context_, 
                                      auth_context_, 
                                      localAddrs,
                                      remoteAddrs))) {
        goto error;
    }
    dprintf(D_SECURITY | D_VERBOSE, "KERBEROS: remoteAddrs[] is {%p, %p}\n", remoteAddrs[0], remoteAddrs[1]);
    
    if (remoteAddrs[0]) {
        struct in_addr in;
        memcpy(&(in.s_addr), (remoteAddrs[0])[0].contents, sizeof(in_addr));
        setRemoteHost(inet_ntoa(in));
    }
    krb5_free_addresses_ptr(krb_context_, localAddrs);
    krb5_free_addresses_ptr(krb_context_, remoteAddrs);
    
    dprintf(D_SECURITY, "Remote host is %s\n", getRemoteHost());

    return;

 error:
    krb5_free_addresses_ptr(krb_context_, localAddrs);
    krb5_free_addresses_ptr(krb_context_, remoteAddrs);

    dprintf( D_ALWAYS, "KERBEROS: Unable to obtain remote address: %s\n",
			 error_message_ptr(code) );
}

int Condor_Auth_Kerberos :: endTime() const
{
    if (creds_) {
        return creds_->times.endtime;
    }
    else {
        return -1;
    }
}

int Condor_Auth_Kerberos :: isValid() const
{
    return auth_context_ != NULL;  // This is incorrect!
}

void Condor_Auth_Kerberos :: dprintf_krb5_principal ( int deblevel,
													  const char *fmt,
													  krb5_principal p ) {

	if (p) {
		char * tmpprincname = 0;
		if (int code = krb5_unparse_name_ptr(krb_context_, p, &tmpprincname)){
			dprintf( deblevel, fmt, "ERROR FOLLOWS");
			dprintf( deblevel, fmt, error_message_ptr(code));
		} else {
			dprintf( deblevel, fmt, tmpprincname );
		}
		free (tmpprincname);
		tmpprincname = 0;
	} else {
		dprintf ( deblevel, fmt, "(NULL)" );
	}
}

#endif
