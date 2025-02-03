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
#include "condor_debug.h"
#include "condor_attributes.h"
#include "basename.h"
#include "classadHistory.h"
#include "condor_uid.h"
#include "directory_util.h"
#include "job_ad_instance_recording.h"
#include "proc.h"

//--------------------------------------------------------------
//                       Data members
//--------------------------------------------------------------

static bool epochHistoryIsInitialized = false;
static HistoryFileRotationInfo hri; //File rotation info for aggregate history like file
static HistoryFileRotationInfo dri; //File rotation info for epoch directory files
// Struct to hold information of where to write epoch ads
struct EpochWriteFilesInfo {
	auto_free_ptr JobEpochInstDir;      //Path to valid directory for epoch files
	auto_free_ptr EpochHistoryFilename; //Path/filename for aggregate file
	//TODO: Add debug file option (written at start of shadow)
	//TODO: Add option for per cluster maybe per run inst
	bool can_writeAd = false;           //Bool for if we have a place to write Ad
} static efi;

struct EpochAdInfo {
	JOB_ID_KEY jid;             //Job Id for cluster & proc
	int runId = -1;             //Job run instance
	std::string buffer = "";    //Buffer that is the whole job ad with banner
	std::string file_path = ""; //Path and filename to write buffer to
};

//--------------------------------------------------------------
//                     Helper Functions
//--------------------------------------------------------------

/*
*	Function to set initialize structs with information from various
*	param calls to determine where/how we write the Ad and any needed
*	file rotation information.
*/
static void
initJobEpochHistoryFiles(){
	epochHistoryIsInitialized = true;
	efi.can_writeAd = false;
	//Initialize epoch aggregate file
	efi.EpochHistoryFilename.set(param("JOB_EPOCH_HISTORY"));
	if (efi.EpochHistoryFilename.ptr()) {

		//Initialize file rotation info
		//TODO: Maybe add possibility for daily and monthly rotation for aggregate file
		hri.IsStandardHistory = false;
		long long maxSize = 0;
		param_longlong("MAX_EPOCH_HISTORY_LOG", maxSize, true, 1024 * 1024 * 20);
		hri.MaxHistoryFileSize = maxSize;
		hri.NumberBackupHistoryFiles = param_integer("MAX_EPOCH_HISTORY_ROTATIONS",2,1);

		dprintf(D_FULLDEBUG, "Writing job run instance Ads to: %s\n",
				efi.EpochHistoryFilename.ptr());
		dprintf(D_FULLDEBUG, "Maximum epoch history size: %lld\n", (long long)hri.MaxHistoryFileSize);
		dprintf(D_FULLDEBUG, "Number of epoch history files: %d\n", hri.NumberBackupHistoryFiles);
		efi.can_writeAd = true;
	}

	//Initialize an epoch Directory
	efi.JobEpochInstDir.set(param("JOB_EPOCH_HISTORY_DIR"));
	if (efi.JobEpochInstDir.ptr()) {
		// If param was found and not null check if it is a valid directory
		struct stat si = {};
		stat(efi.JobEpochInstDir.ptr(), &si);
		if (!(si.st_mode & S_IFDIR)) {
			// Not a valid directory. Log message and set data member to NULL
			dprintf(D_ERROR, "Invalid JOB_EPOCH_HISTORY_DIR (%s): must point to a "
					"valid directory; disabling per-job run instance recording.\n",
						efi.JobEpochInstDir.ptr());
			efi.JobEpochInstDir.clear();
		} else {
			dprintf(D_FULLDEBUG, "Writing per-job run instance recording files to: %s\n",
					efi.JobEpochInstDir.ptr());
			//TODO: Check for per cluster files in epoch dir
			//TODO: Add filesize limit for directory epoch files
			//Note: Max backups is obsolete here because we will rotate to a different directory
			dri.IsStandardHistory = false;
			dri.MaxHistoryFileSize = 1024 * 1024 * 100; //Base 100MB until adding user defined limit (maybe be clever by asking number of adds and doing math)
			efi.can_writeAd = true;
		}
	}
}


