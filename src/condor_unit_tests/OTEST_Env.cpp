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

/* Test the Env implementation.
 */

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "function_test_driver.h"
#include "unit_test_utils.h"
#include "emit.h"
#include "condor_attributes.h"
#include "env.h"
#include <string>

static bool test_count_0(void);
static bool test_count_1(void);
static bool test_count_many(void);
static bool test_clear_empty(void);
static bool test_clear_non_empty(void);
static bool test_mf_v1r_or_v2q_ret_null(void);
static bool test_mf_v1r_or_v2q_detect_v1r(void);
static bool test_mf_v1r_or_v2q_detect_v2q(void);
static bool test_mf_v1r_or_v2q_add_null(void);
static bool test_mf_v2q_ret_null(void);
static bool test_mf_v2q_ret_valid(void);
static bool test_mf_v2q_ret_invalid_quotes(void);
static bool test_mf_v2q_ret_invalid_quotes_end(void);
static bool test_mf_v2q_ret_invalid_trail(void);
static bool test_mf_v2q_ret_invalid_name(void);
static bool test_mf_v2q_ret_invalid_delim(void);
static bool test_mf_v2q_error_invalid_quotes(void);
static bool test_mf_v2q_error_invalid_quotes_end(void);
static bool test_mf_v2q_error_invalid_trail(void);
static bool test_mf_v2q_error_invalid_name(void);
static bool test_mf_v2q_error_invalid_delim(void);
static bool test_mf_v2q_add_null(void);
static bool test_mf_v2q_add_invalid_delim_var(void);
static bool test_mf_v2q_add_invalid_quotes(void);
static bool test_mf_v2q_add_invalid_quotes_end(void);
static bool test_mf_v2q_add_invalid_trail(void);
static bool test_mf_v2q_add(void);
static bool test_mf_v2q_replace(void);
static bool test_mf_v2q_replace_v1r(void);
static bool test_mf_v2q_replace_add(void);
static bool test_mf_v2q_replace_add_v1r(void);
static bool test_mf_v2r_ret_null(void);
static bool test_mf_v2r_ret_valid(void);
static bool test_mf_v2r_ret_invalid_name(void);
static bool test_mf_v2r_ret_invalid_delim(void);
static bool test_mf_v2r_error_invalid_name(void);
static bool test_mf_v2r_error_invalid_delim(void);
static bool test_mf_v2r_add_null(void);
static bool test_mf_v2r_add_invalid(void);
static bool test_mf_v2r_add(void);
static bool test_mf_v2r_replace(void);
static bool test_mf_v2r_replace_v1r(void);
static bool test_mf_v2r_replace_add(void);
static bool test_mf_v2r_replace_add_v1r(void);
static bool test_mf_v1r_ret_null(void);
static bool test_mf_v1r_ret_valid(void);
static bool test_mf_v1r_ret_invalid_name(void);
static bool test_mf_v1r_ret_invalid_delim(void);
static bool test_mf_v1r_error_invalid_name(void);
static bool test_mf_v1r_error_invalid_delim(void);
static bool test_mf_v1r_add_null(void);
static bool test_mf_v1r_add_invalid(void);
static bool test_mf_v1r_add(void);
static bool test_mf_v1r_replace(void);
static bool test_mf_v1r_replace_v2r(void);
static bool test_mf_v1r_replace_v2q(void);
static bool test_mf_v1r_replace_add(void);
static bool test_mf_v1r_replace_add_v2r(void);
static bool test_mf_v1r_replace_add_v2q(void);
static bool test_mf_v1or2_r_ret_null(void);
static bool test_mf_v1or2_r_detect_v1r(void);
static bool test_mf_v1or2_r_detect_v2r(void);
static bool test_mf_v1or2_r_add_null(void);
static bool test_mf_v1or2_r_add_v2r(void);
static bool test_mf_str_array_ret_null(void);
static bool test_mf_str_array_ret_valid(void);
static bool test_mf_str_array_ret_invalid_name(void);
static bool test_mf_str_array_ret_invalid_delim(void);
static bool test_mf_str_array_add_null(void);
static bool test_mf_str_array_add_invalid(void);
static bool test_mf_str_array_add(void);
static bool test_mf_str_array_replace(void);
static bool test_mf_str_array_replace_add(void);
static bool test_mf_str_ret_null(void);
static bool test_mf_str_ret_valid(void);
static bool test_mf_str_ret_invalid_name(void);
static bool test_mf_str_ret_invalid_delim(void);
static bool test_mf_str_add_null(void);
static bool test_mf_str_add_invalid_name(void);
static bool test_mf_str_add_invalid_delim(void);
static bool test_mf_str_add(void);
static bool test_mf_str_replace(void);
static bool test_mf_str_replace_add(void);
static bool test_mf_env_add_empty(void);
static bool test_mf_env_add_one(void);
static bool test_mf_env_add_many(void);
static bool test_mf_env_replace(void);
static bool test_mf_env_replace_v1_v2(void);
static bool test_mf_env_replace_v2_v1(void);
static bool test_mf_env_replace_add(void);
static bool test_mf_env_replace_add_v1_v2(void);
static bool test_mf_env_replace_add_v2_v1(void);
static bool test_mf_env_itself(void);
static bool test_mf_ad_ret_null(void);
static bool test_mf_ad_ret_v1r_valid(void);
static bool test_mf_ad_ret_v2r_valid(void);
static bool test_mf_ad_ret_valid_define(void);
static bool test_mf_ad_ret_v1r_invalid_name(void);
static bool test_mf_ad_ret_v1r_invalid_delim(void);
static bool test_mf_ad_ret_v2r_invalid_name(void);
static bool test_mf_ad_ret_v2r_invalid_delim(void);
static bool test_mf_ad_error_v1r_invalid_name(void);
static bool test_mf_ad_error_v1r_invalid_delim(void);
static bool test_mf_ad_error_v2r_invalid_name(void);
static bool test_mf_ad_error_v2r_invalid_delim(void);
static bool test_mf_ad_add_null(void);
static bool test_mf_ad_add_define(void);
static bool test_mf_ad_add_v1r_one(void);
static bool test_mf_ad_add_v1r_many(void);
static bool test_mf_ad_add_v2r_one(void);
static bool test_mf_ad_add_v2r_many(void);
static bool test_mf_ad_v1r_replace(void);
static bool test_mf_ad_v2r_replace(void);
static bool test_mf_ad_v1r_replace_add(void);
static bool test_mf_ad_v2r_replace_add(void);
static bool test_set_env_with_error_message_ret_null(void);
static bool test_set_env_with_error_message_ret_valid(void);
static bool test_set_env_with_error_message_ret_invalid_name(void);
static bool test_set_env_with_error_message_ret_invalid_delim(void);
static bool test_set_env_with_error_message_err_invalid_name(void);
static bool test_set_env_with_error_message_err_invalid_delim(void);
static bool test_set_env_with_error_message_add_null(void);
static bool test_set_env_with_error_message_add_invalid_delim(void);
static bool test_set_env_with_error_message_add_invalid_var(void);
static bool test_set_env_with_error_message_add(void);
static bool test_set_env_with_error_message_replace(void);
static bool test_set_env_str_ret_null(void);
static bool test_set_env_str_ret_valid(void);
static bool test_set_env_str_ret_invalid_name(void);
static bool test_set_env_str_ret_invalid_delim(void);
static bool test_set_env_str_add_null(void);
static bool test_set_env_str_add_invalid(void);
static bool test_set_env_str_add(void);
static bool test_set_env_str_replace(void);
static bool test_set_env_str_str_ret_null_var(void);
static bool test_set_env_str_str_ret_null_val(void);
static bool test_set_env_str_str_ret_valid(void);
static bool test_set_env_str_str_add_null_var(void);
static bool test_set_env_str_str_add_null_val(void);
static bool test_set_env_str_str_add(void);
static bool test_set_env_str_str_replace(void);
static bool test_set_env_mystr_ret_empty_var(void);
static bool test_set_env_mystr_ret_empty_val(void);
static bool test_set_env_mystr_ret_valid(void);
static bool test_set_env_mystr_add_empty_var(void);
static bool test_set_env_mystr_add_empty_val(void);
static bool test_set_env_mystr_add(void);
static bool test_set_env_mystr_replace(void);
static bool test_insert_env_into_classad_v1_empty(void);
static bool test_insert_env_into_classad_v2_empty(void);
static bool test_insert_env_into_classad_v1_v1_replace(void);
static bool test_insert_env_into_classad_v1_v2_replace(void);
static bool test_insert_env_into_classad_v2_v1_replace(void);
static bool test_insert_env_into_classad_v2_v2_replace(void);
static bool test_insert_env_into_classad_version_v1(void);
static bool test_insert_env_into_classad_version_v1_os_winnt(void);
static bool test_insert_env_into_classad_version_v1_os_win32(void);
static bool test_insert_env_into_classad_version_v1_os_unix(void);
static bool test_insert_env_into_classad_version_v1_semi(void);
static bool test_insert_env_into_classad_version_v1_line(void);
static bool test_insert_env_into_classad_version_v1_current(void);
static bool test_insert_env_into_classad_version_v1_error_v2(void);
static bool test_insert_env_into_classad_version_v1_error(void);
static bool test_insert_env_into_classad_version_v2(void);
static bool test_condor_version_requires_v1_false(void);
static bool test_condor_version_requires_v1_true(void);
static bool test_condor_version_requires_v1_this(void);
static bool test_get_delim_str_v2_raw_return_empty(void);
static bool test_get_delim_str_v2_raw_return_v1(void);
static bool test_get_delim_str_v2_raw_return_v2(void);
static bool test_get_delim_str_v2_raw_result_empty(void);
static bool test_get_delim_str_v2_raw_result_v1(void);
static bool test_get_delim_str_v2_raw_result_v2(void);
static bool test_get_delim_str_v2_raw_result_add(void);
static bool test_get_delim_str_v2_raw_result_replace(void);
static bool test_get_delim_str_v2_raw_result_add_replace(void);
static bool test_get_delim_str_v2_raw_mark_empty(void);
static bool test_get_delim_str_v2_raw_mark_v1(void);
static bool test_get_delim_str_v2_raw_mark_v2(void);
static bool test_get_delim_str_v1_raw_return_empty(void);
static bool test_get_delim_str_v1_raw_return_v1(void);
static bool test_get_delim_str_v1_raw_return_v2(void);
static bool test_get_delim_str_v1_raw_return_delim(void);
static bool test_get_delim_str_v1_raw_error_delim(void);
static bool test_get_delim_str_v1_raw_result_empty(void);
static bool test_get_delim_str_v1_raw_result_v1(void);
static bool test_get_delim_str_v1_raw_result_v2(void);
static bool test_get_delim_str_v1_raw_result_add(void);
static bool test_get_delim_str_v1_raw_result_replace(void);
static bool test_get_delim_str_v1_raw_result_add_replace(void);
static bool test_get_delim_str_v1or2_raw_ad_return_empty(void);
static bool test_get_delim_str_v1or2_raw_ad_return_v1(void);
static bool test_get_delim_str_v1or2_raw_ad_return_v2(void);
static bool test_get_delim_str_v1or2_raw_ad_return_invalid_v1(void);
static bool test_get_delim_str_v1or2_raw_ad_return_invalid_v2(void);
static bool test_get_delim_str_v1or2_raw_ad_error_v1(void);
static bool test_get_delim_str_v1or2_raw_ad_error_v2(void);
static bool test_get_delim_str_v1or2_raw_ad_result_empty(void);
static bool test_get_delim_str_v1or2_raw_ad_result_v1(void);
static bool test_get_delim_str_v1or2_raw_ad_result_v2(void);
static bool test_get_delim_str_v1or2_raw_ad_result_replace(void);
static bool test_get_delim_str_v1or2_raw_return_empty(void);
static bool test_get_delim_str_v1or2_raw_return_v1(void);
static bool test_get_delim_str_v1or2_raw_return_v2(void);
static bool test_get_delim_str_v1or2_raw_result_empty(void);
static bool test_get_delim_str_v1or2_raw_result_v1(void);
static bool test_get_delim_str_v1or2_raw_result_v2(void);
static bool test_get_delim_str_v1or2_raw_result_add(void);
static bool test_get_delim_str_v1or2_raw_result_replace(void);
static bool test_get_delim_str_v1or2_raw_result_add_replace(void);
static bool test_get_delim_str_v2_quoted_return_empty(void);
static bool test_get_delim_str_v2_quoted_return_v1(void);
static bool test_get_delim_str_v2_quoted_return_v2(void);
static bool test_get_delim_str_v2_quoted_result_empty(void);
static bool test_get_delim_str_v2_quoted_result_v1(void);
static bool test_get_delim_str_v2_quoted_result_v2(void);
static bool test_get_delim_str_v2_quoted_result_add(void);
static bool test_get_delim_str_v2_quoted_result_replace(void);
static bool test_get_delim_str_v2_quoted_result_add_replace(void);
static bool test_get_delim_str_v1r_or_v2q_return_empty(void);
static bool test_get_delim_str_v1r_or_v2q_return_v1(void);
static bool test_get_delim_str_v1r_or_v2q_return_v2(void);
static bool test_get_delim_str_v1r_or_v2q_result_empty(void);
static bool test_get_delim_str_v1r_or_v2q_result_v1(void);
static bool test_get_delim_str_v1r_or_v2q_result_v2(void);
static bool test_get_delim_str_v1r_or_v2q_result_add(void);
static bool test_get_delim_str_v1r_or_v2q_result_replace(void);
static bool test_get_delim_str_v1r_or_v2q_result_add_replace(void);
static bool test_get_string_array_empty(void);
static bool test_get_string_array_v1(void);
static bool test_get_string_array_v2(void);
static bool test_get_string_array_add(void);
static bool test_get_string_array_replace(void);
static bool test_get_string_array_add_replace(void);
static bool test_get_env_bool_return_empty_empty(void);
static bool test_get_env_bool_return_empty_not(void);
static bool test_get_env_bool_return_hit_one(void);
static bool test_get_env_bool_return_hit_all(void);
static bool test_get_env_bool_value_miss(void);
static bool test_get_env_bool_value_hit_one(void);
static bool test_get_env_bool_value_hit_all(void);
static bool test_is_safe_env_v1_value_false_null(void);
static bool test_is_safe_env_v1_value_false_semi(void);
static bool test_is_safe_env_v1_value_false_newline(void);
static bool test_is_safe_env_v1_value_false_param(void);
static bool test_is_safe_env_v1_value_true_one(void);
static bool test_is_safe_env_v1_value_true_quotes(void);
static bool test_is_safe_env_v2_value_false_null(void);
static bool test_is_safe_env_v2_value_false_newline(void);
static bool test_is_safe_env_v2_value_true_one(void);
static bool test_is_safe_env_v2_value_true_many(void);
static bool test_get_env_v1_delim_param_winnt(void);
static bool test_get_env_v1_delim_param_win32(void);
static bool test_get_env_v1_delim_param_unix(void);
static bool test_is_v2_quoted_string_false_v1(void);
static bool test_is_v2_quoted_string_false_v2(void);
static bool test_is_v2_quoted_string_true(void);
static bool test_v2_quoted_to_v2_raw_return_false_miss_end(void);
static bool test_v2_quoted_to_v2_raw_return_false_trail(void);
static bool test_v2_quoted_to_v2_raw_return_true(void);
static bool test_v2_quoted_to_v2_raw_return_true_semi(void);
static bool test_v2_quoted_to_v2_raw_error_miss_end(void);
static bool test_v2_quoted_to_v2_raw_error_trail(void);
static bool test_v2_quoted_to_v2_raw_result(void);
static bool test_v2_quoted_to_v2_raw_result_semi(void);
static bool test_input_was_v1_false_empty(void);
static bool test_input_was_v1_false_v2q_or(void);
static bool test_input_was_v1_false_v2q(void);
static bool test_input_was_v1_false_v2r_or(void);
static bool test_input_was_v1_false_v2r(void);
static bool test_input_was_v1_false_array(void);
static bool test_input_was_v1_false_str(void);
static bool test_input_was_v1_false_env(void);
static bool test_input_was_v1_false_ad(void);
static bool test_input_was_v1_true_v1r_or(void);
static bool test_input_was_v1_true_v1r(void);
static bool test_input_was_v1_true_ad(void);

#ifdef WIN32
#define V1_ENV_DELIM "|"
#else
#define V1_ENV_DELIM ";"
#endif
#define V1_ENV_DELIM_NIX ";"
#define V1_ENV_DELIM_WIN "|"


//char* constants
static const char 
	*V1R = "one=1" V1_ENV_DELIM "two=2" V1_ENV_DELIM "three=3",	//V1Raw format
	   *V1R_NIX = "one=1;two=2;three=3",
	   *V1R_WIN = "one=1|two=2|three=3",
	   *V1R_MISS_NAME = "=1" V1_ENV_DELIM "two=2" V1_ENV_DELIM "three=3",
	   *V1R_MISS_DELIM = "one1" V1_ENV_DELIM "two=2" V1_ENV_DELIM "three=3",
	   *V1R_MISS_BOTH = "=1" V1_ENV_DELIM "two2" V1_ENV_DELIM "three=3",
	*V2R ="one=1 two=2 three=3\0",	//V2Raw format
	   *V2R_MISS_NAME ="=1 two=2 three=3",
	   *V2R_MISS_DELIM ="one1 two=2 three=3",
	   *V2R_MISS_BOTH ="=1 two2 three=3",
	   *ARRAY_SKIP_BAD_STR = "one=1 two2 three=3",
	   *V2R_SEMI ="one=1 two=2 three=3 semi=" V1_ENV_DELIM,
	   *V2R_MARK =" one=1 two=2 three=3 semi=" V1_ENV_DELIM ,
	*V2Q ="\"one=1 two=2 three=3\"",	//V2Quoted format
	   *V2Q_MISS_NAME = "\"=1 two=2 three=3\"",
	   *V2Q_MISS_DELIM = "\"one1 two=2 three=3\"",
	   *V2Q_MISS_BOTH = "\"=1 two2 three=3\"",
	   *V2Q_MISS_END = "\"one=1 two=2 three=3",
	   *V2Q_TRAIL = "\"one=1 two=2 three=3\"extra=stuff",
	   *V2Q_SEMI ="\"one=1 two=2 three=3 semi=" V1_ENV_DELIM "\"",
	   *V2Q_DELIM_SEMI = "\"one=1" V1_ENV_DELIM "two=2" V1_ENV_DELIM "three=3\"",
	*V1R_ADD = "four=4" V1_ENV_DELIM "five=5",	//V1Raw format
	*V2R_ADD = "four=4 five=5",	//V2Raw format
	*V1R_REP = "one=10" V1_ENV_DELIM "two=200" V1_ENV_DELIM "three=3000",	//V1Raw format
//		*V1R_REP_NIX = "one=10;two=200;three=3000",
		*V1R_REP_WIN = "one=10|two=200|three=3000",
	*V2R_REP = "one=10 two=200 three=3000",	//V2Raw format
	   *V2R_REP_SEMI = "one=10 two=200 three=3000 semi=" V1_ENV_DELIM,
	*V2Q_REP = "\"one=10 two=200 three=3000\"",	//V2Quoted format
	   *V2Q_REP_SEMI = "\"one=10 two=200 three=3000 semi=" V1_ENV_DELIM "\"",
	*V1R_REP_ADD = "one=10" V1_ENV_DELIM "two=200" V1_ENV_DELIM "three=3000" V1_ENV_DELIM "four=4" V1_ENV_DELIM "five=5",	//V1Raw format
	*V2R_REP_ADD = "one=10 two=200 three=3000 four=4 five=5",	//V2Raw format
	   *V2R_REP_ADD_SEMI = "one=10 two=200 three=3000 four=4 five=5 semi=" V1_ENV_DELIM,
	*V2Q_REP_ADD = "\"one=10 two=200 three=3000 four=4 five=5\"",	//V2Quoted
	   *V2Q_REP_ADD_SEMI = "\"one=10 two=200 three=3000 four=4 five=5 semi=" V1_ENV_DELIM "\"",
	*AD = "\tone=1\n\t\ttwo=2\n\t\tthree=3",	//ClassAd string
	*AD_V1 = "\tEnv = \"one=1" V1_ENV_DELIM "two=2" V1_ENV_DELIM "three=3\"",	//ClassAd with V1 Env 
	   *AD_V1_WIN = "\tEnv = \"one=1|two=2|three=3\"\nEnvDelim = \"|\"", 
	   *AD_V1_MISS_NAME = "\tEnv = \"=1" V1_ENV_DELIM "two=2" V1_ENV_DELIM "three=3\"",
	   *AD_V1_MISS_DELIM = "\tEnv = \"one1" V1_ENV_DELIM "two=2" V1_ENV_DELIM "three=3\"",
	   *AD_V1_MISS_BOTH = "\tEnv = \"=1" V1_ENV_DELIM "two2" V1_ENV_DELIM "three=3\"",
	   *AD_V1_REP = "\tEnv = \"one=10" V1_ENV_DELIM "two=200" V1_ENV_DELIM "three=3000\"", 
	   *AD_V1_REP_ADD = "\tEnv = \"one=10" V1_ENV_DELIM "two=200" V1_ENV_DELIM "three=3000" V1_ENV_DELIM "four=4" V1_ENV_DELIM "five=5\"", 
	*AD_V2 = "\tEnvironment = \"one=1 two=2 three=3\"",	//ClassAd with V2 Env
	   *AD_V2_MISS_NAME = "\tEnvironment = \"=1 two=2 three=3\"",
	   *AD_V2_MISS_DELIM = "\tEnvironment = \"one1 two=2 three=3\"",
	   *AD_V2_MISS_BOTH = "\tEnvironment = \"=1 two2 three=3\"",
	   *AD_V2_REP = "\tEnvironment = \"one=10 two=200 three=3000\"",
	   *AD_V2_REP_ADD = "\tEnvironment = \"one=10 two=200 three=3000 four=4 "
	    	"five=5\"",
		*AD_V2_SEMI = "\tEnvironment = \"one=1 two=2 three=3 semi=" V1_ENV_DELIM "\"",
	*ONE = "one=1",	//Single Env Var string
	   *ONE_MISS_NAME = "=1",
	   *ONE_MISS_DELIM = "one1",
	   *ONE_MISS_VAL = "one=",
	   *ONE_REP = "one=10",
	*NULL_DELIM = "one=1\0two=2\0three=3\0",	//NULL-Delimited string
	   *NULL_DELIM_MISS_NAME = "=1\0two=2\0three=3\0",
	   *NULL_DELIM_MISS_DELIM = "one1\0two=2\0three=3\0",
	   *NULL_DELIM_REP = "one=10\0two=200\0three=3000\0",
	   *NULL_DELIM_REP_ADD = "one=10\0two=200\0three=3000\0four=4\0five=5\0",
	*EMPTY = "";
//char** constants
static const char
	*ARRAY[] = {"one=1", "two=2", "three=3", ""},
	*ARRAY_MISS_NAME[] = {"=1", "two=2", "three=3", ""},
	*ARRAY_MISS_DELIM[] = {"one1", "two=2", "three=3", ""},
	*ARRAY_SKIP_BAD[] = {"one=1", "two2", "three=3", ""},
	*ARRAY_SKIP_BAD_CLEAN[] = {"one=1", "three=3", ""},
	*ARRAY_REP[] = {"one=10", "two=200", "three=3000", ""},
	*ARRAY_REP_ADD[] = {"one=10", "two=200", "three=3000", "four=4", "five=5", 
		""};

//MyString constants
static const MyString
	ADD("one=1 two=2 three=3"),
		ADD_SEMI("one=1 two=2 three=3 semi=;"),
	REP("one=10 two=200 three=3000"),
	   REP_SEMI("one=10 two=200 three=3000 semi=;"),
	REP_ADD("one=10 two=200 three=3000 four=4 five=5"),
	   REP_ADD_SEMI("one=10 two=200 three=3000 four=4 five=5 semi=;");

