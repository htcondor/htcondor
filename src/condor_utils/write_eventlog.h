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

#ifndef __WRITE_EVENTLOG_H__
#define __WRITE_EVENTLOG_H__

#include "condor_event.h"


// base class for event logs that are not job event logs
// do not use this class for the UserLog,  this is for other types of event log
// the STARTD derives a class from this class that it uses for the EP event log
class WriteEventLog
{
protected:
	struct log_file {
		/** Copy of path to the log file */ std::string path;
		/** The log file                 */ int fd{-1};
		/** Max size of log file         */ int max_log{-1}; // bytes or seconds before we rotate, -1 is don't rotate
		/** rotate by time or by size    */ bool rotate_by_time{false};
		/** Whether to fsync (fdatasync) */ bool should_fsync{false};

		log_file() = default;
		log_file(const log_file &that) = delete;
		~log_file() { reset(""); }
		log_file(log_file&& that) noexcept
			: path(std::move(that.path))
			, fd(that.fd)
			, max_log(that.max_log)
			, rotate_by_time(that.rotate_by_time)
			, should_fsync(that.should_fsync) {
			that.fd = -1;
		}
		log_file& operator=(log_file&& that) noexcept {
			if (this != &that) {
				if (fd >= 0) {
					reset(that.path);
				} else {
					path = std::move(that.path);
				}
				fd = that.fd;  that.fd = -1;
				max_log = that.max_log;
				rotate_by_time = that.rotate_by_time;
				should_fsync = that.should_fsync;
			}
			return *this;
		}

		//void set_should_fsync(bool v) { should_fsync = v; }
		//bool get_should_fsync() const { return should_fsync; }

		void reset(std::string_view filename) {
			if (fd >= 0) { close(fd); }
			fd = -1;
			path = filename;
		}
	};

public:
	virtual ~WriteEventLog();
	WriteEventLog() = default;
	WriteEventLog(const WriteEventLog&) = delete;

	/** Initialize the log file.
	@param file the path name of the log file to be written (copied)
	@return true on success
	*/
	bool initialize(const char *file, int max_log, int format_opts = 0);

	/** Write an event to the log file.  Caution: if the log file is
	not initialized, then no event will be written, and this function
	will return a successful value.

	@param event the event to be written
	@param was the event actually written (see above caution).
	@return false for failure, true for success
	*/
	bool writeEvent (const ULogEvent &event);

	bool isInitialized() const { return m_initialized; }
	bool isEnabled() const { return m_enabled; }

protected:
	log_file m_log;
	std::vector<bool> m_select_events;
	std::vector<bool> m_hide_events;
	int      m_format_opts{0};
	bool     m_initialized{false};
	bool     m_enabled{false};

	bool openFile(log_file& file);
	bool renderEvent(const ULogEvent &event, int format_opts, std::string &buffer);
	bool writeEventToFile (const ULogEvent &event, log_file & log, int format_opts);
	bool checkLogRotation (log_file & log, time_t timestamp);
	int  preserve_log_file(log_file & log, time_t timestamp);
};


// declare some event classes here rather than in condor_event.h because you can't use
// classad by value in that header file (currently)

// class used for all EP events,  we may use derived classed in the daemons
// to help will code organization, but the parts needed to read and write the event
// objects to the event log should be here.
//
class EPLogEvent : public ULogEvent
{
public:
	EPLogEvent(ULogEPEventNumber en) {
		init(en,0,0);
	};
	~EPLogEvent(void) {};

	/** Read the body of the next event.
	@param file the non-NULL readable log file
	@return 0 for failure, 1 for success
	*/
	virtual int readEvent (ULogFile& file, bool & got_sync_line);

	/** Format the body of this event.
	@param out string to which the formatted text should be appended
	@return false for failure, true for success
	*/
	virtual bool formatBody( std::string &out );

	/** Return a ClassAd representation of this SubmitEvent.
	@return NULL for failure, the ClassAd pointer otherwise
	*/
	virtual ClassAd* toClassAd(bool event_time_utc);

	/** Initialize from this ClassAd.
	@param a pointer to the ClassAd to initialize from
	*/
	virtual void initFromClassAd(ClassAd* ad);

	const std::string & Head() { return head; } // remainder of event line (without trailing \n)
	void setHead(std::string_view head_text);
	ClassAd & Ad() { return payload; }

	bool is(ULogEPEventNumber en, int id, int subid) const {
		return eventNumber == (ULogEventNumber)en && cluster == id && proc == subid;
	}
	void init(ULogEPEventNumber en, int id, int subid) {
		eventNumber = (ULogEventNumber)en;
		cluster = id;
		proc = subid;
		subproc = 0;
		reset_event_time();
		head = eventName()+5;
		payload.Clear();
	}
	void clear() {
		eventNumber = (ULogEventNumber)ULOG_EP_FUTURE_EVENT;
		cluster = proc = 0;
		head.clear();
		payload.Clear();
	}
	bool empty() const {
		return eventNumber == (ULogEventNumber)ULOG_EP_FUTURE_EVENT;
	}

private:
	std::string head; // the remainder of the first line of the event, after the  timestamp (may be empty)
	ClassAd payload; // the body text of the event (may be empty)
};


#endif // __WRITE_EVENTLOG_H__