classad::ClassAd *
copyEpochJobAttrs( const classad::ClassAd * job_ad, const classad::ClassAd * other_ad, const char * banner_name ) {
    std::string paramName;
    formatstr( paramName, "%s_JOB_ATTRS", banner_name );

    // For admins to explicitly specify no attributes, these three
    // parameters must NOT be in the param table.
    if(! param_defined_by_config(paramName.c_str())) {
        if( (strcmp(banner_name, "INPUT" ) == 0) ||
          (strcmp(banner_name, "OUTPUT" ) == 0) ||
          (strcmp(banner_name, "CHECKPOINT" ) == 0) ) {
            paramName = "TRANSFER_JOB_ATTRS";
        }
    }

    std::string attributes;
    param( attributes, paramName.c_str() );
    if( attributes.empty() ) { return NULL; }

    auto * new_ad = new classad::ClassAd(* other_ad);
    std::vector<std::string> attributeList = split(attributes);
    for( const auto & attribute : attributeList ) {
        CopyAttribute( attribute, * new_ad, attribute, * job_ad );
    }

    return new_ad;
}


/*
*	Function to attempt to grab needed information from the passed job ad,
*	and print ad to a buffer to write to various files
*/
static bool
extractEpochInfo(const classad::ClassAd *job_ad, EpochAdInfo& info, const classad::ClassAd * other_ad, const char * banner_name){
	//Get various information needed for writing epoch file and banner
	std::string owner, missingAttrs;

	if (!job_ad->LookupInteger("ClusterId", info.jid.cluster)) {
		info.jid.cluster = -1;
		missingAttrs += "ClusterId";
	}
	if (!job_ad->LookupInteger("ProcId", info.jid.proc)) {
		info.jid.cluster = -1;
		if (!missingAttrs.empty()) { missingAttrs += ',';}
		missingAttrs += "ProcId";
	}
	if (!job_ad->LookupInteger("NumShadowStarts", info.runId)) {
		//TODO: Replace Using NumShadowStarts with a better counter for epoch
		if (!missingAttrs.empty()) { missingAttrs += ',';}
		missingAttrs += "NumShadowStarts";
	}
	if (!job_ad->LookupString("Owner", owner)) {
		owner = "?";
	}

	//TODO:Until better Job Attribute is added, decrement numShadow to start RunInstanceId at 0
	info.runId--;

	//If any attributes are set to -1 write to shadow log and return
	if (info.jid.cluster < 0 || info.jid.proc < 0 || info.runId < 0){
		dprintf(D_FULLDEBUG,"Missing attribute(s) [%s]: Not writing to job run instance file. Printing current Job Ad:\n%s",missingAttrs.c_str(),info.buffer.c_str());
		return false;
	}

	if(other_ad == NULL) {
		other_ad = job_ad;
		sPrintAd(info.buffer,*other_ad,nullptr,nullptr);
	} else {
		const classad::ClassAd * new_ad =
			copyEpochJobAttrs( job_ad, other_ad, banner_name );
		if( new_ad != NULL ) {
			sPrintAd(info.buffer,*new_ad,nullptr,nullptr);
			delete new_ad;
		} else {
			sPrintAd(info.buffer,*other_ad,nullptr,nullptr);
		}
	}

	//Buffer contains just the ad at this point
	//Check buffer for newline char at end if no newline then add one and then add banner to buffer
	std::string banner;
	time_t currentTime = time(NULL); //Get current time to print in banner
	formatstr(banner,"*** %s ClusterId=%d ProcId=%d RunInstanceId=%d Owner=\"%s\" CurrentTime=%lld\n" ,
					 banner_name, info.jid.cluster, info.jid.proc, info.runId, owner.c_str(), (long long)currentTime);
	if (info.buffer.back() != '\n') { info.buffer += '\n'; }
	info.buffer += std::string(ATTR_JOB_EPOCH_WRITE_DATE) + " = " + std::to_string(currentTime) + "\n";
	info.buffer += banner;
	if (info.buffer.empty())
		return false;
	else
		return true;
}

