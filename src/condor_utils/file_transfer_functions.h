#ifndef   _FILE_TRANSFER_FUNCTIONS_H
#define   _FILE_TRANSFER_FUNCTIONS_H

// #include "condor_common.h"
// #include <memory>
// #include <string>
// #include "condor_ver_info.h"
// #include "reli_sock.h"
// #include "compat_classad.h"
// #include "file_transfer_constants.h"
// #include "file_transfer_functions.h"

namespace FileTransferFunctions {

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
        std::unique_ptr<ReliSock> connectToPeer(
            const std::string & transferAddress,
            const std::string & security_session,
            int command
        );

        void sendTransferKey(
            std::unique_ptr<ReliSock> & sock,
            const std::string & transfer_key
        );

        void receiveTransferInfo(
            std::unique_ptr<ReliSock> & sock,
            int & finalTransfer,
            ClassAd & transferInfoAd
        );

        bool handleOneCommand(
            std::unique_ptr<ReliSock> & sock,
            bool & receivedFinishedCommand,
            GoAheadState & gas
        );

//
// These function were written for the shadow's side of input sandbox
// transfer, but hopefully (some) can be used elsewhere.
//

        void generateTransferKey(
            std::string & transferKey
        );

        void receiveTransferKey(
            ReliSock * sock,
            std::string & peerTransferKey
        );

        void sendTransferInfo(
            ReliSock * sock,
            int finalTransfer,
            const ClassAd & transferInfoAd
        );

        void sendFinishedCommand(
            ReliSock * sock
        );

//
// Used by both the starter and the shadow for input sandbox transfers.
//

        void sendFinalReport(
            ReliSock * sock,
            const ClassAd & report
        );

        void receiveFinalReport(
            ReliSock * sock,
            ClassAd & report
        );

        void receiveGoAhead(
            ReliSock * sock,
            int myKeepaliveInterval,
            int & theirGoAhead
        );

        void sendGoAhead(
            ReliSock * sock,
            int & theirKeepaliveInterval,
            int myGoAhead
        );

}

#endif /* _FILE_TRANSFER_FUNCTIONS_H */
