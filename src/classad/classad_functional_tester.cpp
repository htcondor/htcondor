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


#include "classad/classad_distribution.h"
#include "classad/lexerSource.h"
#include "classad/xmlSink.h"
#include <fstream>
#include <iostream>
#include <ctype.h>
#include <assert.h>
#include "classad_functional_tester.h"

using std::string;
using std::vector;
using std::pair;
using std::map;
using std::cin;
using std::cout;
using std::cerr;
using std::ifstream;
using std::ofstream;
using std::endl;

using namespace classad;

typedef map<string, Variable *> VariableMap;

/*--------------------------------------------------------------------
 *
 * Private Data Types
 *
 *--------------------------------------------------------------------*/

enum Command
{
    cmd_NoCommand,
    cmd_InvalidCommand,
    cmd_Let,
    cmd_Eval,
    cmd_Print,
    cmd_Same,
    cmd_Sameq,
    cmd_Diff,
    cmd_Diffq,
    cmd_Set,
    cmd_Show,
    cmd_Writexml,
    cmd_Readxml,
    cmd_Echo,
    cmd_Help,
    cmd_Quit
};

enum PrintFormat
{
    print_Compact,
    print_Pretty,
    print_XML,
    print_XMLPretty
};

class Parameters
{
public: 
	bool      debug;
    bool      verbose;
    bool      interactive;
    ifstream  *input_file;

	Parameters() { input_file = NULL; }
    ~Parameters() { if (input_file) delete input_file; }
	void ParseCommandLine(int argc, char **argv);
};

class State
{
public:
    State();

    int         number_of_errors;
    int         line_number;
    PrintFormat format;
};

typedef map<string, Variable *> VariableMap;

/*--------------------------------------------------------------------
 *
 * Private Functions
 *
 *--------------------------------------------------------------------*/

bool read_line(string &line, State &state, Parameters &parameters);
bool read_line_stdin(string &line, State &state, Parameters &parameters);
bool read_line_file(string &line, State &state, Parameters &parameters);
bool replace_variables(string &line, State &state, Parameters &parameters);
Command get_command(string &line, Parameters &parameters);
bool get_variable_name(string &line, bool swallow_equals, string &variable_name, 
                       State &state, Parameters &parameters);
bool get_file_name(string &line, string &variable_name, State &state,
                   Parameters &parameters);
ExprTree *get_expr(string &line, State &state, Parameters &parameters);
void get_two_exprs(string &line, ExprTree *&tree1, ExprTree *&tree2, 
                   State &state, Parameters &parameters);
void print_expr(ExprTree *tree, State &state, Parameters &parameters);
bool evaluate_expr(ExprTree *tree, Value &value, Parameters &parameters);
void shorten_line(string &line, int offset);
bool handle_command(Command command, string &line, State &state, 
                    Parameters &parameters);
void handle_let(string &line, State &state, Parameters &parameters);
void handle_eval(string &line, State &state, Parameters &parameters);
void handle_same(string &line, State &state, Parameters &parameters);
void handle_sameq(string &line, State &state, Parameters &parameters);
void handle_diff(string &line, State &state, Parameters &parameters);
void handle_diffq(string &line, State &state, Parameters &parameters);
void handle_set(string &line, State &state, Parameters &parameters);
void handle_show(string &line, State &state, Parameters &parameters);
void handle_writexml(string &line, State &state, Parameters &parameters);
void handle_readxml(string &line, State &state, Parameters &parameters);
void handle_echo(string &line, State &state, Parameters &parameters);
void handle_print(string &line, State &state, Parameters &parameters);
void handle_help(void);
void print_version(void);
void print_error_message(const char *error, State &state);
void print_error_message(string &error, State &state);
void print_final_state(State &state);
bool line_is_comment(string &line);
bool expr_okay_for_xml_file(ExprTree *tree, State &state, Parameters &parameters);

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
	// First we set up the defaults.
	debug       = false;
    verbose     = false;
    interactive = true;
	input_file  = NULL;

	// Then we parse to see what the user wants. 
	for (int arg_index = 1; arg_index < argc; arg_index++) {
		if (   !strcasecmp(argv[arg_index], "-d")
            || !strcasecmp(argv[arg_index], "-debug")) {
			debug = true;
		} else if (   !strcasecmp(argv[arg_index], "-v")
                   || !strcasecmp(argv[arg_index], "-verbose")) {
			verbose = true;
		} else {
			if (input_file == NULL) {
                interactive = false;
				input_file = new ifstream(argv[arg_index]);
                if (input_file->bad()) {
                    cout << "Could not open file '" << argv[arg_index] <<"'.\n";
                    exit(1);
                }
			}
		}
	}

	return;
}

