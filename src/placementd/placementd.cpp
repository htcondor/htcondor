/***************************************************************
 *
 * Copyright (C) 2024, Center for High Throughput Computing
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
#include "condor_daemon_core.h"
#include "subsystem_info.h"
#include "get_daemon_name.h"
#include "token_utils.h"
#include "condor_auth_passwd.h"
#include <sqlite3.h>

GCC_DIAG_OFF(float-equal)
GCC_DIAG_OFF(cast-qual)
#include "jwt-cpp/jwt.h"
GCC_DIAG_ON(float-equal)
GCC_DIAG_ON(cast-qual)

struct UserMapEntry
{
	std::string ap_id;
	std::string authz;
	time_t exp{0};
};

// The key of the m_authz map is the auth-level name as used in the token
struct AuthzMapEntry
{
	std::string label;
	std::string color;
	std::string description;
};

class PlacementDaemon : public Service
{
public:
	~PlacementDaemon();

	void Init();
	void Config();

	bool ReadUserMapFile();
	bool ReadAuthzMapFile();

	void initialize_classad();
	void update_collector(int);
	void invalidate_ad();

	void timer_read_mapfiles(int);

	int command_user_login(int, Stream* stream);
	int command_query_users(int, Stream* stream);
	int command_query_tokens(int, Stream* stream);
	int command_query_authorizations(int, Stream* stream);

	char* m_name{nullptr};
	ClassAd m_daemon_ad;
	int m_update_collector_tid{-1};
	int m_update_collector_interval{300};
	int m_read_mapfiles_tid{-1};

	std::map<std::string, UserMapEntry> m_users;
	std::map<std::string, AuthzMapEntry> m_authz;
	std::string m_databaseFile;
	sqlite3* m_db{nullptr};

	std::string m_mapFile;
	std::string m_authzMapFile;
};

PlacementDaemon* placementd = nullptr;

PlacementDaemon::~PlacementDaemon()
{
	// tell our collector we're going away
	invalidate_ad();

	sqlite3_close(m_db);
	free(m_name);
}

void
PlacementDaemon::Init()
{
	std::string uid_domain;

	if (!param(uid_domain, "PLACEMENTD_UID_DOMAIN")) {
		param(uid_domain, "UID_DOMAIN");
	}

	daemonCore->Register_CommandWithPayload(PLACEMENT_USER_LOGIN, "PLACEMENT_USER_LOGIN",
		(CommandHandlercpp)&PlacementDaemon::command_user_login,
		"command_user_login", this, ADMINISTRATOR, true /*force authentication*/);
	daemonCore->Register_CommandWithPayload(PLACEMENT_QUERY_USERS, "PLACEMENT_QUERY_USERS",
		(CommandHandlercpp)&PlacementDaemon::command_query_users,
		"command_query_users", this, ADMINISTRATOR, true /*force authentication*/);
	daemonCore->Register_CommandWithPayload(PLACEMENT_QUERY_TOKENS, "PLACEMENT_QUERY_TOKENS",
		(CommandHandlercpp)&PlacementDaemon::command_query_tokens,
		"command_query_tokens", this, ADMINISTRATOR, true /*force authentication*/);
	daemonCore->Register_CommandWithPayload(PLACEMENT_QUERY_AUTHORIZATIONS, "PLACEMENT_QUERY_AUTHORIZATIONS",
		(CommandHandlercpp)&PlacementDaemon::command_query_authorizations,
		"command_query_authorizations", this, ADMINISTRATOR, true /*force authentication*/);

	// set timer to periodically advertise ourself to the collector
	m_update_collector_tid = daemonCore->Register_Timer(0, m_update_collector_interval,
				(TimerHandlercpp)&PlacementDaemon::update_collector, "update_collector", this);

	m_read_mapfiles_tid = daemonCore->Register_Timer(0, 300,
			(TimerHandlercpp)&PlacementDaemon::timer_read_mapfiles,
			"timer_read_mapfiles", this);

	Config();

	char *db_err_msg;
	int rc = sqlite3_open_v2(m_databaseFile.c_str(), &m_db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
	if (rc != SQLITE_OK) {
		EXCEPT("Failed to open database file %s: %s\n", m_databaseFile.c_str(), sqlite3_errmsg(m_db));
	}
	rc = sqlite3_exec(m_db, "CREATE TABLE IF NOT EXISTS placementd_tokens (foreign_id TEXT, ap_id TEXT, authz TEXT, token_jti TEXT, token_exp INTEGER)", nullptr, nullptr, &db_err_msg);
	if (rc != SQLITE_OK) {
		EXCEPT("Failed to create db table: %s\n", db_err_msg);
	}
}

