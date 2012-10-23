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

//added by Ameet
#include "condor_environ.h"
//--------------------------------------------------------
#include "condor_debug.h"
//--------------------------------------------------------


#define ESCAPE { errorNumber=(errno==EAGAIN) ? ULOG_NO_EVENT : ULOG_UNK_ERROR;\
					 return 0; }

#include "file_sql.h"
extern FILESQL *FILEObj;


//extern ClassAd *JobAd;

const char ULogEventNumberNames[][30] = {
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
	"ULOG_ATTRIBUTE_UPDATE"			// Job attribute updated
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

	default:
		dprintf( D_ALWAYS, "Invalid ULogEventNumber: %d\n", event );
		// Return NULL/0 here instead of EXCEPTing to fix Gnats PR 706.
		// wenger 2006-06-08.
		return 0;
	}

    return 0;
}


ULogEvent::ULogEvent(void)
{
	struct tm *tm;

	eventNumber = (ULogEventNumber) - 1;
	cluster = proc = subproc = -1;

	(void) time ((time_t *)&eventclock);
	tm = localtime ((time_t *)&eventclock);
	eventTime = *tm;
	scheddname = NULL;
	m_gjid = NULL;
}


ULogEvent::~ULogEvent (void)
{
}


int ULogEvent::getEvent (FILE *file)
{
	if( !file ) {
		dprintf( D_ALWAYS, "ERROR: file == NULL in ULogEvent::getEvent()\n" );
		return 0;
	}
	return (readHeader (file) && readEvent (file));
}


int ULogEvent::putEvent (FILE *file)
{
	if( !file ) {
		dprintf( D_ALWAYS, "ERROR: file == NULL in ULogEvent::putEvent()\n" );
		return 0;
	}
	return (writeHeader (file) && writeEvent (file));
}


const char* ULogEvent::eventName(void) const
{
	if( eventNumber == (ULogEventNumber)-1 ) {
		return NULL;
	}
	return ULogEventNumberNames[eventNumber];
}


// This function reads in the header of an event from the UserLog and fills
// the fields of the event object.  It does *not* read the event number.  The
// caller is supposed to read the event number, instantiate the appropriate
// event type using instantiateEvent(), and then call readEvent() of the
// returned event.
int
ULogEvent::readHeader (FILE *file)
{
	int retval;

	// read from file
	retval = fscanf (file, " (%d.%d.%d) %d/%d %d:%d:%d ",
					 &cluster, &proc, &subproc,
					 &(eventTime.tm_mon), &(eventTime.tm_mday),
					 &(eventTime.tm_hour), &(eventTime.tm_min),
					 &(eventTime.tm_sec));

	// check if all fields were successfully read
	if (retval != 8)
	{
		return 0;
	}

	// recall that tm_mon+1 was written to log; decrement to compensate
	eventTime.tm_mon--;

	return 1;
}


// Write the header for the event to the file
int
ULogEvent::writeHeader (FILE *file)
{
	int       retval;

	// write header
	retval = fprintf (file, "%03d (%03d.%03d.%03d) %02d/%02d %02d:%02d:%02d ",
					  eventNumber,
					  cluster, proc, subproc,
					  eventTime.tm_mon+1, eventTime.tm_mday,
					  eventTime.tm_hour, eventTime.tm_min, eventTime.tm_sec);

	// check if all fields were sucessfully written
	if (retval < 0)
	{
		return 0;
	}

	return 1;
}

ClassAd*
ULogEvent::toClassAd(void)
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
		myad->SetMyTypeName("SubmitEvent");
		break;
	  case ULOG_EXECUTE:
		myad->SetMyTypeName("ExecuteEvent");
		break;
	  case ULOG_EXECUTABLE_ERROR:
		myad->SetMyTypeName("ExecutableErrorEvent");
		break;
	  case ULOG_CHECKPOINTED:
		myad->SetMyTypeName("CheckpointedEvent");
		break;
	  case ULOG_JOB_EVICTED:
		myad->SetMyTypeName("JobEvictedEvent");
		break;
	  case ULOG_JOB_TERMINATED:
		myad->SetMyTypeName("JobTerminatedEvent");
		break;
	  case ULOG_IMAGE_SIZE:
		myad->SetMyTypeName("JobImageSizeEvent");
		break;
	  case ULOG_SHADOW_EXCEPTION:
		myad->SetMyTypeName("ShadowExceptionEvent");
		break;
	  case ULOG_GENERIC:
		myad->SetMyTypeName("GenericEvent");
		break;
	  case ULOG_JOB_ABORTED:
		myad->SetMyTypeName("JobAbortedEvent");
		break;
	  case ULOG_JOB_SUSPENDED:
		myad->SetMyTypeName("JobSuspendedEvent");
		break;
	  case ULOG_JOB_UNSUSPENDED:
		myad->SetMyTypeName("JobUnsuspendedEvent");
		break;
	  case ULOG_JOB_HELD:
		myad->SetMyTypeName("JobHeldEvent");
		break;
	  case ULOG_JOB_RELEASED:
		myad->SetMyTypeName("JobReleaseEvent");
		break;
	  case ULOG_NODE_EXECUTE:
		myad->SetMyTypeName("NodeExecuteEvent");
		break;
	  case ULOG_NODE_TERMINATED:
		myad->SetMyTypeName("NodeTerminatedEvent");
		break;
	  case ULOG_POST_SCRIPT_TERMINATED:
		myad->SetMyTypeName("PostScriptTerminatedEvent");
		break;
	  case ULOG_GLOBUS_SUBMIT:
		myad->SetMyTypeName("GlobusSubmitEvent");
		break;
	  case ULOG_GLOBUS_SUBMIT_FAILED:
		myad->SetMyTypeName("GlobusSubmitFailedEvent");
		break;
	  case ULOG_GLOBUS_RESOURCE_UP:
		myad->SetMyTypeName("GlobusResourceUpEvent");
		break;
	  case ULOG_GLOBUS_RESOURCE_DOWN:
		myad->SetMyTypeName("GlobusResourceDownEvent");
		break;
	case ULOG_REMOTE_ERROR:
		myad->SetMyTypeName("RemoteErrorEvent");
		break;
	case ULOG_JOB_DISCONNECTED:
		myad->SetMyTypeName("JobDisconnectedEvent");
		break;
	case ULOG_JOB_RECONNECTED:
		myad->SetMyTypeName("JobReconnectedEvent");
		break;
	case ULOG_JOB_RECONNECT_FAILED:
		myad->SetMyTypeName("JobReconnectFailedEvent");
		break;
	case ULOG_GRID_RESOURCE_UP:
		myad->SetMyTypeName("GridResourceUpEvent");
		break;
	case ULOG_GRID_RESOURCE_DOWN:
		myad->SetMyTypeName("GridResourceDownEvent");
		break;
	case ULOG_GRID_SUBMIT:
		myad->SetMyTypeName("GridSubmitEvent");
		break;
	case ULOG_JOB_AD_INFORMATION:
		myad->SetMyTypeName("JobAdInformationEvent");
		break;
	case ULOG_ATTRIBUTE_UPDATE:
		myad->SetMyTypeName("AttributeUpdateEvent");
		break;
	  default:
		delete myad;
		return NULL;
	}

	const struct tm tmdup = eventTime;
	char* eventTimeStr = time_to_iso8601(tmdup, ISO8601_ExtendedFormat,
										 ISO8601_DateAndTime, FALSE);
	if( eventTimeStr ) {
		if ( !myad->InsertAttr("EventTime", eventTimeStr) ) {
			delete myad;
			return NULL;
		}
	} else {
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
		bool f = FALSE;
		iso8601_to_time(timestr, &eventTime, &f);
		free(timestr);
	}
	ad->LookupInteger("Cluster", cluster);
	ad->LookupInteger("Proc", proc);
	ad->LookupInteger("Subproc", subproc);
}

void
ULogEvent::insertCommonIdentifiers(ClassAd &adToFill)
{
	//if( !adToFill ) return;
	if(scheddname) {
	  adToFill.Assign("scheddname", scheddname);
	}

	if(m_gjid) {
	  adToFill.Assign("globaljobid", m_gjid);
	}

	adToFill.Assign("cluster_id", cluster);
	adToFill.Assign("proc_id", proc);
	adToFill.Assign("spid", subproc);
}


// class is used to build up Usage/Request/Allocated table for each 
// Partitionable resource before we print out the table.
class  SlotResTermSumy {
public:
	std::string use;
	std::string req;
	std::string alloc;
};

