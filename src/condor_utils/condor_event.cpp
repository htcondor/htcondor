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
#include <string.h>
#include <errno.h>
#include "condor_event.h"
#include "write_user_log.h"
#include "condor_string.h"
#include "condor_classad.h"
#include "iso_dates.h"
#include "condor_attributes.h"
#include "classad_merge.h"

#include "misc_utils.h"
#include "utc_time.h"
#include "ToE.h"

//added by Ameet
#include "condor_environ.h"
//--------------------------------------------------------
#include "condor_debug.h"
//--------------------------------------------------------

#ifndef WIN32
#include <uuid/uuid.h>
#endif

#include <algorithm>

char* ULogFile::readLine(char * buf, size_t bufsize) {
	if (stashed_line) {
		strncpy(buf, stashed_line, bufsize);
		stashed_line = nullptr;
		return buf;
	}
	return ::fgets(buf, (int)bufsize, fp);
}
bool ULogFile::readLine(std::string& str, bool append) {
	if (stashed_line) {
		if (append) { str += stashed_line; }
		else { str = stashed_line; }
		stashed_line = nullptr;
		return true;
	}
	return ::readLine(str, fp, append);
}
/*
int ULogFile::read_formatted(const char * fmt, va_list args) {
	if (stashed_line) {
		int rval = ::vsscanf(stashed_line, fmt, args);
		if (rval) { stashed_line = nullptr; }
		return rval;
	}
	return ::vfscanf(fp, fmt, args);
}
*/

const char ULogEventNumberNames[][41] = {
	"ULOG_SUBMIT",					// Job submitted
	"ULOG_EXECUTE",					// Job now running
	"ULOG_EXECUTABLE_ERROR",		// Error in executable
	"ULOG_CHECKPOINTED",			// Job was checkpointed
	"ULOG_JOB_EVICTED",				// Job evicted from machine
	"ULOG_JOB_TERMINATED",			// Job terminated
	"ULOG_IMAGE_SIZE",				// Image size of job updated
	"ULOG_SHADOW_EXCEPTION",		// Shadow threw an exception
	"ULOG_GENERIC",
	"ULOG_JOB_ABORTED",  			// Job aborted
	"ULOG_JOB_SUSPENDED",			// Job was suspended
	"ULOG_JOB_UNSUSPENDED",			// Job was unsuspended
	"ULOG_JOB_HELD",  				// Job was held
	"ULOG_JOB_RELEASED",  			// Job was released
	"ULOG_NODE_EXECUTE",  			// MPI (or parallel) Node executing
	"ULOG_NODE_TERMINATED",  		// MPI (or parallel) Node terminated
	"ULOG_POST_SCRIPT_TERMINATED",	// POST script terminated
	"ULOG_GLOBUS_SUBMIT",			// Job Submitted to Globus
	"ULOG_GLOBUS_SUBMIT_FAILED",	// Globus Submit Failed
	"ULOG_GLOBUS_RESOURCE_UP",		// Globus Machine UP
	"ULOG_GLOBUS_RESOURCE_DOWN",	// Globus Machine Down
	"ULOG_REMOTE_ERROR",            // Remote Error
	"ULOG_JOB_DISCONNECTED",        // RSC socket lost
	"ULOG_JOB_RECONNECTED",         // RSC socket re-established
	"ULOG_JOB_RECONNECT_FAILED",    // RSC reconnect failure
	"ULOG_GRID_RESOURCE_UP",		// Grid machine UP
	"ULOG_GRID_RESOURCE_DOWN",		// Grid machine Down
	"ULOG_GRID_SUBMIT",				// Job submitted to grid resource
	"ULOG_JOB_AD_INFORMATION",		// Job Ad information update
	"ULOG_JOB_STATUS_UNKNOWN",		// Job status unknown
	"ULOG_JOB_STATUS_KNOWN",		// Job status known
	"ULOG_JOB_STAGE_IN",			// Job staging in input files
	"ULOG_JOB_STAGE_OUT",			// Job staging out output files
	"ULOG_ATTRIBUTE_UPDATE",		// Job attribute updated
	"ULOG_PRESKIP",					// PRE_SKIP event for DAGMan
	"ULOG_CLUSTER_SUBMIT",			// Cluster submitted
	"ULOG_CLUSTER_REMOVE", 			// Cluster removed
	"ULOG_FACTORY_PAUSED",			// Factory paused
	"ULOG_FACTORY_RESUMED",			// Factory resumed
	"ULOG_NONE",					// None (try again later)
	"ULOG_FILE_TRANSFER",			// File transfer
	"ULOG_RESERVE_SPACE",			// Space reserved
	"ULOG_RELEASE_SPACE",			// Space released
	"ULOG_FILE_COMPLETE",			// File transfer has completed successfully
	"ULOG_FILE_USED",				// File in reuse dir utilized
	"ULOG_FILE_REMOVED",			// File in reuse dir removed.
};

const char * const ULogEventOutcomeNames[] = {
  "ULOG_OK       ",
  "ULOG_NO_EVENT ",
  "ULOG_RD_ERROR ",
  "ULOG_MISSED_EVENT ",
  "ULOG_UNK_ERROR"
};


ULogEvent *
instantiateEvent (ClassAd *ad)
{
	ULogEvent *event;
	int eventNumber;
	if(!ad->LookupInteger("EventTypeNumber",eventNumber)) return NULL;

	event = instantiateEvent((ULogEventNumber)eventNumber);
	if(event) event->initFromClassAd(ad);
	return event;
}

ULogEvent *
instantiateEvent (ULogEventNumber event)
{
	switch (event)
	{
	  case ULOG_SUBMIT:
		return new SubmitEvent;

	  case ULOG_EXECUTE:
		return new ExecuteEvent;

	  case ULOG_EXECUTABLE_ERROR:
		return new ExecutableErrorEvent;

	  case ULOG_CHECKPOINTED:
		return new CheckpointedEvent;

	  case ULOG_JOB_EVICTED:
		return new JobEvictedEvent;

	  case ULOG_JOB_TERMINATED:
		return new JobTerminatedEvent;

	  case ULOG_IMAGE_SIZE:
		return new JobImageSizeEvent;

	  case ULOG_SHADOW_EXCEPTION:
		return new ShadowExceptionEvent;

	  case ULOG_GENERIC:
		return new GenericEvent;

	  case ULOG_JOB_ABORTED:
		return new JobAbortedEvent;

	  case ULOG_JOB_SUSPENDED:
		return new JobSuspendedEvent;

	  case ULOG_JOB_UNSUSPENDED:
		return new JobUnsuspendedEvent;

	  case ULOG_JOB_HELD:
		return new JobHeldEvent;

	  case ULOG_JOB_RELEASED:
		return new JobReleasedEvent;

	  case ULOG_NODE_EXECUTE:
		return new NodeExecuteEvent;

	  case ULOG_NODE_TERMINATED:
		return new NodeTerminatedEvent;

	  case ULOG_POST_SCRIPT_TERMINATED:
		return new PostScriptTerminatedEvent;

	case ULOG_REMOTE_ERROR:
		return new RemoteErrorEvent;

	case ULOG_JOB_DISCONNECTED:
		return new JobDisconnectedEvent;

	case ULOG_JOB_RECONNECTED:
		return new JobReconnectedEvent;

	case ULOG_JOB_RECONNECT_FAILED:
		return new JobReconnectFailedEvent;

	case ULOG_GRID_RESOURCE_DOWN:
		return new GridResourceDownEvent;

	case ULOG_GRID_RESOURCE_UP:
		return new GridResourceUpEvent;

	case ULOG_GRID_SUBMIT:
		return new GridSubmitEvent;

	case ULOG_JOB_AD_INFORMATION:
		return new JobAdInformationEvent;

	case ULOG_JOB_STATUS_UNKNOWN:
		return new JobStatusUnknownEvent;

	case ULOG_JOB_STATUS_KNOWN:
		return new JobStatusKnownEvent;

	case ULOG_ATTRIBUTE_UPDATE:
		return new AttributeUpdate;

	case ULOG_PRESKIP:
		return new PreSkipEvent;

	case ULOG_CLUSTER_SUBMIT:
		return new ClusterSubmitEvent;

	case ULOG_CLUSTER_REMOVE:
		return new ClusterRemoveEvent;

	case ULOG_FACTORY_PAUSED:
		return new FactoryPausedEvent;

	case ULOG_FACTORY_RESUMED:
		return new FactoryResumedEvent;

	case ULOG_FILE_TRANSFER:
		return new FileTransferEvent;

	case ULOG_RESERVE_SPACE:
		return new ReserveSpaceEvent;

	case ULOG_RELEASE_SPACE:
		return new ReleaseSpaceEvent;

	case ULOG_FILE_COMPLETE:
		return new FileCompleteEvent;

	case ULOG_FILE_USED:
		return new FileUsedEvent;

	case ULOG_FILE_REMOVED:
		return new FileRemovedEvent;

	case ULOG_DATAFLOW_JOB_SKIPPED:
		return new DataflowJobSkippedEvent;

	case ULOG_GLOBUS_SUBMIT:
	case ULOG_GLOBUS_SUBMIT_FAILED:
	case ULOG_GLOBUS_RESOURCE_DOWN:
	case ULOG_GLOBUS_RESOURCE_UP:
	default:
		dprintf( D_ALWAYS, "Unknown ULogEventNumber: %d, reading it as a FutureEvent\n", event );
		return new FutureEvent(event);
	}

    return 0;
}


void setEventUsageAd(const ClassAd& jobAd, ClassAd ** ppusageAd)
{
	std::string resslist;
	if ( ! jobAd.LookupString("ProvisionedResources", resslist))
		resslist = "Cpus, Disk, Memory";

	ClassAd * puAd = nullptr;
	// if() removed, keeping {} to not re-indent enclosed code
	{
		for (const auto& resname: StringTokenIterator(resslist)) {
			if (puAd == nullptr) {
				puAd = new ClassAd();
			}

			std::string attr;
			std::string res = resname;
			title_case(res); // capitalize it to make it print pretty.
			const int copy_ok = classad::Value::ERROR_VALUE | classad::Value::BOOLEAN_VALUE | classad::Value::INTEGER_VALUE | classad::Value::REAL_VALUE;
			classad::Value value;

			attr = res + "Provisioned";	 // provisioned value
			if (jobAd.EvaluateAttr(attr, value, classad::Value::SCALAR_EX_VALUES) && (value.GetType() & copy_ok) != 0) {
				classad::ExprTree * plit = classad::Literal::MakeLiteral(value);
				if (plit) {
					puAd->Insert(resname, plit); // usage ad has attribs like they appear in Machine ad
				}
			}

			attr = "Request"; attr += res;   	// requested value
			if (jobAd.EvaluateAttr(attr, value, classad::Value::SCALAR_EX_VALUES) && (value.GetType() & copy_ok) != 0) {
				classad::ExprTree * plit = classad::Literal::MakeLiteral(value);
				if (plit) {
					puAd->Insert(attr, plit);
				}
			}

			attr = res + "Usage"; // (implicitly) peak usage value
			if (jobAd.EvaluateAttr(attr, value, classad::Value::SCALAR_EX_VALUES) && (value.GetType() & copy_ok) != 0) {
				classad::ExprTree * plit = classad::Literal::MakeLiteral(value);
				if (plit) {
					puAd->Insert(attr, plit);
				}
			}

			attr = res + "AverageUsage"; // average usage
			if (jobAd.EvaluateAttr(attr, value, classad::Value::SCALAR_EX_VALUES) && (value.GetType() & copy_ok) != 0) {
				classad::ExprTree * plit = classad::Literal::MakeLiteral(value);
				if (plit) {
					puAd->Insert(attr, plit);
				}
			}

			attr = res + "MemoryUsage"; // special case for GPUs.
			if (jobAd.EvaluateAttr(attr, value, classad::Value::SCALAR_EX_VALUES) && (value.GetType() & copy_ok) != 0) {
				classad::ExprTree * plit = classad::Literal::MakeLiteral(value);
				if (plit) {
					puAd->Insert(attr, plit);
				}
			}

			attr = res + "MemoryAverageUsage"; // just in case.
			if (jobAd.EvaluateAttr(attr, value, classad::Value::SCALAR_EX_VALUES) && (value.GetType() & copy_ok) != 0) {
				classad::ExprTree * plit = classad::Literal::MakeLiteral(value);
				if (plit) {
					puAd->Insert(attr, plit);
				}
			}

			attr = "Assigned"; attr += res;
			CopyAttribute( attr, *puAd, jobAd );
		}
	}

	if (puAd) {
		// Hard code a couple of useful time-based attributes that are not "Requested" yet
		// and shorten their names to display more reasonably
		int jaed = 0;
		if (jobAd.LookupInteger(ATTR_JOB_ACTIVATION_EXECUTION_DURATION, jaed)) {
			puAd->Assign("TimeExecuteUsage", jaed);
		}
		int jad = 0;
		if (jobAd.LookupInteger(ATTR_JOB_ACTIVATION_DURATION, jad)) {
			puAd->Assign("TimeSlotBusyUsage", jad);
		}
		*ppusageAd = puAd;
	}
}


ULogEvent::ULogEvent(void)
{
	eventNumber = (ULogEventNumber) - 1;
	cluster = proc = subproc = -1;
	eventclock = condor_gettimestamp(event_usec);
}


ULogEvent::~ULogEvent (void)
{
}

/*static*/
ULogEventNumber ULogEvent::readEventNumber(ULogFile& ulf, char* headbuf, size_t bufsize)
{
	ASSERT(bufsize > 32); // header is something like: 000 (16091.000.000) 01/22 12:09:19
	memset(headbuf, 0, 32);

	if ( ! ulf.readLine(headbuf, bufsize)) {
		return ULOG_NO;
	}

	// parse by hand to avoid setting errno
	int num = 0;
	const char * p = headbuf;
	while (*p >= '0' && *p <= '9') { num = num*10 + (*p++ - '0'); }
	
	// the event number should be exactly 3 digits
	if (*p != ' ' || p != headbuf+3) {
		return ULOG_NO;
	}

	return (ULogEventNumber)num;
}


int ULogEvent::getEvent (ULogFile& ulf, const char * header_line, bool & got_sync_line)
{
	const char * post_header = readHeader(header_line);
	if ( ! post_header) {
		return false;
	}
	ulf.stash(post_header);
	return readEvent (ulf, got_sync_line);
}

/*static*/ int ULogEvent::parse_opts(const char * fmt, int default_opts)
{
	int opts = default_opts;

	if (fmt) {
		StringTokenIterator it(fmt);
		for (const char * opt = it.first(); opt != NULL; opt = it.next()) {
			bool bang = *opt == '!'; if (bang) ++opt;
		#define DOOPT(tag,flag) if (YourStringNoCase(tag) == opt) { if (bang) { opts &= ~(flag); } else { opts |= (flag); } }
			DOOPT("XML", formatOpt::XML)
			DOOPT("JSON", formatOpt::JSON)
			DOOPT("ISO_DATE", formatOpt::ISO_DATE)
			DOOPT("UTC", formatOpt::UTC)
			DOOPT("SUB_SECOND", formatOpt::SUB_SECOND)
		#undef DOOPT
			if (YourStringNoCase("LEGACY") == opt) {
				if (bang) {
					// if !LEGACY turn on a reasonable non-legacy default
					opts |= formatOpt::ISO_DATE;
				} else {
					// turn off the non-legacy options
					opts &= ~(formatOpt::ISO_DATE | formatOpt::UTC | formatOpt::SUB_SECOND);
				}
			}
		}
	}

	return opts;
}


bool ULogEvent::formatEvent( std::string &out, int options )
{
	return formatHeader( out, options ) && formatBody( out );
}

const char * getULogEventNumberName(ULogEventNumber number)
{
	if (number < 0) {
		return NULL;
	}
	if (number < (int)COUNTOF(ULogEventNumberNames)) {
		return ULogEventNumberNames[number];
	}
	return "ULOG_FUTURE_EVENT";
}


const char* ULogEvent::eventName(void) const
{
	return getULogEventNumberName(eventNumber);
}


#if 1

// 000 (16091.000.000) 01/22 12:09:19 Job submitted from host: 
//    ^---- header ------------------^

const char * ULogEvent::readHeader(const char * p)
{
	struct tm dt;
	bool is_utc;

	// check to see if we are pointing to the event number, and skip over it if so.
	if (p[0] == '0' && p[1] && p[2] && p[3] == ' ') { p +=3 ; }

	if (p[0] != ' ' || p[1] != '(') return nullptr;
	p += 2;
	char * endp = nullptr;
	cluster = strtol(p, &endp, 10);
	if (*endp != '.') return nullptr;
	p = endp+1;
	proc = strtol(p, &endp, 10);
	if (*endp != '.') return nullptr;
	p = endp+1;
	subproc = strtol(p, &endp, 10);
	if (*endp != ')') return nullptr;
	p = endp+1;

	// space before date
	if (p[0] != ' ') return nullptr;
	++p;

	// scanning for the next space should either end up pointing to the space after the MM/DD
	// or to the space after the iso8601 date and time.
	const char * datend = strchr(p, ' ');
	if ( ! datend) return nullptr;

	// if we read a / in position 2 of the date, then this must be
	// a date of the form MM/DD, which is not iso8601. we use the old
	// (broken) parsing algorithm to parse it, but we can use the iso time parsing code
	//
	if (isdigit(p[0]) && isdigit(p[1]) && p[2] == '/') {
		// here we expect a date of the form MM/DD HH:MM:SS with a space between time and date
		// datend therefore should point to the space between the date and the time
		if (datend != p+5) return nullptr;
		++datend;

		// we can parse the time part as iso8601
		// this will set -1 into all fields of dt that are not set by time parsing code
		iso8601_to_time(datend, &dt, &event_usec, &is_utc);

		// we parse the date part as two int fields separated by '/'
		dt.tm_mon = atoi(p);
		if (dt.tm_mon <= 0) { // this detects completely garbage dates because atoi returns 0 if there are no numbers to parse
			return nullptr;
		}
		dt.tm_mon -= 1; // recall that tm_mon+1 was written to log
		dt.tm_mday = atoi(p+3);

		// now scan for the space after the time, so we can return that
		datend = strchr(datend, ' ');
	} else if (datend == p+10) {
		// here we expect the date and time to be in iso8601 format
		// but the T between date and time is missing.  this is the usual case
		char datebuf[10 + 1 + 21 + 3]; // longest date is YYYY-MM-DD  = 4+3+3 = 10, longest time is HH:MM:SS.uuuuuu+hh:mm = 7*3 = 21
		strncpy(datebuf, p, sizeof(datebuf)-1);
		datebuf[sizeof(datebuf)-1] = 0;
		// date must be in the form of YYYY-MM-DD and Time must begin at position 11
		// so we can turn this into a full iso8601 datetime by stuffing a T inbetween
		datebuf[10] = 'T';
		// this will set -1 into all fields of dt that are not set by date/time parsing code
		iso8601_to_time(datebuf, &dt, &event_usec, &is_utc);
		// now scan for the space after the time, so we can return that
		datend = strchr(datend+1, ' ');
	} else {
		// here we expect the date and time to be in iso8601 format
		// this will set -1 into all fields of dt that are not set by date/time parsing code
		iso8601_to_time(p, &dt, &event_usec, &is_utc);
	}

	// check for a bogus date stamp
	if (dt.tm_mon < 0 || dt.tm_mon > 11 || dt.tm_mday < 0 || dt.tm_mday > 32 || dt.tm_hour < 0 || dt.tm_hour > 24) {
		return nullptr;
	}

	// force mktime or gmtime to figure out daylight savings time
	dt.tm_isdst = -1;

	// use the year of the current timestamp when we dont get an iso8601 date
	// This is wrong, but consistent with pre 8.8.2 behavior see #6936
	if (dt.tm_year < 0) {
		dt.tm_year = localtime(&eventclock)->tm_year;
		// TODO: adjust the year as necessary so we don't get timestamps in the future
	}

	// Need to set eventclock here, otherwise eventclock and
	// eventTime will not match!!  (See gittrac #5468.)
	if (is_utc) {
		eventclock = timegm(&dt);
	} else {
		eventclock = mktime(&dt);
	}

	if (datend && datend[0] == ' ') ++datend;
	return datend;
}
#else
// This function reads in the header of an event from the UserLog and fills
// the fields of the event object.  It does *not* read the event number.  The
// caller is supposed to read the event number, instantiate the appropriate
// event type using instantiateEvent(), and then call readEvent() of the
// returned event.
int
ULogEvent::readHeader (FILE *file)
{
	struct tm dt;
	bool is_utc;
	char datebuf[10 + 1 + 21 + 3]; // longest date is YYYY-MM-DD  = 4+3+3 = 10
	                               // longest time is HH:MM:SS.uuuuuu+hh:mm = 7*3 = 21
	datebuf[2] = 0; // make sure that there is no / position 2
	int retval = fscanf(file, " (%d.%d.%d) %10s %23s ", &cluster, &proc, &subproc, datebuf, &datebuf[11]);
	if (retval != 5) {
		// try a full iso8601 date-time before we give up
		retval = fscanf(file, " (%d.%d.%d) %10sT%23s ", &cluster, &proc, &subproc, datebuf, &datebuf[11]);
		if (retval != 5) {
			return 0;
		}
	}
	// now parse datebuf and timebuf into event time
	is_utc = false;

	// if we read a / in position 2 of the datebuf, then this must be
	// a date of the form MM/DD, which is not iso8601. we use the old
	// (broken) parsing algorithm to parse it
	if (datebuf[2] == '/') {
		// this will set -1 into all fields of dt that are not set by time parsing code
		iso8601_to_time(&datebuf[11], &dt, &event_usec, &is_utc);
		dt.tm_mon = atoi(datebuf);
		if (dt.tm_mon <= 0) { // this detects completely garbage dates because atoi returns 0 if there are no numbers to parse
			return 0;
		}
		dt.tm_mon -= 1; // recall that tm_mon+1 was written to log
		dt.tm_mday = atoi(datebuf + 3);
	} else {
		// date must be in the form of YYYY-MM-DD and Time must begin at position 11
		// so we can turn this into a full iso8601 datetime by stuffing a T inbetween
		datebuf[10] = 'T';
		// this will set -1 into all fields of dt that are not set by date/time parsing code
		iso8601_to_time(datebuf, &dt, &event_usec, &is_utc);
	}
	// check for a bogus date stamp
	if (dt.tm_mon < 0 || dt.tm_mon > 11 || dt.tm_mday < 0 || dt.tm_mday > 32 || dt.tm_hour < 0 || dt.tm_hour > 24) {
		return 0;
	}
	// force mktime or gmtime to figure out daylight savings time
	dt.tm_isdst = -1;

	// use the year of the current timestamp when we dont get an iso8601 date
	// This is wrong, but consistent with pre 8.8.2 behavior see #6936
	if (dt.tm_year < 0) {
		dt.tm_year = localtime(&eventclock)->tm_year;
		// TODO: adjust the year as necessary so we don't get timestamps in the future
	}

	// Need to set eventclock here, otherwise eventclock and
	// eventTime will not match!!  (See gittrac #5468.)
	if (is_utc) {
		eventclock = timegm(&dt);
	} else {
		eventclock = mktime(&dt);
	}

	return 1;
}
#endif

