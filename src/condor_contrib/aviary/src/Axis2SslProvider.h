/***************************************************************
 *
 * Copyright (C) 2009-2011 Red Hat, Inc.
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

#ifndef _AXIS2_SSL_PROVIDER_H
#define _AXIS2_SSL_PROVIDER_H

#include "Axis2SoapProvider.h"

// lifted out from simple_http_svr_conn.c
typedef struct axis2_simple_http_svr_conn
{
    int socket;
    axutil_stream_t *stream;
    axis2_bool_t keep_alive;
} axis2_simple_http_svr_conn_t;

#include <openssl/ssl.h>

namespace aviary {
namespace soap {

class Axis2SslProvider: public Axis2SoapProvider {
    public:
        Axis2SslProvider(int _log_level=AXIS2_LOG_LEVEL_DEBUG, const char* _log_file=DEFAULT_LOG_FILE, const char* _repo_path=DEFAULT_REPO_FILE);
        ~Axis2SslProvider();
        bool init(int _port, int _read_timeout, std::string& _error);
        bool processRequest(std::string& _error);

    private:
        SSL_CTX* m_ctx;
        SSL* m_ssl;
        void* createServerConnection(axutil_env_t *thread_env, int socket);
        SOCKET processAccept();
};

}}

#endif    // _AXIS2_SSL_PROVIDER_H
