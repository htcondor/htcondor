/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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
#include "condor_debug.h"
#include <stdarg.h>
#include "user_log.c++.h"
#include <time.h>
#include "MyString.h"
#include "condor_uid.h"
#include "condor_xml_classads.h"
#include "condor_config.h"
#include "stat_wrapper.h"
#include "../condor_privsep/condor_privsep.h"
#include "read_user_log_state.h"


static const char SynchDelimiter[] = "...\n";

// Values for min scores
const int SCORE_THRESH_FWSEARCH		= 4;
const int SCORE_THRESH_NONROT		= 3;
const int SCORE_MIN_MATCH			= 1;

// Score factor values
// For Windoze:
//  st_ino is meaningless, so we ignore it; set it's factor to zero
// UNIX:
//  We look at the inode & ctime
//  Note: inodes are recycled; and thus an idential inode number
//  does *not* garentee the we have the same file
//  Also note that ctime is "change time" of the inode *not* 
//  creation time, we can't completely rely on it, either  :(
const int SCORE_FACTOR_UNIQ_MATCH	= 100;
# if defined(WIN32)
const int SCORE_FACTOR_CTIME		= 2;
const int SCORE_FACTOR_INODE		= 0;
# else
const int SCORE_FACTOR_CTIME		= 1;
const int SCORE_FACTOR_INODE		= 2;
# endif
const int SCORE_FACTOR_SAME_SIZE	= 2;
const int SCORE_FACTOR_GROWN		= 1;
const int SCORE_FACTOR_SHRUNK		= -5;


// Threshold to consider file stat's as recent (seconds)
const int SCORE_RECENT_THRESH		= 60;


// Simple class to extract info from a log file header event
class ReadUserLogHeader
{
public:
	ReadUserLogHeader( void ) { m_valid = false; };
	ReadUserLogHeader( const ULogEvent *event ) {
		m_valid = false;
		ExtractEvent( event );
	};
	~ReadUserLogHeader( void ) { };

	// Read the header from a file
	int ReadFileHeader( int rot,
						const char *path,
						const ReadUserLogState *state );

	// Extract data from an event
	int ExtractEvent( const ULogEvent *);

	// Valid?
	bool IsValid( void ) const { return m_valid; };

	// Get extracted info
	void Id( MyString &id ) const { id = m_id; };
	const MyString &Id( void ) const { return m_id; };
	int Sequence( void ) const { return m_sequence; };
	time_t Ctime( void ) const { return m_ctime; };

private:
	MyString	m_id;
	int			m_sequence;
	time_t		m_ctime;

	bool		m_valid;
};


// Class to manage score the user logs
// This class was created because you can't pre-declare the
// StructStatType in the main header file, so I created this simple
// class to manage all related stuff
class ReadUserLogMatch
{
public:

	// Constructor & destructor
	ReadUserLogMatch( ReadUserLogState *state ) {
		m_state = state;
	};
	// Note: *Don't* delete m_state -- see the comment below
	~ReadUserLogMatch( void ) { };

	// Results of file compare
	enum MatchResult {
		MATCH_ERROR = -1, MATCH = 0, UNKNOWN, NOMATCH,
	};
	
	// Compare the specified file / file info with the cached info
	MatchResult Match(
		int				 rot,
		int				 match_thresh,
		int				*score = NULL ) const;
	MatchResult Match(
		const char		*path,
		int				 rot,
		int				 match_thresh,
		int				*score = NULL ) const;
	MatchResult Match(
		StatStructType	&statbuf,
		int				 rot,
		int				 match_thresh,
		int				*score = NULL ) const;

	// Get a string to match the result value
	const char *MatchStr( MatchResult value ) const;

private:
	MatchResult MatchInternal(
		int				 rot,
		const char		*path,
		int				 match_thresh,
		int				*state_score ) const;
	MatchResult EvalScore(
		int				 max_thresh,
		int				 score ) const;

	// NOTE: m_state is *not* owned by this object, but, rather,
	//  we store a pointer to it for easy access
	ReadUserLogState	*m_state;		// File state info
};


// **********************************
// ReadUserLog class methods
// **********************************

ReadUserLog::ReadUserLog (const char * filename)
{
	clear();

    if (!initialize(filename)) {
		dprintf(D_ALWAYS, "Failed to open %s\n", filename);
    }
}

ReadUserLog::ReadUserLog ( const ReadUserLog::FileState &state )
{
	clear();

    if ( !initialize(state, true ) ) {
		dprintf( D_ALWAYS, "Failed to open from state\n" );
    }
}

// In this case, we don't stat the file, but use the stat info
//  restored in "state" passed to us by the application
bool
ReadUserLog::initialize( const ReadUserLog::FileState &state,
						 bool handle_rotation )
{
	m_handle_rot = handle_rotation;
	m_max_rot = handle_rotation ? 1 : 0;

	m_state = new ReadUserLogState( state, m_max_rot, SCORE_RECENT_THRESH );
	m_match = new ReadUserLogMatch( m_state );
	return initialize( handle_rotation, false, true, true );
}

bool
ReadUserLog::initialize( const char *filename,
						 bool handle_rotation,
						 bool check_for_old )
{
	m_handle_rot = handle_rotation;
	m_max_rot = handle_rotation ? 1 : 0;

	m_state = new ReadUserLogState( filename, m_max_rot, SCORE_RECENT_THRESH );
	m_match = new ReadUserLogMatch( m_state );

	bool	enable_header_read = handle_rotation;
	if (! initialize( handle_rotation, check_for_old,
					  false, enable_header_read ) ) {
		return false;
	}

	return true;
}

