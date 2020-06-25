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


// This is a program that does a bunch of testing that is easier to do
// without user input, like in test_classads.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "classad/classad_distribution.h"
#include "classad/lexerSource.h"
#include <fstream>
#include <iostream>
#include <ctype.h>

using namespace std;
using namespace classad;

// Note that we are careful to leave no space between the first and 
// second ClassAd. This is to test that we put back a character correctly
// after parsing.
string parsing_text = 
"[ A = 1; B = \"blue\"; C = true; ]"
"[ AA = 14; BB = \"bonnet\"; CC = false; ]";

string chaining_root_text  = "[  A = 1;   B = \"blue\";    C = true;  ]";
string chaining_child_text = "[ AA = 14;  B = \"bonnet\"; CC = false; ]";

string dirty_classad_text = "[ A = 1; ]";

int main(int argc, char **argv);
static void test_quoting_bug(void);
static void test_parsing(void);
static void test_parsing_helper(ClassAd *classad_a, ClassAd *classad_b);
static void read_from_string(ClassAd **classad_a, ClassAd **classad_b);
static void read_from_string_alt(ClassAd **classad_a, ClassAd **classad_b);
static void read_from_char(ClassAd **classad_a, ClassAd **classad_b);
static void read_from_char_alt(ClassAd **classad_a, ClassAd **classad_b);
static void make_file(void);
static void remove_file(void);
static void read_from_file(ClassAd **classad_a, ClassAd **classad_b);
static void read_from_file_alt(ClassAd **classad_a, ClassAd **classad_b);
static void read_from_stream(ClassAd **classad_a, ClassAd **classad_b);
static void read_from_stream_alt(ClassAd **classad_a, ClassAd **classad_b);
static void check_parse(ClassAd *classad_a, ClassAd *classad_b, 
						ClassAd *classad_c);
static void check_classad_a(ClassAd *classad);
static void check_classad_b(ClassAd *classad);

static void test_chaining(void);
static void test_dirty(void);
static void test_user_functions(void);
static bool triple(
	const char         *name,
	const ArgumentList &arguments,
	EvalState          &state,
	Value              &result);

/*********************************************************************
 *
 * Function: main
 * Purpose:  Overall control
 *
 *********************************************************************/
int 
main(int, char **)
{
    test_quoting_bug();
	test_parsing();
	test_chaining();
	test_dirty();
	test_user_functions();
	return 0;
}

static void test_quoting_bug(void)
{
    ClassAdParser par ;
    string attr = "other.GlueCEStateStatus == \"Production\"";
    if (!par.ParseExpression(attr, true)) {
        cout << "Error in parsing quotes.\n";
    } else {
        cout << "No problem in parsing quotes.\n";
    }
    return;
}

static void test_parsing(void)
{
	ClassAd       *classad_a, *classad_b;

	read_from_string(&classad_a, &classad_b);
	test_parsing_helper(classad_a, classad_b);

	read_from_string_alt(&classad_a, &classad_b);
	test_parsing_helper(classad_a, classad_b);

	read_from_char(&classad_a, &classad_b);
	test_parsing_helper(classad_a, classad_b);

	read_from_char_alt(&classad_a, &classad_b);
	test_parsing_helper(classad_a, classad_b);

	make_file();

	read_from_file(&classad_a, &classad_b);
	test_parsing_helper(classad_a, classad_b);

	read_from_file_alt(&classad_a, &classad_b);
	test_parsing_helper(classad_a, classad_b);

	read_from_stream(&classad_a, &classad_b);
	test_parsing_helper(classad_a, classad_b);

	read_from_stream_alt(&classad_a, &classad_b);
	test_parsing_helper(classad_a, classad_b);

	remove_file();

	return;
}

static void test_parsing_helper(
	ClassAd *classad_a,
	ClassAd *classad_b)
{
	check_classad_a(classad_a);
	check_classad_b(classad_b);
	delete classad_a;
	delete classad_b;
	return;
}

