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

#if !defined(SKIP_AUTHENTICATION) && defined(HAVE_EXT_MUNGE)
#include <stdlib.h>
#include <munge.h>
#include "condor_auth_munge.h"
#include "condor_environ.h"
#include "CondorError.h"
#include "condor_mkstemp.h"
#include "ipv6_hostname.h"

#if defined(DLOPEN_SECURITY_LIBS)
#include <dlfcn.h>
#endif

// Symbols from the munge library
static munge_err_t (*munge_encode_ptr)(char **, munge_ctx_t, const void *, int) = NULL;
static munge_err_t (*munge_decode_ptr)(const char *, munge_ctx_t, void **, int *, uid_t *, gid_t *) = NULL;
static const char * (*munge_strerror_ptr)(munge_err_t) = NULL;

bool Condor_Auth_MUNGE::m_initTried = false;
bool Condor_Auth_MUNGE::m_initSuccess = false;

Condor_Auth_MUNGE :: Condor_Auth_MUNGE(ReliSock * sock)
    : Condor_Auth_Base    ( sock, CAUTH_MUNGE ),
	m_crypto(NULL), m_crypto_state(NULL)
{
	ASSERT( Initialize() == true );
}

Condor_Auth_MUNGE :: ~Condor_Auth_MUNGE()
{
}

bool Condor_Auth_MUNGE::Initialize()
{
	if ( m_initTried ) {
		return m_initSuccess;
	}

#if defined(DLOPEN_SECURITY_LIBS)
	void *dl_hdl;

	if ( (dl_hdl = dlopen(LIBMUNGE_SO, RTLD_LAZY)) == NULL ||
		 !(munge_encode_ptr = (munge_err_t (*)(char **, munge_ctx_t, const void *, int))dlsym(dl_hdl, "munge_encode")) ||
		 !(munge_decode_ptr = (munge_err_t (*)(const char *, munge_ctx_t, void **, int *, uid_t *, gid_t *))dlsym(dl_hdl, "munge_decode")) ||
		 !(munge_strerror_ptr = (const char * (*)(munge_err_t))dlsym(dl_hdl, "munge_strerror"))
		 ) {

		// Error in the dlopen/sym calls, return failure.
		const char *err_msg = dlerror();
		dprintf( D_ALWAYS, "Failed to open Munge library: %s\n",
				 err_msg ? err_msg : "Unknown error" );
		m_initSuccess = false;
	} else {
		m_initSuccess = true;
	}
#else
	munge_encode_ptr = munge_encode;
	munge_decode_ptr = munge_decode;
	munge_strerror_ptr = munge_strerror;

	m_initSuccess = true;
#endif

	m_initTried = true;
	return m_initSuccess;
}

