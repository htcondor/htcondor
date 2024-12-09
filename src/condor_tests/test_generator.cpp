#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"

#include "condor_daemon_core.h"
#include "subsystem_info.h"

#include "dc_coroutines.h"
using namespace condor;


cr::Piperator<int, int>
test_piperator_fn(int request) {
    for( int i = 0; i < 3; ++i ) {
        int guidance = request + 7;
        request = co_yield guidance;
    }
    co_return request - 7;
}


bool
test_piperator_simple() {
    std::vector<int>    input( {0,  5,  4,  8} );
    std::vector<int> expected( {7, 12, 11, 1} );
    std::vector<int>   output;

    auto the_piperator = test_piperator_fn(input[0]);

    for( size_t i = 1; i <= input.size(); ++i ) {
        output.push_back(the_piperator());
        the_piperator.handle.promise().response = input[i];
    }


    if( expected == output ) { return true; }

    for( size_t i = 0; i < output.size(); ++i ) {
        fprintf( stderr, "output %d, expected %d\n", output[i], expected[i] );
    }
    return false;
}


bool
test_piperator_stop() {
    std::vector<int>    input( {0,  5,  4,  8} );
    std::vector<int> expected( {7, 12, 11, 1} );
    std::vector<int>   output;

    auto the_piperator = test_piperator_fn(input[0]);

    int i = 1;
    do {
        output.push_back(the_piperator());
        the_piperator.handle.promise().response = input[i++];
    } while(! the_piperator.handle.done());

    if( expected == output ) { return true; }

    for( size_t i = 0; i < output.size(); ++i ) {
        fprintf( stderr, "output %d, expected %d\n", output[i], expected[i] );
    }
    return false;
}


int
test_fn_one( bool & last_value ) {
    static bool in_conversation = false;
    static cr::Piperator<int, int> the_coroutine;

    int result = -1;
    if(! in_conversation) {
        in_conversation = true;
        the_coroutine = std::move(test_piperator_fn(1));
        result = the_coroutine();
    } else {
        the_coroutine.handle.promise().response = 16;
        result = the_coroutine();
    }

    if( the_coroutine.handle.done() ) {
        last_value = true;
        in_conversation = false;
    }

    return result;
}


int
test_syscall_handler_mode() {
    std::vector<int> expected( {
        8, 23, 23, 9,
        8, 23, 23, 9
    });
    std::vector<bool> expected_last_value( {
        false, false, false, true,
        false, false, false, true
    });

    bool last_value;
    std::vector<int> output;
    for( size_t i = 0; i < expected.size(); ++i ) {
        last_value = false;
        output.push_back( test_fn_one(last_value) );
        if( last_value != expected_last_value[i] ) {
            fprintf( stderr, "last value-p %d, expected %d\n", last_value, (int)expected_last_value[i] );
            return false;
        }
    }

    if( expected == output ) { return true; }

    for( size_t i = 0; i < output.size(); ++i ) {
        fprintf( stderr, "output %d, expected %d\n", output[i], expected[i] );
    }
    return false;
}


int main( int /* argc */, char ** /* argv */ ) {
    if(! test_piperator_simple()) {
        fprintf( stderr, "FAILED test_piperator_simple()\n" );
        return 1;
    }

    if(! test_piperator_stop()) {
        fprintf( stderr, "FAILED test_piperator_stop()\n" );
        return 1;
    }

    if(! test_syscall_handler_mode()) {
        fprintf( stderr, "FAILED test_syscall_handler_mode()\n" );
        return 1;
    }

    fprintf( stdout, "PASSED all tests\n" );
    return 0;
}
