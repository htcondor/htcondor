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
static bool char_constructor(void);
static bool stdstring_constructor(void);
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
static bool concatenation_std_string_non_empty_empty(void);
static bool concatenation_std_string_empty_empty(void);
static bool concatenation_std_string_empty_non_empty(void);
static bool concatenation_std_string_non_empty_non_empty(void);
static bool concatenation_char_empty(void);
static bool concatenation_char_non_empty(void);
static bool concatenation_char_null(void);
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
static bool formatstr_cat_empty(void);
static bool formatstr_cat_non_empty(void);
static bool formatstr_cat_return_true(void);
static bool vformatstr_cat_empty(void);
static bool vformatstr_cat_non_empty(void);
static bool vformatstr_cat_return_true(void);
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
static bool chomp_crlf_end(void);
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
static bool your_string_string_constructor(void);
static bool your_string_equality_true(void);
static bool your_string_equality_false(void);
static bool your_string_equality_default_constructor(void);
static bool your_string_assignment_non_empty_empty(void);
static bool your_string_assignment_empty_non_empty(void);
static bool your_string_assignment_non_empty(void);
static bool test_stl_string_casting(void);

bool OTEST_MyString(void) {
		// beginning junk
	emit_object("MyString");
	emit_comment("The MyString class is a C++ representation of a string. It "
		"was written before we could reliably use the standard string class. "
		"For an example of how to use it, see test_mystring.C. A warning to "
		"anyone changing this code: as currently implemented, an 'empty' "
		"MyString can have two different possible internal representations "
		"depending on its history. Sometimes Data == NULL, sometimes Data[0] =="
		" '\\0'.  So don't assume one or the other...  We should change this to"
		" never having NULL, but there is worry that someone depends on this "
		"behavior.");
	
		// driver to run the tests and all required setup
	FunctionDriver driver;
	driver.register_function(default_constructor);
	driver.register_function(char_constructor);
	driver.register_function(stdstring_constructor);
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
	driver.register_function(concatenation_std_string_non_empty_empty);
	driver.register_function(concatenation_std_string_empty_empty);
	driver.register_function(concatenation_std_string_empty_non_empty);
	driver.register_function(concatenation_std_string_non_empty_non_empty);
	driver.register_function(concatenation_char_empty);
	driver.register_function(concatenation_char_non_empty);
	driver.register_function(concatenation_char_null);
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
	driver.register_function(formatstr_cat_empty);
	driver.register_function(formatstr_cat_non_empty);
	driver.register_function(formatstr_cat_return_true);
	driver.register_function(vformatstr_cat_empty);
	driver.register_function(vformatstr_cat_non_empty);
	driver.register_function(vformatstr_cat_return_true);
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
	driver.register_function(chomp_crlf_end);
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
	driver.register_function(your_string_string_constructor);
	driver.register_function(your_string_equality_true);
	driver.register_function(your_string_equality_false);
	driver.register_function(your_string_equality_default_constructor);
	driver.register_function(your_string_assignment_non_empty_empty);
	driver.register_function(your_string_assignment_empty_non_empty);
	driver.register_function(your_string_assignment_non_empty);
	driver.register_function(test_stl_string_casting);
		// run the tests
	return driver.do_all_functions();
}

