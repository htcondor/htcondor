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

#ifndef _AXIS2_SOAP_PROVIDER_H
#define _AXIS2_SOAP_PROVIDER_H

#include "AviaryProvider.h"

#include <string>
#include <axis2_http_server.h>
#include <axis2_http_transport.h>
#include <platforms/axutil_platform_auto_sense.h>
#include <axis2_http_worker.h>
#include <axutil_network_handler.h>
#include <axis2_http_svr_thread.h>

#include <openssl/ssl.h>

#define DEFAULT_LOG_FILE "./axis2.log"
#define DEFAULT_REPO_FILE "../axis2.xml"
#define DEFAULT_PORT 39000

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

namespace aviary {
namespace soap {

// C++ wrapper around a SINGLE-THREADED
// Axis2/C engine; suitable for integration
// with DaemonCore socket registration
// ./configure --enable-multi-thread=no

class Axis2SoapProvider: public aviary::transport::AviaryProvider {
    public:
        ~Axis2SoapProvider();
        bool init(int _port, int _read_timeout, std::string& _error);
        SOCKET getListenerSocket();
        bool processRequest(std::string& _error);
        void invalidate();

        const axutil_env_t* getEnv() {return m_env;}

    protected:
        friend class aviary::transport::AviaryProviderFactory;
        Axis2SoapProvider(int _log_level=AXIS2_LOG_LEVEL_DEBUG, const char* _log_file=DEFAULT_LOG_FILE, const char* _repo_path=DEFAULT_REPO_FILE);

        std::string m_log_file;
        std::string m_repo_path;
        axutil_log_levels_t m_log_level;
        axutil_env_t* m_env;
        axutil_allocator_t* m_allocator;
        axis2_transport_receiver_t* m_http_server;
        axis2_http_svr_thread_t* m_svr_thread;
        int m_http_socket_read_timeout;
        bool m_init;

        axis2_http_svr_thread_t* createSocket(axutil_env_t* _env, int port);
        axis2_http_svr_thread_t* createReceiver(axutil_env_t* _env, axis2_transport_receiver_t* _server, std::string& _error);
        virtual void* createServerConnection(axutil_env_t *thread_env, SOCKET socket);
        virtual SOCKET processAccept();
        void *AXIS2_THREAD_FUNC invokeWorker( axutil_thread_t * thd, void *data );
};

}}

#endif    // _AXIS2_SOAP_PROVIDER_H
