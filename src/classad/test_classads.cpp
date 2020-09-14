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
 * test_classads.C is now deprecated and unused. 
 * It has been replaced by two other test programs:
 *    classad_functional_tester
 *    classad_unit_tester
 * For now, I'm leaving this program accessible in CVS, but it will
 * not be compiled by default, and it will no longer be distributed
 * as part of the ClassAd distribution. 
 *
 *--------------------------------------------------------------------*/

#include "classad/classad_distribution.h"
#include "classad/lexerSource.h"
#include "classad/xmlSink.h"
#include <fstream>
#include <iostream>
#include <ctype.h>
#include <assert.h>

using namespace std;
using namespace classad;

/*--------------------------------------------------------------------
 *
 * Private Data Types
 *
 *--------------------------------------------------------------------*/

class Parameters
{
public: 
	bool    debug;
	string  input_file;
	
	void ParseCommandLine(int argc, char **argv);
};

class ErrorCount
{
private:
	int number_of_errors;
public: 
	ErrorCount() 
	{
		number_of_errors = 0;
	}
	void IncrementErrors(void) 
	{ 
		number_of_errors++; 
		return;
	}
	void PrintErrors(void) const 
	{
		if (number_of_errors == 0) {
			cout << "No errors were found.\n";
		} else if (number_of_errors == 1) {
			cout << "One error was found.\n";
		} else {
			cout << number_of_errors << " errors were found.\n";
		}
		return;
	}
};

typedef map<string, ClassAd *> ClassAdMap;
typedef map<string, ClassAd *>::iterator ClassAdMapIterator;
typedef map<string, ClassAdCollection *> CollectionMap;
typedef map<string, ClassAdCollection *>::iterator CollectionMapIterator;

/*--------------------------------------------------------------------
 *
 * Private Functions
 *
 *--------------------------------------------------------------------*/
static void cleanup(void);
static void mark_classad_in_collection(string name);
static void unmark_classad_in_collection(string name);
static bool is_classad_in_collection(string name);
static void process_file(
    ifstream &input_file, const Parameters &parameters,
    ErrorCount *errors);
static void process_classad(
	ifstream &input_file, const string &line,
    int token_start, int *line_number, 
	const Parameters &parameters, ErrorCount *errors);
static void process_print_classad(
	const string &line, int token_start, int line_number, 
    const Parameters &parameters, ErrorCount *errors);
static void process_print_classad_xml(
	const string &line, int token_start, int line_number, 
    const Parameters &parameters, ErrorCount *errors);
static void process_test_match(
	const string &line, int token_start, int line_number, 
    const Parameters &parameters, ErrorCount *errors);
static void process_test_lexer_one_token(
	const string &line, int token_start, int line_number, 
	const Parameters &parameters, ErrorCount *errors);
static void process_evaluate(
    const string &line, int token_start, int line_number, 
    const Parameters &parameters, ErrorCount *errors);
static void process_make_collection(
    const string &line, int token_start, int line_number, 
	const Parameters &parameters, ErrorCount *errors);
static void process_add_to_collection(
    const string &line,	int token_start, int line_number, 
	const Parameters &parameters, ErrorCount *errors);
static void process_truncate_log(
	const string &line,	int token_start, int line_number, 
	const Parameters &parameters, ErrorCount *errors);
static void process_create_subview(
    const string &line,	int token_start, int line_number, 
	const Parameters &parameters, ErrorCount *errors);
static void process_check_in_view(
	const string &line, int token_start, int line_number, 
	const Parameters &parameters, ErrorCount *errors);
static string extract_token(int *token_start, const string &line);
static bool empty_line(const string &line);

/*--------------------------------------------------------------------
 *
 * Global Variables
 *
 *--------------------------------------------------------------------*/
static ClassAdMap classads;
static CollectionMap collections;

/*********************************************************************
 *
 * Function: main
 * Purpose:  Overall control
 *
 *********************************************************************/
