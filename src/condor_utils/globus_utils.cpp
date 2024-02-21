/***************************************************************
 *
 * Copyright (C) 1990-2011, Condor Team, Computer Sciences Department,
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
#include "condor_auth_ssl.h"
#include "DelegationInterface.h"
#include "subsystem_info.h"

#if defined(DLOPEN_SECURITY_LIBS)
#include <dlfcn.h>
#endif

#include "globus_utils.h"

#include <openssl/x509v3.h>

static std::string _globus_error_message;

// Symbols from libvomsapi
#if defined(HAVE_EXT_VOMS)
void (*VOMS_Destroy_ptr)(
	struct vomsdata *) = NULL;
char *(*VOMS_ErrorMessage_ptr)(
	struct vomsdata *, int, char *, int) = NULL;
struct vomsdata *(*VOMS_Init_ptr)(
	char *, char *) = NULL;
int (*VOMS_Retrieve_ptr)(
	X509 *, STACK_OF(X509) *, int, struct vomsdata *, int *) = NULL;
int (*VOMS_SetVerificationType_ptr)(
	int, struct vomsdata *, int *) = NULL;
#endif /* defined(HAVE_EXT_VOMS) */

void
warn_on_gsi_config()
{
	static time_t last_warn = 0;
	time_t now = time(NULL);
	if ( now < last_warn + (12*60*60) ) {
		return;
	}
	last_warn = now;
	if ( ! param_boolean("WARN_ON_GSI_CONFIGURATION", true) ) {
		return;
	}
	SubsystemInfo* my_subsys = get_mySubSystem();
	SubsystemType subsys_type = my_subsys ? my_subsys->getType() : SUBSYSTEM_TYPE_MIN;
	if ( subsys_type == SUBSYSTEM_TYPE_TOOL || subsys_type == SUBSYSTEM_TYPE_SUBMIT ) {
		fprintf(stderr, "WARNING: GSI authentication is enabled by your security configuration! GSI is no longer supported.\n");
		fprintf(stderr, "For details, see https://htcondor.org/news/plan-to-replace-gst-in-htcss/\n");
	} else {
		dprintf(D_ALWAYS, "WARNING: GSI authentication is is enabled by your security configuration! GSI is no longer supported. (Will warn again after 12 hours)\n");
		dprintf(D_ALWAYS, "For details, see https://htcondor.org/news/plan-to-replace-gst-in-htcss/\n");
	}
}

const char *
x509_error_string( void )
{
	return _globus_error_message.c_str();
}

static
void
set_error_string( const char *message )
{
	_globus_error_message = message;
}

/* Return the path to the X509 proxy file as determined by GSI/SSL.
 * Returns NULL if the filename can't be determined. Otherwise, the
 * string returned must be freed with free().
 */
char *
get_x509_proxy_filename( void )
{
	char *proxy_file = getenv("X509_USER_PROXY");
	if ( proxy_file == nullptr ) {
#if !defined(WIN32)
		std::string tmp_file;
		formatstr(tmp_file, "/tmp/x509up_u%d", geteuid());
		proxy_file = strdup(tmp_file.c_str());
#endif
	} else {
		proxy_file = strdup(proxy_file);
	}
	return proxy_file;
}


#if defined(HAVE_EXT_VOMS)
static
int
initialize_voms()
{
#if !defined(HAVE_EXT_VOMS)
	set_error_string( "This version of Condor doesn't support VOMS" );
	return -1;
#else
	static bool voms_initialized = false;
	static bool initialization_failed = false;

	if ( voms_initialized ) {
		return 0;
	}
	if ( initialization_failed ) {
		return -1;
	}

	if ( Condor_Auth_SSL::Initialize() == false ) {
		// Error in the dlopen/sym calls for libssl, return failure.
		_globus_error_message = "Failed to open SSL library";
		initialization_failed = true;
		return -1;
	}

#if defined(DLOPEN_SECURITY_LIBS)
	void *dl_hdl;

	if ( (dl_hdl = dlopen(LIBVOMSAPI_SO, RTLD_LAZY)) == NULL ||
		 !(VOMS_Destroy_ptr = (void (*)(vomsdata*))dlsym(dl_hdl, "VOMS_Destroy")) ||
		 !(VOMS_ErrorMessage_ptr = (char* (*)(vomsdata*, int, char*, int))dlsym(dl_hdl, "VOMS_ErrorMessage")) ||
		 !(VOMS_Init_ptr = (vomsdata* (*)(char*, char*))dlsym(dl_hdl, "VOMS_Init")) ||
		 !(VOMS_Retrieve_ptr = (int (*)(X509*, STACK_OF(X509)*, int, struct vomsdata*, int*))dlsym(dl_hdl, "VOMS_Retrieve")) ||
		 !(VOMS_SetVerificationType_ptr = (int (*)(int, vomsdata*, int*))dlsym(dl_hdl, "VOMS_SetVerificationType"))
		 ) {
			 // Error in the dlopen/sym calls, return failure.
		const char *err = dlerror();
		formatstr( _globus_error_message, "Failed to open VOMS library: %s", err ? err : "Unknown error" );
		initialization_failed = true;
		return -1;
	}
#else
	VOMS_Destroy_ptr = VOMS_Destroy;
	VOMS_ErrorMessage_ptr = VOMS_ErrorMessage;
	VOMS_Init_ptr = VOMS_Init;
	VOMS_Retrieve_ptr = VOMS_Retrieve;
	VOMS_SetVerificationType_ptr = VOMS_SetVerificationType;
#endif

	voms_initialized = true;
	return 0;
#endif
}

