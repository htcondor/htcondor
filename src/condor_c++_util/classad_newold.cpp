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

#include "condor_common.h"
#include "condor_debug.h"
#include "MyString.h"
#include "classad_newold.h"
#include "condor_new_classads.h"
#include "condor_classad.h"
#define WANT_CLASSAD_NAMESPACE
#undef open
#include "classad/classad_distribution.h"

bool new_to_old(classad::ClassAd & src, ClassAd & dst)
{
	// Convert back to new classad string
	classad::ClassAdUnParser unparser;
	std::string s_newadout;
	unparser.SetOldClassAd( true );
	unparser.Unparse(s_newadout, &src);

	MyString err_msg;
	if( !dst.initFromString(s_newadout.c_str(),&err_msg) ) {
		dprintf(D_ALWAYS,"Failed to convert the following new classad to old ClassAd form: (%s) %s\n",err_msg.Value(),s_newadout.c_str());
		return false;
	}

	// Ensure dirtiness is preserved
	dst.ClearAllDirtyFlags();
	for(classad::ClassAd::dirtyIterator it = src.dirtyBegin();
		it != src.dirtyEnd(); ++it) {
		dst.SetDirtyFlag(it->c_str(), true);
	}

	return true;
}


bool old_to_new(ClassAd & src, classad::ClassAd & dst)
{
	// Convert to new classad string
	NewClassAdUnparser oldtonew;
	oldtonew.SetOutputType(true);
	oldtonew.SetOutputTargetType(true);
	MyString s_newad;
	oldtonew.Unparse(&src, s_newad);

	// Convert to new classad
	classad::ClassAdParser parser;
	return parser.ParseClassAd(s_newad.Value(), dst, true);
}

bool old_classad_value_to_new_classad_value(char const *old_value, MyString &new_value_buffer, MyString *err_msg)
{
	NewClassAdUnparser oldtonew;
	return oldtonew.OldValueToNewValue(old_value,new_value_buffer,err_msg);
}
