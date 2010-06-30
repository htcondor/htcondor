/***************************************************************
 *
 * Copyright (C) 1990-2010, Condor Team, Computer Sciences Department,
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
	This code tests the sin_to_string() function implementation.
 */

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"

#include "function_test_driver.h"
#include "emit.h"
#include "unit_test_utils.h"

#include "stl_string_utils.h"

static bool test_sprintf_string(void);
static bool test_sprintf_MyString(void);
static bool test_sprintf_cat_string(void);
static bool test_sprintf_cat_MyString(void);
static bool test_comparison_ops_lhs_string(void);
static bool test_comparison_ops_lhs_MyString(void);
static bool test_lower_case_empty(void);
static bool test_lower_case_non_empty(void);
static bool test_upper_case_empty(void);
static bool test_upper_case_non_empty(void);
static bool test_chomp_new_line_end(void);
static bool test_chomp_crlf_end(void);
static bool test_chomp_new_line_beginning(void);
static bool test_chomp_return_false(void);
static bool test_chomp_return_true(void);
static bool test_trim_beginning_and_end(void);
static bool test_trim_end(void);
static bool test_trim_none(void);
static bool test_trim_beginning(void);
static bool test_trim_empty(void);

bool FTEST_stl_string_utils(void) {
		// beginning junk for getPortFromAddr(() {
	emit_function("STL string utils");
	emit_comment("Package of functions/operators to facilitate adoption of std::string");
	
		// driver to run the tests and all required setup
	FunctionDriver driver;
	driver.register_function(test_comparison_ops_lhs_string);
	driver.register_function(test_comparison_ops_lhs_MyString);
	driver.register_function(test_sprintf_string);
	driver.register_function(test_sprintf_MyString);
	driver.register_function(test_sprintf_cat_string);
	driver.register_function(test_sprintf_cat_MyString);
	driver.register_function(test_lower_case_empty);
	driver.register_function(test_lower_case_non_empty);
	driver.register_function(test_upper_case_empty);
	driver.register_function(test_upper_case_non_empty);
	driver.register_function(test_chomp_new_line_end);
	driver.register_function(test_chomp_crlf_end);
	driver.register_function(test_chomp_new_line_beginning);
	driver.register_function(test_chomp_return_false);
	driver.register_function(test_chomp_return_true);
	driver.register_function(test_trim_beginning_and_end);
	driver.register_function(test_trim_end);
	driver.register_function(test_trim_none);
	driver.register_function(test_trim_beginning);
	driver.register_function(test_trim_empty);
	
		// run the tests
	return driver.do_all_functions();
}


static bool test_sprintf_string() {
    emit_test("Test sprintf overloading for std::string");

    int nchars = 0;
    int rchars = 0;
    std::string dst;
    std::string src;

    nchars = STL_STRING_UTILS_FIXBUF / 2;
    src="";
    for (int j = 0;  j < nchars;  ++j) src += char('0' + (j % 10));
    rchars = sprintf(dst, "%s", src.c_str());
    if (dst != src) {
		FAIL;        
    }
    if (rchars != nchars) {
		FAIL;        
    }

    nchars = STL_STRING_UTILS_FIXBUF - 1;
    src="";
    for (int j = 0;  j < nchars;  ++j) src += char('0' + (j % 10));
    rchars = sprintf(dst, "%s", src.c_str());
    if (dst != src) {
		FAIL;        
    }
    if (rchars != nchars) {
		FAIL;        
    }

    nchars = STL_STRING_UTILS_FIXBUF;
    src="";
    for (int j = 0;  j < nchars;  ++j) src += char('0' + (j % 10));
    rchars = sprintf(dst, "%s", src.c_str());
    if (dst != src) {
		FAIL;        
    }
    if (rchars != nchars) {
		FAIL;        
    }

    nchars = STL_STRING_UTILS_FIXBUF + 1;
    src="";
    for (int j = 0;  j < nchars;  ++j) src += char('0' + (j % 10));
    rchars = sprintf(dst, "%s", src.c_str());
    if (dst != src) {
		FAIL;        
    }
    if (rchars != nchars) {
		FAIL;        
    }

    nchars = STL_STRING_UTILS_FIXBUF * 2;
    src="";
    for (int j = 0;  j < nchars;  ++j) src += char('0' + (j % 10));
    rchars = sprintf(dst, "%s", src.c_str());
    if (dst != src) {
		FAIL;        
    }
    if (rchars != nchars) {
		FAIL;        
    }

	PASS;
}