// caller must free result
static
char* trim_quotes( char* instr ) {
	char * result;
	int  instr_len;

	if (instr == NULL) {
		return NULL;
	}

	instr_len = strlen(instr);
	// to trim, must be minimum three characters with a double quote first and last
	if ((instr_len > 2) && (instr[0] == '"') && (instr[instr_len-1]) == '"') {
		// alloc a shorter buffer, copy everything in, and null terminate
		result = (char*)malloc(instr_len - 1); // minus two quotes, plus one NULL terminator.
		strncpy(result, &(instr[1]), instr_len-2);
		result[instr_len-2] = 0;
	} else {
		result = strdup(instr);
	}

	return result;
}

// caller responsible for freeing
static
char*
quote_x509_string( char* instr) {
	char * result_string = 0;
	int    result_string_len = 0;

	char * x509_fqan_escape = 0;
	char * x509_fqan_escape_sub = 0;
	char * x509_fqan_delimiter = 0;
	char * x509_fqan_delimiter_sub = 0;

	int x509_fqan_escape_sub_len = 0;
	int x509_fqan_delimiter_sub_len = 0;

	char * tmp_scan_ptr;

	// NULL in, NULL out
	if (!instr) {
		return NULL;
	}

	// we look at first char only.  default '&'.
    if (!(x509_fqan_escape = param("X509_FQAN_ESCAPE"))) {
		x509_fqan_escape = strdup("&");
	}
	// can be multiple chars
    if (!(x509_fqan_escape_sub = param("X509_FQAN_ESCAPE_SUB"))) {
		x509_fqan_escape_sub = strdup("&amp;");
	}

	// we look at first char only.  default ','.
    if (!(x509_fqan_delimiter = param("X509_FQAN_DELIMITER"))) {
		x509_fqan_delimiter = strdup(",");
	}
	// can be multiple chars
    if (!(x509_fqan_delimiter_sub = param("X509_FQAN_DELIMITER_SUB"))) {
		x509_fqan_delimiter_sub = strdup("&comma;");
	}


	// phase 0, trim quotes off if needed
	// use tmp_scan_ptr to temporarily hold trimmed strings while being reassigned.
	tmp_scan_ptr = trim_quotes(x509_fqan_escape);
	free (x509_fqan_escape);
	x509_fqan_escape = tmp_scan_ptr;

	tmp_scan_ptr = trim_quotes(x509_fqan_escape_sub);
	free (x509_fqan_escape_sub);
	x509_fqan_escape_sub = tmp_scan_ptr;
	x509_fqan_escape_sub_len = strlen(x509_fqan_escape_sub);

	tmp_scan_ptr = trim_quotes(x509_fqan_delimiter);
	free (x509_fqan_delimiter);
	x509_fqan_delimiter = tmp_scan_ptr;

	tmp_scan_ptr = trim_quotes(x509_fqan_delimiter_sub);
	free (x509_fqan_delimiter_sub);
	x509_fqan_delimiter_sub = tmp_scan_ptr;
	x509_fqan_delimiter_sub_len = strlen(x509_fqan_delimiter_sub);


	// phase 1, scan the string to compute the new length
	result_string_len = 0;
	for (tmp_scan_ptr = instr; *tmp_scan_ptr; tmp_scan_ptr++) {
		if( (*tmp_scan_ptr)==x509_fqan_escape[0] ) {
			result_string_len += x509_fqan_escape_sub_len;
		} else if( (*tmp_scan_ptr)==x509_fqan_delimiter[0] ) {
			result_string_len += x509_fqan_delimiter_sub_len;
		} else {
			result_string_len++;
		}
	}

	// phase 2, process the string into the result buffer

	// malloc new string (with NULL terminator)
	result_string = (char*) malloc (result_string_len + 1);
	ASSERT( result_string );
	*result_string = 0;
	result_string_len = 0;

	for (tmp_scan_ptr = instr; *tmp_scan_ptr; tmp_scan_ptr++) {
		if( (*tmp_scan_ptr)==x509_fqan_escape[0] ) {
			strcat(&(result_string[result_string_len]), x509_fqan_escape_sub);
			result_string_len += x509_fqan_escape_sub_len;
		} else if( (*tmp_scan_ptr)==x509_fqan_delimiter[0] ) {
			strcat(&(result_string[result_string_len]), x509_fqan_delimiter_sub);
			result_string_len += x509_fqan_delimiter_sub_len;
		} else {
			result_string[result_string_len] = *tmp_scan_ptr;
			result_string_len++;
		}
		result_string[result_string_len] = 0;
	}

	// clean up
	free(x509_fqan_escape);
	free(x509_fqan_escape_sub);
	free(x509_fqan_delimiter);
	free(x509_fqan_delimiter_sub);

	return result_string;
}
#endif /* defined(HAVE_EXT_VOMS) */

