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
** Authors:  Michael J. Litzkow,
** 	         University of Wisconsin, Computer Sciences Dept.
** 
*/ 

/********************************************************************
**
**	Description:
**	Maintain status of local machine in easily obtainable form.  Each
**	possible status is represented by an integer, (see sched.h).  Status's
**	are:
**		JOB_RUNNING			-- currently executing a condor job
**		KILLED				-- condor job is in process of leaving
**		CHECKPOINTING		-- condor job is in process of periodic checkpoint
**		SUSPENDED			-- condor job is temporarily suspended
**		USER_BUSY			-- machine currently busy with non-Condor work
**		M_IDLE				-- machine idle
**		CONDOR_DOWN			-- condor daemons not running
**		BLOCKED				-- blocked by special system activity (Dumps)
**
**
**	Exports:
**
** 		set_machine_status( status )
**		int	status;
**
**		int get_machine_status()
**
**  Current implementation:
**		The integer codes are maintained in a well known file in ascii.
**		The name of the file is looked up in the config file.
**
********************************************************************/


#include <stdio.h>
#include <sys/file.h>
#include <errno.h>
#include "sched.h"
#include "debug.h"
#include "except.h"

static char *_FileName_ = __FILE__;		/* Used by EXCEPT (see except.h)     */

static int	ReadFd = -1;
static int	WriteFd = -1;
static char	Buf[32];

char *param();

extern int	errno;


set_machine_status( status )
int		status;
{
	if( WriteFd < 0 ) {
		WriteFd = init_machine_status(O_WRONLY|O_CREAT|O_TRUNC,0644);
		if( WriteFd < 0 ) {
			return -1;
		}
	}
		
	if( lseek(WriteFd,0,0) ) {
		EXCEPT( "lseek(%d,0,0)", WriteFd );
	}
	sprintf( Buf, "%d\n", status );
	while( write(WriteFd,Buf,strlen(Buf)) != strlen(Buf) ) {
		if( errno == EINTR ) {
			dprintf( D_ALWAYS, "Write to MachineStatus failed -- retring\n" );
		} else {
			EXCEPT( "write(%d,0x%x,%d\n", WriteFd,Buf,strlen(Buf) );
		}
	}
}

get_machine_status()
{
	if( ReadFd < 0 ) {
		ReadFd = init_machine_status(O_RDONLY,0);
		if( ReadFd < 0 ) {
			return -1;
		}
	}
		
	lseek( ReadFd, 0, 0 );
	read( ReadFd, Buf, sizeof(Buf) );
	return atoi( Buf );
}

static
init_machine_status( flags, mode )
int		flags;
int		mode;
{
	char	*name;
	int		fd;

	name = param( "MACHINE_STATUS_FILE" );

	if( name == NULL ) {
		dprintf( D_ALWAYS,
			"Warning: no MACHINE_STATUS_FILE specified in config file\n" );
		return -1;
	}

	fd = open( name, flags, mode );
	if( fd < 0 &&  (flags & O_CREAT) == O_CREAT ) {
		EXCEPT( "open(%s,0%o,0%o)", name, flags, mode );
	}
	free( name );

	return fd;
}
