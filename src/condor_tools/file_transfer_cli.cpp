#include "condor_common.h"
#include "condor_debug.h"

#include "condor_daemon_core.h"
#include "subsystem_info.h"

#include <string>
#include <memory>
#include "reli_sock.h"
#include "compat_classad.h"
#include "file_transfer_constants.h"
#include "file_transfer_functions.h"
#include "file_transfer_commands.h"
#include "dc_coroutines.h"
#include "file_transfer_utils.h"

#include "dc_coroutines.h"
#include "basename.h"
#include "condor_getcwd.h"

std::string transferKey;
std::string source;
std::string destination;

// wtaf is daemon core doing?
std::string _cwd;


int
shadow_input_command_handler( int command, Stream * s ) {
    assert( s != NULL );
    ReliSock * sock = dynamic_cast<ReliSock *>(s);
    assert( sock != NULL );

    std::string peerTransferKey;
    FileTransferFunctions::receiveTransferKey( sock, peerTransferKey );
    assert( transferKey == peerTransferKey );

    assert( command == FILETRANS_UPLOAD );


    //
    // Decide what we're going to transfer.
    //
    std::map<std::string, std::string> entries;
    entries[source] = destination;

    //
    // Transfer it.
    //
    chdir( _cwd.c_str() );
    bool exit_when_done = false;
    FileTransferUtils::sendFilesToPeer( sock, entries, exit_when_done );


    return KEEP_STREAM;
}

void
do_shadow_input( const char * src, const char * dest ) {
    fprintf( stderr, "do_shadow_input(): begins\n" );

    // Note the globals; sigh.
    FileTransferFunctions::generateTransferKey( transferKey );
    source = src;
    destination = dest;

    fprintf( stdout,
        "COMMAND LINE = '%s' '%s'\n",
        daemonCore->InfoCommandSinfulStringMyself(true),
        transferKey.c_str()
    );

    daemonCore->Register_Command(
        FILETRANS_UPLOAD, "FILETRANS_UPLOAD",
        (CommandHandler) & shadow_input_command_handler, "shadow_input_command_handler",
        WRITE
    );
}


int
starter_input_socket_handler( Stream * s ) {
    static FileTransferFunctions::GoAheadState gas;

    chdir( _cwd.c_str() );

    assert( s != NULL );
    ReliSock * rs = dynamic_cast<ReliSock *>(s);
    assert( rs != NULL );

    bool wasFinishCommand = false;
    std::unique_ptr<ReliSock> sock(rs);
    bool proceed = FileTransferFunctions::handleOneCommand(
        sock, wasFinishCommand, gas
    );
    sock.release();

    if(! proceed) {
        fprintf( stderr, "FileTransferFunctions::handleOneCommand() said not to proceed, closing socket.\n" );
        return CLOSE_STREAM;
    };

    if( wasFinishCommand ) {
        ClassAd peerReport;
        FileTransferFunctions::receiveFinalReport( rs, peerReport );

        ClassAd myReport;
        myReport.Assign(ATTR_RESULT, 0 /* success */);
        FileTransferFunctions::sendFinalReport( rs, myReport );

        DC_Exit(0);
    } else {
        return KEEP_STREAM;
    }
}


void
do_starter_input( const char * addr, const char * key ) {
    fprintf( stderr, "do_starter_input(%s, %s): begins\n", addr, key );

    auto sock = FileTransferFunctions::connectToPeer(
        addr, "",
        FILETRANS_UPLOAD
    );
    FileTransferFunctions::sendTransferKey( sock, key );

    int finalTransfer;
    ClassAd transferInfoAd;
    FileTransferFunctions::receiveTransferInfo( sock,
        finalTransfer, transferInfoAd
    );

    daemonCore->Register_Socket(
        sock.release(), "file_transfer_socket",
        (SocketHandler) starter_input_socket_handler, "file_transfer_handler"
    );
}


int
shadow_output_socket_handler( Stream * s ) {
    static FileTransferFunctions::GoAheadState gas;

    chdir( _cwd.c_str() );

    assert( s != NULL );
    ReliSock * rs = dynamic_cast<ReliSock *>(s);
    assert( rs != NULL );

    bool wasFinishCommand = false;
    std::unique_ptr<ReliSock> sock(rs);
    bool proceed = FileTransferFunctions::handleOneCommand(
        sock, wasFinishCommand, gas
    );
    sock.release();

    if(! proceed) {
        fprintf( stderr, "FileTransferFunctions::handleOneCommand() said not to proceed, closing socket.\n" );
        return CLOSE_STREAM;
    };

    if( wasFinishCommand ) {
        ClassAd myReport;
        myReport.Assign(ATTR_RESULT, 0 /* success */);
        FileTransferFunctions::sendFinalReport( rs, myReport );

        ClassAd peerReport;
        FileTransferFunctions::receiveFinalReport( rs, peerReport );

		return CLOSE_STREAM;
    } else {
        return KEEP_STREAM;
    }
}


