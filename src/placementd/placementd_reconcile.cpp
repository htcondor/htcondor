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
#include "stl_string_utils.h"
#include "safe_fopen.h"
#include "placementd_reconcile.h"

#include <algorithm>
#include <map>

namespace {

using CaselessSet = std::set<std::string, classad::CaseIgnLTStr>;
template <class T> using CaselessMap = std::map<std::string, T, classad::CaseIgnLTStr>;

// A file modified this recently may still be changing within the same
// mtime tick, so its signature is not trusted for the stat-skip.
constexpr time_t MAPFILE_SETTLE_SECONDS = 2;

// ------------------------------------------------------------------ parse

struct UserLine
{
	std::string identity;
	std::string account;
	std::vector<std::string> authz;
	std::vector<std::string> projects;
	std::optional<long long> expires;	// nullopt = never
	int line{0};
};

struct AuthzLine
{
	std::string name;
	std::string label;
	std::string color;
	std::string description;
};

struct Parsed
{
	std::vector<UserLine> lines;			// well-formed user lines, duplicates included
	CaselessMap<AuthzLine> authz;			// well-formed, unique authorization lines
	// Identities with a malformed line, and the earliest expiry those lines
	// give (nullopt if none parses, or if it is "never").
	std::map<std::string, std::optional<long long>> malformed;
	CaselessSet q_authz;					// malformed or duplicate authorization lines
	CaselessSet rejected;					// names that may not be granted
	std::vector<PlacementProblem> problems;
	PlacementMapfileKeys keys;
	// The authorizations file could not be read (and had been read
	// before): authorization display metadata keeps its previous state.
	std::optional<PlacementProblem> authz_file_problem;
	bool authz_file_missing{false};
};

// Whether two caseless sets hold the same names, ignoring case (operator==
// on the sets compares the spellings).
bool
caseless_equal(const CaselessSet &a, const CaselessSet &b)
{
	return a.size() == b.size() &&
		std::all_of(a.begin(), a.end(), [&](const std::string &name) { return b.count(name) > 0; });
}

// The expiry field of a user line: false if it is not a number.  0 (and
// anything negative) means never.
bool
parse_expiry(const std::string &field, std::optional<long long> &out)
{
	char *end = nullptr;
	errno = 0;
	long long exp = strtoll(field.c_str(), &end, 10);
	if (errno != 0 || end == field.c_str() || *end != '\0') {
		return false;
	}
	out.reset();
	if (exp > 0) {
		out = exp;
	}
	return true;
}

// A file's identity for the stat-skip, and its mtime.
bool
stat_signature(const std::string &path, std::string &sig, time_t &mtime)
{
	struct stat st;
	if (stat(path.c_str(), &st) != 0) {
		return false;
	}
#if defined(DARWIN)
	long long mtime_ns = (long long)st.st_mtimespec.tv_nsec;
#elif defined(LINUX)
	long long mtime_ns = (long long)st.st_mtim.tv_nsec;
#else
	long long mtime_ns = 0;
#endif
	formatstr(sig, "%s:%lld:%lld:%lld:%lld.%09lld", path.c_str(), (long long)st.st_dev,
		(long long)st.st_ino, (long long)st.st_size, (long long)st.st_mtime, mtime_ns);
	mtime = st.st_mtime;
	return true;
}

// Split on '|' keeping empty fields, trimming each.
std::vector<std::string>
split_bar(const std::string &line)
{
	std::vector<std::string> out;
	size_t start = 0;
	while (true) {
		size_t bar = line.find('|', start);
		std::string field = line.substr(start, bar == std::string::npos ? std::string::npos : bar - start);
		trim(field);
		out.emplace_back(std::move(field));
		if (bar == std::string::npos) {
			break;
		}
		start = bar + 1;
	}
	return out;
}

void
add_problem(Parsed &p, const char *kind, const std::string &file, int line,
	const std::string &key, const std::string &message)
{
	// The key and message may quote a bad line.
	std::string safe_key = PlacementSanitize(key, PLACEMENTD_MAX_IDENTITY);
	std::string safe_message = PlacementSanitize(message, PLACEMENTD_MAX_LONG_TEXT);
	dprintf(D_ERROR, "Placementd mapfile %s:%d: %s%s%s\n", file.c_str(), line, safe_message.c_str(),
		safe_key.empty() ? "" : " for ", safe_key.c_str());
	p.problems.push_back(PlacementProblem{kind, file, line, safe_key, safe_message});
}

// Names from one comma list that may be granted, de-duplicated
// case-insensitively in order.  The rest are reported and dropped.
std::vector<std::string>
grantable_names(Parsed &p, PlacementEntity t, const std::string &field, const std::string &file, int line)
{
	std::vector<std::string> out;
	CaselessSet seen;
	for (const auto &name: split(field, ",")) {
		std::string why;
		if (!PlacementValidName(t, name, why)) {
			add_problem(p, "Rejected", file, line, name, std::string(PlacementEntityName(t)) + " name " + why);
			p.rejected.insert(name);
			continue;
		}
		if (t == PlacementEntity::Authorization && PlacementPrivilegedAuthorization(name)) {
			add_problem(p, "Rejected", file, line, name, "authorization is a privileged permission level");
			p.rejected.insert(name);
			continue;
		}
		if (seen.insert(name).second) {
			out.push_back(name);
		}
	}
	return out;
}

// Returns false if the file could not be read; missing is set for ENOENT.
bool
parse_user_mapfile(const std::string &path, const std::string &uid_domain, Parsed &p,
	std::string &err, bool &missing)
{
	missing = false;
	FILE *fp = safe_fopen_wrapper(path.c_str(), "r");
	if (!fp) {
		missing = (errno == ENOENT);
		formatstr(err, "Failed to open map file %s: %s", path.c_str(), strerror(errno));
		return false;
	}

	std::string line;
	int lineno = 0;
	while (readLine(line, fp)) {
		lineno++;
		trim(line);
		if (line.empty() || line[0] == '#') {
			continue;
		}
		std::vector<std::string> items = split(line, " \t\n");
		if (items.empty()) {
			continue;
		}
		const std::string &identity = items[0];
		p.keys.identities.insert(identity);

		// A malformed line is not applied, but an expiry in its fourth
		// field still moves the identity's expiry earlier.
		auto malformed = [&](const std::string &why) {
			add_problem(p, "Malformed", path, lineno, identity, why);
			std::optional<long long> line_exp;
			if (items.size() < 4 || !parse_expiry(items[3], line_exp)) {
				line_exp.reset();
			}
			auto it = p.malformed.find(identity);
			if (it == p.malformed.end()) {
				p.malformed.emplace(identity, line_exp);
			} else if (line_exp) {
				it->second = it->second ? std::min(*it->second, *line_exp) : *line_exp;
			}
		};
		std::string why;
		if (!PlacementValidName(PlacementEntity::Identity, identity, why)) {
			malformed("identity " + why);
			continue;
		}
		if (items.size() < 4 || items.size() > 5) {
			malformed("wrong number of fields");
			continue;
		}
		std::optional<long long> expires;
		if (!parse_expiry(items[3], expires)) {
			malformed("expiration is not a number");
			continue;
		}
		std::string account = PlacementCanonicalAccount(items[1], identity, uid_domain);
		if (!PlacementValidName(PlacementEntity::Account, account, why)) {
			malformed("account " + why);
			continue;
		}
		if (PlacementReservedAccount(account)) {
			malformed("account " + account + " is reserved");
			continue;
		}

		UserLine ul;
		ul.identity = identity;
		ul.account = account;
		ul.authz = grantable_names(p, PlacementEntity::Authorization, items[2], path, lineno);
		if (items.size() == 5) {
			ul.projects = grantable_names(p, PlacementEntity::Project, items[4], path, lineno);
		}
		ul.expires = expires;
		ul.line = lineno;
		// Projects and authorizations named on a user line are references,
		// not definitions: the reconcile never reclaims them.
		p.keys.accounts.insert(ul.account);
		for (const auto &name: ul.projects) { p.keys.project_refs[name].insert(ul.account); }
		for (const auto &name: ul.authz) { p.keys.authorization_refs[name].insert(ul.account); }
		p.lines.emplace_back(std::move(ul));
	}
	bool read_error = ferror(fp) != 0;
	fclose(fp);
	if (read_error) {
		formatstr(err, "Error reading map file %s", path.c_str());
		return false;
	}
	return true;
}

bool
parse_authz_mapfile(const std::string &path, Parsed &p, std::string &err, bool &missing)
{
	missing = false;
	FILE *fp = safe_fopen_wrapper(path.c_str(), "r");
	if (!fp) {
		missing = (errno == ENOENT);
		formatstr(err, "Failed to open authz map file %s: %s", path.c_str(), strerror(errno));
		return false;
	}

	std::string line;
	int lineno = 0;
	CaselessMap<std::vector<int>> seen;
	while (readLine(line, fp)) {
		lineno++;
		trim(line);
		if (line.empty() || line[0] == '#') {
			continue;
		}
		std::vector<std::string> items = split_bar(line);
		const std::string &name = items[0];
		std::string why;
		if (!PlacementValidName(PlacementEntity::Authorization, name, why)) {
			add_problem(p, "Rejected", path, lineno, name, "authorization name " + why);
			p.rejected.insert(name);
			continue;
		}
		if (PlacementPrivilegedAuthorization(name)) {
			add_problem(p, "Rejected", path, lineno, name, "authorization is a privileged permission level");
			p.rejected.insert(name);
			continue;
		}
		p.keys.authorizations.insert(name);
		seen[name].push_back(lineno);
		if (items.size() != 4) {
			add_problem(p, "Malformed", path, lineno, name, "wrong number of fields");
			p.q_authz.insert(name);
			continue;
		}
		AuthzLine al{items[0], PlacementSanitize(items[1], PLACEMENTD_MAX_SHORT_TEXT),
			PlacementSanitize(items[2], PLACEMENTD_MAX_SHORT_TEXT), PlacementSanitize(items[3], PLACEMENTD_MAX_LONG_TEXT)};
		if (al.label.empty()) {
			al.label = al.name;
		}
		if (al.color.empty()) {
			al.color = "normal";
		}
		p.authz[name] = al;
	}
	bool read_error = ferror(fp) != 0;
	fclose(fp);
	if (read_error) {
		formatstr(err, "Error reading authz map file %s", path.c_str());
		return false;
	}

	for (const auto &entry: seen) {
		if (entry.second.size() > 1) {
			for (int ln: entry.second) {
				add_problem(p, "Duplicate", path, ln, entry.first, "authorization appears on more than one line");
			}
			p.q_authz.insert(entry.first);
		}
	}
	for (const auto &name: p.q_authz) {
		p.authz.erase(name);
	}
	return true;
}

// ------------------------------------------------------------------ apply

struct Row
{
	long long id{0};
	std::string uuid;
	std::string name;
	std::string source;
	bool IsMapfile() const { return source == PLACEMENT_SOURCE_MAPFILE; }
};

struct AuthzRow : Row
{
	std::string label;
	std::string color;
	std::string description;
	bool requestable{false};
};

struct AccountRow : Row
{
	std::set<long long> authz;
	std::set<long long> projects;
	long long default_project{0};	// 0 = none
};

struct IdentityRow : Row
{
	long long account_id{0};
	std::optional<long long> expires;
};

struct DesiredAuthz
{
	std::string name;
	std::string label;
	std::string color;
	std::string description;
	bool requestable{false};
};

struct DesiredAccount
{
	std::vector<std::string> authz;
	std::vector<std::string> projects;
	std::string default_project;		// empty = none
};

struct DesiredIdentity
{
	std::string account;
	std::optional<long long> expires;
};

// The earlier of two expiries; nullopt means never.
std::optional<long long>
earliest(const std::optional<long long> &a, const std::optional<long long> &b)
{
	if (!a) { return b; }
	if (!b) { return a; }
	return std::min(*a, *b);
}

struct ApplyCounts
{
	int inserted{0};
	int updated{0};
	int deleted{0};
	int reclaimed{0};
	int retained{0};
	int quarantined{0};
};

class Apply
{
public:
	Apply(PlacementStatements &stmts, Parsed &parsed, const std::string &user_file)
		: m_stmts(stmts), m_parsed(parsed), m_user_file(user_file),
		  m_audit(stmts, PlacementChangesetInfo{"mapfile", "mapfile", {}, {}, {}}) {}