static bool default_constructor() {
	emit_test("Test default MyString constructor");
	MyString s;
	emit_output_expected_header();
	emit_retval("");
	emit_output_actual_header();
	emit_retval("%s", s.Value());
	if(strcmp(s.Value(), "") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool char_constructor() {
	emit_test("Test constructor to make a copy of a null-terminated character"
		" string");
	const char *param = "foo";
	MyString s(param);
	emit_input_header();
	emit_param("STRING", "%s", param);
	emit_output_expected_header();
	emit_retval("%s", param);
	emit_output_actual_header();
	emit_retval("%s", s.Value());
	if(strcmp(s.Value(), param) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool stdstring_constructor() {
	emit_test("Test constructor to make a copy of a std::string");
	std::string param = "foo";
	MyString s(param);
	emit_input_header();
	emit_param("STRING", "%s", param.c_str());
	emit_output_expected_header();
	emit_retval("%s", param.c_str());
	emit_output_actual_header();
	emit_retval("%s", s.Value());
	if(strcmp(s.Value(), param.c_str()) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool copy_constructor_value_check() {
	emit_test("Test value of copy constructor");
	const char *param = "foo";
	MyString one(param);
	MyString two(one);
	emit_input_header();
	emit_param("MyString", "%s", two.Value());
	emit_output_expected_header();
	emit_retval("%s", one.Value());
	emit_output_actual_header();
	emit_retval("%s", two.Value());
	if(strcmp(one.Value(), two.Value()) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool copy_constructor_pointer_check() {
	emit_test("Test pointer of copy constructor");
	const char *param = "foo";
	MyString one(param);
	MyString two(one);
	emit_input_header();
	emit_param("MyString", "%s", two.Value());
	emit_output_expected_header();
	emit_retval("!= %p", one.Value());
	emit_output_actual_header();
	emit_retval("%p", two.Value());
	if(one.Value() == two.Value()) {
		FAIL;
	}
	PASS;
}


static bool length_0() {
	emit_test("Length() of MyString of length 0");
	MyString zero;
	emit_output_expected_header();
	emit_retval("%d", 0);
	emit_output_actual_header();
	emit_retval("%d", zero.Length());
	if(zero.Length() != 0) {
		FAIL;
	}
	PASS;
}

//in test_mystring.cpp 94
static bool length_1() {
	emit_test("Length() of MyString of length 1");
	const char *param = "a";
	MyString one(param);
	emit_output_expected_header();
	emit_retval("%d", strlen(param));
	emit_output_actual_header();
	emit_retval("%d", one.Length());
	if(one.Length() != (signed)strlen(param)) {
		FAIL;
	}
	PASS;
}

//in test_mystring.cpp 102
static bool length_2() {
	emit_test("Length() of MyString of length 2");
	const char *param = "ab";
	MyString two(param);
	emit_output_expected_header();
	emit_retval("%d", strlen(param));
	emit_output_actual_header();
	emit_retval("%d", two.Length());
	if(two.Length() != (signed)strlen(param)) {
		FAIL;
	}
	PASS;
}

static bool length_after_change() {
	emit_test("Length() of MyString after being assigned to an empty "
		"MyString");
	MyString long_string("longer");
	MyString empty;
	long_string = empty;
	emit_output_expected_header();
	emit_retval("%d", empty.Length());
	emit_output_actual_header();
	emit_retval("%d", long_string.Length());
	if(long_string.Length() != empty.Length()) {
		FAIL;
	}
	PASS;
}

static bool is_empty_initialized() {
	emit_test("IsEmpty() of an empty MyString");
	MyString empty;
	emit_output_expected_header();
	emit_retval("%d", 1);
	emit_output_actual_header();
	emit_retval("%d", empty.IsEmpty());
	if(!empty.IsEmpty()) {
		FAIL;
	}
	PASS;
}

static bool is_empty_after_change() {
	emit_test("IsEmpty() of a MyString that is changed to an empty "
		"MyString");
	MyString will_be_empty("notEmpty");
	will_be_empty = "";
	emit_output_expected_header();
	emit_retval("%d", 1);
	emit_output_actual_header();
	emit_retval("%d", will_be_empty.IsEmpty());
	if(!will_be_empty.IsEmpty()) {
		FAIL;
	}
	PASS;
}

static bool is_empty_false() {
	emit_test("IsEmpty() of a non-empty MyString");
	MyString not_empty("a");
	emit_output_expected_header();
	emit_retval("%d", 0);
	emit_output_actual_header();
	emit_retval("%d", not_empty.IsEmpty());
	if(not_empty.IsEmpty()) {
		FAIL;
	}
	PASS;
}

static bool capacity_empty() {
	emit_test("Capacity() of an empty MyString");
	emit_comment("This is the way the code works in init()."
		"This will fail if init() changes capacity in the future!");
	MyString empty;
	int cap = 0;
	emit_output_expected_header();
	emit_retval("%d", cap);
	emit_output_actual_header();
	emit_retval("%d", empty.Capacity());
	if(empty.Capacity() != cap) {
		FAIL;
	}
	PASS;
}

static bool capacity_greater_than_length() {
	emit_test("Is the capacity of a non-empty MyString greater or equal to "
		"its length?");
	MyString s("foo");
	emit_output_expected_header();
	emit_param("Capacity", ">=3");
	emit_param("Length", "3");
	emit_retval("%d", 1);
	emit_output_actual_header();
	emit_param("Capicity", "%d", s.Capacity());
	emit_param("Length", "%d", s.Length());
	emit_retval("%d", s.Capacity() >= s.Length());
	if(s.Capacity() < s.Length()) {
		FAIL;
	}
	PASS;
}

static bool str_dup_empty() {
	emit_test("Does StrDup() return the empty string when called on an empty"
		" MyString?");
	MyString empty;
	char* dup = empty.StrDup();
	emit_output_expected_header();
	emit_retval("");
	emit_output_actual_header();
	emit_retval("%s", dup);
	if(strcmp(dup, "") != MATCH) {
		free(dup);
		FAIL;
	}
	free(dup);
	PASS;
}

static bool str_dup_empty_new() {
	emit_test("Does StrDup() return a newly created empty string when called "
		"on an empty MyString?");
	MyString empty;
	char* dup = empty.StrDup();
	emit_output_expected_header();
	emit_retval("");
	emit_output_actual_header();
	emit_retval("%s", dup);
	if(empty.Value() == dup) {
		free(dup);
		FAIL;
	}
	free(dup);
	PASS;
}

static bool str_dup_non_empty() {
	emit_test("Does StrDup() return the equivalent C string when called on a"
		" non-empty MyString?");
	MyString s("foo");
	char* dup = s.StrDup();
	emit_output_expected_header();
	emit_retval("");
	emit_output_actual_header();
	emit_retval("%s", dup);
	if(strcmp(dup, "foo") != MATCH) {
		free(dup);
		FAIL;
	}
	free(dup);
	PASS;
}

static bool str_dup_non_empty_new() {
	emit_test("Does StrDup() return a newly created equivalent C string when "
		"called on a non-empty MyString?");
	MyString s("foo");
	char* dup = s.StrDup();
	emit_output_expected_header();
	emit_retval("");
	emit_output_actual_header();
	emit_retval("%s", dup);
	if(s.Value() == dup) {
		free(dup);
		FAIL;
	}
	free(dup);
	PASS;
}

static bool value_of_empty() {
	emit_test("Does Value() return the empty string for an empty MyString?");
	MyString empty;
	const char* value = empty.Value();
	emit_output_expected_header();
	emit_retval("");
	emit_output_actual_header();
	emit_retval("%s", value);
	if(strcmp(value, "") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool value_of_non_empty() {
	emit_test("Does Value() return the correct string for a non-empty "
		"MyString?");
	const char* param = "foo";
	MyString s(param);
	const char* value = s.Value();
	emit_output_expected_header();
	emit_retval("%s", param);
	emit_output_actual_header();
	emit_retval("%s", value);
	if(strcmp(value, param) != MATCH) {
		FAIL;
	}
	PASS;
}

//in test_mystring.cpp 369
static bool square_brackets_operator_invalid_negative() {
	emit_test("Does the [] operator return '\\0' if passed a negative pos?");
	MyString s("foo");
	char invalid = s[-1];
	emit_input_header();
	emit_param("pos", "%d", -1);
	emit_output_expected_header();
	emit_retval("%c", '\0');
	emit_output_actual_header();
	emit_retval("%c", invalid);
	if(invalid != '\0') {
		FAIL;
	}
	PASS;
}

//in test_mystring.cpp 366
static bool square_brackets_operator_invalid_empty() {
	emit_test("Does the [] operator return '\\0' if accessing the first char "
		"of an empty MyString?");
	MyString empty;
	char invalid = empty[0];
	emit_input_header();
	emit_param("pos", "%d", 0);
	emit_output_expected_header();
	emit_retval("%c", '\0');
	emit_output_actual_header();
	emit_retval("%c", invalid);
	if(invalid != '\0') {
		FAIL;
	}
	PASS;
}

static bool square_brackets_operator_invalid_past() {
	emit_test("Does the [] operator return '\\0' if accessing one char past "
		"the end of the MyString?");
	MyString s("foo");
	char invalid = s[3];
	emit_input_header();
	emit_param("pos", "%d", 3);
	emit_output_expected_header();
	emit_retval("%c", '\0');
	emit_output_actual_header();
	emit_retval("%c", invalid);
	if(invalid != '\0') {
		FAIL;
	}
	PASS;
}

static bool square_brackets_operator_first_char() {
	emit_test("[] operator on the first char of a MySting");
	MyString s("foo");
	char valid = s[0];
	emit_input_header();
	emit_param("pos", "%d", 0);
	emit_output_expected_header();
	emit_retval("%c", 'f');
	emit_output_actual_header();
	emit_retval("%c", valid);
	if(valid != 'f') {
		FAIL;
	}
	PASS;
}

static bool square_brackets_operator_last_char() {
	emit_test("[] operator on the last char of a MySting");
	MyString s("foo");
	char valid = s[2];
	emit_input_header();
	emit_param("pos", "%d", 2);
	emit_output_expected_header();
	emit_retval("%c", 'o');
	emit_output_actual_header();
	emit_retval("%c", valid);
	if(valid != 'o') {
		FAIL;
	}
	PASS;
}

//in test_mystring.cpp 391
static bool set_char_empty() {
	emit_test("Does an empty MyString stay the same after calling setChar()");
	MyString empty;
	empty.setAt(0, 'a');
	emit_input_header();
	emit_param("pos", "%d", 0);
	emit_param("value", "%c", 'a');
	emit_output_expected_header();
	emit_param("Value", "%s", "");
	emit_param("Length", "%d", 0);
	emit_output_actual_header();
	emit_param("Value", "%s", empty.Value());
	emit_param("Length", "%d", empty.Length());
	if(strcmp(empty.Value(), "") != MATCH || empty.Length() != 0) {
		FAIL;
	}
	PASS;
}

//in test_mystring.cpp 423
static bool set_char_truncate() {
	emit_test("Does calling setChar() with '\\0' truncate the original "
		"MyString");
	MyString s("Eddy Merckx");
	s.truncate(5);
	emit_input_header();
	emit_param("MyString", "%s", s.Value());
	emit_param("pos", "%d", 5);
	emit_param("value", "\\0");
	emit_output_expected_header();
	emit_param("Value", "Eddy ");
	emit_param("Length", "%d", 5);
	emit_output_actual_header();
	emit_param("Value", "%s", s.Value());
	emit_param("Length", "%d", s.Length());
	if(strcmp(s.Value(), "Eddy ") != MATCH || s.Length() != 5) {
		FAIL;
	}
	PASS;
}

static bool set_char_first() {
	emit_test("setChar() on the first char of a MyString");
	MyString s("foo");
	s.setAt(0, 't');
	emit_input_header();
	emit_param("MyString", "%s", s.Value());
	emit_param("pos", "%d", 0);
	emit_param("value", "%c", 't');
	emit_output_expected_header();
	emit_param("Value", "too");
	emit_param("Length", "%d", 3);
	emit_output_actual_header();
	emit_param("Value", "%s", s.Value());
	emit_param("Length", "%d", s.Length());
	if(strcmp(s.Value(), "too") != MATCH || s.Length() != 3) {
		FAIL;
	}
	PASS;
}

static bool set_char_last() {
	emit_test("setChar() on the last char of a MyString");
	MyString s("foo");
	s.setAt(2, 'r');
	emit_input_header();
	emit_param("MyString", "%s", s.Value());
	emit_param("pos", "%d", 2);
	emit_param("value", "%c", 'r');
	emit_output_expected_header();
	emit_param("Value", "for");
	emit_param("Length", "%d", 3);
	emit_output_actual_header();
	emit_param("Value", "%s", s.Value());
	emit_param("Length", "%d", s.Length());
	if(strcmp(s.Value(), "for") != MATCH || s.Length() != 3) {
		FAIL;
	}
	PASS;
}

//in test_mystring.cpp 578
static bool random_string_one_char() {
	emit_test("Does randomlyGenerateInsecure() work when passed only one char?");
	std::string rnd;
	randomlyGenerateInsecure(rnd, "1", 5);
	emit_input_header();
	emit_param("set", "%s", "1");
	emit_param("len", "%d", 5);
	emit_output_expected_header();
	emit_retval("%s", "11111");
	emit_output_actual_header();
	emit_retval("%s", rnd.c_str());
	if(strcmp(rnd.c_str(), "11111") != MATCH) {
		FAIL;
	}
	PASS;
}

//in test_mystring.cpp 586
static bool random_string_many_chars() {
	emit_test("Does randomlyGenerateInsecure() work when passed a 10 char set?");
	std::string rnd;
	randomlyGenerateInsecure(rnd, "0123456789", 64);
	emit_input_header();
	emit_param("set", "%s", "0123456789");
	emit_param("len", "%d", 64);
	emit_output_expected_header();
	emit_param("Length", "%d", 64);
	emit_output_actual_header();
	emit_param("Length", "%d", rnd.length());
	if(rnd.length() != 64) {
		FAIL;
	}
	PASS;
}

static bool random_string_existing() {
	emit_test("Does randomlyGenerateInsecure() work with a non empty string?");
	std::string rnd("foo");
	randomlyGenerateInsecure(rnd, "0123456789", 64);
	emit_input_header();
	emit_param("set", "%s", "0123456789");
	emit_param("len", "%d", 64);
	emit_output_expected_header();
	emit_param("Length", "%d", 64);
	emit_output_actual_header();
	emit_param("Length", "%d", rnd.length());
	if(rnd.length() != 64) {
		FAIL;
	}
	PASS;
}

static bool random_string_empty_set() {
	emit_test("Does randomlyGenerateInsecure() work when passed an empty set when "
		"called with an empty string?");
	std::string rnd;
	randomlyGenerateInsecure(rnd, NULL, 64);
	emit_input_header();
	emit_param("set", "%s", "");
	emit_param("len", "%d", 64);
	emit_output_expected_header();
	emit_retval("%s", "");
	emit_output_actual_header();
	emit_retval("%s", rnd.c_str());
	if(strcmp(rnd.c_str(), "") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool random_string_empty_set_existing() {
	emit_test("Does randomlyGenerateInsecure() work when passed an empty set when "
		"called with a non empty string?");
	std::string rnd("foo");
	randomlyGenerateInsecure(rnd, NULL, 64);
	emit_input_header();
	emit_param("set", "%s", "");
	emit_param("len", "%d", 64);
	emit_output_expected_header();
	emit_retval("%s", "");
	emit_output_actual_header();
	emit_retval("%s", rnd.c_str());
	if(strcmp(rnd.c_str(), "") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool random_string_length_zero() {
	emit_test("Does randomlyGenerateInsecure() work when passed a length of zero?");
	std::string rnd;
	randomlyGenerateInsecure(rnd, "0123", 0);
	emit_input_header();
	emit_param("set", "%s", "0123");
	emit_param("len", "%d", 0);
	emit_output_expected_header();
	emit_retval("%s", "");
	emit_output_actual_header();
	emit_retval("%s", rnd.c_str());
	if(strcmp(rnd.c_str(), "") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool random_string_negative_length() {
	emit_test("Does randomlyGenerate() work when passed a negative length?");
	std::string rnd;
	randomlyGenerateInsecure(rnd, "0123", -1);
	emit_input_header();
	emit_param("set", "%s", "0123");
	emit_param("len", "%d", -1);
	emit_output_expected_header();
	emit_retval("%s", "");
	emit_output_actual_header();
	emit_retval("%s", rnd.c_str());
	if(strcmp(rnd.c_str(), "") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool random_string_hex() {
	emit_test("Does randomlyGenerateInsecureHex() modify the string to the given "
		"length?");
	std::string rnd;
	randomlyGenerateInsecureHex(rnd, 10);
	emit_input_header();
	emit_param("len", "%d", 10);
	emit_output_expected_header();
	emit_param("Length", "%d", 10);
	emit_output_actual_header();
	emit_param("Length", "%d", rnd.length());
	if(rnd.length() != 10) {
		FAIL;
	}
	PASS;
}

static bool equals_operator_my_string_empty_non_empty() {
	emit_test("Does the = operator work correctly when an empty MyString gets"
		" assigned a non empty MyString?");
	MyString a;
	MyString b("bar");
	a = b;
	emit_input_header();
	emit_param("MyString", "%s", b.Value());
	emit_output_expected_header();
	emit_retval("%s", b.Value());
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "bar") != MATCH || a.Value() == b.Value()) {
		FAIL;
	}
	PASS;
}

//in test_mystring.cpp 412
static bool equals_operator_my_string_non_empty_non_empty() {
	emit_test("Does the = operator work correctly when a non empty MyString "
		"gets assigned another non empty MyString?");
	MyString a("Bernard Hinault");
	MyString b("Laurent Jalabert");
	a = b;
	emit_input_header();
	emit_param("MyString", "%s", b.Value());
	emit_output_expected_header();
	emit_retval("%s", b.Value());
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "Laurent Jalabert") != MATCH || a.Value() == b.Value())
	{
		FAIL;
	}
	PASS;
}

static bool equals_operator_my_string_non_empty_empty() {
	emit_test("Does the = operator work correctly when a non empty MyString "
		"gets assigned an empty MyString?");
	MyString a("foo");
	MyString b;
	a = b;
	emit_input_header();
	emit_param("MyString", "%s", b.Value());
	emit_output_expected_header();
	emit_retval("%s", b.Value());
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "") != MATCH || a.Value() == b.Value()) {
		FAIL;
	}
	PASS;
}

//in test_mystring.cpp 114
static bool equals_operator_string_empty_non_empty() {
	emit_test("Does the = operator work correctly when an empty MyString gets"
		" assigned a non empty string?");
	MyString a;
	const char* b = "bar";
	a = b;
	emit_input_header();
	emit_param("STRING", "%s", b);
	emit_output_expected_header();
	emit_retval("%s", "bar");
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "bar") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool equals_operator_string_non_empty_non_empty() {
	emit_test("Does the = operator work correctly when a non empty MyString "
		"gets assigned a non empty string?");
	MyString a("foo");
	const char* b = "bar";
	a = b;
	emit_input_header();
	emit_param("STRING", "%s", b);
	emit_output_expected_header();
	emit_retval("%s", b);
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "bar") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool equals_operator_string_non_empty_empty() {
	emit_test("Does the = operator work correctly when a non empty MyString "
		"gets assigned an empty string?");
	MyString a("foo");
	const char* b = "";
	a = b;
	emit_input_header();
	emit_param("STRING", "%s", b);
	emit_output_expected_header();
	emit_retval("%s", b);
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "") != MATCH) {
		FAIL;
	}
	PASS;
}

//in test_mystring.cpp 112
static bool equals_operator_string_null() {
	emit_test("Does the = operator work when a MyString gets assigned to a "
		"Null string?");
	MyString a;
	a = NULL;
	emit_input_header();
	emit_param("STRING", "%s", "NULL");
	emit_output_expected_header();
	emit_retval("%s", "");
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "") != MATCH) {
		FAIL;
	}
	PASS;
}

//in test_mystring.cpp 292
static bool reserve_invalid_negative() {
	emit_test("Does reserve() return false when passed a negative size?");
	MyString a;
	bool retval = a.reserve(-3);
	emit_input_header();
	emit_param("INT", "%d", -3);
	emit_output_expected_header();
	emit_retval("%d", 0);
	emit_output_actual_header();
	emit_retval("%d", retval);
	if(retval) {
		FAIL;
	}
	PASS;
}

//in test_mystring.cpp 302
static bool reserve_shortening_1() {
	emit_test("Does reserve() return true when passed a size smaller than the"
		" MyString's length?");
	MyString a("12345");
	bool retval = a.reserve(1);
	emit_input_header();
	emit_param("INT", "%d", 1);
	emit_output_expected_header();
	emit_retval("%d", 1);
	emit_output_actual_header();
	emit_retval("%d", retval);
	if(!retval) {
		FAIL;
	}
	PASS;
}

//in test_mystring.cpp 325
static bool reserve_shortening_0() {
	emit_test("Does reserve() return true when passed a size of 0?");
	MyString a("12345");
	bool retval = a.reserve(0);
	emit_input_header();
	emit_param("INT", "%d", 0);
	emit_output_expected_header();
	emit_retval("%d", 1);
	emit_output_actual_header();
	emit_retval("%d", retval);
	if(!retval) {
		FAIL;
	}
	PASS;
}

//in test_mystring.cpp 434
static bool reserve_shortening_smaller() {
	emit_test("Does reserve() correctly not shorten a MyString when passed a size"
		" smaller than the length of the MyString?");
	MyString a("Miguel Indurain");
	a.reserve(6);
	emit_input_header();
	emit_param("INT", "%d", 0);
	emit_output_expected_header();
	emit_param("MyString", "%s", "Miguel");
	emit_param("Length", "%d", 6);
	emit_output_actual_header();
	emit_param("MyString", "%s", a.Value());
	emit_param("Length", "%d", a.Length());
	if(strcmp(a.Value(), "Miguel Indurain") != MATCH || a.Length() != 15) {
		FAIL;
	}
	PASS;
}

static bool reserve_no_changes() {
	emit_test("Does reserve() preserve the original MyString when passed a "
		"size greater than the MyString's length?");
	MyString a("foo");
	bool retval = a.reserve(100);
	emit_input_header();
	emit_param("INT", "%d", 100);
	emit_output_expected_header();
	emit_param("MyString", "%s", "foo");
	emit_retval("%d", 1);
	emit_output_actual_header();
	emit_param("MyString", "%s", a.Value());
	emit_retval("%d", retval);
	if(!retval || strcmp(a.Value(), "foo") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool reserve_empty_my_string() {
	emit_test("Does reserve() return true when used on an empty MyString?");
	MyString a;
	bool retval = a.reserve(100);
	emit_input_header();
	emit_param("INT", "%d", 100);
	emit_output_expected_header();
	emit_retval("%d", 1);
	emit_output_actual_header();
	emit_retval("%d", retval);
	if(!retval) {
		FAIL;
	}
	PASS;
}

static bool reserve_larger_capacity() {
	emit_test("Does reserve() increase the capacity of a MyString when passed"
		" a larger size");
	MyString a;
	int size = 100;
	a.reserve(size);
	emit_input_header();
	emit_param("INT", "%d", size);
	emit_output_expected_header();
	emit_param("Capacity", "%d", size);
	emit_output_actual_header();
	emit_param("Capacity", "%d", a.Capacity());
	if(a.Capacity() != size) {
		FAIL;
	}
	PASS;
}

static bool reserve_smaller_capacity() {
	emit_test("Does reverse() decrease the capacity of a MyString when passed"
		" a smaller size?");
	MyString a("foobarfoobarfoobarfoobarfoobarfoobar");
	int size = 10;
	a.truncate(6);
	a.reserve(size);
	emit_input_header();
	emit_param("INT", "%d", size);
	emit_output_expected_header();
	emit_param("Capacity", "%d", size);
	emit_output_actual_header();
	emit_param("Capacity", "%d", a.Capacity());
	if(a.Capacity() != size) {
		FAIL;
	}
	PASS;
}

//in test_mystring.cpp 348
static bool reserve_at_least_negative() {
	emit_test("Does reverse_at_least() return true when passed a negative "
		"size?");
	MyString a("12345");
	bool retval = a.reserve_at_least(-1);
	emit_input_header();
	emit_param("INT", "%d", -1);
	emit_output_expected_header();
	emit_retval("%d", 1);
	emit_output_actual_header();
	emit_retval("%d", retval);
	if(!retval) {
		FAIL;
	}
	PASS;
}

//in test_mystring.cpp 357
static bool reserve_at_least_zero() {
	emit_test("Does reverse_at_least() return true when passed a size of 0?");
	MyString a("12345");
	bool retval = a.reserve_at_least(0);
	emit_input_header();
	emit_param("INT", "%d", 0);
	emit_output_expected_header();
	emit_retval("%d", 1);
	emit_output_actual_header();
	emit_retval("%d", retval);
	if(!retval) {
		FAIL;
	}
	PASS;
}

//in test_mystring.cpp 447
static bool reserve_at_least_no_change() {
	emit_test("Does reverse_at_least() return preserve the original MyString "
		"when passed a size greater than the MyString's length?");
	MyString a("Jacques Anquetil");
	a.reserve_at_least(100);
	emit_input_header();
	emit_param("INT", "%d", 0);
	emit_output_expected_header();
	emit_param("MyString", "%s", "Jacques Anquetil");
	emit_output_actual_header();
	emit_param("MyString", "%s", a.Value());
	if(strcmp(a.Value(), "Jacques Anquetil") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool reserve_at_least_capacity() {
	emit_test("Does reverse_at_least() change the capacity when pased a size "
		"greater than the MyString's length?");
	MyString a("Jacques Anquetil");
	a.reserve_at_least(100);
	emit_input_header();
	emit_param("INT", "%d", 0);
	emit_output_expected_header();
	emit_param("Capacity", "%s", ">= 100");
	emit_output_actual_header();
	emit_param("MyString", "%d", a.Capacity());
	if(a.Capacity() < 100) {
		FAIL;
	}
	PASS;
}

//in test_mystring.cpp 125
static bool concatenation_my_string_non_empty_empty() {
	emit_test("Test concatenating a non-empty MyString with an empty "
		"MyString.");
	MyString a("foo");
	MyString b;
	a += b;
	emit_input_header();
	emit_param("MyString", "%s", b.Value());
	emit_output_expected_header();
	emit_retval("%s", "foo");
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "foo") != MATCH) {
		FAIL;
	}
	PASS;
}

//in test_mystring.cpp 400
static bool concatenation_my_string_empty_empty() {
	emit_test("Test concatenating a empty MyString with another empty "
		"MyString.");
	MyString a;
	MyString b;
	a += b;
	emit_input_header();
	emit_param("MyString", "%s", b.Value());
	emit_output_expected_header();
	emit_retval("%s", "");
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool concatenation_my_string_empty_non_empty() {
	emit_test("Test concatenating a empty MyString with a non-empty "
		"MyString.");
	MyString a;
	MyString b("foo");
	a += b;
	emit_input_header();
	emit_param("MyString", "%s", b.Value());
	emit_output_expected_header();
	emit_retval("%s", "foo");
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "foo") != MATCH) {
		FAIL;
	}
	PASS;
}

//in test_mystring.cpp 144
static bool concatenation_my_string_non_empty_non_empty() {
	emit_test("Test concatenating a non-empty MyString with another non-empty"
		" MyString.");
	MyString a("foo");
	MyString b("bar");
	a += b;
	emit_input_header();
	emit_param("MyString", "%s", b.Value());
	emit_output_expected_header();
	emit_retval("%s", "foobar");
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "foobar") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool concatenation_my_string_multiple() {
	emit_test("Test concatenating an empty MyString with two other non-empty "
		"MyStrings.");
	MyString a("Lance");
	MyString b(" Armstrong");
	MyString c = a + b;
	emit_input_header();
	emit_param("MyString", "%s", a.Value());
	emit_param("MyString", "%s", b.Value());
	emit_output_expected_header();
	emit_retval("%s", "Lance Armstrong");
	emit_output_actual_header();
	emit_retval("%s", c.Value());
	if(strcmp(c.Value(), "Lance Armstrong") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool concatenation_my_string_itself() {
	emit_test("Test concatenating a non-empty MyString with itself.");
	emit_comment("See ticket #1290");
	MyString a("foo");
	a += a;
	emit_input_header();
	emit_param("MyString", "%s", "foo");
	emit_output_expected_header();
	emit_retval("%s", "foofoo");
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "foofoo") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool concatenation_my_string_itself_empty() {
	emit_test("Test concatenating an empty MyString with itself.");
	MyString a;
	a += a;
	emit_input_header();
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_retval("%s", "");
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool concatenation_string_non_empty_null() {
	emit_test("Test concatenating a non-empty MyString with a NULL string.");
	MyString a("foo");
	a += (char *)NULL;
	emit_input_header();
	emit_param("STRING", "%s", "NULL");
	emit_output_expected_header();
	emit_retval("%s", "foo");
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "foo") != MATCH) {
		FAIL;
	}
	PASS;
}

//in test_mystring.cpp 123
static bool concatenation_string_empty_empty() {
	emit_test("Test concatenating an empty MyString with a NULL string.");
	MyString a;
	a += (char *)NULL;
	emit_input_header();
	emit_param("STRING", "%s", "NULL");
	emit_output_expected_header();
	emit_retval("%s", "");
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool concatenation_string_empty_non_empty() {
	emit_test("Test concatenating an empty MyString with a non-empty "
		"string.");
	MyString a;
	const char* b = "foo";
	a += b;
	emit_input_header();
	emit_param("STRING", "%s", b);
	emit_output_expected_header();
	emit_retval("%s", "foo");
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "foo") != MATCH) {
		FAIL;
	}
	PASS;
}

//in test_mystring.cpp 122
static bool concatenation_string_non_empty_non_empty() {
	emit_test("Test concatenating a non-empty MyString with a non-empty "
		"string.");
	MyString a("blah");
	const char* b = "baz";
	a += b;
	emit_input_header();
	emit_param("STRING", "%s", b);
	emit_output_expected_header();
	emit_retval("%s", "blahbaz");
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "blahbaz") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool concatenation_std_string_non_empty_empty() {
	emit_test("Test concatenating a non-empty MyString with an empty "
		"std::string.");
	MyString a("foo");
	std::string b;
	a += b;
	emit_input_header();
	emit_param("MyString", "%s", b.c_str());
	emit_output_expected_header();
	emit_retval("%s", "foo");
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "foo") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool concatenation_std_string_empty_empty() {
	emit_test("Test concatenating a empty MyString with an empty "
		"std::string.");
	MyString a;
	std::string b;
	a += b;
	emit_input_header();
	emit_param("MyString", "%s", b.c_str());
	emit_output_expected_header();
	emit_retval("%s", "");
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool concatenation_std_string_empty_non_empty() {
	emit_test("Test concatenating a empty MyString with a non-empty "
		"std::string.");
	MyString a;
	std::string b("foo");
	a += b;
	emit_input_header();
	emit_param("MyString", "%s", b.c_str());
	emit_output_expected_header();
	emit_retval("%s", "foo");
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "foo") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool concatenation_std_string_non_empty_non_empty() {
	emit_test("Test concatenating a non-empty MyString with a non-empty"
		" std::string.");
	MyString a("foo");
	std::string b("bar");
	a += b;
	emit_input_header();
	emit_param("MyString", "%s", b.c_str());
	emit_output_expected_header();
	emit_retval("%s", "foobar");
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "foobar") != MATCH) {
		FAIL;
	}
	PASS;
}

//in test_mystring.cpp 135
static bool concatenation_char_empty() {
	emit_test("Test concatenating an empty MyString with a character.");
	MyString a;
	a += '!';
	emit_input_header();
	emit_param("CHAR", "%c", '!');
	emit_output_expected_header();
	emit_retval("%c", '!');
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "!") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool concatenation_char_non_empty() {
	emit_test("Test concatenating a non-empty MyString with a character.");
	MyString a("pow");
	a += '!';
	emit_input_header();
	emit_param("CHAR", "%c", '!');
	emit_output_expected_header();
	emit_retval("%s", "pow!");
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "pow!") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool concatenation_char_null() {
	emit_test("Test concatenating a non-empty MyString with the NULL "
		"character.");
	emit_problem("Concatenation works, but the length is still incremented!");
	MyString a("pow");
	a += '\0';
	emit_input_header();
	emit_param("CHAR", "%c", '!');
	emit_output_expected_header();
	emit_retval("%s", "pow");
	emit_param("Length", "%d", 3);
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	emit_param("Length", "%d", a.Length());
	if(strcmp(a.Value(), "pow") != MATCH) {
		FAIL;
	}
	PASS;
}

//in test_mystring.cpp 379
static bool substr_empty() {
	emit_test("Test substr() on an empty MyString.");
	MyString a;
	MyString b = a.substr(0, 5);
	emit_input_header();
	emit_param("Pos1", "%d", 0);
	emit_param("Pos2", "%d", 1);
	emit_output_expected_header();
	emit_retval("%s", "");
	emit_output_actual_header();
	emit_retval("%s", b.Value());
	if(strcmp(b.Value(), "") != MATCH) {
		FAIL;
	}
	PASS;
}

//in test_mystring.cpp 191
static bool substr_beginning() {
	emit_test("Test substr() on the beginning of a MyString.");
	MyString a("blahbaz!");
	MyString b = a.substr(0, 4);
	emit_input_header();
	emit_param("Pos1", "%d", 0);
	emit_param("Pos2", "%d", 4);
	emit_output_expected_header();
	emit_retval("%s", "blah");
	emit_output_actual_header();
	emit_retval("%s", b.Value());
	if(strcmp(b.Value(), "blah") != MATCH) {
		FAIL;
	}
	PASS;
}

//in test_mystring.cpp 200
static bool substr_end() {
	emit_test("Test substr() on the end of a MyString.");
	MyString a("blahbaz!");
	MyString b = a.substr(4, 4);
	emit_input_header();
	emit_param("Pos1", "%d", 4);
	emit_param("Pos2", "%d", 4);
	emit_output_expected_header();
	emit_retval("%s", "baz!");
	emit_output_actual_header();
	emit_retval("%s", b.Value());
	if(strcmp(b.Value(), "baz!") != MATCH) {
		FAIL;
	}
	PASS;
}

//in test_mystring.cpp 218
static bool substr_outside_beginning() {
	emit_test("Test substr() when passed a position before the beginning of "
		"the MyString.");
	MyString a("blahbaz!");
	MyString b = a.substr(-2, 6);
	emit_input_header();
	emit_param("Pos1", "%d", -2);
	emit_param("Pos2", "%d", 6);
	emit_output_expected_header();
	emit_retval("%s", "blahba");
	emit_output_actual_header();
	emit_retval("%s", b.Value());
	if(strcmp(b.Value(), "blahba") != MATCH) {
		FAIL;
	}
	PASS;
}

//in test_mystring.cpp 209
static bool substr_outside_end() {
	emit_test("Test substr() when passed a position after the end of the "
		"MyString.");
	MyString a("blahbaz!");
	MyString b = a.substr(5, 10);
	emit_input_header();
	emit_param("Pos1", "%d", 5);
	emit_param("Pos2", "%d", 10);
	emit_output_expected_header();
	emit_retval("%s", "az!");
	emit_output_actual_header();
	emit_retval("%s", b.Value());
	if(strcmp(b.Value(), "az!") != MATCH) {
		FAIL;
	}
	PASS;
}

//in test_mystring.cpp 230
static bool escape_chars_length_1() {
	emit_test("Test EscapeChars() on a MyString of length 1.");
	MyString a("z");
	MyString b = a.EscapeChars("z", '\\');
	emit_input_header();
	emit_param("STRING", "%s", "z");
	emit_param("CHAR", "%c", '\\');
	emit_output_expected_header();
	emit_retval("%s", "\\z");
	emit_output_actual_header();
	emit_retval("%s", b.Value());
	if(strcmp(b.Value(), "\\z") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool escape_chars_empty() {
	emit_test("Test EscapeChars() on an empty MyString.");
	MyString a;
	MyString b = a.EscapeChars("z", '\\');
	emit_input_header();
	emit_param("STRING", "%s", "z");
	emit_param("CHAR", "%c", '\\');
	emit_output_expected_header();
	emit_retval("%s", "");
	emit_output_actual_header();
	emit_retval("%s", b.Value());
	if(strcmp(b.Value(), "") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool escape_chars_multiple() {
	emit_test("Test EscapeChars() when passed many escape chars.");
	MyString a("foobar");
	MyString b = a.EscapeChars("oa", '*');
	emit_input_header();
	emit_param("STRING", "%s", "oa");
	emit_param("CHAR", "%c", '*');
	emit_output_expected_header();
	emit_retval("%s", "f*o*ob*ar");
	emit_output_actual_header();
	emit_retval("%s", b.Value());
	if(strcmp(b.Value(), "f*o*ob*ar") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool escape_chars_same() {
	emit_test("Test EscapeChars() when passed the same string as escape "
		"chars.");
	MyString a("foobar");
	MyString b = a.EscapeChars("foobar", '*');
	emit_input_header();
	emit_param("STRING", "%s", "foobar");
	emit_param("CHAR", "%c", '*');
	emit_output_expected_header();
	emit_retval("%s", "*f*o*o*b*a*r");
	emit_output_actual_header();
	emit_retval("%s", b.Value());
	if(strcmp(b.Value(), "*f*o*o*b*a*r") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool find_char_first() {
	emit_test("Does FindChar() find the char when its at the beginning of the"
		" MyString?");
	MyString a("foo");
	int pos = a.FindChar('f');
	emit_input_header();
	emit_param("Char", "%c", 'f');
	emit_output_expected_header();
	emit_retval("%d", 0);
	emit_output_actual_header();
	emit_retval("%d", pos);
	if(pos != 0) {
		FAIL;
	}
	PASS;
}

static bool find_char_last() {
	emit_test("Does FindChar() find the char when it first appears at the end"
		" of the MyString?");
	MyString a("bar");
	int pos = a.FindChar('r');
	emit_input_header();
	emit_param("Char", "%c", 'r');
	emit_output_expected_header();
	emit_retval("%d", 2);
	emit_output_actual_header();
	emit_retval("%d", pos);
	if(pos != 2) {
		FAIL;
	}
	PASS;
}

static bool find_char_not_found() {
	emit_test("Does FindChar() return -1 when the char isn't found in the "
		"MyString?");
	MyString a("bar");
	int pos = a.FindChar('t');
	emit_input_header();
	emit_param("Char", "%c", 't');
	emit_output_expected_header();
	emit_retval("%d", -1);
	emit_output_actual_header();
	emit_retval("%d", pos);
	if(pos != -1) {
		FAIL;
	}
	PASS;
}

static bool find_char_invalid_greater() {
	emit_test("Does FindChar() return -1 when the first position is greater "
		"than the length of the MyString?");
	MyString a("bar");
	int pos = a.FindChar('b', 10);
	emit_input_header();
	emit_param("Char", "%c", 'b');
	emit_param("FirstPos", "%d", 10);
	emit_output_expected_header();
	emit_retval("%d", -1);
	emit_output_actual_header();
	emit_retval("%d", pos);
	if(pos != -1) {
		FAIL;
	}
	PASS;
}

static bool find_char_invalid_less() {
	emit_test("Does FindChar() return -1 when the first position is less than"
		" 0?");
	MyString a("bar");
	int pos = a.FindChar('b',-10);
	emit_input_header();
	emit_param("Char", "%c", 'b');
	emit_param("FirstPos", "%d", -10);
	emit_output_expected_header();
	emit_retval("%d", -1);
	emit_output_actual_header();
	emit_retval("%d", pos);
	if(pos != -1) {
		FAIL;
	}
	PASS;
}

//in test_mystring.cpp 598
static bool find_empty() {
	emit_test("Does find() return 0 when looking for the empty string within"
		" an empty MyString?");
	MyString a;
	int loc = a.find("");
	emit_input_header();
	emit_param("STRING", "%s", "");
	emit_output_expected_header();
	emit_retval("%d", 0);
	emit_output_actual_header();
	emit_retval("%d", loc);
	if(loc != 0) {
		FAIL;
	}
	PASS;
}

//in test_mystring.cpp 607
static bool find_non_empty() {
	emit_test("Does find() return 0 when looking for the empty string within "
		"a non-empty MyString?");
	MyString a("Alberto Contador");
	int loc = a.find("");
	emit_input_header();
	emit_param("STRING", "%s", "");
	emit_output_expected_header();
	emit_retval("%d", 0);
	emit_output_actual_header();
	emit_retval("%d", loc);
	if(loc != 0) {
		FAIL;
	}
	PASS;
}

//in test_mystring.cpp 614
static bool find_beginning() {
	emit_test("Does find() return 0 when looking for a substring that is at "
		"the beginning of the MyString?");
	MyString a("Alberto Contador");
	int loc = a.find("Alberto");
	emit_input_header();
	emit_param("STRING", "%s", "Alberto");
	emit_output_expected_header();
	emit_retval("%d", 0);
	emit_output_actual_header();
	emit_retval("%d", loc);
	if(loc != 0) {
		FAIL;
	}
	PASS;
}

//in test_mystring.cpp 621
static bool find_middle() {
	emit_test("Does find() return the correct position when looking for a "
		"substring that is in the middle of the MyString?");
	MyString a("Alberto Contador");
	int loc = a.find("to Cont");
	emit_input_header();
	emit_param("STRING", "%s", "to Cont");
	emit_output_expected_header();
	emit_retval("%d", 5);
	emit_output_actual_header();
	emit_retval("%d", loc);
	if(loc != 5) {
		FAIL;
	}
	PASS;
}

//in test_mystring.cpp 628
static bool find_substring_not_found() {
	emit_test("Does find() return -1 when the substring is not in the "
		"MyString?");
	MyString a("Alberto Contador");
	int loc = a.find("Lance");
	emit_input_header();
	emit_param("STRING", "%s", "Lance");
	emit_output_expected_header();
	emit_retval("%d", -1);
	emit_output_actual_header();
	emit_retval("%d", loc);
	if(loc != -1) {
		FAIL;
	}
	PASS;
}

static bool find_invalid_start_less() {
	emit_test("Does find() return -1 when start position is less than 0?");
	MyString a("Alberto Contador");
	int loc = a.find("Alberto", -1);
	emit_input_header();
	emit_param("STRING", "%s", "Alberto");
	emit_param("Start Pos", "%d", -1);
	emit_output_expected_header();
	emit_retval("%d", -1);
	emit_output_actual_header();
	emit_retval("%d", loc);
	if(loc != -1) {
		FAIL;
	}
	PASS;
}

static bool find_invalid_start_greater() {
	emit_test("Does find() return -1 when start position is greater than the "
		"length of the MyString?");
	MyString a("Alberto Contador");
	int loc = a.find("Alberto", 100);
	emit_input_header();
	emit_param("STRING", "%s", "Alberto");
	emit_param("Start Pos", "%d", 100);
	emit_output_expected_header();
	emit_retval("%d", -1);
	emit_output_actual_header();
	emit_retval("%d", loc);
	if(loc != -1) {
		FAIL;
	}
	PASS;
}

//in test_mystring.cpp 240 
static bool replace_string_valid() {
	emit_test("Test replaceString() when passed a substring that exists in "
		"the MyString.");
	MyString a("12345");
	a.replaceString("234", "ab");
	emit_input_header();
	emit_param("STRING", "%s", "234");
	emit_param("STRING", "%s", "ab");
	emit_output_expected_header();
	emit_retval("%s", "1ab5");
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "1ab5") != MATCH) {
		FAIL;
	}
	PASS;
}

//in test_mystring.cpp 248 
static bool replace_string_empty() {
	emit_test("Test replaceString() when passed an empty new string.");
	MyString a("12345");
	a.replaceString("234", "");
	emit_input_header();
	emit_param("STRING", "%s", "234");
	emit_param("STRING", "%s", "");
	emit_output_expected_header();
	emit_retval("%s", "15");
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "15") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool replace_string_empty_my_string() {
	emit_test("Test replaceString() on an empty MyString.");
	MyString a;
	a.replaceString("1", "2");
	emit_input_header();
	emit_param("STRING", "%s", "1");
	emit_param("STRING", "%s", "2");
	emit_output_expected_header();
	emit_retval("%s", "");
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool replace_string_beginning() {
	emit_test("Test replaceString() at the beginning of the MyString.");
	MyString a("12345");
	a.replaceString("1", "01");
	emit_input_header();
	emit_param("STRING", "%s", "1");
	emit_param("STRING", "%s", "01");
	emit_output_expected_header();
	emit_retval("%s", "012345");
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "012345") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool replace_string_end() {
	emit_test("Test replaceString() at the end of the MyString.");
	MyString a("12345");
	a.replaceString("5", "56");
	emit_input_header();
	emit_param("STRING", "%s", "5");
	emit_param("STRING", "%s", "56");
	emit_output_expected_header();
	emit_retval("%s", "123456");
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "123456") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool replace_string_return_false() {
	emit_test("Does replaceString() return false when the substring doesn't "
		"exist in the MyString.");
	MyString a("12345");
	bool found = a.replaceString("abc", "");
	emit_input_header();
	emit_param("STRING", "%s", "abc");
	emit_param("STRING", "%s", "");
	emit_output_expected_header();
	emit_retval("%d", 0);
	emit_output_actual_header();
	emit_retval("%d", found);
	if(found) {
		FAIL;
	}
	PASS;
}

static bool replace_string_return_true() {
	emit_test("Does replaceString() return true when the substring exists in "
		"the MyString.");
	MyString a("12345");
	bool found = a.replaceString("34", "43");
	emit_input_header();
	emit_param("STRING", "%s", "34");
	emit_param("STRING", "%s", "43");
	emit_output_expected_header();
	emit_retval("%d", 1);
	emit_output_actual_header();
	emit_retval("%d", found);
	if(!found) {
		FAIL;
	}
	PASS;
}

//in test_mystring.cpp 260
static bool sprintf_empty() {
	emit_test("Test sprintf() on an empty MyString.");
	MyString a;
	a.formatstr("%s %d", "happy", 3);
	emit_input_header();
	emit_param("STRING", "%s", "happy");
	emit_param("INT", "%d", 3);
	emit_output_expected_header();
	emit_retval("%s", "happy 3");
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "happy 3") != MATCH) {
		FAIL;
	}
	PASS;
}

//in test_mystring.cpp 270
static bool sprintf_non_empty() {
	emit_test("Test sprintf() on a non-empty MyString.");
	MyString a("replace me!");
	a.formatstr("%s %d", "sad", 5);
	emit_input_header();
	emit_param("STRING", "%s", "sad");
	emit_param("INT", "%d", 5);
	emit_output_expected_header();
	emit_retval("%s", "sad 5");
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "sad 5") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool sprintf_return_true() {
	emit_test("Does sprintf() return true on success?");
	MyString a;
	bool res = a.formatstr("%s %d", "sad", 5);
	emit_input_header();
	emit_param("STRING", "%s", "sad");
	emit_param("INT", "%d", 5);
	emit_output_expected_header();
	emit_retval("%d", 1);
	emit_output_actual_header();
	emit_retval("%d", res);
	if(!res) {
		FAIL;
	}
	PASS;
}

static bool vsprintf_empty() {
	emit_test("Test vsprintf() on an empty MyString.");
	MyString* a = new MyString();
	vsprintfHelper(a, "%s %d", "happy", 3);
	emit_input_header();
	emit_param("STRING", "%s", "happy");
	emit_param("INT", "%d", 3);
	emit_output_expected_header();
	emit_retval("%s", "happy 3");
	emit_output_actual_header();
	emit_retval("%s", a->Value());
	if(strcmp(a->Value(), "happy 3") != MATCH) {
		delete a;
		FAIL;
	}
	delete a;
	PASS;
}

static bool vsprintf_non_empty() {
	emit_test("Test vsprintf() on a non-empty MyString.");
	MyString* a = new MyString("foo");
	vsprintfHelper(a, "%s %d", "happy", 3);
	emit_input_header();
	emit_param("STRING", "%s", "happy");
	emit_param("INT", "%d", 3);
	emit_output_expected_header();
	emit_retval("%s", "happy 3");
	emit_output_actual_header();
	emit_retval("%s", a->Value());
	if(strcmp(a->Value(), "happy 3") != MATCH) {
		delete a;
		FAIL;
	}
	delete a;
	PASS;
}

static bool vsprintf_return_true() {
	emit_test("Does vsprintf() return true on success?");
	MyString* a = new MyString();
	bool res = vsprintfHelper(a, "%s %d", "happy", 3);
	emit_input_header();
	emit_param("STRING", "%s", "happy");
	emit_param("INT", "%d", 3);
	emit_output_expected_header();
	emit_retval("%d", 1);
	emit_output_actual_header();
	emit_retval("%d", res);
	if(!res) {
		delete a;
		FAIL;
	}
	delete a;
	PASS;
}



static bool formatstr_cat_empty() {
	emit_test("Test formatstr_cat() on an empty MyString.");
	MyString a;
	a.formatstr_cat("%s %d", "happy", 3);
	emit_input_header();
	emit_param("STRING", "%s", "happy");
	emit_param("INT", "%d", 3);
	emit_output_expected_header();
	emit_retval("%s", "happy 3");
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "happy 3") != MATCH) {
		FAIL;
	}
	PASS;
}

//in test_mystring.cpp 279
static bool formatstr_cat_non_empty() {
	emit_test("Test formatstr_cat() on a non-empty MyString.");
	MyString a("sad 5");
	a.formatstr_cat(" Luis Ocana");
	emit_input_header();
	emit_param("STRING", "%s", " Luis Ocana");
	emit_output_expected_header();
	emit_retval("%s", "sad 5 Luis Ocana");
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "sad 5 Luis Ocana") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool formatstr_cat_return_true() {
	emit_test("Does formatstr_cat() return true on success?");
	MyString a;
	bool res = a.formatstr_cat("%s %d", "sad", 5);
	emit_input_header();
	emit_param("STRING", "%s", "sad");
	emit_param("INT", "%d", 5);
	emit_output_expected_header();
	emit_retval("%d", 1);
	emit_output_actual_header();
	emit_retval("%d", res);
	if(!res) {
		FAIL;
	}
	PASS;
}

static bool vformatstr_cat_empty() {
	emit_test("Test vformatstr_cat() on an empty MyString.");
	MyString* a = new MyString();
	vformatstr_catHelper(a, "%s %d", "happy", 3);
	emit_input_header();
	emit_param("STRING", "%s", "happy");
	emit_param("INT", "%d", 3);
	emit_output_expected_header();
	emit_retval("%s", "happy 3");
	emit_output_actual_header();
	emit_retval("%s", a->Value());
	if(strcmp(a->Value(), "happy 3") != MATCH) {
		delete a;
		FAIL;
	}
	delete a;
	PASS;
}

static bool vformatstr_cat_non_empty() {
	emit_test("Test vformatstr_cat() on a non-empty MyString.");
	MyString* a = new MyString("foo");
	vformatstr_catHelper(a, "%s %d", "happy", 3);
	emit_input_header();
	emit_param("STRING", "%s", "happy");
	emit_param("INT", "%d", 3);
	emit_output_expected_header();
	emit_retval("%s", "foohappy 3");
	emit_output_actual_header();
	emit_retval("%s", a->Value());
	if(strcmp(a->Value(), "foohappy 3") != MATCH) {
		delete a;
		FAIL;
	}
	delete a;
	PASS;
}

static bool vformatstr_cat_return_true() {
	emit_test("Does vformatstr_cat() return true on success?");
	MyString* a = new MyString();
	bool res = vformatstr_catHelper(a, "%s %d", "happy", 3);
	emit_input_header();
	emit_param("STRING", "%s", "happy");
	emit_param("INT", "%d", 3);
	emit_output_expected_header();
	emit_retval("%d", 1);
	emit_output_actual_header();
	emit_retval("%d", res);
	if(!res) {
		delete a;
		FAIL;
	}
	delete a;
	PASS;
}



static bool append_to_list_string_empty_empty() {
	emit_test("Test append_to_list() on an empty MyString when passed an "
		"empty string.");
	MyString a;
	a.append_to_list("");
	emit_input_header();
	emit_param("STRING", "%s", "");
	emit_output_expected_header();
	emit_retval("%s", "");
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "") != MATCH) {
		FAIL;
	}
	PASS;
}

//in test_mystring.cpp 638
static bool append_to_list_string_empty_non_empty() {
	emit_test("Test append_to_list() on an empty MyString when passed a "
		"non-empty string.");
	MyString a;
	a.append_to_list("Armstrong");
	emit_input_header();
	emit_param("STRING", "%s", "Armstrong");
	emit_output_expected_header();
	emit_retval("%s", "Armstrong");
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "Armstrong") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool append_to_list_string_non_empty_empty() {
	emit_test("Test append_to_list() on a non-empty MyString when passed an "
		"empty string.");
	// Test revealed problem, MyString was fixed. See ticket #1292
	MyString a("ABC");
	a.append_to_list("");
	emit_input_header();
	emit_param("STRING", "%s", "");
	emit_output_expected_header();
	emit_retval("%s", "ABC");
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "ABC") != MATCH) {
		FAIL;
	}
	PASS;
}


static bool append_to_list_string_non_empty() {
	emit_test("Test append_to_list() on a non-empty MyString.");
	MyString a("Armstrong");
	a.append_to_list(" Pereiro");
	emit_input_header();
	emit_param("STRING", "%s", "Pereiro");
	emit_output_expected_header();
	emit_retval("%s", "Armstrong, Pereiro");
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "Armstrong, Pereiro") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool append_to_list_string_multiple() {
	emit_test("Test multiple calls to append_to_list() on a non-empty "
		"MyString.");
	MyString a("Armstrong");
	a.append_to_list(" Pereiro");
	a.append_to_list("Contador", "; ");
	a.append_to_list(" Sastre");
	emit_input_header();
	emit_param("First Append", "%s", "Pereiro");
	emit_param("Second Append", "%s", ";Contador");
	emit_param("Third Append", "%s", "Sastre");
	emit_output_expected_header();
	emit_retval("%s", "Armstrong, Pereiro; Contador, Sastre");
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "Armstrong, Pereiro; Contador, Sastre") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool append_to_list_mystring_empty_empty() {
	emit_test("Test append_to_list() on an empty MyString when passed an "
		"empty MyString.");
	MyString a;
	MyString b;
	a.append_to_list(b);
	emit_input_header();
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_retval("%s", "");
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool append_to_list_mystring_empty_non_empty() {
	emit_test("Test append_to_list() on an empty MyString when passed a "
		"non-empty MyString.");
	MyString a;
	MyString b("Armstrong");
	a.append_to_list(b);
	emit_input_header();
	emit_param("MyString", "%s", "Armstrong");
	emit_output_expected_header();
	emit_retval("%s", "Armstrong");
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "Armstrong") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool append_to_list_mystring_non_empty_empty() {
	emit_test("Test append_to_list() on a non-empty MyString when passed an "
		"empty MyString.");
	// Test revealed problem, MyString was fixed. See ticket #1293
	MyString a("ABC");
	MyString b;
	a.append_to_list(b);
	emit_input_header();
	emit_param("MyString", "%s", "");
	emit_output_expected_header();
	emit_retval("%s", "ABC");
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "ABC") != MATCH) {
		FAIL;
	}
	PASS;
}


//in test_mystring.cpp 640
static bool append_to_list_mystring_non_empty() {
	emit_test("Test append_to_list() on a non-empty MyString.");
	MyString a("Armstrong");
	MyString b(" Pereiro");
	a.append_to_list(b);
	emit_input_header();
	emit_param("MyString", "%s", "Pereiro");
	emit_output_expected_header();
	emit_retval("%s", "Armstrong, Pereiro");
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "Armstrong, Pereiro") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool append_to_list_mystring_multiple() {
	emit_test("Test multiple calls to append_to_list() on a non-empty "
		"MyString.");
	MyString a("Armstrong");
	MyString b(" Pereiro");
	MyString c("Contador");
	MyString d(" Sastre");
	a.append_to_list(b);
	a.append_to_list(c, "; ");
	a.append_to_list(d);
	emit_input_header();
	emit_param("First Append", "%s", "Pereiro");
	emit_param("Second Append", "%s", ";Contador");
	emit_param("Third Append", "%s", "Sastre");
	emit_output_expected_header();
	emit_retval("%s", "Armstrong, Pereiro; Contador, Sastre");
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "Armstrong, Pereiro; Contador, Sastre") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool lower_case_empty() {
	emit_test("Test lower_case() on an empty MyString.");
	MyString a;
	a.lower_case();
	emit_output_expected_header();
	emit_retval("%s", "");
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool lower_case_non_empty() {
	emit_test("Test lower_case() on a non-empty MyString.");
	MyString a("lower UPPER");
	a.lower_case();
	emit_output_expected_header();
	emit_retval("%s", "lower upper");
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "lower upper") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool upper_case_empty() {
	emit_test("Test upper_case() on an empty MyString.");
	MyString a;
	a.upper_case();
	emit_output_expected_header();
	emit_retval("%s", "");
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool upper_case_non_empty() {
	emit_test("Test upper_case() on a non-empty MyString.");
	MyString a("lower UPPER");
	a.upper_case();
	emit_output_expected_header();
	emit_retval("%s", "LOWER UPPER");
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "LOWER UPPER") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool chomp_new_line_end() {
	emit_test("Does chomp() remove the newLine if its the last character in "
		"the MyString?");
	MyString a("stuff\n");
	a.chomp();
	emit_output_expected_header();
	emit_retval("%s", "stuff");
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "stuff") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool chomp_crlf_end() {
	emit_test("Does chomp() remove CR and LF if they are the last characters"
		" in the MyString?");
	MyString a("stuff\r\n");
	a.chomp();
	emit_output_expected_header();
	emit_retval("%s", "stuff");
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "stuff") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool chomp_new_line_beginning() {
	emit_test("Does chomp() do nothing if the newLine if its not the last "
		"character in the MyString?");
	MyString a("stuff\nmore stuff");
	a.chomp();
	emit_output_expected_header();
	emit_retval("%s", "stuff\nmore stuff");
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "stuff\nmore stuff") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool chomp_return_false() {
	emit_test("Does chomp() return false if the MyString doesn't have a "
		"newLine?");
	MyString a("stuff");
	bool result = a.chomp();
	emit_output_expected_header();
	emit_retval("%d", 0);
	emit_output_actual_header();
	emit_retval("%d", result);
	if(result) {
		FAIL;
	}
	PASS;
}

static bool chomp_return_true() {
	emit_test("Does chomp() return true if the MyString has a newLine at the "
		"end?");
	MyString a("stuff\n");
	bool result = a.chomp();
	emit_output_expected_header();
	emit_retval("%d", 1);
	emit_output_actual_header();
	emit_retval("%d", result);
	if(!result) {
		FAIL;
	}
	PASS;
}

//in test_mystring.cpp 539
static bool trim_beginning_and_end() {
	emit_test("Test trim on a MyString with white space at the beginning and "
		"end.");
	MyString a("  Miguel   ");
	a.trim();
	emit_output_expected_header();
	emit_retval("%s", "Miguel");
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "Miguel") != MATCH) {
		FAIL;
	}
	PASS;
}

//in test_mystring.cpp 567
static bool trim_end() {
	emit_test("Test trim() on a MyString with white space at the end.");
	MyString a("Hinault   ");
	a.trim();
	emit_output_expected_header();
	emit_retval("%s", "Hinault");
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "Hinault") != MATCH) {
		FAIL;
	}
	PASS;
}

//in test_mystring.cpp 548
static bool trim_none() {
	emit_test("Test trim() on a MyString with no white space.");
	MyString a("Indurain");
	a.trim();
	emit_output_expected_header();
	emit_retval("%s", "Indurain");
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "Indurain") != MATCH) {
		FAIL;
	}
	PASS;
}

//in test_mystring.cpp 558
static bool trim_beginning() {
	emit_test("Test trim() on a MyString with white space at the beginning.");
	MyString a("   Merckx");
	a.trim();
	emit_output_expected_header();
	emit_retval("%s", "Merckx");
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "Merckx") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool trim_empty() {
	emit_test("Test trim() on an empty MyString.");
	MyString a;
	a.trim();
	emit_output_expected_header();
	emit_retval("%s", "");
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "") != MATCH) {
		FAIL;
	}
	PASS;
}

