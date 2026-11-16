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

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_attributes.h"
#include "condor_config.h"
#include "condor_perms.h"
#include "condor_uid.h"
#include "stl_string_utils.h"
#include "placementd_db.h"
#include "classad/jsonSink.h"
#include "classad/jsonSource.h"

#include <uuid/uuid.h>
#if !defined(WIN32)
#include <grp.h>
#endif

#include <set>

namespace {

const char *const ENTITY_NAMES[] = { "Identity", "Account", "Project", "Authorization" };
const char *const ENTITY_KEY_ATTRS[] = { "Identity", "Name", "Name", "Name" };
const char *const ENTITY_KEY_COLUMNS[] = { "identity", "name", "name", "name" };
const char *const ENTITY_AUDIT_TYPES[] = { "identity", "account", "project", "authorization" };
const char *const ENTITY_TABLES[] = {
	"placementd_identities", "placementd_accounts", "placementd_projects", "placementd_authorizations" };

// The version 2 objects, created in one transaction by Migrate().
const char *const SCHEMA_V2[] = {
	// The audit log.  An entity row refers to its latest modification; a
	// modification deliberately has no foreign key to its entity, so it
	// outlives a deleted one.
	"CREATE TABLE placementd_changesets ("
		" id INTEGER PRIMARY KEY AUTOINCREMENT,"
		" at INTEGER NOT NULL,"
		" channel TEXT NOT NULL CHECK (channel IN ('cedar','mapfile')),"
		" actor TEXT NOT NULL,"
		" on_behalf_of TEXT,"
		" peer_address TEXT,"
		" comment TEXT)",
	"CREATE TABLE placementd_modifications ("
		" id INTEGER PRIMARY KEY AUTOINCREMENT,"
		" changeset_id INTEGER NOT NULL REFERENCES placementd_changesets(id),"
		" entity_type TEXT NOT NULL CHECK (entity_type IN ('identity','account','project','authorization')),"
		" entity_uuid TEXT NOT NULL,"
		" operation TEXT NOT NULL CHECK (operation IN ('create','update','delete','adopt','reclaim','handoff')),"
		" changes TEXT NOT NULL)",
	"CREATE INDEX placementd_modifications_entity ON placementd_modifications(entity_uuid, id)",
	"CREATE TABLE placementd_accounts ("
		" id INTEGER PRIMARY KEY,"
		" uuid TEXT NOT NULL UNIQUE,"
		" name TEXT NOT NULL UNIQUE,"
		" source TEXT NOT NULL CHECK (source IN ('mapfile','internal')),"
		" modification_id INTEGER NOT NULL REFERENCES placementd_modifications(id))",
	"CREATE TABLE placementd_identities ("
		" id INTEGER PRIMARY KEY,"
		" uuid TEXT NOT NULL UNIQUE,"
		" identity TEXT NOT NULL UNIQUE,"
		" account_id INTEGER NOT NULL REFERENCES placementd_accounts(id) ON DELETE RESTRICT,"
		" expires_at INTEGER,"
		" display_name TEXT,"
		" comment TEXT,"
		" source TEXT NOT NULL CHECK (source IN ('mapfile','internal')),"
		" modification_id INTEGER NOT NULL REFERENCES placementd_modifications(id))",
	"CREATE INDEX placementd_identities_account ON placementd_identities(account_id)",
	"CREATE TABLE placementd_projects ("
		" id INTEGER PRIMARY KEY,"
		" uuid TEXT NOT NULL UNIQUE,"
		" name TEXT NOT NULL UNIQUE COLLATE NOCASE,"
		" description TEXT,"
		" source TEXT NOT NULL CHECK (source IN ('mapfile','internal')),"
		" modification_id INTEGER NOT NULL REFERENCES placementd_modifications(id))",
	"CREATE TABLE placementd_authorizations ("
		" id INTEGER PRIMARY KEY,"
		" uuid TEXT NOT NULL UNIQUE,"
		" name TEXT NOT NULL UNIQUE COLLATE NOCASE,"
		" label TEXT NOT NULL,"
		" color TEXT NOT NULL DEFAULT 'normal',"
		" description TEXT NOT NULL DEFAULT '',"
		" requestable INTEGER NOT NULL DEFAULT 1 CHECK (requestable IN (0,1)),"
		" source TEXT NOT NULL CHECK (source IN ('mapfile','internal')),"
		" modification_id INTEGER NOT NULL REFERENCES placementd_modifications(id))",
	"CREATE TABLE placementd_account_authorizations ("
		" account_id INTEGER NOT NULL REFERENCES placementd_accounts(id) ON DELETE CASCADE,"
		" authorization_id INTEGER NOT NULL REFERENCES placementd_authorizations(id) ON DELETE RESTRICT,"
		" PRIMARY KEY (account_id, authorization_id))",
	"CREATE TABLE placementd_account_projects ("
		" account_id INTEGER NOT NULL REFERENCES placementd_accounts(id) ON DELETE CASCADE,"
		" project_id INTEGER NOT NULL REFERENCES placementd_projects(id) ON DELETE RESTRICT,"
		" is_default INTEGER NOT NULL DEFAULT 0 CHECK (is_default IN (0,1)),"
		" PRIMARY KEY (account_id, project_id))",
	("CREATE UNIQUE INDEX placementd_account_default_project"
		" ON placementd_account_projects(account_id) WHERE is_default = 1"),
	// The token log's foreign_id column (its name predates this schema)
	// holds the identity.
	"CREATE INDEX IF NOT EXISTS placementd_tokens_identity ON placementd_tokens(foreign_id, token_exp)",
	"CREATE INDEX IF NOT EXISTS placementd_tokens_jti ON placementd_tokens(token_jti)",
};

const char *const TOKEN_LOG_COLUMNS[] = { "identity_uuid", "account_uuid", "project_uuid" };

// Revocation finds a token log's rows by these.  Created on every start, so
// that a database migrated before they existed gets them too.
const char *const TOKEN_LOG_INDEXES[] = {
	"CREATE INDEX IF NOT EXISTS placementd_tokens_identity_uuid ON placementd_tokens(identity_uuid, token_exp)",
	"CREATE INDEX IF NOT EXISTS placementd_tokens_account_uuid ON placementd_tokens(account_uuid, token_exp)",
};

// What a version 2 database must have; a database from before the audit
// log claims version 2 without some of it.
const char *const SCHEMA_V2_TABLES[] = {
	"placementd_changesets", "placementd_modifications", "placementd_accounts", "placementd_identities",
	"placementd_projects", "placementd_authorizations", "placementd_account_authorizations",
	"placementd_account_projects",
};

// Users and domains that are never minted, beyond the configured super
// users.  Domains: the pool's own (password, family), the ones that
// daemons and sessions authenticate as (child, parent, matchsession), and
// unmapped peers.
const char *const RESERVED_USERS[] = { "condor", "root", "condor_pool" };
const char *const RESERVED_DOMAINS[] = { "password", "child", "parent", "family", "matchsession", "unmapped" };

struct ReservedSubjects
{
	std::vector<std::string> super_users;	// as configured: user, user@domain or %group
};
ReservedSubjects reserved_subjects;

// A Unix group's members, by name.
bool
group_has_member(const std::string &group, const std::string &user)
{
#if defined(WIN32)
	(void)group; (void)user;
	return false;
#else
	struct group *gr = getgrnam(group.c_str());
	if (!gr) {
		return false;
	}
	for (char **member = gr->gr_mem; member && *member; ++member) {
		if (strcasecmp(*member, user.c_str()) == 0) {
			return true;
		}
	}
	return false;
#endif
}

void
exec_sql(sqlite3 *db, const char *sql)
{
	char *err = nullptr;
	if (sqlite3_exec(db, sql, nullptr, nullptr, &err) != SQLITE_OK) {
		std::string msg = err ? err : sqlite3_errmsg(db);
		sqlite3_free(err);
		throw PlacementError(PLACEMENT_ERR_DATABASE, msg);
	}
}

void
assign_common(ClassAd &ad, PlacementStmt &stmt, int first_col)
{
	// source, modification_id, updated_at, actor, on_behalf_of, created_at
	ad.Assign("Source", stmt.getText(first_col));
	ad.Assign("Version", stmt.getInt(first_col + 1));
	ad.Assign("ModificationId", stmt.getInt(first_col + 1));
	ad.Assign("UpdatedAt", stmt.getInt(first_col + 2));
	ad.Assign("ModifiedBy", PlacementModifiedBy(stmt.getText(first_col + 3), stmt.getOptText(first_col + 4)));
	ad.Assign("CreatedAt", stmt.getInt(first_col + 5));
}

// {"Field": [old, new], ...} for the fields that differ; null for absent.
std::string
diff_json(const ClassAd &before, const ClassAd &after)
{
	std::set<std::string, classad::CaseIgnLTStr> attrs;
	for (const auto &entry: before) { attrs.insert(entry.first); }
	for (const auto &entry: after) { attrs.insert(entry.first); }

	classad::ClassAdUnParser unparser;
	ClassAd changes;
	for (const auto &attr: attrs) {
		const classad::ExprTree *old_expr = before.Lookup(attr);
		const classad::ExprTree *new_expr = after.Lookup(attr);
		std::string old_str, new_str;
		if (old_expr) { unparser.Unparse(old_str, old_expr); }
		if (new_expr) { unparser.Unparse(new_str, new_expr); }
		if ((old_expr != nullptr) == (new_expr != nullptr) && old_str == new_str) {
			continue;
		}
		classad::Value undefined;
		undefined.SetUndefinedValue();
		std::vector<classad::ExprTree *> pair;
		pair.push_back(old_expr ? old_expr->Copy() : classad::Literal::MakeLiteral(undefined));
		pair.push_back(new_expr ? new_expr->Copy() : classad::Literal::MakeLiteral(undefined));
		changes.Insert(attr, classad::ExprList::MakeExprList(pair));
	}
	std::string json;
	classad::ClassAdJsonUnParser json_unparser(true);
	json_unparser.Unparse(json, &changes);
	return json;
}

// The UTF-8 sequence at in[pos]: its length, or 0 if it is not valid (a
// stray byte, an overlong form, a surrogate or past U+10FFFF).
size_t
utf8_sequence(const std::string &in, size_t pos, char32_t &cp)
{
	unsigned char c = (unsigned char)in[pos];
	size_t len = 0;
	char32_t min = 0;
	if (c < 0x80) {
		cp = c;
		return 1;
	} else if ((c & 0xE0) == 0xC0) {
		len = 2; cp = c & 0x1F; min = 0x80;
	} else if ((c & 0xF0) == 0xE0) {
		len = 3; cp = c & 0x0F; min = 0x800;
	} else if ((c & 0xF8) == 0xF0) {
		len = 4; cp = c & 0x07; min = 0x10000;
	} else {
		return 0;
	}
	if (pos + len > in.size()) {
		return 0;
	}
	for (size_t i = 1; i < len; i++) {
		unsigned char cc = (unsigned char)in[pos + i];
		if ((cc & 0xC0) != 0x80) {
			return 0;
		}
		cp = (cp << 6) | (cc & 0x3F);
	}
	if (cp < min || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
		return 0;
	}
	return len;
}

// Unicode general categories Cc (controls) and Cf (format characters: the
// bidi embeddings, overrides and isolates, zero-width characters, and the
// like), which can make text read differently from what it is.
bool
control_or_format(char32_t cp)
{
	if (cp < 0x20 || (cp >= 0x7F && cp <= 0x9F)) {
		return true;
	}
	static const char32_t format[][2] = {
		{0x00AD, 0x00AD}, {0x0600, 0x0605}, {0x061C, 0x061C}, {0x06DD, 0x06DD}, {0x070F, 0x070F},
		{0x0890, 0x0891}, {0x08E2, 0x08E2}, {0x180E, 0x180E}, {0x200B, 0x200F}, {0x202A, 0x202E},
		{0x2060, 0x2064}, {0x2066, 0x206F}, {0xFEFF, 0xFEFF}, {0xFFF9, 0xFFFB}, {0x110BD, 0x110BD},
		{0x110CD, 0x110CD}, {0x13430, 0x1343F}, {0x1BCA0, 0x1BCA3}, {0x1D173, 0x1D17A},
		{0xE0001, 0xE0001}, {0xE0020, 0xE007F},
	};
	for (const auto &range: format) {
		if (cp >= range[0] && cp <= range[1]) {
			return true;
		}
	}
	return false;
}

std::vector<long long>
item_ids(const std::vector<PlacementGrants::Item> &items)
{
	std::vector<long long> out;
	for (const auto &item: items) { out.push_back(item.id); }
	return out;
}

std::vector<std::string>
item_names(const std::vector<PlacementGrants::Item> &items)
{
	std::vector<std::string> out;
	for (const auto &item: items) { out.push_back(item.name); }
	return out;
}

} // namespace