void
PlacementDaemon::Config()
{
	if (m_name != NULL) {
		free(m_name);
	}
	m_name = param("PLACEMENTD_HOST");
	if (m_name == NULL) {
		m_name = default_daemon_name();
		ASSERT(m_name != NULL);
	}
	if(m_name == NULL) {
		EXCEPT("default_daemon_name() returned NULL");
	}

	// how often do we update the collector?
	m_update_collector_interval = param_integer ("PLACEMENTD_UPDATE_INTERVAL",
												 5 * MINUTE);

	// fill in our daemon classad
	m_daemon_ad.Clear();
	m_daemon_ad.Assign(ATTR_MY_TYPE, "PlacementD");
	m_daemon_ad.Assign(ATTR_NAME, m_name);

	// Publish all DaemonCore-specific attributes, which also handles
	// SUBSYS_ATTRS for us.
	daemonCore->publish(&m_daemon_ad);

	// reset the timer (if it exists) to update the collector
	if (m_update_collector_tid != -1) {
		daemonCore->Reset_Timer(m_update_collector_tid, 0, m_update_collector_interval);
	}

	if (!param(m_databaseFile, "TOKENS_DATABASE")) {
		EXCEPT("No TOKENS_DATABASE specified!");
	}

	if (!param(m_mapFile, "PLACEMENTD_MAPFILE")) {
		m_mapFile.clear();
	}

	if (!param(m_authzMapFile, "PLACEMENTD_AUTHORIZATIONS_MAPFILE")) {
		m_authzMapFile.clear();
	}

	daemonCore->Reset_Timer(m_read_mapfiles_tid, 0, 300);
}

void
PlacementDaemon::initialize_classad()
{
}

void
PlacementDaemon::update_collector( int /* timerID */ )
{
	daemonCore->sendUpdates(UPDATE_AD_GENERIC, &m_daemon_ad, NULL, true);
}

void
PlacementDaemon::invalidate_ad()
{
	ClassAd query_ad;
	SetMyTypeName(query_ad, QUERY_ADTYPE);
	query_ad.Assign(ATTR_TARGET_TYPE, CREDD_ADTYPE);

	std::string line;
	formatstr(line, "TARGET.%s == \"%s\"", ATTR_NAME, m_name);
	query_ad.AssignExpr(ATTR_REQUIREMENTS, line.c_str());
	query_ad.Assign(ATTR_NAME,m_name);

	daemonCore->sendUpdates(INVALIDATE_ADS_GENERIC, &query_ad, NULL, true);
}

void
main_init(int, char*[])
{
	placementd->Init();
}

void
main_config()
{
	placementd->Config();
}

void
main_shutdown_fast()
{
	delete placementd;
	DC_Exit(0);
}

void
main_shutdown_graceful()
{
	delete placementd;
	DC_Exit(0);
}

int
main(int argc, char **argv)
{
	placementd = new PlacementDaemon;

	set_mySubSystem( "PLACEMENTD", true, SUBSYSTEM_TYPE_DAEMON );// used by Daemon Core

	dc_main_init = main_init;
	dc_main_config = main_config;
	dc_main_shutdown_fast = main_shutdown_fast;
	dc_main_shutdown_graceful = main_shutdown_graceful;
	return dc_main( argc, argv );
}

bool PlacementDaemon::ReadUserMapFile()
{
	std::string uid_domain;
	if (!param(uid_domain, "PLACEMENTD_UID_DOMAIN")) {
		param(uid_domain, "UID_DOMAIN");
	}

	FILE* map_fp = safe_fopen_wrapper(m_mapFile.c_str(), "r");
	if (!map_fp) {
		dprintf(D_ERROR, "Failed to open map file %s: %s\n", m_mapFile.c_str(), strerror(errno));
		return false;
	}

	m_users.clear();

	std::string line;
	std::vector<std::string> items;
	time_t exp = 0;
	while (readLine(line, map_fp)) {
		trim(line);
		if (line.empty() || line[0] == '#') {
			continue;
		}
		items = split(line, " \t\n");
		if (items.size() < 3 || items.size() > 4) {
			dprintf(D_ERROR, "Ignoring malformed map line: %s\n", line.c_str());
			continue;
		}
		if (items[1] == "*") {
			auto at = items[0].find('@');
			items[1] = items[0].substr(0, at);
		}
		if (items[1].find('@') == std::string::npos) {
			items[1] += "@" + uid_domain;
		}
		exp = 0;
		if (items.size() == 4) {
			exp = std::stoll(items[3]);
		}

		m_users[items[0]] = UserMapEntry{items[1], items[2], exp};
	}

	fclose(map_fp);
	return true;
}

