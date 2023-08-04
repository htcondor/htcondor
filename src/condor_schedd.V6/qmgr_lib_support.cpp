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

ReliSock *qmgmt_sock = nullptr;
static Qmgr_connection connection;

Qmgr_connection *
ConnectQ(DCSchedd& schedd, int timeout, bool read_only, CondorError* errstack, const char *effective_owner)
{
	bool ok = false;
	int cmd = read_only ? QMGMT_READ_CMD : QMGMT_WRITE_CMD;

		// do we already have a connection active?
	if( qmgmt_sock ) {
			// yes; reject new connection (we can only handle one at a time)
		return( nullptr );
	}

	// set up the error handling so it will clean up automatically on
	// return.  also allow them to specify their own stack.
	CondorError  our_errstack;
	CondorError* errstack_select = &our_errstack;
	if (errstack) {
		errstack_select = errstack;
	}

    // no connection active as of now; create a new one
	if( ! schedd.locate() ) {
		ok = false;
		dprintf( D_ALWAYS, "Can't find address of queue manager\n" );
	} else { 
		qmgmt_sock = dynamic_cast<ReliSock*>( schedd.startCommand( cmd,
												 Stream::reli_sock,
												 timeout,
												 errstack_select));
		ok = qmgmt_sock != nullptr;
		if( !ok && !errstack) {
			dprintf(D_ALWAYS, "Can't connect to queue manager: %s\n",
					errstack_select->getFullText().c_str() );
		}
	}

	if( !ok ) {
		if( qmgmt_sock ) delete qmgmt_sock;
		qmgmt_sock = nullptr;
		return nullptr;
	}

		// If security negotiation is turned off and we are using WRITE_CMD,
		// then we must force authentication now, before trying to initialize
		// the connection, because this command is registered with
		// force_authentication=true on the server side.
	if( cmd == QMGMT_WRITE_CMD && !qmgmt_sock->triedAuthentication()) {
		if( !SecMan::authenticate_sock(qmgmt_sock, CLIENT_PERM, errstack_select ) )
		{
			delete qmgmt_sock;
			qmgmt_sock = nullptr;
			if (!errstack) {
				dprintf( D_ALWAYS, "Authentication Error: %s\n",
						 errstack_select->getFullText().c_str() );
			}
			return nullptr;
		}
	}

	/* Get the schedd to handle Q ops. */

	if( effective_owner && *effective_owner ) {
		if( QmgmtSetEffectiveOwner( effective_owner ) != 0 ) {
			if( errstack ) {
				errstack->pushf(
					"Qmgmt",SCHEDD_ERR_SET_EFFECTIVE_OWNER_FAILED,
					"SetEffectiveOwner(%s) failed with errno=%d: %s.",
					effective_owner, errno, strerror(errno) );
			}
			else {
				dprintf( D_ALWAYS,
						 "SetEffectiveOwner(%s) failed with errno=%d: %s.\n",
						 effective_owner, errno, strerror(errno) );
			}
			delete qmgmt_sock;
			qmgmt_sock = nullptr;
			return nullptr;
		}
	}

	return &connection;
}


// we can ignore the parameter because there is only one connection
bool
DisconnectQ(Qmgr_connection *,bool commit_transactions, CondorError *errstack)
{
	int rval = -1;

	if( !qmgmt_sock ) return( false );
	if ( commit_transactions ) {
		rval = RemoteCommitTransaction(0, errstack);
	}
	CloseSocket();
	delete qmgmt_sock;
	qmgmt_sock = nullptr;
	return( rval >= 0 );
}


void
FreeJobAd(ClassAd *&ad)
{
	delete ad;
	ad = nullptr;
}

int
SendSpoolFileBytes(char const *filename)
{
	filesize_t	size = 0;
	qmgmt_sock->encode();
	if (qmgmt_sock->put_file(&size, filename) < 0) {		
		return -1;
	}

	return 0;

}


void
WalkJobQueue2(scan_func func, void* pv)
{
	ClassAd *ad = nullptr;
	int rval = 0;

	ad = GetNextJob(1);
	while (ad != nullptr && rval >= 0) {
		rval = func(ad, pv);
		if (rval >= 0) {
			FreeJobAd(ad);
			ad = GetNextJob(0);
		}
	} 
	if (ad != nullptr)
		FreeJobAd(ad);
}


int
rusage_to_float(const struct rusage &ru, double *utime, double *stime )
{
	if ( utime )
		*utime = (double) ru.ru_utime.tv_sec;

	if ( stime )
		*stime = (double) ru.ru_stime.tv_sec;

	return 0;
}

int
float_to_rusage(double utime, double stime, struct rusage *ru)
{
	ru->ru_utime.tv_sec = (time_t)utime;
	ru->ru_stime.tv_sec = (time_t)stime;
	ru->ru_utime.tv_usec = 0;
	ru->ru_stime.tv_usec = 0;
	return 0;
}

