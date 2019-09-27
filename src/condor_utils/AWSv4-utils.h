#ifndef AWSV4_UTILS_H
#define AWSV4_UTILS_H

/*
#include <string>
#include <map>

#include <openssl/hmac.h>

#include "classad.h"
*/

//
// The jobAd must have ATTR_EC2_ACCESS_KEY_ID ATTR_EC2_SECRET_ACCESS_KEY set.
//
bool
generate_presigned_url( const classad::ClassAd & jobAd,
	const std::string & s3url,
	std::string & presignedURL );

// Functions used internally by generate_presigned_url() and the Amazon GAHP.

namespace AWSv4Impl {

std::string
pathEncode( const std::string & original );

std::string
amazonURLEncode( const std::string & input );

std::string
canonicalizeQueryString( const std::map< std::string, std::string > & qp );

void
convertMessageDigestToLowercaseHex( const unsigned char * messageDigest,
	unsigned int mdLength, std::string & hexEncoded );

bool
doSha256( const std::string & payload, unsigned char * messageDigest,
	unsigned int * mdLength );

bool
readShortFile( const std::string & fileName, std::string & contents );

bool
createSignature( const std::string & secretAccessKey,
    const std::string & date, const std::string & region,
    const std::string & service, const std::string & stringToSign,
    std::string & signature );

}

#endif /* AWSV4_UTILS_H */