static bool test_sprintf_MyString() {
    emit_test("Test sprintf overloading for MyString");

    int nchars = 0;
    int rchars = 0;
    MyString dst;
    MyString src;

    nchars = STL_STRING_UTILS_FIXBUF / 2;
    src="";
    for (int j = 0;  j < nchars;  ++j) src += char('0' + (j % 10));
    rchars = sprintf(dst, "%s", src.Value());
    if (dst != src) {
		FAIL;        
    }
    if (rchars != nchars) {
		FAIL;        
    }

    nchars = STL_STRING_UTILS_FIXBUF - 1;
    src="";
    for (int j = 0;  j < nchars;  ++j) src += char('0' + (j % 10));
    rchars = sprintf(dst, "%s", src.Value());
    if (dst != src) {
		FAIL;        
    }
    if (rchars != nchars) {
		FAIL;        
    }

    nchars = STL_STRING_UTILS_FIXBUF;
    src="";
    for (int j = 0;  j < nchars;  ++j) src += char('0' + (j % 10));
    rchars = sprintf(dst, "%s", src.Value());
    if (dst != src) {
		FAIL;        
    }
    if (rchars != nchars) {
		FAIL;        
    }

    nchars = STL_STRING_UTILS_FIXBUF + 1;
    src="";
    for (int j = 0;  j < nchars;  ++j) src += char('0' + (j % 10));
    rchars = sprintf(dst, "%s", src.Value());
    if (dst != src) {
		FAIL;        
    }
    if (rchars != nchars) {
		FAIL;        
    }

    nchars = STL_STRING_UTILS_FIXBUF * 2;
    src="";
    for (int j = 0;  j < nchars;  ++j) src += char('0' + (j % 10));
    rchars = sprintf(dst, "%s", src.Value());
    if (dst != src) {
		FAIL;        
    }
    if (rchars != nchars) {
		FAIL;        
    }

	PASS;
}

static bool test_sprintf_cat_string() {
    emit_test("Test sprintf_cat overloading for std::string");

    std::string s = "foo";
    int r = sprintf_cat(s, "%s", "bar");
    if (s != "foobar") {
		FAIL;        
    }
    if (r != 3) {
		FAIL;        
    }

	PASS;
}

static bool test_sprintf_cat_MyString() {
    emit_test("Test sprintf_cat overloading for MyString");

    MyString s = "foo";
    int r = sprintf_cat(s, "%s", "bar");
    if (s != "foobar") {
		FAIL;        
    }
    if (r != 3) {
		FAIL;        
    }    

	PASS;
}

