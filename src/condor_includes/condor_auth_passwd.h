/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#ifndef CONDOR_AUTH_PASSWD
#define CONDOR_AUTH_PASSWD

#if !defined(SKIP_AUTHENTICATION) && defined(CONDOR_3DES_ENCRYPTION)

#include "condor_auth.h"        // Condor_Auth_Base class is defined here
#include "condor_crypt_3des.h"


class Condor_Auth_Passwd : public Condor_Auth_Base {
 public:

	///  Ctor
	Condor_Auth_Passwd(ReliSock * sock);

	/// Dtor
	~Condor_Auth_Passwd();

	int authenticate(const char * remoteHost, CondorError* errstack);

	/** Tells whether the authenticator is in a valid state, i.e.
		authentication has been successfully performed sometime in the past,
		whether or not calls to wrap/unwrap are permitted, etc.
		@returns 1 if state is valid, 0 if invalid
	*/
	int isValid() const;

	/** Encrypt the input buffer into an output buffer.  The output
		buffer must be dealloated by the caller with free().
		@returns 1 on success, 0 on failure
	*/
    int wrap(char* input, int input_len, char*& output, int& output_len);

	/** Decrypt the input buffer into an output buffer.  The output
		buffer must be dealloated by the caller with free().
		@returns 1 on success, 0 on failure
	*/    
    int unwrap(char* input, int input_len, char*& output, int& output_len);


 private:

	unsigned char* challenge;
	int challenge_len;

	bool create_challenge();


	Condor_Crypt_3des* crypto;
	bool setupCrypto(unsigned char* key, const int keylen);
	unsigned char* fetchPassword(const char* username,const char* localDomain_);

	bool encrypt(unsigned char *  input, 
                 int              input_len, 
                 unsigned char *& output, 
                 int&             output_len);

	bool decrypt(unsigned char *  input, 
                 int              input_len, 
                 unsigned char *& output, 
                 int&             output_len);

    bool encrypt_or_decrypt(bool  want_encrypt,
				 unsigned char *  input, 
                 int              input_len, 
                 unsigned char *& output, 
                 int&             output_len);

	void compute_response(unsigned char* & response_buffer, int & response_len);

	int authenticate_server();
	int authenticate_client();

};

#endif

#endif
