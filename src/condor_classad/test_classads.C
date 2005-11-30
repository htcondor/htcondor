/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
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
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "condor_classad.h"
#include "condor_xml_classads.h"
#include "stringSpace.h"
#include "condor_scanner.h" 
#include "iso_dates.h"
#ifdef CLASSAD_FUNCTIONS
#include "condor_config.h"
#endif

/*----------------------------------------------------------
 *
 *                        Global Variables
 *
 *----------------------------------------------------------*/

#define LARGE_NUMBER_OF_CLASSADS 10000

#ifdef USE_STRING_SPACE_IN_CLASSADS
extern StringSpace classad_string_space; // for debugging only!
#endif

#define NUMBER_OF_CLASSAD_STRINGS (sizeof(classad_strings)/sizeof(char *))
char *classad_strings[] = 
{
#ifdef CLASSAD_FUNCTIONS
	"A = 1, B=2, C = 3, D='2001-04-05T12:14:15', E=script(\"testscript\";\"|/dev/zero\";3), G=GetTime(1), H=foo(1)",
#else
	"A = 1, B=2, C = 3, D='2001-04-05T12:14:15', E=TRUE",
#endif
	"A = 0.7, B=2, C = 3, D = \"alain\", MyType=\"foo\", TargetType=\"blah\"",

	"Rank = (Memory >= 50)",

    "Env = \"CPUTYPE=i86pc;GROUP=unknown;LM_LICENSE_FILE=/p/multifacet/"
            "projects/simics/dist10/v9-sol7-gcc/sys/flexlm/license.dat;"
            "SIMICS_HOME=.;SIMICS_EXTRA_LIB=./modules;PYTHONPATH=./modules;"
            "MACHTYPE=i386;SHELL=/bin/tcsh;PATH=/s/std/bin:/usr/afsws/bin:"
        	"/usr/ccs/bin:/usr/ucb:/bin:/usr/bin:/usr/X11R6/bin:/unsup/condor/bin:.\"",

    "MyType=\"Job\", TargetType=\"Machine\", Owner = \"alain\", Requirements = (TARGET.Memory > 50)",
    "MyType=\"Machine\", TargetType=\"Job\", Memory = 100,  Requirements = (TARGET.owner == \"alain\")",
    "MyType=\"Machine\", TargetType=\"Job\", Memory = 40, Requirements = (TARGET.owner == \"alain\")",
    "MyType=\"Machine\", TargetType=\"Job\", Memory = 100, Requirements = (TARGET.owner != \"alain\")",

	// A classad to test PrintToNewStr()--we need to ensure everything
	// we can print is properly accounted for. 
	"Requirements = (a > 3) && (b >= 1.3) && (c < MY.rank) && ((d <= TARGET.RANK) "
    "|| (g == \"alain\") || (g != \"roy\") || (h =?= 5) || (i =!= 6)) "
    "&& ((a + b) < (c-d)) && ((e * f) > (g / h)) && x == false && y == true "
    "&& z == f && j == t",

	/* Some classads to test GetReferences() 
	 * The first one is simple, but we can also check that things aren't listed twice. */
	"Memory = 60, Disk = 40, OS = Linux, Requirements = ((ImageSize > Memory) "
	"&& (AvailableDisk > Disk) && (AvailableDisk > Memory) && (ImageSize > Disk))",
	/* The second one is to test MY and TARGET. OTHER should be treated like target. */
	"Memory = 60, Disk = 40, OS = Linux, Requirements = ((TARGET.ImageSize > MY.Memory) "
	"&& (AvailableDisk > Disk) && (OTHER.AvailableDisk > MY.Memory) && (TARGET.ImageSize > MY.Disk))",

	/* Test case sensitivity */
	"DoesMatch = \"Bone Machine\" == \"bone machine\" && \"a\" =?= \"a\" && \"a\" =!= \"A\","
	"DoesntMatch = \"a\" =?= \"A\"",

    /* Test XML preservation of spaces */
    "A = \"\", B=\" \""
};

/*----------------------------------------------------------
 *
 *                     Private Datatypes
 *
 *----------------------------------------------------------*/
struct parameters
{
	bool verbose;
	bool test_scanner;
	bool test_copy_constructor;
	bool test_assignment;
	bool test_classads;
	bool test_references;
	bool test_xml;
	bool test_dirty;
	bool test_functions;
    bool test_random;
};

class TestResults
{
public:
	TestResults()
	{
		number_of_tests        = 0;
		number_of_tests_passed = 0;
		number_of_tests_failed = 0;
		return;
	};

	~TestResults()
	{
		return;
	}

	void AddResult(bool passed)
	{
		number_of_tests++;
		if (passed) {
			number_of_tests_passed++;
		} else {
			number_of_tests_failed++;
		}
		return;
	}

	void PrintResults(void)
	{
		if (number_of_tests_failed == 0) {
			printf("All %d tests passed.\n", number_of_tests);
		} else {
			printf("%d of %d tests failed.\n",
				   number_of_tests_failed, number_of_tests);
		}
		return;
	}

private:
	int number_of_tests;
	int number_of_tests_passed;
	int number_of_tests_failed;
};

/*----------------------------------------------------------
 *
 *                     Private Functions
 *
 *----------------------------------------------------------*/
static void parse_command_line(int argc, char **argv, 
    struct parameters *parameters);
void test_scanner(TestResults *results);
void test_token_text(const Token *token, char *text, int line_number, 
    TestResults *test_results);
void test_token_type(const Token *token, int token_type, int line_number,
    TestResults *test_results);
void test_token_integer(const Token *token, int integer, 
    int line_number, TestResults *test_results);
void test_integer_value(ClassAd *classad, const char *attribute_name, 
    int expected_value,int line_number, TestResults *results);
void test_eval_bool(ClassAd *classad, const char  *attribute_name,
	int expected_value, int line_number, TestResults *results);
void test_token_float(const Token *token, float number,
    int line_number, TestResults *test_results);
void test_string_value(ClassAd *classad, const char *attribute_name,
    const char *expected_value, int line_number,
    TestResults *results);
void test_time_string_value(ClassAd *classad, const char *attribute_name,
    const char *expected_value, int line_number,
    TestResults *results);
void test_time_value(ClassAd *classad, const char *attribute_name,
    const struct tm  &expected_time, int line_number,
    TestResults *results);
void test_mytype(ClassAd *classad, const char *expected_value, 
	int line_number, TestResults *results);
void test_targettype(ClassAd *classad, const char *expected_value, 
    int line_number, TestResults *results);
void test_ads_match(ClassAd *classad_1, ClassAd *classad_2,
    int line_number, TestResults  *results);
void test_ads_dont_match(ClassAd *classad_1, ClassAd *classad_2,
    int line_number, TestResults *results);
void test_printed_version(ClassAd *classad, const char *attribute_name, 
    char *expected_string, int line_number, TestResults *results);