static bool test_comparison_ops_lhs_string() {
    emit_test("Test comparison operators: LHS is std::string and RHS is MyString");

    std::string lhs;
    MyString rhs;

        // test operators when lhs == rhs
    lhs = "a";
    rhs = "a";
    if ((lhs == rhs) != true) {
		FAIL;        
    }
    if ((lhs != rhs) != false) {
		FAIL;        
    }
    if ((lhs < rhs) != false) {
		FAIL;        
    }
    if ((lhs > rhs) != false) {
		FAIL;        
    }
    if ((lhs <= rhs) != true) {
		FAIL;        
    }
    if ((lhs >= rhs) != true) {
		FAIL;        
    }

        // test operators when lhs < rhs
    lhs = "a";
    rhs = "b";
    if ((lhs == rhs) != false) {
		FAIL;        
    }
    if ((lhs != rhs) != true) {
		FAIL;        
    }
    if ((lhs < rhs) != true) {
		FAIL;        
    }
    if ((lhs > rhs) != false) {
		FAIL;        
    }
    if ((lhs <= rhs) != true) {
		FAIL;        
    }
    if ((lhs >= rhs) != false) {
		FAIL;        
    }

        // test operators when lhs > rhs
    lhs = "b";
    rhs = "a";
    if ((lhs == rhs) != false) {
		FAIL;        
    }
    if ((lhs != rhs) != true) {
		FAIL;        
    }
    if ((lhs < rhs) != false) {
		FAIL;        
    }
    if ((lhs > rhs) != true) {
		FAIL;        
    }
    if ((lhs <= rhs) != false) {
		FAIL;        
    }
    if ((lhs >= rhs) != true) {
		FAIL;        
    }

	PASS;
}

static bool test_comparison_ops_lhs_MyString() {
    emit_test("Test comparison operators: LHS is MyString and LHS is std::string");

    MyString lhs;
    std::string rhs;

        // test operators when lhs == rhs
    lhs = "a";
    rhs = "a";
    if ((lhs == rhs) != true) {
		FAIL;        
    }
    if ((lhs != rhs) != false) {
		FAIL;        
    }
    if ((lhs < rhs) != false) {
		FAIL;        
    }
    if ((lhs > rhs) != false) {
		FAIL;        
    }
    if ((lhs <= rhs) != true) {
		FAIL;        
    }
    if ((lhs >= rhs) != true) {
		FAIL;        
    }

        // test operators when lhs < rhs
    lhs = "a";
    rhs = "b";
    if ((lhs == rhs) != false) {
		FAIL;        
    }
    if ((lhs != rhs) != true) {
		FAIL;        
    }
    if ((lhs < rhs) != true) {
		FAIL;        
    }
    if ((lhs > rhs) != false) {
		FAIL;        
    }
    if ((lhs <= rhs) != true) {
		FAIL;        
    }
    if ((lhs >= rhs) != false) {
		FAIL;        
    }

        // test operators when lhs > rhs
    lhs = "b";
    rhs = "a";
    if ((lhs == rhs) != false) {
		FAIL;        
    }
    if ((lhs != rhs) != true) {
		FAIL;        
    }
    if ((lhs < rhs) != false) {
		FAIL;        
    }
    if ((lhs > rhs) != true) {
		FAIL;        
    }
    if ((lhs <= rhs) != false) {
		FAIL;        
    }
    if ((lhs >= rhs) != true) {
		FAIL;        
    }

	PASS;
}