//in test_mystring.cpp 53
static bool equality_mystring_copied() {
	emit_test("Test == between two MyStrings after using the copy "
		"constructor.");
	MyString a("12");
	MyString b(a);
	bool result = (a == b);
	emit_input_header();
	emit_param("MyString", "%s", a.Value());
	emit_param("MyString", "%s", b.Value());
	emit_output_expected_header();
	emit_retval("%d", 1);
	emit_output_actual_header();
	emit_retval("%d", result);
	if(!result) {
		FAIL;
	}
	PASS;
}

static bool equality_mystring_one_char() {
	emit_test("Test == between two MyStrings of one character.");
	MyString a("1");
	MyString b("1");
	bool result = (a == b);
	emit_input_header();
	emit_param("MyString", "%s", a.Value());
	emit_param("MyString", "%s", b.Value());
	emit_output_expected_header();
	emit_retval("%d", 1);
	emit_output_actual_header();
	emit_retval("%d", result);
	if(!result) {
		FAIL;
	}
	PASS;
}

static bool equality_mystring_two_char() {
	emit_test("Test == between two MyStrings of two characters.");
	MyString a("12");
	MyString b("12");
	bool result = (a == b);
	emit_input_header();
	emit_param("MyString", "%s", a.Value());
	emit_param("MyString", "%s", b.Value());
	emit_output_expected_header();
	emit_retval("%d", 1);
	emit_output_actual_header();
	emit_retval("%d", result);
	if(!result) {
		FAIL;
	}
	PASS;
}

