/***************************************************************
 *
 * Copyright (C) 1990-2022, Condor Team, Computer Sciences Department,
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

#include "cred_dir.h"

#include "condor_attributes.h"
#include "condor_config.h"
#include "condor_uid.h"
#include "directory.h"
#include "directory_util.h"
#include "secure_file.h"
#include "store_cred.h"

using namespace htcondor;

CredData::~CredData()
{
	if (buf) {
		memset(buf, '\0', len);
		free(buf);
	}
	buf = nullptr;
	len = 0;
}

bool CredDirCreator::WriteToCredDir(const std::string &path, const CredData &cred, CondorError &err)
{
	{
		// We write out credential directories as condor in the case of the shadow hook
		// and then chown the credential file itself over to the user;
		// this prevents the user from putting garbage into the credential
		// directory.

		// Only do this extra-effort dance on non-Windows platforms.  We do this because,
		// in the case of the shadow hook, the user has not been allocated any space and hence
		// shouldn't have a writable directory they can scribble into.
#ifdef WIN32
		TemporaryPrivSentry sentry(PRIV_USER);
#else
		TemporaryPrivSentry sentry(m_creddir_user_priv ? PRIV_USER : PRIV_CONDOR);
#endif
		const bool as_root = false;
		const bool group_readable = false;
		if (!replace_secure_file(path.c_str(), ".tmp", cred.buf, cred.len, as_root, group_readable)) {
			err.pushf("WriteToCredDir", errno,
				"Failed to write out kerberos-style credential for %s: %s\n",
				m_use_case.c_str(), strerror(errno));
			dprintf(D_ERROR, "%s\n", err.message());
			return false;
		}
	}
#ifndef WIN32
	if (!m_creddir_user_priv) {
		TemporaryPrivSentry sentry(PRIV_ROOT);
		if (-1 == chmod(path.c_str(), 0400)) {
			err.pushf("WriteToCredDir", errno,
				"Failed to chmod credential to 0400 for %s: %s",
				m_use_case.c_str(), strerror(errno));
			dprintf(D_ERROR, "%s\n", err.message());
			return false;
		}
		if (-1 == chown(path.c_str(), get_user_uid(), get_user_gid())) {
			err.pushf("WriteToCredDir", errno, "Failed to chown credential to user %d for %s: %s\n",
				get_user_uid(), m_use_case.c_str(), strerror(errno));
			dprintf(D_ERROR, "%s\n", err.message());
			return false;
		}
	}
#endif
	return true;
}


bool CredDirCreator::PrepareCredDir(CondorError &err)
{
	std::string services_needed;
	if (m_ad.EvaluateAttrString(ATTR_OAUTH_SERVICES_NEEDED, services_needed)) {
		dprintf(D_FULLDEBUG, "Will populate credentials directory for %s with credentials: %s\n",
			m_use_case.c_str(), services_needed.c_str());
	} else {
			dprintf(D_FULLDEBUG, "No OAuth services are requested.\n");
	}

	bool send_krb5_credential = false;
	if (!m_ad.EvaluateAttrBool(ATTR_JOB_SEND_CREDENTIAL, send_krb5_credential)) {
		send_krb5_credential = false;
	}

	if (!send_krb5_credential && services_needed.empty()) {
		return true;
	}

	if (m_cred_dir.empty()) {
		err.pushf("PrepareCredDir", 1, "Credentials directory for %s is empty (internal error)",
			m_use_case.c_str());
		dprintf(D_ERROR, "%s\n", err.message());
		return false;
	}

	// Automatically delete the creds directory on failure.
	std::unique_ptr<const std::string, void(*)(const std::string*)> cred_dir_deleter(nullptr, [](const std::string *path) {
		Directory cred_dirp(path->c_str(), PRIV_ROOT);
		cred_dirp.Remove_Entire_Directory();
	});

#ifdef WIN32
	if (!mkdir_and_parents_if_needed(m_cred_dir.c_str(), 0755, PRIV_USER))
#else
	if (!mkdir_and_parents_if_needed(m_cred_dir.c_str(), 0755, m_creddir_user_priv ? PRIV_USER : PRIV_CONDOR))
#endif
	{
		dprintf(D_ERROR, "Failed to create credentials directory %s for %s: %s\n",
			m_cred_dir.c_str(), m_use_case.c_str(), strerror(errno));
		return false;
	}
	cred_dir_deleter.reset(&m_cred_dir);
	m_made_cred_dir = true;

	std::string job_user;
	if (!m_ad.EvaluateAttrString(ATTR_USER, job_user)) {
		dprintf(D_ERROR, "Shadow copy of the job ad does not have user attribute.\n");
		return false;
	}
	auto at_pos = job_user.find('@');
	auto user = job_user.substr(0, at_pos);
	if (send_krb5_credential) {
		std::string domain;
		if (at_pos != std::string::npos) {
			domain = job_user.substr(at_pos + 1);
		}

		CredData cred;
			// For the kerberos case, an empty credential means "do not write out a cred"
		if (!GetKerberosCredential(user, domain, cred, err)) {
			return false;
		}

		if (cred.len) {
			std::string ccfilename;
			dircat(m_cred_dir.c_str(), user.c_str(), ".cc", ccfilename);
			if (!WriteToCredDir(ccfilename, cred, err)) {
				return false;
			}
		}
	}

	if (services_needed.empty()) {
		cred_dir_deleter.release();
		return true;
	}

	std::unordered_set<std::string> processed_services;
	for (auto& curr: StringTokenIterator(services_needed)) {
		// Don't process the same service twice
		if (processed_services.count(curr)) {
			continue;
		}
		processed_services.insert(curr);

		CredData cred;
		if (!GetOAuth2Credential(curr, user, cred, err)) {
			return false;
		}

		std::string fullname, fname;
		formatstr(fname, "%s.use", curr.c_str());
		replace_str(fname, "*", "_");
		formatstr(fullname, "%s%c%s", m_cred_dir.c_str(), DIR_DELIM_CHAR, fname.c_str());
		if (!WriteToCredDir(fullname, cred, err)) {
			return false;
		}
	}

	cred_dir_deleter.release();
	return true;
}


bool LocalCredDirCreator::GetKerberosCredential(const std::string &user, const std::string &domain,
	CredData &cred, CondorError &err)
{
	int credlen;
	cred.buf = getStoredCredential(STORE_CRED_USER_KRB, user.c_str(), domain.c_str(), credlen);
	if (!cred.buf) {
		err.pushf("GetKerberosCredential", 1, "Unable to read stored credential for %s",
			m_use_case.c_str());
		dprintf(D_ERROR, "%s\n", err.message());
		return false;
	}
	cred.len = credlen;
	return true;
}

bool LocalCredDirCreator::GetOAuth2Credential(const std::string &name, const std::string &user, CredData &cred,
	CondorError &err)
{
	std::string oauth_cred_dir;
	if (!param(oauth_cred_dir, "SEC_CREDENTIAL_DIRECTORY_OAUTH")) {
		err.pushf("GetOAuth2Credential", 1, "Unable to retrieve OAuth2-style credentials for %s"
			" as SEC_CREDENTIAL_DIRECTORY_OAUTH is unset.", m_use_case.c_str());
		dprintf(D_ERROR, "%s\n", err.message());
		return false;
	}

	std::string fname, fullname;
	formatstr(fname, "%s.use", name.c_str());
	replace_str(fname, "*", "_");

	formatstr(fullname, "%s%c%s%c%s", oauth_cred_dir.c_str(), DIR_DELIM_CHAR, user.c_str(),
		DIR_DELIM_CHAR, fname.c_str());
	bool trust_cred_dir = param_boolean("TRUST_CREDENTIAL_DIRECTORY", false);
	const int verify_mode = trust_cred_dir ? 0 : SECURE_FILE_VERIFY_ALL;

	dprintf(D_SECURITY, "Credentials: loading %s (service name %s) for %s.\n", fullname.c_str(),
		name.c_str(), m_use_case.c_str());

	if (!read_secure_file(fullname.c_str(), (void**)(&cred.buf), &cred.len, true, verify_mode)) {
		dprintf(D_ERROR, "Failed to read credential file %s: %s\n",
			fullname.c_str(), errno ? strerror(errno) : "unknown error");
		return false;
	}

	return true;
}