void test_in_references(char *name,	StringList &references,
    int line_number, TestResults *results);
void test_not_in_references(char *name,	StringList &references,
    int line_number, TestResults *results);
void test_dirty_attribute(
    TestResults *results);
#ifdef CLASSAD_FUNCTIONS
static void test_functions(
	TestResults  *results);
#endif
void print_truncated_string(const char *s, int max_characters);
static void make_big_string(int length, char **string,
    char **quoted_string);
void test_random(TestResults *results);
void test_equality(TestResults *results);

int 
main(
	 int  argc,
	 char **argv)
{
	ClassAd              **classads;
	struct parameters    parameters;
	TestResults          test_results;
	int                  classad_index = 0; 
	parse_command_line(argc, argv, &parameters);

	if (parameters.verbose) {
		printf("sizeof(ClassAd) = %d\n", sizeof(ClassAd));
		printf("sizeof(AttrListElem) = %d\n", sizeof(AttrListElem));
		printf("sizeof(ExprTree) = %d\n", sizeof(ExprTree));
		printf("We have %d classads.\n", NUMBER_OF_CLASSAD_STRINGS);
	}

	if (parameters.test_scanner) {
		printf("Testing classad scanner...\n");
		test_scanner(&test_results);
	}

	classads = new ClassAd *[NUMBER_OF_CLASSAD_STRINGS];

	printf("\nCreating ClassAds...\n");
	if (parameters.test_copy_constructor) {
		printf("(Will test copy constructor at the same time.)\n");
	}
	else if (parameters.test_assignment) {
		printf("(Will test assignment at the same time.)\n");
	}
	for (  classad_index = 0; 
		   classad_index < (int) NUMBER_OF_CLASSAD_STRINGS;
		   classad_index++) {
		ClassAd *original, *duplicate;

		printf("%s\n", classad_strings[classad_index]);

		original = new ClassAd(classad_strings[classad_index], ',');
		classads[classad_index] = original;

		if (parameters.test_copy_constructor) {
			duplicate = new ClassAd(*original);
			delete original;
			classads[classad_index] = duplicate;
		}
		else if (parameters.test_assignment) {
			duplicate = new ClassAd("A = 1, MyType=\"x\", TargetType=\"y\"",
									',');
			*duplicate = *original;
			delete original;
			classads[classad_index] = duplicate;
		}

		if (parameters.verbose) {
			printf("ClassAd %d:\n", classad_index);
			classads[classad_index]->fPrint(stdout);
			printf("\n");
		}
	}

	if (parameters.test_xml) {
		printf("Testing XML...\n");
		for (  int classad_index = 0; 
			   classad_index < (int) NUMBER_OF_CLASSAD_STRINGS;
			   classad_index++) {
			ClassAdXMLUnparser unparser;
			ClassAdXMLParser   parser;
			ClassAd            *after_classad;
			MyString xml, before_classad_string, after_classad_string;
			
			// 1) Print each ClassAd to a string.
			// 2) Convert it to XML and back and 
			// 3) see if the string is the same. 
			classads[classad_index]->sPrint(before_classad_string);

			unparser.SetUseCompactSpacing(false);
			unparser.Unparse(classads[classad_index], xml);
			if (parameters.verbose) {
				printf("Classad %d in XML:\n%s", classad_index, xml.Value());
			}
			after_classad = parser.ParseClassAd(xml.Value());

			after_classad->sPrint(after_classad_string);
			if (strcmp(before_classad_string.Value(), after_classad_string.Value()) != 0) {
				printf("Failed: XML Parse and UnParse for classad %d\n", classad_index);
				printf("---- Original ClassAd:\n%s\n", before_classad_string.Value());
				printf("---- After ClassAd:\n%s\n", after_classad_string.Value());
				printf("---- Intermediate XML:\n%s\n", xml.Value());
				test_results.AddResult(false);
			} else {
				printf("Passed: XML Parse and Unparse for classad %d\n\n", classad_index);
				test_results.AddResult(true);
			}
			delete after_classad;
		}
	}

	if (parameters.test_classads) {

		printf("\nTesting ClassAds...\n");
		
		test_integer_value(classads[0], "A", 1, __LINE__, &test_results);
		test_integer_value(classads[0], "B", 2, __LINE__, &test_results);
		test_integer_value(classads[0], "C", 3, __LINE__, &test_results);
		test_mytype(classads[0], "", __LINE__, &test_results);
		test_targettype(classads[0], "", __LINE__, &test_results);
		test_time_string_value(classads[0], "D", "2001-04-05T12:14:15",
							   __LINE__, &test_results);
		{
			struct tm  time;
			time.tm_year = 101; // year 2001
			time.tm_mon  = 03;  // that's april, not march. 
			time.tm_mday = 05;
			time.tm_hour = 12;
			time.tm_min  = 14;
			time.tm_sec  = 15;
			test_time_value(classads[0], "D", time, __LINE__, &test_results);
		}

		test_string_value(classads[1], "D", "alain", __LINE__, &test_results);
		test_mytype(classads[1], "foo", __LINE__, &test_results);
		test_targettype(classads[1], "blah", __LINE__, &test_results);
		
		test_ads_match(classads[4], classads[5], __LINE__, &test_results);
		test_ads_match(classads[5], classads[4], __LINE__, &test_results);
		test_ads_dont_match(classads[4], classads[6], __LINE__, &test_results);
		test_ads_dont_match(classads[6], classads[4], __LINE__, &test_results);
		test_ads_dont_match(classads[4], classads[7], __LINE__, &test_results);
		test_ads_dont_match(classads[7], classads[4], __LINE__, &test_results);
		
		// Here we test that we can scan large inputs and everything works.
		char *variable, *string, *expression;
		
		make_big_string(15000, &variable, NULL);
		make_big_string(25000, &string, NULL);
		expression = (char *) malloc(50000);
		sprintf(expression, "%s = \"%s\"", variable, string);
		
		classads[0]->Insert(expression);
		
		test_string_value(classads[0], variable, string, __LINE__, &test_results);

		test_printed_version(classads[2], "Rank",         classad_strings[2], 
							 __LINE__, &test_results);
		test_printed_version(classads[3], "Env",          classad_strings[3],
							 __LINE__, &test_results);
		test_printed_version(classads[8], "Requirements", 
			"Requirements = (a > 3) && (b >= 1.300000) && (c < MY.rank) && ((d <= TARGET.RANK) "
		    "|| (g == \"alain\") || (g != \"roy\") || (h =?= 5) || (i =!= 6)) "
            "&& ((a + b) < (c - d)) && ((e * FALSE) > (g / h)) && x == FALSE && y == TRUE "
            "&& z == FALSE && j == TRUE",
			 __LINE__, &test_results);

		ClassAd *big_classad = new ClassAd(expression, ',');
		test_printed_version(big_classad, variable, expression, __LINE__, &test_results);
		delete big_classad;
		free(variable);
		free(expression);
		free(string);

		/* Test case insensitivity */
		test_eval_bool(classads[11], "DoesMatch",   1, __LINE__, &test_results);
		test_eval_bool(classads[11], "DoesntMatch", 0, __LINE__, &test_results);

        /* Test that reading from a FILE works */
        FILE *classad_file;
        ClassAd *classad_from_file;
        classad_file = fopen("classad_file", "w");
        classads[1]->fPrint(classad_file);
        fprintf(classad_file, "***\n");
        fclose(classad_file);

        int iseof, error, empty;
        classad_file = fopen("classad_file", "r");
        classad_from_file = new ClassAd(classad_file, "***", iseof, error, empty);
        fclose(classad_file);
		test_integer_value(classad_from_file, "B", 2, __LINE__, &test_results);
		test_integer_value(classad_from_file, "C", 3, __LINE__, &test_results);
		test_string_value(classad_from_file, "D", "alain", __LINE__, &test_results);
        delete classad_from_file;
	}

	if (parameters.test_references) {
		printf("\nTesting References...\n");
		StringList  *internal_references; 
		StringList  *external_references; 

		internal_references = new StringList;
		external_references = new StringList;

		classads[9]->GetReferences("Requirements", *internal_references,
								   *external_references);
		test_in_references("Memory", *internal_references, __LINE__, &test_results);
		test_in_references("Disk", *internal_references, __LINE__, &test_results);
		test_in_references("ImageSize", *external_references, __LINE__, &test_results);
		test_in_references("AvailableDisk", *external_references, __LINE__, &test_results);
		
		test_not_in_references("Memory", *external_references, __LINE__, &test_results);
		test_not_in_references("Disk", *external_references, __LINE__, &test_results);
		test_not_in_references("ImageSize", *internal_references, __LINE__, &test_results);
		test_not_in_references("AvailableDisk", *internal_references, __LINE__, &test_results);
		test_not_in_references("Linux", *internal_references, __LINE__, &test_results);
		test_not_in_references("Linux", *external_references, __LINE__, &test_results);
		delete internal_references;
		delete external_references;

		internal_references = new StringList;
		external_references = new StringList;

		classads[10]->GetReferences("Requirements", *internal_references,
								   *external_references);
		test_in_references("Memory", *internal_references, __LINE__, &test_results);
		test_in_references("Disk", *internal_references, __LINE__, &test_results);
		test_in_references("ImageSize", *external_references, __LINE__, &test_results);
		test_in_references("AvailableDisk", *external_references, __LINE__, &test_results);
		
		test_not_in_references("Memory", *external_references, __LINE__, &test_results);
		test_not_in_references("Disk", *external_references, __LINE__, &test_results);
		test_not_in_references("ImageSize", *internal_references, __LINE__, &test_results);
		test_not_in_references("AvailableDisk", *internal_references, __LINE__, &test_results);
		test_not_in_references("Linux", *internal_references, __LINE__, &test_results);
		test_not_in_references("Linux", *external_references, __LINE__, &test_results);

		delete internal_references;
		delete external_references;
	}

	if (parameters.test_dirty) {
		test_dirty_attribute(&test_results);
	}

#ifdef CLASSAD_FUNCTIONS
	if (parameters.test_functions) {
		test_functions(&test_results);
	}
#endif

    if (parameters.test_random) {
        test_random(&test_results);
    }

    test_equality(&test_results);

	//ClassAd *many_ads[LARGE_NUMBER_OF_CLASSADS];
	/*
	for (int i = 0; i < LARGE_NUMBER_OF_CLASSADS; i++) {
		many_ads[i] = new ClassAd(classad_strings[2], ',');
		//print_memory_usage();
	}
	system("ps -v");
	*/

	test_results.PrintResults();
#ifdef USE_STRING_SPACE_IN_CLASSADS
	//classad_string_space.dump();
#endif

	// Clean up when we're done.
	for (  classad_index = 0; 
		   classad_index < (int) NUMBER_OF_CLASSAD_STRINGS;
		   classad_index++) {
		delete classads[classad_index];
	}
	delete [] classads;

	return 0;
}

