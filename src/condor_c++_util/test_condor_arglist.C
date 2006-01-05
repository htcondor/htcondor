/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_arglist.h"

void condor_arglist_unit_test() {
	SimpleList<MyString> args;
	SimpleListIterator<MyString> it(args);
	MyString *arg;
	char const *test_string = "This '''quoted''' arg' 'string 'contains ''many'' \"\"surprises\\'";
	char const *test_cooked_string = "\"This '''quoted''' arg' 'string 'contains ''many'' \\\"\\\"surprises\\'\"";
	char const *test_v1_cooked = "one \\\"two\\\" 'three\\ four'";
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
	ASSERT(arglist.AppendArgsV1or2Input(test_v1_cooked,NULL));
	ASSERT(arglist.Count() == 4);
	ASSERT(!strcmp(arglist.GetArg(0),"one"));
	ASSERT(!strcmp(arglist.GetArg(1),"\"two\""));
	ASSERT(!strcmp(arglist.GetArg(2),"'three\\"));
	ASSERT(!strcmp(arglist.GetArg(3),"four'"));

	arglist.Clear();
	ASSERT(arglist.AppendArgsV2Input(test_cooked_string,NULL));
	ASSERT(arglist.Count() == 4);
	ASSERT(!strcmp(arglist.GetArg(0),"This"));
	ASSERT(!strcmp(arglist.GetArg(1),"'quoted'"));
	ASSERT(!strcmp(arglist.GetArg(2),"arg string"));
	ASSERT(!strcmp(arglist.GetArg(3),"contains 'many' \"\"surprises\\"));
	MyString v2_cooked_args;
	ASSERT(arglist.GetArgsStringV2Input(&v2_cooked_args,&error_msg));

	arglist.Clear();
	ASSERT(arglist.AppendArgsV1or2Input(v2_cooked_args.Value(),NULL));
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
}

int main() {
	condor_arglist_unit_test();
	return 0;
}
