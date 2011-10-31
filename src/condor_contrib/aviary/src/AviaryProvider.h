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

#ifndef _AVIARY_PROVIDER_H
#define _AVIARY_PROVIDER_H

#include <string>

// borrow what DC does
#if !defined(WIN32)
#  ifndef SOCKET
#    define SOCKET int
#  endif
#  ifndef INVALID_SOCKET
#    define INVALID_SOCKET -1
#  endif
#endif /* not WIN32 */

namespace aviary {
namespace transport {
    
    // abstract provider
    class AviaryProvider {
    public:
        virtual SOCKET getListenerSocket() = 0;
        virtual bool processRequest(std::string& _error) = 0;     
    };
    
    // factory pattern to figure out what provider to, er, provide
    // based on condor config only
    class AviaryProviderFactory {
    public:
        static AviaryProvider* create(const std::string& log_file);

    private:
        AviaryProviderFactory();
        ~AviaryProviderFactory();
        AviaryProviderFactory& operator=(AviaryProviderFactory);

    };

}}

#endif    // _AVIARY_PROVIDER_H
