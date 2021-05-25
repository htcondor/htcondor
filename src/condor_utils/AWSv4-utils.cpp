#include "condor_common.h"
#include "condor_debug.h"
#include "condor_attributes.h"

#include <string>
#include <map>
#include <algorithm>
#include <openssl/hmac.h>
#include "classad/classad.h"
#include "CondorError.h"
#include "AWSv4-utils.h"

#include "stl_string_utils.h"
#include "stat_wrapper.h"
#include "utc_time.h"

std::string
AWSv4Impl::pathEncode( const std::string & original ) {
	std::string segment;
	std::string encoded;
	const char * o = original.c_str();

	size_t next = 0;
	size_t offset = 0;
	size_t length = strlen( o );
	while( offset < length ) {
		next = strcspn( o + offset, "/" );
		if( next == 0 ) { encoded += "/"; offset += 1; continue; }

		segment = std::string( o + offset, next );
		encoded += amazonURLEncode( segment );

		offset += next;
	}
	return encoded;
}

//
// This function should not be called for anything in query_parameters,
// except for by AmazonQuery::SendRequest().
//
std::string
AWSv4Impl::amazonURLEncode( const std::string & input ) {
    /*
     * See http://docs.amazonwebservices.com/AWSEC2/2010-11-15/DeveloperGuide/using-query-api.html
     *
     *
     * Since the GAHP protocol is defined to be ASCII, we're going to ignore
     * UTF-8, and hope it goes away.
     *
     */
    std::string output;
    for( unsigned i = 0; i < input.length(); ++i ) {
        // "Do not URL encode ... A-Z, a-z, 0-9, hyphen ( - ),
        // underscore ( _ ), period ( . ), and tilde ( ~ ).  Percent
        // encode all other characters with %XY, where X and Y are hex
        // characters 0-9 and uppercase A-F.  Percent encode extended
        // UTF-8 characters in the form %XY%ZA..."
        if( ('A' <= input[i] && input[i] <= 'Z')
         || ('a' <= input[i] && input[i] <= 'z')
         || ('0' <= input[i] && input[i] <= '9')
         || input[i] == '-'
         || input[i] == '_'
         || input[i] == '.'
         || input[i] == '~' ) {
            char uglyHack[] = "X";
            uglyHack[0] = input[i];
            output.append( uglyHack );
        } else {
            char percentEncode[4];
            int written = snprintf( percentEncode, 4, "%%%.2hhX", input[i] );
            ASSERT( written == 3 );
            output.append( percentEncode );
        }
    }

    return output;
}

std::string
AWSv4Impl::canonicalizeQueryString(
    const std::map< std::string, std::string > & query_parameters ) {
    std::string canonicalQueryString;
    for( auto i = query_parameters.begin(); i != query_parameters.end(); ++i ) {
        // Step 1A: The map sorts the query parameters for us.  Strictly
        // speaking, we should encode into a different AttributeValueMap
        // and then compose the string out of that, in case amazonURLEncode()
        // changes the sort order, but we don't specify parameters like that.

        // Step 1B: Encode the parameter names and values.
        std::string name = amazonURLEncode( i->first );
        std::string value = amazonURLEncode( i->second );

        // Step 1C: Separate parameter names from values with '='.
        canonicalQueryString += name + '=' + value;

        // Step 1D: Separate name-value pairs with '&';
        canonicalQueryString += '&';
    }

    // We'll always have a superflous trailing ampersand.
    canonicalQueryString.erase( canonicalQueryString.end() - 1 );
    return canonicalQueryString;
}

void
AWSv4Impl::convertMessageDigestToLowercaseHex(
  const unsigned char * messageDigest,
  unsigned int mdLength, std::string & hexEncoded ) {
	char * buffer = (char *)malloc( (mdLength * 2) + 1 );
	ASSERT( buffer );
	char * ptr = buffer;
	for( unsigned int i = 0; i < mdLength; ++i, ptr += 2 ) {
		sprintf( ptr, "%02x", messageDigest[i] );
	}
	hexEncoded.assign( buffer, mdLength * 2 );
	free(buffer);
}

