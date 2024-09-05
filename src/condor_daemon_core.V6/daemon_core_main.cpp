/***************************************************************
 *
 * Copyright (C) 1990-2011, Condor Team, Computer Sciences Department,
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
#include "condor_open.h"
#include "condor_config.h"
#include "util_lib_proto.h"
#include "basename.h"
#include "ipv6_hostname.h"
#include "condor_version.h"
#include "limit.h"
#include "condor_email.h"
#include "sig_install.h"
#include "daemon.h"
#include "condor_debug.h"
#include "condor_distribution.h"
#include "condor_environ.h"
#include "setenv.h"
#include "time_offset.h"
#include "subsystem_info.h"
#include "file_lock.h"
#include "directory.h"
#include "exit.h"
#include "match_prefix.h"
#include "historyFileFinder.h"
#include "store_cred.h"
#include "condor_netaddr.h"
#include "dc_collector.h"
#include "token_utils.h"
#include "condor_scitokens.h"

#include <chrono>
#include <algorithm>

#ifdef LINUX
#include <sys/prctl.h>
#endif

#if HAVE_BACKTRACE
#include <execinfo.h>
#endif

#include "condor_auth_passwd.h"
#include "condor_auth_ssl.h"
#include "authentication.h"

#define _NO_EXTERN_DAEMON_CORE 1	
#include "condor_daemon_core.h"
#include "classad/classadCache.h"

#ifdef WIN32
#include "exception_handling.WINDOWS.h"
#endif

#if defined(HAVE_EXT_SCITOKENS)
#include <scitokens/scitokens.h>
#endif

// Externs to Globals
extern DLL_IMPORT_MAGIC char **environ;


// External protos
void (*dc_main_init)(int argc, char *argv[]) = NULL;	// old main
void (*dc_main_config)() = NULL;
void (*dc_main_shutdown_fast)() = NULL;
void (*dc_main_shutdown_graceful)() = NULL;
void (*dc_main_pre_dc_init)(int argc, char *argv[]) = NULL;
void (*dc_main_pre_command_sock_init)() = NULL;

// Internal protos
void dc_reconfig();
void dc_config_auth();       // Configuring GSI (and maybe other) authentication related stuff
int handle_fetch_log_history(ReliSock *s, char *name);
int handle_fetch_log_history_dir(ReliSock *s, char *name);
int handle_fetch_log_history_purge(ReliSock *s);

// Globals
int		Foreground = 1;		// run in foreground by default (condor_master will change this before calling dc_main)
char *_condor_myServiceName;		// name of service on Win32 (argv[0] from SCM)
DaemonCore*	daemonCore;

// Statics (e.g. global only to this file)
static	char*	myFullName;		// set to the full path to ourselves
static const char*	myName;			// set to basename(argv[0])
static	char*	logDir = NULL;
static	char*	pidFile = NULL;
static	char*	addrFile[2] = { NULL, NULL };
static	char*	logAppend = NULL;
static	char*	log2Arg = nullptr;

static int Termlog = 0;	//Replacing the Termlog in dprintf for daemons that use it

static char *core_dir = NULL;
static char *core_name = NULL;

int condor_main_argc;
char **condor_main_argv;
time_t daemon_stop_time;

#ifdef WIN32
int line_where_service_stopped = 0;
#endif
bool	DynamicDirs = false;
bool disable_default_log = false;

// runfor is non-zero if the -r command-line option is specified. 
// It is specified to tell a daemon to kill itself after runfor minutes.
// Currently, it is only used for the master, and it is specified for 
// glide-in jobs that know that they can only run for a certain amount
// of time. Glide-in will tell the master to kill itself a minute before 
// it needs to quit, and it will kill the other daemons. 
// 
// The master will check this global variable and set an environment
// variable to inform the children how much time they have left to run. 
// This will mainly be used by the startd, which will advertise how much
// time it has left before it exits. This can be used by a job's requirements
// so that it can decide if it should run at a particular machine or not. 
int runfor = 0; //allow cmd line option to exit after *runfor* minutes

// This flag tells daemoncore whether to do the core limit initialization.
// It can be set to false by calling the DC_Skip_Core_Init() function.
static bool doCoreInit = true;


	// Right now, the default ID is simply the empty string.
const std::string DCTokenRequester::default_identity = "";

namespace {

class TokenRequest : public Service {
public:

	enum class State {
		Pending,
		Successful,
		Failed,
		Expired
	};

	TokenRequest(const std::string &requester_identity,
			const std::string &requested_identity,
			const std::string &peer_location,
			const std::vector<std::string> &authz_bounding_set,
			int lifetime, const std::string &client_id,
			const std::string &request_id)
	:
		m_lifetime(lifetime),
		m_requested_identity(requested_identity),
		m_requester_identity(requester_identity),
		m_peer_location(peer_location),
		m_authz_bounding_set(authz_bounding_set),
		m_client_id(client_id),
		m_request_id(request_id)
	{
		m_request_time = time(NULL);
	}

	std::string getPublicString() const {
		std::string bounding_set_info = "<none>";
		if (!m_authz_bounding_set.empty()) {
			bounding_set_info = join(m_authz_bounding_set, ",");
		}
		std::string public_str = 
			"[requested_id = " +
		   	m_requested_identity +
			"; requester_id = " +
		   	m_requester_identity +
			"; peer_location = " +
		   	m_peer_location +
			"; m_authz_bounding_set = " +
		   	bounding_set_info +
			"]";
		return public_str;
	}

	void setToken(const std::string &token) {
		m_token = token;
			// Ensure the token expires 60s after it is made.
		m_lifetime = time(NULL) - m_request_time + 60;
		m_state = State::Successful;
	}

	void setFailed() {m_state = State::Failed;}

	void setExpired() {
		if (m_state == State::Pending) {
			m_state = State::Expired;
		}
	}

	const std::string &getToken() const {return m_token;}

	const std::string &getClientId() const {return m_client_id;}

	State getState() const {return m_state;}

	bool isExpiredAt(time_t now, time_t max_lifetime) const {return m_request_time + max_lifetime < now;}

	const std::vector<std::string> &getBoundingSet() const {return m_authz_bounding_set;}

	time_t getLifetime() const {return m_lifetime;}

	const std::string &getRequestedIdentity() const {return m_requested_identity;}

	const std::string &getRequesterIdentity() const {return m_requester_identity;}

	const std::string &getPeerLocation() const {return m_peer_location;}

	const std::string &getRequestId() const {return m_request_id;}

	bool static ShouldAutoApprove(const TokenRequest &token_request, time_t now,
		std::string &rule_text)
	{

			// Only auto-approve requests for the condor identity.
		if (strncmp(token_request.getRequestedIdentity().c_str(), "condor@", 7)) {
			return false;
		}
			// All auto-approve requests must have a bounding set to limit auth.
		if (!token_request.getBoundingSet().size()) {
			return false;
		}
			// Only auto-approve requests from daemons that automatically ask for one.
		for (const auto &authz : token_request.getBoundingSet()) {
			if ((authz != "ADVERTISE_SCHEDD") && (authz != "ADVERTISE_STARTD") && (authz != "ADVERTISE_MASTER")) {
				return false;
			}
		}
			// Only auto-approve requests that aren't pending.
		if (token_request.getState() != State::Pending) {
			dprintf(D_SECURITY|D_FULLDEBUG, "Cannot auto-approve request"
				" because it is pending.\n");
			return false;
		}
			// Only auto-approve requests that haven't expired.
		if (now > (token_request.m_request_time + \
				(token_request.m_lifetime < 0 ?
				86400 * 365 : token_request.m_lifetime))) {
			dprintf(D_SECURITY|D_FULLDEBUG, "Cannot auto-approve request"
				" because it is expired (token was requested at %ld"
				"; lifetime is %ld; now is %ld).\n",
				token_request.m_request_time,
				token_request.m_lifetime, now);
			return false;
		}

		const auto &peer_location = token_request.getPeerLocation();
		dprintf(D_FULLDEBUG|D_SECURITY, "Evaluating request against %zu rules.\n", m_approval_rules.size());
		for (auto &rule : m_approval_rules) {
			if (!matches_withnetwork(rule.m_approval_netblock,
					peer_location.c_str())) {
				dprintf(D_FULLDEBUG|D_SECURITY, "Cannot auto-approve request;"
					" peer %s does not match netblock %s.\n",
					peer_location.c_str(),
					rule.m_approval_netblock.c_str());
				continue;
			}
			if (token_request.m_request_time > rule.m_expiry_time) {
				dprintf(D_SECURITY|D_FULLDEBUG, "Cannot auto-approve request"
					" because request time (%ld) is after rule expiration (%ld).\n",
					token_request.m_request_time, rule.m_expiry_time);
				continue;
			}
				// Approve requests that came in up to 60s before the
				// auto-approval rule was installed.
			if (token_request.m_request_time < rule.m_issue_time - 60) {
				dprintf(D_SECURITY|D_FULLDEBUG, "Cannot auto-approve request"
					" because it is too old");
				continue;
			}
			formatstr(rule_text, "[netblock = %s; lifetime_left = %ld]", rule.m_approval_netblock.c_str(),  rule.m_expiry_time - now);
			return true;
		}
		return false;
	}

	static bool addApprovalRule(std::string netblock, time_t lifetime, CondorError & error ) {
		if( lifetime <= 0 ) {
			error.push( "DAEMON", -1, "Auto-approval rule lifetimes must be greater than zero." );
			return false;
		}

		condor_netaddr na;
		if(! na.from_net_string(netblock.c_str())) {
			error.push( "DAEMON", -2, "Auto-approval rule netblock invalid." );
			return false;
		}

		m_approval_rules.emplace_back();
		auto &rule = m_approval_rules.back();
		rule.m_approval_netblock = netblock;
		rule.m_issue_time = time(NULL);
		rule.m_expiry_time = rule.m_issue_time + lifetime;
		return true;
	}

	static void clearApprovalRules() {m_approval_rules.clear();}

	static void cleanupApprovalRules() {
		auto now = time(NULL);
		m_approval_rules.erase(
			std::remove_if(m_approval_rules.begin(), m_approval_rules.end(),
				[=](const ApprovalRule &rule) {return rule.m_expiry_time < now;}),
			m_approval_rules.end()
		);
	}

		// Callbacks for collector updates; determine whether a token request would be useful!
	static void
	daemonUpdateCallback(bool success, Sock *sock, CondorError *, const std::string &trust_domain, bool should_try_token_request, void *miscdata) {

			// Put our void *miscdata into a managed unique_ptr so it is deallocated on return
		auto data_ptr = reinterpret_cast<DCTokenRequester::DCTokenRequesterData*>(miscdata);
		if (!data_ptr) {
			return;
		}
		std::unique_ptr<DCTokenRequester::DCTokenRequesterData> data(data_ptr);

			// No need request a token if we already successfully updated the
			// collector, or if no request tokens attempts desired...
		if (success || !should_try_token_request || !sock) return;

			// Avoiding requesting tokens for already-pending requests.
		for (const auto &pending_request : m_token_requests) {
			if (pending_request.m_identity != data->m_identity) {
				continue;
			}
			if (pending_request.m_trust_domain == trust_domain) {
				return;
			}
		}

		dprintf(D_ALWAYS, "Collector update failed; will try to get a token request for trust domain %s"
			", identity %s.\n", trust_domain.c_str(),
			data->m_identity == DCTokenRequester::default_identity ?
				"(default)" : data->m_identity.c_str());
		m_token_requests.emplace_back();
		auto &back = m_token_requests.back();
		back.m_identity = data->m_identity;
		back.m_trust_domain = trust_domain;
		back.m_authz_name = data->m_authz_name;
		back.m_daemon.reset(new DCCollector(data->m_addr.c_str()));
		back.m_daemon->setOwner(data->m_identity);
			// Unprivileged requests can only use SSL or TOKEN.
		if (data->m_identity != DCTokenRequester::default_identity) {
			back.m_daemon->setAuthenticationMethods({"SSL", "TOKEN"});
		}
		back.m_callback_fn = &DCTokenRequester::tokenRequestCallback;
		// At this point, the m_callback_data pointer will be deallocated by
		// tokenRequestCallback(), so do a release() here so it is not
		// deallocated when data goes out of scope.
		back.m_callback_data = data.release();

		if (m_token_requests_tid == -1) {
			m_token_requests_tid = daemonCore->Register_Timer( 0,
			(TimerHandler)&TokenRequest::tryTokenRequests,
			"TokenRequest::tryTokenRequests" );
		}

	}

	static void
	clearTokenRequests() {m_token_requests.clear();}

private:
	struct PendingRequest {
			// Request ID sent to the remote server.
		std::string m_request_id;
			// My auto-generated client ID; an empty
			// client ID indicates the token request has not
			// been sent to the remote daemon yet.
		std::string m_client_id;
			// Identity requested for this token
		std::string m_identity;
		std::string m_trust_domain;
			// Authorization level name
		std::string m_authz_name;
		std::unique_ptr<Daemon> m_daemon{nullptr};
		RequestCallbackFn *m_callback_fn{nullptr};
			// Callback data must be guaranteed to outlive the request;
			// borrowed reference.
		void *m_callback_data{nullptr};
	};

	static void
	tryTokenRequests() {
		bool should_reschedule = false;
		dprintf(D_SECURITY|D_FULLDEBUG, "There are %zu token requests remaining.\n",
		m_token_requests.size());
		for (auto & request : m_token_requests)
		{
			should_reschedule |= tryTokenRequest(request);
		}
		if (should_reschedule) {
			daemonCore->Reset_Timer(m_token_requests_tid, 5, 1);
			dprintf(D_SECURITY|D_FULLDEBUG, "Will reschedule another poll of requests.\n");
		} else {
			daemonCore->Cancel_Timer(m_token_requests_tid);
			m_token_requests_tid = -1;
		}
		m_token_requests.erase(
				std::remove_if(
					m_token_requests.begin(),
					m_token_requests.end(),
					[](const PendingRequest &req) {return req.m_client_id.empty();}),
				m_token_requests.end()
				);
	};

	static bool
	tryTokenRequest(PendingRequest &req) {

		/* NOTE: upon leaving this function, in order to avoid leaking memory,
		 * we MUST either have a) set req.m_client_id, or b) invoked the callback and 
		 * cleared the req.m_client_id. 
		 */

		std::string subsys_name = get_mySubSystemName();

		dprintf(D_SECURITY, "Trying token request to remote host %s for user %s.\n",
			req.m_daemon->name() ? req.m_daemon->name() : req.m_daemon->addr(),
			req.m_identity == DCTokenRequester::default_identity ? "(default)" : req.m_identity.c_str());
		if (!req.m_daemon) {
			dprintf(D_ERROR, "Logic error!  Token request without associated daemon.\n");
			req.m_client_id = "";
			(*req.m_callback_fn)(false, req.m_callback_data);
			return false;
		}
		std::string token;
		if (req.m_client_id.empty()) {
			req.m_request_id = "";
			req.m_client_id = htcondor::generate_client_id();

			std::string request_id;
			std::vector<std::string> authz_list;
			authz_list.push_back(req.m_authz_name);
			int lifetime = -1;
			CondorError err;
			if (!req.m_daemon->startTokenRequest(req.m_identity, authz_list, lifetime,
				req.m_client_id, token, request_id, &err))
			{
				dprintf(D_ALWAYS, "Failed to request a new token: %s\n", err.getFullText().c_str());
				req.m_client_id = "";
				(*req.m_callback_fn)(false, req.m_callback_data);
				return false;
			}
			if (token.empty()) {
				req.m_request_id = request_id;
				dprintf(D_ALWAYS, "Token requested; please ask collector %s admin to approve request ID %s.\n", req.m_daemon->name(), request_id.c_str());
				// m_client_id is set here, so safe to return without calling the callback
				return true;
			} else {
				dprintf(D_ALWAYS, "Token request auto-approved.\n");
				Condor_Auth_Passwd::retry_token_search();
				daemonCore->getSecMan()->reconfig();
				(*req.m_callback_fn)(true, req.m_callback_data);
				req.m_client_id = "";
			}
		} else {
			CondorError err;
			if (!req.m_daemon->finishTokenRequest(req.m_client_id, req.m_request_id, token, &err)) {
				dprintf(D_ALWAYS, "Failed to retrieve a new token: %s\n", err.getFullText().c_str());
				req.m_client_id = "";
				(*req.m_callback_fn)(false, req.m_callback_data);
				return false;
			}
			if (token.empty()) {
				dprintf(D_FULLDEBUG|D_SECURITY, "Token request not approved; will retry in 5 seconds.\n");
				dprintf(D_ALWAYS, "Token requested not yet approved; please ask collector %s admin to approve request ID %s.\n", req.m_daemon->name(), req.m_request_id.c_str());
				// m_client_id is set here, so safe to return without calling the callback
				return true;
			} else {
				dprintf(D_ALWAYS, "Token request approved.\n");
					// Flush the cached result of the token search.
				Condor_Auth_Passwd::retry_token_search();
					// Don't flush any security sessions. It's highly unlikely
					// there are any relevant ones, and flushing the
					// non-negotiated sessions will cause problems
					// (like crashing when we want to use the family session).
				#if 0
					// Flush out the security sessions; will need to force a re-auth.
				auto sec_man = daemonCore->getSecMan();
				sec_man->reconfig();
				if (!req.m_identity.empty()) {
					std::string orig_tag = sec_man->getTag();
					sec_man->setTag(req.m_identity);
					sec_man->invalidateAllCache();
					sec_man->setTag(orig_tag);
				} else {
					sec_man->invalidateAllCache();
				}
				#endif
					// Invoke the daemon-provided callback to do daemon-specific cleanups.
				(*req.m_callback_fn)(true, req.m_callback_data);
			}
			req.m_client_id = "";
		}
		if (!token.empty()) {
			htcondor::write_out_token(subsys_name + "_auto_generated_token", token, req.m_identity);
		}
		return false;
	}

	// The initial state of a request should always be pending.
	State m_state{State::Pending};

	// Initialize these to known-bogus values out of an abundance of caution.
	time_t m_request_time{-1};
	time_t m_lifetime{-1};

	std::string m_requested_identity;
	std::string m_requester_identity;
	std::string m_peer_location;
	std::vector<std::string> m_authz_bounding_set;
	std::string m_client_id;
        std::string m_request_id;
	std::string m_token;

	struct ApprovalRule
	{
		std::string m_approval_netblock;
		time_t m_issue_time;
		time_t m_expiry_time;
	};

	static std::vector<ApprovalRule> m_approval_rules;

	static std::vector<PendingRequest> m_token_requests;
	static int m_token_requests_tid;
};


int TokenRequest::m_token_requests_tid{-1};


std::vector<TokenRequest::ApprovalRule> TokenRequest::m_approval_rules;

std::vector<TokenRequest::PendingRequest> TokenRequest::m_token_requests;

typedef std::unordered_map<int, std::unique_ptr<TokenRequest>> TokenMap;

TokenMap g_request_map;

class RequestRateLimiter {
public:
	RequestRateLimiter(unsigned rate_limit)
	: m_rate_limit(rate_limit),
	m_last_update(std::chrono::steady_clock::now())
	{
		std::shared_ptr<stats_ema_config> ema_config(new stats_ema_config);
		ema_config->add(10, "10s");
		m_request_rate.ConfigureEMAHorizons(ema_config);
		m_request_rate.recent_start_time = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
		m_request_rate.Update(m_request_rate.recent_start_time);
	}

	RequestRateLimiter(const RequestRateLimiter &) = delete;