int 
main(int argc, char **argv)
{
	ErrorCount  errors;
	Parameters  parameters;

	parameters.ParseCommandLine(argc, argv);

	ifstream input_file(parameters.input_file.c_str());
	if (!input_file) {
		cout << "Can't open file: \""
			 << parameters.input_file
			 << "\"\n";
	}
	else {
#if (__GNUC__<3)
		input_file.flags(0);
#else
		input_file.flags((_Ios_Fmtflags)0);
#endif
		process_file(input_file, parameters, &errors);
		cleanup();
		errors.PrintErrors();
	}

	return 0;
}

static void
cleanup(void)
{
	// We want to only delete a ClassAd once. So we remove 
	// each classad in each collection, then delete all of the ClassAds. 
	CollectionMapIterator ci;

	for (ci = collections.begin(); ci != collections.end(); ci++ ) {
		ClassAdCollection *collection;

		collection = ci->second;

		LocalCollectionQuery  query;
		
		query.Bind(collection);
		assert(query.Query("root", NULL));
		
		string classad_key;
		
		for (query.ToFirst(), query.Current(classad_key); 
			 !query.IsAfterLast(); query.Next(classad_key)) {
			collection->RemoveClassAd(classad_key);
			break;
		}
		collections.erase(ci);
		delete collection;
	}

	ClassAdMapIterator i;
	for (i = classads.begin(); i != classads.end(); i++) {
		ClassAd *classad;

		
		classad = i->second;
		classads.erase(i);
		if (!is_classad_in_collection(i->first)) {
			delete classad;
		}
	}
	return;

	
}

map<string, bool> in_collection;

static void mark_classad_in_collection(string name)
{
	in_collection[name] = true;
	return;
}

static void unmark_classad_in_collection(string name)
{
	in_collection[name] = false;	
}

static bool is_classad_in_collection(string name)
{
	return in_collection[name];
}

/*********************************************************************
 *
 * Function: process_file
 * Purpose:  Walk through a file of tests and do each test. 
 *           The file has already been opened, and we do not 
 *           close it--the caller has to do that. 
 *
 *********************************************************************/
static void
process_file(ifstream &input_file, const Parameters &parameters,
			 ErrorCount *errors)
{
	int    line_number;
	string line;

	line_number = 0;
	while (1) {
		int token_start;

		getline(input_file, line, '\n');
		line_number++;
		
		token_start = 0;
		string first_token = extract_token(&token_start, line);
		
		if (input_file.eof()) {
			break;
		} else if (first_token[0] == '#') {
			// # indicates a comment.
			continue;
		} else if (!first_token.compare("begin-classad")) {
			process_classad(input_file, line, token_start, 
							&line_number, parameters, errors);
		} else if (!first_token.compare("print-classad")) {
			process_print_classad(line, token_start, line_number, parameters,
							   errors);
		} else if (!first_token.compare("print-classad-xml")) {
			process_print_classad_xml(line, token_start, line_number, parameters,
                                      errors);
		} else if (!first_token.compare("test-match")) {
			process_test_match(line, token_start, line_number, parameters,
							   errors);
		} else if (!first_token.compare("test-lexer-one-token")) {
			process_test_lexer_one_token(line, token_start, line_number,
										 parameters, errors);
		} else if (!first_token.compare("evaluate")) {
			process_evaluate(line, token_start, line_number, parameters,
							 errors);
		} else if (!first_token.compare("make-collection")) {
			process_make_collection(line, token_start, line_number, parameters,
							 errors);
		} else if (!first_token.compare("add-to-collection")) {
			process_add_to_collection(line, token_start, line_number, parameters,
							 errors);
		} else if (!first_token.compare("truncate-log")) {
			process_truncate_log(line, token_start, line_number, parameters,
								 errors);
		} else if (!first_token.compare("create-subview")) {
			process_create_subview(line, token_start, line_number, parameters,
							 errors);
		} else if (!first_token.compare("check-in-view")) {
			process_check_in_view(line, token_start, line_number, parameters,
							 errors);
		} else if (!empty_line(line)){
			cout << "Error: Unknown input (" << first_token
				 << ") on line " << line_number << "." << endl;
			errors->IncrementErrors();
		}
	}
	return;
}

/*********************************************************************
 *
 * Function: process-classad
 * Purpose:  A line began with "begin-classad". We'll get the name of
 *           the classad from the same line, then read in through 
 *           "end-classad". We'll parse the ClassAd, then stick it into
 *           a data structure (STL map) to store all of the classads.
 *
 *********************************************************************/