// class to write the usage ClassAd to the userlog
// The usage ClassAd should contain attrbutes that match the pattern
// "<RES>", "Request<RES>", or "<RES>Usage", where <RES> can be
// Cpus, Disk, Memory, or others as defined for use by the ProvisionedResources
// attribute.
//
static void writeUsageAd(FILE * file, ClassAd * pusageAd)
{
	if ( ! pusageAd)
		return;

	classad::ClassAdUnParser unp;
	unp.SetOldClassAd( true );

	std::map<std::string, SlotResTermSumy*> useMap;

	for (classad::ClassAd::iterator iter = pusageAd->begin();
		 iter != pusageAd->end();
		 iter++) {
		int ixu = (int)iter->first.size() - 5; // size "Usage" == 5
		std::string key = "";
		int efld = -1;
		if (0 == iter->first.find("Request")) {
			key = iter->first.substr(7); // size "Request" == 7
			efld = 1;
		} else if (ixu > 0 && 0 == iter->first.substr(ixu).compare("Usage")) {
			efld = 0;
			key = iter->first.substr(0,ixu);
		} 
		else /*if (useMap[iter->first])*/ { // Allocated
			efld = 2;
			key = iter->first;
		}
		if (key.size() != 0) {
			SlotResTermSumy * psumy = useMap[key];
			if ( ! psumy) {
				psumy = new SlotResTermSumy();
				useMap[key] = psumy;
				//fprintf(file, "\tadded %x for key %s\n", psumy, key.c_str());
			} else {
				//fprintf(file, "\tfound %x for key %s\n", psumy, key.c_str());
			}
			std::string val = "";
			unp.Unparse(val, iter->second);

			//fprintf(file, "\t%-8s \t= %4s\t(efld%d, key = %s)\n", iter->first.c_str(), val.c_str(), efld, key.c_str());

			switch (efld)
			{
				case 0: // Usage
					psumy->use = val;
					break;
				case 1: // Request
					psumy->req = val;
					break;
				case 2:	// Allocated
					psumy->alloc = val;
					break;
			}
		} else {
			std::string val = "";
			unp.Unparse(val, iter->second);
			fprintf(file, "\t%s = %s\n", iter->first.c_str(), val.c_str());
		}
	}
	if (useMap.empty())
		return;

	int cchRes = sizeof("Memory (MB)"), cchUse = 8, cchReq = 8, cchAlloc = 0;
	for (std::map<std::string, SlotResTermSumy*>::iterator it = useMap.begin();
		 it != useMap.end();
		 ++it) {
		SlotResTermSumy * psumy = it->second;
		if ( ! psumy->alloc.size()) {
			classad::ExprTree * tree = pusageAd->Lookup(it->first);
			if (tree) {
				unp.Unparse(psumy->alloc, tree);
			}
		}
		//fprintf(file, "\t%s %s %s %s\n", it->first.c_str(), psumy->use.c_str(), psumy->req.c_str(), psumy->alloc.c_str());
		cchRes = MAX(cchRes, (int)it->first.size());
		cchUse = MAX(cchUse, (int)psumy->use.size());
		cchReq = MAX(cchReq, (int)psumy->req.size());
		cchAlloc = MAX(cchAlloc, (int)psumy->alloc.size());
	}

	MyString fmt;
	fmt.formatstr("\tPartitionable Resources : %%%ds %%%ds %%%ds\n", cchUse, cchReq, MAX(cchAlloc,9));
	fprintf(file, fmt.Value(), "Usage", "Request", cchAlloc ? "Allocated" : "");
	fmt.formatstr("\t   %%-%ds : %%%ds %%%ds %%%ds\n", cchRes+8, cchUse, cchReq, MAX(cchAlloc,9));
	//fputs(fmt.Value(), file);
	for (std::map<std::string, SlotResTermSumy*>::iterator it = useMap.begin();
		 it != useMap.end();
		 ++it) {
		SlotResTermSumy * psumy = it->second;
		std::string lbl = it->first.c_str(); 
		if (lbl.compare("Memory") == 0) lbl += " (MB)";
		else if (lbl.compare("Disk") == 0) lbl += " (KB)";
		fprintf(file, fmt.Value(), lbl.c_str(), psumy->use.c_str(), psumy->req.c_str(), psumy->alloc.c_str());
	}
	//fprintf(file, "\t  *See Section %d.%d in the manual for information about requesting resources\n", 2, 5);
}

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
				while (*psz && *psz != ' ') ++psz; // skip "Allocated"
				ixAlloc = (int)(psz - pszTbl)+1;
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
		}
	}

	*ppusageAd = puAd;
}


