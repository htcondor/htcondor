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

#include "classad_distribution.h"
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
    cmd_Diff,
    cmd_Help
};

class Parameters
{
public: 
	bool    debug;
    bool    verbose;
    bool    interactive;
	string  input_file;
	
	void ParseCommandLine(int argc, char **argv);
};

class Variable
{
public:
    // When you give a Variable an ExprTree, it owns it. You don't. 
    Variable(string &name, ExprTree *tree);
    Variable(string &name, Value &value);
    ~Variable();

    void GetStringRepresentation(string &representation);

private:
    string   _name;
    bool     _is_tree; // If false, then is value
    ExprTree *_tree;
    Value    _value;
};

typedef map<string, Variable *> VariableMap;

/*--------------------------------------------------------------------
 *
 * Private Functions
 *
 *--------------------------------------------------------------------*/

bool read_line(string &line, Parameters &parameters);
bool replace_variables(string &line, string &error, Parameters &parameters);
Command get_command(string &line, Parameters &parameters);
void get_variable_name(string &line, bool swallow_equals, string &variable_name, Parameters &parameters);
ExprTree *get_expr(string &line, Parameters &parameters);
void get_two_exprs(string &line, ExprTree *&tree1, ExprTree *&tree2, string &error, Parameters &parameters);
void print_expr(ExprTree *tree, Parameters &parameters);
bool evaluate_expr(ExprTree *tree, Value &value, Parameters &parameters);
void shorten_line(string &line, int offset);

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
	debug       = false;
    verbose     = false;
    interactive = true;
	input_file = "";

	// Then we parse to see what the user wants. 
	for (int arg_index = 1; arg_index < argc; arg_index++) {
		if (   !strcmp(argv[arg_index], "-d")
            || !strcmp(argv[arg_index], "-debug")) {
			debug = true;
		} else if (   !strcmp(argv[arg_index], "-v")
                   || !strcmp(argv[arg_index], "-verbose")) {
			verbose = true;
		} else {
			if (!input_file.compare("")) {
				input_file = argv[arg_index];
                interactive = false;
			}
		}
	}

	return;
}

Variable::Variable(string &name, ExprTree *tree)
{
    _name    = name;
    _tree    = tree;
    _is_tree = true;
    return;
}

Variable::Variable(string &name, Value &value)
{
    _name    = name;
    _value   = value;
    _is_tree = false;
    _tree    = NULL;
    return;
}

Variable::~Variable()
{
    if (_is_tree && _tree != NULL) {
        delete _tree;
        _tree = NULL;
    }
    return;
}

void Variable::GetStringRepresentation(string &representation)
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
 * Function: 
 * Purpose:  
 *
 *********************************************************************/
int main(int argc, char **argv)
{
    string      line;
    Parameters  parameters;

    /* --- */
    Value        value;
    Variable     *var;
    string       name = "alain";

    value.SetIntegerValue(33);
    var = new Variable(name, value);

    variables[name] = var;
    /* --- */

    parameters.ParseCommandLine(argc, argv);

    while (read_line(line, parameters) == true) {
        bool      good_line;
        string    error;
        Command   command;
        string    variable_name;
        ExprTree  *tree, *tree2;
        Variable  *variable;
        

        good_line = replace_variables(line, error, parameters);
        if (!good_line) {
            cout << "Error: " << error << endl;
        } else {
            command = get_command(line, parameters);
            switch (command) {
            case cmd_NoCommand:
                cout << "Error: No command on line.\n";
                break;
            case cmd_InvalidCommand:
                cout << "Unknown command on line.\n";
                break;
            case cmd_Let:
                get_variable_name(line, true, variable_name, parameters);
                tree = get_expr(line, parameters);
                if (tree == NULL) {
                    cout << "Couldn't parse rvalue.\n";
                } else {
                    variable = new Variable(variable_name, tree);
                    variables[variable_name] = variable;
                    print_expr(tree, parameters);
                }
                break;
            case cmd_Eval:
                get_variable_name(line, true, variable_name, parameters);
                tree = get_expr(line, parameters);
                if (tree == NULL) {
                    cout << "Couldn't parse rvalue.\n";
                } else {
                    Value value;
                    if (!evaluate_expr(tree, value, parameters)) {
                        cout << "Couldn't evaluate rvalue.\n";
                    } else {
                        variable = new Variable(variable_name, value);
                        variables[variable_name] = variable;
                        cout << value << endl;
                    }
                }
                break;
            case cmd_Print:
                tree = get_expr(line, parameters);
                print_expr(tree, parameters);
                break;
            case cmd_Same:
                get_two_exprs(line, tree, tree2, error, parameters);
                if (tree == NULL || tree2 == NULL) {
                    cout << "Error: " << error << endl;
                } else {
                    if (tree->SameAs(tree2)) {
                        cout << "Same.\n";
                    } else {
                        cout << "Different.\n";
                    }
                }
                break;
            case cmd_Diff:
                break;
            case cmd_Help:
                break;
            }
        }
        
    }
    cout << endl;
    return 0;
}

