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
#include "mpi_master_proc.h"
#include "NTsenders.h"
#include "condor_attributes.h"
#include "condor_string.h"  // for strnewp
#include "my_hostname.h"

extern int main_shutdown_graceful();

MPIMasterProc::MPIMasterProc( ClassAd * jobAd ) : MPIComradeProc( jobAd )
{
#if ! MPI_USES_RSH
	port = 0;
	port_check_tid = -1;
	port_file = NULL;
	num_port_file_opens = 0;
		// Should we make either of these config file knobs?
	max_port_file_opens = 10;
	port_check_interval = 2;
#endif
    dprintf ( D_FULLDEBUG, "Constructor of MPIMasterProc::MPIMasterProc\n" );
    dprintf ( D_PROTOCOL, "#4 - MPI MasterProc Started.\n" );
}

MPIMasterProc::~MPIMasterProc() {}


#if ! MPI_USES_RSH
// We only need our own version of this function if we're not using
// rsh and have a port file to clean up.
int 
MPIMasterProc::JobCleanup( int pid, int status )
{ 
	dprintf(D_FULLDEBUG,"in MPIMasterProc::JobCleanup()\n");

		// First, we've got to clean up the mpi port file, so that
		// doesn't get transfered back to the user.
	if( port_file ) {
		priv_state priv;
		priv = set_user_priv();
		if( unlink( port_file ) < 0 ) {
			dprintf( D_ALWAYS, "unlink(%s) failed: %s\n", port_file,
					 strerror(errno) ); 
		}
		set_priv( priv );
	}

		// Now, just let our parent versions of this function do their
		// magic. 
    return MPIComradeProc::JobCleanup( pid, status );
}
#endif /* ! MPI_USES_RSH */


int
MPIMasterProc::StartJob()
{
	int rval;

    dprintf ( D_FULLDEBUG, "MPIMasterProc::StartJob()\n" );

#if MPI_USES_RSH

    if ( !alterEnv() ) {
        return FALSE;
    }
    dprintf ( D_PROTOCOL, "#5 - altered job environment, starting master:\n" );

#else

	preparePortFile();
    dprintf ( D_PROTOCOL, "#5 - created port file, starting master:\n" );

#endif /* ! MPI_USES_RSH */

        // The master starts like the comrades...
	rval = MPIComradeProc::StartJob();

#if ! MPI_USES_RSH
		// now, we've got to check for the port file, grab the port,
		// and send that back to the shadow as a pseudo syscall so it
		// can spawn the rest of the workers.  If the port file isn't
		// yet ready, this function will set a timer so we keep
		// checking until we find it.
	checkPortFile();
#endif 

	return rval;
}


#if MPI_USES_RSH

