

#ifndef __CONDOR_SCITOKENS_H_
#define __CONDOR_SCITOKENS_H_

#include <string>
#include <vector>

#include "CondorError.h"

bool
validate_scitoken(const std::string &scitoken_str, std::string &issuer, std::string &subject,
	long long &expiry, std::vector<std::string> &bounding_set, CondorError &err);

#endif // __CONDOR_SCITOKENS_H_
