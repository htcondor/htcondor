#ifndef AWSV4_IMPL_H
#define AWSV4_IMPL_H

/*
 * This header requires the following headers.
 *
#include <string>
#include <map>
 *
 */

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
createSignature( const std::string & secretAccessKey,
    const std::string & date, const std::string & region,
    const std::string & service, const std::string & stringToSign,
    std::string & signature );

}

#endif /* AWSV4_UTILS_H */