/***************************************************************
 *
 * Function: parse_command_line
 * Purpose:  Read the command line arguments and fill in 
 *           the parameters structure to reflect them. 
 *
 ***************************************************************/
static void
parse_command_line(
	 int                argc,
	 char               **argv,
	 struct parameters  *parameters)
{
	int   argument_index;
	bool  dump_usage;

	dump_usage = false;

	parameters->verbose               = false;
	parameters->test_scanner          = false;
	parameters->test_copy_constructor = false;
	parameters->test_assignment       = false;
	parameters->test_classads         = false;
	parameters->test_references       = false;
	parameters->test_xml              = false;
	parameters->test_dirty            = false;
	parameters->test_functions        = false;
    parameters->test_random           = false;
	argument_index = 1;

	while (argument_index < argc) {
		if (!strcmp(argv[argument_index], "-v")
			|| !strcmp(argv[argument_index], "-verbose")) {
			parameters->verbose = !parameters->verbose;
		} else if (!strcmp(argv[argument_index], "-h")
			|| !strcmp(argv[argument_index], "-help")
			|| !strcmp(argv[argument_index], "-?")) {
			dump_usage = true;
		} else if (!strcmp(argv[argument_index], "-a")
		    || !strcmp(argv[argument_index], "-all")) {
			parameters->test_scanner          = true;
			if (!parameters->test_assignment) {
				parameters->test_copy_constructor = true;
			}
			parameters->test_classads         = true;
			parameters->test_references       = true;
			parameters->test_xml              = true;
			parameters->test_dirty            = true;
			parameters->test_functions        = true;
            parameters->test_random           = true;
		} else if (!strcmp(argv[argument_index], "-s")
		    || !strcmp(argv[argument_index], "-scanner")) {
			parameters->test_scanner          = true;
		} else if (!strcmp(argv[argument_index], "-c")
		    || !strcmp(argv[argument_index], "-classads")) {
			parameters->test_classads          = true;
		} else if (!strcmp(argv[argument_index], "-r")
		    || !strcmp(argv[argument_index], "-references")) {
			parameters->test_references        = true;
		} else if (!strcmp(argv[argument_index], "-C")
		    || !strcmp(argv[argument_index], "-copy")) {
			parameters->test_copy_constructor  = true;
			parameters->test_assignment        = false;
		} else if (!strcmp(argv[argument_index], "-A")
		    || !strcmp(argv[argument_index], "-assignment")) {
			parameters->test_assignment       = true;
			parameters->test_copy_constructor = false;
		} else if (!strcmp(argv[argument_index], "-x")
		    || !strcmp(argv[argument_index], "-xml")) {
			parameters->test_xml              = true;
		} else if (!strcmp(argv[argument_index], "-d")
		    || !strcmp(argv[argument_index], "-dirty")) {
			parameters->test_dirty            = true;
		} else if (!strcmp(argv[argument_index], "-f")
		    || !strcmp(argv[argument_index], "-functions")) {
			parameters->test_functions        = true;
		} else if (!strcmp(argv[argument_index], "-random")) {
			parameters->test_random           = true;
		} else {
			fprintf(stderr, "Unknown argument: %s\n", 
					argv[argument_index]);
			dump_usage = true;

		}
		argument_index++;
	}

