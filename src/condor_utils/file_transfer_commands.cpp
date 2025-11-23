#include "condor_common.h"

#include <string>
#include <memory>
#include "reli_sock.h"
#include "compat_classad.h"
#include "file_transfer_constants.h"
#include "file_transfer_functions.h"
#include "file_transfer_commands.h"

FileTransferCommands::Command *
FileTransferCommands::make(
  TransferCommand command,
  const std::string & source, const std::string & destination
) {
    switch(command) {
        case TransferCommand::XferFile:
            return new FileTransferCommands::TransferFile( source, destination );
        case TransferCommand::Mkdir:
            return new FileTransferCommands::MakeDirectory( source, destination );
        default:
            dprintf( D_ALWAYS, "Unknown command %d requested, failing.\n", static_cast<int>(command) );
            return NULL;
    }
}


FileTransferCommands::TransferFile::TransferFile(
  const std::string & source,
  const std::string & destination
) {
    this->source = source;
    this->destination = destination;
}

void
FileTransferCommands::TransferFile::execute(
  FileTransferFunctions::GoAheadState & gas,
  ReliSock * sock
) {
    dprintf( D_FULLDEBUG, "FileTransferCommands::TransferFile::execute(): begins.\n" );

    //
    // Send the transfer-file command.
    //
    sock->encode();
    sock->put(static_cast<int>(TransferCommand::XferFile));
    sock->end_of_message();

    //
    // Send the file name.
    //
    // (We're ignoring paths for now.)
    //
    sock->put(this->destination);
    sock->end_of_message();

    //
    // Ask for our peer's go-ahead.
    //
    if( gas.remote_go_ahead != GO_AHEAD_ALWAYS ) {
        int myKeepaliveInterval = 300; /* magic */
        FileTransferFunctions::receiveGoAhead( sock,
            myKeepaliveInterval,
            gas.remote_go_ahead
        );
    }

    //
    // Send our peer the go-ahead.
    //
    if( gas.local_go_ahead != GO_AHEAD_ALWAYS ) {
        gas.local_go_ahead = GO_AHEAD_ALWAYS;

        int theirKeepaliveInterval;
        FileTransferFunctions::sendGoAhead( sock,
            theirKeepaliveInterval,
            gas.local_go_ahead
        );
    }

    filesize_t bytes;
    sock->put_file_with_permissions(
        & bytes,        /* recording the file size here is unconditional */
        source.c_str(), /* path to the source file */
        -1              /* no size limit */,
        NULL            /* no transfer queue (only used for reporting) */
    );

    sock->end_of_message();
}


FileTransferCommands::MakeDirectory::MakeDirectory(
  const std::string & source,
  const std::string & destination
) {
    this->source = source;
    this->destination = destination;
}

void
FileTransferCommands::MakeDirectory::execute(
  FileTransferFunctions::GoAheadState & gas,
  ReliSock * sock
) {
    dprintf( D_FULLDEBUG, "FileTransferCommands::MakeDirectory::execute(): begins.\n" );

    //
    // Send the mkdir command.
    //
    sock->encode();
    sock->put(static_cast<int>(TransferCommand::Mkdir));
    sock->end_of_message();

    //
    // Send the file name.
    //
    // (We're ignoring paths for now.)
    //
    sock->put(this->destination);
    sock->end_of_message();

    //
    // Ask for our peer's go-ahead.
    //
    if( gas.remote_go_ahead != GO_AHEAD_ALWAYS ) {
        int myKeepaliveInterval = 300; /* magic */
        FileTransferFunctions::receiveGoAhead( sock,
            myKeepaliveInterval,
            gas.remote_go_ahead
        );
    }

    //
    // Send our peer the go-ahead.
    //
    if( gas.local_go_ahead != GO_AHEAD_ALWAYS ) {
        gas.local_go_ahead = GO_AHEAD_ALWAYS;

        int theirKeepaliveInterval;
        FileTransferFunctions::sendGoAhead( sock,
            theirKeepaliveInterval,
            gas.local_go_ahead
        );
    }


    //
    // Since the above is all boiler-plate, it should be refactored if we
    // decide to take this approach.
    //
    condor_mode_t file_mode = NULL_FILE_PERMISSIONS;
#ifndef WIN32
    struct stat statbuf {};
    lstat( source.c_str(), & statbuf );
    file_mode = (condor_mode_t)statbuf.st_mode;
#endif
    sock->put(file_mode);


    sock->end_of_message();
}