static void process_classad(ifstream &input_file, const string &line,
							int token_start, int *line_number, 
							const Parameters &parameters,
							ErrorCount *errors)
{
	int    starting_line_number;
	string name, extra, classad_line, classad_string;

	starting_line_number = *line_number;

	name = extract_token(&token_start, line);
	extra = extract_token(&token_start, line);
	if (extra.compare("")) {
		cout << "Ignored extra input (" << extra << ") on line "
			 << *line_number << "." << endl;
	}

	bool classad_done = false;

	while (!classad_done) {
		getline(input_file, classad_line, '\n');
		if (input_file.eof()) {
			cout << "Error: Unfinished classad on line " 
				 << *line_number << endl;
			classad_done = true;
			errors->IncrementErrors();
		}
		else {
			(*line_number)++;
			token_start = 0;
			string token = extract_token(&token_start, classad_line);
			if (token.compare("end-classad")) {
				classad_string += classad_line;
                // If we don't add newlines, then comments will
                // be misunderstood.
                classad_string += '\n'; 
			}
			else {
				classad_done = true;
			}
		}
	}

	// We've now read a name for a classad and the classad string.
	// Let's parse the ClassAd and store it away for future reference.
	ClassAdParser	parser;
	ClassAd         *classad;

	classad = parser.ParseClassAd(classad_string, true);

	if (classad == NULL) {
		cout << "Error: Bad classad, lines " << starting_line_number 
			 << " to " << *line_number << endl;
		errors->IncrementErrors();
	}
	else {
		cout << "OK: Read Classad \"" << name 
			 << "\", Line " << starting_line_number << endl;
	}
	classads[name] = classad;
	unmark_classad_in_collection(name);

	return;
}

/*********************************************************************
 *
 * Function: process_print_classad
 * Purpose:  Print a ClassAd
 *           The line in the file looks like:
 *             print-classad ClassAd-1
 *
 *********************************************************************/
static void process_print_classad(const string &line,
								  int token_start, int line_number, 
								  const Parameters &parameters,
								  ErrorCount *errors)
{
	string classad_name, extra;

	classad_name = extract_token(&token_start, line);
	extra = extract_token(&token_start, line);
	if (!classad_name.compare("")) {
		cout << "Error: Missing ClassAd name on line " << line_number 
			 << "." << endl;
		cout << "       Format: print-classad <classad>" << endl;
		errors->IncrementErrors();
	}
	else if (extra.compare("")) {
		cout << "Ignored extra input (" << extra << ") on line "
			 << line_number << "." << endl;
	}
	
	ClassAd *classad;

	classad = classads[classad_name];

	if (classad == NULL) {
		cout << "Error: Unknown ClassAd: \"" << classad_name
			 << "\" on line " << line_number << "." << endl;
		errors->IncrementErrors();
	} else {
		PrettyPrint  printer;
		string       text;

		printer.Unparse(text, classad);
		cout << "ClassAd " << classad_name << ":\n";
		cout << text;
		cout << endl;
	}
	return;
}

/*********************************************************************
 *
 * Function: process_print_classad_xml
 * Purpose:  Print a ClassAd in XML
 *           The line in the file looks like:
 *             print-classad-xml ClassAd-1
 *
 *********************************************************************/
static void process_print_classad_xml(const string &line,
							  	      int token_start, int line_number, 
								      const Parameters &parameters,
								      ErrorCount *errors)
{
	string classad_name, extra;

	classad_name = extract_token(&token_start, line);
	extra = extract_token(&token_start, line);
	if (!classad_name.compare("")) {
		cout << "Error: Missing ClassAd name on line " << line_number 
			 << "." << endl;
		cout << "       Format: print-classad <classad>" << endl;
		errors->IncrementErrors();
	}
	else if (extra.compare("")) {
		cout << "Ignored extra input (" << extra << ") on line "
			 << line_number << "." << endl;
	}
	
	ClassAd *classad;

	classad = classads[classad_name];

	if (classad == NULL) {
		cout << "Error: Unknown ClassAd: \"" << classad_name
			 << "\" on line " << line_number << "." << endl;
		errors->IncrementErrors();
	} else {
		ClassAdXMLUnParser  printer;
		string              text;

		printer.Unparse(text, classad);
		cout << "ClassAd " << classad_name << ":\n";
		cout << text;
		cout << endl;
	}
	return;
}

