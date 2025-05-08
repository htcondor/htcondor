#ifndef   _FILE_TRANSFER_CONSTANTS_H
#define   _FILE_TRANSFER_CONSTANTS_H

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

enum class TransferSubCommand {
    Unknown = -1,
    UploadUrl = 7,
    ReuseInfo = 8,
    SignUrls = 9
};

const int GO_AHEAD_FAILED       = -1;
const int GO_AHEAD_UNDEFINED    = 0;
const int GO_AHEAD_ONCE         = 1;
const int GO_AHEAD_ALWAYS       = 2;

#endif /* _FILE_TRANSFER_CONSTANTS_H */

