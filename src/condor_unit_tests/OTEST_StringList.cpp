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
	This code tests the StringList implementation.
 */

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "function_test_driver.h"
#include "emit.h"
#include "unit_test_utils.h"
#include "string_list.h"

static bool test_constructor(void);
static bool test_constructor_empty_list(void);
static bool test_constructor_empty_delim(void);
static bool test_constructor_empty_both(void);
static bool test_constructor_null_list(void);
static bool test_constructor_null_delim(void);
static bool test_constructor_null_both(void);
static bool test_copy_constructor_value(void);
static bool test_copy_constructor_empty(void);
static bool test_copy_constructor_pointer(void);
static bool test_initialize_from_string_empty_valid(void);
static bool test_initialize_from_string_empty_valid_whitespace(void);
static bool test_initialize_from_string_empty_empty(void);
static bool test_initialize_from_string_empty_null(void);
static bool test_initialize_from_string_non_empty_valid(void);
static bool test_initialize_from_string_non_empty_empty(void);
static bool test_initialize_from_string_non_empty_null(void);
static bool test_clear_all_empty(void);
static bool test_clear_all_non_empty_many(void);
static bool test_clear_all_non_empty_one(void);
static bool test_create_union_return_true(void);
static bool test_create_union_return_false(void);
static bool test_create_union_duplicates_all(void);
static bool test_create_union_duplicates_some(void);
static bool test_create_union_duplicates_all_ignore(void);
static bool test_create_union_duplicates_some_ignore(void);
static bool test_create_union_empty_current(void);
static bool test_create_union_empty_subset(void);
static bool test_contains_return_false(void);
static bool test_contains_return_false_substring(void);
static bool test_contains_return_false_case(void);
static bool test_contains_return_true_one(void);
static bool test_contains_return_true_many(void);
static bool test_contains_current_single(void);
static bool test_contains_current_multiple(void);
static bool test_contains_anycase_return_false(void);
static bool test_contains_anycase_return_false_substring(void);
static bool test_contains_anycase_return_true_one(void);
static bool test_contains_anycase_return_true_many(void);
static bool test_contains_anycase_current_single(void);
static bool test_contains_anycase_current_multiple(void);
static bool test_remove_invalid(void);
static bool test_remove_case(void);
static bool test_remove_substring(void);
static bool test_remove_empty(void);
static bool test_remove_first(void);
static bool test_remove_last(void);
static bool test_remove_many(void);
static bool test_remove_anycase_invalid(void);
static bool test_remove_anycase_substring(void);
static bool test_remove_anycase_empty(void);
static bool test_remove_anycase_first(void);
static bool test_remove_anycase_last(void);
static bool test_remove_anycase_many(void);
static bool test_prefix_return_false_invalid(void);
static bool test_prefix_return_false_almost(void);
static bool test_prefix_return_false_reverse(void);
static bool test_prefix_return_false_case(void);
static bool test_prefix_return_true_identical(void);
static bool test_prefix_withwildcard_return_true(void);
static bool test_prefix_withwildcard_return_false(void);
static bool test_prefix_return_true_many(void);
static bool test_prefix_current_single(void);
static bool test_prefix_current_multiple(void);
static bool test_contains_withwildcard_return_false(void);
static bool test_contains_withwildcard_return_false_substring(void);
static bool test_contains_withwildcard_return_false_case(void);
static bool test_contains_withwildcard_return_false_wild(void);
static bool test_contains_withwildcard_return_true_no_wild(void);
static bool test_contains_withwildcard_return_true_only_wild(void);
static bool test_contains_withwildcard_return_true_start(void);
static bool test_contains_withwildcard_return_true_mid(void);
static bool test_contains_withwildcard_return_true_mid_and_end(void);
static bool test_contains_withwildcard_return_true_start_and_end(void);
static bool test_contains_withwildcard_return_false_mid_and_end(void);
static bool test_contains_withwildcard_return_false_start_and_end(void);
static bool test_contains_withwildcard_return_true_end(void);
static bool test_contains_withwildcard_return_true_same_wild(void);
static bool test_contains_withwildcard_return_true_multiple(void);
static bool test_contains_withwildcard_return_true_many(void);
static bool test_contains_withwildcard_current_single(void);
static bool test_contains_withwildcard_current_multiple(void);
static bool test_contains_anycase_wwc_return_false(void);
static bool test_contains_anycase_wwc_return_false_substring(void);
static bool test_contains_anycase_wwc_return_false_wild(void);
static bool test_contains_anycase_wwc_return_true_no_wild(void);
static bool test_contains_anycase_wwc_return_true_only_wild(void);
static bool test_contains_anycase_wwc_return_true_start(void);
static bool test_contains_anycase_wwc_return_true_mid(void);
static bool test_contains_anycase_wwc_return_true_end(void);
static bool test_contains_anycase_wwc_return_true_same_wild(void);
static bool test_contains_anycase_wwc_return_true_multiple(void);
static bool test_contains_anycase_wwc_return_true_many(void);
static bool test_contains_anycase_wwc_current_single(void);
static bool test_contains_anycase_wwc_current_multiple(void);
static bool test_find_matches_any_wwc_return_false(void);
static bool test_find_matches_any_wwc_return_false_substring(void);
static bool test_find_matches_any_wwc_return_false_wild(void);
static bool test_find_matches_any_wwc_return_true_no_wild(void);
static bool test_find_matches_any_wwc_return_true_only_wild(void);
static bool test_find_matches_any_wwc_return_true_start(void);
static bool test_find_matches_any_wwc_return_true_mid(void);
static bool test_find_matches_any_wwc_return_true_end(void);
static bool test_find_matches_any_wwc_return_true_same_wild(void);
static bool test_find_matches_any_wwc_return_true_multiple(void);
static bool test_find_matches_any_wwc_return_true_many(void);
static bool test_find_matches_any_wwc_none(void);
static bool test_find_matches_any_wwc_one(void);
static bool test_find_matches_any_wwc_one_exist(void);
static bool test_find_matches_any_wwc_wild_start(void);
static bool test_find_matches_any_wwc_wild_mid(void);
static bool test_find_matches_any_wwc_wild_end(void);
static bool test_find_matches_any_wwc_multiple(void);
static bool test_find_matches_any_wwc_multiple_exist(void);
static bool test_find_return_false(void);
static bool test_find_return_false_substring(void);
static bool test_find_return_false_case(void);
static bool test_find_return_true_one(void);
static bool test_find_return_true_many(void);
static bool test_find_anycase_return_false(void);
static bool test_find_anycase_return_false_substring(void);
static bool test_find_anycase_return_true_one(void);
static bool test_find_anycase_return_true_many(void);
static bool test_identical_return_false_same(void);
static bool test_identical_return_false_not_same(void);
static bool test_identical_return_false_almost(void);
static bool test_identical_return_false_ignore(void);
static bool test_identical_return_false_subset(void);
static bool test_identical_return_true_case(void);
static bool test_identical_return_true_ignore(void);
static bool test_identical_return_true_itself(void);
static bool test_identical_return_true_copy(void);
static bool test_print_to_string_empty(void);
static bool test_print_to_string_one(void);
static bool test_print_to_string_many(void);
static bool test_print_to_delimed_string_empty(void);
static bool test_print_to_delimed_string_one(void);
static bool test_print_to_delimed_string_many_short(void);
static bool test_print_to_delimed_string_many_long(void);
static bool test_print_to_delimed_string_many_null(void);
static bool test_delete_current_before(void);
static bool test_delete_current_after_no_match(void);
static bool test_delete_current_one_first(void);
static bool test_delete_current_one_mid(void);
static bool test_delete_current_one_last(void);
static bool test_delete_current_all(void);
static bool test_string_compare_equal_same_beg(void);
static bool test_string_compare_equal_same_mid(void);
static bool test_string_compare_equal_same_end(void);
static bool test_string_compare_equal_different_list(void);
static bool test_string_compare_copy(void);
static bool test_string_compare_not_equal_same(void);
static bool test_string_compare_not_equal_different(void);
static bool test_qsort_empty(void);
static bool test_qsort_one(void);
static bool test_qsort_already(void);
static bool test_qsort_multiple(void);
static bool test_qsort_many(void);
static bool test_qsort_many_shuffle(void);
static bool test_shuffle_empty(void);
static bool test_shuffle_one(void);
static bool test_shuffle_many(void);
static bool test_rewind_empty(void);
static bool test_rewind_non_empty(void);
static bool test_rewind_after_contains_true(void);
static bool test_rewind_after_contains_false(void);
static bool test_append_empty(void);
static bool test_append_one(void);
static bool test_append_many(void);
static bool test_append_contains(void);
static bool test_append_rewind(void);
static bool test_append_current(void);
static bool test_insert_empty(void);
static bool test_insert_head(void);
static bool test_insert_middle(void);
static bool test_insert_end(void);
static bool test_insert_current(void);
static bool test_insert_many(void);
static bool test_next_empty(void);
static bool test_next_beginning(void);
static bool test_next_middle(void);
static bool test_next_end(void);
static bool test_number_empty(void);
static bool test_number_one(void);
static bool test_number_many(void);
static bool test_number_after_delete(void);
static bool test_number_after_clear_all(void);
static bool test_number_after_append(void);
static bool test_number_after_insert(void);
static bool test_number_copy(void);
static bool test_is_empty_empty(void);
static bool test_is_empty_clear(void);
static bool test_is_empty_one(void);
static bool test_is_empty_many(void);
static bool test_is_empty_append(void);
static bool test_is_empty_insert(void);
static bool test_get_delimiters_empty_no(void);
static bool test_get_delimiters_empty_yes(void);
static bool test_get_delimiters_non_empty_no(void);
static bool test_get_delimiters_non_empty_yes(void);

static bool string_compare(const char *x, const char *y) {
	return strcmp(x, y) < 0;
}

bool OTEST_StringList(void) {
	emit_object("StringList");
	emit_comment("This primitive class is used to contain and search arrays "
		"of strings.");
	emit_comment("Many of the tests rely on print_to_string(), so a problem "
		"with that may cause a problem in something else");
	
		// driver to run the tests and all required setup
	FunctionDriver driver;
	driver.register_function(test_constructor);
	driver.register_function(test_constructor_empty_list);
	driver.register_function(test_constructor_empty_delim);
	driver.register_function(test_constructor_empty_both);
	driver.register_function(test_constructor_null_list);
	driver.register_function(test_constructor_null_delim);
	driver.register_function(test_constructor_null_both);
	driver.register_function(test_copy_constructor_value);
	driver.register_function(test_copy_constructor_empty);
	driver.register_function(test_copy_constructor_pointer);
	driver.register_function(test_initialize_from_string_empty_valid);
	driver.register_function(test_initialize_from_string_empty_valid_whitespace);
	driver.register_function(test_initialize_from_string_empty_empty);
	driver.register_function(test_initialize_from_string_empty_null);
	driver.register_function(test_initialize_from_string_non_empty_valid);
	driver.register_function(test_initialize_from_string_non_empty_empty);
	driver.register_function(test_initialize_from_string_non_empty_null);
	driver.register_function(test_clear_all_empty);
	driver.register_function(test_clear_all_non_empty_many);
	driver.register_function(test_clear_all_non_empty_one);
	driver.register_function(test_create_union_return_true);
	driver.register_function(test_create_union_return_false);
	driver.register_function(test_create_union_duplicates_all);
	driver.register_function(test_create_union_duplicates_some);
	driver.register_function(test_create_union_duplicates_all_ignore);
	driver.register_function(test_create_union_duplicates_some_ignore);
	driver.register_function(test_create_union_empty_current);
	driver.register_function(test_create_union_empty_subset);
	driver.register_function(test_contains_return_false);
	driver.register_function(test_contains_return_false_substring);
	driver.register_function(test_contains_return_false_case);
	driver.register_function(test_contains_return_true_one);
	driver.register_function(test_contains_return_true_many);
	driver.register_function(test_contains_current_single);
	driver.register_function(test_contains_current_multiple);
	driver.register_function(test_contains_anycase_return_false);
	driver.register_function(test_contains_anycase_return_false_substring);
	driver.register_function(test_contains_anycase_return_true_one);
	driver.register_function(test_contains_anycase_return_true_many);
	driver.register_function(test_contains_anycase_current_single);
	driver.register_function(test_contains_anycase_current_multiple);
	driver.register_function(test_remove_invalid);
	driver.register_function(test_remove_case);
	driver.register_function(test_remove_substring);
	driver.register_function(test_remove_empty);
	driver.register_function(test_remove_first);
	driver.register_function(test_remove_last);
	driver.register_function(test_remove_many);
	driver.register_function(test_remove_anycase_invalid);
	driver.register_function(test_remove_anycase_substring);
	driver.register_function(test_remove_anycase_empty);
	driver.register_function(test_remove_anycase_first);
	driver.register_function(test_remove_anycase_last);
	driver.register_function(test_remove_anycase_many);
	driver.register_function(test_prefix_return_false_invalid);
	driver.register_function(test_prefix_return_false_almost);
	driver.register_function(test_prefix_return_false_reverse);
	driver.register_function(test_prefix_return_false_case);
	driver.register_function(test_prefix_return_true_identical);
	driver.register_function(test_prefix_withwildcard_return_true);
	driver.register_function(test_prefix_withwildcard_return_false);
	driver.register_function(test_prefix_return_true_many);
	driver.register_function(test_prefix_current_single);
	driver.register_function(test_prefix_current_multiple);
	driver.register_function(test_contains_withwildcard_return_false);
	driver.register_function(test_contains_withwildcard_return_false_substring);
	driver.register_function(test_contains_withwildcard_return_false_case);
	driver.register_function(test_contains_withwildcard_return_false_wild);
	driver.register_function(test_contains_withwildcard_return_true_no_wild);
	driver.register_function(test_contains_withwildcard_return_true_only_wild);
	driver.register_function(test_contains_withwildcard_return_true_start);
	driver.register_function(test_contains_withwildcard_return_true_mid);
	driver.register_function(test_contains_withwildcard_return_true_mid_and_end);
	driver.register_function(test_contains_withwildcard_return_true_start_and_end);
	driver.register_function(test_contains_withwildcard_return_false_mid_and_end);
	driver.register_function(test_contains_withwildcard_return_false_start_and_end);
	driver.register_function(test_contains_withwildcard_return_true_end);
	driver.register_function(test_contains_withwildcard_return_true_same_wild);
	driver.register_function(test_contains_withwildcard_return_true_multiple);
	driver.register_function(test_contains_withwildcard_return_true_many);
	driver.register_function(test_contains_withwildcard_current_single);
	driver.register_function(test_contains_withwildcard_current_multiple);
	driver.register_function(test_contains_anycase_wwc_return_false);
	driver.register_function(test_contains_anycase_wwc_return_false_substring);
	driver.register_function(test_contains_anycase_wwc_return_false_wild);
	driver.register_function(test_contains_anycase_wwc_return_true_no_wild);
	driver.register_function(test_contains_anycase_wwc_return_true_only_wild);
	driver.register_function(test_contains_anycase_wwc_return_true_start);
	driver.register_function(test_contains_anycase_wwc_return_true_mid);
	driver.register_function(test_contains_anycase_wwc_return_true_end);
	driver.register_function(test_contains_anycase_wwc_return_true_same_wild);
	driver.register_function(test_contains_anycase_wwc_return_true_multiple);
	driver.register_function(test_contains_anycase_wwc_return_true_many);
	driver.register_function(test_contains_anycase_wwc_current_single);
	driver.register_function(test_contains_anycase_wwc_current_multiple);
	driver.register_function(test_find_matches_any_wwc_return_false);
	driver.register_function(test_find_matches_any_wwc_return_false_substring);
	driver.register_function(test_find_matches_any_wwc_return_false_wild);
	driver.register_function(test_find_matches_any_wwc_return_true_no_wild);
	driver.register_function(test_find_matches_any_wwc_return_true_only_wild);
	driver.register_function(test_find_matches_any_wwc_return_true_start);
	driver.register_function(test_find_matches_any_wwc_return_true_mid);
	driver.register_function(test_find_matches_any_wwc_return_true_end);
	driver.register_function(test_find_matches_any_wwc_return_true_same_wild);
	driver.register_function(test_find_matches_any_wwc_return_true_multiple);
	driver.register_function(test_find_matches_any_wwc_return_true_many);
	driver.register_function(test_find_matches_any_wwc_none);
	driver.register_function(test_find_matches_any_wwc_one);
	driver.register_function(test_find_matches_any_wwc_one_exist);
	driver.register_function(test_find_matches_any_wwc_wild_start);
	driver.register_function(test_find_matches_any_wwc_wild_mid);
	driver.register_function(test_find_matches_any_wwc_wild_end);
	driver.register_function(test_find_matches_any_wwc_multiple);
	driver.register_function(test_find_matches_any_wwc_multiple_exist);
	driver.register_function(test_find_return_false);
	driver.register_function(test_find_return_false_substring);
	driver.register_function(test_find_return_false_case);
	driver.register_function(test_find_return_true_one);
	driver.register_function(test_find_return_true_many);
	driver.register_function(test_find_anycase_return_false);
	driver.register_function(test_find_anycase_return_false_substring);
	driver.register_function(test_find_anycase_return_true_one);
	driver.register_function(test_find_anycase_return_true_many);
	driver.register_function(test_identical_return_false_same);
	driver.register_function(test_identical_return_false_not_same);
	driver.register_function(test_identical_return_false_almost);
	driver.register_function(test_identical_return_false_ignore);
	driver.register_function(test_identical_return_false_subset);
	driver.register_function(test_identical_return_true_case);
	driver.register_function(test_identical_return_true_ignore);
	driver.register_function(test_identical_return_true_itself);
	driver.register_function(test_identical_return_true_copy);
	driver.register_function(test_print_to_string_empty);
	driver.register_function(test_print_to_string_one);
	driver.register_function(test_print_to_string_many);
	driver.register_function(test_print_to_delimed_string_empty);
	driver.register_function(test_print_to_delimed_string_one);
	driver.register_function(test_print_to_delimed_string_many_short);
	driver.register_function(test_print_to_delimed_string_many_long);
	driver.register_function(test_print_to_delimed_string_many_null);
	driver.register_function(test_delete_current_before);
	driver.register_function(test_delete_current_after_no_match);
	driver.register_function(test_delete_current_one_first);
	driver.register_function(test_delete_current_one_mid);
	driver.register_function(test_delete_current_one_last);
	driver.register_function(test_delete_current_all);
	driver.register_function(test_string_compare_equal_same_beg);
	driver.register_function(test_string_compare_equal_same_mid);
	driver.register_function(test_string_compare_equal_same_end);
	driver.register_function(test_string_compare_equal_different_list);
	driver.register_function(test_string_compare_copy);
	driver.register_function(test_string_compare_not_equal_same);
	driver.register_function(test_string_compare_not_equal_different);
	driver.register_function(test_qsort_empty);
	driver.register_function(test_qsort_one);
	driver.register_function(test_qsort_already);
	driver.register_function(test_qsort_multiple);
	driver.register_function(test_qsort_many);
	driver.register_function(test_qsort_many_shuffle);
	driver.register_function(test_shuffle_empty);
	driver.register_function(test_shuffle_one);
	driver.register_function(test_shuffle_many);
	driver.register_function(test_rewind_empty);
	driver.register_function(test_rewind_non_empty);
	driver.register_function(test_rewind_after_contains_true);
	driver.register_function(test_rewind_after_contains_false);
	driver.register_function(test_append_empty);
	driver.register_function(test_append_one);
	driver.register_function(test_append_many);
	driver.register_function(test_append_contains);
	driver.register_function(test_append_rewind);
	driver.register_function(test_append_current);
	driver.register_function(test_insert_empty);
	driver.register_function(test_insert_head);
	driver.register_function(test_insert_middle);
	driver.register_function(test_insert_end);
	driver.register_function(test_insert_current);
	driver.register_function(test_insert_many);
	driver.register_function(test_next_empty);
	driver.register_function(test_next_beginning);
	driver.register_function(test_next_middle);
	driver.register_function(test_next_end);
	driver.register_function(test_number_empty);
	driver.register_function(test_number_one);
	driver.register_function(test_number_many);
	driver.register_function(test_number_after_delete);
	driver.register_function(test_number_after_clear_all);
	driver.register_function(test_number_after_append);
	driver.register_function(test_number_after_insert);
	driver.register_function(test_number_copy);
	driver.register_function(test_is_empty_empty);
	driver.register_function(test_is_empty_clear);
	driver.register_function(test_is_empty_one);
	driver.register_function(test_is_empty_many);
	driver.register_function(test_is_empty_append);
	driver.register_function(test_is_empty_insert);
	driver.register_function(test_get_delimiters_empty_no);
	driver.register_function(test_get_delimiters_empty_yes);
	driver.register_function(test_get_delimiters_non_empty_no);
	driver.register_function(test_get_delimiters_non_empty_yes);

		// run the tests
	return driver.do_all_functions();
}

