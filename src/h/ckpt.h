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