X509Credential* x509_proxy_read( const char *proxy_file )
{
	X509Credential *cred = nullptr;
	char *my_proxy_file = NULL;
	bool error = false;

	/* Check for proxy file */
	if (proxy_file == NULL) {
		my_proxy_file = get_x509_proxy_filename();
		if (my_proxy_file == NULL) {
			goto cleanup;
		}
		proxy_file = my_proxy_file;
	}

	// We should have a proxy file, now, try to read it
	cred = new X509Credential(proxy_file, "");
	if (! cred->GetCert()) {
		set_error_string( "unable to read proxy file" );
		error = true;
		goto cleanup;
	}

 cleanup:
	if (my_proxy_file) {
		free(my_proxy_file);
	}

	if (error && cred) {
		delete cred;
		cred = nullptr;
	}

	return cred;
}

// This is a slightly modified verson of globus_gsi_cert_utils_make_time()
// from the Grid Community Toolkit. It turns an ASN1_UTCTIME into a time_t.
// libressl and older openssl don't provide a way to do this conversion.
#if defined(LIBRESSL_VERSION_NUMBER)
static
time_t my_globus_gsi_cert_utils_make_time(const ASN1_UTCTIME *ctm)
{
	char *str;
	time_t offset;
	char buff1[24];
	char *p;
	int i;
	struct tm tm;
	time_t newtime = -1;

	p = buff1;
	i = ctm->length;
	str = (char *)ctm->data;
	if ((i < 11) || (i > 17)) {
		newtime = 0;
	}
	memcpy(p,str,10);
	p += 10;
	str += 10;

	if ((*str == 'Z') || (*str == '-') || (*str == '+')) {
		*(p++)='0'; *(p++)='0';
	} else {
		*(p++)= *(str++); *(p++)= *(str++);
	}
	*(p++)='Z';
	*(p++)='\0';

	if (*str == 'Z') {
		offset=0;
	} else {
		if ((*str != '+') && (str[5] != '-')) {
			newtime = 0;
		}
		offset=((str[1]-'0')*10+(str[2]-'0'))*60;
		offset+=(str[3]-'0')*10+(str[4]-'0');
		if (*str == '-') {
			offset=-offset;
		}
	}

	tm.tm_isdst = 0;
	tm.tm_year = (buff1[0]-'0')*10+(buff1[1]-'0');

	if (tm.tm_year < 70) {
		tm.tm_year+=100;
	}

	tm.tm_mon   = (buff1[2]-'0')*10+(buff1[3]-'0')-1;
	tm.tm_mday  = (buff1[4]-'0')*10+(buff1[5]-'0');
	tm.tm_hour  = (buff1[6]-'0')*10+(buff1[7]-'0');
	tm.tm_min   = (buff1[8]-'0')*10+(buff1[9]-'0');
	tm.tm_sec   = (buff1[10]-'0')*10+(buff1[11]-'0');

	newtime = (timegm(&tm) + offset*60*60);

	return newtime;
}
#endif

