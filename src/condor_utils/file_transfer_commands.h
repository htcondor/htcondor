#ifndef   _FILE_TRANSFER_SEQUENCE_H
#define   _FILE_TRANSFER_SEQUENCE_H

// #include <string>
// #include <memory>
// #include "file_transfer_functions.h"
// #include "file_transfer_commands.h"

namespace FileTransferCommands {

    class Command {
        public:
            virtual void execute(
                FileTransferFunctions::GoAheadState & gas,
                ReliSock * sock
            ) = 0;

            virtual ~Command() = default;

        protected:
            std::string source;
            std::string destination;
    };

    class TransferFile : public Command {
        public:
            TransferFile(
                const std::string & src,
                const std::string & dest
            );

            virtual void execute(
                FileTransferFunctions::GoAheadState & gas,
                ReliSock * sock
            );

            virtual ~TransferFile() = default;
    };

    Command * make(
                TransferCommand command,
                const std::string & source,
                const std::string & destination
            );

}

#endif /* _FILE_TRANSFER_SEQUENCE_H */