bool ULogEvent::formatHeader( std::string &out, int options )
{
	out.reserve(1024);

	// print event number and job id.
	int retval = formatstr_cat(out, "%03d (%03d.%03d.%03d) ", eventNumber, cluster, proc, subproc);
	if (retval < 0)
		return false;

	// print date and time
	const struct tm * lt;
	if (options & formatOpt::UTC) {
		lt = gmtime(&eventclock);
	} else {
		lt = localtime(&eventclock);
	}
	if (options & formatOpt::ISO_DATE) {
		formatstr_cat(out, "%04d-%02d-%02d %02d:%02d:%02d",
			lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday,
			lt->tm_hour, lt->tm_min,
			lt->tm_sec);

	} else {
		retval = formatstr_cat(out,
			"%02d/%02d %02d:%02d:%02d",
			lt->tm_mon + 1, lt->tm_mday,
			lt->tm_hour, lt->tm_min,
			lt->tm_sec);
	}
	if (options & formatOpt::SUB_SECOND) formatstr_cat(out, ".%03d", (int)(event_usec / 1000));
	if (options & formatOpt::UTC) out += "Z";
	out += " ";

	// check if all fields were sucessfully written
	return retval >= 0;
}

bool ULogEvent::is_sync_line(const char * line)
{
	if (line[0] == '.' && line[1] == '.' && line[2] == '.') {
		line += 3;
		if ( ! line[0]) {
			return true; // if the line was chomp()ed it ends here.
		}
		if (line[0] == '\r') { ++line; }
		if (line[0] == '\n' && line[1] == '\0') {
			return true;
		}
	}
	return false;
}

// read a line into the supplied buffer and return true if there was any data read
// and the data is not a sync line, if return value is false got_sync_line will be set accordingly
// if chomp is true, trailing \r and \n will be changed to \0
bool ULogEvent::read_optional_line(ULogFile& file, bool & got_sync_line, char * buf, size_t bufsize, bool chomp /*=true*/, bool trim /*=false*/)
{
	buf[0] = 0;
	if( ! file.readLine(buf, (int)bufsize)) {
		return false;
	}
	if (is_sync_line(buf)) {
		got_sync_line = true;
		return false;
	}
	// if we didn't read a \n, we had a truncated read
	int len = (int)strlen(buf);
	if (len <= 0 || buf[len-1] != '\n') {
		return false;
	}
	if (trim) { // trim will also chomp
		len = trim_in_place(buf, len);
		buf[len] = 0;
	} else if (chomp) {
		// remove trailing newline and optional \r
		int ix = len-1;
		buf[ix] = 0;
		if (ix >= 1 && buf[ix-1] == '\r') buf[ix-1] = 0;
	}
	return true;
}

bool ULogEvent::read_optional_line(std::string & str, ULogFile& file, bool & got_sync_line, bool want_chomp /*=true*/, bool want_trim /*false*/)
{
	if ( ! readLine(str, file, false)) {
		return false;
	}
	if (is_sync_line(str.c_str())) {
		str.clear();
		got_sync_line = true;
		return false;
	}
	if (want_chomp) { chomp(str); }
	if (want_trim) { trim(str); }
	return true;
}

bool ULogEvent::read_line_value(const char * prefix, std::string & val, ULogFile& file, bool & got_sync_line, bool want_chomp /*=true*/)
{
	val.clear();
	std::string str;
	if ( ! readLine(str, file, false)) {
		return false;
	}
	if (is_sync_line(str.c_str())) {
		got_sync_line = true;
		return false;
	}
	if (want_chomp) { chomp(str); }
	size_t prefix_len = strlen(prefix);
	if (strncmp(str.c_str(), prefix, prefix_len) == 0) {
		val = str.substr(prefix_len);
		return true;
	}
	return false;
}

// store a string into an event member, while converting all \n to | and \r to space
void ULogEvent::set_reason_member(std::string & reason_out, const std::string & reason_in)
{
	if (reason_in.empty()) {
		reason_out.clear();
		return;
	}

	reason_out.resize(reason_in.size(), 0);
	std::transform(reason_in.begin(), reason_in.end(), reason_out.begin(),
		[](char ch) -> char {
			if (ch == '\n') return '|';
			if (ch == '\r') return ' ';
			return ch;
		}
	);
}

#if 0 // not currently used
int ULogEvent::read_formatted(ULogFile& file, const char * fmt, ...)
{
	int num;
	va_list args;
	va_start(args, fmt);
	num = file.read_formatted(fmt, args);
	va_end(args);
	return num;
}
#endif

ClassAd*
ULogEvent::toClassAd(bool event_time_utc)
{
	ClassAd* myad = new ClassAd;

	if( eventNumber >= 0 ) {
		if ( !myad->InsertAttr("EventTypeNumber", eventNumber) ) {
			delete myad;
			return NULL;
		}
	}

	switch( (ULogEventNumber) eventNumber )
	{
	  case ULOG_SUBMIT:
		SetMyTypeName(*myad, "SubmitEvent");
		break;
	  case ULOG_EXECUTE:
		SetMyTypeName(*myad, "ExecuteEvent");
		break;
	  case ULOG_EXECUTABLE_ERROR:
		SetMyTypeName(*myad, "ExecutableErrorEvent");
		break;
	  case ULOG_CHECKPOINTED:
		SetMyTypeName(*myad, "CheckpointedEvent");
		break;
	  case ULOG_JOB_EVICTED:
		SetMyTypeName(*myad, "JobEvictedEvent");
		break;
	  case ULOG_JOB_TERMINATED:
		SetMyTypeName(*myad, "JobTerminatedEvent");
		break;
	  case ULOG_IMAGE_SIZE:
		SetMyTypeName(*myad, "JobImageSizeEvent");
		break;
	  case ULOG_SHADOW_EXCEPTION:
		SetMyTypeName(*myad, "ShadowExceptionEvent");
		break;
	  case ULOG_GENERIC:
		SetMyTypeName(*myad, "GenericEvent");
		break;
	  case ULOG_JOB_ABORTED:
		SetMyTypeName(*myad, "JobAbortedEvent");
		break;
	  case ULOG_JOB_SUSPENDED:
		SetMyTypeName(*myad, "JobSuspendedEvent");
		break;
	  case ULOG_JOB_UNSUSPENDED:
		SetMyTypeName(*myad, "JobUnsuspendedEvent");
		break;
	  case ULOG_JOB_HELD:
		SetMyTypeName(*myad, "JobHeldEvent");
		break;
	  case ULOG_JOB_RELEASED:
		SetMyTypeName(*myad, "JobReleaseEvent");
		break;
	  case ULOG_NODE_EXECUTE:
		SetMyTypeName(*myad, "NodeExecuteEvent");
		break;
	  case ULOG_NODE_TERMINATED:
		SetMyTypeName(*myad, "NodeTerminatedEvent");
		break;
	  case ULOG_POST_SCRIPT_TERMINATED:
		SetMyTypeName(*myad, "PostScriptTerminatedEvent");
		break;
	case ULOG_REMOTE_ERROR:
		SetMyTypeName(*myad, "RemoteErrorEvent");
		break;
	case ULOG_JOB_DISCONNECTED:
		SetMyTypeName(*myad, "JobDisconnectedEvent");
		break;
	case ULOG_JOB_RECONNECTED:
		SetMyTypeName(*myad, "JobReconnectedEvent");
		break;
	case ULOG_JOB_RECONNECT_FAILED:
		SetMyTypeName(*myad, "JobReconnectFailedEvent");
		break;
	case ULOG_GRID_RESOURCE_UP:
		SetMyTypeName(*myad, "GridResourceUpEvent");
		break;
	case ULOG_GRID_RESOURCE_DOWN:
		SetMyTypeName(*myad, "GridResourceDownEvent");
		break;
	case ULOG_GRID_SUBMIT:
		SetMyTypeName(*myad, "GridSubmitEvent");
		break;
	case ULOG_JOB_AD_INFORMATION:
		SetMyTypeName(*myad, "JobAdInformationEvent");
		break;
	case ULOG_ATTRIBUTE_UPDATE:
		SetMyTypeName(*myad, "AttributeUpdateEvent");
		break;
	case ULOG_CLUSTER_SUBMIT:
		SetMyTypeName(*myad, "ClusterSubmitEvent");
		break;
	case ULOG_CLUSTER_REMOVE:
		SetMyTypeName(*myad, "ClusterRemoveEvent");
		break;
	case ULOG_FACTORY_PAUSED:
		SetMyTypeName(*myad, "FactoryPausedEvent");
		break;
	case ULOG_FACTORY_RESUMED:
		SetMyTypeName(*myad, "FactoryResumedEvent");
		break;
	case ULOG_FILE_TRANSFER:
		SetMyTypeName(*myad, "FileTransferEvent");
		break;
	case ULOG_RESERVE_SPACE:
		SetMyTypeName(*myad, "ReserveSpaceEvent");
		break;
	case ULOG_RELEASE_SPACE:
		SetMyTypeName(*myad, "ReleaseSpaceEvent");
		break;
	case ULOG_FILE_COMPLETE:
		SetMyTypeName(*myad, "FileCompleteEvent");
		break;
	case ULOG_FILE_USED:
		SetMyTypeName(*myad, "FileUsedEvent");
		break;
	case ULOG_FILE_REMOVED:
		SetMyTypeName(*myad, "FileRemovedEvent");
		break;
	case ULOG_DATAFLOW_JOB_SKIPPED:
		SetMyTypeName(*myad, "DataflowJobSkippedEvent");
		break;
	case ULOG_GLOBUS_SUBMIT:
	case ULOG_GLOBUS_SUBMIT_FAILED:
	case ULOG_GLOBUS_RESOURCE_UP:
	case ULOG_GLOBUS_RESOURCE_DOWN:
	default:
		SetMyTypeName(*myad, "FutureEvent");
		break;
	}

	struct tm eventTime;
	if (event_time_utc) {
		gmtime_r(&eventclock, &eventTime);
	} else {
		localtime_r(&eventclock, &eventTime);
	}
	// print the time, including milliseconds if the event_usec is non-zero
	char str[ISO8601_DateAndTimeBufferMax];
	unsigned int sub_sec = event_usec / 1000;
	int sub_digits = event_usec ? 3 : 0;
	time_to_iso8601(str, eventTime, ISO8601_ExtendedFormat,
		ISO8601_DateAndTime, event_time_utc, sub_sec, sub_digits);
	if ( !myad->InsertAttr("EventTime", str) ) {
		delete myad;
		return NULL;
	}

	if( cluster >= 0 ) {
		if( !myad->InsertAttr("Cluster", cluster) ) {
			delete myad;
			return NULL;
		}
	}

	if( proc >= 0 ) {
		if( !myad->InsertAttr("Proc", proc) ) {
			delete myad;
			return NULL;
		}
	}

	if( subproc >= 0 ) {
		if( !myad->InsertAttr("Subproc", subproc) ) {
			delete myad;
			return NULL;
		}
	}

	return myad;
}

void
ULogEvent::initFromClassAd(ClassAd* ad)
{
	if( !ad ) return;
	int en;
	if ( ad->LookupInteger("EventTypeNumber", en) ) {
		eventNumber = (ULogEventNumber) en;
	}
	std::string timestr;
	if( ad->LookupString("EventTime", timestr) ) {
		bool is_utc = false;
		struct tm eventTime;
		iso8601_to_time(timestr.c_str(), &eventTime, &event_usec, &is_utc);
		if (is_utc) {
			eventclock = timegm(&eventTime);
		} else {
			eventclock = mktime(&eventTime);
		}
	}
	ad->LookupInteger("Cluster", cluster);
	ad->LookupInteger("Proc", proc);
	ad->LookupInteger("Subproc", subproc);
}


// class is used to build up Usage/Request/Allocated table for each
// Partitionable resource before we print out the table.
class SlotResTermSumy {
	public:
		std::string use;
		std::string req;
		std::string alloc;
		std::string assigned;
};

typedef std::map<std::string, SlotResTermSumy, classad::CaseIgnLTStr > UsageMap;

static bool is_bare_integer( const std::string & s ) {
	if( s.empty() ) { return false; }

	const char * str = s.c_str();
	while (isdigit(*str)) { ++str; }
	return *str == '\0';
}

//
// The usage ClassAd contains attributes that match the pattern "<RES>",
// "Request<RES>", "<RES>Usage", "<RES>AverageUsage",
// where <RES> can be CPUs, Disk, Memory, or another resource as defined by
// the ProvisionedResources attribute.  Note that this function does NOT use
// the contents of that attribute.
//
static void formatUsageAd( std::string &out, ClassAd * pusageAd )
{
	if (! pusageAd) { return; }

	classad::ClassAdUnParser unp;
	unp.SetOldClassAd( true, true );

	UsageMap useMap;
	bool reqHFP = false, assignedHFP = false, allocHFP = false, useHFP = false;
	for( auto iter = pusageAd->begin(); iter != pusageAd->end(); ++iter ) {
		// Compute the string value.
		std::string value;
		classad::Value cVal;
		double rVal, iVal;

		bool hasFractionalPart = false;
		if( ExprTreeIsLiteral(iter->second, cVal) && cVal.IsRealValue(rVal) ) {
			// show fractional values only if there actually *are* fractional
			// values; format doubles with %.2f
			if( modf( rVal, &iVal ) > 0.0 ) {
				formatstr( value, "%.2f", rVal );
				hasFractionalPart = true;
			} else {
				formatstr( value, "%lld", (long long)iVal);
			}
		} else {
			unp.Unparse( value, iter->second );
		}

		// Determine the attribute name.
		std::string resourceName;
		std::string attributeName = iter->first;
		if( starts_with( attributeName, "Request" ) ) {
			resourceName = attributeName.substr(7);
			useMap[resourceName].req = value;
			reqHFP |= hasFractionalPart;
		} else if( starts_with( attributeName, "Assigned" ) ) {
			resourceName = attributeName.substr(8);
			useMap[resourceName].assigned = value;
			assignedHFP = hasFractionalPart;
		} else if( ends_with( attributeName, "AverageUsage" ) ) {
			resourceName = attributeName.substr( 0, attributeName.size() - 12 );
			useMap[resourceName].use = value;
			useHFP |= hasFractionalPart;
		} else if( ends_with( attributeName, "Usage" ) ) {
			resourceName = attributeName.substr( 0, attributeName.size() - 5 );
			useMap[resourceName].use = value;
			useHFP |= hasFractionalPart;
		} else {
			resourceName = attributeName;
			useMap[resourceName].alloc = value;
			allocHFP |= hasFractionalPart;
		}

		if( resourceName.empty() ) {
			formatstr_cat( out, "\t%s = %s\n", iter->first.c_str(), value.c_str() );
		}
	}
	if( useMap.empty() ) { return; }

	//
	// Compute column widths (and fix up 'alloc' entries, I guess).
	// Pad integer values to align with the floating point dots.
	//
	int cchRes = sizeof("Memory (MB)"), cchUse = 8, cchReq = 8, cchAlloc = 0, cchAssigned = 0;
	for( auto & i : useMap ) {
		SlotResTermSumy & psumy = i.second;
		if( psumy.alloc.empty() ) {
			classad::ExprTree * tree = pusageAd->Lookup(i.first);
			if( tree ) { unp.Unparse( psumy.alloc, tree ); }
		}

		if( useHFP && is_bare_integer(psumy.use) ) { psumy.use += "   "; }
		if( reqHFP && is_bare_integer(psumy.req) ) { psumy.req += "   "; }
		if( allocHFP && is_bare_integer(psumy.alloc) ) { psumy.alloc += "   "; }
		if( assignedHFP && is_bare_integer(psumy.assigned) ) { psumy.assigned += "   "; }

		cchRes = MAX(cchRes, (int)i.first.size());
		cchUse = MAX(cchUse, (int)psumy.use.size());
		cchReq = MAX(cchReq, (int)psumy.req.size());
		cchAlloc = MAX(cchAlloc, (int)psumy.alloc.size());
		cchAssigned = MAX(cchAssigned, (int)psumy.assigned.size());
	}

	// Print table header.
	std::string fString;
	formatstr( fString, "\tPartitionable Resources : %%%ds %%%ds %%%ds %%s\n",
		cchUse, cchReq, MAX(cchAlloc, 9) );
	formatstr_cat( out, fString.c_str(), "Usage", "Request",
		 cchAlloc ? "Allocated" : "", cchAssigned ? "Assigned" : "" );

	// Print table.
	formatstr( fString, "\t   %%-%ds : %%%ds %%%ds %%%ds %%s\n",
		cchRes + 8, cchUse, cchReq, MAX(cchAlloc, 9) );
	for( const auto & i : useMap ) {
		if( i.first.empty() ) { continue; }

		std::string label = i.first;
		if( label == "Memory" ) { label += " (MB)"; }
		else if( label == "Disk" ) { label += " (KB)"; }
		// It would be nice if this weren't a special case, but we don't
		// have a way of representing a single resource with multiple metrics.
		else if( label == "Gpus" ) { label += " (Average)"; }
		else if( label == "GpusMemory" ) { label += " (MB)"; }
		else if( label == "TimeExecute" ) { label += " (s)"; }
		else if( label == "TimeSlotBusy" ) { label += " (s)"; }
		const SlotResTermSumy & psumy = i.second;
		formatstr_cat( out, fString.c_str(), label.c_str(), psumy.use.c_str(),
			psumy.req.c_str(), psumy.alloc.c_str(), psumy.assigned.c_str() );
	}
}

class UsageLineParser {
public:
	UsageLineParser() : ixColon(-1), ixUse(-1), ixReq(-1), ixAlloc(-1), ixAssigned(-1) {}

	// use header to determine column widths
	void init(const char * sz) {
		const char * pszColon = strchr(sz, ':');
		if ( ! pszColon) 
			ixColon = 0;
		else
			ixColon = (int)(pszColon - sz);
		const char * pszTbl = &sz[ixColon+1]; // pointer to the usage table
		const char * psz = pszTbl;
		while (*psz == ' ') ++psz;         // skip spaces
		while (*psz && *psz != ' ') ++psz; // skip "Usage"
		ixUse = (int)(psz - pszTbl)+1;     // save right edge of Usage
		while (*psz == ' ') ++psz;         // skip spaces
		while (*psz && *psz != ' ') ++psz; // skip "Request"
		ixReq = (int)(psz - pszTbl)+1;     // save right edge of Request
		while (*psz == ' ') ++psz;         // skip spaces
		if (*psz) {                        // if there is an "Allocated"
			const char *p = strstr(psz, "Allocated");
			if (p) {
				ixAlloc = (int)(p - pszTbl)+9; // save right edge of Allocated
				p = strstr(p, "Assigned");
				if (p) { // if there is an "Assigned"
					ixAssigned = (int)(p - pszTbl); // save *left* edge of assigned (it gets the remaineder of the line)
				}
			}
		}
	}

	void Parse(const char * sz, ClassAd * puAd) const {
		std::string tag;

		// parse out resource tag
		const char * p = sz;
		while (*p == '\t' || *p == ' ') ++p;
		const char * e = p;
		while (*e && (*e != ' ' && *e != ':')) ++e;
		tag.assign(p, e-p);

		// setup a pointer to the usage table by scanning for the :
		const char * pszTbl = strchr(e, ':');
		if ( ! pszTbl)
			return;
		++pszTbl;

		std::string attrn;
		std::string exprstr;

		//pszTbl[ixUse] = 0;
		// Insert <Tag>Usage = <value1>
		attrn = tag;
		attrn += "Usage";
		exprstr.assign(pszTbl, ixUse);
		puAd->AssignExpr(attrn, exprstr.c_str());

		//pszTbl[ixReq] = 0;
		// Insert Request<Tag> = <value2>
		attrn = "Request";
		attrn += tag;
		exprstr.assign(&pszTbl[ixUse], ixReq-ixUse);
		puAd->AssignExpr(attrn, exprstr.c_str());

		if (ixAlloc > 0) {
			//pszTbl[ixAlloc] = 0;
			// Insert <tag> = <value3>
			attrn = tag;
			exprstr.assign(&pszTbl[ixReq], ixAlloc-ixReq);
			puAd->AssignExpr(attrn, exprstr.c_str());
		}
		if (ixAssigned > 0) {
			// the remainder of the line is the assigned value
			attrn = "Assigned";
			attrn += tag;
			exprstr = &pszTbl[ixAssigned];
			puAd->AssignExpr(attrn, exprstr.c_str());
		}
	}


protected:
	int ixColon;
	int ixUse;
	int ixReq;
	int ixAlloc;
	int ixAssigned;
};

// ----- the FutureEvent class
bool
FutureEvent::formatBody( std::string &out )
{
	out += head;
	out += "\n";
	if ( ! payload.empty()) {
		out += payload;
	}
	return true;
}

int
FutureEvent::readEvent (ULogFile& file, bool & got_sync_line)
{
	// read lines until we see "...\n" or "...\r\n"

	bool athead = true;
	std::string line;
	while (readLine(line, file, false)) {
		if (line[0] == '.' && (line == "...\n" || line == "...\r\n")) {
			got_sync_line = true;
			break;
		}
		else if (athead) {
			chomp(line);
			head = line;
			athead = false;
		} else {
			payload += line;
		}
	}
	return 1;
}

ClassAd*
FutureEvent::toClassAd(bool event_time_utc)
{
	ClassAd* myad = ULogEvent::toClassAd(event_time_utc);
	if( !myad ) return NULL;

	myad->Assign("EventHead", head);
	if ( ! payload.empty()) {
		StringTokenIterator lines(payload, "\r\n");
		const std::string * str;
		while ((str = lines.next_string())) { 
			if ( ! myad->Insert(*str)) {
				// TODO: append lines that are not classad statements to an "EventPayloadLines" attribute.
			}
		}
	}
	return myad;
}

void
FutureEvent::initFromClassAd(ClassAd* ad)
{
	ULogEvent::initFromClassAd(ad);

	if ( ! ad->LookupString("EventHead", head)) { head.clear(); }

	// Get the list of attributes that remain after we remove the known ones.
	classad::References attrs;
	sGetAdAttrs(attrs, *ad);
	attrs.erase(ATTR_MY_TYPE);
	attrs.erase("EventTypeNumber");
	attrs.erase("Cluster");
	attrs.erase("Proc");
	attrs.erase("Subproc");
	attrs.erase("EventTime");
	attrs.erase("EventHead");
	attrs.erase("EventPayloadLines");

	// print the remaining attributes into the payload.
	payload.clear();
	if ( ! attrs.empty()) {
		sPrintAdAttrs(payload, *ad, attrs);
	}

	// TODO Print value of EventPayloadLines attribute as raw text to the payload
}

void FutureEvent::setHead(const char * head_text)
{
	head = head_text;
	chomp(head);
}

void FutureEvent::setPayload(const char * payload_text)
{
	payload = payload_text;
}



// ----- the SubmitEvent class
SubmitEvent::SubmitEvent(void)
{
	eventNumber = ULOG_SUBMIT;
}

void
SubmitEvent::setSubmitHost(char const *addr)
{
	submitHost = addr ? addr : "";
}

bool
SubmitEvent::formatBody( std::string &out )
{
	int retval = formatstr_cat (out, "Job submitted from host: %s\n", submitHost.c_str());
	if (retval < 0)
	{
		return false;
	}
	if( !submitEventLogNotes.empty() ) {
		retval = formatstr_cat( out, "    %.8191s\n", submitEventLogNotes.c_str() );
		if( retval < 0 ) {
			return false;
		}
	}
	if( !submitEventUserNotes.empty() ) {
		retval = formatstr_cat( out, "    %.8191s\n", submitEventUserNotes.c_str() );
		if( retval < 0 ) {
			return false;
		}
	}
	if( !submitEventWarnings.empty() ) {
		retval = formatstr_cat( out, "    WARNING: Committed job submission into the queue with the following warning(s): %.8110s\n", submitEventWarnings.c_str() );
		if( retval < 0 ) {
			return false;
		}
	}
	return true;
}


