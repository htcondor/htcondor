#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "classad_shared.h"

extern "C" 
{

	void sharedstring(const int number_of_arguments,
					  const ClassAdSharedValue *arguments,
					  ClassAdSharedValue *result)
	{
		char *value = new char[128];

		strcpy(value, "Alain is a doofus");

		result->type = ClassAdSharedType_String;
		result->text = value;

		return;
	}

	void sharedfloat(const int number_of_arguments,
					 const ClassAdSharedValue *arguments,
					 ClassAdSharedValue *result)
	{
		if (number_of_arguments != 1
			|| arguments == NULL
			|| result == NULL
	        || (arguments[0].type != ClassAdSharedType_Integer
				&& arguments[0].type != ClassAdSharedType_Float)) {
			result->type = ClassAdSharedType_Error;
		} else {
			if (arguments[0].type == ClassAdSharedType_Integer) {
				result->real = (float) (arguments[0].integer * arguments[0].integer);
			} else {
				result->real = (float) (arguments[0].real    * arguments[0].real);
			}
			result->type = ClassAdSharedType_Float;

		}
		return;
	}

	void sharedinteger(const int number_of_arguments,
					   const ClassAdSharedValue *arguments,
					   ClassAdSharedValue *result)
	{
		if (number_of_arguments != 1
			|| arguments == NULL
			|| result == NULL
	        || (arguments[0].type != ClassAdSharedType_Integer
				&& arguments[0].type != ClassAdSharedType_Float)) {
			result->type = ClassAdSharedType_Error;
		} else {
			if (arguments[0].type == ClassAdSharedType_Integer) {
				result->integer = (int) (arguments[0].integer * arguments[0].integer);
			} else {
				result->integer = (int) (arguments[0].real    * arguments[0].real);
			}
			result->type = ClassAdSharedType_Integer;
		}
		return;
	}

}
