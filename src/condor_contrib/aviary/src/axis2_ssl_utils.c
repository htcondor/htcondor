/*
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "axis2_ssl_utils.h"
#include "openssl/ssl.h"
#include "openssl/x509.h"
#include "openssl/x509_vfy.h"
#include "openssl/x509v3.h"
#include "openssl/err.h"

BIO *bio_err = 0;
static axutil_log_t* local_log = NULL;

/* disable password callbacks for now */
/*
static int
password_cb(
    char *buf,
    int size,
    int rwflag,
    void *passwd)
{
    strncpy(buf, (char *) passwd, size);
    buf[size - 1] = '\0';
    return (int)(strlen(buf));
}
*/

int verify_callback(int ok, X509_STORE_CTX *store)
{
    char data[256];
 
    if (!ok) {
        X509 *cert = X509_STORE_CTX_get_current_cert(store);
        int  depth = X509_STORE_CTX_get_error_depth(store);
        int  err = X509_STORE_CTX_get_error(store);

        AXIS2_LOG_INFO (local_log, "[ssl] error depth set to: %i", depth );
        X509_NAME_oneline( X509_get_issuer_name( cert ), data, 256 );
        AXIS2_LOG_INFO (local_log, "[ssl]  issuer   = %s", data );
        X509_NAME_oneline( X509_get_subject_name( cert ), data, 256 );
        AXIS2_LOG_INFO (local_log, "[ssl]  subject  = %s", data );
        AXIS2_LOG_INFO (local_log, "[ssl]  err %i:%s", err, X509_verify_cert_error_string( err ) );
    }
 
    return ok;
}

AXIS2_EXTERN SSL_CTX *AXIS2_CALL
axis2_ssl_utils_initialize_ctx(
    const axutil_env_t * env,
    axis2_char_t * server_cert,
    axis2_char_t * server_key,
    axis2_char_t *ca_file,
    axis2_char_t *ca_dir,
    axis2_char_t * ssl_pp)
{
    const SSL_METHOD *meth = NULL;
    SSL_CTX *ctx = NULL;
    char* cipherlist = "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH";
    local_log = env->log;

	// need one or the other
    if (!ca_dir && !ca_file)
    {
        AXIS2_LOG_INFO(env->log, "[ssl] neither CA certificate file nor directory specified");
        AXIS2_HANDLE_ERROR(env, AXIS2_ERROR_SSL_NO_CA_FILE, AXIS2_FAILURE);
        return NULL;
    }

    if (!bio_err)
    {
        /* Global system initialization */
        SSL_library_init();
        SSL_load_error_strings();

        /* An error write context */
        bio_err = BIO_new_fp(stderr, BIO_NOCLOSE);
    }

    /* Create our context */
    meth = SSLv23_method();
    ctx = SSL_CTX_new(meth);

    /* Load our keys and certificates
     * If we need client certificates it has to be done here
     */
    if (server_key) /*can we check if the server needs client auth? */
    {
        if (!ssl_pp)
        {
        /* disable password checking on the key for now...impractical for server-side */
        /*
            AXIS2_LOG_INFO(env->log,
                "[ssl] No passphrase specified for key file %s and server cert %s", server_key, server_cert);
        */
        }
        /* SSL_CTX_set_default_passwd_cb_userdata(ctx, (void *) ssl_pp); */
        /* SSL_CTX_set_default_passwd_cb(ctx, password_cb); */

        if (!(SSL_CTX_use_certificate_chain_file(ctx, server_cert)))
        {
            AXIS2_LOG_ERROR(env->log, AXIS2_LOG_SI,
                            "[ssl] Loading server certificate failed, cert file '%s'", server_cert);
            SSL_CTX_free(ctx);
            return NULL;
        }

        if (!(SSL_CTX_use_PrivateKey_file(ctx, server_key, SSL_FILETYPE_PEM)))
        {
            AXIS2_LOG_ERROR(env->log, AXIS2_LOG_SI,
                "[ssl] Loading server key failed, key file '%s'", server_key);
            SSL_CTX_free(ctx);
            return NULL;
        }
    }
    else
    {
        AXIS2_LOG_INFO(env->log,
            "[ssl] Server key file not specified");
        return NULL;
    }

    /* Load the CAs we trust */
    if (!(SSL_CTX_load_verify_locations(ctx, ca_file, ca_dir)))
    {
        AXIS2_LOG_ERROR(env->log, AXIS2_LOG_SI,
            "[ssl] Loading CA certificate failed, ca_file is '%s', ca_dir is '%s'", ca_file, ca_dir);
        SSL_CTX_free(ctx);
        return NULL;
    }
        
    SSL_CTX_set_verify( ctx, SSL_VERIFY_PEER|SSL_VERIFY_FAIL_IF_NO_PEER_CERT, verify_callback );
    SSL_CTX_set_verify_depth( ctx, 4 );
    // prohibit v2 vulnerabilities
    SSL_CTX_set_options( ctx, SSL_OP_ALL|SSL_OP_NO_SSLv2 );
    if (SSL_CTX_set_cipher_list( ctx, cipherlist ) != 1 ) {
        AXIS2_LOG_INFO (env->log, "[ssl] Error setting cipher list (no valid ciphers)" );
        SSL_CTX_free(ctx);
        return NULL;
    }

    return ctx;
}

