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
#ifndef _SIMPLEATTRIBUTE_H
#define _SIMPLEATTRIBUTE_H

#include <string>
#include <map>
#include <list>

using namespace std;

namespace aviary {
namespace util {

class Attribute
{
    public:
        enum AttributeType
        {
            EXPR_TYPE = 0,
            INTEGER_TYPE = 1,
            FLOAT_TYPE = 2,
            STRING_TYPE = 3
        };
        Attribute ( AttributeType , const char* );
        ~Attribute();

        AttributeType GetType() const;
        const char * GetValue() const;

    private:
        AttributeType m_type;
        const char * m_value;
};

typedef map<string, Attribute*> SimpleAttributeMapType;
typedef list<Attribute*> SimpleAttributeListType;

}} /* aviary::util */

#endif /* _SIMPLEATTRIBUTE_H */