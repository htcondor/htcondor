#ifndef __CASE_SENSITIVITY_H__
#define __CASE_SENSITIVITY_H__

#include "condor_common.h"
#include <string.h>
#include "condor_string.h"

// string compare to be used for attribute names
#define CLASSAD_ATTR_NAMES_STRCMP 	strcasecmp

// string compare to be used for checking scope names
#define CLASSAD_SCOPE_NAMES_STRCMP 	strcasecmp

// string compare to be used for checking reserved keywords like
// "true", "false", "undefined" and "error"
#define CLASSAD_RESERVED_STRCMP		strcasecmp

// string compare for function names
#define CLASSAD_FN_CALL_STRCMP		strcasecmp

// string compare for string values
#define CLASSAD_STR_VALUES_STRCMP	strcmp

#endif//__CASE_SENSITIVITY_H__
