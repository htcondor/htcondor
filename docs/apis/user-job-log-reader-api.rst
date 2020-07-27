The HTCondor User and Job Log Reader API
========================================

:index:`ReadUserLog class<single: ReadUserLog class; API>`
:index:`User Log Reader API`
:index:`Event Log Reader API` :index:`ReadUserLog`
:index:`Job Log Reader API`

HTCondor has the ability to log an HTCondor job's significant events
during its lifetime. This is enabled in the job's submit description
file with the **Log** :index:`Log<single: Log; submit commands>` command.

This section describes the API defined by the C++ ``ReadUserLog`` class,
which provides a programming interface for applications to read and
parse events, polling for events, and saving and restoring reader state.

Constants and Enumerated Types
------------------------------

The following define enumerated types useful to the API.

-  ``ULogEventOutcome`` (defined in ``condor_event.h``):

   -  ``ULOG_OK``: Event is valid
   -  ``ULOG_NO_EVENT``: No event occurred (like EOF)
   -  ``ULOG_RD_ERROR``: Error reading log file
   -  ``ULOG_MISSED_EVENT``: Missed event
   -  ``ULOG_UNK_ERROR``: Unknown Error

-  ``ReadUserLog::FileStatus``

   -  ``LOG_STATUS_ERROR``: An error was encountered
   -  ``LOG_STATUS_NOCHANGE``: No change in file size
   -  ``LOG_STATUS_GROWN``: File has grown
   -  ``LOG_STATUS_SHRUNK``: File has shrunk

Constructors and Destructors
----------------------------

All ``ReadUserLog`` constructors invoke one of the ``initialize()``
methods. Since C++ constructors cannot return errors, an application
using any but the default constructor should call ``isIinitialized()``
to verify that the object initialized correctly, and for example, had
permissions to open required files.

Note that because the constructors cannot return status information,
most of these constructors will be eliminated in the future. All
constructors, except for the default constructor with no parameters,
will be removed. The application will need to call the appropriate
``initialize()`` method.

-  ReadUserLog::ReadUserLog(bool isEventLog)
   **Synopsis:** Constructor default
   **Returns:** None
   **Constructor** parameters:

   -  ``bool`` ``isEventLog`` (*Optional with default* = ``false``)
      If ``true``, the ``ReadUserLog`` object is initialized to read the
      schedd-wide event log.
      NOTE: If ``isEventLog`` is ``true``, the initialization may
      silently fail, so the value of ReadUserLog::isInitialized should
      be checked to verify that the initialization was successful.
      NOTE: The ``isEventLog`` parameter will be removed in the future.

