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

 
#include "condor_common.h"
#include "condor_debug.h"
#include "condor_constants.h"
#include "condor_ver_info.h"


#if defined( LINUX )

/****************************************************************
Purpose:
	The checkpoint file transferred from the submitting machine
	should be a valid LINUX executable file, and should have been linked
	with the Condor remote execution library.  Any executable format
	supported by the BFD library will be detected.  For now, the
	accepted formats are a.out and ELF.  We also look for a well known 
	symbol from the Condor library (_linked_with_condor_message) to ensure 
	proper linking.

Portability:
	This code depends upon executable formats for LINUX 2.0.x systems, 
	and is not portable to other systems.
	Requires libbfd and liberty and correct bfd.h and ansidecl.h.
	Some versions of these headers are badly broken.  Get a new
	binutils for correct versions.  I have tested with binutils 
	2.6.0.14 only.  Please back up old versions just in case!!
******************************************************************/

#include "condor_common.h"
#include <sys/file.h>
#include <bfd.h>

extern "C" {
int
sysapi_magic_check( char *executable )
{
	struct stat buf;

	/* This should prolly do more, but for now this is good enough */

	if (stat(executable, &buf) < 0) {
		return -1;
	}
	if (!(buf.st_mode & S_IFREG)) {
		return -1;
	}
	if (!(buf.st_mode & S_IXUSR)) {
		dprintf(D_ALWAYS, "Magic check warning. Executable '%s' not "
			"executable\n", executable);
	}

	return 0;
}


/*
  - Check to see that the checkpoint file is linked with the Condor
  - library by looking for the symbol "_linked_with_condor_message".

  - Ok. This used to do the above work, but now it just uses the version 
  	object to see if the executable contains the right version info.
	I have a feeling this will be rewritten to do what it used to, just not
	with libbfd. But for now, this needs to go in like this to get rid of
	libbfd.
	psilord 04/22/2002
*/
int
sysapi_symbol_main_check( char *executable )
{
	char *version;
	char *platform;
	CondorVersionInfo vinfo;

	version = vinfo.get_version_from_file(executable);
	
    if (version == NULL)
    {
        dprintf(D_ALWAYS, 
			"File '%s' is not a valid standard universe executable\n", 
			executable);
        return -1;
    }

	/* if the above existed, this prolly does too */
	platform = vinfo.get_platform_from_file(executable);

	dprintf( D_ALWAYS,  "Executable '%s' is linked with \"%s\" on a \"%s\"\n",
		executable, version, platform?platform:"(NULL)");

    return 0;
}

} /* extern "C" */

#elif defined( Solaris )

/****************************************************************
Purpose:
	The checkpoint (a.out) file transferred from the submitting machine
	should be a valid ULTRIX executable file, and should have been linked
	with the Condor remote execution library.  Here we check the
	magic numbers to determine if we have a valid executable file, and
	also look for a well known symbol from the Condor library (MAIN)
	to ensure proper linking.

Note : The file has been modified for the Solaris platform by :
	   Dhaval N. Shah
	   University of Wisconsin, Computer Sciences Department.

Portability:
	This code depends upon the executable format for ULTRIX.4.2 systems,
	and is not portable to other systems.
******************************************************************/

#include <sys/file.h>
#include <sys/exec.h>
#include <sys/exechdr.h>
#include <nlist.h>

#if defined(X86)
#define SOLARIS_MAGIC 043114 /* Ugh! */
#else
#define SOLARIS_MAGIC 046106
#endif

