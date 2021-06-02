/***************************************************************
 *
 * Copyright (C) 1990-2010, Condor Team, Computer Sciences Department,
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
#include <limits.h>
#include <string.h>
#include "condor_debug.h"
#include "condor_daemon_core.h"
#include "condor_cron_job.h"
#include "condor_cron_job_io.h"

// Size of the buffer for reading from the child process
#define STDOUT_LINEBUF_SIZE	(64*1024)
#define STDERR_LINEBUF_SIZE	1024

// Cron's Line I/O base class constructor
CronJobIO::CronJobIO( class CronJob &job, unsigned buf_size )
		: LineBuffer( buf_size ),
		  m_job( job )
{
}

// Cron's Line StdOut Buffer constructor
CronJobOut::CronJobOut( class CronJob &job )
		: CronJobIO( job, STDOUT_LINEBUF_SIZE )
{
}

// Output function
int
CronJobOut::Output( const char *buf, int len )
{
	// Ignore empty lines
	if ( 0 == len ) {
		return 0;
	}

	// Check for record delimitter
	if ( '-' == buf[0] ) {
		if (buf[1]) {
			m_q_sep = &buf[1];
			m_q_sep.trim();
		}
		return 1;
	}

	// Build up the string
	const char	*prefix = m_job.GetPrefix( );
	size_t fulllen = len;
	if ( prefix ) {
		fulllen += strlen( prefix );
	}
	char	*line = (char *) malloc( fulllen + 1 );
	if ( NULL == line ) {
		dprintf( D_ALWAYS,
				 "cronjob: Unable to duplicate %d bytes\n",
				 (int)fulllen );
		return -1;
	}
	if ( prefix ) {
		strcpy( line, prefix );
	} else {
		*line = '\0';
	}
	strcat( line, buf );

	// Queue it up, get out
	m_lineq.push( line );

	// Done
	return 0;
}

// Get size of the queue
int
CronJobOut::GetQueueSize( void )
{
	return m_lineq.size( );
}

// Flush the queue
int
CronJobOut::FlushQueue( void )
{
	int		size = m_lineq.size( );

	// Flush out the queue
	while( m_lineq.size() ) {
		free( m_lineq.front() );
		m_lineq.pop();
	}
	m_q_sep.clear();

	// Return the size
	return size;
}

// Get next queue element
char *
CronJobOut::GetLineFromQueue( void )
{
	char	*line;

	if ( m_lineq.size() ) {
		line = m_lineq.front();
		m_lineq.pop();
		return line;
	} else {
		m_q_sep.clear();
		return NULL;
	}
}

CronJobErr::CronJobErr( class CronJob &job )
		: CronJobIO( job, STDERR_LINEBUF_SIZE )
{
}

// StdErr Output function
int
CronJobErr::Output( const char *buf, int   /*len*/ )
{
	dprintf( D_FULLDEBUG, "%s: %s\n", m_job.GetName( ), buf );

	// Done
	return 0;
}
