#ifndef   _NULL_FILE_TRANSFER_H
#define   _NULL_FILE_TRANSFER_H

// #include "condor_common.h"
// #include <memory>
// #include <string>
// #include "condor_ver_info.h"
// #include "reli_sock.h"
// #include "compat_classad.h"
// #include "file_transfer_constants.h"
// #include "null_file_transfer.h"

class Service;
typedef int (Service::*NullFileTransferCallback)();

class NullFileTransfer {
    public:

        typedef struct {
            int local_go_ahead {GO_AHEAD_UNDEFINED};
            int remote_go_ahead {GO_AHEAD_UNDEFINED};
        } GoAheadState;

        // This is idiomatically modern C++, saying that the
        // caller owns the return value.
        static std::unique_ptr<ReliSock> connectToPeer(
            const std::string & transferAddress,
            const std::string & security_session
        );

        static void sendTransferKey(
            std::unique_ptr<ReliSock> & sock,
            const std::string & transfer_key
        );

        static void getTransferInfo(
            std::unique_ptr<ReliSock> & sock,
            int & finalTransfer,
            ClassAd & transferInfoAd
        );

        static bool handleOneCommand(
            std::unique_ptr<ReliSock> & sock,
            bool & receivedFinishedCommand,
            GoAheadState & gas
        );
};

#endif /* _NULL_FILE_TRANSFER_H */