bool
ReadUserLog::initialize ( bool handle_rotation,
						  bool check_for_rotation,
						  bool restore,
						  bool enable_header_read )
{	
	m_handle_rot = handle_rotation;
	m_max_rot = handle_rotation ? 1 : 0;
	m_read_header = enable_header_read;

	// Set the score factor in the file state manager
	m_state->SetScoreFactor( ReadUserLogState::SCORE_CTIME,
							 SCORE_FACTOR_CTIME );
	m_state->SetScoreFactor( ReadUserLogState::SCORE_INODE,
							 SCORE_FACTOR_INODE );
	m_state->SetScoreFactor( ReadUserLogState::SCORE_SAME_SIZE,
							 SCORE_FACTOR_SAME_SIZE );
	m_state->SetScoreFactor( ReadUserLogState::SCORE_GROWN,
							 SCORE_FACTOR_GROWN );
	m_state->SetScoreFactor( ReadUserLogState::SCORE_SHRUNK,
							 SCORE_FACTOR_SHRUNK );

	if ( restore ) {
		// Do nothing
	}
	else if ( m_handle_rot && check_for_rotation ) {
		m_max_rot = 1;
		if (! FindPrevFile( m_max_rot, 0, true ) ) {
			releaseResources();
			return false;
		}
	}
	else {
		m_max_rot = 0;
		if ( m_state->Rotation( 0, true ) ) {
			releaseResources();
			return false;
		}
	}

	// Should we be locking?
	m_lock_file = param_boolean( "ENABLE_USERLOG_LOCKING", true );

	// Should we close the file between operations?
# if defined(WIN32)
	m_close_file = param_boolean( "ALWAYS_CLOSE_USERLOG", true );
	if ( m_handle_rot ) {
		m_close_file = true;	// Can't rely on open FD with unlink / rename
	}
# else
	m_close_file = param_boolean( "ALWAYS_CLOSE_USERLOG", false );
# endif

	// Now, open the file, setup locks, read the header, etc.
	if ( restore ) {
		dprintf( D_FULLDEBUG, "init: ReOpening file %s\n",
				 m_state->CurPath() );
		ULogEventOutcome status = ReopenLogFile( );
		if ( ULOG_MISSED_EVENT == status ) {
			m_missed_event = true;	// We'll check this during readEvent()
			dprintf( D_FULLDEBUG,
					 "ReadUserLog::initialize: Missed event\n" );
		}
		else if ( status != ULOG_OK ) {
			dprintf( D_ALWAYS,
					 "ReadUserLog::initialize: error re-opening file\n" );
			releaseResources();
			return false;
		}
	}
	else {
		dprintf( D_FULLDEBUG, "init: Opening file %s\n", m_state->CurPath() );
		if ( ULOG_OK != OpenLogFile( restore ) ) {
			dprintf( D_ALWAYS,
					 "ReadUserLog::initialize: error opening file\n" );
			releaseResources();
			return false;
		}
	}

	// Close the file between operations
	if ( m_close_file ) {
		CloseLogFile( );
	}

	m_initialized = true;
	return true;
}

bool
ReadUserLog::CloseLogFile( void )
{

	// Close the file pointer
    if ( m_fp ) {
		fclose( m_fp );
		m_fp = NULL;
		m_fd = -1;
	}

	// Close the FD
	if ( m_fd >= 0 ) {
	    close( m_fd );
		m_fd = -1;
	}

	// And, unlock it
	if ( m_is_locked ) {
		m_lock->release();
		m_is_locked = false;
		m_lock_rot = -1;
	}

	return true;
}

