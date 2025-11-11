// Required to be a daemon core daemon.
#include "condor_common.h"
#include "condor_daemon_core.h"
#include "subsystem_info.h"

// For dprintf().
#include "condor_debug.h"

#include <stdio.h>


void
main_init( int, char * [] ) {
    dprintf( D_ALWAYS, "main_init()\n" );

    int pipe_ends[2];
    const bool CAN_REGISTER_READ = true;
    int rv = daemonCore->Create_Pipe( pipe_ends, CAN_REGISTER_READ );
    if( rv != TRUE ) {
        dprintf( D_ALWAYS, "Unable to Create_Pipe(), aborting.\n" );
        DC_Exit(1);
    }

    int read_end = pipe_ends[0];
    int write_end = pipe_ends[1];


    daemonCore->Register_Timer( 0, 1,
        [=](int /* timerID */) {
            static unsigned char v[20] = {
                0x07, 0x04, 0x03, 0x71, 0x22,
                0x70, 0x61, 0x55, 0x24, 0xAA,
                0xBC, 0xAD, 0xFF, 0xEE, 0xCB,
                0x07, 0x04, 0x03, 0x71, 0x22
            };
            static int count = 0;
            if( count >= 10 ) { return; }
            daemonCore->Write_Pipe( write_end, (void *)(v + (2 * count)), 2 );
            ++count;
        },
        "write-to-pipe"
    );

    daemonCore->Register_Pipe( read_end, "the test pipe",
        [=](int /* pipeID? */) {
            static int count = 0;
            static unsigned char v[20];
            daemonCore->Read_Pipe( read_end, (void *)(v + count), 1 );
            ++count;

            std::string the_bytes;
            for( int i = 0; i < count; ++i ) {
                formatstr_cat( the_bytes, " 0x%x", v[i] );
            }
            dprintf( D_ALWAYS, "Read%s so far.\n", the_bytes.c_str() );

            return 0;
        },
        "read-from-pipe", HANDLE_READ
    );

    daemonCore->Register_Timer( 30, 0,
        [=](int /* timerID */) {
            dprintf( D_ALWAYS, "Done!\n" );
            DC_Exit(0);
        },
        "die-gloriously"
    );

}


void
main_config() {
    dprintf( D_ALWAYS, "main_config()\n" );
}


void
main_shutdown_fast() {
    dprintf( D_ALWAYS, "main_shutdown_fast()\n" );
    DC_Exit( 0 );
}


void
main_shutdown_graceful() {
    dprintf( D_ALWAYS, "main_shutdown_graceful()\n" );
    DC_Exit( 0 );
}


int
main( int argc, char * argv [] ) {
    set_mySubSystem( "TPIPEH", true, SUBSYSTEM_TYPE_DAEMON );

    dc_main_init = main_init;
    dc_main_config = main_config;
    dc_main_shutdown_fast = main_shutdown_fast;
    dc_main_shutdown_graceful = main_shutdown_graceful;

    return dc_main( argc, argv );
}
