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
#include "condor_config.h"
#include "condor_debug.h"
#include "util_lib_proto.h"

#include "globus_utils.h"

#if defined(HAVE_EXT_GLOBUS)
#     include "globus_gsi_credential.h"
#     include "globus_gsi_system_config.h"
#     include "globus_gsi_system_config_constants.h"
#     include "gssapi.h"
#     include "globus_gss_assist.h"
#     include "globus_gsi_proxy.h"
#endif

#if defined(HAVE_EXT_VOMS)
#include "glite/security/voms/voms_apic.h"
#endif


#define DEFAULT_MIN_TIME_LEFT 8*60*60;


static char * _globus_error_message = NULL;

#define GRAM_STATUS_STR_LEN		8

const char *
GlobusJobStatusName( int status )
{
	static char buf[GRAM_STATUS_STR_LEN];
#if defined(HAVE_EXT_GLOBUS)
	switch ( status ) {
	case GLOBUS_GRAM_PROTOCOL_JOB_STATE_PENDING:
		return "PENDING";
	case GLOBUS_GRAM_PROTOCOL_JOB_STATE_ACTIVE:
		return "ACTIVE";
	case GLOBUS_GRAM_PROTOCOL_JOB_STATE_FAILED:
		return "FAILED";
	case GLOBUS_GRAM_PROTOCOL_JOB_STATE_DONE:
		return "DONE";
	case GLOBUS_GRAM_PROTOCOL_JOB_STATE_SUSPENDED:
		return "SUSPENDED";
	case GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNSUBMITTED:
		return "UNSUBMITTED";
	case GLOBUS_GRAM_PROTOCOL_JOB_STATE_STAGE_IN:
		return "STAGE_IN";
	case GLOBUS_GRAM_PROTOCOL_JOB_STATE_STAGE_OUT:
		return "STAGE_OUT";
	case GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNKNOWN:
		return "UNKNOWN";
	default:
		snprintf( buf, GRAM_STATUS_STR_LEN, "%d", status );
		return buf;
	}
#else
	snprintf( buf, GRAM_STATUS_STR_LEN, "%d", status );
	return buf;
#endif
}

const char *
x509_error_string( void )
{
	return _globus_error_message;
}

static
void
set_error_string( const char *message )
{
	if ( _globus_error_message ) {
		free( _globus_error_message );
	}
	_globus_error_message = strdup( message );
}

/* Activate the globus gsi modules for use by functions in this file.
 * Returns zero if the modules were successfully activated. Returns -1 if
 * something went wrong.
 */
static
int
activate_globus_gsi( void )
{
#if !defined(HAVE_EXT_GLOBUS)
	set_error_string( "This version of Condor doesn't support X509 credentials!" );
	return -1;
#else
	static int globus_gsi_activated = 0;

	if ( globus_gsi_activated != 0 ) {
		return 0;
	}

/* This module is activated by GLOBUS_GSI_CREDENTIAL_MODULE
	if ( globus_module_activate(GLOBUS_GSI_SYSCONFIG_MODULE) ) {
		set_error_string( "couldn't activate globus gsi sysconfig module" );
		return -1;
	}
*/

	if ( globus_module_activate(GLOBUS_GSI_CREDENTIAL_MODULE) ) {
		set_error_string( "couldn't activate globus gsi credential module" );
		return -1;
	}


	if ( globus_module_activate(GLOBUS_GSI_GSSAPI_MODULE) ) {
		set_error_string( "couldn't activate globus gsi gssapi module" );
		return -1;
	}


	if ( globus_module_activate(GLOBUS_GSI_PROXY_MODULE) ) {
		set_error_string( "couldn't activate globus gsi proxy module" );
		return -1;
	}


	globus_gsi_activated = 1;
	return 0;
#endif
}

/* Return the path to the X509 proxy file as determined by GSI/SSL.
 * Returns NULL if the filename can't be determined. Otherwise, the
 * string returned must be freed with free().
 */
char *
get_x509_proxy_filename( void )
{
	char *proxy_file = NULL;
#if defined(HAVE_EXT_GLOBUS)
	globus_gsi_proxy_file_type_t     file_type    = GLOBUS_PROXY_FILE_INPUT;

	if ( activate_globus_gsi() != 0 ) {
		return NULL;
	}

	if ( GLOBUS_GSI_SYSCONFIG_GET_PROXY_FILENAME(&proxy_file, file_type) !=
		 GLOBUS_SUCCESS ) {
		set_error_string( "unable to locate proxy file" );
	}
#endif
	return proxy_file;
}

