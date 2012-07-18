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

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"

#include <axutil_error_default.h>
#include <axutil_log_default.h>
#include <axutil_thread_pool.h>
#include <axiom_xml_reader.h>
#include <axutil_file_handler.h>

// axis2/c non-public ssl api
#include "axis2_ssl_utils.h"
#include "axis2_ssl_stream.h"

#include "Axis2SslProvider.h"
#include "axis2_ssl_utils.h"

typedef struct ssl_stream_impl ssl_stream_impl_t;

struct ssl_stream_impl
{
    axutil_stream_t stream;
    axutil_stream_type_t stream_type;
    SSL *ssl;
    SSL_CTX *ctx;
    axis2_socket_t socket;
};

using namespace std;
using namespace aviary::soap;

Axis2SslProvider::Axis2SslProvider(int _log_level, const char* _log_file, const char* _repo_path):
    Axis2SoapProvider(_log_level,_log_file,_repo_path)
{
}

Axis2SslProvider::~Axis2SslProvider()
{
    axis2_ssl_utils_cleanup_ssl(m_ctx,m_ssl);
}

bool 
Axis2SslProvider::init(int _port, int _read_timeout, std::string& _error) {

    char* tmp = NULL;
    axis2_char_t *server_cert, *server_key, *ca_file, *ca_dir;
	server_cert = server_key = ca_file = ca_dir = NULL;

    // collect our certs, ca, etc.
    if ((tmp = param("AVIARY_SSL_SERVER_CERT"))) {
        server_cert = strdup(tmp);
        free(tmp);
    }
    if ((tmp = param("AVIARY_SSL_SERVER_KEY"))) {
        server_key = strdup(tmp);
        free(tmp);
    }
    if ((tmp = param("AVIARY_SSL_CA_FILE"))) {
        ca_file = strdup(tmp);
        free(tmp);
    }
    if ((tmp = param("AVIARY_SSL_CA_DIR"))) {
        ca_dir = strdup(tmp);
        free(tmp);
    }
    
    // init the ssl lib, errors, etc.
    m_ctx = axis2_ssl_utils_initialize_ctx(m_env,server_cert, server_key,
                                               ca_file, ca_dir,
                                               NULL);
    if (!m_ctx) {
        dprintf(D_ALWAYS, "axis2_ssl_utils_initialize_ctx failed\n");
        return false;
    }

    // init our parent AFTER checking that SSL is configured OK
    if (!Axis2SoapProvider::init(_port,_read_timeout,_error)) {
        dprintf(D_ALWAYS, "%s\n",_error.c_str());
        return false;
    }

    return true;
}

void* Axis2SslProvider::createServerConnection(axutil_env_t *thread_env, int socket) {
    axis2_simple_http_svr_conn_t *svr_conn = NULL;
    svr_conn = axis2_simple_http_svr_conn_create(thread_env, (int)socket);
    
    // we'll manipulate the connection to give it SSLness
    axutil_stream_free(svr_conn->stream,m_env);
    
    ssl_stream_impl_t *stream_impl = NULL;

    stream_impl = (ssl_stream_impl_t *) AXIS2_MALLOC(m_env->allocator,sizeof(ssl_stream_impl_t));

    if (!stream_impl)
    {
        AXIS2_HANDLE_ERROR(m_env, AXIS2_ERROR_NO_MEMORY, AXIS2_FAILURE);
        return NULL;
    }
    memset ((void *)stream_impl, 0, sizeof (ssl_stream_impl_t));
    stream_impl->socket = socket;
    stream_impl->stream.socket = socket;
    stream_impl->ctx = NULL;
    stream_impl->ssl = NULL;

    stream_impl->ctx = m_ctx;
    
    stream_impl->ssl = m_ssl;
    stream_impl->stream_type = AXIS2_STREAM_MANAGED;
    stream_impl->stream.stream_type = AXIS2_STREAM_MANAGED;

    axutil_stream_set_read(&(stream_impl->stream), m_env, axis2_ssl_stream_read);
    axutil_stream_set_write(&(stream_impl->stream), m_env, axis2_ssl_stream_write);
    axutil_stream_set_skip(&(stream_impl->stream), m_env, axis2_ssl_stream_skip);
    axutil_stream_set_peek(&(stream_impl->stream), m_env, axis2_ssl_stream_peek);
    
    svr_conn->stream = &(stream_impl->stream);

    return svr_conn;
}

SOCKET 
Axis2SslProvider::processAccept() {
    SOCKET conn_socket = Axis2SoapProvider::processAccept();

    if (!(m_ssl = axis2_ssl_utils_initialize_ssl(m_env,m_ctx,conn_socket))) {
        dprintf(D_ALWAYS, "axis2_ssl_utils_initialize_ssl failed\n");
        return INVALID_SOCKET;
    }
    return conn_socket;
}

bool
Axis2SslProvider::processRequest(std::string& _error)
{
    bool status = Axis2SoapProvider::processRequest(_error);
    if (m_ssl) {
        SSL_shutdown(m_ssl);
        SSL_free(m_ssl);
        m_ssl = NULL;
    }
    return status;
}
