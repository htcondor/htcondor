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