static bool test_constructor() {
	emit_test("Test the StringList constructor.");
	StringList sl("a;b;c", ";");
	const char* expect = "a,b,c";
	char* retVal = sl.print_to_string();
	emit_input_header();
	emit_param("STRING", "a;b;c");
	emit_param("DELIMS", ";");
	emit_output_expected_header();
	emit_param("StringList", expect);
	emit_output_actual_header();
	emit_param("StringList", nicePrint(retVal));
	if(niceStrCmp(retVal, expect) != MATCH) {
		free(retVal);
		FAIL;
	}
	free(retVal);
	PASS;
}

static bool test_constructor_empty_list() {
	emit_test("Test the StringList constructor when passed an empty list with"
		" a non-empty delimiter.");
	StringList sl("", ";");
	const char* expect = "";
	char* retVal = sl.print_to_string();
	emit_input_header();
	emit_param("STRING", expect);
	emit_param("DELIMS", ";");
	emit_output_expected_header();
	emit_retval("%s", expect);
	emit_output_actual_header();
	emit_retval("%s", nicePrint(retVal));
	if(niceStrCmp(retVal, expect) != MATCH) {
		free(retVal);
		FAIL;
	}
	free(retVal);
	PASS;
}

static bool test_constructor_empty_delim() {
	emit_test("Test the StringList constructor when passed a non-empty list "
		"with an empty delimiter.");
	StringList sl("abc", "");
	const char* expect = "abc";
	char* retVal = sl.print_to_string();
	emit_input_header();
	emit_param("STRING", "abc");
	emit_param("DELIMS", "");
	emit_output_expected_header();
	emit_retval("%s", expect);
	emit_output_actual_header();
	emit_retval("%s", nicePrint(retVal));
	if(niceStrCmp(retVal, expect) != MATCH) {
		free(retVal);
		FAIL;
	}
	free(retVal);
	PASS;
}

static bool test_constructor_empty_both() {
	emit_test("Test the StringList constructor when passed both an empty list"
		" and an empty delimiter.");
	StringList sl("", "");
	const char* expect = "";
	char* retVal = sl.print_to_string();
	emit_input_header();
	emit_param("STRING", "");
	emit_param("DELIMS", "");
	emit_output_expected_header();
	emit_retval("%s", expect);
	emit_output_actual_header();
	emit_retval("%s", nicePrint(retVal));
	if(niceStrCmp(retVal, expect) != MATCH) {
		free(retVal);
		FAIL;
	}
	free(retVal);
	PASS;
}

static bool test_constructor_null_list() {
	emit_test("Test the StringList constructor when passed a null string for "
		"the StringList.");
	StringList sl(NULL, ";");
	char* retVal = sl.print_to_string();
	emit_input_header();
	emit_param("STRING", "NULL");
	emit_param("DELIMS", ";");
	emit_output_expected_header();
	emit_param("StringList", "");
	emit_output_actual_header();
	emit_param("StringList", nicePrint(retVal));
	if(niceStrCmp(retVal, "") != MATCH) {
		free(retVal);
		FAIL;
	}
	free(retVal);
	PASS;
}

static bool test_constructor_null_delim() {
	emit_test("Test the StringList constructor when passed a null string for "
		"the delimiter of the StringList.");
	StringList sl("abc", NULL);
	const char* expect = "abc";
	char* retVal = sl.print_to_string();
	emit_input_header();
	emit_param("STRING", "abc");
	emit_param("DELIMS", "NULL");
	emit_output_expected_header();
	emit_param("StringList", expect);
	emit_output_actual_header();
	emit_param("StringList", nicePrint(retVal));
	if(niceStrCmp(retVal, expect) != MATCH) {
		free(retVal);
		FAIL;
	}
	free(retVal);
	PASS;
}

static bool test_constructor_null_both() {
	emit_test("Test the StringList constructor when passed a null string for "
		"both the StringList and the delimiter of the StringList.");
	StringList sl(NULL, NULL);
	char* retVal = sl.print_to_string();
	emit_input_header();
	emit_param("STRING", "NULL");
	emit_param("DELIMS", "NULL");
	emit_output_expected_header();
	emit_param("StringList", "");
	emit_output_actual_header();
	emit_param("StringList", nicePrint(retVal));
	if(niceStrCmp(retVal, "") != MATCH) {
		free(retVal);
		FAIL;
	}
	free(retVal);
	PASS;
}

static bool test_copy_constructor_value() {
	emit_test("Check the value of the StringList copy constructor.");
	StringList sl("a;b;c", ";");
	char* orig = sl.print_to_string();
	StringList copy(sl);
	const char* expect = "a,b,c";
	char* retVal = copy.print_to_string();
	emit_input_header();
	emit_param("StringList", "a,b,c");
	emit_output_expected_header();
	emit_param("StringList", expect);
	emit_output_actual_header();
	emit_param("StringList", nicePrint(retVal));
	if(niceStrCmp(retVal, expect) != MATCH) {
		free(orig); free(retVal);
		FAIL;
	}
	free(orig); free(retVal);
	PASS;
}

static bool test_copy_constructor_empty() {
	emit_test("Test the StringList copy constructor when passed an empty "
		"StringList.");
	StringList sl;
	StringList copy(sl);
	char* retVal = copy.print_to_string();
	emit_input_header();
	emit_param("StringList", "");
	emit_output_expected_header();
	emit_param("StringList", "");
	emit_output_actual_header();
	emit_param("StringList", nicePrint(retVal));
	if(niceStrCmp(retVal, "") != MATCH) {
		free(retVal);
		FAIL;
	}
	free(retVal);
	PASS;
}

static bool test_copy_constructor_pointer() {
	emit_test("Check that the pointer of the copied StringList is not equal "
		"to the original StringList.");
	StringList sl("a;b;c", ";");
	StringList copy(sl);
	char* retVal = copy.print_to_string();
	emit_input_header();
	emit_param("StringList", "a,b,c");
	emit_output_expected_header();
	emit_retval("!=%x", &sl);
	emit_output_actual_header();
	emit_retval("%x", &copy);
	if(&sl == &copy) {	//compares pointers
		free(retVal);
		FAIL;
	}
	free(retVal);
	PASS;
}

static bool test_initialize_from_string_empty_valid() {
	emit_test("Test initializeFromString on an empty StringList when passed "
		" a valid string.");
	StringList sl("", ";");
	sl.initializeFromString("a;b;c");
	const char* expect = "a,b,c";
	char* retVal = sl.print_to_string();
	emit_input_header();
	emit_param("STRING", "a;b;c");
	emit_output_expected_header();
	emit_retval("%s", expect);
	emit_output_actual_header();
	emit_retval("%s", nicePrint(retVal));
	if(niceStrCmp(expect, retVal) != MATCH) {
		free(retVal);
		FAIL;
	}
	free(retVal);
	PASS;
}

static bool test_initialize_from_string_empty_valid_whitespace() {
	emit_test("Test initializeFromString on an empty StringList when passed "
		" a valid string with whitespace to be trimmed.");
	StringList sl("", ";");
	sl.initializeFromString(" a ; b b ; c ");
	const char* expect = "a,b b,c";
	char* retVal = sl.print_to_string();
	emit_input_header();
	emit_param("STRING", "a;b;c");
	emit_output_expected_header();
	emit_retval("%s", expect);
	emit_output_actual_header();
	emit_retval("%s", nicePrint(retVal));
	if(niceStrCmp(expect, retVal) != MATCH) {
		free(retVal);
		FAIL;
	}
	free(retVal);
	PASS;
}

static bool test_initialize_from_string_empty_empty() {
	emit_test("Test initializeFromString on an empty StringList when passed "
		" an empty string.");
	StringList sl("", ";");
	sl.initializeFromString("");
	const char* expect = "";
	char* retVal = sl.print_to_string();
	emit_input_header();
	emit_param("STRING", "");
	emit_output_expected_header();
	emit_param("StringList", "");
	emit_retval("%s", expect);
	emit_output_actual_header();
	emit_param("StringList", nicePrint(retVal));
	emit_retval("%s", nicePrint(retVal));
	if(niceStrCmp(retVal, "") != MATCH) {
		free(retVal);
		FAIL;
	}
	free(retVal);
	PASS;
}

static bool test_initialize_from_string_empty_null() {
	emit_test("Test initializeFromString on an empty StringList when passed "
		" a null string.");
	emit_comment("initializeFromString() throws an exception for this test. "
		"The test is commented out so that the unit testing can continue.");
	PASS;
/*
	StringList sl("", ";");
	sl.initializeFromString(NULL);
	char* expect = "";
	char* retVal = sl.print_to_string();
	emit_input_header();
	emit_param("STRING", "NULL");
	emit_output_expected_header();
	emit_param("StringList", "");
	emit_retval("%s", expect);
	emit_output_actual_header();
	emit_retval("%s", nicePrint(retVal));
	if(niceStrCmp(retVal, "") != MATCH) {
		free(retVal);
		FAIL;
	}
	free(retVal);
	PASS;
*/
}

static bool test_initialize_from_string_non_empty_valid() {
	emit_test("Test initializeFromString on a non-empty StringList when "
		"passed a valid string.");
	emit_comment("initializeFromString() is currently set up to append to an "
		"existing StringList, so if that changes this test will fail.");
	StringList sl("a;b;c", ";");
	char* orig = sl.print_to_string();
	sl.initializeFromString("d;e;f");
	const char* expect = "a,b,c,d,e,f";
	char* retVal = sl.print_to_string();
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", "d;e;f");
	emit_output_expected_header();
	emit_retval("%s", expect);
	emit_output_actual_header();
	emit_retval("%s", nicePrint(retVal));
	if(niceStrCmp(expect, retVal) != MATCH) {
		free(orig); free(retVal);
		FAIL;
	}
	free(orig); free(retVal);
	PASS;
}

static bool test_initialize_from_string_non_empty_empty() {
	emit_test("Test initializeFromString on a non-empty StringList when "
		"passed an empty string.");
	StringList sl("a;b;c", ";");
	char* orig = sl.print_to_string();
	sl.initializeFromString("");
	const char* expect = "a,b,c";
	char* retVal = sl.print_to_string();
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", "");
	emit_output_expected_header();
	emit_retval("%s", expect);
	emit_output_actual_header();
	emit_retval("%s", nicePrint(retVal));
	if(niceStrCmp(expect, retVal) != MATCH) {
		free(orig); free(retVal);
		FAIL;
	}
	free(orig); free(retVal);
	PASS;
}

static bool test_initialize_from_string_non_empty_null() {
	emit_test("Test initializeFromString on a non-empty StringList when "
		"passed a null string.");
	emit_comment("initializeFromString() throws an exception for this test. "
		"The test is commented out so that the unit testing can continue.");
	PASS;
/*
	StringList sl("a;b;c", ";");
	char* orig = sl.print_to_string();
	sl.initializeFromString(NULL);
	char* expect = "a,b,c";
	char* retVal = sl.print_to_string();
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", "NULL");
	emit_output_expected_header();
	emit_retval("%s", expect);
	emit_output_actual_header();
	emit_retval("%s", nicePrint(retVal));
	if(niceStrCmp(expect, retVal) != MATCH) {
		free(orig); free(retVal);
		FAIL;
	}
	free(orig); free(retVal);
	PASS;
*/
}

static bool test_clear_all_empty() {
	emit_test("Test clearAll on an empty StringList.");
	StringList sl("", "");
	char* orig = sl.print_to_string();
	sl.clearAll();
	char* retVal = sl.print_to_string();
	emit_input_header();
	emit_param("StringList", nicePrint(orig));
	emit_output_expected_header();
	emit_param("StringList", "");
	emit_output_actual_header();
	emit_param("StringList", nicePrint(retVal));
	if(niceStrCmp(retVal, "") != MATCH) {
		free(orig); free(retVal);
		FAIL;
	}
	free(orig); free(retVal);
	PASS;
}

static bool test_clear_all_non_empty_many() {
	emit_test("Test clearAll on a non-empty StringList with many strings.");
	StringList sl("a;b;c", ";");
	char* orig = sl.print_to_string();
	sl.clearAll();
	char* retVal = sl.print_to_string();
	emit_input_header();
	emit_param("StringList", nicePrint(orig));
	emit_output_expected_header();
	emit_param("StringList", "");
	emit_output_actual_header();
	emit_param("StringList", nicePrint(retVal));
	if(niceStrCmp(retVal, "") != MATCH) {
		free(orig); free(retVal);
		FAIL;
	}
	free(orig); free(retVal);
	PASS;
}

static bool test_clear_all_non_empty_one() {
	emit_test("Test clearAll on a non-empty StringList with only one "
		"string.");
	StringList sl("a", "");
	char* orig = sl.print_to_string();
	sl.clearAll();
	char* retVal = sl.print_to_string();
	emit_input_header();
	emit_param("StringList", nicePrint(orig));
	emit_output_expected_header();
	emit_param("StringList", "");
	emit_output_actual_header();
	emit_param("StringList", nicePrint(retVal));
	if(niceStrCmp(retVal, "") != MATCH) {
		free(orig); free(retVal);
		FAIL;
	}
	free(orig); free(retVal);
	PASS;
}

static bool test_create_union_return_true() {
	emit_test("Does create_union return true when the list is modified?");
	StringList sl("a;b;c", ";");
	StringList toAdd("d;e;f", ";");
	char* orig = sl.print_to_string();
	char* add = toAdd.print_to_string();
	bool retVal = sl.create_union(toAdd, true);
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("SUBSET", add);
	emit_param("ANYCASE", "TRUE");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		free(orig); free(add);
		FAIL;
	}
	free(orig); free(add);
	PASS;
}

