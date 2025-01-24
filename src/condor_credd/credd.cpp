/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
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
#include "credd.h"
#include "condor_config.h"
#include "condor_daemon_core.h"
#include "condor_debug.h"
#include "store_cred.h"
#include "internet.h"
#include "get_daemon_name.h"
#include "subsystem_info.h"
#include "credmon_interface.h"
#include "ipv6_hostname.h"
#include "secure_file.h"
#include "directory_util.h"

#include <filesystem>

//-------------------------------------------------------------

CredDaemon *credd;


int
cgth_bailout( int level, const std::string & msg, ReliSock * sock ) {
	dprintf( level, "%s\n", msg.c_str() );

	if( sock ) {
		ClassAd replyAd;
		replyAd.Assign( ATTR_ERROR_STRING, msg );
		putClassAd( sock, replyAd );
		sock->end_of_message();
	}

	return CLOSE_STREAM;
}

//
// Cribbed from cred_get_password_handler(), but not put in
// condor_utils/store_cred.cpp because we almost certainly
// don't want anyone other than the credd doing this.
//

int
cred_get_token_handler(int /*i*/, Stream *s)
{
	/* Check our connection.  We must be very picky since we are talking
	   about sending out passwords.  We want to make certain
	     a) the Stream is a ReliSock (tcp)
	     b) it is authenticated (and thus authorized by daemoncore)
	     c) it is encrypted
	*/

	if ( s->type() != Stream::reli_sock ) {
		std::string m;
		formatstr( m,
			"cred_get_token_handler(): WARNING - credential fetch attempt via UDP from %s",
				((Sock*)s)->peer_addr().to_sinful().c_str());
		return cgth_bailout( D_ALWAYS, m, NULL );
	}

	ReliSock * sock = (ReliSock*)s;

	// Ensure authentication happened and succeeded
	// Daemons should register this command with force_authentication = true
	if ( !sock->isAuthenticated() ) {
		std::string m;
		formatstr( m,
				"cred_get_token_handler(): WARNING - authentication failed for credential fetch attempt from %s",
				sock->peer_addr().to_sinful().c_str());
		return cgth_bailout( D_ALWAYS, m, sock );
	}

	// Enable encryption if available. If it's not available, the next
	// call will fail and we'll abort the connection.
	sock->set_crypto_mode(true);

	std::string socket_peer = sock->peer_addr().to_sinful();
	if ( !sock->get_encryption() ) {
		std::string m;
		formatstr( m,
			"cred_get_token_handler(): WARNING - credential fetch attempt without encryption from %s",
			socket_peer.c_str()
		);
		return cgth_bailout( D_ALWAYS, m, sock );
	}
	/* End cribbing from cred_get_password_handler(). */

	// This is what we _should_ be using, but in other places, we assume
	// that all authenticated identities are also (unix) user names and
	// use those, instead.
	// std::string socket_authenticated_identity = sock->getFullyQualifiedUser();
	std::string socket_authenticated_identity = sock->getOwner();


	// ... validate the command ...
	sock->decode();
	ClassAd commandAd;
	if(! getClassAd(sock, commandAd)) {
		std::string m;
		formatstr( m, "cred_get_token_handler(): failed to receive command ad." );
		return cgth_bailout( D_ALWAYS, m, sock );
	}

	int result = sock->end_of_message();
	if( !result ) {
		std::string m;
		formatstr( m, "cred_get_token_handler(): failed to receive end-of-message." );
		return cgth_bailout( D_ALWAYS, m, sock );
	}

	std::string service;
	if(! commandAd.LookupString( "Service", service )) {
		std::string m;
		formatstr( m, "cred_get_token_handler(): invalid command ad, no Service attribute." );
		return cgth_bailout( D_ALWAYS, m, sock );
	}
	if( service.empty() ) {
		std::string m;
		formatstr( m, "cred_get_token_handler(): invalid command ad, empty Service attribute." );
		return cgth_bailout( D_ALWAYS, m, sock );
	}
	if(! okay_for_oauth_filename(service)) {
		std::string m;
		formatstr( m,  "cred_get_token_handler(): illegal character in service name." );
		return cgth_bailout( D_ALWAYS, m, sock );
	}

	std::string handle;
	if( commandAd.LookupString( "Handle", handle ) ) {
		if( handle.empty() ) {
			std::string m;
			formatstr( m, "cred_get_token_handler(): invalid command ad, empty Handle attribute." );
			return cgth_bailout( D_ALWAYS, m, sock );
		}
		if(! okay_for_oauth_filename(handle)) {
			std::string m;
			formatstr( m, "cred_get_token_handler(): illegal character in handle name." );
			return cgth_bailout( D_ALWAYS, m, sock );
		}
	}


	// ... fetch the token ...
	std::string token_file_name = service;
	if(! handle.empty()) {
		token_file_name += "_" + handle;
	}
	token_file_name += ".use";

	std::string oauthCredentialDir;
	if(! param( oauthCredentialDir, "SEC_CREDENTIAL_DIRECTORY_OAUTH" )) {
		std::string m;
		formatstr( m, "cred_get_token_handler(): SEC_CREDENTIAL_DIRECTORY_OAUTH not defined." );
		return cgth_bailout( D_ALWAYS, m, sock );
	}
	std::filesystem::path token_path =
		std::filesystem::path(oauthCredentialDir) /
		socket_authenticated_identity /
		token_file_name;

	bool as_root = true;
	size_t credSize = 0;
	void * credential = nullptr;
	if( std::filesystem::exists(token_path) && std::filesystem::is_regular_file(token_path)) {
		bool rv = read_secure_file( token_path.string().c_str(),
			& credential, & credSize,
			as_root, SECURE_FILE_VERIFY_ACCESS
		);
		if(! rv) {
			std::string m;
			formatstr( m, "cred_get_token_handler(): read_secure_file(%s) failed.", token_path.string().c_str() );
			return cgth_bailout( D_ALWAYS, m, sock );
		}
	} else {
		std::string m;
		formatstr( m, "cred_get_token_handler(): %s not an existing regular file.", token_path.string().c_str() );
		return cgth_bailout( D_ALWAYS, m, sock );
	}


	// ... send the token ...
	sock->encode();
	ClassAd replyAd;
	replyAd.InsertAttr( ATTR_SEC_TOKEN, (char *)credential, credSize );
	if(! putClassAd(sock, replyAd) ) {
		SecureZeroMemory( credential, credSize );
		free(credential);

        // Both StringLiterals and std::strings are intended to be immutable,
        // so this code is a little dodgy.
		ExprTree * tokenExpr = replyAd.Remove( ATTR_SEC_TOKEN );
		classad::StringLiteral * tokenLiteral = dynamic_cast<classad::StringLiteral *>(tokenExpr);
		ASSERT(tokenLiteral != NULL );
		SecureZeroMemory( const_cast<char *>(tokenLiteral->getCString()), credSize );

		std::string m;
		formatstr( m, "cred_get_token_handler(): failed to send reply ad." );
		return cgth_bailout( D_ALWAYS, m, sock );
	}

	result = sock->end_of_message();
	if( !result ) {
		SecureZeroMemory( credential, credSize );
		free(credential);

		std::string m;
		formatstr( m, "cred_get_token_handler(): failed to receive end-of-message." );
		return cgth_bailout( D_ALWAYS, m, sock );
	}

	dprintf( D_ALWAYS,
			"Sent token in %s to %s at %s\n",
			token_path.string().c_str(),
			socket_authenticated_identity.c_str(),
			socket_peer.c_str()
	);

	// Zero the token out of RAM once it's been sent.
	SecureZeroMemory( credential, credSize );
	free(credential);

    // Both StringLiterals and std::strings are intended to be immutable,
    // so this code is a little dodgy.
	ExprTree * tokenExpr = replyAd.Remove( ATTR_SEC_TOKEN );
	classad::StringLiteral * tokenLiteral = dynamic_cast<classad::StringLiteral *>(tokenExpr);
	ASSERT(tokenLiteral != NULL );
	SecureZeroMemory( const_cast<char *>(tokenLiteral->getCString()), credSize );

	return CLOSE_STREAM;
}