ULogEventOutcome
ReadUserLog::OpenLogFile( bool do_seek, bool read_header )
{
	// Note: For whatever reason, we obtain a WRITE lock in method
	// readEvent.  On Linux, if the file is opened O_RDONLY, then a
	// WRITE_LOCK never blocks.  Thus open the file RDWR so the
	// WRITE_LOCK below works correctly.
	//  
	// NOTE: we tried changing this to O_READONLY once and things
	// stopped working right, so don't try it again, smarty-pants!

	// Is the lock current?
	bool	is_lock_current = ( m_state->Rotation() == m_lock_rot );

	dprintf( D_FULLDEBUG, "Opening log file #%d '%s'"
			 "(is_lock_cur=%s,seek=%s,read_header=%s)\n",
			 m_state->Rotation(), m_state->CurPath(),
			 is_lock_current ? "true" : "false",
			 do_seek ? "true" : "false",
			 read_header ? "true" : "false" );
	if ( m_state->Rotation() < 0 ) {
		m_state->Rotation(-1);
	}

	m_fd = safe_open_wrapper( m_state->CurPath(), O_RDWR, 0 );
	if ( m_fd < 0 ) {
		return ULOG_RD_ERROR;
	}

	m_fp = fdopen( m_fd, "r" );
	if ( m_fp == NULL ) {
		CloseLogFile( );
	    return ULOG_RD_ERROR;
	}

	// Seek to the previous location
	if ( do_seek && m_state->Offset() ) {
		if( fseek( m_fp, m_state->Offset(), SEEK_SET) ) {
			CloseLogFile( );
			return ULOG_RD_ERROR;
		}
	}

	// Prepare to lock the file
	if ( m_lock_file ) {

		// If the lock isn't for the current file (rotation #), destroy it
		if ( ( !is_lock_current ) && m_lock ) {
			delete m_lock;
			m_lock = NULL;
			m_lock_rot = -1;
		}

		// Create a lock if none exists
		// otherwise, update the lock's fd & fp
		if ( ! m_lock ) {
			m_lock = new FileLock( m_fd, m_fp, m_state->CurPath() );
			if( ! m_lock ) {
				CloseLogFile( );
				return ULOG_RD_ERROR;
			}
			m_lock_rot = m_state->Rotation( );
		}
		else {
			m_lock->SetFdFp( m_fd, m_fp );
		}
	}

	// Determine the type of the log file (if needed)
	if ( m_state->IsLogType( ReadUserLogState::LOG_TYPE_UNKNOWN) ) {
		if ( !determineLogType() ) {
			dprintf( D_ALWAYS,
					 "ReadUserLog::OpenLogFile(): Can't log type\n" );
			releaseResources();
			return ULOG_RD_ERROR;
		}
	}

	// Read the file's header event
	if ( read_header && m_read_header && ( !m_state->ValidUniqId()) ) {
		ReadUserLogHeader	header;
		if ( ! header.ReadFileHeader( m_state->Rotation(),
									  m_state->CurPath(),
									  m_state ) ) {
			m_state->UniqId( header.Id() );
			m_state->Sequence( header.Sequence() );
			dprintf( D_FULLDEBUG,
					 "%s: Set UniqId to '%s', sequence to %d\n",
					 m_state->CurPath(),
					 header.Id().GetCStr(),
					 header.Sequence() );
		}
	}
	

	return ULOG_OK;
}

bool
ReadUserLog::determineLogType( void )
{
	// now determine if the log file is XML and skip over the header (if
	// there is one) if it is XML

	// we obtain a write lock here not because we want to write
	// anything, but because we want to ensure we don't read
	// mid-way through someone else's write
	Lock( false );

	// store file position so we can rewind to this location
	long filepos = ftell( m_fp );
	if( filepos < 0 ) {
		dprintf(D_ALWAYS, "ftell failed in ReadUserLog::determineLogType\n");
		Unlock( false );
		return false;
	}
	m_state->Offset( filepos );

	char afterangle;
	int scanf_result = fscanf(m_fp, " <%c", &afterangle);
	if( scanf_result == EOF ) {
		// Format is unknown.
		m_state->LogType( ReadUserLogState::LOG_TYPE_UNKNOWN );

	} else if( scanf_result > 0 ) {

		m_state->LogType( ReadUserLogState::LOG_TYPE_XML );

		if( !skipXMLHeader(afterangle, filepos) ) {
			m_state->LogType( ReadUserLogState::LOG_TYPE_UNKNOWN );
			Unlock( false );
		    return false;
		}

	} else {
		// the first non whitespace char is not <, so this doesn't look like
		// XML; go back to the beginning and take another look
		if( fseek( m_fp, filepos, SEEK_SET) )	{
			dprintf(D_ALWAYS, "fseek failed in ReadUserLog::determineLogType");
			Unlock( false );
			return false;
		}

		int nothing;
		if( fscanf( m_fp, " %d", &nothing) > 0 ) {

			setIsOldLog(true);

			if( fseek( m_fp, filepos, SEEK_SET) )	{
				dprintf(D_ALWAYS,
						"fseek failed in ReadUserLog::determineLogType");
				Unlock( false );
				return false;
			}
		} else {
			// what sort of log is this!
			dprintf(D_ALWAYS, "Error, apparently invalid user log file\n");
			if( fseek( m_fp, filepos, SEEK_SET) )	{
				dprintf(D_ALWAYS,
						"fseek failed in ReadUserLog::determineLogType");
				Unlock( false );
				return false;
			}
			Unlock( false );
			return false;
		}
	}

	Unlock( false );

	return true;
}

bool ReadUserLog::
skipXMLHeader(char afterangle, long filepos)
{
	// now figure out if there is a header, and if so, advance _fp past
	// it - this is really ugly
	int nextchar = afterangle;
	if( nextchar == '?' || nextchar == '!' ) {
		// we're probably in the document prolog
		while( nextchar == '?' || nextchar == '!' ) {
			// skip until we get out of this tag
			nextchar = fgetc(m_fp);
			while( nextchar != EOF && nextchar != '>' ) {
				nextchar = fgetc(m_fp);
			}

			if( nextchar == EOF ) {
				return false;
			}

			// skip until we get to the next tag, saving our location as
			// we go so we can skip back two chars later
			while( nextchar != EOF && nextchar != '<' ) {
				filepos = ftell(m_fp);
				nextchar = fgetc(m_fp);
			}
			if( nextchar == EOF ) {
				return false;
			}
			nextchar = fgetc(m_fp);
		}

		// now we are in a tag like <[^?!]*>, so go back two chars and
		// we're all set
		if( fseek(m_fp, filepos, SEEK_SET) )	{
			dprintf(D_ALWAYS, "fseek failed in ReadUserLog::skipXMLHeader");
			return false;
		}
	} else {
		// there was no prolog, so go back to the beginning
		if( fseek(m_fp, filepos, SEEK_SET) )	{
			dprintf(D_ALWAYS, "fseek failed in ReadUserLog::skipXMLHeader");
			return false;
		}
	}

	m_state->Offset( filepos );

	return true;
}