int Condor_Auth_MUNGE::authenticate(const char * /* remoteHost */, CondorError* errstack, bool /* non_blocking */)
{
	int client_result = -1;
	int server_result = -1;
	int fail = -1 == 0;
	char *munge_token = NULL;
	munge_err_t err;

	if ( mySock_->isClient() ) {

		// Generate a "payload" that will be sent (securely) to the
		// server.  This payload will become the encryption key if
		// encryption or integrity is enabled on this connection.
		// Without this, keys would just be exchanged in the clear.
		unsigned char* key = Condor_Crypt_Base::randomKey(24);

		// Until session caching supports clients that present
		// different identities to the same service at different
		// times, we want condor daemons to always authenticate
		// as condor priv, rather than as the current euid.
		// For tools and daemons not started as root, this
		// is a no-op.
		priv_state saved_priv = set_condor_priv();
		err = (*munge_encode_ptr) (&munge_token, NULL, key, 24);
		set_priv(saved_priv);

		if ( err != EMUNGE_SUCCESS ) {
			dprintf(D_ALWAYS, "AUTHENTICATE_MUNGE: Client error: %i: %s\n", err, (*munge_strerror_ptr) (err));
			errstack->pushf("MUNGE", 1000,  "Client error: %i: %s", err, (*munge_strerror_ptr) (err));

			// send the text of the error as the token so we stay sync in on
			// the wire protocol and the other side can print out a reason.
			munge_token = strdup((*munge_strerror_ptr)(err));
			client_result = -1;
		} else {
			// success on client side
			dprintf(D_SECURITY, "AUTHENTICATE_MUNGE: Client succeeded.\n");
			client_result = 0;
			setupCrypto(key, 24);
		}

		// key no longer needed.
		free(key);

		dprintf (D_SECURITY | D_FULLDEBUG, "AUTHENTICATE_MUNGE: sending client_result %i, munge_token %s\n", client_result, munge_token);

		// send over result as a success/failure indicator (-1 == failure)
		mySock_->encode();
		if (!mySock_->code( client_result ) || !mySock_->code( munge_token ) || !mySock_->end_of_message()) {
			dprintf(D_ALWAYS, "Protocol failure at %s, %d!\n", __FUNCTION__, __LINE__);
			errstack->pushf("MUNGE", 1001,  "Protocol failure at %s, %d!\n", __FUNCTION__, __LINE__);
			client_result = -1;
		}

		// whether this was a real token or an error string we are finished with it now.
		free(munge_token);

		// abort protocol now if we sent client failure or had protocol failure
		if(client_result==-1) {
			return fail;
		}

		// now let the server verify
		mySock_->decode();
		if (!mySock_->code( server_result ) || !mySock_->end_of_message()) {
			dprintf(D_ALWAYS, "Protocol failure at %s, %d!\n", __FUNCTION__, __LINE__);
			errstack->pushf("MUNGE", 1002,  "Protocol failure at %s, %d!\n", __FUNCTION__, __LINE__);
			return fail;
		}

		dprintf( D_SECURITY, "AUTHENTICATE_MUNGE:  Server sent: %d\n", server_result);

		// this function returns TRUE on success, FALSE on failure,
		// which is just the opposite of server_result.
		return( server_result == 0 );

	} else {
		uid_t uid;
		gid_t gid;

		// server code
		setRemoteUser( NULL );

		mySock_->decode();
		if (!mySock_->code( client_result ) || !mySock_->code( munge_token ) || !mySock_->end_of_message()) {
			dprintf(D_ALWAYS, "Protocol failure at %s, %d!\n", __FUNCTION__, __LINE__);
			errstack->pushf("MUNGE", 1003,  "Protocol failure at %s, %d!\n", __FUNCTION__, __LINE__);
			if(munge_token) {
				free(munge_token);
			}
			return fail;
		}

		dprintf (D_SECURITY | D_FULLDEBUG, "AUTHENTICATE_MUNGE: received client_result %i, munge_token %s\n", client_result, munge_token);

		if (client_result != 0) {
			// in this case the token is actually the error message client encountered.
			dprintf(D_ALWAYS, "AUTHENTICATE_MUNGE: Client had error: %s, aborting.\n", munge_token);
			errstack->pushf("MUNGE", 1004, "Client had error: %s", munge_token);
			free(munge_token);
			return fail;
		} else {
			dprintf(D_SECURITY, "AUTHENTICATE_MUNGE: Client succeeded.\n");
		}

		unsigned char *key;
		int   len;
		err = (*munge_decode_ptr) (munge_token, NULL, (void**)&key, &len, &uid, &gid);
		free(munge_token);

		if (err != EMUNGE_SUCCESS) {
			dprintf(D_ALWAYS, "AUTHENTICATE_MUNGE: Server error: %i: %s.\n", err, (*munge_strerror_ptr)(err));
			errstack->pushf("MUNGE", 1005, "Server error: %i: %s", err, (*munge_strerror_ptr)(err));
			server_result = -1;
		} else {
			char *tmpOwner = my_username( uid );
			if (!tmpOwner) {
				// this could happen if, for instance,
				// getpwuid() failed.
				dprintf(D_ALWAYS, "AUTHENTICATE_MUNGE: Unable to lookup uid %i\n", uid);
				server_result = -1;
				errstack->pushf("MUNGE", 1006,
						"Unable to lookup uid %i", uid);
			} else {
				dprintf(D_SECURITY, "AUTHENTICATE_MUNGE: Server believes client is uid %i (%s).\n", uid, tmpOwner);
				server_result = 0;	// 0 means success here. sigh.
				setRemoteUser( tmpOwner );
				setAuthenticatedName( tmpOwner );
				free( tmpOwner );
				setRemoteDomain( getLocalDomain() );
				setupCrypto(key, len);
			}
		}

		// free the key (payload) from munge_decode
		free(key);

		mySock_->encode();
		if (!mySock_->code( server_result ) || !mySock_->end_of_message()) {
			dprintf(D_ALWAYS, "Protocol failure at %s, %d!\n", __FUNCTION__, __LINE__);
			errstack->pushf("MUNGE", 1007,  "Protocol failure at %s, %d!\n", __FUNCTION__, __LINE__);
			return fail;
		}

		dprintf(D_SECURITY, "AUTHENTICATE_MUNGE: Server sent final result to client: %i\n", server_result);

		// this function returns TRUE on success, FALSE on failure,
		// which is just the opposite of server_result.
		return server_result == 0;
	}
}

