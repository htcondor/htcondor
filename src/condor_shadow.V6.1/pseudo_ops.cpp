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
#include "condor_classad.h"
#include "condor_attributes.h"
#include "condor_debug.h"
#include "condor_io.h"
#include "condor_uid.h"
#include "shadow.h"
#include "event_notification.h"
#include "guidance.h"
#include "pseudo_ops.h"
#include "condor_config.h"
#include "exit.h"
#include "filename_tools.h"
#include "basename.h"
#include "nullfile.h"
#include "spooled_job_files.h"
#include "condor_holdcodes.h"
#include "condor_event.h"

#include <filesystem>
#include "manifest.h"
#include "dc_coroutines.h"
#include "checkpoint_cleanup_utils.h"

#include "condor_base64.h"
#include "shortfile.h"
#include "single_provider_syndicate.h"

extern ReliSock *syscall_sock;
extern BaseShadow *Shadow;
extern RemoteResource *thisRemoteResource;
extern RemoteResource *parallelMasterResource;

static void append_buffer_info( std::string &url, const char *method, char const *path );
static int use_append( const char *method, const char *path );
static int use_compress( const char *method, const char *path );
static int use_fetch( const char *method, const char *path );
static int use_local_access( const char *file );

int
pseudo_register_machine_info(char * /* uiddomain */, char * /* fsdomain */, 
							 char * starterAddr, char *full_hostname )
{

	thisRemoteResource->setStarterAddress( starterAddr );
	thisRemoteResource->setMachineName( full_hostname );

		/* For backwards compatibility, if we get this old pseudo call
		   from the starter, we assume we're not going to get the
		   happy new pseudo_begin_execution call.  So, pretend we got
		   it now so we still log execute events and so on, even if
		   it's not as acurate as we'd like.
		*/
	thisRemoteResource->beginExecution();
	return 0;
}


int
pseudo_register_starter_info( ClassAd* ad )
{
	thisRemoteResource->setStarterInfo( ad );
	return 0;
}


int
pseudo_register_job_info(ClassAd* ad)
{
	// The starter sends an update with provisioned resources but no
	// JobState atrribute before the job begins execution.
	// Updates with a JobState attribute occur after the job is running.
	if (thisRemoteResource->getResourceState() == RR_STARTUP && ad->Lookup(ATTR_JOB_STATE)) {
		// We missed a begin_execution syscall, probably due to a disconnect.
		// Do the begin_execution logic now.
		dprintf(D_FULLDEBUG, "Received a register_job_info syscall without a begin_execution syscall. Doing begin_execution logic.\n");
		thisRemoteResource->beginExecution();
	}

	fix_update_ad(*ad);
	Shadow->updateFromStarterClassAd(ad);
	return 0;
}


int
pseudo_begin_execution()
{
	thisRemoteResource->beginExecution();
	return 0;
}


// In rare instances, the ad this function returns will need to be
// deleted by the caller. In those cases, delete_ad will be set to true.
// Otherwise, delete_ad will be set to false and the returned ad should
// not be deleted.
int
pseudo_get_job_info(ClassAd *&ad, bool &delete_ad)
{
	ClassAd * the_ad;
	delete_ad = false;

	the_ad = thisRemoteResource->getJobAd();
	ASSERT( the_ad );

	thisRemoteResource->initFileTransfer();

	Shadow->publishShadowAttrs( the_ad );

	ad = the_ad;

	return 0;
}


int
pseudo_get_user_info(ClassAd *&ad)
{
	static ClassAd* user_ad = NULL;


	if( ! user_ad ) {
			// if we don't have the ClassAd yet, allocate it and fill
			// it in with the info we care about
		user_ad = new ClassAd;

#ifndef WIN32
		user_ad->Assign( ATTR_UID, (int)get_user_uid() );

		user_ad->Assign( ATTR_GID, (int)get_user_gid() );
#endif

	}

	ad = user_ad;
	return 0;
}

// The list of attributes that some (old?) statrers try and incorrectly update
// this table is used to remove or rename them before we process the update ad.
//
typedef struct {
	const char * const updateAttr; // name in update ad
	const char * const newAttr;    // rename to this before processing the update ad, if NULL, delete the attribute
} AttrToAttr;
static const AttrToAttr updateAdBlacklist[] = {
	// Prior to 8.7.2, the starter incorrectly sends JobStartDate each time the job starts
	// but JobStartDate is defined to be the timestamp of the FIRST execution of the job
	// and it is set by the Schedd the first time it makes a shadow, so we want to just delete
	// this attribute.
	{ ATTR_JOB_START_DATE, NULL },
};

void fix_update_ad(ClassAd & update_ad)
{
	// remove or rename attributes in the update ad before we process it.
	for (size_t ii = 0; ii < COUNTOF(updateAdBlacklist); ++ii) {
		ExprTree * tree = update_ad.Remove(updateAdBlacklist[ii].updateAttr);
		if (tree) {
			if (IsDebugLevel(D_MACHINE)) {
				dprintf(D_MACHINE, "Update ad contained '%s=%s' %s it\n",
					updateAdBlacklist[ii].updateAttr, ExprTreeToString(tree),
					updateAdBlacklist[ii].newAttr ? "Renaming" : "Removing"
					);
			}
			if (updateAdBlacklist[ii].newAttr) {
				update_ad.Insert(updateAdBlacklist[ii].newAttr, tree);
			} else {
				delete tree;
			}
		}
	}
}

int
pseudo_job_exit(int status, int reason, ClassAd* ad)
{
	if (thisRemoteResource->getResourceState() == RR_STARTUP) {
		// We missed a begin_execution syscall, probably due to a disconnect.
		// Do the begin_execution logic now.
		dprintf(D_FULLDEBUG, "Received a job_exit syscall without a begin_execution syscall. Doing begin_execution logic.\n");
		thisRemoteResource->beginExecution();
	}

	// reset the reason if less than EXIT_CODE_OFFSET so that
	// an older starter can be made compatible with the newer
	// schedd exit reasons.
	if ( reason < EXIT_CODE_OFFSET ) {
		if ( reason != JOB_EXCEPTION && reason != DPRINTF_ERROR ) {
			reason += EXIT_CODE_OFFSET;
			dprintf(D_SYSCALLS, "in pseudo_job_exit: old starter, reason reset"
				" from %d to %d\n",reason-EXIT_CODE_OFFSET,reason);
		}
	}
	dprintf(D_SYSCALLS, "in pseudo_job_exit: status=%d,reason=%d\n",
			status, reason);

	// Despite what exit.h says, JOB_COREDUMPED is set by the starter only
	// when the job is NOT killed (and left a core file).
	if( reason == JOB_EXITED
		|| reason == JOB_EXITED_AND_CLAIM_CLOSING
		|| reason == JOB_COREDUMPED ) {
		thisRemoteResource->incrementJobCompletionCount();
	}
	fix_update_ad(*ad);
	thisRemoteResource->updateFromStarter( ad );
	thisRemoteResource->resourceExit( reason, status );
	Shadow->updateJobInQueue( U_STATUS );
	return 0;
}

int 
pseudo_job_termination( ClassAd *ad )
{
	bool exited_by_signal = false;
	bool core_dumped = false;
	int exit_signal = 0;
	int exit_code = 0;
	std::string exit_reason;

	if (thisRemoteResource->getResourceState() == RR_STARTUP) {
		// We missed a begin_execution syscall, probably due to a disconnect.
		// Do the begin_execution logic now.
		dprintf(D_FULLDEBUG, "Received a job_termination syscall without a begin_execution syscall. Doing begin_execution logic.\n");
		thisRemoteResource->beginExecution();
	}

	ad->LookupBool(ATTR_ON_EXIT_BY_SIGNAL,exited_by_signal);
	ad->LookupBool(ATTR_JOB_CORE_DUMPED,core_dumped);
	ad->LookupString(ATTR_EXIT_REASON,exit_reason);

	// Only one of these next two exist.
	ad->LookupInteger(ATTR_ON_EXIT_SIGNAL,exit_signal);
	ad->LookupInteger(ATTR_ON_EXIT_CODE,exit_code);

	// This will utilize only the correct arguments depending on if the
	// process exited with a signal or not.
	Shadow->mockTerminateJob( exit_reason, exited_by_signal, exit_code,
		exit_signal, core_dumped );

	return 0;
}


