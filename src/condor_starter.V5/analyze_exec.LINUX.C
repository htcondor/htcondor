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

#define _POSIX_SOURCE

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_constants.h"
#include "condor_syscall_mode.h"
#include <sys/file.h>
#include <bfd.h>


int magic_check( char *executable )
{
	bfd		*bfdp;
	long	storage_needed;


	bfd_init();
	if((bfdp=bfd_openr(executable, 0))!=NULL) {
		dprintf(D_ALWAYS, "\n\nbfdopen(%s) succeeded\n", executable);
		// We only want objects, not cores, ...
		if(bfd_check_format(bfdp, bfd_object)) {
			dprintf(D_ALWAYS, "File type=bfd_object\n");
			// Make sure it is an executable object.
			// FIXME - Make sure this eliminates .o files...
			if(bfd_get_file_flags(bfdp) & EXEC_P)
				dprintf(D_ALWAYS, "This file is executable\n");
			else
				dprintf(D_ALWAYS, "This file is NOT executable\n");
			bfd_close(bfdp);
			return 0;
		} else {
			dprintf(D_ALWAYS, "File type unknown!\n");
			bfd_close(bfdp);
			return -1;
		}
	} else {
		dprintf(D_ALWAYS, "bfdopen(%s) failed\n", executable);
		return -1;
	}
}

/*
  - Check to see that the checkpoint file is linked with the Condor
  - library by looking for the symbol "_linked_with_condor_message".
*/
int symbol_main_check( char *executable )
{
	bfd		*bfdp;
	long	storage_needed;
	asymbol	**symbol_table;
	long	number_of_symbols;
	long	i;


	bfd_init();
	if((bfdp=bfd_openr(executable, 0))!=NULL) {
		dprintf(D_ALWAYS, "\n\nbfdopen(%s)\n", executable);
		if(bfd_check_format(bfdp, bfd_object)) {
			storage_needed=bfd_get_symtab_upper_bound(bfdp);
			// Calculate storage needed for the symtab
			if(storage_needed < 0) {
				dprintf(D_ALWAYS, "Read of symbol table failed");
				bfd_close(bfdp);
				return -1;
			} else if(storage_needed==0) {
				dprintf(D_ALWAYS, "Executable has been stripped??");
				bfd_close(bfdp);
				return 0;
			}
			dprintf(D_ALWAYS, "storage_needed=%d\n", storage_needed);
			symbol_table=(asymbol **)malloc(storage_needed);
			if(!symbol_table) {
				dprintf(D_ALWAYS, "malloc failed");
				bfd_close(bfdp);
				return -1;
			}

			// Get number of syms in the symbol table
			number_of_symbols=bfd_canonicalize_symtab(bfdp, symbol_table);
			dprintf(D_ALWAYS, "number_of_symbols=%d\n", number_of_symbols);
			if(number_of_symbols < 0) {
				dprintf(D_ALWAYS, "Error reading symbol table");
				bfd_close(bfdp);
				free(symbol_table);
				return -1;
			}

			// Search for the _linked_with_condor_message sym
			for(i=0;i<number_of_symbols;i++) {
				if(strcmp(bfd_asymbol_name(symbol_table[i]), "_linked_with_condor_message")==0) {
        			dprintf( D_ALWAYS, 	
						"Symbol _linked_with_condor_message check - OK\n" );
					bfd_close(bfdp);
					free(symbol_table);
					return 0;
				}
			}
		}
	} else {
		dprintf(D_ALWAYS, "bfdopen(%s) failed in symbol check\n", executable);
		return -1;
	}
}


// Are these even used???
int
calc_hdr_blocks()
{
	/*
	return(sizeof(struct exec));
	*/
}

int
calc_text_blocks( char *name )
{
	/*
	int		exec_fd;
	struct exec	exec_struct;

	if( (exec_fd=open(name,O_RDONLY,0)) < 0 ) {
		dprintf( D_ALWAYS, "open(%s,O_RDONLY,0)", name );
		return -1;
	}

	return exec_struct.a_text / 1024;
	*/
}