// ---------------------------------------------------------------- names

bool
PlacementEntityFromName(const std::string &name, PlacementEntity &out)
{
	for (int i = 0; i < 4; i++) {
		if (strcasecmp(name.c_str(), ENTITY_NAMES[i]) == 0) {
			out = static_cast<PlacementEntity>(i);
			return true;
		}
	}
	return false;
}

const char *
PlacementEntityName(PlacementEntity t)
{
	return ENTITY_NAMES[static_cast<int>(t)];
}

const char *
PlacementEntityTable(PlacementEntity t)
{
	return ENTITY_TABLES[static_cast<int>(t)];
}

const char *
PlacementEntityKeyAttr(PlacementEntity t)
{
	return ENTITY_KEY_ATTRS[static_cast<int>(t)];
}

const char *
PlacementEntityKeyColumn(PlacementEntity t)
{
	return ENTITY_KEY_COLUMNS[static_cast<int>(t)];
}

std::string
PlacementNewUuid()
{
	uuid_t uuid;
	char uuid_str[37];
	uuid_generate_random(uuid);
	uuid_unparse_lower(uuid, uuid_str);
	return std::string(uuid_str, 36);
}

std::string
PlacementCanonicalAccount(const std::string &name, const std::string &identity,
	const std::string &uid_domain)
{
	std::string result = name;
	if (result == "*" && !identity.empty()) {
		auto at = identity.find_last_of('@');
		result = identity.substr(0, at);
	}
	if (result.find('@') == std::string::npos) {
		result += "@" + uid_domain;
	}
	return result;
}

