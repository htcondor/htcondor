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
** Authors:  Allan Bricker and Michael J. Litzkow,
** 	         University of Wisconsin, Computer Sciences Dept.
** 
*/ 


/************************************************************************
**
**	Set up the various dprintf variables based on the configuration file.
**
************************************************************************/

#include <stdio.h>
#include <varargs.h>
#include <time.h>
#include <sys/file.h>
#include <sys/param.h>

#include "condor_sys.h"
#include "debug.h"
#include "except.h"
#include "clib.h"

static char *_FileName_ = __FILE__;		/* Used by EXCEPT (see except.h)     */

int		Termlog;

dprintf_config( subsys, logfd )
char *subsys;
int logfd;		/* The descriptor to use if the log output goes to a tty */
{
	char pname[ BUFSIZ ];
	char *pval, *param();

	/*
	**	Pick up the subsys_DEBUG parameters
	*/
	(void)sprintf(pname, "%s_DEBUG", subsys);
	pval = param(pname);
	if( pval == NULL ) {
		DebugFlags = D_ALWAYS;
	} else {
		set_debug_flags( pval );
	}

	/*
	**	If this is not going to the terminal, pick up the name
	**	of the log file, maximum log size, and the name of the
	**	lock file (if it is specified).
	*/
	if( !( DebugFlags & D_TERMLOG) ) {
		(void)sprintf(pname, "%s_LOG", subsys);
		DebugFile = param(pname);

		if( DebugFile == NULL ) {
			EXCEPT("No '%s' parameter specified.", pname);
		}

		(void)sprintf(pname, "TRUNC_%s_LOG_ON_OPEN", subsys);
		pval = param(pname);
		if( pval && (*pval == 't' || *pval == 'T') ) {
			DebugFP = fopen( DebugFile, "w" );
		} else {
			DebugFP = fopen( DebugFile, "a" );
		}

		if( DebugFP == NULL ) {
			EXCEPT("Cannot open log file '%s'", DebugFile);
		}

		(void)fclose( DebugFP );
		DebugFP = (FILE *)0;

#ifdef notdef
		/*
		**	We don't need these anymore...
		*/
		(void) close(0);
		(void) close(1);
		(void) close(2);
#endif notdef

		(void)sprintf(pname, "MAX_%s_LOG", subsys);
		pval = param(pname);
		if( pval != NULL ) {
			MaxLog = atoi( pval );
		}

		if( MaxLog == 0 ) {
			MaxLog = 65536;
		}

		(void)sprintf(pname, "%s_LOCK", subsys);
		DebugLock = param(pname);
	} else {
		if( fileno(stderr) != logfd ) {
			if( dup2(fileno(stderr), logfd) < 0 ) {
				perror("Can't get error channel:");
				exit( 1 );
			}
		}
#if !defined(HPUX8)
#if defined(LINT) || defined(ULTRIX42) || defined(ULTRIX43) || defined(IRIX331)
		((stderr)->_file) = logfd;
#else
		fileno(stderr) = logfd;
#endif
#else /* HPUX8 is defined */
	stderr->__fileL = logfd & 0xf0;	/* Low byte of fd */
	stderr->__fileH = logfd & 0x0f;	/* High byte of fd */
#endif

		setlinebuf( stderr );

		(void)fflush( stderr );	/* Don't know why we need this, but if not here
							   the first couple dprintf don't come out right */
	}
}

#if 0
static
#endif
set_debug_flags( strflags )
char *strflags;
{
	char tmp[ BUFSIZ ];
	char *argv[ BUFSIZ ], *flag;
	int argc, notflag, bit, i;

	(void)strncpy(tmp, strflags, sizeof(tmp));
	mkargv(&argc, argv, tmp);

	while( --argc >= 0 ) {
		flag = argv[argc];
		if( *flag == '-' ) {
			flag += 1;
			notflag = 1;
		} else {
			notflag = 0;
		}

		bit = 0;
		if( stricmp(flag, "D_ALL") == 0 ) {
			bit = D_ALL;
		} else for( i = 0; i < D_MAXFLAGS; i++ ) {
			if( stricmp(flag, DebugFlagNames[i]) == 0 ) {
				bit = (1 << i);
				break;
			}
		}

		if( notflag ) {
			DebugFlags &= ~bit;
		} else {
			DebugFlags |= bit;
		}
	}
	if( Termlog ) {
		DebugFlags |= D_TERMLOG;
	}
}