static void read_from_string(ClassAd **classad_a, ClassAd **classad_b)
{
	ClassAdParser parser;
	ClassAd *classad_c;
	int offset;

	cout << "Reading from C++ string.\n";

	offset = 0;
	*classad_a = parser.ParseClassAd(parsing_text, offset);
	*classad_b = parser.ParseClassAd(parsing_text, offset);
	classad_c = parser.ParseClassAd(parsing_text, offset);

	check_parse(*classad_a, *classad_b, classad_c);

	return;
}

static void read_from_string_alt(ClassAd **classad_a, ClassAd **classad_b)
{
	ClassAdParser parser;
	ClassAd *classad_c;
	int offset;

	cout << "Reading from C++ string (alt parse).\n";

	offset = 0;
	*classad_a = new ClassAd;
	*classad_b = new ClassAd;
	classad_c  = new ClassAd;
	parser.ParseClassAd(parsing_text, **classad_a, offset);
	parser.ParseClassAd(parsing_text, **classad_b, offset);
	if (!parser.ParseClassAd(parsing_text, *classad_c, offset)) {
		delete classad_c;
		classad_c = NULL;
	}

	check_parse(*classad_a, *classad_b, classad_c);
	return;
}

static void read_from_char(ClassAd **classad_a, ClassAd **classad_b)
{
	ClassAdParser parser;
	ClassAd *classad_c;
	const char *text = parsing_text.c_str();
	int offset;

	cout << "Reading from C string.\n";

	offset = 0;
	*classad_a = parser.ParseClassAd(text, offset);
	*classad_b = parser.ParseClassAd(text, offset);
	classad_c = parser.ParseClassAd(text, offset);

	check_parse(*classad_a, *classad_b, classad_c);
	return;
}

static void read_from_char_alt(ClassAd **classad_a, ClassAd **classad_b)
{
	ClassAdParser parser;
	ClassAd *classad_c;
	const char *text = parsing_text.c_str();
	int offset;

	cout << "Reading from C string (alt parse).\n";

	offset = 0;
	*classad_a = new ClassAd;
	*classad_b = new ClassAd;
	classad_c  = new ClassAd;
	parser.ParseClassAd(text, **classad_a, offset);
	parser.ParseClassAd(text, **classad_b, offset);
	if (!parser.ParseClassAd(text, *classad_c, offset)) {
		delete classad_c;
		classad_c = NULL;
	}

	check_parse(*classad_a, *classad_b, classad_c);
	return;
}

static void make_file(void)
{
	FILE *file;

	remove_file();
	file = fopen("tmp.classads.tmp", "w");
	fprintf(file, "%s", parsing_text.c_str());
	fclose(file);

	return;
}

static void remove_file(void)
{
	system("rm -f tmp.classads.tmp");
	return;
}

static void read_from_file(ClassAd **classad_a, ClassAd **classad_b)
{
	ClassAdParser parser;
	ClassAd *classad_c;
	FILE *file;

	file = fopen("tmp.classads.tmp", "r");
	if (file == NULL) {
		cout << "Can't open temporary ClassAd file.\n";
		exit(1);
	}

	cout << "Reading from FILE.\n";

	*classad_a = parser.ParseClassAd(file);
	*classad_b = parser.ParseClassAd(file);
	classad_c = parser.ParseClassAd(file);

	check_parse(*classad_a, *classad_b, classad_c);
	return;
}

static void read_from_file_alt(ClassAd **classad_a, ClassAd **classad_b)
{
	ClassAdParser parser;
	ClassAd *classad_c;
	FILE *file;

	file = fopen("tmp.classads.tmp", "r");
	if (file == NULL) {
		cout << "Can't open temporary ClassAd file.\n";
		exit(1);
	}

	cout << "Reading from FILE (alt parse).\n";

	*classad_a = new ClassAd;
	*classad_b = new ClassAd;
	classad_c  = new ClassAd;
	parser.ParseClassAd(file, **classad_a);
	parser.ParseClassAd(file, **classad_b);
	if (!parser.ParseClassAd(file, *classad_c)) {
		delete classad_c;
		classad_c = NULL;
	}

	check_parse(*classad_a, *classad_b, classad_c);
	return;
}

