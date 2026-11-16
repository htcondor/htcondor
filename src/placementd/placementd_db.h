/***************************************************************
 *
 * Copyright (C) 2026, Center for High Throughput Computing
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

#ifndef PLACEMENTD_DB_H
#define PLACEMENTD_DB_H

// SQLite data layer for the condor_placementd: the normalized identity,
// account, project and authorization tables, and the audit log, that live
// in TOKENS_DATABASE next to the token log.  The placementd is the only
// writer of the placementd_* tables.

#include "condor_common.h"
#include "condor_classad.h"
#include <sqlite3.h>

#include <exception>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// Schema version stored in placementd_meta under 'schema_version'.
constexpr int PLACEMENTD_SCHEMA_VERSION = 2;

// Value of PlacementdApiVersion in the daemon ad.
constexpr int PLACEMENTD_API_VERSION = 2;

// Owners of a row (the source column).  The map files own 'mapfile' rows;
// the database (edited over CEDAR) owns 'internal' ones.
inline constexpr const char *PLACEMENT_SOURCE_MAPFILE = "mapfile";
inline constexpr const char *PLACEMENT_SOURCE_INTERNAL = "internal";

// Error codes on the wire.  1-12 predate the database.
enum PlacementErrorCode {
	PLACEMENT_ERR_NOT_ENCRYPTED = 1,
	PLACEMENT_ERR_MISSING_USERNAME = 2,
	PLACEMENT_ERR_NOT_AUTHORIZED = 3,
	PLACEMENT_ERR_BAD_AUTHORIZATIONS = 4,
	PLACEMENT_ERR_EXPIRED = 6,
	PLACEMENT_ERR_UNKNOWN_REQUESTER = 7,
	PLACEMENT_ERR_REQUESTER_NOT_INSTRUCTOR = 8,
	PLACEMENT_ERR_REQUESTER_EXPIRED = 9,
	PLACEMENT_ERR_PROJECT_NOT_ALLOWED = 10,
	PLACEMENT_ERR_DATABASE = 11,
	PLACEMENT_ERR_SCHEDD = 12,
	PLACEMENT_ERR_NOT_FOUND = 13,
	PLACEMENT_ERR_VERSION = 14,
	PLACEMENT_ERR_OWNER = 15,
	PLACEMENT_ERR_INVALID = 16,
	PLACEMENT_ERR_REFERENCED = 17,
	PLACEMENT_ERR_DUPLICATE = 18,
	PLACEMENT_ERR_UNKNOWN_TYPE = 19,
};

// Thrown by the data layer; caught at the command and reconcile boundaries,
// where the enclosing transaction is rolled back.
class PlacementError : public std::exception
{
public:
	PlacementError(int code, std::string message)
		: m_code(code), m_message(std::move(message)) {}
	int code() const { return m_code; }
	const char *what() const noexcept override { return m_message.c_str(); }
private:
	int m_code;
	std::string m_message;
};

// The four entity types.  Values index the tables in placementd_db.cpp.
enum class PlacementEntity { Identity = 0, Account, Project, Authorization };

// "Identity" etc. to the enum; false for anything else.
bool PlacementEntityFromName(const std::string &name, PlacementEntity &out);
const char *PlacementEntityName(PlacementEntity t);
const char *PlacementEntityTable(PlacementEntity t);
// The natural key: attribute "Identity" and column identity for an
// Identity, attribute "Name" and column name for everything else.
const char *PlacementEntityKeyAttr(PlacementEntity t);
const char *PlacementEntityKeyColumn(PlacementEntity t);

// A new random (v4) uuid in lower-case RFC 4122 text form.
std::string PlacementNewUuid();

// Canonicalize an AP account name the way the map file does: "*" is
// replaced by the identity up to its last '@' (only when identity is
// given), and a name without '@' gets "@<uid_domain>" appended.
std::string PlacementCanonicalAccount(const std::string &name,
	const std::string &identity, const std::string &uid_domain);

// Caps on names, in bytes.  An identity is an IdP's subject and issuer, so
// it gets more room.
constexpr size_t PLACEMENTD_MAX_NAME = 256;
constexpr size_t PLACEMENTD_MAX_IDENTITY = 1024;
// Caps on free text, in bytes, after PlacementSanitize(): a label, color or
// display name, and a description or comment.
constexpr size_t PLACEMENTD_MAX_SHORT_TEXT = 256;
constexpr size_t PLACEMENTD_MAX_LONG_TEXT = 4096;

// What may be minted.  A name is valid UTF-8 within its cap, and may not
// contain '"', '\', whitespace, or a control or format (bidi, zero-width)
// character; account, project and authorization names may not contain ','
// either (they travel in comma lists), and an account name must be
// user@domain, with a user other than '*'.  On failure, why says what is
// wrong.
bool PlacementValidName(PlacementEntity t, const std::string &name, std::string &why);
// Subjects a token must never carry, compared case-insensitively:
// condor@*, root@*, condor_pool@*; anything in the domains password,
// child, parent, family, matchsession and unmapped; and the schedd's
// queue super users (QUEUE_SUPER_USERS, OCU_SUPER_USERS, and the user a
// personal condor runs as) in every domain: their user names are matched
// and their domains ignored, and %group names the members of a Unix group.
bool PlacementReservedAccount(const std::string &account);
// Read the super-user lists from the configuration; call at startup and
// on reconfig.
void PlacementConfigureReserved();
// The configured reserved subjects as one string, so that a change to
// them can force a fresh reconcile.
std::string PlacementReservedSignature();
// Authorization names that are HTCondor permission levels other than READ
// and WRITE (ADMINISTRATOR, DAEMON, ...).  They may not be granted.
bool PlacementPrivilegedAuthorization(const std::string &name);
// Make text safe to store, show and log: drop bytes that are not valid
// UTF-8 and control and format characters (Unicode categories Cc and Cf,
// which include the bidi overrides and zero-width characters), and cap
// the length at a character boundary.
std::string PlacementSanitize(const std::string &in, size_t max_bytes);

// The wire ModifiedBy: the actor, plus the end user it named, as
// "<actor> on behalf of <on_behalf_of>".
std::string PlacementModifiedBy(const std::string &actor, const std::optional<std::string> &on_behalf_of);

// RAII wrapper for a prepared statement.  Every failure throws
// PlacementError: a constraint violation with the matching wire code,
// anything else with PLACEMENT_ERR_DATABASE.
class PlacementStmt
{
public:
	PlacementStmt(sqlite3 *db, const char *sql);
	~PlacementStmt();
	PlacementStmt(const PlacementStmt &) = delete;
	PlacementStmt &operator=(const PlacementStmt &) = delete;

	PlacementStmt &bind(int idx, const std::string &val);
	PlacementStmt &bind(int idx, const char *val);	// nullptr binds NULL
	PlacementStmt &bind(int idx, long long val);
	PlacementStmt &bind(int idx, const std::optional<long long> &val);
	PlacementStmt &bind(int idx, const std::optional<std::string> &val);
	PlacementStmt &bindNull(int idx);

	// Returns true for SQLITE_ROW, false for SQLITE_DONE; throws otherwise.
	bool step();
	// step() for statements that return no rows; returns sqlite3_changes().
	int exec();
	// Ready the statement for reuse: reset it and clear its bindings.  This
	// also releases the read lock a statement holds until it is reset or
	// has returned its last row.
	void reset();

	bool isNull(int col) const;
	long long getInt(int col) const;
	std::string getText(int col) const;
	std::optional<long long> getOptInt(int col) const;
	std::optional<std::string> getOptText(int col) const;

private:
	sqlite3 *m_db{nullptr};
	sqlite3_stmt *m_stmt{nullptr};
};

// Prepared statements kept for reuse, keyed by their SQL.  Get() returns
// the statement reset and ready to bind.  A caller must be done with a
// statement (stepped to the end, or what it needs copied out) before
// anything else asks for the same SQL.
//
// A statement left part way through its rows holds a read lock on the
// database, which blocks every other daemon's writes to it (token
// minting among them).  ResetAll() releases them; PlacementLockRelease
// does so at the end of a scope.
class PlacementStatements
{
public:
	explicit PlacementStatements(sqlite3 *db) : m_db(db) {}
	PlacementStmt &Get(const std::string &sql);
	void ResetAll();
	sqlite3 *handle() const { return m_db; }
private:
	sqlite3 *m_db;
	std::map<std::string, std::unique_ptr<PlacementStmt>> m_cache;
};

class PlacementLockRelease
{
public:
	explicit PlacementLockRelease(PlacementStatements &stmts) : m_stmts(stmts) {}
	~PlacementLockRelease() { m_stmts.ResetAll(); }
	PlacementLockRelease(const PlacementLockRelease &) = delete;
	PlacementLockRelease &operator=(const PlacementLockRelease &) = delete;
private:
	PlacementStatements &m_stmts;
};

// Release every read lock this connection holds: reset the cached
// statements, and log any other statement that is still part way through
// its rows (a bug: it keeps the lock).  Call before sending a reply, so
// that a slow client cannot block writers, and before BEGIN IMMEDIATE.
// SQLite does not wait for a lock (it fails at once with "database is
// locked") when the connection that wants to write already holds a read
// lock, since waiting could deadlock.
void PlacementReleaseReadLocks(PlacementStatements &stmts, const char *where);

// One BEGIN IMMEDIATE ... COMMIT.  Releases the connection's read locks
// first.  Rolls back on destruction unless commit() was called.
class PlacementTxn
{
public:
	explicit PlacementTxn(PlacementStatements &stmts);
	~PlacementTxn();
	PlacementTxn(const PlacementTxn &) = delete;
	PlacementTxn &operator=(const PlacementTxn &) = delete;
	void commit();
	void rollback();
private:
	sqlite3 *m_db;
	bool m_open{false};
};

// An account's grants.  Projects come default first, then in grant order;
// authorizations in grant order.
struct PlacementGrants
{
	struct Item
	{
		long long id{0};
		std::string uuid;
		std::string name;
	};
	std::vector<Item> authorizations;
	std::vector<Item> projects;
	long long default_project{0};	// project id; 0 = none

	std::vector<long long> AuthorizationIds() const;
	std::vector<long long> ProjectIds() const;
	std::vector<std::string> AuthorizationNames() const;
	std::vector<std::string> ProjectNames() const;
	std::string DefaultProjectName() const;		// empty if none
};

void PlacementReadGrants(PlacementStatements &stmts, long long account_id, PlacementGrants &out);
// Replace an account's grants.  default_project is 0 or one of projects.
void PlacementWriteGrants(PlacementStatements &stmts, long long account_id,
	const std::vector<long long> &authorizations, const std::vector<long long> &projects,
	long long default_project);

// Rows that refer to an entity: the identities on an account, or the
// accounts granted a project or authorization.  Identities have none.
struct PlacementReferrer
{
	long long id{0};
	std::string uuid;
	std::string name;
	std::string source;
};
std::vector<PlacementReferrer> PlacementReferrers(PlacementStatements &stmts, PlacementEntity t,
	long long row_id);

// The writable fields of one entity row, as wire attributes (absent when
// NULL).  An empty ad if the row does not exist.  The audit log records
// the difference between two of these.
ClassAd PlacementSnapshot(PlacementStatements &stmts, PlacementEntity t, long long row_id);

// Who makes a change set.
struct PlacementChangesetInfo
{
	std::string channel;	// 'cedar' or 'mapfile'
	std::string actor;		// authenticated peer, or 'mapfile'
	std::optional<std::string> on_behalf_of;
	std::optional<std::string> peer_address;
	std::optional<std::string> comment;
};

// What a token was minted for, as the token log records it.
enum class PlacementTokenBasis { Identity, Account };

// Writes the audit log for one change set: a PLACEMENT_EDIT batch or a
// reconcile.  The change set is inserted with the first modification, so a
// change set that modifies nothing writes no audit records.  Must be used
// inside the transaction that makes the changes.
class PlacementAudit
{
public:
	PlacementAudit(PlacementStatements &stmts, PlacementChangesetInfo info)
		: m_stmts(stmts), m_info(std::move(info)), m_now(time(nullptr)) {}

	// Record one modification of an entity around its write.  The
	// modification is inserted first; write(modification_id) then makes the
	// change, storing modification_id on the row, and returns the row's id
	// (0 once the row is deleted).  The modification's changes are the
	// difference between the row's fields before and after.  Returns the
	// modification id, which is the row's new Version.  An 'update' that
	// changes no field is undone: no record is kept, the row keeps its
	// Version, and that Version is returned.
	long long Modify(PlacementEntity t, long long row_id, const std::string &uuid,
		const char *operation, const std::function<long long(long long)> &write);
	// As above, with the "before" fields taken earlier by the caller, for a
	// row whose visible fields change as a side effect of another write
	// (an account whose project is renamed).
	long long Modify(PlacementEntity t, long long row_id, const std::string &uuid,
		const char *operation, const ClassAd &before, const std::function<long long(long long)> &write);

	long long ChangesetId() const { return m_changeset_id; }

	// Revoke the unexpired tokens minted for an identity or an account (by
	// uuid): delete them from the authentication layer's tables, so that
	// they no longer validate.  With outliving, only the tokens that expire
	// after it.  The token log keeps its rows, as the record of what was
	// issued.  Call from inside a Modify() write: the count is recorded in
	// that modification's Changes as RevokedTokens.  Returns the count.
	long long RevokeTokens(PlacementTokenBasis basis, const std::string &uuid,
		std::optional<long long> outliving = std::nullopt);
	// Tokens revoked so far in this change set.
	long long RevokedTokens() const { return m_revoked_total; }

private:
	PlacementStatements &m_stmts;
	PlacementChangesetInfo m_info;
	time_t m_now;
	long long m_changeset_id{0};
	int m_count{0};
	long long m_revoked{0};			// by the current Modify()
	long long m_revoked_total{0};
};

// Login view of one identity, read with one query per login.
struct PlacementLoginInfo
{
	std::string identity_uuid;
	time_t expires{0};			// 0 = never
	std::string account_uuid;
	std::string account_name;
	PlacementGrants grants;
};

// What the legacy PLACEMENT_QUERY_USERS reply needs for one identity.
struct PlacementLegacyUser
{
	std::string identity;
	std::string account;
	std::string authorizations;	// comma-joined
	std::string projects;		// comma-joined, default first
	time_t expires{0};			// 0 = never
};

struct PlacementLegacyAuthz
{
	std::string name;
	std::string label;
	std::string color;
	std::string description;
};

class PlacementDb
{
public:
	~PlacementDb();

	// Open the database, set the busy timeout and foreign_keys, create the
	// token log and migrate to the current schema.  EXCEPTs on failure,
	// including a database that claims the current schema version but
	// lacks its tables or columns.
	void Open(const std::string &path);
	void Close();
	sqlite3 *handle() const { return m_db; }
	PlacementStatements &Statements() { return *m_stmts; }

	bool GetMeta(const std::string &key, std::string &value);
	void SetMeta(const std::string &key, const std::string &value);

	// See PlacementReleaseReadLocks().
	void ReleaseReadLocks(const char *where) { PlacementReleaseReadLocks(*m_stmts, where); }

	// Login and legacy-command reads.
	bool LookupLogin(const std::string &identity, PlacementLoginInfo &out);
	void ListLegacyUsers(const std::string &identity, std::vector<PlacementLegacyUser> &out);
	void ListRequestableAuthorizations(std::vector<PlacementLegacyAuthz> &out);

	// PLACEMENT_QUERY: full result ads for one entity type, optionally
	// restricted to one uuid or one natural key.
	void EntityAds(PlacementEntity t, const std::string &uuid, const std::string &name,
		std::vector<ClassAd> &out);

	// PLACEMENT_QUERY of TargetType Modification: newest first, optionally
	// only those of one entity.  limit < 0 means no limit.
	void ModificationAds(const std::string &entity_uuid, long long limit, std::vector<ClassAd> &out);

private:
	void Migrate();
	// EXCEPT unless the tables and columns of the current schema exist.
	void CheckShape();
	bool HasTokenLogIndexes();

	sqlite3 *m_db{nullptr};
	std::unique_ptr<PlacementStatements> m_stmts;
};

// Build a ClassAd list of strings.
void PlacementAssignStringList(ClassAd &ad, const char *attr, const std::vector<std::string> &list);

#endif
