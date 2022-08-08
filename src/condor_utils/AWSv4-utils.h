#ifndef AWSV4_UTILS_H
#define AWSV4_UTILS_H

/*
 * This header requires the following headers.
 *
#include <string>

#include "classad.h"
#include "CondorError.h"
 *
 */

namespace htcondor {
//
// The jobAd must have ATTR_EC2_ACCESS_KEY_ID ATTR_EC2_SECRET_ACCESS_KEY set.
// Optionally, ATTR_AWS_REGION may be provided.
//
bool
generate_presigned_url( const classad::ClassAd & jobAd,
	const std::string & s3url,
	const std::string & verb,
	std::string & presignedURL,
	CondorError & err );
}

#endif /* AWSV4_UTILS_H */
