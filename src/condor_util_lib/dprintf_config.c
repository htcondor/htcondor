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
#include "debug.h"
#include "except.h"
#include "condor_string.h"


int		Termlog = 0;

extern int		DebugFlags;
extern FILE		*DebugFP;
extern int		MaxLog[D_NUMLEVELS+1];
extern char		*DebugFile[D_NUMLEVELS+1];
extern char		*DebugLock;
extern char		*_condor_DebugFlagNames[];
extern int		_condor_dprintf_works;

extern void		_condor_set_debug_flags( char *strflags );

FILE *open_debug_file( int debug_level, char flags[] );

void
dprintf_config( subsys, logfd )
char *subsys;
int logfd;		/* logfd is the descriptor to use if the log output goes to a tty */
{
	char pname[ BUFSIZ ];
	char *pval, *param();
	static int first_time = 1;
	int want_truncate;
	int debug_level;

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
		_condor_set_debug_flags( pval );
		free( pval );
	}

	/*
	**	If this is not going to the terminal, pick up the name
	**	of the log file, maximum log size, and the name of the
	**	lock file (if it is specified).
	*/
	if( !( Termlog) ) {
		for (debug_level = 0; debug_level <= D_NUMLEVELS; debug_level++) {
			want_truncate = 0;
			if (debug_level == 0) {
				/*
				** the level 0 file gets all debug messages; thus, the
				** offset into DebugFlagNames is off by one, since the
				** first level-specific file goes into the other arrays at
				** index 1
				*/
				(void)sprintf(pname, "%s_LOG", subsys);
			} else {
				(void)sprintf(pname, "%s_%s_LOG", subsys,
							  _condor_DebugFlagNames[debug_level-1]+2);
			}
			if( DebugFile[debug_level] ) {
				free( DebugFile[debug_level] );
			}
			DebugFile[debug_level] = param(pname);

			if( debug_level == 0 && DebugFile[0] == NULL ) {
				EXCEPT("No '%s' parameter specified.", pname);
			} else if ( DebugFile[debug_level] != NULL ) {

				if (debug_level == 0) {
					(void)sprintf(pname, "TRUNC_%s_LOG_ON_OPEN", subsys);
				} else {
					(void)sprintf(pname, "TRUNC_%s_%s_LOG_ON_OPEN", subsys,
								  _condor_DebugFlagNames[debug_level-1]+2);
				}
				pval = param(pname);
				if( pval ) {
					if( *pval == 't' || *pval == 'T' ) {
						want_truncate = 1;
					} 
					free(pval);
				}
				if( first_time && want_truncate ) {
					DebugFP = open_debug_file(debug_level, "w");
				} else {
					DebugFP = open_debug_file(debug_level, "a");
				}

				if( DebugFP == NULL && debug_level == 0 ) {
					EXCEPT("Cannot open log file '%s'",
						   DebugFile[debug_level]);
				}

				if (DebugFP) (void)fclose( DebugFP );
				DebugFP = (FILE *)0;

				if (debug_level == 0) {
					(void)sprintf(pname, "MAX_%s_LOG", subsys);
				} else {
					(void)sprintf(pname, "MAX_%s_%s_LOG", subsys,
								  _condor_DebugFlagNames[debug_level-1]+2);
				}
				pval = param(pname);
				if( pval != NULL ) {
					MaxLog[debug_level] = atoi( pval );
					free(pval);
				} else {
					MaxLog[debug_level] = 65536;
				}

				if (debug_level == 0) {
					(void)sprintf(pname, "%s_LOCK", subsys);
					if (DebugLock) {
						free(DebugLock);
					}
					DebugLock = param(pname);
				}
			}
		}
	} else {

#if !defined(WIN32)
		setlinebuf( stderr );
#endif

		(void)fflush( stderr );	/* Don't know why we need this, but if not here
							   the first couple dprintf don't come out right */
	}

	first_time = 0;
	_condor_dprintf_works = 1;
}

