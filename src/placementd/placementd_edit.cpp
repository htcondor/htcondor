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
#include "condor_perms.h"
#include "stl_string_utils.h"
#include "placementd_edit.h"

#include <algorithm>

namespace {

[[noreturn]] void
fail(int code, const std::string &msg)
{
	throw PlacementError(code, msg);
}

// ------------------------------------------------------------ attribute access

enum class Field { Absent, Undefined, Value };

Field
lookup_value(const ClassAd &ad, const char *attr, classad::Value &val)
{
	if (!ad.Lookup(attr)) {
		return Field::Absent;
	}
	if (!ad.EvaluateAttr(attr, val)) {
		fail(PLACEMENT_ERR_INVALID, std::string("cannot evaluate ") + attr);
	}
	if (val.IsUndefinedValue()) {
		return Field::Undefined;
	}
	return Field::Value;
}

Field
get_string(const ClassAd &ad, const char *attr, std::string &out)
{
	classad::Value val;
	Field f = lookup_value(ad, attr, val);
	if (f == Field::Value && !val.IsStringValue(out)) {
		fail(PLACEMENT_ERR_INVALID, std::string(attr) + " must be a string");
	}
	return f;
}

// Free text: a string, made safe to store and show, and capped.
Field
get_text(const ClassAd &ad, const char *attr, size_t max_bytes, std::string &out)
{
	Field f = get_string(ad, attr, out);
	if (f == Field::Value) {
		out = PlacementSanitize(out, max_bytes);
	}
	return f;
}

Field
get_int(const ClassAd &ad, const char *attr, long long &out)
{
	classad::Value val;
	Field f = lookup_value(ad, attr, val);
	if (f == Field::Value && !val.IsIntegerValue(out)) {
		fail(PLACEMENT_ERR_INVALID, std::string(attr) + " must be an integer");
	}
	return f;
}

Field
get_bool(const ClassAd &ad, const char *attr, bool &out)
{
	classad::Value val;
	Field f = lookup_value(ad, attr, val);
	if (f == Field::Value) {
		long long i = 0;
		if (val.IsBooleanValue(out)) {
			// done
		} else if (val.IsIntegerValue(i) && (i == 0 || i == 1)) {
			out = (i == 1);
		} else {
			fail(PLACEMENT_ERR_INVALID, std::string(attr) + " must be a boolean");
		}
	}
	return f;
}

// A ClassAd list of strings.  A comma-separated string is also accepted.
Field
get_list(const ClassAd &ad, const char *attr, std::vector<std::string> &out)
{
	classad::Value val;
	out.clear();
	Field f = lookup_value(ad, attr, val);
	if (f != Field::Value) {
		return f;
	}
	std::string str;
	const classad::ExprList *list = nullptr;
	classad_shared_ptr<classad::ExprList> slist;
	if (val.IsStringValue(str)) {
		out = split(str, ",");
		return f;
	}
	if (val.IsSListValue(slist)) {
		list = slist.get();
	} else if (!val.IsListValue(list)) {
		list = nullptr;
	}
	if (!list) {
		fail(PLACEMENT_ERR_INVALID, std::string(attr) + " must be a list of strings");
	}
	for (const auto *expr: *list) {
		classad::Value item;
		std::string s;
		if (!expr || !expr->Evaluate(item) || !item.IsStringValue(s)) {
			fail(PLACEMENT_ERR_INVALID, std::string(attr) + " must be a list of strings");
		}
		trim(s);
		if (s.empty()) {
			fail(PLACEMENT_ERR_INVALID, std::string(attr) + " contains an empty name");
		}
		out.push_back(s);
	}
	return f;
}

// ------------------------------------------------------------ the batch

struct RowInfo
{
	long long id{0};
	std::string uuid;
	std::string name;
	std::string source;
	long long version{0};	// the row's latest modification id
};

// What one Update changed, for the audit line and the result.
struct UpdateNotes
{
	std::vector<std::string> changed;
	std::string warning;
	// An authorization was renamed (other than in case): the tokens of the
	// accounts holding it carry the old name.
	bool revoke_holders{false};
};

class EditBatch
{
public:
	EditBatch(PlacementStatements &stmts, const PlacementEditContext &ctx)
		: m_stmts(stmts), m_ctx(ctx),
		  m_audit(stmts, PlacementChangesetInfo{"cedar", ctx.peer, ctx.on_behalf_of, ctx.peer_address, ctx.comment})
	{}

	void Execute(const ClassAd &op, ClassAd &result);

	long long ChangesetId() const { return m_audit.ChangesetId(); }
	std::vector<std::string> &AuditLines() { return m_audit_lines; }

private:
	void Create(PlacementEntity t, const ClassAd &op, ClassAd &result);
	void Update(PlacementEntity t, const ClassAd &op, ClassAd &result);
	void Delete(PlacementEntity t, const ClassAd &op, ClassAd &result);
	void Adopt(PlacementEntity t, const ClassAd &op, ClassAd &result);
	void Revoke(PlacementEntity t, const ClassAd &op, ClassAd &result);
	void AdoptAll(ClassAd &result);

	// The writes of one Update, per type; run inside its modification.
	void UpdateIdentity(const RowInfo &row, const ClassAd &op, UpdateNotes &notes);
	void UpdateAccount(const RowInfo &row, const ClassAd &op, UpdateNotes &notes);
	void UpdateProject(const RowInfo &row, const ClassAd &op, UpdateNotes &notes);
	void UpdateAuthorization(const RowInfo &row, const ClassAd &op, UpdateNotes &notes);

