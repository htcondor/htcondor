#ifdef IN_CONDOR
#include "condor_common.h"
#include "condor_config.h"
#include "condor_classad.h"
#else
#include "classad_package.h"
#include <ctype.h>
#endif

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
#ifdef IN_CONDOR
	PARAM,
#endif
	QUIT,
	REMOVE_ATTRIBUTE, 
	SET_MATCH_TOPLEVEL,

	_LAST_COMMAND_ 
};

char CommandWords[][32] = {
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
#ifdef IN_CONDOR
	"param",
#endif
	"quit",
	"delete_attribute",
	"set_match_toplevel",

	""	
};

void help( void );
int  findCommand( char* );

int 
main( void )
{
	ClassAd 	*ad, *adptr;
	char 		cmdString[128], buffer1[2048], buffer2[2048], buffer3[2048];
#ifdef IN_CONDOR
	char		*paramStr, *attrName;
#endif
	ExprTree	*expr=NULL, *fexpr=NULL;
	int 		command;
	Value		value;
	Source		src;
	Sink		snk;
	FormatOptions	pp;

		// set formatter options
	pp.SetClassAdIndentation( true , 4 );
	pp.SetListIndentation( true , 4 );
	pp.SetMinimalParentheses( true );

	snk.SetSink( stdout );
	snk.SetFormatOptions( &pp );

#ifdef IN_CONDOR
	config( 0 );
#endif

	ad = new ClassAd();

	printf( "'h' for help\n# " );
	while( 1 ) {	
		scanf( "%s", cmdString );
		command = findCommand( cmdString );
		switch( command ) {
			case CLEAR_TOPLEVEL: 
				ad->Clear();
				fgets( buffer1, 2048, stdin );
				break;

			case DEEP_INSERT_ATTRIBUTE:
				if( scanf( "%s", buffer1 ) != 1 ) {	// expr
					printf( "Error reading primary expression\n" );
					break;
				}
				src.SetSource( buffer1, 2048 );
				if( !src.ParseExpression( expr, true ) ) {
					printf( "Error parsing expression: %s\n", buffer1 );
					break;
				}
				if( scanf( "%s", buffer2 ) != 1 ) {
					printf( "Error reading attribute name\n" );
					break;
				}	
				fgets( buffer3, 2048, stdin );
				src.SetSource( buffer3, 2048 );
				if( !src.ParseExpression( fexpr, true ) ) {
					printf( "Error parsing expression: %s\n", buffer3 );
					break;
				} 
				expr->SetParentScope( ad );
				if( !ad->EvaluateExpr( expr, value ) ) {
					printf( "Error evaluating expression: %s\n", buffer1 );
					delete expr;
					delete fexpr;
					expr = NULL;
					fexpr = NULL;
					break;
				}
				if( !value.IsClassAdValue( adptr ) ) {
					printf( "Error:  Primary expression was not a classad\n" );
					delete expr;
					expr = NULL;
					delete fexpr;
					fexpr = NULL;
					break;
				}	
				if( !adptr->Insert( buffer2, fexpr ) ) {
					printf( "Error Inserting expression: %s\n", buffer3 );
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
				if( scanf( "%s", buffer1 ) != 1 ) { // expr
					printf( "Error reading primary expression\n" );
					break;
				}
				src.SetSource( buffer1, 2048 );
				if( !src.ParseExpression( expr, true ) ) {
					printf( "Error parsing expression: %s\n", buffer1 );
					break;
				}
				fgets( buffer2, 2048, stdin ); // name
				expr->SetParentScope( ad );
				if( !ad->EvaluateExpr( expr, value ) ) {
					printf( "Error evaluating expression: %s\n", buffer1 );
					delete expr;
					break;
				}
				if( !value.IsClassAdValue( adptr ) ) {
					printf( "Error:  Primary expression was not a classad\n" );
					delete expr;
					expr = NULL;
					break;
				}
				if( !adptr->Remove( buffer2 ) ) {
					printf( "Warning:  Attribute %s not found\n", buffer2 );
				}
				delete expr;
				expr = NULL;
				break;

			case EVALUATE:
				fgets( buffer1, 2048, stdin );
				src.SetSource( buffer1, 2048 );
				if( !src.ParseExpression( expr, true ) ) {
					printf( "Error parsing expression: %s\n", buffer1 );
					break;
				}
				expr->SetParentScope( ad );
				if( !ad->EvaluateExpr( expr, value ) ) {
					printf( "Error evaluating expression: %s\n", buffer1 );
					delete expr;
					expr = NULL;
					break;
				}
				if( !value.ToSink( snk  ) ) {
					printf( "Error writing evaluation result to sink\n" );
				}
				snk.FlushSink();
				delete expr;
				expr = NULL;
				break;
		
			case EVALUATE_WITH_SIGNIFICANCE:
				fgets( buffer1, 2048, stdin );
				src.SetSource( buffer1, 2048 );
				if( !src.ParseExpression( expr, true ) ) {
					printf( "Error parsing expression: %s\n", buffer1 );
					break;
				}
				expr->SetParentScope( ad );
				if( !ad->EvaluateExpr( expr, value, fexpr ) ) {
					printf( "Error evaluating expression: %s\n", buffer1 );
					delete expr;
					delete fexpr;
					expr = NULL;
					fexpr = NULL;
					break;
				}
				if( !value.ToSink( snk ) ) {
					printf( "Error writing result to sink\n" );
				}
				snk.FlushSink();
				printf( "\n" );
				if( !fexpr->ToSink( snk ) ) {
					printf( "Error writing expression to sink\n" );
				}
				snk.FlushSink();
				delete expr;
				expr = NULL;
				delete fexpr;
				fexpr = NULL;
				break;

			case FLATTEN: 
				fgets( buffer1, 2048, stdin );
				src.SetSource( buffer1, 2048 );
				if( !src.ParseExpression( expr, true ) ) {
					printf( "Error parsing expression: %s\n", buffer1 );
					break;
				}
				expr->SetParentScope( ad );
				if( ad->Flatten( expr, value, fexpr ) ) {
					if( fexpr ) {
						if( !fexpr->ToSink( snk ) ) {
							printf( "Error writing flattened expression to "
								"sink\n" );
						}
						delete fexpr;
						fexpr = NULL;
					} else {
						if( !value.ToSink( snk ) ) {
							printf( "Error writing value to sink\n" );
						}
					}
					snk.FlushSink();
				} else {	
					printf( "Error flattening expression\n" );
				}
				delete expr;
				expr = NULL;
				break;
				
			case HELP: 
				help(); 
				fgets( buffer1, 2048, stdin );
				break;

			case INSERT_ATTRIBUTE:
				if( scanf( "%s", buffer1 ) != 1 ) {
					printf( "Error reading attribute name\n" );
					break;
				}	
				fgets( buffer2, 2048, stdin );
				src.SetSource( buffer2, 2048 );
				if( !src.ParseExpression( expr, true ) ) {
					printf( "Error parsing expression: %s\n", buffer2 );
					break;
				} 
				if( !ad->Insert( buffer1, expr ) ) {
					printf( "Error Inserting expression\n" );
				}
				expr = NULL;
				break;

			case NEW_TOPLEVEL: 
				ad->Clear();
				fgets( buffer1, 2048, stdin );
				src.SetSource( buffer1, 2048 );
				if( !src.ParseClassAd( ad, true ) ) {
					printf( "Error parsing classad\n" );
				}
				break;
			
			case OUTPUT_TOPLEVEL:
				if( !ad->ToSink( snk ) ) {
					printf( "Error writing to sink\n" );
				}
				snk.FlushSink();
				fgets( buffer1, 2048, stdin );
				break;

#ifdef IN_CONDOR
			case PARAM:
				fgets( buffer1, 2048, stdin );
				attrName = strtok( buffer1, " \t\n" );
				if( !( paramStr = param( attrName ) ) ) {
					printf( "Did not find %s in config file\n", buffer1 );
					break;
				}
				src.SetSource( paramStr, strlen( paramStr ) );
				if( !src.ParseExpression( expr, true ) ) {
					printf( "Error parsing expression: %s\n", paramStr );
					free( paramStr );
					break;
				}
				if( !ad->Insert( buffer1, expr ) ) {
					printf( "Error Inserting: %s = %s\n", buffer1, paramStr );
				}
				free( paramStr );
				expr = NULL;
				break;
#endif
				
			case QUIT: 
				printf( "Done\n\n" );
				exit( 0 );

			case REMOVE_ATTRIBUTE:
				if( scanf( "%s", buffer1 ) != 1 ) {
					printf( "Error reading attribute name\n" );
					break;
				}
				if( !ad->Remove( buffer1 ) ) {
					printf( "Error removing attribute %s\n", buffer1 );
				}
				fgets( buffer1, 2048, stdin );
				break;

			case SET_MATCH_TOPLEVEL: 
				delete ad;
				if( !( ad = MatchClassAd::MakeMatchClassAd( NULL, NULL ) ) ){
					printf( "Error making classad\n" );
				}
				fgets( buffer1, 2048, stdin );
				break;

			default:
				fflush( stdin );
		}

		printf( "\n# " );
	}

	exit( 0 );
}


// Print help information for the hapless user
void 
help( void )
{
	printf( "\nCommands are:\n" );
	printf( "clear_toplevel\n\tClear toplevel ad\n\n" );
	printf( "deep_insert <expr1> <name> <expr2>\n\tInsert (<name>,<expr2>) into"
		" classad <expr1>. (No inter-token spaces\n\tallowed in <expr1>.)\n\n");
	printf( "deep_delete <expr> <name>\n\tDelete <name> from classad "
		"<expr>. (No inter-token spaces allowed\n\tin <expr>.\n\n" );
	printf( "evaluate <expr>\n\tEvaluate <expr> (in toplevel ad)\n\n" );	
	printf( "sigeval <expr>\n\tEvaluate <expr> (in toplevel) and "
		"identify significant\n\tsubexpressions\n\n" );
	printf( "flatten <expr>\n\tFlatten <expr> (in toplevel ad)\n\n" );
	printf( "help\n\tHelp --- this screen\n\n" );
	printf( "insert_attribute <name> <expr>\n\tInsert attribute (<name>,<expr>)"
		"into toplevel\n\n" );
	printf( "new_toplevel <classad>\n\tEnter new toplevel ad\n\n" );
	printf( "output_toplevel\n\tOutput toplevel ad\n\n" );
#ifdef IN_CONDOR
	printf("param <name>\n\tParam from config file and insert in toplevel\n\n");
#endif
	printf( "quit\n\tQuit\n\n" );
	printf( "delete_attribute <name>\n\tDelete attribute <name> from "
		"toplevel\n\n" );
	printf( "set_condor_toplevel\n\tSetup a toplevel ad for matching\n\n" );
	printf( "\nA command may be specified by an unambiguous prefix\n\n" );
}

int
findCommand( char *cmdStr )
{
	int cmd = 0, len = strlen( cmdStr );

	for( int i = 0 ; i < len ; i++ ) {
		cmdStr[i] = tolower( cmdStr[i] );
	}

	for( int i = _NO_CMD_+1 ; i < _LAST_COMMAND_ ; i++ ) {
		if( strncmp( cmdStr, CommandWords[i], len ) == 0 ) {
			if( cmd == 0 ) {
				cmd = i;
			} else if( cmd > 0 ) {
				cmd = -1;
			}
		}
	}

	if( cmd > 0 ) return cmd;

	if( cmd == 0 ) {
		printf( "\nUnknown command %s\n", cmdStr );
		return -1;
	}

	printf( "\nAmbiguous command %s; matches\n", cmdStr );
	for( int i = _NO_CMD_+1 ; i < _LAST_COMMAND_ ; i++ ) {
		if( strncmp( cmdStr, CommandWords[i], len ) == 0 ) {
			printf( "\t%s\n", CommandWords[i] );
		}
	}
	return -1;
}
