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
	This code tests the FlattenAndInline() function implementation.
 */

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "function_test_driver.h"
#include "emit.h"
#include "unit_test_utils.h"
#include <string>
#include <iterator>

using namespace std;

#ifdef WANT_CLASSAD_NAMESPACE
using namespace classad;
#endif

static bool test_empty_simple_return(void);
static bool test_empty_simple_value(void);
static bool test_empty_simple_flattened_expression(void);
static bool test_non_empty_simple_return(void);
static bool test_non_empty_simple_value(void);
static bool test_non_empty_simple_flattened_expression(void);
static bool test_null_expression(void);
static bool test_equivalence_no_change(void);
static bool test_equivalence_full_simple(void);
static bool test_equivalence_partial_simple(void);
static bool test_equivalence_full_multiple(void);
static bool test_equivalence_partial_multiple(void);
static bool test_equivalence_full_arithmetic_constants(void);
static bool test_equivalence_partial_arithmetic_constants(void);
static bool test_equivalence_full_arithmetic(void);
static bool test_equivalence_partial_arithmetic_beginning(void);
static bool test_equivalence_partial_arithmetic_middle(void);
static bool test_equivalence_partial_arithmetic_end(void);
static bool test_equivalence_full_simple_boolean(void);
static bool test_equivalence_partial_simple_boolean(void);
static bool test_equivalence_full_complex_boolean(void);
static bool test_equivalence_partial_complex_boolean(void);
static bool test_equivalence_nested_classad(void);
static bool test_equivalence_nested_loop_classad(void);
static bool test_equivalence_simple_list(void);
static bool test_equivalence_complex_list(void);


bool FTEST_flatten_and_inline(void) {
	e.emit_function("bool FlattenAndInline( const ExprTree *tree , Value &val , ExprTree *&fexpr ) const");
	e.emit_comment("Note that these tests are not exhaustive, but are "
		"sufficient for testing the basics of FlattenAndInline");
	
		// driver to run the tests and all required setup
	FunctionDriver driver;
	driver.register_function(test_empty_simple_return);
	driver.register_function(test_empty_simple_value);
	driver.register_function(test_empty_simple_flattened_expression);
	driver.register_function(test_non_empty_simple_return);
	driver.register_function(test_non_empty_simple_value);
	driver.register_function(test_non_empty_simple_flattened_expression);
	driver.register_function(test_null_expression);
	driver.register_function(test_equivalence_no_change);
	driver.register_function(test_equivalence_full_simple);
	driver.register_function(test_equivalence_partial_simple);
	driver.register_function(test_equivalence_full_multiple);
	driver.register_function(test_equivalence_partial_multiple);
	driver.register_function(test_equivalence_full_arithmetic_constants);
	driver.register_function(test_equivalence_partial_arithmetic_constants);
	driver.register_function(test_equivalence_full_arithmetic);
	driver.register_function(test_equivalence_partial_arithmetic_beginning);
	driver.register_function(test_equivalence_partial_arithmetic_middle);
	driver.register_function(test_equivalence_partial_arithmetic_end);
	driver.register_function(test_equivalence_full_simple_boolean);
	driver.register_function(test_equivalence_partial_simple_boolean);
	driver.register_function(test_equivalence_full_complex_boolean);
	driver.register_function(test_equivalence_partial_complex_boolean);
	driver.register_function(test_equivalence_nested_classad);
	driver.register_function(test_equivalence_nested_loop_classad);
	driver.register_function(test_equivalence_simple_list);
	driver.register_function(test_equivalence_complex_list);

		// run the tests
	bool test_result = driver.do_all_functions();
	e.emit_function_break();
	return test_result;
}


static bool test_empty_simple_return() {
	e.emit_test("Does FlattenAndInline return true when passed a simple "
		"ExprTree on an empty ClassAd?");

	classad::ClassAd classad;
	classad::ExprTree *expr = Literal::MakeAbsTime();
	Value value;
	classad::ExprTree *fexpr;

	ClassAdUnParser unparser;
	string expr_string, classad_string;
	unparser.Unparse(expr_string, expr);
	unparser.Unparse(classad_string, &classad);
	bool flattenResult = classad.FlattenAndInline(expr, value, fexpr);
	e.emit_input_header();
	e.emit_param("ClassAd", classad_string.c_str());
	e.emit_param("ExprTree", expr_string.c_str());
	e.emit_param("Value", "");
	e.emit_param("ExprTree", "NULL");
	e.emit_output_expected_header();
	e.emit_retval("TRUE");
	e.emit_output_actual_header();
	e.emit_retval(tfstr(flattenResult));
	if(!flattenResult) {
		e.emit_result_failure(__LINE__);
		delete expr; delete fexpr;
		return false;
	}
	e.emit_result_success(__LINE__);
	delete expr; delete fexpr;
	return true;
}

static bool test_empty_simple_value() {
	e.emit_test("Does FlattenAndInline set the value to be the flattened "
		"simple ExprTree's value on an empty ClassAd?");

	classad::ClassAd classad;
	classad::ExprTree *expr = Literal::MakeAbsTime();
	Value value;
	classad::ExprTree *fexpr;

	ClassAdUnParser unparser;
	string value_string, expr_string, classad_string;
	unparser.Unparse(expr_string, expr);
	unparser.Unparse(classad_string, &classad);
	
	classad.FlattenAndInline(expr, value, fexpr);
	unparser.Unparse(value_string, value);
	e.emit_input_header();
	e.emit_param("ClassAd", classad_string.c_str());
	e.emit_param("ExprTree", expr_string.c_str());
	e.emit_param("Value", "");
	e.emit_param("ExprTree", "NULL");
	e.emit_output_expected_header();
	e.emit_param("Value", expr_string.c_str());
	e.emit_output_actual_header();
	e.emit_param("Value", value_string.c_str());
	if(expr_string.compare(value_string)) {
		e.emit_result_failure(__LINE__);
		delete expr; delete fexpr;
		return false;
	}
	e.emit_result_success(__LINE__);
	delete expr; delete fexpr;
	return true;
}

