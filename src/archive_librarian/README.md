# Archive Librarian (`condor_librarian`)

`condor_librarian` is an HTCondor daemon that ingests job completion records from history archive
files into a SQLite database. It runs as a long-lived process managed by `condor_master` and
updates incrementally on a configurable timer.

**Goals:**
1. Speed up job history queries by replacing sequential file scans with indexed DB lookups.
2. Make aggregate statistics easier to compute with structured relational storage.
3. Retain job data slightly longer than the flat-file archive retention window.

---

## File Structure

```
src/archive_librarian/
├── librarian_main.cpp      # DaemonCore entry point
├── librarian.cpp           # Core update-cycle orchestration
├── librarian.h             # Librarian class declaration
├── librarian_types.h       # Shared data structures (ArchiveFile, Status, StatusData)
├── dbHandler.cpp           # SQLite database operations
├── dbHandler.h             # DBHandler class declaration
├── config.hpp              # LibrarianConfig class and option enums
├── SavedQueries.h          # SQL strings: schema DDL and garbage-collection query
├── CMakeLists.txt
└── README.md

src/condor_utils/
├── archive_reader.cpp      # Low-level archive file reader (forward and backward)
└── archive_reader.h        # ArchiveReader and ArchiveRecord declarations
```

---

## Architecture Overview

### Daemon Lifecycle (`librarian_main.cpp`)

`condor_librarian` integrates with DaemonCore:

| Callback | Purpose |
|----------|---------|
| `main_init` | Calls `librarian.reconfig(true)`, `librarian.initialize()`, registers the periodic `update_timer` |
| `main_config` | Calls `librarian.reconfig()` on `condor_reconfig` |
| `update_timer` | Fires every `LIBRARIAN_UPDATE_INTERVAL` seconds; calls `librarian.update()` |
| `main_exit` | Calls `DC_Exit(0)` |

### Update Cycle (`librarian.cpp`)

Each timer tick runs `Librarian::update()` in five phases:

| Phase | What happens |
|-------|-------------|
| 1 | `findHistoryFiles()` discovers all current archive files on disk (oldest rotated first, active last) |
| 1.5 | `reconcileArchiveFiles()` registers new files and detects rotations by comparing inodes (Linux) or first-record hashes (Windows) |
| 2 | `std::erase_if` removes in-memory entries for files that have left disk; marks them `DateOfDeletion` in the DB; renames unexpectedly removed active files to `<name>.REMOVED` in the DB |
| 3 | For each unread file, `readJobRecords()` opens (or reuses) a persistent `ArchiveReader`, reads new records incrementally, and calls `dbHandler_.insertJobFileRecords()` to atomically insert records and update the file's offset |
| 4 | `cleanupDatabaseIfNeeded()` runs garbage collection if the DB exceeds `LIBRARIAN_HIGH_WATER_MARK`; GC deletes the oldest files (and their job records) until the DB shrinks below `LIBRARIAN_LOW_WATER_MARK` |
| 5 | `writeStatusAndData()` records per-cycle and rolling aggregate metrics |

### File Reading (`archive_reader.cpp`)

`ArchiveReader` wraps a `FILE*` and reads HTCondor archive files record-by-record. Each record
consists of key=value ClassAd body lines terminated by a `***`-prefixed banner line. The reader
supports:
- **Forward** reading with `SeekForward(offset)` for incremental ingestion.
- **Backward** reading (chunk-buffered) for reverse traversal.
- On Windows, the file is opened via `CreateFileA` with `FILE_SHARE_DELETE` so that
  `condor_schedd`'s `MoveFile()` rotation calls are never blocked.

Per-file read progress (byte offset) is tracked in `ArchiveFile::last_offset` and persisted to
`Files.LastOffset` in the DB so restarts resume exactly where they left off.

### Record Size Estimation

Each file tracks a Welford online running average of record sizes
(`ArchiveFile::avg_record_size`, `ArchiveFile::records_read`). After each update cycle these
per-file averages are combined into a global `EstimatedBytesPerJobInArchive_`, which drives the
backlog estimator and GC file-count calculation.