extern "C" {

int
sysapi_magic_check( char *a_out )
{
	int		exec_fd;
	struct exec	header;
	int		nbytes;


	if( (exec_fd=open(a_out,O_RDONLY,0)) < 0 ) {
		dprintf( D_ALWAYS, "open(%s,O_RDONLY,0)", a_out );
		return -1;
	}
	dprintf( D_ALWAYS, "Opened file \"%s\", fd = %d\n", a_out, exec_fd );

	errno = 0;
	nbytes = read( exec_fd, (char *)&header, sizeof(header) );
	if( nbytes != sizeof(header) ) {
		dprintf(D_ALWAYS,
			"Error on read(%d,0x%x,%d), nbytes = %d, errno = %d\n",
			exec_fd, (char *)&header, sizeof(header), nbytes, errno
		);
		close( exec_fd );
		return -1;
	}

	close( exec_fd );

	if( header.a_magic != SOLARIS_MAGIC ) {
		dprintf( D_ALWAYS, "\"%s\": BAD MAGIC NUMBER\n", a_out );
		dprintf( D_ALWAYS, "0x%x != 0x%x\n", header.a_magic, SOLARIS_MAGIC );
		dprintf( D_ALWAYS, "0%o != 0%o\n", header.a_magic, SOLARIS_MAGIC );
		dprintf( D_ALWAYS, "%d != %d\n", header.a_magic, SOLARIS_MAGIC );
		return -1;
	}

#if !defined(X86)
	if( header.a_machtype != 69 ) {
		dprintf( D_ALWAYS,
			"\"%s\": NOT COMPILED FOR SPARC ARCHITECTURE\n", a_out
		);
		dprintf( D_ALWAYS, "%d != %d\n", header.a_machtype, 69 );
		return -1;
	}
#endif

#if (!defined Solaris)
	if( header.a_dynamic ) {
		dprintf( D_ALWAYS, "\"%s\": LINKED FOR DYNAMIC LOADING\n", a_out );
		return -1;
	}
#endif
	return 0;
}

/*
Check to see that the checkpoint file is linked with the Condor
library by looking for the symbol "MAIN".
*/
int
sysapi_symbol_main_check( char *name )
{
	int	status;
	struct nlist nl[3];

	/* Unlike on some other architectures, we want to look for
	   MAIN and _condor_prestart on Solaris, not _MAIN and
	   __condor_prestart.  -Jim B. */

	nl[0].n_name = "MAIN";
	nl[1].n_name = "_condor_prestart";
	nl[2].n_name = "";


		/* If nlist fails just return TRUE - executable may be stripped. */
	status = nlist( name, nl );
	if( status < 0 ) {
		dprintf(D_ALWAYS, "Error: nlist(\"%s\",0x%x) returns %d, errno = %d\n",
													name, nl, status, errno);
		return(0);
	}

	if( nl[0].n_type == 0 ) {
		dprintf( D_ALWAYS, "No symbol \"MAIN\" in executable(%s)\n", name );
		return(-1);
	}

	if( nl[1].n_type == 0 ) {
		dprintf( D_ALWAYS,
				"No symbol \"_condor_prestart\" in executable(%s)\n", name );
		return(-1);
	}

	dprintf( D_ALWAYS, "Symbol MAIN check - OK\n" );
	return 0;
}

} /* extern "C" */

#elif defined( IRIX )

/****************************************************************
Purpose:
	The checkpoint (a.out) file transferred from the submitting machine
	should be a valid IRIX executable file, and should have been linked
	with the Condor remote execution library.  Here we check the
	magic numbers to determine if we have a valid executable file, and
	also look for a well known symbol from the Condor library (MAIN)
	to ensure proper linking.
Portability:
	This code depends upon the executable format for IRIX 5.3 systems,
	and is not portable to other systems.
******************************************************************/

#include <sys/file.h>
#include <a.out.h>
#include <nlist.h>


extern "C" {

int
sysapi_magic_check( char *a_out )
{
	int		exec_fd;
	int		nbytes;
	struct filehdr header;

	if( (exec_fd=open(a_out,O_RDONLY,0)) < 0 ) {
		dprintf( D_ALWAYS, "open(%s,O_RDONLY,0)", a_out );
		return -1;
	}
	dprintf( D_ALWAYS, "Opened file \"%s\", fd = %d\n", a_out, exec_fd );

	errno = 0;
	nbytes = read( exec_fd, (char *)&header, sizeof(header) );
	if( nbytes != sizeof(header) ) {
		dprintf(D_ALWAYS,
			"Error on read(%d,0x%x,%d), nbytes = %d, errno = %d\n",
			exec_fd, (char *)&header, sizeof(header), nbytes, errno
		);
		close( exec_fd );
		return -1;
	}

	close( exec_fd );

	if( header.f_magic != 0x7f45 ) {
		dprintf( D_ALWAYS, "\"%s\": BAD MAGIC NUMBER\n", a_out );
		dprintf( D_ALWAYS, "0x%x != 0x%x\n", header.f_magic, 0x7f45 );
		return -1;
	}

	return 0;
}

/*
Check to see that the checkpoint file is linked with the Condor
library by looking for the symbol "MAIN".
*/
int
sysapi_symbol_main_check( char *name )
{
	int	status;
	struct nlist nl[3];

	nl[0].n_name = "MAIN";
	nl[1].n_name = "_condor_prestart";
	nl[2].n_name = "";

		/* If nlist fails just return TRUE - executable may be stripped. */
	status = nlist( name, nl );
	if( status < 0 ) {
		dprintf(D_ALWAYS, "Error: nlist(\"%s\",0x%x) returns %d, errno = %d\n",
													name, nl, status, errno);
		return(0);
	}

	if( nl[0].n_type == 0 ) {
		dprintf( D_ALWAYS, "No symbol \"MAIN\" in executable(%s)\n", name );
		return(-1);
	}

	if( nl[1].n_type == 0 ) {
		dprintf( D_ALWAYS,
				"No symbol \"_condor_prestart\" in executable(%s)\n", name );
		return(-1);
	}

	dprintf( D_ALWAYS, "Symbol MAIN check - OK\n" );
	return 0;
}

} /* extern "C" */

