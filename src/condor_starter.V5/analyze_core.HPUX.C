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

/****************************************************************
Purpose:
	Look for a "core" file at a given location.  Return TRUE if it
	exists and appears complete and correct, and FALSE otherwise.
	We check that we recognize each coreheader segment, that the
	core file format version is what we expect/recognize, and that
	the core file does not appear truncated.
Portability:
	This code depends upon the core format for HPUX 9.x systems,
	and is not portable to other systems.
******************************************************************/

#include "condor_common.h"
#include <sys/core.h>
#include "condor_debug.h"
#include "proto.h"


/*
** Do a seek(), but check the return value.
*/
int
my_seek( int fd, int offset, int whence )
{
        int             answer;

        if( (answer=lseek(fd,offset,whence))  < 0 ) {
                perror( "lseek" );
                exit( 1 );
        }
        return answer;
}

int core_is_valid( char *name )
{
        int  fd, nbytes, next_hdr, this_obj;
        struct corehead head, *h = &head;
	int version = -1;
	int found_data = FALSE;
	int found_stack = FALSE;
	int bad_header = FALSE;
	unsigned int total_size = 0;
	unsigned int actual_size = 0;


	fd = open(name,O_RDONLY);
	if ( fd < 0 ) {
		dprintf(D_ALWAYS,"cannot open core file %s in core_is_valid\n",name);
		return FALSE;
	}

        for(;;) {

		h->type = -1;

                nbytes = read( fd, h, sizeof(head) );

                if( nbytes == 0 ) {
                        break;
                }

                this_obj = my_seek( fd, 0, 1 );
                next_hdr = this_obj + h->len;

                switch( h->type ) {
                        case CORE_DATA:
				found_data = TRUE;
                               break;
                        case CORE_STACK:
				found_stack = TRUE;
                                break;
			case CORE_FORMAT:
				read(fd,(char *) &version,sizeof(int));
				break;		
			case CORE_EXEC:
			case CORE_KERNEL:
			case CORE_PROC:
			case CORE_TEXT:
			case CORE_SHM:
			case CORE_MMF:
				break;
                        default:
				/* if we made it here, we don't recognize
				 * this coreheader */
				bad_header = TRUE;
                                break;
                }

		total_size += ( sizeof(struct corehead) + h->len );
                my_seek( fd, next_hdr, 0 );
        }

	actual_size = lseek( fd, 0, SEEK_END );

	close(fd);

	if ( (version != 1) && (version != -1) ) {
		dprintf(D_ALWAYS,"Core file has unsupported format version (ver=%d)\n",version);
		return FALSE;
	}

	if ( bad_header == TRUE ) {
		dprintf(D_ALWAYS,"Core file had unrecognized headers\n");
		return FALSE;
	}

	if ( found_data == FALSE ) {
		dprintf(D_ALWAYS,"No data area found in core file\n");
		return FALSE;
	}

	if ( found_stack == FALSE ) {
		dprintf(D_ALWAYS,"No stack area found in core file\n");
		return FALSE;
	}

	if ( actual_size < total_size ) {
		dprintf(D_ALWAYS,"Core file appears truncated/incomplete\n");
		return FALSE;
	}

	/* if we made it this far, things check out OK */
	return TRUE;
}