int
SubmitEvent::readEvent (ULogFile& file, bool & got_sync_line)
{
	if ( ! read_line_value("Job submitted from host: ", submitHost, file, got_sync_line)) {
		return 0;
	}

	// check if event ended without specifying submit host.
	// in this case, the submit host would be the event delimiter
	if ( strncmp(submitHost.c_str(),"...",3)==0 ) {
		submitHost.clear();
		got_sync_line = true;
		return 1;
	}

	// see if the next line contains an optional event notes string
	if( ! read_optional_line(submitEventLogNotes, file, got_sync_line, true, true) ) {
		return 1;
	}

	// see if the next line contains an optional user event notes string
	if ( ! read_optional_line(submitEventUserNotes, file, got_sync_line, true, true) ) {
		return 1;
	}

	if ( ! read_optional_line(submitEventWarnings, file, got_sync_line, true, false) ) {
		return 1;
	}

	return 1;
}

ClassAd*
SubmitEvent::toClassAd(bool event_time_utc)
{
	ClassAd* myad = ULogEvent::toClassAd(event_time_utc);
	if( !myad ) return NULL;

	if( !submitHost.empty() ) {
		if( !myad->InsertAttr("SubmitHost",submitHost) ) return NULL;
	}

	if( !submitEventLogNotes.empty() ) {
		if( !myad->InsertAttr("LogNotes",submitEventLogNotes) ) return NULL;
	}
	if( !submitEventUserNotes.empty() ) {
		if( !myad->InsertAttr("UserNotes",submitEventUserNotes) ) return NULL;
	}
	if( !submitEventWarnings.empty() ) {
		if( !myad->InsertAttr("Warnings",submitEventWarnings) ) return NULL;
	}

	return myad;
}

void
SubmitEvent::initFromClassAd(ClassAd* ad)
{
	ULogEvent::initFromClassAd(ad);

	if( !ad ) return;

	ad->LookupString("SubmitHost", submitHost);

	ad->LookupString("LogNotes", submitEventLogNotes);

	ad->LookupString("UserNotes", submitEventUserNotes);

	ad->LookupString("Warnings", submitEventWarnings);
}

// ----- the GenericEvent class
GenericEvent::GenericEvent(void)
{
	info[0] = '\0';
	eventNumber = ULOG_GENERIC;
}

GenericEvent::~GenericEvent(void)
{
}

bool
GenericEvent::formatBody( std::string &out )
{
    int retval = formatstr_cat( out, "%s\n", info );
    if (retval < 0)
    {
		return false;
    }

    return true;
}

int
GenericEvent::readEvent(ULogFile& file, bool & got_sync_line)
{
	std::string str;
	if ( ! read_optional_line(str, file, got_sync_line) || str.length() >= sizeof(info)) {
		return 0;
	}
	strncpy(info, str.c_str(), sizeof(info)-1);
	info[sizeof(info)-1] = 0;

    return 1;
}

ClassAd*
GenericEvent::toClassAd(bool event_time_utc)
{
	ClassAd* myad = ULogEvent::toClassAd(event_time_utc);
	if( !myad ) return NULL;

	if( info[0] ) {
		if( !myad->InsertAttr("Info", info) ) {
			delete myad;
			return NULL;
		}
	}

	return myad;
}

void
GenericEvent::initFromClassAd(ClassAd* ad)
{
	ULogEvent::initFromClassAd(ad);

	if( !ad ) return;

	ad->LookupString("Info", info, sizeof(info) );
}

void
GenericEvent::setInfoText(char const *str)
{
	strncpy(info,str,sizeof(info) - 1);
	info[ sizeof(info) - 1 ] = '\0'; //ensure null-termination
}

// ----- the RemoteErrorEvent class
RemoteErrorEvent::RemoteErrorEvent(void)
{
	eventNumber = ULOG_REMOTE_ERROR;
	critical_error = true;
	hold_reason_code = 0;
	hold_reason_subcode = 0;
}

void
RemoteErrorEvent::setHoldReasonCode(int hold_reason_code_arg)
{
	this->hold_reason_code = hold_reason_code_arg;
}
void
RemoteErrorEvent::setHoldReasonSubCode(int hold_reason_subcode_arg)
{
	this->hold_reason_subcode = hold_reason_subcode_arg;
}

bool
RemoteErrorEvent::formatBody( std::string &out )
{
	char const *error_type = "Error";
	int retval;

	if(!critical_error) error_type = "Message";

	retval = formatstr_cat(
	  out,
	  "%s from %s on %s:\n",
	  error_type,
	  daemon_name.c_str(),
	  execute_host.c_str());



    if (retval < 0)
    {
        return false;
    }

	//output each line of error_str, indented by one tab
	size_t ix = 0;
	while (ix < error_str.size()) {
		size_t len = std::string::npos;
		size_t eol = error_str.find('\n', ix);
		if (eol != std::string::npos) {
			len = eol - ix;
		}
		out += '\t';
		out += error_str.substr(ix, len);
		out += '\n';
		if (eol != std::string::npos && error_str[eol] == '\n') {
			eol++;
		}
		ix = eol;
	}
#if 0
	char *cpy = strdup(error_str.c_str());
	char *line = cpy;
	if (line) while(*line) {
		char *next_line = strchr(line,'\n');
		if(next_line) *next_line = '\0';

		retval = formatstr_cat( out, "\t%s\n", line );
		if(retval < 0) return 0;

		if(!next_line) break;
		*next_line = '\n';
		line = next_line+1;
	}
	free(cpy);
#endif

	if (hold_reason_code) {
		formatstr_cat( out, "\tCode %d Subcode %d\n",
					   hold_reason_code, hold_reason_subcode );
	}

	return true;
}

int
RemoteErrorEvent::readEvent(ULogFile& file, bool & got_sync_line)
{
	char error_type[128];

	std::string line;
	if ( ! read_optional_line(line, file, got_sync_line)) {
		return 0;
	}

	// parse error_type, daemon_name and execute_host from a line of the form
	// [Error|Warning] from <daemon_name> on <execute_host>:
	// " from ", " on ", and the trailing ":" are added by format body and should be removed here.

	int retval = 0;
	trim(line);
	size_t ix = line.find(" from ");
	if (ix != std::string::npos) {
		std::string et = line.substr(0, ix);
		trim(et);
		strncpy(error_type, et.c_str(), sizeof(error_type) - 1);
		line = line.substr(ix + 6, line.length());
		trim(line);
	} else {
		strncpy(error_type, "Error", sizeof(error_type) - 1);
		retval = -1;
	}

	ix = line.find(" on ");
	if (ix == 0 || ix == std::string::npos) {
		daemon_name.clear();
	} else {
		std::string dn = line.substr(0, ix);
		trim(dn);
		daemon_name = dn;
		line = line.substr(ix + 4, line.length());
		trim(line);
	}

	// we expect to see a : after the daemon name, if we find it. remove it.
	ix = line.length();
	if (ix > 0 && line[ix-1] == ':') { line.erase(ix - 1); }

	execute_host = line;

	if (retval < 0) {
		return 0;
	}

	error_type[sizeof(error_type)-1] = '\0';

	if(!strcmp(error_type,"Error")) critical_error = true;
	else if(!strcmp(error_type,"Warning")) critical_error = false;

	error_str.clear();

		// see if the next line contains an optional event notes string,
		// this can be multiple lines, each starting with a tab. it *may*
		// or may not end with a hold code and subcode line.
	while (read_optional_line(line, file, got_sync_line)) {
		const char *l = line.c_str();
		if(l[0] == '\t') l++;

		int code,subcode;
		if( sscanf(l,"Code %d Subcode %d",&code,&subcode) == 2 ) {
			hold_reason_code = code;
			hold_reason_subcode = subcode;
			break;
		}

		if(error_str.length()) error_str += "\n";
		error_str += l;
	}

	return 1;
}

ClassAd*
RemoteErrorEvent::toClassAd(bool event_time_utc)
{
	ClassAd* myad = ULogEvent::toClassAd(event_time_utc);
	if( !myad ) return NULL;

	if(!daemon_name.empty()) {
		myad->Assign("Daemon",daemon_name);
	}
	if(!execute_host.empty()) {
		myad->Assign("ExecuteHost",execute_host);
	}
	if(!error_str.empty()) {
		myad->Assign("ErrorMsg",error_str);
	}
	if(!critical_error) { //default is true
		myad->Assign("CriticalError",(int)critical_error);
	}
	if(hold_reason_code) {
		// NOTE: It is important to leave these attributes undefined
		// if the HOLD_REASON_CODE is 0.
		myad->Assign(ATTR_HOLD_REASON_CODE, hold_reason_code);
		myad->Assign(ATTR_HOLD_REASON_SUBCODE, hold_reason_subcode);
	}

	return myad;
}

void
RemoteErrorEvent::initFromClassAd(ClassAd* ad)
{
	ULogEvent::initFromClassAd(ad);
	int crit_err = 0;

	if( !ad ) return;

	ad->LookupString("Daemon", daemon_name);
	ad->LookupString("ExecuteHost", execute_host);
	ad->LookupString("ErrorMsg", error_str);
	if( ad->LookupInteger("CriticalError",crit_err) ) {
		critical_error = (crit_err != 0);
	}
	ad->LookupInteger(ATTR_HOLD_REASON_CODE, hold_reason_code);
	ad->LookupInteger(ATTR_HOLD_REASON_SUBCODE, hold_reason_subcode);
}

void
RemoteErrorEvent::setCriticalError(bool f)
{
	critical_error = f;
}

// ----- the ExecuteEvent class
ExecuteEvent::ExecuteEvent(void)
{
	eventNumber = ULOG_EXECUTE;
}

ExecuteEvent::~ExecuteEvent(void)
{
	if (executeProps) delete executeProps;
	executeProps = nullptr;
}

void
ExecuteEvent::setExecuteHost(char const *addr)
{
	executeHost = addr ? addr : "";
}

char const *
ExecuteEvent::getExecuteHost()
{
	return executeHost.c_str();
}

void
ExecuteEvent::setSlotName(char const *name)
{
	slotName = name ? name : "";
}

bool ExecuteEvent::hasProps() {
	return executeProps && executeProps->size() > 0;
}

ClassAd & ExecuteEvent::setProp() {
	if ( ! executeProps) executeProps = new ClassAd(); 
	return *executeProps;
}

bool
ExecuteEvent::formatBody( std::string &out )
{
	int retval;

	retval = formatstr_cat( out, "Job executing on host: %s\n", executeHost.c_str() );

	if (retval < 0) {
		return false;
	}

	if ( ! slotName.empty()) {
		formatstr_cat(out, "\tSlotName: %s\n", slotName.c_str());
	}

	if (hasProps()) {
		// print sorted key=value pairs for the properties
		classad::References attrs;
		sGetAdAttrs(attrs, *executeProps);
		sPrintAdAttrs(out, *executeProps, attrs, "\t");
	}

	return true;
}

int
ExecuteEvent::readEvent (ULogFile& file, bool & got_sync_line)
{
	if ( ! read_line_value("Job executing on host: ", executeHost, file, got_sync_line)) {
		return 0;
	}
	ExprTree * tree = nullptr;
	std::string line, attr;
	// read optional SlotName : <name>
	if ( ! read_optional_line(line, file, got_sync_line)) {
		return 1; // OK for there to be no more lines - backwards compatibility
	}
	if (starts_with(line, "\tSlotName:")) {
		slotName = strchr(line.c_str(),':') + 1,
		trim(slotName);
		trim_quotes(slotName, "\"");
	} else if (ParseLongFormAttrValue(line.c_str(), attr, tree)) {
		setProp().Insert(attr, tree);
	}
	if (got_sync_line) return 1;
	// read optional other properties
	while (read_optional_line(line, file, got_sync_line)) {
		if (ParseLongFormAttrValue(line.c_str(), attr, tree)) {
			setProp().Insert(attr, tree);
		}
	}
	return 1;
}

ClassAd*
ExecuteEvent::toClassAd(bool event_time_utc)
{
	ClassAd* myad = ULogEvent::toClassAd(event_time_utc);
	if( !myad ) return NULL;

	if( !executeHost.empty() ) {
		if( !myad->Assign("ExecuteHost",executeHost) ) return NULL;
	}
	if( !slotName.empty()) {
		myad->Assign("SlotName", slotName);
	}
	if (hasProps()) {
		myad->Insert("ExecuteProps", executeProps->Copy());
	}

	return myad;
}

void
ExecuteEvent::initFromClassAd(ClassAd* ad)
{
	ULogEvent::initFromClassAd(ad);

	if( !ad ) return;

	ad->LookupString("ExecuteHost", executeHost);
	slotName.clear();
	ad->LookupString("SlotName", slotName);

	delete executeProps;
	executeProps = nullptr;

	ClassAd * props = nullptr;
	ExprTree * tree = ad->Lookup("ExecuteProps");
	if (tree && tree->isClassad(&props)) {
		executeProps = static_cast<ClassAd*>(props->Copy());
	}
}


// ----- the ExecutableError class
ExecutableErrorEvent::ExecutableErrorEvent(void)
{
	errType = (ExecErrorType) -1;
	eventNumber = ULOG_EXECUTABLE_ERROR;
}


ExecutableErrorEvent::~ExecutableErrorEvent(void)
{
}

bool
ExecutableErrorEvent::formatBody( std::string &out )
{
	int retval;

	switch (errType)
	{
	  case CONDOR_EVENT_NOT_EXECUTABLE:
		retval = formatstr_cat( out, "(%d) Job file not executable.\n", errType );
		break;

	  case CONDOR_EVENT_BAD_LINK:
		retval=formatstr_cat( out, "(%d) Job not properly linked for Condor.\n", errType );
		break;

	  default:
		retval = formatstr_cat( out, "(%d) [Bad error number.]\n", errType );
	}

	if (retval < 0) return false;

	return true;
}

int
ExecutableErrorEvent::readEvent (ULogFile& file, bool & got_sync_line)
{
	std::string line;
	if ( ! read_line_value("(", line, file, got_sync_line)) {
		return 0;
	}
	// get the error type number
	YourStringDeserializer ser(line.c_str());
	if ( ! ser.deserialize_int((int*)&errType) || ! ser.deserialize_sep(")")) {
		return 0;
	}
	// we ignore the rest of the line.

	return 1;
}

ClassAd*
ExecutableErrorEvent::toClassAd(bool event_time_utc)
{
	ClassAd* myad = ULogEvent::toClassAd(event_time_utc);
	if( !myad ) return NULL;

	if( errType >= 0 ) {
		if( !myad->InsertAttr("ExecuteErrorType", errType) ) {
			delete myad;
			return NULL;
		}
	}

	return myad;
}

void
ExecutableErrorEvent::initFromClassAd(ClassAd* ad)
{
	ULogEvent::initFromClassAd(ad);

	if( !ad ) return;

	int reallyExecErrorType;
	if( ad->LookupInteger("ExecuteErrorType", reallyExecErrorType) ) {
		switch( reallyExecErrorType ) {
		  case CONDOR_EVENT_NOT_EXECUTABLE:
			errType = CONDOR_EVENT_NOT_EXECUTABLE;
			break;
		  case CONDOR_EVENT_BAD_LINK:
			errType = CONDOR_EVENT_BAD_LINK;
			break;
		}
	}
}

// ----- the CheckpointedEvent class
CheckpointedEvent::CheckpointedEvent(void)
{
	(void)memset((void*)&run_local_rusage,0,(size_t) sizeof(run_local_rusage));
	run_remote_rusage = run_local_rusage;

	eventNumber = ULOG_CHECKPOINTED;

    sent_bytes = 0.0;
}

CheckpointedEvent::~CheckpointedEvent(void)
{
}

bool
CheckpointedEvent::formatBody( std::string &out )
{
	if ((formatstr_cat( out, "Job was checkpointed.\n" ) < 0)	||
		(!formatRusage( out, run_remote_rusage ))				||
		(formatstr_cat( out, "  -  Run Remote Usage\n" ) < 0)	||
		(!formatRusage( out, run_local_rusage ))				||
		(formatstr_cat( out, "  -  Run Local Usage\n" ) < 0))
		return false;

	if( formatstr_cat( out, "\t%.0f  -  Run Bytes Sent By Job For Checkpoint\n",
					  sent_bytes) < 0 ) {
		return false;
	}


	return true;
}

int
CheckpointedEvent::readEvent (ULogFile& file, bool & got_sync_line)
{
	std::string line;
	if ( ! read_line_value("Job was checkpointed.", line, file, got_sync_line)) {
		return 0;
	}

	int remain_off;
	if (!readRusageLine(line,file,got_sync_line,run_remote_rusage,remain_off) ||
		!readRusageLine(line,file,got_sync_line,run_local_rusage,remain_off))
		return 0;

	if ( ! read_optional_line(line, file, got_sync_line)) {
		return 1;		//backwards compatibility
	}
	if (sscanf(line.c_str(), "\t%lf  -  Run Bytes Sent By Job For Checkpoint", &sent_bytes) != 1) {
		return 0;
	}

	return 1;
}

ClassAd*
CheckpointedEvent::toClassAd(bool event_time_utc)
{
	ClassAd* myad = ULogEvent::toClassAd(event_time_utc);
	if( !myad ) return NULL;

	char* rs = rusageToStr(run_local_rusage);
	if( !myad->InsertAttr("RunLocalUsage", rs) ) {
		free(rs);
		delete myad;
		return NULL;
	}
	free(rs);

	rs = rusageToStr(run_remote_rusage);
	if( !myad->InsertAttr("RunRemoteUsage", rs) ) {
		free(rs);
		delete myad;
		return NULL;
	}
	free(rs);

	if( !myad->InsertAttr("SentBytes", sent_bytes) ) {
		delete myad;
		return NULL;
	}

	return myad;
}

void
CheckpointedEvent::initFromClassAd(ClassAd* ad)
{
	ULogEvent::initFromClassAd(ad);

	if( !ad ) return;

	std::string usageStr;
	if( ad->LookupString("RunLocalUsage", usageStr) ) {
		strToRusage(usageStr.c_str(), run_local_rusage);
	}
	usageStr.clear();
	if( ad->LookupString("RunRemoteUsage", usageStr) ) {
		strToRusage(usageStr.c_str(), run_remote_rusage);
	}

	ad->LookupFloat("SentBytes", sent_bytes);
}

// ----- the JobEvictedEvent class
JobEvictedEvent::JobEvictedEvent(void)
{
	eventNumber = ULOG_JOB_EVICTED;
	checkpointed = false;

	(void)memset((void*)&run_local_rusage,0,(size_t) sizeof(run_local_rusage));
	run_remote_rusage = run_local_rusage;

	sent_bytes = recvd_bytes = 0.0;

	terminate_and_requeued = false;
	normal = false;
	return_value = -1;
	signal_number = -1;
	pusageAd = NULL;
}


JobEvictedEvent::~JobEvictedEvent(void)
{
	if ( pusageAd ) delete pusageAd;
}

int
JobEvictedEvent::readEvent( ULogFile& file, bool & got_sync_line )
{
	int  ckpt;
	char buffer [128];

	reason.clear();
	core_file.clear();

	std::string line;
	if ( ! read_line_value("Job was evicted.", line, file, got_sync_line) || 
		 ! read_optional_line(line, file, got_sync_line)) {
		return 0;
	}
	if (2 != sscanf(line.c_str(), "\t(%d) %127[a-zA-z ]", &ckpt, buffer)) {
		return 0;
	}
	checkpointed = (bool) ckpt;
	buffer[sizeof(buffer)-1] = 0;

		/*
		   since the old parsing code treated the integer we read as a
		   bool (only to decide between checkpointed or not), we now
		   have to parse the string we just read to figure out if this
		   was a terminate_and_requeued eviction or not.
		*/
	if( ! strncmp(buffer, "Job terminated and was requeued", 31) ) {
		terminate_and_requeued = true;
	} else {
		terminate_and_requeued = false;
	}

	int remain=-1;
	if( !readRusageLine(line,file,got_sync_line,run_remote_rusage,remain)||
		!readRusageLine(line,file,got_sync_line,run_local_rusage,remain))
	{
		return 0;
	}

	if ( ! read_optional_line(line, file, got_sync_line) ||
		(1 != sscanf(line.c_str(), "\t%lf  -  Run Bytes Sent By Job", &sent_bytes)) ||
		 ! read_optional_line(line, file, got_sync_line) ||
		(1 != sscanf(line.c_str(), "\t%lf  -  Run Bytes Received By Job", &recvd_bytes)))
	{
		return 1;				// backwards compatibility
	}

	if( ! terminate_and_requeued ) {
			// nothing more to read
		return 1;
	}

		// now, parse the terminate and requeue specific stuff.

	int  normal_term;

	// we expect one of these
	//  \t(0) Normal termination (return value %d)
	//  \t(1) Abnormal termination (signal %d)
	// Then if abnormal termination one of these
	//  \t(0) No core file
	//  \t(1) Corefile in: %s
	if ( ! read_optional_line(line, file, got_sync_line) ||
		(2 != sscanf(line.c_str(), "\t(%d) %127[^\r\n]", &normal_term, buffer)))
	{
		return 0;
	}
	if( normal_term ) {
		normal = true;
		if (1 != sscanf(buffer, "Normal termination (return value %d)", &return_value)) {
			return 0;
		}
	} else {
		normal = false;
		if (1 != sscanf(buffer, "Abnormal termination (signal %d)", &signal_number)) {
			return 0;
		}
		// we now expect a line to tell us about core files
		if ( ! read_optional_line(line, file, got_sync_line)) {
			return 0;
		}
		trim(line);
		const char cpre[] = "(1) Corefile in: ";
		if (starts_with(line.c_str(), cpre)) {
			core_file = line.c_str() + strlen(cpre);
		} else if ( ! starts_with(line.c_str(), "(0)")) {
			return 0; // not a valid value
		}
	}

	// finally, see if there's a reason.  this is optional.
	if (read_optional_line(line, file, got_sync_line, true)) {
		trim(line);
		reason = line;
	}

	return 1;
}


bool
JobEvictedEvent::formatBody( std::string &out )
{
  int retval;

  if( formatstr_cat( out, "Job was evicted.\n\t" ) < 0 ) {
    return false;
  }

  if( terminate_and_requeued ) {
    retval = formatstr_cat( out, "(0) Job terminated and was requeued\n\t" );
  } else if( checkpointed ) {
    retval = formatstr_cat( out, "(1) Job was checkpointed.\n\t" );
  } else {
    retval = formatstr_cat( out, "(0) CPU times\n\t" );
  }

  if( retval < 0 ) {
    return false;
  }

  if( (!formatRusage( out, run_remote_rusage ))				||
      (formatstr_cat( out, "  -  Run Remote Usage\n\t" ) < 0)	||
      (!formatRusage( out, run_local_rusage ))					||
      (formatstr_cat( out, "  -  Run Local Usage\n" ) < 0) )
    {
      return false;
    }

  if( formatstr_cat( out, "\t%.0f  -  Run Bytes Sent By Job\n",
	      sent_bytes ) < 0 ) {
    return false;
  }
  if( formatstr_cat( out, "\t%.0f  -  Run Bytes Received By Job\n",
	      recvd_bytes ) < 0 ) {
    return false;
  }

  if(terminate_and_requeued ) {
    if( normal ) {
      if( formatstr_cat( out, "\t(1) Normal termination (return value %d)\n",
		  return_value ) < 0 ) {
	return false;
      }
    }
    else {
      if( formatstr_cat( out, "\t(0) Abnormal termination (signal %d)\n",
		  signal_number ) < 0 ) {
	return false;
      }

      if( !core_file.empty() ) {
	retval = formatstr_cat( out, "\t(1) Corefile in: %s\n", core_file.c_str() );
      }
      else {
	retval = formatstr_cat( out, "\t(0) No core file\n" );
      }
      if( retval < 0 ) {
	return false;
      }
    }
  }

	if( !reason.empty() ) {
		if( formatstr_cat( out, "\t%s\n", reason.c_str() ) < 0 ) {
			return false;
		}
	}

	// print out resource request/usage values.
	//
	if (pusageAd) {
		formatUsageAd( out, pusageAd );
	}

  return true;
}

