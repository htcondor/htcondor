#include <stdio.h>
#include "condor_common.h"
#include "exprTree.h"

void help( void );

int 
main( void )
{
	ClassAd 	ad;
	char 		buffer1[2048], buffer2[2048];
	ExprTree	*expr=NULL, *fexpr=NULL;
	int 		command;
	Value		value;
	Source		src;
	Sink		snk;

	snk.setSink( stdout );

	printf( "'h' for help\n# " );
	while( 1 ) {	
		command = tolower( getchar() );
		switch( command ) {
			case 'c': 
				ad.clear();
				break;

			case 'd':
				if( scanf( "%s", buffer1 ) != 1 ) {
					printf( "Error reading attribute name\n" );
					break;
				}
				if( !ad.remove( buffer1 ) ) {
					printf( "Error removing attribute %s\n", buffer1 );
				}
				break;

			case 'e':
				fgets( buffer1, 2048, stdin );
				src.setSource( buffer1, 2048 );
				if( !src.parseExpression( expr ) ) {
					printf( "Error parsing expression: %s\n", buffer1 );
					break;
				}
				ad.evaluate( expr, value );
				value.toSink( snk );
				snk.flushSink();
				delete expr;
				expr = NULL;
				break;
		
			case 'f': 
				fgets( buffer1, 2048, stdin );
				src.setSource( buffer1, 2048 );
				if( !src.parseExpression( expr ) ) {
					printf( "Error parsing expression: %s\n", buffer1 );
					break;
				}
				if( ad.flatten( expr, value, fexpr ) ) {
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
				
			case 'h': 
				help(); 
				break;

			case 'i':
				if( scanf( "%s", buffer1 ) != 1 ) {	// name
					printf( "Error reading attribute name\n" );
					break;
				}
				fgets( buffer2, 2048, stdin );		// expr
				src.setSource( buffer2, 2048 );
				if( !src.parseExpression( expr ) ) {
					printf( "Error parsing expression: %s\n", buffer2 );
				} 
				if( !ad.insert( buffer1, expr ) ) {
					printf( "Error inserting expression\n" );
				}
				break;

			case 'n': 
				ad.clear();
				fgets( buffer1, 2048, stdin );
				src.setSource( buffer1, 2048 );
				if( !src.parseClassAd( &ad ) ) {
					printf( "Error parsing classad\n" );
				}
				break;
			
			case 'p':
				ad.toSink( snk );
				snk.flushSink();
				break;

			case 'q': 
				printf( "Done\n\n" );
				exit( 0 );

			case 's': 
				ad.clear();
				sprintf( buffer1, "[adc1=[self=.adc1.ad;other=.adc2.ad;ad=[]];"
								  " adc2=[self=.adc2.ad;other=.adc1.ad;ad=[]]]"
						);
				src.setSource( buffer1, 2048 );
				ad.clear();
				if( !src.parseClassAd( &ad ) ) {
					printf( "Error parsing classad: %s\n", buffer1 );
				}
				break;

			default:
				printf( "Unknown command: '%c' -- Ignoring input\n", command );
				fflush( stdin );
		}

		fflush( stdin );
		printf( "\n# " );
	}

	exit( 0 );
}


void 
help( void )
{
	printf( "\nCommands are:\n" );
	printf( "\tc\t\t- (C)lear toplevel ad\n" );
	printf( "\td <name>\t- (D)elete expression from ad\n" );
	printf( "\te <expr>\t- (E)valuate expression (in toplevel ad)\n" );	
	printf( "\tf <expr>\t- (F)latten expression (in toplevel ad)\n" );
	printf( "\th\t\t- (H)elp; This screen\n" );
	printf( "\ti <name> <expr>\t- (I)nsert expression into toplevel ad\n" );
	printf( "\tn <classad>\t- Enter (n)ew toplevel ad\n" );
	printf( "\to\t\t- Output toplevel ad\n" );
	printf( "\tq\t\t- (Q)uit\n" );
	printf( "\ts\t\t- (S)etup Condor toplevel ad\n" );
}	
