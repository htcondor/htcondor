/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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

extern ReliSock *syscall_sock;
extern BaseShadow *Shadow;
extern RemoteResource *thisRemoteResource;

static void append_buffer_info( char *url, char *method, char *path );
static int use_append( char *method, char *path );
static int use_compress( char *method, char *path );
static int use_fetch( char *method, char *path );
static int use_local_access( const char *file );
static int use_special_access( const char *file );
static int access_via_afs( const char *file );
static int access_via_nfs( const char *file );
static int lookup_boolean_param( const char *name );

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
pseudo_begin_execution()
{
	thisRemoteResource->beginExecution();
	return 0;
}


int
pseudo_get_job_info(ClassAd *&ad)
{
	ClassAd * the_ad;

	the_ad = thisRemoteResource->getJobAd();
	ASSERT( the_ad );

		// FileTransfer now makes sure we only do Init() once.
		//
		// New for WIN32: want_check_perms = false.
		// Since the shadow runs as the submitting user, we
		// let the OS enforce permissions instead of relying on
		// the pesky perm object to get it right.
	thisRemoteResource->filetrans.Init( the_ad, false, PRIV_USER );

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
 
static void complete_path( const char *short_path, char *full_path )
{
	if(short_path[0]==DIR_DELIM_CHAR) {
		strcpy(full_path,short_path);
	} else {
		// strcpy(full_path,CurrentWorkingDir);
		strcpy(full_path,Shadow->getIwd());
		strcat(full_path,DIR_DELIM_STRING);
		strcat(full_path,short_path);
	}
}

/*
This call translates a logical path name specified by a user job
into an actual url which describes how and where to fetch
the file from.  For example, joe/data might become buffer:remote:/usr/joe/data
*/

int pseudo_get_file_info_new( const char *logical_name, char *actual_url )
{
	char	remap_list[ATTRLIST_MAX_EXPRESSION];
	char	split_dir[_POSIX_PATH_MAX];
	char	split_file[_POSIX_PATH_MAX];
	char	full_path[_POSIX_PATH_MAX];
	char	remap[_POSIX_PATH_MAX];
	char	*method;

	dprintf( D_SYSCALLS, "\tlogical_name = \"%s\"\n", logical_name );

	/* The incoming logical name might be a simple, relative, or complete path */
	/* We need to examine both the full path and the simple name. */

	filename_split( (char*) logical_name, split_dir, split_file );
	complete_path( logical_name, full_path );

	/* Any name comparisons must check the logical name, the simple name, and the full path */

	if(Shadow->getJobAd()->LookupString(ATTR_FILE_REMAPS,remap_list) &&
	  (filename_remap_find( remap_list, (char*) logical_name, remap ) ||
	   filename_remap_find( remap_list, split_file, remap ) ||
	   filename_remap_find( remap_list, full_path, remap ))) {

		dprintf(D_SYSCALLS,"\tremapped to: %s\n",remap);

		/* If the remap is a full URL, return right away */
		/* Otherwise, continue processing */

		if(strchr(remap,':')) {
			dprintf(D_SYSCALLS,"\tremap is complete url\n");
			strcpy(actual_url,remap);
			return 0;
		} else {
			dprintf(D_SYSCALLS,"\tremap is simple file\n");
			complete_path( remap, full_path );
		}
	} else {
		dprintf(D_SYSCALLS,"\tnot remapped\n");
	}

	dprintf( D_SYSCALLS,"\tfull_path = \"%s\"\n", full_path );

	/* Now, we have a full pathname. */
	/* Figure out what url modifiers to slap on it. */

#ifdef HPUX
	/* I have no idea why this is happening, but I have seen it happen many
	 * times on the HPUX version, so here is a quick hack -Todd 5/19/95 */
	if ( strcmp(full_path,"/usr/lib/nls////strerror.cat") == 0 )
		strcpy(full_path,"/usr/lib/nls/C/strerror.cat\0");
#endif

	if( use_special_access(full_path) ) {
		method = "special";
	} else if( use_local_access(full_path) ) {
		method = "local";
	} else if( access_via_afs(full_path) ) {
		method = "local";
	} else if( access_via_nfs(full_path) ) {
		method = "local";
	} else {
		method = "remote";
	}

	actual_url[0] = 0;

	if( use_fetch(method,full_path) ) {
		strcat(actual_url,"fetch:");
	}

	if( use_compress(method,full_path) ) {
		strcat(actual_url,"compress:");
	}

	append_buffer_info(actual_url,method,full_path);

	if( use_append(method,full_path) ) {
		strcat(actual_url,"append:");
	}

	strcat(actual_url,method);
	strcat(actual_url,":");
	strcat(actual_url,full_path);

	dprintf(D_SYSCALLS,"\tactual_url: %s\n",actual_url);

	return 0;
}

static void append_buffer_info( char *url, char *method, char *path )
{
	char buffer_list[ATTRLIST_MAX_EXPRESSION];
	char buffer_string[ATTRLIST_MAX_EXPRESSION];
	char dir[_POSIX_PATH_MAX];
	char file[_POSIX_PATH_MAX];
	int s,bs,ps;
	int result;

	filename_split(path,dir,file);

	/* Get the default buffer setting */
	pseudo_get_buffer_info( &s, &bs, &ps );

	/* Now check for individual file overrides */
	/* These lines have the same syntax as a remap list */

	if(Shadow->getJobAd()->LookupString(ATTR_BUFFER_FILES,buffer_list)) {
		if( filename_remap_find(buffer_list,path,buffer_string) ||
		    filename_remap_find(buffer_list,file,buffer_string) ) {

			/* If the file is merely mentioned, turn on the default buffer */
			strcat(url,"buffer:");

			/* If there is also a size setting, use that */
			result = sscanf(buffer_string,"(%d,%d)",&s,&bs);
			if( result==2 ) strcat(url,buffer_string);

			return;
		}
	}

	/* Turn on buffering if the value is set and is not special or local */
	/* In this case, use the simple syntax 'buffer:' so as not to confuse old libs */

	if( s>0 && bs>0 && strcmp(method,"local") && strcmp(method,"special")  ) {
		strcat(url,"buffer:");
	}
}

/* Return true if this JobAd attribute contains this path */

static int attr_list_has_file( const char *attr, const char *path )
{
	char dir[_POSIX_PATH_MAX];
	char file[_POSIX_PATH_MAX];
	char str[ATTRLIST_MAX_EXPRESSION];
	StringList *list=0;

	filename_split( path, dir, file );

	str[0] = 0;
	Shadow->getJobAd()->LookupString(attr,str);
	list = new StringList(str);

	if( list->contains_withwildcard(path) || list->contains_withwildcard(file) ) {
		delete list;
		return 1;
	} else {
		delete list;
		return 0;
	}
}

static int use_append( char *method, char *path )
{
	return attr_list_has_file( ATTR_APPEND_FILES, path );
}

static int use_compress( char *method, char *path )
{
	return attr_list_has_file( ATTR_COMPRESS_FILES, path );
}

static int use_fetch( char *method, char *path )
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

static int access_via_afs( const char *file )
{
	char *my_fs_domain=0;
	char *remote_fs_domain=0;
	int result=0;

	dprintf( D_SYSCALLS, "\tentering access_via_afs()\n" );

	my_fs_domain = param("FILESYSTEM_DOMAIN");
	thisRemoteResource->getFilesystemDomain(remote_fs_domain);

	if(!lookup_boolean_param("USE_AFS")) {
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
	return result;
}

static int access_via_nfs( const char *file )
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

	if( !lookup_boolean_param("USE_NFS") ) {
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
	return result;
}

static int lookup_boolean_param( const char *name )
{
	int result;
	char *s = param(name);
	if(s) {
		if(s[0] == 'T' || s[0] == 't') {
			result = TRUE;
		} else {
			result = FALSE;
		}
		free(s);
	} else {
		result = FALSE;
	}
	return result;
}

int
pseudo_ulog( ClassAd *ad )
{
	ULogEvent *event = instantiateEvent(ad);
	int result = 0;
	char const *critical_error = NULL;
	MyString CriticalErrorBuf;
	ClassAdUnParser unp;

	if(!event) {
		std::string add_str;
		unp.Unparse( add_str, ad );
		dprintf(
		  D_ALWAYS,
		  "invalid event ClassAd in pseudo_ulog: %s\n",
		  add_str.c_str( ));
		return -1;
	}

	if( event->eventNumber == ULOG_REMOTE_ERROR ) {
		RemoteErrorEvent *err = (RemoteErrorEvent *)event;

		if(!err->getExecuteHost() || !*err->getExecuteHost()) {
			//Insert remote host information.
			char *execute_host = NULL;
			thisRemoteResource->getMachineName(execute_host);
			err->setExecuteHost(execute_host);
			delete execute_host;
		}

		if(err->isCriticalError()) {
			CriticalErrorBuf.sprintf(
			  "Error from %s on %s: %s",
			  err->getDaemonName(),
			  err->getExecuteHost(),
			  err->getErrorText());

			critical_error = CriticalErrorBuf.GetCStr();

			//Temporary: the following line causes critical remote errors
			//to be logged as ShadowExceptionEvents, rather than
			//RemoteErrorEvents.  The result is ugly, but guaranteed to
			//be compatible with other user-log reading tools.
			EXCEPT((char *)critical_error);
		}
	}

	if( !Shadow->uLog.writeEvent( event ) ) {
		std::string add_str;
		unp.Unparse( add_str, ad );
		dprintf(
		  D_ALWAYS,
		  "unable to log event in pseudo_ulog: %s\n",
		  add_str.c_str( ));
		result = -1;
	}

	if( critical_error ) {
		//Suppress ugly "Shadow exception!"
		Shadow->exception_already_logged = true;

		//lame: at the time of this writing, EXCEPT does not want const:
		EXCEPT((char *)critical_error);
	}

	delete event;
	return result;
}

int
pseudo_get_job_attr( const char *name, char *expr )
{
	ClassAd *ad = thisRemoteResource->getJobAd();
	ExprTree *e = ad->Lookup(name);
	ClassAdUnParser unp;
	std::string expr_string;
	if(e) {
		expr[0] = 0;
		unp.Unparse( expr_string, e );
		strcat( expr, expr_string.c_str( ) );
		dprintf(D_SYSCALLS,"pseudo_get_job_attr(%s) = %s\n",name,expr);
		return 0;
	} else {
		dprintf(D_SYSCALLS,"pseudo_get_job_attr(%s) failed\n",name);
		return -1;
	}
}

int
pseudo_set_job_attr( const char *name, const char *expr )
{
	if(Shadow->updateJobAttr(name,expr)) {
		dprintf(D_SYSCALLS,"pseudo_set_job_attr(%s,%s) succeeded\n",name,expr);
		char str[ATTRLIST_MAX_EXPRESSION];
		sprintf(str,"%s = %s",name,expr);
		ClassAd *ad = thisRemoteResource->getJobAd();
		ad->Insert(str);
		return 0;
	} else {
		dprintf(D_SYSCALLS,"pseudo_set_job_attr(%s,%s) failed\n",name,expr);
		return -1;
	}
}

int
pseudo_constrain( const char *expr )
{
	char reqs[ATTRLIST_MAX_EXPRESSION];
	char newreqs[ATTRLIST_MAX_EXPRESSION];

	dprintf(D_SYSCALLS,"pseudo_constrain(%s)\n",expr);
	dprintf(D_SYSCALLS,"\tchanging AgentRequirements to %s\n",expr);
	
	if(pseudo_set_job_attr("AgentRequirements",expr)!=0) return -1;
	if(pseudo_get_job_attr("Requirements",reqs)!=0) return -1;

	if(strstr(reqs,"AgentRequirements")) {
		dprintf(D_SYSCALLS,"\tRequirements already refers to AgentRequirements\n");
		return 0;
	} else {
		sprintf(newreqs,"(%s) && AgentRequirements",reqs);
		dprintf(D_SYSCALLS,"\tchanging Requirements to %s\n",newreqs);
		return pseudo_set_job_attr("Requirements",newreqs);
	}
}