	bool FindRow(PlacementEntity t, const std::string &uuid, const std::string &name, RowInfo &out);
	RowInfo Select(PlacementEntity t, const ClassAd &op);
	// With an Id selector, the key attribute (Identity or Name) is the new
	// value.  Without one, it is the selector and renames nothing.
	Field NewKey(PlacementEntity t, const ClassAd &op, std::string &val);
	void CheckVersion(const RowInfo &row, const ClassAd &op);
	void CheckOwner(PlacementEntity t, const RowInfo &row);
	void CheckUnique(PlacementEntity t, const std::string &name, long long except_id);
	// Validate a name for its type (and what may be minted); fails with 16.
	void CheckName(PlacementEntity t, const std::string &name);
	std::string Canonical(const std::string &name);

	void SetColumn(PlacementEntity t, long long id, const char *column, const std::optional<std::string> &val);
	void SetModification(PlacementEntity t, long long id, long long modification_id);
	// Record a modification that only changes the row's source.
	long long ChangeSource(PlacementEntity t, long long id, const std::string &uuid,
		const char *operation, const char *source);

	long long ResolveAccount(const std::string &ref);
	void ResolveNames(PlacementEntity t, const std::vector<std::string> &names, std::vector<long long> &ids);
	bool StillInFile(PlacementEntity t, const std::string &key) const;
	// A warning for renaming a project or authorization that user lines in
	// the last parsed map file still name by its old name; empty if none do.
	std::string MapfileStillNames(PlacementEntity t, const std::string &old_name) const;
	void Audit(const char *op, PlacementEntity t, const std::string &uuid, const std::string &fields);