bool OTEST_Env(void) {
	emit_object("Env");
	emit_comment("The Env object maintains a collection of environment "
		"settings, e.g. for a process that we are about to exec.  Environment "
		"values may be fed into the Env object in several formats.");
	emit_comment("Many of the tests use getDelimitedStringForDisplay() to "
		"compare the contents of the Env so any issues with that may cause "
		"other tests to fail.");
	emit_comment("Although the EXPECTED OUTPUT and ACTUAL OUTPUT may not "
		"be identical for some tests, they contain the same variables and "
		"values just in a different order which doesn't matter");
	
	FunctionDriver driver;
	driver.register_function(test_count_0);
	driver.register_function(test_count_1);
	driver.register_function(test_count_many);
	driver.register_function(test_clear_empty);
	driver.register_function(test_clear_non_empty);
	driver.register_function(test_mf_v1r_or_v2q_ret_null);
	driver.register_function(test_mf_v1r_or_v2q_detect_v1r);
	driver.register_function(test_mf_v1r_or_v2q_detect_v2q);
	driver.register_function(test_mf_v1r_or_v2q_add_null);
	driver.register_function(test_mf_v2q_ret_null);
	driver.register_function(test_mf_v2q_ret_valid);
	driver.register_function(test_mf_v2q_ret_invalid_quotes);
	driver.register_function(test_mf_v2q_ret_invalid_quotes_end);
	driver.register_function(test_mf_v2q_ret_invalid_trail);
	driver.register_function(test_mf_v2q_ret_invalid_name);
	driver.register_function(test_mf_v2q_ret_invalid_delim);
	driver.register_function(test_mf_v2q_error_invalid_quotes);
	driver.register_function(test_mf_v2q_error_invalid_quotes_end);
	driver.register_function(test_mf_v2q_error_invalid_trail);
	driver.register_function(test_mf_v2q_error_invalid_name);
	driver.register_function(test_mf_v2q_error_invalid_delim);
	driver.register_function(test_mf_v2q_add_null);
	driver.register_function(test_mf_v2q_add_invalid_delim_var);
	driver.register_function(test_mf_v2q_add_invalid_quotes);
	driver.register_function(test_mf_v2q_add_invalid_quotes_end);
	driver.register_function(test_mf_v2q_add_invalid_trail);
	driver.register_function(test_mf_v2q_add);
	driver.register_function(test_mf_v2q_replace);
	driver.register_function(test_mf_v2q_replace_v1r);
	driver.register_function(test_mf_v2q_replace_add);
	driver.register_function(test_mf_v2q_replace_add_v1r);
	driver.register_function(test_mf_v2r_ret_null);
	driver.register_function(test_mf_v2r_ret_valid);
	driver.register_function(test_mf_v2r_ret_invalid_name);
	driver.register_function(test_mf_v2r_ret_invalid_delim);
	driver.register_function(test_mf_v2r_error_invalid_name);
	driver.register_function(test_mf_v2r_error_invalid_delim);
	driver.register_function(test_mf_v2r_add_null);
	driver.register_function(test_mf_v2r_add_invalid);
	driver.register_function(test_mf_v2r_add);
	driver.register_function(test_mf_v2r_replace);
	driver.register_function(test_mf_v2r_replace_v1r);
	driver.register_function(test_mf_v2r_replace_add);
	driver.register_function(test_mf_v2r_replace_add_v1r);
	driver.register_function(test_mf_v1r_ret_null);
	driver.register_function(test_mf_v1r_ret_valid);
	driver.register_function(test_mf_v1r_ret_invalid_name);
	driver.register_function(test_mf_v1r_ret_invalid_delim);
	driver.register_function(test_mf_v1r_error_invalid_name);
	driver.register_function(test_mf_v1r_error_invalid_delim);
	driver.register_function(test_mf_v1r_add_null);
	driver.register_function(test_mf_v1r_add_invalid);
	driver.register_function(test_mf_v1r_add);
	driver.register_function(test_mf_v1r_replace);
	driver.register_function(test_mf_v1r_replace_v2r);
	driver.register_function(test_mf_v1r_replace_v2q);
	driver.register_function(test_mf_v1r_replace_add);
	driver.register_function(test_mf_v1r_replace_add_v2r);
	driver.register_function(test_mf_v1r_replace_add_v2q);
	driver.register_function(test_mf_v1or2_r_ret_null);
	driver.register_function(test_mf_v1or2_r_detect_v1r);
	driver.register_function(test_mf_v1or2_r_detect_v2r);
	driver.register_function(test_mf_v1or2_r_add_null);
	driver.register_function(test_mf_v1or2_r_add_v2r);
	driver.register_function(test_mf_str_array_ret_null);
	driver.register_function(test_mf_str_array_ret_valid);
	driver.register_function(test_mf_str_array_ret_invalid_name);
	driver.register_function(test_mf_str_array_ret_invalid_delim);
	driver.register_function(test_mf_str_array_add_null);
	driver.register_function(test_mf_str_array_add_invalid);
	driver.register_function(test_mf_str_array_add);
	driver.register_function(test_mf_str_array_replace);
	driver.register_function(test_mf_str_array_replace_add);
	driver.register_function(test_mf_str_ret_null);
	driver.register_function(test_mf_str_ret_valid);
	driver.register_function(test_mf_str_ret_invalid_name);
	driver.register_function(test_mf_str_ret_invalid_delim);
	driver.register_function(test_mf_str_add_null);
	driver.register_function(test_mf_str_add_invalid_name);
	driver.register_function(test_mf_str_add_invalid_delim);
	driver.register_function(test_mf_str_add);
	driver.register_function(test_mf_str_replace);
	driver.register_function(test_mf_str_replace_add);
	driver.register_function(test_mf_env_add_empty);
	driver.register_function(test_mf_env_add_one);
	driver.register_function(test_mf_env_add_many);
	driver.register_function(test_mf_env_replace);
	driver.register_function(test_mf_env_replace_v1_v2);
	driver.register_function(test_mf_env_replace_v2_v1);
	driver.register_function(test_mf_env_replace_add);
	driver.register_function(test_mf_env_replace_add_v1_v2);
	driver.register_function(test_mf_env_replace_add_v2_v1);
	driver.register_function(test_mf_env_itself);
	driver.register_function(test_mf_ad_ret_null);
	driver.register_function(test_mf_ad_ret_v1r_valid);
	driver.register_function(test_mf_ad_ret_v2r_valid);
	driver.register_function(test_mf_ad_ret_valid_define);
	driver.register_function(test_mf_ad_ret_v1r_invalid_name);
	driver.register_function(test_mf_ad_ret_v1r_invalid_delim);
	driver.register_function(test_mf_ad_ret_v2r_invalid_name);
	driver.register_function(test_mf_ad_ret_v2r_invalid_delim);
	driver.register_function(test_mf_ad_error_v1r_invalid_name);
	driver.register_function(test_mf_ad_error_v1r_invalid_delim);
	driver.register_function(test_mf_ad_error_v2r_invalid_name);
	driver.register_function(test_mf_ad_error_v2r_invalid_delim);
	driver.register_function(test_mf_ad_add_null);
	driver.register_function(test_mf_ad_add_define);
	driver.register_function(test_mf_ad_add_v1r_one);
	driver.register_function(test_mf_ad_add_v1r_many);
	driver.register_function(test_mf_ad_add_v2r_one);
	driver.register_function(test_mf_ad_add_v2r_many);
	driver.register_function(test_mf_ad_v1r_replace);
	driver.register_function(test_mf_ad_v2r_replace);
	driver.register_function(test_mf_ad_v1r_replace_add);
	driver.register_function(test_mf_ad_v2r_replace_add);
	driver.register_function(test_set_env_with_error_message_ret_null);
	driver.register_function(test_set_env_with_error_message_ret_valid);
	driver.register_function(test_set_env_with_error_message_ret_invalid_name);
	driver.register_function(test_set_env_with_error_message_ret_invalid_delim);
	driver.register_function(test_set_env_with_error_message_err_invalid_name);
	driver.register_function(test_set_env_with_error_message_err_invalid_delim);
	driver.register_function(test_set_env_with_error_message_add_null);
	driver.register_function(test_set_env_with_error_message_add_invalid_delim);
	driver.register_function(test_set_env_with_error_message_add_invalid_var);
	driver.register_function(test_set_env_with_error_message_add);
	driver.register_function(test_set_env_with_error_message_replace);
	driver.register_function(test_set_env_str_ret_null);
	driver.register_function(test_set_env_str_ret_valid);
	driver.register_function(test_set_env_str_ret_invalid_name);
	driver.register_function(test_set_env_str_ret_invalid_delim);
	driver.register_function(test_set_env_str_add_null);
	driver.register_function(test_set_env_str_add_invalid);
	driver.register_function(test_set_env_str_add);
	driver.register_function(test_set_env_str_replace);
	driver.register_function(test_set_env_str_str_ret_null_var);
	driver.register_function(test_set_env_str_str_ret_null_val);
	driver.register_function(test_set_env_str_str_ret_valid);
	driver.register_function(test_set_env_str_str_add_null_var);
	driver.register_function(test_set_env_str_str_add_null_val);
	driver.register_function(test_set_env_str_str_add);
	driver.register_function(test_set_env_str_str_replace);
	driver.register_function(test_set_env_mystr_ret_empty_var);
	driver.register_function(test_set_env_mystr_ret_empty_val);
	driver.register_function(test_set_env_mystr_ret_valid);
	driver.register_function(test_set_env_mystr_add_empty_var);
	driver.register_function(test_set_env_mystr_add_empty_val);
	driver.register_function(test_set_env_mystr_add);
	driver.register_function(test_set_env_mystr_replace);
	driver.register_function(test_insert_env_into_classad_v1_empty);
	driver.register_function(test_insert_env_into_classad_v2_empty);
	driver.register_function(test_insert_env_into_classad_v1_v1_replace);
	driver.register_function(test_insert_env_into_classad_v1_v2_replace);
	driver.register_function(test_insert_env_into_classad_v2_v1_replace);
	driver.register_function(test_insert_env_into_classad_v2_v2_replace);
	driver.register_function(test_insert_env_into_classad_version_v1);
	driver.register_function(test_insert_env_into_classad_version_v1_os_winnt);
	driver.register_function(test_insert_env_into_classad_version_v1_os_win32);
	driver.register_function(test_insert_env_into_classad_version_v1_os_unix);
	driver.register_function(test_insert_env_into_classad_version_v1_semi);
	driver.register_function(test_insert_env_into_classad_version_v1_line);
	driver.register_function(test_insert_env_into_classad_version_v1_current);
	driver.register_function(test_insert_env_into_classad_version_v1_error_v2);
	driver.register_function(test_insert_env_into_classad_version_v1_error);
	driver.register_function(test_insert_env_into_classad_version_v2);
	driver.register_function(test_condor_version_requires_v1_false);
	driver.register_function(test_condor_version_requires_v1_true);
	driver.register_function(test_condor_version_requires_v1_this);
	driver.register_function(test_get_delim_str_v2_raw_return_empty);
	driver.register_function(test_get_delim_str_v2_raw_return_v1);
	driver.register_function(test_get_delim_str_v2_raw_return_v2);
	driver.register_function(test_get_delim_str_v2_raw_result_empty);
	driver.register_function(test_get_delim_str_v2_raw_result_v1);
	driver.register_function(test_get_delim_str_v2_raw_result_v2);
	driver.register_function(test_get_delim_str_v2_raw_result_add);
	driver.register_function(test_get_delim_str_v2_raw_result_replace);
	driver.register_function(test_get_delim_str_v2_raw_result_add_replace);
	driver.register_function(test_get_delim_str_v2_raw_mark_empty);
	driver.register_function(test_get_delim_str_v2_raw_mark_v1);
	driver.register_function(test_get_delim_str_v2_raw_mark_v2);
	driver.register_function(test_get_delim_str_v1_raw_return_empty);
	driver.register_function(test_get_delim_str_v1_raw_return_v1);
	driver.register_function(test_get_delim_str_v1_raw_return_v2);
	driver.register_function(test_get_delim_str_v1_raw_return_delim);
	driver.register_function(test_get_delim_str_v1_raw_error_delim);
	driver.register_function(test_get_delim_str_v1_raw_result_empty);
	driver.register_function(test_get_delim_str_v1_raw_result_v1);
	driver.register_function(test_get_delim_str_v1_raw_result_v2);
	driver.register_function(test_get_delim_str_v1_raw_result_add);
	driver.register_function(test_get_delim_str_v1_raw_result_replace);
	driver.register_function(test_get_delim_str_v1_raw_result_add_replace);
	driver.register_function(test_get_delim_str_v1or2_raw_ad_return_empty);
	driver.register_function(test_get_delim_str_v1or2_raw_ad_return_v1);
	driver.register_function(test_get_delim_str_v1or2_raw_ad_return_v2);
	driver.register_function(test_get_delim_str_v1or2_raw_ad_return_invalid_v1);
	driver.register_function(test_get_delim_str_v1or2_raw_ad_return_invalid_v2);
	driver.register_function(test_get_delim_str_v1or2_raw_ad_error_v1);
	driver.register_function(test_get_delim_str_v1or2_raw_ad_error_v2);
	driver.register_function(test_get_delim_str_v1or2_raw_ad_result_empty);
	driver.register_function(test_get_delim_str_v1or2_raw_ad_result_v1);
	driver.register_function(test_get_delim_str_v1or2_raw_ad_result_v2);
	driver.register_function(test_get_delim_str_v1or2_raw_ad_result_replace);
	driver.register_function(test_get_delim_str_v1or2_raw_return_empty);
	driver.register_function(test_get_delim_str_v1or2_raw_return_v1);
	driver.register_function(test_get_delim_str_v1or2_raw_return_v2);
	driver.register_function(test_get_delim_str_v1or2_raw_result_empty);
	driver.register_function(test_get_delim_str_v1or2_raw_result_v1);
	driver.register_function(test_get_delim_str_v1or2_raw_result_v2);
	driver.register_function(test_get_delim_str_v1or2_raw_result_add);
	driver.register_function(test_get_delim_str_v1or2_raw_result_replace);
	driver.register_function(test_get_delim_str_v1or2_raw_result_add_replace);
	driver.register_function(test_get_delim_str_v2_quoted_return_empty);
	driver.register_function(test_get_delim_str_v2_quoted_return_v1);
	driver.register_function(test_get_delim_str_v2_quoted_return_v2);
	driver.register_function(test_get_delim_str_v2_quoted_result_empty);
	driver.register_function(test_get_delim_str_v2_quoted_result_v1);
	driver.register_function(test_get_delim_str_v2_quoted_result_v2);
	driver.register_function(test_get_delim_str_v2_quoted_result_add);
	driver.register_function(test_get_delim_str_v2_quoted_result_replace);
	driver.register_function(test_get_delim_str_v2_quoted_result_add_replace);
	driver.register_function(test_get_delim_str_v1r_or_v2q_return_empty);
	driver.register_function(test_get_delim_str_v1r_or_v2q_return_v1);
	driver.register_function(test_get_delim_str_v1r_or_v2q_return_v2);
	driver.register_function(test_get_delim_str_v1r_or_v2q_result_empty);
	driver.register_function(test_get_delim_str_v1r_or_v2q_result_v1);
	driver.register_function(test_get_delim_str_v1r_or_v2q_result_v2);
	driver.register_function(test_get_delim_str_v1r_or_v2q_result_add);
	driver.register_function(test_get_delim_str_v1r_or_v2q_result_replace);
	driver.register_function(test_get_delim_str_v1r_or_v2q_result_add_replace);
	driver.register_function(test_get_string_array_empty);
	driver.register_function(test_get_string_array_v1);
	driver.register_function(test_get_string_array_v2);
	driver.register_function(test_get_string_array_add);
	driver.register_function(test_get_string_array_replace);
	driver.register_function(test_get_string_array_add_replace);
	driver.register_function(test_get_env_bool_return_empty_empty);
	driver.register_function(test_get_env_bool_return_empty_not);
	driver.register_function(test_get_env_bool_return_hit_one);
	driver.register_function(test_get_env_bool_return_hit_all);
	driver.register_function(test_get_env_bool_value_miss);
	driver.register_function(test_get_env_bool_value_hit_one);
	driver.register_function(test_get_env_bool_value_hit_all);
	driver.register_function(test_is_safe_env_v1_value_false_null);
	driver.register_function(test_is_safe_env_v1_value_false_semi);
	driver.register_function(test_is_safe_env_v1_value_false_newline);
	driver.register_function(test_is_safe_env_v1_value_false_param);
	driver.register_function(test_is_safe_env_v1_value_true_one);
	driver.register_function(test_is_safe_env_v1_value_true_quotes);
	driver.register_function(test_is_safe_env_v2_value_false_null);
	driver.register_function(test_is_safe_env_v2_value_false_newline);
	driver.register_function(test_is_safe_env_v2_value_true_one);
	driver.register_function(test_is_safe_env_v2_value_true_many);
	driver.register_function(test_get_env_v1_delim_param_winnt);
	driver.register_function(test_get_env_v1_delim_param_win32);
	driver.register_function(test_get_env_v1_delim_param_unix);
	driver.register_function(test_is_v2_quoted_string_false_v1);
	driver.register_function(test_is_v2_quoted_string_false_v2);
	driver.register_function(test_is_v2_quoted_string_true);
	driver.register_function(test_v2_quoted_to_v2_raw_return_false_miss_end);
	driver.register_function(test_v2_quoted_to_v2_raw_return_false_trail);
	driver.register_function(test_v2_quoted_to_v2_raw_return_true);
	driver.register_function(test_v2_quoted_to_v2_raw_return_true_semi);
	driver.register_function(test_v2_quoted_to_v2_raw_error_miss_end);
	driver.register_function(test_v2_quoted_to_v2_raw_error_trail);
	driver.register_function(test_v2_quoted_to_v2_raw_result);
	driver.register_function(test_v2_quoted_to_v2_raw_result_semi);
	driver.register_function(test_input_was_v1_false_empty);
	driver.register_function(test_input_was_v1_false_v2q_or);
	driver.register_function(test_input_was_v1_false_v2q);
	driver.register_function(test_input_was_v1_false_v2r_or);
	driver.register_function(test_input_was_v1_false_v2r);
	driver.register_function(test_input_was_v1_false_array);
	driver.register_function(test_input_was_v1_false_str);
	driver.register_function(test_input_was_v1_false_env);
	driver.register_function(test_input_was_v1_false_ad);
	driver.register_function(test_input_was_v1_true_v1r_or);
	driver.register_function(test_input_was_v1_true_v1r);
	driver.register_function(test_input_was_v1_true_ad);
	
	return driver.do_all_functions();
}

