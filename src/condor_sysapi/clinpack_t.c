/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
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
#include "condor_debug.h"
#include "sysapi.h"
#include "sysapi_externs.h"
#include "stdio.h"
#include "math.h"

int kflops_test(int test_blocksize, double perc_sd_variation_ok, double perc_failed_test_blocks_ok)
{
	int foo,  bar;
	int	return_val = 0;
	int i, j;
	int	num_tests = 0;
	int	num_warnings = 0;
	double mean, raw_mean;
	double variance, raw_variance;
	int test[test_blocksize], raw_test[test_blocksize];
	double testblocks_sd[test_blocksize], raw_testblocks_sd[test_blocksize];
	double testblocks_mean[test_blocksize], raw_testblocks_mean[test_blocksize];

	dprintf(D_ALWAYS, "SysAPI: Running kflops_test.\n");
	dprintf(D_ALWAYS, "        I will test sysapi_kflops (and sysapi_kflops_raw) in blocks of %d "
					"tests, and take the standard\n", test_blocksize);
	dprintf(D_ALWAYS, "        deviation of those test blocks. If the standard deviation is above "
					"%d%% of the average,\n", perc_sd_variation_ok);
	dprintf(D_ALWAYS, "        the kflops reported are erratic and the test block is considered "
					"a failure. I will run %d test blocks, and if\n");
	dprintf(D_ALWAYS, "        more than %d%% of those blocks fail, this entire test fails.\n", 
					test_blocksize, perc_failed_test_blocks_ok*100);

	foo = sysapi_kflops_raw();
	dprintf(D_ALWAYS, "SysAPI: Initial sysapi_kflops_raw -> %d\n", foo);
	bar = sysapi_kflops();
	dprintf(D_ALWAYS, "SysAPI: Initial sysapi_kflops -> %d\n", bar);
		
	for (i=0; i<test_blocksize; i++) {
		raw_mean = 0;
		raw_variance = 0;

		for (j=0; j<test_blocksize; j++) {
			raw_test[j] = sysapi_kflops_raw();

			raw_mean += raw_test[j];

			dprintf(D_ALWAYS, "SysAPI: Testblock %d, test %d: Raw kflops: %d\n", i, j, raw_test[j]);

			if (raw_test[j] <= 0) {
				dprintf(D_ALWAYS, "SysAPI: ERROR! Raw kflops (%d) is negative!\n", bar);
				return_val = 1;
			}
		}

		raw_mean /= test_blocksize;
		raw_testblocks_mean[i] = raw_mean;

		/* Test if there were any unusually large jumps in the means of the testblocks */
		num_tests++;
		if (i > 0) {
			if (perc_sd_variation_ok < fabs((raw_testblocks_mean[i]/raw_testblocks_mean[i-1])-1)) {
				dprintf(D_ALWAYS, "SysAPI: WARNING: Raw testblock %d's value was more than "
								"%2.2f%% different from the previous testblock - something "
								"changed dramatically.\n", i, perc_sd_variation_ok*100);
				num_warnings++;
			}
		}

		for (j=0; j<test_blocksize; j++) {
			raw_variance += (raw_test[j] - raw_mean)*(raw_test[j] - raw_mean);
		}
		raw_variance /= test_blocksize;

		raw_testblocks_sd[i] = sqrt(raw_variance);

		dprintf(D_ALWAYS, "SysAPI: Standard deviation of raw testblock %d is: %d KFLOPS\n",
						i, (int)raw_testblocks_sd[i]);

		num_tests++;
		if (raw_testblocks_sd[i] > perc_sd_variation_ok*mean) {
			dprintf(D_ALWAYS, "SysAPI: WARNING: Raw testblock %d had a standard deviation of %d, "
							"which is more than %2.2f%% of the mean.\n", i, 
							(int)raw_testblocks_sd[i], perc_sd_variation_ok*100);
			num_warnings++;
		}
	}


	if (((double)num_warnings/(double)num_tests) > perc_sd_variation_ok) {
			dprintf(D_ALWAYS, "SysAPI: ERROR! Failing because %d of the raw
							testblocks failed, which is more than %d
							(%2.2f%%).\n", num_warnings, perc_sd_variation_ok*test_blocksize,
							perc_sd_variation_ok*100);
			return_val = return_val | 1;
	}
	
	dprintf(D_ALWAYS, "\n\n");
	num_warnings = 0;

	for (i=0; i<test_blocksize; i++) {
		mean = 0;
		variance = 0;

		for (j=0; j<test_blocksize; j++) {
			test[j] = sysapi_kflops();
			mean += test[j];

			dprintf(D_ALWAYS, "SysAPI: Testblock %d, test %d: Cooked kflops: %d\n", i, j, test[j]);

			if (test[j] <= 0) {
				dprintf(D_ALWAYS, "SysAPI: ERROR! Cooked kflops (%d) is negative!\n", bar);
				return_val = 1;
			}
		}

		mean /= test_blocksize;
		testblocks_mean[i] = mean;

		/* Test if there were any unusually large jumps in the means of the testblocks */
		num_tests++;
		if (i > 0) {
			if (perc_sd_variation_ok < fabs((testblocks_mean[i]/testblocks_mean[i-1])-1)) {
				dprintf(D_ALWAYS, "SysAPI: WARNING: Cooked testblock %d's value was more than "
								"%2.2f%% different from the previous testblock - something "
								"changed dramatically.\n", i, perc_sd_variation_ok*100);
				num_warnings++;
			}
		}

		for (j=0; j<test_blocksize; j++) {
			variance += (test[j] - mean)*(test[j] - mean);
		}
		variance /= test_blocksize;

		testblocks_sd[i] = sqrt(variance);

		dprintf(D_ALWAYS, "SysAPI: Standard deviation of testblock %d is: %d KFLOPS\n", i, 
						(int)testblocks_sd[i]);

		num_tests++;
		if (testblocks_sd[i] > perc_sd_variation_ok*mean) {
			dprintf(D_ALWAYS, "SysAPI: WARNING: Testblock %d had a standard deviation of %d, "
							"which is more than %2.2f%% of the mean.\n", i, 
							(int)testblocks_sd[i], perc_sd_variation_ok*100);
			num_warnings++;
		}
	}

	if (((double)num_warnings/(double)num_tests) > perc_sd_variation_ok) {
			dprintf(D_ALWAYS, "SysAPI: ERROR! Failing because %d of the cooked testblocks failed, "
							"which is more than %d (%2.2f%%).\n", num_warnings, 
							perc_sd_variation_ok*test_blocksize, perc_sd_variation_ok*100);
			return_val = return_val | 1;
	}
	return return_val;
}