	if (dump_usage) {
		fprintf(stderr, "Usage: test_classads [-v | -verbose] "
				        "[-h | -help| -?]\n");
		fprintf(stderr, "       [-v | -verbose]   Print oodles of info.\n");
		fprintf(stderr, "       [-h | -help | -?] Print this message.\n");
		exit(1);
	}

	if (   parameters->test_scanner == false
		&& parameters->test_copy_constructor == false
		&& parameters->test_assignment       == false
		&& parameters->test_classads         == false
		&& parameters->test_references       == false
		&& parameters->test_dirty            == false) {
		parameters->test_scanner          = true;
		parameters->test_copy_constructor = true;
		parameters->test_classads         = true;
		parameters->test_references       = true;
		parameters->test_dirty            = true;
	}
	return;
}

/***************************************************************
 *
 * Function: test_scanner
 * Purpose:  Test the classad scanner: that's the bit that
 *           walks through the input character by character and
 *           tokenizes it: it's a lexer.
 *
 ***************************************************************/
void 
test_scanner(
    TestResults *results) // OUT: Modified to reflect result of test
{
	Token token;
	char  *input = "Rank = ((Owner == \"Alain\") || (Memory < 500)) 5.4 '2001-10-10'";
	char  *current;

	current = input;

	Scanner(current, token);

	// Rank
	test_token_type(&token, LX_VARIABLE, __LINE__, results);
	test_token_text(&token, "Rank", __LINE__, results);
	Scanner(current, token);

	// =
	test_token_type(&token, LX_ASSIGN, __LINE__, results);
	Scanner(current, token);

	// ((
	test_token_type(&token, LX_LPAREN, __LINE__, results);
	Scanner(current, token);
	test_token_type(&token, LX_LPAREN, __LINE__, results);
	Scanner(current, token);

	// Owner
	test_token_type(&token, LX_VARIABLE, __LINE__, results);
	test_token_text(&token, "Owner", __LINE__, results);
	Scanner(current, token);

	// ==
	test_token_type(&token, LX_EQ, __LINE__, results);
	Scanner(current, token);

	// "Alain"
	test_token_type(&token, LX_STRING, __LINE__, results);
	test_token_text(&token, "Alain", __LINE__, results);
	Scanner(current, token);

	// )
	test_token_type(&token, LX_RPAREN, __LINE__, results);
	Scanner(current, token);

	// ||
	test_token_type(&token, LX_OR, __LINE__, results);
	Scanner(current, token);

	// (
	test_token_type(&token, LX_LPAREN, __LINE__, results);
	Scanner(current, token);

	// Memory
	test_token_type(&token, LX_VARIABLE, __LINE__, results);
	test_token_text(&token, "Memory", __LINE__, results);
	Scanner(current, token);

	// LT
	test_token_type(&token, LX_LT, __LINE__, results);
	Scanner(current, token);

	// 500 
	test_token_type(&token, LX_INTEGER, __LINE__, results);
	test_token_integer(&token, 500, __LINE__, results);
	Scanner(current, token);

	// ))
	test_token_type(&token, LX_RPAREN, __LINE__, results);
	Scanner(current, token);
	test_token_type(&token, LX_RPAREN, __LINE__, results);
	Scanner(current, token);

	test_token_type(&token, LX_FLOAT, __LINE__, results);
	test_token_float(&token, 5.4, __LINE__, results);
	Scanner(current, token);

	test_token_type(&token, LX_TIME, __LINE__, results);
	test_token_text(&token, "2001-10-10", __LINE__, results);
	Scanner(current, token);

	// end
	test_token_type(&token, LX_EOF, __LINE__, results);

	// Test some big strings.
	char *big_string, *quoted_big_string, *scanner_text;
	
	make_big_string(30000, &big_string, &quoted_big_string);
	scanner_text = quoted_big_string;
	Scanner(scanner_text, token);
	test_token_type(&token, LX_STRING, __LINE__, results);
	test_token_text(&token, big_string, __LINE__, results);

	scanner_text = big_string;
	Scanner(scanner_text, token);
	test_token_type(&token, LX_VARIABLE, __LINE__, results);
	test_token_text(&token, big_string, __LINE__, results);

	free(big_string);
	free(quoted_big_string);

	return;
}

/***************************************************************
 *
 * Function: test_token_type
 * Purpose:  Used by test_scanner(). Given a token, make sure
 *           it's the type we expect.
 *
 ***************************************************************/
void
test_token_type(
	const Token *token,        // IN: The token we're examining
    int         token_type,    // IN: The token type we expect
    int         line_number,   // IN: The line number to print
	TestResults *test_results) // OUT: Modified to reflect result of test
{
	if (token->type == token_type) {
		printf("Passed: token_type == %d in line %d\n",
			   token_type, line_number);
		test_results->AddResult(true);
	} else {
		printf("Failed: token_type == %d, not %d in line %d\n", 
			   token->type, token_type, line_number);
		test_results->AddResult(false);
	}
	return; 
}

/***************************************************************
 *
 * Function: test_token_text
 * Purpose:  Used by test_scanner(). Given a token, make sure it
 *           has the text we expect. 
 *
 ***************************************************************/
void
test_token_text(
	const Token *token,        // IN: The token we're examining
	char        *text,         // IN: The text we expect
    int         line_number,   // IN: The line number to print
	TestResults *test_results) // OUT: Modified to reflect result of test
{
	if (!strcmp(token->strVal, text)) {
		printf("Passed: token_text == \"");
		print_truncated_string(text, 40);
		printf("\" in line %d\n", line_number);
		test_results->AddResult(true);
	} else {
		printf("Failed: token_text == \"");
		print_truncated_string(token->strVal, 30);
		printf("\" not \"");
		print_truncated_string(text, 30);
		printf("\" in line %d\n", line_number);
		test_results->AddResult(false);
	}
	return; 
}

/***************************************************************
 *
 * Function: test_token_integer
 * Purpose:  Given a token, make sure its integer value is as
 *           we expect. Used by test_scanner().
 *
 ***************************************************************/
void
test_token_integer(
    const Token *token,        // IN: The token we're examining
    int         integer,       // IN: The integer we're expecting.
    int         line_number,   // IN: The line number to print
	TestResults *test_results) // OUT: Modified to reflect result of test
{
	if (token->intVal == integer) {
		printf("Passed: token value == %d in line %d\n",
			   integer, line_number);
		test_results->AddResult(true);
	} else {
		printf("Failed: token value == %d, not %d in line %d\n", 
			   token->intVal, integer, line_number);
		test_results->AddResult(false);
	}
	return; 
}

