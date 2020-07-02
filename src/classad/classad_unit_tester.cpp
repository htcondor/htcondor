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


/*--------------------------------------------------------------------
 *
 * To Do:
 *  - Write test_value()
 *  - Write test_literal()
 *  - Write test_match()
 *  - Write test_operator()
 *  - Extend test_collection() to test much more of the interface
 *  - Extend test_classad() to test much more of the interface
 *  - Test xml: roll in test_xml.C
 *  - Get stuff from extra_tests.C
 *
 *-------------------------------------------------------------------*/

#include "classad/classad_distribution.h"
#include "classad/lexerSource.h"
#include "classad/xmlSink.h"
#include <fstream>
#include <iostream>
#include <ctype.h>
#include <assert.h>

using namespace std;
using namespace classad;

#define TEST(name, test) {if (test) \
results.AddSuccessfulTest(name, __LINE__); \
else results.AddFailedTest(name, __LINE__);}

class Parameters
{
public: 
	bool  debug;
    bool  verbose;
    bool  very_verbose;

    bool  check_all;
    bool  check_parsing;
    bool  check_classad;
	bool  check_exprlist;
    bool  check_value;
    bool  check_literal;
    bool  check_match;
    bool  check_operator;
    bool  check_collection;
    bool  check_utils;
	void  ParseCommandLine(int argc, char **argv);
};

class Results
{
public:
    Results(const Parameters &parameters);

    void AddSuccessfulTest(const char *name, int line_number);
    void AddFailedTest(const char *name, int line_number);

    void GetResults(int &number_of_errors, int &number_of_tests) const;

private:
    int         _number_of_errors;
    int         _number_of_tests;
    bool        _verbose;
    bool        _very_verbose;
};

/*--------------------------------------------------------------------
 *
 * Private Functions
 *
 *--------------------------------------------------------------------*/

static void test_parsing(const Parameters &parameters, Results &results);
static void test_classad(const Parameters &parameters, Results &results);
static void test_exprlist(const Parameters &parameters, Results &results);
static void test_value(const Parameters &parameters, Results &results);
static void test_collection(const Parameters &parameters, Results &results);
static void test_utils(const Parameters &parameters, Results &results);
static bool check_in_view(ClassAdCollection *collection, string view_name, string classad_name);
static void print_version(void);

/*********************************************************************
 *
 * Function: Parameters::ParseCommandLine
 * Purpose:  This parses the command line. Note that it will exit
 *           if there are any problems. 
 *
 *********************************************************************/
void Parameters::ParseCommandLine(
    int  argc, 
    char **argv)
{
    bool selected_test  = false;
    bool help           = false;

	// First we set up the defaults.
    help                = false;
	debug               = false;
    verbose             = false;
    very_verbose        = false;
    check_all           = false;
    check_parsing       = false;
    check_classad       = false;
	check_exprlist      = false;
    check_value         = false;
    check_literal       = false;
    check_match         = false;
    check_operator      = false;
    check_collection    = false;
    check_utils         = false;

	// Then we parse to see what the user wants. 
	for (int arg_index = 1; arg_index < argc; arg_index++) {
        if (   !strcasecmp(argv[arg_index], "-h")
            || !strcasecmp(argv[arg_index], "-help")) {
            help = true;
            break;
		} else if (   !strcasecmp(argv[arg_index], "-d")
            || !strcasecmp(argv[arg_index], "-debug")) {
			debug = true;
		} else if (   !strcasecmp(argv[arg_index], "-v")
                   || !strcasecmp(argv[arg_index], "-verbose")) {
			verbose = true;
		} else if (   !strcasecmp(argv[arg_index], "-vv")
                   || !strcasecmp(argv[arg_index], "-veryverbose")) {
			verbose = true;
            very_verbose = true;
		} else if (!strcasecmp(argv[arg_index], "-all")){
            check_all           = true;
            selected_test       = true;
		} else if (!strcasecmp(argv[arg_index], "-parsing")){
            check_parsing       = true;
            selected_test       = true;
		} else if (!strcasecmp(argv[arg_index], "-classad")){
            check_classad       = true;
            selected_test       = true;
		} else if (!strcasecmp(argv[arg_index], "-epxrlist")){
            check_exprlist      = true;
            selected_test       = true;
		} else if (!strcasecmp(argv[arg_index], "-value")){
            check_value         = true;
            selected_test       = true;
		} else if (!strcasecmp(argv[arg_index], "-literal")){
            check_literal       = true;
            selected_test       = true;
		} else if (!strcasecmp(argv[arg_index], "-match")){
            check_match         = true;
            selected_test       = true;
		} else if (!strcasecmp(argv[arg_index], "-operator")){
            check_operator      = true;
            selected_test       = true;
		} else if (!strcasecmp(argv[arg_index], "-collection")){
            check_collection    = true;
            selected_test       = true;
		} else if (!strcasecmp(argv[arg_index], "-utils")){
            check_utils         = true;
            selected_test       = true;
		} else {
            cout << "Unknown argument: " << argv[arg_index] << endl;
            help = true;
		}
    }

    if (help) {
        cout << "Usage: classad_unit_tester [options]\n";
        cout << "\n";
        cout << "Basic options:\n";
        cout << "    -h  | -help:        print help\n";
        cout << "    -d  | -debug:       debug\n";
        cout << "    -v  | -verbose:     verbose output\n";
        cout << "    -vv | -veryverbose: very verbose output\n";
        cout << "\n";
        cout << "Test selectors:\n";
        cout << "    -all:        all tests listed below (the default)\n";
        cout << "    -parsing:    test non-ClassAd parsing.\n";
        cout << "    -classad:    test the ClassAd class.\n";
        cout << "    -exprlist:   test the ExprList class.\n";
        cout << "    -value:      test the Value class.\n";
        cout << "    -literal:    test the Literal class.\n";
        cout << "    -match:      test the MatchClassAd class.\n";
        cout << "    -operator:   test the Operator class.\n";
        cout << "    -collection: test the Collection class.\n";
        cout << "    -utils:      test little utilities.\n";
        exit(1);
    }
    if (!selected_test) {
        check_all = true;
    }

	return;
}

