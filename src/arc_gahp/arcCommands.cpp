/***************************************************************
 *
 * Copyright (C) 1990-2021, Condor Team, Computer Sciences Department,
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
#include "condor_debug.h"
#include "condor_config.h"
#include "basename.h"
#include "arcgahp_common.h"
#include "arcCommands.h"
#include "shortfile.h"

#include "stat_wrapper.h"
#include <curl/curl.h>
#include "thread_control.h"
#include "condor_regex.h"

#include "DelegationInterface.h"

using std::string;
using std::map;
using std::vector;

#define NULLSTRING "NULL"

curl_version_info_data *HttpRequest::curlVerInfo = NULL;
bool HttpRequest::curlUsesNss = false;

const char * nullStringIfEmpty( const string & str ) {
	if( str.empty() ) { return NULLSTRING; }
	else { return str.c_str(); }
}

// Fill in missing components of the service URL with defaults, like so:
//   https://hostname:443/arex/rest/1.0
// The hostname is the only required component.
std::string fillURL(const char *url)
{
	Regex r; int errCode = 0, errOffset = 0;
	bool patternOK = r.compile( "([^:]+://)?([^:/]+)(:[0-9]*)?(.*)", &errCode, &errOffset);
	ASSERT( patternOK );
	std::vector<std::string> groups;
	if(! r.match(url, &groups )) {
		return url;
	}
	if( groups[1].empty() ) {
		groups[1] = "https://";
	}
	if( groups[3].empty() ) {
		groups[3] = ":443";
	}
	if( groups[4].empty() ) {
		groups[4] = "/arex/rest/1.0";
	}

	return groups[1] + groups[2] + groups[3] + groups[4];
}

// Utility function for parsing the JSON response returned by the server.
bool ParseJSONLine( const char *&input, string &key, string &value, int &nesting )
{
	const char *ptr = NULL;
	bool in_key = false;
	bool in_value_str = false;
	bool in_value_int = false;
	key.clear();
	value.clear();

	for ( ptr = input; *ptr; ptr++ ) {
		if ( in_key ) {
			if ( *ptr == '"' ) {
				in_key = false;
			} else {
				key += *ptr;
			}
		} else if ( in_value_str ) {
			if ( *ptr == '"' ) {
				in_value_str = false;
			} else {
				value += *ptr;
			}
		} else if ( in_value_int ) {
			if ( isdigit( *ptr ) ) {
				value += *ptr;
			} else {
				in_value_int = false;
			}
		} else if ( *ptr == '"' ) {
			if ( key.empty() ) {
				in_key = true;
			} else if ( value.empty() ) {
				in_value_str = true;
			}
		} else if ( isdigit( *ptr ) ) {
			if ( value.empty() ) {
				value += *ptr;
				in_value_int = true;
			}
		} else if ( *ptr == '{' || *ptr == '[' ) {
			nesting++;
		} else if ( *ptr == '}' || *ptr == ']' ) {
			nesting--;
		} else if ( *ptr == '\n' ) {
			ptr++;
			input = ptr;
			return true;
		}
	}

	input = ptr;
	return false;
}

// From the body of a failure reply from the server, extract the best
// human-readable error message.
void ExtractErrorMessage( const string &response, int response_code, string &err_msg )
{
	string key;
	string value;
	int nesting = 0;

	const char *pos = response.c_str();
	while ( ParseJSONLine( pos, key, value, nesting ) ) {
		if ( nesting == 2 && key == "message" ) {
			err_msg = value;
			return;
		}
	}
	switch (response_code) {
	case 400:
		err_msg = "Bad Request";
		break;
	case 401:
		err_msg = "Unauthorized";
		break;
	case 403:
		err_msg = "Forbidden";
		break;
	case 404:
		err_msg = "Not Found";
		break;
	default:
		formatstr(err_msg, "HTTP response %d", response_code);
		break;
	}
}

//
// "This function gets called by libcurl as soon as there is data received
//  that needs to be saved. The size of the data pointed to by ptr is size
//  multiplied with nmemb, it will not be zero terminated. Return the number
//  of bytes actually taken care of. If that amount differs from the amount
//  passed to your function, it'll signal an error to the library. This will
//  abort the transfer and return CURLE_WRITE_ERROR."
//
// We also make extensive use of this function in the XML parsing code,
// for pretty much exactly the same reason.
//
size_t appendToString( const void * ptr, size_t size, size_t nmemb, void * str ) {
	if( size == 0 || nmemb == 0 ) { return 0; }

	string source( (const char *)ptr, size * nmemb );
	string * ssptr = (string *)str;
	ssptr->append( source );

	return (size * nmemb);
}

bool GetSingleJobAd(const ClassAd& resp_ad, ClassAd*& job_ad, std::string& err_msg)
{
	classad::ExprTree *expr = resp_ad.Lookup("job");
	if ( expr == NULL) {
		err_msg = "Invalid response (no job element)";
		return false;
	}

	// Old ARC CE servers returned a single record instead of an array
	// of records when there was data for only one job.
	if (expr->GetKind() == classad::ExprTree::CLASSAD_NODE) {
		job_ad = (classad::ClassAd*)expr;
	} else if (expr->GetKind() == classad::ExprTree::EXPR_LIST_NODE) {
		std::vector<classad::ExprTree*> expr_list;
		((classad::ExprList*)expr)->GetComponents(expr_list);
		if (expr_list.size() == 0) {
			err_msg = "Invalid response (empty job array)";
			return false;
		}
		job_ad = (classad::ClassAd*)expr_list[0];
	} else {
		err_msg = "Invalid response (bad job element type)";
		return false;
	}
	return true;
}

HttpRequest::HttpRequest()
{
	if ( curlVerInfo == NULL ) {
		curlVerInfo = curl_version_info(CURLVERSION_NOW);
		if ( curlVerInfo && curlVerInfo->ssl_version && strstr(curlVerInfo->ssl_version, "NSS") ) {
			curlUsesNss = true;
		}
	}

	includeResponseHeader = false;
	contentType = "application/json";
	requestBodyReadPos = 0;
}

HttpRequest::~HttpRequest() { }

// This function is called by libcurl while none of our locks are held.
// Don't call any Condor functions or access any variables outside of
// the HttpRequest in userdata.
size_t HttpRequest::CurlReadCb(char *buffer, size_t size, size_t nitems, void *userdata)
{
	HttpRequest *request = (HttpRequest*)userdata;
	size_t copy_sz = size * nitems;
	if ( copy_sz + request->requestBodyReadPos > request->requestBody.length() ) {
		copy_sz = request->requestBody.length() - request->requestBodyReadPos;
	}
	if ( copy_sz > 0 ) {
		memcpy(buffer, &request->requestBody[request->requestBodyReadPos], copy_sz);
		request->requestBodyReadPos += copy_sz;
	}
	return copy_sz;
}

pthread_mutex_t globalCurlMutex = PTHREAD_MUTEX_INITIALIZER;
bool HttpRequest::SendRequest() 
{
	struct  curl_slist *curl_headers = NULL;
	string buf;
	unsigned long response_code = 0;
	const char *ca_dir = NULL;
	FILE *in_fp = NULL;
	FILE *out_fp = NULL;

	this->responseBody.clear();
	this->errorMessage.clear();

	// Generate the final URI.
	// TODO Eliminate this copy if we always use the serviceURL unmodified
	string finalURI = this->serviceURL;
	dprintf( D_FULLDEBUG, "Request URI is '%s'\n", finalURI.c_str() );
	if ( requestMethod == "POST" ) {
		dprintf( D_FULLDEBUG, "Request body is '%s'\n", requestBody.c_str() );
	}

	CURLcode rv;

	CURL * curl = curl_easy_init();
	if( curl == NULL ) {
		this->errorCode = "499";
		this->errorMessage = "curl_easy_init() failed.";
		dprintf( D_ALWAYS, "curl_easy_init() failed, failing.\n" );
		goto error_return;
	}

	char errorBuffer[CURL_ERROR_SIZE];
	rv = curl_easy_setopt( curl, CURLOPT_ERRORBUFFER, errorBuffer );
	if( rv != CURLE_OK ) {
		this->errorCode = "499";
		this->errorMessage = "curl_easy_setopt( CURLOPT_ERRORBUFFER ) failed.";
		dprintf( D_ALWAYS, "curl_easy_setopt( CURLOPT_ERRORBUFFER ) failed (%d): '%s', failing.\n",
				 rv, curl_easy_strerror( rv ) );
		goto error_return;
	}

/*  // Useful for debuggery.  Could be rewritten with CURLOPT_DEBUGFUNCTION
	// and dumped via dprintf() to allow control via EC2_GAHP_DEBUG.
	rv = curl_easy_setopt( curl, CURLOPT_VERBOSE, 1 );
	if( rv != CURLE_OK ) {
		this->errorCode = "499";
		this->errorMessage = "curl_easy_setopt( CURLOPT_VERBOSE ) failed.";
		dprintf( D_ALWAYS, "curl_easy_setopt( CURLOPT_VERBOSE ) failed (%d): '%s', failing.\n",
			rv, curl_easy_strerror( rv ) );
		goto error_return;
	}
*/

	rv = curl_easy_setopt( curl, CURLOPT_URL, finalURI.c_str() );
	if( rv != CURLE_OK ) {
		this->errorCode = "499";
		this->errorMessage = "curl_easy_setopt( CURLOPT_URL ) failed.";
		dprintf( D_ALWAYS, "curl_easy_setopt( CURLOPT_URL ) failed (%d): '%s', failing.\n",
				 rv, curl_easy_strerror( rv ) );
		goto error_return;
	}

	rv = curl_easy_setopt( curl, CURLOPT_NOPROGRESS, 1 );
	if( rv != CURLE_OK ) {
		this->errorCode = "499";
		this->errorMessage = "curl_easy_setopt( CURLOPT_NOPROGRESS ) failed.";
		dprintf( D_ALWAYS, "curl_easy_setopt( CURLOPT_NOPROGRESS ) failed (%d): '%s', failing.\n",
				 rv, curl_easy_strerror( rv ) );
		goto error_return;
	}

	if ( requestMethod == "POST" ) {

		rv = curl_easy_setopt( curl, CURLOPT_POST, 1 );
		if( rv != CURLE_OK ) {
			this->errorCode = "499";
			this->errorMessage = "curl_easy_setopt( CURLOPT_POST ) failed.";
			dprintf( D_ALWAYS, "curl_easy_setopt( CURLOPT_POST ) failed (%d): '%s', failing.\n",
					 rv, curl_easy_strerror( rv ) );
			goto error_return;
		}
		
		rv = curl_easy_setopt( curl, CURLOPT_POSTFIELDS, requestBody.c_str() );
		if( rv != CURLE_OK ) {
			this->errorCode = "499";
			this->errorMessage = "curl_easy_setopt( CURLOPT_POSTFIELDS ) failed.";
			dprintf( D_ALWAYS, "curl_easy_setopt( CURLOPT_POSTFIELDS ) failed (%d): '%s', failing.\n",
					 rv, curl_easy_strerror( rv ) );
			goto error_return;
		}

	} else if ( requestMethod == "PUT" ) {

#if (LIBCURL_VERSION_MAJOR > 7) || (LIBCURL_VERSION_MAJOR == 7 && LIBCURL_VERSION_MINOR > 12)
		rv = curl_easy_setopt( curl, CURLOPT_UPLOAD, 1 );
#else
		rv = curl_easy_setopt( curl, CURLOPT_PUT, 1 );
#endif
		if( rv != CURLE_OK ) {
			this->errorCode = "499";
			this->errorMessage = "curl_easy_setopt( CURLOPT_PUT ) failed.";
			dprintf( D_ALWAYS, "curl_easy_setopt( CURLOPT_PUT ) failed (%d): '%s', failing.\n",
					 rv, curl_easy_strerror( rv ) );
			goto error_return;
		}

		curl_off_t filesize = 0;

		if( ! requestBodyFilename.empty() ) {
			in_fp = fopen(this->requestBodyFilename.c_str(), "r");
			if( in_fp == NULL ) {
				this->errorCode = "499";
				this->errorMessage = "Failed to open file";
				dprintf( D_ALWAYS, "fopen(%s) failed (%d): '%s', failing.\n",
			             this->requestBodyFilename.c_str(), errno, strerror( errno ) );
				goto error_return;
			}

			rv = curl_easy_setopt( curl, CURLOPT_READDATA, in_fp );
			if( rv != CURLE_OK ) {
				this->errorCode = "499";
				this->errorMessage = "curl_easy_setopt( CURLOPT_READDATA ) failed.";
				dprintf( D_ALWAYS, "curl_easy_setopt( CURLOPT_READDATA ) failed (%d): '%s', failing.\n",
				         rv, curl_easy_strerror( rv ) );
				goto error_return;
			}

			StatWrapper stw(this->requestBodyFilename.c_str());
			filesize = stw.GetBuf()->st_size;
		} else {
			rv = curl_easy_setopt( curl, CURLOPT_READFUNCTION, & CurlReadCb );
			if( rv != CURLE_OK ) {
				this->errorCode = "499";
				this->errorMessage = "curl_easy_setopt( CURLOPT_READFUNCTION ) failed.";
				dprintf( D_ALWAYS, "curl_easy_setopt( CURLOPT_READFUNCTION ) failed (%d): '%s', failing.\n",
				         rv, curl_easy_strerror( rv ) );
				goto error_return;
			}

			rv = curl_easy_setopt( curl, CURLOPT_READDATA, this );
			if( rv != CURLE_OK ) {
				this->errorCode = "499";
				this->errorMessage = "curl_easy_setopt( CURLOPT_READDATA ) failed.";
				dprintf( D_ALWAYS, "curl_easy_setopt( CURLOPT_READDATA ) failed (%d): '%s', failing.\n",
				         rv, curl_easy_strerror( rv ) );
				goto error_return;
			}

			filesize = requestBody.length();
		}

		rv = curl_easy_setopt( curl, CURLOPT_INFILESIZE_LARGE, filesize );
		if( rv != CURLE_OK ) {
			this->errorCode = "499";
			this->errorMessage = "curl_easy_setopt( CURLOPT_INFILESIZE_LARGE ) failed.";
			dprintf( D_ALWAYS, "curl_easy_setopt( CURLOPT_INFILESIZE_LARGE ) failed (%d): '%s', failing.\n",
					 rv, curl_easy_strerror( rv ) );
			goto error_return;
		}
		
	} else if ( requestMethod != "GET" ) {

		rv = curl_easy_setopt( curl, CURLOPT_CUSTOMREQUEST, requestMethod.c_str() );
		if( rv != CURLE_OK ) {
			this->errorCode = "499";
			this->errorMessage = "curl_easy_setopt( CURLOPT_CUSTOMREQUEST ) failed.";
			dprintf( D_ALWAYS, "curl_easy_setopt( CURLOPT_CUSTOMREQUEST ) failed (%d): '%s', failing.\n",
					 rv, curl_easy_strerror( rv ) );
			goto error_return;
		}
	}

	buf = "Content-Type: ";
	buf += contentType;
	curl_headers = curl_slist_append( curl_headers, buf.c_str() );
	if ( curl_headers == NULL ) {
		this->errorCode = "499";
		this->errorMessage = "curl_slist_append() failed.";
		dprintf( D_ALWAYS, "curl_slist_append() failed, failing.\n" );
		goto error_return;
	}

	curl_headers = curl_slist_append( curl_headers, "Accept: application/json" );
	if ( curl_headers == NULL ) {
		this->errorCode = "499";
		this->errorMessage = "curl_slist_append() failed.";
		dprintf( D_ALWAYS, "curl_slist_append() failed, failing.\n" );
		goto error_return;
	}

	if (!tokenFile.empty()) {
		std::string token;
		if (!htcondor::readShortFile(tokenFile, token)) {
			this->errorCode = "499";
			this->errorMessage = "Failed to read token file";
			dprintf(D_ALWAYS, "Failed to read token file %s, failing.\n", tokenFile.c_str());
			goto error_return;
		}
		trim(token);
		buf = "Authorization: Bearer " + token;
		curl_headers = curl_slist_append( curl_headers, buf.c_str() );
		if ( curl_headers == NULL ) {
			this->errorCode = "499";
			this->errorMessage = "curl_slist_append() failed.";
			dprintf( D_ALWAYS, "curl_slist_append() failed, failing.\n" );
			goto error_return;
		}
	}

	rv = curl_easy_setopt( curl, CURLOPT_HTTPHEADER, curl_headers );
	if( rv != CURLE_OK ) {
		this->errorCode = "499";
		this->errorMessage = "curl_easy_setopt( CURLOPT_HTTPHEADER ) failed.";
		dprintf( D_ALWAYS, "curl_easy_setopt( CURLOPT_HTTPHEADER ) failed (%d): '%s', failing.\n",
				 rv, curl_easy_strerror( rv ) );
		goto error_return;
	}

	if ( includeResponseHeader ) {
		rv = curl_easy_setopt( curl, CURLOPT_HEADER, 1 );
		if( rv != CURLE_OK ) {
			this->errorCode = "499";
			this->errorMessage = "curl_easy_setopt( CURLOPT_HEADER ) failed.";
			dprintf( D_ALWAYS, "curl_easy_setopt( CURLOPT_HEADER ) failed (%d): '%s', failing.\n",
					 rv, curl_easy_strerror( rv ) );
			goto error_return;
		}
	}

	if ( ! this->responseBodyFilename.empty() ) {
		out_fp = fopen(this->responseBodyFilename.c_str(), "w");
		if( out_fp == NULL ) {
			this->errorCode = "499";
			this->errorMessage = "Failed to open file";
			dprintf( D_ALWAYS, "fopen(%s) failed (%d): '%s', failing.\n",
			         this->responseBodyFilename.c_str(), errno, strerror( errno ) );
			goto error_return;
		}

		rv = curl_easy_setopt( curl, CURLOPT_WRITEDATA, out_fp );
		if( rv != CURLE_OK ) {
			this->errorCode = "499";
			this->errorMessage = "curl_easy_setopt( CURLOPT_WRITEDATA ) failed.";
			dprintf( D_ALWAYS, "curl_easy_setopt( CURLOPT_WRITEDATA ) failed (%d): '%s', failing.\n",
					 rv, curl_easy_strerror( rv ) );
			goto error_return;
		}
	} else {
		rv = curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, & appendToString );
		if( rv != CURLE_OK ) {
			this->errorCode = "499";
			this->errorMessage = "curl_easy_setopt( CURLOPT_WRITEFUNCTION ) failed.";
			dprintf( D_ALWAYS, "curl_easy_setopt( CURLOPT_WRITEFUNCTION ) failed (%d): '%s', failing.\n",
			         rv, curl_easy_strerror( rv ) );
			goto error_return;
		}

		rv = curl_easy_setopt( curl, CURLOPT_WRITEDATA, & this->responseBody );
		if( rv != CURLE_OK ) {
			this->errorCode = "499";
			this->errorMessage = "curl_easy_setopt( CURLOPT_WRITEDATA ) failed.";
			dprintf( D_ALWAYS, "curl_easy_setopt( CURLOPT_WRITEDATA ) failed (%d): '%s', failing.\n",
			         rv, curl_easy_strerror( rv ) );
			goto error_return;
		}
	}

	//
	// Set security options.
	//
	rv = curl_easy_setopt( curl, CURLOPT_SSL_VERIFYPEER, 1 );
	if( rv != CURLE_OK ) {
		this->errorCode = "499";
		this->errorMessage = "curl_easy_setopt( CURLOPT_SSL_VERIFYPEER ) failed.";
		dprintf( D_ALWAYS, "curl_easy_setopt( CURLOPT_SSL_VERIFYPEER ) failed (%d): '%s', failing.\n",
				 rv, curl_easy_strerror( rv ) );
		goto error_return;
	}

	rv = curl_easy_setopt( curl, CURLOPT_SSL_VERIFYHOST, 2 );
	if( rv != CURLE_OK ) {
		this->errorCode = "499";
		this->errorMessage = "curl_easy_setopt( CURLOPT_SSL_VERIFYHOST ) failed.";
		dprintf( D_ALWAYS, "curl_easy_setopt( CURLOPT_SSL_VERIFYHOST ) failed (%d): '%s', failing.\n",
				 rv, curl_easy_strerror( rv ) );
		goto error_return;
	}

	// NB: Contrary to libcurl's manual, it doesn't strdup() strings passed
	// to it, so they MUST remain in scope until after we call
	// curl_easy_cleanup().  Otherwise, curl_perform() will fail with
	// a completely bogus error, number 60, claiming that there's a
	// 'problem with the SSL CA cert'.

	ca_dir = getenv( "X509_CERT_DIR" );
	if ( ca_dir == NULL ) {
		ca_dir = "/etc/grid-security/certificates";
	}

	dprintf( D_FULLDEBUG, "Setting CA path to '%s'\n", ca_dir );

	rv = curl_easy_setopt( curl, CURLOPT_CAPATH, ca_dir );
	if( rv != CURLE_OK ) {
		this->errorCode = "499";
		this->errorMessage = "curl_easy_setopt( CURLOPT_CAPATH ) failed.";
		dprintf( D_ALWAYS, "curl_easy_setopt( CURLOPT_CAPATH ) failed (%d): '%s', failing.\n",
				 rv, curl_easy_strerror( rv ) );
		goto error_return;
	}

	if( setenv( "OPENSSL_ALLOW_PROXY", "1", 0 ) != 0 ) {
		dprintf( D_FULLDEBUG, "Failed to set OPENSSL_ALLOW_PROXY.\n" );
	}

	if ( !proxyFile.empty() ) {
		rv = curl_easy_setopt( curl, CURLOPT_SSLCERT, proxyFile.c_str() );
		if( rv != CURLE_OK ) {
			this->errorCode = "499";
			this->errorMessage = "curl_easy_setopt(CURLOPT_SSLCERT) failed";
			dprintf(D_ALWAYS, "curl_easy_setopt(CURLOPT_SSLCERT) failed (%d): '%s', failing\n",
			        rv, curl_easy_strerror(rv));
			goto error_return;
		}

		rv = curl_easy_setopt( curl, CURLOPT_SSLCERTTYPE, "PEM" );
		if( rv != CURLE_OK ) {
			this->errorCode = "499";
			this->errorMessage = "curl_easy_setopt(CURLOPT_SSLCERTTYPE) failed";
			dprintf(D_ALWAYS, "curl_easy_setopt(CURLOPT_SSLCERTTYPE) failed (%d): '%s', failing\n",
			        rv, curl_easy_strerror(rv));
			goto error_return;
		}

		// When authenticating with an X.509 proxy certificate, NSS
		// only sends the final proxy certificate from the proxy file
		// to the server. It doesn't send the user's certificate or
		// any intermediary proxy certificates from the proxy file.
		// But if we tell curl that the proxy file contains CA
		// certificates, then NSS will send all of the necessary
		// certificates.
		if( curlUsesNss ) {
			dprintf( D_FULLDEBUG, "Setting CA file to '%s'\n", proxyFile.c_str() );

			rv = curl_easy_setopt( curl, CURLOPT_CAINFO, proxyFile.c_str() );
			if( rv != CURLE_OK ) {
				this->errorCode = "499";
				this->errorMessage = "curl_easy_setopt( CURLOPT_CAINFO ) failed.";
				dprintf( D_ALWAYS, "curl_easy_setopt( CURLOPT_CAINFO ) failed (%d): '%s', failing.\n",
				         rv, curl_easy_strerror( rv ) );
				goto error_return;
			}
		}

	}

	arc_gahp_release_big_mutex();
	if (lock_for_curl) {
		pthread_mutex_lock( & globalCurlMutex );
	}
	rv = curl_easy_perform( curl );
	if (lock_for_curl) {
		pthread_mutex_unlock( & globalCurlMutex );
	}
	arc_gahp_grab_big_mutex();
	if( rv != 0 ) {
		this->errorCode = "499";
		formatstr( this->errorMessage, "curl_easy_perform() failed (%d): '%s'.", rv, curl_easy_strerror(rv) );
		dprintf( D_ALWAYS, "%s\n", this->errorMessage.c_str() );
		dprintf( D_FULLDEBUG, "%s\n", errorBuffer );
		goto error_return;
	}

	rv = curl_easy_getinfo( curl, CURLINFO_RESPONSE_CODE, & response_code );
	if( rv != CURLE_OK ) {
		this->errorCode = "499";
		this->errorMessage = "curl_easy_getinfo() failed.";
		dprintf( D_ALWAYS, "curl_easy_getinfo( CURLINFO_RESPONSE_CODE ) failed (%d): '%s', failing.\n",
				 rv, curl_easy_strerror( rv ) );
		goto error_return;
	}

	if( includeResponseHeader ) {
		size_t end_pos = responseBody.find("\r\n\r\n");
		if( end_pos == std::string::npos ) {
			end_pos = 0;
		}
		size_t start_pos = responseBody.find("\r\n") + 2;
		while( start_pos < end_pos ) {
			size_t sep_pos = responseBody.find(": ", start_pos);
			size_t endl_pos = responseBody.find("\r\n", start_pos);
			responseHeaders[responseBody.substr(start_pos, sep_pos-start_pos)] = responseBody.substr(sep_pos+2, endl_pos-(sep_pos+2));
			start_pos = endl_pos + 2;
		}
		responseBody.erase(0, end_pos + 4);
	}

	curl_easy_cleanup( curl );
	curl = NULL;

	curl_slist_free_all( curl_headers );
	curl_headers = NULL;

	if ( in_fp ) {
		fclose(in_fp);
		in_fp = NULL;
	}
	if ( out_fp ) {
		fclose(out_fp);
		out_fp = NULL;
	}

	if( response_code < 200 || response_code > 299 ) {
		this->errorCode = std::to_string(response_code);
		ExtractErrorMessage( responseBody, response_code, this->errorMessage );
		if( this->errorMessage.empty() ) {
			formatstr( this->errorMessage, "HTTP response was %lu, not 2XX, and no error message was returned.", response_code );
		}
		dprintf( D_ALWAYS, "Query did not return 2XX (%lu), failing.\n",
				 response_code );
		dprintf( D_ALWAYS, "Failure response text was '%s'.\n", responseBody.c_str() );
		goto error_return;
	}

	this->errorCode = std::to_string(response_code);
	this->errorMessage = "OK";

	dprintf( D_FULLDEBUG, "Response was '%s'\n", responseBody.c_str() );
	return true;

 error_return:
	if ( curl ) {
		curl_easy_cleanup( curl );
	}
	if ( curl_headers ) {
		curl_slist_free_all( curl_headers );
	}
	if ( in_fp ) {
		fclose(in_fp);
	}
	if ( out_fp ) {
		fclose(out_fp);
	}

	return false;
}


