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
#include <openssl/err.h>
BIO *bio_err = 0;

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
    /* We are sure that the difference lies within the int range */
}

AXIS2_EXTERN SSL_CTX *AXIS2_CALL
axis2_ssl_utils_initialize_ctx(
    const axutil_env_t * env,
    axis2_char_t * server_cert,
    axis2_char_t * key_file,
    axis2_char_t * ssl_pp)
{
    SSL_METHOD *meth = NULL;
    SSL_CTX *ctx = NULL;
    axis2_char_t *ca_file = server_cert;

    if (!ca_file)
    {
        AXIS2_LOG_INFO(env->log, "[ssl client] CA certificate not specified");
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
    if (key_file) /*can we check if the server needs client auth? */
    {
        if (!ssl_pp)
        {
            AXIS2_LOG_INFO(env->log,
                "[ssl client] No passphrase specified for \
key file %s and server cert %s", key_file, server_cert);
        }

        SSL_CTX_set_default_passwd_cb_userdata(ctx, (void *) ssl_pp);
        SSL_CTX_set_default_passwd_cb(ctx, password_cb);

        if (!(SSL_CTX_use_certificate_chain_file(ctx, ca_file)))
        {
            AXIS2_LOG_ERROR(env->log, AXIS2_LOG_SI,
                "[ssl client] Loading client certificate failed \
, key file %s", key_file);
            ERR_print_errors(bio_err);
            ERR_print_errors_fp(stderr);
            SSL_CTX_free(ctx);
            return NULL;
        }

        if (!(SSL_CTX_use_PrivateKey_file(ctx, key_file, SSL_FILETYPE_PEM)))
        {
            AXIS2_LOG_ERROR(env->log, AXIS2_LOG_SI,
                "[ssl client] Loading client key failed, key file \
%s", key_file);
            SSL_CTX_free(ctx);
            ERR_print_errors(bio_err);
            ERR_print_errors_fp(stderr);
            return NULL;
        }
    }
    else
    {
        AXIS2_LOG_INFO(env->log,
            "[ssl client] Client certificate chain file"
            "not specified");
    }

    /* Load the CAs we trust */
    if (!(SSL_CTX_load_verify_locations(ctx, ca_file, 0)))
    {
        AXIS2_LOG_ERROR(env->log, AXIS2_LOG_SI,
            "[ssl client] Loading CA certificate failed, \
ca_file is %s", ca_file);
        SSL_CTX_free(ctx);
        return NULL;
    }

    return ctx;
}

AXIS2_EXTERN SSL *AXIS2_CALL
axis2_ssl_utils_initialize_ssl(
    const axutil_env_t * env,
    SSL_CTX * ctx,
    axis2_socket_t socket)
{
    SSL *ssl = NULL;
    BIO *sbio = NULL;

    AXIS2_PARAM_CHECK(env->error, ctx, NULL);

    ssl = SSL_new(ctx);
    if (!ssl)
    {
        AXIS2_LOG_ERROR (env->log, AXIS2_LOG_SI,
            "[ssl]unable to create new ssl context");
        return NULL;
    }

    sbio = BIO_new_socket((int)socket, BIO_NOCLOSE);
    if (!sbio)
    {
        AXIS2_LOG_ERROR (env->log, AXIS2_LOG_SI,
            "[ssl]unable to create BIO new socket for socket %d",
            (int)socket);
        return NULL;
    }

    SSL_set_bio(ssl, sbio, sbio);
//     int err;
//     if ((err = SSL_accept(ssl)) <= 0)
//     {
//         SSL_get_error(ssl,err);
//         printf("SSL_get_error from ssl_accept() = %d\n",err);
//         ERR_print_errors_fp(stderr);
//         //AXIS2_HANDLE_ERROR(env, AXIS2_ERROR_SSL_ENGINE, AXIS2_FAILURE);        
//         return NULL;
//     }

    if (SSL_get_verify_result(ssl) != X509_V_OK)
    {
        char sslerror[128]; /** error buffer must be at least 120 bytes long */
        X509 *peer_cert = NULL;
        X509_STORE *cert_store = NULL;
        X509_NAME *peer_name = NULL;
        X509_OBJECT *client_object = NULL;
        X509 *client_cert = NULL;

        peer_cert = SSL_get_peer_certificate(ssl);
        if (peer_cert && peer_cert->cert_info)
        {
            peer_name = (peer_cert->cert_info)->subject;
        }

        cert_store = SSL_CTX_get_cert_store(ctx);
        if (peer_name && cert_store)
        {
            client_object = X509_OBJECT_retrieve_by_subject(cert_store->objs,
                X509_LU_X509,
                peer_name);
        }
        if (client_object)
        {
            client_cert = (client_object->data).x509;
            if (client_cert &&
                (M_ASN1_BIT_STRING_cmp(client_cert->signature,
                        peer_cert->signature) == 0))
            {
                if (peer_cert)
                {
                    X509_free(peer_cert);
                }
                AXIS2_LOG_DEBUG(env->log, AXIS2_LOG_SI,
                    "[ssl client] SSL certificate verified against peer");
                return ssl;
            }
        }
        if (peer_cert)
        {
            X509_free(peer_cert);
        }
        ERR_error_string(SSL_get_verify_result(ssl), sslerror);
        AXIS2_LOG_ERROR(env->log, AXIS2_LOG_SI,
            "[ssl client] SSL certificate verification failed (%s)",
            sslerror);
        return NULL;
    }

    return ssl;
}

AXIS2_EXTERN axis2_status_t AXIS2_CALL
axis2_ssl_utils_cleanup_ssl(
    const axutil_env_t * env,
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