//--------------------------------------------------------------
//                      Write Functions
//--------------------------------------------------------------
/*
*	Write the given epoch ad to the given file after checking if
*	file rotation needs to occur.
*/
static void
writeEpochAdToFile(const HistoryFileRotationInfo& fri, const EpochAdInfo& info, const char* new_path = NULL) {
	//Set priv_condor to allow writing to condor owned locations i.e. spool directory
	TemporaryPrivSentry tps(PRIV_CONDOR);
	//Check if we want to rotate the file
	MaybeRotateHistory(fri, info.buffer.length(), info.file_path.c_str(), new_path);
	//Open file and append Job Ad to it
	int fd = -1;
	const char * errmsg = nullptr;
	const int   open_flags = O_RDWR | O_CREAT | O_APPEND | _O_BINARY | O_LARGEFILE | _O_NOINHERIT;
#ifdef WIN32
	DWORD err = 0;
	const DWORD attrib =  FILE_ATTRIBUTE_NORMAL;
	const DWORD create_mode = (open_flags & O_CREAT) ? OPEN_ALWAYS : OPEN_EXISTING;
	const DWORD share_mode = FILE_SHARE_READ | FILE_SHARE_WRITE;
	HANDLE hf = CreateFile(info.file_path.c_str(), FILE_APPEND_DATA, share_mode, NULL, create_mode, attrib, NULL);
	if (hf == INVALID_HANDLE_VALUE) {
		err = GetLastError();
	} else {
		fd = _open_osfhandle((intptr_t)hf, open_flags & (_O_TEXT | _O_WTEXT));
		if (fd < 0) {
			// open_osfhandle can sometimes set errno and sometimes _doserrno (i.e. GetLastError()),
			// the only non-windows error code it sets is EMFILE when the c-runtime fd table is full.
			if (errno == EMFILE) {
				err = ERROR_TOO_MANY_OPEN_FILES;
			} else {
				err = _doserrno;
				if (err == NO_ERROR) err = ERROR_INVALID_FUNCTION; // make sure we get an error code
			}
			CloseHandle(hf);
		}
	}
	if (fd < 0) errmsg = GetLastErrorString(err);
#else
	int err = 0;
	fd = safe_open_wrapper_follow(info.file_path.c_str(), open_flags, 0644);
	if (fd < 0) {
		err = errno;
		errmsg = strerror(errno);
	}
#endif
	if (fd < 0) {
		dprintf(D_ALWAYS | D_ERROR,"ERROR (%d): Opening job run instance file (%s): %s\n",
									err, condor_basename(info.file_path.c_str()), errmsg);
		return;
	}

	//Write buffer with job ad and banner to epoch file
	if (write(fd,info.buffer.c_str(),info.buffer.length()) < 0){
		dprintf(D_ALWAYS,"ERROR (%d): Failed to write job ad for job %d.%d run instance %d to file (%s): %s\n",
						  errno, info.jid.cluster, info.jid.proc, info.runId, condor_basename(info.file_path.c_str()), strerror(errno));
		dprintf(D_FULLDEBUG,"Printing Failed Job Ad:\n%s", info.buffer.c_str());
	}
	close(fd);
}

/*
*	Function for shadow to call that will write the current job ad
*	to a job epoch file. Depending on configuration the ad can be 
*	written to various types of epoch files:
*		1. Full Aggregate (Like history file)
*		2. File per cluster.proc in a specified directory
*/
void
writeJobEpochFile(const classad::ClassAd *job_ad, const classad::ClassAd * other_ad, const char * banner_name) {
	//If not initialized then call init function
	if (!epochHistoryIsInitialized) { initJobEpochHistoryFiles(); }
	// If not specified to write epoch files then return
	if (!efi.can_writeAd) { return; }
	//If no Job Ad then log error and return
	if (!job_ad) {
		dprintf(D_ALWAYS | D_ERROR,
			"ERROR: No Job Ad. Not able to write to Job Run Instance File\n");
		return;
	}

	EpochAdInfo info;
	if (extractEpochInfo(job_ad, info, other_ad, banner_name)) {
		//For every ad recording location check/try to write.
		if (efi.EpochHistoryFilename.ptr()) {
			info.file_path = efi.EpochHistoryFilename.ptr();
			writeEpochAdToFile(hri, info);
		}
		if (efi.JobEpochInstDir.ptr()) {
			//TODO: Add per cluster option
			//TODO: "Spooling"
			//Construct file_name and file_path for writing job ad to
			std::string file_name;
			formatstr(file_name,EPOCH_PER_JOB_FILE,info.jid.cluster,info.jid.proc);
			dircat(efi.JobEpochInstDir.ptr(),file_name.c_str(),info.file_path);
			writeEpochAdToFile(dri, info);
		}
	}
}