static void read_from_stream(ClassAd **classad_a, ClassAd **classad_b)
{
	ClassAdParser parser;
	ClassAd *classad_c;

	ifstream stream("tmp.classads.tmp");
	if (!stream) {
		cout << "Can't open temporary ClassAd file.\n";
		exit(1);
	}

	cout << "Reading from ifstream.\n";

	*classad_a = parser.ParseClassAd(stream);
	*classad_b = parser.ParseClassAd(stream);
	classad_c = parser.ParseClassAd(stream);

	check_parse(*classad_a, *classad_b, classad_c);
	return;
}

static void read_from_stream_alt(ClassAd **classad_a, ClassAd **classad_b)
{
	ClassAdParser parser;
	ClassAd *classad_c;

	ifstream stream("tmp.classads.tmp");
	if (!stream) {
		cout << "Can't open temporary ClassAd file.\n";
		exit(1);
	}

	cout << "Reading from ifstream (alt parse).\n";

	*classad_a = new ClassAd;
	*classad_b = new ClassAd;
	classad_c  = new ClassAd;
	parser.ParseClassAd(stream, **classad_a);
	parser.ParseClassAd(stream, **classad_b);
	if (!parser.ParseClassAd(stream, *classad_c)) {
		delete classad_c;
		classad_c = NULL;
	}

	check_parse(*classad_a, *classad_b, classad_c);
	return;
}

static void check_parse(
	ClassAd *classad_a,
	ClassAd *classad_b,
	ClassAd *classad_c)
{

	if (classad_a == NULL) {
		cout << "Couldn't parse first classad!\n";
		exit(1);
	} else if (classad_b == NULL) {
		cout << "Couldn't parse second classad!\n";
		exit(1);
	} else if (classad_c != NULL) {
		cout << "Parsed third ClassAd?\n";
		exit(1);
	}
	return;
}

static void check_classad_a(ClassAd *classad)
{
	int    a;
	string b;
	bool   c;
	bool   success = true;

	if (!classad->EvaluateAttrInt("A", a) || a != 1) {
		cout << "  Failed: A.A is bad.\n";
		success = false;
	}

	if (!classad->EvaluateAttrString("B", b) || b != "blue") {
		cout << "  Failed: A.B is bad.\n";
		success = false;
	}

	if (!classad->EvaluateAttrBool("C", c) || c != true) {
		cout << "  Failed: A.C is bad.\n";
		success = false;
	}
   
	if (success) {
		cout << "  ClassAd A looks good.\n";
	}
	return;
}

static void check_classad_b(ClassAd *classad)
{
	int    aa;
	string bb;
	bool   cc;
	bool   success = true;

	if (!classad->EvaluateAttrInt("AA", aa) || aa != 14) {
		cout << "  Failed: B.AA is bad.\n";
		success = false;
	}

	if (!classad->EvaluateAttrString("BB", bb) || bb != "bonnet") {
		cout << "  Failed: B.BB is bad.\n";
		success = false;
	}

	if (!classad->EvaluateAttrBool("CC", cc) || cc != false) {
		cout << "  Failed: B.CC is bad.\n";
		success = false;
	}
   
	if (success) {
		cout << "  ClassAd B looks good.\n";
	}
	return;
}