int
MPIMasterProc::alterEnv()
{
/* This routine is here in the starter because only here do we know 
   if there is a PATH environment variable set.  This is important
   if there is no Env in the JobAd, or there is no PATH in said
   Env.  In this case, we get the path from the local environment
   and prepend stuff to it - we DON'T want to clobber the path
   totally (which is what we'd do if we set this PATH back in 
   the shadow....). */

/* task:  First, see if there's a PATH var. in the JobAd->env.  
   If there is, alter it.  If there isn't, insert one. */
    
    dprintf ( D_FULLDEBUG, "MPIMasterProc::alterPath()\n" );

    char *tmp;
	char env[_POSIX_ARG_MAX];
	if ( !JobAd->LookupString( ATTR_JOB_ENVIRONMENT, env )) {
		dprintf( D_ALWAYS, "%s not found in JobAd.  Aborting.\n", 
				 ATTR_JOB_ENVIRONMENT );
		return 0;
	}

    char *condor_rsh = param( "MPI_CONDOR_RSH_PATH" );
    if ( !condor_rsh ) {
        dprintf ( D_ALWAYS, "Can't find MPI_CONDOR_RSH_PATH "
                  "in config file! Aborting!\n" ); 
        return 0;
    }

    char path[_POSIX_ARG_MAX];
    if ( (env[0] == '\0') ||
         (!strstr( env, "PATH" ) ) ) {
            // User did not specify any env, or there is no 'PATH'
            // in env sent along.  We get $PATH and alter it.

        tmp = getenv( "PATH" );
        if ( tmp ) {
            dprintf ( D_FULLDEBUG, "No Path in ad, $PATH in env\n" );
            dprintf ( D_FULLDEBUG, "before: %s\n", tmp );
            sprintf ( path, "PATH=%s:%s", condor_rsh, tmp );
        }
        else {   // no PATH in env.  Make one.
            dprintf ( D_FULLDEBUG, "No Path in ad, no $PATH in env\n" );
            sprintf ( path, "PATH=%s", condor_rsh );
        }

        if ( env[0] == '\0' ) {
            sprintf ( env, "%s = \"%s", ATTR_JOB_ENVIRONMENT, path );
        } else {
            char bar[4096];
            sprintf ( bar, "%s = \"%s;%s", ATTR_JOB_ENVIRONMENT, env, path );
            strcpy( env, bar );
        }
    }
    else {
            // The user gave us a path in env.  Find & alter:
        dprintf ( D_FULLDEBUG, "$PATH in ad...env:\n" );
        dprintf ( D_FULLDEBUG, "%s\n", env );
        
        /* The env. is a ';' delimited list, the elements within 
           the path are ':' delimited.  */

        char *tok;
        int n = 0;
        tmp = strnewp( env );
        memset( env, 0, _POSIX_ARG_MAX );

        n += sprintf ( env, "%s = \"", ATTR_JOB_ENVIRONMENT );

        tok = strtok( tmp, ";" );
        while ( tok ) {
            if ( !strncmp( tok, "PATH=", 5 ) ) {
                    // match!
                n += sprintf( &env[n], "PATH=%s:%s;", condor_rsh, &tok[5] );
            }
            else {  // not PATH, stick back in...
                n += sprintf( &env[n], "%s;", tok );
            }
            tok = strtok( NULL, ";" );
        }

        delete [] tmp;
    }
    
        /* We want to add one little thing to the environment:
           We want to put the var MPI_MY_ADDRESS in here, 
           so condor_rsh can find it.  In this context, "MY"
		   referrs to the shadow.  Really. */
    char shad[128], foo[128];
    shad[0] = foo[0] = 0;
	if ( JobAd->LookupString( ATTR_MY_ADDRESS, shad ) < 1 ) {
		dprintf( D_ALWAYS, "%s not found in JobAd.  Aborting.\n", 
				 ATTR_MY_ADDRESS );
		return 0;
	}

    sprintf( foo, ";MPI_SHADOW_SINFUL=%s", shad );
    strcat( env, foo );

		// In case the user job is linked with a newer version of
		// MPICH that honors the P4_RSHCOMMAND env var, let's set
		// that, too.
    sprintf( foo, ";P4_RSHCOMMAND=%s/rsh\"", condor_rsh );
    strcat( env, foo );

        // now put the env back into the JobAd:
    dprintf ( D_FULLDEBUG, "%s\n", env );
	if ( !JobAd->InsertOrUpdate( env ) ) {
		dprintf( D_ALWAYS, "Unable to update env! Aborting.\n" );
		return 0;
	}

    return 1;
}

#else /* ! MPI_USES_RSH */