static bool equality_mystring_substring() {
	emit_test("Test == between a MyString that is a substring of the other.");
	MyString a("12345");
	MyString b("123");
	bool result = (a == b);
	emit_input_header();
	emit_param("MyString", "%s", a.Value());
	emit_param("MyString", "%s", b.Value());
	emit_output_expected_header();
	emit_retval("%d", 0);
	emit_output_actual_header();
	emit_retval("%d", result);
	if(result) {
		FAIL;
	}
	PASS;
}

static bool equality_mystring_one_empty() {
	emit_test("Test == between a MyString that is empty and one that isn't.");
	MyString a("12345");
	MyString b;
	bool result = (a == b);
	emit_input_header();
	emit_param("MyString", "%s", a.Value());
	emit_param("MyString", "%s", b.Value());
	emit_output_expected_header();
	emit_retval("%d", 0);
	emit_output_actual_header();
	emit_retval("%d", result);
	if(result) {
		FAIL;
	}
	PASS;
}

static bool equality_mystring_two_empty() {
	emit_test("Test == between a two empty MyStrings.");
	MyString a;
	MyString b;
	bool result = (a == b);
	emit_input_header();
	emit_param("MyString", "%s", a.Value());
	emit_param("MyString", "%s", b.Value());
	emit_output_expected_header();
	emit_retval("%d", 0);
	emit_output_actual_header();
	emit_retval("%d", result);
	if(!result) {
		FAIL;
	}
	PASS;
}

