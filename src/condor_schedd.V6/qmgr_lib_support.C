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

#include "condor_io.h"

#include "qmgr.h"
#include "condor_qmgr.h"
#include "condor_debug.h"
#include "condor_attributes.h"
#include "condor_classad.h"
#include "daemon.h"
#include "my_hostname.h"
#include "my_username.h"
#include "get_daemon_addr.h"

int open_url(char *, int, int);
extern "C" char* get_schedd_addr(const char*, const char*); 
int	strcmp_until(const char *, const char *, const char);

ReliSock *qmgmt_sock = NULL;
static Qmgr_connection connection;

int
strcmp_until( const char *s1, const char *s2, const char until ) {
   while ( ( *s1 || *s2 ) && *s1 != until && *s2 != until ) {

      if ( *s1 != *s2 ) {
         return( *s2 - *s1 );
      }
      s1++;
      s2++;
   }
   return( 0 );
}

Qmgr_connection *
ConnectQ(char *qmgr_location, int timeout, bool read_only )
{
	int		rval, ok, is_local = FALSE;
	char*	scheddAddr = get_schedd_addr(0);
	char*	localScheddAddr = NULL;

	if( scheddAddr ) {
		localScheddAddr = strdup( scheddAddr );
		scheddAddr = NULL;
	} 

		// get the address of the schedd to which we want a connection
	if( !qmgr_location || !*qmgr_location ) {
			/* No schedd identified --- use local schedd */
		scheddAddr = localScheddAddr;
		is_local = TRUE;
	} else if(qmgr_location[0] != '<') {
			/* Get schedd's IP address from collector */
		scheddAddr = get_schedd_addr(qmgr_location);
	} else {
			/* We were passed the sinful string already */
		scheddAddr = qmgr_location;
	}


		// do we already have a connection active?
	if( qmgmt_sock ) {
			// yes; reject new connection (we can only handle one at a time)
		if( localScheddAddr ) free( localScheddAddr );
		return( NULL );
	}

    // no connection active as of now; create a new one
	if(scheddAddr) {
        Daemon d (scheddAddr);
        qmgmt_sock = (ReliSock*) d.startCommand (QMGMT_CMD, Stream::reli_sock, timeout);
        ok = (int)qmgmt_sock;
        if( !ok ) {
            dprintf(D_ALWAYS, "Can't connect to queue manager\n");
        }
	} else {
		ok = FALSE;
		if( qmgr_location ) {
			dprintf( D_ALWAYS, "Can't find address of queue manager %s\n", 
					 qmgr_location );
		} else {
			dprintf( D_ALWAYS, "Can't find address of local queue manager\n" );
		}
	}

	if( !ok ) {
		if ( localScheddAddr ) free(localScheddAddr);
		if( qmgmt_sock ) delete qmgmt_sock;
		qmgmt_sock = NULL;
		return 0;
	}

		/* Figure out if we're trying to connect to a remote queue, in
		   which case we'll set our username to "nobody" */
	if( localScheddAddr ) {
		//mju replaced strcmp with new method that strcmp until char (:)
		//so that we don't worry about the port number
		if( ! is_local && ! strcmp_until(localScheddAddr, scheddAddr, ':' ) ) {
			is_local = TRUE;
		}
		else {
			dprintf(D_FULLDEBUG,"ConnectQ failed on check for localScheddAddr\n" );
		}
		free(localScheddAddr);
	}

    // This could be a problem
	char *username = my_username();

	if ( !username ) {
		dprintf(D_FULLDEBUG,"Failure getting my_username()\n", username );
		delete qmgmt_sock;
		qmgmt_sock = NULL;
		return( 0 );
	}

    // This is really stupid. Why are we authenticate first and 
    // then reauthenticate again? Looks like the schedd is calling
    // Q_SOCK->unAuthenticate(), which forces the client to 
    // reauthenticate. Have to fix this.  Hao 2/2002

    // Okay, let's fix it. Hao
    qmgmt_sock->setOwner( username );

	//dprintf(D_FULLDEBUG,"Connecting to queue as user \"%s\"\n", username );

	/* Get the schedd to handle Q ops. */

    /* Get rid of all the code below */

    if (!qmgmt_sock->isAuthenticated()) {
        if ( read_only ) {
            rval = InitializeReadOnlyConnection( username );
        } else {
            rval = InitializeConnection( username );
        }

        if (rval < 0) {
            delete qmgmt_sock;
            qmgmt_sock = NULL;
            return 0;
        }

        if ( !read_only ) {
            if (!qmgmt_sock->authenticate()) {
                delete qmgmt_sock;
                qmgmt_sock = NULL;
                return 0;
            }
        }
    }

	free( username );

	return &connection;
}


// we can ignore the parameter because there is only one connection
bool
DisconnectQ(Qmgr_connection *)
{
	int rval;

	if( !qmgmt_sock ) return( false );
	rval = CloseConnection();
	delete qmgmt_sock;
	qmgmt_sock = NULL;
	return( rval >= 0 );
}


void
FreeJobAd(ClassAd *&ad)
{
	delete ad;
	ad = NULL;
}

int
SendSpoolFileBytes(char *filename)
{
	qmgmt_sock->encode();
	if (qmgmt_sock->put_file(filename) < 0) {		
		return -1;
	}

	return 0;

}


void
WalkJobQueue(scan_func func)
{
	ClassAd *ad;
	int rval = 0;

	ad = GetNextJob(1);
	while (ad != NULL && rval >= 0) {
		rval = func(ad);
		if (rval >= 0) {
			FreeJobAd(ad);
			ad = GetNextJob(0);
		}
	} 
	if (ad != NULL)
		FreeJobAd(ad);
}