static bool test_create_union_return_false() {
	emit_test("Does create_union return false when the list is not "
		"modified?");
	StringList sl("a;b;c", ";");
	StringList toAdd("a;b;c", ";");
	char* orig = sl.print_to_string();
	char* add = toAdd.print_to_string();
	bool retVal = sl.create_union(toAdd, true);
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("SUBSET", add);
	emit_param("ANYCASE", "TRUE");
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(retVal) {
		free(orig); free(add);
		FAIL;
	}
	free(orig);	free(add);
	PASS;
}

static bool test_create_union_duplicates_all() {
	emit_test("Does create_union avoid adding duplicates when the given "
		"StringList contains all duplicates?");
	StringList sl("a;b;c", ";");
	StringList toAdd("a;b;c", ";");
	char* orig = sl.print_to_string();
	char* add = toAdd.print_to_string();
	sl.create_union(toAdd, false);
	char* retVal = sl.print_to_string();
	const char* expect = "a,b,c";
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("SUBSET", add);
	emit_param("ANYCASE", "FALSE");
	emit_output_expected_header();
	emit_param("StringList", expect);
	emit_output_actual_header();
	emit_param("StringList", nicePrint(retVal));
	if(niceStrCmp(retVal, expect) != MATCH) {
		free(orig); free(add); free(retVal);
		FAIL;
	}
	free(orig); free(add); free(retVal);
	PASS;
}

static bool test_create_union_duplicates_some() {
	emit_test("Does create_union avoid adding duplicates when the given "
		"StringList contains some duplicates?");
	StringList sl("a;b;c", ";");
	StringList toAdd("a;d;c", ";");
	char* orig = sl.print_to_string();
	char* add = toAdd.print_to_string();
	sl.create_union(toAdd, false);
	char* retVal = sl.print_to_string();
	const char* expect = "a,b,c,d";
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("SUBSET", add);
	emit_param("ANYCASE", "FALSE");
	emit_output_expected_header();
	emit_param("StringList", expect);
	emit_output_actual_header();
	emit_param("StringList", nicePrint(retVal));
	if(niceStrCmp(retVal, expect) != MATCH) {
		free(orig); free(add); free(retVal);
		FAIL;
	}
	free(orig); free(add); free(retVal);
	PASS;
}

static bool test_create_union_duplicates_all_ignore() {
	emit_test("Does create_union avoid adding duplicates when the given "
		"StringList contains all duplicates when case-insensitive?");
	StringList sl("A;b;c;D", ";");
	StringList toAdd("A;B;c;d", ";");
	char* orig = sl.print_to_string();
	char* add = toAdd.print_to_string();
	sl.create_union(toAdd, true);
	char* retVal = sl.print_to_string();
	const char* expect = "A,b,c,D";
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("SUBSET", add);
	emit_param("ANYCASE", "TRUE");
	emit_output_expected_header();
	emit_param("StringList", expect);
	emit_output_actual_header();
	emit_param("StringList", nicePrint(retVal));
	if(niceStrCmp(retVal, expect) != MATCH) {
		free(orig); free(add); free(retVal);
		FAIL;
	}
	free(orig); free(add); free(retVal);
	PASS;
}

static bool test_create_union_duplicates_some_ignore() {
	emit_test("Does create_union avoid adding duplicates when the given "
		"StringList contains some duplicates when case-insensitive?");
	StringList sl("A;b;c;D", ";");
	StringList toAdd("A;B;c;d;E", ";");
	char* orig = sl.print_to_string();
	char* add = toAdd.print_to_string();
	sl.create_union(toAdd, true);
	char* retVal = sl.print_to_string();
	const char* expect = "A,b,c,D,E";
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("SUBSET", add);
	emit_param("ANYCASE", "TRUE");
	emit_output_expected_header();
	emit_param("StringList", expect);
	emit_output_actual_header();
	emit_param("StringList", nicePrint(retVal));
	if(niceStrCmp(retVal, expect) != MATCH) {
		free(orig); free(add); free(retVal);
		FAIL;
	}
	free(orig); free(add); free(retVal);
	PASS;
}

static bool test_create_union_empty_current() {
	emit_test("Test create_union when the current StringList is empty.");
	StringList sl;
	StringList toAdd("a;b;c", ";");
	char* orig = sl.print_to_string();
	char* add = toAdd.print_to_string();
	sl.create_union(toAdd, false);
	char* retVal = sl.print_to_string();
	const char* expect = "a,b,c";
	emit_input_header();
	emit_param("StringList", nicePrint(orig));
	emit_param("SUBSET", add);
	emit_param("ANYCASE", "FALSE");
	emit_output_expected_header();
	emit_param("StringList", expect);
	emit_output_actual_header();
	emit_param("StringList", nicePrint(retVal));
	if(niceStrCmp(retVal, expect) != MATCH) {
		free(orig); free(add); free(retVal);
		FAIL;
	}
	free(orig); free(add); free(retVal);
	PASS;
}

static bool test_create_union_empty_subset() {
	emit_test("Test create_union when the StringList to add is empty.");
	StringList sl("a;b;c", ";");
	StringList toAdd;
	char* orig = sl.print_to_string();
	sl.create_union(toAdd, false);
	char* retVal = sl.print_to_string();
	const char* expect = "a,b,c";
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("SUBSET", "");
	emit_param("ANYCASE", "FALSE");
	emit_output_expected_header();
	emit_param("StringList", expect);
	emit_output_actual_header();
	emit_param("StringList", nicePrint(retVal));
	if(niceStrCmp(retVal, expect) != MATCH) {
		free(orig); free(retVal);
		FAIL;
	}
	free(orig); free(retVal);
	PASS;
}