#if defined(HAVE_EXT_GLOBUS)
char* extract_VOMS_attrs( globus_gsi_cred_handle_t handle, int *error) {

#if !defined(HAVE_EXT_VOMS)
	*error = 1;
	return strdup("VOMS support not compiled into this build");
#else

   int ret;
   struct vomsdata *voms_data = NULL;
   struct voms **voms_cert  = NULL;
   char **fqan = NULL;
   int voms_err;
   char *retfqan = 0;
   int num_attrs = 0;
   int fqan_len = 0;

    STACK_OF(X509) *chain = NULL;
	X509 *cert = NULL;

	if (!param_boolean_int("USE_VOMS_ATTRIBUTES", 1)) {
		*error = 1;
		return strdup("VOMS support was disabled by config parameter USE_VOMS_ATTRIBUTES.");
	}

	ret = globus_gsi_cred_get_cert_chain(handle, &chain);
	if(ret != GLOBUS_SUCCESS) {
		retfqan = strdup("Failed to get cert chain from credential");
		*error = 1;
		goto end;
	}

	ret = globus_gsi_cred_get_cert(handle, &cert);
	if(ret != GLOBUS_SUCCESS) {
		retfqan = strdup("Failed to get cert from credential");
		*error = 1;
		goto end;
	}

   voms_data = VOMS_Init(NULL, NULL);
   if (voms_data == NULL) {
		retfqan = strdup("Failed to read VOMS attributes, VOMS_Init() failed");
	   *error = 1;
		goto end;
   }

   ret = VOMS_SetVerificationType( VERIFY_NONE, voms_data, &voms_err );
   if (ret == 0) {
      retfqan = VOMS_ErrorMessage(voms_data, voms_err, NULL, 0);
      *error = 1;
      goto end;
   }

   ret = VOMS_Retrieve(cert, chain, RECURSE_CHAIN,
                          voms_data, &voms_err);
   if (ret == 0) {
      if (voms_err == VERR_NOEXT) {
         /* No VOMS extensions present, return silently */
         *error = 0;
         goto end;
      } else {
         retfqan = VOMS_ErrorMessage(voms_data, voms_err, NULL, 0);
     	*error = 1;
        goto end;
      }
   }


   // skim through once to find the length
   for (voms_cert = voms_data->data; voms_cert && *voms_cert; voms_cert++) {
      for (fqan = (*voms_cert)->fqan; fqan && *fqan; fqan++) {
          num_attrs++;
          fqan_len += strlen(*fqan);
      }
   }

   // enough room for the attrs, ':' separator and NULL terminator.
   retfqan = (char*) malloc (fqan_len + num_attrs);
   *retfqan = 0;

   // now fill it all in
   for (voms_cert = voms_data->data; voms_cert && *voms_cert; voms_cert++) {
      for (fqan = (*voms_cert)->fqan; fqan && *fqan; fqan++) {
          if (*retfqan) {
              strcat (retfqan, ":");
          }
          strcat(retfqan, *fqan);
      }
   }

   *error = 0;

end:
   if (voms_data)
      VOMS_Destroy(voms_data);
	if (cert)
		X509_free(cert);
	if(chain)
		sk_X509_free(chain);

   return retfqan;
#endif /* HAVE_EXT_VOMS */

}
#endif /* HAVE_EXT_GLOBUS */

/* Return the subject name of a given proxy cert. 
  On error, return NULL.
  On success, return a pointer to a null-terminated string.
  IT IS THE CALLER'S RESPONSBILITY TO DE-ALLOCATE THE STIRNG
  WITH free().
 */