time_t x509_proxy_expiration_time( X509 *cert, STACK_OF(X509)* chain )
{
	time_t expiration_time = -1;
	X509 *curr_cert = cert;
	int cert_cnt = 0;
	if ( chain ) {
		cert_cnt = sk_X509_num(chain);
	}

	while ( curr_cert ) {
		time_t curr_expire = 0;
#if !defined(LIBRESSL_VERSION_NUMBER)
		int diff_days = 0;
		int diff_secs = 0;
		if ( ! ASN1_TIME_diff(&diff_days, &diff_secs, NULL, X509_get_notAfter(curr_cert)) ) {
			_globus_error_message = "Failed to calculate expration time";
			expiration_time = -1;
			break;
		} else {
			curr_expire = time(NULL) + diff_secs + 24*3600*diff_days;
		}
#else
		curr_expire = my_globus_gsi_cert_utils_make_time(X509_get_notAfter(curr_cert));
#endif
		if ( expiration_time == -1 || curr_expire < expiration_time ) {
			expiration_time = curr_expire;
		}
		if ( chain && cert_cnt ) {
			cert_cnt--;
			curr_cert = sk_X509_value(chain, cert_cnt);
		} else {
			curr_cert = NULL;
		}
	}
	return expiration_time;
}

time_t x509_proxy_expiration_time(X509Credential* cred)
{
	return x509_proxy_expiration_time(cred->GetCert(), cred->GetChain());
}

char* x509_proxy_email( X509 * /*cert*/, STACK_OF(X509)* cert_chain )
{
	X509_NAME *email_orig = NULL;
	GENERAL_NAME *gen;
	GENERAL_NAMES *gens;
	X509 *tmp_cert = NULL;
	char *email = NULL, *email2 = NULL;
	int i, j;

	// TODO We ignore the end-point certificate. This is fine for the
	//   usual case with a proxy certificate, but is wrong.
	for(i = 0; i < sk_X509_num(cert_chain) && email == NULL; ++i) {
		if((tmp_cert = sk_X509_value(cert_chain, i)) == NULL) {
			continue;
		}
		if ((email_orig = (X509_NAME *)X509_get_ext_d2i(tmp_cert, NID_pkcs9_emailAddress, 0, 0)) != NULL) {
			if ((email2 = X509_NAME_oneline(email_orig, NULL, 0)) == NULL) {
				continue;
			} else {
				// Return something that we can free().
				email = strdup(email2);
				OPENSSL_free(email2);
				break;
			}
		}
		gens = (GENERAL_NAMES *)X509_get_ext_d2i(tmp_cert, NID_subject_alt_name, 0, 0);
		if (gens) {
			for (j = 0; j < sk_GENERAL_NAME_num(gens); ++j) {
				if ((gen = sk_GENERAL_NAME_value(gens, j)) == NULL) {
					continue;
				}
				if (gen->type != GEN_EMAIL) {
					continue;
				}
				ASN1_IA5STRING *email_ia5 = gen->d.ia5;
				// Sanity checks.
				if (email_ia5->type != V_ASN1_IA5STRING) goto cleanup;
				if (!email_ia5->data || !email_ia5->length) goto cleanup;
				email2 = BUF_strdup((char *)email_ia5->data);
				// We want to return something we can free(), so make another copy.
				if (email2) {
					email = strdup(email2);
					OPENSSL_free(email2);
				}
				break;
			}
			sk_GENERAL_NAME_pop_free(gens, GENERAL_NAME_free);
		}
	}

	if (email == NULL) {
		set_error_string( "unable to extract email" );
		goto cleanup;
	}

 cleanup:
	if (email_orig) {
		X509_NAME_free(email_orig);
	}

	return email;
}

char* x509_proxy_email(X509Credential* cred)
{
	return x509_proxy_email(cred->GetCert(), cred->GetChain());
}

char* x509_proxy_subject_name(X509* cert)
{
	char *subject_name = NULL;

	char* buf = X509_NAME_oneline(X509_get_subject_name(cert), NULL, 0);
	if(buf) {
		// Return something that we can free().
		subject_name = strdup(buf);
		OPENSSL_free(buf);
	} else {
		set_error_string( "unable to extract subject name" );
	}

	return subject_name;
}

char* x509_proxy_subject_name(X509Credential* cred)
{
	return x509_proxy_subject_name(cred->GetCert());
}

