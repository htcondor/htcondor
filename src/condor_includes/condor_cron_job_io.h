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

#ifndef _CONDOR_CRON_JOB_IO_H
#define _CONDOR_CRON_JOB_IO_H

#include "condor_common.h"
#include "linebuffer.h"
#include <queue>

// Pre-declare the CronJob class
class CronJob;

// Cron's I/O base class
class CronJobIO : public LineBuffer
{
  public:
	CronJobIO( CronJob &job, unsigned buf_size );
	virtual ~CronJobIO( void ) {};
	virtual int Output( const char *buf, int len ) = 0;

  protected:
	CronJob			&m_job;
};

// Cron's StdOut Line Buffer
class CronJobOut : public CronJobIO
{
  public:
	CronJobOut( CronJob &job );
	virtual ~CronJobOut( void ) {};
	virtual int Output( const char *buf, int len );
	int GetQueueSize( void );
	const char * GetQueueSep( void ) { return m_q_sep.c_str(); }
	char *GetLineFromQueue( void );
	int FlushQueue( void );
  private:
	std::queue<char *> m_lineq;
	MyString m_q_sep; // when record separator '-' is read from the stream, this holds that line with the '-'
};

// Cron's StdErr Line Buffer
class CronJobErr : public CronJobIO
{
  public:
	CronJobErr( CronJob &job );
	virtual ~CronJobErr( void ) {};
	virtual int Output( const char *buf, int len );
};

#endif /* _CONDOR_CRON_JOB_IO_H */
