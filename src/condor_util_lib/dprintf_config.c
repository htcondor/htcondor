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


/************************************************************************
**
**	Set up the various dprintf variables based on the configuration file.
**
************************************************************************/

#include "condor_common.h"
#include "condor_debug.h"
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
extern void		_condor_dprintf_saved_lines( void );

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
	** First, add the debug flags that are shared by everyone.
	*/
	pval = param("ALL_DEBUG");
	if( pval ) {
		_condor_set_debug_flags( pval );
		free( pval );
	}

	/*
	**  Then, pick up the subsys_DEBUG parameters.   Note: if we don't have
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
	_condor_dprintf_saved_lines();
}