CredDaemon::CredDaemon() : m_name(NULL), m_update_collector_tid(-1)
{

	reconfig();

		// Command handler for the user condor_store_cred tool
	daemonCore->Register_Command( STORE_CRED, "STORE_CRED",
								&store_cred_handler,
								"store_cred_handler", WRITE,
								true /*force authentication*/ );

		// Command handler for daemons to get the password
	daemonCore->Register_Command( CREDD_GET_PASSWD, "CREDD_GET_PASSWD",
								&cred_get_password_handler,
								"cred_get_password_handler", DAEMON,
								true /*force authentication*/ );

	// Command handle for obtaining OAuth tokens
	daemonCore->Register_Command( CREDD_GET_TOKEN, "CREDD_GET_TOKEN",
								& cred_get_token_handler,
								"cred_get_token_handler", WRITE,
								true /* force authentication */ );

		// NOP command for testing authentication
	daemonCore->Register_Command( CREDD_NOP, "CREDD_NOP",
								(CommandHandlercpp)&CredDaemon::nop_handler,
								"nop_handler", this, DAEMON );

		// See if creds are present for all modules requested
	daemonCore->Register_Command( CREDD_CHECK_CREDS, "CREDD_CHECK_CREDS",
								(CommandHandlercpp)&CredDaemon::check_creds_handler,
								"check_creds_handler", this, WRITE, true);

		// set timer to periodically advertise ourself to the collector
	daemonCore->Register_Timer(0, m_update_collector_interval,
		(TimerHandlercpp)&CredDaemon::update_collector, "update_collector", this);

	// only sweep if we have a cred dir
	auto_free_ptr cred_dir_krb(param("SEC_CREDENTIAL_DIRECTORY_KRB"));
	auto_free_ptr cred_dir_oauth(param("SEC_CREDENTIAL_DIRECTORY_OAUTH"));
	if (cred_dir_krb || cred_dir_oauth) {
		// didn't need the value, just to see if it's defined.
		dprintf(D_FULLDEBUG, "CREDD: setting sweep_timer_handler for KRB\n");
		int sec_cred_sweep_interval = param_integer("SEC_CREDENTIAL_SWEEP_INTERVAL", 300);
		m_cred_sweep_tid = daemonCore->Register_Timer( sec_cred_sweep_interval, sec_cred_sweep_interval,
								(TimerHandlercpp)&CredDaemon::sweep_timer_handler,
								"sweep_timer_handler", this );
	}
}