/*********************************************************************
 *
 * Function: 
 * Purpose:  
 *
 *********************************************************************/
State::State()
{
    number_of_errors = 0;
    line_number      = 0;
    format           = print_Compact;
    return;
}

/*********************************************************************
 *
 * Function: 
 * Purpose:  
 *
 *********************************************************************/
Variable::Variable(
    string  &name, 
    ExprTree *tree)
{
    _name    = name;
    _tree    = tree;
    _is_tree = true;
    return;
}

/*********************************************************************
 *
 * Function: 
 * Purpose:  
 *
 *********************************************************************/
Variable::Variable(
    string &name, 
    Value  &value)
{
    _name    = name;
    _value   = value;
    _is_tree = false;
    _tree    = NULL;
    return;
}

/*********************************************************************
 *
 * Function: 
 * Purpose:  
 *
 *********************************************************************/
Variable::~Variable()
{
    if (_is_tree && _tree != NULL) {
        delete _tree;
        _tree = NULL;
    }
    return;
}

/*********************************************************************
 *
 * Function: 
 * Purpose:  
 *
 *********************************************************************/
void Variable::GetStringRepresentation(
    string &representation)
{
    ClassAdUnParser unparser;

    if (_is_tree) {
        unparser.Unparse(representation, _tree);
    } else {
        unparser.Unparse(representation, _value);
    }
    return;
}

VariableMap  variables;

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
    bool        quit;
    string      line;
    State       state;
    Parameters  parameters;

    print_version();
    parameters.ParseCommandLine(argc, argv);
    quit = false;

    while (!quit && read_line(line, state, parameters) == true) {
        bool      good_line;
        string    error;
        Command   command;

        good_line = replace_variables(line, state, parameters);
        if (good_line) {
            command = get_command(line, parameters);
            quit = handle_command(command, line, state, parameters);
        }
    }
    print_final_state(state);

    if (!parameters.interactive && parameters.input_file != NULL) {
        parameters.input_file->close();
    }

    if (state.number_of_errors == 0) {
        return 0;
    } else {
        return 1;
    }
}

/*********************************************************************
 *
 * Function: read_line
 * Purpose:  
 *
 *********************************************************************/
bool read_line(
    string     &line, 
    State      &state, 
    Parameters &parameters)
{
    bool have_input;

    if (parameters.interactive) {
        have_input = read_line_stdin(line, state, parameters);
    } else {
        have_input = read_line_file(line, state, parameters);
    }
    return have_input;
}

/*********************************************************************
 *
 * Function: read_line_stdin
 * Purpose:  
 *
 *********************************************************************/
bool read_line_stdin(
    string     &line, 
    State      &state, 
    Parameters &)
{
    bool have_input;

    line = "";
    have_input = true;

    if (cin.eof()) {
        have_input = false;
    } else {
        bool   in_continuation = false;
        bool   done = false;
        string line_segment;

        cout << "> ";

        while (!done) {
            getline(cin, line_segment, '\n');
            if (line_is_comment(line_segment)) {
                // ignore comments
                if (in_continuation) {
                    cout << "? ";
                    continue;
                } else {
                    have_input = true;
                    line = "";
                    break;
                }
            }
            line += line_segment;
            state.line_number++;
            if (cin.eof() && line.size() == 0) {
                have_input = false;
            } else {
                have_input = true;
            }
            if (line[line.size()-1] == '\\' && !cin.eof()) {
                in_continuation = true;
                line = line.substr(0, line.size()-1); // chop off trailing /
                done = false;
                cout << "? ";
            } else {
                done = true;
            }
        }
    }

    if (!have_input) {
        // The user did not explicitly quit, but the end of input
        // was reached. So print out a newline to look nice.
        cout << endl;
    }

    return have_input;
}

/*********************************************************************
 *
 * Function: read_line_file
 * Purpose:  
 *
 *********************************************************************/
