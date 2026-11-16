/***************************************************************
 *
 * Copyright (C) 2026, Condor Team, Computer Sciences Department,
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
#include "condor_debug.h"
#include "condor_config.h"

#if defined(WITH_PLACEMENT) && defined(HAVE_SQLITE3_H)
#include <sqlite3.h>
#endif

#include "tokens_db.h"

static std::string tokens_db_file;

#if defined(WITH_PLACEMENT) && defined(HAVE_SQLITE3_H)

sqlite3* acquireTokensDbHandle()
{
	std::string cfg_db_file;
	int rc = 0;
	sqlite3* db = nullptr;
	char *db_err_msg = nullptr;
	std::string stmt_str;

	if (!param(cfg_db_file, "TOKENS_DATABASE")) {
		tokens_db_file.clear();
		return nullptr;
	}

	rc = sqlite3_open_v2(cfg_db_file.c_str(), &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
	if (rc != SQLITE_OK) {
		dprintf(D_ERROR, "Failed to open tokens database file %s: %s\n", cfg_db_file.c_str(), sqlite3_errmsg(db));
		return nullptr;
	}
	// Wait a short time if another daemon is holding a write lock instead
	// of failing immediately. Not too long, since we're probably in the
	// middle of authentication.
	sqlite3_busy_timeout(db, 2000);

	if (cfg_db_file != tokens_db_file) {
		// First time opening this database file. Check that our tables exist.
		rc = sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS idtokens_minting (token_iss TEXT, token_kid TEXT, token_jti TEXT PRIMARY KEY, token_iat INTEGER, token_exp INTEGER, token_sub TEXT, token_scope TEXT, capability INTEGER)", nullptr, nullptr, &db_err_msg);
		if (rc != SQLITE_OK) {
			dprintf(D_ERROR, "Failed to create tokens db table: %s\n", db_err_msg);
			sqlite3_close(db);
			free(db_err_msg);
			return nullptr;
		}
		rc = sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS idtokens_minting_extra_claims (token_jti TEXT, claim TEXT, value TEXT, FOREIGN KEY (token_jti) REFERENCES idtokens_minting (token_jti), PRIMARY KEY (token_jti, claim))", nullptr, nullptr, &db_err_msg);
		if (rc != SQLITE_OK) {
			dprintf(D_ERROR, "Failed to create extra_claims db table: %s\n", db_err_msg);
			sqlite3_close(db);
			free(db_err_msg);
			return nullptr;
		}

		tokens_db_file = cfg_db_file;
	}

	return db;
}

void releaseTokensDbHandle(sqlite3* db)
{
	// TODO Abort any active transaction?

	sqlite3_close(db);
}

#endif

bool LookupToken([[maybe_unused]] const std::string& jti, [[maybe_unused]] const std::string& kid, [[maybe_unused]] TokenInfo& token)
{
#if ! (defined(WITH_PLACEMENT) && defined(HAVE_SQLITE3_H))
	return false;
#else
	int rc = 0;
	bool found = false;
	bool db_success = false;
	sqlite3* db = nullptr;
	sqlite3_stmt* stmt = nullptr;

	db = acquireTokensDbHandle();
	if (db == nullptr) {
		return false;
	}

	rc = sqlite3_prepare_v2(db, "SELECT token_sub, token_scope FROM idtokens_minting WHERE token_kid=? AND token_jti=? ;", -1, &stmt, nullptr);
	if (rc != SQLITE_OK) {
		goto db_done;
	}
	if (sqlite3_bind_text(stmt, 1, kid.c_str(), -1, SQLITE_STATIC) != SQLITE_OK ||
		sqlite3_bind_text(stmt, 2, jti.c_str(), -1, SQLITE_STATIC) != SQLITE_OK)
	{
		goto db_done;
	}

	while ( (rc = sqlite3_step(stmt)) == SQLITE_ROW ) {
		found = true;
		const char* value;
		value = (const char*)sqlite3_column_text(stmt, 0);
		token.sub = value ? value : "";
		value = (const char*)sqlite3_column_text(stmt, 1);
		token.scope = value ? value : "";
	}
	if (rc != SQLITE_DONE) {
		goto db_done;
	}

	sqlite3_finalize(stmt);
	stmt = nullptr;

	rc = sqlite3_prepare_v2(db, "SELECT claim, value FROM idtokens_minting_extra_claims WHERE token_jti=? ;", -1, &stmt, nullptr);
	if (rc != SQLITE_OK) {
		goto db_done;
	}
	if (sqlite3_bind_text(stmt, 1, jti.c_str(), -1, SQLITE_STATIC) != SQLITE_OK)
	{
		goto db_done;
	}

	while ( (rc = sqlite3_step(stmt)) == SQLITE_ROW ) {
		const char* claim = (const char*)sqlite3_column_text(stmt, 0);
		const char* value = (const char*)sqlite3_column_text(stmt, 1);
		if (claim && value) {
			token.extra_claims[claim] = value;
		}
	}
	if (rc != SQLITE_DONE) {
		goto db_done;
	}

	db_success = true;

 db_done:
	if (!db_success) {
		dprintf(D_ERROR, "Tokens database failure: %d %s\n", sqlite3_extended_errcode(db), sqlite3_errmsg(db));
	}
	sqlite3_finalize(stmt);
	releaseTokensDbHandle(db);

	return db_success && found;
#endif
}

bool AddToken([[maybe_unused]] const TokenInfo& token)
{
#if ! (defined(WITH_PLACEMENT) && defined(HAVE_SQLITE3_H))
	return false;
#else
	sqlite3* db = acquireTokensDbHandle();
	if (db == nullptr) {
		return false;
	}

	bool db_success = false;
	int rc = 0;
	sqlite3_stmt* pstmt = nullptr;
	std::string stmt_str;

	rc = sqlite3_prepare_v2(db, "INSERT INTO idtokens_minting (token_iss, token_kid, token_jti, token_iat, token_exp, token_sub, token_scope, capability) VALUES (?, ?, ?, ?, ?, ?, ?, ?);", -1, &pstmt, nullptr);
	if (rc != SQLITE_OK) {
		goto db_done;
	}
	if (sqlite3_bind_text(pstmt, 1, token.iss.c_str(), -1, SQLITE_STATIC) != SQLITE_OK ||
		sqlite3_bind_text(pstmt, 2, token.kid.c_str(), -1, SQLITE_STATIC) != SQLITE_OK ||
		sqlite3_bind_text(pstmt, 3, token.jti.c_str(), -1, SQLITE_STATIC) != SQLITE_OK ||
		sqlite3_bind_int64(pstmt, 4, (sqlite3_int64)token.iat) != SQLITE_OK ||
		sqlite3_bind_int64(pstmt, 5, (sqlite3_int64)token.exp) != SQLITE_OK ||
		sqlite3_bind_text(pstmt, 6, token.sub.c_str(), -1, SQLITE_STATIC) != SQLITE_OK ||
		sqlite3_bind_text(pstmt, 7, token.scope.c_str(), -1, SQLITE_STATIC) != SQLITE_OK ||
		sqlite3_bind_int(pstmt, 8, (int)token.cap) != SQLITE_OK)
	{
		goto db_done;
	}
	rc = sqlite3_step(pstmt);
	if (rc != SQLITE_DONE) {
		goto db_done;
	}
	sqlite3_finalize(pstmt);
	pstmt = nullptr;

	for (auto& [claim, value]: token.extra_claims) {
		rc = sqlite3_prepare_v2(db, "INSERT INTO idtokens_minting_extra_claims (token_jti, claim, value) VALUES (?, ?, ?);", -1, &pstmt, nullptr);
		if (rc != SQLITE_OK) {
			goto db_done;
		}
		if (sqlite3_bind_text(pstmt, 1, token.jti.c_str(), -1, SQLITE_STATIC) != SQLITE_OK ||
			sqlite3_bind_text(pstmt, 2, claim.c_str(), -1, SQLITE_STATIC) != SQLITE_OK ||
			sqlite3_bind_text(pstmt, 3, value.c_str(), -1, SQLITE_STATIC) != SQLITE_OK)
		{
			goto db_done;
		}
		if (sqlite3_step(pstmt) != SQLITE_DONE) {
			goto db_done;
		}
		sqlite3_finalize(pstmt);
		pstmt = nullptr;
	}

	db_success = true;

 db_done:
	if (!db_success) {
		dprintf(D_ERROR, "Tokens database failure: %d %s\n", sqlite3_extended_errcode(db), sqlite3_errmsg(db));
	}
	sqlite3_finalize(pstmt);
	releaseTokensDbHandle(db);
	return db_success;
#endif
}
