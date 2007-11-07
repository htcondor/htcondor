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
#include "condor_classad.h"
#include "classad_merge.h"

char *classad_strings[] = 
{
	"A = 1, B = 2",
	"A = 1, B = 2",
	"B = 1241, C = 3, D = 4"
};

bool test_merge(
	bool verbose);

bool test_integer(
	ClassAd  *classad,
	char     *attribute,
	int      expected_value,
	int      line_number, 
	bool     verbose);

int main(int argc, char **argv)
{
	bool verbose;
	bool failed;

	if (argc > 1 && !strcmp(argv[1], "-v")) {
		verbose = true;
	} else {
		verbose = false;
	}

	failed = test_merge(verbose);
	if (failed) {
		printf("FAILED\n");
	} else {
		printf("All tests passed.\n");
	}
	return failed;
}

bool test_merge(
	bool verbose)
{
	bool    failed;
	ClassAd *c1, *c2, *c3;

	c1 = new ClassAd(classad_strings[0], ',');
	c2 = new ClassAd(classad_strings[1], ',');
	c3 = new ClassAd(classad_strings[2], ',');

	if (verbose) {
		printf("C1:\n"); c1->fPrint(stdout); printf("\n");
		printf("C2:\n"); c2->fPrint(stdout); printf("\n");
		printf("C3:\n"); c3->fPrint(stdout); printf("\n");
	}

	MergeClassAds(c1, c3, true);
	MergeClassAds(c2, c3, false);

	if (verbose) {
		printf("C1:\n"); c1->fPrint(stdout); printf("\n");
		printf("C2:\n"); c2->fPrint(stdout); printf("\n");
		printf("C3:\n"); c3->fPrint(stdout); printf("\n");
	}

	failed = false;
	failed = failed | test_integer(c1, "B", 1241, __LINE__, verbose);
	failed = failed | test_integer(c1, "C", 3, __LINE__, verbose);
	failed = failed | test_integer(c1, "D", 4, __LINE__, verbose);

	failed = failed | test_integer(c2, "B", 2, __LINE__, verbose);
	failed = failed | test_integer(c2, "C", 3, __LINE__, verbose);
	failed = failed | test_integer(c2, "D", 4, __LINE__, verbose);

	return failed;
}

bool test_integer(
	ClassAd  *classad,
	char     *attribute,
	int      expected_value,
	int      line_number,
	bool     verbose)
{
	int   actual_value;
	bool  failed;

	if (classad->LookupInteger(attribute, actual_value) 
		&& actual_value == expected_value) {
		if (verbose) {
			printf("Passed in line %d\n", line_number);
		}
		failed = false;
	} else {
		if (verbose) {
			printf("Failed in line %d, expected %d, got %d\n", 
				   line_number, expected_value, actual_value);
		}
		failed = true;
	}

	return failed;
}