char *
x509_proxy_subject_name( const char *proxy_file, int include_voms_fqan )
{
#if !defined(HAVE_EXT_GLOBUS)
	set_error_string( "This version of Condor doesn't support X509 credentials!" );
	return NULL;
#else

	globus_gsi_cred_handle_t         handle       = NULL;
	globus_gsi_cred_handle_attrs_t   handle_attrs = NULL;
	char *subject_name = NULL;
	char *my_proxy_file = NULL;
	char *fqan = NULL;
	char *combined_dn_and_fqan = NULL;
	int error = 0;

#if !defined(HAVE_EXT_VOMS)
	// If we don't have VOMS, pretend the proxy doesn't have any VOMS
	// extensions.
	include_voms_fqan = 0;
#endif

	if ( activate_globus_gsi() != 0 ) {
		return NULL;
	}

	if (globus_gsi_cred_handle_attrs_init(&handle_attrs)) {
		set_error_string( "problem during internal initialization1" );
		goto cleanup;
	}

	if (globus_gsi_cred_handle_init(&handle, handle_attrs)) {
		set_error_string( "problem during internal initialization2" );
		goto cleanup;
	}

	/* Check for proxy file */
	if (proxy_file == NULL) {
		my_proxy_file = get_x509_proxy_filename();
		if (my_proxy_file == NULL) {
			goto cleanup;
		}
		proxy_file = my_proxy_file;
	}

	// We should have a proxy file, now, try to read it
	if (globus_gsi_cred_read_proxy(handle, proxy_file)) {
		set_error_string( "unable to read proxy file" );
	   goto cleanup;
	}

	if (globus_gsi_cred_get_subject_name(handle, &subject_name)) {
		set_error_string( "unable to extract subject name" );
		goto cleanup;
	}

	if (include_voms_fqan && param_boolean_int("USE_VOMS_ATTRIBUTES", 1)) {
		fqan = extract_VOMS_attrs(handle, &error);
		if (error) {
			set_error_string(fqan);
			free(fqan);
			goto cleanup;
		}

		// if there are attributes, append them
		if (fqan) {
			combined_dn_and_fqan = (char*)malloc(strlen(subject_name) + strlen(fqan) + 2);
			strcpy (combined_dn_and_fqan, subject_name);
			strcat (combined_dn_and_fqan, ":");
			strcat (combined_dn_and_fqan, fqan);
			free(subject_name);
			free(fqan);
		} else {
			// this is returned at the end.  caller of function must free it.
			combined_dn_and_fqan = subject_name;
		}
	} else {
		// this is returned at the end.  caller of function must free it.
		combined_dn_and_fqan = subject_name;
	}

 cleanup:
	if (my_proxy_file) {
		free(my_proxy_file);
	}

	if (handle_attrs) {
		globus_gsi_cred_handle_attrs_destroy(handle_attrs);
	}

	if (handle) {
		globus_gsi_cred_handle_destroy(handle);
	}

	return combined_dn_and_fqan;

#endif /* !defined(GSS_AUTHENTICATION) */
}


/* Return the identity name of a given X509 cert. For proxy certs, this
  will return the identity that the proxy can act on behalf of, rather than
  the subject of the proxy cert itself. Normally, this will have the
  practical effect of stripping off any "/CN=proxy" strings from the subject
  name.
  On error, return NULL.
  On success, return a pointer to a null-terminated string.
  IT IS THE CALLER'S RESPONSBILITY TO DE-ALLOCATE THE STIRNG
  WITH free().
 */
