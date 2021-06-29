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

#ifndef GLOBUS_UTILS_H
#define GLOBUS_UTILS_H

#include "condor_common.h"

#if defined(HAVE_EXT_GLOBUS)
#     include "globus_gsi_credential.h"
#     include "globus_gsi_system_config.h"
#     include "globus_gsi_system_config_constants.h"
#     include "gssapi.h"
#     include "globus_gss_assist.h"
#     include "globus_gsi_proxy.h"
#endif

#if defined(HAVE_EXT_VOMS)
extern "C"
{
	#include "voms/voms_apic.h"
}
#endif

#include "DelegationInterface.h"

// Pointers to Globus symbols used in Condor_Auth_X509
#ifdef HAVE_EXT_GLOBUS
extern int (*globus_thread_set_model_ptr)(
	const char *);
extern globus_result_t (*globus_gsi_cred_get_cert_ptr)(
	globus_gsi_cred_handle_t, X509 **);
extern globus_result_t (*globus_gsi_cred_get_cert_chain_ptr)(
	globus_gsi_cred_handle_t, STACK_OF(X509) **);
extern OM_uint32 (*globus_gss_assist_display_status_str_ptr)(
	char **, char *, OM_uint32, OM_uint32, int);
extern globus_result_t (*globus_gss_assist_map_and_authorize_ptr)(
	gss_ctx_id_t, char *, char *, char *, unsigned int);
extern OM_uint32 (*globus_gss_assist_acquire_cred_ptr)(
	OM_uint32 *, gss_cred_usage_t, gss_cred_id_t *);
extern OM_uint32 (*globus_gss_assist_init_sec_context_ptr)(
	OM_uint32 *, const gss_cred_id_t, gss_ctx_id_t *, char *, OM_uint32,
	OM_uint32 *, int *, int (*)(void *, void **, size_t *), void *,
	int (*)(void *, void *, size_t), void *);
extern OM_uint32 (*gss_accept_sec_context_ptr)(
	OM_uint32 *, gss_ctx_id_t *, const gss_cred_id_t, const gss_buffer_t,
	const gss_channel_bindings_t, gss_name_t *, gss_OID *, gss_buffer_t,
	OM_uint32 *, OM_uint32 *, gss_cred_id_t *);
extern OM_uint32 (*gss_compare_name_ptr)(
	OM_uint32 *, const gss_name_t, const gss_name_t, int *);
extern OM_uint32 (*gss_context_time_ptr)(
	OM_uint32 *, const gss_ctx_id_t, OM_uint32 *);
extern OM_uint32 (*gss_delete_sec_context_ptr)(
	OM_uint32 *, gss_ctx_id_t *, gss_buffer_t);
extern OM_uint32 (*gss_display_name_ptr)(
	OM_uint32 *, const gss_name_t, gss_buffer_t, gss_OID *);
extern OM_uint32 (*gss_import_name_ptr)(
	OM_uint32 *, const gss_buffer_t, const gss_OID, gss_name_t *);
extern OM_uint32 (*gss_inquire_context_ptr)(
	OM_uint32 *, const gss_ctx_id_t, gss_name_t *, gss_name_t *,
	OM_uint32 *, gss_OID *, OM_uint32 *, int *, int *);
extern OM_uint32 (*gss_release_buffer_ptr)(
	OM_uint32 *, gss_buffer_t);
extern OM_uint32 (*gss_release_cred_ptr)(
	OM_uint32 *, gss_cred_id_t *);
extern OM_uint32 (*gss_release_name_ptr)(
	OM_uint32 *, gss_name_t *);
extern OM_uint32 (*gss_unwrap_ptr)(
	OM_uint32 *, const gss_ctx_id_t, const gss_buffer_t, gss_buffer_t, int *,
	gss_qop_t *);
extern OM_uint32 (*gss_wrap_ptr)(
	OM_uint32 *, const gss_ctx_id_t, int, gss_qop_t, const gss_buffer_t,
	int *, gss_buffer_t);
extern gss_OID_desc **gss_nt_host_ip_ptr;
#endif

void warn_on_gsi_usage();

int activate_globus_gsi();

char *get_x509_proxy_filename( void );

time_t x509_proxy_expiration_time( const char *proxy_file );

char* x509_proxy_email( const char *proxy_file);

char* x509_proxy_subject_name( const char *proxy_file);

char* x509_proxy_identity_name( const char *proxy_file);

const char* x509_error_string( void );

/*
 * expiration_time: 0 if none; o.w. timestamp of delegated proxy expiration
 * result_expiration_time; if non-NULL, gets set to actual expiration time
 *                         of delegated proxy; could be shorter than requested
 *                         if source proxy expires earlier
 */
int
x509_send_delegation( const char *source_file,
					  time_t expiration_time,
					  time_t *result_expiration_time,
					  int (*recv_data_func)(void *, void **, size_t *), 
					  void *recv_data_ptr,
					  int (*send_data_func)(void *, void *, size_t),
					  void *send_data_ptr );

/* Receive the delegation over set of send/recv functions.
 *
 * 0 - success.
 * -1 - failure.
 * 2 - continue.
 *
 * Continuations are only possible if the state_ptr is non-null.
 */
int
x509_receive_delegation( const char *destination_file,
						 int (*recv_data_func)(void *, void **, size_t *), 
						 void *recv_data_ptr,
						 int (*send_data_func)(void *, void *, size_t),
						 void *send_data_ptr,
						 void **state_ptr);

/* The second half of receive delegation.
 *
 * 0 - success
 * -1 - failure.
 *
 * state_ptr represents the memory status of the delegation, using an internal data structure.
 * The function takes ownership of state_ptr and will call delete on it.
 */
int
x509_receive_delegation_finish( int (*recv_data_func)(void *, void **, size_t *),
                                void *recv_data_ptr,
                                void *state_ptr);


X509Credential* x509_proxy_read( const char *proxy_file );

time_t x509_proxy_expiration_time( X509 *cert, STACK_OF(X509)* chain );
time_t x509_proxy_expiration_time( X509Credential* cred );

char* x509_proxy_email( X509 *cert, STACK_OF(X509)* chain );
char* x509_proxy_email( X509Credential* cred );

char* x509_proxy_subject_name( X509* cert );
char* x509_proxy_subject_name( X509Credential* cred );

char* x509_proxy_identity_name( X509 *cert, STACK_OF(X509)* chain );
char* x509_proxy_identity_name( X509Credential* cred );

/* functions for extracting voms attributes */
int extract_VOMS_info( X509* cert, STACK_OF(X509)* chain, int verify_type, char **voname, char **firstfqan, char **quoted_DN_and_FQAN);
int extract_VOMS_info( X509Credential* cred, int verify_type, char **voname, char **firstfqan, char **quoted_DN_and_FQAN);
int extract_VOMS_info_from_file( const char* proxy_file, int verify_type, char **voname, char **firstfqan, char **quoted_DN_and_FQAN);

#endif