int
pseudo_register_mpi_master_info( ClassAd* ad ) 
{
	char *addr = NULL;

	if( ! ad->LookupString(ATTR_MPI_MASTER_ADDR, &addr) ) {
		dprintf( D_ALWAYS,
				 "ERROR: mpi_master_info ClassAd doesn't contain %s\n",
				 ATTR_MPI_MASTER_ADDR );
		return -1;
	}
	if( ! Shadow->setMpiMasterInfo(addr) ) {
		dprintf( D_ALWAYS, "ERROR: received "
				 "pseudo_register_mpi_master_info for a non-MPI job!\n" );
		free(addr);
		return -1;
	}
	free(addr);
	return 0;
}

/*
If short_path is an absolute path, copy it to full path.
Otherwise, tack the current directory on to the front
of short_path, and copy it to full_path.
Notice that the old shadow kept track of the job as it
moved around, but there is no such notion in this shadow,
so CurrentWorkingDir is replaced with the job's iwd.
*/
 
static void complete_path( const char *short_path, std::string &full_path )
{
	if ( fullpath(short_path) ) {
		full_path = short_path;
	} else {
		formatstr(full_path, "%s%s%s",
						  Shadow->getIwd(),
						  DIR_DELIM_STRING,
						  short_path);
	}
}

/*
This call translates a logical path name specified by a user job
into an actual url which describes how and where to fetch
the file from.  For example, joe/data might become buffer:remote:/usr/joe/data
*/

int pseudo_get_file_info_new( const char *logical_name, char *&actual_url )
{
	std::string remap_list;
	std::string split_dir;
	std::string split_file;
	std::string full_path;
	std::string remap;
	std::string urlbuf;
	const char	*method = NULL;

	dprintf( D_SYSCALLS, "\tlogical_name = \"%s\"\n", logical_name );

	ASSERT( actual_url == NULL );

	/* The incoming logical name might be a simple, relative, or complete path */
	/* We need to examine both the full path and the simple name. */

	filename_split( logical_name, split_dir, split_file );
	complete_path( logical_name, full_path );

	/* Any name comparisons must check the logical name, the simple name, and the full path */

	if(Shadow->getJobAd()->LookupString(ATTR_FILE_REMAPS,remap_list) &&
	  (filename_remap_find( remap_list.c_str(), logical_name, remap ) ||
	   filename_remap_find( remap_list.c_str(), split_file.c_str(), remap ) ||
	   filename_remap_find( remap_list.c_str(), full_path.c_str(), remap ))) {

		dprintf(D_SYSCALLS,"\tremapped to: %s\n",remap.c_str());

		/* If the remap is a full URL, return right away */
		/* Otherwise, continue processing */

		if(strchr(remap.c_str(),':')) {
			dprintf(D_SYSCALLS,"\tremap is complete url\n");
			actual_url = strdup(remap.c_str());
			return 0;
		} else {
			dprintf(D_SYSCALLS,"\tremap is simple file\n");
			complete_path( remap.c_str(), full_path );
		}
	} else {
		dprintf(D_SYSCALLS,"\tnot remapped\n");
	}

	dprintf( D_SYSCALLS,"\tfull_path = \"%s\"\n", full_path.c_str() );

	/* Now, we have a full pathname. */
	/* Figure out what url modifiers to slap on it. */

	if( use_local_access(full_path.c_str()) ) {
		method = "local";
	} else {
		method = "remote";
	}

	if( use_fetch(method,full_path.c_str()) ) {
		urlbuf += "fetch:";
	}

	if( use_compress(method,full_path.c_str()) ) {
		urlbuf += "compress:";
	}

	append_buffer_info(urlbuf,method,full_path.c_str());

	if( use_append(method,full_path.c_str()) ) {
		urlbuf += "append:";
	}

	if (method) {
		urlbuf += method;
		urlbuf += ":";
	}
	urlbuf += full_path;
	actual_url = strdup(urlbuf.c_str());

	dprintf(D_SYSCALLS,"\tactual_url: %s\n",actual_url);

	return 0;
}

static void append_buffer_info( std::string &url, const char *method, char const *path )
{
	std::string buffer_list;
	std::string buffer_string;
	std::string dir;
	std::string file;
	int s,bs,ps;
	int result;

	filename_split(path,dir,file);

	/* Do not buffer special device files, whether local or remote */
	if(!strncmp(path,"/dev/",5)) return;

	/* Get the default buffer setting */
	pseudo_get_buffer_info( &s, &bs, &ps );

	/* Now check for individual file overrides */
	/* These lines have the same syntax as a remap list */

	if(Shadow->getJobAd()->LookupString(ATTR_BUFFER_FILES,buffer_list)) {
		if( filename_remap_find(buffer_list.c_str(),path,buffer_string) ||
			filename_remap_find(buffer_list.c_str(),file.c_str(),buffer_string) ) {

			/* If the file is merely mentioned, turn on the default buffer */
			url += "buffer:";

			/* If there is also a size setting, use that */
			result = sscanf(buffer_string.c_str(),"(%d,%d)",&s,&bs);
			if( result==2 ) url += buffer_string;

			return;
		}
	}

	/* Turn on buffering if the value is set and is not special or local */
	/* In this case, use the simple syntax 'buffer:' so as not to confuse old libs */

	if (s>0 && bs>0 && method && strcmp(method,"local")) {
		url += "buffer:";
	}
}

/* Return true if this JobAd attribute contains this path */

static int attr_list_has_file( const char *attr, const char *path )
{
	char const *file;
	std::string str;

	file = condor_basename(path);

	Shadow->getJobAd()->LookupString(attr,str);
	std::vector<std::string> list = split(str);

	if( contains_withwildcard(list, path) || contains_withwildcard(list, file) ) {
		return 1;
	} else {
		return 0;
	}
}

static int use_append( const char * /* method */, const char *path )
{
	return attr_list_has_file( ATTR_APPEND_FILES, path );
}

static int use_compress( const char * /* method */, const char *path )
{
	return attr_list_has_file( ATTR_COMPRESS_FILES, path );
}

static int use_fetch( const char * /* method */, const char *path )
{
	return attr_list_has_file( ATTR_FETCH_FILES, path );
}

/*
Return the buffer configuration.  If the classad contains nothing,
assume it is zero.
*/

int pseudo_get_buffer_info( int *bytes_out, int *block_size_out, int *prefetch_bytes_out )
{
	int bytes=0, block_size=0;

	Shadow->getJobAd()->LookupInteger(ATTR_BUFFER_SIZE,bytes);
	Shadow->getJobAd()->LookupInteger(ATTR_BUFFER_BLOCK_SIZE,block_size);

	if( bytes<0 ) bytes = 0;
	if( block_size<0 ) block_size = 0;
	if( bytes<block_size ) block_size = bytes;

	*bytes_out = bytes;
	*block_size_out = block_size;
	*prefetch_bytes_out = 0;

	dprintf(D_SYSCALLS,"\tbuffer configuration is bytes=%d block_size=%d\n",bytes, block_size );

	return 0;
}

static int use_local_access( const char *file )
{
	return
		!strcmp(file,"/dev/null") ||
		!strcmp(file,"/dev/zero") ||
		attr_list_has_file( ATTR_LOCAL_FILES, file );
}