char *
x509_proxy_identity_name( const char *proxy_file, int include_voms_fqan )
{
#if !defined(HAVE_EXT_GLOBUS)
	set_error_string( "This version of Condor doesn't support X509 credentials!" );
	return NULL;
#else

	globus_gsi_cred_handle_t         handle       = NULL;
	globus_gsi_cred_handle_attrs_t   handle_attrs = NULL;
	char *subject_name = NULL;
	char *my_proxy_file = NULL;
	char *fqan = NULL;
	char *combined_dn_and_fqan = NULL;
	int error = 0;

#if !defined(HAVE_EXT_VOMS)
	// If we don't have VOMS, pretend the proxy doesn't have any VOMS
	// extensions.
	include_voms_fqan = 0;
#endif

	if ( activate_globus_gsi() != 0 ) {
		return NULL;
	}

	if (globus_gsi_cred_handle_attrs_init(&handle_attrs)) {
		set_error_string( "problem during internal initialization1" );
		goto cleanup;
	}

	if (globus_gsi_cred_handle_init(&handle, handle_attrs)) {
		set_error_string( "problem during internal initialization2" );
		goto cleanup;
	}

	/* Check for proxy file */
	if (proxy_file == NULL) {
		my_proxy_file = get_x509_proxy_filename();
		if (my_proxy_file == NULL) {
			goto cleanup;
		}
		proxy_file = my_proxy_file;
	}

	// We should have a proxy file, now, try to read it
	if (globus_gsi_cred_read_proxy(handle, proxy_file)) {
		set_error_string( "unable to read proxy file" );
	   goto cleanup;
	}

	if (globus_gsi_cred_get_identity_name(handle, &subject_name)) {
		set_error_string( "unable to extract identity name" );
		goto cleanup;
	}

	if (include_voms_fqan && param_boolean_int("USE_VOMS_ATTRIBUTES", 1)) {
		fqan = extract_VOMS_attrs(handle, &error);
		if (error) {
			set_error_string(fqan);
			free(fqan);
			goto cleanup;
		}

		// if there are attributes, append them
		if (fqan) {
			combined_dn_and_fqan = (char*)malloc(strlen(subject_name) + strlen(fqan) + 2);
			strcpy (combined_dn_and_fqan, subject_name);
			strcat (combined_dn_and_fqan, ":");
			strcat (combined_dn_and_fqan, fqan);
			free(subject_name);
			free(fqan);
		} else {
			// this is returned at the end.  caller of function must free it.
			combined_dn_and_fqan = subject_name;
		}
	} else {
		// this is returned at the end.  caller of function must free it.
		combined_dn_and_fqan = subject_name;
	}

 cleanup:
	if (my_proxy_file) {
		free(my_proxy_file);
	}

	if (handle_attrs) {
		globus_gsi_cred_handle_attrs_destroy(handle_attrs);
	}

	if (handle) {
		globus_gsi_cred_handle_destroy(handle);
	}

	return combined_dn_and_fqan;

#endif /* !defined(GSS_AUTHENTICATION) */
}

/* Return the time at which the proxy expires. On error, return -1.
 */
time_t
x509_proxy_expiration_time( const char *proxy_file )
{
#if !defined(HAVE_EXT_GLOBUS)
	set_error_string( "This version of Condor doesn't support X509 credentials!" );
	return -1;
#else

    globus_gsi_cred_handle_t         handle       = NULL;
    globus_gsi_cred_handle_attrs_t   handle_attrs = NULL;
	time_t expiration_time = -1;
	time_t time_left;
	char *my_proxy_file = NULL;

	if ( activate_globus_gsi() != 0 ) {
		return -1;
	}

    if (globus_gsi_cred_handle_attrs_init(&handle_attrs)) {
        set_error_string( "problem during internal initialization" );
        goto cleanup;
    }

    if (globus_gsi_cred_handle_init(&handle, handle_attrs)) {
        set_error_string( "problem during internal initialization" );
        goto cleanup;
    }

    /* Check for proxy file */
    if (proxy_file == NULL) {
        my_proxy_file = get_x509_proxy_filename();
        if (my_proxy_file == NULL) {
            goto cleanup;
        }
		proxy_file = my_proxy_file;
	}

    // We should have a proxy file, now, try to read it
    if (globus_gsi_cred_read_proxy(handle, proxy_file)) {
       set_error_string( "unable to read proxy file" );
       goto cleanup;
    }

	if (globus_gsi_cred_get_lifetime(handle, &time_left)) {
		set_error_string( "unable to extract expiration time" );
        goto cleanup;
    }

	expiration_time = time(NULL) + time_left;

 cleanup:
    if (my_proxy_file) {
        free(my_proxy_file);
    }

    if (handle_attrs) {
        globus_gsi_cred_handle_attrs_destroy(handle_attrs);
    }

    if (handle) {
        globus_gsi_cred_handle_destroy(handle);
    }

	return expiration_time;

#endif /* !defined(GSS_AUTHENTICATION) */
}

/* Return the number of seconds until the supplied proxy
 * file will expire.  
 * On error, return -1.    - Todd <tannenba@cs.wisc.edu>
 */
int
x509_proxy_seconds_until_expire( const char *proxy_file )
{
#if !defined(HAVE_EXT_GLOBUS)
	set_error_string( "This version of Condor doesn't support X509 credentials!" );
	return -1;
#else

	time_t time_now;
	time_t time_expire;
	time_t time_diff;

	time_now = time(NULL);
	time_expire = x509_proxy_expiration_time( proxy_file );

	if ( time_expire == -1 ) {
		return -1;
	}

	time_diff = time_expire - time_now;

	if ( time_diff < 0 ) {
		time_diff = 0;
	}

	return (int)time_diff;

#endif /* !defined(GSS_AUTHENTICATION) */
}

