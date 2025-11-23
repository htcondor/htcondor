Although the old file-transfer protocol is not specific to the shadow and
starter, for clarity, we will pretend that it is.

File transfer is initiated by the starter.  The starter may initiate transfers
to the shadow or from the shadow.  The former we call "output" transfer and
the latter we call "input" transfer.  Because input transfer happens first,
we'll start there.

Starter: Input Transfer
=======================

How the starter obtains the address of the shadow('s transfer object) and the
corresponding transfer key is out of the scope of this document.

- FileTransferFunctions::connectToPeer() sends the FILETRANS_UPLOAD command
  to get things started.  This requires the shadow to have register the
  appropriate function to that command handler.

- FileTransferFunctions::sendTransferKey() sends the transfer key.

- FileTransferFunctions::receiveTransferInfo() reads the header.

- The main transfer loop begins.  This loop is not presently coded to return
  to the main event loop, but it probably should be.  Each time through the
  loop:
  - Read the command int.  If the command is the final command, exit the loop.
  - For some commands, set crypto mode.  For others, unset crypto mode.
  - Then read the filename from the socket using CEDAR; end the message.
  - If necessary, send and receive the go-ahead.
  - Handle the specific commands:
    - For TransferCommand::XferFile, call the CEDAR get_file_with_permissions()
      (and end the message).
    - For TransferCommand::Mkdir, read the file_mode int off the wire, then
      create the directory named `filename` with the give mode and umask 0
      (and end the message).
    - ...

- After the main transfer loop exits:
  - receive shadow's final report
  - send the starter's final report

Shadow: Input Transfer
=======================

- As noted above, the shadow must have registered the FILETRANS_UPLOAD
  DaemonCore command before the transfer starts; the sinful string for this
  command port is the address above.  It will have also generated the
  the transfer key.

  The transfer key is a capability the starter must present in order to
  be allowed to transfer files via the shadow.  It should be randomly
  generated for each new shadow instance, using
  FileTransferFunctions::generateTransferKey().

- FileTransferFunctions::sendTransferKey() receives the transfer key and
  validates that it matches the one initially generated.

- FileTransferFunctions::sendFilesToStarter() does the following:
  - FileTransferFunctions::sendTransferInfo() sends the transfer info.
  - For each entry in entries:
    - Send the corresponding transfer command.
    - Send the file name.
    - If necessary, receive and send the go-ahead.
    - If the command was:
    - Handle the specific commands:
      - For TransferCommand::XferFile, call the CEDAR put_file_with_permissions()
        (and end the message).
      - For TransferCommand::Mkdir, write the file_mode int
        (and end the message).
  - Send the finished command.
  - Send the shadow's final report.
  - Receive the starter's final report.


Output Transfer
===============

Output transfer looks just like input transfer, except that:
- the starter sends the FILETRANS_UPLOAD command
- the starter sends the transfer info
- the starter sends-and-receives the go-ahead
- the starter sends the transfer commands
- the starter sends the finished command
- the starter sends the final report first, then receives.