bool
PlacementValidName(PlacementEntity t, const std::string &name, std::string &why)
{
	if (name.empty()) {
		why = "is empty";
		return false;
	}
	size_t cap = (t == PlacementEntity::Identity) ? PLACEMENTD_MAX_IDENTITY : PLACEMENTD_MAX_NAME;
	if (name.size() > cap) {
		formatstr(why, "is longer than %zu bytes", cap);
		return false;
	}
	for (size_t pos = 0; pos < name.size(); ) {
		char32_t cp = 0;
		size_t len = utf8_sequence(name, pos, cp);
		if (len == 0) {
			why = "is not valid UTF-8";
			return false;
		}
		pos += len;
		if (cp == '"' || cp == '\\') {
			why = "contains a quote or backslash";
			return false;
		}
		if ((cp < 0x80 && isspace((int)cp)) || control_or_format(cp)) {
			why = "contains whitespace, or a control or format character";
			return false;
		}
		if (cp == ',' && t != PlacementEntity::Identity) {
			why = "contains a comma";
			return false;
		}
	}
	if (t == PlacementEntity::Account) {
		auto at = name.find('@');
		if (at == std::string::npos || at == 0 || at == name.size() - 1 ||
			name.find('@', at + 1) != std::string::npos) {
			why = "is not user@domain";
			return false;
		}
		if (name.compare(0, at, "*") == 0) {
			why = "has '*' for its user";
			return false;
		}
	}
	return true;
}

bool
PlacementReservedAccount(const std::string &account)
{
	auto at = account.find('@');
	std::string user = account.substr(0, at);
	std::string domain = (at == std::string::npos) ? "" : account.substr(at + 1);
	for (const char *reserved_user: RESERVED_USERS) {
		if (strcasecmp(user.c_str(), reserved_user) == 0) {
			return true;
		}
	}
	for (const char *reserved_domain: RESERVED_DOMAINS) {
		if (strcasecmp(domain.c_str(), reserved_domain) == 0) {
			return true;
		}
	}

	// The schedd's super users, by user name alone.  The schedd qualifies
	// a bare name with its own UID_DOMAIN, which need not be the domain
	// accounts are made in, and matches one domain as a prefix of another;
	// an account that differs from a super user only in its domain could
	// still be one.  So the domain is ignored, and so is case.
	for (const auto &super_user: reserved_subjects.super_users) {
		if (super_user[0] == '%') {
			// A Unix group: its members, as local users.
			if (group_has_member(super_user.substr(1), user)) {
				return true;
			}
			continue;
		}
		std::string super_name = super_user.substr(0, super_user.find('@'));
		if (!super_name.empty() && strcasecmp(user.c_str(), super_name.c_str()) == 0) {
			return true;
		}
	}
	return false;
}

void
PlacementConfigureReserved()
{
	ReservedSubjects cfg;
	std::string list;
	if (!param(list, "QUEUE_SUPER_USERS")) {
		list = "root";
	}
	for (const auto &name: StringTokenIterator(list)) {
		cfg.super_users.emplace_back(name);
	}
	if (param(list, "OCU_SUPER_USERS")) {
		for (const auto &name: StringTokenIterator(list)) {
			cfg.super_users.emplace_back(name);
		}
	}
#if !defined(WIN32)
	// The schedd also makes the user it runs as a super user.
	if (param_boolean("PERSONAL_CONDOR_IS_SUPER_USER", true)) {
		const char *condor_user = get_condor_username();
		if (condor_user && *condor_user) {
			cfg.super_users.emplace_back(condor_user);
		}
	}
#endif
	reserved_subjects = std::move(cfg);
}

std::string
PlacementReservedSignature()
{
	return join(reserved_subjects.super_users, ",");
}

bool
PlacementPrivilegedAuthorization(const std::string &name)
{
	DCpermission perm = getPermissionFromString(name.c_str());
	return perm != NOT_A_PERM && perm != READ && perm != WRITE;
}

std::string
PlacementSanitize(const std::string &in, size_t max_bytes)
{
	std::string out;
	out.reserve(std::min(in.size(), max_bytes));
	for (size_t pos = 0; pos < in.size(); ) {
		char32_t cp = 0;
		size_t len = utf8_sequence(in, pos, cp);
		if (len == 0) {
			pos++;		// a stray byte
			continue;
		}
		if (!control_or_format(cp)) {
			if (out.size() + len > max_bytes) {
				break;
			}
			out.append(in, pos, len);
		}
		pos += len;
	}
	return out;
}

std::string
PlacementModifiedBy(const std::string &actor, const std::optional<std::string> &on_behalf_of)
{
	if (on_behalf_of && !on_behalf_of->empty()) {
		return actor + " on behalf of " + *on_behalf_of;
	}
	return actor;
}

void
PlacementAssignStringList(ClassAd &ad, const char *attr, const std::vector<std::string> &list)
{
	std::vector<classad::ExprTree *> exprs;
	exprs.reserve(list.size());
	for (const auto &item: list) {
		exprs.push_back(classad::Literal::MakeString(item));
	}
	ad.Insert(attr, classad::ExprList::MakeExprList(exprs));
}

// ---------------------------------------------------------------- PlacementStmt

PlacementStmt::PlacementStmt(sqlite3 *db, const char *sql)
	: m_db(db)
{
	int rc = sqlite3_prepare_v2(db, sql, -1, &m_stmt, nullptr);
	if (rc != SQLITE_OK) {
		std::string msg;
		formatstr(msg, "prepare failed: %s (%s)", sqlite3_errmsg(db), sql);
		throw PlacementError(PLACEMENT_ERR_DATABASE, msg);
	}
}

