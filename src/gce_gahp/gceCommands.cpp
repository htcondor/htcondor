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
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_string.h"
#include "string_list.h"
#include "condor_arglist.h"
#include "MyString.h"
#include "util_lib_proto.h"
#include "internet.h"
#include "my_popen.h"
#include "basename.h"
#include "vm_univ_utils.h"
#include "gcegahp_common.h"
#include "gceCommands.h"

#include "condor_base64.h"
#include <sstream>
#include "stat_wrapper.h"
#include "Regex.h"
#include <algorithm>
#include <openssl/hmac.h>
#include <curl/curl.h>
#include "thread_control.h"
#include <expat.h>

#define NULLSTRING "NULL"

const char * nullStringIfEmpty( const std::string & str ) {
	if( str.empty() ) { return NULLSTRING; }
	else { return str.c_str(); }
}

//
// Utility function.
//
bool writeShortFile( const std::string & fileName, const std::string & contents ) {
	int fd = safe_open_wrapper_follow( fileName.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600 );

	if( fd < 0 ) {
		dprintf( D_ALWAYS, "Failed to open file '%s' for writing: '%s' (%d).\n", fileName.c_str(), strerror( errno ), errno );
		return false;
	}

	unsigned long written = full_write( fd, contents.c_str(), contents.length() );
	close( fd );
	if( written != contents.length() ) {
		dprintf( D_ALWAYS, "Failed to completely write file '%s'; wanted to write %lu but only put %lu.\n",
				 fileName.c_str(), (unsigned long)contents.length(), written );
		return false;
	}

	return true;
}

//
// Utility function; inefficient.
// FIXME: GT #3924.  Also, broken for binary data with embedded NULs.
//
bool readShortFile( const std::string & fileName, std::string & contents ) {
	int fd = safe_open_wrapper_follow( fileName.c_str(), O_RDONLY, 0600 );

	if( fd < 0 ) {
		dprintf( D_ALWAYS, "Failed to open file '%s' for reading: '%s' (%d).\n", fileName.c_str(), strerror( errno ), errno );
		return false;
	}

	StatWrapper sw( fd );
	unsigned long fileSize = sw.GetBuf()->st_size;

	char * rawBuffer = (char *)malloc( fileSize + 1 );
	assert( rawBuffer != NULL );
	unsigned long totalRead = full_read( fd, rawBuffer, fileSize );
	close( fd );
	if( totalRead != fileSize ) {
		dprintf( D_ALWAYS, "Failed to completely read file '%s'; needed %lu but got %lu.\n",
				 fileName.c_str(), fileSize, totalRead );
		free( rawBuffer );
		return false;
	}
	rawBuffer[ fileSize ] = '\0';
	contents = rawBuffer;
	free( rawBuffer );

	return true;
}

// Utility function for parsing the response returned by the server.
void ParseLine( const char *line, std::string &key, std::string &value,
				int &nesting)
{
	bool in_key = false;
	bool in_value = false;
	key.clear();
	value.clear();

	for ( const char *ptr = line; *ptr; ptr++ ) {
		if ( in_key ) {
			if ( *ptr == '"' ) {
				in_key = false;
			} else {
				key += *ptr;
			}
		} else if ( in_value ) {
			if ( *ptr == '"' ) {
				in_value = false;
			} else {
				value += *ptr;
			}
		} else if ( *ptr == '"' ) {
			if ( key.empty() ) {
				in_key = true;
			} else if ( value.empty() ) {
				in_value = true;
			}
		} else if ( *ptr == '{' || *ptr == '[' ) {
			nesting++;
		} else if ( *ptr == '}' || *ptr == ']' ) {
			nesting--;
		}
	}
}

void ExtractErrorMessage( const std::string &response, std::string &err_msg )
{
	StringList lines( response.c_str(), "\n" );
	std::string key;
	std::string value;
	int nesting = 0;
	const char *line;

	lines.rewind();
	while ( (line = lines.next()) ) {
		ParseLine( line, key, value, nesting );
		if ( nesting == 2 && key == "message" ) {
			err_msg = value;
			return;
		}
	}
}