AXIS2_EXTERN SSL *AXIS2_CALL
axis2_ssl_utils_initialize_ssl(
    const axutil_env_t * env,
    SSL_CTX * ctx,
    axis2_socket_t _socket)
{
    SSL *ssl = NULL;
    BIO *sbio = NULL;

    AXIS2_PARAM_CHECK(env->error, ctx, NULL);

    ssl = SSL_new(ctx);
    if (!ssl)
    {
        AXIS2_LOG_ERROR (env->log, AXIS2_LOG_SI,
            "[ssl] Unable to create new ssl context");
        return NULL;
    }

    sbio = BIO_new_socket((int)_socket, BIO_NOCLOSE);
    if (!sbio)
    {
        AXIS2_LOG_ERROR (env->log, AXIS2_LOG_SI,
            "[ssl] Unable to create BIO new socket for socket %d",socket);
        if (ssl) {
            SSL_shutdown(ssl);
            SSL_free(ssl);
        }
        return NULL;
    }

    SSL_set_bio(ssl, sbio, sbio);
    int err;
    if ((err = SSL_accept(ssl)) <= 0)
    {
        SSL_get_error(ssl,err);
        AXIS2_LOG_ERROR (env->log, AXIS2_LOG_SI,"[ssl] SSL_accept failed = %d",err);
        if (ssl) {
            SSL_shutdown(ssl);
            SSL_free(ssl);
        }
        return NULL;
    }
    
    X509* peer = NULL;
    if ((peer = SSL_get_peer_certificate(ssl)) != NULL) {
        long int vok;
        if ((vok = SSL_get_verify_result(ssl)) == X509_V_OK) {
            AXIS2_LOG_INFO (env->log, "[ssl] Client verified OK");
        }
        else {
            AXIS2_LOG_ERROR (env->log, AXIS2_LOG_SI,"[ssl] Client verify failed: %d", vok);
        }
    }
    else {
        AXIS2_LOG_ERROR (env->log, AXIS2_LOG_SI,"[ssl] Client certificate not presented");
        if (ssl) {
            SSL_shutdown(ssl);
            SSL_free(ssl);
        }
        return NULL;
    }


    return ssl;
}

AXIS2_EXTERN axis2_status_t AXIS2_CALL
axis2_ssl_utils_cleanup_ssl(
    /*const axutil_env_t * env,*/
    SSL_CTX * ctx,
    SSL * ssl)
{

    if (ssl)
    {
        SSL_shutdown(ssl);
    }
    if (ctx)
    {
        SSL_CTX_free(ctx);
    }
    return AXIS2_SUCCESS;
}
