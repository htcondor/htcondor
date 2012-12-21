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
#include "pseudo_ops.h"
#include "condor_config.h"
#include "exit.h"
#include "filename_tools.h"
#include "basename.h"
#include "nullfile.h"

extern ReliSock *syscall_sock;
extern BaseShadow *Shadow;
extern RemoteResource *thisRemoteResource;
extern RemoteResource *parallelMasterResource;

static void append_buffer_info( MyString &url, const char *method, char const *path );
static int use_append( const char *method, const char *path );
static int use_compress( const char *method, const char *path );
static int use_fetch( const char *method, const char *path );
static int use_local_access( const char *file );
static int use_special_access( const char *file );
static int access_via_afs( const char *file );
static int access_via_nfs( const char *file );

int
pseudo_register_machine_info(char *uiddomain, char *fsdomain, 
							 char * starterAddr, char *full_hostname )
{

	thisRemoteResource->setUidDomain( uiddomain );
	thisRemoteResource->setFilesystemDomain( fsdomain );
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

		// FileTransfer now makes sure we only do Init() once.
		//
		// New for WIN32: want_check_perms = false.
		// Since the shadow runs as the submitting user, we
		// let the OS enforce permissions instead of relying on
		// the pesky perm object to get it right.
		//
		// Tell the FileTransfer object to create a file catalog if
		// the job's files are spooled. This prevents FileTransfer
		// from listing unmodified input files as intermediate files
		// that need to be transferred back from the starter.
	int spool_time = 0;
	the_ad->LookupInteger(ATTR_STAGE_IN_FINISH,spool_time);
	thisRemoteResource->filetrans.Init( the_ad, false, PRIV_USER, spool_time != 0 );

	// Add extra remaps for the canonical stdout/err filenames.
	// If using the FileTransfer object, the starter will rename the
	// stdout/err files, and we need to remap them back here.
	std::string file;
	if ( the_ad->LookupString( ATTR_JOB_OUTPUT, file ) &&
		 strcmp( file.c_str(), StdoutRemapName ) ) {

		thisRemoteResource->filetrans.AddDownloadFilenameRemap( StdoutRemapName, file.c_str() );
	}
	if ( the_ad->LookupString( ATTR_JOB_ERROR, file ) &&
		 strcmp( file.c_str(), StderrRemapName ) ) {

		thisRemoteResource->filetrans.AddDownloadFilenameRemap( StderrRemapName, file.c_str() );
	}

	Shadow->publishShadowAttrs( the_ad );

	ad = the_ad;

	// If we're dealing with an old starter (pre 7.7.2) and file transfer
	// may be used, we need to rename the stdout/err files to
	// StdoutRemapName and StderrRemapName. Otherwise, they won't transfer
	// back correctly if they contain any path information.
	// If we don't know what version the starter is and we know file
	// transfer will be used, do the rename. It won't harm a new starter
	// and allows us to work correctly with old starters in more cases.
	const CondorVersionInfo *vi = syscall_sock->get_peer_version();
	if ( vi == NULL || !vi->built_since_version(7,7,2) ) {
		std::string value;
		ad->LookupString( ATTR_SHOULD_TRANSFER_FILES, value );
		ShouldTransferFiles_t should_transfer = getShouldTransferFilesNum( value.c_str() );

		if ( should_transfer == STF_YES ||
			 ( vi != NULL && should_transfer == STF_IF_NEEDED ) ) {
			ad = new ClassAd( *ad );
			delete_ad = true;

			// This is the same modification a modern starter will do when
			// using file transfer in JICShadow::initWithFileTransfer()
			bool stream;
			std::string stdout_name;
			std::string stderr_name;
			ad->LookupString( ATTR_JOB_OUTPUT, stdout_name );
			ad->LookupString( ATTR_JOB_ERROR, stderr_name );
			if ( ad->LookupBool( ATTR_STREAM_OUTPUT, stream ) && !stream &&
				 !nullFile( stdout_name.c_str() ) ) {
				ad->Assign( ATTR_JOB_OUTPUT, StdoutRemapName );
			}
			if ( ad->LookupBool( ATTR_STREAM_ERROR, stream ) && !stream &&
				 !nullFile( stderr_name.c_str() ) ) {
				if ( stdout_name == stderr_name ) {
					ad->Assign( ATTR_JOB_ERROR, StdoutRemapName );
				} else {
					ad->Assign( ATTR_JOB_ERROR, StderrRemapName );
				}
			}
		}
	}

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
		char buf[1024];

		sprintf( buf, "%s = %d", ATTR_UID, (int)get_user_uid() );
		user_ad->Insert( buf );

		sprintf( buf, "%s = %d", ATTR_GID, (int)get_user_gid() );
		user_ad->Insert( buf );
#endif

	}

	ad = user_ad;
	return 0;
}