//in test_mystring.cpp 36
static bool equality_string_one_char() {
	emit_test("Test == between a MyString and a string of one character.");
	MyString a("1");
	const char* b = "1";
	bool result = (a == b);
	emit_input_header();
	emit_param("MyString", "%s", a.Value());
	emit_param("String", "%s", b);
	emit_output_expected_header();
	emit_retval("%d", 1);
	emit_output_actual_header();
	emit_retval("%d", result);
	if(!result) {
		FAIL;
	}
	PASS;
}

//in test_mystring.cpp 44
static bool equality_string_two_char() {
	emit_test("Test == between a MyString and a string of two characters.");
	MyString a("12");
	const char* b = "12";
	bool result = (a == b);
	emit_input_header();
	emit_param("MyString", "%s", a.Value());
	emit_param("String", "%s", b);
	emit_output_expected_header();
	emit_retval("%d", 1);
	emit_output_actual_header();
	emit_retval("%d", result);
	if(!result) {
		FAIL;
	}
	PASS;
}

//in test_mystring.cpp 62
static bool equality_string_copied_delete() {
	emit_test("Test == between two MyStrings after using the copy constructor"
		" and then deleting the original MyString.");
	const char* b = "12";
	MyString* b_ms = new MyString(b);
	MyString b_dup(*b_ms);
	delete b_ms;
	bool result = (b == b_dup);
	emit_input_header();
	emit_param("String", "%s", b);
	emit_param("MyString", "%s", b_dup.Value());
	emit_output_expected_header();
	emit_retval("%d", 1);
	emit_output_actual_header();
	emit_retval("%d", result);
	if(!result) {
		FAIL;
	}
	PASS;
}

