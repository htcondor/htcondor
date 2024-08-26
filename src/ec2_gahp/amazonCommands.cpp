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
#include "condor_arglist.h"
#include "util_lib_proto.h"
#include "internet.h"
#include "my_popen.h"
#include "basename.h"
#include "vm_univ_utils.h"
#include "amazongahp_common.h"
#include "amazonCommands.h"
#include "AWSv4-utils.h"
#include "AWSv4-impl.h"
#include "shortfile.h"

#include "condor_base64.h"
#include <sstream>
#include "stat_wrapper.h"
#include "condor_regex.h"
#include <algorithm>
#include <openssl/hmac.h>
#include <curl/curl.h>
#include "thread_control.h"
#include <expat.h>
#include "stl_string_utils.h"
#include "subsystem_info.h"

// Statistics of interest.
int NumRequests = 0;
int NumDistinctRequests = 0;
int NumRequestsExceedingLimit = 0;
int NumExpiredSignatures = 0;

#define NULLSTRING "NULL"

const char * nullStringIfEmpty( const std::string & str ) {
    if( str.empty() ) { return NULLSTRING; }
    else { return str.c_str(); }
}

std::string amazonURLEncode( const std::string & input )
{
    return AWSv4Impl::amazonURLEncode( input );
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

AmazonRequest::~AmazonRequest() { }

#define SET_CURL_SECURITY_OPTION( A, B, C ) { \
    CURLcode rv##B = curl_easy_setopt( A, B, C ); \
    if( rv##B != CURLE_OK ) { \
        this->errorCode = "E_CURL_LIB"; \
        this->errorMessage = "curl_easy_setopt( " #B " ) failed."; \
        dprintf( D_ALWAYS, "curl_easy_setopt( %s ) failed (%d): '%s', failing.\n", \
            #B, rv##B, curl_easy_strerror( rv##B ) ); \
        return false; \
    } \
}


class Throttle {
    public:
        Throttle() : count( 0 ), rateLimit( 0 ) {
#ifdef UNIX
            // Determine which type of clock to use.
            int rv = clock_gettime( CLOCK_MONOTONIC, & when );
            if( rv == 0 ) { type = CLOCK_MONOTONIC; }
            else {
                ASSERT( errno == EINVAL );
                type = CLOCK_REALTIME;
            }
#endif

            when.tv_sec = 0;
            when.tv_nsec = 0;

            deadline.tv_sec = 0;
            deadline.tv_nsec = 0;
        }

        struct timespec getWhen() { return when; }

        // This function is called without the big mutex.  Do NOT add
        // dprintf() statements or refers to globals other than 'this'.
        bool isValid() const { return when.tv_sec != 0; }

        // This function is called without the big mutex.  Do NOT add
        // dprintf() statements or refers to globals other than 'this'.
        struct timespec getDeadline() { return deadline; }

        // This function is called without the big mutex.  Do NOT add
        // dprintf() statements or refers to globals other than 'this'.
        bool setDeadline( struct timespec t, time_t offset ) {
            if( t.tv_sec == 0 ) { return false; }

            deadline = t;
            deadline.tv_sec += offset;
            return true;
        }

        // This function is called without the big mutex.  Do NOT add
        // dprintf() statements or refers to globals other than 'this'.
        void sleepIfNecessary() {
            if( this->isValid() ) {
                int rv;
#if defined(HAVE_CLOCK_NANOSLEEP)
                do {
                    rv = clock_nanosleep( type, TIMER_ABSTIME, & when, NULL );
                } while( rv == EINTR );
#else
                struct timespec delay;
                now( &delay );
                delay.tv_sec = when.tv_sec - delay.tv_sec;
                delay.tv_nsec = when.tv_nsec - delay.tv_nsec;
                if ( delay.tv_nsec < 0 ) {
                    delay.tv_sec -= 1;
                    delay.tv_nsec += 1'000'000'000;
                }
                if ( delay.tv_sec < 0 ) {
                    return;
                }
                do {
                    rv = nanosleep( &delay, &delay );
                } while ( rv != 0 && errno == EINTR );
#endif
                // Suicide rather than overburden the service.
                ASSERT( rv == 0 );
            }
        }

        static void now( struct timespec * t ) {
            if( t != NULL ) {
#ifdef UNIX
                clock_gettime( type, t );
#else
                struct timeval tv;
                gettimeofday( &tv, NULL );
                t->tv_sec = tv.tv_sec;
                t->tv_nsec = tv.tv_usec * 1000;
#endif
            }
        }

        static long difference( const struct timespec * s, const struct timespec * t ) {
            long secondsDiff = t->tv_sec - s->tv_sec;
            long millisDiff = ((t->tv_nsec - s->tv_nsec) + 500'000)/1'000'000;
			// If secondsDiff is too large (as when, for instance, the
			// the liveline is 0 because the limit has never been exceeded
			// and the deadline based on the monotonic clock of a mchine
			// up for more than ~27 days), converting secondsDiff into
			// milliseconds will overflow on 32-bit machines.
			//
			// Since difference() is only called for debugging purposes
			// and to compare against zero, we only care about making
			// sure the sign doesn't change.
			long secondsDiffInMillis = secondsDiff * 1000;
			if( (secondsDiffInMillis < 0 && secondsDiff > 0)
			   || (secondsDiffInMillis > 0 && secondsDiff < 0) ) {
				return secondsDiff;
			}
            millisDiff += (secondsDiff * 1000);
            return millisDiff;
        }

        bool limitExceeded() {
            // Compute until when to sleep before making another request.
            now( & when );

            assert( count < 32 );
            unsigned milliseconds = (1 << count) * 100;
   			if( rateLimit > 0 && milliseconds < (unsigned)rateLimit ) { milliseconds = rateLimit; }
            unsigned seconds = milliseconds / 1000;
            unsigned nanoseconds = (milliseconds % 1000) * 1'000'000;

            dprintf( D_PERF_TRACE | D_VERBOSE,
            	"limitExceeded(): setting when to %u milliseconds from now (%ld.%09ld)\n", milliseconds, when.tv_sec, when.tv_nsec );
            when.tv_sec += seconds;
            when.tv_nsec += nanoseconds;
            if( when.tv_nsec > 1'000'000'000 ) {
                when.tv_nsec -= 1'000'000'000;
                when.tv_sec += 1;
            }
            dprintf( D_PERF_TRACE | D_VERBOSE,
            	"limitExceeded(): set when to %ld.%09ld\n", when.tv_sec, when.tv_nsec );

            // If we've waited so long that our next delay will be after
            // the deadline, fail immediately rather than get an invalid
            // signature error.
            if( deadline.tv_sec < when.tv_sec ||
                (deadline.tv_sec == when.tv_sec &&
                  deadline.tv_nsec < when.tv_nsec) ) {
                dprintf( D_PERF_TRACE | D_VERBOSE,
                	"limitExceeded(): which is after the deadline.\n" );
                ++NumExpiredSignatures;
                return true;
            }

            // Even if we're given a longer deadline, assume that 204.8
            // seconds is as long as we ever want to wait.
            if( count <= 11 ) { ++count; return false; }
            else { return true; }
        }

        void limitNotExceeded() {
            count = 0;
            when.tv_sec = 0;
            deadline.tv_sec = 0;

            if( rateLimit <= 0 ) { return; }

            now( & when );
            unsigned milliseconds = rateLimit;
            unsigned seconds = milliseconds / 1000;
            unsigned nanoseconds = (milliseconds % 1000) * 1'000'000;
            dprintf( D_PERF_TRACE | D_VERBOSE,
            	"rate limiting: setting when to %u milliseconds from now (%ld.%09ld)\n", milliseconds, when.tv_sec, when.tv_nsec );

            when.tv_sec += seconds;
            when.tv_nsec += nanoseconds;
            if( when.tv_nsec > 1'000'000'000 ) {
                when.tv_nsec -= 1'000'000'000;
                when.tv_sec += 1;
            }
            dprintf( D_PERF_TRACE | D_VERBOSE,
            	"rate limiting: set when to %ld.%09ld\n", when.tv_sec, when.tv_nsec );
        }

    protected:
#ifdef UNIX
        static clockid_t type;
#endif
        unsigned int count;
        struct timespec when;
        struct timespec deadline;

	public:
		// Managed by AmazonRequest::SendRequest() because the param system
		// isn't up yet when this object is constructed.
        int rateLimit;

};

#ifdef UNIX
clockid_t Throttle::type;
#endif
Throttle globalCurlThrottle;

pthread_mutex_t globalCurlMutex = PTHREAD_MUTEX_INITIALIZER;

bool AmazonRequest::SendRequest() {
    query_parameters.insert( std::make_pair( "Version", "2012-10-01" ) );

	switch( signatureVersion ) {
		case 2:
			return sendV2Request();
		case 4:
			return sendV4Request( canonicalizeQueryString() );
		default:
			this->errorCode = "E_INTERNAL";
			this->errorMessage = "Invalid signature version.";
			dprintf( D_ALWAYS, "Invalid signature version (%d), failing.\n", signatureVersion );
			return false;
	}
}

std::string AmazonRequest::canonicalizeQueryString() {
    return AWSv4Impl::canonicalizeQueryString( query_parameters );
}

bool parseURL(	const std::string & url,
				std::string & protocol,
				std::string & host,
				std::string & path ) {
    Regex r; int errCode = 0; int errOffset = 0;
    bool patternOK = r.compile( "([^:]+)://(([^/]+)(/.*)?)", &errCode, &errOffset);
    ASSERT( patternOK );
	std::vector<std::string> groups;
    if(! r.match( url, & groups )) {
		return false;
	}

    protocol = groups[1];
    host     = groups[3];
    if (groups.size() > 4) path     = groups[4];
    return true;
}

void convertMessageDigestToLowercaseHex(
		const unsigned char * messageDigest,
		unsigned int mdLength,
		std::string & hexEncoded ) {
	AWSv4Impl::convertMessageDigestToLowercaseHex( messageDigest,
		mdLength, hexEncoded );
}


bool doSha256(	const std::string & payload,
				unsigned char * messageDigest,
				unsigned int * mdLength ) {
	return AWSv4Impl::doSha256( payload, messageDigest, mdLength );
}

std::string pathEncode( const std::string & original ) {
    return AWSv4Impl::pathEncode( original );
}

bool AmazonMetadataQuery::SendRequest( const std::string & uri ) {
	// Don't know what the meta-data server would do with anything else.
	httpVerb = "GET";
	// Spin the throttling engine up appropriately.
	Throttle::now( & signatureTime );
	// Send a "prepared" request (e.g., don't do any AWS security).
	return sendPreparedRequest( "http", uri, "" );
}

bool
fetchSecurityTokens( int rID, std::string & keyID, std::string & saKey, std::string & token ) {
	int requestID = -10 * rID;

	// (1) Determine the IAM role to use:
	//     * Execute `curl http://169.254.169.254/latest/meta-data/iam/security-credentials/`;
	//       the trailing slash is important. :(
	//     * If there's only one, use that one.  Otherwise, look for the
	//       'HTCondor' role and use that one.  Otherwise, fail.
	// (2) Execute `curl http://169.254.169.254/latest/meta-data/iam/security-credentials/<iam-role>`.
	// (3) Parse the JSON from step two for for "AccessKeyId",
	//     "SecretAccessKey", and "Token", assigning them appropriately.

	AmazonMetadataQuery listRequest( requestID--, "list-credentials" );
	std::string uri = "http://169.254.169.254/latest/meta-data/iam/security-credentials/";
	bool result = listRequest.SendRequest( uri );
	if(! result) {
		dprintf( D_ALWAYS, "fetchSecurityTokens(): Failed to fetch list of IAM roles (%s).\n", uri.c_str() );
		return false;
	}

	std::string listOfRoles = listRequest.getResultString();

	std::string role;
	std::vector<std::string> sl = split( listOfRoles, "\n" );
	switch( sl.size() ) {
		case 0:
			dprintf( D_ALWAYS, "fetchSecurityTokens(): Found empty list of roles.\n" );
			return false;
		case 1:
			role = sl[0];
			break;
		default:
			for (const auto& name : sl) {
				if( strstr( name.c_str(), "HTCondor" ) != NULL ) {
					role = name;
					break;
				}
			}
			break;
	}

	AmazonMetadataQuery roleRequest( requestID--, "fetch-credentials" );
	uri += role;
	result = roleRequest.SendRequest( uri );
	if(! result) {
		dprintf( D_ALWAYS, "fetchSecurityTokens(): Failed to fetch security tokens for role (%s).\n", uri.c_str() );
		return false;
	}

	ClassAd reply;
	classad::ClassAdJsonParser cajp;
	if(! cajp.ParseClassAd( roleRequest.getResultString(), reply, true )) {
		dprintf( D_ALWAYS, "fetchSecurityTokens(): Failed to parse reply '%s' as JSON.\n",
			roleRequest.getResultString().c_str() );
		return false;
	}

	// I think this ordering prevents short-circuiting.
	result = true;
	result = reply.LookupString( "AccessKeyId", keyID ) && result;
	result = reply.LookupString( "SecretAccessKey", saKey ) && result;
	result = reply.LookupString( "Token", token ) && result;
	return result;
}

bool AmazonRequest::createV4Signature(	const std::string & payload,
										std::string & authorizationValue,
										bool sendContentSHA ) {
	Throttle::now( & signatureTime );
	time_t now; time( & now );
	struct tm brokenDownTime; gmtime_r( & now, & brokenDownTime );
	dprintf( D_PERF_TRACE, "request #%d (%s): signature\n", requestID, requestCommand.c_str() );

	//
	// Create task 1's inputs.
	//

	// The canonical URI is the absolute path component of the service URL,
	// normalized according to RFC 3986 (removing redundant and relative
	// path components), with each path segment being URI-encoded.
	std::string protocol, host, canonicalURI;
	if(! parseURL( serviceURL, protocol, host, canonicalURI )) {
		this->errorCode = "E_INVALID_SERVICE_URL";
		this->errorMessage = "Failed to parse service URL.";
		dprintf( D_ALWAYS, "Failed to match regex against service URL '%s'.\n", serviceURL.c_str() );
		return false;
	}
	if( canonicalURI.empty() ) { canonicalURI = "/"; }

	// But that sounds like a lot of work, so until something we do actually
	// requires it, I'll just assume the path is already normalized.
	canonicalURI = pathEncode( canonicalURI );

	// The canonical query string is the alphabetically sorted list of
	// URI-encoded parameter names '=' values, separated by '&'s.  That
	// wouldn't be hard to do, but we don't need to, since we send
	// everything in the POST body, instead.
	std::string canonicalQueryString;

	// This function doesn't (currently) support query parameters,
	// but no current caller attempts to use them.
	ASSERT( (httpVerb != "GET") || query_parameters.size() == 0 );

	// The canonical headers must include the Host header, so add that
	// now if we don't have it.
	if( headers.find( "Host" ) == headers.end() ) {
		headers[ "Host" ] = host;
	}

	// If we're using temporary credentials, we need to add the token
	// header here as well.  We set saKey and keyID here (well before
	// necessary) since we'll get them for free when we get the token.
	std::string keyID;
	std::string saKey;
	std::string token;
	if( this->secretKeyFile == USE_INSTANCE_ROLE_MAGIC_STRING || this->accessKeyFile == USE_INSTANCE_ROLE_MAGIC_STRING ) {
		if(! fetchSecurityTokens( requestID, keyID, saKey, token ) ) {
			this->errorCode = "E_FILE_IO";
			this->errorMessage = "Unable to obtain role credentials'.";
			dprintf( D_ALWAYS, "Unable to obtain role credentials, failing.\n" );
			return false;
		}
		headers[ "X-Amz-Security-Token" ] = token;
	} else {
		if( ! htcondor::readShortFile( this->secretKeyFile, saKey ) ) {
			this->errorCode = "E_FILE_IO";
			this->errorMessage = "Unable to read from secretkey file '" + this->secretKeyFile + "'.";
			dprintf( D_ALWAYS, "Unable to read secretkey file '%s', failing.\n", this->secretKeyFile.c_str() );
			return false;
		}
		trim( saKey );

		if( ! htcondor::readShortFile( this->accessKeyFile, keyID ) ) {
			this->errorCode = "E_FILE_IO";
			this->errorMessage = "Unable to read from accesskey file '" + this->accessKeyFile + "'.";
			dprintf( D_ALWAYS, "Unable to read accesskey file '%s', failing.\n", this->accessKeyFile.c_str() );
			return false;
		}
		trim( keyID );
	}

	// S3 complains if x-amz-date isn't signed, so do this early.
	char dt[] = "YYYYMMDDThhmmssZ";
	strftime( dt, sizeof(dt), "%Y%m%dT%H%M%SZ", & brokenDownTime );
	headers[ "X-Amz-Date" ] = dt;

	char d[] = "YYYYMMDD";
	strftime( d, sizeof(d), "%Y%m%d", & brokenDownTime );

	// S3 complains if x-amz-content-sha256 isn't signed, which makes sense,
	// so do this early.

	// The canonical payload hash is the lowercase hexadecimal string of the
	// (SHA256) hash value of the payload.
	unsigned int mdLength = 0;
	unsigned char messageDigest[EVP_MAX_MD_SIZE];
	if(! doSha256( payload, messageDigest, & mdLength )) {
		this->errorCode = "E_INTERNAL";
		this->errorMessage = "Unable to hash payload.";
		dprintf( D_ALWAYS, "Unable to hash payload, failing.\n" );
		return false;
	}
	std::string payloadHash;
	convertMessageDigestToLowercaseHex( messageDigest, mdLength, payloadHash );
	if( sendContentSHA ) {
		headers[ "x-amz-content-sha256" ] = payloadHash;
	}

	// The canonical list of headers is a sorted list of lowercase header
	// names paired via ':' with the trimmed header value, each pair
	// terminated with a newline.
	AmazonRequest::AttributeValueMap transformedHeaders;
	for( auto i = headers.begin(); i != headers.end(); ++i ) {
		std::string header = i->first;
		std::transform( header.begin(), header.end(), header.begin(), & tolower );

		std::string value = i->second;
		// We need to leave empty headers alone so that they can be used
		// to disable CURL stupidity later.
		if( value.size() == 0 ) {
			continue;
		}

		// Eliminate trailing spaces.
		unsigned j = value.length() - 1;
		while( value[j] == ' ' ) { --j; }
		if( j != value.length() - 1 ) { value.erase( j + 1 ); }

		// Eliminate leading spaces.
		for( j = 0; value[j] == ' '; ++j ) { }
		value.erase( 0, j );

		// Convert internal runs of spaces into single spaces.
		unsigned left = 1;
		unsigned right = 1;
		bool inSpaces = false;
		while( right < value.length() ) {
			if(! inSpaces) {
				if( value[right] == ' ' ) {
					inSpaces = true;
					left = right;
					++right;
				} else {
					++right;
				}
			} else {
				if( value[right] == ' ' ) {
					++right;
				} else {
					inSpaces = false;
					value.erase( left, right - left - 1 );
					right = left + 1;
				}
			}
		}

		transformedHeaders[ header ] = value;
	}

	// The canonical list of signed headers is trivial to generate while
	// generating the list of headers.
	std::string signedHeaders;
	std::string canonicalHeaders;
	for( auto i = transformedHeaders.begin(); i != transformedHeaders.end(); ++i ) {
		canonicalHeaders += i->first + ":" + i->second + "\n";
		signedHeaders += i->first + ";";
	}
	signedHeaders.erase( signedHeaders.end() - 1 );
	// dprintf( D_ALWAYS, "signedHeaders: '%s'\n", signedHeaders.c_str() );
	// dprintf( D_ALWAYS, "canonicalHeaders: '%s'.\n", canonicalHeaders.c_str() );

	// Task 1: create the canonical request.
	std::string canonicalRequest = httpVerb + "\n"
								 + canonicalURI + "\n"
								 + canonicalQueryString + "\n"
								 + canonicalHeaders + "\n"
								 + signedHeaders + "\n"
								 + payloadHash;
	dprintf( D_SECURITY | D_VERBOSE, "canonicalRequest:\n%s\n", canonicalRequest.c_str() );


	//
	// Create task 2's inputs.
	//

	// Hash the canonical request the way we did the payload.
	if(! doSha256( canonicalRequest, messageDigest, & mdLength )) {
		this->errorCode = "E_INTERNAL";
		this->errorMessage = "Unable to hash canonical request.";
		dprintf( D_ALWAYS, "Unable to hash canonical request, failing.\n" );
		return false;
	}
	std::string canonicalRequestHash;
	convertMessageDigestToLowercaseHex( messageDigest, mdLength, canonicalRequestHash );

	std::string s = this->service;
	if( s.empty() ) {
		size_t i = host.find( "." );
		if( i != std::string::npos ) {
			s = host.substr( 0, i );
		} else {
			dprintf( D_ALWAYS, "Could not derive service from host '%s'; using host name as service name for testing purposes.\n", host.c_str() );
			s = host;
		}
	}

	std::string r = this->region;
	if( r.empty() ) {
		size_t i = host.find( "." );
		size_t j = host.find( ".", i + 1 );
		if( j != std::string::npos ) {
			r = host.substr( i + 1, j - i - 1 );
		} else {
			dprintf( D_ALWAYS, "Could not derive region from host '%s'; using host name as region name for testing purposes.\n", host.c_str() );
			r = host;
		}
	}


	// Task 2: create the string to sign.
	std::string credentialScope;
	formatstr( credentialScope, "%s/%s/%s/aws4_request", d, r.c_str(), s.c_str() );
	std::string stringToSign;
	formatstr( stringToSign, "AWS4-HMAC-SHA256\n%s\n%s\n%s",
		dt, credentialScope.c_str(), canonicalRequestHash.c_str() );
	dprintf( D_SECURITY | D_VERBOSE, "string to sign:\n%s\n", stringToSign.c_str() );


	//
	// Creating task 3's inputs was done when we checked to see if we needed
	// to get the security token, since they come along for free when we do.
	//

	// Task 3: calculate the signature.
	saKey = "AWS4" + saKey;
	const unsigned char * hmac = HMAC( EVP_sha256(), saKey.c_str(), saKey.length(),
		(unsigned char *)d, sizeof(d) - 1,
		messageDigest, & mdLength );
	if( hmac == NULL ) { return false; }

	unsigned int md2Length = 0;
	unsigned char messageDigest2[EVP_MAX_MD_SIZE];
	hmac = HMAC( EVP_sha256(), messageDigest, mdLength,
		(const unsigned char *)r.c_str(), r.length(), messageDigest2, & md2Length );
	if( hmac == NULL ) { return false; }

	hmac = HMAC( EVP_sha256(), messageDigest2, md2Length,
		(const unsigned char *)s.c_str(), s.length(), messageDigest, & mdLength );
	if( hmac == NULL ) { return false; }

	const char c[] = "aws4_request";
	hmac = HMAC( EVP_sha256(), messageDigest, mdLength,
		(const unsigned char *)c, sizeof(c) - 1, messageDigest2, & md2Length );
	if( hmac == NULL ) { return false; }

	hmac = HMAC( EVP_sha256(), messageDigest2, md2Length,
		(const unsigned char *)stringToSign.c_str(), stringToSign.length(),
		messageDigest, & mdLength );
	if( hmac == NULL ) { return false; }

	std::string signature;
	convertMessageDigestToLowercaseHex( messageDigest, mdLength, signature );

	formatstr( authorizationValue, "AWS4-HMAC-SHA256 Credential=%s/%s,"
				" SignedHeaders=%s, Signature=%s",
				keyID.c_str(), credentialScope.c_str(),
				signedHeaders.c_str(), signature.c_str() );
	// dprintf( D_ALWAYS, "authorization value: '%s'\n", authorizationValue.c_str() );
	return true;
}

bool AmazonRequest::sendV4Request( const std::string & payload, bool sendContentSHA ) {
    std::string protocol, host, path;
    if(! parseURL( serviceURL, protocol, host, path )) {
        this->errorCode = "E_INVALID_SERVICE_URL";
        this->errorMessage = "Failed to parse service URL.";
        dprintf( D_ALWAYS, "Failed to match regex against service URL '%s'.\n", serviceURL.c_str() );
        return false;
    }
    if( (protocol != "http") && (protocol != "https") ) {
        this->errorCode = "E_INVALID_SERVICE_URL";
        this->errorMessage = "Service URL not of a known protocol (http[s]).";
        dprintf( D_ALWAYS, "Service URL '%s' not of a known protocol (http[s]).\n", serviceURL.c_str() );
        return false;
    }

    dprintf( D_FULLDEBUG, "Request URI is '%s'\n", serviceURL.c_str() );
    if(! sendContentSHA) {
    	dprintf( D_FULLDEBUG, "Payload is '%s'\n", payload.c_str() );
    }

    std::string authorizationValue;
    if(! createV4Signature( payload, authorizationValue, sendContentSHA )) {
        if( this->errorCode.empty() ) { this->errorCode = "E_INTERNAL"; }
        if( this->errorMessage.empty() ) { this->errorMessage = "Failed to create v4 signature."; }
        dprintf( D_ALWAYS, "Failed to create v4 signature.\n" );
        return false;
    }
    headers[ "Authorization" ] = authorizationValue;

    return sendPreparedRequest( protocol, serviceURL, payload );
}

bool AmazonRequest::sendV2Request() {
    //
    // Every request must have the following parameters:
    //
    //      Action, Version, AWSAccessKeyId, Timestamp (or Expires),
    //      Signature, SignatureMethod, and SignatureVersion.
    //

    if( query_parameters.find( "Action" ) == query_parameters.end() ) {
        this->errorCode = "E_INTERNAL";
        this->errorMessage = "No action specified in request.";
        dprintf( D_ALWAYS, "No action specified in request, failing.\n" );
        return false;
    }

    // We need to know right away if we're doing FermiLab-style authentication.
    //
    // While we're at it, extract "the value of the Host header in lowercase"
    // and the "HTTP Request URI" from the service URL.  The service URL must
    // be of the form '[http[s]|x509|euca3[s]]://hostname[:port][/path]*'.
    std::string protocol, host, httpRequestURI;
    if(! parseURL( serviceURL, protocol, host, httpRequestURI )) {
        this->errorCode = "E_INVALID_SERVICE_URL";
        this->errorMessage = "Failed to parse service URL.";
        dprintf( D_ALWAYS, "Failed to match regex against service URL '%s'.\n", serviceURL.c_str() );
        return false;
    }

    if( (protocol != "http" && protocol != "https" && protocol != "x509" && protocol != "euca3" && protocol != "euca3s" ) ) {
        this->errorCode = "E_INVALID_SERVICE_URL";
        this->errorMessage = "Service URL not of a known protocol (http[s]|x509|euca3[s]).";
        dprintf( D_ALWAYS, "Service URL '%s' not of a known protocol (http[s]|x509|euca3[s]).\n", serviceURL.c_str() );
        return false;
    }
    std::string hostAndPath = host + httpRequestURI;
    std::transform( host.begin(), host.end(), host.begin(), & tolower );
    if( httpRequestURI.empty() ) { httpRequestURI = "/"; }

    //
    // Eucalyptus 3 bombs if it sees this attribute.
    //
    if( protocol == "euca3" || protocol == "euca3s" ) {
        query_parameters.erase( "InstanceInitiatedShutdownBehavior" );
    }

    //
    // The AWSAccessKeyId is just the contents of this->accessKeyFile,
    // and are (currently) 20 characters long.
    //
    std::string keyID;
    if( protocol != "x509" ) {
        if( ! htcondor::readShortFile( this->accessKeyFile, keyID ) ) {
            this->errorCode = "E_FILE_IO";
            this->errorMessage = "Unable to read from accesskey file '" + this->accessKeyFile + "'.";
            dprintf( D_ALWAYS, "Unable to read accesskey file '%s', failing.\n", this->accessKeyFile.c_str() );
            return false;
        }
        trim( keyID );
        query_parameters.insert( std::make_pair( "AWSAccessKeyId", keyID ) );
    }

    //
    // This implementation computes signature version 2,
    // using the "HmacSHA256" method.
    //
    query_parameters.insert( std::make_pair( "SignatureVersion", "2" ) );
    query_parameters.insert( std::make_pair( "SignatureMethod", "HmacSHA256" ) );

    //
    // We're calculating the signature now. [YYYY-MM-DDThh:mm:ssZ]
    //
    Throttle::now( & signatureTime );
    time_t now; time( & now );
    struct tm brokenDownTime; gmtime_r( & now, & brokenDownTime );
    char iso8601[] = "YYYY-MM-DDThh:mm:ssZ";
    strftime( iso8601, 20, "%Y-%m-%dT%H:%M:%SZ", & brokenDownTime );
    query_parameters.insert( std::make_pair( "Timestamp", iso8601 ) );
    dprintf( D_PERF_TRACE, "request #%d (%s): signature\n", requestID, requestCommand.c_str() );


    /*
     * The tricky party of sending a Query API request is calculating
     * the signature.  See
     *
     * http://docs.amazonwebservices.com/AWSEC2/2010-11-15/DeveloperGuide/using-query-api.html
     *
     */

    // Step 1: Create the canonicalized query string.
    std::string canonicalQueryString = canonicalizeQueryString();

    // Step 2: Create the string to sign.
    std::string stringToSign = "POST\n"
                             + host + "\n"
                             + httpRequestURI + "\n"
                             + canonicalQueryString;

    // Step 3: "Calculate an RFC 2104-compliant HMAC with the string
    // you just created, your Secret Access Key as the key, and SHA256
    // or SHA1 as the hash algorithm."
    std::string saKey;
    if( protocol == "x509" ) {
        // If we ever support the UploadImage action, we'll need to
        // extract the DN from the user's certificate here.  Otherwise,
        // since the x.509 implementation ignores the AWSAccessKeyId
        // and Signature, we can do whatever we want.
        saKey = std::string( "not-the-DN" );
        dprintf( D_FULLDEBUG, "Using '%s' as secret key for x.509\n", saKey.c_str() );
    } else {
        if( ! htcondor::readShortFile( this->secretKeyFile, saKey ) ) {
            this->errorCode = "E_FILE_IO";
            this->errorMessage = "Unable to read from secretkey file '" + this->secretKeyFile + "'.";
            dprintf( D_ALWAYS, "Unable to read secretkey file '%s', failing.\n", this->secretKeyFile.c_str() );
            return false;
        }
        trim( saKey );
    }

    unsigned int mdLength = 0;
    unsigned char messageDigest[EVP_MAX_MD_SIZE];
    const unsigned char * hmac = HMAC( EVP_sha256(), saKey.c_str(), saKey.length(),
        (const unsigned char *)stringToSign.c_str(), stringToSign.length(), messageDigest, & mdLength );
    if( hmac == NULL ) {
        this->errorCode = "E_INTERNAL";
        this->errorMessage = "Unable to calculate query signature (SHA256 HMAC).";
        dprintf( D_ALWAYS, "Unable to calculate SHA256 HMAC to sign query, failing.\n" );
        return false;
    }

    // Step 4: "Convert the resulting value to base64."
    char * base64Encoded = condor_base64_encode( messageDigest, mdLength );
    std::string signatureInBase64 = base64Encoded;
    free( base64Encoded );

    // Generate the final URI.
    canonicalQueryString += "&Signature=" + amazonURLEncode( signatureInBase64 );
    std::string postURI;
    if( protocol == "x509" ) {
        postURI = "https://" + hostAndPath;
    } else if( protocol == "euca3" ) {
        postURI = "http://" + hostAndPath;
    } else if( protocol == "euca3s" ) {
        postURI = "https://" + hostAndPath;
    } else {
        postURI = this->serviceURL;
    }
    dprintf( D_FULLDEBUG, "Request URI is '%s'\n", postURI.c_str() );

    // The AWS docs now say that " " - > "%20", and that "+" is an error.
    size_t index = canonicalQueryString.find( "AWSAccessKeyId=" );
    if( index != std::string::npos ) {
        size_t skipLast = canonicalQueryString.find( "&", index + 14 );
        char swap = canonicalQueryString[ index + 15 ];
        canonicalQueryString[ index + 15 ] = '\0';
        char const * cqs = canonicalQueryString.c_str();
        if( skipLast == std::string::npos ) {
            dprintf( D_FULLDEBUG, "Post body is '%s...'\n", cqs );
        } else {
            dprintf( D_FULLDEBUG, "Post body is '%s...%s'\n", cqs, cqs + skipLast );
        }
        canonicalQueryString[ index + 15 ] = swap;
    } else {
        dprintf( D_FULLDEBUG, "Post body is '%s'\n", canonicalQueryString.c_str() );
    }

    return sendPreparedRequest( protocol, postURI, canonicalQueryString );
}

bool AmazonRequest::SendURIRequest() {
    httpVerb = "GET";
    std::string noPayloadAllowed;
    return sendV4Request( noPayloadAllowed );
}

bool AmazonRequest::SendJSONRequest( const std::string & payload ) {
    headers[ "Content-Type" ] = "application/x-amz-json-1.1";
    return sendV4Request( payload );
}

// It's stated in the API documentation that you can upload to any region
// via us-east-1, which is moderately crazy.
bool AmazonRequest::SendS3Request( const std::string & payload ) {
	headers[ "Content-Type" ] = "binary/octet-stream";
	std::string contentLength; formatstr( contentLength, "%zu", payload.size() );
	headers[ "Content-Length" ] = contentLength;
	// Another undocumented CURL feature: transfer-encoding is "chunked"
	// by default for "PUT", which we really don't want.
	headers[ "Transfer-Encoding" ] = "";
	service = "s3";
	if( region.empty() ) {
		region = "us-east-1";
	}
	return sendV4Request( payload, true );
}

int
debug_callback( CURL *, curl_infotype ci, char * data, size_t size, void * ) {
	switch( ci ) {
		default:
			dprintf( D_ALWAYS, "debug_callback( unknown )\n" );
			break;

		case CURLINFO_TEXT:
			dprintf( D_ALWAYS, "debug_callback( TEXT ): '%*s'\n", (int)size, data );
			break;

		case CURLINFO_HEADER_IN:
			dprintf( D_ALWAYS, "debug_callback( HEADER_IN ): '%*s'\n", (int)size, data );
			break;

		case CURLINFO_HEADER_OUT:
			dprintf( D_ALWAYS, "debug_callback( HEADER_IN ): '%*s'\n", (int)size, data );
			break;

		case CURLINFO_DATA_IN:
			dprintf( D_ALWAYS, "debug_callback( DATA_IN )\n" );
			break;

		case CURLINFO_DATA_OUT:
			dprintf( D_ALWAYS, "debug_callback( DATA_OUT )\n" );
			break;

		case CURLINFO_SSL_DATA_IN:
			dprintf( D_ALWAYS, "debug_callback( SSL_DATA_IN )\n" );
			break;

		case CURLINFO_SSL_DATA_OUT:
			dprintf( D_ALWAYS, "debug_callback( SSL_DATA_OUT )\n" );
			break;
	}

	return 0;
}

size_t
read_callback( char * buffer, size_t size, size_t n, void * v ) {
	// This can be static because only one curl_easy_perform() can be
	// running at a time.
	static size_t sentSoFar = 0;
	std::string * payload = (std::string *)v;

	if( sentSoFar == payload->size() ) {
		// dprintf( D_ALWAYS, "read_callback(): resetting sentSoFar.\n" );
		sentSoFar = 0;
		return 0;
	}

	size_t request = size * n;
	if( request > payload->size() ) { request = payload->size(); }

	if( sentSoFar + request > payload->size() ) {
		request = payload->size() - sentSoFar;
	}

	// dprintf( D_ALWAYS, "read_callback(): sending %lu (sent %lu already).\n", request, sentSoFar );
	memcpy( buffer, payload->data() + sentSoFar, request );
	sentSoFar += request;

	return request;
}

bool AmazonRequest::sendPreparedRequest(
        const std::string & protocol,
        const std::string & uri,
        const std::string & payload ) {
    static bool rateLimitInitialized = false;
    if(! rateLimitInitialized) {
        // FIXME: convert to the new form of param() when it becomes available.
        std::string rateLimit;
        formatstr( rateLimit, "%s_RATE_LIMIT", get_mySubSystem()->getName() );
        globalCurlThrottle.rateLimit = param_integer( rateLimit.c_str(), 100 );
        dprintf( D_PERF_TRACE, "rate limit = %d\n", globalCurlThrottle.rateLimit );
        rateLimitInitialized = true;
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

    std::unique_ptr<CURL,decltype(&curl_easy_cleanup)> curl(curl_easy_init(), &curl_easy_cleanup);

    if( curl.get() == NULL ) {
        this->errorCode = "E_CURL_LIB";
        this->errorMessage = "curl_easy_init() failed.";
        dprintf( D_ALWAYS, "curl_easy_init() failed, failing.\n" );
        return false;
    }

    char errorBuffer[CURL_ERROR_SIZE];
    rv = curl_easy_setopt( curl.get(), CURLOPT_ERRORBUFFER, errorBuffer );
    if( rv != CURLE_OK ) {
        this->errorCode = "E_CURL_LIB";
        this->errorMessage = "curl_easy_setopt( CURLOPT_ERRORBUFFER ) failed.";
        dprintf( D_ALWAYS, "curl_easy_setopt( CURLOPT_ERRORBUFFER ) failed (%d): '%s', failing.\n",
            rv, curl_easy_strerror( rv ) );
        return false;
    }


/*
    rv = curl_easy_setopt( curl.get(), CURLOPT_DEBUGFUNCTION, debug_callback );
    if( rv != CURLE_OK ) {
        this->errorCode = "E_CURL_LIB";
        this->errorMessage = "curl_easy_setopt( CURLOPT_DEBUGFUNCTION ) failed.";
        dprintf( D_ALWAYS, "curl_easy_setopt( CURLOPT_DEBUGFUNCTION ) failed (%d): '%s', failing.\n",
            rv, curl_easy_strerror( rv ) );
        return false;
    }

    // CURLOPT_DEBUGFUNCTION does nothing without CURLOPT_DEBUG set.
    rv = curl_easy_setopt( curl.get(), CURLOPT_VERBOSE, 1 );
    if( rv != CURLE_OK ) {
        this->errorCode = "E_CURL_LIB";
        this->errorMessage = "curl_easy_setopt( CURLOPT_VERBOSE ) failed.";
        dprintf( D_ALWAYS, "curl_easy_setopt( CURLOPT_VERBOSE ) failed (%d): '%s', failing.\n",
            rv, curl_easy_strerror( rv ) );
        return false;
    }
*/


    // dprintf( D_ALWAYS, "sendPreparedRequest(): CURLOPT_URL = '%s'\n", uri.c_str() );
    rv = curl_easy_setopt( curl.get(), CURLOPT_URL, uri.c_str() );
    if( rv != CURLE_OK ) {
        this->errorCode = "E_CURL_LIB";
        this->errorMessage = "curl_easy_setopt( CURLOPT_URL ) failed.";
        dprintf( D_ALWAYS, "curl_easy_setopt( CURLOPT_URL ) failed (%d): '%s', failing.\n",
            rv, curl_easy_strerror( rv ) );
        return false;
    }

	if( httpVerb == "POST" ) {
		rv = curl_easy_setopt( curl.get(), CURLOPT_POST, 1 );
		if( rv != CURLE_OK ) {
			this->errorCode = "E_CURL_LIB";
			this->errorMessage = "curl_easy_setopt( CURLOPT_POST ) failed.";
			dprintf( D_ALWAYS, "curl_easy_setopt( CURLOPT_POST ) failed (%d): '%s', failing.\n",
				rv, curl_easy_strerror( rv ) );
			return false;
		}

		// dprintf( D_ALWAYS, "sendPreparedRequest(): CURLOPT_POSTFIELDS = '%s'\n", payload.c_str() );
		rv = curl_easy_setopt( curl.get(), CURLOPT_POSTFIELDS, payload.c_str() );
		if( rv != CURLE_OK ) {
			this->errorCode = "E_CURL_LIB";
			this->errorMessage = "curl_easy_setopt( CURLOPT_POSTFIELDS ) failed.";
			dprintf( D_ALWAYS, "curl_easy_setopt( CURLOPT_POSTFIELDS ) failed (%d): '%s', failing.\n",
				rv, curl_easy_strerror( rv ) );
			return false;
		}
	}

	if( httpVerb == "PUT" ) {
		rv = curl_easy_setopt( curl.get(), CURLOPT_UPLOAD, 1 );
		if( rv != CURLE_OK ) {
			this->errorCode = "E_CURL_LIB";
			this->errorMessage = "curl_easy_setopt( CURLOPT_UPLOAD ) failed.";
			dprintf( D_ALWAYS, "curl_easy_setopt( CURLOPT_UPLOAD ) failed (%d): '%s', failing.\n",
				rv, curl_easy_strerror( rv ) );
			return false;
		}

		rv = curl_easy_setopt( curl.get(), CURLOPT_READDATA, & payload );
		if( rv != CURLE_OK ) {
			this->errorCode = "E_CURL_LIB";
			this->errorMessage = "curl_easy_setopt( CURLOPT_READDATA ) failed.";
			dprintf( D_ALWAYS, "curl_easy_setopt( CURLOPT_READDATA ) failed (%d): '%s', failing.\n",
				rv, curl_easy_strerror( rv ) );
			return false;
		}

		rv = curl_easy_setopt( curl.get(), CURLOPT_READFUNCTION, read_callback );
		if( rv != CURLE_OK ) {
			this->errorCode = "E_CURL_LIB";
			this->errorMessage = "curl_easy_setopt( CURLOPT_READFUNCTION ) failed.";
			dprintf( D_ALWAYS, "curl_easy_setopt( CURLOPT_READFUNCTION ) failed (%d): '%s', failing.\n",
				rv, curl_easy_strerror( rv ) );
			return false;
		}
	}

    rv = curl_easy_setopt( curl.get(), CURLOPT_NOPROGRESS, 1 );
    if( rv != CURLE_OK ) {
        this->errorCode = "E_CURL_LIB";
        this->errorMessage = "curl_easy_setopt( CURLOPT_NOPROGRESS ) failed.";
        dprintf( D_ALWAYS, "curl_easy_setopt( CURLOPT_NOPROGRESS ) failed (%d): '%s', failing.\n",
            rv, curl_easy_strerror( rv ) );
        return false;
    }

    if ( includeResponseHeader ) {
        rv = curl_easy_setopt( curl.get(), CURLOPT_HEADER, 1 );
        if( rv != CURLE_OK ) {
            this->errorCode = "E_CURL_LIB";
            this->errorMessage = "curl_easy_setopt( CURLOPT_HEADER ) failed.";
            dprintf( D_ALWAYS, "curl_easy_setopt( CURLOPT_HEADER ) failed (%d): '%s', failing.\n",
                     rv, curl_easy_strerror( rv ) );
            return false;
        }
    }

    rv = curl_easy_setopt( curl.get(), CURLOPT_WRITEFUNCTION, & appendToString );
    if( rv != CURLE_OK ) {
        this->errorCode = "E_CURL_LIB";
        this->errorMessage = "curl_easy_setopt( CURLOPT_WRITEFUNCTION ) failed.";
        dprintf( D_ALWAYS, "curl_easy_setopt( CURLOPT_WRITEFUNCTION ) failed (%d): '%s', failing.\n",
            rv, curl_easy_strerror( rv ) );
        return false;
    }

    rv = curl_easy_setopt( curl.get(), CURLOPT_WRITEDATA, & this->resultString );
    if( rv != CURLE_OK ) {
        this->errorCode = "E_CURL_LIB";
        this->errorMessage = "curl_easy_setopt( CURLOPT_WRITEDATA ) failed.";
        dprintf( D_ALWAYS, "curl_easy_setopt( CURLOPT_WRITEDATA ) failed (%d): '%s', failing.\n",
            rv, curl_easy_strerror( rv ) );
        return false;
    }

    //
    // Set security options.
    //
    SET_CURL_SECURITY_OPTION( curl.get(), CURLOPT_SSL_VERIFYPEER, 1 );
    SET_CURL_SECURITY_OPTION( curl.get(), CURLOPT_SSL_VERIFYHOST, 2 );

    // NB: Contrary to libcurl's manual, it doesn't strdup() strings passed
    // to it, so they MUST remain in scope until after we call
    // curl_easy_cleanup().  Otherwise, curl_perform() will fail with
    // a completely bogus error, number 60, claiming that there's a
    // 'problem with the SSL CA cert'.
    std::string CAFile = "";
    std::string CAPath = "";

    char * x509_ca_dir = getenv( "X509_CERT_DIR" );
    if( x509_ca_dir != NULL ) {
        CAPath = x509_ca_dir;
    }

    char * x509_ca_file = getenv( "X509_CERT_FILE" );
    if( x509_ca_file != NULL ) {
        CAFile = x509_ca_file;
    }

    if( CAPath.empty() ) {
        char * soap_ssl_ca_dir = getenv( "GAHP_SSL_CADIR" );
        if( soap_ssl_ca_dir != NULL ) {
            CAPath = soap_ssl_ca_dir;
        }
    }

    if( CAFile.empty() ) {
        char * soap_ssl_ca_file = getenv( "GAHP_SSL_CAFILE" );
        if( soap_ssl_ca_file != NULL ) {
            CAFile = soap_ssl_ca_file;
        }
    }

    if( ! CAPath.empty() ) {
        dprintf( D_FULLDEBUG, "Setting CA path to '%s'\n", CAPath.c_str() );
        SET_CURL_SECURITY_OPTION( curl.get(), CURLOPT_CAPATH, CAPath.c_str() );
    }

    if( ! CAFile.empty() ) {
        dprintf( D_FULLDEBUG, "Setting CA file to '%s'\n", CAFile.c_str() );
        SET_CURL_SECURITY_OPTION( curl.get(), CURLOPT_CAINFO, CAFile.c_str() );
    }

    if( setenv( "OPENSSL_ALLOW_PROXY", "1", 0 ) != 0 ) {
        dprintf( D_FULLDEBUG, "Failed to set OPENSSL_ALLOW_PROXY.\n" );
    }

    //
    // Configure for x.509 operation.
    //
    if( protocol == "x509" ) {
        dprintf( D_FULLDEBUG, "Configuring x.509...\n" );

        SET_CURL_SECURITY_OPTION( curl.get(), CURLOPT_SSLKEYTYPE, "PEM" );
        SET_CURL_SECURITY_OPTION( curl.get(), CURLOPT_SSLKEY, this->secretKeyFile.c_str() );

        SET_CURL_SECURITY_OPTION( curl.get(), CURLOPT_SSLCERTTYPE, "PEM" );
        SET_CURL_SECURITY_OPTION( curl.get(), CURLOPT_SSLCERT, this->accessKeyFile.c_str() );
    }


	std::string headerPair;
	struct curl_slist * header_slist = NULL;
	for( auto i = headers.begin(); i != headers.end(); ++i ) {
		formatstr( headerPair, "%s: %s", i->first.c_str(), i->second.c_str() );
		// dprintf( D_FULLDEBUG, "sendPreparedRequest(): adding header = '%s: %s'\n", i->first.c_str(), i->second.c_str() );
		header_slist = curl_slist_append( header_slist, headerPair.c_str() );
		if( header_slist == NULL ) {
			this->errorCode = "E_CURL_LIB";
			this->errorMessage = "curl_slist_append() failed.";
			dprintf( D_ALWAYS, "curl_slist_append() failed, failing.\n" );
			return false;
		}
	}

	rv = curl_easy_setopt( curl.get(), CURLOPT_HTTPHEADER, header_slist );
	if( rv != CURLE_OK ) {
		this->errorCode = "E_CURL_LIB";
		this->errorMessage = "curl_easy_setopt( CURLOPT_HTTPHEADER ) failed.";
		dprintf( D_ALWAYS, "curl_easy_setopt( CURLOPT_HTTPHEADER ) failed (%d): '%s', failing.\n",
			rv, curl_easy_strerror( rv ) );
		if( header_slist ) { curl_slist_free_all( header_slist ); }
		return false;
	}


    static unsigned failureCount = 0;
    const char * failureMode = getenv( "EC2_GAHP_FAILURE_MODE" );

    dprintf( D_PERF_TRACE, "request #%d (%s): ready to call %s\n", requestID, requestCommand.c_str(), query_parameters[ "Action" ].c_str() );
    ++NumDistinctRequests;

    Throttle::now( & this->mutexReleased );
    amazon_gahp_release_big_mutex();
    pthread_mutex_lock( & globalCurlMutex );
    Throttle::now( & this->lockGained );

    // We don't check the deadline after the retry because limitExceeded()
    // already checks.  (limitNotExceeded() does not, but if we call that
    // then the request has succeeded and we won't be retrying.)
    globalCurlThrottle.setDeadline( signatureTime, 300 );
    struct timespec liveline = globalCurlThrottle.getWhen();
    struct timespec deadline = globalCurlThrottle.getDeadline();
    if( Throttle::difference( & liveline, & deadline ) < 0 ) {
        amazon_gahp_grab_big_mutex();
        dprintf( D_PERF_TRACE, "request #%d (%s): deadline would be exceeded\n", requestID, requestCommand.c_str() );
        failureCount = 0;
        ++NumExpiredSignatures;

        this->errorCode = "E_DEADLINE_WOULD_BE_EXCEEDED";
        this->errorMessage = "Signature would have expired before next permissible time to use it.";
        if( header_slist ) { curl_slist_free_all( header_slist ); }

        pthread_mutex_unlock( & globalCurlMutex );
        return false;
    }

retry:
    this->liveLine = globalCurlThrottle.getWhen();
    Throttle::now( & this->sleepBegan );
    globalCurlThrottle.sleepIfNecessary();
    Throttle::now( & this->sleepEnded );

    Throttle::now( & this->requestBegan );
    if( failureMode == NULL ) {
        rv = curl_easy_perform( curl.get() );
    } else if( strcmp( failureMode, "0" ) == 0 ) {
        rv = curl_easy_perform( curl.get() );
    } else {
        switch( failureMode[0] ) {
            default:
                rv = curl_easy_perform( curl.get() );
                break;

            case '1':
                rv = CURLE_OK;
                break;

            case '2':
                if( requestCommand != "EC2_VM_STATUS_ALL" &&
                     requestCommand != "EC2_VM_STATUS_ALL_SPOT" &&
                      requestCommand != "EC2_VM_SERVER_TYPE" ) {
                    rv = CURLE_OK;
                } else {
                    rv = curl_easy_perform( curl.get() );
                }
                break;

            case '3':
                if( requestCommand != "EC2_VM_STATUS_ALL" &&
                     requestCommand != "EC2_VM_STATUS_ALL_SPOT" &&
                      requestCommand != "EC2_VM_SERVER_TYPE" ) {
                    if( failureCount < 3 && requestID % 2 ) {
                        rv = CURLE_OK;
                    } else {
                        rv = curl_easy_perform( curl.get() );
                    }
                } else {
                    rv = curl_easy_perform( curl.get() );
                }
                break;

            case '4':
                if( requestCommand != "EC2_VM_STOP_SPOT" &&
                     requestCommand != "EC2_VM_STOP" ) {
                    rv = CURLE_OK;
                } else {
                    rv = curl_easy_perform( curl.get() );
                }
                break;
        }
    }
    Throttle::now( & this->requestEnded );

    amazon_gahp_grab_big_mutex();
    Throttle::now( & this->mutexGained );
    ++NumRequests;

    dprintf( D_PERF_TRACE, "request #%d (%s): called %s\n", requestID, requestCommand.c_str(), query_parameters[ "Action" ].c_str() );
    dprintf( D_PERF_TRACE | D_VERBOSE,
        "request #%d (%s): "
        "scheduling delay (release mutex, grab lock): %ld ms; "
        "cumulative delay time: %ld ms; "
        "last delay time: %ld ms "
        "request time: %ld ms; "
        "grab mutex: %ld ms\n",
        requestID, requestCommand.c_str(),
        Throttle::difference( & this->mutexReleased, & this->lockGained ),
        Throttle::difference( & this->lockGained, & this->requestBegan ),
        Throttle::difference( & this->sleepBegan, & this->sleepEnded ),
        Throttle::difference( & this->requestBegan, & this->requestEnded ),
        Throttle::difference( & this->requestEnded, & this->mutexGained )
    );

    if( rv != 0 ) {
        // We'll be very conservative here, and set the next liveline as if
        // this request had exceeded the server's rate limit, which will also
        // set the client-side rate limit if that's larger.  Since we're
        // already terminally failing the request, don't bother to check if
        // this was our last chance at retrying.
        globalCurlThrottle.limitExceeded();

        this->errorCode = "E_CURL_IO";
        std::ostringstream error;
        error << "curl_easy_perform() failed (" << rv << "): '" << curl_easy_strerror( rv ) << "'.";
        this->errorMessage = error.str();
        dprintf( D_ALWAYS, "%s\n", this->errorMessage.c_str() );
        dprintf( D_FULLDEBUG, "%s\n", errorBuffer );
        if( header_slist ) { curl_slist_free_all( header_slist ); }

        pthread_mutex_unlock( & globalCurlMutex );
        return false;
    }

    responseCode = 0;
    rv = curl_easy_getinfo( curl.get(), CURLINFO_RESPONSE_CODE, & responseCode );
    if( rv != CURLE_OK ) {
        // So we contacted the server but it returned such gibberish that
        // CURL couldn't identify the response code.  Let's assume that's
        // bad news.  Since we're already terminally failing the request,
        // don't bother to check if this was our last chance at retrying.
        globalCurlThrottle.limitExceeded();

        this->errorCode = "E_CURL_LIB";
        this->errorMessage = "curl_easy_getinfo() failed.";
        dprintf( D_ALWAYS, "curl_easy_getinfo( CURLINFO_RESPONSE_CODE ) failed (%d): '%s', failing.\n",
            rv, curl_easy_strerror( rv ) );
        if( header_slist ) { curl_slist_free_all( header_slist ); }

        pthread_mutex_unlock( & globalCurlMutex );
        return false;
    }

    if( failureMode != NULL && (strcmp( failureMode, "1" ) == 0) ) {
        responseCode = 503;
        resultString = "FAILURE INJECTION: <Error><Code>RequestLimitExceeded</Code></Error>";
    }

    if( failureMode != NULL && (strcmp( failureMode, "2" ) == 0) ) {
        if( requestCommand != "EC2_VM_STATUS_ALL" &&
             requestCommand != "EC2_VM_STATUS_ALL_SPOT" &&
              requestCommand != "EC2_VM_SERVER_TYPE" ) {
            responseCode = 503;
            resultString = "FAILURE INJECTION: <Error><Code>RequestLimitExceeded</Code></Error>";
        }
    }

    if( failureMode != NULL && (strcmp( failureMode, "3" ) == 0) ) {
        if( requestCommand != "EC2_VM_STATUS_ALL" &&
             requestCommand != "EC2_VM_STATUS_ALL_SPOT" &&
              requestCommand != "EC2_VM_SERVER_TYPE" ) {
            if( failureCount < 3 && requestID % 2 ) {
                ++failureCount;
                responseCode = 503;
                resultString = "FAILURE INJECTION: <Error><Code>RequestLimitExceeded</Code></Error>";
            } else {
                failureCount = 0;
            }
        }
    }

    if( failureMode != NULL && (strcmp( failureMode, "4" ) == 0) ) {
        if( requestCommand != "EC2_VM_STOP_SPOT" &&
              requestCommand != "EC2_VM_STOP" ) {
            responseCode = 503;
            resultString = "FAILURE INJECTION: <Error><Code>RequestLimitExceeded</Code></Error>";
        }
    }

    if( responseCode == 503 && (resultString.find( "<Error><Code>RequestLimitExceeded</Code>" ) != std::string::npos) ) {
    	++NumRequestsExceedingLimit;
        if( globalCurlThrottle.limitExceeded() ) {
            dprintf( D_PERF_TRACE, "request #%d (%s): retry limit exceeded\n", requestID, requestCommand.c_str() );
        	failureCount = 0;

            // This should almost certainly be E_REQUEST_LIMIT_EXCEEDED, but
            // for now return the same error code for this condition that
            // we did before we recongized it.
            formatstr( this->errorCode, "E_HTTP_RESPONSE_NOT_200 (%lu)", responseCode );
            this->errorMessage = resultString;
	        if( header_slist ) { curl_slist_free_all( header_slist ); }

            pthread_mutex_unlock( & globalCurlMutex );
            return false;
        }

        dprintf( D_PERF_TRACE, "request #%d (%s): will retry\n", requestID, requestCommand.c_str() );
        resultString.clear();
        amazon_gahp_release_big_mutex();
        goto retry;
    } else {
        globalCurlThrottle.limitNotExceeded();
    }

    if( header_slist ) { curl_slist_free_all( header_slist ); }

    if( responseCode != 200 ) {
        formatstr( this->errorCode, "E_HTTP_RESPONSE_NOT_200 (%lu)", responseCode );
        this->errorMessage = resultString;
        if( this->errorMessage.empty() ) {
            formatstr( this->errorMessage, "HTTP response was %lu, not 200, and no body was returned.", responseCode );
        }
        dprintf( D_FULLDEBUG, "Query did not return 200 (%lu), failing.\n",
            responseCode );
        dprintf( D_FULLDEBUG, "Failure response text was '%s'.\n", resultString.c_str() );

        dprintf( D_PERF_TRACE, "request #%d (%s): call to %s returned %lu.\n", requestID, requestCommand.c_str(), query_parameters[ "Action" ].c_str(), responseCode );
        pthread_mutex_unlock( & globalCurlMutex );
        return false;
    }

    dprintf( D_FULLDEBUG, "Response was '%s'\n", resultString.c_str() );
    pthread_mutex_unlock( & globalCurlMutex );
    dprintf( D_PERF_TRACE, "request #%d (%s): call to %s returned 200 (OK).\n", requestID, requestCommand.c_str(), query_parameters[ "Action" ].c_str() );
    return true;
}

// ---------------------------------------------------------------------------

/*
 * The *[ESH|CDH|EEH] functions are Expat callbacks for parsing the result
 * of the corresponding query.  ('EntityStartHandler', 'CharacterDataHandler',
 * and 'EntityEndHandler', respectively; referring to the opening tag, the
 * enclosed data, and the closing tag.)  This method of XML parsing is
 * extremely opaque and bug-prone, because you have to represent the structure
 * of the XML in executable code (and the *UD ('UserData') datastructures).
 *
 * If the use of libxml2 is not contraindicated (it was at one point), then
 * we can use XPath expressions (regex-like) instead of code to identify
 * the character data of interest.  This will be particularly clarity-
 * enhancing when parsing the results of DescribeInstances queries; see
 * AmazonVMStatusAll.
 *
 * See 'http://xmlsoft.org/examples/xpath1.c', which looks a lot cleaner.
 */

/*
 * Some versions (installations?) of Eucalyptus 3 return valid XML which
 * places the tags we're expecting in the 'euca' namespace.  Rather than
 * handle namespaces in each entity name comparison, use the following
 * function to remove them.  If it becomes necessary, this function could
 * be expanded to learn about namespaces as they're entered and exited
 * and check that the namespace for any given entity name is, in fact,
 * that of the EC2 API.  (If not, just returning the whole string will
 * force the comparisons to fail.)
 */
const char * ignoringNameSpace( const XML_Char * entityName ) {
    const char * entityNameString = (const char *)entityName;
    const char * firstColon = strchr( entityNameString, ':' );
    if( firstColon ) { return ++firstColon; }
    else{ return entityNameString; }
}

/*
 * FIXME: None of the ::SendRequest() methods verify that the 200/OK
 * response they tried to parse contain(ed) the attribute(s) they
 * were looking for.  Although, strictly speaking, this is a server-
 * side problem, it seems prudent to check.
 *
 * In particular, this bug allows FermiLab's x509:// schema to work,
 * since their server doesn't presently handle keypairs.  Note, however,
 * that this severely limits (if not eliminates) the GridManager's
 * crash recovery.  Newer version of the EC2 API will allow us to
 * avoid this problem by using the ability to supply a name for the instance
 * directly.
 */

AmazonVMStart::~AmazonVMStart() { }

struct vmStartUD_t {
    bool inInstanceId;
    std::string & instanceID;
    std::vector< std::string > * instanceIDs;

    vmStartUD_t( std::string & iid, std::vector< std::string > * iids = NULL ) : inInstanceId( false ), instanceID( iid ), instanceIDs( iids ) { }
};
typedef struct vmStartUD_t vmStartUD;

void vmStartESH( void * vUserData, const XML_Char * name, const XML_Char ** ) {
    vmStartUD * vsud = (vmStartUD *)vUserData;
    if( strcasecmp( ignoringNameSpace( name ), "instanceId" ) == 0 ) {
        vsud->inInstanceId = true;
    }
}

void vmStartCDH( void * vUserData, const XML_Char * cdata, int len ) {
    vmStartUD * vsud = (vmStartUD *)vUserData;
    if( vsud->inInstanceId ) {
        appendToString( (const void *)cdata, len, 1, (void *) & vsud->instanceID );
    }
}

void vmStartEEH( void * vUserData, const XML_Char * name ) {
    vmStartUD * vsud = (vmStartUD *)vUserData;
    if( strcasecmp( ignoringNameSpace( name ), "instanceId" ) == 0 ) {
        vsud->inInstanceId = false;
        if( vsud->instanceIDs ) {
        	vsud->instanceIDs->push_back( vsud->instanceID );
        	vsud->instanceID.clear();
        }
    }
}

bool AmazonVMStart::SendRequest() {
    bool result = AmazonRequest::SendRequest();
    if ( result ) {
        vmStartUD vsud( this->instanceID, & this->instanceIDs );
        XML_Parser xp = XML_ParserCreate( NULL );
        XML_SetElementHandler( xp, & vmStartESH, & vmStartEEH );
        XML_SetCharacterDataHandler( xp, & vmStartCDH );
        XML_SetUserData( xp, & vsud );
        XML_Parse( xp, this->resultString.c_str(), this->resultString.length(), 1 );
        XML_ParserFree( xp );
    } else {
        if( this->errorCode == "E_CURL_IO" ) {
            // To be on the safe side, if the I/O failed, make the gridmanager
            // check to see the VM was started or not.
            this->errorCode = "NEED_CHECK_VM_START";
            return false;
        }
    }
    return result;
}

bool AmazonVMStart::workerFunction(char **argv, int argc, std::string &result_string) {
    assert( strcasecmp( argv[0], "EC2_VM_START" ) == 0 );

    // Uses the Query API function 'RunInstances', as documented at
    // http://docs.amazonwebservices.com/AWSEC2/latest/APIReference/ApiReference-query-RunInstances.html

    int requestID;
    get_int( argv[1], & requestID );
    dprintf( D_PERF_TRACE, "request #%d (%s): work begins\n",
        requestID, argv[0] );

    if( ! verify_min_number_args( argc, 21 ) ) {
        result_string = create_failure_result( requestID, "Wrong_Argument_Number" );
        dprintf( D_ALWAYS, "Wrong number of arguments (%d should be >= %d) to %s\n",
                 argc, 21, argv[0] );
        return false;
    }

    // Fill in required attributes.
    AmazonVMStart vmStartRequest = AmazonVMStart( requestID, argv[0] );
    vmStartRequest.serviceURL = argv[2];
    vmStartRequest.accessKeyFile = argv[3];
    vmStartRequest.secretKeyFile = argv[4];

    // Fill in user-specified parameters.  (Also handles security groups
    // and security group IDS.)
    //
    // We could have split up the block device mapping like this, but since
    // we were inventing a syntax anyway, it wasn't too hard to make the
    // parser simple.  Rather than risk a quoting problem with security
    // names or group IDs, we just introduce a convention that we terminate
    // each list with the null string.

    unsigned positionInList = 0;
    unsigned which = 0;
    for( int i = 18; i < argc; ++i ) {
        if( strcasecmp( argv[i], NULLSTRING ) == 0 ) {
            ++which;
            positionInList = 1;
            continue;
        }

        std::ostringstream parameterName;
        switch( which ) {
            case 0:
                parameterName << "SecurityGroup." << positionInList;
                ++positionInList;
                break;
            case 1:
                parameterName << "SecurityGroupId." << positionInList;
                ++positionInList;
                break;
            case 2:
                parameterName << argv[i];
                if( ! ( i + 1 < argc ) ) {
                    dprintf( D_ALWAYS, "Found parameter '%s' without value, ignoring it.\n", argv[i] );
                    continue;
                }
                ++i;
                break;
            default:
                dprintf( D_ALWAYS, "Found unexpected null strings at end of variable-count parameter lists.  Ignoring.\n" );
                continue;
        }

        vmStartRequest.query_parameters[ parameterName.str() ] = argv[ i ];
    }

    // Fill in required parameters.
    vmStartRequest.query_parameters[ "Action" ] = "RunInstances";
    vmStartRequest.query_parameters[ "ImageId" ] = argv[5];
    vmStartRequest.query_parameters[ "MinCount" ] = "1";
    vmStartRequest.query_parameters[ "MaxCount" ] = "1";
    vmStartRequest.query_parameters[ "InstanceInitiatedShutdownBehavior" ] = "terminate";

	if( strcasecmp( argv[17], NULLSTRING ) ) {
		vmStartRequest.query_parameters[ "MaxCount" ] = argv[17];
	}

    // Fill in optional parameters.
    if( strcasecmp( argv[6], NULLSTRING ) ) {
        vmStartRequest.query_parameters[ "KeyName" ] = argv[6];
    }

    if( strcasecmp( argv[9], NULLSTRING ) ) {
        vmStartRequest.query_parameters[ "InstanceType" ] = argv[9];
    }

    if( strcasecmp( argv[10], NULLSTRING ) ) {
        vmStartRequest.query_parameters[ "Placement.AvailabilityZone" ] = argv[10];
    }

    if( strcasecmp( argv[11], NULLSTRING ) ) {
        vmStartRequest.query_parameters[ "SubnetId" ] = argv[11];
    }

    if( strcasecmp( argv[12], NULLSTRING ) ) {
        vmStartRequest.query_parameters[ "PrivateIpAddress" ] = argv[12];
    }

    if( strcasecmp( argv[13], NULLSTRING ) ) {
        vmStartRequest.query_parameters[ "ClientToken" ] = argv[13];
    }

    if( strcasecmp( argv[14], NULLSTRING ) ) {
        // We can't pass an arbitrarily long list of block device mappings
        // because we're already using that to pass security group names.
		size_t i = 1;
        for (const auto& mapping : StringTokenIterator(argv[14])) {
            std::vector<std::string> pair = split(mapping, ":");
            if( pair.size() != 2 ) {
                dprintf( D_ALWAYS, "Ignoring invalid block device mapping '%s'.\n", mapping.c_str() );
                continue;
            }

            std::ostringstream virtualName;
            virtualName << "BlockDeviceMapping." << i << ".VirtualName";
            vmStartRequest.query_parameters[ virtualName.str() ] = pair[0];

            std::ostringstream deviceName;
            deviceName << "BlockDeviceMapping." << i << ".DeviceName";
            vmStartRequest.query_parameters[ deviceName.str() ] = pair[1];
			i++;
        }
    }

    if( strcasecmp( argv[15], NULLSTRING ) ) {
        vmStartRequest.query_parameters[ "IamInstanceProfile.Arn" ] = argv[15];
    }

    if( strcasecmp( argv[16], NULLSTRING ) ) {
        vmStartRequest.query_parameters[ "IamInstanceProfile.Name" ] = argv[16];
    }


    //
    // Handle user data.
    //
    std::string buffer;
    if( strcasecmp( argv[7], NULLSTRING ) ) {
        buffer = argv[7];
    }
    if( strcasecmp( argv[8], NULLSTRING ) ) {
        std::string udFileName = argv[8];
        std::string udFileContents;
        if( ! htcondor::readShortFile( udFileName, udFileContents ) ) {
            result_string = create_failure_result( requestID, "Failed to read userdata file.", "E_FILE_IO" );
            dprintf( D_ALWAYS, "Failed to read userdata file '%s'.\n", udFileName.c_str() );
            return false;
            }
        buffer += udFileContents;
    }
    if( ! buffer.empty() ) {
        char * base64Encoded = condor_base64_encode( (const unsigned char *)buffer.c_str(), buffer.length() );

        // OpenNebula barfs on embedded newlines in this data.
        char * read = base64Encoded;
        char * write = base64Encoded;
        for( ; * read != '\0'; ++read ) {
            char tmp = * read;
            // We may need to check for \r on Windows (depends on OpenSSL).
            if( tmp != '\n' ) {
                * write = tmp;
                ++write;
            }
        }
        * write = '\0';

        vmStartRequest.query_parameters[ "UserData" ] = base64Encoded;
        free( base64Encoded );
    }

    // Send the request.
    if( ! vmStartRequest.SendRequest() ) {
        result_string = create_failure_result( requestID,
            vmStartRequest.errorMessage.c_str(),
            vmStartRequest.errorCode.c_str() );
    } else {
        if( vmStartRequest.instanceIDs.size() == 0 ) {
            dprintf( D_ALWAYS, "Got result from endpoint that did not include an instance ID, failing.  Response follows.\n" );
            dprintf( D_ALWAYS, "-- RESPONSE BEGINS --\n" );
            dprintf( D_ALWAYS, "%s", vmStartRequest.resultString.c_str() );
            dprintf( D_ALWAYS, "-- RESPONSE ENDS --\n" );
            result_string = create_failure_result( requestID, "Could not find instance ID in response from server.  Check the EC2 GAHP log for details.", "E_NO_INSTANCE_ID" );
        } else {
            std::vector<std::string> resultList = vmStartRequest.instanceIDs;
            result_string = create_success_result( requestID, & resultList );
        }
    }

    return true;
}

// ---------------------------------------------------------------------------

AmazonVMStartSpot::~AmazonVMStartSpot() { }

struct vmSpotUD_t {
    bool inInstanceId;
    bool inSpotRequestId;
    std::string & instanceID;
    std::string & spotRequestID;

    vmSpotUD_t( std::string & iid, std::string & srid ) : inInstanceId( false ), inSpotRequestId( false ), instanceID( iid ), spotRequestID( srid ) { }
};
typedef struct vmSpotUD_t vmSpotUD;

//
// Like the vsmStart*() functions, these assume we only ever get back
// single-item response sets.  See the note above for a cleaner way of
// handling multi-item response sets, should we ever need so to do.
//

void vmSpotESH( void * vUserData, const XML_Char * name, const XML_Char ** ) {
    vmSpotUD * vsud = (vmSpotUD *)vUserData;
    if( strcasecmp( ignoringNameSpace( name ), "instanceId" ) == 0 ) {
        vsud->inInstanceId = true;
    } else if( strcasecmp( ignoringNameSpace( name ), "spotInstanceRequestId" ) == 0 ) {
        vsud->inSpotRequestId = true;
    }
}

void vmSpotCDH( void * vUserData, const XML_Char * cdata, int len ) {
    vmSpotUD * vsud = (vmSpotUD *)vUserData;
    if( vsud->inInstanceId ) {
        appendToString( (const void *)cdata, len, 1, (void *) & vsud->instanceID );
    } else if( vsud->inSpotRequestId ) {
        appendToString( (const void *)cdata, len, 1, (void *) & vsud->spotRequestID );
    }
}

void vmSpotEEH( void * vUserData, const XML_Char * name ) {
    vmSpotUD * vsud = (vmSpotUD *)vUserData;
    if( strcasecmp( ignoringNameSpace( name ), "instanceId" ) == 0 ) {
        vsud->inInstanceId = false;
    } else if( strcasecmp( ignoringNameSpace( name ), "spotInstanceRequestId" ) == 0 ) {
        vsud->inSpotRequestId = false;
    }
}

bool AmazonVMStartSpot::SendRequest() {
    bool result = AmazonRequest::SendRequest();
    if ( result ) {
        vmSpotUD vsud( this->instanceID, this->spotRequestID );
        XML_Parser xp = XML_ParserCreate( NULL );
        XML_SetElementHandler( xp, & vmSpotESH, & vmSpotEEH );
        XML_SetCharacterDataHandler( xp, & vmSpotCDH );
        XML_SetUserData( xp, & vsud );
        XML_Parse( xp, this->resultString.c_str(), this->resultString.length(), 1 );
        XML_ParserFree( xp );
    } else {
        if( this->errorCode == "E_CURL_IO" ) {
            // To be on the safe side, if the I/O failed, make the gridmanager
            // check to see the VM was started or not.
            this->errorCode = "NEED_CHECK_VM_START";
            return false;
        }
    }
    return result;
}

bool AmazonVMStartSpot::workerFunction( char ** argv, int argc, std::string & result_string ) {
    assert( strcasecmp( argv[0], "EC2_VM_START_SPOT" ) == 0 );

    // Uses the Query AP function 'RequestSpotInstances', as documented at
    // http://docs.amazonwebservices.com/AWSEC2/latest/APIReference/ApiReference-query-RequestSpotInstances.html

    int requestID;
    get_int( argv[1], & requestID );
    dprintf( D_PERF_TRACE, "request #%d (%s): work begins\n",
        requestID, argv[0] );

    if( ! verify_min_number_args( argc, 17 ) ) {
        result_string = create_failure_result( requestID, "Wrong_Argument_Number" );
        dprintf( D_ALWAYS, "Wrong number of arguments (%d should be >= %d) to %s\n", argc, 17, argv[0] );
        return false;
    }

    // Fill in required attributes / parameters.
    AmazonVMStartSpot vmSpotRequest = AmazonVMStartSpot( requestID, argv[0] );
    vmSpotRequest.serviceURL = argv[2];
    vmSpotRequest.accessKeyFile = argv[3];
    vmSpotRequest.secretKeyFile = argv[4];
    vmSpotRequest.query_parameters[ "Action" ] = "RequestSpotInstances";
    vmSpotRequest.query_parameters[ "LaunchSpecification.ImageId" ] = argv[5];
    vmSpotRequest.query_parameters[ "InstanceCount" ] = "1";
    vmSpotRequest.query_parameters[ "SpotPrice" ] = argv[6];

    // Optional attributes / parameters.
    if( strcasecmp( argv[7], NULLSTRING ) ) {
        vmSpotRequest.query_parameters[ "LaunchSpecification.KeyName" ] = argv[7];
    }

    if( strcasecmp( argv[10], NULLSTRING ) ) {
        vmSpotRequest.query_parameters[ "LaunchSpecification.InstanceType" ] = argv[10];
    } else {
        vmSpotRequest.query_parameters[ "LaunchSpecification.InstanceType" ] = "m1.small";
    }

    if( strcasecmp( argv[11], NULLSTRING ) ) {
        vmSpotRequest.query_parameters[ "LaunchSpecification.Placement.AvailabilityZone" ] = argv[11];
    }

    if( strcasecmp( argv[12], NULLSTRING ) ) {
        vmSpotRequest.query_parameters[ "LaunchSpecification.SubnetId" ] = argv[12];
    }

    if( strcasecmp( argv[13], NULLSTRING ) ) {
        vmSpotRequest.query_parameters[ "LaunchSpecification.NetworkInterface.1.PrivateIpAddress" ] = argv[13];
    }

    // Use LaunchGroup, which we don't otherwise support, as an idempotence
    // token, since RequestSpotInstances doesn't support ClientToken.
    if( strcasecmp( argv[14], NULLSTRING ) ) {
        vmSpotRequest.query_parameters[ "LaunchGroup" ] = argv[14];
    }

    if( strcasecmp( argv[15], NULLSTRING ) ) {
        vmSpotRequest.query_parameters[ "LaunchSpecification.IamInstanceProfile.Arn" ] = argv[15];
    }

    if( strcasecmp( argv[16], NULLSTRING ) ) {
        vmSpotRequest.query_parameters[ "LaunchSpecification.IamInstanceProfile.Name" ] = argv[16];
    }

	int i = 17;
	int sgNum = 1;
    for( ; i < argc; ++i ) {
    	if( strcasecmp( argv[ i ], NULLSTRING ) == 0 ) { break; }
        std::ostringstream groupName;
        groupName << "LaunchSpecification.SecurityGroup." << sgNum++;
        vmSpotRequest.query_parameters[ groupName.str() ] = argv[ i ];
    }

	++i;
	int sgidNum = 1;
    for( ; i < argc; ++i ) {
    	if( strcasecmp( argv[ i ], NULLSTRING ) == 0 ) { break; }
        std::ostringstream groupID;
        groupID << "LaunchSpecification.SecurityGroupId." << sgidNum++;
        vmSpotRequest.query_parameters[ groupID.str() ] = argv[ i ];
    }

    // Handle user data.
    std::string buffer;
    if( strcasecmp( argv[8], NULLSTRING ) ) {
        buffer = argv[8];
    }
    if( strcasecmp( argv[9], NULLSTRING ) ) {
        std::string udFileName = argv[9];
        std::string udFileContents;
        if( ! htcondor::readShortFile( udFileName, udFileContents ) ) {
            result_string = create_failure_result( requestID, "Failed to read userdata file.", "E_FILE_IO" );
            dprintf( D_ALWAYS, "failed to read userdata file '%s'.\n", udFileName.c_str() ) ;
            return false;
        }
        buffer += udFileContents;
    }
    if( ! buffer.empty() ) {
        char * base64Encoded = condor_base64_encode( (const unsigned char *)buffer.c_str(), buffer.length() );
        vmSpotRequest.query_parameters[ "LaunchSpecification.UserData" ] = base64Encoded;
        free( base64Encoded );
    }

    // Send the request.
    if( ! vmSpotRequest.SendRequest() ) {
        result_string = create_failure_result( requestID, vmSpotRequest.errorMessage.c_str(), vmSpotRequest.errorCode.c_str() ) ;
    } else {
        if( vmSpotRequest.spotRequestID.empty() ) {
            dprintf( D_ALWAYS, "Got a result from endpoint that did not include a spot request ID, failing.  Response follows.\n" );
            dprintf( D_ALWAYS, "-- RESPONSE BEGINS --\n" );
            dprintf( D_ALWAYS, "%s", vmSpotRequest.resultString.c_str() );
            dprintf( D_ALWAYS, "-- RESPONSE ENDS --\n" );
            result_string = create_failure_result( requestID, "Could not find spot request ID in repsonse from server.  Check the EC2 GAHP log for details.", "E_NO_SPOT_REQUEST_ID" );
            // We don't return false here, because this isn't an error;
            // it's a failure, which the grid manager will handle.
        } else {
            std::vector<std::string> resultList;
            resultList.emplace_back( vmSpotRequest.spotRequestID );
            // GM_SPOT_START -> GM_SPOT_SUBMITTED -> GM_SPOT_QUERY always
            // happens, so simplify things and just don't report this.
            // resultList.emplace_back( nullStringIfEmpty( vmSpotRequest.instanceID ) );
            result_string = create_success_result( requestID, & resultList );
        }
    }

    return true;
} // end AmazonVMStartSpot::workerFunction()

// ---------------------------------------------------------------------------

AmazonVMStop::~AmazonVMStop() { }

// Expecting:EC2_VM_STOP <req_id> <serviceurl> <accesskeyfile> <secretkeyfile> <instance-id>
bool AmazonVMStop::workerFunction(char **argv, int argc, std::string &result_string) {
    assert( strcasecmp( argv[0], "EC2_VM_STOP" ) == 0 );

    // Uses the Query API function 'TerminateInstances', as documented at
    // http://docs.amazonwebservices.com/AWSEC2/latest/APIReference/ApiReference-query-TerminateInstances.html

    int requestID;
    get_int( argv[1], & requestID );
    dprintf( D_PERF_TRACE, "request #%d (%s): work begins\n",
        requestID, argv[0] );

    if( ! verify_min_number_args( argc, 6 ) ) {
        result_string = create_failure_result( requestID, "Wrong_Argument_Number" );
        dprintf( D_ALWAYS, "Wrong number of arguments (%d should be >= %d) to %s\n",
                 argc, 6, argv[0] );
        return false;
    }

    // Fill in required attributes & parameters.
    AmazonVMStop terminationRequest = AmazonVMStop( requestID, argv[0] );
    terminationRequest.serviceURL = argv[2];
    terminationRequest.accessKeyFile = argv[3];
    terminationRequest.secretKeyFile = argv[4];
    terminationRequest.query_parameters[ "Action" ] = "TerminateInstances";
    terminationRequest.query_parameters[ "InstanceId.1" ] = argv[5];

	int parameterNumber = 2;
	std::string parameterName;
	for( int i = 6; i < argc; ++i, ++parameterNumber ) {
		formatstr( parameterName, "InstanceId.%d", parameterNumber );
		terminationRequest.query_parameters[ parameterName.c_str() ] = argv[i];
	}

    // Send the request.
    if( ! terminationRequest.SendRequest() ) {
        result_string = create_failure_result( requestID,
            terminationRequest.errorMessage.c_str(),
            terminationRequest.errorCode.c_str() );
    } else {
        // AmazonVMStop::SendRequest() could parse its resultString
        // to look for instance IDs (and current/previous states),
        // but it doesn't presently look like it needs to so do.
        result_string = create_success_result( requestID, NULL );
    }

    return true;
}

// ---------------------------------------------------------------------------

AmazonVMStopSpot::~AmazonVMStopSpot() { }

bool AmazonVMStopSpot::workerFunction( char ** argv, int argc, std::string & result_string ) {
    assert( strcasecmp( argv[0], "EC2_VM_STOP_SPOT" ) == 0 );

    // Uses the Query API function 'CancelSpotInstanceRequests', documented at
    // http://docs.amazonwebservices.com/AWSEC2/latest/APIReference/ApiReference-query-CancelSpotInstanceRequests.html

    int requestID;
    get_int( argv[1], & requestID );
    dprintf( D_PERF_TRACE, "request #%d (%s): work begins\n",
        requestID, argv[0] );

    if( ! verify_min_number_args( argc, 6 ) ) {
        result_string = create_failure_result( requestID, "Wrong_Argument_Number" );
        dprintf( D_ALWAYS, "Wrong number of arguments (%d should be >= %d) to %s\n",
                 argc, 6, argv[0] );
        return false;
    }

    AmazonVMStopSpot terminationRequest = AmazonVMStopSpot( requestID, argv[0] );
    terminationRequest.serviceURL = argv[2];
    terminationRequest.accessKeyFile = argv[3];
    terminationRequest.secretKeyFile = argv[4];
    terminationRequest.query_parameters[ "Action" ] = "CancelSpotInstanceRequests";
    //
    // Rather than cancel the corresponding instance in this function,
    // just have the grid manager call EC2_VM_STOP; that allows us to
    // return two error messages for two activities.
    //
    terminationRequest.query_parameters[ "SpotInstanceRequestId.1" ] = argv[5];

    if( ! terminationRequest.SendRequest() ) {
        result_string = create_failure_result( requestID,
            terminationRequest.errorMessage.c_str(),
            terminationRequest.errorCode.c_str() );
    } else {
        result_string = create_success_result( requestID, NULL );
    }

    return true;
} // end AmazonVMStop::workerFunction()

// ---------------------------------------------------------------------------

AmazonVMStatus::~AmazonVMStatus() { }

// Expecting:EC2_VM_STATUS <req_id> <serviceurl> <accesskeyfile> <secretkeyfile> <instance-id>
bool AmazonVMStatus::workerFunction(char **argv, int argc, std::string &result_string) {
    assert( strcasecmp( argv[0], "EC2_VM_STATUS" ) == 0 );

    // Uses the Query API function 'DescribeInstances', as documented at
    // http://docs.amazonwebservices.com/AWSEC2/latest/APIReference/ApiReference-query-DescribeInstances.html

    int requestID;
    get_int( argv[1], & requestID );
    dprintf( D_PERF_TRACE, "request #%d (%s): work begins\n",
        requestID, argv[0] );

    if( ! verify_min_number_args( argc, 6 ) ) {
        result_string = create_failure_result( requestID, "Wrong_Argument_Number" );
        dprintf( D_ALWAYS, "Wrong number of arguments (%d should be >= %d) to %s\n",
                 argc, 6, argv[0] );
        return false;
    }

    // Fill in required attributes & parameters.
    AmazonVMStatus sRequest = AmazonVMStatus( requestID, argv[0] );
    sRequest.serviceURL = argv[2];
    sRequest.accessKeyFile = argv[3];
    sRequest.secretKeyFile = argv[4];
    sRequest.query_parameters[ "Action" ] = "DescribeInstances";
    // We should also be able to set the parameter InstanceId.1
    // and only get one instance back, rather than filtering.
    std::string instanceID = argv[5];

    // Send the request.
    if( ! sRequest.SendRequest() ) {
        result_string = create_failure_result( requestID,
            sRequest.errorMessage.c_str(),
            sRequest.errorCode.c_str() );
    } else {
        if( sRequest.results.size() == 0 ) {
            result_string = create_success_result( requestID, NULL );
        } else {
            std::vector<std::string> resultList;
            for( unsigned i = 0; i < sRequest.results.size(); ++i ) {
                AmazonStatusResult asr = sRequest.results[i];
                if( asr.instance_id != instanceID ) { continue; }

                resultList.emplace_back( asr.instance_id );
                resultList.emplace_back( asr.status );
                resultList.emplace_back( asr.ami_id );
                resultList.emplace_back( nullStringIfEmpty( asr.stateReasonCode ) );

                // if( ! asr.stateReasonCode.empty() ) { dprintf( D_ALWAYS, "DEBUG: Instance %s has status %s because %s\n", asr.instance_id.c_str(), asr.status.c_str(), asr.stateReasonCode.c_str() ); }

                if( strcasecmp( asr.status.c_str(), AMAZON_STATUS_RUNNING ) == 0 ) {
                    resultList.emplace_back( nullStringIfEmpty( asr.public_dns ) );
                    resultList.emplace_back( nullStringIfEmpty( asr.private_dns ) );
                    resultList.emplace_back( nullStringIfEmpty( asr.keyname ) );
                    if( asr.securityGroups.size() == 0 ) {
                        resultList.emplace_back( NULLSTRING );
                    } else {
                        for( unsigned j = 0; j < asr.securityGroups.size(); ++j ) {
                            resultList.emplace_back( asr.securityGroups[j] );
                        }
                    }
                }
            }
            result_string = create_success_result( requestID, & resultList );
        }
    }

    return true;
}

// ---------------------------------------------------------------------------

AmazonVMStatusSpot::~AmazonVMStatusSpot() { }

struct vmStatusSpotUD_t {
    enum vmStatusSpotTags_t {
        NONE,
        INSTANCE_ID,
        STATE,
        LAUNCH_GROUP,
        REQUEST_ID,
        STATUS_CODE
    };
    typedef enum vmStatusSpotTags_t vmStatusSpotTags;

    // The groupSet member of the launchSpecification also contains
    // an 'item' member.  Do NOT stop looking for tags of interest
    // (like the instance ID) because of them.  XPath is looking more
    // and more tasty...
    unsigned short inItem;

    bool inStatus;
    vmStatusSpotTags inWhichTag;
    AmazonStatusSpotResult * currentResult;

    std::vector< AmazonStatusSpotResult > & results;

    vmStatusSpotUD_t( std::vector< AmazonStatusSpotResult > & assrList ) :
        inItem( 0 ),
        inStatus( false ),
        inWhichTag( vmStatusSpotUD_t::NONE ),
        currentResult( NULL ),
        results( assrList ) { };
};
typedef struct vmStatusSpotUD_t vmStatusSpotUD;

void vmStatusSpotESH( void * vUserData, const XML_Char * name, const XML_Char ** ) {
    vmStatusSpotUD * vsud = (vmStatusSpotUD *)vUserData;

    if( strcasecmp( ignoringNameSpace( name ), "item" ) == 0 ) {
        if( vsud->inItem == 0 ) {
            vsud->currentResult = new AmazonStatusSpotResult();
            assert( vsud->currentResult != NULL );
        }
        vsud->inItem += 1;
        return;
    }

    if( strcasecmp( ignoringNameSpace( name ), "spotInstanceRequestId" ) == 0 ) {
        vsud->inWhichTag = vmStatusSpotUD::REQUEST_ID;
    } else if( strcasecmp( ignoringNameSpace( name ), "state" ) == 0 ) {
        vsud->inWhichTag = vmStatusSpotUD::STATE;
    } else if( strcasecmp( ignoringNameSpace( name ), "instanceId" ) == 0 ) {
        vsud->inWhichTag = vmStatusSpotUD::INSTANCE_ID;
    } else if( strcasecmp( ignoringNameSpace( name ), "launchGroup" ) == 0 ) {
        vsud->inWhichTag = vmStatusSpotUD::LAUNCH_GROUP;
    } else if( strcasecmp( ignoringNameSpace( name ), "status" ) == 0 ) {
        vsud->inStatus = true;
    } else if( vsud->inStatus && strcasecmp( ignoringNameSpace( name ), "code" ) == 0 ) {
        vsud->inWhichTag = vmStatusSpotUD::STATUS_CODE;
    } else {
        vsud->inWhichTag = vmStatusSpotUD::NONE;
    }
}

void vmStatusSpotCDH( void * vUserData, const XML_Char * cdata, int len ) {
    vmStatusSpotUD * vsud = (vmStatusSpotUD *)vUserData;
    if( vsud->inItem != 1 ) { return; }

    std::string * targetString = NULL;
    switch( vsud->inWhichTag ) {
        case vmStatusSpotUD::NONE:
            return;

        case vmStatusSpotUD::REQUEST_ID:
            targetString = & vsud->currentResult->request_id;
            break;

        case vmStatusSpotUD::STATE:
            targetString = & vsud->currentResult->state;
            break;

        case vmStatusSpotUD::LAUNCH_GROUP:
            targetString = & vsud->currentResult->launch_group;
            break;

        case vmStatusSpotUD::INSTANCE_ID:
            targetString = & vsud->currentResult->instance_id;
            break;

        case vmStatusSpotUD::STATUS_CODE:
            targetString = & vsud->currentResult->status_code;
            break;

        default:
            // This should never happen.
            break;
    }

    appendToString( (const void *)cdata, len, 1, (void *)targetString );
}

void vmStatusSpotEEH( void * vUserData, const XML_Char * name ) {
    vmStatusSpotUD * vsud = (vmStatusSpotUD *)vUserData;

    if( strcasecmp( ignoringNameSpace( name ), "item" ) == 0 ) {
        if( vsud->inItem == 1 ) {
            vsud->results.push_back( * vsud->currentResult );
            delete vsud->currentResult;
            vsud->currentResult = NULL;
        }

        vsud->inItem -= 1;
    }

    vsud->inWhichTag = vmStatusSpotUD::NONE;
}

bool AmazonVMStatusSpot::SendRequest() {
    bool result = AmazonRequest::SendRequest();
    if( result ) {
        vmStatusSpotUD vssud( this->spotResults );
        XML_Parser xp = XML_ParserCreate( NULL );
        XML_SetElementHandler( xp, & vmStatusSpotESH, & vmStatusSpotEEH );
        XML_SetCharacterDataHandler( xp, & vmStatusSpotCDH );
        XML_SetUserData( xp, & vssud );
        XML_Parse( xp, this->resultString.c_str(), this->resultString.length(), 1 );
        XML_ParserFree( xp );
    }
    return result;
} // end AmazonVMStatusSpot::SendRequest()

bool AmazonVMStatusSpot::workerFunction(char **argv, int argc, std::string &result_string) {
    assert( strcasecmp( argv[0], "EC2_VM_STATUS_SPOT" ) == 0 );

    // Uses the Query API function 'DescribeSpotInstanceRequests', as documented at
    // http://docs.amazonwebservices.com/AWSEC2/latest/APIReference/ApiReference-query-DescribeSpotInstanceRequests.html

    int requestID;
    get_int( argv[1], & requestID );
    dprintf( D_PERF_TRACE, "request #%d (%s): work begins\n",
        requestID, argv[0] );

    if( ! verify_min_number_args( argc, 6 ) ) {
        result_string = create_failure_result( requestID, "Wrong_Argument_Number" );
        dprintf( D_ALWAYS, "Wrong number of arguments (%d should be >= %d) to %s\n",
                 argc, 6, argv[0] );
        return false;
    }

    AmazonVMStatusSpot statusRequest = AmazonVMStatusSpot( requestID, argv[0] );
    statusRequest.serviceURL = argv[2];
    statusRequest.accessKeyFile = argv[3];
    statusRequest.secretKeyFile = argv[4];
    statusRequest.query_parameters[ "Action" ] = "DescribeSpotInstanceRequests";
    statusRequest.query_parameters[ "SpotInstanceRequestId.1" ] = argv[5];

    if( ! statusRequest.SendRequest() ) {
        result_string = create_failure_result( requestID,
            statusRequest.errorMessage.c_str(),
            statusRequest.errorCode.c_str() );
    } else {
        if( statusRequest.spotResults.size() == 0 ) {
            result_string = create_success_result( requestID, NULL );
        } else {
            // There should only ever be one result, but let's not worry.
            std::vector<std::string> resultList;
            for( unsigned i = 0; i < statusRequest.spotResults.size(); ++i ) {
                AmazonStatusSpotResult & assr = statusRequest.spotResults[i];
                resultList.emplace_back( assr.request_id );
                resultList.emplace_back( assr.state );
                resultList.emplace_back( assr.launch_group );
                resultList.emplace_back( nullStringIfEmpty( assr.instance_id ) );
                resultList.emplace_back( nullStringIfEmpty( assr.status_code.c_str() ) );
            }
            result_string = create_success_result( requestID, & resultList );
        }
    }

    return true;
} // end AmazonVmStatusSpot::workerFunction()

// ---------------------------------------------------------------------------

AmazonVMStatusAllSpot::~AmazonVMStatusAllSpot( ) { }

bool AmazonVMStatusAllSpot::workerFunction(char **argv, int argc, std::string &result_string) {
    assert( strcasecmp( argv[0], "EC2_VM_STATUS_ALL_SPOT" ) == 0 );

    // Uses the Query API function 'DescribeSpotInstanceRequests', as documented at
    // http://docs.amazonwebservices.com/AWSEC2/latest/APIReference/ApiReference-query-DescribeSpotInstanceRequests.html

    int requestID;
    get_int( argv[1], & requestID );
    dprintf( D_PERF_TRACE, "request #%d (%s): work begins\n",
        requestID, argv[0] );

    if( ! verify_min_number_args( argc, 5 ) ) {
        result_string = create_failure_result( requestID, "Wrong_Argument_Number" );
        dprintf( D_ALWAYS, "Wrong number of arguments (%d should be >= %d) to %s\n",
                 argc, 5, argv[0] );
        return false;
    }

    AmazonVMStatusAllSpot statusRequest = AmazonVMStatusAllSpot( requestID, argv[0] );
    statusRequest.serviceURL = argv[2];
    statusRequest.accessKeyFile = argv[3];
    statusRequest.secretKeyFile = argv[4];
    statusRequest.query_parameters[ "Action" ] = "DescribeSpotInstanceRequests";

    if( ! statusRequest.SendRequest() ) {
        result_string = create_failure_result( requestID,
            statusRequest.errorMessage.c_str(),
            statusRequest.errorCode.c_str() );
    } else {
        if( statusRequest.spotResults.size() == 0 ) {
            result_string = create_success_result( requestID, NULL );
        } else {
            std::vector<std::string> resultList;
            for( unsigned i = 0; i < statusRequest.spotResults.size(); ++i ) {
                AmazonStatusSpotResult & assr = statusRequest.spotResults[i];
                resultList.emplace_back( assr.request_id );
                resultList.emplace_back( assr.state );
                resultList.emplace_back( assr.launch_group );
                resultList.emplace_back( nullStringIfEmpty( assr.instance_id ) );
                resultList.emplace_back( nullStringIfEmpty( assr.status_code.c_str() ) );
            }
            result_string = create_success_result( requestID, & resultList );
        }
    }

    return true;
} // end AmazonVmStatusAllSpot::workerFunction()

// ---------------------------------------------------------------------------

AmazonVMStatusAll::~AmazonVMStatusAll() { }

struct vmStatusUD_t {
    enum vmStatusTags_t {
        NONE,
        INSTANCE_ID,
        STATUS,
        AMI_ID,
        PUBLIC_DNS,
        PRIVATE_DNS,
        KEY_NAME,
        INSTANCE_TYPE,
        GROUP_ID,
        STATE_REASON_CODE,
        CLIENT_TOKEN,
        TAG_KEY,
        TAG_VALUE
    };
    typedef enum vmStatusTags_t vmStatusTags;

    bool inInstancesSet;
    bool inInstance;
    bool inInstanceState;
    bool inStateReason;
    vmStatusTags inWhichTag;
    AmazonStatusResult * currentResult;
    std::vector< AmazonStatusResult > & results;

    unsigned short inItem;

    bool inGroupSet;
    bool inGroup;
    std::string currentSecurityGroup;
    std::vector< std::string > currentSecurityGroups;

	bool inTagSet;
	bool inTag;
	std::string tagKey;
	std::string tagValue;

    vmStatusUD_t( std::vector< AmazonStatusResult > & asrList ) :
        inInstancesSet( false ),
        inInstance( false ),
        inInstanceState( false ),
        inStateReason( false ),
        inWhichTag( vmStatusUD_t::NONE ),
        currentResult( NULL ),
        results( asrList ),
        inItem( 0 ),
        inGroupSet( false ),
        inGroup( false ),
        inTagSet( false ),
        inTag( false ) { }
};
typedef struct vmStatusUD_t vmStatusUD;

void vmStatusESH( void * vUserData, const XML_Char * name, const XML_Char ** ) {
    vmStatusUD * vsud = (vmStatusUD *)vUserData;

    if( ! vsud->inInstancesSet ) {
        if( strcasecmp( ignoringNameSpace( name ), "instancesSet" ) == 0 ) {
            vsud->inInstancesSet = true;
        }
        return;
    }

    if( ! vsud->inInstance ) {
        if( strcasecmp( ignoringNameSpace( name ), "item" ) == 0 ) {
            vsud->currentResult = new AmazonStatusResult();
            assert( vsud->currentResult != NULL );
            vsud->inInstance = true;
            vsud->inItem += 1;
        }
        return;
    }

    //
    // If we get here, we're in an instance.
    //

    // We don't care about item tags inside an instance; we just need
    // to make sure we know which one closes the instance's item tag.
    if( strcasecmp( ignoringNameSpace( name ), "item" ) == 0 ) {
        vsud->inItem += 1;
        if( vsud->inTagSet ) { vsud->inTag = true; }
        return;
    }

    if( strcasecmp( ignoringNameSpace( name ), "groupSet" ) == 0 ) {
        vsud->currentSecurityGroups.clear();
        vsud->inGroupSet = true;
        return;
    }

    //
    // Check for any tags of interest in the group set.
    //
    if( vsud->inGroupSet ) {
        if( strcasecmp( ignoringNameSpace( name ), "groupId" ) == 0 ) {
            vsud->inGroup = true;
            return;
        }
    }

    //
    // Check for any tags of interest in the instance.
    //
    if( strcasecmp( ignoringNameSpace( name ), "instanceId" ) == 0 ) {
        vsud->inWhichTag = vmStatusUD::INSTANCE_ID;
    } else if( strcasecmp( ignoringNameSpace( name ), "imageId" ) == 0 ) {
        vsud->inWhichTag = vmStatusUD::AMI_ID;
    } else if( strcasecmp( ignoringNameSpace( name ), "privateDnsName" ) == 0 ) {
        vsud->inWhichTag = vmStatusUD::PRIVATE_DNS;
    } else if( strcasecmp( ignoringNameSpace( name ), "dnsName" ) == 0 ) {
        vsud->inWhichTag = vmStatusUD::PUBLIC_DNS;
    } else if( strcasecmp( ignoringNameSpace( name ), "keyName" ) == 0 ) {
        vsud->inWhichTag = vmStatusUD::KEY_NAME;
    } else if( strcasecmp( ignoringNameSpace( name ), "instanceType" ) == 0 ) {
        vsud->inWhichTag = vmStatusUD::INSTANCE_TYPE;
    } else if( strcasecmp( ignoringNameSpace( name ), "clientToken" ) == 0 ) {
        vsud->inWhichTag = vmStatusUD::CLIENT_TOKEN;
    // This is wrong, but Eucalyptus does it anyway.  Humour them.
    } else if( strcasecmp( ignoringNameSpace( name ), "stateName" ) == 0 ) {
        vsud->inWhichTag = vmStatusUD::STATUS;
    //
    // instanceState and stateReason share the 'code' tagname, but can't
    // ever be nested, so we can use this simpler style to check for
    // the tags we care about.
    //
    } else if( strcasecmp( ignoringNameSpace( name ), "instanceState" ) == 0 ) {
        vsud->inWhichTag = vmStatusUD::NONE;
        vsud->inInstanceState = true;
    } else if( vsud->inInstanceState && strcasecmp( ignoringNameSpace( name ), "name" ) == 0 ) {
        vsud->inWhichTag = vmStatusUD::STATUS;
    } else if( strcasecmp( ignoringNameSpace( name ), "stateReason" ) == 0 ) {
        vsud->inWhichTag = vmStatusUD::NONE;
        vsud->inStateReason = true;
    } else if( vsud->inStateReason && strcasecmp( ignoringNameSpace( name ), "code" ) == 0 )  {
        vsud->inWhichTag = vmStatusUD::STATE_REASON_CODE;
    } else if( strcasecmp( ignoringNameSpace( name ), "tagSet" ) == 0 ) {
        vsud->inWhichTag = vmStatusUD::NONE;
        vsud->inTagSet = true;
    } else if( vsud->inTag && strcasecmp( ignoringNameSpace( name ), "key" ) == 0 ) {
        vsud->inWhichTag = vmStatusUD::TAG_KEY;
    } else if( vsud->inTag && strcasecmp( ignoringNameSpace( name ), "value" ) == 0 ) {
        vsud->inWhichTag = vmStatusUD::TAG_VALUE;
    }
}

void vmStatusCDH( void * vUserData, const XML_Char * cdata, int len ) {
    vmStatusUD * vsud = (vmStatusUD *)vUserData;

    //
    // We could probably convert this into a tag type, but I think the logic
    // is a little clearer if tag types are limited to direct children of
    // the instance.
    //
    if( vsud->inGroup ) {
        appendToString( (const void *)cdata, len, 1, (void *) & vsud->currentSecurityGroup );
        return;
    }

    if( vsud->currentResult == NULL ) {
        return;
    }

    std::string * targetString = NULL;
    switch( vsud->inWhichTag ) {
        case vmStatusUD::NONE:
            return;

        case vmStatusUD::INSTANCE_ID:
            targetString = & vsud->currentResult->instance_id;
            break;

        case vmStatusUD::STATUS:
            targetString = & vsud->currentResult->status;
            break;

        case vmStatusUD::AMI_ID:
            targetString = & vsud->currentResult->ami_id;
            break;

        case vmStatusUD::PRIVATE_DNS:
            targetString = & vsud->currentResult->private_dns;
            break;

        case vmStatusUD::PUBLIC_DNS:
            targetString = & vsud->currentResult->public_dns;
            break;

        case vmStatusUD::KEY_NAME:
            targetString = & vsud->currentResult->keyname;
            break;

        case vmStatusUD::INSTANCE_TYPE:
            targetString = & vsud->currentResult->instancetype;
            break;

        case vmStatusUD::STATE_REASON_CODE:
            targetString = & vsud->currentResult->stateReasonCode;
            break;

        case vmStatusUD::CLIENT_TOKEN:
            targetString = & vsud->currentResult->clientToken;
            break;

		case vmStatusUD::TAG_KEY:
			targetString = & vsud->tagKey;
			break;

		case vmStatusUD::TAG_VALUE:
			targetString = & vsud->tagValue;
			break;

        default:
            /* This should never happen. */
            return;
    }

    appendToString( (const void *)cdata, len, 1, (void *)targetString );
}

void vmStatusEEH( void * vUserData, const XML_Char * name ) {
    vmStatusUD * vsud = (vmStatusUD *)vUserData;
    vsud->inWhichTag = vmStatusUD::NONE;

    if( ! vsud->inInstancesSet ) {
        return;
    }

    if( strcasecmp( ignoringNameSpace( name ), "instancesSet" ) == 0 ) {
        vsud->inInstancesSet = false;
        return;
    }

    if( ! vsud->inInstance ) {
        return;
    }

    if( strcasecmp( ignoringNameSpace( name ), "item" ) == 0 ) {
        vsud->inItem -= 1;

        if( vsud->inItem == 0 ) {
            ASSERT( vsud->inGroupSet == false );
            vsud->results.push_back( * vsud->currentResult );
            delete vsud->currentResult;
            vsud->currentResult = NULL;
            vsud->inInstance = false;
        } else {
            if( vsud->inTagSet ) {
            	if( vsud->tagKey == "aws:ec2spot:fleet-request-id" ) {
            		vsud->currentResult->spotFleetRequestID = vsud->tagValue;
            	} else if( vsud->tagKey == "htcondor:AnnexName" ) {
            		vsud->currentResult->annexName = vsud->tagValue;
            	}
            	vsud->tagKey.erase();
            	vsud->tagValue.erase();
            	vsud->inTag = false;
            }
        }

        return;
    }

    if( strcasecmp( ignoringNameSpace( name ), "groupSet" ) == 0 ) {
        ASSERT( vsud->inGroupSet );
        vsud->inGroupSet = false;

        // Store the collected set of security groups.
        vsud->currentResult->securityGroups = vsud->currentSecurityGroups;
        return;
    }

    if( vsud->inGroupSet ) {
        if( strcasecmp( ignoringNameSpace( name ), "groupId" ) == 0 ) {
            // Store the security group in the current set.
            vsud->currentSecurityGroups.push_back( vsud->currentSecurityGroup );
            vsud->currentSecurityGroup.erase();
            vsud->inGroup = false;
            return;
        }
    }

    if( strcasecmp( ignoringNameSpace( name ), "instanceState" ) == 0 ) {
        ASSERT( vsud->inInstanceState );
        vsud->inInstanceState = false;
        return;
    }

    if( strcasecmp( ignoringNameSpace( name ), "stateReason" ) == 0 ) {
        ASSERT( vsud->inStateReason );
        vsud->inStateReason = false;
        return;
    }

	if( strcasecmp( ignoringNameSpace( name ), "tagSet" ) == 0 ) {
        vsud->inTagSet = false;
        return;
    }
    if( vsud->inTag && strcasecmp( ignoringNameSpace( name ), "key" ) == 0 ) {
        vsud->inWhichTag = vmStatusUD::NONE;
        return;
    }
    if( vsud->inTag && strcasecmp( ignoringNameSpace( name ), "value" ) == 0 ) {
        vsud->inWhichTag = vmStatusUD::NONE;
        return;
    }


}

bool AmazonVMStatusAll::SendRequest() {
    bool result = AmazonRequest::SendRequest();
    if( result ) {
        vmStatusUD vsud( this->results );
        XML_Parser xp = XML_ParserCreate( NULL );
        XML_SetElementHandler( xp, & vmStatusESH, & vmStatusEEH );
        XML_SetCharacterDataHandler( xp, & vmStatusCDH );
        XML_SetUserData( xp, & vsud );
        XML_Parse( xp, this->resultString.c_str(), this->resultString.length(), 1 );
        XML_ParserFree( xp );
    }
    return result;
}

// Expecting:EC2_VM_STATUS_ALL <req_id> <serviceurl> <accesskeyfile> <secretkeyfile>
bool AmazonVMStatusAll::workerFunction(char **argv, int argc, std::string &result_string) {
    assert( strcasecmp( argv[0], "EC2_VM_STATUS_ALL" ) == 0 );

    // Uses the Query API function 'DescribeInstances', as documented at
    // http://docs.amazonwebservices.com/AWSEC2/latest/APIReference/ApiReference-query-DescribeInstances.html

    int requestID;
    get_int( argv[1], & requestID );
    dprintf( D_PERF_TRACE, "request #%d (%s): work begins\n",
        requestID, argv[0] );

    if( ! verify_min_number_args( argc, 5 ) ) {
        result_string = create_failure_result( requestID, "Wrong_Argument_Number" );
        dprintf( D_ALWAYS, "Wrong number of arguments (%d should be >= %d) to %s\n",
                 argc, 5, argv[0] );
        return false;
    }

    // Fill in required attributes & parameters.
    AmazonVMStatusAll saRequest = AmazonVMStatusAll( requestID, argv[0] );
    saRequest.serviceURL = argv[2];
    saRequest.accessKeyFile = argv[3];
    saRequest.secretKeyFile = argv[4];
    saRequest.query_parameters[ "Action" ] = "DescribeInstances";

	if( argc >= 7 && strcmp( argv[5], NULLSTRING ) && strcmp( argv[6], NULLSTRING ) ) {
		saRequest.query_parameters[ "Filter.1.Name" ] = argv[5];
		saRequest.query_parameters[ "Filter.1.Value.1" ] = argv[6];
	}

    // Send the request.
    if( ! saRequest.SendRequest() ) {
        result_string = create_failure_result( requestID,
            saRequest.errorMessage.c_str(),
            saRequest.errorCode.c_str() );
    } else {
        if( saRequest.results.size() == 0 ) {
            result_string = create_success_result( requestID, NULL );
        } else {
            std::vector<std::string> resultList;
            for( unsigned i = 0; i < saRequest.results.size(); ++i ) {
                AmazonStatusResult & asr = saRequest.results[i];
                resultList.emplace_back( asr.instance_id );
                resultList.emplace_back( asr.status );
                // For GT #3682: only one of the following two may be null.
                resultList.emplace_back( nullStringIfEmpty( asr.clientToken ) );
                resultList.emplace_back( nullStringIfEmpty( asr.keyname ) );
                resultList.emplace_back( nullStringIfEmpty( asr.stateReasonCode ) );
                resultList.emplace_back( nullStringIfEmpty( asr.public_dns ) );
                resultList.emplace_back( nullStringIfEmpty( asr.spotFleetRequestID ) );
                resultList.emplace_back( nullStringIfEmpty( asr.annexName ) );
            }
            result_string = create_success_result( requestID, & resultList );
        }
    }

    return true;
}

// ---------------------------------------------------------------------------

AmazonVMCreateKeypair::~AmazonVMCreateKeypair() { }

struct privateKeyUD_t {
    bool inKeyMaterial;
    std::string keyMaterial;

    privateKeyUD_t() : inKeyMaterial( false ) { }
};
typedef struct privateKeyUD_t privateKeyUD;

void createKeypairESH( void * vUserData, const XML_Char * name, const XML_Char ** ) {
    privateKeyUD * pkud = (privateKeyUD *)vUserData;
    if( strcasecmp( ignoringNameSpace( name ), "keyMaterial" ) == 0 ) {
        pkud->inKeyMaterial = true;
    }
}

void createKeypairCDH( void * vUserData, const XML_Char * cdata, int len ) {
    privateKeyUD * pkud = (privateKeyUD *)vUserData;
    if( pkud->inKeyMaterial ) {
        appendToString( (const void *)cdata, len, 1, (void *) & pkud->keyMaterial );
    }
}

void createKeypairEEH( void * vUserData, const XML_Char * name ) {
    privateKeyUD * pkud = (privateKeyUD *)vUserData;
    if( strcasecmp( ignoringNameSpace( name ), "keyMaterial" ) == 0 ) {
        pkud->inKeyMaterial = false;
    }    
}

bool AmazonVMCreateKeypair::SendRequest() {
    bool result = AmazonRequest::SendRequest();
    if( result ) {
        privateKeyUD pkud;
        XML_Parser xp = XML_ParserCreate( NULL );
        XML_SetElementHandler( xp, & createKeypairESH, & createKeypairEEH );
        XML_SetCharacterDataHandler( xp, & createKeypairCDH );
        XML_SetUserData( xp, & pkud );
        XML_Parse( xp, this->resultString.c_str(), this->resultString.length(), 1 );
        XML_ParserFree( xp );

        if( ! htcondor::writeShortFile( this->privateKeyFileName, pkud.keyMaterial ) ) {
            this->errorCode = "E_FILE_IO";
            this->errorMessage = "Failed to write private key to file.";
            dprintf( D_ALWAYS, "Failed to write private key material to '%s', failing.\n",
                this->privateKeyFileName.c_str() );
            return false;
        }
    } else {
        if( this->errorCode == "E_CURL_IO" ) {
            // To be on the safe side, if the I/O failed, make the gridmanager
            // check to see the keys were created or not.
            this->errorCode = "NEED_CHECK_SSHKEY";
            return false;
        }
    }
    return result;
}

// Expecting:EC2_VM_CREATE_KEYPAIR <req_id> <serviceurl> <accesskeyfile> <secretkeyfile> <keyname> <outputfile>
bool AmazonVMCreateKeypair::workerFunction(char **argv, int argc, std::string &result_string) {
    assert( strcasecmp( argv[0], "EC2_VM_CREATE_KEYPAIR" ) == 0 );

    // Uses the Query API function 'CreateKeyPair', as documented at
    // http://docs.amazonwebservices.com/AWSEC2/latest/APIReference/ApiReference-query-CreateKeyPair.html

    int requestID;
    get_int( argv[1], & requestID );
    dprintf( D_PERF_TRACE, "request #%d (%s): work begins\n",
        requestID, argv[0] );

    if( ! verify_min_number_args( argc, 7 ) ) {
        result_string = create_failure_result( requestID, "Wrong_Argument_Number" );
        dprintf( D_ALWAYS, "Wrong number of arguments (%d should be >= %d) to %s\n",
                 argc, 7, argv[0] );
        return false;
    }

    // Fill in required attributes & parameters.
    AmazonVMCreateKeypair ckpRequest = AmazonVMCreateKeypair( requestID, argv[0] );
    ckpRequest.serviceURL = argv[2];
    ckpRequest.accessKeyFile = argv[3];
    ckpRequest.secretKeyFile = argv[4];
    ckpRequest.query_parameters[ "Action" ] = "CreateKeyPair";
    ckpRequest.query_parameters[ "KeyName" ] = argv[5];
    ckpRequest.privateKeyFileName = argv[6];

    // Send the request.
    if( ! ckpRequest.SendRequest() ) {
        result_string = create_failure_result( requestID,
            ckpRequest.errorMessage.c_str(),
            ckpRequest.errorCode.c_str() );
    } else {
        result_string = create_success_result( requestID, NULL );
    }

    return true;
}

// ---------------------------------------------------------------------------

AmazonVMDestroyKeypair::~AmazonVMDestroyKeypair() { }

// Expecting:EC2_VM_DESTROY_KEYPAIR <req_id> <serviceurl> <accesskeyfile> <secretkeyfile> <keyname>
bool AmazonVMDestroyKeypair::workerFunction(char **argv, int argc, std::string &result_string) {
    assert( strcasecmp( argv[0], "EC2_VM_DESTROY_KEYPAIR" ) == 0 );

    // Uses the Query API function 'DeleteKeyPair', as documented at
    // http://docs.amazonwebservices.com/AWSEC2/latest/APIReference/ApiReference-query-DeleteKeyPair.html

    int requestID;
    get_int( argv[1], & requestID );
    dprintf( D_PERF_TRACE, "request #%d (%s): work begins\n",
        requestID, argv[0] );

    if( ! verify_min_number_args( argc, 6 ) ) {
        result_string = create_failure_result( requestID, "Wrong_Argument_Number" );
        dprintf( D_ALWAYS, "Wrong number of arguments (%d should be >= %d) to %s\n",
                 argc, 6, argv[0] );
        return false;
    }

    // Fill in required attributes & parameters.
    AmazonVMDestroyKeypair dkpRequest = AmazonVMDestroyKeypair( requestID, argv[0] );
    dkpRequest.serviceURL = argv[2];
    dkpRequest.accessKeyFile = argv[3];
    dkpRequest.secretKeyFile = argv[4];
    dkpRequest.query_parameters[ "Action" ] = "DeleteKeyPair";
    dkpRequest.query_parameters[ "KeyName" ] = argv[5];

    // Send the request.
    if( ! dkpRequest.SendRequest() ) {
        result_string = create_failure_result( requestID,
            dkpRequest.errorMessage.c_str(),
            dkpRequest.errorCode.c_str() );
    } else {
        result_string = create_success_result( requestID, NULL );
    }

    return true;
}

// ---------------------------------------------------------------------------

AmazonAssociateAddress::~AmazonAssociateAddress() { }

// Expecting:EC2_VM_ASSOCIATE_ADDRESS <req_id> <serviceurl> <accesskeyfile> <secretkeyfile> <instance-id> <elastic-ip>
bool AmazonAssociateAddress::workerFunction(char **argv, int argc, std::string &result_string) {
    assert( strcasecmp( argv[0], "EC2_VM_ASSOCIATE_ADDRESS" ) == 0 );

    // Uses the Query API function 'AssociateAddress', as documented at
    // http://docs.amazonwebservices.com/AWSEC2/latest/APIReference/index.html?ApiReference-query-AssociateAddress.html

    int requestID;
    get_int( argv[1], & requestID );
    dprintf( D_PERF_TRACE, "request #%d (%s): work begins\n",
        requestID, argv[0] );

    if( ! verify_min_number_args( argc, 7 ) ) {
        result_string = create_failure_result( requestID, "Wrong_Argument_Number" );
        dprintf( D_ALWAYS, "Wrong number of arguments (%d should be >= %d) to %s\n", argc, 7, argv[0] );
        return false;
    }

    // Fill in required attributes & parameters.
    AmazonAssociateAddress asRequest = AmazonAssociateAddress( requestID, argv[0] );
    asRequest.serviceURL = argv[2];
    asRequest.accessKeyFile = argv[3];
    asRequest.secretKeyFile = argv[4];
    asRequest.query_parameters[ "Action" ] = "AssociateAddress";
    asRequest.query_parameters[ "InstanceId" ] = argv[5];

    // here it could be a standard ip or a vpc ip
    // vpc ip's will have a : separating
    const char * pszFullIPStr = argv[6];
    const char * pszIPStr=0;
    const char * pszAllocationId=0;
    std::vector<std::string> elastic_ip_addr_info = split(pszFullIPStr, ":");
    pszIPStr = elastic_ip_addr_info[0].c_str();
    pszAllocationId=elastic_ip_addr_info[1].c_str();

    if ( pszAllocationId )
    {
        asRequest.query_parameters[ "AllocationId" ] = pszAllocationId;
    }
    else
    {
        asRequest.query_parameters[ "PublicIp" ] = pszIPStr;
    }

    // Send the request.
    if( ! asRequest.SendRequest() ) {
        result_string = create_failure_result( requestID,
            asRequest.errorMessage.c_str(),
            asRequest.errorCode.c_str() );
    } else {
        result_string = create_success_result( requestID, NULL );
    }

    return true;
}


// ---------------------------------------------------------------------------

AmazonCreateTags::~AmazonCreateTags() { }

bool AmazonCreateTags::ioCheck(char **argv, int argc)
{
    return verify_min_number_args(argc, 8) &&
        verify_request_id(argv[1]) &&
        verify_string_name(argv[2]) &&
        verify_string_name(argv[3]) &&
        verify_string_name(argv[4]) &&
        verify_string_name(argv[5]) &&
        verify_string_name(argv[6]) &&
        verify_string_name(argv[7]);
}

// Expecting:
//   EC2_VM_CREATE_TAGS <req_id> <serviceurl> <accesskeyfile> <secretkeyfile> <instance-id> <tag name>=<tag value>* NULLSTRING
bool
AmazonCreateTags::workerFunction(char **argv,
                                 int argc,
                                 std::string &result_string)
{
    assert(strcasecmp(argv[0], "EC2_VM_CREATE_TAGS") == 0);

    // Uses the Query API function 'CreateTags', as documented at
    // http://docs.amazonwebservices.com/AWSEC2/latest/APIReference/index.html?ApiReference-query-CreateTags.html

    int requestID;
    get_int(argv[1], &requestID);
    dprintf( D_PERF_TRACE, "request #%d (%s): work begins\n",
        requestID, argv[0] );

    if (!verify_min_number_args(argc, 7)) {
        result_string = create_failure_result(requestID, "Wrong_Argument_Number");
        dprintf(D_ALWAYS,
                "Wrong number of arguments (%d should be >= %d) to %s\n",
                argc, 7, argv[0]);
        return false;
    }

    // Fill in required attributes & parameters.
    AmazonCreateTags asRequest = AmazonCreateTags( requestID, argv[0] );
    asRequest.serviceURL = argv[2];
    asRequest.accessKeyFile = argv[3];
    asRequest.secretKeyFile = argv[4];
    asRequest.query_parameters["Action"] = "CreateTags";
    asRequest.query_parameters["ResourceId.0"] = argv[5];

    std::string tag;
    for (int i = 6; i < argc - 1; i++) {
        std::stringstream ss;
        ss << "Tag." << (i - 6);
        tag = argv[i];
        asRequest.query_parameters[ss.str().append(".Key")] =
            tag.substr(0, tag.find('='));
        asRequest.query_parameters[ss.str().append(".Value")] =
            tag.substr(tag.find('=') + 1);
    }

    // Send the request.
    if (!asRequest.SendRequest()) {
        result_string = create_failure_result(requestID,
                                              asRequest.errorMessage.c_str(),
                                              asRequest.errorCode.c_str());
    } else {
        result_string = create_success_result(requestID, NULL);
    }

    return true;
}


// ---------------------------------------------------------------------------

AmazonAttachVolume::~AmazonAttachVolume() { }

bool AmazonAttachVolume::workerFunction(char **argv, int argc, std::string &result_string) 
{
    assert( strcasecmp( argv[0], "EC2_VM_ATTACH_VOLUME" ) == 0 );

    int requestID;
    get_int( argv[1], & requestID );
    dprintf( D_PERF_TRACE, "request #%d (%s): work begins\n",
        requestID, argv[0] );

    if( ! verify_min_number_args( argc, 8 ) ) {
        result_string = create_failure_result( requestID, "Wrong_Argument_Number" );
        dprintf( D_ALWAYS, "Wrong number of arguments (%d should be >= %d) to %s\n", argc, 8, argv[0] );
        return false;
    }

    // Fill in required attributes & parameters.
    AmazonAttachVolume asRequest = AmazonAttachVolume( requestID, argv[0] );
    asRequest.serviceURL = argv[2];
    asRequest.accessKeyFile = argv[3];
    asRequest.secretKeyFile = argv[4];
    asRequest.query_parameters[ "Action" ] = "AttachVolume";
    asRequest.query_parameters[ "VolumeId" ] = argv[5];
    asRequest.query_parameters[ "InstanceId" ] = argv[6];
    asRequest.query_parameters[ "Device" ] = argv[7];

    // Send the request.
    if( ! asRequest.SendRequest() ) {
        result_string = create_failure_result( requestID,
            asRequest.errorMessage.c_str(),
            asRequest.errorCode.c_str() );
    } else {
        result_string = create_success_result( requestID, NULL );
    }

    return true;

}


// ---------------------------------------------------------------------------

// This command issues a DescribeKeyPairs request and analyzes the
// response to detect which of several popular implementations of EC2
// the server is running.
// The types detected are Amazon, OpenStack, Nimbus, and Eucalyptus.
// If the server's response doesn't match any of these types, then
// "Unknown" is returned.
// The following checks are performed:
// * Amazon's header includes "Server: AmazonEC2"
// * Nimbus's header includes "Server: Jetty"
// * Eucalyptus's body doesn't include an "<?xml ...?>" tag
//   Neither does OpenStack's starting with version Havana
// * Amazon and Nimbus's <?xml?> tag includes version="1.0" and
//   encoding="UTF-8" properties
// * Nimbus and Eucalyptus's response doesn't include a <requestId> tag
// * Eucalyptus's response puts all XML elements in a "euca" scope

AmazonVMServerType::~AmazonVMServerType() { }

bool AmazonVMServerType::SendRequest() {
    bool result = AmazonRequest::SendRequest();
    if( result ) {
        serverType = "Unknown";
        size_t idx = this->resultString.find( "\r\n\r\n" );
        if ( idx == std::string::npos ) {
            return result;
        }

        std::string header = this->resultString.substr( 0, idx + 4 );
        std::string body = this->resultString.substr( idx + 4 );

        bool server_amazon = false;
        bool server_jetty = false;
        bool xml_tag = false;
        bool xml_encoding = false;
        bool request_id = false;
        bool euca_tag = false;

        if ( header.find( "Server: AmazonEC2" ) != std::string::npos ) {
            server_amazon = true;
        }
        if ( header.find( "Server: Jetty" ) != std::string::npos ) {
            server_jetty = true;
        }
        if ( body.find( "<?xml " ) != std::string::npos ) {
            xml_tag = true;
        }
        if ( body.find( "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" ) != std::string::npos ) {
            xml_encoding = true;
        }
        if ( body.find( "<requestId>" ) != std::string::npos ) {
            request_id = true;
        }
        if ( body.find( "<euca:Describe" ) != std::string::npos ) {
            euca_tag = true;
        }

        if ( server_amazon && !server_jetty && xml_tag &&
             xml_encoding && request_id && !euca_tag ) {
            serverType = "Amazon";
        } else if ( !server_amazon && !server_jetty && xml_tag &&
                    !xml_encoding && request_id && !euca_tag ) {
            serverType = "OpenStack";
        } else if ( !server_amazon && !server_jetty && !xml_tag &&
                    !xml_encoding && request_id && !euca_tag ) {
            // OpenStack Havana altered formatting from previous versions,
            // but we don't want to treat it differently for now.
            serverType = "OpenStack";
        } else if ( !server_amazon && server_jetty && xml_tag &&
                    xml_encoding && !request_id && !euca_tag ) {
            serverType = "Nimbus";
        } else if ( !server_amazon && !server_jetty && !xml_tag &&
                    !xml_encoding && !request_id && euca_tag ) {
            serverType = "Eucalyptus";
        }
    }
    return result;
}

// Expecting:EC2_VM_SERVER_TYPE <req_id> <serviceurl> <accesskeyfile> <secretkeyfile>
bool AmazonVMServerType::workerFunction(char **argv, int argc, std::string &result_string) {
    assert( strcmp( argv[0], "EC2_VM_SERVER_TYPE" ) == 0 );

    // Uses the Query API function 'DescribeKeyPairs', as documented at
    // http://docs.amazonwebservices.com/AWSEC2/latest/APIReference/ApiReference-query-DescribeKeyPairs.html

    int requestID;
    get_int( argv[1], & requestID );
    dprintf( D_PERF_TRACE, "request #%d (%s): work begins\n",
        requestID, argv[0] );

    if( ! verify_min_number_args( argc, 5 ) ) {
        result_string = create_failure_result( requestID, "Wrong_Argument_Number" );
        dprintf( D_ALWAYS, "Wrong number of arguments (%d should be >= %d) to %s\n",
                 argc, 5, argv[0] );
        return false;
    }

    // Fill in required attributes & parameters.
    AmazonVMServerType serverTypeRequest = AmazonVMServerType( requestID, argv[0] );
    serverTypeRequest.serviceURL = argv[2];
    serverTypeRequest.accessKeyFile = argv[3];
    serverTypeRequest.secretKeyFile = argv[4];
    serverTypeRequest.query_parameters[ "Action" ] = "DescribeKeyPairs";

    // We need the header in the response to help determine what
    // implementation we're talking with.
    serverTypeRequest.includeResponseHeader = true;

    // Send the request.
    if( ! serverTypeRequest.SendRequest() ) {
        result_string = create_failure_result( requestID,
                                        serverTypeRequest.errorMessage.c_str(),
                                        serverTypeRequest.errorCode.c_str() );
    } else {
        std::vector<std::string> resultList;
        resultList.emplace_back( serverTypeRequest.serverType );
        result_string = create_success_result( requestID, & resultList );
    }

    return true;
}

// ---------------------------------------------------------------------------

AmazonBulkStart::~AmazonBulkStart() { }

struct bulkStartUD_t {
    bool inRequestId;
    std::string & requestID;

    bulkStartUD_t( std::string & rid ) : inRequestId( false ), requestID( rid ) { }
};
typedef struct bulkStartUD_t bulkStartUD;

void bulkStartESH( void * vUserData, const XML_Char * name, const XML_Char ** ) {
    bulkStartUD * bsud = (bulkStartUD *)vUserData;
    if( strcasecmp( ignoringNameSpace( name ), "spotFleetRequestId" ) == 0 ) {
        bsud->inRequestId = true;
    }
}

void bulkStartCDH( void * vUserData, const XML_Char * cdata, int len ) {
    bulkStartUD * bsud = (bulkStartUD *)vUserData;
    if( bsud->inRequestId ) {
        appendToString( (const void *)cdata, len, 1, (void *) & bsud->requestID );
    }
}

void bulkStartEEH( void * vUserData, const XML_Char * name ) {
    bulkStartUD * bsud = (bulkStartUD *)vUserData;
    if( strcasecmp( ignoringNameSpace( name ), "spotFleetRequestId" ) == 0 ) {
        bsud->inRequestId = false;
    }
}

bool AmazonBulkStart::SendRequest() {
	bool result = AmazonRequest::SendRequest();
	if( result ) {
        bulkStartUD bsud( this->bulkRequestID );
        XML_Parser xp = XML_ParserCreate( NULL );
        XML_SetElementHandler( xp, & bulkStartESH, & bulkStartEEH );
        XML_SetCharacterDataHandler( xp, & bulkStartCDH );
        XML_SetUserData( xp, & bsud );
        XML_Parse( xp, this->resultString.c_str(), this->resultString.length(), 1 );
        XML_ParserFree( xp );
	} else {
		if( this->errorCode == "E_CURL_IO" ) {
			// To be on the safe side, if the I/O failed, the annexd should
			// check to see the Spot Fleet was started or not.
			this->errorCode = "NEED_CHECK_BULK_START";
			return false;
		}
	}
	return result;
}

bool parseJSON( char * s, std::map< std::string, std::string > & m, unsigned & i ) {
	if( s[0] != '{' ) { return false; }

	enum STATE { ATTR_OPEN, ATTR_CLOSE, COLON, VALUE_OPEN, VALUE_CLOSE, COMMA };

	STATE state = ATTR_OPEN;
	std::string attribute;
	unsigned stringBegan = 0;
	for( i = 1; s[i] != '\0'; ++i ) {
		switch( state ) {
			case ATTR_OPEN:
				if( s[i] == ' ' ) { continue; }
				if( s[i] == '"' ) {
					stringBegan = i + 1;
					state = ATTR_CLOSE;
					continue;
				}
				return false;

			case ATTR_CLOSE:
				if( s[i] == '"' && s[i-1] != '\\' ) {
					attribute = std::string( & s[stringBegan], i - stringBegan );
				  	// dprintf( D_ALWAYS, "Found attribute '%s'.\n", attribute.c_str() );
					state = COLON;
				}
				continue;

			case COLON:
				if( s[i] == ' ' ) { continue; }
				if( s[i] == ':' ) { state = VALUE_OPEN; continue; }
				return false;

			case VALUE_OPEN:
				if( s[i] == ' ' ) { continue; }
				if( s[i] == '"' ) {
					stringBegan = i + 1;
					state = VALUE_CLOSE;
					continue;
				}
				return false;

			case VALUE_CLOSE:
				if( s[i] == '"' && s[i-1] != '\\' ) {
					std::string value( & s[stringBegan], i - stringBegan );
				  	// dprintf( D_ALWAYS, "Found value '%s'.\n", value.c_str() );
				  	m[ attribute ] = value;
					state = COMMA;
				}
				continue;

			case COMMA:
				if( s[i] == ' ' ) { continue; }
				if( s[i] == ',' ) { state = ATTR_OPEN; continue; }
				if( s[i] == '}' ) { return true; }
				return false;
		}
	}

	return false;
}

void AmazonBulkStart::setLaunchSpecificationAttribute(
		int lcIndex, std::map< std::string, std::string > & blob,
		const char * lcAttributeName,
		const char * blobAttributeName ) {
	std::ostringstream ss;
	ss << "SpotFleetRequestConfig.LaunchSpecifications.";
	ss << lcIndex << "." << lcAttributeName;

	std::string & value = blob[ ( blobAttributeName == NULL ? lcAttributeName : blobAttributeName ) ];
	if(! value.empty()) {
		query_parameters[ ss.str() ] = value;
	}
}

bool AmazonBulkStart::workerFunction( char ** argv, int argc, std::string & result_string ) {
	assert( strcasecmp( argv[0], "EC2_BULK_START" ) == 0 );

	int requestID;
	get_int( argv[1], & requestID );
	dprintf( D_PERF_TRACE, "request #%d (%s): work begins\n",
		requestID, argv[0] );

	if( ! verify_min_number_args( argc, 12 ) ) {
		result_string = create_failure_result( requestID, "Wrong_Argument_Number" );
		dprintf( D_ALWAYS, "Wrong number of arguments (%d should be >= %d) to %s\n",
			argc, 12, argv[0] );
		return false;
	}

	// Fill in required attributes.
	AmazonBulkStart request = AmazonBulkStart( requestID, argv[0] );
	request.serviceURL = argv[2];
	request.accessKeyFile = argv[3];
	request.secretKeyFile = argv[4];

	// Fill in required parameters.
	request.query_parameters[ "Action" ] = "RequestSpotFleet";
	// The Spot Fleet API is relatively new.  Set the version here, rather
	// than update the minimum necessary, in case somebody wants to use the
	// non-annex functionality with an older-interface service.  2015-04-15
	// is the oldest API with Spot Fleet, but this code was implemented
	// against the 2016-04-01 documentation.
	// request.query_parameters[ "Version" ] = "2016-04-01";
	// We need version 2016-11-15 for Spot Fleet tags.
	request.query_parameters[ "Version" ] = "2016-11-15";

	if( strcasecmp( argv[5], NULLSTRING ) ) {
		request.query_parameters[ "SpotFleetRequestConfig."
			"ClientToken" ] = argv[5];
	}
	request.query_parameters[ "SpotFleetRequestConfig."
		"SpotPrice" ] = argv[6];
	request.query_parameters[ "SpotFleetRequestConfig."
		"TargetCapacity" ] = argv[7];
	request.query_parameters[ "SpotFleetRequestConfig."
		"IamFleetRole" ] = argv[8];
	request.query_parameters[ "SpotFleetRequestConfig."
		"AllocationStrategy" ] = argv[9];

	request.query_parameters[ "SpotFleetRequestConfig."
		"ExcessCapacityTerminationPolicy" ] = "noTermination";
	request.query_parameters[ "SpotFleetRequestConfig."
		"TerminateInstancesWithExpiration" ] = "false";
	request.query_parameters[ "SpotFleetRequestConfig."
		"Type" ] = "request";

	if( strcmp( argv[10], NULLSTRING ) ) {
		request.query_parameters[ "SpotFleetRequestConfig."
			"ValidUntil" ] = argv[10];
	}

	for( int i = 11; i < argc; ++i ) {
		if( strcmp( argv[i], NULLSTRING ) == 0 ) { break; }

		int lcIndex = i - 10;

		// argv[i] is a JSON blob, because otherwise things got complicated.
		// Luckily, we don't have handle generic JSON, just the single-level
		// string-to-string stuff the annexd sends us.
		unsigned errChar = 0;
		std::map< std::string, std::string > blob;
		if(! parseJSON( argv[i], blob, errChar )) {
			dprintf( D_ALWAYS, "Got bogus launch configuration, noticed around character %u.\n", errChar );
			dprintf( D_ALWAYS, "%s\n", argv[i] );
			std::string spaces; spaces.replace( 0, std::string::npos, errChar, ' ' );
			dprintf( D_ALWAYS, "%s^\n", spaces.c_str() );
			return false;
		}

		request.setLaunchSpecificationAttribute( lcIndex, blob,
			"ImageId" );
		request.setLaunchSpecificationAttribute( lcIndex, blob,
			"SpotPrice" );
		request.setLaunchSpecificationAttribute( lcIndex, blob,
			"KeyName" );
		request.setLaunchSpecificationAttribute( lcIndex, blob,
			"UserData", "UserData" );

		request.setLaunchSpecificationAttribute( lcIndex, blob,
			"InstanceType" );
		request.setLaunchSpecificationAttribute( lcIndex, blob,
			"SubnetId" );

		request.setLaunchSpecificationAttribute( lcIndex, blob,
			"WeightedCapacity" );

		request.setLaunchSpecificationAttribute( lcIndex, blob,
			"IamInstanceProfile.Arn", "IAMProfileARN" );
		request.setLaunchSpecificationAttribute( lcIndex, blob,
			"IamInstanceProfile.Name", "IAMProfileName" );

		request.setLaunchSpecificationAttribute( lcIndex, blob,
			"SpotPlacement.AvailabilityZone", "AvailabilityZone" );

		if(! blob[ "Tags" ].empty()) {
			size_t j = 1;
			for (const auto& tag : StringTokenIterator(blob["Tags"])) {
				std::ostringstream ss;
				ss << "SpotFleetRequestConfig.LaunchSpecifications.";
				ss << lcIndex << ".";

				// Once again, the AWS documentation has this wrong.
				ss << "TagSpecificationSet.1.";
				request.query_parameters[ ss.str() + "ResourceType" ] = "instance";
				ss << "Tag." << j << ".";

				request.query_parameters[ ss.str() + "Key" ] =
					tag.substr( 0, tag.find( '=' ) );
				request.query_parameters[ ss.str() + "Value" ] =
					tag.substr( tag.find( '=' ) + 1 );
				j++;
			}
		}

		if(! blob[ "SecurityGroupNames" ].empty()) {
			size_t j = 1;
			for (const auto& groupName : StringTokenIterator(blob["SecurityGroupNames"])) {
				std::ostringstream ss;
				ss << "SpotFleetRequestConfig.LaunchSpecifications.";
				ss << lcIndex << ".";
				// AWS' documentation is wrong, claims this is 'SecurityGroups'.
				ss << "GroupSet." << j << ".GroupName";
				request.query_parameters[ ss.str() ] = groupName;
				j++;
			}
		}

		if(! blob[ "SecurityGroupIDs" ].empty()) {
			size_t j = 1;
			for (const auto& groupID : StringTokenIterator(blob["SecurityGroupIDs"])) {
				std::ostringstream ss;
				ss << "SpotFleetRequestConfig.LaunchSpecifications.";
				ss << lcIndex << ".";
				// AWS' documentation is wrong, claims this is 'SecurityGroups'.
				ss << "GroupSet." << j << ".GroupId";
				request.query_parameters[ ss.str() ] = groupID;
				j++;
			}
		}

		if(! blob[ "BlockDeviceMapping" ].empty() ) {
			size_t j = 1;
			for (const auto& mapping : StringTokenIterator(blob["BlockDeviceMapping"])) {
				std::ostringstream ss;
				ss << "SpotFleetRequestConfig.LaunchSpecifications.";
				ss << lcIndex << ".";
				// AWS' documentation is wrong, claims this is plural.
				ss << "BlockDeviceMapping." << j;

				if( strchr( mapping.c_str(), '=' ) != NULL ) {
					// New-style mapping (copied from AWS web console).
					std::vector<std::string> pair = split( mapping, "=" );

					if( pair.size() != 2 ) {
						dprintf( D_ALWAYS, "Ignoring invalid block device mapping '%s'.\n", mapping.c_str() );
						continue;
					}

					std::string deviceName = ss.str() + ".DeviceName";
					request.query_parameters[ deviceName ] = pair[0];

					std::vector<std::string> quad = split(pair[1], ":");
					if( quad.size() != 4 ) {
						dprintf( D_ALWAYS, "Ignoring invalid block device mapping '%s'.\n", mapping.c_str() );
						continue;
					}

					ss << ".Ebs.";
					std::string sid = ss.str() + "SnapshotId";
					request.query_parameters[ sid ] = quad[0];
					std::string vs = ss.str() + "VolumeSize";
					request.query_parameters[ vs ] = quad[1];
					std::string dot = ss.str() + "DeleteOnTermination";
					request.query_parameters[ dot ] = quad[2];
					std::string vt = ss.str() + "VolumeType";
					request.query_parameters[ vt ] = quad[3];
				} else {
					// Handle old-style mapping.
					std::vector<std::string> pair = split( mapping, ":" );
					if( pair.size() != 2 ) {
						dprintf( D_ALWAYS, "Ignoring invalid block device mapping '%s'.\n", mapping.c_str() );
						continue;
					}

					std::string virtualName = ss.str() + ".VirtualName";
					request.query_parameters[ virtualName ] = pair[0];
					std::string deviceName = ss.str() + ".DeviceName";
					request.query_parameters[ deviceName ] = pair[1];
				}
				j++;
			}
		}
	}

#if 0
	AttributeValueMap::const_iterator i;
	for( i = request.query_parameters.begin(); i != request.query_parameters.end(); ++i ) {
		std::string name = amazonURLEncode( i->first );
		std::string value = amazonURLEncode( i->second );
		dprintf( D_ALWAYS, "Found query parameter '%s' = '%s'.\n", name.c_str(), value.c_str() );
	}
#endif /* 0 */

	if( ! request.SendRequest() ) {
		result_string = create_failure_result( requestID,
			request.errorMessage.c_str(),
			request.errorCode.c_str() );
	} else {
		if( request.bulkRequestID.empty() ) {
			result_string = create_failure_result( requestID, "Could not find bulk request ID in response from server.  Check the EC2 GAHP log for details.", "E_NO_BULK_REQUEST_ID" );
		} else {
			std::vector<std::string> sl;
			sl.emplace_back( request.bulkRequestID );
			result_string = create_success_result( requestID, & sl );
		}
	}

	return true;
}

// ---------------------------------------------------------------------------

AmazonPutRule::~AmazonPutRule() { }

bool AmazonPutRule::SendJSONRequest( const std::string & payload ) {
	bool result = AmazonRequest::SendJSONRequest( payload );
	if( result ) {
		// We could use a dedicated JSON parser, but its API is annoying,
		// and the ClassAd converter is good enough.
		ClassAd reply;
		classad::ClassAdJsonParser cajp;
		if(! cajp.ParseClassAd( this->resultString, reply, true )) {
			dprintf( D_ALWAYS, "Failed to parse reply '%s' as JSON.\n", this->resultString.c_str() );
			return false;
		}
		reply.LookupString( "RuleArn", this->ruleARN );
	}
	return result;
}

bool AmazonPutRule::workerFunction( char ** argv, int argc, std::string & result_string ) {
	assert( strcasecmp( argv[0], AMAZON_COMMAND_PUT_RULE ) == 0 );
	int requestID;
	get_int( argv[1], & requestID );
	dprintf( D_PERF_TRACE, "request #%d (%s): work begins\n",
		requestID, argv[0] );

	if( ! verify_min_number_args( argc, 8 ) ) {
		result_string = create_failure_result( requestID, "Wrong_Argument_Number" );
		dprintf( D_ALWAYS, "Wrong number of arguments (%d should be >= %d) to %s\n",
			argc, 8, argv[0] );
		return false;
	}

	// Fill in required attributes.
	AmazonPutRule request = AmazonPutRule( requestID, argv[0] );
	request.serviceURL = argv[2];
	request.accessKeyFile = argv[3];
	request.secretKeyFile = argv[4];

	// Set the required headers.  (SendJSONRequest() sets the content-type.)
	request.headers[ "X-Amz-Target" ] = "AWSEvents.PutRule";

	// Construct the JSON payload.
	std::string name, scheduleExpression, state;
	classad::ClassAdJsonUnParser::UnparseAuxEscapeString( name, argv[5] );
	classad::ClassAdJsonUnParser::UnparseAuxEscapeString( scheduleExpression, argv[6] );
	classad::ClassAdJsonUnParser::UnparseAuxEscapeString( state, argv[7] );

	std::string payload;
	formatstr( payload, "{\n"
				"\"Name\": \"%s\",\n"
				"\"ScheduleExpression\": \"%s\",\n"
				"\"State\": \"%s\"\n"
				"}\n",
				name.c_str(), scheduleExpression.c_str(), state.c_str() );

	if( ! request.SendJSONRequest( payload ) ) {
		result_string = create_failure_result( requestID,
			request.errorMessage.c_str(),
			request.errorCode.c_str() );
	} else {
		if( request.ruleARN.empty() ) {
			result_string = create_failure_result( requestID, "Could not find name ARN in response from server.  Check the EC2 GAHP log for details.", "E_NO_NAME_ARN" );
		} else {
			std::vector<std::string> sl;
			sl.emplace_back( request.ruleARN );
			result_string = create_success_result( requestID, & sl );
		}
	}

	return true;
}

// ---------------------------------------------------------------------------

AmazonPutTargets::~AmazonPutTargets() { }

bool AmazonPutTargets::SendJSONRequest( const std::string & payload ) {
	bool result = AmazonRequest::SendJSONRequest( payload );
	if( result ) {
		// We could use a dedicated JSON parser, but its API is annoying,
		// and the ClassAd converter is good enough.
		ClassAd reply;
		classad::ClassAdJsonParser cajp;
		if(! cajp.ParseClassAd( this->resultString, reply, true )) {
			dprintf( D_ALWAYS, "Failed to parse reply '%s' as JSON.\n", this->resultString.c_str() );
			return false;
		}

		int failedEntryCount;
		if(! reply.LookupInteger( "FailedEntryCount", failedEntryCount ) ) {
			dprintf( D_ALWAYS, "Reply '%s' did contain FailedEntryCount attribute.\n", this->resultString.c_str() );
			return false;
		}
		if( failedEntryCount != 0 ) {
			dprintf( D_ALWAYS, "Reply '%s' indicates a failure.\n", this->resultString.c_str() );
			return false;
		}
	}
	return result;
}

bool AmazonPutTargets::workerFunction( char ** argv, int argc, std::string & result_string ) {
	assert( strcasecmp( argv[0], AMAZON_COMMAND_PUT_TARGETS ) == 0 );
	int requestID;
	get_int( argv[1], & requestID );
	dprintf( D_PERF_TRACE, "request #%d (%s): work begins\n",
		requestID, argv[0] );

	if( ! verify_min_number_args( argc, 9 ) ) {
		result_string = create_failure_result( requestID, "Wrong_Argument_Number" );
		dprintf( D_ALWAYS, "Wrong number of arguments (%d should be >= %d) to %s\n",
			argc, 9, argv[0] );
		return false;
	}

	// Fill in required attributes.
	AmazonPutTargets request = AmazonPutTargets( requestID, argv[0] );
	request.serviceURL = argv[2];
	request.accessKeyFile = argv[3];
	request.secretKeyFile = argv[4];

	// Set the required headers.  (SendJSONRequest() sets the content-type.)
	request.headers[ "X-Amz-Target" ] = "AWSEvents.PutTargets";

	// Construct the JSON payload.
	std::string ruleName, targetID, functionARN, inputString;
	classad::ClassAdJsonUnParser::UnparseAuxEscapeString( ruleName, argv[5] );
	classad::ClassAdJsonUnParser::UnparseAuxEscapeString( targetID, argv[6] );
	classad::ClassAdJsonUnParser::UnparseAuxEscapeString( functionARN, argv[7] );
	classad::ClassAdJsonUnParser::UnparseAuxEscapeString( inputString, argv[8] );

	std::string payload;
	formatstr( payload, "{\n"
				"\"Rule\": \"%s\",\n"
				"\"Targets\": [\n"
				"    {\n"
				"    \"Id\": \"%s\",\n"
				"    \"Arn\": \"%s\",\n"
				"    \"Input\": \"%s\"\n"
				"    }\n"
				"]\n"
				"}\n",
				ruleName.c_str(), targetID.c_str(),
				functionARN.c_str(), inputString.c_str() );

	if( ! request.SendJSONRequest( payload ) ) {
		result_string = create_failure_result( requestID,
			request.errorMessage.c_str(),
			request.errorCode.c_str() );
	} else {
		result_string = create_success_result( requestID, NULL );
	}

	return true;
}

// ---------------------------------------------------------------------------

AmazonRemoveTargets::~AmazonRemoveTargets() { }

bool AmazonRemoveTargets::SendJSONRequest( const std::string & payload ) {
	bool result = AmazonRequest::SendJSONRequest( payload );
	if( result ) {
		ClassAd reply;
		classad::ClassAdJsonParser cajp;
		if(! cajp.ParseClassAd( this->resultString, reply, true )) {
			dprintf( D_ALWAYS, "Failed to parse reply '%s' as JSON.\n", this->resultString.c_str() );
			return false;
		}

		int failedEntryCount;
		if(! reply.LookupInteger( "FailedEntryCount", failedEntryCount ) ) {
			dprintf( D_ALWAYS, "Reply '%s' did contain FailedEntryCount attribute.\n", this->resultString.c_str() );
			return false;
		}
		if( failedEntryCount != 0 ) {
			dprintf( D_ALWAYS, "Reply '%s' indicates a failure.\n", this->resultString.c_str() );
			return false;
		}
	}
	return result;
}

bool AmazonRemoveTargets::workerFunction( char ** argv, int argc, std::string & result_string ) {
	assert( strcasecmp( argv[0], AMAZON_COMMAND_REMOVE_TARGETS ) == 0 );
	int requestID;
	get_int( argv[1], & requestID );
	dprintf( D_PERF_TRACE, "request #%d (%s): work begins\n",
		requestID, argv[0] );

	if( ! verify_min_number_args( argc, 7 ) ) {
		result_string = create_failure_result( requestID, "Wrong_Argument_Number" );
		dprintf( D_ALWAYS, "Wrong number of arguments (%d should be >= %d) to %s\n",
			argc, 7, argv[0] );
		return false;
	}

	// Fill in required attributes.
	AmazonRemoveTargets request = AmazonRemoveTargets( requestID, argv[0] );
	request.serviceURL = argv[2];
	request.accessKeyFile = argv[3];
	request.secretKeyFile = argv[4];

	// Set the required headers.  (SendJSONRequest() sets the content-type.)
	request.headers[ "X-Amz-Target" ] = "AWSEvents.RemoveTargets";

	// Construct the JSON payload.
	std::string ruleName, id;
	classad::ClassAdJsonUnParser::UnparseAuxEscapeString( ruleName, argv[5] );
	classad::ClassAdJsonUnParser::UnparseAuxEscapeString( id, argv[6] );

	std::string payload;
	formatstr( payload, "{\n"
				"\"Ids\": [ \"%s\" ],\n"
				"\"Rule\": \"%s\"\n"
				"}\n",
				ruleName.c_str(), id.c_str() );

	if( ! request.SendJSONRequest( payload ) ) {
		result_string = create_failure_result( requestID,
			request.errorMessage.c_str(),
			request.errorCode.c_str() );
	} else {
		result_string = create_success_result( requestID, NULL );
	}

	return true;
}

// ---------------------------------------------------------------------------

AmazonDeleteRule::~AmazonDeleteRule() { }

bool AmazonDeleteRule::SendJSONRequest( const std::string & payload ) {
	bool result = AmazonRequest::SendJSONRequest( payload );
	// Succesful results are defined to be empty.
	return result;
}

bool AmazonDeleteRule::workerFunction( char ** argv, int argc, std::string & result_string ) {
	assert( strcasecmp( argv[0], AMAZON_COMMAND_DELETE_RULE ) == 0 );
	int requestID;
	get_int( argv[1], & requestID );
	dprintf( D_PERF_TRACE, "request #%d (%s): work begins\n",
		requestID, argv[0] );

	if( ! verify_min_number_args( argc, 6 ) ) {
		result_string = create_failure_result( requestID, "Wrong_Argument_Number" );
		dprintf( D_ALWAYS, "Wrong number of arguments (%d should be >= %d) to %s\n",
			argc, 6, argv[0] );
		return false;
	}

	// Fill in required attributes.
	AmazonDeleteRule request = AmazonDeleteRule( requestID, argv[0] );
	request.serviceURL = argv[2];
	request.accessKeyFile = argv[3];
	request.secretKeyFile = argv[4];

	// Set the required headers.  (SendJSONRequest() sets the content-type.)
	request.headers[ "X-Amz-Target" ] = "AWSEvents.DeleteRule";

	// Construct the JSON payload.
	std::string ruleName;
	classad::ClassAdJsonUnParser::UnparseAuxEscapeString( ruleName, argv[5] );

	std::string payload;
	formatstr( payload, "{\n"
				"\"Name\": \"%s\"\n"
				"}\n",
				ruleName.c_str() );

	if( ! request.SendJSONRequest( payload ) ) {
		result_string = create_failure_result( requestID,
			request.errorMessage.c_str(),
			request.errorCode.c_str() );
	} else {
		result_string = create_success_result( requestID, NULL );
	}

	return true;
}

// ---------------------------------------------------------------------------

AmazonBulkStop::~AmazonBulkStop() { }

struct bulkStopUD_t {
	bool inFailure;
	bool & success;

    bulkStopUD_t( bool & s ) : inFailure( false ), success( s ) { }
};
typedef struct bulkStopUD_t bulkStopUD;

void bulkStopESH( void * vUserData, const XML_Char * name, const XML_Char ** ) {
    bulkStopUD * bsud = (bulkStopUD *)vUserData;
    if( strcasecmp( ignoringNameSpace( name ), "unsuccessfulFleetRequestSet" ) == 0 ) {
    	bsud->inFailure = true;
    }
    if( bsud->inFailure && strcasecmp( ignoringNameSpace( name ), "spotFleetRequestId" ) == 0 ) {
        bsud->success = false;
    }
}

void bulkStopCDH( void *, const XML_Char *, int ) {
}

void bulkStopEEH( void * vUserData, const XML_Char * name ) {
    bulkStopUD * bsud = (bulkStopUD *)vUserData;
    if( strcasecmp( ignoringNameSpace( name ), "unsuccessfulFleetRequestSet" ) == 0 ) {
        bsud->inFailure = false;
    }
}

bool AmazonBulkStop::SendRequest() {
	bool result = AmazonRequest::SendRequest();
	if( result ) {
        bulkStopUD bsud( this->success );
        XML_Parser xp = XML_ParserCreate( NULL );
        XML_SetElementHandler( xp, & bulkStopESH, & bulkStopEEH );
        XML_SetCharacterDataHandler( xp, & bulkStopCDH );
        XML_SetUserData( xp, & bsud );
        XML_Parse( xp, this->resultString.c_str(), this->resultString.length(), 1 );
        XML_ParserFree( xp );
	} else {
		if( this->errorCode == "E_CURL_IO" ) {
			// To be on the safe side, if the I/O failed, the annexd should
			// check to see the Spot Fleet was stopped or not.
			this->errorCode = "NEED_CHECK_BULK_STOP";
			return false;
		}
	}
	return result;
}

bool AmazonBulkStop::workerFunction( char ** argv, int argc, std::string & result_string ) {
	assert( strcasecmp( argv[0], "EC2_BULK_STOP" ) == 0 );
	int requestID;
	get_int( argv[1], & requestID );
	dprintf( D_PERF_TRACE, "request #%d (%s): work begins\n",
		requestID, argv[0] );

	if( ! verify_min_number_args( argc, 6 ) ) {
		result_string = create_failure_result( requestID, "Wrong_Argument_Number" );
		dprintf( D_ALWAYS, "Wrong number of arguments (%d should be >= %d) to %s\n",
			argc, 6, argv[0] );
		return false;
	}

	// Fill in required attributes.
	AmazonBulkStop request = AmazonBulkStop( requestID, argv[0] );
	request.serviceURL = argv[2];
	request.accessKeyFile = argv[3];
	request.secretKeyFile = argv[4];

	// Fill in required parameters.
	request.query_parameters[ "Action" ] = "CancelSpotFleetRequests";
	// See comment in AmazonBulkStart::workerFunction().
	request.query_parameters[ "Version" ] = "2016-04-01";
	request.query_parameters[ "SpotTerminateInstances" ] = "true";

	request.query_parameters[ "SpotFleetRequestId.1" ] = argv[5];

	if( ! request.SendRequest() ) {
		result_string = create_failure_result( requestID,
			request.errorMessage.c_str(),
			request.errorCode.c_str() );
	} else {
		if( request.success ) {
			result_string = create_success_result( requestID, NULL );
		} else {
			// FIXME: Extract the error code and return success if it's
			// 'fleetRequestIdDoesNotExist'.  It should never be, but if
			// the user put it in 'fleetRequestNotInCancellableState',
			// then we should wait a bit and retry (in the annex daemon,
			// so maybe we should just return the error code in that field
			// and let the daemon look at it, and maybe the message in
			// the message field).  The 'unexpectedError' should also be
			// retried, but 'fleetRequestIdMalformed' should probably
			// cause an abort in the annex daemon.
			std::string message;
			formatstr( message, "Unexpected failure encountered while "
				"cancelling spot fleet request '%s'.  Check the Annex GAHP "
				"log for details.",
				request.query_parameters[ "SpotFleetRequestId.1" ].c_str() );
			result_string = create_failure_result( requestID, message.c_str(), "E_UNEXPECTED_ERROR" );
		}
	}

	return true;
}

// ---------------------------------------------------------------------------

AmazonGetFunction::~AmazonGetFunction() { }

bool AmazonGetFunction::SendURIRequest() {
	bool result = AmazonRequest::SendURIRequest();
	if( result ) {
		ClassAd reply;
		classad::ClassAdJsonParser cajp;
		if(! cajp.ParseClassAd( this->resultString, reply, true )) {
			dprintf( D_ALWAYS, "Failed to parse reply '%s' as JSON.\n", this->resultString.c_str() );
			return false;
		}

		classad::ExprTree * c = reply.LookupExpr( "Configuration" );
		classad::ClassAd * d = NULL;
		if((! c) || (! c->isClassad( & d ))) {
			dprintf( D_ALWAYS, "Failed to find JSON blob value 'Configuration' in reply.\n" );
			return false;
		}

		ClassAd configuration( * d );
		if(! configuration.LookupString( "CodeSha256", functionHash )) {
			dprintf( D_ALWAYS, "Failed to find string value 'Configuration.CodeSha256' in reply.\n" );
			return false;
		}

		return true;
	} else {
		if( responseCode == 404 &&
		  errorMessage.find( "Function not found" ) != std::string::npos ) {
		  	// This signals that our query successfully found nothing
		  	// (so that we don't have to try again).
		  	functionHash = "";
			return true;
		}
	}
	return result;
}

bool AmazonGetFunction::workerFunction( char ** argv, int argc, std::string & result_string ) {
	assert( strcasecmp( argv[0], AMAZON_COMMAND_GET_FUNCTION ) == 0 );
	int requestID;
	get_int( argv[1], & requestID );
	dprintf( D_PERF_TRACE, "request #%d (%s): work begins\n",
		requestID, argv[0] );

	if( ! verify_min_number_args( argc, 6 ) ) {
		result_string = create_failure_result( requestID, "Wrong_Argument_Number" );
		dprintf( D_ALWAYS, "Wrong number of arguments (%d should be >= %d) to %s\n",
			argc, 6, argv[0] );
		return false;
	}

	// Fill in required attributes.
	AmazonGetFunction request = AmazonGetFunction( requestID, argv[0] );
	request.accessKeyFile = argv[3];
	request.secretKeyFile = argv[4];

	const char * separator = "/";
	if( argv[2][strlen( argv[2] ) - 1] == '/' ) {
		separator = "";
	}
	formatstr( request.serviceURL, "%s%s2015-03-31/functions/%s", argv[2],
		separator, amazonURLEncode( argv[5] ).c_str() );

	if( ! request.SendURIRequest() ) {
		result_string = create_failure_result( requestID,
			request.errorMessage.c_str(),
			request.errorCode.c_str() );
	} else {
		std::vector<std::string> sl;
		sl.emplace_back( request.functionHash );
		result_string = create_success_result( requestID, & sl );
	}

	return true;
}

// ---------------------------------------------------------------------------

AmazonS3Upload::~AmazonS3Upload() { }

bool AmazonS3Upload::SendRequest() {
	httpVerb = "PUT";
	std::string payload;
	if( ! htcondor::readShortFile( this->path, payload ) ) {
		this->errorCode = "E_FILE_IO";
		this->errorMessage = "Unable to read file to upload '" + this->path + "'.";
		dprintf( D_ALWAYS, "Unable to read from file to upload '%s', failing.\n", this->path.c_str() );
		return false;
	}
	return SendS3Request( payload );
}

// Expecting:	S3_UPLOAD <req_id>
//				<serviceurl> <accesskeyfile> <secretkeyfile>
//				<bucketName> <fileName> <path>
bool AmazonS3Upload::workerFunction(char **argv, int argc, std::string &result_string) {
	assert( strcasecmp( argv[0], "S3_UPLOAD" ) == 0 );

	int requestID;
	get_int( argv[1], & requestID );
	dprintf( D_PERF_TRACE, "request #%d (%s): work begins\n",
		requestID, argv[0] );

	if( ! verify_number_args( argc, 8 ) ) {
		result_string = create_failure_result( requestID, "Wrong_Argument_Number" );
		dprintf( D_ALWAYS, "Wrong number of arguments (%d should be >= %d) to %s\n",
			argc, 8, argv[0] );
		return false;
	}

	// Fill in required attributes & parameters.
	AmazonS3Upload uploadRequest = AmazonS3Upload( requestID, argv[0] );

	std::string serviceURL = argv[2];
	std::string bucketName = argv[5];
	std::string fileName = argv[6];

	std::string protocol, host, canonicalURI;
	if(! parseURL( serviceURL, protocol, host, canonicalURI )) {
		uploadRequest.errorCode = "E_INVALID_SERVICE_URL";
		uploadRequest.errorMessage = "Failed to parse service URL.";
		dprintf( D_ALWAYS, "Failed to match regex against service URL '%s'.\n", serviceURL.c_str() );

		result_string = create_failure_result( requestID,
			uploadRequest.errorMessage.c_str(),
			uploadRequest.errorCode.c_str() );
		return false;
	}
	if( canonicalURI.empty() ) { canonicalURI = "/"; }

	uploadRequest.serviceURL =	protocol + "://" + bucketName + "." +
								host + canonicalURI + fileName;
	uploadRequest.accessKeyFile = argv[3];
	uploadRequest.secretKeyFile = argv[4];
	uploadRequest.path = argv[7];

	// If we can, set the region based on the host.
	size_t secondDot = host.find( ".", 2 + 1 );
	if( host.find( "s3." ) == 0 ) {
		uploadRequest.region = host.substr( 3, secondDot - 2 - 1 );
	}

	// Send the request.
	if( ! uploadRequest.SendRequest() ) {
		result_string = create_failure_result( requestID,
		uploadRequest.errorMessage.c_str(),
		uploadRequest.errorCode.c_str() );
	} else {
		result_string = create_success_result( requestID, NULL );
	}

	return true;
}

// ---------------------------------------------------------------------------

AmazonCreateStack::~AmazonCreateStack() { }

bool AmazonCreateStack::SendRequest() {
	bool result = AmazonRequest::SendRequest();
	if( result ) {
		Regex r; int errCode = 0; int errOffset;
		bool patternOK = r.compile( "<StackId>(.*)</StackId>", &errCode, &errOffset);
		ASSERT( patternOK );
		std::vector<std::string> groups;
		if( r.match( resultString, & groups ) ) {
			this->stackID = groups[1];
		}
	}
	return result;
}

// Expecting:	CF_CREATE_STACK <req_id>
//				<serviceurl> <accesskeyfile> <secretkeyfile>
//				<stackName> <templateURL> <capability>
//				(<parameters-name> <parameter-value>)* <NULLSTRING>

bool AmazonCreateStack::workerFunction(char **argv, int argc, std::string &result_string) {
	assert( strcasecmp( argv[0], "CF_CREATE_STACK" ) == 0 );

	int requestID;
	get_int( argv[1], & requestID );
	dprintf( D_PERF_TRACE, "request #%d (%s): work begins\n",
		requestID, argv[0] );

	if( ! verify_min_number_args( argc, 9 ) ) {
		result_string = create_failure_result( requestID, "Wrong_Argument_Number" );
		dprintf( D_ALWAYS, "Wrong number of arguments (%d should be >= %d) to %s\n",
			argc, 9, argv[0] );
		return false;
	}

	// Fill in required attributes & parameters.
	AmazonCreateStack csRequest = AmazonCreateStack( requestID, argv[0] );
	csRequest.serviceURL = argv[2];
	csRequest.accessKeyFile = argv[3];
	csRequest.secretKeyFile = argv[4];

	csRequest.query_parameters[ "Action" ] = "CreateStack";
	csRequest.query_parameters[ "Version" ] = "2010-05-15";
	csRequest.query_parameters[ "OnFailure" ] = "DELETE";
	csRequest.query_parameters[ "StackName" ] = argv[5];
	csRequest.query_parameters[ "TemplateURL" ] = argv[6];
	if( strcasecmp( argv[7], NULLSTRING ) ) {
		csRequest.query_parameters[ "Capabilities.member.1" ] = argv[7];
	}

	std::string pn;
	for( int i = 8; i + 1 < argc && strcmp( argv[i], NULLSTRING ); i += 2 ) {
		formatstr( pn, "Parameters.member.%d", (i - 6)/2 );
		csRequest.query_parameters[ pn + ".ParameterKey" ] = argv[i];
		csRequest.query_parameters[ pn + ".ParameterValue" ] = argv[i+1];
	}

	if(! csRequest.SendRequest()) {
		result_string = create_failure_result( requestID,
				csRequest.errorMessage.c_str(),
				csRequest.errorCode.c_str() );
	} else {
		if( csRequest.stackID.empty() ) {
			result_string = create_failure_result( requestID,
				"Could not find stack ID in response from server.",
				"E_NO_STACK_ID" );
		} else {
			std::vector<std::string> sl;
			sl.emplace_back( csRequest.stackID );
			result_string = create_success_result( requestID, & sl );
		}
	}

	return true;
}

// ---------------------------------------------------------------------------

AmazonDescribeStacks::~AmazonDescribeStacks() { }

bool AmazonDescribeStacks::SendRequest() {
	bool result = AmazonRequest::SendRequest();
	if( result ) {
		int errCode = 0; int errOffset;

		Regex r;
		bool patternOK = r.compile( "<StackStatus>(.*)</StackStatus>", &errCode, &errOffset);
		ASSERT( patternOK );
		std::vector<std::string> statusGroups;
		if( r.match( resultString, & statusGroups ) ) {
			this->stackStatus = statusGroups[1];
		}

		Regex s;
		patternOK = s.compile( "<Outputs>(.*)</Outputs>", &errCode, &errOffset, Regex::multiline | Regex::dotall );
		ASSERT( patternOK );
		std::vector<std::string> outputGroups;
		if( s.match( resultString, & outputGroups ) ) {
			dprintf( D_ALWAYS, "Found output string '%s'.\n", outputGroups[1].c_str() );
			std::string membersRemaining = outputGroups[1];

			Regex t;
			patternOK = t.compile( "\\s*<member>\\s*(.*?)\\s*</member>\\s*", &errCode, &errOffset, Regex::multiline | Regex::dotall );
			ASSERT( patternOK );

			std::vector<std::string> memberGroups;
			while( t.match( membersRemaining, & memberGroups ) ) {
				std::string member = memberGroups[1];
				dprintf( D_ALWAYS, "Found member '%s'.\n", member.c_str() );

				Regex u;
				patternOK = u.compile( "<OutputKey>(.*)</OutputKey>", &errCode, &errOffset);
				ASSERT( patternOK );

				std::string outputKey;
				std::vector<std::string> keyGroups;
				if( u.match( member, & keyGroups ) ) {
					outputKey = keyGroups[1];
				}

				Regex v;
				patternOK = v.compile( "<OutputValue>(.*)</OutputValue>", &errCode, &errOffset);
				ASSERT( patternOK );

				std::string outputValue;
				std::vector<std::string> valueGroups;
				if( v.match( member, & valueGroups ) ) {
					outputValue = valueGroups[1];
				}

				if(! outputKey.empty()) {
					outputs.push_back( outputKey );
					outputs.push_back( outputValue );
				}

				replace_str( membersRemaining, memberGroups[0], "" );
			}
		}
	}
	return result;
}

// Expecting:	CF_DESCRIBE_STACKS <req_id>
//				<serviceurl> <accesskeyfile> <secretkeyfile>
//				<stackName>
bool AmazonDescribeStacks::workerFunction(char **argv, int argc, std::string &result_string) {
	assert( strcasecmp( argv[0], "CF_DESCRIBE_STACKS" ) == 0 );

	int requestID;
	get_int( argv[1], & requestID );
	dprintf( D_PERF_TRACE, "request #%d (%s): work begins\n",
		requestID, argv[0] );

	if( ! verify_number_args( argc, 6 ) ) {
		result_string = create_failure_result( requestID, "Wrong_Argument_Number" );
		dprintf( D_ALWAYS, "Wrong number of arguments (%d should be >= %d) to %s\n",
			argc, 6, argv[0] );
		return false;
	}

	// Fill in required attributes & parameters.
	AmazonDescribeStacks dsRequest = AmazonDescribeStacks( requestID, argv[0] );
	dsRequest.serviceURL = argv[2];
	dsRequest.accessKeyFile = argv[3];
	dsRequest.secretKeyFile = argv[4];

	dsRequest.query_parameters[ "Action" ] = "DescribeStacks";
	dsRequest.query_parameters[ "Version" ] = "2010-05-15";
	dsRequest.query_parameters[ "StackName" ] = argv[5];

	if(! dsRequest.SendRequest()) {
		result_string = create_failure_result( requestID,
				dsRequest.errorMessage.c_str(),
				dsRequest.errorCode.c_str() );
	} else {
		if( dsRequest.stackStatus.empty() ) {
		result_string = create_failure_result( requestID,
			"Could not find stack status in response from server.",
			"E_NO_STACK_STATUS" );
		} else {
			std::vector<std::string> sl;
			sl.emplace_back( dsRequest.stackStatus );
			for( unsigned i = 0; i < dsRequest.outputs.size(); ++i ) {
				sl.emplace_back( nullStringIfEmpty( dsRequest.outputs[i] ) );
			}
			result_string = create_success_result( requestID, & sl );
		}
	}

	return true;
}

// ---------------------------------------------------------------------------

AmazonCallFunction::~AmazonCallFunction() { }

bool AmazonCallFunction::SendJSONRequest( const std::string & payload ) {
	bool result = AmazonRequest::SendJSONRequest( payload );
	return result;
}

// Expecting:   AWS_CALL_FUNCTION <req_id>
//				<service_url> <accesskeyfile> <secretkeyfile>
//				<function-name-or-arn> <function-argument-blob>
bool AmazonCallFunction::workerFunction( char ** argv, int argc, std::string & result_string ) {
	assert( strcasecmp( argv[0], AMAZON_COMMAND_CALL_FUNCTION ) == 0 );
	int requestID;
	get_int( argv[1], & requestID );
	dprintf( D_PERF_TRACE, "request #%d (%s): work begins\n",
		requestID, argv[0] );

	if( ! verify_min_number_args( argc, 7 ) ) {
		result_string = create_failure_result( requestID, "Wrong_Argument_Number" );
		dprintf( D_ALWAYS, "Wrong number of arguments (%d should be >= %d) to %s\n",
			argc, 7, argv[0] );
		return false;
	}

	// Fill in required attributes.
	AmazonCallFunction request = AmazonCallFunction( requestID, argv[0] );
	request.accessKeyFile = argv[3];
	request.secretKeyFile = argv[4];

	const char * separator = "/";
	if( argv[2][strlen( argv[2] ) - 1] == '/' ) {
		separator = "";
	}
	formatstr( request.serviceURL, "%s%s2015-03-31/functions/%s/invocations", argv[2],
		separator, amazonURLEncode( argv[5] ).c_str() );
	request.headers[ "X-Amz-Invocation-Type" ] = "RequestResponse";

	if( ! request.SendJSONRequest( argv[6] ) ) {
		result_string = create_failure_result( requestID,
			request.errorMessage.c_str(),
			request.errorCode.c_str() );
	} else {
		std::vector<std::string> sl;
		sl.emplace_back( request.resultString );
		result_string = create_success_result( requestID, & sl );
	}

	return true;
}

// ---------------------------------------------------------------------------

AmazonBulkQuery::~AmazonBulkQuery() { }

struct bulkQueryUD_t {
	int	itemDepth;
	bool inCreateTime;
	bool inClientToken;
	bool inSpotFleetRequestID;

	std::string currentSFRID;
	std::string currentCreateTime;
	std::string currentClientToken;

	std::vector<std::string> & resultList;

	bulkQueryUD_t( std::vector<std::string> & r ) : itemDepth( 0 ), inCreateTime( false ),
		inClientToken( false ), inSpotFleetRequestID( false ), resultList( r ) { }
};
typedef struct bulkQueryUD_t bulkQueryUD;

void bulkQueryESH( void * vUserData, const XML_Char * name, const XML_Char ** ) {
	bulkQueryUD * bqUD = (bulkQueryUD *)vUserData;
	if( strcasecmp( ignoringNameSpace( name ), "item" ) == 0 ) {
		bqUD->itemDepth += 1;
	} else if( strcasecmp( ignoringNameSpace( name ), "spotFleetRequestId" ) == 0 ) {
		bqUD->inSpotFleetRequestID = true;
	} else if( strcasecmp( ignoringNameSpace( name ), "clientToken" ) == 0 ) {
		bqUD->inClientToken = true;
	} else if( strcasecmp( ignoringNameSpace( name ), "createTime" ) == 0 ) {
		bqUD->inCreateTime = true;
	}
}

void bulkQueryCDH( void * vUserData, const XML_Char * cdata, int len ) {
	bulkQueryUD * bqUD = (bulkQueryUD *)vUserData;
	if( bqUD->inSpotFleetRequestID ) {
		appendToString( (const void *)cdata, len, 1, (void *) & bqUD->currentSFRID );
	} else if( bqUD->inClientToken ) {
		appendToString( (const void *)cdata, len, 1, (void *) & bqUD->currentClientToken );
	} else if( bqUD->inCreateTime ) {
		appendToString( (const void *)cdata, len, 1, (void *) & bqUD->currentCreateTime );
	}
}

void bulkQueryEEH( void * vUserData, const XML_Char * name ) {
	bulkQueryUD * bqUD = (bulkQueryUD *)vUserData;
	if( strcasecmp( ignoringNameSpace( name ), "item" ) == 0 ) {
		bqUD->itemDepth -= 1;

		if( bqUD->itemDepth == 0 ) {
			if(! bqUD->currentSFRID.empty()) {
				bqUD->resultList.emplace_back( bqUD->currentSFRID );
				bqUD->resultList.emplace_back( nullStringIfEmpty( bqUD->currentCreateTime ) );
				bqUD->resultList.emplace_back( nullStringIfEmpty( bqUD->currentClientToken ) );
			}

			bqUD->currentSFRID.clear();
			bqUD->currentCreateTime.clear();
			bqUD->currentClientToken.clear();
		}
	} else if( strcasecmp( ignoringNameSpace( name ), "spotFleetRequestId" ) == 0 ) {
		bqUD->inSpotFleetRequestID = false;
	} else if( strcasecmp( ignoringNameSpace( name ), "clientToken" ) == 0 ) {
		bqUD->inClientToken = false;
	} else if( strcasecmp( ignoringNameSpace( name ), "createTime" ) == 0 ) {
		bqUD->inCreateTime = false;
	}
}

bool AmazonBulkQuery::SendRequest() {
	bool result = AmazonRequest::SendRequest();
	if( result ) {
		bulkQueryUD bqUD( resultList );
		XML_Parser xp = XML_ParserCreate( NULL );
		XML_SetElementHandler( xp, & bulkQueryESH, & bulkQueryEEH );
		XML_SetCharacterDataHandler( xp, & bulkQueryCDH );
		XML_SetUserData( xp, & bqUD );
		XML_Parse( xp, this->resultString.c_str(), this->resultString.length(), 1 );
		XML_ParserFree( xp );
	}
	return result;
}

bool AmazonBulkQuery::workerFunction( char ** argv, int argc, std::string & result_string ) {
	assert( strcasecmp( argv[0], "EC2_BULK_QUERY" ) == 0 );
	int requestID;
	get_int( argv[1], & requestID );
	dprintf( D_PERF_TRACE, "request #%d (%s): work begins\n",
		requestID, argv[0] );

	if( ! verify_min_number_args( argc, 5 ) ) {
		result_string = create_failure_result( requestID, "Wrong_Argument_Number" );
		dprintf( D_ALWAYS, "Wrong number of arguments (%d should be >= %d) to %s\n",
			argc, 6, argv[0] );
		return false;
	}

	// Fill in required attributes.
	AmazonBulkQuery request = AmazonBulkQuery( requestID, argv[0] );
	request.serviceURL = argv[2];
	request.accessKeyFile = argv[3];
	request.secretKeyFile = argv[4];

	// Fill in required parameters.
	request.query_parameters[ "Action" ] = "DescribeSpotFleetRequests";
	// See comment in AmazonBulkStart::workerFunction().
	request.query_parameters[ "Version" ] = "2016-04-01";

	if( ! request.SendRequest() ) {
		result_string = create_failure_result( requestID,
			request.errorMessage.c_str(),
			request.errorCode.c_str() );
	} else {
		result_string = create_success_result( requestID, & request.resultList );
	}

	return true;
}
