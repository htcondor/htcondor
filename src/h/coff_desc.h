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
** Author:  Michael J. Litzkow
** 	         University of Wisconsin, Computer Sciences Dept.
** 
*/ 

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
