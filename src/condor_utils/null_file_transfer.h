#ifndef   _NULL_FILE_TRANSFER_H
#define   _NULL_FILE_TRANSFER_H

// #include "condor_common.h"
// #include <string>
// #include "condor_ver_info.h"
// #include "null_file_transfer.h"

class Service;
typedef int (Service::*NullFileTransferCallback)();

class NullFileTransfer /* : public Service */ {
    public:
        NullFileTransfer();

        // Consider making these part of the constructor.
        void setPeerVersion( const CondorVersionInfo & peerVersion );
        void setTransferAddress( const std::string & address );
        void setTransferKey( const std::string & key );

        // This may also be a hack; see receiveFilesFromPeer().
        // Strictly speaking, the caller should be able to poll for
        // completion, but if we have a higher-level abstraction class,
        // that class might be a better place for this callback.
        void setCompletionCallback(
            NullFileTransferCallback handler,
            Service * handler_this
        );


        // Used for match-session security, if available.
        void setSecuritySession( char const * session_id );

        // A hack; see the comment in jic_shadow.cpp.
        void receiveFilesFromPeer();

    protected:
        std::string         transfer_address;
        std::string         transfer_key;
        std::string         security_session;

        NullFileTransferCallback    completion_callback {NULL};
        Service *                   completion_callback_this {NULL};
};

#endif /* _NULL_FILE_TRANSFER_H */