int
pseudo_ulog( ClassAd *ad )
{
	// Ignore the event time we were given, use the
	// current time and timezone
	ad->Delete("EventTime");

	ULogEvent *event = instantiateEvent(ad);
	int result = 0;
	char const *critical_error = NULL;
	std::string CriticalErrorBuf;
	int hold_reason_code = 0;
	int hold_reason_sub_code = 0;

	if(!event) {
		std::string add_str;
		sPrintAd(add_str, *ad);
		dprintf(
		  D_ALWAYS,
		  "invalid event ClassAd in pseudo_ulog: %s\n",
		  add_str.c_str());
		return -1;
	}

	if( event->eventNumber == ULOG_REMOTE_ERROR ) {
		RemoteErrorEvent *err = (RemoteErrorEvent *)event;

		if(!err->getExecuteHost() || !*err->getExecuteHost()) {
			//Insert remote host information.
			char *execute_host = NULL;
			thisRemoteResource->getMachineName(execute_host);
			err->execute_host = execute_host;
			free(execute_host);
		}

		if(err->isCriticalError()) {
			formatstr(
			  CriticalErrorBuf,
			  "Error from %s: %s",
			  err->getExecuteHost(),
			  err->getErrorText());

			critical_error = CriticalErrorBuf.c_str();

			hold_reason_code = err->hold_reason_code;
			hold_reason_sub_code = err->hold_reason_subcode;
			if (hold_reason_code == 0) {
				hold_reason_code = CONDOR_HOLD_CODE::StarterError;
			}
		}
	}

	if( !Shadow->uLog.writeEvent( event, ad ) ) {
		std::string add_str;
		sPrintAd(add_str, *ad);
		dprintf(
		  D_ALWAYS,
		  "unable to log event in pseudo_ulog: %s\n",
		  add_str.c_str());
		result = -1;
	}

	if (critical_error) {
		// Let the RemoteResource know that the starter is shutting
		// down and failing to kill it it expected.
		thisRemoteResource->resourceExit( JOB_SHOULD_REQUEUE, -1 );
		Shadow->evictJob(JOB_SHOULD_REQUEUE, critical_error, hold_reason_code, hold_reason_sub_code);
	}

	delete event;
	return result;
}

int
pseudo_get_job_ad( ClassAd* &ad )
{
	RemoteResource *remote;
	if (parallelMasterResource == NULL) {
		remote = thisRemoteResource;
	} else {
		remote = parallelMasterResource;
	}
	ad = remote->getJobAd();
	return 0;
}


int
pseudo_get_job_attr( const char *name, std::string &expr )
{
	RemoteResource *remote;
	if (parallelMasterResource == NULL) {
		remote = thisRemoteResource;
	} else {
		remote = parallelMasterResource;
	}
	ClassAd *ad = remote->getJobAd();

	ExprTree *e = ad->LookupExpr(name);
	if(e) {
		expr = ExprTreeToString(e);
		dprintf(D_SYSCALLS,"pseudo_get_job_attr(%s) = %s\n",name,expr.c_str());
		return 0;
	} else {
		dprintf(D_SYSCALLS,"pseudo_get_job_attr(%s) is UNDEFINED\n",name);
		expr = "UNDEFINED";
		return 0;
	}
}

int
pseudo_set_job_attr( const char *name, const char *expr, bool log )
{
	RemoteResource *remote;
	if (parallelMasterResource == NULL) {
		remote = thisRemoteResource;
	} else {
		remote = parallelMasterResource;
	}
	if(Shadow->updateJobAttr(name,expr,log)) {
		dprintf(D_SYSCALLS,"pseudo_set_job_attr(%s,%s) succeeded\n",name,expr);
		ClassAd *ad = remote->getJobAd();
		ASSERT(ad);
		ad->AssignExpr(name,expr);
		return 0;
	} else {
		dprintf(D_SYSCALLS,"pseudo_set_job_attr(%s,%s) failed\n",name,expr);
		return -1;
	}
}

int
pseudo_constrain( const char *expr )
{
	std::string reqs;
	std::string newreqs;

	dprintf(D_SYSCALLS,"pseudo_constrain(%s)\n",expr);
	dprintf(D_SYSCALLS,"\tchanging AgentRequirements to %s\n",expr);

	if(pseudo_set_job_attr("AgentRequirements",expr)!=0) return -1;
	if(pseudo_get_job_attr("Requirements",reqs)!=0) return -1;

	if(strstr(reqs.c_str(),"AgentRequirements")) {
		dprintf(D_SYSCALLS,"\tRequirements already refers to AgentRequirements\n");
		return 0;
	} else {
		formatstr(newreqs, "(%s) && AgentRequirements", reqs.c_str());
		dprintf(D_SYSCALLS,"\tchanging Requirements to %s\n",newreqs.c_str());
		return pseudo_set_job_attr("Requirements",newreqs.c_str());
	}
}

int pseudo_get_sec_session_info(
	char const *starter_reconnect_session_info,
	std::string &reconnect_session_id,
	std::string &reconnect_session_info,
	std::string &reconnect_session_key,
	char const *starter_filetrans_session_info,
	std::string &filetrans_session_id,
	std::string &filetrans_session_info,
	std::string &filetrans_session_key)
{
	RemoteResource *remote;
	if (parallelMasterResource == NULL) {
		remote = thisRemoteResource;
	} else {
		remote = parallelMasterResource;
	}

	bool rc = remote->getSecSessionInfo(
		starter_reconnect_session_info,
		reconnect_session_id,
		reconnect_session_info,
		reconnect_session_key,
		starter_filetrans_session_info,
		filetrans_session_id,
		filetrans_session_info,
		filetrans_session_key);

	if( !rc ) {
		return -1;
	}
	return 1;
}


//
// Shadow-specific utility functions for dealing with checkpoint notifications.
//
void
deleteCheckpoint( const ClassAd * jobAd, int checkpointNumber ) {
	// Record on disk that this checkpoint attempt failed, so that
	// we won't worry about not being able to entirely delete it.
	std::string jobSpoolPath;
	SpooledJobFiles::getJobSpoolPath(jobAd, jobSpoolPath);
	std::filesystem::path spoolPath(jobSpoolPath);

	std::string manifestName;
	formatstr( manifestName, "_condor_checkpoint_MANIFEST.%.4d", checkpointNumber );
	std::string failureName;
	formatstr( failureName, "_condor_checkpoint_FAILURE.%.4d", checkpointNumber );
	std::error_code errorCode;
	std::filesystem::rename( spoolPath / manifestName, spoolPath / failureName, errorCode );
	if( errorCode.value() != 0 ) {
		// If no MANIFEST file was written, we can't clean up
		// anyway, so it doesn't matter if we didn't rename it.
		dprintf( D_FULLDEBUG,
			"Failed to rename %s to %s on checkpoint upload/validation failure, error code %d (%s)\n",
			(spoolPath / manifestName).string().c_str(),
			(spoolPath / failureName).string().c_str(),
			errorCode.value(), errorCode.message().c_str()
		);

		return;
	}

	// Clean up just this checkpoint attempt.  We don't actually
	// care if it succeeds; condor_preen is back-stopping us.
	//
	std::string checkpointDestination;
	if(! jobAd->LookupString( ATTR_JOB_CHECKPOINT_DESTINATION, checkpointDestination ) ) {
		dprintf( D_ALWAYS,
			"While handling a checkpoint event, could not find %s in job ad, which is required to attempt a checkpoint.\n",
			ATTR_JOB_CHECKPOINT_DESTINATION
		);

		return;
	}

	std::string globalJobID;
	jobAd->LookupString( ATTR_GLOBAL_JOB_ID, globalJobID );
	ASSERT(! globalJobID.empty());
	std::replace( globalJobID.begin(), globalJobID.end(), '#', '_' );

	formatstr( checkpointDestination, "%s/%s/%.4d",
		checkpointDestination.c_str(), globalJobID.c_str(),
		checkpointNumber
	);

	// The clean-up script is entitled to a copy of the job ad,
	// and the only way to give it one is via the filesystem.
	// It's clumsy, but for now, rather than deal with cleaning
	// this job ad file up, just store it in the job's SPOOL.
	// (Jobs with a checkpoint destination set don't transfer
	// anything from SPOOL except for the MANIFEST files, so
	// this won't cause problems even if the .job.ad file is
	// written by the starter before file transfer rather than
	// after.)
	std::string jobAdPath = jobSpoolPath;

	// FIXME: This invocation assumes that it's OK to block here
	// in the shadow until the clean-up attempt is done.  We'll
	// probably need to (refactor it into the cleanup utils and)
	// call spawnCheckpointCleanupProcessWithTimeout() -- or
	// something very similar -- and plumb through an additional
	// option specifying which specific checkpoint to clean-up.
	std::string error;
	manifest::deleteFilesStoredAt( checkpointDestination,
		(spoolPath / failureName).string(),
		jobAdPath,
		error,
		true /* this was a failed checkpoint */
	);

	// It's OK that we don't remove the .job.ad file and the
	// corresponding parent directory after a successful clean-up;
	// we know we'll need them again later, since we aren't
	// deleting all of the job's checkpoints, and the schedd's
	// call to condor_manifest will delete them when job exits
	// the queue.
}