// ---------------------------------------------------------------------------


// Expecting:ARC_PING <req_id> <serviceurl>
bool ArcPingArgsCheck(char **argv, int argc)
{
	return verify_number_args(argc, 3) &&
		verify_request_id(argv[1]) &&
		verify_string_name(argv[2]);
}

// Expecting:ARC_PING <req_id> <serviceurl>
bool ArcPingWorkerFunction(GahpRequest *gahp_request)
{
	int argc = gahp_request->m_args.argc;
	char **argv = gahp_request->m_args.argv;
	int requestID = gahp_request->m_reqid;

	if( ! verify_number_args( argc, 3 ) ) {
		gahp_request->m_result = create_result_string(requestID, "499", "Wrong_Argument_Number");
		dprintf( D_ALWAYS, "Wrong number of arguments (%d should be >= %d) to %s\n",
				 argc, 3, argv[0] );
		return false;
	}

	// Fill in required attributes & parameters.
	HttpRequest ping_request;
	ping_request.serviceURL = fillURL(argv[2]);
	ping_request.serviceURL += "/jobs";
	ping_request.requestMethod = "GET";
	ping_request.proxyFile = gahp_request->m_proxy_file;
	ping_request.tokenFile = gahp_request->m_token_file;

	// Send the request.
	if( ! ping_request.SendRequest() ) {
		// TODO Fix construction of error message
		gahp_request->m_result = create_result_string( requestID,
								ping_request.errorCode,
								ping_request.errorMessage );
	} else {
		gahp_request->m_result = create_result_string( requestID,
								ping_request.errorCode,
								ping_request.errorMessage );
	}

	return true;
}