bool PlacementDaemon::ReadAuthzMapFile()
{
	FILE* map_fp = safe_fopen_wrapper(m_authzMapFile.c_str(), "r");
	if (!map_fp) {
		dprintf(D_ERROR, "Failed to open authz map file %s: %s\n", m_authzMapFile.c_str(), strerror(errno));
		return false;
	}

	m_authz.clear();

	std::string line;
	std::vector<std::string> items;
	while (readLine(line, map_fp)) {
		trim(line);
		if (line.empty() || line[0] == '#') {
			continue;
		}
		items = split(line, "|");
		if (items.size() != 4) {
			dprintf(D_ERROR, "Ignoring malformed authz map line: %s\n", line.c_str());
			continue;
		}

		m_authz[items[0]] = AuthzMapEntry{items[1], items[2], items[3]};
	}

	fclose(map_fp);
	return true;
}

void PlacementDaemon::timer_read_mapfiles(int)
{
	if (!m_mapFile.empty()) {
		ReadUserMapFile();
	}

	if (!m_authzMapFile.empty()) {
		ReadAuthzMapFile();
	}
}

int PlacementDaemon::command_user_login(int cmd, Stream* stream)
{
	const char * cmd_name = getCommandStringSafe(cmd);
	ClassAd cmd_ad;
	int rc = 0;
	char *db_err_msg = nullptr;
	std::string stmt_str;

	dprintf( D_FULLDEBUG, "In command_user_login\n" );

	stream->decode();
	stream->timeout(15);

	if( !getClassAd(stream, cmd_ad)) {
		dprintf( D_ERROR, "Failed to receive user ad for %s command: aborting\n", cmd_name);
		return FALSE;
	}

	if (!stream->end_of_message()) {
		dprintf( D_ERROR, "Failed to receive EOM: for %s command: aborting\n", cmd_name );
		return FALSE;
	}
	// done reading input command stream
	stream->encode();

	ClassAd result_ad;
	ReliSock* rsock = (ReliSock*)stream;
	std::string user_name;
	std::string authz_list; // authz requested for this token
	std::vector<std::string> full_authz_list; // all authz from the mapfile
	std::string bad_authz;

	CondorError err;
	std::vector<std::string> bounding_set;
	int lifetime = -1; // No expiration
	std::string key_name;
	std::string token;
	std::string token_identity;
	std::string token_jti;
	long long token_exp;
	auto user_it = m_users.end();

	if (!rsock->get_encryption()) {
		result_ad.Assign(ATTR_ERROR_STRING, "Request to server was not encrypted.");
		result_ad.Assign(ATTR_ERROR_CODE, 1);
		goto send_reply;
	}

	cmd_ad.LookupString("UserName", user_name);
	if (user_name.empty()) {
		dprintf(D_ERROR, "Missing UserName for %s command: aborting\n", cmd_name);
		result_ad.Assign(ATTR_ERROR_STRING, "Missing UserName");
		result_ad.Assign(ATTR_ERROR_CODE, 2);
		goto send_reply;
	}
	cmd_ad.LookupString("Authorizations", authz_list);

	user_it = m_users.find(user_name);
	if (user_it == m_users.end()) {
		dprintf(D_FULLDEBUG, "User %s is not authorized\n", user_name.c_str());
		result_ad.Assign(ATTR_ERROR_STRING, "User not authorized");
		result_ad.Assign(ATTR_ERROR_CODE, 3);
		goto send_reply;
	}

	token_identity = user_it->second.ap_id;

	full_authz_list = split(user_it->second.authz);
	for (const auto& one_authz: StringTokenIterator(authz_list)) {
		if (contains(full_authz_list, one_authz)) {
			bounding_set.emplace_back(one_authz);
		} else {
			bad_authz = one_authz;
			break;
		}
	}
	if (!bad_authz.empty()) {
		dprintf(D_FULLDEBUG, "User %s don't have authorization type %s\n", user_name.c_str(), bad_authz.c_str());
		result_ad.Assign(ATTR_ERROR_STRING, "User don't have requested authorizations");
		result_ad.Assign(ATTR_ERROR_CODE, 4);
		goto send_reply;
	}
	if (bounding_set.empty()) {
		dprintf(D_FULLDEBUG, "Client didn't request any authorizations\n");
		result_ad.Assign(ATTR_ERROR_STRING, "Missing Authorizations");
		result_ad.Assign(ATTR_ERROR_CODE, 5);
		goto send_reply;
	}
	authz_list = join(bounding_set, ",");

	lifetime = param_integer("PLACEMENTD_TOKEN_DURATION", 24*60*60);
	if (user_it->second.exp > 0) {
		if (user_it->second.exp <= time(nullptr)) {
			dprintf(D_FULLDEBUG, "User mapping is expired\n");
			result_ad.Assign(ATTR_ERROR_STRING, "User mapping is expired");
			result_ad.Assign(ATTR_ERROR_CODE, 6);
			goto send_reply;
		}
		if (user_it->second.exp - time(nullptr) < lifetime) {
			lifetime = user_it->second.exp - time(nullptr);
		}
	}

	dprintf(D_FULLDEBUG,"JEF Creating IDToken\n");
	// Create an IDToken for this user
	key_name = htcondor::get_token_signing_key(err);
	if (key_name.empty()) {
		result_ad.Assign(ATTR_ERROR_STRING, err.getFullText());
		result_ad.Assign(ATTR_ERROR_CODE, err.code());
		goto send_reply;
	}
	if (!Condor_Auth_Passwd::generate_token(token_identity, key_name, bounding_set, lifetime, true, token, rsock->getUniqueId(), &err)) {
		result_ad.Assign(ATTR_ERROR_STRING, err.getFullText());
		result_ad.Assign(ATTR_ERROR_CODE, err.code());
		goto send_reply;
	}

	{
		auto jwt = jwt::decode(token);
		token_jti = jwt.get_id();
		auto datestamp = jwt.get_expires_at();
		token_exp = std::chrono::duration_cast<std::chrono::seconds>(datestamp.time_since_epoch()).count();
	}

	formatstr(stmt_str, "INSERT INTO placementd_tokens (foreign_id, ap_id, authz, token_jti, token_exp) VALUES ('%s', '%s', '%s', '%s', %lld);", user_name.c_str(), token_identity.c_str(), authz_list.c_str(), token_jti.c_str(), token_exp);
	rc = sqlite3_exec(m_db, stmt_str.c_str(), nullptr, nullptr, &db_err_msg);
	if (rc != SQLITE_OK) {
		dprintf(D_ERROR, "Adding db entry failed: %s\n", db_err_msg);
	}
	free(db_err_msg);

	dprintf(D_AUDIT, *rsock, "User Login token issued for UserName '%s', AP user account '%s', authz '%s'\n", user_name.c_str(), token_identity.c_str(), authz_list.c_str());

	result_ad.Assign(ATTR_SEC_TOKEN, token);

 send_reply:
	// Finally, close up shop.  We have to send the result ad to signal the end.
	if( !putClassAd(stream, result_ad) || !stream->end_of_message() ) {
		dprintf( D_ALWAYS, "Error sending result ad for %s command\n", cmd_name );
		return FALSE;
	}
	return TRUE;
}

