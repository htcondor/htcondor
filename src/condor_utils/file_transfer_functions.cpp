#include "condor_common.h"

#include <memory>
#include <string>
#include "condor_ver_info.h"
#include "reli_sock.h"
#include "compat_classad.h"
#include "file_transfer_constants.h"
#include "file_transfer_functions.h"

#include "daemon.h"
#include "condor_attributes.h"

//
// For the starter, doing input sandbox transfer.
//

std::unique_ptr<ReliSock>
FileTransferFunctions::connectToPeer(
  const std::string & transferAddress,
  const std::string & security_session,
  int command
) {
    ReliSock * sock = new ReliSock();
    CondorError errorStack;

    Daemon peer( DT_ANY, transferAddress.c_str() );
    peer.connectSock( sock, 0 /* ? */ );
    peer.startCommand( command,
        sock, 0 /* timeout */, & errorStack,
        NULL /* command description */, false /* raw_protocol */,
        security_session.size() == 0 ? NULL : security_session.c_str()
    );

    return std::unique_ptr<ReliSock>(sock);
}

void
FileTransferFunctions::sendTransferKey( std::unique_ptr<ReliSock> & sock, const std::string & transfer_key ) {
    sock->encode();
    sock->put_secret( transfer_key );
    sock->end_of_message();
}

void
FileTransferFunctions::receiveTransferInfo( std::unique_ptr<ReliSock> & sock, int & finalTransfer, ClassAd & transferInfoAd ) {
    sock->decode();
    sock->get(finalTransfer);
    getClassAd(sock.get(), transferInfoAd);
    sock->end_of_message();
}

bool /* did we successfully handle a command? */
FileTransferFunctions::handleOneCommand(
    std::unique_ptr<ReliSock> & sock, bool & receivedFinishedCommand,
    FileTransferFunctions::GoAheadState & gas
) {
    //
    // FIXME: We need to move this out one layer of code, because
    // the original code set the crypto at the top of the command loop,
    // it didn't _resest_ it until the command loop was done.  It
    // works in testing anyway because in practice, we start in crypto
    // mode and never leave.
    //

    // Store socket crypto state (since we'll be changing it).
    auto socket_default_crypto = sock->get_encryption();


    //
    // Read the command int.
    //
    TransferCommand command = TransferCommand::Unknown;
    {
        int raw;
        if(! sock->get(raw)) {
            dprintf( D_ALWAYS, "FileTransferFunctions: failed to get() command, aborting.\n" );
            return false;
        }
        command = static_cast<TransferCommand>(raw);
    }
    sock->end_of_message();

    //
    // If the command protocol is done, return after letting our caller know.
    //
    dprintf( D_ALWAYS, "FileTransferFunctions: ignoring command %i\n", static_cast<int>(command) );
    if( command == TransferCommand::Finished ) {
        receivedFinishedCommand = true;
        return true;
    }

    //
    // Set the crypto mode.
    //
    // Consider re-writing when adding the error-handling to just set
    // a flag and only call set_crypto_mode() in one place, so we only
    // have one copy of the error-handling code.
    //
    switch( command ) {
        case TransferCommand::DownloadUrl:
            sock->set_crypto_mode( true );
            break;

        case TransferCommand::EnableEncryption:
            sock->set_crypto_mode( true );
            break;

        case TransferCommand::DisableEncryption:
            sock->set_crypto_mode( false );
            break;

        default:
            sock->set_crypto_mode(socket_default_crypto);
            break;
    }

    //
    // Every command int is followed a by string.
    //
    std::string fileName;
    sock->get( fileName );
    dprintf( D_ALWAYS, "FileTransferFunctions: ignoring filename %s\n", fileName.c_str() );

    //
    // In the original code, this only happens if the other peer "does
    // go ahead", which seems like a bug; logically, it should be associated
    // with having sent the filename.
    //
    sock->end_of_message();

    if( gas.local_go_ahead != GO_AHEAD_ALWAYS ) {
        // The starter is always ready to receive files.
        gas.local_go_ahead = GO_AHEAD_ALWAYS;

        int theirKeepaliveInterval;
        sendGoAhead( sock.get(), theirKeepaliveInterval, gas.local_go_ahead );
    }

    if( gas.remote_go_ahead != GO_AHEAD_ALWAYS ) {
        receiveGoAhead( sock.get(), 300 /* magic */, gas.remote_go_ahead );
    }

    //
    // Handle each individual command.
    //
    switch(command) {
        case TransferCommand::XferFile:
            dprintf( D_ALWAYS, "FileTransferFunctions: transferring file to '%s'.\n", fileName.c_str() );

            filesize_t bytes;
            sock->get_file_with_permissions( & bytes,
                fileName.c_str(), /* destination */
                false /* don't flush buffers */,
                -1 /* this_file_max_bytes */,
                NULL /* the transfer queue to contact */
            );
            break;

        default:
            dprintf( D_ALWAYS, "FileTransferFunctions: command %d not recognized\n", static_cast<int>(command) );
            break;
    }

    //
    //
    //
    sock->end_of_message();


    // Reset socket crypto state.
    sock->set_crypto_mode(socket_default_crypto);

    return true;
}


