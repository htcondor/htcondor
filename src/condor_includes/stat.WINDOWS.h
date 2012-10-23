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


#ifndef _WINDOWS_STAT_FIX_
#define _WINDOWS_STAT_FIX_

/*
	It appearers that the _[f]stat function under Windows NT 4.0 and, by all 
	reports, many other Windows systems, will return file times 3600 seconds 
	(1 hour) too big when daylight savings time is in effect (this also 
	includes st_atime and st_ctime).  This seems surprising to me: since, in 
	general, the MSDN is an accurate source of information, but that the stat 
	section leaves out is a glaring omission. This is a to be considered a bug since 
	it is defined as returning the time in UTC. The following fix is kinda gross, 
	but it seems to fix the problem. Also, since the problem has been around since 
	NT 4.0, it's sure to survive at least another 10 years... if not forever.
	-B [11/6/2007]
*/

/* 
	I hate macros, 'C' and those that thought it clever to name the functions
	that we used on structures after the structures themselves (differentiating 
	them only by the keyword struct: as if it would shield the fortunate 
	maintenance people from having to redefine the functions and structures to fix 
	the shortsightedness of the original designers) 
*/
struct _fixed_windows_stat {
	_dev_t st_dev;
	_ino_t st_ino;
	unsigned short st_mode;
	short st_nlink;
	short st_uid;
	short st_gid;
	_dev_t st_rdev;
	_off_t st_size;
	time_t st_atime;
	time_t st_mtime;
    time_t st_ctime;
};

#if defined ( __cplusplus )
extern "C" {
#endif

/* on success returns the file's *real* UTC time; otherwise, -1 */
int
_fixed_windows_stat(const char *file, struct _fixed_windows_stat *sbuf);

/* on success returns the file's *real* UTC time; otherwise, -1 */
int
_fixed_windows_fstat(int file, struct _fixed_windows_stat *sbuf);

#if defined ( __cplusplus )
}
#endif // defined ( __cplusplus )

#endif // _WINDOWS_STAT_FIX_
