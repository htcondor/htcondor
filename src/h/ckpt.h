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
