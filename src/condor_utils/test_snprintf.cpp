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


#include "condor_common.h"
#include "condor_snutils.h"

int main(int argc, char **argv)
{
	// A buffer to store the output of snprintf
	char output[11];

	// A string to test our limits
	char input[] = "1234567890abcdefghijklmnopqrstuvwxyz";

	int num_chars;
	int reported_length;

	output[0] = 0;
	for (num_chars = 0; num_chars < 10; num_chars++) {
		// If we want 0 characters, we still need space for the
		// null termination, so we add one in the next line.
		reported_length = snprintf(output, num_chars+1, "%s", input);
		if (strlen(output) == num_chars) {
			printf("PASSED: Our snprintf output %d characters, as expected\n",
				   num_chars);
		} else {
			printf("FAILED: Our snprintf output %d characters instead of "
				   "%d characters!\n", strlen(output), num_chars);
		}

		if (reported_length == strlen(input)) {
			printf("PASSED: Our snprintf reported the desired length (%d) "
				   "correctly.\n", reported_length);
		} else {
			printf("FAILED: Our snprintf reported %d instead of %d.\n",
				   reported_length, strlen(input));
		}
	}
			
	output[0] = 0;
	reported_length = snprintf(output, 0, "%s", input);
	if (strlen(output) == 0) {
		printf("PASSED: The degenerative case worked.\n");
	} else {
		printf("FAILED: The degenerative case made a string with %d chars.\n",
			   strlen(output));
	}
		
	if (reported_length == strlen(input)) {
		printf("PASSED: The degenerative case reported the desired length "
			   "(%d) correctly.\n", reported_length);
	} else {
		printf("FAILED: The degenerative case reported %d instead of %d.\n",
			   reported_length, strlen(input));
	}
		
	return 0;
}
