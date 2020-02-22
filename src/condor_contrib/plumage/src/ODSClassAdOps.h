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

#ifndef _ODS_CLASSAD_OPS_H
#define _ODS_CLASSAD_OPS_H

// condor includes
#include <compat_classad.h>
#include <classad/value.h>

// attribute and classad operations - not all derivations will need every CRUD op

namespace plumage {
namespace etl {
    
template <typename T>
class ODSKeyBuilder {
    static void addToKey(T&, const char* name, const char* value, const  classad::Value& type);
};

template <typename T>
class ODSAttributeOps {
public:
    virtual bool createAttr(const T&) { return true; }    
    virtual bool readAttr(const T&, std::string&, std::string&, classad::Value&) { return true; }    
    virtual bool updateAttr(const T&, const char*, const char* , const classad::Value*) { return true; }
    virtual bool deleteAttr(const T&, const char* ) { return true; }
};

template <typename T>
class ODSClassAdOps {
public:
    virtual bool createAd(const T&) { return true; }
    virtual bool readAd(const T&, ClassAd&) { return true; }
    virtual bool updateAd(const T&, ClassAd*) { return true; }
    virtual bool deleteAd(const T&) { return true; }
};

}}

#endif    // _ODS_CLASSAD_OPS_H