static bool test_lower_case_empty() {
	emit_test("Test lower_case() on an empty std::string.");
	std::string a;
	lower_case(a);
	emit_output_expected_header();
	emit_retval("%s", "");
	emit_output_actual_header();
	emit_retval("%s", a.c_str());
	if(strcmp(a.c_str(), "") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_lower_case_non_empty() {
	emit_test("Test lower_case() on a non-empty std::string.");
	std::string a("lower UPPER");
	lower_case(a);
	emit_output_expected_header();
	emit_retval("%s", "lower upper");
	emit_output_actual_header();
	emit_retval("%s", a.c_str());
	if(strcmp(a.c_str(), "lower upper") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_upper_case_empty() {
	emit_test("Test upper_case() on an empty std::string.");
	std::string a;
	upper_case(a);
	emit_output_expected_header();
	emit_retval("%s", "");
	emit_output_actual_header();
	emit_retval("%s", a.c_str());
	if(strcmp(a.c_str(), "") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_upper_case_non_empty() {
	emit_test("Test upper_case() on a non-empty std::string.");
	std::string a("lower UPPER");
	upper_case(a);
	emit_output_expected_header();
	emit_retval("%s", "LOWER UPPER");
	emit_output_actual_header();
	emit_retval("%s", a.c_str());
	if(strcmp(a.c_str(), "LOWER UPPER") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_chomp_new_line_end() {
	emit_test("Does chomp() remove the newLine if its the last character in "
		"the std::string?");
	std::string a("stuff\n");
	chomp(a);
	emit_output_expected_header();
	emit_retval("%s", "stuff");
	emit_output_actual_header();
	emit_retval("%s", a.c_str());
	if(strcmp(a.c_str(), "stuff") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_chomp_crlf_end() {
	emit_test("Does chomp() remove CR and LF if they are the last characters"
		" in the std::string?");
	std::string a("stuff\r\n");
	chomp(a);
	emit_output_expected_header();
	emit_retval("%s", "stuff");
	emit_output_actual_header();
	emit_retval("%s", a.c_str());
	if(strcmp(a.c_str(), "stuff") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_chomp_new_line_beginning() {
	emit_test("Does chomp() do nothing if the newLine if it's not the last "
		"character in the std::string?");
	std::string a("stuff\nmore stuff");
	chomp(a);
	emit_output_expected_header();
	emit_retval("%s", "stuff\nmore stuff");
	emit_output_actual_header();
	emit_retval("%s", a.c_str());
	if(strcmp(a.c_str(), "stuff\nmore stuff") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_chomp_return_false() {
	emit_test("Does chomp() return false if the std::string doesn't have a "
		"newLine?");
	std::string a("stuff");
	bool result = chomp(a);
	emit_output_expected_header();
	emit_retval("%d", 0);
	emit_output_actual_header();
	emit_retval("%d", result);
	if(result) {
		FAIL;
	}
	PASS;
}

static bool test_chomp_return_true() {
	emit_test("Does chomp() return true if the std::string has a newLine at the "
		"end?");
	std::string a("stuff\n");
	bool result = chomp(a);
	emit_output_expected_header();
	emit_retval("%d", 1);
	emit_output_actual_header();
	emit_retval("%d", result);
	if(!result) {
		FAIL;
	}
	PASS;
}

static bool test_trim_beginning_and_end() {
	emit_test("Test trim on a std::string with white space at the beginning and "
		"end.");
	std::string a("  Miguel   ");
	trim(a);
	emit_output_expected_header();
	emit_retval("%s", "Miguel");
	emit_output_actual_header();
	emit_retval("%s", a.c_str());
	if(strcmp(a.c_str(), "Miguel") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_trim_end() {
	emit_test("Test trim() on a std::string with white space at the end.");
	std::string a("Hinault   ");
	trim(a);
	emit_output_expected_header();
	emit_retval("%s", "Hinault");
	emit_output_actual_header();
	emit_retval("%s", a.c_str());
	if(strcmp(a.c_str(), "Hinault") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_trim_none() {
	emit_test("Test trim() on a std::string with no white space.");
	std::string a("Indurain");
	trim(a);
	emit_output_expected_header();
	emit_retval("%s", "Indurain");
	emit_output_actual_header();
	emit_retval("%s", a.c_str());
	if(strcmp(a.c_str(), "Indurain") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_trim_beginning() {
	emit_test("Test trim() on a std::string with white space at the beginning.");
	std::string a("   Merckx");
	trim(a);
	emit_output_expected_header();
	emit_retval("%s", "Merckx");
	emit_output_actual_header();
	emit_retval("%s", a.c_str());
	if(strcmp(a.c_str(), "Merckx") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_trim_empty() {
	emit_test("Test trim() on an empty std::string.");
	std::string a;
	trim(a);
	emit_output_expected_header();
	emit_retval("%s", "");
	emit_output_actual_header();
	emit_retval("%s", a.c_str());
	if(strcmp(a.c_str(), "") != MATCH) {
		FAIL;
	}
	PASS;
}
