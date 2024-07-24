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

/*
	This file contains functions used by the rest of the unit_test suite.
 */

#include "condor_common.h"
#include "MyString.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "iso_dates.h"

/* These macros are pseudo asserts that will print an error message and
   exit if the given condition does not hold */
#define cut_assert_z(expr) \
    cut_assert_z_impl(expr, #expr, __FILE__, __LINE__);

#define cut_assert_nz(expr) \
    cut_assert_nz_impl(expr, #expr, __FILE__, __LINE__);

#define cut_assert_gz(expr) \
    cut_assert_gz_impl(expr, #expr, __FILE__, __LINE__);

#define cut_assert_lz(expr) \
    cut_assert_lz_impl(expr, #expr, __FILE__, __LINE__);

#define cut_assert_gez(expr) \
    cut_assert_gez_impl(expr, #expr, __FILE__, __LINE__);

#define cut_assert_lez(expr) \
    cut_assert_lez_impl(expr, #expr, __FILE__, __LINE__);

#define cut_assert_true(expr) \
    cut_assert_true_impl(expr, #expr, __FILE__, __LINE__);

#define cut_assert_false(expr) \
    cut_assert_false_impl(expr, #expr, __FILE__, __LINE__);

#define cut_assert_not_null(expr) \
    cut_assert_not_null_impl(expr, #expr, __FILE__, __LINE__);

#define cut_assert_null(expr) \
    cut_assert_null_impl(expr, #expr, __FILE__, __LINE__);

bool utest_sock_eq_octet( struct in_addr* address, unsigned char oct1, unsigned char oct2, unsigned char oct3, unsigned char oct4 );

/*  Prints TRUE or FALSE depending on if the input indicates success or failure */
const char* tfstr(bool);
const char* tfstr(int);

/*  tfstr() for ints where -1 indicates failure and 0 indicates success */
const char* tfnze(int);

/* Returns the empty string when the passed string is null */
const char* nicePrint(const char* str);

/* Exactly like strcmp, but treats NULL and "" as equal */
int niceStrCmp(const char* str1, const char* str2);

/* Frees a char** */
void free_helper(char** array, int num_strs);

/* Originallly from condor_c++_util/test_old_classads.cpp */
void make_big_string(int length, char **string, char **quoted_string);

/* Originally from condor_c++_util/test_old_classads.cpp*/
ClassAd* get_classad_from_file();

/* Originally from condor_c++_util/test_old_classads.cpp*/
bool floats_close( double one, double two, double diff = .0001);

bool strings_similar(const std::string& str1, const std::string& str2,
	const char* delims = " ");

/* Checks if the given strings contain the same strings when split into 
	tokens by the given delimiters .
   Note that if the two strings have duplicates, this does not check if they
    have the same number of duplicates. */
bool strings_similar(const char* str1, const char* str2, 
	const char* delims = " ");

/* Converts the given char** into a MyString seperated by the given delims */
std::string * convert_string_array(char** str, int size, const char* delim = " ");

/* Converts the given char** into a MyString seperated by the given delims */
std::string * convert_string_array(const char** str, int size, const char* delim = " ");

/* Deletes a char** */
void delete_helper(char** array, int num_strs);

/* 
 Adds a string representation of the tm struct into the string based on the 
 ISO8601Type. 
 */
void get_tm(ISO8601Type type, const struct tm &time, std::string& str);

/*
 Checks if the ClassAd has the following attributes with the given values
 	ATTR_PERIODIC_HOLD_CHECK
	ATTR_PERIODIC_REMOVE_CHECK
	ATTR_PERIODIC_RELEASE_CHECK
	ATTR_ON_EXIT_HOLD_CHECK
	ATTR_ON_EXIT_REMOVE_CHECK
 */
bool user_policy_ad_checker(ClassAd* ad,
							bool periodic_hold,
							bool periodic_remove,
							bool periodic_release,
							bool hold_check,
							bool remove_check,
							int absent_mask = 0);

/*
 Checks if the ClassAd has the following attributes with the given values
 	ATTR_TIMER_REMOVE_CHECK
	ATTR_PERIODIC_HOLD_CHECK
	ATTR_PERIODIC_REMOVE_CHECK
	ATTR_PERIODIC_RELEASE_CHECK
	ATTR_ON_EXIT_HOLD_CHECK
	ATTR_ON_EXIT_REMOVE_CHECK
 */
bool user_policy_ad_checker(ClassAd* ad,
							bool timer_remove,
							bool periodic_hold,
							bool periodic_remove,
							bool periodic_release,
							bool hold_check,
							bool remove_check,
							int absent_mask = 0);

/*
 Inserts the given attribute and value into the ClassAd
 */
void insert_into_ad(ClassAd* ad, const char* attribute, const char* value);

/* Prints error message and exits if value is not zero */
void cut_assert_z_impl(int value, const char *expr, const char *file, int line);

/* Prints error message and exits if value is zero */
int cut_assert_nz_impl(int value, const char *expr, const char *file, int line);

/* Prints error message and exits if value is not greater than zero */
int cut_assert_gz_impl(int value, const char *expr, const char *file, int line);

/* Prints error message and exits if value is not less than zero */
int cut_assert_lz_impl(int value, const char *expr, const char *file, int line);

/* Prints error message and exits if value is not greater than or equal to 
   zero */
int cut_assert_gez_impl(int value, const char *expr, const char *file, int line);

/* Prints error message and exits if value is not less than or equal to 
   zero */
int cut_assert_lez_impl(int value, const char *expr, const char *file, int line);

/* Prints error message and exits if value is not true */
bool cut_assert_true_impl(bool value, const char *expr, const char *file, int line);

/* Prints error message and exits if value is not false */
bool cut_assert_false_impl(bool value, const char *expr, const char *file, int line);

/* Prints error message and exits if value is NULL */
void* cut_assert_not_null_impl(void *value, const char *expr, const char *file, int line);

/* Prints error message and exits if value is not NULL */
void cut_assert_null_impl(void *value, const char *expr, const char *file, int line);

/* Creates an empty file */
void create_empty_file(const char *file);

#ifdef WIN32
#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS)
  #define DELTA_EPOCH_IN_MICROSECS  11644473600000000Ui64
#else
  #define DELTA_EPOCH_IN_MICROSECS  11644473600000000ULL
#endif

struct ttimezone
{
  int  tz_minuteswest; /* minutes W of Greenwich */
  int  tz_dsttime;     /* type of dst correction */
};

int gettimeofday(struct timeval *tv, struct ttimezone *tz);
#endif 