	void Run();
	const ApplyCounts &Counts() const { return m_counts; }
	long long ChangesetId() const { return m_audit.ChangesetId(); }
	long long RevokedTokens() const { return m_audit.RevokedTokens(); }

private:
	void LoadState();
	void ComputeIdentities();
	void ComputeAccounts();
	void ComputeAuthorizations();
	void ApplyAuthorizations();
	void ApplyProjects();
	void ApplyAccounts();
	void ApplyIdentities();
	void TightenQuarantined();
	void DeleteIdentities();
	void DeleteAccounts();
	void DeleteNamed(PlacementEntity t, const CaselessSet &keep);

	void Reclaimed(PlacementEntity t, const std::string &key);
	// Rename a project or authorization (write() changes the row and
	// returns its id), recording an update on each account that holds it,
	// since an account shows the names of its grants.
	void Rename(PlacementEntity t, const Row &row, const char *operation,
		const std::function<long long(long long)> &write);
	void Handoff(PlacementEntity t, const Row &row, const std::string &referrer);
	void Delete(PlacementEntity t, const Row &row);
	// The referrers decide a row's fate: an internal one keeps it alive as
	// internal (hand-off); a mapfile one (a quarantined key) keeps it as is.
	// Returns true if the row was deleted or handed off.
	bool Retire(PlacementEntity t, const Row &row, const char *referrer_kind);

