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

namespace plumage {
namespace etl {
    
class ODSMongodbWriter: public ODSClassAdWriter {
public:
    ODSMongodbWriter(const std::string& db_name);
    ~ODSMongodbWriter();
    bool writeAttribute(const std::string& key, const std::string& name, const classad::Value& type, const std::string& value);
    bool writeClassAd(const std::string& key, compat_classad::ClassAd* ad);
    bool init(const std::string& loc);
    
protected:
    mongo::DBClientConnection m_db_conn;
    std::string m_db_name;

};

class ODSMongodbReader: public ODSClassAdReader {
public:
    ODSMongodbReader(const std::string& db_name);
    ~ODSMongodbReader();
    bool readAttribute(const std::string& key, std::string& name, classad::Value& type, std::string& value);
    bool readClassAd(const std::string& key, compat_classad::ClassAd& ad);
    bool init(const std::string& loc);
    
protected:
    mongo::DBClientConnection m_db_conn;
    std::string m_db_name;

};

}}

#endif    // _ODS_MONGODB_RW_H