// Expecting:ARC_JOB_NEW <req_id> <serviceurl> <rsl>
bool ArcJobNewArgsCheck(char **argv, int argc)
{
	return verify_number_args(argc, 4) &&
		verify_request_id(argv[1]) &&
		verify_string_name(argv[2]) &&
		verify_string_name(argv[3]);
}

// Expecting:ARC_JOB_NEW <req_id> <serviceurl> <rsl>
bool ArcJobNewWorkerFunction(GahpRequest *gahp_request)
{
	int argc = gahp_request->m_args.argc;
	char **argv = gahp_request->m_args.argv;
	int request_id = gahp_request->m_reqid;

	if( ! verify_number_args( argc, 4 ) ) {
		gahp_request->m_result = create_result_string(request_id, "499", "Wrong_Argument_Number");
		dprintf( D_ALWAYS, "Wrong number of arguments (%d should be >= %d) to %s\n",
				 argc, 4, argv[0] );
		return false;
	}

	// Fill in required attributes & parameters.
	HttpRequest submit_request;
	submit_request.serviceURL = fillURL(argv[2]);
	submit_request.serviceURL += "/jobs?action=new";
 	submit_request.requestMethod = "POST";
	submit_request.proxyFile = gahp_request->m_proxy_file;
	submit_request.tokenFile = gahp_request->m_token_file;
	submit_request.requestBody = argv[3];
	if ( argv[3][0] == '<' ) {
		submit_request.contentType = "application/xml";
	} else {
		submit_request.contentType = "applicaton/rsl";
	}

	// Send the request.
	if( ! submit_request.SendRequest() ) {
		// TODO Fix construction of error message
		gahp_request->m_result = create_result_string( request_id,
								submit_request.errorCode,
								submit_request.errorMessage );
		return true;
	}

	classad::ClassAd resp_ad;
	classad::ClassAdJsonParser parser;
	if ( ! parser.ParseClassAd(submit_request.responseBody, resp_ad, true) ) {
		gahp_request->m_result = create_result_string( request_id,
								"499", "Invalid response (parsing json)" );
		return true;
	}

	classad::ClassAd *job_ad = nullptr;
	std::string err_msg;
	if ( !GetSingleJobAd(resp_ad, job_ad, err_msg) ) {
		gahp_request->m_result = create_result_string( request_id,
								"499", err_msg );
		return true;
	}

	std::string val;

	if ( ! job_ad->EvaluateAttrString("status-code", val) ) {
		gahp_request->m_result = create_result_string( request_id,
								"499", "Invalid response (no status-code element)" );
		return true;
	}
	submit_request.errorCode = val;

	val.clear();
	if ( ! job_ad->EvaluateAttrString("reason", val) ) {
		gahp_request->m_result = create_result_string( request_id,
								"499", "Invalid response (no reason element)" );
		return true;
	}
	submit_request.errorMessage = val;

	if ( submit_request.errorCode[0] != '2' ) {
		gahp_request->m_result = create_result_string( request_id,
									submit_request.errorCode,
									submit_request.errorMessage );
		return true;
	}

	std::vector<std::string> result_args;

	val.clear();
	if ( ! job_ad->EvaluateAttrString("id", val) ) {
		gahp_request->m_result = create_result_string( request_id,
								"499", "Invalid response (no id element)" );
		return true;
	}
	result_args.push_back(val);

	val.clear();
	if ( ! job_ad->EvaluateAttrString("state", val) ) {
		gahp_request->m_result = create_result_string( request_id,
								"499", "Invalid response (no state element)" );
		return true;
	}
	result_args.push_back(val);

	gahp_request->m_result = create_result_string( request_id,
								submit_request.errorCode,
								submit_request.errorMessage, result_args );

	return true;
}

