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

#ifndef _ODS_READER_WRITER_H
#define _ODS_READER_WRITER_H

// condor includes
#include <compat_classad.h>
#include <classad/value.h>

namespace plumage {
namespace etl {
    
class ODSClassAdWriter {
public:
    virtual bool writeAttribute(const std::string& key, const std::string& name, const classad::Value& type, const std::string& value) = 0;
    virtual bool writeClassAd(const std::string& key, compat_classad::ClassAd* ad) = 0;

};

class ODSClassAdReader {
public:
    virtual bool readAttribute(const std::string& key, std::string& name, classad::Value& type, std::string& value) = 0;
    virtual bool readClassAd(const std::string& key, compat_classad::ClassAd &ad) = 0;

};

}}

#endif    // _ODS_READER_WRITER_H
