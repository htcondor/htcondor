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
/****************************************************************
Purpose:
	Look for a "core" file at a given location.  Return TRUE if it
	exists and appears complete and correct, and FALSE otherwise.
	The SUN core consists of a header, the data and stack segments
	and finally the uarea.  We check the magic number in the header
	first.  If that's OK we then calculate what the size of the
	core should be using the size of the header, the sizes of the
	data and stack areas recorded in the header and the size of the
	uarea as defined in "sys/param.h".  We then compare this
	calculation with the actual size of the file.
Portability:
	This code depends upon the core format for SUNOS4.1 systems,
	and is not portable to other systems.

Modified for Solaris by : Dhaval N. Shah
							 University of Wisconsin, Madison
**                           Computer Sciences Department 
******************************************************************/

#define _ALL_SOURCE

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_constants.h"
#include <sys/file.h>
#include <sys/core.h>
#include <sys/param.h>
#include "proto.h"


#if defined(Solaris)
  #define UPAGES 4 
  #define NBPG 0x1000   /*couldnt get this val from header files. Rough
		        estimate from Suns header files - Raghu */
  #undef CORE_MAGIC
  #if defined(X86)
  #define CORE_MAGIC 0x464c457f  /* at least, this is the magic for my
									core file - Jim B. */
  #else
  #define CORE_MAGIC 0x7f454c46
  #endif
#endif

const int       UAREA_SIZE = UPAGES * NBPG;
const int	HEADER_SIZE = sizeof(struct core);

int
core_is_valid( char *name )
{
	struct core header;

	int		fd;
	int		total_size;
	off_t	file_size;

	dprintf( D_ALWAYS,
		"Analyzing core file \"%s\" for existence and completeness\n", name
	);

	if( (fd=open(name,O_RDONLY)) < 0 ) {
		dprintf( D_ALWAYS, "Can't open core file \"%s\", errno = %d\n",
				 name, errno );
		return FALSE;
	}

	if( read(fd,(char *)&header,HEADER_SIZE) != HEADER_SIZE ) {
		dprintf( D_ALWAYS, "Can't read header from core file\n" );
		(void)close( fd );
		return FALSE;
	}

	file_size = lseek( fd, (off_t)0, SEEK_END );
	(void)close( fd );


	if( header.c_magic != CORE_MAGIC ) {
		dprintf( D_ALWAYS,
			"Core file - bad magic number (0x%x)\n", header.c_magic
		);
		return FALSE;
	}

	dprintf( D_ALWAYS,
		"Core file - magic number OK\n"
	);

	/* the size test is bogus on Solaris -- there is something wrong
	   with the header information -- I'm taking it out.  -Jim B. */

/*	total_size = header.c_len + header.c_dsize + header.c_ssize + UAREA_SIZE;

	if( file_size != total_size ) {
		dprintf( D_ALWAYS,
			"file_size should be %d, but is %d\n", total_size, file_size
		);
		return FALSE;
	} else {
		dprintf( D_ALWAYS,
			"Core file_size of %d is correct\n", file_size
		); */
		return TRUE;
/*	} */

#if 0
	dprintf( D_ALWAYS, "c_len = %d bytes\n", header.c_len );
	dprintf( D_ALWAYS, "c_dsize = %d bytes\n", header.c_dsize );
	dprintf( D_ALWAYS, "c_ssize = %d bytes\n", header.c_ssize );
	dprintf( D_ALWAYS, "UAREA_SIZE = %d bytes\n", UAREA_SIZE );
	dprintf( D_ALWAYS, "total_size = %d bytes\n", total_size );
#endif


}