/*********************************************************************
 *
 * Function: 
 * Purpose:  
 *
 *********************************************************************/
Results::Results(const Parameters &parameters)
{
    _number_of_errors = 0;
    _number_of_tests  = 0;
    _verbose          = parameters.verbose;
    _very_verbose     = parameters.very_verbose;
    return;
}

void Results::AddSuccessfulTest(const char *name, int line_number)
{
    _number_of_tests++;
    if (_very_verbose) {
        cout << "SUCCESS: " <<  name << " on line " << line_number << endl;
    }
    return;
}

void Results::AddFailedTest(const char *name, int line_number)
{
    _number_of_errors++;
    _number_of_tests++;

    cout << "FAILURE " << name << " on line " << line_number << endl;
    return;
}

void Results::GetResults(int &number_of_errors, int &number_of_tests) const
{
    number_of_errors = _number_of_errors;
    number_of_tests  = _number_of_tests;
    return;
}

/*********************************************************************
 *
 * Function: main
 * Purpose:  The main control loop.
 *
 *********************************************************************/
int main(
    int  argc, 
    char **argv)
{
    int         number_of_errors;
    int         number_of_tests;
    bool        have_errors;
    string      line;
    Parameters  parameters;

    /* ----- Setup ----- */
    print_version();
    parameters.ParseCommandLine(argc, argv);
    Results  results(parameters);

    /* ----- Run tests ----- */
    if (parameters.check_all || parameters.check_parsing) {
        test_parsing(parameters, results);
    }

    if (parameters.check_all || parameters.check_classad) {
        test_classad(parameters, results);
    }
    if (parameters.check_all || parameters.check_exprlist) {
        test_exprlist(parameters, results);
    }
    if (parameters.check_all || parameters.check_value) {
        test_value(parameters, results);
    }
    if (parameters.check_all || parameters.check_literal) {
    }
    if (parameters.check_all || parameters.check_match) {
    }
    if (parameters.check_all || parameters.check_operator) {
    }
    if (parameters.check_all || parameters.check_collection) {
        test_collection(parameters, results);
    }
    if (parameters.check_all || parameters.check_utils) {
        test_utils(parameters, results);
    }

    /* ----- Report ----- */
    cout << endl;
    results.GetResults(number_of_errors, number_of_tests);
    if (number_of_errors > 0) {
        have_errors = true;
        cout << "Finished with errors: \n";
        cout << "    " << number_of_errors << " errors\n";
        cout << "    " << number_of_tests << " tests\n";
    } else {
        have_errors = false;
        cout << "Finished with no errors.\n";
        cout << "    " << number_of_tests << " tests\n";
    }
    return have_errors;
}

/*********************************************************************
 *
 * Function: test_parsing
 * Purpose:  Test parsing that isn't ClassAd-specific. (ClassAd-specific
 *           is in test_clasad
 *
 *********************************************************************/
static void test_parsing(const Parameters &, Results &results)
{
    ClassAdParser parser;
    ExprTree  *tree;

    // My goal is to ensure that these expressions don't crash
    // They should also return a NULL tree
    tree = parser.ParseExpression("true || false || ;");
    TEST("Bad or doesn't crash & isn't bogus", tree == NULL);

    tree = parser.ParseExpression("true && false && ;");
    TEST("Bad and doesn't crash & isn't bogus", tree == NULL);

    tree = parser.ParseExpression("3 | 4 | ;");
    TEST("Bad and doesn't crash & isn't bogus", tree == NULL);

    tree = parser.ParseExpression("3 ^ 4 ^ ;");
    TEST("Bad exclusive or doesn't crash & isn't bogus", tree == NULL);

    tree = parser.ParseExpression("3 & 4 & ;");
    TEST("Bad bitwise and doesn't crash & isn't bogus", tree == NULL);

    tree = parser.ParseExpression("3 == 4 ==  ;");
    TEST("Bad equality doesn't crash & isn't bogus", tree == NULL);

    tree = parser.ParseExpression("1 < 3 < ;");
    TEST("Bad relational doesn't crash & isn't bogus", tree == NULL);

    tree = parser.ParseExpression("1 << 3 << ;");
    TEST("Bad shift doesn't crash & isn't bogus", tree == NULL);

    tree = parser.ParseExpression("1 + 3 + ;");
    TEST("Bad additive doesn't crash & isn't bogus", tree == NULL);

    tree = parser.ParseExpression("1 * 3 * ;");
    TEST("Bad multiplicative doesn't crash & isn't bogus", tree == NULL);
    return;
}