// Utility function meant for GceInstanceList
void AddInstanceToResult( std::vector<std::string> &result, std::string &id,
						  std::string &name, std::string &status,
						  std::string &status_msg )
{
	result.push_back( id );
	result.push_back( name );
	result.push_back( status );
	if ( status_msg.empty() ) {
		result.push_back( std::string( "NULL" ) );
	} else {
		result.push_back( status_msg );
	}
	id.clear();
	name.clear();
	status.clear();
	status_msg.clear();
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

	std::string source( (const char *)ptr, size * nmemb );
	std::string * ssptr = (std::string *)str;
	ssptr->append( source );

	return (size * nmemb);
}

GceRequest::GceRequest()
{
	includeResponseHeader = false;
}

GceRequest::~GceRequest() { }

pthread_mutex_t globalCurlMutex = PTHREAD_MUTEX_INITIALIZER;
bool GceRequest::SendRequest() 
{
	struct  curl_slist *curl_headers = NULL;
	std::string buf;
	unsigned long responseCode = 0;
	char *ca_dir = NULL;
	char *ca_file = NULL;

	std::string access_token;
	if( ! readShortFile( this->authFile, access_token ) ) {
		this->errorCode = "E_FILE_IO";
		this->errorMessage = "Unable to read from secretkey file '" + this->authFile + "'.";
		dprintf( D_ALWAYS, "Unable to read secretkey file '%s', failing.\n", this->authFile.c_str() );
		return false;
	}
	if ( access_token[ access_token.length() - 1 ] == '\n' ) {
		access_token.erase( access_token.length() - 1 );
	}

	// Generate the final URI.
	// TODO Eliminate this copy if we always use the serviceURL unmodified
	std::string finalURI = this->serviceURL;
	dprintf( D_FULLDEBUG, "Request URI is '%s'\n", finalURI.c_str() );

	// curl_global_init() is not thread-safe.  However, it's safe to call
	// multiple times.  Therefore, we'll just call it before we drop the
	// mutex, since we know that means only one thread is running.
	CURLcode rv = curl_global_init( CURL_GLOBAL_ALL );
	if( rv != 0 ) {
		this->errorCode = "E_CURL_LIB";
		this->errorMessage = "curl_global_init() failed.";
		dprintf( D_ALWAYS, "curl_global_init() failed, failing.\n" );
		return false;
	}

	CURL * curl = curl_easy_init();
	if( curl == NULL ) {
		this->errorCode = "E_CURL_LIB";
		this->errorMessage = "curl_easy_init() failed.";
		dprintf( D_ALWAYS, "curl_easy_init() failed, failing.\n" );
		goto error_return;
	}

	char errorBuffer[CURL_ERROR_SIZE];
	rv = curl_easy_setopt( curl, CURLOPT_ERRORBUFFER, errorBuffer );
	if( rv != CURLE_OK ) {
		this->errorCode = "E_CURL_LIB";
		this->errorMessage = "curl_easy_setopt( CURLOPT_ERRORBUFFER ) failed.";
		dprintf( D_ALWAYS, "curl_easy_setopt( CURLOPT_ERRORBUFFER ) failed (%d): '%s', failing.\n",
				 rv, curl_easy_strerror( rv ) );
		goto error_return;
	}

/*  // Useful for debuggery.  Could be rewritten with CURLOPT_DEBUGFUNCTION
	// and dumped via dprintf() to allow control via EC2_GAHP_DEBUG.
	rv = curl_easy_setopt( curl, CURLOPT_VERBOSE, 1 );
	if( rv != CURLE_OK ) {
		this->errorCode = "E_CURL_LIB";
		this->errorMessage = "curl_easy_setopt( CURLOPT_VERBOSE ) failed.";
		dprintf( D_ALWAYS, "curl_easy_setopt( CURLOPT_VERBOSE ) failed (%d): '%s', failing.\n",
			rv, curl_easy_strerror( rv ) );
		goto error_return;
	}
*/

	rv = curl_easy_setopt( curl, CURLOPT_URL, finalURI.c_str() );
	if( rv != CURLE_OK ) {
		this->errorCode = "E_CURL_LIB";
		this->errorMessage = "curl_easy_setopt( CURLOPT_URL ) failed.";
		dprintf( D_ALWAYS, "curl_easy_setopt( CURLOPT_URL ) failed (%d): '%s', failing.\n",
				 rv, curl_easy_strerror( rv ) );
		goto error_return;
	}

	rv = curl_easy_setopt( curl, CURLOPT_NOPROGRESS, 1 );
	if( rv != CURLE_OK ) {
		this->errorCode = "E_CURL_LIB";
		this->errorMessage = "curl_easy_setopt( CURLOPT_NOPROGRESS ) failed.";
		dprintf( D_ALWAYS, "curl_easy_setopt( CURLOPT_NOPROGRESS ) failed (%d): '%s', failing.\n",
				 rv, curl_easy_strerror( rv ) );
		goto error_return;
	}

	if ( requestMethod == "POST" ) {

		rv = curl_easy_setopt( curl, CURLOPT_POST, 1 );
		if( rv != CURLE_OK ) {
			this->errorCode = "E_CURL_LIB";
			this->errorMessage = "curl_easy_setopt( CURLOPT_POST ) failed.";
			dprintf( D_ALWAYS, "curl_easy_setopt( CURLOPT_POST ) failed (%d): '%s', failing.\n",
					 rv, curl_easy_strerror( rv ) );
			goto error_return;
		}
		
		rv = curl_easy_setopt( curl, CURLOPT_COPYPOSTFIELDS, requestBody.c_str() );
		if( rv != CURLE_OK ) {
			this->errorCode = "E_CURL_LIB";
			this->errorMessage = "curl_easy_setopt( CURLOPT_COPYPOSTFIELDS ) failed.";
			dprintf( D_ALWAYS, "curl_easy_setopt( CURLOPT_COPYPOSTFIELDS ) failed (%d): '%s', failing.\n",
					 rv, curl_easy_strerror( rv ) );
			goto error_return;
		}

	} else if ( requestMethod != "GET" ) {

		rv = curl_easy_setopt( curl, CURLOPT_CUSTOMREQUEST, requestMethod.c_str() );
		if( rv != CURLE_OK ) {
			this->errorCode = "E_CURL_LIB";
			this->errorMessage = "curl_easy_setopt( CURLOPT_CUSTOMREQUEST ) failed.";
			dprintf( D_ALWAYS, "curl_easy_setopt( CURLOPT_CUSTOMREQUEST ) failed (%d): '%s', failing.\n",
					 rv, curl_easy_strerror( rv ) );
			goto error_return;
		}
	}

	buf = "Authorization: Bearer ";
	buf += access_token;
	curl_headers = curl_slist_append( curl_headers, buf.c_str() );
	if ( curl_headers == NULL ) {
		this->errorCode = "E_CURL_LIB";
		this->errorMessage = "curl_slist_append() failed.";
		dprintf( D_ALWAYS, "curl_slist_append() failed, failing.\n" );
		goto error_return;
	}

	curl_headers = curl_slist_append( curl_headers, "Content-Type: application/json" );
	if ( curl_headers == NULL ) {
		this->errorCode = "E_CURL_LIB";
		this->errorMessage = "curl_slist_append() failed.";
		dprintf( D_ALWAYS, "curl_slist_append() failed, failing.\n" );
		goto error_return;
	}

	rv = curl_easy_setopt( curl, CURLOPT_HTTPHEADER, curl_headers );
	if( rv != CURLE_OK ) {
		this->errorCode = "E_CURL_LIB";
		this->errorMessage = "curl_easy_setopt( CURLOPT_HTTPHEADER ) failed.";
		dprintf( D_ALWAYS, "curl_easy_setopt( CURLOPT_HTTPHEADER ) failed (%d): '%s', failing.\n",
				 rv, curl_easy_strerror( rv ) );
		goto error_return;
	}

	if ( includeResponseHeader ) {
		rv = curl_easy_setopt( curl, CURLOPT_HEADER, 1 );
		if( rv != CURLE_OK ) {
			this->errorCode = "E_CURL_LIB";
			this->errorMessage = "curl_easy_setopt( CURLOPT_HEADER ) failed.";
			dprintf( D_ALWAYS, "curl_easy_setopt( CURLOPT_HEADER ) failed (%d): '%s', failing.\n",
					 rv, curl_easy_strerror( rv ) );
			goto error_return;
		}
	}

	rv = curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, & appendToString );
	if( rv != CURLE_OK ) {
		this->errorCode = "E_CURL_LIB";
		this->errorMessage = "curl_easy_setopt( CURLOPT_WRITEFUNCTION ) failed.";
		dprintf( D_ALWAYS, "curl_easy_setopt( CURLOPT_WRITEFUNCTION ) failed (%d): '%s', failing.\n",
				 rv, curl_easy_strerror( rv ) );
		goto error_return;
	}

	rv = curl_easy_setopt( curl, CURLOPT_WRITEDATA, & this->resultString );
	if( rv != CURLE_OK ) {
		this->errorCode = "E_CURL_LIB";
		this->errorMessage = "curl_easy_setopt( CURLOPT_WRITEDATA ) failed.";
		dprintf( D_ALWAYS, "curl_easy_setopt( CURLOPT_WRITEDATA ) failed (%d): '%s', failing.\n",
				 rv, curl_easy_strerror( rv ) );
		goto error_return;
	}

	//
	// Set security options.
	//
	rv = curl_easy_setopt( curl, CURLOPT_SSL_VERIFYPEER, 1 );
	if( rv != CURLE_OK ) {
		this->errorCode = "E_CURL_LIB";
		this->errorMessage = "curl_easy_setopt( CURLOPT_SSL_VERIFYPEER ) failed.";
		dprintf( D_ALWAYS, "curl_easy_setopt( CURLOPT_SSL_VERIFYPEER ) failed (%d): '%s', failing.\n",
				 rv, curl_easy_strerror( rv ) );
		goto error_return;
	}

	rv = curl_easy_setopt( curl, CURLOPT_SSL_VERIFYHOST, 2 );
	if( rv != CURLE_OK ) {
		this->errorCode = "E_CURL_LIB";
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
		ca_dir = getenv( "SOAP_SSL_CA_DIR" );
	}

	ca_file = getenv( "X509_CERT_FILE" );
	if ( ca_file == NULL ) {
		ca_file = getenv( "SOAP_SSL_CA_FILE" );
	}

	// FIXME: Update documentation to reflect no hardcoded default.
	if( ca_dir ) {
		dprintf( D_FULLDEBUG, "Setting CA path to '%s'\n", ca_dir );

		rv = curl_easy_setopt( curl, CURLOPT_CAPATH, ca_dir );
		if( rv != CURLE_OK ) {
			this->errorCode = "E_CURL_LIB";
			this->errorMessage = "curl_easy_setopt( CURLOPT_CAPATH ) failed.";
			dprintf( D_ALWAYS, "curl_easy_setopt( CURLOPT_CAPATH ) failed (%d): '%s', failing.\n",
					 rv, curl_easy_strerror( rv ) );
			goto error_return;
		}
	}

	if( ca_file ) {
		dprintf( D_FULLDEBUG, "Setting CA file to '%s'\n", ca_file );

		rv = curl_easy_setopt( curl, CURLOPT_CAINFO, ca_file );
		if( rv != CURLE_OK ) {
			this->errorCode = "E_CURL_LIB";
			this->errorMessage = "curl_easy_setopt( CURLOPT_CAINFO ) failed.";
			dprintf( D_ALWAYS, "curl_easy_setopt( CURLOPT_CAINFO ) failed (%d): '%s', failing.\n",
					 rv, curl_easy_strerror( rv ) );
			goto error_return;
		}
	}

	if( setenv( "OPENSSL_ALLOW_PROXY", "1", 0 ) != 0 ) {
		dprintf( D_FULLDEBUG, "Failed to set OPENSSL_ALLOW_PROXY.\n" );
	}

	gce_gahp_release_big_mutex();
	pthread_mutex_lock( & globalCurlMutex );
	rv = curl_easy_perform( curl );
	pthread_mutex_unlock( & globalCurlMutex );
	gce_gahp_grab_big_mutex();
	if( rv != 0 ) {
		this->errorCode = "E_CURL_IO";
		std::ostringstream error;
		error << "curl_easy_perform() failed (" << rv << "): '" << curl_easy_strerror( rv ) << "'.";
		this->errorMessage = error.str();
		dprintf( D_ALWAYS, "%s\n", this->errorMessage.c_str() );
		dprintf( D_FULLDEBUG, "%s\n", errorBuffer );
		goto error_return;
	}

	rv = curl_easy_getinfo( curl, CURLINFO_RESPONSE_CODE, & responseCode );
	if( rv != CURLE_OK ) {
		this->errorCode = "E_CURL_LIB";
		this->errorMessage = "curl_easy_getinfo() failed.";
		dprintf( D_ALWAYS, "curl_easy_getinfo( CURLINFO_RESPONSE_CODE ) failed (%d): '%s', failing.\n",
				 rv, curl_easy_strerror( rv ) );
		goto error_return;
	}

	curl_easy_cleanup( curl );
	curl = NULL;

	curl_slist_free_all( curl_headers );
	curl_headers = NULL;

	if( responseCode != 200 ) {
		// this->errorCode = "E_HTTP_RESPONSE_NOT_200";
		formatstr( this->errorCode, "E_HTTP_RESPONSE_NOT_200 (%lu)", responseCode );
		ExtractErrorMessage( resultString, this->errorMessage );
		if( this->errorMessage.empty() ) {
			formatstr( this->errorMessage, "HTTP response was %lu, not 200, and no error message was returned.", responseCode );
		}
		dprintf( D_ALWAYS, "Query did not return 200 (%lu), failing.\n",
				 responseCode );
		dprintf( D_ALWAYS, "Failure response text was '%s'.\n", resultString.c_str() );
		goto error_return;
	}

	dprintf( D_FULLDEBUG, "Response was '%s'\n", resultString.c_str() );
	return true;

 error_return:
	if ( curl ) {
		curl_easy_cleanup( curl );
	}
	if ( curl_headers ) {
		curl_slist_free_all( curl_headers );
	}

	return false;
}


