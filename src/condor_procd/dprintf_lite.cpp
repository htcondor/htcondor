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

FILE* debug_fp = NULL;
char *debug_fn = NULL;
int log_size = -1;

extern "C" void preserve_log_file();

static char *formatTimeHeader(struct tm *tm) {
	static char timebuf[80];
	static char *timeFormat = 0; 
	static int firstTime = 1;

	if (firstTime) {
		firstTime = 0;
		timeFormat = strdup("%m/%d/%y %H:%M:%S ");
	}	
	
	strftime(timebuf, 80, timeFormat, tm);
	return timebuf;
}


extern "C" void
dprintf(int, const char* format, ...)
{
	if (debug_fp != NULL) {
		time_t clock_now;
		(void)time( &clock_now );
		struct tm *tm = localtime( &clock_now );
		fprintf(debug_fp, "%s: ", formatTimeHeader(tm));
		va_list ap;
		va_start(ap, format);
		vfprintf(debug_fp, format, ap);
		va_end(ap);
		fflush(debug_fp);
		long pos = ftell(debug_fp);
		if (log_size > 0 && pos > log_size)
			preserve_log_file(); 
		
	}
}

int	_EXCEPT_Line;
const char*	_EXCEPT_File;
int	_EXCEPT_Errno;

extern "C" void
_EXCEPT_(const char* format, ...)
{
	if (debug_fp) {
		time_t clock_now;
		(void)time( &clock_now );
		struct tm *tm = localtime( &clock_now );
		fprintf(debug_fp, "%s: ", formatTimeHeader(tm));
		fprintf(debug_fp, "ERROR: ");
		va_list ap;
		va_start(ap, format);
		vfprintf(debug_fp, format, ap);
		va_end(ap);
		fprintf(debug_fp, "\n");
		fflush(debug_fp);
		int pos = ftell(debug_fp);
		if (log_size > 0 && pos > log_size)
			preserve_log_file(); 
	}
	exit(1);
}


extern "C" FILE *
open_debug_file(const char flags[])
{
	FILE		*fp;
	int save_errno;


	/* Note: The log file shouldn't need to be group writeable anymore,
	   since PRIV_CONDOR changes euid now. */

	errno = 0;
	if( (fp=safe_fopen_wrapper_follow(debug_fn,flags,0644)) == NULL ) {
		save_errno = errno;

		if (debug_fp == 0) {
			debug_fp = stderr;
		}
		fprintf( debug_fp, "Can't open \"%s\" (error code: %i)\n",debug_fn, save_errno);

		return NULL;
	}

	return fp;
}

extern "C" void
preserve_log_file()
{
	char		old[MAXPATHLEN + 4];
	int			still_in_old_file = FALSE;
	int			failed_to_rotate = FALSE;
	int			save_errno;
	int         rename_failed = 0;
	time_t clock_now;
	struct tm *tm; 

	(void)sprintf( old, "%s.old", debug_fn );
	(void)time( &clock_now );
	tm = localtime( &clock_now );
	fprintf(debug_fp, "%s: ", formatTimeHeader(tm));
	fprintf( debug_fp, "Saving log file to \"%s\"\n", old );
	(void)fflush( debug_fp );

	fclose(debug_fp);
	debug_fp = NULL;

#if defined(WIN32)

	(void)unlink(old);

	/* use rename on WIN32, since link isn't available */
	if (rename(debug_fn, old) < 0) {
		/* the rename failed, perhaps one of the log files
		 * is currently open.  Sleep a half second and try again. */		 
		Sleep(500);
		(void)unlink(old);
		if ( rename(debug_fn,old) < 0) {
			/* Feh.  Some bonehead must be keeping one of the files
			 * open for an extended period.  Win32 will not permit an
			 * open file to be unlinked or renamed.  So, here we copy
			 * the file over (instead of renaming it) and then truncate
			 * our original. */

			if ( CopyFile(debug_fn,old,FALSE) == 0 ) {
				/* Even our attempt to copy failed.  We're screwed. */
				failed_to_rotate = TRUE;
			}

			/* now truncate the original by reopening _not_ with append */
			debug_fp = open_debug_file("w");
			if ( debug_fp ==  NULL ) {
				still_in_old_file = TRUE;
			}
		}
	}

#else

	errno = 0;
	if( rename(debug_fn, old) < 0 ) {
		save_errno = errno;
		if( save_errno == ENOENT ) {
				/* This can happen if we are not using debug file locking,
				   and two processes try to rotate this log file at the
				   same time.  The other process must have already done
				   the rename but not created the new log file yet.
				*/
			rename_failed = 1;
		}
		else {
			fprintf( stderr, "Can't rename(%s,%s)\n",
					  debug_fn, old );
			return;
		}
	}

#endif

	if (debug_fp == NULL) {
		debug_fp = open_debug_file( "a");
	}

	if( debug_fp == NULL ) {
		save_errno = errno;
		fprintf(stderr, "Can't open file %s\n",
				 debug_fn ); 
		return;
	}

	if ( !still_in_old_file ) {
		(void)time( &clock_now );
		tm = localtime( &clock_now );
		fprintf(debug_fp, "%s: ", formatTimeHeader(tm));
		fprintf (debug_fp, "Now in new log file %s\n", debug_fn);
	}

	if ( failed_to_rotate || rename_failed ) {
		fprintf(debug_fp,"WARNING: Failed to rotate log into file %s!\n",old);
		if( rename_failed ) {
			fprintf(debug_fp,"Likely cause is that another Condor process rotated the file at the same time.\n");
		}
		else {
			fprintf(debug_fp,"       Perhaps someone is keeping log files open???");
		}
	}
}

/* Some additional symbols referenced by ../condor_utils/selector.cpp.
 */
DebugOutputChoice AnyDebugBasicListener;

BEGIN_C_DECLS
void _mark_thread_safe(int /* start_or_stop */, int /* dologging */,
					   const char* /* descrip */, const char* /* func */,
					   const char* /* file */, int /* line */)
{
}
END_C_DECLS
