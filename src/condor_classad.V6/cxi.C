// #include "condor_config.h"
#include "classad_package.h"

enum Commands {
	_NO_CMD_,

	CLEAR_TOPLEVEL, 
	DEEP_INSERT_ATTRIBUTE, 
	EVALUATE, 
	EVALUATE_WITH_SIGNIFICANCE,
	FLATTEN, 
	HELP, 
	INSERT_ATTRIBUTE, 
	NEW_TOPLEVEL, 
	OUTPUT_TOPLEVEL, 
	QUIT,
	REMOVE_ATTRIBUTE, 
	SET_CONDOR_TOPLEVEL,

	_LAST_COMMAND_ 
};

char CommandWords[][32] = {
	"",

	"clear_toplevel",
	"deep_insert",
	"evaluate",
	"sigeval",
	"flatten",
	"help",
	"insert_attribute",
	"new_toplevel",
	"output_toplevel",
	"quit",
	"remove_attribute",
	"set_condor_toplevel",

	""	
};

void help( void );
int  findCommand( char* );

int 
main( void )
{
	ClassAd 	*ad, *adptr;
	char 		cmdString[128], buffer1[2048], buffer2[2048];
//	char		*tmp;
	ExprTree	*expr=NULL, *fexpr=NULL;
	int 		command;
	Value		value;
	Source		src;
	Sink		snk;

	snk.setSink( stdout );

//	config( 0 );

	ad = new ClassAd();

	printf( "'h' for help\n# " );
	while( 1 ) {	
		scanf( "%s", cmdString );
		command = findCommand( cmdString );
		switch( command ) {
			case CLEAR_TOPLEVEL: 
				ad->clear();
				fgets( buffer1, 2048, stdin );
				break;

			case DEEP_INSERT_ATTRIBUTE:
				if( scanf( "%s", buffer1 ) != 1 ) {	// expr
					printf( "Error reading primary expression\n" );
					break;
				}
				src.setSource( buffer1, 2048 );
				if( !src.parseExpression( expr, true ) ) {
					printf( "Error parsing expression: %s\n", buffer1 );
					break;
				}
				if( scanf( "%s", buffer1 ) != 1 ) {
					printf( "Error reading attribute name\n" );
					break;
				}	
				fgets( buffer2, 2048, stdin );
				src.setSource( buffer2, 2048 );
				if( !src.parseExpression( fexpr, true ) ) {
					printf( "Error parsing expression: %s\n", buffer2 );
					break;
				} 
				expr->setParentScope( ad );
				ad->evaluateExpr( expr, value );
				if( !value.isClassAdValue( adptr ) ) {
					printf( "Error:  Primary expression was not a classad\n" );
					break;
				}	
				if( !adptr->insert( buffer1, fexpr ) ) {
					printf( "Error inserting expression\n" );
				}
				delete expr;
				adptr = NULL;
				expr  = NULL;
				break;

			case EVALUATE:
				fgets( buffer1, 2048, stdin );
				src.setSource( buffer1, 2048 );
				if( !src.parseExpression( expr, true ) ) {
					printf( "Error parsing expression: %s\n", buffer1 );
					break;
				}
				expr->setParentScope( ad );
				ad->evaluateExpr( expr, value );
				value.toSink( snk );
				snk.flushSink();
				delete expr;
				expr = NULL;
				break;
		
			case EVALUATE_WITH_SIGNIFICANCE:
				fgets( buffer1, 2048, stdin );
				src.setSource( buffer1, 2048 );
				if( !src.parseExpression( expr, true ) ) {
					printf( "Error parsing expression: %s\n", buffer1 );
					break;
				}
				expr->setParentScope( ad );
				ad->evaluateExpr( expr, value, fexpr );
				value.toSink( snk );
				snk.flushSink();
				printf( "\n" );
				fexpr->toSink( snk );
				snk.flushSink();
				delete expr;
				expr = NULL;
				delete fexpr;
				fexpr = NULL;
				break;

			case FLATTEN: 
				fgets( buffer1, 2048, stdin );
				src.setSource( buffer1, 2048 );
				if( !src.parseExpression( expr, true ) ) {
					printf( "Error parsing expression: %s\n", buffer1 );
					break;
				}
				expr->setParentScope( ad );
				if( ad->flatten( expr, value, fexpr ) ) {
					if( fexpr ) {
						fexpr->toSink( snk );
						delete fexpr;
						fexpr = NULL;
					} else {
						value.toSink( snk );
					}
					snk.flushSink();
				} else {	
					printf( "Error flattening expression\n" );
				}
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
				src.setSource( buffer2, 2048 );
				if( !src.parseExpression( expr, true ) ) {
					printf( "Error parsing expression: %s\n", buffer2 );
					break;
				} 
				if( !ad->insert( buffer1, expr ) ) {
					printf( "Error inserting expression\n" );
				}
				expr = NULL;
				break;

			case NEW_TOPLEVEL: 
				ad->clear();
				fgets( buffer1, 2048, stdin );
				src.setSource( buffer1, 2048 );
				if( !src.parseClassAd( ad, true ) ) {
					printf( "Error parsing classad\n" );
				}
				break;
			
			case OUTPUT_TOPLEVEL:
				ad->toSink( snk );
				snk.flushSink();
				fgets( buffer1, 2048, stdin );
				break;

/*
			case 'p':
				fgets( buffer1, 2048, stdin );
				if( !( tmp = param( buffer1 ) ) ) {
					printf( "Did not find %s in config file\n", buffer1 );
					break;
				}
				src.setSource( tmp, strlen( tmp ) );
				if( !src.parseExpression( expr, true ) ) {
					printf( "Error parsing expression: %s\n", tmp );
					free( tmp );
					break;
				}
				if( !ad->insert( buffer1, expr ) ) {
					printf( "Error inserting: %s = %s\n", buffer1, tmp );
				}
				delete tmp;
				expr = NULL;
				break;
*/
				
			case QUIT: 
				printf( "Done\n\n" );
				exit( 0 );

			case REMOVE_ATTRIBUTE:
				if( scanf( "%s", buffer1 ) != 1 ) {
					printf( "Error reading attribute name\n" );
					break;
				}
				if( !ad->remove( buffer1 ) ) {
					printf( "Error removing attribute %s\n", buffer1 );
				}
				fgets( buffer1, 2048, stdin );
				break;

			case SET_CONDOR_TOPLEVEL: 
				delete ad;
				ad = CondorClassAd::makeCondorClassAd( NULL, NULL );
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
	printf( "\tc\t\t- (C)lear toplevel ad\n" );
	printf( "\td <e1><name><e2>- (D)eep insert e2 into classad e1\n");
	printf( "\te <expr>\t- (E)valuate expression (in toplevel ad)\n" );	
	printf( "\tf <expr>\t- (F)latten expression (in toplevel ad)\n" );
	printf( "\th\t\t- (H)elp; This screen\n" );
	printf( "\ti <name><expr>\t- (I)nsert expression into toplevel ad\n" );
	printf( "\tn <classad>\t- Enter (n)ew toplevel ad\n" );
	printf( "\to\t\t- (O)utput toplevel ad\n" );
/*
	printf( "\tp <name>\t- (P)aram from config file and insert in toplevel\n" );
*/
	printf( "\tq\t\t- (Q)uit\n" );
	printf( "\ts\t\t- (S)etup Condor toplevel ad\n" );
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
	for( int i = _NO_CMD_+1 ; i < _LAST_COMMAND_-1 ; i++ ) {
		if( strncmp( cmdStr, CommandWords[i], len ) == 0 ) {
			printf( "\t%s\n", CommandWords[i] );
		}
	}
	return -1;
}