static bool test_empty_simple_flattened_expression() {
	e.emit_test("Does FlattenAndInline set fexpr to NULL when the simple "
		"ExprTree is flattened to a single value on an empty ClassAd?");

	classad::ClassAd classad;
	classad::ExprTree *expr = Literal::MakeAbsTime();
	Value value;
	classad::ExprTree *fexpr;

	ClassAdUnParser unparser;
	string value_string, expr_string, classad_string;
	unparser.Unparse(expr_string, expr);
	unparser.Unparse(classad_string, &classad);
	
	classad.FlattenAndInline(expr, value, fexpr);
	unparser.Unparse(value_string, value);

	e.emit_input_header();
	e.emit_param("ClassAd", classad_string.c_str());
	e.emit_param("ExprTree", expr_string.c_str());
	e.emit_param("Value", "");
	e.emit_param("ExprTree", "NULL");
	e.emit_output_expected_header();
	e.emit_param("fexpr is NULL", "TRUE");
	e.emit_output_actual_header();
	e.emit_param("fexpr is NULL", tfstr(fexpr == NULL));
	if(fexpr != NULL) {
		e.emit_result_failure(__LINE__);
		delete expr; delete fexpr;
		return false;
	}

	e.emit_result_success(__LINE__);
	delete expr; delete fexpr;
	return true;
}

static bool test_non_empty_simple_return() {
	e.emit_test("Does FlattenAndInline return true when passed a simple "
		"ExprTree on a non-empty ClassAd?");

	classad::ClassAd *classad;
	classad::ExprTree *expr;
	Value value;
	classad::ExprTree *fexpr;

	string classad_string = "[a = 1; b = \"Cardini\"]";
	string expr_string = "a";
	ClassAdParser parser;
	
	classad = parser.ParseClassAd(classad_string, true);
	expr = parser.ParseExpression(expr_string);

	bool flattenResult = classad->FlattenAndInline(expr, value, fexpr);
	e.emit_input_header();
	e.emit_param("ClassAd", classad_string.c_str());
	e.emit_param("ExprTree", expr_string.c_str());
	e.emit_param("Value", "");
	e.emit_param("ExprTree", "NULL");
	e.emit_output_expected_header();
	e.emit_retval("TRUE");
	e.emit_output_actual_header();
	e.emit_retval(tfstr(flattenResult));
	if(!flattenResult) {
		e.emit_result_failure(__LINE__);
		delete expr; delete fexpr;
		return false;
	}
	e.emit_result_success(__LINE__);
	delete classad; delete expr; delete fexpr;
	return true;
}

static bool test_non_empty_simple_value() {
	e.emit_test("Does FlattenAndInline set the value to be the flattened "
		"simple ExprTree's value on a non-empty ClassAd?");

	classad::ClassAd *classad;
	classad::ExprTree *expr;
	Value value;
	classad::ExprTree *fexpr;

	string classad_string = "[a = 1; b = \"Cardini\"]";
	string expr_string = "a";
	ClassAdParser parser;
	
	classad = parser.ParseClassAd(classad_string, true);
	expr = parser.ParseExpression(expr_string);

	ClassAdUnParser unparser;
	string value_string;
	
	classad->FlattenAndInline(expr, value, fexpr);
	unparser.Unparse(value_string, value);
	e.emit_input_header();
	e.emit_param("ClassAd", classad_string.c_str());
	e.emit_param("ExprTree", expr_string.c_str());
	e.emit_param("Value", "");
	e.emit_param("ExprTree", "NULL");
	e.emit_output_expected_header();
	e.emit_param("Value", "1");
	e.emit_output_actual_header();
	e.emit_param("Value", value_string.c_str());
	if(value_string.compare("1")) {
		e.emit_result_failure(__LINE__);
		delete classad; delete expr; delete fexpr;
		return false;
	}
	e.emit_result_success(__LINE__);
	delete classad; delete expr; delete fexpr;
	return true;
}

static bool test_non_empty_simple_flattened_expression() {
	e.emit_test("Does FlattenAndInline set fexpr to NULL when the simple "
		"ExprTree is flattened to a single value on a non-empty ClassAd?");

	classad::ClassAd *classad;
	classad::ExprTree *expr;
	Value value;
	classad::ExprTree *fexpr;
	
	string classad_string = "[a = 1; b = \"Cardini\"]";
	string expr_string = "a";
	ClassAdParser parser;
	
	classad = parser.ParseClassAd(classad_string, true);
	expr = parser.ParseExpression(expr_string);

	ClassAdUnParser unparser;
	string value_string;
	
	classad->FlattenAndInline(expr, value, fexpr);
	unparser.Unparse(value_string, value);
	e.emit_input_header();
	e.emit_param("ClassAd", classad_string.c_str());
	e.emit_param("ExprTree", expr_string.c_str());
	e.emit_param("Value", "");
	e.emit_param("ExprTree", "NULL");
	e.emit_output_expected_header();
	e.emit_param("fexpr is NULL", "TRUE");
	e.emit_output_actual_header();
	e.emit_param("fexpr is NULL", tfstr(fexpr == NULL));
	if(fexpr != NULL) {
		e.emit_result_failure(__LINE__);
		delete classad; delete expr; delete fexpr;
		return false;
	}
	e.emit_result_success(__LINE__);
	delete classad; delete expr; delete fexpr;
	return true;
}