int
pseudo_job_exit(int status, int reason, ClassAd* ad)
{
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
	thisRemoteResource->updateFromStarter( ad );
	thisRemoteResource->resourceExit( reason, status );
	return 0;
}

int 
pseudo_job_termination( ClassAd *ad )
{
	bool exited_by_signal = false;
	bool core_dumped = false;
	int exit_signal = 0;
	int exit_code = 0;
	MyString exit_reason;

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
		dprintf( D_ALWAYS, "ERROR: recieved "
				 "pseudo_register_mpi_master_info for a non-MPI job!\n" );
		return -1;
	}
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
 
static void complete_path( const char *short_path, MyString &full_path )
{
	if(short_path[0]==DIR_DELIM_CHAR) {
		full_path = short_path;
	} else {
		// strcpy(full_path,CurrentWorkingDir);
		full_path.formatstr("%s%s%s",
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
	MyString remap_list;
	MyString	split_dir;
	MyString	split_file;
	MyString	full_path;
	MyString	remap;
	MyString urlbuf;
	const char	*method;

	dprintf( D_SYSCALLS, "\tlogical_name = \"%s\"\n", logical_name );

	ASSERT( actual_url == NULL );

	/* The incoming logical name might be a simple, relative, or complete path */
	/* We need to examine both the full path and the simple name. */

	filename_split( logical_name, split_dir, split_file );
	complete_path( logical_name, full_path );

	/* Any name comparisons must check the logical name, the simple name, and the full path */

	if(Shadow->getJobAd()->LookupString(ATTR_FILE_REMAPS,remap_list) &&
	  (filename_remap_find( remap_list.Value(), logical_name, remap ) ||
	   filename_remap_find( remap_list.Value(), split_file.Value(), remap ) ||
	   filename_remap_find( remap_list.Value(), full_path.Value(), remap ))) {

		dprintf(D_SYSCALLS,"\tremapped to: %s\n",remap.Value());

		/* If the remap is a full URL, return right away */
		/* Otherwise, continue processing */

		if(strchr(remap.Value(),':')) {
			dprintf(D_SYSCALLS,"\tremap is complete url\n");
			actual_url = strdup(remap.Value());
			return 0;
		} else {
			dprintf(D_SYSCALLS,"\tremap is simple file\n");
			complete_path( remap.Value(), full_path );
		}
	} else {
		dprintf(D_SYSCALLS,"\tnot remapped\n");
	}

	dprintf( D_SYSCALLS,"\tfull_path = \"%s\"\n", full_path.Value() );

	/* Now, we have a full pathname. */
	/* Figure out what url modifiers to slap on it. */

#ifdef HPUX
	/* I have no idea why this is happening, but I have seen it happen many
	 * times on the HPUX version, so here is a quick hack -Todd 5/19/95 */
	if ( full_path == "/usr/lib/nls////strerror.cat" )
		full_path = "/usr/lib/nls/C/strerror.cat\0";
#endif

	if( use_special_access(full_path.Value()) ) {
		method = "special";
	} else if( use_local_access(full_path.Value()) ) {
		method = "local";
	} else if( access_via_afs(full_path.Value()) ) {
		method = "local";
	} else if( access_via_nfs(full_path.Value()) ) {
		method = "local";
	} else {
		method = "remote";
	}

	if( use_fetch(method,full_path.Value()) ) {
		urlbuf += "fetch:";
	}

	if( use_compress(method,full_path.Value()) ) {
		urlbuf += "compress:";
	}

	append_buffer_info(urlbuf,method,full_path.Value());

	if( use_append(method,full_path.Value()) ) {
		urlbuf += "append:";
	}

	urlbuf += method;
	urlbuf += ":";
	urlbuf += full_path;
	actual_url = strdup(urlbuf.Value());

	dprintf(D_SYSCALLS,"\tactual_url: %s\n",actual_url);

	return 0;
}

static void append_buffer_info( MyString &url, const char *method, char const *path )
{
	MyString buffer_list;
	MyString buffer_string;
	MyString dir;
	MyString file;
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
		if( filename_remap_find(buffer_list.Value(),path,buffer_string) ||
		    filename_remap_find(buffer_list.Value(),file.Value(),buffer_string) ) {

			/* If the file is merely mentioned, turn on the default buffer */
			url += "buffer:";

			/* If there is also a size setting, use that */
			result = sscanf(buffer_string.Value(),"(%d,%d)",&s,&bs);
			if( result==2 ) url += buffer_string;

			return;
		}
	}

	/* Turn on buffering if the value is set and is not special or local */
	/* In this case, use the simple syntax 'buffer:' so as not to confuse old libs */

	if( s>0 && bs>0 && strcmp(method,"local") && strcmp(method,"special")  ) {
		url += "buffer:";
	}
}

/* Return true if this JobAd attribute contains this path */

static int attr_list_has_file( const char *attr, const char *path )
{
	char const *file;
	MyString str;

	file = condor_basename(path);

	Shadow->getJobAd()->LookupString(attr,str);
	StringList list(str.Value());

	if( list.contains_withwildcard(path) || list.contains_withwildcard(file) ) {
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

/*
"special" access means open the file locally, and also protect it by preventing checkpointing while it is open.   This needs to be applied to sockets (not trapped here) and special files that take the place of sockets.
*/

static int use_special_access( const char *file )
{
	return
		!strcmp(file,"/dev/tcp") ||
		!strcmp(file,"/dev/udp") ||
		!strcmp(file,"/dev/icmp") ||
		!strcmp(file,"/dev/ip");	
}

static int access_via_afs( const char * /* file */ )
{
	char *my_fs_domain=0;
	char *remote_fs_domain=0;
	int result=0;

	dprintf( D_SYSCALLS, "\tentering access_via_afs()\n" );

	my_fs_domain = param("FILESYSTEM_DOMAIN");
	thisRemoteResource->getFilesystemDomain(remote_fs_domain);

	if(!param_boolean_crufty("NONSTD_USE_AFS", false)) {
		dprintf( D_SYSCALLS, "\tnot configured to use AFS for file access\n" );
		goto done;
	}

	if(!my_fs_domain) {
		dprintf( D_SYSCALLS, "\tmy FILESYSTEM_DOMAIN is not defined\n" );
		goto done;
	}

	if(!remote_fs_domain) {
		dprintf( D_SYSCALLS, "\tdon't know FILESYSTEM_DOMAIN of executing machine\n" );
		goto done;
	}

	dprintf( D_SYSCALLS,
		"\tMy_FS_Domain = \"%s\", Executing_FS_Domain = \"%s\"\n",
		my_fs_domain,
		remote_fs_domain
	);

	if( strcmp(my_fs_domain,remote_fs_domain) != MATCH ) {
		dprintf( D_SYSCALLS, "\tFilesystem domains don't match\n" );
		goto done;
	}

	result = 1;

	done:
	dprintf(D_SYSCALLS,"\taccess_via_afs() returning %s\n", result ? "TRUE" : "FALSE" );
	if(my_fs_domain) free(my_fs_domain);
	if(remote_fs_domain) free(remote_fs_domain);
	return result;
}

static int access_via_nfs( const char * /* file */ )
{
	char *my_uid_domain=0;
	char *my_fs_domain=0;
	char *remote_uid_domain=0;
	char *remote_fs_domain=0;
	int result = 0;

	dprintf( D_SYSCALLS, "\tentering access_via_nfs()\n" );

	my_uid_domain = param("UID_DOMAIN");
	my_fs_domain = param("FILESYSTEM_DOMAIN");

	thisRemoteResource->getUidDomain(remote_uid_domain);
	thisRemoteResource->getFilesystemDomain(remote_fs_domain);

	if( !param_boolean_crufty("NONSTD_USE_NFS", false) ) {
		dprintf( D_SYSCALLS, "\tnot configured to use NFS for file access\n" );
		goto done;
	}

	if( !my_uid_domain ) {
		dprintf( D_SYSCALLS, "\tdon't know my UID domain\n" );
		goto done;
	}

	if( !my_fs_domain ) {
		dprintf( D_SYSCALLS, "\tdon't know my FS domain\n" );
		goto done;
	}

	if( !remote_uid_domain ) {
		dprintf( D_SYSCALLS, "\tdon't know UID domain of executing machine\n" );
		goto done;
	}

	if( !remote_fs_domain ) {
		dprintf( D_SYSCALLS, "\tdon't know FS domain of executing machine\n" );
		goto done;
	}

	dprintf( D_SYSCALLS,
		"\tMy_FS_Domain = \"%s\", Executing_FS_Domain = \"%s\"\n",
		my_fs_domain,
		remote_fs_domain
	);

	dprintf( D_SYSCALLS,
		"\tMy_UID_Domain = \"%s\", Executing_UID_Domain = \"%s\"\n",
		my_uid_domain,
		remote_uid_domain
	);

	if( strcmp(my_fs_domain,remote_fs_domain) != MATCH ) {
		dprintf( D_SYSCALLS, "\tFilesystem domains don't match\n" );
		goto done;
	}

	if( strcmp(my_uid_domain,remote_uid_domain) != MATCH ) {
		dprintf( D_SYSCALLS, "\tUID domains don't match\n" );
		goto done;
	}

	result = 1;

	done:
	dprintf( D_SYSCALLS, "\taccess_via_NFS() returning %s\n", result ? "TRUE" : "FALSE" );
	if (remote_fs_domain) free(remote_fs_domain);
	if (remote_uid_domain) free(remote_uid_domain);
	return result;
}

int
pseudo_ulog( ClassAd *ad )
{
	ULogEvent *event = instantiateEvent(ad);
	int result = 0;
	char const *critical_error = NULL;
	MyString CriticalErrorBuf;
	bool event_already_logged = false;
	bool put_job_on_hold = false;
	char const *hold_reason = NULL;
	char *hold_reason_buf = NULL;
	int hold_reason_code = 0;
	int hold_reason_sub_code = 0;

	if(!event) {
		MyString add_str;
		ad->sPrint(add_str);
		dprintf(
		  D_ALWAYS,
		  "invalid event ClassAd in pseudo_ulog: %s\n",
		  add_str.Value());
		return -1;
	}

	if(ad->LookupInteger(ATTR_HOLD_REASON_CODE,hold_reason_code)) {
		put_job_on_hold = true;
		ad->LookupInteger(ATTR_HOLD_REASON_SUBCODE,hold_reason_sub_code);
		ad->LookupString(ATTR_HOLD_REASON,&hold_reason_buf);
		if(hold_reason_buf) {
			hold_reason = hold_reason_buf;
		}
	}

	if( event->eventNumber == ULOG_REMOTE_ERROR ) {
		RemoteErrorEvent *err = (RemoteErrorEvent *)event;

		if(!err->getExecuteHost() || !*err->getExecuteHost()) {
			//Insert remote host information.
			char *execute_host = NULL;
			thisRemoteResource->getMachineName(execute_host);
			err->setExecuteHost(execute_host);
			delete[] execute_host;
		}

		if(err->isCriticalError()) {
			CriticalErrorBuf.formatstr(
			  "Error from %s: %s",
			  err->getExecuteHost(),
			  err->getErrorText());

			critical_error = CriticalErrorBuf.Value();
			if(!hold_reason) {
				hold_reason = critical_error;
			}

			//Temporary: the following causes critical remote errors
			//to be logged as ShadowExceptionEvents, rather than
			//RemoteErrorEvents.  The result is ugly, but guaranteed to
			//be compatible with other user-log reading tools.
			BaseShadow::log_except(critical_error);
			event_already_logged = true;
		}
	}

	if( !event_already_logged && !Shadow->uLog.writeEvent( event, ad ) ) {
		MyString add_str;
		ad->sPrint(add_str);
		dprintf(
		  D_ALWAYS,
		  "unable to log event in pseudo_ulog: %s\n",
		  add_str.Value());
		result = -1;
	}

	if(put_job_on_hold) {
		hold_reason = critical_error;
		if(!hold_reason) {
			hold_reason = "Job put on hold by remote host.";
		}
		Shadow->holdJobAndExit(hold_reason,hold_reason_code,hold_reason_sub_code);
		//should never get here, because holdJobAndExit() exits.
	}

	if( critical_error ) {
		//Suppress ugly "Shadow exception!"
		Shadow->exception_already_logged = true;

		//lame: at the time of this writing, EXCEPT does not want const:
		EXCEPT("%s", critical_error);
	}

	delete event;
	return result;
}

int
pseudo_get_job_attr( const char *name, MyString &expr )
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
		dprintf(D_SYSCALLS,"pseudo_get_job_attr(%s) = %s\n",name,expr.Value());
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
	MyString reqs;
	MyString newreqs;

	dprintf(D_SYSCALLS,"pseudo_constrain(%s)\n",expr);
	dprintf(D_SYSCALLS,"\tchanging AgentRequirements to %s\n",expr);
	
	if(pseudo_set_job_attr("AgentRequirements",expr)!=0) return -1;
	if(pseudo_get_job_attr("Requirements",reqs)!=0) return -1;

	if(strstr(reqs.Value(),"AgentRequirements")) {
		dprintf(D_SYSCALLS,"\tRequirements already refers to AgentRequirements\n");
		return 0;
	} else {
		newreqs.formatstr("(%s) && AgentRequirements",reqs.Value());
		dprintf(D_SYSCALLS,"\tchanging Requirements to %s\n",newreqs.Value());
		return pseudo_set_job_attr("Requirements",newreqs.Value());
	}
}

int pseudo_get_sec_session_info(
	char const *starter_reconnect_session_info,
	MyString &reconnect_session_id,
	MyString &reconnect_session_info,
	MyString &reconnect_session_key,
	char const *starter_filetrans_session_info,
	MyString &filetrans_session_id,
	MyString &filetrans_session_info,
	MyString &filetrans_session_key)
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
