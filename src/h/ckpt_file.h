/* 
** Copyright 1986, 1987, 1988, 1989, 1990, 1991 by the Condor Design Team
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the Condor Design Team not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the Condor
** Design Team make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE CONDOR DESIGN TEAM DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE CONDOR DESIGN TEAM BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Authors:  Allan Bricker and Michael J. Litzkow,
** 	         University of Wisconsin, Computer Sciences Dept.
** 
*/ 

#ifndef _POSIX_SOURCE
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
