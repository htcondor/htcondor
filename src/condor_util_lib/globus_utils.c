/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"

#include "globus_utils.h"

#if defined(CONDOR_GSI)
#     include "globus_gsi_credential.h"
#     include "globus_gsi_system_config.h"
#     include "globus_gsi_system_config_constants.h"
#     include "gssapi.h"
#endif

#define DEFAULT_MIN_TIME_LEFT 8*60*60;


static char * _globus_error_message = NULL;

char *GlobusJobStatusName( int status )
{
#if defined(CONDOR_G)
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
		return "??????";
	}
#else
	return "??????";
#endif
}

const char *
x509_error_string()
{
	return _globus_error_message;
}

/* Activate the globus gsi modules for use by functions in this file.
 * Returns zero if the modules were successfully activated. Returns -1 if
 * something went wrong.
 */
static
int
activate_globus_gsi()
{
#if !defined(CONDOR_GSI)
	_globus_error_message = "This version of Condor doesn't support X509 credentials!" ;
	return -1;
#else
	static int globus_gsi_activated = 0;

	if ( globus_gsi_activated != 0 ) {
		return 0;
	}

/* This module is activated by GLOBUS_GSI_CREDENTIAL_MODULE
	if ( globus_module_activate(GLOBUS_GSI_SYSCONFIG_MODULE) ) {
		_globus_error_message = "couldn't activate globus gsi sysconfig module";
		return -1;
	}
*/

	if ( globus_module_activate(GLOBUS_GSI_CREDENTIAL_MODULE) ) {
		_globus_error_message = "couldn't activate globus gsi credential module";
		return -1;
	}

	if ( globus_module_activate(GLOBUS_GSI_GSSAPI_MODULE) ) {
		_globus_error_message = "couldn't activate globus gsi gssapi module";
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
get_x509_proxy_filename()
{
	char *proxy_file = NULL;
#if defined(CONDOR_GSI)
	globus_gsi_proxy_file_type_t     file_type    = GLOBUS_PROXY_FILE_INPUT;

	if ( activate_globus_gsi() != 0 ) {
		return NULL;
	}

	if ( GLOBUS_GSI_SYSCONFIG_GET_PROXY_FILENAME(&proxy_file, file_type) !=
		 GLOBUS_SUCCESS ) {
		_globus_error_message = "unable to locate proxy file";
	}
#endif
	return proxy_file;
}

/* Return the subject name of a given proxy cert. 
  On error, return NULL.
  On success, return a pointer to a null-terminated string.
  IT IS THE CALLER'S RESPONSBILITY TO DE-ALLOCATE THE STIRNG
  WITH free().
 */
char *
x509_proxy_subject_name( const char *proxy_file )
{
#if !defined(CONDOR_GSI)
	_globus_error_message = "This version of Condor doesn't support X509 credentials!" ;
	return NULL;
#else

	globus_gsi_cred_handle_t         handle       = NULL;
	globus_gsi_cred_handle_attrs_t   handle_attrs = NULL;
	char *subject_name = NULL;
	int must_free_proxy_file = FALSE;

	if ( activate_globus_gsi() != 0 ) {
		return NULL;
	}

	if (globus_gsi_cred_handle_attrs_init(&handle_attrs)) {
		_globus_error_message = "problem during internal initialization1";
		goto cleanup;
	}

	if (globus_gsi_cred_handle_init(&handle, handle_attrs)) {
		_globus_error_message = "problem during internal initialization2";
		goto cleanup;
	}

	/* Check for proxy file */
	if (proxy_file == NULL) {
		proxy_file = get_x509_proxy_filename();
		if (proxy_file == NULL) {
			goto cleanup;
		}
		must_free_proxy_file = TRUE;
	}

	// We should have a proxy file, now, try to read it
	if (globus_gsi_cred_read_proxy(handle, proxy_file)) {
		_globus_error_message = "unable to read proxy file";
	   goto cleanup;
	}

	if (globus_gsi_cred_get_subject_name(handle, &subject_name)) {
		_globus_error_message = "unable to extract subject name";
		goto cleanup;
	}

 cleanup:
	if (must_free_proxy_file) {
		free(proxy_file);
	}

	if (handle_attrs) {
		globus_gsi_cred_handle_attrs_destroy(handle_attrs);
	}

	if (handle) {
		globus_gsi_cred_handle_destroy(handle);
	}

	return subject_name;

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
x509_proxy_identity_name( const char *proxy_file )
{
#if !defined(CONDOR_GSI)
	_globus_error_message = "This version of Condor doesn't support X509 credentials!" ;
	return NULL;
#else

	globus_gsi_cred_handle_t         handle       = NULL;
	globus_gsi_cred_handle_attrs_t   handle_attrs = NULL;
	char *subject_name = NULL;
	int must_free_proxy_file = FALSE;

	if ( activate_globus_gsi() != 0 ) {
		return NULL;
	}

	if (globus_gsi_cred_handle_attrs_init(&handle_attrs)) {
		_globus_error_message = "problem during internal initialization1";
		goto cleanup;
	}

	if (globus_gsi_cred_handle_init(&handle, handle_attrs)) {
		_globus_error_message = "problem during internal initialization2";
		goto cleanup;
	}

	/* Check for proxy file */
	if (proxy_file == NULL) {
		proxy_file = get_x509_proxy_filename();
		if (proxy_file == NULL) {
			goto cleanup;
		}
		must_free_proxy_file = TRUE;
	}

	// We should have a proxy file, now, try to read it
	if (globus_gsi_cred_read_proxy(handle, proxy_file)) {
		_globus_error_message = "unable to read proxy file";
	   goto cleanup;
	}

	if (globus_gsi_cred_get_identity_name(handle, &subject_name)) {
		_globus_error_message = "unable to extract identity name";
		goto cleanup;
	}

 cleanup:
	if (must_free_proxy_file) {
		free(proxy_file);
	}

	if (handle_attrs) {
		globus_gsi_cred_handle_attrs_destroy(handle_attrs);
	}

	if (handle) {
		globus_gsi_cred_handle_destroy(handle);
	}

	return subject_name;

#endif /* !defined(GSS_AUTHENTICATION) */
}

/* Return the time at which the proxy expires. On error, return -1.
 */
time_t
x509_proxy_expiration_time( const char *proxy_file )
{
#if !defined(CONDOR_GSI)
	_globus_error_message = "This version of Condor doesn't support X509 credentials!" ;
	return -1;
#else

    globus_gsi_cred_handle_t         handle       = NULL;
    globus_gsi_cred_handle_attrs_t   handle_attrs = NULL;
	time_t expiration_time = -1;
	time_t time_left;
	int must_free_proxy_file = FALSE;

	if ( activate_globus_gsi() != 0 ) {
		return -1;
	}

    if (globus_gsi_cred_handle_attrs_init(&handle_attrs)) {
        _globus_error_message = "problem during internal initialization";
        goto cleanup;
    }

    if (globus_gsi_cred_handle_init(&handle, handle_attrs)) {
        _globus_error_message = "problem during internal initialization";
        goto cleanup;
    }

    /* Check for proxy file */
    if (proxy_file == NULL) {
        proxy_file = get_x509_proxy_filename();
        if (proxy_file == NULL) {
            goto cleanup;
        }
		must_free_proxy_file = TRUE;
    }

    // We should have a proxy file, now, try to read it
    if (globus_gsi_cred_read_proxy(handle, proxy_file)) {
       _globus_error_message = "unable to read proxy file";
       goto cleanup;
    }

	if (globus_gsi_cred_get_lifetime(handle, &time_left)) {
		_globus_error_message = "unable to extract expiration time";
        goto cleanup;
    }

	expiration_time = time(NULL) + time_left;

 cleanup:
    if (must_free_proxy_file) {
        free(proxy_file);
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
#if !defined(CONDOR_GSI)
	_globus_error_message = "This version of Condor doesn't support X509 credentials!" ;
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
#if !defined(CONDOR_GSI)

	_globus_error_message = "This version of Condor doesn't support X509 credentials!";
	return -1;

#else
	int rc;
	int min_stat;
	gss_buffer_desc import_buf;
	gss_cred_id_t cred_handle;
	static char buf_value[4096];
	int must_free_proxy_file = FALSE;

	if ( activate_globus_gsi() != 0 ) {
		return -1;
	}

	/* Check for proxy file */
	if (proxy_file == NULL) {
		proxy_file = get_x509_proxy_filename();
		if (proxy_file == NULL) {
			goto cleanup;
		}
		must_free_proxy_file = TRUE;
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
		_globus_error_message = buf_value;
		return -1;
	}

	gss_release_cred( &min_stat, &cred_handle );

 cleanup:
    if (must_free_proxy_file) {
        free(proxy_file);
    }

	return 0;
#endif /* !defined(CONDOR_GSI) */
}

int
check_x509_proxy( const char *proxy_file )
{
#if !defined(CONDOR_GSI)

	_globus_error_message = "This version of Condor doesn't support X509 credentials!" ;
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
		_globus_error_message =	"proxy has expired";
		return -1;
	}

	if ( time_diff < min_time_left ) {
		_globus_error_message =	"proxy lifetime too short";
		return -1;
	}

	return 0;

#endif /* !defined(GSS_AUTHENTICATION) */
}


int
have_condor_g()
{
#if defined(CONDOR_G)
	
	// Our Condor-G test will be to see if we've defined a GRIDMANAGER. 
	// If we don't have one, then this install does not support Condor-G.	
	char *gridmanager;

	gridmanager = NULL;
	gridmanager = param("GRIDMANAGER");
	if(gridmanager) {
		free(gridmanager);
		return 1;
	} else {
		return 0;
	}
#else
	return 0;
#endif
}

void parse_resource_manager_string( const char *string, char **host,
									char **port, char **service,
									char **subject )
{
	char *p;
	char *q;
	int len = strlen( string );

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

#if 0 /* We're not currently using these functions */

/*
 * Function: simple_query_ldap()
 *
 * Connect to the ldap server, and return all the value of the
 * fields "attribute" contained in the entries maching a search string.
 *
 * Parameters:
 *     attribute -     field for which we want the list of value returned.
 *     maximum -       maximum number of strings returned in the list.
 *     search_string - Search string to use to select the entry for which
 *                     we will return the values of the field attribute.
 * 
 * Returns:
 *     a list of strings containing the result.
 *     
 */ 
#if defined(GLOBUS_SUPPORT) && defined(HAS_LDAP)
static
int
retrieve_attr_values(
    LDAP *            ldap_server,
    LDAPMessage *     reply,
    char *            attribute,
    int               maximum,
    char *            search_string,
    globus_list_t **  value_list)
{
	LDAPMessage *entry;
	int cnt = 0;
    
	for ( entry = ldap_first_entry(ldap_server, reply);
		  (entry) && (maximum);
		  entry = ldap_next_entry(ldap_server, entry) ) {
		char *attr_value;
		char *a;
		BerElement *ber;
		int numValues;
		char **values;
		int i;

		for (a = ldap_first_attribute(ldap_server,entry,&ber);
			 a; a = ldap_next_attribute(ldap_server,entry,ber) ) {
			if (strcmp(a, attribute) == 0) {
				/* got our match, so copy and return it*/
				cnt++;
				values = ldap_get_values(ldap_server,entry,a);
				numValues = ldap_count_values(values);
				for (i=0; i<numValues; i++) {
					attr_value = strdup(values[i]);
					globus_list_insert(value_list,attr_value);
					if (--maximum==0)
						break;
				}
				ldap_value_free(values);
		
				/* we never have 2 time the same attibute for the same entry,
				   steveF said this is not always the case XXX */
				break;
			}
		}
	}
	return cnt;

} /* retrieve_attr_values */
#endif /* defined(GLOBUS_SUPPORT) */

#if defined(GLOBUS_SUPPORT) && defined(HAS_LDAP)
/*
static
void
set_ld_timeout(LDAP  *ld, int timeout_val, char *env_string)
{
	if(timeout_val > 0) {
		ld->ld_timeout=timeout_val;
        } else { 
		char *tmp=getenv(env_string);
		int tmp_int=0;
		if(tmp) tmp_int=atoi(tmp);
		if(tmp_int>0) ld->ld_timeout=tmp_int;
	}
}
*/
#endif /* defined(GLOBUS_SUPPORT) */

#if defined(GLOBUS_SUPPORT) && defined(HAS_LDAP)
static
int
simple_query_ldap(
    char *            attribute,
    int               maximum,
    char *            search_string,
    globus_list_t **  value_list,
	char *            server,
	int               port,
	char *            base_dn)
{
	LDAP *ldap_server;
	LDAPMessage *reply;
	LDAPMessage *entry;
	char *attrs[3];
	int rc;
	int match, msgidp;
    struct timeval timeout;

	*value_list = GLOBUS_NULL;
	match=0;
    
	/* connect to the ldap server */
	if((ldap_server = ldap_open(server, port)) == GLOBUS_NULL) {
		ldap_perror(ldap_server, "rsl_assist:ldap_open");
		free(server);
		free(base_dn);
		return -1;
	}
	free(server);

    // OpenLDAP 2 changed API
	//set_ld_timeout(ldap_server, 0, "GRID_INFO_TIMEOUT");

	/* bind anonymously (we can only read public records now */
	if(ldap_simple_bind_s(ldap_server, "", "") != LDAP_SUCCESS) {
		ldap_perror(ldap_server, "rsl_assist:ldap_simple_bind_s");
		ldap_unbind(ldap_server);
		free(base_dn);
		return -1;
	}
    
	/* I should verify the attribute is a valid string...     */
	/* the function allows only one attribute to be returned  */
	attrs[0] = attribute;
	attrs[1] = GLOBUS_NULL;
    
	/* do a search of the entire ldap tree,
	 * and return the desired attribute
	 */
    // OpenLDAP 2 changed the API. ldap_search is deprecated

    timeout.tv_sec  = 0;
    timeout.tv_usec = 0;
    if ( rc = ldap_search_ext( ldap_server, base_dn, LDAP_SCOPE_SUBTREE,
                                  search_string, attrs, 0, NULL, 
                                  NULL, &timeout, -1, &msgidp ) == -1 ) {
		ldap_perror( ldap_server, "rsl_assist:ldap_search" );
		return( msgidp );
	}

//	if ( ldap_search( ldap_server, base_dn, LDAP_SCOPE_SUBTREE,
//					  search_string, attrs, 0 ) == -1 ) {
//		ldap_perror( ldap_server, "rsl_assist:ldap_search" );
//		return( ldap_server->ld_errno );
//	}

	while ( (rc = ldap_result( ldap_server, LDAP_RES_ANY, 0, NULL, &reply ))
			== LDAP_RES_SEARCH_ENTRY || (rc==-1)) {
		if(rc==-1) {       // This is timeout
			continue;
		}
		match += retrieve_attr_values(ldap_server,reply, attrs[0],
									  maximum, search_string, value_list);
	}

	if( rc == -1 ) {
		ldap_perror(ldap_server, "rsl_assist:ldap_search");
	}
	/* to avoid a leak in ldap code */
	ldap_msgfree(reply);

	/* disconnect from the server */
	ldap_unbind(ldap_server);
	free(base_dn);

	if(match)
		return GLOBUS_SUCCESS;
	else
		return rc;

} /* simple_query_ldap() */
#endif /* defined(GLOBUS_SUPPORT) */


int
check_globus_rm_contacts(char* resource)
{
#if !defined(GLOBUS_SUPPORT) || !defined(HAS_LDAP)
	fprintf( stderr, "This is not a Globus-enabled version of Condor!\n" );
	exit( 1 );
	return 0;
#else

	int rc;
	int len;
	char *host;
	char *ptr;
	char *ptr2;
	char *service;
	char *search_string;
	char *format = "(&(objectclass=GlobusService)(hn=%s*)(service=%s))";
	globus_list_t *rm_contact_list = GLOBUS_NULL;

	// Pick out the hostname of the resource
	// It's at the beginning of the resource string
	len = strcspn( resource, ":/" );
	host = (char *)malloc( (len + 1) * sizeof(char) );
	strncpy( host, resource, len );
	host[len] = '\0';

	// Pick out the servicename of the resource, if it's there
	// It'll be after the first '/' character in the string unless two
	// colons precede it (confusing, eh?)
	ptr = strchr( resource, '/' );
	ptr2 = strchr( resource, ':' );
	if ( ptr2 != NULL )
		ptr2 = strchr( ptr2 + 1, ':' );
	if ( ptr != NULL && ( ptr2 == NULL || ptr2 > ptr ) ) {
		ptr++;
		len = strcspn( ptr, ":" );
		service = (char *)malloc( (len + 1) * sizeof(char) );
		strncpy( service, ptr, len );
		service[len] = '\0';
	} else {
		service = strdup( "*" );
	}

	search_string = (char *)malloc( strlen(format) + 
									strlen(host)   +
									strlen(service) + 1 );
	if ( !search_string ) {
		free( service );
		free( host );
		return 1;
	}

	sprintf( search_string, format, host, service );

	rc = simple_query_ldap( "contact", 1, search_string, &rm_contact_list,
							host, 2135, "o=Grid" );

	free( search_string );
	free( service );
	free( host );

	if ( rc == GLOBUS_SUCCESS && rm_contact_list ) {
		globus_list_remove( &rm_contact_list, rm_contact_list );
		return 0;
	} else {
		return 1;
	}

#endif /* !defined(GLOBUS_SUPPORT) */

} /* check_globus_rm_contact() */

#endif /* 0 */

