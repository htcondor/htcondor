#include "condor_common.h"
#include "condor_classad.h"

namespace classad {

ClassAd* getOldClassAd( Sock& sock )
{
	ClassAd *ad = new ClassAd( );
	if( !ad ) { 
		return (ClassAd*) 0;
	}
	if( !getOldClassAd( sock, *ad ) ) {
		delete ad;
		return NULL;
	}
	return ad;	
}


bool getOldClassAd( Sock& sock, ClassAd& ad )
{
	Source 	src;
	int 	numExprs;
	char 	*eq;
	ExprTree *expr;
	static  char *buffer = new char[ 10240 ];

	sock.decode( );
	if( !sock.code( numExprs ) ) {
		return false;
	}

	for( int i = 0 ; i < numExprs ; i++ ) {
			// get the expression and find the '=' in the expr
		if( !sock.code( buffer ) || !( eq = strchr( buffer, '=' ) ) ) {
			return false;
		}
			// split the expression at the '='
		*eq = '\0';

			// set the source to the part after the '='
		src.SetSource( eq+1 );

			// parse the expression and insert it into the classad
		if( !src.ParseExpression( expr ) || !ad.Insert( buffer, expr ) ) {
			return false;
		}
	}

	return true;
}


void printClassAdExpr( ExprTree *tree )
{
	static Sink				sink;
	static FormatOptions	fo;

	sink.SetSink( stdout );
	fo.SetClassAdIndentation( );
	fo.SetListIndentation( );
	sink.SetFormatOptions( &fo );
	tree->ToSink( sink );
	sink.FlushSink( );
}


void printClassAdValue( Value &val )
{
	static Sink				sink;
	static FormatOptions	fo;

	sink.SetSink( stdout );
	fo.SetClassAdIndentation( );
	fo.SetListIndentation( );
	sink.SetFormatOptions( &fo );
	val.ToSink( sink );
	sink.FlushSink( );
}

} // namespace classad