//
// This syscall MUST ignore information it doesn't know how to deal with.
//
// The thinking for this syscall is the following: as Miron asks for more
// metrics, it's highly likely that there will be other event notifications
// that the starter needs to send to the shadow.  Rather than create a
// semantically significant remote syscall for each one, let's just create
// a single one that they can all use.  (As a new syscall, pools won't see
// correct values for the new metrics until both the shadow and the starter
// have been upgraded.)  However, rather than writing a bunch of complicated
// code to determine which version(s) can send what events to which
// version(s), we can just declare and require that this function just
// ignore attributes it doesn't know how to deal with (and not fail if an
// attribute has the wrong type).
//

int
pseudo_event_notification( const ClassAd & ad ) {
	std::string eventType;
	if(! ad.LookupString( "EventType", eventType )) {
		return GENERIC_EVENT_RV_NO_ETYPE;
	}

	ClassAd * jobAd = Shadow->getJobAd();
	ASSERT(jobAd);

	if( eventType == "ActivationExecutionExit" ) {
		thisRemoteResource->recordActivationExitExecutionTime(time(NULL));
	} else if( eventType == "SuccessfulCheckpoint" ) {
		std::string checkpointDestination;
		if(! jobAd->LookupString( ATTR_JOB_CHECKPOINT_DESTINATION, checkpointDestination ) ) {
			dprintf( D_TEST, "Not attempting to clean up checkpoints going to SPOOL.\n" );
			return GENERIC_EVENT_RV_OK;
		}

		int checkpointNumber = -1;
		if( ad.LookupInteger( ATTR_JOB_CHECKPOINT_NUMBER, checkpointNumber ) ) {
			unsigned long numToKeep = 1 + param_integer( "DEFAULT_NUM_EXTRA_CHECKPOINTS", 1 );
			dprintf( D_STATUS | D_VERBOSE, "Checkpoint number %d succeeded, deleting all but the most recent %lu successful checkpoints.\n", checkpointNumber, numToKeep );

			// Before we can delete a checkpoint, it needs to be moved from the
			// job's spool to the job owner's checkpoint-cleanup directory.  So
			// we need to figure out which checkpoint(s) we want to keep and
			// move all of the other ones.

			std::string spoolPath;
			SpooledJobFiles::getJobSpoolPath( jobAd, spoolPath );
			std::filesystem::path spool( spoolPath );
			if(! (std::filesystem::exists(spool) && std::filesystem::is_directory(spool))) {
				dprintf(D_STATUS, "Checkpoint suceeded but job spool directory either doesn't exist or isn't a directory; not trying to clean up old checkpoints.\n" );
				return GENERIC_EVENT_RV_OK;
			}

			std::error_code errCode;
			std::set<long> checkpointsToSave;
			auto spoolDir = std::filesystem::directory_iterator(spool, errCode);
			if( errCode ) {
				dprintf( D_STATUS, "Checkpoint suceeded but job spool directory couldn't be checked for old checkpoints, not trying to clean them up: %d '%s'.\n", errCode.value(), errCode.message().c_str() );
				return GENERIC_EVENT_RV_OK;
			}
			for( const auto & entry : spoolDir ) {
				const auto & stem = entry.path().stem();
				if( starts_with(stem.string(), "_condor_checkpoint_") ) {
					bool success = ends_with(stem.string(), "MANIFEST");
					bool failure = ends_with(stem.string(), "FAILURE");
					if( success || failure ) {
						char * endptr = NULL;
						const auto & suffix = entry.path().extension().string().substr(1);
						long manifestNumber = strtol( suffix.c_str(), & endptr, 10 );
						if( endptr == suffix.c_str() || *endptr != '\0' ) {
							dprintf( D_FULLDEBUG, "Unable to extract checkpoint number from '%s', skipping.\n", entry.path().string().c_str() );
							continue;
						}

						if( success ) {
							checkpointsToSave.insert(manifestNumber);
							if( checkpointsToSave.size() > numToKeep ) {
								checkpointsToSave.erase(checkpointsToSave.begin());
							}
						}
					}
				}
			}

			std::string buffer;
			formatstr( buffer, "Last %lu succesful checkpoints were: ", numToKeep );
			for( const auto & value : checkpointsToSave ) {
				formatstr_cat( buffer, "%ld ", value );
			}
			dprintf( D_STATUS | D_VERBOSE, "%s\n", buffer.c_str() );

			// Move all but checkpointsToSave's MANIFEST/FAILURE files.  The
			// schedd won't interrupt because it never does anything to the
			// job's spool while the shadow is running, and we won't be moving
			// (or removing) the shared intermediate directories: the schedd
			// will clean those up when the job leaves the queue, as normal.

			int cluster, proc;
			jobAd->LookupInteger( ATTR_CLUSTER_ID, cluster );
			jobAd->LookupInteger( ATTR_PROC_ID, proc );
			if(! moveCheckpointsToCleanupDirectory(
				cluster, proc, jobAd, checkpointsToSave
			)) {
				// Then there's nothing we can do here.
				return GENERIC_EVENT_RV_OK;
			}

			int CLEANUP_TIMEOUT = param_integer( "SHADOW_CHECKPOINT_CLEANUP_TIMEOUT", 300 );
			spawnCheckpointCleanupProcessWithTimeout( cluster, proc, jobAd, CLEANUP_TIMEOUT );
		}
	} else if( eventType == "FailedCheckpoint" ) {
		int checkpointNumber = -1;
		if( ad.LookupInteger( ATTR_JOB_CHECKPOINT_NUMBER, checkpointNumber ) ) {
			dprintf( D_STATUS, "Checkpoint number %d failed, deleting it and updating schedd.\n", checkpointNumber );

			deleteCheckpoint( jobAd, checkpointNumber );

			// If the checkpoint upload succeeded, we have an on-disk (in
			// SPOOL) record of its success, and we'll find whether or not the
			// next iteration of the starter has the most-recent checkpoint
			// number.  Howver, if the checkpoint upload failed hard enough,
			// we may not have a failure manifest, so we should make sure the
			// next iteration of the starter doesn't attempt to overwrite an
			// existing checkpoint (and fail as a result).
			jobAd->Assign( ATTR_JOB_CHECKPOINT_NUMBER, checkpointNumber );
			Shadow->updateJobInQueue( U_PERIODIC );
		}
	} else if( eventType == "InvalidCheckpointDownload" ) {
		int checkpointNumber = -1;
		if(! ad.LookupInteger( ATTR_JOB_CHECKPOINT_NUMBER, checkpointNumber )) {
			dprintf( D_ALWAYS, "Starter sent an InvalidCheckpointDownload event notification, but the job has no checkpoint number; ignoring.\n" );
			return GENERIC_EVENT_RV_INCOMPLETE;
		}

		//
		// The starter can't just restart the job from scratch, because
		// the sandbox may have been modified by the attempt to transfer
		// the checkpoint -- we don't require that our plug-ins are
		// all-or-nothing.
		//
		dprintf( D_STATUS, "Checkpoint number %d was invalid, rolling back.\n", checkpointNumber );

		//
		// We should absolutely write an event to job's event log here
		// noting that the checkpoint was invalid and that we're rolling
		// back, but I'd have to think about what that event should
		// look like.
		//

		//
		// This moves the MANIFEST file out of the way, so that  even if
		// the deletion proper fails, we won't try to use the checkpoint
		// again.
		//
		// Arguably, we should distinguish between storage and retrieval
		// failures when we move the MANIFEST file, but I think it makes
		// sense to accept not being able to fully-delete a corrupt checkpoint.
		//
		deleteCheckpoint( jobAd, checkpointNumber );

		// Do NOT set the checkpoint number.  Either the most-recent
		// checkpoint failed to validate, in which case it's redundant,
		// or an earlier one did, and we don't want to cause an
		// upload failure later by overwriting it.  (Checkpoint numbers
		// should be monotonically increasing.)

		//
		// If it becomes necessary, the shadow could go into its
		// exit-to-requeue routine here.
		//
		return GENERIC_EVENT_RV_OK;
	} else if( eventType == ETYPE_DIAGNOSTIC_RESULT ) {
		dprintf( D_ALWAYS, "Received diagnostic result.\n" );

		std::string diagnostic;
		if(! ad.LookupString( ATTR_DIAGNOSTIC, diagnostic )) {
			dprintf( D_ALWAYS, "Starter sent a diagnostic result, but did not name which diagnostic; ignoring.\n" );
			return GENERIC_EVENT_RV_INCOMPLETE;
		}

		std::string result;
		if(! ad.LookupString( "Result", result ) ) {
			dprintf( D_ALWAYS, "Starter sent a diagnostic result for '%s', but it had no result.\n", diagnostic.c_str() );
			return GENERIC_EVENT_RV_INCOMPLETE;
		}

		if( result != "Completed" ) {
			dprintf( D_ALWAYS, "Diagnostic '%s' did not complete: '%s'\n",
				diagnostic.c_str(), result.c_str()
			);
			return GENERIC_EVENT_RV_OK;
		}


		int exitStatus;
		if(! ad.LookupInteger( "ExitStatus", exitStatus ) ) {
			dprintf( D_ALWAYS, "Starter sent a completed diagnostic result for '%s', but it had no exit status.\n", diagnostic.c_str() );
			return GENERIC_EVENT_RV_INCOMPLETE;
		}

		std::string contents;
		if(! ad.LookupString( "Contents", contents ) ) {
			dprintf( D_ALWAYS, "Starter sent a completed diagnostic result for '%s', but it had no contents.\n", diagnostic.c_str() );
			return GENERIC_EVENT_RV_INCOMPLETE;
		}

		int decoded_bytes = 0;
		unsigned char * decoded = NULL;
		condor_base64_decode( contents.c_str(), & decoded, & decoded_bytes, false );
		if( decoded == NULL ) {
			dprintf( D_ALWAYS, "Failed to decode contents of diagnostic result for '%s'.\n", diagnostic.c_str() );
			return GENERIC_EVENT_RV_INVALID;
		}
		decoded[decoded_bytes] = '\0';

		if( exitStatus != 0 ) {
			dprintf( D_ALWAYS, "Starter sent a completed diagnostic result for '%s', but its exit status was non-zero (%d)\n", diagnostic.c_str(), exitStatus );
			dprintf( D_FULLDEBUG, "Output to first NUL follows: '%s'\n", decoded );
			return GENERIC_EVENT_RV_OK;
		}

		if( diagnostic != DIAGNOSTIC_SEND_EP_LOGS ) {
			dprintf( D_ALWAYS, "Starter sent an unexpected diagnostic result (for '%s'); ignoring.\n", diagnostic.c_str() );
			return GENERIC_EVENT_RV_CONFUSED;
		}

#ifdef TODDT_SAID_OTHERWISE
		// Write `decoded` to a well-known location.  We should probably
		// add a job-ad attribute which controls the location.
		std::string jobIWD;
		if( jobAd->LookupString( ATTR_JOB_IWD, jobIWD ) ) {
			std::filesystem::path iwd(jobIWD);
			std::filesystem::path diagnostic_dir( iwd / ".diagnostic" );

			if(! std::filesystem::exists( diagnostic_dir )) {
				std::error_code ec;
				std::filesystem::create_directory( diagnostic_dir, ec );
			}

			// I'm thinking that any logging here would be uninteresting.
			long long int clusterID = 0, procID = 0, numJobStarts = 0;
			jobAd->LookupInteger( ATTR_CLUSTER_ID, clusterID );
			jobAd->LookupInteger( ATTR_PROC_ID, procID );
			jobAd->LookupInteger( ATTR_NUM_JOB_STARTS, numJobStarts );

			std::string name;
			formatstr( name, "%s.%lld.%lld.%lld", diagnostic.c_str(), clusterID, procID, numJobStarts );
			std::filesystem::path output( diagnostic_dir / name );
			if(! htcondor::writeShortFile( output.string(), decoded, decoded_bytes )) {
				dprintf( D_ALWAYS, "Failed to write output for diagnostic '%s' to %s\n", diagnostic.c_str(), output.string().c_str() );
			}
		} else {
			dprintf( D_ALWAYS, "No IWD in job ad, not writing output for diagnostic '%s'\n", diagnostic.c_str() );
		}
#else

		// Write 'decoded' to the log, one prefixed line at a time.
		for( const auto & line : StringTokenIterator( (char *)decoded, "\n" ) ) {
			dprintf( D_ALWAYS, "[diagnostic log] >>> %s\n", line.c_str() );
		}

#endif

		free( decoded );
		return GENERIC_EVENT_RV_OK;
	} else {
		dprintf( D_ALWAYS, "Ignoring unknown event type '%s'\n", eventType.c_str() );
	}

	return GENERIC_EVENT_RV_UNKNOWN;
}


