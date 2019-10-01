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
#include "condor_netdb.h"

#include "misc_utils.h"
#include "utc_time.h"
#include "ToE.h"

//added by Ameet
#include "condor_environ.h"
//--------------------------------------------------------
#include "condor_debug.h"
//--------------------------------------------------------

// define this to turn off seeking in the event reader methods
#define DONT_EVER_SEEK 1

#define ESCAPE { errorNumber=(errno==EAGAIN) ? ULOG_NO_EVENT : ULOG_UNK_ERROR;\
					 return 0; }

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

	  case ULOG_GLOBUS_SUBMIT:
		return new GlobusSubmitEvent;

	  case ULOG_GLOBUS_SUBMIT_FAILED:
		return new GlobusSubmitFailedEvent;

	  case ULOG_GLOBUS_RESOURCE_DOWN:
		return new GlobusResourceDownEvent;

	  case ULOG_GLOBUS_RESOURCE_UP:
		return new GlobusResourceUpEvent;

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

	default:
		dprintf( D_ALWAYS, "Unknown ULogEventNumber: %d, reading it as a FutureEvent\n", event );
		return new FutureEvent(event);
	}

    return 0;
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


int ULogEvent::getEvent (FILE *file, bool & got_sync_line)
{
	if( !file ) {
		dprintf( D_ALWAYS, "ERROR: file == NULL in ULogEvent::getEvent()\n" );
		return 0;
	}
	return (readHeader (file) && readEvent (file, got_sync_line));
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
	if( number == (ULogEventNumber)-1 ) {
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
bool ULogEvent::read_optional_line(FILE* file, bool & got_sync_line, char * buf, size_t bufsize, bool chomp /*=true*/, bool trim /*=false*/)
{
	buf[0] = 0;
	if( !fgets( buf, (int)bufsize, file )) {
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

// read a line into a MyString 
bool ULogEvent::read_optional_line(MyString & str, FILE* file, bool & got_sync_line, bool chomp /*=true*/)
{
	if ( ! str.readLine(file, false)) {
		return false;
	}
	if (is_sync_line(str.Value())) {
		got_sync_line = true;
		return false;
	}
	if (chomp) { str.chomp(); }
	return true;
}

bool ULogEvent::read_line_value(const char * prefix, MyString & val, FILE* file, bool & got_sync_line, bool chomp /*=true*/)
{
	val.clear();
	MyString str;
	if ( ! str.readLine(file, false)) {
		return false;
	}
	if (is_sync_line(str.Value())) {
		got_sync_line = true;
		return false;
	}
	if (chomp) { str.chomp(); }
	if (starts_with(str.c_str(), prefix)) {
		val = str.substr((int)strlen(prefix), str.Length());
		return true;
	}
	return false;
}



// returns a new'ed pointer to a buffer containing the next line if there is a next line
// and it is not a sync line. got_sync_line will be set to true if it was a sync line
// if chomp is true, trailing \r and \n will not be returned
// if trim is true, leading whitespace will not be returned.
char * ULogEvent::read_optional_line(FILE* file, bool & got_sync_line, bool chomp /*=true*/, bool trim /*=false*/)
{
	MyString str;
	if (read_optional_line(str, file, got_sync_line, chomp)) {
		if (trim) { str.trim(); }
		return str.detach_buffer();
	}
	return NULL;
}


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
	  case ULOG_GLOBUS_SUBMIT:
		SetMyTypeName(*myad, "GlobusSubmitEvent");
		break;
	  case ULOG_GLOBUS_SUBMIT_FAILED:
		SetMyTypeName(*myad, "GlobusSubmitFailedEvent");
		break;
	  case ULOG_GLOBUS_RESOURCE_UP:
		SetMyTypeName(*myad, "GlobusResourceUpEvent");
		break;
	  case ULOG_GLOBUS_RESOURCE_DOWN:
		SetMyTypeName(*myad, "GlobusResourceDownEvent");
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
	char* timestr = NULL;
	if( ad->LookupString("EventTime", &timestr) ) {
		bool is_utc = false;
		struct tm eventTime;
		iso8601_to_time(timestr, &eventTime, &event_usec, &is_utc);
		if (is_utc) {
			eventclock = timegm(&eventTime);
		} else {
			eventclock = mktime(&eventTime);
		}
		free(timestr);
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
	MyString fString;
	fString.formatstr( "\tPartitionable Resources : %%%ds %%%ds %%%ds %%s\n",
		cchUse, cchReq, MAX(cchAlloc, 9) );
	formatstr_cat( out, fString.Value(), "Usage", "Request",
		 cchAlloc ? "Allocated" : "", cchAssigned ? "Assigned" : "" );

	// Print table.
	fString.formatstr( "\t   %%-%ds : %%%ds %%%ds %%%ds %%s\n",
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
		const SlotResTermSumy & psumy = i.second;
		formatstr_cat( out, fString.Value(), label.c_str(), psumy.use.c_str(),
			psumy.req.c_str(), psumy.alloc.c_str(), psumy.assigned.c_str() );
	}
}

#ifdef DONT_EVER_SEEK
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

	void Parse(const char * sz, ClassAd * puAd) {
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

		//pszTbl[ixUse] = 0;
		// Insert <Tag>Usage = <value1>
		std::string exprstr(tag); exprstr += "Usage = "; exprstr.append(pszTbl, ixUse);
		puAd->Insert(exprstr);

		//pszTbl[ixReq] = 0;
		// Insert Request<Tag> = <value2>
		exprstr = "Request"; exprstr += tag; exprstr += " = "; exprstr.append(&pszTbl[ixUse+1], ixReq-ixUse-1);
		puAd->Insert(exprstr);

		if (ixAlloc > 0) {
			//pszTbl[ixAlloc] = 0;
			// Insert <tag> = <value3>
			exprstr = tag; exprstr += " = "; exprstr.append(&pszTbl[ixReq+1], ixAlloc-ixReq-1);
			puAd->Insert(exprstr);
		}
		if (ixAssigned > 0) {
			// the remainder of the line is the assigned value
			exprstr = "Assigned"; exprstr += tag; exprstr += " = "; exprstr += &pszTbl[ixAssigned];
			puAd->Insert(exprstr);
		}
	}


protected:
	int ixColon;
	int ixUse;
	int ixReq;
	int ixAlloc;
	int ixAssigned;
};
#else
static void readUsageAd(FILE * file, /* in,out */ ClassAd ** ppusageAd)
{
	ClassAd * puAd = *ppusageAd;
	if ( ! puAd) {
		puAd = new ClassAd();
		if ( ! puAd)
			return;
	}
	puAd->Clear();

	int ixColon = -1;
	int ixUse = -1;
	int ixReq = -1;
	int ixAlloc = -1;
	int ixAssigned = -1;

	for (;;) {
		char sz[250];

		// if we hit end of file or end of record "..." rewind the file pointer.
		fpos_t filep;
		fgetpos( file, &filep );
		if ( ! fgets(sz, sizeof(sz), file) || 
			(sz[0] == '.' && sz[1] == '.' && sz[2] == '.')) {
			fsetpos( file, &filep );
			break;
		}

		// lines for reading the usageAd must be of the form "\tlabel : data\n"
		// the ':' characters in each line will line up vertically, so we can find
		// the colon in the first line, and use it as a quick to validate subsequent
		// lines.
		if (ixColon < 0) {
			const char * pszColon = strchr(sz, ':');
			if ( ! pszColon) 
				ixColon = 0;
			else
				ixColon = (int)(pszColon - sz);
		}
		int cchLine = strlen(sz);
		if (sz[0] != '\t' || ixColon <= 0 || ixColon+1 >= cchLine || 
			sz[ixColon] != ':' || sz[ixColon-1] != ' ' || sz[ixColon+1] != ' ') {
			fsetpos( file, &filep );
			break;
		}

		sz[ixColon] = 0; // separate sz into 2 strings
		// parse out label
		char * pszLbl = sz;
		while (*pszLbl == '\t' || *pszLbl == ' ') ++pszLbl;
		char * psz = pszLbl;
		while (*psz && *psz != ' ') ++psz;
		*psz = 0;

		char * pszTbl = &sz[ixColon+1]; // pointer to the usage table

		// 
		if (MATCH == strcmp(pszLbl, "Partitionable")) {
			psz = pszTbl;
			while (*psz == ' ') ++psz;         // skip spaces
			while (*psz && *psz != ' ') ++psz; // skip "Usage"
			ixUse = (int)(psz - pszTbl)+1;     // save right edge of Usage
			while (*psz == ' ') ++psz;         // skip spaces
			while (*psz && *psz != ' ') ++psz; // skip "Request"
			ixReq = (int)(psz - pszTbl)+1;     // save right edge of Request
			while (*psz == ' ') ++psz;         // skip spaces
			if (*psz) {                        // if there is an "Allocated"
				char *p = strstr(psz, "Allocated");
				if (p) {
					ixAlloc = (int)(p - pszTbl)+9; // save right edge of Allocated
					p = strstr(p, "Assigned");
					if (p) { // if there is an "Assigned"
						ixAssigned = (int)(p - pszTbl); // save *left* edge of assigned (it gets the remaineder of the line)
					}
				}
			}
		} else if (ixUse > 0) {
			pszTbl[ixUse] = 0;
			pszTbl[ixReq] = 0;
			std::string exprstr;
			formatstr(exprstr, "%sUsage = %s", pszLbl, pszTbl);
			puAd->Insert(exprstr.c_str());
			formatstr(exprstr, "Request%s = %s", pszLbl, &pszTbl[ixUse+1]);
			puAd->Insert(exprstr.c_str());
			if (ixAlloc > 0) {
				pszTbl[ixAlloc] = 0;
				formatstr(exprstr, "%s = %s", pszLbl, &pszTbl[ixReq+1]);
				puAd->Insert(exprstr.c_str());
			}
			if (ixAssigned > 0) {
				// the remainder of the line is the assigned value
				formatstr(exprstr, "Assigned%s = %s", pszLbl, &pszTbl[ixAssigned]);
				//trim(exprstr);
				puAd->Insert(exprstr.c_str());
			}
		}
	}

	*ppusageAd = puAd;
}
#endif

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
FutureEvent::readEvent (FILE * file, bool & got_sync_line)
{
	// read lines until we see "...\n" or "...\r\n"
	// then rewind to the beginning of the end sentinal and return

	fpos_t filep;
	fgetpos( file, &filep );

	bool athead = true;
	MyString line;
	while (line.readLine(file)) {
		if (line[0] == '.' && (line == "...\n" || line == "...\r\n")) {
#ifdef DONT_EVER_SEEK
			got_sync_line = true;
#else
			fsetpos( file, &filep );
#endif
			break;
		}
		else if (athead) {
			line.chomp();
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
		StringTokenIterator lines(payload, 120, "\r\n");
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
	sGetAdAttrs(attrs, *ad, false, NULL);
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
	MyString line(head_text);
	line.chomp();
	head = line;
}

void FutureEvent::setPayload(const char * payload_text)
{
	payload = payload_text;
}



// ----- the SubmitEvent class
SubmitEvent::SubmitEvent(void)
{
	submitHost = NULL;
	submitEventLogNotes = NULL;
	submitEventUserNotes = NULL;
	submitEventWarnings = NULL;
	eventNumber = ULOG_SUBMIT;
}

SubmitEvent::~SubmitEvent(void)
{
	if( submitHost ) {
		delete[] submitHost;
	}
    if( submitEventLogNotes ) {
        delete[] submitEventLogNotes;
    }
    if( submitEventUserNotes ) {
        delete[] submitEventUserNotes;
    }
    if( submitEventWarnings ) {
        delete[] submitEventWarnings;
    }
}

void
SubmitEvent::setSubmitHost(char const *addr)
{
	if( submitHost ) {
		delete[] submitHost;
	}
	if( addr ) {
		submitHost = strnewp(addr);
		ASSERT( submitHost );
	}
	else {
		submitHost = NULL;
	}
}

bool
SubmitEvent::formatBody( std::string &out )
{
	if( !submitHost ) {
		setSubmitHost("");
	}
	int retval = formatstr_cat (out, "Job submitted from host: %s\n", submitHost);
	if (retval < 0)
	{
		return false;
	}
	if( submitEventLogNotes ) {
		retval = formatstr_cat( out, "    %.8191s\n", submitEventLogNotes );
		if( retval < 0 ) {
			return false;
		}
	}
	if( submitEventUserNotes ) {
		retval = formatstr_cat( out, "    %.8191s\n", submitEventUserNotes );
		if( retval < 0 ) {
			return false;
		}
	}
	if( submitEventWarnings ) {
		retval = formatstr_cat( out, "    WARNING: Committed job submission into the queue with the following warning(s): %.8110s\n", submitEventWarnings );
		if( retval < 0 ) {
			return false;
		}
	}
	return true;
}


int
SubmitEvent::readEvent (FILE *file, bool & got_sync_line)
{
	delete[] submitEventLogNotes;
	submitEventLogNotes = NULL;
#ifdef DONT_EVER_SEEK
	MyString host;
	if ( ! read_line_value("Job submitted from host: ", host, file, got_sync_line)) {
		return 0;
	}
	submitHost = host.detach_buffer();
#else
	char s[8192];
	s[0] = '\0';
	MyString line;
	if( !line.readLine(file) ) {
		return 0;
	}
	setSubmitHost(line.Value()); // allocate memory
	if( sscanf( line.Value(), "Job submitted from host: %s\n", submitHost ) != 1 ) {
		return 0;
	}
#endif

	// check if event ended without specifying submit host.
	// in this case, the submit host would be the event delimiter
	if ( strncmp(submitHost,"...",3)==0 ) {
		submitHost[0] = '\0';
#ifdef DONT_EVER_SEEK
		got_sync_line = true;
#else
		// Backup to leave event delimiter unread go past \n too
		fseek( file, -4, SEEK_CUR );
#endif
		return 1;
	}

	// see if the next line contains an optional event notes string
#ifdef DONT_EVER_SEEK
	submitEventLogNotes = read_optional_line(file, got_sync_line, true, true);
	if( ! submitEventLogNotes) {
		return 1;
	}
#else
	// and, if not, rewind, because that means we slurped in the next
	// event delimiter looking for it...
	fpos_t filep;
	fgetpos( file, &filep );
	if( !fgets( s, 8192, file ) || strcmp( s, "...\n" ) == 0 ) {
		fsetpos( file, &filep );
		return 1;
	}
	// remove trailing newline
	s[ strlen( s ) - 1 ] = '\0';

	// some users of this library (dagman) depend on whitespace
	// being stripped from the beginning of the log notes field
	char const *strip_s = s;
	while( *strip_s && isspace(*strip_s) ) {
		strip_s++;
	}
	submitEventLogNotes = strnewp( strip_s );
#endif

	// see if the next line contains an optional user event notes string
#ifdef DONT_EVER_SEEK
	submitEventUserNotes = read_optional_line(file, got_sync_line, true, true);
	if( ! submitEventUserNotes) {
		return 1;
	}
#else
	// and, if not, rewind, because that means we slurped in
	// the next event delimiter looking for it...
	fgetpos( file, &filep );
	if( !fgets( s, 8192, file ) || strcmp( s, "...\n" ) == 0 ) {
		fsetpos( file, &filep );
		return 1;
	}
	// remove trailing newline
	s[ strlen( s ) - 1 ] = '\0';
	submitEventUserNotes = strnewp( s );
#endif


#ifdef DONT_EVER_SEEK
	submitEventWarnings = read_optional_line(file, got_sync_line, true, false);
	if( ! submitEventWarnings) {
		return 1;
	}
#else
	fgetpos( file, &filep );
	if( !fgets( s, 8192, file ) || strcmp( s, "...\n" ) == 0 ) {
		fsetpos( file, &filep );
		return 1;
	}
	// remove trailing newline
	s[ strlen( s ) - 1 ] = '\0';
	submitEventWarnings = strnewp( s );
#endif

	return 1;
}

ClassAd*
SubmitEvent::toClassAd(bool event_time_utc)
{
	ClassAd* myad = ULogEvent::toClassAd(event_time_utc);
	if( !myad ) return NULL;

	if( submitHost && submitHost[0] ) {
		if( !myad->InsertAttr("SubmitHost",submitHost) ) return NULL;
	}

	if( submitEventLogNotes && submitEventLogNotes[0] ) {
		if( !myad->InsertAttr("LogNotes",submitEventLogNotes) ) return NULL;
	}
	if( submitEventUserNotes && submitEventUserNotes[0] ) {
		if( !myad->InsertAttr("UserNotes",submitEventUserNotes) ) return NULL;
	}
	if( submitEventWarnings && submitEventWarnings[0] ) {
		if( !myad->InsertAttr("Warnings",submitEventWarnings) ) return NULL;
	}

	return myad;
}

void
SubmitEvent::initFromClassAd(ClassAd* ad)
{
	ULogEvent::initFromClassAd(ad);

	if( !ad ) return;
	char* mallocstr = NULL;
	ad->LookupString("SubmitHost", &mallocstr);
	if( mallocstr ) {
		setSubmitHost(mallocstr);
		free(mallocstr);
		mallocstr = NULL;
	}

	// this fanagling is to ensure we don't malloc a pointer then delete it
	ad->LookupString("LogNotes", &mallocstr);
	if( mallocstr ) {
		submitEventLogNotes = new char[strlen(mallocstr) + 1];
		strcpy(submitEventLogNotes, mallocstr);
		free(mallocstr);
		mallocstr = NULL;
	}

	// this fanagling is to ensure we don't malloc a pointer then delete it
	ad->LookupString("UserNotes", &mallocstr);
	if( mallocstr ) {
		submitEventUserNotes = new char[strlen(mallocstr) + 1];
		strcpy(submitEventUserNotes, mallocstr);
		free(mallocstr);
		mallocstr = NULL;
	}

	ad->LookupString("Warnings", &mallocstr);
	if( mallocstr ) {
		submitEventWarnings = new char[strlen(mallocstr) + 1];
		strcpy(submitEventWarnings, mallocstr);
		free(mallocstr);
		mallocstr = NULL;
	}
}

// ----- the GlobusSubmitEvent class
GlobusSubmitEvent::GlobusSubmitEvent(void)
{
	eventNumber = ULOG_GLOBUS_SUBMIT;
	rmContact = NULL;
	jmContact = NULL;
	restartableJM = false;
}

GlobusSubmitEvent::~GlobusSubmitEvent(void)
{
	delete[] rmContact;
	delete[] jmContact;
}

bool
GlobusSubmitEvent::formatBody( std::string &out )
{
	const char * unknown = "UNKNOWN";
	const char * rm = unknown;
	const char * jm = unknown;

	int retval = formatstr_cat( out, "Job submitted to Globus\n" );
	if (retval < 0)
	{
		return false;
	}

	if ( rmContact ) rm = rmContact;
	if ( jmContact ) jm = jmContact;

	retval = formatstr_cat( out, "    RM-Contact: %.8191s\n", rm );
	if( retval < 0 ) {
		return false;
	}

	retval = formatstr_cat( out, "    JM-Contact: %.8191s\n", jm );
	if( retval < 0 ) {
		return false;
	}

	int newjm = 0;
	if ( restartableJM ) {
		newjm = 1;
	}
	retval = formatstr_cat( out, "    Can-Restart-JM: %d\n", newjm );
	if( retval < 0 ) {
		return false;
	}

	return true;
}

int GlobusSubmitEvent::readEvent (FILE *file, bool & got_sync_line)
{

	delete[] rmContact;
	delete[] jmContact;
	rmContact = NULL;
	jmContact = NULL;
	int newjm = 0;

#ifdef DONT_EVER_SEEK
	MyString tmp;
	if ( ! read_line_value("Job submitted to Globus", tmp, file, got_sync_line)) {
		return 0;
	}

	if ( ! read_line_value("    RM-Contact: ", tmp, file, got_sync_line)) {
		return 0;
	}
	rmContact = tmp.detach_buffer();

	if ( ! read_line_value("    JM-Contact: ", tmp, file, got_sync_line)) {
		return 0;
	}
	jmContact = tmp.detach_buffer();

	if ( ! read_line_value("    Can-Restart-JM: ", tmp, file, got_sync_line)) {
		return 0;
	}
	if ( ! YourStringDeserializer(tmp.Value()).deserialize_int(&newjm)) {
		return 0;
	}
#else
	char s[8192];
	int retval = fscanf (file, "Job submitted to Globus\n");
    if (retval != 0)
    {
		return 0;
    }
	s[0] = '\0';
	retval = fscanf( file, "    RM-Contact: %8191s\n", s );
	if ( retval != 1 )
	{
		return 0;
	}
	rmContact = strnewp(s);
	retval = fscanf( file, "    JM-Contact: %8191s\n", s );
	if ( retval != 1 )
	{
		return 0;
	}
	jmContact = strnewp(s);

	retval = fscanf( file, "    Can-Restart-JM: %d\n", &newjm );
	if ( retval != 1 )
	{
		return 0;
	}
#endif
	if ( newjm ) {
		restartableJM = true;
	} else {
		restartableJM = false;
	}

	return 1;
}

ClassAd*
GlobusSubmitEvent::toClassAd(bool event_time_utc)
{
	ClassAd* myad = ULogEvent::toClassAd(event_time_utc);
	if( !myad ) return NULL;

	if( rmContact && rmContact[0] ) {
		if( !myad->InsertAttr("RMContact", rmContact) ) {
			delete myad;
			return NULL;
		}
	}
	if( jmContact && jmContact[0] ) {
		if( !myad->InsertAttr("JMContact", jmContact) ) {
			delete myad;
			return NULL;
		}
	}

	if( !myad->InsertAttr("RestartableJM", restartableJM ? true : false) ){
		delete myad;
		return NULL;
	}

	return myad;
}

void
GlobusSubmitEvent::initFromClassAd(ClassAd* ad)
{
	ULogEvent::initFromClassAd(ad);

	if( !ad ) return;

	// this fanagling is to ensure we don't malloc a pointer then delete it
	char* mallocstr = NULL;
	ad->LookupString("RMContact", &mallocstr);
	if( mallocstr ) {
		rmContact = new char[strlen(mallocstr) + 1];
		strcpy(rmContact, mallocstr);
		free(mallocstr);
	}

	// this fanagling is to ensure we don't malloc a pointer then delete it
	mallocstr = NULL;
	ad->LookupString("JMContact", &mallocstr);
	if( mallocstr ) {
		jmContact = new char[strlen(mallocstr) + 1];
		strcpy(jmContact, mallocstr);
		free(mallocstr);
	}

	int reallybool;
	if( ad->LookupInteger("RestartableJM", reallybool) ) {
		restartableJM = reallybool ? TRUE : FALSE;
	}
}

// ----- the GlobusSubmitFailedEvent class
GlobusSubmitFailedEvent::GlobusSubmitFailedEvent(void)
{
	eventNumber = ULOG_GLOBUS_SUBMIT_FAILED;
	reason = NULL;
}

GlobusSubmitFailedEvent::~GlobusSubmitFailedEvent(void)
{
	delete[] reason;
}

bool
GlobusSubmitFailedEvent::formatBody( std::string &out )
{
	const char * unknown = "UNKNOWN";
	const char * reasonString = unknown;

	int retval = formatstr_cat( out, "Globus job submission failed!\n" );
	if (retval < 0)
	{
		return false;
	}

	if ( reason ) reasonString = reason;

	retval = formatstr_cat( out, "    Reason: %.8191s\n", reasonString );
	if( retval < 0 ) {
		return false;
	}

	return true;
}

int
GlobusSubmitFailedEvent::readEvent (FILE *file, bool & got_sync_line)
{
	delete[] reason;
	reason = NULL;

#ifdef DONT_EVER_SEEK
	MyString tmp;
	if ( ! read_line_value("Globus job submission failed!", tmp, file, got_sync_line)) {
		return 0;
	}
	if ( ! read_line_value("    Reason: ", tmp, file, got_sync_line)) {
		return 0;
	}
	reason = tmp.detach_buffer();
#else
	char s[8192];

	int retval = fscanf (file, "Globus job submission failed!\n");
    if (retval != 0)
    {
		return 0;
    }
	s[0] = '\0';

	fpos_t filep;
	fgetpos( file, &filep );

	if( !fgets( s, 8192, file ) || strcmp( s, "...\n" ) == 0 ) {
		fsetpos( file, &filep );
		return 1;
	}

	// remove trailing newline
	s[ strlen( s ) - 1 ] = '\0';
	// Copy after the "Reason: "
	reason = strnewp( &s[8] );
#endif
	return 1;
}

ClassAd*
GlobusSubmitFailedEvent::toClassAd(bool event_time_utc)
{
	ClassAd* myad = ULogEvent::toClassAd(event_time_utc);
	if( !myad ) return NULL;

	if( reason && reason[0] ) {
		if( !myad->InsertAttr("Reason", reason) ) {
			delete myad;
			return NULL;
		}
	}

	return myad;
}

void
GlobusSubmitFailedEvent::initFromClassAd(ClassAd* ad)
{
	ULogEvent::initFromClassAd(ad);

	if( !ad ) return;

	// this fanagling is to ensure we don't malloc a pointer then delete it
	char* mallocstr = NULL;
	ad->LookupString("Reason", &mallocstr);
	if( mallocstr ) {
		reason = new char[strlen(mallocstr) + 1];
		strcpy(reason, mallocstr);
		free(mallocstr);
	}
}

// ----- the GlobusResourceUp class
GlobusResourceUpEvent::GlobusResourceUpEvent(void)
{
	eventNumber = ULOG_GLOBUS_RESOURCE_UP;
	rmContact = NULL;
}

GlobusResourceUpEvent::~GlobusResourceUpEvent(void)
{
	delete[] rmContact;
}

bool
GlobusResourceUpEvent::formatBody( std::string &out )
{
	const char * unknown = "UNKNOWN";
	const char * rm = unknown;

	int retval = formatstr_cat( out, "Globus Resource Back Up\n" );
	if (retval < 0)
	{
		return false;
	}

	if ( rmContact ) rm = rmContact;

	retval = formatstr_cat( out, "    RM-Contact: %.8191s\n", rm );
	if( retval < 0 ) {
		return false;
	}

	return true;
}

int
GlobusResourceUpEvent::readEvent (FILE *file, bool & got_sync_line)
{
	delete[] rmContact;
	rmContact = NULL;
#ifdef DONT_EVER_SEEK
	MyString tmp;
	if ( ! read_line_value("Globus Resource Back Up", tmp, file, got_sync_line)) {
		return 0;
	}
	if ( ! read_line_value("    RM-Contact: ", tmp, file, got_sync_line)) {
		return 0;
	}
	rmContact = tmp.detach_buffer();
#else
	char s[8192];

	int retval = fscanf (file, "Globus Resource Back Up\n");
    if (retval != 0)
    {
		return 0;
    }
	s[0] = '\0';
	retval = fscanf( file, "    RM-Contact: %8191s\n", s );
	if ( retval != 1 )
	{
		return 0;
	}
	rmContact = strnewp(s);
#endif
	return 1;
}

ClassAd*
GlobusResourceUpEvent::toClassAd(bool event_time_utc)
{
	ClassAd* myad = ULogEvent::toClassAd(event_time_utc);
	if( !myad ) return NULL;

	if( rmContact && rmContact[0] ) {
		if( !myad->InsertAttr("RMContact", rmContact) ) {
			delete myad;
			return NULL;
		}
	}

	return myad;
}

void
GlobusResourceUpEvent::initFromClassAd(ClassAd* ad)
{
	ULogEvent::initFromClassAd(ad);

	if( !ad ) return;

	// this fanagling is to ensure we don't malloc a pointer then delete it
	char* mallocstr = NULL;
	ad->LookupString("RMContact", &mallocstr);
	if( mallocstr ) {
		rmContact = new char[strlen(mallocstr) + 1];
		strcpy(rmContact, mallocstr);
		free(mallocstr);
	}
}


// ----- the GlobusResourceUp class
GlobusResourceDownEvent::GlobusResourceDownEvent(void)
{
	eventNumber = ULOG_GLOBUS_RESOURCE_DOWN;
	rmContact = NULL;
}

GlobusResourceDownEvent::~GlobusResourceDownEvent(void)
{
	delete[] rmContact;
}

bool
GlobusResourceDownEvent::formatBody( std::string &out )
{
	const char * unknown = "UNKNOWN";
	const char * rm = unknown;

	int retval = formatstr_cat( out, "Detected Down Globus Resource\n" );
	if (retval < 0)
	{
		return false;
	}

	if ( rmContact ) rm = rmContact;

	retval = formatstr_cat( out, "    RM-Contact: %.8191s\n", rm );
	if( retval < 0 ) {
		return false;
	}

	return true;
}

int
GlobusResourceDownEvent::readEvent (FILE *file, bool & got_sync_line)
{
	delete[] rmContact;
	rmContact = NULL;
#ifdef DONT_EVER_SEEK
	MyString tmp;
	if ( ! read_line_value("Detected Down Globus Resource", tmp, file, got_sync_line)) {
		return 0;
	}
	if ( ! read_line_value("    RM-Contact: ", tmp, file, got_sync_line)) {
		return 0;
	}
	rmContact = tmp.detach_buffer();
#else
	char s[8192];

	int retval = fscanf (file, "Detected Down Globus Resource\n");
    if (retval != 0)
    {
		return 0;
    }
	s[0] = '\0';
	retval = fscanf( file, "    RM-Contact: %8191s\n", s );
	if ( retval != 1 )
	{
		return 0;
	}
	rmContact = strnewp(s);
#endif
	return 1;
}

ClassAd*
GlobusResourceDownEvent::toClassAd(bool event_time_utc)
{
	ClassAd* myad = ULogEvent::toClassAd(event_time_utc);
	if( !myad ) return NULL;

	if( rmContact && rmContact[0] ) {
		if( !myad->InsertAttr("RMContact", rmContact) ) {
			delete myad;
			return NULL;
		}
	}

	return myad;
}

void
GlobusResourceDownEvent::initFromClassAd(ClassAd* ad)
{
	ULogEvent::initFromClassAd(ad);

	if( !ad ) return;

	// this fanagling is to ensure we don't malloc a pointer then delete it
	char* mallocstr = NULL;
	ad->LookupString("RMContact", &mallocstr);
	if( mallocstr ) {
		rmContact = new char[strlen(mallocstr) + 1];
		strcpy(rmContact, mallocstr);
		free(mallocstr);
	}
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
GenericEvent::readEvent(FILE *file, bool & got_sync_line)
{
#ifdef DONT_EVER_SEEK
	MyString str;
	if ( ! read_optional_line(str, file, got_sync_line) || str.Length() >= (int)sizeof(info)) {
		return 0;
	}
	strncpy(info, str.c_str(), sizeof(info)-1);
	info[sizeof(info)-1] = 0;
#else
    int retval = fscanf(file, "%[^\n]\n", info);
    if (retval < 0)
    {
	return 0;
    }
#endif
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
	strncpy(info,str,sizeof(info));
	info[ sizeof(info) - 1 ] = '\0'; //ensure null-termination
}

// ----- the RemoteErrorEvent class
RemoteErrorEvent::RemoteErrorEvent(void)
{
	error_str = NULL;
	execute_host[0] = daemon_name[0] = '\0';
	eventNumber = ULOG_REMOTE_ERROR;
	critical_error = true;
	hold_reason_code = 0;
	hold_reason_subcode = 0;
}

RemoteErrorEvent::~RemoteErrorEvent(void)
{
	delete[] error_str;
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

	if(!critical_error) error_type = "Warning";

	retval = formatstr_cat(
	  out,
	  "%s from %s on %s:\n",
	  error_type,
	  daemon_name,
	  execute_host);



    if (retval < 0)
    {
        return false;
    }

	//output each line of error_str, indented by one tab
	char *line = error_str;
	if(line)
	while(*line) {
		char *next_line = strchr(line,'\n');
		if(next_line) *next_line = '\0';

		retval = formatstr_cat( out, "\t%s\n", line );
		if(retval < 0) return 0;

		if(!next_line) break;
		*next_line = '\n';
		line = next_line+1;
	}

	if (hold_reason_code) {
		formatstr_cat( out, "\tCode %d Subcode %d\n",
					   hold_reason_code, hold_reason_subcode );
	}

	return true;
}

int
RemoteErrorEvent::readEvent(FILE *file, bool & got_sync_line)
{
	char error_type[128];
#ifdef DONT_EVER_SEEK
	MyString line;
	if ( ! read_optional_line(line, file, got_sync_line)) {
		return 0;
	}

	// parse error_type, daemon_name and execute_host from a line of the form
	// [Error|Warning] from <daemon_name> on <execute_host>:
	// " from ", " on ", and the trailing ":" are added by format body and should be removed here.

	int retval = 0;
	line.trim();
	int ix = line.find(" from ");
	if (ix > 0) {
		MyString et = line.substr(0, ix);
		et.trim();
		strncpy(error_type, et.Value(), sizeof(error_type));
		line = line.substr(ix + 6, line.length());
		line.trim();
	} else {
		strncpy(error_type, "Error", sizeof(error_type));
		retval = -1;
	}

	ix = line.find(" on ");
	if (ix <= 0) {
		daemon_name[0] = 0;
	} else {
		MyString dn = line.substr(0, ix);
		dn.trim();
		strncpy(daemon_name, dn.Value(), sizeof(daemon_name));
		line = line.substr(ix + 4, line.length());
		line.trim();
	}

	// we expect to see a : after the daemon name, if we find it. remove it.
	ix = line.length();
	if (ix > 0 && line[ix-1] == ':') { line.truncate(ix - 1); }

	strncpy(execute_host, line.Value(), sizeof(execute_host));
#else
    int retval = fscanf(
	  file,
	  "%127s from %127s on %127s\n",
	  error_type,
	  daemon_name,
	  execute_host);
#endif

	if (retval < 0) {
		return 0;
	}

	error_type[sizeof(error_type)-1] = '\0';
	daemon_name[sizeof(daemon_name)-1] = '\0';
	execute_host[sizeof(execute_host)-1] = '\0';

	if(!strcmp(error_type,"Error")) critical_error = true;
	else if(!strcmp(error_type,"Warning")) critical_error = false;

	MyString lines;

	while(!feof(file)) {
		// see if the next line contains an optional event notes string,
		// and, if not, rewind, because that means we slurped in the next
		// event delimiter looking for it...

#ifdef DONT_EVER_SEEK
		if ( ! read_optional_line(line, file, got_sync_line) || got_sync_line) {
			break;
		}
		line.chomp();
		const char *l = line.Value();
#else
		char line[8192];
		fpos_t filep;
		fgetpos( file, &filep );

		if( !fgets(line, sizeof(line), file) || strcmp(line, "...\n") == 0 ) {
			fsetpos( file, &filep );
			break;
		}
		char *l = strchr(line,'\n');
		if(l) *l = '\0';

		l = line;
#endif
		if(l[0] == '\t') l++;

		int code,subcode;
		if( sscanf(l,"Code %d Subcode %d",&code,&subcode) == 2 ) {
			hold_reason_code = code;
			hold_reason_subcode = subcode;
			continue;
		}

		if(lines.Length()) lines += "\n";
		lines += l;
	}

	setErrorText(lines.Value());
	return 1;
}

ClassAd*
RemoteErrorEvent::toClassAd(bool event_time_utc)
{
	ClassAd* myad = ULogEvent::toClassAd(event_time_utc);
	if( !myad ) return NULL;

	if(*daemon_name) {
		myad->Assign("Daemon",daemon_name);
	}
	if(*execute_host) {
		myad->Assign("ExecuteHost",execute_host);
	}
	if(error_str) {
		myad->Assign("ErrorMsg",error_str);
	}
	if(!critical_error) { //default is true
		myad->Assign("CriticalError",(int)critical_error);
	}
	if(hold_reason_code) {
		myad->Assign(ATTR_HOLD_REASON_CODE, hold_reason_code);
		myad->Assign(ATTR_HOLD_REASON_SUBCODE, hold_reason_subcode);
	}

	return myad;
}

void
RemoteErrorEvent::initFromClassAd(ClassAd* ad)
{
	ULogEvent::initFromClassAd(ad);
	char *buf;
	int crit_err = 0;

	if( !ad ) return;

	ad->LookupString("Daemon", daemon_name, sizeof(daemon_name));
	ad->LookupString("ExecuteHost", execute_host, sizeof(execute_host));
	if( ad->LookupString("ErrorMsg", &buf) ) {
		setErrorText(buf);
		free(buf);
	}
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

void
RemoteErrorEvent::setErrorText(char const *str)
{
	char *s = strnewp(str);
	delete [] error_str;
	error_str = s;
}

void
RemoteErrorEvent::setDaemonName(char const *str)
{
	if(!str) str = "";
	strncpy(daemon_name,str,sizeof(daemon_name));
	daemon_name[sizeof(daemon_name)-1] = '\0';
}

void
RemoteErrorEvent::setExecuteHost(char const *str)
{
	if(!str) str = "";
	strncpy(execute_host,str,sizeof(execute_host));
	execute_host[sizeof(execute_host)-1] = '\0';
}

// ----- the ExecuteEvent class
ExecuteEvent::ExecuteEvent(void)
{
	executeHost = NULL;
	remoteName = NULL;
	eventNumber = ULOG_EXECUTE;
}

ExecuteEvent::~ExecuteEvent(void)
{
	if( executeHost ) {
		delete[] executeHost;
	}
	if( remoteName ) {
		delete[] remoteName;
	}
}

void
ExecuteEvent::setExecuteHost(char const *addr)
{
	if( executeHost ) {
		delete[] executeHost;
	}
	if( addr ) {
		executeHost = strnewp(addr);
		ASSERT( executeHost );
	}
	else {
		executeHost = NULL;
	}
}

void ExecuteEvent::setRemoteName(char const *name)
{
	if( remoteName ) {
		delete[] remoteName;
	}
	if( name ) {
		remoteName = strnewp(name);
		ASSERT( remoteName );
	}
	else {
		remoteName = NULL;
	}
}

char const *
ExecuteEvent::getExecuteHost()
{
	if( !executeHost ) {
			// There are a few callers that do not expect NULL execute host,
			// so set it to empty string.
		setExecuteHost("");
	}
	return executeHost;
}

bool
ExecuteEvent::formatBody( std::string &out )
{
	int retval;

	retval = formatstr_cat( out, "Job executing on host: %s\n", executeHost );

	if (retval < 0) {
		return false;
	}

	return true;
}

int
ExecuteEvent::readEvent (FILE *file, bool & got_sync_line)
{
	MyString line;

#ifdef DONT_EVER_SEEK
	if ( ! read_line_value("Job executing on host: ", line, file, got_sync_line)) {
		return 0;
	}
	executeHost = line.detach_buffer();
	return 1;
#else
	if ( ! line.readLine(file) )
	{
		return 0; // EOF or error
	}
	setExecuteHost(line.Value()); // allocate memory
	int retval  = sscanf (line.Value(), "Job executing on host: %[^\n]",
						  executeHost);
	if (retval == 1)
	{
		return 1;
	}

	if(strcmp(line.Value(), "Job executing on host: \n") == 0) {
		// Simply lacks a hostname.  Allow.
		executeHost[0] = 0;
		return 1;
	}
	return 0;
#endif
}

ClassAd*
ExecuteEvent::toClassAd(bool event_time_utc)
{
	ClassAd* myad = ULogEvent::toClassAd(event_time_utc);
	if( !myad ) return NULL;

	if( executeHost && executeHost[0] ) {
		if( !myad->Assign("ExecuteHost",executeHost) ) return NULL;
	}

	return myad;
}

void
ExecuteEvent::initFromClassAd(ClassAd* ad)
{
	ULogEvent::initFromClassAd(ad);

	if( !ad ) return;

	char *mallocstr = NULL;
	ad->LookupString("ExecuteHost", &mallocstr);
	if( mallocstr ) {
		setExecuteHost( mallocstr );
		free( mallocstr );
		mallocstr = NULL;
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
ExecutableErrorEvent::readEvent (FILE *file, bool & got_sync_line)
{
#ifdef DONT_EVER_SEEK
	MyString line;
	if ( ! read_line_value("(", line, file, got_sync_line)) {
		return 0;
	}
	// get the error type number
	YourStringDeserializer ser(line.Value());
	if ( ! ser.deserialize_int((int*)&errType) || ! ser.deserialize_sep(")")) {
		return 0;
	}
	// we ignore the rest of the line.
#else
	int  retval;
	char buffer [128];

	// get the error number
	retval = fscanf (file, "(%d)", (int*)&errType);
	if (retval != 1)
	{
		return 0;
	}

	// skip over the rest of the line
	if (fgets (buffer, 128, file) == 0)
	{
		return 0;
	}
#endif
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
CheckpointedEvent::readEvent (FILE *file, bool & got_sync_line)
{
#ifdef DONT_EVER_SEEK
	MyString line;
	if ( ! read_line_value("Job was checkpointed.", line, file, got_sync_line)) {
		return 0;
	}
#else
	int retval = fscanf (file, "Job was checkpointed.\n");
	if (retval == EOF) {
		return 0;
	}
#endif

	char buffer[128];
	if (!readRusage(file,run_remote_rusage) || fgets (buffer,128,file) == 0  ||
		!readRusage(file,run_local_rusage)  || fgets (buffer,128,file) == 0)
		return 0;

#ifdef DONT_EVER_SEEK
	if ( ! read_optional_line(line, file, got_sync_line)) {
		return 1;		//backwards compatibility
	}
	sscanf(line.Value(), "\t%f  -  Run Bytes Sent By Job For Checkpoint", &sent_bytes);
#else
    if( !fscanf(file, "\t%f  -  Run Bytes Sent By Job For Checkpoint\n",
                &sent_bytes)) {
        return 1;		//backwards compatibility
    }
#endif
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

	char* usageStr = NULL;
	if( ad->LookupString("RunLocalUsage", &usageStr) ) {
		strToRusage(usageStr, run_local_rusage);
		free(usageStr);
	}
	usageStr = NULL;
	if( ad->LookupString("RunRemoteUsage", &usageStr) ) {
		strToRusage(usageStr, run_remote_rusage);
		free(usageStr);
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
	reason = NULL;
	core_file = NULL;
	pusageAd = NULL;
}


JobEvictedEvent::~JobEvictedEvent(void)
{
	if ( pusageAd ) delete pusageAd;
	delete[] reason;
	delete[] core_file;
}


void
JobEvictedEvent::setReason( const char* reason_str )
{
    delete[] reason;
    reason = NULL;
    if( reason_str ) {
        reason = strnewp( reason_str );
        if( !reason ) {
            EXCEPT( "ERROR: out of memory!" );
        }
    }
}


const char*
JobEvictedEvent::getReason( void ) const
{
	return reason;
}


void
JobEvictedEvent::setCoreFile( const char* core_name )
{
	delete[] core_file;
	core_file = NULL;
	if( core_name ) {
		core_file = strnewp( core_name );
		if( !core_file ) {
			EXCEPT( "ERROR: out of memory!" );
		}
	}
}


const char*
JobEvictedEvent::getCoreFile( void )
{
	return core_file;
}


int
JobEvictedEvent::readEvent( FILE *file, bool & got_sync_line )
{
	int  ckpt;
	char buffer [128];

	delete [] reason;
	reason = NULL;
	delete [] core_file;
	core_file = NULL;

#ifdef DONT_EVER_SEEK
	MyString line;
	if ( ! read_line_value("Job was evicted.", line, file, got_sync_line) || 
		 ! read_optional_line(line, file, got_sync_line)) {
		return 0;
	}
	if (2 != sscanf(line.Value(), "\t(%d) %127[a-zA-z ]", &ckpt, buffer)) {
		return 0;
	}
	checkpointed = (bool) ckpt;
#else
	if( (fscanf(file, "Job was evicted.") == EOF) ||
		(fscanf(file, "\n\t(%d) ", &ckpt) != 1) )
	{
		return 0;
	}
	checkpointed = (bool) ckpt;
	if( fgets(buffer, 128, file) == 0 ) {
		return 0;
	}
#endif

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

	if( !readRusage(file,run_remote_rusage) || !fgets(buffer,128,file) ||
		!readRusage(file,run_local_rusage) || !fgets(buffer,128,file) )
	{
		return 0;
	}

#ifdef DONT_EVER_SEEK
	if ( ! read_optional_line(line, file, got_sync_line) ||
		(1 != sscanf(line.Value(), "\t%f  -  Run Bytes Sent By Job", &sent_bytes)) ||
		 ! read_optional_line(line, file, got_sync_line) ||
		(1 != sscanf(line.Value(), "\t%f  -  Run Bytes Received By Job", &recvd_bytes)))
#else
	if( !fscanf(file, "\t%f  -  Run Bytes Sent By Job\n", &sent_bytes) ||
		!fscanf(file, "\t%f  -  Run Bytes Received By Job\n",
				&recvd_bytes) )
#endif
	{
		return 1;				// backwards compatibility
	}

	if( ! terminate_and_requeued ) {
			// nothing more to read
		return 1;
	}

		// now, parse the terminate and requeue specific stuff.

	int  normal_term;

#ifdef DONT_EVER_SEEK
	// we expect one of these
	//  \t(0) Normal termination (return value %d)
	//  \t(1) Abnormal termination (signal %d)
	// Then if abnormal termination one of these
	//  \t(0) No core file
	//  \t(1) Corefile in: %s
	if ( ! read_optional_line(line, file, got_sync_line) ||
		(2 != sscanf(line.Value(), "\t(%d) %127[^\r\n]", &normal_term, buffer)))
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
		line.trim();
		const char cpre[] = "(1) Corefile in: ";
		if (starts_with(line.Value(), cpre)) {
			setCoreFile( line.Value() + strlen(cpre) );
		} else if ( ! starts_with(line.Value(), "(0)")) {
			return 0; // not a valid value
		}
	}

	// finally, see if there's a reason.  this is optional.
	if (read_optional_line(line, file, got_sync_line, true)) {
		line.trim();
		reason = line.detach_buffer();
	}

#else
	if( fscanf(file, "\n\t(%d) ", &normal_term) != 1 ) {
		return 0;
	}
	if( normal_term ) {
		normal = true;
		if( fscanf(file, "Normal termination (return value %d)\n",
				   &return_value) !=1 ) {
			return 0;
		}
	} else {
		normal = false;
		if( fscanf(file, "Abnormal termination (signal %d)",
				   &signal_number) !=1 ) {
			return 0;
		}
		int  got_core;
		if( fscanf(file, "\n\t(%d) ", &got_core) != 1 ) {
			return 0;
		}
		if( got_core ) {
			if( fscanf(file, "Corefile in: ") == EOF ) {
				return 0;
			}
			if( !fgets(buffer, 128, file) ) {
				return 0;
			}
			chomp( buffer );
			setCoreFile( buffer );
		} else {
			if( !fgets(buffer, 128, file) ) {
				return 0;
			}
		}
	}
		// finally, see if there's a reason.  this is optional.

	// if we get a reason, fine. If we don't, we need to
	// rewind the file position.
	fpos_t filep;
	fgetpos( file, &filep );

    char reason_buf[BUFSIZ];
    if( !fgets( reason_buf, BUFSIZ, file ) ||
		strcmp( reason_buf, "...\n" ) == 0 ) {

		fsetpos( file, &filep );
		return 1;  // not considered failure
	}

	chomp( reason_buf );
		// This is strange, sometimes we get the \t from fgets(), and
		// sometimes we don't.  Instead of trying to figure out why,
		// we just check for it here and do the right thing...
	if( reason_buf[0] == '\t' && reason_buf[1] ) {
		setReason( &reason_buf[1] );
	} else {
		setReason( reason_buf );
	}
#endif
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

      if( core_file ) {
	retval = formatstr_cat( out, "\t(1) Corefile in: %s\n", core_file );
      }
      else {
	retval = formatstr_cat( out, "\t(0) No core file\n" );
      }
      if( retval < 0 ) {
	return false;
      }
    }

    if( reason ) {
      if( formatstr_cat( out, "\t%s\n", reason ) < 0 ) {
	return false;
      }
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

	if( reason ) {
		if( !myad->InsertAttr("Reason", reason) ) {
			delete myad;
			return NULL;
		}
	}
	if( core_file ) {
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

	char* usageStr = NULL;
	if( ad->LookupString("RunLocalUsage", &usageStr) ) {
		strToRusage(usageStr, run_local_rusage);
		free(usageStr);
	}
	usageStr = NULL;
	if( ad->LookupString("RunRemoteUsage", &usageStr) ) {
		strToRusage(usageStr, run_remote_rusage);
		free(usageStr);
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

	char* multi = NULL;
	ad->LookupString("Reason", &multi);
	if( multi ) {
		setReason(multi);
		free(multi);
		multi = NULL;
	}
	ad->LookupString("CoreFile", &multi);
	if( multi ) {
		setCoreFile(multi);
		free(multi);
		multi = NULL;
	}
}


// ----- JobAbortedEvent class
JobAbortedEvent::JobAbortedEvent (void) : toeTag(NULL)
{
	eventNumber = ULOG_JOB_ABORTED;
	reason = NULL;
}

JobAbortedEvent::~JobAbortedEvent(void)
{
	if( reason ) {
		delete[] reason;
	}
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

void
JobAbortedEvent::setReason( const char* reason_str )
{
	delete[] reason;
	reason = NULL;
	if( reason_str ) {
		reason = strnewp( reason_str );
		if( !reason ) {
			EXCEPT( "ERROR: out of memory!" );
		}
	}
}


const char*
JobAbortedEvent::getReason( void ) const
{
	return reason;
}


bool
JobAbortedEvent::formatBody( std::string &out )
{

	if( formatstr_cat( out, "Job was aborted.\n" ) < 0 ) {
		return false;
	}
	if( reason ) {
		if( formatstr_cat( out, "\t%s\n", reason ) < 0 ) {
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
JobAbortedEvent::readEvent (FILE *file, bool & got_sync_line)
{
	delete [] reason;
	reason = NULL;
#ifdef DONT_EVER_SEEK
	MyString line;
	if ( ! read_line_value("Job was aborted", line, file, got_sync_line)) {
		return 0;
	}
	// try to read the reason, this is optional
	if (read_optional_line(line, file, got_sync_line, true)) {
		line.trim();
		reason = line.detach_buffer();
	}

	// Try to read the ToE tag.
	if( got_sync_line ) { return 1; }
	if( read_optional_line( line, file, got_sync_line ) ) {
		if( line.empty() ) {
			if(! read_optional_line( line, file, got_sync_line )) {
				return 0;
			}
		}

		if( line.remove_prefix( "\tJob terminated by " ) ) {
			if( toeTag != NULL ) { delete toeTag; }
			toeTag = new ToE::Tag();
			if(! toeTag->readFromString( line )) {
				return 0;
			}
		} else {
			return 0;
		}
	}
#else
    #error New JobAbortedEvent::readEvent() not implemented with seeking.
#endif
	return 1;
}

ClassAd*
JobAbortedEvent::toClassAd(bool event_time_utc)
{
	ClassAd* myad = ULogEvent::toClassAd(event_time_utc);
	if( !myad ) return NULL;

	if( reason ) {
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

	char* multi = NULL;
	ad->LookupString("Reason", &multi);
	if( multi ) {
		setReason(multi);
		free(multi);
		multi = NULL;
	}

	setToeTag( dynamic_cast<classad::ClassAd *>(ad->Lookup(ATTR_JOB_TOE)) );
}

// ----- TerminatedEvent baseclass
TerminatedEvent::TerminatedEvent(void) : toeTag(NULL)
{
	normal = false;
	core_file = NULL;
	returnValue = signalNumber = -1;
	pusageAd = NULL;

	(void)memset((void*)&run_local_rusage,0,(size_t)sizeof(run_local_rusage));
	run_remote_rusage=total_local_rusage=total_remote_rusage=run_local_rusage;

	sent_bytes = recvd_bytes = total_sent_bytes = total_recvd_bytes = 0.0;
}

TerminatedEvent::~TerminatedEvent(void)
{
	if ( pusageAd ) delete pusageAd;
	delete[] core_file;
	if( toeTag ) { delete toeTag; }
}

void
TerminatedEvent::setToeTag( classad::ClassAd * tt ) {
	if( tt ) {
		if( toeTag ) { delete toeTag; }
		toeTag = new classad::ClassAd( * tt );
	}
}

void
TerminatedEvent::setCoreFile( const char* core_name )
{
	delete[] core_file;
	core_file = NULL;
	if( core_name ) {
		core_file = strnewp( core_name );
		if( !core_file ) {
            EXCEPT( "ERROR: out of memory!" );
		}
	}
}


const char*
TerminatedEvent::getCoreFile( void )
{
	return core_file;
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

		if( core_file ) {
			retval = formatstr_cat( out, "\t(1) Corefile in: %s\n\t",
									core_file );
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
TerminatedEvent::readEventBody( FILE *file, bool & got_sync_line, const char* header )
{
	char buffer[128];
	int  normalTerm;

	if (pusageAd) {
		pusageAd->Clear();
	}

#ifdef DONT_EVER_SEEK
	// when we get here, all of the header line will have been read

	MyString line;
	if ( ! read_optional_line(line, file, got_sync_line) ||
		(2 != sscanf(line.Value(), "\t(%d) %127[^\r\n]", &normalTerm, buffer))) {
		return 0;
	}

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
		line.trim();
		const char cpre[] = "(1) Corefile in: ";
		if (starts_with(line.Value(), cpre)) {
			setCoreFile( line.Value() + strlen(cpre) );
		} else if ( ! starts_with(line.Value(), "(0)")) {
			return 0; // not a valid value
		}
	}

	bool in_usage_ad = false;
#else
	int  gotCore;
	int  retval;

	if( (retval = fscanf (file, "\n\t(%d) ", &normalTerm)) != 1 ) {
		return 0;
	}

	if( normalTerm ) {
		normal = true;
		if(fscanf(file,"Normal termination (return value %d)",&returnValue)!=1)
			return 0;
	} else {
		normal = false;
		if((fscanf(file,"Abnormal termination (signal %d)",&signalNumber)!=1)||
		   (fscanf(file,"\n\t(%d) ", &gotCore) != 1))
			return 0;

		if( gotCore ) {
			if( fscanf(file, "Corefile in: ") == EOF ) {
				return 0;
			}
			if( !fgets(buffer, 128, file) ) {
				return 0;
			}
			chomp( buffer );
			setCoreFile( buffer );
		} else {
			if (fgets (buffer, 128, file) == 0)
				return 0;
		}
	}
#endif

		// read in rusage values
	if (!readRusage(file,run_remote_rusage) || !fgets(buffer, 128, file) ||
		!readRusage(file,run_local_rusage)   || !fgets(buffer, 128, file) ||
		!readRusage(file,total_remote_rusage)|| !fgets(buffer, 128, file) ||
		!readRusage(file,total_local_rusage) || !fgets(buffer, 128, file))
		return 0;

#ifdef DONT_EVER_SEEK
		// read in the transfer info, and then the resource usage info
		// we expect transfer info is first, as soon as we get a line that
		// doesn't match that pattern, we switch to resource usage parsing.
	UsageLineParser ulp;
#endif
	for (;;) {
		char srun[sizeof("Total")];
		char sdir[sizeof("Received")];
		char sjob[22];

#ifdef DONT_EVER_SEEK
		if ( ! read_optional_line(line, file, got_sync_line)) {
			break;
		}
		const char * sz = line.Value();
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
#else
		char sz[250];
		// if we hit end of file or end of record "..." rewind the file pointer.
		fpos_t filep;
		fgetpos( file, &filep );
		if ( ! fgets(sz, sizeof(sz), file) || 
			(sz[0] == '.' && sz[1] == '.' && sz[2] == '.')) {
			fsetpos( file, &filep );
			break;
		}
#endif

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
#ifdef DONT_EVER_SEEK
		else {
			// no match, we have (probably) just read the banner of the useage ad.
			if (starts_with(sz, "\tPartitionable ")) {
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
#else
		if ( ! fOK) {
			fsetpos(file, &filep);
			break;
		}
#endif
	}
#ifdef DONT_EVER_SEEK
#else
	// the usage ad is optional
	readUsageAd(file, &pusageAd);
#endif
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
				if( formatstr_cat( out, "\n\tJob terminated of its own accord at %s.\n", tag.when.c_str() ) < 0 ) {
					return false;
				}
			} else {
				rv = tag.writeToString( out );
			}
		}
	}
	return rv;
}


int
JobTerminatedEvent::readEvent (FILE *file, bool & got_sync_line)
{
#ifdef DONT_EVER_SEEK
	MyString str;
	if ( ! read_line_value("Job terminated.", str, file, got_sync_line)) {
		return 0;
	}
#else
	if( fscanf(file, "Job terminated.") == EOF ) {
		return 0;
	}
#endif
	int rv = TerminatedEvent::readEventBody( file, got_sync_line, "Job" );
	if(! rv) { return rv; }

	MyString line;
	if( got_sync_line ) { return 1; }
	if( read_optional_line( line, file, got_sync_line ) ) {
		if(line.empty()) {
			if( read_optional_line( line, file, got_sync_line ) ) {
				return 0;
			}
		}
		if( line.remove_prefix( "\tJob terminated of its own accord at " ) ) {
			if( toeTag != NULL ) {
				delete toeTag;
			}
			toeTag = new classad::ClassAd();

			toeTag->InsertAttr( "Who", ToE::itself );
			toeTag->InsertAttr( "How", ToE::strings[ToE::OfItsOwnAccord] );
			toeTag->InsertAttr( "HowCode", ToE::OfItsOwnAccord );

			// This code gets more complicated if we don't assume UTC i/o.
			struct tm eventTime;
			iso8601_to_time( line.Value(), & eventTime, NULL, NULL );
			toeTag->InsertAttr( "When", timegm(&eventTime) );
		} else if( line.remove_prefix( "\tJob terminated by " ) ) {
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

	const char* core = getCoreFile();
	if( core ) {
		if( !myad->InsertAttr("CoreFile", core) ) {
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

	char* multi = NULL;
	ad->LookupString("CoreFile", &multi);
	if( multi ) {
		setCoreFile(multi);
		free(multi);
		multi = NULL;
	}

	if( ad->LookupString("RunLocalUsage", &multi) ) {
		strToRusage(multi, run_local_rusage);
		free(multi);
	}
	if( ad->LookupString("RunRemoteUsage", &multi) ) {
		strToRusage(multi, run_remote_rusage);
		free(multi);
	}
	if( ad->LookupString("TotalLocalUsage", &multi) ) {
		strToRusage(multi, total_local_rusage);
		free(multi);
	}
	if( ad->LookupString("TotalRemoteUsage", &multi) ) {
		strToRusage(multi, total_remote_rusage);
		free(multi);
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
JobImageSizeEvent::readEvent (FILE *file, bool & got_sync_line)
{
#ifdef DONT_EVER_SEEK
	MyString str;
	if ( ! read_line_value("Image size of job updated: ", str, file, got_sync_line) || 
		! YourStringDeserializer(str.Value()).deserialize_int(&image_size_kb))
	{
		return 0;
	}
#else
	int retval;
	if ((retval=fscanf(file,"Image size of job updated: %lld\n", &image_size_kb)) != 1)
		return 0;
#endif

	// These fields were added to this event in 2012, so we need to tolerate the
	// situation when they are not there in the log that we read back.
	memory_usage_mb = -1;
	resident_set_size_kb = 0;
	proportional_set_size_kb = -1;

	for (;;) {
		char sz[250];

#ifdef DONT_EVER_SEEK
		if ( ! read_optional_line(file, got_sync_line, sz, sizeof(sz))) {
			break;
		}
#else
		// if we hit end of file or end of record "..." rewind the file pointer.
		fpos_t filep;
		fgetpos(file, &filep);
		if ( ! fgets(sz, sizeof(sz), file) || 
			(sz[0] == '.' && sz[1] == '.' && sz[2] == '.')) {
			fsetpos(file, &filep);
			break;
		}
#endif

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
#ifdef DONT_EVER_SEEK
#else
			// rewind the file pointer so we don't consume what we can't parse.
			fsetpos(file, &filep);
#endif
			break;
		} else {
			if (MATCH == strcasecmp(lbl,"MemoryUsage")) {
				memory_usage_mb = val;
			} else if (MATCH == strcasecmp(lbl, "ResidentSetSize")) {
				resident_set_size_kb = val;
			} else if (MATCH == strcasecmp(lbl, "ProportionalSetSize")) {
				proportional_set_size_kb = val;
			} else {
#ifdef DONT_EVER_SEEK
#else
				// rewind the file pointer so we don't consume what we can't parse.
				fsetpos(file, &filep);
#endif
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
	message[0] = '\0';
	sent_bytes = recvd_bytes = 0.0;
	began_execution = FALSE;
}

ShadowExceptionEvent::~ShadowExceptionEvent (void)
{
}

int
ShadowExceptionEvent::readEvent (FILE *file, bool & got_sync_line)
{
#ifdef DONT_EVER_SEEK
	MyString line;
	if ( ! read_line_value("Shadow exception!", line, file, got_sync_line)) {
		return 0;
	}
	// read message
	if ( ! read_optional_line(file, got_sync_line, message, sizeof(message), true, true)) {
		return 1; // backwards compatibility
	}

	// read transfer info
	if ( ! read_optional_line(line, file, got_sync_line) ||
		(1 != sscanf (line.Value(), "\t%f  -  Run Bytes Sent By Job", &sent_bytes)) ||
		! read_optional_line(line, file, got_sync_line) ||
		(1 != sscanf (line.Value(), "\t%f  -  Run Bytes Received By Job", &recvd_bytes)))
	{
		return 1;				// backwards compatibility
	}
#else
	if (fscanf (file, "Shadow exception!\n\t") == EOF)
		return 0;
	if (fgets(message, BUFSIZ, file) == NULL) {
		message[0] = '\0';
		return 1;				// backwards compatibility
	}

	// remove '\n' from message
	message[strlen(message)-1] = '\0';

	if (fscanf (file, "\t%f  -  Run Bytes Sent By Job\n", &sent_bytes) == 0 ||
		fscanf (file, "\t%f  -  Run Bytes Received By Job\n",
				&recvd_bytes) == 0)
		return 1;				// backwards compatibility
#endif
	return 1;
}

bool
ShadowExceptionEvent::formatBody( std::string &out )
{
	if (formatstr_cat( out, "Shadow exception!\n\t" ) < 0)
		return false;
	if (formatstr_cat( out, "%s\n", message ) < 0)
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

	ad->LookupString("Message", message, BUFSIZ);

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
JobSuspendedEvent::readEvent (FILE *file, bool & got_sync_line)
{
#ifdef DONT_EVER_SEEK
	MyString line;
	if ( ! read_line_value("Job was suspended.", line, file, got_sync_line)) {
		return 0;
	}
	if ( ! read_optional_line(line, file, got_sync_line) ||
		(1 != sscanf (line.Value(), "\tNumber of processes actually suspended: %d", &num_pids)))
	{
		return 0;
	}
#else
	if (fscanf (file, "Job was suspended.\n\t") == EOF)
		return 0;
	if (fscanf (file, "Number of processes actually suspended: %d\n",
			&num_pids) == EOF)
		return 1;				// backwards compatibility
#endif
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
JobUnsuspendedEvent::readEvent (FILE *file, bool & got_sync_line)
{
#ifdef DONT_EVER_SEEK
	MyString line;
	if ( ! read_line_value("Job was unsuspended.", line, file, got_sync_line)) {
		return 0;
	}
#else
	if (fscanf (file, "Job was unsuspended.\n") == EOF)
		return 0;
#endif
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
	reason = NULL;
	code = 0;
	subcode = 0;
}


JobHeldEvent::~JobHeldEvent (void)
{
	delete[] reason;
}


void
JobHeldEvent::setReason( const char* reason_str )
{
    delete[] reason;
    reason = NULL;
    if( reason_str ) {
        reason = strnewp( reason_str );
        if( !reason ) {
            EXCEPT( "ERROR: out of memory!" );
        }
    }
}

void
JobHeldEvent::setReasonCode(const int val)
{
	code = val;
}

void
JobHeldEvent::setReasonSubCode(const int val)
{
	subcode = val;
}


const char*
JobHeldEvent::getReason( void ) const
{
	return reason;
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
JobHeldEvent::readEvent( FILE *file, bool & got_sync_line )
{
#ifdef DONT_EVER_SEEK
	delete [] reason;
	reason = NULL;
	code = subcode = 0;

	MyString line;
	if ( ! read_line_value("Job was held.", line, file, got_sync_line)) {
		return 0;
	}
	// try to read the reason, this is optional
	if ( ! read_optional_line(line, file, got_sync_line, true)) {
		return 1;	// backwards compatibility
	}
	line.trim();
	if (line != "Reason unspecified") {
		reason = line.detach_buffer();
	}

	int incode = 0;
	int insubcode = 0;
	if ( ! read_optional_line(line, file, got_sync_line) ||
		(2 != sscanf(line.Value(), "\tCode %d Subcode %d", &incode,&insubcode)))
	{
		return 1;	// backwards compatibility
	}

	code = incode;
	subcode = insubcode;
#else
	if( fscanf(file, "Job was held.\n") == EOF ) {
		return 0;
	}

	// try to read the reason, but if its not there,
	// rewind so we don't slurp up the next event delimiter
	fpos_t filep;
	fgetpos( file, &filep );
	char reason_buf[BUFSIZ];
	if( !fgets( reason_buf, BUFSIZ, file ) ||
		   	strcmp( reason_buf, "...\n" ) == 0 ) {
		setReason( NULL );
		fsetpos( file, &filep );
		return 1;	// backwards compatibility
	}


	chomp( reason_buf );  // strip the newline
		// This is strange, sometimes we get the \t from fgets(), and
		// sometimes we don't.  Instead of trying to figure out why,
		// we just check for it here and do the right thing...
	if( reason_buf[0] == '\t' && reason_buf[1] ) {
		reason = strnewp( &reason_buf[1] );
	} else {
		reason = strnewp( reason_buf );
	}

	// read the code and subcodes, but if not there, rewind
	// for backwards compatibility.
	fgetpos( file, &filep );
	int incode = 0;
	int insubcode = 0;
	int fsf_ret = fscanf(file, "\tCode %d Subcode %d\n", &incode,&insubcode);
	if ( fsf_ret != 2 ) {
		code = 0;
		subcode = 0;
		fsetpos( file, &filep );
		return 1;	// backwards compatibility
	}
	code = incode;
	subcode = insubcode;
#endif

	return 1;
}


bool
JobHeldEvent::formatBody( std::string &out )
{
	if( formatstr_cat( out, "Job was held.\n" ) < 0 ) {
		return false;
	}
	if( reason ) {
		if( formatstr_cat( out, "\t%s\n", reason ) < 0 ) {
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

	const char* hold_reason = getReason();
	if ( hold_reason ) {
		if( !myad->InsertAttr(ATTR_HOLD_REASON, hold_reason) ) {
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

	char* multi = NULL;
	int incode = 0;
	int insubcode = 0;
	ad->LookupString(ATTR_HOLD_REASON, &multi);
	if( multi ) {
		setReason(multi);
		free(multi);
		multi = NULL;
	}
	ad->LookupInteger(ATTR_HOLD_REASON_CODE, incode);
	setReasonCode(incode);
	ad->LookupInteger(ATTR_HOLD_REASON_SUBCODE, insubcode);
	setReasonSubCode(insubcode);
}

JobReleasedEvent::JobReleasedEvent(void)
{
	eventNumber = ULOG_JOB_RELEASED;
	reason = NULL;
}


JobReleasedEvent::~JobReleasedEvent(void)
{
	delete[] reason;
}


void
JobReleasedEvent::setReason( const char* reason_str )
{
    delete[] reason;
    reason = NULL;
    if( reason_str ) {
        reason = strnewp( reason_str );
        if( !reason ) {
            EXCEPT( "ERROR: out of memory!" );
        }
    }
}


const char*
JobReleasedEvent::getReason( void ) const
{
	return reason;
}


int
JobReleasedEvent::readEvent( FILE *file, bool & got_sync_line )
{
#ifdef DONT_EVER_SEEK
	MyString line;
	if ( ! read_line_value("Job was released.", line, file, got_sync_line)) {
		return 0;
	}
	// try to read the reason, this is optional
	if ( ! read_optional_line(line, file, got_sync_line, true)) {
		return 1;	// backwards compatibility
	}
	line.trim();
	if (! line.empty()) {
		reason = line.detach_buffer();
	}
#else
	if( fscanf(file, "Job was released.\n") == EOF ) {
		return 0;
	}
	// try to read the reason, but if its not there,
	// rewind so we don't slurp up the next event delimiter
	fpos_t filep;
	fgetpos( file, &filep );
	char reason_buf[BUFSIZ];
	if( !fgets( reason_buf, BUFSIZ, file ) ||
		   	strcmp( reason_buf, "...\n" ) == 0 ) {
		setReason( NULL );
		fsetpos( file, &filep );
		return 1;	// backwards compatibility
	}

	chomp( reason_buf );  // strip the newline
		// This is strange, sometimes we get the \t from fgets(), and
		// sometimes we don't.  Instead of trying to figure out why,
		// we just check for it here and do the right thing...
	if( reason_buf[0] == '\t' && reason_buf[1] ) {
		reason = strnewp( &reason_buf[1] );
	} else {
		reason = strnewp( reason_buf );
	}
#endif
	return 1;
}


bool
JobReleasedEvent::formatBody( std::string &out )
{
	if( formatstr_cat( out, "Job was released.\n" ) < 0 ) {
		return false;
	}
	if( reason ) {
		if( formatstr_cat( out, "\t%s\n", reason ) < 0 ) {
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

	const char* release_reason = getReason();
	if( release_reason ) {
		if( !myad->InsertAttr("Reason", release_reason) ) {
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

	char* multi = NULL;
	ad->LookupString("Reason", &multi);
	if( multi ) {
		setReason(multi);
		free(multi);
		multi = NULL;
	}
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

int
ULogEvent::readRusage (FILE *file, rusage &usage)
{
	int usr_secs, usr_minutes, usr_hours, usr_days;
	int sys_secs, sys_minutes, sys_hours, sys_days;
	int retval;

	retval = fscanf (file, "\tUsr %d %d:%d:%d, Sys %d %d:%d:%d",
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
 
	sprintf(result, "Usr %d %02d:%02d:%02d, Sys %d %02d:%02d:%02d",
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
	executeHost = NULL;
	eventNumber = ULOG_NODE_EXECUTE;
	node = -1;
}


NodeExecuteEvent::~NodeExecuteEvent(void)
{
	if( executeHost ) {
		delete[] executeHost;
	}
}

void
NodeExecuteEvent::setExecuteHost(char const *addr)
{
	if( executeHost ) {
		delete[] executeHost;
	}
	if( addr ) {
		executeHost = strnewp(addr);
		ASSERT( executeHost );
	}
	else {
		executeHost = NULL;
	}
}

bool
NodeExecuteEvent::formatBody( std::string &out )
{
	if( !executeHost ) {
		setExecuteHost("");
	}
	return( formatstr_cat( out, "Node %d executing on host: %s\n",
						   node, executeHost ) >= 0 );
}


int
NodeExecuteEvent::readEvent (FILE *file, bool & /*got_sync_line*/)
{
	MyString line;
	if( !line.readLine(file) ) {
		return 0; // EOF or error
	}
	line.chomp();
	setExecuteHost(line.Value()); // allocate memory
	int retval = sscanf(line.Value(), "Node %d executing on host: %s",
						&node, executeHost);
	return retval == 2;
}

ClassAd*
NodeExecuteEvent::toClassAd(bool event_time_utc)
{
	ClassAd* myad = ULogEvent::toClassAd(event_time_utc);
	if( !myad ) return NULL;

	if( executeHost ) {
		if( !myad->InsertAttr("ExecuteHost",executeHost) ) return NULL;
	}
	if( !myad->InsertAttr("Node", node) ) {
		delete myad;
		return NULL;
	}

	return myad;
}

void
NodeExecuteEvent::initFromClassAd(ClassAd* ad)
{
	ULogEvent::initFromClassAd(ad);

	if( !ad ) return;

	char *mallocstr = NULL;
	ad->LookupString("ExecuteHost", &mallocstr);
	if( mallocstr ) {
		setExecuteHost(mallocstr);
		free(mallocstr);
		mallocstr = NULL;
	}

	ad->LookupInteger("Node", node);
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
NodeTerminatedEvent::readEvent( FILE *file, bool & got_sync_line )
{
#ifdef DONT_EVER_SEEK
	MyString str;
	if ( ! read_optional_line(str, file, got_sync_line) || 
		(1 != sscanf(str.Value(), "Node %d terminated.", &node)))
	{
		return 0;
	}
#else
	if( fscanf(file, "Node %d terminated.", &node) == EOF ) {
		return 0;
	}
#endif
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

	const char* core = getCoreFile();
	if( core ) {
		if( !myad->InsertAttr("CoreFile", core) ) {
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

	char* multi = NULL;
	ad->LookupString("CoreFile", &multi);
	if( multi ) {
		setCoreFile(multi);
		free(multi);
		multi = NULL;
	}

	if( ad->LookupString("RunLocalUsage", &multi) ) {
		strToRusage(multi, run_local_rusage);
		free(multi);
	}
	if( ad->LookupString("RunRemoteUsage", &multi) ) {
		strToRusage(multi, run_remote_rusage);
		free(multi);
	}
	if( ad->LookupString("TotalLocalUsage", &multi) ) {
		strToRusage(multi, total_local_rusage);
		free(multi);
	}
	if( ad->LookupString("TotalRemoteUsage", &multi) ) {
		strToRusage(multi, total_remote_rusage);
		free(multi);
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
	dagNodeName = NULL;
}


PostScriptTerminatedEvent::~PostScriptTerminatedEvent(void)
{
	if( dagNodeName ) {
		delete[] dagNodeName;
	}
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

    if( dagNodeName ) {
        if( formatstr_cat( out, "    %s%.8191s\n",
					 dagNodeNameLabel, dagNodeName ) < 0 ) {
            return false;
        }
    }

    return true;
}


int
PostScriptTerminatedEvent::readEvent( FILE* file, bool & got_sync_line )
{
		// first clear any existing DAG node name
	if( dagNodeName ) {
		delete[] dagNodeName;
	}
    dagNodeName = NULL;

#ifdef DONT_EVER_SEEK
	MyString line;
	if ( ! read_line_value("POST Script terminated.", line, file, got_sync_line)) {
		return 0;
	}
	char buf[128];
	int tmp;
	if ( ! read_optional_line(line, file, got_sync_line) ||
		(2 != sscanf(line.Value(), "\t(%d) %127[^\r\n]", &tmp, buf)))
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
	line.trim();
	if (starts_with(line.Value(), dagNodeNameLabel)) {
		size_t label_len = strlen( dagNodeNameLabel );
		dagNodeName = strnewp( line.Value() + label_len );
	}

#else
	int tmp;
	char buf[8192];
	buf[0] = '\0';


	if( fscanf( file, "POST Script terminated.\n\t(%d) ", &tmp ) != 1 ) {
		return 0;
	}
	if( tmp == 1 ) {
		normal = true;
	} else {
		normal = false;
	}
    if( normal ) {
        if( fscanf( file, "Normal termination (return value %d)\n",
					&returnValue ) != 1 ) {
            return 0;
		}
    } else {
        if( fscanf( file, "Abnormal termination (signal %d)\n",
					&signalNumber ) != 1 ) {
            return 0;
		}
    }

	// see if the next line contains an optional DAG node name string,
	// and, if not, rewind, because that means we slurped in the next
	// event delimiter looking for it...

	fpos_t filep;
	fgetpos( file, &filep );

	if( !fgets( buf, 8192, file ) || strcmp( buf, "...\n" ) == 0 ) {
		fsetpos( file, &filep );
		return 1;
	}

	// remove trailing newline
	buf[ strlen( buf ) - 1 ] = '\0';

		// skip "DAG Node: " label to find start of actual node name
	int label_len = strlen( dagNodeNameLabel );
	dagNodeName = strnewp( buf + label_len );
#endif
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
	if( dagNodeName && dagNodeName[0] ) {
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

	if( dagNodeName ) {
		delete[] dagNodeName;
		dagNodeName = NULL;
	}
	char* mallocstr = NULL;
	ad->LookupString( dagNodeNameAttr, &mallocstr );
	if( mallocstr ) {
		dagNodeName = strnewp( mallocstr );
		free( mallocstr );
		mallocstr = NULL;
	}
}


// ----- JobDisconnectedEvent class

JobDisconnectedEvent::JobDisconnectedEvent(void)
{
	eventNumber = ULOG_JOB_DISCONNECTED;
	startd_addr = NULL;
	startd_name = NULL;
	disconnect_reason = NULL;
	no_reconnect_reason = NULL;
	can_reconnect = true;
}


JobDisconnectedEvent::~JobDisconnectedEvent(void)
{
	if( startd_addr ) {
		delete [] startd_addr;
	}
	if( startd_name ) {
		delete [] startd_name;
	}
	if( disconnect_reason ) {
		delete [] disconnect_reason;
	}
	if( no_reconnect_reason ) {
		delete [] no_reconnect_reason;
	}
}


void
JobDisconnectedEvent::setStartdAddr( const char* startd )
{
	if( startd_addr ) {
		delete[] startd_addr;
		startd_addr = NULL;
	}
	if( startd ) {
		startd_addr = strnewp( startd );
		if( !startd_addr ) {
			EXCEPT( "ERROR: out of memory!" );
		}
	}
}


void
JobDisconnectedEvent::setStartdName( const char* name )
{
	if( startd_name ) {
		delete[] startd_name;
		startd_name = NULL;
	}
	if( name ) {
		startd_name = strnewp( name );
		if( !startd_name ) {
			EXCEPT( "ERROR: out of memory!" );
		}
	}
}


void
JobDisconnectedEvent::setDisconnectReason( const char* reason_str )
{
	if( disconnect_reason ) {
		delete [] disconnect_reason;
		disconnect_reason = NULL;
	}
	if( reason_str ) {
		disconnect_reason = strnewp( reason_str );
		if( !disconnect_reason ) {
			EXCEPT( "ERROR: out of memory!" );
		}
	}
}


void
JobDisconnectedEvent::setNoReconnectReason( const char* reason_str )
{
	if( no_reconnect_reason ) {
		delete [] no_reconnect_reason;
		no_reconnect_reason = NULL;
	}
	if( reason_str ) {
		no_reconnect_reason = strnewp( reason_str );
		if( !no_reconnect_reason ) {
			EXCEPT( "ERROR: out of memory!" );
		}
		can_reconnect = false;
	}
}


bool
JobDisconnectedEvent::formatBody( std::string &out )
{
	if( ! disconnect_reason ) {
		EXCEPT( "JobDisconnectedEvent::formatBody() called without "
				"disconnect_reason" );
	}
	if( ! startd_addr ) {
		EXCEPT( "JobDisconnectedEvent::formatBody() called without "
				"startd_addr" );
	}
	if( ! startd_name ) {
		EXCEPT( "JobDisconnectedEvent::formatBody() called without "
				"startd_name" );
	}
	if( ! can_reconnect && ! no_reconnect_reason ) {
		EXCEPT( "impossible: JobDisconnectedEvent::formatBody() called "
				"without no_reconnect_reason when can_reconnect is FALSE" );
	}

	if( formatstr_cat( out, "Job disconnected, %s reconnect\n",
					   can_reconnect ? "attempting to" : "can not" ) < 0 ) {
		return false;
	}
	if( formatstr_cat( out, "    %.8191s\n", disconnect_reason ) < 0 ) {
		return false;
	}
	if( formatstr_cat( out, "    %s reconnect to %s %s\n",
					   can_reconnect ? "Trying to" : "Can not",
					   startd_name, startd_addr ) < 0 ) {
		return false;
	}
	if( no_reconnect_reason ) {
		if( formatstr_cat( out, "    %.8191s\n", no_reconnect_reason ) < 0 ) {
			return false;
		}
		if( formatstr_cat( out, "    Rescheduling job\n" ) < 0 ) {
			return false;
		}
	}
	return true;
}


int
JobDisconnectedEvent::readEvent( FILE *file, bool & /*got_sync_line*/ )
{
	MyString line;
	if(line.readLine(file) && line.replaceString("Job disconnected, ", "")) {
		line.chomp();
		if( line == "attempting to reconnect" ) {
			can_reconnect = true;
		} else if( line == "can not reconnect" ) {
			can_reconnect = false;
		} else {
			return 0;
		}
	} else {
		return 0;
	}

	if( line.readLine(file) && line[0] == ' ' && line[1] == ' '
		&& line[2] == ' ' && line[3] == ' ' && line[4] )
	{
		line.chomp();
		setDisconnectReason( line.Value()+4 );
	} else {
		return 0;
	}

	if( ! line.readLine(file) ) {
		return 0;
	}
	line.chomp();
	if( line.replaceString("    Trying to reconnect to ", "") ) {
		int i = line.FindChar( ' ' );
		if( i > 0 ) {
			setStartdAddr( line.Value()+(i+1) );
			line.truncate( i );
			setStartdName( line.Value() );
		} else {
			return 0;
		}
	} else if( line.replaceString("    Can not reconnect to ", "") ) {
		if( can_reconnect ) {
			return 0;
		}
		int i = line.FindChar( ' ' );
		if( i > 0 ) {
			setStartdAddr( line.Value()+(i+1) );
			line.truncate( i );
			setStartdName( line.Value() );
		} else {
			return 0;
		}
		if( line.readLine(file) && line[0] == ' ' && line[1] == ' '
			&& line[2] == ' ' && line[3] == ' ' && line[4] )
		{
			line.chomp();
			setNoReconnectReason( line.Value()+4 );
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
	if( ! disconnect_reason ) {
		EXCEPT( "JobDisconnectedEvent::toClassAd() called without"
				"disconnect_reason" );
	}
	if( ! startd_addr ) {
		EXCEPT( "JobDisconnectedEvent::toClassAd() called without "
				"startd_addr" );
	}
	if( ! startd_name ) {
		EXCEPT( "JobDisconnectedEvent::toClassAd() called without "
				"startd_name" );
	}
	if( ! can_reconnect && ! no_reconnect_reason ) {
		EXCEPT( "JobDisconnectedEvent::toClassAd() called without "
				"no_reconnect_reason when can_reconnect is FALSE" );
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

	MyString line = "Job disconnected, ";
	if( can_reconnect ) {
		line += "attempting to reconnect";
	} else {
		line += "can not reconnect, rescheduling job";
	}
	if( !myad->InsertAttr("EventDescription", line.Value()) ) {
		delete myad;
		return NULL;
	}

	if( no_reconnect_reason ) {
		if( !myad->InsertAttr("NoReconnectReason", no_reconnect_reason) ) {
			return NULL;
		}
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

	// this fanagling is to ensure we don't malloc a pointer then delete it
	char* mallocstr = NULL;
	ad->LookupString( "DisconnectReason", &mallocstr );
	if( mallocstr ) {
		setDisconnectReason( mallocstr );
		free( mallocstr );
		mallocstr = NULL;
	}

	ad->LookupString( "NoReconnectReason", &mallocstr );
	if( mallocstr ) {
		setNoReconnectReason( mallocstr );
		free( mallocstr );
		mallocstr = NULL;
	}

	ad->LookupString( "StartdAddr", &mallocstr );
	if( mallocstr ) {
		setStartdAddr( mallocstr );
		free( mallocstr );
		mallocstr = NULL;
	}

	ad->LookupString( "StartdName", &mallocstr );
	if( mallocstr ) {
		setStartdName( mallocstr );
		free( mallocstr );
		mallocstr = NULL;
	}
}


// ----- JobReconnectedEvent class

JobReconnectedEvent::JobReconnectedEvent(void)
{
	eventNumber = ULOG_JOB_RECONNECTED;
	startd_addr = NULL;
	startd_name = NULL;
	starter_addr = NULL;
}


JobReconnectedEvent::~JobReconnectedEvent(void)
{
	if( startd_addr ) {
		delete [] startd_addr;
	}
	if( startd_name ) {
		delete [] startd_name;
	}
	if( starter_addr ) {
		delete [] starter_addr;
	}
}


void
JobReconnectedEvent::setStartdAddr( const char* startd )
{
	if( startd_addr ) {
		delete[] startd_addr;
		startd_addr = NULL;
	}
	if( startd ) {
		startd_addr = strnewp( startd );
		if( !startd_addr ) {
			EXCEPT( "ERROR: out of memory!" );
		}
	}
}


void
JobReconnectedEvent::setStartdName( const char* name )
{
	if( startd_name ) {
		delete[] startd_name;
		startd_name = NULL;
	}
	if( name ) {
		startd_name = strnewp( name );
		if( !startd_name ) {
			EXCEPT( "ERROR: out of memory!" );
		}
	}
}


void
JobReconnectedEvent::setStarterAddr( const char* starter )
{
	if( starter_addr ) {
		delete[] starter_addr;
		starter_addr = NULL;
	}
	if( starter ) {
		starter_addr = strnewp( starter );
		if( !starter_addr ) {
			EXCEPT( "ERROR: out of memory!" );
		}
	}
}


bool
JobReconnectedEvent::formatBody( std::string &out )
{
	if( ! startd_addr ) {
		EXCEPT( "JobReconnectedEvent::formatBody() called without "
				"startd_addr" );
	}
	if( ! startd_name ) {
		EXCEPT( "JobReconnectedEvent::formatBody() called without "
				"startd_name" );
	}
	if( ! starter_addr ) {
		EXCEPT( "JobReconnectedEvent::formatBody() called without "
				"starter_addr" );
	}

	if( formatstr_cat( out, "Job reconnected to %s\n", startd_name ) < 0 ) {
		return false;
	}
	if( formatstr_cat( out, "    startd address: %s\n", startd_addr ) < 0 ) {
		return false;
	}
	if( formatstr_cat( out, "    starter address: %s\n", starter_addr ) < 0 ) {
		return false;
	}
	return true;
}


int
JobReconnectedEvent::readEvent( FILE *file, bool & /*got_sync_line*/ )
{
	MyString line;

	if( line.readLine(file) &&
		line.replaceString("Job reconnected to ", "") )
	{
		line.chomp();
		setStartdName( line.Value() );
	} else {
		return 0;
	}

	if( line.readLine(file) &&
		line.replaceString( "    startd address: ", "" ) )
	{
		line.chomp();
		setStartdAddr( line.Value() );
	} else {
		return 0;
	}

	if( line.readLine(file) &&
		line.replaceString( "    starter address: ", "" ) )
	{
		line.chomp();
		setStarterAddr( line.Value() );
	} else {
		return 0;
	}

	return 1;
}


ClassAd*
JobReconnectedEvent::toClassAd(bool event_time_utc)
{
	if( ! startd_addr ) {
		EXCEPT( "JobReconnectedEvent::toClassAd() called without "
				"startd_addr" );
	}
	if( ! startd_name ) {
		EXCEPT( "JobReconnectedEvent::toClassAd() called without "
				"startd_name" );
	}
	if( ! starter_addr ) {
		EXCEPT( "JobReconnectedEvent::toClassAd() called without "
				"starter_addr" );
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

	// this fanagling is to ensure we don't malloc a pointer then delete it
	char* mallocstr = NULL;
	ad->LookupString( "StartdAddr", &mallocstr );
	if( mallocstr ) {
		if( startd_addr ) {
			delete [] startd_addr;
		}
		startd_addr = strnewp( mallocstr );
		free( mallocstr );
		mallocstr = NULL;
	}

	ad->LookupString( "StartdName", &mallocstr );
	if( mallocstr ) {
		if( startd_name ) {
			delete [] startd_name;
		}
		startd_name = strnewp( mallocstr );
		free( mallocstr );
		mallocstr = NULL;
	}

	ad->LookupString( "StarterAddr", &mallocstr );
	if( mallocstr ) {
		if( starter_addr ) {
			delete [] starter_addr;
		}
		starter_addr = strnewp( mallocstr );
		free( mallocstr );
		mallocstr = NULL;
	}
}


// ----- JobReconnectFailedEvent class

JobReconnectFailedEvent::JobReconnectFailedEvent(void)
{
	eventNumber = ULOG_JOB_RECONNECT_FAILED;
	reason = NULL;
	startd_name = NULL;
}


JobReconnectFailedEvent::~JobReconnectFailedEvent(void)
{
	if( reason ) {
		delete [] reason;
	}
	if( startd_name ) {
		delete [] startd_name;
	}
}


void
JobReconnectFailedEvent::setReason( const char* reason_str )
{
	if( reason ) {
		delete [] reason;
		reason = NULL;
	}
	if( reason_str ) {
		reason = strnewp( reason_str );
		if( !reason ) {
			EXCEPT( "ERROR: out of memory!" );
		}
	}
}


void
JobReconnectFailedEvent::setStartdName( const char* name )
{
	if( startd_name ) {
		delete[] startd_name;
		startd_name = NULL;
	}
	if( name ) {
		startd_name = strnewp( name );
		if( !startd_name ) {
			EXCEPT( "ERROR: out of memory!" );
		}
	}
}


bool
JobReconnectFailedEvent::formatBody( std::string &out )
{
	if( ! reason ) {
		EXCEPT( "JobReconnectFailedEvent::formatBody() called without "
				"reason" );
	}
	if( ! startd_name ) {
		EXCEPT( "JobReconnectFailedEvent::formatBody() called without "
				"startd_name" );
	}

	if( formatstr_cat( out, "Job reconnection failed\n" ) < 0 ) {
		return false;
	}
	if( formatstr_cat( out, "    %.8191s\n", reason ) < 0 ) {
		return false;
	}
	if( formatstr_cat( out, "    Can not reconnect to %s, rescheduling job\n",
					   startd_name ) < 0 ) {
		return false;
	}
	return true;
}


int
JobReconnectFailedEvent::readEvent( FILE *file, bool & /*got_sync_line*/ )
{
	MyString line;

		// the first line contains no useful information for us, but
		// it better be there or we've got a parse error.
	if( ! line.readLine(file) ) {
		return 0;
	}

		// 2nd line is the reason
	if( line.readLine(file) && line[0] == ' ' && line[1] == ' '
		&& line[2] == ' ' && line[3] == ' ' && line[4] )
	{
		line.chomp();
		setReason( line.Value()+4 );
	} else {
		return 0;
	}

		// 3rd line is who we tried to reconnect to
	if( line.readLine(file) &&
		line.replaceString( "    Can not reconnect to ", "" ) )
	{
			// now everything until the first ',' will be the name
		int i = line.FindChar( ',' );
		if( i > 0 ) {
			line.truncate( i );
			setStartdName( line.Value() );
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
	if( ! reason ) {
		EXCEPT( "JobReconnectFailedEvent::toClassAd() called without "
				"reason" );
	}
	if( ! startd_name ) {
		EXCEPT( "JobReconnectFailedEvent::toClassAd() called without "
				"startd_name" );
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

	// this fanagling is to ensure we don't malloc a pointer then delete it
	char* mallocstr = NULL;
	ad->LookupString( "Reason", &mallocstr );
	if( mallocstr ) {
		if( reason ) {
			delete [] reason;
		}
		reason = strnewp( mallocstr );
		free( mallocstr );
		mallocstr = NULL;
	}

	ad->LookupString( "StartdName", &mallocstr );
	if( mallocstr ) {
		if( startd_name ) {
			delete [] startd_name;
		}
		startd_name = strnewp( mallocstr );
		free( mallocstr );
		mallocstr = NULL;
	}
}


// ----- the GridResourceUp class
GridResourceUpEvent::GridResourceUpEvent(void)
{
	eventNumber = ULOG_GRID_RESOURCE_UP;
	resourceName = NULL;
}

GridResourceUpEvent::~GridResourceUpEvent(void)
{
	delete[] resourceName;
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

	if ( resourceName ) resource = resourceName;

	retval = formatstr_cat( out, "    GridResource: %.8191s\n", resource );
	if( retval < 0 ) {
		return false;
	}

	return true;
}

int
GridResourceUpEvent::readEvent (FILE *file, bool & got_sync_line)
{
	delete[] resourceName;
	resourceName = NULL;
#ifdef DONT_EVER_SEEK
	MyString line;
	if ( ! read_line_value("Grid Resource Back Up", line, file, got_sync_line)) {
		return 0;
	}
	if ( ! read_line_value("    GridResource: ", line, file, got_sync_line)) {
		return 0;
	}
	resourceName = line.detach_buffer();
#else
	char s[8192];

	int retval = fscanf (file, "Grid Resource Back Up\n");
    if (retval != 0)
    {
		return 0;
    }
	s[0] = '\0';
	retval = fscanf( file, "    GridResource: %8191[^\n]\n", s );
	if ( retval != 1 )
	{
		return 0;
	}
	resourceName = strnewp(s);
#endif
	return 1;
}

ClassAd*
GridResourceUpEvent::toClassAd(bool event_time_utc)
{
	ClassAd* myad = ULogEvent::toClassAd(event_time_utc);
	if( !myad ) return NULL;

	if( resourceName && resourceName[0] ) {
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

	// this fanagling is to ensure we don't malloc a pointer then delete it
	char* mallocstr = NULL;
	ad->LookupString("GridResource", &mallocstr);
	if( mallocstr ) {
		resourceName = new char[strlen(mallocstr) + 1];
		strcpy(resourceName, mallocstr);
		free(mallocstr);
	}
}


// ----- the GridResourceDown class
GridResourceDownEvent::GridResourceDownEvent(void)
{
	eventNumber = ULOG_GRID_RESOURCE_DOWN;
	resourceName = NULL;
}

GridResourceDownEvent::~GridResourceDownEvent(void)
{
	delete[] resourceName;
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

	if ( resourceName ) resource = resourceName;

	retval = formatstr_cat( out, "    GridResource: %.8191s\n", resource );
	if( retval < 0 ) {
		return false;
	}

	return true;
}

int
GridResourceDownEvent::readEvent (FILE *file, bool & got_sync_line)
{
	delete[] resourceName;
	resourceName = NULL;
#ifdef DONT_EVER_SEEK
	MyString line;
	if ( ! read_line_value("Detected Down Grid Resource", line, file, got_sync_line)) {
		return 0;
	}
	if ( ! read_line_value("    GridResource: ", line, file, got_sync_line)) {
		return 0;
	}
	resourceName = line.detach_buffer();
#else
	char s[8192];

	int retval = fscanf (file, "Detected Down Grid Resource\n");
    if (retval != 0)
    {
		return 0;
    }
	s[0] = '\0';
	retval = fscanf( file, "    GridResource: %8191[^\n]\n", s );
	if ( retval != 1 )
	{
		return 0;
	}
	resourceName = strnewp(s);
#endif
	return 1;
}

ClassAd*
GridResourceDownEvent::toClassAd(bool event_time_utc)
{
	ClassAd* myad = ULogEvent::toClassAd(event_time_utc);
	if( !myad ) return NULL;

	if( resourceName && resourceName[0] ) {
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

	// this fanagling is to ensure we don't malloc a pointer then delete it
	char* mallocstr = NULL;
	ad->LookupString("GridResource", &mallocstr);
	if( mallocstr ) {
		resourceName = new char[strlen(mallocstr) + 1];
		strcpy(resourceName, mallocstr);
		free(mallocstr);
	}
}


// ----- the GridSubmitEvent class
GridSubmitEvent::GridSubmitEvent(void)
{
	eventNumber = ULOG_GRID_SUBMIT;
	resourceName = NULL;
	jobId = NULL;
}

GridSubmitEvent::~GridSubmitEvent(void)
{
	delete[] resourceName;
	delete[] jobId;
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

	if ( resourceName ) resource = resourceName;
	if ( jobId ) job = jobId;

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
GridSubmitEvent::readEvent (FILE *file, bool & got_sync_line)
{
	delete[] resourceName;
	delete[] jobId;
	resourceName = NULL;
	jobId = NULL;
#ifdef DONT_EVER_SEEK
	MyString line;
	if ( ! read_line_value("Job submitted to grid resource", line, file, got_sync_line)) {
		return 0;
	}
	if ( ! read_line_value("    GridResource: ", line, file, got_sync_line)) {
		return 0;
	}
	resourceName = line.detach_buffer();

	if ( ! read_line_value("    GridJobId: ", line, file, got_sync_line)) {
		return 0;
	}
	jobId = line.detach_buffer();
#else
	char s[8192];

	int retval = fscanf (file, "Job submitted to grid resource\n");
    if (retval != 0)
    {
		return 0;
    }
	s[0] = '\0';
	retval = fscanf( file, "    GridResource: %8191[^\n]\n", s );
	if ( retval != 1 )
	{
		return 0;
	}
	resourceName = strnewp(s);
	retval = fscanf( file, "    GridJobId: %8191[^\n]\n", s );
	if ( retval != 1 )
	{
		return 0;
	}
	jobId = strnewp(s);
#endif
	return 1;
}

ClassAd*
GridSubmitEvent::toClassAd(bool event_time_utc)
{
	ClassAd* myad = ULogEvent::toClassAd(event_time_utc);
	if( !myad ) return NULL;

	if( resourceName && resourceName[0] ) {
		if( !myad->InsertAttr("GridResource", resourceName) ) {
			delete myad;
			return NULL;
		}
	}
	if( jobId && jobId[0] ) {
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

	// this fanagling is to ensure we don't malloc a pointer then delete it
	char* mallocstr = NULL;
	ad->LookupString("GridResource", &mallocstr);
	if( mallocstr ) {
		resourceName = new char[strlen(mallocstr) + 1];
		strcpy(resourceName, mallocstr);
		free(mallocstr);
	}

	// this fanagling is to ensure we don't malloc a pointer then delete it
	mallocstr = NULL;
	ad->LookupString("GridJobId", &mallocstr);
	if( mallocstr ) {
		jobId = new char[strlen(mallocstr) + 1];
		strcpy(jobId, mallocstr);
		free(mallocstr);
	}
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
JobAdInformationEvent::readEvent(FILE *file, bool & got_sync_line)
{
#ifdef DONT_EVER_SEEK
	MyString line;
	if ( ! read_line_value("Job ad information event triggered.", line, file, got_sync_line)) {
		return 0;
	}
	if ( jobad ) delete jobad;
	jobad = new ClassAd();

	int num_attrs = 0;
	while (read_optional_line(line, file, got_sync_line)) {
		if ( ! jobad->Insert(line.Value())) {
			// dprintf(D_ALWAYS,"failed to create classad; bad expr = '%s'\n", line.Value());
			return 0;
		}
		++num_attrs;
	}
	return num_attrs > 0;
#else
    int retval = 0;	// 0 == FALSE == failure
	int EndFlag, ErrorFlag, EmptyFlag;

	EndFlag =  ErrorFlag =  EmptyFlag = 0;

	if( fscanf(file, "Job ad information event triggered.") == EOF ) {
		return 0;
	}

	if ( jobad ) delete jobad;

	if( !( jobad=new ClassAd(file,"...", EndFlag, ErrorFlag, EmptyFlag) ) )
	{
		// Out of memory?!?!
		return 0;
	}

	// Backup to leave event delimiter unread go past \n too
	fseek( file, -4, SEEK_CUR );

	retval = ! (ErrorFlag || EmptyFlag);

	return retval;
#endif
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
JobAdInformationEvent::LookupString (const char *attributeName, char **value) const
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
JobAdInformationEvent::LookupFloat (const char *attributeName, float & value) const
{
	if ( !jobad ) return 0;		// 0 = failure

	return jobad->LookupFloat(attributeName,value);
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

int JobStatusUnknownEvent::readEvent (FILE *file, bool & got_sync_line)
{
#ifdef DONT_EVER_SEEK
	MyString line;
	if ( ! read_line_value("The job's remote status is unknown", line, file, got_sync_line)) {
		return 0;
	}
#else
	int retval = fscanf (file, "The job's remote status is unknown\n");
    if (retval != 0)
    {
		return 0;
    }
#endif
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

int JobStatusKnownEvent::readEvent (FILE *file, bool & got_sync_line)
{
#ifdef DONT_EVER_SEEK
	MyString line;
	if ( ! read_line_value("The job's remote status is known again", line, file, got_sync_line)) {
		return 0;
	}
#else
	int retval = fscanf (file, "The job's remote status is known again\n");
    if (retval != 0)
    {
		return 0;
    }
#endif
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
JobStageInEvent::readEvent (FILE *file, bool & got_sync_line)
{
#ifdef DONT_EVER_SEEK
	MyString line;
	if ( ! read_line_value("Job is performing stage-in of input files", line, file, got_sync_line)) {
		return 0;
	}
#else
	int retval = fscanf (file, "Job is performing stage-in of input files\n");
    if (retval != 0)
    {
		return 0;
    }
#endif
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
JobStageOutEvent::readEvent (FILE *file, bool & got_sync_line)
{
#ifdef DONT_EVER_SEEK
	MyString line;
	if ( ! read_line_value("Job is performing stage-out of output files", line, file, got_sync_line)) {
		return 0;
	}
#else
	int retval = fscanf (file, "Job is performing stage-out of output files\n");
    if (retval != 0)
    {
		return 0;
    }
#endif
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
AttributeUpdate::readEvent(FILE *file, bool & got_sync_line)
{
	char buf1[4096], buf2[4096], buf3[4096];
	buf1[0] = '\0';
	buf2[0] = '\0';
	buf3[0] = '\0';

	if (name) { free(name); }
	if (value) { free(value); }
	if (old_value) { free(old_value); }
	name = value = old_value = NULL;

#ifdef DONT_EVER_SEEK
	MyString line;
	if ( ! read_optional_line(line, file, got_sync_line)) {
		return 0;
	}

	int retval = sscanf(line.Value(), "Changing job attribute %s from %s to %s", buf1, buf2, buf3);
	if (retval < 0)
	{
		retval = sscanf(line.Value(), "Setting job attribute %s to %s", buf1, buf3);
		if (retval < 0)
		{
			return 0;
		}
	}
#else
	int retval;

	retval = fscanf(file, "Changing job attribute %s from %s to %s\n", buf1, buf2, buf3);
	if (retval < 0)
	{
		retval = fscanf(file, "Setting job attribute %s to %s\n", buf1, buf3);
		if (retval < 0)
		{
			return 0;
		}
	}
#endif
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
	MyString buf;
	ULogEvent::initFromClassAd(ad);

	if( !ad ) return;

	if( ad->LookupString("Attribute", buf ) ) {
		name = strdup(buf.Value());
	}
	if( ad->LookupString("Value", buf ) ) {
		value = strdup(buf.Value());
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


PreSkipEvent::PreSkipEvent(void) : skipEventLogNotes(0)
{
	eventNumber = ULOG_PRESKIP;
}

PreSkipEvent::~PreSkipEvent(void)
{
	delete [] skipEventLogNotes;
}

int PreSkipEvent::readEvent (FILE *file, bool & got_sync_line)
{
	delete[] skipEventLogNotes;
	skipEventLogNotes = NULL;
	MyString line;
#ifdef DONT_EVER_SEEK
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
	line.trim();
	skipEventLogNotes = line.detach_buffer();
#else
	if( !line.readLine(file) ) {
		return 0;
	}
	setSkipNote(line.Value()); // allocate memory

	// check if event ended without specifying the DAG node.
	// in this case, the submit host would be the event delimiter
	if (skipEventLogNotes && (MATCH == strncmp(skipEventLogNotes,"...",3))) {
			// This should not happen. The event should have a 
			// DAGMan node associated with it.
		skipEventLogNotes[0] = '\0';
		// Backup to leave event delimiter unread go past \n too
		fseek( file, -4, SEEK_CUR );
		return 0;
	}
	char s[8192];
	
	// This event must have a DAG Node attached to it.
	fpos_t fpos;
	fgetpos(file,&fpos);
	if(!fgets(s,8192,file) || strcmp( s,"...\n" ) == 0 ) {
		fsetpos(file,&fpos);
		return 0;
	}
	char* newline = strchr(s,'\n');
	if(newline) {
		*newline = '\0';
	}
		// some users of this library (dagman) depend on whitespace
		// being stripped from the beginning of the log notes field
	char const *strip_s = s;
	while( *strip_s && isspace(*strip_s) ) {
		strip_s++;
	}
	char *p = s;
		// Don't use strcpy because the strings "may not overlap"
		// according to strcpy(3) man page
	if (p != strip_s) {
		while((*p++ = *strip_s++)) {}
	}
	delete [] skipEventLogNotes;
	skipEventLogNotes = strnewp(s);
#endif
	return ( !skipEventLogNotes || strlen(skipEventLogNotes) == 0 )?0:1;
}

bool
PreSkipEvent::formatBody( std::string &out )
{
	int retval = formatstr_cat( out, "PRE script return value is PRE_SKIP value\n" );
		// 
	if (!skipEventLogNotes || retval < 0)
	{
		return false;
	}
	retval = formatstr_cat( out, "    %.8191s\n", skipEventLogNotes );
	if( retval < 0 ) {
		return false;
	}
	return true;
}

ClassAd* PreSkipEvent::toClassAd(bool event_time_utc)
{
	ClassAd* myad = ULogEvent::toClassAd(event_time_utc);
	if( !myad ) return NULL;

	if( skipEventLogNotes && skipEventLogNotes[0] ) {
		if( !myad->InsertAttr("SkipEventLogNotes",skipEventLogNotes) ) return NULL;
	}
	return myad;
}

void PreSkipEvent::initFromClassAd(ClassAd* ad)
{
	ULogEvent::initFromClassAd(ad);

	if( !ad ) return;
	char* mallocstr = NULL;
	ad->LookupString("SkipEventLogNotes", &mallocstr);
	if( mallocstr ) {
		setSkipNote(mallocstr);
		free(mallocstr);
		mallocstr = NULL;
	}
}

void PreSkipEvent::setSkipNote(const char* s)
{
	if( skipEventLogNotes ) {
		delete[] skipEventLogNotes;
	}
	if( s ) {
		skipEventLogNotes = strnewp(s);
		ASSERT( skipEventLogNotes );
	}
	else {
		skipEventLogNotes = NULL;
	}
}

// ----- the ClusterSubmitEvent class
ClusterSubmitEvent::ClusterSubmitEvent(void)
{
	submitEventLogNotes = NULL;
	submitEventUserNotes = NULL;
	submitHost = NULL;
	eventNumber = ULOG_CLUSTER_SUBMIT;
}

ClusterSubmitEvent::~ClusterSubmitEvent(void)
{
	if( submitHost ) {
		delete[] submitHost;
	}
	if( submitEventLogNotes ) {
		delete[] submitEventLogNotes;
	}
	if( submitEventUserNotes ) {
		delete[] submitEventUserNotes;
	}
}

void
ClusterSubmitEvent::setSubmitHost(char const *addr)
{
	if( submitHost ) {
		delete[] submitHost;
	}
	if( addr ) {
		submitHost = strnewp(addr);
		ASSERT( submitHost );
	}
	else {
		submitHost = NULL;
	}
}

bool
ClusterSubmitEvent::formatBody( std::string &out )
{
	int retval = formatstr_cat (out, "Cluster submitted from host: %s\n", submitHost);
	if (retval < 0)
	{
		return false;
	}
	if( submitEventLogNotes ) {
		retval = formatstr_cat( out, "    %.8191s\n", submitEventLogNotes );
		if( retval < 0 ) {
			return false;
		}
	}
	if( submitEventUserNotes ) {
		retval = formatstr_cat( out, "    %.8191s\n", submitEventUserNotes );
		if( retval < 0 ) {
			return false;
		}
	}
	return true;
}

int
ClusterSubmitEvent::readEvent (FILE *file, bool & got_sync_line)
{
	delete[] submitHost;
	submitHost = NULL;
	delete[] submitEventLogNotes;
	submitEventLogNotes = NULL;
#ifdef DONT_EVER_SEEK
	MyString line;
	if ( ! read_line_value("Cluster submitted from host: ", line, file, got_sync_line)) {
		return 0;
	}
	submitHost = line.detach_buffer();

	// see if the next line contains an optional event notes string,
	if ( ! read_optional_line(line, file, got_sync_line)) {
		return 1;
	}
		// some users of this library (dagman) depend on whitespace
		// being stripped from the beginning of the log notes field
	line.trim();
	submitEventLogNotes = line.detach_buffer();

	// see if the next line contains an optional user event notes
	if ( ! read_optional_line(line, file, got_sync_line)) {
		return 1;
	}
	line.trim();
	submitEventUserNotes = line.detach_buffer();
#else

	char s[8192];
	s[0] = '\0';
	MyString line;
	if( !line.readLine(file) ) {
		return 0;
	}
	setSubmitHost(line.Value()); // allocate memory
	if( sscanf( line.Value(), "Cluster submitted from host: %s\n", submitHost ) != 1 ) {
		return 0;
	}

	// check if event ended without specifying submit host.
	// in this case, the submit host would be the event delimiter
	if ( strncmp(submitHost,"...",3)==0 ) {
		submitHost[0] = '\0';
		// Backup to leave event delimiter unread go past \n too
		fseek( file, -4, SEEK_CUR );
		return 1;
	}

	// see if the next line contains an optional event notes string,
	// and, if not, rewind, because that means we slurped in the next
	// event delimiter looking for it...

	fpos_t filep;
	fgetpos( file, &filep );

	if( !fgets( s, 8192, file ) || strcmp( s, "...\n" ) == 0 ) {
		fsetpos( file, &filep );
		return 1;
	}

	// remove trailing newline
	s[ strlen( s ) - 1 ] = '\0';
	
		// some users of this library (dagman) depend on whitespace
		// being stripped from the beginning of the log notes field
	char const *strip_s = s;
	while( *strip_s && isspace(*strip_s) ) {
		strip_s++;
	}

	submitEventLogNotes = strnewp( strip_s );

	// see if the next line contains an optional user event notes
	// string, and, if not, rewind, because that means we slurped in
	// the next event delimiter looking for it...

	fgetpos( file, &filep );

	if( !fgets( s, 8192, file ) || strcmp( s, "...\n" ) == 0 ) {
		fsetpos( file, &filep );
		return 1;
	}

	// remove trailing newline
	s[ strlen( s ) - 1 ] = '\0';
	submitEventUserNotes = strnewp( s );
#endif
	return 1;
}

ClassAd*
ClusterSubmitEvent::toClassAd(bool event_time_utc)
{
	ClassAd* myad = ULogEvent::toClassAd(event_time_utc);
	if( !myad ) return NULL;

	if( submitHost && submitHost[0] ) {
		if( !myad->InsertAttr("SubmitHost",submitHost) ) return NULL;
	}

	return myad;
}


void
ClusterSubmitEvent::initFromClassAd(ClassAd* ad)
{
	ULogEvent::initFromClassAd(ad);

	if( !ad ) return;
	char* mallocstr = NULL;
	ad->LookupString("SubmitHost", &mallocstr);
	if( mallocstr ) {
		setSubmitHost(mallocstr);
		free(mallocstr);
		mallocstr = NULL;
	}
}

// ----- the ClusterRemoveEvent class
ClusterRemoveEvent::ClusterRemoveEvent(void)
	: next_proc_id(0), next_row(0), completion(Incomplete), notes(NULL)
{
	eventNumber = ULOG_CLUSTER_REMOVE;
}

ClusterRemoveEvent::~ClusterRemoveEvent(void)
{
	if (notes) { free(notes); } notes = NULL;
}

// read until \n into the supplied buffer
// if got a record terminator or EOF, then rewind the file position and return false
// otherwise return true.
#ifdef DONT_EVER_SEEK
#else
static bool read_line_or_rewind(FILE *file, char *buf, int bufsiz) {
	memset(buf, 0, bufsiz);
	if (feof(file)) return false;
	fpos_t filep;
	fgetpos( file, &filep );
	if( !fgets( buf, bufsiz, file ) || strcmp( buf, "...\n" ) == 0 ) {
		fsetpos( file, &filep );
		return false;
	}
	return true;
}
#endif

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
	if (notes) { formatstr_cat(out, "\t%s\n", notes); }
	return true;
}


int
ClusterRemoveEvent::readEvent (FILE *file, bool & got_sync_line)
{
	if( !file ) {
		return 0;
	}

	next_proc_id = next_row = 0;
	completion = Incomplete;
	if (notes) { free(notes); } notes = NULL;

	// get the remainder of the first line (if any)
	// or rewind so we don't slurp up the next event delimiter
	char buf[BUFSIZ];
#ifdef DONT_EVER_SEEK
	if ( ! read_optional_line(file, got_sync_line, buf, sizeof(buf))) {
		return 1; // backwards compatibility
	}
#else
	if ( ! read_line_or_rewind(file, buf, sizeof(buf)) ) {
		return 1; // backwards compatibility
	}
#endif

	// be intentionally forgiving about the text here, so as not to introduce bugs if we change terminology
	if (strstr(buf, "remove") || strstr(buf,"Remove")) {
#ifdef DONT_EVER_SEEK
		if ( ! read_optional_line(file, got_sync_line, buf, sizeof(buf))) {
			return 1; // this field is optional
		}
#else
		// got the "Cluster removed" line, now get the next line.
		if ( ! read_line_or_rewind(file, buf, sizeof(buf)) ) { 
			return 1; // this field is optional
		}
#endif
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
#ifdef DONT_EVER_SEEK
	if ( ! read_optional_line(file, got_sync_line, buf, sizeof(buf))) {
		return 1; // this field is optional
	}
#else
	if ( ! read_line_or_rewind(file, buf, sizeof(buf)) ) { 
		return 1; // notes field is optional
	}
#endif

	chomp(buf);  // strip the newline
	p = buf;
	// discard leading spaces, and store the result as the notes
	while (isspace(*p)) ++p;
	if (*p) { notes = strdup(p); }

	return 1;
}

ClassAd*
ClusterRemoveEvent::toClassAd(bool event_time_utc)
{
	ClassAd* myad = ULogEvent::toClassAd(event_time_utc);
	if( !myad ) return NULL;

	if (notes) {
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
	if (notes) { free(notes); } notes = NULL;

	ULogEvent::initFromClassAd(ad);

	if( !ad ) return;

	int code = Incomplete;
	ad->LookupInteger("Completion", code);
	completion = (CompletionCode)code;

	ad->LookupInteger("NextProcId", next_proc_id);
	ad->LookupInteger("NextRow", next_row);

	ad->LookupString("Notes", &notes);
}

// ----- the FactoryPausedEvent class


#define FACTORY_PAUSED_BANNER "Job Materialization Paused"
#define FACTORY_RESUMED_BANNER "Job Materialization Resumed"

int
FactoryPausedEvent::readEvent (FILE *file, bool & got_sync_line)
{
	if( !file ) {
		return 0;
	}

	pause_code = 0;
	if (reason) { free(reason); }
	reason = NULL;

	char buf[BUFSIZ];
#ifdef DONT_EVER_SEEK
	if ( ! read_optional_line(file, got_sync_line, buf, sizeof(buf))) {
		return 1; // backwards compatibility
	}
#else
	if ( ! read_line_or_rewind(file, buf, sizeof(buf)) ) {
		return 1; // backwards compatibility
	}
#endif

	// be intentionally forgiving about the text here, so as not to introduce bugs if we change terminology
	if (strstr(buf, "pause") || strstr(buf,"Pause")) {
		// got the "Paused" line, now get the next line.
#ifdef DONT_EVER_SEEK
		if ( ! read_optional_line(file, got_sync_line, buf, sizeof(buf))) {
			return 1; // this field is optional
		}
#else
		if ( ! read_line_or_rewind(file, buf, sizeof(buf)) ) { 
			return 1; // this field is optional
		}
#endif
	}

	// The next line should be the pause reason.
	chomp(buf);  // strip the newline
	const char * p = buf;
	// discard leading spaces, and store the result as the reason
	while (isspace(*p)) ++p;
	if (*p) { reason = strdup(p); }

	// read the pause code and/or hold code, if they exist
#ifdef DONT_EVER_SEEK
	while ( read_optional_line(file, got_sync_line, buf, sizeof(buf)) )
#else
	while ( read_line_or_rewind(file, buf, sizeof(buf)) )
#endif
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
	if (reason || pause_code != 0) {
		formatstr_cat(out, "\t%s\n", reason ? reason : "");
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

	if (reason) {
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
	if (reason) { free(reason); } reason = NULL;
	ULogEvent::initFromClassAd(ad);

	if( !ad ) return;

	ad->LookupString("Reason", &reason);
	ad->LookupInteger("PauseCode", pause_code);
	ad->LookupInteger("HoldCode", hold_code);
}

void FactoryPausedEvent::setReason(const char* str)
{
	if (reason) { free(reason); } reason = NULL;
	if (str) reason = strdup(str);
}

// ----- the FactoryResumedEvent class

bool
FactoryResumedEvent::formatBody( std::string &out )
{
	out += FACTORY_RESUMED_BANNER "\n";
	if (reason) {
		formatstr_cat(out, "\t%s\n", reason);
	}
	return true;
}

int
FactoryResumedEvent::readEvent (FILE *file, bool & got_sync_line)
{
	if( !file ) {
		return 0;
	}

	if (reason) { free(reason); }
	reason = NULL;

	char buf[BUFSIZ];
#ifdef DONT_EVER_SEEK
	if ( ! read_optional_line(file, got_sync_line, buf, sizeof(buf))) {
		return 1; // backwards compatibility
	}
#else
	if ( ! read_line_or_rewind(file, buf, sizeof(buf)) ) {
		return 1; // backwards compatibility
	}
#endif

	// be intentionally forgiving about the text here, so as not to introduce bugs if we change terminology
	if (strstr(buf, "resume") || strstr(buf,"Resume")) {
		// got the "Resumed" line, now get the next line.
#ifdef DONT_EVER_SEEK
		if ( ! read_optional_line(file, got_sync_line, buf, sizeof(buf))) {
			return 1; // this field is optional
		}
#else
		if ( ! read_line_or_rewind(file, buf, sizeof(buf)) ) { 
			return 1; // this field is optional
		}
#endif
	}

	// The next line should be the resume reason.
	chomp(buf);  // strip the newline
	const char * p = buf;
	// discard leading spaces, and store the result as the reason
	while (isspace(*p)) ++p;
	if (*p) { reason = strdup(p); }

	return 1;
}

ClassAd*
FactoryResumedEvent::toClassAd(bool event_time_utc)
{
	ClassAd* myad = ULogEvent::toClassAd(event_time_utc);
	if( !myad ) return NULL;

	if (reason) {
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
	if (reason) { free(reason); } reason = NULL;
	ULogEvent::initFromClassAd(ad);

	if( !ad ) return;

	ad->LookupString("Reason", &reason);
}

void FactoryResumedEvent::setReason(const char* str)
{
	if (reason) { free(reason); } reason = NULL;
	if (str) reason = strdup(str);
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
FileTransferEvent::readEvent( FILE * f, bool & got_sync_line ) {
	// Require an 'optional' line  because read_line_value() requires a prefix.
	MyString eventString;
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
	MyString optionalLine;
	if(! read_optional_line( optionalLine, f, got_sync_line )) {
		return got_sync_line ? 1 : 0;
	}
	optionalLine.chomp();

	// Did we record the queueing delay?
	MyString prefix = "\tSeconds spent in queue: ";
	if( starts_with( optionalLine.c_str(), prefix.c_str() ) ) {
		MyString value = optionalLine.substr( prefix.Length(), optionalLine.Length() );

		char * endptr = NULL;
		queueingDelay = strtol( value.c_str(), & endptr, 10 );
		if( endptr == NULL || endptr[0] != '\0' ) {
			return 0;
		}

		// If we read an optional line, check for the next one.
		if(! read_optional_line( optionalLine, f, got_sync_line )) {
			return got_sync_line ? 1 : 0;
		}
		optionalLine.chomp();
	}


	// Did we record the starter host?
	prefix = "\tTransferring to host: ";
	if( starts_with( optionalLine.c_str(), prefix.c_str() ) ) {
		host = optionalLine.substr( prefix.Length(), optionalLine.Length() );

/*
		// If we read an optional line, check for the next one.
		if(! read_optional_line( optionalLine, f, got_sync_line )) {
			return got_sync_line ? 1 : 0;
		}
		optionalLine.chomp();
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
