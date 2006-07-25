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

#ifndef CONDOR_AUTHENTICATOR_OPENSSL
#define CONDOR_AUTHENTICATOR_OPENSSL

#if !defined(SKIP_AUTHENTICATION) && defined(SSL_AUTHENTICATION)

#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/err.h>
#include "condor_auth.h"        // Condor_Auth_Base class is defined here
#include "condor_crypt_3des.h"

#define AUTH_SSL_BUF_SIZE         1048576
#define AUTH_SSL_ERROR            -1
#define AUTH_SSL_A_OK             0
#define AUTH_SSL_SENDING          1
#define AUTH_SSL_RECEIVING        2
#define AUTH_SSL_QUITTING         3
#define AUTH_SSL_HOLDING          4
#define AUTH_SSL_ROLE_CLIENT      5
#define AUTH_SSL_ROLE_SERVER      6

#define AUTH_SSL_SESSION_KEY_LEN  256

#define AUTH_SSL_CIPHERLIST_STR "AUTH_SSL_CIPHERLIST"

#define AUTH_SSL_CLIENT_CAFILE_STR "AUTH_SSL_CLIENT_CAFILE"
#define AUTH_SSL_CLIENT_CADIR_STR "AUTH_SSL_CLIENT_CADIR"
#define AUTH_SSL_CLIENT_CERTFILE_STR "AUTH_SSL_CLIENT_CERTFILE"
#define AUTH_SSL_CLIENT_KEYFILE_STR "AUTH_SSL_CLIENT_KEYFILE"

#define AUTH_SSL_SERVER_CAFILE_STR "AUTH_SSL_SERVER_CAFILE"
#define AUTH_SSL_SERVER_CADIR_STR "AUTH_SSL_SERVER_CADIR"
#define AUTH_SSL_SERVER_CERTFILE_STR "AUTH_SSL_SERVER_CERTFILE"
#define AUTH_SSL_SERVER_KEYFILE_STR "AUTH_SSL_SERVER_KEYFILE"

#define AUTH_SSL_DEFAULT_CIPHERLIST "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH"

class Condor_Auth_SSL : public Condor_Auth_Base {
 public:
    Condor_Auth_SSL(ReliSock * sock, int remote = 0);
    //------------------------------------------
    // Constructor
    //------------------------------------------

    ~Condor_Auth_SSL();
    //------------------------------------------
    // Destructor
    //------------------------------------------

    int authenticate(const char * remoteHost, CondorError* errstack);
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

    int wrap(char* input, int input_len, char*& output, int& output_len);
    int unwrap(char* input, int input_len, char*& output, int& output_len);

 private:

    int init_OpenSSL(void);
    SSL_CTX *setup_ssl_ctx(bool is_server);
    int client_share_status(int client_status);
    int receive_status(int &status);
    int send_status(int status);
    int server_share_status(int server_status);
    int send_message(int status, char *buf, int len);
    int receive_message(int &status, int &len, char *buf);
    int server_send_message( int server_status, char *buf,
                             BIO *conn_in, BIO *conn_out );
    int server_receive_message( int server_status, char *buf,
                                BIO *conn_in, BIO *conn_out );
    int client_send_message( int server_status, char *buf,
                             BIO *conn_in, BIO *conn_out );
    int client_receive_message( int server_status, char *buf,
                                BIO *conn_in, BIO *conn_out );
    int server_exchange_messages(int server_status, char *buf,
                                 BIO *conn_in, BIO *conn_out);
    int client_exchange_messages(int client_status, char *buf,
                                 BIO *conn_in, BIO *conn_out);
//    int verify_callback(int ok, X509_STORE_CTX *store);
    long post_connection_check(SSL *ssl, int role);

		/** This stores the shared session key produced as output of
			the protocol. 
		*/
	Condor_Crypt_3des* m_crypto;

		/** Produce the shared key object from raw key material.
		 */
	bool setup_crypto(unsigned char* key, const int keylen);

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
	
};

#endif

#endif
