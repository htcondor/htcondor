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
#include "string_funcs.h"

int
main( argc, argv )
int		argc;
char	*argv[];
{
	const char *strings[] =
		{
			"abc",
			"ABC",
			"Abc",

			"abc_",
			"ABC_",
			"Abc_",

			"_abc",
			"_ABC",
			"_Abc",

			"abc_def",
			"ABC_DEF",
			"Abc_Def",

			"abc__def",
			"ABC__DEF",
			"Abc__Def",

			"abc_def_ghi",
			"ABC_DEF_GHI",
			"Abc_Def_GHI",

			"abc_123",
			"ABC_123",
			"Abc_123",

			"123_abc_123",
			"123_ABC_123",
			"123_Abc_123",


			NULL
		};

	const char	*s;
	int		i;
	for ( i = 0, s = strings[0];  (strings[i] != NULL);  s = strings[i++] ) {
		char	*ucfirst  = getUcFirst( s );
		char	*ucfirst_ = getUcFirstUnderscore( s );
		printf( "%s => '%s' '%s'\n", s, ucfirst, ucfirst_ );
		free( ucfirst );
		free( ucfirst_ );
	}
	return 0;
}

/*
### Local Variables: ***
### mode:c++ ***
### style:cc-mode ***
### c-abc-offset: 4 ***
### tab-width:4 ***
### End: ***
*/
