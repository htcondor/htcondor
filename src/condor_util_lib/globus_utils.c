/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"


#if defined(GLOBUS_SUPPORT)
#	include "globus_common.h"
#	include "globus_rsl_assist.h"
#endif

#if defined(GSS_AUTHENTICATION)
#   include "sslutils.h"
#endif

#define DEFAULT_MIN_TIME_LEFT 8*60*60;


char *GlobusJobStatusNames[] = {
	"UNSUBMITTED",
	"PENDING",
	"ACTIVE",
	"FAILED",
	"DONE",
	"SUSPENDED"
};

int
check_x509_proxy( char *proxy_file )
{
#if !defined(GSS_AUTHENTICATION)

	fprintf( stderr, "This version of Condor doesn't support X509 authentication!\n" );
	return 1;

#else

	char *min_time_left_param = NULL;
	int min_time_left;
	proxy_cred_desc *pcd = NULL;
	time_t time_after;
	time_t time_now;
	time_t time_diff;
	ASN1_UTCTIME *asn1_time = NULL;
	struct stat stx;

	/* initialize SSLeay and the error strings */
	ERR_load_prxyerr_strings(0);
	SSLeay_add_ssl_algorithms();

	/* Load proxy */
	if (!proxy_file) 
		proxy_get_filenames(1, NULL, NULL, &proxy_file, NULL, NULL);

	if (!proxy_file) {
		fprintf(stderr,"ERROR: unable to determine proxy file name\n");
		return 1;
	}

	if (stat(proxy_file,&stx) != 0) {
		fprintf(stderr, "ERROR: file %s not found\n",proxy_file);
		return 1;
	}

	pcd = proxy_cred_desc_new();
	if (!pcd) {
		fprintf(stderr,"ERROR: problem during internal initialization\n");
		return 1;
	}

	if (proxy_load_user_cert(pcd, proxy_file, NULL)) {
		fprintf(stderr,"ERROR: unable to load proxy");
		return 1;
	}

	if ((pcd->upkey = X509_get_pubkey(pcd->ucert)) == NULL) {
		fprintf(stderr,"ERROR: unable to load public key from proxy");
		return 1;
	}

	/* validity: set time_diff to time to expiration (in seconds) */
	asn1_time = ASN1_UTCTIME_new();
	X509_gmtime_adj(asn1_time,0);
	time_now = ASN1_UTCTIME_mktime(asn1_time);
	time_after = ASN1_UTCTIME_mktime(X509_get_notAfter(pcd->ucert));
	time_diff = time_after - time_now ;

	/* check validity */
	min_time_left_param = param( "CRED_MIN_TIME_LEFT" );

	if ( min_time_left_param != NULL ) {
		min_time_left = atoi( min_time_left_param );
	} else {
		min_time_left = DEFAULT_MIN_TIME_LEFT;
	}

	if ( time_diff < 0 ) {
		fprintf(stderr,"ERROR: proxy has expired\n");
		return 1;
	}

	if ( time_diff < min_time_left ) {
		dprintf(D_ALWAYS,"ERROR: proxy lifetime too short\n");
		return 1;
	}

	return 0;

#endif
}


int
have_globus_support()
{
#if defined(GLOBUS_SUPPORT)
	return 1;
#else
	return 0;
#endif
}


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
#if defined(GLOBUS_SUPPORT)
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
#endif

#if defined(GLOBUS_SUPPORT)
static
void
set_ld_timeout(LDAP  *ld, int timeout_val, char *env_string)
{
	if(timeout_val > 0) {
		ld->ld_timeout=timeout_val;
	} else { /* check the environment variable */
		char *tmp=getenv(env_string);
		int tmp_int=0;
		if(tmp) tmp_int=atoi(tmp);
		if(tmp_int>0) ld->ld_timeout=tmp_int;
	}
}
#endif

#if defined(GLOBUS_SUPPORT)
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
	int match;

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

	set_ld_timeout(ldap_server, 0, "GRID_INFO_TIMEOUT");

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
	if ( ldap_search( ldap_server, base_dn, LDAP_SCOPE_SUBTREE,
					  search_string, attrs, 0 ) == -1 ) {
		ldap_perror( ldap_server, "rsl_assist:ldap_search" );
		return( ldap_server->ld_errno );
	}

	while ( (rc = ldap_result( ldap_server, LDAP_RES_ANY, 0, NULL, &reply ))
			== LDAP_RES_SEARCH_ENTRY || ldap_server->ld_errno==LDAP_TIMEOUT) {
		if(ldap_server->ld_errno==LDAP_TIMEOUT) {
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
#endif


int
check_globus_rm_contacts(char* resource)
{
#if !defined(GLOBUS_SUPPORT)
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

#endif

} /* check_globus_rm_contact() */

char *rsl_stringify( char *string )
{
	static char buffer[5000];

	strcpy( buffer, "'" );
	while ( strlen( string ) > 0 ) {
		char *macro_start = strstr( string, "$(" );
		char *macro_stop = macro_start == NULL ? NULL :
			strstr( macro_start, ")" );
		if ( macro_start && macro_stop ) {
			strncat( buffer, string, macro_start - string );
			strcat( buffer, "'#" );
			strncat( buffer, macro_start, macro_stop - macro_start + 1 );
			strcat( buffer, "#'" );
			string = macro_stop + 1;
		} else {
			strcat( buffer, string );
			string += strlen( string );
		}
	}
	strcat( buffer, "'" );

	return buffer;
}