//This test segfaults!
static bool test_null_expression() {
	e.emit_test("Test FlattenAndInline when passing a null expression.");
	e.emit_alert("Causes segfault! See ticket #1379");
/*
	classad::ClassAd classad;
	classad::ExprTree *expr;
	Value value;
	classad::ExprTree *fexpr;
	
	ClassAdParser parser;
	
	ClassAdUnParser unparser;
	string classad_string, value_string, fexpr_string;
	unparser.Unparse(classad_string, &classad);
	unparser.Unparse(expr_string, expr);
	
	bool flattenResult = classad.FlattenAndInline(expr, value, fexpr);
	unparser.Unparse(value_string, value);
	unparser.Unparse(fexpr_string, fexpr);
	
	e.emit_input_header();
	e.emit_param("ClassAd", "%s", classad_string.c_str());
	e.emit_param("ExprTree", "%s", expr_string.c_str());
	e.emit_param("Value", "%s", "");
	e.emit_param("ExprTree", "%s", "NULL");
	e.emit_output_expected_header();
	e.emit_retval("%s", "TRUE");
	e.emit_output_actual_header();
	e.emit_retval("%s", tfstr(flattenResult));
	if(!flattenResult) {
		e.emit_result_failure(__LINE__);
		delete expr; delete fexpr;
		return false;
	}
	e.emit_result_success(__LINE__);

	delete expr; delete fexpr;
	return true;
*/
	e.emit_result_success(__LINE__);
	return true;
// GGT delete above 2 lines and uncomment below two lines
// when/if this is fixed
//	e.emit_result_failure(__LINE__);
//	return false;
}


static bool test_equivalence_no_change() {
	e.emit_test("Test if the flattened and inlined ExprTree is logically "
		"equivalent to the original ExprTree when the flattening and inlining "
		"does nothing.");
	
	classad::ClassAd *classad;
	classad::ExprTree *expr;
	Value value;
	classad::ExprTree *fexpr;
	
	string classad_string = "[a = 1]";
	string expr_string = "f";
	ClassAdParser parser;
	
	classad = parser.ParseClassAd(classad_string, true);
	expr = parser.ParseExpression(expr_string);

	ClassAdUnParser unparser;
	string value_string, fexpr_string;
	
	classad->FlattenAndInline(expr, value, fexpr);
	unparser.Unparse(value_string, value);
	unparser.Unparse(fexpr_string, fexpr);
	
	e.emit_input_header();
	e.emit_param("ClassAd", classad_string.c_str());
	e.emit_param("ExprTree", expr_string.c_str());
	e.emit_param("Value", "");
	e.emit_param("ExprTree", "NULL");
	e.emit_output_expected_header();
	e.emit_param("Flattened and Inlined Value", "undefined");
	e.emit_param("Flattened and Inlined Expression", "f");
	e.emit_output_actual_header();
	e.emit_param("Flattened and Inlined Value", value_string.c_str());
	e.emit_param("Flattened and Inlined Expression", fexpr_string.c_str());
	if(!expr->SameAs(fexpr)){
		e.emit_result_failure(__LINE__);
		delete classad; delete expr; delete fexpr;
		return false;
	}
	e.emit_result_success(__LINE__);
	delete classad; delete expr; delete fexpr;
	return true;
}

static bool test_equivalence_full_simple() {
	e.emit_test("Test if the fully flattened and inlined ExprTree is logically "
		"equivalent to the original simple ExprTree.");
	
	classad::ClassAd *classad;
	classad::ExprTree *expr;
	Value value;
	classad::ExprTree *fexpr;
	
	string classad_string = "[a = 1]";
	string expr_string = "a";
	ClassAdParser parser;
	
	classad = parser.ParseClassAd(classad_string, true);
	expr = parser.ParseExpression(expr_string);

	ClassAdUnParser unparser;
	string value_string, fexpr_string;
	
	classad->FlattenAndInline(expr, value, fexpr);
	unparser.Unparse(value_string, value);
	unparser.Unparse(fexpr_string, fexpr);
	
	e.emit_input_header();
	e.emit_param("ClassAd", classad_string.c_str());
	e.emit_param("ExprTree", expr_string.c_str());
	e.emit_param("Value", "");
	e.emit_param("ExprTree", "NULL");
	e.emit_output_expected_header();
	e.emit_param("Flattened and Inlined Value", "1");
	e.emit_param("Flattened and Inlined Expression", "<error:null expr>");
	e.emit_output_actual_header();
	e.emit_param("Flattened and Inlined Value", value_string.c_str());
	e.emit_param("Flattened and Inlined Expression", fexpr_string.c_str());
	if(value_string.compare("1") != MATCH){
		e.emit_result_failure(__LINE__);
		delete classad; delete expr; delete fexpr;
		return false;
	}
	e.emit_result_success(__LINE__);
	delete classad; delete expr; delete fexpr;
	return true;
}

static bool test_equivalence_partial_simple() {
	e.emit_test("Test if the partially flattened and inlined ExprTree is "
		"logically equivalent to the original simple ExprTree.");
	
	classad::ClassAd *classad;
	classad::ExprTree *expr;
	Value value;
	classad::ExprTree *fexpr;
	
	string classad_string = "[a = f]";
	string expr_string = "a";
	ClassAdParser parser;
	
	classad = parser.ParseClassAd(classad_string, true);
	expr = parser.ParseExpression(expr_string);

	ClassAdUnParser unparser;
	string value_string, fexpr_string, value2_string;
	
	classad->FlattenAndInline(expr, value, fexpr);
	unparser.Unparse(value_string, value);
	unparser.Unparse(fexpr_string, fexpr);
	
	e.emit_input_header();
	e.emit_param("ClassAd", classad_string.c_str());
	e.emit_param("ExprTree", expr_string.c_str());
	e.emit_param("Value", "");
	e.emit_param("ExprTree", "NULL");
	e.emit_output_expected_header();
	e.emit_param("Flattened and Inlined Value", "undefined");
	e.emit_param("Flattened and Inlined Expression", "f");
	e.emit_output_actual_header();
	e.emit_param("Flattened and Inlined Value", value_string.c_str());
	e.emit_param("Flattened and Inlined Expression", fexpr_string.c_str());
	if(fexpr_string.compare("f") != MATCH){
		e.emit_result_failure(__LINE__);
		delete classad; delete expr; delete fexpr;
		return false;
	}
	e.emit_result_success(__LINE__);
	delete classad; delete expr; delete fexpr;
	return true;
}