PlacementStmt::~PlacementStmt()
{
	sqlite3_finalize(m_stmt);
}

PlacementStmt &
PlacementStmt::bind(int idx, const std::string &val)
{
	if (sqlite3_bind_text(m_stmt, idx, val.c_str(), (int)val.size(), SQLITE_TRANSIENT) != SQLITE_OK) {
		throw PlacementError(PLACEMENT_ERR_DATABASE, sqlite3_errmsg(m_db));
	}
	return *this;
}

PlacementStmt &
PlacementStmt::bind(int idx, const char *val)
{
	if (!val) {
		return bindNull(idx);
	}
	return bind(idx, std::string(val));
}

PlacementStmt &
PlacementStmt::bind(int idx, long long val)
{
	if (sqlite3_bind_int64(m_stmt, idx, (sqlite3_int64)val) != SQLITE_OK) {
		throw PlacementError(PLACEMENT_ERR_DATABASE, sqlite3_errmsg(m_db));
	}
	return *this;
}

PlacementStmt &
PlacementStmt::bind(int idx, const std::optional<long long> &val)
{
	return val ? bind(idx, *val) : bindNull(idx);
}

PlacementStmt &
PlacementStmt::bind(int idx, const std::optional<std::string> &val)
{
	return val ? bind(idx, *val) : bindNull(idx);
}

PlacementStmt &
PlacementStmt::bindNull(int idx)
{
	if (sqlite3_bind_null(m_stmt, idx) != SQLITE_OK) {
		throw PlacementError(PLACEMENT_ERR_DATABASE, sqlite3_errmsg(m_db));
	}
	return *this;
}

bool
PlacementStmt::step()
{
	int rc = sqlite3_step(m_stmt);
	if (rc == SQLITE_ROW) {
		return true;
	}
	if (rc == SQLITE_DONE) {
		return false;
	}
	rc = sqlite3_extended_errcode(m_db);
	std::string msg;
	formatstr(msg, "%s (%d)", sqlite3_errmsg(m_db), rc);
	// Constraint failures are the caller's fault, not the database's.
	switch (rc) {
	case SQLITE_CONSTRAINT_UNIQUE:
	case SQLITE_CONSTRAINT_PRIMARYKEY:
		throw PlacementError(PLACEMENT_ERR_DUPLICATE, msg);
	case SQLITE_CONSTRAINT_FOREIGNKEY:
		throw PlacementError(PLACEMENT_ERR_REFERENCED, msg);
	case SQLITE_CONSTRAINT_CHECK:
	case SQLITE_CONSTRAINT_NOTNULL:
		throw PlacementError(PLACEMENT_ERR_INVALID, msg);
	default:
		throw PlacementError(PLACEMENT_ERR_DATABASE, msg);
	}
}

int
PlacementStmt::exec()
{
	while (step()) {}
	return sqlite3_changes(m_db);
}

void
PlacementStmt::reset()
{
	sqlite3_reset(m_stmt);
	sqlite3_clear_bindings(m_stmt);
}

bool
PlacementStmt::isNull(int col) const
{
	return sqlite3_column_type(m_stmt, col) == SQLITE_NULL;
}

long long
PlacementStmt::getInt(int col) const
{
	return (long long)sqlite3_column_int64(m_stmt, col);
}

std::string
PlacementStmt::getText(int col) const
{
	const char *str = (const char *)sqlite3_column_text(m_stmt, col);
	return str ? std::string(str) : std::string();
}

std::optional<long long>
PlacementStmt::getOptInt(int col) const
{
	if (isNull(col)) {
		return std::nullopt;
	}
	return getInt(col);
}

std::optional<std::string>
PlacementStmt::getOptText(int col) const
{
	if (isNull(col)) {
		return std::nullopt;
	}
	return getText(col);
}

PlacementStmt &
PlacementStatements::Get(const std::string &sql)
{
	auto it = m_cache.find(sql);
	if (it == m_cache.end()) {
		it = m_cache.emplace(sql, std::make_unique<PlacementStmt>(m_db, sql.c_str())).first;
	} else {
		it->second->reset();
	}
	return *it->second;
}

void
PlacementStatements::ResetAll()
{
	for (auto &entry: m_cache) {
		entry.second->reset();
	}
}

void
PlacementReleaseReadLocks(PlacementStatements &stmts, const char *where)
{
	stmts.ResetAll();
	sqlite3 *db = stmts.handle();
	for (sqlite3_stmt *stmt = sqlite3_next_stmt(db, nullptr); stmt; stmt = sqlite3_next_stmt(db, stmt)) {
		if (sqlite3_stmt_busy(stmt)) {
			dprintf(D_ERROR, "Placementd statement still holds a read lock at %s: %s\n", where, sqlite3_sql(stmt));
		}
	}
}

// ---------------------------------------------------------------- PlacementTxn

PlacementTxn::PlacementTxn(PlacementStatements &stmts)
	: m_db(stmts.handle())
{
	// With a read lock held, BEGIN IMMEDIATE would fail at once instead of
	// waiting out another connection's write.
	PlacementReleaseReadLocks(stmts, "BEGIN IMMEDIATE");
	exec_sql(m_db, "BEGIN IMMEDIATE");
	m_open = true;
}

PlacementTxn::~PlacementTxn()
{
	if (m_open) {
		rollback();
	}
}

void
PlacementTxn::commit()
{
	exec_sql(m_db, "COMMIT");
	m_open = false;
}

void
PlacementTxn::rollback()
{
	if (!m_open) {
		return;
	}
	m_open = false;
	char *err = nullptr;
	if (sqlite3_exec(m_db, "ROLLBACK", nullptr, nullptr, &err) != SQLITE_OK) {
		dprintf(D_ERROR, "Placementd database rollback failed: %s\n", err ? err : "(unknown)");
		sqlite3_free(err);
	}
}

// ---------------------------------------------------------------- grants

std::vector<long long> PlacementGrants::AuthorizationIds() const { return item_ids(authorizations); }
std::vector<long long> PlacementGrants::ProjectIds() const { return item_ids(projects); }
std::vector<std::string> PlacementGrants::AuthorizationNames() const { return item_names(authorizations); }
std::vector<std::string> PlacementGrants::ProjectNames() const { return item_names(projects); }

std::string
PlacementGrants::DefaultProjectName() const
{
	for (const auto &item: projects) {
		if (item.id == default_project) {
			return item.name;
		}
	}
	return "";
}