/*********************************************************************
 *
 * Function: test_classad
 * Purpose:  Test the ClassAd class.
 *
 *********************************************************************/
static void test_classad(const Parameters &, Results &results)
{
    ClassAdParser parser;
    bool have_attribute;
    bool success;

    cout << "Testing the ClassAd class...\n";

    string input_basic = "[ A = 3; B = 4.0; C = \"babyzilla\"; D = true; E = {1}; F = [ AA = 3; ]; G =\"deleteme\";]";
    ClassAd *basic;
    int i;
    bool b;
    double r;
    string s;
    ClassAd *c;
    //ExprList *l;
    
    basic = parser.ParseClassAd(input_basic);

    /* ----- Test EvaluateAttr* ----- */
    have_attribute = basic->EvaluateAttrInt("A", i);
    TEST("Have attribute A", (have_attribute == true));
    TEST("A is 3", (i == 3));

    have_attribute = basic->EvaluateAttrReal("B", r);
    TEST("Have attribute B", (have_attribute == true));
    TEST("B is 4.0", (r == 4.0));

    have_attribute = basic->EvaluateAttrString("C", s);
    TEST("Have attribute C", (have_attribute == true));
    TEST("C is 'babyzilla'", (s.compare("babyzilla") == 0));

    have_attribute = basic->EvaluateAttrBool("D", b);
    TEST("Have attribute D", (have_attribute == true));
    TEST("D is true", (b == true));

/* These functions removed
    have_attribute = basic->EvaluateAttrList("E", l);
    TEST("Have attribute E", (have_attribute == true));
    TEST("E is list of size ", (l->size() == 1));

    have_attribute = basic->EvaluateAttrClassAd("F", c);
    TEST("Have attribute F", (have_attribute == true));
    have_attribute = c->EvaluateAttrInt("AA", i);
    TEST("F looks correct", (i == 3));
*/

    /* ----- Test basic insert and delete ----- */
    success = basic->InsertAttr("new", 4);
    TEST("InsertAttr claims to have worked", (success == true));
    have_attribute = basic->EvaluateAttrInt("new", i);
    TEST("Have new attribute", (have_attribute == true));
    TEST("new attribute is 4", i == 4);

    success = basic->Delete("new");
    TEST("Delete claims to have worked", (success == true));
    have_attribute = basic->EvaluateAttrInt("new", i);
    TEST("New attribute was deleted", (have_attribute == false));

    success = basic->Delete("G");
    TEST("DELETE claims to have worked", (success == true));
    have_attribute = basic->EvaluateAttrString("G", s);
    TEST("Attribute G was deleted", (have_attribute == false));

    delete basic;
    basic = NULL;

    /* ----- Test GetExternalReferences ----- */
    string input_ref = "[ Rank=Member(\"LCG-2_1_0\",other.Environment) ? other.Time/seconds : other.Time/minutes; minutes=60; ]";
    References            refs;
    References::iterator  iter;
    ExprTree              *rank;

    c = parser.ParseClassAd(input_ref);
    TEST("Made classad_ref", (c != NULL));
    if (c != NULL) {
        rank = c->Lookup("Rank");
        TEST("Rank exists", (rank != NULL));
        
        if (rank != NULL) {
            bool have_references;
            if ((have_references = c->GetExternalReferences(rank, refs, true))) {
                TEST("have_references", (have_references == true));

                if (have_references) {
                    bool have_environment;
                    bool have_time;
                    bool have_seconds;
                    bool have_other;
                    have_environment = false;
                    have_time = false;
                    have_seconds = false;
                    have_other = false;
                    for (iter = refs.begin(); iter != refs.end(); iter++) {
                        printf("%s\n",(*iter).c_str());
                        if ((*iter).compare("other.Environment") == 0) {
                            have_environment = true;
                        } else if ((*iter).compare("other.Time") == 0) {
                            have_time = true;
                        } else if ((*iter).compare("seconds") == 0) {
                            have_seconds = true;
                        } else {
                            have_other = true;
                        }
                    }
                    TEST("Have external reference to Environment", (have_environment == true));
                    TEST("Have external reference to Time", (have_time == true));
                    TEST("Have external reference to seconds", (have_seconds == true));
                    TEST("Have no other external references", (have_other != true));
                }
            }
        }
        delete c;
        c = NULL;
    }

    // This ClassAd may cause problems. Perhaps a memory leak. 
    // This test is only useful when run under valgrind.
    string memory_problem_classad = 
        "[ Updates = [status = \"request_completed\"; timestamp = absTime(\"2004-12-16T18:10:59-0600]\")] ]";
    c = parser.ParseClassAd(memory_problem_classad);
    delete c;
    c = NULL;

    /* ----- Test Parsing multiple ClassAds ----- */
    const char *two_classads = "[ a = 3; ][ b = 4; ]";
    ClassAd classad1;
    ClassAd classad2;
    int offset = 0;

    parser.ParseClassAd(two_classads, classad1, offset);
    TEST("Have good offset #1", offset == 10);
    parser.ParseClassAd(two_classads, classad2, offset);
    TEST("Have good offset #2", offset == 20);

    /* ----- Test chained ClassAds ----- */
    //classad1 and classad2 from above test are used.
    ClassAd classad3;

    classad1.ChainToAd(&classad2);
    TEST("classad1's parent is classad2", classad1.GetChainedParentAd() == &classad2);
    have_attribute = classad1.EvaluateAttrInt("b", i);
    TEST("chain has attribute b from parent", (have_attribute == true));
    TEST("chain attribute b from parent is 4", (i == 4));

    have_attribute = classad1.EvaluateAttrInt("a", i);
    TEST("chain has attribute a from self", (have_attribute == true));
    TEST("chain attribute a is 3", (i == 3));

    // Now we modify classad2 (parent) to contain "a".
    success = classad2.InsertAttr("a",7);
    TEST("insert a into parent",(success == true));
    have_attribute = classad1.EvaluateAttrInt("a", i);
    TEST("chain has attribute a from self (overriding parent)", (have_attribute == true));
    TEST("chain attribute a is 3 (overriding parent)", (i == 3));
    have_attribute = classad2.EvaluateAttrInt("a", i);
    TEST("chain parent has attribute a", (have_attribute == true));
    TEST("chain parent attribute a is 7", (i == 7));

    success = classad3.CopyFromChain(classad1);
    TEST("copy from chain succeeded", (success == true));
    have_attribute = classad3.EvaluateAttrInt("b",i);
    TEST("copy of chain has attribute b",(have_attribute == true));
    TEST("copy of chain has attribute b==4",(i==4));

    success = classad3.InsertAttr("c", 6);
    TEST("insert into copy of chain succeeded",(success==true));
    classad3.CopyFromChain(classad1);
    have_attribute = classad3.EvaluateAttrInt("c",i);
    TEST("copy of chain is clean",(have_attribute==false));
    classad3.InsertAttr("c", 6);
    success = classad3.UpdateFromChain(classad1);
    TEST("update from chain succeeded",(success == true));
    have_attribute = classad3.EvaluateAttrInt("c",i);
    TEST("update from chain is merged",(have_attribute==true));
    TEST("update from chain has attribute c==6",(i==6));

    return;
}