bool
ReadUserLog::FindPrevFile( int start, int num, bool store_stat )
{
	if ( !m_handle_rot ) {
		return true;
	}

	// Deterine the range to look at
	int				end;
	if ( 0 == num ) {
		end = 0;
	} else {
		end = start - num + 1;
		if ( end < 0 ) {
			end = 0;
		}
	}

	// Search for the previous file
	for ( int rot = start;  rot >= end;  rot-- ) {
		if ( 0 == m_state->Rotation( rot, store_stat )  ) {
			dprintf( D_FULLDEBUG, "Found: '%s'\n", m_state->CurPath() );
			return true;
		}
	}
	return false;
}

ULogEventOutcome
ReadUserLog::ReopenLogFile( void )
{

	// First, if the file's open, we're done.  :)
	if ( m_fp ) {
		return ULOG_OK;
	}

	// If we're not handling rotation, just try to reopen the file
	if ( ! m_handle_rot ) {
		return OpenLogFile( true );
	}

	// If we don't have valid info, do a new file search, just like init
	if ( ! m_state->IsValid() ) {
		if ( m_handle_rot ) {
			dprintf( D_FULLDEBUG, "reopen: looking for previous file...\n" );
			if (! FindPrevFile( m_max_rot, 0, true ) ) {
				return ULOG_NO_EVENT;
			}
		}
		else {
			if ( m_state->Rotation( 0, true ) ) {
				return ULOG_NO_EVENT;
			}
		}
		return OpenLogFile( false );
	}

	// Search forward, starting with the "current" file, looking
	// for a file with the same "signature", or the highest score.
	int new_rot = -1;
	int max_score = -1;
	int max_score_rot = -1;
	int *scores = new int(m_max_rot+1);
	int	start = m_state->Rotation();
	for( int rot = start; (rot <= m_max_rot) && (new_rot < 0); rot++ ) {
		int		score;

		ReadUserLogMatch::MatchResult result =
			m_match->Match( rot, SCORE_THRESH_FWSEARCH, &score );
		if ( ReadUserLogMatch::MATCH_ERROR == result ) {
			scores[rot] = -1;
		}
		else if ( ReadUserLogMatch::MATCH == result ) {
			new_rot = rot;
		}
		else if ( ReadUserLogMatch::UNKNOWN == result ) {
			scores[rot] = score;
			if ( score > max_score ) {
				max_score_rot = rot;
				max_score = score;
			}
		}
	}
	delete [] scores;

	// No good match found, fall back to highest score
	if ( ( new_rot < 0 )  &&  ( max_score > 0 )  ) {
		new_rot = max_score_rot;
	}

	// If we've found a good match, or a high score, do it
	if ( new_rot >= 0 ) {
		if ( m_state->Rotation( new_rot ) ) {
			return ULOG_RD_ERROR;
		}
		return OpenLogFile( true );
	}

	// If we got here, no match found.  :(
	m_state->Reset( );			// We know nothing about the state now!
	return ULOG_MISSED_EVENT;
}

ULogEventOutcome
ReadUserLog::readEvent (ULogEvent *& event )
{
	return readEvent( event, true );
}

ULogEventOutcome
ReadUserLog::readEvent (ULogEvent *& event, bool store_state )
{
	if ( !m_initialized ) {
		return ULOG_RD_ERROR;
	}

	// Previous operation (initialization) detected a missed event
	// but couldn't report it to the application (the API doesn't
	// allow us to reliably return that type of info).
	if ( m_missed_event ) {
		m_missed_event = false;
		return ULOG_MISSED_EVENT;
	}

	// If the file was closed on us, try to reopen it
	if ( !m_fp ) {
		ULogEventOutcome	status = ReopenLogFile( );
		if ( ULOG_OK != status ) {
			return status;
		}
	}

	if ( !m_fp ) {
		return ULOG_NO_EVENT;
	}

	ULogEventOutcome	outcome = ULOG_OK;
	if( m_state->IsLogType( ReadUserLogState::LOG_TYPE_UNKNOWN ) ) {
	    if( !determineLogType() ) {
			outcome = ULOG_RD_ERROR;
			goto CLEANUP;
		}
	}

	// Now, read the actual event (depending on the file type)
	bool	try_again;
	outcome = readEvent( event, &try_again );
	if ( ! m_handle_rot ) {
		try_again = false;
	}

	// If we hit the end of a rotated file, try the previous one
	if ( try_again ) {

		// We've hit the end of file and file has been closed
		// This means that we've missed an event :(
		if ( m_state->Rotation() < 0 ) {
			return ULOG_MISSED_EVENT;
		}

		// We've hit the end of a non-rotated file
		// (a file that isn't a ".old" or ".1", etc.)
		else if ( m_state->Rotation() == 0 ) {
			// Same file?
			ReadUserLogMatch::MatchResult result;
			result = m_match->Match( m_state->CurPath(),
									 m_state->Rotation(),
									 SCORE_THRESH_NONROT );
			dprintf( D_FULLDEBUG,
					 "readEvent: checking to see if file (%s) matches: %s\n",
					 m_state->CurPath(), m_match->MatchStr(result) );
			if ( result == ReadUserLogMatch::NOMATCH ) {
				CloseLogFile( );
			}
			else {
				try_again = false;
			}
		}

		// We've hit the end of a ".old" or ".1", ".2" ... file
		else {
			CloseLogFile( );

			bool found = FindPrevFile( m_state->Rotation() - 1, 1, true );
			dprintf( D_FULLDEBUG,
					 "readEvent: checking for previous file (%d): %s\n",
					 m_state->Rotation(), found ? "Found" : "Not found" );
			if ( !found ) {
				try_again = false;
			}
		}
	}

	// Finally, one more attempt to read an event
	if ( try_again ) {
		outcome = ReopenLogFile();
		if ( ULOG_OK == outcome ) {
			outcome = readEvent( event, (bool*)NULL );
		}
	}

	// Store off our current offset
	if (  ( ULOG_OK == outcome ) && ( store_state )  )  {
		long	pos = ftell( m_fp );
		if ( pos > 0 ) {
			m_state->Offset( pos );
		}
		m_state->StatFile( m_fd );
	}
	
	// Close the file between operations
  CLEANUP:
	if ( m_close_file ) {
		CloseLogFile( );
	}

	return outcome;

}	