void
PlacementReadGrants(PlacementStatements &stmts, long long account_id, PlacementGrants &out)
{
	out = PlacementGrants();
	PlacementStmt &authz = stmts.Get("SELECT z.id, z.uuid, z.name FROM placementd_account_authorizations g"
		" JOIN placementd_authorizations z ON z.id = g.authorization_id"
		" WHERE g.account_id = ?1 ORDER BY g.rowid");
	authz.bind(1, account_id);
	while (authz.step()) {
		out.authorizations.push_back(PlacementGrants::Item{authz.getInt(0), authz.getText(1), authz.getText(2)});
	}
	PlacementStmt &proj = stmts.Get("SELECT p.id, p.uuid, p.name, g.is_default FROM placementd_account_projects g"
		" JOIN placementd_projects p ON p.id = g.project_id"
		" WHERE g.account_id = ?1 ORDER BY g.is_default DESC, g.rowid");
	proj.bind(1, account_id);
	while (proj.step()) {
		out.projects.push_back(PlacementGrants::Item{proj.getInt(0), proj.getText(1), proj.getText(2)});
		if (proj.getInt(3)) {
			out.default_project = proj.getInt(0);
		}
	}
}

void
PlacementWriteGrants(PlacementStatements &stmts, long long account_id,
	const std::vector<long long> &authorizations, const std::vector<long long> &projects,
	long long default_project)
{
	stmts.Get("DELETE FROM placementd_account_authorizations WHERE account_id = ?1").bind(1, account_id).exec();
	stmts.Get("DELETE FROM placementd_account_projects WHERE account_id = ?1").bind(1, account_id).exec();
	for (long long id: authorizations) {
		stmts.Get("INSERT INTO placementd_account_authorizations (account_id, authorization_id) VALUES (?1, ?2)")
			.bind(1, account_id).bind(2, id).exec();
	}
	for (long long id: projects) {
		stmts.Get("INSERT INTO placementd_account_projects (account_id, project_id, is_default) VALUES (?1, ?2, ?3)")
			.bind(1, account_id).bind(2, id).bind(3, (long long)(id == default_project ? 1 : 0)).exec();
	}
}

std::vector<PlacementReferrer>
PlacementReferrers(PlacementStatements &stmts, PlacementEntity t, long long row_id)
{
	const char *sql = nullptr;
	switch (t) {
	case PlacementEntity::Identity:
		return {};
	case PlacementEntity::Account:
		sql = "SELECT id, uuid, identity, source FROM placementd_identities WHERE account_id = ?1 ORDER BY identity";
		break;
	case PlacementEntity::Project:
		sql = "SELECT a.id, a.uuid, a.name, a.source FROM placementd_account_projects g"
			" JOIN placementd_accounts a ON a.id = g.account_id WHERE g.project_id = ?1 ORDER BY a.name";
		break;
	case PlacementEntity::Authorization:
		sql = "SELECT a.id, a.uuid, a.name, a.source FROM placementd_account_authorizations g"
			" JOIN placementd_accounts a ON a.id = g.account_id WHERE g.authorization_id = ?1 ORDER BY a.name";
		break;
	}
	std::vector<PlacementReferrer> out;
	PlacementStmt &stmt = stmts.Get(sql);
	stmt.bind(1, row_id);
	while (stmt.step()) {
		out.push_back(PlacementReferrer{stmt.getInt(0), stmt.getText(1), stmt.getText(2), stmt.getText(3)});
	}
	return out;
}

// ---------------------------------------------------------------- audit

ClassAd
PlacementSnapshot(PlacementStatements &stmts, PlacementEntity t, long long row_id)
{
	ClassAd ad;
	if (row_id == 0) {
		return ad;
	}
	auto opt_text = [&](PlacementStmt &stmt, int col, const char *attr) {
		if (!stmt.isNull(col)) {
			ad.Assign(attr, stmt.getText(col));
		}
	};
	switch (t) {
	case PlacementEntity::Identity: {
		PlacementStmt &stmt = stmts.Get("SELECT i.identity, a.name, i.expires_at, i.display_name, i.comment"
			" FROM placementd_identities i JOIN placementd_accounts a ON a.id = i.account_id WHERE i.id = ?1");
		stmt.bind(1, row_id);
		if (stmt.step()) {
			ad.Assign("Identity", stmt.getText(0));
			ad.Assign("Account", stmt.getText(1));
			if (!stmt.isNull(2)) {
				ad.Assign("ExpiresAt", stmt.getInt(2));
			}
			opt_text(stmt, 3, "DisplayName");
			opt_text(stmt, 4, "Comment");
		}
		break;
	}
	case PlacementEntity::Account: {
		PlacementStmt &stmt = stmts.Get("SELECT name FROM placementd_accounts WHERE id = ?1");
		stmt.bind(1, row_id);
		if (stmt.step()) {
			ad.Assign("Name", stmt.getText(0));
			PlacementGrants grants;
			PlacementReadGrants(stmts, row_id, grants);
			PlacementAssignStringList(ad, "Authorizations", grants.AuthorizationNames());
			PlacementAssignStringList(ad, "Projects", grants.ProjectNames());
			std::string default_project = grants.DefaultProjectName();
			if (!default_project.empty()) {
				ad.Assign("DefaultProject", default_project);
			}
		}
		break;
	}
	case PlacementEntity::Project: {
		PlacementStmt &stmt = stmts.Get("SELECT name, description FROM placementd_projects WHERE id = ?1");
		stmt.bind(1, row_id);
		if (stmt.step()) {
			ad.Assign("Name", stmt.getText(0));
			opt_text(stmt, 1, "Description");
		}
		break;
	}
	case PlacementEntity::Authorization: {
		PlacementStmt &stmt = stmts.Get("SELECT name, label, color, description, requestable"
			" FROM placementd_authorizations WHERE id = ?1");
		stmt.bind(1, row_id);
		if (stmt.step()) {
			ad.Assign("Name", stmt.getText(0));
			ad.Assign("Label", stmt.getText(1));
			ad.Assign("Color", stmt.getText(2));
			ad.Assign("Description", stmt.getText(3));
			ad.Assign("Requestable", stmt.getInt(4) != 0);
		}
		break;
	}
	}
	return ad;
}

long long
PlacementAudit::Modify(PlacementEntity t, long long row_id, const std::string &uuid,
	const char *operation, const std::function<long long(long long)> &write)
{
	return Modify(t, row_id, uuid, operation, PlacementSnapshot(m_stmts, t, row_id), write);
}

