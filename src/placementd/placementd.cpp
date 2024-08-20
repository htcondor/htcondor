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

const char* StmtCreateTable = "CREATE TABLE IF NOT EXISTS mappings (ap_user_id TEXT, user_name TEXT, mapping_time INTEGER, token_expiration INTEGER);";

struct ApUser
{
	std::string ap_user_id;
	std::string user_name;
	time_t mapping_time{0};
	time_t token_expiration{0};
};

class PlacementDaemon : public Service
{
public:
	~PlacementDaemon();

	void Init();
	void Config();

	bool ReadDatabaseEntries();
	bool AddDatabaseEntry(const ApUser& entry);
	bool UpdateDatabaseEntry(const ApUser& entry);

	bool ReadDatafile();
	bool WriteDatafile();
	bool WriteDatafileEntry(const ApUser& entry);
	bool WriteDatafileEntry(const ApUser& entry, FILE* fp);

	void initialize_classad();
	void update_collector(int);
	void invalidate_ad();

	int command_user_login(int, Stream* stream);
	int command_map_user(int, Stream* stream);

	char* m_name{nullptr};
	ClassAd m_daemon_ad;
	int m_update_collector_tid{-1};
	int m_update_collector_interval{300};

	std::string m_dataFilename;
	std::vector<ApUser> m_apUsers;
	std::string m_databaseFile;
	sqlite3* m_db{nullptr};
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
	daemonCore->Register_CommandWithPayload(MAP_USER, "MAP_USER",
		(CommandHandlercpp)&PlacementDaemon::command_map_user,
		"command_map_user", this, ADMINISTRATOR, true /*force authentication*/);
	
	// set timer to periodically advertise ourself to the collector
	m_update_collector_tid = daemonCore->Register_Timer(0, m_update_collector_interval,
				(TimerHandlercpp)&PlacementDaemon::update_collector, "update_collector", this);

	Config();