char* x509_proxy_identity_name(X509 *cert, STACK_OF(X509)* cert_chain)
{
	X509* target_cert = NULL;
	X509* tmp_cert = NULL;

	if (X509_get_ext_by_NID(cert, NID_proxyCertInfo, -1) < 0) {
		target_cert = cert;
	}

	for (int i = 0; i < sk_X509_num(cert_chain) && target_cert == NULL; ++i) {
		if ((tmp_cert = sk_X509_value(cert_chain, i)) == NULL) {
			continue;
		}
		if (X509_get_ext_by_NID(tmp_cert, NID_proxyCertInfo, -1) < 0) {
			target_cert = tmp_cert;
		}
	}

	if (target_cert) {
		return x509_proxy_subject_name(target_cert);
	}

	set_error_string( "unable to extract identity name" );
	return NULL;
}

char* x509_proxy_identity_name(X509Credential* cred)
{
	return x509_proxy_identity_name(cred->GetCert(), cred->GetChain());
}

#if !defined(HAVE_EXT_VOMS)
int
extract_VOMS_info( X509*, STACK_OF(X509)*, int, char **, char **, char **)
{
	return 1;
}
#else
int
extract_VOMS_info( X509 *cert, STACK_OF(X509) *chain, int verify_type, char **voname, char **firstfqan, char **quoted_DN_and_FQAN)
{


	int ret;
	struct vomsdata *voms_data = NULL;
	struct voms *voms_cert  = NULL;
	char *subject_name = NULL;
	char **fqan = NULL;
	int voms_err;
	char *errmsg = nullptr;
	int fqan_len = 0;
	char *retfqan = NULL;
	char *tmp_scan_ptr = NULL;

	char* x509_fqan_delimiter = NULL;

	if ( initialize_voms() != 0 ) {
		return 1;
	}

	// calling this function on something that doesn't have VOMS attributes
	// should return error 1.  when the config knob disables VOMS, behave the
	// same way.
	if (!param_boolean("USE_VOMS_ATTRIBUTES", false)) {
		return 1;
	}

	subject_name = x509_proxy_identity_name(cert, chain);
	if (subject_name == NULL) {
		set_error_string( "unable to extract subject name" );
		ret = 12;
		goto end;
	}

	voms_data = (*VOMS_Init_ptr)(NULL, NULL);
	if (voms_data == NULL) {
		ret = 13;
		goto end;
	}

	if (verify_type == 0) {
		ret = (*VOMS_SetVerificationType_ptr)( VERIFY_NONE, voms_data, &voms_err );
		if (ret == 0) {
			errmsg = (*VOMS_ErrorMessage_ptr)(voms_data, voms_err, NULL, 0);
			set_error_string(errmsg);
			dprintf(D_SECURITY, "VOMS Error: %s\n", errmsg);
			free(errmsg);
			ret = voms_err;
			goto end;
		}
	}

	ret = (*VOMS_Retrieve_ptr)(cert, chain, RECURSE_CHAIN,
						voms_data, &voms_err);

	if (ret == 0 && voms_err == VERR_NOEXT) {
		// No VOMS extensions present
		ret = 1;
		goto end;
	}

	// If verification was requested and no extensions were returned,
	// try again without verification. If we get extensions that time,
	// then verification failed. In that case, issue a warning, then
	// act as if there were no extensions.
	if (ret == 0 && verify_type != 0 ) {
		errmsg = (*VOMS_ErrorMessage_ptr)(voms_data, voms_err, NULL, 0);
		dprintf(D_SECURITY, "VOMS Error: %s\n", errmsg);
		free(errmsg);

		ret = (*VOMS_SetVerificationType_ptr)( VERIFY_NONE, voms_data, &voms_err );
		if (ret == 0) {
			errmsg = (*VOMS_ErrorMessage_ptr)(voms_data, voms_err, NULL, 0);
			set_error_string(errmsg);
			dprintf(D_SECURITY, "VOMS Error: %s\n", errmsg);
			free(errmsg);
			ret = voms_err;
			goto end;
		}

		ret = (*VOMS_Retrieve_ptr)(cert, chain, RECURSE_CHAIN,
						voms_data, &voms_err);
		if (ret != 0) {
			dprintf(D_ALWAYS, "WARNING! X.509 certificate '%s' has VOMS "
					"extensions that can't be verified. Ignoring them. "
					"(To silence this warning, set USE_VOMS_ATTRIBUTES=False)\n",
					subject_name);
		}
		// Report no (verified) VOMS extensions
		ret = 1;
		goto end;
	}
	if (ret == 0) {
		errmsg = (*VOMS_ErrorMessage_ptr)(voms_data, voms_err, NULL, 0);
		set_error_string(errmsg);
		dprintf(D_SECURITY, "VOMS Error: %s\n", errmsg);
		free(errmsg);
		ret = voms_err;
		goto end;
	}

	// we only support one cert for now.  serializing and encoding all the
	// attributes is bad enough, i don't want to deal with doing this to
	// multiple certs.
	voms_cert = voms_data->data[0];

	if (voms_cert == NULL) {
		// No VOMS certs?? Treat like VOMS_Retrieve() returned VERR_NOEXT.
		ret = 1;
		goto end;
	}

	// fill in the unquoted versions of things
	if(voname) {
		*voname = strdup(voms_cert->voname ? voms_cert->voname : "");
	}

	if(firstfqan) {
		*firstfqan = strdup(voms_cert->fqan[0] ? voms_cert->fqan[0] : "");
	}

	// only construct the quoted_DN_and_FQAN if needed
	if (quoted_DN_and_FQAN) {
		// get our delimiter and trim it
		if (!(x509_fqan_delimiter = param("X509_FQAN_DELIMITER"))) {
			x509_fqan_delimiter = strdup(",");
		}
		tmp_scan_ptr = trim_quotes(x509_fqan_delimiter);
		free(x509_fqan_delimiter);
		x509_fqan_delimiter = tmp_scan_ptr;

		// calculate the length
		fqan_len = 0;

		// start with the length of the quoted DN
		tmp_scan_ptr = quote_x509_string( subject_name );
		fqan_len += strlen( tmp_scan_ptr );
		free(tmp_scan_ptr);

		// add the length of delimiter plus each voms attribute
		for (fqan = voms_cert->fqan; fqan && *fqan; fqan++) {
			// delimiter
			fqan_len += strlen(x509_fqan_delimiter);

			tmp_scan_ptr = quote_x509_string( *fqan );
			fqan_len += strlen( tmp_scan_ptr );
			free(tmp_scan_ptr);
		}

		// now malloc enough room for the quoted DN, quoted attrs, delimiters, and
		// NULL terminator
		retfqan = (char*) malloc (fqan_len+1);
		*retfqan = 0;  // set null terminiator

		// reset length counter -- we use this for efficient appending.
		fqan_len = 0;

		// start with the quoted DN
		tmp_scan_ptr = quote_x509_string( subject_name );
		strcat(retfqan, tmp_scan_ptr);
		fqan_len += strlen( tmp_scan_ptr );
		free(tmp_scan_ptr);

		// add the delimiter plus each voms attribute
		for (fqan = voms_cert->fqan; fqan && *fqan; fqan++) {
			// delimiter
			strcat(&(retfqan[fqan_len]), x509_fqan_delimiter);
			fqan_len += strlen(x509_fqan_delimiter);

			tmp_scan_ptr = quote_x509_string( *fqan );
			strcat(&(retfqan[fqan_len]), tmp_scan_ptr);
			fqan_len += strlen( tmp_scan_ptr );
			free(tmp_scan_ptr);
		}

		*quoted_DN_and_FQAN = retfqan;
	}

	ret = 0;

end:
	free(subject_name);
	free(x509_fqan_delimiter);
	if (voms_data)
		(*VOMS_Destroy_ptr)(voms_data);

	return ret;
}
#endif

