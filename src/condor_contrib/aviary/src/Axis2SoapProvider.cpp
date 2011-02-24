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

#include <axutil_error_default.h>
#include <axutil_log_default.h>
#include <axutil_thread_pool.h>
#include <axiom_xml_reader.h>
#include <axutil_file_handler.h>
#include "Axis2SoapProvider.h"

// NOTE: these types are not in the public
// Axis2/C API via headers but we need them;
// review if there is a newer rev after 1.6

// lifted out from http_receiver.c
typedef struct
{
     axis2_transport_receiver_t http_server;
     axis2_http_svr_thread_t *svr_thread;
     int port;
     axis2_conf_ctx_t *conf_ctx;
     axis2_conf_ctx_t *conf_ctx_private;
} axis2_http_server_impl_t;
 
#define AXIS2_INTF_TO_IMPL(http_server) \
 ((axis2_http_server_impl_t *)(http_server))
 
// lifted out from http_svr_thread.c
struct axis2_http_svr_thread
{
    int listen_socket;
    axis2_bool_t stopped;
    axis2_http_worker_t *worker;
    int port;
};

typedef struct axis2_http_svr_thd_args
{
    axutil_env_t *env;
    axis2_socket_t socket;
    axis2_http_worker_t *worker;
    axutil_thread_t *thread;
} axis2_http_svr_thd_args_t;

static void *AXIS2_THREAD_FUNC
axis2_svr_thread_worker_func(
    axutil_thread_t * thd,
    void *data);

Axis2SoapProvider::Axis2SoapProvider()
{
    m_env = NULL;
    m_http_server = NULL;
    m_svr_thread = NULL;
    m_initialized = false;
}

Axis2SoapProvider::~Axis2SoapProvider()
{
    if (m_http_server) {
        axis2_transport_receiver_free(m_http_server, m_env);
    }

    if (m_env) {
        axutil_env_free(m_env);
    }

    axiom_xml_reader_cleanup();
}

bool
Axis2SoapProvider::init(int _log_level, const char* _log_file, const char* _repo_path, int _port, std::string& _error)
{

    if (!m_initialized) {
        axutil_allocator_t* allocator = axutil_allocator_init(NULL);
        axutil_error_t *error = axutil_error_create(allocator);
        axutil_log_t *log = axutil_log_create(allocator, NULL, _log_file);

        // TODO: not sure we need a TP but don't wanted to get tripped up by a NP
        // deeper in the stack
        axutil_thread_pool_t *thread_pool = axutil_thread_pool_init(allocator);
        axiom_xml_reader_init();
        m_env = axutil_env_create(allocator);
        axutil_error_init();

        m_env = axutil_env_create_with_error_log_thread_pool(allocator, error, log, thread_pool);
        m_env->log->level = axutil_log_levels_t(_log_level);

        axis2_status_t status = axutil_file_handler_access(_repo_path, AXIS2_R_OK);

        if (status != AXIS2_SUCCESS) {
            AXIS2_LOG_ERROR(m_env->log, AXIS2_LOG_SI, "provided repo path %s does "
                                                      "not exist or no permissions to read, set "
                                                      "repo_path to DEFAULT_REPO_PATH", _repo_path);
            return m_initialized;
        }

        m_http_server = axis2_http_server_create_with_file(m_env, _repo_path, _port);
        if (!m_http_server) {
            AXIS2_LOG_ERROR(m_env->log, AXIS2_LOG_SI, "Server creation failed: Error code:" " %d :: %s",
                            m_env->error->error_number, AXIS2_ERROR_GET_MESSAGE(m_env->error));
            return m_initialized;
        }

        m_svr_thread = createHttpReceiver(m_env,m_http_server,_error); 
        if (!m_svr_thread) {
            return m_initialized;
        }

        m_initialized = true;
    }

    return m_initialized;

}

axis2_http_svr_thread_t*
Axis2SoapProvider::createHttpReceiver(axutil_env_t* _env, axis2_transport_receiver_t* _server, std::string& _error)
{

    axis2_http_server_impl_t *server_impl = NULL;
    axis2_http_worker_t *worker = NULL;

    server_impl = AXIS2_INTF_TO_IMPL(_server);
    server_impl->svr_thread = axis2_http_svr_thread_create(_env, server_impl->port);

    // shouldn't bother checking this for ST but we'll play along
    if(!server_impl->svr_thread) {
        AXIS2_LOG_ERROR(_env->log, AXIS2_LOG_SI, "unable to create server thread for port %d",
                server_impl->port);
        return NULL;
    }

    worker = axis2_http_worker_create(_env, server_impl->conf_ctx);
    if(!worker) {
        AXIS2_LOG_ERROR(_env->log, AXIS2_LOG_SI, "axis2 http worker creation failed");
        axis2_http_svr_thread_free(server_impl->svr_thread, _env);
        server_impl->svr_thread = NULL;
        return NULL;
    }

    axis2_http_worker_set_svr_port(worker, _env, server_impl->port);
    axis2_http_svr_thread_set_worker(server_impl->svr_thread, _env, worker);
    return server_impl->svr_thread;

}

SOCKET
Axis2SoapProvider::getHttpListenerSocket()
{
    return m_svr_thread->listen_socket;
}

bool
Axis2SoapProvider::processHttpRequest(std::string& _error)
{
    if (m_initialized) {

        AXIS2_ENV_CHECK(m_env, AXIS2_FAILURE);

        int socket = INVALID_SOCKET;
        axis2_http_svr_thd_args_t *arg_list = NULL;

        socket = (int)axutil_network_handler_svr_socket_accept(m_env, m_svr_thread->listen_socket);
        if(!m_svr_thread->worker)
        {
            AXIS2_LOG_ERROR(m_env->log, AXIS2_LOG_SI,
                "Worker not ready yet. Cannot serve the request");
            axutil_network_handler_close_socket(m_env, socket);
            return false;
        }

        arg_list = (axis2_http_svr_thd_args_t *)AXIS2_MALLOC(m_env->allocator, sizeof(axis2_http_svr_thd_args_t));
        if(!arg_list)
        {
            AXIS2_LOG_ERROR(m_env->log, AXIS2_LOG_SI,
                "Memory allocation error in the svr thread loop");
            return false;
        }

        arg_list->env = (axutil_env_t *)m_env;
        arg_list->socket = socket;
        arg_list->worker = m_svr_thread->worker;

        // single-threaded for DC
        axis2_svr_thread_worker_func(NULL, (void *)arg_list);
    }
    else {
        _error = "Axis2SoapPovider has not been initialized yet";
        return false;
    }

    return true;
}

// TODO: need a public axis2_tcp_worker.h for this
//Axis2SoapProvider::processTcpRequest() {
//    axis2_tcp_worker_process_request();
//}