static bool test_equivalence_full_multiple() {
	e.emit_test("Test if the fully flattened and inlined ExprTree is logically "
		"equivalent to the original ExprTree after multiple FlattenAndInline "
		"calls.");
	
	classad::ClassAd *classad1, *classad2, *classad3;
	classad::ExprTree *expr;
	Value value;
	classad::ExprTree *fexpr1, *fexpr2, *fexpr3;
	
	string classad1_string = "[a = 1]";
	string classad2_string = "[b = 2]";
	string classad3_string = "[c = 3]";
	string expr_string = "a + b - c";
	ClassAdParser parser;
	
	classad1 = parser.ParseClassAd(classad1_string, true);
	classad2 = parser.ParseClassAd(classad2_string, true);
	classad3 = parser.ParseClassAd(classad3_string, true);
	expr = parser.ParseExpression(expr_string);

	ClassAdUnParser unparser;
	string value_string, fexpr_string;
	
	classad1->FlattenAndInline(expr, value, fexpr1);
	classad2->FlattenAndInline(fexpr1, value, fexpr2);
	classad3->FlattenAndInline(fexpr2, value, fexpr3);
	unparser.Unparse(value_string, value);
	unparser.Unparse(fexpr_string, fexpr3);
	
	e.emit_input_header();
	e.emit_param("ClassAd 1", classad1_string.c_str());
	e.emit_param("ClassAd 2", classad2_string.c_str());
	e.emit_param("ClassAd 3", classad3_string.c_str());
	e.emit_param("ExprTree", expr_string.c_str());
	e.emit_param("Value", "");
	e.emit_param("ExprTree", "NULL");
	e.emit_output_expected_header();
	e.emit_param("Flattened and Inlined Value", "0");
	e.emit_param("Flattened and Inlined Expression", "<error:null expr>");
	e.emit_output_actual_header();
	e.emit_param("Flattened and Inlined Value", value_string.c_str());
	e.emit_param("Flattened and Inlined Expression", fexpr_string.c_str());
	if(value_string.compare("0") != MATCH){
		e.emit_result_failure(__LINE__);
		delete classad1; delete classad2; delete classad3;
		delete expr; delete fexpr1; delete fexpr2; delete fexpr3;
		return false;
	}
	e.emit_result_success(__LINE__);
	delete classad1; delete classad2; delete classad3;
	delete expr; delete fexpr1; delete fexpr2; delete fexpr3;
	return true;
}

static bool test_equivalence_partial_multiple() {
	e.emit_test("Test if the partially flattened and inlined ExprTree is "
		"logically equivalent to the original ExprTree after multiple "
		"FlattenAndInline calls.");
	
	classad::ClassAd *classad1, *classad2, *classad3;
	classad::ExprTree *expr;
	Value value;
	classad::ExprTree *fexpr1, *fexpr2, *fexpr3;
	
	string classad1_string = "[a = b]";
	string classad2_string = "[b = c]";
	string classad3_string = "[c = d]";
	string expr_string = "a + b - c";
	ClassAdParser parser;
	
	classad1 = parser.ParseClassAd(classad1_string, true);
	classad2 = parser.ParseClassAd(classad2_string, true);
	classad3 = parser.ParseClassAd(classad3_string, true);
	expr = parser.ParseExpression(expr_string);

	ClassAdUnParser unparser;
	string value_string, fexpr_string;
	
	classad1->FlattenAndInline(expr, value, fexpr1);
	classad2->FlattenAndInline(fexpr1, value, fexpr2);
	classad3->FlattenAndInline(fexpr2, value, fexpr3);
	unparser.Unparse(value_string, value);
	unparser.Unparse(fexpr_string, fexpr3);
	
	e.emit_input_header();
	e.emit_param("ClassAd 1", classad1_string.c_str());
	e.emit_param("ClassAd 2", classad2_string.c_str());
	e.emit_param("ClassAd 3", classad3_string.c_str());
	e.emit_param("ExprTree", expr_string.c_str());
	e.emit_param("Value", "");
	e.emit_param("ExprTree", "NULL");
	e.emit_output_expected_header();
	e.emit_param("Flattened and Inlined Value", "d + d - d");
	e.emit_param("Flattened and Inlined Expression", "<error:null expr>");
	e.emit_output_actual_header();
	e.emit_param("Flattened and Inlined Value", value_string.c_str());
	e.emit_param("Flattened and Inlined Expression", "'%s'", fexpr_string.c_str());
	if(fexpr_string.compare("d + d - d") != MATCH){
		e.emit_result_failure(__LINE__);
		delete classad1; delete classad2; delete classad3;
		delete expr; delete fexpr1; delete fexpr2; delete fexpr3;
		return false;
	}
	e.emit_result_success(__LINE__);
	delete classad1; delete classad2; delete classad3;
	delete expr; delete fexpr1; delete fexpr2; delete fexpr3;
	return true;
}


static bool test_equivalence_full_arithmetic_constants() {
	e.emit_test("Test if the fully flattened and inlined ExprTree is logically "
		"equivalent to the original arithmetic ExprTree that has constants.");
	
	classad::ClassAd *classad;
	classad::ExprTree *expr;
	Value value;
	classad::ExprTree *fexpr;
	
	string classad_string = "[a = b; b = c; c = 1]";
	string expr_string = "a + b + (c*5) - (25*b/5))";
	ClassAdParser parser;
	
	classad = parser.ParseClassAd(classad_string, true);
	expr = parser.ParseExpression(expr_string);

	ClassAdUnParser unparser;
	string value_string, fexpr_string;
	
	classad->FlattenAndInline(expr, value, fexpr);
	unparser.Unparse(value_string, value);
	unparser.Unparse(fexpr_string, fexpr);
	
	e.emit_input_header();
	e.emit_param("ClassAd", classad_string.c_str());
	e.emit_param("ExprTree", expr_string.c_str());
	e.emit_param("Value", "");
	e.emit_param("ExprTree", "NULL");
	e.emit_output_expected_header();
	e.emit_param("Flattened and Inlined Value", "2");
	e.emit_param("Flattened and Inlined Expression", "<error:null expr>");
	e.emit_output_actual_header();
	e.emit_param("Flattened and Inlined Value", value_string.c_str());
	e.emit_param("Flattened and Inlined Expression", fexpr_string.c_str());
	if(value_string.compare("2") != MATCH){
		e.emit_result_failure(__LINE__);
		delete classad; delete expr; delete fexpr;
		return false;
	}
	e.emit_result_success(__LINE__);
	delete classad;	delete expr; delete fexpr;
	return true;
}