static bool equality_string_substring() {
	emit_test("Test == between a String that is a substring of the "
		"MyString.");
	MyString a("12345");
	const char* b = "123";
	bool result = (a == b);
	emit_input_header();
	emit_param("MyString", "%s", a.Value());
	emit_param("String", "%s", b);
	emit_output_expected_header();
	emit_retval("%d", 0);
	emit_output_actual_header();
	emit_retval("%d", result);
	if(result) {
		FAIL;
	}
	PASS;
}

static bool equality_string_one_empty() {
	emit_test("Test == between a MyString that is empty and a string that "
		"isn't.");
	MyString a;
	const char* b = "12345";
	bool result = (a == b);
	emit_input_header();
	emit_param("MyString", "%s", a.Value());
	emit_param("String", "%s", b);
	emit_output_expected_header();
	emit_retval("%d", 0);
	emit_output_actual_header();
	emit_retval("%d", result);
	if(result) {
		FAIL;
	}
	PASS;
}

static bool equality_string_two_empty() {
	emit_test("Test == between an empty MyString and an empty string.");
	MyString a;
	const char* b = "\0";
	bool result = (a == b);
	emit_input_header();
	emit_param("MyString", "%s", a.Value());
	emit_param("String", "%s", "NULL");
	emit_output_expected_header();
	emit_retval("%d", 1);
	emit_output_actual_header();
	emit_retval("%d", result);
	if(!result) {
		FAIL;
	}
	PASS;
}

