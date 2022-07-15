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
#include "directory.h" // for StatInfo
#include "directory_util.h"
#include "job_ad_instance_recording.h"

//--------------------------------------------------------------
//                       Data members
//--------------------------------------------------------------
//TODO: Make a structor to hold all this data
static bool file_per_epoch = false;
static bool	isInitialized = false;
static char *JobEpochInstDir = NULL;

//--------------------------------------------------------------
//                     Helper Functions
//--------------------------------------------------------------

/*
*	Function to set the job epoch directory from configuration variable
*	JOB_EPOCH_INSTANCE_DIR if it is set and a valid directory.
*/
static void
setEpochDir(){
	if (JobEpochInstDir != NULL) free(JobEpochInstDir);
	// Attempt to grab job epoch instance directory
	if ((JobEpochInstDir = param("JOB_EPOCH_INSTANCE_DIR")) != NULL) {
		// If param was found and not null check if it is a valid directory
		StatInfo si(JobEpochInstDir);
        if (!si.IsDirectory()) {
			// Not a valid directory. Log message and set data member to NULL
            dprintf(D_ALWAYS | D_ERROR,
                    "Invalid JOB_EPOCH_INSTANCE_DIR (%s): must point to a "
                    "valid directory; disabling per-job run instance recording.\n",
                    JobEpochInstDir);
            free(JobEpochInstDir);
            JobEpochInstDir = NULL;
        } else {
            dprintf(D_ALWAYS,
                    "Writing per-job run instance recording files to: %s\n",
                    JobEpochInstDir);
        }
	}
	isInitialized = true;
}

//--------------------------------------------------------------
//                      Write Functions
//--------------------------------------------------------------
/*
*	Write/Append job ad to one file per job with banner seperation
*
*	TODO:Do we want file size limits and file rotation for appending?
*/
static void
appendJobEpochFile(const classad::ClassAd *job_ad){
	// If not initialized then set up job epoch directory
	if (!isInitialized) { setEpochDir(); }
	// Verify that a directory is set if not return
	if (!JobEpochInstDir) { return; }
	
	//Get various information needed for writing epoch file and banner
	int clusterId, procId, numShadow;
	std::string owner, missingAttrs;
	
	if (!job_ad->LookupInteger("ClusterId", clusterId)) {
		clusterId = -1;
		missingAttrs += "ClusterId";
	}
	if (!job_ad->LookupInteger("ProcId", procId)) {
		procId = -1;
		if (!missingAttrs.empty()) { missingAttrs += ',';}
		missingAttrs += "ProcId";
	}
	if (!job_ad->LookupInteger("NumShadowStarts", numShadow)) {
		//TODO: Replace Using NumShadowStarts with a better counter for epoch
		numShadow = -1;
		if (!missingAttrs.empty()) { missingAttrs += ',';}
		missingAttrs += "NumShadowStarts";
	}
	if (!job_ad->LookupString("Owner", owner)) {
		owner = "?";
	}
	
	//TODO:Until better Job Attribute is added, decrement numShadow to start RunInstanceId at 0
	numShadow--;
	
	//Write job ad to a buffer
	std::string buffer;
	sPrintAd(buffer,*job_ad,nullptr,nullptr);
	//If any attributes are set to -1 write to shadow log and return
	if (clusterId < 0 || procId < 0 || numShadow < 0){
		dprintf(D_FULLDEBUG,"Missing attribute(s) [%s]: Not writing to job run instance file. Printing current Job Ad:\n%s",missingAttrs.c_str(),buffer.c_str());
		return;
	}
	
	//Construct file_name and file_path for writing job ad to
	std::string file_name,file_path;
	formatstr(file_name,"job.runs.%d.%d.ads",clusterId,procId);
	dircat(JobEpochInstDir,file_name.c_str(),file_path);

	//Open file and append Job Ad to it
	int fd = -1;
	const char * errmsg = nullptr;
	const int   open_flags = O_RDWR | O_CREAT | O_APPEND | _O_BINARY | O_LARGEFILE | _O_NOINHERIT;
#ifdef WIN32
	DWORD err = 0;
	const DWORD attrib =  FILE_ATTRIBUTE_NORMAL;
	const DWORD create_mode = (open_flags & O_CREAT) ? OPEN_ALWAYS : OPEN_EXISTING;
	const DWORD share_mode = FILE_SHARE_READ | FILE_SHARE_WRITE;
	HANDLE hf = CreateFile(file_path.c_str(), FILE_APPEND_DATA, share_mode, NULL, create_mode, attrib, NULL);
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
	fd = safe_open_wrapper_follow(file_path.c_str(), open_flags, 0644);
	if (fd < 0) {
		err = errno;
		errmsg = strerror(errno);
	}
#endif
	if (fd < 0) {
		dprintf(D_ALWAYS | D_ERROR,"ERROR (%d): Opening job run instance file (%s): %s",
									err, file_name.c_str(), errmsg);
		return;
	}

	//Buffer contains just the job ad at this point
	//Check buffer for newline char at end if no newline then add one and then add banner to buffer
	std::string banner;
	time_t currentTime = time(NULL); //Get current time to print in banner
	formatstr(banner,"*** ClusterId=%d ProcId=%d RunInstanceId=%d Owner=\"%s\" CurrentTime=%lld\n" ,
					 clusterId, procId, numShadow, owner.c_str(), (long long)currentTime);
	if (buffer.back() != '\n') { buffer += '\n'; }
	buffer += banner;
	//Write buffer with job ad and banner to epoch file
	if (write(fd,buffer.c_str(),buffer.length()) < 0){
		dprintf(D_ALWAYS,"ERROR (%d): Failed to write job ad for job %d.%d run instance %d to file (%s): %s\n",
						  errno, clusterId, procId, numShadow, file_name.c_str(), strerror(errno));
	}
	close(fd);
}

/*
*	Write a single/current job ad to a new and unique file
*	labeled <filler>.cluster-id.proc-id.epoch-number (num_shadow_starts)
*/
//static void
//writeJobEpochInstance(const classad::ClassAd *job_ad){
	//TODO: -Make per instance writing
	//	-Handle massive amounts of files with sub dirs (see spooling
	//	-Max file limits?
//}

/*
*	Function for shadow to call that will write the current job ad
*	to a job epoch file. This functionality will either write 1 file
*	per job and append each epoch instance of the job ad to said file.
*	Or will write 1 job ad to 1 file per job epoch instance
*/
void
writeJobEpochFile(const classad::ClassAd *job_ad) {

	//If no Job Ad then log error and return
	if (!job_ad) {
		dprintf(D_ALWAYS | D_ERROR,
			"ERROR: No Job Ad. Not able to write to Job Run Instance File");
		return;
	}

	if (file_per_epoch) { // Check If user wants 1 file per job epoch/instance
		;//writeJobEpochInstance(job_ad);
	} else { // Otherwise write to single file per job
		appendJobEpochFile(job_ad);
	}
}

//--------------------------------------------------------------
//                   Read/Scrape Functions
//--------------------------------------------------------------


