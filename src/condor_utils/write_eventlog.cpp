/***************************************************************
 *
 * Copyright (C) 1990-2009, Condor Team, Computer Sciences Department,
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
#include "condor_open.h"
#include "condor_debug.h"
#include "util_lib_proto.h"
#include "condor_event.h"
#include "write_eventlog.h"

#include <time.h>
#include "condor_uid.h"
#include "condor_config.h"
#include "utc_time.h"
#include "file_lock.h"
#include "condor_fsync.h"
#include "condor_attributes.h"
#include "CondorError.h"

#include <string>
#include <algorithm>
#include "basename.h"
#include "log_rotate.h"

// Set to non-zero to enable fine-grained rotation debugging / timing
#define ROTATION_TRACE	0

static const char SynchDelimiter[] = "...\n";


// Destructor
WriteEventLog::~WriteEventLog()
{
	m_log.reset("");
}

// ***********************************
//  WriteEventLog initialize() methods
// ***********************************

bool
WriteEventLog::initialize( const char *file, int max_log, int format_opts )
{
	m_format_opts = format_opts;
	m_log.reset(file);
	m_log.max_log = max_log;
	m_log.should_fsync = false;

	m_initialized = true;
	m_enabled = ! m_log.path.empty();
	return true;
}

// This should be correct for our use.
// We create one of these things and shove it into a vector.
// After it enters the vector, it never leaves; it gets destroyed.
// Probably ought to use shared_ptr or something to be truly safe.
// The (!copied) case is probably not necessary, but I am trying
// to be as safe as possible.

bool
WriteEventLog::openFile(log_file& file)
{
	if (file.fd >= 0) {
		dprintf( D_ALWAYS, "WriteUserLog::openFile: already open!\n" );
		return false;
	}

	if (file.path.empty()) {
		dprintf( D_ALWAYS, "WriteUserLog::openFile: NULL filename!\n" );
		return false;
	}

	if (file.path == UNIX_NULL_FILE) {
		// special case - deal with /dev/null.  we don't really want
		// to open /dev/null, but we don't want to fail in this case either
		// because this is common when the user does not want a log, but
		// the condor admin desires a global event log.
		// Note: we always check UNIX_NULL_FILE, since we canonicalize
		// to this even on Win32.
		file.fd = -1;
		return true;
	}

	// Unix
	int	flags = O_WRONLY | O_CREAT | O_APPEND;
#if defined(WIN32)

	// if we want lock-free append, we have to open the handle in a diffent file mode than what the
	// c-runtime uses.  FILE_APPEND_DATA but NOT FILE_WRITE_DATA or GENERIC_WRITE.
	// note that we do NOT pass _O_APPEND to _open_osfhandle() since what that does in the current (broken)
	// c-runtime is tell it to call seek before every write, but you *can't* seek an append-only file...
	// PRAGMA_REMIND("TJ: remove use_lock test here for 8.5.x")
	if (true) {
		DWORD err = 0;
		DWORD attrib =  FILE_ATTRIBUTE_NORMAL; // set to FILE_ATTRIBUTE_READONLY based on mode???
		DWORD create_mode = (flags & O_CREAT) ? OPEN_ALWAYS : OPEN_EXISTING;
		DWORD share_mode = FILE_SHARE_READ | FILE_SHARE_WRITE;
		HANDLE hf = CreateFile(file.path.c_str(), FILE_APPEND_DATA, share_mode, NULL, create_mode, attrib, NULL);
		if (hf == INVALID_HANDLE_VALUE) {
			file.fd = -1;
			err = GetLastError();
		} else {
			file.fd = _open_osfhandle((intptr_t)hf, flags & (/*_O_APPEND | */_O_RDONLY | _O_TEXT | _O_WTEXT));
			if (file.fd < 0) {
				// open_osfhandle can sometimes set errno and sometimes _doserrno (i.e. GetLastError()),
				// the only non-windows error code it sets is EMFILE when the c-runtime fd table is full.
				if (errno == EMFILE) {
					err = ERROR_TOO_MANY_OPEN_FILES;
				} else {
					err = _doserrno;
					if (err == NO_ERROR) err = ERROR_INVALID_FUNCTION; // make sure we get an error code
				}
			}
		}

		if (file.fd < 0) {
			dprintf( D_ALWAYS,
					 "WriteEventLog::openFile "
						 "CreateFile/_open_osfhandle(\"%s\") failed - err %d (%s)\n",
					 file.path.c_str(),
					 err,
					 GetLastErrorString(err) );
			return false;
		}

		return true;
	}
#endif
	mode_t mode = 0664;
	file.fd = safe_open_wrapper_follow( file.path.c_str(), flags, mode );
	if (file.fd < 0) {
		dprintf( D_ALWAYS,
					"WriteEventLog::openFile "
						"safe_open_wrapper(\"%s\") failed - errno %d (%s)\n",
					file.path.c_str(),
					errno,
					strerror(errno) );
		return false;
	}

	return true;
}

