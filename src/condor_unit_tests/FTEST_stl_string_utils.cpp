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

bool test_sprintf_string();
bool test_sprintf_MyString();
bool test_sprintf_cat_string();
bool test_sprintf_cat_MyString();
bool test_comparison_ops_lhs_string();
bool test_comparison_ops_lhs_MyString();

bool FTEST_stl_string_utils(void) {
		// beginning junk for getPortFromAddr()
	e.emit_function("STL string utils");
	e.emit_comment("Package of functions/operators to facilitate adoption of std::string");
	
		// driver to run the tests and all required setup
	FunctionDriver driver(10);
	driver.register_function(test_comparison_ops_lhs_string);
	driver.register_function(test_comparison_ops_lhs_MyString);
	driver.register_function(test_sprintf_string);
	driver.register_function(test_sprintf_MyString);
	driver.register_function(test_sprintf_cat_string);
	driver.register_function(test_sprintf_cat_MyString);
	
		// run the tests
	bool test_result = driver.do_all_functions();
	e.emit_function_break();
	return test_result;
}


bool test_sprintf_string() {
    e.emit_test("Test sprintf overloading for std::string");

    int nchars = 0;
    int rchars = 0;
    std::string dst;
    std::string src;

    nchars = STL_STRING_UTILS_FIXBUF / 2;
    src="";
    for (int j = 0;  j < nchars;  ++j) src += char('0' + (j % 10));
    rchars = sprintf(dst, "%s", src.c_str());
    if (dst != src) {
		e.emit_result_failure(__LINE__);
		return false;        
    }
    if (rchars != nchars) {
		e.emit_result_failure(__LINE__);
		return false;        
    }

    nchars = STL_STRING_UTILS_FIXBUF - 1;
    src="";
    for (int j = 0;  j < nchars;  ++j) src += char('0' + (j % 10));
    rchars = sprintf(dst, "%s", src.c_str());
    if (dst != src) {
		e.emit_result_failure(__LINE__);
		return false;        
    }
    if (rchars != nchars) {
		e.emit_result_failure(__LINE__);
		return false;        
    }

    nchars = STL_STRING_UTILS_FIXBUF;
    src="";
    for (int j = 0;  j < nchars;  ++j) src += char('0' + (j % 10));
    rchars = sprintf(dst, "%s", src.c_str());
    if (dst != src) {
		e.emit_result_failure(__LINE__);
		return false;        
    }
    if (rchars != nchars) {
		e.emit_result_failure(__LINE__);
		return false;        
    }

    nchars = STL_STRING_UTILS_FIXBUF + 1;
    src="";
    for (int j = 0;  j < nchars;  ++j) src += char('0' + (j % 10));
    rchars = sprintf(dst, "%s", src.c_str());
    if (dst != src) {
		e.emit_result_failure(__LINE__);
		return false;        
    }
    if (rchars != nchars) {
		e.emit_result_failure(__LINE__);
		return false;        
    }

    nchars = STL_STRING_UTILS_FIXBUF * 2;
    src="";
    for (int j = 0;  j < nchars;  ++j) src += char('0' + (j % 10));
    rchars = sprintf(dst, "%s", src.c_str());
    if (dst != src) {
		e.emit_result_failure(__LINE__);
		return false;        
    }
    if (rchars != nchars) {
		e.emit_result_failure(__LINE__);
		return false;        
    }

	e.emit_result_success(__LINE__);
	return true;
}