/*********************************************************************
 *
 * Function: test_exprlist
 * Purpose:  Test the ExprList class.
 *
 *********************************************************************/
static void test_exprlist(const Parameters &, Results &results)
{
    cout << "Testing the ExprList class...\n";

    Literal *literal1_0;
    Literal *literal2_0;
    Literal *literal2_1;

    vector<ExprTree*> vector1;
    vector<ExprTree*> vector2;

    ExprList *list0;
    ExprList *list0_copy;
    ExprList *list1;
    ExprList *list1_copy;
    ExprList *list2;
    ExprList *list2_copy;

    ExprList::iterator  iter;

    /* ----- Setup Literals, the vectors, then ExprLists ----- */
    literal1_0 = Literal::MakeReal("1.0");
    literal2_0 = Literal::MakeReal("2.0");
    literal2_1 = Literal::MakeReal("2.1");

    vector1.push_back(literal1_0);
    vector2.push_back(literal2_0);
    vector2.push_back(literal2_1);

    list0 = new ExprList();
    list1 = new ExprList(vector1);
    list2 = new ExprList(vector2);

    /* ----- Did the lists get made? ----- */
    TEST("Made list 0", (list0 != NULL));
    TEST("Made list 1", (list1 != NULL));
    TEST("Made list 2", (list2 != NULL));

    /* ----- Are these lists identical to themselves? ----- */
    TEST("ExprList identical 0", list0->SameAs(list0));
    TEST("ExprList identical 1", list1->SameAs(list1));
    TEST("ExprList identical 2", list2->SameAs(list2));

    /* ----- Are they different from each other? ----- */
    TEST("ExprLists different 0-1", !(list0->SameAs(list1)));
    TEST("ExprLists different 1-2", !(list1->SameAs(list2)));
    TEST("ExprLists different 0-2", !(list0->SameAs(list2)));

    /* ----- Check the size of the ExprLists to make sure they are ok ----- */
    TEST("ExprList size 0", (list0->size() == 0));
    TEST("ExprList size 1", (list1->size() == 1));
    TEST("ExprList size 2", (list2->size() == 2));

    /* ----- Make sure the iterators work ----- */
    iter = list0->begin();
    TEST("Reached end of list 0", iter == list0->end());

    iter = list1->begin();
    TEST("Item 1.0 is as expected", (*iter)->SameAs(literal1_0));
    iter++;
    TEST("Reached end of list 1", iter == list1->end());

    iter = list2->begin();
    TEST("Item 2.0 is as expected", (*iter)->SameAs(literal2_0));
    iter++;
    TEST("Item 2.1 is as expected", (*iter)->SameAs(literal2_1));
    iter++;
    TEST("Reached end of list 2", iter == list2->end());

    /* ----- Make copies of the ExprLists ----- */
    list0_copy = (ExprList *) list0->Copy();
    list1_copy = (ExprList *) list1->Copy();
    list2_copy = (ExprList *) list2->Copy();

    /* ----- Did the copies get made? ----- */
    TEST("Made copy of list 0", (list0_copy != NULL));
    TEST("Made copy of list 1", (list1_copy != NULL));
    TEST("Made copy of list 2", (list2_copy != NULL));

    /* ----- Are they identical to the originals? ----- */
    TEST("ExprList self-identity 0", (list0->SameAs(list0_copy)));
    TEST("ExprList self-identity 1", (list1->SameAs(list1_copy)));
    TEST("ExprList self-identity 2", (list2->SameAs(list2_copy)));

    /* ----- Test adding and deleting from a list ----- */
    Literal *add;
    add = Literal::MakeReal("2.2");

    if (list2_copy) {
        iter = list2_copy->begin();
        list2_copy->insert(iter, add);
        TEST("Edited list is different", !(list2->SameAs(list2_copy)));

        iter = list2_copy->begin();
        list2_copy->erase(iter);
        TEST("Twice Edited list is same", (list2->SameAs(list2_copy)));
    }

    delete list0;
    delete list1;
    delete list2;
    delete list0_copy;
    delete list1_copy;
    delete list2_copy;
    
    // Note that we do not delete the Literals that we created, because
    // they should have been deleted when the list was deleted.

    /* ----- Test an ExprList bug that Nate Mueller found ----- */
    ClassAd       *classad;
    ClassAdParser parser;
    bool          b;
    bool          have_attribute;
    bool          can_evaluate;
    Value         value;

    string list_classad_text = "[foo = 3; have_foo = member(foo, {1, 2, 3});]";
    classad = parser.ParseClassAd(list_classad_text);
    have_attribute = classad->EvaluateAttrBool("have_foo", b);
    TEST("Can evaluate list in member function", (have_attribute == true && b == true));

    can_evaluate = classad->EvaluateExpr("member(foo, {1, 2, blah, 3})", value);
    TEST("Can evaluate list in member() outside of ClassAd", can_evaluate == true);

	delete classad;

    return;
}