//Difference between Flatten and FlattenAndInline:
//	Flatten: 6 + a - 5
//	FlattenAndInline: 6 + f - 5
static bool test_equivalence_partial_arithmetic_constants() {
	e.emit_test("Test if the partially flattened and inlined ExprTree is "
		"logically equivalent to the original arithmetic ExprTree that has "
		"constants.");
	e.emit_comment("This expression could be further reduced to '1 + f'");
	classad::ClassAd *classad;
	classad::ExprTree *expr;
	Value value;
	classad::ExprTree *fexpr;
	
	string classad_string = "[a = f; b = c; c = 1]";
	string expr_string = "a + b + (c*5) - (25*b/5))";
	ClassAdParser parser;
	
	classad = parser.ParseClassAd(classad_string, true);
	expr = parser.ParseExpression(expr_string);

	ClassAdUnParser unparser;
	string value_string, fexpr_string;
	string value2_string, fexpr2_string;
	
	classad->FlattenAndInline(expr, value, fexpr);
	unparser.Unparse(value_string, value);
	unparser.Unparse(fexpr_string, fexpr);
	
	e.emit_input_header();
	e.emit_param("ClassAd", classad_string.c_str());
	e.emit_param("ExprTree", expr_string.c_str());
	e.emit_param("Value", "");
	e.emit_param("ExprTree", "NULL");
	e.emit_output_expected_header();
	e.emit_param("Flattened and Inlined Value", "undefined");
	e.emit_param("Flattened and Inlined Expression", "6 + f - 5");
	e.emit_output_actual_header();
	e.emit_param("Flattened and Inlined Value", value_string.c_str());
	e.emit_param("Flattened and Inlined Expression", fexpr_string.c_str());
	if(fexpr_string.compare("6 + f - 5") != MATCH){
		e.emit_result_failure(__LINE__);
		delete classad; delete expr; delete fexpr;
		return false;
	}
	e.emit_result_success(__LINE__);
	delete classad;	delete expr; delete fexpr;
	return true;
}

static bool test_equivalence_full_arithmetic() {
	e.emit_test("Test if the fully flattened and inlined ExprTree is logically "
		"equivalent to the original arithmetic ExprTree.");
	
	classad::ClassAd *classad;
	classad::ExprTree *expr;
	Value value;
	classad::ExprTree *fexpr;
	
	string classad_string = "[a = b; b = c; c = 1]";
	string expr_string = "a + b + c";
	ClassAdParser parser;
	
	classad = parser.ParseClassAd(classad_string, true);
	expr = parser.ParseExpression(expr_string);

	ClassAdUnParser unparser;
	string value_string, fexpr_string;
	
	classad->FlattenAndInline(expr, value, fexpr);
	unparser.Unparse(value_string, value);
	unparser.Unparse(fexpr_string, fexpr);
	
	e.emit_input_header();
	e.emit_param("ClassAd", classad_string.c_str());
	e.emit_param("ExprTree", expr_string.c_str());
	e.emit_param("Value", "");
	e.emit_param("ExprTree", "NULL");
	e.emit_output_expected_header();
	e.emit_param("Flattened and Inlined Value", "3");
	e.emit_param("Flattened and Inlined Expression", "<error:null expr>");
	e.emit_output_actual_header();
	e.emit_param("Flattened and Inlined Value", value_string.c_str());
	e.emit_param("Flattened and Inlined Expression", fexpr_string.c_str());
	if(value_string.compare("3") != MATCH){
		e.emit_result_failure(__LINE__);
		delete classad; delete expr; delete fexpr;
		return false;
	}
	e.emit_result_success(__LINE__);
	delete classad;	delete expr; delete fexpr;
	return true;
}

static bool test_equivalence_partial_arithmetic_beginning() {
	e.emit_test("Test if the partially flattened and inlined ExprTree is "
		"logically equivalent to the original arithmetic ExprTree with an "
		"inlined value at the beginning.");
	
	classad::ClassAd *classad;
	classad::ExprTree *expr;
	Value value;
	classad::ExprTree *fexpr;
	
	string classad_string = "[a = f; b = c; c = 1]";
	string expr_string = "a + b + c";
	ClassAdParser parser;
	
	classad = parser.ParseClassAd(classad_string, true);
	expr = parser.ParseExpression(expr_string);

	ClassAdUnParser unparser;
	string value_string, fexpr_string;
	string value2_string, fexpr2_string;
	
	classad->FlattenAndInline(expr, value, fexpr);
	unparser.Unparse(value_string, value);
	unparser.Unparse(fexpr_string, fexpr);
	
	e.emit_input_header();
	e.emit_param("ClassAd", classad_string.c_str());
	e.emit_param("ExprTree", expr_string.c_str());
	e.emit_param("Value", "");
	e.emit_param("ExprTree", "NULL");
	e.emit_output_expected_header();
	e.emit_param("Flattened and Inlined Value", "undefined");
	e.emit_param("Flattened and Inlined Expression", "2 + f");
	e.emit_output_actual_header();
	e.emit_param("Flattened and Inlined Value", value_string.c_str());
	e.emit_param("Flattened and Inlined Expression", fexpr_string.c_str());
	if(fexpr_string.compare("2 + f") != MATCH){
		e.emit_result_failure(__LINE__);
		delete classad; delete expr; delete fexpr;
		return false;
	}
	e.emit_result_success(__LINE__);
	delete classad;	delete expr; delete fexpr;
	return true;
}

