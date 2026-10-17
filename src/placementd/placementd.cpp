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
#include "dc_schedd.h"
#include "compat_classad_util.h"
#include "condor_uid.h"
#include "placementd_db.h"
#include "placementd_reconcile.h"
#include "placementd_edit.h"

#include <set>

GCC_DIAG_OFF(float-equal)
GCC_DIAG_OFF(cast-qual)
#include "jwt-cpp/jwt.h"
GCC_DIAG_ON(float-equal)
GCC_DIAG_ON(cast-qual)

// MyType of our generic daemon ad.
#define PLACEMENTD_ADTYPE "PlacementD"
// MyType of the ad that ends each reply.
#define PLACEMENTD_SUMMARY_ADTYPE "Summary"

class PlacementDaemon : public Service
{
public:
	PlacementDaemon() : m_reconciler(m_db) {}
	~PlacementDaemon();

	void Init();
	void Config();

	void publish_status();
	void update_collector(int);
	void invalidate_ad();

	void timer_read_mapfiles(int);

	int command_user_login(int, Stream* stream);
	int command_query_users(int, Stream* stream);
	int command_query_tokens(int, Stream* stream);
	int command_query_authorizations(int, Stream* stream);
	int command_query(int, Stream* stream);
	int command_edit(int, Stream* stream);

private:
	// Mint a token for a PLACEMENT_USER_LOGIN request.  On failure,
	// result_ad gets ErrorCode and ErrorString.
	void user_login(ReliSock* rsock, const ClassAd& cmd_ad, ClassAd& result_ad);
	// Check the schedd's records for the user and project, creating the
	// missing ones.  False (with result_ad filled in) if a record is
	// disabled or the schedd cannot be reached.
	bool check_schedd_records(const std::string& user, const std::string& project, ClassAd& result_ad);

	char* m_name{nullptr};
	ClassAd m_daemon_ad;
	int m_update_collector_tid{-1};
	int m_update_collector_interval{300};
	int m_read_mapfiles_tid{-1};

	std::string m_databaseFile;
	PlacementDb m_db;
	PlacementReconciler m_reconciler;

	std::string m_mapFile;		// empty = reconcile disabled
	std::string m_authzMapFile;	// empty = no authorizations file
	std::string m_uidDomain;
};

PlacementDaemon* placementd = nullptr;

// A login refusal's ErrorString: what failed, then the CondorError text if
// there is any.  The ErrorCode is always the placementd's own (11 or 12),
// never the CondorError's, which may be 0 or collide with another meaning.
static std::string
failure_text(const char* what, const CondorError& err)
{
	std::string text = what;
	std::string detail = err.getFullText();
	if (!detail.empty()) {
		text += ": " + detail;
	}
	return text;
}

// Read one request ad and the end of message, then switch to encode.
static bool
recv_request(Stream* stream, ClassAd& cmd_ad, const char* cmd_name)
{
	stream->decode();
	stream->timeout(15);

	if( !getClassAd(stream, cmd_ad)) {
		dprintf( D_ERROR, "Failed to receive request ad for %s command: aborting\n", cmd_name);
		return false;
	}

	if (!stream->end_of_message()) {
		dprintf( D_ERROR, "Failed to receive EOM: for %s command: aborting\n", cmd_name );
		return false;
	}
	// done reading input command stream
	stream->encode();
	return true;
}

static int
send_summary(Stream* stream, ClassAd& summary_ad, const char* cmd_name)
{
	dprintf(D_FULLDEBUG,"Sending summary ad\n");
	// Finally, close up shop.  We have to send the result ad to signal the end.
	if( !putClassAd(stream, summary_ad) || !stream->end_of_message() ) {
		dprintf( D_ERROR, "Error sending result ad for %s command\n", cmd_name );
		return FALSE;
	}
	return TRUE;
}

static int
send_error(Stream* stream, ClassAd& summary_ad, const char* cmd_name, int code, const std::string& msg)
{
	summary_ad.Assign(ATTR_ERROR_STRING, msg);
	summary_ad.Assign(ATTR_ERROR_CODE, code);
	return send_summary(stream, summary_ad, cmd_name);
}

static int
send_db_error(Stream* stream, ClassAd& summary_ad, const char* cmd_name, const PlacementError& e)
{
	dprintf(D_ERROR, "Placementd database failure: %s\n", e.what());
	return send_error(stream, summary_ad, cmd_name, PLACEMENT_ERR_DATABASE, "Failed to read database");
}

static ClassAd
summary_ad()
{
	ClassAd ad;
	ad.Assign(ATTR_MY_TYPE, PLACEMENTD_SUMMARY_ADTYPE);
	return ad;
}

PlacementDaemon::~PlacementDaemon()
{
	// tell our collector we're going away
	invalidate_ad();

	m_db.Close();
	free(m_name);
}

