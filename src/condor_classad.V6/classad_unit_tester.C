/*********************************************************************
 *
 * Condor ClassAd library
 * Copyright (C) 1990-2003, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI and Rajesh Raman.
 *
 * This source code is covered by the Condor Public License, which can
 * be found in the accompanying LICENSE file, or online at
 * www.condorproject.org.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
 * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
 * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
 * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
 * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
 * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
 * RIGHT.
 *
 *********************************************************************/

#if defined(CLASSAD_DISTRIBUTION)
#include "classad_distribution.h"
#else
#include "condor_classad.h"
#endif
#include "lexerSource.h"
#include "xmlSink.h"
#include <fstream>
#include <iostream>
#include <ctype.h>
#include <assert.h>

using namespace std;
#ifdef WANT_NAMESPACES
using namespace classad;
#endif

#define TEST(name, test) {if (test) \
results.AddSuccessfulTest(name, __LINE__); \
else results.AddFailedTest(name, __LINE__);}

class Parameters
{
public: 
	bool      debug;
    bool      verbose;
    bool      very_verbose;

    bool      check_all;
    bool      check_classad;
	bool      check_exprlist;
    bool      check_value;
    bool      check_literal;
    bool      check_match;
    bool      check_operator;
    bool      check_collection;
	void ParseCommandLine(int argc, char **argv);
};

class Results
{
public:
    Results(const Parameters &parameters);

    void AddSuccessfulTest(const char *name, int line_number);
    void AddFailedTest(const char *name, int line_number);

    void GetResults(int &number_of_errors, int &number_of_tests);

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

static void test_exprlist(const Parameters &parameters, Results &results);
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
    check_classad       = false;
	check_exprlist      = false;
    check_value         = false;
    check_literal       = false;
    check_match         = false;
    check_operator      = false;
    check_collection    = false;

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
        cout << "    -classad:    test the ClassAd class.\n";
        cout << "    -exprlist:   test the ExprList class.\n";
        cout << "    -value:      test the Value class.\n";
        cout << "    -literal:    test the Literal class.\n";
        cout << "    -match:      test the MatchClassAd class.\n";
        cout << "    -operator:   test the Operator class.\n";
        cout << "    -collection: test the Collection class.\n";
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

void Results::GetResults(int &number_of_errors, int &number_of_tests)
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
    bool        success;
    string      line;
    Parameters  parameters;

    /* ----- Setup ----- */
    print_version();
    parameters.ParseCommandLine(argc, argv);
    Results  results(parameters);

    /* ----- Run tests ----- */
    if (parameters.check_all || parameters.check_classad) {
    }
    if (parameters.check_all || parameters.check_exprlist) {
        test_exprlist(parameters, results);
    }
    if (parameters.check_all || parameters.check_value) {
    }
    if (parameters.check_all || parameters.check_literal) {
    }
    if (parameters.check_all || parameters.check_match) {
    }
    if (parameters.check_all || parameters.check_operator) {
    }
    if (parameters.check_all || parameters.check_collection) {
    }

    /* ----- Report ----- */
    cout << endl;
    results.GetResults(number_of_errors, number_of_tests);
    if (number_of_errors > 0) {
        success = false;
        cout << "Finished with errors: \n";
        cout << "    " << number_of_errors << " errors\n";
        cout << "    " << number_of_tests << " tests\n";
    } else {
        success = true;
        cout << "Finished with no errors.\n";
        cout << "    " << number_of_tests << " tests\n";
    }
    return success;
}

/*********************************************************************
 *
 * Function: test_exprlist
 * Purpose:  Test the ExprList class.
 *
 *********************************************************************/
static void test_exprlist(const Parameters &parameters, Results &results)
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

    iter = list2_copy->begin();
    list2_copy->insert(iter, add);
    TEST("Edited list is different", !(list2->SameAs(list2_copy)));

    iter = list2_copy->begin();
    list2_copy->erase(iter);
    TEST("Twice Edited list is same", (list2->SameAs(list2_copy)));

    delete list0;
    delete list1;
    delete list2;
    delete list0_copy;
    delete list1_copy;
    delete list2_copy;
    
    // Note that we do not delete the Literals that we created, because
    // they should have been deleted when the list was deleted.

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
