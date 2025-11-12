#ifndef   _FILE_TRANSFER_UTILS_H
#define   _FILE_TRANSFER_UTILS_H

// #include "condor_common.h"
// #include <set>
// #include <string>
// #include "reli_sock.h"
// #include "compat_classad.h"
// #include "dc_coroutines.h"
// #include "file_transfer_utils.h"

namespace FileTransferUtils {

    condor::cr::void_coroutine sendFilesToPeer(
        ReliSock * sock,
        // Force a copy for lifetime management reasons.
        std::map<std::string, std::string> entries,
        bool exit_when_done_hack = false
    );

}

#endif /* _FILE_TRANSFER_UTILS_H */
