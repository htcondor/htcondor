# Task Design

This file outlines the current task

## Goals

1. Update Librarian to use new ArchiveReader
2. Improve Librarian to better track and read archive files
    1. Key feature to re-track files correctly on daemon start


## Technical/Architecture

1. Replace use of src/archive_librarian/read_history.h/cpp with src/condor_utils/archive_reader.h/cpp
2. Keep reader objects open until done reading an archive file completely i.e. read to EOF of a rotated archive file
3. Update ArchiveReader to clear EOF for continual reading of main file or freshly rotated file that was already read to EOF
4. **Always** read archive files in forwards mode
5. **Always** read oldest files first
6. Update Librarian file tracking code to correctly re-track files in Database upon restart

## Requirements/Constraints

- Only modify code in src/archive_librarian directory, src/condor_utils/archive_reader.h, and src/condor_utils/archive_reader.cpp
- Ensure src/condor_unit_tests/OTEST_ArchiveReader.cpp is update to reflect any changes to ArchiveReader and ArchiveRecord classes
- Do not change database schema

## Hints

- None