CredDaemon::~CredDaemon()
{
	// tell our collector we're going away
	invalidate_ad();

	if (m_name != NULL) {
		free(m_name);
	}
}

void
CredDaemon::reconfig()
{
	// get our daemon name; if CREDD_HOST is defined, we use it
	// as our name. this is because clients that have CREDD_HOST
	// defined will query the Collector for a CredD ad that has
	// a name matching its setting for CREDD_HOST. but CREDD_HOST
	// will not necessarily match what default_daemon_name()
	// returns - for example, if CREDD_HOST is set to a DNS alias
	//
	if (m_name != NULL) {
		free(m_name);
	}
	m_name = param("CREDD_HOST");
	if (m_name == NULL) {
		m_name = default_daemon_name();
		ASSERT(m_name != NULL);
	}
	if(m_name == NULL) {
		EXCEPT("default_daemon_name() returned NULL");
	}

	// how often do we update the collector?
	m_update_collector_interval = param_integer ("CREDD_UPDATE_INTERVAL",
												 5 * MINUTE);

	// fill in our classad
	initialize_classad();

	// reset the timer (if it exists) to update the collector
	if (m_update_collector_tid != -1) {
		daemonCore->Reset_Timer(m_update_collector_tid, 0, m_update_collector_interval);
	}

	m_sorter.Init();
}

