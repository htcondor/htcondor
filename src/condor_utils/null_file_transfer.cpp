#include "condor_common.h"

#include <memory>
#include <string>
#include "condor_ver_info.h"
#include "reli_sock.h"
#include "compat_classad.h"
#include "null_file_transfer.h"

#include "daemon.h"
#include "condor_attributes.h"

// Begin constants From file_transfer.cpp;
// these should be in a header file.

enum class TransferCommand {
    Unknown = -1,
    Finished = 0,
    XferFile = 1,
    EnableEncryption = 2,
    DisableEncryption = 3,
    XferX509 = 4,
    DownloadUrl = 5,
    Mkdir = 6,
    Other = 999
};

const int GO_AHEAD_FAILED       = -1;
const int GO_AHEAD_UNDEFINED    = 0;
const int GO_AHEAD_ONCE         = 1;
const int GO_AHEAD_ALWAYS       = 2;

// end constants from file_transfer.cpp

std::unique_ptr<ReliSock>
NullFileTransfer::connectToPeer(
  const std::string & transferAddress,
  const std::string & security_session
) {
    ReliSock * sock = new ReliSock();
    CondorError errorStack;

    Daemon peer( DT_ANY, transferAddress.c_str() );
    peer.connectSock( sock, 0 /* ? */ );
    peer.startCommand( FILETRANS_UPLOAD,
        sock, 0 /* timeout */, & errorStack,
        NULL /* command description */, false /* raw_protocol */,
        security_session.c_str()
    );

    return std::unique_ptr<ReliSock>(sock);
}

void
NullFileTransfer::sendTransferKey( std::unique_ptr<ReliSock> & sock, const std::string & transfer_key ) {
    sock->encode();
    sock->put_secret( transfer_key );
    sock->end_of_message();
}

void
NullFileTransfer::getTransferInfo( std::unique_ptr<ReliSock> & sock, int & finalTransfer, ClassAd & transferInfoAd ) {
    sock->decode();
    sock->get(finalTransfer);
    getClassAd(sock.get(), transferInfoAd);
    sock->end_of_message();
}

void
NullFileTransfer::handleOneCommand( std::unique_ptr<ReliSock> & sock, bool & receivedFinishedCommand ) {
    // Store socket crypto state (since we'll be changing it).
    auto socket_default_crypto = sock->get_encryption();


    //
    // Read the command int.
    //
    TransferCommand command = TransferCommand::Unknown;
    {
        int raw;
        if(! sock->get(raw)) {
            dprintf( D_ALWAYS, "NullFileTransfer: failed to get() command, aborting.\n" );
            return;
        }
        command = static_cast<TransferCommand>(raw);
    }
    sock->end_of_message();

    //
    // If the command protocol is done, return after letting our caller know.
    //
    dprintf( D_ALWAYS, "NullFileTransfer: ignoring command %i\n", static_cast<int>(command) );
    if( command == TransferCommand::Finished ) {
        receivedFinishedCommand = true;
        return;
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
    dprintf( D_ALWAYS, "NullFileTransfer: ignoring filename %s\n", fileName.c_str() );

    //
    // There's a bunch of complicated logic here having to do with
    // sending and receiving go-aheads, and how those interact with
    // the transfer queue, and we can't just skip all of it because
    // it mucks with the protocol.
    //
    sock->end_of_message(); /* This is almost certainly a logic error in the original code; it should happen as part of receiving the file name, but the original code made it conditional on the go-ahead logic. */

    int theirKeepaliveInterval;
    sock->get(theirKeepaliveInterval);
    sock->end_of_message();
    dprintf( D_ALWAYS, "NullFileTransfer: ignoring their keepalive interval %d\n", theirKeepaliveInterval );

    ClassAd outMessage;
    outMessage.Assign(ATTR_RESULT, GO_AHEAD_ALWAYS);
    sock->encode();
    putClassAd(sock.get(), outMessage);
    sock->end_of_message();
    dprintf( D_ALWAYS, "NullFileTransfer: skipping transfer queue\n" );

    int myKeepaliveInterval = 300; /* magic, for now */
    sock->encode();
    sock->put(myKeepaliveInterval);
    sock->end_of_message();
    dprintf( D_ALWAYS, "NullFileTransfer: sent my keepalive interval %d\n", myKeepaliveInterval );

    ClassAd inMessage;
    sock->decode();
    getClassAd(sock.get(), inMessage);
    sock->end_of_message();
    int goAhead = GO_AHEAD_UNDEFINED;
    inMessage.LookupInteger(ATTR_RESULT, goAhead);
    dprintf( D_ALWAYS, "NullFileTransfer: ignoring go-ahead of %d\n", goAhead );


    //
    // Handle each individual command.
    //
    switch(command) {
        case TransferCommand::XferFile:
            dprintf( D_ALWAYS, "NullFileTransfer: transferring file to '%s'.\n", fileName.c_str() );

            filesize_t bytes;
            sock->get_file_with_permissions( & bytes,
                fileName.c_str(), /* destination */
                false /* don't flush buffers */,
                -1 /* this_file_max_bytes */,
                NULL /* the transfer queue to contact */
            );
            break;

        default:
            dprintf( D_ALWAYS, "NullFileTransfer: command %d not recognized\n", static_cast<int>(command) );
            break;
    }


    // Reset socket crypto state.
    sock->set_crypto_mode(socket_default_crypto);
}