int Condor_Auth_MUNGE :: isValid() const
{
    return TRUE;
}

bool
Condor_Auth_MUNGE::setupCrypto(const unsigned char* key, const int keylen)
{
	// get rid of any old crypto object
	if ( m_crypto ) delete m_crypto;
	m_crypto = NULL;
	if ( m_crypto_state ) delete m_crypto_state;
	m_crypto_state = NULL;

	if ( !key || !keylen ) {
		// cannot setup anything without a key
		return false;
	}

	// This could be 3des -- maybe we should use "best crypto" indirection.
	KeyInfo thekey(key, keylen, CONDOR_3DES, 0);
	m_crypto = new Condor_Crypt_3des();
	if ( m_crypto ) {
		m_crypto_state = new Condor_Crypto_State(CONDOR_3DES,thekey);
		// if this failed, clean up other mem
		if ( !m_crypto_state ) {
			delete m_crypto;
			m_crypto = NULL;
		}
	}

	return m_crypto ? true : false;
}

bool
Condor_Auth_MUNGE::encrypt(const unsigned char* input, int input_len, unsigned char* &output, int &output_len)
{
	return encrypt_or_decrypt(true,input,input_len,output,output_len);
}

bool
Condor_Auth_MUNGE::decrypt(const unsigned char* input, int input_len, unsigned char* &output, int &output_len)
{
	return encrypt_or_decrypt(false,input,input_len,output,output_len);
}

bool
Condor_Auth_MUNGE::encrypt_or_decrypt(bool /*want_encrypt*/, const unsigned char* /*input*/, int /*input_len*/, unsigned char* &/*output*/, int &/*output_len*/)
{

	// what is going on here?  munge has 3DES hard-coded to do wrap/unwrap.
	// this isn't negotiated or configurable.  i'm working on #7725 so not
	// digging more into this now.
	return false;

#ifdef THIS_DEAD_CODE_WILL_BE_REINSTATED_WITH_7725
	bool result;

	// clean up any old buffers that perhaps were left over
	if ( output ) free(output);
	output = NULL;
	output_len = 0;

	// check some intput params
	if (!input || input_len < 1) {
		return false;
	}

	// make certain we got a crypto object
	if (!m_crypto || !m_crypto_state) {
		dprintf(D_SECURITY, "In Condor_Auth_MUNGE.  Found NULL m_crypto or m_crypto_state!\n");
		return false;
	}

	// do the work
	m_crypto_state->reset();
	if (want_encrypt) {
		result = m_crypto->encrypt(m_crypto_state, input,input_len,output,output_len);
	} else {
		result = m_crypto->decrypt(m_crypto_state, input,input_len,output,output_len);
	}

	// mark output_len as zero upon failure
	if (!result) {
		output_len = 0;
	}

	// an output_len of zero means failure; cleanup and return
	if ( output_len == 0 ) {
		if ( output ) free(output);
		output = NULL;
		return false;
	}

	// if we made it here, we're golden!
	return true;
#endif
}

int
Condor_Auth_MUNGE::wrap(const char *input, int input_len, char* &output, int &output_len)
{
	bool result;
	const unsigned char* in = (const unsigned char*)input;
	unsigned char* out = (unsigned char*)output;
	dprintf(D_SECURITY, "In Condor_Auth_MUNGE::wrap.\n");
	result = encrypt(in,input_len,out,output_len);

	output = (char *)out;

	return result ? TRUE : FALSE;
}

int
Condor_Auth_MUNGE::unwrap(const char *input, int input_len, char* &output, int &output_len)
{
	bool result;
	const unsigned char* in = (const unsigned char*)input;
	unsigned char* out = (unsigned char*)output;

	dprintf(D_SECURITY, "In Condor_Auth_MUNGE::unwrap.\n");
	result = decrypt(in,input_len,out,output_len);

	output = (char *)out;

	return result ? TRUE : FALSE;
}

#endif
