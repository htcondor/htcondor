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

#ifndef SYSAPI_TEST_H
#define SYSAPI_TEST_H

/*
 * arch_test(int) tests sysapi_arch for consistency (making sure it is valid
 * over n trials), and to be sure that the returned string is not null or empty.
 *    PARAMS:
 *    int trials - how many times to check sysapi_arch for consistency.
 */
extern "C" int arch_test(int trials);

/*
 * free_fs_blocks_test(int, int, double) tests sysapi_free_fs_blocks against
 * negative results, and for consistency by filling up /tmp by half its
 * reported value, testing, unlinking, and testing again. Free space should
 * decrease then increase by the amount written, plus or minus some tolerance.
 * If it does not, warnings are issued. If more than some percentage of the
 * tests yield warnings, this whole test fails.
 *    PARAMS:
 *    int trials - how many times to check sysapi_free_fs_blocks for consistency.
 *    int tolerance - how many KB discrepency to allow in consistency check
 *        without issuing a warning.
 *    double warn_ok_ratio - what ratio (0=none,1=all) of the test performed can issue
 *        warnings without the test failing.
 */
extern "C" int free_fs_blocks_test(int trials, double tolerance, double
				warn_ok_ratio);

/*
 * idle_time_test(int, int, double) tests sysapi_idle_time against
 * negative results, and for consistency by testing idle_time, sleeping, and
 * testing idle_time again.  If the new idle time is not equal to the old idle
 * time +/- some tolerance, a warning is issued. If more than some percentage
 * of the tests yield warnings, this whole test fails.
 *    PARAMS:
 *    int trials - how many times to check sysapi_idle_time for consistency.
 *    int tolerance - how many seconds discrepency to allow in consistency check
 *        without issuing a warning.
 *    int interval - how many seconds to wait before checking the idle_time again.
 *    double warn_ok_ratio - what ratio of the test performed can issue
 *        warnings without the test failing.
 */
extern "C" int idle_time_test(int trials, int interval, int tolerance, double
				warn_ok_ratio);

/*
 * load_avg_test(int, int, int, double) tests sysapi_load_avg against negative
 * results, and for consistency by testing the load, spawning n children, and
 * testing load_avg again.  If the new idle time is not greater than the
 * old idle a warning is issued. If more than some percentage of the tests
 * yield warnings, this whole test fails.
 *    PARAMS:
 *    int trials - how many times to check sysapi_load_avg for consistency.
 *    int interval - how many seconds to wait before checking the load again.
 *    int num_children - how many children to spawn per test.
 *    double warn_ok_ratio - what ratio of the test performed can issue
 *        warnings without the test failing.
 */
extern "C" int load_avg_test(int trials, int interval, int num_children, double
				warn_ok_ratio);

/*
 * phys_memory_test(int, int, double) tests sysapi_phys_mem against negative
 * results, and for consistency by testing the memory, then testing it again.
 * If the new number is not equal to the old number a warning is issued. If
 * more than some percentage of the tests yield warnings, this whole test
 * fails.
 *    PARAMS:
 *    int trials - how many times to check sysapi_phy_mem for consistency.
 *    double warn_ok_ratio - what ratio of the test performed can issue
 *        warnings without the test failing.
 *
 */
extern "C" int phys_memory_test(int trials, double warn_ok_ratio);

/*
 * ncpus_test(int, double) tests sysapi_ncpus against negative
 * results, and for consistency by testing the memory, then testing it again.
 * If the new number is not equal to the old number a warning is issued. If
 * more than some percentage of the tests yield warnings, this whole test
 * fails.
 *    PARAMS:
 *    int trials - how many times to check sysapi_ncpus for consistency.
 *    double warn_ok_ratio - what ratio of the test performed can issue
 *        warnings without the test failing.
 *
 */
extern "C" int ncpus_test(int trials, double warn_ok_ratio);

/*
 * These three tests all have the same structure: they perform n^2 function
 * calls, and compute n standard deviations of the appropriate units; If any of
 * those standard deviations are greater than a certain percentace of the mean,
 * that block of n tests "fails". If more than a different percentage of those
 * testblocks "fail", the whole test fails.
 *    PARAMS:
 *    int    test_blocksize - the number of tests to run per testblock, and the
 *           number of testblocks to run. n^2 calls are made.
 *    double max_sd_variation_ratio - range: 0-1. Percentage of the mean. Used to test
 *           whether the standard deviation of a testblock is too high.
 *    double max_failed_test_ratio - range: 0-1. Percentage of test_blocksize.
 *           Used to determine whether the test as a whole fails.
 */
extern "C" int virt_memory_test(int test_blocksize, double
				max_sd_variation_ratio, double max_failed_test_ratio);
extern "C" int mips_test(int test_blocksize, double max_sd_variation_ratio,
				double max_failed_test_ratio);
extern "C" int kflops_test(int test_blocksize, double max_sd_variation_ratio,
				double max_failed_test_ratio);

#endif