// Expecting:ARC_JOB_STATUS <req_id> <serviceurl> <job_id>
bool ArcJobStatusArgsCheck(char **argv, int argc)
{
	return verify_number_args(argc, 4) &&
		verify_request_id(argv[1]) &&
		verify_string_name(argv[2]) &&
		verify_string_name(argv[3]);
}

// Expecting:ARC_JOB_STATUS <req_id> <serviceurl> <job_id>
bool ArcJobStatusWorkerFunction(GahpRequest *gahp_request)
{
	int argc = gahp_request->m_args.argc;
	char **argv = gahp_request->m_args.argv;
	int request_id = gahp_request->m_reqid;

	if( ! verify_number_args( argc, 4 ) ) {
		gahp_request->m_result = create_result_string(request_id, "499", "Wrong_Argument_Number");
		dprintf( D_ALWAYS, "Wrong number of arguments (%d should be >= %d) to %s\n",
				 argc, 4, argv[0] );
		return false;
	}

	// Fill in required attributes & parameters.
	HttpRequest status_request;
	status_request.serviceURL = fillURL(argv[2]);
	status_request.serviceURL += "/jobs?action=status";
 	status_request.requestMethod = "POST";
	status_request.proxyFile = gahp_request->m_proxy_file;
	status_request.tokenFile = gahp_request->m_token_file;
	formatstr(status_request.requestBody, "{\"job\":[{\"id\":\"%s\"}]}", argv[3]);
//	status_request.requestBody="{\"job\":[{\"id\":\"1FeKDmC5WhynOSAtDmEBFKDmABFKDmABFKDmGPHKDmABFKDmZuDOhn\"},{\"id\":\"Mv3MDmU9WhynOSAtDmEBFKDmABFKDmABFKDmGPHKDmCBFKDmEhhugm\"}]}";

	// Send the request.
	if( ! status_request.SendRequest() ) {
		// TODO Fix construction of error message
		gahp_request->m_result = create_result_string( request_id,
								status_request.errorCode,
								status_request.errorMessage );
		return true;
	}

	classad::ClassAd resp_ad;
	classad::ClassAdJsonParser parser;
	if ( ! parser.ParseClassAd(status_request.responseBody, resp_ad, true) ) {
		gahp_request->m_result = create_result_string( request_id,
								"499", "Invalid response (parsing json)" );
		return true;
	}

	classad::ClassAd *job_ad = nullptr;
	std::string err_msg;
	if ( !GetSingleJobAd(resp_ad, job_ad, err_msg) ) {
		gahp_request->m_result = create_result_string( request_id,
								"499", err_msg );
		return true;
	}

	std::string val;

	if ( ! job_ad->EvaluateAttrString("status-code", val) ) {
		gahp_request->m_result = create_result_string( request_id,
								"499", "Invalid response (no status-code element)" );
		return true;
	}
	status_request.errorCode = val;

	val.clear();
	if ( ! job_ad->EvaluateAttrString("reason", val) ) {
		gahp_request->m_result = create_result_string( request_id,
								"499", "Invalid response (no reason element)" );
		return true;
	}
	status_request.errorMessage = val;

	std::vector<std::string> result_args;

	if ( status_request.errorCode == "200" ) {
		val.clear();
		if ( ! job_ad->EvaluateAttrString("state", val) ) {
			gahp_request->m_result = create_result_string( request_id,
								"499", "Invalid response (no state element)" );
			return true;
		}
		result_args.push_back(val);
	}

	gahp_request->m_result = create_result_string( request_id,
								status_request.errorCode,
								status_request.errorMessage, result_args );

	return true;
}

