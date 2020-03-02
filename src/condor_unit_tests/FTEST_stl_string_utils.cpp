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
	This code tests some of our string manipulation utility functions.
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
static bool test_formatstr_cat_string(void);
static bool test_formatstr_cat_MyString(void);
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
static bool tokenize_null(void);
static bool tokenize_skip(void);
static bool tokenize_multiple_calls(void);
static bool tokenize_end(void);
static bool tokenize_empty(void);
static bool tokenize_empty_delimiter(void);
static bool escape_chars_length_1();
static bool escape_chars_empty();
static bool escape_chars_multiple();
static bool escape_chars_same();

bool FTEST_stl_string_utils(void) {
	emit_function("STL string utils");
	emit_comment("Package of functions/operators to facilitate adoption of std::string");
	
		// driver to run the tests and all required setup
	FunctionDriver driver;
	driver.register_function(test_comparison_ops_lhs_string);
	driver.register_function(test_comparison_ops_lhs_MyString);
	driver.register_function(test_sprintf_string);
	driver.register_function(test_sprintf_MyString);
	driver.register_function(test_formatstr_cat_string);
	driver.register_function(test_formatstr_cat_MyString);
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
	driver.register_function(tokenize_null);
	driver.register_function(tokenize_skip);
	driver.register_function(tokenize_multiple_calls);
	driver.register_function(tokenize_end);
	driver.register_function(tokenize_empty);
	driver.register_function(tokenize_empty_delimiter);
	driver.register_function(escape_chars_length_1);
	driver.register_function(escape_chars_empty);
	driver.register_function(escape_chars_multiple);
	driver.register_function(escape_chars_same);
	
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
    rchars = formatstr(dst, "%s", src.c_str());
    if (dst != src) {
		FAIL;        
    }
    if (rchars != nchars) {
		FAIL;        
    }

    nchars = STL_STRING_UTILS_FIXBUF - 1;
    src="";
    for (int j = 0;  j < nchars;  ++j) src += char('0' + (j % 10));
    rchars = formatstr(dst, "%s", src.c_str());
    if (dst != src) {
		FAIL;        
    }
    if (rchars != nchars) {
		FAIL;        
    }

    nchars = STL_STRING_UTILS_FIXBUF;
    src="";
    for (int j = 0;  j < nchars;  ++j) src += char('0' + (j % 10));
    rchars = formatstr(dst, "%s", src.c_str());
    if (dst != src) {
		FAIL;        
    }
    if (rchars != nchars) {
		FAIL;        
    }

    nchars = STL_STRING_UTILS_FIXBUF + 1;
    src="";
    for (int j = 0;  j < nchars;  ++j) src += char('0' + (j % 10));
    rchars = formatstr(dst, "%s", src.c_str());
    if (dst != src) {
		FAIL;        
    }
    if (rchars != nchars) {
		FAIL;        
    }

    nchars = STL_STRING_UTILS_FIXBUF * 2;
    src="";
    for (int j = 0;  j < nchars;  ++j) src += char('0' + (j % 10));
    rchars = formatstr(dst, "%s", src.c_str());
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
    rchars = formatstr(dst, "%s", src.Value());
    if (dst != src) {
		FAIL;        
    }
    if (rchars != nchars) {
		FAIL;        
    }

    nchars = STL_STRING_UTILS_FIXBUF - 1;
    src="";
    for (int j = 0;  j < nchars;  ++j) src += char('0' + (j % 10));
    rchars = formatstr(dst, "%s", src.Value());
    if (dst != src) {
		FAIL;        
    }
    if (rchars != nchars) {
		FAIL;        
    }

    nchars = STL_STRING_UTILS_FIXBUF;
    src="";
    for (int j = 0;  j < nchars;  ++j) src += char('0' + (j % 10));
    rchars = formatstr(dst, "%s", src.Value());
    if (dst != src) {
		FAIL;        
    }
    if (rchars != nchars) {
		FAIL;        
    }

    nchars = STL_STRING_UTILS_FIXBUF + 1;
    src="";
    for (int j = 0;  j < nchars;  ++j) src += char('0' + (j % 10));
    rchars = formatstr(dst, "%s", src.Value());
    if (dst != src) {
		FAIL;        
    }
    if (rchars != nchars) {
		FAIL;        
    }

    nchars = STL_STRING_UTILS_FIXBUF * 2;
    src="";
    for (int j = 0;  j < nchars;  ++j) src += char('0' + (j % 10));
    rchars = formatstr(dst, "%s", src.Value());
    if (dst != src) {
		FAIL;        
    }
    if (rchars != nchars) {
		FAIL;        
    }

	PASS;
}

