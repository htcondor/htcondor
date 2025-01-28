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
#include "condor_debug.h"

#if defined (LINUX)
#include "execinfo.h"

/*
void
__stack_chk_fail() {
  void *trace[2];
  char **messages = (char **)NULL;

  backtrace(trace, 2);
  messages = backtrace_symbols(trace, 2);
  EXCEPT("Stack overflow at: %s", messages[1]);
}
*/
#endif

#ifdef LINT
/* VARARGS */
_EXCEPT_(foo)
const char	*foo;
{ printf( foo ); }
#else /* LINT */

bool _EXCEPT_Active = false;
int _EXCEPT_Line = 0;
int _EXCEPT_Errno = 0;
const char *_EXCEPT_File = nullptr;
int (*_EXCEPT_Cleanup)(int,int,const char*) = nullptr;
void (*_EXCEPT_Reporter)(const char * msg, int line, const char * file) = nullptr;

extern int		_condor_dprintf_works;

	/* This is configured in the config file with ABORT_ON_EXCEPTION = True */
static int _condor_except_should_dump_core;
void condor_except_should_dump_core( int flag ) {
	_condor_except_should_dump_core = flag;
}

void
_EXCEPT_(const char *fmt, ...)
{
	va_list pvar;
	char buf[ BUFSIZ ];

	if (_EXCEPT_Active) {
		exit(JOB_EXCEPTION);
	}
	_EXCEPT_Active = true;

	va_start(pvar, fmt);

	vsnprintf( buf, BUFSIZ, fmt, pvar );

	if (_EXCEPT_Reporter) {
		_EXCEPT_Reporter(buf, _EXCEPT_Line, _EXCEPT_File);
	} else
	if( _condor_dprintf_works ) {
		dprintf( D_ERROR | D_EXCEPT, "ERROR \"%s\" at line %d in file %s\n",
				 buf, _EXCEPT_Line, _EXCEPT_File );
	} else {
		fprintf( stderr, "ERROR \"%s\" at line %d in file %s\n",
				 buf, _EXCEPT_Line, _EXCEPT_File );
	}

	if( _EXCEPT_Cleanup ) {
		(*_EXCEPT_Cleanup)( _EXCEPT_Line, _EXCEPT_Errno, buf );
	}

	va_end(pvar);

	if( _condor_except_should_dump_core ) {
		abort();
	}

	exit( JOB_EXCEPTION );
}

#endif /* LINT */
