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
#include "condor_config.h"
#include "stat_wrapper_internal.h"
#include "stat_wrapper.h"

// Ugly hack: stat64 prototyps are wacked on HPUX, so define them myself
// These are cut & pasted from the HPUX man pages...
#if defined( HPUX )
extern "C" {
	extern int stat64(const char *path, struct stat64 *buf);
	extern int lstat64(const char *path, struct stat64 *buf);
	extern int fstat64(int fildes, struct stat64 *buf);
}
#endif

// These are the actual stat functions that we'll use below
#if defined(HAVE_STAT64)
#  define STAT_FUNC stat64
   const char *STAT_NAME = "stat64";
#elif defined(HAVE__STATI64)
#  define STAT_FUNC _stati64
   const char *STAT_NAME = "_stati64";
#else
#  define STAT_FUNC stat
   const char *STAT_NAME = "stat";
#endif

// Ditto for lstat.  NOTE: We fall back to stat if there is no lstat
#if defined(HAVE_LSTAT64)
#  define LSTAT_FUNC lstat64
   const char *LSTAT_NAME = "lstat64";
#  define HAVE_AN_LSTAT
#elif defined(HAVE__LSTATI64)
#  define LSTAT_FUNC _lstati64
   const char *LSTAT_NAME = "_lstati64";
#  define HAVE_AN_LSTAT
#elif defined(HAVE_LSTAT)
#  define LSTAT_FUNC lstat
   const char *LSTAT_NAME = "lstat";
#  define HAVE_AN_LSTAT
#else
#  define LSTAT_FUNC STAT_FUNC
   const char *LSTAT_NAME = STAT_NAME;
#  undef HAVE_AN_LSTAT
#endif

// These are the actual fstat functions that we'll use below
#if defined(HAVE_FSTAT64)
#  define FSTAT_FUNC fstat64
   const char *FSTAT_NAME = "fstat64";
#elif defined(HAVE__FSTATI64)
#  define FSTAT_FUNC _fstati64
   const char *FSTAT_NAME = "_fstati64";
#else
#  define FSTAT_FUNC fstat
   const char *FSTAT_NAME = "fstat";
#endif


//
// Simple class to wrap which we should do
//
class StatWrapperOp
{
public:
	StatWrapperOp( StatWrapperIntBase	*stat,
				   StatWrapperIntBase	*lstat,
				   StatWrapperIntBase	*fstat,
				   StatWrapperIntBase	*primary )
		{
			m_stat   = stat;
			m_lstat  = lstat;
			m_fstat  = fstat;

			m_all[0] = stat;
			m_all[1] = lstat;
			m_all[2] = fstat;

			m_primary = primary;
		}
	~StatWrapperOp( void ) { };

	// Perform the actual stat operation
	int StatAll( bool force );

	StatWrapperIntBase	*getPrimary( void ) {
		return m_primary;
	}
	StatWrapperIntBase	**getAll( void ) {
		return m_all;
	}

private:
	StatWrapperIntBase	*m_stat;
	StatWrapperIntBase	*m_fstat;
	StatWrapperIntBase	*m_lstat;

	StatWrapperIntBase	*m_primary;
	StatWrapperIntBase	*m_all[3];
};

int
StatWrapperOp::StatAll( bool force )
{
	m_stat ->Stat( force );
	m_lstat->Stat( force );
	m_fstat->Stat( force );

	if ( m_stat ->GetRc( ) ) return m_stat ->GetRc();
	if ( m_lstat->GetRc( ) ) return m_lstat->GetRc();
	if ( m_fstat->GetRc( ) ) return m_fstat->GetRc();
	return 0;
}


//
// StatWrapper proper class methods
//
StatWrapper::StatWrapper( const MyString &path, StatOpType which )
{
	init( );

	if ( which != STATOP_NONE ) {
		(void) Stat( path, which, true );
	}
}

StatWrapper::StatWrapper( const char *path, StatOpType which )
{
	init( );
	m_stat ->SetPath( path );
	m_lstat->SetPath( path );

	if ( which != STATOP_NONE ) {
		(void) Stat( path, which, true );
	}
}

StatWrapper::StatWrapper( int fd, StatOpType which )
{
	init( );
	m_fstat->SetFD( fd );

	if ( which != STATOP_NONE ) {
		(void) Stat( fd, true );
	}
}

StatWrapper::StatWrapper( void )
{
	init( );
}

StatWrapper::~StatWrapper( void )
{
	for(int i=STATOP_MIN; i<=STATOP_MAX; i++ ) {
		if( m_ops[i] ) {
			delete m_ops[i];
		}
	}

	delete m_nop;
	delete m_stat;
	delete m_lstat;
	delete m_fstat;
}

bool
StatWrapper::IsInitialized( void ) const
{
	return m_stat->IsValid() || m_fstat->IsValid();
}

