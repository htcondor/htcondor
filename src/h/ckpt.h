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


#include <a.out.h>

#include "ckpt_file.h"
#include "job.h"

/*
**	In a checkpoint file, the a_syms field of the exec header
**	points to the checkpoint map.  This map tells the position
**	and length of various structures needed to restart the job.
*/
typedef struct {
	int		cs_pos;				/* The position in the checkpoint file       */
	int		cs_len;				/* The length of the segment                 */
	int		cs_fd;				/* The fd to read from (internal to _mkckpt) */
} CKPTSEG;


#define CM_TEXT		0			/* The checkpoint text segment               */
#define CM_DATA		1			/* The checkpoint data segment               */
#define CM_TRELOC	2			/* The checkpoint text reloc info segment    */
#define CM_DRELOC	3			/* The checkpoint data reloc info segment    */
#define CM_SYMS		4			/* The checkpoint symbol table segment       */
#define CM_STRTAB	5			/* The checkpoint string table segment       */
#define CM_STACK	6			/* The checkpoint stack segment              */

#ifdef notdef
#define CM_JOB		7			/* The job structure segment                 */
#define CM_SEGMAP	8			/* The checkpoint map segment                */
#define NCKPTSEGS	9
#else notdef

#define NCKPTSEGS	7
#endif notdef

typedef struct {
	CKPTSEG	cm_segs[NCKPTSEGS];	/* The map is an array of segments           */
} CKPTMAP;

/*
**	Version number for current checkpoint format
*/
#define CKPT_VERS		400
