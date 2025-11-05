The New File-Transfer Object
============================

The goal of the "new FTO" was to demonstrate that we had a large number of
better options available for the interface to / API of the FTO, and that
those options did not require any changes to the wire protocol.  To that
end, this branch includes a regression test -- test_new_ft_interop -- that
runs the same job four times, once for each shadow/starter new/old pairing,
to demonstrate wire-level compability, both forwards and backwards.

File Transfer in the Shadow
---------------------------

In `remoteresouce.cpp`, the new code demonstrates:

    * better naming: `handleInputSandboxTransfer()` and `handleOutputSandboxTransfer()`,
      which are unambigious because they're (only) in the shadow.  And then
      the implementations of those command handlers, `sendFilesToStarter()`
      and `receiveFilesFromStarter()`.
    * The ability to have nonblocking file transfer without forking, and still
      retain straight-line functions, by making both `sendFilesToStarter()` and
      `receiveFilesFromStarter()` coroutines.

      (These coroutines are called once and manage their own lifecycle
      thereafter; the convention from the starter guidance code is that they
      should instead be `start_*()`, but I'm not sure that'd be helpful in
      that case, or if it is generally...)
    * Those coroutines make use of the utilities in the `FileTransferFunctions`
      namespace to avoid code duplication and simplify/clarify their flow.
    * They _do_ use a hack to wibble in and out of the event loop, but it
      would not be difficult to regularize.
    * `sendFilesToStarter()` uses the factory-style `FileTransferCommands`
      utilities to actually operate the protocol itself.
    * Conversely, `receiveFilesFromStarter()` uses the `handleOneCommand()`
      utility to do just that, and implements no transfer code itself.


File Transfer in the Starter
----------------------------

The `newTransferOutput()` function is an example of a simple blocking
invocation of the new file-transfer code; this was OK, because we didn't use
to fork for output transfer.  Of interest is the re-use of the
FileTransferFunctions utilities and the FileTransferCommand objects, hopefully
making it clear that the protocol is symmetric even though the name of the
function is (hopefully) clear.

In `beginNewInputTransfer()` we show how the new code can use DaemonCore's
socket registration directly in order to be non-blocking.


Miscellany
----------

This branch includes a command-line tool intended to allow for more-precise
testing; it knows how to spin up the new FTO in input mode for both the
starter and shadow.  (It does _not_ know how to do output mode, despite
riggging to the contrary.)  It should also know how to spin up the old FTO
for interop testing, but presently doesn't.  The input mode code appears to
have just been copied from the shadow and starter sources; it should be
refactored, if at all possible, so that both call the _same_ code.

- [ ] Add output flag implementations.
- [ ] Add ability to spin up, at least approximately, old FTOs.
- [ ] Refactor.

This branch also includes code for the shadow and starter to agree at run-time
which file-transfer protocol to use.

- [ ] (This isn't tested yet.)