ULogEventOutcome
ReadUserLog::readEvent( ULogEvent *& event, bool *try_again )
{
	ULogEventOutcome	outcome;

	if( m_state->IsLogType( ReadUserLogState::LOG_TYPE_XML ) ) {
		outcome = readEventXML( event );
		if ( try_again ) {
			*try_again = (outcome == ULOG_NO_EVENT );
		}
	} else if( m_state->IsLogType( ReadUserLogState::LOG_TYPE_OLD ) ) {
		outcome = readEventOld( event );
		if ( try_again ) {
			*try_again = (outcome == ULOG_NO_EVENT );
		}
	} else {
		outcome = ULOG_NO_EVENT;
		if ( try_again ) {
			try_again = false;
		}
	}
	return outcome;
}

ULogEventOutcome
ReadUserLog::readEventXML(ULogEvent *& event)
{
	ClassAdXMLParser xmlp;

	// we obtain a write lock here not because we want to write
	// anything, but because we want to ensure we don't read
	// mid-way through someone else's write
	Lock( true );

	// store file position so that if we are unable to read the event, we can
	// rewind to this location
  	long     filepos;
  	if (!m_fp || ((filepos = ftell(m_fp)) == -1L))
  	{
  		Unlock( true );
		event = NULL;
  		return ULOG_UNK_ERROR;
  	}

	ClassAd* eventad;
	eventad = xmlp.ParseClassAd(m_fp);

	Unlock( true );

	if( !eventad ) {
		// we don't have the full event in the stream yet; restore file
		// position and return
		if( fseek(m_fp, filepos, SEEK_SET) )	{
			dprintf(D_ALWAYS, "fseek() failed in ReadUserLog::readEvent");
			return ULOG_UNK_ERROR;
		}
		clearerr(m_fp);
		event = NULL;
		return ULOG_NO_EVENT;
	}

	int enmbr;
	if( !eventad->LookupInteger("EventTypeNumber", enmbr) ) {
		event = NULL;
		delete eventad;
		return ULOG_NO_EVENT;
	}

	if( !(event = instantiateEvent((ULogEventNumber) enmbr)) ) {
		event = NULL;
		delete eventad;
		return ULOG_UNK_ERROR;
	}
	
	event->initFromClassAd(eventad);	

	delete eventad;
	return ULOG_OK;
}