int
rusage_to_float(struct rusage ru, float *utime, float *stime )
{
	if ( utime )
		*utime = (float) ru.ru_utime.tv_sec;

	if ( stime )
		*stime = (float) ru.ru_stime.tv_sec;

	return 0;
}

int
float_to_rusage(float utime, float stime, struct rusage *ru)
{
	ru->ru_utime.tv_sec = (time_t)utime;
	ru->ru_stime.tv_sec = (time_t)stime;
	ru->ru_utime.tv_usec = 0;
	ru->ru_stime.tv_usec = 0;
	return 0;
}

// I looked at this code for a while today, trying to figure out how
// to get GetAttributeString() to create a new string of the correct
// size so that we can get any size environment. Then I realized that
// no one calls this code. shadow_common.c now contains MakeProc() which 
// replaces this code. Thefore, I have #if-ed it out, to help other
// people avoid similar confusion. 
// Sincerely,
// Alain Roy, 30-Oct-2001
#if 0
#if !defined(WIN32)
int
GetProc(int cl, int pr, PROC *p)
{
	int disconn_when_done = 0;
	char buf[ATTRLIST_MAX_EXPRESSION];
	float	utime,stime;
	char	*s;

	if (!qmgmt_sock) {
		disconn_when_done = 1;
		if( !ConnectQ(0) ) {
			return -1;
		}
	}

	p->version_num = 3;
	p->id.cluster = cl;
	p->id.proc = pr;

	GetAttributeInt(cl, pr, ATTR_JOB_UNIVERSE, &(p->universe));
	GetAttributeInt(cl, pr, ATTR_WANT_CHECKPOINT, &(p->checkpoint));
	GetAttributeInt(cl, pr, ATTR_WANT_REMOTE_SYSCALLS, &(p->remote_syscalls));
	GetAttributeString(cl, pr, ATTR_OWNER, buf);
	p->owner = strdup(buf);
	GetAttributeInt(cl, pr, ATTR_Q_DATE, &(p->q_date));
	GetAttributeInt(cl, pr, ATTR_COMPLETION_DATE, &(p->completion_date));
	GetAttributeInt(cl, pr, ATTR_JOB_STATUS, &(p->status));
	GetAttributeInt(cl, pr, ATTR_JOB_PRIO, &(p->prio));
	GetAttributeInt(cl, pr, ATTR_JOB_NOTIFICATION, &(p->notification));
	GetAttributeInt(cl, pr, ATTR_IMAGE_SIZE, &(p->image_size));
	GetAttributeString(cl, pr, ATTR_JOB_ENVIRONMENT, buf);
	p->env = strdup(buf);

	p->n_cmds = 1;
	p->cmd = (char **) malloc(p->n_cmds * sizeof(char *));
	p->args = (char **) malloc(p->n_cmds * sizeof(char *));
	p->in = (char **) malloc(p->n_cmds * sizeof(char *));
	p->out = (char **) malloc(p->n_cmds * sizeof(char *));
	p->err = (char **) malloc(p->n_cmds * sizeof(char *));
	p->exit_status = (int *) malloc(p->n_cmds * sizeof(int));

	GetAttributeString(cl, pr, ATTR_JOB_CMD, buf);
	p->cmd[0] = strdup(buf);
	GetAttributeString(cl, pr, ATTR_JOB_ARGUMENTS, buf);
	p->args[0] = strdup(buf);
	GetAttributeString(cl, pr, ATTR_JOB_INPUT, buf);
	p->in[0] = strdup(buf);
	GetAttributeString(cl, pr, ATTR_JOB_OUTPUT, buf);
	p->out[0] = strdup(buf);
	GetAttributeString(cl, pr, ATTR_JOB_ERROR, buf);
	p->err[0] = strdup(buf);
	GetAttributeInt(cl, pr, ATTR_JOB_EXIT_STATUS, &(p->exit_status[0]));

	GetAttributeInt(cl, pr, ATTR_MIN_HOSTS, &(p->min_needed));
	GetAttributeInt(cl, pr, ATTR_MAX_HOSTS, &(p->max_needed));

	GetAttributeString(cl, pr, ATTR_JOB_ROOT_DIR, buf);
	p->rootdir = strdup(buf);
	GetAttributeString(cl, pr, ATTR_JOB_IWD, buf);
	p->iwd = strdup(buf);
	GetAttributeExpr(cl, pr, ATTR_REQUIREMENTS, buf);
	s = strchr(buf, '=');
	if (s) {
		s++;
		p->requirements = strdup(s);
	} 
	else {
		p->requirements = strdup(buf);
	}
	GetAttributeExpr(cl, pr, ATTR_PREFERENCES, buf);
	s = strchr(buf, '=');
	if (s) {
		s++;
		p->preferences = strdup(s);
	} 
	else {
		p->preferences = strdup(buf);
	}

	GetAttributeFloat(cl, pr, ATTR_JOB_LOCAL_USER_CPU, &utime);
	GetAttributeFloat(cl, pr, ATTR_JOB_LOCAL_SYS_CPU, &stime);
	float_to_rusage(utime, stime, &(p->local_usage));

	p->remote_usage = (struct rusage *) malloc(p->n_cmds * 
		sizeof(struct rusage));

	memset(p->remote_usage, 0, sizeof( struct rusage ));

	GetAttributeFloat(cl, pr, ATTR_JOB_REMOTE_USER_CPU, &utime);
	GetAttributeFloat(cl, pr, ATTR_JOB_REMOTE_SYS_CPU, &stime);
	float_to_rusage(utime, stime, &(p->remote_usage[0]));
	

	if (disconn_when_done) {
		DisconnectQ(&connection);
	}
	return 0;
}
#endif
#endif