void
PlacementDaemon::Init()
{
	Config();

	m_db.Open(m_databaseFile);
	try {
		PlacementLockRelease release(m_db.Statements());
		m_reconciler.Status().Load(m_db);
	} catch (const PlacementError& e) {
		dprintf(D_ERROR, "Failed to load placementd mapfile status: %s\n", e.what());
	}

	// Import the map files before accepting commands, so that the first
	// login after an upgrade sees them.
	m_reconciler.Run(m_mapFile, m_authzMapFile, m_uidDomain);
	publish_status();

	// All commands run at ADMINISTRATOR with forced authentication: the
	// caller (a web front end) acts on behalf of the users it names.
	struct {
		int cmd;
		const char* name;
		int (PlacementDaemon::*handler)(int, Stream*);
	} commands[] = {
		{PLACEMENT_USER_LOGIN, "PLACEMENT_USER_LOGIN", &PlacementDaemon::command_user_login},
		{PLACEMENT_QUERY_USERS, "PLACEMENT_QUERY_USERS", &PlacementDaemon::command_query_users},
		{PLACEMENT_QUERY_TOKENS, "PLACEMENT_QUERY_TOKENS", &PlacementDaemon::command_query_tokens},
		{PLACEMENT_QUERY_AUTHORIZATIONS, "PLACEMENT_QUERY_AUTHORIZATIONS", &PlacementDaemon::command_query_authorizations},
		{PLACEMENT_QUERY, "PLACEMENT_QUERY", &PlacementDaemon::command_query},
		{PLACEMENT_EDIT, "PLACEMENT_EDIT", &PlacementDaemon::command_edit},
	};
	for (const auto& c: commands) {
		auto handler = c.handler;
		daemonCore->Register_Command(c.cmd, c.name,
			[this, handler](int cmd, Stream* stream) { return (this->*handler)(cmd, stream); },
			c.name, ADMINISTRATOR, true /*force authentication*/, STANDARD_COMMAND_PAYLOAD_TIMEOUT);
	}

	// set timer to periodically advertise ourself to the collector
	m_update_collector_tid = daemonCore->Register_Timer(0, m_update_collector_interval,
		[this](int tid) { update_collector(tid); }, "update_collector");

	// The first reconcile already ran above.
	m_read_mapfiles_tid = daemonCore->Register_Timer(300, 300,
		[this](int tid) { timer_read_mapfiles(tid); }, "timer_read_mapfiles");
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
	m_daemon_ad.Assign(ATTR_MY_TYPE, PLACEMENTD_ADTYPE);
	m_daemon_ad.Assign(ATTR_NAME, m_name);

	// Publish all DaemonCore-specific attributes, which also handles
	// SUBSYS_ATTRS for us.
	daemonCore->publish(&m_daemon_ad);
	publish_status();

	// reset the timer (if it exists) to update the collector
	if (m_update_collector_tid != -1) {
		daemonCore->Reset_Timer(m_update_collector_tid, 0, m_update_collector_interval);
	}

	std::string db_file;
	if (!param(db_file, "TOKENS_DATABASE")) {
		EXCEPT("No TOKENS_DATABASE specified!");
	}
	if (m_databaseFile.empty()) {
		m_databaseFile = db_file;
	} else if (db_file != m_databaseFile) {
		dprintf(D_ALWAYS, "TOKENS_DATABASE changed to %s; the change takes effect on restart\n", db_file.c_str());
	}

	// PLACEMENTD_MAPFILE has a default path; setting it to the empty
	// string disables the reconcile, leaving the mapfile rows read-only
	// until they are adopted.
	if (!param(m_mapFile, "PLACEMENTD_MAPFILE")) {
		m_mapFile.clear();
	}

	if (!param(m_authzMapFile, "PLACEMENTD_AUTHORIZATIONS_MAPFILE")) {
		m_authzMapFile.clear();
	}

	if (!param(m_uidDomain, "PLACEMENTD_UID_DOMAIN")) {
		param(m_uidDomain, "UID_DOMAIN");
	}
	PlacementConfigureReserved();

	// condor_reconfig reconciles right away.
	if (m_read_mapfiles_tid != -1) {
		daemonCore->Reset_Timer(m_read_mapfiles_tid, 0, 300);
	}
}