ULogEventOutcome
ReadUserLog::readEventOld(ULogEvent *& event)
{
	long   filepos;
	int    eventnumber;
	int    retval1, retval2;

	// we obtain a write lock here not because we want to write
	// anything, but because we want to ensure we don't read
	// mid-way through someone else's write
	if (!m_is_locked) {
		m_lock->obtain( WRITE_LOCK );
	}

	// store file position so that if we are unable to read the event, we can
	// rewind to this location
	if (!m_fp || ((filepos = ftell(m_fp)) == -1L))
	{
		dprintf( D_FULLDEBUG,
				 "ReadUserLog: invalid m_fp, or ftell() failed\n" );
		if (!m_is_locked) {
			m_lock->release();
		}
		return ULOG_UNK_ERROR;
	}

	retval1 = fscanf (m_fp, "%d", &eventnumber);

	// so we don't dump core if the above fscanf failed
	if (retval1 != 1) {
		eventnumber = 1;
		// check for end of file -- why this is needed has been
		// lost, but it was removed once and everything went to
		// hell, so don't touch it...
			// Note: this is needed because if this method is called and
			// you're at the end of the file, fscanf returns EOF (-1) and
			// you get here.  If you're at EOF you had better bail out...
			// (This is not uncommon -- any time you try to read an event
			// and there aren't any events to read you get here.)
			// If fscanf returns 0, you're probably *really* in trouble.
			// wenger 2004-10-07.
		if( feof( m_fp ) ) {
			event = NULL;  // To prevent FMR: Free memory read
			clearerr( m_fp );
			if( !m_is_locked ) {
				m_lock->release();
			}
			return ULOG_NO_EVENT;
		}
		dprintf( D_FULLDEBUG, "ReadUserLog: error (not EOF) reading "
					"event number\n" );
	}

	// allocate event object; check if allocated successfully
	event = instantiateEvent ((ULogEventNumber) eventnumber);
	if (!event) 
	{
		dprintf( D_FULLDEBUG, "ReadUserLog: unable to instantiate event\n" );
		if (!m_is_locked) {
			m_lock->release();
		}
		return ULOG_UNK_ERROR;
	}

	// read event from file; check for result
	retval2 = event->getEvent (m_fp);
		
	// check if error in reading event
	if (!retval1 || !retval2)
	{	
		dprintf( D_FULLDEBUG, "ReadUserLog: error reading event; re-trying\n" );

		// we could end up here if file locking did not work for
		// whatever reason (usual NFS bugs, whatever).  so here
		// try to wait a second until the current partially-written
		// event has benn completely written.  the algorithm is
		// wait a second, rewind to our initial position (in case a
		// buggy getEvent() slurped up more than one event), then
		// again try to synchronize the log
		// 
		// NOTE: this code is important, so don't remove or "fix"
		// it unless you *really* know what you're doing and test it
		// extermely well
		if( !m_is_locked ) {
			m_lock->release();
		}
		sleep( 1 );
		if( !m_is_locked ) {
			m_lock->obtain( WRITE_LOCK );
		}                             
		if( fseek( m_fp, filepos, SEEK_SET)) {
			dprintf( D_ALWAYS, "fseek() failed in %s:%d", __FILE__, __LINE__ );
			if (!m_is_locked) {
				m_lock->release();
			}
			return ULOG_UNK_ERROR;
		}
		if( synchronize() )
		{
			// if synchronization was successful, reset file position and ...
			if (fseek (m_fp, filepos, SEEK_SET))
			{
				dprintf(D_ALWAYS, "fseek() failed in ReadUserLog::readEvent");
				if (!m_is_locked) {
					m_lock->release();
				}
				return ULOG_UNK_ERROR;
			}
			
			// ... attempt to read the event again
			clearerr (m_fp);
			int oldeventnumber = eventnumber;
			eventnumber = -1;
			retval1 = fscanf (m_fp, "%d", &eventnumber);
			if( retval1 == 1 ) {
			  if( eventnumber != oldeventnumber ) {
			    if( event ) {
			      delete event;
			    }
			    // allocate event object; check if allocated
			    // successfully
			    event =
			      instantiateEvent( (ULogEventNumber)eventnumber );
			    if( !event ) { 
				  dprintf( D_FULLDEBUG, "ReadUserLog: unable to "
				  			"instantiate event\n" );
			      if( !m_is_locked ) {
					m_lock->release();
			      }
			      return ULOG_UNK_ERROR;
			    }
			  }
			  retval2 = event->getEvent( m_fp );
			}

			// if failed again, we have a parse error
			if (!retval1 != 1 || !retval2)
			{
				dprintf( D_FULLDEBUG, "ReadUserLog: error reading event "
							"on second try\n");
				delete event;
				event = NULL;  // To prevent FMR: Free memory read
				synchronize ();
				if (!m_is_locked) {
					m_lock->release();
				}
				return ULOG_RD_ERROR;
			}
			else
			{
			  // finally got the event successfully --
			  // synchronize the log
			  if( synchronize() ) {
			    if( !m_is_locked ) {
			      m_lock->release();
			    }
			    return ULOG_OK;
			  }
			  else
			  {
			    // got the event, but could not synchronize!!
			    // treat as incomplete event
				dprintf( D_FULLDEBUG, "ReadUserLog: got event on second try "
						"but synchronize() failed\n");
			    delete event;
			    event = NULL;  // To prevent FMR: Free memory read
			    clearerr( m_fp );
			    if( !m_is_locked ) {
			      m_lock->release();
			    }
			    return ULOG_NO_EVENT;
			  }
			}
		}
		else
		{
			// if we could not synchronize the log, we don't have the full	
			// event in the stream yet; restore file position and return
			dprintf( D_FULLDEBUG, "ReadUserLog: syncronize() failed\n");
			if (fseek (m_fp, filepos, SEEK_SET))
			{
				dprintf(D_ALWAYS, "fseek() failed in ReadUserLog::readEvent");
				if (!m_is_locked) {
					m_lock->release();
				}
				return ULOG_UNK_ERROR;
			}
			clearerr (m_fp);
			delete event;
			event = NULL;  // To prevent FMR: Free memory read
			if (!m_is_locked) {
				m_lock->release();
			}
			return ULOG_NO_EVENT;
		}
	}
	else
	{
		// got the event successfully -- synchronize the log
		if (synchronize ())
		{
			if (!m_is_locked) {
				m_lock->release();
			}
			return ULOG_OK;
		}
		else
		{
			// got the event, but could not synchronize!!  treat as incomplete
			// event
			dprintf( D_FULLDEBUG, "ReadUserLog: got event on first try "
					"but synchronize() failed\n");

			delete event;
			event = NULL;  // To prevent FMR: Free memory read
			clearerr (m_fp);
			if (!m_is_locked) {
				m_lock->release();
			}
			return ULOG_NO_EVENT;
		}
	}

	// will not reach here
	if (!m_is_locked) {
		m_lock->release();
	}

	dprintf( D_ALWAYS, "Error: got to the end of "
			"ReadUserLog::readEventOld()\n");

	return ULOG_UNK_ERROR;
}

bool
ReadUserLog::InitFileState( ReadUserLog::FileState &state )
{
	return ReadUserLogState::InitState( state );
}

bool
ReadUserLog::UninitFileState( ReadUserLog::FileState &state ) const
{
	return m_state->UninitState( state );
}

