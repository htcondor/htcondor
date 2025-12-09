#include "condor_common.h"

#include <set>
#include <string>
#include "reli_sock.h"
#include "compat_classad.h"
#include "dc_coroutines.h"
#include "file_transfer_utils.h"

#include "condor_attributes.h"
#include "file_transfer_constants.h"
#include "file_transfer_functions.h"
#include "file_transfer_commands.h"

// For exit_when_done_hack, should remove.
#include "condor_daemon_core.h"

condor::cr::void_coroutine
FileTransferUtils::sendFilesToPeer(
    ReliSock * sock,
    std::map<std::string, std::string> entries,
    bool exit_when_done_hack
) {
    int sandbox_size = 0;

    //
    // Send transfer info.
    //
    dprintf( D_FULLDEBUG, "FileTransferUtils::sendFilesToStarter(): sending transfer info.\n" );
    ClassAd transferInfoAd;
    transferInfoAd.Assign( ATTR_SANDBOX_SIZE, sandbox_size );
    FileTransferFunctions::sendTransferInfo( sock,
        0 /* definitely not the final transfer */,
        transferInfoAd
    );
    dprintf( D_FULLDEBUG, "FileTransferUtils::sendFilesToStarter(): sent transfer info.\n" );

    // The idea of this hack is that the reaper for PID 1 can't ever be
    // called, so co_await() just goes in and out of the event loop
    // via a 0-second timer every time we call it.
    {
        condor::dc::AwaitableDeadlineReaper hack;
        hack.born( 1, 0 );
        auto [pid, timed_out, status] = co_await(hack);
        ASSERT(pid == 1);
        ASSERT(timed_out);
    }

    //
    // Then we send the starter one command at a time until we've
    // transferred everything.
    //
    FileTransferFunctions::GoAheadState gas;
    for( const auto & [source, destination] : entries ) {
        auto * c = FileTransferCommands::make(
            TransferCommand::XferFile,
            source,
            destination
        );
        c->execute( gas, sock );
        delete(c);

        {
            condor::dc::AwaitableDeadlineReaper hack;
            hack.born( 1, 0 );
            auto [pid, timed_out, status] = co_await(hack);
            ASSERT(pid == 1);
            ASSERT(timed_out);
        }
    }

    //
    // After sending the last file, send the finish command.
    //
    dprintf( D_FULLDEBUG, "FileTransferUtils::sendFilesToStarter(): sending finished command.\n" );
    FileTransferFunctions::sendFinishedCommand( sock );
    dprintf( D_FULLDEBUG, "FileTransferUtils::sendFilesToStarter(): sent finished command.\n" );

    {
        condor::dc::AwaitableDeadlineReaper hack;
        hack.born( 1, 0 );
        auto [pid, timed_out, status] = co_await(hack);
        ASSERT(pid == 1);
        ASSERT(timed_out);
    }

    //
    // After the finish command, send our final report.
    //
    ClassAd myFinalReport;
    myFinalReport.Assign( ATTR_RESULT, 0 /* success */ );
    dprintf( D_FULLDEBUG, "FileTransferUtils::sendFilesToStarter(): sending final report.\n" );
    FileTransferFunctions::sendFinalReport( sock, myFinalReport );
    dprintf( D_FULLDEBUG, "FileTransferUtils::sendFilesToStarter(): sent final report.\n" );

    {
        condor::dc::AwaitableDeadlineReaper hack;
        hack.born( 1, 0 );
        auto [pid, timed_out, status] = co_await(hack);
        ASSERT(pid == 1);
        ASSERT(timed_out);
    }

    //
    // After sending our final report, receive our peer's final report.
    //
    ClassAd peerFinalReport;
    dprintf( D_FULLDEBUG, "FileTransferUtils::sendFilesToStarter(): receiving final report.\n" );
    FileTransferFunctions::receiveFinalReport( sock, peerFinalReport );
    dprintf( D_FULLDEBUG, "FileTransferUtils::sendFilesToStarter(): received final report.\n" );

    //
    // In the command handler, we returned KEEP_STREAM, so I think
    // nobody else will do this for us.
    //
    delete sock;

    // If this should be a boolean parameter, we'd normally make it an enum
    // instead so that the calls would be `..., EXIT_WHEN_DONE)` and
    // `..., STAY_ALIVE)`, but since the _proper_ thing to do would be to
    // make this coroutine awaitable(), instead...
    if( exit_when_done_hack ) {
        DC_Exit(0);
    }
}