-  | ReadUserLog::ReadUserLog(FILE \*fp, bool is_xml, bool enable_close
   | **Synopsis:** Constructor of a limited functionality reader:
     no rotation handling, no locking
   | **Returns:** None
   | **Constructor** parameters:

   -  ``FILE *`` ``fp``
      File pointer to the previously opened log file to read.
   -  ``bool`` ``is_xml``
      If ``true``, the file is treated as XML; otherwise, it will be
      read as an old style file.
   -  ``bool`` ``enable_close`` (*Optional with default* = ``false``)
      If ``true``, the reader will open the file read-only.

   | NOTE: The ReadUserLog::isInitialized method should be invoked to
     verify that this constructor was initialized successfully.
   | NOTE: This constructor will be removed in the future.

-  | ReadUserLog::ReadUserLog(const char \*filename, bool read_only)
   | **Synopsis:** Constructor to read a specific log file
   | **Returns:** None
   | **Constructor** parameters:

   -  ``const char *`` ``filename``
      Path to the log file to read
   -  ``bool`` ``read_only`` (*Optional with default* = ``false``)
      If ``true``, the reader will open the file read-only and disable
      locking.

   NOTE: This constructor will be removed in the future.

-  | ReadUserLog::ReadUserLog(const FileState &state, bool read_only)
   | **Synopsis:** Constructor to continue from a persisted reader state
   | **Returns:** None
   | **Constructor** parameters:

   -  ``const FileState &`` ``state``
      Reference to the persisted state to restore from
   -  ``bool`` ``read_only`` (*Optional with default* = ``false``)
      If ``true``, the reader will open the file read-only and disable
      locking.

   | NOTE: The ReadUserLog::isInitialized method should be invoked to
     verify that this constructor was initialized successfully.
   | NOTE: This constructor will be removed in the future.

-  ReadUserLog::˜ReadUserLog(void)
   **Synopsis:** Destructor
   **Returns:** None
   **Destructor** parameters:

   -  None.

Initializers
------------

These methods are used to perform the initialization of the
``ReadUserLog`` objects. These initializers are used by all constructors
that do real work. Applications should never use those constructors,
should use the default constructor, and should instead use one of these
initializer methods.

All of these functions will return ``false`` if there are problems such
as being unable to open the log file, or ``true`` if successful.

-  ``bool`` ReadUserLog::initialize(void)
   **Synopsis:** Initialize to read the EventLog file.
   NOTE: This method will likely be eliminated in the future, and this
   functionality will be moved to a new ``ReadEventLog`` class.
   **Returns:** ``bool``; ``true``: success, ``false``: failed
   **Method** parameters:

   -  None.

-  ``bool`` ReadUserLog::initialize(const char \*filename, bool
   handle_rotation, bool check_for_rotated, bool read_only)
   **Synopsis:** Initialize to read a specific log file.
   **Returns:** ``bool``; ``true``: success, ``false``: failed
   **Method** parameters:

   -  ``const char *`` ``filename``
      Path to the log file to read
   -  ``bool`` ``handle_rotation`` (*Optional with default* = ``false``)
      If ``true``, enable the reader to handle rotating log files, which
      is only useful for global user logs
   -  ``bool`` ``check_for_rotated`` (*Optional with default* =
      ``false``)
      If ``true``, try to open the rotated files (with file names
      appended with ``.old`` or ``.1``, ``.2``, ...) first.
   -  ``bool`` ``read_only`` (*Optional with default* = ``false``)
      If ``true``, the reader will open the file read-only and disable
      locking.

-  ``bool`` ReadUserLog::initialize(const char \*filename, int
   max_rotation, bool check_for_rotated, bool read_only)
   **Synopsis:** Initialize to read a specific log file.
   **Returns:** ``bool``; ``true``: success, ``false``: failed
   **Method** parameters:

   -  ``const char *`` ``filename``
      Path to the log file to read
   -  ``int`` ``max_rotation``
      Limits what previously rotated files will be considered by the
      number given in the file name suffix. A value of 0 disables
      looking for rotated files. A value of 1 limits the rotated file to
      be that with the file name suffix of ``.old``. As only event logs
      are rotated, this parameter is only useful for event logs.
   -  ``bool`` ``check_for_rotated`` (*Optional with default* =
      ``false``)
      If ``true``, try to open the rotated files (with file names
      appended with ``.old`` or ``.1``, ``.2``, ...) first.
   -  ``bool`` ``read_only`` (*Optional with default* = ``false``)
      If ``true``, the reader will open the file read-only and disable
      locking.

-  ``bool`` ReadUserLog::initialize(const FileState &state, bool
   read_only)
   **Synopsis:** Initialize to continue from a persisted reader state.
   **Returns:** ``bool``; ``true``: success, ``false``: failed
   **Method** parameters:

   -  ``const FileState &`` ``state``
      Reference to the persisted state to restore from
   -  ``bool`` ``read_only`` (*Optional with default* = ``false``)
      If ``true``, the reader will open the file read-only and disable
      locking.

-  ``bool`` ReadUserLog::initialize(const FileState &state, int
   max_rotation, bool read_only)
   **Synopsis:** Initialize to continue from a persisted reader state
   and set the rotation parameters.
   **Returns:** ``bool``; ``true``: success, ``false``: failed
   **Method** parameters:

   -  ``const FileState &`` ``state``
      Reference to the persisted state to restore from
   -  ``int`` ``max_rotation``
      Limits what previously rotated files will be considered by the
      number given in the file name suffix. A value of 0 disables
      looking for rotated files. A value of 1 limits the rotated file to
      be that with the file name suffix of ``.old``. As only event logs
      are rotated, this parameter is only useful for event logs.
   -  ``bool`` ``read_only`` (*Optional with default* = ``false``)
      If ``true``, the reader will open the file read-only and disable
      locking.

Primary Methods
---------------

-  ``ULogEventOutcome`` ReadUserLog::readEvent(ULogEvent \*& event)
   **Synopsis:** Read the next event from the log file.
   **Returns:** ``ULogEventOutcome``; Outcome of the log read attempt.
   ``ULogEventOutcome`` is an enumerated type.
   **Method** parameters:

   -  ``ULogEvent`` \*& ``event``
      Pointer to an ``ULogEvent`` that is allocated by this call to
      ReadUserLog::readEvent. If no event is allocated, this pointer is
      set to ``NULL``. Otherwise the event needs to be delete()ed by the
      application.

-  ``bool`` ReadUserLog::synchronize(void)
   **Synopsis:** Synchronize the log file if the last event read was an
   error. This safe guard function should be called if there is some
   error reading an event, but there are events after it in the file. It
   will skip over the bad event, meaning it will read up to and
   including the event separator, so that the rest of the events can be
   read.
   **Returns:** ``bool``; ``true``: success, ``false``: failed
   **Method** parameters:

   -  None.

Accessors
---------

-  ``ReadUserLog::FileStatus`` ReadUserLog::CheckFileStatus(void)
   **Synopsis:** Check the status of the file, and whether it has grown,
   shrunk, etc.
   **Returns:** ``ReadUserLog::FileStatus``; the status of the log file,
   an enumerated type.
   **Method** parameters:

   -  None.

-  ``ReadUserLog::FileStatus`` ReadUserLog::CheckFileStatus(bool
   &is_empty)
   **Synopsis:** Check the status of the file, and whether it has grown,
   shrunk, etc.
   **Returns:** ``ReadUserLog::FileStatus``; the status of the log file,
   an enumerated type.
   **Method** parameters:

   -  ``bool &`` ``is_empty``
      Set to ``true`` if the file is empty, ``false`` otherwise.

Methods for saving and restoring persistent reader state
--------------------------------------------------------

The ``ReadUserLog::FileState`` structure is used to save and restore the
state of the ``ReadUserLog`` state for persistence. The application
should always use InitFileState() to initialize this structure.

All of these methods take a reference to a state buffer as their only
parameter.

All of these methods return ``true`` upon success.

Save state to persistent storage
--------------------------------

To save the state, do something like this:

.. code-block:: c++

    ReadUserLog                reader;
    ReadUserLog::FileState     statebuf;

    status = ReadUserLog::InitFileState( statebuf );

    status = reader.GetFileState( statebuf );
    write( fd, statebuf.buf, statebuf.size );
    ...
    status = reader.GetFileState( statebuf );
    write( fd, statebuf.buf, statebuf.size );
    ...

    status = UninitFileState( statebuf );

Restore state from persistent storage
-------------------------------------

To restore the state, do something like this:

.. code-block:: c++

    ReadUserLog::FileState     statebuf;
    status = ReadUserLog::InitFileState( statebuf );

    read( fd, statebuf.buf, statebuf.size );

    ReadUserLog                reader;
    status = reader.initialize( statebuf );

    status = UninitFileState( statebuf );
    ....

API Reference
-------------

-  static ``bool`` ReadUserLog::InitFileState(ReadUserLog::FileState
   &state)
   **Synopsis:** Initialize a file state buffer
   **Returns:** ``bool``; ``true`` if successful, ``false`` otherwise
   **Method** parameters:

   -  ``ReadUserLog::FileState &`` ``state``
      The file state buffer to initialize.

-  static ``bool`` ReadUserLog::UninitFileState(ReadUserLog::FileState
   &state)
   **Synopsis:** Clean up a file state buffer and free allocated memory
   **Returns:** ``bool``; ``true`` if successful, ``false`` otherwise
   **Method** parameters:

   -  ``ReadUserLog::FileState &`` ``state``
      The file state buffer to un-initialize.

-  ``bool`` ReadUserLog::GetFileState(ReadUserLog::FileState &state)
   ``const``
   **Synopsis:** Get the current state to persist it or save it off to
   disk
   **Returns:** ``bool``; ``true`` if successful, ``false`` otherwise
   **Method** parameters:

   -  ``ReadUserLog::FileState &`` ``state``
      The file state buffer to read the state into.

-  ``bool`` ReadUserLog::SetFileState(const ReadUserLog::FileState
   &state)
   **Synopsis:** Use this method to set the current state, after
   restoring it.
   NOTE: The state buffer is NOT automatically updated; a call MUST be
   made to the GetFileState() method each time before persisting the
   buffer to disk, or however else is chosen to persist its contents.
   **Returns:** ``bool``; ``true`` if successful, ``false`` otherwise
   **Method** parameters:

   -  ``const ReadUserLog::FileState &`` ``state``
      The file state buffer to restore from.

Access to the persistent state data
-----------------------------------

If the application needs access to the data elements in a persistent
state, it should instantiate a ``ReadUserLogStateAccess`` object.

-  Constructors / Destructors

   -  ReadUserLogStateAccess::ReadUserLogStateAccess(const
      ReadUserLog::FileState &state)
      **Synopsis:** Constructor default
      **Returns:** None
      **Constructor** parameters:

      -  ``const ReadUserLog::FileState &`` ``state``
         Reference to the persistent state data to initialize from.

   -  ReadUserLogStateAccess::˜ReadUserLogStateAccess(void)
      **Synopsis:** Destructor
      **Returns:** None
      **Destructor** parameters:

      -  None.

-  Accessor Methods

   -  ``bool`` ReadUserLogFileState::isInitialized(void) ``const``
      **Synopsis:** Checks if the buffer initialized
      **Returns:** ``bool``; ``true`` if successfully initialized,
      ``false`` otherwise
      **Method** parameters:

      -  None.

   -  ``bool`` ReadUserLogFileState::isValid(void) ``const``
      **Synopsis:** Checks if the buffer is valid for use by
      ReadUserLog::initialize()
      **Returns:** ``bool``; ``true`` if successful, ``false`` otherwise
      **Method** parameters:

      -  None.

   -  ``bool`` ReadUserLogFileState::getFileOffset(unsigned long &pos)
      ``const``
      **Synopsis:** Get position within individual file.
      NOTE: Can return an error if the result is too large to be stored
      in a ``long``.
      **Returns:** ``bool``; ``true`` if successful, ``false`` otherwise
      **Method** parameters:

      -  ``unsigned long &`` ``pos``
         Byte position within the current log file

   -  ``bool`` ReadUserLogFileState::getFileEventNum(unsigned long &num)
      ``const``
      **Synopsis:** Get event number in individual file.
      NOTE: Can return an error if the result is too large to be stored
      in a ``long``.
      **Returns:** ``bool``; ``true`` if successful, ``false`` otherwise
      **Method** parameters:

      -  ``unsigned long &`` ``num``
         Event number of the current event in the current log file

   -  ``bool`` ReadUserLogFileState::getLogPosition(unsigned long &pos)
      ``const``
      **Synopsis:** Position of the start of the current file in overall
      log.
      NOTE: Can return an error if the result is too large to be stored
      in a ``long``.
      **Returns:** ``bool``; ``true`` if successful, ``false`` otherwise
      **Method** parameters:

      -  ``unsigned long &`` ``pos``
         Byte offset of the start of the current file in the overall
         logical log stream.

   -  bool ReadUserLogFileState::getEventNumber(unsigned long &num)
      ``const``
      **Synopsis:** Get the event number of the first event in the
      current file
      NOTE: Can return an error if the result is too large to be stored
      in a ``long``.
      **Returns:** bool; ``true`` if successful, ``false`` otherwise
      **Method** parameters:

      -  ``unsigned long &`` ``num``
         This is the absolute event number of the first event in the
         current file in the overall logical log stream.

   -  bool ReadUserLogFileState::getUniqId(char \*buf, int size)
      ``const``
      **Synopsis:** Get the unique ID of the associated state file.
      **Returns:** bool; ``true`` if successful, ``false`` otherwise
      **Method** parameters:

      -  ``char *``\ ``buf``
         Buffer to fill with the unique ID of the current file.
      -  ``int`` ``size``
         Size in bytes of ``buf``.
         This is to prevent ReadUserLogFileState::getUniqId from writing
         past the end of ``buf``.

   -  ``bool`` ReadUserLogFileState::getSequenceNumber(int &seqno)
      ``const``
      **Synopsis:** Get the sequence number of the associated state
      file.
      **Returns:** ``bool``; ``true`` if successful, ``false`` otherwise
      **Method** parameters:

      -  ``int &`` ``seqno``
         Sequence number of the current file

-  Comparison Methods

   -  ``bool`` ReadUserLogFileState::getFileOffsetDiff(const
      ReadUserLogStateAccess &other, unsigned long &pos) ``const``
      **Synopsis:** Get the position difference of two states given by
      ``this`` and ``other``.
      NOTE: Can return an error if the result is too large to be stored
      in a ``long``.
      **Returns:** ``bool``; ``true`` if successful, ``false`` otherwise
      **Method** parameters:

      -  ``const ReadUserLogStateAccess &`` ``other``
         Reference to the state to compare to.
      -  ``long &`` ``diff``
         Difference in the positions

   -  bool ReadUserLogFileState::getFileEventNumDiff(const
      ReadUserLogStateAccess &other, long &diff) ``const``
      **Synopsis:** Get event number in individual file.
      NOTE: Can return an error if the result is too large to be stored
      in a ``long``.
      **Returns:** bool; ``true`` if successful, ``false`` otherwise
      **Method** parameters:

      -  ``const ReadUserLogStateAccess &`` ``other``
         Reference to the state to compare to.
      -  ``long &`` ``diff``
         Event number of the current event in the current log file

   -  bool ReadUserLogFileState::getLogPosition(const
      ReadUserLogStateAccess &other, long &diff) ``const``
      **Synopsis:** Get the position difference of two states given by
      ``this`` and ``other``.
      NOTE: Can return an error if the result is too large to be stored
      in a ``long``.
      **Returns:** bool; ``true`` if successful, ``false`` otherwise
      **Method** parameters:

      -  ``const ReadUserLogStateAccess &`` ``other``
         Reference to the state to compare to.
      -  ``long &`` ``diff``
         Difference between the byte offset of the start of the current
         file in the overall logical log stream and that of ``other``.

   -  bool ReadUserLogFileState::getEventNumber(const
      ReadUserLogStateAccess &other, long &diff) ``const``
      **Synopsis:** Get the difference between the event number of the
      first event in two state buffers (this - other).
      NOTE: Can return an error if the result is too large to be stored
      in a ``long``.
      **Returns:** bool; ``true`` if successful, ``false`` otherwise
      **Method** parameters:

      -  ``const ReadUserLogStateAccess &`` ``other``
         Reference to the state to compare to.
      -  ``long &`` ``diff``
         Difference between the absolute event number of the first event
         in the current file in the overall logical log stream and that
         of ``other``.

Future persistence API
----------------------

The ``ReadUserLog::FileState`` will likely be replaced with a new C++
``ReadUserLog::NewFileState``, or a similarly named class that will self
initialize.

Additionally, the functionality of ``ReadUserLogStateAccess`` will be
integrated into this class.


