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

class PlacementDaemon : public Service
{
public:
	~PlacementDaemon();

	void Init();
	void Config();

	bool LogTokenCreation(const std::string& token, const std::string& foreign_id);

	bool ReadMapFile();

	void initialize_classad();
	void update_collector(int);
	void invalidate_ad();

	int command_user_login(int, Stream* stream);
	int command_query_users(int, Stream* stream);
	int command_query_tokens(int, Stream* stream);

	char* m_name{nullptr};
	ClassAd m_daemon_ad;
	int m_update_collector_tid{-1};
	int m_update_collector_interval{300};

	std::map<std::string, std::string> m_users;
	std::string m_databaseFile;
	sqlite3* m_db{nullptr};

	std::string m_mapFile;
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

	if (!param(uid_domain, "PLACEMENT_UID_DOMAIN")) {
		param(uid_domain, "UID_DOMAIN");
	}

	daemonCore->Register_CommandWithPayload(USER_LOGIN, "USER_LOGIN",
		(CommandHandlercpp)&PlacementDaemon::command_user_login,
		"command_user_login", this, ADMINISTRATOR, true /*force authentication*/);
	daemonCore->Register_CommandWithPayload(QUERY_USERS, "QUERY_USERS",
		(CommandHandlercpp)&PlacementDaemon::command_query_users,
		"command_query_users", this, ADMINISTRATOR, true /*force authentication*/);
	daemonCore->Register_CommandWithPayload(QUERY_TOKENS, "QUERY_TOKENS",
		(CommandHandlercpp)&PlacementDaemon::command_query_tokens,
		"command_query_tokens", this, ADMINISTRATOR, true /*force authentication*/);

	// set timer to periodically advertise ourself to the collector
	m_update_collector_tid = daemonCore->Register_Timer(0, m_update_collector_interval,
				(TimerHandlercpp)&PlacementDaemon::update_collector, "update_collector", this);

	Config();