/* Attempt a gss_import_cred() to catch some certificate problems. This
 * won't catch all problems (it doesn't verify the entire certificate
 * chain), but it's a start. Returns 0 on success, and -1 on any errors.
 */
int
x509_proxy_try_import( const char *proxy_file )
{
#if !defined(HAVE_EXT_GLOBUS)

	set_error_string( "This version of Condor doesn't support X509 credentials!" );
	return -1;

#else
	unsigned int rc;
	unsigned int min_stat;
	gss_buffer_desc import_buf;
	gss_cred_id_t cred_handle;
	char buf_value[4096];
	char *my_proxy_file = NULL;

	if ( activate_globus_gsi() != 0 ) {
		return -1;
	}

	/* Check for proxy file */
	if (proxy_file == NULL) {
		my_proxy_file = get_x509_proxy_filename();
		if (my_proxy_file == NULL) {
			goto cleanup;
		}
		proxy_file = my_proxy_file;
	}

	snprintf( buf_value, sizeof(buf_value), "X509_USER_PROXY=%s", proxy_file);
	import_buf.value = buf_value;
	import_buf.length = strlen(buf_value) + 1;

	rc = gss_import_cred( &min_stat, &cred_handle, GSS_C_NO_OID, 1,
						  &import_buf, 0, NULL );

	if ( rc != GSS_S_COMPLETE ) {
		char *message;
        globus_gss_assist_display_status_str(&message,
											 "",
											 rc,
											 min_stat,
											 0);
		snprintf( buf_value, sizeof(buf_value), "%s", message );
		free(message);
//		snprintf( buf_value, sizeof(buf_value),
//				  "Failed to import credential maj=%d min=%d", rc,
//				  min_stat );
		set_error_string( buf_value );
		return -1;
	}

	gss_release_cred( &min_stat, &cred_handle );

 cleanup:
    if (my_proxy_file) {
        free(my_proxy_file);
    }

	return 0;
#endif /* !defined(HAVE_EXT_GLOBUS) */
}

int
check_x509_proxy( const char *proxy_file )
{
#if !defined(HAVE_EXT_GLOBUS)

	set_error_string( "This version of Condor doesn't support X509 credentials!" );
	return -1;

#else
	char *min_time_left_param = NULL;
	int min_time_left;
	int time_diff;

	if ( x509_proxy_try_import( proxy_file ) != 0 ) {
		/* Error! Don't set error message, it is already set */
		return -1;
	}

	time_diff = x509_proxy_seconds_until_expire( proxy_file );

	if ( time_diff < 0 ) {
		/* Error! Don't set error message, it is already set */
		return -1;
	}

	/* check validity */
	min_time_left_param = param( "CRED_MIN_TIME_LEFT" );

	if ( min_time_left_param != NULL ) {
		min_time_left = atoi( min_time_left_param );
		free(min_time_left_param);
	} else {
		min_time_left = DEFAULT_MIN_TIME_LEFT;
	}

	if ( time_diff == 0 ) {
		set_error_string( "proxy has expired" );
		return -1;
	}

	if ( time_diff < min_time_left ) {
		set_error_string( "proxy lifetime too short" );
		return -1;
	}

	return 0;

#endif /* !defined(GSS_AUTHENTICATION) */
}


#if defined(HAVE_EXT_GLOBUS)

static int
buffer_to_bio( char *buffer, int buffer_len, BIO **bio )
{
	if ( buffer == NULL ) {
		return FALSE;
	}

	*bio = BIO_new( BIO_s_mem() );
	if ( *bio == NULL ) {
		return FALSE;
	}

	if ( BIO_write( *bio, buffer, buffer_len ) < buffer_len ) {
		BIO_free( *bio );
		return FALSE;
	}

	return TRUE;
}

static int
bio_to_buffer( BIO *bio, char **buffer, int *buffer_len )
{
	if ( bio == NULL ) {
		return FALSE;
	}

	*buffer_len = BIO_pending( bio );

	*buffer = (char *)malloc( *buffer_len );
	if ( *buffer == NULL ) {
		return FALSE;
	}

	if ( BIO_read( bio, *buffer, *buffer_len ) < *buffer_len ) {
		free( *buffer );
		return FALSE;
	}

	return TRUE;
}