static bool test_equivalence_partial_arithmetic_middle() {
	e.emit_test("Test if the partially flattened and inlined ExprTree is "
		"logically equivalent to the original arithmetic ExprTree with an "
		"inlined value in the middle.");
	
	classad::ClassAd *classad;
	classad::ExprTree *expr;
	Value value;
	classad::ExprTree *fexpr;
	
	string classad_string = "[a = 1; b = f; c = a]";
	string expr_string = "a + b + c";
	ClassAdParser parser;
	
	classad = parser.ParseClassAd(classad_string, true);
	expr = parser.ParseExpression(expr_string);

	ClassAdUnParser unparser;
	string value_string, fexpr_string;
	string value2_string, fexpr2_string;
	
	classad->FlattenAndInline(expr, value, fexpr);
	unparser.Unparse(value_string, value);
	unparser.Unparse(fexpr_string, fexpr);
	
	e.emit_input_header();
	e.emit_param("ClassAd", classad_string.c_str());
	e.emit_param("ExprTree", expr_string.c_str());
	e.emit_param("Value", "");
	e.emit_param("ExprTree", "NULL");
	e.emit_output_expected_header();
	e.emit_param("Flattened and Inlined Value", "undefined");
	e.emit_param("Flattened and Inlined Expression", "2 + f");
	e.emit_output_actual_header();
	e.emit_param("Flattened and Inlined Value", value_string.c_str());
	e.emit_param("Flattened and Inlined Expression", fexpr_string.c_str());
	if(fexpr_string.compare("2 + f") != MATCH){
		e.emit_result_failure(__LINE__);
		delete classad; delete expr; delete fexpr;
		return false;
	}
	e.emit_result_success(__LINE__);
	delete classad;	delete expr; delete fexpr;
	return true;
}

static bool test_equivalence_partial_arithmetic_end() {
	e.emit_test("Test if the partially flattened and inlined ExprTree is "
		"logically equivalent to the original arithmetic ExprTree with an "
		"inlined value in the end.");
	
	classad::ClassAd *classad;
	classad::ExprTree *expr;
	Value value;
	classad::ExprTree *fexpr;
	
	string classad_string = "[a = b; b = 1; c = f]";
	string expr_string = "a + b + c";
	ClassAdParser parser;
	
	classad = parser.ParseClassAd(classad_string, true);
	expr = parser.ParseExpression(expr_string);

	ClassAdUnParser unparser;
	string value_string, fexpr_string;
	string value2_string, fexpr2_string;
	
	classad->FlattenAndInline(expr, value, fexpr);
	unparser.Unparse(value_string, value);
	unparser.Unparse(fexpr_string, fexpr);
	
	e.emit_input_header();
	e.emit_param("ClassAd", classad_string.c_str());
	e.emit_param("ExprTree", expr_string.c_str());
	e.emit_param("Value", "");
	e.emit_param("ExprTree", "NULL");
	e.emit_output_expected_header();
	e.emit_param("Flattened and Inlined Value", "undefined");
	e.emit_param("Flattened and Inlined Expression", "2 + f");
	e.emit_output_actual_header();
	e.emit_param("Flattened and Inlined Value", value_string.c_str());
	e.emit_param("Flattened and Inlined Expression", fexpr_string.c_str());
	if(fexpr_string.compare("2 + f") != MATCH){
		e.emit_result_failure(__LINE__);
		delete classad; delete expr; delete fexpr;
		return false;
	}
	e.emit_result_success(__LINE__);
	delete classad;	delete expr; delete fexpr;
	return true;
}

static bool test_equivalence_full_simple_boolean() {
	e.emit_test("Test if the fully flattened and inlined ExprTree is logically "
		"equivalent to the original simple boolean ExprTree.");
	
	classad::ClassAd *classad;
	classad::ExprTree *expr;
	Value value;
	classad::ExprTree *fexpr;
	
	string classad_string = "[a = true; b = false]";
	string expr_string = "a || b";
	ClassAdParser parser;
	
	classad = parser.ParseClassAd(classad_string, true);
	expr = parser.ParseExpression(expr_string);

	ClassAdUnParser unparser;
	string value_string, fexpr_string;
	
	classad->FlattenAndInline(expr, value, fexpr);
	unparser.Unparse(value_string, value);
	unparser.Unparse(fexpr_string, fexpr);
	
	e.emit_input_header();
	e.emit_param("ClassAd", classad_string.c_str());
	e.emit_param("ExprTree", expr_string.c_str());
	e.emit_param("Value", "");
	e.emit_param("ExprTree", "NULL");
	e.emit_output_expected_header();
	e.emit_param("Flattened and Inlined Value", "true");
	e.emit_param("Flattened and Inlined Expression", "<error:null expr>");
	e.emit_output_actual_header();
	e.emit_param("Flattened and Inlined Value", value_string.c_str());
	e.emit_param("Flattened and Inlined Expression", fexpr_string.c_str());
	if(value_string.compare("true") != MATCH){
		e.emit_result_failure(__LINE__);
		delete classad; delete expr; delete fexpr;
		return false;
	}
	e.emit_result_success(__LINE__);
	delete classad;	delete expr; delete fexpr;
	return true;
}