ClassAd*
JobEvictedEvent::toClassAd(bool event_time_utc)
{
	ClassAd* myad = ULogEvent::toClassAd(event_time_utc);
	if( !myad ) return NULL;

	if( !myad->InsertAttr("Checkpointed", checkpointed ? true : false) ) {
		delete myad;
		return NULL;
	}

	char* rs = rusageToStr(run_local_rusage);
	if( !myad->InsertAttr("RunLocalUsage", rs) ) {
		free(rs);
		delete myad;
		return NULL;
	}
	free(rs);

	rs = rusageToStr(run_remote_rusage);
	if( !myad->InsertAttr("RunRemoteUsage", rs) ) {
		free(rs);
		delete myad;
		return NULL;
	}
	free(rs);

	if( !myad->InsertAttr("SentBytes", sent_bytes) ) {
		delete myad;
		return NULL;
	}
	if( !myad->InsertAttr("ReceivedBytes", recvd_bytes) ) {
		delete myad;
		return NULL;
	}

	if( !myad->InsertAttr("TerminatedAndRequeued",
					  terminate_and_requeued ? true : false) ) {
		delete myad;
		return NULL;
	}
	if( !myad->InsertAttr("TerminatedNormally", normal ? true : false) ) {
		delete myad;
		return NULL;
	}

	if( return_value >= 0 ) {
		if( !myad->InsertAttr("ReturnValue", return_value) ) {
			delete myad;
			return NULL;
		}
	}
	if( signal_number >= 0 ) {
		if( !myad->InsertAttr("TerminatedBySignal", signal_number) ) {
			delete myad;
			return NULL;
		}
	}

	if( !reason.empty() ) {
		if( !myad->InsertAttr("Reason", reason) ) {
			delete myad;
			return NULL;
		}
	}
	if( !core_file.empty() ) {
		if( !myad->InsertAttr("CoreFile", core_file) ) {
			delete myad;
			return NULL;
		}
	}

	return myad;
}

void
JobEvictedEvent::initFromClassAd(ClassAd* ad)
{
	ULogEvent::initFromClassAd(ad);

	if( !ad ) return;

	int reallybool;
	if( ad->LookupInteger("Checkpointed", reallybool) ) {
		checkpointed = reallybool ? TRUE : FALSE;
	}

	std::string usageStr;
	if( ad->LookupString("RunLocalUsage", usageStr) ) {
		strToRusage(usageStr.c_str(), run_local_rusage);
	}
	usageStr.clear();
	if( ad->LookupString("RunRemoteUsage", usageStr) ) {
		strToRusage(usageStr.c_str(), run_remote_rusage);
	}

	ad->LookupFloat("SentBytes", sent_bytes);
	ad->LookupFloat("ReceivedBytes", recvd_bytes);

	if( ad->LookupInteger("TerminatedAndRequeued", reallybool) ) {
		terminate_and_requeued = reallybool ? TRUE : FALSE;
	}
	if( ad->LookupInteger("TerminatedNormally", reallybool) ) {
		normal = reallybool ? TRUE : FALSE;
	}

	ad->LookupInteger("ReturnValue", return_value);
	ad->LookupInteger("TerminatedBySignal", signal_number);

	ad->LookupString("Reason", reason);
	ad->LookupString("CoreFile", core_file);
}


// ----- JobAbortedEvent class
JobAbortedEvent::JobAbortedEvent (void) : toeTag(NULL)
{
	eventNumber = ULOG_JOB_ABORTED;
}

JobAbortedEvent::~JobAbortedEvent(void)
{
	if( toeTag ) {
		delete toeTag;
	}
}

void
JobAbortedEvent::setToeTag( classad::ClassAd * tt ) {
	if(! tt) { return; }
	if( toeTag ) { delete toeTag; }
	toeTag = new ToE::Tag();
	if(! ToE::decode( tt, * toeTag )) {
		delete toeTag;
		toeTag = NULL;
	}
}

bool
JobAbortedEvent::formatBody( std::string &out )
{

	if( formatstr_cat( out, "Job was aborted.\n" ) < 0 ) {
		return false;
	}
	if( !reason.empty() ) {
		if( formatstr_cat( out, "\t%s\n", reason.c_str() ) < 0 ) {
			return false;
		}
	}
	if( toeTag ) {
		if(! toeTag->writeToString( out )) {
			return false;
		}
	}
	return true;
}


int
JobAbortedEvent::readEvent (ULogFile& file, bool & got_sync_line)
{
	reason.clear();

	std::string line;
	if ( ! read_line_value("Job was aborted", line, file, got_sync_line)) {
		return 0;
	}
	// try to read the reason, this is optional
	if (read_optional_line(line, file, got_sync_line, true)) {
		trim(line);
		reason = line;
	}

	// Try to read the ToE tag.
	if( got_sync_line ) { return 1; }
	if( read_optional_line( line, file, got_sync_line ) ) {
		if( line.empty() ) {
			if(! read_optional_line( line, file, got_sync_line )) {
				return 0;
			}
		}

		if( replace_str(line, "\tJob terminated by ", "") ) {
			if( toeTag != NULL ) { delete toeTag; }
			toeTag = new ToE::Tag();
			if(! toeTag->readFromString( line )) {
				return 0;
			}
		} else {
			return 0;
		}
	}

	return 1;
}

ClassAd*
JobAbortedEvent::toClassAd(bool event_time_utc)
{
	ClassAd* myad = ULogEvent::toClassAd(event_time_utc);
	if( !myad ) return NULL;

	if( !reason.empty() ) {
		if( !myad->InsertAttr("Reason", reason) ) {
			delete myad;
			return NULL;
		}
	}

	if( toeTag ) {
		classad::ClassAd * tt = new classad::ClassAd();
		if(! ToE::encode( * toeTag, tt )) {
			delete tt;
			delete myad;
			return NULL;
		}
		if(! myad->Insert(ATTR_JOB_TOE, tt )) {
			delete tt;
			delete myad;
			return NULL;
		}
	}

	return myad;
}

void
JobAbortedEvent::initFromClassAd(ClassAd* ad)
{
	ULogEvent::initFromClassAd(ad);

	if( !ad ) return;

	ad->LookupString("Reason", reason);

	setToeTag( dynamic_cast<classad::ClassAd *>(ad->Lookup(ATTR_JOB_TOE)) );
}

// ----- TerminatedEvent baseclass
TerminatedEvent::TerminatedEvent(void) : toeTag(NULL)
{
	normal = false;
	returnValue = signalNumber = -1;
	pusageAd = NULL;

	(void)memset((void*)&run_local_rusage,0,(size_t)sizeof(run_local_rusage));
	run_remote_rusage=total_local_rusage=total_remote_rusage=run_local_rusage;

	sent_bytes = recvd_bytes = total_sent_bytes = total_recvd_bytes = 0.0;
}

TerminatedEvent::~TerminatedEvent(void)
{
	if ( pusageAd ) delete pusageAd;
	if( toeTag ) { delete toeTag; }
}

void
TerminatedEvent::setToeTag( classad::ClassAd * tt ) {
	if( tt ) {
		if( toeTag ) { delete toeTag; }
		toeTag = new classad::ClassAd( * tt );
	}
}

bool
TerminatedEvent::formatBody( std::string &out, const char *header )
{
	int retval=0;

	if( normal ) {
		if( formatstr_cat( out, "\t(1) Normal termination (return value %d)\n\t",
					returnValue) < 0 ) {
			return false;
		}

	} else {
		if( formatstr_cat( out, "\t(0) Abnormal termination (signal %d)\n",
					signalNumber ) < 0 ) {
			return false;
		}

		if( !core_file.empty() ) {
			retval = formatstr_cat( out, "\t(1) Corefile in: %s\n\t",
									core_file.c_str() );
		} else {
			retval = formatstr_cat( out, "\t(0) No core file\n\t" );
		}
	}

	if ((retval < 0)												||
		(!formatRusage( out, run_remote_rusage ))					||
		(formatstr_cat( out, "  -  Run Remote Usage\n\t" ) < 0)	||
		(!formatRusage( out, run_local_rusage ))					||
		(formatstr_cat( out, "  -  Run Local Usage\n\t" ) < 0)		||
		(!formatRusage( out, total_remote_rusage ))				||
		(formatstr_cat( out, "  -  Total Remote Usage\n\t" ) < 0)	||
		(!formatRusage( out,  total_local_rusage ))				||
		(formatstr_cat( out, "  -  Total Local Usage\n" ) < 0))
		return false;


	if (formatstr_cat( out, "\t%.0f  -  Run Bytes Sent By %s\n",
					   sent_bytes, header ) < 0 ||
		formatstr_cat( out, "\t%.0f  -  Run Bytes Received By %s\n",
					   recvd_bytes, header ) < 0 ||
		formatstr_cat( out, "\t%.0f  -  Total Bytes Sent By %s\n",
					   total_sent_bytes, header ) < 0 ||
		formatstr_cat( out, "\t%.0f  -  Total Bytes Received By %s\n",
					   total_recvd_bytes, header ) < 0)
		return true;				// backwards compatibility

	// print out resource request/usage values.
	//
	if (pusageAd) {
		formatUsageAd( out, pusageAd );
	}

	return true;
}


int
TerminatedEvent::readEventBody( ULogFile& file, bool & got_sync_line, const char* header )
{
	char buffer[128];
	int  normalTerm;

	if (pusageAd) {
		pusageAd->Clear();
	}

	// when we get here, all of the header line will have been read

	std::string line;
	if ( ! read_optional_line(line, file, got_sync_line) ||
		(2 != sscanf(line.c_str(), "\t(%d) %127[^\r\n]", &normalTerm, buffer))) {
		return 0;
	}

	buffer[sizeof(buffer)-1] = 0;
	if( normalTerm ) {
		normal = true;
		if(1 != sscanf(buffer,"Normal termination (return value %d)",&returnValue))
			return 0;
	} else {
		normal = false;
		if(1 != sscanf(buffer,"Abnormal termination (signal %d)",&signalNumber)) {
			return 0;
		}
		// we now expect a line to tell us about core files
		if ( ! read_optional_line(line, file, got_sync_line)) {
			return 0;
		}
		trim(line);
		const char cpre[] = "(1) Corefile in: ";
		if (starts_with(line.c_str(), cpre)) {
			core_file = line.c_str() + strlen(cpre);
		} else if ( ! starts_with(line.c_str(), "(0)")) {
			return 0; // not a valid value
		}
	}

	bool in_usage_ad = false;

		// read in rusage values
	int remain=-1;
	if (!readRusageLine(line,file,got_sync_line,run_remote_rusage,remain)||
		!readRusageLine(line,file,got_sync_line,run_local_rusage,remain) ||
		!readRusageLine(line,file,got_sync_line,total_remote_rusage,remain) ||
		!readRusageLine(line,file,got_sync_line,total_local_rusage,remain))
		return 0;

		// read in the transfer info, and then the resource usage info
		// we expect transfer info is first, as soon as we get a line that
		// doesn't match that pattern, we switch to resource usage parsing.
	UsageLineParser ulp;

	for (;;) {
		char srun[sizeof("Total")];
		char sdir[sizeof("Received")];
		char sjob[22];

		if ( ! read_optional_line(line, file, got_sync_line)) {
			break;
		}
		const char * sz = line.c_str();
		if (in_usage_ad) {
			// lines for reading the usageAd must be of the form "\tlabel : value value value value\n"
			// where the first word of label is the resource type and the values are the resource values
			// When we get here, we have already read the header line, which is:
			//     Partitionable Resources : Usage  Request Allocated [Assigned]
			// the Assigned column will only appear if there are non-fungible resources
			// the columns will be widened if necessary to fit the data, the data is
			// unparsed classad values, not necessarily integers (but usually integers)
			// Older version of HTCondor will unparse using fixed width fields determined by parsing
			// the header column
			const char * pszColon = strchr(sz, ':');
			if ( ! pszColon) {
				// No colon, we are done parsing the usage ad
				break;
			}
			ulp.Parse(sz, pusageAd);
			continue;
		}

		// expect for strings of the form "\t%f  -  Run Bytes Sent By Job"
		// where "Run" "Sent" and "Job" can all vary. 
		float val; srun[0] = sdir[0] = sjob[0] = 0;
		bool fOK = false;
		if (4 == sscanf(sz, "\t%f  -  %5s Bytes %8s By %21s", &val, srun, sdir, sjob)) {
			if (!strcmp(sjob,header)) {
				if (!strcmp(srun,"Run")) {
					if (!strcmp(sdir,"Sent")) {
						sent_bytes = val; fOK = true;
					} else if (!strcmp(sdir,"Received")) {
						recvd_bytes = val; fOK = true;
					}
				} else if (!strcmp(srun,"Total")) {
					if (!strcmp(sdir,"Sent")) {
						total_sent_bytes = val; fOK = true;
					} else if (!strcmp(sdir,"Received")) {
						total_recvd_bytes = val; fOK = true;
					}
				}
			}
		}
		else {
			// no match, we have (probably) just read the banner of the useage ad.
			if (starts_with(sz, "\tPartitionable ") ||  
				starts_with(sz, "\tResources")) {
				if ( ! pusageAd) { pusageAd = new ClassAd(); }
				pusageAd->Clear();
				ulp.init(sz);
				in_usage_ad = true;
				continue;
			}
			if ( ! fOK) {
				break;
			}
		}
	}

	return 1;
}

// extract Request<RES>, "<RES>Usage", "Assigned<RES>" and "<RES>" attributes from the given ad
// both Request<RES> and <RES> must exist, Assignd<RES> and <RES>Usage may or may not
// other attributes of the input ad are ignored
int TerminatedEvent::initUsageFromAd(const classad::ClassAd& ad)
{
	std::string strRequest("Request");
	std::string attr; // temporary

	// the strategy here is to iterate the attributes of the classad
	// looking ones that start with "Request" If we find one
	// we remove the "Request" prefix, and check to see if the remainder
	// is a resource tag. We do this by looking for a bare attribute with that name.
	// if we find that, then we have a <RES> tag
	// then we copy the "<RES>", "Request<RES>", "<RES>Usage" and "Assigned<RES>" attributes
	for (auto it = ad.begin(); it != ad.end(); ++it) {
		if (starts_with_ignore_case(it->first, strRequest)) {
			// remove the Request prefix to get a tag. does that tag exist as an attribute?
			std::string tag = it->first.substr(7);
			if (tag.empty())
				continue; 
			ExprTree * expr = ad.Lookup(tag);
			if ( ! expr)
				continue;

			// got one, tag is a resource tag

			if ( ! pusageAd) {
				pusageAd = new ClassAd();
				if ( ! pusageAd)
					return 0; // failure
			}

			// Copy <RES> attribute into the pusageAd
			expr = expr->Copy();
			if ( ! expr)
				return 0;
			pusageAd->Insert(tag, expr);

			// Copy Request<RES> attribute into the pusageAd
			expr = it->second->Copy();
			if ( ! expr)
				return 0;
			pusageAd->Insert(it->first, expr);

			// Copy <RES>Usage attribute if it exists
			attr = tag; attr += "Usage";
			expr = ad.Lookup(attr);
			if (expr) {
				expr = expr->Copy();
				if ( ! expr)
					return 0;
				pusageAd->Insert(attr, expr);
			} else {
				pusageAd->Delete(attr);
			}

			// Copy Assigned<RES> attribute if it exists
			attr = "Assigned"; attr += tag;
			expr = ad.Lookup(attr);
			if (expr) {
				expr = expr->Copy();
				if (!expr)
					return 0;
				pusageAd->Insert(attr, expr);
			} else {
				pusageAd->Delete(attr);
			}
		}
	}

	return 1;
}


// ----- JobTerminatedEvent class
JobTerminatedEvent::JobTerminatedEvent(void) : TerminatedEvent()
{
	eventNumber = ULOG_JOB_TERMINATED;
}


JobTerminatedEvent::~JobTerminatedEvent(void)
{
}


bool
JobTerminatedEvent::formatBody( std::string &out )
{
	if( formatstr_cat( out, "Job terminated.\n" ) < 0 ) {
		return false;
	}
	bool rv = TerminatedEvent::formatBody( out, "Job" );
	if( rv && toeTag != NULL ) {
		ToE::Tag tag;
		if( ToE::decode( toeTag, tag ) ) {
			if( tag.howCode == 0 ) {
				// There is no signal 0, so this combination means we read
				// an old tag with no exit information.
				if( tag.exitBySignal && tag.signalOrExitCode == 0 ) {
					if( formatstr_cat( out, "\n\tJob terminated of its own accord at %s.\n", tag.when.c_str() ) < 0 ) {
						return false;
					}
				} else {
					// Both 'signal' and 'exit-code' must not contain spaces
					// because of the way that sscanf() works.
					if( formatstr_cat( out, "\n\tJob terminated of its own accord at %s with %s %d.\n", tag.when.c_str(), tag.exitBySignal ? "signal" : "exit-code", tag.signalOrExitCode ) < 0 ) {
						return false;
					}
				}
			} else {
				rv = tag.writeToString( out );
			}
		}
	}
	return rv;
}


int
JobTerminatedEvent::readEvent (ULogFile& file, bool & got_sync_line)
{
	std::string str;
	if ( ! read_line_value("Job terminated.", str, file, got_sync_line)) {
		return 0;
	}

	int rv = TerminatedEvent::readEventBody( file, got_sync_line, "Job" );
	if(! rv) { return rv; }

	std::string line;
	if( got_sync_line ) { return 1; }
	if( read_optional_line( line, file, got_sync_line ) ) {
		if(line.empty()) {
			if( read_optional_line( line, file, got_sync_line ) ) {
				return 0;
			}
		}
		if( replace_str(line, "\tJob terminated of its own accord at ", "") ) {
			if( toeTag != NULL ) {
				delete toeTag;
			}
			toeTag = new classad::ClassAd();

			toeTag->InsertAttr( "Who", ToE::itself );
			toeTag->InsertAttr( "How", ToE::strings[ToE::OfItsOwnAccord] );
			toeTag->InsertAttr( "HowCode", ToE::OfItsOwnAccord );

			// This code gets more complicated if we don't assume UTC i/o.
			struct tm eventTime;
			iso8601_to_time( line.c_str(), & eventTime, NULL, NULL );
			toeTag->InsertAttr( "When", timegm(&eventTime) );

			char type[16];
			int signalOrExitCode;
			size_t offset = line.find(" with ");
			if( offset != std::string::npos ) {
				if( 2 == sscanf( line.c_str() + offset, " with %15s %d", type, & signalOrExitCode )) {
					if( strcmp( type, "signal" ) == 0 ) {
						toeTag->InsertAttr( ATTR_ON_EXIT_BY_SIGNAL, true );
						toeTag->InsertAttr( ATTR_ON_EXIT_SIGNAL, signalOrExitCode );
					} else if( strcmp( type, "exit-code" ) == 0 ) {
						toeTag->InsertAttr( ATTR_ON_EXIT_BY_SIGNAL, false );
						toeTag->InsertAttr( ATTR_ON_EXIT_CODE, signalOrExitCode );
					}
				}
			}
		} else if( replace_str(line, "\tJob terminated by ", "") ) {
			ToE::Tag tag;
			if(! tag.readFromString( line )) {
				return 0;
			}

			if(toeTag != NULL) {
				delete toeTag;
			}
			toeTag = new ClassAd();
			ToE::encode( tag, toeTag );
		} else {
			return 0;
		}
	}

	return 1;
}

ClassAd*
JobTerminatedEvent::toClassAd(bool event_time_utc)
{
	ClassAd* myad = ULogEvent::toClassAd(event_time_utc);
	if( !myad ) return NULL;

	if (pusageAd) {
		myad->Update(*pusageAd);
	}

	if( !myad->InsertAttr("TerminatedNormally", normal ? true : false) ) {
		delete myad;
		return NULL;
	}
	if( returnValue >= 0 ) {
		if( !myad->InsertAttr("ReturnValue", returnValue) ) {
			delete myad;
			return NULL;
		}
	}
	if( signalNumber >= 0 ) {
		if( !myad->InsertAttr("TerminatedBySignal", signalNumber) ) {
			delete myad;
			return NULL;
		}
	}

	if( !core_file.empty() ) {
		if( !myad->InsertAttr("CoreFile", core_file) ) {
			delete myad;
			return NULL;
		}
	}

	char* rs = rusageToStr(run_local_rusage);
	if( !myad->InsertAttr("RunLocalUsage", rs) ) {
		free(rs);
		delete myad;
		return NULL;
	}
	free(rs);
	rs = rusageToStr(run_remote_rusage);
	if( !myad->InsertAttr("RunRemoteUsage", rs) ) {
		free(rs);
		delete myad;
		return NULL;
	}
	free(rs);
	rs = rusageToStr(total_local_rusage);
	if( !myad->InsertAttr("TotalLocalUsage", rs) ) {
		free(rs);
		delete myad;
		return NULL;
	}
	free(rs);
	rs = rusageToStr(total_remote_rusage);
	if( !myad->InsertAttr("TotalRemoteUsage", rs) ) {
		free(rs);
		delete myad;
		return NULL;
	}
	free(rs);

	if( !myad->InsertAttr("SentBytes", sent_bytes) ) {
		delete myad;
		return NULL;
	}
	if( !myad->InsertAttr("ReceivedBytes", recvd_bytes) ) {
		delete myad;
		return NULL;
	}
	if( !myad->InsertAttr("TotalSentBytes", total_sent_bytes)  ) {
		delete myad;
		return NULL;
	}
	if( !myad->InsertAttr("TotalReceivedBytes", total_recvd_bytes) ) {
		delete myad;
		return NULL;
	}

	if( toeTag ) {
	    classad::ExprTree * tt = toeTag->Copy();
		if(! myad->Insert(ATTR_JOB_TOE, tt)) {
			delete myad;
			return NULL;
		}
	}

	return myad;
}

void
JobTerminatedEvent::initFromClassAd(ClassAd* ad)
{
	ULogEvent::initFromClassAd(ad);

	if( !ad ) return;

	initUsageFromAd(*ad);

	int reallybool;
	if( ad->LookupInteger("TerminatedNormally", reallybool) ) {
		normal = reallybool ? TRUE : FALSE;
	}

	ad->LookupInteger("ReturnValue", returnValue);
	ad->LookupInteger("TerminatedBySignal", signalNumber);

	ad->LookupString("CoreFile", core_file);

	std::string multi;
	if( ad->LookupString("RunLocalUsage", multi) ) {
		strToRusage(multi.c_str(), run_local_rusage);
	}
	if( ad->LookupString("RunRemoteUsage", multi) ) {
		strToRusage(multi.c_str(), run_remote_rusage);
	}
	if( ad->LookupString("TotalLocalUsage", multi) ) {
		strToRusage(multi.c_str(), total_local_rusage);
	}
	if( ad->LookupString("TotalRemoteUsage", multi) ) {
		strToRusage(multi.c_str(), total_remote_rusage);
	}

	ad->LookupFloat("SentBytes", sent_bytes);
	ad->LookupFloat("ReceivedBytes", recvd_bytes);
	ad->LookupFloat("TotalSentBytes", total_sent_bytes);
	ad->LookupFloat("TotalReceivedBytes", total_recvd_bytes);

	if( toeTag ) { delete toeTag; }

	ExprTree * fail = ad->Lookup(ATTR_JOB_TOE);
	classad::ClassAd * ca = dynamic_cast<classad::ClassAd *>( fail );
	if( ca ) { toeTag = new classad::ClassAd( * ca ); }
}