/*********************************************************************
 *
 * Function: process_test_match
 * Purpose:  Given a line that begins with test-match, process
 *           the rest of the line and check if the two given classads
 *           match. (We're expecting that they do match.)
 *           The line in the file looks like:
 *             test--match match-type Classad-1 Classad-1
 *           match-type is something like symmetricMatch, and the other
 *           match types that ClassAds allow.
 *
 *********************************************************************/
static void process_test_match(const string &line,
							   int token_start, int line_number, 
							   const Parameters &parameters,
							   ErrorCount *errors)
{
	string match_type, left_classad_name, right_classad_name;
	string expect_match, extra;

	match_type = extract_token(&token_start, line);
	left_classad_name = extract_token(&token_start, line);
	right_classad_name = extract_token(&token_start, line);
	expect_match = extract_token(&token_start, line);
	extra = extract_token(&token_start, line);
	if (!match_type.compare("") || !left_classad_name.compare("")
		|| !right_classad_name.compare("") || !expect_match.compare("")) {
		cout << "Error: Missing match information on line " << line_number 
			 << "." << endl;
		cout << "       Format: test-match <type> <classad1> <classad2> " 
			 << "[ExpectMatch | ExpectDontMatch] " << endl;
		errors->IncrementErrors();
	}
	else if (extra.compare("")) {
		cout << "Ignored extra input (" << extra << ") on line "
			 << line_number << "." << endl;
	}
	else if (expect_match.compare("ExpectMatch") 
			 && expect_match.compare("ExpectDontMatch")) {
		cout << "Error: Need to specify \"ExpectMatch\" or "
			 << "\"ExpectDontMatch\" on line "
			 << line_number << ".\n";
		errors->IncrementErrors();
		expect_match = "ExpectMatch";
	}

	
	ClassAd *left_classad, *right_classad;

	left_classad = classads[left_classad_name];
	right_classad = classads[right_classad_name];

	if (left_classad == NULL) {
		cout << "Error: Unknown ClassAd: \"" << left_classad_name
			 << "\" on line " << line_number << "." << endl;
		errors->IncrementErrors();
	}
	if (right_classad == NULL) {
		cout << "Error: Unknown ClassAd: \"" << right_classad_name 
			 << "\" on line " << line_number << "." << endl;
		errors->IncrementErrors();
	}
	if (left_classad != NULL && right_classad != NULL) {
		MatchClassAd    match_ad(left_classad, right_classad);

		bool does_match;
		bool evaluate_succeeded;
		evaluate_succeeded = match_ad.EvaluateAttrBool(match_type, does_match);
		if (!evaluate_succeeded ) {
			cout << "Error: Couldn't match, bad requirements? Line "
				 << line_number << "." << endl;
			errors->IncrementErrors();
		} else if (!expect_match.compare("ExpectMatch")) {
			if (!does_match){
				cout << "Error: ClassAds don't match: " 
					 << "on line " << line_number << "." << endl;
				errors->IncrementErrors();
			} else {
				cout << "OK: ClassAds match on line " << line_number
					 << "." << endl;
			}
		}
		else {
			if (!does_match){
				cout << "OK: ClassAds don't match on line " << line_number
					 << "." << endl;
			} else {
				cout << "Error: ClassAds match: " 
					 << "on line " << line_number << "." << endl;
				errors->IncrementErrors();
			}
		}
		match_ad.RemoveRightAd();
		match_ad.RemoveLeftAd();
	}
	return;
}

/*********************************************************************
 *
 * Function: process_test_lexer_one_token
 * Purpose:  
 *
 *********************************************************************/
