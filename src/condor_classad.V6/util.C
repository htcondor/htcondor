#include "condor_common.h"
#include "condor_classad.h"

BEGIN_NAMESPACE( classad )

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
	char 	*eq=NULL;
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

	// Get my type and target type
	if (!sock.code(buffer) || !ad.Insert("MyType",buffer)) return false;
	if (!sock.code(buffer) || !ad.Insert("TargetType",buffer)) return false;

	return true;
}

bool putOldClassAd ( Sock& sock, ClassAd& ad )
{
	char* attr;
	ExprTree* expr;

	char buffer[10240];
	char tmp[10240];
	char* tmpPtr=tmp;

	int numExprs=0;
	ClassAdIterator itor(ad);
	while (itor.NextAttribute(attr, expr)) {
		if (strcmp(attr,"MyType")==0 || strcmp(attr,"TargetType")==0) continue;
		numExprs++;
	}
	
	sock.encode( );
//printf("Sending: %d\n",numExprs);
	if( !sock.code( numExprs ) ) {
		return false;
	}
		
	itor.ToBeforeFirst();
	while (itor.NextAttribute(attr, expr)) {
		if (strcmp(attr,"MyType")==0 || strcmp(attr,"TargetType")==0) continue;
		memset(buffer,0,sizeof(buffer));
		Sink s;
		s.SetSink(buffer,sizeof(buffer));
		expr->ToSink(s);
		sprintf(tmp,"%s = %s",attr,buffer);
//printf("Sending: %s\n",tmpPtr);
		if (!sock.code(tmpPtr)) {
			return false;
		}
	}

	// Send the type
	char* type=NULL;
	if (!ad.EvaluateAttrString("MyType",type)) type="(unknown type)";
//printf("Sending: %s\n",type);
	if (!sock.code(type)) {
		return false;
	}

	char* target_type=NULL;
	if (!ad.EvaluateAttrString("TargetType",target_type)) target_type="(unknown type)";
//printf("Sending: %s\n",target_type);
	if (!sock.code(target_type)) {
		return false;
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
	sink.Terminate( );
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
	sink.Terminate( );
	sink.FlushSink( );
}

END_NAMESPACE // classad
