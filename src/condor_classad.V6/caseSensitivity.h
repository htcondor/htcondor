#ifndef __CASE_SENSITIVITY_H__
#define __CASE_SENSITIVITY_H__

// Attribute lookups are case insensitive
#define CLASSAD_ATTR_NAMES_CASE_INSENSITIVE
#define CLASSAD_ATTR_NAMES_STRCMP	strcasecmp

// Function names are case insensitive
#define CLASSAD_FN_CALL_CASE_INSENSITIVE

// string compare to be used for checking reserved keywords like
// "true", "false", "undefined" and "error"
#define CLASSAD_RESERVED_STRCMP		strcasecmp


// string compare for string values
#define CLASSAD_STR_VALUES_STRCMP	strcmp

#endif//__CASE_SENSITIVITY_H__