	char *db_err_msg;
	int rc = sqlite3_open_v2(m_databaseFile.c_str(), &m_db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
	if (rc != SQLITE_OK) {
		EXCEPT("Failed to open database file %s: %s\n", m_databaseFile.c_str(), sqlite3_errmsg(m_db));
	}
	rc = sqlite3_exec(m_db, "CREATE TABLE IF NOT EXISTS placementd_tokens (token_iss TEXT, token_kid TEXT, token_jti TEXT, token_iat INTEGER, token_exp INTEGER, token_sub TEXT, token_scope TEXT, foreign_id TEXT)", nullptr, nullptr, &db_err_msg);
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

	if (param(m_mapFile, "PLACEMENTD_MAPFILE")) {
		ReadMapFile();
	} else {
		m_mapFile.clear();
	}
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

bool PlacementDaemon::LogTokenCreation(const std::string& token, const std::string& foreign_id)
{
	int rc = 0;
	char *db_err_msg = nullptr;
	std::string stmt_str;

	std::string token_iss;
	std::string token_kid;
	std::string token_jti;
	long long token_iat = 0;
	long long token_exp = 0;
	std::string token_sub;
	std::string token_scope;

	auto jwt = jwt::decode(token);

		// gather token data
	if (jwt.has_issuer()) {
		token_iss = jwt.get_issuer();
	}

	if (jwt.has_key_id()) {
		token_kid = jwt.get_key_id();
	}

	if (jwt.has_id()) {
		token_jti = jwt.get_id();
	}

	if (jwt.has_issued_at()) {
		auto datestamp = jwt.get_issued_at();
		token_iat = std::chrono::duration_cast<std::chrono::seconds>(datestamp.time_since_epoch()).count();
	}

	if (jwt.has_expires_at()) {
		auto datestamp = jwt.get_expires_at();
		token_exp = std::chrono::duration_cast<std::chrono::seconds>(datestamp.time_since_epoch()).count();
	}

	if (jwt.has_subject()) {
		token_sub = jwt.get_subject();
	}

	if (jwt.has_payload_claim("scope")) {
		token_scope = jwt.get_payload_claim("scope").as_string();
	}

	dprintf(D_ALWAYS, "JEF inserting iss='%s' kid='%s' jti='%s' iat=%lld exp=%lld sub='%s' scope='%s' foreign_id='%s'\n",token_iss.c_str(),token_kid.c_str(), token_jti.c_str(), (long long)token_iat, (long long)token_exp, token_sub.c_str(), token_scope.c_str(), foreign_id.c_str());
	formatstr(stmt_str, "INSERT INTO placementd_tokens (token_iss, token_kid, token_jti, token_iat, token_exp, token_sub, token_scope, foreign_id) VALUES ('%s', '%s', '%s', %lld, %lld, '%s', '%s', '%s');", token_iss.c_str(), token_kid.c_str(), token_jti.c_str(), token_iat, token_exp, token_sub.c_str(), token_scope.c_str(), foreign_id.c_str());
	rc = sqlite3_exec(m_db, stmt_str.c_str(), nullptr, nullptr, &db_err_msg);
	if (rc != SQLITE_OK) {
		dprintf(D_ERROR, "Adding db entry failed: %s\n", db_err_msg);
		goto done;
	}

 done:
	free(db_err_msg);
	return rc == SQLITE_OK;
}

bool PlacementDaemon::ReadMapFile()
{
	std::string uid_domain;
	if (!param(uid_domain, "PLACEMENT_UID_DOMAIN")) {
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
	while (readLine(line, map_fp)) {
		trim(line);
		if (line.empty() || line[0] == '#') {
			continue;
		}
		items = split(line, " \t\n");
		if (items.empty()) {
			dprintf(D_ERROR, "Ignoring malformed map line: %s\n", line.c_str());
			continue;
		}
		if (items.size() == 1) {
			auto at = items[0].find('@');
			items.emplace_back(items[0].substr(0, at));
		}
		if (items[1].find('@') == std::string::npos) {
			items[1] += "@" + uid_domain;
		}

		m_users[items[0]] = items[1];
	}

	fclose(map_fp);
	return true;
}

int PlacementDaemon::command_user_login(int cmd, Stream* stream)
{
	const char * cmd_name = getCommandStringSafe(cmd);
	ClassAd cmd_ad;

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

	CondorError err;
	std::vector<std::string> bounding_set = { "WRITE", "READ" };
	int lifetime = -1; // No expiration
	time_t new_expire = 0; 
	std::string key_name;
	std::string token;
	std::string token_identity;
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

	user_it = m_users.find(user_name);
	if (user_it == m_users.end()) {
		dprintf(D_FULLDEBUG, "User %s is not authorized\n", user_name.c_str());
		result_ad.Assign(ATTR_ERROR_STRING, "User not authorized");
		result_ad.Assign(ATTR_ERROR_CODE, 3);
		goto send_reply;
	}

	token_identity = user_it->second;

	// TODO interact with schedd

	dprintf(D_FULLDEBUG,"JEF Creating IDToken\n");
	// Create an IDToken for this user
	key_name = htcondor::get_token_signing_key(err);
	if (key_name.empty()) {
		result_ad.Assign(ATTR_ERROR_STRING, err.getFullText());
		result_ad.Assign(ATTR_ERROR_CODE, err.code());
		goto send_reply;
	}
	lifetime = param_integer("PLACEMENTD_TOKEN_DURATION", 24*60*60);
	if (!Condor_Auth_Passwd::generate_token(token_identity, key_name, bounding_set, lifetime, token, rsock->getUniqueId(), &err)) {
		result_ad.Assign(ATTR_ERROR_STRING, err.getFullText());
		result_ad.Assign(ATTR_ERROR_CODE, err.code());
		goto send_reply;
	}

	LogTokenCreation(token, user_name);

	dprintf(D_AUDIT, *rsock, "User Login token issued for UserName '%s', AP user account %s\n", user_name.c_str(), token_identity.c_str());

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

	ClassAd summary_ad;
	summary_ad.Assign("MyType", "Summary");

	struct reply_rec {
		std::string sub;
		time_t exp;
	};

	std::map<std::string, reply_rec> reply_users;
	for (const auto& [username, ap_id]: m_users) {
		reply_users[username] = {ap_id, 0};
	}

	std::string stmt_str;
	formatstr(stmt_str, "SELECT foreign_id, token_exp, token_sub FROM placementd_tokens WHERE token_exp >= %lld;", (long long)time(nullptr));
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
			reply_users[rec_user].exp = rec_exp;
			reply_users[rec_user].sub = rec_sub;
		} else if (rec_exp > reply_it->second.exp) {
			reply_it->second.exp = rec_exp;
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
		user_ad.Assign("TokenExpiration", rec.exp);
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

	ClassAd summary_ad;
	summary_ad.Assign("MyType", "Summary");

	ClassAd token_ad;

	sqlite3_stmt* stmt = nullptr;
	int rc = sqlite3_prepare_v2(m_db, "SELECT token_iss, token_kid, token_jti, token_iat, token_exp, token_sub, token_scope, foreign_id FROM placementd_tokens;", -1, &stmt, nullptr);
	if (rc != SQLITE_OK) {
		dprintf(D_ERROR, "sqlite3_prepare failed: %s\n", sqlite3_errmsg(m_db));
		sqlite3_finalize(stmt);
		return false;
	}

	while ( (rc = sqlite3_step(stmt)) == SQLITE_ROW ) {
		token_ad.Clear();
		token_ad.Assign("Iss", (const char*)sqlite3_column_text(stmt, 0));
		token_ad.Assign("Kid", (const char*)sqlite3_column_text(stmt, 1));
		token_ad.Assign("Jti", (const char*)sqlite3_column_text(stmt, 2));
		token_ad.Assign("Iat", sqlite3_column_int(stmt, 3));
		token_ad.Assign("Exp", sqlite3_column_int(stmt, 4));
		token_ad.Assign("Sub", (const char*)sqlite3_column_text(stmt, 5));
		token_ad.Assign("Scope", (const char*)sqlite3_column_text(stmt, 6));
		token_ad.Assign("UserName", (const char*)sqlite3_column_text(stmt, 7));
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