	PlacementStatements &m_stmts;
	const PlacementEditContext &m_ctx;
	PlacementAudit m_audit;		// one change set for the batch
	std::vector<std::string> m_audit_lines;
};

std::string
EditBatch::Canonical(const std::string &raw)
{
	std::string name = raw;
	trim(name);
	if (name.empty() || name == "*") {
		fail(PLACEMENT_ERR_INVALID, "invalid account name '" + raw + "'");
	}
	name = PlacementCanonicalAccount(name, "", m_ctx.uid_domain);
	CheckName(PlacementEntity::Account, name);
	return name;
}

void
EditBatch::CheckName(PlacementEntity t, const std::string &name)
{
	std::string why;
	if (!PlacementValidName(t, name, why)) {
		fail(PLACEMENT_ERR_INVALID, std::string("invalid ") + PlacementEntityName(t) + " name '" +
			name + "': " + why);
	}
	if (t == PlacementEntity::Account && PlacementReservedAccount(name)) {
		fail(PLACEMENT_ERR_INVALID, "account name '" + name + "' is reserved");
	}
	if (t == PlacementEntity::Authorization && PlacementPrivilegedAuthorization(name)) {
		fail(PLACEMENT_ERR_INVALID, "authorization '" + name + "' is a privileged permission level");
	}
}

bool
EditBatch::FindRow(PlacementEntity t, const std::string &uuid, const std::string &name, RowInfo &out)
{
	std::string sql;
	formatstr(sql, "SELECT id, uuid, %s, source, modification_id FROM %s WHERE %s = ?1",
		PlacementEntityKeyColumn(t), PlacementEntityTable(t),
		uuid.empty() ? PlacementEntityKeyColumn(t) : "uuid");
	PlacementStmt &stmt = m_stmts.Get(sql);
	stmt.bind(1, uuid.empty() ? name : uuid);
	if (!stmt.step()) {
		return false;
	}
	out.id = stmt.getInt(0);
	out.uuid = stmt.getText(1);
	out.name = stmt.getText(2);
	out.source = stmt.getText(3);
	out.version = stmt.getInt(4);
	return true;
}

RowInfo
EditBatch::Select(PlacementEntity t, const ClassAd &op)
{
	std::string uuid, name;
	get_string(op, "Id", uuid);
	get_string(op, PlacementEntityKeyAttr(t), name);
	if (uuid.empty() && name.empty()) {
		fail(PLACEMENT_ERR_INVALID, std::string("operation needs an Id or ") + PlacementEntityKeyAttr(t) + " selector");
	}
	lower_case(uuid);
	if (uuid.empty() && t == PlacementEntity::Account) {
		name = Canonical(name);
	}
	RowInfo row;
	if (!FindRow(t, uuid, name, row)) {
		fail(PLACEMENT_ERR_NOT_FOUND, std::string(PlacementEntityName(t)) + " '" +
			(uuid.empty() ? name : uuid) + "' not found");
	}
	return row;
}

Field
EditBatch::NewKey(PlacementEntity t, const ClassAd &op, std::string &val)
{
	std::string id;
	if (get_string(op, "Id", id) != Field::Value || id.empty()) {
		return Field::Absent;
	}
	return get_string(op, PlacementEntityKeyAttr(t), val);
}

void
EditBatch::CheckVersion(const RowInfo &row, const ClassAd &op)
{
	long long expected = 0;
	if (get_int(op, "ExpectedVersion", expected) == Field::Value && expected != row.version) {
		std::string msg;
		formatstr(msg, "version mismatch on %s: expected %lld, current %lld", row.uuid.c_str(),
			expected, row.version);
		fail(PLACEMENT_ERR_VERSION, msg);
	}
}

void
EditBatch::CheckOwner(PlacementEntity t, const RowInfo &row)
{
	if (row.source != PLACEMENT_SOURCE_INTERNAL) {
		fail(PLACEMENT_ERR_OWNER, std::string(PlacementEntityName(t)) + " '" + row.name +
			"' is owned by " + row.source + "; Adopt it first");
	}
}

void
EditBatch::CheckUnique(PlacementEntity t, const std::string &name, long long except_id)
{
	RowInfo existing;
	if (FindRow(t, "", name, existing) && existing.id != except_id) {
		fail(PLACEMENT_ERR_DUPLICATE, std::string(PlacementEntityName(t)) + " '" + name + "' already exists");
	}
}

void
EditBatch::SetColumn(PlacementEntity t, long long id, const char *column, const std::optional<std::string> &val)
{
	std::string sql;
	formatstr(sql, "UPDATE %s SET %s = ?1 WHERE id = ?2", PlacementEntityTable(t), column);
	m_stmts.Get(sql).bind(1, val).bind(2, id).exec();
}

void
EditBatch::SetModification(PlacementEntity t, long long id, long long modification_id)
{
	std::string sql;
	formatstr(sql, "UPDATE %s SET modification_id = ?1 WHERE id = ?2", PlacementEntityTable(t));
	m_stmts.Get(sql).bind(1, modification_id).bind(2, id).exec();
}

long long
EditBatch::ChangeSource(PlacementEntity t, long long id, const std::string &uuid,
	const char *operation, const char *source)
{
	return m_audit.Modify(t, id, uuid, operation, [this, t, id, source](long long mod) {
		std::string sql;
		formatstr(sql, "UPDATE %s SET source = ?1, modification_id = ?2 WHERE id = ?3", PlacementEntityTable(t));
		m_stmts.Get(sql).bind(1, source).bind(2, mod).bind(3, id).exec();
		return id;
	});
}

long long
EditBatch::ResolveAccount(const std::string &ref)
{
	RowInfo row;
	std::string lowered = ref;
	lower_case(lowered);
	if (FindRow(PlacementEntity::Account, lowered, "", row)) {
		return row.id;
	}
	if (FindRow(PlacementEntity::Account, "", Canonical(ref), row)) {
		return row.id;
	}
	fail(PLACEMENT_ERR_NOT_FOUND, "account '" + ref + "' not found");
}

void
EditBatch::ResolveNames(PlacementEntity t, const std::vector<std::string> &names, std::vector<long long> &ids)
{
	ids.clear();
	for (const auto &name: names) {
		RowInfo row;
		if (!FindRow(t, "", name, row)) {
			fail(PLACEMENT_ERR_NOT_FOUND, std::string(PlacementEntityName(t)) + " '" + name + "' not found");
		}
		if (std::find(ids.begin(), ids.end(), row.id) == ids.end()) {
			ids.push_back(row.id);
		}
	}
}

bool
EditBatch::StillInFile(PlacementEntity t, const std::string &key) const
{
	const PlacementMapfileKeys *keys = m_ctx.keys;
	if (!keys || !keys->valid) {
		return false;
	}
	switch (t) {
	case PlacementEntity::Identity: return keys->identities.count(key) > 0;
	case PlacementEntity::Account: return keys->accounts.count(key) > 0;
	case PlacementEntity::Project: return keys->projects.count(key) > 0;
	case PlacementEntity::Authorization: return keys->authorizations.count(key) > 0;
	}
	return false;
}

std::string
EditBatch::MapfileStillNames(PlacementEntity t, const std::string &old_name) const
{
	const PlacementMapfileKeys *keys = m_ctx.keys;
	if (!keys || !keys->valid) {
		return "";
	}
	const auto &refs = (t == PlacementEntity::Project) ? keys->project_refs : keys->authorization_refs;
	auto it = refs.find(old_name);
	if (it == refs.end() || it->second.empty()) {
		return "";
	}
	const size_t shown = 3;
	std::vector<std::string> accounts;
	for (const auto &account: it->second) {
		if (accounts.size() == shown) {
			break;
		}
		accounts.push_back(account);
	}
	std::string names = join(accounts, ", ");
	if (it->second.size() > shown) {
		formatstr_cat(names, ", and %zu more", it->second.size() - shown);
	}
	std::string warning;
	formatstr(warning, "the map file still lists %s for %zu account(s) (%s), so after the next reload those "
		"accounts keep a grant under the old name until the map file is updated", old_name.c_str(),
		it->second.size(), names.c_str());
	return warning;
}

void
EditBatch::Audit(const char *op, PlacementEntity t, const std::string &uuid, const std::string &fields)
{
	std::string line;
	formatstr(line, "PLACEMENT_EDIT by %s: %s %s %s%s%s", m_ctx.peer.c_str(), op,
		PlacementEntityName(t), uuid.c_str(), fields.empty() ? "" : " fields=", fields.c_str());
	m_audit_lines.emplace_back(PlacementSanitize(line, 8192));
}

void
EditBatch::Execute(const ClassAd &op, ClassAd &result)
{
	std::string operation, type_name;
	get_string(op, "Operation", operation);
	get_string(op, ATTR_TARGET_TYPE, type_name);

	if (strcasecmp(type_name.c_str(), "All") == 0) {
		if (strcasecmp(operation.c_str(), "Adopt") != 0) {
			fail(PLACEMENT_ERR_UNKNOWN_TYPE, "TargetType All is only valid for Adopt");
		}
		AdoptAll(result);
		return;
	}
	PlacementEntity t;
	if (!PlacementEntityFromName(type_name, t)) {
		fail(PLACEMENT_ERR_UNKNOWN_TYPE, "unknown TargetType '" + type_name + "'");
	}
	// An identity's key is Identity; a Name would otherwise be ignored.
	if (t == PlacementEntity::Identity && op.Lookup(ATTR_NAME)) {
		fail(PLACEMENT_ERR_INVALID, "an Identity has no Name; use Identity");
	}
	const long long revoked_before = m_audit.RevokedTokens();
	if (strcasecmp(operation.c_str(), "Create") == 0) {
		Create(t, op, result);
	} else if (strcasecmp(operation.c_str(), "Update") == 0) {
		Update(t, op, result);
	} else if (strcasecmp(operation.c_str(), "Delete") == 0) {
		Delete(t, op, result);
	} else if (strcasecmp(operation.c_str(), "Adopt") == 0) {
		Adopt(t, op, result);
	} else if (strcasecmp(operation.c_str(), "Revoke") == 0) {
		Revoke(t, op, result);
	} else {
		fail(PLACEMENT_ERR_UNKNOWN_TYPE, "unknown Operation '" + operation + "'");
	}
	const long long revoked = m_audit.RevokedTokens() - revoked_before;
	if (revoked > 0) {
		result.Assign("RevokedTokens", revoked);
		std::string id;
		result.LookupString("Id", id);
		std::string line;
		formatstr(line, "PLACEMENT_EDIT by %s: %s %s %s revoked %lld token(s)", m_ctx.peer.c_str(),
			operation.c_str(), PlacementEntityName(t), id.c_str(), revoked);
		m_audit_lines.emplace_back(PlacementSanitize(line, 8192));
	}
}

void
EditBatch::Create(PlacementEntity t, const ClassAd &op, ClassAd &result)
{
	std::string uuid = PlacementNewUuid();
	std::string fields;
	long long version = 0;
	sqlite3 *db = m_stmts.handle();

	switch (t) {
	case PlacementEntity::Identity: {
		std::string name, account, display_name, comment;
		long long expires = 0;
		get_string(op, "Identity", name);
		CheckName(t, name);
		if (get_string(op, "Account", account) != Field::Value || account.empty()) {
			fail(PLACEMENT_ERR_INVALID, "Identity needs an Account");
		}
		long long account_id = ResolveAccount(account);
		std::optional<long long> expires_at;
		if (get_int(op, "ExpiresAt", expires) == Field::Value) {
			if (expires < 0) {
				fail(PLACEMENT_ERR_INVALID, "ExpiresAt must not be negative");
			}
			if (expires > 0) {
				expires_at = expires;
			}
		}
		std::optional<std::string> dn, cm;
		if (get_text(op, "DisplayName", PLACEMENTD_MAX_SHORT_TEXT, display_name) == Field::Value) { dn = display_name; }
		if (get_text(op, "Comment", PLACEMENTD_MAX_LONG_TEXT, comment) == Field::Value) { cm = comment; }
		CheckUnique(t, name, 0);
		version = m_audit.Modify(t, 0, uuid, "create", [&](long long mod) {
			m_stmts.Get("INSERT INTO placementd_identities (uuid, identity, account_id, expires_at,"
				" display_name, comment, source, modification_id) VALUES (?1, ?2, ?3, ?4, ?5, ?6, 'internal', ?7)")
				.bind(1, uuid).bind(2, name).bind(3, account_id).bind(4, expires_at).bind(5, dn)
				.bind(6, cm).bind(7, mod).exec();
			return (long long)sqlite3_last_insert_rowid(db);
		});
		fields = "Identity=" + name + ",Account=" + account;
		break;
	}
	case PlacementEntity::Account: {
		std::string raw_name, default_project;
		get_string(op, ATTR_NAME, raw_name);
		std::string name = Canonical(raw_name);
		std::vector<std::string> authz_names, project_names;
		std::vector<long long> authz_ids, project_ids;
		get_list(op, "Authorizations", authz_names);
		get_list(op, "Projects", project_names);
		ResolveNames(PlacementEntity::Authorization, authz_names, authz_ids);
		ResolveNames(PlacementEntity::Project, project_names, project_ids);
		long long default_id = 0;
		if (get_string(op, "DefaultProject", default_project) == Field::Value) {
			RowInfo proj;
			if (!FindRow(PlacementEntity::Project, "", default_project, proj) ||
				std::find(project_ids.begin(), project_ids.end(), proj.id) == project_ids.end()) {
				fail(PLACEMENT_ERR_INVALID, "DefaultProject '" + default_project + "' is not in Projects");
			}
			default_id = proj.id;
		}
		CheckUnique(t, name, 0);
		version = m_audit.Modify(t, 0, uuid, "create", [&](long long mod) {
			m_stmts.Get("INSERT INTO placementd_accounts (uuid, name, source, modification_id)"
				" VALUES (?1, ?2, 'internal', ?3)").bind(1, uuid).bind(2, name).bind(3, mod).exec();
			long long id = sqlite3_last_insert_rowid(db);
			PlacementWriteGrants(m_stmts, id, authz_ids, project_ids, default_id);
			return id;
		});
		fields = "Name=" + name + ",Authorizations=" + join(authz_names, ",") + ",Projects=" + join(project_names, ",");
		break;
	}
	case PlacementEntity::Project: {
		std::string name, description;
		get_string(op, ATTR_NAME, name);
		CheckName(t, name);
		std::optional<std::string> desc;
		if (get_text(op, "Description", PLACEMENTD_MAX_LONG_TEXT, description) == Field::Value) { desc = description; }
		CheckUnique(t, name, 0);
		version = m_audit.Modify(t, 0, uuid, "create", [&](long long mod) {
			m_stmts.Get("INSERT INTO placementd_projects (uuid, name, description, source,"
				" modification_id) VALUES (?1, ?2, ?3, 'internal', ?4)")
				.bind(1, uuid).bind(2, name).bind(3, desc).bind(4, mod).exec();
			return (long long)sqlite3_last_insert_rowid(db);
		});
		fields = "Name=" + name;
		break;
	}
	case PlacementEntity::Authorization: {
		std::string name, label, color = "normal", description;
		bool requestable = true;
		get_string(op, ATTR_NAME, name);
		CheckName(t, name);
		if (get_text(op, "Label", PLACEMENTD_MAX_SHORT_TEXT, label) != Field::Value || label.empty()) { label = name; }
		if (get_text(op, "Color", PLACEMENTD_MAX_SHORT_TEXT, color) != Field::Value || color.empty()) {
			color = "normal";
		}
		get_text(op, "Description", PLACEMENTD_MAX_LONG_TEXT, description);
		if (get_bool(op, "Requestable", requestable) == Field::Undefined) { requestable = true; }
		CheckUnique(t, name, 0);
		version = m_audit.Modify(t, 0, uuid, "create", [&](long long mod) {
			m_stmts.Get("INSERT INTO placementd_authorizations (uuid, name, label, color, description,"
				" requestable, source, modification_id) VALUES (?1, ?2, ?3, ?4, ?5, ?6, 'internal', ?7)")
				.bind(1, uuid).bind(2, name).bind(3, label).bind(4, color).bind(5, description)
				.bind(6, (long long)requestable).bind(7, mod).exec();
			return (long long)sqlite3_last_insert_rowid(db);
		});
		fields = "Name=" + name;
		break;
	}
	}

	result.Assign("Id", uuid);
	result.Assign("Version", version);
	Audit("Create", t, uuid, fields);
}

void
EditBatch::UpdateIdentity(const RowInfo &row, const ClassAd &op, UpdateNotes &notes)
{
	const PlacementEntity t = PlacementEntity::Identity;
	long long old_account = 0;
	std::optional<long long> old_expires;
	{
		PlacementStmt &stmt = m_stmts.Get("SELECT account_id, expires_at FROM placementd_identities WHERE id = ?1");
		stmt.bind(1, row.id);
		if (stmt.step()) {
			old_account = stmt.getInt(0);
			old_expires = stmt.getOptInt(1);
		}
	}
	// A token is minted for the identity's login and account, and lives no
	// longer than its expiry: changing the first two revokes the tokens,
	// and moving the expiry earlier revokes those that outlive it.
	bool revoke_all = false;
	std::optional<long long> revoke_outliving;

	std::string val;
	Field f = NewKey(t, op, val);
	if (f == Field::Undefined) {
		fail(PLACEMENT_ERR_INVALID, "an Identity must have an Identity");
	}
	if (f == Field::Value && val != row.name) {
		CheckName(t, val);
		CheckUnique(t, val, row.id);
		SetColumn(t, row.id, "identity", val);
		notes.changed.emplace_back("Identity");
		revoke_all = true;
	}
	f = get_string(op, "Account", val);
	if (f == Field::Undefined || (f == Field::Value && val.empty())) {
		fail(PLACEMENT_ERR_INVALID, "an Identity must have an Account");
	}
	if (f == Field::Value) {
		long long account_id = ResolveAccount(val);
		m_stmts.Get("UPDATE placementd_identities SET account_id = ?1 WHERE id = ?2")
			.bind(1, account_id).bind(2, row.id).exec();
		notes.changed.emplace_back("Account");
		revoke_all = revoke_all || account_id != old_account;
	}
	long long expires = 0;
	f = get_int(op, "ExpiresAt", expires);
	if (f != Field::Absent) {
		if (f == Field::Value && expires < 0) {
			fail(PLACEMENT_ERR_INVALID, "ExpiresAt must not be negative");
		}
		std::optional<long long> v;
		if (f == Field::Value && expires > 0) {
			v = expires;
		}
		m_stmts.Get("UPDATE placementd_identities SET expires_at = ?1 WHERE id = ?2").bind(1, v).bind(2, row.id).exec();
		notes.changed.emplace_back("ExpiresAt");
		if (v && (!old_expires || *v < *old_expires)) {
			revoke_outliving = v;
		}
	}
	for (const char *attr: {"DisplayName", "Comment"}) {
		bool comment = strcmp(attr, "Comment") == 0;
		f = get_text(op, attr, comment ? PLACEMENTD_MAX_LONG_TEXT : PLACEMENTD_MAX_SHORT_TEXT, val);
		if (f == Field::Absent) {
			continue;
		}
		std::optional<std::string> v;
		if (f == Field::Value) {
			v = val;
		}
		SetColumn(t, row.id, comment ? "comment" : "display_name", v);
		notes.changed.emplace_back(attr);
	}
	if (revoke_all) {
		m_audit.RevokeTokens(PlacementTokenBasis::Identity, row.uuid);
	} else if (revoke_outliving) {
		m_audit.RevokeTokens(PlacementTokenBasis::Identity, row.uuid, revoke_outliving);
	}
}

void
EditBatch::UpdateAccount(const RowInfo &row, const ClassAd &op, UpdateNotes &notes)
{
	const PlacementEntity t = PlacementEntity::Account;
	// A token carries the account's name and is bounded by its grants:
	// renaming the account or narrowing its grants revokes its tokens.
	bool revoke = false;
	std::string val;
	Field f = NewKey(t, op, val);
	if (f == Field::Undefined) {
		fail(PLACEMENT_ERR_INVALID, "an Account must have a Name");
	}
	if (f == Field::Value) {
		std::string name = Canonical(val);
		if (name != row.name) {
			CheckUnique(t, name, row.id);
			SetColumn(t, row.id, "name", name);
			notes.changed.emplace_back("Name");
			notes.warning = "Account renamed from " + row.name + " to " + name +
				"; tokens minted for the old name were revoked, and the schedd user record was not renamed";
			revoke = true;
		}
	}

	PlacementGrants current;
	PlacementReadGrants(m_stmts, row.id, current);
	std::vector<long long> authz = current.AuthorizationIds();
	std::vector<long long> projects = current.ProjectIds();
	long long default_project = current.default_project;
	bool grants = false;
	std::vector<std::string> names;
	if (get_list(op, "Authorizations", names) != Field::Absent) {
		ResolveNames(PlacementEntity::Authorization, names, authz);
		notes.changed.emplace_back("Authorizations");
		grants = true;
	}
	if (get_list(op, "Projects", names) != Field::Absent) {
		ResolveNames(PlacementEntity::Project, names, projects);
		notes.changed.emplace_back("Projects");
		grants = true;
	}
	f = get_string(op, "DefaultProject", val);
	if (f == Field::Undefined) {
		default_project = 0;
		notes.changed.emplace_back("DefaultProject");
		grants = true;
	} else if (f == Field::Value) {
		RowInfo proj;
		if (!FindRow(PlacementEntity::Project, "", val, proj) ||
			std::find(projects.begin(), projects.end(), proj.id) == projects.end()) {
			fail(PLACEMENT_ERR_INVALID, "DefaultProject '" + val + "' is not in Projects");
		}
		default_project = proj.id;
		notes.changed.emplace_back("DefaultProject");
		grants = true;
	} else if (std::find(projects.begin(), projects.end(), default_project) == projects.end()) {
		default_project = 0;	// the old default left the list
	}
	if (grants) {
		PlacementWriteGrants(m_stmts, row.id, authz, projects, default_project);
	}
	auto dropped = [](const std::vector<long long> &before, const std::vector<long long> &after) {
		return std::any_of(before.begin(), before.end(), [&](long long id) {
			return std::find(after.begin(), after.end(), id) == after.end();
		});
	};
	if (dropped(current.AuthorizationIds(), authz) || dropped(current.ProjectIds(), projects)) {
		revoke = true;
	}
	if (revoke) {
		m_audit.RevokeTokens(PlacementTokenBasis::Account, row.uuid);
	}
}

void
EditBatch::UpdateProject(const RowInfo &row, const ClassAd &op, UpdateNotes &notes)
{
	const PlacementEntity t = PlacementEntity::Project;
	std::string val;
	Field f = NewKey(t, op, val);
	if (f == Field::Undefined) {
		fail(PLACEMENT_ERR_INVALID, "a Project must have a Name");
	}
	if (f == Field::Value && val != row.name) {
		CheckName(t, val);
		CheckUnique(t, val, row.id);
		SetColumn(t, row.id, "name", val);
		notes.changed.emplace_back("Name");
		if (strcasecmp(val.c_str(), row.name.c_str()) != 0) {
			notes.warning = "Project renamed from " + row.name + " to " + val +
				"; only future tokens use the new name, and the schedd project record was not renamed";
			std::string mapfile = MapfileStillNames(t, row.name);
			if (!mapfile.empty()) {
				notes.warning += "; " + mapfile;
			}
		}
	}
	f = get_text(op, "Description", PLACEMENTD_MAX_LONG_TEXT, val);
	if (f != Field::Absent) {
		std::optional<std::string> v;
		if (f == Field::Value) {
			v = val;
		}
		SetColumn(t, row.id, "description", v);
		notes.changed.emplace_back("Description");
	}
}

void
EditBatch::UpdateAuthorization(const RowInfo &row, const ClassAd &op, UpdateNotes &notes)
{
	const PlacementEntity t = PlacementEntity::Authorization;
	std::string val;
	Field f = NewKey(t, op, val);
	if (f == Field::Undefined) {
		fail(PLACEMENT_ERR_INVALID, "an Authorization must have a Name");
	}
	std::string name = row.name;
	if (f == Field::Value && val != row.name) {
		// Grants refer to the row, not the name: every holder would mint
		// the level from its next login.  A change of case is no change.
		if (strcasecmp(val.c_str(), row.name.c_str()) != 0 && getPermissionFromString(val.c_str()) != NOT_A_PERM) {
			fail(PLACEMENT_ERR_INVALID, "renaming authorization '" + row.name + "' to the permission level '" + val +
				"' would grant it to every account holding it; create the level and grant it explicitly");
		}
		CheckName(t, val);
		CheckUnique(t, val, row.id);
		SetColumn(t, row.id, "name", val);
		notes.changed.emplace_back("Name");
		name = val;
		if (strcasecmp(val.c_str(), row.name.c_str()) != 0) {
			notes.revoke_holders = true;
			std::string mapfile = MapfileStillNames(t, row.name);
			if (!mapfile.empty()) {
				notes.warning = "Authorization renamed from " + row.name + " to " + val + "; " + mapfile;
			}
		}
	}
	// Undefined restores a field's default.
	f = get_text(op, "Label", PLACEMENTD_MAX_SHORT_TEXT, val);
	if (f != Field::Absent) {
		SetColumn(t, row.id, "label", (f == Field::Value && !val.empty()) ? val : name);
		notes.changed.emplace_back("Label");
	}
	f = get_text(op, "Color", PLACEMENTD_MAX_SHORT_TEXT, val);
	if (f != Field::Absent) {
		SetColumn(t, row.id, "color", (f == Field::Value && !val.empty()) ? val : std::string("normal"));
		notes.changed.emplace_back("Color");
	}
	f = get_text(op, "Description", PLACEMENTD_MAX_LONG_TEXT, val);
	if (f != Field::Absent) {
		SetColumn(t, row.id, "description", f == Field::Value ? val : std::string());
		notes.changed.emplace_back("Description");
	}
	bool requestable = true;
	f = get_bool(op, "Requestable", requestable);
	if (f != Field::Absent) {
		m_stmts.Get("UPDATE placementd_authorizations SET requestable = ?1 WHERE id = ?2")
			.bind(1, (long long)(f == Field::Undefined ? true : requestable)).bind(2, row.id).exec();
		notes.changed.emplace_back("Requestable");
	}
}

void
EditBatch::Update(PlacementEntity t, const ClassAd &op, ClassAd &result)
{
	RowInfo row = Select(t, op);
	CheckVersion(row, op);
	CheckOwner(t, row);

	// Accounts show the names of the projects and authorizations they hold,
	// and identities the name of their account, so renaming one is also a
	// change to each holder.
	std::vector<PlacementReferrer> holders;
	std::vector<ClassAd> holders_before;
	const PlacementEntity holder_type = (t == PlacementEntity::Account) ? PlacementEntity::Identity
		: PlacementEntity::Account;
	if (t != PlacementEntity::Identity) {
		holders = PlacementReferrers(m_stmts, t, row.id);
		for (const auto &holder: holders) {
			holders_before.push_back(PlacementSnapshot(m_stmts, holder_type, holder.id));
		}
	}

	UpdateNotes notes;
	long long version = m_audit.Modify(t, row.id, row.uuid, "update", [&](long long mod) {
		switch (t) {
		case PlacementEntity::Identity: UpdateIdentity(row, op, notes); break;
		case PlacementEntity::Account: UpdateAccount(row, op, notes); break;
		case PlacementEntity::Project: UpdateProject(row, op, notes); break;
		case PlacementEntity::Authorization: UpdateAuthorization(row, op, notes); break;
		}
		SetModification(t, row.id, mod);
		return row.id;
	});
	// A holder whose visible fields did not change gets no record.
	for (size_t i = 0; i < holders.size(); i++) {
		const long long holder_id = holders[i].id;
		const std::string &holder_uuid = holders[i].uuid;
		const bool revoke = notes.revoke_holders;
		m_audit.Modify(holder_type, holder_id, holder_uuid, "update", holders_before[i],
			[this, holder_type, holder_id, &holder_uuid, revoke](long long mod) {
				SetModification(holder_type, holder_id, mod);
				if (revoke) {
					m_audit.RevokeTokens(PlacementTokenBasis::Account, holder_uuid);
				}
				return holder_id;
			});
	}

	// An adopted row whose key the map file still defines is reclaimed by
	// the next reconcile, which undoes this update.
	RowInfo updated;
	if (version != row.version && FindRow(t, row.uuid, "", updated) && StillInFile(t, updated.name)) {
		if (!notes.warning.empty()) {
			notes.warning += "; ";
		}
		notes.warning += "still named in the map file, so the next reconcile will reclaim: " + updated.name;
	}

	result.Assign("Id", row.uuid);
	result.Assign("Version", version);
	if (!notes.warning.empty()) {
		result.Assign("Warning", notes.warning);
	}
	if (version != row.version) {
		Audit("Update", t, row.uuid, join(notes.changed, ","));
	}
}

void
EditBatch::Delete(PlacementEntity t, const ClassAd &op, ClassAd &result)
{
	RowInfo row = Select(t, op);
	CheckVersion(row, op);
	CheckOwner(t, row);
	bool cascade = false;
	get_bool(op, "Cascade", cascade);
	std::string cascaded;

	std::vector<PlacementReferrer> refs = PlacementReferrers(m_stmts, t, row.id);
	if (!refs.empty()) {
		std::vector<std::string> names;
		bool mapfile_ref = false;
		for (const auto &ref: refs) {
			names.push_back(ref.name);
			mapfile_ref = mapfile_ref || ref.source == PLACEMENT_SOURCE_MAPFILE;
		}
		const char *what = (t == PlacementEntity::Account) ? "identities" : "accounts";
		if (!cascade) {
			fail(PLACEMENT_ERR_REFERENCED, std::string(PlacementEntityName(t)) + " '" + row.name +
				"' is referenced by " + what + " " + join(names, ",") + "; retry with Cascade");
		}
		if (mapfile_ref) {
			fail(PLACEMENT_ERR_OWNER, std::string(PlacementEntityName(t)) + " '" + row.name +
				"' is referenced by mapfile " + what);
		}
		for (const auto &ref: refs) {
			const long long ref_id = ref.id;
			if (t == PlacementEntity::Account) {
				// The identities go with their account.
				const std::string &ref_uuid = ref.uuid;
				m_audit.Modify(PlacementEntity::Identity, ref_id, ref_uuid, "delete", [this, ref_id, &ref_uuid](long long) {
					m_stmts.Get("DELETE FROM placementd_identities WHERE id = ?1").bind(1, ref_id).exec();
					m_audit.RevokeTokens(PlacementTokenBasis::Identity, ref_uuid);
					return 0LL;
				});
			} else {
				// Losing the grant is a modification of each account.
				std::string sql;
				formatstr(sql, "DELETE FROM %s WHERE account_id = ?1 AND %s = ?2",
					t == PlacementEntity::Project ? "placementd_account_projects" : "placementd_account_authorizations",
					t == PlacementEntity::Project ? "project_id" : "authorization_id");
				// ...which narrows it, revoking its tokens.
				const long long target_id = row.id;
				const std::string &ref_uuid = ref.uuid;
				m_audit.Modify(PlacementEntity::Account, ref_id, ref_uuid, "update",
					[this, sql, ref_id, target_id, &ref_uuid](long long mod) {
						m_stmts.Get(sql).bind(1, ref_id).bind(2, target_id).exec();
						SetModification(PlacementEntity::Account, ref_id, mod);
						m_audit.RevokeTokens(PlacementTokenBasis::Account, ref_uuid);
						return ref_id;
					});
			}
		}
		cascaded = std::string(what) + "=" + join(names, ",");
	}

	m_audit.Modify(t, row.id, row.uuid, "delete", [&](long long) {
		std::string sql;
		formatstr(sql, "DELETE FROM %s WHERE id = ?1", PlacementEntityTable(t));
		m_stmts.Get(sql).bind(1, row.id).exec();
		if (t == PlacementEntity::Identity) {
			m_audit.RevokeTokens(PlacementTokenBasis::Identity, row.uuid);
		} else if (t == PlacementEntity::Account) {
			// Tokens of identities that have since left the account.
			m_audit.RevokeTokens(PlacementTokenBasis::Account, row.uuid);
		}
		return 0LL;
	});

	result.Assign("Id", row.uuid);
	Audit("Delete", t, row.uuid, cascaded.empty() ? row.name : row.name + " " + cascaded);
}

void
EditBatch::Revoke(PlacementEntity t, const ClassAd &op, ClassAd &result)
{
	if (t != PlacementEntity::Identity && t != PlacementEntity::Account) {
		fail(PLACEMENT_ERR_UNKNOWN_TYPE, "Revoke is only valid for an Identity or an Account");
	}
	RowInfo row = Select(t, op);
	CheckVersion(row, op);
	// Revoking changes no field, so a map file row needs no Adopt.  It is
	// recorded as an update whose Changes hold only RevokedTokens; one that
	// revokes nothing writes nothing.
	long long count = 0;
	long long version = m_audit.Modify(t, row.id, row.uuid, "update", [&](long long mod) {
		SetModification(t, row.id, mod);
		count = m_audit.RevokeTokens(t == PlacementEntity::Identity ? PlacementTokenBasis::Identity
			: PlacementTokenBasis::Account, row.uuid);
		return row.id;
	});

	result.Assign("Id", row.uuid);
	result.Assign("Version", version);
	result.Assign("RevokedTokens", count);
}

void
EditBatch::Adopt(PlacementEntity t, const ClassAd &op, ClassAd &result)
{
	RowInfo row = Select(t, op);
	CheckVersion(row, op);

	long long version = row.version;
	std::vector<std::string> warn_keys;
	if (row.source != PLACEMENT_SOURCE_INTERNAL) {
		version = ChangeSource(t, row.id, row.uuid, "adopt", PLACEMENT_SOURCE_INTERNAL);
		if (StillInFile(t, row.name)) {
			warn_keys.push_back(row.name);
		}
	}
	std::string adopted_identities;
	if (t == PlacementEntity::Account) {
		// An account's mapfile identities come with it.
		std::vector<std::string> names;
		for (const auto &ref: PlacementReferrers(m_stmts, t, row.id)) {
			if (ref.source != PLACEMENT_SOURCE_MAPFILE) {
				continue;
			}
			ChangeSource(PlacementEntity::Identity, ref.id, ref.uuid, "adopt", PLACEMENT_SOURCE_INTERNAL);
			names.push_back(ref.name);
			if (StillInFile(PlacementEntity::Identity, ref.name)) {
				warn_keys.push_back(ref.name);
			}
		}
		if (!names.empty()) {
			adopted_identities = "identities=" + join(names, ",");
		}
	}

	result.Assign("Id", row.uuid);
	result.Assign("Version", version);
	if (!warn_keys.empty()) {
		result.Assign("Warning", "still named in the map file, so the next reconcile will reclaim: " +
			join(warn_keys, ","));
	}
	Audit("Adopt", t, row.uuid, adopted_identities.empty() ? row.name : row.name + " " + adopted_identities);
}

void
EditBatch::AdoptAll(ClassAd &result)
{
	int count = 0;
	for (int i = 0; i < 4; i++) {
		PlacementEntity t = static_cast<PlacementEntity>(i);
		std::vector<std::pair<long long, std::string>> rows;
		std::string sql;
		formatstr(sql, "SELECT id, uuid FROM %s WHERE source = 'mapfile'", PlacementEntityTable(t));
		PlacementStmt &stmt = m_stmts.Get(sql);
		while (stmt.step()) {
			rows.emplace_back(stmt.getInt(0), stmt.getText(1));
		}
		for (const auto &row: rows) {
			ChangeSource(t, row.first, row.second, "adopt", PLACEMENT_SOURCE_INTERNAL);
			count++;
		}
	}
	result.Assign("Count", count);
	const PlacementMapfileKeys *keys = m_ctx.keys;
	if (count > 0 && keys && keys->valid &&
		(!keys->identities.empty() || !keys->accounts.empty() ||
		 !keys->projects.empty() || !keys->authorizations.empty())) {
		result.Assign("Warning", "the map file still names keys, so the next reconcile will reclaim them");
	}
	std::string line;
	formatstr(line, "PLACEMENT_EDIT by %s: Adopt All (%d rows)", m_ctx.peer.c_str(), count);
	m_audit_lines.emplace_back(PlacementSanitize(line, 8192));
}

} // namespace