static void process_test_lexer_one_token(const string &line,
										 int token_start, int line_number, 
										 const Parameters &parameters,
										 ErrorCount *errors)
{
	string token_string, expected_token_type;
	string extra;

	token_string          = extract_token(&token_start, line);
	expected_token_type   = extract_token(&token_start, line);
	extra                 = extract_token(&token_start, line);
	if (!token_string.compare("") 
		|| !expected_token_type.compare("")) {
		cout << "Error: Missing match information on line " << line_number 
			 << "." << endl;
		cout << "       Format: test-lexer-one-token <token> <token-type> "
			 << endl;
		errors->IncrementErrors();
	}
	else if (extra.compare("")) {
		cout << "Ignored extra input (" << extra << ") on line "
			 << line_number << "." << endl;
	}

	Lexer             lexer;
	Lexer::TokenType  token_type;
	const char        *token_name;
	StringLexerSource   *lexer_source;

	lexer_source = new StringLexerSource(&token_string);
	if (lexer_source == NULL) {
		cout << "Error: Couldn't allocate LexerSource! on line ";
		cout << line_number << "." << endl;
	} else {
		lexer.Initialize(lexer_source);
		token_type = lexer.PeekToken(NULL);
		token_name = lexer.strLexToken(token_type);
		lexer.FinishedParse();
		if (!strcmp(expected_token_type.c_str(), token_name)) {
			cout << "OK: Token type for \"" << token_string << "\" is "
				 << token_name << " on line " << line_number
				 << "." << endl;
		}
		else {
			cout << "Error: Token type for \"" << token_string << "\" is not "
				 << expected_token_type << " but is \"" << token_name 
				 << "\" on line " << line_number << "." << endl;
			errors->IncrementErrors();
		}
		delete lexer_source;
	}
	
	return;
}

/*********************************************************************
 *
 * Function: process_evaluate
 * Purpose:  Given a line that begins with "evaluate", parse the rest
 *           of the line and perform the evaluation.
 *
 *********************************************************************/
static void process_evaluate(const string &line,
							 int token_start, int line_number, 
							 const Parameters &parameters,
							 ErrorCount *errors)
{
	string classad_name, expression_string;
	ExprTree       *expr;
	ClassAdParser  parser;

	classad_name = extract_token(&token_start, line);
	expr = NULL;
	if ((unsigned) token_start < line.size()) {
		expression_string = line.substr(token_start, line.size() - token_start);
	} else {
		expression_string = "";
	}
	if (!classad_name.compare("") 
		|| !expression_string.compare("")) {
		cout << "Error: Missing evaluate information on line " << line_number 
			 << "." << endl;
		cout << "       Format: evaluate <classad> <expression>"
			 << endl;
		errors->IncrementErrors();
	}
	else {
		if (!parser.ParseExpression(expression_string, expr)) {
			cout << "Error: Can't parse expression (" << expression_string 
				 << ") on line " << line_number << "." << endl;
			errors->IncrementErrors();
		}
		else {
			ClassAd *classad = classads[classad_name];
			
			if (classad == NULL) {
				cout << "Error: Unknown ClassAd: \"" << classad_name 
					 << "\" on line " << line_number << "." << endl;
				errors->IncrementErrors();
			}
			else {
				Value  value;
				expr->SetParentScope(classad);
				if (!classad->EvaluateExpr(expr, value)) {
					cout << "Error: Can't evaluate expression (" << expression_string 
						 << ") on line " <<line_number << "." << endl;
					errors->IncrementErrors();
				}
				else {
					PrettyPrint unparser;
					string value_string;

					unparser.Unparse(value_string, value);
					cout << "OK: Evaluating \"" << expression_string
						 << "\" in " << classad_name 
						 << " evaluates to " << value_string
						 << " on line " << line_number << endl;
				}
			}
		}
	}
	if (expr != NULL) {
		delete expr;
	}
	return;
}

/*********************************************************************
 *
 * Function: process_make_collection
 * Purpose:  
 *
 *********************************************************************/