int
extract_VOMS_info( X509Credential* cred, int verify_type, char **voname, char **firstfqan, char **quoted_DN_and_FQAN)
{
	return extract_VOMS_info(cred->GetCert(), cred->GetChain(), verify_type, voname, firstfqan, quoted_DN_and_FQAN);
}

int
extract_VOMS_info_from_file( const char* proxy_file, int verify_type, char **voname, char **firstfqan, char **quoted_DN_and_FQAN)
{
	int rc = 1;
	X509Credential* cred = x509_proxy_read( proxy_file );

	if ( cred == NULL ) {
		return rc;
	}

	rc = extract_VOMS_info(cred->GetCert(), cred->GetChain(), verify_type, voname, firstfqan, quoted_DN_and_FQAN);

	delete cred;

	return rc;
}

/* Return the email of a given proxy cert. 
  On error, return NULL.
  On success, return a pointer to a null-terminated string.
  IT IS THE CALLER'S RESPONSBILITY TO DE-ALLOCATE THE STIRNG
  WITH free().  
 */             
char *
x509_proxy_email( const char *proxy_file )
{
	char *email = NULL;
	X509Credential* proxy_handle = x509_proxy_read( proxy_file );

	if ( proxy_handle == NULL ) {
		return NULL;
	}

	email = x509_proxy_email( proxy_handle );

	delete proxy_handle;

	return email;
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
	char *subject_name = NULL;
	X509Credential* proxy_handle = x509_proxy_read( proxy_file );

	if ( proxy_handle == NULL ) {
		return NULL;
	}

	subject_name = x509_proxy_subject_name( proxy_handle );

	delete proxy_handle;

	return subject_name;
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
	char *subject_name = NULL;
	X509Credential* proxy_handle = x509_proxy_read( proxy_file );

	if ( proxy_handle == NULL ) {
		return NULL;
	}

	subject_name = x509_proxy_identity_name( proxy_handle );

	delete proxy_handle;

	return subject_name;
}

