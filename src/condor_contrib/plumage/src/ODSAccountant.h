/***************************************************************
 *
 * Copyright (C) 2009-2012 Red Hat, Inc.
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

#ifndef _ODS_ACCOUNTANT_H
#define _ODS_ACCOUNTANT_H

// condor includes
#include "condor_common.h"
#include "compat_classad.h"
#include "daemon.h"

namespace plumage {
namespace etl {
    
class ODSAccountant {
public:
    ODSAccountant();
    ~ODSAccountant();

    bool connect();
    AttrList* fetchAd();

private:
    Daemon* m_negotiator;
};

}}

#endif    // _ODS_ACCOUNTANT_H
