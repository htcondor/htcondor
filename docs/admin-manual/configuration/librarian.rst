.. _librarian-config:

Librarian Configuration Options
===============================

The following options effect the operations of the archive librarian. Automatically
enable historical archive record indexing to a database by setting :macro:`use feature:librarian`.

:macro-def:`USING_LIBRARIAN`
    A boolean value representing whether or not the archive librarian is in
    use for tools to utilize for faster archive queries. Defaults to ``False``.

:macro-def:`LIBRARIAN`
    The path to the *condor_librarian* executable. Defaults to ``$(SBIN)/condor_librarian``.

:macro-def:`LIBRARIAN_LOG`
    The path to the archive librarian's debug file. Defaults to ``$(LOG)/LibrarianLog``.

:macro-def:`MAX_LIBRARIAN_LOG`
    The maximum size of the archive librarian's debug file before being rotated. Defaults
    to :macro:`MAX_DEBUG_LOG`.

:macro-def:`LIBRARIAN_DEBUG`
    The archive librarian's debugging level. Default is nothing.

:macro-def:`LIBRARIAN_DATABASE`
    The path to where the archive librarian's SQLite3 database file. Defaults to
    ``$(SPOOL)/library.db``.

:macro-def:`LIBRARIAN_MAX_DATABASE_SIZE`
    A 64-bit integer representing the maximum size of the archive librarian's database
    file. This defaults to ``2GB``.

:macro-def:`LIBRARIAN_HIGH_WATER_MARK`
    A floating point number between the range of 0 and 1 that represents a percent value
    threshold of garbage collection trigger size in the database i.e. a max size of ``2GB``
    with a high water mark of ``0.75`` would trigger garbage collection once the database
    becomes ``1.5GB`` or larger. Default is ``0.97``.

:macro-def:`LIBRARIAN_LOW_WATER_MARK`
    A floating point number between the range of 0 and 1 that represents a percent value
    threshold to attempt reducing the archive librarian's database size during garbage
    collection i.e. a max size of ``2GB`` with a low water mark of ``0.5`` will try to
    reduce the database size to or below ``1GB`` during garbage collection. Default is
    ``0.8``.

:macro-def:`LIBRARIAN_MAX_JOBS_CACHED`
    An integer value representing the maximum number of job id information to database
    reference id's to cache in memory. Defaults to ``10,000``.

:macro-def:`LIBRARIAN_MAX_UPDATES_PER_CYCLE`
    A 64-bit integer representing the maximum number of new archive record index insertions
    into the archive librarian's database per update interval. Defaults to ``100,000``.

:macro-def:`LIBRARIAN_STATUS_RETENTION_SECONDS`
    An integer value representing the lifetime in seconds of per update interval status
    entries within the archive librarian's database. Defaults to ``300`` or 5 minutes.

:macro-def:`LIBRARIAN_UPDATE_INTERVAL`
    An integer value representing the time interval in seconds between the archive
    librarians main actions of scanning archive files for new records and updating
    the database with indexes. Defaults to ``5``.