long long
PlacementAudit::Modify(PlacementEntity t, long long row_id, const std::string &uuid,
	const char *operation, const ClassAd &before, const std::function<long long(long long)> &write)
{
	// Write order: the change set, the modification, then the row that
	// refers to it.
	bool new_changeset = false;
	if (m_changeset_id == 0) {
		m_stmts.Get("INSERT INTO placementd_changesets (at, channel, actor, on_behalf_of, peer_address, comment)"
			" VALUES (?1, ?2, ?3, ?4, ?5, ?6)")
			.bind(1, (long long)m_now).bind(2, m_info.channel).bind(3, m_info.actor).bind(4, m_info.on_behalf_of)
			.bind(5, m_info.peer_address).bind(6, m_info.comment).exec();
		m_changeset_id = sqlite3_last_insert_rowid(m_stmts.handle());
		new_changeset = true;
	}

	long long old_modification_id = 0;
	if (row_id != 0) {
		std::string sql;
		formatstr(sql, "SELECT modification_id FROM %s WHERE id = ?1", PlacementEntityTable(t));
		PlacementStmt &stmt = m_stmts.Get(sql);
		stmt.bind(1, row_id);
		if (stmt.step()) {
			old_modification_id = stmt.getInt(0);
		}
	}

	m_stmts.Get("INSERT INTO placementd_modifications (changeset_id, entity_type, entity_uuid, operation, changes)"
		" VALUES (?1, ?2, ?3, ?4, '{}')")
		.bind(1, m_changeset_id).bind(2, ENTITY_AUDIT_TYPES[static_cast<int>(t)]).bind(3, uuid)
		.bind(4, operation).exec();
	long long modification_id = sqlite3_last_insert_rowid(m_stmts.handle());

	m_revoked = 0;
	long long new_row_id = write(modification_id);

	ClassAd after = PlacementSnapshot(m_stmts, t, new_row_id);
	if (m_revoked > 0) {
		// Recorded as a change from nothing, like any other field.
		after.Assign("RevokedTokens", m_revoked);
		m_revoked = 0;
	}
	std::string changes = diff_json(before, after);

	if (strcmp(operation, "update") == 0 && changes == "{}" && new_row_id != 0 && old_modification_id != 0) {
		// Nothing visible changed: keep no record, and leave Version alone.
		std::string sql;
		formatstr(sql, "UPDATE %s SET modification_id = ?1 WHERE id = ?2", PlacementEntityTable(t));
		m_stmts.Get(sql).bind(1, old_modification_id).bind(2, new_row_id).exec();
		m_stmts.Get("DELETE FROM placementd_modifications WHERE id = ?1").bind(1, modification_id).exec();
		if (new_changeset) {
			m_stmts.Get("DELETE FROM placementd_changesets WHERE id = ?1").bind(1, m_changeset_id).exec();
			m_changeset_id = 0;
		}
		return old_modification_id;
	}

	m_stmts.Get("UPDATE placementd_modifications SET changes = ?1 WHERE id = ?2")
		.bind(1, changes).bind(2, modification_id).exec();
	m_count++;
	return modification_id;
}

long long
PlacementAudit::RevokeTokens(PlacementTokenBasis basis, const std::string &uuid, std::optional<long long> outliving)
{
	// The authentication layer creates its tables when it first opens the
	// database; without them, no token was ever recorded.
	bool have_minting = false, have_claims = false;
	{
		PlacementStmt &stmt = m_stmts.Get("SELECT name FROM sqlite_master WHERE type = 'table' AND name IN"
			" ('idtokens_minting', 'idtokens_minting_extra_claims')");
		while (stmt.step()) {
			std::string table = stmt.getText(0);
			have_minting = have_minting || table == "idtokens_minting";
			have_claims = have_claims || table == "idtokens_minting_extra_claims";
		}
	}
	if (!have_minting) {
		return 0;
	}

	// Unexpired tokens only, and with outliving, those that expire after it.
	long long after = (long long)time(nullptr) - 1;
	if (outliving && *outliving > after) {
		after = *outliving;
	}
	std::string tokens;
	formatstr(tokens, "SELECT token_jti FROM placementd_tokens WHERE %s = ?1 AND token_exp > ?2",
		basis == PlacementTokenBasis::Identity ? "identity_uuid" : "account_uuid");
	// The extra claims refer to the token's row, so they go first.
	if (have_claims) {
		m_stmts.Get("DELETE FROM idtokens_minting_extra_claims WHERE token_jti IN (" + tokens + ")")
			.bind(1, uuid).bind(2, after).exec();
	}
	long long count = m_stmts.Get("DELETE FROM idtokens_minting WHERE token_jti IN (" + tokens + ")")
		.bind(1, uuid).bind(2, after).exec();
	m_revoked += count;
	m_revoked_total += count;
	return count;
}

// ---------------------------------------------------------------- PlacementDb

PlacementDb::~PlacementDb()
{
	Close();
}

void
PlacementDb::Close()
{
	m_stmts.reset();
	if (m_db) {
		sqlite3_close(m_db);
		m_db = nullptr;
	}
}