bool
ReadUserLog::GetFileState( ReadUserLog::FileState &state ) const
{
	return m_state->GetState( state );
}

bool
ReadUserLog::SetFileState( const ReadUserLog::FileState &state )
{
	return m_state->SetState( state );
}


void
ReadUserLog::Lock( bool verify_init )
{
	if( verify_init ) {
		ASSERT ( m_initialized );
	}
	if ( !m_is_locked ) {
		m_is_locked = m_lock->obtain( WRITE_LOCK );
	}
	ASSERT( m_is_locked );
}

void
ReadUserLog::Unlock( bool verify_init )
{
	if( verify_init ) {
		ASSERT ( m_initialized );
	}
	if ( m_is_locked ) {
		m_is_locked = ! m_lock->release();
	}
	ASSERT( !m_is_locked );
}

bool
ReadUserLog::synchronize ()
{
	if ( !m_initialized ) {
		return false;
	}

	const int bufSize = 512;
    char buffer[bufSize];
    while( fgets( buffer, bufSize, m_fp ) != NULL ) {
		if( strcmp( buffer, SynchDelimiter) == 0 ) {
            return true;
        }
    }
    return false;
}

void
ReadUserLog::outputFilePos(const char *pszWhereAmI)
{
	ASSERT ( m_initialized );
	dprintf(D_ALWAYS, "Filepos: %ld, context: %s\n", ftell(m_fp), pszWhereAmI);
}

void
ReadUserLog::setIsXMLLog( bool is_xml )
{
	if( is_xml ) {
	    m_state->LogType( ReadUserLogState::LOG_TYPE_XML );
	} else {
	    m_state->LogType( ReadUserLogState::LOG_TYPE_UNKNOWN );
	}
}

bool
ReadUserLog::getIsXMLLog( void ) const
{
	return ( m_state->IsLogType( ReadUserLogState::LOG_TYPE_XML ) );
}

void
ReadUserLog::setIsOldLog( bool is_old )
{
	if( is_old ) {
	    m_state->LogType( ReadUserLogState::LOG_TYPE_OLD );
	} else {
	    m_state->LogType( ReadUserLogState::LOG_TYPE_UNKNOWN );
	}
}

bool
ReadUserLog::getIsOldLog( void ) const
{
	return ( m_state->IsLogType( ReadUserLogState::LOG_TYPE_OLD ) );
}

void
ReadUserLog::clear( void )
{
	m_initialized = false;
	m_missed_event = false;
	m_state = NULL;
	m_match = NULL;
    m_fd = -1;
	m_fp = NULL;
	m_lock = NULL;
	m_is_locked = false;
	m_lock_rot = -1;
}

void
ReadUserLog::releaseResources( void )
{
	if ( m_match ) {
		delete m_match;
		m_match = NULL;
	}

	if ( m_state ) {
		delete m_state;
		m_state = NULL;
	}

    if (m_fp) {
		fclose(m_fp);
		m_fp = NULL;
		m_fd = -1;
	}

	if (m_fd != -1) {
	    close(m_fd);
		m_fd = -1;
	}

	if (m_is_locked) {
		m_lock->release();
		m_is_locked = false;
		m_lock_rot = -1;
	}

	delete m_lock;
	m_lock = NULL;
}

void
ReadUserLog::FormatFileState ( MyString &str, const char *label ) const
{
	if ( ! m_state ) {
		if ( label ) {
			str.sprintf( "%s: no state", label );
		}
		else {
			str = "no state\n";
		}
		return;
	}

	m_state->GetState( str, label );
}


// **********************************
// ReadUserLogHeader methods
// **********************************

// Read the header from a file
int
ReadUserLogHeader::ReadFileHeader(
	int						 rot,
	const char				*path,
	const ReadUserLogState	*state )
{
	// Here, we have an indeterminate result
	// Read the file's header info

	// We'll instantiate a new log reader to do this for us
	// Note: we disable rotation for this one, so we won't recurse infinitely
	ReadUserLog			 reader;

	// If no path provided, generate one
	MyString temp_path;
	if ( NULL == path ) {
		state->GeneratePath( rot, temp_path );
		path = temp_path.GetCStr( );
	}

	// Initialize the reader
	if ( !reader.initialize( path, false, false ) ) {
		return -1;
	}

	// Now, read the event itself
	ULogEvent			*event;
	ULogEventOutcome	outcome;
	outcome = reader.readEvent( event );
	if ( ULOG_RD_ERROR == outcome ) {
		return -1;
	}
	else if ( ULOG_OK != outcome ) {
		return 1;
	}

	// Finally, if it's a generic event, let's see if we can parse it
	int status = ExtractEvent( event );
	delete event;

	return status;
}

// Extract info from an event
int
ReadUserLogHeader::ExtractEvent( const ULogEvent *event )
{
	// Not a generic event -- ignore it
	if ( ULOG_GENERIC != event->eventNumber ) {
		return 1;
	}

	const GenericEvent	*generic = dynamic_cast <const GenericEvent*>( event );
	if ( ! generic ) {
		dprintf( D_ALWAYS, "Can't pointer cast generic event!\n" );
		return -1;
	} else {
		int		ctime;
		int		sequence;
		char	id[256];
		if ( sscanf( generic->info,
					 "Global JobLog: ctime=%d id=%s sequence=%d\n",
					 &ctime, id, &sequence ) == 3) {
			m_id = id;
			m_ctime = ctime;
			m_sequence = sequence;
			m_valid = true;
			return 0;
		}
		return 1;
	}
}