#elif defined( OSF1 )

/****************************************************************
Purpose:
	The checkpoint (a.out) file transferred from the submitting
	machine should be a valid OSF1/DUX executable file, and should
	have been linked with the Condor remote execution library.  Here
	we check the magic numbers to determine if we have a valid
	executable file, and also look for a well known symbol from the
	Condor library (MAIN) to ensure proper linking.

Portability:
	This code depends upon the executable format for OSF1/DUX systems, 
	and is not portable to other systems.
******************************************************************/

#include <sys/file.h>
#include <a.out.h>

#define COFF_MAGIC ALPHAMAGIC
#define AOUT_MAGIC ZMAGIC

typedef struct filehdr  FILE_HDR;
typedef struct aouthdr  AOUT_HDR;
#define FILE_HDR_SIZ    sizeof(FILE_HDR)
#define AOUT_HDR_SIZ    sizeof(AOUT_HDR)

extern "C" {

int
sysapi_magic_check( char *a_out )
{
	int		exec_fd;
	FILE_HDR	file_hdr;
	AOUT_HDR	aout_hdr;
	HDRR		symbolic_hdr;


	if( (exec_fd=open(a_out,O_RDONLY,0)) < 0 ) {
		dprintf( D_ALWAYS, "open(%s,O_RDONLY,0)", a_out );
		return -1;
	}

		/* Deal with coff file header */
	if( read(exec_fd,&file_hdr,FILE_HDR_SIZ) != FILE_HDR_SIZ ) {
		dprintf(D_ALWAYS, "read(%d,0x%x,%d)", exec_fd, &file_hdr, FILE_HDR_SIZ);
		return -1;
	}

	if( file_hdr.f_magic != COFF_MAGIC ) {
		dprintf( D_ALWAYS, "BAD MAGIC NUMBER IN COFF HEADER\n" );
		return -1;
	}
	dprintf( D_ALWAYS, "COFF header - magic number OK\n" );


		/* Deal with optional header (a.out header) */
    if( file_hdr.f_opthdr != AOUT_HDR_SIZ ) {
		dprintf( D_ALWAYS, "BAD A.OUT HEADER SIZE IN COFF HEADER\n" );
		return -1;
    }


    if( read(exec_fd,&aout_hdr,AOUT_HDR_SIZ) != AOUT_HDR_SIZ ) {
    	dprintf( D_ALWAYS,"read(%d,0x%x,%d)", exec_fd, &aout_hdr, AOUT_HDR_SIZ);
		return -1;
	}

	if( aout_hdr.magic != AOUT_MAGIC ) {
		dprintf( D_ALWAYS, "BAD MAGIC NUMBER IN A.OUT HEADER\n" );
		return -1;
	}
	dprintf( D_ALWAYS, "AOUT header - magic number OK\n" );

		/* Bring symbol table header into core */
	if( lseek(exec_fd,file_hdr.f_symptr,0) != file_hdr.f_symptr ) {
		dprintf( D_ALWAYS, "lseek(%d,%d,0)", exec_fd, file_hdr.f_symptr );
		return -1;
	}
	if( read(exec_fd,&symbolic_hdr,cbHDRR) != cbHDRR ) {
		dprintf( D_ALWAYS,
			"read(%d,0x%x,%d)", exec_fd, &symbolic_hdr, cbHDRR );
		return -1;
	}

	if( symbolic_hdr.magic != magicSym ) {
		dprintf( D_ALWAYS, "Bad magic number in symbol table header\n" );
		dprintf( D_ALWAYS, " -- expected 0x%x, found 0x%x\n",
			magicSym, symbolic_hdr.magic );
		return -1;
	}
	return 0;
}

/*
  - Check to see that the checkpoint file is linked with the Condor
  - library by looking for the symbol "MAIN".

  As we move into the new checkpointing and remote system call mechanisms
  the use of "MAIN" may dissappear.  It seems what we really want to
  know here is whether the executable is linked for remote system
  calls.  We should therefore look for a symbol which must be present
  if that is the case.

  On OSF/1 platforms, this routine calls mmap.  Mmap is not a routine
  which we are (yet) supporting for Condor programs.  We tried
  generating a switch for it, and extrating the code directly from
  the C library, but some problem still led to a segmentation fault.
  We have therefore eliminated the switches for mmap and all related
  calls.  This means the file access mode must be unmapped, cause
  there is no mapping code involved.  It seems right then to put
  the whole routine into unmapped file access mode to avoid any
  possible conflicts between routines using mapped and unmapped
  file descriptors.
*/
int
sysapi_symbol_main_check( char *name )
{
	int	status;
	char *target;
	struct nlist nl[2];

	target = "_linked_with_condor_message";
	nl[0].n_name = target;
	nl[0].n_value = 0;
	nl[0].n_type = 0;

	nl[1].n_name = "";
	nl[1].n_value = 0;
	nl[1].n_type = 0;

	dprintf( D_ALWAYS, "nl[0].n_name = \"%s\"\n", nl[0].n_name );
	dprintf( D_ALWAYS, "nl[1].n_name = \"%s\"\n", nl[1].n_name );


	status = nlist( name, nl );

		/* If nlist fails just return TRUE - executable may be stripped. */
	if( status < 0 ) {
		dprintf(D_ALWAYS, "Error: nlist(\"%s\",0x%x) returns %d, errno = %d\n",
													name, nl, status, errno);
		return(0);
	}

	if( status != 0 ) {
		dprintf( D_ALWAYS, "%d name list entries not found\n", status );
	}

	if( nl[0].n_type == 0 ) {
		dprintf( D_ALWAYS,
			"No symbol \"%s\" in executable(%s)\n",
			target,
			name
		);
		return(-1);
	}
	dprintf( D_ALWAYS, "Symbol \"%s\" check - OK\n", target );
	return 0;
}

} /* extern "C" */