// ---------------------------------------------------------------------------

GcePing::GcePing() { }

GcePing::~GcePing() { }

// Expecting:GCE_PING <req_id> <serviceurl> <authfile> <project> <zone>
bool GcePing::workerFunction(char **argv, int argc, std::string &result_string) {
	assert( strcasecmp( argv[0], "GCE_PING" ) == 0 );

	int requestID;
	get_int( argv[1], & requestID );

	if( ! verify_number_args( argc, 6 ) ) {
		result_string = create_failure_result( requestID, "Wrong_Argument_Number" );
		dprintf( D_ALWAYS, "Wrong number of arguments (%d should be >= %d) to %s\n",
				 argc, 6, argv[0] );
		return false;
	}

	// Fill in required attributes & parameters.
	GcePing ping_request;
	ping_request.serviceURL = argv[2];
	ping_request.serviceURL += "/projects/";
	ping_request.serviceURL += argv[4];
	ping_request.serviceURL += "/zones/";
	ping_request.serviceURL += argv[5];
	ping_request.serviceURL += "/instances";
	ping_request.authFile = argv[3];
	ping_request.requestMethod = "GET";

	// Send the request.
	if( ! ping_request.SendRequest() ) {
		// TODO Fix construction of error message
		result_string = create_failure_result( requestID,
								ping_request.errorMessage.c_str(),
								ping_request.errorCode.c_str() );
	} else {
		result_string = create_success_result( requestID, NULL );
	}

	return true;
}


