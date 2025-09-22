# Archive Librarian 
The Archive Librarian is a tool that aims to make dealing with historical job data from Condor easier. It ingests job record data from history files and writes it to a database - it can then answer queries and generate statistics for users. The goal of this project to be able to 
1) speed up job queries by avoiding the sequential scan mechanic that condor_history uses
2) make generating aggregates/statistics easier by storing job info in a structured database, like SQLite
3) allow information about finished jobs to persist slightly longer after they are deleted from the flat file history archive 

## Current Status
The Archive Librarian is currently not a finished product. However, you can run a small demo by following the instructions under "How to run the Archive Librarian". 

**Note**: The system is currently hard-coded to read both epoch records and history records, but this should be made configurable in future versions. 

## Main Files

### File Structure
```
├── librarian.cpp
├── readHistory.cpp  
├── dbHandler.cpp
├── archiveMonitor.cpp
├── JobAnalysisUtils.cpp
├── librarian_config.txt
├── hist/
│   └── [test history files]
└── README.md
```

### librarian.cpp
Core orchestration code that handles query execution, file management, and coordinates between file reading operations, database insertions, and querying interface functionality.

### readHistory.cpp
Handles the low-level reading and parsing of HTCondor history files. May later be replaced by an 'ArchiveReader' class, which will hopefully be more generic. Does not currently use ClassAds. 

### dbHandler.cpp
Manages SQLite database operations, including reading and writing. 

### archiveMonitor.cpp
Monitors and manages HTCondor history file archives. Provides the main functionality for noticing and handling file rotation. 

### JobAnalysisUtils.cpp
Contains parsing utilities and output formatting functions for queries. 

### SavedQueries.h
Contains important SQL code:\
    'SCHEMA_SQL'    - defines the schema of the database, including its tables, indexes, and constraints\
    'GC_QUERY_SQL'  - defines the garbage collection policy for the database\

These queries are read in and saved upon initialization of the Librarian() class. 

### librarian_config.txt
Currently the file that contains the configurations for the librarian file. These configs include:\
- epoch_history_path : a path to an epoch history file
- history_path : a path to a history file
- db_path : a path to where we want the database to go (the librarian creates the database at this location, with this name at startup)
- the Librarian is initialized with other configurations that are currently hardcoded into main.cpp (dbSizeLimit, jobCacheSize, schemaVersionNumber)

### FileSet.h
File management structures - Defines the 'FileSet' struct to help the librarian and archiveMonitor track collections of history files 

### JobRecord.h
Core data structures that manage ingestion and insertion - Fundamental structs that store data for 'Jobs', 'JobRecords', 'Files', 'Status, and other structs. Composes the internal data model throughout
the system. 

### JobQueryStructures.h
Query data structures for the demo command line tool to use - Defines 'ParsedJobRecord', 'QueryResult', and other structs to help the command line interface manage queried data. 

# Architecture Overview
The librarian works by ingesting historical job data from the flat file archive and then writing it into a SQLite database. 
- The ingestion part of this cycle is handled by the readHistory.cpp file. 
- The writing part of this cycle is handled by the dbHandler.cpp file. 
- The archiveMonitor.cpp file holds utilities that allow the process to track and react to file rotation. 
- The librarian.cpp file holds the functionality that orchestrates all this different code working together. 

## Configuration
Config file (librarianc_config.txt) accepts these attributes:\
    epoch_history_path  - the path to the current epoch history file that's being written to\
    history_path        - the path to the current history file that's being written to\
    db_path             - the path to the database we'd like to write to (the librarian will build the database here, it shouldn't already exist)


The following attributes are not currently configurable, but could be made to be:\
    jobCacheSize        - the maximum number of jobs that the incache map of processed jobs will hold\
    dbSizeLimit         - the maximum size, in bytes, that the database will grow to 

## Dependencies
- C++20 or later
- SQLite3


## Future Development
- Epoch data added to database (EpochAds, TransferAds, SpawnAds)
- Job record projection table with data from the body of a JobRecord (for ex, ExitStatus)
- Daemonized process
- More thorough error propogation
- Safer database operations (possibly make a Transaction wrapper class that handles cleanup after a SQL statement is run)

# myHistoryList Demo Overview
myHistoryList serves as a demo command line tool to demonstrate what a fully fleshed out Archive Librarian could theoretically do. Because of how the Archive Librarian isn't currently daemonized, you'll note that it's functionality has limitations - you have to use the menu to choose '1. Update' to ingest a history file before users can test the query functionality. This tool serves more as a proof of concept than a working service. 

## Features
- Query jobs by user and cluster ID
- Multiple analysis modes (usage, files, batch, DAG, timing, location, status)
- Statistical summaries with min/max/mean/median calculations
- File reading with offset jumping rather than sequential scans
- Table output format

## How to run the Archive Librarian demo (myHistoryList)
```bash
# Build instructions here
make
```

## Usage

**Main Function**: Currently serves as a demo of database capabilities. To run queries, you must:
1. Run the program
2. Navigate to the menu 
3. Select "1. Update" to populate the database
4. Select "2. Queries" to run actual queries - the menu will prompt you when to enter your actual query

### Basic Query Syntax
```bash
-user <username> -clusterId <clusterid> [analysis_flags]
```

### Required Arguments
- `-user <username>` - Filter by job owner
- `-clusterId <clusterid>` - Filter by cluster ID

### Analysis Flags (Optional)
- `-usage` - Show resource usage statistics (memory, wall clock time)
- `-files` - Show file paths (output, error, log, submit files)
- `-batch` - Show batch-related information
- `-dag` - Show DAG-related information  
- `-when` - Show timing information (job start dates)
- `-where` - Show execution location information
- `-status` - Show job status and exit codes

### Examples

#### Basic Query (Raw ClassAd Output)
```bash
-user aanand37 -clusterId 4327172
```

#### Single Analysis Mode
```bash
-user aanand37 -clusterId 4327172 -usage
```

#### Multiple Analysis Modes
```bash
-user aanand37 -clusterId 4327172 -usage -files -status
```

## Output Format

### Usage Analysis
- Table showing memory usage (MB) and wall clock time (seconds)
- Statistics: min, max, mean, median, total

### Files Analysis  
- Table showing output files, error files, user logs, and submit files

### Status Analysis
- Table showing job status, exit codes, and exit status
- Distribution statistics with counts and percentages

### Other Analysis Modes (-dag, -batch, -where, -when)
- Table format with other relevant job information
- Summary statistics where applicable 

## Performance
- Query execution times are logged to `librarian_query_times.txt`
- Offset-based file reading theoretically speeds up query retrieval time

### Data Flow / How the Demo Works
1. Query database for job offsets
2. Read from archive files at specified offsets  
3. Parse ClassAd records into structured data
4. Apply analysis filters and generate output


## Related Links
- Project Report: [https://docs.google.com/document/d/1msO0zYkLzXs1MPPRfWmt5gBZxX1ZGw8SQpbsSJojxdo/edit?usp=sharing] (link will only work within UW Madison workspace emails)
- CHTC Fellowship Website: [Link to CHTC fellowship website, will be added when Fellows page is updated]