// Expecting:ARC_JOB_STATUS_ALL <req_id> <serviceurl> <statuses>
bool ArcJobStatusAllArgsCheck(char **argv, int argc)
{
	return verify_number_args(argc, 4) &&
		verify_request_id(argv[1]) &&
		verify_string_name(argv[2]);
}

// Expecting:ARC_JOB_STATUS_ALL <req_id> <serviceurl> <statuses>
bool ArcJobStatusAllWorkerFunction(GahpRequest *gahp_request)
{
	int argc = gahp_request->m_args.argc;
	char **argv = gahp_request->m_args.argv;
	int request_id = gahp_request->m_reqid;

	if( ! verify_number_args( argc, 4 ) ) {
		gahp_request->m_result = create_result_string(request_id, "499", "Wrong_Argument_Number");
		dprintf( D_ALWAYS, "Wrong number of arguments (%d should be >= %d) to %s\n",
				 argc, 4, argv[0] );
		return false;
	}

	// Fill in required attributes & parameters.
	HttpRequest query_request;
	query_request.serviceURL = fillURL(argv[2]);
	query_request.serviceURL += "/jobs";
	if( argv[3][0] != '\0' && strcasecmp( argv[3], NULLSTRING ) ) {
		query_request.serviceURL += "?state=";
		query_request.serviceURL += argv[3];
	}
 	query_request.requestMethod = "GET";
	query_request.proxyFile = gahp_request->m_proxy_file;
	query_request.tokenFile = gahp_request->m_token_file;

	// Send the request.
	if( ! query_request.SendRequest() ) {
		// TODO Fix construction of error message
		gahp_request->m_result = create_result_string( request_id,
								query_request.errorCode,
								query_request.errorMessage );
		return true;
	}

	// Fill in required attributes & parameters.
	HttpRequest status_request;
	status_request.serviceURL = fillURL(argv[2]);
	status_request.serviceURL += "/jobs?action=status";
 	status_request.requestMethod = "POST";
	status_request.proxyFile = gahp_request->m_proxy_file;
	status_request.tokenFile = gahp_request->m_token_file;
	status_request.requestBody = query_request.responseBody;

	// Send the request.
	if( ! status_request.SendRequest() ) {
		// TODO Fix construction of error message
		gahp_request->m_result = create_result_string( request_id,
								status_request.errorCode,
								status_request.errorMessage );
		return true;
	}

	std::vector<classad::ExprTree*> expr_list;

	// If there are no jobs, then the response body will be empty.
	classad::ClassAd resp_ad;
	if( ! status_request.responseBody.empty() ) {
		classad::ClassAdJsonParser parser;
		if ( ! parser.ParseClassAd(status_request.responseBody, resp_ad, true) ) {
			gahp_request->m_result = create_result_string( request_id,
								"499", "Invalid response (parsing json)" );
			return true;
		}

		classad::ExprTree *expr = resp_ad.Lookup("job");
		if ( expr == NULL ) {
			gahp_request->m_result = create_result_string( request_id,
								"499", "Invalid response (no job element)" );
			return true;
		}

		// Old ARC CE servers returned a single record instead of an array
		// of records when there was data for only one job.
		if ( expr->GetKind() == classad::ExprTree::EXPR_LIST_NODE ) {
			((classad::ExprList*)expr)->GetComponents(expr_list);
		} else if ( expr->GetKind() == classad::ExprTree::CLASSAD_NODE ) {
			expr_list.push_back(expr);
		} else {
			gahp_request->m_result = create_result_string( request_id,
						"499", "Invalid response (bad job element type)" );
			return true;
		}
	}

	std::vector<std::string> result_args;
	result_args.push_back(std::to_string(expr_list.size()));

	for ( auto itr = expr_list.begin(); itr != expr_list.end(); itr++ ) {
		if ( (*itr)->GetKind() != classad::ExprTree::CLASSAD_NODE ) {
			gahp_request->m_result = create_result_string( request_id,
								"499", "Invalid response (bad job element type)" );
			return true;
		}
		classad::ClassAd *job_ad = (classad::ClassAd*)*itr;
		std::string val;

		if ( ! job_ad->EvaluateAttrString("id", val) ) {
			gahp_request->m_result = create_result_string( request_id,
								"499", "Invalid response (no id element)" );
			return true;
		}
		result_args.push_back(val);

		val.clear();
		if ( ! job_ad->EvaluateAttrString("state", val) ) {
			gahp_request->m_result = create_result_string( request_id,
								"499", "Invalid response (no state element)" );
			return true;
		}
		result_args.push_back(val);
	}

	gahp_request->m_result = create_result_string( request_id,
								status_request.errorCode,
								status_request.errorMessage, result_args );

	return true;
}