/*********************************************************************
 *
 * Function: test_value
 * Purpose:  Test the Value class.
 *
 *********************************************************************/
static void test_value(const Parameters &, Results &results)
{
    Value v;
    bool  is_expected_type;

    cout << "Testing the Value class...\n";

    TEST("New value is undefined", (v.IsUndefinedValue()));
    TEST("New value isn't boolean", !(v.IsBooleanValue()));
    TEST("GetType gives UNDEFINED_VALUE", (v.GetType() == Value::UNDEFINED_VALUE));

    v.SetErrorValue();
    TEST("Is error value", (v.IsErrorValue()));
    TEST("GetType gives ERROR_VALUE", (v.GetType() == Value::ERROR_VALUE));

    bool  b;
    v.SetBooleanValue(true);
    b = false;
    is_expected_type = v.IsBooleanValue(b);
    TEST("Value is not undefined", !(v.IsUndefinedValue()));
    TEST("Value is boolean", (v.IsBooleanValue()));
    TEST("Try 2: New value is boolean", (is_expected_type == true))
    TEST("Boolean is true", (b == true));
    TEST("GetType gives BOOLEAN_VALUE", (v.GetType() == Value::BOOLEAN_VALUE));

    double r = 0.0;
    v.SetRealValue(1.0);
    is_expected_type = v.IsRealValue(r);
    TEST("Value is real", is_expected_type);
    TEST("Real is 1.0", (r == 1.0));
    TEST("GetType gives REAL_VALUE", (v.GetType() == Value::REAL_VALUE));
    TEST("Real is a number", v.IsNumber());

    int i = 0;
    v.SetIntegerValue(1);
    is_expected_type = v.IsIntegerValue(i);
    TEST("Value is integer", is_expected_type);
    TEST("Integer is 1", (i == 1));
    TEST("GetType gives INTEGER_VALUE", (v.GetType() == Value::INTEGER_VALUE));
    TEST("Integer is a number", v.IsNumber());

    const char *s = NULL;
    v.SetStringValue("Robert-Houdin");
    is_expected_type = v.IsStringValue(s);
    TEST("Value is string", is_expected_type);
    TEST("String is 'Robert-Houdin'", (0 == strcmp(s, "Robert-Houdin")));
    TEST("GetType gives STRING_VALUE", (v.GetType() == Value::STRING_VALUE));

    abstime_t at = { 10, 10 };
    v.SetAbsoluteTimeValue(at);
    at.secs = at.offset = 0;
    is_expected_type = v.IsAbsoluteTimeValue(at);
    TEST("Value is absolute time", is_expected_type);
    TEST("Absolute time is 10, 0", (10 == at.secs && 10 == at.offset));
    TEST("GetType gives ABSOLUTE_TIME_VALUE", (v.GetType() == Value::ABSOLUTE_TIME_VALUE));

    time_t rt = 0;
    v.SetRelativeTimeValue((time_t) 10);
    is_expected_type = v.IsRelativeTimeValue(rt);
    TEST("Value is relative time", is_expected_type);
    TEST("Relative time is 10", (10 == rt));
    TEST("GetType gives RELATIVE_TIME_VALUE", (v.GetType() == Value::RELATIVE_TIME_VALUE));

    ExprList *l = new ExprList();
    ExprList *ll = NULL;
    v.SetListValue(l);
    is_expected_type = v.IsListValue(ll);
    TEST("Value is list value", is_expected_type);
    TEST("List value is correct", l == ll);
    TEST("GetType gives LIST_VALUE", (v.GetType() == Value::LIST_VALUE));
    delete l;

    classad_shared_ptr<ExprList> sl(new ExprList());
    ll = NULL;
    v.SetListValue(sl);
    is_expected_type = v.IsListValue(ll);
    TEST("Value is list value", is_expected_type);
    TEST("List value is correct", sl.get() == ll);
    TEST("GetType gives SLIST_VALUE", (v.GetType() == Value::SLIST_VALUE));

    ClassAd *c = new ClassAd();
    ClassAd *cc = NULL;
    v.SetClassAdValue(c);
    is_expected_type = v.IsClassAdValue(cc);
    TEST("Value is ClassAd value", is_expected_type);
    TEST("ClassAd value is correct", c == cc);
    TEST("GetType gives CLASSAD_VALUE", (v.GetType() == Value::CLASSAD_VALUE));
    delete c;

    return;
}