#elif defined( HPUX )

#include <filehdr.h>
#include <aouthdr.h>
#include <model.h>
#include <magic.h>
#include <nlist.h>

extern "C" {

int
sysapi_magic_check( char *a_out )
{
	int exec_fd = -1;
	struct header exec_header;
	struct som_exec_auxhdr hpux_header;
	int nbytes;

	if ( (exec_fd=open(a_out,O_RDONLY)) < 0) {
		dprintf(D_ALWAYS,"error opening executeable file %s\n",a_out);
		return(-1);
	}

	nbytes = read(exec_fd, (char *)&exec_header, sizeof( exec_header) );
	if ( nbytes != sizeof( exec_header) ) {
		dprintf(D_ALWAYS,"read executeable main header error \n");
		close(exec_fd);
		return(-1);
	}

	close(exec_fd);

	if ( exec_header.a_magic != SHARE_MAGIC &&
	   	 exec_header.a_magic != EXEC_MAGIC ) {
		dprintf(D_ALWAYS,"EXECUTEABLE %s HAS BAD MAGIC NUMBER (0x%x)\n",a_out,exec_header.a_magic);
		return -1;
	}

	return 0;
}


/*
Check to see that the checkpoint file is linked with the Condor
library by looking for the symbol "_START".
*/
int
sysapi_symbol_main_check( char *name )
{
	int status;
	struct nlist nl[2];

	nl[0].n_name = "_START";
	nl[1].n_name = "";

	status = nlist( name, nl);

	/* Return TRUE even if nlist reports an error because the executeable
	 * may have simply been stripped by the user */
	if ( status < 0 ) {
		dprintf(D_ALWAYS,"Error: nlist returns %d, errno=%d\n",status,errno);
		return 0;
	}

	if ( nl[0].n_type == 0 ) {
		dprintf(D_ALWAYS,"No symbol _START found in executeable %s\n",name);
		return -1;

	}

	return 0;
}

} /* extern "C" */

#else

#error DO NOT KNOW HOW TO ANALYZE EXECUTABLES ON THIS PLATFORM

#endif


