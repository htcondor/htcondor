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