//
// The ClassAds are all deliberately copies, because it's way easier to be
// marginally less efficient than to worry about heap object lifetimes
// (especially considering that locals from the caller won't be preserved
// across the co_yield() statements).
//
condor::cr::Piperator<ClassAd, ClassAd>
start_input_transfer_failure_conversation( ClassAd request ) {
	ClassAd guidance;

	// Step one: gather logs.
	guidance.InsertAttr(ATTR_COMMAND, COMMAND_RUN_DIAGNOSTIC);
	guidance.InsertAttr(ATTR_DIAGNOSTIC, DIAGNOSTIC_SEND_EP_LOGS);
	request = co_yield guidance;

	// Step two: abort the job.
	guidance.Clear();
	guidance.InsertAttr(ATTR_COMMAND, COMMAND_ABORT);
	co_return guidance;
}


ClassAd
UniShadow::do_common_file_transfer(
	const ClassAd & /* request */,
	const std::string & commonInputFiles,
	const std::string & cifName
) {
	ClassAd commonAd;
	CopyAttribute(
		ATTR_JOB_IWD, commonAd,
		ATTR_JOB_IWD, * Shadow->getJobAd()
	);
	commonAd.InsertAttr( ATTR_TRANSFER_INPUT_FILES, commonInputFiles );
	// If we neglect to set ATTR_TRANSFER_EXECUTABLE, the FTO adds the
	// job's (non-existent) spool directory to the input list!
	commonAd.InsertAttr( ATTR_TRANSFER_EXECUTABLE, false );


	this->commonFTO = new FileTransfer();
	if( this->commonFTO == NULL ) {
		// This is bad, but it's consistent with failing to Init().
		EXCEPT( "UniShadow::do_common_file_transfer(): new FileTransfer() failed." );
	}
	// This sets ATTR_TRANSFER_[SOCKET|KEY] in `commonAd` "for us."
	int rval = this->commonFTO->Init( & commonAd, false, PRIV_USER, false );
	if( rval == 0 ) {
		// This is bad, but it's consistent with what we do in RemoteResouce::initFileTransfer().
		EXCEPT( "UniShadow::do_common_file_transfer(): Init() failed." );
	}
	this->commonFTO->setPeerVersion( this->getStarterVersion() );
	// This is broken on Windows, but we really want our timer to fire
	// during the transfer process.  Not clear if the progress function
	// will always be called often enough to be useful for keeping-alive.
	this->commonFTO->SetServerShouldBlock( false );


	ClassAd guidance;
	CopyAttribute(
		ATTR_TRANSFER_SOCKET, guidance,
		ATTR_TRANSFER_SOCKET, commonAd
	);
	CopyAttribute(
		ATTR_TRANSFER_KEY, guidance,
		ATTR_TRANSFER_KEY, commonAd
	);

	guidance.InsertAttr( ATTR_NAME, cifName );
	guidance.InsertAttr( ATTR_COMMAND, COMMAND_STAGE_COMMON_FILES );
	guidance.InsertAttr( ATTR_COMMON_INPUT_FILES, commonInputFiles );
	return guidance;
}


