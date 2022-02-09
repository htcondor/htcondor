#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_attributes.h"
#include "condor_classad.h"

int
main(int /* argc */, char ** /* argv */) {
    classad::ClassAd c;

    int i = c[{"PresentInt", 7}];
    fprintf( stdout, "PresentInt: %d\n", i );
    // Impossible, and rejected by the compiler.  Yay!
    // c[{"PresentInt", 7}] = 6;

    std::string s = c[{"PresentString", "value"}];
    fprintf( stdout, "PresentString: %s\n", s.c_str() );
    // Impossible, and rejected by the compiler.  Yay!
    // c[{"PresentString", "value"}] = "new-value";

    return 0;
}
