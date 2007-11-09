/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
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


#include <map>
#if defined( CLASSAD_DISTRIBUTION )
#include "classad/classad_distribution.h"
#else
#include "condor_classad.h"
#endif

#ifdef WANT_CLASSAD_NAMESPACE
using namespace classad;
#endif

class Variable
{
public:
    // When you give a Variable an ExprTree, it owns it. You don't. 
    Variable(std::string &name, ExprTree *tree);
    Variable(std::string &name, Value &value);
    ~Variable();

    void GetStringRepresentation(std::string &representation);

private:
    std::string   _name;
    bool          _is_tree; // If false, then is value
    ExprTree      *_tree;
    Value         _value;
};