/***************************************************************
 *
 * Function: test_token_float
 * Purpose:  Given a token, make sure its floating-point value
 *           is as we expect. Used by test_scanner().
 *
 ***************************************************************/
void
test_token_float(
	const Token *token,         // IN: The token we're examining
	float       number,         // IN: The float we're expecting
    int         line_number,    // IN: The line number to print
	TestResults *test_results)  // OUT: Modified to reflect result of test
{
	if (token->floatVal == number) {
		printf("Passed: token value == %g in line %d\n",
			   number, line_number);
		test_results->AddResult(true);
	} else {
		printf("Failed: token value == %g, not %g in line %d\n", 
			   token->floatVal, number, line_number);
		test_results->AddResult(false);
	}
	return;
}

/***************************************************************
 *
 * Function: test_integer_value
 * Purpose:  Given a classad and an attribute within the classad,
 *           test that the attribute has the integer value we
 *           expect.
 *
 ***************************************************************/
void 
test_integer_value(
    ClassAd     *classad,        // IN: The ClassAd we're examining
	const char  *attribute_name, // IN: The attribute we're examining
	int         expected_value,  // IN: The integer we're expecting
	int         line_number,     // IN: The line number to print
    TestResults *results)        // OUT: Modified to reflect result of test
{
	int actual_value;
 	int found_integer;

	found_integer = classad->LookupInteger(attribute_name, actual_value);
	if (expected_value == actual_value) {
		printf("Passed: %s is %d in line %d\n",
			   attribute_name, expected_value, line_number);
		results->AddResult(true);
	} else if (!found_integer) {
		printf("Failed: Attribute \"%s\" is not found.\n", attribute_name);
		results->AddResult(false);
	} else {
		printf("Failed: %s is %d not %d in line %d\n",
			   attribute_name, actual_value, expected_value, line_number);
		results->AddResult(false);
	}
	return;
}

/***************************************************************
 *
 * Function: test_eval_bool
 * Purpose:  Given a classad and an attribute within the classad,
 *           test that the attribute evaluates to the boolean we
 *           expect
 *
 ***************************************************************/
void 
test_eval_bool(
    ClassAd     *classad,        // IN: The ClassAd we're examining
	const char  *attribute_name, // IN: The attribute we're examining
	int         expected_value,  // IN: The integer we're expecting
	int         line_number,     // IN: The line number to print
    TestResults *results)        // OUT: Modified to reflect result of test
{
	int actual_value;
 	int found_bool;

	found_bool = classad->EvalBool(attribute_name, NULL, actual_value);
	if (expected_value == actual_value) {
		printf("Passed: %s is %d in line %d\n",
			   attribute_name, expected_value, line_number);
		results->AddResult(true);
	} else if (!found_bool) {
		printf("Failed: Attribute \"%s\" is not found.\n", attribute_name);
		results->AddResult(false);
	} else {
		printf("Failed: %s is %d not %d in line %d\n",
			   attribute_name, actual_value, expected_value, line_number);
		results->AddResult(false);
	}
	return;
}

/***************************************************************
 *
 * Function: test_string_value
 * Purpose:  Given a classad, test that an attribute within the
 *           classad has the string value we expect.
 *
 ***************************************************************/
void 
test_string_value(
    ClassAd     *classad,        // IN: The ClassAd we're examining
	const char  *attribute_name, // IN: The attribute we're examining
	const char  *expected_value, // IN: The string we're expecting
	int         line_number,     // IN: The line number to print
    TestResults *results)        // OUT: Modified to reflect result of test
{
	int         found_string;
	char        *actual_value=NULL;

	found_string = classad->LookupString(attribute_name, &actual_value);
	if (found_string && !strcmp(expected_value, actual_value)) {
		printf("Passed: ");
		print_truncated_string(attribute_name, 40);
		printf(" is \"");
		print_truncated_string(expected_value, 40);
		printf(" \" in line %d\n", line_number);
		results->AddResult(true);
	} else if (!found_string) {
		printf("Failed: Attribute \"%s\" is not found.\n", attribute_name);
		results->AddResult(false);
	} else {
		printf("Failed: ");
		print_truncated_string(attribute_name, 40);
		printf(" is: ");
		print_truncated_string(actual_value, 30);
		printf("\" not \"");
		print_truncated_string(expected_value, 30);
		printf("\" in line %d\n", line_number);
		results->AddResult(false);
	}
    if (actual_value != NULL){
        free(actual_value);
    }
	return;
}

/***************************************************************
 *
 * Function: test_time_string_value
 * Purpose:  Given a classad, test that an attribute within the
 *           classad has the time string we expect.
 *
 ***************************************************************/
void 
test_time_string_value(
    ClassAd     *classad,        // IN: The ClassAd we're examining
	const char  *attribute_name, // IN: The attribute we're examining
	const char  *expected_value, // IN: The string we're expecting
	int         line_number,     // IN: The line number to print
    TestResults *results)        // OUT: Modified to reflect result of test
{
	int         found_string;
	char        *actual_value;

	found_string = classad->LookupTime(attribute_name, &actual_value);
	if (found_string && !strcmp(expected_value, actual_value)) {
		printf("Passed: ");
		print_truncated_string(attribute_name, 40);
		printf(" is \"");
		print_truncated_string(expected_value, 40);
		printf("\" in line %d\n", line_number);
		results->AddResult(true);
	} else if (!found_string) {
		printf("Failed: Attribute \"%s\" is not found.\n", attribute_name);
		results->AddResult(false);
	} else {
		printf("Failed: ");
		print_truncated_string(attribute_name, 40);
		printf(" is: ");
		print_truncated_string(actual_value, 30);
		printf("\" not \"");
		print_truncated_string(expected_value, 30);
		printf("\" in line %d\n", line_number);
		results->AddResult(false);
	}
	free(actual_value);
	return;
}

/***************************************************************
 *
 * Function: test_time_value
 * Purpose:  Given a classad, test that an attribute within the
 *           classad has the time we expect.
 *
 ***************************************************************/
