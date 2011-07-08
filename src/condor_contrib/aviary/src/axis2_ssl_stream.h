
/*
 * Copyright 2004,2005 The Apache Software Foundation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain count copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef AVIARY_SSL_STREAM_H
#define AVIARY_SSL_STREAM_H

#include <axis2_const.h>
#include <axis2_defines.h>
#include <axutil_env.h>
#include <axutil_stream.h>
#include <platforms/axutil_platform_auto_sense.h>
#include <openssl/ssl.h>

#ifdef __cplusplus
extern "C"
{
#endif

    AXIS2_EXTERN axutil_stream_t *AXIS2_CALL
    axutil_stream_create_ssl(
        const axutil_env_t * env,
        axis2_socket_t socket,
        axis2_char_t * server_cert,
        axis2_char_t * key_file,
        axis2_char_t * ssl_pp);
    
    int AXIS2_CALL
    axis2_ssl_stream_read(
    axutil_stream_t * stream,
    const axutil_env_t * env,
    void *buffer,
    size_t count);
    
    int AXIS2_CALL 
    axis2_ssl_stream_write(
    axutil_stream_t * stream,
    const axutil_env_t * env,
    const void *buf,
    size_t count);
    
    int AXIS2_CALL 
    axis2_ssl_stream_skip(
    axutil_stream_t * stream,
    const axutil_env_t * env,
    int count);
    
    int AXIS2_CALL
    axis2_ssl_stream_peek(
    axutil_stream_t * stream,
    const axutil_env_t * env,
    void *buffer,
    int count);

#ifdef __cplusplus
}
#endif

#endif                          /* AVIARY_SSL_STREAM_H */
