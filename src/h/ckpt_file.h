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

 

#if !defined(_POSIX_SOURCE) &&  !defined(Solaris) && !defined(HPUX10) && !defined(IRIX)
typedef unsigned short ushort_t;
typedef unsigned long ulong_t;
#endif

/*
**	The rstrt record contains all information needed to
**	restart a checkpointed program.
*/

#ifndef _CKPT_FILE_H
#define _CKPT_FILE_H

typedef struct FileInfo FINFO;

struct FileInfo {
	mode_t	fi_flags;		/* See below                            */
	mode_t	fi_priv;		/* R/W/RW                               */
	off_t	fi_pos;			/* Position within the file         	*/
	int 	fi_fdno;		/* File descriptor number               */
	int 	fi_dupof;		/* Fd this is a dup of                  */
	char   	*fi_path;		/* Variable length *FULL* path to file  */
};

/*
**	Flag definitions for fi_flags
*/
#define FI_OPEN			0x0001		/* This file is open                     */
#define FI_DUP			0x0002		/* This file is a dup of 'fi_fdno'       */
#define FI_PREOPEN		0x0004		/* This file was opened previously       */
#define FI_NFS			0x0008		/* access via NFS 				         */
#define FI_RSC			0x0000		/* access via remote sys calls	         */
#define FI_WELL_KNOWN	0x0010		/* well know socket to the shadow        */

/*
** Remote file access methods
*/

#ifndef NOFILE
#ifdef OPEN_MAX
#define NOFILE OPEN_MAX
#else
#define NOFILE _POSIX_OPEN_MAX
#endif
#endif

struct RestartRec {
	FINFO		rr_file[NOFILE];/* Open file information                     */
};
typedef struct RestartRec RESTREC;

#define CONDOR_SIG_SAVE 1
#define CONDOR_SIG_RESTORE 2

#endif /* _CKPT_FILE_H */