ClassAd
do_wiring_up( const std::string & stagingDir, const std::string & cifName ) {
	ClassAd guidance;

	guidance.InsertAttr( ATTR_NAME, cifName );
	guidance.InsertAttr( ATTR_COMMAND, COMMAND_MAP_COMMON_FILES );
	guidance.InsertAttr( "StagingDir", stagingDir );

	return guidance;
}


bool
LookupBoolInContext( const ClassAd & ad, const std::string & attr, bool & value ) {
	auto * ctx = ad.Lookup( ATTR_CONTEXT_AD );
	const ClassAd * context = dynamic_cast<ClassAd *>(ctx);
	if( context ) {
		return context->LookupBool( attr, value );
	}
	return false;
}


bool
LookupStringInContext( const ClassAd & ad, const std::string & attr, std::string & value ) {
	ClassAd * context = dynamic_cast<ClassAd *>(ad.Lookup( ATTR_CONTEXT_AD ));
	if( context ) {
		return context->LookupString( attr, value );
	}
	return false;
}


// The parameters are copies to simplify thinking about this coroutine.
condor::cr::Piperator<ClassAd, ClassAd>
UniShadow::start_common_input_conversation(
	ClassAd request, std::string commonInputFiles, std::string cifName,
	bool print_waiting /* = true */
) {
	ClassAd guidance;

	//
	//  1. Determine if this shadow the first one to want these common files
	//     on this machine for that submitter: create the semaphore on disk
	//     (O_CREAT | O_EXCL).
	//  2. If not:
	//     (a) If the semaphore's state is READY, go to step 4.
	//     (b) Otherwise, check that the semaphore has been touched recently.
	//         If it hasn't, remove the semaphore and back to step 1.
	//     (c) Start a timer in case the starter never calls us back?
	//     (d) Ask the starter to wait five minutes and ask again.  When
	//         it does, repeat this process.
	//  3. If so, perform common file transfer, touching the semaphore as we
	//     go, so that step (2) will work.  Initially update the state to
	//     STAGING, then to READY when the transfer is complete.
	//  4. Ask the starter to wire up the common files from the previous step
	//     to the current job's input sandbox.
	//  5. Ask the starter to carry on.
	//
	//  Obviously, it would be more efficient to the regular input transfer
	//  while waiting for the common file transfer, but we'll optimize that
	//  later: strictly speaking, files later in the input list overwrite
	//  files earlier in it, so we'll either have to settle for some semantic
	//  breakage or figure how to avoid it / tolerate unwritable files during
	//  normal transfer.
	//

	this->cfLock = new SingleProviderSyndicate(cifName);

	bool success;
	while( true ) {
		std::string message;
		auto status = this->cfLock->acquire( message );
		dprintf( D_ZKM, "start_common_input_conversation(): cfLock.acquire() = %d\n", (int)status );
		switch( status ) {
			case SingleProviderSyndicate::PROVIDER: {
				// Before we start common file transfer, start a timer that
				// calls this->cfLock->touch() every sixty seconds.
				this->producer_keep_alive = daemonCore->Register_Timer(
					0, 60,
					[this] (int /* timerID */) -> void {
						dprintf( D_TEST, "Elected producer touch()ing keyfile.\n" );
						if(! this->cfLock->touch()) {
							delete this->cfLock;
							EXCEPT( "Elected producer touch() failed, aborting.\n" );
						}
					},
					"SingleProviderSyndicate producer keep-alive"
				);
				if( this->producer_keep_alive == -1 ) {
					delete this->cfLock;
					EXCEPT( "Elected producer couldn't register keep-alive, aborting.\n" );
				}


				//
				// FIXME: The provider process should poll release() once the
				// job is done until it returns true, then exit.  We don't call
				// release() from the consumer processes because their release()
				// and destructor calls are identical; thus, it's better to
				// have the destructor handle it, rather than miss places
				// where release() needs to be called and have it magically
				// work anyway.  This works for shadow reuse because that
				// explicitly delete the shadow object in recycleShadow().
				//
				// The above only applies if the starter asks for guidance
				// after the job is done and can therefore be instructed
				// to prolong its life.
				//

				dprintf( D_ALWAYS, "Starting common files transfer.\n" );
				CommonFilesEvent cfe1;
				cfe1.setType( CommonFilesEventType::TransferStarted );
				uLog.writeEvent( & cfe1, jobAd );

				guidance = this->do_common_file_transfer(request, commonInputFiles, cifName);
				request = co_yield guidance;

				// See the RTYPE_JOB_ENVIRONMENT code for what we should
				// double-check here.

				success = false;
				LookupBoolInContext( request, ATTR_RESULT, success );

				CommonFilesEvent cfe2;
				cfe2.setType( CommonFilesEventType::TransferFinished );
				uLog.writeEvent( & cfe2, jobAd );
				dprintf( D_ALWAYS, "Finished common files transfer: %s.\n", success ? "success" : "failure" );

				std::string stagingDir;
				if( success ) {
					// We'll assume that malformed replies are transients.
					if(! LookupStringInContext( request, "StagingDir", stagingDir )) {
						dprintf( D_ALWAYS, "UniShadow::start_common_input_conversation(): malformed reply to doing common files transfer; aborting job.\n" );

						// Make sure we take care of this.
						delete this->cfLock;
						this->cfLock = NULL;

						// Consider replacing this with a call to evictJob().
						this->jobAd->Assign(ATTR_LAST_VACATE_TIME, time(nullptr));
						this->jobAd->Assign(ATTR_VACATE_REASON, "Starter sent malformed reply when asked to stage common files." );
						this->jobAd->Assign(ATTR_VACATE_REASON_CODE, CONDOR_HOLD_CODE::JobNotStarted);
						this->jobAd->Assign(ATTR_VACATE_REASON_SUBCODE, 1);
						remRes->setExitReason(JOB_SHOULD_REQUEUE);
						remRes->killStarter(false);

						// We can't "wire up" the CIFs; since the job needs
						// them, we must abort it.
						guidance.Clear();
						guidance.InsertAttr(ATTR_COMMAND, COMMAND_ABORT);
						co_return guidance;
					}

					dprintf( D_TEST, "Staging successful, calling ready(%s)\n", stagingDir.c_str() );
					if(! this->cfLock->ready( stagingDir )) {
						// We failed to tell the jobs waiting on us that we're
						// ready.  This job can continue, but the others can't.
						//
						// We can't just call release(), because that doesn't
						// do anything if other jobs are waiting.  Deleting
						// the keyfile, however, will select a new lockholder.
						delete this->cfLock;
					}
				} else {
					dprintf( D_ALWAYS, "UniShadow::start_common_input_conversation(): common file transfer failed, aborting job.\n" );

					delete this->cfLock;
					this->cfLock = NULL;

					const FileTransfer::FileTransferInfo info = this->commonFTO->GetInfo();
					if( info.try_again ) {
						// Consider replacing this with a delayed (zero-second
						// timer) call to evictJob().
						this->jobAd->Assign(ATTR_LAST_VACATE_TIME, time(nullptr));
						this->jobAd->Assign(ATTR_VACATE_REASON, "Failed to transfer common files." );
						this->jobAd->Assign(ATTR_VACATE_REASON_CODE, CONDOR_HOLD_CODE::JobNotStarted);
						this->jobAd->Assign(ATTR_VACATE_REASON_SUBCODE, 2);
						remRes->setExitReason(JOB_SHOULD_REQUEUE);
						remRes->killStarter(false);
					} else {
						holdJob( "Failed to transfer common files.", CONDOR_HOLD_CODE::JobNotStarted, 3 );
					}

					guidance.Clear();
					guidance.InsertAttr(ATTR_COMMAND, COMMAND_ABORT);
					co_return guidance;
				}

				guidance = do_wiring_up(stagingDir, cifName);
				request = co_yield guidance;

				success = false;
				LookupBoolInContext( request, ATTR_RESULT, success );
				if(! success) {
					// Consider replacing this with a delayed (zero-second
					// timer) call to evictJob().
					this->jobAd->Assign(ATTR_LAST_VACATE_TIME, time(nullptr));
					this->jobAd->Assign(ATTR_VACATE_REASON, "Failed to map files." );
					this->jobAd->Assign(ATTR_VACATE_REASON_CODE, CONDOR_HOLD_CODE::JobNotStarted);
					this->jobAd->Assign(ATTR_VACATE_REASON_SUBCODE, 3);
					remRes->setExitReason(JOB_SHOULD_REQUEUE);
					remRes->killStarter(false);

					guidance.Clear();
					guidance.InsertAttr(ATTR_COMMAND, COMMAND_ABORT);
					co_return guidance;
				}


				//
				// TODO.
				//
				// For maximum efficiency, set a flag so that the shadow
				// doesn't exit until this->cfLock->release() returns true.
				// This will require the starter to ask for advice after
				// the job terminates, because otherwise _it_ will go away,
				// and the point is to keep it alive.  (The flag will be
				// checked in the corresponding guidance function.)
				//
				// Once the slot-splitting function is available, that
				// guidance function is where it will be called when the
				// job is done (and the resulting claim conveyed to the
				// schedd).
				//

				guidance.Clear();
				guidance.InsertAttr(ATTR_COMMAND, COMMAND_CARRY_ON);
				co_return guidance;
				}
				break;

			case SingleProviderSyndicate::UNREADY: {
				// The common files are being transferred by someone else.
				//
				// We'd like to do input transfer while we wait.  Rather
				// than duplicate all that code -- and its error handling --
				// let's just have the starter call us back after input
				// file transfer -- which it knows how to do -- and do
				// whatever waiting remains then.
				//
				// We can therefore only send COMMAND_CARRY_ON if we're
				// sure the starter has already set up the job environment.

				bool job_environment_ready = false;
				LookupBoolInContext(
					request, ATTR_JOB_ENVIRONMENT_READY, job_environment_ready
				);
				if(! job_environment_ready) {
					this->resume_job_setup = true;
					guidance.InsertAttr(ATTR_COMMAND, COMMAND_CARRY_ON);
					co_return guidance;
				} else {
					if( print_waiting ) {
						print_waiting = false;
						dprintf( D_ALWAYS, "Waiting for common files to be transferred (%s)\n", cifName.c_str() );

						CommonFilesEvent cfe;
						cfe.setType( CommonFilesEventType::WaitStarted );
						uLog.writeEvent( & cfe, jobAd );
					}
					guidance.InsertAttr(ATTR_COMMAND, COMMAND_RETRY_REQUEST);
					guidance.InsertAttr(ATTR_RETRY_DELAY, 5);
					// It's a little clumsy to send the starter's own context
					// ad back to it, but it's also easier than preserving it
					// across invocations over there.
					CopyAttribute(ATTR_CONTEXT_AD, guidance, request);
					request = co_yield guidance;
				}
				} break;

			case SingleProviderSyndicate::READY:
				// For testing purposes only.
				// guidance.InsertAttr(ATTR_COMMAND, COMMAND_RETRY_REQUEST);
				// guidance.InsertAttr(ATTR_RETRY_DELAY, 5);
				// request = co_yield guidance;

				// Map the common files into the sandbox.
				guidance = do_wiring_up(message, cifName);
				request = co_yield guidance;

				success = false;
				LookupBoolInContext( request, ATTR_RESULT, success );
				if(! success) {
					// Consider replacing this with a delayed (zero-second
					// timer) call to evictJob().
					this->jobAd->Assign(ATTR_LAST_VACATE_TIME, time(nullptr));
					this->jobAd->Assign(ATTR_VACATE_REASON, "Failed to map files." );
					this->jobAd->Assign(ATTR_VACATE_REASON_CODE, CONDOR_HOLD_CODE::JobNotStarted);
					this->jobAd->Assign(ATTR_VACATE_REASON_SUBCODE, 3);
					remRes->setExitReason(JOB_SHOULD_REQUEUE);
					remRes->killStarter(false);

					guidance.Clear();
					guidance.InsertAttr(ATTR_COMMAND, COMMAND_ABORT);
					co_return guidance;
				}

				guidance.Clear();
				guidance.InsertAttr(ATTR_COMMAND, COMMAND_CARRY_ON);
				co_return guidance;
				break;

			case SingleProviderSyndicate::INVALID:
				EXCEPT("Something went terribly wrong in cfLock.acquire()." );
				break;

			case SingleProviderSyndicate::MIN:
				EXCEPT("Invalid return value (MIN) from cfLock.acquire()." );
				break;

			case SingleProviderSyndicate::MAX:
				EXCEPT("Invalid return value (MAX) from cfLock.acquire()." );
				break;
		}
	}
}



