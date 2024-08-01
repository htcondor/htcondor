#include "condor_common.h"

#include <string>
#include "condor_ver_info.h"
#include "null_file_transfer.h"

#include "reli_sock.h"
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

NullFileTransfer::NullFileTransfer() { }


void
NullFileTransfer::setPeerVersion( const CondorVersionInfo & /* version */ ) {
    // This function is a total disaster in the original.
}


void
NullFileTransfer::setTransferAddress( const std::string & address ) {
    this->transfer_address = address;
}


void
NullFileTransfer::setTransferKey( const std::string & key ) {
    this->transfer_key = key;
}


void
NullFileTransfer::setSecuritySession( char const * session_id ) {
    if( session_id == NULL ) {
        this->security_session.clear();
    } else {
        this->security_session = session_id;
    }
}


void
NullFileTransfer::setCompletionCallback(
  NullFileTransferCallback handler,
  Service * handler_this
) {
    this->completion_callback = handler;
    this->completion_callback_this = handler_this;
}


// It's not that keeping track of this socket is problematic, but
// unique_ptr<> is the modern C++ way of indicating that doing so
// is the caller's problem.
std::unique_ptr<ReliSock>
connectToPeer(
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

// This is a hack.
void
NullFileTransfer::receiveFilesFromPeer() {
    auto sock = connectToPeer( this->transfer_address, this->security_session );

    // Send the transfer key.
    sock->encode();
    sock->put_secret( this->transfer_key );
    sock->end_of_message();

    auto socket_default_crypto = sock->get_encryption();


    //
    // I don't think it matters where we initialize this, but we
    // may as well do it at the same place as in the original code.
    //
    // DCTransferQueue transferQueue( this->transferQueueContactInfo );


    // Receive the final_transfer flag and the (optional) transfer info ClassAd.
    sock->decode();

    int finalTransfer;
    sock->get(finalTransfer);

    ClassAd transferInfoAd;
    getClassAd(sock.get(), transferInfoAd);

    sock->end_of_message();


    //
    // The existence of this primary command loop is what makes this
    // whole function a hack.  More on that later, when I get this to
    // actually do something.
    //
    /* begin primary command loop */ while( true ) {

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
        // Break out of the loop if we're done.
        //
        dprintf( D_ALWAYS, "NullFileTransfer: ignoring command %i\n", static_cast<int>(command) );
        if( command == TransferCommand::Finished ) { break; }

        //
        // Set the crypto mode.
        // (This is probably always a no-op at this point.)
        //
        // Consider re-writing when doing error-handling to just set
        // a flag and do all the error handling in one place, after the switch.
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
        sock->end_of_message(); /* This is almost certainly a logic error; it should happen as part of receiving the file name, but the original code made it conditional on the go-ahead logic. */

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
    } /* end primary command loop */


    // Reset socket crypto state.
    sock->set_crypto_mode(socket_default_crypto);

    // Receive final report.
    ClassAd peerReport;
    sock->decode();
    getClassAd(sock.get(), peerReport);
    sock->end_of_message();
    dprintf( D_ALWAYS, "NullFileTransfer: ignoring peer's report.\n" );

    // Send final report.
    ClassAd myReport;
    myReport.Assign(ATTR_RESULT, 0 /* success */);
    sock->encode();
    putClassAd(sock.get(), myReport);
    sock->end_of_message();
    dprintf( D_ALWAYS, "NullFileTransfer: sent report to peer.\n" );

    if( completion_callback && completion_callback_this ) {
        ((completion_callback_this)->*(completion_callback))();
    }
}