JobImageSizeEvent::JobImageSizeEvent(void)
{
	eventNumber = ULOG_IMAGE_SIZE;
	image_size_kb = -1;
	resident_set_size_kb = 0;
	proportional_set_size_kb = -1;
	memory_usage_mb = -1;
}


JobImageSizeEvent::~JobImageSizeEvent(void)
{
}


bool
JobImageSizeEvent::formatBody( std::string &out )
{
	if (formatstr_cat( out, "Image size of job updated: %lld\n", image_size_kb ) < 0)
		return false;

	// when talking to older starters, memory_usage, rss & pss may not be set
	if (memory_usage_mb >= 0 && 
		formatstr_cat( out, "\t%lld  -  MemoryUsage of job (MB)\n", memory_usage_mb ) < 0)
		return false;

	if (resident_set_size_kb >= 0 &&
		formatstr_cat( out, "\t%lld  -  ResidentSetSize of job (KB)\n", resident_set_size_kb ) < 0)
		return false;

	if (proportional_set_size_kb >= 0 &&
		formatstr_cat( out, "\t%lld  -  ProportionalSetSize of job (KB)\n", proportional_set_size_kb ) < 0)
		return false;

	return true;
}


int
JobImageSizeEvent::readEvent (ULogFile& file, bool & got_sync_line)
{
	std::string str;
	if ( ! read_line_value("Image size of job updated: ", str, file, got_sync_line) || 
		! YourStringDeserializer(str.c_str()).deserialize_int(&image_size_kb))
	{
		return 0;
	}

	// These fields were added to this event in 2012, so we need to tolerate the
	// situation when they are not there in the log that we read back.
	memory_usage_mb = -1;
	resident_set_size_kb = 0;
	proportional_set_size_kb = -1;

	for (;;) {
		char sz[250];

		if ( ! read_optional_line(file, got_sync_line, sz, sizeof(sz))) {
			break;
		}

		// line should be a number, followed by "  -  " followed by an attribute name
		// parse the number, then find the start of the attribute and null terminate it.
		char * p = sz;
		const char * lbl = NULL;
		while (*p && isspace(*p)) ++p;
		char *endptr = NULL;
		long long val = strtoll(p,&endptr,10);
		if (endptr != p && isspace(*endptr)) {
			p = endptr;
			while (*p && isspace(*p)) ++p;
			if (*p == '-')  {
				++p;
				while (*p && isspace(*p)) ++p;
				lbl = p;
				while (*p && ! isspace(*p)) ++p;
				*p = 0;
			}
		}

		if ( ! lbl) {
			break;
		} else {
			if (MATCH == strcasecmp(lbl,"MemoryUsage")) {
				memory_usage_mb = val;
			} else if (MATCH == strcasecmp(lbl, "ResidentSetSize")) {
				resident_set_size_kb = val;
			} else if (MATCH == strcasecmp(lbl, "ProportionalSetSize")) {
				proportional_set_size_kb = val;
			} else {
				break;
			}
		}
	}

	return 1;
}

ClassAd*
JobImageSizeEvent::toClassAd(bool event_time_utc)
{
	ClassAd* myad = ULogEvent::toClassAd(event_time_utc);
	if( !myad ) return NULL;

	if( image_size_kb >= 0 ) {
		if( !myad->Assign("Size", image_size_kb) ) return NULL;
	}
	if( memory_usage_mb >= 0 ) {
		if( !myad->Assign("MemoryUsage", memory_usage_mb) ) return NULL;
	}
	if( resident_set_size_kb >= 0 ) {
		if( !myad->Assign("ResidentSetSize", resident_set_size_kb) ) return NULL;
	}
	if( proportional_set_size_kb >= 0 ) {
		if( !myad->Assign("ProportionalSetSize", proportional_set_size_kb) ) return NULL;
	}

	return myad;
}

void
JobImageSizeEvent::initFromClassAd(ClassAd* ad)
{
	ULogEvent::initFromClassAd(ad);

	if( !ad ) return;

	// default these fields, they were added in 2012.
	memory_usage_mb = -1;
	resident_set_size_kb = 0;
	proportional_set_size_kb = -1;

	ad->LookupInteger("Size", image_size_kb);
	ad->LookupInteger("MemoryUsage", memory_usage_mb);
	ad->LookupInteger("ResidentSetSize", resident_set_size_kb);
	ad->LookupInteger("ProportionalSetSize", proportional_set_size_kb);
}

ShadowExceptionEvent::ShadowExceptionEvent (void)
{
	eventNumber = ULOG_SHADOW_EXCEPTION;
	sent_bytes = recvd_bytes = 0.0;
	began_execution = FALSE;
}

ShadowExceptionEvent::~ShadowExceptionEvent (void)
{
}

int
ShadowExceptionEvent::readEvent (ULogFile& file, bool & got_sync_line)
{
	std::string line;
	if ( ! read_line_value("Shadow exception!", line, file, got_sync_line)) {
		return 0;
	}
	// read message
	if ( ! read_optional_line(message, file, got_sync_line, true, true)) {
		return 1; // backwards compatibility
	}

	// read transfer info
	if ( ! read_optional_line(line, file, got_sync_line) ||
		(1 != sscanf (line.c_str(), "\t%lf  -  Run Bytes Sent By Job", &sent_bytes)) ||
		! read_optional_line(line, file, got_sync_line) ||
		(1 != sscanf (line.c_str(), "\t%lf  -  Run Bytes Received By Job", &recvd_bytes)))
	{
		return 1;				// backwards compatibility
	}

	return 1;
}

bool
ShadowExceptionEvent::formatBody( std::string &out )
{
	if (formatstr_cat( out, "Shadow exception!\n\t" ) < 0)
		return false;
	if (formatstr_cat( out, "%s\n", message.c_str() ) < 0)
		return false;

	if (formatstr_cat( out, "\t%.0f  -  Run Bytes Sent By Job\n", sent_bytes ) < 0 ||
		formatstr_cat( out, "\t%.0f  -  Run Bytes Received By Job\n",
				 recvd_bytes ) < 0)
		return true;				// backwards compatibility

	return true;
}

ClassAd*
ShadowExceptionEvent::toClassAd(bool event_time_utc)
{
	bool     success = true;
	ClassAd* myad = ULogEvent::toClassAd(event_time_utc);
	if( myad ) {
		if( !myad->InsertAttr("Message", message) ) {
			success = false;
		}

		if( !myad->InsertAttr("SentBytes", sent_bytes) ) {
			success = false;
		}
		if( !myad->InsertAttr("ReceivedBytes", recvd_bytes) ) {
			success = false;
		}
	}
	if (!success) {
		delete myad;
		myad = NULL;
	}
	return myad;
}

void
ShadowExceptionEvent::initFromClassAd(ClassAd* ad)
{
	ULogEvent::initFromClassAd(ad);

	if( !ad ) return;

	if ( ! ad->LookupString("Message", message)) {
		message.clear();
	}

	ad->LookupFloat("SentBytes", sent_bytes);
	ad->LookupFloat("ReceivedBytes", recvd_bytes);
}

JobSuspendedEvent::JobSuspendedEvent (void)
{
	eventNumber = ULOG_JOB_SUSPENDED;
	num_pids = -1;
}

JobSuspendedEvent::~JobSuspendedEvent (void)
{
}

int
JobSuspendedEvent::readEvent (ULogFile& file, bool & got_sync_line)
{
	std::string line;
	if ( ! read_line_value("Job was suspended.", line, file, got_sync_line)) {
		return 0;
	}
	if ( ! read_optional_line(line, file, got_sync_line) ||
		(1 != sscanf (line.c_str(), "\tNumber of processes actually suspended: %d", &num_pids)))
	{
		return 0;
	}

	return 1;
}


bool
JobSuspendedEvent::formatBody( std::string &out )
{
	if (formatstr_cat( out, "Job was suspended.\n\t" ) < 0)
		return false;
	if (formatstr_cat( out, "Number of processes actually suspended: %d\n",
			num_pids ) < 0)
		return false;

	return true;
}

ClassAd*
JobSuspendedEvent::toClassAd(bool event_time_utc)
{
	ClassAd* myad = ULogEvent::toClassAd(event_time_utc);
	if( !myad ) return NULL;

	if( !myad->InsertAttr("NumberOfPIDs", num_pids) ) {
		delete myad;
		return NULL;
	}

	return myad;
}

void
JobSuspendedEvent::initFromClassAd(ClassAd* ad)
{
	ULogEvent::initFromClassAd(ad);

	if( !ad ) return;

	ad->LookupInteger("NumberOfPIDs", num_pids);
}

JobUnsuspendedEvent::JobUnsuspendedEvent (void)
{
	eventNumber = ULOG_JOB_UNSUSPENDED;
}

JobUnsuspendedEvent::~JobUnsuspendedEvent (void)
{
}

int
JobUnsuspendedEvent::readEvent (ULogFile& file, bool & got_sync_line)
{
	std::string line;
	if ( ! read_line_value("Job was unsuspended.", line, file, got_sync_line)) {
		return 0;
	}

	return 1;
}

bool
JobUnsuspendedEvent::formatBody( std::string &out )
{
	if (formatstr_cat( out, "Job was unsuspended.\n" ) < 0)
		return false;

	return true;
}

ClassAd*
JobUnsuspendedEvent::toClassAd(bool event_time_utc)
{
	ClassAd* myad = ULogEvent::toClassAd(event_time_utc);
	if( !myad ) return NULL;

	return myad;
}

void
JobUnsuspendedEvent::initFromClassAd(ClassAd* ad)
{
	ULogEvent::initFromClassAd(ad);
}

JobHeldEvent::JobHeldEvent (void)
{
	eventNumber = ULOG_JOB_HELD;
	code = 0;
	subcode = 0;
}


const char*
JobHeldEvent::getReason( void ) const
{
	return reason.c_str();
}

int
JobHeldEvent::getReasonCode( void ) const
{
	return code;
}

int
JobHeldEvent::getReasonSubCode( void ) const
{
	return subcode;
}

int
JobHeldEvent::readEvent( ULogFile& file, bool & got_sync_line )
{
	reason.clear();
	code = subcode = 0;

	std::string line;
	if ( ! read_line_value("Job was held.", line, file, got_sync_line)) {
		return 0;
	}
	// try to read the reason, this is optional
	if ( ! read_optional_line(line, file, got_sync_line, true)) {
		return 1;	// backwards compatibility
	}
	trim(line);
	if (line != "Reason unspecified") {
		reason = line;
	}

	int incode = 0;
	int insubcode = 0;
	if ( ! read_optional_line(line, file, got_sync_line) ||
		(2 != sscanf(line.c_str(), "\tCode %d Subcode %d", &incode,&insubcode)))
	{
		return 1;	// backwards compatibility
	}

	code = incode;
	subcode = insubcode;

	return 1;
}


bool
JobHeldEvent::formatBody( std::string &out )
{
	if( formatstr_cat( out, "Job was held.\n" ) < 0 ) {
		return false;
	}
	if( !reason.empty() ) {
		if( formatstr_cat( out, "\t%s\n", reason.c_str() ) < 0 ) {
			return false;
		}
	} else {
		if( formatstr_cat( out, "\tReason unspecified\n" ) < 0 ) {
			return false;
		}
	}

	// write the codes
	if( formatstr_cat( out, "\tCode %d Subcode %d\n", code,subcode ) < 0 ) {
		return false;
	}

	return true;
}

ClassAd*
JobHeldEvent::toClassAd(bool event_time_utc)
{
	ClassAd* myad = ULogEvent::toClassAd(event_time_utc);
	if( !myad ) return NULL;

	if ( !reason.empty() ) {
		if( !myad->InsertAttr(ATTR_HOLD_REASON, reason) ) {
			delete myad;
			return NULL;
		}
	}
	if( !myad->InsertAttr(ATTR_HOLD_REASON_CODE, code) ) {
		delete myad;
		return NULL;
	}
	if( !myad->InsertAttr(ATTR_HOLD_REASON_SUBCODE, subcode) ) {
		delete myad;
		return NULL;
	}

	return myad;
}

void
JobHeldEvent::initFromClassAd(ClassAd* ad)
{
	ULogEvent::initFromClassAd(ad);

	if( !ad ) return;

	reason.clear();
	code = 0;
	subcode = 0;

	ad->LookupString(ATTR_HOLD_REASON, reason);
	ad->LookupInteger(ATTR_HOLD_REASON_CODE, code);
	ad->LookupInteger(ATTR_HOLD_REASON_SUBCODE, subcode);
}

JobReleasedEvent::JobReleasedEvent(void)
{
	eventNumber = ULOG_JOB_RELEASED;
}

const char*
JobReleasedEvent::getReason( void ) const
{
	return reason.c_str();
}


int
JobReleasedEvent::readEvent( ULogFile& file, bool & got_sync_line )
{
	std::string line;
	if ( ! read_line_value("Job was released.", line, file, got_sync_line)) {
		return 0;
	}
	// try to read the reason, this is optional
	if ( ! read_optional_line(line, file, got_sync_line, true)) {
		return 1;	// backwards compatibility
	}
	trim(line);
	if (! line.empty()) {
		reason = line;
	}

	return 1;
}


bool
JobReleasedEvent::formatBody( std::string &out )
{
	if( formatstr_cat( out, "Job was released.\n" ) < 0 ) {
		return false;
	}
	if( !reason.empty() ) {
		if( formatstr_cat( out, "\t%s\n", reason.c_str() ) < 0 ) {
			return false;
		} else {
			return true;
		}
	}
		// do we want to do anything else if there's no reason?
		// should we fail?  EXCEPT()?
	return true;
}

ClassAd*
JobReleasedEvent::toClassAd(bool event_time_utc)
{
	ClassAd* myad = ULogEvent::toClassAd(event_time_utc);
	if( !myad ) return NULL;

	if( !reason.empty() ) {
		if( !myad->InsertAttr("Reason", reason) ) {
			delete myad;
			return NULL;
		}
	}

	return myad;
}

void
JobReleasedEvent::initFromClassAd(ClassAd* ad)
{
	ULogEvent::initFromClassAd(ad);

	if( !ad ) return;

	reason.clear();
	ad->LookupString("Reason", reason);
}

static const int seconds = 1;
static const int minutes = 60 * seconds;
static const int hours = 60 * minutes;
static const int days = 24 * hours;

bool
ULogEvent::formatRusage (std::string &out, const rusage &usage)
{
	int usr_secs = usage.ru_utime.tv_sec;
	int sys_secs = usage.ru_stime.tv_sec;

	int usr_days, usr_hours, usr_minutes;
	int sys_days, sys_hours, sys_minutes;

	usr_days = usr_secs/days;  			usr_secs %= days;
	usr_hours = usr_secs/hours;			usr_secs %= hours;
	usr_minutes = usr_secs/minutes;		usr_secs %= minutes;
 
	sys_days = sys_secs/days;  			sys_secs %= days;
	sys_hours = sys_secs/hours;			sys_secs %= hours;
	sys_minutes = sys_secs/minutes;		sys_secs %= minutes;
 
	int retval;
	retval = formatstr_cat( out, "\tUsr %d %02d:%02d:%02d, Sys %d %02d:%02d:%02d",
					  usr_days, usr_hours, usr_minutes, usr_secs,
					  sys_days, sys_hours, sys_minutes, sys_secs );

	return (retval > 0);
}

bool
ULogEvent::readRusageLine (std::string &line, ULogFile& file, bool & got_sync_line, rusage &usage, int & remain)
{
	int usr_secs, usr_minutes, usr_hours, usr_days;
	int sys_secs, sys_minutes, sys_hours, sys_days;
	int retval;

	remain = -1;
	if ( ! read_optional_line(line, file, got_sync_line)) {
		return false;
	}

	retval = sscanf (line.c_str(), "\tUsr %d %d:%d:%d, Sys %d %d:%d:%d%n",
					&usr_days, &usr_hours, &usr_minutes, &usr_secs,
					&sys_days, &sys_hours, &sys_minutes, &sys_secs,
					&remain);
	if (retval < 8) {
		return false;
	}

	usage.ru_utime.tv_sec = usr_secs + usr_minutes*minutes + usr_hours*hours +
		usr_days*days;

	usage.ru_stime.tv_sec = sys_secs + sys_minutes*minutes + sys_hours*hours +
		sys_days*days;

	return true;
}

char*
ULogEvent::rusageToStr (const rusage &usage)
{
	char* result = (char*) malloc(128);
	ASSERT( result != NULL );

	int usr_secs = usage.ru_utime.tv_sec;
	int sys_secs = usage.ru_stime.tv_sec;

	int usr_days, usr_hours, usr_minutes;
	int sys_days, sys_hours, sys_minutes;

	usr_days = usr_secs/days;  			usr_secs %= days;
	usr_hours = usr_secs/hours;			usr_secs %= hours;
	usr_minutes = usr_secs/minutes;		usr_secs %= minutes;
 
	sys_days = sys_secs/days;  			sys_secs %= days;
	sys_hours = sys_secs/hours;			sys_secs %= hours;
	sys_minutes = sys_secs/minutes;		sys_secs %= minutes;
 
	snprintf(result, 128, "Usr %d %02d:%02d:%02d, Sys %d %02d:%02d:%02d",
			usr_days, usr_hours, usr_minutes, usr_secs,
			sys_days, sys_hours, sys_minutes, sys_secs);

	return result;
}

int
ULogEvent::strToRusage (const char* rusageStr, rusage & usage)
{
	int usr_secs, usr_minutes, usr_hours, usr_days;
	int sys_secs, sys_minutes, sys_hours, sys_days;
	int retval;

	while (isspace(*rusageStr)) ++rusageStr;

	retval = sscanf (rusageStr, "Usr %d %d:%d:%d, Sys %d %d:%d:%d",
					 &usr_days, &usr_hours, &usr_minutes, &usr_secs,
					 &sys_days, &sys_hours, &sys_minutes, &sys_secs);

	if (retval < 8)
	{
		return 0;
	}

	usage.ru_utime.tv_sec = usr_secs + usr_minutes*minutes + usr_hours*hours +
		usr_days*days;

	usage.ru_stime.tv_sec = sys_secs + sys_minutes*minutes + sys_hours*hours +
		sys_days*days;

	return (1);
}

// ----- the NodeExecuteEvent class
NodeExecuteEvent::NodeExecuteEvent(void)
{
	eventNumber = ULOG_NODE_EXECUTE;
	node = -1;
}

NodeExecuteEvent::~NodeExecuteEvent(void)
{
	if (executeProps) delete executeProps;
	executeProps = nullptr;
}


bool
NodeExecuteEvent::formatBody( std::string &out )
{
	int retval = formatstr_cat( out, "Node %d executing on host: %s\n", node, executeHost.c_str());
	if (retval < 0) {
		return false;
	}

	if ( ! slotName.empty()) {
		formatstr_cat(out, "\tSlotName: %s\n", slotName.c_str());
	}

	if (hasProps()) {
		// print sorted key=value pairs for the properties
		classad::References attrs;
		sGetAdAttrs(attrs, *executeProps);
		sPrintAdAttrs(out, *executeProps, attrs, "\t");
	}

	return true;
}


int
NodeExecuteEvent::readEvent (ULogFile& file, bool & got_sync_line)
{
	std::string line, attr;
	if( !readLine(line, file) ) {
		return 0; // EOF or error
	}
	if (is_sync_line(line.c_str())) {
		got_sync_line = true;
		return 0;
	}
	chomp(line);
	int retval = sscanf(line.c_str(), "Node %d executing on host: ", &node);
	if (retval == 1) {
		executeHost = strchr(line.c_str(), ':')+1;
		trim(executeHost);
	} else {
		return 0;
	}

	ExprTree * tree = nullptr;
	// read optional SlotName : <name>
	if ( ! read_optional_line(line, file, got_sync_line)) {
		return 1; // OK for there to be no more lines - backwards compatibility
	}
	if (starts_with(line, "\tSlotName:")) {
		slotName = strchr(line.c_str(),':') + 1,
			trim(slotName);
		trim_quotes(slotName, "\"");
	} else if (ParseLongFormAttrValue(line.c_str(), attr, tree)) {
		setProp().Insert(attr, tree);
	}
	if (got_sync_line) return 1;
	// read optional other properties
	while (read_optional_line(line, file, got_sync_line)) {
		if (ParseLongFormAttrValue(line.c_str(), attr, tree)) {
			setProp().Insert(attr, tree);
		}
	}
	return 1;
}

ClassAd*
NodeExecuteEvent::toClassAd(bool event_time_utc)
{
	ClassAd* myad = ULogEvent::toClassAd(event_time_utc);
	if( !myad ) return NULL;

	if( !executeHost.empty() ) {
		if( !myad->InsertAttr("ExecuteHost",executeHost) ) return NULL;
	}
	if( !myad->InsertAttr("Node", node) ) {
		delete myad;
		return NULL;
	}

	if( !slotName.empty()) {
		myad->Assign("SlotName", slotName);
	}
	if (hasProps()) {
		myad->Insert("ExecuteProps", executeProps->Copy());
	}


	return myad;
}

void
NodeExecuteEvent::initFromClassAd(ClassAd* ad)
{
	ULogEvent::initFromClassAd(ad);

	if( !ad ) return;

	ad->LookupString("ExecuteHost", executeHost);

	ad->LookupInteger("Node", node);

	slotName.clear();
	ad->LookupString("SlotName", slotName);

	delete executeProps;
	executeProps = nullptr;

	ClassAd * props = nullptr;
	ExprTree * tree = ad->Lookup("ExecuteProps");
	if (tree && tree->isClassad(&props)) {
		executeProps = static_cast<ClassAd*>(props->Copy());
	}
}

void
NodeExecuteEvent::setSlotName(char const *name)
{
	slotName = name ? name : "";
}

bool NodeExecuteEvent::hasProps() {
	return executeProps && executeProps->size() > 0;
}

ClassAd & NodeExecuteEvent::setProp() {
	if ( ! executeProps) executeProps = new ClassAd(); 
	return *executeProps;
}


// ----- NodeTerminatedEvent class
NodeTerminatedEvent::NodeTerminatedEvent(void) : TerminatedEvent()
{
	eventNumber = ULOG_NODE_TERMINATED;
	node = -1;
	pusageAd = NULL;
}


NodeTerminatedEvent::~NodeTerminatedEvent(void)
{
}


bool
NodeTerminatedEvent::formatBody( std::string &out )
{
	if( formatstr_cat( out, "Node %d terminated.\n", node ) < 0 ) {
		return false;
	}
	return TerminatedEvent::formatBody( out, "Node" );
}


int
NodeTerminatedEvent::readEvent( ULogFile& file, bool & got_sync_line )
{
	std::string str;
	if ( ! read_optional_line(str, file, got_sync_line) || 
		(1 != sscanf(str.c_str(), "Node %d terminated.", &node)))
	{
		return 0;
	}

	return TerminatedEvent::readEventBody( file, got_sync_line, "Node" );
}