int PlacementDaemon::command_query_users(int cmd, Stream* stream)
{
	const char * cmd_name = getCommandStringSafe(cmd);
	ClassAd cmd_ad;
	std::string username;

	dprintf( D_FULLDEBUG, "In command_query_users\n" );

	stream->decode();
	stream->timeout(15);

	if( !getClassAd(stream, cmd_ad)) {
		dprintf( D_ERROR, "Failed to receive user ad for %s command: aborting\n", cmd_name);
		return FALSE;
	}

	if (!stream->end_of_message()) {
		dprintf( D_ERROR, "Failed to receive EOM: for %s command: aborting\n", cmd_name );
		return FALSE;
	}
	// done reading input command stream
	stream->encode();

	cmd_ad.LookupString("Username", username);

	ClassAd summary_ad;
	summary_ad.Assign("MyType", "Summary");

	struct reply_rec {
		std::string sub;
		std::string authz;
		time_t token_exp;
		time_t mapping_exp;
	};

	std::map<std::string, reply_rec> reply_users;
	if (username.empty()) {
		for (const auto& [name, entry]: m_users) {
			reply_users[name] = {entry.ap_id, entry.authz, 0, entry.exp};
		}
	} else if (m_users.find(username) != m_users.end()) {
		reply_users[username] = {m_users[username].ap_id, m_users[username].authz, 0, m_users[username].exp};
	}

	std::string stmt_str;
	if (username.empty()) {
		formatstr(stmt_str, "SELECT foreign_id, token_exp, ap_id FROM placementd_tokens WHERE token_exp >= %lld;", (long long)time(nullptr));
	} else {
		formatstr(stmt_str, "SELECT foreign_id, token_exp, ap_id FROM placementd_tokens WHERE token_exp >= %lld AND foreign_id='%s';", (long long)time(nullptr), username.c_str());
	}
	sqlite3_stmt* stmt = nullptr;
	int rc = sqlite3_prepare_v2(m_db, stmt_str.c_str(), -1, &stmt, nullptr);
	if (rc != SQLITE_OK) {
		dprintf(D_ERROR, "sqlite3_prepare failed: %s\n", sqlite3_errmsg(m_db));
		sqlite3_finalize(stmt);
		return false;
	}

	while ( (rc = sqlite3_step(stmt)) == SQLITE_ROW ) {
		const char* rec_user = (const char*)sqlite3_column_text(stmt, 0);
		time_t rec_exp = sqlite3_column_int(stmt, 1);
		const char* rec_sub = (const char*)sqlite3_column_text(stmt, 2);
		auto reply_it = reply_users.find(rec_user);
		if (reply_it == reply_users.end()) {
			reply_users[rec_user].token_exp = rec_exp;
			reply_users[rec_user].sub = rec_sub;
		} else if (rec_exp > reply_it->second.token_exp) {
			reply_it->second.token_exp = rec_exp;
			reply_it->second.sub = rec_sub;
		}
	}
	if (rc != SQLITE_DONE) {
		dprintf(D_ERROR, "sqlite3_step returned %d\n", rc);
		sqlite3_finalize(stmt);
		return FALSE;
	}

	sqlite3_finalize(stmt);

	ClassAd user_ad;

	for (const auto& [username, rec]: reply_users) {
		user_ad.Clear();
		user_ad.Assign("UserName", username);
		user_ad.Assign("ApUserId", rec.sub);
		user_ad.Assign("TokenExpiration", rec.token_exp);
		user_ad.Assign("MappingExpration", rec.mapping_exp);
		if (!rec.authz.empty()) {
			user_ad.Assign("Authorizations", rec.authz);
		}
		user_ad.Assign("Authorized", m_users.find(username) != m_users.end());
		if (!putClassAd(stream, user_ad)) {
			dprintf(D_ALWAYS, "Error sending user ad for %s command\n", cmd_name);
			return FALSE;
		}
	}

	dprintf(D_FULLDEBUG,"Sending summary ad\n");
	// Finally, close up shop.  We have to send the result ad to signal the end.
	if( !putClassAd(stream, summary_ad) || !stream->end_of_message() ) {
		dprintf( D_ALWAYS, "Error sending result ad for %s command\n", cmd_name );
		return FALSE;
	}
	return TRUE;
}