static void test_chaining(void)
{
	ClassAd *root, *child;
	ClassAdParser  parser;

	root = parser.ParseClassAd(chaining_root_text);
	child = parser.ParseClassAd(chaining_child_text);

	if (root == NULL) {
		cout << "Failed: Couldn't parse chaining root.\n";
		exit(1);
	} else if (child == NULL) {
		cout << "Failed: Couldn't parser chaining child.\n";
		exit(1);
	}
	
	cout << "\nTesting chaining...\n";
	child->ChainToAd(root);

	// Make sure we can look up in child.
	int aa;
	if (!child->EvaluateAttrInt("AA", aa) || aa != 14) {
		cout << "  Couldn't retrieve AA from chained child.\n";
	} else {
		cout << "  Can look up AA.\n";
	}

	// Make sure we can look up in parent:
	int a;
	if (!child->EvaluateAttrInt("A", a) || a != 1) {
		cout << "  Couldn't retrieve A from chained parent.\n";
	} else {
		cout << "  Can look up A from chained parent.\n";
	} 

	// When we look up B, it should come from child, not parent, 
	// because it is duplicated.
	string b;
	if (!child->EvaluateAttrString("B", b) || b != "bonnet") {
		cout << "  Failed to look up B from chained child.\n";
	} else {
		cout << "  Can look up B from child.\n";
	}

	// Let's delete BB and look it up again. It should come back
	// as undefined.
	Value lookup_value;
	string bb;
	child->Delete("BB");
	if (child->EvaluateAttrString("BB", bb)) {
		cout << "  Failed: BB still has string value?\n";
	} else {
		cout << "  BB is now undefined.\n";
	}

	// Let's delete B and look it up again. It should come back
	// as undefined.
	child->Delete("B");
	if (child->EvaluateAttrString("B", b)) {
		cout << "  Failed: B still has string value?\n";
	} else {
		cout << "  B is now undefined.\n";
	}
	cout << child << endl;

	child->Unchain();

	return;
}

static void test_dirty(void)
{
	ClassAd        *classad;
	ClassAdParser  parser;

	classad = parser.ParseClassAd(dirty_classad_text);

	cout << "Testing dirty attributes...\n";

	classad->EnableDirtyTracking();

	if (classad->IsAttributeDirty("A")) {
		cout << "  Failed: A is dirty just after construction.\n";
	} else {
		cout << "  OK: A is clean.\n";
	}

	classad->InsertAttr("B", true);
	if (classad->IsAttributeDirty("A")) {
		cout << "  Failed: A is dirty after inserting B.\n";
	} else {
		cout << "  OK: A is still clean.\n";
	}
	
	if (!classad->IsAttributeDirty("B")) {
		cout << "  Failed: B is not dirty.\n";
	} else {
		cout << "  OK: B is dirty.\n";
	}

	ClassAd::dirtyIterator  it = classad->dirtyBegin();
	
	if (it == classad->dirtyEnd()) {
		cout << "  Failed: Dirty iterator gives us nothing.\n";
	} else {
		if ((*it) != "B") {
			cout << "  Failed: Dirty iterator is not pointing at B.\n";
		} else {
			cout << "  OK: Dirty iterator starts out okay.\n";

			it++;
			if (it != classad->dirtyEnd()) {
				cout << "  Failed: Dirty iterator it not at end.\n";
			} else {
				cout << "  OK: Dirty iterator ends okay.\n";
			}
		}
	}

	classad->ClearAllDirtyFlags();
	if (classad->IsAttributeDirty("B")) {
		cout << "  Failed: B is dirty after clearing flags.\n";
	} else {
		cout << "  OK: B is clean.\n";
	}

	classad->MarkAttributeDirty("A");
	if (!classad->IsAttributeDirty("A")) {
		cout << "  Failed: A is not dirty.\n";
	} else {
		cout << "  OK: A is dirty.\n";
	}

	classad->MarkAttributeClean("A");
	if (classad->IsAttributeDirty("A")) {
		cout << "  Failed: A is dirty after cleaning it.\n";
	} else {
		cout << "  OK: A is clean.\n";
	}

	return;
}