#endif /* defined(HAVE_EXT_GLOBUS) */

int
x509_send_delegation( const char *source_file,
					  int (*recv_data_func)(void *, void **, size_t *), 
					  void *recv_data_ptr,
					  int (*send_data_func)(void *, void *, size_t),
					  void *send_data_ptr )
{
#if !defined(HAVE_EXT_GLOBUS)

	_globus_error_message = "This version of Condor doesn't support X509 credentials!" ;
	return -1;

#else
	int rc = 0;
	int error_line = 0;
	globus_result_t result = GLOBUS_SUCCESS;
	globus_gsi_cred_handle_t source_cred =  NULL;
	globus_gsi_proxy_handle_t new_proxy = NULL;
	char *buffer = NULL;
	int buffer_len = 0;
	BIO *bio = NULL;
	X509 *cert = NULL;
	STACK_OF(X509) *cert_chain = NULL;
	int idx = 0;
	globus_gsi_cert_utils_cert_type_t cert_type;

	if ( activate_globus_gsi() != 0 ) {
		return -1;
	}

	result = globus_gsi_cred_handle_init( &source_cred, NULL );
	if ( result != GLOBUS_SUCCESS ) {
		rc = -1;
		error_line = __LINE__;
		goto cleanup;
	}

	result = globus_gsi_proxy_handle_init( &new_proxy, NULL );
	if ( result != GLOBUS_SUCCESS ) {
		rc = -1;
		error_line = __LINE__;
		goto cleanup;
	}

	result = globus_gsi_cred_read_proxy( source_cred, source_file );
	if ( result != GLOBUS_SUCCESS ) {
		rc = -1;
		error_line = __LINE__;
		goto cleanup;
	}

	if ( recv_data_func( recv_data_ptr, (void **)&buffer, (size_t *)&buffer_len ) != 0 ) {
		rc = -1;
		error_line = __LINE__;
		goto cleanup;
	}

	if ( buffer_to_bio( buffer, buffer_len, &bio ) == FALSE ) {
		rc = -1;
		error_line = __LINE__;
		goto cleanup;
	}

	free( buffer );
	buffer = NULL;

	result = globus_gsi_proxy_inquire_req( new_proxy, bio );
	if ( result != GLOBUS_SUCCESS ) {
		rc = -1;
		error_line = __LINE__;
		goto cleanup;
	}

	BIO_free( bio );
	bio = NULL;

		// modify certificate properties
		// set the appropriate proxy type
	result = globus_gsi_cred_get_cert_type( source_cred, &cert_type );
	if ( result != GLOBUS_SUCCESS ) {
		rc = -1;
		error_line = __LINE__;
		goto cleanup;
	}
	switch ( cert_type ) {
	case GLOBUS_GSI_CERT_UTILS_TYPE_CA:
		rc = -1;
		error_line = __LINE__;
		goto cleanup;
	case GLOBUS_GSI_CERT_UTILS_TYPE_EEC:
	case GLOBUS_GSI_CERT_UTILS_TYPE_GSI_3_INDEPENDENT_PROXY:
	case GLOBUS_GSI_CERT_UTILS_TYPE_GSI_3_RESTRICTED_PROXY:
		cert_type = GLOBUS_GSI_CERT_UTILS_TYPE_GSI_3_IMPERSONATION_PROXY;
		break;
	case GLOBUS_GSI_CERT_UTILS_TYPE_RFC_INDEPENDENT_PROXY:
	case GLOBUS_GSI_CERT_UTILS_TYPE_RFC_RESTRICTED_PROXY:
		cert_type = GLOBUS_GSI_CERT_UTILS_TYPE_RFC_IMPERSONATION_PROXY;
		break;
	case GLOBUS_GSI_CERT_UTILS_TYPE_GSI_3_IMPERSONATION_PROXY:
	case GLOBUS_GSI_CERT_UTILS_TYPE_GSI_3_LIMITED_PROXY:
	case GLOBUS_GSI_CERT_UTILS_TYPE_GSI_2_PROXY:
	case GLOBUS_GSI_CERT_UTILS_TYPE_GSI_2_LIMITED_PROXY:
	case GLOBUS_GSI_CERT_UTILS_TYPE_RFC_IMPERSONATION_PROXY:
	case GLOBUS_GSI_CERT_UTILS_TYPE_RFC_LIMITED_PROXY:
	default:
			// Use the same certificate type
		break;
	}
	result = globus_gsi_proxy_handle_set_type( new_proxy, cert_type);
	if ( result != GLOBUS_SUCCESS ) {
		rc = -1;
		error_line = __LINE__;
		goto cleanup;
	}

	/* TODO Do we have to destroy and re-create bio, or can we reuse it? */
	bio = BIO_new( BIO_s_mem() );
	if ( bio == NULL ) {
		rc = -1;
		error_line = __LINE__;
		goto cleanup;
	}

	result = globus_gsi_proxy_sign_req( new_proxy, source_cred, bio );
	if ( result != GLOBUS_SUCCESS ) {
		rc = -1;
		error_line = __LINE__;
		goto cleanup;
	}

		// Now we need to stuff the certificate chain into in the bio.
		// This consists of the signed certificate and its whole chain.
	result = globus_gsi_cred_get_cert( source_cred, &cert );
	if ( result != GLOBUS_SUCCESS ) {
		rc = -1;
		error_line = __LINE__;
		goto cleanup;
	}
	i2d_X509_bio( bio, cert );
	X509_free( cert );
	cert = NULL;

	result = globus_gsi_cred_get_cert_chain( source_cred, &cert_chain );
	if ( result != GLOBUS_SUCCESS ) {
		rc = -1;
		error_line = __LINE__;
		goto cleanup;
	}

	for( idx = 0; idx < sk_X509_num( cert_chain ); idx++ ) {
		X509 *next_cert;
		next_cert = sk_X509_value( cert_chain, idx );
		i2d_X509_bio( bio, next_cert );
	}
	sk_X509_pop_free( cert_chain, X509_free );
	cert_chain = NULL;

	if ( bio_to_buffer( bio, &buffer, &buffer_len ) == FALSE ) {
		rc = -1;
		error_line = __LINE__;
		goto cleanup;
	}

	if ( send_data_func( send_data_ptr, buffer, buffer_len ) != 0 ) {
		rc = -1;
		error_line = __LINE__;
		goto cleanup;
	}

 cleanup:
	/* TODO Extract Globus error message if result isn't GLOBUS_SUCCESS */
	if ( error_line ) {
		char buff[1024];
		snprintf( buff, sizeof(buff), "x509_send_delegation failed at line %d",
				  error_line );
		set_error_string( buff );
	}

	if ( bio ) {
		BIO_free( bio );
	}
	if ( buffer ) {
		free( buffer );
	}
	if ( new_proxy ) {
		globus_gsi_proxy_handle_destroy( new_proxy );
	}
	if ( source_cred ) {
		globus_gsi_cred_handle_destroy( source_cred );
	}
	if ( cert ) {
		X509_free( cert );
	}
	if ( cert_chain ) {
		sk_X509_pop_free( cert_chain, X509_free );
	}

	return rc;
#endif
}


