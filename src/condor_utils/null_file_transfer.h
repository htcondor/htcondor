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

//
// These functions were written for the starter's side of input sandbox
// transfer, but (some) can hopefully be used elsewhere.
//

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

        static void receiveTransferInfo(
            std::unique_ptr<ReliSock> & sock,
            int & finalTransfer,
            ClassAd & transferInfoAd
        );

        static bool handleOneCommand(
            std::unique_ptr<ReliSock> & sock,
            bool & receivedFinishedCommand,
            GoAheadState & gas
        );

//
// These function were written for the shadow's side of input sandbox
// transfer, but hopefully (some) can be used elsewhere.
//

        static void generateTransferKey(
            std::string & transferKey
        );

        static void receiveTransferKey(
            ReliSock * sock,
            std::string & peerTransferKey
        );

        static void sendTransferInfo(
            ReliSock * sock,
            int finalTransfer,
            const ClassAd & transferInfoAd
        );

        static void sendFinishedCommand(
            ReliSock * sock
        );

//
// Used by both the starter and the shadow for input sandbox transfers.
//

        static void sendFinalReport(
            ReliSock * sock,
            const ClassAd & report
        );

        static void receiveFinalReport(
            ReliSock * sock,
            ClassAd & report
        );
};

#endif /* _NULL_FILE_TRANSFER_H */