bool read_line_file(
    string     &line, 
    State      &state, 
    Parameters &parameters)
{
    bool           have_input;
    static bool    have_cached_line = false;
    static string  cached_line = "";

    // We read a line, either from our one-line cache, or the file.
    if (!have_cached_line) {
        getline(*parameters.input_file, cached_line, '\n');
        state.line_number++;
        have_cached_line = true;
    } else {
        // We have a cached-line, but we need to increment the line number for it.
        // We don't increment it until we use it. 
        state.line_number++;
    }
    if (parameters.input_file->eof() && cached_line.size() == 0) {
        have_input = false;
        have_cached_line = false;
    } else {
        line = cached_line;
        have_input = true;
        have_cached_line = false;
    }

    // If we actually have a non-comment line, we read another line
    // from the file If it begins with a whitespace character, then we
    // append it to the previous line, otherwise we cache it for the
    // next time we call this function.
    if (have_input) {
        if (line_is_comment(line)) {
            line = "";
        } else {
            bool done = false;
            while (!done) {
                getline(*parameters.input_file, cached_line, '\n');
                if (line_is_comment(cached_line)) {
                    // ignore comments
                    state.line_number++;
                } else if (isspace(cached_line[0])) {
                    line += cached_line;
                    state.line_number++;
                } else {
                    done = true;
                    have_cached_line = true;
                }
            }
        }
    }
    return have_input;
}
    
    
/*********************************************************************
 *
 * Function: replace_variables
 * Purpose:  
 *
 *********************************************************************/
bool replace_variables(
    string     &line, 
    State      &state, 
    Parameters &parameters)
{
    bool    good_line;
    string  error;

    good_line = true;
    error = "";
    for (;;) {
        size_t      dollar, current_position;
        string   variable_name;
        string   variable_value;
        Variable *var;

        current_position = 0;
        dollar = line.find('$', current_position);
        if (dollar == std::string::npos) {
            break;
        }
        current_position = dollar + 1;
        if (!isalpha(line[current_position])){ 
            good_line = false;
            error = "Bad variable name.";
            break;
        }
        current_position++;
        while (isalnum(line[current_position]) || line[current_position] == '_') {
            current_position++;
        }
        
        variable_name = line.substr(dollar + 1, current_position - (dollar + 1));

        var = variables[variable_name];

        if (var == NULL) {
            good_line = false;
            error = "Unknown variable '$";
            error += variable_name;
            error += "'";
            break;
        }

        var->GetStringRepresentation(variable_value);

        // We have to be careful with substr() because with gcc 2.96, it likes to 
        // assert/except if you give it values that are too large.
        string end;
        if (current_position < line.size()) {
            end = line.substr(current_position);
        } else {
            end = "";
        }
        line = line.substr(0, dollar) + variable_value + end;
    }
    

    if (parameters.debug) {
        cerr << "# after replacement: " << line << endl;
    }
    
    if (!good_line) {
        print_error_message(error, state);
    }

    return good_line;
}

/*********************************************************************
 *
 * Function: get_command
 * Purpose:  
 *
 *********************************************************************/
Command get_command(
    string     &line, 
    Parameters &)
{
    int      current_position;
    int      length;
    string   command_name;
    Command  command;
   
    current_position = 0;
    length           = (int)line.size();
    command_name     = "";
    command          = cmd_NoCommand;

    // Skip whitespace
    while (current_position < length && isspace(line[current_position])) {
        current_position++;
    }
    // Find command name
    while (current_position < length && isalpha(line[current_position])) {
        command_name += line[current_position];
        current_position++;
    }
    // Figure out what the command is.
    if (command_name.size() == 0) {
        command = cmd_NoCommand;
    } else if (command_name == "let") {
        command = cmd_Let;
    } else if (command_name == "eval") {
        command = cmd_Eval;
    } else if (command_name == "print") {
        command = cmd_Print;
    } else if (command_name == "same") {
        command = cmd_Same;
    } else if (command_name == "sameq") {
        command = cmd_Sameq;
    } else if (command_name == "diff") {
        command = cmd_Diff;
    } else if (command_name == "diffq") {
        command = cmd_Diffq;
    } else if (command_name == "set") {
        command = cmd_Set;
    } else if (command_name == "show") {
        command = cmd_Show;
    } else if (command_name == "writexml") {
        command = cmd_Writexml;
    } else if (command_name == "readxml") {
        command = cmd_Readxml;
    } else if (command_name == "echo") {
        command = cmd_Echo;
    } else if (command_name == "help") {
        command = cmd_Help;
    } else if (command_name == "quit") {
        command = cmd_Quit;
    } else {
        command = cmd_InvalidCommand;
    }
    shorten_line(line, current_position);
    return command;
}

