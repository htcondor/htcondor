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


/* Test the MyString implementation.
 */

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "MyString.h"
#include "function_test_driver.h"
#include "unit_test_utils.h"
#include "emit.h"

// function prototypes
static bool default_constructor(void);
static bool int_constructor(void);
static bool char_constructor(void);
static bool copy_constructor_value_check(void);
static bool copy_constructor_pointer_check(void);
static bool length_0(void);
static bool length_1(void);
static bool length_2(void);
static bool length_after_change(void);
static bool is_empty_initialized(void);
static bool is_empty_after_change(void);
static bool is_empty_false(void);
static bool capacity_empty(void);
static bool capacity_greater_than_length(void); 
static bool str_dup_empty(void);
static bool str_dup_empty_new(void);
static bool str_dup_non_empty(void);
static bool str_dup_non_empty_new(void);
static bool value_of_empty(void);
static bool value_of_non_empty(void);
static bool square_brackets_operator_invalid_negative(void);
static bool square_brackets_operator_invalid_empty(void);
static bool square_brackets_operator_invalid_past(void);
static bool square_brackets_operator_first_char(void);
static bool square_brackets_operator_last_char(void);
static bool set_char_empty(void);
static bool set_char_truncate(void);
static bool set_char_first(void);
static bool set_char_last(void);
static bool random_string_one_char(void);
static bool random_string_many_chars(void);
static bool random_string_existing(void);
static bool random_string_empty_set(void);
static bool random_string_empty_set_existing(void);
static bool random_string_length_zero(void);
static bool random_string_negative_length(void);
static bool random_string_hex(void);
static bool equals_operator_my_string_empty_non_empty(void);
static bool equals_operator_my_string_non_empty_non_empty(void);
static bool equals_operator_my_string_non_empty_empty(void);
static bool equals_operator_string_empty_non_empty(void);
static bool equals_operator_string_non_empty_non_empty(void);
static bool equals_operator_string_non_empty_empty(void);
static bool equals_operator_string_null(void);
static bool reserve_invalid_negative(void);
static bool reserve_shortening_1(void);
static bool reserve_shortening_0(void);
static bool reserve_shortening_smaller(void);
static bool reserve_no_changes(void);
static bool reserve_empty_my_string(void);
static bool reserve_larger_capacity(void);
static bool reserve_smaller_capacity(void);
static bool reserve_at_least_negative(void);
static bool reserve_at_least_zero(void);
static bool reserve_at_least_no_change(void);
static bool reserve_at_least_capacity(void);
static bool concatenation_my_string_non_empty_empty(void);
static bool concatenation_my_string_empty_empty(void);
static bool concatenation_my_string_empty_non_empty(void);
static bool concatenation_my_string_non_empty_non_empty(void);
static bool concatenation_my_string_multiple(void);
static bool concatenation_my_string_itself(void);
static bool concatenation_my_string_itself_empty(void);
static bool concatenation_string_non_empty_null(void);
static bool concatenation_string_empty_empty(void);
static bool concatenation_string_empty_non_empty(void);
static bool concatenation_string_non_empty_non_empty(void);
static bool concatenation_char_empty(void);
static bool concatenation_char_non_empty(void);
static bool concatenation_char_null(void);
static bool concatenation_int_empty(void);
static bool concatenation_int_non_empty(void);
static bool concatenation_int_large(void);
static bool concatenation_int_small(void);
static bool concatenation_uint_empty(void);
static bool concatenation_uint_non_empty(void);
static bool concatenation_uint_large(void);
static bool concatenation_long_empty(void);
static bool concatenation_long_non_empty(void);
static bool concatenation_long_large(void);
static bool concatenation_long_small(void);
static bool concatenation_double_empty(void);
static bool concatenation_double_non_empty(void);
static bool substr_empty(void);
static bool substr_beginning(void);
static bool substr_end(void);
static bool substr_outside_beginning(void);
static bool substr_outside_end(void);
static bool escape_chars_length_1(void);
static bool escape_chars_empty(void);
static bool escape_chars_multiple(void);
static bool escape_chars_same(void);
static bool find_char_first(void);
static bool find_char_last(void);
static bool find_char_not_found(void);
static bool find_char_invalid_greater(void);
static bool find_char_invalid_less(void);
static bool hash_empty(void);
static bool hash_non_empty(void);
static bool find_empty(void);
static bool find_non_empty(void);
static bool find_beginning(void);
static bool find_middle(void);
static bool find_substring_not_found(void);
static bool find_invalid_start_less(void);
static bool find_invalid_start_greater(void);
static bool replace_string_valid(void);
static bool replace_string_empty(void);
static bool replace_string_empty_my_string(void);
static bool replace_string_beginning(void);
static bool replace_string_end(void);
static bool replace_string_return_false(void);
static bool replace_string_return_true(void);
static bool sprintf_empty(void);
static bool sprintf_non_empty(void);
static bool sprintf_return_true(void);
static bool vsprintf_empty(void);
static bool vsprintf_non_empty(void);
static bool vsprintf_return_true(void);
static bool sprintf_cat_empty(void);
static bool sprintf_cat_non_empty(void);
static bool sprintf_cat_return_true(void);
static bool vsprintf_cat_empty(void);
static bool vsprintf_cat_non_empty(void);
static bool vsprintf_cat_return_true(void);
static bool append_to_list_string_empty_empty(void);
static bool append_to_list_string_empty_non_empty(void);
static bool append_to_list_string_non_empty_empty(void);
static bool append_to_list_string_non_empty(void);
static bool append_to_list_string_multiple(void);
static bool append_to_list_mystring_empty_empty(void);
static bool append_to_list_mystring_empty_non_empty(void);
static bool append_to_list_mystring_non_empty_empty(void);
static bool append_to_list_mystring_non_empty(void);
static bool append_to_list_mystring_multiple(void);
static bool lower_case_empty(void);
static bool lower_case_non_empty(void);
static bool upper_case_empty(void);
static bool upper_case_non_empty(void);
static bool chomp_new_line_end(void);
static bool chomp_new_line_beginning(void);
static bool chomp_return_false(void);
static bool chomp_return_true(void);
static bool trim_beginning_and_end(void);
static bool trim_end(void);
static bool trim_none(void);
static bool trim_beginning(void);
static bool trim_empty(void);
static bool equality_mystring_copied(void);
static bool equality_mystring_one_char(void);
static bool equality_mystring_two_char(void);
static bool equality_mystring_substring(void);
static bool equality_mystring_one_empty(void);
static bool equality_mystring_two_empty(void);
static bool equality_string_one_char(void);
static bool equality_string_copied_delete(void);
static bool equality_string_two_char(void);
static bool equality_string_substring(void);
static bool equality_string_one_empty(void);
static bool equality_string_two_empty(void);
static bool inequality_mystring_copy(void);
static bool inequality_mystring_not_equal(void);
static bool inequality_string_equal(void);
static bool inequality_string_not_equal(void);
static bool less_than_mystring_less(void);
static bool less_than_mystring_equal(void);
static bool less_than_mystring_greater(void);
static bool less_than_or_equal_mystring_less(void);
static bool less_than_or_equal_mystring_equal(void);
static bool less_than_or_equal_mystring_greater(void);
static bool greater_than_mystring_less(void);
static bool greater_than_mystring_equal(void);
static bool greater_than_mystring_greater(void);
static bool greater_than_or_equal_mystring_less(void);
static bool greater_than_or_equal_mystring_equal(void);
static bool greater_than_or_equal_mystring_greater(void);
static bool read_line_empty_file_return(void);
static bool read_line_empty_file_preserve(void);
static bool read_line_empty(void);
static bool read_line_null_file(void);
static bool read_line_append(void);
static bool read_line_return(void);
static bool read_line_replace(void);
static bool read_line_multiple(void);
static bool tokenize_null(void);
static bool tokenize_skip(void);
static bool tokenize_multiple_calls(void);
static bool tokenize_end(void);
static bool tokenize_empty(void);
static bool tokenize_empty_delimiter(void);
static bool sensitive_string_string_constructor(void);
static bool sensitive_string_equality_true(void);
static bool sensitive_string_equality_false(void);
static bool sensitive_string_equality_default_constructor(void);
static bool sensitive_string_assignment_non_empty_empty(void);
static bool sensitive_string_assignment_empty_non_empty(void);
static bool sensitive_string_assignment_non_empty(void);
static bool sensitive_string_hash_function_non_empty(void);
static bool sensitive_string_hash_function_empty(void);

bool OTEST_MyString() {
		// beginning junk
	e.emit_object("MyString");
	e.emit_comment("The MyString class is a C++ representation of a string. It "
		"was written before we could reliably use the standard string class. "
		"For an example of how to use it, see test_mystring.C. A warning to "
		"anyone changing this code: as currently implemented, an 'empty' "
		"MyString can have two different possible internal representations "
		"depending on its history. Sometimes Data == NULL, sometimes Data[0] =="
		" '\\0'.  So don't assume one or the other...  We should change this to"
		" never having NULL, but there is worry that someone depends on this "
		"behavior.");
	
		// driver to run the tests and all required setup
	FunctionDriver driver(200);
	driver.register_function(default_constructor);
	driver.register_function(int_constructor);
	driver.register_function(char_constructor);
	driver.register_function(copy_constructor_value_check);
	driver.register_function(copy_constructor_pointer_check);
	driver.register_function(length_0);
	driver.register_function(length_1);
	driver.register_function(length_2);
	driver.register_function(length_after_change);
	driver.register_function(is_empty_initialized);
	driver.register_function(is_empty_after_change);
	driver.register_function(is_empty_false);
	driver.register_function(capacity_empty);		
	driver.register_function(capacity_greater_than_length);
	driver.register_function(str_dup_empty);
	driver.register_function(str_dup_empty_new);
	driver.register_function(str_dup_non_empty);
	driver.register_function(str_dup_non_empty_new);
	driver.register_function(value_of_empty);
	driver.register_function(value_of_non_empty);
	driver.register_function(square_brackets_operator_invalid_negative);
	driver.register_function(square_brackets_operator_invalid_empty);
	driver.register_function(square_brackets_operator_invalid_past);
	driver.register_function(square_brackets_operator_first_char);
	driver.register_function(square_brackets_operator_last_char);
	driver.register_function(set_char_empty);
	driver.register_function(set_char_truncate);
	driver.register_function(set_char_first);
	driver.register_function(set_char_last);
	driver.register_function(random_string_one_char);
	driver.register_function(random_string_many_chars);
	driver.register_function(random_string_existing);
	driver.register_function(random_string_empty_set);
	driver.register_function(random_string_empty_set_existing);
	driver.register_function(random_string_length_zero);
	driver.register_function(random_string_negative_length);
	driver.register_function(random_string_hex);
	driver.register_function(equals_operator_my_string_empty_non_empty);
	driver.register_function(equals_operator_my_string_non_empty_non_empty);
	driver.register_function(equals_operator_my_string_non_empty_empty);
	driver.register_function(equals_operator_string_empty_non_empty);
	driver.register_function(equals_operator_string_non_empty_non_empty);
	driver.register_function(equals_operator_string_non_empty_empty);
	driver.register_function(equals_operator_string_null);
	driver.register_function(reserve_invalid_negative);
	driver.register_function(reserve_shortening_1);
	driver.register_function(reserve_shortening_0);
	driver.register_function(reserve_shortening_smaller);
	driver.register_function(reserve_no_changes);
	driver.register_function(reserve_empty_my_string);
	driver.register_function(reserve_larger_capacity);
	driver.register_function(reserve_smaller_capacity);
	driver.register_function(reserve_at_least_negative);
	driver.register_function(reserve_at_least_zero);
	driver.register_function(reserve_at_least_no_change);
	driver.register_function(reserve_at_least_capacity);
	driver.register_function(concatenation_my_string_non_empty_empty);
	driver.register_function(concatenation_my_string_empty_empty);
	driver.register_function(concatenation_my_string_empty_non_empty);
	driver.register_function(concatenation_my_string_non_empty_non_empty);
	driver.register_function(concatenation_my_string_multiple);
	driver.register_function(concatenation_my_string_itself);
	driver.register_function(concatenation_my_string_itself_empty);
	driver.register_function(concatenation_string_non_empty_null);
	driver.register_function(concatenation_string_empty_empty);
	driver.register_function(concatenation_string_empty_non_empty);
	driver.register_function(concatenation_string_non_empty_non_empty);
	driver.register_function(concatenation_char_empty);
	driver.register_function(concatenation_char_non_empty);
	driver.register_function(concatenation_char_null);
	driver.register_function(concatenation_int_empty);
	driver.register_function(concatenation_int_non_empty);
	driver.register_function(concatenation_int_large);
	driver.register_function(concatenation_int_small);
	driver.register_function(concatenation_uint_empty);
	driver.register_function(concatenation_uint_non_empty);
	driver.register_function(concatenation_uint_large);
	driver.register_function(concatenation_long_empty);
	driver.register_function(concatenation_long_non_empty);
	driver.register_function(concatenation_long_large);
	driver.register_function(concatenation_long_small);
	driver.register_function(concatenation_double_empty);
	driver.register_function(concatenation_double_non_empty);
	driver.register_function(substr_empty);
	driver.register_function(substr_beginning);
	driver.register_function(substr_end);
	driver.register_function(substr_outside_beginning);
	driver.register_function(substr_outside_end);
	driver.register_function(escape_chars_length_1);
	driver.register_function(escape_chars_empty);
	driver.register_function(escape_chars_multiple);
	driver.register_function(escape_chars_same);
	driver.register_function(find_char_first);
	driver.register_function(find_char_last);
	driver.register_function(find_char_not_found);
	driver.register_function(find_char_invalid_greater);
	driver.register_function(find_char_invalid_less);
	driver.register_function(hash_empty);
	driver.register_function(hash_non_empty);
	driver.register_function(find_empty);
	driver.register_function(find_non_empty);
	driver.register_function(find_beginning);
	driver.register_function(find_middle);
	driver.register_function(find_substring_not_found);
	driver.register_function(find_invalid_start_less);
	driver.register_function(find_invalid_start_greater);
	driver.register_function(replace_string_valid);
	driver.register_function(replace_string_empty);
	driver.register_function(replace_string_empty_my_string);
	driver.register_function(replace_string_beginning);
	driver.register_function(replace_string_end);
	driver.register_function(replace_string_return_false);
	driver.register_function(replace_string_return_true);
	driver.register_function(sprintf_empty);
	driver.register_function(sprintf_non_empty);
	driver.register_function(sprintf_return_true);
	driver.register_function(vsprintf_empty);
	driver.register_function(vsprintf_non_empty);
	driver.register_function(vsprintf_return_true);
	driver.register_function(sprintf_cat_empty);
	driver.register_function(sprintf_cat_non_empty);
	driver.register_function(sprintf_cat_return_true);
	driver.register_function(vsprintf_cat_empty);
	driver.register_function(vsprintf_cat_non_empty);
	driver.register_function(vsprintf_cat_return_true);
	driver.register_function(append_to_list_string_empty_empty);
	driver.register_function(append_to_list_string_empty_non_empty);
	driver.register_function(append_to_list_string_non_empty_empty);
	driver.register_function(append_to_list_string_non_empty);
	driver.register_function(append_to_list_string_multiple);
	driver.register_function(append_to_list_mystring_empty_empty);
	driver.register_function(append_to_list_mystring_empty_non_empty);
	driver.register_function(append_to_list_mystring_non_empty_empty);
	driver.register_function(append_to_list_mystring_non_empty);
	driver.register_function(append_to_list_mystring_multiple);
	driver.register_function(lower_case_empty);
	driver.register_function(lower_case_non_empty);
	driver.register_function(upper_case_empty);
	driver.register_function(upper_case_non_empty);
	driver.register_function(chomp_new_line_end);
	driver.register_function(chomp_new_line_beginning);
	driver.register_function(chomp_return_false);
	driver.register_function(chomp_return_true);
	driver.register_function(trim_beginning_and_end);
	driver.register_function(trim_end);
	driver.register_function(trim_none);
	driver.register_function(trim_beginning);
	driver.register_function(trim_empty);
	driver.register_function(equality_mystring_copied);
	driver.register_function(equality_mystring_one_char);
	driver.register_function(equality_mystring_two_char);
	driver.register_function(equality_mystring_substring);
	driver.register_function(equality_mystring_one_empty);
	driver.register_function(equality_mystring_two_empty);
	driver.register_function(equality_string_one_char);
	driver.register_function(equality_string_two_char);
	driver.register_function(equality_string_copied_delete);
	driver.register_function(equality_string_substring);
	driver.register_function(equality_string_one_empty);
	driver.register_function(equality_string_two_empty);
	driver.register_function(inequality_mystring_copy);
	driver.register_function(inequality_mystring_not_equal);
	driver.register_function(inequality_string_equal);
	driver.register_function(inequality_string_not_equal);
	driver.register_function(less_than_mystring_less);
	driver.register_function(less_than_mystring_equal);
	driver.register_function(less_than_mystring_greater);
	driver.register_function(less_than_or_equal_mystring_less);
	driver.register_function(less_than_or_equal_mystring_equal);
	driver.register_function(less_than_or_equal_mystring_greater);
	driver.register_function(greater_than_mystring_less);
	driver.register_function(greater_than_mystring_equal);
	driver.register_function(greater_than_mystring_greater);
	driver.register_function(greater_than_or_equal_mystring_less);
	driver.register_function(greater_than_or_equal_mystring_equal);
	driver.register_function(greater_than_or_equal_mystring_greater);
	driver.register_function(read_line_empty_file_return);
	driver.register_function(read_line_empty_file_preserve);
	driver.register_function(read_line_empty);
	driver.register_function(read_line_null_file);
	driver.register_function(read_line_append);
	driver.register_function(read_line_return);
	driver.register_function(read_line_replace);
	driver.register_function(read_line_multiple);
	driver.register_function(tokenize_null);
	driver.register_function(tokenize_skip);
	driver.register_function(tokenize_multiple_calls);
	driver.register_function(tokenize_end);
	driver.register_function(tokenize_empty);
	driver.register_function(tokenize_empty_delimiter);
	driver.register_function(sensitive_string_string_constructor);
	driver.register_function(sensitive_string_equality_true);
	driver.register_function(sensitive_string_equality_false);
	driver.register_function(sensitive_string_equality_default_constructor);
	driver.register_function(sensitive_string_assignment_non_empty_empty);
	driver.register_function(sensitive_string_assignment_empty_non_empty);
	driver.register_function(sensitive_string_assignment_non_empty);
	driver.register_function(sensitive_string_hash_function_non_empty);
	driver.register_function(sensitive_string_hash_function_empty);

		// run the tests
	bool test_result = driver.do_all_functions();
	e.emit_function_break();
	return test_result;
}