static bool inequality_mystring_copy() {
	emit_test("Test != between two MyStrings after using the copy "
		"constructor.");
	MyString a("foo");
	MyString b(a);
	bool result = (a != b);
	emit_input_header();
	emit_param("MyString", "%s", a.Value());
	emit_param("MyString", "%s", b.Value());
	emit_output_expected_header();
	emit_retval("%d", 0);
	emit_output_actual_header();
	emit_retval("%d", result);
	if(result) {
		FAIL;
	}
	PASS;
}

//in test_mystring.cpp 78
static bool inequality_mystring_not_equal() {
	emit_test("Test != between two MyStrings that are not equal.");
	MyString a("12");
	MyString b("1");
	bool result = (a != b);
	emit_input_header();
	emit_param("MyString", "%s", a.Value());
	emit_param("MyString", "%s", b.Value());
	emit_output_expected_header();
	emit_retval("%d", 1);
	emit_output_actual_header();
	emit_retval("%d", result);
	if(!result) {
		FAIL;
	}
	PASS;
}

static bool inequality_string_equal() {
	emit_test("Test != between a MyString and a string that are equal.");
	MyString a("123");
	const char* b = "123";
	bool result = (a != b);
	emit_input_header();
	emit_param("MyString", "%s", a.Value());
	emit_param("String", "%s", b);
	emit_output_expected_header();
	emit_retval("%d", 0);
	emit_output_actual_header();
	emit_retval("%d", result);
	if(result) {
		FAIL;
	}
	PASS;
}

//in test_mystring.cpp 70
static bool inequality_string_not_equal() {
	emit_test("Test != between a MyString and a string that are not equal.");
	MyString a("12");
	const char* b = "1";
	bool result = (a != b);
	emit_input_header();
	emit_param("MyString", "%s", a.Value());
	emit_param("String", "%s", b);
	emit_output_expected_header();
	emit_retval("%d", 1);
	emit_output_actual_header();
	emit_retval("%d", result);
	if(!result) {
		FAIL;
	}
	PASS;
}

//in test_mystring.cpp 86
static bool less_than_mystring_less() {
	emit_test("Test < on a MyString that is less than the other MyString.");
	MyString a("1");
	MyString b("12");
	bool result = (a < b);
	emit_input_header();
	emit_param("MyString", "%s", a.Value());
	emit_param("MyString", "%s", b.Value());
	emit_output_expected_header();
	emit_retval("%d", 1);
	emit_output_actual_header();
	emit_retval("%d", result);
	if(!result) {
		FAIL;
	}
	PASS;
}

static bool less_than_mystring_equal() {
	emit_test("Test < between two MyStrings that are equal.");
	MyString a("foo");
	MyString b(a);
	bool result = (a < b);
	emit_input_header();
	emit_param("MyString", "%s", a.Value());
	emit_param("MyString", "%s", b.Value());
	emit_output_expected_header();
	emit_retval("%d", 0);
	emit_output_actual_header();
	emit_retval("%d", result);
	if(result) {
		FAIL;
	}
	PASS;
}

static bool less_than_mystring_greater() {
	emit_test("Test < on a MyString that is greater than the other "
		"MyString.");
	MyString a("foo");
	MyString b("bar");
	bool result = (a < b);
	emit_input_header();
	emit_param("MyString", "%s", a.Value());
	emit_param("MyString", "%s", b.Value());
	emit_output_expected_header();
	emit_retval("%d", 0);
	emit_output_actual_header();
	emit_retval("%d", result);
	if(result) {
		FAIL;
	}
	PASS;
}

static bool less_than_or_equal_mystring_less() {
	emit_test("Test <= on a MyString that is less than the other MyString.");
	MyString a("bar");
	MyString b("foo");
	bool result = (a <= b);
	emit_input_header();
	emit_param("MyString", "%s", a.Value());
	emit_param("MyString", "%s", b.Value());
	emit_output_expected_header();
	emit_retval("%d", 1);
	emit_output_actual_header();
	emit_retval("%d", result);
	if(!result) {
		FAIL;
	}
	PASS;
}

static bool less_than_or_equal_mystring_equal() {
	emit_test("Test <= on a MyString that is equal to the other MyString.");
	MyString a("foo");
	MyString b(a);
	bool result = (a <= b);
	emit_input_header();
	emit_param("MyString", "%s", a.Value());
	emit_param("MyString", "%s", b.Value());
	emit_output_expected_header();
	emit_retval("%d", 1);
	emit_output_actual_header();
	emit_retval("%d", result);
	if(!result) {
		FAIL;
	}
	PASS;
}

static bool less_than_or_equal_mystring_greater() {
	emit_test("Test <= on a MyString that is greater that the other "
		"MyString.");
	MyString a("foo");
	MyString b("bar");
	bool result = (a <= b);
	emit_input_header();
	emit_param("MyString", "%s", a.Value());
	emit_param("MyString", "%s", b.Value());
	emit_output_expected_header();
	emit_retval("%d", 0);
	emit_output_actual_header();
	emit_retval("%d", result);
	if(result) {
		FAIL;
	}
	PASS;
}

static bool greater_than_mystring_less() {
	emit_test("Test > on a MyString that is less than the other MyString.");
	MyString a("bar");
	MyString b("foo");
	bool result = (a > b);
	emit_input_header();
	emit_param("MyString", "%s", a.Value());
	emit_param("MyString", "%s", b.Value());
	emit_output_expected_header();
	emit_retval("%d", 0);
	emit_output_actual_header();
	emit_retval("%d", result);
	if(result) {
		FAIL;
	}
	PASS;
}

static bool greater_than_mystring_equal() {
	emit_test("Test > on a MyString that is equal to the other MyString.");
	MyString a("foo");
	MyString b(a);
	bool result = (a > b);
	emit_input_header();
	emit_param("MyString", "%s", a.Value());
	emit_param("MyString", "%s", b.Value());
	emit_output_expected_header();
	emit_retval("%d", 0);
	emit_output_actual_header();
	emit_retval("%d", result);
	if(result) {
		FAIL;
	}
	PASS;
}

static bool greater_than_mystring_greater() {
	emit_test("Test > on a MyString that is greater than the other "
		"MyString.");
	MyString a("foo");
	MyString b("bar");
	bool result = (a > b);
	emit_input_header();
	emit_param("MyString", "%s", a.Value());
	emit_param("MyString", "%s", b.Value());
	emit_output_expected_header();
	emit_retval("%d", 1);
	emit_output_actual_header();
	emit_retval("%d", result);
	if(!result) {
		FAIL;
	}
	PASS;
}

static bool greater_than_or_equal_mystring_less() {
	emit_test("Test >= on a MyString that is less than the other MyString.");
	MyString a("bar");
	MyString b("foo");
	bool result = (a >= b);
	emit_input_header();
	emit_param("MyString", "%s", a.Value());
	emit_param("MyString", "%s", b.Value());
	emit_output_expected_header();
	emit_retval("%d", 0);
	emit_output_actual_header();
	emit_retval("%d", result);
	if(result) {
		FAIL;
	}
	PASS;
}

static bool greater_than_or_equal_mystring_equal() {
	emit_test("Test >= on a MyString that is equal to the other MyString.");
	MyString a("foo");
	MyString b(a);
	bool result = (a >= b);
	emit_input_header();
	emit_param("MyString", "%s", a.Value());
	emit_param("MyString", "%s", b.Value());
	emit_output_expected_header();
	emit_retval("%d", 1);
	emit_output_actual_header();
	emit_retval("%d", result);
	if(!result) {
		FAIL;
	}
	PASS;
}

static bool greater_than_or_equal_mystring_greater() {
	emit_test("Test >= on a MyString that is greater than the other "
		"MyString.");
	MyString a("foo");
	MyString b("bar");
	bool result = (a >= b);
	emit_input_header();
	emit_param("MyString", "%s", a.Value());
	emit_param("MyString", "%s", b.Value());
	emit_output_expected_header();
	emit_retval("%d", 1);
	emit_output_actual_header();
	emit_retval("%d", result);
	if(!result) {
		FAIL;
	}
	PASS;
}

static bool read_line_empty_file_return() {
	emit_test("Does readLine() return false when called with an empty file?");
	MyString a;
	FILE* tmp = tmpfile();
	bool res = a.readLine(tmp, false);
	fclose(tmp);
	emit_input_header();
	emit_param("FILE", "%s", "tmp");
	emit_param("append", "%d", false);
	emit_output_expected_header();
	emit_retval("%d", 0);
	emit_output_actual_header();
	emit_retval("%d", res);
	if(res) {
		FAIL;
	}
	PASS;
}