/*********************************************************************
 *
 * Function: handle_command
 * Purpose:  
 *
 *********************************************************************/
bool handle_command(
    Command    command, 
    string     &line, 
    State      &state, 
    Parameters &parameters)
{
    bool quit = false;

    switch (command) {
    case cmd_NoCommand:
        // Ignore. This isn't a problem.
        break;
    case cmd_InvalidCommand:
        print_error_message("Unknown command on line", state);
        break;
    case cmd_Let:
        handle_let(line, state, parameters);
        break;
    case cmd_Eval:
        handle_eval(line, state, parameters);
        break;
    case cmd_Print:
        handle_print(line, state, parameters);
        break;
    case cmd_Same:
        handle_same(line, state, parameters);
        break;
    case cmd_Sameq:
        handle_sameq(line, state, parameters);
        break;
    case cmd_Diff:
        handle_diff(line, state, parameters);
        break;
    case cmd_Diffq:
        handle_diffq(line, state, parameters);
        break;
    case cmd_Set:
        handle_set(line, state, parameters);
        break;
    case cmd_Show:
        handle_show(line, state, parameters);
        break;
    case cmd_Writexml:
        handle_writexml(line, state, parameters);
        break;
    case cmd_Readxml:
        handle_readxml(line, state, parameters);
        break;
    case cmd_Echo:
        handle_echo(line, state, parameters);
        break;
    case cmd_Help:
        handle_help();
        break;
    case cmd_Quit:
        quit = true;
        break;
    }
    return quit;
}

/*********************************************************************
 *
 * Function: handle_let
 * Purpose:  
 *
 *********************************************************************/
void handle_let(
    string     &line, 
    State      &state, 
    Parameters &parameters)
{
    string    variable_name;
    ExprTree  *tree;
    Variable  *variable;

    if (get_variable_name(line, true, variable_name, state, parameters)) {
        tree = get_expr(line, state, parameters);
        if (tree != NULL) {
            variable = new Variable(variable_name, tree);
            variables[variable_name] = variable;
            if (parameters.interactive) {
                print_expr(tree, state, parameters);
            }
        }
    }
    return;
}

/*********************************************************************
 *
 * Function: handle_eval
 * Purpose:  
 *
 *********************************************************************/
void handle_eval(
    string     &line, 
    State      &state, 
    Parameters &parameters)
{
    string    variable_name;
    ExprTree  *tree;
    Variable  *variable;

    if (get_variable_name(line, true, variable_name, state, parameters)) {
        tree = get_expr(line, state, parameters);
        if (tree != NULL) {
            Value value;
            if (!evaluate_expr(tree, value, parameters)) {
                print_error_message("Couldn't evaluate rvalue", state);
            } else {
                variable = new Variable(variable_name, value);
                variables[variable_name] = variable;
                if (parameters.interactive) {
					classad::ClassAdUnParser unp;
					std::string s;
					unp.Unparse(s, value);
                    cout << s << endl;
                }
            }
        }
    }
    return;
}

/*********************************************************************
 *
 * Function: handle_print
 * Purpose:  
 *
 *********************************************************************/
void handle_print(
    string     &line, 
    State      &state, 
    Parameters &parameters)
{
    ExprTree *tree;

    tree = get_expr(line, state, parameters);
    if (tree) {
        print_expr(tree, state, parameters);
    }
	delete tree;
    return;
}

/*********************************************************************
 *
 * Function: handle_same
 * Purpose:  
 *
 *********************************************************************/
void handle_same(
    string     &line, 
    State      &state, 
    Parameters &parameters)
{
    ExprTree  *tree, *tree2;
    Value     value1, value2;

    get_two_exprs(line, tree, tree2, state, parameters);
    if (tree != NULL || tree2 != NULL) {
        if (parameters.debug) {
            cout << "Sameeval has two trees: \n";
            cout << " "; 
            print_expr(tree, state, parameters); 
            cout << endl;
            cout << " "; 
            print_expr(tree2, state, parameters);
            cout << endl;
        }
        if (!evaluate_expr(tree, value1, parameters)) {
            print_error_message("Couldn't evaluate first expression.\n", state);
        } else if (!evaluate_expr(tree2, value2, parameters)) {
            print_error_message("Couldn't evaluate second expressions.\n", state);
        } else if (!value1.SameAs(value2)) {
            if (parameters.debug) {
				classad::ClassAdUnParser unp;
				std::string s1;
				std::string s2;
				unp.Unparse(s1, value1);
				unp.Unparse(s2, value2);
                cout << "They evaluated to: \n";
                cout << " " << s1 << endl;
                cout << " " << s2 << endl;
            }
            print_error_message("the expressions are different.", state);
        }
    }

    if (tree != NULL) {
        delete tree;
    } 
    if (tree2 != NULL) {
        delete tree2;
    }
    return;
}