// **********************************
// ReadUserLogMatch methods
// **********************************

// Compare a file by rotation # to the cached info
ReadUserLogMatch::MatchResult
ReadUserLogMatch::Match(
	int				 rot,
	int				 match_thresh,
	int				*score_ptr ) const
{
	// Get the initial score from the state
	int	local_score;
	if ( NULL == score_ptr ) {
		score_ptr = &local_score;
	}
	*score_ptr = m_state->ScoreFile( rot );

	// Generate the final score using the internal logic
	return MatchInternal( rot, NULL, match_thresh, score_ptr );
}

// Compare the "current" file to the cached info
ReadUserLogMatch::MatchResult
ReadUserLogMatch::Match(
	const char		*path,
	int				 rot,
	int				 match_thresh,
	int				*score_ptr ) const
{
	// Get the initial score from the state
	int	local_score;
	if ( NULL == score_ptr ) {
		score_ptr = &local_score;
	}
	*score_ptr = m_state->ScoreFile( path, rot );

	// Generate the final score using the internal logic
	return MatchInternal( rot, path, match_thresh, score_ptr );
}

// Compare the stat info passed in to the cached info
ReadUserLogMatch::MatchResult
ReadUserLogMatch::Match(
	StatStructType 	&statbuf,
	int				 rot,
	int				 match_thresh,
	int				*score_ptr ) const
{
	// Get the initial score from the state
	int	local_score;
	if ( NULL == score_ptr ) {
		score_ptr = &local_score;
	}
	*score_ptr = m_state->ScoreFile( statbuf, rot );

	// Generate the final score using the internal logic
	return MatchInternal( rot, NULL, match_thresh, score_ptr );
}

// Score analysis
ReadUserLogMatch::MatchResult
ReadUserLogMatch::MatchInternal(
	int				 rot,
	const char		*path,
	int				 match_thresh,
	int				*score_ptr ) const
{
	ULogEventOutcome	outcome;
	int					score = *score_ptr;

	// If no path provided, generate one
	MyString path_str;
	if ( NULL == path ) {
		m_state->GeneratePath( rot, path_str );
	} else {
		path_str = path;
	}
	dprintf( D_FULLDEBUG, "Match: score of '%s' = %d\n",
			 path_str.GetCStr(), score );

	// Quick look at the score passed in from the state comparison
	// We can return immediately in some cases

	MatchResult result = EvalScore( match_thresh, score );
	if ( UNKNOWN != result ) {
		return result;
	}


	// Here, we have an indeterminate result
	// Read the file's header info

	// We'll instantiate a new log reader to do this for us
	// Note: we disable rotation for this one, so we won't recurse infinitely
	ReadUserLog			 reader;
	dprintf( D_FULLDEBUG, "Match: reading file %s\n", path_str.GetCStr() );

	// Initialize the reader
	if ( !reader.initialize( path_str.GetCStr(), false, false ) ) {
		return MATCH_ERROR;
	}

	// Now, read the event itself
	ULogEvent			*event;
	outcome = reader.readEvent( event );
	if ( ULOG_RD_ERROR == outcome ) {
		return MATCH_ERROR;
	}
	else if ( ULOG_OK != outcome ) {
		return NOMATCH;
	}

	// Read the file's header info
	ReadUserLogHeader	header;
	int status = header.ReadFileHeader( rot, path_str.GetCStr(), m_state );
	if( status < 0 ) {
		return MATCH_ERROR;
	}
	else if ( status > 0 ) {
		return EvalScore( match_thresh, score );
	}

	// Finally, extract the ID & store it
	int	id_result = m_state->CompareUniqId( header.Id() );
	const char *result_str = "unknown";
	if ( id_result > 0 ) {
		score += SCORE_FACTOR_UNIQ_MATCH;
		result_str = "match";
	}
	else if ( id_result < 0 ) {
		score = 0;
		result_str = "no match";
	}
	dprintf( D_FULLDEBUG, "Read ID from '%s' as '%s': %d (%s)\n",
			 path_str.GetCStr(), header.Id().GetCStr(),
			 id_result, result_str );

	// And, last but not least, re-evaluate the score
	dprintf( D_FULLDEBUG, "Match: Final score is %d\n", score );
	return EvalScore( match_thresh, score );
}

ReadUserLogMatch::MatchResult
ReadUserLogMatch::EvalScore( int match_thresh, int score ) const
{

	// < 0 is an error
	if ( score < 0 ) {
		return MATCH_ERROR;
	}
	// Less than the min threshold - give up, declare "no match"
	else if ( score < SCORE_MIN_MATCH ) {
		return NOMATCH;
	}
	// Or, if it's above the min match threshold, 
	else if ( score >= match_thresh ) {
		return MATCH;
	}
	else {
		return UNKNOWN;
	}
}

const char *
ReadUserLogMatch::MatchStr( ReadUserLogMatch::MatchResult value ) const
{
	switch( value ) {
	case MATCH_ERROR:
		return "ERROR";
	case MATCH:
		return "MATCH";
	case UNKNOWN:
		return "UNKNOWN";
	case NOMATCH:
		return "NOMATCH";
	default:
		return "<invalid>";
	};
	
}
		
