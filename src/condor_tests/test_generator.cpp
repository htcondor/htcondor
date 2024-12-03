#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"

#include "condor_daemon_core.h"
#include "subsystem_info.h"

#include "dc_coroutines.h"
using namespace condor;


cr::Piperator<bool, int>
test_function(int i) {
    for( ; i >= 0; --i ) {
        int i = co_yield true;
        fprintf( stderr, "co_yield() = %d\n", i );
    }
    int j = co_yield false;
    fprintf( stderr, "co_yield() = %d\n", j );
}


int main( int /* argc */, char ** /* argv */ ) {
    int i = 7;
    auto generator = test_function(3);
    generator.handle.promise().response = i++;
    while( generator() ) {
        fprintf( stderr, "generator() returned true\n" );
        generator.handle.promise().response = i++;
    }

    return 0;
}