	bool AllowIncomingRequest() {
		auto now = std::chrono::steady_clock::now();
		m_request_rate += 1;
		auto duration = m_last_update - now;
		auto seconds_elapsed = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
		if (seconds_elapsed > 0) {
			auto since_epoch = now.time_since_epoch();
			m_request_rate.Update(std::chrono::duration_cast<std::chrono::seconds>(since_epoch).count());
			m_recent_rate = m_request_rate.EMAValue("10s");
			m_last_update = now;
		}
		return (m_rate_limit <= 0) || (m_recent_rate <= m_rate_limit);
	}

	void SetLimit(unsigned rate_limit) {m_rate_limit = rate_limit;}

private:

	double m_rate_limit{0};
	double m_recent_rate{0};
	std::chrono::steady_clock::time_point m_last_update;
	stats_entry_sum_ema_rate<uint64_t> m_request_rate;
};

	// We allow 60 seconds for the remote side to pick up the result.
	// During that time, an attacker can try to brute-force the request ID.
	// This is a variant of the birthday paradox: what's the likelihood of
	// N random selections out of D options being the same as your selection?
	// Turns out, the answer is 1 - ((D-1)/D) ^ N.  Setting D = 9999999 and
	// N = 60*10, there is a 0.006% chance of a determined attacker guessing
	// a 7-digit number
	RequestRateLimiter g_request_limit(10);

	void cleanup_request_map(int /* tid */) {
	std::vector<int> requests_to_delete;
	auto now = time(NULL);
	auto lifetime = param_integer("SEC_TOKEN_REQUEST_LIFETIME", 3600);
	auto max_lifetime = lifetime + 3600;

	for (auto &entry : g_request_map) {
		if (entry.second->isExpiredAt(now, lifetime)) {
			entry.second->setExpired();
			dprintf(D_SECURITY|D_FULLDEBUG, "Request %d has expired.\n", entry.first);
		}
		if (entry.second->isExpiredAt(now, max_lifetime)) {
			requests_to_delete.push_back(entry.first);
		}
	}
	for (auto request : requests_to_delete) {
		dprintf(D_SECURITY|D_FULLDEBUG, "Cleaning up request %d.\n", request);
		auto iter = g_request_map.find(request);
		if (iter != g_request_map.end()) {
			g_request_map.erase(iter);
		}
	}

	TokenRequest::cleanupApprovalRules();
}

}


void*
DCTokenRequester::createCallbackData(const std::string &daemon_addr, const std::string &identity,
	const std::string &authz_name)
{
	DCTokenRequesterData *data = new DCTokenRequesterData();
	data->m_addr = daemon_addr;
	data->m_identity = identity;
	data->m_authz_name = authz_name;
	data->m_callback_fn = m_callback;
	data->m_callback_data = m_callback_data;

	return data;
}


void
DCTokenRequester::tokenRequestCallback(bool success, void *miscdata)
{
	auto data = reinterpret_cast<DCTokenRequester::DCTokenRequesterData *>(miscdata);
	std::unique_ptr<DCTokenRequester::DCTokenRequesterData> data_uptr(data);

	(*data->m_callback_fn)(success, data->m_callback_data);

	// Note: miscdata is being deleted now, as data_uptr is going out of scope...
}


void
DCTokenRequester::daemonUpdateCallback(bool success, Sock *sock, CondorError *errstack, const std::string &trust_domain,
                bool should_try_token_request, void *miscdata)
{
	TokenRequest::daemonUpdateCallback(success, sock, errstack, trust_domain, should_try_token_request, miscdata);
}


#ifndef WIN32
// This function polls our parent process; if it is gone, shutdown.
void
check_parent(int /* tid */)
{
	if ( daemonCore->Is_Pid_Alive( daemonCore->getppid() ) == FALSE ) {
		// our parent is gone!
		dprintf(D_ALWAYS,
			"Our parent process (pid %d) went away; shutting down fast\n",
			daemonCore->getppid());
		daemonCore->Signal_Myself(SIGQUIT); // SIGQUIT means shutdown fast
	}
}
#endif

// This function clears expired sessions from the cache
void
check_session_cache(int /* tid */)
{
	daemonCore->getSecMan()->invalidateExpiredCache();
}

bool global_dc_set_cookie(int len, unsigned char* data) {
	if (daemonCore) {
		return daemonCore->set_cookie(len, data);
	} else {
		return false;
	}
}

bool global_dc_get_cookie(int &len, unsigned char* &data) {
	if (daemonCore) {
		return daemonCore->get_cookie(len, data);
	} else {
		return false;
	}
}