//
// For the shadow, doing input sandbox transfer.
//

void
FileTransferFunctions::generateTransferKey( std::string & transferKey ) {
    static int sequenceNumber = 0;

    formatstr( transferKey, "%x#%x%x%x",
        ++sequenceNumber, (unsigned)time(NULL),
        get_csrng_int(), get_csrng_int()
    );
}

void
FileTransferFunctions::receiveTransferKey(
    ReliSock * sock,
    std::string & peerTransferKey
) {
    sock->decode();
    sock->get_secret(peerTransferKey);
    sock->end_of_message();
}

void
FileTransferFunctions::sendTransferInfo(
    ReliSock * sock,
    int finalTransfer, const ClassAd & transferInfoAd
) {
    sock->encode();
    sock->put(finalTransfer);
    putClassAd(sock, transferInfoAd);
    sock->end_of_message();
}

void
FileTransferFunctions::sendFinishedCommand( ReliSock * sock ) {
    sock->encode();
    sock->put(static_cast<int>(TransferCommand::Finished));
    sock->end_of_message();
}

//
// For either side of input sandbox transfer.
//
void
FileTransferFunctions::sendFinalReport(
    ReliSock * sock, const ClassAd & report
) {
    sock->encode();
    putClassAd(sock, report);
    sock->end_of_message();
}

void
FileTransferFunctions::receiveFinalReport(
    ReliSock * sock, ClassAd & report
) {
    sock->decode();
    getClassAd(sock, report);
    sock->end_of_message();
}

void
FileTransferFunctions::sendGoAhead(
    ReliSock * sock, int & theirKeepaliveInterval, int myGoAhead
) {
    sock->decode();
    sock->get(theirKeepaliveInterval);
    sock->end_of_message();
    dprintf( D_ALWAYS, "FileTransferFunctions: ignoring their keepalive interval %d\n", theirKeepaliveInterval );

    ClassAd outMessage;
    outMessage.Assign(ATTR_RESULT, myGoAhead);

    sock->encode();
    putClassAd(sock, outMessage);
    sock->end_of_message();
    dprintf( D_ALWAYS, "FileTransferFunctions: sent go-ahead %d\n", myGoAhead );
}

void
FileTransferFunctions::receiveGoAhead(
    ReliSock * sock, int myKeepaliveInterval, int & theirGoAhead
) {
    sock->encode();
    sock->put(myKeepaliveInterval);
    sock->end_of_message();
    dprintf( D_ALWAYS, "FileTransferFunctions: sent my keepalive interval %d\n", myKeepaliveInterval );

    // Wait for the peer to be ready to send.
    ClassAd inMessage;

    sock->decode();
    getClassAd(sock, inMessage);
    sock->end_of_message();

    inMessage.LookupInteger(ATTR_RESULT, theirGoAhead);
    dprintf( D_ALWAYS, "FileTransferFunctions: received go-ahead %d\n", theirGoAhead );
}