void 
test_time_value(
    ClassAd          *classad,        // IN: The ClassAd we're examining
	const char       *attribute_name, // IN: The attribute we're examining
	const struct tm  &expected_time,  // IN: The string we're expecting
	int              line_number,     // IN: The line number to print
    TestResults      *results)        // OUT: Modified to reflect result of test
{
	int         found_time;
	struct tm   actual_time;
	bool        is_utc;

	found_time = classad->LookupTime(attribute_name, &actual_time, &is_utc);
	if (found_time 
		&& actual_time.tm_year == expected_time.tm_year
		&& actual_time.tm_mon  == expected_time.tm_mon
		&& actual_time.tm_mday == expected_time.tm_mday
		&& actual_time.tm_hour == expected_time.tm_hour
		&& actual_time.tm_min  == expected_time.tm_min
		&& actual_time.tm_sec  == expected_time.tm_sec) {
		printf("Passed: ");
		print_truncated_string(attribute_name, 40);
		printf(" has correct time in line %d\n", line_number);
		results->AddResult(true);
	} else if (!found_time) {
		printf("Failed: Attribute \"%s\" is not found.\n", attribute_name);
		results->AddResult(false);
	} else {
		char *t1, *t2;
		bool is_utc;
		t1 = time_to_iso8601(actual_time, ISO8601_ExtendedFormat, 
						ISO8601_DateAndTime, &is_utc);
		t2 = time_to_iso8601(expected_time, ISO8601_ExtendedFormat, 
						ISO8601_DateAndTime, &is_utc);
		printf("Failed: ");
		print_truncated_string(attribute_name, 40);
		printf(" is: ");
		print_truncated_string(t1, 30);
		printf("\" not \"");
		print_truncated_string(t2, 30);
		printf("\" in line %d\n", line_number);
		results->AddResult(false);
		free(t1);
		free(t2);
	}
	return;
}

/***************************************************************
 *
 * Function: test_mytype
 * Purpose:  Given a classad, make sure that the type (mytype)
 *           is what we expect it to be. 
 *
 ***************************************************************/
void 
test_mytype(
    ClassAd     *classad,        // IN: The ClassAd we're examining
	const char  *expected_value, // IN: The type we're expecting.
	int         line_number,     // IN: The line number to print
    TestResults *results)        // OUT: Modified to reflect result of test
{
	static const char *actual_value;

	actual_value = classad->GetMyTypeName();
	if (!strcmp(expected_value, actual_value)) {
		printf("Passed: MyType is \"");
		print_truncated_string(expected_value, 40);
		printf("\" in line %d\n", line_number);
		results->AddResult(true);
	} else {
		printf("Failed: MyType is \"");
		print_truncated_string(actual_value, 30);
		printf("\" not \"");
		print_truncated_string(expected_value, 30);
		printf("\" in line %d\n", line_number);
		results->AddResult(false);
	}
	return;
}

/***************************************************************
 *
 * Function: test_targettype
 * Purpose:  Given a classad, test that the target type is what
 *           we expect it to be.
 *
 ***************************************************************/
void 
test_targettype(
    ClassAd     *classad,        // IN: The ClassAd we're examining
	const char  *expected_value, // IN: The targettype we're expecting
	int         line_number,     // IN: The line number to print
    TestResults *results)        // OUT: Modified to reflect result of test
{
	static const char *actual_value;

	actual_value = classad->GetTargetTypeName();
	if (!strcmp(expected_value, actual_value)) {
		printf("Passed: TargetType is \"");
		print_truncated_string(expected_value, 40);
		printf("\" in line %d\n", line_number);
		results->AddResult(true);
	} else {
		printf("Failed: TargetType is \"");
		print_truncated_string(actual_value, 30);
		printf("\" not \"");
		print_truncated_string(expected_value, 30);
		printf("\" in line %d\n", line_number);
		results->AddResult(false);
	}
	return;
}

/***************************************************************
 *
 * Function: test_ads_match
 * Purpose:  Verify that two class ads match, as we expect.
 *
 ***************************************************************/
void
test_ads_match(
	ClassAd      *classad_1,   // IN: One of the class ads
	ClassAd      *classad_2,   // IN: The other the class ads
    int          line_number,  // IN: The line number to print
	TestResults  *results)     // OUT: Modified to reflect result of test
{
	if (classad_1->IsAMatch(classad_2)) {
		printf("Passed: classads match in line %d.\n", line_number);
		results->AddResult(true);
	} else {
		printf("Failed: classads don't match in line %d.\n", line_number);
		results->AddResult(false);
	}
	return;
}

/***************************************************************
 *
 * Function: test_ads_dont_match
 * Purpose:  Given two class ads, ensure that they don't match.
 *
 ***************************************************************/
void
test_ads_dont_match(
	ClassAd      *classad_1,   // IN: One of the class ads
	ClassAd      *classad_2,   // IN: The other the class ads
    int          line_number,  // IN: The line number to print
	TestResults  *results)     // OUT: Modified to reflect result of test
{
	if (!classad_1->IsAMatch(classad_2)) {
		printf("Passed: classads don't match in line %d.\n", line_number);
		results->AddResult(true);
	} else {
		printf("Failed: classads match in line %d.\n", line_number);
		results->AddResult(false);
	}
	return;
}

void 
test_printed_version(
    ClassAd     *classad,         // IN: The ClassAd we're examining
	const char  *attribute_name,  // IN: The attribute we're examining
	char        *expected_string, // IN: The string we're expecting
	int         line_number,      // IN: The line number to print
    TestResults *results)         // OUT: Modified to reflect result of test
{
	char      *printed_version;
	ExprTree  *tree;

	tree = classad->Lookup(attribute_name);
	tree->PrintToNewStr(&printed_version);

	if (!strcmp(expected_string, printed_version)) {
		printf("Passed: ");
		print_truncated_string(attribute_name, 40);
		printf(" prints correctly in line %d.\n", line_number);
		results->AddResult(true);
	} else {
		printf("Failed: ");
		print_truncated_string(attribute_name, 40);
		printf(" does not print correctly in line %d.\n", line_number);
		printf("Printed as: %s\n", printed_version);
		printf("Expected  : %s\n", expected_string);
		results->AddResult(false);
	}
	free(printed_version);
	return;
}

/***************************************************************
 *
 * Function: test_not_references
 * Purpose:  Test if the name (reference) is found in the
 *           list of refrences.
 *
 ***************************************************************/
void
test_in_references(
	char         *name,        // IN: What to look for 
	StringList   &references,   // IN: References to examine
    int          line_number,  // IN: The line number to print
	TestResults  *results)     // OUT: Modified to reflect result of test
{
	char *reference;
	bool is_in_references = false;

	references.rewind(); 
	while ((reference = references.next()) != NULL) {
		if (!strcmp(reference, name)) {
			is_in_references = true;
			break;
		}
	}
	if (is_in_references) {
		printf("Passed: %s is in references in line %d.\n", 
			   name, line_number);
		results->AddResult(true);
	}
	else {
		printf("Failed: %s is not in references in line %d.\n", 
			   name, line_number);
		results->AddResult(false);
	}
	return;
}
    
/***************************************************************
 *
 * Function: test_not_in_references
 * Purpose:  Test if the name (reference) is not found in the
 *           list of refrences.
 *
 ***************************************************************/