bool
MPIMasterProc::preparePortFile( void )
{
	int fd;

		// First, construct the appropriate filename to use.  Luckily
		// for us here, the temporary execute directory has already
		// been created, and it's already our cwd.  So, we can just
		// use a filename of our choosing, and it'll already be unique
		// across all VMs, it'll automatically get deleted, etc.
	port_file = strdup( "mpi_port_file" );

		// To create this file, we've got to be in the user's priv
		// state. 
	priv_state priv;
	priv = set_user_priv();

		// For this stuff to work, we've got to create the file and
		// have it be 0 bytes.
	fd = open( port_file, O_WRONLY|O_CREAT|O_TRUNC, 0666 );
	if( fd < 0 ) {
		dprintf( D_ALWAYS, "ERROR: Can't create port file (%s)\n",
				 port_file );
		return false;
	}
		// if that worked, we can just close the file, since we don't
		// want to put anything in it, yet.
	close( fd );

		// We're done being the user...
	set_priv( priv );

		// Now that the file exists, we also need to hack the
		// environment of our job ad so we set the right env var so
		// MPICH puts the port in here.

		// Eventually, we're going to need to reinsert this as a
		// ClassAd attribute, so we might as well initialize it with 
		// 'ATTR_JOB_ENVIRONMENT = "'...
	char buf[1024];
	sprintf( buf, "%s = \"", ATTR_JOB_ENVIRONMENT );
	MyString env( buf );

		// Now, grab the existing environment.  Do so without static
		// buffers so we can handle really huge environments
		// correctly.  
	char* env_str = NULL;
	if( JobAd->LookupString(ATTR_JOB_ENVIRONMENT, &env_str) ) {
		env += env_str;
		free( env_str );
	} else {
			// Maybe this is a little harsh, but it should never 
			// happen.   
		EXCEPT( "MPI Master node started w/o an environment!" );
	}
	sprintf( buf, "%cMPICH_EXTRA=%s\"", env_delimiter, port_file );
	env += buf;

	if( JobAd->Insert(env.Value()) ) {
		return true;
	} 
	return false;
}



bool
MPIMasterProc::checkPortFile( void )
{
		// First, see if the file's got anything tasty in it. 
	FILE* fp;
	char buf[256];
	char* rval;

	if( ! port_file ) {
			// This is really bad, there's no filename for the port
			// file.  this really should NEVER happen, since we don't
			// call checkPortFile until after we've spawned the job,
			// and we *ALWAYS* at least have a value for the file name
			// as soon as we call preparePortFile()
		EXCEPT( "checkPortFile(): no port_file defined!" );
	}

	fp = fopen( port_file, "r" );
	if( fp ) {
			// check if there's anything there...
		rval = fgets( buf, 100, fp );
			// No matter what happened with the fgets(), we want to
			// close the file now, so we don't leak an FD
		fclose( fp );
		if( rval ) {
				// cool, we read something.  stash the port in our
				// local variable
			port = atoi( buf );

				// At this point, we can actually do our pseudo
				// syscall to tell the shadow.  First, create the
				// string we need and stuff it in a ClassAd 
			sprintf( buf, "%s=\"%s:%d\"", ATTR_MPI_MASTER_ADDR, 
					 inet_ntoa(*(my_sin_addr())), port );
			ClassAd ad;
			ad.Insert( buf );

				// Now, do the call:
			REMOTE_CONDOR_register_mpi_master_info( &ad );

				// clear our tid (since we're not going to reset the
				// timer) 
			port_check_tid = -1;

				// we're done.
			return true;
		}
	} else {
			// Couldn't even open the file
		num_port_file_opens++;
		if( num_port_file_opens >= max_port_file_opens ) {
			dprintf( D_ALWAYS, "ERROR: Can't open %s after %d attempts, "
					 "aborting", port_file, num_port_file_opens );
			main_shutdown_graceful();
		} else {
			dprintf( D_FULLDEBUG, "WARNING: Can't open %s, will try again\n", 
					 port_file ); 
		}
	}
				 
		// if we got here, either: we couldn't open the file (and
		// we're willing to keep trying), or we opened it and there
		// was nothing in there.  so, we need to set a timer to call
		// ourselves again in a few seconds...  if we've already done
		// this already, all we have to do is reset the timer.  
	if( port_check_tid >= 0 ) {
			// we've already got a timer, just reset it.
		daemonCore->Reset_Timer( port_check_tid, port_check_interval, 0 );
		return false;
	}
		// if we get here, it means this is our first time, and we
		// need to register our timer...
	port_check_tid = daemonCore->
		Register_Timer( port_check_interval, 0, 
						(TimerHandlercpp)&MPIMasterProc::checkPortFile,
						"MPIMasterProc::checkPortFile", this );
	if( port_check_tid < 0 ) {
		EXCEPT( "Can't register DaemonCore Timer!" );
	}
	return false;
}

#endif /* ! MPI_USES_RSH */