// Return false on error, true on goodness
bool WriteEventLog::writeEvent (const ULogEvent &event)
{
	// the the log is not initialized, don't bother --- just return OK
	if ( !m_initialized ) {
		return true;
	}

	// make certain some parameters we will need are initialized
	if (event.eventNumber < ULOG_EP_FIRST) {
		return false;
	}

	// write ulog event
	bool ret = true;
	if (m_enabled) {
		for (int ix = 0; ix < 1; ++ix) // loop over multiple logs someday?
		{
			log_file & log = m_log;

				// Check our mask vector for the event
				// If we have a mask, the event must be in the mask to write the event.
			if ( ! m_select_events.empty()){
				int index = event.eventNumber - ULOG_EP_FIRST;
				if ((int)m_select_events.size() >= index || ! m_select_events[index]) {
					dprintf( D_FULLDEBUG, "Did not find %d in the selection mask, so do not write this event.\n",
						event.eventNumber );
					break; // We are done caring about this event
				}
			}
			if ( ! m_hide_events.empty()) {
				int index = event.eventNumber - ULOG_EP_FIRST;
				if ((int)m_hide_events.size() < index && m_hide_events[index]) {
					dprintf( D_FULLDEBUG, "Event %d is in the hide mask, so do not write this event.\n",
						event.eventNumber );
					break; // We are done caring about this event
				}
			}

			int fmt_opts = m_format_opts;
			if ( ! writeEventToFile(event, m_log, fmt_opts) ) {
				dprintf( D_ALWAYS, "WARNING: WriteUserLog::writeEvent user doWriteEvent() failed on normal log %s!\n", log.path.c_str());
				ret = false;
			}
		}
	}

	return ret;
}

bool WriteEventLog::renderEvent(const ULogEvent &event, int format_opts, std::string &output)
{
	bool success = true;

	bool utc_event_time = (format_opts & ULogEvent::formatOpt::UTC) != 0;

	if (format_opts & ULogEvent::formatOpt::CLASSAD) {

		ClassAd eventAd;
		
		if ( ! event.toClassAd(eventAd, utc_event_time)) {
			dprintf( D_ALWAYS,
				"WriteUserLog Failed to convert event type # %d to classAd.\n",
				event.eventNumber);
			success = false;
		} else {
			if (format_opts & ULogEvent::formatOpt::JSON) {
				classad::ClassAdJsonUnParser  unparser;
				unparser.Unparse(output, &eventAd);
				if ( ! output.empty()) output += "\n";
			} else /*if (format_opts & ULogEvent::formatOpt::XML)*/ {
				classad::ClassAdXMLUnParser unparser;
				unparser.SetCompactSpacing(false);
				unparser.Unparse(output, &eventAd);
			}

			if (output.empty()) {
				dprintf( D_ALWAYS,
					"WriteUserLog Failed to convert event type # %d to %s.\n",
					event.eventNumber,
					(format_opts & ULogEvent::formatOpt::JSON) ? "JSON" : "XML");
			}
			success = true;
		}
	} else {
		success = const_cast<ULogEvent &>(event).formatEvent(output, format_opts);
		if (success) { output += SynchDelimiter; }
	}
	return success;
}

bool WriteEventLog::writeEventToFile (const ULogEvent &event, log_file & log, int format_opts)
{
	if (log.fd < 0) openFile(log);
	checkLogRotation(log, event.GetEventTime());

	std::string output;
	bool success = renderEvent(event, format_opts, output);

	if (success && write(log.fd, output.data(), output.length()) < (ssize_t)output.length()) {
		success = false;
	}

	if (success && log.should_fsync) {
		if (condor_fdatasync(log.fd, log.path.c_str()) != 0) {
			// TODO: report ??
		}
	}

	return success;
}


bool WriteEventLog::checkLogRotation (log_file & log, time_t timestamp)
{
	if (log.fd < 0)
		return false;

	long long length = 0; // this gets assigned return value from lseek()
#ifdef WIN32
	length = _lseeki64(log.fd, 0, SEEK_END);
#elif Linux
	length = lseek64(log.fd, 0, SEEK_END);
#else
	length = lseek(log.fd, 0, SEEK_END);
#endif
	if (length < 0) {
		//debug_close_file(it);
		//save_errno = errno;
		//snprintf( msg_buf, sizeof(msg_buf), "Can't seek to end of DebugFP file\n" );
		//_condor_dprintf_exit( save_errno, msg_buf );
		// 
		// TODO: do we quit here? exit the process??
		length = 0;
	}

	//This is checking for a non-zero max length.  Zero is infinity.
	if (log.max_log && length > 0 && length > log.max_log) {
		preserve_log_file(log, timestamp);
		return true;
	}

	return false;
}

/*
** Copy the log file to a backup, then truncate the current one.
*/
int
WriteEventLog::preserve_log_file(log_file & it, time_t now)
{
	bool failed_to_rotate = FALSE;
	int max_log_num = 2; // TODO: make this settable (per log?)

	(void)setBaseName(it.path.c_str());
	const char *extension = createRotateFilename(NULL, max_log_num, now);
	(void)close(it.fd); it.fd = -1;

	int result = rotateTimestamp(extension, max_log_num, now);

#if defined(WIN32)
	if (result != 0) { // MoveFileEx and Copy failed
		failed_to_rotate = TRUE;
	}
#else

	errno = 0;
	if (result != 0) {
		failed_to_rotate = TRUE;
		if( result == ENOENT /* && !DebugLock */) {
			/* This can happen if we are not using file locking,
			and two processes try to rotate this log file at the
			same time.  The other process must have already done
			the rename but not created the new log file yet.
			*/
		} else {
			// TODO: something here? the dprintf code would kill the process
		}
	}

#endif


	if (failed_to_rotate) {
	#ifdef WIN32
		const char * reason_hint = "Perhaps another process is keeping log files open?";
	#else
		const char * reason_hint = "perhaps another process rotated the file at the same time?";
	#endif
		dprintf(D_ERROR, "WARNING: Error %d rotating event log %s %s\n",result,it.path.c_str(),reason_hint);
	}

	if (it.fd < 0) {
		openFile(it);
	}

	cleanUpOldLogFiles(max_log_num);

	return it.fd;
}



