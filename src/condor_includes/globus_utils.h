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

#define NULL_JOB_CONTACT	"X"

// Keep these in synch with the values defined in the Globus header files.
#define GLOBUS_SUCCESS 0
typedef void (* globus_gram_client_callback_func_t)(void * user_callback_arg,
						    char * job_contact,
						    int state,
						    int errorcode);
typedef enum
{
    GLOBUS_GRAM_PROTOCOL_ERROR_PARAMETER_NOT_SUPPORTED = 1,
    GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_REQUEST = 2,
    GLOBUS_GRAM_PROTOCOL_ERROR_NO_RESOURCES = 3,
    GLOBUS_GRAM_PROTOCOL_ERROR_BAD_DIRECTORY = 4,
    GLOBUS_GRAM_PROTOCOL_ERROR_EXECUTABLE_NOT_FOUND = 5,
    GLOBUS_GRAM_PROTOCOL_ERROR_INSUFFICIENT_FUNDS = 6,
    GLOBUS_GRAM_PROTOCOL_ERROR_AUTHORIZATION = 7,
    GLOBUS_GRAM_PROTOCOL_ERROR_USER_CANCELLED = 8,
    GLOBUS_GRAM_PROTOCOL_ERROR_SYSTEM_CANCELLED = 9,
    GLOBUS_GRAM_PROTOCOL_ERROR_PROTOCOL_FAILED = 10,
    GLOBUS_GRAM_PROTOCOL_ERROR_STDIN_NOT_FOUND = 11,
    GLOBUS_GRAM_PROTOCOL_ERROR_CONNECTION_FAILED = 12,
    GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_MAXTIME = 13,
    GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_COUNT = 14,
    GLOBUS_GRAM_PROTOCOL_ERROR_NULL_SPECIFICATION_TREE = 15,
    GLOBUS_GRAM_PROTOCOL_ERROR_JM_FAILED_ALLOW_ATTACH = 16,
    GLOBUS_GRAM_PROTOCOL_ERROR_JOB_EXECUTION_FAILED = 17,
    GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_PARADYN = 18,
    GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_JOBTYPE = 19,
    GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_GRAM_MYJOB = 20,
    GLOBUS_GRAM_PROTOCOL_ERROR_BAD_SCRIPT_ARG_FILE = 21,
    GLOBUS_GRAM_PROTOCOL_ERROR_ARG_FILE_CREATION_FAILED = 22,
    GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_JOBSTATE = 23,
    GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_SCRIPT_REPLY = 24,
    GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_SCRIPT_STATUS = 25,
    GLOBUS_GRAM_PROTOCOL_ERROR_JOBTYPE_NOT_SUPPORTED = 26,
    GLOBUS_GRAM_PROTOCOL_ERROR_UNIMPLEMENTED = 27,
    GLOBUS_GRAM_PROTOCOL_ERROR_TEMP_SCRIPT_FILE_FAILED = 28,
    GLOBUS_GRAM_PROTOCOL_ERROR_USER_PROXY_NOT_FOUND = 29,
    GLOBUS_GRAM_PROTOCOL_ERROR_OPENING_USER_PROXY = 30,
    GLOBUS_GRAM_PROTOCOL_ERROR_JOB_CANCEL_FAILED = 31,
    GLOBUS_GRAM_PROTOCOL_ERROR_MALLOC_FAILED = 32,
    GLOBUS_GRAM_PROTOCOL_ERROR_DUCT_INIT_FAILED = 33,
    GLOBUS_GRAM_PROTOCOL_ERROR_DUCT_LSP_FAILED = 34,
    GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_HOST_COUNT = 35,
    GLOBUS_GRAM_PROTOCOL_ERROR_UNSUPPORTED_PARAMETER = 36,
    GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_QUEUE = 37,
    GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_PROJECT = 38,
    GLOBUS_GRAM_PROTOCOL_ERROR_RSL_EVALUATION_FAILED = 39,
    GLOBUS_GRAM_PROTOCOL_ERROR_BAD_RSL_ENVIRONMENT = 40,
    GLOBUS_GRAM_PROTOCOL_ERROR_DRYRUN = 41,
    GLOBUS_GRAM_PROTOCOL_ERROR_ZERO_LENGTH_RSL = 42,
    GLOBUS_GRAM_PROTOCOL_ERROR_STAGING_EXECUTABLE = 43,
    GLOBUS_GRAM_PROTOCOL_ERROR_STAGING_STDIN = 44,
    GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_JOB_MANAGER_TYPE = 45,
    GLOBUS_GRAM_PROTOCOL_ERROR_BAD_ARGUMENTS = 46,
    GLOBUS_GRAM_PROTOCOL_ERROR_GATEKEEPER_MISCONFIGURED = 47,
    GLOBUS_GRAM_PROTOCOL_ERROR_BAD_RSL = 48,
    GLOBUS_GRAM_PROTOCOL_ERROR_VERSION_MISMATCH = 49,
    GLOBUS_GRAM_PROTOCOL_ERROR_RSL_ARGUMENTS = 50,
    GLOBUS_GRAM_PROTOCOL_ERROR_RSL_COUNT = 51,
    GLOBUS_GRAM_PROTOCOL_ERROR_RSL_DIRECTORY = 52,
    GLOBUS_GRAM_PROTOCOL_ERROR_RSL_DRYRUN = 53,
    GLOBUS_GRAM_PROTOCOL_ERROR_RSL_ENVIRONMENT = 54,
    GLOBUS_GRAM_PROTOCOL_ERROR_RSL_EXECUTABLE = 55,
    GLOBUS_GRAM_PROTOCOL_ERROR_RSL_HOST_COUNT = 56,
    GLOBUS_GRAM_PROTOCOL_ERROR_RSL_JOBTYPE = 57,
    GLOBUS_GRAM_PROTOCOL_ERROR_RSL_MAXTIME = 58,
    GLOBUS_GRAM_PROTOCOL_ERROR_RSL_MYJOB = 59,
    GLOBUS_GRAM_PROTOCOL_ERROR_RSL_PARADYN = 60,
    GLOBUS_GRAM_PROTOCOL_ERROR_RSL_PROJECT = 61,
    GLOBUS_GRAM_PROTOCOL_ERROR_RSL_QUEUE = 62,
    GLOBUS_GRAM_PROTOCOL_ERROR_RSL_STDERR = 63,
    GLOBUS_GRAM_PROTOCOL_ERROR_RSL_STDIN = 64,
    GLOBUS_GRAM_PROTOCOL_ERROR_RSL_STDOUT = 65,
    GLOBUS_GRAM_PROTOCOL_ERROR_OPENING_JOBMANAGER_SCRIPT = 66,
    GLOBUS_GRAM_PROTOCOL_ERROR_CREATING_PIPE = 67,
    GLOBUS_GRAM_PROTOCOL_ERROR_FCNTL_FAILED = 68,
    GLOBUS_GRAM_PROTOCOL_ERROR_STDOUT_FILENAME_FAILED = 69,
    GLOBUS_GRAM_PROTOCOL_ERROR_STDERR_FILENAME_FAILED = 70,
    GLOBUS_GRAM_PROTOCOL_ERROR_FORKING_EXECUTABLE = 71,
    GLOBUS_GRAM_PROTOCOL_ERROR_EXECUTABLE_PERMISSIONS = 72,
    GLOBUS_GRAM_PROTOCOL_ERROR_OPENING_STDOUT = 73,
    GLOBUS_GRAM_PROTOCOL_ERROR_OPENING_STDERR = 74,
    GLOBUS_GRAM_PROTOCOL_ERROR_OPENING_CACHE_USER_PROXY = 75,
    GLOBUS_GRAM_PROTOCOL_ERROR_OPENING_CACHE = 76,
    GLOBUS_GRAM_PROTOCOL_ERROR_INSERTING_CLIENT_CONTACT = 77,
    GLOBUS_GRAM_PROTOCOL_ERROR_CLIENT_CONTACT_NOT_FOUND = 78,
    GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER = 79,
    GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_JOB_CONTACT = 80,
    GLOBUS_GRAM_PROTOCOL_ERROR_UNDEFINED_EXE = 81,
    GLOBUS_GRAM_PROTOCOL_ERROR_CONDOR_ARCH = 82,
    GLOBUS_GRAM_PROTOCOL_ERROR_CONDOR_OS = 83,
    GLOBUS_GRAM_PROTOCOL_ERROR_RSL_MIN_MEMORY = 84,
    GLOBUS_GRAM_PROTOCOL_ERROR_RSL_MAX_MEMORY = 85,
    GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_MIN_MEMORY = 86,
    GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_MAX_MEMORY = 87,
    GLOBUS_GRAM_PROTOCOL_ERROR_HTTP_FRAME_FAILED = 88,
    GLOBUS_GRAM_PROTOCOL_ERROR_HTTP_UNFRAME_FAILED = 89,
    GLOBUS_GRAM_PROTOCOL_ERROR_HTTP_PACK_FAILED = 90,
    GLOBUS_GRAM_PROTOCOL_ERROR_HTTP_UNPACK_FAILED = 91,
    GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_JOB_QUERY = 92,
    GLOBUS_GRAM_PROTOCOL_ERROR_SERVICE_NOT_FOUND = 93,
    GLOBUS_GRAM_PROTOCOL_ERROR_JOB_QUERY_DENIAL = 94,
    GLOBUS_GRAM_PROTOCOL_ERROR_CALLBACK_NOT_FOUND = 95,
    GLOBUS_GRAM_PROTOCOL_ERROR_BAD_GATEKEEPER_CONTACT = 96,
    GLOBUS_GRAM_PROTOCOL_ERROR_POE_NOT_FOUND = 97,
    GLOBUS_GRAM_PROTOCOL_ERROR_MPIRUN_NOT_FOUND = 98,
    GLOBUS_GRAM_PROTOCOL_ERROR_RSL_START_TIME = 99,
    GLOBUS_GRAM_PROTOCOL_ERROR_RSL_RESERVATION_HANDLE = 100,
    GLOBUS_GRAM_PROTOCOL_ERROR_RSL_MAX_WALL_TIME = 101,
    GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_MAX_WALL_TIME = 102,
    GLOBUS_GRAM_PROTOCOL_ERROR_RSL_MAX_CPU_TIME = 103,
    GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_MAX_CPU_TIME = 104,
    GLOBUS_GRAM_PROTOCOL_ERROR_JM_SCRIPT_NOT_FOUND = 105,
    GLOBUS_GRAM_PROTOCOL_ERROR_JM_SCRIPT_PERMISSIONS = 106,
    GLOBUS_GRAM_PROTOCOL_ERROR_SIGNALING_JOB = 107,
    GLOBUS_GRAM_PROTOCOL_ERROR_UNKNOWN_SIGNAL_TYPE = 108,
    GLOBUS_GRAM_PROTOCOL_ERROR_GETTING_JOBID = 109,
    GLOBUS_GRAM_PROTOCOL_ERROR_WAITING_FOR_COMMIT = 110,
    GLOBUS_GRAM_PROTOCOL_ERROR_COMMIT_TIMED_OUT = 111,
    GLOBUS_GRAM_PROTOCOL_ERROR_RSL_SAVE_STATE = 112,
    GLOBUS_GRAM_PROTOCOL_ERROR_RSL_RESTART = 113,
    GLOBUS_GRAM_PROTOCOL_ERROR_RSL_TWO_PHASE_COMMIT = 114,
    GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_TWO_PHASE_COMMIT = 115,
    GLOBUS_GRAM_PROTOCOL_ERROR_RSL_STDOUT_POSITION = 116,
    GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_STDOUT_POSITION = 117,
    GLOBUS_GRAM_PROTOCOL_ERROR_RSL_STDERR_POSITION = 118,
    GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_STDERR_POSITION = 119,
    GLOBUS_GRAM_PROTOCOL_ERROR_RESTART_FAILED = 120,
    GLOBUS_GRAM_PROTOCOL_ERROR_NO_STATE_FILE = 121,
    GLOBUS_GRAM_PROTOCOL_ERROR_READING_STATE_FILE = 122,
    GLOBUS_GRAM_PROTOCOL_ERROR_WRITING_STATE_FILE = 123,
    GLOBUS_GRAM_PROTOCOL_ERROR_OLD_JM_ALIVE = 124,
    GLOBUS_GRAM_PROTOCOL_ERROR_TTL_EXPIRED = 125,
    GLOBUS_GRAM_PROTOCOL_ERROR_SUBMIT_UNKNOWN = 126,
    GLOBUS_GRAM_PROTOCOL_ERROR_RSL_REMOTE_IO_URL = 127,
    GLOBUS_GRAM_PROTOCOL_ERROR_WRITING_REMOTE_IO_URL = 128,
    GLOBUS_GRAM_PROTOCOL_ERROR_STDIO_SIZE = 129,
    GLOBUS_GRAM_PROTOCOL_ERROR_JM_STOPPED = 130,
    GLOBUS_GRAM_PROTOCOL_ERROR_USER_PROXY_EXPIRED = 131,
    GLOBUS_GRAM_PROTOCOL_ERROR_JOB_UNSUBMITTED = 132,
    GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_COMMIT = 133,
	GLOBUS_GRAM_PROTOCOL_ERROR_RSL_SCHEDULER_SPECIFIC = 134,
	GLOBUS_GRAM_PROTOCOL_ERROR_STAGE_IN_FAILED = 135,
	GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_SCRATCH = 136,
	GLOBUS_GRAM_PROTOCOL_ERROR_RSL_CACHE = 137,
	GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_SUBMIT_ATTRIBUTE = 138,
	GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_STDIO_UPDATE_ATTRIBUTE = 139,
	GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_RESTART_ATTRIBUTE = 140,
	GLOBUS_GRAM_PROTOCOL_ERROR_RSL_FILE_STAGE_IN = 141,
	GLOBUS_GRAM_PROTOCOL_ERROR_RSL_FILE_STAGE_IN_SHARED = 142,
	GLOBUS_GRAM_PROTOCOL_ERROR_RSL_FILE_STAGE_OUT = 143,
	GLOBUS_GRAM_PROTOCOL_ERROR_RSL_GASS_CACHE = 144,
	GLOBUS_GRAM_PROTOCOL_ERROR_RSL_FILE_CLEANUP = 145,
	GLOBUS_GRAM_PROTOCOL_ERROR_RSL_SCRATCH = 146,
	GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_SCHEDULER_SPECIFIC = 147,
	GLOBUS_GRAM_PROTOCOL_ERROR_UNDEFINED_ATTRIBUTE = 148,
	GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_CACHE = 149,
	GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_SAVE_STATE = 150,
	GLOBUS_GRAM_PROTOCOL_ERROR_OPENING_VALIDATION_FILE = 151,
	GLOBUS_GRAM_PROTOCOL_ERROR_READING_VALIDATION_FILE = 152,
	GLOBUS_GRAM_PROTOCOL_ERROR_RSL_PROXY_TIMEOUT = 153,
	GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_PROXY_TIMEOUT = 154,
	GLOBUS_GRAM_PROTOCOL_ERROR_STAGE_OUT_FAILED = 155,
	GLOBUS_GRAM_PROTOCOL_ERROR_JOB_CONTACT_NOT_FOUND = 156,
	GLOBUS_GRAM_PROTOCOL_ERROR_DELEGATION_FAILED = 157,
	GLOBUS_GRAM_PROTOCOL_ERROR_LOCKING_STATE_LOCK_FILE = 158,
	GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_ATTR = 159,
	GLOBUS_GRAM_PROTOCOL_ERROR_NULL_PARAMETER = 160,
	GLOBUS_GRAM_PROTOCOL_ERROR_STILL_STREAMING = 161,
	GLOBUS_GRAM_PROTOCOL_ERROR_LAST = 162
} globus_gram_protocol_error_t;
typedef enum
{
    GLOBUS_GRAM_PROTOCOL_JOB_STATE_PENDING = 1,
    GLOBUS_GRAM_PROTOCOL_JOB_STATE_ACTIVE = 2,
    GLOBUS_GRAM_PROTOCOL_JOB_STATE_FAILED = 4,
    GLOBUS_GRAM_PROTOCOL_JOB_STATE_DONE = 8,
    GLOBUS_GRAM_PROTOCOL_JOB_STATE_SUSPENDED = 16,
    GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNSUBMITTED = 32,
	GLOBUS_GRAM_PROTOCOL_JOB_STATE_STAGE_IN = 64,
	GLOBUS_GRAM_PROTOCOL_JOB_STATE_STAGE_OUT = 128,
    GLOBUS_GRAM_PROTOCOL_JOB_STATE_ALL = 0xFFFFF,
	// This last state isn't a real GRAM job state. It's used
	// by the gridmanager when a job's state is unknown
	GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNKNOWN = 0
} globus_gram_protocol_job_state_t;
typedef enum
{
    GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_CANCEL = 1,
    GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_SUSPEND = 2,
    GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_RESUME = 3,
    GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_PRIORITY = 4,
    GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_COMMIT_REQUEST = 5,
    GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_COMMIT_EXTEND = 6,
    GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_STDIO_UPDATE = 7,
    GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_STDIO_SIZE = 8,
    GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_STOP_MANAGER = 9,
    GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_COMMIT_END = 10
} globus_gram_protocol_job_signal_t;


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

int activate_globus_gsi();

const char *GlobusJobStatusName( int status );

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


void parse_resource_manager_string( const char *string, char **host,
									char **port, char **service,
									char **subject );

/* Returns true (non-0) if path looks like an URL that Globus
   (specifically, globus-url-copy) can handle */
int is_globus_friendly_url(const char * path);

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


