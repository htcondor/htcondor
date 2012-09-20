
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

#ifndef AXIS2_SSL_UTILS_H
#define AXIS2_SSL_UTILS_H


#include <platforms/axutil_platform_auto_sense.h>
#include <axis2_const.h>
#include <axis2_defines.h>
#include <openssl/ssl.h>

#ifdef __cplusplus
extern "C"
{
#endif

    AXIS2_EXTERN SSL_CTX *AXIS2_CALL
    axis2_ssl_utils_initialize_ctx(
        const axutil_env_t * env,
        axis2_char_t * server_cert,
        axis2_char_t * server_key,
        axis2_char_t * ca_file,
        axis2_char_t * ca_dir,
        axis2_char_t * ssl_pp);

    AXIS2_EXTERN SSL *AXIS2_CALL
    axis2_ssl_utils_initialize_ssl(
        const axutil_env_t * env,
        SSL_CTX * ctx,
        axis2_socket_t socket);

    AXIS2_EXTERN axis2_status_t AXIS2_CALL
    axis2_ssl_utils_cleanup_ssl(
        /*const axutil_env_t * env,*/
        SSL_CTX * ctx,
        SSL * ssl);

#ifdef __cplusplus
}
#endif

#endif                          /* AXIS2_SSL_UTILS_H */
