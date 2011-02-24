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

#ifndef AXIS2SOAPPROVIDER_H
#define AXIS2SOAPPROVIDER_H

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

// C++ wrapper around a SINGLE-THREADED
// Axis2/C engine; suitable for integration
// with DaemonCore socket registration
// ./configure --enable-multi-thread=no
class Axis2SoapProvider {
    public:
        Axis2SoapProvider();
        ~Axis2SoapProvider();
        bool init(int _log_level, const char* _log_file, const char* _repo_path, int _port, std::string& _error);
        SOCKET getHttpListenerSocket();
        bool processHttpRequest(std::string& _error);

    private:
        axutil_env_t* m_env;
        axis2_transport_receiver_t* m_http_server;
        axis2_http_svr_thread_t* m_svr_thread;
        bool m_initialized;

        axis2_http_svr_thread_t* createHttpReceiver(axutil_env_t* _env, axis2_transport_receiver_t* _server, std::string& _error);
};

#endif    // AXIS2SOAPPROVIDER_H