// ---------------------------------------------------------------------------

GceInstanceInsert::GceInstanceInsert() { }

GceInstanceInsert::~GceInstanceInsert() { }

// Expecting:GCE_INSTACE_INSERT <req_id> <serviceurl> <authfile> <project> <zone>
//     <instance_name> <machine_type> <image> <metadata> <metadata_file>
bool GceInstanceInsert::workerFunction(char **argv, int argc, std::string &result_string) {
	assert( strcasecmp( argv[0], "GCE_INSTANCE_INSERT" ) == 0 );

	int requestID;
	get_int( argv[1], & requestID );

	if( ! verify_number_args( argc, 11 ) ) {
		result_string = create_failure_result( requestID, "Wrong_Argument_Number" );
		dprintf( D_ALWAYS, "Wrong number of arguments (%d should be >= %d) to %s\n",
				 argc, 6, argv[0] );
		return false;
	}

	// Fill in required attributes & parameters.
	GceInstanceInsert insert_request;
	insert_request.serviceURL = argv[2];
	insert_request.serviceURL += "/projects/";
	insert_request.serviceURL += argv[4];
	insert_request.serviceURL += "/zones/";
	insert_request.serviceURL += argv[5];
	insert_request.serviceURL += "/instances";
	insert_request.authFile = argv[3];
	insert_request.requestMethod = "POST";

	// TODO Construct insert_request.requestBody;
	insert_request.requestBody = "{\n";
	insert_request.requestBody += "\"machineType\": \"";
	insert_request.requestBody += argv[7];
	insert_request.requestBody += "\",\n";
	insert_request.requestBody += "\"name\": \"";
	insert_request.requestBody += argv[6];
	insert_request.requestBody += "\",\n";
	insert_request.requestBody += "\"image\": \"";
	insert_request.requestBody += argv[8];
	insert_request.requestBody += "\",\n";
	insert_request.requestBody += "\"networkInterfaces\": [\n{\n";
	insert_request.requestBody += "\"network\": \"https://www.googleapis.com/compute/v1beta16/projects/jfrey-condor/global/networks/default\",\n";
	insert_request.requestBody += "\"accessConfigs\": [\n{\n";
	insert_request.requestBody += "\"name\": \"External NAT\",\n";
	insert_request.requestBody += "\"type\": \"ONE_TO_ONE_NAT\"\n";
	insert_request.requestBody += "}\n]\n";
	insert_request.requestBody += "}\n]\n";
	insert_request.requestBody += "}\n";

	// Send the request.
	if( ! insert_request.SendRequest() ) {
		// TODO Fix construction of error message
		result_string = create_failure_result( requestID,
							insert_request.errorMessage.c_str(),
							insert_request.errorCode.c_str() );
	} else {
		// TODO Read insert_reqest.resultString for instance id
		//   Instance id isn't provided in response!
		StringList reply;
		reply.append( "dummy" );

		result_string = create_success_result( requestID, &reply );
	}

	return true;
}