/*********************************************************************
 *
 * Function: handle_sameq
 * Purpose:  
 *
 *********************************************************************/
void handle_sameq(
    string     &line, 
    State      &state, 
    Parameters &parameters)
{
    ExprTree  *tree, *tree2;

    get_two_exprs(line, tree, tree2, state, parameters);
    if (tree != NULL || tree2 != NULL) {
        if (!tree->SameAs(tree2)) {
            print_error_message("the expressions are different.", state);
        }
    }
    if (tree != NULL) {
        delete tree;
    }
    if (tree2 != NULL) {
        delete tree2;
    }
    return;
}

/*********************************************************************
 *
 * Function: handle_diff
 * Purpose:  
 *
 *********************************************************************/
void handle_diff(
    string     &line, 
    State      &state, 
    Parameters &parameters)
{
    ExprTree  *tree, *tree2;
    Value     value1, value2;

    get_two_exprs(line, tree, tree2, state, parameters);
    if (tree != NULL || tree2 != NULL) {
        if (!evaluate_expr(tree, value1, parameters)) {
            print_error_message("Couldn't evaluate first expression.\n", state);
        } else if (!evaluate_expr(tree2, value2, parameters)) {
            print_error_message("Couldn't evaluate second expressions.\n", state);
        } else if (value1.SameAs(value2)) {
                print_error_message("the expressions are the same.", state);
        }
    }
    if (tree != NULL) {
        delete tree;
    }
    if (tree2 != NULL) {
        delete tree2;
    }
    return;
}

/*********************************************************************
 *
 * Function: handle_diffq
 * Purpose:  
 *
 *********************************************************************/
void handle_diffq(
    string     &line, 
    State      &state, 
    Parameters &parameters)
{
    ExprTree  *tree, *tree2;

    get_two_exprs(line, tree, tree2, state, parameters);
    if (tree != NULL || tree2 != NULL) {
        if (tree->SameAs(tree2)) {
            print_error_message("the expressions are the same.", state);
        }
    }
    if (tree != NULL) {
        delete tree;
    }
    if (tree2 != NULL) {
        delete tree2;
    }
    return;
}

/*********************************************************************
 *
 * Function: handle_set
 * Purpose:  
 *
 *********************************************************************/
void handle_set(
    string     &line, 
    State      &state, 
    Parameters &parameters)
{
    string  option_name;
    string  option_value;

    if (get_variable_name(line, false, option_name, state, parameters)) {
        if (get_variable_name(line, false, option_value, state, parameters)) {
            if (option_name == "format") {
                if (option_value == "compact") {
                    state.format = print_Compact;
                } else if (option_value == "pretty") {
                    state.format = print_Pretty; 
                } else if (option_value == "xml") {
                    state.format = print_XML;
                } else if (option_value == "xmlpretty") {
                    state.format = print_XMLPretty;
                } else {
                    print_error_message("Unknown print format. Use compact, pretty, xml, or xmlpretty", state);
                }
            } else {
                print_error_message("Unknown option. The only option currently available is format", state);
            }
        }
    }
    return;
}

/*********************************************************************
 *
 * Function: handle_show
 * Purpose:  
 *
 *********************************************************************/
void handle_show(
    string     &line, 
    State      &state, 
    Parameters &parameters)
{
    string  option_name;

    if (get_variable_name(line, false, option_name, state, parameters)) {
        if (option_name == "format") {
            cout << "Format: ";
            switch (state.format) {
            case print_Compact:
                cout << "Traditional Compact\n";
                break;
            case print_Pretty:
                cout << "Traditional Pretty\n";
                break;
            case print_XML:
                cout << "XML Compact\n";
                break;
            case print_XMLPretty:
                cout << "XML Pretty\n";
                break;
            }
        } else {
            print_error_message("Unknown option. The only option currently available is format", state);
        }
    }

    return;
}

/*********************************************************************
 *
 * Function: handle_writexml
 * Purpose:  
 *
 *********************************************************************/