void
PlacementDaemon::publish_status()
{
	m_reconciler.Status().Publish(m_daemon_ad);
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
	query_ad.Assign(ATTR_TARGET_TYPE, PLACEMENTD_ADTYPE);

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

void
PlacementDaemon::timer_read_mapfiles(int)
{
	bool ran = m_reconciler.Run(m_mapFile, m_authzMapFile, m_uidDomain);
	publish_status();
	if (ran) {
		update_collector(0);
	}
}

int
PlacementDaemon::command_user_login(int cmd, Stream* stream)
{
	PlacementLockRelease release(m_db.Statements());
	const char * cmd_name = getCommandStringSafe(cmd);
	ClassAd cmd_ad;

	dprintf( D_FULLDEBUG, "In command_user_login\n" );

	if (!recv_request(stream, cmd_ad, cmd_name)) {
		return FALSE;
	}

	ClassAd result_ad;
	user_login((ReliSock*)stream, cmd_ad, result_ad);
	m_db.ReleaseReadLocks(cmd_name);

	// Finally, close up shop.  We have to send the result ad to signal the end.
	if( !putClassAd(stream, result_ad) || !stream->end_of_message() ) {
		dprintf( D_ERROR, "Error sending result ad for %s command\n", cmd_name );
		return FALSE;
	}
	return TRUE;
}

bool
PlacementDaemon::check_schedd_records(const std::string& user, const std::string& project, ClassAd& result_ad)
{
	auto refuse = [&](int code, const std::string& msg) {
		result_ad.Assign(ATTR_ERROR_STRING, msg);
		result_ad.Assign(ATTR_ERROR_CODE, code);
		return false;
	};

	DCSchedd schedd;
	CondorError err;
	if (!schedd.locate()) {
		dprintf(D_ERROR, "Failed to locate schedd\n");
		return refuse(PLACEMENT_ERR_SCHEDD, "Failed to locate schedd");
	}

	// Fail closed.  The schedd finds a user record with is_same_user() (the
	// domain compared without case, and one domain may be a prefix of the
	// other) and a project record by name without case.  Creating a record
	// that exists re-enables it, so ask for every record that could be the
	// one the schedd would find, refuse if any of them is disabled, and
	// create only when there is none.  The values are quoted.
	bool user_exists = false;
	bool project_exists = false;
	bool user_disabled = false;
	bool project_disabled = false;
	{
		ClassAd query_ad;
		ClassAd* summary = nullptr;
		std::string quoted, constraint;
		std::string user_part = user.substr(0, user.find('@'));
		formatstr(constraint, "(%s == \"%s\" && splitusername(%s)[0] == %s)", ATTR_MY_TYPE, OWNER_ADTYPE,
			ATTR_USER, QuoteAdStringValue(user_part.c_str(), quoted));
		if (!project.empty()) {
			formatstr_cat(constraint, " || (%s == \"%s\" && %s == %s)", ATTR_MY_TYPE, PROJECT_ADTYPE,
				ATTR_NAME, QuoteAdStringValue(project.c_str(), quoted));
		}
		const char* projection = ATTR_MY_TYPE "," ATTR_ENABLED "," ATTR_USER "," ATTR_NAME;
		schedd.makeUsersQueryAd(query_ad, constraint.c_str(), projection);
		if (!project.empty()) {
			query_ad.Assign(ATTR_TARGET_TYPE, OWNER_ADTYPE "," PROJECT_ADTYPE);
		}
		// A record whose Enabled is not a boolean counts as disabled.
		auto disabled = [](ClassAd* ad) {
			bool enabled = true;
			return ad->Lookup(ATTR_ENABLED) && (!ad->EvaluateAttrBoolEquiv(ATTR_ENABLED, enabled) || !enabled);
		};
		const auto match_user = (CompareUsersOpt)(COMPARE_DOMAIN_PREFIX | ASSUME_UID_DOMAIN | CASELESS_USER);
		auto process_func = [&](void*, ClassAd* ad) -> int {
			std::string ad_type, value;
			ad->LookupString(ATTR_MY_TYPE, ad_type);
			if (strcasecmp(ad_type.c_str(), OWNER_ADTYPE) == MATCH) {
				// The user is compared without case too: never less than
				// the schedd would match.
				if (ad->LookupString(ATTR_USER, value) &&
					is_same_user(user.c_str(), value.c_str(), match_user, nullptr)) {
					user_exists = true;
					user_disabled = user_disabled || disabled(ad);
				}
			} else if (strcasecmp(ad_type.c_str(), PROJECT_ADTYPE) == MATCH) {
				if (!project.empty() && ad->LookupString(ATTR_NAME, value) &&
					strcasecmp(value.c_str(), project.c_str()) == MATCH) {
					project_exists = true;
					project_disabled = project_disabled || disabled(ad);
				}
			}
			return 1;
		};
		int rval = schedd.queryUsers(query_ad, process_func, nullptr, 10, &err, &summary);
		delete summary;
		if (rval != 0) {
			std::string msg = failure_text("Failed to query the schedd", err);
			dprintf(D_ERROR, "%s\n", msg.c_str());
			return refuse(PLACEMENT_ERR_SCHEDD, msg);
		}
	}
	if (user_disabled) {
		dprintf(D_ERROR, "User %s is disabled, not issuing token\n", user.c_str());
		return refuse(PLACEMENT_ERR_SCHEDD, "User is disabled");
	}
	if (project_disabled) {
		dprintf(D_ERROR, "Project %s is disabled, not issuing token\n", project.c_str());
		return refuse(PLACEMENT_ERR_SCHEDD, "Project is disabled");
	}

	auto add_record = [&](bool user_record, const std::string& name) {
		const char *names[] = { name.c_str() };
		std::unique_ptr<ClassAd> reply(user_record ? schedd.addUsers(names, 1, &err) : schedd.addProjects(names, 1, &err));
		std::string what = std::string("Failed to add the ") + (user_record ? "user" : "project") +
			" record in the schedd";
		if (!reply) {
			std::string msg = failure_text(what.c_str(), err);
			dprintf(D_ERROR, "%s\n", msg.c_str());
			return refuse(PLACEMENT_ERR_SCHEDD, msg);
		}
		int rval = -1;
		reply->LookupInteger(ATTR_RESULT, rval);
		if (rval != 0) {
			std::string err_str;
			reply->LookupString(ATTR_ERROR_STRING, err_str);
			std::string msg;
			formatstr(msg, "%s: %d%s%s", what.c_str(), rval, err_str.empty() ? "" : " ", err_str.c_str());
			dprintf(D_ERROR, "%s\n", msg.c_str());
			return refuse(PLACEMENT_ERR_SCHEDD, msg);
		}
		return true;
	};
	if (!user_exists && !add_record(true, user)) {
		return false;
	}
	if (!project.empty() && !project_exists && !add_record(false, project)) {
		return false;
	}
	return true;
}

void
PlacementDaemon::user_login(ReliSock* rsock, const ClassAd& cmd_ad, ClassAd& result_ad)
{
	auto refuse = [&](int code, const std::string& msg) {
		result_ad.Assign(ATTR_ERROR_STRING, msg);
		result_ad.Assign(ATTR_ERROR_CODE, code);
	};

	if (!rsock->get_encryption()) {
		return refuse(PLACEMENT_ERR_NOT_ENCRYPTED, "Request to server was not encrypted.");
	}

	std::string user_name, requester, project, authz_list;
	cmd_ad.LookupString("UserName", user_name);
	if (user_name.empty()) {
		dprintf(D_ERROR, "Missing UserName for PLACEMENT_USER_LOGIN command: aborting\n");
		return refuse(PLACEMENT_ERR_MISSING_USERNAME, "Missing UserName");
	}
	cmd_ad.LookupString("Project", project);	// project requested for this token
	cmd_ad.LookupString("Authorizations", authz_list);	// authorizations requested
	cmd_ad.LookupString("Requester", requester);
	if (requester.empty()) {
		requester = user_name;
	}
	// For the log: these are the client's, unchecked until looked up.
	const std::string log_user = PlacementSanitize(user_name, PLACEMENTD_MAX_IDENTITY);
	const std::string log_requester = PlacementSanitize(requester, PLACEMENTD_MAX_IDENTITY);

	PlacementLoginInfo user_info, requester_info;
	bool user_found = false, requester_found = false;
	try {
		user_found = m_db.LookupLogin(user_name, user_info);
		if (user_found && requester != user_name) {
			requester_found = m_db.LookupLogin(requester, requester_info);
		}
	} catch (const PlacementError& e) {
		dprintf(D_ERROR, "Placementd database failure: %s\n", e.what());
		return refuse(PLACEMENT_ERR_DATABASE, "Failed to read database");
	}
	// Release the read lock before the schedd round trip and before the
	// auth layer records the token in the same database.
	m_db.ReleaseReadLocks("login");
	if (!user_found) {
		dprintf(D_FULLDEBUG, "User %s is not authorized\n", log_user.c_str());
		return refuse(PLACEMENT_ERR_NOT_AUTHORIZED, "User not authorized");
	}

	if (requester != user_name) {
		if (!requester_found) {
			dprintf(D_FULLDEBUG, "Requester %s is unknown\n", log_requester.c_str());
			return refuse(PLACEMENT_ERR_UNKNOWN_REQUESTER, "Requester not known");
		}
		if (!contains_anycase(requester_info.grants.AuthorizationNames(), "INSTRUCTOR")) {
			dprintf(D_FULLDEBUG, "Requester %s doesn't have INSTRUCTOR\n", log_requester.c_str());
			return refuse(PLACEMENT_ERR_REQUESTER_NOT_INSTRUCTOR, "Requester not authorized as INSTRUCTOR");
		}
		if (requester_info.expires > 0 && requester_info.expires < time(nullptr)) {
			dprintf(D_FULLDEBUG, "Requester %s mapping is expired\n", log_requester.c_str());
			return refuse(PLACEMENT_ERR_REQUESTER_EXPIRED, "Requester mapping is expired");
		}
	}

	// What may be minted: checked here too, for rows that came from the
	// map file.
	const std::string& token_identity = user_info.account_name;
	std::string why;
	if (!PlacementValidName(PlacementEntity::Account, token_identity, why) ||
		PlacementReservedAccount(token_identity)) {
		dprintf(D_ERROR, "Refusing to mint a token for account '%s'\n", PlacementSanitize(token_identity, 256).c_str());
		return refuse(PLACEMENT_ERR_NOT_AUTHORIZED, "User's account may not be issued tokens");
	}

	std::string project_uuid;
	ClassAd extra_claims;
	if (!project.empty()) {
		bool found = false;
		for (const auto& item: user_info.grants.projects) {
			if (strcasecmp(project.c_str(), item.name.c_str()) == 0) {
				found = true;
				project_uuid = item.uuid;
				break;
			}
		}
		if (!found || !PlacementValidName(PlacementEntity::Project, project, why)) {
			dprintf(D_FULLDEBUG, "User %s isn't authorized for requested project %s\n", log_user.c_str(),
				PlacementSanitize(project, 256).c_str());
			return refuse(PLACEMENT_ERR_PROJECT_NOT_ALLOWED, "User not authorized for project");
		}
		extra_claims.Assign("project", project);
	}

	// The bounding set is the requested subset of the grants, or all of
	// them.  No grants, no token: an empty bounding set would be unscoped.
	std::vector<std::string> full_authz_list = user_info.grants.AuthorizationNames();
	if (full_authz_list.empty()) {
		dprintf(D_FULLDEBUG, "User %s's account has no authorizations\n", log_user.c_str());
		return refuse(PLACEMENT_ERR_BAD_AUTHORIZATIONS, "User's account has no authorizations");
	}
	std::vector<std::string> bounding_set;
	for (const auto& one_authz: StringTokenIterator(authz_list)) {
		if (!contains_anycase(full_authz_list, one_authz)) {
			dprintf(D_FULLDEBUG, "User %s don't have authorization type %s\n", log_user.c_str(),
				PlacementSanitize(one_authz, 256).c_str());
			return refuse(PLACEMENT_ERR_BAD_AUTHORIZATIONS, "User don't have requested authorizations");
		}
		bounding_set.emplace_back(one_authz);
	}
	if (bounding_set.empty()) {
		dprintf(D_FULLDEBUG, "Client didn't request any authorizations, giving full set\n");
		bounding_set = full_authz_list;
	}
	for (const auto& one_authz: bounding_set) {
		if (PlacementPrivilegedAuthorization(one_authz) ||
			!PlacementValidName(PlacementEntity::Authorization, one_authz, why)) {
			dprintf(D_ERROR, "Refusing to mint a token with authorization '%s'\n",
				PlacementSanitize(one_authz, 256).c_str());
			return refuse(PLACEMENT_ERR_BAD_AUTHORIZATIONS, "Requested authorizations may not be issued");
		}
	}
	authz_list = join(bounding_set, ",");

	int lifetime = param_integer("PLACEMENTD_TOKEN_DURATION", 24*60*60);
	if (user_info.expires > 0) {
		if (user_info.expires <= time(nullptr)) {
			dprintf(D_FULLDEBUG, "User mapping is expired\n");
			return refuse(PLACEMENT_ERR_EXPIRED, "User mapping is expired");
		}
		if (user_info.expires - time(nullptr) < lifetime) {
			lifetime = user_info.expires - time(nullptr);
		}
	}

	// Check the status of the user/project record in the schedd.
	// If they don't exist, create them.
	// If they exist and are disabled, don't issue a token.
	if (!check_schedd_records(token_identity, project, result_ad)) {
		return;
	}

	dprintf(D_FULLDEBUG, "Creating IDToken\n");
	CondorError err;
	std::string token;
	std::string key_name = htcondor::get_token_signing_key(err);
	if (key_name.empty()) {
		std::string msg = failure_text("Failed to find a token signing key", err);
		dprintf(D_ERROR, "%s\n", msg.c_str());
		return refuse(PLACEMENT_ERR_DATABASE, msg);
	}
	// This also records the token in TOKENS_DATABASE (idtokens_minting).
	if (!Condor_Auth_Passwd::generate_token(token_identity, key_name, bounding_set, lifetime, true, token,
		rsock->getUniqueId(), &err, &extra_claims)) {
		std::string msg = failure_text("Failed to generate a token", err);
		dprintf(D_ERROR, "%s\n", msg.c_str());
		return refuse(PLACEMENT_ERR_DATABASE, msg);
	}

	std::string token_jti;
	long long token_exp = 0;
	{
		auto jwt = jwt::decode(token);
		token_jti = jwt.get_id();
		auto datestamp = jwt.get_expires_at();
		token_exp = std::chrono::duration_cast<std::chrono::seconds>(datestamp.time_since_epoch()).count();
	}

	// The text columns are the audit record and survive deletes; the uuid
	// columns name what was used at issue time.
	try {
		m_db.Statements().Get("INSERT INTO placementd_tokens (requester_id, foreign_id, ap_id, authz, token_jti,"
			" token_exp, token_project, identity_uuid, account_uuid, project_uuid)"
			" VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10)")
			.bind(1, requester).bind(2, user_name).bind(3, token_identity).bind(4, authz_list)
			.bind(5, token_jti).bind(6, token_exp).bind(7, project).bind(8, user_info.identity_uuid)
			.bind(9, user_info.account_uuid)
			.bind(10, project_uuid.empty() ? std::optional<std::string>() : std::optional<std::string>(project_uuid))
			.exec();
	} catch (const PlacementError& e) {
		// The token is valid but unlogged, and the client will not get it:
		// revoke it by removing it from the auth layer's table.
		dprintf(D_ERROR, "Placementd database failure: %s; revoking token %s\n", e.what(), token_jti.c_str());
		try {
			m_db.Statements().Get("DELETE FROM idtokens_minting_extra_claims WHERE token_jti = ?1")
				.bind(1, token_jti).exec();
			m_db.Statements().Get("DELETE FROM idtokens_minting WHERE token_jti = ?1").bind(1, token_jti).exec();
		} catch (const PlacementError& revoke_error) {
			dprintf(D_ERROR, "Failed to revoke unlogged token %s: %s\n", token_jti.c_str(), revoke_error.what());
		}
		return refuse(PLACEMENT_ERR_DATABASE, "Failed to record the token in the database");
	}

	dprintf(D_AUDIT, *rsock, "User Login token issued for UserName '%s', AP user account '%s', authz '%s'\n",
		log_user.c_str(), token_identity.c_str(), authz_list.c_str());

	result_ad.Assign(ATTR_SEC_TOKEN, token);
}

int
PlacementDaemon::command_query_users(int cmd, Stream* stream)
{
	PlacementLockRelease release(m_db.Statements());
	const char * cmd_name = getCommandStringSafe(cmd);
	ClassAd cmd_ad;
	std::string username;

	dprintf( D_FULLDEBUG, "In command_query_users\n" );

	if (!recv_request(stream, cmd_ad, cmd_name)) {
		return FALSE;
	}

	cmd_ad.LookupString("Username", username);

	ClassAd summary = summary_ad();

	struct reply_rec {
		std::string sub;
		std::string authz;
		std::string projects;
		time_t token_exp{0};
		time_t mapping_exp{0};
	};

	std::map<std::string, reply_rec> reply_users;
	std::set<std::string> mapped;
	try {
		std::vector<PlacementLegacyUser> users;
		m_db.ListLegacyUsers(username, users);
		for (const auto& user: users) {
			reply_users[user.identity] = {user.account, user.authorizations, user.projects, 0, user.expires};
			mapped.insert(user.identity);
		}

		// Identities that hold an unexpired token are listed even when
		// they are no longer mapped (Authorized = false).
		std::string sql = "SELECT foreign_id, token_exp, ap_id FROM placementd_tokens WHERE token_exp >= ?1";
		if (!username.empty()) {
			sql += " AND foreign_id = ?2";
		}
		PlacementStmt stmt(m_db.handle(), sql.c_str());
		stmt.bind(1, (long long)time(nullptr));
		if (!username.empty()) {
			stmt.bind(2, username);
		}
		while (stmt.step()) {
			std::string rec_user = stmt.getText(0);
			time_t rec_exp = (time_t)stmt.getInt(1);
			std::string rec_sub = stmt.getText(2);
			// A mapped identity's account is its mapping's, whatever its
			// tokens were minted for; one known only from the token log
			// reports its newest token's.
			auto reply_it = reply_users.find(rec_user);
			if (reply_it == reply_users.end()) {
				reply_users[rec_user].token_exp = rec_exp;
				reply_users[rec_user].sub = rec_sub;
			} else if (rec_exp > reply_it->second.token_exp) {
				reply_it->second.token_exp = rec_exp;
				if (!mapped.count(rec_user)) {
					reply_it->second.sub = rec_sub;
				}
			}
		}
	} catch (const PlacementError& e) {
		m_db.ReleaseReadLocks(cmd_name);
		return send_db_error(stream, summary, cmd_name, e);
	}
	// Never hold a read lock while a client reads the reply.
	m_db.ReleaseReadLocks(cmd_name);

	ClassAd user_ad;

	for (const auto& entry: reply_users) {
		const reply_rec& rec = entry.second;
		user_ad.Clear();
		user_ad.Assign("UserName", entry.first);
		user_ad.Assign("ApUserId", rec.sub);
		user_ad.Assign("TokenExpiration", rec.token_exp);
		// Existing clients read the misspelled name.
		user_ad.Assign("MappingExpration", rec.mapping_exp);
		user_ad.Assign("MappingExpiration", rec.mapping_exp);
		if (!rec.projects.empty()) {
			user_ad.Assign("Projects", rec.projects);
		}
		if (!rec.authz.empty()) {
			user_ad.Assign("Authorizations", rec.authz);
		}
		bool authorized = mapped.count(entry.first) > 0;
		user_ad.Assign("Authorized", authorized);
		if (authorized) {
			user_ad.Assign("MappedAccount", rec.sub);
		}
		if (!putClassAd(stream, user_ad)) {
			dprintf(D_ERROR, "Error sending user ad for %s command\n", cmd_name);
			return FALSE;
		}
	}

	return send_summary(stream, summary, cmd_name);
}

int
PlacementDaemon::command_query_tokens(int cmd, Stream* stream)
{
	PlacementLockRelease release(m_db.Statements());
	const char * cmd_name = getCommandStringSafe(cmd);
	ClassAd cmd_ad;
	std::string jti;
	std::string username;
	bool valid_only = false;

	dprintf( D_FULLDEBUG, "In command_query_tokens\n" );

	if (!recv_request(stream, cmd_ad, cmd_name)) {
		return FALSE;
	}

	cmd_ad.LookupString("TokenId", jti);
	cmd_ad.LookupString("Username", username);
	cmd_ad.LookupBool("ValidOnly", valid_only);

	ClassAd summary = summary_ad();

	std::string where_str;
	bool bind_username = false;
	bool bind_time = false;
	if (!jti.empty()) {
		where_str = "WHERE token_jti=?1 ";
	} else if (!username.empty() || valid_only) {
		where_str = "WHERE ";
		if (!username.empty()) {
			bind_username = true;
			where_str += "foreign_id=?2 ";
		}
		if (!username.empty() && valid_only) {
			where_str += "AND ";
		}
		if (valid_only) {
			bind_time = true;
			where_str += "token_exp >= ?3";
		}
	}
	std::string stmt_str;
	formatstr(stmt_str, "SELECT requester_id, foreign_id, ap_id, authz, token_jti, token_exp, token_project,"
		" identity_uuid, account_uuid, project_uuid FROM placementd_tokens %s;", where_str.c_str());

	// Read every row first: never hold a read lock while a client reads
	// the reply.
	std::vector<ClassAd> token_ads;
	try {
		PlacementStmt stmt(m_db.handle(), stmt_str.c_str());
		if (!jti.empty()) {
			stmt.bind(1, jti);
		}
		if (bind_username) {
			stmt.bind(2, username);
		}
		if (bind_time) {
			stmt.bind(3, (long long)time(nullptr));
		}

		const char* id_attrs[] = { "IdentityId", "AccountId", "ProjectId" };
		while (stmt.step()) {
			ClassAd& token_ad = token_ads.emplace_back();
			token_ad.Assign("Requester", stmt.getText(0));
			token_ad.Assign("UserName", stmt.getText(1));
			token_ad.Assign("ApUserId", stmt.getText(2));
			token_ad.Assign("Authorizations", stmt.getText(3));
			token_ad.Assign("TokenId", stmt.getText(4));
			token_ad.Assign("TokenExpiration", stmt.getInt(5));
			std::string str = stmt.getText(6);
			if (!str.empty()) {
				token_ad.Assign("Project", str);
			}
			for (int i = 0; i < 3; i++) {
				str = stmt.getText(7 + i);
				if (!str.empty()) {
					token_ad.Assign(id_attrs[i], str);
				}
			}
		}
	} catch (const PlacementError& e) {
		m_db.ReleaseReadLocks(cmd_name);
		return send_db_error(stream, summary, cmd_name, e);
	}
	m_db.ReleaseReadLocks(cmd_name);

	for (auto& token_ad: token_ads) {
		if (!putClassAd(stream, token_ad)) {
			dprintf(D_ERROR, "Error sending token ad for %s command\n", cmd_name);
			return FALSE;
		}
	}
	return send_summary(stream, summary, cmd_name);
}

int
PlacementDaemon::command_query_authorizations(int cmd, Stream* stream)
{
	PlacementLockRelease release(m_db.Statements());
	const char * cmd_name = getCommandStringSafe(cmd);
	ClassAd cmd_ad;
	std::string username;

	dprintf( D_FULLDEBUG, "In command_query_authorizations\n" );

	if (!recv_request(stream, cmd_ad, cmd_name)) {
		return FALSE;
	}

	cmd_ad.LookupString("Username", username);

	ClassAd summary = summary_ad();

	std::vector<std::string> user_authz;
	std::vector<PlacementLegacyAuthz> authz;
	bool user_found = false;
	try {
		if (!username.empty()) {
			PlacementLoginInfo info;
			user_found = m_db.LookupLogin(username, info);
			if (!user_found) {
				dprintf(D_FULLDEBUG, "User %s is not authorized\n",
					PlacementSanitize(username, PLACEMENTD_MAX_IDENTITY).c_str());
				m_db.ReleaseReadLocks(cmd_name);
				return send_error(stream, summary, cmd_name, PLACEMENT_ERR_NOT_AUTHORIZED, "User not authorized");
			}
			user_authz = info.grants.AuthorizationNames();
		}
		// Requestable authorizations are the ones that were described
		// (by the authorizations map file or an admin).
		m_db.ListRequestableAuthorizations(authz);
	} catch (const PlacementError& e) {
		m_db.ReleaseReadLocks(cmd_name);
		return send_db_error(stream, summary, cmd_name, e);
	}
	// Never hold a read lock while a client reads the reply.
	m_db.ReleaseReadLocks(cmd_name);

	ClassAd reply_ad;
	for (const auto& entry: authz) {
		if (user_found && !contains_anycase(user_authz, entry.name)) {
			continue;
		}
		reply_ad.Clear();
		reply_ad.Assign("Name", entry.name);
		reply_ad.Assign("Label", entry.label);
		reply_ad.Assign("Color", entry.color);
		reply_ad.Assign("Description", entry.description);
		if (!putClassAd(stream, reply_ad)) {
			dprintf(D_ERROR, "Error sending auth ad for %s command\n", cmd_name);
			return FALSE;
		}
	}

	return send_summary(stream, summary, cmd_name);
}

int
PlacementDaemon::command_query(int cmd, Stream* stream)
{
	PlacementLockRelease release(m_db.Statements());
	const char * cmd_name = getCommandStringSafe(cmd);
	ClassAd cmd_ad;

	dprintf( D_FULLDEBUG, "In command_query\n" );

	if (!recv_request(stream, cmd_ad, cmd_name)) {
		return FALSE;
	}

	ClassAd summary = summary_ad();

	std::string target_type, uuid, name, projection;
	long long limit = -1;
	cmd_ad.LookupString(ATTR_TARGET_TYPE, target_type);
	cmd_ad.LookupString("Id", uuid);
	cmd_ad.LookupString("Projection", projection);
	lower_case(uuid);
	classad::ExprTree* requirements = cmd_ad.Lookup(ATTR_REQUIREMENTS);
	if (cmd_ad.Lookup("Limit") && (!cmd_ad.LookupInteger("Limit", limit) || limit < 0)) {
		return send_error(stream, summary, cmd_name, PLACEMENT_ERR_INVALID, "Limit must be a non-negative integer");
	}

	std::vector<ClassAd> ads;
	PlacementEntity entity = PlacementEntity::Identity;
	if (target_type.empty()) {
		return send_error(stream, summary, cmd_name, PLACEMENT_ERR_INVALID, "Missing TargetType");
	} else if (strcasecmp(target_type.c_str(), "MapfileStatus") == 0) {
		ClassAd ad;
		ad.Assign(ATTR_MY_TYPE, "PlacementMapfileStatus");
		m_reconciler.Status().PublishProblems(ad);
		ad.Assign("UserMapfile", m_mapFile);
		ad.Assign("AuthorizationsMapfile", m_authzMapFile);
		ads.emplace_back(std::move(ad));
	} else if (strcasecmp(target_type.c_str(), "Modification") == 0) {
		// The audit log, newest first.  With Requirements the limit
		// applies after filtering, below.
		std::string entity_id;
		cmd_ad.LookupString("EntityId", entity_id);
		lower_case(entity_id);
		try {
			m_db.ModificationAds(entity_id, requirements ? -1 : limit, ads);
		} catch (const PlacementError& e) {
			m_db.ReleaseReadLocks(cmd_name);
			return send_db_error(stream, summary, cmd_name, e);
		}
	} else if (PlacementEntityFromName(target_type, entity)) {
		// The natural key is Identity for an Identity, Name otherwise.
		if (entity == PlacementEntity::Identity && cmd_ad.Lookup(ATTR_NAME)) {
			return send_error(stream, summary, cmd_name, PLACEMENT_ERR_INVALID, "An Identity has no Name; use Identity");
		}
		cmd_ad.LookupString(PlacementEntityKeyAttr(entity), name);
		try {
			if (uuid.empty() && !name.empty() && entity == PlacementEntity::Account) {
				name = PlacementCanonicalAccount(name, "", m_uidDomain);
			}
			m_db.EntityAds(entity, uuid, name, ads);
		} catch (const PlacementError& e) {
			m_db.ReleaseReadLocks(cmd_name);
			return send_db_error(stream, summary, cmd_name, e);
		}
	} else {
		return send_error(stream, summary, cmd_name, PLACEMENT_ERR_UNKNOWN_TYPE, "Unknown TargetType " + target_type);
	}

	// Never hold a read lock while a client reads the reply.
	m_db.ReleaseReadLocks(cmd_name);

	// A projection always includes MyType and Id.
	std::vector<std::string> attrs;
	if (!projection.empty()) {
		attrs = split(projection, ",");
		attrs.emplace_back(ATTR_MY_TYPE);
		attrs.emplace_back("Id");
	}
	long long sent = 0;
	for (auto& ad: ads) {
		if (limit >= 0 && sent >= limit) {
			break;
		}
		if (requirements && !EvalExprBool(&ad, requirements)) {
			continue;
		}
		ClassAd projected;
		ClassAd* out = &ad;
		if (!attrs.empty()) {
			for (const auto& attr: attrs) {
				if (auto* expr = ad.Lookup(attr)) {
					projected.Insert(attr, expr->Copy());
				}
			}
			out = &projected;
		}
		if (!putClassAd(stream, *out)) {
			dprintf(D_ERROR, "Error sending result ad for %s command\n", cmd_name);
			return FALSE;
		}
		sent++;
	}

	return send_summary(stream, summary, cmd_name);
}

int
PlacementDaemon::command_edit(int cmd, Stream* stream)
{
	PlacementLockRelease release(m_db.Statements());
	const char * cmd_name = getCommandStringSafe(cmd);
	ReliSock* rsock = (ReliSock*)stream;
	ClassAd header_ad;

	dprintf( D_FULLDEBUG, "In command_edit\n" );

	stream->decode();
	stream->timeout(15);

	ClassAd summary = summary_ad();

	if (!getClassAd(stream, header_ad)) {
		dprintf(D_ERROR, "Failed to receive header ad for %s command: aborting\n", cmd_name);
		return FALSE;
	}
	long long num_ops = 0;
	header_ad.LookupInteger("NumOperations", num_ops);
	if (num_ops < 1 || num_ops > PLACEMENTD_MAX_EDIT_OPS) {
		// Do not try to read what the client claims to have sent.
		stream->encode();
		std::string msg;
		formatstr(msg, "NumOperations must be between 1 and %d", PLACEMENTD_MAX_EDIT_OPS);
		return send_error(stream, summary, cmd_name, PLACEMENT_ERR_INVALID, msg);
	}
	std::vector<ClassAd> ops((size_t)num_ops);
	for (auto& op: ops) {
		if (!getClassAd(stream, op)) {
			dprintf(D_ERROR, "Failed to receive operation ad for %s command: aborting\n", cmd_name);
			return FALSE;
		}
	}
	if (!stream->end_of_message()) {
		dprintf(D_ERROR, "Failed to receive EOM: for %s command: aborting\n", cmd_name);
		return FALSE;
	}
	stream->encode();

	if (!rsock->get_encryption()) {
		return send_error(stream, summary, cmd_name, PLACEMENT_ERR_NOT_ENCRYPTED, "Request to server was not encrypted.");
	}

	// A DryRun that is not a boolean must not turn into a real edit.
	bool dry_run = false;
	if (header_ad.Lookup("DryRun") && !header_ad.LookupBool("DryRun", dry_run)) {
		return send_error(stream, summary, cmd_name, PLACEMENT_ERR_INVALID, "DryRun must be a boolean");
	}

	PlacementEditContext ctx;
	const char* fqu = rsock->getFullyQualifiedUser();
	ctx.peer = fqu ? fqu : "unknown";
	if (const char* peer_address = rsock->peer_description()) {
		ctx.peer_address = peer_address;
	}
	// Recorded, never used to authorize: the caller's own authentication
	// decided that.
	std::string text;
	if (header_ad.LookupString("OnBehalfOf", text)) {
		text = PlacementSanitize(text, PLACEMENTD_MAX_ON_BEHALF_OF);
		if (!text.empty()) {
			ctx.on_behalf_of = text;
		}
	}
	if (header_ad.LookupString("Comment", text)) {
		text = PlacementSanitize(text, PLACEMENTD_MAX_COMMENT);
		if (!text.empty()) {
			ctx.comment = text;
		}
	}
	ctx.uid_domain = m_uidDomain;
	ctx.keys = &m_reconciler.Keys();
	ctx.dry_run = dry_run;

	PlacementEditOutcome outcome;
	PlacementRunEdit(m_db, ops, ctx, outcome);
	m_db.ReleaseReadLocks(cmd_name);

	if (outcome.committed) {
		for (const auto& line: outcome.audit) {
			dprintf(D_AUDIT, *rsock, "%s (change set %lld%s%s)\n", line.c_str(), outcome.changeset_id,
				ctx.on_behalf_of ? ", on behalf of " : "", ctx.on_behalf_of ? ctx.on_behalf_of->c_str() : "");
		}
		// Do a full pass next time, so that a key adopted while it is still
		// in the map file is reclaimed without waiting for the file to change.
		m_reconciler.Invalidate();
	} else if (outcome.error_code) {
		dprintf(D_FULLDEBUG, "%s from %s rolled back: %s\n", cmd_name, ctx.peer.c_str(),
			PlacementSanitize(outcome.error_string, 1024).c_str());
	}

	for (auto& result: outcome.results) {
		if (!putClassAd(stream, result)) {
			dprintf(D_ERROR, "Error sending edit result for %s command\n", cmd_name);
			return FALSE;
		}
	}
	if (outcome.error_code) {
		summary.Assign(ATTR_ERROR_CODE, outcome.error_code);
		summary.Assign(ATTR_ERROR_STRING, outcome.error_string);
	}
	return send_summary(stream, summary, cmd_name);
}
