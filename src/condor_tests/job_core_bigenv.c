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


/*---------------------------------------------------------------------------
 *
 * This program tests to see if the environment that was provided at
 * submit time (via GetEnv or the Environment setting) has survived
 * without being corrupted in any way. We expect a very specific
 * environment, and it is about 12K. The purpose of doing this testing
 * it to make sure that Condor doesn't choke on environments that are
 * larger than 10K, which it used to do. In the past, condor_submit, 
 * condor_schedd, and condor_shadow would all have problems. Particularly 
 * the schedd and shadows: they often use 10K buffers on the stack, and the
 * environment would just walk all over the stack. 
 *
 *---------------------------------------------------------------------------*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv, char **envp);
static int check_environment_variable(
	char *variable_name,
	char repeating_character);

int main(int argc, char **argv, char **envp)
{
	int  some_test_failed;

	some_test_failed = 0;

	if (check_environment_variable("V1", '1')) some_test_failed = 1;
	if (check_environment_variable("V2", '2')) some_test_failed = 1;
	if (check_environment_variable("V3", '3')) some_test_failed = 1;
	if (check_environment_variable("V4", '4')) some_test_failed = 1;
	if (check_environment_variable("V5", '5')) some_test_failed = 1;
	if (check_environment_variable("V6", '6')) some_test_failed = 1;
	if (check_environment_variable("V7", '7')) some_test_failed = 1;
	if (check_environment_variable("V8", '8')) some_test_failed = 1;
	if (check_environment_variable("V9", '9')) some_test_failed = 1;
	if (check_environment_variable("V10", 'A')) some_test_failed = 1;
	if (check_environment_variable("V11", 'B')) some_test_failed = 1;
	if (some_test_failed) {
		printf("FAILURE\n");
	} else {
		printf("SUCCESS\n");
	}
	return some_test_failed;
}

static int check_environment_variable(
	char *variable_name,
	char repeating_character)
{
	char *value;
	int failed;

	failed = 0;
	value = getenv(variable_name);
	if (value == NULL) {
		printf("Failed: %s doesn't exist.\n", variable_name);
		failed = 1;
	} else if (strlen(value) != 1000) {
		printf("Failed: %s is corrupt (bad length).\n", variable_name);
		failed = 1;
	} else {
		while (*value != 0) {
			if (*value != repeating_character) {
				printf("Failed: %s is corrupt.\n", variable_name);
				failed = 1;
				break;
			}
			else {
				value++;
			}
		}
	}
	if (!failed) {
		printf("Passed: %s is okay.\n", variable_name);
	}
	return failed;
}