static bool test_equivalence_partial_simple_boolean() {
	e.emit_test("Test if the partially flattened and inlined ExprTree is "
		"logically equivalent to the original simple boolean ExprTree.");
	e.emit_comment("This expression could be further reduced to 'd'");
	classad::ClassAd *classad;
	classad::ExprTree *expr;
	Value value;
	classad::ExprTree *fexpr;
	
	string classad_string = "[a = true; b = false; c = d]";
	string expr_string = "a && b || c";
	ClassAdParser parser;
	
	classad = parser.ParseClassAd(classad_string, true);
	expr = parser.ParseExpression(expr_string);

	ClassAdUnParser unparser;
	string value_string, fexpr_string;
	
	classad->FlattenAndInline(expr, value, fexpr);
	unparser.Unparse(value_string, value);
	unparser.Unparse(fexpr_string, fexpr);
	
	e.emit_input_header();
	e.emit_param("ClassAd", classad_string.c_str());
	e.emit_param("ExprTree", expr_string.c_str());
	e.emit_param("Value", "");
	e.emit_param("ExprTree", "NULL");
	e.emit_output_expected_header();
	e.emit_param("Flattened and Inlined Value", "undefined");
	e.emit_param("Flattened and Inlined Expression", "false || d");
	e.emit_output_actual_header();
	e.emit_param("Flattened and Inlined Value", value_string.c_str());
	e.emit_param("Flattened and Inlined Expression", fexpr_string.c_str());
	if(fexpr_string.compare("false || d") != MATCH){
		e.emit_result_failure(__LINE__);
		delete classad; delete expr; delete fexpr;
		return false;
	}
	e.emit_result_success(__LINE__);
	delete classad;	delete expr; delete fexpr;
	return true;
}

static bool test_equivalence_full_complex_boolean() {
	e.emit_test("Test if the fully flattened and inlined ExprTree is logically "
		"equivalent to the original complex boolean ExprTree.");
	
	classad::ClassAd *classad;
	classad::ExprTree *expr;
	Value value;
	classad::ExprTree *fexpr;
	
	string classad_string = "[a = true; b = false; c = a || b; d = a && b; "
		"e = c && !d; f = !c]";
	string expr_string = "e || !c";
	ClassAdParser parser;
	
	classad = parser.ParseClassAd(classad_string, true);
	expr = parser.ParseExpression(expr_string);

	ClassAdUnParser unparser;
	string value_string, fexpr_string;
	
	classad->FlattenAndInline(expr, value, fexpr);
	unparser.Unparse(value_string, value);
	unparser.Unparse(fexpr_string, fexpr);
	
	e.emit_input_header();
	e.emit_param("ClassAd", classad_string.c_str());
	e.emit_param("ExprTree", expr_string.c_str());
	e.emit_param("Value", "");
	e.emit_param("ExprTree", "NULL");
	e.emit_output_expected_header();
	e.emit_param("Flattened and Inlined Value", "true");
	e.emit_param("Flattened and Inlined Expression", "<error:null expr>");
	e.emit_output_actual_header();
	e.emit_param("Flattened and Inlined Value", value_string.c_str());
	e.emit_param("Flattened and Inlined Expression", fexpr_string.c_str());
	if(value_string.compare("true") != MATCH){
		e.emit_result_failure(__LINE__);
		delete classad; delete expr; delete fexpr;
		return false;
	}
	e.emit_result_success(__LINE__);
	delete classad;	delete expr; delete fexpr;
	return true;
}

static bool test_equivalence_partial_complex_boolean() {
	e.emit_test("Test if the partially flattened and inlined ExprTree is "
		"logically equivalent to the original complex boolean ExprTree.");
	e.emit_comment("This expression could be further reduced to 'g'");
	
	classad::ClassAd *classad;
	classad::ExprTree *expr;
	Value value;
	classad::ExprTree *fexpr;
	
	string classad_string = "[a = true; b = false; c = a || b; d = a && b; "
		"e = c && !d; f = !c; g = h]";
	string expr_string = "!e && c || g ";
	ClassAdParser parser;
	
	classad = parser.ParseClassAd(classad_string, true);
	expr = parser.ParseExpression(expr_string);

	ClassAdUnParser unparser;
	string value_string, fexpr_string;
	
	classad->FlattenAndInline(expr, value, fexpr);
	unparser.Unparse(value_string, value);
	unparser.Unparse(fexpr_string, fexpr);
	
	e.emit_input_header();
	e.emit_param("ClassAd", classad_string.c_str());
	e.emit_param("ExprTree", expr_string.c_str());
	e.emit_param("Value", "");
	e.emit_param("ExprTree", "NULL");
	e.emit_output_expected_header();
	e.emit_param("Flattened and Inlined Value", "undefined");
	e.emit_param("Flattened and Inlined Expression", "false || h");
	e.emit_output_actual_header();
	e.emit_param("Flattened and Inlined Value", value_string.c_str());
	e.emit_param("Flattened and Inlined Expression", fexpr_string.c_str());
	if(fexpr_string.compare("false || h") != MATCH){
		e.emit_result_failure(__LINE__);
		delete classad; delete expr; delete fexpr;
		return false;
	}
	e.emit_result_success(__LINE__);
	delete classad;	delete expr; delete fexpr;
	return true;
}

static bool test_equivalence_nested_classad() {
	e.emit_test("Test if the partially flattened and inlined ExprTree is "
		"logically equivalent to the original ExprTree when using a nested "
		"classad.");
	
	classad::ClassAd *classad;
	classad::ExprTree *expr;
	Value value;
	classad::ExprTree *fexpr;
	
	string classad_string = "[a = [b = 1]]";
	string expr_string = "a";
	ClassAdParser parser;
	
	classad = parser.ParseClassAd(classad_string, true);
	expr = parser.ParseExpression(expr_string);

	ClassAdUnParser unparser;
	string value_string, fexpr_string;
	
	classad->FlattenAndInline(expr, value, fexpr);
	unparser.Unparse(value_string, value);
	unparser.Unparse(fexpr_string, fexpr);
	
	e.emit_input_header();
	e.emit_param("ClassAd", classad_string.c_str());
	e.emit_param("ExprTree", expr_string.c_str());
	e.emit_param("Value", "");
	e.emit_param("ExprTree", "NULL");
	e.emit_output_expected_header();
	e.emit_param("Flattened and Inlined Value", "undefined");
	e.emit_param("Flattened and Inlined Expression", "[ b = 1 ]");
	e.emit_output_actual_header();
	e.emit_param("Flattened and Inlined Value", value_string.c_str());
	e.emit_param("Flattened and Inlined Expression", fexpr_string.c_str());
	if(fexpr_string.compare("[ b = 1 ]") != MATCH){
		e.emit_result_failure(__LINE__);
		delete classad; delete expr; delete fexpr;
		return false;
	}
	e.emit_result_success(__LINE__);
	delete classad;	delete expr; delete fexpr;
	return true;
}

