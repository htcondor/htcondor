#include "condor_common.h"
#include "condor_classad.h"

ClassAd a;
ClassAd b;

extern "C" int SetSyscalls() { return 0; }

int main (void)
{
	EvalResult 	r;
	ExprTree   	*t;
	char		buffer[1024];
	char		buf[2048];

	while( strcmp( gets( buffer ) , "done" ) != 0 ) {
		buf[0] = '\0';
		if (Parse (buffer, t)) {
			cerr << "Parse error" << endl;
			exit( 1 );
		}
		t->PrintToStr( buf );
		puts( buf );
		t->EvalTree((ClassAd*)NULL,(ClassAd*)NULL,&r);
		r.fPrintResult(stdout);
		printf("\n");
	}
		
	return 0;
}