	PlacementStatements &m_stmts;
	Parsed &m_parsed;
	const std::string &m_user_file;
	PlacementAudit m_audit;		// one change set per run that changes anything
	ApplyCounts m_counts;

	CaselessMap<AuthzRow> m_authz;
	CaselessMap<Row> m_projects;
	std::map<std::string, AccountRow> m_accounts;
	std::map<std::string, IdentityRow> m_identities;

	CaselessMap<DesiredAuthz> m_want_authz;
	CaselessMap<std::string> m_want_projects;
	// Mapfile authorizations whose display metadata stays as it is: the
	// authorizations file could not be read.
	CaselessSet m_keep_authz;
	std::map<std::string, DesiredAccount> m_want_accounts;
	std::map<std::string, DesiredIdentity> m_want_identities;
	// Identities whose input was not applied; their rows keep their state,
	// except that an earlier expiry from the file still applies.
	std::map<std::string, std::optional<long long>> m_q_identities;
};

void
Apply::LoadState()
{
	PlacementStmt &authz = m_stmts.Get("SELECT id, uuid, name, source, label, color, description, requestable"
		" FROM placementd_authorizations");
	while (authz.step()) {
		AuthzRow r;
		r.id = authz.getInt(0); r.uuid = authz.getText(1); r.name = authz.getText(2);
		r.source = authz.getText(3);
		r.label = authz.getText(4); r.color = authz.getText(5); r.description = authz.getText(6);
		r.requestable = authz.getInt(7) != 0;
		m_authz[r.name] = r;
	}
	PlacementStmt &proj = m_stmts.Get("SELECT id, uuid, name, source FROM placementd_projects");
	while (proj.step()) {
		Row r;
		r.id = proj.getInt(0); r.uuid = proj.getText(1); r.name = proj.getText(2); r.source = proj.getText(3);
		m_projects[r.name] = r;
	}
	PlacementStmt &acct = m_stmts.Get("SELECT id, uuid, name, source FROM placementd_accounts");
	while (acct.step()) {
		AccountRow r;
		r.id = acct.getInt(0); r.uuid = acct.getText(1); r.name = acct.getText(2); r.source = acct.getText(3);
		m_accounts[r.name] = r;
	}
	for (auto &entry: m_accounts) {
		PlacementGrants grants;
		PlacementReadGrants(m_stmts, entry.second.id, grants);
		for (long long id: grants.AuthorizationIds()) { entry.second.authz.insert(id); }
		for (long long id: grants.ProjectIds()) { entry.second.projects.insert(id); }
		entry.second.default_project = grants.default_project;
	}
	PlacementStmt &ident = m_stmts.Get("SELECT id, uuid, identity, source, account_id, expires_at FROM placementd_identities");
	while (ident.step()) {
		IdentityRow r;
		r.id = ident.getInt(0); r.uuid = ident.getText(1); r.name = ident.getText(2); r.source = ident.getText(3);
		r.account_id = ident.getInt(4); r.expires = ident.getOptInt(5);
		m_identities[r.name] = r;
	}
}

void
Apply::ComputeIdentities()
{
	std::map<std::string, std::vector<const UserLine *>> by_identity;
	for (const auto &ul: m_parsed.lines) {
		by_identity[ul.identity].push_back(&ul);
	}
	for (const auto &entry: by_identity) {
		const std::string &identity = entry.first;
		const auto &lines = entry.second;
		std::optional<long long> expires = lines.front()->expires;
		bool same_account = true;
		for (const UserLine *ul: lines) {
			expires = earliest(expires, ul->expires);
			same_account = same_account && ul->account == lines.front()->account;
		}
		if (lines.size() > 1) {
			for (const UserLine *ul: lines) {
				add_problem(m_parsed, "Duplicate", m_user_file, ul->line, identity,
					same_account ? "identity appears on more than one line; the earliest expiry applies"
						: "identity appears on more than one line for different accounts");
			}
		}
		if (m_parsed.malformed.count(identity) || !same_account) {
			// Not applied, but an earlier expiry still tightens the row.
			m_q_identities[identity] = expires;
			continue;
		}
		m_want_identities[identity] = DesiredIdentity{lines.front()->account, expires};
	}
	// So does an expiry on a malformed line.
	for (const auto &entry: m_parsed.malformed) {
		auto it = m_q_identities.find(entry.first);
		if (it == m_q_identities.end()) {
			m_q_identities.emplace(entry.first, entry.second);
		} else if (entry.second) {
			it->second = earliest(it->second, entry.second);
		}
	}
}

void
Apply::ComputeAccounts()
{
	// Lines of applied identities, grouped by account.
	std::map<std::string, std::vector<const UserLine *>> groups;
	for (const auto &ul: m_parsed.lines) {
		if (m_want_identities.count(ul.identity)) {
			groups[ul.account].push_back(&ul);
		}
	}
	for (const auto &entry: groups) {
		const std::string &account = entry.first;
		const auto &lines = entry.second;
		const UserLine *first = lines.front();
		CaselessSet first_authz(first->authz.begin(), first->authz.end());
		CaselessSet first_projects(first->projects.begin(), first->projects.end());

		// Authorizations: the intersection (never more access than every
		// line grants).  Projects: the union (they are accounting, not
		// privilege).  Default: the first line's.  Names compare without
		// case, as everywhere else.
		DesiredAccount da;
		da.projects = first->projects;
		CaselessSet in_union = first_projects;
		std::vector<std::string> all_authz;		// every line's, in order
		CaselessSet in_all_authz;
		bool conflict = false;
		for (const UserLine *ul: lines) {
			CaselessSet authz(ul->authz.begin(), ul->authz.end());
			CaselessSet projects(ul->projects.begin(), ul->projects.end());
			bool same_first = (first->projects.empty() && ul->projects.empty()) ||
				(!first->projects.empty() && !ul->projects.empty() &&
				 strcasecmp(first->projects[0].c_str(), ul->projects[0].c_str()) == 0);
			if (!caseless_equal(authz, first_authz) || !caseless_equal(projects, first_projects) || !same_first) {
				conflict = true;
			}
			for (const auto &name: ul->projects) {
				if (in_union.insert(name).second) {
					da.projects.push_back(name);
				}
			}
			for (const auto &name: ul->authz) {
				if (in_all_authz.insert(name).second) {
					all_authz.push_back(name);
				}
			}
		}
		std::vector<std::string> dropped;
		for (const auto &name: all_authz) {
			bool everywhere = std::all_of(lines.begin(), lines.end(), [&](const UserLine *ul) {
				return std::any_of(ul->authz.begin(), ul->authz.end(), [&](const std::string &other) {
					return strcasecmp(other.c_str(), name.c_str()) == 0;
				});
			});
			(everywhere ? da.authz : dropped).push_back(name);
		}
		if (!first->projects.empty()) {
			da.default_project = first->projects.front();
		}
		if (conflict) {
			for (const UserLine *ul: lines) {
				add_problem(m_parsed, "AccountConflict", m_user_file, ul->line, account,
					"lines for this account disagree; granting the authorizations they share and the projects of any");
			}
		}
		if (!dropped.empty()) {
			std::vector<std::string> line_numbers;
			for (const UserLine *ul: lines) {
				line_numbers.push_back(std::to_string(ul->line));
			}
			add_problem(m_parsed, "GrantsNarrowed", m_user_file, first->line, account,
				"authorizations " + join(dropped, ",") + " are not granted by every line for this account (lines " +
				join(line_numbers, ",") + "), so the account does not get them");
		}
		if (da.authz.empty()) {
			add_problem(m_parsed, "NoAuthorizations", m_user_file, first->line, account,
				"account has no authorizations, so its logins will be refused");
		}
		m_want_accounts[account] = da;
	}
}

void
Apply::ComputeAuthorizations()
{
	// Projects and user-line authorizations come from every well-formed
	// line: the file names those keys even if a line is not applied.  They
	// are references: they need a row to exist, and do not define it.
	for (const auto &ul: m_parsed.lines) {
		for (const auto &name: ul.projects) {
			m_want_projects.emplace(name, name);
		}
	}

	if (m_parsed.authz_file_problem) {
		// Without the authorizations file, what it defined is unknown, so
		// every mapfile authorization keeps its display metadata (and is not
		// deleted).
		for (const auto &entry: m_authz) {
			if (entry.second.IsMapfile()) {
				m_keep_authz.insert(entry.first);
			}
		}
	}

	// Authorizations-file lines define their rows, and reclaim internal ones.
	for (const auto &entry: m_parsed.authz) {
		const AuthzLine &al = entry.second;
		m_want_authz[entry.first] = DesiredAuthz{al.name, al.label, al.color, al.description, true};
	}
	for (const auto &ul: m_parsed.lines) {
		for (const auto &name: ul.authz) {
			if (m_want_authz.count(name) || m_keep_authz.count(name)) {
				continue;
			}
			auto it = m_authz.find(name);
			// A reference to an internal row uses it as it is.
			if (it != m_authz.end() && !it->second.IsMapfile()) {
				continue;
			}
			// A quarantined authorizations-file line keeps the row's display
			// metadata; with no row yet, a placeholder lets accounts grant it.
			if (m_parsed.q_authz.count(name) && it != m_authz.end()) {
				continue;
			}
			m_want_authz[name] = DesiredAuthz{name, name, "normal", "", false};
		}
	}
}

void
Apply::Reclaimed(PlacementEntity t, const std::string &key)
{
	m_counts.reclaimed++;
	dprintf(D_ALWAYS, "Placementd mapfile reclaimed %s '%s' from internal (internal edits overwritten)\n",
		PlacementEntityName(t), key.c_str());
	m_parsed.problems.push_back(PlacementProblem{"Reclaimed", m_user_file, 0, key,
		std::string("internal ") + PlacementEntityName(t) + " reclaimed by the map file; internal edits overwritten"});
}

void
Apply::Rename(PlacementEntity t, const Row &row, const char *operation,
	const std::function<long long(long long)> &write)
{
	std::vector<PlacementReferrer> holders = PlacementReferrers(m_stmts, t, row.id);
	std::vector<ClassAd> before;
	for (const auto &holder: holders) {
		before.push_back(PlacementSnapshot(m_stmts, PlacementEntity::Account, holder.id));
	}
	m_audit.Modify(t, row.id, row.uuid, operation, write);
	for (size_t i = 0; i < holders.size(); i++) {
		const long long holder_id = holders[i].id;
		m_audit.Modify(PlacementEntity::Account, holder_id, holders[i].uuid, "update", before[i],
			[this, holder_id](long long mod) {
				m_stmts.Get("UPDATE placementd_accounts SET modification_id = ?1 WHERE id = ?2")
					.bind(1, mod).bind(2, holder_id).exec();
				return holder_id;
			});
	}
}

void
Apply::Handoff(PlacementEntity t, const Row &row, const std::string &referrer)
{
	m_audit.Modify(t, row.id, row.uuid, "handoff", [&](long long mod) {
		std::string sql;
		formatstr(sql, "UPDATE %s SET source = 'internal', modification_id = ?1 WHERE id = ?2",
			PlacementEntityTable(t));
		m_stmts.Get(sql).bind(1, mod).bind(2, row.id).exec();
		return row.id;
	});
	m_counts.retained++;
	dprintf(D_ALWAYS, "Placementd mapfile retained %s '%s' as internal: still referenced by %s\n",
		PlacementEntityName(t), row.name.c_str(), referrer.c_str());
}

void
Apply::Delete(PlacementEntity t, const Row &row)
{
	m_audit.Modify(t, row.id, row.uuid, "delete", [&](long long) {
		std::string sql;
		formatstr(sql, "DELETE FROM %s WHERE id = ?1", PlacementEntityTable(t));
		m_stmts.Get(sql).bind(1, row.id).exec();
		if (t == PlacementEntity::Identity) {
			m_audit.RevokeTokens(PlacementTokenBasis::Identity, row.uuid);
		} else if (t == PlacementEntity::Account) {
			m_audit.RevokeTokens(PlacementTokenBasis::Account, row.uuid);
		}
		return 0LL;
	});
	m_counts.deleted++;
}

bool
Apply::Retire(PlacementEntity t, const Row &row, const char *referrer_kind)
{
	std::string internal_ref;
	bool mapfile_ref = false;
	for (const auto &referrer: PlacementReferrers(m_stmts, t, row.id)) {
		if (referrer.source == PLACEMENT_SOURCE_MAPFILE) {
			mapfile_ref = true;
		} else if (internal_ref.empty()) {
			internal_ref = std::string(referrer_kind) + " " + referrer.name;
		}
	}
	if (!internal_ref.empty()) {
		Handoff(t, row, internal_ref);
		return true;
	}
	if (mapfile_ref) {
		return false;
	}
	Delete(t, row);
	return true;
}

void
Apply::ApplyAuthorizations()
{
	for (const auto &entry: m_want_authz) {
		const DesiredAuthz &want = entry.second;
		auto it = m_authz.find(entry.first);
		if (it == m_authz.end()) {
			AuthzRow r;
			r.uuid = PlacementNewUuid();
			m_audit.Modify(PlacementEntity::Authorization, 0, r.uuid, "create", [&](long long mod) {
				m_stmts.Get("INSERT INTO placementd_authorizations (uuid, name, label, color, description,"
					" requestable, source, modification_id) VALUES (?1, ?2, ?3, ?4, ?5, ?6, 'mapfile', ?7)")
					.bind(1, r.uuid).bind(2, want.name).bind(3, want.label).bind(4, want.color)
					.bind(5, want.description).bind(6, (long long)want.requestable).bind(7, mod).exec();
				r.id = sqlite3_last_insert_rowid(m_stmts.handle());
				return r.id;
			});
			r.name = want.name; r.source = PLACEMENT_SOURCE_MAPFILE;
			m_authz[entry.first] = r;
			m_counts.inserted++;
			continue;
		}
		AuthzRow &r = it->second;
		bool reclaim = !r.IsMapfile();
		bool changed = reclaim || r.name != want.name || r.label != want.label || r.color != want.color ||
			r.description != want.description || r.requestable != want.requestable;
		if (!changed) {
			continue;
		}
		Rename(PlacementEntity::Authorization, r, reclaim ? "reclaim" : "update", [&](long long mod) {
			m_stmts.Get("UPDATE placementd_authorizations SET name = ?1, label = ?2, color = ?3,"
				" description = ?4, requestable = ?5, source = 'mapfile', modification_id = ?6 WHERE id = ?7")
				.bind(1, want.name).bind(2, want.label).bind(3, want.color).bind(4, want.description)
				.bind(5, (long long)want.requestable).bind(6, mod).bind(7, r.id).exec();
			return r.id;
		});
		if (reclaim) {
			Reclaimed(PlacementEntity::Authorization, want.name);
		} else {
			m_counts.updated++;
		}
		r.name = want.name; r.source = PLACEMENT_SOURCE_MAPFILE;
	}
}

void
Apply::ApplyProjects()
{
	for (const auto &entry: m_want_projects) {
		const std::string &spelling = entry.second;
		auto it = m_projects.find(entry.first);
		if (it == m_projects.end()) {
			Row r;
			r.uuid = PlacementNewUuid();
			m_audit.Modify(PlacementEntity::Project, 0, r.uuid, "create", [&](long long mod) {
				m_stmts.Get("INSERT INTO placementd_projects (uuid, name, description, source,"
					" modification_id) VALUES (?1, ?2, '', 'mapfile', ?3)")
					.bind(1, r.uuid).bind(2, spelling).bind(3, mod).exec();
				r.id = sqlite3_last_insert_rowid(m_stmts.handle());
				return r.id;
			});
			r.name = spelling; r.source = PLACEMENT_SOURCE_MAPFILE;
			m_projects[entry.first] = r;
			m_counts.inserted++;
			continue;
		}
		// A user line only refers to a project: an internal one is used as
		// it is, and a mapfile one follows the file's spelling.
		Row &r = it->second;
		if (!r.IsMapfile() || r.name == spelling) {
			continue;
		}
		Rename(PlacementEntity::Project, r, "update", [&](long long mod) {
			m_stmts.Get("UPDATE placementd_projects SET name = ?1, modification_id = ?2"
				" WHERE id = ?3").bind(1, spelling).bind(2, mod).bind(3, r.id).exec();
			return r.id;
		});
		m_counts.updated++;
		r.name = spelling;
	}
}

void
Apply::ApplyAccounts()
{
	for (const auto &entry: m_want_accounts) {
		const std::string &name = entry.first;
		const DesiredAccount &want = entry.second;

		// Every name was created above, as a placeholder if need be.
		std::vector<long long> authz_ids, project_ids;
		long long default_id = 0;
		for (const auto &a: want.authz) {
			auto it = m_authz.find(a);
			if (it == m_authz.end()) {
				throw PlacementError(PLACEMENT_ERR_DATABASE, "authorization " + a + " vanished during reconcile");
			}
			authz_ids.push_back(it->second.id);
		}
		for (const auto &p: want.projects) {
			auto it = m_projects.find(p);
			if (it == m_projects.end()) {
				throw PlacementError(PLACEMENT_ERR_DATABASE, "project " + p + " vanished during reconcile");
			}
			project_ids.push_back(it->second.id);
			if (strcasecmp(p.c_str(), want.default_project.c_str()) == 0) {
				default_id = it->second.id;
			}
		}

		auto it = m_accounts.find(name);
		if (it == m_accounts.end()) {
			AccountRow r;
			r.uuid = PlacementNewUuid();
			m_audit.Modify(PlacementEntity::Account, 0, r.uuid, "create", [&](long long mod) {
				m_stmts.Get("INSERT INTO placementd_accounts (uuid, name, source, modification_id)"
					" VALUES (?1, ?2, 'mapfile', ?3)").bind(1, r.uuid).bind(2, name).bind(3, mod).exec();
				r.id = sqlite3_last_insert_rowid(m_stmts.handle());
				PlacementWriteGrants(m_stmts, r.id, authz_ids, project_ids, default_id);
				return r.id;
			});
			r.name = name; r.source = PLACEMENT_SOURCE_MAPFILE;
			m_accounts[name] = r;
			m_counts.inserted++;
			continue;
		}

		AccountRow &r = it->second;
		bool reclaim = !r.IsMapfile();
		std::set<long long> want_authz(authz_ids.begin(), authz_ids.end());
		std::set<long long> want_projects(project_ids.begin(), project_ids.end());
		bool grants_changed = want_authz != r.authz || want_projects != r.projects || default_id != r.default_project;
		if (!reclaim && !grants_changed) {
			continue;
		}
		// Narrowed grants revoke the account's tokens, which they bound.
		bool narrowed = !std::includes(want_authz.begin(), want_authz.end(), r.authz.begin(), r.authz.end()) ||
			!std::includes(want_projects.begin(), want_projects.end(), r.projects.begin(), r.projects.end());
		m_audit.Modify(PlacementEntity::Account, r.id, r.uuid, reclaim ? "reclaim" : "update", [&](long long mod) {
			if (grants_changed) {
				PlacementWriteGrants(m_stmts, r.id, authz_ids, project_ids, default_id);
			}
			if (narrowed) {
				m_audit.RevokeTokens(PlacementTokenBasis::Account, r.uuid);
			}
			m_stmts.Get("UPDATE placementd_accounts SET source = 'mapfile', modification_id = ?1 WHERE id = ?2")
				.bind(1, mod).bind(2, r.id).exec();
			return r.id;
		});
		if (reclaim) {
			Reclaimed(PlacementEntity::Account, name);
		} else {
			m_counts.updated++;
		}
		r.source = PLACEMENT_SOURCE_MAPFILE;
		r.authz = want_authz;
		r.projects = want_projects;
		r.default_project = default_id;
	}
}

// Whether the expiry moved earlier: never to a time, or one time to an
// earlier one.
bool
expiry_tightened(const std::optional<long long> &before, const std::optional<long long> &after)
{
	return after && (!before || *after < *before);
}

void
Apply::ApplyIdentities()
{
	for (const auto &entry: m_want_identities) {
		const std::string &identity = entry.first;
		const DesiredIdentity &want = entry.second;
		auto acct_it = m_accounts.find(want.account);
		if (acct_it == m_accounts.end()) {
			throw PlacementError(PLACEMENT_ERR_DATABASE, "account " + want.account + " vanished during reconcile");
		}
		long long account_id = acct_it->second.id;

		auto it = m_identities.find(identity);
		if (it == m_identities.end()) {
			std::string uuid = PlacementNewUuid();
			m_audit.Modify(PlacementEntity::Identity, 0, uuid, "create", [&](long long mod) {
				m_stmts.Get("INSERT INTO placementd_identities (uuid, identity, account_id, expires_at,"
					" source, modification_id) VALUES (?1, ?2, ?3, ?4, 'mapfile', ?5)")
					.bind(1, uuid).bind(2, identity).bind(3, account_id).bind(4, want.expires).bind(5, mod).exec();
				return (long long)sqlite3_last_insert_rowid(m_stmts.handle());
			});
			m_counts.inserted++;
			continue;
		}
		IdentityRow &r = it->second;
		bool reclaim = !r.IsMapfile();
		if (!reclaim && r.account_id == account_id && r.expires == want.expires) {
			continue;
		}
		m_audit.Modify(PlacementEntity::Identity, r.id, r.uuid, reclaim ? "reclaim" : "update", [&](long long mod) {
			m_stmts.Get("UPDATE placementd_identities SET account_id = ?1, expires_at = ?2,"
				" source = 'mapfile', modification_id = ?3 WHERE id = ?4")
				.bind(1, account_id).bind(2, want.expires).bind(3, mod).bind(4, r.id).exec();
			// Tokens were minted for the old account, and live no longer
			// than the old expiry.
			if (r.account_id != account_id) {
				m_audit.RevokeTokens(PlacementTokenBasis::Identity, r.uuid);
			} else if (expiry_tightened(r.expires, want.expires)) {
				m_audit.RevokeTokens(PlacementTokenBasis::Identity, r.uuid, want.expires);
			}
			return r.id;
		});
		if (reclaim) {
			Reclaimed(PlacementEntity::Identity, identity);
		} else {
			m_counts.updated++;
		}
		r.source = PLACEMENT_SOURCE_MAPFILE;
		r.account_id = account_id;
		r.expires = want.expires;
	}
}

void
Apply::TightenQuarantined()
{
	// A quarantined identity keeps its row, but never keeps access past an
	// expiry the file moved earlier.
	for (const auto &entry: m_q_identities) {
		auto it = m_identities.find(entry.first);
		if (it == m_identities.end() || !it->second.IsMapfile() || !entry.second) {
			continue;
		}
		IdentityRow &r = it->second;
		std::optional<long long> tighter = earliest(r.expires, entry.second);
		if (tighter == r.expires) {
			continue;
		}
		m_audit.Modify(PlacementEntity::Identity, r.id, r.uuid, "update", [&](long long mod) {
			m_stmts.Get("UPDATE placementd_identities SET expires_at = ?1, modification_id = ?2 WHERE id = ?3")
				.bind(1, tighter).bind(2, mod).bind(3, r.id).exec();
			m_audit.RevokeTokens(PlacementTokenBasis::Identity, r.uuid, tighter);
			return r.id;
		});
		r.expires = tighter;
		m_counts.updated++;
	}
}

void
Apply::DeleteIdentities()
{
	// An identity whose line is gone is deleted, whatever else is wrong
	// with the file.
	for (const auto &entry: m_identities) {
		const IdentityRow &row = entry.second;
		if (!row.IsMapfile() || m_want_identities.count(entry.first) || m_q_identities.count(entry.first)) {
			continue;
		}
		Delete(PlacementEntity::Identity, row);
	}
}

void
Apply::DeleteAccounts()
{
	for (const auto &entry: m_accounts) {
		const AccountRow &row = entry.second;
		if (!row.IsMapfile() || m_want_accounts.count(entry.first)) {
			continue;
		}
		Retire(PlacementEntity::Account, row, "identity");
	}
}

void
Apply::DeleteNamed(PlacementEntity t, const CaselessSet &keep)
{
	std::vector<const Row *> rows;
	if (t == PlacementEntity::Project) {
		for (const auto &entry: m_projects) { rows.push_back(&entry.second); }
	} else {
		for (const auto &entry: m_authz) { rows.push_back(&entry.second); }
	}
	for (const Row *row: rows) {
		if (!row->IsMapfile() || keep.count(row->name)) {
			continue;
		}
		Retire(t, *row, "account");
	}
}

void
Apply::Run()
{
	LoadState();
	if (m_parsed.authz_file_problem) {
		// The authorizations file is optional: a missing one is empty while
		// nothing has been imported from it, and otherwise missing is not
		// empty.
		bool imported = std::any_of(m_authz.begin(), m_authz.end(), [](const auto &entry) {
			return entry.second.IsMapfile() && entry.second.requestable;
		});
		const PlacementProblem &problem = *m_parsed.authz_file_problem;
		if (m_parsed.authz_file_missing && !imported) {
			dprintf(D_FULLDEBUG, "%s; treating it as empty\n", problem.message.c_str());
			m_parsed.authz_file_problem.reset();
		} else {
			dprintf(m_parsed.authz_file_missing ? D_ALWAYS : D_ERROR, "%s; keeping the authorizations' "
				"display metadata and applying the user map file\n", problem.message.c_str());
			m_parsed.problems.push_back(problem);
		}
	}
	ComputeIdentities();
	ComputeAccounts();
	ComputeAuthorizations();

	ApplyAuthorizations();
	ApplyProjects();
	ApplyAccounts();
	ApplyIdentities();
	TightenQuarantined();

	DeleteIdentities();
	DeleteAccounts();
	CaselessSet keep_projects;
	for (const auto &entry: m_want_projects) { keep_projects.insert(entry.first); }
	DeleteNamed(PlacementEntity::Project, keep_projects);
	CaselessSet keep_authz = m_parsed.q_authz;
	keep_authz.insert(m_keep_authz.begin(), m_keep_authz.end());
	for (const auto &entry: m_want_authz) { keep_authz.insert(entry.first); }
	DeleteNamed(PlacementEntity::Authorization, keep_authz);

	m_counts.quarantined = (int)(m_q_identities.size() + m_parsed.q_authz.size() + m_parsed.rejected.size());
}

} // namespace