static void process_make_collection(const string &line,
							 int token_start, int line_number, 
							 const Parameters &parameters,
							 ErrorCount *errors)
{
	string collection_name;
	string log_name;
	string extra_text;

	collection_name = extract_token(&token_start, line);
	log_name = extract_token(&token_start, line);
	extra_text = extract_token(&token_start, line);
	if (!collection_name.compare("")) {
		cout << "Error: Missing make-collection information on line " << line_number 
			 << "." << endl;
		cout << "       Format: make-collection <collection-name> [log-name]" << endl;
		errors->IncrementErrors();
	} else if (extra_text.compare("")) {
		cout << "Error: Extra make-collection information on line " << line_number 
			 << "." << endl;
		cout << "       Format: make-collection <collection-name> [log-name]" << endl;
		errors->IncrementErrors();
	} else {
		ClassAdCollection *collection;

		collection = new ClassAdCollection();
		if (collection == NULL ) {
			cout << "Error: Failed to construct collection on line " << line_number 
				 << "." << endl;
			errors->IncrementErrors();
		} else {
			collections[collection_name] = collection;
			cout << "OK: Made collection named " << collection_name 
				 << " on line " << line_number << ".\n";
			if (log_name.compare("")) {
				bool success;
				success = collection->InitializeFromLog(log_name, "", "");
				if (success) {
					cout << "OK: Initialized " << collection_name << " from " 
						 << log_name << " on line " << line_number << ".\n";
				} else {
					cout << "Error: Couldn't initialize " << collection_name 
						 << " from "  << log_name << " on line " 
						 << line_number << ".\n";
					errors->IncrementErrors();
				}
			}
		}
	}

	return;
}

/*********************************************************************
 *
 * Function: process_add_to_collection
 * Purpose:  
 *
 *********************************************************************/
static void process_add_to_collection(const string &line,
							 int token_start, int line_number, 
							 const Parameters &parameters,
							 ErrorCount *errors)
{
	string collection_name, classad_name;
	string extra_text;

	collection_name = extract_token(&token_start, line);
	classad_name = extract_token(&token_start, line);
	extra_text = extract_token(&token_start, line);
	if (!collection_name.compare("") || !collection_name.compare("")) {
		cout << "Error: Missing add-to-collection information on line " << line_number 
			 << "." << endl;
		cout << "       Format: add-to-collection <collection-name> <classad-name>" << endl;
		errors->IncrementErrors();
	}
	else if (extra_text.compare("")) {
		cout << "Error: Extra make-collection information on line " << line_number 
			 << "." << endl;
		cout << "       Format: add-to-collection <collection-name> <classad-name>" << endl;
		errors->IncrementErrors();
	}
	else {
		ClassAdCollection *collection;
		ClassAd           *classad;

		collection = collections[collection_name];
		classad = classads[classad_name];
		if (collection == NULL ) {
			cout << "Error: Unknown collection (" << collection_name 
				 << ") on line " << line_number  << "." << endl;
			errors->IncrementErrors();
		} else if (classad == NULL ) {
			cout << "Error: Unknown classad (" << classad_name 
				 << ") on line " << line_number  << "." << endl;
			errors->IncrementErrors();
		}
		else {
			bool success;
			success = collection->AddClassAd(classad_name, classad);
			if (!success) {
				cout << "Error: couldn't add " << classad_name << " to "
					 << collection_name << " on line " << line_number << ".\n";
				errors->IncrementErrors();
			}
			else {
				ClassAd *retrieved_ad;
				retrieved_ad = collection->GetClassAd(classad_name);
				if (retrieved_ad != classad) {
					cout << "Error: added " << classad_name << " to "
						 << collection_name << " but it's not there, on line " 
						 << line_number << ".\n";
					errors->IncrementErrors();
				}
				else {
					cout << "OK: added " << classad_name << " to "
						 << collection_name << ", on line " 
						 << line_number << ".\n";
					mark_classad_in_collection(classad_name);
				}
			}
		}
	}

	return;
}

/*********************************************************************
 *
 * Function: process_truncate_log
 * Purpose:  
 *
 *********************************************************************/
static void process_truncate_log(
	const string &line,
	int token_start, int line_number, 
	const Parameters &parameters,
	ErrorCount *errors)
{
	string collection_name;

	collection_name = extract_token(&token_start, line);
	if (!collection_name.compare("") || !collection_name.compare("")) {
		cout << "Error: Missing add-to-collection information on line " << line_number 
			 << "." << endl;
		cout << "       Format: add-to-collection <collection-name> <classad-name>" << endl;
		errors->IncrementErrors();
	}
	else {
		ClassAdCollection *collection;

		collection = collections[collection_name];
		if (collection == NULL ) {
			cout << "Error: Unknown collection (" << collection_name 
				 << ") on line " << line_number  << "." << endl;
			errors->IncrementErrors();
		}
		else {
			bool success;
			success = collection->TruncateLog();
			if (!success) {
				cout << "Error: couldn't truncate log for " << collection_name 
					 << " on line " << line_number << ".\n";
				cout << "  (Error is: " << CondorErrMsg << ")\n";
				errors->IncrementErrors();
			} else {
				cout << "OK: Truncated log for " << collection_name
					 << " on line " << line_number << ".\n";
			}
		}
	}

	return;
}