bool test_sprintf_MyString() {
    e.emit_test("Test sprintf overloading for MyString");

    int nchars = 0;
    int rchars = 0;
    MyString dst;
    MyString src;

    nchars = STL_STRING_UTILS_FIXBUF / 2;
    src="";
    for (int j = 0;  j < nchars;  ++j) src += char('0' + (j % 10));
    rchars = sprintf(dst, "%s", src.Value());
    if (dst != src) {
		e.emit_result_failure(__LINE__);
		return false;        
    }
    if (rchars != nchars) {
		e.emit_result_failure(__LINE__);
		return false;        
    }

    nchars = STL_STRING_UTILS_FIXBUF - 1;
    src="";
    for (int j = 0;  j < nchars;  ++j) src += char('0' + (j % 10));
    rchars = sprintf(dst, "%s", src.Value());
    if (dst != src) {
		e.emit_result_failure(__LINE__);
		return false;        
    }
    if (rchars != nchars) {
		e.emit_result_failure(__LINE__);
		return false;        
    }

    nchars = STL_STRING_UTILS_FIXBUF;
    src="";
    for (int j = 0;  j < nchars;  ++j) src += char('0' + (j % 10));
    rchars = sprintf(dst, "%s", src.Value());
    if (dst != src) {
		e.emit_result_failure(__LINE__);
		return false;        
    }
    if (rchars != nchars) {
		e.emit_result_failure(__LINE__);
		return false;        
    }

    nchars = STL_STRING_UTILS_FIXBUF + 1;
    src="";
    for (int j = 0;  j < nchars;  ++j) src += char('0' + (j % 10));
    rchars = sprintf(dst, "%s", src.Value());
    if (dst != src) {
		e.emit_result_failure(__LINE__);
		return false;        
    }
    if (rchars != nchars) {
		e.emit_result_failure(__LINE__);
		return false;        
    }

    nchars = STL_STRING_UTILS_FIXBUF * 2;
    src="";
    for (int j = 0;  j < nchars;  ++j) src += char('0' + (j % 10));
    rchars = sprintf(dst, "%s", src.Value());
    if (dst != src) {
		e.emit_result_failure(__LINE__);
		return false;        
    }
    if (rchars != nchars) {
		e.emit_result_failure(__LINE__);
		return false;        
    }

	e.emit_result_success(__LINE__);
	return true;
}

bool test_sprintf_cat_string() {
    e.emit_test("Test sprintf_cat overloading for std::string");

    std::string s = "foo";
    int r = sprintf_cat(s, "%s", "bar");
    if (s != "foobar") {
		e.emit_result_failure(__LINE__);
		return false;        
    }
    if (r != 3) {
		e.emit_result_failure(__LINE__);
		return false;        
    }

	e.emit_result_success(__LINE__);
	return true;
}

bool test_sprintf_cat_MyString() {
    e.emit_test("Test sprintf_cat overloading for MyString");

    MyString s = "foo";
    int r = sprintf_cat(s, "%s", "bar");
    if (s != "foobar") {
		e.emit_result_failure(__LINE__);
		return false;        
    }
    if (r != 3) {
		e.emit_result_failure(__LINE__);
		return false;        
    }    

	e.emit_result_success(__LINE__);
	return true;
}