static void test_user_functions(void)
{
	string name = "triple";
	string classad_text = "[ Test1 = 3 + triple(9); Test2 = 3 + double(9); "
		                  "Test3 = todays_date(); ]";
	ClassAd        *classad;
	ClassAdParser  parser;

	cout << "Testing user functions...\n";

	FunctionCall::RegisterFunction(name, triple);

#if defined(HAVE_DLOPEN) && !defined(DISABLE_DLOPEN_CHECK)
	bool opened_library;
	char path[10240];
	string libname;
	path[0] = 0;
	getcwd(path, 10239);
	libname = path;
	libname += "/";
	libname += TEST_LIBNAME; // TEST_LIBNAME is #define'd
	opened_library = FunctionCall::RegisterSharedLibraryFunctions(libname.c_str());
	if (!opened_library) {
		cout << "  Failed to open libshared.so: " << CondorErrMsg << endl;
	}
#endif

	classad = parser.ParseClassAd(classad_text);
	if (classad == NULL) {
		cout << "  Failed to parse ClassAd in test_internal_user_function().\n";
		exit(1);
	}

	int test;
	if (!classad->EvaluateAttrInt("Test1", test) || test != 30) {
		cout << "  Failed: Couldn't evaluate internal user function triple correctly.";
	} else {
		cout << "  OK: Evaluated internal user function triple (";
		cout << test << ") correctly.\n";
	}

#if defined(HAVE_DLOPEN) && !defined(DISABLE_DLOPEN_CHECK)
	if (opened_library) {
		// The library defines triple, but it shouldn't be allowed to
		// overwrite the original defintion, so we verify that the
		// result hasn't changed.
		if (!classad->EvaluateAttrInt("Test1", test) || test != 30) {
			cout << "  Failed: Couldn't evaluate internal user function triple correctly."
				 << endl;
		} else {
			cout << "  OK: Evaluated internal user function triple (";
			cout << test << ") correctly.\n";
		}

		// Then we test one function that comes from the shared library.
		if (!classad->EvaluateAttrInt("Test2", test) || test != 21) {
			cout << "  Failed: Couldn't evaluate shared user function double " 
				 << " (" << test << ") correctly." << endl;
			if (!classad->EvaluateAttrInt("Test2", test)) {
				cout << "  Couldn't even evaluate it at all." << endl;
			}
		} else {
			cout << "  OK: Evaluated shared user function double (";
			cout << test << ") correctly.\n";
		}

		// Then we test another function that comes from the shared library.
		string date;
		if (!classad->EvaluateAttrString("Test3", date)) {
			cout << "  Failed: Couldn't evaluate shared user function " 
				 << " todays_date (" << test << ") correctly." << endl;
		} else {
			cout << "  OK: Evaluated shared function todays_date: ";
			cout << date << endl;
		}
		
	}
#endif
	
	return;
}


static bool triple(
	const char         *,
	const ArgumentList &arguments,
	EvalState          &state,
	Value              &result)
{
	Value	arg;
	
	// takes exactly one argument
	if( arguments.size() > 1 ) {
		result.SetErrorValue( );
		return true;
	}
	if( !arguments[0]->Evaluate( state, arg ) ) {
		result.SetErrorValue( );
		return false;
	}

	switch( arg.GetType( ) ) {
		case Value::UNDEFINED_VALUE:
			result.SetUndefinedValue( );
			return true;

		case Value::ERROR_VALUE:
		case Value::CLASSAD_VALUE:
		case Value::LIST_VALUE:
		case Value::SLIST_VALUE:
		case Value::STRING_VALUE:
		case Value::ABSOLUTE_TIME_VALUE:
		case Value::RELATIVE_TIME_VALUE:
		case Value::BOOLEAN_VALUE:
			result.SetErrorValue( );
			return true;

		case Value::INTEGER_VALUE:
			int int_value;

			arg.IsIntegerValue(int_value);
			result.SetIntegerValue(3 * int_value);
			return true;

		case Value::REAL_VALUE:
			{
				double real_value;
				arg.IsRealValue( real_value );
				result.SetRealValue(3 * real_value);
				return true;
			}
		
		default:
			return false;
	}

	return false;
}

