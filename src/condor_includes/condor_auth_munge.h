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


#ifndef CONDOR_AUTHENTICATOR_MUNGE
#define CONDOR_AUTHENTICATOR_MUNGE

#if !defined(SKIP_AUTHENTICATION) && defined(HAVE_EXT_MUNGE)

#include "condor_auth.h"        // Condor_Auth_Base class is defined here
#include "condor_crypt_3des.h"

class Condor_Auth_MUNGE : public Condor_Auth_Base {
public:
	Condor_Auth_MUNGE(ReliSock * sock);
	//------------------------------------------
	// Constructor
	//------------------------------------------

	~Condor_Auth_MUNGE();
	//------------------------------------------
	// Destructor
	//------------------------------------------

    static bool Initialize();
    // Perform one-time initialization, primarily dlopen()ing the
    // munge lib. Returns true on success, false on failure.

	int authenticate(const char * remoteHost, CondorError* errstack, bool non_blocking);
	//------------------------------------------
	// PURPOSE: authenticate with the other side
	// REQUIRE: hostAddr -- host to authenticate
	// RETURNS:
	//------------------------------------------

	int isValid() const;
	//------------------------------------------
	// PURPOSE: whether the authenticator is in
	//          valid state.
	// REQUIRE: None
	// RETURNS: 1 -- true; 0 -- false
	//------------------------------------------

	int wrap(const char* input, int input_len, char*& output, int& output_len);
	//------------------------------------------
	// PURPOSE: Encrypt the input buffer into an output buffer.  The output
	//          buffer must be dealloated by the caller with free().
	// REQUIRE: input buffer and output destination to be filled in
	// RETURNS: 1 -- success; 0 -- failure
	//------------------------------------------

	int unwrap(const char* input, int input_len, char*& output, int& output_len);
	//------------------------------------------
	// PURPOSE: Decrypt the input buffer into an output buffer.  The output
	//          buffer must be dealloated by the caller with free().
	// REQUIRE: input buffer and output destination to be filled in
	// RETURNS: 1 -- success; 0 -- failure
	//------------------------------------------

private:
	static bool m_initTried;
	static bool m_initSuccess;

	// the shared session key produced as output of the protocol
	Condor_Crypt_3des* m_crypto;
	Condor_Crypto_State* m_crypto_state;

	// Produce the shared key object from raw key material.
	bool setupCrypto(const unsigned char* key, const int keylen);

	// Encrypt data using the session key.  Called by wrap.
	bool encrypt(const unsigned char* input, int input_len, unsigned char* &output, int &output_len);

	// Decrypt data based on the session key.  Called by unwrap.
	bool decrypt(const unsigned char* input, int input_len, unsigned char* &output, int &output_len);

	// Decrypt or encrypt based on first argument.
	bool encrypt_or_decrypt(bool want_encrypt, const unsigned char *input, int input_len, unsigned char* &output, int &output_len);

};

#endif

#endif