extern bool use_guidance_in_job_ad;

GuidanceResult
send_guidance_from_job_ad( const ClassAd & /* request */, ClassAd & guidance ) {
	ExprTree * the_test_case = Shadow->getJobAd()->LookupExpr( "_condor_guidance_test_case" );
	if( the_test_case == NULL ) {
		guidance.InsertAttr(ATTR_COMMAND, COMMAND_CARRY_ON );
		return GuidanceResult::Command;
	}

	classad::ExprList * test_case_list = dynamic_cast<classad::ExprList *>(the_test_case);
	if( test_case_list == NULL ) {
		guidance.InsertAttr(ATTR_COMMAND, COMMAND_CARRY_ON );
		return GuidanceResult::Command;
	}

	std::vector<ExprTree *> the_list;
	test_case_list->GetComponents(the_list);

	static int the_index = 0;
	ExprTree * the_guidance = the_list[the_index++];
	ClassAd * the_classad = dynamic_cast<ClassAd *>(the_guidance);
	if( the_classad == NULL ) {
		guidance.InsertAttr(ATTR_COMMAND, COMMAND_CARRY_ON );
		return GuidanceResult::Command;
	}
	guidance = * the_classad;
	return GuidanceResult::Command;
}


//
// This syscall MUST ignore information it doesn't know how to deal with.
//


GuidanceResult
BaseShadow::pseudo_request_guidance( const ClassAd & /* request */, ClassAd & guidance ) {
	dprintf( D_ALWAYS, "Internal error: BaseShadow::pseudo_request_guidance() called.\n" );
	guidance.InsertAttr( ATTR_COMMAND, COMMAND_CARRY_ON );
	return GuidanceResult::Command;
}