/*********************************************************************
 *
 * Function: test_collection
 * Purpose:  Test the ClassAdCollection class. Note that we test the
 *           local Collections only: we don't test the server/client
 *           versions available in ClassAdCollectionServer and 
 *           ClassAdCollectionClient.
 *
 *********************************************************************/
static void test_collection(const Parameters &, Results &results)
{
    bool               success;
    static const char  *collection_log_file_name = "collection.log";
    ClassAd            *machine1, *machine2, *machine3;
    ClassAd            *retrieved;
    ClassAdParser      parser;
    ClassAdCollection  *collection;

    cout << "Testing the ClassAdCollection class...\n";

    /* ----- Create the ClassAds that we'll put into the collection ----- */
    machine1 = parser.ParseClassAd("[ Type = \"machine\"; OS = \"Linux\"; Memory = 5000;]", true);
    machine2 = parser.ParseClassAd("[ Type = \"machine\"; OS = \"Linux\"; Memory = 3000;]", true);
    machine3 = parser.ParseClassAd("[ Type = \"machine\"; OS = \"Windows\"; Memory = 6000;]", true);
    TEST("Made ClassAds for Collection", ((machine1 != NULL) && (machine2 != NULL) && (machine3 != NULL)));

    /* ----- Create the collection ----- */
    collection = new ClassAdCollection();
    TEST("Made Collection", collection != NULL);
    if (collection == NULL) {
        return;
    }
    
    // We delete the log file so we start with a fresh slate.
    // Later on we will make a different collection from the log file
    // that we create, so we can test that it does what we want.
#if defined ( WIN32 )
	_unlink(collection_log_file_name);
#else
	unlink(collection_log_file_name);
#endif
    success = collection->InitializeFromLog(collection_log_file_name);
    TEST("Initialized from empty log", success == true);

    /* ----- Add the machines to the collection ----- */
    success = collection->AddClassAd("machine1", machine1);
    TEST("Added machine1 to collection", success == true);
    success = collection->AddClassAd("machine2", machine2);
    TEST("Added machine2 to collection", success == true);
    success = collection->AddClassAd("machine3", machine3);
    TEST("Added machine3 to collection", success == true);
    /* ----- Put in one machine twice, to make sure it doesn't break ----- */
    //success = collection->AddClassAd("machine3", machine3);
    //TEST("Added machine3 to collection", success == true);


    /* ----- Make sure that they are in the collection ----- */
    retrieved = collection->GetClassAd("machine1");
    TEST("machine1 in the collection", retrieved == machine1);
    retrieved = collection->GetClassAd("machine2");
    TEST("machine2 in the collection", retrieved == machine2);
    retrieved = collection->GetClassAd("machine3");
    TEST("machine3 in the collection", retrieved == machine3);

    /* ----- Make a couple of subviews ----- */
    success = collection->CreateSubView("Linux-View", "root",
                                        "(other.OS == \"Linux\")", "", "");
    TEST("Create Machine-View", success == true);
    success = collection->CreateSubView("BigLinux-View", "Linux-View",
                                        "(other.Memory >= 5000)", "", "");
    TEST("Create BigMachine-View", success == true);

    /* ----- Now test that the right things are in each view ----- */
    TEST("machine1 in Linux-View", (check_in_view(collection, "Linux-View", "machine1") == true));
    TEST("machine2 in Linux-View", (check_in_view(collection, "Linux-View", "machine2") == true));
    TEST("machine3 not in Linux-View", (check_in_view(collection, "Linux-View", "machine3") == false));
    TEST("machine1 not in BigLinux-View", (check_in_view(collection, "BigLinux-View", "machine1") == true));
    TEST("machine2 in BigLinux-View", (check_in_view(collection, "BigLinux-View", "machine2") == false));
    TEST("machine3 not in BigLinux-View", (check_in_view(collection, "BigLinux-View", "machine3") == false));

    bool have_machine1, have_machine2, have_machine3, have_others;
    View::const_iterator view_iterator;

    /* ----- Now test the iterator for the root view ----- */
    const View *root_view = collection->GetView("root");

    have_machine1 = have_machine2 = have_machine3 = have_others = false;
    for (view_iterator = root_view->begin(); 
         view_iterator != root_view->end();
         view_iterator++) {
        string key;
        view_iterator->GetKey(key);
        if (key == "machine1") {
            have_machine1 = true;
        } else if (key == "machine2") {
            have_machine2 = true;
        } else if (key == "machine3") {
            have_machine3 = true;
        } else {
            have_others = true;
        }
    }
    TEST("machine1 found with iterator",  have_machine1 == true);
    TEST("machine2 found with iterator",  have_machine2 == true);
    TEST("machine3 found with iterator",  have_machine3 == true);
    TEST("No others found with iterator", have_others   == false);

    /* ----- Now test the iterator for a subview ----- */
    const View *linux_view = collection->GetView("Linux-View");

    have_machine1 = have_machine2 = have_machine3 = have_others = false;
    for (view_iterator = linux_view->begin(); 
         view_iterator != linux_view->end();
         view_iterator++) {
        string key;
        view_iterator->GetKey(key);
        if (key == "machine1") {
            have_machine1 = true;
        } else if (key == "machine2") {
            have_machine2 = true;
        } else {
            have_others = true;
        }
    }
    TEST("machine1 found with iterator",  have_machine1 == true);
    TEST("machine2 found with iterator",  have_machine2 == true);
    TEST("No others found with iterator", have_others   == false);

    /* ----- Reread from the collection log and see if everything is still good ----- */
    delete collection;
    collection = new ClassAdCollection();
    TEST("Made Collection again", collection != NULL);
    if (collection == NULL) {
        return;
    }
    success = collection->InitializeFromLog(collection_log_file_name);
    TEST("Initialized from full log", success == true);
    TEST("machine1 in root", (check_in_view(collection, "root", "machine1") == true));
    TEST("machine2 in root", (check_in_view(collection, "root", "machine2") == true));
    TEST("machine3 not in root", (check_in_view(collection, "root", "machine3") == true));
    TEST("machine1 in Linux-View", (check_in_view(collection, "Linux-View", "machine1") == true));
    TEST("machine2 in Linux-View", (check_in_view(collection, "Linux-View", "machine2") == true));
    TEST("machine3 not in Linux-View", (check_in_view(collection, "Linux-View", "machine3") == false));
    TEST("machine1 not in BigLinux-View", (check_in_view(collection, "BigLinux-View", "machine1") == true));
    TEST("machine2 in BigLinux-View", (check_in_view(collection, "BigLinux-View", "machine2") == false));
    TEST("machine3 not in BigLinux-View", (check_in_view(collection, "BigLinux-View", "machine3") == false));

    delete collection;

    unlink(collection_log_file_name);
    return;
}