// ---------------------------------------------------------------------------

GceInstanceDelete::GceInstanceDelete() { }

GceInstanceDelete::~GceInstanceDelete() { }

// Expecting:GCE_INSTACE_DELETE <req_id> <serviceurl> <authfile> <project> <zone> <instance_name>
bool GceInstanceDelete::workerFunction(char **argv, int argc, std::string &result_string) {
	assert( strcasecmp( argv[0], "GCE_INSTANCE_DELETE" ) == 0 );

	int requestID;
	get_int( argv[1], & requestID );

	if( ! verify_number_args( argc, 7 ) ) {
		result_string = create_failure_result( requestID, "Wrong_Argument_Number" );
		dprintf( D_ALWAYS, "Wrong number of arguments (%d should be >= %d) to %s\n",
				 argc, 6, argv[0] );
		return false;
	}

	// Fill in required attributes & parameters.
	GceInstanceDelete delete_request;
	delete_request.serviceURL = argv[2];
	delete_request.serviceURL += "/projects/";
	delete_request.serviceURL += argv[4];
	delete_request.serviceURL += "/zones/";
	delete_request.serviceURL += argv[5];
	delete_request.serviceURL += "/instances/";
	delete_request.serviceURL += argv[6];
	delete_request.authFile = argv[3];
	delete_request.requestMethod = "DELETE";

	// Send the request.
	if( ! delete_request.SendRequest() ) {
		// TODO Fix construction of error message
		result_string = create_failure_result( requestID,
							delete_request.errorMessage.c_str(),
							delete_request.errorCode.c_str() );
	} else {
		result_string = create_success_result( requestID, NULL );
	}

	return true;
}