int
x509_receive_delegation( const char *destination_file,
						 int (*recv_data_func)(void *, void **, size_t *), 
						 void *recv_data_ptr,
						 int (*send_data_func)(void *, void *, size_t),
						 void *send_data_ptr )
{
#if !defined(HAVE_EXT_GLOBUS)

	_globus_error_message = "This version of Condor doesn't support X509 credentials!" ;
	return -1;

#else
	int rc = 0;
	int error_line = 0;
	globus_result_t result = GLOBUS_SUCCESS;
	globus_gsi_cred_handle_t proxy_handle =  NULL;
	globus_gsi_proxy_handle_t request_handle = NULL;
	char *buffer = NULL;
	int buffer_len = 0;
	BIO *bio = NULL;

	if ( activate_globus_gsi() != 0 ) {
		return -1;
	}

	result = globus_gsi_proxy_handle_init( &request_handle, NULL );
	if ( result != GLOBUS_SUCCESS ) {
		rc = -1;
		error_line = __LINE__;
		goto cleanup;
	}

	bio = BIO_new( BIO_s_mem() );
	if ( bio == NULL ) {
		rc = -1;
		error_line = __LINE__;
		goto cleanup;
	}

	result = globus_gsi_proxy_create_req( request_handle, bio );
	if ( result != GLOBUS_SUCCESS ) {
		rc = -1;
		error_line = __LINE__;
		goto cleanup;
	}


	if ( bio_to_buffer( bio, &buffer, &buffer_len ) == FALSE ) {
		rc = -1;
		error_line = __LINE__;
		goto cleanup;
	}

	BIO_free( bio );
	bio = NULL;

	if ( send_data_func( send_data_ptr, buffer, buffer_len ) != 0 ) {
		rc = -1;
		error_line = __LINE__;
		goto cleanup;
	}

	free( buffer );
	buffer = NULL;

	if ( recv_data_func( recv_data_ptr, (void **)&buffer, (size_t*)&buffer_len ) != 0 ) {
		rc = -1;
		error_line = __LINE__;
		goto cleanup;
	}

	if ( buffer_to_bio( buffer, buffer_len, &bio ) == FALSE ) {
		rc = -1;
		error_line = __LINE__;
		goto cleanup;
	}

	result = globus_gsi_proxy_assemble_cred( request_handle, &proxy_handle,
											 bio );
	if ( result != GLOBUS_SUCCESS ) {
		rc = -1;
		error_line = __LINE__;
		goto cleanup;
	}

	/* globus_gsi_cred_write_proxy() declares its second argument non-const,
	 * but never modifies it. The cast gets rid of compiler warnings.
	 */
	result = globus_gsi_cred_write_proxy( proxy_handle, (char *)destination_file );
	if ( result != GLOBUS_SUCCESS ) {
		rc = -1;
		error_line = __LINE__;
		goto cleanup;
	}

 cleanup:
	/* TODO Extract Globus error message if result isn't GLOBUS_SUCCESS */
	if ( error_line ) {
		char buff[1024];
		snprintf( buff, sizeof(buff), "x509_receive_delegation failed "
				  "at line %d", error_line );
		set_error_string( buff );
	}

	if ( bio ) {
		BIO_free( bio );
	}
	if ( buffer ) {
		free( buffer );
	}
	if ( request_handle ) {
		globus_gsi_proxy_handle_destroy( request_handle );
	}
	if ( proxy_handle ) {
		globus_gsi_cred_handle_destroy( proxy_handle );
	}

	return rc;
#endif
}

