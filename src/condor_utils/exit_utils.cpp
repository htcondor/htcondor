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
#include "exit.h"

/*
** These routines are the reverse of the WEXITSTATUS() and friend
** macros. I can't find a POSIX manual that says how it has to be
** laid out, so I'm going with a man page and header file definition
** from a Redhat 9 box. 
*/

/* generate the reverse of WEXITSTATUS */
int 
generate_exit_code(
int input_code
)
{

#if !defined(WIN32)
	return( (input_code & 0x00FF) << 8);
#endif

	/* On Win32, we don't do anything special */
	return input_code;
}

/* Return the reverse of WTERMSIG */
int 
generate_exit_signal(
int input_signal
)
{
#if !defined(WIN32)
	return( (input_signal & 0x007F) );
#endif

	/* On Win32, we don't do anything special */
	return input_signal;
}
