#include <iostream.h>
#include "exprTree.h"


int main( void )
{
	Source		src;
	Sink		snk;
	ClassAd		ad;
	char		buf[32];
	Value		val;
	
	src.setSource( stdin );
	snk.setSink ( stdout );

	if( !src.parseClassAd( &ad ) ) 
		cerr << "Parse error" << endl;
	else
		cerr << "Parsed ok" << endl;

	scanf( "%s" , buf );
	while( strcmp( buf , "done" ) != 0 ) {
		if( !ad.evaluate( buf , val ) ) {
			cerr << "Parse error of " << buf << endl << endl;
		} else {
			val.toSink( snk );
			printf( "\n\n" );
		}
		scanf( "%s" , buf );
		val.clear();
	}

	return 0;
}
	