static bool check_in_view(
    ClassAdCollection  *collection,
    string             view_name,
    string             classad_name)
{
    bool have_view;
    bool in_view;

    in_view = false;

    LocalCollectionQuery  query;

    query.Bind(collection);
    
    have_view = query.Query(view_name, NULL);
    if (have_view) {
        string classad_key;
        for (query.ToFirst(), query.Current(classad_key); 
             !query.IsAfterLast(); 
             query.Next(classad_key)) {
            if (!classad_key.compare(classad_name)) {
                in_view = true;
                break;
            }
        }
    }
    return in_view;
}

/*********************************************************************
 *
 * Function: test_utils
 * Purpose:  Test utils
 *
 *********************************************************************/
static void test_utils(const Parameters &, Results &results)
{
    cout << "Testing little utilities...\n";

    TEST("1800 is not a leap year", !is_leap_year(1800));
    TEST("1900 is not a leap year", !is_leap_year(1900));
    TEST("2000 is a leap year", is_leap_year(2000));
    TEST("2001 is not a leap year", !is_leap_year(2001));
    TEST("2002 is not a leap year", !is_leap_year(2002));
    TEST("2003 is not a leap year", !is_leap_year(2003));
    TEST("2004 is a leap year", is_leap_year(2004));

    TEST("70, 9, 24 -> 25469",      fixed_from_gregorian(70, 9, 24) == 25469);
    TEST("135, 10, 2 -> 49217",     fixed_from_gregorian(135, 10, 2) == 49217);
    TEST("470, 1, 8 -> 171307",     fixed_from_gregorian(470, 1, 8) == 171307);
    TEST("576, 5, 20 -> 210155",    fixed_from_gregorian(576, 5, 20) == 210155);
    TEST("694,  11, 10 -> 253427",  fixed_from_gregorian(694,  11, 10) == 253427);
    TEST("1013,  4, 25 -> 369740",  fixed_from_gregorian(1013,  4, 25) == 369740);
    TEST("1096,  5, 24 -> 400085",  fixed_from_gregorian(1096,  5, 24) == 400085);
    TEST("1190,  3, 23 -> 434355",  fixed_from_gregorian(1190,  3, 23) == 434355);
    TEST("1240,  3, 10 -> 452605",  fixed_from_gregorian(1240,  3, 10) == 452605);
    TEST("1288,  4, 2 -> 470160",   fixed_from_gregorian(1288,  4, 2) == 470160);
    TEST("1298,  4, 27 -> 473837",  fixed_from_gregorian(1298,  4, 27) == 473837);
    TEST("1391,  6, 12 -> 507850",  fixed_from_gregorian(1391,  6, 12) == 507850);
    TEST("1436,  2, 3 -> 524156",   fixed_from_gregorian(1436,  2, 3) == 524156);
    TEST("1492,  4, 9 -> 544676",   fixed_from_gregorian(1492,  4, 9) == 544676);
    TEST("1553,  9, 19 -> 567118",  fixed_from_gregorian(1553,  9, 19) == 567118);
    TEST("1560,  3, 5 -> 569477",   fixed_from_gregorian(1560,  3, 5) == 569477);
    TEST("1648,  6, 10 -> 601716",  fixed_from_gregorian(1648,  6, 10) == 601716);
    TEST("1680,  6, 30 -> 613424",  fixed_from_gregorian(1680,  6, 30) == 613424);
    TEST("1716,  7, 24 -> 626596",  fixed_from_gregorian(1716,  7, 24) == 626596);
    TEST("1768,  6, 19 -> 645554",  fixed_from_gregorian(1768,  6, 19) == 645554);
    TEST("1819,  8, 2 -> 664224",   fixed_from_gregorian(1819,  8, 2) == 664224);
    TEST("1839,  3, 27 -> 671401",  fixed_from_gregorian(1839,  3, 27) == 671401);
    TEST("1903,  4, 19 -> 694799",  fixed_from_gregorian(1903,  4, 19) == 694799);
    TEST("1929,  8, 25 -> 704424",  fixed_from_gregorian(1929,  8, 25) == 704424);
    TEST("1941,  9, 29 -> 708842",  fixed_from_gregorian(1941,  9, 29) == 708842);
    TEST("1943,  4, 19 -> 709409",  fixed_from_gregorian(1943,  4, 19) == 709409);
    TEST("1943,  10, 7 -> 709580",  fixed_from_gregorian(1943,  10, 7) == 709580);
    TEST("1992,  3, 17 -> 727274",  fixed_from_gregorian(1992,  3, 17) == 727274);
    TEST("1996,  2, 25 -> 728714",  fixed_from_gregorian(1996,  2, 25) == 728714);
    TEST("2038,  11, 10 -> 744313", fixed_from_gregorian(2038,  11, 10) == 744313);
    TEST("2094,  7, 18 -> 764652",  fixed_from_gregorian(2094,  7, 18) == 764652);

    int weekday;
    int yearday;
    day_numbers(2005, 1, 1, weekday, yearday);
    TEST("Jan 1, 2005->6, 0", weekday==6 && yearday==0);
    day_numbers(2005, 1, 2, weekday, yearday);
    TEST("Jan 2, 2005->6, 1", weekday==0 && yearday==1);
    day_numbers(2005, 12, 30, weekday, yearday);
    TEST("Dec 30, 2005->5, 363", weekday==5 && yearday==363);
    day_numbers(2005, 12, 31, weekday, yearday);
    TEST("Dec 31, 2005->6, 364", weekday==6 && yearday==364);
    day_numbers(2004, 12, 31, weekday, yearday);
    TEST("Dec 31, 2005->5, 365", weekday==5 && yearday==365);
    return;
}

/*********************************************************************
 *
 * Function: print_version
 * Purpose:  
 *
 *********************************************************************/
static void print_version(void)
{
    string classad_version;

    ClassAdLibraryVersion(classad_version);

    cout << "ClassAd Unit Tester v" << classad_version << "\n\n";

    return;
}