void handle_writexml(
    string     &line, 
    State      &state, 
    Parameters &parameters)
{
    string    filename;
    ExprTree  *expr;
    ofstream  xml_file;

    expr = NULL;
    if (get_file_name(line, filename, state, parameters)) {
        if ((expr = get_expr(line, state, parameters)) != NULL) {
            if (expr_okay_for_xml_file(expr, state, parameters)) {
                xml_file.open(filename.c_str());
                if (xml_file.bad()) {
                    string error_message;
                    error_message = "Can't open ";
                    error_message += filename;
                    error_message += " for output";
                    print_error_message(error_message, state);
                } else {
                    ClassAdXMLUnParser unparser;
                    string             classad_text;

                    xml_file << "<classads>\n";

                    if (expr->GetKind() == ExprTree::CLASSAD_NODE) {
                        unparser.Unparse(classad_text, expr);
                        xml_file << classad_text;
                    } else {
                        ExprList *list = (ExprList*) expr;
                        ExprList::iterator iter;
                        for (iter = list->begin(); iter != list->end(); iter++) {
                            ExprTree *classad;

                            classad = *iter;
                            classad_text = "";
                            unparser.Unparse(classad_text, classad);
                            xml_file << classad_text << endl;
                        }
                    }
                    xml_file << "</classads>\n";
                }
            }
        }
    }
    if (expr) {
        delete expr;
    }
    return;
}

/*********************************************************************
 *
 * Function: handle_readxml
 * Purpose:  
 *
 *********************************************************************/
void handle_readxml(
    string     &line, 
    State      &state, 
    Parameters &parameters)
{
    string    variable_name;
    string    file_name;

    if (get_variable_name(line, false, variable_name, state, parameters)) {
        if (get_file_name(line, file_name, state, parameters)) {
            FILE *xml_file = fopen(file_name.c_str(), "r");

            if (!xml_file) {
                string error;
                error = "Can't read file: ";
                error += file_name;
                print_error_message(error, state);
            } else {
                ExprList         *list;
                ClassAd          *classad;
                ClassAdXMLParser parser;
                Variable         *variable;

                list = new ExprList();
                do {
                    classad = parser.ParseClassAd(xml_file);
                    if (classad != NULL) {
                        list->push_back(classad);
                    }
                } while (classad != NULL);
                variable = new Variable(variable_name, list);
                variables[variable_name] = variable;
                if (parameters.interactive) {
                    print_expr(list, state, parameters);
                }
            }
        }
    }
    return;
}

/*********************************************************************
 *
 * Function: handle_echo
 * Purpose:  
 *
 *********************************************************************/
void handle_echo(
    string     &line, 
    State      &, 
    Parameters &)
{
    string new_line = "";
    int    index;

    index = 0;

    while (index < (int) line.size() && isspace(line[index])) {
        index++;
    }
    while (index < (int) line.size()) {
        new_line += line[index];
        index++;
    }

    cout << new_line << endl;
    return;
}

/*********************************************************************
 *
 * Function: handle_help
 * Purpose:  
 *
 *********************************************************************/
void handle_help(void)
{
    print_version();

    cout << "\n";
    cout << "Commands:\n";
    cout << "let name = expr   Set a variable to an unevaluated expression.\n";
    cout << "eval name = expr  Set a variable to an evaluated expression.\n";
    cout << "same expr1 expr2  Prints a message only if expr1 and expr2 are different.\n";
    cout << "sameq expr1 expr2 Prints a message only if expr1 and expr2 are different.\n";
    cout <<"                   same evaluates its expressions first, sameq doesn't.\n";
    cout << "diff expr1 expr2  Prints a message only if expr1 and expr2 are the same.\n";
    cout << "diffq expr1 expr2 Prints a message only if expr1 and expr2 are the same.\n";
    cout <<"                   diff evaluates its expressions first, diffq doesn't.\n";
    cout << "set opt value     Sets an option to a particular value.\n";
    cout << "quit              Exit this program.\n";
    cout << "help              Print this message.\n";
    cout << "\n";
    cout << "Options (for the set command):\n";
    cout << "format              Set the way ClassAds print.\n";
    cout << "  compact           A compact, traditional style\n";
    cout << "  pretty            Traditional, with more spaces\n";
    cout << "  xml               A compact XML representation\n";
    cout << "  xmlpretty         XML with extra spacing for readability.\n";
    return;
}