void
test_not_in_references(
	char         *name,        // IN: What to look for 
	StringList   &references,   // IN: References to examine
    int          line_number,  // IN: The line number to print
	TestResults  *results)     // OUT: Modified to reflect result of test
{
	char *reference;
	bool is_in_references = false;

	references.rewind(); 
	while ((reference = references.next()) != NULL) {
		if (!strcmp(reference, name)) {
			is_in_references = true;
			break;
		}
	}
	if (!is_in_references) {
		printf("Passed: %s is not in references in line %d.\n", 
			   name, line_number);
		results->AddResult(true);
	}
	else {
		printf("Failed: %s is in references in line %d.\n", 
			   name, line_number);
		results->AddResult(false);
	}
	return;
}

/***************************************************************
 *
 * Function: test_dirty_attribute
 * Purpose:  Test if dirty attributes (attributes modified after
 *           the creation of the ClassAd) are properly maintained.
 *
 ***************************************************************/
void 
test_dirty_attribute(
	TestResults  *results)     // OUT: Modified to reflect result of test
{
	ClassAd  *classad;
	char     *name;

	classad = new ClassAd("A = 1, B = 2", ',');

	// First of all, we should have zero dirty attributes. 
	classad->ResetName(); 
	name = classad->NextDirtyName();
	if (name) {
		delete [] name;
		printf("Failed: new ClassAd has dirty attributes in line %d\n",
			   __LINE__);
		results->AddResult(false);
	} else {
		printf("Passed: new ClassAd is clean in line %d\n", __LINE__);
		results->AddResult(true);
	}

	// Add an attribute
	classad->Insert("C = 3");

	// Now we should have exactly one dirty attribute, C.
	classad->ResetName();
	name = classad->NextDirtyName();
	if (!name) {
		printf("Failed: C isn't dirty in line %d\n", __LINE__);
		results->AddResult(false);
	} else if (strcmp(name, "C")) {
		printf("Failed: %s is dirty, not C in line %d\n", name, __LINE__);
		results->AddResult(false);
		delete [] name;
	} else {
		printf("Passed: C is dirty in line %d\n", __LINE__);
		results->AddResult(true);
		delete [] name;
	}
	name = classad->NextDirtyName();
	if (name) {
		printf("Failed: more than one dirty attribute in line %d\n", __LINE__);
		results->AddResult(false);
		delete [] name;
	} else {
		printf("Passed: not more than one dirty attribute in line %d\n", __LINE__);
		results->AddResult(true);
	}

	// Add an attribute
	classad->Insert("D = 4");

	// Now we should have two dirty attributes, C & D
	classad->ResetName();
	name = classad->NextDirtyName();
	if (!name) {
		printf("Failed: C isn't dirty in line %d\n", __LINE__);
		results->AddResult(false);
	} else if (strcmp(name, "C")) {
		printf("Failed: %s is dirty, not C in line %d\n", name, __LINE__);
		results->AddResult(false);
		delete [] name;
	} else {
		printf("Passed: C is dirty in line %d\n", __LINE__);
		results->AddResult(true);
		delete [] name;
	}
	name = classad->NextDirtyName();
	if (!name) {
		printf("Failed: D isn't dirty in line %d\n", __LINE__);
		results->AddResult(false);
	} else if (strcmp(name, "D")) {
		printf("Failed: %s is dirty, not D in line %d\n", name, __LINE__);
		results->AddResult(false);
		delete [] name;
	} else {
		printf("Passed: D is dirty in line %d\n", __LINE__);
		results->AddResult(true);
		delete [] name;
	}
	name = classad->NextDirtyName();
	if (name) {
		printf("Failed: more than two dirty attributes in line %d\n", __LINE__);
		results->AddResult(false);
		delete [] name;
	} else {
		printf("Passed: not more than two dirty attributes in line %d\n", __LINE__);
		results->AddResult(true);
	}

	// Clear the dirty flags, and there should be no dirty attributes
	classad->ClearAllDirtyFlags();
	classad->ResetName(); 
	name = classad->NextDirtyName();
	if (name) {
		delete [] name;
		printf("Failed: ClassAd has dirty attributes in line %d\n",
			   __LINE__);
		results->AddResult(false);
	} else {
		printf("Passed: new ClassAd is clean in line %d\n", __LINE__);
		results->AddResult(true);
	}

	// Now make A dirty, and B dirty then clean, and we should have 
	// just A dirty. 
	classad->SetDirtyFlag("A", true);
	classad->SetDirtyFlag("B", true);
	classad->SetDirtyFlag("B", false);

	classad->ResetName();
	name = classad->NextDirtyName();
	if (!name) {
		printf("Failed: A isn't dirty in line %d\n", __LINE__);
		results->AddResult(false);
	} else if (strcmp(name, "A")) {
		printf("Failed: %s is dirty, not A in line %d\n", name, __LINE__);
		results->AddResult(false);
		delete [] name;
	} else {
		printf("Passed: A is dirty in line %d\n", __LINE__);
		results->AddResult(true);
		delete [] name;
	}
	name = classad->NextDirtyName();
	if (name) {
		printf("Failed: more than one dirty attribute in line %d\n", __LINE__);
		results->AddResult(false);
		delete [] name;
	} else {
		printf("Passed: not more than one dirty attribute in line %d\n", __LINE__);
		results->AddResult(true);
	}

	// We should also be able to test it with GetDirtyFlag()
	bool exists, dirty;
	classad->GetDirtyFlag("A", &exists, &dirty);
	if (!exists) {
		printf("Failed: A doesn't exist in line %d\n", __LINE__);
		results->AddResult(false);
	} else if (!dirty) {
		printf("Failed: A isn't dirty  in line %d\n", __LINE__);
		results->AddResult(false);
	} else {
		printf("Passed: A is dirty in line %d\n", __LINE__);
		results->AddResult(true);
	}

	classad->GetDirtyFlag("B", &exists, &dirty);
	if (!exists) {
		printf("Failed: B doesn't exist in line %d\n", __LINE__);
		results->AddResult(false);
	} else if (dirty) {
		printf("Failed: B is dirty  in line %d\n", __LINE__);
		results->AddResult(false);
	} else {
		printf("Passed: B is not dirty in line %d\n", __LINE__);
		results->AddResult(true);
	}

	classad->GetDirtyFlag("Unknown", &exists, NULL);
	if (exists) {
		printf("Failed: 'Unknown' should not exist in line %d\n", __LINE__);
		results->AddResult(false);
	} else {
		printf("Passed: 'Unknown' doesn't exist in line %d\n", __LINE__);
	}


	delete classad;

	return;
}