int
shadow_output_command_handler( int command, Stream * s ) {
    assert( s != NULL );
    ReliSock * sock = dynamic_cast<ReliSock *>(s);
    assert( sock != NULL );

    std::string peerTransferKey;
    FileTransferFunctions::receiveTransferKey( sock, peerTransferKey );
    assert( transferKey == peerTransferKey );

    assert( command == FILETRANS_DOWNLOAD );


    int finalTransfer;
    ClassAd transferInfoAd;
    std::unique_ptr<ReliSock> u_sock(sock);
    FileTransferFunctions::receiveTransferInfo( u_sock,
        finalTransfer, transferInfoAd
    );
    u_sock.release();

    daemonCore->Register_Socket(
        sock, "file_transfer_socket",
        (SocketHandler) shadow_output_socket_handler, "file_transfer_handler"
    );


    return KEEP_STREAM;
}


void
do_shadow_output() {
    fprintf( stderr, "do_shadow_output(): begins\n" );

    FileTransferFunctions::generateTransferKey( transferKey );

    fprintf( stdout,
        "COMMAND LINE = '%s' '%s'\n",
        daemonCore->InfoCommandSinfulStringMyself(true),
        transferKey.c_str()
    );

    daemonCore->Register_Command(
        FILETRANS_DOWNLOAD, "FILETRANS_DOWNLOAD",
        (CommandHandler) & shadow_output_command_handler, "shadow_output_command_handler",
        WRITE
    );
}


void
do_starter_output( const char * addr, const char * key, const char * source, const char * destination ) {
    fprintf( stderr, "do_starter_output(%s, %s): begins\n", addr, key );

    auto sock = FileTransferFunctions::connectToPeer(
        addr, "",
        FILETRANS_DOWNLOAD
    );
    FileTransferFunctions::sendTransferKey( sock, key );


    //
    // Decide what we're going to transfer.
    //
    std::map<std::string, std::string> entries;
    entries[source] = destination;

    //
    // Transfer it.
    //
    chdir( _cwd.c_str() );
    bool exit_when_done = true;
    FileTransferUtils::sendFilesToPeer( sock.release(), entries, exit_when_done );
}


int _argc;
char ** _argv;

void
main_init( int /* argc */, char ** /* argv */ ) {
    dprintf_set_tool_debug( "TOOL", 0 );

    if( _argc < 2 ) {
        fprintf( stderr, " --shadow --input source destination\n" );
        fprintf( stderr, "--starter --input sinful transfer_key\n" );
        fprintf( stderr, " --shadow --output\n" );
        fprintf( stderr, "--starter --output source destination sinful transfer_key\n" );
        DC_Exit(1);
    }

    if( 0 == strcmp( _argv[1], "--shadow" )) {
        if( 0 == strcmp( _argv[2], "--input" )) {
            do_shadow_input( _argv[3], _argv[4] );
        } else if( 0 == strcmp( _argv[2], "--output" )) {
            do_shadow_output();
        } else {
            fprintf( stderr, "--shadow (--input|--output)\n" );
            DC_Exit(1);
        }
    } else if( 0 == strcmp( _argv[1], "--starter" )) {
        if( 0 == strcmp( _argv[2], "--input" )) {
            do_starter_input( _argv[3], _argv[4] );
        } else if( 0 == strcmp( _argv[2], "--output" )) {
            do_starter_output( _argv[3], _argv[4], _argv[5], _argv[6] );
        } else {
            fprintf( stderr, "--starter (--input|--output)\n" );
            DC_Exit(1);
        }
    } else {
        fprintf( stderr, "(--shadow|--starter)\n" );
        DC_Exit(1);
    }
}

void
main_shutdown_fast() {
    DC_Exit( 0 );
}

void
main_shutdown_graceful() {
    DC_Exit( 0 );
}

void
main_pre_dc_init( int /* argc */, char ** /* argv */ ) {
}

void
main_pre_command_sock_init() {
}

void
main_config() {
}

int
main( int argc, char ** argv ) {
    condor_getcwd(_cwd);

    set_mySubSystem( "FT_EXPERIMENT", true, SUBSYSTEM_TYPE_DAEMON );

    // This is dumb, but easier than fighting daemon core about parsing.
    _argc = argc;
    _argv = (char **)malloc( argc * sizeof( char * ) );
    for( int i = 0; i < argc; ++i ) {
        _argv[i] = strdup( argv[i] );
    }

    // This is also dumb, but less dangerous than (a) reaching into daemon
    // core to set a flag and (b) hoping that my command-line arguments and
    // its command-line arguments don't conflict.
    char ** dcArgv = (char **)malloc( 3 * sizeof( char * ) );
    dcArgv[0] = argv[0];
    // Force daemon core to run in the foreground.
    dcArgv[1] = strdup( "-f" );
    dcArgv[2] = NULL;

    dc_main_init = & main_init;
    dc_main_config = & main_config;
    dc_main_shutdown_fast = & main_shutdown_fast;
    dc_main_shutdown_graceful = & main_shutdown_graceful;
    dc_main_pre_dc_init = & main_pre_dc_init;
    dc_main_pre_command_sock_init = & main_pre_command_sock_init;

    return dc_main( 2, dcArgv );
}