// ----- the SubmitEvent class
SubmitEvent::SubmitEvent(void)
{
	submitHost = NULL;
	submitEventLogNotes = NULL;
	submitEventUserNotes = NULL;
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

int
SubmitEvent::writeEvent (FILE *file)
{
	if( !submitHost ) {
		setSubmitHost("");
	}
	int retval = fprintf (file, "Job submitted from host: %s\n", submitHost);
	if (retval < 0)
	{
		return 0;
	}
	if( submitEventLogNotes ) {
		retval = fprintf( file, "    %.8191s\n", submitEventLogNotes );
		if( retval < 0 ) {
			return 0;
		}
	}
	if( submitEventUserNotes ) {
		retval = fprintf( file, "    %.8191s\n", submitEventUserNotes );
		if( retval < 0 ) {
			return 0;
		}
	}
	return (1);
}

int
SubmitEvent::readEvent (FILE *file)
{
	char s[8192];
	s[0] = '\0';
	delete[] submitEventLogNotes;
	submitEventLogNotes = NULL;
	MyString line;
	if( !line.readLine(file) ) {
		return 0;
	}
	setSubmitHost(line.Value()); // allocate memory
	if( sscanf( line.Value(), "Job submitted from host: %s\n", submitHost ) != 1 ) {
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
	return 1;
}

ClassAd*
SubmitEvent::toClassAd(void)
{
	ClassAd* myad = ULogEvent::toClassAd();
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

int GlobusSubmitEvent::writeEvent (FILE *file)
{
	const char * unknown = "UNKNOWN";
	const char * rm = unknown;
	const char * jm = unknown;

	int retval = fprintf (file, "Job submitted to Globus\n");
	if (retval < 0)
	{
		return 0;
	}

	if ( rmContact ) rm = rmContact;
	if ( jmContact ) jm = jmContact;

	retval = fprintf( file, "    RM-Contact: %.8191s\n", rm );
	if( retval < 0 ) {
		return 0;
	}

	retval = fprintf( file, "    JM-Contact: %.8191s\n", jm );
	if( retval < 0 ) {
		return 0;
	}

	int newjm = 0;
	if ( restartableJM ) {
		newjm = 1;
	}
	retval = fprintf( file, "    Can-Restart-JM: %d\n", newjm );
	if( retval < 0 ) {
		return 0;
	}

	return (1);
}

int GlobusSubmitEvent::readEvent (FILE *file)
{
	char s[8192];

	delete[] rmContact;
	delete[] jmContact;
	rmContact = NULL;
	jmContact = NULL;
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

	int newjm = 0;
	retval = fscanf( file, "    Can-Restart-JM: %d\n", &newjm );
	if ( retval != 1 )
	{
		return 0;
	}
	if ( newjm ) {
		restartableJM = true;
	} else {
		restartableJM = false;
	}

	return 1;
}

ClassAd*
GlobusSubmitEvent::toClassAd(void)
{
	ClassAd* myad = ULogEvent::toClassAd();
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

int
GlobusSubmitFailedEvent::writeEvent (FILE *file)
{
	const char * unknown = "UNKNOWN";
	const char * reasonString = unknown;

	int retval = fprintf (file, "Globus job submission failed!\n");
	if (retval < 0)
	{
		return 0;
	}

	if ( reason ) reasonString = reason;

	retval = fprintf( file, "    Reason: %.8191s\n", reasonString );
	if( retval < 0 ) {
		return 0;
	}

	return (1);
}

int
GlobusSubmitFailedEvent::readEvent (FILE *file)
{
	char s[8192];

	delete[] reason;
	reason = NULL;
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
	return 1;
}

ClassAd*
GlobusSubmitFailedEvent::toClassAd(void)
{
	ClassAd* myad = ULogEvent::toClassAd();
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

int
GlobusResourceUpEvent::writeEvent (FILE *file)
{
	const char * unknown = "UNKNOWN";
	const char * rm = unknown;

	int retval = fprintf (file, "Globus Resource Back Up\n");
	if (retval < 0)
	{
		return 0;
	}

	if ( rmContact ) rm = rmContact;

	retval = fprintf( file, "    RM-Contact: %.8191s\n", rm );
	if( retval < 0 ) {
		return 0;
	}

	return (1);
}

int
GlobusResourceUpEvent::readEvent (FILE *file)
{
	char s[8192];

	delete[] rmContact;
	rmContact = NULL;
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
	return 1;
}

ClassAd*
GlobusResourceUpEvent::toClassAd(void)
{
	ClassAd* myad = ULogEvent::toClassAd();
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

int
GlobusResourceDownEvent::writeEvent (FILE *file)
{
	const char * unknown = "UNKNOWN";
	const char * rm = unknown;

	int retval = fprintf (file, "Detected Down Globus Resource\n");
	if (retval < 0)
	{
		return 0;
	}

	if ( rmContact ) rm = rmContact;

	retval = fprintf( file, "    RM-Contact: %.8191s\n", rm );
	if( retval < 0 ) {
		return 0;
	}

	return (1);
}

int
GlobusResourceDownEvent::readEvent (FILE *file)
{
	char s[8192];

	delete[] rmContact;
	rmContact = NULL;
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
	return 1;
}

ClassAd*
GlobusResourceDownEvent::toClassAd(void)
{
	ClassAd* myad = ULogEvent::toClassAd();
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

int
GenericEvent::writeEvent(FILE *file)
{
    int retval = fprintf(file, "%s\n", info);
    if (retval < 0)
    {
		return 0;
    }

    return 1;
}

int
GenericEvent::readEvent(FILE *file)
{
    int retval = fscanf(file, "%[^\n]\n", info);
    if (retval < 0)
    {
	return 0;
    }
    return 1;
}

ClassAd*
GenericEvent::toClassAd(void)
{
	ClassAd* myad = ULogEvent::toClassAd();
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

int
RemoteErrorEvent::writeEvent(FILE *file)
{
	char const *error_type = "Error";
	char messagestr[512];

	ClassAd tmpCl1, tmpCl2;
	//ClassAd *tmpClP1 = &tmpCl1, *tmpClP2 = &tmpCl2;
	int retval;

	snprintf(messagestr, 512, "Remote %s from %s on %s",
			error_type,
			daemon_name,
			execute_host);

	scheddname = getenv( EnvGetName( ENV_SCHEDD_NAME ) );

	if(!critical_error) error_type = "Warning";

	if (critical_error) {
		tmpCl1.Assign("endts", (int)eventclock);
		tmpCl1.Assign("endtype", ULOG_REMOTE_ERROR);
		tmpCl1.Assign("endmessage", messagestr);

		// this inserts scheddname, cluster, proc, etc
		insertCommonIdentifiers(tmpCl2);

		MyString tmp;
		tmp.formatstr("endtype = null");
		tmpCl2.Insert(tmp.Value());

			// critical error means this run is ended.
			// condor_event.o is part of cplus_lib.a, which may be linked by
			// non-daemons who wouldn't have initialized FILEObj. We don't
			// need to log events for non-daemons.
		if (FILEObj) {
			if (FILEObj->file_updateEvent("Runs", &tmpCl1, &tmpCl2)
				== QUILL_FAILURE) {
				dprintf(D_ALWAYS, "Logging Event 5--- Error\n");
				return 0; // return a error code, 0
			}
		}

	} else {
		        // this inserts scheddname, cluster, proc, etc
        insertCommonIdentifiers(tmpCl1);

		tmpCl1.Assign("eventtype", ULOG_REMOTE_ERROR);
		tmpCl1.Assign("eventtime", (int)eventclock);
		tmpCl1.Assign("description", messagestr);

		if (FILEObj) {
			if (FILEObj->file_newEvent("Events", &tmpCl1) == QUILL_FAILURE) {
				dprintf(D_ALWAYS, "Logging Event 5--- Error\n");
				return 0; // return a error code, 0
			}
		}
	}

    retval = fprintf(
	  file,
	  "%s from %s on %s:\n",
	  error_type,
	  daemon_name,
	  execute_host);



    if (retval < 0)
    {
	return 0;
    }

	//output each line of error_str, indented by one tab
	char *line = error_str;
	if(line)
	while(*line) {
		char *next_line = strchr(line,'\n');
		if(next_line) *next_line = '\0';

		retval = fprintf(file,"\t%s\n",line);
		if(retval < 0) return 0;

		if(!next_line) break;
		*next_line = '\n';
		line = next_line+1;
	}

	if (hold_reason_code) {
		fprintf(file,"\tCode %d Subcode %d\n",
		        hold_reason_code, hold_reason_subcode);
	}

    return 1;
}

int
RemoteErrorEvent::readEvent(FILE *file)
{
	char line[8192];
	char error_type[128];
    int retval = fscanf(
	  file,
	  "%127s from %127s on %127s\n",
	  error_type,
	  daemon_name,
	  execute_host);

    if (retval < 0)
    {
	return 0;
    }

	error_type[sizeof(error_type)-1] = '\0';
	daemon_name[sizeof(daemon_name)-1] = '\0';
	execute_host[sizeof(execute_host)-1] = '\0';

	if(!strcmp(error_type,"Error")) critical_error = true;
	else if(!strcmp(error_type,"Warning")) critical_error = false;

	//Now read one or more error_str lines from the body.
	MyString lines;

	while(!feof(file)) {
		// see if the next line contains an optional event notes string,
		// and, if not, rewind, because that means we slurped in the next
		// event delimiter looking for it...

		fpos_t filep;
		fgetpos( file, &filep );

		if( !fgets(line, sizeof(line), file) || strcmp(line, "...\n") == 0 ) {
			fsetpos( file, &filep );
			break;
		}

		char *l = strchr(line,'\n');
		if(l) *l = '\0';

		l = line;
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
RemoteErrorEvent::toClassAd(void)
{
	ClassAd* myad = ULogEvent::toClassAd();
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

int
ExecuteEvent::writeEvent (FILE *file)
{
	struct hostent *hp;
	unsigned long addr = -1;
	ClassAd tmpCl1, tmpCl2, tmpCl3;
	//ClassAd *tmpClP1 = &tmpCl1, *tmpClP2 = &tmpCl2, *tmpClP3 = &tmpCl3;
	MyString tmp = "";
	int retval;

	//JobAd is defined in condor_shadow.V6/log_events.C and is simply
	//defined as an external variable here

	scheddname = getenv( EnvGetName( ENV_SCHEDD_NAME ) );

	if(scheddname)
		dprintf(D_FULLDEBUG, "scheddname = %s\n", scheddname);
	else
		dprintf(D_FULLDEBUG, "scheddname is null\n");

	if( !executeHost ) {
		setExecuteHost("");
	}

	dprintf(D_FULLDEBUG, "executeHost = %s\n", executeHost);

	char *start = index(executeHost, '<');
	char *end = index(executeHost, ':');

	if(start && end) {
		char *tmpaddr;
		tmpaddr = (char *) malloc(32 * sizeof(char));
		tmpaddr = strncpy(tmpaddr, start+1, end-start-1);
		tmpaddr[end-start-1] = '\0';

		inet_pton(AF_INET, tmpaddr, &addr);

		dprintf(D_FULLDEBUG, "start = %s\n", start);
		dprintf(D_FULLDEBUG, "end = %s\n", end);
		dprintf(D_FULLDEBUG, "tmpaddr = %s\n", tmpaddr);
		free(tmpaddr);
	}
	else {
		inet_pton(AF_INET, executeHost, &addr);
	}

	//hp = condor_gethostbyaddr((char *) &addr, sizeof(addr), AF_INET);
	hp = gethostbyaddr((char *) &addr, sizeof(addr), AF_INET);
	if(hp) {
		dprintf(D_FULLDEBUG, "Executehost name = %s (hp->h_name) \n", hp->h_name);
	}
	else {
		dprintf(D_FULLDEBUG, "Executehost name = %s (executeHost) \n", executeHost);
	}

	tmpCl1.Assign("endts", (int)eventclock);

	tmp.formatstr("endtype = -1");
	tmpCl1.Insert(tmp.Value());

	tmp.formatstr("endmessage = \"UNKNOWN ERROR\"");
	tmpCl1.Insert(tmp.Value());

	// this inserts scheddname, cluster, proc, etc
	insertCommonIdentifiers(tmpCl2);

	tmp.formatstr("endtype = null");
	tmpCl2.Insert(tmp.Value());

	if (FILEObj) {
		if (FILEObj->file_updateEvent("Runs", &tmpCl1, &tmpCl2) == QUILL_FAILURE) {
			dprintf(D_ALWAYS, "Logging Event 1--- Error\n");
			return 0; // return a error code, 0
		}
	}

	if( !remoteName ) {
		setRemoteName("");
	}
	tmpCl3.Assign("machine_id", remoteName);

	// this inserts scheddname, cluster, proc, etc
	insertCommonIdentifiers(tmpCl3);

	tmpCl3.Assign("startts", (int)eventclock);

	if(FILEObj) {
		if (FILEObj->file_newEvent("Runs", &tmpCl3) == QUILL_FAILURE) {
			dprintf(D_ALWAYS, "Logging Event 1--- Error\n");
			return 0; // return a error code, 0
		}
	}

	retval = fprintf (file, "Job executing on host: %s\n", executeHost);

	if (retval < 0) {
		return 0;
	}

	return 1;
}

int
ExecuteEvent::readEvent (FILE *file)
{
	MyString line;
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
}

ClassAd*
ExecuteEvent::toClassAd(void)
{
	ClassAd* myad = ULogEvent::toClassAd();
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

int
ExecutableErrorEvent::writeEvent (FILE *file)
{
	int retval;
	char messagestr[512];
	ClassAd tmpCl1, tmpCl2;
	//ClassAd *tmpClP1 = &tmpCl1, *tmpClP2 = &tmpCl2;
	MyString tmp = "";

	scheddname = getenv( EnvGetName( ENV_SCHEDD_NAME ) );

	tmpCl1.Assign("endts", (int)eventclock);
	tmpCl1.Assign("endtype", ULOG_EXECUTABLE_ERROR);
	tmpCl1.Assign("endmessage", messagestr);

	// this inserts scheddname, cluster, proc, etc
	insertCommonIdentifiers(tmpCl2);

	tmp.formatstr( "endtype = null");
	tmpCl2.Insert(tmp.Value());

	if (FILEObj) {
		if (FILEObj->file_updateEvent("Runs", &tmpCl1, &tmpCl2) == QUILL_FAILURE) {
			dprintf(D_ALWAYS, "Logging Event 12--- Error\n");
			return 0; // return a error code, 0
		}
	}

	switch (errType)
	{
	  case CONDOR_EVENT_NOT_EXECUTABLE:
		retval = fprintf (file, "(%d) Job file not executable.\n", errType);
		sprintf(messagestr,  "Job file not executable");
		break;

	  case CONDOR_EVENT_BAD_LINK:
		retval=fprintf(file,"(%d) Job not properly linked for Condor.\n", errType);
		sprintf(messagestr,  "Job not properly linked for Condor");
		break;

	  default:
		retval = fprintf (file, "(%d) [Bad error number.]\n", errType);
		sprintf(messagestr,  "Unknown error");
	}

	if (retval < 0) return 0;

	return 1;
}


int
ExecutableErrorEvent::readEvent (FILE *file)
{
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

	return 1;
}

ClassAd*
ExecutableErrorEvent::toClassAd(void)
{
	ClassAd* myad = ULogEvent::toClassAd();
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

int
CheckpointedEvent::writeEvent (FILE *file)
{
	char messagestr[512];
	ClassAd tmpCl1;
	//ClassAd *tmpClP1 = &tmpCl1;

	sprintf(messagestr,  "Job was checkpointed");

	scheddname = getenv( EnvGetName( ENV_SCHEDD_NAME ) );

	// this inserts scheddname, cluster, proc, etc
	insertCommonIdentifiers(tmpCl1);

	tmpCl1.Assign("eventtype", ULOG_CHECKPOINTED);
	tmpCl1.Assign("eventtime", (int)eventclock);

	tmpCl1.Assign("description", messagestr);

	if (FILEObj) {
		if (FILEObj->file_newEvent("Events", &tmpCl1) == QUILL_FAILURE) {
			dprintf(D_ALWAYS, "Logging Event 6--- Error\n");
			return 0; // return a error code, 0
		}
	}

	if (fprintf (file, "Job was checkpointed.\n") < 0  		||
		(!writeRusage (file, run_remote_rusage)) 			||
		(fprintf (file, "  -  Run Remote Usage\n") < 0) 	||
		(!writeRusage (file, run_local_rusage)) 			||
		(fprintf (file, "  -  Run Local Usage\n") < 0))
		return 0;

    if( fprintf(file, "\t%.0f  -  Run Bytes Sent By Job For Checkpoint\n",
                sent_bytes) < 0 ) {
        return 0;
    }


	return 1;
}

int
CheckpointedEvent::readEvent (FILE *file)
{
	int retval = fscanf (file, "Job was checkpointed.\n");

	char buffer[128];
	if (retval == EOF ||
		!readRusage(file,run_remote_rusage) || fgets (buffer,128,file) == 0  ||
		!readRusage(file,run_local_rusage)  || fgets (buffer,128,file) == 0)
		return 0;

    if( !fscanf(file, "\t%f  -  Run Bytes Sent By Job For Checkpoint\n",
                &sent_bytes)) {
        return 1;		//backwards compatibility
    }

	return 1;
}

ClassAd*
CheckpointedEvent::toClassAd(void)
{
	ClassAd* myad = ULogEvent::toClassAd();
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
            EXCEPT( "ERROR: out of memory!\n" );
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
			EXCEPT( "ERROR: out of memory!\n" );
		}
	}
}


const char*
JobEvictedEvent::getCoreFile( void )
{
	return core_file;
}


int
JobEvictedEvent::readEvent( FILE *file )
{
	int  ckpt;
	char buffer [128];

	if( (fscanf(file, "Job was evicted.") == EOF) ||
		(fscanf(file, "\n\t(%d) ", &ckpt) != 1) )
	{
		return 0;
	}
	checkpointed = (bool) ckpt;
	if( fgets(buffer, 128, file) == 0 ) {
		return 0;
	}

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

	if( !fscanf(file, "\t%f  -  Run Bytes Sent By Job\n", &sent_bytes) ||
		!fscanf(file, "\t%f  -  Run Bytes Received By Job\n",
				&recvd_bytes) )
	{
		return 1;				// backwards compatibility
	}

	if( ! terminate_and_requeued ) {
			// nothing more to read
		return 1;
	}

		// now, parse the terminate and requeue specific stuff.

	int  normal_term;
	int  got_core;

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
	return 1;
}


int
JobEvictedEvent::writeEvent( FILE *file )
{
  char messagestr[512], checkpointedstr[6], terminatestr[512];
  ClassAd tmpCl1, tmpCl2;
  //ClassAd *tmpClP1 = &tmpCl1, *tmpClP2 = &tmpCl2;
  MyString tmp = "";

  //JobAd is defined in condor_shadow.V6/log_events.C and is simply
  //defined as an external variable here

  strcpy(checkpointedstr, "");
  strcpy(messagestr, "");
  strcpy(terminatestr, "");


  int retval;

  if( fprintf(file, "Job was evicted.\n\t") < 0 ) {
    return 0;
  }

  if( terminate_and_requeued ) {
    retval = fprintf( file, "(0) Job terminated and was requeued\n\t" );
    sprintf(messagestr,  "Job evicted, terminated and was requeued");
    strcpy(checkpointedstr, "false");
  } else if( checkpointed ) {
    retval = fprintf( file, "(1) Job was checkpointed.\n\t" );
    sprintf(messagestr,  "Job evicted and was checkpointed");
    strcpy(checkpointedstr, "true");
  } else {
    retval = fprintf( file, "(0) Job was not checkpointed.\n\t" );
    sprintf(messagestr,  "Job evicted and was not checkpointed");
    strcpy(checkpointedstr, "false");
  }

  if( retval < 0 ) {
    return 0;
  }

  if( (!writeRusage (file, run_remote_rusage)) 			||
      (fprintf (file, "  -  Run Remote Usage\n\t") < 0) 	||
      (!writeRusage (file, run_local_rusage)) 			||
      (fprintf (file, "  -  Run Local Usage\n") < 0) )
    {
      return 0;
    }

  if( fprintf(file, "\t%.0f  -  Run Bytes Sent By Job\n",
	      sent_bytes) < 0 ) {
    return 0;
  }
  if( fprintf(file, "\t%.0f  -  Run Bytes Received By Job\n",
	      recvd_bytes) < 0 ) {
    return 0;
  }

  if(terminate_and_requeued ) {
    if( normal ) {
      if( fprintf(file, "\t(1) Normal termination (return value %d)\n",
		  return_value) < 0 ) {
	return 0;
      }
      sprintf(terminatestr,  " (1) Normal termination (return value %d)", return_value);
    }
    else {
      if( fprintf(file, "\t(0) Abnormal termination (signal %d)\n",
		  signal_number) < 0 ) {
	return 0;
      }
      sprintf(terminatestr,  " (0) Abnormal termination (signal %d)", signal_number);

      if( core_file ) {
	retval = fprintf( file, "\t(1) Corefile in: %s\n", core_file );
	strcat(terminatestr, " (1) Corefile in: ");
	strcat(terminatestr, core_file);
      }
      else {
	retval = fprintf( file, "\t(0) No core file\n" );
	strcat(terminatestr, " (0) No core file ");
      }
      if( retval < 0 ) {
	return 0;
      }
    }

    if( reason ) {
      if( fprintf(file, "\t%s\n", reason) < 0 ) {
	return 0;
      }
      strcat(terminatestr,  " reason: ");
      strcat(terminatestr,  reason);
    }

  }

	// print out resource request/useage values.
	//
	if (pusageAd) {
		writeUsageAd(file, pusageAd);
	}

  scheddname = getenv( EnvGetName( ENV_SCHEDD_NAME ) );

  tmpCl1.Assign("endts", (int)eventclock);
  tmpCl1.Assign("endtype", ULOG_JOB_EVICTED);

  tmp.formatstr( "endmessage = \"%s%s\"", messagestr, terminatestr);
  tmpCl1.Insert(tmp.Value());

  tmpCl1.Assign("wascheckpointed", checkpointedstr);
  tmpCl1.Assign("runbytessent", sent_bytes);
  tmpCl1.Assign("runbytesreceived", recvd_bytes);

  // this inserts scheddname, cluster, proc, etc
  insertCommonIdentifiers(tmpCl2);

  tmp.formatstr( "endtype = null");
  tmpCl2.Insert(tmp.Value());

  if (FILEObj) {
	  if (FILEObj->file_updateEvent("Runs", &tmpCl1, &tmpCl2) == QUILL_FAILURE) {
		  dprintf(D_ALWAYS, "Logging Event 2 --- Error\n");
		  return 0; // return a error code, 0
	  }
  }

  return 1;
}

ClassAd*
JobEvictedEvent::toClassAd(void)
{
	ClassAd* myad = ULogEvent::toClassAd();
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
JobAbortedEvent::JobAbortedEvent (void)
{
	eventNumber = ULOG_JOB_ABORTED;
	reason = NULL;
}

JobAbortedEvent::~JobAbortedEvent(void)
{
	delete[] reason;
}


void
JobAbortedEvent::setReason( const char* reason_str )
{
	delete[] reason;
	reason = NULL;
	if( reason_str ) {
		reason = strnewp( reason_str );
		if( !reason ) {
			EXCEPT( "ERROR: out of memory!\n" );
		}
	}
}


const char*
JobAbortedEvent::getReason( void ) const
{
	return reason;
}


int
JobAbortedEvent::writeEvent (FILE *file)
{

	char messagestr[512];
	ClassAd tmpCl1;
	//ClassAd *tmpClP1 = &tmpCl1;
	MyString tmp = "";

	scheddname = getenv( EnvGetName( ENV_SCHEDD_NAME ) );

	if (reason)
		snprintf(messagestr,  512, "Job was aborted by the user: %s", reason);
	else
		sprintf(messagestr,  "Job was aborted by the user");

	// this inserts scheddname, cluster, proc, etc
	insertCommonIdentifiers(tmpCl1);

	tmpCl1.Assign("eventtype", ULOG_JOB_ABORTED);
	tmpCl1.Assign("eventtime", (int)eventclock);
	tmpCl1.Assign("description", messagestr);

	if (FILEObj) {
		if (FILEObj->file_newEvent("Events", &tmpCl1) == QUILL_FAILURE) {
			dprintf(D_ALWAYS, "Logging Event 7--- Error\n");
			return 0; // return a error code, 0
		}
	}

	if( fprintf(file, "Job was aborted by the user.\n") < 0 ) {
		return 0;
	}
	if( reason ) {
		if( fprintf(file, "\t%s\n", reason) < 0 ) {
			return 0;
		}
	}
	return 1;
}


int
JobAbortedEvent::readEvent (FILE *file)
{
	if( fscanf(file, "Job was aborted by the user.\n") == EOF ) {
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

	chomp( reason_buf );  // strip the newline, if it's there.
		// This is strange, sometimes we get the \t from fgets(), and
		// sometimes we don't.  Instead of trying to figure out why,
		// we just check for it here and do the right thing...
	if( reason_buf[0] == '\t' && reason_buf[1] ) {
		setReason( &reason_buf[1] );
	} else {
		setReason( reason_buf );
	}
	return 1;
}

ClassAd*
JobAbortedEvent::toClassAd(void)
{
	ClassAd* myad = ULogEvent::toClassAd();
	if( !myad ) return NULL;

	if( reason ) {
		if( !myad->InsertAttr("Reason", reason) ) {
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
}

// ----- TerminatedEvent baseclass
TerminatedEvent::TerminatedEvent(void)
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
}


void
TerminatedEvent::setCoreFile( const char* core_name )
{
	delete[] core_file;
	core_file = NULL;
	if( core_name ) {
		core_file = strnewp( core_name );
		if( !core_file ) {
            EXCEPT( "ERROR: out of memory!\n" );
		}
	}
}


const char*
TerminatedEvent::getCoreFile( void )
{
	return core_file;
}


int
TerminatedEvent::writeEvent( FILE *file, const char* header )
{
  char messagestr[512];
  ClassAd tmpCl1, tmpCl2;
  //ClassAd *tmpClP1 = &tmpCl1, *tmpClP2 = &tmpCl2;
  MyString tmp = "";

  //JobAd is defined in condor_shadow.V6/log_events.C and is simply
  //defined as an external variable here

  strcpy(messagestr, "");

	int retval=0;

	if( normal ) {
		if( fprintf(file, "\t(1) Normal termination (return value %d)\n\t",
					returnValue) < 0 ) {
			return 0;
		}
		sprintf(messagestr,  "(1) Normal termination (return value %d)", returnValue);

	} else {
		if( fprintf(file, "\t(0) Abnormal termination (signal %d)\n",
					signalNumber) < 0 ) {
			return 0;
		}

		sprintf(messagestr,  "(0) Abnormal termination (signal %d)", signalNumber);

		if( core_file ) {
			retval = fprintf( file, "\t(1) Corefile in: %s\n\t",
							  core_file );
			strcat(messagestr, " (1) Corefile in: ");
			strcat(messagestr, core_file);
		} else {
			retval = fprintf( file, "\t(0) No core file\n\t" );
			strcat(messagestr, " (0) No core file ");
		}
	}

	if ((retval < 0)										||
		(!writeRusage (file, run_remote_rusage))			||
		(fprintf (file, "  -  Run Remote Usage\n\t") < 0) 	||
		(!writeRusage (file, run_local_rusage)) 			||
		(fprintf (file, "  -  Run Local Usage\n\t") < 0)   	||
		(!writeRusage (file, total_remote_rusage))			||
		(fprintf (file, "  -  Total Remote Usage\n\t") < 0)	||
		(!writeRusage (file,  total_local_rusage))			||
		(fprintf (file, "  -  Total Local Usage\n") < 0))
		return 0;


	if (fprintf(file, "\t%.0f  -  Run Bytes Sent By %s\n",
				sent_bytes, header) < 0 ||
		fprintf(file, "\t%.0f  -  Run Bytes Received By %s\n",
				recvd_bytes, header) < 0 ||
		fprintf(file, "\t%.0f  -  Total Bytes Sent By %s\n",
				total_sent_bytes, header) < 0 ||
		fprintf(file, "\t%.0f  -  Total Bytes Received By %s\n",
				total_recvd_bytes, header) < 0)
		return 1;				// backwards compatibility

	// print out resource request/useage values.
	//
	if (pusageAd) {
		writeUsageAd(file, pusageAd);
	}

	scheddname = getenv( EnvGetName( ENV_SCHEDD_NAME ) );

	tmpCl1.Assign("endmessage", messagestr);
	tmpCl1.Assign("runbytessent", sent_bytes);
	tmpCl1.Assign("runbytesreceived", recvd_bytes);

	// this inserts scheddname, cluster, proc, etc
	insertCommonIdentifiers(tmpCl2);

	tmpCl2.Assign("endts", (int)eventclock);

	if (FILEObj) {
		if (FILEObj->file_updateEvent("Runs", &tmpCl1, &tmpCl2) == QUILL_FAILURE) {
			dprintf(D_ALWAYS, "Logging Event 3--- Error\n");
			return 0; // return a error code, 0
		}
	}

	return 1;
}


int
TerminatedEvent::readEvent( FILE *file, const char* header )
{
	char buffer[128];
	int  normalTerm;
	int  gotCore;
	int  retval;

	if (pusageAd) {
		pusageAd->Clear();
	}

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

		// read in rusage values
	if (!readRusage(file,run_remote_rusage) || !fgets(buffer, 128, file) ||
		!readRusage(file,run_local_rusage)   || !fgets(buffer, 128, file) ||
		!readRusage(file,total_remote_rusage)|| !fgets(buffer, 128, file) ||
		!readRusage(file,total_local_rusage) || !fgets(buffer, 128, file))
		return 0;

#if 1
	for (;;) {
		char sz[250];
		char srun[sizeof("Total")];
		char sdir[sizeof("Recieved")];
		char sjob[22];

		// if we hit end of file or end of record "..." rewind the file pointer.
		fpos_t filep;
		fgetpos( file, &filep );
		if ( ! fgets(sz, sizeof(sz), file) || 
			(sz[0] == '.' && sz[1] == '.' && sz[2] == '.')) {
			fsetpos( file, &filep );
			break;
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
		if ( ! fOK) {
			fsetpos(file, &filep);
			break;
		}
	}
	// the useage ad is optional
	readUsageAd(file, &pusageAd);
#else

		// THIS CODE IS TOTALLY BROKEN.  Please fix me.
		// In particular: fscanf() when you don't convert anything to
		// a local variable returns 0, but we think that's failure.
	if( fscanf (file, "\t%f  -  Run Bytes Sent By ", &sent_bytes) == 0 ||
		fscanf (file, header) == 0 ||
		fscanf (file, "\n") == 0 ||
		fscanf (file, "\t%f  -  Run Bytes Received By ",
				&recvd_bytes) == 0 ||
		fscanf (file, header) == 0 ||
		fscanf (file, "\n") == 0 ||
		fscanf (file, "\t%f  -  Total Bytes Sent By ",
				&total_sent_bytes) == 0 ||
		fscanf (file, header) == 0 ||
		fscanf (file, "\n") == 0 ||
		fscanf (file, "\t%f  -  Total Bytes Received By ",
				&total_recvd_bytes) == 0 ||
		fscanf (file, header) == 0 ||
		fscanf (file, "\n") == 0 ) {
		return 1;		// backwards compatibility
	}
#endif
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


int
JobTerminatedEvent::writeEvent (FILE *file)
{
  ClassAd tmpCl1, tmpCl2;
  //ClassAd *tmpClP1 = &tmpCl1, *tmpClP2 = &tmpCl2;
  MyString tmp = "";

  //JobAd is defined in condor_shadow.V6/log_events.C and is simply
  //defined as an external variable here

  scheddname = getenv( EnvGetName( ENV_SCHEDD_NAME ) );

  tmpCl1.Assign("endts", (int)eventclock);
  tmpCl1.Assign("endtype", ULOG_JOB_TERMINATED);

  // this inserts scheddname, cluster, proc, etc
  insertCommonIdentifiers(tmpCl2);

  tmp.formatstr( "endtype = null");
  tmpCl2.Insert(tmp.Value());

  if (FILEObj) {
	  if (FILEObj->file_updateEvent("Runs", &tmpCl1, &tmpCl2) == QUILL_FAILURE) {
		  dprintf(D_ALWAYS, "Logging Event 4--- Error\n");
		  return 0; // return a error code, 0
	  }
  }

  if( fprintf(file, "Job terminated.\n") < 0 ) {
	  return 0;
  }
  return TerminatedEvent::writeEvent( file, "Job" );
}


int
JobTerminatedEvent::readEvent (FILE *file)
{
	if( fscanf(file, "Job terminated.") == EOF ) {
		return 0;
	}
	return TerminatedEvent::readEvent( file, "Job" );
}

ClassAd*
JobTerminatedEvent::toClassAd(void)
{
	ClassAd* myad = ULogEvent::toClassAd();
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

	return myad;
}

void
JobTerminatedEvent::initFromClassAd(ClassAd* ad)
{
	ULogEvent::initFromClassAd(ad);

	if( !ad ) return;

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


int
JobImageSizeEvent::writeEvent (FILE *file)
{
	if (fprintf (file, "Image size of job updated: %" PRId64"\n", image_size_kb) < 0)
		return 0;

	// when talking to older starters, memory_usage, rss & pss may not be set
	if (memory_usage_mb >= 0 && 
		fprintf (file, "\t%" PRId64"  -  MemoryUsage of job (MB)\n", memory_usage_mb) < 0)
		return 0;

	if (resident_set_size_kb >= 0 &&
		fprintf (file, "\t%" PRId64"  -  ResidentSetSize of job (KB)\n", resident_set_size_kb) < 0)
		return 0;

	if (proportional_set_size_kb >= 0 &&
		fprintf (file, "\t%" PRId64"  -  ProportionalSetSize of job (KB)\n", proportional_set_size_kb) < 0)
		return 0;

	return 1;
}


int
JobImageSizeEvent::readEvent (FILE *file)
{
	int retval;
	if ((retval=fscanf(file,"Image size of job updated: %" PRId64, &image_size_kb)) != 1)
		return 0;

	// These fields were added to this event in 2012, so we need to tolerate the
	// situation when they are not there in the log that we read back.
	memory_usage_mb = -1;
	resident_set_size_kb = 0;
	proportional_set_size_kb = -1;

	for (;;) {
		char sz[250];
		char lbl[48+1];

		// if we hit end of file or end of record "..." rewind the file pointer.
		fpos_t filep;
		fgetpos(file, &filep);
		if ( ! fgets(sz, sizeof(sz), file) || 
			(sz[0] == '.' && sz[1] == '.' && sz[2] == '.')) {
			fsetpos(file, &filep);
			break;
		}

		int64_t val; lbl[0] = 0;
		if (2 == sscanf(sz, "\t%" PRId64"  -  %48s", &val, lbl)) {
			if (!strcmp(lbl,"MemoryUsage")) {
				memory_usage_mb = val;
			} else if (!strcmp(lbl, "ResidentSetSize")) {
				resident_set_size_kb = val;
			} else if (!strcmp(lbl, "ProportionalSetSize")) {
				proportional_set_size_kb = val;
			} else {
				// rewind the file pointer so we don't consume what we can't parse.
				fsetpos(file, &filep);
				break;
			}
		}
	}

	return 1;
}

ClassAd*
JobImageSizeEvent::toClassAd(void)
{
	ClassAd* myad = ULogEvent::toClassAd();
	if( !myad ) return NULL;
	char buf0[250];

	if( image_size_kb >= 0 ) {
		snprintf(buf0, sizeof(buf0), "Size = %" PRId64, image_size_kb);
		buf0[sizeof(buf0)-1] = 0;
		if( !myad->Insert(buf0) ) return NULL;
	}
	if( memory_usage_mb >= 0 ) {
		snprintf(buf0, sizeof(buf0), "MemoryUsage = %" PRId64, memory_usage_mb);
		buf0[sizeof(buf0)-1] = 0;
		if( !myad->Insert(buf0) ) return NULL;
	}
	if( resident_set_size_kb >= 0 ) {
		snprintf(buf0, sizeof(buf0), "ResidentSetSize = %" PRId64, resident_set_size_kb);
		buf0[sizeof(buf0)-1] = 0;
		if( !myad->Insert(buf0) ) return NULL;
	}
	if( proportional_set_size_kb >= 0 ) {
		snprintf(buf0, sizeof(buf0), "ProportionalSetSize = %" PRId64, proportional_set_size_kb);
		buf0[sizeof(buf0)-1] = 0;
		if( !myad->Insert(buf0) ) return NULL;
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
ShadowExceptionEvent::readEvent (FILE *file)
{
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

	return 1;
}

int
ShadowExceptionEvent::writeEvent (FILE *file)
{
	char messagestr[512];
	ClassAd tmpCl1, tmpCl2;
	//ClassAd *tmpClP1 = &tmpCl1, *tmpClP2 = &tmpCl2;
	MyString tmp = "";

	scheddname = getenv( EnvGetName( ENV_SCHEDD_NAME ) );

	snprintf(messagestr, 512, "Shadow exception: %s", message);
	messagestr[COUNTOF(messagestr)-1] = 0;

		// remove the new line in the end if any
	if  (messagestr[strlen(messagestr)-1] == '\n')
		messagestr[strlen(messagestr)-1] = '\0';

	if (began_execution) {
		tmpCl1.Assign("endts", (int)eventclock);
		tmpCl1.Assign("endtype", ULOG_SHADOW_EXCEPTION);
		tmpCl1.Assign("endmessage", messagestr);
		tmpCl1.Assign("runbytessent", sent_bytes);

		tmpCl1.Assign("runbytesreceived", recvd_bytes);

		// this inserts scheddname, cluster, proc, etc
		insertCommonIdentifiers(tmpCl2);

		tmp.formatstr( "endtype = null");
		tmpCl2.Insert(tmp.Value());

		if (FILEObj) {
			if (FILEObj->file_updateEvent("Runs", &tmpCl1, &tmpCl2) == QUILL_FAILURE) {
				dprintf(D_ALWAYS, "Logging Event 13--- Error\n");
				return 0; // return a error code, 0
			}
		}
	} else {
		// this inserts scheddname, cluster, proc, etc
        insertCommonIdentifiers(tmpCl1);

		tmpCl1.Assign("eventtype", ULOG_SHADOW_EXCEPTION);
		tmpCl1.Assign("eventtime", (int)eventclock);
		tmpCl1.Assign("description", messagestr);

		if (FILEObj) {
			if (FILEObj->file_newEvent("Events", &tmpCl1) == QUILL_FAILURE) {
				dprintf(D_ALWAYS, "Logging Event 14 --- Error\n");
				return 0; // return a error code, 0
			}
		}

	}

	if (fprintf (file, "Shadow exception!\n\t") < 0)
		return 0;
	if (fprintf (file, "%s\n", message) < 0)
		return 0;

	if (fprintf (file, "\t%.0f  -  Run Bytes Sent By Job\n", sent_bytes) < 0 ||
		fprintf (file, "\t%.0f  -  Run Bytes Received By Job\n",
				 recvd_bytes) < 0)
		return 1;				// backwards compatibility

	return 1;
}

ClassAd*
ShadowExceptionEvent::toClassAd(void)
{
	bool     success = true;
	ClassAd* myad = ULogEvent::toClassAd();
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
JobSuspendedEvent::readEvent (FILE *file)
{
	if (fscanf (file, "Job was suspended.\n\t") == EOF)
		return 0;
	if (fscanf (file, "Number of processes actually suspended: %d\n",
			&num_pids) == EOF)
		return 1;				// backwards compatibility

	return 1;
}


int
JobSuspendedEvent::writeEvent (FILE *file)
{
	char messagestr[512];
	ClassAd tmpCl1;
	//ClassAd *tmpClP1 = &tmpCl1;
	MyString tmp = "";

	sprintf(messagestr, "Job was suspended (Number of processes actually suspended: %d)", num_pids);

	scheddname = getenv( EnvGetName( ENV_SCHEDD_NAME ) );

	// this inserts scheddname, cluster, proc, etc
	insertCommonIdentifiers(tmpCl1);

	tmpCl1.Assign("eventtype", ULOG_JOB_SUSPENDED);
	tmpCl1.Assign("eventtime", (int)eventclock);
	tmpCl1.Assign("description", messagestr);

	if (FILEObj) {
		if (FILEObj->file_newEvent("Events", &tmpCl1) == QUILL_FAILURE) {
			dprintf(D_ALWAYS, "Logging Event 8--- Error\n");
			return 0; // return a error code, 0
		}
	}

	if (fprintf (file, "Job was suspended.\n\t") < 0)
		return 0;
	if (fprintf (file, "Number of processes actually suspended: %d\n",
			num_pids) < 0)
		return 0;

	return 1;
}

ClassAd*
JobSuspendedEvent::toClassAd(void)
{
	ClassAd* myad = ULogEvent::toClassAd();
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
JobUnsuspendedEvent::readEvent (FILE *file)
{
	if (fscanf (file, "Job was unsuspended.\n") == EOF)
		return 0;

	return 1;
}

int
JobUnsuspendedEvent::writeEvent (FILE *file)
{
	char messagestr[512];
	ClassAd tmpCl1;
	//ClassAd *tmpClP1 = &tmpCl1;
	MyString tmp = "";

	sprintf(messagestr, "Job was unsuspended");

	scheddname = getenv( EnvGetName( ENV_SCHEDD_NAME ) );

	// this inserts scheddname, cluster, proc, etc
	insertCommonIdentifiers(tmpCl1);

	tmpCl1.Assign("eventtype", ULOG_JOB_UNSUSPENDED);
	tmpCl1.Assign("eventtime", (int)eventclock);
	tmpCl1.Assign("description", messagestr);

	if (FILEObj) {
 	    if (FILEObj->file_newEvent("Events", &tmpCl1) == QUILL_FAILURE) {
			dprintf(D_ALWAYS, "Logging Event 9--- Error\n");
			return 0; // return a error code, 0
		}
	}

	if (fprintf (file, "Job was unsuspended.\n") < 0)
		return 0;

	return 1;
}

ClassAd*
JobUnsuspendedEvent::toClassAd(void)
{
	ClassAd* myad = ULogEvent::toClassAd();
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
            EXCEPT( "ERROR: out of memory!\n" );
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
JobHeldEvent::readEvent( FILE *file )
{
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

	return 1;
}


int
JobHeldEvent::writeEvent( FILE *file )
{
	char messagestr[512];
	ClassAd tmpCl1;
	//ClassAd *tmpClP1 = &tmpCl1;

	if (reason)
		snprintf(messagestr, 512, "Job was held: %s", reason);
	else
		sprintf(messagestr, "Job was held: reason unspecified");

	scheddname = getenv( EnvGetName( ENV_SCHEDD_NAME ) );

	// this inserts scheddname, cluster, proc, etc
	insertCommonIdentifiers(tmpCl1);

	tmpCl1.Assign("eventtype", ULOG_JOB_HELD);
	tmpCl1.Assign("eventtime", (int)eventclock);
	tmpCl1.Assign("description", messagestr);

	if (FILEObj) {
		if (FILEObj->file_newEvent("Events", &tmpCl1) == QUILL_FAILURE) {
			dprintf(D_ALWAYS, "Logging Event 10--- Error\n");
			return 0; // return a error code, 0
		}
	}

	if( fprintf(file, "Job was held.\n") < 0 ) {
		return 0;
	}
	if( reason ) {
		if( fprintf(file, "\t%s\n", reason) < 0 ) {
			return 0;
		}
	} else {
		if( fprintf(file, "\tReason unspecified\n") < 0 ) {
			return 0;
		}
	}

	// write the codes
	if( fprintf(file, "\tCode %d Subcode %d\n", code,subcode) < 0 ) {
		return 0;
	}

	return 1;
}

ClassAd*
JobHeldEvent::toClassAd(void)
{
	ClassAd* myad = ULogEvent::toClassAd();
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
            EXCEPT( "ERROR: out of memory!\n" );
        }
    }
}


const char*
JobReleasedEvent::getReason( void ) const
{
	return reason;
}


int
JobReleasedEvent::readEvent( FILE *file )
{
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
	return 1;
}


int
JobReleasedEvent::writeEvent( FILE *file )
{
	char messagestr[512];
	ClassAd tmpCl1;
	//ClassAd *tmpClP1 = &tmpCl1;
	MyString tmp = "";

	if (reason)
		snprintf(messagestr, 512, "Job was released: %s", reason);
	else
		sprintf(messagestr, "Job was released: reason unspecified");

	scheddname = getenv( EnvGetName( ENV_SCHEDD_NAME ) );

	// this inserts scheddname, cluster, proc, etc
	insertCommonIdentifiers(tmpCl1);

	tmpCl1.Assign("eventtype", ULOG_JOB_RELEASED);
	tmpCl1.Assign("eventtime", (int)eventclock);
	tmpCl1.Assign("description", messagestr);

	if (FILEObj) {
		if (FILEObj->file_newEvent("Events", &tmpCl1) == QUILL_FAILURE) {
			dprintf(D_ALWAYS, "Logging Event 11--- Error\n");
			return 0; // return a error code, 0
		}
	}

	if( fprintf(file, "Job was released.\n") < 0 ) {
		return 0;
	}
	if( reason ) {
		if( fprintf(file, "\t%s\n", reason) < 0 ) {
			return 0;
		} else {
			return 1;
		}
	}
		// do we want to do anything else if there's no reason?
		// should we fail?  EXCEPT()?
	return 1;
}

ClassAd*
JobReleasedEvent::toClassAd(void)
{
	ClassAd* myad = ULogEvent::toClassAd();
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

int
ULogEvent::writeRusage (FILE *file, rusage &usage)
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
	retval = fprintf (file, "\tUsr %d %02d:%02d:%02d, Sys %d %02d:%02d:%02d",
					  usr_days, usr_hours, usr_minutes, usr_secs,
					  sys_days, sys_hours, sys_minutes, sys_secs);

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
ULogEvent::rusageToStr (rusage usage)
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
ULogEvent::strToRusage (char* rusageStr, rusage & usage)
{
	int usr_secs, usr_minutes, usr_hours, usr_days;
	int sys_secs, sys_minutes, sys_hours, sys_days;
	int retval;

	retval = sscanf (rusageStr, "\tUsr %d %d:%d:%d, Sys %d %d:%d:%d",
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

int
NodeExecuteEvent::writeEvent (FILE *file)
{
	if( !executeHost ) {
		setExecuteHost("");
	}
	return( fprintf(file, "Node %d executing on host: %s\n",
					node, executeHost) >= 0 );
}


int
NodeExecuteEvent::readEvent (FILE *file)
{
	MyString line;
	if( !line.readLine(file) ) {
		return 0; // EOF or error
	}
	setExecuteHost(line.Value()); // allocate memory
	int retval = sscanf(line.Value(), "Node %d executing on host: %s",
						&node, executeHost);
	return retval == 2;
}

ClassAd*
NodeExecuteEvent::toClassAd(void)
{
	ClassAd* myad = ULogEvent::toClassAd();
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
}


NodeTerminatedEvent::~NodeTerminatedEvent(void)
{
}


int
NodeTerminatedEvent::writeEvent( FILE *file )
{
	if( fprintf(file, "Node %d terminated.\n", node) < 0 ) {
		return 0;
	}
	return TerminatedEvent::writeEvent( file, "Node" );
}


int
NodeTerminatedEvent::readEvent( FILE *file )
{
	if( fscanf(file, "Node %d terminated.", &node) == EOF ) {
		return 0;
	}
	return TerminatedEvent::readEvent( file, "Node" );
}

ClassAd*
NodeTerminatedEvent::toClassAd(void)
{
	ClassAd* myad = ULogEvent::toClassAd();
	if( !myad ) return NULL;

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


int
PostScriptTerminatedEvent::writeEvent( FILE* file )
{
    if( fprintf( file, "POST Script terminated.\n" ) < 0 ) {
        return 0;
    }

    if( normal ) {
        if( fprintf( file, "\t(1) Normal termination (return value %d)\n",
					 returnValue ) < 0 ) {
            return 0;
        }
    } else {
        if( fprintf( file, "\t(0) Abnormal termination (signal %d)\n",
					 signalNumber ) < 0 ) {
            return 0;
        }
    }

    if( dagNodeName ) {
        if( fprintf( file, "    %s%.8191s\n",
					 dagNodeNameLabel, dagNodeName ) < 0 ) {
            return 0;
        }
    }

    return 1;
}


int
PostScriptTerminatedEvent::readEvent( FILE* file )
{
	int tmp;
	char buf[8192];
	buf[0] = '\0';

		// first clear any existing DAG node name
	if( dagNodeName ) {
		delete[] dagNodeName;
	}
    dagNodeName = NULL;

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

    return 1;
}

ClassAd*
PostScriptTerminatedEvent::toClassAd(void)
{
	ClassAd* myad = ULogEvent::toClassAd();
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
			EXCEPT( "ERROR: out of memory!\n" );
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
			EXCEPT( "ERROR: out of memory!\n" );
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
			EXCEPT( "ERROR: out of memory!\n" );
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
			EXCEPT( "ERROR: out of memory!\n" );
		}
		can_reconnect = false;
	}
}


int
JobDisconnectedEvent::writeEvent( FILE *file )
{
	if( ! disconnect_reason ) {
		EXCEPT( "JobDisconnectedEvent::writeEvent() called without "
				"disconnect_reason" );
	}
	if( ! startd_addr ) {
		EXCEPT( "JobDisconnectedEvent::writeEvent() called without "
				"startd_addr" );
	}
	if( ! startd_name ) {
		EXCEPT( "JobDisconnectedEvent::writeEvent() called without "
				"startd_name" );
	}
	if( ! can_reconnect && ! no_reconnect_reason ) {
		EXCEPT( "impossible: JobDisconnectedEvent::writeEvent() called "
				"without no_reconnect_reason when can_reconnect is FALSE" );
	}

	if( fprintf(file, "Job disconnected, %s reconnect\n",
				can_reconnect ? "attempting to" : "can not") < 0 ) {
		return 0;
	}
	if( fprintf(file, "    %.8191s\n", disconnect_reason) < 0 ) {
		return 0;
	}
	if( fprintf(file, "    %s reconnect to %s %s\n",
				can_reconnect ? "Trying to" : "Can not",
				startd_name, startd_addr) < 0 ) {
		return 0;
	}
	if( no_reconnect_reason ) {
		if( fprintf(file, "    %.8191s\n", no_reconnect_reason) < 0 ) {
			return 0;
		}
		if( fprintf(file, "    Rescheduling job\n") < 0 ) {
			return 0;
		}
	}
	return( 1 );
}


int
JobDisconnectedEvent::readEvent( FILE *file )
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
		setDisconnectReason( &line[4] );
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
			line.setChar( i, '\0' );
			setStartdName( line.Value() );
			setStartdAddr( &line[i+1] );
		} else {
			return 0;
		}
	} else if( line.replaceString("    Can not reconnect to ", "") ) {
		if( can_reconnect ) {
			return 0;
		}
		int i = line.FindChar( ' ' );
		if( i > 0 ) {
			line.setChar( i, '\0' );
			setStartdName( line.Value() );
			setStartdAddr( &line[i+1] );
		} else {
			return 0;
		}
		if( line.readLine(file) && line[0] == ' ' && line[1] == ' '
			&& line[2] == ' ' && line[3] == ' ' && line[4] )
		{
			line.chomp();
			setNoReconnectReason( &line[4] );
		} else {
			return 0;
		}
	} else {
		return 0;
	}

	return 1;
}


ClassAd*
JobDisconnectedEvent::toClassAd( void )
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


	ClassAd* myad = ULogEvent::toClassAd();
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
			EXCEPT( "ERROR: out of memory!\n" );
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
			EXCEPT( "ERROR: out of memory!\n" );
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
			EXCEPT( "ERROR: out of memory!\n" );
		}
	}
}


int
JobReconnectedEvent::writeEvent( FILE *file )
{
	if( ! startd_addr ) {
		EXCEPT( "JobReconnectedEvent::writeEvent() called without "
				"startd_addr" );
	}
	if( ! startd_name ) {
		EXCEPT( "JobReconnectedEvent::writeEvent() called without "
				"startd_name" );
	}
	if( ! starter_addr ) {
		EXCEPT( "JobReconnectedEvent::writeEvent() called without "
				"starter_addr" );
	}

	if( fprintf(file, "Job reconnected to %s\n", startd_name) < 0 ) {
		return 0;
	}
	if( fprintf(file, "    startd address: %s\n", startd_addr) < 0 ) {
		return 0;
	}
	if( fprintf(file, "    starter address: %s\n", starter_addr) < 0 ) {
		return 0;
	}
	return( 1 );
}


int
JobReconnectedEvent::readEvent( FILE *file )
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
JobReconnectedEvent::toClassAd( void )
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

	ClassAd* myad = ULogEvent::toClassAd();
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
			EXCEPT( "ERROR: out of memory!\n" );
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
			EXCEPT( "ERROR: out of memory!\n" );
		}
	}
}


int
JobReconnectFailedEvent::writeEvent( FILE *file )
{
	if( ! reason ) {
		EXCEPT( "JobReconnectFailedEvent::writeEvent() called without "
				"reason" );
	}
	if( ! startd_name ) {
		EXCEPT( "JobReconnectFailedEvent::writeEvent() called without "
				"startd_name" );
	}

	if( fprintf(file, "Job reconnection failed\n") < 0 ) {
		return 0;
	}
	if( fprintf(file, "    %.8191s\n", reason) < 0 ) {
		return 0;
	}
	if( fprintf(file, "    Can not reconnect to %s, rescheduling job\n",
				startd_name) < 0 ) {
		return 0;
	}
	return( 1 );
}


int
JobReconnectFailedEvent::readEvent( FILE *file )
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
		setReason( &line[4] );
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
			line.setChar( i, '\0' );
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
JobReconnectFailedEvent::toClassAd( void )
{
	if( ! reason ) {
		EXCEPT( "JobReconnectFailedEvent::toClassAd() called without "
				"reason" );
	}
	if( ! startd_name ) {
		EXCEPT( "JobReconnectFailedEvent::toClassAd() called without "
				"startd_name" );
	}

	ClassAd* myad = ULogEvent::toClassAd();
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

int GridResourceUpEvent::writeEvent (FILE *file)
{
	const char * unknown = "UNKNOWN";
	const char * resource = unknown;

	int retval = fprintf (file, "Grid Resource Back Up\n");
	if (retval < 0)
	{
		return 0;
	}

	if ( resourceName ) resource = resourceName;

	retval = fprintf( file, "    GridResource: %.8191s\n", resource );
	if( retval < 0 ) {
		return 0;
	}

	return (1);
}

int
GridResourceUpEvent::readEvent (FILE *file)
{
	char s[8192];

	delete[] resourceName;
	resourceName = NULL;
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
	return 1;
}

ClassAd*
GridResourceUpEvent::toClassAd(void)
{
	ClassAd* myad = ULogEvent::toClassAd();
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

int
GridResourceDownEvent::writeEvent (FILE *file)
{
	const char * unknown = "UNKNOWN";
	const char * resource = unknown;

	int retval = fprintf (file, "Detected Down Grid Resource\n");
	if (retval < 0)
	{
		return 0;
	}

	if ( resourceName ) resource = resourceName;

	retval = fprintf( file, "    GridResource: %.8191s\n", resource );
	if( retval < 0 ) {
		return 0;
	}

	return (1);
}

int
GridResourceDownEvent::readEvent (FILE *file)
{
	char s[8192];

	delete[] resourceName;
	resourceName = NULL;
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
	return 1;
}

ClassAd*
GridResourceDownEvent::toClassAd(void)
{
	ClassAd* myad = ULogEvent::toClassAd();
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

int
GridSubmitEvent::writeEvent (FILE *file)
{
	const char * unknown = "UNKNOWN";
	const char * resource = unknown;
	const char * job = unknown;

	int retval = fprintf (file, "Job submitted to grid resource\n");
	if (retval < 0)
	{
		return 0;
	}

	if ( resourceName ) resource = resourceName;
	if ( jobId ) job = jobId;

	retval = fprintf( file, "    GridResource: %.8191s\n", resource );
	if( retval < 0 ) {
		return 0;
	}

	retval = fprintf( file, "    GridJobId: %.8191s\n", job );
	if( retval < 0 ) {
		return 0;
	}

	return (1);
}

int
GridSubmitEvent::readEvent (FILE *file)
{
	char s[8192];

	delete[] resourceName;
	delete[] jobId;
	resourceName = NULL;
	jobId = NULL;
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

	return 1;
}

ClassAd*
GridSubmitEvent::toClassAd(void)
{
	ClassAd* myad = ULogEvent::toClassAd();
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
}

int
JobAdInformationEvent::writeEvent(FILE *file)
{
	return writeEvent(file,jobad);
}

int
JobAdInformationEvent::writeEvent(FILE *file, ClassAd *jobad_arg)
{
    int retval = 0;	 // 0 == FALSE == failure

	fprintf(file,"Job ad information event triggered.\n");

	if ( jobad_arg ) {
		retval = jobad_arg->fPrint(file);
	}

    return retval;
}

int
JobAdInformationEvent::readEvent(FILE *file)
{
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
}

ClassAd*
JobAdInformationEvent::toClassAd(void)
{
	ClassAd* myad = ULogEvent::toClassAd();
	if( !myad ) return NULL;

	MergeClassAds(myad,jobad,false);

		// Reset MyType in case MergeClassAds() clobbered it.
	myad->SetMyTypeName("JobAdInformationEvent");

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
JobAdInformationEvent::LookupFloat (const char *attributeName, float & value) const
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

int
JobStatusUnknownEvent::writeEvent (FILE *file)
{
	int retval = fprintf (file, "The job's remote status is unknown\n");
	if (retval < 0)
	{
		return 0;
	}
	
	return (1);
}

int JobStatusUnknownEvent::readEvent (FILE *file)
{
	int retval = fscanf (file, "The job's remote status is unknown\n");
    if (retval != 0)
    {
		return 0;
    }
	return 1;
}

ClassAd*
JobStatusUnknownEvent::toClassAd(void)
{
	return ULogEvent::toClassAd();
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

int
JobStatusKnownEvent::writeEvent (FILE *file)
{
	int retval = fprintf (file, "The job's remote status is known again\n");
	if (retval < 0)
	{
		return 0;
	}

	return (1);
}

int JobStatusKnownEvent::readEvent (FILE *file)
{
	int retval = fscanf (file, "The job's remote status is known again\n");
    if (retval != 0)
    {
		return 0;
    }
	return 1;
}

ClassAd*
JobStatusKnownEvent::toClassAd(void)
{
	return ULogEvent::toClassAd();
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

int
JobStageInEvent::writeEvent (FILE *file)
{
	int retval = fprintf (file, "Job is performing stage-in of input files\n");
	if (retval < 0)
	{
		return 0;
	}

	return (1);
}

int
JobStageInEvent::readEvent (FILE *file)
{
	int retval = fscanf (file, "Job is performing stage-in of input files\n");
    if (retval != 0)
    {
		return 0;
    }
	return 1;
}

ClassAd*
JobStageInEvent::toClassAd(void)
{
	return ULogEvent::toClassAd();
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

int
JobStageOutEvent::writeEvent (FILE *file)
{
	int retval = fprintf (file, "Job is performing stage-out of output files\n");
	if (retval < 0)
	{
		return 0;
	}

	return (1);
}

int
JobStageOutEvent::readEvent (FILE *file)
{
	int retval = fscanf (file, "Job is performing stage-out of output files\n");
    if (retval != 0)
    {
		return 0;
    }
	return 1;
}

ClassAd*
JobStageOutEvent::toClassAd(void)
{
	return ULogEvent::toClassAd();
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

int
AttributeUpdate::writeEvent(FILE *file)
{
	int retval;
	if (old_value)
	{
		retval = fprintf(file, "Changing job attribute %s from %s to %s\n", name, old_value, value);
	} else {
		retval = fprintf(file, "Setting job attribute %s to %s\n", name, value);
	}

	if (retval < 0)
	{
		return 0;
	}

	return 1;
}

int
AttributeUpdate::readEvent(FILE *file)
{
	char buf1[4096], buf2[4096], buf3[4096];
	int retval;

	buf1[0] = '\0';
	buf2[0] = '\0';
	buf3[0] = '\0';
	retval = fscanf(file, "Changing job attribute %s from %s to %s\n", buf1, buf2, buf3);
	if (retval < 0)
	{
		retval = fscanf(file, "Setting job attribute %s to %s\n", buf1, buf3);
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
AttributeUpdate::toClassAd(void)
{
	ClassAd* myad = ULogEvent::toClassAd();
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
