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


#ifndef CONDOR_AUTH_PASSWD
#define CONDOR_AUTH_PASSWD

#if !defined(SKIP_AUTHENTICATION) && defined(HAVE_EXT_OPENSSL)

#include "condor_auth.h"        // Condor_Auth_Base class is defined here
#include "condor_crypt_3des.h"




#define AUTH_PW_KEY_LEN 256
#define AUTH_PW_MAX_NAME_LEN 1024

#define AUTH_PW_ERROR          -1
#define AUTH_PW_A_OK           0
#define AUTH_PW_ABORT          1

/** A class to implement the AKEP2 protocol for password-based authentication.
 
  Refer to _The Handbook of Applied Cryptography_, Menezes, et. al.,
  fifth printing, p. 499, section 12.20, for more info on the AKEP2 protocol used
  here.  The protocol uses 1 1/2 round trips for a total of three
  messages.  ISBN number 0-8493-8523-7.  Note this book is available online
  at URL http://www.cacr.math.uwaterloo.ca/hac/.

  An overview of the protocol follows:

	SUMMARY: A and B exchange 3 messages to derive a session key W.

	RESULT: mutual entity authentication, and implicit key authentication of W.

	1. Setup: A and B share long-term symmetric keys K, K' (these should differ but need
	not be independent). hK is a MAC (keyed hash function) used for entity 
	authentication.  h'K' is a pseudorandom permutation or keyed 
	one-way function used for key derivation.

	2. Protocol messages.  Define  T = (B, A, rA, rB).
		A -> B : rA 				(1)
		A <- B : T, hK(T) 			(2)
		A -> B : (A, rB), hK(A, rB) (3)
		W = h'K'(rB)

	3. Protocol actions. Perform the following steps for each shared key required.

	(a) A selects and sends to B a random number rA.

	(b) B selects a random number rB and sends to A the values (B,A,rA,rB), along
	with a MAC over these quantities generated using h with key K.

	(c) Upon receiving message (2), A checks the identities are proper, that the 
	rA received matches that in (1), and verifies the MAC.

	(d) A then sends to B the values (A,rB), along with a MAC thereon.

	(e) Upon receiving (3), B verifies that the MAC is correct, and that the received
	value rB matches that sent earlier.

	(f) Both A and B compute the session key as W = h'K'(rB).

 */


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

		/// This structure stores the message referred to as T in the book.
	struct msg_t_buf {
		char *a;              // The name of the client.
		char *b;              // The name of the server.
		unsigned char *ra;    // The random string produced by the client.
		unsigned char *rb;    // The random string produced by the server.
		unsigned char *hkt;   // The result of hashing T with K.
		unsigned int hkt_len; 
		unsigned char *hk;    // The result of hashing a,rb with K
		unsigned int hk_len;
	};

		/// This structure stores the shared key and its derivatives.
	struct sk_buf {
		volatile char *shared_key;          // The shared key.  
		int len;                            // Of the shared key.
		volatile unsigned char *ka;         // K
		int ka_len;
		volatile unsigned char *kb;         // K'
		int kb_len;
	};

		/** This stores the shared session key produced as output of
			the protocol. 
		*/
	Condor_Crypt_3des* m_crypto;

		/** Produce the shared key object from raw key material.
		 */
	bool setupCrypto(unsigned char* key, const int keylen);

		/** Lookup a shared key based on the correspondent's
			information.  
		*/
	volatile char* fetchPassword(const char* nameA,
								 const char* nameB);

		/** Return a malloc-ed string "user@domain" that represents who we
		 	are.
		 */
	char* fetchLogin();

		/** Encrypt data using the session key.  Called by wrap. 
		 */
	bool encrypt(unsigned char *  input, 
                 int              input_len, 
                 unsigned char *& output, 
                 int&             output_len);
	
		/* Decrypt data based on the session key.  Called by unwrap.
		 */
	bool decrypt(unsigned char *  input, 
                 int              input_len, 
                 unsigned char *& output, 
                 int&             output_len);

		/** Decrypt or encrypt based on first argument. 
		 */
	bool encrypt_or_decrypt(bool want_encrypt,
							unsigned char *  input,
							int              input_len,
							unsigned char *& output,
							int&             output_len);
	
		/** The protocol in the book uses long term symmetric keys K
			and K'.  "These should differ but need not be
			independent."  We derive these keys from the shared key by
			hmacing it with two different keys.  This relies on the
			difficulty of calculating the input to an hmac given its
			output, even when the key is known. 
		*/
	bool setup_shared_keys(struct sk_buf *sk);

		/** The "seed" consists of the two keys used to hmac the
			shared key.  Each byte in the seed keys is specified in
			this function.  
		*/
	void setup_seed(unsigned char *ka, unsigned char *kb);

		/** This is just a wrapper for the openssl HMAC function. */
	void hmac(unsigned char *sk, int sk_len,
			  unsigned char *key, int key_len,
			  unsigned char *result, unsigned int *result_len);
		/** Fill the structure with NULLS. */
	void init_sk(struct sk_buf *sk);
		/** Free the elements of the structure, explicitly zeroing key
			material. 
		*/
	void destroy_sk(struct sk_buf *sk);
		/** Fill the stucture with NULLS. */
	void init_t_buf(struct msg_t_buf *t);
		/** Free the elements of the structure. */
	void destroy_t_buf(struct msg_t_buf *t);
		/** Generate the random string. */
	int client_gen_rand_ra(struct msg_t_buf *t);
		/** Generate the random string. */
	int server_gen_rand_rb(struct msg_t_buf *t);
		/** Client side of the first protocol mesage. */
	int client_send_one(int client_status, struct msg_t_buf *t_client);
		/** Client side of the first protocol message. */
	int client_receive(int *client_status, struct msg_t_buf *t_server);
		/** Client check of second message validity. */
	int client_check_t_validity(struct msg_t_buf *t_client, 
								struct msg_t_buf *t_server, 
								struct sk_buf *sk);
		/** Client side of the third message. */
	int client_send_two(int client_status, 
						struct msg_t_buf *t_client, 
						struct sk_buf *sk);
//	void set_session_key(struct msg_t_buf *t_client);
		/** Calculate the hash on message three. */
	bool calculate_hk(struct msg_t_buf *t_buf, struct sk_buf *sk);
		/** Calculate the hash on message two. */
	bool calculate_hkt(struct msg_t_buf *t_buf, struct sk_buf *sk);
		/** Server side of the first message. */
	int server_receive_one(int *server_status, struct msg_t_buf *t_client);
		/** Server side of the second message. */
	int server_send(int server_status, 
					struct msg_t_buf *t_server, struct sk_buf *sk);
		/** Server check of third message hash. */
	int server_check_hk_validity(struct msg_t_buf *t_client, 
								 struct msg_t_buf *t_server, 
								 struct sk_buf *sk);
		/** Server side of the third message. */
	int server_receive_two(int *server_status, struct msg_t_buf *t_client);
		/** Both sides call this when complete to set the session key. */
	bool set_session_key(struct msg_t_buf *t_buf, struct sk_buf *sk);
		/** See Secure Programming Cookbook for C/C++.  Secure zeroize
			for memory containing keys. 
		*/
	volatile void *spc_memset(volatile void *dst, int c, size_t len);
		//void print_binary(volatile unsigned char *buf, int len);
};

#endif /* SKIP_AUTHENTICATION */

#endif /* CONDOR_AUTH_PASSWD */
