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
#include "condor_arglist.h"

void condor_arglist_unit_test() {
	SimpleList<MyString> args;
	SimpleListIterator<MyString> it(args);
	MyString *arg;
	char const *test_string = "This '''quoted''' arg' 'string 'contains ''many'' \"\"surprises\\'";
	char const *test_cooked_string = "\"This '''quoted''' arg' 'string 'contains ''many'' \"\"\"\"surprises\\'\"";
	char const *test_v1_wacked = "one \\\"two\\\" 'three\\ four'";
	char const *test_win32_v1 = "one \"two three\" four\\ \"five \\\"six\\\\\\\"\" \"seven\\\\\\\"\"";
	MyString joined_args,joined_args2;
	MyString error_msg;
	char **string_array;

	ASSERT(split_args(test_string,&args,NULL));
	ASSERT(args.Number() == 4);
	it.ToBeforeFirst();
	it.Next(arg);
	ASSERT(*arg == "This");
	it.Next(arg);
	ASSERT(*arg == "'quoted'");
	it.Next(arg);
	ASSERT(*arg == "arg string");
	it.Next(arg);
	ASSERT(*arg == "contains 'many' \"\"surprises\\");

	join_args(args,&joined_args);

	args.Clear();
	ASSERT(split_args(joined_args.Value(),&args,NULL));
	ASSERT(args.Number() == 4);
	it.ToBeforeFirst();
	it.Next(arg);
	ASSERT(*arg == "This");
	it.Next(arg);
	ASSERT(*arg == "'quoted'");
	it.Next(arg);
	ASSERT(*arg == "arg string");
	it.Next(arg);
	ASSERT(*arg == "contains 'many' \"\"surprises\\");

	args.Clear();
	ASSERT(!split_args("Unterminated 'quote",&args,NULL));
	ASSERT(!split_args("Unterminated 'quote",&args,&error_msg));

	ASSERT(split_args(joined_args.Value(),&string_array,NULL));
	ASSERT(!strcmp(string_array[0],"This"));
	ASSERT(!strcmp(string_array[1],"'quoted'"));
	ASSERT(!strcmp(string_array[2],"arg string"));
	ASSERT(!strcmp(string_array[3],"contains 'many' \"\"surprises\\"));
	ASSERT(string_array[4] == NULL);

	join_args(string_array,&joined_args2);
	ASSERT(joined_args == joined_args2);

	deleteStringArray(string_array);

	ArgList arglist;

	arglist.Clear();
	ASSERT(arglist.AppendArgsV1WackedOrV2Quoted(test_v1_wacked,NULL));
	ASSERT(arglist.Count() == 4);
	ASSERT(!strcmp(arglist.GetArg(0),"one"));
	ASSERT(!strcmp(arglist.GetArg(1),"\"two\""));
	ASSERT(!strcmp(arglist.GetArg(2),"'three\\"));
	ASSERT(!strcmp(arglist.GetArg(3),"four'"));

	arglist.Clear();
	ASSERT(arglist.AppendArgsV2Quoted(test_cooked_string,NULL));
	ASSERT(arglist.Count() == 4);
	ASSERT(!strcmp(arglist.GetArg(0),"This"));
	ASSERT(!strcmp(arglist.GetArg(1),"'quoted'"));
	ASSERT(!strcmp(arglist.GetArg(2),"arg string"));
	ASSERT(!strcmp(arglist.GetArg(3),"contains 'many' \"\"surprises\\"));
	MyString v2_cooked_args;
	ASSERT(arglist.GetArgsStringV2Quoted(&v2_cooked_args,&error_msg));

	arglist.Clear();
	ASSERT(arglist.AppendArgsV1WackedOrV2Quoted(v2_cooked_args.Value(),NULL));
	ASSERT(arglist.Count() == 4);
	ASSERT(!strcmp(arglist.GetArg(0),"This"));
	ASSERT(!strcmp(arglist.GetArg(1),"'quoted'"));
	ASSERT(!strcmp(arglist.GetArg(2),"arg string"));
	ASSERT(!strcmp(arglist.GetArg(3),"contains 'many' \"\"surprises\\"));

	arglist.Clear();
	ASSERT(arglist.AppendArgsV2Raw(test_string,NULL));
	arglist.AppendArg("appended");
	arglist.InsertArg("inserted",0);
	ASSERT(arglist.Count() == 6);
	ASSERT(!strcmp(arglist.GetArg(0),"inserted"));
	ASSERT(!strcmp(arglist.GetArg(1),"This"));
	ASSERT(!strcmp(arglist.GetArg(2),"'quoted'"));
	ASSERT(!strcmp(arglist.GetArg(3),"arg string"));
	ASSERT(!strcmp(arglist.GetArg(4),"contains 'many' \"\"surprises\\"));
	ASSERT(!strcmp(arglist.GetArg(5),"appended"));

	MyString v2_args;
	ASSERT(arglist.GetArgsStringV2Raw(&v2_args,&error_msg));

	MyString v1or2_args;
	ASSERT(arglist.GetArgsStringV1or2Raw(&v1or2_args,&error_msg));

	arglist.Clear();
	ASSERT(arglist.AppendArgsV1or2Raw(v1or2_args.Value(),NULL));
	ASSERT(arglist.Count() == 6);
	ASSERT(!strcmp(arglist.GetArg(0),"inserted"));
	ASSERT(!strcmp(arglist.GetArg(1),"This"));
	ASSERT(!strcmp(arglist.GetArg(2),"'quoted'"));
	ASSERT(!strcmp(arglist.GetArg(3),"arg string"));
	ASSERT(!strcmp(arglist.GetArg(4),"contains 'many' \"\"surprises\\"));
	ASSERT(!strcmp(arglist.GetArg(5),"appended"));

	arglist.RemoveArg(2);
	ASSERT(arglist.Count() == 5);
	ASSERT(!strcmp(arglist.GetArg(0),"inserted"));
	ASSERT(!strcmp(arglist.GetArg(1),"This"));
	ASSERT(!strcmp(arglist.GetArg(2),"arg string"));
	ASSERT(!strcmp(arglist.GetArg(3),"contains 'many' \"\"surprises\\"));
	ASSERT(!strcmp(arglist.GetArg(4),"appended"));

	arglist.RemoveArg(0);
	ASSERT(arglist.Count() == 4);
	ASSERT(!strcmp(arglist.GetArg(0),"This"));
	ASSERT(!strcmp(arglist.GetArg(1),"arg string"));
	ASSERT(!strcmp(arglist.GetArg(2),"contains 'many' \"\"surprises\\"));
	ASSERT(!strcmp(arglist.GetArg(3),"appended"));

	arglist.Clear();
	arglist.SetArgV1Syntax(ArgList::WIN32_ARGV1_SYNTAX);
	ASSERT(arglist.AppendArgsV1Raw(test_win32_v1,NULL));
	ASSERT(!arglist.InputWasV1());
	ASSERT(arglist.Count() == 5);
	ASSERT(!strcmp(arglist.GetArg(0),"one"));
	ASSERT(!strcmp(arglist.GetArg(1),"two three"));
	ASSERT(!strcmp(arglist.GetArg(2),"four\\"));
	ASSERT(!strcmp(arglist.GetArg(3),"five \"six\\\""));
	ASSERT(!strcmp(arglist.GetArg(4),"seven\\\""));

	MyString win32_result;
	ASSERT(arglist.GetArgsStringWin32(&win32_result,1,NULL));
	arglist.Clear();
	arglist.SetArgV1Syntax(ArgList::WIN32_ARGV1_SYNTAX);
	ASSERT(arglist.AppendArgsV1Raw(win32_result.Value(),NULL));
	ASSERT(!arglist.InputWasV1());
	ASSERT(arglist.Count() == 4);
	ASSERT(!strcmp(arglist.GetArg(0),"two three"));
	ASSERT(!strcmp(arglist.GetArg(1),"four\\"));
	ASSERT(!strcmp(arglist.GetArg(2),"five \"six\\\""));
	ASSERT(!strcmp(arglist.GetArg(3),"seven\\\""));

	arglist.Clear();
	arglist.SetArgV1Syntax(ArgList::UNIX_ARGV1_SYNTAX);
	ASSERT(arglist.AppendArgsV1Raw("one 'two three' four",NULL));
	ASSERT(!arglist.InputWasV1());
	ASSERT(arglist.Count() == 4);

	arglist.Clear();
	arglist.SetArgV1Syntax(ArgList::UNKNOWN_ARGV1_SYNTAX);
	ASSERT(arglist.AppendArgsV1Raw("one 'two three' four",NULL));
	ASSERT(arglist.InputWasV1());
	ASSERT(arglist.Count() == 4);
}

int main() {
	condor_arglist_unit_test();
	return 0;
}