static bool test_count_0() {
	emit_test("Test that Count() returns 0 for an Env object with 0 "
		"environment variables.");
	Env env;
	int expect = 0;
	int actual = env.Count();
	emit_output_expected_header();
	emit_retval("%d", expect);
	emit_output_actual_header();
	emit_retval("%d", actual);
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_count_1() {
	emit_test("Test that Count() returns 1 for an Env object with 1 "
		"environment variable after adding one with SetEnv().");
	Env env;
	env.SetEnv("one", "1");
	int expect = 1;
	int actual = env.Count();
	emit_output_expected_header();
	emit_retval("%d", expect);
	emit_output_actual_header();
	emit_retval("%d", actual);
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_count_many() {
	emit_test("Test that Count() returns the correct number of environment "
		"varaibles after adding many variables with MergeFromV2Raw().");
	Env env;
	env.MergeFromV2Raw(V2R, NULL);
	int expect = 3;
	int actual = env.Count();
	emit_output_expected_header();
	emit_retval("%d", expect);
	emit_output_actual_header();
	emit_retval("%d", actual);
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_clear_empty() {
	emit_test("Test Clear() on an empty Env object.");
	Env env;
	env.Clear();
	int expect = 0;
	int actual = env.Count();
	emit_output_expected_header();
	emit_param("Count", "%d", expect);
	emit_output_actual_header();
	emit_param("Count", "%d", actual);
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_clear_non_empty() {
	emit_test("Test Clear() on a non-empty Env object.");
	Env env;
	env.MergeFromV2Raw(V2R, NULL);
	env.Clear();
	int expect = 0;
	int actual = env.Count();
	emit_output_expected_header();
	emit_param("Count", "%d", expect);
	emit_output_actual_header();
	emit_param("Count", "%d", actual);
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_mf_v1r_or_v2q_ret_null() {
	emit_test("Test that MergeFromV1RawOrV2Quoted() returns true when passed "
		"a NULL string.");
	Env env;
	bool expect = true;
	bool actual = env.MergeFromV1RawOrV2Quoted(NULL, NULL);
	emit_input_header();
	emit_param("STRING", "%s", "NULL");
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_mf_v1r_or_v2q_detect_v1r() {
	emit_test("Test that MergeFromV1RawOrV2Quoted() correctly handles a V1Raw"
		"string by checking that InputWasV1() returns true.");
	emit_comment("MergeFromV1RaworV2Quoted() just calls MergeFromV1Raw(), "
		"which is tested below.");
	Env env;
	bool expect = true;
	env.MergeFromV1RawOrV2Quoted(V1R, NULL);
	bool actual = env.InputWasV1();
	emit_input_header();
	emit_param("STRING", "%s", V1R);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_param("InputWasV1()", "%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("InputWasV1()", "%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_mf_v1r_or_v2q_detect_v2q() {
	emit_test("Test that MergeFromV1RawOrV2Quoted() correctly handles a "
		"V2Quoted string by checking that InputWasV1() returns false.");
	emit_comment("MergeFromV1RaworV2Quoted() just calls MergeFromVqQuoted(), "
		"which is tested below.");
	Env env;
	bool expect = false;
	env.MergeFromV1RawOrV2Quoted(V2Q_SEMI, NULL);
	bool actual = env.InputWasV1();
	emit_input_header();
	emit_param("STRING", "%s", V2Q_SEMI);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_param("InputWasV1()", "%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("InputWasV1()", "%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_mf_v1r_or_v2q_add_null() {
	emit_test("Test that MergeFromV1RawOrV2Quoted() doesn't add any "
		"environment variables for a NULL string.");
	Env env;
	MyString actual;
	env.MergeFromV1RawOrV2Quoted(NULL, NULL);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("STRING", "%s", "NULL");
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_param("Env", "%s", EMPTY);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(actual != EMPTY) {
		FAIL;
	}
	PASS;
}

static bool test_mf_v2q_ret_null() {
	emit_test("Test that MergeFromV2Quoted() returns true when passed a NULL "
		"string.");
	Env env;
	bool expect = true;
	bool actual = env.MergeFromV2Quoted(NULL, NULL);
	emit_input_header();
	emit_param("STRING", "%s", "NULL");
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_mf_v2q_ret_valid() {
	emit_test("Test that MergeFromV2Quoted() returns true when passed a valid"
		" V2Quoted string.");
	Env env;
	bool expect = true;
	bool actual = env.MergeFromV2Quoted(V2Q, NULL);
	emit_input_header();
	emit_param("STRING", "%s", V2Q);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_mf_v2q_ret_invalid_quotes() {
	emit_test("Test that MergeFromV2Quoted() returns false when passed an "
		"invalid V2Quoted string due to no quotes.");
	Env env;
	bool expect = false;
	bool actual = env.MergeFromV2Quoted(V2R, NULL);
	emit_input_header();
	emit_param("STRING", "%s", V2R);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_mf_v2q_ret_invalid_quotes_end() {
	emit_test("Test that MergeFromV2Quoted() returns false when passed an "
		"invalid V2Quoted string due missing quotes at the end.");
	Env env;
	bool expect = false;
	bool actual = env.MergeFromV2Quoted(V2Q_MISS_END, NULL);
	emit_input_header();
	emit_param("STRING", "%s", V2Q_MISS_END);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_mf_v2q_ret_invalid_trail() {
	emit_test("Test that MergeFromV2Quoted() returns false when passed an "
		"invalid V2Quoted string due to trailing characters after the quotes.");
	Env env;
	bool expect = false;
	bool actual = env.MergeFromV2Quoted(V2Q_TRAIL, NULL);
	emit_input_header();
	emit_param("STRING", "%s", V2Q_TRAIL);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_mf_v2q_ret_invalid_name() {
	emit_test("Test that MergeFromV2Quoted() returns false when passed an "
		"invalid V2Quoted string due to a missing variable name.");
	Env env;
	bool expect = false;
	bool actual = env.MergeFromV2Quoted(V2Q_MISS_NAME, NULL);
	emit_input_header();
	emit_param("STRING", "%s", V2Q_MISS_NAME);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_mf_v2q_ret_invalid_delim() {
	emit_test("Test that MergeFromV2Quoted() returns false when passed an "
		"invalid V2Quoted string due to a missing delimiter.");
	Env env;
	bool expect = false;
	bool actual = env.MergeFromV2Quoted(V2Q_MISS_DELIM, NULL);
	emit_input_header();
	emit_param("STRING", "%s", V2Q_MISS_DELIM);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_mf_v2q_error_invalid_quotes() {
	emit_test("Test that MergeFromV2Quoted() generates an error message for "
		"an invalid V2Quoted string due to no quotes.");
	emit_comment("This test just checks if the error message is not empty.");
	Env env;
	MyString actual;
	env.MergeFromV2Quoted(V1R, &actual);
	emit_input_header();
	emit_param("STRING", "%s", V1R);
	emit_param("MyString", "%s", "");
	emit_output_actual_header();
	emit_param("Error Message", "%s", actual.Value());
	if(actual.IsEmpty()) {
		FAIL;
	}
	PASS;
}

static bool test_mf_v2q_error_invalid_quotes_end() {
	emit_test("Test that MergeFromV2Quoted() generates an error message for "
		"an invalid V2Quoted string due to missing quotes at the end.");
	emit_comment("This test just checks if the error message is not empty.");
	Env env;
	MyString actual;
	env.MergeFromV2Quoted(V2Q_MISS_END, &actual);
	emit_input_header();
	emit_param("STRING", "%s", V2Q_MISS_END);
	emit_param("MyString", "%s", "");
	emit_output_actual_header();
	emit_param("Error Message", "%s", actual.Value());
	if(actual.IsEmpty()) {
		FAIL;
	}
	PASS;
}

static bool test_mf_v2q_error_invalid_trail() {
	emit_test("Test that MergeFromV2Quoted() generates an error message for "
		"an invalid V2Quoted string due to trailing characters after the "
		"quotes.");
	emit_comment("This test just checks if the error message is not empty.");
	Env env;
	MyString actual;
	env.MergeFromV2Quoted(V2Q_TRAIL, &actual);
	emit_input_header();
	emit_param("STRING", "%s", V2Q_TRAIL);
	emit_param("MyString", "%s", "");
	emit_output_actual_header();
	emit_param("Error Message", "%s", actual.Value());
	if(actual.IsEmpty()) {
		FAIL;
	}
	PASS;
}

static bool test_mf_v2q_error_invalid_name() {
	emit_test("Test that MergeFromV2Quoted() generates an error message for "
		"an invalid V2Quoted string due to a missing variable name.");
	emit_comment("This test just checks if the error message is not empty.");
	Env env;
	MyString actual;
	env.MergeFromV2Quoted(V2Q_MISS_NAME, &actual);
	emit_input_header();
	emit_param("STRING", "%s", V2Q_MISS_NAME);
	emit_param("MyString", "%s", "");
	emit_output_actual_header();
	emit_param("Error Message", "%s", actual.Value());
	if(actual.IsEmpty()) {
		FAIL;
	}
	PASS;
}

static bool test_mf_v2q_error_invalid_delim() {
	emit_test("Test that MergeFromV2Quoted() generates an error message for "
		"an invalid V2Quoted string due to a missing delimiter.");
	emit_comment("This test just checks if the error message is not empty.");
	Env env;
	MyString actual;
	env.MergeFromV2Quoted(V2Q_MISS_DELIM, &actual);
	emit_input_header();
	emit_param("STRING", "%s", V2Q_MISS_DELIM);
	emit_param("MyString", "%s", "");
	emit_output_actual_header();
	emit_param("Error Message", "%s", actual.Value());
	if(actual.IsEmpty()) {
		FAIL;
	}
	PASS;
}

static bool test_mf_v2q_add_null() {
	emit_test("Test that MergeFromV2Quoted() doesn't add the environment "
		"variables for a NULL string.");
	Env env;
	MyString actual;
	env.MergeFromV2Quoted(NULL, NULL);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("STRING", "%s", "NULL");
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_param("Env", "%s", EMPTY);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(actual != EMPTY) {
		FAIL;
	}
	PASS;
}

static bool test_mf_v2q_add_invalid_delim_var() {
	emit_test("Test that MergeFromV2Quoted() doesn't add the environment "
		"variables for an invalid V2Quoted string with a missing delimiter and "
		"a missing variable name.");
	Env env;
	MyString actual;
	env.MergeFromV2Quoted(V2Q_MISS_BOTH, NULL);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("STRING", "%s", V2Q_MISS_BOTH);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_param("Env", "%s", EMPTY);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(actual != EMPTY) {
		FAIL;
	}
	PASS;
}

static bool test_mf_v2q_add_invalid_quotes() {
	emit_test("Test that MergeFromV2Quoted() doesn't add the environment "
		"variables for an invalid V2Quoted string due to missing quotes.");
	Env env;
	MyString actual;
	env.MergeFromV2Quoted(V2R, NULL);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("STRING", "%s", V2R);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_param("Env", "%s", EMPTY);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(actual != EMPTY) {
		FAIL;
	}
	PASS;
}

static bool test_mf_v2q_add_invalid_quotes_end() {
	emit_test("Test that MergeFromV2Quoted() doesn't add the environment "
		"variables for an invalid V2Quoted string due to missing quotes at the "
		"end.");
	Env env;
	MyString actual;
	env.MergeFromV2Quoted(V2Q_MISS_END, NULL);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("STRING", "%s", V2Q_MISS_END);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_param("Env", "%s", EMPTY);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(actual != EMPTY) {
		FAIL;
	}
	PASS;
}

static bool test_mf_v2q_add_invalid_trail() {
	emit_test("Test that MergeFromV2Quoted() doesn't add the environment "
		"variables for an invalid V2Quoted string due to trailing characters "
		"after the quotes at the end.");
	Env env;
	MyString actual;
	env.MergeFromV2Quoted(V2Q_TRAIL, NULL);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("STRING", "%s", V2Q_TRAIL);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_param("Env", "%s", EMPTY);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(actual != EMPTY) {
		FAIL;
	}
	PASS;
}

static bool test_mf_v2q_add() {
	emit_test("Test that MergeFromV2Quoted() adds the environment variables "
		"for a valid V2Quoted string.");
	Env env;
	MyString actual;
	env.MergeFromV2Quoted(V2Q, NULL);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("STRING", "%s", V2Q);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_param("Env", "%s", V2R);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(!strings_similar(actual.Value(), V2R)) {
		FAIL;
	}
	PASS;
}

static bool test_mf_v2q_replace() {
	emit_test("Test that MergeFromV2Quoted() replaces the environment "
		"variables for a valid V2Quoted string.");
	Env env;
	MyString actual;
	env.MergeFromV2Quoted(V2Q, NULL);
	env.MergeFromV2Quoted(V2Q_REP, NULL);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("Env", "%s", V2Q);
	emit_param("STRING", "%s", V2Q_REP);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_param("Env", "%s", V2R_REP);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(!strings_similar(actual.Value(), V2R_REP)) {
		FAIL;
	}
	PASS;
}

static bool test_mf_v2q_replace_v1r() {
	emit_test("Test that MergeFromV2Quoted() replaces the environment "
		"variables for a valid V2Quoted string on an Env object originally "
		"constructed from a V1Raw string.");
	Env env;
	MyString actual;
	env.MergeFromV1Raw(V1R, NULL);
	env.MergeFromV2Quoted(V2Q_REP_SEMI, NULL);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("Env", "%s", V1R);
	emit_param("STRING", "%s", V2Q_REP_SEMI);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_param("Env", "%s", V2R_REP_SEMI);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(!strings_similar(actual.Value(), V2R_REP_SEMI)) {
		FAIL;
	}
	PASS;
}

static bool test_mf_v2q_replace_add() {
	emit_test("Test that MergeFromV2Quoted() replaces some environment "
		"variables and also adds new ones for a valid V2Quoted string.");
	Env env;
	MyString actual;
	env.MergeFromV2Quoted(V2Q, NULL);
	env.MergeFromV2Quoted(V2Q_REP_ADD, NULL);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("Env", "%s", V2Q);
	emit_param("STRING", "%s", V2Q_REP_ADD);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_param("Env", "%s", V2R_REP_ADD);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(!strings_similar(actual.Value(), V2R_REP_ADD)) {
		FAIL;
	}
	PASS;
}

static bool test_mf_v2q_replace_add_v1r() {
	emit_test("Test that MergeFromV2Quoted() replaces some environment "
		"variables and also adds new ones for a valid V2Quoted string on an Env"
		" object originally constructed from a V1Raw string.");
	Env env;
	MyString actual;
	env.MergeFromV1Raw(V1R, NULL);
	env.MergeFromV2Quoted(V2Q_REP_ADD_SEMI, NULL);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("Env", "%s", V1R);
	emit_param("STRING", "%s", V2Q_REP_ADD_SEMI);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_param("Env", "%s", V2R_REP_ADD_SEMI);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(!strings_similar(actual.Value(), V2R_REP_ADD_SEMI)) {
		FAIL;
	}
	PASS;
}

static bool test_mf_v2r_ret_null() {
	emit_test("Test that MergeFromV2Raw() returns true when passed a NULL "
		"string.");
	Env env;
	bool expect = true;
	bool actual = env.MergeFromV2Raw(NULL, NULL);
	emit_input_header();
	emit_param("STRING", "%s", "NULL");
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_mf_v2r_ret_valid() {
	emit_test("Test that MergeFromV2Raw() returns true when passed a valid "
		"V2Raw string.");
	Env env;
	bool expect = true;
	bool actual = env.MergeFromV2Raw(V2R, NULL);
	emit_input_header();
	emit_param("STRING", "%s", V2R);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_mf_v2r_ret_invalid_name() {
	emit_test("Test that MergeFromV2Raw() returns false when passed an "
		"invalid V2Raw string due to a missing variable name.");
	Env env;
	bool expect = false;
	bool actual = env.MergeFromV2Raw(V2R_MISS_NAME, NULL);
	emit_input_header();
	emit_param("STRING", "%s", V2R_MISS_NAME);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_mf_v2r_ret_invalid_delim() {
	emit_test("Test that MergeFromV2Raw() returns false when passed an "
		"invalid V2Raw string due to a missing delimiter.");
	Env env;
	bool expect = false;
	bool actual = env.MergeFromV2Raw(V2R_MISS_DELIM, NULL);
	emit_input_header();
	emit_param("STRING", "%s", V2R_MISS_DELIM);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_mf_v2r_error_invalid_name() {
	emit_test("Test that MergeFromV2Raw() generates an error message for "
		"an invalid V2Raw string due to a missing variable name.");
	emit_comment("This test just checks if the error message is not empty.");
	Env env;
	MyString actual;
	env.MergeFromV2Raw(V2R_MISS_NAME, &actual);
	emit_input_header();
	emit_param("STRING", "%s", V2R_MISS_NAME);
	emit_param("MyString", "%s", "");
	emit_output_actual_header();
	emit_param("Error Message", "%s", actual.Value());
	if(actual.IsEmpty()) {
		FAIL;
	}
	PASS;
}

static bool test_mf_v2r_error_invalid_delim() {
	emit_test("Test that MergeFromV2Raw() generates an error message for "
		"an invalid V2Raw string due to a missing delimiter.");
	emit_comment("This test just checks if the error message is not empty.");
	Env env;
	MyString actual;
	env.MergeFromV2Raw(V2R_MISS_DELIM, &actual);
	emit_input_header();
	emit_param("STRING", "%s", V2R_MISS_DELIM);
	emit_param("MyString", "%s", "");
	emit_output_actual_header();
	emit_param("Error Message", "%s", actual.Value());
	if(actual.IsEmpty()) {
		FAIL;
	}
	PASS;
}

static bool test_mf_v2r_add_null() {
	emit_test("Test that MergeFromV2Raw() doesn't add the environment "
		"variable for a NULL string.");
	Env env;
	MyString actual;
	env.MergeFromV2Raw(NULL, NULL);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("STRING", "%s", "NULL");
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_param("Env", "%s", EMPTY);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(actual != EMPTY) {
		FAIL;
	}
	PASS;
}

static bool test_mf_v2r_add_invalid() {
	emit_test("Test that MergeFromV2Raw() doesn't add the environment "
		"variables for an invalid V2Raw string.");
	Env env;
	MyString actual;
	env.MergeFromV2Raw(V2R_MISS_BOTH, NULL);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("STRING", "%s", V2R_MISS_BOTH);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_param("Env", "%s", EMPTY);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(actual != EMPTY) {
		FAIL;
	}
	PASS;
}

static bool test_mf_v2r_add() {
	emit_test("Test that MergeFromV2Raw() adds the environment variables "
		"for a valid V2Raw string.");
	Env env;
	MyString actual;
	env.MergeFromV2Raw(V2R, NULL);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("STRING", "%s", V2R);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_param("Env", "%s", V2R);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(!strings_similar(actual.Value(), V2R)) {
		FAIL;
	}
	PASS;
}

static bool test_mf_v2r_replace() {
	emit_test("Test that MergeFromV2Raw() replaces the environment "
		"variables for a valid V2Raw string.");
	Env env;
	MyString actual;
	env.MergeFromV2Raw(V2R, NULL);
	env.MergeFromV2Raw(V2R_REP, NULL);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("Env", "%s", V2R);
	emit_param("STRING", "%s", V2R_REP);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_param("Env", "%s", V2R_REP);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(!strings_similar(actual.Value(), V2R_REP)) {
		FAIL;
	}
	PASS;
}

static bool test_mf_v2r_replace_v1r() {
	emit_test("Test that MergeFromV2Raw() replaces the environment "
		"variables for a valid V2Raw string on an Env object originally "
		"constructed from a V1Raw string.");
	Env env;
	MyString actual;
	env.MergeFromV1Raw(V1R, NULL);
	env.MergeFromV2Raw(V2R_REP_SEMI, NULL);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("Env", "%s", V1R);
	emit_param("STRING", "%s", V2R_REP_SEMI);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_param("Env", "%s", V2R_REP_SEMI);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(!strings_similar(actual.Value(), V2R_REP_SEMI)) {
		FAIL;
	}
	PASS;
}

static bool test_mf_v2r_replace_add() {
	emit_test("Test that MergeFromV2Raw() replaces some environment "
		"variables and also adds new ones for a valid V2Raw string.");
	Env env;
	MyString actual;
	env.MergeFromV2Raw(V2R, NULL);
	env.MergeFromV2Raw(V2R_REP_ADD, NULL);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("Env", "%s", V2R);
	emit_param("STRING", "%s", V2R_REP_ADD);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_param("Env", "%s", V2R_REP_ADD);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(!strings_similar(actual.Value(), V2R_REP_ADD)) {
		FAIL;
	}
	PASS;
}

static bool test_mf_v2r_replace_add_v1r() {
	emit_test("Test that MergeFromV2Raw() replaces some environment "
		"variables and also adds new ones for a valid V2Raw string on an Env "
		"object originally constructed from a V1Raw string.");
	Env env;
	MyString actual;
	env.MergeFromV1Raw(V1R, NULL);
	env.MergeFromV2Raw(V2R_REP_ADD_SEMI, NULL);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("Env", "%s", V1R);
	emit_param("STRING", "%s", V2R_REP_ADD_SEMI);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_param("Env", "%s", V2R_REP_ADD_SEMI);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(!strings_similar(actual.Value(), V2R_REP_ADD_SEMI)) {
		FAIL;
	}
	PASS;
}

static bool test_mf_v1r_ret_null() {
	emit_test("Test that MergeFromV1Raw() returns true when passed a NULL "
		"string.");
	Env env;
	bool expect = true;
	bool actual = env.MergeFromV1Raw(NULL, NULL);
	emit_input_header();
	emit_param("STRING", "%s", "NULL");
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_mf_v1r_ret_valid() {
	emit_test("Test that MergeFromV1Raw() returns true when passed a valid "
		"V1Raw string.");
	Env env;
	bool expect = true;
	bool actual = env.MergeFromV1Raw(V1R, NULL);
	emit_input_header();
	emit_param("STRING", "%s", V1R);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_mf_v1r_ret_invalid_name() {
	emit_test("Test that MergeFromV1Raw() returns false when passed an "
		"invalid V1Raw string due to a missing variable name.");
	Env env;
	bool expect = false;
	bool actual = env.MergeFromV1Raw(V1R_MISS_NAME, NULL);
	emit_input_header();
	emit_param("STRING", "%s", V1R_MISS_NAME);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_mf_v1r_ret_invalid_delim() {
	emit_test("Test that MergeFromV1Raw() returns false when passed an "
		"invalid V1Raw string due to a missing delimiter.");
	Env env;
	bool expect = false;
	bool actual = env.MergeFromV1Raw(V1R_MISS_DELIM, NULL);
	emit_input_header();
	emit_param("STRING", "%s", V1R_MISS_DELIM);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_mf_v1r_error_invalid_name() {
	emit_test("Test that MergeFromV1Raw() generates an error message for "
		"an invalid V1Raw string due to a missing variable name.");
	emit_comment("This test just checks if the error message is not empty.");
	Env env;
	MyString actual;
	env.MergeFromV1Raw(V1R_MISS_NAME, &actual);
	emit_input_header();
	emit_param("STRING", "%s", V1R_MISS_NAME);
	emit_param("MyString", "%s", "");
	emit_output_actual_header();
	emit_param("Error Message", "%s", actual.Value());
	if(actual.IsEmpty()) {
		FAIL;
	}
	PASS;
}

static bool test_mf_v1r_error_invalid_delim() {
	emit_test("Test that MergeFromV1Raw() generates an error message for "
		"an invalid V1Raw string due to a missing delimiter.");
	emit_comment("This test just checks if the error message is not empty.");
	Env env;
	MyString actual;
	env.MergeFromV1Raw(V1R_MISS_DELIM, &actual);
	emit_input_header();
	emit_param("STRING", "%s", V1R_MISS_DELIM);
	emit_param("MyString", "%s", "");
	emit_output_actual_header();
	emit_param("Error Message", "%s", actual.Value());
	if(actual.IsEmpty()) {
		FAIL;
	}
	PASS;
}

static bool test_mf_v1r_add_null() {
	emit_test("Test that MergeFromV1Raw() doesn't add the environment "
		"variable for a NULL string.");
	Env env;
	MyString actual;
	env.MergeFromV1Raw(NULL, NULL);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("STRING", "%s", "NULL");
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_param("Env", "%s", EMPTY);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(actual != EMPTY) {
		FAIL;
	}
	PASS;
}

static bool test_mf_v1r_add_invalid() {
	emit_test("Test that MergeFromV1Raw() doesn't add the environment "
		"variables for an invalid V1Raw string.");
	Env env;
	MyString actual;
	env.MergeFromV1Raw(V1R_MISS_BOTH, NULL);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("STRING", "%s", V1R_MISS_BOTH);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_param("Env", "%s", EMPTY);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(actual != EMPTY) {
		FAIL;
	}
	PASS;
}

static bool test_mf_v1r_add() {
	emit_test("Test that MergeFromV1Raw() adds the environment variables "
		"for a valid V1Raw string.");
	Env env;
	MyString actual;
	env.MergeFromV1Raw(V1R, NULL);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("STRING", "%s", V1R);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_param("Env", "%s", V2R);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(!strings_similar(actual.Value(), V2R)) {
		FAIL;
	}
	PASS;
}

static bool test_mf_v1r_replace() {
	emit_test("Test that MergeFromV1Raw() replaces the environment "
		"variables for a valid V1Raw string.");
	Env env;
	MyString actual;
	env.MergeFromV1Raw(V1R, NULL);
	env.MergeFromV1Raw(V1R_REP, NULL);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("Env", "%s", V1R);
	emit_param("STRING", "%s", V1R_REP);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_param("Env", "%s", V2R_REP);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(!strings_similar(actual.Value(), V2R_REP)) {
		FAIL;
	}
	PASS;
}

static bool test_mf_v1r_replace_v2r() {
	emit_test("Test that MergeFromV1Raw() replaces the environment "
		"variables for a valid V1Raw string on an Env object originally "
		"constructed from a V2Raw string.");
	Env env;
	MyString actual;
	env.MergeFromV2Raw(V2R_SEMI, NULL);
	env.MergeFromV1Raw(V1R_REP, NULL);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("Env", "%s", V2R_SEMI);
	emit_param("STRING", "%s", V1R_REP);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_param("Env", "%s", V2R_REP_SEMI);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(!strings_similar(actual.Value(), V2R_REP_SEMI)) {
		FAIL;
	}
	PASS;
}

static bool test_mf_v1r_replace_v2q() {
	emit_test("Test that MergeFromV1Raw() replaces the environment "
		"variables for a valid V1Raw string on an Env object originally "
		"constructed from a V2Quoted string.");
	Env env;
	MyString actual;
	env.MergeFromV2Quoted(V2Q_SEMI, NULL);
	env.MergeFromV1Raw(V1R_REP, NULL);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("Env", "%s", V2Q_SEMI);
	emit_param("STRING", "%s", V1R_REP);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_param("Env", "%s", V2R_REP_SEMI);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(!strings_similar(actual.Value(), V2R_REP_SEMI)) {
		FAIL;
	}
	PASS;
}

static bool test_mf_v1r_replace_add() {
	emit_test("Test that MergeFromV1Raw() replaces some environment "
		"variables and also adds new ones for a valid V1Raw string.");
	Env env;
	MyString actual;
	env.MergeFromV1Raw(V1R, NULL);
	env.MergeFromV1Raw(V1R_REP_ADD, NULL);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("Env", "%s", V1R);
	emit_param("STRING", "%s", V1R_REP_ADD);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_param("Env", "%s", V2R_REP_ADD);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(!strings_similar(actual.Value(), V2R_REP_ADD)) {
		FAIL;
	}
	PASS;
}

static bool test_mf_v1r_replace_add_v2r() {
	emit_test("Test that MergeFromV1Raw() replaces some environment "
		"variables and also adds new ones for a valid V1Raw string on an Env "
		"object originally constructed from a V2Raw string.");
	Env env;
	MyString actual;
	env.MergeFromV2Raw(V2R_SEMI, NULL);
	env.MergeFromV1Raw(V1R_REP_ADD, NULL);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("Env", "%s", V2R_SEMI);
	emit_param("STRING", "%s", V1R_REP_ADD);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_param("Env", "%s", V2R_REP_ADD_SEMI);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(!strings_similar(actual.Value(), V2R_REP_ADD_SEMI)) {
		FAIL;
	}
	PASS;
}

static bool test_mf_v1r_replace_add_v2q() {
	emit_test("Test that MergeFromV1Raw() replaces some environment "
		"variables and also adds new ones for a valid V1Raw string on an Env "
		"object originally constructed from a V2Quoted string.");
	Env env;
	MyString actual;
	env.MergeFromV2Quoted(V2Q_SEMI, NULL);
	env.MergeFromV1Raw(V1R_REP_ADD, NULL);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("Env", "%s", V2Q_SEMI);
	emit_param("STRING", "%s", V1R_REP_ADD);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_param("Env", "%s", V2R_REP_ADD_SEMI);
	emit_output_actual_header();
	emit_param("Env after", "%s", actual.Value());
	if(!strings_similar(actual.Value(), V2R_REP_ADD_SEMI)) {
		FAIL;
	}
	PASS;
}

static bool test_mf_v1or2_r_ret_null() {
	emit_test("Test that MergeFromV1or2Raw() returns true when passed "
		"a NULL string.");
	Env env;
	bool expect = true;
	bool actual = env.MergeFromV1or2Raw(NULL, NULL);
	emit_input_header();
	emit_param("STRING", "%s", "NULL");
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_mf_v1or2_r_detect_v1r() {
	emit_test("Test that MergeFromV1or2Raw() correctly handles a V1Raw "
		"string by checking that InputWasV1() returns true.");
	emit_comment("MergeFromV1or2Raw() just calls MergeFromV1Raw(), which was "
		"tested above.");
	Env env;
	bool expect = true;
	env.MergeFromV1or2Raw(V1R, NULL);
	bool actual =env.InputWasV1();
	emit_input_header();
	emit_param("STRING", "%s", V1R);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_param("InputWasV1()", "%s", tfstr(expect));
	emit_output_actual_header();
	emit_param("InputWasV1()", "%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_mf_v1or2_r_detect_v2r() {
	emit_test("Test that MergeFromV1or2Raw() correctly handles a V1Raw "
		"string by checking that InputWasV1() returns true.");
	emit_comment("MergeFromV1or2Raw() just calls MergeFromV2Raw(), which was "
		"tested above.");
	Env env;
	bool expect = false;
	env.MergeFromV1or2Raw(V2R_MARK, NULL);
	bool actual =env.InputWasV1();
	emit_input_header();
	emit_param("STRING", "%s", V2R_MARK);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_param("InputWasV1()", "%s", tfstr(expect));
	emit_output_actual_header();
	emit_param("InputWasV1()", "%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_mf_v1or2_r_add_null() {
	emit_test("Test that MergeFromV1or2Raw() doesn't add any environment "
		"variables for a NULL string.");
	Env env;
	MyString actual;
	env.MergeFromV1or2Raw(NULL, NULL);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("STRING", "%s", "NULL");
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_param("Env", "%s", EMPTY);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(actual != EMPTY) {
		FAIL;
	}
	PASS;
}

static bool test_mf_v1or2_r_add_v2r() {
	emit_test("Test that MergeFromV1or2Raw() adds the environment variables "
		"for a valid V2Raw with the V2Raw environment marker.");
	emit_comment("We need to make sure MergeFromV2Raw() correctly handles the"
		" V2Raw environment marker.");
	Env env;
	MyString actual;
	env.MergeFromV1or2Raw(V2R_MARK, NULL);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("STRING", "%s", V2R_MARK);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_param("Env", "%s", V2R);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(!strings_similar(actual.Value(), V2R_SEMI)) {
		FAIL;
	}
	PASS;
}

static bool test_mf_str_array_ret_null() {
	emit_test("Test that MergeFrom() returns false when passed a NULL string "
		"array.");
	Env env;
	const char** str = NULL;
	bool expect = false;
	bool actual = env.MergeFrom(str);
	emit_input_header();
	emit_param("STRING ARRAY", "%s", "NULL");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_mf_str_array_ret_valid() {
	emit_test("Test that MergeFrom() returns true when passed a valid string "
		"array.");
	Env env;
	MyString temp;
	bool expect = true;
	bool actual = env.MergeFrom(ARRAY);
	env.getDelimitedStringForDisplay(&temp);
	emit_input_header();
	emit_param("STRING ARRAY", "%s", V2R);
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	emit_param("Env", "%s", temp.Value());
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_mf_str_array_ret_invalid_name() {
	emit_test("Test that MergeFrom() returns false when passed a invalid "
		"string array due to a missing variable name.");
	Env env;
	bool expect = false;
	bool actual = env.MergeFrom(ARRAY_MISS_NAME);
	emit_input_header();
	emit_param("STRING ARRAY", "%s", V2R_MISS_NAME);
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_mf_str_array_ret_invalid_delim() {
	emit_test("Test that MergeFrom() returns false when passed a invalid "
		"string array due to a missing delimiter.");
	Env env;
	bool expect = false;
	bool actual = env.MergeFrom(ARRAY_MISS_DELIM);
	emit_input_header();
	emit_param("STRING ARRAY", "%s", V2R_MISS_DELIM);
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_mf_str_array_add_null() {
	emit_test("Test that MergeFrom() doesn't add the environment variables "
		"for a NULL string array.");
	Env env;
	MyString actual;
	const char** str = NULL;
	env.MergeFrom(str);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("Env", "%s", "");
	emit_param("STRING ARRAY", "%s", "NULL");
	emit_output_expected_header();
	emit_param("Env", "%s", EMPTY);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(actual != EMPTY) {
		FAIL;
	}
	PASS;
}

static bool test_mf_str_array_add_invalid() {
	emit_test("Test that MergeFrom() skips invalid entries but adds valid "
		"entries for an invalid string array.");
	Env env;
	MyString actual;
	Env expected_env;
	MyString expected;
	env.MergeFrom(ARRAY_SKIP_BAD);
	env.getDelimitedStringForDisplay(&actual);
	expected_env.MergeFrom(ARRAY_SKIP_BAD_CLEAN);
	expected_env.getDelimitedStringForDisplay(&expected);
	emit_input_header();
	emit_param("Env", "%s", "");
	emit_param("STRING ARRAY", "%s", ARRAY_SKIP_BAD_STR);
	emit_output_expected_header();
	emit_param("Env", "%s", expected.Value());
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(actual != expected) {
		FAIL;
	}
	PASS;
}

static bool test_mf_str_array_add() {
	emit_test("Test that MergeFrom() adds the environment variables for a "
		"valid string array.");
	Env env;
	MyString actual;
	env.MergeFrom(ARRAY);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("Env", "%s", "");
	emit_param("STRING ARRAY", "%s", V2R);
	emit_output_expected_header();
	emit_param("Env", "%s", V2R);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(!strings_similar(&actual, &ADD)) {
		FAIL;
	}
	PASS;
}

static bool test_mf_str_array_replace() {
	emit_test("Test that MergeFrom() replaces the environment variables for "
		"a valid string array.");
	Env env;
	MyString actual;
	env.MergeFrom(ARRAY);
	env.MergeFrom(ARRAY_REP);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("Env", "%s", V2R);
	emit_param("STRING ARRAY", "%s", V2R_REP);
	emit_output_expected_header();
	emit_param("Env", "%s", V2R_REP);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(!strings_similar(&actual, &REP)) {
		FAIL;
	}
	PASS;
}

static bool test_mf_str_array_replace_add() {
	emit_test("Test that MergeFrom() replaces the values of some of the "
		"environment variables and also adds new ones for a valid string "
		"array.");
	Env env;
	MyString actual;
	env.MergeFrom(ARRAY);
	env.MergeFrom(ARRAY_REP_ADD);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("Env", "%s", V2R);
	emit_param("STRING ARRAY", "%s", V2R_REP_ADD); 
	emit_output_expected_header();
	emit_param("Env", "%s", V2R_REP_ADD);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(!strings_similar(&actual, &REP_ADD)) {
		FAIL;
	}
	PASS;
}

static bool test_mf_str_ret_null() {
	emit_test("Test that MergeFrom() returns false when passed a NULL "
		"string.");
	Env env;
	const char* str = NULL;
	bool expect = false;
	bool actual = env.MergeFrom(str);
	emit_input_header();
	emit_param("STRING", "%s", "NULL");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_mf_str_ret_valid() {
	emit_test("Test that MergeFrom() returns true when passed a valid NULL-"
		"delimited string.");
	Env env;
	bool expect = true;
	bool actual = env.MergeFrom(NULL_DELIM);
	emit_input_header();
	emit_param("STRING", "%s", V2R);
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_mf_str_ret_invalid_name() {
	emit_test("Test that MergeFrom() returns true when passed an invalid "
		"NULL-delimited string due to a missing variable name.");
	emit_comment("MergeFrom() will ignore errors from SetEnv() and insert "
		"what it can.");
	Env env;
	bool expect = true;
	bool actual = env.MergeFrom(NULL_DELIM_MISS_NAME);
	emit_input_header();
	emit_param("STRING", "%s", V2R_MISS_NAME);
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_mf_str_ret_invalid_delim() {
	emit_test("Test that MergeFrom() true when passed an invalid "
		"NULL-delimited string due to a missing delimiter.");
	emit_comment("MergeFrom() will ignore errors from SetEnv() and insert "
		"what it can.");
	Env env;
	bool expect = true;
	bool actual = env.MergeFrom(NULL_DELIM_MISS_DELIM);
	emit_input_header();
	emit_param("STRING", "%s", V2R_MISS_DELIM);
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_mf_str_add_null() {
	emit_test("Test that MergeFrom() doesn't add any environment variables "
		"for a NULL string.");
	Env env;
	MyString actual;
	const char* str = NULL;
	env.MergeFrom(str);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("STRING", "%s", "NULL");
	emit_output_expected_header();
	emit_param("Env", "%s", EMPTY);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(actual != EMPTY) {
		FAIL;
	}
	PASS;
}

static bool test_mf_str_add_invalid_name() {
	emit_test("Test that MergeFrom() doesn't add the environment variable "
		"with a missing variable name, but still adds the valid variables.");
	emit_comment("MergeFrom() will ignore errors from SetEnv() and insert "
		"what it can.");
	Env env;
	MyString actual;
	env.MergeFrom(NULL_DELIM_MISS_NAME);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("STRING", "%s", V2R_MISS_NAME);
	emit_output_expected_header();
	emit_param("Env", "%s", "two=2 three=3");
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(!strings_similar(actual.Value(), "two=2 three=3")) {
		FAIL;
	}
	PASS;
}

static bool test_mf_str_add_invalid_delim() {
	emit_test("Test that MergeFrom() doesn't add the environment variable "
		"with a missing delimiter, but still adds the valid variables.");
	emit_comment("MergeFrom() will ignore errors from SetEnv() and insert "
		"what it can.");
	Env env;
	MyString actual;
	env.MergeFrom(NULL_DELIM_MISS_DELIM);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("STRING", "%s", V2R_MISS_DELIM);
	emit_output_expected_header();
	emit_param("Env", "%s", "two=2 three=3");
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(!strings_similar(actual.Value(), "two=2 three=3")) {
		FAIL;
	}
	PASS;
}

static bool test_mf_str_add() {
	emit_test("Test that MergeFrom() adds the environment variables for a "
		"valid NULL-delimited string.");
	Env env;
	MyString actual;
	env.MergeFrom(NULL_DELIM);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("STRING", "%s", V2R);
	emit_output_expected_header();
	emit_param("Env", "%s", V2R);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(!strings_similar(actual.Value(), V2R)) {
		FAIL;
	}
	PASS;
}

static bool test_mf_str_replace() {
	emit_test("Test that MergeFrom() replaces the environment variables for a"
		" valid NULL-delimited string.");
	Env env;
	MyString actual;
	env.MergeFrom(NULL_DELIM);
	env.MergeFrom(NULL_DELIM_REP);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("STRING", "%s", V2R);
	emit_param("STRING", "%s", V2R_REP);
	emit_output_expected_header();
	emit_param("Env", "%s", V2R_REP);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(!strings_similar(actual.Value(), V2R_REP)) {
		FAIL;
	}
	PASS;
}

static bool test_mf_str_replace_add() {
	emit_test("Test that MergeFrom() replaces and adds environment variables "
		"for a valid NULL-delimited string.");
	Env env;
	MyString actual;
	env.MergeFrom(NULL_DELIM);
	env.MergeFrom(NULL_DELIM_REP_ADD);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("STRING", "%s", V2R);
	emit_param("STRING", "%s", V2R_REP_ADD);
	emit_output_expected_header();
	emit_param("Env", "%s", V2R_REP);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(!strings_similar(actual.Value(), V2R_REP_ADD)) {
		FAIL;
	}
	PASS;
}

static bool test_mf_env_add_empty() {
	emit_test("Test that MergeFrom() doesn't add the environment variables "
		"when passed an empty Env.");
	Env env1, env2;
	MyString actual;
	env1.MergeFrom(env2);
	env1.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("Env", "%s", "");
	emit_param("Env Parameter", "%s", "");
	emit_output_expected_header();
	emit_param("Env", "%s", EMPTY);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(actual != EMPTY) {
		FAIL;
	}
	PASS;
}

static bool test_mf_env_add_one() {
	emit_test("Test that MergeFrom() adds the environment variables when "
		"passed an Env with one variable.");
	Env env1, env2;
	MyString actual;
	env2.MergeFromV2Raw(ONE, NULL);
	env1.MergeFrom(env2);
	env1.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("Env", "%s", "");
	emit_param("Env Parameter", "%s", ONE);
	emit_output_expected_header();
	emit_param("Env", "%s", ONE);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(actual != ONE) {
		FAIL;
	}
	PASS;
}

static bool test_mf_env_add_many() {
	emit_test("Test that MergeFrom() adds the environment variables when "
		"passed an Env with many variables.");
	Env env1, env2;
	MyString actual;
	env2.MergeFromV2Raw(V2R, NULL);
	env1.MergeFrom(env2);
	env1.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("Env", "%s", "");
	emit_param("Env Parameter", "%s", V2R);
	emit_output_expected_header();
	emit_param("Env", "%s", V2R);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(!strings_similar(actual.Value(), V2R)) {
		FAIL;
	}
	PASS;
}

static bool test_mf_env_replace() {
	emit_test("Test that MergeFrom() replaces the environment variables when "
		"passed an Env with many variables.");
	Env env1, env2;
	MyString actual;
	env1.MergeFromV2Raw(V2R, NULL);
	env2.MergeFromV2Raw(V2R_REP, NULL);
	env1.MergeFrom(env2);
	env1.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("Env", "%s", V2R);
	emit_param("Env Parameter", "%s", V2R_REP);
	emit_output_expected_header();
	emit_param("Env", "%s", V2R_REP);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(!strings_similar(actual.Value(), V2R_REP)) {
		FAIL;
	}
	PASS;
}

static bool test_mf_env_replace_v1_v2() {
	emit_test("Test that MergeFrom() replaces the environment variables on an"
		" Env object originally constructed from a V1Raw string when passed an "
		"Env constructed from a V2Raw string.");
	Env env1, env2;
	MyString actual;
	env1.MergeFromV1Raw(V1R, NULL);
	env2.MergeFromV2Raw(V2R_REP_SEMI, NULL);
	env1.MergeFrom(env2);
	env1.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("Env", "%s", V1R);
	emit_param("Env Parameter", "%s", V2R_REP_SEMI);
	emit_output_expected_header();
	emit_param("Env", "%s", V2R_REP_SEMI);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(!strings_similar(actual.Value(), V2R_REP_SEMI)) {
		FAIL;
	}
	PASS;
}

static bool test_mf_env_replace_v2_v1() {
	emit_test("Test that MergeFrom() replaces the environment variables on an"
		" Env object originally constructed from a V2Raw string when passed an "
		"Env constructed from a V1Raw string.");
	Env env1, env2;
	MyString actual;
	env1.MergeFromV2Raw(V2R_SEMI, NULL);
	env2.MergeFromV1Raw(V1R_REP, NULL);
	env1.MergeFrom(env2);
	env1.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("Env", "%s", V2R_SEMI);
	emit_param("Env Parameter", "%s", V1R_REP);
	emit_output_expected_header();
	emit_param("Env", "%s", V1R_REP);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(!strings_similar(actual.Value(), V2R_REP_SEMI)) {
		FAIL;
	}
	PASS;
}

static bool test_mf_env_replace_add() {
	emit_test("Test that MergeFrom() replaces some of the environment "
		"variables and adds new ones when passed an Env with many variables.");
	Env env1, env2;
	MyString actual;
	MyString rep;
	env1.MergeFromV2Raw(V2R, NULL);
	env2.MergeFromV2Raw(V2R_REP_ADD, NULL);
	env1.MergeFrom(env2);
	env1.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("Env", "%s", V2R);
	emit_param("Env Parameter", "%s", V2R_REP_ADD);
	emit_output_expected_header();
	emit_param("Env", "%s", V2R_REP);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(!strings_similar(actual.Value(), V2R_REP_ADD)) {
		FAIL;
	}
	PASS;
}

static bool test_mf_env_replace_add_v1_v2() {
	emit_test("Test that MergeFrom() replaces some of the environment "
		"variables and adds new ones on an Env object originally constructed "
		"from a V1Raw string when passed an Env constructed from a V2Raw "
		"string.");
	Env env1, env2;
	MyString actual;
	env1.MergeFromV1Raw(V1R, NULL);
	env2.MergeFromV2Raw(V2R_REP_ADD_SEMI, NULL);
	env1.MergeFrom(env2);
	env1.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("Env", "%s", V1R);
	emit_param("Env Parameter", "%s", V2R_REP_ADD_SEMI);
	emit_output_expected_header();
	emit_param("Env", "%s", V2R_REP_ADD_SEMI);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(!strings_similar(actual.Value(), V2R_REP_ADD_SEMI)) {
		FAIL;
	}
	PASS;
}

static bool test_mf_env_replace_add_v2_v1() {
	emit_test("Test that MergeFrom() replaces some of the environment "
		"variables and adds new ones on an Env object originally constructed "
		"from a V2Raw string when passed an Env constructed from a V1Raw "
		"string.");
	Env env1, env2;
	MyString actual;
	env1.MergeFromV2Raw(V2R_SEMI, NULL);
	env2.MergeFromV1Raw(V1R_REP_ADD, NULL);
	env1.MergeFrom(env2);
	env1.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("Env", "%s", V2R_SEMI);
	emit_param("Env Parameter", "%s", V1R_REP_ADD);
	emit_output_expected_header();
	emit_param("Env", "%s", V2R_REP_ADD_SEMI);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(!strings_similar(actual.Value(), V2R_REP_ADD_SEMI)) {
		FAIL;
	}
	PASS;
}


static bool test_mf_env_itself() {
	emit_test("Test that MergeFrom() doesn't add the environment variables "
		"when passed itself.");
	Env env;
	MyString actual;
	env.MergeFromV2Raw(V2R, NULL);
	env.MergeFrom(env);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("Env", "%s", V2R);
	emit_param("Env Parameter", "%s", V2R);
	emit_output_expected_header();
	emit_param("Env", "%s", V2R);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(!strings_similar(actual.Value(), V2R)) {
		FAIL;
	}
	PASS;
}

static bool test_mf_ad_ret_null() {
	emit_test("Test that MergeFrom() returns true when passed a NULL "
		"ClassAd.");
	Env env;
	bool expect = true;
	bool actual = env.MergeFrom(NULL, NULL);
	emit_input_header();
	emit_param("ClassAd", "%s", "NULL");
	emit_param("MyString", "%s", "NULL");
	emit_param("CONSTANT", "%s", ATTR_JOB_ENVIRONMENT2);
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_mf_ad_ret_v1r_valid() {
	emit_test("Test that MergeFrom() returns true when passed a valid "
		"ClassAd that uses V1Raw.");
	Env env;
	ClassAd classad;
	initAdFromString(AD_V1, classad);
	bool expect = true;
	bool actual = env.MergeFrom(&classad, NULL);
	emit_input_header();
	emit_param("ClassAd", "%s", AD_V1);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_mf_ad_ret_v2r_valid() {
	emit_test("Test that MergeFrom() returns true when passed a valid "
		"ClassAd that uses V2Raw.");
	Env env;
	ClassAd classad;
	initAdFromString(AD_V2, classad);
	bool expect = true;
	bool actual = env.MergeFrom(&classad, NULL);
	emit_input_header();
	emit_param("ClassAd", "%s", AD_V2);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_mf_ad_ret_valid_define() {
	emit_test("Test that MergeFrom() returns true when passed a valid "
		"ClassAd that doesn't define an Environment");
	Env env;
	ClassAd classad;
	initAdFromString(AD, classad);
	bool expect = true;
	bool actual = env.MergeFrom(&classad, NULL);
	emit_input_header();
	emit_param("ClassAd", "%s", AD);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;	
	}
	PASS;
}

static bool test_mf_ad_ret_v1r_invalid_name() {
	emit_test("Test that MergeFrom() returns false when passed an invalid "
		"ClassAd that uses V1Raw due to a missing variable name.");
	Env env;
	ClassAd classad;
	initAdFromString(AD_V1_MISS_NAME, classad);
	bool expect = false;
	bool actual = env.MergeFrom(&classad, NULL);
	emit_input_header();
	emit_param("ClassAd", "%s", AD_V1_MISS_NAME);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_mf_ad_ret_v1r_invalid_delim() {
	emit_test("Test that MergeFrom() returns false when passed an invalid "
		"ClassAd that uses V1Raw due to a missing delimiter.");
	Env env;
	ClassAd classad;
	initAdFromString(AD_V1_MISS_DELIM, classad);
	bool expect = false;
	bool actual = env.MergeFrom(&classad, NULL);
	emit_input_header();
	emit_param("ClassAd", "%s", AD_V1_MISS_DELIM);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_mf_ad_ret_v2r_invalid_name() {
	emit_test("Test that MergeFrom() returns false when passed a invalid "
		"ClassAd that uses V2Raw due to a missing variable name.");
	Env env;
	ClassAd classad;
	initAdFromString(AD_V2_MISS_NAME, classad);
	bool expect = false;
	bool actual = env.MergeFrom(&classad, NULL);
	emit_input_header();
	emit_param("ClassAd", "%s", AD_V2_MISS_NAME);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_mf_ad_ret_v2r_invalid_delim() {
	emit_test("Test that MergeFrom() returns false when passed a invalid "
		"ClassAd that uses V2Raw due to a missing delimiter.");
	Env env;
	ClassAd classad;
	initAdFromString(AD_V2_MISS_DELIM, classad);
	bool expect = false;
	bool actual = env.MergeFrom(&classad, NULL);
	emit_input_header();
	emit_param("ClassAd", "%s", AD_V2_MISS_DELIM);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_mf_ad_error_v1r_invalid_name() {
	emit_test("Test that MergeFrom() generates an error message for an "
		"invalid ClassAd that uses V1Raw due to a missing variable name.");
	emit_comment("This test just checks if the error message is not empty.");
	Env env;
	MyString actual;
	ClassAd classad;
	initAdFromString(AD_V1_MISS_NAME, classad);
	env.MergeFrom(&classad, &actual);
	emit_input_header();
	emit_param("ClassAd", "%s", AD_V1_MISS_NAME);
	emit_param("MyString", "%s", "");
	emit_output_actual_header();
	emit_param("Error Message", "%s", actual.Value());
	if(actual.IsEmpty()) {
		FAIL;
	}
	PASS;
}

static bool test_mf_ad_error_v1r_invalid_delim() {
	emit_test("Test that MergeFrom() generates an error message for an "
		"invalid ClassAd that uses V1Raw due to a missing delimiter.");
	emit_comment("This test just checks if the error message is not empty.");
	Env env;
	MyString actual;
	ClassAd classad;
	initAdFromString(AD_V1_MISS_DELIM, classad);
	env.MergeFrom(&classad, &actual);
	emit_input_header();
	emit_param("ClassAd", "%s", AD_V1_MISS_DELIM);
	emit_param("MyString", "%s", "");
	emit_output_actual_header();
	emit_param("Error Message", "%s", actual.Value());
	if(actual.IsEmpty()) {
		FAIL;
	}
	PASS;
}

static bool test_mf_ad_error_v2r_invalid_name() {
	emit_test("Test that MergeFrom() generates an error message for an "
		"invalid ClassAd that uses V2Raw due to a missing variable name.");
	emit_comment("This test just checks if the error message is not empty.");
	Env env;
	MyString actual;
	ClassAd classad;
	initAdFromString(AD_V2_MISS_NAME, classad);
	env.MergeFrom(&classad, &actual);
	emit_input_header();
	emit_param("ClassAd", "%s", AD_V2_MISS_NAME);
	emit_param("MyString", "%s", "");
	emit_output_actual_header();
	emit_param("Error Message", "%s", actual.Value());
	if(actual.IsEmpty()) {
		FAIL;
	}
	PASS;
}

static bool test_mf_ad_error_v2r_invalid_delim() {
	emit_test("Test that MergeFrom() generates an error message for an "
		"invalid ClassAd that uses V2Raw due to a missing delimiter.");
	emit_comment("This test just checks if the error message is not empty.");
	Env env;
	MyString actual;
	ClassAd classad;
	initAdFromString(AD_V2_MISS_NAME, classad);
	env.MergeFrom(&classad, &actual);
	emit_input_header();
	emit_param("ClassAd", "%s", AD_V2_MISS_NAME);
	emit_param("MyString", "%s", "");
	emit_output_actual_header();
	emit_param("Error Message", "%s", actual.Value());
	if(actual.IsEmpty()) {
		FAIL;
	}
	PASS;
}

static bool test_mf_ad_add_null() {
	emit_test("Test that MergeFrom() doesn't add the environment variables "
		"when passed a NULL ClassAd pointer.");
	Env env;
	MyString actual;
	env.MergeFrom(NULL, NULL);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("ClassAd", "%s", "NULL");
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_param("Env", "%s", EMPTY);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(actual != EMPTY) {
		FAIL;
	}
	PASS;
}

static bool test_mf_ad_add_define() {
	emit_test("Test that MergeFrom() doesn't any environment variables "
		"when passed a ClassAd that doesn't define an environment variable.");
	Env env;
	ClassAd classad;
	initAdFromString(AD, classad);
	MyString actual;
	env.MergeFrom(&classad, NULL);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("ClassAd", "%s", AD);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_param("Env", "%s", EMPTY);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(actual != EMPTY) {
		FAIL;
	}
	PASS;
}

static bool test_mf_ad_add_v1r_one() {
	emit_test("Test that MergeFrom() adds the environment variables when "
		"passed a valid ClassAd that uses V1Raw with one variable.");
	Env env;
	const char* classad_string = "\tEnv = \"one=1\"";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	MyString actual;
	env.MergeFrom(&classad, NULL);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("ClassAd", "%s", classad_string);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_param("Env", "%s", ONE);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(actual != ONE) {
		FAIL;
	}
	PASS;
}

static bool test_mf_ad_add_v1r_many() {
	emit_test("Test that MergeFrom() adds the environment variables when "
		"passed a valid ClassAd that uses V1Raw with many variables.");
	Env env;
	ClassAd classad;
	initAdFromString(AD_V1, classad);
	MyString actual;
	env.MergeFrom(&classad, NULL);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("ClassAd", "%s", AD_V1);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_param("Env", "%s", V2R);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(!strings_similar(actual.Value(), V2R)) {
		FAIL;
	}
	PASS;
}

static bool test_mf_ad_add_v2r_one() {
	emit_test("Test that MergeFrom() adds the environment variables when "
		"passed a valid ClassAd that uses V2Raw with one variable.");
	Env env;
	const char* classad_string = "\tEnvironment = \"one=1\"";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	MyString actual;
	env.MergeFrom(&classad, NULL);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("ClassAd", "%s", classad_string);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_param("Env", "%s", ONE);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(actual != ONE) {
		FAIL;
	}
	PASS;
}

static bool test_mf_ad_add_v2r_many() {
	emit_test("Test that MergeFrom() adds the environment variables when "
		"passed a valid ClassAd that uses V2Raw with many variables.");
	Env env;
	ClassAd classad;
	initAdFromString(AD_V2, classad);
	MyString actual;
	env.MergeFrom(&classad, NULL);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("ClassAd", "%s", AD_V2);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_param("Env", "%s", V2R);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(!strings_similar(actual.Value(), V2R)) {
		FAIL;
	}
	PASS;
}

static bool test_mf_ad_v1r_replace() {
	emit_test("Test that MergeFrom() replaces the environment variables when "
		"passed a valid ClassAd that uses V1Raw with many variables.");
	Env env;
	ClassAd classad;
	initAdFromString(AD_V1_REP, classad);
	MyString actual;
	env.MergeFromV2Raw(V2R, NULL);
	env.MergeFrom(&classad, NULL);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("Env", "%s", V2R);
	emit_param("ClassAd", "%s", AD_V1_REP);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_param("Env", "%s", V2R_REP);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(!strings_similar(actual.Value(), V2R_REP)) {
		FAIL;
	}
	PASS;
}

static bool test_mf_ad_v2r_replace() {
	emit_test("Test that MergeFrom() replaces the environment variables when "
		"passed a valid ClassAd that uses V2Raw with many variables.");
	Env env;
	ClassAd classad;
	initAdFromString(AD_V2_REP, classad);
	MyString actual;
	env.MergeFromV2Raw(V2R, NULL);
	env.MergeFrom(&classad, NULL);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("Env", "%s", V2R);
	emit_param("ClassAd", "%s", AD_V2_REP);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_param("Env", "%s", V2R_REP);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(!strings_similar(actual.Value(), V2R_REP)) {
		FAIL;
	}
	PASS;
}

static bool test_mf_ad_v1r_replace_add() {
	emit_test("Test that MergeFrom() replaces some of the environment "
		"variables and adds new ones when passed a valid ClassAd that uses"
		" V1Raw with many variables.");
	Env env;
	ClassAd classad;
	initAdFromString(AD_V1_REP_ADD, classad);
	MyString actual;
	env.MergeFromV2Raw(V2R, NULL);
	env.MergeFrom(&classad, NULL);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("Env", "%s", V2R);
	emit_param("ClassAd", "%s", AD_V1_REP_ADD);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_param("Env", "%s", V2R_REP_ADD);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(!strings_similar(actual.Value(), V2R_REP_ADD)) {
		FAIL;
	}
	PASS;
}

static bool test_mf_ad_v2r_replace_add() {
	emit_test("Test that MergeFrom() replaces some of the environment "
		"variables when and adds new ones when passed a valid ClassAd that uses"
		" V2Raw with many variables.");
	Env env;
	ClassAd classad;
	initAdFromString(AD_V2_REP_ADD, classad);
	MyString actual;
	env.MergeFromV2Raw(V2R, NULL);
	env.MergeFrom(&classad, NULL);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("Env", "%s", V2R);
	emit_param("ClassAd", "%s", AD_V2_REP_ADD);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_param("Env", "%s", V2R_REP_ADD);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(!strings_similar(actual.Value(), V2R_REP_ADD)) {
		FAIL;
	}
	PASS;
}

static bool test_set_env_with_error_message_ret_null() {
	emit_test("Test that SetEnvWithErrorMessage() returns false when passed a"
		" NULL string.");
	Env env;
	bool expect = false;
	bool actual = env.SetEnvWithErrorMessage(NULL, NULL);
	emit_input_header();
	emit_param("STRING", "%s", "NULL");
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_set_env_with_error_message_ret_valid() {
	emit_test("Test that SetEnvWithErrorMessage() returns true when passed a"
		" valid string.");
	Env env;
	bool expect = true;
	bool actual = env.SetEnvWithErrorMessage(ONE, NULL);
	emit_input_header();
	emit_param("STRING", "%s", ONE);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_set_env_with_error_message_ret_invalid_name() {
	emit_test("Test that SetEnvWithErrorMessage() returns false when passed "
		"an invalid string due to a missing variable name.");
	Env env;
	bool expect = false;
	bool actual = env.SetEnvWithErrorMessage(ONE_MISS_NAME, NULL);
	emit_input_header();
	emit_param("STRING", "%s", ONE_MISS_NAME);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_set_env_with_error_message_ret_invalid_delim() {
	emit_test("Test that SetEnvWithErrorMessage() returns false when passed "
		"an invalid string due to a missing delimiter.");
	Env env;
	bool expect = false;
	bool actual = env.SetEnvWithErrorMessage(ONE_MISS_DELIM, NULL);
	emit_input_header();
	emit_param("STRING", "%s", ONE_MISS_DELIM);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_set_env_with_error_message_err_invalid_name() {
	emit_test("Test that SetEnvWithErrorMessage() generates an error message "
		"for an invalid string due to a missing variable name.");
	emit_comment("This test just checks if the error message is not empty.");
	Env env;
	MyString actual;
	bool retval;
	retval = env.SetEnvWithErrorMessage(ONE_MISS_NAME, &actual);
	emit_input_header();
	emit_param("STRING", "%s", ONE_MISS_NAME);
	emit_param("MyString", "%s", "");
	emit_output_actual_header();
	emit_param("Error Message", "%s", actual.Value());
	emit_param("Return Value", "%s", retval ? "true" : "false");
	if(actual.IsEmpty() || retval) {
		FAIL;
	}
	PASS;
}

static bool test_set_env_with_error_message_err_invalid_delim() {
	emit_test("Test that SetEnvWithErrorMessage() generates an error message "
		"for an invalid string due to a missing delimiter.");
	emit_comment("This test just checks if the error message is not empty.");
	Env env;
	MyString actual;
	bool retval;
	retval = env.SetEnvWithErrorMessage(ONE_MISS_DELIM, &actual);
	emit_input_header();
	emit_param("STRING", "%s", ONE_MISS_DELIM);
	emit_param("MyString", "%s", "");
	emit_output_actual_header();
	emit_param("Error Message", "%s", actual.Value());
	emit_param("Return Value", "%s", retval ? "true" : "false");
	if(actual.IsEmpty() || retval) {
		FAIL;
	}
	PASS;
}

static bool test_set_env_with_error_message_add_null() {
	emit_test("Test that SetEnvWithErrorMessage() doesn't add the environment"
		" variable when passed a NULL string.");
	Env env;
	MyString actual;
	env.SetEnvWithErrorMessage(NULL, NULL);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("Env", "%s", "");
	emit_param("STRING", "%s", "NULL");
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_param("Env", "%s", EMPTY);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(actual != EMPTY) {
		FAIL;
	}
	PASS;
}

static bool test_set_env_with_error_message_add_invalid_delim() {
	emit_test("Test that SetEnvWithErrorMessage() doesn't adds any "
		"environment variables when passed a invalid string due to a missing "
		"delimiter.");
	Env env;
	MyString actual;
	env.SetEnvWithErrorMessage(ONE_MISS_DELIM, NULL);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("Env", "%s", "");
	emit_param("STRING", "%s", ONE_MISS_DELIM);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_param("Env", "%s", EMPTY);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(actual != EMPTY) {
		FAIL;
	}
	PASS;
}

static bool test_set_env_with_error_message_add_invalid_var() {
	emit_test("Test that SetEnvWithErrorMessage() doesn't adds any "
		"environment variables when passed an invalid string due to a missing "
		"variable name.");
	Env env;
	MyString actual, expect;
	env.SetEnvWithErrorMessage(ONE_MISS_NAME, NULL);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("Env", "%s", "");
	emit_param("STRING", "%s", ONE_MISS_NAME);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_param("Env", "%s", EMPTY);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_set_env_with_error_message_add() {
	emit_test("Test that SetEnvWithErrorMessage() adds the environment "
		"variable when passed a valid string.");
	Env env;
	MyString actual;
	env.SetEnvWithErrorMessage(ONE, NULL);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("Env", "%s", "");
	emit_param("STRING", "%s", ONE);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_param("Env", "%s", ONE);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(actual != ONE) {
		FAIL;
	}
	PASS;
}

static bool test_set_env_with_error_message_replace() {
	emit_test("Test that SetEnvWithErrorMessage() replaces the environment "
		"variable when passed a valid string.");
	Env env;
	MyString actual;
	env.SetEnvWithErrorMessage(ONE, NULL);
	env.SetEnvWithErrorMessage(ONE_REP, NULL);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("Env", "%s", ONE);
	emit_param("STRING", "%s", ONE_REP);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_param("Env", "%s", ONE_REP);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(actual != ONE_REP) {
		FAIL;
	}
	PASS;
}

static bool test_set_env_str_ret_null() {
	emit_test("Test that SetEnv() returns false when passed a NULL string.");
	Env env;
	bool expect = false;
	bool actual = env.SetEnv(NULL);
	emit_input_header();
	emit_param("STRING", "%s", "NULL");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_set_env_str_ret_valid() {
	emit_test("Test that SetEnv() returns true when passed a valid string.");
	Env env;
	bool expect = true;
	bool actual = env.SetEnv(ONE);
	emit_input_header();
	emit_param("STRING", "%s", ONE);
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_set_env_str_ret_invalid_name() {
	emit_test("Test that SetEnv() returns false when passed an invalid string"
		" due to a missing variable name.");
	Env env;
	bool expect = false;
	bool actual = env.SetEnv(ONE_MISS_NAME);
	emit_input_header();
	emit_param("STRING", "%s", ONE_MISS_NAME);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_set_env_str_ret_invalid_delim() {
	emit_test("Test that SetEnv() returns false when passed an invalid string"
		" due to a missing delimiter.");
	Env env;
	bool expect = false;
	bool actual = env.SetEnv(ONE_MISS_DELIM);
	emit_input_header();
	emit_param("STRING", "%s", ONE_MISS_DELIM);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_set_env_str_add_null() {
	emit_test("Test that SetEnv() doesn't add any environment variables when "
		"passed a NULL string.");
	Env env;
	MyString actual, expect;
	env.SetEnv(NULL);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("Env", "%s", "");
	emit_param("STRING", "%s", "NULL");
	emit_output_expected_header();
	emit_param("Env", "%s", EMPTY);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_set_env_str_add_invalid() {
	emit_test("Test that SetEnv() doesn't add the environment variables when "
		"passed invalid strings.");
	Env env;
	MyString actual;
	env.SetEnv(ONE_MISS_NAME);
	env.SetEnv(ONE_MISS_DELIM);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("Env", "%s", "");
	emit_param("String 1", "%s", ONE_MISS_NAME);
	emit_param("String 2", "%s", ONE_MISS_DELIM);
	emit_output_expected_header();
	emit_param("Env", "%s", EMPTY);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(actual != EMPTY) {
		FAIL;
	}
	PASS;
}

static bool test_set_env_str_add() {
	emit_test("Test that SetEnv() adds the environment variables when passed "
		"a valid string.");
	Env env;
	MyString actual;
	env.SetEnv(ONE);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("Env", "%s", "");
	emit_param("STRING", "%s", ONE);
	emit_output_expected_header();
	emit_param("Env", "%s", ONE);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(actual != ONE) {
		FAIL;
	}
	PASS;
}

static bool test_set_env_str_replace() {
	emit_test("Test that SetEnv() replaces the environment variables when "
		"passed a valid string.");
	Env env;
	MyString actual;
	env.SetEnv(ONE);
	env.SetEnv(ONE_REP);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("Env", "%s", ONE);
	emit_param("STRING", "%s", ONE_REP);
	emit_output_expected_header();
	emit_param("Env", "%s", ONE_REP);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(actual != ONE_REP) {
		FAIL;
	}
	PASS;
}

static bool test_set_env_str_str_ret_null_var() {
	emit_test("Test that SetEnv() returns false when passed a NULL string for"
		" the variable name.");
	Env env;
	bool expect = false;
	bool actual = env.SetEnv(NULL, "1");
	emit_input_header();
	emit_param("STRING", "%s", "NULL");
	emit_param("STRING", "%s", "1");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_set_env_str_str_ret_null_val() {
	emit_test("Test that SetEnv() returns true when passed a NULL string for"
		" the variable value.");
	Env env;
	bool expect = true;
	bool actual = env.SetEnv("one", NULL);
	emit_input_header();
	emit_param("STRING", "%s", "one");
	emit_param("STRING", "%s", "NULL");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_set_env_str_str_ret_valid() {
	emit_test("Test that SetEnv() returns true when passed a valid string for"
		" both the variable name and value.");
	Env env;
	bool expect = true;
	bool actual = env.SetEnv("one", "1");
	emit_input_header();
	emit_param("STRING", "%s", "one");
	emit_param("STRING", "%s", "1");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_set_env_str_str_add_null_var() {
	emit_test("Test that SetEnv() doesn't add any environment variables when "
		"passed a NULL string for the variable name.");
	Env env;
	MyString actual;
	env.SetEnv(NULL, "1");
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("Env", "%s", "");
	emit_param("STRING", "%s", "NULL");
	emit_param("STRING", "%s", "1");
	emit_output_expected_header();
	emit_param("Env", "%s", EMPTY);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(actual != EMPTY) {
		FAIL;
	}
	PASS;
}

static bool test_set_env_str_str_add_null_val() {
	emit_test("Test that SetEnv() adds the environment variables when passed "
		"a NULL string for the variable value.");
	Env env;
	MyString actual;
	env.SetEnv("one", NULL);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("Env", "%s", "");
	emit_param("STRING", "%s", "one");
	emit_param("STRING", "%s", "NULL");
	emit_output_expected_header();
	emit_param("Env", "%s", ONE_MISS_VAL);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(actual != ONE_MISS_VAL) {
		FAIL;
	}
	PASS;
}

static bool test_set_env_str_str_add() {
	emit_test("Test that SetEnv() adds the environment variables when passed "
		"a valid string for both the variable name and value.");
	Env env;
	MyString actual;
	env.SetEnv("one", "1");
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("Env", "%s", "");
	emit_param("STRING", "%s", "one");
	emit_param("STRING", "%s", "1");
	emit_output_expected_header();
	emit_param("Env", "%s", ONE);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(actual != ONE) {
		FAIL;
	}
	PASS;
}

static bool test_set_env_str_str_replace() {
	emit_test("Test that SetEnv() replaces the environment variables when "
		"passed a valid string for both the variable name and value.");
	Env env;
	MyString actual;
	env.SetEnv("one", "1");
	env.SetEnv("one", "10");
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("Env", "%s", ONE);
	emit_param("STRING", "%s", "one");
	emit_param("STRING", "%s", "10");
	emit_output_expected_header();
	emit_param("Env", "%s", ONE_REP);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(actual != ONE_REP) {
		FAIL;
	}
	PASS;
}

static bool test_set_env_mystr_ret_empty_var() {
	emit_test("Test that SetEnv() returns false when passed an empty MyString"
		" for the variable name.");
	Env env;
	MyString str2("1");
	bool expect = false;
	bool actual = env.SetEnv(EMPTY, str2);
	emit_input_header();
	emit_param("STRING", "%s", EMPTY);
	emit_param("STRING", "%s", str2.Value());
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_set_env_mystr_ret_empty_val() {
	emit_test("Test that SetEnv() returns false when passed an empty MyString"
		" for the variable value.");
	Env env;
	MyString str1("one");
	bool expect = true;
	bool actual = env.SetEnv(str1, EMPTY);
	emit_input_header();
	emit_param("MyString", "%s", str1.Value());
	emit_param("MyString", "%s", EMPTY);
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_set_env_mystr_ret_valid() {
	emit_test("Test that SetEnv() returns true when passed a valid MyString"
		" for both the variable name and value.");
	Env env;
	MyString str1("one"), str2("1");
	bool expect = true;
	bool actual = env.SetEnv(str1, str2);
	emit_input_header();
	emit_param("MyString", "%s", str1.Value());
	emit_param("MyString", "%s", str2.Value());
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_set_env_mystr_add_empty_var() {
	emit_test("Test that SetEnv() doesn't add any environment variables when "
		"passed an empty MyString for the variable name.");
	Env env;
	MyString str2("1"), actual;
	env.SetEnv(EMPTY, str2);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("Env", "%s", "");
	emit_param("MyString", "%s", EMPTY);
	emit_param("MyString", "%s", str2.Value());
	emit_output_expected_header();
	emit_param("Env", "%s", EMPTY);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(actual != EMPTY) {
		FAIL;
	}
	PASS;
}

static bool test_set_env_mystr_add_empty_val() {
	emit_test("Test that SetEnv() adds the environment variables when passed "
		"an empty MyString for the variable name.");
	Env env;
	MyString str1("one"), actual;
	env.SetEnv(str1, EMPTY);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("Env", "%s", "");
	emit_param("MyString", "%s", str1.Value());
	emit_param("MyString", "%s", EMPTY);
	emit_output_expected_header();
	emit_param("Env", "%s", ONE_MISS_VAL);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(actual != ONE_MISS_VAL) {
		FAIL;
	}
	PASS;
}

static bool test_set_env_mystr_add() {
	emit_test("Test that SetEnv() adds the environment variables when passed "
		"a valid MyString for both the variable name and value.");
	Env env;
	MyString str1("one"), str2("1"), actual;
	env.SetEnv(str1, str2);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("Env", "%s", "");
	emit_param("MyString", "%s", str1.Value());
	emit_param("MyString", "%s", str2.Value());
	emit_output_expected_header();
	emit_param("Env", "%s", ONE);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(actual != ONE) {
		FAIL;
	}
	PASS;
}

static bool test_set_env_mystr_replace() {
	emit_test("Test that SetEnv() replaces the environment variables when "
		"passed a valid MyString for both the variable name and value.");
	Env env;
	MyString str1("one"), str2("10"), actual;
	env.SetEnv("one", "1");
	env.SetEnv(str1, str2);
	env.getDelimitedStringForDisplay(&actual);
	emit_input_header();
	emit_param("Env", "%s", ONE);
	emit_param("MyString", "%s", str1.Value());
	emit_param("MyString", "%s", str2.Value());
	emit_output_expected_header();
	emit_param("Env", "%s", ONE_REP);
	emit_output_actual_header();
	emit_param("Env", "%s", actual.Value());
	if(actual != ONE_REP) {
		FAIL;
	}
	PASS;
}

static bool test_insert_env_into_classad_v1_empty() {
	emit_test("Test that InsertEnvIntoClassAd() inserts the environment "
		"variables from an Env object in V1 format into the empty classad.");
	Env env;
	env.MergeFromV1Raw(V1R, NULL);
	ClassAd classad;
	env.InsertEnvIntoClassAd(&classad, NULL);
	char* actual = NULL;
	classad.LookupString("Environment", &actual);
	emit_input_header();
	emit_param("Env", "%s", V1R);
	emit_param("ClassAd", "%s", "");
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_param("ClassAd Environment", "%s", V2R);
	emit_output_actual_header();
	emit_param("ClassAd Environment", "%s", actual);
	if(!strings_similar(actual, V2R)) {
		free(actual);
		FAIL;
	}
	free(actual);
	PASS;
}

static bool test_insert_env_into_classad_v2_empty() {
	emit_test("Test that InsertEnvIntoClassAd() inserts the environment "
		"variables from an Env object in V2 format into the empty classad.");
	Env env;
	env.MergeFromV2Raw(V2R, NULL);
	ClassAd classad;
	env.InsertEnvIntoClassAd(&classad, NULL);
	char* actual = NULL;
	classad.LookupString("Environment", &actual);
	emit_input_header();
	emit_param("Env", "%s", V2R);
	emit_param("ClassAd", "%s", "");
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_param("ClassAd Environment", "%s", V2R);
	emit_output_actual_header();
	emit_param("ClassAd Environment", "%s", actual);
	if(!strings_similar(actual, V2R)) {
		free(actual);
		FAIL;
	}
	free(actual);
	PASS;
}

static bool test_insert_env_into_classad_v1_v1_replace() {
	emit_test("Test that InsertEnvIntoClassAd() replaces the environment "
		"variables from an Env object in V1 format into the ClassAd with a V1 "
		"Environment.");
	Env env;
	char* actual = NULL;
	env.MergeFromV1Raw(V1R_REP, NULL);
	ClassAd classad;
	initAdFromString(AD_V1, classad);
	env.InsertEnvIntoClassAd(&classad, NULL);
	classad.LookupString("Env", &actual);
	emit_input_header();
	emit_param("Env", "%s", V1R_REP);
	emit_param("ClassAd", "%s", AD_V1);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_param("ClassAd Env", "%s", V1R_REP);
	emit_output_actual_header();
	emit_param("ClassAd Env", "%s", actual);
	if(!strings_similar(actual, V1R_REP, V1_ENV_DELIM)) {
		free(actual);
		FAIL;
	}
	free(actual);
	PASS;
}

static bool test_insert_env_into_classad_v1_v2_replace() {
	emit_test("Test that InsertEnvIntoClassAd() replaces the environment "
		"variables from an Env object in V1 format into the ClassAd with a V2 "
		"Environment.");
	Env env;
	char* actual = NULL;
	env.MergeFromV1Raw(V1R_REP, NULL);
	ClassAd classad;
	initAdFromString(AD_V2, classad);
	env.InsertEnvIntoClassAd(&classad, NULL);
	classad.LookupString("Environment", &actual);
	emit_input_header();
	emit_param("Env", "%s", V1R_REP);
	emit_param("ClassAd", "%s", AD_V2);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_param("ClassAd Environment", "%s", V2R_REP);
	emit_output_actual_header();
	emit_param("ClassAd Environment", "%s", actual);
	if(!strings_similar(actual, V2R_REP)) {
		free(actual);
		FAIL;
	}
	free(actual);
	PASS;
}

static bool test_insert_env_into_classad_v2_v1_replace() {
	emit_test("Test that InsertEnvIntoClassAd() replaces the environment "
		"variables from an Env object in V2 format into the ClassAd with a V1 "
		"Environment.");
	Env env;
	char* actual = NULL;
	env.MergeFromV2Raw(V2R_REP, NULL);
	ClassAd classad;
	initAdFromString(AD_V1, classad);
	env.InsertEnvIntoClassAd(&classad, NULL);
	classad.LookupString("Env", &actual);
	emit_input_header();
	emit_param("Env", "%s", V2R_REP);
	emit_param("ClassAd", "%s", AD_V1);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_param("ClassAd Env", "%s", V1R_REP);
	emit_output_actual_header();
	emit_param("ClassAd Env", "%s", actual);
	if(!strings_similar(actual, V1R_REP, V1_ENV_DELIM)) {
		free(actual);
		FAIL;
	}
	free(actual);
	PASS;
}

static bool test_insert_env_into_classad_v2_v2_replace() {
	emit_test("Test that InsertEnvIntoClassAd() replaces the environment "
		"variables from an Env object in V2 format into the ClassAd with a V2 "
		"Environment.");
	Env env;
	char* actual = NULL;
	env.MergeFromV2Raw(V2R_REP, NULL);
	ClassAd classad;
	initAdFromString(AD_V2, classad);
	env.InsertEnvIntoClassAd(&classad, NULL);
	classad.LookupString("Environment", &actual);
	emit_input_header();
	emit_param("Env", "%s", V2R_REP);
	emit_param("ClassAd", "%s", AD_V2);
	emit_param("MyString", "%s", "NULL");
	emit_output_expected_header();
	emit_param("ClassAd Environment", "%s", V2R_REP);
	emit_output_actual_header();
	emit_param("ClassAd Environment", "%s", actual);
	if(!strings_similar(actual, V2R_REP)) {
		free(actual);
		FAIL;
	}
	free(actual);
	PASS;
}
static bool test_insert_env_into_classad_version_v1() {
	emit_test("Test that InsertEnvIntoClassAd() adds the environment "
		"variables from an Env object in V2 format into the ClassAd when the "
		"CondorVersionInfo requires V1 format.");
	Env env;
	CondorVersionInfo info("$CondorVersion: 6.0.0 " __DATE__ " PRE-RELEASE $");
	char *actual = NULL, *version = info.get_version_string();
	env.MergeFromV2Raw(V2R, NULL);
	ClassAd classad;
	env.InsertEnvIntoClassAd(&classad, NULL, NULL, &info);
	classad.LookupString("Env", &actual);
	emit_input_header();
	emit_param("Env", "%s", V2R);
	emit_param("ClassAd", "%s", EMPTY);
	emit_param("MyString", "%s", "NULL");
	emit_param("STRING", "%s", "NULL");
	emit_param("CondorVersionInfo", "%s", version);
	emit_output_expected_header();
	emit_param("ClassAd Env", "%s", V1R);
	emit_output_actual_header();
	emit_param("ClassAd Env", "%s", actual);
	if(!strings_similar(actual, V1R, V1_ENV_DELIM)) {
		free(actual); free(version);
		FAIL;
	}
	free(actual); free(version);
	PASS;
}

static bool test_insert_env_into_classad_version_v1_os_winnt() {
	emit_test("Test that InsertEnvIntoClassAd() adds the environment "
		"variables from an Env object in V2 format into the ClassAd when the "
		"CondorVersionInfo requires V1 format and the target OS is WINNT.");
	Env env;
	CondorVersionInfo info("$CondorVersion: 6.0.0 " __DATE__ " PRE-RELEASE $");
	char *actual = NULL, *version = info.get_version_string();
	env.MergeFromV2Raw(V2R, NULL);
	ClassAd classad;
	env.InsertEnvIntoClassAd(&classad, NULL, "WINNT", &info);
	classad.LookupString("Env", &actual);
	emit_input_header();
	emit_param("Env", "%s", V2R);
	emit_param("ClassAd", "%s", EMPTY);
	emit_param("MyString", "%s", "NULL");
	emit_param("STRING", "%s", "WINNT");
	emit_param("CondorVersionInfo", "%s", version);
	emit_output_expected_header();
	emit_param("ClassAd Env", "%s", V1R_WIN);
	emit_output_actual_header();
	emit_param("ClassAd Env", "%s", actual);
	if(!strings_similar(actual, V1R_WIN, "|")) {
		free(actual); free(version);
		FAIL;
	}
	free(actual); free(version);
	PASS;
}

static bool test_insert_env_into_classad_version_v1_os_win32() {
	emit_test("Test that InsertEnvIntoClassAd() adds the environment "
		"variables from an Env object in V2 format into the ClassAd when the "
		"CondorVersionInfo requires V1 format and the target OS is WIN32.");
	Env env;
	CondorVersionInfo info("$CondorVersion: 6.0.0 " __DATE__ " PRE-RELEASE $");
	char *actual = NULL, *version = info.get_version_string();
	env.MergeFromV2Raw(V2R, NULL);
	ClassAd classad;
	env.InsertEnvIntoClassAd(&classad, NULL, "WIN32", &info);
	classad.LookupString("Env", &actual);
	emit_input_header();
	emit_param("Env", "%s", V2R);
	emit_param("ClassAd", "%s", EMPTY);
	emit_param("MyString", "%s", "NULL");
	emit_param("STRING", "%s", "WIN32");
	emit_param("CondorVersionInfo", "%s", version);
	emit_output_expected_header();
	emit_param("ClassAd Env", "%s", V1R_WIN);
	emit_output_actual_header();
	emit_param("ClassAd Env", "%s", actual);
	if(!strings_similar(actual, V1R_WIN, "|")) {
		free(actual); free(version);
		FAIL;
	}
	free(actual); free(version);
	PASS;
}

static bool test_insert_env_into_classad_version_v1_os_unix() {
	emit_test("Test that InsertEnvIntoClassAd() adds the environment "
		"variables from an Env object in V2 format into the ClassAd when the "
		"CondorVersionInfo requires V1 format and the target OS is UNIX.");
	Env env;
	CondorVersionInfo info("$CondorVersion: 6.0.0 " __DATE__ " PRE-RELEASE $");
	char *actual = NULL, *version = info.get_version_string();
	env.MergeFromV2Raw(V2R, NULL);
	ClassAd classad;
	env.InsertEnvIntoClassAd(&classad, NULL, "UNIX", &info);
	classad.LookupString("Env", &actual);
	emit_input_header();
	emit_param("Env", "%s", V2R);
	emit_param("ClassAd", "%s", EMPTY);
	emit_param("MyString", "%s", "NULL");
	emit_param("STRING", "%s", "UNIX");
	emit_param("CondorVersionInfo", "%s", version);
	emit_output_expected_header();
	emit_param("ClassAd Env", "%s", V1R_NIX);
	emit_output_actual_header();
	emit_param("ClassAd Env", "%s", actual);
	if(!strings_similar(actual, V1R_NIX, V1_ENV_DELIM_NIX)) {
		free(actual); free(version);
		FAIL;
	}
	free(actual); free(version);
	PASS;
}

static bool test_insert_env_into_classad_version_v1_semi() {
	emit_test("Test that InsertEnvIntoClassAd() adds the environment "
		"variables from an Env object in V2 format into the ClassAd when the "
		"CondorVersionInfo requires V1 format and the ClassAd previously used "
		"a semicolon as a delimiter.");
	Env env;
	CondorVersionInfo info("$CondorVersion: 6.0.0 " __DATE__ " PRE-RELEASE $");
	char *actual = NULL, *version = info.get_version_string();
	env.MergeFromV2Raw(V2R, NULL);
	ClassAd classad;
	initAdFromString(AD_V1, classad);
	env.InsertEnvIntoClassAd(&classad, NULL, NULL, &info);
	classad.LookupString("Env", &actual);
	emit_input_header();
	emit_param("Env", "%s", V2R);
	emit_param("ClassAd", "%s", AD_V1);
	emit_param("MyString", "%s", "NULL");
	emit_param("STRING", "%s", "NULL");
	emit_param("CondorVersionInfo", "%s", version);
	emit_output_expected_header();
	emit_param("ClassAd Env", "%s", V1R);
	emit_output_actual_header();
	emit_param("ClassAd Env", "%s", actual);
	if(!strings_similar(actual, V1R, V1_ENV_DELIM)) {
		free(actual); free(version);
		FAIL;
	}
	free(actual); free(version);
	PASS;
}

static bool test_insert_env_into_classad_version_v1_line() {
	emit_test("Test that InsertEnvIntoClassAd() adds the environment "
		"variables from an Env object in V2 format into the ClassAd when the "
		"CondorVersionInfo requires V1 format and the ClassAd previously used "
		"a '|' as a delimiter.");
	Env env;
	CondorVersionInfo info("$CondorVersion: 6.0.0 " __DATE__ " PRE-RELEASE $");
	char *actual = NULL, *version = info.get_version_string();
	env.MergeFromV2Raw(V2R_REP, NULL);
	ClassAd classad;
	initAdFromString(AD_V1_WIN, classad);
	env.InsertEnvIntoClassAd(&classad, NULL, NULL, &info);
	classad.LookupString("Env", &actual);
	emit_input_header();
	emit_param("Env", "%s", V2R);
	emit_param("ClassAd", "%s", AD_V1_WIN);
	emit_param("MyString", "%s", "NULL");
	emit_param("STRING", "%s", "NULL");
	emit_param("CondorVersionInfo", "%s", version);
	emit_output_expected_header();
	emit_param("ClassAd Env", "%s", V1R_WIN);
	emit_output_actual_header();
	emit_param("ClassAd Env", "%s", actual);
	if(!strings_similar(actual, V1R_REP_WIN, "|")) {
		free(actual); free(version);
		FAIL;
	}
	free(actual); free(version);
	PASS;
}

static bool test_insert_env_into_classad_version_v1_current() {
	emit_test("Test that InsertEnvIntoClassAd() adds the environment "
		"variables from an Env object in V2 format into the ClassAd when the "
		"CondorVersionInfo requires V1 format and we use the delimiter for the "
		"current OS.");
	Env env;
	CondorVersionInfo info("$CondorVersion: 6.0.0 " __DATE__ " PRE-RELEASE $");
	char *actual = NULL, *version = info.get_version_string();
	env.MergeFromV2Raw(V2R, NULL);
	ClassAd classad;
	env.InsertEnvIntoClassAd(&classad, NULL, NULL, &info);
	classad.LookupString("Env", &actual);
	emit_input_header();
	emit_param("Env", "%s", V2R);
	emit_param("ClassAd", "%s", EMPTY);
	emit_param("MyString", "%s", "NULL");
	emit_param("STRING", "%s", "NULL");
	emit_param("CondorVersionInfo", "%s", version);
	emit_output_expected_header();
	emit_param("ClassAd Env", "%s", V1R);
	emit_output_actual_header();
	emit_param("ClassAd Env", "%s", actual);
	if(!strings_similar(actual, V1R, V1_ENV_DELIM)) {
		free(actual); free(version);
		FAIL;
	}
	free(actual); free(version);
	PASS;
}

static bool test_insert_env_into_classad_version_v1_error_v2() {
	emit_test("Test that InsertEnvIntoClassAd() sets the error MyString for "
		"an Env object in V2 format that cannot be converted into V1 format "
		"when the CondorVersionInfo requires V1 format, but the ClassAd started"
		" with V2.");
	emit_comment("This test just checks if the error message is not empty.");
	Env env;
	CondorVersionInfo info("$CondorVersion: 6.0.0 " __DATE__ " PRE-RELEASE $");
	char *actual = NULL, *version = info.get_version_string();
	MyString error;
	env.MergeFromV2Raw(V2R_SEMI, NULL);
	ClassAd classad;
	initAdFromString(AD_V2, classad);
	env.InsertEnvIntoClassAd(&classad, &error, NULL, &info);
	classad.LookupString("Env", &actual);
	emit_input_header();
	emit_param("Env", "%s", V2R_SEMI);
	emit_param("ClassAd", "%s", EMPTY);
	emit_param("MyString", "%s", "");
	emit_param("STRING", "%s", "NULL");
	emit_param("CondorVersionInfo", "%s", version);
	emit_output_actual_header();
	emit_param("Error MyString", "%s", error.Value());
	if(error.IsEmpty()) {
		free(actual); free(version);
		FAIL;
	}
	free(actual); free(version);
	PASS;
}

static bool test_insert_env_into_classad_version_v1_error() {
	emit_test("Test that InsertEnvIntoClassAd() sets the error MyString for "
		"an Env object in V2 format that cannot be converted into V1 format "
		"when the CondorVersionInfo requires V1 format.");
	emit_comment("This test just checks if the error message is not empty.");
	Env env;
	CondorVersionInfo info("$CondorVersion: 6.0.0 " __DATE__ " PRE-RELEASE $");
	char *actual = NULL, *version = info.get_version_string();
	MyString error;
	env.MergeFromV2Raw(V2R_SEMI, NULL);
	ClassAd classad;
	env.InsertEnvIntoClassAd(&classad, &error, NULL, &info);
	classad.LookupString("Env", &actual);
	emit_input_header();
	emit_param("Env", "%s", V2R_SEMI);
	emit_param("ClassAd", "%s", EMPTY);
	emit_param("MyString", "%s", "");
	emit_param("STRING", "%s", "NULL");
	emit_param("CondorVersionInfo", "%s", version);
	emit_output_actual_header();
	emit_param("Error MyString", "%s", error.Value());
	if(error.IsEmpty()) {
		free(version);	//don't need to free 'actual'
		FAIL;
	}
	free(version);	//don't need to free 'actual'
	PASS;
}

static bool test_insert_env_into_classad_version_v2() {
	emit_test("Test that InsertEnvIntoClassAd() adds the environment "
		"variables from an Env object in V1 format into the ClassAd when the "
		"CondorVersionInfo doesn't require V1 format.");
	Env env;
	CondorVersionInfo info("$CondorVersion: 7.0.0 " __DATE__ " PRE-RELEASE $");
	char *actual = NULL, *version = info.get_version_string();
	env.MergeFromV1Raw(V1R, NULL);
	ClassAd classad;
	env.InsertEnvIntoClassAd(&classad, NULL, NULL, &info);
	classad.LookupString("Environment", &actual);
	emit_input_header();
	emit_param("Env", "%s", V1R);
	emit_param("ClassAd", "%s", EMPTY);
	emit_param("MyString", "%s", "NULL");
	emit_param("STRING", "%s", "NULL");
	emit_param("CondorVersionInfo", "%s", version);
	emit_output_expected_header();
	emit_param("ClassAd Environment", "%s", V2R);
	emit_output_actual_header();
	emit_param("ClassAd Environment", "%s", actual);
	if(!strings_similar(actual, V2R)) {
		free(actual); free(version);
		FAIL;
	}
	free(actual); free(version);
	PASS;
}

static bool test_condor_version_requires_v1_false() {
	emit_test("Test that CondorVersionRequiresV1() returns false for condor "
		"version 7.0.0.");
	CondorVersionInfo info("$CondorVersion: 7.0.0 " __DATE__ " PRE-RELEASE $");
	bool expect = false;
	bool actual = Env::CondorVersionRequiresV1(info);
	char* version = info.get_version_string();
	emit_input_header();
	emit_param("Condor Version", "%s", version);
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		free(version);
		FAIL;
	}
	free(version);
	PASS;
}

static bool test_condor_version_requires_v1_true() {
	emit_test("Test that CondorVersionRequiresV1() returns true for condor "
		"version 6.0.0.");
	CondorVersionInfo info("$CondorVersion: 6.0.0 " __DATE__ " PRE-RELEASE $");
	bool expect = true;
	bool actual = Env::CondorVersionRequiresV1(info);
	char* version = info.get_version_string();
	emit_input_header();
	emit_param("Condor Version", "%s", version);
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		free(version);
		FAIL;
	}
	free(version);
	PASS;
}

static bool test_condor_version_requires_v1_this() {
	emit_test("Test that CondorVersionRequiresV1() returns false for this "
		"version of condor.");
	CondorVersionInfo info;
	bool expect = false;
	bool actual = Env::CondorVersionRequiresV1(info);
	char* version = info.get_version_string();
	emit_input_header();
	emit_param("Condor Version", "%s", version);
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		free(version);
		FAIL;
	}
	free(version);
	PASS;
}

static bool test_get_delim_str_v2_raw_return_empty() {
	emit_test("Test that getDelimitedStringV2Raw() returns true for an empty "
		"Env object.");
	Env env;
	MyString result, error;
	bool expect = true;
	bool actual = env.getDelimitedStringV2Raw(&result, &error);
	emit_input_header();
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v2_raw_return_v1() {
	emit_test("Test that getDelimitedStringV2Raw() returns true for an Env "
		"object using V1 format.");
	Env env;
	MyString result, error;
	env.MergeFromV1Raw(V1R, NULL);
	bool expect = true;
	bool actual = env.getDelimitedStringV2Raw(&result, &error);
	emit_input_header();
	emit_param("Env", "%s", V1R);
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v2_raw_return_v2() {
	emit_test("Test that getDelimitedStringV2Raw() returns true for an Env "
		"object using V2 format.");
	Env env;
	MyString result, error;
	env.MergeFromV2Raw(V2R, NULL);
	bool expect = true;
	bool actual = env.getDelimitedStringV2Raw(&result, &error);
	emit_input_header();
	emit_param("Env", "%s", V2R);
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v2_raw_result_empty() {
	emit_test("Test that getDelimitedStringV2Raw() sets the result MyString "
		"to the expected value for an empty Env object.");
	Env env;
	MyString actual, error; 
	env.getDelimitedStringV2Raw(&actual, &error);
	emit_input_header();
	emit_param("Env", "%s", "");
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_param("Result MyString", "%s", EMPTY); 
	emit_output_actual_header();
	emit_param("Result MyString", "%s", actual.Value());
	if(actual != EMPTY) {
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v2_raw_result_v1() {
	emit_test("Test that getDelimitedStringV2Raw() sets the result MyString "
		"to the expected value for an Env object using V1 format.");
	Env env;
	MyString actual, error;
	env.MergeFromV1Raw(V1R, NULL);
	env.getDelimitedStringV2Raw(&actual, &error);
	emit_input_header();
	emit_param("Env", "%s", V1R);
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_param("Result MyString", "%s", V2R);
	emit_output_actual_header();
	emit_param("Result MyString", "%s", actual.Value());
	if(!strings_similar(actual.Value(), V2R)) { 
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v2_raw_result_v2() {
	emit_test("Test that getDelimitedStringV2Raw() sets the result MyString "
		"to the expected value for an Env object using V2 format.");
	Env env;
	MyString actual, error;
	env.MergeFromV2Raw(V2R, NULL);
	env.getDelimitedStringV2Raw(&actual, &error);
	emit_input_header();
	emit_param("Env", "%s", V2R);
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_param("Result MyString", "%s", V2R);
	emit_output_actual_header();
	emit_param("Result MyString", "%s", actual.Value());
	if(!strings_similar(actual.Value(), V2R)) { 
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v2_raw_result_add() {
	emit_test("Test that getDelimitedStringV2Raw() sets the result MyString "
		"to the expected value after adding environment variables with "
		"MergeFromV2Raw().");
	Env env;
	MyString actual1, actual2, error;
	env.MergeFromV2Raw(V2R_REP, NULL);
	env.getDelimitedStringV2Raw(&actual1, &error);
	env.MergeFromV2Raw(V2R_ADD, NULL);
	env.getDelimitedStringV2Raw(&actual2, &error);
	emit_input_header();
	emit_param("Env", "%s", V2R_REP);
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_param("Result MyString Before", "%s", V2R_REP);
	emit_param("Result MyString After", "%s", V2R_REP_ADD);
	emit_output_actual_header();
	emit_param("Result MyString Before", "%s", actual1.Value());
	emit_param("Result MyString After", "%s", actual2.Value());
	if(!strings_similar(actual1.Value(), V2R_REP) || 
		!strings_similar(actual2.Value(), V2R_REP_ADD))
	{
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v2_raw_result_replace() {
	emit_test("Test that getDelimitedStringV2Raw() sets the result MyString "
		"to the expected value after replacing environment variables with"
		"MergeFromV2Raw().");
	Env env;
	MyString actual1, actual2, error;
	env.MergeFromV2Raw(V2R, NULL);
	env.getDelimitedStringV2Raw(&actual1, &error);
	env.MergeFromV2Raw(V2R_REP, NULL);
	env.getDelimitedStringV2Raw(&actual2, &error);
	emit_input_header();
	emit_param("Env", "%s", V2R);
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_param("Result MyString Before", "%s", V2R);
	emit_param("Result MyString After", "%s", V2R_REP);
	emit_output_actual_header();
	emit_param("Result MyString Before", "%s", actual1.Value());
	emit_param("Result MyString After", "%s", actual2.Value());
	if(!strings_similar(actual1.Value(), V2R) ||
		!strings_similar(actual2.Value(), V2R_REP))
	{
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v2_raw_result_add_replace() {
	emit_test("Test that getDelimitedStringV2Raw() sets the result MyString "
		"to the expected value after adding and replacing environment variables"
		" with MergeFromV2Raw().");
	Env env;
	MyString actual1, actual2, error;
	env.MergeFromV2Raw(V2R, NULL);
	env.getDelimitedStringV2Raw(&actual1, &error);
	env.MergeFromV2Raw(V2R_REP_ADD, NULL);
	env.getDelimitedStringV2Raw(&actual2, &error);
	emit_input_header();
	emit_param("Env", "%s", V2R);
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_param("Result MyString Before", "%s", V2R);
	emit_param("Result MyString After", "%s", V2R_REP_ADD);
	emit_output_actual_header();
	emit_param("Result MyString Before", "%s", actual1.Value());
	emit_param("Result MyString After", "%s", actual2.Value());
	if(!strings_similar(actual1.Value(), V2R) || 
		!strings_similar(actual2.Value(), V2R_REP_ADD))
	{
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v2_raw_mark_empty() {
	emit_test("Test that getDelimitedStringV2Raw() adds the RAW_ENV_V2_MARKER"
		" to the result MyString for an empty Env object.");
	Env env;
	MyString actual, error;
	env.getDelimitedStringV2Raw(&actual, &error, true);
	emit_input_header();
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_param("BOOLEAN", "%s", "TRUE");
	emit_output_expected_header();
	emit_param("Result MyString", "'%s'", " ");
	emit_output_actual_header();
	emit_param("Result MyString", "'%s'", actual.Value());
	if(actual != " ") {
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v2_raw_mark_v1() {
	emit_test("Test that getDelimitedStringV2Raw() adds the RAW_ENV_V2_MARKER"
		" to the result MyString for an Env object using V1 format.");
	Env env;
	MyString actual, error;
	env.MergeFromV1Raw(V1R, NULL);
	env.getDelimitedStringV2Raw(&actual, &error, true);
	emit_input_header();
	emit_param("Env", "%s", V1R);
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_param("BOOLEAN", "%s", "TRUE");
	emit_output_expected_header();
	emit_param("Result MyString", "%s", V2R);
	emit_output_actual_header();
	emit_param("Result MyString", "%s", actual.Value());
	if(!strings_similar(actual.Value(), V2R) || actual[0] != ' ') {
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v2_raw_mark_v2() {
	emit_test("Test that getDelimitedStringV2Raw() adds the RAW_ENV_V2_MARKER"
		" to the result MyString for an Env object using V2 format.");
	Env env;
	MyString actual, error;
	env.MergeFromV2Raw(V2R_SEMI, NULL);
	env.getDelimitedStringV2Raw(&actual, &error, true);
	emit_input_header();
	emit_param("Env", "%s", V2R);
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_param("BOOLEAN", "%s", "FALSE");
	emit_output_expected_header();
	emit_param("Result MyString", "%s", V2R_MARK);
	emit_output_actual_header();
	emit_param("Result MyString", "%s", actual.Value());
	if(!strings_similar(actual.Value(), V2R_MARK) || 
		actual[0] != ' ') 
	{
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v1_raw_return_empty() {
	emit_test("Test that getDelimitedStringV1Raw() returns true for an empty "
		"Env object.");
	Env env;
	MyString result, error;
	bool expect = true;
	bool actual = env.getDelimitedStringV1Raw(&result, &error);
	emit_input_header();
	emit_param("Env", "%s", "");
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v1_raw_return_v1() {
	emit_test("Test that getDelimitedStringV1Raw() returns true for an Env "
		"object using V1 format.");
	Env env;
	MyString result, error;
	env.MergeFromV1Raw(V1R, NULL);
	bool expect = true;
	bool actual = env.getDelimitedStringV1Raw(&result, &error);
	emit_input_header();
	emit_param("Env", "%s", V1R);
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v1_raw_return_v2() {
	emit_test("Test that getDelimitedStringV1Raw() returns true for an Env "
		"object using V2 format.");
	Env env;
	MyString result, error;
	env.MergeFromV2Raw(V2R, NULL);
	bool expect = true;
	bool actual = env.getDelimitedStringV1Raw(&result, &error);
	emit_input_header();
	emit_param("Env", "%s", V2R);
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v1_raw_return_delim() {
	emit_test("Test that getDelimitedStringV1Raw() returns false for an Env "
		"object using V2 format with a ';'.");
	Env env;
	MyString result, error;
	env.MergeFromV2Raw(V2R_SEMI, NULL);
	bool expect = false;
	bool actual = env.getDelimitedStringV1Raw(&result, &error);
	emit_input_header();
	emit_param("Env", "%s", V2R_SEMI);
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v1_raw_error_delim() {
	emit_test("Test that getDelimitedStringV1Raw() sets the error MyString "
		"for an Env object using V2 format with a ';'.");
	emit_comment("This test just checks if the error message is not empty.");
	Env env;
	MyString result, error;
	env.MergeFromV2Raw(V2R_SEMI, NULL);
	env.getDelimitedStringV1Raw(&result, &error);
	emit_input_header();
	emit_param("Env", "%s", V2R_SEMI);
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_param("Error MyString is Empty", "%s", "FALSE");
	emit_output_actual_header();
	emit_param("Error MyString is Empty", "%s", tfstr(error.IsEmpty()));
	if(error.IsEmpty()) {
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v1_raw_result_empty() {
	emit_test("Test that getDelimitedStringV1Raw() sets the result MyString "
		"to the expected value for an empty Env object.");
	Env env;
	MyString actual, error;
	env.getDelimitedStringV1Raw(&actual, &error);
	emit_input_header();
	emit_param("Env", "%s", "");
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_param("Result MyString", "%s", EMPTY);
	emit_output_actual_header();
	emit_param("Result MyString", "%s", actual.Value());
	if(actual != EMPTY) {
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v1_raw_result_v1() {
	emit_test("Test that getDelimitedStringV1Raw() sets the result MyString "
		"to the expected value for an Env object in V1 format.");
	Env env;
	MyString actual, error;
	env.MergeFromV1Raw(V1R, NULL);
	env.getDelimitedStringV1Raw(&actual, &error);
	emit_input_header();
	emit_param("Env", "%s", V1R);
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_param("Result MyString", "%s", V1R);
	emit_output_actual_header();
	emit_param("Result MyString", "%s", actual.Value());
	if(!strings_similar(actual.Value(), V1R, V1_ENV_DELIM)) {
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v1_raw_result_v2() {
	emit_test("Test that getDelimitedStringV1Raw() sets the result MyString "
		"to the expected value for an Env object in V2 format.");
	Env env;
	MyString actual, error;
	env.MergeFromV2Raw(V2R, NULL);
	env.getDelimitedStringV1Raw(&actual, &error);
	emit_input_header();
	emit_param("Env", "%s", V2R);
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_param("Result MyString", "%s", V1R);
	emit_output_actual_header();
	emit_param("Result MyString", "%s", actual.Value());
	if(!strings_similar(actual.Value(), V1R, V1_ENV_DELIM)) {
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v1_raw_result_add() {
	emit_test("Test that getDelimitedStringV1Raw() sets the result MyString "
		"to the expected value after adding environment variables with "
		"MergeFromV1Raw().");
	Env env;
	MyString actual1, actual2, error;
	env.MergeFromV1Raw(V1R_REP, NULL);
	env.getDelimitedStringV1Raw(&actual1, &error);
	env.MergeFromV1Raw(V1R_ADD, NULL);
	env.getDelimitedStringV1Raw(&actual2, &error);
	emit_input_header();
	emit_param("Env", "%s", V1R_REP);
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_param("Result MyString Before", "%s", V1R_REP);
	emit_param("Result MyString After", "%s", V1R_REP_ADD);
	emit_output_actual_header();
	emit_param("Result MyString Before", "%s", actual1.Value());
	emit_param("Result MyString After", "%s", actual2.Value());
	if(!strings_similar(actual1.Value(), V1R_REP, V1_ENV_DELIM) || 
		!strings_similar(actual2.Value(), V1R_REP_ADD, V1_ENV_DELIM))
	{
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v1_raw_result_replace() {
	emit_test("Test that getDelimitedStringV1Raw() sets the result MyString "
		"to the expected value after replacing environment variables with "
		"MergeFromV1Raw().");
	Env env;
	MyString actual1, actual2, error;
	env.MergeFromV1Raw(V1R, NULL);
	env.getDelimitedStringV1Raw(&actual1, &error);
	env.MergeFromV1Raw(V1R_REP, NULL);
	env.getDelimitedStringV1Raw(&actual2, &error);
	emit_input_header();
	emit_param("Env", "%s", V1R);
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_param("Result MyString Before", "%s", V1R);
	emit_param("Result MyString After", "%s", V1R_REP); 
	emit_output_actual_header();
	emit_param("Result MyString Before", "%s", actual1.Value());
	emit_param("Result MyString After", "%s", actual2.Value());
	if(!strings_similar(actual1.Value(), V1R, V1_ENV_DELIM) || 
		!strings_similar(actual2.Value(), V1R_REP, V1_ENV_DELIM))
	{
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v1_raw_result_add_replace() {
	emit_test("Test that getDelimitedStringV1Raw() sets the result MyString "
		"to the expected value after adding and replacing environment variables"
		" with MergeFromV1Raw().");
	Env env;
	MyString actual1, actual2, error;
	env.MergeFromV1Raw(V1R, NULL);
	env.getDelimitedStringV1Raw(&actual1, &error);
	env.MergeFromV1Raw(V1R_REP_ADD, NULL);
	env.getDelimitedStringV1Raw(&actual2, &error);
	emit_input_header();
	emit_param("Env", "%s", V1R);
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_param("Result MyString Before", "%s", V1R);
	emit_param("Result MyString After", "%s", V1R_REP);
	emit_output_actual_header();
	emit_param("Result MyString Before", "%s", actual1.Value());
	emit_param("Result MyString After", "%s", actual2.Value());
	if(!strings_similar(actual1.Value(), V1R, V1_ENV_DELIM) || 
		!strings_similar(actual2.Value(), V1R_REP_ADD, V1_ENV_DELIM))
	{
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v1or2_raw_ad_return_empty() {
	emit_test("Test that getDelimitedStringV1or2Raw() returns true for an "
		"empty ClassAd.");
	Env env;
	ClassAd classad;
	MyString result, error;
	bool expect = true;
	bool actual = env.getDelimitedStringV1or2Raw(&classad, &result, &error);
	emit_input_header();
	emit_param("Env", "%s", "");
	emit_param("ClassAd", "%s", "");
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v1or2_raw_ad_return_v1() {
	emit_test("Test that getDelimitedStringV1or2Raw() returns true for a "
		"ClassAd using V1 format.");
	Env env;
	ClassAd classad;
	initAdFromString(AD_V1, classad);
	MyString result, error;
	bool expect = true;
	bool actual = env.getDelimitedStringV1or2Raw(&classad, &result, &error);
	emit_input_header();
	emit_param("Env", "%s", "");
	emit_param("ClassAd", "%s", AD_V1);
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v1or2_raw_ad_return_v2() {
	emit_test("Test that getDelimitedStringV1or2Raw() returns true for a "
		"ClassAd using V2 format.");
	Env env;
	ClassAd classad;
	initAdFromString(AD_V2, classad);
	MyString result, error;
	bool expect = true;
	bool actual = env.getDelimitedStringV1or2Raw(&classad, &result, &error);
	emit_input_header();
	emit_param("Env", "%s", "");
	emit_param("ClassAd", "%s", AD_V2);
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v1or2_raw_ad_return_invalid_v1() {
	emit_test("Test that getDelimitedStringV1or2Raw() returns false when "
		"passed an invalid ClassAd using V1 format.");
	Env env;
	ClassAd classad;
	initAdFromString(AD_V1_MISS_BOTH, classad);
	MyString result, error;
	bool expect = false;
	bool actual = env.getDelimitedStringV1or2Raw(&classad, &result, &error);
	emit_input_header();
	emit_param("Env", "%s", "");
	emit_param("ClassAd", "%s", AD_V1_MISS_BOTH);
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v1or2_raw_ad_return_invalid_v2() {
	emit_test("Test that getDelimitedStringV1or2Raw() returns false when "
		"passed an invalid ClassAd using V2 format.");
	Env env;
	ClassAd classad;
	initAdFromString(AD_V2_MISS_BOTH, classad);
	MyString result, error;
	bool expect = false;
	bool actual = env.getDelimitedStringV1or2Raw(&classad, &result, &error);
	emit_input_header();
	emit_param("Env", "%s", "");
	emit_param("ClassAd", "%s", AD_V2_MISS_BOTH);
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v1or2_raw_ad_error_v1() {
	emit_test("Test that getDelimitedStringV1or2Raw() sets the error MyString"
		" when passed an invalid ClassAd using V1 format.");
	emit_comment("This test just checks if the error message is not empty.");
	Env env;
	ClassAd classad;
	initAdFromString(AD_V1_MISS_BOTH, classad);
	MyString result, error;
	env.getDelimitedStringV1or2Raw(&classad, &result, &error);
	emit_input_header();
	emit_param("Env", "%s", "");
	emit_param("ClassAd", "%s", AD_V1_MISS_BOTH);
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_actual_header();
	emit_param("Error Message", "%s", error.Value());
	if(error.IsEmpty()) {
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v1or2_raw_ad_error_v2() {
	emit_test("Test that getDelimitedStringV1or2Raw() sets the error MyString"
		" when passed an invalid ClassAd using V2 format.");
	emit_comment("This test just checks if the error message is not empty.");
	Env env;
	ClassAd classad;
	initAdFromString(AD_V2_MISS_BOTH, classad);
	MyString result, error;
	env.getDelimitedStringV1or2Raw(&classad, &result, &error);
	emit_input_header();
	emit_param("Env", "%s", "");
	emit_param("ClassAd", "%s", AD_V2_MISS_BOTH);
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_actual_header();
	emit_param("Error Message", "%s", error.Value());
	if(error.IsEmpty()) {
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v1or2_raw_ad_result_empty() {
	emit_test("Test that getDelimitedStringV1or2Raw() sets the result "
		"MyString to the expected value for an empty Env object.");
	Env env;
	ClassAd classad;
	MyString actual, error;
	env.getDelimitedStringV1or2Raw(&classad, &actual, &error);
	emit_input_header();
	emit_param("Env", "%s", "");
	emit_param("ClassAd", "%s", "");
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_param("Result MyString", "%s", EMPTY);
	emit_output_actual_header();
	emit_param("Result MyString", "%s", actual.Value());
	if(actual != EMPTY) {
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v1or2_raw_ad_result_v1() {
	emit_test("Test that getDelimitedStringV1or2Raw() sets the result "
		"MyString to the expected value for an Env object in V1 format.");
	Env env;
	ClassAd classad;
	initAdFromString(AD_V1, classad);
	MyString actual, error;
	env.getDelimitedStringV1or2Raw(&classad, &actual, &error);
	emit_input_header();
	emit_param("Env", "%s", "");
	emit_param("ClassAd", "%s", AD_V1);
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_param("Result MyString", "%s", V1R);
	emit_output_actual_header();
	emit_param("Result MyString", "%s", actual.Value());
	if(!strings_similar(actual.Value(), V1R, V1_ENV_DELIM)) {
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v1or2_raw_ad_result_v2() {
	emit_test("Test that getDelimitedStringV1or2Raw() sets the result "
		"MyString to the expected value for an Env object in V2 format.");
	Env env;
	ClassAd classad;
	initAdFromString(AD_V2_SEMI, classad);
	MyString actual, error;
	env.getDelimitedStringV1or2Raw(&classad, &actual, &error);
	emit_input_header();
	emit_param("Env", "%s", "");
	emit_param("ClassAd", "%s", AD_V2_SEMI);
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_param("Result MyString", "%s", V2R_SEMI);
	emit_output_actual_header();
	emit_param("Result MyString", "%s", actual.Value());
	if(!strings_similar(actual.Value(), V2R_SEMI)) {
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v1or2_raw_ad_result_replace() {
	emit_test("Test that getDelimitedStringV1or2Raw() sets the result "
		"MyString to the expected value after replacing environment variables "
		"from the ClassAd.");
	Env env;
	ClassAd classad;
	initAdFromString(AD_V1_REP, classad);
	MyString actual1, actual2, error;
	env.MergeFromV1Raw(V1R, NULL);
	env.getDelimitedStringV1Raw(&actual1, &error);
	env.getDelimitedStringV1or2Raw(&classad, &actual2, &error);
	emit_input_header();
	emit_param("Env", "%s", V1R);
	emit_param("ClassAd", "%s", AD_V1_REP);
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_param("Result MyString Before", "%s", V1R);
	emit_param("Result MyString After", "%s", V1R_REP);
	emit_output_actual_header();
	emit_param("Result MyString Before", "%s", actual1.Value());
	emit_param("Result MyString After", "%s", actual2.Value());
	if(!strings_similar(actual1.Value(), V1R, V1_ENV_DELIM) || 
		!strings_similar(actual2.Value(), V1R_REP, V1_ENV_DELIM)) 
	{
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v1or2_raw_return_empty() {
	emit_test("Test that getDelimitedStringV1or2Raw() returns true for an "
		"empty Env object.");
	Env env;
	MyString result, error;
	bool expect = true;
	bool actual = env.getDelimitedStringV1or2Raw(&result, &error);
	emit_input_header();
	emit_param("Env", "%s", "");
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v1or2_raw_return_v1() {
	emit_test("Test that getDelimitedStringV1or2Raw() returns true for an "
		"Env object using V1 format.");
	Env env;
	MyString result, error;
	env.MergeFromV1Raw(V1R, NULL);
	bool expect = true;
	bool actual = env.getDelimitedStringV1or2Raw(&result, &error);
	emit_input_header();
	emit_param("Env", "%s", V1R);
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v1or2_raw_return_v2() {
	emit_test("Test that getDelimitedStringV1or2Raw() returns true for an "
		"Env object using V2 format.");
	Env env;
	MyString result, error;
	env.MergeFromV2Raw(V2R_SEMI, NULL);
	bool expect = true;
	bool actual = env.getDelimitedStringV1or2Raw(&result, &error);
	emit_input_header();
	emit_param("Env", "%s", V2R_SEMI);
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v1or2_raw_result_empty() {
	emit_test("Test that getDelimitedStringV1or2Raw() sets the result "
		"MyString to the expected value for an empty Env object.");
	Env env;
	MyString actual, error;
	env.getDelimitedStringV1or2Raw(&actual, &error);
	emit_input_header();
	emit_param("Env", "%s", "");
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_param("Result MyString", "%s", EMPTY);
	emit_output_actual_header();
	emit_param("Result MyString", "%s", actual.Value());
	if(actual != EMPTY) {
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v1or2_raw_result_v1() {
	emit_test("Test that getDelimitedStringV1or2Raw() sets the result "
		"MyString to the expected value for an Env object using V1 format.");
	Env env;
	MyString actual, error;
	env.MergeFromV1Raw(V1R, NULL);
	env.getDelimitedStringV1or2Raw(&actual, &error);
	emit_input_header();
	emit_param("Env", "%s", V1R);
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_param("Result MyString", "%s", V1R);
	emit_output_actual_header();
	emit_param("Result MyString", "%s", actual.Value());
	if(!strings_similar(actual.Value(), V1R, V1_ENV_DELIM)) {
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v1or2_raw_result_v2() {
	emit_test("Test that getDelimitedStringV1or2Raw() sets the result "
		"MyString to the expected value for an Env object using V2 format.");
	Env env;
	MyString actual, error;
	env.MergeFromV2Raw(V2R_SEMI, NULL);
	env.getDelimitedStringV1or2Raw(&actual, &error);
	emit_input_header();
	emit_param("Env", "%s", V2R_SEMI);
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_param("Result MyString", "%s", V2R_SEMI);
	emit_output_actual_header();
	emit_param("Result MyString", "%s", actual.Value());
	if(!strings_similar(actual.Value(), V2R_SEMI)) {
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v1or2_raw_result_add() {
	emit_test("Test that getDelimitedStringV1or2Raw() sets the result "
		"MyString to the expected value after adding environment variables with"
		" MergeFromV1Raw().");
	Env env;
	MyString actual1, actual2, error;
	env.MergeFromV1Raw(V1R_REP, NULL);
	env.getDelimitedStringV1or2Raw(&actual1, &error);
	env.MergeFromV1Raw(V1R_ADD, NULL);
	env.getDelimitedStringV1or2Raw(&actual2, &error);
	emit_input_header();
	emit_param("Env", "%s", V1R_REP);
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_param("Result MyString Before", "%s", V1R_REP);
	emit_param("Result MyString After", "%s", V1R_REP_ADD);
	emit_output_actual_header();
	emit_param("Result MyString Before", "%s", actual1.Value());
	emit_param("Result MyString After", "%s", actual2.Value());
	if(!strings_similar(actual1.Value(), V1R_REP, V1_ENV_DELIM) ||
		!strings_similar(actual2.Value(), V1R_REP_ADD, V1_ENV_DELIM)) 
	{
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v1or2_raw_result_replace() {
	emit_test("Test that getDelimitedStringV1or2Raw() sets the result "
		"MyString to the expected value after replacing environment variables "
		"with MergeFromV1Raw().");
	Env env;
	MyString actual1, actual2, error;
	env.MergeFromV1Raw(V1R, NULL);
	env.getDelimitedStringV1or2Raw(&actual1, &error);
	env.MergeFromV1Raw(V1R_REP, NULL);
	env.getDelimitedStringV1or2Raw(&actual2, &error);
	emit_input_header();
	emit_param("Env", "%s", V1R);
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_param("Result MyString Before", "%s", V1R);
	emit_param("Result MyString After", "%s", V1R_REP);
	emit_output_actual_header();
	emit_param("Result MyString Before", "%s", actual1.Value());
	emit_param("Result MyString After", "%s", actual2.Value());
	if(!strings_similar(actual1.Value(), V1R, V1_ENV_DELIM) ||
		!strings_similar(actual2.Value(), V1R_REP, V1_ENV_DELIM)) 
	{
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v1or2_raw_result_add_replace() {
	emit_test("Test that getDelimitedStringV1or2Raw() sets the result "
		"MyString to the expected value after adding and replacing environment "
		"variables with MergeFromV1Raw().");
	Env env;
	MyString actual1, actual2, error;
	env.MergeFromV1Raw(V1R, NULL);
	env.getDelimitedStringV1or2Raw(&actual1, &error);
	env.MergeFromV1Raw(V1R_REP_ADD, NULL);
	env.getDelimitedStringV1or2Raw(&actual2, &error);
	emit_input_header();
	emit_param("Env", "%s", V1R);
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_param("Result MyString Before", "%s", V1R);
	emit_param("Result MyString After", "%s", V1R_REP_ADD);
	emit_output_actual_header();
	emit_param("Result MyString Before", "%s", actual1.Value());
	emit_param("Result MyString After", "%s", actual2.Value());
	if(!strings_similar(actual1.Value(), V1R, V1_ENV_DELIM) ||
		!strings_similar(actual2.Value(), V1R_REP_ADD, V1_ENV_DELIM)) 
	{
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v2_quoted_return_empty() {
	emit_test("Test that getDelimitedStringV2Quoted() returns true for an "
		"empty Env object.");
	Env env;
	MyString result, error;
	bool expect = true;
	bool actual = env.getDelimitedStringV2Quoted(&result, &error);
	emit_input_header();
	emit_param("Env", "%s", "");
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v2_quoted_return_v1() {
	emit_test("Test that getDelimitedStringV2Quoted() returns true for an "
		"Env object using V1 format.");
	Env env;
	MyString result, error;
	env.MergeFromV1Raw(V1R, NULL);
	bool expect = true;
	bool actual = env.getDelimitedStringV2Quoted(&result, &error);
	emit_input_header();
	emit_param("Env", "%s", V1R);
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v2_quoted_return_v2() {
	emit_test("Test that getDelimitedStringV2Quoted() returns true for an "
		"Env object using V2 format.");
	Env env;
	MyString result, error;
	env.MergeFromV2Raw(V2R, NULL);
	bool expect = true;
	bool actual = env.getDelimitedStringV2Quoted(&result, &error);
	emit_input_header();
	emit_param("Env", "%s", V2R);
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v2_quoted_result_empty() {
	emit_test("Test that getDelimitedStringV2Quoted() sets the result "
		"MyString to the expected value for an empty Env object.");
	Env env;
	MyString actual, error;
	env.getDelimitedStringV2Quoted(&actual, &error);
	emit_input_header();
	emit_param("Env", "%s", "");
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_param("Result MyString", "%s", "\"\"");
	emit_output_actual_header();
	emit_param("Result MyString", "%s", actual.Value());
	if(actual != "\"\"") {
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v2_quoted_result_v1() {
	emit_test("Test that getDelimitedStringV2Quoted() sets the result "
		"MyString to the expected value for an Env object using V1 format.");
	Env env;
	MyString actual, error;
	env.MergeFromV1Raw(V1R, NULL);
	env.getDelimitedStringV2Quoted(&actual, &error);
	emit_input_header();
	emit_param("Env", "%s", V1R);
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_param("Result MyString", "%s", V2Q);
	emit_output_actual_header();
	emit_param("Result MyString", "%s", actual.Value());
	if(!strings_similar(actual.Value(), V2Q, " \"")) {
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v2_quoted_result_v2() {
	emit_test("Test that getDelimitedStringV2Quoted() sets the result "
		"MyString to the expected value for an Env object using V2 format.");
	Env env;
	MyString actual, error;
	env.MergeFromV2Raw(V2R, NULL);
	env.getDelimitedStringV2Quoted(&actual, &error);
	emit_input_header();
	emit_param("Env", "%s", V2R);
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_param("Result MyString", "%s", V2Q);
	emit_output_actual_header();
	emit_param("Result MyString", "%s", actual.Value());
	if(!strings_similar(actual.Value(), V2Q, " \"")) {
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v2_quoted_result_add() {
	emit_test("Test that getDelimitedStringV2Quoted() sets the result "
		"MyString to the expected value after adding environment variables with"
		" MergeFromV2Raw().");
	Env env;
	MyString actual1, actual2, error;
	env.MergeFromV2Raw(V2R_REP, NULL);
	env.getDelimitedStringV2Quoted(&actual1, &error);
	env.MergeFromV2Raw(V2R_ADD, NULL);
	env.getDelimitedStringV2Quoted(&actual2, &error);
	emit_input_header();
	emit_param("Env", "%s", V2R_REP);
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_param("Result MyString Before", "%s", V2Q_REP);
	emit_param("Result MyString After", "%s", V2Q_REP_ADD);
	emit_output_actual_header();
	emit_param("Result MyString Before", "%s", actual1.Value());
	emit_param("Result MyString After", "%s", actual2.Value());
	if(!strings_similar(actual1.Value(), V2Q_REP, " \"") ||
		!strings_similar(actual2.Value(), V2Q_REP_ADD, " \"")) 
	{
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v2_quoted_result_replace() {
	emit_test("Test that getDelimitedStringV2Quoted() sets the result "
		"MyString to the expected value after replacing environment variables "
		"with MergeFromV2Raw().");
	Env env;
	MyString actual1, actual2, error;
	env.MergeFromV2Raw(V2R, NULL);
	env.getDelimitedStringV2Quoted(&actual1, &error);
	env.MergeFromV2Raw(V2R_REP, NULL);
	env.getDelimitedStringV2Quoted(&actual2, &error);
	emit_input_header();
	emit_param("Env", "%s", V2R);
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_param("Result MyString Before", "%s", V2Q);
	emit_param("Result MyString After", "%s", V2Q_REP);
	emit_output_actual_header();
	emit_param("Result MyString Before", "%s", actual1.Value());
	emit_param("Result MyString After", "%s", actual2.Value());
	if(!strings_similar(actual1.Value(), V2Q, " \"") ||
		!strings_similar(actual2.Value(), V2Q_REP, " \"")) 
	{
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v2_quoted_result_add_replace() {
	emit_test("Test that getDelimitedStringV2Quoted() sets the result "
		"MyString to the expected value after adding and replacing environment "
		"variables with MergeFromV2Raw().");
	Env env;
	MyString actual1, actual2, error;
	env.MergeFromV2Raw(V2R, NULL);
	env.getDelimitedStringV2Quoted(&actual1, &error);
	env.MergeFromV2Raw(V2R_REP_ADD, NULL);
	env.getDelimitedStringV2Quoted(&actual2, &error);
	emit_input_header();
	emit_param("Env", "%s", V2R);
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_param("Result MyString Before", "%s", V2Q);
	emit_param("Result MyString After", "%s", V2Q_REP_ADD);
	emit_output_actual_header();
	emit_param("Result MyString Before", "%s", actual1.Value());
	emit_param("Result MyString After", "%s", actual2.Value());
	if(!strings_similar(actual1.Value(), V2Q, " \"") ||
		!strings_similar(actual2.Value(), V2Q_REP_ADD, " \"")) 
	{
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v1r_or_v2q_return_empty() {
	emit_test("Test that getDelimitedStringV1RawOrV2Quoted() returns true for"
		" an empty Env object.");
	Env env;
	MyString result, error;
	bool expect = true;
	bool actual = env.getDelimitedStringV1RawOrV2Quoted(&result, &error);
	emit_input_header();
	emit_param("Env", "%s", "");
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v1r_or_v2q_return_v1() {
	emit_test("Test that getDelimitedStringV1RawOrV2Quoted() returns true for"
		" an Env object using V1 format.");
	Env env;
	MyString result, error;
	env.MergeFromV1Raw(V1R, NULL);
	bool expect = true;
	bool actual = env.getDelimitedStringV1RawOrV2Quoted(&result, &error);
	emit_input_header();
	emit_param("Env", "%s", V1R);
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v1r_or_v2q_return_v2() {
	emit_test("Test that getDelimitedStringV1RawOrV2Quoted() returns true for"
		" an Env object using V2 format.");
	Env env;
	MyString result, error;
	env.MergeFromV2Raw(V2R, NULL);
	bool expect = true;
	bool actual = env.getDelimitedStringV1RawOrV2Quoted(&result, &error);
	emit_input_header();
	emit_param("Env", "%s", V2R);
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v1r_or_v2q_result_empty() {
	emit_test("Test that getDelimitedStringV1RawOrV2Quoted() sets the result "
		"MyString to the expected value for an empty Env object.");
	Env env;
	MyString actual, error;
	env.getDelimitedStringV1RawOrV2Quoted(&actual, &error);
	emit_input_header();
	emit_param("Env", "%s", "");
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_param("Result MyString", "%s", EMPTY);
	emit_output_actual_header();
	emit_param("Result MyString", "%s", actual.Value());
	if(actual != EMPTY) {
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v1r_or_v2q_result_v1() {
	emit_test("Test that getDelimitedStringV1RawOrV2Quoted() sets the result "
		"MyString to the expected value for an Env object using V1 format.");
	Env env;
	MyString actual, error;
	env.MergeFromV1Raw(V1R, NULL);
	env.getDelimitedStringV1RawOrV2Quoted(&actual, &error);
	emit_input_header();
	emit_param("Env", "%s", V1R);
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_param("Result MyString", "%s", V1R);
	emit_output_actual_header();
	emit_param("Result MyString", "%s", actual.Value());
	if(!strings_similar(actual.Value(), V1R, V1_ENV_DELIM)) {
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v1r_or_v2q_result_v2() {
	emit_test("Test that getDelimitedStringV1RawOrV2Quoted() sets the result "
		"MyString to the expected value for an Env object using V2Raw format.");
	Env env;
	MyString actual, actual2, error;
	env.MergeFromV2Raw(V2R_SEMI, NULL);
	env.getDelimitedStringForDisplay(&actual2);
	env.getDelimitedStringV1RawOrV2Quoted(&actual, &error);
	emit_input_header();
	emit_param("Env", "%s", V2R_SEMI);
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_param("Result MyString", "%s", V2Q_SEMI);
	emit_output_actual_header();
	emit_param("Display MyString", "%s", actual2.Value());
	emit_param("Result MyString", "%s", actual.Value());
	if(!strings_similar(actual.Value(), V2Q_SEMI, "\" ")) {
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v1r_or_v2q_result_add() {
	emit_test("Test that getDelimitedStringV1RawOrV2Quoted() sets the result "
		"MyString to the expected value after adding environment variables with"
		" MergeFromV1Raw().");
	Env env;
	MyString actual1, actual2, error;
	env.MergeFromV1Raw(V1R_REP, NULL);
	env.getDelimitedStringV1RawOrV2Quoted(&actual1, &error);
	env.MergeFromV1Raw(V1R_ADD, NULL);
	env.getDelimitedStringV1RawOrV2Quoted(&actual2, &error);
	emit_input_header();
	emit_param("Env", "%s", V1R_REP);
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_param("Result MyString Before", "%s", V1R_REP);
	emit_param("Result MyString After", "%s", V1R_REP_ADD);
	emit_output_actual_header();
	emit_param("Result MyString Before", "%s", actual1.Value());
	emit_param("Result MyString After", "%s", actual2.Value());
	if(!strings_similar(actual1.Value(), V1R_REP, V1_ENV_DELIM) ||
		!strings_similar(actual2.Value(), V1R_REP_ADD, V1_ENV_DELIM)) 
	{
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v1r_or_v2q_result_replace() {
	emit_test("Test that getDelimitedStringV1RawOrV2Quoted() sets the result "
		"MyString to the expected value after replacing environment variables "
		"with MergeFromV1Raw().");
	Env env;
	MyString actual1, actual2, error;
	env.MergeFromV1Raw(V1R, NULL);
	env.getDelimitedStringV1RawOrV2Quoted(&actual1, &error);
	env.MergeFromV1Raw(V1R_REP, NULL);
	env.getDelimitedStringV1RawOrV2Quoted(&actual2, &error);
	emit_input_header();
	emit_param("Env", "%s", V1R);
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_param("Result MyString Before", "%s", V1R);
	emit_param("Result MyString After", "%s", V1R_REP);
	emit_output_actual_header();
	emit_param("Result MyString Before", "%s", actual1.Value());
	emit_param("Result MyString After", "%s", actual2.Value());
	if(!strings_similar(actual1.Value(), V1R, V1_ENV_DELIM) ||
		!strings_similar(actual2.Value(), V1R_REP, V1_ENV_DELIM)) 
	{
		FAIL;
	}
	PASS;
}

static bool test_get_delim_str_v1r_or_v2q_result_add_replace() {
	emit_test("Test that getDelimitedStringV1RawOrV2Quoted() sets the result "
		"MyString to the expected value after adding and replacing environment "
		"variables with MergeFromV1Raw().");
	Env env;
	MyString actual1, actual2, error;
	env.MergeFromV1Raw(V1R, NULL);
	env.getDelimitedStringV1RawOrV2Quoted(&actual1, &error);
	env.MergeFromV1Raw(V1R_REP_ADD, NULL);
	env.getDelimitedStringV1RawOrV2Quoted(&actual2, &error);
	emit_input_header();
	emit_param("Env", "%s", V1R);
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_param("Result MyString Before", "%s", V1R);
	emit_param("Result MyString After", "%s", V1R_REP_ADD);
	emit_output_actual_header();
	emit_param("Result MyString Before", "%s", actual1.Value());
	emit_param("Result MyString After", "%s", actual2.Value());
	if(!strings_similar(actual1.Value(), V1R, V1_ENV_DELIM) ||
		!strings_similar(actual2.Value(), V1R_REP_ADD, V1_ENV_DELIM)) 
	{
		FAIL;
	}
	PASS;
}

static bool test_get_string_array_empty() {
	emit_test("Test that getStringArray() returns the expected string array "
		"for an empty Env object.");
	Env env;
	char** result = env.getStringArray();
	emit_input_header();
	emit_param("Env", "%s", "");
	emit_output_expected_header();
	emit_param("array[0] == NULL", "%s", "TRUE");
	emit_output_actual_header();
	emit_param("array[0] == NULL", "%s", 
		tfstr(result != NULL && *result == NULL));
	if(result == NULL || *result != NULL) {
		deleteStringArray(result);
		FAIL;
	}
	deleteStringArray(result); 
	PASS;
}

static bool test_get_string_array_v1() {
	emit_test("Test that getStringArray() returns the expected string array "
		"for an Env object using V1 format.");
	Env env;
	env.MergeFromV1Raw(V1R, NULL);
	char** result = env.getStringArray();
	MyString* actual = convert_string_array(result, 3);
	emit_input_header();
	emit_param("Env", "%s", V1R);
	emit_output_expected_header();
	emit_retval("%s", V2R);
	emit_output_actual_header();
	emit_retval("%s", actual->Value());
	if(!strings_similar(actual->Value(), V2R)) {
		deleteStringArray(result); delete actual;
		FAIL;
	}
	deleteStringArray(result); delete actual;
	PASS;
}

static bool test_get_string_array_v2() {
	emit_test("Test that getStringArray() returns the expected string array "
		"for an Env object using V2 format.");
	Env env;
	env.MergeFromV2Raw(V2R, NULL);
	char** result = env.getStringArray();
	MyString* actual = convert_string_array(result, 3);
	emit_input_header();
	emit_param("Env", "%s", V2R);
	emit_output_expected_header();
	emit_retval("%s", V2R);
	emit_output_actual_header();
	emit_retval("%s", actual->Value());
	if(!strings_similar(actual->Value(), V2R)) {
		deleteStringArray(result); delete actual;
		FAIL;
	}
	deleteStringArray(result); delete actual;
	PASS;
}

static bool test_get_string_array_add() {
	emit_test("Test that getStringArray() returns the expected string array "
		"after adding environment variables with MergeFromV2Raw().");
	Env env;
	env.MergeFromV2Raw(V2R_REP, NULL);
	char** result1 = env.getStringArray();
	env.MergeFromV2Raw(V2R_ADD, NULL);
	char** result2 = env.getStringArray();
	MyString* actual1 = convert_string_array(result1, 3);
	MyString* actual2 = convert_string_array(result2, 5);
	emit_input_header();
	emit_param("Env", "%s", V2R_REP);
	emit_output_expected_header();
	emit_param("String Array Before", "%s", V2R_REP);
	emit_param("String Array After", "%s", V2R_REP_ADD);
	emit_output_actual_header();
	emit_param("String Array Before", "%s", actual1->Value());
	emit_param("String Array After", "%s", actual2->Value());
	if(!strings_similar(actual1->Value(), V2R_REP) || 
		!(strings_similar(actual2->Value(), V2R_REP_ADD))) 
	{
		deleteStringArray(result1); deleteStringArray(result2);
		delete actual1; delete actual2;
		FAIL;
	}
	deleteStringArray(result1); deleteStringArray(result2);
	delete actual1; delete actual2;
	PASS;
}

static bool test_get_string_array_replace() {
	emit_test("Test that getStringArray() returns the expected string array "
		"after replacing environment variables with MergeFromV2Raw().");
	Env env;
	env.MergeFromV2Raw(V2R, NULL);
	char** result1 = env.getStringArray();
	env.MergeFromV2Raw(V2R_REP, NULL);
	char** result2 = env.getStringArray();
	MyString* actual1 = convert_string_array(result1, 3);
	MyString* actual2 = convert_string_array(result2, 3);
	emit_input_header();
	emit_param("Env", "%s", V2R);
	emit_output_expected_header();
	emit_param("String Array Before", "%s", V2R);
	emit_param("String Array After", "%s", V2R_REP);
	emit_output_actual_header();
	emit_param("String Array Before", "%s", actual1->Value());
	emit_param("String Array After", "%s", actual2->Value());
	if(!strings_similar(actual1->Value(), V2R) || 
		!(strings_similar(actual2->Value(), V2R_REP))) 
	{
		deleteStringArray(result1); deleteStringArray(result2);
		delete actual1; delete actual2;
		FAIL;
	}
	deleteStringArray(result1); deleteStringArray(result2);
	delete actual1; delete actual2;
	PASS;
}

static bool test_get_string_array_add_replace() {
	emit_test("Test that getStringArray() returns the expected string array "
		"after adding and replacing environment variables with "
		"MergeFromV2Raw().");
	Env env;
	env.MergeFromV2Raw(V2R, NULL);
	char** result1 = env.getStringArray();
	env.MergeFromV2Raw(V2R_REP_ADD, NULL);
	char** result2 = env.getStringArray();
	MyString* actual1 = convert_string_array(result1, 3);
	MyString* actual2 = convert_string_array(result2, 5);
	emit_input_header();
	emit_param("Env", "%s", V2R);
	emit_output_expected_header();
	emit_param("String Array Before", "%s", V2R);
	emit_param("String Array After", "%s", V2R_REP_ADD);
	emit_output_actual_header();
	emit_param("String Array Before", "%s", actual1->Value());
	emit_param("String Array After", "%s", actual2->Value());
	if(!strings_similar(actual1->Value(), V2R) || 
		!(strings_similar(actual2->Value(), V2R_REP_ADD))) 
	{
		deleteStringArray(result1); deleteStringArray(result2);
		delete actual1; delete actual2;
		FAIL;
	}
	deleteStringArray(result1); deleteStringArray(result2);
	delete actual1; delete actual2;
	PASS;
}

static bool test_get_env_bool_return_empty_empty() {
	emit_test("Test that getEnv() returns false for an empty Env object when "
		"passed an empty variable and value.");
	Env env;
	bool expect = false;
	MyString val;
	const bool actual = env.GetEnv(EMPTY, val);
	emit_input_header();
	emit_param("Env", "%s", "");
	emit_param("Variable", "%s", EMPTY);
	emit_param("Value", "%s", val.Value());
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_get_env_bool_return_empty_not() {
	emit_test("Test that getEnv() returns false for an empty Env object when "
		"passed a non-existent variable.");
	Env env;
	MyString var("one"), val;
	bool expect = false;
	bool actual = env.GetEnv(var, val);
	emit_input_header();
	emit_param("Env", "%s", "");
	emit_param("Variable", "%s", var.Value());
	emit_param("Value", "%s", val.Value());
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_get_env_bool_return_hit_one() {
	emit_test("Test that getEnv() returns true for an Env object when "
		"passed a correct variable.");
	Env env;
	env.MergeFromV2Raw(V2R, NULL);
	MyString var("one"), val("1");
	bool expect = true;
	bool actual = env.GetEnv(var, val);
	emit_input_header();
	emit_param("Env", "%s", V2R);
	emit_param("Variable", "%s", var.Value());
	emit_param("Value", "%s", "");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_get_env_bool_return_hit_all() {
	emit_test("Test that getEnv() returns true for an Env object when "
		"passed a correct variable name for each variable within the "
		"Env.");
	Env env;
	env.MergeFromV2Raw(V2R, NULL);
	MyString var1("one"), var2("two"), var3("three"), 
		val1("1"), val2("2"), val3("3");
	bool expect = true;
	bool actual = env.GetEnv(var1, val1) && env.GetEnv(var2, val2) &&
		env.GetEnv(var3, val3);
	emit_input_header();
	emit_param("Env", "%s", V2R);
	emit_param("Variable", "%s", var1.Value());
	emit_param("Value", "%s", "");
	emit_param("Variable", "%s", var2.Value());
	emit_param("Value", "%s", "");
	emit_param("Variable", "%s", var3.Value());
	emit_param("Value", "%s", "");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_get_env_bool_value_miss() {
	emit_test("Test that getEnv() doesn't change the value MyString when the "
		"variable does not exist.");
	Env env;
	env.MergeFromV2Raw(V2R, NULL);
	MyString var("miss"), val("1");
	env.GetEnv(var, val);
	emit_input_header();
	emit_param("Env", "%s", V2R);
	emit_param("Variable", "%s", var.Value());
	emit_param("Value", "%s", "1");
	emit_output_expected_header();
	emit_param("Value", "%s", "1");
	emit_output_actual_header();
	emit_param("Value", "%s", val.Value());
	if(val != "1") {
		FAIL;
	}
	PASS;
}

static bool test_get_env_bool_value_hit_one() {
	emit_test("Test that getEnv() sets the value MyString to the expected "
		"value for a variable that exists.");
	Env env;
	env.MergeFromV2Raw(V2R, NULL);
	MyString var("one"), val;
	env.GetEnv(var, val);
	emit_input_header();
	emit_param("Env", "%s", V2R);
	emit_param("Variable", "%s", var.Value());
	emit_param("Value", "%s", "");
	emit_output_expected_header();
	emit_param("Value", "%s", "1");
	emit_output_actual_header();
	emit_param("Value", "%s", val.Value());
	if(val != "1") {
		FAIL;
	}
	PASS;
}

static bool test_get_env_bool_value_hit_all() {
	emit_test("Test that getEnv() sets the value MyString to the expected "
		"value for each variable within the Env.");
	Env env;
	env.MergeFromV2Raw(V2R, NULL);
	MyString var1("one"), var2("two"), var3("three"), val1, val2, val3,
		expect1("1"), expect2("2"), expect3("3");
	env.GetEnv(var1, val1);
	env.GetEnv(var2, val2);
	env.GetEnv(var3, val3);
	emit_input_header();
	emit_param("Env", "%s", V2R);
	emit_param("Variable", "%s", var1.Value());
	emit_param("Value", "%s", "");
	emit_param("Variable", "%s", var2.Value());
	emit_param("Value", "%s", "");
	emit_param("Variable", "%s", var3.Value());
	emit_param("Value", "%s", "");
	emit_output_expected_header();
	emit_param("Value", "%s", expect1.Value());
	emit_param("Value", "%s", expect2.Value());
	emit_param("Value", "%s", expect3.Value());
	emit_output_actual_header();
	emit_param("Value", "%s", val1.Value());
	emit_param("Value", "%s", val2.Value());
	emit_param("Value", "%s", val3.Value());
	if(val1 != expect1 || val2 != expect2 || val3 != expect3) {
		FAIL;
	}
	PASS;
}

static bool test_is_safe_env_v1_value_false_null() {
	emit_test("Test that IsSafeEnvV1Value() returns false for a NULL "
		"string.");
	Env env;
	bool expect = false;
	bool actual = env.IsSafeEnvV1Value(NULL);
	emit_input_header();
	emit_param("STRING", "%s", "NULL");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_is_safe_env_v1_value_false_semi() {
	emit_test("Test that IsSafeEnvV1Value() returns false for a string "
		"with a semicolon.");
	Env env;
	bool expect = false;
	bool actual = env.IsSafeEnvV1Value("semi=" V1_ENV_DELIM);
	emit_input_header();
	emit_param("STRING", "%s", "semi=" V1_ENV_DELIM);
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_is_safe_env_v1_value_false_newline() {
	emit_test("Test that IsSafeEnvV1Value() returns false for a string "
		"with a newline character.");
	Env env;
	bool expect = false;
	bool actual = env.IsSafeEnvV1Value("newline=\n");
	emit_input_header();
	emit_param("STRING", "%s", "newline=\n");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_is_safe_env_v1_value_false_param() {
	emit_test("Test that IsSafeEnvV1Value() returns false for a string "
		"with the passed delimiter.");
	Env env;
	bool expect = false;
	bool actual = env.IsSafeEnvV1Value("star=*", '*');
	emit_input_header();
	emit_param("STRING", "%s", "star=*");
	emit_param("CHAR", "%c", '*');
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_is_safe_env_v1_value_true_one() {
	emit_test("Test that IsSafeEnvV1Value() returns true for a valid V1 "
		"format string.");
	Env env;
	bool expect = true;
	bool actual = env.IsSafeEnvV1Value(ONE);
	emit_input_header();
	emit_param("STRING", "%s", ONE);
	emit_param("CHAR", "%c", '*');
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_is_safe_env_v1_value_true_quotes() {
	emit_test("Test that IsSafeEnvV1Value() returns true for a string "
		"surrounded in quotes.");
	Env env;
	bool expect = true;
	bool actual = env.IsSafeEnvV1Value("\"one=1\"");
	emit_input_header();
	emit_param("STRING", "%s", "\"one=1\"");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_is_safe_env_v2_value_false_null() {
	emit_test("Test that IsSafeEnvV2Value() returns false for a NULL "
		"string.");
	Env env;
	bool expect = false;
	bool actual = env.IsSafeEnvV2Value(NULL);
	emit_input_header();
	emit_param("STRING", "%s", "NULL");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_is_safe_env_v2_value_false_newline() {
	emit_test("Test that IsSafeEnvV2Value() returns false for a string with "
		"a newline character.");
	Env env;
	bool expect = false;
	bool actual = env.IsSafeEnvV2Value("newline=\n");
	emit_input_header();
	emit_param("STRING", "%s", "newline=\"");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_is_safe_env_v2_value_true_one() {
	emit_test("Test that IsSafeEnvV2Value() returns true for a valid V2 "
		"format string with only one environment variable.");
	Env env;
	bool expect = true;
	bool actual = env.IsSafeEnvV2Value(ONE);
	emit_input_header();
	emit_param("STRING", "%s", ONE);
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_is_safe_env_v2_value_true_many() {
	emit_test("Test that IsSafeEnvV2Value() returns true for a valid V2 "
		"format string with many environment variables.");
	Env env;
	bool expect = true;
	bool actual = env.IsSafeEnvV2Value(V2R_SEMI);
	emit_input_header();
	emit_param("STRING", "%s", V2R_SEMI);
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_get_env_v1_delim_param_winnt() {
	emit_test("Test that GetEnvV1Delimiter() returns a '|' for an Env "
		"when passed a string beginning with \"WINNT\".");
	Env env;
	char expect = '|';
	char actual = env.GetEnvV1Delimiter("WINNT");
	emit_input_header();
	emit_param("Env", "%s", "");
	emit_param("STRING", "%s", "WINNT");
	emit_output_expected_header();
	emit_retval("%c", expect);
	emit_output_actual_header();
	emit_retval("%c", actual);
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_get_env_v1_delim_param_win32() {
	emit_test("Test that GetEnvV1Delimiter() returns a '|' for an Env "
		"when passed a string beginning with \"WIN32\".");
	Env env;
	char expect = '|';
	char actual = env.GetEnvV1Delimiter("WIN32");
	emit_input_header();
	emit_param("Env", "%s", "");
	emit_param("STRING", "%s", "WIN32");
	emit_output_expected_header();
	emit_retval("%c", expect);
	emit_output_actual_header();
	emit_retval("%c", actual);
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_get_env_v1_delim_param_unix() {
	emit_test("Test that GetEnvV1Delimiter() returns a semicolon for an Env "
		"when passed a string beginning with \"UNIX\".");
	Env env;
	char expect = ';';
	char actual = env.GetEnvV1Delimiter("UNIX");
	emit_input_header();
	emit_param("Env", "%s", "");
	emit_param("STRING", "%s", "UNIX");
	emit_output_expected_header();
	emit_retval("%c", expect);
	emit_output_actual_header();
	emit_retval("%c", actual);
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_is_v2_quoted_string_false_v1() {
	emit_test("Test that IsV2QuotedString() returns false for a V1 format "
		"string that doesn't begin or end with quotes.");
	bool expect = false;
	bool actual = Env::IsV2QuotedString(V1R);
	emit_input_header();
	emit_param("STRING", "%s", V1R);
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_is_v2_quoted_string_false_v2() {
	emit_test("Test that IsV2QuotedString() returns false for a V2 format "
		"string that doesn't begin or end with quotes.");
	bool expect = false;
	bool actual = Env::IsV2QuotedString(V2R);
	emit_input_header();
	emit_param("STRING", "%s", V2R);
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_is_v2_quoted_string_true() {
	emit_test("Test that IsV2QuotedString() returns true for a V2 format "
		"string that begins and ends with quotes.");
	bool expect = true;
	bool actual = Env::IsV2QuotedString(V2Q);
	emit_input_header();
	emit_param("STRING", "%s", V2Q);
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_v2_quoted_to_v2_raw_return_false_miss_end() {
	emit_test("Test that V2QuotedToV2Raw() returns false for an invalid V2 "
		"quoted string due to missing quotes at the end.");
	MyString result, error;
	bool expect = false;
	bool actual = Env::V2QuotedToV2Raw(V2Q_MISS_END, &result, &error);
	emit_input_header();
	emit_param("STRING", "%s", V2Q_MISS_END);
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_v2_quoted_to_v2_raw_return_false_trail() {
	emit_test("Test that V2QuotedToV2Raw() returns false for an invalid V2 "
		"quoted string due to trailing characters after the quotes.");
	MyString result, error;
	bool expect = false;
	bool actual = Env::V2QuotedToV2Raw(V2Q_TRAIL, &result, &error);
	emit_input_header();
	emit_param("STRING", "%s", V2Q_TRAIL);
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_v2_quoted_to_v2_raw_return_true() {
	emit_test("Test that V2QuotedToV2Raw() returns true for a valid V2 "
		"quoted string.");
	MyString result, error;
	bool expect = true;
	bool actual = Env::V2QuotedToV2Raw(V2Q, &result, &error);
	emit_input_header();
	emit_param("STRING", "%s", V2Q);
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_v2_quoted_to_v2_raw_return_true_semi() {
	emit_test("Test that V2QuotedToV2Raw() returns true for a V2 "
		"quoted string that uses a semicolon as a delimiter.");
	MyString result, error;
	bool expect = true;
	bool actual = Env::V2QuotedToV2Raw(V2Q_DELIM_SEMI, &result, &error);
	emit_input_header();
	emit_param("STRING", "%s", V2Q_DELIM_SEMI);
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_v2_quoted_to_v2_raw_error_miss_end() {
	emit_test("Test that V2QuotedToV2Raw() sets an error message for an "
		"invalid V2 quoted string due to missing quotes at the end.");
	emit_comment("This test just checks if the error message is not empty.");
	MyString result, error;
	Env::V2QuotedToV2Raw(V2Q_MISS_END, &result, &error);
	emit_input_header();
	emit_param("STRING", "%s", V2Q_MISS_END);
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_actual_header();
	emit_param("Error MyString", "%s", error.Value());
	if(error.IsEmpty()) {
		FAIL;
	}
	PASS;
}

static bool test_v2_quoted_to_v2_raw_error_trail() {
	emit_test("Test that V2QuotedToV2Raw() sets an error message for an "
		"invalid V2 quoted string due to trailing characters after the "
		"quotes.");
	emit_comment("This test just checks if the error message is not empty.");
	MyString result, error;
	Env::V2QuotedToV2Raw(V2Q_TRAIL, &result, &error);
	emit_input_header();
	emit_param("STRING", "%s", V2Q_TRAIL);
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_actual_header();
	emit_param("Error MyString", "%s", error.Value());
	if(error.IsEmpty()) {
		FAIL;
	}
	PASS;
}

static bool test_v2_quoted_to_v2_raw_result() {
	emit_test("Test that V2QuotedToV2Raw() sets the result MyString to the "
		"expected value for a valid V2 quoted string.");
	MyString result, error;
	Env::V2QuotedToV2Raw(V2Q, &result, &error);
	emit_input_header();
	emit_param("STRING", "%s", V2Q);
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_param("Result MyString", "%s", V2R);
	emit_output_actual_header();
	emit_param("Result MyString", "%s", result.Value());
	if(!strings_similar(result.Value(), V2R)) {
		FAIL;
	}
	PASS;
}

static bool test_v2_quoted_to_v2_raw_result_semi() {
	emit_test("Test that V2QuotedToV2Raw() sets the result MyString to the "
		"expected value for a  V2 quoted string that uses a semicolon as a "
		"delimiter.");
	MyString result, error;
	Env::V2QuotedToV2Raw(V2Q_DELIM_SEMI, &result, &error);
	emit_input_header();
	emit_param("STRING", "%s", V2Q_DELIM_SEMI);
	emit_param("MyString", "%s", "");
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_param("Result MyString", "%s", V1R);
	emit_output_actual_header();
	emit_param("Result MyString", "%s", result.Value());
	if(!strings_similar(result.Value(), V1R, V1_ENV_DELIM)) {
		FAIL;
	}
	PASS;
}

static bool test_input_was_v1_false_empty() {
	emit_test("Test that InputWasV1() returns false for an empty Env "
		"object.");
	Env env;
	bool expect = false;
	bool actual = env.InputWasV1();
	emit_input_header();
	emit_param("Env", "%s", "");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_input_was_v1_false_v2q_or() {
	emit_test("Test that InputWasV1() returns false for an Env object "
		"created from a V2Quoted string with MergeFromV1RawOrV2Quoted().");
	Env env;
	env.MergeFromV1RawOrV2Quoted(V2Q, NULL);
	bool expect = false;
	bool actual = env.InputWasV1();
	emit_input_header();
	emit_param("Env", "%s", V2Q);
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_input_was_v1_false_v2q() {
	emit_test("Test that InputWasV1() returns false for an Env object "
		"created from a V2Quoted string with MergeFromV2Quoted().");
	Env env;
	env.MergeFromV2Quoted(V2Q, NULL);
	bool expect = false;
	bool actual = env.InputWasV1();
	emit_input_header();
	emit_param("Env", "%s", V2Q);
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_input_was_v1_false_v2r_or() {
	emit_test("Test that InputWasV1() returns false for an Env object "
		"created from a V2Raw string with MergeFromV1or2Raw().");
	Env env;
	env.MergeFromV1or2Raw(V2R_MARK, NULL);
	bool expect = false;
	bool actual = env.InputWasV1();
	emit_input_header();
	emit_param("Env", "%s", V2R_MARK);
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_input_was_v1_false_v2r() {
	emit_test("Test that InputWasV1() returns false for an Env object "
		"created from a V2Raw string with MergeFromV2Raw().");
	Env env;
	env.MergeFromV2Raw(V2R, NULL);
	bool expect = false;
	bool actual = env.InputWasV1();
	emit_input_header();
	emit_param("Env", "%s", V2R);
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_input_was_v1_false_array() {
	emit_test("Test that InputWasV1() returns false for an Env object "
		"created from a string array with MergeFrom().");
	Env env;
	env.MergeFrom(ARRAY);
	bool expect = false;
	bool actual = env.InputWasV1();
	emit_input_header();
	emit_param("Env", "%s", V2R);
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_input_was_v1_false_str() {
	emit_test("Test that InputWasV1() returns false for an Env object "
		"created from a string with MergeFrom().");
	Env env;
	env.MergeFrom(V2R);
	bool expect = false;
	bool actual = env.InputWasV1();
	emit_input_header();
	emit_param("Env", "%s", V2R); 
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_input_was_v1_false_env() {
	emit_test("Test that InputWasV1() returns false for an Env object "
		"created from another Env object with MergeFrom().");
	Env env1, env2;
	env2.MergeFromV2Raw(V2R, NULL);
	env1.MergeFrom(env2);
	bool expect = false;
	bool actual = env2.InputWasV1();
	emit_input_header();
	emit_param("Env", "%s", V2R); 
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_input_was_v1_false_ad() {
	emit_test("Test that InputWasV1() returns false for an Env object "
		"created from a ClassAd that uses a V2 environment with MergeFrom().");
	ClassAd classad;
	initAdFromString(AD_V2, classad);
	Env env;
	env.MergeFrom(&classad, NULL);
	bool expect = false;
	bool actual = env.InputWasV1();
	emit_input_header();
	emit_param("ClassAd", "%s", AD_V2); 
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_input_was_v1_true_v1r_or() {
	emit_test("Test that InputWasV1() returns true for an Env object "
		"created from V1Raw string that with MergeFromV1RawOrV2Quoted().");
	Env env;
	env.MergeFromV1RawOrV2Quoted(V1R, NULL);
	bool expect = true;
	bool actual = env.InputWasV1();
	emit_input_header();
	emit_param("ClassAd", "%s", V1R); 
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_input_was_v1_true_v1r() {
	emit_test("Test that InputWasV1() returns true for an Env object "
		"created from V1Raw string that with MergeFromV1Raw().");
	Env env;
	env.MergeFromV1Raw(V1R, NULL);
	bool expect = true;
	bool actual = env.InputWasV1();
	emit_input_header();
	emit_param("ClassAd", "%s", V1R); 
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

static bool test_input_was_v1_true_ad() {
	emit_test("Test that InputWasV1() returns true for an Env object "
		"created from a ClassAd that uses a V1 environment with MergeFrom().");
	ClassAd classad;
	initAdFromString(AD_V1, classad);
	Env env;
	env.MergeFrom(&classad, NULL);
	bool expect = true;
	bool actual = env.InputWasV1();
	emit_input_header();
	emit_param("ClassAd", "%s", AD_V1); 
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(actual));
	if(actual != expect) {
		FAIL;
	}
	PASS;
}

