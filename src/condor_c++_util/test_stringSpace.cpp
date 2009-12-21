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

#include <iostream>
#include "stringSpace.h"

/*-------------------------------------------------------------
 *
 *                 Global Variables  
 *
 *-------------------------------------------------------------*/
int number_of_tests         = 0;
int number_of_tests_passed  = 0;
int number_of_tests_failed  = 0;

/*-------------------------------------------------------------
 *
 *                 Function Prototypes
 *
 *-------------------------------------------------------------*/
static void test_number_of_strings(
    StringSpace *strings, 
	int         number_of_expected_strings,
	int         line_number);
static void test_string_in_space(
    StringSpace *strings, 
	char        *s,
	int         line_number);
static void test_string_not_in_space(
    StringSpace *strings, 
	char        *s,
	int         line_number);
static void test_string_is_correct(
    StringSpace  *strings,
    int          canonical_index,
	char         *original,
	int          line_number);
static void test_strings_are_equal(
    StringSpace  *strings,
	SSString     &s1,
	SSString     &s2,
	int          line_number);
static void test_strings_are_unequal(
    StringSpace  *strings,
	SSString     &s1,
	SSString     &s2,
	int          line_number);

int main (int argc, char *argv[]) 
{
	int         Alain_index;
	int         alain_index;
	int         roy_index;
	SSString    alain_ssstring;
	SSString    roy_ssstring, roy_ssstring_2;
	StringSpace strings;

	// We create a few strings, and run a bunch of tests
	// on them. Test results for each test will be printed to stdout. 
	// Each test prints a line number, and you can use that 
	// to begin figuring out what went wrong. 

	// Make "Alain" and play with it.
	test_number_of_strings(&strings, 0, __LINE__);
	test_string_not_in_space(&strings, "Alain", __LINE__);
	Alain_index = strings.getCanonical("Alain");
	test_string_in_space(&strings, "Alain", __LINE__);
	test_number_of_strings(&strings, 1, __LINE__);

	// Make "alain" and play with it. 
	test_string_not_in_space(&strings, "alain", __LINE__);
	alain_index = strings.getCanonical("alain", alain_ssstring);
	test_string_in_space(&strings, "alain", __LINE__);
	test_number_of_strings(&strings, 2, __LINE__);

	// Make a second copy of "alain", and make sure we do the right thing.
	strings.getCanonical("alain");
	test_string_in_space(&strings, "alain", __LINE__);
	test_number_of_strings(&strings, 2, __LINE__);

	// Make a second copy of "Alain", and again make sure we do the right thing.
	strings.getCanonical("Alain");
	test_string_in_space(&strings, "Alain", __LINE__);
	test_number_of_strings(&strings, 2, __LINE__);

	// Make a string "roy"
	roy_index = strings.getCanonical("roy", roy_ssstring);
	test_string_in_space(&strings, "roy", __LINE__);
	test_number_of_strings(&strings, 3, __LINE__);
	
	// Make sure the strings were stored correctly.
	test_string_is_correct(&strings, Alain_index, "Alain", __LINE__);
	test_string_is_correct(&strings, alain_index, "alain", __LINE__);
	test_string_is_correct(&strings, roy_index,   "roy",   __LINE__);

	// Make sure the == and != operators work.
	roy_index = strings.getCanonical("roy", roy_ssstring_2);
	test_strings_are_equal(&strings, roy_ssstring, roy_ssstring_2, __LINE__);
	test_strings_are_unequal(&strings, roy_ssstring, alain_ssstring, __LINE__);

	// If we dispose of the first "roy", the second one should still be there.
	roy_ssstring.dispose();
	//strings.disposeByIndex(roy_index);
	test_string_in_space(&strings, "roy", __LINE__);
	test_string_is_correct(&strings, roy_index, "roy", __LINE__);

	// But if we dispose of the scond "roy", it should no longer be there.
	// Note that getNumStrings() won't be correct though. That's not hard
	// to fix in a simple way, but I didn't get around to it. 
	//roy_ssstring_2.dispose();
	strings.disposeByIndex(roy_index);
	test_string_not_in_space(&strings, "roy", __LINE__);

	// Now we test that disposing of strings works okay. 
	strings.getCanonical("a");
	strings.getCanonical("b");
	int c_index = strings.getCanonical("c");
	strings.getCanonical("d");
	strings.getCanonical("e");
	int f_index = strings.getCanonical("f");

	strings.disposeByIndex(c_index);
	strings.disposeByIndex(f_index);
	strings.getCanonical("g");
	test_string_in_space(&strings, "g", __LINE__);
	strings.getCanonical("h");
	test_string_in_space(&strings, "g", __LINE__);
	test_string_in_space(&strings, "h", __LINE__);
	test_number_of_strings(&strings, 8, __LINE__);

	// Print out a summary of what happened.
	printf("\nResults: \n");
	if (number_of_tests_passed == number_of_tests) {
		printf("    All automatic tests were passed.\n");
	}
	else {
		printf("    Only %d out of %d automatic tests were passed.\n",
			   number_of_tests_passed, number_of_tests);
	}

	return 0;
}

