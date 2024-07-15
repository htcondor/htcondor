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


/**
 * Utilities for preparing a _CONDOR_CREDS directory given a job ad.
 */

#ifndef __CRED_DIR_H_
#define __CRED_DIR_H_

#include <string>

namespace classad {
	class ClassAd;
}

class CondorError;

namespace htcondor {

/**
 * A class for handling credential data.
 * For example, the implementation will memset to 0 data prior
 * to free'ing it
 */
struct CredData {
public:
	CredData() {}
	~CredData();

	CredData(const CredData& other) {
		len = other.len;
		if (len != 0) {
			buf = static_cast<unsigned char *>(malloc(other.len));
			memcpy(buf, other.buf, other.len);
		}
	}

	CredData(CredData &&other) {
		buf = other.buf;
		other.buf = nullptr;
		len = other.len;
		other.len = 0;
	}

	CredData& operator=(const CredData &other) {
		if (this != &other) {
			free(buf);
			len = other.len;
			if (len == 0) {
				buf = nullptr;
			} else {
				buf = static_cast<unsigned char *>(malloc(other.len));
				memcpy(buf, other.buf, other.len);
			}
		}
		return *this;
	}

	CredData& operator=(CredData&& other) {
		free(buf);
		buf = other.buf;
		other.buf = nullptr;
		len = other.len;
		other.len = 0;
		return *this;
	}

	unsigned char *buf{nullptr};
	size_t len{0};
};

/**
 * Given a job ad, create a corresponding directory full of credentials
 */
class CredDirCreator {

public:
	/**
	 * - ad: Job ad describing which services or credentials are needed.
	 * - cred_dir: Directory where credentials will be stored.
	 * - use_case: Advisory string for generating log messages.  For example, if `use_case` is
	 *   set to "job hook", a log string may say "Preparing credential directory for job hook".
	*/
	CredDirCreator(const classad::ClassAd &ad, const std::string &cred_dir, const std::string &use_case) :
		m_use_case(use_case),
		m_ad(ad),
		m_cred_dir(cred_dir)
	{}

	/**
	 * Given a job ad, create a corresponding directory full of credentials
	 * - err: On failure, this will be populated with relevant error message(s).
	 * - returns: True on success, failure otherwise
	 */
	bool PrepareCredDir(CondorError &err);

	/**
	 * Returns true if the object has created a credentials directory.
	 */
	bool MadeCredDir() const {return m_made_cred_dir;}

	/**
	 * Returns the credentials directory path
	 */
	const std::string &CredDir() const {return m_cred_dir;}

protected:
	// For log/error messages, the use case for the credential directory.
	const std::string m_use_case;

	// Set to true if the newly-created credentials directory should belong
	// to the user; if false, it'll be owned by PRIV_CONDOR
	bool m_creddir_user_priv{false};

private:
	/**
	 * Fetch the kerberos credential associated with the job.  Abstract methods.
	 * - user: Kerberos username for credential to retrieve.
	 * - domain: Kerberos domain for credential to retrieve.
	 * - cred: Output variable.  Binary contents of credential.
	 * - err: On failure, this will be populated with relevant error message(s).
	 * - returns: True on success, failure otherwise.
	 */
	virtual bool GetKerberosCredential(const std::string &user, const std::string &domain,
		CredData &cred, CondorError &err) = 0;

	/**
	 * Fetch the OAuth2 credential associated with the job with a given name.  Abstract methods.
	 * - service_name: Name of the OAuth2 credential to fetch.
	 * - user: Name of the login user
	 * - cred: Output variable.  Binary contents of credential.
	 * - err: On failure, this will be populated with relevant error message(s).
	 * - returns: True on success, failure otherwise.
	 */
	virtual bool GetOAuth2Credential(const std::string &service_name, const std::string &user,
		CredData &cred, CondorError &err) = 0;

	/**
	 * Write a given credential to a path.  Will ensure that the file is not readble by anyone
	 * other than root or the user.
	 * - path: Path for the credential contents
	 * - cred: Binary contents of credential
	 * - credlen: Length of the contents
	 * - err: On failure, this will be populated with relevant error message(s).
	 * - returns: True on success, failure otherwise.
	 */
	bool WriteToCredDir(const std::string &path, const CredData &cred, CondorError &err);

	// A reference to the job ad.
	const classad::ClassAd &m_ad;

	// The desired location of the output directory for credentials
	const std::string m_cred_dir;

	// Whether or not a credentials directory was created
	bool m_made_cred_dir{false};
};


// Shadow hook version of the cred dir creator.  It assumes it's run on the same host
// as the credmon / schedd, can just pull various files from disk, and condor owns
// the created credential directory
class LocalCredDirCreator : public CredDirCreator {
protected:
	LocalCredDirCreator(const classad::ClassAd &ad, const std::string &cred_dir, const std::string &use_case)
	: CredDirCreator(ad, cred_dir, use_case)
	{}

private:
	virtual bool GetKerberosCredential(const std::string &user, const std::string &domain, CredData &cred,
		CondorError &err) override;

	virtual bool GetOAuth2Credential(const std::string &service_name, const std::string &user, CredData &cred,
		CondorError &err) override;
};

// Shadow hook version of the cred dir creator.
class ShadowHookCredDirCreator final : public LocalCredDirCreator {
public:
	ShadowHookCredDirCreator(const classad::ClassAd &ad, const std::string &cred_dir) :
		LocalCredDirCreator(ad, cred_dir, "shadow prepare hook")
	{}
};

// Local universe version of the local cred dir creator.  Same as the shadow hook but the user owns the creds dir.
class LocalUnivCredDirCreator final : public LocalCredDirCreator {
public:
	LocalUnivCredDirCreator(const classad::ClassAd &ad, const std::string &cred_dir) :
		LocalCredDirCreator(ad, cred_dir, "local universe starter")
	{m_creddir_user_priv = true;}
};

}

#endif