/*********************************************************************
 *
 * Function: get_variable_name
 * Purpose:  
 *
 *********************************************************************/
bool get_variable_name(
    string     &line, 
    bool       swallow_equals, 
    string     &variable_name, 
    State      &state,
    Parameters &parameters)
{
    int      current_position;
    int      length;
    bool     have_good_name;

    current_position = 0;
    length           = (int)line.size();
    variable_name    = "";
    have_good_name   = false;

    // Skip whitespace
    while (current_position < length && isspace(line[current_position])) {
        current_position++;
    }
    // Find variable name
    if (current_position < length && isalpha(line[current_position])) {
        variable_name += line[current_position];
        current_position++;
        // As soon as we have at least one character in the name, it's good.
        have_good_name = true;

        while (   current_position < length 
                  && (isalnum(line[current_position]) || line[current_position] == '_')) {
            variable_name += line[current_position];
            current_position++;
        }
    }
    if (!have_good_name) {
        print_error_message("Bad variable name", state);
    } else if (swallow_equals) {
        // Skip whitespace
        while (current_position < length && isspace(line[current_position])) {
            current_position++;
        }
        if (line[current_position] == '=') {
            current_position++;
        } else {
            print_error_message("Missing equal sign", state);
            have_good_name = false;
        }
    }

    if (parameters.debug) {
        if (have_good_name) {
            cout << "# Got variable name: " << variable_name << endl;
        } else {
            cout << "# Bad variable name: " << variable_name << endl;
        }
    }

    shorten_line(line, current_position);
    return have_good_name;
}

/*********************************************************************
 *
 * Function: get_file_name
 * Purpose:  
 *
 *********************************************************************/
bool get_file_name(
    string     &line, 
    string     &variable_name, 
    State      &state,
    Parameters &parameters)
{
    int      current_position;
    int      length;
    bool     have_good_name;

    current_position = 0;
    length           = (int)line.size();
    variable_name    = "";
    have_good_name   = false;

    // Skip whitespace
    while (current_position < length && isspace(line[current_position])) {
        current_position++;
    }
    // Find file name
    while (   current_position < length 
              && (!isspace(line[current_position]))) {
        have_good_name = true;
        variable_name += line[current_position];
        current_position++;
    }
    if (!have_good_name) {
        print_error_message("Bad file name", state);
    } 

    if (parameters.debug) {
        if (have_good_name) {
            cout << "# Got file name: " << variable_name << endl;
        } else {
            cout << "# Bad file name: " << variable_name << endl;
        }
    }

    shorten_line(line, current_position);
    return have_good_name;
}

/*********************************************************************
 *
 * Function: get_expr
 * Purpose:  
 *
 *********************************************************************/
ExprTree *get_expr(
    string     &line, 
    State      &state, 
    Parameters &)
{
    int               offset;
    ExprTree          *tree;
    ClassAdParser     parser;
    StringLexerSource lexer_source(&line);

    tree = parser.ParseExpression(&lexer_source, false);
    offset = lexer_source.GetCurrentLocation();
    shorten_line(line, offset);

    if (tree == NULL) {
        print_error_message("Missing expression", state);
    }

    return tree;
}

/*********************************************************************
 *
 * Function: get_two_exprs
 * Purpose:  
 *
 *********************************************************************/
void get_two_exprs(
    string     &line, 
    ExprTree   *&tree1, 
    ExprTree   *&tree2, 
    State      &state, 
    Parameters &parameters)
{
    int offset;
    ClassAdParser parser;
    StringLexerSource lexer_source(&line);

    tree1 = parser.ParseExpression(&lexer_source, false);
    if (tree1 == NULL) {
        print_error_message("Couldn't parse first expression.", state);
        tree2 = NULL;
    } else {
        if (parameters.debug) {
            cout << "# Tree1: "; 
            print_expr(tree1, state, parameters); 
            cout << endl;
        }
    
        if (parser.PeekToken() != Lexer::LEX_COMMA) {
            print_error_message("Missing comma.\n", state);
            delete tree1;
            tree1 = NULL;
            tree2 = NULL;
        } else {
            parser.ConsumeToken();
            tree2 = parser.ParseNextExpression();
            offset = lexer_source.GetCurrentLocation();
            shorten_line(line, offset);
            if (tree2 == NULL) {
                print_error_message("Couldn't parse second expression.", state);
                delete tree1;
                tree1 = NULL;
            } else if (parameters.debug){
                cout << "# Tree2: "; 
                print_expr(tree2, state, parameters); 
                cout << "# Tree1: "; 
                print_expr(tree1, state, parameters); 
                cout << endl;
            }
        }
    }

    return;
}


