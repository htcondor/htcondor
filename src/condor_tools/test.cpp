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

    std::string s = c[{"PresentString", "value"}];
    fprintf( stdout, "PresentString: %s\n", s.c_str() );

    return 0;
}