// ------------------------------------------------------------------ status

void
PlacementMapfileStatus::Publish(ClassAd &ad) const
{
	ad.Assign("PlacementdApiVersion", PLACEMENTD_API_VERSION);
	if (load_time) {
		ad.Assign("PlacementdMapfileLoadTime", (long long)load_time);
	} else {
		ad.Delete("PlacementdMapfileLoadTime");
	}
	if (attempt_time) {
		ad.Assign("PlacementdMapfileLoadAttemptTime", (long long)attempt_time);
	} else {
		ad.Delete("PlacementdMapfileLoadAttemptTime");
	}
	ad.Assign("PlacementdMapfileOK", ok);
	ad.Assign("PlacementdMapfileQuarantined", quarantined);
	ad.Assign("PlacementdMapfileConflicts", conflicts);
}

void
PlacementMapfileStatus::PublishProblems(ClassAd &ad) const
{
	Publish(ad);
	std::vector<classad::ExprTree *> recs;
	for (const auto &p: problems) {
		auto *rec = new classad::ClassAd();
		rec->InsertAttr("Kind", p.kind);
		rec->InsertAttr("File", p.file);
		rec->InsertAttr("Line", p.line);
		rec->InsertAttr("Key", p.key);
		rec->InsertAttr("Message", p.message);
		recs.push_back(rec);
	}
	ad.Insert("Problems", classad::ExprList::MakeExprList(recs));
}

