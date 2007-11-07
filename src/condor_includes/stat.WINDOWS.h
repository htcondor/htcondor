/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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