bool test_comparison_ops_lhs_string() {
    e.emit_test("Test comparison operators: LHS is std::string and RHS is MyString");

    std::string lhs;
    MyString rhs;

        // test operators when lhs == rhs
    lhs = "a";
    rhs = "a";
    if ((lhs == rhs) != true) {
		e.emit_result_failure(__LINE__);
		return false;        
    }
    if ((lhs != rhs) != false) {
		e.emit_result_failure(__LINE__);
		return false;        
    }
    if ((lhs < rhs) != false) {
		e.emit_result_failure(__LINE__);
		return false;        
    }
    if ((lhs > rhs) != false) {
		e.emit_result_failure(__LINE__);
		return false;        
    }
    if ((lhs <= rhs) != true) {
		e.emit_result_failure(__LINE__);
		return false;        
    }
    if ((lhs >= rhs) != true) {
		e.emit_result_failure(__LINE__);
		return false;        
    }

        // test operators when lhs < rhs
    lhs = "a";
    rhs = "b";
    if ((lhs == rhs) != false) {
		e.emit_result_failure(__LINE__);
		return false;        
    }
    if ((lhs != rhs) != true) {
		e.emit_result_failure(__LINE__);
		return false;        
    }
    if ((lhs < rhs) != true) {
		e.emit_result_failure(__LINE__);
		return false;        
    }
    if ((lhs > rhs) != false) {
		e.emit_result_failure(__LINE__);
		return false;        
    }
    if ((lhs <= rhs) != true) {
		e.emit_result_failure(__LINE__);
		return false;        
    }
    if ((lhs >= rhs) != false) {
		e.emit_result_failure(__LINE__);
		return false;        
    }

        // test operators when lhs > rhs
    lhs = "b";
    rhs = "a";
    if ((lhs == rhs) != false) {
		e.emit_result_failure(__LINE__);
		return false;        
    }
    if ((lhs != rhs) != true) {
		e.emit_result_failure(__LINE__);
		return false;        
    }
    if ((lhs < rhs) != false) {
		e.emit_result_failure(__LINE__);
		return false;        
    }
    if ((lhs > rhs) != true) {
		e.emit_result_failure(__LINE__);
		return false;        
    }
    if ((lhs <= rhs) != false) {
		e.emit_result_failure(__LINE__);
		return false;        
    }
    if ((lhs >= rhs) != true) {
		e.emit_result_failure(__LINE__);
		return false;        
    }

	e.emit_result_success(__LINE__);
	return true;
}

bool test_comparison_ops_lhs_MyString() {
    e.emit_test("Test comparison operators: LHS is MyString and LHS is std::string");

    MyString lhs;
    std::string rhs;

        // test operators when lhs == rhs
    lhs = "a";
    rhs = "a";
    if ((lhs == rhs) != true) {
		e.emit_result_failure(__LINE__);
		return false;        
    }
    if ((lhs != rhs) != false) {
		e.emit_result_failure(__LINE__);
		return false;        
    }
    if ((lhs < rhs) != false) {
		e.emit_result_failure(__LINE__);
		return false;        
    }
    if ((lhs > rhs) != false) {
		e.emit_result_failure(__LINE__);
		return false;        
    }
    if ((lhs <= rhs) != true) {
		e.emit_result_failure(__LINE__);
		return false;        
    }
    if ((lhs >= rhs) != true) {
		e.emit_result_failure(__LINE__);
		return false;        
    }

        // test operators when lhs < rhs
    lhs = "a";
    rhs = "b";
    if ((lhs == rhs) != false) {
		e.emit_result_failure(__LINE__);
		return false;        
    }
    if ((lhs != rhs) != true) {
		e.emit_result_failure(__LINE__);
		return false;        
    }
    if ((lhs < rhs) != true) {
		e.emit_result_failure(__LINE__);
		return false;        
    }
    if ((lhs > rhs) != false) {
		e.emit_result_failure(__LINE__);
		return false;        
    }
    if ((lhs <= rhs) != true) {
		e.emit_result_failure(__LINE__);
		return false;        
    }
    if ((lhs >= rhs) != false) {
		e.emit_result_failure(__LINE__);
		return false;        
    }

        // test operators when lhs > rhs
    lhs = "b";
    rhs = "a";
    if ((lhs == rhs) != false) {
		e.emit_result_failure(__LINE__);
		return false;        
    }
    if ((lhs != rhs) != true) {
		e.emit_result_failure(__LINE__);
		return false;        
    }
    if ((lhs < rhs) != false) {
		e.emit_result_failure(__LINE__);
		return false;        
    }
    if ((lhs > rhs) != true) {
		e.emit_result_failure(__LINE__);
		return false;        
    }
    if ((lhs <= rhs) != false) {
		e.emit_result_failure(__LINE__);
		return false;        
    }
    if ((lhs >= rhs) != true) {
		e.emit_result_failure(__LINE__);
		return false;        
    }

	e.emit_result_success(__LINE__);
	return true;
}