ClassAd*
NodeTerminatedEvent::toClassAd(bool event_time_utc)
{
	ClassAd* myad = ULogEvent::toClassAd(event_time_utc);
	if( !myad ) return NULL;

	if (pusageAd) {
		myad->Update(*pusageAd);
	}

	if( !myad->InsertAttr("TerminatedNormally", normal ? true : false) ) {
		delete myad;
		return NULL;
	}
	if( !myad->InsertAttr("ReturnValue", returnValue) ) {
		delete myad;
		return NULL;
	}
	if( !myad->InsertAttr("TerminatedBySignal", signalNumber) ) {
		delete myad;
		return NULL;
	}

	if( !core_file.empty() ) {
		if( !myad->InsertAttr("CoreFile", core_file) ) {
			delete myad;
			return NULL;
		}
	}

	char* rs = rusageToStr(run_local_rusage);
	if( !myad->InsertAttr("RunLocalUsage", rs) ) {
		free(rs);
		delete myad;
		return NULL;
	}
	free(rs);
	rs = rusageToStr(run_remote_rusage);
	if( !myad->InsertAttr("RunRemoteUsage", rs) ) {
		free(rs);
		delete myad;
		return NULL;
	}
	free(rs);
	rs = rusageToStr(total_local_rusage);
	if( !myad->InsertAttr("TotalLocalUsage", rs) ) {
		free(rs);
		delete myad;
		return NULL;
	}
	free(rs);
	rs = rusageToStr(total_remote_rusage);
	if( !myad->InsertAttr("TotalRemoteUsage", rs) ) {
		free(rs);
		delete myad;
		return NULL;
	}
	free(rs);

	if( !myad->InsertAttr("SentBytes", sent_bytes) ) {
		delete myad;
		return NULL;
	}
	if( !myad->InsertAttr("ReceivedBytes", recvd_bytes) ) {
		delete myad;
		return NULL;
	}
	if( !myad->InsertAttr("TotalSentBytes", total_sent_bytes) ) {
		delete myad;
		return NULL;
	}
	if( !myad->InsertAttr("TotalReceivedBytes", total_recvd_bytes) ) {
		delete myad;
		return NULL;
	}

	if( node >= 0 ) {
		if( !myad->InsertAttr("Node", node) ) {
			delete myad;
			return NULL;
		}
	}

	return myad;
}

void
NodeTerminatedEvent::initFromClassAd(ClassAd* ad)
{
	ULogEvent::initFromClassAd(ad);

	if( !ad ) return;

	initUsageFromAd(*ad);

	int reallybool;
	if( ad->LookupInteger("TerminatedNormally", reallybool) ) {
		normal = reallybool ? TRUE : FALSE;
	}

	ad->LookupInteger("ReturnValue", returnValue);
	ad->LookupInteger("TerminatedBySignal", signalNumber);

	ad->LookupString("CoreFile", core_file);

	std::string multi;
	if( ad->LookupString("RunLocalUsage", multi) ) {
		strToRusage(multi.c_str(), run_local_rusage);
	}
	if( ad->LookupString("RunRemoteUsage", multi) ) {
		strToRusage(multi.c_str(), run_remote_rusage);
	}
	if( ad->LookupString("TotalLocalUsage", multi) ) {
		strToRusage(multi.c_str(), total_local_rusage);
	}
	if( ad->LookupString("TotalRemoteUsage", multi) ) {
		strToRusage(multi.c_str(), total_remote_rusage);
	}

	ad->LookupFloat("SentBytes", sent_bytes);
	ad->LookupFloat("ReceivedBytes", recvd_bytes);
	ad->LookupFloat("TotalSentBytes", total_sent_bytes);
	ad->LookupFloat("TotalReceivedBytes", total_recvd_bytes);

	ad->LookupInteger("Node", node);
}

// ----- PostScriptTerminatedEvent class

PostScriptTerminatedEvent::PostScriptTerminatedEvent(void) :
	dagNodeNameLabel ("DAG Node: "),
	dagNodeNameAttr ("DAGNodeName")
{
	eventNumber = ULOG_POST_SCRIPT_TERMINATED;
	normal = false;
	returnValue = -1;
	signalNumber = -1;
}

bool
PostScriptTerminatedEvent::formatBody( std::string &out )
{
    if( formatstr_cat( out, "POST Script terminated.\n" ) < 0 ) {
        return false;
    }

    if( normal ) {
        if( formatstr_cat( out, "\t(1) Normal termination (return value %d)\n",
					 returnValue ) < 0 ) {
            return false;
        }
    } else {
        if( formatstr_cat( out, "\t(0) Abnormal termination (signal %d)\n",
					 signalNumber ) < 0 ) {
            return false;
        }
    }

    if( !dagNodeName.empty() ) {
        if( formatstr_cat( out, "    %s%.8191s\n",
					 dagNodeNameLabel, dagNodeName.c_str() ) < 0 ) {
            return false;
        }
    }

    return true;
}


int
PostScriptTerminatedEvent::readEvent( ULogFile& file, bool & got_sync_line )
{
		// first clear any existing DAG node name
    dagNodeName.clear();

	std::string line;
	if ( ! read_line_value("POST Script terminated.", line, file, got_sync_line)) {
		return 0;
	}
	char buf[128];
	int tmp;
	if ( ! read_optional_line(line, file, got_sync_line) ||
		(2 != sscanf(line.c_str(), "\t(%d) %127[^\r\n]", &tmp, buf)))
	{
		return 0;
	}
	if( tmp == 1 ) {
		normal = true;
	} else {
		normal = false;
	}
	if( normal ) {
		if( sscanf( buf, "Normal termination (return value %d)",
			&returnValue ) != 1 ) {
				return 0;
		}
	} else {
		if( sscanf( buf, "Abnormal termination (signal %d)",
			&signalNumber ) != 1 ) {
				return 0;
		}
	}

	// see if the next line contains an optional DAG node name string,
	if ( ! read_optional_line(line, file, got_sync_line)) {
		return 1;
	}
	trim(line);
	if (starts_with(line, dagNodeNameLabel)) {
		size_t label_len = strlen( dagNodeNameLabel );
		dagNodeName = line.c_str() + label_len;
	}

    return 1;
}

ClassAd*
PostScriptTerminatedEvent::toClassAd(bool event_time_utc)
{
	ClassAd* myad = ULogEvent::toClassAd(event_time_utc);
	if( !myad ) return NULL;

	if( !myad->InsertAttr("TerminatedNormally", normal ? true : false) ) {
		delete myad;
		return NULL;
	}
	if( returnValue >= 0 ) {
		if( !myad->InsertAttr("ReturnValue", returnValue) ) {
			delete myad;
			return NULL;
		}
	}
	if( signalNumber >= 0 ) {
		if( !myad->InsertAttr("TerminatedBySignal", signalNumber) ) {
			delete myad;
			return NULL;
		}
	}
	if( !dagNodeName.empty() ) {
		if( !myad->InsertAttr( dagNodeNameAttr, dagNodeName ) ) {
			delete myad;
			return NULL;
		}
	}

	return myad;
}

void
PostScriptTerminatedEvent::initFromClassAd(ClassAd* ad)
{
	ULogEvent::initFromClassAd(ad);

	if( !ad ) return;

	int reallybool;
	if( ad->LookupInteger("TerminatedNormally", reallybool) ) {
		normal = reallybool ? TRUE : FALSE;
	}

	ad->LookupInteger("ReturnValue", returnValue);
	ad->LookupInteger("TerminatedBySignal", signalNumber);

	dagNodeName.clear();
	ad->LookupString( dagNodeNameAttr, dagNodeName );
}


// ----- JobDisconnectedEvent class

JobDisconnectedEvent::JobDisconnectedEvent(void)
{
	eventNumber = ULOG_JOB_DISCONNECTED;
}

bool
JobDisconnectedEvent::formatBody( std::string &out )
{
	if( disconnect_reason.empty() ) {
		dprintf(D_ALWAYS, "JobDisconnectedEvent::formatBody() called without "
		        "disconnect_reason\n");
		return false;
	}
	if( startd_addr.empty() ) {
		dprintf(D_ALWAYS, "JobDisconnectedEvent::formatBody() called without "
		        "startd_addr\n");
		return false;
	}
	if( startd_name.empty() ) {
		dprintf(D_ALWAYS, "JobDisconnectedEvent::formatBody() called without "
		        "startd_name\n");
		return false;
	}

	if( formatstr_cat(out, "Job disconnected, attempting to reconnect\n") < 0 ) {
		return false;
	}
	if( formatstr_cat( out, "    %.8191s\n", disconnect_reason.c_str() ) < 0 ) {
		return false;
	}
	if( formatstr_cat( out, "    Trying to reconnect to %s %s\n",
					   startd_name.c_str(), startd_addr.c_str() ) < 0 ) {
		return false;
	}
	return true;
}


int
JobDisconnectedEvent::readEvent( ULogFile& file, bool & /*got_sync_line*/ )
{
	std::string line;
		// the first line contains no useful information for us, but
		// it better be there or we've got a parse error.
	if( ! readLine(line, file) ) {
		return 0;
	}

	if( readLine(line, file) && line[0] == ' ' && line[1] == ' '
		&& line[2] == ' ' && line[3] == ' ' && line[4] )
	{
		chomp(line);
		disconnect_reason = line.c_str()+4;
	} else {
		return 0;
	}

	if( ! readLine(line, file) ) {
		return 0;
	}
	chomp(line);
	if( replace_str(line, "    Trying to reconnect to ", "") ) {
		size_t i = line.find( ' ' );
		if( i != std::string::npos ) {
			startd_addr = line.c_str()+(i+1);
			line.erase( i );
			startd_name = line.c_str();
		} else {
			return 0;
		}
	} else {
		return 0;
	}

	return 1;
}


ClassAd*
JobDisconnectedEvent::toClassAd(bool event_time_utc)
{
	if( disconnect_reason.empty() ) {
		dprintf(D_ALWAYS, "JobDisconnectedEvent::toClassAd() called without"
		        "disconnect_reason");
		return nullptr;
	}
	if( startd_addr.empty() ) {
		dprintf(D_ALWAYS, "JobDisconnectedEvent::toClassAd() called without "
		        "startd_addr");
		return nullptr;
	}
	if( startd_name.empty() ) {
		dprintf(D_ALWAYS, "JobDisconnectedEvent::toClassAd() called without "
		        "startd_name");
		return nullptr;
	}

	ClassAd* myad = ULogEvent::toClassAd(event_time_utc);
	if( !myad ) {
		return NULL;
	}

	if( !myad->InsertAttr("StartdAddr", startd_addr) ) {
		delete myad;
		return NULL;
	}
	if( !myad->InsertAttr("StartdName", startd_name) ) {
		delete myad;
		return NULL;
	}
	if( !myad->InsertAttr("DisconnectReason", disconnect_reason) ) {
		delete myad;
		return NULL;
	}

	std::string line = "Job disconnected, attempting to reconnect";
	if( !myad->InsertAttr("EventDescription", line) ) {
		delete myad;
		return NULL;
	}

	return myad;
}


void
JobDisconnectedEvent::initFromClassAd( ClassAd* ad )
{
	ULogEvent::initFromClassAd(ad);

	if( !ad ) {
		return;
	}

	ad->LookupString( "DisconnectReason", disconnect_reason );

	ad->LookupString( "StartdAddr", startd_addr );

	ad->LookupString( "StartdName", startd_name );
}


// ----- JobReconnectedEvent class

JobReconnectedEvent::JobReconnectedEvent(void)
{
	eventNumber = ULOG_JOB_RECONNECTED;
}

bool
JobReconnectedEvent::formatBody( std::string &out )
{
	if( startd_addr.empty() ) {
		dprintf(D_ALWAYS, "JobReconnectedEvent::formatBody() called without "
		        "startd_addr");
		return false;
	}
	if( startd_name.empty() ) {
		dprintf(D_ALWAYS, "JobReconnectedEvent::formatBody() called without "
		        "startd_name");
		return false;
	}
	if( starter_addr.empty() ) {
		dprintf(D_ALWAYS, "JobReconnectedEvent::formatBody() called without "
		        "starter_addr");
		return false;
	}

	if( formatstr_cat( out, "Job reconnected to %s\n", startd_name.c_str() ) < 0 ) {
		return false;
	}
	if( formatstr_cat( out, "    startd address: %s\n", startd_addr.c_str() ) < 0 ) {
		return false;
	}
	if( formatstr_cat( out, "    starter address: %s\n", starter_addr.c_str() ) < 0 ) {
		return false;
	}
	return true;
}


int
JobReconnectedEvent::readEvent( ULogFile& file, bool & /*got_sync_line*/ )
{
	std::string line;

	if( readLine(line, file) &&
		replace_str(line, "Job reconnected to ", "") )
	{
		chomp(line);
		startd_name = line;
	} else {
		return 0;
	}

	if( readLine(line, file) &&
		replace_str(line, "    startd address: ", "") )
	{
		chomp(line);
		startd_addr = line;
	} else {
		return 0;
	}

	if( readLine(line, file) &&
		replace_str(line, "    starter address: ", "") )
	{
		chomp(line);
		starter_addr = line;
	} else {
		return 0;
	}

	return 1;
}


ClassAd*
JobReconnectedEvent::toClassAd(bool event_time_utc)
{
	if( startd_addr.empty() ) {
		dprintf(D_ALWAYS, "JobReconnectedEvent::toClassAd() called without "
		        "startd_addr");
		return nullptr;
	}
	if( startd_name.empty() ) {
		dprintf(D_ALWAYS, "JobReconnectedEvent::toClassAd() called without "
		        "startd_name");
		return nullptr;
	}
	if( starter_addr.empty() ) {
		dprintf(D_ALWAYS, "JobReconnectedEvent::toClassAd() called without "
		        "starter_addr");
		return nullptr;
	}

	ClassAd* myad = ULogEvent::toClassAd(event_time_utc);
	if( !myad ) {
		return NULL;
	}

	if( !myad->InsertAttr("StartdAddr", startd_addr) ) {
		delete myad;
		return NULL;
	}
	if( !myad->InsertAttr("StartdName", startd_name) ) {
		delete myad;
		return NULL;
	}
	if( !myad->InsertAttr("StarterAddr", starter_addr) ) {
		delete myad;
		return NULL;
	}
	if( !myad->InsertAttr("EventDescription", "Job reconnected") ) {
		delete myad;
		return NULL;
	}
	return myad;
}


void
JobReconnectedEvent::initFromClassAd( ClassAd* ad )
{
	ULogEvent::initFromClassAd(ad);

	if( !ad ) {
		return;
	}

	ad->LookupString( "StartdAddr", startd_addr );

	ad->LookupString( "StartdName", startd_name );

	ad->LookupString( "StarterAddr", starter_addr );
}


// ----- JobReconnectFailedEvent class

JobReconnectFailedEvent::JobReconnectFailedEvent(void)
{
	eventNumber = ULOG_JOB_RECONNECT_FAILED;
}

bool
JobReconnectFailedEvent::formatBody( std::string &out )
{
	if( reason.empty() ) {
		dprintf(D_ALWAYS, "JobReconnectFailedEvent::formatBody() called without "
		        "reason");
		return false;
	}
	if( startd_name.empty() ) {
		dprintf(D_ALWAYS, "JobReconnectFailedEvent::formatBody() called without "
		        "startd_name");
		return false;
	}

	if( formatstr_cat( out, "Job reconnection failed\n" ) < 0 ) {
		return false;
	}
	if( formatstr_cat( out, "    %.8191s\n", reason.c_str() ) < 0 ) {
		return false;
	}
	if( formatstr_cat( out, "    Can not reconnect to %s, rescheduling job\n",
					   startd_name.c_str() ) < 0 ) {
		return false;
	}
	return true;
}


int
JobReconnectFailedEvent::readEvent( ULogFile& file, bool & /*got_sync_line*/ )
{
	std::string line;

		// the first line contains no useful information for us, but
		// it better be there or we've got a parse error.
	if( ! readLine(line, file) ) {
		return 0;
	}

		// 2nd line is the reason
	if( readLine(line, file) && line[0] == ' ' && line[1] == ' '
		&& line[2] == ' ' && line[3] == ' ' && line[4] )
	{
		chomp(line);
		reason = line.c_str()+4;
	} else {
		return 0;
	}

		// 3rd line is who we tried to reconnect to
	if( readLine(line, file) &&
		replace_str(line, "    Can not reconnect to ", "") )
	{
			// now everything until the first ',' will be the name
		size_t i = line.find( ',' );
		if( i != std::string::npos ) {
			line.erase( i );
			startd_name = line;
		} else {
			return 0;
		}
	} else {
		return 0;
	}

	return 1;
}


ClassAd*
JobReconnectFailedEvent::toClassAd(bool event_time_utc)
{
	if( reason.empty() ) {
		dprintf(D_ALWAYS, "JobReconnectFailedEvent::toClassAd() called without "
		        "reason");
		return nullptr;
	}
	if( startd_name.empty() ) {
		dprintf(D_ALWAYS, "JobReconnectFailedEvent::toClassAd() called without "
		        "startd_name");
		return nullptr;
	}

	ClassAd* myad = ULogEvent::toClassAd(event_time_utc);
	if( !myad ) {
		return NULL;
	}

	if( !myad->InsertAttr("StartdName", startd_name) ) {
		delete myad;
		return NULL;
	}
	if( !myad->InsertAttr("Reason", reason) ) {
		delete myad;
		return NULL;
	}
	if( !myad->InsertAttr("EventDescription", "Job reconnect impossible: rescheduling job") ) {
		delete myad;
		return NULL;
	}
	return myad;
}


void
JobReconnectFailedEvent::initFromClassAd( ClassAd* ad )
{
	ULogEvent::initFromClassAd(ad);
	if( !ad ) {
		return;
	}

	ad->LookupString( "Reason", reason );

	ad->LookupString( "StartdName", startd_name );
}


// ----- the GridResourceUp class
GridResourceUpEvent::GridResourceUpEvent(void)
{
	eventNumber = ULOG_GRID_RESOURCE_UP;
}

bool
GridResourceUpEvent::formatBody( std::string &out )
{
	const char * unknown = "UNKNOWN";
	const char * resource = unknown;

	int retval = formatstr_cat( out, "Grid Resource Back Up\n" );
	if (retval < 0)
	{
		return false;
	}

	if ( !resourceName.empty() ) resource = resourceName.c_str();

	retval = formatstr_cat( out, "    GridResource: %.8191s\n", resource );
	if( retval < 0 ) {
		return false;
	}

	return true;
}

int
GridResourceUpEvent::readEvent (ULogFile& file, bool & got_sync_line)
{
	std::string line;
	if ( ! read_line_value("Grid Resource Back Up", line, file, got_sync_line)) {
		return 0;
	}
	if ( ! read_line_value("    GridResource: ", resourceName, file, got_sync_line)) {
		return 0;
	}

	return 1;
}

ClassAd*
GridResourceUpEvent::toClassAd(bool event_time_utc)
{
	ClassAd* myad = ULogEvent::toClassAd(event_time_utc);
	if( !myad ) return NULL;

	if( !resourceName.empty() ) {
		if( !myad->InsertAttr("GridResource", resourceName) ) {
			delete myad;
			return NULL;
		}
	}

	return myad;
}

void
GridResourceUpEvent::initFromClassAd(ClassAd* ad)
{
	ULogEvent::initFromClassAd(ad);

	if( !ad ) return;

	ad->LookupString("GridResource", resourceName);
}


// ----- the GridResourceDown class
GridResourceDownEvent::GridResourceDownEvent(void)
{
	eventNumber = ULOG_GRID_RESOURCE_DOWN;
}

bool
GridResourceDownEvent::formatBody( std::string &out )
{
	const char * unknown = "UNKNOWN";
	const char * resource = unknown;

	int retval = formatstr_cat( out, "Detected Down Grid Resource\n" );
	if (retval < 0)
	{
		return false;
	}

	if ( !resourceName.empty() ) resource = resourceName.c_str();

	retval = formatstr_cat( out, "    GridResource: %.8191s\n", resource );
	if( retval < 0 ) {
		return false;
	}

	return true;
}

int
GridResourceDownEvent::readEvent (ULogFile& file, bool & got_sync_line)
{
	std::string line;
	if ( ! read_line_value("Detected Down Grid Resource", line, file, got_sync_line)) {
		return 0;
	}
	if ( ! read_line_value("    GridResource: ", resourceName, file, got_sync_line)) {
		return 0;
	}

	return 1;
}

ClassAd*
GridResourceDownEvent::toClassAd(bool event_time_utc)
{
	ClassAd* myad = ULogEvent::toClassAd(event_time_utc);
	if( !myad ) return NULL;

	if( !resourceName.empty() ) {
		if( !myad->InsertAttr("GridResource", resourceName) ) {
			delete myad;
			return NULL;
		}
	}

	return myad;
}

void
GridResourceDownEvent::initFromClassAd(ClassAd* ad)
{
	ULogEvent::initFromClassAd(ad);

	if( !ad ) return;

	ad->LookupString("GridResource", resourceName);
}


// ----- the GridSubmitEvent class
GridSubmitEvent::GridSubmitEvent(void)
{
	eventNumber = ULOG_GRID_SUBMIT;
}

bool
GridSubmitEvent::formatBody( std::string &out )
{
	const char * unknown = "UNKNOWN";
	const char * resource = unknown;
	const char * job = unknown;

	int retval = formatstr_cat( out, "Job submitted to grid resource\n" );
	if (retval < 0)
	{
		return false;
	}

	if ( !resourceName.empty() ) resource = resourceName.c_str();
	if ( !jobId.empty() ) job = jobId.c_str();

	retval = formatstr_cat( out, "    GridResource: %.8191s\n", resource );
	if( retval < 0 ) {
		return false;
	}

	retval = formatstr_cat( out, "    GridJobId: %.8191s\n", job );
	if( retval < 0 ) {
		return false;
	}

	return true;
}

int
GridSubmitEvent::readEvent (ULogFile& file, bool & got_sync_line)
{
	std::string line;
	if ( ! read_line_value("Job submitted to grid resource", line, file, got_sync_line)) {
		return 0;
	}
	if ( ! read_line_value("    GridResource: ", resourceName, file, got_sync_line)) {
		return 0;
	}

	if ( ! read_line_value("    GridJobId: ", jobId, file, got_sync_line)) {
		return 0;
	}

	return 1;
}

ClassAd*
GridSubmitEvent::toClassAd(bool event_time_utc)
{
	ClassAd* myad = ULogEvent::toClassAd(event_time_utc);
	if( !myad ) return NULL;

	if( !resourceName.empty() ) {
		if( !myad->InsertAttr("GridResource", resourceName) ) {
			delete myad;
			return NULL;
		}
	}
	if( !jobId.empty() ) {
		if( !myad->InsertAttr("GridJobId", jobId) ) {
			delete myad;
			return NULL;
		}
	}

	return myad;
}

void
GridSubmitEvent::initFromClassAd(ClassAd* ad)
{
	ULogEvent::initFromClassAd(ad);

	if( !ad ) return;

	ad->LookupString("GridResource", resourceName);

	ad->LookupString("GridJobId", jobId);
}

// ----- the JobAdInformationEvent class
JobAdInformationEvent::JobAdInformationEvent(void)
{
	jobad = NULL;
	eventNumber = ULOG_JOB_AD_INFORMATION;
}

JobAdInformationEvent::~JobAdInformationEvent(void)
{
	if ( jobad ) delete jobad;
	jobad = NULL;
}

bool
JobAdInformationEvent::formatBody( std::string &out )
{
	return formatBody( out, jobad );
}

bool
JobAdInformationEvent::formatBody( std::string &out, ClassAd *jobad_arg )
{
    int retval = 0;	 // 0 == FALSE == failure

	formatstr_cat( out, "Job ad information event triggered.\n" );

	if ( jobad_arg ) {
		retval = sPrintAd( out, *jobad_arg );
	}

    return retval != 0;
}

int
JobAdInformationEvent::readEvent(ULogFile& file, bool & got_sync_line)
{
	std::string line;
	if ( ! read_line_value("Job ad information event triggered.", line, file, got_sync_line)) {
		return 0;
	}
	if ( jobad ) delete jobad;
	jobad = new ClassAd();

	int num_attrs = 0;
	while (read_optional_line(line, file, got_sync_line)) {
		if ( ! jobad->Insert(line.c_str())) {
			// dprintf(D_ALWAYS,"failed to create classad; bad expr = '%s'\n", line.c_str());
			return 0;
		}
		++num_attrs;
	}
	return num_attrs > 0;
}