// Expecting:ARC_JOB_INFO <req_id> <serviceurl> <job_id> [<proj_list>]
bool ArcJobInfoArgsCheck(char **argv, int argc)
{
	return (verify_number_args(argc, 4) || verify_number_args(argc, 5)) &&
		verify_request_id(argv[1]) &&
		verify_string_name(argv[2]) &&
		verify_string_name(argv[3]);
}

// Expecting:ARC_JOB_INFO <req_id> <serviceurl> <job_id> [<proj_list>]
bool ArcJobInfoWorkerFunction(GahpRequest *gahp_request)
{
	int argc = gahp_request->m_args.argc;
	char **argv = gahp_request->m_args.argv;
	int request_id = gahp_request->m_reqid;

	// Fill in required attributes & parameters.
	HttpRequest status_request;
	status_request.serviceURL = fillURL(argv[2]);
	status_request.serviceURL += "/jobs?action=info";
 	status_request.requestMethod = "POST";
	status_request.proxyFile = gahp_request->m_proxy_file;
	status_request.tokenFile = gahp_request->m_token_file;
	formatstr(status_request.requestBody, "{\"job\":[{\"id\":\"%s\"}]}", argv[3]);

	// Send the request.
	if( ! status_request.SendRequest() ) {
		// TODO Fix construction of error message
		gahp_request->m_result = create_result_string( request_id,
								status_request.errorCode,
								status_request.errorMessage );
		return true;
	} else {
		gahp_request->m_result = create_result_string( request_id,
								status_request.errorCode,
								status_request.errorMessage );
	}

	classad::ClassAd resp_ad;
	classad::ClassAdJsonParser parser;
	if ( ! parser.ParseClassAd(status_request.responseBody, resp_ad, true) ) {
		gahp_request->m_result = create_result_string( request_id,
								"499", "Invalid response (parsing json)" );
		return true;
	}

	// First, extract the status-code and reason.
	classad::ClassAd *job_ad = nullptr;
	std::string err_msg;
	if ( !GetSingleJobAd(resp_ad, job_ad, err_msg) ) {
		gahp_request->m_result = create_result_string( request_id,
								"499", err_msg );
		return true;
	}

	std::string val_str;

	if ( ! job_ad->EvaluateAttrString("status-code", val_str) ) {
		gahp_request->m_result = create_result_string( request_id,
								"499", "Invalid response (no status-code element" );
		return true;
	}
	status_request.errorCode = val_str;

	val_str.clear();
	if ( ! job_ad->EvaluateAttrString("reason", val_str) ) {
		gahp_request->m_result = create_result_string( request_id,
								"499", "Invalid response (no reason element)" );
		return true;
	}
	status_request.errorMessage = val_str;

	// Now, extract the ComputingActivity attributes (possibly a subset)
	classad::Value value;
	classad::ClassAd *info_ad = NULL;
	if ( ! job_ad->EvaluateExpr("info_document.ComputingActivity", value) || ! value.IsClassAdValue(info_ad) ) {
		gahp_request->m_result = create_result_string( request_id,
								"499", "Invalid response (no ComputingActivity element)" );
		return true;
	}

	std::vector<std::string> result_args;
	result_args.push_back("");

	classad::ClassAdJsonUnParser unparser;
	if ( argc == 5 ) {
		classad::References refs;
		Tokenize(argv[4]);
		const char *token;
		while ( (token = GetNextToken(",", true)) ) {
			refs.insert(token);
		}
		unparser.Unparse(result_args[0], info_ad, refs);
	} else {
		unparser.Unparse(result_args[0], info_ad);
	}

	gahp_request->m_result = create_result_string( request_id,
								status_request.errorCode,
								status_request.errorMessage, result_args );

	return true;
}

// Expecting:ARC_JOB_STAGE_IN <req_id> <serviceurl> <job_id> <count>
//     <filename>*
bool ArcJobStageInArgsCheck(char **argv, int argc)
{
	// TODO Verify filename arguments?
	if ( argc < 5 ) {
		return false;
	}
	int cnt = atoi(argv[4]);
	return verify_number_args(argc, 5 + cnt) &&
		verify_request_id(argv[1]) &&
		verify_string_name(argv[2]) &&
		verify_string_name(argv[3]);
}

