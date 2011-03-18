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

#ifndef _AXIS2SOAPPROVIDER_H
#define _AXIS2SOAPPROVIDER_H

#include <string>
#include <axis2_http_server.h>
#include <axis2_http_transport.h>
#include <platforms/axutil_platform_auto_sense.h>
#include <axis2_http_worker.h>
#include <axutil_network_handler.h>
#include <axis2_http_svr_thread.h>
// TODO: future tcp support
//#include <axis2_tcp_worker.h>

// borrow what DC does
#if !defined(WIN32)
#  ifndef SOCKET
#    define SOCKET int
#  endif
#  ifndef INVALID_SOCKET
#    define INVALID_SOCKET -1
#  endif
#endif /* not WIN32 */

#define DEFAULT_LOG_FILE "./axis2.log"
#define DEFAULT_REPO_FILE "../axis2.xml"
#define DEFAULT_PORT 9090

// C++ wrapper around a SINGLE-THREADED
// Axis2/C engine; suitable for integration
// with DaemonCore socket registration
// ./configure --enable-multi-thread=no

namespace aviary {
namespace soap {

class Axis2SoapProvider {
    public:
        Axis2SoapProvider(int _log_level=AXIS2_LOG_LEVEL_DEBUG, const char* _log_file=DEFAULT_LOG_FILE, const char* _repo_path=DEFAULT_REPO_FILE);
        ~Axis2SoapProvider();
        bool init(int _port, int _read_timeout, std::string& _error);
        SOCKET getHttpListenerSocket();
        bool processHttpRequest(std::string& _error);

    private:
        std::string m_log_file;
        std::string m_repo_path;
        axutil_log_levels_t m_log_level;
        axutil_env_t* m_env;
        axis2_transport_receiver_t* m_http_server;
        axis2_http_svr_thread_t* m_svr_thread;
        int m_http_socket_read_timeout;
        bool m_initialized;

        axis2_http_svr_thread_t* createHttpReceiver(axutil_env_t* _env, axis2_transport_receiver_t* _server, std::string& _error);
        void *AXIS2_THREAD_FUNC invokeHttpWorker( axutil_thread_t * thd, void *data );
};

}}

#endif    // _AXIS2SOAPPROVIDER_H
