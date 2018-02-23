/***************************************************************
 *
 * Copyright (C) 1990-2015, Condor Team, Computer Sciences Department,
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

#include <sstream>

#include "openstackCommands.h"
#include "openstackgahp_common.h"
#include "thread_control.h"
#include <curl/curl.h>

#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

using std::string;

// Globals --------------------------------------------------------------------

#define NULLSTRING "NULL"
pthread_mutex_t globalCurlMutex = PTHREAD_MUTEX_INITIALIZER;

// For authentication.
string currentURL;
string currentUsername;
string currentPassword;
string currentProject;

// Utility functions. ---------------------------------------------------------

const char * nullStringIfEmpty( const string & str ) {
	if( str.empty() ) { return NULLSTRING; }
	else { return str.c_str(); }
}

string emptyIfNullString( const char * str ) {
	if( str == NULL ) { return string( "" ); }
	if( strcmp( NULLSTRING, str ) == 0 ) { return ""; }
	else { return string( str); }
}

//
// "This function gets called by libcurl as soon as there is data received
//  that needs to be saved. The size of the data pointed to by ptr is size
//  multiplied with nmemb, it will not be zero terminated. Return the number
//  of bytes actually taken care of. If that amount differs from the amount
//  passed to your function, it'll signal an error to the library. This will
//  abort the transfer and return CURLE_WRITE_ERROR."
//
size_t appendToString( const void * ptr, size_t size, size_t nmemb, void * str ) {
	if( size == 0 || nmemb == 0 ) { return 0; }

	string source( (const char *)ptr, size * nmemb );
	string * ssptr = (string *)str;
	ssptr->append( source );

	return (size * nmemb);
}

// Authentication functions. --------------------------------------------------

class KeystoneToken;
class KeystoneToken {
	public:
		// Gets a token from the Keystone URL for the given username,
		// password, and project.  Manages a cache of those tokens and
		// automatically renews them as necessary.  This function
		// may block, but must be called with the global_big_mutex locked,
		// since it will unlock in order to block on the renew condition.
		// This will happen without any special work in normal GAHP code.
		static const KeystoneToken & get(	const string & url,
											const string & username,
											const string & password,
											const string & project );

		bool isValid() const;
		const string & getAuthToken() const { return token; }
		const string & getErrorCode() const { return errorCode; }
		const string & getErrorMessage() const { return errorMessage; }

		~KeystoneToken() { pthread_cond_destroy( & condition ); }

	private:
		KeystoneToken() : isRenewing( false ), expiration( 0 ), lastAttempt( 0 ) {
			pthread_cond_init( & condition, NULL );
		}
		bool getToken(	const string & url, const string & username,
						const string & password, const string & project );

		string token;
		string errorCode;
		string errorMessage;

		// We don't need to record the authentication information unless
		// we expect the GAHP to be so busy that a token that was valid
		// when the command was received becomes invalid by the time the
		// command actually executes.  In this scenario, I tend to think
		// the correct thing to do is complain, rather than create more
		// work by attempting a renewal.

		bool			isRenewing;
		pthread_cond_t	condition;
		time_t			expiration;
		time_t			lastAttempt;

		// key = url + username + password + project
		typedef std::map< string, KeystoneToken > Cache;
		typedef Cache::iterator CacheIterator;
		static Cache cache;
};
KeystoneToken::Cache KeystoneToken::cache;

bool KeystoneToken::isValid() const {
	return time(NULL) < expiration;
}

const KeystoneToken & KeystoneToken::get(	const string & url,
											const string & username,
											const string & password,
											const string & project ) {
	string hashKey = url + username + password + project;
	CacheIterator ci = KeystoneToken::cache.find( hashKey );
	if( ci != KeystoneToken::cache.end() ) {
		dprintf( D_FULLDEBUG, "Found cached token for ( %s, %s, %s, %s )\n", url.c_str(), username.c_str(), password.c_str(), project.c_str() );
		KeystoneToken & kt = ci->second;

		if( (! kt.isValid()) || (kt.expiration - time(NULL) < (5 * 60)) ) {
			dprintf( D_FULLDEBUG, "Token was either invalid or about to expire, renewing.\n" );
			kt.getToken( url, username, password, project );
		}

		return kt;
	}

	dprintf( D_FULLDEBUG, "Creating token for ( %s, %s, %s, %s )\n", url.c_str(), username.c_str(), password.c_str(), project.c_str() );
	KeystoneToken kt;
	KeystoneToken::cache.insert( make_pair( hashKey, kt ) );
	return KeystoneToken::get( url, username, password, project );
}

// JSON string constants.  Should be marginally easier to read.
#define AUTH_		"\"auth\": "
#define IDENTITY_	"\"identity\": "
#define METHODS_	"\"methods\": "
#define PASSWORD	"\"password\""
#define PASSWORD_	"\"password\": "
#define USER_		"\"user\": "
#define NAME_		"\"name\": "
#define DOMAIN_		"\"domain\": "
#define ID_			"\"id\": "
#define DEFAULT		"\"default\""
#define SCOPE_		"\"scope\": "
#define PROJECT_	"\"project\": "

#define JSON_STRING	"\"#\""

bool KeystoneToken::getToken(	const string & url, const string & username,
								const string & password,
								const string & project ) {
	dprintf( D_FULLDEBUG, "Attempting getToken( %s, %s, %s, %s )\n", url.c_str(), username.c_str(), password.c_str(), project.c_str() );

	// Locking.  If we're already trying to renew the token, wait for that
	// attempt to complete and don't retry unless it's been long enough.
	while( this->isRenewing ) {
		pthread_cond_wait( & this->condition, & global_big_mutex );
	}

	if( this->isValid() ) {
		dprintf( D_FULLDEBUG, "Not getting token (%s, %s, %s, %s): somebody else already renewed it.\n",
			url.c_str(), username.c_str(), password.c_str(), project.c_str() );
		return true;
	}

	if( time( NULL ) - this->lastAttempt < (5 * 60) ) {
		dprintf( D_FULLDEBUG, "Not getting token (%s, %s, %s, %s): tried too recently.\n",
			url.c_str(), username.c_str(), password.c_str(), project.c_str() );
		return false;
	}

	this->isRenewing = true;

	NovaRequest tokenRequest;
	tokenRequest.includeResponseHeader = true;
	tokenRequest.serviceURL = url + "/v3/auth/tokens?nocatalog";
	tokenRequest.requestMethod = "POST";

	if( project.empty() ) {
		formatstr( tokenRequest.requestBody,
			"{ " AUTH_ "{ "
				IDENTITY_ "{ "
					METHODS_ "[ " PASSWORD " ], "
					PASSWORD_ "{ "
						USER_ "{ "
							NAME_ "\"%s\", "
							DOMAIN_ "{ " ID_ DEFAULT " }, "
							PASSWORD_ "\"%s\" "
						"} "
					"} "
				"} "
			"}",
			username.c_str(),
			password.c_str()
		);
	} else {
		formatstr( tokenRequest.requestBody,
			"{ " AUTH_ "{ "
				IDENTITY_ "{ "
					METHODS_ "[ " PASSWORD " ], "
					PASSWORD_ "{ "
						USER_ "{ "
							NAME_ "\"%s\", "
							DOMAIN_ "{ " ID_ DEFAULT " }, "
							PASSWORD_ "\"%s\" "
						"} "
					"} "
				"}, "
				SCOPE_ "{ "
					PROJECT_ "{ "
						NAME_ "\"%s\", "
						DOMAIN_ "{ " ID_ DEFAULT " } "
					"} "
				"} "
			"} " "}"
			,
			username.c_str(),
			password.c_str(),
			project.c_str()
		);
	}

	if( ! tokenRequest.sendRequest() ) {
		this->token = "";
		this->expiration = 0;
		this->errorCode = tokenRequest.errorCode;
		this->errorMessage = tokenRequest.errorMessage;

		dprintf( D_FULLDEBUG, "Failed getToken( %s, %s, %s, %s ): '%s' (%s).\n",
			url.c_str(), username.c_str(), password.c_str(), project.c_str(),
			this->errorCode.c_str(),
			this->errorMessage.c_str() );
		goto done;
	}

	if( tokenRequest.responseCode != 201 ) {
		this->token = "";
		this->expiration = 0;

		formatstr( this->errorCode, "E_HTTP_RESPONSE_NOT_201 (%lu)", tokenRequest.responseCode );
		this->errorMessage = tokenRequest.responseString;
		if( this->errorMessage.empty() ){
			formatstr( this->errorMessage, "HTTP response was %lu, not 201, and no error message was returned.", tokenRequest.responseCode );
		}

		dprintf( D_FULLDEBUG, "Failed getToken( %s, %s, %s, %s ): '%s' (%s).\n",
			url.c_str(), username.c_str(), password.c_str(), project.c_str(),
			this->errorCode.c_str(),
			this->errorMessage.c_str() );
		goto done;
	} else {
		// The X-Subject-Token header contains the token "ID".
		size_t idx = tokenRequest.responseString.find( "\r\n\r\n" );
		if( idx == string::npos ) {
			this->token = "";
			this->expiration = 0;
			this->errorCode = "E_NO_HEADER";
			this->errorMessage = "Unable to find HTTP header in response.\n";

			dprintf( D_FULLDEBUG, "Failed getToken( %s, %s, %s, %s ): '%s' (%s).\n",
				url.c_str(), username.c_str(), password.c_str(), project.c_str(),
				this->errorCode.c_str(),
				this->errorMessage.c_str() );
			goto done;
		}

		string header = tokenRequest.responseString.substr( 0, idx + 4 );
		string body = tokenRequest.responseString.substr( idx + 4 );

		idx = header.find( "\r\nX-Subject-Token:" );
		if( idx == string::npos ) {
			this->token = "";
			this->expiration = 0;
			this->errorCode = "E_NO_TOKEN";
			this->errorMessage = "Unable to find start of token in response headers.\n";

			dprintf( D_FULLDEBUG, "Failed getToken( %s, %s, %s, %s ): '%s' (%s).\n",
				url.c_str(), username.c_str(), password.c_str(), project.c_str(),
				this->errorCode.c_str(),
				this->errorMessage.c_str() );
			goto done;
		}

		size_t end = header.find( "\r\n", idx + 19 );
		if( end == string::npos ) {
			this->token = "";
			this->expiration = 0;
			this->errorCode = "E_NO_TOKEN";
			this->errorMessage = "Unable to find end of token in response headers.\n";

			dprintf( D_FULLDEBUG, "Failed getToken( %s, %s, %s, %s ): '%s' (%s).\n",
				url.c_str(), username.c_str(), password.c_str(), project.c_str(),
				this->errorCode.c_str(),
				this->errorMessage.c_str() );
			goto done;
		}
		this->token = header.substr( idx + 19, (end - (idx + 19)) );

		// the "expires_at" JSON attribute of the body.
		rapidjson::Document d;
		d.Parse( body.c_str() );
		if( d.HasParseError() ) {
			this->token = "";
			this->expiration = 0;
			this->errorCode = "E_NOT_JSON";
			formatstr(	this->errorMessage,
						"Failed to parse response body ('%s').",
						body.c_str() );

			dprintf( D_FULLDEBUG, "Failed getToken( %s, %s, %s, %s ): '%s' (%s).\n",
				url.c_str(), username.c_str(), password.c_str(), project.c_str(),
				this->errorCode.c_str(),
				this->errorMessage.c_str() );
			goto done;
		}

		rapidjson::Value::MemberIterator j = d.FindMember( "token" );
		if( j == d.MemberEnd() ) {
			this->token = "";
			this->expiration = 0;
			this->errorCode = "E_NO_MEMBER";
			formatstr(	this->errorMessage,
						"Failed to find token in body ('%s').",
						body.c_str() );

			dprintf( D_FULLDEBUG, "Failed getToken( %s, %s, %s, %s ): '%s' (%s).\n",
				url.c_str(), username.c_str(), password.c_str(), project.c_str(),
				this->errorCode.c_str(),
				this->errorMessage.c_str() );
			goto done;
		}

		if(! j->value.IsObject()) {
			this->token = "";
			this->expiration = 0;
			this->errorCode = "E_NOT_OBJECT";
			formatstr( this->errorMessage,
						"Token attribute not a JSON object in body ('%s').",
						body.c_str() );

			dprintf( D_FULLDEBUG, "Failed getToken( %s, %s, %s, %s ): '%s' (%s).\n",
				url.c_str(), username.c_str(), password.c_str(), project.c_str(),
				this->errorCode.c_str(),
				this->errorMessage.c_str() );
			goto done;
		}

		rapidjson::Value::MemberIterator i = j->value.FindMember( "expires_at" );
		if( i == d.MemberEnd() ) {
			this->token = "";
			this->expiration = 0;
			this->errorCode = "E_NO_EXPIRES_AT_MEMBER";
			formatstr( this->errorMessage,
						"Failed to find expires_at in body ('%s').",
						body.c_str() );

			dprintf( D_FULLDEBUG, "Failed getToken( %s, %s, %s, %s ): '%s' (%s).\n",
				url.c_str(), username.c_str(), password.c_str(), project.c_str(),
				this->errorCode.c_str(),
				this->errorMessage.c_str() );
			goto done;
		}

		if(! i->value.IsString()) {
			this->token = "";
			this->expiration = 0;
			this->errorCode = "E_EXPIRES_AT_NOT_STRING";
			formatstr( this->errorMessage,
						"Attribute expires_at not string in body ('%s').",
						body.c_str() );

			dprintf( D_FULLDEBUG, "Failed getToken( %s, %s, %s, %s ): '%s' (%s).\n",
				url.c_str(), username.c_str(), password.c_str(), project.c_str(),
				this->errorCode.c_str(),
				this->errorMessage.c_str() );
			goto done;
		}
		string expirationString = i->value.GetString();

		// Convert the ISO 8601 extended format date time (with microseconds)
		// string into a time_t.
		struct tm expiration; memset( & expiration, 0, sizeof( expiration ) );
		char * ptr = strptime( expirationString.c_str(), "%Y-%m-%dT%H:%M:%S", & expiration );
		if( ptr == NULL || * ptr != '.' ) {
			this->token = "";
			this->expiration = 0;
			this->errorCode = "E_EXPIRES_AT_NOT_VALID";
			formatstr( this->errorMessage,
						"Attribute expires_at not valid ('%s').",
						expirationString.c_str() );

			dprintf( D_FULLDEBUG, "Failed getToken( %s, %s, %s, %s ): '%s' (%s).\n",
				url.c_str(), username.c_str(), password.c_str(), project.c_str(),
				this->errorCode.c_str(),
				this->errorMessage.c_str() );
			goto done;
		}
		// We could parse the microseconds, but why bother?
		this->expiration = timegm( & expiration );

		// At this point, we could also look for the service catalog, which
		// "specifies all the services available to/for the token."  Each
		// service may include its endpoints, each of which may specify a
		// region and a URL.  However, since even the catalog itself is
		// optional, it seems like we might as well just query for a region's
		// Nova endpoint directly when necessary (caching the result).

		this->errorCode = "";
		this->errorMessage = "";
		dprintf( D_FULLDEBUG, "Succeeded getToken( %s, %s, %s, %s ) = %s (expiring at %lu)\n",
			url.c_str(), username.c_str(), password.c_str(), project.c_str(),
			this->token.c_str(), this->expiration );
		goto done;
	}

done:
	// We set the last attempt time to when we finish getting the reply
	// from the service because we could block before sending the request.
	this->lastAttempt = time( NULL );
	this->isRenewing = false;
	pthread_cond_broadcast( & condition );
	return this->isValid();
}

// ----------------------------------------------------------------------------

NovaRequest::NovaRequest() :
	contentType( "application/json" ),
	responseCode(0),
	includeResponseHeader( false ) { }
NovaRequest::~NovaRequest() { }

std::map< string, string > NovaRegionEndpointCache;
bool NovaRequest::getNovaEndpointForRegion( const string & requestedRegion ) {
	// This forces the cache to refresh when the auth token does.
	string hashKey = this->authToken + requestedRegion;
	string & endpoint = NovaRegionEndpointCache[ hashKey ];
	if(! endpoint.empty()) {
		dprintf( D_FULLDEBUG, "Found cached endpoint '%s' for region '%s' (using auth token '%s').\n", endpoint.c_str(), requestedRegion.c_str(), this->authToken.c_str() );
		this->serviceURL = endpoint;
		return true;
	}

	dprintf( D_FULLDEBUG, "Looking up endpoint for region '%s' (using auth token '%s'.\n", requestedRegion.c_str(), this->authToken.c_str() );

	// We need to look up the Nova endpoint URL for this region.
	NovaRequest regionRequest;
	regionRequest.serviceURL = currentURL + "/v3/auth/catalog";
	regionRequest.requestMethod = "GET";
	regionRequest.authToken = this->authToken;
	regionRequest.requestBody = "";

	if(! regionRequest.sendRequest()) {
		this->errorCode = regionRequest.errorCode;
		this->errorMessage = regionRequest.errorMessage;
		return false;
	}

	if( regionRequest.responseCode != 200 ) {
		formatstr( this->errorCode, "E_HTTP_RESPONSE_NOT_200 (%lu)", regionRequest.responseCode );
		this->errorMessage = regionRequest.responseString;
		if( this->errorMessage.empty() ){
			formatstr( this->errorMessage, "HTTP response was %lu, not 201, and no error message was returned.", regionRequest.responseCode );
		}
		return false;
	}

	this->serviceURL = "";

	rapidjson::Document d;
	string & body = regionRequest.responseString;
	d.Parse( body.c_str() );
	if( d.HasParseError() ) {
		this->errorCode = "E_NOT_JSON";
		formatstr(	this->errorMessage,
					"Failed to parse response body ('%s').",
					body.c_str() );
		return false;
	}

	rapidjson::Value::MemberIterator j = d.FindMember( "catalog" );
	if( j == d.MemberEnd() ) {
		this->errorCode = "E_NO_MEMBER";
		formatstr(	this->errorMessage,
					"Failed to find catalog in body ('%s').",
					body.c_str() );
		return false;
	}

	if(! j->value.IsArray()) {
		this->errorCode = "E_NOT_ARRAY";
		formatstr(	this->errorMessage,
					"Catalog attribute not a JSON array in body ('%s').",
					body.c_str() );
		return false;
	}

	// If we ever care, we can replace the continues with warnings.
	for( rapidjson::SizeType i = 0; i < j->value.Size(); ++i ) {
		const rapidjson::Value & service = j->value[i];

		// assert( service.type == "compute" )
		rapidjson::Value::ConstMemberIterator serviceType = service.FindMember( "type" );
		if( serviceType == service.MemberEnd() ) { continue; }
		if( ! serviceType->value.IsString() ) { continue; }
		if( 0 != strcasecmp( "compute", serviceType->value.GetString() ) ) { continue; }

		// Look for the service's public endpoint.
		rapidjson::Value::ConstMemberIterator endpoints = service.FindMember( "endpoints" );
		if( endpoints == service.MemberEnd() ) { continue; }
		if( ! endpoints->value.IsArray() ) { continue; }

		for( rapidjson::SizeType endpointIndex = 0; endpointIndex < endpoints->value.Size(); ++endpointIndex ) {
			const rapidjson::Value & endpoint = endpoints->value[ endpointIndex ];

			// asssert( endpoint.interface == "public" )
			rapidjson::Value::ConstMemberIterator interface = endpoint.FindMember( "interface" );
			if( interface == endpoint.MemberEnd() ) { continue; }
			if( ! interface->value.IsString() ) { continue; }
			if( 0 != strcasecmp( "public", interface->value.GetString() ) ) { continue; }

			// assert( endpoint.region == requestedRegion )
			rapidjson::Value::ConstMemberIterator region = endpoint.FindMember( "region" );
			if( region == endpoint.MemberEnd() ) { continue; }
			if( ! region->value.IsString() ) { continue; }
			if( 0 != strcasecmp( requestedRegion.c_str(), region->value.GetString() ) ) { continue; }

			rapidjson::Value::ConstMemberIterator url = endpoint.FindMember( "url" );
			if( url == endpoint.MemberEnd() ) { continue; }
			if( ! url->value.IsString() ) { continue; }

			this->serviceURL = url->value.GetString();
			dprintf(	D_FULLDEBUG, "Using url '%s' for nova service in region '%s'.\n",
						this->serviceURL.c_str(), requestedRegion.c_str() );
		}
	}

	if( serviceURL.empty() ) {
		this->errorCode = "E_NOT_FOUND";
		formatstr(	this->errorMessage,
					"Failed to find a public endpoint for nova service in region '%s'.",
					requestedRegion.c_str() );
		dprintf( D_FULLDEBUG, "%s\n", body.c_str() );
		return false;
	}

	endpoint = serviceURL;
	return true;
}

bool NovaRequest::getAuthToken() {
	const KeystoneToken & kt = KeystoneToken::get( currentURL, currentUsername, currentPassword, currentProject );
	if(! kt.isValid()) {
		this->errorCode = kt.getErrorCode();
		this->errorMessage = kt.getErrorMessage();
		return false;
	}

	this->authToken = kt.getAuthToken();
	return true;
}

bool NovaRequest::sendRequest() {
	// gcc really hates it when you cross the initialization of variables,
	// even if the code after the jump doesn't use them.
	string buf( "" );
	char * ca_dir = NULL;
	char * ca_file = NULL;
	struct curl_slist * curl_headers = NULL;

	// We could probably skip this copy.
	string finalURI = this->serviceURL;

	dprintf( D_FULLDEBUG, "Request URI is '%s'\n", finalURI.c_str() );
	if ( this->requestMethod == "POST" ) {
		dprintf( D_FULLDEBUG, "Request body is '%s'\n", this->requestBody.c_str() );
	}

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

	if ( this->requestMethod == "POST" ) {

		rv = curl_easy_setopt( curl, CURLOPT_POST, 1 );
		if( rv != CURLE_OK ) {
			this->errorCode = "E_CURL_LIB";
			this->errorMessage = "curl_easy_setopt( CURLOPT_POST ) failed.";
			dprintf( D_ALWAYS, "curl_easy_setopt( CURLOPT_POST ) failed (%d): '%s', failing.\n",
					 rv, curl_easy_strerror( rv ) );
			goto error_return;
		}

		rv = curl_easy_setopt( curl, CURLOPT_POSTFIELDS, this->requestBody.c_str() );
		if( rv != CURLE_OK ) {
			this->errorCode = "E_CURL_LIB";
			this->errorMessage = "curl_easy_setopt( CURLOPT_POSTFIELDS ) failed.";
			dprintf( D_ALWAYS, "curl_easy_setopt( CURLOPT_POSTFIELDS ) failed (%d): '%s', failing.\n",
					 rv, curl_easy_strerror( rv ) );
			goto error_return;
		}

	} else if ( this->requestMethod != "GET" ) {

		rv = curl_easy_setopt( curl, CURLOPT_CUSTOMREQUEST, this->requestMethod.c_str() );
		if( rv != CURLE_OK ) {
			this->errorCode = "E_CURL_LIB";
			this->errorMessage = "curl_easy_setopt( CURLOPT_CUSTOMREQUEST ) failed.";
			dprintf( D_ALWAYS, "curl_easy_setopt( CURLOPT_CUSTOMREQUEST ) failed (%d): '%s', failing.\n",
					 rv, curl_easy_strerror( rv ) );
			goto error_return;
		}
	}

	if( ! this->authToken.empty() ) {
		buf = "X-Auth-Token: " + this->authToken;
		curl_headers = curl_slist_append( curl_headers, buf.c_str() );
		if ( curl_headers == NULL ) {
			this->errorCode = "E_CURL_LIB";
			this->errorMessage = "curl_slist_append() failed.";
			dprintf( D_ALWAYS, "curl_slist_append() failed, failing.\n" );
			goto error_return;
		}
	}

	buf = "Content-Type: " + this->contentType;
	curl_headers = curl_slist_append( curl_headers, buf.c_str() );
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

	rv = curl_easy_setopt( curl, CURLOPT_WRITEDATA, & this->responseString );
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

	openstack_gahp_release_big_mutex();
	pthread_mutex_lock( & globalCurlMutex );
	rv = curl_easy_perform( curl );
	pthread_mutex_unlock( & globalCurlMutex );
	openstack_gahp_grab_big_mutex();

	if( rv != 0 ) {
		this->errorCode = "E_CURL_IO";
		std::ostringstream error;
		error << "curl_easy_perform() failed (" << rv << "): '" << curl_easy_strerror( rv ) << "'.";
		this->errorMessage = error.str();
		dprintf( D_ALWAYS, "%s\n", this->errorMessage.c_str() );
		dprintf( D_FULLDEBUG, "%s\n", errorBuffer );
		goto error_return;
	}

	rv = curl_easy_getinfo( curl, CURLINFO_RESPONSE_CODE, & this->responseCode );
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

	dprintf( D_FULLDEBUG, "Response was code %lu, '%s'\n", this->responseCode, this->responseString.c_str() );
	return true;

error_return:
	if( curl ) {
		curl_easy_cleanup( curl );
	}

	if( curl_headers ) {
		curl_slist_free_all( curl_headers );
	}

	return false;
}

// ----------------------------------------------------------------------------

KeystoneService::KeystoneService() { }
KeystoneService::~KeystoneService() { }

// KEYSTONE_SERVICE <url> <username> <password> <project>
bool KeystoneService::workerFunction( char ** argv, int argc, string & resultLine ) {
	assert( strcasecmp( argv[0], "KEYSTONE_SERVICE" ) == 0 );

	if( ! verify_number_args( argc, 5 ) ) {
		dprintf( D_ALWAYS, "Wrong number of arguments (%d should be >= %d) to %s\n", argc, 5, argv[0] );
		return false;
	}

	// NB: Since only one thread can be running at a time, we don't need to
	// do anything special to make these assignments atomic.  However, we'd
	// have to do something rather exotic if the worker threads could wake
	// up out of order.
	currentURL = argv[1];
	currentUsername = argv[2];
	currentPassword = argv[3];
	currentProject = emptyIfNullString( argv[4] );

	// KEYSTONE_SERVICE is defined to have no result line.
	resultLine = "";
	return true;
}

//
// There's a lot of boilerplate in the workerFunction()s that we should
// probably factor out.
//

// ----------------------------------------------------------------------------

NovaPing::NovaPing() { }
NovaPing::~NovaPing() { }

// NOVA_PING <request-id> <region>
bool NovaPing::workerFunction( char ** argv, int argc, string & resultLine ) {
	assert( strcasecmp( argv[0], "NOVA_PING" ) == 0 );

	int requestID;
	get_int( argv[1], & requestID );

	if( ! verify_number_args( argc, 3 ) ) {
		dprintf( D_ALWAYS, "Wrong number of arguments (%d should be >= %d) to %s\n", argc, 3, argv[0] );
		return false;
	}

	NovaPing np;
	if(! np.getAuthToken()) {
		string error;
		formatstr( error, "Unable to obtain valid authorization token: '%s' (%s).",
			np.errorMessage.c_str(), np.errorCode.c_str() );
		resultLine = create_failure_result( requestID, error.c_str() );
		return false;
	}

	// Look for the requested region's endpoint.
	if(! np.getNovaEndpointForRegion( argv[2] )) {
		string error;
		formatstr( error, "Unable to locate Nova endpoint for requested region: '%s' (%s).",
			np.errorMessage.c_str(), np.errorCode.c_str() );
		resultLine = create_failure_result( requestID, error.c_str() );
		return false;
	}

	// We could grab the version numbers, but the URLs we're getting back
	// include both the version number and the tenant ID already, so let's
	// not be clever.  Instead, just grab the image list.
	np.serviceURL += "/images";
	np.requestMethod = "GET";
	np.requestBody = "";
	if(! np.sendRequest()) {
		string error;
		formatstr( error, "Unable to send ping: '%s' (%s).",
			np.errorMessage.c_str(), np.errorCode.c_str() );
		resultLine = create_failure_result( requestID, error.c_str() );
		return false;
	}

	if( np.responseCode != 200 && np.responseCode != 203 ) {
		formatstr( np.errorCode, "E_HTTP_RESPONSE_NEITHER_200_NOR_203 (%lu)", np.responseCode );
		np.errorMessage = np.responseString;
		if( np.errorMessage.empty() ){
			formatstr( np.errorMessage,
				"HTTP response was %lu, not 200 nor 203, and no error message was returned.",
				np.responseCode );
		}

		string error;
		formatstr( error, "Ping failed: '%s' (%s).",
			np.errorMessage.c_str(), np.errorCode.c_str() );
		dprintf( D_FULLDEBUG, "%s\n", error.c_str() );
		resultLine = create_failure_result( requestID, error.c_str() );
		return false;
	}

	resultLine = create_success_result( requestID, NULL );
	return true;
}

// ----------------------------------------------------------------------------

NovaServerCreate::NovaServerCreate() { }
NovaServerCreate::~NovaServerCreate() { }

// NOVA_SERVER_CREATE <request-id> <region> <desc>
bool NovaServerCreate::workerFunction( char ** argv, int argc, string & resultLine ) {
	assert( strcasecmp( argv[0], "NOVA_SERVER_CREATE" ) == 0 );

	int requestID;
	get_int( argv[1], & requestID );

	if( ! verify_number_args( argc, 4 ) ) {
		dprintf( D_ALWAYS, "Wrong number of arguments (%d should be >= %d) to %s\n", argc, 4, argv[0] );
		return false;
	}

	NovaServerCreate nsc;
	if(! nsc.getAuthToken()) {
		string error;
		formatstr( error, "Unable to obtain valid authorization token: '%s' (%s).",
			nsc.errorMessage.c_str(), nsc.errorCode.c_str() );
		resultLine = create_failure_result( requestID, error.c_str() );
		return false;
	}

	// Look for the requested region's endpoint.
	if(! nsc.getNovaEndpointForRegion( argv[2] )) {
		string error;
		formatstr( error, "Unable to locate Nova endpoint for requested region: '%s' (%s).",
			nsc.errorMessage.c_str(), nsc.errorCode.c_str() );
		resultLine = create_failure_result( requestID, error.c_str() );
		return false;
	}

	nsc.serviceURL += "/servers";
	nsc.requestMethod = "POST";
	nsc.requestBody = argv[3];

	if(! nsc.sendRequest()) {
		string error;
		formatstr( error, "Failed to request server creation: '%s' (%s).",
			nsc.errorMessage.c_str(), nsc.errorCode.c_str() );
		resultLine = create_failure_result( requestID, error.c_str() );
		return false;
	}

	if( nsc.responseCode != 202 ) {
		formatstr( nsc.errorCode, "E_HTTP_RESPONSE_NOT_202 (%lu)", nsc.responseCode );
		nsc.errorMessage = nsc.responseString;
		if( nsc.errorMessage.empty() ){
			formatstr( nsc.errorMessage,
				"HTTP response was %lu, not 202, and no error message was returned.",
				nsc.responseCode );
		}

		string error;
		formatstr( error, "Server creation request failed: '%s' (%s).",
			nsc.errorMessage.c_str(), nsc.errorCode.c_str() );
		dprintf( D_FULLDEBUG, "%s\n", error.c_str() );
		resultLine = create_failure_result( requestID, error.c_str() );
		return false;
	}

	rapidjson::Document d;
	string & body = nsc.responseString;
	d.Parse( body.c_str() );
	if( d.HasParseError() ) {
		nsc.errorCode = "E_NOT_JSON";
		formatstr(	nsc.errorMessage,
					"Failed to parse response body ('%s').",
					body.c_str() );
		resultLine = create_failure_result( requestID, nsc.errorMessage.c_str(), nsc.errorCode.c_str() );
		dprintf( D_FULLDEBUG, "%s\n", nsc.errorMessage.c_str() );
		return false;
	}

	rapidjson::Value::MemberIterator server = d.FindMember( "server" );
	if( server == d.MemberEnd() ) {
		nsc.errorCode = "E_NO_MEMBER";
		formatstr(	nsc.errorMessage,
					"Failed to find server in body ('%s').",
					body.c_str() );
		resultLine = create_failure_result( requestID, nsc.errorMessage.c_str(), nsc.errorCode.c_str() );
		dprintf( D_FULLDEBUG, "%s\n", nsc.errorMessage.c_str() );
		return false;
	}

	if(! server->value.IsObject() ) {
		nsc.errorCode = "E_NOT_OBJECT";
		formatstr(	nsc.errorMessage,
					"Server attribute is not a JSON object in body ('%s').",
					body.c_str() );
		resultLine = create_failure_result( requestID, nsc.errorMessage.c_str(), nsc.errorCode.c_str() );
		dprintf( D_FULLDEBUG, "%s\n", nsc.errorMessage.c_str() );
		return false;
	}

	rapidjson::Value::MemberIterator serverID = server->value.FindMember( "id" );
	if( serverID == server->value.MemberEnd() ) {
		nsc.errorCode = "E_NO_MEMBER";
		formatstr(	nsc.errorMessage,
					"Failed to find id in server object in body ('%s').",
					body.c_str() );
		resultLine = create_failure_result( requestID, nsc.errorMessage.c_str(), nsc.errorCode.c_str() );
		dprintf( D_FULLDEBUG, "%s\n", nsc.errorMessage.c_str() );
		return false;
	}

	if(! serverID->value.IsString() ) {
		nsc.errorCode = "E_NOT_STRING";
		formatstr(	nsc.errorMessage,
					"Server object's ID attribute is not a JSON string in body ('%s').",
					body.c_str() );
		resultLine = create_failure_result( requestID, nsc.errorMessage.c_str(), nsc.errorCode.c_str() );
		dprintf( D_FULLDEBUG, "%s\n", nsc.errorMessage.c_str() );
		return false;
	}

	StringList resultList;
	resultList.append( serverID->value.GetString() );
	resultLine = create_success_result( requestID, & resultList );
	return true;
}

// ----------------------------------------------------------------------------

NovaServerDelete::NovaServerDelete() { }
NovaServerDelete::~NovaServerDelete() { }

// NOVA_SERVER_DELETE <request-id> <region> <server-id>
bool NovaServerDelete::workerFunction( char ** argv, int argc, string & resultLine ) {
	assert( strcasecmp( argv[0], "NOVA_SERVER_DELETE" ) == 0 );

	int requestID;
	get_int( argv[1], & requestID );

	if( ! verify_number_args( argc, 4 ) ) {
		dprintf( D_ALWAYS, "Wrong number of arguments (%d should be >= %d) to %s\n", argc, 4, argv[0] );
		return false;
	}

	NovaServerDelete nsd;
	if(! nsd.getAuthToken()) {
		string error;
		formatstr( error, "Unable to obtain valid authorization token: '%s' (%s).",
			nsd.errorMessage.c_str(), nsd.errorCode.c_str() );
		resultLine = create_failure_result( requestID, error.c_str() );
		return false;
	}

	// Look for the requested region's endpoint.
	if(! nsd.getNovaEndpointForRegion( argv[2] )) {
		string error;
		formatstr( error, "Unable to locate Nova endpoint for requested region: '%s' (%s).",
			nsd.errorMessage.c_str(), nsd.errorCode.c_str() );
		resultLine = create_failure_result( requestID, error.c_str() );
		return false;
	}

	string serverID = argv[3];
	nsd.serviceURL += "/servers/" + serverID;
	nsd.requestMethod = "DELETE";
	nsd.requestBody = "";

	if(! nsd.sendRequest()) {
		string error;
		formatstr( error, "Failed to request server deletion: '%s' (%s).",
			nsd.errorMessage.c_str(), nsd.errorCode.c_str() );
		resultLine = create_failure_result( requestID, error.c_str() );
		return false;
	}

	if( nsd.responseCode != 204 ) {
		formatstr( nsd.errorCode, "E_HTTP_RESPONSE_NOT_204 (%lu)", nsd.responseCode );
		nsd.errorMessage = nsd.responseString;
		if( nsd.errorMessage.empty() ){
			formatstr( nsd.errorMessage,
				"HTTP response was %lu, not 204, and no error message was returned.",
				nsd.responseCode );
		}

		string error;
		formatstr( error, "Server list failed: '%s' (%s).",
			nsd.errorMessage.c_str(), nsd.errorCode.c_str() );
		dprintf( D_FULLDEBUG, "%s\n", error.c_str() );
		resultLine = create_failure_result( requestID, error.c_str() );
		return false;
	}

	resultLine = create_success_result( requestID, NULL );
	return true;
}

// ----------------------------------------------------------------------------

NovaServerList::NovaServerList() { }
NovaServerList::~NovaServerList() { }

// NOVA_SERVER_LIST <request-id> <region> <filter>
bool NovaServerList::workerFunction( char ** argv, int argc, string & resultLine ) {
	assert( strcasecmp( argv[0], "NOVA_SERVER_LIST" ) == 0 );

	int requestID;
	get_int( argv[1], & requestID );

	if( ! verify_number_args( argc, 4 ) ) {
		dprintf( D_ALWAYS, "Wrong number of arguments (%d should be >= %d) to %s\n", argc, 4, argv[0] );
		return false;
	}

	NovaServerList nsl;
	if(! nsl.getAuthToken()) {
		string error;
		formatstr( error, "Unable to obtain valid authorization token: '%s' (%s).",
			nsl.errorMessage.c_str(), nsl.errorCode.c_str() );
		resultLine = create_failure_result( requestID, error.c_str() );
		return false;
	}

	// Look for the requested region's endpoint.
	if(! nsl.getNovaEndpointForRegion( argv[2] )) {
		string error;
		formatstr( error, "Unable to locate Nova endpoint for requested region: '%s' (%s).",
			nsl.errorMessage.c_str(), nsl.errorCode.c_str() );
		resultLine = create_failure_result( requestID, error.c_str() );
		return false;
	}

	nsl.serviceURL += "/servers";
	nsl.requestMethod = "GET";
	nsl.requestBody = "";

	string filter = emptyIfNullString( argv[3] );
	if(! filter.empty() ) {
		nsl.serviceURL += "?" + filter;
	}

	if(! nsl.sendRequest()) {
		string error;
		formatstr( error, "Failed to request server list: '%s' (%s).",
			nsl.errorMessage.c_str(), nsl.errorCode.c_str() );
		resultLine = create_failure_result( requestID, error.c_str() );
		return false;
	}

	if( nsl.responseCode != 200 && nsl.responseCode != 203 ) {
		formatstr( nsl.errorCode, "E_HTTP_RESPONSE_NEITHER_200_NOR_203 (%lu)", nsl.responseCode );
		nsl.errorMessage = nsl.responseString;
		if( nsl.errorMessage.empty() ){
			formatstr( nsl.errorMessage,
				"HTTP response was %lu, not 200 nor 203, and no error message was returned.",
				nsl.responseCode );
		}

		string error;
		formatstr( error, "Server list failed: '%s' (%s).",
			nsl.errorMessage.c_str(), nsl.errorCode.c_str() );
		dprintf( D_FULLDEBUG, "%s\n", error.c_str() );
		resultLine = create_failure_result( requestID, error.c_str() );
		return false;
	}

	rapidjson::Document d;
	string & body = nsl.responseString;
	d.Parse( body.c_str() );
	if( d.HasParseError() ) {
		nsl.errorCode = "E_NOT_JSON";
		formatstr(	nsl.errorMessage,
					"Failed to parse response body ('%s').",
					body.c_str() );
		resultLine = create_failure_result( requestID, nsl.errorMessage.c_str(), nsl.errorCode.c_str() );
		return false;
	}

	rapidjson::Value::MemberIterator serverList = d.FindMember( "servers" );
	if( serverList == d.MemberEnd() ) {
		nsl.errorCode = "E_NO_MEMBER";
		formatstr(	nsl.errorMessage,
					"Failed to find server list in body ('%s').",
					body.c_str() );
		resultLine = create_failure_result( requestID, nsl.errorMessage.c_str(), nsl.errorCode.c_str() );
		return false;
	}

	if(! serverList->value.IsArray()) {
		nsl.errorCode = "E_NOT_ARRAY";
		formatstr(	nsl.errorMessage,
					"Servers attribute is not a JSON array in body ('%s').",
					body.c_str() );
		resultLine = create_failure_result( requestID, nsl.errorMessage.c_str(), nsl.errorCode.c_str() );
		return false;
	}

	StringList reply;
	// If we ever care, we can replace the continues with warnings.
	for( rapidjson::SizeType i = 0; i < serverList->value.Size(); ++i ) {
		const rapidjson::Value & server = serverList->value[i];

		rapidjson::Value::ConstMemberIterator serverID = server.FindMember( "id" );
		if( serverID == server.MemberEnd() ) { continue; }
		if( ! serverID->value.IsString() ) { continue; }

		rapidjson::Value::ConstMemberIterator serverName = server.FindMember( "name" );
		if( serverName == server.MemberEnd() ) { continue; }
		if( ! serverName->value.IsString() ) { continue; }

		// Only create a reply-list entry if we've got both halves.
		reply.append( serverID->value.GetString() );
		reply.append( serverName->value.GetString() );
	}

	string count;
	formatstr( count, "%d", reply.number() / 2 );
	reply.first();
	reply.insert( count.c_str() );

	resultLine = create_success_result( requestID, & reply );
	return true;
}

// ----------------------------------------------------------------------------

NovaServerListDetail::NovaServerListDetail() { }
NovaServerListDetail::~NovaServerListDetail() { }

// NOVA_SERVER_LIST_DETAIL <request-id> <region> <filter>
bool NovaServerListDetail::workerFunction( char ** argv, int argc, string & resultLine ) {
	assert( strcasecmp( argv[0], "NOVA_SERVER_LIST_DETAIL" ) == 0 );

	int requestID;
	get_int( argv[1], & requestID );

	if( ! verify_number_args( argc, 4 ) ) {
		dprintf( D_ALWAYS, "Wrong number of arguments (%d should be >= %d) to %s\n", argc, 4, argv[0] );
		return false;
	}

	NovaServerListDetail nsld;
	if(! nsld.getAuthToken()) {
		string error;
		formatstr( error, "Unable to obtain valid authorization token: '%s' (%s).",
			nsld.errorMessage.c_str(), nsld.errorCode.c_str() );
		resultLine = create_failure_result( requestID, error.c_str() );
		return false;
	}

	// Look for the requested region's endpoint.
	if(! nsld.getNovaEndpointForRegion( argv[2] )) {
		string error;
		formatstr( error, "Unable to locate Nova endpoint for requested region: '%s' (%s).",
			nsld.errorMessage.c_str(), nsld.errorCode.c_str() );
		resultLine = create_failure_result( requestID, error.c_str() );
		return false;
	}

	nsld.serviceURL += "/servers/detail";
	nsld.requestMethod = "GET";
	nsld.requestBody = "";

	string filter = emptyIfNullString( argv[3] );
	if(! filter.empty() ) {
		nsld.serviceURL += "?" + filter;
	}

	if(! nsld.sendRequest()) {
		string error;
		formatstr( error, "Failed to request server list: '%s' (%s).",
			nsld.errorMessage.c_str(), nsld.errorCode.c_str() );
		resultLine = create_failure_result( requestID, error.c_str() );
		return false;
	}

	if( nsld.responseCode != 200 && nsld.responseCode != 203 ) {
		formatstr( nsld.errorCode, "E_HTTP_RESPONSE_NEITHER_200_NOR_203 (%lu)", nsld.responseCode );
		nsld.errorMessage = nsld.responseString;
		if( nsld.errorMessage.empty() ){
			formatstr( nsld.errorMessage,
				"HTTP response was %lu, not 200 nor 203, and no error message was returned.",
				nsld.responseCode );
		}

		string error;
		formatstr( error, "Server list failed: '%s' (%s).",
			nsld.errorMessage.c_str(), nsld.errorCode.c_str() );
		dprintf( D_FULLDEBUG, "%s\n", error.c_str() );
		resultLine = create_failure_result( requestID, error.c_str() );
		return false;
	}

	rapidjson::Document d;
	string & body = nsld.responseString;
	d.Parse( body.c_str() );
	if( d.HasParseError() ) {
		nsld.errorCode = "E_NOT_JSON";
		formatstr(	nsld.errorMessage,
					"Failed to parse response body ('%s').",
					body.c_str() );
		resultLine = create_failure_result( requestID, nsld.errorMessage.c_str(), nsld.errorCode.c_str() );
		return false;
	}

	rapidjson::Value::MemberIterator serverList = d.FindMember( "servers" );
	if( serverList == d.MemberEnd() ) {
		nsld.errorCode = "E_NO_MEMBER";
		formatstr(	nsld.errorMessage,
					"Failed to find server list in body ('%s').",
					body.c_str() );
		resultLine = create_failure_result( requestID, nsld.errorMessage.c_str(), nsld.errorCode.c_str() );
		return false;
	}

	if(! serverList->value.IsArray()) {
		nsld.errorCode = "E_NOT_ARRAY";
		formatstr(	nsld.errorMessage,
					"Servers attribute not a JSON array in body ('%s').",
					body.c_str() );
		resultLine = create_failure_result( requestID, nsld.errorMessage.c_str(), nsld.errorCode.c_str() );
		return false;
	}

	StringList reply;
	// If we ever care, we can replace the continues with warnings.
	for( rapidjson::SizeType i = 0; i < serverList->value.Size(); ++i ) {
		const rapidjson::Value & server = serverList->value[i];

		rapidjson::Value::ConstMemberIterator serverID = server.FindMember( "id" );
		if( serverID == server.MemberEnd() ) { continue; }
		if( ! serverID->value.IsString() ) { continue; }

		rapidjson::Value::ConstMemberIterator serverName = server.FindMember( "name" );
		if( serverName == server.MemberEnd() ) { continue; }
		if( ! serverName->value.IsString() ) { continue; }

		rapidjson::Value::ConstMemberIterator serverStatus = server.FindMember( "status" );
		if( serverStatus == server.MemberEnd() ) { continue; }
		if( ! serverStatus->value.IsString() ) { continue; }

		// The document gives an example with "accessIPv4" and "accessIPv6"
		// set to empty values, and "addresses" an object with one attribute,
		// "private", which is a list of objects; the sole entry has the "addr"
		// and "version" properties.  Our goal is to update the job ad with
		// an IP address for use with SSH.  For now, until we get more data,
		// just send the grid manager the JSON string for the addresses object.
		rapidjson::Value::ConstMemberIterator serverAddresses = server.FindMember( "addresses" );
		if( serverAddresses == server.MemberEnd() ) { continue; }
		if( ! serverAddresses->value.IsObject() ) { continue; }

		rapidjson::StringBuffer sb;
		rapidjson::Writer< rapidjson::StringBuffer > w( sb );
		serverAddresses->value.Accept( w );

		// Only create a reply-list entry if we've got every value.
		reply.append( serverID->value.GetString() );
		reply.append( serverName->value.GetString() );
		reply.append( serverStatus->value.GetString() );
		reply.append( sb.GetString() );
	}

	string count;
	formatstr( count, "%d", reply.number() / 4 );
	reply.first();
	reply.insert( count.c_str() );

	resultLine = create_success_result( requestID, & reply );
	return true;
}