// Expecting:ARC_JOB_STAGE_IN <req_id> <serviceurl> <job_id> <count>
//     <filename>*
bool ArcJobStageInWorkerFunction(GahpRequest *gahp_request)
{
	//int argc = gahp_request->m_args.argc;
	char **argv = gahp_request->m_args.argv;
	int request_id = gahp_request->m_reqid;

	int cnt = atoi(argv[4]);

	// Fill in required attributes & parameters.
	HttpRequest put_request;
 	put_request.requestMethod = "PUT";
	put_request.proxyFile = gahp_request->m_proxy_file;
	put_request.tokenFile = gahp_request->m_token_file;

	// If we're giving 0 files to transfer, just fake a successful result.
	put_request.errorCode = "200";
	put_request.errorMessage = "OK";

	for ( int i = 5; i < cnt + 5; i++ ) {
		put_request.serviceURL = fillURL(argv[2]);
		formatstr_cat(put_request.serviceURL, "/jobs/%s/session/%s", argv[3], condor_basename(argv[i]));
		put_request.requestBodyFilename = argv[i];

		// Send the request.
		if( ! put_request.SendRequest() ) {

			std::string err_msg;
			formatstr(err_msg, "Transfer of %s failed: %s", condor_basename(argv[i]), put_request.errorMessage.c_str());

			gahp_request->m_result = create_result_string( request_id,
								put_request.errorCode,
								err_msg );
			return true;
		}
	}

	gahp_request->m_result = create_result_string( request_id,
								put_request.errorCode,
								put_request.errorMessage );

	return true;
}

// Expecting:ARC_JOB_STAGE_OUT <req_id> <serviceurl> <job_id> <count>
//     [<remote_filename> <local_filename>]*
bool ArcJobStageOutArgsCheck(char **argv, int argc)
{
	// TODO Verify filename arguments?
	if ( argc < 5 ) {
		return false;
	}
	int cnt = atoi(argv[4]);
	return verify_number_args(argc, 5 + (2*cnt)) &&
		verify_request_id(argv[1]) &&
		verify_string_name(argv[2]) &&
		verify_string_name(argv[3]);
}

// Expecting:ARC_JOB_STAGE_OUT <req_id> <serviceurl> <job_id> <count>
//     [<remote_filename> <local_filename>]*
bool ArcJobStageOutWorkerFunction(GahpRequest *gahp_request)
{
	//int argc = gahp_request->m_args.argc;
	char **argv = gahp_request->m_args.argv;
	int request_id = gahp_request->m_reqid;

	int cnt = atoi(argv[4]);

	// Fill in required attributes & parameters.
	HttpRequest get_request;
 	get_request.requestMethod = "GET";
	get_request.proxyFile = gahp_request->m_proxy_file;
	get_request.tokenFile = gahp_request->m_token_file;

	// If we're giving 0 files to transfer, just fake a successful result.
	get_request.errorCode = "200";
	get_request.errorMessage = "OK";

	for ( int i = 5; i < (2*cnt + 5); i += 2 ) {
		get_request.serviceURL = fillURL(argv[2]);
		formatstr_cat(get_request.serviceURL, "/jobs/%s/session/%s", argv[3], condor_basename(argv[i]));
		get_request.responseBodyFilename = argv[i+1];
		
		// Send the request.
		if( ! get_request.SendRequest() ) {
		
			std::string err_msg;
			formatstr(err_msg, "Transfer of %s failed: %s", condor_basename(argv[i+1]), get_request.errorMessage.c_str());

			gahp_request->m_result = create_result_string( request_id,
								get_request.errorCode,
								err_msg );
			return true;
		}
	}

	gahp_request->m_result = create_result_string( request_id,
								get_request.errorCode,
								get_request.errorMessage );

	return true;
}

// Expecting:ARC_KILL <req_id> <serviceurl> <job_id>
bool ArcJobKillArgsCheck(char **argv, int argc)
{
	return verify_number_args(argc, 4) &&
		verify_request_id(argv[1]) &&
		verify_string_name(argv[2]) &&
		verify_string_name(argv[3]);
}

// Expecting:ARC_KILL <req_id> <serviceurl> <job_id>
bool ArcJobKillWorkerFunction(GahpRequest *gahp_request)
{
	int argc = gahp_request->m_args.argc;
	char **argv = gahp_request->m_args.argv;
	int request_id = gahp_request->m_reqid;

	if( ! verify_number_args( argc, 4 ) ) {
		gahp_request->m_result = create_result_string(request_id, "499", "Wrong_Argument_Number");
		dprintf( D_ALWAYS, "Wrong number of arguments (%d should be >= %d) to %s\n",
				 argc, 4, argv[0] );
		return false;
	}

	// Fill in required attributes & parameters.
	HttpRequest status_request;
	status_request.serviceURL = fillURL(argv[2]);
	status_request.serviceURL += "/jobs?action=kill";
 	status_request.requestMethod = "POST";
	status_request.proxyFile = gahp_request->m_proxy_file;
	status_request.tokenFile = gahp_request->m_token_file;
	formatstr(status_request.requestBody, "{\"job\":[{\"id\":\"%s\"}]}", argv[3]);

	// Send the request.
	if( ! status_request.SendRequest() ) {
		// TODO Fix construction of error message
		gahp_request->m_result = create_result_string( request_id,
								status_request.errorCode,
								status_request.errorMessage );
		return true;
	}

	classad::ClassAd resp_ad;
	classad::ClassAdJsonParser parser;
	if ( ! parser.ParseClassAd(status_request.responseBody, resp_ad, true) ) {
		gahp_request->m_result = create_result_string( request_id,
								"499", "Invalid response( parsing json)" );
		return true;
	}

	classad::ClassAd *job_ad = nullptr;
	std::string err_msg;
	if ( !GetSingleJobAd(resp_ad, job_ad, err_msg) ) {
		gahp_request->m_result = create_result_string( request_id,
								"499", err_msg );
		return true;
	}

	std::string val;

	if ( ! job_ad->EvaluateAttrString("status-code", val) ) {
		gahp_request->m_result = create_result_string( request_id,
								"499", "Invalid response (no status-code element)" );
		return true;
	}
	status_request.errorCode = val;

	val.clear();
	if ( ! job_ad->EvaluateAttrString("reason", val) ) {
		gahp_request->m_result = create_result_string( request_id,
								"499", "Invalid response (no reason element)" );
		return true;
	}
	status_request.errorMessage = val;

	gahp_request->m_result = create_result_string( request_id,
								status_request.errorCode,
								status_request.errorMessage );

	return true;
}

// Expecting:ARC_CLEAN <req_id> <serviceurl> <job_id>
bool ArcJobCleanArgsCheck(char **argv, int argc)
{
	return verify_number_args(argc, 4) &&
		verify_request_id(argv[1]) &&
		verify_string_name(argv[2]) &&
		verify_string_name(argv[3]);
}