void parse_resource_manager_string( const char *string, char **host,
									char **port, char **service,
									char **subject )
{
	char *p;
	char *q;
	size_t len = strlen( string );

	char *my_host = (char *)calloc( len+1, sizeof(char) );
	char *my_port = (char *)calloc( len+1, sizeof(char) );
	char *my_service = (char *)calloc( len+1, sizeof(char) );
	char *my_subject = (char *)calloc( len+1, sizeof(char) );

	p = my_host;
	q = my_host;

	while ( *string != '\0' ) {
		if ( *string == ':' ) {
			if ( q == my_host ) {
				p = my_port;
				q = my_port;
				string++;
			} else if ( q == my_port || q == my_service ) {
				p = my_subject;
				q = my_subject;
				string++;
			} else {
				*(p++) = *(string++);
			}
		} else if ( *string == '/' ) {
			if ( q == my_host || q == my_port ) {
				p = my_service;
				q = my_service;
				string++;
			} else {
				*(p++) = *(string++);
			}
		} else {
			*(p++) = *(string++);
		}
	}

	if ( host != NULL ) {
		*host = my_host;
	} else {
		free( my_host );
	}

	if ( port != NULL ) {
		*port = my_port;
	} else {
		free( my_port );
	}

	if ( service != NULL ) {
		*service = my_service;
	} else {
		free( my_service );
	}

	if ( subject != NULL ) {
		*subject = my_subject;
	} else {
		free( my_subject );
	}
}

/* Returns true (non-0) if path looks like an URL that Globus
   (specifically, globus-url-copy) can handle

   Expected use: is the input/stdout file actually a Globus URL
   that we can just hand off to Globus instead of a local file
   that we need to rewrite as a Globus URL.

   Probably doesn't make sense to use if universe != globus
*/
int
is_globus_friendly_url(const char * path)
{
	if(path == 0)
		return 0;
	// Should this be more aggressive and allow anything with ://?
	return 
		strstr(path, "http://") == path ||
		strstr(path, "https://") == path ||
		strstr(path, "ftp://") == path ||
		strstr(path, "gsiftp://") == path ||
		0;
}

