/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
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