void
PlacementMapfileStatus::Save(PlacementDb &db) const
{
	db.SetMeta("mapfile_load_time", std::to_string((long long)load_time));
	db.SetMeta("mapfile_load_attempt_time", std::to_string((long long)attempt_time));
	db.SetMeta("mapfile_ok", ok ? "1" : "0");
	db.SetMeta("mapfile_quarantined", std::to_string(quarantined));
	db.SetMeta("mapfile_conflicts", std::to_string(conflicts));
}

void
PlacementMapfileStatus::Load(PlacementDb &db)
{
	std::string val;
	if (db.GetMeta("mapfile_load_time", val)) { load_time = (time_t)atoll(val.c_str()); }
	if (db.GetMeta("mapfile_load_attempt_time", val)) { attempt_time = (time_t)atoll(val.c_str()); }
	if (db.GetMeta("mapfile_ok", val)) { ok = val == "1"; }
	if (db.GetMeta("mapfile_quarantined", val)) { quarantined = atoi(val.c_str()); }
	if (db.GetMeta("mapfile_conflicts", val)) { conflicts = atoi(val.c_str()); }
}

// ------------------------------------------------------------------ run

bool
PlacementReconciler::Run(const std::string &user_mapfile, const std::string &authz_mapfile,
	const std::string &uid_domain)
{
	time_t now = time(nullptr);
	m_status.attempt_time = now;
	PlacementLockRelease release(m_db.Statements());

	if (user_mapfile.empty()) {
		// Disabled: mapfile rows stay, read-only, until adopted.
		dprintf(D_FULLDEBUG, "PLACEMENTD_MAPFILE is empty; mapfile reconcile disabled\n");
		m_keys = PlacementMapfileKeys();
		m_last_signature.clear();
		m_status.ok = true;
		m_status.quarantined = 0;
		m_status.conflicts = 0;
		m_status.problems.clear();
		return false;
	}

	// Stat-skip: nothing to do if neither file changed since the last
	// successful run.
	// The reserved subjects decide which lines are malformed, so a change to
	// them (on reconfig) is a change too.
	std::string signature = "uid_domain=" + uid_domain + ";reserved=" + PlacementReservedSignature() + ";";
	std::string sig;
	time_t user_mtime = 0, authz_mtime = 0;
	bool user_stat_ok = stat_signature(user_mapfile, sig, user_mtime);
	signature += user_stat_ok ? sig : "user:missing";
	signature += ";";
	if (!authz_mapfile.empty() && stat_signature(authz_mapfile, sig, authz_mtime)) {
		signature += sig;
	} else {
		signature += "authz:none:" + authz_mapfile;
	}
	if (user_stat_ok && signature == m_last_signature) {
		dprintf(D_FULLDEBUG, "Placementd map files unchanged; skipping reconcile\n");
		return false;
	}
	// A file written within the last moments may change again without its
	// mtime moving; do not trust its signature to skip the next run.
	bool settled = std::max(user_mtime, authz_mtime) + MAPFILE_SETTLE_SECONDS < now;

	Parsed parsed;
	std::string errmsg;
	auto abort_run = [&](const std::string &file, const std::string &msg, bool missing) {
		if (missing) {
			dprintf(D_ALWAYS, "Placementd map file %s is missing; keeping the existing rows. "
				"To revoke every mapping, empty the file instead.\n", file.c_str());
		} else {
			dprintf(D_ERROR, "Placementd mapfile reconcile aborted, no rows changed: %s\n", msg.c_str());
		}
		m_status.ok = false;
		m_status.problems.clear();
		m_status.problems.push_back(PlacementProblem{missing ? "Missing" : "Unreadable", file, 0, "", msg});
		m_last_signature.clear();
		try {
			PlacementTxn txn(m_db.Statements());
			m_status.Save(m_db);
			txn.commit();
		} catch (const PlacementError &e) {
			dprintf(D_ERROR, "Failed to save placementd mapfile status: %s\n", e.what());
		}
		return true;
	};

	bool missing = false;
	if (!parse_user_mapfile(user_mapfile, uid_domain, parsed, errmsg, missing)) {
		return abort_run(user_mapfile, errmsg, missing);
	}
	if (!authz_mapfile.empty()) {
		Parsed authz_only;
		if (!parse_authz_mapfile(authz_mapfile, authz_only, errmsg, missing)) {
			// Only authorization display metadata depends on this file, so
			// the user file is still applied (its revocations above all).
			// Whether the problem is reported is decided in the transaction.
			parsed.authz_file_problem = PlacementProblem{missing ? "Missing" : "Unreadable", authz_mapfile, 0, "",
				errmsg};
			parsed.authz_file_missing = missing;
		} else {
			parsed.authz = std::move(authz_only.authz);
			parsed.q_authz = std::move(authz_only.q_authz);
			parsed.rejected.insert(authz_only.rejected.begin(), authz_only.rejected.end());
			parsed.problems.insert(parsed.problems.end(), authz_only.problems.begin(), authz_only.problems.end());
			parsed.keys.authorizations = std::move(authz_only.keys.authorizations);
		}
	}

	Apply apply(m_db.Statements(), parsed, user_mapfile);
	PlacementMapfileStatus next = m_status;
	try {
		PlacementTxn txn(m_db.Statements());
		apply.Run();
		const ApplyCounts &counts = apply.Counts();
		next.load_time = now;
		next.quarantined = counts.quarantined;
		next.conflicts = counts.reclaimed;
		next.problems = parsed.problems;
		next.ok = std::none_of(next.problems.begin(), next.problems.end(),
			[](const PlacementProblem &p) { return p.kind != "Reclaimed"; });
		if (next.problems.size() > PLACEMENTD_MAX_PROBLEMS) {
			next.problems.resize(PLACEMENTD_MAX_PROBLEMS);
		}
		next.Save(m_db);
		txn.commit();
	} catch (const PlacementError &e) {
		dprintf(D_ERROR, "Placementd mapfile reconcile failed and was rolled back: %s\n", e.what());
		m_status.ok = false;
		m_status.problems.clear();
		m_status.problems.push_back(PlacementProblem{"DatabaseError", user_mapfile, 0, "", e.what()});
		m_last_signature.clear();
		return true;
	}
	m_status = next;

	parsed.keys.valid = true;
	m_keys = std::move(parsed.keys);
	// An unreadable authorizations file may become readable without its
	// signature changing (chmod), so it does not settle the run.
	m_last_signature = (settled && !parsed.authz_file_problem) ? signature : "";

	const ApplyCounts &counts = apply.Counts();
	dprintf(D_AUDIT, "Placementd mapfile reconcile of %s: %d inserted, %d updated, %d deleted, "
		"%d reclaimed, %d retained, %d quarantined, %lld tokens revoked (change set %lld)\n", user_mapfile.c_str(),
		counts.inserted, counts.updated, counts.deleted, counts.reclaimed, counts.retained,
		counts.quarantined, apply.RevokedTokens(), apply.ChangesetId());
	return true;
}