/* Return the time at which the proxy expires. On error, return -1.
 */
time_t
x509_proxy_expiration_time( const char *proxy_file )
{
	time_t expiration_time = -1;
	X509Credential* proxy_handle = x509_proxy_read( proxy_file );

	if ( proxy_handle == NULL ) {
		return -1;
	}

	expiration_time = x509_proxy_expiration_time( proxy_handle );

	delete proxy_handle;

	return expiration_time;
}

static int
buffer_to_bio( char *buffer, size_t buffer_len, BIO **bio )
{
	if ( buffer == NULL ) {
		return FALSE;
	}

	*bio = BIO_new( BIO_s_mem() );
	if ( *bio == NULL ) {
		return FALSE;
	}

	if ( BIO_write( *bio, buffer, buffer_len ) < (int)buffer_len ) {
		BIO_free( *bio );
		return FALSE;
	}

	return TRUE;
}

static int
bio_to_buffer( BIO *bio, char **buffer, size_t *buffer_len )
{
	if ( bio == NULL ) {
		return FALSE;
	}

	*buffer_len = BIO_pending( bio );

	*buffer = (char *)malloc( *buffer_len );
	if ( *buffer == NULL ) {
		return FALSE;
	}

	if ( BIO_read( bio, *buffer, *buffer_len ) < (int)*buffer_len ) {
		free( *buffer );
		return FALSE;
	}

	return TRUE;
}

int
x509_send_delegation( const char *source_file,
					  time_t expiration_time,
					  time_t *result_expiration_time,
					  int (*recv_data_func)(void *, void **, size_t *), 
					  void *recv_data_ptr,
					  int (*send_data_func)(void *, void *, size_t),
					  void *send_data_ptr )
{
	int rc = -1;
	char *buffer = nullptr;
	size_t buffer_len = 0;
	BIO *in_bio = nullptr;
	BIO *out_bio = nullptr;
	X509 *src_cert = nullptr;
	STACK_OF(X509) *src_chain = nullptr;
	bool did_send = false;
	DelegationRestrictions restrict;
	X509Credential deleg_provider(source_file, "");

	if ( recv_data_func( recv_data_ptr, (void **)&buffer, &buffer_len ) != 0 || buffer == NULL ) {
		_globus_error_message = "Failed to receive delegation request";
		goto cleanup;
	}

	if ( buffer_to_bio( buffer, buffer_len, &in_bio ) == FALSE ) {
		_globus_error_message = "buffer_to_bio() failed";
		goto cleanup;
	}

	free(buffer);
	buffer = nullptr;

	if ( ! param_boolean("DELEGATE_FULL_JOB_GSI_CREDENTIALS", false) ) {
		restrict["policyLimited"] = "true";
	}

	src_cert = deleg_provider.GetCert();
	src_chain = deleg_provider.GetChain();
	if ( ! src_cert ) {
		_globus_error_message = "Failed to read proxy file";
		goto cleanup;
	}

	if ( expiration_time || result_expiration_time ) {
		time_t orig_expiration_time = x509_proxy_expiration_time(src_cert, src_chain);

		if( expiration_time && orig_expiration_time > expiration_time ) {
			restrict["validityEnd"] = std::to_string(expiration_time);
		}

		if( result_expiration_time ) {
			*result_expiration_time = expiration_time;
		}
	}

	out_bio = deleg_provider.Delegate(in_bio, restrict);
	if ( ! out_bio ) {
		_globus_error_message = "X509Credential::Delegate() failed";
		goto cleanup;
	}

	if ( bio_to_buffer( out_bio, &buffer, &buffer_len ) == FALSE ) {
		_globus_error_message = "bio_to_buffer() failed";
		goto cleanup;
	}

	did_send = true;
	if ( send_data_func( send_data_ptr, buffer, buffer_len ) != 0 ) {
		_globus_error_message = "Failed to send delegated proxy";
		goto cleanup;
	}

	rc = 0;

 cleanup:
	if ( ! did_send ) {
		send_data_func( send_data_ptr, NULL, 0 );
	}
	if ( buffer ) {
		free( buffer );
	}
	if ( in_bio ) {
		BIO_free( in_bio );
	}
	if ( out_bio ) {
		BIO_free( out_bio );
	}

	return rc;
}