void
CredDaemon::sweep_timer_handler( int /* timerID */ ) const
{
	dprintf(D_FULLDEBUG, "CREDD: calling and resetting sweep_timer_handler()\n");

	auto_free_ptr cred_dir(param("SEC_CREDENTIAL_DIRECTORY_KRB"));
	credmon_sweep_creds(cred_dir, credmon_type_KRB);
	cred_dir.set(param("SEC_CREDENTIAL_DIRECTORY_OAUTH"));
	credmon_sweep_creds(cred_dir, credmon_type_OAUTH);
	int sec_cred_sweep_interval = param_integer("SEC_CREDENTIAL_SWEEP_INTERVAL", 300);
	daemonCore->Reset_Timer (m_cred_sweep_tid, sec_cred_sweep_interval, sec_cred_sweep_interval);
}


void
CredDaemon::initialize_classad()
{
	m_classad.Clear();

	SetMyTypeName(m_classad, CREDD_ADTYPE);

	m_classad.Assign(ATTR_NAME, m_name);

	m_classad.Assign(ATTR_CREDD_IP_ADDR,
			daemonCore->InfoCommandSinfulString() );

        // Publish all DaemonCore-specific attributes, which also handles
        // SUBSYS_ATTRS for us.
    daemonCore->publish(&m_classad);
}

void
CredDaemon::update_collector( int /* timerID */ )
{
	daemonCore->sendUpdates(UPDATE_AD_GENERIC, &m_classad, NULL, true);
}

void
CredDaemon::invalidate_ad()
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

int
CredDaemon::nop_handler(int, Stream*)
{
	return 0; // ????
}


