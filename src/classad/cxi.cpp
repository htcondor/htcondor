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
#include <iostream>

using namespace std;
using namespace classad;

enum Commands {
	_NO_CMD_,

	CLEAR_TOPLEVEL, 
	DEEP_INSERT_ATTRIBUTE, 
	DEEP_DELETE_ATTRIBUTE,
	EVALUATE, 
	EVALUATE_WITH_SIGNIFICANCE,
	FLATTEN, 
	HELP, 
	INSERT_ATTRIBUTE, 
	NEW_TOPLEVEL, 
	OUTPUT_TOPLEVEL, 
	QUIT,
	REMOVE_ATTRIBUTE, 
	SET_MATCH_TOPLEVEL,

	_LAST_COMMAND_ 
};

string CommandWords[] = 
{
	"",

	"clear_toplevel",
	"deep_insert",
	"deep_delete",
	"evaluate",
	"sigeval",
	"flatten",
	"help",
	"insert_attribute",
	"new_toplevel",
	"output_toplevel",
	"quit",
	"delete_attribute",
	"set_match_toplevel",

	""	
};

void help( void );
int  findCommand( string cmdStr );

int 
main( void )
{
	ClassAd 		*ad, *adptr;
    string          cmdString;
	ExprTree		*expr=NULL, *fexpr=NULL;
	int 			command;
	Value			value;
	ClassAdParser	parser;
	PrettyPrint		unparser;
	string			output, buffer1, buffer2, buffer3;

	ad = new ClassAd();

	cout << "'h' for help\n# ";
	while( 1 ) {	
	    cin >> cmdString;
        if (cin.eof()) {
            cout << "\nExiting.\n";
            exit(0);
        }
		command = findCommand( cmdString );
		switch( command ) {
			case CLEAR_TOPLEVEL: 
				ad->Clear();
                getline(cin, buffer1, '\n');
				break;

			case DEEP_INSERT_ATTRIBUTE:
                cin >> buffer1;
				if( buffer1.length() < 1 ) {	// expr
					cout << "Error reading primary expression\n";
					break;
				}
				if( !parser.ParseExpression( buffer1, expr, true ) ) {
					cout << "Error parsing expression: " <<  buffer1 << endl;
                    cout << "(" << CondorErrMsg << ")\n";
					break;
				}
                cin >> buffer2;
				if( buffer2.length() < 1) {
					cout << "Error reading attribute name\n";
					break;
				}	
                getline(cin, buffer3, '\n');
				if( !parser.ParseExpression( buffer3, fexpr, true ) ) {
					cout << "Error parsing expression: " << buffer3 << endl;
                    cout << "(" << CondorErrMsg << ")\n";
					break;
				} 
				expr->SetParentScope( ad );
				if( !ad->EvaluateExpr( expr, value ) ) {
					cout << "Error evaluating expression: " <<  buffer1 << endl;
					delete expr;
					delete fexpr;
					expr = NULL;
					fexpr = NULL;
					break;
				}
				if( !value.IsClassAdValue( adptr ) ) {
					cout << "Error:  Primary expression was not a classad\n";
					delete expr;
					expr = NULL;
					delete fexpr;
					fexpr = NULL;
					break;
				}	
				if( !adptr->Insert( buffer2, fexpr ) ) {
					cout << "Error Inserting expression: " << buffer3 << endl;
					delete fexpr;
					delete expr;
					fexpr = NULL;
					expr = NULL;
				}
				delete expr;
				adptr = NULL;
				expr  = NULL;
				break;

			case DEEP_DELETE_ATTRIBUTE:
                cin >> buffer1;
				if( buffer1.length() < 1) {
					cout << "Error reading primary expression\n";
					break;
				}
				if( !parser.ParseExpression( buffer1, expr, true ) ) {
					cout << "Error parsing expression: " << buffer1 << endl;
					break;
				}
                getline(cin, buffer2, '\n');
				expr->SetParentScope( ad );
				if( !ad->EvaluateExpr( expr, value ) ) {
					cout << "Error evaluating expression: " <<  buffer1 << endl;
					delete expr;
					break;
				}
				if( !value.IsClassAdValue( adptr ) ) {
					cout << "Error:  Primary expression was not a classad\n";
					delete expr;
					expr = NULL;
					break;
				}
				if( !adptr->Remove( buffer2 ) ) {
					cout << "Warning:  Attribute %s not found" << buffer2 << endl;
				}
				delete expr;
				expr = NULL;
				break;

			case EVALUATE:
                getline(cin, buffer1, '\n');
				if( !parser.ParseExpression( buffer1, expr, true ) ) {
					cout << "Error parsing expression: " << buffer1 << endl;
					break;
				}
				expr->SetParentScope( ad );
				if( !ad->EvaluateExpr( expr, value ) ) {
					cout << "Error evaluating expression: " << buffer1 << endl;
					delete expr;
					expr = NULL;
					break;
				}
				output = "";
				unparser.Unparse( output, value );
				cout << output << endl;
				delete expr;
				expr = NULL;
				break;
		
			case EVALUATE_WITH_SIGNIFICANCE:
                getline(cin, buffer1, '\n');
				if( !parser.ParseExpression( buffer1, expr, true ) ) {
					cout << "Error parsing expression: " <<  buffer1 << endl;
					break;
				}
				expr->SetParentScope( ad );
				if( !ad->EvaluateExpr( expr, value, fexpr ) ) {
					cout << "Error evaluating expression: " << buffer1 << endl;
					delete expr;
					delete fexpr;
					expr = NULL;
					fexpr = NULL;
					break;
				}
				output = "";
				unparser.Unparse( output, value );
				cout << output << endl;
				output = "";
				unparser.Unparse( output, fexpr );
				cout << output << endl;
				delete expr;
				expr = NULL;
				delete fexpr;
				fexpr = NULL;
				break;

			case FLATTEN: 
                getline(cin, buffer1, '\n');
				if( !parser.ParseExpression( buffer1, expr, true ) ) {
					cout << "Error parsing expression: " << buffer1 << endl;
					break;
				}
				expr->SetParentScope( ad );
				if( ad->Flatten( expr, value, fexpr ) ) {
					if( fexpr ) {
						output = "";
						unparser.Unparse( output, fexpr );
						cout << output << endl;
						delete fexpr;
						fexpr = NULL;
					} else {
						output = "";
						unparser.Unparse( output, value );
						cout << output << endl;
					}
				} else {	
					cout << "Error flattening expression\n";
				}
				delete expr;
				expr = NULL;
				break;
				
			case HELP: 
				help(); 
                getline(cin, buffer1, '\n');
				break;

			case INSERT_ATTRIBUTE:
                cin >> buffer1; 
				if( buffer1.length() < 1) {
					cout << "Error reading attribute name\n";
					break;
				}	
                getline(cin, buffer2, '\n');
				if( !parser.ParseExpression( buffer2, expr, true ) ) {
					cout << "Error parsing expression: " <<  buffer2 << endl;
					break;
				} 
				if( !ad->Insert( buffer1, expr ) ) {
					cout << "Error Inserting expression\n";
				}
				expr = NULL;
				break;

			case NEW_TOPLEVEL: 
				ad->Clear();
                getline(cin, buffer1, '\n');
				if( !( ad = parser.ParseClassAd( buffer1, true ) ) ) {
					cout << "Error parsing classad\n";
				}
				break;
			
			case OUTPUT_TOPLEVEL:
				output = "";
				unparser.Unparse( output, ad );
				cout << output << endl;
                getline(cin, buffer1, '\n');
				break;

			case QUIT: 
				cout << "Exiting\n\n";
				exit( 0 );

			case REMOVE_ATTRIBUTE:
                cin >> buffer1;
				if( buffer1.length() < 1) {
					cout << "Error reading attribute name\n";
					break;
				}
				if( !ad->Remove( buffer1 ) ) {
					cout << "Error removing attribute " << buffer1 << endl;
				}
                getline(cin, buffer1, '\n');
				break;

			case SET_MATCH_TOPLEVEL: 
				delete ad;
				if( !( ad = MatchClassAd::MakeMatchClassAd( NULL, NULL ) ) ){
					cout << "Error making classad\n" << endl;
				}
                getline(cin, buffer1, '\n');
				break;

			default:
                getline(cin, buffer1, '\n');
		}

		cout << "\n# ";
	}

	exit( 0 );
}


