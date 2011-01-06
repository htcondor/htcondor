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
#include "condor_debug.h"
#include "sysapi.h"
#include "sysapi_externs.h"
#include "stdio.h"
#include "math.h"

int virt_memory_test(int test_blocksize, double max_sd_varation_ratio,
					 					double max_failed_test_ratio);

int virt_memory_test(int	test_blocksize,
					 double max_sd_varation_ratio, 
					 double max_failed_test_ratio)
{
	int foo,  bar;
	int	return_val = 0;
	int i, j;
	int	num_tests = 0;
	int	num_warnings = 0;
	double mean, raw_mean;
	double variance, raw_variance;
	long test[test_blocksize], raw_test[test_blocksize];
	double testblocks_sd[test_blocksize], raw_testblocks_sd[test_blocksize];
	double testblocks_mean[test_blocksize], raw_testblocks_mean[test_blocksize];

	dprintf(D_ALWAYS, "SysAPI: Running virt_memory_test.\n");
	dprintf(D_ALWAYS, "        I will test sysapi_swap_space (and sysapi_swap_space_raw) in "
					"blocks of %d tests, and take the standard\n", test_blocksize);
	dprintf(D_ALWAYS, "        deviation of those test blocks. If the standard deviation is "
					"above %2f%% of the average,\n", max_sd_varation_ratio);
	dprintf(D_ALWAYS, "        the swap space reported are erratic and the test block is "
					"considered a failure\n");
	dprintf(D_ALWAYS, "        I will run %d test blocks, and if more than %f%% of those blocks "
					"fail, this entire test fails.\n", test_blocksize, 
					max_failed_test_ratio*100);

	foo = sysapi_swap_space_raw();
	dprintf(D_ALWAYS, "SysAPI: Initial sysapi_swap_space_raw -> %d\n", foo);
	bar = sysapi_swap_space();
	dprintf(D_ALWAYS, "SysAPI: Initial sysapi_swap_space -> %d\n", bar);

	for (i=0; i<test_blocksize; i++) {
		raw_mean = 0;
		raw_variance = 0;

		for (j=0; j<test_blocksize; j++) {
			raw_test[j] = sysapi_swap_space_raw();

			raw_mean += raw_test[j];

			dprintf(D_ALWAYS, "SysAPI: Testblock %d, test %d: Raw swap space: %ld KB\n", 
							i, j, raw_test[j]);

			if (raw_test[j] <= 0) {
				dprintf(D_ALWAYS, "SysAPI: ERROR! Raw swap space (%d KB) is negative!\n", foo);
				return_val = 1;
			}
		}

		raw_mean /= test_blocksize;
		raw_testblocks_mean[i] = raw_mean;

		/* Test if there were any unusually large jumps in the means of the testblocks */
		num_tests++;
		if (i > 0) {
			if (max_sd_varation_ratio < fabs((raw_testblocks_mean[i]/raw_testblocks_mean[i-1])-1)) {
				dprintf(D_ALWAYS, "SysAPI: WARNING: Raw testblock %d's value was more than "
								"%2.2f%% different from the previous testblock - something "
								"changed dramatically.\n", i, max_sd_varation_ratio*100);
				num_warnings++;
			}
		}


		for (j=0; j<test_blocksize; j++) {
			raw_variance += (raw_test[j] - raw_mean)*(raw_test[j] - raw_mean);
		}
		raw_variance /= test_blocksize;

		raw_testblocks_sd[i] = sqrt(raw_variance);

		dprintf(D_ALWAYS, "SysAPI: Standard deviation of raw testblock %d is: %d KB\n", 
						i, (int)raw_testblocks_sd[i]);

		num_tests++;
		if (raw_testblocks_sd[i] > max_sd_varation_ratio*raw_mean) {
			dprintf(D_ALWAYS, "SysAPI: WARNING: Raw testblock %d had a standard deviation of %d, "
							"which is more than %2.2f%% of the mean.\n", i, 
							(int)raw_testblocks_sd[i], max_sd_varation_ratio*100);
			num_warnings++;
		}
	}

	if (((double)num_warnings/(double)num_tests) > max_sd_varation_ratio) {
			dprintf(D_ALWAYS, "SysAPI: ERROR! Failing because %d of the raw testblocks failed, "
							"which is more than %d (%2.2f%%).\n", num_warnings,
							(int)(max_sd_varation_ratio*test_blocksize), max_sd_varation_ratio*100);
			return_val = return_val | 1;
	}
	dprintf(D_ALWAYS, "\n\n");
	num_warnings = 0;

	for (i=0; i<test_blocksize; i++) {
		mean = 0;
		variance = 0;

		for (j=0; j<test_blocksize; j++) {
			test[j] = sysapi_swap_space();

			mean += test[j];

			dprintf(D_ALWAYS, "SysAPI: Testblock %d, test %d: cooked swap space: %ld\n", 
							i, j, test[j]);

			if (raw_test[j] <= 0) {
				dprintf(D_ALWAYS, "SysAPI: ERROR! Cooked swap space (%d KB) is negative!\n", bar);
				return_val = 1;
			}
		}

		mean /= test_blocksize;

		for (j=0; j<test_blocksize; j++) {
			variance += (test[j] - mean)*(test[j] - mean);
		}
		variance /= test_blocksize;
		testblocks_mean[i] = mean;

		/* Test if there were any unusually large jumps in the means of the testblocks */
		num_tests++;
		if (i > 0) {
			if (max_sd_varation_ratio < fabs((testblocks_mean[i]/testblocks_mean[i-1])-1)) {
				dprintf(D_ALWAYS, "SysAPI: WARNING: Cooked testblock %d's value was more than "
								"%2.2f%% different from the previous testblock - something "
								"changed dramatically.\n", i, max_sd_varation_ratio*100);
				num_warnings++;
			}
		}


		testblocks_sd[i] = sqrt(variance);

		dprintf(D_ALWAYS, "SysAPI: Standard deviation of cooked testblock %d is: %d KB\n", 
						i, (int)testblocks_sd[i]);

		num_tests++;
		if (testblocks_sd[i] > max_sd_varation_ratio*mean) {
			dprintf(D_ALWAYS, "SysAPI: WARNING: Testblock %d had a standard deviation of %d KB, "
							"which is more than %2.2f%% of the mean.\n", i, (int)testblocks_sd[i], 
							max_sd_varation_ratio*100);
			num_warnings++;
		}
	}

	if (((double)num_warnings/(double)num_tests) > max_sd_varation_ratio) {
			dprintf(D_ALWAYS, "SysAPI: ERROR! Failing because %d of the cooked testblocks failed, "
							"which is more than %d (%2.2f%%).\n", num_warnings, 
							(int)(max_sd_varation_ratio*test_blocksize), max_sd_varation_ratio*100);
			return_val = return_val | 1;
	}
	return return_val;
}