ClassAd*
JobAdInformationEvent::toClassAd(bool event_time_utc)
{
	ClassAd* myad = ULogEvent::toClassAd(event_time_utc);
	if( !myad ) return NULL;

	MergeClassAds(myad,jobad,false);

		// Reset MyType in case MergeClassAds() clobbered it.
	SetMyTypeName(*myad, "JobAdInformationEvent");

	return myad;
}

void
JobAdInformationEvent::initFromClassAd(ClassAd* ad)
{
	ULogEvent::initFromClassAd(ad);

	if( !ad ) return;

	if ( !jobad ) delete jobad;

	jobad = new ClassAd( *ad );	// invoke copy constructor to make deep copy

	return;
}

void JobAdInformationEvent::Assign(const char *attr, const char * value)
{
	if ( ! jobad) jobad = new ClassAd();
	jobad->Assign(attr, value);
}
void JobAdInformationEvent::Assign(const char * attr, int value)
{
	if ( ! jobad) jobad = new ClassAd();
	jobad->Assign(attr, value);
}
void JobAdInformationEvent::Assign(const char * attr, long long value)
{
	if ( ! jobad) jobad = new ClassAd();
	jobad->Assign(attr, value);
}
void JobAdInformationEvent::Assign(const char * attr, double value)
{
	if ( ! jobad) jobad = new ClassAd();
	jobad->Assign(attr, value);
}
void JobAdInformationEvent::Assign(const char * attr, bool value)
{
	if ( ! jobad) jobad = new ClassAd();
	jobad->Assign(attr, value);
}

int
JobAdInformationEvent::LookupString (const char *attributeName, std::string &value) const
{
	if ( !jobad ) return 0;		// 0 = failure

	return jobad->LookupString(attributeName,value);
}

int
JobAdInformationEvent::LookupInteger (const char *attributeName, int & value) const
{
	if ( !jobad ) return 0;		// 0 = failure

	return jobad->LookupInteger(attributeName,value);
}

int
JobAdInformationEvent::LookupInteger (const char *attributeName, long long & value) const
{
	if ( !jobad ) return 0;		// 0 = failure

	return jobad->LookupInteger(attributeName,value);
}

int
JobAdInformationEvent::LookupFloat (const char *attributeName, double & value) const
{
	if ( !jobad ) return 0;		// 0 = failure

	return jobad->LookupFloat(attributeName,value);
}

int
JobAdInformationEvent::LookupBool  (const char *attributeName, bool & value) const
{
	if ( !jobad ) return 0;		// 0 = failure

	return jobad->LookupBool(attributeName,value);
}


// ----- the JobStatusUnknownEvent class
JobStatusUnknownEvent::JobStatusUnknownEvent(void)
{
	eventNumber = ULOG_JOB_STATUS_UNKNOWN;
}

JobStatusUnknownEvent::~JobStatusUnknownEvent(void)
{
}

bool
JobStatusUnknownEvent::formatBody( std::string &out )
{
	int retval = formatstr_cat( out, "The job's remote status is unknown\n" );
	if (retval < 0)
	{
		return false;
	}
	
	return true;
}

int JobStatusUnknownEvent::readEvent (ULogFile& file, bool & got_sync_line)
{
	std::string line;
	if ( ! read_line_value("The job's remote status is unknown", line, file, got_sync_line)) {
		return 0;
	}

	return 1;
}

ClassAd*
JobStatusUnknownEvent::toClassAd(bool event_time_utc)
{
	return ULogEvent::toClassAd(event_time_utc);
}

void
JobStatusUnknownEvent::initFromClassAd(ClassAd* ad)
{
	ULogEvent::initFromClassAd(ad);
}

// ----- the JobStatusKnownEvent class
JobStatusKnownEvent::JobStatusKnownEvent(void)
{
	eventNumber = ULOG_JOB_STATUS_KNOWN;
}

JobStatusKnownEvent::~JobStatusKnownEvent(void)
{
}

bool
JobStatusKnownEvent::formatBody( std::string &out )
{
	int retval = formatstr_cat( out, "The job's remote status is known again\n" );
	if (retval < 0)
	{
		return false;
	}

	return true;
}

int JobStatusKnownEvent::readEvent (ULogFile& file, bool & got_sync_line)
{
	std::string line;
	if ( ! read_line_value("The job's remote status is known again", line, file, got_sync_line)) {
		return 0;
	}

	return 1;
}

ClassAd*
JobStatusKnownEvent::toClassAd(bool event_time_utc)
{
	return ULogEvent::toClassAd(event_time_utc);
}

void
JobStatusKnownEvent::initFromClassAd(ClassAd* ad)
{
	ULogEvent::initFromClassAd(ad);
}



// ----- the JobStageInEvent class
JobStageInEvent::JobStageInEvent(void)
{
	eventNumber = ULOG_JOB_STAGE_IN;
}

JobStageInEvent::~JobStageInEvent(void)
{
}

bool
JobStageInEvent::formatBody( std::string &out )
{
	int retval = formatstr_cat( out, "Job is performing stage-in of input files\n" );
	if (retval < 0)
	{
		return false;
	}

	return true;
}

int
JobStageInEvent::readEvent (ULogFile& file, bool & got_sync_line)
{
	std::string line;
	if ( ! read_line_value("Job is performing stage-in of input files", line, file, got_sync_line)) {
		return 0;
	}

	return 1;
}

ClassAd*
JobStageInEvent::toClassAd(bool event_time_utc)
{
	return ULogEvent::toClassAd(event_time_utc);
}

void
JobStageInEvent::initFromClassAd(ClassAd* ad)
{
	ULogEvent::initFromClassAd(ad);
}


// ----- the JobStageOutEvent class
JobStageOutEvent::JobStageOutEvent(void)
{
	eventNumber = ULOG_JOB_STAGE_OUT;
}

JobStageOutEvent::~JobStageOutEvent(void)
{
}

bool
JobStageOutEvent::formatBody( std::string &out )
{
	int retval = formatstr_cat( out, "Job is performing stage-out of output files\n" );
	if (retval < 0)
	{
		return false;
	}

	return true;
}

int
JobStageOutEvent::readEvent (ULogFile& file, bool & got_sync_line)
{
	std::string line;
	if ( ! read_line_value("Job is performing stage-out of output files", line, file, got_sync_line)) {
		return 0;
	}

	return 1;
}

ClassAd*
JobStageOutEvent::toClassAd(bool event_time_utc)
{
	return ULogEvent::toClassAd(event_time_utc);
}

void JobStageOutEvent::initFromClassAd(ClassAd* ad)
{
	ULogEvent::initFromClassAd(ad);
}

// ----- the AttributeUpdate class
AttributeUpdate::AttributeUpdate(void)
{
	name = NULL;
	value = NULL;
	old_value = NULL;
	eventNumber = ULOG_ATTRIBUTE_UPDATE;
}

AttributeUpdate::~AttributeUpdate(void)
{
	if (name) {
		free(name);
	}
	if (value) {
		free(value);
	}
	if (old_value) {
		free(old_value);
	}
}

bool
AttributeUpdate::formatBody( std::string &out )
{
	int retval;
	if (old_value)
	{
		retval = formatstr_cat( out, "Changing job attribute %s from %s to %s\n", name, old_value, value );
	} else {
		retval = formatstr_cat( out, "Setting job attribute %s to %s\n", name, value );
	}

	if (retval < 0)
	{
		return false;
	}

	return true;
}

int
AttributeUpdate::readEvent(ULogFile& file, bool & got_sync_line)
{
	char buf1[4096], buf2[4096], buf3[4096];
	buf1[0] = '\0';
	buf2[0] = '\0';
	buf3[0] = '\0';

	if (name) { free(name); }
	if (value) { free(value); }
	if (old_value) { free(old_value); }
	name = value = old_value = NULL;

	std::string line;
	if ( ! read_optional_line(line, file, got_sync_line)) {
		return 0;
	}

	int retval = sscanf(line.c_str(), "Changing job attribute %s from %s to %s", buf1, buf2, buf3);
	if (retval < 0)
	{
		retval = sscanf(line.c_str(), "Setting job attribute %s to %s", buf1, buf3);
		if (retval < 0)
		{
			return 0;
		}
	}

	name = strdup(buf1);
	value = strdup(buf3);
	if (buf2[0] != '\0')
	{
		old_value = strdup(buf2);
	} else {
		old_value = NULL;
	}
	return 1;
}

ClassAd*
AttributeUpdate::toClassAd(bool event_time_utc)
{
	ClassAd* myad = ULogEvent::toClassAd(event_time_utc);
	if( !myad ) return NULL;

	if (name) {
		myad->InsertAttr("Attribute", name);
	}
	if (value) {
		myad->InsertAttr("Value", value);
	}

	return myad;
}

void
AttributeUpdate::initFromClassAd(ClassAd* ad)
{
	std::string buf;
	ULogEvent::initFromClassAd(ad);

	if( !ad ) return;

	if( ad->LookupString("Attribute", buf ) ) {
		name = strdup(buf.c_str());
	}
	if( ad->LookupString("Value", buf ) ) {
		value = strdup(buf.c_str());
	}
}

void
AttributeUpdate::setName(const char* attr_name)
{
	if (!attr_name) {
		return;
	}
	if (name) {
		free(name);
	}
	name = strdup(attr_name);
}

void
AttributeUpdate::setValue(const char* attr_value)
{
	if (!attr_value) {
		return;
	}
	if (value) {
		free(value);
	}
	value = strdup(attr_value);
}

void
AttributeUpdate::setOldValue(const char* attr_value)
{
	if (!attr_value) {
		return;
	}
	if (old_value) {
		free(old_value);
	}
	if (attr_value) {
		old_value = strdup(attr_value);
	}
}


PreSkipEvent::PreSkipEvent(void)
{
	eventNumber = ULOG_PRESKIP;
}

int PreSkipEvent::readEvent (ULogFile& file, bool & got_sync_line)
{
	skipEventLogNotes.clear();
	std::string line;

	// read the remainder of the event header line
	if ( ! read_optional_line(line, file, got_sync_line)) {
		return 0;
	}

	// This event must have a a notes line
	if ( ! read_optional_line(line, file, got_sync_line)) {
		return 0;
	}
		// some users of this library (dagman) depend on whitespace
		// being stripped from the beginning of the log notes field
	trim(line);
	skipEventLogNotes = line;

	return ( skipEventLogNotes.empty() )?0:1;
}

bool
PreSkipEvent::formatBody( std::string &out )
{
	int retval = formatstr_cat( out, "PRE script return value is PRE_SKIP value\n" );
		// 
	if (skipEventLogNotes.empty() || retval < 0)
	{
		return false;
	}
	retval = formatstr_cat( out, "    %.8191s\n", skipEventLogNotes.c_str() );
	if( retval < 0 ) {
		return false;
	}
	return true;
}

ClassAd* PreSkipEvent::toClassAd(bool event_time_utc)
{
	ClassAd* myad = ULogEvent::toClassAd(event_time_utc);
	if( !myad ) return NULL;

	if( !skipEventLogNotes.empty() ) {
		if( !myad->InsertAttr("SkipEventLogNotes",skipEventLogNotes) ) return NULL;
	}
	return myad;
}

void PreSkipEvent::initFromClassAd(ClassAd* ad)
{
	ULogEvent::initFromClassAd(ad);

	if( !ad ) return;
	ad->LookupString("SkipEventLogNotes", skipEventLogNotes);
}

// ----- the ClusterSubmitEvent class
ClusterSubmitEvent::ClusterSubmitEvent(void)
{
	eventNumber = ULOG_CLUSTER_SUBMIT;
}

void
ClusterSubmitEvent::setSubmitHost(char const *addr)
{
	submitHost = addr ? addr : "";
}

bool
ClusterSubmitEvent::formatBody( std::string &out )
{
	int retval = formatstr_cat (out, "Cluster submitted from host: %s\n", submitHost.c_str());
	if (retval < 0)
	{
		return false;
	}
	if( !submitEventLogNotes.empty() ) {
		retval = formatstr_cat( out, "    %.8191s\n", submitEventLogNotes.c_str() );
		if( retval < 0 ) {
			return false;
		}
	}
	if( !submitEventUserNotes.empty() ) {
		retval = formatstr_cat( out, "    %.8191s\n", submitEventUserNotes.c_str() );
		if( retval < 0 ) {
			return false;
		}
	}
	return true;
}

int
ClusterSubmitEvent::readEvent (ULogFile& file, bool & got_sync_line)
{
	if ( ! read_line_value("Cluster submitted from host: ", submitHost, file, got_sync_line)) {
		return 0;
	}

	// see if the next line contains an optional event notes string,
	if ( ! read_optional_line(submitEventLogNotes, file, got_sync_line, true, true)) {
		return 1;
	}

	// see if the next line contains an optional user event notes
	if ( ! read_optional_line(submitEventUserNotes, file, got_sync_line, true, true)) {
		return 1;
	}

	return 1;
}

ClassAd*
ClusterSubmitEvent::toClassAd(bool event_time_utc)
{
	ClassAd* myad = ULogEvent::toClassAd(event_time_utc);
	if( !myad ) return NULL;

	if( !submitHost.empty() ) {
		if( !myad->InsertAttr("SubmitHost",submitHost) ) return NULL;
	}

	return myad;
}


void
ClusterSubmitEvent::initFromClassAd(ClassAd* ad)
{
	ULogEvent::initFromClassAd(ad);

	if( !ad ) return;
	ad->LookupString("SubmitHost", submitHost);
}

// ----- the ClusterRemoveEvent class
ClusterRemoveEvent::ClusterRemoveEvent(void)
	: next_proc_id(0), next_row(0), completion(Incomplete)
{
	eventNumber = ULOG_CLUSTER_REMOVE;
}

ClusterRemoveEvent::~ClusterRemoveEvent(void) {}


#define CLUSTER_REMOVED_BANNER "Cluster removed"

bool
ClusterRemoveEvent::formatBody( std::string &out )
{
	int retval = formatstr_cat (out, CLUSTER_REMOVED_BANNER "\n");
	if (retval < 0)
		return false;
	// show progress.
	formatstr_cat(out, "\tMaterialized %d jobs from %d items.", next_proc_id, next_row);
	// and completion status
	if (completion <= Error) {
		formatstr_cat(out, "\tError %d\n", completion);
	} else if (completion == Complete) {
		out += "\tComplete\n";
	} else if (completion >= Paused) {
		out += "\tPaused\n";
	} else {
		out += "\tIncomplete\n";
	}
	// and optional notes
	if (!notes.empty()) { formatstr_cat(out, "\t%s\n", notes.c_str()); }
	return true;
}


int
ClusterRemoveEvent::readEvent (ULogFile& file, bool & got_sync_line)
{
	next_proc_id = next_row = 0;
	completion = Incomplete;
	notes.clear();

	// get the remainder of the first line (if any)
	// or rewind so we don't slurp up the next event delimiter
	char buf[BUFSIZ];
	if ( ! read_optional_line(file, got_sync_line, buf, sizeof(buf))) {
		return 1; // backwards compatibility
	}

	// be intentionally forgiving about the text here, so as not to introduce bugs if we change terminology
	if (strstr(buf, "remove") || strstr(buf,"Remove")) {
		if ( ! read_optional_line(file, got_sync_line, buf, sizeof(buf))) {
			return 1; // this field is optional
		}
	}

	const char * p = buf;
	while (isspace(*p)) ++p;

	// parse out progress
	if (2 == sscanf(p, "Materialized %d jobs from %d items.", &next_proc_id, &next_row)) {
		p = strstr(p, "items.") + 6;
		while (isspace(*p)) ++p;
	}
	// parse out completion
	if (starts_with_ignore_case(p, "error")) {
		int code = atoi(p+5);
		completion = (code < 0) ? (CompletionCode)code : Error;
	} else if (starts_with_ignore_case(p, "Complete")) {
		completion = Complete;
	} else if (starts_with_ignore_case(p, "Paused")) {
		completion = Paused;
	} else {
		completion = Incomplete;
	}

	// read the notes field.
	if ( ! read_optional_line(file, got_sync_line, buf, sizeof(buf))) {
		return 1; // this field is optional
	}

	chomp(buf);  // strip the newline
	p = buf;
	// discard leading spaces, and store the result as the notes
	while (isspace(*p)) ++p;
	if (*p) {
		notes = p;
	}

	return 1;
}

ClassAd*
ClusterRemoveEvent::toClassAd(bool event_time_utc)
{
	ClassAd* myad = ULogEvent::toClassAd(event_time_utc);
	if( !myad ) return NULL;

	if (!notes.empty()) {
		if( !myad->InsertAttr("Notes", notes) ) {
			delete myad;
			return NULL;
		}
	}
	if( !myad->InsertAttr("NextProcId", next_proc_id) ||
		!myad->InsertAttr("NextRow", next_row) ||
		!myad->InsertAttr("Completion", completion)
		) {
		delete myad;
		return NULL;
	}
	return myad;
}


void
ClusterRemoveEvent::initFromClassAd(ClassAd* ad)
{
	next_proc_id = next_row = 0;
	completion = Incomplete;
	notes.clear();

	ULogEvent::initFromClassAd(ad);

	if( !ad ) return;

	int code = Incomplete;
	ad->LookupInteger("Completion", code);
	completion = (CompletionCode)code;

	ad->LookupInteger("NextProcId", next_proc_id);
	ad->LookupInteger("NextRow", next_row);

	ad->LookupString("Notes", notes);
}

// ----- the FactoryPausedEvent class


#define FACTORY_PAUSED_BANNER "Job Materialization Paused"
#define FACTORY_RESUMED_BANNER "Job Materialization Resumed"

int
FactoryPausedEvent::readEvent (ULogFile& file, bool & got_sync_line)
{
	pause_code = 0;
	reason.clear();

	char buf[BUFSIZ];
	if ( ! read_optional_line(file, got_sync_line, buf, sizeof(buf))) {
		return 1; // backwards compatibility
	}

	// be intentionally forgiving about the text here, so as not to introduce bugs if we change terminology
	if (strstr(buf, "pause") || strstr(buf,"Pause")) {
		// got the "Paused" line, now get the next line.
		if ( ! read_optional_line(file, got_sync_line, buf, sizeof(buf))) {
			return 1; // this field is optional
		}
	}

	// The next line should be the pause reason.
	chomp(buf);  // strip the newline
	const char * p = buf;
	// discard leading spaces, and store the result as the reason
	while (isspace(*p)) ++p;
	if (*p) { reason = p; }

	// read the pause code and/or hold code, if they exist
	while ( read_optional_line(file, got_sync_line, buf, sizeof(buf)) )
	{
		char * endp;
		p = buf;

		p = strstr(p, "PauseCode ");
		if (p) {
			p += sizeof("PauseCode ")-1;
			pause_code = (int)strtoll(p,&endp,10);
			if ( ! strstr(endp, "HoldCode")) {
				continue;
			}
		} else {
			p = buf;
		}

		p = strstr(p, "HoldCode ");
		if (p) {
			p += sizeof("HoldCode ")-1;
			hold_code = (int)strtoll(p,&endp,10);
			continue;
		}

		break;
	}

	return 1;
}


bool
FactoryPausedEvent::formatBody( std::string &out )
{
	out += FACTORY_PAUSED_BANNER "\n";
	if ((!reason.empty()) || pause_code != 0) {
		formatstr_cat(out, "\t%s\n", reason.c_str());
	}
	if (pause_code != 0) {
		formatstr_cat(out, "\tPauseCode %d\n", pause_code);
	}
	if (hold_code != 0) {
		formatstr_cat(out, "\tHoldCode %d\n", hold_code);
	}
	return true;
}


ClassAd*
FactoryPausedEvent::toClassAd(bool event_time_utc)
{
	ClassAd* myad = ULogEvent::toClassAd(event_time_utc);
	if( !myad ) return NULL;

	if (!reason.empty()) {
		if( !myad->InsertAttr("Reason", reason) ) {
			delete myad;
			return NULL;
		}
	}
	if( !myad->InsertAttr("PauseCode", pause_code) ) {
		delete myad;
		return NULL;
	}
	if( !myad->InsertAttr("HoldCode", hold_code) ) {
		delete myad;
		return NULL;
	}

	return myad;
}


void
FactoryPausedEvent::initFromClassAd(ClassAd* ad)
{
	pause_code = 0;
	reason.clear();
	ULogEvent::initFromClassAd(ad);

	if( !ad ) return;

	ad->LookupString("Reason", reason);
	ad->LookupInteger("PauseCode", pause_code);
	ad->LookupInteger("HoldCode", hold_code);
}

void FactoryPausedEvent::setReason(const char* str)
{
	set_reason_member(reason, str);
}

// ----- the FactoryResumedEvent class

bool
FactoryResumedEvent::formatBody( std::string &out )
{
	out += FACTORY_RESUMED_BANNER "\n";
	if (!reason.empty()) {
		formatstr_cat(out, "\t%s\n", reason.c_str());
	}
	return true;
}

int
FactoryResumedEvent::readEvent (ULogFile& file, bool & got_sync_line)
{
	reason.clear();

	char buf[BUFSIZ];
	if ( ! read_optional_line(file, got_sync_line, buf, sizeof(buf))) {
		return 1; // backwards compatibility
	}

	// be intentionally forgiving about the text here, so as not to introduce bugs if we change terminology
	if (strstr(buf, "resume") || strstr(buf,"Resume")) {
		// got the "Resumed" line, now get the next line.
		if ( ! read_optional_line(file, got_sync_line, buf, sizeof(buf))) {
			return 1; // this field is optional
		}
	}

	// The next line should be the resume reason.
	chomp(buf);  // strip the newline
	const char * p = buf;
	// discard leading spaces, and store the result as the reason
	while (isspace(*p)) ++p;
	if (*p) { reason = p;}

	return 1;
}

ClassAd*
FactoryResumedEvent::toClassAd(bool event_time_utc)
{
	ClassAd* myad = ULogEvent::toClassAd(event_time_utc);
	if( !myad ) return NULL;

	if (!reason.empty()) {
		if( !myad->InsertAttr("Reason", reason) ) {
			delete myad;
			return NULL;
		}
	}
	return myad;
}


void
FactoryResumedEvent::initFromClassAd(ClassAd* ad)
{
	reason.clear();
	ULogEvent::initFromClassAd(ad);

	if( !ad ) return;

	ad->LookupString("Reason", reason);
}

void FactoryResumedEvent::setReason(const char* str)
{
	set_reason_member(reason, str);
}

//
// FileTransferEvent
//

const char * FileTransferEvent::FileTransferEventStrings[] = {
	"NONE",
	"Entered queue to transfer input files",
	"Started transferring input files",
	"Finished transferring input files",
	"Entered queue to transfer output files",
	"Started transferring output files",
	"Finished transferring output files"
};

FileTransferEvent::FileTransferEvent() : queueingDelay(-1),
  type( FileTransferEvent::NONE )
{
	eventNumber = ULOG_FILE_TRANSFER;
}

FileTransferEvent::~FileTransferEvent() { }

bool
FileTransferEvent::formatBody( std::string & out ) {
	if( type == FileTransferEventType::NONE ) {
		dprintf( D_ALWAYS, "Unspecified type in FileTransferEvent::formatBody()\n" );
		return false;
	} else if( FileTransferEventType::NONE < type
	           && type < FileTransferEventType::MAX ) {
		if( formatstr_cat( out, "%s\n",
		                   FileTransferEventStrings[type] ) < 0 ) {
			return false;
		}
	} else {
		dprintf( D_ALWAYS, "Unknown type in FileTransferEvent::formatBody()\n" );
		return false;
	}


	if( queueingDelay != -1 ) {
		if( formatstr_cat( out, "\tSeconds spent in queue: %lu\n",
		                   queueingDelay ) < 0 ) {
			return false;
		}
	}


	if(! host.empty()) {
		if( formatstr_cat( out, "\tTransferring to host: %s\n",
		                   host.c_str() ) < 0 ) {
			return false;
		}
	}


	return true;
}

