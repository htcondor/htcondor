#include <iostream.h>
#include "exprTree.h"


int main( void )
{
	Source		src;
	Sink		snk;
	Value		val;
	ExprTree	*tree, *ftree;
	
	src.setSource( stdin );
	snk.setSink ( stdout );

	if( !src.parseExpression( tree, true ) ) {
		cerr << "Parse error" << endl;
		exit( 1 );
	} 

	cout << endl << "---" << endl;
	tree->toSink( snk );
	snk.flushSink();
	cout << endl << "---" << endl;

	if( !tree->flatten( val, ftree ) ) {
		cerr << "Failed to flatten" << endl;
	}

	if( ftree ) {
		ftree->toSink( snk );
	} else {
		val.toSink( snk );
	}

	snk.flushSink();
	cout << endl << endl;
		
	val.clear();
	delete tree;
	delete ftree;

	return 0;
}
	
