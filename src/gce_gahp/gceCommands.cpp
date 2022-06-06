/***************************************************************
 *
 * Copyright (C) 1990-2014, Condor Team, Computer Sciences Department,
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
#include "string_list.h"
#include "gcegahp_common.h"
#include "gceCommands.h"
#include "shortfile.h"

#include "condor_base64.h"
#include <sstream>
#include "stat_wrapper.h"
#include <curl/curl.h>
#include "thread_control.h"
#include <sqlite3.h>

using std::string;
using std::map;
using std::vector;

#define NULLSTRING "NULL"

const char * nullStringIfEmpty( const string & str ) {
	if( str.empty() ) { return NULLSTRING; }
	else { return str.c_str(); }
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

const char *escapeJSONString( const char *value )
{
	static string result;
	result.clear();

	while( *value ) {
		if ( *value == '"' || *value == '\\' ) {
			result += '\\';
		}
		result += *value;
		value++;
	}

	return result.c_str();
}

// Utility function for parsing the metadata file given by the user.
// We expect to see one key/value pair per line, key and value separated
// by '=' or ':', and no extraneous whitespace.
bool ParseMetadataLine( const char *&input, string &key, string &value,
						char delim )
{
	const char *ptr = NULL;
	bool in_key = true;
	key.clear();
	value.clear();

	for ( ptr = input; *ptr; ptr++ ) {
		if ( in_key ) {
			if ( *ptr == '=' || *ptr == ':' ) {
				in_key = false;
			} else {
				key += *ptr;
			}
		} else if ( *ptr == delim ) {
			ptr++;
			input = ptr;
			return true;
		} else {
			value += *ptr;
		}
	}

	input = ptr;
	return !key.empty() && !value.empty();
}

// From the body of a failure reply from the server, extract the best
// human-readable error message.
void ExtractErrorMessage( const string &response, string &err_msg )
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
}

// Utility function meant for GceInstanceList
void AddInstanceToResult( vector<string> &result, string &id,
						  string &name, string &status,
						  string &status_msg )
{
	result.push_back( id );
	result.push_back( name );
	result.push_back( status );
	if ( status_msg.empty() ) {
		result.emplace_back( "NULL" );
	} else {
		result.push_back( status_msg );
	}
	id.clear();
	name.clear();
	status.clear();
	status_msg.clear();
}


struct AuthInfo {
	ClassAd m_json_data;
	string m_auth_file;
	string m_account;
	string m_access_token;
	string m_client_id;
	string m_client_secret;
	string m_refresh_token;
	time_t m_expiration;
	time_t m_last_attempt;
	pthread_cond_t m_cond;
	string m_err_msg;
	bool m_refreshing;

	AuthInfo() : m_expiration(0), m_last_attempt(0), m_refreshing(false)
	{ pthread_cond_init( &m_cond, NULL ); }

	~AuthInfo() { pthread_cond_destroy( &m_cond ); }
};

// A table of Google OAuth 2.0 credentials, keyed on the filename the
// credentials were read from and which account the credentials are for.
map<string, AuthInfo> authTable;

// Obtain an access token from Google's OAuth 2.0 service using the
// provided credentials (refresh token and client secret).
bool TryAuthRefresh( AuthInfo &auth_info )
{
	// Fill in required attributes & parameters.
	GceRequest request;
	request.serviceURL = "https://accounts.google.com/o/oauth2/token";
	request.requestMethod = "POST";
	request.contentType = "application/x-www-form-urlencoded";

	// TODO Do we need any newlines in this body?
	request.requestBody = "client_id=";
	request.requestBody += auth_info.m_client_id;
	request.requestBody += "&client_secret=";
	request.requestBody += auth_info.m_client_secret;
	request.requestBody += "&refresh_token=";
	request.requestBody += auth_info.m_refresh_token;
	request.requestBody += "&grant_type=refresh_token";

	// Send the request.
	if( ! request.SendRequest() ) {
		auth_info.m_err_msg = request.errorMessage;
		return false;
	}

	string key;
	string value;
	int nesting = 0;
	string my_access_token;
	int my_expires_in = 0;

	const char *pos = request.resultString.c_str();
	while( ParseJSONLine( pos, key, value, nesting ) ) {
		if ( key == "access_token" ) {
			my_access_token = value;
		}
		if ( key == "expires_in" ) {
			my_expires_in = atoi( value.c_str() );
		}
	}

	if ( my_access_token.empty() ) {
		auth_info.m_err_msg = "No access_token in server response";
		return false;
	}
	if ( my_expires_in == 0 ) {
		auth_info.m_err_msg = "No expiration in server_response";
		return false;
	}

	auth_info.m_access_token = my_access_token;
	auth_info.m_expiration = my_expires_in + time(NULL);

	return true;
}

bool ReadCredData( AuthInfo &auth_info, const classad::ClassAd &cred_ad )
{
	string access_token;
	string refresh_token;
	string client_id;
	string client_secret;
	string token_expiry;
	time_t expiration = 0;
	cred_ad.EvaluateAttrString("access_token", access_token);
	cred_ad.EvaluateAttrString("refresh_token", refresh_token);
	cred_ad.EvaluateAttrString("client_id", client_id);
	cred_ad.EvaluateAttrString("client_secret", client_secret);
	cred_ad.EvaluateAttrString("token_expiry", token_expiry);
	if ( ! token_expiry.empty() ) {
		struct tm expiration_tm;
		if ( strptime(token_expiry.c_str(), "%Y-%m-%dT%TZ", &expiration_tm ) != NULL) {
			expiration = timegm(&expiration_tm);
		} else if ( strptime(token_expiry.c_str(), "%Y-%m-%d %T", &expiration_tm ) != NULL ) {
			expiration = timegm(&expiration_tm);
		}
	}

	if ( refresh_token.empty() ) {
		// This is a service credential
		if ( access_token.empty() ) {
			auth_info.m_err_msg = "Failed to find refresh_token or access_token in credentials file";
			return false;
		}
		if ( expiration == 0 ) {
			auth_info.m_err_msg = "Failed to parse expiration: " + token_expiry;
			return false;
		}
		if ( expiration < time(NULL) ) {
			auth_info.m_err_msg = "Access_token is expired, please re-login";
			return false;
		}
		auth_info.m_access_token = access_token;
		auth_info.m_expiration = expiration;
	} else {
		// This is a user credential
		if ( client_id.empty() ) {
			auth_info.m_err_msg = "Failed to find client_id in credentials file";
			return false;
		}
		if ( client_secret.empty() ) {
			auth_info.m_err_msg = "Failed to find client_secret in credentials file";
			return false;
		}
		auth_info.m_refresh_token = refresh_token;
		auth_info.m_client_id = client_id;
		auth_info.m_client_secret = client_secret;
	}
	return true;
}

int SqliteCredFileCB( void *auth_ptr, int argc, char **argv, char **col_name )
{
	AuthInfo *auth_info = (AuthInfo *)auth_ptr;
	if ( argc < 2 || strcmp( col_name[1], "value" ) ) {
		auth_info->m_err_msg = "Bad schema in credentials DB";
		return 0;
	}
	classad::ClassAdJsonParser parser;
	if ( ! parser.ParseClassAd(argv[1], auth_info->m_json_data, true) ) {
		auth_info->m_err_msg = "Invalid JSON data";
		return 0;
	}
	// If the user didn't specify an account, remember which one we got.
	// This is important if we need to look in the access_tokens db.
	if ( auth_info->m_account.empty() ) {
		auth_info->m_account = argv[0];
	}
	// Clear the not-found error message set before the query.
	auth_info->m_err_msg.clear();
	return 0;
}

int SqliteAccessFileCB( void *auth_ptr, int argc, char **argv, char **col_name )
{
	AuthInfo *auth_info = (AuthInfo *)auth_ptr;
	if ( argc < 4 || strcmp( col_name[1], "access_token" ) ||
		 strcmp( col_name[2], "token_expiry" ) ) {
		auth_info->m_err_msg = "Bad schema in access_token DB";
		return 0;
	}
	// Clear the not-found error message set before the query.
	auth_info->m_err_msg.clear();

	auth_info->m_json_data.Assign( "access_token", argv[1] );
	auth_info->m_json_data.Assign( "token_expiry", argv[2] );
	return 0;
}

// These are return values for ReadCredFileSqlite() and ReadCredFileJson().
// CRED_FILE_SUCCESS: The requested credentials were successfully read.
// CRED_FILE_FAILURE: The requested credentials could not be obtained.
// CRED_FILE_BAD_FORMAT: The file was not the correct format. The caller
//    could retry reading in a different format.
#define CRED_FILE_SUCCESS       0
#define CRED_FILE_FAILURE       1
#define CRED_FILE_BAD_FORMAT    2

// Read a gcloud credentials file in the new SQL format and extract
// credentials for the given account. If no account is specified, one
// is picked arbitrarily.
int ReadCredFileSqlite( AuthInfo &auth_info )
{
	sqlite3 *db;
	char *db_err_msg;
	int rc;
	char *query_stmt;

	auth_info.m_json_data.Clear();
	if ( auth_info.m_account.empty() ) {
		rc = asprintf(&query_stmt, "select * from credentials limit 1;");
	} else {
		rc = asprintf(&query_stmt, "select * from credentials where account_id = \"%s\";", auth_info.m_account.c_str());
	}
	assert(rc >= 0);

	rc = sqlite3_open_v2(auth_info.m_auth_file.c_str(), &db, SQLITE_OPEN_READONLY, NULL);
	if ( rc ) {
		formatstr(auth_info.m_err_msg, "Failed to open credentials file: %s",
		          sqlite3_errmsg(db));
		sqlite3_close(db);
		free(query_stmt);
		return CRED_FILE_FAILURE;
	}
	auth_info.m_err_msg = "Account not found in credentials db";
	rc = sqlite3_exec(db, query_stmt, SqliteCredFileCB, &auth_info, &db_err_msg);
	free(query_stmt);
	query_stmt = NULL;
	if ( rc != SQLITE_OK ) {
		formatstr(auth_info.m_err_msg, "Failed to query credentials db: %s",
		          db_err_msg);
		sqlite3_free(db_err_msg);
		sqlite3_close(db);
		return (rc == SQLITE_NOTADB) ? CRED_FILE_BAD_FORMAT : CRED_FILE_FAILURE;
	}
	sqlite3_close(db);

	if ( ! auth_info.m_err_msg.empty() ) {
		return CRED_FILE_FAILURE;
	}
	if ( ! auth_info.m_refresh_token.empty() ) {
		return CRED_FILE_SUCCESS;
	}
	// If there's no refresh token, then we're dealing with a service
	// account. We need to read the accompanying access_tokens.db file.
	if ( auth_info.m_account.empty() ) {
		rc = asprintf(&query_stmt, "select * from access_tokens limit 1;");
	} else {
		rc = asprintf(&query_stmt, "select * from access_tokens where account_id = \"%s\";", auth_info.m_account.c_str());
	}
	assert(rc >= 0);

	string access_tokens_file;
	size_t slash_pos = auth_info.m_auth_file.find_last_of('/');
	if ( slash_pos != string::npos ) {
		access_tokens_file = auth_info.m_auth_file.substr( 0, slash_pos );
		access_tokens_file += "/";
	}
	access_tokens_file += "access_tokens.db";
	rc = sqlite3_open_v2(access_tokens_file.c_str(), &db, SQLITE_OPEN_READONLY, NULL);
	if ( rc ) {
		formatstr(auth_info.m_err_msg, "Failed to open access_tokens file: %s",
		          sqlite3_errmsg(db));
		sqlite3_close(db);
		free(query_stmt);
		return CRED_FILE_FAILURE;
	}
	auth_info.m_err_msg = "Account not found in access_tokens db";
	rc = sqlite3_exec(db, query_stmt, SqliteAccessFileCB, &auth_info, &db_err_msg);
	free(query_stmt);
	query_stmt = NULL;
	if ( rc != SQLITE_OK ) {
		formatstr(auth_info.m_err_msg, "Failed to query access_tokens db: %s",
		          db_err_msg);
		sqlite3_free(db_err_msg);
		sqlite3_close(db);
		return CRED_FILE_FAILURE;
	}
	sqlite3_close(db);
	if ( ! auth_info.m_err_msg.empty() ) {
		return CRED_FILE_FAILURE;
	}

	ReadCredData(auth_info, auth_info.m_json_data);
	if ( ! auth_info.m_err_msg.empty() ) {
		return CRED_FILE_FAILURE;
	}
	return CRED_FILE_SUCCESS;
}

// Read a gcloud credentials file in the old JSON format and extract
// credentials for the given account. If no account is specified, one
// is picked arbitrarily.
int ReadCredFileJson( AuthInfo &auth_info )
{
	auth_info.m_err_msg.clear();
	auth_info.m_json_data.Clear();
	FILE *fp = safe_fopen_wrapper_follow(auth_info.m_auth_file.c_str(), "r");
	if ( fp == NULL ) {
		auth_info.m_err_msg = "Failed to open credentials file";
		return CRED_FILE_FAILURE;
	}
	classad::ClassAdJsonParser parser;
	if ( ! parser.ParseClassAd(fp, auth_info.m_json_data, true) ) {
		auth_info.m_err_msg = "Invalid JSON data";
		fclose(fp);
		return CRED_FILE_BAD_FORMAT;
	}
	fclose(fp);
	classad::ExprTree *tree;
	tree = auth_info.m_json_data.Lookup("data");
	if ( tree == NULL || tree->GetKind() != classad::ExprTree::EXPR_LIST_NODE ) {
		auth_info.m_err_msg = "Invalid JSON data";
		return CRED_FILE_FAILURE;
	}
	std::vector<ExprTree*> cred_list;
	((classad::ExprList*)tree)->GetComponents(cred_list);
	for ( std::vector<ExprTree*>::iterator itr = cred_list.begin(); itr != cred_list.end(); itr++ ) {
		if ( (*itr)->GetKind() != classad::ExprTree::CLASSAD_NODE ) {
			continue;
		}
		classad::ClassAd *acct_ad = (classad::ClassAd*)(*itr);
		classad::Value val;
		string account_id;
		if ( !acct_ad->EvaluateExpr("key.account", val) || !val.IsStringValue(account_id) ) {
			continue;
		}
		if ( !auth_info.m_account.empty() ) {
			if ( account_id != auth_info.m_account ) {
				continue;
			}
		}
		classad::ExprTree *cred_expr;
		cred_expr = acct_ad->Lookup("credential");
		if ( cred_expr == NULL || cred_expr->GetKind() != classad::ExprTree::CLASSAD_NODE ) {
			continue;
		}
		if ( ReadCredData(auth_info, *(classad::ClassAd*)cred_expr ) ) {
			auth_info.m_account = account_id;
			return CRED_FILE_SUCCESS;
		}
	}
	if ( auth_info.m_err_msg.empty() ) {
		auth_info.m_err_msg = "Account not found in credentials file";
	}
	return CRED_FILE_FAILURE;
}

bool FindDefaultCredentialsFile( string &cred_file )
{
	struct passwd *pw;
	if ( (pw = getpwuid(getuid())) == NULL ) {
		return false;
	}
	string test_cred_file = pw->pw_dir;
	test_cred_file += "/.config/gcloud/credentials.db";
	if ( access(test_cred_file.c_str(), R_OK) == 0 ) {
		cred_file = test_cred_file;
		return true;
	}
	test_cred_file = pw->pw_dir;
	test_cred_file += "/.config/gcloud/credentials";
	if ( access(test_cred_file.c_str(), R_OK) == 0 ) {
		cred_file = test_cred_file;
		return true;
	}
	return false;
}

// Given a file containing OAuth 2.0 credentials, return an access
// token that can be used to perform GCE requests.
// If true is returned, then result contains the access token.
// If false is returned, then result contains an error message.
// The credentials file is expected to look like that written by
// Google's gcutil command-line tool. Specifically, it should contain
// JSON data that includes the refresh_token, client_id, and
// client_secret values required to obtain an access token from
// Google's OAuth 2.0 service.
// The results are cached (in authTable map), so that the OAuth service
// is only contacted when necessary (the first time we see a file and
// when the access token is about to expire).
bool GetAccessToken( const string &auth_file_in, const string &account_in,
                     string &access_token, string &err_msg)
{
	// If we get an empty or 'NULL' filename, look for the file in the
	// default location used by the gcloud client tools.
	string auth_file;
	if ( !auth_file_in.empty() && strcasecmp( auth_file_in.c_str(), NULLSTRING ) != 0 ) {
		auth_file = auth_file_in;
	} else if ( FindDefaultCredentialsFile( auth_file ) == false ) {
		err_msg = "Failed to find default credentials file";
		return false;
	}

	// If we get an account name of NULL, treat it like an empty string,
	// which means use any account.
	string map_key;
	string account;
	if ( strcasecmp( account_in.c_str(), NULLSTRING ) != 0 ) {
		account = account_in;
	}
	map_key = auth_file + "#" + account;
	AuthInfo &auth_entry = authTable[map_key];

	while ( auth_entry.m_refreshing ) {
		pthread_cond_wait( &auth_entry.m_cond, &global_big_mutex );
	}

	if ( !auth_entry.m_err_msg.empty() &&
		 time(NULL) - auth_entry.m_last_attempt < (5 * 60) ) {
		err_msg = auth_entry.m_err_msg;
		return false;
	}

	if ( auth_entry.m_auth_file.empty() ) {
		auth_entry.m_auth_file = auth_file;
		auth_entry.m_account = account;
	}

	if ( auth_entry.m_expiration < time(NULL) + (5 * 60) ) {
		int rc;
		auth_entry.m_refreshing = true;

		// We either don't have an access token or we have one that's
		// about to expire. Try to get a shiny new one.

		if ( auth_entry.m_err_msg.empty() && ! auth_entry.m_refresh_token.empty() ) {
			if ( TryAuthRefresh( auth_entry ) ) {
				goto done;
			}
		}

		// Refresh failed or not possible (no refresh token).
		// Try to read our credentials from the file(s).
		auth_entry.m_err_msg.clear();

		// First try reading the credentials file in the new sql format.
		// If it doesn't look like an sql file, try again using the old
		// json text format.
		rc = ReadCredFileSqlite( auth_entry );
		if ( rc == CRED_FILE_BAD_FORMAT ) {
			rc = ReadCredFileJson( auth_entry );
		}
		if ( rc != CRED_FILE_SUCCESS ) {
			// auth_entry.m_err_msg set by the read functions on any error
			goto done;
		}

		if ( auth_entry.m_refresh_token.empty() ) {
			// This is a service credential, should already have an access token
			goto done;
		}

		TryAuthRefresh( auth_entry );

	done:
		auth_entry.m_last_attempt = time(NULL);
		auth_entry.m_refreshing = false;
		pthread_cond_broadcast( &auth_entry.m_cond );
	}

	if ( !auth_entry.m_err_msg.empty() ) {
		err_msg = auth_entry.m_err_msg;
		return false;
	} else {
		ASSERT( !auth_entry.m_access_token.empty() );
		access_token = auth_entry.m_access_token;
		return true;
	}
}

// Repeatedly calls GCP for operation status until the operation
// is completed. On failure, stores an appropriate message in
// err_msg and returns false. On success, stores targetId value in
// instance_id (if non-NULL) and returns true.
bool verifyRequest( const string &serviceUrl, const string &auth_file,
                    const string &account, string &err_msg,
                    string *instance_id = NULL )
{
	string status;
	bool in_err = false;
	do {

		// Give the operation some time to complete.
		// TODO Is there a better way to do this?
		gce_gahp_release_big_mutex();
		sleep( 5 );
		gce_gahp_grab_big_mutex();

		GceRequest op_request;
		op_request.serviceURL = serviceUrl;
		op_request.requestMethod = "GET";

		if ( !GetAccessToken( auth_file, account, op_request.accessToken, err_msg ) ) {
			return false;
		}

		if ( !op_request.SendRequest() ) {
			err_msg = op_request.errorMessage;
			return false;
		}

		int nesting = 0;
		string key;
		string value;
		const char *pos = op_request.resultString.c_str();
		while ( ParseJSONLine( pos, key, value, nesting ) ) {
			if ( key == "status" ) {
				status = value;
			} else if ( key == "error" ) {
				in_err = true;
			} else if ( key == "warnings" ) {
				in_err = false;
			} else if ( key == "message" && in_err ) {
				err_msg = value;
			} else if ( key == "targetId" && instance_id ) {
				*instance_id = value;
			}
		}
	} while ( status != "DONE" );

	if ( !err_msg.empty() ) {
		return false;
	}

	return true;
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

GceRequest::GceRequest()
{
	includeResponseHeader = false;
	contentType = "application/json";
}

GceRequest::~GceRequest() { }

pthread_mutex_t globalCurlMutex = PTHREAD_MUTEX_INITIALIZER;
bool GceRequest::SendRequest() 
{
	struct  curl_slist *curl_headers = NULL;
	string buf;
	unsigned long responseCode = 0;
	char *ca_dir = NULL;
	char *ca_file = NULL;

	// Generate the final URI.
	// TODO Eliminate this copy if we always use the serviceURL unmodified
	string finalURI = this->serviceURL;
	dprintf( D_FULLDEBUG, "Request URI is '%s'\n", finalURI.c_str() );
	if ( requestMethod == "POST" ) {
		dprintf( D_FULLDEBUG, "Request body is '%s'\n", requestBody.c_str() );
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
		
		rv = curl_easy_setopt( curl, CURLOPT_POSTFIELDS, requestBody.c_str() );
		if( rv != CURLE_OK ) {
			this->errorCode = "E_CURL_LIB";
			this->errorMessage = "curl_easy_setopt( CURLOPT_POSTFIELDS ) failed.";
			dprintf( D_ALWAYS, "curl_easy_setopt( CURLOPT_POSTFIELDS ) failed (%d): '%s', failing.\n",
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

	if ( !accessToken.empty() ) {
		buf = "Authorization: Bearer ";
		buf += accessToken;
		curl_headers = curl_slist_append( curl_headers, buf.c_str() );
		if ( curl_headers == NULL ) {
			this->errorCode = "E_CURL_LIB";
			this->errorMessage = "curl_slist_append() failed.";
			dprintf( D_ALWAYS, "curl_slist_append() failed, failing.\n" );
			goto error_return;
		}
	}

	buf = "Content-Type: ";
	buf += contentType;
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
		ca_dir = getenv( "GAHP_SSL_CADIR" );
	}

	ca_file = getenv( "X509_CERT_FILE" );
	if ( ca_file == NULL ) {
		ca_file = getenv( "GAHP_SSL_CAFILE" );
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

// Expecting:GCE_PING <req_id> <serviceurl> <authfile> <account> <project> <zone>
bool GcePing::workerFunction(char **argv, int argc, string &result_string) {
	assert( strcasecmp( argv[0], "GCE_PING" ) == 0 );

	int requestID;
	get_int( argv[1], & requestID );

	if( ! verify_number_args( argc, 7 ) ) {
		result_string = create_failure_result( requestID, "Wrong_Argument_Number" );
		dprintf( D_ALWAYS, "Wrong number of arguments (%d should be >= %d) to %s\n",
				 argc, 7, argv[0] );
		return false;
	}

	// Fill in required attributes & parameters.
	GcePing ping_request;
	ping_request.serviceURL = argv[2];
	ping_request.serviceURL += "/projects/";
	ping_request.serviceURL += argv[5];
	ping_request.serviceURL += "/zones/";
	ping_request.serviceURL += argv[6];
	ping_request.serviceURL += "/instances";
	ping_request.requestMethod = "GET";

	string auth_file = argv[3];
	if ( !GetAccessToken( auth_file, argv[4], ping_request.accessToken,
						  ping_request.errorMessage ) ) {
		result_string = create_failure_result( requestID,
											   ping_request.errorMessage.c_str() );
		return true;
	}

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

// https://cloud.google.com/compute/docs/labeling-resources says
// "Keys and values can only contain lowercase letters, numeric characters,
// underscores, and dashes. International characters are allowed."
// We're ignoring the second sentence until someone complains, in part
// because it's not clear how it interacts with the first sentence.

bool
isLabelSafe( const std::string & korv ) {
	for( size_t i = 0; i < korv.length(); ++i ) {
		char c = korv[i];
		if(! (std::islower(c) || std::isdigit(c) || c == '_' || c == '-' )) {
			return false;
		}
	}
	return true;
}

GceInstanceInsert::GceInstanceInsert() { }

GceInstanceInsert::~GceInstanceInsert() { }

// Expecting:GCE_INSTANCE_INSERT <req_id> <serviceurl> <authfile> <account> <project> <zone>
//     <instance_name> <machine_type> <image> <metadata> <metadata_file>
//     <preemptible> <json_file> <tag=value>* NULLSTRING
bool GceInstanceInsert::workerFunction(char **argv, int argc, string &result_string) {
	assert( strcasecmp( argv[0], "GCE_INSTANCE_INSERT" ) == 0 );

	int requestID;
	get_int( argv[1], & requestID );

	if( ! verify_min_number_args( argc, 15 ) ) {
		result_string = create_failure_result( requestID, "Wrong_Argument_Number" );
		dprintf( D_ALWAYS, "Wrong number of arguments (%d should be >= %d) to %s\n",
				 argc, 15, argv[0] );
		return false;
	}

	// Fill in required attributes & parameters.
	GceInstanceInsert insert_request;
	insert_request.serviceURL = argv[2];
	insert_request.serviceURL += "/projects/";
	insert_request.serviceURL += argv[5];
	insert_request.serviceURL += "/zones/";
	insert_request.serviceURL += argv[6];
	insert_request.serviceURL += "/instances";
	insert_request.requestMethod = "POST";

	std::map<string, string> metadata;
	if ( strcasecmp( argv[10], NULLSTRING ) ) {
		string key;
		string value;
		const char *pos = argv[10];
		while ( ParseMetadataLine( pos, key, value, ',' ) ) {
			metadata[key] = value;
		}
	}
	if ( strcasecmp( argv[11], NULLSTRING ) ) {
		string file_contents;
		if ( !htcondor::readShortFile( argv[11], file_contents ) ) {
			result_string = create_failure_result( requestID, "Failed to open metadata file" );
			return true;
		}
		string key;
		string value;
		const char *pos = file_contents.c_str();
		while( ParseMetadataLine( pos, key, value, '\n' ) ) {
			metadata[key] = value;
		}
	}
	string json_file_contents;
	if ( strcasecmp( argv[13], NULLSTRING ) ) {
		if ( !htcondor::readShortFile( argv[13], json_file_contents ) ) {
			result_string = create_failure_result( requestID, "Failed to open additional JSON file" );
			return true;
		}
	}

	insert_request.requestBody = "{\n";
	insert_request.requestBody += " \"machineType\": \"";
	insert_request.requestBody += argv[8];
	insert_request.requestBody += "\",\n";

	// This is documented incorrectly at https://cloud.google.com/compute/docs/labeling-resources;
	// the actual API is the sane one you'd expect.
	int endOfTagList = 14;
	if( strcasecmp( argv[endOfTagList], NULLSTRING ) ) {
		insert_request.requestBody += " \"labels\": {";
		for( ; endOfTagList < argc - 2; endOfTagList += 2 ) {
			if( strcasecmp( argv[endOfTagList], NULLSTRING ) == 0 ) { break; }
			std::string key = argv[endOfTagList];
			std::string value = argv[endOfTagList + 1];

			if( key.length() >= 64 || value.length() >= 64 ) {
				result_string = create_failure_result( requestID,
					"Label key or value too long" );
				return true;
			}

			if( key.empty() ) {
				result_string = create_failure_result( requestID,
					"Label keys may not be empty" );
				return true;
			}

			// ibid., "Label keys must start with a lowercase letter and
			// international characters are allowed."  See comment about
			// isLabelSafe().
			if(! std::islower(key[0])) {
				result_string = create_failure_result( requestID,
					"Label keys must start with a lower case letter" );
				return true;
			}

			if( (!isLabelSafe( key )) || (!isLabelSafe( value )) ) {
				result_string = create_failure_result( requestID,
					"Invalid label key or value" );
				return true;
			}

			insert_request.requestBody +=
				"\"" + key + "\": \"" + value + "\",";
		}
		if( strcasecmp( argv[endOfTagList], NULLSTRING ) ) {
			result_string = create_failure_result( requestID,
									"Failed to parse labels" );
			return true;
		}
		insert_request.requestBody.erase( insert_request.requestBody.length() - 1 );
		insert_request.requestBody += "},\n";
	}

	insert_request.requestBody += " \"name\": \"";
	insert_request.requestBody += argv[7];
	insert_request.requestBody += "\",\n";
	insert_request.requestBody += "  \"scheduling\":\n";
	insert_request.requestBody += "  {\n";
	insert_request.requestBody += "    \"preemptible\": ";
	insert_request.requestBody += argv[12];
	insert_request.requestBody += "\n  },\n";
	insert_request.requestBody += " \"disks\": [\n  {\n";
	insert_request.requestBody += "   \"boot\": true,\n";
	insert_request.requestBody += "   \"autoDelete\": true,\n";
	insert_request.requestBody += "   \"initializeParams\": {\n";
	insert_request.requestBody += "    \"sourceImage\": \"";
	insert_request.requestBody += argv[9];
	insert_request.requestBody += "\"\n";
	insert_request.requestBody += "   }\n  }\n ],\n";
	if ( !metadata.empty() ) {
		insert_request.requestBody += " \"metadata\": {\n";
		insert_request.requestBody += "  \"items\": [\n";

		for ( map<string, string>::const_iterator itr = metadata.begin(); itr != metadata.end(); itr++ ) {
			if ( itr != metadata.begin() ) {
				insert_request.requestBody += ",\n";
			}
			insert_request.requestBody += "   {\n    \"key\": \"";
			insert_request.requestBody += itr->first;
			insert_request.requestBody += "\",\n";
			insert_request.requestBody += "    \"value\": \"";
			insert_request.requestBody += escapeJSONString( itr->second.c_str() );
			insert_request.requestBody += "\"\n   }";
		}

		insert_request.requestBody += "\n  ]\n";
		insert_request.requestBody += " },\n";
	}
	insert_request.requestBody += " \"networkInterfaces\": [\n  {\n";
	insert_request.requestBody += "   \"network\": \"";
	insert_request.requestBody += argv[2];
	insert_request.requestBody += "/projects/";
	insert_request.requestBody += argv[5];
	insert_request.requestBody += "/global/networks/default\",\n";
	insert_request.requestBody += "   \"accessConfigs\": [\n    {\n";
	insert_request.requestBody += "     \"name\": \"External NAT\",\n";
	insert_request.requestBody += "     \"type\": \"ONE_TO_ONE_NAT\"\n";
	insert_request.requestBody += "    }\n   ]\n";
	insert_request.requestBody += "  }\n ]";
	insert_request.requestBody += "\n}\n";

	if ( !json_file_contents.empty() ) {
		classad::ClassAd instance_ad;
		classad::ClassAd custom_ad;
		classad::ClassAdJsonParser parser;
		classad::ClassAdJsonUnParser unparser;
		string wrap_custom_attrs = "{" + json_file_contents + "}";

		if ( !parser.ParseClassAd( insert_request.requestBody, instance_ad, true ) ) {
			result_string = create_failure_result( requestID,
									"Failed to parse instance description" );
			return true;
		}

		if ( !parser.ParseClassAd( wrap_custom_attrs, custom_ad, true ) ) {
			result_string = create_failure_result( requestID,
									"Failed to parse custom attributes" );
			return true;
		}

		instance_ad.Update( custom_ad );

		insert_request.requestBody.clear();
		unparser.Unparse( insert_request.requestBody, &instance_ad );
	}

	string auth_file = argv[3];
	if ( !GetAccessToken( auth_file, argv[4], insert_request.accessToken,
						  insert_request.errorMessage ) ) {
		result_string = create_failure_result( requestID,
											   insert_request.errorMessage.c_str() );
		return true;
	}

	// Send the request.
	if( ! insert_request.SendRequest() ) {
		// TODO Fix construction of error message
		result_string = create_failure_result( requestID,
							insert_request.errorMessage.c_str(),
							insert_request.errorCode.c_str() );
		return true;
	}

	string op_name;
	string key;
	string value;
	int nesting = 0;

	const char *pos = insert_request.resultString.c_str();
	while( ParseJSONLine( pos, key, value, nesting ) ) {
		if ( key == "name" ) {
			op_name = value;
			break;
		}
	}

	string status;
	string instance_id;
	string err_msg;
	bool in_err = false;
	do {

		// Give the operation some time to complete.
		// TODO Is there a better way to do this?
		gce_gahp_release_big_mutex();
		sleep( 5 );
		gce_gahp_grab_big_mutex();

		GceRequest op_request;
		op_request.serviceURL = argv[2];
		op_request.serviceURL += "/projects/";
		op_request.serviceURL += argv[5];
		op_request.serviceURL += "/zones/";
		op_request.serviceURL += argv[6];
		op_request.serviceURL += "/operations/";
		op_request.serviceURL += op_name;
		op_request.requestMethod = "GET";

		if ( !GetAccessToken( auth_file, argv[4], op_request.accessToken,
							  op_request.errorMessage ) ) {
			result_string = create_failure_result( requestID,
												   op_request.errorMessage.c_str() );
			return true;
		}

		if ( !op_request.SendRequest() ) {
			// TODO Fix construction of error message
			result_string = create_failure_result( requestID,
								op_request.errorMessage.c_str(),
								op_request.errorCode.c_str() );
			return true;
		}

		nesting = 0;
		pos = op_request.resultString.c_str();
		while ( ParseJSONLine( pos, key, value, nesting ) ) {
			if ( key == "status" ) {
				status = value;
			} else if ( key == "error" ) {
				in_err = true;
			} else if ( key == "warnings" ) {
				in_err = false;
			} else if ( key == "message" && in_err ) {
				err_msg = value;
			} else if ( key == "targetId" ) {
				instance_id = value;
			}
		}
	} while ( status != "DONE" );

	if ( !err_msg.empty() ) {
		result_string = create_failure_result( requestID, err_msg.c_str() );
		return true;
	}
	if ( instance_id.empty() ) {
		result_string = create_failure_result( requestID,
								"Completed instance insert has no id" );
		return true;
	}

	StringList reply;
	reply.append( instance_id.c_str() );
	result_string = create_success_result( requestID, &reply );

	return true;
}

// ---------------------------------------------------------------------------

GceInstanceDelete::GceInstanceDelete() { }

GceInstanceDelete::~GceInstanceDelete() { }

// Expecting:GCE_INSTANCE_DELETE <req_id> <serviceurl> <authfile> <account> <project> <zone> <instance_name>
bool GceInstanceDelete::workerFunction(char **argv, int argc, string &result_string) {
	assert( strcasecmp( argv[0], "GCE_INSTANCE_DELETE" ) == 0 );

	int requestID;
	get_int( argv[1], & requestID );

	if( ! verify_number_args( argc, 8 ) ) {
		result_string = create_failure_result( requestID, "Wrong_Argument_Number" );
		dprintf( D_ALWAYS, "Wrong number of arguments (%d should be >= %d) to %s\n",
				 argc, 8, argv[0] );
		return false;
	}

	// Fill in required attributes & parameters.
	GceInstanceDelete delete_request;
	delete_request.serviceURL = argv[2];
	delete_request.serviceURL += "/projects/";
	delete_request.serviceURL += argv[5];
	delete_request.serviceURL += "/zones/";
	delete_request.serviceURL += argv[6];
	delete_request.serviceURL += "/instances/";
	delete_request.serviceURL += argv[7];
	delete_request.requestMethod = "DELETE";

	string auth_file = argv[3];
	if ( !GetAccessToken( auth_file, argv[4], delete_request.accessToken,
						  delete_request.errorMessage ) ) {
		result_string = create_failure_result( requestID,
											   delete_request.errorMessage.c_str() );
		return true;
	}

	// Send the request.
	if( ! delete_request.SendRequest() ) {
		// TODO Fix construction of error message
		result_string = create_failure_result( requestID,
							delete_request.errorMessage.c_str(),
							delete_request.errorCode.c_str() );
		return true;
	}

	string op_name;
	string key;
	string value;
	int nesting = 0;

	const char *pos = delete_request.resultString.c_str();
	while( ParseJSONLine( pos, key, value, nesting ) ) {
		if ( key == "name" ) {
			op_name = value;
			break;
		}
	}

	string status;
	string err_msg;
	bool in_err = false;
	do {

		// Give the operation some time to complete.
		// TODO Is there a better way to do this?
		gce_gahp_release_big_mutex();
		sleep( 5 );
		gce_gahp_grab_big_mutex();

		GceRequest op_request;
		op_request.serviceURL = argv[2];
		op_request.serviceURL += "/projects/";
		op_request.serviceURL += argv[5];
		op_request.serviceURL += "/zones/";
		op_request.serviceURL += argv[6];
		op_request.serviceURL += "/operations/";
		op_request.serviceURL += op_name;
		op_request.requestMethod = "GET";

		if ( !GetAccessToken( auth_file, argv[4], op_request.accessToken,
							  op_request.errorMessage ) ) {
			result_string = create_failure_result( requestID,
												   op_request.errorMessage.c_str() );
			return true;
		}

		if ( !op_request.SendRequest() ) {
			// TODO Fix construction of error message
			result_string = create_failure_result( requestID,
								op_request.errorMessage.c_str(),
								op_request.errorCode.c_str() );
			return true;
		}

		nesting = 0;
		pos = op_request.resultString.c_str();
		while ( ParseJSONLine( pos, key, value, nesting ) ) {
			if ( key == "status" ) {
				status = value;
			} else if ( key == "error" ) {
				in_err = true;
			} else if ( key == "warnings" ) {
				in_err = false;
			} else if ( key == "message" && in_err ) {
				err_msg = value;
			}
		}
	} while ( status != "DONE" );

	if ( !err_msg.empty() ) {
		result_string = create_failure_result( requestID, err_msg.c_str() );
		return true;
	}

	result_string = create_success_result( requestID, NULL );

	return true;
}

// ---------------------------------------------------------------------------

GceInstanceList::GceInstanceList() { }

GceInstanceList::~GceInstanceList() { }

// Expecting:GCE_INSTANCE_LIST <req_id> <serviceurl> <authfile> <account> <project> <zone>
bool GceInstanceList::workerFunction(char **argv, int argc, string &result_string) {
	assert( strcasecmp( argv[0], "GCE_INSTANCE_LIST" ) == 0 );

	int requestID;
	string next_page_token;
	vector<string> results;
	get_int( argv[1], & requestID );

	if( ! verify_number_args( argc, 7 ) ) {
		result_string = create_failure_result( requestID, "Wrong_Argument_Number" );
		dprintf( D_ALWAYS, "Wrong number of arguments (%d should be >= %d) to %s\n",
				 argc, 7, argv[0] );
		return false;
	}

	while( true ) {
		// Fill in required attributes & parameters.
		GceInstanceList list_request;
		list_request.serviceURL = argv[2];
		list_request.serviceURL += "/projects/";
		list_request.serviceURL += argv[5];
		list_request.serviceURL += "/zones/";
		list_request.serviceURL += argv[6];
		list_request.serviceURL += "/instances";
		if (!next_page_token.empty() ) {
			dprintf( D_ALWAYS, "Requesting page token %s\n", next_page_token.c_str() );
			list_request.serviceURL += "?pageToken=";
			list_request.serviceURL += next_page_token;
			next_page_token="";
		}

		list_request.requestMethod = "GET";

		string auth_file = argv[3];
		if ( !GetAccessToken( auth_file, argv[4], list_request.accessToken,
							  list_request.errorMessage ) ) {
			result_string = create_failure_result( requestID,
												   list_request.errorMessage.c_str() );
			return true;
		}

		// Send the request.
		if( ! list_request.SendRequest() ) {
			// TODO Fix construction of error message
			result_string = create_failure_result( requestID,
								list_request.errorMessage.c_str(),
								list_request.errorCode.c_str() );
			return true;
		} else {
			string next_id;
			string next_name;
			string next_status;
			string next_status_msg;
			string key;
			string value;
			int nesting = 0;

			const char *pos = list_request.resultString.c_str();
			while( ParseJSONLine( pos, key, value, nesting ) ) {
				if ( key == "nextPageToken") {
					next_page_token = value;
					dprintf( D_ALWAYS, "Found page token %s\n", next_page_token.c_str() );
				}
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
		}
		if (next_page_token.empty()) {
			char buff[16];
			sprintf( buff, "%d", (int)(results.size() / 4) );

			StringList response;
			response.append( buff );
			for ( vector<string>::iterator idx = results.begin(); idx != results.end(); idx++ ) {
				response.append( idx->c_str() );
			}
			result_string = create_success_result( requestID, &response );
			return true;
		}
	}
}

// ---------------------------------------------------------------------------

GceGroupInsert::GceGroupInsert() { }

GceGroupInsert::~GceGroupInsert() { }

// Expecting GCE_GROUP_INSERT <req_id> <serviceurl> <authfile> <account> <project> <zone>
//     <group_name> <machine_type> <image> <metadata> <metadata_file>
//     <preemptible> <json_file> <count> <duration_hours>

bool GceGroupInsert::workerFunction(char **argv, int argc, string &result_string) {
	assert( strcasecmp( argv[0], "GCE_GROUP_INSERT" ) == 0 );

	int requestID;
	get_int( argv[1], & requestID );

	if( ! verify_number_args( argc, 16 ) ) {
		result_string = create_failure_result( requestID, "Wrong_Argument_Number" );
		dprintf( D_ALWAYS, "Wrong number of arguments (%d should be >= %d) to %s\n",
				 argc, 16, argv[0] );
		return false;
	}

	// Fill in required attributes & parameters.
	GceGroupInsert insert_request;
	insert_request.serviceURL = argv[2];
	insert_request.serviceURL += "/projects/";
	insert_request.serviceURL += argv[5];
	insert_request.serviceURL += "/global/instanceTemplates";
	insert_request.requestMethod = "POST";

	std::map<string, string> metadata;
	if ( strcasecmp( argv[10], NULLSTRING ) ) {
		string key;
		string value;
		const char *pos = argv[10];
		while ( ParseMetadataLine( pos, key, value, ',' ) ) {
			metadata[key] = value;
		}
	}
	if ( strcasecmp( argv[11], NULLSTRING ) ) {
		string file_contents;
		if ( !htcondor::readShortFile( argv[11], file_contents ) ) {
			result_string = create_failure_result( requestID, "Failed to open metadata file" );
			return true;
		}
		string key;
		string value;
		const char *pos = file_contents.c_str();
		while( ParseMetadataLine( pos, key, value, '\n' ) ) {
			metadata[key] = value;
		}
	}
	string json_file_contents;
	if ( strcasecmp( argv[13], NULLSTRING ) ) {
		if ( !htcondor::readShortFile( argv[13], json_file_contents ) ) {
			result_string = create_failure_result( requestID, "Failed to open additional JSON file" );
			return true;
		}
	}

	insert_request.requestBody = "{\n";
	insert_request.requestBody += " \"name\": \"";
	insert_request.requestBody += argv[7];
	insert_request.requestBody += "-template\",\n";
	insert_request.requestBody += "  \"properties\": \n";
	insert_request.requestBody += "  {\n";
	insert_request.requestBody += "   \"machineType\": \"";
	insert_request.requestBody += argv[8];
	insert_request.requestBody += "\",\n";
	insert_request.requestBody += "    \"scheduling\":\n";
	insert_request.requestBody += "    {\n";
	insert_request.requestBody += "      \"preemptible\": ";
	insert_request.requestBody += argv[12];
	insert_request.requestBody += "\n    },\n";
	insert_request.requestBody += "   \"disks\": [\n  {\n";
	insert_request.requestBody += "     \"boot\": true,\n";
	insert_request.requestBody += "     \"autoDelete\": true,\n";
	insert_request.requestBody += "     \"initializeParams\": {\n";
	insert_request.requestBody += "      \"sourceImage\": \"";
	insert_request.requestBody += argv[9];
	insert_request.requestBody += "\"\n";
	insert_request.requestBody += "   }\n  }\n ],\n";
	if ( !metadata.empty() ) {
		insert_request.requestBody += " \"metadata\": {\n";
		insert_request.requestBody += "  \"items\": [\n";

		for ( map<string, string>::const_iterator itr = metadata.begin(); itr != metadata.end(); itr++ ) {
			if ( itr != metadata.begin() ) {
				insert_request.requestBody += ",\n";
			}
			insert_request.requestBody += "   {\n    \"key\": \"";
			insert_request.requestBody += itr->first;
			insert_request.requestBody += "\",\n";
			insert_request.requestBody += "    \"value\": \"";
			insert_request.requestBody += escapeJSONString( itr->second.c_str() );
			insert_request.requestBody += "\"\n   }";
		}

		insert_request.requestBody += "\n  ]\n";
		insert_request.requestBody += " },\n";
	}
	insert_request.requestBody += " \"networkInterfaces\": [\n  {\n";
	insert_request.requestBody += "   \"network\": \"";
	insert_request.requestBody += argv[2];
	insert_request.requestBody += "/projects/";
	insert_request.requestBody += argv[5];
	insert_request.requestBody += "/global/networks/default\",\n";
	insert_request.requestBody += "   \"accessConfigs\": [\n    {\n";
	insert_request.requestBody += "     \"name\": \"External NAT\",\n";
	insert_request.requestBody += "     \"type\": \"ONE_TO_ONE_NAT\"\n";
	insert_request.requestBody += "    }\n   ]\n";
	insert_request.requestBody += "  }\n ]";
	insert_request.requestBody += "}\n}\n";

	if ( !json_file_contents.empty() ) {
		classad::ClassAd instance_ad;
		classad::ClassAd custom_ad;
		classad::ClassAdJsonParser parser;
		classad::ClassAdJsonUnParser unparser;
		string wrap_custom_attrs = "{" + json_file_contents + "}";

		if ( !parser.ParseClassAd( insert_request.requestBody, instance_ad, true ) ) {
			result_string = create_failure_result( requestID,
									"Failed to parse instance description" );
			return true;
		}

		if ( !parser.ParseClassAd( wrap_custom_attrs, custom_ad, true ) ) {
			result_string = create_failure_result( requestID,
									"Failed to parse custom attributes" );
			return true;
		}

		instance_ad.Update( custom_ad );

		insert_request.requestBody.clear();
		unparser.Unparse( insert_request.requestBody, &instance_ad );
	}

	string auth_file = argv[3];
	string account = argv[4];
	if ( !GetAccessToken( auth_file, account, insert_request.accessToken,
						  insert_request.errorMessage ) ) {
		result_string = create_failure_result( requestID,
											   insert_request.errorMessage.c_str() );
		return true;
	}

	// Send the request.
	if( ! insert_request.SendRequest() ) {
		dprintf( D_ALWAYS, "Error is '%s'\n", insert_request.errorMessage.c_str() );
		// TODO Fix construction of error message
		result_string = create_failure_result( requestID,
							insert_request.errorMessage.c_str(),
							insert_request.errorCode.c_str() );
		return true;
	}

	string op_name;
	string key;
	string value;
	int nesting = 0;

	const char *pos = insert_request.resultString.c_str();
	while( ParseJSONLine( pos, key, value, nesting ) ) {
		if ( key == "name" ) {
			op_name = value;
			break;
		}
	}

	// Wait for instance template to be created
	string err_msg;
	string opUrl = argv[2];
	opUrl += "/projects/";
	opUrl += argv[5];
	opUrl += "/global/operations/";
	opUrl += op_name;
	if ( verifyRequest( opUrl, auth_file, account, err_msg ) == false ) {
		result_string = create_failure_result( requestID, err_msg.c_str() );
		return true;
	}

	// Now construct managed instance group
	GceRequest group_request;
	group_request.serviceURL = argv[2];
	group_request.serviceURL += "/projects/";
	group_request.serviceURL += argv[5];
	group_request.serviceURL += "/zones/";
	group_request.serviceURL += argv[6];
	group_request.serviceURL += "/instanceGroupManagers";
	group_request.requestMethod = "POST";
	group_request.requestBody = "{\n";
	group_request.requestBody += " \"name\": \"";
	group_request.requestBody += argv[7];
	group_request.requestBody += "-group\",\n";
	group_request.requestBody += " \"instanceTemplate\": \"";
	group_request.requestBody += "/projects/";
	group_request.requestBody += argv[5];
	group_request.requestBody += "/global/instanceTemplates/";
	group_request.requestBody += argv[7];
	group_request.requestBody += "-template\",\n";
	group_request.requestBody += " \"baseInstanceName\": \"";
	group_request.requestBody += argv[7];
	group_request.requestBody += "-instance\",\n";
	group_request.requestBody += " \"targetSize\": \"";
	group_request.requestBody += argv[14];
	group_request.requestBody += "\"\n";
	group_request.requestBody += "}\n";


	if ( !GetAccessToken( auth_file, account, group_request.accessToken,
						  group_request.errorMessage ) ) {
		result_string = create_failure_result( requestID,
											   group_request.errorMessage.c_str() );
		return true;
	}

	if ( !group_request.SendRequest() ) {
		result_string = create_failure_result( requestID,
							group_request.errorMessage.c_str(),
							group_request.errorCode.c_str() );
		return true;
	}

	nesting = 0;
	pos = group_request.resultString.c_str();
	while( ParseJSONLine( pos, key, value, nesting ) ) {
		if ( key == "name" ) {
			op_name = value;
			break;
		}
	}

	// Wait for group template to be created
	opUrl = argv[2];
	opUrl += "/projects/";
	opUrl += argv[5];
	opUrl += "/zones/";
	opUrl += argv[6];
	opUrl += "/operations/";
	opUrl += op_name;
	if ( verifyRequest( opUrl, auth_file, account, err_msg ) ) {
		result_string = create_success_result( requestID, NULL );
	} else {
		result_string = create_failure_result( requestID, err_msg.c_str() );
	}

	return true;
}


// ---------------------------------------------------------------------------

GceGroupDelete::GceGroupDelete() { }

GceGroupDelete::~GceGroupDelete() { }

// Expecting GCE_GROUP_DELETE <req_id> <serviceurl> <authfile> <account> <project> <zone> <group_name>

bool GceGroupDelete::workerFunction(char **argv, int argc, string &result_string) {
	assert( strcasecmp( argv[0], "GCE_GROUP_DELETE" ) == 0 );

	int requestID;
	get_int( argv[1], & requestID );

	if( ! verify_number_args( argc, 8 ) ) {
		result_string = create_failure_result( requestID, "Wrong_Argument_Number" );
		dprintf( D_ALWAYS, "Wrong number of arguments (%d should be >= %d) to %s\n",
				 argc, 8, argv[0] );
		return false;
	}

	// Now construct managed instance group
	GceRequest group_request;
	group_request.serviceURL = argv[2];
	group_request.serviceURL += "/projects/";
	group_request.serviceURL += argv[5];
	group_request.serviceURL += "/zones/";
	group_request.serviceURL += argv[6];
	group_request.serviceURL += "/instanceGroupManagers/";
	group_request.serviceURL += argv[7];
	group_request.serviceURL += "-group";
	group_request.requestMethod = "DELETE";

	string auth_file = argv[3];
	string account = argv[4];
	if ( !GetAccessToken( auth_file, account, group_request.accessToken,
						  group_request.errorMessage ) ) {
		result_string = create_failure_result( requestID,
											   group_request.errorMessage.c_str() );
		return true;
	}

	string op_name;
	string key;
	string value;
	int nesting = 0;
	const char *pos = group_request.resultString.c_str();
	string opUrl = argv[2];
	string err_msg;

	if ( !group_request.SendRequest() ) {
		if (group_request.errorCode.find("404") != std::string::npos) {
		    dprintf( D_ALWAYS, "Got 404, group manager already deleted, continuing\n");
			// Continue if group manager delete not found
			// Attempt to delete instance template
		} else {
			// Other responses indicate some other critical failure
			result_string = create_failure_result( requestID,
							group_request.errorMessage.c_str(),
							group_request.errorCode.c_str() );
			return true;
		}
	} else {

		while( ParseJSONLine( pos, key, value, nesting ) ) {
			if ( key == "name" ) {
				op_name = value;
				break;
			}
		}

		// Wait for instance group manager to be deleted
		opUrl += "/projects/";
		opUrl += argv[5];
		opUrl += "/zones/";
		opUrl += argv[6];
		opUrl += "/operations/";
		opUrl += op_name;
		if ( verifyRequest( opUrl, auth_file, account, err_msg ) == false ) {
			result_string = create_failure_result( requestID, err_msg.c_str() );
			return true;
		}
	}

	// Fill in required attributes & parameters.
	GceGroupDelete delete_request;
	delete_request.serviceURL = argv[2];
	delete_request.serviceURL += "/projects/";
	delete_request.serviceURL += argv[5];
	delete_request.serviceURL += "/global/instanceTemplates/";
	delete_request.serviceURL += argv[7];
	delete_request.serviceURL += "-template";
	delete_request.requestMethod = "DELETE";

	if ( !GetAccessToken( auth_file, account, delete_request.accessToken,
						  delete_request.errorMessage ) ) {
		result_string = create_failure_result( requestID,
											   delete_request.errorMessage.c_str() );
		return true;
	}

	// Send the request.
	if( ! delete_request.SendRequest() ) {
		dprintf( D_ALWAYS, "Error is '%s'\n", delete_request.errorMessage.c_str() );
		// TODO Fix construction of error message
		result_string = create_failure_result( requestID,
							delete_request.errorMessage.c_str(),
							delete_request.errorCode.c_str() );
		return true;
	}

	nesting = 0;
	pos = delete_request.resultString.c_str();
	while( ParseJSONLine( pos, key, value, nesting ) ) {
		if ( key == "name" ) {
			op_name = value;
			break;
		}
	}

	// Wait for instance template to be deleted
	opUrl = argv[2];
	opUrl += "/projects/";
	opUrl += argv[5];
	opUrl += "/global/operations/";
	opUrl += op_name;
	if ( verifyRequest( opUrl, auth_file, account, err_msg ) ) {
		result_string = create_success_result( requestID, NULL );
	} else {
		result_string = create_failure_result( requestID, err_msg.c_str() );
	}
	return true;
}
