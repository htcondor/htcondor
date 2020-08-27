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

#ifndef _ODS_MONGODB_OPS_H
#define _ODS_MONGODB_OPS_H

#include "condor_common.h"

// global includes
#include "mongo/client/dbclient.h"

// condor includes
#include <compat_classad.h>

// local includes
#include "ODSClassAdOps.h"

namespace plumage {
namespace etl {
    
    class ODSMongodbKeyBuilder: public ODSKeyBuilder<mongo::BSONObjBuilder> {
        static void addToKey(mongo::BSONObjBuilder& key, const char* name, const char* value, const classad::Value* type);
    };
    
    class ODSMongodbOps: public ODSAttributeOps<mongo::BSONObjBuilder>, public ODSClassAdOps<mongo::BSONObjBuilder> {
    public:
        ODSMongodbOps(const std::string& db_name);
        ~ODSMongodbOps();
        bool init(const std::string& loc);
        
        // ODSAttributeOps
        bool updateAttr(mongo::BSONObjBuilder& key, const char* name, const char* value, const classad::Value* type=NULL);
        bool deleteAttr(mongo::BSONObjBuilder& key, const char* name);
        
        // ODSClassAdOps
        bool createAd(mongo::BSONObjBuilder& key);
        bool updateAd(mongo::BSONObjBuilder& key, ClassAd* ad);
        bool deleteAd(mongo::BSONObjBuilder& key);
        
        // raw record write
        bool createRecord(mongo::BSONObjBuilder& bob);
        bool readRecord(mongo::BSONObjBuilder& bob);

        // specify what fields to index
        bool addIndex(mongo::BSONObj bo);

        // TODO: expose this for now as we work through encapsulation design
        mongo::DBClientConnection* m_db_conn;

    protected:
        std::string m_db_name;

};

}}

#endif    // _ODS_MONGODB_OPS_H
