#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_attributes.h"
#include "condor_classad.h"

int
main(int /* argc */, char ** /* argv */) {
    classad::ClassAd c;
    c.Assign( "PresentInt", 6 );
    c.Assign( "PresentString", "original value" );
    fPrintAd( stdout, c );
    fprintf( stdout, "\n" );

    int i = c[{"PresentInt", 7}];
    fprintf( stdout, "PresentInt: %d\n", i );
    // Impossible, and rejected by the compiler.  Yay!
    // c[{"PresentInt", 7}] = 8;

    // This throws std::bad_alloc() if operator [] is declared with a
    // pair whose second member is a const std::string &.
    std::string s = c[{"PresentString", "default value"}];
    fprintf( stdout, "PresentString: %s\n", s.c_str() );
    // Impossible, and rejected by the compiler.  Yay!
    // c[{"PresentString", "value"}] = "new-value";

    int j = c[{"AbsentInt", 7}];
    fprintf( stdout, "AbsentInt: %d\n", j );

    std::string t = c[{"AbsentStr", "default value"}];
    fprintf( stdout, "AbsentStr: %s\n", t.c_str() );

	int k = c["PresentInt"_ca_int];
	fprintf( stdout, "PresentInt [UDL]: %d\n", k );

	std::string x = c["PresentString"_ca_str];
	fprintf( stdout, "PresentStr [UDL]: %s\n", x.c_str() );

	bool success = false;
	try {
		int l = c["PresentString"_ca_int];
	} catch( std::out_of_range & oor ) {
		success = true;
	}
	if(success) { fprintf( stdout, "Correctly detected wrong type!\n" ); }
	ASSERT(success);

	success = false;
	try {
		std::string y = c["PresentInt"_ca_str];
	} catch( std::out_of_range & oor ) {
		success = true;
	}
	if(success) { fprintf( stdout, "Correctly detected wrong type!\n" ); }
	ASSERT(success);

    return 0;
}
