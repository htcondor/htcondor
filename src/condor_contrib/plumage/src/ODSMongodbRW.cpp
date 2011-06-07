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

// condor includes
#include "condor_common.h"
#include "condor_qmgr.h"

// local includes
#include "ODSMongodbRW.h"

using namespace compat_classad;
using namespace plumage::etl;

ODSMongodbReader::ODSMongodbReader()
{
    //
}

ODSMongodbReader::~ODSMongodbReader()
{
    //
}


bool
ODSMongodbReader::readAttribute (const std::string& attr, classad::Value& value)
{
    return true;
}

bool
ODSMongodbReader::readClassAd(const std::string& str, ClassAd& ad)
{
    return true;
}