bool
AWSv4Impl::doSha256( const std::string & payload,
  unsigned char * messageDigest,
  unsigned int * mdLength ) {
	EVP_MD_CTX * mdctx = EVP_MD_CTX_create();
	if( mdctx == NULL ) { return false; }

	if(! EVP_DigestInit_ex( mdctx, EVP_sha256(), NULL )) {
		EVP_MD_CTX_destroy( mdctx );
		return false;
	}

	if(! EVP_DigestUpdate( mdctx, payload.c_str(), payload.length() )) {
		EVP_MD_CTX_destroy( mdctx );
		return false;
	}

	if(! EVP_DigestFinal_ex( mdctx, messageDigest, mdLength )) {
		EVP_MD_CTX_destroy( mdctx );
		return false;
	}

	EVP_MD_CTX_destroy( mdctx );
	return true;
}

//
// Utility function; inefficient.
//
bool
AWSv4Impl::readShortFile( const std::string & fileName, std::string & contents ) {
    int fd = safe_open_wrapper_follow( fileName.c_str(), O_RDONLY, 0600 );

    if( fd < 0 ) {
        dprintf( D_ALWAYS, "Failed to open file '%s' for reading: '%s' (%d).\n",
            fileName.c_str(), strerror( errno ), errno );
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
    contents.assign( rawBuffer, fileSize );
    free( rawBuffer );

    return true;
}

bool
AWSv4Impl::createSignature( const std::string & secretAccessKey,
  const std::string & date, const std::string & region,
  const std::string & service, const std::string & stringToSign,
  std::string & signature ) {
    unsigned int mdLength = 0;
    unsigned char messageDigest[EVP_MAX_MD_SIZE];

	std::string saKey = "AWS4" + secretAccessKey;
	const unsigned char * hmac = HMAC( EVP_sha256(), saKey.c_str(), saKey.length(),
		(const unsigned char *)date.c_str(), date.length(),
		messageDigest, & mdLength );
	if( hmac == NULL ) { return false; }

	unsigned int md2Length = 0;
	unsigned char messageDigest2[EVP_MAX_MD_SIZE];
	hmac = HMAC( EVP_sha256(), messageDigest, mdLength,
		(const unsigned char *)region.c_str(), region.length(), messageDigest2, & md2Length );
	if( hmac == NULL ) { return false; }

	hmac = HMAC( EVP_sha256(), messageDigest2, md2Length,
		(const unsigned char *)service.c_str(), service.length(), messageDigest, & mdLength );
	if( hmac == NULL ) { return false; }

	const char c[] = "aws4_request";
	hmac = HMAC( EVP_sha256(), messageDigest, mdLength,
		(const unsigned char *)c, sizeof(c) - 1, messageDigest2, & md2Length );
	if( hmac == NULL ) { return false; }

	hmac = HMAC( EVP_sha256(), messageDigest2, md2Length,
		(const unsigned char *)stringToSign.c_str(), stringToSign.length(),
		messageDigest, & mdLength );
	if( hmac == NULL ) { return false; }

	convertMessageDigestToLowercaseHex( messageDigest, mdLength, signature );
	return true;
}

bool
isPathStyleBucket( const std::string & bucket ) {
    if( bucket.find( "_" ) != std::string::npos ) { return true; }
    if( std::find_if( bucket.begin(), bucket.end(),
      [](const char c){return isupper(c); } ) != bucket.end() ) {
        return true;
    }
    return false;
}

bool
generate_presigned_url( const std::string & accessKeyID,
  const std::string & secretAccessKey,
  const std::string & securityToken,
  const std::string & s3url,
  const std::string & input_region,
  const std::string & verb,
  std::string & presignedURL,
  CondorError & err ) {

    time_t now; time( & now );
    // Allow for modest clock skews.
    now -= 5;
    struct tm brokenDownTime; gmtime_r( & now, & brokenDownTime );
    char dateAndTime[] = "YYYYMMDDThhmmssZ";
    strftime( dateAndTime, sizeof(dateAndTime), "%Y%m%dT%H%M%SZ",
        & brokenDownTime );
    char date[] = "YYYYMMDD";
    strftime( date, sizeof(date), "%Y%m%d", & brokenDownTime );

    //
    // Convert gs:// URLs to the equivalent s3:// URLs.
    //
    std::string theURL = s3url;
    if( starts_with_ignore_case( s3url, "gs://" ) ) {
        std::string bucket_and_key = s3url.substr(5 /* strlen( "gs://") */ );
        formatstr( theURL, "s3://storage.googleapis.com/%s", bucket_and_key.c_str() );
    }

    // If the named bucket isn't valid as part of a DNS name,
    // we assume it's an old "path-style"-only bucket.
    std::string canonicalURI( "/" );

    // Extract the S3 bucket and key from the S3 URL.
    std::string bucket, key;
    if(! starts_with_ignore_case( theURL, "s3://" )) {
        err.push( "AWS SigV4", 1, "an S3 URL must begin with s3://" );
        return false;
    }
    size_t protocolLength = 5; // std::string("s3://").size()
    size_t middle = theURL.find( "/", protocolLength );
    if( middle == std::string::npos ) {
        err.push( "AWS SigV4", 2, "an S3 URL must be of the form s3://<bucket>/<object>, "
            "s3://<bucket>.s3.<region>.amazonaws.com/<object>, or s3://.../... for non-AWS endpoints" );
        return false;
    }
    std::string region = input_region;
    auto bucket_or_hostname = theURL.substr( protocolLength, middle - protocolLength );

    // Strip out the port number for now in order to simplify logic below.
    auto port_idx = bucket_or_hostname.find(":");
    std::string port_number;
    if (port_idx != std::string::npos) {
        port_number = bucket_or_hostname.substr(port_idx + 1);
        bucket_or_hostname = bucket_or_hostname.substr(0, port_idx);
    }
    // URLs of the form s3://<bucket>/<key>
    std::string host = bucket_or_hostname;
    if (bucket_or_hostname.find(".") == std::string::npos) {
        bucket = bucket_or_hostname;
        if(! region.empty()) {
            host = bucket + ".s3." + region + ".amazonaws.com";
        } else {
            host = bucket + ".s3.amazonaws.com";

            // If the bucket name isn't valid for DNS, assume it's an
            // old "path-style" bucket.
            if( isPathStyleBucket( bucket ) ) {
                host = "s3.amazonaws.com";
                region = "us-east-1";
                formatstr_cat( canonicalURI, "%s/",
                  AWSv4Impl::pathEncode(bucket).c_str() );
            }
        }
    // URLs of the form s3://<bucket>.s3.<region>.amazonaws.com/<object>
    } else if (bucket_or_hostname.substr(bucket_or_hostname.size() - 14) == ".amazonaws.com") {
        auto bucket_and_region = bucket_or_hostname.substr(0, bucket_or_hostname.size() - 14);
        auto last_idx = bucket_and_region.rfind(".s3.");
        if (last_idx == std::string::npos) {
            err.push( "AWS SigV4", 3, "invalid format for domain-based buckets; must be of the"
                " form s3://<bucket>.s3.<region>.amazonaws.com/<object>" );
            return false;
        }
        bucket = bucket_and_region.substr(0, last_idx);
        region = bucket_and_region.substr(last_idx + 4);
    } else {
        // All other URLs were s3://<something-without-dots>/<something>
        // where the first part is now in 'host' and the second part
        // will be in 'key' at the end of this stanza.
    }
    if (!port_number.empty()) {
        host = host + ":" + port_number;
    }
    key = theURL.substr( middle + 1 );
    // Pick a default region.
    if (region.empty()) {
        region = "us-east-1";
    }

    //
    // Construct the canonical request.
    //

    // Part 1: The canonical URI.  Note that we don't have to worry about
    // path normalization, because S3 keys aren't actually path names.
    formatstr_cat( canonicalURI, "%s", AWSv4Impl::pathEncode(key).c_str() );

    // Part 4: The signed headers.
    std::string signedHeaders = "host";

    //
    // Part 2: The canonical query string.
    //
    std::string canonicalQueryString;

    std::string credentialScope;
    std::string service = "s3";
    formatstr( credentialScope, "%s/%s/%s/aws4_request",
        date, region.c_str(), service.c_str() );

    std::map< std::string, std::string > queries;
    queries["X-Amz-Algorithm"] = "AWS4-HMAC-SHA256";
    queries["X-Amz-Credential"] = accessKeyID + "/" + credentialScope;
    queries["X-Amz-Date"] = dateAndTime;
    queries["X-Amz-Expires"] = "3600";
    queries["X-Amz-SignedHeaders"] = signedHeaders;
    if(! securityToken.empty()) {
        queries["X-Amz-Security-Token"] = securityToken;
    }

    std::string buffer;
    for( const auto & i : queries ) {
        formatstr( buffer, "%s=%s&",
            AWSv4Impl::amazonURLEncode( i.first ).c_str(),
            AWSv4Impl::amazonURLEncode( i.second ).c_str() );
        canonicalQueryString += buffer;
    }
    canonicalQueryString.erase( canonicalQueryString.end() - 1 );

    // Part 3: The canonical headers.  This MUST include "Host".
    std::string canonicalHeaders;
    formatstr( canonicalHeaders, "host:%s\n", host.c_str() );

    std::string canonicalRequest = verb + "\n"
                                 + canonicalURI + "\n"
                                 + canonicalQueryString + "\n"
                                 + canonicalHeaders + "\n"
                                 + signedHeaders + "\n"
                                 + "UNSIGNED-PAYLOAD";

    //
    // Create the signature.
    //
    unsigned int mdLength = 0;
    unsigned char messageDigest[EVP_MAX_MD_SIZE];
    std::string canonicalRequestHash;
    if(! AWSv4Impl::doSha256( canonicalRequest, messageDigest, & mdLength )) {
        err.push( "AWS SigV4", 5, "unable to hash canonical request, failing" );
        return false;
    }
    AWSv4Impl::convertMessageDigestToLowercaseHex( messageDigest, mdLength, canonicalRequestHash );

    std::string stringToSign;
    formatstr( stringToSign, "AWS4-HMAC-SHA256\n%s\n%s\n%s",
        dateAndTime, credentialScope.c_str(), canonicalRequestHash.c_str() );

    std::string signature;
    if(! AWSv4Impl::createSignature( secretAccessKey, date, region, service, stringToSign, signature )) {
        err.push( "AWS SigV4", 6, "failed to create signature, failing" );
        return false;
    }

    formatstr( presignedURL,
        "https://%s%s?%s&X-Amz-Signature=%s",
        host.c_str(), canonicalURI.c_str(), canonicalQueryString.c_str(),
        signature.c_str() );

    return true;
}

bool
htcondor::generate_presigned_url( const classad::ClassAd & jobAd,
  const std::string & s3url,
  const std::string & verb,
  std::string & presignedURL,
  CondorError & err ) {
	std::string accessKeyIDFile;
	jobAd.EvaluateAttrString( ATTR_EC2_ACCESS_KEY_ID, accessKeyIDFile );
	if( accessKeyIDFile.empty() ) {
		err.push( "AWS SigV4", 7, "access key file not defined" );
		return false;
	}

	std::string accessKeyID;
	if(! AWSv4Impl::readShortFile( accessKeyIDFile, accessKeyID )) {
		err.push( "AWS SigV4", 8, "unable to read from access key file" );
		return false;
	}
	trim( accessKeyID );


	std::string secretAccessKeyFile;
	jobAd.EvaluateAttrString( ATTR_EC2_SECRET_ACCESS_KEY, secretAccessKeyFile );
	if( secretAccessKeyFile.empty() ) {
		err.push( "AWS SigV4", 9, "secret key file not defined" );
		return false;
	}

	std::string secretAccessKey;
	if(! AWSv4Impl::readShortFile( secretAccessKeyFile, secretAccessKey )) {
		err.push( "AWS SigV4", 10, "unable to read from secret key file" );
		return false;
	}
	trim( secretAccessKey );


	// AWS calls it the "session token" in the user documentation and the
	// "security token" in the API.  We translate between the two here.
	std::string securityToken;
	std::string securityTokenFile;
	jobAd.EvaluateAttrString( ATTR_EC2_SESSION_TOKEN, securityTokenFile );
	if(! securityTokenFile.empty()) {
		if(! AWSv4Impl::readShortFile( securityTokenFile, securityToken )) {
			err.push( "AWS SigV4", 11, "unable to read from security token file" );
			return false;
		}
		trim( securityToken );
	}

	std::string region;
	jobAd.EvaluateAttrString( ATTR_AWS_REGION, region );

	return generate_presigned_url( accessKeyID, secretAccessKey, securityToken,
		s3url, region, verb, presignedURL, err );
}
