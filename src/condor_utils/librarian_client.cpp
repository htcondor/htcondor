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

#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"

#include "librarian_client.h"

#ifdef HAVE_SQLITE3_H
#include <sqlite3.h>
#endif

// ---- PIMPL implementation struct ----------------------------------------

struct LibrarianClient::Impl {
#ifdef HAVE_SQLITE3_H
	sqlite3* db{nullptr};

	~Impl() {
		if (db) {
			sqlite3_close(db);
			db = nullptr;
		}
	}

	// Run a COUNT(*) query with one integer bind parameter.
	// Appends the optional extra_filter clause (e.g. "AND f.DateOfDeletion IS NULL")
	// before executing.
	int countQuery(const char* base_sql, const char* extra_filter, int param) const {
		std::string sql = base_sql;
		if (extra_filter) { sql += extra_filter; }
		sqlite3_stmt* stmt = nullptr;
		if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
			dprintf(D_ERROR, "LibrarianClient: prepare failed: %s\n", sqlite3_errmsg(db));
			return 0;
		}
		std::ignore = sqlite3_bind_int(stmt, 1, param);
		int count = 0;
		if (sqlite3_step(stmt) == SQLITE_ROW) {
			count = sqlite3_column_int(stmt, 0);
		}
		std::ignore = sqlite3_finalize(stmt);
		return count;
	}

	// Run a COUNT(*) query with one text bind parameter.
	int countQueryText(const char* base_sql, const char* extra_filter, const std::string& param) const {
		std::string sql = base_sql;
		if (extra_filter) { sql += extra_filter; }
		sqlite3_stmt* stmt = nullptr;
		if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
			dprintf(D_ERROR, "LibrarianClient: prepare failed: %s\n", sqlite3_errmsg(db));
			return 0;
		}
		std::ignore = sqlite3_bind_text(stmt, 1, param.c_str(), -1, SQLITE_STATIC);
		int count = 0;
		if (sqlite3_step(stmt) == SQLITE_ROW) {
			count = sqlite3_column_int(stmt, 0);
		}
		std::ignore = sqlite3_finalize(stmt);
		return count;
	}
#endif // HAVE_SQLITE3_H
};

// ---- LibrarianClient public interface -----------------------------------

LibrarianClient::LibrarianClient() : m_impl(std::make_unique<Impl>()) {
#ifdef HAVE_SQLITE3_H
	if ( ! param_boolean("USING_LIBRARIAN", false)) { return; }

	std::string db_path;
	param(db_path, "LIBRARIAN_DATABASE");
	if (db_path.empty()) { return; }

	if (sqlite3_open_v2(db_path.c_str(), &m_impl->db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
		dprintf(D_ERROR, "LibrarianClient: failed to open '%s': %s\n",
		        db_path.c_str(), sqlite3_errmsg(m_impl->db));
		std::ignore = sqlite3_close(m_impl->db);
		m_impl->db = nullptr;
	}
#endif
}

LibrarianClient::~LibrarianClient() = default;

bool LibrarianClient::IsValid() const {
#ifdef HAVE_SQLITE3_H
	return m_impl && m_impl->db != nullptr;
#else
	return false;
#endif
}

int LibrarianClient::CountByCluster([[maybe_unused]] int cluster_id, [[maybe_unused]] FileFilter filter) const {
#ifdef HAVE_SQLITE3_H
	if ( ! IsValid()) { return 0; }

	static const char* base_sql =
		"SELECT COUNT(*) FROM JobRecords jr"
		" JOIN Files f    ON jr.FileId    = f.FileId"
		" JOIN JobLists jl ON jr.JobListId = jl.JobListId"
		" WHERE jl.ClusterId = ?";
	const char* extra = (filter == FileFilter::OnlyExisting)
		? " AND f.DateOfDeletion IS NULL" : nullptr;
	return m_impl->countQuery(base_sql, extra, cluster_id);
#else
	return 0;
#endif
}

int LibrarianClient::CountByUser([[maybe_unused]] const std::string& username, [[maybe_unused]] FileFilter filter) const {
#ifdef HAVE_SQLITE3_H
	if ( ! IsValid()) { return 0; }

	static const char* base_sql =
		"SELECT COUNT(*) FROM JobRecords jr"
		" JOIN Files f    ON jr.FileId    = f.FileId"
		" JOIN JobLists jl ON jr.JobListId = jl.JobListId"
		" JOIN Users u    ON jl.UserId    = u.UserId"
		" WHERE u.UserName = ?";
	const char* extra = (filter == FileFilter::OnlyExisting)
		? " AND f.DateOfDeletion IS NULL" : nullptr;
	return m_impl->countQueryText(base_sql, extra, username);
#else
	return 0;
#endif
}

std::vector<LibrarianRecord> LibrarianClient::GetRecords([[maybe_unused]] const std::vector<std::pair<int,int>>& job_ids) const {
	std::vector<LibrarianRecord> results;
#ifdef HAVE_SQLITE3_H
	if ( ! IsValid()) { return results; }

	static const char* sql =
		"SELECT jr.Offset, f.FileName FROM JobRecords jr"
		" JOIN Files f ON jr.FileId = f.FileId"
		" JOIN Jobs j  ON jr.JobId  = j.JobId"
		" WHERE j.ClusterId = ? AND j.ProcId = ?";

	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(m_impl->db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
		dprintf(D_ERROR, "LibrarianClient: prepare failed: %s\n", sqlite3_errmsg(m_impl->db));
		return results;
	}

	for (const auto& [cluster, proc] : job_ids) {
		std::ignore = sqlite3_reset(stmt);
		std::ignore = sqlite3_bind_int(stmt, 1, cluster);
		std::ignore = sqlite3_bind_int(stmt, 2, proc);
		while (sqlite3_step(stmt) == SQLITE_ROW) {
			LibrarianRecord rec;
			rec.offset    = sqlite3_column_int64(stmt, 0);
			const char* fn = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
			rec.file_path = fn ? fn : "";
			results.push_back(std::move(rec));
		}
	}
	std::ignore = sqlite3_finalize(stmt);
#endif
	return results;
}