int
FileTransferEvent::readEvent( ULogFile& f, bool & got_sync_line ) {
	// Require an 'optional' line  because read_line_value() requires a prefix.
	std::string eventString;
	if(! read_optional_line( eventString, f, got_sync_line )) {
		return 0;
	}

	// NONE is not a legal event in the log.
	bool foundEventString = false;
	for( int i = 1; i < FileTransferEventType::MAX; ++i ) {
		if( FileTransferEventStrings[i] == eventString ) {
			foundEventString = true;
			type = (FileTransferEventType)i;
			break;
		}
	}
	if(! foundEventString) { return false; }


	// Check for an optional line.
	std::string optionalLine;
	if(! read_optional_line( optionalLine, f, got_sync_line )) {
		return got_sync_line ? 1 : 0;
	}
	chomp(optionalLine);

	// Did we record the queueing delay?
	std::string prefix = "\tSeconds spent in queue: ";
	if( starts_with( optionalLine.c_str(), prefix.c_str() ) ) {
		std::string value = optionalLine.substr( prefix.length() );

		char * endptr = NULL;
		queueingDelay = strtol( value.c_str(), & endptr, 10 );
		if( endptr == NULL || endptr[0] != '\0' ) {
			return 0;
		}

		// If we read an optional line, check for the next one.
		if(! read_optional_line( optionalLine, f, got_sync_line )) {
			return got_sync_line ? 1 : 0;
		}
		chomp(optionalLine);
	}


	// Did we record the starter host?
	prefix = "\tTransferring to host: ";
	if( starts_with( optionalLine.c_str(), prefix.c_str() ) ) {
		host = optionalLine.substr( prefix.length() );

/*
		// If we read an optional line, check for the next one.
		if(! read_optional_line( optionalLine, f, got_sync_line )) {
			return got_sync_line ? 1 : 0;
		}
		chomp(optionalLine);
*/
	}


	return 1;
}

ClassAd *
FileTransferEvent::toClassAd(bool event_time_utc) {
	ClassAd * ad = ULogEvent::toClassAd(event_time_utc);
	if(! ad) { return NULL; }

	if(! ad->InsertAttr( "Type", (int)type )) {
		delete ad;
		return NULL;
	}

	if( queueingDelay != -1 ) {
		if(! ad->InsertAttr( "QueueingDelay", queueingDelay )) {
			delete ad;
			return NULL;
		}
	}

	if(! host.empty()) {
		if(! ad->InsertAttr( "Host", host )) {
			delete ad;
			return NULL;
		}
	}

	return ad;
}

void
FileTransferEvent::initFromClassAd( ClassAd * ad ) {
	ULogEvent::initFromClassAd( ad );

	int typePunning = -1;
	ad->LookupInteger( "Type", typePunning );
	if( typePunning != -1 ) {
		type = (FileTransferEventType)typePunning;
	}

	ad->LookupInteger( "QueueingDelay", queueingDelay );

	ad->LookupString( "Host", host );
}


void
ReserveSpaceEvent::initFromClassAd( ClassAd *ad )
{
	ULogEvent::initFromClassAd( ad );

	time_t expiry_time;
	if (ad->EvaluateAttrInt(ATTR_EXPIRATION_TIME, expiry_time)) {
		m_expiry = std::chrono::system_clock::from_time_t(expiry_time);
	}
	long long reserved_space;
	if (ad->EvaluateAttrInt(ATTR_RESERVED_SPACE, reserved_space)) {
		m_reserved_space = reserved_space;
	}
	std::string uuid;
	if (ad->EvaluateAttrString(ATTR_UUID, uuid)) {
		m_uuid = uuid;
	}
	std::string tag;
	if (ad->EvaluateAttrString(ATTR_TAG, tag)) {
		m_tag = tag;
	}
}


ClassAd *
ReserveSpaceEvent::toClassAd(bool event_time_utc) {
	std::unique_ptr<ClassAd> ad(ULogEvent::toClassAd(event_time_utc));
	if (!ad.get()) {return nullptr;}

	time_t expiry_time = std::chrono::system_clock::to_time_t(m_expiry);
	if (!ad->InsertAttr(ATTR_EXPIRATION_TIME, expiry_time)) {
		return nullptr;
	}

	if (!ad->InsertAttr(ATTR_RESERVED_SPACE, static_cast<long long>(m_reserved_space))) {
		return nullptr;
	}

	if (!ad->InsertAttr(ATTR_UUID, m_uuid)) {
		return nullptr;
	}

	if (!ad->InsertAttr(ATTR_TAG, m_tag)) {
		return nullptr;
	}
	return ad.release();
}


bool
ReserveSpaceEvent::formatBody(std::string &out)
{
	if (m_reserved_space &&
		formatstr_cat(out, "\n\tBytes reserved: %zu\n",
		m_reserved_space) < 0)
	{
		return false;
	}

	time_t expiry = std::chrono::system_clock::to_time_t(m_expiry);
	if (formatstr_cat(out, "\tReservation Expiration: %lu\n", expiry) < 0) {
		return false;
	}

	if (formatstr_cat(out, "\tReservation UUID: %s\n", m_uuid.c_str()) < 0) {
		return false;
	}

	if (formatstr_cat(out, "\tTag: %s\n", m_tag.c_str()) < 0) {
		return false;
	}
	return true;
}


int
ReserveSpaceEvent::readEvent(ULogFile&  fp, bool &got_sync_line) {
	std::string optionalLine;

		// Check for bytes reserved.
	if (!read_optional_line(optionalLine, fp, got_sync_line)) {
		return false;
	}
	chomp(optionalLine);
	std::string prefix = "Bytes reserved:";
	if (starts_with(optionalLine.c_str(), prefix.c_str())) {
		std::string bytes_str = optionalLine.substr(prefix.size());
		long long bytes_long;
		try {
			bytes_long = stoll(bytes_str);
		} catch (...) {
			dprintf(D_FULLDEBUG,
				"Unable to convert byte count to integer: %s\n",
				bytes_str.c_str());
			return false;
		}
		m_reserved_space = bytes_long;
	} else {
		dprintf(D_FULLDEBUG, "Bytes reserved line missing.\n");
		return false;
	}

		// Check the reservation expiry time.
	if (!read_optional_line(optionalLine, fp, got_sync_line)) {
		return false;
	}
	chomp(optionalLine);
	prefix = "\tReservation Expiration:";
	if (starts_with(optionalLine.c_str(), prefix.c_str())) {
		std::string expiry_str = optionalLine.substr(prefix.size());
		long long expiry_long;
		try {
			expiry_long = stoll(expiry_str);
		} catch (...) {
			dprintf(D_FULLDEBUG,
				"Unable to convert reservation expiration to integer: %s\n",
				expiry_str.c_str());
			return false;
		}
		m_expiry = std::chrono::system_clock::from_time_t(expiry_long);
	} else {
		dprintf(D_FULLDEBUG, "Reservation expiration line missing.\n");
		return false;
	}

		// Check the reservation UUID.
	if (!read_optional_line(optionalLine, fp, got_sync_line)) {
		return false;
	}
	prefix = "\tReservation UUID: ";
	if (starts_with(optionalLine.c_str(), prefix.c_str())) {
		m_uuid = optionalLine.substr(prefix.size());
	} else {
		dprintf(D_FULLDEBUG, "Reservation UUID line missing.\n");
		return false;
	}

		// Check the reservation tag.
	if (!read_optional_line(optionalLine, fp, got_sync_line)) {
		return false;
	}
	prefix = "\tTag: ";
	if (starts_with(optionalLine.c_str(), prefix.c_str())) {
		m_tag = optionalLine.substr(prefix.size());
	} else {
		dprintf(D_FULLDEBUG, "Reservation tag line missing.\n");
		return false;
	}


	return true;
}


std::string
ReserveSpaceEvent::generateUUID()
{
	// We do not link against libuuid when doing a static build.
	// Static builds are only used for the shadow - while the space
	// reservation events are intended for Win32 and the startd/starter
#if defined(WIN32)
	return "";
#else
	char uuid_str[37];
	uuid_t uuid;
	uuid_generate_random(uuid);
	uuid_unparse(uuid, uuid_str);
	return std::string(uuid_str, 36);
#endif
}

void
ReleaseSpaceEvent::initFromClassAd( ClassAd *ad )
{
	ULogEvent::initFromClassAd( ad );

	std::string uuid;
	if (ad->EvaluateAttrString(ATTR_UUID, uuid)) {
		m_uuid = uuid;
	}
}


ClassAd *
ReleaseSpaceEvent::toClassAd(bool event_time_utc) {
	std::unique_ptr<ClassAd> ad(ULogEvent::toClassAd(event_time_utc));
	if (!ad.get()) {return nullptr;}

	if (!ad->InsertAttr(ATTR_UUID, m_uuid)) {
		return nullptr;
	}

	return ad.release();
}


bool
ReleaseSpaceEvent::formatBody(std::string &out)
{
	if (formatstr_cat(out, "\n\tReservation UUID: %s\n", m_uuid.c_str()) < 0) {
		return false;
	}

	return true;
}


int
ReleaseSpaceEvent::readEvent(ULogFile& fp, bool &got_sync_line) {
	std::string optionalLine;

		// Check the reservation UUID.
	if (!read_optional_line(optionalLine, fp, got_sync_line)) {
		return false;
	}
	std::string prefix = "Reservation UUID: ";
	if (starts_with(optionalLine.c_str(), prefix.c_str())) {
		m_uuid= optionalLine.substr(prefix.size());
	} else {
		dprintf(D_FULLDEBUG, "Reservation UUID line missing.\n");
		return false;
	}

	return true;
}


void
FileCompleteEvent::initFromClassAd( ClassAd *ad )
{
	ULogEvent::initFromClassAd( ad );

	long long size;
	if (ad->EvaluateAttrInt(ATTR_SIZE, size)) {
		m_size = size;
	}

	ad->EvaluateAttrString(ATTR_CHECKSUM, m_checksum);

	ad->EvaluateAttrString(ATTR_CHECKSUM_TYPE, m_checksum_type);

	ad->EvaluateAttrString(ATTR_UUID, m_uuid);
}


ClassAd *
FileCompleteEvent::toClassAd(bool event_time_utc) {
	std::unique_ptr<ClassAd> ad(ULogEvent::toClassAd(event_time_utc));
	if (!ad.get()) {return nullptr;}

	if (!ad->InsertAttr(ATTR_SIZE, static_cast<long long>(m_size))) {
		return nullptr;
	}

	if (!ad->InsertAttr(ATTR_CHECKSUM, m_checksum)) {
		return nullptr;
	}

	if (!ad->InsertAttr(ATTR_CHECKSUM_TYPE, m_checksum_type)) {
		return nullptr;
	}

	if (!ad->InsertAttr(ATTR_UUID, m_uuid)) {
		return nullptr;
	}

	return ad.release();
}


bool
FileCompleteEvent::formatBody(std::string &out)
{
	if (formatstr_cat(out, "\n\tBytes: %zu\n", m_size) < 0)
	{
		return false;
	}

	if (formatstr_cat(out, "\tChecksum Value: %s\n", m_checksum.c_str()) < 0) {
		return false;
	}

	if (formatstr_cat(out, "\tChecksum Type: %s\n", m_checksum_type.c_str()) < 0) {
		return false;
	}

	if (formatstr_cat(out, "\tUUID: %s\n", m_uuid.c_str()) < 0) {
		return false;
	}

	return true;
}


int
FileCompleteEvent::readEvent(ULogFile& fp, bool &got_sync_line) {
	std::string optionalLine;

		// Check for filesize in bytes.
	if (!read_optional_line(optionalLine, fp, got_sync_line)) {
		return false;
	}
	chomp(optionalLine);
	std::string prefix = "Bytes:";
	if (starts_with(optionalLine.c_str(), prefix.c_str())) {
		std::string bytes_str = optionalLine.substr(prefix.size());
		long long bytes_long;
		try {
			bytes_long = stoll(bytes_str);
		} catch (...) {
			dprintf(D_FULLDEBUG,
				"Unable to convert byte count to integer: %s\n",
				bytes_str.c_str());
			return false;
		}
		m_size = bytes_long;
	} else {
		dprintf(D_FULLDEBUG, "Bytes line missing.\n");
		return false;
	}

		// Check the checksum value.
	if (!read_optional_line(optionalLine, fp, got_sync_line)) {
		return false;
	}
	prefix = "\tChecksum Value: ";
	if (starts_with(optionalLine.c_str(), prefix.c_str())) {
		m_checksum = optionalLine.substr(prefix.size());
	} else {
		dprintf(D_FULLDEBUG, "Checksum line missing.\n");
		return false;
	}

		// Check the checksum type.
	if (!read_optional_line(optionalLine, fp, got_sync_line)) {
		return false;
	}
	prefix = "\tChecksum Type: ";
	if (starts_with(optionalLine.c_str(), prefix.c_str())) {
		m_checksum_type = optionalLine.substr(prefix.size());
	} else {
		dprintf(D_FULLDEBUG, "Checksum type line missing.\n");
		return false;
	}

		// Check the file UUID.
	if (!read_optional_line(optionalLine, fp, got_sync_line)) {
		return false;
	}
	prefix = "\tUUID: ";
	if (starts_with(optionalLine.c_str(), prefix.c_str())) {
		m_uuid = optionalLine.substr(prefix.size());
	} else {
		dprintf(D_FULLDEBUG, "File UUID line missing.\n");
		return false;
	}

	return true;
}


void
FileUsedEvent::initFromClassAd( ClassAd *ad )
{
	ULogEvent::initFromClassAd( ad );

	std::string checksum;
	if (ad->EvaluateAttrString(ATTR_CHECKSUM, checksum)) {
		m_checksum = checksum;
	}
	std::string checksum_type;
	if (ad->EvaluateAttrString(ATTR_CHECKSUM_TYPE, checksum_type)) {
		m_checksum_type = checksum_type;
	}

	std::string tag;
	if (ad->EvaluateAttrString(ATTR_TAG, tag)) {
		m_tag = tag;
	}
}


ClassAd *
FileUsedEvent::toClassAd(bool event_time_utc) {
	std::unique_ptr<ClassAd> ad(ULogEvent::toClassAd(event_time_utc));
	if (!ad.get()) {return nullptr;}

	if (!ad->InsertAttr(ATTR_CHECKSUM, m_checksum)) {
		return nullptr;
	}

	if (!ad->InsertAttr(ATTR_CHECKSUM_TYPE, m_checksum_type)) {
		return nullptr;
	}

	if (!ad->InsertAttr(ATTR_TAG, m_tag)) {
		return nullptr;
	}

	return ad.release();
}


bool
FileUsedEvent::formatBody(std::string &out)
{
	if (formatstr_cat(out, "\n\tChecksum Value: %s\n", m_checksum.c_str()) < 0) {
		return false;
	}

	if (formatstr_cat(out, "\tChecksum Type: %s\n", m_checksum_type.c_str()) < 0) {
		return false;
	}

	if (formatstr_cat(out, "\tTag: %s\n", m_tag.c_str()) < 0) {
		return false;
	}

	return true;
}


int
FileUsedEvent::readEvent(ULogFile& fp, bool &got_sync_line) {
	std::string optionalLine;

		// Check the checksum value.
	if (!read_optional_line(optionalLine, fp, got_sync_line)) {
		return false;
	}
	chomp(optionalLine);
	std::string prefix = "Checksum Value: ";
	if (starts_with(optionalLine.c_str(), prefix.c_str())) {
		m_checksum = optionalLine.substr(prefix.size());
	} else {
		dprintf(D_FULLDEBUG, "Checksum line missing.\n");
		return false;
	}

		// Check the checksum type.
	if (!read_optional_line(optionalLine, fp, got_sync_line)) {
		return false;
	}
	prefix = "\tChecksum Type: ";
	if (starts_with(optionalLine.c_str(), prefix.c_str())) {
		m_checksum_type = optionalLine.substr(prefix.size());
	} else {
		dprintf(D_FULLDEBUG, "Checksum type line missing.\n");
		return false;
	}

		// Check the reservation tag.
	if (!read_optional_line(optionalLine, fp, got_sync_line)) {
		return false;
	}
	prefix = "\tTag: ";
	if (starts_with(optionalLine.c_str(), prefix.c_str())) {
		m_tag = optionalLine.substr(prefix.size());
	} else {
		dprintf(D_FULLDEBUG, "Reservation tag line missing.\n");
		return false;
	}


	return true;
}


void
FileRemovedEvent::initFromClassAd( ClassAd *ad )
{
	ULogEvent::initFromClassAd( ad );

	long long size;
	if (ad->EvaluateAttrInt(ATTR_SIZE, size)) {
		m_size = size;
	}

	std::string checksum;
	if (ad->EvaluateAttrString(ATTR_CHECKSUM, checksum)) {
		m_checksum = checksum;
	}
	std::string checksum_type;
	if (ad->EvaluateAttrString(ATTR_CHECKSUM_TYPE, checksum_type)) {
		m_checksum_type = checksum_type;
	}

	std::string tag;
	if (ad->EvaluateAttrString(ATTR_TAG, tag)) {
		m_tag = tag;
	}
}


ClassAd *
FileRemovedEvent::toClassAd(bool event_time_utc) {
	std::unique_ptr<ClassAd> ad(ULogEvent::toClassAd(event_time_utc));
	if (!ad.get()) {return nullptr;}

	if (!ad->InsertAttr(ATTR_SIZE, static_cast<long long>(m_size))) {
		return nullptr;
	}

	if (!ad->InsertAttr(ATTR_CHECKSUM, m_checksum)) {
		return nullptr;
	}

	if (!ad->InsertAttr(ATTR_CHECKSUM_TYPE, m_checksum_type)) {
		return nullptr;
	}

	if (!ad->InsertAttr(ATTR_TAG, m_tag)) {
		return nullptr;
	}

	return ad.release();
}


bool
FileRemovedEvent::formatBody(std::string &out)
{
	if (formatstr_cat(out, "\n\tBytes: %zu\n", m_size) < 0)
	{
		return false;
	}

	if (formatstr_cat(out, "\tChecksum Value: %s\n", m_checksum.c_str()) < 0) {
		return false;
	}

	if (formatstr_cat(out, "\tChecksum Type: %s\n", m_checksum_type.c_str()) < 0) {
		return false;
	}

	if (formatstr_cat(out, "\tTag: %s\n", m_tag.c_str()) < 0) {
		return false;
	}

	return true;
}


int
FileRemovedEvent::readEvent(ULogFile& fp, bool &got_sync_line) {
	std::string optionalLine;

		// Check for filesize in bytes.
	if (!read_optional_line(optionalLine, fp, got_sync_line)) {
		return false;
	}
	chomp(optionalLine);
	std::string prefix = "Bytes:";
	if (starts_with(optionalLine.c_str(), prefix.c_str())) {
		std::string bytes_str = optionalLine.substr(prefix.size());
		long long bytes_long;
		try {
			bytes_long = stoll(bytes_str);
		} catch (...) {
			dprintf(D_FULLDEBUG,
				"Unable to convert byte count to integer: %s\n",
				bytes_str.c_str());
			return false;
		}
		m_size = bytes_long;
	} else {
		dprintf(D_FULLDEBUG, "Bytes line missing.\n");
		return false;
	}

		// Check the checksum value.
	if (!read_optional_line(optionalLine, fp, got_sync_line)) {
		return false;
	}
	chomp(optionalLine);
	prefix = "\tChecksum Value: ";
	if (starts_with(optionalLine.c_str(), prefix.c_str())) {
		m_checksum = optionalLine.substr(prefix.size());
	} else {
		dprintf(D_FULLDEBUG, "Checksum line missing.\n");
		return false;
	}

		// Check the checksum type.
	if (!read_optional_line(optionalLine, fp, got_sync_line)) {
		return false;
	}
	prefix = "\tChecksum Type: ";
	if (starts_with(optionalLine.c_str(), prefix.c_str())) {
		m_checksum_type = optionalLine.substr(prefix.size());
	} else {
		dprintf(D_FULLDEBUG, "Checksum type line missing.\n");
		return false;
	}

		// Check the file tag.
	if (!read_optional_line(optionalLine, fp, got_sync_line)) {
		return false;
	}
	prefix = "\tTag: ";
	if (starts_with(optionalLine.c_str(), prefix.c_str())) {
		m_tag = optionalLine.substr(prefix.size());
	} else {
		dprintf(D_FULLDEBUG, "File tag line missing.\n");
		return false;
	}

	return true;
}

// ----- DataflowJobSkippedEvent class
DataflowJobSkippedEvent::DataflowJobSkippedEvent (void) : toeTag(NULL)
{
	eventNumber = ULOG_DATAFLOW_JOB_SKIPPED;
}

DataflowJobSkippedEvent::~DataflowJobSkippedEvent(void)
{
	if( toeTag ) {
		delete toeTag;
	}
}

void
DataflowJobSkippedEvent::setToeTag( classad::ClassAd * tt ) {
	if(! tt) { return; }
	if( toeTag ) { delete toeTag; }
	toeTag = new ToE::Tag();
	if(! ToE::decode( tt, * toeTag )) {
		delete toeTag;
		toeTag = NULL;
	}
}

bool
DataflowJobSkippedEvent::formatBody( std::string &out )
{

	if( formatstr_cat( out, "Dataflow job was skipped.\n" ) < 0 ) {
		return false;
	}
	if( !reason.empty() ) {
		if( formatstr_cat( out, "\t%s\n", reason.c_str() ) < 0 ) {
			return false;
		}
	}
	if( toeTag ) {
		if(! toeTag->writeToString( out )) {
			return false;
		}
	}
	return true;
}


int
DataflowJobSkippedEvent::readEvent (ULogFile& file, bool & got_sync_line)
{
	reason.clear();

	std::string line;
	if ( ! read_line_value("Dataflow job was skipped.", line, file, got_sync_line)) {
		return 0;
	}
	// try to read the reason, this is optional
	if (read_optional_line(line, file, got_sync_line, true)) {
		trim(line);
		reason = line;
	}

	// Try to read the ToE tag.
	if( got_sync_line ) { return 1; }
	if( read_optional_line( line, file, got_sync_line ) ) {
		if( line.empty() ) {
			if(! read_optional_line( line, file, got_sync_line )) {
				return 0;
			}
		}

		if( replace_str(line, "\tJob terminated by ", "") ) {
			if( toeTag != NULL ) { delete toeTag; }
			toeTag = new ToE::Tag();
			if(! toeTag->readFromString( line )) {
				return 0;
			}
		} else {
			return 0;
		}
	}

	return 1;
}

ClassAd*
DataflowJobSkippedEvent::toClassAd(bool event_time_utc)
{
	ClassAd* myad = ULogEvent::toClassAd(event_time_utc);
	if( !myad ) return NULL;

	if( !reason.empty() ) {
		if( !myad->InsertAttr("Reason", reason) ) {
			delete myad;
			return NULL;
		}
	}

	if( toeTag ) {
		classad::ClassAd * tt = new classad::ClassAd();
		if(! ToE::encode( * toeTag, tt )) {
			delete tt;
			delete myad;
			return NULL;
		}
		if(! myad->Insert(ATTR_JOB_TOE, tt )) {
			delete tt;
			delete myad;
			return NULL;
		}
	}

	return myad;
}

void
DataflowJobSkippedEvent::initFromClassAd(ClassAd* ad)
{
	ULogEvent::initFromClassAd(ad);

	if( !ad ) return;

	ad->LookupString("Reason", reason);

	setToeTag( dynamic_cast<classad::ClassAd *>(ad->Lookup(ATTR_JOB_TOE)) );
}