	char *db_err_msg;
	int rc = sqlite3_open_v2(m_databaseFile.c_str(), &m_db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
	if (rc != SQLITE_OK) {
		EXCEPT("Failed to open database file %s: %s\n", m_databaseFile.c_str(), sqlite3_errmsg(m_db));
	}
	rc = sqlite3_exec(m_db, StmtCreateTable, nullptr, nullptr, &db_err_msg);
	if (rc != SQLITE_OK) {
		EXCEPT("Failed to create db table: %s\n", db_err_msg);
	}

	//ReadDatafile();
	ReadDatabaseEntries();

	std::string id_list;
	param(id_list, "PLACEMENT_AP_USERS");
	for (const auto& id: StringTokenIterator(id_list)) {
		std::string full_id = id;
		if (full_id.find('@') == std::string::npos) {
			full_id += '@';
			full_id += uid_domain;
		}
		bool found = false;
		for (const auto& entry: m_apUsers) {
			if (entry.ap_user_id == id) {
				found = true;
				break;
			}
		}
		if (!found) {
			m_apUsers.emplace_back();
			m_apUsers.back().ap_user_id = id;
		}
	}

	WriteDatafile();
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

	if (!param(m_dataFilename, "PLACEMENTD_DATAFILE")) {
		EXCEPT("No PLACEMENTD_DATAFILE specified!");
	}

	if (!param(m_databaseFile, "PLACEMENTD_DATABASE_FILE")) {
		EXCEPT("No PLACEMENTD_DATABASE_FILE specified!");
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

bool PlacementDaemon::ReadDatabaseEntries()
{
	sqlite3_stmt* stmt = nullptr;
	int rc = sqlite3_prepare_v2(m_db, "SELECT ap_user_id, user_name, mapping_time, token_expiration FROM mappings;", -1, &stmt, nullptr);
	if (rc != SQLITE_OK) {
		dprintf(D_ERROR, "sqlite3_prepare failed: %s\n", sqlite3_errmsg(m_db));
		sqlite3_finalize(stmt);
		return false;
	}

	while ( (rc = sqlite3_step(stmt)) == SQLITE_ROW ) {
		m_apUsers.emplace_back();
		m_apUsers.back().ap_user_id = (const char*)sqlite3_column_text(stmt, 0);
		m_apUsers.back().user_name = (const char*)sqlite3_column_text(stmt, 1);
		m_apUsers.back().mapping_time = sqlite3_column_int(stmt, 2);
		m_apUsers.back().token_expiration = sqlite3_column_int(stmt, 3);
		dprintf(D_ALWAYS,"JEF read '%s' '%s' %d %d\n",m_apUsers.back().ap_user_id.c_str(), m_apUsers.back().user_name.c_str(),(int)m_apUsers.back().mapping_time,(int)m_apUsers.back().token_expiration);
	}
	if (rc != SQLITE_DONE) {
		dprintf(D_ERROR, "sqlite3_step returned %d\n", rc);
		sqlite3_finalize(stmt);
		return false;
	}

	sqlite3_finalize(stmt);
	return true;
}

bool PlacementDaemon::AddDatabaseEntry(const ApUser& entry)
{
	std::string stmt_str;
	formatstr(stmt_str, "INSERT INTO mappings (ap_user_id, user_name, mapping_time, token_expiration) VALUES ('%s', '%s', %d, %d);", entry.ap_user_id.c_str(), entry.user_name.c_str(), (int)entry.mapping_time, (int)entry.token_expiration);
	char *db_err_msg = nullptr;
	int rc = sqlite3_exec(m_db, stmt_str.c_str(), nullptr, nullptr, &db_err_msg);
	if (rc != SQLITE_OK) {
		dprintf(D_ERROR, "Adding db entry failed: %s\n", db_err_msg);
		free(db_err_msg);
		return false;
	}
	return true;
}

bool PlacementDaemon::UpdateDatabaseEntry(const ApUser& entry)
{
	std::string stmt_str;
	formatstr(stmt_str, "UPDATE mappings SET user_name= '%s', mapping_time= %d, token_expiration = %d WHERE ap_user_id = '%s';", entry.user_name.c_str(), (int)entry.mapping_time, (int)entry.token_expiration, entry.ap_user_id.c_str());
	char *db_err_msg = nullptr;
	int rc = sqlite3_exec(m_db, stmt_str.c_str(), nullptr, nullptr, &db_err_msg);
	if (rc != SQLITE_OK) {
		dprintf(D_ERROR, "Updating db entry failed: %s\n", db_err_msg);
		free(db_err_msg);
		return false;
	}
	return true;
}

bool PlacementDaemon::ReadDatafile()
{
	dprintf(D_FULLDEBUG,"JEF ReadDatafile()\n");
	FILE* fp = safe_fopen_wrapper(m_dataFilename.c_str(), "r");
	if (fp == nullptr) {
		dprintf(D_ERROR, "Failed to open '%s': %s", m_dataFilename.c_str(),
				strerror(errno));
		return false;
	}

	std::string line;
	while (readLine(line, fp)) {
dprintf(D_FULLDEBUG,"  line:'%s'\n",line.c_str());
		std::vector<std::string> items = split(line, ",", STI_NO_TRIM);
		if (items.size() != 4) {
			dprintf(D_ERROR, "Datafile entry has %zu items, skipping\n", items.size());
			continue;
		}
		ApUser new_entry{items[0], items[1], atol(items[2].c_str()), atol(items[3].c_str())};
		bool add_new = true;
		for (auto& old_entry: m_apUsers) {
			if (old_entry.ap_user_id == new_entry.ap_user_id) {
				old_entry = new_entry;
				add_new = false;
dprintf(D_ALWAYS,"ReadDatafile(): update entry for %s\n",new_entry.ap_user_id.c_str());
				break;
			}
		}
		if (add_new) {
dprintf(D_ALWAYS,"ReadDatafile(): new entry for %s\n",new_entry.ap_user_id.c_str());
			m_apUsers.emplace_back(new_entry);
		}
	}

	fclose(fp);

	return true;
}

bool PlacementDaemon::WriteDatafile()
{
	dprintf(D_ALWAYS,"WriteDatafile()\n");
	bool rc = true;
	std::string tmp_file = m_dataFilename + ".tmp";
	FILE* fp = safe_fopen_wrapper(tmp_file.c_str(), "w");
	if (fp == nullptr) {
		dprintf(D_ERROR, "Failed to open '%s': %s\n", tmp_file.c_str(),
				strerror(errno));
		return false;
	}

	for (const auto& entry: m_apUsers) {
		if (!WriteDatafileEntry(entry, fp)) {
			rc = false;
			break;
		}
	}

	fclose(fp);

	if (rename(tmp_file.c_str(), m_dataFilename.c_str()) < 0) {
		dprintf(D_ERROR, "Failed to rename '%s' to '%s': %s\n", tmp_file.c_str(),
				m_dataFilename.c_str(), strerror(errno));
		unlink(tmp_file.c_str());
		rc = false;
	}
	
	return rc;
}

bool PlacementDaemon::WriteDatafileEntry(const ApUser& entry)
{
dprintf(D_ALWAYS,"WriteDatafileEntry()\n");
	bool rc = true;
	FILE* fp = safe_fopen_wrapper(m_dataFilename.c_str(), "a");
	if (fp == nullptr) {
		dprintf(D_ERROR, "Failed to open '%s': %s\n", m_dataFilename.c_str(),
				strerror(errno));
		return false;
	}

	if (!WriteDatafileEntry(entry, fp)) {
		rc = false;
	}

	fclose(fp);

	return rc;
}

bool PlacementDaemon:: WriteDatafileEntry(const ApUser& entry, FILE* fp)
{
	std::string line = entry.ap_user_id;
	line += ',';
	line += entry.user_name;
	line += ',';
	line += std::to_string(entry.mapping_time);
	line += ',';
	line += std::to_string(entry.token_expiration);
	line += '\n';
	if (fwrite(line.c_str(), line.size(), 1, fp) < 1) {
		dprintf(D_ERROR, "Failed to write datafile entry: %s\n", strerror(errno));
		return false;
	}
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
	ApUser* acct_to_use = nullptr;

	CondorError err;
	std::vector<std::string> bounding_set = { "WRITE", "READ" };
	int lifetime = -1; // No expiration
	time_t new_expire = 0; 
	std::string key_name;
	std::string token;
	std::string token_identity;

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

	// TODO allow admin mapping of user_name to os-acct

	// Sanitize the user name for storage
	for (auto& ch: user_name) {
		if (ch == ',' || ch == '\n') {
			ch = ' ';
		}
	}
	trim(user_name);

	for (auto& acct: m_apUsers) {
		dprintf(D_FULLDEBUG,"JEF Checking acct %s\n",acct.ap_user_id.c_str());
		if (acct.user_name.empty()) {
			acct_to_use = &acct;
		} else if (acct.user_name == user_name) {
			acct_to_use = &acct;
			break;
		}
	}

	if (acct_to_use == nullptr) {
		// No available accts, return failure
		dprintf(D_ERROR, "No accounts available for %s command: aborting\n", cmd_name);
		result_ad.Assign(ATTR_ERROR_STRING, "No accounts available");
		result_ad.Assign(ATTR_ERROR_CODE, 3);
		goto send_reply;
	}

	if (acct_to_use->user_name.empty()) {
		dprintf(D_FULLDEBUG,"JEF Claiming new account %s for user %s\n", acct_to_use->ap_user_id.c_str(), user_name.c_str());
		acct_to_use->user_name = user_name;
		acct_to_use->mapping_time = time(nullptr);
		AddDatabaseEntry(*acct_to_use);
	}
	else dprintf(D_FULLDEBUG,"JEF Using existing account %s for user %s\n", acct_to_use->ap_user_id.c_str(), user_name.c_str());

	// TODO interact with schedd
	// TODO record assignment persistently

	dprintf(D_FULLDEBUG,"JEF Creating IDToken\n");
	// Create an IDToken for this user
	key_name = htcondor::get_token_signing_key(err);
	if (key_name.empty()) {
		result_ad.Assign(ATTR_ERROR_STRING, err.getFullText());
		result_ad.Assign(ATTR_ERROR_CODE, err.code());
		goto send_reply;
	}
	token_identity = acct_to_use->ap_user_id;
	lifetime = param_integer("PLACEMENTD_TOKEN_DURATION", 24*60*60);
	if (!Condor_Auth_Passwd::generate_token(token_identity, key_name, bounding_set, lifetime, token, rsock->getUniqueId(), &err)) {
		result_ad.Assign(ATTR_ERROR_STRING, err.getFullText());
		result_ad.Assign(ATTR_ERROR_CODE, err.code());
		goto send_reply;
	}

	new_expire = lifetime + time(nullptr);
	if (new_expire > acct_to_use->token_expiration) {
		acct_to_use->token_expiration = new_expire;
	}
	WriteDatafileEntry(*acct_to_use);
	UpdateDatabaseEntry(*acct_to_use);

	dprintf(D_AUDIT, *rsock, "User Login token issued for UserName '%s', AP user account %s\n", user_name.c_str(), acct_to_use->ap_user_id.c_str());

	result_ad.Assign(ATTR_SEC_TOKEN, token);

 send_reply:
	// Finally, close up shop.  We have to send the result ad to signal the end.
	if( !putClassAd(stream, result_ad) || !stream->end_of_message() ) {
		dprintf( D_ALWAYS, "Error sending result ad for %s command\n", cmd_name );
		return FALSE;
	}
	return TRUE;
}

int PlacementDaemon::command_map_user(int cmd, Stream* stream)
{
	const char * cmd_name = getCommandStringSafe(cmd);
	ClassAd cmd_ad;

	dprintf( D_FULLDEBUG, "In command_map_user\n" );

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
	std::string ap_user_id;
	ApUser* acct_to_use = nullptr;

	CondorError err;

	cmd_ad.LookupString("UserName", user_name);
	if (user_name.empty()) {
		dprintf(D_ERROR, "Missing UserName for %s command: aborting\n", cmd_name);
		result_ad.Assign(ATTR_ERROR_STRING, "Missing UserName");
		result_ad.Assign(ATTR_ERROR_CODE, 2);
		goto send_reply;
	}

	// Sanitize the user name for storage
	for (auto& ch: user_name) {
		if (ch == ',' || ch == '\n') {
			ch = ' ';
		}
	}
	trim(user_name);

	cmd_ad.LookupString("ApUserId", ap_user_id);
	if (ap_user_id.empty()) {
		dprintf(D_FULLDEBUG, "Missing ApUserId for %s command, will use UserName\n", cmd_name);
		ap_user_id = user_name;
	}
	if (ap_user_id.find('@') == std::string::npos) {
		std::string uid_domain;
		if (!param(uid_domain, "PLACEMENT_UID_DOMAIN")) {
			param(uid_domain, "UID_DOMAIN");
		}
		ap_user_id += "@" + uid_domain;
	}

	for (auto& acct: m_apUsers) {
		dprintf(D_FULLDEBUG,"JEF Checking acct %s\n",acct.ap_user_id.c_str());
		if (acct.user_name == user_name) {
			if (acct.ap_user_id == ap_user_id) {
				acct_to_use = &acct;
				break;
			} else {
				dprintf(D_ERROR, "UserName %s already mapped to %s\n", user_name.c_str(), acct.ap_user_id.c_str());
				result_ad.Assign(ATTR_ERROR_STRING, "UserName already mapped");
				result_ad.Assign(ATTR_ERROR_CODE, 2);
				goto send_reply;
			}
		} else if (acct.ap_user_id == ap_user_id) {
			if (acct.user_name.empty()) {
				acct_to_use = &acct;
				break;
			} else {
				dprintf(D_ERROR, "ApUserId %s already mapped to %s\n", ap_user_id.c_str(), acct.user_name.c_str());
				result_ad.Assign(ATTR_ERROR_STRING, "ApUserId already mapped");
				result_ad.Assign(ATTR_ERROR_CODE, 2);
				goto send_reply;
			}
		}
	}

	if (acct_to_use == nullptr) {
		// Add new account entry
		dprintf(D_FULLDEBUG, "JEF adding new entry %s for user %s\n", ap_user_id.c_str(), user_name.c_str());
		m_apUsers.emplace_back();
		acct_to_use = &m_apUsers.back();
		acct_to_use->ap_user_id = ap_user_id;
	}
	if (acct_to_use->user_name.empty()) {
		// Add new mapping
		dprintf(D_FULLDEBUG, "JEF Claiming existing entry %s for user %s\n", acct_to_use->ap_user_id.c_str(), user_name.c_str());
		acct_to_use->user_name = user_name;
		acct_to_use->mapping_time = time(nullptr);
		AddDatabaseEntry(*acct_to_use);
	}

	WriteDatafileEntry(*acct_to_use);

	dprintf(D_AUDIT, *rsock, "User mapping made for UserName '%s', AP user account %s\n", user_name.c_str(), acct_to_use->ap_user_id.c_str());

	result_ad.Assign("ApUserId", acct_to_use->ap_user_id);

 send_reply:
	// Finally, close up shop.  We have to send the result ad to signal the end.
	if( !putClassAd(stream, result_ad) || !stream->end_of_message() ) {
		dprintf( D_ALWAYS, "Error sending result ad for %s command\n", cmd_name );
		return FALSE;
	}
	return TRUE;
}