GuidanceResult
UniShadow::pseudo_request_guidance( const ClassAd & request, ClassAd & guidance ) {
	dprintf( D_ZKM, "Received request for guidance.\n" );

	if( param_boolean( "GUIDANCE_KEEP_CALM_AND", false ) ) {
		dprintf( D_ALWAYS, "Keep calm and (always send the command) %s\n", COMMAND_CARRY_ON );
		guidance.InsertAttr( ATTR_COMMAND, COMMAND_CARRY_ON );
		return GuidanceResult::Command;
	}

	std::string requestType;
	if(! request.LookupString( ATTR_REQUEST_TYPE, requestType )) {
		return GuidanceResult::MalformedRequest;
	}

	if( requestType == RTYPE_JOB_ENVIRONMENT ) {
		dprintf( D_ZKM, "Received request for guidance about the job environment.\n" );

		// There's no reason for this to be exclusive to the job environment
		// guidance, but to unbreak the test, let's pretend it is.  (The
		// in-job guidance would have to specify the request type.)
		if( use_guidance_in_job_ad ) {
			dprintf( D_TEST, "Using guidance in job ad.\n" );
			return send_guidance_from_job_ad( request, guidance );
		}

		if( thisRemoteResource->download_transfer_info.xfer_status == XFER_STATUS_UNKNOWN ) {
			// This isn't copied into thisRemoteResource->download_transfer_info
			// until the FTO reaper fires, which might be a a while.
			const FileTransfer::FileTransferInfo current = thisRemoteResource->filetrans.GetInfo();
			if ( current.xfer_status == XFER_STATUS_ACTIVE ) {
				// We haven't gotten an update from our side's FTO yet.  What
				// I would like to do is go back into the event loop and re-
				// check after a few seconds, but the syscall socket code is
				// _very_ synchronous.  Instead, since we'll want to have
				// this command for other purposes later, ask the starter to
				// call us back in a few seconds.
				//
				// Because we're now checking the live copy of the FTO status
				// variables, this should never happen, but since we have a
				// coded work-around and it's still at least possible, let's
				// leave it in.
				guidance.InsertAttr(ATTR_COMMAND, COMMAND_RETRY_REQUEST);
				guidance.InsertAttr(ATTR_RETRY_DELAY, 5);
				// guidance.InsertAttr(ATTR_CONTEXT_AD, context);
			} else if( current.xfer_status == XFER_STATUS_DONE
			 && current.success == true
			) {
				if(! this->resume_job_setup) {
					guidance.InsertAttr(ATTR_COMMAND, COMMAND_START_JOB);
				} else {
					this->resume_job_setup = false;
					guidance.InsertAttr(ATTR_COMMAND, COMMAND_JOB_SETUP);
				}
			} else if (
				current.xfer_status == XFER_STATUS_DONE
			 && current.success == false
			) {
				if( current.try_again == true ) {
					// I'm not sure this case ever actually happens in practice,
					// but in case it does, this seems like the right thing to do.
					guidance.InsertAttr(ATTR_COMMAND, COMMAND_RETRY_TRANSFER);
				} else {
					//
					// It's massive overkill for a simple two-step protocol, but
					// let's make this a coroutine so that we can easily make it
					// more complicated.
					//
					// Since we're only talking to one starter at a time, we can
					// simply record if we've already started this conversation.
					//
					static bool in_conversation = false;
					static condor::cr::Piperator<ClassAd, ClassAd> the_coroutine;

					if(! in_conversation) {
						in_conversation = true;
						the_coroutine = std::move(
							start_input_transfer_failure_conversation(request)
						);
						guidance = the_coroutine();
					} else {
						the_coroutine.set_co_yield_value( request );
						guidance = the_coroutine();
					}

					if( the_coroutine.handle.done() ) {
						in_conversation = false;
					}
				}
			} else {
				guidance.InsertAttr( ATTR_COMMAND, COMMAND_CARRY_ON );
			}
		} else {
			// The starter asked for guidance about the job environment, but
			// we think we're doing output transfer.
			guidance.InsertAttr( ATTR_COMMAND, COMMAND_CARRY_ON );
		}

		std::string command;
		guidance.LookupString(ATTR_COMMAND, command);
		dprintf( D_ZKM, "Sending (job environment) guidance with command %s\n", command.c_str());
		return GuidanceResult::Command;
	} else if( requestType == RTYPE_JOB_SETUP ) {
		dprintf( D_ZKM, "Received request for guidance about job setup.\n" );

		// If the AP has decided to stage common input for this job, issue
		// the corresponding command.
		//
		// For milestone 1, we will just check for a job-ad attribute.
		std::string commonInputFiles;
		if(! getJobAd()->LookupString( ATTR_COMMON_INPUT_FILES, commonInputFiles )) {
			guidance.InsertAttr( ATTR_COMMAND, COMMAND_CARRY_ON );
		} else if(! this->hasCIFName()) {
			dprintf( D_ALWAYS, "Unable to determine name for common input files, can't run job!\n" );

			// We don't have a mechanism to inform the submitter of internal
			// errors like this, so for now we're stuck putting the job on hold.
			holdJob( "Unable to determine name for common input files, can't run job.",
				CONDOR_HOLD_CODE::JobNotStarted, 4
			);

			guidance.InsertAttr( ATTR_COMMAND, COMMAND_ABORT );
		} else {
			// This should have been take care of by match-making, but for the
			// first milestone, that has to be done by hand and might have been
			// forgotten.  I'd like to check the slot ad, but the shadow doesn't
			// have access to that; instead, the starter will send along the
			// version number in the request ad.
			//
			// This doesn't help if the starter is too old to even ask for
			// guidance, but there's nothing we can do about that if the
			// shadow doesn't have access to the slot ad.
			int hasCommonFilesTransfer = 0;
			request.LookupInteger(
				ATTR_HAS_COMMON_FILES_TRANSFER, hasCommonFilesTransfer
			);
			if( hasCommonFilesTransfer < 1 ) {
				// Put the job on hold with a request to add the requirement.
				holdJob("Please add TARGET.HasCommonFilesTransfer to your requirements expression.", 1003, 5 );

				guidance.InsertAttr(ATTR_COMMAND, COMMAND_ABORT);
				return GuidanceResult::Command;
			}


			//
			// Since we're only talking to one starter at a time, we can
			// simply record if we've already started this conversation.
			//
			static bool in_conversation = false;
			static condor::cr::Piperator<ClassAd, ClassAd> the_coroutine;

			if(! in_conversation) {
				in_conversation = true;
				the_coroutine = std::move(
					this->start_common_input_conversation(request, commonInputFiles, this->getCIFName())
				);
				guidance = the_coroutine();
			} else {
				the_coroutine.set_co_yield_value( request );
				guidance = the_coroutine();
			}

			if( the_coroutine.handle.done() ) {
				in_conversation = false;
			}
		}

		std::string command;
		guidance.LookupString(ATTR_COMMAND, command);
		dprintf( D_TEST, "Sending (job setup) guidance with command %s\n", command.c_str());
		return GuidanceResult::Command;
	}

	return GuidanceResult::UnknownRequest;
}


bool
UniShadow::hasCIFName() {
	if(! _cifNameInitialized) {
		getCIFName();
		return hasCIFName();
	}
	return _cifNameInitialized && (! _cifName.empty());
}


const std::string &
UniShadow::getCIFName() {
	if(! _cifNameInitialized) {
		char * startdAddress = NULL;
		this->remRes->getStartdAddress(startdAddress);
		auto cifName = makeCIFName(* this->jobAd, startdAddress);
		free( startdAddress );
		if( cifName ) { _cifName = * cifName; }
		_cifNameInitialized = true;
	}
	return _cifName;
}