// check to see if the desired OAUTH tokens are stored already, if not generate and return a URL
int
CredDaemon::check_creds_handler( int, Stream* s)
{
	ReliSock* r = (ReliSock*)s;
	r->decode();
	int numads = 0;
	if (!r->code(numads)) {
		dprintf(D_ALWAYS, "check_creds: cannot read numads off wire\n");
		r->end_of_message();
		return CLOSE_STREAM;
	}

	std::vector<ClassAd> requests;
	requests.resize(numads);
	for(int i=0; i<numads; i++) {
		if (!getClassAd(r, requests[i])) {
			dprintf(D_ALWAYS, "check_creds: cannot read classad off wire\n");
			r->end_of_message();
			return CLOSE_STREAM;
		}
	}
	r->end_of_message();

	dprintf(D_ALWAYS, "Got check_creds for %d OAUTH services.\n", numads);

	std::string URL;

	auto_free_ptr cred_dir(param("SEC_CREDENTIAL_DIRECTORY_OAUTH"));
	if ( ! cred_dir) {
		r->encode();
		URL = "ERROR - SEC_CREDENTIAL_DIRECTORY_OAUTH not configured in condor_credd";
		(void) r->code(URL);
		r->end_of_message();
		return CLOSE_STREAM;
	}

	// do the superuser check once and set a flag for later use
	bool is_cred_super_user = false;

	std::vector<std::string> super_users;
	param_and_insert_unique_items("CRED_SUPER_USERS", super_users);
	std::string sock_user = r->getOwner();
	if ( !sock_user.empty() &&
#if defined(WIN32)
	     contains_anycase_withwildcard(super_users, sock_user)
#else
	     contains_withwildcard(super_users, sock_user)
#endif
	   )
	{
		dprintf( D_ALWAYS, "OAuth creds being requested by \"%s\", a member of CRED_SUPER_USERS\n", sock_user.c_str());
		is_cred_super_user = true;
	}

	std::vector<ClassAd *> oauth2_missing;
	std::vector<ClassAd *> local_issuer_missing;
	std::vector<ClassAd *> local_client_missing;
	for(int i=0; i<numads; i++) {
		std::string service;
		std::string handle;
		std::string req_user;
		requests[i].LookupString("Service", service);
		requests[i].LookupString("Handle", handle);
		requests[i].LookupString("Username", req_user);

		// username handling is special.  we have the req_user sent by
		// the client, and we have the authenticated sock_user from the
		// socket (obtained above). the logic is as follows:
		//
		// empty req_user means we always proceed using the sock_user.
		//
		// if req_user and sock_user match then proceed using sock_user.
		//   NOTE: the definition of "match" will need to be addressed
		//   if these usernames include a domain.  TODO ZKM FIX
		//
		// if the sock_user is a super user proceed using the req_user.
		//
		// otherwise we fail.

		// this holds the final result
		std::string username;

		if (req_user.empty() || (req_user == sock_user)) {
			username = sock_user;
		} else if (is_cred_super_user) {
			dprintf(D_SECURITY, "OAuth creds for \"%s\" were requested by \"%s\".", req_user.c_str(), sock_user.c_str());
			username = req_user;
		} else {
			// a non-super user requested a username that is not theirs.
			formatstr(URL, "ERROR: credentials for user \"%s\" cannot be requested by authenticated user \"%s\".", req_user.c_str(), sock_user.c_str());
			goto bail;
		}

		// whatever we decided, put it back into the ad so anything
		// that examines this attribute later doesn't need to redo all
		// this username logic.
		requests[i].Assign("Username", username);

		// TODO ZKM FIX: eventually we will need to honor domain names.
		// strip off the domain from the username (if any)
		std::string user(username);
		int ix = user.find("@");
		if (ix >= 0) { user.resize(ix); }

		CredSorter::CredType cred_type = m_sorter.Sort(service);

		if (cred_type == CredSorter::LocalIssuerType && !handle.empty()) {
			formatstr(URL, "ERROR: Local Issuer credential '%s' can't have a handle.", service.c_str());
			goto bail;
		}
		if (cred_type == CredSorter::LocalClientType && !handle.empty()) {
			formatstr(URL, "ERROR: Local Client credential '%s' can't have a handle.", service.c_str());
			goto bail;
		}

		// reformat the service and handle into a filename
		std::string service_fname(service);
        replace_str( service_fname, "/", ":" ); // TODO: : isn't going to work on Windows. should use ; instead
		if ( ! handle.empty()) {
			service_fname += "_";
			service_fname += handle;
		}
		std::string service_name(service_fname);
		service_fname += ".top";

		dprintf(D_FULLDEBUG, "check_creds: checking for OAUTH %s/%s\n", user.c_str(), service_fname.c_str());

		// concatinate the oauth_cred_dir / user / service_filename
		std::string tmpfname;
		dirscat(cred_dir, user.c_str(), tmpfname);
		tmpfname += service_fname;

		// stat the file as root
		struct stat stat_buf;
		priv_state priv = set_root_priv();
		int rc = stat(tmpfname.c_str(), &stat_buf);
		set_priv(priv);

		// if the file is not found, add this request to the collection of missing requests
		if (rc==-1) {
			dprintf(D_ALWAYS, "check_creds: did not find %s\n", tmpfname.c_str());
			switch(cred_type) {
			case CredSorter::LocalIssuerType:
				local_issuer_missing.push_back(&requests[i]);
				break;
			case CredSorter::LocalClientType:
				local_client_missing.push_back(&requests[i]);
				break;
			case CredSorter::OAuth2Type:
				oauth2_missing.push_back(&requests[i]);
				break;
			case CredSorter::VaultType:
				formatstr(URL, "ERROR: Vault credential '%s' is missing.", service.c_str());
				goto bail;
				break;
			default:
				formatstr(URL, "ERROR: Credential '%s' of unknown type is missing.", service.c_str());
				goto bail;
			}
		} else {
			// check to see if new scopes and audience match previous cred
			if (cred_matches(tmpfname, &requests[i]) == FAILURE_CRED_MISMATCH) {
				r->encode();
				URL = "ERROR - credentials exist that do not match the request";
				URL += "\n  They can be removed with";
				URL += "\n    condor_store_cred delete-oauth -s " + service_name;
				URL += "\n  but make sure no other job is using them.";
				(void) r->code(URL);
				r->end_of_message();
				return CLOSE_STREAM;
			}
			// else ignore other problems or a match
		}
	}

	if (!local_issuer_missing.empty()) {
		for (const auto& req: local_issuer_missing) {
			std::string username;
			std::string service;
			std::string val;
			req->LookupString("Username", username);
			req->LookupString("Service", service);
			if ((req->LookupString("Scopes", val) && !val.empty()) ||
				(req->LookupString("Audience", val) && !val.empty()))
			{
				formatstr(URL, "ERROR: Local Issuer credential '%s' can't have user-provided scope or audience.", service.c_str());
				goto bail;
			}
			// HACK store_cred_blob wants a domain in username, but always
			//   strips it off.
			username += "@foo";
			// write .top file
			std::string ccfile;
			long long rv = store_cred_blob(username.c_str(), ADD_OAUTH_MODE, (const unsigned char*)username.c_str(), username.length(), req, ccfile);
			if (rv != SUCCESS) {
				formatstr(URL, "ERROR: Failed to generate credential '%s'.", service.c_str());
				goto bail;
			}
		}
	}

	if (!local_client_missing.empty()) {
		for (const auto& req: local_client_missing) {
			std::string username;
			std::string service;
			std::string val;
			req->LookupString("Username", username);
			req->LookupString("Service", service);
			if ((req->LookupString("Scopes", val) && !val.empty()) ||
				(req->LookupString("Audience", val) && !val.empty()))
			{
				formatstr(URL, "ERROR: Local Client credential '%s' can't have user-provided scope or audience.", service.c_str());
				goto bail;
			}
			// HACK store_cred_blob wants a domain in username, but always
			//   strips it off.
			username += "@foo";
			// write .top file
			std::string ccfile;
			long long rv = store_cred_blob(username.c_str(), ADD_OAUTH_MODE, (const unsigned char*)username.c_str(), username.length(), req, ccfile);
			if (rv != SUCCESS) {
				formatstr(URL, "ERROR: Failed to generate credential '%s'.", service.c_str());
				goto bail;
			}
		}
	}

	if (!local_issuer_missing.empty() || !local_client_missing.empty()) {
		credmon_kick(credmon_type_OAUTH);
		// TODO wait for .use files to appear?
	}

	if (!oauth2_missing.empty()) {
		// create unique request file with classad metadata
		auto_free_ptr key(Condor_Crypt_Base::randomHexKey(32));

		std::string web_prefix;
		param(web_prefix, "CREDMON_WEB_PREFIX");
		if (web_prefix.empty()) {
			web_prefix = "https://";
			web_prefix += get_local_fqdn();
		}

		std::string contents; // what we will write to the credential directory for this URL

		for (ClassAd *req: oauth2_missing) {
			// fill in everything we need to pass
			ClassAd ad;
			std::string tmpname;
			std::string tmpvalue;
			std::string secret_filename;

			std::string service;
			req->LookupString("Service", service);
			ad.Assign("Provider", service);

			tmpvalue.clear();
			req->LookupString("Username", tmpvalue);
			ad.Assign("LocalUser", tmpvalue);

			tmpvalue.clear();
			req->LookupString("Handle", tmpvalue);
			ad.Assign("Handle", tmpvalue);

			tmpvalue.clear();
			req->LookupString("Scopes", tmpvalue);
			ad.Assign("Scopes", tmpvalue);

			tmpvalue.clear();
			req->LookupString("Audience", tmpvalue);
			ad.Assign("Audience", tmpvalue);

			tmpname = service;
			tmpname += "_CLIENT_ID";
			param(tmpvalue, tmpname.c_str());
			ad.Assign("ClientId", tmpvalue);

			tmpname = service;
			tmpname += "_CLIENT_SECRET_FILE";
			tmpvalue.clear();
			char *buf = NULL;
			size_t buf_len = 0;
			size_t secret_len = 0;
			if (param(secret_filename, tmpname.c_str()) && ! secret_filename.empty() &&
				read_secure_file(secret_filename.c_str(), (void**)&buf, &buf_len, true)) {
				// Read the file and use the contents before the first
				// newline, if any. remember the buffer is probably not null terminated!
				size_t i = 0;
				for ( i = 0; i < buf_len; i++ ) {
					// file may have terminating \r\n, etc,
					// we use only up to the first non-printing character
					if ( buf[i] <= 0x1F ) {
						break;
					}
				}
				secret_len = i;  // the secret length might be less than the file length
				tmpvalue.assign(buf, secret_len); // copy so that we can null terminate
				memset(buf, 0, buf_len); // overwrite the secret before freeing the buffer
				free(buf);
			} else {
				//TODO: make a better error return channel for this command
				formatstr(URL, "Failed to securely read client secret for service %s", service.c_str());
				if (secret_filename.empty()) {
					formatstr_cat(URL, "; Tell your admin that %s is not configured", tmpname.c_str());
					dprintf(D_ALWAYS, "check_creds: ERROR: %s not configured for service %s\n", tmpname.c_str(), service.c_str());
				} else {
					dprintf(D_ALWAYS, "check_creds: ERROR: could not securely read file '%s' for service %s\n", secret_filename.c_str(), service.c_str());
				}
				goto bail;
			}
			ad.Assign("ClientSecret", tmpvalue);

			tmpname = service;
			tmpname += "_RETURN_URL_SUFFIX";
			auto_free_ptr url_suffix(param(tmpname.c_str()));
			tmpvalue = web_prefix;
			if (url_suffix) { tmpvalue += url_suffix.ptr(); }
			ad.Assign("ReturnUrl", tmpvalue);

			tmpname = service;
			tmpname += "_AUTHORIZATION_URL";
			param(tmpvalue, tmpname.c_str());
			ad.Assign("AuthorizationUrl", tmpvalue);

			tmpname = service;
			tmpname += "_TOKEN_URL";
			param(tmpvalue, tmpname.c_str());
			ad.Assign("TokenUrl", tmpvalue);

			// serialize classad into string
			if(!contents.empty()) {
				contents += "\n";
			}
			// append the new ad
			sPrintAdWithSecrets(contents, ad);

			if (IsDebugVerbose(D_FULLDEBUG)) {
				std::string tmp;
				formatstr(tmp, "<contents of %s> %d bytes", secret_filename.c_str(), (int)secret_len);
				ad.Assign("ClientSecret", tmp);
				dprintf(D_FULLDEBUG, "Service %s ad:\n", service.c_str());
				dPrintAd(D_FULLDEBUG, ad);
			}
		}

		std::string path;
		dircat(cred_dir, key, path);

		dprintf(D_ALWAYS, "check_creds: storing %zu bytes for %zu services to %s\n", contents.length(), oauth2_missing.size(), path.c_str());
		const bool as_root = false; // write as current user
		const bool group_readable = true;
		int rc = write_secure_file(path.c_str(), contents.c_str(), contents.length(), as_root, group_readable);

		if (rc != SUCCESS) {
			dprintf(D_ALWAYS, "check_creds: failed to write secure temp file %s\n", path.c_str());
		} else {
			// on success we return a URL
			URL = web_prefix;
			URL += "/key/";
			URL += key.ptr();
			dprintf(D_ALWAYS, "check_creds: returning URL %s\n", URL.c_str());
		}
	} else {
		dprintf(D_ALWAYS, "check_creds: found all requested OAUTH services. returning empty URL %s\n", URL.c_str());
	}

bail:
	r->encode();
	if (!r->code(URL)) {
		dprintf(D_ALWAYS, "check_creds: error sending URL to client\n");
	}
	r->end_of_message();

	return CLOSE_STREAM;
}

//-------------------------------------------------------------

void
main_init(int /*argc*/, char * /*argv*/ [])
{
	dprintf(D_ALWAYS, "main_init() called\n");

	credd = new CredDaemon;
}

//-------------------------------------------------------------

void
main_config()
{
	dprintf(D_ALWAYS, "main_config() called\n");

	credd->reconfig();
}

//-------------------------------------------------------------

void main_shutdown_fast()
{
	dprintf(D_ALWAYS, "main_shutdown_fast() called\n");

	delete credd;

	DC_Exit(0);
}

//-------------------------------------------------------------

void main_shutdown_graceful()
{
	dprintf(D_ALWAYS, "main_shutdown_graceful() called\n");

	delete credd;

	DC_Exit(0);
}

//-------------------------------------------------------------

int
main( int argc, char **argv )
{
	set_mySubSystem( "CREDD", true, SUBSYSTEM_TYPE_DAEMON );

	dc_main_init = main_init;
	dc_main_config = main_config;
	dc_main_shutdown_fast = main_shutdown_fast;
	dc_main_shutdown_graceful = main_shutdown_graceful;
	return dc_main( argc, argv );
}