void test_number_of_strings(
    StringSpace *strings, 
	int         number_of_expected_strings,
	int         line_number)
{
	int actual_number_of_strings;

	number_of_tests++;

	actual_number_of_strings = strings->getNumStrings();
	if (actual_number_of_strings == number_of_expected_strings) {
		printf("Passed: number of strings passed in line %d.\n", 
			   line_number);
 		number_of_tests_passed++;
	}
	else {
		printf("Failed: Expected %d strings, found %d strings in line %d.\n",
			   number_of_expected_strings, actual_number_of_strings,
			   line_number);
 		number_of_tests_failed++;
	}
	return;
}

void test_string_in_space(
    StringSpace *strings, 
	char        *s,
	int         line_number)
{
	number_of_tests++;

	if (strings->checkFor(s) != -1) {
		printf("Passed: \"%s\" is in space in line %d.\n",
			   s, line_number);
 		number_of_tests_passed++;
	}
	else {
		printf("Failed: \"%s\" is in not in space in line %d.\n",
			   s, line_number);
 		number_of_tests_failed++;
	}
	return;
}

void test_string_not_in_space(
    StringSpace *strings, 
	char        *s,
	int         line_number)
{
	number_of_tests++;

	if (strings->checkFor(s) == -1) {
		printf("Passed: \"%s\" is not space in line %d.\n",
			   s, line_number);
 		number_of_tests_passed++;
	} else {
		printf("Failed: \"%s\" is in space in line %d.\n",
			   s, line_number);
 		number_of_tests_failed++;
	}
	return;
}

void test_string_is_correct(
    StringSpace  *strings,
    int          canonical_index,
	char         *original,
	int          line_number)
{
	const char *string_from_canonical;
	
	string_from_canonical = (*strings)[canonical_index];
	number_of_tests++;
	if (original != NULL && string_from_canonical != NULL
		&& !strcmp(original, (*strings)[canonical_index])) {
		printf("Passed: The string for %d really is \"%s\" in line number %d.\n",
			   canonical_index, original, line_number);
 		number_of_tests_passed++;
	} else {
		printf("Failed: The string for %d is not \"%s\" but is %s"
			   "in line number %d.\n",
			   canonical_index, original, (*strings)[canonical_index], line_number);
 		number_of_tests_failed++;
	}
	return;
}

void test_strings_are_equal(
    StringSpace  *strings,
	SSString     &s1,
	SSString     &s2,
	int          line_number)
{
	number_of_tests++;
	if (s1 == s2) {
		printf("Passed: equality test in line number %d.\n", 
			   line_number);
 		number_of_tests_passed++;
	} else {
		printf("Failed: equality test in line number %d.\n",
			   line_number);
 		number_of_tests_failed++;
	}
	return;
}

void test_strings_are_unequal(
    StringSpace  *strings,
	SSString     &s1,
	SSString     &s2,
	int          line_number)
{
	number_of_tests++;
	if (s1 != s2) {
		printf("Passed: inequality test in line number %d.\n", 
			   line_number);
 		number_of_tests_passed++;
	} else {
		printf("Failed: inequality test in line number %d.\n",
			   line_number);
 		number_of_tests_failed++;
	}
	return;
}