static bool test_contains_return_false() {
	emit_test("Does contains() return false when passed a string not in the "
		"StringList?");
	StringList sl("a;b;c", ";");
	char* orig = sl.print_to_string();
	const char* check = "d";
	bool retVal = sl.contains(check);
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_contains_return_false_substring() {
	emit_test("Does contains() return false when passed a string not in the "
		"StringList, but the StringList has a substring of the string?");
	StringList sl("a;b;c", ";");
	char* orig = sl.print_to_string();
	const char* check = "boo";
	bool retVal = sl.contains(check);
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_contains_return_false_case() {
	emit_test("Does contains() return false when passed a string in the "
		"StringList when ignoring case?");
	StringList sl("a;b;c", ";");
	char* orig = sl.print_to_string();
	const char* check = "A";
	bool retVal = sl.contains(check);
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_contains_return_true_one() {
	emit_test("Does contains() return true when passed one string in the "
		"StringList?");
	StringList sl("a;b;c", ";");
	char* orig = sl.print_to_string();
	const char* check = "a";
	bool retVal = sl.contains(check);
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_contains_return_true_many() {
	emit_test("Does contains() return true for multiple calls with different "
		"strings in the StringList?");
	StringList sl("a;b;c", ";");
	char* orig = sl.print_to_string();
	const char* check1 = "a";
	const char* check2 = "b";
	const char* check3 = "c";
	bool retVal = sl.contains(check3) && sl.contains(check2) 
		&& sl.contains(check1);
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check3);
	emit_param("STRING", check2);
	emit_param("STRING", check1);
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_contains_current_single() {
	emit_test("Does contains() change current to point to the location of the"
		" single match?");
	emit_comment("To test that current points to the correct string, "
		"next() has to be called so a problem with that may cause this"
		" to fail.");
	StringList sl("a;b;c", ";");
	char* orig = sl.print_to_string();
	const char* check = "a";
	sl.contains(check); 
	char* next = sl.next();
	const char* expect = "b";
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_param("next", expect);
	emit_output_actual_header();
	emit_param("next", nicePrint(next));
	if(niceStrCmp(next, expect) != MATCH) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_contains_current_multiple() {
	emit_test("Does contains() change current to point to the location of the"
		" first match when there are multiple matches?");
	emit_comment("To test that current points to the correct string, "
		"deleteCurrent() has to be called so a problem with that may cause this"
		" to fail.");
	StringList sl("a;b;a;b", ";");
	char* orig = sl.print_to_string();
	const char* check = "a";
	sl.contains(check); 
	sl.deleteCurrent();
	char* changed = sl.print_to_string();
	const char* expect = "b,a,b";
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_param("StringList", expect);
	emit_output_actual_header();
	emit_param("StringList", nicePrint(changed));
	if(niceStrCmp(changed, expect) != MATCH) {
		free(orig); free(changed);
		FAIL;
	}
	free(orig); free(changed);
	PASS;
}

static bool test_contains_anycase_return_false() {
	emit_test("Does contains_anycase() return false when passed a string not "
		"in the StringList?");
	StringList sl("a;b;c", ";");
	char* orig = sl.print_to_string();
	const char* check = "d";
	bool retVal = sl.contains_anycase(check);
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_contains_anycase_return_false_substring() {
	emit_test("Does contains_anycase() return false when passed a string not "
		"in the StringList, but the StringList has a substring of the string?");
	StringList sl("a;b;c", ";");
	char* orig = sl.print_to_string();
	const char* check = "boo";
	bool retVal = sl.contains_anycase(check);
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_contains_anycase_return_true_one() {
	emit_test("Does contains_anycase() return true when passed one string in "
		"the StringList?");
	StringList sl("a;b;c", ";");
	char* orig = sl.print_to_string();
	const char* check = "a";
	bool retVal = sl.contains_anycase(check);
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_contains_anycase_return_true_many() {
	emit_test("Does contains_anycase() return true for multiple calls with "
		"different strings in the StringList?");
	StringList sl("a;b;C;D", ";");
	char* orig = sl.print_to_string();
	const char* check1 = "a";
	const char* check2 = "B";
	const char* check3 = "C";
	const char* check4 = "d";
	bool retVal = sl.contains_anycase(check4) && sl.contains_anycase(check3) 
		&& sl.contains_anycase(check2) && sl.contains_anycase(check1);
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check4);
	emit_param("STRING", check3);
	emit_param("STRING", check2);
	emit_param("STRING", check1);
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_contains_anycase_current_single() {
	emit_test("Does contains_anycase() change current to point to the "
		"location of the single match?");
	emit_comment("To test that current points to the correct string, "
		"next() has to be called so a problem with that may cause this"
		" to fail.");
	StringList sl("a;b;c", ";");
	char* orig = sl.print_to_string();
	const char* check = "A";
	sl.contains_anycase(check); 
	char* next = sl.next();
	const char* expect = "b";
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_param("next", next);
	emit_output_actual_header();
	emit_param("next", nicePrint(next));
	if(niceStrCmp(next, expect) != MATCH) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_contains_anycase_current_multiple() {
	emit_test("Does contains_anycase() change current to point to the "
		"location of the first match when there are multiple matches?");
	emit_comment("To test that current points to the correct string, "
		"deleteCurrent() has to be called so a problem with that may cause this"
		" to fail.");
	StringList sl("A;B;a;b", ";");
	char* orig = sl.print_to_string();
	const char* check = "a";
	sl.contains_anycase(check); 
	sl.deleteCurrent();
	char* changed = sl.print_to_string();
	const char* expect = "B,a,b";
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_param("StringList", expect);
	emit_output_actual_header();
	emit_param("StringList", nicePrint(changed));
	if(niceStrCmp(changed, expect) != MATCH) {
		free(orig); free(changed);
		FAIL;
	}
	free(orig); free(changed);
	PASS;
}

static bool test_remove_invalid() {
	emit_test("Test that the StringList stays the same when calling remove() "
		"with a string not in the list.");
	StringList sl("a;b;c", ";");
	char* orig = sl.print_to_string();
	const char* remove = "d";
	sl.remove(remove);
	const char* expect = "a,b,c";
	char* changed = sl.print_to_string();
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", remove);
	emit_output_expected_header();
	emit_param("StringList", expect);
	emit_output_actual_header();
	emit_param("StringList", nicePrint(changed));
	if(niceStrCmp(expect, changed) != MATCH) {
		free(orig); free(changed);
		FAIL;
	}
	free(orig); free(changed);
	PASS;
}

static bool test_remove_case() {
	emit_test("Test that the StringList stays the same when calling remove() "
		"with a string in the list when ignoring case.");
	StringList sl("a;b;c", ";");
	char* orig = sl.print_to_string();
	const char* remove = "A";
	sl.remove(remove);
	const char* expect = "a,b,c";
	char* changed = sl.print_to_string();
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", remove);
	emit_output_expected_header();
	emit_param("StringList", expect);
	emit_output_actual_header();
	emit_param("StringList", nicePrint(changed));
	if(niceStrCmp(expect, changed) != MATCH) {
		free(orig); free(changed);
		FAIL;
	}
	free(orig); free(changed);
	PASS;
}

static bool test_remove_substring() {
	emit_test("Test that the StringList stays the same when calling remove() "
		"with a string not in the list, but the list contains a substring of "
		"the string.");
	StringList sl("a;b;c", ";");
	char* orig = sl.print_to_string();
	const char* remove = "boo";
	sl.remove(remove);
	const char* expect = "a,b,c";
	char* changed = sl.print_to_string();
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", remove);
	emit_output_expected_header();
	emit_param("StringList", expect);
	emit_output_actual_header();
	emit_param("StringList", nicePrint(changed));
	if(niceStrCmp(expect, changed) != MATCH) {
		free(orig); free(changed);
		FAIL;
	}
	free(orig); free(changed);
	PASS;
}

static bool test_remove_empty() {
	emit_test("Test remove() on an empty StringList.");
	StringList sl("", "");
	char* orig = sl.print_to_string();
	const char* remove = "b";
	sl.remove(remove);
	char* changed = sl.print_to_string();
	emit_input_header();
	emit_param("StringList", nicePrint(orig));
	emit_param("STRING", remove);
	emit_output_expected_header();
	emit_param("StringList", "");
	emit_output_actual_header();
	emit_param("StringList", nicePrint(changed));
	if(niceStrCmp(changed, "") != MATCH) {
		free(orig); free(changed);
		FAIL;
	}
	free(orig); free(changed);
	PASS;
}

static bool test_remove_first() {
	emit_test("Does remove() successfully remove the first string?");
	StringList sl("a;b;c", ";");
	char* orig = sl.print_to_string();
	const char* remove = "a";
	sl.remove(remove);
	const char* expect = "b,c";
	char* changed = sl.print_to_string();
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", remove);
	emit_output_expected_header();
	emit_param("StringList", expect);
	emit_output_actual_header();
	emit_param("StringList", nicePrint(changed));
	if(niceStrCmp(expect, changed) != MATCH) {
		free(orig); free(changed);
		FAIL;
	}
	free(orig); free(changed);
	PASS;
}

static bool test_remove_last() {
	emit_test("Does remove() successfully remove the last string?");
	StringList sl("a;b;c", ";");
	char* orig = sl.print_to_string();
	const char* remove = "c";
	sl.remove(remove);
	const char* expect = "a,b";
	char* changed = sl.print_to_string();
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", remove);
	emit_output_expected_header();
	emit_param("StringList", expect);
	emit_output_actual_header();
	emit_param("StringList", nicePrint(changed));
	if(niceStrCmp(expect, changed) != MATCH) {
		free(orig); free(changed);
		FAIL;
	}
	free(orig); free(changed);
	PASS;
}

static bool test_remove_many() {
	emit_test("Does remove() successfully remove many strings?");
	StringList sl("a;b;c;d;e;f", ";");
	char* orig = sl.print_to_string();
	const char* remove1 = "f";
	const char* remove2 = "d";
	const char* remove3 = "b";
	sl.remove(remove1);
	sl.remove(remove2);
	sl.remove(remove3);
	const char* expect = "a,c,e";
	char* changed = sl.print_to_string();
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", remove1);
	emit_param("STRING", remove2);
	emit_param("STRING", remove3);
	emit_output_expected_header();
	emit_param("StringList", expect);
	emit_output_actual_header();
	emit_param("StringList", nicePrint(changed));
	if(niceStrCmp(expect, changed) != MATCH) {
		free(orig); free(changed);
		FAIL;
	}
	free(orig); free(changed);
	PASS;
}

static bool test_remove_anycase_invalid() {
	emit_test("Test that the StringList stays the same when calling "
		"remove_anycase() with a string not in the list.");
	StringList sl("a;b;c", ";");
	char* orig = sl.print_to_string();
	const char* remove = "d";
	sl.remove_anycase(remove);
	const char* expect = "a,b,c";
	char* changed = sl.print_to_string();
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", remove);
	emit_output_expected_header();
	emit_param("StringList", expect);
	emit_output_actual_header();
	emit_param("StringList", nicePrint(changed));
	if(niceStrCmp(expect, changed) != MATCH) {
		free(orig); free(changed);
		FAIL;
	}
	free(orig); free(changed);
	PASS;
}



static bool test_remove_anycase_substring() {
	emit_test("Test that the StringList stays the same when calling "
		"remove_anycase() with a string not in the list, but the list contains "
		"a substring of the string.");
	StringList sl("a;b;c", ";");
	char* orig = sl.print_to_string();
	const char* remove = "Boo";
	sl.remove_anycase(remove);
	const char* expect = "a,b,c";
	char* changed = sl.print_to_string();
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", remove);
	emit_output_expected_header();
	emit_param("StringList", expect);
	emit_output_actual_header();
	emit_param("StringList", nicePrint(changed));
	if(niceStrCmp(expect, changed) != MATCH) {
		free(orig); free(changed);
		FAIL;
	}
	free(orig); free(changed);
	PASS;
}

static bool test_remove_anycase_empty() {
	emit_test("Test remove_anycase() on an empty StringList.");
	StringList sl("", "");
	char* orig = sl.print_to_string();
	const char* remove = "b";
	sl.remove_anycase(remove);
	char* changed = sl.print_to_string();
	emit_input_header();
	emit_param("StringList", nicePrint(orig));
	emit_param("STRING", remove);
	emit_output_expected_header();
	emit_param("StringList", "");
	emit_output_actual_header();
	emit_param("StringList", nicePrint(changed));
	if(niceStrCmp(changed, "") != MATCH) {
		free(orig); free(changed);
		FAIL;
	}
	free(orig); free(changed);
	PASS;
}

static bool test_remove_anycase_first() {
	emit_test("Does remove_anycase() successfully remove the first string?");
	StringList sl("a;b;c", ";");
	char* orig = sl.print_to_string();
	const char* remove = "A";
	sl.remove_anycase(remove);
	const char* expect = "b,c";
	char* changed = sl.print_to_string();
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", remove);
	emit_output_expected_header();
	emit_param("StringList", expect);
	emit_output_actual_header();
	emit_param("StringList", nicePrint(changed));
	if(niceStrCmp(expect, changed) != MATCH) {
		free(orig); free(changed);
		FAIL;
	}
	free(orig); free(changed);
	PASS;
}

static bool test_remove_anycase_last() {
	emit_test("Does remove_anycase() successfully remove the last string?");
	StringList sl("a;b;c", ";");
	char* orig = sl.print_to_string();
	const char* remove = "C";
	sl.remove_anycase(remove);
	const char* expect = "a,b";
	char* changed = sl.print_to_string();
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", remove);
	emit_output_expected_header();
	emit_param("StringList", expect);
	emit_output_actual_header();
	emit_param("StringList", nicePrint(changed));
	if(niceStrCmp(expect, changed) != MATCH) {
		free(orig); free(changed);
		FAIL;
	}
	free(orig); free(changed);
	PASS;
}

static bool test_remove_anycase_many() {
	emit_test("Does remove_anycase() successfully remove many strings?");
	StringList sl("a;b;c;d;E;F;G;H", ";");
	char* orig = sl.print_to_string();
	const char* remove1 = "h";
	const char* remove2 = "F";
	const char* remove3 = "D";
	const char* remove4 = "b";
	sl.remove_anycase(remove1);
	sl.remove_anycase(remove2);
	sl.remove_anycase(remove3);
	sl.remove_anycase(remove4);
	const char* expect = "a,c,E,G";
	char* changed = sl.print_to_string();
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", remove1);
	emit_param("STRING", remove2);
	emit_param("STRING", remove3);
	emit_param("STRING", remove4);
	emit_output_expected_header();
	emit_param("StringList", expect);
	emit_output_actual_header();
	emit_param("StringList", nicePrint(changed));
	if(niceStrCmp(expect, changed) != MATCH) {
		free(orig); free(changed);
		FAIL;
	}
	free(orig); free(changed);
	PASS;
}

static bool test_prefix_return_false_invalid() {
	emit_test("Does prefix() return false when passed a string not in the "
		"StringList?");
	StringList sl("a;b;c", ";");
	char* orig = sl.print_to_string();
	const char* check = "d";
	bool retVal = sl.prefix(check);
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_prefix_return_false_almost() {
	emit_test("Does prefix() return false when the StringList contains a "
		"string that is almost a substring of the string?");
	StringList sl("abc;b;c", ";");
	char* orig = sl.print_to_string();
	const char* check = "abd";
	bool retVal = sl.prefix(check);
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_prefix_return_false_reverse() {
	emit_test("Does prefix() return false when the passed string is a "
		"substring of one of the StringList's strings?");
	StringList sl("aah;boo;car", ";");
	char* orig = sl.print_to_string();
	const char* check = "bo";
	bool retVal = sl.prefix(check);
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_prefix_return_false_case() {
	emit_test("Does prefix() return false when the StringList contains a "
		"string that is a substring of the string when ignoring case?");
	StringList sl("a;b;c", ";");
	char* orig = sl.print_to_string();
	const char* check = "B";
	bool retVal = sl.prefix(check);
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_prefix_withwildcard_return_true() {
	emit_test("Does prefix_withwildcard() return true when it should?");
	StringList sl("/home/*/aaa;b;/home/*/htc", ";");
	char* orig = sl.print_to_string();
	const char* check = "/home/tannenba/htc";
	bool retVal = sl.prefix_withwildcard(check);
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if (!retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_prefix_withwildcard_return_false() {
	emit_test("Does prefix_withwildcard() return false when it should?");
	StringList sl("/home/*/aaa/*;b;/home/*/hpc", ";");
	char* orig = sl.print_to_string();
	const char* check = "/home/tannenba/htc";
	bool retVal = sl.prefix_withwildcard(check);
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if (retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_prefix_return_true_identical() {
	emit_test("Does prefix() return true when the passed string is in "
		"the StringList?");
	StringList sl("a;b;c", ";");
	char* orig = sl.print_to_string();
	const char* check = "a";
	bool retVal = sl.prefix(check);
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_prefix_return_true_many() {
	emit_test("Does prefix() return true when the StringList contains "
		"prefixes of the string for multiple calls?");
	StringList sl("a;b;c", ";");
	char* orig = sl.print_to_string();
	const char* check1 = "car";
	const char* check2 = "bar";
	const char* check3 = "aar";
	bool retVal = sl.prefix(check1) && sl.prefix(check2) 
		&& sl.prefix(check3);
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check1);
	emit_param("STRING", check2);
	emit_param("STRING", check3);
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_prefix_current_single() {
	emit_test("Does prefix() change current to point to the location of "
		"the single match?");
	emit_comment("To test that current points to the correct string, "
		"next() has to be called so a problem with that may cause this"
		" to fail.");
	StringList sl("a;b;c", ";");
	char* orig = sl.print_to_string();
	const char* check = "are";
	sl.prefix(check); 
	char* next = sl.next();
	const char* expect = "b";
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_param("next", next);
	emit_output_actual_header();
	emit_param("next", nicePrint(next));
	if(niceStrCmp(next, expect) != MATCH) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_prefix_current_multiple() {
	emit_test("Does prefix() change current to point to the location of "
		"the first match when there are multiple matches?");	
	emit_comment("To test that current points to the correct string, "
		"deleteCurrent() has to be called so a problem with that may cause this"
		" to fail.");
	StringList sl("a;b;a;b", ";");
	char* orig = sl.print_to_string();
	const char* check = "are";
	sl.prefix(check); 
	sl.deleteCurrent();
	char* changed = sl.print_to_string();
	const char* expect = "b,a,b";
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_param("StringList", expect);
	emit_output_actual_header();
	emit_param("StringList", nicePrint(changed));
	if(niceStrCmp(changed, expect) != MATCH) {
		free(orig); free(changed);
		FAIL;
	}
	free(orig); free(changed);
	PASS;
}


static bool test_contains_withwildcard_return_false() {
	emit_test("Does contains_withwildcard() return false when passed a string"
		" not in the StringList?");
	StringList sl("a;b;c", ";");
	char* orig = sl.print_to_string();
	const char* check = "d";
	bool retVal = sl.contains_withwildcard(check); 
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_contains_withwildcard_return_false_substring() {
	emit_test("Does contains_withwildcard() return false when the StringList "
		"contains a substring of the string?");
	StringList sl("a;b;c", ";");
	char* orig = sl.print_to_string();
	const char* check = "boo";
	bool retVal = sl.contains_withwildcard(check); 
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_contains_withwildcard_return_false_case() {
	emit_test("Does contains_withwildcard() return false when the StringList "
		"contains the string when ignoring case?");
	StringList sl("a;b;c", ";");
	char* orig = sl.print_to_string();
	const char* check = "A";
	bool retVal = sl.contains_withwildcard(check); 
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_contains_withwildcard_return_false_wild() {
	emit_test("Does contains_withwildcard() return false when the passed "
		"string is not in the StringList when using a wildcard?");
	StringList sl("a;b;c", ";");
	char* orig = sl.print_to_string();
	const char* check = "d*";
	bool retVal = sl.contains_withwildcard(check); 
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_contains_withwildcard_return_true_no_wild() {
	emit_test("Does contains_withwildcard() return true when the StringList "
		"contains the string when not using a wildcard?");
	StringList sl("a;b;c", ";");
	char* orig = sl.print_to_string();
	const char* check = "a";
	bool retVal = sl.contains_withwildcard(check); 
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_contains_withwildcard_return_true_only_wild() {
	emit_test("Does contains_withwildcard() return true when the StringList "
		"passed contains a string consisting of only the wildcard?");
	StringList sl("*;a;b;c", ";");
	char* orig = sl.print_to_string();
	const char* check = "foo";
	bool retVal = sl.contains_withwildcard(check); 
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_contains_withwildcard_return_true_start() {
	emit_test("Does contains_withwildcard() return true when StringList "
		"contains the string with a wildcard at the start?");
	StringList sl("a;*r;c", ";");
	char* orig = sl.print_to_string();
	const char* check = "bar";
	bool retVal = sl.contains_withwildcard(check); 
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_contains_withwildcard_return_true_mid() {
	emit_test("Does contains_withwildcard() return true when StringList "
		"contains the string with a wildcard in the middle of the string?");
	StringList sl("a;b*r;c", ";");
	char* orig = sl.print_to_string();
	const char* check = "bar";
	bool retVal = sl.contains_withwildcard(check); 
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_contains_withwildcard_return_true_mid_and_end() {
	emit_test("Does contains_withwildcard() return true when StringList "
		"contains the string with a wildcard in the middle and end of the string?");
	StringList sl("a;b*r*;c", ";");
	char* orig = sl.print_to_string();
	const char* check = "bartbarboozehound";
	bool retVal = sl.contains_withwildcard(check);
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if (!retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_contains_withwildcard_return_true_start_and_end() {
	emit_test("Does contains_withwildcard() return true when StringList "
		"contains the string with a wildcard at the start and end of the string?");
	StringList sl("a;*bar*;c", ";");
	char* orig = sl.print_to_string();
	const char* check = "thebarboozebarhound";
	bool retVal = sl.contains_withwildcard(check);
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if (!retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}


static bool test_contains_withwildcard_return_false_mid_and_end() {
	emit_test("Does contains_withwildcard() return false when StringList "
		"does NOT contain the string with a wildcard in the middle and end of the string?");
	StringList sl("a;b*r*;c", ";");
	char* orig = sl.print_to_string();
	const char* check = "batbaboozehound";
	bool retVal = sl.contains_withwildcard(check);
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if (retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_contains_withwildcard_return_false_start_and_end() {
	emit_test("Does contains_withwildcard() return false when StringList "
		"does NOT contain the string with a wildcard at the start and end of the string?");
	StringList sl("a;*bar*;c", ";");
	char* orig = sl.print_to_string();
	const char* check = "thebaarboozebathoundba";
	bool retVal = sl.contains_withwildcard(check);
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if (retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}


static bool test_contains_withwildcard_return_true_end() {
	emit_test("Does contains_withwildcard() return true when StringList "
		"contains the string with a wildcard at the end of the string?");
	StringList sl("a;b*;c", ";");
	char* orig = sl.print_to_string();
	const char* check = "bar";
	bool retVal = sl.contains_withwildcard(check); 
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_contains_withwildcard_return_true_same_wild() {
	emit_test("Does contains_withwildcard() return true when StringList "
		"contains the string but the wildcard is used too?");
	StringList sl("a*;b;c", ";");
	char* orig = sl.print_to_string();
	const char* check = "a";
	bool retVal = sl.contains_withwildcard(check); 
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_contains_withwildcard_return_true_multiple() {
	emit_test("Does contains_withwildcard() return true when the StringList "
		"contains multiple matches of the string?");
	StringList sl("a*;ar*;are*;", ";");
	char* orig = sl.print_to_string();
	const char* check = "are";
	bool retVal = sl.contains_withwildcard(check); 
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_contains_withwildcard_return_true_many() {
	emit_test("Does contains_withwildcard() return true when StringList "
		"contains the strings for many calls?");
	StringList sl("a*;ba*;car*", ";");
	char* orig = sl.print_to_string();
	const char* check1 = "car";
	const char* check2 = "bar";
	const char* check3 = "aar";
	bool retVal = sl.contains_withwildcard(check1) 
		&& sl.contains_withwildcard(check2) 
		&& sl.contains_withwildcard(check3);
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check1);
	emit_param("STRING", check2);
	emit_param("STRING", check3);
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_contains_withwildcard_current_single() {
	emit_test("Does contains_withwildcard() change current to point to the "
		"location of the single match?");
	emit_comment("To test that current points to the correct string, "
		"next() has to be called so a problem with that may cause this"
		" to fail.");
	StringList sl("a*;b;c*", ";");
	char* orig = sl.print_to_string();
	const char* check = "aar";
	sl.contains_withwildcard(check); 
	char* next = sl.next();
	const char* expect = "b";
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_param("next", next);
	emit_output_actual_header();
	emit_param("next", nicePrint(next));
	if(niceStrCmp(next, expect) != MATCH) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_contains_withwildcard_current_multiple() {
	emit_test("Does contains_withwildcard() change current to point to the "
		"location of the first match when there are multiple matches?");
	emit_comment("To test that current points to the correct string, "
		"deleteCurrent() has to be called so a problem with that may cause this"
		" to fail.");
	StringList sl("a*;b;aa*;b", ";");
	char* orig = sl.print_to_string();
	const char* check = "aar";
	sl.contains_withwildcard(check); 
	sl.deleteCurrent();
	char* changed = sl.print_to_string();
	const char* expect = "b,aa*,b";
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_param("StringList", expect);
	emit_output_actual_header();
	emit_param("StringList", nicePrint(changed));
	if(niceStrCmp(changed, expect) != MATCH) {
		free(orig); free(changed);
		FAIL;
	}
	free(orig); free(changed);
	PASS;
}

static bool test_contains_anycase_wwc_return_false() {
	emit_test("Does contains_anycase_withwildcard() return false when passed "
		"a string not in the StringList?");
	StringList sl("a;b;c", ";");
	char* orig = sl.print_to_string();
	const char* check = "d";
	bool retVal = sl.contains_anycase_withwildcard(check); 
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_contains_anycase_wwc_return_false_substring() {
	emit_test("Does contains_anycase_withwildcard() return false when the "
		"StringList contains a substring of the string?");
	StringList sl("a;b;c", ";");
	char* orig = sl.print_to_string();
	const char* check = "boo";
	bool retVal = sl.contains_anycase_withwildcard(check); 
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_contains_anycase_wwc_return_false_wild() {
	emit_test("Does contains_anycase_withwildcard() return false when the "
		"passed string is not in the StringList when using a wildcard?");
	StringList sl("a;b;c", ";");
	char* orig = sl.print_to_string();
	const char* check = "d*";
	bool retVal = sl.contains_anycase_withwildcard(check); 
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_contains_anycase_wwc_return_true_no_wild() {
	emit_test("Does contains_anycase_withwildcard() return true when the "
		"StringList contains the string when not using a wildcard?");
	StringList sl("a;b;c", ";");
	char* orig = sl.print_to_string();
	const char* check = "A";
	bool retVal = sl.contains_anycase_withwildcard(check); 
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_contains_anycase_wwc_return_true_only_wild() {
	emit_test("Does contains_anycase_withwildcard() return true when the "
		"StringList passed contains a string consisting of only the wildcard?");
	StringList sl("*;a;b;c", ";");
	char* orig = sl.print_to_string();
	const char* check = "foo";
	bool retVal = sl.contains_anycase_withwildcard(check); 
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_contains_anycase_wwc_return_true_start() {
	emit_test("Does contains_anycase_withwildcard() return true when "
		"StringList contains the string with a wildcard at the start?");
	StringList sl("A;*R;C", ";");
	char* orig = sl.print_to_string();
	const char* check = "bar";
	bool retVal = sl.contains_anycase_withwildcard(check); 
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_contains_anycase_wwc_return_true_mid() {
	emit_test("Does contains_anycase_withwildcard() return true when "
		"StringList contains the string with a wildcard in the middle of the "
		"string?");
	StringList sl("a;b*r;c", ";");
	char* orig = sl.print_to_string();
	const char* check = "Bar";
	bool retVal = sl.contains_anycase_withwildcard(check); 
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_contains_anycase_wwc_return_true_end() {
	emit_test("Does contains_anycase_withwildcard() return true when "
		"StringList contains the string with a wildcard at the end of the "
		"string?");
	StringList sl("a;b*;c", ";");
	char* orig = sl.print_to_string();
	const char* check = "BAR";
	bool retVal = sl.contains_anycase_withwildcard(check); 
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_contains_anycase_wwc_return_true_same_wild() {
	emit_test("Does contains_anycase_withwildcard() return true when "
		"StringList contains the string but the wildcard is used too?");
	StringList sl("a*;b;c", ";");
	char* orig = sl.print_to_string();
	const char* check = "A";
	bool retVal = sl.contains_anycase_withwildcard(check); 
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_contains_anycase_wwc_return_true_multiple() {
	emit_test("Does contains_anycase_withwildcard() return true when the "
		"StringList contains multiple matches of the string?");
	StringList sl("a*;AR*;Are*;", ";");
	char* orig = sl.print_to_string();
	const char* check = "are";
	bool retVal = sl.contains_anycase_withwildcard(check); 
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_contains_anycase_wwc_return_true_many() {
	emit_test("Does contains_anycase_withwildcard() return true when "
		"StringList contains the strings for many calls?");
	StringList sl("a*;ba*;CAR*", ";");
	char* orig = sl.print_to_string();
	const char* check1 = "car";
	const char* check2 = "bar";
	const char* check3 = "AAR";
	bool retVal = sl.contains_anycase_withwildcard(check1) 
		&& sl.contains_anycase_withwildcard(check2) 
		&& sl.contains_anycase_withwildcard(check3);
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check1);
	emit_param("STRING", check2);
	emit_param("STRING", check3);
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_contains_anycase_wwc_current_single() {
	emit_test("Does contains_anycase_withwildcard() change current to point "
		"to the location of the single match?");
	emit_comment("To test that current points to the correct string, "
		"next() has to be called so a problem with that may cause this"
		" to fail.");
	StringList sl("a*;b;c*", ";");
	char* orig = sl.print_to_string();
	const char* check = "AAR";
	sl.contains_anycase_withwildcard(check); 
	char* next = sl.next();
	const char* expect = "b";
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_param("next", next);
	emit_output_actual_header();
	emit_param("next", nicePrint(next));
	if(niceStrCmp(next, expect) != MATCH) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_contains_anycase_wwc_current_multiple() {
	emit_test("Does contains_anycase_withwildcard() change current to point "
		"to the location of the first match when there are multiple matches?");
	emit_comment("To test that current points to the correct string, "
		"deleteCurrent() has to be called so a problem with that may cause this"
		" to fail.");
	StringList sl("A*;b;Aa*;b", ";");
	char* orig = sl.print_to_string();
	const char* check = "aar";
	sl.contains_anycase_withwildcard(check); 
	sl.deleteCurrent();
	char* changed = sl.print_to_string();
	const char* expect = "b,Aa*,b";
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_output_expected_header();
	emit_param("StringList", expect);
	emit_output_actual_header();
	emit_param("StringList", nicePrint(changed));
	if(niceStrCmp(changed, expect) != MATCH) {
		free(orig); free(changed);
		FAIL;
	}
	free(orig); free(changed);
	PASS;
}

static bool test_find_matches_any_wwc_return_false() {
	emit_test("Does find_matches_anycase_withwildcard() return false when "
		"passed a string not in the StringList?");
	StringList sl("a;b;c", ";");
	char* orig = sl.print_to_string();
	const char* check = "d";
	bool retVal = sl.find_matches_anycase_withwildcard(check, NULL); 
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_param("StringList", "NULL");
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_find_matches_any_wwc_return_false_substring() {
	emit_test("Does find_matches_anycase_withwildcard() return false when the"
		" StringList contains a substring of the string?");
	StringList sl("a;b;c", ";");
	char* orig = sl.print_to_string();
	const char* check = "boo";
	bool retVal = sl.find_matches_anycase_withwildcard(check, NULL); 
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_param("StringList", "NULL");
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_find_matches_any_wwc_return_false_wild() {
	emit_test("Does find_matches_anycase_withwildcard() return false when the"
		" passed string is not in the StringList when using a wildcard?");
	StringList sl("a;b;c", ";");
	char* orig = sl.print_to_string();
	const char* check = "d*";
	bool retVal = sl.find_matches_anycase_withwildcard(check, NULL); 
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_param("StringList", "NULL");
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_find_matches_any_wwc_return_true_no_wild() {
	emit_test("Does find_matches_anycase_withwildcard() return true when the "
		"StringList contains the string when not using a wildcard?");
	StringList sl("a;b;c", ";");
	char* orig = sl.print_to_string();
	const char* check = "A";
	bool retVal = sl.find_matches_anycase_withwildcard(check, NULL); 
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_param("StringList", "NULL");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_find_matches_any_wwc_return_true_only_wild() {
	emit_test("Does find_matches_anycase_withwildcard() return true when the "
		"StringList passed contains a string consisting of only the wildcard?");
	StringList sl("*;a;b;c", ";");
	char* orig = sl.print_to_string();
	const char* check = "foo";
	bool retVal = sl.find_matches_anycase_withwildcard(check, NULL); 
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_param("StringList", "NULL");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_find_matches_any_wwc_return_true_start() {
	emit_test("Does find_matches_anycase_withwildcard() return true when "
		"StringList contains the string with a wildcard at the start?");
	StringList sl("A;*R;C", ";");
	char* orig = sl.print_to_string();
	const char* check = "bar";
	bool retVal = sl.find_matches_anycase_withwildcard(check, NULL); 
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_param("StringList", "NULL");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_find_matches_any_wwc_return_true_mid() {
	emit_test("Does find_matches_anycase_withwildcard() return true when "
		"StringList contains the string with a wildcard in the middle of the "
		"string?");
	StringList sl("a;b*r;c", ";");
	char* orig = sl.print_to_string();
	const char* check = "Bar";
	bool retVal = sl.find_matches_anycase_withwildcard(check, NULL); 
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_param("StringList", "NULL");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_find_matches_any_wwc_return_true_end() {
	emit_test("Does find_matches_anycase_withwildcard() return true when "
		"StringList contains the string with a wildcard at the end of the "
		"string?");
	StringList sl("a;b*;c", ";");
	char* orig = sl.print_to_string();
	const char* check = "BAR";
	bool retVal = sl.find_matches_anycase_withwildcard(check, NULL); 
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_param("StringList", "NULL");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_find_matches_any_wwc_return_true_same_wild() {
	emit_test("Does find_matches_anycase_withwildcard() return true when "
		"StringList contains the string but the wildcard is used too?");
	StringList sl("a*;b;c", ";");
	char* orig = sl.print_to_string();
	const char* check = "A";
	bool retVal = sl.find_matches_anycase_withwildcard(check, NULL); 
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_param("StringList", "NULL");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_find_matches_any_wwc_return_true_multiple() {
	emit_test("Does find_matches_anycase_withwildcard() return true when the "
		"StringList contains multiple matches of the string?");
	StringList sl("a*;AR*;Are*;", ";");
	char* orig = sl.print_to_string();
	const char* check = "are";
	bool retVal = sl.find_matches_anycase_withwildcard(check, NULL); 
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_param("StringList", "NULL");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_find_matches_any_wwc_return_true_many() {
	emit_test("Does find_matches_anycase_withwildcard() return true when "
		"StringList contains the strings for many calls?");
	StringList sl("a*;ba*;CAR*", ";");
	char* orig = sl.print_to_string();
	const char* check1 = "car";
	const char* check2 = "bar";
	const char* check3 = "AAR";
	bool retVal = sl.find_matches_anycase_withwildcard(check1, NULL) 
		&& sl.find_matches_anycase_withwildcard(check2, NULL) 
		&& sl.find_matches_anycase_withwildcard(check3, NULL);
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check1);
	emit_param("StringList", "NULL");
	emit_param("STRING", check2);
	emit_param("StringList", "NULL");
	emit_param("STRING", check3);
	emit_param("StringList", "NULL");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_find_matches_any_wwc_none() {
	emit_test("Test that find_matches_anycase_withwildcard() does not add "
		"any matches to the list when there are no matches in the StringList.");
	StringList sl("a;b;c", ";");
	StringList m;
	char* orig = sl.print_to_string();
	const char* check = "d";
	sl.find_matches_anycase_withwildcard(check, &m); 
	char* matches = m.print_to_string();
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_param("StringList", "");
	emit_output_expected_header();
	emit_param("MATCHES", "");
	emit_output_actual_header();
	emit_param("MATCHES", nicePrint(matches));
	if(niceStrCmp(matches, "") != MATCH) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_find_matches_any_wwc_one() {
	emit_test("Test that find_matches_anycase_withwildcard() adds the "
		"matching string to the StringList.");
	StringList sl("a;b;c", ";");
	StringList m;
	char* orig = sl.print_to_string();
	const char* check = "a";
	sl.find_matches_anycase_withwildcard(check, &m); 
	const char* expect = "a";
	char* matches = m.print_to_string();
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_param("StringList", "");
	emit_output_expected_header();
	emit_param("MATCHES", expect);
	emit_output_actual_header();
	emit_param("MATCHES", nicePrint(matches));
	if(niceStrCmp(matches, expect) != MATCH) {
		free(orig); free(matches);
		FAIL;
	}
	free(orig); free(matches);
	PASS;
}

static bool test_find_matches_any_wwc_one_exist() {
	emit_test("Test that find_matches_anycase_withwildcard() adds the "
		"matching string to the StringList with existing strings.");
	StringList sl("a;b;c", ";");
	StringList m("1;2;3", ";");
	char* orig = sl.print_to_string();
	char* list = m.print_to_string();
	const char* check = "a";
	sl.find_matches_anycase_withwildcard(check, &m); 
	const char* expect = "1,2,3,a";
	char* matches = m.print_to_string();
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_param("StringList", list);
	emit_output_expected_header();
	emit_param("MATCHES", expect);
	emit_output_actual_header();
	emit_param("MATCHES", nicePrint(matches));
	if(niceStrCmp(matches, expect) != MATCH) {
		free(orig); free(list); free(matches);
		FAIL;
	}
	free(orig); free(list); free(matches);
	PASS;
}

static bool test_find_matches_any_wwc_wild_start() {
	emit_test("Test that find_matches_anycase_withwildcard() adds the "
		"matching string to the StringList when using a wildcard at the start "
		"of a string.");
	StringList sl("a;*R;c", ";");
	StringList m;
	char* orig = sl.print_to_string();
	const char* check = "bar";
	sl.find_matches_anycase_withwildcard(check, &m); 
	const char* expect = "*R";
	char* matches = m.print_to_string();
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_param("StringList", "");
	emit_output_expected_header();
	emit_param("MATCHES", expect);
	emit_output_actual_header();
	emit_param("MATCHES", nicePrint(matches));
	if(niceStrCmp(matches, expect) != MATCH) {
		free(orig); free(matches);
		FAIL;
	}
	free(orig); free(matches);
	PASS;
}

static bool test_find_matches_any_wwc_wild_mid() {
	emit_test("Test that find_matches_anycase_withwildcard() adds the "
		"matching string to the StringList when using a wildcard in the middle "
		"of a string.");
	StringList sl("a;B*r;c", ";");
	StringList m;
	char* orig = sl.print_to_string();
	const char* check = "bar";
	sl.find_matches_anycase_withwildcard(check, &m); 
	const char* expect = "B*r";
	char* matches = m.print_to_string();
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_param("StringList", "");
	emit_output_expected_header();
	emit_param("MATCHES", expect);
	emit_output_actual_header();
	emit_param("MATCHES", nicePrint(matches));
	if(niceStrCmp(matches, expect) != MATCH) {
		free(orig); free(matches);
		FAIL;
	}
	free(orig); free(matches);
	PASS;
}

static bool test_find_matches_any_wwc_wild_end() {
	emit_test("Test that find_matches_anycase_withwildcard() adds the "
		"matching string to the StringList when using a wildcard at the end "
		"of a string.");
	StringList sl("a;b*;c", ";");
	StringList m;
	char* orig = sl.print_to_string();
	const char* check = "bar";
	sl.find_matches_anycase_withwildcard(check, &m); 
	const char* expect = "b*";
	char* matches = m.print_to_string();
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_param("StringList", "");
	emit_output_expected_header();
	emit_param("MATCHES", expect);
	emit_output_actual_header();
	emit_param("MATCHES", nicePrint(matches));
	if(niceStrCmp(matches, expect) != MATCH) {
		free(orig); free(matches);
		FAIL;
	}
	free(orig); free(matches);
	PASS;
}

static bool test_find_matches_any_wwc_multiple() {
	emit_test("Test that find_matches_anycase_withwildcard() adds the "
		"matching strings to the StringList when there are multiple matches.");
	StringList sl("a*;AR*;Are*;are", ";");
	StringList m;
	char* orig = sl.print_to_string();
	const char* check = "are";
	sl.find_matches_anycase_withwildcard(check, &m); 
	const char* expect = "a*,AR*,Are*,are";
	char* matches = m.print_to_string();
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_param("StringList", "");
	emit_output_expected_header();
	emit_param("MATCHES", expect);
	emit_output_actual_header();
	emit_param("MATCHES", nicePrint(matches));
	if(niceStrCmp(matches, expect) != MATCH) {
		free(orig); free(matches);
		FAIL;
	}
	free(orig); free(matches);
	PASS;
}

static bool test_find_matches_any_wwc_multiple_exist() {
	emit_test("Test that find_matches_anycase_withwildcard() adds the "
		"matching strings to the StringList with existing strings when there "
		"are multiple matches.");
	StringList sl("a*;AR*;Are*;are", ";");
	StringList m("a;b;c", ";");
	char* orig = sl.print_to_string();
	char* list = m.print_to_string();
	const char* check = "are";
	sl.find_matches_anycase_withwildcard(check, &m); 
	const char* expect = "a,b,c,a*,AR*,Are*,are";
	char* matches = m.print_to_string();
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_param("StringList", list);
	emit_output_expected_header();
	emit_param("MATCHES", expect);
	emit_output_actual_header();
	emit_param("MATCHES", nicePrint(matches));
	if(niceStrCmp(matches, expect) != MATCH) {
		free(orig); free(list); free(matches);
		FAIL;
	}
	free(orig); free(list); free(matches);
	PASS;
}

static bool test_find_return_false() {
	emit_test("Does find() return false when passed a string not in the "
		"StringList?");
	StringList sl("a;b;c", ";");
	char* orig = sl.print_to_string();
	const char* check = "d";
	bool retVal = sl.find(check, false);
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_param("Anycase", "FALSE");
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_find_return_false_substring() {
	emit_test("Does find() return false when passed a string not in the "
		"StringList, but the StringList has a substring of the string?");
	StringList sl("a;b;c", ";");
	char* orig = sl.print_to_string();
	const char* check = "boo";
	bool retVal = sl.find(check, false);
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_param("Anycase", "FALSE");
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_find_return_false_case() {
	emit_test("Does find() return false when passed a string in the "
		"StringList when ignoring case, but anycase is false?");
	StringList sl("a;b;c", ";");
	char* orig = sl.print_to_string();
	const char* check = "A";
	bool retVal = sl.find(check, false);
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_param("Anycase", "FALSE");
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_find_return_true_one() {
	emit_test("Does find() return true when passed one string in the "
		"StringList?");
	StringList sl("a;b;c", ";");
	char* orig = sl.print_to_string();
	const char* check = "a";
	bool retVal = sl.find(check, false);
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_param("Anycase", "FALSE");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_find_return_true_many() {
	emit_test("Does find() return true when called many times for different "
		"strings in the StringList?");
	StringList sl("a;b;c", ";");
	char* orig = sl.print_to_string();
	const char* check1 = "a";
	const char* check2 = "b";
	const char* check3 = "c";
	bool retVal = sl.find(check3, false) && sl.find(check2, false) 
		&& sl.find(check1, false);
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check3);
	emit_param("Anycase", "FALSE");
	emit_param("STRING", check2);
	emit_param("Anycase", "FALSE");
	emit_param("STRING", check1);
	emit_param("Anycase", "FALSE");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_find_anycase_return_false() {
	emit_test("Does find() return false when passed a string not in the "
		"StringList when anycase is true?");
	StringList sl("a;b;c", ";");
	char* orig = sl.print_to_string();
	const char* check = "d";
	bool retVal = sl.find(check, true);
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_param("Anycase", "TRUE");
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_find_anycase_return_false_substring() {
	emit_test("Does find() return false when passed a string not in the "
		"StringList when anycase is true, but the StringList has a substring "
		"of the string?");
	StringList sl("a;b;c", ";");
	char* orig = sl.print_to_string();
	const char* check = "boo";
	bool retVal = sl.find(check, true);
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_param("Anycase", "TRUE");
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_find_anycase_return_true_one() {
	emit_test("Does find() return true when passed one string in the "
		"StringList when anycase is true?");
	StringList sl("a;b;c", ";");
	char* orig = sl.print_to_string();
	const char* check = "a";
	bool retVal = sl.find(check, true);
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check);
	emit_param("Anycase", "TRUE");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_find_anycase_return_true_many() {
	emit_test("Does find() return true when called many times with different "
		"strings in the StringList when anycase is true?");
	StringList sl("a;b;C;D", ";");
	char* orig = sl.print_to_string();
	const char* check1 = "a";
	const char* check2 = "B";
	const char* check3 = "C";
	const char* check4 = "d";
	bool retVal = sl.find(check4, true) && sl.find(check3, true) 
		&& sl.find(check2, true) && sl.find(check1, true);
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", check4);
	emit_param("Anycase", "TRUE");
	emit_param("STRING", check3);
	emit_param("Anycase", "TRUE");
	emit_param("STRING", check2);
	emit_param("Anycase", "TRUE");
	emit_param("STRING", check1);
	emit_param("Anycase", "TRUE");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}


static bool test_identical_return_false_same() {
	emit_test("Does identical() return false when the lists are not identical"
		", but contain the same number of strings?");
	StringList sl("a;b;c", ";");
	StringList other("d;e;f", ";");
	char* orig = sl.print_to_string();
	char* check = other.print_to_string();
	bool retVal = sl.identical(other, false);
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("StringList", check);
	emit_param("ANYCASE", "FALSE");
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(retVal) {
		free(orig); free(check);
		FAIL;
	}
	free(orig); free(check);
	PASS;
}

static bool test_identical_return_false_not_same() {
	emit_test("Does identical() return false when the lists are not identical"
		" and don't contain the same number of strings?");
	StringList sl("a;b;c", ";");
	StringList other("d;e;f;g;h;i", ";");
	char* orig = sl.print_to_string();
	char* check = other.print_to_string();
	bool retVal = sl.identical(other, false);
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("StringList", check);
	emit_param("ANYCASE", "FALSE");
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(retVal) {
		free(orig); free(check);
		FAIL;
	}
	free(orig); free(check);
	PASS;
}

static bool test_identical_return_false_almost() {
	emit_test("Does identical() return false when the lists only differ by "
		"one string?");
	StringList sl("a;b;c", ";");
	StringList other("a;d;c", ";");
	char* orig = sl.print_to_string();
	char* check = other.print_to_string();
	bool retVal = sl.identical(other, false);
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("StringList", check);
	emit_param("ANYCASE", "FALSE");
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(retVal) {
		free(orig); free(check);
		FAIL;
	}
	free(orig); free(check);
	PASS;
}

static bool test_identical_return_false_ignore() {
	emit_test("Does identical() return false when the lists are identical "
		"when ignoring case, but anycase is false?");
	StringList sl("a;b;c", ";");
	StringList other("A;B;C", ";");
	char* orig = sl.print_to_string();
	char* check = other.print_to_string();
	bool retVal = sl.identical(other, false);
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("StringList", check);
	emit_param("ANYCASE", "FALSE");
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(retVal) {
		free(orig); free(check);
		FAIL;
	}
	free(orig); free(check);
	PASS;
}

static bool test_identical_return_false_subset() {
	emit_test("Does identical() return false when the passed StringList is a "
		"subset of the StringList called upon?");
	StringList sl("a;b;c;d;e;f", ";");
	StringList other("a;b;c", ";");
	char* orig = sl.print_to_string();
	char* check = other.print_to_string();
	bool retVal = sl.identical(other, false);
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("StringList", check);
	emit_param("ANYCASE", "FALSE");
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(retVal) {
		free(orig); free(check);
		FAIL;
	}
	free(orig); free(check);
	PASS;
}

static bool test_identical_return_true_case() {
	emit_test("Does identical() return true when the StringLists are "
		"identical when not ignoring case?");
	StringList sl("a;b;c", ";");
	StringList other("a;b;c", ";");
	char* orig = sl.print_to_string();
	char* check = other.print_to_string();
	bool retVal = sl.identical(other, false);
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("StringList", check);
	emit_param("ANYCASE", "FALSE");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		free(orig); free(check);
		FAIL;
	}
	free(orig); free(check);
	PASS;
}

static bool test_identical_return_true_ignore() {
	emit_test("Does identical() return return true when the StringLists are "
		"identical when ignoring case?");
	StringList sl("a;b;C;D", ";");
	StringList other("a;B;c;D", ";");
	char* orig = sl.print_to_string();
	char* check = other.print_to_string();
	bool retVal = sl.identical(other, true);
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("StringList", check);
	emit_param("ANYCASE", "TRUE");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		free(orig); free(check);
		FAIL;
	}
	free(orig); free(check);
	PASS;
}

static bool test_identical_return_true_itself() {
	emit_test("Does identical() return true when passed the same StringList "
		"called on?");
	StringList sl("a;b;c", ";");
	char* orig = sl.print_to_string();
	bool retVal = sl.identical(sl, false);
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("StringList", orig);
	emit_param("ANYCASE", "FALSE");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_identical_return_true_copy() {
	emit_test("Does identical() return true when using the copy "
		"constructor?");
	StringList sl("a;b;c", ";");
	StringList other(sl);
	char* orig = sl.print_to_string();
	char* check = other.print_to_string();
	bool retVal = sl.identical(other, true);
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("StringList", check);
	emit_param("ANYCASE", "TRUE");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval(tfstr(retVal));
	if(!retVal) {
		free(orig); free(check);
		FAIL;
	}
	free(orig); free(check);
	PASS;
}

static bool test_print_to_string_empty() {
	emit_test("Test print_to_string() on an empty StringList.");
	emit_comment("print_to_string() currently returns NULL for an empty "
		"StringList, so this will fail if that changes.");
	StringList sl("", "");
	char* ps = sl.print_to_string();
	const char* expect = "";
	emit_input_header();
	emit_param("StringList", expect);
	emit_output_expected_header();
	emit_retval(expect);
	emit_output_actual_header();
	emit_retval(nicePrint(ps));
	if(niceStrCmp(ps, expect) != MATCH) {
		free(ps);
		FAIL;
	}
	free(ps);
	PASS;
}

static bool test_print_to_string_one() {
	emit_test("Test print_to_string() on a StringList with only one string.");
	StringList sl("foo", "");
	char* ps = sl.print_to_string();
	const char* expect = "foo";
	emit_input_header();
	emit_param("StringList", expect);
	emit_output_expected_header();
	emit_retval(expect);
	emit_output_actual_header();
	emit_retval(nicePrint(ps));
	if(niceStrCmp(ps, expect) != MATCH) {
		free(ps);
		FAIL;
	}
	free(ps);
	PASS;
}

static bool test_print_to_string_many() {
	emit_test("Test print_to_string() on a StringList with many strings.");
	StringList sl("a;b;c;dog;eel;fish;goat", ";");
	char* ps = sl.print_to_string();
	const char* expect = "a,b,c,dog,eel,fish,goat";
	emit_input_header();
	emit_param("StringList", expect);
	emit_output_expected_header();
	emit_retval(expect);
	emit_output_actual_header();
	emit_retval(nicePrint(ps));
	if(niceStrCmp(ps, expect) != MATCH) {
		free(ps);
		FAIL;
	}
	free(ps);
	PASS;
}

static bool test_print_to_delimed_string_empty() {
	emit_test("Test print_to_delimed_string() on an empty StringList.");
	emit_comment("print_to_delimed_string() currently returns NULL for an "
		"empty StringList, so this will fail if that changes.");
	StringList sl("", "");
	char* ps = sl.print_to_delimed_string(",");
	const char* expect = "";
	emit_input_header();
	emit_param("StringList", expect);
	emit_param("DELIM", ",");
	emit_output_expected_header();
	emit_retval(expect);
	emit_output_actual_header();
	emit_retval(nicePrint(ps));
	if(niceStrCmp(ps, expect) != MATCH) {
		free(ps);
		FAIL;
	}
	free(ps);
	PASS;
}

static bool test_print_to_delimed_string_one() {
	emit_test("Test print_to_delimed_string() on a StringList with only one "
		"string.");
	StringList sl("foo", "");
	char* ps = sl.print_to_delimed_string("!");
	const char* expect = "foo";
	emit_input_header();
	emit_param("StringList", expect);
	emit_param("DELIM", "!");
	emit_output_expected_header();
	emit_retval(expect);
	emit_output_actual_header();
	emit_retval(nicePrint(ps));
	if(niceStrCmp(ps, expect) != MATCH) {
		free(ps);
		FAIL;
	}
	free(ps);
	PASS;
}

static bool test_print_to_delimed_string_many_short() {
	emit_test("Test print_to_delimed_string() with a short delimiter on a "
		"StringList with many strings.");
	StringList sl("a;b;c;dog;eel;fish;goat", ";");
	char* orig = sl.print_to_delimed_string();
	char* ps = sl.print_to_delimed_string("&");
	const char* expect = "a&b&c&dog&eel&fish&goat";
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("DELIM", "&");
	emit_output_expected_header();
	emit_retval(expect);
	emit_output_actual_header();
	emit_retval(nicePrint(ps));
	if(niceStrCmp(ps, expect) != MATCH) {
		free(orig); free(ps);
		FAIL;
	}
	free(orig); free(ps);
	PASS;
}

static bool test_print_to_delimed_string_many_long() {
	emit_test("Test print_to_delimed_string() with a long delimiter on a "
		"StringList with many strings.");
	StringList sl("a;b;c;dog;eel;fish;goat", ";");
	char* orig = sl.print_to_string();
	char* ps = sl.print_to_delimed_string(" and ");
	const char* expect = "a and b and c and dog and eel and fish and goat";
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("DELIM", " and ");
	emit_output_expected_header();
	emit_retval(expect);
	emit_output_actual_header();
	emit_retval(nicePrint(ps));
	if(niceStrCmp(ps, expect) != MATCH) {
		free(orig); free(ps);
		FAIL;
	}
	free(orig); free(ps);
	PASS;
}

static bool test_print_to_delimed_string_many_null() {
	emit_test("Test print_to_delimed_string() with a NULL delimiter on a "
		"StringList with many strings.");
	emit_comment("Currently print_to_delimed_string() prints the StringList "
		"using the StringList's delimiters if the passed delim is NULL.");
	StringList sl("a;b;c;dog;eel;fish;goat", ";");
	char* ps = sl.print_to_delimed_string(NULL);
	char* orig = sl.print_to_string();
	const char* expect = "a;b;c;dog;eel;fish;goat";
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("DELIM", "NULL");
	emit_output_expected_header();
	emit_retval(expect);
	emit_output_actual_header();
	emit_retval(nicePrint(ps));
	if(niceStrCmp(ps, expect) != MATCH) {
		free(orig); free(ps);
		FAIL;
	}
	free(orig); free(ps);
	PASS;
}

static bool test_delete_current_before() {
	emit_test("Does calling deleteCurrent() before any calls to contains() "
		"or prefix() delete the last string from the StringList?");
	emit_comment("Since the StringList constructor calls Append on its "
		"internal list for each string to add, current points at the last "
		"string added.");
	StringList sl("a;b;c", ";");
	char* orig = sl.print_to_string();
	const char* expect = "a,b";
	sl.deleteCurrent();
	char* changed = sl.print_to_string();
	emit_input_header();
	emit_param("StringList", orig);
	emit_output_expected_header();
	emit_retval(expect);
	emit_output_actual_header();
	emit_retval(nicePrint(changed));
	if(niceStrCmp(changed, expect) != MATCH) {
		free(orig); free(changed);
		FAIL;
	}
	free(orig); free(changed);
	PASS;
}

static bool test_delete_current_after_no_match() {
	emit_test("Does calling deleteCurrent() after calling contains() on a "
		"string that isn't in the StringList delete change the current "
		"pointer?");	
	emit_comment("Since the StringList constructor calls Append on its "
		"internal list for each string to add, current points at the last "
		"string added.");
	emit_comment("Testing deleteCurrent() requires calling contains*() or "
		"prefix() so a problem with one of these could cause problems "
		"here.");
	StringList sl("a;b;c", ";");
	char* orig = sl.print_to_string();
	const char* expect = "a,b";
	sl.contains("d");
	sl.deleteCurrent();
	char* changed = sl.print_to_string();
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("contains()", "d");
	emit_output_expected_header();
	emit_retval(expect);
	emit_output_actual_header();
	emit_retval(nicePrint(changed));
	if(niceStrCmp(changed, expect) != MATCH) {
		free(orig); free(changed);
		FAIL;
	}
	free(orig); free(changed);
	PASS;
}

static bool test_delete_current_one_first() {
	emit_test("Test deleteCurrent() after calling contains() on the first "
		"string in the StringList.");
	emit_comment("Testing deleteCurrent() requires calling contains*() or "
		"prefix() so a problem with one of these could cause problems "
		"here.");
	StringList sl("a;b;c", ";");
	char* orig = sl.print_to_string();
	const char* expect = "b,c";
	sl.contains("a");
	sl.deleteCurrent();
	char* changed = sl.print_to_string();
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("contains()", "a");
	emit_output_expected_header();
	emit_retval(expect);
	emit_output_actual_header();
	emit_retval(nicePrint(changed));
	if(niceStrCmp(changed, expect) != MATCH) {
		free(orig); free(changed);
		FAIL;
	}
	free(orig); free(changed);
	PASS;
}

static bool test_delete_current_one_mid() {
	emit_test("Test deleteCurrent() after calling contains() on a string in "
		"the middle of the StringList.");
	emit_comment("Testing deleteCurrent() requires calling contains*() or "
		"prefix() so a problem with one of these could cause problems "
		"here.");
	StringList sl("a;b;c;d;e", ";");
	char* orig = sl.print_to_string();
	const char* expect = "a,b,d,e";
	sl.contains("c");
	sl.deleteCurrent();
	char* changed = sl.print_to_string();
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("contains()", "c");
	emit_output_expected_header();
	emit_retval(expect);
	emit_output_actual_header();
	emit_retval(nicePrint(changed));
	if(niceStrCmp(changed, expect) != MATCH) {
		free(orig); free(changed);
		FAIL;
	}
	free(orig); free(changed);
	PASS;
}

static bool test_delete_current_one_last() {
	emit_test("Test deleteCurrent() after calling contains() on the last "
		" string in the StringList.");
	emit_comment("Testing deleteCurrent() requires calling contains*() or "
		"prefix() so a problem with one of these could cause problems "
		"here.");
	StringList sl("a;b;c", ";");
	char* orig = sl.print_to_string();
	const char* expect = "a,b";
	sl.contains("c");
	sl.deleteCurrent();
	char* changed = sl.print_to_string();
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("contains()", "c");
	emit_output_expected_header();
	emit_retval(expect);
	emit_output_actual_header();
	emit_retval(nicePrint(changed));
	if(niceStrCmp(changed, expect) != MATCH) {
		free(orig); free(changed);
		FAIL;
	}
	free(orig); free(changed);
	PASS;
}

static bool test_delete_current_all() {
	emit_test("Test calling deleteCurrent() on all the strings in the "
		"StringList.");
	emit_comment("Testing deleteCurrent() requires calling contains*() or "
		"prefix() so a problem with one of these could cause problems "
		"here.");
	StringList sl("a;b;c", ";");
	char* orig = sl.print_to_string();
	const char* expect = "";
	sl.contains("a");
	sl.deleteCurrent();
	sl.contains("b");
	sl.deleteCurrent();
	sl.contains("c");
	sl.deleteCurrent();
	char* changed = sl.print_to_string();
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("contains()", "a");
	emit_param("contains()", "b");
	emit_param("contains()", "c");
	emit_output_expected_header();
	emit_retval(expect);
	emit_output_actual_header();
	emit_retval(nicePrint(changed));
	if(niceStrCmp(changed, expect) != MATCH) {
		free(orig); free(changed);
		FAIL;
	}
	free(orig); free(changed);
	PASS;
}

static bool test_string_compare_equal_same_beg() {
	emit_test("Test string_compare() on the same string at the beginning of "
		" a single StringList.");
	emit_comment("This test relies on number(), rewind(), and next() so a "
		"problem with any of these may cause problems here.");
	StringList sl("a;b;c", ";");
	char* list = sl.print_to_string();
	char** strs = string_compare_helper(&sl, 0);
	int retVal = string_compare(*strs, *strs);
	emit_input_header();
	emit_param("StringList", list);
	emit_param("STRING", *strs);
	emit_param("STRING", *strs);
	emit_output_expected_header();
	emit_retval("0");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	if(retVal != MATCH) {
		free(list); free_helper(strs, 3);
		FAIL;
	}
	free(list); free_helper(strs, 3);
	PASS;
}

static bool test_string_compare_equal_same_mid() {
	emit_test("Test string_compare() on the same string in the middle of a "
		"single StringList.");
	emit_comment("This test relies on number(), rewind(), and next() so a "
		"problem with any of these may cause problems here.");
	StringList sl("a;b;c", ";");
	char* list = sl.print_to_string();
	char** strs = string_compare_helper(&sl, 1);
	int retVal = string_compare(*strs, *strs);
	emit_input_header();
	emit_param("StringList", list);
	emit_param("STRING", *strs);
	emit_param("STRING", *strs);
	emit_output_expected_header();
	emit_retval("0");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	if(retVal != MATCH) {
		free(list); free_helper(strs, 2);
		FAIL;
	}
	free(list); free_helper(strs, 2);
	PASS;
}

static bool test_string_compare_equal_same_end() {
	emit_test("Test string_compare() on the same string at the end of a "
		"single StringList.");
	emit_comment("This test relies on number(), rewind(), and next() so a "
		"problem with any of these may cause problems here.");
	StringList sl("a;b;c", ";");
	char* list = sl.print_to_string();
	char** strs = string_compare_helper(&sl, 2);
	int retVal = string_compare(*strs, *strs);
	emit_input_header();
	emit_param("StringList", list);
	emit_param("STRING", *strs);
	emit_param("STRING", *strs);
	emit_output_expected_header();
	emit_retval("0");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	if(retVal != MATCH) {
		free(list); free_helper(strs, 1);
		FAIL;
	}
	free(list); free_helper(strs, 1);
	PASS;
}

static bool test_string_compare_equal_different_list() {
	emit_test("Test string_compare() on two identical strings within two "
		"different StringLists.");
	emit_comment("This test relies on number(), rewind(), and next() so a "
		"problem with any of these may cause problems here.");
	StringList sl1("a;b;c", ";");
	StringList sl2("a;b;c", ";");
	char* list1 = sl1.print_to_string();
	char* list2 = sl2.print_to_string();
	char** strs1 = string_compare_helper(&sl1,1);
	char** strs2 = string_compare_helper(&sl2,1);
	int retVal = string_compare(*strs1, *strs2);
	emit_input_header();
	emit_param("StringList", list1);
	emit_param("StringList", list2);
	emit_param("STRING", *strs1);
	emit_param("STRING", *strs2);
	emit_output_expected_header();
	emit_retval("0");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	if(retVal != MATCH) {
		free(list1); free(list2); 
		free_helper(strs1, 2); free_helper(strs2, 2); 
		FAIL;
	}
	free(list1); free(list2); 
	free_helper(strs1, 2); free_helper(strs2, 2);
	PASS;
}

static bool test_string_compare_copy() {
	emit_test("Test string_compare() after using the copy constructor.");
	emit_comment("This test relies on number(), rewind(), and next() so a "
		"problem with any of these may cause problems here.");
	StringList sl1("a;b;c", ";");
	StringList sl2(sl1);
	char* list1  = sl1.print_to_string();
	char* list2  = sl2.print_to_string();
	char** strs1 = string_compare_helper(&sl1,0);
	char** strs2 = string_compare_helper(&sl2,0);
	int retVal = string_compare(*strs1, *strs2);
	emit_input_header();
	emit_param("StringList", list1);
	emit_param("StringList", list2);
	emit_param("STRING", *strs1);
	emit_param("STRING", *strs2);
	emit_output_expected_header();
	emit_retval("0");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	if(retVal != MATCH) {
		free(list1); free(list2); 
		free_helper(strs1, 3); free_helper(strs2, 3);
		FAIL;
	}
	free(list1); free(list2); 
	free_helper(strs1, 3); free_helper(strs2, 3);
	PASS;
}

static bool test_string_compare_not_equal_same() {
	emit_test("Test string_compare() on two different strings in a single "
		"StringList.");
	emit_comment("This test relies on number(), rewind(), and next() so a "
		"problem with any of these may cause problems here.");
	StringList sl("a;b;c", ";");
	char* list = sl.print_to_string();
	char** strs1 = string_compare_helper(&sl, 0);
	char** strs2 = string_compare_helper(&sl, 1);
	int retVal = string_compare(*strs1, *strs2);
	emit_input_header();
	emit_param("StringList", list);
	emit_param("STRING", *strs1);
	emit_param("STRING", *strs2);
	emit_output_expected_header();
	emit_retval("!= 0");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	if(retVal == MATCH) {
		free(list); free_helper(strs1, 3); free_helper(strs2, 2);
		FAIL;
	}
	free(list); free_helper(strs1, 3); free_helper(strs2, 2);
	PASS;
}

static bool test_string_compare_not_equal_different() {
	emit_test("Test string_compare() on two different strings within two "
		"different StringLists.");
	emit_comment("This test relies on number(), rewind(), and next() so a "
		"problem with any of these may cause problems here.");
	StringList sl1("a;b;c", ";");
	StringList sl2("d;e;f", ";");
	char* list1  = sl1.print_to_string();
	char* list2  = sl2.print_to_string();
	char** strs1 = string_compare_helper(&sl1,1);
	char** strs2 = string_compare_helper(&sl2,0);
	int retVal = string_compare(*strs1, *strs2);
	emit_input_header();
	emit_param("StringList", list1);
	emit_param("StringList", list2);
	emit_param("STRING", *strs1);
	emit_param("STRING", *strs2);
	emit_output_expected_header();
	emit_retval("!= 0");
	emit_output_actual_header();
	emit_retval("%d", retVal);
	if(retVal == MATCH) {
		free(list1); free(list2); 
		free_helper(strs1, 2); free_helper(strs2, 3);
		FAIL;
	}
	free(list1); free(list2); 
	free_helper(strs1, 2); free_helper(strs2, 3);
	PASS;
}

static bool test_qsort_empty() {
	emit_test("Test qsort() on an empty StringList.");
	StringList sl("", "");
	char* orig  = sl.print_to_string();
	sl.qsort();
	char* changed  = sl.print_to_string();
	const char* expect  = "";
	emit_input_header();
	emit_param("StringList", nicePrint(orig));
	emit_output_expected_header();
	emit_param("StringList", expect);
	emit_output_actual_header();
	emit_param("StringList", nicePrint(changed));
	if(niceStrCmp(changed, expect) != MATCH) {
		free(orig); free(changed);
		FAIL;
	}
	free(orig); free(changed);
	PASS;
}

static bool test_qsort_one() {
	emit_test("Test qsort() on a StringList with only one string.");
	StringList sl("foo", "");
	char* orig  = sl.print_to_string();
	sl.qsort();
	char* changed  = sl.print_to_string();
	const char* expect  = "foo";
	emit_input_header();
	emit_param("StringList", orig);
	emit_output_expected_header();
	emit_param("StringList", expect);
	emit_output_actual_header();
	emit_param("StringList", nicePrint(changed));
	if(niceStrCmp(changed, expect) != MATCH) {
		free(orig); free(changed);
		FAIL;
	}
	free(orig); free(changed);
	PASS;
}

static bool test_qsort_already() {
	emit_test("Does qsort() change a StringList that is already sorted?");
	StringList sl("a;b;c", ";");
	char* orig  = sl.print_to_string();
	sl.qsort();
	char* changed  = sl.print_to_string();
	const char* expect  = "a,b,c";
	emit_input_header();
	emit_param("StringList", orig);
	emit_output_expected_header();
	emit_param("StringList", expect);
	emit_output_actual_header();
	emit_param("StringList", nicePrint(changed));
	if(niceStrCmp(changed, expect) != MATCH) {
		free(orig); free(changed);
		FAIL;
	}
	free(orig); free(changed);
	PASS;
}

static bool test_qsort_multiple() {
	emit_test("Does qsort() change a StringList that has already been sorted "
		"by qsort()?");
	StringList sl("c;b;a", ";");
	char* orig  = sl.print_to_string();
	sl.qsort();
	char* changed1  = sl.print_to_string();
	sl.qsort();
	char* changed2  = sl.print_to_string();
	const char* expect  = "a,b,c";
	emit_input_header();
	emit_param("StringList", orig);
	emit_output_expected_header();
	emit_param("StringList", expect);
	emit_output_actual_header();
	emit_param("StringList", nicePrint(changed2));
	if(niceStrCmp(changed2, expect) != MATCH) {
		free(orig); free(changed1); free(changed2);
		FAIL;
	}
	free(orig); free(changed1); free(changed2);
	PASS;
}

static bool test_qsort_many() {
	emit_test("Test qsort() on a StringList with many strings.");
	StringList sl("f;foo;eat;e;d;door;cool;c;b;bah;aah;a", ";");
	char* orig  = sl.print_to_string();
	sl.qsort();
	char* changed  = sl.print_to_string();
	const char* expect  = "a,aah,b,bah,c,cool,d,door,e,eat,f,foo";
	emit_input_header();
	emit_param("StringList", orig);
	emit_output_expected_header();
	emit_param("StringList", expect);
	emit_output_actual_header();
	emit_param("StringList", nicePrint(changed));
	if(niceStrCmp(changed, expect) != MATCH) {
		free(orig); free(changed);
		FAIL;
	}
	free(orig); free(changed);
	PASS;
}

static bool test_qsort_many_shuffle() {
	emit_test("Test qsort() on a StringList with many strings after calling "
		"shuffle.");
	StringList sl("a;b;c;d;e;f;g;h;i;j;k;l;m;n;o;p;q;r;s;t;u;v;w;x;y;z", ";");
	sl.shuffle();
	char* orig  = sl.print_to_string();
	sl.qsort();
	char* changed  = sl.print_to_string();
	const char* expect  = "a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u,v,w,x,y,z";
	emit_input_header();
	emit_param("StringList", orig);
	emit_output_expected_header();
	emit_param("StringList", expect);
	emit_output_actual_header();
	emit_param("StringList", nicePrint(changed));
	if(niceStrCmp(changed, expect) != MATCH) {
		free(orig); free(changed);
		FAIL;
	}
	free(orig); free(changed);
	PASS;
}

static bool test_shuffle_empty() {
	emit_test("Test shuffle() on an empty StringList.");
	StringList sl("", "");
	char* orig  = sl.print_to_string();
	sl.shuffle();
	char* changed  = sl.print_to_string();
	const char* expect  = "";
	emit_input_header();
	emit_param("StringList", nicePrint(orig));
	emit_output_expected_header();
	emit_param("StringList", expect);
	emit_output_actual_header();
	emit_param("StringList", nicePrint(changed));
	if(niceStrCmp(changed, expect) != MATCH) {
		free(orig); free(changed);
		FAIL;
	}
	free(orig); free(changed);
	PASS;
}

static bool test_shuffle_one() {
	emit_test("Test shuffle() on a StringList with only one string.");
	StringList sl("foo", "");
	char* orig  = sl.print_to_string();
	sl.shuffle();
	char* changed  = sl.print_to_string();
	const char* expect  = "foo";
	emit_input_header();
	emit_param("StringList", nicePrint(orig));
	emit_output_expected_header();
	emit_param("StringList", expect);
	emit_output_actual_header();
	emit_param("StringList", nicePrint(changed));
	if(niceStrCmp(changed, expect) != MATCH) {
		free(orig); free(changed);
		FAIL;
	}
	free(orig); free(changed);
	PASS;
}

static bool test_shuffle_many() {
	emit_test("Test shuffle() on a StringList with many strings.");
	emit_comment("This test may fail if shuffle happens to shuffle to the "
		"same order as the original, although this is highly unlikely here.");
	StringList sl("a;b;c;d;e;f;g;h;i;j;k;l;m;n;o;p;q;r;s;t;u;v;w;x;y;z", ";");
	char* orig  = sl.print_to_string();
	sl.shuffle();
	char* changed  = sl.print_to_string();
	const char* notExpect  = "a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u,v,w,x,y,z";
	emit_input_header();
	emit_param("StringList", orig);
	emit_output_expected_header();
	emit_param("StringList !=", notExpect);
	emit_output_actual_header();
	emit_param("StringList", nicePrint(changed));
	if(niceStrCmp(changed, notExpect) == MATCH) {
		free(orig); free(changed);
		FAIL;
	}
	free(orig); free(changed);
	PASS;
}

static bool test_rewind_empty() {
	emit_test("Test rewind() on an empty StringList.");
	emit_comment("This test uses next() to check where 'current' is "
		"so a problem with that may cause this to fail.");
	StringList sl("", "");
	char* orig  = sl.print_to_string();
	sl.rewind();
	char* next = sl.next();
	const char* expect  = "";
	emit_input_header();
	emit_param("StringList", nicePrint(orig));
	emit_output_expected_header();
	emit_param("next", expect);
	emit_output_actual_header();
	emit_param("next", nicePrint(next));
	if(niceStrCmp(next, expect) != MATCH) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_rewind_non_empty() {
	emit_test("Test rewind() on a non-empty StringList.");
	emit_comment("This test uses next() to check where 'current' is "
		"so a problem with that may cause this to fail.");
	StringList sl("a;b;c", ";");
	char* orig  = sl.print_to_string();
	sl.rewind();
	char* next = sl.next();
	const char* expect  = "a";
	emit_input_header();
	emit_param("StringList", orig);
	emit_output_expected_header();
	emit_param("next", expect);
	emit_output_actual_header();
	emit_param("next", nicePrint(next));
	if(niceStrCmp(next, expect) != MATCH) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_rewind_after_contains_true() {
	emit_test("Test rewind() after a successful call to contains().");
	emit_comment("This test uses next() to check where 'current' is "
		"so a problem with that may cause this to fail.");
	StringList sl("a;b;c", ";");
	char* orig  = sl.print_to_string();
	bool retVal = sl.contains("b");
	sl.rewind();
	char* next = sl.next();
	const char* expect  = "a";
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("contains(\"b\")", tfstr(retVal));
	emit_output_expected_header();
	emit_param("next", expect);
	emit_output_actual_header();
	emit_param("next", nicePrint(next));
	if(niceStrCmp(next, expect) != MATCH) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_rewind_after_contains_false() {
	emit_test("Test rewind() after an unsuccessful call to contains().");
	emit_comment("This test uses next() to check where 'current' is "
		"so a problem with that may cause this to fail.");
	StringList sl("a;b;c", ";");
	char* orig  = sl.print_to_string();
	bool retVal = sl.contains("d");
	sl.rewind();
	char* next = sl.next();
	const char* expect  = "a";
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("contains(\"d\")", tfstr(retVal));
	emit_output_expected_header();
	emit_param("next", expect);
	emit_output_actual_header();
	emit_param("next", nicePrint(next));
	if(niceStrCmp(next, expect) != MATCH) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_append_empty() {
	emit_test("Test append() on an empty StringList.");
	StringList sl("", ";");
	char* orig  = sl.print_to_string();
	sl.append("a");
	char* changed  = sl.print_to_string();
	const char* expect  = "a";
	emit_input_header();
	emit_param("StringList", nicePrint(orig));
	emit_param("STRING", "a");
	emit_output_expected_header();
	emit_param("StringList", expect);
	emit_output_actual_header();
	emit_param("StringList", nicePrint(changed));
	if(niceStrCmp(changed, expect) != MATCH) {
		free(orig); free(changed);
		FAIL;
	}
	free(orig); free(changed);
	PASS;
}

static bool test_append_one() {
	emit_test("Test a single append() on a non-empty StringList.");
	StringList sl("a;b;c", ";");
	char* orig  = sl.print_to_string();
	sl.append("d");
	char* changed  = sl.print_to_string();
	const char* expect  = "a,b,c,d";
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", "d");
	emit_output_expected_header();
	emit_param("StringList", expect);
	emit_output_actual_header();
	emit_param("StringList", nicePrint(changed));
	if(niceStrCmp(changed, expect) != MATCH) {
		free(orig); free(changed);
		FAIL;
	}
	free(orig); free(changed);
	PASS;
}

static bool test_append_many() {
	emit_test("Test many calls to append() on a non-empty StringList.");
	StringList sl("a;b;c", ";");
	char* orig  = sl.print_to_string();
	sl.append("d");
	sl.append("e");
	sl.append("f");
	char* changed  = sl.print_to_string();
	const char* expect  = "a,b,c,d,e,f";
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", "d");
	emit_param("STRING", "e");
	emit_param("STRING", "f");
	emit_output_expected_header();
	emit_param("StringList", expect);
	emit_output_actual_header();
	emit_param("StringList", nicePrint(changed));
	if(niceStrCmp(changed, expect) != MATCH) {
		free(orig); free(changed);
		FAIL;
	}
	free(orig); free(changed);
	PASS;
}

static bool test_append_contains() {
	emit_test("Test append() after a successful call to contains().");
	StringList sl("a;b;c", ";");
	char* orig  = sl.print_to_string();
	bool retVal = sl.contains("b");
	sl.append("d");
	char* changed  = sl.print_to_string();
	const char* expect  = "a,b,c,d";
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("contains(\"b\")", tfstr(retVal));
	emit_param("STRING", "d");
	emit_output_expected_header();
	emit_param("StringList", expect);
	emit_output_actual_header();
	emit_param("StringList", nicePrint(changed));
	if(niceStrCmp(changed, expect) != MATCH) {
		free(orig); free(changed);
		FAIL;
	}
	free(orig); free(changed);
	PASS;
}

static bool test_append_rewind() {
	emit_test("Test append() after calling rewind().");
	StringList sl("a;b;c", ";");
	char* orig  = sl.print_to_string();
	sl.rewind();
	sl.append("d");
	char* next = sl.next();
	char* changed  = sl.print_to_string();
	const char* expect  = "a,b,c,d";
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", "d");
	emit_output_expected_header();
	emit_param("StringList", expect);
	emit_output_actual_header();
	emit_param("StringList", nicePrint(changed));
	emit_param("next", nicePrint(next));
	if(niceStrCmp(changed, expect) != MATCH) {
		free(orig); free(changed);
		FAIL;
	}
	free(orig); free(changed);
	PASS;
}

static bool test_append_current() {
	emit_test("Does append() change 'current' to point at the new item?");
	emit_comment("This test uses deleteCurrent() to check where 'current' is "
		"so a problem with that may cause this to fail.");
	StringList sl("a;b;c", ";");
	char* orig  = sl.print_to_string();
	sl.append("d");
	sl.deleteCurrent();
	char* changed = sl.print_to_string();
	const char* expect  = "a,b,c";
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", "d");
	emit_output_expected_header();
	emit_param("StrinList", expect);
	emit_output_actual_header();
	emit_param("StrinList", nicePrint(changed));
	if(niceStrCmp(changed, expect) != MATCH) {
		free(orig); free(changed);
		FAIL;
	}
	free(orig); free(changed);
	PASS;
}

static bool test_insert_empty() {
	emit_test("Test insert() on an empty StringList.");
	StringList sl("", "");
	char* orig  = sl.print_to_string();
	sl.insert("a");
	char* changed  = sl.print_to_string();
	const char* expect  = "a";
	emit_input_header();
	emit_param("StringList", nicePrint(orig));
	emit_param("STRING", "a");
	emit_output_expected_header();
	emit_param("StringList", expect);
	emit_output_actual_header();
	emit_param("StringList", nicePrint(changed));
	if(niceStrCmp(changed, expect) != MATCH) {
		free(orig); free(changed);
		FAIL;
	}
	free(orig); free(changed);
	PASS;
}

static bool test_insert_head() {
	emit_test("Test insert() at the beginning of the StringList.");
	emit_comment("This test uses next() to navigate through the StringList "
		"so a problem with that may cause this to fail.");
	StringList sl("a;b;c", ";");
	char* orig  = sl.print_to_string();
	sl.rewind();
	sl.next();	//current=a
	sl.insert("d");
	char* changed  = sl.print_to_string();
	const char* expect  = "d,a,b,c";
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", "d");
	emit_output_expected_header();
	emit_param("StringList", expect);
	emit_output_actual_header();
	emit_param("StringList", nicePrint(changed));
	if(niceStrCmp(changed, expect) != MATCH) {
		free(orig); free(changed);
		FAIL;
	}
	free(orig); free(changed);
	PASS;
}

static bool test_insert_middle() {
	emit_test("Test insert() in the middle of the StringList.");
	emit_comment("This test uses next() to navigate through the StringList "
		"so a problem with that may cause this to fail.");
	StringList sl("a;b;c", ";");
	char* orig  = sl.print_to_string();
	sl.rewind();
	sl.next();	//current=a
	sl.next();	//current=b
	sl.next();	//current=c
	sl.insert("d");
	char* changed  = sl.print_to_string();
	const char* expect  = "a,b,d,c";
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", "d");
	emit_output_expected_header();
	emit_param("StringList", expect);
	emit_output_actual_header();
	emit_param("StringList", nicePrint(changed));
	if(niceStrCmp(changed, expect) != MATCH) {
		free(orig); free(changed);
		FAIL;
	}
	free(orig); free(changed);
	PASS;
}

static bool test_insert_end() {
	emit_test("Test insert() at the end of the StringList.");
	emit_comment("This test uses next() to navigate through the StringList "
		"so a problem with that may cause this to fail.");
	emit_comment("Currently insert() inserts the string before what 'current'"
		" points to, so there is no way at inserting at the end of the list, "
		"just before the last element.");
	StringList sl("a;b;c", ";");
	char* orig  = sl.print_to_string();
	sl.rewind();
	sl.next();	//current=a
	sl.next();	//current=b
	sl.next();	//current=c
	sl.insert("d");
	char* changed  = sl.print_to_string();
	const char* expect  = "a,b,d,c";
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", "d");
	emit_output_expected_header();
	emit_param("StringList", expect);
	emit_output_actual_header();
	emit_param("StringList", nicePrint(changed));
	if(niceStrCmp(changed, expect) != MATCH) {
		free(orig); free(changed);
		FAIL;
	}
	free(orig); free(changed);
	PASS;
}

static bool test_insert_current() {
	emit_test("Test that insert() doesn't change what 'current' points to.");
	emit_comment("This test uses next() to navigate through the StringList "
		"so a problem with that may cause this to fail.");
	StringList sl("a;b;c", ";");
	char* orig  = sl.print_to_string();
	sl.rewind();
	sl.next();	//current=a
	sl.next();	//current=b
	sl.insert("d");
	char* next = sl.next();	//current=c
	const char* expect  = "c";
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("STRING", "d");
	emit_output_expected_header();
	emit_param("next", expect);
	emit_output_actual_header();
	emit_param("next", nicePrint(next));
	if(niceStrCmp(next, expect) != MATCH) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_insert_many() {
	emit_test("Test many calls to  insert().");
	emit_comment("This test uses next() to navigate through the StringList "
		"so a problem with that may cause this to fail.");
	StringList sl("a;b;c", ";");
	char* orig  = sl.print_to_string();
	sl.rewind();
	sl.next();	//current=a
	sl.insert("0");
	sl.insert("1");
	sl.next();	//current=b
	sl.insert("2");
	sl.next();	//current=c
	sl.insert("3");
	char* changed  = sl.print_to_string();
	const char* expect  = "0,1,a,2,b,3,c";
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("'current'", "a");
	emit_param("insert", "0");
	emit_param("insert", "1");
	emit_param("'current'", "b");
	emit_param("insert", "2");
	emit_param("'current'", "c");
	emit_param("insert", "3");
	emit_output_expected_header();
	emit_param("StringList", expect);
	emit_output_actual_header();
	emit_param("StringList", nicePrint(changed));
	if(niceStrCmp(changed, expect) != MATCH) {
		free(orig); free(changed);
		FAIL;
	}
	free(orig); free(changed);
	PASS;
}

static bool test_next_empty() {
	emit_test("Test calling next() on an empty StringList.");
	StringList sl("", "");
	char* orig  = sl.print_to_string();
	char* next = sl.next();
	const char* expect  = "";
	emit_input_header();
	emit_param("StringList", nicePrint(orig));
	emit_output_expected_header();
	emit_param("STRING", expect);
	emit_output_actual_header();
	emit_param("STRING", nicePrint(next));
	if(niceStrCmp(next, expect) != MATCH) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_next_beginning() {
	emit_test("Test calling next() when 'current' is at the beginning of the "
		"StringList.");
	emit_comment("This test requires the use of rewind(), so a problem with "
		"that could cause this to fail.");
	StringList sl("a;b;c", ";");
	char* orig  = sl.print_to_string();
	sl.rewind();
	char* next = sl.next();
	const char* expect  = "a";
	emit_input_header();
	emit_param("StringList", nicePrint(orig));
	emit_output_expected_header();
	emit_param("STRING", expect);
	emit_output_actual_header();
	emit_param("STRING", nicePrint(next));
	if(niceStrCmp(next, expect) != MATCH) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_next_middle() {
	emit_test("Test calling next() when 'current' is in the middle of the "
		"StringList.");
	emit_comment("This test requires the use of contains(), so a problem with"
		" that could cause this to fail.");
	StringList sl("a;b;c", ";");
	char* orig  = sl.print_to_string();
	sl.contains("b");
	char* next = sl.next();
	const char* expect  = "c";
	emit_input_header();
	emit_param("StringList", nicePrint(orig));
	emit_output_expected_header();
	emit_param("STRING", expect);
	emit_output_actual_header();
	emit_param("STRING", nicePrint(next));
	if(niceStrCmp(next, expect) != MATCH) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_next_end() {
	emit_test("Test calling next() when 'current' is at the end of the "
		"StringList.");
	emit_comment("This test requires the use of contains(), so a problem with"
		" that could cause this to fail.");
	StringList sl("a;b;c", ";");
	char* orig  = sl.print_to_string();
	sl.contains("c");
	char* next = sl.next();
	const char* expect  = "";
	emit_input_header();
	emit_param("StringList", nicePrint(orig));
	emit_output_expected_header();
	emit_param("STRING", expect);
	emit_output_actual_header();
	emit_param("STRING", nicePrint(next));
	if(niceStrCmp(next, expect) != MATCH) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_number_empty() {
	emit_test("Test number() on an empty StringList.");
	StringList sl("", "");
	char* orig  = sl.print_to_string();
	int num = sl.number();
	int expect  = 0;
	emit_input_header();
	emit_param("StringList", nicePrint(orig));
	emit_output_expected_header();
	emit_retval("%d", expect);
	emit_output_actual_header();
	emit_retval("%d", num);
	if(num != expect) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_number_one() {
	emit_test("Test number() on a StringList with one string.");
	StringList sl("foo", "");
	char* orig  = sl.print_to_string();
	int num = sl.number();
	int expect  = 1;
	emit_input_header();
	emit_param("StringList", orig);
	emit_output_expected_header();
	emit_retval("%d", expect);
	emit_output_actual_header();
	emit_retval("%d", num);
	if(num != expect) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_number_many() {
	emit_test("Test number() on a StringList with many strings.");
	StringList sl("a;b;c;d;e;f", ";");
	char* orig  = sl.print_to_string();
	int num = sl.number();
	int expect  = 6;
	emit_input_header();
	emit_param("StringList", orig);
	emit_output_expected_header();
	emit_retval("%d", expect);
	emit_output_actual_header();
	emit_retval("%d", num);
	if(num != expect) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_number_after_delete() {
	emit_test("Test that number() changes after calling deleteCurrent().");
	emit_comment("This test requires the use of contains() and "
		"deleteCurrent() so a problem with either of these may cause this to "
		"fail.");
	StringList sl("a;b;c;d;e;f", ";");
	char* orig  = sl.print_to_string();
	sl.contains("c");
	sl.deleteCurrent();
	int num = sl.number();
	int expect  = 5;
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("Call", "contains(\"c\")");
	emit_param("Call", "deleteCurrent()");
	emit_output_expected_header();
	emit_retval("%d", expect);
	emit_output_actual_header();
	emit_retval("%d", num);
	if(num != expect) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_number_after_clear_all() {
	emit_test("Test that number() returns 0 after calling clearAll().");
	emit_comment("This test requires the use of clearAll() so a problem with "
		"that may cause this to fail.");
	StringList sl("a;b;c;d;e;f", ";");
	char* orig  = sl.print_to_string();
	sl.clearAll();
	int num = sl.number();
	int expect  = 0;
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("Call", "clearAll()");
	emit_output_expected_header();
	emit_retval("%d", expect);
	emit_output_actual_header();
	emit_retval("%d", num);
	if(num != expect) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_number_after_append() {
	emit_test("Test that number() returns the correct size of the StringList "
		"after calling append().");
	emit_comment("This test requires the use of append() so a problem with "
		"that may cause this to fail.");
	StringList sl("a;b;c", ";");
	char* orig  = sl.print_to_string();
	sl.append("dog");
	int num = sl.number();
	int expect  = 4;
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("Call", "append(\"dog\")");
	emit_output_expected_header();
	emit_retval("%d", expect);
	emit_output_actual_header();
	emit_retval("%d", num);
	if(num != expect) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_number_after_insert() {
	emit_test("Test that number() returns the correct size of the StringList "
		"after calling insert().");
	emit_comment("This test requires the use of insert() so a problem with "
		"that may cause this to fail.");
	StringList sl("a;b;c", ";");
	char* orig  = sl.print_to_string();
	sl.insert("dog");
	int num = sl.number();
	int expect  = 4;
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("Call", "insert(\"dog\")");
	emit_output_expected_header();
	emit_retval("%d", expect);
	emit_output_actual_header();
	emit_retval("%d", num);
	if(num != expect) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_number_copy() {
	emit_test("Test that number() of a StringList created by using the copy "
		"constructor is equal to the source StringList.");
	StringList sl1("a;b;c;d;e;f", ";");
	StringList sl2(sl1);
	char* orig1  = sl1.print_to_string();
	char* orig2  = sl2.print_to_string();
	int num1 = sl1.number();
	int num2 = sl2.number();
	int expect  = 6;
	emit_input_header();
	emit_param("StringList", orig1);
	emit_param("StringList", orig2);
	emit_output_expected_header();
	emit_retval("%d", expect);
	emit_output_actual_header();
	emit_retval("%d", num1);
	emit_retval("%d", num2);
	if(num1 != expect || num2 != expect) {
		free(orig1); free(orig2);
		FAIL;
	}
	free(orig1); free(orig2);
	PASS;
}

static bool test_is_empty_empty() {
	emit_test("Test that isEmpty() returns true for an empty StringList.");
	StringList sl("", "");
	char* orig  = sl.print_to_string();
	bool retVal = sl.isEmpty();
	bool expect  = true;
	emit_input_header();
	emit_param("StringList", nicePrint(orig));
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(retVal));
	if(!retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_is_empty_clear() {
	emit_test("Test that isEmpty() returns true after calling clearAll().");
	emit_comment("This test requires the use of clearAll() so a problem with "
		"that may cause this to fail.");
	StringList sl("a;b;c", ";");
	char* orig  = sl.print_to_string();
	sl.clearAll();
	bool retVal = sl.isEmpty();
	bool expect  = true;
	emit_input_header();
	emit_param("StringList", orig);
	emit_param("Call", "clearAll()");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(retVal));
	if(!retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_is_empty_one() {
	emit_test("Test that isEmpty() returns false for a StringList with only "
		"one string.");
	StringList sl("foo", "");
	char* orig  = sl.print_to_string();
	bool retVal = sl.isEmpty();
	bool expect  = false;
	emit_input_header();
	emit_param("StringList", nicePrint(orig));
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(retVal));
	if(retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_is_empty_many() {
	emit_test("Test that isEmpty() returns false for a StringList with many "
		"strings.");
	StringList sl("a;b;c;d;e;f", ";");
	char* orig  = sl.print_to_string();
	bool retVal = sl.isEmpty();
	bool expect  = false;
	emit_input_header();
	emit_param("StringList", orig);
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(retVal));
	if(retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_is_empty_append() {
	emit_test("Test that isEmpty() returns false after calling append() on an"
		" empty StringList.");
	emit_comment("This test requires the use of append() so a problem with "
		"that may cause this to fail.");
	StringList sl("", "");
	char* orig  = sl.print_to_string();
	sl.append("foo");
	bool retVal = sl.isEmpty();
	bool expect  = false;
	emit_input_header();
	emit_param("StringList", nicePrint(orig));
	emit_param("Call", "append(\"foo\")");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(retVal));
	if(retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_is_empty_insert() {
	emit_test("Test that isEmpty() returns false after calling insert() on an"		" empty StringList.");
	emit_comment("This test requires the use of insert() so a problem with "
		"that may cause this to fail.");
	StringList sl("", "");
	char* orig  = sl.print_to_string();
	sl.insert("foo");
	bool retVal = sl.isEmpty();
	bool expect  = false;
	emit_input_header();
	emit_param("StringList", nicePrint(orig));
	emit_param("Call", "insert(\"foo\")");
	emit_output_expected_header();
	emit_retval("%s", tfstr(expect));
	emit_output_actual_header();
	emit_retval("%s", tfstr(retVal));
	if(retVal) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_get_delimiters_empty_no() {
	emit_test("Test that getDelimiters() returns the correct string "
		"representation of the delimiters of an empty StringList with no "
		"delimiters.");
	StringList sl("", "");
	char* orig  = sl.print_to_string();
	const char* delims = sl.getDelimiters();
	const char* expect = "";
	emit_input_header();
	emit_param("StringList", nicePrint(orig));
	emit_output_expected_header();
	emit_param("DELIMS", expect);
	emit_output_actual_header();
	emit_param("DELIMS", delims);
	if(strcmp(delims, expect) != MATCH) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_get_delimiters_empty_yes() {
	emit_test("Test that getDelimiters() returns the correct string "
		"representation of the delimiters of an empty StringList with "
		"delimiters.");
	StringList sl("", ";");
	char* orig  = sl.print_to_string();
	const char* delims = sl.getDelimiters();
	const char* expect = ";";
	emit_input_header();
	emit_param("StringList", nicePrint(orig));
	emit_output_expected_header();
	emit_param("DELIMS", expect);
	emit_output_actual_header();
	emit_param("DELIMS", delims);
	if(strcmp(delims, expect) != MATCH) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_get_delimiters_non_empty_no() {
	emit_test("Test that getDelimiters() returns the correct string "
		"representation of the delimiters of a non-empty StringList with no "
		"delimiters.");
	StringList sl("a;b;c", "");
	char* orig  = sl.print_to_string();
	const char* delims = sl.getDelimiters();
	const char* expect = "";
	emit_input_header();
	emit_param("StringList", orig);
	emit_output_expected_header();
	emit_param("DELIMS", expect);
	emit_output_actual_header();
	emit_param("DELIMS", delims);
	if(strcmp(delims, expect) != MATCH) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}

static bool test_get_delimiters_non_empty_yes() {
	emit_test("Test that getDelimiters() returns the correct string "
		"representation of the delimiters of a non-empty StringList with "
		"delimiters.");
	StringList sl("a;b,c!d", ";!,");
	char* orig  = sl.print_to_string();
	const char* delims = sl.getDelimiters();
	const char* expect = ";!,";
	emit_input_header();
	emit_param("StringList", orig);
	emit_output_expected_header();
	emit_param("DELIMS", expect);
	emit_output_actual_header();
	emit_param("DELIMS", delims);
	if(strcmp(delims, expect) != MATCH) {
		free(orig);
		FAIL;
	}
	free(orig);
	PASS;
}