void
PlacementRunEdit(PlacementDb &db, const std::vector<ClassAd> &ops, const PlacementEditContext &ctx,
	PlacementEditOutcome &outcome)
{
	outcome = PlacementEditOutcome();
	PlacementLockRelease release(db.Statements());
	// A failed batch rolled back: the Id of a row it created and every
	// Version describe writes that never happened (and may be reused).
	auto strip_rolled_back = [&outcome]() {
		for (auto &result: outcome.results) {
			result.Delete("Id");
			result.Delete("Version");
		}
	};
	try {
		PlacementTxn txn(db.Statements());
		EditBatch batch(db.Statements(), ctx);
		size_t index = 0;
		for (; index < ops.size(); index++) {
			ClassAd result;
			result.Assign(ATTR_MY_TYPE, "PlacementEditResult");
			result.Assign("Index", (long long)index);
			try {
				batch.Execute(ops[index], result);
			} catch (const PlacementError &e) {
				result.Assign(ATTR_ERROR_CODE, e.code());
				result.Assign(ATTR_ERROR_STRING, e.what());
				outcome.error_code = e.code();
				formatstr(outcome.error_string, "operation %zu: %s", index, e.what());
				outcome.results.emplace_back(std::move(result));
				break;
			}
			// A dry run's ids are reused by the next real write, so it
			// reports no Version.
			if (ctx.dry_run) {
				result.Delete("Version");
			}
			outcome.results.emplace_back(std::move(result));
		}
		// Operations after a failure were not attempted.
		for (index++; index < ops.size(); index++) {
			ClassAd result;
			result.Assign(ATTR_MY_TYPE, "PlacementEditResult");
			result.Assign("Index", (long long)index);
			outcome.results.emplace_back(std::move(result));
		}
		if (outcome.error_code || ctx.dry_run) {
			txn.rollback();
			if (outcome.error_code) {
				strip_rolled_back();
			}
			return;
		}
		txn.commit();
		outcome.committed = true;
		outcome.changeset_id = batch.ChangesetId();
		outcome.audit = std::move(batch.AuditLines());
	} catch (const PlacementError &e) {
		// BEGIN or COMMIT failed (e.g. the database stayed locked).
		outcome.error_code = e.code();
		outcome.error_string = e.what();
		outcome.committed = false;
		outcome.audit.clear();
		strip_rolled_back();
	}
}