static bool test_formatstr_cat_string() {
    emit_test("Test formatstr_cat overloading for std::string");

    std::string s = "foo";
    int r = formatstr_cat(s, "%s", "bar");
    if (s != "foobar") {
		FAIL;        
    }
    if (r != 3) {
		FAIL;        
    }

	PASS;
}

static bool test_formatstr_cat_MyString() {
    emit_test("Test formatstr_cat overloading for MyString");

    MyString s = "foo";
    int r = formatstr_cat(s, "%s", "bar");
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
	std::string a="\0";
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

static bool tokenize_null() {
	emit_test("Does calling GetNextToken() before calling Tokenize() return "
		"NULL?");
	const char* tok = GetNextToken(",", false);
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

static bool tokenize_skip() {
	emit_test("Test GetNextToken() when skipping blank tokens.");
	const char *a = "     Ottavio Bottechia_";
	Tokenize(a);
	const char* tok = GetNextToken(" ", true);
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

static bool tokenize_multiple_calls() {
	emit_test("Test multiple calls to GetNextToken().");
	const char *a = "To  be or not to be; that is the question";
	Tokenize(a);
	const char* expectedTokens[] = {"To", "", "be", "or", "not", "to", "be", ""
		, "that", "is", "the", "question"};
	const char* resultToken0 = GetNextToken(" ;", false);
	const char* resultToken1 = GetNextToken(" ;", false);
	const char* resultToken2 = GetNextToken(" ;", false);
	const char* resultToken3 = GetNextToken(" ;", false);
	const char* resultToken4 = GetNextToken(" ;", false);
	const char* resultToken5 = GetNextToken(" ;", false);
	const char* resultToken6 = GetNextToken(" ;", false);
	const char* resultToken7 = GetNextToken(" ;", false);
	const char* resultToken8 = GetNextToken(" ;", false);
	const char* resultToken9 = GetNextToken(" ;", false);
	const char* resultToken10 = GetNextToken(" ;", false);
	const char* resultToken11 = GetNextToken(" ;", false);
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

static bool tokenize_end() {
	emit_test("Test GetNextToken() after getting to the end.");
	const char *a = "foo;";
	Tokenize(a);
	const char* tok = GetNextToken(";", false);
	tok = GetNextToken(";", false);
	tok = GetNextToken(";", false);
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

static bool tokenize_empty() {
	emit_test("Test GetNextToken() on an empty MyString.");
	Tokenize("");
	const char* tok = GetNextToken(" ", false);
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

static bool tokenize_empty_delimiter() {
	emit_test("Test GetNextToken() on an empty delimiter string.");
	const char *a = "foobar";
	Tokenize(a);
	const char* tok = GetNextToken("", false);
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

static bool escape_chars_length_1() {
	emit_test("Test EscapeChars() on a source string of length 1.");
	std::string a = EscapeChars("z", "z", '\\');
	emit_input_header();
	emit_param("STRING", "%s", "z");
	emit_param("CHAR", "%c", '\\');
	emit_output_expected_header();
	emit_retval("%s", "\\z");
	emit_output_actual_header();
	emit_retval("%s", a.c_str());
	if(strcmp(a.c_str(), "\\z") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool escape_chars_empty() {
	emit_test("Test EscapeChars() on an empty source string.");
	std::string a = EscapeChars("", "z", '\\');
	emit_input_header();
	emit_param("STRING", "%s", "z");
	emit_param("CHAR", "%c", '\\');
	emit_output_expected_header();
	emit_retval("%s", "");
	emit_output_actual_header();
	emit_retval("%s", a.c_str());
	if(strcmp(a.c_str(), "") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool escape_chars_multiple() {
	emit_test("Test EscapeChars() when passed many escape chars.");
	std::string a = EscapeChars("foobar", "oa", '*');
	emit_input_header();
	emit_param("STRING", "%s", "oa");
	emit_param("CHAR", "%c", '*');
	emit_output_expected_header();
	emit_retval("%s", "f*o*ob*ar");
	emit_output_actual_header();
	emit_retval("%s", a.c_str());
	if(strcmp(a.c_str(), "f*o*ob*ar") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool escape_chars_same() {
	emit_test("Test EscapeChars() when passed the same string as escape "
		"chars.");
	std::string a = EscapeChars("foobar", "foobar", '*');
	emit_input_header();
	emit_param("STRING", "%s", "foobar");
	emit_param("CHAR", "%c", '*');
	emit_output_expected_header();
	emit_retval("%s", "*f*o*o*b*a*r");
	emit_output_actual_header();
	emit_retval("%s", a.c_str());
	if(strcmp(a.c_str(), "*f*o*o*b*a*r") != MATCH) {
		FAIL;
	}
	PASS;
}
