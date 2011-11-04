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

using namespace aviary::soap;

Axis2SoapProvider::Axis2SoapProvider(int _log_level, const char* _log_file, const char* _repo_path)
{
    if (_log_file) {
        m_log_file = _log_file;
    }
    if (_repo_path) {
        m_repo_path = _repo_path;
    }
    m_log_level = axutil_log_levels_t(_log_level);
    m_env = NULL;
    m_http_server = NULL;
    m_svr_thread = NULL;
    m_init = false;
    m_http_socket_read_timeout = AXIS2_HTTP_DEFAULT_SO_TIMEOUT;

    m_allocator = axutil_allocator_init(NULL);
    m_env = axutil_env_create(m_allocator);

}

Axis2SoapProvider::~Axis2SoapProvider()
{
    if (m_http_server) {
        axis2_transport_receiver_free(m_http_server, m_env);
    }
    
    if (m_svr_thread) {
        axis2_http_svr_thread_free(m_svr_thread, m_env);
    }

    if (m_env) {
        axutil_env_free(m_env);
    }

    // don't free m_allocator

    axiom_xml_reader_cleanup();

}

bool
Axis2SoapProvider::init(int _port, int _read_timeout, std::string& _error)
{
    m_http_socket_read_timeout = _read_timeout;
    
    if (m_log_file.empty() || m_repo_path.empty()) {
        _error = "Log file or repo path is NULL";
        return false;
    }

    if (!m_init) {

        axutil_log_t *log = axutil_log_create(m_allocator, NULL, m_log_file.c_str());

        // TODO: not sure we need a TP but don't wanted to get tripped up by a NP
        // deeper in the stack
        axutil_thread_pool_t *thread_pool = axutil_thread_pool_init(m_allocator);
        axiom_xml_reader_init();

        axutil_error_t *error = axutil_error_create(m_allocator);
        axutil_error_init();
        m_env = axutil_env_create_with_error_log_thread_pool(m_allocator, error, log, thread_pool);
        m_env->log->level = m_log_level;

        axis2_status_t status = axutil_file_handler_access(m_repo_path.c_str(), AXIS2_R_OK);

        if (status != AXIS2_SUCCESS) {
			_error = m_repo_path;
			_error += " does not exist or insufficient permissions";
            AXIS2_LOG_ERROR(m_env->log, AXIS2_LOG_SI,_error.c_str());
            return m_init;
        }

        m_http_server = axis2_http_server_create_with_file(m_env, m_repo_path.c_str(), _port);
        if (!m_http_server) {
			_error =  AXIS2_ERROR_GET_MESSAGE(m_env->error);
            AXIS2_LOG_ERROR(m_env->log, AXIS2_LOG_SI, "HTTP server create failed: %d: %s",
                            m_env->error->error_number,_error.c_str());
            return m_init;
        }

        m_svr_thread = createReceiver(m_env,m_http_server,_error); 
        if (!m_svr_thread) {
			_error =  AXIS2_ERROR_GET_MESSAGE(m_env->error);
			AXIS2_LOG_ERROR(m_env->log, AXIS2_LOG_SI, "HTTP receiver create failed: %d: %s",
                            m_env->error->error_number,_error.c_str());
            return m_init;
        }

        m_init = true;
    }

    return m_init;

}

axis2_http_svr_thread_t*
Axis2SoapProvider::createReceiver(axutil_env_t* _env, axis2_transport_receiver_t* _server, std::string& /*_error */)
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
Axis2SoapProvider::getListenerSocket()
{
    SOCKET socket = INVALID_SOCKET;
    if (m_svr_thread) {
        socket = m_svr_thread->listen_socket;
    }
    return socket;
}

bool
Axis2SoapProvider::processRequest(std::string& _error)
{
    if (!m_init) {
         _error = "Axis2SoapPovider has not been initialized yet";
        return false;
    }
    else {

        AXIS2_ENV_CHECK(m_env, AXIS2_FAILURE);

        int socket = INVALID_SOCKET;
        axis2_http_svr_thd_args_t *arg_list = NULL;

        if (INVALID_SOCKET == (socket = this->processAccept())) {
            _error = "Failed to accept connection";
            return false;
        }

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
        invokeWorker(NULL, (void *)arg_list);
    }

    return true;
}