static bool read_line_empty_file_preserve() {
	emit_test("Does readLine() preserve the original MyString when called "
		"with an empty file?");
	MyString a("foo");
	FILE* tmp = tmpfile();
	a.readLine(tmp, false);
	fclose(tmp);
	emit_input_header();
	emit_param("FILE", "%s", "tmp");
	emit_param("append", "%d", false);
	emit_output_expected_header();
	emit_retval("%s", "foo");
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "foo") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool read_line_empty() {
	emit_test("Does readLine() correctly append to an empty MyString?");
	MyString a;
	FILE* tmp = tmpfile();
	fputs("foo", tmp);
	rewind(tmp);
	a.readLine(tmp, true);
	fclose(tmp);
	emit_input_header();
	emit_param("FILE", "%s", "tmp");
	emit_param("append", "%d", true);
	emit_output_expected_header();
	emit_retval("%s", "foo");
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "foo") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool read_line_null_file() {
	emit_test("Test readLine() when passed a null file pointer.");
	emit_problem("By inspection, readLine() code correctly ASSERTs on fp=NULL, but we can't verify that in the current unit test framework.");

/*
	MyString a("foo");
	FILE* tmp = NULL;
	a.readLine(tmp, true);
	emit_input_header();
	emit_param("FILE", "%s", "tmp");
	emit_param("append", "%d", false);
	emit_output_expected_header();
	emit_retval("%s", "foo");
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "foo") != MATCH) {
		FAIL;
	}
*/
	PASS;
}


static bool read_line_append() {
	emit_test("Does readLine() correctly append to a MyString from a file?");
	MyString a("Line");
	FILE* tmp = tmpfile();
	fputs(" of text", tmp);
	rewind(tmp);
	a.readLine(tmp, true);
	fclose(tmp);
	emit_input_header();
	emit_param("FILE", "%s", "tmp");
	emit_param("append", "%d", false);
	emit_output_expected_header();
	emit_retval("%s", "Line of text");
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "Line of text") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool read_line_return() {
	emit_test("Does readLine() return true when reading a line from a file?");
	MyString a("Line");
	FILE* tmp = tmpfile();
	fputs(" of text", tmp);
	rewind(tmp);
	bool res = a.readLine(tmp, false);
	fclose(tmp);
	emit_input_header();
	emit_param("FILE", "%s", "tmp");
	emit_param("append", "%d", false);
	emit_output_expected_header();
	emit_retval("%d", 1);
	emit_output_actual_header();
	emit_retval("%d", res);
	if(!res) {
		FAIL;
	}
	PASS;
}

static bool read_line_replace() {
	emit_test("Does readLine() replace the original MyString when append is "
		"set to false?");
	MyString a("foo");
	FILE* tmp = tmpfile();
	fputs("bar", tmp);
	rewind(tmp);
	a.readLine(tmp, false);
	fclose(tmp);
	emit_input_header();
	emit_param("FILE", "%s", "tmp");
	emit_param("append", "%d", false);
	emit_output_expected_header();
	emit_retval("%s", "bar");
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "bar")) {
		FAIL;
	}
	PASS;
}

static bool read_line_multiple() {
	emit_test("Test multiple calls to readLine().");
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
	emit_input_header();
	emit_param("FILE", "%s", "tmp");
	emit_param("append", "%d", true);
	emit_output_expected_header();
	emit_retval("%s", "Line 1\nLine 2\nLine 3\nLine 4");
	emit_output_actual_header();
	emit_retval("%s", a.Value());
	if(strcmp(a.Value(), "Line 1\nLine 2\nLine 3\nLine 4")) {
		FAIL;
	}
	PASS;
}

//in test_mystring.cpp 460
static bool tokenize_null() {
	emit_test("Does calling GetNextToken() before calling Tokenize() return "
		"NULL?");
	MyStringWithTokener a("foo, bar");
	const char* tok = a.GetNextToken(",", false);
	emit_input_header();
	emit_param("delim", "%s", ",");
	emit_param("skipBlankTokens", "%d", false);
	emit_output_expected_header();
	emit_retval("%s", "NULL");
	if(tok != NULL) {
		FAIL;
	}
	PASS;
}

//in test_mystring.cpp 471
static bool tokenize_skip() {
	emit_test("Test GetNextToken() when skipping blank tokens.");
	MyStringTokener a;
	a.Tokenize("     Ottavio Bottechia_");
	const char* tok = a.GetNextToken(" ", true);
	emit_input_header();
	emit_param("delim", "%s", " ");
	emit_param("skipBlankTokens", "%d", true);
	emit_output_expected_header();
	emit_retval("%s", "Ottavio");
	emit_output_actual_header();
	emit_retval("%s", tok);
	if(strcmp(tok, "Ottavio") != MATCH) {
		FAIL;
	}
	PASS;
}

//in test_mystring.cpp 483
static bool tokenize_multiple_calls() {
	emit_test("Test multiple calls to GetNextToken().");
	MyStringTokener a;
	a.Tokenize("To  be or not to be; that is the question");
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
	emit_input_header();
	emit_param("delim", "%s", " ;");
	emit_param("skipBlankTokens", "%d", false);
	emit_output_expected_header();
	emit_param("Token 1", "%s", expectedTokens[0]);
	emit_param("Token 2", "%s", expectedTokens[1]);
	emit_param("Token 3", "%s", expectedTokens[2]);
	emit_param("Token 4", "%s", expectedTokens[3]);
	emit_param("Token 5", "%s", expectedTokens[4]);
	emit_param("Token 6", "%s", expectedTokens[5]);
	emit_param("Token 7", "%s", expectedTokens[6]);
	emit_param("Token 8", "%s", expectedTokens[7]);
	emit_param("Token 9", "%s", expectedTokens[8]);
	emit_param("Token 10", "%s", expectedTokens[9]);
	emit_param("Token 11", "%s", expectedTokens[10]);
	emit_param("Token 12", "%s", expectedTokens[11]);
	emit_output_actual_header();
	emit_param("Token 1", "%s", resultToken0);
	emit_param("Token 2", "%s", resultToken1);
	emit_param("Token 3", "%s", resultToken2);
	emit_param("Token 4", "%s", resultToken3);
	emit_param("Token 5", "%s", resultToken4);
	emit_param("Token 6", "%s", resultToken5);
	emit_param("Token 7", "%s", resultToken6);
	emit_param("Token 8", "%s", resultToken7);
	emit_param("Token 9", "%s", resultToken8);
	emit_param("Token 10", "%s", resultToken9);
	emit_param("Token 11", "%s", resultToken10);
	emit_param("Token 12", "%s", resultToken11);
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
		FAIL;
	}
	PASS;
}

//in test_mystring.cpp 491
static bool tokenize_end() {
	emit_test("Test GetNextToken() after getting to the end.");
	MyStringTokener a;
	a.Tokenize("foo;");
	const char* tok = a.GetNextToken(";", false);
	tok = a.GetNextToken(";", false);
	tok = a.GetNextToken(";", false);
	emit_input_header();
	emit_param("delim", "%s", ";");
	emit_param("skipBlankTokens", "%d", false);
	emit_output_expected_header();
	emit_retval("%s", "NULL");
	if(tok != NULL) {
		FAIL;
	}
	PASS;
}

//in test_mystring.cpp 519
static bool tokenize_empty() {
	emit_test("Test GetNextToken() on an empty MyString.");
	MyStringTokener a;
	a.Tokenize("");
	const char* tok = a.GetNextToken(" ", false);
	emit_input_header();
	emit_param("delim", "%s", " ");
	emit_param("skipBlankTokens", "%d", false);
	emit_output_expected_header();
	emit_retval("%s", "NULL");
	if(tok != NULL) {
		FAIL;
	}
	PASS;
}

//in test_mystring.cpp 529
static bool tokenize_empty_delimiter() {
	emit_test("Test GetNextToken() on an empty delimiter string.");
	MyStringTokener a;
	a.Tokenize("foobar");
	const char* tok = a.GetNextToken("", false);
	emit_input_header();
	emit_param("delim", "%s", " ");
	emit_param("skipBlankTokens", "%d", false);
	emit_output_expected_header();
	emit_retval("%s", "NULL");
	if(tok != NULL) {
		FAIL;
	}
	PASS;
}

static bool your_string_string_constructor() {
	emit_test("Test the YourString constructor that assigns the "
		"sensitive string to the passed c string.");
	YourString a("foo");
	emit_output_expected_header();
	emit_retval("%s", "foo");
	if(!(a == "foo")) {
		FAIL;
	}
	PASS;
}

static bool your_string_equality_true() {
	emit_test("Does the == operator return true when the sensitive string is "
		"equal to the other string?");
	YourString a("foo");
	bool res = (a == "foo");
	emit_input_header();
	emit_param("STRING", "%s", "foo");
	emit_output_expected_header();
	emit_retval("%d", 1);
	emit_output_actual_header();
	emit_retval("%d", res);
	if(!res) {
		FAIL;
	}
	PASS;
}

static bool your_string_equality_false() {
	emit_test("Does the == operator return false when the sensitive string is"
		" not equal to the other string?");
	YourString a("foo");
	bool res = (a == "bar");
	emit_input_header();
	emit_param("STRING", "%s", "bar");
	emit_output_expected_header();
	emit_retval("%d", 0);
	emit_output_actual_header();
	emit_retval("%d", res);
	if(res) {
		FAIL;
	}
	PASS;
}

static bool your_string_equality_default_constructor() {
	emit_test("Does the == operator return false after using the default "
		"YourString constructor?");

	YourString a;
	bool res = (a == "foo");
	emit_input_header();
	emit_param("STRING", "%s", "foo");
	emit_output_expected_header();
	emit_retval("%d", 0);
	emit_output_actual_header();
	emit_retval("%d", res);
	if(res) {
		FAIL;
	}
	PASS;
}


static bool your_string_assignment_non_empty_empty() {
	emit_test("Does the = operator assign the non-empty sensitive string to "
		"an empty one?");
	YourString a("foo");
	a = "";
	emit_input_header();
	emit_param("STRING", "%s", "");
	emit_output_expected_header();
	emit_retval("%s", "");
	if(!(a == "")) {
		FAIL;
	}
	PASS;
}

static bool your_string_assignment_empty_non_empty() {
	emit_test("Does the = operator assign the empty sensitive string to a "
		"non-empty one?");
	YourString a;
	a = "foo";
	emit_input_header();
	emit_param("STRING", "%s", "foo");
	emit_output_expected_header();
	emit_retval("%s", "foo");
	if(!(a == "foo")) {
		FAIL;
	}
	PASS;
}

static bool your_string_assignment_non_empty() {
	emit_test("Does the = operator assign the non-empty sensitive string to a"
		" different non-empty one of different length?");
	YourString a("foo");
	a = "b";
	emit_input_header();
	emit_param("STRING", "%s", "b");
	emit_output_expected_header();
	emit_retval("%s", "b");
	if(!(a == "b")) {
		FAIL;
	}
	PASS;
}

static bool test_stl_string_casting() {
    emit_test("Test casting operations wrt std::string");
        //emit_input_header();
        //emit_output_expected_header();
        //emit_output_actual_header();

    // test casting ops provided via MyString
    MyString str1 = "foo";
    std::string str2 = "bar";
    str2 = str1;
    if (str2 != str1.Value()) {
		FAIL;
    }

    str2 = "bar";
    str1 = str2;
    if (str1 != str2.c_str()) {
		FAIL;
    }

    PASS;
}