---

## Data Structures (`librarian_types.h`)

### `ArchiveFile`
In-memory state for one tracked archive file; keyed by full path in `m_archive_files`.

| Field | Description |
|-------|-------------|
| `reader` | Persistent `ArchiveReader`; held open between cycles; released when `fully_read` |
| `filename` | Basename only (e.g. `history` or `history.20241215T143022`) |
| `hash` | FNV-1a hash of the first record — used as a file identity fingerprint |
| `rotation_time` | Rotation timestamp string; empty while the file is still the active archive |
| `last_offset` | Byte offset of the next unread byte |
| `id` | DB `FileId` |
| `inode` | Inode number (0 on Windows) |
| `fully_read` | True once a rotated file's reader reaches clean EOF |
| `avg_record_size` | Welford running mean: bytes per record |
| `records_read` | Welford denominator: records seen so far |

### `Status`
Per-cycle metrics written to the `Status` DB table at the end of each update.

### `StatusData`
Rolling aggregate metrics (Welford means, totals) written to the `StatusData` DB table.

---

## Database Schema (`SavedQueries.h`)

Schema version is tracked via `PRAGMA user_version` (current: 1).

| Table | Purpose |
|-------|---------|
| `Files` | One row per tracked archive file; holds offset, rotation/deletion timestamps, `AvgRecordSize`, `RecordsRead` |
| `Users` | Unique job owners |
| `JobLists` | `(ClusterId, UserId)` associations |
| `Jobs` | One row per `(ClusterId, ProcId)` |
| `JobRecords` | Completion record location: `Offset`, `CompletionDate`, `FileId`, `JobId` |
| `Status` | Per-cycle metrics; rows older than 5 minutes are pruned by GC |
| `StatusData` | Single-row rolling aggregate (upserted each cycle) |

Garbage collection (`GC_QUERY_SQL`) targets files where `DateOfDeletion IS NOT NULL`, deletes
them oldest-first up to the calculated file limit, then cascades to `JobRecords` and `Jobs`.

---

## Configuration

`condor_librarian` reads its configuration from the HTCondor config system. Add it to the
pool configuration:

```ini
LIBRARIAN          = $(SBIN)/condor_librarian
DAEMON_LIST        = $(DAEMON_LIST) LIBRARIAN
LIBRARIAN_DATABASE = $(LOCAL_DIR)/librarian.db
```

### Config Knobs

| Knob | Default | Description |
|------|---------|-------------|
| `LIBRARIAN_DATABASE` | *(required)* | Path to the SQLite database file |
| `HISTORY` | *(inherited)* | Path to the active HTCondor history archive file |
| `LIBRARIAN_UPDATE_INTERVAL` | `5` | Seconds between update cycles |
| `LIBRARIAN_MAX_UPDATES_PER_CYCLE` | `100000` | Maximum records ingested per cycle |
| `LIBRARIAN_MAX_JOBS_CACHED` | `10000` | In-memory `(ClusterId, ProcId) → JobId` cache size |
| `LIBRARIAN_MAX_DATABASE_SIZE` | `2147483648` (2 GiB) | DB size limit in bytes |
| `LIBRARIAN_HIGH_WATER_MARK` | `0.97` | Fraction of size limit that triggers GC |
| `LIBRARIAN_LOW_WATER_MARK` | `0.80` | Fraction of size limit GC targets |

---

## Dependencies

- C++20
- SQLite3
- HTCondor DaemonCore (links against `condor_utils`)

---

## Related Links

- Project Report: [https://docs.google.com/document/d/1msO0zYkLzXs1MPPRfWmt5gBZxX1ZGw8SQpbsSJojxdo/edit?usp=sharing] (link will only work within UW Madison workspace emails)
- CHTC Fellowship Website: [Link to CHTC fellowship website, will be added when Fellows page is updated]
