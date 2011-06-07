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

#ifndef _ODS_MONGODB_RW_H
#define _ODS_MONGODB_RW_H

// global includes
#include "mongo/client/dbclient.h"

// condor includes
#include <compat_classad.h>

// local includes
#include "ODSReaderWriter.h"

using namespace std;
using namespace compat_classad;
using namespace mongo;

namespace plumage {
namespace etl {
    
class ODSMongodbDataSource: public ODSDataSource {
public:
    void init();

private:
    DBClientConnection* m_dbConn;

};

class ODSMongodbWriter: public ODSClassAdWriter {
public:
    bool writeAttribute(const char* name, const ExprTree* exprTree);
    bool writeClassAd(const ClassAd* ad);

};

class ODSMongodbReader: public ODSClassAdReader {
public:
    bool readAttribute(const char* name, const char* value);
    bool readClassAd(ClassAd& ad);
    
private:
    ODSDataSource m_source;

};

}}

#endif    // _ODS_MONGODB_RW_H
