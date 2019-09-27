#include "condor_common.h"
#include "condor_debug.h"
#include "condor_attributes.h"

#include <string>
#include <map>
#include <openssl/hmac.h>
#include "classad/classad.h"
#include "AWSv4-utils.h"

#include "stl_string_utils.h"
#include "stat_wrapper.h"

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

//
// This will work for all buckets created in any region "launched" before
// March 29, 2019, because s3.amazonaws.com will reply with a code 307
// redirect to the correct region.  This will fail for all other buckets.
//
// It would also be nice to support direct specification of bucket regions
// for speed reasons.
//
// It is wildly unclear how to do this in the s3:// URL schema, which mostly
// seems to exist in the 'aws' CLI rather than in the wild.  Without having
// thought about it too hard, though, it seems like cramming the region into
// the s3:// URL is probably better than anything else.
//
// For Amazon proper, we could allow either endpoints or regions; the mapping
// between the two is consistent.  It's not clear what we'd want for other
// S3 implementations -- or, for that matter, if this code will work for them
// at all.
//
// Maybe s3://bucket@region/key ?  (I don't know if '@' is legal in bucket
// names.  It's certainly a bad idea.)  For disambiguation, I guess we
// could do regionalS3://endpoint/bucket/key, instead.
//

bool
generate_presigned_url( const std::string & accessKeyID,
  const std::string & secretAccessKey,
  const std::string & s3url,
  std::string & presignedURL ) {

    time_t now; time( & now );
    struct tm brokenDownTime; gmtime_r( & now, & brokenDownTime );
    char dateAndTime[] = "YYYYMMDDThhmmssZ";
    strftime( dateAndTime, sizeof(dateAndTime), "%Y%m%dT%H%M%SZ",
        & brokenDownTime );
    char date[] = "YYYYMMDD";
    strftime( date, sizeof(date), "%Y%m%d", & brokenDownTime );

    // Extract the S3 bucket and key from the S3 URL.
    std::string bucket, key;
    if(! starts_with( s3url, "s3://" )) {
        dprintf( D_ALWAYS, "An S3 URL must begin with s3://.\n" );
        return false;
    }
    size_t protocolLength = 5; // std::string("s3://").size()
    size_t middle = s3url.find( "/", protocolLength );
    if( middle == std::string::npos ) {
        dprintf( D_ALWAYS, "An S3 URL be of the form s3://<bucket>/<key>.\n" );
        return false;
    }
    bucket = s3url.substr( protocolLength, middle - protocolLength );
    key = s3url.substr( middle + 1 );

    //
    // Construct the canonical request.
    //

    // Part 1: The canonical URI.  Note that we don't have to worry about
    // path normalization, because S3 keys aren't actually path names.
    std::string canonicalURI;
    formatstr( canonicalURI, "/%s", AWSv4Impl::pathEncode(key).c_str() );

    // Part 4: The signed headers.
    std::string signedHeaders = "host";

    //
    // Part 2: The canonical query string.
    //
    std::string canonicalQueryString;

    std::string credentialScope;
    std::string region = "us-east-1";
    std::string service = "s3";
    formatstr( credentialScope, "%s/%s/%s/aws4_request",
        date, region.c_str(), service.c_str() );

    std::map< std::string, std::string > queries;
    queries["X-Amz-Algorithm"] = "AWS4-HMAC-SHA256";
    queries["X-Amz-Credential"] = accessKeyID + "/" + credentialScope;
    queries["X-Amz-Date"] = dateAndTime;
    queries["X-Amz-Expires"] = "3600";
    queries["X-Amz-SignedHeaders"] = signedHeaders;

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
    std::string host = bucket + ".s3.amazonaws.com";
    formatstr( canonicalHeaders, "host:%s\n", host.c_str() );

    // FIXME: This will probably need to be PUT for S3 uploads.
    std::string httpVerb = "GET";
    std::string canonicalRequest = httpVerb + "\n"
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
        dprintf( D_ALWAYS, "Unable to hash canonical request, failing.\n" );
        return false;
    }
    AWSv4Impl::convertMessageDigestToLowercaseHex( messageDigest, mdLength, canonicalRequestHash );

    std::string stringToSign;
    formatstr( stringToSign, "AWS4-HMAC-SHA256\n%s\n%s\n%s",
        dateAndTime, credentialScope.c_str(), canonicalRequestHash.c_str() );

	std::string signature;
	if(! AWSv4Impl::createSignature( secretAccessKey, date, region, service, stringToSign, signature )) {
		dprintf( D_ALWAYS, "Failed to create signature, failing.\n" );
		return false;
	}

    formatstr( presignedURL,
        "https://%s/%s?%s&X-Amz-Signature=%s",
        host.c_str(), key.c_str(), canonicalQueryString.c_str(),
        signature.c_str() );

    return true;
}

bool
generate_presigned_url( const classad::ClassAd & jobAd,
  const std::string & s3url,
  std::string & presignedURL ) {
	std::string accessKeyIDFile;
	jobAd.EvaluateAttrString( ATTR_EC2_ACCESS_KEY_ID, accessKeyIDFile );
	if( accessKeyIDFile.empty() ) {
		dprintf( D_ALWAYS, "Public key file not defined.\n" );
		return false;
	}

	std::string accessKeyID;
	if(! AWSv4Impl::readShortFile( accessKeyIDFile, accessKeyID )) {
		dprintf( D_ALWAYS, "Unable to read from public key file.\n" );
		return false;
	}
	trim( accessKeyID );

	std::string secretAccessKeyFile;
	jobAd.EvaluateAttrString( ATTR_EC2_SECRET_ACCESS_KEY, secretAccessKeyFile );
	if( secretAccessKeyFile.empty() ) {
		dprintf( D_ALWAYS, "Private key file not defined.\n" );
		return false;
	}

	std::string secretAccessKey;
	if(! AWSv4Impl::readShortFile( secretAccessKeyFile, secretAccessKey )) {
		dprintf( D_ALWAYS, "Unable to read from secret key file.\n" );
		return false;
	}
	trim( secretAccessKey );

	return generate_presigned_url( accessKeyID, secretAccessKey,
		s3url, presignedURL );
}