/*********************************************************************
 *
 * Function: print_expr
 * Purpose:  
 *
 *********************************************************************/
void print_expr(
    ExprTree   *tree, 
    State      &state,
    Parameters &)
{
    string output;

    if (state.format == print_Compact) {
        ClassAdUnParser unparser;
        unparser.Unparse(output, tree);
    } else if (state.format == print_Pretty) {
        PrettyPrint     unparser;
        unparser.Unparse(output, tree);
    } else if (state.format == print_XML) {
        ClassAdXMLUnParser unparser;
        unparser.SetCompactSpacing(true);
        unparser.Unparse(output, tree);
    } else if (state.format == print_XMLPretty) {
        ClassAdXMLUnParser unparser;
        unparser.SetCompactSpacing(false);
        unparser.Unparse(output, tree);
    }
    cout << output << endl;
    return;
}

/*********************************************************************
 *
 * Function: evaluate_expr
 * Purpose:  
 *
 *********************************************************************/
bool evaluate_expr(
    ExprTree   *tree, 
    Value      &value, 
    Parameters &)
{
    ClassAd classad;
    bool    success;

    classad.Insert("internal___", tree);
    success = classad.EvaluateAttr("internal___", value);
    classad.Remove("internal___");
    return success;
}

/*********************************************************************
 *
 * Function: shorten_line
 * Purpose:  
 *
 *********************************************************************/
void shorten_line(
    string &line, 
    int    offset)
{
    // We have to be careful with substr() because with gcc 2.96, it likes to 
    // assert/except if you give it values that are too large.
    if (offset < (int) line.size()) {
        line = line.substr(offset);
    } else {
        line = "";
    }
    return;
}

/*********************************************************************
 *
 * Function: print_version
 * Purpose:  
 *
 *********************************************************************/
void print_version(void)
{
    string classad_version;

    ClassAdLibraryVersion(classad_version);

    cout << "ClassAd Functional Tester v" << classad_version << endl;

    return;
}

/*********************************************************************
 *
 * Function: print_error_message
 * Purpose:  
 *
 *********************************************************************/
void print_error_message(
    const char  *error, 
    State &state)
{
    string error_s = error;
    print_error_message(error_s, state);
    return;
}


/*********************************************************************
 *
 * Function: print_error_message
 * Purpose:  
 *
 *********************************************************************/
void print_error_message(
    string &error, 
    State  &state)
{
    cout << "* Line " << state.line_number << ": " << error << endl;
    state.number_of_errors++;
    return;
}

/*********************************************************************
 *
 * Function: print_final_state
 * Purpose:  
 *
 *********************************************************************/
void print_final_state(
    State &state)
{
    if (state.number_of_errors == 0) {
        cout << "No errors.\n";
    } else if (state.number_of_errors == 1) {
        cout << "1 error.\n";
    } else {
        cout << state.number_of_errors << " errors\n";
    }
    return;
}

bool line_is_comment(
    string &line)
{
    bool is_comment;

    if (line.size() > 1 && line[0] == '/' && line[1] == '/') {
        is_comment = true;
    } else {
        is_comment = false;
    }
    return is_comment;
}

bool expr_okay_for_xml_file(
    ExprTree   *tree,
    State      &state,
    Parameters &)
{
    bool is_okay;

    if (tree->GetKind() == ExprTree::CLASSAD_NODE) {
        is_okay = true;
    } else if (tree->GetKind() != ExprTree::EXPR_LIST_NODE) {
        is_okay = false;
        cout << "We have " << (int) tree->GetKind() << endl;
    } else {
        ExprList *list = (ExprList *) tree;
        ExprList::iterator  iter;

        is_okay = true;
        for (iter = list->begin(); iter != list->end(); iter++) {
            ExprTree *element;

            element = *iter;
            if (element->GetKind() != ExprTree::CLASSAD_NODE) {
                cout << "Inside list, we have " << (int) tree->GetKind() << endl;
                is_okay = false;
                break;
            }
        }
    }

    if (!is_okay) {
        print_error_message(
            "writexml requires a ClassAd or list of ClassAds as an argument.", 
            state);
    }
    return is_okay;
}
