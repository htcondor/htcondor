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

#include "SimpleAttribute.h"

using namespace aviary::util;

Attribute::Attribute ( AttributeType _type, const char *_value ) :
        m_type ( _type ),
        m_value ( _value )
{ }

Attribute::~Attribute()
{
}

Attribute::AttributeType
Attribute::GetType() const
{
    return m_type;
}

const char *
Attribute::GetValue() const
{
    return m_value;
}