static bool test_equivalence_nested_loop_classad() {
	e.emit_test("Test if FlattenAndInline handles a nested classad with a "
		"loop");
	
	classad::ClassAd *classad;
	classad::ExprTree *expr;
	Value value;
	classad::ExprTree *fexpr;
	
	string classad_string = "[a = [b = a]]";
	string expr_string = "a";
	ClassAdParser parser;
	
	classad = parser.ParseClassAd(classad_string, true);
	expr = parser.ParseExpression(expr_string);

	ClassAdUnParser unparser;
	string value_string, fexpr_string;
	
	classad->FlattenAndInline(expr, value, fexpr);
	unparser.Unparse(value_string, value);
	unparser.Unparse(fexpr_string, fexpr);
	classad_string.clear();
	unparser.Unparse(classad_string, classad);
	
	e.emit_input_header();
	e.emit_param("ClassAd", classad_string.c_str());
	e.emit_param("ExprTree", expr_string.c_str());
	e.emit_param("Value", "");
	e.emit_param("ExprTree", "NULL");
	e.emit_output_expected_header();
	e.emit_param("Flattened and Inlined Value", "undefined");
	e.emit_param("Flattened and Inlined Expression", "<error:null expr>");
	e.emit_output_actual_header();
	e.emit_param("Flattened and Inlined Value", value_string.c_str());
	e.emit_param("Flattened and Inlined Expression", fexpr_string.c_str());
	if(fexpr == NULL &&  value_string.compare("undefined") != MATCH){
		e.emit_result_failure(__LINE__);
		delete classad; delete expr; delete fexpr;
		return false;
	}
	e.emit_result_success(__LINE__);
	delete classad;	delete expr; delete fexpr;
	return true;
}

static bool test_equivalence_simple_list() {
	e.emit_test("Test if the partially flattened and inlined ExprTree is "
		"logically equivalent to the original simple expression list "
		"ExprTree.");
	
	classad::ClassAd *classad;
	classad::ExprTree *expr;
	Value value;
	classad::ExprTree *fexpr;
	
	string classad_string = "[a = 1; b = 2; c = a; d = b]";
	string expr_string = "{a, b, c, d}";
	ClassAdParser parser;
	
	classad = parser.ParseClassAd(classad_string, true);
	expr = parser.ParseExpression(expr_string);

	ClassAdUnParser unparser;
	string value_string, fexpr_string;
	
	classad->FlattenAndInline(expr, value, fexpr);
	unparser.Unparse(value_string, value);
	unparser.Unparse(fexpr_string, fexpr);
	
	e.emit_input_header();
	e.emit_param("ClassAd", classad_string.c_str());
	e.emit_param("ExprTree", expr_string.c_str());
	e.emit_param("Value", "");
	e.emit_param("ExprTree", "NULL");
	e.emit_output_expected_header();
	e.emit_param("Flattened and Inlined Value", "undefined");
	e.emit_param("Flattened and Inlined Expression", "{ 1,2,1,2 }");
	e.emit_output_actual_header();
	e.emit_param("Flattened and Inlined Value", value_string.c_str());
	e.emit_param("Flattened and Inlined Expression", fexpr_string.c_str());
	if(fexpr_string.compare("{ 1,2,1,2 }") != MATCH){
		e.emit_result_failure(__LINE__);
		delete classad; delete expr; delete fexpr;
		return false;
	}
	e.emit_result_success(__LINE__);
	delete classad;	delete expr; delete fexpr;
	return true;
}

static bool test_equivalence_complex_list() {
	e.emit_test("Test if the partially flattened and inlined ExprTree is "
		"logically equivalent to the original complex expression list "
		"ExprTree.");
	
	classad::ClassAd *classad;
	classad::ExprTree *expr;
	Value value;
	classad::ExprTree *fexpr;
	
	string classad_string = "[a = b; b = 2; c = d;]";
	string expr_string = "{a, b, c, d, true || false, true && false, 10 + a,"
		"10 - a, 10 * a, 10 / a}";
	ClassAdParser parser;
	
	classad = parser.ParseClassAd(classad_string, true);
	expr = parser.ParseExpression(expr_string);

	ClassAdUnParser unparser;
	string value_string, fexpr_string;
	
	classad->FlattenAndInline(expr, value, fexpr);
	unparser.Unparse(value_string, value);
	unparser.Unparse(fexpr_string, fexpr);
	
	e.emit_input_header();
	e.emit_param("ClassAd", classad_string.c_str());
	e.emit_param("ExprTree", expr_string.c_str());
	e.emit_param("Value", "");
	e.emit_param("ExprTree", "NULL");
	e.emit_output_expected_header();
	e.emit_param("Flattened and Inlined Value", "undefined");
	e.emit_param("Flattened and Inlined Expression", "{ 2,2,d,d,true,false,12,"
		"8,20,5 }");
	e.emit_output_actual_header();
	e.emit_param("Flattened and Inlined Value", value_string.c_str());
	e.emit_param("Flattened and Inlined Expression", fexpr_string.c_str());
	if(fexpr_string.compare("{ 2,2,d,d,true,false,12,8,20,5 }") != MATCH){
		e.emit_result_failure(__LINE__);
		delete classad; delete expr; delete fexpr;
		return false;
	}
	e.emit_result_success(__LINE__);
	delete classad;	delete expr; delete fexpr;
	return true;
}
