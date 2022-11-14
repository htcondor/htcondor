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

#if defined(HAVE_EXT_VOMS)
extern "C"
{
	#include "voms/voms_apic.h"
}
#endif

#include "DelegationInterface.h"

void warn_on_gsi_config();

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


