#include <stdio.h>
#include "condor_common.h"
/*
#include "condor_config.h"
*/
#include "classad_package.h"

void help( void );

int 
main( void )
{
	ClassAd 	ad, *adptr;
	char 		buffer1[2048], buffer2[2048];
//	char		*tmp;
	ExprTree	*expr=NULL, *fexpr=NULL;
	int 		command;
	Value		value;
	Source		src;
	Sink		snk;

	snk.setSink( stdout );
/*
	config( 0 );
*/
	printf( "'h' for help\n# " );
	while( 1 ) {	
		command = tolower( getchar() );
		switch( command ) {
			case 'c': 
				ad.clear();
				break;

			case 'd':
				if( scanf( "%s", buffer1 ) != 1 ) {	// expr
					printf( "Error reading primary expression\n" );
					break;
				}
				src.setSource( buffer1, 2048 );
				if( !src.parseExpression( expr ) ) {
					printf( "Error parsing expression: %s\n", buffer1 );
					break;
				}
				if( scanf( "%s", buffer1 ) != 1 ) {
					printf( "Error reading attribute name\n" );
					break;
				}	
				fgets( buffer2, 2048, stdin );
				src.setSource( buffer2, 2048 );
				if( !src.parseExpression( fexpr ) ) {
					printf( "Error parsing expression: %s\n", buffer2 );
					break;
				} 
				ad.evaluate( expr, value );
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
				if( scanf( "%s", buffer1 ) != 1 ) {
					printf( "Error reading attribute name\n" );
					break;
				}	
				fgets( buffer2, 2048, stdin );
				src.setSource( buffer2, 2048 );
				if( !src.parseExpression( fexpr ) ) {
					printf( "Error parsing expression: %s\n", buffer2 );
					break;
				} 
				if( ad.insert( buffer1, expr ) ) {
					printf( "Error inserting expression\n" );
				}
				expr  = NULL;
				break;

			case 'n': 
				ad.clear();
				fgets( buffer1, 2048, stdin );
				src.setSource( buffer1, 2048 );
				if( !src.parseClassAd( &ad ) ) {
					printf( "Error parsing classad\n" );
				}
				break;
			
			case 'o':
				ad.toSink( snk );
				snk.flushSink();
				break;

/*
			case 'p':
				fgets( buffer1, 2048, stdin );
				if( !( tmp = param( buffer1 ) ) ) {
					printf( "Did not find %s in config file\n", buffer1 );
					break;
				}
				src.setSource( tmp, strlen( tmp ) );
				if( !src.parseExpression( expr ) ) {
					printf( "Error parsing expression: %s\n", tmp );
					free( tmp );
					break;
				}
				if( !ad.insert( buffer1, expr ) ) {
					printf( "Error inserting: %s = %s\n", buffer1, tmp );
				}
				delete tmp;
				expr = NULL;
				break;
*/
				
			case 'q': 
				printf( "Done\n\n" );
				exit( 0 );

			case 'r':
				if( scanf( "%s", buffer1 ) != 1 ) {
					printf( "Error reading attribute name\n" );
					break;
				}
				if( !ad.remove( buffer1 ) ) {
					printf( "Error removing attribute %s\n", buffer1 );
				}
				break;

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