void
PlacementDb::Open(const std::string &path)
{
	int rc = sqlite3_open_v2(path.c_str(), &m_db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
	if (rc != SQLITE_OK) {
		EXCEPT("Failed to open database file %s: %s", path.c_str(), sqlite3_errmsg(m_db));
	}
	// Every daemon's auth layer opens this file too; wait for its locks
	// rather than failing.  Do not change journal_mode (it is persistent
	// and would change locking for all of them).
	sqlite3_busy_timeout(m_db, 5000);
	m_stmts = std::make_unique<PlacementStatements>(m_db);

	try {
		// foreign_keys is per connection and a no-op inside a transaction.
		exec_sql(m_db, "PRAGMA foreign_keys = ON");
		Migrate();
	} catch (const PlacementError &e) {
		EXCEPT("Failed to initialize placementd database %s: %s", path.c_str(), e.what());
	}
}

void
PlacementDb::Migrate()
{
	PlacementTxn txn(*m_stmts);

	exec_sql(m_db, "CREATE TABLE IF NOT EXISTS placementd_tokens (requester_id TEXT, foreign_id TEXT, ap_id TEXT, authz TEXT, token_jti TEXT, token_exp INTEGER, token_project TEXT)");
	exec_sql(m_db, "CREATE TABLE IF NOT EXISTS placementd_meta (key TEXT PRIMARY KEY, value TEXT NOT NULL)");

	std::string version_str;
	int version = 1;
	if (GetMeta("schema_version", version_str)) {
		version = atoi(version_str.c_str());
		if (version <= 0) {
			EXCEPT("Placementd database has an invalid schema_version '%s'", version_str.c_str());
		}
	}
	if (version > PLACEMENTD_SCHEMA_VERSION) {
		EXCEPT("Placementd database schema version %d is newer than this daemon supports (%d); refusing to guess at it",
			version, PLACEMENTD_SCHEMA_VERSION);
	}
	if (version == PLACEMENTD_SCHEMA_VERSION) {
		CheckShape();
		if (!HasTokenLogIndexes()) {
			for (const char *sql: TOKEN_LOG_INDEXES) {
				exec_sql(m_db, sql);
			}
			txn.commit();
		}
		return;		// otherwise txn rolls back; nothing written
	}

	dprintf(D_ALWAYS, "Migrating placementd database from schema version %d to %d\n",
		version, PLACEMENTD_SCHEMA_VERSION);

	// The token log may already carry the new columns (a downgrade that
	// lost placementd_meta); only add the missing ones.
	std::vector<std::string> columns;
	{
		PlacementStmt stmt(m_db, "PRAGMA table_info(placementd_tokens)");
		while (stmt.step()) {
			columns.emplace_back(stmt.getText(1));
		}
	}
	for (const char *col: TOKEN_LOG_COLUMNS) {
		if (!contains_anycase(columns, col)) {
			std::string sql;
			formatstr(sql, "ALTER TABLE placementd_tokens ADD COLUMN %s TEXT", col);
			exec_sql(m_db, sql.c_str());
		}
	}
	for (const char *sql: SCHEMA_V2) {
		exec_sql(m_db, sql);
	}
	for (const char *sql: TOKEN_LOG_INDEXES) {
		exec_sql(m_db, sql);
	}
	SetMeta("schema_version", std::to_string(PLACEMENTD_SCHEMA_VERSION));
	txn.commit();
}

void
PlacementDb::CheckShape()
{
	std::vector<std::string> missing;
	for (const char *table: SCHEMA_V2_TABLES) {
		PlacementStmt stmt(m_db, "SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = ?1");
		stmt.bind(1, table);
		if (!stmt.step()) {
			missing.emplace_back(std::string("table ") + table);
		}
	}
	for (int i = 0; i < 4; i++) {
		const char *table = ENTITY_TABLES[i];
		std::string sql;
		formatstr(sql, "PRAGMA table_info(%s)", table);
		PlacementStmt stmt(m_db, sql.c_str());
		bool found = false;
		bool exists = false;
		while (stmt.step()) {
			exists = true;
			found = found || stmt.getText(1) == "modification_id";
		}
		if (exists && !found) {
			missing.emplace_back(std::string("column ") + table + ".modification_id");
		}
	}
	if (!missing.empty()) {
		EXCEPT("Placementd database claims schema version %d but lacks %s; it was not written by a release "
			"of this daemon.  Move it aside to start over.", PLACEMENTD_SCHEMA_VERSION, join(missing, ", ").c_str());
	}
}

bool
PlacementDb::HasTokenLogIndexes()
{
	PlacementStmt stmt(m_db, "SELECT COUNT(*) FROM sqlite_master WHERE type = 'index' AND name IN"
		" ('placementd_tokens_identity_uuid', 'placementd_tokens_account_uuid')");
	return stmt.step() && stmt.getInt(0) == 2;
}

bool
PlacementDb::GetMeta(const std::string &key, std::string &value)
{
	PlacementStmt &stmt = m_stmts->Get("SELECT value FROM placementd_meta WHERE key = ?1");
	stmt.bind(1, key);
	if (!stmt.step()) {
		return false;
	}
	value = stmt.getText(0);
	stmt.reset();
	return true;
}

void
PlacementDb::SetMeta(const std::string &key, const std::string &value)
{
	m_stmts->Get("INSERT OR REPLACE INTO placementd_meta (key, value) VALUES (?1, ?2)")
		.bind(1, key).bind(2, value).exec();
}

bool
PlacementDb::LookupLogin(const std::string &identity, PlacementLoginInfo &out)
{
	long long account_id = 0;
	{
		PlacementStmt &stmt = m_stmts->Get("SELECT i.uuid, i.expires_at, a.id, a.uuid, a.name"
			" FROM placementd_identities i JOIN placementd_accounts a ON a.id = i.account_id"
			" WHERE i.identity = ?1");
		stmt.bind(1, identity);
		if (!stmt.step()) {
			return false;
		}
		out.identity_uuid = stmt.getText(0);
		out.expires = (time_t)stmt.getOptInt(1).value_or(0);
		account_id = stmt.getInt(2);
		out.account_uuid = stmt.getText(3);
		out.account_name = stmt.getText(4);
		stmt.reset();	// release its read lock
	}
	PlacementReadGrants(*m_stmts, account_id, out.grants);
	return true;
}

void
PlacementDb::ListLegacyUsers(const std::string &identity, std::vector<PlacementLegacyUser> &out)
{
	std::string sql = "SELECT i.identity, i.expires_at, a.id, a.name"
		" FROM placementd_identities i JOIN placementd_accounts a ON a.id = i.account_id";
	if (!identity.empty()) {
		sql += " WHERE i.identity = ?1";
	}
	sql += " ORDER BY i.identity";

	struct Row { PlacementLegacyUser user; long long account_id; };
	std::vector<Row> rows;
	{
		PlacementStmt stmt(m_db, sql.c_str());
		if (!identity.empty()) {
			stmt.bind(1, identity);
		}
		while (stmt.step()) {
			Row row;
			row.user.identity = stmt.getText(0);
			row.user.expires = (time_t)stmt.getOptInt(1).value_or(0);
			row.account_id = stmt.getInt(2);
			row.user.account = stmt.getText(3);
			rows.push_back(std::move(row));
		}
	}

	std::map<long long, std::pair<std::string, std::string>> grant_cache;
	for (auto &row: rows) {
		auto it = grant_cache.find(row.account_id);
		if (it == grant_cache.end()) {
			PlacementGrants grants;
			PlacementReadGrants(*m_stmts, row.account_id, grants);
			it = grant_cache.emplace(row.account_id, std::make_pair(join(grants.AuthorizationNames(), ","),
				join(grants.ProjectNames(), ","))).first;
		}
		row.user.authorizations = it->second.first;
		row.user.projects = it->second.second;
		out.emplace_back(std::move(row.user));
	}
}

void
PlacementDb::ListRequestableAuthorizations(std::vector<PlacementLegacyAuthz> &out)
{
	// The old std::map ordered by name, byte-wise.
	PlacementStmt &stmt = m_stmts->Get("SELECT name, label, color, description FROM placementd_authorizations"
		" WHERE requestable = 1 ORDER BY name COLLATE BINARY");
	while (stmt.step()) {
		out.push_back(PlacementLegacyAuthz{stmt.getText(0), stmt.getText(1), stmt.getText(2), stmt.getText(3)});
	}
}

void
PlacementDb::EntityAds(PlacementEntity t, const std::string &uuid, const std::string &name,
	std::vector<ClassAd> &out)
{
	// Columns 0-7 are common: id, uuid, then what assign_common() reads.
	// UpdatedAt and ModifiedBy come from the latest modification, CreatedAt
	// from the first one.
	std::string sql = "SELECT e.id, e.uuid, e.source, e.modification_id, c.at, c.actor, c.on_behalf_of,"
		" (SELECT c1.at FROM placementd_modifications m1 JOIN placementd_changesets c1 ON c1.id = m1.changeset_id"
		"  WHERE m1.entity_uuid = e.uuid ORDER BY m1.id LIMIT 1)";
	switch (t) {
	case PlacementEntity::Identity:
		sql += ", e.identity, e.expires_at, e.display_name, e.comment, a.id, a.uuid, a.name"
			" FROM placementd_identities e JOIN placementd_accounts a ON a.id = e.account_id";
		break;
	case PlacementEntity::Account:
		sql += ", e.name FROM placementd_accounts e";
		break;
	case PlacementEntity::Project:
		sql += ", e.name, e.description FROM placementd_projects e";
		break;
	case PlacementEntity::Authorization:
		sql += ", e.name, e.label, e.color, e.description, e.requestable FROM placementd_authorizations e";
		break;
	}
	sql += " JOIN placementd_modifications m ON m.id = e.modification_id"
		" JOIN placementd_changesets c ON c.id = m.changeset_id";
	if (!uuid.empty()) {
		sql += " WHERE e.uuid = ?1";
	} else if (!name.empty()) {
		formatstr_cat(sql, " WHERE e.%s = ?1", PlacementEntityKeyColumn(t));
	}
	formatstr_cat(sql, " ORDER BY e.%s", PlacementEntityKeyColumn(t));

	PlacementStmt stmt(m_db, sql.c_str());
	if (!uuid.empty()) {
		stmt.bind(1, uuid);
	} else if (!name.empty()) {
		stmt.bind(1, name);
	}

	std::string my_type = std::string("Placement") + PlacementEntityName(t);
	time_t now = time(nullptr);
	while (stmt.step()) {
		ClassAd ad;
		long long row_id = stmt.getInt(0);
		ad.Assign(ATTR_MY_TYPE, my_type);
		ad.Assign("Id", stmt.getText(1));
		assign_common(ad, stmt, 2);

		PlacementGrants grants;
		auto assign_grants = [&](long long account_id) {
			PlacementReadGrants(*m_stmts, account_id, grants);
			PlacementAssignStringList(ad, "Authorizations", grants.AuthorizationNames());
			PlacementAssignStringList(ad, "Projects", grants.ProjectNames());
			std::string default_project = grants.DefaultProjectName();
			if (!default_project.empty()) {
				ad.Assign("DefaultProject", default_project);
			}
		};
		auto referrer_names = [&](PlacementEntity type) {
			std::vector<std::string> names;
			for (const auto &referrer: PlacementReferrers(*m_stmts, type, row_id)) {
				names.push_back(referrer.name);
			}
			return names;
		};

		switch (t) {
		case PlacementEntity::Identity: {
			std::string identity = stmt.getText(8);
			ad.Assign("Identity", identity);
			auto expires = stmt.getOptInt(9);
			if (expires) {
				ad.Assign("ExpiresAt", *expires);
			}
			ad.Assign("Expired", expires.has_value() && *expires <= now);
			if (!stmt.isNull(10)) { ad.Assign("DisplayName", stmt.getText(10)); }
			if (!stmt.isNull(11)) { ad.Assign("Comment", stmt.getText(11)); }
			ad.Assign("AccountId", stmt.getText(13));
			ad.Assign("Account", stmt.getText(14));
			assign_grants(stmt.getInt(12));
			// The token log's foreign_id column holds the identity.
			PlacementStmt &tok = m_stmts->Get("SELECT MAX(token_exp) FROM placementd_tokens WHERE foreign_id = ?1");
			tok.bind(1, identity);
			if (tok.step() && !tok.isNull(0)) {
				ad.Assign("LastTokenExpiration", tok.getInt(0));
			}
			tok.reset();	// release its read lock
			break;
		}
		case PlacementEntity::Account:
			ad.Assign("Name", stmt.getText(8));
			assign_grants(row_id);
			PlacementAssignStringList(ad, "Identities", referrer_names(PlacementEntity::Account));
			break;
		case PlacementEntity::Project:
			ad.Assign("Name", stmt.getText(8));
			if (!stmt.isNull(9)) {
				ad.Assign("Description", stmt.getText(9));
			}
			PlacementAssignStringList(ad, "Accounts", referrer_names(PlacementEntity::Project));
			break;
		case PlacementEntity::Authorization:
			ad.Assign("Name", stmt.getText(8));
			ad.Assign("Label", stmt.getText(9));
			ad.Assign("Color", stmt.getText(10));
			ad.Assign("Description", stmt.getText(11));
			ad.Assign("Requestable", stmt.getInt(12) != 0);
			break;
		}
		out.emplace_back(std::move(ad));
	}
}

void
PlacementDb::ModificationAds(const std::string &entity_uuid, long long limit, std::vector<ClassAd> &out)
{
	std::string sql = "SELECT m.id, m.changeset_id, c.at, c.channel, c.actor, c.on_behalf_of, c.peer_address,"
		" c.comment, m.operation, m.entity_type, m.entity_uuid, m.changes"
		" FROM placementd_modifications m JOIN placementd_changesets c ON c.id = m.changeset_id";
	if (!entity_uuid.empty()) {
		sql += " WHERE m.entity_uuid = ?1";
	}
	sql += " ORDER BY m.id DESC";
	if (limit >= 0) {
		sql += " LIMIT ?2";
	}
	PlacementStmt stmt(m_db, sql.c_str());
	if (!entity_uuid.empty()) {
		stmt.bind(1, entity_uuid);
	}
	if (limit >= 0) {
		stmt.bind(2, limit);
	}

	static const char *const optional_attrs[] = { "OnBehalfOf", "PeerAddress", "Comment" };
	classad::ClassAdJsonParser parser;
	while (stmt.step()) {
		ClassAd ad;
		ad.Assign(ATTR_MY_TYPE, "PlacementModification");
		ad.Assign("Id", stmt.getInt(0));
		ad.Assign("ChangesetId", stmt.getInt(1));
		ad.Assign("Time", stmt.getInt(2));
		ad.Assign("Channel", stmt.getText(3));
		ad.Assign("Actor", stmt.getText(4));
		for (int col = 5; col <= 7; col++) {
			if (!stmt.isNull(col)) {
				ad.Assign(optional_attrs[col - 5], stmt.getText(col));
			}
		}
		ad.Assign("Operation", stmt.getText(8));
		ad.Assign("EntityType", stmt.getText(9));
		ad.Assign("EntityId", stmt.getText(10));
		ClassAd *changes = parser.ParseClassAd(stmt.getText(11), true);
		if (!changes) {
			changes = new ClassAd();
		}
		ad.Insert("Changes", changes);
		out.emplace_back(std::move(ad));
	}
}