// Expecting:ARC_CLEAN <req_id> <serviceurl> <job_id>
bool ArcJobCleanWorkerFunction(GahpRequest *gahp_request)
{
	int argc = gahp_request->m_args.argc;
	char **argv = gahp_request->m_args.argv;
	int request_id = gahp_request->m_reqid;

	if( ! verify_number_args( argc, 4 ) ) {
		gahp_request->m_result = create_result_string(request_id, "499", "Wrong_Argument_Number");
		dprintf( D_ALWAYS, "Wrong number of arguments (%d should be >= %d) to %s\n",
				 argc, 4, argv[0] );
		return false;
	}

	// Fill in required attributes & parameters.
	HttpRequest status_request;
	status_request.serviceURL = fillURL(argv[2]);
	status_request.serviceURL += "/jobs?action=clean";
 	status_request.requestMethod = "POST";
	status_request.proxyFile = gahp_request->m_proxy_file;
	status_request.tokenFile = gahp_request->m_token_file;
	formatstr(status_request.requestBody, "{\"job\":[{\"id\":\"%s\"}]}", argv[3]);

	// Send the request.
	if( ! status_request.SendRequest() ) {
		// TODO Fix construction of error message
		gahp_request->m_result = create_result_string( request_id,
								status_request.errorCode,
								status_request.errorMessage );
		return true;
	}

	classad::ClassAd resp_ad;
	classad::ClassAdJsonParser parser;
	if ( ! parser.ParseClassAd(status_request.responseBody, resp_ad, true) ) {
		gahp_request->m_result = create_result_string( request_id,
								"499", "Invalid response (parsing json)" );
		return true;
	}

	classad::ClassAd *job_ad = nullptr;
	std::string err_msg;
	if ( !GetSingleJobAd(resp_ad, job_ad, err_msg) ) {
		gahp_request->m_result = create_result_string( request_id,
								"499", err_msg );
		return true;
	}

	std::string val;

	if ( ! job_ad->EvaluateAttrString("status-code", val) ) {
		gahp_request->m_result = create_result_string( request_id,
								"499", "Invalid response (no status-code element" );
		return true;
	}
	status_request.errorCode = val;

	val.clear();
	if ( ! job_ad->EvaluateAttrString("reason", val) ) {
		gahp_request->m_result = create_result_string( request_id,
								"499", "Invalid response (no reason element)" );
		return true;
	}
	status_request.errorMessage = val;

	gahp_request->m_result = create_result_string( request_id,
								status_request.errorCode,
								status_request.errorMessage );

	return true;
}

// Expecting:ARC_DELEGATION_NEW <req_id> <serviceurl> <proxy-file>
bool ArcDelegationNewArgsCheck(char **argv, int argc)
{
	return verify_number_args(argc, 4) &&
		verify_request_id(argv[1]) &&
		verify_string_name(argv[2]) &&
		verify_string_name(argv[3]);
}

// Expecting:ARC_DELEGATION_NEW <req_id> <serviceurl> <proxy-file>
bool ArcDelegationNewWorkerFunction(GahpRequest *gahp_request)
{
	int argc = gahp_request->m_args.argc;
	char **argv = gahp_request->m_args.argv;
	int request_id = gahp_request->m_reqid;

	if( ! verify_number_args( argc, 4 ) ) {
		gahp_request->m_result = create_result_string(request_id, "499", "Wrong_Argument_Number");
		dprintf( D_ALWAYS, "Wrong number of arguments (%d should be >= %d) to %s\n",
				 argc, 4, argv[0] );
		return false;
	}

	// Fill in required attributes & parameters.
	HttpRequest deleg1_request;
	deleg1_request.serviceURL = fillURL(argv[2]);
	deleg1_request.serviceURL += "/delegations?action=new";
	deleg1_request.requestMethod = "POST";
	deleg1_request.proxyFile = gahp_request->m_proxy_file;
	deleg1_request.tokenFile = gahp_request->m_token_file;
	deleg1_request.includeResponseHeader = true;

	// Send the request.
	if( ! deleg1_request.SendRequest() ) {
		// TODO Fix construction of error message
		gahp_request->m_result = create_result_string( request_id,
								deleg1_request.errorCode,
								deleg1_request.errorMessage );
		return true;
	}

	std::string deleg_url = deleg1_request.responseHeaders["location"];
	std::string deleg_id = deleg_url.substr(deleg_url.rfind('/')+1);
	std::string deleg_cert_request = deleg1_request.responseBody;
	X509Credential deleg_provider(argv[3], "");
	std::string deleg_resp = deleg_provider.Delegate(deleg_cert_request);

	HttpRequest deleg2_request;
	deleg2_request.serviceURL = fillURL(argv[2]);
	deleg2_request.serviceURL += "/delegations/";
	deleg2_request.serviceURL += deleg_id;
	deleg2_request.requestMethod = "PUT";
	deleg2_request.proxyFile = gahp_request->m_proxy_file;
	deleg2_request.tokenFile = gahp_request->m_token_file;
	deleg2_request.requestBody = deleg_resp;

	// Send the request.
	if( ! deleg2_request.SendRequest() ) {
		// TODO Fix construction of error message
		gahp_request->m_result = create_result_string( request_id,
								deleg2_request.errorCode,
								deleg2_request.errorMessage );
		return true;
	}

	std::vector<std::string> result_args;
	result_args.push_back(deleg_id);
	gahp_request->m_result = create_result_string( request_id,
								deleg2_request.errorCode,
								deleg2_request.errorMessage, result_args );
	return true;
}

// Expecting:ARC_DELEGATION_RENEW <req_id> <serviceurl> <deleg-id> <proxy-file>
bool ArcDelegationRenewArgsCheck(char **argv, int argc)
{
	return verify_number_args(argc, 5) &&
		verify_request_id(argv[1]) &&
		verify_string_name(argv[2]) &&
		verify_string_name(argv[3]) &&
		verify_string_name(argv[4]);
}

// Expecting:ARC_DELEGATION_RENEW <req_id> <serviceurl> <deleg-id> <proxy-file>
bool ArcDelegationRenewWorkerFunction(GahpRequest *gahp_request)
{
	int argc = gahp_request->m_args.argc;
	char **argv = gahp_request->m_args.argv;
	int request_id = gahp_request->m_reqid;

	if( ! verify_number_args( argc, 4 ) ) {
		gahp_request->m_result = create_result_string(request_id, "499", "Wrong_Argument_Number");
		dprintf( D_ALWAYS, "Wrong number of arguments (%d should be >= %d) to %s\n",
				 argc, 4, argv[0] );
		return false;
	}

	// Fill in required attributes & parameters.
	HttpRequest deleg1_request;
	deleg1_request.serviceURL = fillURL(argv[2]);
	deleg1_request.serviceURL += "/delegations/";
	deleg1_request.serviceURL += argv[3];
	deleg1_request.serviceURL += "?action=renew";
	deleg1_request.requestMethod = "POST";
	deleg1_request.proxyFile = gahp_request->m_proxy_file;
	deleg1_request.tokenFile = gahp_request->m_token_file;
	deleg1_request.includeResponseHeader = true;

	// Send the request.
	if( ! deleg1_request.SendRequest() ) {
		// TODO Fix construction of error message
		gahp_request->m_result = create_result_string( request_id,
								deleg1_request.errorCode,
								deleg1_request.errorMessage );
		return true;
	}

	std::string deleg_cert_request = deleg1_request.responseBody;
	X509Credential deleg_provider(argv[4], "");
	std::string deleg_resp = deleg_provider.Delegate(deleg_cert_request);

	HttpRequest deleg2_request;
	deleg2_request.serviceURL = fillURL(argv[2]);
	deleg2_request.serviceURL += "/delegations/";
	deleg2_request.serviceURL += argv[3];
	deleg2_request.requestMethod = "PUT";
	deleg2_request.proxyFile = gahp_request->m_proxy_file;
	deleg2_request.tokenFile = gahp_request->m_token_file;
	deleg2_request.requestBody = deleg_resp;

	// Send the request.
	if( ! deleg2_request.SendRequest() ) {
		// TODO Fix construction of error message
		gahp_request->m_result = create_result_string( request_id,
								deleg2_request.errorCode,
								deleg2_request.errorMessage );
		return true;
	}

	gahp_request->m_result = create_result_string( request_id,
								deleg2_request.errorCode,
								deleg2_request.errorMessage );
	return true;
}
