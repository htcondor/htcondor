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

 

typedef struct filehdr	FILE_HDR;
typedef struct scnhdr	SCN_HDR;
typedef struct aouthdr	AOUT_HDR;

#define FILE_HDR_SIZ	sizeof(FILE_HDR)
#define SCN_HDR_SIZ		sizeof(SCN_HDR)
#define AOUT_HDR_SIZ	sizeof(AOUT_HDR)

/* Apparently MIPS file headers contain byte counts rather than structure
   counts. -- mike
#define RELOC_SIZ		sizeof(RELOC)
#define SYM_SIZ			sizeof(struct syment)
#define LINENO_SIZ		sizeof(struct lineno)
*/
#define RELOC_SIZ		1
#define SYM_SIZ			1
#define LINENO_SIZ		1

typedef struct {
	int		fd;
	int		offset;
	int		len;
} FILE_BLOCK;

typedef struct {
	char	*data;
	int		len;
} CORE_BLOCK;

typedef struct {
	int		block_tag;
	union {
		FILE_BLOCK	file_block;
		CORE_BLOCK	core_block;
	} block_val;
} DATA_BLOCK;
#define FILE_DATA	1
#define CORE_DATA	2
	
typedef struct {
	SCN_HDR			*hdr;
	DATA_BLOCK		raw_data;
	DATA_BLOCK		reloc_info;
	DATA_BLOCK		lineno_info;
} SECTION;

typedef struct {
	FILE_HDR		*file_hdr;
	AOUT_HDR		*aout_hdr;
	SECTION			*section;	/* allocate and access as an array */
	DATA_BLOCK		sym_tab_loc;
	DATA_BLOCK		str_tab_loc;
} COFF_DESC;