/*********************************************************************
 *
 * Function: read_line
 * Purpose:  
 *
 *********************************************************************/
bool read_line(string &line, Parameters &parameters)
{
    bool have_input;

    line = "";
    have_input = true;

    if (cin.eof()) {
        have_input = false;
    } else {
        cout << "> ";
        getline(cin, line, '\n');
        if (cin.eof() && line.size() == 0) {
            have_input = false;
        } else {
            have_input = true;
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
bool replace_variables(string &line, string &error, Parameters &parameters)
{
    bool      good_line;

    good_line = true;
    error = "";
    for (;;) {
        int      dollar, current_position;
        string   variable_name;
        string   variable_value;
        Variable *var;

        current_position = 0;
        dollar = line.find('$', current_position);
        if (dollar == -1) {
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
        if (current_position < (int) line.size()) {
            end = line.substr(current_position);
        } else {
            end = "";
        }
        line = line.substr(0, dollar) + variable_value + end;
    }
    

    if (parameters.debug) {
        cerr << "after replacement: " << line << endl;
    }
    
    return good_line;
}

/*********************************************************************
 *
 * Function: get_command
 * Purpose:  
 *
 *********************************************************************/
Command get_command(string &line, Parameters &parameters)
{
    int      current_position;
    int      length;
    string   command_name;
    Command  command;
   
    current_position = 0;
    length           = line.size();
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
    } else if (command_name == "diff") {
        command = cmd_Diff;
    } else if (command_name == "help") {
        command = cmd_Help;
    } else {
        command = cmd_InvalidCommand;
    }
    shorten_line(line, current_position);
    return command;
}

/*********************************************************************
 *
 * Function: get_variable_name
 * Purpose:  
 *
 *********************************************************************/
void get_variable_name(string &line, bool swallow_equals, string &variable_name, Parameters &parameters)
{
    int      current_position;
    int      length;

    current_position = 0;
    length           = line.size();
    variable_name    = "";

    // Skip whitespace
    while (current_position < length && isspace(line[current_position])) {
        current_position++;
    }
    // Find variable name
    while (current_position < length && isalpha(line[current_position])) {
        variable_name += line[current_position];
        current_position++;
    }
    if (swallow_equals) {
        // Skip whitespace
        while (current_position < length && isspace(line[current_position])) {
            current_position++;
        }
        if (line[current_position] == '=') {
            current_position++;
        }
        // We should probably report an error if we didn't find an equal sign. 
        // Maybe later.
    }

    shorten_line(line, current_position);
    return;
}

/*********************************************************************
 *
 * Function: get_expr
 * Purpose:  
 *
 *********************************************************************/
ExprTree *get_expr(string &line, Parameters &parameters)
{
    int               offset;
    ExprTree          *tree;
    ClassAdParser     parser;
    StringLexerSource lexer_source(&line);

    tree = parser.ParseExpression(&lexer_source, false);
    offset = lexer_source.GetCurrentLocation();
    shorten_line(line, offset);
    return tree;
}

void get_two_exprs(string &line, ExprTree *&tree1, ExprTree *&tree2, string &error, Parameters &parameters)
{
    int offset;
    ClassAdParser parser;
    StringLexerSource lexer_source(&line);

    tree1 = parser.ParseExpression(&lexer_source, false);
    if (tree1 == NULL) {
        error = "Couldn't parse first expression.";
    } else {
        if (parameters.debug) {
            cout << "Tree1: "; 
            print_expr(tree1, parameters); 
            cout << endl;
        }
    
        if (parser.PeekToken() != Lexer::LEX_COMMA) {
            error =  "Missing comma.\n";
            delete tree1;
            tree1 = NULL;
            tree2 = NULL;
        } else {
            parser.ConsumeToken();
            tree2 = parser.ParseNextExpression();
            offset = lexer_source.GetCurrentLocation();
            shorten_line(line, offset);
            if (tree2 == NULL) {
                error = "Couldn't parse second expression.";
                delete tree1;
                tree1 = NULL;
            } else if (parameters.debug){
                cout << "Tree2: "; 
                print_expr(tree2, parameters); 
                cout << endl;
            }
        }
    }

    return;
}


void print_expr(ExprTree *tree, Parameters &parameters)
{
    string output;
    ClassAdUnParser unparser;
    
    unparser.Unparse(output, tree);
    cout << output << endl;
    return;
}

bool evaluate_expr(ExprTree *tree, Value &value, Parameters &parameters)
{
    ClassAd classad;
    bool    success;

    classad.Insert("internal___", tree);
    success = classad.EvaluateAttr("internal___", value);
    classad.Remove("internal___");
    return success;
}

void shorten_line(string &line, int offset)
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