int PlacementDaemon::command_query_tokens(int cmd, Stream* stream)
{
	const char * cmd_name = getCommandStringSafe(cmd);
	ClassAd cmd_ad;
	std::string jti;
	std::string username;
	bool valid_only = false;

	dprintf( D_FULLDEBUG, "In command_query_tokens\n" );

	stream->decode();
	stream->timeout(15);

	if( !getClassAd(stream, cmd_ad)) {
		dprintf( D_ERROR, "Failed to receive user ad for %s command: aborting\n", cmd_name);
		return FALSE;
	}

	if (!stream->end_of_message()) {
		dprintf( D_ERROR, "Failed to receive EOM: for %s command: aborting\n", cmd_name );
		return FALSE;
	}
	// done reading input command stream
	stream->encode();

	cmd_ad.LookupString("TokenId", jti);
	cmd_ad.LookupString("Username", username);
	cmd_ad.LookupBool("ValidOnly", valid_only);

	ClassAd summary_ad;
	summary_ad.Assign("MyType", "Summary");

	ClassAd token_ad;

	std::string stmt_str;
	std::string where_str;
	if (!jti.empty()) {
		formatstr(where_str, "WHERE token_jti='%s' ", jti.c_str());
	} else if (!username.empty() || valid_only) {
		where_str = "WHERE ";
		if (!username.empty()) {
			formatstr_cat(where_str, "foreign_id='%s' ", username.c_str());
		}
		if (!username.empty() && valid_only) {
			where_str += "AND ";
		}
		if (valid_only) {
			formatstr_cat(where_str, "token_exp >= %lld", (long long)time(nullptr));
		}
	}
	formatstr(stmt_str, "SELECT foreign_id, ap_id, authz, token_jti, token_exp FROM placementd_tokens %s;", where_str.c_str());

	sqlite3_stmt* stmt = nullptr;
	int rc = sqlite3_prepare_v2(m_db, stmt_str.c_str(), -1, &stmt, nullptr);
	if (rc != SQLITE_OK) {
		dprintf(D_ERROR, "sqlite3_prepare failed: %s\n", sqlite3_errmsg(m_db));
		sqlite3_finalize(stmt);
		return false;
	}

	while ( (rc = sqlite3_step(stmt)) == SQLITE_ROW ) {
		token_ad.Clear();
		token_ad.Assign("UserName", (const char*)sqlite3_column_text(stmt, 0));
		token_ad.Assign("ApUserId", (const char*)sqlite3_column_text(stmt, 1));
		token_ad.Assign("Authorizations", (const char*)sqlite3_column_text(stmt, 2));
		token_ad.Assign("TokenId", (const char*)sqlite3_column_text(stmt, 3));
		token_ad.Assign("TokenExpiration", sqlite3_column_int(stmt, 4));
		if (!putClassAd(stream, token_ad)) {
			dprintf(D_ALWAYS, "Error sending token ad for %s command\n", cmd_name);
			sqlite3_finalize(stmt);
			return FALSE;
		}
	}
	if (rc != SQLITE_DONE) {
		dprintf(D_ERROR, "sqlite3_step returned %d\n", rc);
		sqlite3_finalize(stmt);
		return FALSE;
	}

	sqlite3_finalize(stmt);

	dprintf(D_FULLDEBUG,"Sending summary ad\n");
	// Finally, close up shop.  We have to send the result ad to signal the end.
	if( !putClassAd(stream, summary_ad) || !stream->end_of_message() ) {
		dprintf( D_ALWAYS, "Error sending result ad for %s command\n", cmd_name );
		return FALSE;
	}
	return TRUE;
}

