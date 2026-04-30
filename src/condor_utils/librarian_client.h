/***************************************************************
 *
 * Copyright (C) 1990-2025, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// A single indexed record: the byte offset within the file and the file path.
struct LibrarianRecord {
	int64_t     offset;
	std::string file_path;
};

// Read-only client for the Librarian SQLite3 index database.
//
// Callers never need compile-time sqlite3 checks — create an instance and
// call IsValid() before using any query methods.  All query methods are
// no-ops (returning 0 or empty) when IsValid() returns false.
class LibrarianClient {
public:
	enum class FileFilter { All, OnlyExisting };

	LibrarianClient();
	~LibrarianClient();

	LibrarianClient(const LibrarianClient&)            = delete;
	LibrarianClient& operator=(const LibrarianClient&) = delete;
	LibrarianClient(LibrarianClient&&)                 = default;
	LibrarianClient& operator=(LibrarianClient&&)      = default;

	// Returns true iff sqlite3 was compiled in, USING_LIBRARIAN=True,
	// and the database file was opened successfully.
	bool IsValid() const;

	// Count JobRecords associated with a cluster ID.
	// With OnlyExisting (default), excludes records in files that have
	// been marked deleted (DateOfDeletion IS NOT NULL).
	int CountByCluster(int cluster_id, FileFilter filter = FileFilter::OnlyExisting) const;

	// Count JobRecords associated with a username.
	// Same file-existence filtering as CountByCluster.
	int CountByUser(const std::string& username, FileFilter filter = FileFilter::OnlyExisting) const;

	// Return the file offset and file path for each (cluster, proc) pair.
	std::vector<LibrarianRecord> GetRecords(const std::vector<std::pair<int,int>>& job_ids) const;

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;
};