// ---------------------------------------------------------------------------

GceInstanceList::GceInstanceList() { }

GceInstanceList::~GceInstanceList() { }

// Expecting:GCE_INSTANCE_LIST <req_id> <serviceurl> <authfile> <project> <zone>
bool GceInstanceList::workerFunction(char **argv, int argc, std::string &result_string) {
	assert( strcasecmp( argv[0], "GCE_INSTANCE_LIST" ) == 0 );

	int requestID;
	get_int( argv[1], & requestID );

	if( ! verify_number_args( argc, 6 ) ) {
		result_string = create_failure_result( requestID, "Wrong_Argument_Number" );
		dprintf( D_ALWAYS, "Wrong number of arguments (%d should be >= %d) to %s\n",
				 argc, 6, argv[0] );
		return false;
	}

	// Fill in required attributes & parameters.
	GceInstanceList list_request;
	list_request.serviceURL = argv[2];
	list_request.serviceURL += "/projects/";
	list_request.serviceURL += argv[4];
	list_request.serviceURL += "/zones/";
	list_request.serviceURL += argv[5];
	list_request.serviceURL += "/instances";
	list_request.authFile = argv[3];
	list_request.requestMethod = "GET";

	// Send the request.
	if( ! list_request.SendRequest() ) {
		// TODO Fix construction of error message
		result_string = create_failure_result( requestID,
							list_request.errorMessage.c_str(),
							list_request.errorCode.c_str() );
	} else {
		StringList response( list_request.resultString.c_str(), "\n" );
		std::string next_id;
		std::string next_name;
		std::string next_status;
		std::string next_status_msg;
		std::vector<std::string> results;
		std::string key;
		std::string value;
		int nesting = 0;

		const char *line;
		response.rewind();
		while( (line = response.next()) ) {
			ParseLine( line, key, value, nesting );
			if ( nesting != 3 ) {
				continue;
			}
			if ( key == "id" ) {
				if ( !next_id.empty() ) {
					AddInstanceToResult( results, next_id, next_name,
										 next_status, next_status_msg );
				}
				next_id = value;
			} else if ( key == "name" ) {
				if ( !next_name.empty() ) {
					AddInstanceToResult( results, next_id, next_name,
										 next_status, next_status_msg );
				}
				next_name = value;
			} else if ( key == "status" ) {
				if ( !next_status.empty() ) {
					AddInstanceToResult( results, next_id, next_name,
										 next_status, next_status_msg );
				}
				next_status = value;
			} else if ( key == "statusMessage" ) {
				if ( !next_status_msg.empty() ) {
					AddInstanceToResult( results, next_id, next_name,
										 next_status, next_status_msg );
				}
				next_status_msg = value;
			}
		}
		if ( !next_name.empty() ) {
			AddInstanceToResult( results, next_id, next_name,
								 next_status, next_status_msg );
		}

		char buff[16];
		sprintf( buff, "%d", (int)(results.size() / 4) );

		response.clearAll();
		response.append( buff );
		for ( std::vector<std::string>::iterator idx = results.begin(); idx != results.end(); idx++ ) {
			response.append( idx->c_str() );
		}

		result_string = create_success_result( requestID, &response );
	}

	return true;
}