// Print help information for the hapless user
void 
help( void )
{
	cout << "\nCommands are:\n";
	cout << "clear_toplevel\n\tClear toplevel ad\n";
	cout << "deep_insert <expr1> <name> <expr2>\n\tInsert (<name>,<expr2>) into"
		" classad <expr1>. (No inter-token spaces\n\tallowed in <expr1>.)\n";
	cout << "deep_delete <expr> <name>\n\tDelete <name> from classad "
		"<expr>. (No inter-token spaces allowed\n\tin <expr>.\n";
	cout << "evaluate <expr>\n\tEvaluate <expr> (in toplevel ad)\n" ;
	cout << "sigeval <expr>\n\tEvaluate <expr> (in toplevel) and "
		"identify significant\n\tsubexpressions\n";
	cout << "flatten <expr>\n\tFlatten <expr> (in toplevel ad)\n";
	cout << "help\n\tHelp --- this screen\n" ;
	cout << "insert_attribute <name> <expr>\n\tInsert attribute (<name>,<expr>)"
		"into toplevel\n";
	cout << "new_toplevel <classad>\n\tEnter new toplevel ad\n";
	cout << "output_toplevel\n\tOutput toplevel ad\n";
	cout << "quit\n\tQuit\n";
	cout << "delete_attribute <name>\n\tDelete attribute <name> from "
		"toplevel\n";
	cout << "set_match_toplevel\n\tSetup a toplevel ad for matching\n";
	cout << "\nA command may be specified by an unambiguous prefix\n";
}

int
findCommand( string cmdStr )
{
	int cmd = 0, len = (int)cmdStr.length();

	for( int i = 0 ; i < len ; i++ ) {
		cmdStr[i] = tolower( cmdStr[i] );
	}

	for( int i = _NO_CMD_+1 ; i < _LAST_COMMAND_ ; i++ ) {
		if( strncmp(CommandWords[i].c_str(), cmdStr.c_str(), len) == 0)  {
			if( cmd == 0 ) {
				cmd = i;
			} else if( cmd > 0 ) {
				cmd = -1;
			}
		}
	}

    if (cmd < 0) {
        cout <<  "\nAmbiguous command " << cmdStr << "; matches:\n";
        for( int i = _NO_CMD_+1 ; i < _LAST_COMMAND_ ; i++ ) {
            if( strncmp(CommandWords[i].c_str(), cmdStr.c_str(), len) == 0)  {
                cout << "\t" << CommandWords[i] << "\n";
            }
        }
    } else if( cmd == 0) {
		cout << "\nUnknown command " << cmdStr <<" \n";
		return -1;
	}

	return cmd;
}