// Internal
void
StatWrapper::init( void )
{
	m_nop   = new StatWrapperIntNop(  NULL,       NULL );
	m_stat  = new StatWrapperIntPath( STAT_NAME,  STAT_FUNC  );
	m_lstat = new StatWrapperIntPath( LSTAT_NAME, LSTAT_FUNC );
	m_fstat = new StatWrapperIntFd(   FSTAT_NAME, FSTAT_FUNC );

	// Initialize the ops table
	//  Note: we use m_stat if we don't have a real lstat()
	memset( m_ops, 0, sizeof(m_ops) );
# ifdef HAVE_AN_LSTAT
	StatWrapperIntPath	*lstat = m_lstat;
# else
	StatWrapperIntPath	*lstat = m_stat;
# endif
	m_ops[STATOP_NONE]	= new StatWrapperOp( m_nop,  m_nop, m_nop,   m_nop );
	m_ops[STATOP_STAT]	= new StatWrapperOp( m_stat, m_nop, m_nop,   m_stat );
	m_ops[STATOP_LSTAT]	= new StatWrapperOp( m_nop,  lstat, m_nop,   m_lstat );
	m_ops[STATOP_BOTH]	= new StatWrapperOp( m_stat, lstat, m_nop,   m_nop );
	m_ops[STATOP_FSTAT]	= new StatWrapperOp( m_nop,  m_nop, m_fstat, m_fstat );
	m_ops[STATOP_ALL]	= new StatWrapperOp( m_stat, lstat, m_fstat, m_nop );
	m_ops[STATOP_LAST]	= new StatWrapperOp( m_nop,  m_nop, m_nop,   m_nop );

	m_last_op = m_ops[STATOP_NONE];
	m_last_which = STATOP_NONE;
}

// Set the path(s)
bool
StatWrapper::SetPath( const MyString &path )
{
	return SetPath( path.IsEmpty() ? NULL : path.Value());
}

// Set the path(s)
bool
StatWrapper::SetPath( const char *path )
{
	bool status = true;

	if ( !m_stat->SetPath( path ) ) {
		status = false;
	}
	if ( !m_lstat->SetPath( path ) ) {
		status = false;
	}

	return status;
}

bool
StatWrapper::SetFD( int fd )
{
	return m_fstat->SetFD( fd );
}

int
StatWrapper::GetRc( const StatWrapperIntBase *stat ) const
{
	if ( !stat ) {
		return -1;
	}
	return stat->GetRc( );
}

int
StatWrapper::GetErrno( const StatWrapperIntBase *stat ) const
{
	if ( !stat ) {
		return -1;
	}
	return stat->GetErrno( );
}

bool
StatWrapper::IsValid( const StatWrapperIntBase *stat ) const
{
	if ( !stat ) {
		return false;
	}
	return stat->IsValid( );
}

const char *
StatWrapper::GetStatFn( const StatWrapperIntBase *stat ) const
{
	if ( !stat ) {
		return NULL;
	}
	return stat->GetFnName( );
}

bool
StatWrapper::IsBufValid( const StatWrapperIntBase *stat ) const
{
	if ( !stat ) {
		return false;
	}
	return stat->IsBufValid( );
}

const StatStructType *
StatWrapper::GetBuf( const StatWrapperIntBase *stat ) const
{
	if ( !stat ) {
		return NULL;
	}
	return stat->GetBuf( );
}

bool
StatWrapper::GetBuf( const StatWrapperIntBase *stat,
					 StatStructType &buf ) const
{
	if ( !stat ) {
		return false;
	}
	return stat->GetBuf( buf );
}


// stat() that'll run all of the stats
int
StatWrapper::Stat( StatOpType which, bool force )
{
	StatWrapperOp	*op = m_ops[which];

	m_last_op = op;
	m_last_which = which;
	m_last_stat = op->getPrimary( );

	// Invoke the relevant stat functions
	return op->StatAll( force );
}

// Path specific Stat() - Will run stat() and lstat()
int
StatWrapper::Stat( const MyString &path, StatOpType which, bool force )
{
	if ( !SetPath( path ) ) {
		return -1;
	}

	return Stat( which, force );
}

// Path specific Stat() - Will run stat() and lstat()
int
StatWrapper::Stat( const char *path, StatOpType which, bool force )
{
	if ( !SetPath( path ) ) {
		return -1;
	}

	return Stat( which, force );
}

// FD specific Stat()
int
StatWrapper::Stat( int fd, bool force )
{
	if ( !SetFD( fd ) ) {
		return -1;
	}

	return Stat( STATOP_FSTAT, force );
}

// Retry the last operation
int
StatWrapper::Retry( void )
{
	return m_last_op->StatAll( true );
}

// Get the stat structure
StatWrapperIntBase *
StatWrapper::GetStat( StatOpType which ) const
{
	if ( STATOP_LAST == which ) {
		which = m_last_which;
	}
	if ( (which < STATOP_MIN) || (which > STATOP_MAX) ) {
		which = STATOP_NONE;
	}
	return m_ops[which]->getPrimary( );
}
