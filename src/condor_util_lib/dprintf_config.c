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


/************************************************************************
**
**	Set up the various dprintf variables based on the configuration file.
**
************************************************************************/

#include "condor_common.h"
#include "condor_sys.h"
#include "debug.h"
#include "except.h"
#include "clib.h"
#include "condor_string.h"

static char *_FileName_ = __FILE__;		/* Used by EXCEPT (see except.h)     */

int		Termlog;

void set_debug_flags( char *strflags );
void debug_unlock();
FILE *open_debug_file( char flags[] );

void
dprintf_config( subsys, logfd )
char *subsys;
int logfd;		/* logfd is the descriptor to use if the log output goes to a tty */
{
	char pname[ BUFSIZ ];
	char *pval, *param();
	static int did_truncate = 0;
	int want_truncate = 0;

	/*  
	**  We want to initialize this here so if we reconfig and the
	**	debug flags have changed, we actually use the new
	**  flags.  -Derek Wright 12/8/97 
	*/
	DebugFlags = D_ALWAYS;

	/*
	**	Pick up the subsys_DEBUG parameters.   Note: if we don't have
	**  anything set, we just leave it as D_ALWAYS.
	*/
	(void)sprintf(pname, "%s_DEBUG", subsys);
	pval = param(pname);
	if( pval ) {
		set_debug_flags( pval );
		free( pval );
	}

	/*
	**	If this is not going to the terminal, pick up the name
	**	of the log file, maximum log size, and the name of the
	**	lock file (if it is specified).
	*/
	if( !( DebugFlags & D_TERMLOG) ) {
		(void)sprintf(pname, "%s_LOG", subsys);
		if( DebugFile ) {
			free( DebugFile );
		}
		DebugFile = param(pname);

		if( DebugFile == NULL ) {
			EXCEPT("No '%s' parameter specified.", pname);
		}

		(void)sprintf(pname, "TRUNC_%s_LOG_ON_OPEN", subsys);
		pval = param(pname);
		if( pval ) {
			if( *pval == 't' || *pval == 'T' ) {
				want_truncate = 1;
			} 
			free(pval);
		}
		if( !did_truncate && want_truncate ) {
			DebugFP = open_debug_file("w");
			did_truncate = 1;
		} else {
			DebugFP = open_debug_file("a");
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
			free(pval);
		}

		if( MaxLog == 0 ) {
			MaxLog = 65536;
		}

		(void)sprintf(pname, "%s_LOCK", subsys);
		if (DebugLock) {
			free(DebugLock);
		}
		DebugLock = param(pname);
	} else {
		if( fileno(stderr) != logfd ) {
			if( dup2(fileno(stderr), logfd) < 0 ) {
				perror("Can't get error channel:");
				exit( 1 );
			}

#if defined(HPUX9)
			stderr->__fileL = logfd & 0xf0;	/* Low byte of fd */
			stderr->__fileH = logfd & 0x0f;	/* High byte of fd */
#elif defined(ULTRIX43) || defined(IRIX331) || defined(Solaris251) || defined(WIN32)
			((stderr)->_file) = logfd;
#elif defined(LINUX)
			stderr->_fileno = logfd;
#else
			fileno(stderr) = logfd;
#endif
		}

#if !defined(WIN32)
		setlinebuf( stderr );
#endif

		(void)fflush( stderr );	/* Don't know why we need this, but if not here
							   the first couple dprintf don't come out right */
	}
}

void
set_debug_flags( char *strflags )
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
