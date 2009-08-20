/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

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

extern "C" int		strcmp_until(const char *, const char *, const char);

ReliSock *qmgmt_sock = NULL;
static Qmgr_connection connection;

Qmgr_connection *
ConnectQ(const char *qmgr_location, int timeout, bool read_only, CondorError* errstack )
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
		dprintf(D_FULLDEBUG,"Failure getting my_username()\n");
		delete qmgmt_sock;
		qmgmt_sock = NULL;
		if (domain) free(domain);
		return( 0 );
	}

	/* Get the schedd to handle Q ops. */

    /* Get rid of all the code below */

    if (!qmgmt_sock->triedAuthentication()) {
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
            if (!SecMan::authenticate_sock(qmgmt_sock, CLIENT_PERM, errstack_select)) {
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
	CloseSocket();
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
SendSpoolFileBytes(char const *filename)
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
