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

int open_url(char *, int, int);
extern "C" int		strcmp_until(const char *, const char *, const char);

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
ConnectQ(char *qmgr_location, int timeout, bool read_only, CondorError* errstack )
{
	int		rval, ok;

		// do we already have a connection active?
	if( qmgmt_sock ) {
			// yes; reject new connection (we can only handle one at a time)
		return( NULL );
	}

	// set up the error handling so it will clean up automatically on
	// return.  also allow them to specify their own stack.
	CondorError  our_errstack;
	CondorError* errstack_select = &our_errstack;
	if (errstack) {
		errstack_select = errstack;
	}

    // no connection active as of now; create a new one
	Daemon d( DT_SCHEDD, qmgr_location );
	if( ! d.locate() ) {
		ok = FALSE;
		if( qmgr_location ) {
			dprintf( D_ALWAYS, "Can't find address of queue manager %s\n", 
					 qmgr_location );
		} else {
			dprintf( D_ALWAYS, "Can't find address of local queue manager\n" );
		}
	} else { 
		qmgmt_sock = (ReliSock*) d.startCommand( QMGMT_CMD, 
												 Stream::reli_sock,
												 timeout,
												 errstack_select);
		ok = qmgmt_sock != NULL;
		if( !ok && !errstack) {
			dprintf(D_ALWAYS, "Can't connect to queue manager: %s\n",
					errstack_select->getFullText() );
		}
	}

	if( !ok ) {
		if( qmgmt_sock ) delete qmgmt_sock;
		qmgmt_sock = NULL;
		return 0;
	}


    // This could be a problem
	char *username = my_username();
	char *domain = my_domainname();

	if ( !username ) {
		dprintf(D_FULLDEBUG,"Failure getting my_username()\n", username );
		delete qmgmt_sock;
		qmgmt_sock = NULL;
		if (domain) free(domain);
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
            rval = InitializeConnection( username, domain );
        }

		if (username) {
			free(username);
			username = NULL;
		}
		if (domain) {
			free(domain);
			domain = NULL;
		}

        if (rval < 0) {
            delete qmgmt_sock;
            qmgmt_sock = NULL;
            return 0;
        }

        if ( !read_only ) {
			char * p = SecMan::getSecSetting ("SEC_%s_AUTHENTICATION_METHODS", "CLIENT");
			MyString methods;
			if (p) {
				methods = p;
				free(p);
			} else {
				methods = SecMan::getDefaultAuthenticationMethods();
			}

            if (!qmgmt_sock->authenticate(methods.Value(), errstack_select)) {
                delete qmgmt_sock;
                qmgmt_sock = NULL;
				if (!errstack) {
					dprintf( D_ALWAYS, "Authentication Error: %s\n",
							 errstack_select->getFullText() );
				}
                return 0;
            }
        }
    }

	if (username) free(username);
	if (domain) free(domain);

	return &connection;
}


// we can ignore the parameter because there is only one connection
bool
DisconnectQ(Qmgr_connection *,bool commit_transactions)
{
	int rval = -1;

	if( !qmgmt_sock ) return( false );
	if ( commit_transactions ) {
		rval = CloseConnection();
	}
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
	filesize_t	size;
	qmgmt_sock->encode();
	if (qmgmt_sock->put_file(&size, filename) < 0) {		
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