int PlacementDaemon::command_query_authorizations(int cmd, Stream* stream)
{
	const char * cmd_name = getCommandStringSafe(cmd);
	ClassAd cmd_ad;
	std::string username;

	dprintf( D_FULLDEBUG, "In command_query_authorizations\n" );

	stream->decode();
	stream->timeout(15);

	if( !getClassAd(stream, cmd_ad)) {
		dprintf( D_ERROR, "Failed to receive user ad for %s command: aborting\n", cmd_name);
		return FALSE;
	}

	if (!stream->end_of_message()) {
		dprintf( D_ERROR, "Failed to receive EOM: for %s command: aborting\n", cmd_name );
		return FALSE;
	}
	// done reading input command stream
	stream->encode();

	cmd_ad.LookupString("Username", username);

	ClassAd summary_ad;
	summary_ad.Assign("MyType", "Summary");

	ClassAd reply_ad;

	std::vector<std::string> user_authz;
	auto user_it = m_users.find(username);
	if (user_it != m_users.end()) {
		user_authz = split(user_it->second.authz);
	} else if (!username.empty()) {
		dprintf(D_FULLDEBUG, "User %s is not authorized\n", username.c_str());
		summary_ad.Assign(ATTR_ERROR_STRING, "User not authorized");
		summary_ad.Assign(ATTR_ERROR_CODE, 3);
		goto send_summary;
	}

	for (const auto& [authz_name, authz_entry]: m_authz) {
		if (user_it != m_users.end() && !contains(user_authz, authz_name)) {
			continue;
		}
		reply_ad.Clear();
		reply_ad.Assign("Name", authz_name);
		reply_ad.Assign("Label", authz_entry.label);
		reply_ad.Assign("Color", authz_entry.color);
		reply_ad.Assign("Description", authz_entry.description);
		if (!putClassAd(stream, reply_ad)) {
			dprintf(D_ALWAYS, "Error sending auth ad for %s command\n", cmd_name);
			return FALSE;
		}
	}

 send_summary:
	dprintf(D_FULLDEBUG,"Sending summary ad\n");
	// Finally, close up shop.  We have to send the result ad to signal the end.
	if( !putClassAd(stream, summary_ad) || !stream->end_of_message() ) {
		dprintf( D_ALWAYS, "Error sending result ad for %s command\n", cmd_name );
		return FALSE;
	}
	return TRUE;
}