/*********************************************************************
 *
 * Function: process_create_subview
 * Purpose:  
 *
 *********************************************************************/
static void process_create_subview(const string &line,
							 int token_start, int line_number, 
							 const Parameters &parameters,
							 ErrorCount *errors)
{
	string collection_name, parent_view_name, view_name, constraint;

	collection_name = extract_token(&token_start, line);
	parent_view_name = extract_token(&token_start, line);
	view_name = extract_token(&token_start, line);
	if ((unsigned) token_start < line.size()) {
		constraint = line.substr(token_start, line.size() - token_start);
	} else {
		constraint = "";
	}
	if (!collection_name.compare("") || !parent_view_name.compare("")
		|| !view_name.compare("") || !constraint.compare("")) {
		cout << "Error: Missing subview information on line " << line_number 
			 << "." << endl;
		cout << "       Format: create-subview collection parent-view view constraint"
			 << endl;
		errors->IncrementErrors();
	}
	else {
		ClassAdCollection  *collection;

		collection = collections[collection_name];
		if (collection == NULL) {
			cout << "Error: Unknown collection (" << collection_name 
				 << ") on line " << line_number << ".\n";
			errors->IncrementErrors();
		} else {
			bool success;

			success = collection->CreateSubView(view_name, parent_view_name,
												constraint, "", "");
			if (success) {
				cout << "OK: Made subview " << view_name << " in " 
					 << collection_name << " with constraint = " << constraint 
					 << " on line " << line_number
					 << ".\n";
			}
			else {
				cout << "Error: Failed to make subview " << view_name << " in " 
					 << collection_name << " on line " << line_number
					 << ".\n";
				errors->IncrementErrors();
				cout << "CondorErrMsg: " << CondorErrMsg << endl;
			}
		}
	}
	return;
}

/*********************************************************************
 *
 * Function: process_check_in_view
 * Purpose:  
 *
 *********************************************************************/
static void process_check_in_view(const string &line,
							 int token_start, int line_number, 
							 const Parameters &parameters,
							 ErrorCount *errors)
{
	string  collection_name, view_name, classad_name;
	string  expect, extra;

	collection_name = extract_token(&token_start, line);
	view_name = extract_token(&token_start, line);
	classad_name = extract_token(&token_start, line);
	expect = extract_token(&token_start, line);
	extra = extract_token(&token_start, line);
	if (!collection_name.compare("") || !view_name.compare("")
		|| !classad_name.compare("") || !expect.compare("")) {
		cout << "Error: Missing information on line " << line_number 
			 << "." << endl;
		cout << "       Format: check-in-subview collection view classad "
			 << " [ExpectIn | ExpectNotIn]\n";
		errors->IncrementErrors();
	} else if (expect.compare("ExpectIn") && expect.compare("ExpectNotIn")) {
		cout << "Error: Need to specify \"ExpectIn\" or "
			 << "\"ExpectInMatch\" on line "
			 << line_number << ".\n";
		errors->IncrementErrors();
		expect = "ExpectIn";
	} else if (extra.compare("")) {
		cout << "Error: Extra information on line " << line_number 
			 << "." << endl;
		cout << "       Format: check-in-subview collection view classad "
			 << " [ExpectIn | ExpectNotIn]\n";
		errors->IncrementErrors();
	} else {
		ClassAdCollection  *collection;
		ClassAd            *classad;

		collection = collections[collection_name];
		classad = classads[classad_name];
		if (collection == NULL) {
			cout << "Error: Unknown collection (" << collection_name 
				 << ") on line " << line_number << ".\n";
			errors->IncrementErrors();
		} else if (classad == NULL) {
			cout << "Error: Unknown classad (" << classad_name 
				 << ") on line " << line_number << ".\n";
			errors->IncrementErrors();
		} else {
			bool                  have_view;
			LocalCollectionQuery  query;

			query.Bind(collection);
			have_view = query.Query(view_name, NULL);
			if (!have_view) {
				cout << "Error: Unknown view (" << view_name 
					 << ") on line " << line_number << ".\n";
				errors->IncrementErrors();
			}
			else {
				bool have_classad;
				string classad_key;

				have_classad = false;
				for (query.ToFirst(), query.Current(classad_key); 
					 !query.IsAfterLast(); query.Next(classad_key)) {
					if (!classad_key.compare(classad_name)) {
						have_classad = true;
						break;
					}
				}
				if (!expect.compare("ExpectIn")) {
					if (have_classad) {
						cout << "OK: " << classad_name << " in " 
							 << view_name << " on line " << line_number << ".\n";
					} else {
						cout << "Error: " << classad_name << " is not in " 
							 << view_name << " on line " << line_number << ".\n";
						errors->IncrementErrors();
					}
				}
				else {
					if (have_classad) {
						cout << "Error: " << classad_name << " in " 
							 << view_name << " on line " << line_number << ".\n";
						errors->IncrementErrors();
					} else {
						cout << "OK: " << classad_name << " is not in " 
							 << view_name << " on line " << line_number << ".\n";
					}
				}
			}
		}
	}
	return;
}