static bool default_constructor() {
	e.emit_test("Test default MyString constructor");
	MyString s;
	e.emit_output_expected_header();
	e.emit_retval("");
	e.emit_output_actual_header();
	e.emit_retval("%s", s.Value());
	if(strcmp(s.Value(), "") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 655
static bool int_constructor(void) {
	e.emit_test("Test constructor to make an integer string");
	const int param = 123;
	MyString s(param);
	const char *expected = "123";
	e.emit_input_header();
	e.emit_param("INT", "%d", param);
	e.emit_output_expected_header();
	e.emit_retval("%s", expected);
	e.emit_output_actual_header();
	e.emit_retval("%s", s.Value());
	if(strcmp(s.Value(), expected) != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool char_constructor(void) {
	e.emit_test("Test constructor to make a copy of a null-terminated character"
		" string");
	const char *param = "foo";
	MyString s(param);
	e.emit_input_header();
	e.emit_param("STRING", "%s", param);
	e.emit_output_expected_header();
	e.emit_retval("%s", param);
	e.emit_output_actual_header();
	e.emit_retval("%s", s.Value());
	if(strcmp(s.Value(), param) != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool copy_constructor_value_check(void) {
	e.emit_test("Test value of copy constructor");
	const char *param = "foo";
	MyString one(param);
	MyString two(one);
	e.emit_input_header();
	e.emit_param("MyString", "%s", two.Value());
	e.emit_output_expected_header();
	e.emit_retval("%s", one.Value());
	e.emit_output_actual_header();
	e.emit_retval("%s", two.Value());
	if(strcmp(one.Value(), two.Value()) != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool copy_constructor_pointer_check(void) {
	e.emit_test("Test pointer of copy constructor");
	const char *param = "foo";
	MyString one(param);
	MyString two(one);
	e.emit_input_header();
	e.emit_param("MyString", "%s", two.Value());
	e.emit_output_expected_header();
	e.emit_retval("!= %p", one.Value());
	e.emit_output_actual_header();
	e.emit_retval("%p", two.Value());
	if(one.Value() == two.Value()) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}


static bool length_0(void) {
	e.emit_test("Length() of MyString of length 0");
	MyString zero;
	e.emit_output_expected_header();
	e.emit_retval("%d", 0);
	e.emit_output_actual_header();
	e.emit_retval("%d", zero.Length());
	if(zero.Length() != 0) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 94
static bool length_1(void) {
	e.emit_test("Length() of MyString of length 1");
	const char *param = "a";
	MyString one(param);
	e.emit_output_expected_header();
	e.emit_retval("%d", strlen(param));
	e.emit_output_actual_header();
	e.emit_retval("%d", one.Length());
	if(one.Length() != (signed)strlen(param)) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 102
static bool length_2(void) {
	e.emit_test("Length() of MyString of length 2");
	const char *param = "ab";
	MyString two(param);
	e.emit_output_expected_header();
	e.emit_retval("%d", strlen(param));
	e.emit_output_actual_header();
	e.emit_retval("%d", two.Length());
	if(two.Length() != (signed)strlen(param)) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool length_after_change(void) {
	e.emit_test("Length() of MyString after being assigned to an empty "
		"MyString");
	MyString long_string("longer");
	MyString empty;
	long_string = empty;
	e.emit_output_expected_header();
	e.emit_retval("%d", empty.Length());
	e.emit_output_actual_header();
	e.emit_retval("%d", long_string.Length());
	if(long_string.Length() != empty.Length()) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool is_empty_initialized(void) {
	e.emit_test("IsEmpty() of an empty MyString");
	MyString empty;
	e.emit_output_expected_header();
	e.emit_retval("%d", 1);
	e.emit_output_actual_header();
	e.emit_retval("%d", empty.IsEmpty());
	if(!empty.IsEmpty()) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool is_empty_after_change(void) {
	e.emit_test("IsEmpty() of an MyString that is changed to an empty "
		"MyString");
	MyString will_be_empty("notEmpty");
	will_be_empty = "";
	e.emit_output_expected_header();
	e.emit_retval("%d", 1);
	e.emit_output_actual_header();
	e.emit_retval("%d", will_be_empty.IsEmpty());
	if(!will_be_empty.IsEmpty()) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool is_empty_false(void) {
	e.emit_test("IsEmpty() of a non-empty MyString");
	MyString not_empty("a");
	e.emit_output_expected_header();
	e.emit_retval("%d", 0);
	e.emit_output_actual_header();
	e.emit_retval("%d", not_empty.IsEmpty());
	if(not_empty.IsEmpty()) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool capacity_empty(void) {
	e.emit_test("Capacity() of an empty MyString");
	e.emit_comment("This is the way the code works in init()."
		"This will fail if init() changes capacity in the future!");
	MyString empty;
	int cap = 0;
	e.emit_output_expected_header();
	e.emit_retval("%d", cap);
	e.emit_output_actual_header();
	e.emit_retval("%d", empty.Capacity());
	if(empty.Capacity() != cap) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool capacity_greater_than_length(void) {
	e.emit_test("Is the capacity of a non-empty MyString greater or equal to "
		"its length?");
	MyString s("foo");
	e.emit_output_expected_header();
	e.emit_param("Capacity", ">=3");
	e.emit_param("Length", "3");
	e.emit_retval("%d", 1);
	e.emit_output_actual_header();
	e.emit_param("Capicity", "%d", s.Capacity());
	e.emit_param("Length", "%d", s.Length());
	e.emit_retval("%d", s.Capacity() >= s.Length());
	if(s.Capacity() < s.Length()) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool str_dup_empty(void) {
	e.emit_test("Does StrDup() return the empty string when called on an empty"
		" MyString?");
	MyString empty;
	char* dup = empty.StrDup();
	e.emit_output_expected_header();
	e.emit_retval("");
	e.emit_output_actual_header();
	e.emit_retval("%s", dup);
	if(strcmp(dup, "") != MATCH) {
		e.emit_result_failure(__LINE__);
		free(dup);
		return false;
	}
	e.emit_result_success(__LINE__);
	free(dup);
	return true;
}

static bool str_dup_empty_new(void) {
	e.emit_test("Does StrDup() return a newly created empty string when called "
		"on an empty MyString?");
	MyString empty;
	char* dup = empty.StrDup();
	e.emit_output_expected_header();
	e.emit_retval("");
	e.emit_output_actual_header();
	e.emit_retval("%s", dup);
	if(empty.Value() == dup) {
		e.emit_result_failure(__LINE__);
		free(dup);
		return false;
	}
	e.emit_result_success(__LINE__);
	free(dup);
	return true;
}

static bool str_dup_non_empty(void) {
	e.emit_test("Does StrDup() return the equivalent C string when called on a"
		" non-empty MyString?");
	MyString s("foo");
	char* dup = s.StrDup();
	e.emit_output_expected_header();
	e.emit_retval("");
	e.emit_output_actual_header();
	e.emit_retval("%s", dup);
	if(strcmp(dup, "foo") != MATCH) {
		e.emit_result_failure(__LINE__);
		free(dup);
		return false;
	}
	e.emit_result_success(__LINE__);
	free(dup);
	return true;
}

static bool str_dup_non_empty_new(void) {
	e.emit_test("Does StrDup() return a newly created equivalent C string when "
		"called on a non-empty MyString?");
	MyString s("foo");
	char* dup = s.StrDup();
	e.emit_output_expected_header();
	e.emit_retval("");
	e.emit_output_actual_header();
	e.emit_retval("%s", dup);
	if(s.Value() == dup) {
		e.emit_result_failure(__LINE__);
		free(dup);
		return false;
	}
	e.emit_result_success(__LINE__);
	free(dup);
	return true;
}

static bool value_of_empty(void) {
	e.emit_test("Does Value() return the empty string for an empty MyString?");
	MyString empty;
	const char* value = empty.Value();
	e.emit_output_expected_header();
	e.emit_retval("");
	e.emit_output_actual_header();
	e.emit_retval("%s", value);
	if(strcmp(value, "") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool value_of_non_empty(void) {
	e.emit_test("Does Value() return the correct string for a non-empty "
		"MyString?");
	const char* param = "foo";
	MyString s(param);
	const char* value = s.Value();
	e.emit_output_expected_header();
	e.emit_retval("%s", param);
	e.emit_output_actual_header();
	e.emit_retval("%s", value);
	if(strcmp(value, param) != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 369
static bool square_brackets_operator_invalid_negative(void) {
	e.emit_test("Does the [] operator return '\\0' if passed a negative pos?");
	MyString s("foo");
	char invalid = s[-1];
	e.emit_input_header();
	e.emit_param("pos", "%d", -1);
	e.emit_output_expected_header();
	e.emit_retval("%c", '\0');
	e.emit_output_actual_header();
	e.emit_retval("%c", invalid);
	if(invalid != '\0') {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 366
static bool square_brackets_operator_invalid_empty(void) {
	e.emit_test("Does the [] operator return '\\0' if accessing the first char "
		"of an empty MyString?");
	MyString empty;
	char invalid = empty[0];
	e.emit_input_header();
	e.emit_param("pos", "%d", 0);
	e.emit_output_expected_header();
	e.emit_retval("%c", '\0');
	e.emit_output_actual_header();
	e.emit_retval("%c", invalid);
	if(invalid != '\0') {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool square_brackets_operator_invalid_past(void) {
	e.emit_test("Does the [] operator return '\\0' if accessing one char past "
		"the end of the MyString?");
	MyString s("foo");
	char invalid = s[3];
	e.emit_input_header();
	e.emit_param("pos", "%d", 3);
	e.emit_output_expected_header();
	e.emit_retval("%c", '\0');
	e.emit_output_actual_header();
	e.emit_retval("%c", invalid);
	if(invalid != '\0') {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool square_brackets_operator_first_char(void) {
	e.emit_test("[] operator on the first char of a MySting");
	MyString s("foo");
	char valid = s[0];
	e.emit_input_header();
	e.emit_param("pos", "%d", 0);
	e.emit_output_expected_header();
	e.emit_retval("%c", 'f');
	e.emit_output_actual_header();
	e.emit_retval("%c", valid);
	if(valid != 'f') {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool square_brackets_operator_last_char(void) {
	e.emit_test("[] operator on the last char of a MySting");
	MyString s("foo");
	char valid = s[2];
	e.emit_input_header();
	e.emit_param("pos", "%d", 2);
	e.emit_output_expected_header();
	e.emit_retval("%c", 'o');
	e.emit_output_actual_header();
	e.emit_retval("%c", valid);
	if(valid != 'o') {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 391
static bool set_char_empty(void) {
	e.emit_test("Does an empty MyString stay the same after calling setChar()");
	MyString empty;
	empty.setChar(0, 'a');
	e.emit_input_header();
	e.emit_param("pos", "%d", 0);
	e.emit_param("value", "%c", 'a');
	e.emit_output_expected_header();
	e.emit_param("Value", "%s", "");
	e.emit_param("Length", "%d", 0);
	e.emit_output_actual_header();
	e.emit_param("Value", "%s", empty.Value());
	e.emit_param("Length", "%d", empty.Length());
	if(strcmp(empty.Value(), "") != MATCH || empty.Length() != 0) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 423
static bool set_char_truncate(void) {
	e.emit_test("Does calling setChar() with '\\0' truncate the original "
		"MyString");
	MyString s("Eddy Merckx");
	s.setChar(5, '\0');
	e.emit_input_header();
	e.emit_param("MyString", "%s", s.Value());
	e.emit_param("pos", "%d", 5);
	e.emit_param("value", "\\0");
	e.emit_output_expected_header();
	e.emit_param("Value", "Eddy ");
	e.emit_param("Length", "%d", 5);
	e.emit_output_actual_header();
	e.emit_param("Value", "%s", s.Value());
	e.emit_param("Length", "%d", s.Length());
	if(strcmp(s.Value(), "Eddy ") != MATCH || s.Length() != 5) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool set_char_first(void) {
	e.emit_test("setChar() on the first char of a MyString");
	MyString s("foo");
	s.setChar(0, 't');
	e.emit_input_header();
	e.emit_param("MyString", "%s", s.Value());
	e.emit_param("pos", "%d", 0);
	e.emit_param("value", "%c", 't');
	e.emit_output_expected_header();
	e.emit_param("Value", "too");
	e.emit_param("Length", "%d", 3);
	e.emit_output_actual_header();
	e.emit_param("Value", "%s", s.Value());
	e.emit_param("Length", "%d", s.Length());
	if(strcmp(s.Value(), "too") != MATCH || s.Length() != 3) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool set_char_last(void) {
	e.emit_test("setChar() on the last char of a MyString");
	MyString s("foo");
	s.setChar(2, 'r');
	e.emit_input_header();
	e.emit_param("MyString", "%s", s.Value());
	e.emit_param("pos", "%d", 2);
	e.emit_param("value", "%c", 'r');
	e.emit_output_expected_header();
	e.emit_param("Value", "for");
	e.emit_param("Length", "%d", 3);
	e.emit_output_actual_header();
	e.emit_param("Value", "%s", s.Value());
	e.emit_param("Length", "%d", s.Length());
	if(strcmp(s.Value(), "for") != MATCH || s.Length() != 3) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 578
static bool random_string_one_char(void) {
	e.emit_test("Does randomlyGenerate() work when passed only one char?");
	MyString rnd;
	rnd.randomlyGenerate("1", 5);
	e.emit_input_header();
	e.emit_param("set", "%s", "1");
	e.emit_param("len", "%d", 5);
	e.emit_output_expected_header();
	e.emit_retval("%s", "11111");
	e.emit_output_actual_header();
	e.emit_retval("%s", rnd.Value());
	if(strcmp(rnd.Value(), "11111") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 586
static bool random_string_many_chars(void) {
	e.emit_test("Does randomlyGenerate() work when passed a 10 char set?");
	MyString rnd;
	rnd.randomlyGenerate("0123456789", 64);
	e.emit_input_header();
	e.emit_param("set", "%s", "0123456789");
	e.emit_param("len", "%d", 64);
	e.emit_output_expected_header();
	e.emit_param("Length", "%d", 64);
	e.emit_output_actual_header();
	e.emit_param("Length", "%d", rnd.Length());
	if(rnd.Length() != 64) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool random_string_existing(void) {
	e.emit_test("Does randomlyGenerate() work on a non empty MyString?");
	MyString rnd("foo");
	rnd.randomlyGenerate("0123456789", 64);
	e.emit_input_header();
	e.emit_param("set", "%s", "0123456789");
	e.emit_param("len", "%d", 64);
	e.emit_output_expected_header();
	e.emit_param("Length", "%d", 64);
	e.emit_output_actual_header();
	e.emit_param("Length", "%d", rnd.Length());
	if(rnd.Length() != 64) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool random_string_empty_set(void) {
	e.emit_test("Does randomlyGenerate() work when passed an empty set when "
		"called on an empty MyString?");
	MyString rnd;
	rnd.randomlyGenerate(NULL, 64);
	e.emit_input_header();
	e.emit_param("set", "%s", "");
	e.emit_param("len", "%d", 64);
	e.emit_output_expected_header();
	e.emit_retval("%s", "");
	e.emit_output_actual_header();
	e.emit_retval("%s", rnd.Value());
	if(strcmp(rnd.Value(), "") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool random_string_empty_set_existing(void) {
	e.emit_test("Does randomlyGenerate() work when passed an empty set when "
		"called on a non empty MyString?");
	MyString rnd("foo");
	rnd.randomlyGenerate(NULL, 64);
	e.emit_input_header();
	e.emit_param("set", "%s", "");
	e.emit_param("len", "%d", 64);
	e.emit_output_expected_header();
	e.emit_retval("%s", "");
	e.emit_output_actual_header();
	e.emit_retval("%s", rnd.Value());
	if(strcmp(rnd.Value(), "") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool random_string_length_zero(void) {
	e.emit_test("Does randomlyGenerate() work when passed a length of zero?");
	MyString rnd;
	rnd.randomlyGenerate("0123", 0);
	e.emit_input_header();
	e.emit_param("set", "%s", "0123");
	e.emit_param("len", "%d", 0);
	e.emit_output_expected_header();
	e.emit_retval("%s", "");
	e.emit_output_actual_header();
	e.emit_retval("%s", rnd.Value());
	if(strcmp(rnd.Value(), "") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool random_string_negative_length(void) {
	e.emit_test("Does randomlyGenerate() work when passed a negative length?");
	MyString rnd;
	rnd.randomlyGenerate("0123", -1);
	e.emit_input_header();
	e.emit_param("set", "%s", "0123");
	e.emit_param("len", "%d", -1);
	e.emit_output_expected_header();
	e.emit_retval("%s", "");
	e.emit_output_actual_header();
	e.emit_retval("%s", rnd.Value());
	if(strcmp(rnd.Value(), "") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool random_string_hex(void) {
	e.emit_test("Does randomlyGenerateHex() modify the string to the given "
		"length?");
	MyString rnd;
	rnd.randomlyGenerateHex(10);
	e.emit_input_header();
	e.emit_param("len", "%d", 10);
	e.emit_output_expected_header();
	e.emit_param("Length", "%d", 10);
	e.emit_output_actual_header();
	e.emit_param("Length", "%d", rnd.Length());
	if(rnd.Length() != 10) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool equals_operator_my_string_empty_non_empty(void) {
	e.emit_test("Does the = operator work correctly when an empty MyString gets"
		" assigned a non empty MyString?");
	MyString a;
	MyString b("bar");
	a = b;
	e.emit_input_header();
	e.emit_param("MyString", "%s", b.Value());
	e.emit_output_expected_header();
	e.emit_retval("%s", b.Value());
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "bar") != MATCH || a.Value() == b.Value()) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 412
static bool equals_operator_my_string_non_empty_non_empty(void) {
	e.emit_test("Does the = operator work correctly when a non empty MyString "
		"gets assigned another non empty MyString?");
	MyString a("Bernard Hinault");
	MyString b("Laurent Jalabert");
	a = b;
	e.emit_input_header();
	e.emit_param("MyString", "%s", b.Value());
	e.emit_output_expected_header();
	e.emit_retval("%s", b.Value());
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "Laurent Jalabert") != MATCH || a.Value() == b.Value())
	{
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool equals_operator_my_string_non_empty_empty(void) {
	e.emit_test("Does the = operator work correctly when a non empty MyString "
		"gets assigned an empty MyString?");
	MyString a("foo");
	MyString b;
	a = b;
	e.emit_input_header();
	e.emit_param("MyString", "%s", b.Value());
	e.emit_output_expected_header();
	e.emit_retval("%s", b.Value());
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "") != MATCH || a.Value() == b.Value()) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 114
static bool equals_operator_string_empty_non_empty(void) {
	e.emit_test("Does the = operator work correctly when an empty MyString gets"
		" assigned a non empty string?");
	MyString a;
	char* b = "bar";
	a = b;
	e.emit_input_header();
	e.emit_param("STRING", "%s", b);
	e.emit_output_expected_header();
	e.emit_retval("%s", "bar");
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "bar") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool equals_operator_string_non_empty_non_empty(void) {
	e.emit_test("Does the = operator work correctly when a non empty MyString "
		"gets assigned a non empty string?");
	MyString a("foo");
	char* b = "bar";
	a = b;
	e.emit_input_header();
	e.emit_param("STRING", "%s", b);
	e.emit_output_expected_header();
	e.emit_retval("%s", b);
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "bar") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool equals_operator_string_non_empty_empty(void) {
	e.emit_test("Does the = operator work correctly when a non empty MyString "
		"gets assigned an empty string?");
	MyString a("foo");
	char* b = "";
	a = b;
	e.emit_input_header();
	e.emit_param("STRING", "%s", b);
	e.emit_output_expected_header();
	e.emit_retval("%s", b);
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 112
static bool equals_operator_string_null(void) {
	e.emit_test("Does the = operator work when a MyString gets assigned to a "
		"Null string?");
	MyString a;
	a = NULL;
	e.emit_input_header();
	e.emit_param("STRING", "%s", "NULL");
	e.emit_output_expected_header();
	e.emit_retval("%s", "");
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 292
static bool reserve_invalid_negative(void) {
	e.emit_test("Does reserve() return false when passed a negative size?");
	MyString a;
	bool retval = a.reserve(-3);
	e.emit_input_header();
	e.emit_param("INT", "%d", -3);
	e.emit_output_expected_header();
	e.emit_retval("%d", 0);
	e.emit_output_actual_header();
	e.emit_retval("%d", retval);
	if(retval) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 302
static bool reserve_shortening_1(void) {
	e.emit_test("Does reserve() return true when passed a size smaller than the"
		" MyString's length?");
	MyString a("12345");
	bool retval = a.reserve(1);
	e.emit_input_header();
	e.emit_param("INT", "%d", 1);
	e.emit_output_expected_header();
	e.emit_retval("%d", 1);
	e.emit_output_actual_header();
	e.emit_retval("%d", retval);
	if(!retval) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 325
static bool reserve_shortening_0(void) {
	e.emit_test("Does reserve() return true when passed a size of 0?");
	MyString a("12345");
	bool retval = a.reserve(0);
	e.emit_input_header();
	e.emit_param("INT", "%d", 0);
	e.emit_output_expected_header();
	e.emit_retval("%d", 1);
	e.emit_output_actual_header();
	e.emit_retval("%d", retval);
	if(!retval) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 434
static bool reserve_shortening_smaller(void) {
	e.emit_test("Does reserve() correctly shorten a MyString when passed a size"
		" smaller than the length of the MyString?");
	MyString a("Miguel Indurain");
	a.reserve(6);
	e.emit_input_header();
	e.emit_param("INT", "%d", 0);
	e.emit_output_expected_header();
	e.emit_param("MyString", "%s", "Miguel");
	e.emit_param("Length", "%d", 6);
	e.emit_output_actual_header();
	e.emit_param("MyString", "%s", a.Value());
	e.emit_param("Length", "%d", a.Length());
	if(strcmp(a.Value(), "Miguel") != MATCH || a.Length() != 6) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool reserve_no_changes(void) {
	e.emit_test("Does reserve() preserve the original MyString when passed a "
		"size greater than the MyString's length?");
	MyString a("foo");
	bool retval = a.reserve(100);
	e.emit_input_header();
	e.emit_param("INT", "%d", 100);
	e.emit_output_expected_header();
	e.emit_param("MyString", "%s", "foo");
	e.emit_retval("%d", 1);
	e.emit_output_actual_header();
	e.emit_param("MyString", "%s", a.Value());
	e.emit_retval("%d", retval);
	if(!retval || strcmp(a.Value(), "foo") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool reserve_empty_my_string(void) {
	e.emit_test("Does reserve() return true when used on an empty MyString?");
	MyString a;
	bool retval = a.reserve(100);
	e.emit_input_header();
	e.emit_param("INT", "%d", 100);
	e.emit_output_expected_header();
	e.emit_retval("%d", 1);
	e.emit_output_actual_header();
	e.emit_retval("%d", retval);
	if(!retval) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool reserve_larger_capacity(void) {
	e.emit_test("Does reserve() increase the capacity of a MyString when passed"
		" a larger size");
	MyString a;
	int size = 100;
	a.reserve(size);
	e.emit_input_header();
	e.emit_param("INT", "%d", size);
	e.emit_output_expected_header();
	e.emit_param("Capacity", "%d", size);
	e.emit_output_actual_header();
	e.emit_param("Capacity", "%d", a.Capacity());
	if(a.Capacity() != size) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool reserve_smaller_capacity(void) {
	e.emit_test("Does reverse() decrease the capacity of a MyString when passed"
		" a smaller size?");
	MyString a("foobar");
	int size = 1;
	a.reserve(size);
	e.emit_input_header();
	e.emit_param("INT", "%d", size);
	e.emit_output_expected_header();
	e.emit_param("Capacity", "%d", size);
	e.emit_output_actual_header();
	e.emit_param("Capacity", "%d", a.Capacity());
	if(a.Capacity() != size) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 348
static bool reserve_at_least_negative(void) {
	e.emit_test("Does reverse_at_least() return true when passed a negative "
		"size?");
	MyString a("12345");
	bool retval = a.reserve_at_least(-1);
	e.emit_input_header();
	e.emit_param("INT", "%d", -1);
	e.emit_output_expected_header();
	e.emit_retval("%d", 1);
	e.emit_output_actual_header();
	e.emit_retval("%d", retval);
	if(!retval) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 357
static bool reserve_at_least_zero(void) {
	e.emit_test("Does reverse_at_least() return true when passed a size of 0?");
	MyString a("12345");
	bool retval = a.reserve_at_least(0);
	e.emit_input_header();
	e.emit_param("INT", "%d", 0);
	e.emit_output_expected_header();
	e.emit_retval("%d", 1);
	e.emit_output_actual_header();
	e.emit_retval("%d", retval);
	if(!retval) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 447
static bool reserve_at_least_no_change(void) {
	e.emit_test("Does reverse_at_least() return preserve the original MyString "
		"when passed a size greater than the MyString's length?");
	MyString a("Jacques Anquetil");
	a.reserve_at_least(100);
	e.emit_input_header();
	e.emit_param("INT", "%d", 0);
	e.emit_output_expected_header();
	e.emit_param("MyString", "%s", "Jacques Anquetil");
	e.emit_output_actual_header();
	e.emit_param("MyString", "%s", a.Value());
	if(strcmp(a.Value(), "Jacques Anquetil") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool reserve_at_least_capacity(void) {
	e.emit_test("Does reverse_at_least() change the capacity when pased a size "
		"greater than the MyString's length?");
	MyString a("Jacques Anquetil");
	a.reserve_at_least(100);
	e.emit_input_header();
	e.emit_param("INT", "%d", 0);
	e.emit_output_expected_header();
	e.emit_param("Capacity", "%s", ">= 100");
	e.emit_output_actual_header();
	e.emit_param("MyString", "%d", a.Capacity());
	if(a.Capacity() < 100) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 125
static bool concatenation_my_string_non_empty_empty(void) {
	e.emit_test("Test concatenating a non-empty MyString with an empty "
		"MyString.");
	MyString a("foo");
	MyString b;
	a += b;
	e.emit_input_header();
	e.emit_param("MyString", "%s", b.Value());
	e.emit_output_expected_header();
	e.emit_retval("%s", "foo");
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "foo") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 400
static bool concatenation_my_string_empty_empty(void) {
	e.emit_test("Test concatenating a empty MyString with another empty "
		"MyString.");
	MyString a;
	MyString b;
	a += b;
	e.emit_input_header();
	e.emit_param("MyString", "%s", b.Value());
	e.emit_output_expected_header();
	e.emit_retval("%s", "");
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool concatenation_my_string_empty_non_empty(void) {
	e.emit_test("Test concatenating a empty MyString with a non-empty "
		"MyString.");
	MyString a;
	MyString b("foo");
	a += b;
	e.emit_input_header();
	e.emit_param("MyString", "%s", b.Value());
	e.emit_output_expected_header();
	e.emit_retval("%s", "foo");
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "foo") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 144
static bool concatenation_my_string_non_empty_non_empty(void) {
	e.emit_test("Test concatenating a non-empty MyString with another non-empty"
		" MyString.");
	MyString a("foo");
	MyString b("bar");
	a += b;
	e.emit_input_header();
	e.emit_param("MyString", "%s", b.Value());
	e.emit_output_expected_header();
	e.emit_retval("%s", "foobar");
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "foobar") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool concatenation_my_string_multiple(void) {
	e.emit_test("Test concatenating an empty MyString with two other non-empty "
		"MyStrings.");
	MyString a("Lance");
	MyString b(" Armstrong");
	MyString c = a + b;
	e.emit_input_header();
	e.emit_param("MyString", "%s", a.Value());
	e.emit_param("MyString", "%s", b.Value());
	e.emit_output_expected_header();
	e.emit_retval("%s", "Lance Armstrong");
	e.emit_output_actual_header();
	e.emit_retval("%s", c.Value());
	if(strcmp(c.Value(), "Lance Armstrong") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool concatenation_my_string_itself(void) {
	e.emit_test("Test concatenating a non-empty MyString with itself.");
	e.emit_comment("See ticket #1290");
	MyString a("foo");
	a += a;
	e.emit_input_header();
	e.emit_param("MyString", "%s", "foo");
	e.emit_output_expected_header();
	e.emit_retval("%s", "foofoo");
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "foofoo") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool concatenation_my_string_itself_empty(void) {
	e.emit_test("Test concatenating an empty MyString with itself.");
	MyString a;
	a += a;
	e.emit_input_header();
	e.emit_param("MyString", "%s", "");
	e.emit_output_expected_header();
	e.emit_retval("%s", "");
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool concatenation_string_non_empty_null(void) {
	e.emit_test("Test concatenating a non-empty MyString with a NULL string.");
	MyString a("foo");
	a += (char *)NULL;
	e.emit_input_header();
	e.emit_param("STRING", "%s", "NULL");
	e.emit_output_expected_header();
	e.emit_retval("%s", "foo");
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "foo") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 123
static bool concatenation_string_empty_empty(void) {
	e.emit_test("Test concatenating an empty MyString with a NULL string.");
	MyString a;
	a += (char *)NULL;
	e.emit_input_header();
	e.emit_param("STRING", "%s", "NULL");
	e.emit_output_expected_header();
	e.emit_retval("%s", "");
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool concatenation_string_empty_non_empty(void) {
	e.emit_test("Test concatenating an empty MyString with a non-empty "
		"string.");
	MyString a;
	char* b = "foo";
	a += b;
	e.emit_input_header();
	e.emit_param("STRING", "%s", b);
	e.emit_output_expected_header();
	e.emit_retval("%s", "foo");
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "foo") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 122
static bool concatenation_string_non_empty_non_empty(void) {
	e.emit_test("Test concatenating a non-empty MyString with a non-empty "
		"string.");
	MyString a("blah");
	char* b = "baz";
	a += b;
	e.emit_input_header();
	e.emit_param("STRING", "%s", b);
	e.emit_output_expected_header();
	e.emit_retval("%s", "blahbaz");
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "blahbaz") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 135
static bool concatenation_char_empty(void) {
	e.emit_test("Test concatenating an empty MyString with a character.");
	MyString a;
	a += '!';
	e.emit_input_header();
	e.emit_param("CHAR", "%c", '!');
	e.emit_output_expected_header();
	e.emit_retval("%c", '!');
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "!") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool concatenation_char_non_empty(void) {
	e.emit_test("Test concatenating a non-empty MyString with a character.");
	MyString a("pow");
	a += '!';
	e.emit_input_header();
	e.emit_param("CHAR", "%c", '!');
	e.emit_output_expected_header();
	e.emit_retval("%s", "pow!");
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "pow!") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool concatenation_char_null(void) {
	e.emit_test("Test concatenating a non-empty MyString with the NULL "
		"character.");
	e.emit_problem("Concatenation works, but the length is still incremented!");
	MyString a("pow");
	a += '\0';
	e.emit_input_header();
	e.emit_param("CHAR", "%c", '!');
	e.emit_output_expected_header();
	e.emit_retval("%s", "pow");
	e.emit_param("Length", "%d", 3);
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	e.emit_param("Length", "%d", a.Length());
	if(strcmp(a.Value(), "pow") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool concatenation_int_empty(void) {
	e.emit_test("Test concatenating an empty MyString with an integer.");
	MyString a;
	a += 123;
	e.emit_input_header();
	e.emit_param("INT", "%d", 123);
	e.emit_output_expected_header();
	e.emit_retval("%s", "123");
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "123") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 170
static bool concatenation_int_non_empty(void) {
	e.emit_test("Test concatenating a non-empty MyString with an integer.");
	MyString a("Lance Armstrong");
	a += 123;
	e.emit_input_header();
	e.emit_param("INT", "%d", 123);
	e.emit_output_expected_header();
	e.emit_retval("%s", "Lance Armstrong123");
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "Lance Armstrong123") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool concatenation_int_large(void) {
	e.emit_test("Test concatenating a MyString with INT_MAX.");
	char buf[1024];
	sprintf(buf, "%s%d", "foo", INT_MAX);
	MyString a("foo");
	a += INT_MAX;
	e.emit_input_header();
	e.emit_param("INT", "%d", INT_MAX);
	e.emit_output_expected_header();
	e.emit_retval("%s", buf);
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), buf) != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool concatenation_int_small(void) {
	e.emit_test("Test concatenating a MyString with INT_MIN.");	
	char buf[1024];
	sprintf(buf, "%s%d", "foo", INT_MIN);
	MyString a("foo");
	a += INT_MIN;
	e.emit_input_header();
	e.emit_param("INT", "%d", INT_MIN);
	e.emit_output_expected_header();
	e.emit_retval("%s", buf);
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), buf) != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool concatenation_uint_empty(void) {
	e.emit_test("Test concatenating an empty MyString with an unsigned "
		"integer.");
	MyString a;
	unsigned int b = 123;
	a += b;
	e.emit_input_header();
	e.emit_param("UINT", "%u", b);
	e.emit_output_expected_header();
	e.emit_retval("%s", "123");
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "123") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool concatenation_uint_non_empty(void) {
	e.emit_test("Test concatenating a non-empty MyString with an unsigned "
		"integer.");
	MyString a("foo");
	unsigned int b = 123;
	a += b;
	e.emit_input_header();
	e.emit_param("UINT", "%u", b);
	e.emit_output_expected_header();
	e.emit_retval("%s", "foo123");
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "foo123") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool concatenation_uint_large(void) {
	e.emit_test("Test concatenating a MyString with UINT_MAX");	
	char buf[1024];
	sprintf(buf, "%s%u", "foo", UINT_MAX);
	MyString a("foo");
	a += UINT_MAX;
	e.emit_input_header();
	e.emit_param("UINT", "%u", UINT_MAX);
	e.emit_output_expected_header();
	e.emit_retval("%s", buf);
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), buf) != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool concatenation_long_empty(void) {
	e.emit_test("Test concatenating an empty MyString with a long.");
	MyString a;
	long b = 123;
	a += b;
	e.emit_input_header();
	e.emit_param("LONG", "%ld", b);
	e.emit_output_expected_header();
	e.emit_retval("%s", "123");
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "123") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool concatenation_long_non_empty(void) {
	e.emit_test("Test concatenating a non-empty MyString with a long.");
	MyString a("foo");
	long b = 123;
	a += b;
	e.emit_input_header();
	e.emit_param("LONG", "%ld", b);
	e.emit_output_expected_header();
	e.emit_retval("%s", "foo123");
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "foo123") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool concatenation_long_large(void) {
	e.emit_test("Test concatenating a MyString with LONG_MAX.");
	char buf[1024];
	sprintf(buf, "%s%ld", "foo", LONG_MAX);
	MyString a("foo");
	a += LONG_MAX;
	e.emit_input_header();
	e.emit_param("LONG", "%ld", LONG_MAX);
	e.emit_output_expected_header();
	e.emit_retval("%s", buf);
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), buf) != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool concatenation_long_small(void) {
	e.emit_test("Test concatenating a MyString with LONG_MIN.");
	char buf[1024];
	sprintf(buf, "%s%ld", "foo", LONG_MIN);
	MyString a("foo");
	a += LONG_MIN;
	e.emit_input_header();
	e.emit_param("LONG", "%ld", LONG_MIN);
	e.emit_output_expected_header();
	e.emit_retval("%s", buf);
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), buf) != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 180
static bool concatenation_double_empty(void) {
	e.emit_test("Test concatenating an empty MyString with a double.");
	MyString a;
	a += 12.3;
	e.emit_input_header();
	e.emit_param("MyString", "%f", 12.3);
	e.emit_output_expected_header();
	e.emit_retval("%s", "12.3");
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strncmp(a.Value(), "12.3", 4) != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool concatenation_double_non_empty(void) {
	e.emit_test("Test concatenating a non-empty MyString with a double.");
	MyString a("foo");
	a += 12.3;
	e.emit_input_header();
	e.emit_param("MyString", "%f", 12.3);
	e.emit_output_expected_header();
	e.emit_retval("%s", "foo12.3");
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strncmp(a.Value(), "foo12.3", 7) != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 379
static bool substr_empty(void) {
	e.emit_test("Test Substr() on an empty MyString.");
	MyString a;
	MyString b = a.Substr(0, 5);
	e.emit_input_header();
	e.emit_param("Pos1", "%d", 0);
	e.emit_param("Pos2", "%d", 1);
	e.emit_output_expected_header();
	e.emit_retval("%s", "");
	e.emit_output_actual_header();
	e.emit_retval("%s", b.Value());
	if(strcmp(b.Value(), "") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 191
static bool substr_beginning(void) {
	e.emit_test("Test Substr() on the beginning of a MyString.");
	MyString a("blahbaz!");
	MyString b = a.Substr(0, 3);
	e.emit_input_header();
	e.emit_param("Pos1", "%d", 0);
	e.emit_param("Pos2", "%d", 3);
	e.emit_output_expected_header();
	e.emit_retval("%s", "blah");
	e.emit_output_actual_header();
	e.emit_retval("%s", b.Value());
	if(strcmp(b.Value(), "blah") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 200
static bool substr_end(void) {
	e.emit_test("Test Substr() on the end of a MyString.");
	MyString a("blahbaz!");
	MyString b = a.Substr(4, 7);
	e.emit_input_header();
	e.emit_param("Pos1", "%d", 4);
	e.emit_param("Pos2", "%d", 7);
	e.emit_output_expected_header();
	e.emit_retval("%s", "baz!");
	e.emit_output_actual_header();
	e.emit_retval("%s", b.Value());
	if(strcmp(b.Value(), "baz!") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 218
static bool substr_outside_beginning(void) {
	e.emit_test("Test Substr() when passed a position before the beginning of "
		"the MyString.");
	MyString a("blahbaz!");
	MyString b = a.Substr(-2, 5);
	e.emit_input_header();
	e.emit_param("Pos1", "%d", -2);
	e.emit_param("Pos2", "%d", 5);
	e.emit_output_expected_header();
	e.emit_retval("%s", "blahba");
	e.emit_output_actual_header();
	e.emit_retval("%s", b.Value());
	if(strcmp(b.Value(), "blahba") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 209
static bool substr_outside_end(void) {
	e.emit_test("Test Substr() when passed a position after the end of the "
		"MyString.");
	MyString a("blahbaz!");
	MyString b = a.Substr(5, 10);
	e.emit_input_header();
	e.emit_param("Pos1", "%d", 5);
	e.emit_param("Pos2", "%d", 10);
	e.emit_output_expected_header();
	e.emit_retval("%s", "az!");
	e.emit_output_actual_header();
	e.emit_retval("%s", b.Value());
	if(strcmp(b.Value(), "az!") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 230
static bool escape_chars_length_1(void) {
	e.emit_test("Test EscapeChars() on a MyString of length 1.");
	MyString a("z");
	MyString b = a.EscapeChars("z", '\\');
	e.emit_input_header();
	e.emit_param("STRING", "%s", "z");
	e.emit_param("CHAR", "%c", '\\');
	e.emit_output_expected_header();
	e.emit_retval("%s", "\\z");
	e.emit_output_actual_header();
	e.emit_retval("%s", b.Value());
	if(strcmp(b.Value(), "\\z") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool escape_chars_empty(void) {
	e.emit_test("Test EscapeChars() on an empty MyString.");
	MyString a;
	MyString b = a.EscapeChars("z", '\\');
	e.emit_input_header();
	e.emit_param("STRING", "%s", "z");
	e.emit_param("CHAR", "%c", '\\');
	e.emit_output_expected_header();
	e.emit_retval("%s", "");
	e.emit_output_actual_header();
	e.emit_retval("%s", b.Value());
	if(strcmp(b.Value(), "") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool escape_chars_multiple(void) {
	e.emit_test("Test EscapeChars() when passed many escape chars.");
	MyString a("foobar");
	MyString b = a.EscapeChars("oa", '*');
	e.emit_input_header();
	e.emit_param("STRING", "%s", "oa");
	e.emit_param("CHAR", "%c", '*');
	e.emit_output_expected_header();
	e.emit_retval("%s", "f*o*ob*ar");
	e.emit_output_actual_header();
	e.emit_retval("%s", b.Value());
	if(strcmp(b.Value(), "f*o*ob*ar") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool escape_chars_same(void) {
	e.emit_test("Test EscapeChars() when passed the same string as escape "
		"chars.");
	MyString a("foobar");
	MyString b = a.EscapeChars("foobar", '*');
	e.emit_input_header();
	e.emit_param("STRING", "%s", "foobar");
	e.emit_param("CHAR", "%c", '*');
	e.emit_output_expected_header();
	e.emit_retval("%s", "*f*o*o*b*a*r");
	e.emit_output_actual_header();
	e.emit_retval("%s", b.Value());
	if(strcmp(b.Value(), "*f*o*o*b*a*r") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool find_char_first(void) {
	e.emit_test("Does FindChar() find the char when its at the beginning of the"
		" MyString?");
	MyString a("foo");
	int pos = a.FindChar('f');
	e.emit_input_header();
	e.emit_param("Char", "%c", 'f');
	e.emit_output_expected_header();
	e.emit_retval("%d", 0);
	e.emit_output_actual_header();
	e.emit_retval("%d", pos);
	if(pos != 0) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool find_char_last(void) {
	e.emit_test("Does FindChar() find the char when it first appears at the end"
		" of the MyString?");
	MyString a("bar");
	int pos = a.FindChar('r');
	e.emit_input_header();
	e.emit_param("Char", "%c", 'r');
	e.emit_output_expected_header();
	e.emit_retval("%d", 2);
	e.emit_output_actual_header();
	e.emit_retval("%d", pos);
	if(pos != 2) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool find_char_not_found(void) {
	e.emit_test("Does FindChar() return -1 when the char isn't found in the "
		"MyString?");
	MyString a("bar");
	int pos = a.FindChar('t');
	e.emit_input_header();
	e.emit_param("Char", "%c", 't');
	e.emit_output_expected_header();
	e.emit_retval("%d", -1);
	e.emit_output_actual_header();
	e.emit_retval("%d", pos);
	if(pos != -1) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool find_char_invalid_greater(void) {
	e.emit_test("Does FindChar() return -1 when the first position is greater "
		"than the length of the MyString?");
	MyString a("bar");
	int pos = a.FindChar('b', 10);
	e.emit_input_header();
	e.emit_param("Char", "%c", 'b');
	e.emit_param("FirstPos", "%d", 10);
	e.emit_output_expected_header();
	e.emit_retval("%d", -1);
	e.emit_output_actual_header();
	e.emit_retval("%d", pos);
	if(pos != -1) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool find_char_invalid_less(void) {
	e.emit_test("Does FindChar() return -1 when the first position is less than"
		" 0?");
	MyString a("bar");
	int pos = a.FindChar('b',-10);
	e.emit_input_header();
	e.emit_param("Char", "%c", 'b');
	e.emit_param("FirstPos", "%d", -10);
	e.emit_output_expected_header();
	e.emit_retval("%d", -1);
	e.emit_output_actual_header();
	e.emit_retval("%d", pos);
	if(pos != -1) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool hash_empty(void) {
	e.emit_test("Test Hash() on an empty MyString.");
	e.emit_comment("This test compares the hash function to 0 even though 0 is "
		" a possible hash function");
	MyString a;
	unsigned int hash = a.Hash();
	e.emit_output_expected_header();
	e.emit_retval("%s", "!= 0");
	e.emit_output_actual_header();
	e.emit_retval("%d", hash);
	if(hash != 0) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool hash_non_empty(void) {
	e.emit_test("Test Hash() on a non-empty MyString.");
	e.emit_comment("This test compares the hash function to 0 even though 0 is "
		" a possible hash function");
	MyString a("foobar");
	unsigned int hash = a.Hash();
	e.emit_output_expected_header();
	e.emit_retval("%s", "!= 0");
	e.emit_output_actual_header();
	e.emit_retval("%d", hash);
	if(hash == 0) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 598
static bool find_empty(void) {
	e.emit_test("Does find() return 0 when looking for the empty string within"
		" an empty MyString?");
	MyString a;
	int loc = a.find("");
	e.emit_input_header();
	e.emit_param("STRING", "%s", "");
	e.emit_output_expected_header();
	e.emit_retval("%d", 0);
	e.emit_output_actual_header();
	e.emit_retval("%d", loc);
	if(loc != 0) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 607
static bool find_non_empty(void) {
	e.emit_test("Does find() return 0 when looking for the empty string within "
		"an non-empty MyString?");
	MyString a("Alberto Contador");
	int loc = a.find("");
	e.emit_input_header();
	e.emit_param("STRING", "%s", "");
	e.emit_output_expected_header();
	e.emit_retval("%d", 0);
	e.emit_output_actual_header();
	e.emit_retval("%d", loc);
	if(loc != 0) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 614
static bool find_beginning(void) {
	e.emit_test("Does find() return 0 when looking for a substring that is at "
		"the beginning of the MyString?");
	MyString a("Alberto Contador");
	int loc = a.find("Alberto");
	e.emit_input_header();
	e.emit_param("STRING", "%s", "Alberto");
	e.emit_output_expected_header();
	e.emit_retval("%d", 0);
	e.emit_output_actual_header();
	e.emit_retval("%d", loc);
	if(loc != 0) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 621
static bool find_middle(void) {
	e.emit_test("Does find() return the correct position when looking for a "
		"substring that is in the middle of the MyString?");
	MyString a("Alberto Contador");
	int loc = a.find("to Cont");
	e.emit_input_header();
	e.emit_param("STRING", "%s", "to Cont");
	e.emit_output_expected_header();
	e.emit_retval("%d", 5);
	e.emit_output_actual_header();
	e.emit_retval("%d", loc);
	if(loc != 5) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 628
static bool find_substring_not_found(void) {
	e.emit_test("Does find() return -1 when the substring is not in the "
		"MyString?");
	MyString a("Alberto Contador");
	int loc = a.find("Lance");
	e.emit_input_header();
	e.emit_param("STRING", "%s", "Lance");
	e.emit_output_expected_header();
	e.emit_retval("%d", -1);
	e.emit_output_actual_header();
	e.emit_retval("%d", loc);
	if(loc != -1) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool find_invalid_start_less(void) {
	e.emit_test("Does find() return -1 when start position is less than 0?");
	MyString a("Alberto Contador");
	int loc = a.find("Alberto", -1);
	e.emit_input_header();
	e.emit_param("STRING", "%s", "Alberto");
	e.emit_param("Start Pos", "%d", -1);
	e.emit_output_expected_header();
	e.emit_retval("%d", -1);
	e.emit_output_actual_header();
	e.emit_retval("%d", loc);
	if(loc != -1) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool find_invalid_start_greater(void) {
	e.emit_test("Does find() return -1 when start position is greater than the "
		"length of the MyString?");
	MyString a("Alberto Contador");
	int loc = a.find("Alberto", 100);
	e.emit_input_header();
	e.emit_param("STRING", "%s", "Alberto");
	e.emit_param("Start Pos", "%d", 100);
	e.emit_output_expected_header();
	e.emit_retval("%d", -1);
	e.emit_output_actual_header();
	e.emit_retval("%d", loc);
	if(loc != -1) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 240 
static bool replace_string_valid(void) {
	e.emit_test("Test replaceString() when passed a substring that exists in "
		"the MyString.");
	MyString a("12345");
	a.replaceString("234", "ab");
	e.emit_input_header();
	e.emit_param("STRING", "%s", "234");
	e.emit_param("STRING", "%s", "ab");
	e.emit_output_expected_header();
	e.emit_retval("%s", "1ab5");
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "1ab5") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 248 
static bool replace_string_empty(void) {
	e.emit_test("Test replaceString() when passed an empty new string.");
	MyString a("12345");
	a.replaceString("234", "");
	e.emit_input_header();
	e.emit_param("STRING", "%s", "234");
	e.emit_param("STRING", "%s", "");
	e.emit_output_expected_header();
	e.emit_retval("%s", "15");
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "15") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool replace_string_empty_my_string(void) {
	e.emit_test("Test replaceString() on an empty MyString.");
	MyString a;
	a.replaceString("1", "2");
	e.emit_input_header();
	e.emit_param("STRING", "%s", "1");
	e.emit_param("STRING", "%s", "2");
	e.emit_output_expected_header();
	e.emit_retval("%s", "");
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool replace_string_beginning(void) {
	e.emit_test("Test replaceString() at the beginning of the MyString.");
	MyString a("12345");
	a.replaceString("1", "01");
	e.emit_input_header();
	e.emit_param("STRING", "%s", "1");
	e.emit_param("STRING", "%s", "01");
	e.emit_output_expected_header();
	e.emit_retval("%s", "012345");
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "012345") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool replace_string_end(void) {
	e.emit_test("Test replaceString() at the end of the MyString.");
	MyString a("12345");
	a.replaceString("5", "56");
	e.emit_input_header();
	e.emit_param("STRING", "%s", "5");
	e.emit_param("STRING", "%s", "56");
	e.emit_output_expected_header();
	e.emit_retval("%s", "123456");
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "123456") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool replace_string_return_false(void) {
	e.emit_test("Does replaceString() return false when the substring doesn't "
		"exist in the MyString.");
	MyString a("12345");
	bool found = a.replaceString("abc", "");
	e.emit_input_header();
	e.emit_param("STRING", "%s", "abc");
	e.emit_param("STRING", "%s", "");
	e.emit_output_expected_header();
	e.emit_retval("%d", 0);
	e.emit_output_actual_header();
	e.emit_retval("%d", found);
	if(found) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool replace_string_return_true(void) {
	e.emit_test("Does replaceString() return true when the substring exists in "
		"the MyString.");
	MyString a("12345");
	bool found = a.replaceString("34", "43");
	e.emit_input_header();
	e.emit_param("STRING", "%s", "34");
	e.emit_param("STRING", "%s", "43");
	e.emit_output_expected_header();
	e.emit_retval("%d", 1);
	e.emit_output_actual_header();
	e.emit_retval("%d", found);
	if(!found) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 260
static bool sprintf_empty(void) {
	e.emit_test("Test sprintf() on an empty MyString.");
	MyString a;
	a.sprintf("%s %d", "happy", 3);
	e.emit_input_header();
	e.emit_param("STRING", "%s", "happy");
	e.emit_param("INT", "%d", 3);
	e.emit_output_expected_header();
	e.emit_retval("%s", "happy 3");
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "happy 3") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 270
static bool sprintf_non_empty(void) {
	e.emit_test("Test sprintf() on a non-empty MyString.");
	MyString a("replace me!");
	a.sprintf("%s %d", "sad", 5);
	e.emit_input_header();
	e.emit_param("STRING", "%s", "sad");
	e.emit_param("INT", "%d", 5);
	e.emit_output_expected_header();
	e.emit_retval("%s", "sad 5");
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "sad 5") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool sprintf_return_true(void) {
	e.emit_test("Does sprintf() return true on success?");
	MyString a;
	bool res = a.sprintf("%s %d", "sad", 5);
	e.emit_input_header();
	e.emit_param("STRING", "%s", "sad");
	e.emit_param("INT", "%d", 5);
	e.emit_output_expected_header();
	e.emit_retval("%d", 1);
	e.emit_output_actual_header();
	e.emit_retval("%d", res);
	if(!res) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool vsprintf_empty(void) {
	e.emit_test("Test vsprintf() on an empty MyString.");
	MyString* a = new MyString();
	vsprintfHelper(a, "%s %d", "happy", 3);
	e.emit_input_header();
	e.emit_param("STRING", "%s", "happy");
	e.emit_param("INT", "%d", 3);
	e.emit_output_expected_header();
	e.emit_retval("%s", "happy 3");
	e.emit_output_actual_header();
	e.emit_retval("%s", a->Value());
	if(strcmp(a->Value(), "happy 3") != MATCH) {
		e.emit_result_failure(__LINE__);
		delete a;
		return false;
	}
	e.emit_result_success(__LINE__);
	delete a;
	return true;
}

static bool vsprintf_non_empty(void) {
	e.emit_test("Test vsprintf() on a non-empty MyString.");
	MyString* a = new MyString("foo");
	vsprintfHelper(a, "%s %d", "happy", 3);
	e.emit_input_header();
	e.emit_param("STRING", "%s", "happy");
	e.emit_param("INT", "%d", 3);
	e.emit_output_expected_header();
	e.emit_retval("%s", "happy 3");
	e.emit_output_actual_header();
	e.emit_retval("%s", a->Value());
	if(strcmp(a->Value(), "happy 3") != MATCH) {
		e.emit_result_failure(__LINE__);
		delete a;
		return false;
	}
	e.emit_result_success(__LINE__);
	delete a;
	return true;
}

static bool vsprintf_return_true(void) {
	e.emit_test("Does vsprintf() return true on success?");
	MyString* a = new MyString();
	bool res = vsprintfHelper(a, "%s %d", "happy", 3);
	e.emit_input_header();
	e.emit_param("STRING", "%s", "happy");
	e.emit_param("INT", "%d", 3);
	e.emit_output_expected_header();
	e.emit_retval("%d", 1);
	e.emit_output_actual_header();
	e.emit_retval("%d", res);
	if(!res) {
		e.emit_result_failure(__LINE__);
		delete a;
		return false;
	}
	e.emit_result_success(__LINE__);
	delete a;
	return true;
}



static bool sprintf_cat_empty(void) {
	e.emit_test("Test sprintf_cat() on an empty MyString.");
	MyString a;
	a.sprintf_cat("%s %d", "happy", 3);
	e.emit_input_header();
	e.emit_param("STRING", "%s", "happy");
	e.emit_param("INT", "%d", 3);
	e.emit_output_expected_header();
	e.emit_retval("%s", "happy 3");
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "happy 3") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 279
static bool sprintf_cat_non_empty(void) {
	e.emit_test("Test sprintf_cat() on a non-empty MyString.");
	MyString a("sad 5");
	a.sprintf_cat(" Luis Ocana");
	e.emit_input_header();
	e.emit_param("STRING", "%s", " Luis Ocana");
	e.emit_output_expected_header();
	e.emit_retval("%s", "sad 5 Luis Ocana");
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "sad 5 Luis Ocana") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool sprintf_cat_return_true(void) {
	e.emit_test("Does sprintf_cat() return true on success?");
	MyString a;
	bool res = a.sprintf_cat("%s %d", "sad", 5);
	e.emit_input_header();
	e.emit_param("STRING", "%s", "sad");
	e.emit_param("INT", "%d", 5);
	e.emit_output_expected_header();
	e.emit_retval("%d", 1);
	e.emit_output_actual_header();
	e.emit_retval("%d", res);
	if(!res) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool vsprintf_cat_empty(void) {
	e.emit_test("Test vsprintf_cat() on an empty MyString.");
	MyString* a = new MyString();
	vsprintf_catHelper(a, "%s %d", "happy", 3);
	e.emit_input_header();
	e.emit_param("STRING", "%s", "happy");
	e.emit_param("INT", "%d", 3);
	e.emit_output_expected_header();
	e.emit_retval("%s", "happy 3");
	e.emit_output_actual_header();
	e.emit_retval("%s", a->Value());
	if(strcmp(a->Value(), "happy 3") != MATCH) {
		e.emit_result_failure(__LINE__);
		delete a;
		return false;
	}
	e.emit_result_success(__LINE__);
	delete a;
	return true;
}

static bool vsprintf_cat_non_empty(void) {
	e.emit_test("Test vsprintf_cat() on a non-empty MyString.");
	MyString* a = new MyString("foo");
	vsprintf_catHelper(a, "%s %d", "happy", 3);
	e.emit_input_header();
	e.emit_param("STRING", "%s", "happy");
	e.emit_param("INT", "%d", 3);
	e.emit_output_expected_header();
	e.emit_retval("%s", "foohappy 3");
	e.emit_output_actual_header();
	e.emit_retval("%s", a->Value());
	if(strcmp(a->Value(), "foohappy 3") != MATCH) {
		e.emit_result_failure(__LINE__);
		delete a;
		return false;
	}
	e.emit_result_success(__LINE__);
	delete a;
	return true;
}

static bool vsprintf_cat_return_true(void) {
	e.emit_test("Does vsprintf_cat() return true on success?");
	MyString* a = new MyString();
	bool res = vsprintf_catHelper(a, "%s %d", "happy", 3);
	e.emit_input_header();
	e.emit_param("STRING", "%s", "happy");
	e.emit_param("INT", "%d", 3);
	e.emit_output_expected_header();
	e.emit_retval("%d", 1);
	e.emit_output_actual_header();
	e.emit_retval("%d", res);
	if(!res) {
		e.emit_result_failure(__LINE__);
		delete a;
		return false;
	}
	e.emit_result_success(__LINE__);
	delete a;
	return true;
}



static bool append_to_list_string_empty_empty(void) {
	e.emit_test("Test append_to_list() on an empty MyString when passed an "
		"empty string.");
	MyString a;
	a.append_to_list("");
	e.emit_input_header();
	e.emit_param("STRING", "%s", "");
	e.emit_output_expected_header();
	e.emit_retval("%s", "");
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 638
static bool append_to_list_string_empty_non_empty(void) {
	e.emit_test("Test append_to_list() on an empty MyString when passed a "
		"non-empty string.");
	MyString a;
	a.append_to_list("Armstrong");
	e.emit_input_header();
	e.emit_param("STRING", "%s", "Armstrong");
	e.emit_output_expected_header();
	e.emit_retval("%s", "Armstrong");
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "Armstrong") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool append_to_list_string_non_empty_empty(void) {
	e.emit_test("Test append_to_list() on a non-empty MyString when passed an "
		"empty string.");
	// Test revealed problem, MyString was fixed. See ticket #1292
	MyString a("ABC");
	a.append_to_list("");
	e.emit_input_header();
	e.emit_param("STRING", "%s", "");
	e.emit_output_expected_header();
	e.emit_retval("%s", "ABC");
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "ABC") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}


static bool append_to_list_string_non_empty(void) {
	e.emit_test("Test append_to_list() on a non-empty MyString.");
	MyString a("Armstrong");
	a.append_to_list(" Pereiro");
	e.emit_input_header();
	e.emit_param("STRING", "%s", "Pereiro");
	e.emit_output_expected_header();
	e.emit_retval("%s", "Armstrong, Pereiro");
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "Armstrong, Pereiro") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool append_to_list_string_multiple(void) {
	e.emit_test("Test multiple calls to append_to_list() on a non-empty "
		"MyString.");
	MyString a("Armstrong");
	a.append_to_list(" Pereiro");
	a.append_to_list("Contador", "; ");
	a.append_to_list(" Sastre");
	e.emit_input_header();
	e.emit_param("First Append", "%s", "Pereiro");
	e.emit_param("Second Append", "%s", ";Contador");
	e.emit_param("Third Append", "%s", "Sastre");
	e.emit_output_expected_header();
	e.emit_retval("%s", "Armstrong, Pereiro; Contador, Sastre");
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "Armstrong, Pereiro; Contador, Sastre") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool append_to_list_mystring_empty_empty(void) {
	e.emit_test("Test append_to_list() on an empty MyString when passed an "
		"empty MyString.");
	MyString a;
	MyString b;
	a.append_to_list(b);
	e.emit_input_header();
	e.emit_param("MyString", "%s", "");
	e.emit_output_expected_header();
	e.emit_retval("%s", "");
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool append_to_list_mystring_empty_non_empty(void) {
	e.emit_test("Test append_to_list() on an empty MyString when passed a "
		"non-empty MyString.");
	MyString a;
	MyString b("Armstrong");
	a.append_to_list(b);
	e.emit_input_header();
	e.emit_param("MyString", "%s", "Armstrong");
	e.emit_output_expected_header();
	e.emit_retval("%s", "Armstrong");
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "Armstrong") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool append_to_list_mystring_non_empty_empty(void) {
	e.emit_test("Test append_to_list() on a non-empty MyString when passed an "
		"empty MyString.");
	// Test revealed problem, MyString was fixed. See ticket #1293
	MyString a("ABC");
	MyString b;
	a.append_to_list(b);
	e.emit_input_header();
	e.emit_param("MyString", "%s", "");
	e.emit_output_expected_header();
	e.emit_retval("%s", "ABC");
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "ABC") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}


//in test_mystring.cpp 640
static bool append_to_list_mystring_non_empty(void) {
	e.emit_test("Test append_to_list() on a non-empty MyString.");
	MyString a("Armstrong");
	MyString b(" Pereiro");
	a.append_to_list(b);
	e.emit_input_header();
	e.emit_param("MyString", "%s", "Pereiro");
	e.emit_output_expected_header();
	e.emit_retval("%s", "Armstrong, Pereiro");
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "Armstrong, Pereiro") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool append_to_list_mystring_multiple(void) {
	e.emit_test("Test multiple calls to append_to_list() on a non-empty "
		"MyString.");
	MyString a("Armstrong");
	MyString b(" Pereiro");
	MyString c("Contador");
	MyString d(" Sastre");
	a.append_to_list(b);
	a.append_to_list(c, "; ");
	a.append_to_list(d);
	e.emit_input_header();
	e.emit_param("First Append", "%s", "Pereiro");
	e.emit_param("Second Append", "%s", ";Contador");
	e.emit_param("Third Append", "%s", "Sastre");
	e.emit_output_expected_header();
	e.emit_retval("%s", "Armstrong, Pereiro; Contador, Sastre");
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "Armstrong, Pereiro; Contador, Sastre") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool lower_case_empty(void) {
	e.emit_test("Test lower_case() on an empty MyString.");
	MyString a;
	a.lower_case();
	e.emit_output_expected_header();
	e.emit_retval("%s", "");
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool lower_case_non_empty(void) {
	e.emit_test("Test lower_case() on a non-empty MyString.");
	MyString a("lower UPPER");
	a.lower_case();
	e.emit_output_expected_header();
	e.emit_retval("%s", "lower upper");
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "lower upper") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool upper_case_empty(void) {
	e.emit_test("Test upper_case() on an empty MyString.");
	MyString a;
	a.upper_case();
	e.emit_output_expected_header();
	e.emit_retval("%s", "");
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool upper_case_non_empty(void) {
	e.emit_test("Test upper_case() on a non-empty MyString.");
	MyString a("lower UPPER");
	a.upper_case();
	e.emit_output_expected_header();
	e.emit_retval("%s", "LOWER UPPER");
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "LOWER UPPER") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool chomp_new_line_end(void) {
	e.emit_test("Does chomp() remove the newLine if its the last character in "
		"the MyString?");
	MyString a("stuff\n");
	a.chomp();
	e.emit_output_expected_header();
	e.emit_retval("%s", "stuff");
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "stuff") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool chomp_new_line_beginning(void) {
	e.emit_test("Does chomp() do nothing if the newLine if its not the last "
		"character in the MyString?");
	MyString a("stuff\nmore stuff");
	a.chomp();
	e.emit_output_expected_header();
	e.emit_retval("%s", "stuff\nmore stuff");
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "stuff\nmore stuff") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool chomp_return_false(void) {
	e.emit_test("Does chomp() return false if the MyString doesn't have a "
		"newLine?");
	MyString a("stuff");
	bool result = a.chomp();
	e.emit_output_expected_header();
	e.emit_retval("%d", 0);
	e.emit_output_actual_header();
	e.emit_retval("%d", result);
	if(result) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool chomp_return_true(void) {
	e.emit_test("Does chomp() return true if the MyString has a newLine at the "
		"end?");
	MyString a("stuff\n");
	bool result = a.chomp();
	e.emit_output_expected_header();
	e.emit_retval("%d", 1);
	e.emit_output_actual_header();
	e.emit_retval("%d", result);
	if(!result) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 539
static bool trim_beginning_and_end(void) {
	e.emit_test("Test trim on a MyString with white space at the beginning and "
		"end.");
	MyString a("  Miguel   ");
	a.trim();
	e.emit_output_expected_header();
	e.emit_retval("%s", "Miguel");
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "Miguel") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 567
static bool trim_end(void) {
	e.emit_test("Test trim() on a MyString with white space at the end.");
	MyString a("Hinault   ");
	a.trim();
	e.emit_output_expected_header();
	e.emit_retval("%s", "Hinault");
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "Hinault") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 548
static bool trim_none(void) {
	e.emit_test("Test trim() on a MyString with no white space.");
	MyString a("Indurain");
	a.trim();
	e.emit_output_expected_header();
	e.emit_retval("%s", "Indurain");
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "Indurain") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 558
static bool trim_beginning(void) {
	e.emit_test("Test trim() on a MyString with white space at the beginning.");
	MyString a("   Merckx");
	a.trim();
	e.emit_output_expected_header();
	e.emit_retval("%s", "Merckx");
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "Merckx") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool trim_empty(void) {
	e.emit_test("Test trim() on an empty MyString.");
	MyString a;
	a.trim();
	e.emit_output_expected_header();
	e.emit_retval("%s", "");
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 53
static bool equality_mystring_copied(void) {
	e.emit_test("Test == between two MyStrings after using the copy "
		"constructor.");
	MyString a("12");
	MyString b(a);
	bool result = (a == b);
	e.emit_input_header();
	e.emit_param("MyString", "%s", a.Value());
	e.emit_param("MyString", "%s", b.Value());
	e.emit_output_expected_header();
	e.emit_retval("%d", 1);
	e.emit_output_actual_header();
	e.emit_retval("%d", result);
	if(!result) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool equality_mystring_one_char(void) {
	e.emit_test("Test == between two MyStrings of one character.");
	MyString a("1");
	MyString b("1");
	bool result = (a == b);
	e.emit_input_header();
	e.emit_param("MyString", "%s", a.Value());
	e.emit_param("MyString", "%s", b.Value());
	e.emit_output_expected_header();
	e.emit_retval("%d", 1);
	e.emit_output_actual_header();
	e.emit_retval("%d", result);
	if(!result) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool equality_mystring_two_char(void) {
	e.emit_test("Test == between two MyStrings of two characters.");
	MyString a("12");
	MyString b("12");
	bool result = (a == b);
	e.emit_input_header();
	e.emit_param("MyString", "%s", a.Value());
	e.emit_param("MyString", "%s", b.Value());
	e.emit_output_expected_header();
	e.emit_retval("%d", 1);
	e.emit_output_actual_header();
	e.emit_retval("%d", result);
	if(!result) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool equality_mystring_substring(void) {
	e.emit_test("Test == between a MyString that is a substring of the other.");
	MyString a("12345");
	MyString b("123");
	bool result = (a == b);
	e.emit_input_header();
	e.emit_param("MyString", "%s", a.Value());
	e.emit_param("MyString", "%s", b.Value());
	e.emit_output_expected_header();
	e.emit_retval("%d", 0);
	e.emit_output_actual_header();
	e.emit_retval("%d", result);
	if(result) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool equality_mystring_one_empty(void) {
	e.emit_test("Test == between a MyString that is empty and one that isn't.");
	MyString a("12345");
	MyString b;
	bool result = (a == b);
	e.emit_input_header();
	e.emit_param("MyString", "%s", a.Value());
	e.emit_param("MyString", "%s", b.Value());
	e.emit_output_expected_header();
	e.emit_retval("%d", 0);
	e.emit_output_actual_header();
	e.emit_retval("%d", result);
	if(result) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool equality_mystring_two_empty(void) {
	e.emit_test("Test == between a two empty MyStrings.");
	MyString a;
	MyString b;
	bool result = (a == b);
	e.emit_input_header();
	e.emit_param("MyString", "%s", a.Value());
	e.emit_param("MyString", "%s", b.Value());
	e.emit_output_expected_header();
	e.emit_retval("%d", 0);
	e.emit_output_actual_header();
	e.emit_retval("%d", result);
	if(!result) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 36
static bool equality_string_one_char(void) {
	e.emit_test("Test == between a MyString and a string of one character.");
	MyString a("1");
	char* b = "1";
	bool result = (a == b);
	e.emit_input_header();
	e.emit_param("MyString", "%s", a.Value());
	e.emit_param("String", "%s", b);
	e.emit_output_expected_header();
	e.emit_retval("%d", 1);
	e.emit_output_actual_header();
	e.emit_retval("%d", result);
	if(!result) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 44
static bool equality_string_two_char(void) {
	e.emit_test("Test == between a MyString and a string of two characters.");
	MyString a("12");
	char* b = "12";
	bool result = (a == b);
	e.emit_input_header();
	e.emit_param("MyString", "%s", a.Value());
	e.emit_param("String", "%s", b);
	e.emit_output_expected_header();
	e.emit_retval("%d", 1);
	e.emit_output_actual_header();
	e.emit_retval("%d", result);
	if(!result) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 62
static bool equality_string_copied_delete(void) {
	e.emit_test("Test == between two MyStrings after using the copy constructor"
		" and then deleting the original MyString.");
	char* b = "12";
	MyString* b_ms = new MyString(b);
	MyString b_dup(*b_ms);
	delete b_ms;
	bool result = (b == b_dup);
	e.emit_input_header();
	e.emit_param("String", "%s", b);
	e.emit_param("MyString", "%s", b_dup.Value());
	e.emit_output_expected_header();
	e.emit_retval("%d", 1);
	e.emit_output_actual_header();
	e.emit_retval("%d", result);
	if(!result) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool equality_string_substring(void) {
	e.emit_test("Test == between a String that is a substring of the "
		"MyString.");
	MyString a("12345");
	char* b = "123";
	bool result = (a == b);
	e.emit_input_header();
	e.emit_param("MyString", "%s", a.Value());
	e.emit_param("String", "%s", b);
	e.emit_output_expected_header();
	e.emit_retval("%d", 0);
	e.emit_output_actual_header();
	e.emit_retval("%d", result);
	if(result) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool equality_string_one_empty(void) {
	e.emit_test("Test == between a MyString that is empty and a string that "
		"isn't.");
	MyString a;
	char* b = "12345";
	bool result = (a == b);
	e.emit_input_header();
	e.emit_param("MyString", "%s", a.Value());
	e.emit_param("String", "%s", b);
	e.emit_output_expected_header();
	e.emit_retval("%d", 0);
	e.emit_output_actual_header();
	e.emit_retval("%d", result);
	if(result) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool equality_string_two_empty(void) {
	e.emit_test("Test == between an empty MyString and an empty string.");
	MyString a;
	char* b = "\0";
	bool result = (a == b);
	e.emit_input_header();
	e.emit_param("MyString", "%s", a.Value());
	e.emit_param("String", "%s", "NULL");
	e.emit_output_expected_header();
	e.emit_retval("%d", 1);
	e.emit_output_actual_header();
	e.emit_retval("%d", result);
	if(!result) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool inequality_mystring_copy(void) {
	e.emit_test("Test != between two MyStrings after using the copy "
		"constructor.");
	MyString a("foo");
	MyString b(a);
	bool result = (a != b);
	e.emit_input_header();
	e.emit_param("MyString", "%s", a.Value());
	e.emit_param("MyString", "%s", b.Value());
	e.emit_output_expected_header();
	e.emit_retval("%d", 0);
	e.emit_output_actual_header();
	e.emit_retval("%d", result);
	if(result) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 78
static bool inequality_mystring_not_equal(void) {
	e.emit_test("Test != between two MyStrings that are not equal.");
	MyString a("12");
	MyString b("1");
	bool result = (a != b);
	e.emit_input_header();
	e.emit_param("MyString", "%s", a.Value());
	e.emit_param("MyString", "%s", b.Value());
	e.emit_output_expected_header();
	e.emit_retval("%d", 1);
	e.emit_output_actual_header();
	e.emit_retval("%d", result);
	if(!result) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool inequality_string_equal(void) {
	e.emit_test("Test != between a MyString and a string that are equal.");
	MyString a("123");
	char* b = "123";
	bool result = (a != b);
	e.emit_input_header();
	e.emit_param("MyString", "%s", a.Value());
	e.emit_param("String", "%s", b);
	e.emit_output_expected_header();
	e.emit_retval("%d", 0);
	e.emit_output_actual_header();
	e.emit_retval("%d", result);
	if(result) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 70
static bool inequality_string_not_equal(void) {
	e.emit_test("Test != between a MyString and a string that are not equal.");
	MyString a("12");
	char* b = "1";
	bool result = (a != b);
	e.emit_input_header();
	e.emit_param("MyString", "%s", a.Value());
	e.emit_param("String", "%s", b);
	e.emit_output_expected_header();
	e.emit_retval("%d", 1);
	e.emit_output_actual_header();
	e.emit_retval("%d", result);
	if(!result) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 86
static bool less_than_mystring_less(void) {
	e.emit_test("Test < on a MyString that is less than the other MyString.");
	MyString a("1");
	MyString b("12");
	bool result = (a < b);
	e.emit_input_header();
	e.emit_param("MyString", "%s", a.Value());
	e.emit_param("MyString", "%s", b.Value());
	e.emit_output_expected_header();
	e.emit_retval("%d", 1);
	e.emit_output_actual_header();
	e.emit_retval("%d", result);
	if(!result) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool less_than_mystring_equal(void) {
	e.emit_test("Test < between two MyStrings that are equal.");
	MyString a("foo");
	MyString b(a);
	bool result = (a < b);
	e.emit_input_header();
	e.emit_param("MyString", "%s", a.Value());
	e.emit_param("MyString", "%s", b.Value());
	e.emit_output_expected_header();
	e.emit_retval("%d", 0);
	e.emit_output_actual_header();
	e.emit_retval("%d", result);
	if(result) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool less_than_mystring_greater(void) {
	e.emit_test("Test < on a MyString that is greater than the other "
		"MyString.");
	MyString a("foo");
	MyString b("bar");
	bool result = (a < b);
	e.emit_input_header();
	e.emit_param("MyString", "%s", a.Value());
	e.emit_param("MyString", "%s", b.Value());
	e.emit_output_expected_header();
	e.emit_retval("%d", 0);
	e.emit_output_actual_header();
	e.emit_retval("%d", result);
	if(result) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool less_than_or_equal_mystring_less(void) {
	e.emit_test("Test <= on a MyString that is less than the other MyString.");
	MyString a("bar");
	MyString b("foo");
	bool result = (a <= b);
	e.emit_input_header();
	e.emit_param("MyString", "%s", a.Value());
	e.emit_param("MyString", "%s", b.Value());
	e.emit_output_expected_header();
	e.emit_retval("%d", 1);
	e.emit_output_actual_header();
	e.emit_retval("%d", result);
	if(!result) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool less_than_or_equal_mystring_equal(void) {
	e.emit_test("Test <= on a MyString that is equal to the other MyString.");
	MyString a("foo");
	MyString b(a);
	bool result = (a <= b);
	e.emit_input_header();
	e.emit_param("MyString", "%s", a.Value());
	e.emit_param("MyString", "%s", b.Value());
	e.emit_output_expected_header();
	e.emit_retval("%d", 1);
	e.emit_output_actual_header();
	e.emit_retval("%d", result);
	if(!result) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool less_than_or_equal_mystring_greater(void) {
	e.emit_test("Test <= on a MyString that is greater that the other "
		"MyString.");
	MyString a("foo");
	MyString b("bar");
	bool result = (a <= b);
	e.emit_input_header();
	e.emit_param("MyString", "%s", a.Value());
	e.emit_param("MyString", "%s", b.Value());
	e.emit_output_expected_header();
	e.emit_retval("%d", 0);
	e.emit_output_actual_header();
	e.emit_retval("%d", result);
	if(result) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool greater_than_mystring_less(void) {
	e.emit_test("Test > on a MyString that is less than the other MyString.");
	MyString a("bar");
	MyString b("foo");
	bool result = (a > b);
	e.emit_input_header();
	e.emit_param("MyString", "%s", a.Value());
	e.emit_param("MyString", "%s", b.Value());
	e.emit_output_expected_header();
	e.emit_retval("%d", 0);
	e.emit_output_actual_header();
	e.emit_retval("%d", result);
	if(result) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool greater_than_mystring_equal(void) {
	e.emit_test("Test > on a MyString that is equal to the other MyString.");
	MyString a("foo");
	MyString b(a);
	bool result = (a > b);
	e.emit_input_header();
	e.emit_param("MyString", "%s", a.Value());
	e.emit_param("MyString", "%s", b.Value());
	e.emit_output_expected_header();
	e.emit_retval("%d", 0);
	e.emit_output_actual_header();
	e.emit_retval("%d", result);
	if(result) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool greater_than_mystring_greater(void) {
	e.emit_test("Test > on a MyString that is greater than the other "
		"MyString.");
	MyString a("foo");
	MyString b("bar");
	bool result = (a > b);
	e.emit_input_header();
	e.emit_param("MyString", "%s", a.Value());
	e.emit_param("MyString", "%s", b.Value());
	e.emit_output_expected_header();
	e.emit_retval("%d", 1);
	e.emit_output_actual_header();
	e.emit_retval("%d", result);
	if(!result) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool greater_than_or_equal_mystring_less(void) {
	e.emit_test("Test >= on a MyString that is less than the other MyString.");
	MyString a("bar");
	MyString b("foo");
	bool result = (a >= b);
	e.emit_input_header();
	e.emit_param("MyString", "%s", a.Value());
	e.emit_param("MyString", "%s", b.Value());
	e.emit_output_expected_header();
	e.emit_retval("%d", 0);
	e.emit_output_actual_header();
	e.emit_retval("%d", result);
	if(result) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool greater_than_or_equal_mystring_equal(void) {
	e.emit_test("Test >= on a MyString that is equal to the other MyString.");
	MyString a("foo");
	MyString b(a);
	bool result = (a >= b);
	e.emit_input_header();
	e.emit_param("MyString", "%s", a.Value());
	e.emit_param("MyString", "%s", b.Value());
	e.emit_output_expected_header();
	e.emit_retval("%d", 1);
	e.emit_output_actual_header();
	e.emit_retval("%d", result);
	if(!result) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool greater_than_or_equal_mystring_greater(void) {
	e.emit_test("Test >= on a MyString that is greater than the other "
		"MyString.");
	MyString a("foo");
	MyString b("bar");
	bool result = (a >= b);
	e.emit_input_header();
	e.emit_param("MyString", "%s", a.Value());
	e.emit_param("MyString", "%s", b.Value());
	e.emit_output_expected_header();
	e.emit_retval("%d", 1);
	e.emit_output_actual_header();
	e.emit_retval("%d", result);
	if(!result) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool read_line_empty_file_return(void) {
	e.emit_test("Does readLine() return false when called with an empty file?");
	MyString a;
	FILE* tmp = tmpfile();
	bool res = a.readLine(tmp, false);
	fclose(tmp);
	e.emit_input_header();
	e.emit_param("FILE", "%s", "tmp");
	e.emit_param("append", "%d", false);
	e.emit_output_expected_header();
	e.emit_retval("%d", 0);
	e.emit_output_actual_header();
	e.emit_retval("%d", res);
	if(res) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool read_line_empty_file_preserve(void) {
	e.emit_test("Does readLine() preserve the original MyString when called "
		"with an empty file?");
	MyString a("foo");
	FILE* tmp = tmpfile();
	a.readLine(tmp, false);
	fclose(tmp);
	e.emit_input_header();
	e.emit_param("FILE", "%s", "tmp");
	e.emit_param("append", "%d", false);
	e.emit_output_expected_header();
	e.emit_retval("%s", "foo");
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "foo") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool read_line_empty(void) {
	e.emit_test("Does readLine() correctly append to an empty MyString?");
	MyString a;
	FILE* tmp = tmpfile();
	fputs("foo", tmp);
	rewind(tmp);
	a.readLine(tmp, true);
	fclose(tmp);
	e.emit_input_header();
	e.emit_param("FILE", "%s", "tmp");
	e.emit_param("append", "%d", true);
	e.emit_output_expected_header();
	e.emit_retval("%s", "foo");
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "foo") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool read_line_null_file(void) {
	e.emit_test("Test readLine() when passed a null file pointer.");
	e.emit_problem("By inspection, readLine() code correctly ASSERTs on fp=NULL, but we can't verify that in the current unit test framework.");

/*
	MyString a("foo");
	FILE* tmp = NULL;
	a.readLine(tmp, true);
	e.emit_input_header();
	e.emit_param("FILE", "%s", "tmp");
	e.emit_param("append", "%d", false);
	e.emit_output_expected_header();
	e.emit_retval("%s", "foo");
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "foo") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
*/
	e.emit_result_success(__LINE__);
	return true;
}


static bool read_line_append(void) {
	e.emit_test("Does readLine() correctly append to a MyString from a file?");
	MyString a("Line");
	FILE* tmp = tmpfile();
	fputs(" of text", tmp);
	rewind(tmp);
	a.readLine(tmp, true);
	fclose(tmp);
	e.emit_input_header();
	e.emit_param("FILE", "%s", "tmp");
	e.emit_param("append", "%d", false);
	e.emit_output_expected_header();
	e.emit_retval("%s", "Line of text");
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "Line of text") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool read_line_return(void) {
	e.emit_test("Does readLine() return true when reading a line from a file?");
	MyString a("Line");
	FILE* tmp = tmpfile();
	fputs(" of text", tmp);
	rewind(tmp);
	bool res = a.readLine(tmp, false);
	fclose(tmp);
	e.emit_input_header();
	e.emit_param("FILE", "%s", "tmp");
	e.emit_param("append", "%d", false);
	e.emit_output_expected_header();
	e.emit_retval("%d", 1);
	e.emit_output_actual_header();
	e.emit_retval("%d", res);
	if(!res) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool read_line_replace(void) {
	e.emit_test("Does readLine() replace the original MyString when append is "
		"set to false?");
	MyString a("foo");
	FILE* tmp = tmpfile();
	fputs("bar", tmp);
	rewind(tmp);
	a.readLine(tmp, false);
	fclose(tmp);
	e.emit_input_header();
	e.emit_param("FILE", "%s", "tmp");
	e.emit_param("append", "%d", false);
	e.emit_output_expected_header();
	e.emit_retval("%s", "bar");
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "bar")) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool read_line_multiple(void) {
	e.emit_test("Test multiple calls to readLine().");
	MyString a;
	FILE* tmp = tmpfile();
	fputs("Line 1\nLine 2\nLine 3\nLine 4", tmp);
	rewind(tmp);
	a.readLine(tmp, true);
	a.readLine(tmp, true);
	a.readLine(tmp, true);
	a.readLine(tmp, true);
	a.readLine(tmp, true);
	fclose(tmp);
	e.emit_input_header();
	e.emit_param("FILE", "%s", "tmp");
	e.emit_param("append", "%d", true);
	e.emit_output_expected_header();
	e.emit_retval("%s", "Line 1\nLine 2\nLine 3\nLine 4");
	e.emit_output_actual_header();
	e.emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "Line 1\nLine 2\nLine 3\nLine 4")) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 460
static bool tokenize_null(void) {
	e.emit_test("Does calling GetNextToken() before calling Tokenize() return "
		"NULL?");
	MyString a("foo, bar");
	const char* tok = a.GetNextToken(",", false);
	e.emit_input_header();
	e.emit_param("delim", "%s", ",");
	e.emit_param("skipBlankTokens", "%d", false);
	e.emit_output_expected_header();
	e.emit_retval("%s", "NULL");
	if(tok != NULL) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 471
static bool tokenize_skip(void) {
	e.emit_test("Test GetNextToken() when skipping blank tokens.");
	MyString a("     Ottavio Bottechia_");
	a.Tokenize();
	const char* tok = a.GetNextToken(" ", true);
	e.emit_input_header();
	e.emit_param("delim", "%s", " ");
	e.emit_param("skipBlankTokens", "%d", true);
	e.emit_output_expected_header();
	e.emit_retval("%s", "Ottavio");
	e.emit_output_actual_header();
	e.emit_retval("%s", tok);
	if(strcmp(tok, "Ottavio") != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 483
static bool tokenize_multiple_calls(void) {
	e.emit_test("Test multiple calls to GetNextToken().");
	MyString a("To  be or not to be; that is the question");
	a.Tokenize();
	const char* expectedTokens[] = {"To", "", "be", "or", "not", "to", "be", ""
		, "that", "is", "the", "question"};
	const char* resultToken0 = a.GetNextToken(" ;", false);
	const char* resultToken1 = a.GetNextToken(" ;", false);
	const char* resultToken2 = a.GetNextToken(" ;", false);
	const char* resultToken3 = a.GetNextToken(" ;", false);
	const char* resultToken4 = a.GetNextToken(" ;", false);
	const char* resultToken5 = a.GetNextToken(" ;", false);
	const char* resultToken6 = a.GetNextToken(" ;", false);
	const char* resultToken7 = a.GetNextToken(" ;", false);
	const char* resultToken8 = a.GetNextToken(" ;", false);
	const char* resultToken9 = a.GetNextToken(" ;", false);
	const char* resultToken10 = a.GetNextToken(" ;", false);
	const char* resultToken11 = a.GetNextToken(" ;", false);
	e.emit_input_header();
	e.emit_param("delim", "%s", " ;");
	e.emit_param("skipBlankTokens", "%d", false);
	e.emit_output_expected_header();
	e.emit_param("Token 1", "%s", expectedTokens[0]);
	e.emit_param("Token 2", "%s", expectedTokens[1]);
	e.emit_param("Token 3", "%s", expectedTokens[2]);
	e.emit_param("Token 4", "%s", expectedTokens[3]);
	e.emit_param("Token 5", "%s", expectedTokens[4]);
	e.emit_param("Token 6", "%s", expectedTokens[5]);
	e.emit_param("Token 7", "%s", expectedTokens[6]);
	e.emit_param("Token 8", "%s", expectedTokens[7]);
	e.emit_param("Token 9", "%s", expectedTokens[8]);
	e.emit_param("Token 10", "%s", expectedTokens[9]);
	e.emit_param("Token 11", "%s", expectedTokens[10]);
	e.emit_param("Token 12", "%s", expectedTokens[11]);
	e.emit_output_actual_header();
	e.emit_param("Token 1", "%s", resultToken0);
	e.emit_param("Token 2", "%s", resultToken1);
	e.emit_param("Token 3", "%s", resultToken2);
	e.emit_param("Token 4", "%s", resultToken3);
	e.emit_param("Token 5", "%s", resultToken4);
	e.emit_param("Token 6", "%s", resultToken5);
	e.emit_param("Token 7", "%s", resultToken6);
	e.emit_param("Token 8", "%s", resultToken7);
	e.emit_param("Token 9", "%s", resultToken8);
	e.emit_param("Token 10", "%s", resultToken9);
	e.emit_param("Token 11", "%s", resultToken10);
	e.emit_param("Token 12", "%s", resultToken11);
	if(strcmp(expectedTokens[0], resultToken0) != MATCH || 
			strcmp(expectedTokens[1], resultToken1) != MATCH ||
			strcmp(expectedTokens[2], resultToken2) != MATCH ||
			strcmp(expectedTokens[3], resultToken3) != MATCH ||
			strcmp(expectedTokens[4], resultToken4) != MATCH ||
			strcmp(expectedTokens[5], resultToken5) != MATCH ||
			strcmp(expectedTokens[6], resultToken6) != MATCH ||
			strcmp(expectedTokens[7], resultToken7) != MATCH ||
			strcmp(expectedTokens[8], resultToken8) != MATCH ||
			strcmp(expectedTokens[9], resultToken9) != MATCH ||
			strcmp(expectedTokens[10], resultToken10) != MATCH ||
			strcmp(expectedTokens[11], resultToken11) != MATCH) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 491
static bool tokenize_end(void) {
	e.emit_test("Test GetNextToken() after getting to the end.");
	MyString a("foo;");
	a.Tokenize();
	const char* tok = a.GetNextToken(";", false);
	tok = a.GetNextToken(";", false);
	tok = a.GetNextToken(";", false);
	e.emit_input_header();
	e.emit_param("delim", "%s", ";");
	e.emit_param("skipBlankTokens", "%d", false);
	e.emit_output_expected_header();
	e.emit_retval("%s", "NULL");
	if(tok != NULL) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 519
static bool tokenize_empty(void) {
	e.emit_test("Test GetNextToken() on an empty MyString.");
	MyString a;
	a.Tokenize();
	const char* tok = a.GetNextToken(" ", false);
	e.emit_input_header();
	e.emit_param("delim", "%s", " ");
	e.emit_param("skipBlankTokens", "%d", false);
	e.emit_output_expected_header();
	e.emit_retval("%s", "NULL");
	if(tok != NULL) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

//in test_mystring.cpp 529
static bool tokenize_empty_delimiter(void) {
	e.emit_test("Test GetNextToken() on an empty delimiter string.");
	MyString a("foobar");
	a.Tokenize();
	const char* tok = a.GetNextToken("", false);
	e.emit_input_header();
	e.emit_param("delim", "%s", " ");
	e.emit_param("skipBlankTokens", "%d", false);
	e.emit_output_expected_header();
	e.emit_retval("%s", "NULL");
	if(tok != NULL) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool sensitive_string_string_constructor(void) {
	e.emit_test("Test the YourSensitiveString constructor that assigns the "
		"sensitive string to the passed c string.");
	YourSensitiveString a("foo");
	e.emit_output_expected_header();
	e.emit_retval("%s", "foo");
	if(!(a == "foo")) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool sensitive_string_equality_true(void) {
	e.emit_test("Does the == operator return true when the sensitive string is "
		"equal to the other string?");
	YourSensitiveString a("foo");
	bool res = (a == "foo");
	e.emit_input_header();
	e.emit_param("STRING", "%s", "foo");
	e.emit_output_expected_header();
	e.emit_retval("%d", 1);
	e.emit_output_actual_header();
	e.emit_retval("%d", res);
	if(!res) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool sensitive_string_equality_false(void) {
	e.emit_test("Does the == operator return false when the sensitive string is"
		" not equal to the other string?");
	YourSensitiveString a("foo");
	bool res = (a == "bar");
	e.emit_input_header();
	e.emit_param("STRING", "%s", "bar");
	e.emit_output_expected_header();
	e.emit_retval("%d", 0);
	e.emit_output_actual_header();
	e.emit_retval("%d", res);
	if(res) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool sensitive_string_equality_default_constructor(void) {
	e.emit_test("Does the == operator return false after using the default "
		"YourSensitiveString constructor?");

	YourSensitiveString a;
	bool res = (a == "foo");
	e.emit_input_header();
	e.emit_param("STRING", "%s", "foo");
	e.emit_output_expected_header();
	e.emit_retval("%d", 0);
	e.emit_output_actual_header();
	e.emit_retval("%d", res);
	if(res) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}


static bool sensitive_string_assignment_non_empty_empty(void) {
	e.emit_test("Does the = operator assign the non-empty sensitive string to "
		"an empty one?");
	YourSensitiveString a("foo");
	a = "";
	e.emit_input_header();
	e.emit_param("STRING", "%s", "");
	e.emit_output_expected_header();
	e.emit_retval("%s", "");
	if(!(a == "")) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool sensitive_string_assignment_empty_non_empty(void) {
	e.emit_test("Does the = operator assign the empty sensitive string to a "
		"non-empty one?");
	YourSensitiveString a;
	a = "foo";
	e.emit_input_header();
	e.emit_param("STRING", "%s", "foo");
	e.emit_output_expected_header();
	e.emit_retval("%s", "foo");
	if(!(a == "foo")) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool sensitive_string_assignment_non_empty(void) {
	e.emit_test("Does the = operator assign the non-empty sensitive string to a"
		" different non-empty one of different length?");
	YourSensitiveString a("foo");
	a = "b";
	e.emit_input_header();
	e.emit_param("STRING", "%s", "b");
	e.emit_output_expected_header();
	e.emit_retval("%s", "b");
	if(!(a == "b")) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool sensitive_string_hash_function_non_empty(void) {
	e.emit_test("Test hashFunction() on a non-empty sensitive string.");
	YourSensitiveString a("foo");
	unsigned int hash = YourSensitiveString::hashFunction(a);
	e.emit_input_header();
	e.emit_param("YourSensitiveString", "%s", "foo");
	e.emit_output_expected_header();
	e.emit_retval("%s", "!=0");
	e.emit_output_actual_header();
	e.emit_retval("%d", hash);
	if(hash == 0) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

static bool sensitive_string_hash_function_empty(void) {
	e.emit_test("Test hashFunction() on an empty sensitive string.");
	YourSensitiveString a;
	unsigned int hash = YourSensitiveString::hashFunction(a);
	e.emit_input_header();
	e.emit_param("YourSensitiveString", "%s", "foo");
	e.emit_output_expected_header();
	e.emit_retval("%s", "!=0");
	e.emit_output_actual_header();
	e.emit_retval("%d", hash);
	if(hash == 0) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