#ifdef CLASSAD_FUNCTIONS
static void test_functions(
	TestResults  *results)     // OUT: Modified to reflect result of test
{
	char   big_string[1024];
	int    integer;
    float  real;
	char classad_string[] = "A=script(\"scriptfloat\"),"
                            "B=script(\"scriptinteger\"),"
	                        "C=script(\"scriptstring\"; \"|/dev/zero\"),"
	                        "D=GetTime(),"
		                    "E=sharedstring(),"
                            "G=sharedinteger(2),"
	                        "H=sharedfloat(3.14)";
	ClassAd  *classad;

	config(0);

	classad = new ClassAd(classad_string, ',');
	if (classad == NULL) {
		printf("Can't parse ClassAd for testing functions in line %d\n", 
			   __LINE__);
		results->AddResult(false);
	} else {
		printf("Parsed ClassAd for testing functions in line %d\n", 
			   __LINE__);
		results->AddResult(true);

		if (classad->EvalFloat("A", NULL, real)) {
			printf("Passed: Evaluting scriptfloat gives: %f in line %d\n", 
				   real, __LINE__);
			results->AddResult(true);
		} else {
			printf("Failed to evaluate scriptfloat in line %d\n",
				   __LINE__);
			results->AddResult(false);
		}

		if (classad->EvalInteger("B", NULL, integer)) {
			printf("Passed: Evaluting scriptinteger gives: %d in line %d\n", 
				   integer, __LINE__);
			results->AddResult(true);
		} else {
			printf("Failed: Evaluating scriptinteger in line %d\n",
				   __LINE__);
			results->AddResult(false);
		}

		if (classad->EvalString("C", NULL, big_string)) {
			printf("Passed: Evaluting scriptstring gives: '%s', in line %d\n", 
				   big_string, __LINE__);
			results->AddResult(true);
		} else {
			printf("Failed: Evaluating scriptstring in line %d\n",
				   __LINE__);
			results->AddResult(false);
		} 

		if (classad->EvalInteger("D", NULL, integer)) {
			printf("Passed: Evaluting gettime function gives: %d in line %d\n", 
				   integer, __LINE__);
			results->AddResult(true);
		} else {
			printf("Failed: Evaluating gettime function in line %d\n",
				   __LINE__);
			results->AddResult(false);
		}

		if (classad->EvalString("E", NULL, big_string)) {
			printf("Passed: Evaluating sharedstring function gives: '%s' in line %d\n", 
				   big_string, __LINE__);
			results->AddResult(true);
		} else {
			printf("Failed: Evaluating foo sharedstring in line %d\n",
				   __LINE__);
			results->AddResult(false);
		}

		if (classad->EvalInteger("G", NULL, integer)) {
			printf("Passed: Evaluting sharedinteger gives: %d in line %d\n", 
				   integer, __LINE__);
			results->AddResult(true);
		} else {
			printf("Failed: Evaluating sharedinteger in line %d\n",
				   __LINE__);
			results->AddResult(false);
		}

		if (classad->EvalFloat("H", NULL, real)) {
			printf("Passed: Evaluting sharedfloat gives: %f in line %d\n", 
				   real, __LINE__);
			results->AddResult(true);
		} else {
			printf("Failed: Evaluating sharedfloat in line %d\n",
				   __LINE__);
			results->AddResult(false);
		}
        delete classad;
	}
	return;
}
#endif

/***************************************************************
 *
 * Function: print_truncated_string
 * Purpose:  Make sure that a string prints out with no more than
 *           the given number of characters. If the string is 
 *           too long, we put an ellipsis at the end. 
 *
 ***************************************************************/
void 
print_truncated_string(
    const char *s,        // IN: The string to print
	int max_characters)   // IN: The maximum number of characters to print
{
	int length;

	if (max_characters < 1) {
		max_characters = 1;
	}

	length = strlen(s);
	if (length > max_characters) {
		if (max_characters > 3) {
			printf("%.*s...", max_characters - 3, s);
		}
		else {
			printf("%.*s", max_characters, s); 
		}
	}
	else {
		printf("%s", s);
	}
	return;
}

/***************************************************************
 *
 * Function: make_big_string
 * Purpose:  create a string of a given length, and fill it with 
 *           random stuff. If quoted_string is not NULL, we make it
 *           a copy of the big string, except with quotes around it.
 *
 ***************************************************************/
static void 
make_big_string(
    int length,           // IN: The desired length
	char **string,        // OUT: the big random string
	char **quoted_string) // OUT: the string in quotes
{
	*string = (char *) malloc(length + 1);

	for (int i = 0; i < length; i++) {
		(*string)[i] = (rand() % 26) + 97; 
	}
	(*string)[length] = 0;

	if (quoted_string != NULL) {
		*quoted_string = (char *) malloc(length + 3);
		sprintf(*quoted_string, "\"%s\"", *string);
	}
	return;
}

/***************************************************************
 *
 * Function: test_random
 * Purpose:  Test the random() function in ClassAds. Our testing
 *           is a bit wonky: since we are getting random numbers,
 *           we can't be sure our tests will work, but chances
 *           are good. :)
 *
 ***************************************************************/
void test_random(
    TestResults *results)
{
    char *classad_string = "R1 = random(), R2 = random(10)";
    int  base_r1, r1;
    int  r2;
    bool have_different_numbers;
    bool numbers_in_range;

    ClassAd *classad;

    classad = new ClassAd(classad_string, ',');

    // First we check that random gives us different numbers
    have_different_numbers = false;
    classad->EvalInteger("R1", NULL, base_r1);
    for (int i = 0; i < 10; i++) {
        classad->EvalInteger("R1", NULL, r1);
        if (r1 != base_r1) {
            have_different_numbers = true;
            break;
        }
    }

    // Then we check that random gives numbers in the correct range
    numbers_in_range = true;
    for (int i = 0; i < 10; i++) {
        classad->EvalInteger("R2", NULL, r2);
        if (r2 < 0 || r2 >= 10) {
            numbers_in_range = false;
            break;
        }
    }

    if (have_different_numbers) {
        printf("Passed: Random generates a variety of numbers in line %d\n", 
               __LINE__);
        results->AddResult(true);
    } else {
        printf("Failed: Random does not generate a variety of numbers in line %d\n", 
               __LINE__);
        results->AddResult(false);
    }

    if (numbers_in_range) {
        printf("Passed: Random generates numbers in correct range in %d\n", 
               __LINE__);
        results->AddResult(true);
    } else {
        printf("Passed: Random does not generate numbers in correct range in %d\n", 
               __LINE__);
        results->AddResult(false);
    }
    delete classad;
    return;
}

void test_equality(TestResults *results)
{
    ExprTree *e1, *e2, *e3;
    const char *s1 = "Foo = 3";
    const char *s3 = "Bar = 5";

    Parse(s1, e1);
    Parse(s1, e2);
    Parse(s3, e3);

    if ((*e1) == (*e2)) {
        printf("Passed: operator== detects equality in line %d\n", __LINE__);
        results->AddResult(true);
    } else {
        printf("Failed: operator== does not detect equality in line %d\n", __LINE__);
        results->AddResult(false);
    }

    if ((*e1) == (*e3)) {
        printf("Failed: operator== does not detect inequality in line %d\n", __LINE__);
        results->AddResult(false);
    } else {
        printf("Passed: operator== detects inequality in line %d\n", __LINE__);
        results->AddResult(true);
    }
    return;
}