struct x509_delegation_state
{
	std::string m_dest;
	X509Credential m_deleg_consumer;
};


int
x509_receive_delegation( const char *destination_file,
						 int (*recv_data_func)(void *, void **, size_t *), 
						 void *recv_data_ptr,
						 int (*send_data_func)(void *, void *, size_t),
						 void *send_data_ptr,
						 void ** state_ptr )
{
	int rc = -1;
	x509_delegation_state *st = new x509_delegation_state();
	st->m_dest = destination_file;
	char *buffer = NULL;
	size_t buffer_len = 0;
	BIO *bio = NULL;
	bool did_send = false;

		// make cert request
	bio = BIO_new( BIO_s_mem() );
	if ( bio == nullptr ) {
		_globus_error_message = "BIO_new() failed";
		goto cleanup;
	}

	if ( ! st->m_deleg_consumer.Request(bio) ) {
		_globus_error_message = "X509Credential::Request() failed";
		goto cleanup;
	}

	if ( bio_to_buffer( bio, &buffer, &buffer_len ) == FALSE ) {
		_globus_error_message = "bio_to_buffer() failed";
		goto cleanup;
	}

	did_send = true;
	if ( send_data_func( send_data_ptr, buffer, buffer_len ) != 0 ) {
		_globus_error_message = "Failed to send delegation request";
		goto cleanup;
	}

	rc = 0;

 cleanup:
	if ( !did_send ) {
		send_data_func( send_data_ptr, NULL, 0 );
	}
	if ( bio ) {
		BIO_free( bio );
	}
	if ( buffer ) {
		free( buffer );
	}

	// Error!  Cleanup memory immediately and return.
	if ( rc ) {
		delete st;
		return rc;
	}

	// We were given a state pointer - caller will take care of monitoring the
	// socket for more data and call delegation_finish later.
	if (state_ptr != NULL) {
		*state_ptr = st;
		return 2;
	}

	// Else, we block and finish up immediately.
	return x509_receive_delegation_finish(recv_data_func, recv_data_ptr, st);
}


// Finish up the delegation operation, waiting for data on the socket if necessary.
// NOTE: get_x509_delegation_finish will take ownership of state_ptr and free its
// memory.
int x509_receive_delegation_finish(int (*recv_data_func)(void *, void **, size_t *),
                               void *recv_data_ptr,
                               void *state_ptr_raw)
{
	int rc = -1;
	x509_delegation_state *state_ptr = static_cast<x509_delegation_state*>(state_ptr_raw);
	char *buffer = nullptr;
	size_t buffer_len = 0;
	BIO *bio = nullptr;
	std::string proxy_contents;
	std::string proxy_subject;
	int fd = -1;

	if ( recv_data_func( recv_data_ptr, (void **)&buffer, &buffer_len ) != 0 || buffer == NULL ) {
		_globus_error_message = "Failed to receive delegated proxy";
		goto cleanup;
	}

	if ( buffer_to_bio( buffer, buffer_len, &bio ) == FALSE ) {
		_globus_error_message = "buffer_to_bio() failed";
		goto cleanup;
	}

	if ( ! state_ptr->m_deleg_consumer.Acquire(bio, proxy_contents, proxy_subject) ) {
		_globus_error_message = "X509Credential::Acquire() failed";
		goto cleanup;
	}

		// write proxy file
		// safe_open O_WRONLY|O_EXCL|O_CREAT, S_IRUSR|S_IWUSR
	fd = safe_open_wrapper_follow(state_ptr->m_dest.c_str(), O_WRONLY|O_EXCL|O_CREAT, 0600);
	if ( fd < 0 ) {
		_globus_error_message = "Failed to open proxy file";
		goto cleanup;
	}

	if ( write(fd, proxy_contents.c_str(), proxy_contents.size()) < (ssize_t)proxy_contents.size() ) {
		_globus_error_message = "Failed to write proxy file";
		goto cleanup;
	}

	rc = 0;

 cleanup:
	if ( bio ) {
		BIO_free( bio );
	}
	if ( buffer ) {
		free( buffer );
	}
	if ( state_ptr ) {
		delete state_ptr;
	}
	if ( fd >= 0 ) {
		close(fd);
	}

	return rc;
}
