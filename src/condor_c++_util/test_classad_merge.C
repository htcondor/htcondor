/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-2002 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
 ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

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