/*********************************************************************
 *
 * Function: extract_token
 * Purpose:  Given a string and a place to start in the string,
 *           returns a new string with the next space-delimited
 *           token. Also, token_start is updated so this function can
 *           be called repeatedly to get multiple functions, without
 *           the caller having to worry about what token_start is. 
 *
 *********************************************************************/
static string extract_token(int *token_start, const string &line)
{
	int token_end;
	bool in_quote;
	string token;

	if ((unsigned int) *token_start >= line.size()) {
		token = "";
	}
	else {
		// First, skip whitespace
		while (isspace(line[*token_start])) {
			(*token_start)++;
		}
		token_end = *token_start;
		in_quote = false;
		while ((!isspace(line[token_end]) || in_quote) && line[token_end] != 0) {
			if (line[token_end] == '\"' || line[token_end] == '\'') {
				if (token_end == *token_start) {
					in_quote = true;
				}
				else {
					in_quote = !in_quote;
					token_end++;
					break; // end quote means end of token. 
				}
			}
			// Skip over quote marks that are escaped.
			if (line[token_end] == '\\' 
				&& (line[token_end+1] == '\"' || line[token_end+1] == '\'')) {
				token_end++;
			}
			token_end++;
		}
		
		if (*token_start == token_end) {
			token = "";
		}
		else {
			token = line.substr(*token_start, token_end-(*token_start));
			*token_start = token_end + 1;
		}
	}
	return token;
}

/*********************************************************************
 *
 * Function: empty_line
 * Purpose:  Returns true if the line is of length 0 or contains only
 *           whitespace. Otherwise, returns false.
 *
 *********************************************************************/
static bool empty_line(const string &line)
{
	bool is_empty = true;
	int  index;

	for (index = 0; line[index] != 0; index++) {
		if (!isspace(line[index])) {
			is_empty = false;
			break;
		}
	}
			
	return is_empty;
}


/*********************************************************************
 *
 * Function: Parameters::ParseCommandLine
 * Purpose:  This parses the command line. Note that it will exit
 *           if there are any problems. 
 *
 *********************************************************************/
void Parameters::ParseCommandLine(int argc, char **argv)
{
	// First we set up the defaults.
	debug      = false;
	input_file = "";

	// Then we parse to see what the user wants. 
	for (int arg_index = 1; arg_index < argc; arg_index++) {
		if (!strcmp(argv[arg_index], "-d")
			|| !strcmp(argv[arg_index], "-debug")) {
			debug = true;
		}
		else {
			if (!input_file.compare("")) {
				input_file = argv[arg_index];
			}
		}
	}

	if (!input_file.compare("")) {
		cout << "Error: you didn't specify an input file.\n";
		cout << "Usage: test_classads [-debug] <input-file>\n";
		exit(1);
	}

	return;
}
