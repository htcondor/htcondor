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


/* 
   This file contains special stubs from the CEDAR library (ReliSock,
   SafeSock, etc) that are needed to make things link correctly within
   a user job that can't use their normal functionality.
*/

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_io.h"
#include "authentication.h"


int
ReliSock::get_file( filesize_t *size, const char *destination, bool flush_buffers)
{
	EXCEPT( "ReliSock::get_file() should never be "
			"called within the Condor syscall library" );
	return FALSE;
}

int
ReliSock::get_file( filesize_t *size, int fd, bool flush_buffers )
{
	EXCEPT( "ReliSock::get_file() should never be "
			"called within the Condor syscall library" );
	return FALSE;
}

int
ReliSock::put_empty_file( filesize_t *size )
{
	EXCEPT( "ReliSock::put_empty_file() should never be "
			"called within the Condor syscall library" );
	return FALSE;
}

int
ReliSock::put_file( filesize_t *size, const char *source)
{
	EXCEPT( "ReliSock::put_file() should never be "
			"called within the Condor syscall library" );
	return FALSE;
}

int
ReliSock::put_file( filesize_t *size, int fd )
{
	EXCEPT( "ReliSock::put_file() should never be "
			"called within the Condor syscall library" );
	return FALSE;
}

int
ReliSock::get_file_with_permissions( filesize_t*, const char *, bool )
{
	EXCEPT( "ReliSock::get_file_with_permissions() should never be "
			"called within the Condor syscall library" );
	return FALSE;
}


int
ReliSock::put_file_with_permissions( filesize_t *, const char * )
{
	EXCEPT( "ReliSock::put_file_with_permissions() should never be "
			"called within the Condor syscall library" );
	return FALSE;
}

int
ReliSock::get_x509_delegation( filesize_t*, const char *, bool )
{
	EXCEPT( "ReliSock::get_x509_delegation() should never be "
			"called within the Condor syscall library" );
	return FALSE;
}


int
ReliSock::put_x509_delegation( filesize_t *, const char * )
{
	EXCEPT( "ReliSock::put_x509_delegation() should never be "
			"called within the Condor syscall library" );
	return FALSE;
}

void Authentication::split_canonical_name(char const *can_name,char **user,char **domain) {
	EXCEPT( "Authentication::split_canonical_name() should never be "
			"called within the Condor syscall library" );
}

void
Stream::prepare_crypto_for_secret()
{
	EXCEPT( "Stream::prepare_crypto_for_secret() should never be "
			"called within the Condor syscall library" );
}

void
Stream::restore_crypto_after_secret()
{
	EXCEPT( "Stream::restore_crypto_after_secret() should never be "
			"called within the Condor syscall library" );
}

bool
Stream::prepare_crypto_for_secret_is_noop()
{
	EXCEPT( "Stream::prepare_crypto_for_secret_is_noop() should never be "
			"called within the Condor syscall library" );
	return true;
}

int Sock::special_connect(char const *,int,bool)
{
	return CEDAR_ENOCCB;
}

int
SafeSock::do_reverse_connect(char const *,bool)
{
	return CEDAR_ENOCCB;
}

int
ReliSock::do_reverse_connect(char const *,bool)
{
	return CEDAR_ENOCCB;
}

void
ReliSock::cancel_reverse_connect() {
	EXCEPT( "ReliSock::cancel_reverse_connect() should never be "
			"called within the Condor syscall library" );
}

int
SafeSock::do_shared_port_local_connect( char const *,bool )
{
	return 0;
}

int
ReliSock::do_shared_port_local_connect( char const *,bool )
{
	return 0;
}

bool
ReliSock::sendTargetSharedPortID()
{
	return true;
}

char const *
Sock::get_sinful_public()
{
	return get_sinful();
}