SOCKET Axis2SoapProvider::processAccept() {
    return (SOCKET)axutil_network_handler_svr_socket_accept(m_env, m_svr_thread->listen_socket);
}

void* Axis2SoapProvider::createServerConnection(axutil_env_t *thread_env, SOCKET socket) {
    return axis2_simple_http_svr_conn_create(thread_env, (SOCKET)socket);
}

void *AXIS2_THREAD_FUNC
Axis2SoapProvider::invokeWorker(
    axutil_thread_t * /*thd */,
    void *data)
{
    struct AXIS2_PLATFORM_TIMEB t1, t2;
    axis2_simple_http_svr_conn_t *svr_conn = NULL;
    axis2_http_simple_request_t *request = NULL;
    int millisecs = 0;
    double secs = 0;
    axis2_http_worker_t *tmp = NULL;
    axis2_status_t status = AXIS2_FAILURE;
    axutil_env_t *env = NULL;
    axis2_socket_t socket;
    axutil_env_t *thread_env = NULL;
    axis2_http_svr_thd_args_t *arg_list = NULL;

#ifndef WIN32
#ifdef AXIS2_SVR_MULTI_THREADED
    signal(SIGPIPE, SIG_IGN);
#endif
#endif

    if(!data)
    {
        return NULL;
    }
    arg_list = (axis2_http_svr_thd_args_t *)data;

    env = arg_list->env;
    thread_env = axutil_init_thread_env(env);

    IF_AXIS2_LOG_DEBUG_ENABLED(env->log)
    {
        AXIS2_PLATFORM_GET_TIME_IN_MILLIS(&t1);
    }

    socket = arg_list->socket;
    svr_conn = (axis2_simple_http_svr_conn_t*)(this->createServerConnection(thread_env, (int)socket));
    
    if(!svr_conn)
    {
        AXIS2_LOG_ERROR(env->log, AXIS2_LOG_SI, "creating simple_http_svr_connection failed");
        return NULL;
    }

    axis2_simple_http_svr_conn_set_rcv_timeout(svr_conn, thread_env, m_http_socket_read_timeout);

    /* read HTTPMethod, URL, HTTP Version and http headers. Leave the remaining in the stream */
    request = axis2_simple_http_svr_conn_read_request(svr_conn, thread_env);
    if(!request)
    {
        AXIS2_LOG_ERROR(env->log, AXIS2_LOG_SI, "Could not create request");
        axis2_simple_http_svr_conn_free(svr_conn, thread_env);
        return NULL;
    }

    tmp = arg_list->worker;
    status = axis2_http_worker_process_request(tmp, thread_env, svr_conn, request);
    axis2_simple_http_svr_conn_free(svr_conn, thread_env);
    axis2_http_simple_request_free(request, thread_env);

    IF_AXIS2_LOG_DEBUG_ENABLED(env->log)
    {
        AXIS2_PLATFORM_GET_TIME_IN_MILLIS(&t2);
        millisecs = t2.millitm - t1.millitm;
        secs = difftime(t2.time, t1.time);
        if(millisecs < 0)
        {
            millisecs += 1000;
            secs--;
        }
        secs += millisecs / 1000.0;

        AXIS2_LOG_DEBUG(thread_env->log, AXIS2_LOG_SI, "Request processed in %.3f seconds", secs);
    }

    if(status == AXIS2_SUCCESS) {
        AXIS2_LOG_DEBUG(thread_env->log, AXIS2_LOG_SI, "Request served successfully");
    }
    else {
        AXIS2_LOG_WARNING(thread_env->log, AXIS2_LOG_SI, "Error occurred in processing request ");
    }

    // just ST here
    AXIS2_FREE(thread_env->allocator, arg_list);
    axutil_free_thread_env(thread_env);
    thread_env = NULL;

    return NULL;
}