void
handle_cookie_refresh(int /* tid */)
{
	unsigned char randomjunk[256];
	char symbols[16] = { '0', '1', '2', '3', '4', '5', '6', '7',
		'8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
	for (int i = 0; i < 128; i++) {
		randomjunk[i] = symbols[rand() % 16];
	}
	
	// good ol null terminator
	randomjunk[127] = 0;

	global_dc_set_cookie (128, randomjunk);	
}

char const* global_dc_sinful() {
	if (daemonCore) {
		return daemonCore->InfoCommandSinfulString();
	} else {
		return NULL;
	}
}

void clean_files()
{
		// If we created a pid file, remove it.
	if( pidFile ) {
		if( unlink(pidFile) < 0 ) {
			dprintf( D_ALWAYS, 
					 "DaemonCore: ERROR: Can't delete pid file %s\n",
					 pidFile );
		} else {
			if( IsDebugVerbose( D_DAEMONCORE ) ) {
				dprintf( D_DAEMONCORE, "Removed pid file %s\n", pidFile );
			}
		}
	}

	for (int i=0; i<2; i++) {
		if( addrFile[i] ) {
			if( unlink(addrFile[i]) < 0 ) {
				dprintf( D_ALWAYS,
						 "DaemonCore: ERROR: Can't delete address file %s\n",
						 addrFile[i] );
			} else {
				if( IsDebugVerbose( D_DAEMONCORE ) ) {
					dprintf( D_DAEMONCORE, "Removed address file %s\n",
							 addrFile[i] );
				}
			}
				// Since we param()'ed for this, we need to free it now.
			free( addrFile[i] );
		}
	}
	
	if(daemonCore) {
		if( daemonCore->localAdFile ) {
			if( unlink(daemonCore->localAdFile) < 0 ) {
				dprintf( D_ALWAYS, 
						 "DaemonCore: ERROR: Can't delete classad file %s\n",
						 daemonCore->localAdFile );
			} else {
				if( IsDebugVerbose( D_DAEMONCORE ) ) {
					dprintf( D_DAEMONCORE, "Removed local classad file %s\n", 
							 daemonCore->localAdFile );
				}
			}
			free( daemonCore->localAdFile );
			daemonCore->localAdFile = NULL;
		}
	}

}


void
DaemonCore::kill_immediate_children() {
	// Disabled by default for everything but the schedd in stable.
	bool enabled = param_boolean( "DEFAULT_KILL_CHILDREN_ON_EXIT", true );

	std::string pname;
	formatstr( pname, "%s_KILL_CHILDREN_ON_EXIT", get_mySubSystem()->getName() );
	enabled = param_boolean( pname.c_str(), enabled );

	if(! enabled) { return; }


	//
	// Send each of our children a SIGKILL.  We'd rather kill each child's
	// whole process tree, but we don't want to block talking to the procd.
	//
	for (const auto& [key, pid_entry] : pidTable) {
		// Don't try to kill our parent process; it's a bad idea, Send_Signal()
		// will fail, and we don't want the attempt in the log.
		if( pid_entry.pid == ppid ) { continue; }
		// Don't try to kill processes which have already exited; this avoids
		// a race condition with PID reuse, logging an error in the attempt,
		// and looking like we're trying to kill the job after noticing it die.
		if (pid_entry.process_exited) { continue; }
		if (ProcessExitedButNotReaped(pid_entry.pid)) {
			dprintf(D_FULLDEBUG, "Daemon exiting before reaping child pid %d\n", pid_entry.pid);
			continue;
		}
		if (pid_entry.cleanup_signal == 0) {
			dprintf(D_FULLDEBUG, "Daemon not killing child pid %d at exit\n", pid_entry.pid);
			continue;
		}
		dprintf( D_ALWAYS, "Daemon exiting before all child processes gone; killing %d\n", pid_entry.pid );
		Send_Signal( pid_entry.pid, pid_entry.cleanup_signal );
	}
}


// All daemons call this function when they want daemonCore to really
// exit.  Put any daemon-wide shutdown code in here.   
void
DC_Exit( int status, const char *shutdown_program )
{
	if( daemonCore ) {
		daemonCore->kill_immediate_children();
	}

		// First, delete any files we might have created, like the
		// address file or the pid file.
	clean_files();

		// See if this daemon wants to be restarted (true by
		// default).  If so, use the given status.  Otherwise, use the
		// special code to tell our parent not to restart us.
	int exit_status;
	if (daemonCore == NULL || daemonCore->wantsRestart()) {
		exit_status = status;
	}
	else {
		exit_status = DAEMON_NO_RESTART;
	}

#ifndef WIN32
	// unregister our signal handlers in case some 3rd-party lib
	// was masking signals on us...no late arrivals
	install_sig_handler(SIGCHLD,SIG_DFL);
	install_sig_handler(SIGHUP,SIG_DFL);
	install_sig_handler(SIGTERM,SIG_DFL);
	install_sig_handler(SIGQUIT,SIG_DFL);
	install_sig_handler(SIGUSR1,SIG_DFL);
	install_sig_handler(SIGUSR2,SIG_DFL);
#endif /* ! WIN32 */

		// Now, delete the daemonCore object, since we allocated it. 
	unsigned long	pid = 0;
	if (daemonCore) {
		pid = daemonCore->getpid( );
		delete daemonCore;
		daemonCore = NULL;
	}

		// Free up the memory from the config hash table, too.
	clear_global_config_table();

		// and deallocate the memory from the passwd_cache (uids.C)
	delete_passwd_cache();

	if ( core_dir ) {
		free( core_dir );
		core_dir = NULL;
	}

	if (core_name) {
		free(core_name);
		core_name = NULL;
	}

		/*
		  Log a message.  We want to do this *AFTER* we delete the
		  daemonCore object and free up other memory, just to make
		  sure we don't hit an EXCEPT() or anything in there and end
		  up exiting with something else after we print this.  all the
		  dprintf() code has already initialized everything it needs
		  to know from the config file, so it's ok that we already
		  cleared out our config hashtable, too.  Derek 2004-11-23
		*/
	if ( shutdown_program ) {
#     if !defined(WIN32)
		dprintf( D_ALWAYS, "**** %s (%s_%s) pid %lu EXITING BY EXECING %s\n",
				 myName, MY_condor_NAME, get_mySubSystem()->getName(), pid,
				 shutdown_program );
		priv_state p = set_root_priv( );
		int exec_status = execl( shutdown_program, shutdown_program, NULL );
		set_priv( p );
		dprintf( D_ALWAYS, "**** execl() FAILED %d %d %s\n",
				 exec_status, errno, strerror(errno) );
#     else
		dprintf( D_ALWAYS,
				 "**** %s (%s_%s) pid %lu EXECING SHUTDOWN PROGRAM %s\n",
				 myName, MY_condor_NAME, get_mySubSystem()->getName(), pid,
				 shutdown_program );
		priv_state p = set_root_priv( );
		int exec_status = execl( shutdown_program, shutdown_program, NULL );
		set_priv( p );
		if ( exec_status ) {
			dprintf( D_ALWAYS, "**** _execl() FAILED %d %d %s\n",
					 exec_status, errno, strerror(errno) );
		}
#     endif
	}
	dprintf( D_ALWAYS, "**** %s (%s_%s) pid %lu EXITING WITH STATUS %d\n",
			 myName, MY_condor_NAME, get_mySubSystem()->getName(), pid,
			 exit_status );

	// Disable log rotation during process teardown (i.e. calling
	// destructors of global or static objects).
	dprintf_allow_log_rotation(false);

		// Finally, exit with the appropriate status.
	exit( exit_status );
}


void
DC_Skip_Core_Init()
{
	doCoreInit = false;
}

static void
kill_daemon_ad_file()
{
	std::string param_name;
	formatstr( param_name, "%s_DAEMON_AD_FILE", get_mySubSystem()->getName() );
	char *ad_file = param(param_name.c_str());
	if( !ad_file ) {
		return;
	}

	MSC_SUPPRESS_WARNING_FOREVER(6031) // return value of unlink ignored.
	unlink(ad_file);

	free(ad_file);
}

void
drop_addr_file()
{
	FILE	*ADDR_FILE;
	char	addr_file[100];
	const char* addr[2];

	// build up a prefix as LOCALNAME.SUBSYSTEM or just SUBSYSTEM if no localname
	// that way, daemons that have a localname will never stomp the address file of the
	// primary daemon of that subsys unless explicitly told to do so.
	std::string prefix(get_mySubSystem()->getLocalName(""));
	if ( ! prefix.empty()) { prefix += "."; }
	prefix += get_mySubSystem()->getName();

	// Fill in addrFile[0] and addr[0] with info about regular command port
	snprintf( addr_file, sizeof(addr_file), "%s_ADDRESS_FILE", prefix.c_str() );
	if( addrFile[0] ) {
		free( addrFile[0] );
	}
	addrFile[0] = param( addr_file );
		// Always prefer the local, private address if possible.
	addr[0] = daemonCore->privateNetworkIpAddr();
	if (!addr[0]) {
			// And if not, fall back to the public.
		addr[0] = daemonCore->publicNetworkIpAddr();
	}

	// Fill in addrFile[1] and addr[1] with info about superuser command port
	snprintf( addr_file, sizeof(addr_file), "%s_SUPER_ADDRESS_FILE", prefix.c_str() );
	if( addrFile[1] ) {
		free( addrFile[1] );
	}
	addrFile[1] = param( addr_file );
	addr[1] = daemonCore->superUserNetworkIpAddr();

	for (int i=0; i<2; i++) {
		if( addrFile[i] ) {
			std::string newAddrFile;
			formatstr(newAddrFile,"%s.new",addrFile[i]);
			if( (ADDR_FILE = safe_fopen_wrapper_follow(newAddrFile.c_str(), "w")) ) {
				fprintf( ADDR_FILE, "%s\n", addr[i] );
				fprintf( ADDR_FILE, "%s\n", CondorVersion() );
				fprintf( ADDR_FILE, "%s\n", CondorPlatform() );
				fclose( ADDR_FILE );
				if( rotate_file(newAddrFile.c_str(),addrFile[i])!=0 ) {
					dprintf( D_ALWAYS,
							 "DaemonCore: ERROR: failed to rotate %s to %s\n",
							 newAddrFile.c_str(),
							 addrFile[i]);
				}
			} else {
				dprintf( D_ALWAYS,
						 "DaemonCore: ERROR: Can't open address file %s\n",
						 newAddrFile.c_str() );
			}
		}
	}	// end of for loop
}

void
drop_pid_file() 
{
	FILE	*PID_FILE;

	if( !pidFile ) {
			// There's no file, just return
		return;
	}

	if( (PID_FILE = safe_fopen_wrapper_follow(pidFile, "w")) ) {
		fprintf( PID_FILE, "%lu\n", 
				 (unsigned long)daemonCore->getpid() ); 
		fclose( PID_FILE );
	} else {
		dprintf( D_ALWAYS,
				 "DaemonCore: ERROR: Can't open pid file %s\n",
				 pidFile );
	}
}


void
do_kill() 
{
#ifndef WIN32
	FILE	*PID_FILE;
	pid_t 	pid = 0;
	unsigned long tmp_ul_int = 0;

	if( !pidFile ) {
		fprintf( stderr, 
				 "DaemonCore: ERROR: no pidfile specified for -kill\n" );
		exit( 1 );
	}
	if( pidFile[0] != '/' ) {
			// There's no absolute path, prepend the LOG directory
		std::string log;
		if (param(log, "LOG")) {
			log += '/';
			log += pidFile;
			pidFile = strdup(log.c_str());
		}
	}
	if( (PID_FILE = safe_fopen_wrapper_follow(pidFile, "r")) ) {
		if (fscanf( PID_FILE, "%lu", &tmp_ul_int ) != 1) {
			fprintf( stderr, "DaemonCore: ERROR: fscanf failed processing pid file %s\n",
					 pidFile );
			exit( 1 );
		}
		pid = (pid_t)tmp_ul_int;
		fclose( PID_FILE );
	} else {
		fprintf( stderr, 
				 "DaemonCore: ERROR: Can't open pid file %s for reading\n",
				 pidFile );
		exit( 1 );
	}
	if( pid > 0 ) {
			// We have a valid pid, try to kill it.
		if( kill(pid, SIGTERM) < 0 ) {
			fprintf( stderr, 
					 "DaemonCore: ERROR: can't send SIGTERM to pid (%lu)\n",  
					 (unsigned long)pid );
			fprintf( stderr, 
					 "\terrno: %d (%s)\n", errno, strerror(errno) );
			exit( 1 );
		} 
			// kill worked, now, make sure the thing is gone.  Keep
			// trying to send signal 0 (test signal) to the process
			// until that fails.  
		while( kill(pid, 0) == 0 ) {
			sleep( 3 );
		}
			// Everything's cool, exit normally.
		exit( 0 );
	} else {  	// Invalid pid
		fprintf( stderr, 
				 "DaemonCore: ERROR: pid (%lu) in pid file (%s) is invalid.\n",
				 (unsigned long)pid, pidFile );	
		exit( 1 );
	}
#endif  // of ifndef WIN32
}


// Create the directory we were given, with extra error checking.  
void
make_dir( const char* logdir )
{
	mode_t mode = S_IRWXU | S_IRWXG | S_IRWXO;
	struct stat stats;
	if( stat(logdir, &stats) >= 0 ) {
		if( ! S_ISDIR(stats.st_mode) ) {
			fprintf( stderr, 
					 "DaemonCore: ERROR: %s exists and is not a directory.\n", 
					 logdir );
			exit( 1 );
		}
	} else {
		if( mkdir(logdir, mode) < 0 ) {
			fprintf( stderr, 
					 "DaemonCore: ERROR: can't create directory %s\n", 
					 logdir );
			fprintf( stderr, 
					 "\terrno: %d (%s)\n", errno, strerror(errno) );
			exit( 1 );
		}
	}
}


// Set our log directory in our config hash table, and create it.
void
set_log_dir()
{
	if( !logDir ) {
		return;
	}
	config_insert( "LOG", logDir );
	make_dir( logDir );
}


void
handle_log_append( char* append_str )
{
	if( ! append_str ) {
		return;
	}
	std::string fname;
	char buf[100];
	snprintf( buf, sizeof(buf), "%s_LOG", get_mySubSystem()->getName() );
	if( !param(fname, buf) ) {
		EXCEPT( "%s not defined!", buf );
	}
	fname += '.';
	fname += append_str;
	config_insert( buf, fname.c_str() );

	if (get_mySubSystem()->getLocalName()) {
		std::string fullParamName;
		fullParamName.append(get_mySubSystem()->getLocalName());
		fullParamName.append(".");
		fullParamName.append(get_mySubSystem()->getName());
		fullParamName.append("_LOG");

		config_insert( fullParamName.c_str(), fname.c_str() );
	}
}


void
dc_touch_log_file(int /* tid */)
{
	dprintf_touch_log();

	daemonCore->Register_Timer( param_integer( "TOUCH_LOG_INTERVAL", 60 ),
				dc_touch_log_file, "dc_touch_log_file" );
}

void
dc_touch_lock_files(int /* tid */)
{
	priv_state p;

	// For any outstanding lock objects that are associated with the real lock
	// file, update their timestamps. Do it as Condor, and ignore files that we
	// don't have permissions to alter. This allows things like the Instance
	// lock to be updated, but doesn't update things like the job event log.

	// do this here to make the priv switching operations fast in the 
	// FileLock Object.
	p = set_condor_priv();

	FileLock::updateAllLockTimestamps();

	set_priv(p);

	// reset the timer for next incarnation of the update.
	daemonCore->Register_Timer(
		param_integer("LOCK_FILE_UPDATE_INTERVAL", 3600 * 8, 60, INT_MAX),
		dc_touch_lock_files, "dc_touch_lock_files" );
}


void
set_dynamic_dir( const char* param_name, const char* append_str )
{
	std::string val;
	std::string newdir;

	if( !param( val, param_name ) ) {
			// nothing to do
		return;
	}

		// First, create the new name.
	formatstr( newdir, "%s.%s", val.c_str(), append_str );
	
		// Next, try to create the given directory, if it doesn't
		// already exist.
	make_dir( newdir.c_str() );

		// Now, set our own config hashtable entry so we start using
		// this new directory.
	config_insert( param_name, newdir.c_str() );

	// Finally, insert the _condor_<param_name> environment
	// variable, so our children get the right configuration.
	std::string env_str( "_condor_" );
	env_str += param_name;
	env_str += "=";
	env_str += newdir;
	char *env_cstr = strdup( env_str.c_str() );
	if( SetEnv(env_cstr) != TRUE ) {
		fprintf( stderr, "ERROR: Can't add %s to the environment!\n", 
				 env_cstr );
		free(env_cstr);
		exit( 4 );
	}
	free(env_cstr);
}


void
handle_dynamic_dirs()
{
		// We want the log, spool and execute directory of ourselves
		// and our children to have our pid appended to them.  If the
		// directories aren't there, we should create them, as Condor,
		// if possible.
	if( ! DynamicDirs ) {
		return;
	}

		// if the master is restarting, it already has the changes in
		// the environment and we don't want to append yet another suffix,
		// so bail out if we find our sentinel.
	if(param_boolean("ALREADY_CREATED_LOCAL_DYNAMIC_DIRECTORIES", false)) {
		return;
	}

	int mypid = daemonCore->getpid();
	char buf[256];
	// TODO: Picking IPv4 arbitrarily.
	snprintf( buf, sizeof(buf), "%s-%d", get_local_ipaddr(CP_IPV4).to_ip_string().c_str(), mypid );

	dprintf(D_DAEMONCORE | D_VERBOSE, "Using dynamic directories with suffix: %s\n", buf);
	set_dynamic_dir( "LOG", buf );
	set_dynamic_dir( "SPOOL", buf );
	set_dynamic_dir( "EXECUTE", buf );

		// Final, evil hack.  Set the _condor_STARTD_NAME environment
		// variable, so that the startd will have a unique name. 
	std::string cur_startd_name;
	if(param(cur_startd_name, "STARTD_NAME")) {
		snprintf( buf, sizeof(buf), "_condor_STARTD_NAME=%d@%s", mypid, cur_startd_name.c_str());
	} else {
		snprintf( buf, sizeof(buf), "_condor_STARTD_NAME=%d", mypid );
	}

		// insert modified startd name
	dprintf(D_DAEMONCORE | D_VERBOSE, "Using dynamic directories and setting env %s\n", buf);
	char* env_str = strdup( buf );
	if( SetEnv(env_str) != TRUE ) {
		fprintf( stderr, "ERROR: Can't add %s to the environment!\n", 
				 env_str );
		exit( 4 );
	}
	free(env_str);

		// insert sentinel
	char* acldd = strdup("_condor_ALREADY_CREATED_LOCAL_DYNAMIC_DIRECTORIES=TRUE");
	SetEnv(acldd);
	free(acldd);
}

#if defined(UNIX)
void
unix_sig_coredump(int signum, siginfo_t *s_info, void *)
{
	unsigned long log_args[5];
	struct sigaction sa;
	static bool down = false;

	/* It turns out that the abort() call will unblock the sig
		abort signal and allow the handler to be called again. So,
		in a real world case, which led me to write this test,
		glibc decided something was wrong and called abort(),
		then, in this signal handler, we tickled the exact
		thing glibc didn't like in the first place and so it
		called abort() again, leading back to this handler. A
		segmentation fault happened finally when the stack was
		exhausted. This guard is here to prevent that type
		of scenario from happening again with this handler.
		This fixes ticket number #183 in the condor-wiki. NOTE:
		We never set down to false again, because this handler
		is meant to exit() and not return. */

	if (down == true) {
		return;
	}
	down = true;

	// If si_code > 0, then the signal was generated by the kernel,
	// and si_addr will have relevant data.
	// If si_code <= 0, then the signal was generated via kill() or
	// sigqueue(), and si_pid and si_uid will have relevant data.
	// On linux, si_addr and si_pid/si_uid are in a union distinguished
	// by si_code, so values in the non-relevant fields are invalid and
	// should be ignored.
	log_args[0] = signum;
	log_args[1] = s_info->si_code;
	log_args[2] = s_info->si_pid;
	log_args[3] = s_info->si_uid;
	log_args[4] = (unsigned long)s_info->si_addr;
	dprintf_async_safe("Caught signal %0: si_code=%1, si_pid=%2, si_uid=%3, si_addr=0x%x4\n", log_args, 5);

	dprintf_dump_stack();

	// Just in case we're running as condor or a user.
	if ( setuid(0) ) { }
	if ( setgid(0) ) { }

	if (core_dir != NULL) {
		if (chdir(core_dir)) {
			log_args[0] = (unsigned long)core_dir;
			log_args[1] = (unsigned long)errno;
			dprintf_async_safe("Error: chdir(%s0) failed: %1\n", log_args, 3);
		}
	}

#ifdef LINUX
	if ( prctl(PR_SET_DUMPABLE, 1, 0, 0) != 0 ) {
		log_args[0] = (unsigned long)errno;
		dprintf_async_safe("Warning: prctl() failed: errno %0\n", log_args, 1);
	}
#endif
	// It would be a good idea to actually terminate for the same reason.
	sa.sa_handler = SIG_DFL;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sigaction(signum, &sa, NULL);
	sigprocmask(SIG_SETMASK, &sa.sa_mask, NULL);

	// On linux, raise() calls tgkill() with values for tgid and tid that
	// are cached in memory by glibc. If clone() was previously called with
	// the CLONE_VM flag, glibc sets these cached values to -1. This will
	// cause the tgkill() call in raise() to fail. raise() then returns
	// failure with errno EINVAL.
	// We use clonse() with CLONE_VM in Create_Process() in the schedd to
	// make spawning of child processes more efficient.
	// tgkill() isn't exposed by glibc, but we can call kill() on ourselves
	// to re-raise the signal.
	// There's a possibility that the signal will be delivered to another
	// thread while this thread continues execution. For this case, add a
	// short sleep to prevent _exit() from happening before the signal is.
	// acted upon.
#if defined(LINUX)
	if ( kill(getpid(),signum) != 0) {
#else
	if ( raise(signum) != 0 ) {
#endif
		log_args[0] = (unsigned long)signum;
		log_args[1] = (unsigned long)errno;
		dprintf_async_safe("Error: raise(%0) failed: errno %1\n", log_args, 2);
	}
#if defined(LINUX)
	else {
		sleep(1);
	}
#endif

	// If for whatever reason the second raise doesn't kill us properly, 
	// we shall exit with a non-zero code so if anything depends on us,
	// at least they know there was a problem.
	_exit(JOB_EXCEPTION);
}
#endif


void
install_core_dump_handler()
{
#if defined(UNIX)
		sigset_t fullset;
		sigfillset( &fullset );
		install_sig_action_with_mask(SIGSEGV, &fullset, unix_sig_coredump);
		install_sig_action_with_mask(SIGABRT, &fullset, unix_sig_coredump);
		install_sig_action_with_mask(SIGILL, &fullset, unix_sig_coredump);
		install_sig_action_with_mask(SIGFPE, &fullset, unix_sig_coredump);
		install_sig_action_with_mask(SIGBUS, &fullset, unix_sig_coredump);
#	endif // of if defined(UNIX)
}

void
drop_core_in_log( void )
{
	// chdir to the LOG directory so that if we dump a core
	// it will go there.
	// and on Win32, tell our ExceptionHandler class to drop
	// its pseudo-core file to the LOG directory as well.
	char* ptmp = param("LOG");
	if ( ptmp ) {
		if ( chdir(ptmp) < 0 ) {
#ifdef WIN32
			if (MATCH == strcmpi(get_mySubSystem()->getName(), "KBDD")) {
				dprintf (D_FULLDEBUG, "chdir() to LOG directory failed for KBDD, "
					     "cannot drop core in LOG dir\n");
				free(ptmp);
				return;
			}
#endif
    	EXCEPT("cannot chdir to dir <%s>",ptmp);
		}
	} else {
		dprintf( D_FULLDEBUG, 
				 "No LOG directory specified in config file(s), "
				 "not calling chdir()\n" );
		return;
	}

	if ( core_dir ) {
		free( core_dir );
		core_dir = NULL;
	}
	core_dir = strdup(ptmp);

	// get the name for core files, we need to access this pointer
	// later in the exception handlers, so keep it around in a module static
	// the core dump handler is expected to deal with the case of core_name == NULL
	// by using a default name.
	if (core_name) {
		free(core_name);
		core_name = NULL;
	}
	core_name = param("CORE_FILE_NAME");

	// in some case we need to hook up our own handler to generate
	// core files.
	install_core_dump_handler();

#ifdef WIN32
	{
		// give our Win32 exception handler a filename for the core file
		std::string pseudoCoreFileName;
		formatstr(pseudoCoreFileName,"%s\\%s", ptmp, core_name ? core_name : "core.WIN32");
		g_ExceptionHandler.SetLogFileName(pseudoCoreFileName.c_str());

		// set the path where our Win32 exception handler can find
		// debug symbols
		char *binpath = param("BIN");
		if ( binpath ) {
			SetEnv( "_NT_SYMBOL_PATH", binpath );
			free(binpath);
		}

		// give the handler our pid
		g_ExceptionHandler.SetPID ( daemonCore->getpid () );
	}
#endif
	free(ptmp);
}


// See if we should set the limits on core files.  If the parameter is
// defined, do what it says.  Otherwise, do nothing.
// On NT, if CREATE_CORE_FILES is False, then we will use the
// default NT exception handler which brings up the "Abort or Debug"
// dialog box, etc.  Otherwise, we will just write out a core file
// "summary" in the log directory and not display the dialog.
void
check_core_files()
{
	bool want_set_error_mode = param_boolean_crufty("CREATE_CORE_FILES", true);

#ifndef WIN32
	if( want_set_error_mode ) {
		limit( RLIMIT_CORE, RLIM_INFINITY, CONDOR_SOFT_LIMIT,"max core size" );
	} else {
		limit( RLIMIT_CORE, 0, CONDOR_SOFT_LIMIT,"max core size" );
	}
#endif

#ifdef WIN32
		// Call SetErrorMode so that Win32 "critical errors" and such
		// do not open up a dialog window!
	if ( want_set_error_mode ) {
		::SetErrorMode( 
			SEM_NOGPFAULTERRORBOX | SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX );
		g_ExceptionHandler.TurnOn();
	} else {
		::SetErrorMode( 0 );
		g_ExceptionHandler.TurnOff();
	}
#endif

}


static int
handle_off_fast(int, Stream* stream)
{
	if( !stream->end_of_message() ) {
		dprintf( D_ALWAYS, "handle_off_fast: failed to read end of message\n");
		return FALSE;
	}
	if (daemonCore) {
		daemonCore->Signal_Myself(SIGQUIT);
	}
	return TRUE;
}

	
static int
handle_off_graceful(int, Stream* stream)
{
	if( !stream->end_of_message() ) {
		dprintf( D_ALWAYS, "handle_off_graceful: failed to read end of message\n");
		return FALSE;
	}
	if (daemonCore) {
		daemonCore->Signal_Myself(SIGTERM);
	}
	return TRUE;
}

class SigtermContinue {
public:
	static bool should_sigterm_continue() { return should_continue; };
	static void sigterm_should_not_continue() { should_continue = false; }
	static void sigterm_should_continue() { should_continue = true; }
private:
	static bool should_continue;
};

bool SigtermContinue::should_continue = true;


static int
handle_off_force(int, Stream* stream)
{
	if( !stream->end_of_message() ) {
		dprintf( D_ALWAYS, "handle_off_force: failed to read end of message\n");
		return FALSE;
	}
	if (daemonCore) {
		daemonCore->SetPeacefulShutdown( false );
		SigtermContinue::sigterm_should_continue();
		daemonCore->Signal_Myself(SIGTERM);
	}
	return TRUE;
}

static int
handle_off_peaceful(int, Stream* stream)
{
	// Peaceful shutdown is the same as graceful, except
	// there is no timeout waiting for things to finish.

	if( !stream->end_of_message() ) {
		dprintf( D_ALWAYS, "handle_off_peaceful: failed to read end of message\n");
		return FALSE;
	}
	if (daemonCore) {
		daemonCore->SetPeacefulShutdown(true);
		daemonCore->Signal_Myself(SIGTERM);
	}
	return TRUE;
}

static int
handle_set_peaceful_shutdown(int, Stream* stream)
{
	// If the master could send peaceful shutdown signals, it would
	// not be necessary to have a message for turning on the peaceful
	// shutdown toggle.  Since the master only sends fast and graceful
	// shutdown signals, condor_off is responsible for first turning
	// on peaceful shutdown in appropriate daemons.

	if( !stream->end_of_message() ) {
		dprintf( D_ALWAYS, "handle_set_peaceful_shutdown: failed to read end of message\n");
		return FALSE;
	}
	daemonCore->SetPeacefulShutdown(true);
	return TRUE;
}

static int
handle_set_force_shutdown(int, Stream* stream)
{
	// If the master could send peaceful shutdown signals, it would
	// not be necessary to have a message for turning on the peaceful
	// shutdown toggle.  Since the master only sends fast and graceful
	// shutdown signals, condor_off is responsible for first turning
	// on peaceful shutdown in appropriate daemons.

	if( !stream->end_of_message() ) {
		dprintf( D_ALWAYS, "handle_set_force_shutdown: failed to read end of message\n");
		return FALSE;
	}
	daemonCore->SetPeacefulShutdown( false );
	SigtermContinue::sigterm_should_continue();
	return TRUE;
}


static int
handle_reconfig( int /* cmd */, Stream* stream )
{
	if( !stream->end_of_message() ) {
		dprintf( D_ALWAYS, "handle_reconfig: failed to read end of message\n");
		return FALSE;
	}
	if (!daemonCore->GetDelayReconfig()) {
		dc_reconfig();
	} else {
        dprintf(D_FULLDEBUG, "Delaying reconfig.\n");
 		daemonCore->SetNeedReconfig(true);
	}
	return TRUE;
}

int
handle_fetch_log(int cmd, Stream *s )
{
	char *name = NULL;
	int  total_bytes = 0;
	int result;
	int type = -1;
	ReliSock *stream = (ReliSock *) s;

	if ( cmd == DC_PURGE_LOG ) {
		return handle_fetch_log_history_purge( stream );
	}

	if( ! stream->code(type) ||
		! stream->code(name) || 
		! stream->end_of_message()) {
		dprintf( D_ALWAYS, "DaemonCore: handle_fetch_log: can't read log request\n" );
		free( name );
		return FALSE;
	}

	stream->encode();

	switch (type) {
		case DC_FETCH_LOG_TYPE_PLAIN:
			break; // handled below
		case DC_FETCH_LOG_TYPE_HISTORY:
			return handle_fetch_log_history(stream, name);
		case DC_FETCH_LOG_TYPE_HISTORY_DIR:
			return handle_fetch_log_history_dir(stream, name);
		case DC_FETCH_LOG_TYPE_HISTORY_PURGE:
			free(name);
			return handle_fetch_log_history_purge(stream);
		default:
			dprintf(D_ALWAYS,"DaemonCore: handle_fetch_log: I don't know about log type %d!\n",type);
			result = DC_FETCH_LOG_RESULT_BAD_TYPE;
			if (!stream->code(result)) {
				dprintf(D_ALWAYS,"DaemonCore: handle_fetch_log: and the remote side hung up\n");
			}
			stream->end_of_message();
			free(name);
			return FALSE;
	}

	char *pname = (char*)malloc (strlen(name) + 5);
	ASSERT(pname);
	char *ext = strchr(name,'.');

	//If there is a dot in the name, it is of the form "<SUBSYS>.<ext>"
	//Otherwise, it is "<SUBSYS>".  The file extension is used to
	//handle such things as "StarterLog.slot1" and "StarterLog.cod"

	if(ext) {
		strncpy(pname, name, ext-name);
		pname[ext-name] = '\0';
	}
	else {
		strcpy(pname, name);
	}

	strcat (pname, "_LOG");

	char *filename = param(pname);
	if(!filename) {
		dprintf( D_ALWAYS, "DaemonCore: handle_fetch_log: no parameter named %s\n",pname);
		result = DC_FETCH_LOG_RESULT_NO_NAME;
		if (stream->code(result)) {
				dprintf(D_ALWAYS,"DaemonCore: handle_fetch_log: and the remote side hung up\n");
		}
		stream->end_of_message();
        free(pname);
        free(name);
		return FALSE;
	}

	std::string full_filename = filename;
	if(ext) {
		full_filename += ext;

		if( strchr(ext,DIR_DELIM_CHAR) ) {
			dprintf( D_ALWAYS, "DaemonCore: handle_fetch_log: invalid file extension specified by user: ext=%s, filename=%s\n",ext,full_filename.c_str() );
			free(pname);
			return FALSE;
		}
	}

	int fd = safe_open_wrapper_follow(full_filename.c_str(),O_RDONLY);
	if(fd<0) {
		dprintf( D_ALWAYS, "DaemonCore: handle_fetch_log: can't open file %s\n",full_filename.c_str());
		result = DC_FETCH_LOG_RESULT_CANT_OPEN;
		if (!stream->code(result)) {
				dprintf(D_ALWAYS,"DaemonCore: handle_fetch_log: and the remote side hung up\n");
		}
		stream->end_of_message();
        free(filename);
        free(pname);
        free(name);
		return FALSE;
	}

	result = DC_FETCH_LOG_RESULT_SUCCESS;
	if (!stream->code(result)) {
		dprintf(D_ALWAYS, "DaemonCore: handle_fetch_log: client hung up before we could send result back\n");
	}

	filesize_t size;
	stream->put_file(&size, fd);
	total_bytes += size;

	stream->end_of_message();

	if(total_bytes<0) {
		dprintf( D_ALWAYS, "DaemonCore: handle_fetch_log: couldn't send all data!\n");
	}

	close(fd);
	free(filename);
	free(pname);
	free(name);

	return total_bytes>=0;
}

int
handle_fetch_log_history(ReliSock *stream, char *name) {
	int result = DC_FETCH_LOG_RESULT_BAD_TYPE;

	const char *history_file_param = "HISTORY";
	if (strcmp(name, "STARTD_HISTORY") == 0) {
		history_file_param = "STARTD_HISTORY";
	}

	free(name);

	std::string history_file;
	if (!param(history_file, history_file_param)) {
		dprintf( D_ALWAYS, "DaemonCore: handle_fetch_log_history: no parameter named %s\n", history_file_param);
		if (!stream->code(result)) {
				dprintf(D_ALWAYS,"DaemonCore: handle_fetch_log: and the remote side hung up\n");
		}
		stream->end_of_message();
		return FALSE;
	}

	std::vector<std::string> historyFiles = findHistoryFiles(history_file.c_str());

	result = DC_FETCH_LOG_RESULT_SUCCESS;
	if (!stream->code(result)) {
		dprintf(D_ALWAYS, "DaemonCore: handle_fetch_log_history: client hung up before we could send result back\n");
	}

	for (const auto& histFile : historyFiles) {
		filesize_t size;
		stream->put_file(&size, histFile.c_str());
	}

	stream->end_of_message();

	return TRUE;
}

int
handle_fetch_log_history_dir(ReliSock *stream, char *paramName) {
	int result = DC_FETCH_LOG_RESULT_BAD_TYPE;

	free(paramName);
	char *dirName = param("STARTD.PER_JOB_HISTORY_DIR"); 
	if (!dirName) {
		dprintf( D_ALWAYS, "DaemonCore: handle_fetch_log_history_dir: no parameter named PER_JOB\n");
		if (!stream->code(result)) {
				dprintf( D_ALWAYS, "DaemonCore: handle_fetch_log_history_dir: and the remote side hung up\n");
		}
		stream->end_of_message();
		return FALSE;
	}

	Directory d(dirName);
	const char *filename;
	int one=1;
	int zero=0;
	while ((filename = d.Next())) {
		if (!stream->code(one)) { // more data
			dprintf(D_ALWAYS, "fetch_log_history_dir: client disconnected\n");
			break;
		}
		stream->put(filename);
		std::string fullPath(dirName);
		fullPath += "/";
		fullPath += filename;
		int fd = safe_open_wrapper_follow(fullPath.c_str(),O_RDONLY);
		if (fd >= 0) {
			filesize_t size;
			stream->put_file(&size, fd);
			close(fd);
		}
	}

	free(dirName);


	if (!stream->code(zero)) { // no more data
		dprintf(D_ALWAYS, "DaemonCore: handle_fetch_log_history_dir: client hung up before we could send result back\n");
	}
	stream->end_of_message();
	return 0;
}

int
handle_fetch_log_history_purge(ReliSock *s) {

	int result = 0;
	time_t cutoff = 0;
	if (!s->code(cutoff)) {
		dprintf(D_ALWAYS, "fetch_log_history_purge: client disconnect\n");
	}
	s->end_of_message();

	s->encode();

	char *dirName = param("STARTD.PER_JOB_HISTORY_DIR"); 
	if (!dirName) {
		dprintf( D_ALWAYS, "DaemonCore: handle_fetch_log_history_dir: no parameter named PER_JOB\n");
		if (!s->code(result)) {
				dprintf( D_ALWAYS, "DaemonCore: handle_fetch_log_history_dir: and the remote side hung up\n");
		}
		s->end_of_message();
		return FALSE;
	}

	Directory d(dirName);

	result = 1;
	while (d.Next()) {
		time_t last = d.GetModifyTime();
		if (last < cutoff) {
			d.Remove_Current_File();
		}
	}

    free(dirName);

    if (!s->code(result)) { // no more data
		dprintf(D_ALWAYS, "DaemonCore: handle_fetch_log_history_purge: client hung up before we could send result back\n");
	}
    s->end_of_message();
    return 0;
}

int
handle_dc_query_instance( int, Stream* stream)
{
	if( !stream->end_of_message() ) {
		dprintf( D_FULLDEBUG, "handle_dc_query_instance: failed to read end of message\n");
		return FALSE;
	}

	// the first caller causes us to make a random instance id
	// all subsequent queries will get the same instance id.
	static char * instance_id = NULL;
	const int instance_length = 16;
	if ( ! instance_id) {
		unsigned char * bytes = Condor_Crypt_Base::randomKey(instance_length/2);
		ASSERT(bytes);
		std::string tmp; tmp.reserve(instance_length+1);
		for (int ii = 0; ii < instance_length/2; ++ii) {
			formatstr_cat(tmp, "%02x", bytes[ii]);
		}
		instance_id = strdup(tmp.c_str());
		free(bytes);
	}

	stream->encode();
	if ( ! stream->put_bytes(instance_id, instance_length) ||
		 ! stream->end_of_message()) {
		dprintf( D_FULLDEBUG, "handle_dc_query_instance: failed to send instance value\n");
	}

	return TRUE;
}


static int
handle_dc_start_token_request(int, Stream* stream)
{
	classad::ClassAd ad;
	if (!getClassAd(stream, ad) ||
		!stream->end_of_message())
	{
		dprintf(D_FULLDEBUG, "handle_dc_start_token_request: failed to read input from client\n");
		return false;
	}

	int error_code = 0;
	std::string error_string;

	std::string client_id;
	if (!ad.EvaluateAttrString(ATTR_SEC_CLIENT_ID, client_id)) {
		error_code = 2;
		error_string = "No client ID provided.";
	}

	std::string requested_identity;
	if (!ad.EvaluateAttrString(ATTR_SEC_USER, requested_identity)) {
		error_code = 2;
		error_string = "No identity request.";
	}
		// Note this allows unauthenticated users.
	const char *fqu = static_cast<Sock*>(stream)->getFullyQualifiedUser();
	if (!fqu) {
		error_code = 2;
		error_string = "Missing requester identity.";
	}

	const char *peer_location = static_cast<Sock*>(stream)->peer_ip_str();

        std::set<std::string> config_bounding_set;
        std::string config_bounding_set_str;
        if (param(config_bounding_set_str, "SEC_TOKEN_REQUEST_LIMITS")) {
                for (const auto& authz: StringTokenIterator(config_bounding_set_str)) {
                        config_bounding_set.insert(authz);
                }
        }

	std::vector<std::string> authz_list;
	std::string authz_list_str;
	if (ad.EvaluateAttrString(ATTR_SEC_LIMIT_AUTHORIZATION, authz_list_str)) {
		for (const auto& authz: StringTokenIterator(authz_list_str)) {
			if (config_bounding_set.empty() || (config_bounding_set.find(authz) != config_bounding_set.end())) {
				authz_list.emplace_back(authz);
			}
		}
			// If all potential bounds were removed by the set intersection,
			// throw an error instead of generating an "all powerful" token.
		if (!config_bounding_set.empty() && authz_list.empty()) {
			error_code = 3;
			error_string = "All requested authorizations were eliminated by the"
				" SEC_TOKEN_REQUEST_LIMITS setting";
		}
	} else if (!config_bounding_set.empty()) {
		for (const auto &authz : config_bounding_set) {
			authz_list.push_back(authz);
		}
	}

	int requested_lifetime;
	if (!ad.EvaluateAttrInt(ATTR_SEC_TOKEN_LIFETIME, requested_lifetime)) {
		requested_lifetime = -1;
	}
	int max_lifetime = param_integer("SEC_ISSUED_TOKEN_EXPIRATION", -1);
	if ((max_lifetime > 0) && (requested_lifetime > max_lifetime)) {
		requested_lifetime = max_lifetime;
	} else if ((max_lifetime > 0)  && (requested_lifetime < 0)) {
		requested_lifetime = max_lifetime;
	}

	classad::ClassAd result_ad;
	if (error_code) {
		result_ad.InsertAttr(ATTR_ERROR_STRING, error_string);
		result_ad.InsertAttr(ATTR_ERROR_CODE, error_code);
		// TODO: If this is an ADMINISTRATOR-level connection, why not authorize immediately?
	} else if (g_request_map.size() > 1000) {
		error_code = 3;
		error_string = "Too many requests in the system.";
	} else {
		unsigned request_id = get_csrng_uint() % 10'000'000;
		auto iter = g_request_map.find(request_id);
		int idx = 0;
			// Try a few randomly generated request IDs; to avoid strange issues,
			// bail out after a fixed limit.
		while ((iter != g_request_map.end() && (idx++ < 5))) {
			request_id = get_csrng_uint() % 10'000'000;
			iter = g_request_map.find(request_id);
		}
		std::string request_id_str;
		formatstr(request_id_str, "%07d", request_id);
		if (iter != g_request_map.end()) {
			result_ad.InsertAttr(ATTR_ERROR_STRING, "Unable to generate new request ID");
			result_ad.InsertAttr(ATTR_ERROR_CODE, 4);
		} else {
			g_request_map[request_id] = std::unique_ptr<TokenRequest>(
				new TokenRequest{fqu, requested_identity, peer_location, authz_list, requested_lifetime, client_id, request_id_str});
		}
			// Note we currently store this as a string; this way we can come back later
			// and introduce alphanumeric characters if we so wish.
		result_ad.InsertAttr(ATTR_SEC_REQUEST_ID, request_id_str);

		iter = g_request_map.find(request_id);
		time_t now = time(NULL);

		CondorError err;
		std::string rule_text;
		std::string final_key_name = htcondor::get_token_signing_key(err);
		if (final_key_name.empty()) {
			result_ad.InsertAttr(ATTR_ERROR_STRING, err.getFullText());
			result_ad.InsertAttr(ATTR_ERROR_CODE, err.code());
			iter = g_request_map.end();
		} else {
			if ((iter != g_request_map.end()) && TokenRequest::ShouldAutoApprove(*(iter->second), now, rule_text)) {
				auto token_request = *(iter->second);
				CondorError err;
				std::string token;
				if (!Condor_Auth_Passwd::generate_token(
					token_request.getRequestedIdentity(),
					final_key_name,
					token_request.getBoundingSet(),
					token_request.getLifetime(),
					token,
					static_cast<Sock*>(stream)->getUniqueId(),
					&err))
				{
					result_ad.InsertAttr(ATTR_ERROR_STRING, err.getFullText());
					result_ad.InsertAttr(ATTR_ERROR_CODE, err.code());
					token_request.setFailed();
				} else {
					g_request_map.erase(iter);
					if (token.empty()) {
						error_code = 6;
						error_string = "Internal state error.";
					}
					result_ad.InsertAttr(ATTR_SEC_TOKEN, token);
					dprintf(D_ALWAYS, "Token request %s approved via auto-approval rule %s.\n",
						token_request.getPublicString().c_str(), rule_text.c_str());
				}
			// Auto-approval rules are effectively host-based security; if we trust the network
			// blindly, the lack of encryption does not bother us.
			//
			// In all other cases, encryption is a hard requirement.
			} else if (!stream->get_encryption()) {
				g_request_map.erase(iter);
				result_ad.Clear();
				result_ad.InsertAttr(ATTR_ERROR_STRING, "Request to server was not encrypted.");
				result_ad.InsertAttr(ATTR_ERROR_CODE, 7);
			} else {
				Sock * sock = dynamic_cast<Sock *>(stream);
				if( sock ) {
					const char * method = sock->getAuthenticationMethodUsed();
					if( strcasecmp( method, "ANONYMOUS" ) == 0 ) {
						g_request_map.erase(iter);
						result_ad.Clear();
						result_ad.InsertAttr(ATTR_ERROR_STRING, "Request to server was made using ANONYMOUS authentication.");
						result_ad.InsertAttr(ATTR_ERROR_CODE, 7);
					}
				}
			}
		}
	}

	stream->encode();
	if (!putClassAd(stream, result_ad) ||
		!stream->end_of_message())
	{
		dprintf(D_FULLDEBUG, "handle_dc_start_token_request: failed to send response ad to client\n");
		return false;
	}
	return true;
}


static int
handle_dc_finish_token_request(int, Stream* stream)
{
	classad::ClassAd ad;
	if (!getClassAd(stream, ad) ||
		!stream->end_of_message())
	{
		dprintf(D_FULLDEBUG, "handle_dc_finish_token_request: failed to read input from client\n");
		return false;
	}

	int error_code = 0;
	std::string error_string;

	if (!g_request_limit.AllowIncomingRequest()) {
		error_code = 5;
		error_string = "Request rate limit hit.";
	}

	std::string client_id;
	std::string request_id_str;
	int request_id = -1;
	if (!error_code) {
		if (!ad.EvaluateAttrString(ATTR_SEC_CLIENT_ID, client_id)) {
			error_code = 2;
			error_string = "No client ID provided.";
		}

		// See comment in handle_dc_list_token_request().
		if (!ad.EvaluateAttrString(ATTR_SEC_REQUEST_ID, request_id_str)) {
			error_code = 2;
			error_string = "No request ID provided.";
		} else {
			YourStringDeserializer des(request_id_str);
			if ( ! des.deserialize_int(&request_id) || ! des.at_end()) {
				error_code = 2;
				error_string = "Unable to convert request ID to integer.";
			}
		}
	}

	std::string token;
	auto iter = (request_id >= 0) ? g_request_map.find(request_id) : g_request_map.end();
	if (iter == g_request_map.end()) {
		error_code = 3;
		error_string = "Request ID is not known.";
	} else if (iter->second->getClientId() != client_id) {
		error_code = 3;
		error_string = "Client ID is incorrect.";
	} else {
		const auto &req = *(iter->second);
		switch (req.getState()) {
		case TokenRequest::State::Pending:
			break;
		case TokenRequest::State::Successful:
			token = req.getToken();
			// Remove the token from the request list; no one else should be able to retrieve it.
			g_request_map.erase(iter);
			if (token.empty()) {
				error_code = 6;
				error_string = "Internal state error.";
			}
			break;
		case TokenRequest::State::Failed:
			error_code = 4;
			error_string = "Request failed.";
			g_request_map.erase(iter);
			break;
		case TokenRequest::State::Expired:
			g_request_map.erase(iter);
			error_code = 5;
			error_string = "Request has expired.";
		};
	}

	classad::ClassAd result_ad;
	if (error_code) {
		result_ad.InsertAttr(ATTR_ERROR_STRING, error_string);
		result_ad.InsertAttr(ATTR_ERROR_CODE, error_code);
	} else {
			// NOTE: client always expects this attribute to
			// be set; if it is set to an empty string, then
			// it knows to poll again.
		result_ad.InsertAttr(ATTR_SEC_TOKEN, token);
	}

	stream->encode();
	if (!putClassAd(stream, result_ad) ||
		!stream->end_of_message())
	{
		dprintf(D_FULLDEBUG, "handle_dc_finish_token_request: failed to send response ad to client\n");
		return false;
	}
	return true;
}


static int
handle_dc_list_token_request(int, Stream* stream)
{
	classad::ClassAd ad;
	if (!getClassAd(stream, ad) || !stream->end_of_message())
	{
		dprintf(D_FULLDEBUG, "handle_dc_list_token_request: failed to read input from client\n");
		return false;
	}

	int error_code = 0;
	std::string error_string;

	bool has_admin = static_cast<Sock*>(stream)->isAuthorizationInBoundingSet("ADMINISTRATOR") &&
		daemonCore->Verify("list request", ADMINISTRATOR,
		static_cast<ReliSock*>(stream)->peer_addr(),
		static_cast<Sock*>(stream)->getFullyQualifiedUser());

	// While it looks like we could use EvaluateAttrInt() here, we're storing
	// request IDs as strings in case in turns out people don't like random
	// 7-digit integers.  The std::stol() call is an optimization which we'll
	// have to remove or replace if we cahnge the request ID format.
	//
	// The filter is optional; if it's absent, we'll list all pending requests.
	std::string request_filter_str;
	if (!error_code && (ad.EvaluateAttrString(ATTR_SEC_REQUEST_ID, request_filter_str) && !request_filter_str.empty())) {
		int request_id=-1;
		YourStringDeserializer des(request_filter_str);
		if ( ! des.deserialize_int(&request_id) || ! des.at_end()) {
			error_code = 2;
			error_string = "Unable to convert request ID to integer.";
		}
	}

	stream->encode();

	classad::ClassAd result_ad;
	for (const auto & iter : g_request_map) {
		if (error_code) { break; }

		const auto &token_request = iter.second;

		if (token_request->getState() != TokenRequest::State::Pending) {continue;}

		const auto &request_id_str = iter.second->getRequestId();
		if (!request_filter_str.empty() && (request_filter_str != request_id_str)) {continue;}

		std::string bounds = join(token_request->getBoundingSet(), ",");

			// If we do not have ADMINISTRATOR privileges, the requested identity
			// and the authenticated identity must match!
		if (!has_admin && strcmp(token_request->getRequestedIdentity().c_str(),
			static_cast<Sock*>(stream)->getFullyQualifiedUser()))
		{
			continue;
		}

		if (!result_ad.InsertAttr(ATTR_SEC_REQUEST_ID, request_id_str) ||
			!result_ad.InsertAttr(ATTR_SEC_CLIENT_ID, token_request->getClientId()) ||
			!result_ad.InsertAttr(ATTR_AUTHENTICATED_IDENTITY, token_request->getRequesterIdentity()) ||
			!result_ad.InsertAttr("RequestedIdentity", token_request->getRequestedIdentity()) ||
			!result_ad.InsertAttr("PeerLocation", token_request->getPeerLocation()))
		{
			dprintf(D_FULLDEBUG, "handle_dc_list_token_request: failed to create"
				" token request ad listing.\n");
			return false;
		}
		if (bounds.size() && !result_ad.InsertAttr(ATTR_SEC_LIMIT_AUTHORIZATION, bounds))
		{
			dprintf(D_FULLDEBUG, "handle_dc_list_token_request: failed to create"
				" token request ad listing.\n");
			return false;
		}
		if (token_request->getLifetime() >= 0 && !result_ad.InsertAttr(ATTR_SEC_TOKEN_LIFETIME, token_request->getLifetime())) {
			dprintf(D_FULLDEBUG, "handle_dc_list_token_request: failed to create"
				" token request ad listing.\n");
			return false;
		}

		if (!putClassAd(stream, result_ad) || !stream->end_of_message())
		{
			dprintf(D_FULLDEBUG, "handle_dc_list_token_request: failed to send response ad to client\n");
			return false;
		}
		result_ad.Clear();
	}

	result_ad.Clear();
	int intVal = 0;
	if (!result_ad.InsertAttr(ATTR_ERROR_CODE, error_code) ||
		!result_ad.InsertAttr(ATTR_OWNER, intVal))
	{
		dprintf(D_FULLDEBUG, "handle_dc_list_token_request: failed to create final response ad");
		return false;
	}
	if (error_code) {
		result_ad.InsertAttr(ATTR_ERROR_STRING, error_string);
	}
	if (!putClassAd(stream, result_ad) || !stream->end_of_message())
	{
		dprintf(D_FULLDEBUG, "handle_dc_list_token_request: failed to send final response ad to client\n");
		return false;
	}
	return true;
}


static int
handle_dc_approve_token_request(int, Stream* stream)
{
	classad::ClassAd ad;
	if (!getClassAd(stream, ad) || !stream->end_of_message())
	{
		dprintf(D_FULLDEBUG, "handle_dc_approve_token_request: failed to read input from client\n");
		return false;
	}

	int error_code = 0;
	std::string error_string;

	bool has_admin = static_cast<Sock*>(stream)->isAuthorizationInBoundingSet("ADMINISTRATOR") &&
		daemonCore->Verify("approve request", ADMINISTRATOR, static_cast<ReliSock*>(stream)->peer_addr(),
		static_cast<Sock*>(stream)->getFullyQualifiedUser());

	// See comment in handle_dc_list_token_request().
	int request_id = -1;
	std::string request_id_str;
	if (!error_code && (!ad.EvaluateAttrString(ATTR_SEC_REQUEST_ID, request_id_str) || request_id_str.empty()))
	{
		error_code = 1;
		error_string = "Request ID not provided.";
	} else {
		YourStringDeserializer des(request_id_str);
		if ( ! des.deserialize_int(&request_id) || ! des.at_end()) {
			error_code = 2;
			error_string = "Unable to convert request ID to integer.";
		}
	}

	auto iter = g_request_map.find(request_id);
	if (request_id != -1 && iter == g_request_map.end()) {
		error_code = 5;
		error_string = "Request unknown.";
		request_id = -1;
		dprintf(D_SECURITY, "Request ID (%d) unknown.\n", request_id);
	}

	std::string client_id;
	if (!error_code && (!ad.EvaluateAttrString(ATTR_SEC_CLIENT_ID, client_id) || client_id.empty()))
	{
		error_code = 1;
		error_string = "Client ID not provided.";
	}

	if (!error_code && request_id != -1 && client_id != iter->second->getClientId()) {
		error_code = 5;
		error_string = "Request unknown.";
		request_id = -1;
		dprintf(D_SECURITY, "Request ID (%s) correct but client ID (%s) incorrect.\n", request_id_str.c_str(),
			client_id.c_str());
	}
	if (!error_code && request_id != -1 && iter->second->getState() != TokenRequest::State::Pending) {
		error_code = 5;
		error_string = "Request in incorrect state.";
		request_id = -1;
	}

		// If we do not have ADMINISTRATOR privileges, the requested identity
		// and the authenticated identity must match!
	if (!error_code && !has_admin && strcmp(iter->second->getRequestedIdentity().c_str(),
		static_cast<Sock*>(stream)->getFullyQualifiedUser()))
	{
		error_code = 6;
		error_string = "Insufficient privilege to approve request.";
		request_id = -1;
	}

	CondorError err;
	std::string final_key_name = htcondor::get_token_signing_key(err);
	if ((request_id != -1) && final_key_name.empty()) {
		error_string = err.getFullText();
		error_code = err.code();
	}

	stream->encode();
	classad::ClassAd result_ad;

	if (error_code) {
		result_ad.InsertAttr(ATTR_ERROR_CODE, error_code);
		result_ad.InsertAttr(ATTR_ERROR_STRING, error_string);
	} else {
		auto &token_request = *(iter->second);
		CondorError err;
		std::string token;
		if (!Condor_Auth_Passwd::generate_token(
			token_request.getRequestedIdentity(),
			final_key_name,
			token_request.getBoundingSet(),
			token_request.getLifetime(),
			token,
			static_cast<Sock*>(stream)->getUniqueId(),
			&err))
		{
			result_ad.InsertAttr(ATTR_ERROR_STRING, err.getFullText());
			result_ad.InsertAttr(ATTR_ERROR_CODE, err.code());
			token_request.setFailed();
		} else {
			token_request.setToken(token);
			result_ad.InsertAttr(ATTR_ERROR_CODE, 0);
		}
	}

	if (!putClassAd(stream, result_ad) || !stream->end_of_message())
	{
		dprintf(D_FULLDEBUG, "handle_dc_approve_token_request: failed to send final response ad to client\n");
		return false;
	}
	return true;
}


static int
handle_dc_auto_approve_token_request(int, Stream* stream )
{
	classad::ClassAd ad;
	if (!getClassAd(stream, ad) || !stream->end_of_message())
	{
		dprintf(D_FULLDEBUG, "handle_dc_auto_approve_token_request: failed to read input from client\n");
		return false;
	}

	std::string netblock;
	time_t lifetime = -1;
	ad.EvaluateAttrString(ATTR_SUBNET, netblock);
	ad.EvaluateAttrInt(ATTR_SEC_LIFETIME, lifetime);
	int max_lifetime = param_integer("TOKEN_REQUEST_AUTO_APPROVE_MAX_LIFETIME", 3600);
	if (lifetime > max_lifetime) lifetime = max_lifetime;

	stream->encode();
	classad::ClassAd result_ad;

	CondorError err;
	int error_code = 0;
	std::string error_string;

	if( TokenRequest::addApprovalRule(netblock, lifetime, err) ) {
		dprintf(D_SECURITY|D_FULLDEBUG, "Added a new auto-approve rule for netblock %s"
			" with lifetime %ld.\n", netblock.c_str(), lifetime);

		std::string final_key_name = htcondor::get_token_signing_key(err);
		if (final_key_name.empty()) {
			error_string = err.getFullText();
			error_code = err.code();
		}

		time_t now = time(NULL);
		// We otherwise only evaluate each request as it comes in, so we have to
		// check all of them now if we want to approve ones that came in recently.
		dprintf(D_SECURITY|D_FULLDEBUG, "Evaluating %zu existing requests for "
			"auto-approval.\n", g_request_map.size());
		for (auto &iter : g_request_map) {
			if (error_code) {break;}

			std::string rule_text;
			if (!TokenRequest::ShouldAutoApprove(*(iter.second), now, rule_text)) {
				continue;
			}
			auto &token_request = *(iter.second);
			CondorError err;
			std::string token;
			if (!Condor_Auth_Passwd::generate_token(
				token_request.getRequestedIdentity(),
				final_key_name,
				token_request.getBoundingSet(),
				token_request.getLifetime(),
				token,
				static_cast<Sock*>(stream)->getUniqueId(),
				&err))
			{
				error_string = err.getFullText();
				error_code = err.code();
				token_request.setFailed();
			} else {
				token_request.setToken(token);
				dprintf(D_SECURITY|D_FULLDEBUG,
					"Auto-approved existing request %d.\n",
					iter.first);
				dprintf(D_ALWAYS, "Token request %s passed via auto-approval rule %s.\n",
					token_request.getPublicString().c_str(), rule_text.c_str());
			}
		}
	} else {
		dprintf(D_FULLDEBUG, "Rejected new auto-approve rule for netblock %s "
			"with lifetime %ld: %s\n", netblock.c_str(), lifetime, err.getFullText().c_str());
		error_string = err.getFullText();
		error_code = err.code();
	}

	result_ad.InsertAttr(ATTR_ERROR_CODE, error_code);
	if (error_code) {
		result_ad.InsertAttr(ATTR_ERROR_STRING, error_string);
	}

	if (!putClassAd(stream, result_ad) || !stream->end_of_message())
	{
		dprintf(D_FULLDEBUG, "handle_dc_auto_approve_token_request: failed to send final response ad to client\n");
		return false;
	}
	return true;
}


static int
handle_dc_exchange_scitoken(int, Stream *stream)
{
	classad::ClassAd request_ad;
	if (!getClassAd(stream, request_ad) ||
		!stream->end_of_message())
	{
		dprintf(D_FULLDEBUG, "handle_dc_exchange_scitoken: failed to read input from client\n");
		return false;
	}

	classad::ClassAd result_ad;
	std::string result_token;
	int error_code = 0;
	std::string error_string;

	std::string scitoken;
	if (!request_ad.EvaluateAttrString(ATTR_SEC_TOKEN, scitoken) || scitoken.empty()) {
		error_code = 1;
		error_string = "SciToken not provided by the client";
	}

#if defined(HAVE_EXT_SCITOKENS)
	if (!error_code) {
		std::string subject;
		std::string issuer;
		long long expiry;
		std::vector<std::string> bounding_set;
		CondorError err;
		std::string key_name;
		auto map_file = Authentication::getGlobalMapFile();
		std::string identity;
		std::string jti;
		std::vector<std::string> groups, scopes;
		if (!htcondor::validate_scitoken(scitoken, issuer, subject, expiry, bounding_set, groups, scopes, jti, static_cast<Sock*>(stream)->getUniqueId(), err))
		{
			error_code = err.code();
			error_string = err.getFullText();
		} else if ((key_name = htcondor::get_token_signing_key(err)).empty()) {
			error_code = err.code();
			error_string = err.getFullText();
		} else if (!map_file || map_file->GetCanonicalization("SCITOKENS", issuer + "," + subject, identity)) {
			error_code = 5;
			error_string = "Failed to map SciToken to a local identity.";
		} else {
			long lifetime = expiry - time(NULL);
			int max_lifetime = param_integer("SEC_ISSUED_TOKEN_EXPIRATION", -1);
			if ((max_lifetime > 0) && (lifetime > max_lifetime)) {
				lifetime = max_lifetime;
			}
			if (lifetime < 0) {lifetime = 0;}

			if (!Condor_Auth_Passwd::generate_token(identity, key_name, bounding_set,
				lifetime, result_token, static_cast<Sock*>(stream)->getUniqueId(), &err))
			{
				error_code = err.code();
				error_string = err.getFullText();
			} else {
				auto peer_location = static_cast<Sock*>(stream)->peer_ip_str();
				auto peer_identity = static_cast<Sock*>(stream)->getFullyQualifiedUser();
				std::string bounding_set_str;
				if (!bounding_set.empty()) {
					bounding_set_str = join(bounding_set, ",");
				} else {
					bounding_set_str = "(none)";
				}

				dprintf(D_ALWAYS, "For peer %s (identity %s), exchanging SciToken "
					"from issuer %s, subject %s for a local token with identity %s,"
					" bounding set %s, and lifetime %ld.\n", peer_location,
					peer_identity, issuer.c_str(), subject.c_str(),
					identity.c_str(), bounding_set_str.c_str(), lifetime);
			}
		}
	}
#else
	error_code = 2;
	error_string = "Server not built with SciTokens support";
#endif

	if (error_code)
	{
		result_ad.InsertAttr(ATTR_ERROR_STRING, error_string);
		result_ad.InsertAttr(ATTR_ERROR_CODE, error_code);
	}
	else
	{
		result_ad.InsertAttr(ATTR_SEC_TOKEN, result_token);
	}

	stream->encode();
	if (!putClassAd(stream, result_ad) ||
		!stream->end_of_message())
	{
		dprintf(D_FULLDEBUG, "handle_dc_exchange_scitoken: failed to send response ad to client\n");
		return false;
	}
	return true;
}


static int
handle_dc_session_token(int, Stream* stream)
{
	classad::ClassAd ad;
	if (!getClassAd(stream, ad) ||
		!stream->end_of_message())
	{
		dprintf(D_FULLDEBUG, "handle_dc_session_token: failed to read input from client\n");
		return false;
	}
	CondorError err;
	classad::ClassAd result_ad;

	std::vector<std::string> authz_list;
	std::string authz_list_str;
	if (ad.EvaluateAttrString(ATTR_SEC_LIMIT_AUTHORIZATION, authz_list_str)) {
		authz_list = split(authz_list_str);
	}
	int requested_lifetime;
	if (ad.EvaluateAttrInt(ATTR_SEC_TOKEN_LIFETIME, requested_lifetime)) {
		int max_lifetime = param_integer("SEC_ISSUED_TOKEN_EXPIRATION", -1);
		if ((max_lifetime > 0) && (requested_lifetime > max_lifetime)) {
			requested_lifetime = max_lifetime;
		} else if ((max_lifetime > 0)  && (requested_lifetime < 0)) {
			requested_lifetime = max_lifetime;
		}
	} else {
		requested_lifetime = -1;
	}

	std::string requested_key_name;
	std::string final_key_name = htcondor::get_token_signing_key(err);
	if (ad.EvaluateAttrString(ATTR_SEC_REQUESTED_KEY, requested_key_name)) {
		std::string allowed_key_names_list;
		param( allowed_key_names_list, "SEC_TOKEN_FETCH_ALLOWED_SIGNING_KEYS", "POOL" );
		std::vector<std::string> sl = split(allowed_key_names_list);
		if( contains_withwildcard(sl, requested_key_name) ) {
			final_key_name = requested_key_name;
		} else {
			result_ad.InsertAttr(ATTR_ERROR_STRING, "Server will not sign with requested key.");
			result_ad.InsertAttr(ATTR_ERROR_CODE, 3);

			// *sigh*
			stream->encode();
			if (!putClassAd(stream, result_ad) ||
				!stream->end_of_message())
			{
				dprintf(D_FULLDEBUG, "handle_dc_session_token: failed to send response ad to client\n");
				return false;
			}
			return true;
		}
	}

	classad::ClassAd policy_ad;
	static_cast<ReliSock*>(stream)->getPolicyAd(policy_ad);
	const char *auth_user_cstr;
	std::string auth_user;
	long original_expiry = -1;
	long max_lifetime = 0;
	if (policy_ad.EvaluateAttrInt("TokenExpirationTime", original_expiry)) {
		time_t now = time(NULL);
		max_lifetime = original_expiry - now;
		if ((requested_lifetime > max_lifetime) ||
			((max_lifetime >= 0) && (requested_lifetime < 0)))
		{
			requested_lifetime = max_lifetime;
		}
	}

	if (max_lifetime < 0) {
		result_ad.InsertAttr(ATTR_ERROR_STRING, "Cannot create new session as original one expired.");
		result_ad.InsertAttr(ATTR_ERROR_CODE, 3);
	} else if (!static_cast<Sock*>(stream)->isMappedFQU() ||
		!(auth_user_cstr = static_cast<Sock*>(stream)->getFullyQualifiedUser()) ||
		(auth_user = auth_user_cstr).empty())
	{
		result_ad.InsertAttr(ATTR_ERROR_STRING, "Server did not successfully authenticate session.");
		result_ad.InsertAttr(ATTR_ERROR_CODE, 2);
	} else if (final_key_name.empty()) {
		result_ad.InsertAttr(ATTR_ERROR_STRING, "Server does not have access to configured key.");
		result_ad.InsertAttr(ATTR_ERROR_CODE, 1);
		std::string key_name = "POOL";
		param(key_name, "SEC_TOKEN_ISSUER_KEY");
		dprintf(D_SECURITY, "Daemon configured to sign with key named %s; this is not available.\n",
			key_name.c_str());
	}
	else
	{
		std::string token;
		if (!Condor_Auth_Passwd::generate_token(
			auth_user,
			final_key_name,
			authz_list,
			requested_lifetime,
			token,
			static_cast<Sock*>(stream)->getUniqueId(),
			&err))
		{
			result_ad.InsertAttr(ATTR_ERROR_STRING, err.getFullText());
			result_ad.InsertAttr(ATTR_ERROR_CODE, err.code());
		} else {
			result_ad.InsertAttr(ATTR_SEC_TOKEN, token);
		}
	}

	stream->encode();
	if (!putClassAd(stream, result_ad) ||
		!stream->end_of_message())
	{
		dprintf(D_FULLDEBUG, "handle_dc_session_token: failed to send response ad to client\n");
		return false;
	}
	return true;
}

int
handle_nop(int, Stream* stream)
{
	if( !stream->end_of_message() ) {
		dprintf( D_FULLDEBUG, "handle_nop: failed to read end of message\n");
		return FALSE;
	}
	return TRUE;
}


int
handle_invalidate_key(int, Stream* stream)
{
	int result = 0;
	std::string key_id;
	std::string their_sinful;
	size_t id_end_idx;

	stream->decode();
	if ( ! stream->code(key_id) ) {
		dprintf ( D_ALWAYS, "DC_INVALIDATE_KEY: unable to receive key id!.\n");
		return FALSE;
	}

	if ( ! stream->end_of_message() ) {
		dprintf ( D_ALWAYS, "DC_INVALIDATE_KEY: unable to receive EOM on key %s.\n", key_id.c_str());
		return FALSE;
	}

	id_end_idx = key_id.find('\n');
	if (id_end_idx != std::string::npos) {
		ClassAd info_ad;
		int info_ad_idx = id_end_idx + 1;
		classad::ClassAdParser parser;
		if (!parser.ParseClassAd(key_id, info_ad, info_ad_idx)) {
			dprintf ( D_ALWAYS, "DC_INVALIDATE_KEY: got unparseable classad\n");
			return FALSE;
		}
		info_ad.LookupString(ATTR_SEC_CONNECT_SINFUL, their_sinful);
		key_id.erase(id_end_idx);
	}

	if (key_id == daemonCore->m_family_session_id) {
		dprintf(D_FULLDEBUG, "DC_INVALIDATE_KEY: Refusing to invalidate family session\n");
		if (!their_sinful.empty()) {
			dprintf(D_ALWAYS, "DC_INVALIDATE_KEY: The daemon at %s says it's not in the same family of Condor daemon processes as me.\n", their_sinful.c_str());
			dprintf(D_ALWAYS, "  If that is in error, you may need to change how the configuration parameter SEC_USE_FAMILY_SESSION is set.\n");
			daemonCore->getSecMan()->m_not_my_family.insert(their_sinful);
		}
	} else {
		result = daemonCore->getSecMan()->invalidateKey(key_id.c_str());
	}
	return result;
}

int
handle_config_val(int idCmd, Stream* stream ) 
{
	char *param_name = NULL, *tmp;

	stream->decode();

	if ( ! stream->code(param_name)) {
		dprintf( D_ALWAYS, "Can't read parameter name\n" );
		free( param_name );
		return FALSE;
	}

	if( ! stream->end_of_message() ) {
		dprintf( D_ALWAYS, "Can't read end_of_message\n" );
		free( param_name );
		return FALSE;
	}

	stream->encode();

	// DC_CONFIG_VAL command has extended behavior not shared by CONFIG_VAL.
	// if param name begins with ? then it is a command name rather than a param name.
	//   ?names[:pattern] - return a set of strings containing the names of all paramters in the param table.
	if ((DC_CONFIG_VAL == idCmd) && ('?' == param_name[0])) {
		int retval = TRUE; // assume success

		const char * pcolon;
		if (is_arg_colon_prefix(param_name, "?names", &pcolon, -1)) {
			const char * restr = ".*";
			if (pcolon) { restr = ++pcolon; }

			// magic pattern that means they want the -summary names.
			if  (starts_with(restr, ".*|.summary")) {
				// this is a request to return the names that condor_config_val -summary would show (mostly)
				// and in the same order that summary would show, we will also return comment lines
				// with filenames as separators
				std::map<int64_t, std::string> names;
				if (param_names_for_summary(names)) {
					int source_id = -999999;
					std::string line;
					line = "#";
					const char * localname = get_mySubSystem()->getLocalName();
					if ( ! localname || !localname[0]) { localname = get_mySubSystem()->getName(); }
					line += localname; line += " "; line += CondorVersion();
					if ( ! stream->code(line)) {
						dprintf( D_ALWAYS, "Can't send ?names (summary) reply for DC_CONFIG_VAL\n" );
						retval = FALSE;
						names.clear(); // skip the loop.
					}
					for (auto it = names.begin(); it != names.end(); ++it) {
						_param_names_sumy_key key; key.all = it->first;
						if (source_id != key.sid) {
							source_id = key.sid;
							const char * source = config_source_by_id(source_id);
							line = "#";
							if (source) { line += source; }
							if ( ! stream->code(line)) {
								dprintf( D_ALWAYS, "Can't send ?names (summary) reply for DC_CONFIG_VAL\n" );
								retval = FALSE;
								break;
							}
						}
						if ( ! stream->code(it->second)) {
							dprintf( D_ALWAYS, "Can't send ?names (summary) reply for DC_CONFIG_VAL\n" );
							retval = FALSE;
							break;
						}
					}
					if (retval && ! stream->end_of_message() ) {
						dprintf( D_ALWAYS, "Can't send end of message for DC_CONFIG_VAL\n" );
						retval = FALSE;
					}
					return retval;
				}
			}

			Regex re; int errcode = 0, erroffset = 0;

			if ( ! re.compile(restr, &errcode, &erroffset, PCRE2_CASELESS)) {
				dprintf( D_ALWAYS, "Can't compile regex for DC_CONFIG_VAL ?names query\n" );
				std::string errmsg; formatstr(errmsg, "!error:regex:%d: error code %d", erroffset, errcode);
				if (!stream->code(errmsg)) {
						dprintf( D_ALWAYS, "and remote side disconnected from use\n" );
				}
				retval = FALSE;
			} else {
				std::vector<std::string> names;
				if (param_names_matching(re, names)) {
					for (int ii = 0; ii < (int)names.size(); ++ii) {
						if ( ! stream->code(names[ii])) {
							dprintf( D_ALWAYS, "Can't send ?names reply for DC_CONFIG_VAL\n" );
							retval = FALSE;
							break;
						}
					}
				} else {
					std::string empty("");
					if ( ! stream->code(empty)) {
						dprintf( D_ALWAYS, "Can't send ?names reply for DC_CONFIG_VAL\n" );
						retval = FALSE;
					}
				}

				if (retval && ! stream->end_of_message() ) {
					dprintf( D_ALWAYS, "Can't send end of message for DC_CONFIG_VAL\n" );
					retval = FALSE;
				}
				names.clear();
			}
		} else if (is_arg_prefix(param_name, "?stats", -1)) {

			struct _macro_stats stats;
			int cQueries = get_config_stats(&stats);
			// for backward compatility, we have to put a single string on the wire
			// before we can put the stats classad.
			std::string queries;
			formatstr(queries, "%d", cQueries);
			if ( ! stream->code(queries)) {
				dprintf(D_ALWAYS, "Can't send param stats for DC_CONFIG_VAL\n");
				retval = false;
			} else {
				ClassAd ad; ad.Clear(); // remove time()
				ad.Assign("Macros", stats.cEntries);
				ad.Assign("Used", stats.cUsed);
				ad.Assign("Referenced", stats.cReferenced);
				ad.Assign("Files", stats.cFiles);
				ad.Assign("StringBytes", stats.cbStrings);
				ad.Assign("TablesBytes", stats.cbTables);
				ad.Assign("Sorted", stats.cSorted);
				if ( ! putClassAd(stream, ad)) {
					dprintf(D_ALWAYS, "Can't send param stats ad for DC_CONFIG_VAL\n");
					retval = false;
				}
			}
			if (retval && ! stream->end_of_message()) {
				retval = false;
			}

		} else { // unrecognised ?command

			std::string errmsg; formatstr(errmsg, "!error:unsup:1: '%s' is not supported", param_name);
			if ( ! stream->code(errmsg)) retval = FALSE;
			if (retval && ! stream->end_of_message()) retval = FALSE;
		}

		free (param_name);
		return retval;
	}

	// DC_CONFIG_VAL command has extended behavior not shared by CONFIG_VAL.
	// in addition to returning. the param() value, it returns consecutive strings containing
	//   <NAME_USED> = <raw_value>
	//   <filename>[, line <num>]
	//   <default_value>
	//note: ", line <num>" is present only when the line number is meaningful
	//and "<default_value>" may be NULL
	if (idCmd == DC_CONFIG_VAL) {
		int retval = TRUE; // assume success

		std::string name_used;
		std::string value;
		const char * def_val = NULL;
		const MACRO_META * pmet = NULL;
		const char * subsys = get_mySubSystem()->getName();
		const char * local_name  = get_mySubSystem()->getLocalName();
		const char * val = param_get_info(param_name, subsys, local_name, name_used, &def_val, &pmet);
		if (name_used.empty()) {
			dprintf( D_FULLDEBUG,
					 "Got DC_CONFIG_VAL request for unknown parameter (%s)\n",
					 param_name );
			// send a NULL to indicate undefined. (val is NULL here)
			if( ! stream->put_nullstr(val) ) {
				dprintf( D_ALWAYS, "Can't send reply for DC_CONFIG_VAL\n" );
				retval = FALSE;
			}
		} else {

			dprintf(D_CONFIG | D_FULLDEBUG, "DC_CONFIG_VAL(%s) def: %s = %s\n", param_name, name_used.c_str(), def_val ? def_val : "NULL");

			if (val) { tmp = expand_param(val, local_name, subsys, 0); } else { tmp = NULL; }
			if( ! stream->code_nullstr(tmp) ) {
				dprintf( D_ALWAYS, "Can't send reply for DC_CONFIG_VAL\n" );
				retval = FALSE;
			}
			if (tmp) {free(tmp);} tmp = NULL;

			upper_case(name_used);
			name_used += " = ";
			if (val) name_used += val;
			if ( ! stream->code(name_used)) {
				dprintf( D_ALWAYS, "Can't send raw reply for DC_CONFIG_VAL\n" );
			}
			param_get_location(pmet, value);
			if ( ! stream->code(value)) {
				dprintf( D_ALWAYS, "Can't send filename reply for DC_CONFIG_VAL\n" );
			}
			if ( ! stream->put_nullstr(def_val)) {
				dprintf( D_ALWAYS, "Can't send default reply for DC_CONFIG_VAL\n" );
			}
			if (pmet->ref_count) { formatstr(value, "%d / %d", pmet->use_count, pmet->ref_count);
			} else  { formatstr(value, "%d", pmet->use_count); }
			if ( ! stream->code(value)) {
				dprintf( D_ALWAYS, "Can't send use count reply for DC_CONFIG_VAL\n" );
			}
		}
		if( ! stream->end_of_message() ) {
			dprintf( D_ALWAYS, "Can't send end of message for DC_CONFIG_VAL\n" );
			retval = FALSE;
		}
		free (param_name);
		return retval;
	}

	tmp = param( param_name );
	if( ! tmp ) {
		dprintf( D_FULLDEBUG, 
				 "Got CONFIG_VAL request for unknown parameter (%s)\n", 
				 param_name );
		free( param_name );
		if( ! stream->put("Not defined") ) {
			dprintf( D_ALWAYS, "Can't send reply for CONFIG_VAL\n" );
			return FALSE;
		}
		if( ! stream->end_of_message() ) {
			dprintf( D_ALWAYS, "Can't send end of message for CONFIG_VAL\n" );
			return FALSE;
		}
		return FALSE;
	} else {
		if( ! stream->code(tmp) ) {
			dprintf( D_ALWAYS, "Can't send reply for CONFIG_VAL\n" );
			free( param_name );
			free( tmp );
			return FALSE;
		}

		free( param_name );
		free( tmp );
		if( ! stream->end_of_message() ) {
			dprintf( D_ALWAYS, "Can't send end of message for CONFIG_VAL\n" );
			return FALSE;
		}
	}
	return TRUE;
}


int
handle_config(int cmd, Stream *stream )
{
	char *admin = NULL, *config = NULL;
	char *to_check = NULL;
	int rval = 0;
	bool failed = false;

	stream->decode();

	if ( ! stream->code(admin) ) {
		dprintf( D_ALWAYS, "Can't read admin string\n" );
		free( admin );
		return FALSE;
	}

	if ( ! stream->code(config) ) {
		dprintf( D_ALWAYS, "Can't read configuration string\n" );
		free( admin );
		free( config );
		return FALSE;
	}

	if( !stream->end_of_message() ) {
		dprintf( D_ALWAYS, "handle_config: failed to read end of message\n");
		return FALSE;
	}
	bool is_meta = admin[0] == '$';
	if( config && config[0] ) {
		#if 0 // tj: we've decide to just fail the assign instead of 'fixing' it. //def WARN_COLON_FOR_PARAM_ASSIGN
		// for backward compat with older senders, change first : to = before we do the assignment.
		for (char * p = config; *p; ++p) { if (*p==':') *p = '='; if (*p=='=') break; }
		#endif
		to_check = is_valid_config_assignment(config);
	} else {
		to_check = strdup(admin);
	}
	if (!is_valid_param_name(to_check + is_meta)) {
		dprintf( D_ALWAYS, "Rejecting attempt to set param with invalid name (%s)\n", (to_check?to_check:"(null)") );
		free(admin); free(config);
		rval = -1;
		failed = true;
	} else if( ! daemonCore->CheckConfigSecurity(to_check, (Sock*)stream) ) {
			// This request is insecure, so don't try to do anything
			// with it.  We can't return yet, since we want to send
			// back an rval indicating the error.
		free( admin );
		free( config );
		rval = -1;
		failed = true;
	} 
	free(to_check);

		// If we haven't hit an error yet, try to process the command  
	if( ! failed ) {
		switch(cmd) {
		case DC_CONFIG_PERSIST:
			rval = set_persistent_config(admin, config);
				// set_persistent_config will free admin and config
				// when appropriate  
			break;
		case DC_CONFIG_RUNTIME:
			rval = set_runtime_config(admin, config);
				// set_runtime_config will free admin and config when
				// appropriate 
			break;
		default:
			dprintf( D_ALWAYS, "unknown DC_CONFIG command!\n" );
			free( admin );
			free( config );
			return FALSE;
		}
	}

	stream->encode();
	if ( ! stream->code(rval) ) {
		dprintf (D_ALWAYS, "Failed to send rval for DC_CONFIG.\n" );
		return FALSE;
	}
	if( ! stream->end_of_message() ) {
		dprintf( D_ALWAYS, "Can't send end of message for DC_CONFIG.\n" );
		return FALSE;
	}

	return (failed ? FALSE : TRUE);
}


#ifndef WIN32
void
unix_sighup(int)
{
	if (daemonCore) {
		daemonCore->Signal_Myself(SIGHUP);
	}
}


void
unix_sigterm(int, siginfo_t *s_info, void *)
{
	if (daemonCore) {
		dprintf(D_ALWAYS, "Caught SIGTERM: si_pid=%d si_uid=%d\n",
		        (int)s_info->si_pid, (int)s_info->si_uid);
		daemonCore->Signal_Myself(SIGTERM);
	}
}


void
unix_sigquit(int, siginfo_t *s_info, void *)
{
	if (daemonCore) {
		dprintf(D_ALWAYS, "Caught SIGQUIT: si_pid=%d si_uid=%d\n",
		        (int)s_info->si_pid, (int)s_info->si_uid);
		daemonCore->Signal_Myself(SIGQUIT);
	}
}


void
unix_sigchld(int)
{
	if (daemonCore) {
		daemonCore->Signal_Myself(SIGCHLD);
	}
}


void
unix_sigusr1(int)
{
	if (daemonCore) {
		daemonCore->Signal_Myself(SIGUSR1);
	}
	
}

void
unix_sigusr2(int)
{
	if (daemonCore) {
		daemonCore->Signal_Myself(SIGUSR2);
	}
}



#endif /* ! WIN32 */



void
dc_reconfig()
{
		// do this first in case anything else depends on DNS
	daemonCore->refreshDNS();

		// Actually re-read the files...  Added by Derek Wright on
		// 12/8/97 (long after this function was first written... 
		// nice goin', Todd).  *grin*

		// We always want to be root when we read config as a daemon
		// we do this because reading config can run scripts and even create files
	{
		TemporaryPrivSentry sentry(PRIV_ROOT);
		int want_meta = get_mySubSystem()->isType(SUBSYSTEM_TYPE_SHADOW) ? 0 : CONFIG_OPT_WANT_META;
		config_ex(CONFIG_OPT_WANT_QUIET | want_meta);
	}

		// See if we're supposed to be allowing core files or not
	if ( doCoreInit ) {
		check_core_files();
	}

	if ( ! disable_default_log) {
		// If we're supposed to be using our own log file, reset that here. 
		if ( logDir ) {
			set_log_dir();
		}

		if( logAppend ) {
			handle_log_append( logAppend );
		}

		// Reinitialize logging system; after all, LOG may have been changed.
		dprintf_config(get_mySubSystem()->getName(), nullptr, 0, log2Arg);
	}

	// again, chdir to the LOG directory so that if we dump a core
	// it will go there.  the location of LOG may have changed, so redo it here.
	drop_core_in_log();

	// Re-read everything from the config file DaemonCore itself cares about.
	// This also cleares the DNS cache.
	daemonCore->reconfig();

	// Clear out the passwd cache.
	clear_passwd_cache();

	// Clear out the cached list of IDTOKEN issuer key names.
	clearIssuerKeyNameCache();

	// Allow us to search for new tokens
	Condor_Auth_Passwd::retry_token_search();

	// Allow us to search for SSL certificate and key
	Condor_Auth_SSL::retry_cert_search();

	// Re-drop the address file, if it's defined, just to be safe.
	drop_addr_file();

		// Re-drop the pid file, if it's requested, just to be safe.
	if( pidFile ) {
		drop_pid_file();
	}

		// If requested to do so in the config file, do a segv now.
		// This is to test our handling/writing of a core file.
	char* ptmp;
	if ( param_boolean_crufty("DROP_CORE_ON_RECONFIG", false) ) {
			// on purpose, derefernce a null pointer.
			ptmp = NULL;
			char segfault;	
			MSC_SUPPRESS_WARNING_FOREVER(6011) // warning about NULL pointer deref.
			segfault = *ptmp; // should blow up here
			if (segfault) {} // Line to avoid compiler warnings.
			ptmp[0] = 'a';
			
			// should never make it to here!
			EXCEPT("FAILED TO DROP CORE");	
	}

		// On reconfig, reset token requests to a pristine state.
	TokenRequest::clearApprovalRules();
	for (auto &entry : g_request_map) {
		entry.second->setFailed();
	}
	TokenRequest::clearTokenRequests();

	// call this daemon's specific main_config()
	dc_main_config();
}

int
handle_dc_sighup(int )
{
	dprintf( D_ALWAYS, "Got SIGHUP.  Re-reading config files.\n" );
	dc_reconfig();
	return TRUE;
}


void
TimerHandler_main_shutdown_fast(int /* tid */)
{
	dc_main_shutdown_fast();
}


int
handle_dc_sigterm(int )
{
	const char * xful = daemonCore->GetPeacefulShutdown() ? "peaceful" : "graceful";
		// Introduces a race condition.
		// What if SIGTERM received while we are here?
	if( !SigtermContinue::should_sigterm_continue() ) {
		dprintf(D_STATUS, "Got SIGTERM, but we've already started %s shutdown.  Ignoring.\n", xful );
		return TRUE;
	}
	SigtermContinue::sigterm_should_not_continue(); // After this

	dprintf(D_STATUS, "Got SIGTERM. Performing %s shutdown.\n", xful);

#if defined(WIN32) && 0
	if ( line_where_service_stopped != 0 ) {
		dprintf(D_ALWAYS,"Line where service stopped = %d\n",
			line_where_service_stopped);
	}
#endif

	if( daemonCore->GetPeacefulShutdown() ) {
		dprintf( D_FULLDEBUG, 
				 "Peaceful shutdown in effect.  No timeout enforced.\n");
	}
	else {
		int timeout = param_integer("SHUTDOWN_GRACEFUL_TIMEOUT", 30 * MINUTE);
		daemonCore->Register_Timer( timeout, 0, 
									TimerHandler_main_shutdown_fast,
									"main_shutdown_fast" );
		dprintf( D_FULLDEBUG, 
				 "Started timer to call main_shutdown_fast in %d seconds\n", 
				 timeout );
	}
	dc_main_shutdown_graceful();
	return TRUE;
}

void
TimerHandler_dc_sigterm(int /* tid */)
{
	handle_dc_sigterm(SIGTERM);
}


int
handle_dc_sigquit(int )
{
	static int been_here = FALSE;
	if( been_here ) {
		dprintf( D_FULLDEBUG, 
				 "Got SIGQUIT, but we've already done fast shutdown.  Ignoring.\n" );
		return TRUE;
	}
	been_here = TRUE;

	dprintf(D_ALWAYS, "Got SIGQUIT.  Performing fast shutdown.\n");
	dc_main_shutdown_fast();
	return TRUE;
}

#ifndef WIN32
// if we fork into the background, this is the write pipe in the child
// and the read pipe for the parent (i.e. the current process)
static int dc_background_pipe = -1;
static bool dc_background_parent_is_master = false;
bool dc_set_background_parent_mode(bool is_master)
{
	bool retval = dc_background_parent_is_master;
	dc_background_parent_is_master = is_master;
	return retval;
}

bool dc_release_background_parent(int status)
{
	if (dc_background_pipe >= 0) {
		int data = status;
		if (sizeof(data) != write(dc_background_pipe, (void*)&data, sizeof(data))) {
			// do what?
		}
		close(dc_background_pipe); dc_background_pipe = -1;
		return true;
	}
	return false;
}
#endif

// Disable default log setup and ignore -l log. Must be called before dc_main()
void DC_Disable_Default_Log() { disable_default_log = true; }

// This is the main entry point for daemon core.  On WinNT, however, we
// have a different, smaller main which checks if "-f" is ommitted from
// the command line args of the condor_master, in which case it registers as 
// an NT service.
int dc_main( int argc, char** argv )
{
	char**	ptr;
	int		command_port = -1;
	char const *daemon_sock_name = NULL;
	int		dcargs = 0;		// number of daemon core command-line args found
	char	*ptmp;
	int		i;
	int		wantsKill = FALSE, wantsQuiet = FALSE;
	bool	done;

	set_priv_initialize();

	condor_main_argc = argc;
	condor_main_argv = (char **)malloc((argc+1)*sizeof(char *));
	for(i=0;i<argc;i++) {
		condor_main_argv[i] = strdup(argv[i]);
	}
	condor_main_argv[i] = NULL;

#ifdef WIN32
	/** Enable support of the %n format in the printf family 
		of functions. */
	_set_printf_count_output(TRUE);
#endif

#ifndef WIN32
		// Set a umask value so we get reasonable permissions on the
		// files we create.  Derek Wright <wright@cs.wisc.edu> 3/3/98
	umask( 022 );

		// Handle Unix signals
		// Block all signals now.  We'll unblock them right before we
		// do the select.
	sigset_t fullset;
	sigfillset( &fullset );
	// We do not want to block the following signals ----
	   sigdelset(&fullset, SIGSEGV);	// so we get a core right away
	   sigdelset(&fullset, SIGABRT);	// so assert() failures drop core right away
	   sigdelset(&fullset, SIGILL);		// so we get a core right away
	   sigdelset(&fullset, SIGBUS);		// so we get a core right away
	   sigdelset(&fullset, SIGFPE);		// so we get a core right away
	   sigdelset(&fullset, SIGTRAP);	// so gdb works when it uses SIGTRAP
	sigprocmask( SIG_SETMASK, &fullset, NULL );

		// Install these signal handlers with a default mask
		// of all signals blocked when we're in the handlers.
	install_sig_action_with_mask(SIGQUIT, &fullset, unix_sigquit);
	install_sig_handler_with_mask(SIGHUP, &fullset, unix_sighup);
	install_sig_action_with_mask(SIGTERM, &fullset, unix_sigterm);
	install_sig_handler_with_mask(SIGCHLD, &fullset, unix_sigchld);
	install_sig_handler_with_mask(SIGUSR1, &fullset, unix_sigusr1);
	install_sig_handler_with_mask(SIGUSR2, &fullset, unix_sigusr2);
	install_sig_handler(SIGPIPE, SIG_IGN );

#endif // of ifndef WIN32

	_condor_myServiceName = argv[0];
	// set myName to be argv[0] with the path stripped off
	myName = condor_basename(argv[0]);
	myFullName = getExecPath();
	if( ! myFullName ) {
			// if getExecPath() didn't work, the best we can do is try
			// saving argv[0] and hope for the best...
		if( argv[0][0] == '/' ) {
				// great, it's already a full path
			myFullName = strdup(argv[0]);
		} else {
				// we don't have anything reliable, so forget it.  
			myFullName = NULL;
		}
	}

		// call out to the handler for pre daemonCore initialization
		// stuff so that our client side can do stuff before we start
		// messing with argv[]
	if ( dc_main_pre_dc_init ) {
		dc_main_pre_dc_init( argc, argv );
	}

		// Make sure this is set, since DaemonCore needs it for all
		// sorts of things, and it's better to clearly EXCEPT here
		// than to seg fault down the road...
	if( ! get_mySubSystem() ) {
		EXCEPT( "Programmer error: get_mySubSystem() is NULL!" );
	}
	if( !get_mySubSystem()->isValid() ) {
		get_mySubSystem()->printf( );
		EXCEPT( "Programmer error: get_mySubSystem() info is invalid(%s,%ld,%s)!",
				get_mySubSystem()->getName(),
				get_mySubSystem()->getType(),
				get_mySubSystem()->getTypeName() );
	}

	if ( !dc_main_init ) {
		EXCEPT( "Programmer error: dc_main_init is NULL!" );
	}
	if ( !dc_main_config ) {
		EXCEPT( "Programmer error: dc_main_config is NULL!" );
	}
	if ( !dc_main_shutdown_fast ) {
		EXCEPT( "Programmer error: dc_main_shutdown_fast is NULL!" );
	}
	if ( !dc_main_shutdown_graceful ) {
		EXCEPT( "Programmer error: dc_main_shutdown_graceful is NULL!" );
	}

	// strip off any daemon-core specific command line arguments
	// from the front of the command line.
	i = 0;
	done = false;

	for(ptr = argv + 1; *ptr && (i < argc - 1); ptr++,i++) {
		if(ptr[0][0] != '-') {
			break;
		}

			/*
			  NOTE!  If you're adding a new command line argument to
			  this switch statement, YOU HAVE TO ADD IT TO THE #ifdef
			  WIN32 VERSION OF "main()" NEAR THE END OF THIS FILE,
			  TOO.  You should actually deal with the argument here,
			  but you need to add it there to be skipped over because
			  of NT weirdness with starting as a service, etc.  Also,
			  please add your argument in alphabetical order, so we
			  can maintain some semblance of order in here, and make
			  it easier to keep the two switch statements in sync.
			  Derek Wright <wright@cs.wisc.edu> 11/11/99
			*/
		switch(ptr[0][1]) {
		case 'a':		// Append to the log file name, or if "a2" capture the 2nd log argument
			ptr++;
			if( ptr && *ptr ) {
				if (ptr[-1][2] == '2') { // was it -append? or -a2 ?
					log2Arg = *ptr;
				} else {
					logAppend = *ptr;
				}
				dcargs += 2;
			} else {
				fprintf( stderr, 
						 "DaemonCore: ERROR: -append needs another argument.\n" );
				fprintf( stderr, 
					 "   Please specify a string to append to our log's filename.\n" );
				exit( 1 );
			}
			break;
		case 'b':		// run in Background (default for condor_master)
			Foreground = 0;
			dcargs++;
			break;
		case 'c':		// specify directory where Config file lives
			ptr++;
			if( ptr && *ptr ) {
				ptmp = *ptr;
				dcargs += 2;

				SetEnv("CONDOR_CONFIG", ptmp);
			} else {
				fprintf( stderr, 
						 "DaemonCore: ERROR: -config needs another argument.\n" );
				fprintf( stderr, 
						 "   Please specify the filename of the config file.\n" );
				exit( 1 );
			}				  
			break;
		case 'd':		// Dynamic local directories
			if( strcmp( "-d", *ptr ) && strcmp( "-dynamic", *ptr ) ) {
				done = true;
				break;
			}
			else {
				DynamicDirs = true;
				dcargs++;
				break;
			}
		case 'f':		// run in Foreground (default for all daemons other than condor_master)
			Foreground = 1;
			dcargs++;
			break;
#ifndef WIN32
		case 'k':		// Kill the pid in the given pid file
			ptr++;
			if( ptr && *ptr ) {
				pidFile = *ptr;
				wantsKill = TRUE;
				dcargs += 2;
			} else {
				fprintf( stderr, 
						 "DaemonCore: ERROR: -kill needs another argument.\n" );
				fprintf( stderr, 
				  "   Please specify a file that holds the pid you want to kill.\n" );
				exit( 1 );
			}
			break;
#endif
		case 'l':	// -local-name or -log
			// specify local name
			if ( strcmp( &ptr[0][1], "local-name" ) == 0 ) {
				ptr++;
				if( ptr && *ptr ) {
					get_mySubSystem()->setLocalName( *ptr );
					dcargs += 2;
				}
				else {
					fprintf( stderr, "DaemonCore: ERROR: "
							 "-local-name needs another argument.\n" );
					fprintf( stderr, 
							 "   Please specify the local config to use.\n" );
					exit( 1 );
				}
			}

			// Disable the creation of a normal log file in the log directory
			else if (strcmp(&ptr[0][1], "ld") == MATCH) {
				dcargs++;
				disable_default_log = true;
			}

			// specify Log directory 
			else {
				ptr++;
				if( ptr && *ptr ) {
					logDir = *ptr;
					dcargs += 2;
				} else {
					fprintf( stderr, "DaemonCore: ERROR: -log needs another "
							 "argument\n" );
					exit( 1 );
				}
			}
			break;

		case 'h':		// -http <port> : specify port for HTTP and SOAP requests
			if ( ptr[0][2] && ptr[0][2] == 't' ) {
					// specify an HTTP port
				ptr++;
				if( ptr && *ptr ) {
					fprintf( stderr, 
							 "DaemonCore: ERROR: -http no longer accepted.\n" );
					exit( 1 );
				}
			} else {
					// it's not -http, so do NOT consume this arg!!
					// in fact, we're done w/ DC args, since it's the
					// first option we didn't recognize.
				done = true;
			}
			break;
		case 'p':		// use well-known Port for command socket, or
				        // specify a Pidfile to drop your pid into
			if( ptr[0][2] && ptr[0][2] == 'i' ) {
					// Specify a Pidfile
				ptr++;
				if( ptr && *ptr ) {
					pidFile = *ptr;
					dcargs += 2;
				} else {
					fprintf( stderr, 
							 "DaemonCore: ERROR: -pidfile needs another argument.\n" );
					fprintf( stderr, 
							 "   Please specify a filename to store the pid.\n" );
					exit( 1 );
				}
			} else {
					// use well-known Port for command socket				
					// note: "-p 0" means _no_ command socket
				ptr++;
				if( ptr && *ptr ) {
					command_port = atoi( *ptr );
					dcargs += 2;
				} else {
					fprintf( stderr, 
							 "DaemonCore: ERROR: -port needs another argument.\n" );
					fprintf( stderr, 
					   "   Please specify the port to use for the command socket.\n" );

					exit( 1 );
				}
			}
			break;
		case 'q':		// Quiet output
			wantsQuiet = TRUE;
			dcargs++;
			break;			
		case 'r':	   // Run for <arg> minutes, then gracefully exit
			ptr++;
			if( ptr && *ptr ) {
				runfor = atoi( *ptr );
				dcargs += 2;
			}
			else {
				fprintf( stderr, 
						 "DaemonCore: ERROR: -runfor needs another argument.\n" );
				fprintf( stderr, 
				   "   Please specify the number of minutes to run for.\n" );

				exit( 1 );
			}
			//call Register_Timer below after intialized...
			break;
		case 's':
				// the c-gahp uses -s, so for backward compatibility,
				// do not allow abbreviations of -sock
			if( strcmp("-sock",*ptr) ) {
				done = true;
				break;
			}
			else {
				ptr++;
				if( *ptr ) {
					daemon_sock_name = *ptr;
					dcargs += 2;
				} else {
					fprintf( stderr, 
							 "DaemonCore: ERROR: -sock needs another argument.\n" );
					fprintf( stderr, 
							 "   Please specify a socket name.\n" );
					exit( 1 );
				}
			}
			break;
		case 't':		// log to Terminal (stderr)
			Termlog = 1;
			dcargs++;
			break;
		case 'v':		// display Version info and exit
			printf( "%s\n%s\n", CondorVersion(), CondorPlatform() );
			exit(0);
			break;
		default:
			done = true;
			break;	
		}
		if ( done ) {
			break;		// break out of for loop
		}
	}

	// using "-t" also implicitly sets "-f"; i.e. only to stderr in the foreground
	if ( Termlog ) {
		Foreground = 1;
	}

	// call config so we can call param.  
	// Try to minimize shadow footprint by not loading the metadata from the config file
	int config_options = get_mySubSystem()->isType(SUBSYSTEM_TYPE_SHADOW) ? 0 : CONFIG_OPT_WANT_META;
	//config_options |= get_mySubSystem()->isType(SUBSYSTEM_TYPE_MASTER) ? CONFIG_OPT_DEPRECATION_WARNINGS : 0;
	if (wantsQuiet) { config_options |= CONFIG_OPT_WANT_QUIET; }
	config_ex(config_options);


		// See if we're supposed to be allowing core files or not
	if ( doCoreInit ) {
		check_core_files();
	}

		// If we want to kill something, do that now.
	if( wantsKill ) {
		do_kill();
	}

	if (!disable_default_log && !DynamicDirs) {

			// We need to setup logging.  Normally, we want to do this
			// before the fork(), so that if there are problems and we
			// fprintf() to stderr, we'll still see them.  However, if
			// we want DynamicDirs, we can't do this yet, since we
			// need our pid to specify the log directory...

		// If want to override what's set in our config file, we've
		// got to do that here, between where we config and where we
		// setup logging.  Note: we also have to do this in reconfig.
		if( logDir ) {
			set_log_dir();
		}

		// If we're told on the command-line to append something to
		// the name of our log file, we do that here, so that when we
		// setup logging, we get the right filename.  -Derek Wright
		// 11/20/98
		if( logAppend ) {
			handle_log_append( logAppend );
		}
		
			// Actually set up logging.
		if(Termlog)
			dprintf_set_tool_debug(get_mySubSystem()->getName(), 0);
		else
			dprintf_config(get_mySubSystem()->getName(), nullptr, 0, log2Arg);
	}

		// run as condor 99.9% of the time, so studies tell us.
	set_condor_priv();

	// we want argv[] stripped of daemoncore options
	ptmp = argv[0];			// save a temp pointer to argv[0]
	argv = --ptr;			// make space for argv[0]
	argv[0] = ptmp;			// set argv[0]
	argc -= dcargs;
	if ( argc < 1 )
		argc = 1;

	// Arrange to run in the background.
	// SPECIAL CASE: if this is the MASTER, and we are to run in the background,
	// then register ourselves as an NT Service.
	if (!Foreground) {
#ifdef WIN32
		// Disconnect from the console
		FreeConsole();
#else	// UNIX

		// before we fork, make a pipe that the child can use to tell the parent
		// that it's done with initialization.
		int bgpipe[2] = { -1,-1 };
		if (pipe(bgpipe) == -1) {
			fprintf(stderr, "could not open background pipe\n");
		}

		// on unix, background means just fork ourselves
		if ( fork() ) {
			int child_status = 0;
			if (bgpipe[1] >= 0) {
				close(bgpipe[1]); // close write end of pipe
				dc_background_pipe = bgpipe[0];
				if (sizeof(child_status) != read(dc_background_pipe, (void*)&child_status, sizeof(child_status))) {
					child_status = 0; // child never wrote to the pipe!
				}
				close(dc_background_pipe); dc_background_pipe = -1;
				if (child_status != 0) { fprintf(stderr, "forked condor_master status is %d\n", child_status); }
			}
			// parent
			exit(child_status);
		}
		if (bgpipe[0] >= 0) {
			close(bgpipe[0]); // close read end of pipe
			dc_background_pipe = bgpipe[1]; // save hte write end of the pipe for later
		}

		// And close stdin, out, err if we are the MASTER.

		// NRL 2006-08-10: Here's what and why we're doing this.....
		//
		// In days of yore, we'd simply close FDs 0,1,2 and be done
		// with it.  Unfortunately, some 3rd party tools were writing
		// directly to these FDs.  Now, you might not that that this
		// would be a problem, right?  Unfortunately, once you close
		// an FD (say, 1 or 2), then next safe_open_wrapper() or socket()
		// call can / will now return 1 (or 2).  So, when libfoo.a thinks
		// it fprintf()ing an error message to fd 2 (the FD behind
		// stderr), it's actually sending that message over a socket
		// to a process that has no idea how to handle it.
		//
		// So, here's what we do.  We open /dev/null, and then use
		// dup2() to copy this "safe" fd to these decriptors.  Note
		// that if you don't open it with O_RDWR, you can cause writes
		// to the FD to return errors.
		//
		// Finally, we only do this in the master.  Why?  Because
		// Create_Process() has similar logic, so when the master
		// starts, say, condor_startd, the Create_Process() logic
		// takes over and does The Right Thing(tm).
		//
		// Regarding the std in/out/err streams (the FILE *s, not the
		// FDs), we leave those open.  Otherwise, if libfoo.a tries to
		// fwrite to stderr, it would get back an error from fprintf()
		// (or fwrite(), etc.).  If it checks this, it might Do
		// Something Bad(tm) like abort() -- who knows.  In any case,
		// it seems safest to just leave them open but attached to
		// /dev/null.

		if ( get_mySubSystem()->isType( SUBSYSTEM_TYPE_MASTER ) ) {
			int	fd_null = safe_open_wrapper_follow( NULL_FILE, O_RDWR );
			if ( fd_null < 0 ) {
				fprintf( stderr, "Unable to open %s: %s\n", NULL_FILE, strerror(errno) );
				dprintf( D_ALWAYS, "Unable to open %s: %s\n", NULL_FILE, strerror(errno) );
			}
			int	fd;
			for( fd=0;  fd<=2;  fd++ ) {
				close( fd );
				if ( ( fd_null >= 0 ) && ( fd_null != fd ) &&
					 ( dup2( fd_null, fd ) < 0 )  ) {
					dprintf( D_ALWAYS, "Error dup2()ing %s -> %d: %s\n",
							 NULL_FILE, fd, strerror(errno) );
				}
			}
			// Close the /dev/null descriptor _IF_ it's not stdin/out/err
			if ( fd_null > 2 ) {
				close( fd_null );
			}
		}
		// and detach from the controlling tty
		detach();

#endif	// of else of ifdef WIN32
	}	// if if !Foreground

	// See if the config tells us to wait on startup for a debugger to attach.
	std::string debug_wait_param;
	formatstr(debug_wait_param, "%s_DEBUG_WAIT", get_mySubSystem()->getName() );
	if (param_boolean(debug_wait_param.c_str(), false, false)) {
		volatile int debug_wait = 1;
		dprintf(D_ALWAYS,
				"%s is TRUE, waiting for debugger to attach to pid %d.\n", 
				debug_wait_param.c_str(), (int)::getpid());
		#ifndef WIN32
			// since we are about to delay for an arbitrary amount of time, write to the background pipe
			// so that our forked parent can exit.
			dc_release_background_parent(0);
		#endif
		while (debug_wait) {
			sleep(1);
		}
	}

#ifdef WIN32
	formatstr(debug_wait_param, "%s_WAIT_FOR_DEBUGGER", get_mySubSystem()->getName() );
	int wait_for_win32_debugger = param_integer(debug_wait_param.c_str(), 0);
	if (wait_for_win32_debugger) {
		UINT ms = GetTickCount() - 10;
		BOOL is_debugger = IsDebuggerPresent();
		while ( ! is_debugger) {
			if (GetTickCount() > ms) {
				dprintf(D_ALWAYS,
						"%s is %d, waiting for debugger to attach to pid %d.\n", 
						debug_wait_param.c_str(), wait_for_win32_debugger, GetCurrentProcessId());
				ms = GetTickCount() + (1000 * 60 * 1); // repeat message every 1 minute
			}
			sleep(10);
			is_debugger = IsDebuggerPresent();
		}
	}
#endif

		// Now that we've potentially forked, we have our real pid, so
		// we can instantiate a daemon core and it'll have the right
		// pid. 
	daemonCore = new DaemonCore();

	if (!disable_default_log && DynamicDirs) {
			// If we want to use dynamic dirs for log, spool and
			// execute, we now have our real pid, so we can actually
			// give it the correct name.

		handle_dynamic_dirs();

		if( logAppend ) {
			handle_log_append( logAppend );
		}
		
			// Actually set up logging.
		dprintf_config(get_mySubSystem()->getName(), nullptr, 0, log2Arg);
	}

		// Now that we have the daemonCore object, we can finally
		// know what our pid is, so we can print out our opening
		// banner.  Plus, if we're using dynamic dirs, we have dprintf
		// configured now, so the dprintf()s will work.
	dprintf(D_ALWAYS,"******************************************************\n");
	dprintf(D_ALWAYS,"** %s (%s_%s) STARTING UP\n",
			myName, MY_CONDOR_NAME_UC, get_mySubSystem()->getName() );
	if( myFullName ) {
		dprintf( D_ALWAYS, "** %s\n", myFullName );
		free( myFullName );
		myFullName = NULL;
	}
	dprintf(D_ALWAYS,"** %s\n", get_mySubSystem()->getString() );
	dprintf(D_ALWAYS,"** Configuration: subsystem:%s local:%s class:%s\n",
			get_mySubSystem()->getName(),
			get_mySubSystem()->getLocalName("<NONE>"),
			get_mySubSystem()->getClassName( )
			);
	dprintf(D_ALWAYS,"** %s\n", CondorVersion());
	dprintf(D_ALWAYS,"** %s\n", CondorPlatform());
	dprintf(D_ALWAYS,"** PID = %lu", (unsigned long) daemonCore->getpid());
#ifdef WIN32
	dprintf(D_ALWAYS | D_NOHEADER,"\n");
#else
	dprintf(D_ALWAYS | D_NOHEADER, " RealUID = %u\n", getuid());
#endif

	time_t log_last_mod_time = dprintf_last_modification();
	if ( log_last_mod_time <= 0 ) {
		dprintf(D_ALWAYS,"** Log last touched time unavailable (%s)\n",
				strerror((int)-log_last_mod_time));
	} else {
		struct tm *tm = localtime( &log_last_mod_time );
		dprintf(D_ALWAYS,"** Log last touched %d/%d %02d:%02d:%02d\n",
				tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min,
				tm->tm_sec);
	}

	dprintf(D_ALWAYS,"******************************************************\n");

	if (global_config_source != "") {
		dprintf(D_ALWAYS, "Using config source: %s\n", 
				global_config_source.c_str());
	} else {
		const char* env_name = ENV_CONDOR_CONFIG;
		char* env = getenv( env_name );
		if( env ) {
			dprintf(D_ALWAYS, 
					"%s is set to '%s', not reading a config file\n",
					env_name, env );
		}
	}

	if (!local_config_sources.empty()) {
		dprintf(D_ALWAYS, "Using local config sources: \n");
		for (const auto& source: local_config_sources) {
			dprintf(D_ALWAYS, "   %s\n", source.c_str() );
		}
	}
	_macro_stats stats;
	get_config_stats(&stats);
	dprintf(D_ALWAYS, "config Macros = %d, Sorted = %d, StringBytes = %d, TablesBytes = %d\n",
				stats.cEntries, stats.cSorted, stats.cbStrings, stats.cbTables);


	// TJ: Aug/2013 we are going to turn on classad caching by default in 8.1.1 we want to see in the log that its on.
	dprintf (D_ALWAYS, "CLASSAD_CACHING is %s\n", param_boolean("ENABLE_CLASSAD_CACHING", false) ? "ENABLED" : "OFF");

		// chdir() into our log directory so if we drop core, that's
		// where it goes.  We also do some NT-specific stuff in here.
	drop_core_in_log();

#if defined(HAVE_BACKTRACE)
	// On linux, the first call to backtrace() may trigger a dlopen() of
	// libgcc, which includes a malloc(). This is not safe in a signal
	// handler triggered by memory corruption, so call it here.
	void* trace[10];
	backtrace(trace, 10);
#endif

	// write dprintf's contribution to the daemon header.
	dprintf_print_daemon_header();

#ifdef WIN32
		// On NT, we need to make certain we have a console allocated,
		// since many standard mechanisms do not work without a console
		// attached [like popen(), stdin/out/err, GenerateConsoleEvent].
		// There are several reasons why we may not have a console
		// right now: a) we are running as a service, and services are 
		// born without a console, or b) the user did not specify "-f" 
		// or "-t", and thus we called FreeConsole() above.

		// for now, don't create a console for the kbdd, as that daemon
		// is now a win32 app and not a console app.
	BOOL is_kbdd = (0 == strcmp(get_mySubSystem()->getName(), "KBDD"));
	if(!is_kbdd)
		AllocConsole();
#endif

#ifndef WIN32
		// Now that logging is setup, create a pipe to deal with unix 
		// async signals.  We do this after logging is setup so that
		// we can EXCEPT (and really log something) on failure...
	if ( pipe(daemonCore->async_pipe) == -1 ||
		 fcntl(daemonCore->async_pipe[0],F_SETFL,O_NONBLOCK) == -1 ||
		 fcntl(daemonCore->async_pipe[1],F_SETFL,O_NONBLOCK) == -1 ) {
			EXCEPT("Failed to create async pipe");
	}
#ifdef LINUX
	// By default, Linux now allocates 16 4kbyte pages for each pipe,
	// until the sum of all pages used for pipe buffers for a user hits
	// /proc/sys/fs/pipe-user-pages-soft , which defaults to 16k.
	// We create one of these pipes to forward signals
	// for each daemon core process, which means that once 1,000 shadows
	// are running for a user, Linux will switch to using one 4k page
	// per pipe.  This particular pipe we just created to forward signals
	// from a signal handler to the select loop doesn't need to be that
	// big, especially as we just turned on non-blocking i/o above.
	// Reduce the size of this pipe to one page, to save pages for
	// other pipes which really need bigger buffers.

	int defaultPipeSize = 0;
	int smallPipeSize = 256; // probably will get rounded up to 4096

	defaultPipeSize = fcntl(daemonCore->async_pipe[0], F_GETPIPE_SZ);
	fcntl(daemonCore->async_pipe[0], F_SETPIPE_SZ, smallPipeSize);
	smallPipeSize = fcntl(daemonCore->async_pipe[0], F_GETPIPE_SZ);
	dprintf(D_FULLDEBUG, "Internal pipe for signals resized to %d from %d\n", smallPipeSize, defaultPipeSize);
#endif

#else
	if ( daemonCore->async_pipe[1].connect_socketpair(daemonCore->async_pipe[0])==false )
	{
		EXCEPT("Failed to create async pipe socket pair");
	}
#endif

	if ( dc_main_pre_command_sock_init ) {
		dc_main_pre_command_sock_init();
	}

		/* NOTE re main_pre_command_sock_init:
		  *
		  * The Master uses main_pre_command_sock_init to check for
		  * the InstanceLock. Any operation that is distructive before
		  * this point will possibly change the state/environment for
		  * an already running master.
		  *
		  *  In the pidfile case, a second Master will start, drop its
		  *  pid in the file and then exit, see GT343.
		  *
		  *  In the DAEMON_AD_FILE case, which isn't actually a
		  *  problem as of this comment because the Master does not
		  *  drop one, there would be a period of time when the file
		  *  would be missing.
		  *
		  * PROBLEM: TRUNC_*_LOG_ON_OPEN=TRUE will get to delete the
		  * log file before getting here.
		  */

		// Now that we have our pid, we could dump our pidfile, if we
		// want it. 
	if( pidFile ) {
		drop_pid_file();
	}

		// Avoid possibility of stale info sticking around from previous run.
		// For example, we had problems in 7.0.4 and earlier with reconnect
		// shadows in parallel universe reading the old schedd ad file.
	kill_daemon_ad_file();


		// SETUP COMMAND SOCKET
	daemonCore->SetDaemonSockName( daemon_sock_name );
	daemonCore->InitDCCommandSocket( command_port );

		// Install DaemonCore signal handlers common to all daemons.
	daemonCore->Register_Signal( SIGHUP, "SIGHUP", 
								 handle_dc_sighup,
								 "handle_dc_sighup()" );
	daemonCore->Register_Signal( SIGQUIT, "SIGQUIT", 
								 handle_dc_sigquit,
								 "handle_dc_sigquit()" );
	daemonCore->Register_Signal( SIGTERM, "SIGTERM", 
								 handle_dc_sigterm,
								 "handle_dc_sigterm()" );

	daemonCore->Register_Signal( DC_SERVICEWAITPIDS, "DC_SERVICEWAITPIDS",
								(SignalHandlercpp)&DaemonCore::HandleDC_SERVICEWAITPIDS,
								"HandleDC_SERVICEWAITPIDS()",daemonCore);
#ifndef WIN32
	daemonCore->Register_Signal( SIGCHLD, "SIGCHLD",
								 (SignalHandlercpp)&DaemonCore::HandleDC_SIGCHLD,
								 "HandleDC_SIGCHLD()",daemonCore);
#endif

		// Install DaemonCore timers common to all daemons.

		//if specified on command line, set timer to gracefully exit
	if ( runfor ) {
		daemon_stop_time = time(NULL)+runfor*60;
		daemonCore->Register_Timer( runfor * 60, 0, 
				TimerHandler_dc_sigterm, "handle_dc_sigterm" );
		dprintf(D_ALWAYS,"Registered Timer for graceful shutdown in %d minutes\n",
				runfor );
	}
	else {
		daemon_stop_time = 0;
	}

#ifndef WIN32
		// This timer checks if our parent is dead; if so, we shutdown.
		// Note: we do not want the master to exibit this behavior!
		// We only do this on Unix; on NT we watch our parent via passing our
		// parent pid into the pidwatcher thread, then shutting down the NT child
		// in DaemonCore::HandleProcessExit().
		//
	if ( ! get_mySubSystem()->isType(SUBSYSTEM_TYPE_MASTER) ) {
		daemonCore->Register_Timer( 15, 120, 
				check_parent, "check_parent" );
	}
#endif

	daemonCore->Register_Timer( 0,
				dc_touch_log_file, "dc_touch_log_file" );

	daemonCore->Register_Timer( 0,
				dc_touch_lock_files, "dc_touch_lock_files" );

	daemonCore->Register_Timer( 0, 5 * 60,
				check_session_cache, "check_session_cache" );

	daemonCore->Register_Timer( 0, 60,
				cleanup_request_map, "cleanup_request_map" );

	// set the timer for half the session duration, 
	// since we retain the old cookie. Also make sure
	// the value is atleast 1.
	int cookie_refresh = (param_integer("SEC_DEFAULT_SESSION_DURATION", 3600)/2)+1;

	daemonCore->Register_Timer( 0, cookie_refresh, 
				handle_cookie_refresh, "handle_cookie_refresh");
 

	if( get_mySubSystem()->isType( SUBSYSTEM_TYPE_MASTER ) ||
		get_mySubSystem()->isType( SUBSYSTEM_TYPE_COLLECTOR ) ||
		get_mySubSystem()->isType( SUBSYSTEM_TYPE_NEGOTIATOR ) ||
		get_mySubSystem()->isType( SUBSYSTEM_TYPE_SCHEDD ) ||
		get_mySubSystem()->isType( SUBSYSTEM_TYPE_STARTD ) ) {
        daemonCore->monitor_data.EnableMonitoring();
    }

	std::vector<DCpermission> allow_perms{ALLOW};


		// Install DaemonCore command handlers common to all daemons.
	daemonCore->Register_Command( DC_RECONFIG, "DC_RECONFIG",
								  handle_reconfig,
								  "handle_reconfig()", ADMINISTRATOR );

	daemonCore->Register_Command( DC_RECONFIG_FULL, "DC_RECONFIG_FULL",
								  handle_reconfig,
								  "handle_reconfig()", ADMINISTRATOR );

	daemonCore->Register_Command( DC_CONFIG_VAL, "DC_CONFIG_VAL",
								  handle_config_val,
								  "handle_config_val()", READ );
		// Deprecated name for it.
	daemonCore->Register_Command( CONFIG_VAL, "CONFIG_VAL",
								  handle_config_val,
								  "handle_config_val()", READ );

		// The handler for setting config variables does its own
		// authorization, so these two commands should be registered
		// as "ALLOW" and the handler will do further checks.
	daemonCore->Register_Command( DC_CONFIG_PERSIST, "DC_CONFIG_PERSIST",
								  handle_config,
								  "handle_config()", DAEMON,
								  false, 0, &allow_perms);

	daemonCore->Register_Command( DC_CONFIG_RUNTIME, "DC_CONFIG_RUNTIME",
								  handle_config,
								  "handle_config()", DAEMON,
								  false, 0, &allow_perms);

	daemonCore->Register_Command( DC_OFF_FAST, "DC_OFF_FAST",
								  handle_off_fast,
								  "handle_off_fast()", ADMINISTRATOR );

	daemonCore->Register_Command( DC_OFF_GRACEFUL, "DC_OFF_GRACEFUL",
								  handle_off_graceful,
								  "handle_off_graceful()", ADMINISTRATOR );

	daemonCore->Register_Command( DC_OFF_FORCE, "DC_OFF_FORCE",
								  handle_off_force,
								  "handle_off_force()", ADMINISTRATOR );

	daemonCore->Register_Command( DC_OFF_PEACEFUL, "DC_OFF_PEACEFUL",
								  handle_off_peaceful,
								  "handle_off_peaceful()", ADMINISTRATOR );

	daemonCore->Register_Command( DC_SET_PEACEFUL_SHUTDOWN, "DC_SET_PEACEFUL_SHUTDOWN",
								  handle_set_peaceful_shutdown,
								  "handle_set_peaceful_shutdown()", ADMINISTRATOR );

	daemonCore->Register_Command( DC_SET_FORCE_SHUTDOWN, "DC_SET_FORCE_SHUTDOWN",
								  handle_set_force_shutdown,
								  "handle_set_force_shutdown()", ADMINISTRATOR );

		// DC_NOP is for waking up select.  There is no need for
		// security here, because anyone can wake up select anyway.
		// This command is also used to gracefully close a socket
		// that has been registered to read a command.
	daemonCore->Register_Command( DC_NOP, "DC_NOP",
								  handle_nop,
								  "handle_nop()", ALLOW );

		// the next several commands are NOPs registered at all permission
		// levels, for security testing and diagnostics.  for now they all
		// invoke the same function, but how they are authorized before calling
		// it may vary depending on the configuration
	daemonCore->Register_Command( DC_NOP_READ, "DC_NOP_READ",
								  handle_nop,
								  "handle_nop()", READ );

	daemonCore->Register_Command( DC_NOP_WRITE, "DC_NOP_WRITE",
								  handle_nop,
								  "handle_nop()", WRITE );

	daemonCore->Register_Command( DC_NOP_NEGOTIATOR, "DC_NOP_NEGOTIATOR",
								  handle_nop,
								  "handle_nop()", NEGOTIATOR );

	daemonCore->Register_Command( DC_NOP_ADMINISTRATOR, "DC_NOP_ADMINISTRATOR",
								  handle_nop,
								  "handle_nop()", ADMINISTRATOR );

	// CRUFT The old OWNER authorization level was merged into
	//   ADMINISTRATOR in HTCondor 9.9.0. For older clients, we still
	//   recognize the DC_NOP_OWNER command.
	//   We should remove it eventually.
	daemonCore->Register_Command( DC_NOP_OWNER, "DC_NOP_OWNER",
								  handle_nop,
								  "handle_nop()", ADMINISTRATOR );

	daemonCore->Register_Command( DC_NOP_CONFIG, "DC_NOP_CONFIG",
								  handle_nop,
								  "handle_nop()", CONFIG_PERM );

	daemonCore->Register_Command( DC_NOP_DAEMON, "DC_NOP_DAEMON",
								  handle_nop,
								  "handle_nop()", DAEMON );

	daemonCore->Register_Command( DC_NOP_ADVERTISE_STARTD, "DC_NOP_ADVERTISE_STARTD",
								  handle_nop,
								  "handle_nop()", ADVERTISE_STARTD_PERM );

	daemonCore->Register_Command( DC_NOP_ADVERTISE_SCHEDD, "DC_NOP_ADVERTISE_SCHEDD",
								  handle_nop,
								  "handle_nop()", ADVERTISE_SCHEDD_PERM );

	daemonCore->Register_Command( DC_NOP_ADVERTISE_MASTER, "DC_NOP_ADVERTISE_MASTER",
								  handle_nop,
								  "handle_nop()", ADVERTISE_MASTER_PERM );


	daemonCore->Register_Command( DC_FETCH_LOG, "DC_FETCH_LOG",
								  handle_fetch_log,
								  "handle_fetch_log()", ADMINISTRATOR );

	daemonCore->Register_Command( DC_PURGE_LOG, "DC_PURGE_LOG",
								  handle_fetch_log,
								  "handle_fetch_log_history_purge()", ADMINISTRATOR );

	daemonCore->Register_Command( DC_INVALIDATE_KEY, "DC_INVALIDATE_KEY",
								  handle_invalidate_key,
								  "handle_invalidate_key()", ALLOW );

		// DC_QUERY_INSTANCE is for determining if you are talking to the correct instance of a daemon.
		// There is no need for security here, the use case is a lambda function in AWS which won't have
		// authorization to do anything else.
	daemonCore->Register_Command( DC_QUERY_INSTANCE, "DC_QUERY_INSTANCE",
								  handle_dc_query_instance,
								  "handle_dc_query_instance()", ALLOW );

		//
		// The time offset command is used to figure out what
		// the range of the clock skew is between the daemon code and another
		// entity calling into us
		//
	daemonCore->Register_Command( DC_TIME_OFFSET, "DC_TIME_OFFSET",
								  time_offset_receive_cedar_stub,
								  "time_offset_cedar_stub", DAEMON );

		//
		// Request a token that can be used to authenticat / authorize a future
		// session using the TOKEN protocol.
		//
	daemonCore->Register_CommandWithPayload( DC_GET_SESSION_TOKEN, "DC_GET_SESSION_TOKEN",
								handle_dc_session_token,
								"handle_dc_session_token()", DAEMON,
								  false, 0, &allow_perms );

		//
		// Start a token request workflow.
		//
	daemonCore->Register_CommandWithPayload( DC_START_TOKEN_REQUEST, "DC_START_TOKEN_REQUEST",
								handle_dc_start_token_request,
								"handle_dc_start_token_request()", DAEMON,
								  false, 0, &allow_perms );

		//
		// Poll for token request completion.
		//
	daemonCore->Register_CommandWithPayload( DC_FINISH_TOKEN_REQUEST, "DC_FINISH_TOKEN_REQUEST",
								handle_dc_finish_token_request,
								"handle_dc_finish_token_request()", DAEMON,
								  false, 0, &allow_perms );

		//
		// List the outstanding token requests.
		//
		// In the handler, we further restrict the returned information based on
		// the user authorization.
		//
	daemonCore->Register_CommandWithPayload( DC_LIST_TOKEN_REQUEST, "DC_LIST_TOKEN_REQUEST",
		handle_dc_list_token_request,
		"handle_dc_list_token_request", DAEMON, true, 0, &allow_perms );

		//
		// Approve a token request.
		//
		// In the handler, we further restrict the returned information based on
		// the user authorization; non-ADMINISTRATORs can only approve their own
		// requests..
		//
	daemonCore->Register_CommandWithPayload( DC_APPROVE_TOKEN_REQUEST, "DC_APPROVE_TOKEN_REQUEST",
		handle_dc_approve_token_request,
		"handle_dc_approve_token_request", DAEMON, true, 0, &allow_perms );

		//
		// Install an auto-approval rule
		//
	daemonCore->Register_CommandWithPayload( DC_AUTO_APPROVE_TOKEN_REQUEST, "DC_AUTO_APPROVE_TOKEN_REQUEST",
		handle_dc_auto_approve_token_request,
		"handle_dc_auto_approve_token_request", ADMINISTRATOR );

		//
		// Exchange a SciToken for an equivalent HTCondor token.
		//
	daemonCore->Register_CommandWithPayload( DC_EXCHANGE_SCITOKEN, "DC_EXCHANGE_SCITOKEN",
		handle_dc_exchange_scitoken,
		"handle_dc_exchange_scitoken", WRITE, true, 0, &allow_perms );

	// Call daemonCore's reconfig(), which reads everything from
	// the config file that daemonCore cares about and initializes
	// private data members, etc.
	daemonCore->reconfig();

	// zmiller
	// look in the env for ENV_PARENT_ID
	const char* envName = ENV_CONDOR_PARENT_ID;
	std::string parent_id;

	GetEnv( envName, parent_id );

	// send it to the SecMan object so it can include it in any
	// classads it sends.  if this is NULL, it will not include
	// the attribute.
	daemonCore->sec_man->set_parent_unique_id(parent_id.c_str());

	// now re-set the identity so that any children we spawn will have it
	// in their environment
	SetEnv( envName, daemonCore->sec_man->my_unique_id() );

	// call the daemon's main_init()
	dc_main_init( argc, argv );

#ifndef WIN32
	// last chance to send status to the fork parent so it can exit.
	// unless we have been put into parent_is_master mode, where we let the master
	// decide when to release the forked parent (i.e. after shared port)
	if ( ! dc_background_parent_is_master) {
		dc_release_background_parent(0);
	}
#endif

	// now call the driver.  we never return from the driver (infinite loop).
	daemonCore->Driver();

	// should never get here
	EXCEPT("returned from Driver()");
	return FALSE;
}	

// set the default for -f / -b flag for this daemon
// used by the master to default to backround, all other daemons default to foreground.
bool dc_args_default_to_background(bool background)
{
	bool ret = ! Foreground;
	Foreground = ! background;
	return ret;
}

// Parse argv enough to decide if we are starting as foreground or as background
// We need this for windows because when starting as background, we need to actually
// start via the Service Control Manager rather than calling dc_main directly.
//
bool dc_args_is_background(int argc, char** argv)
{
    bool ForegroundFlag = Foreground; // default to foreground

	// Scan our command line arguments for a "-f".  If we don't find a "-f",
	// or a "-v", then we want to register as an NT Service.
	int i = 0;
	bool done = false;
	for(char ** ptr = argv + 1; *ptr && (i < argc - 1); ptr++,i++)
	{
		if( ptr[0][0] != '-' ) {
			break;
		}
		switch( ptr[0][1] ) {
		case 'a':		// Append to the log file name.
			ptr++;
			break;
		case 'b':		// run in Background (default)
			ForegroundFlag = false;
			break;
		case 'c':		// specify directory where Config file lives
			ptr++;
			break;
		case 'd':		// Dynamic local directories
			if( strcmp( "-d", *ptr ) && strcmp( "-dynamic", *ptr ) ) {
				done = true;
			}
			break;
		case 't':		// log to Terminal (stderr), implies -f
		case 'f':		// run in ForegroundFlag
			ForegroundFlag = true;
			break;
		case 'h':		// -http
			if ( ptr[0][2] && ptr[0][2] == 't' ) {
					// specify an HTTP port
				ptr++;
			} else {
				done = true;
			}
            break;
#ifndef WIN32
		case 'k':		// Kill the pid in the given pid file
			ptr++;
			break;
#endif
		case 'l':		// specify Log directory
			 ptr++;
			 break;
		case 'p':		// Use well-known Port for command socket.
			ptr++;		// Also used to specify a Pid file, but both
			break;		// versions require a 2nd arg, so we're safe. 
		case 'q':		// Quiet output
			break;
		case 'r':		// Run for <arg> minutes, then gracefully exit
			ptr++;
			break;
		case 's':       // the c-gahp uses -s, so for backward compatibility,
						// do not allow abbreviations of -sock
			if(0 == strcmp("-sock",*ptr)) {
				ptr++;
			} else {
				done = true;
			}
			break;
		case 'v':		// display Version info and exit
			ForegroundFlag = true;
			break;
		default:
			done = true;
			break;	
		}
		if( done ) {
			break;		// break out of for loop
		}
	}

    return ! ForegroundFlag;
}
