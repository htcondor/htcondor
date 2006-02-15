/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
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
