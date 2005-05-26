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
#include "condor_attributes.h"
#include "condor_config.h"
#include "starter.h"

#include "java_proc.h"
#include "java_config.h"

extern CStarter * Starter;

JavaProc::JavaProc( ClassAd * jobAd, const char *xdir ) : VanillaProc(jobAd)
{
	execute_dir = strdup(xdir);
	startfile[0] = 0;
	endfile[0] = 0;
	ex_hier[0] = 0;
	ex_name[0] = 0;
	ex_type[0] = 0;
}

JavaProc::~JavaProc()
{
	free(execute_dir);
}

int JavaProc::StartJob()
{
	char tmp[ATTRLIST_MAX_EXPRESSION];
	
	char java_cmd[_POSIX_PATH_MAX];
	char java_args[_POSIX_ARG_MAX];
	char* jarfiles = NULL;

	ExprTree  *tree;
	char	  *tmp_args;
	char      *job_args;
	int		  length;

	// Construct the list of jar files for the command line
	// If a jar file is transferred locally, use its local name
	// (in the execute directory)
	// otherwise use the original name

	StringList jarfiles_orig_list;
	StringList jarfiles_local_list;
	StringList* jarfiles_final_list = NULL;

	if( JobAd->LookupString(ATTR_JAR_FILES,&jarfiles) ) {
		jarfiles_orig_list.initializeFromString( jarfiles );
		free( jarfiles );
		jarfiles = NULL;

		char * jarfile_name;
		const char * base_name;
		struct stat stat_buff;
		if( Starter->jic->iwdIsChanged() ) {
				// If the job's IWD has been changed (because we're
				// running in the sandbox due to file transfer), we
				// need to use a local version of the path to the jar
				// files, not the full paths from the submit machine. 
			jarfiles_orig_list.rewind();
			while( (jarfile_name = jarfiles_orig_list.next()) ) {
					// Construct the local name
				base_name = condor_basename( jarfile_name );
				MyString local_name = execute_dir;
				local_name += DIR_DELIM_CHAR;
				local_name += base_name; 

				if( stat(local_name.Value(), &stat_buff) == 0 ) {
						// Jar file exists locally, use local name
					jarfiles_local_list.append( local_name.Value() );
				} else {
						// Use the original name
					jarfiles_local_list.append (jarfile_name);
				}
			} // while(jarfiles_orig_list)

				// jarfiles_local_list is our real copy...
			jarfiles_final_list = &jarfiles_local_list;

		} else {  // !iwdIsChanged()

				// just use jarfiles_orig_list as our real copy...
			jarfiles_final_list = &jarfiles_orig_list;
		}			
	}

	if( !java_config(java_cmd,java_args,jarfiles_final_list) ) {
		dprintf(D_FAILURE|D_ALWAYS,"JavaProc: Java is not configured!\n");
		return 0;
	}

	sprintf(tmp,"%s=\"%s\"",ATTR_JOB_CMD,java_cmd);
	JobAd->InsertOrUpdate(tmp);

	sprintf(startfile,"%s%cjvm.start",execute_dir,DIR_DELIM_CHAR);
	sprintf(endfile,"%s%cjvm.end",execute_dir,DIR_DELIM_CHAR);

	// this new code pulls the value out of the classad with the
	// quote backwhacks still in place, so when we InsertOrUpdate() we
	// don't lose any of the backwhacks.  -stolley, 8/02

	tree = JobAd->Lookup(ATTR_JOB_ARGUMENTS);
	if ( tree == NULL ) {
		dprintf(D_ALWAYS,"JavaProc: %s is not defined!\n",ATTR_JOB_ARGUMENTS);
		return 0;
	} else if ( tree->RArg() == NULL ) { 
		// this shouldn't happen, but to be safe
		dprintf(D_ALWAYS,"JavaProc: %s RArg is not defined!\n",ATTR_JOB_ARGUMENTS);
		return 0;
	}
	tree->RArg()->PrintToNewStr(&tmp_args);
	job_args = tmp_args+1; // skip first quote
	length = strlen(job_args);
	job_args[length-1] = '\0'; // destroy last quote

	char *vm_args = NULL;

	JobAd->LookupString(ATTR_JOB_JAVA_VM_ARGS, &vm_args);

		// If no VM args, just pass empty string
	if (vm_args == NULL) {
		vm_args = (char *)malloc(1);
		vm_args[0] = '\000';
	}

	/*
	We really need to be passing these arguments through an
	argv-like interface to DaemonCore.  However, we don't have
	one.  Currently, the Windows version interprets and removes
	quotes in argument strings, but the UNIX version does not.
	Thus, we need two different strings, depending on the platform.
	Todd and Colin promise to fix this.  -thain
	*/
	
	sprintf(tmp,
#ifdef WIN32
		"%s=\"%s -Dchirp.config=\\\"%s%cchirp.config\\\" %s CondorJavaWrapper \\\"%s\\\" \\\"%s\\\" %s\"",
#else
		"%s=\"%s -Dchirp.config=%s%cchirp.config %s CondorJavaWrapper %s %s %s\"",
#endif

		ATTR_JOB_ARGUMENTS,
		java_args,
		execute_dir,
		DIR_DELIM_CHAR,
		vm_args,
		startfile,
		endfile,
		job_args);

	JobAd->InsertOrUpdate(tmp);
	
	free(tmp_args);

	free(vm_args);

	dprintf(D_ALWAYS,"JavaProc: Cmd=%s\n",java_cmd);
	dprintf(D_ALWAYS,"JavaProc: Args=%s\n",tmp);

	return VanillaProc::StartJob();
}

/*
Scan an exception hierarchy line.
Save the last exception name as the 'name', and look for
three specific middle types to save as the 'type'.
*/

int JavaProc::ParseExceptionLine( const char *line, char *name, char *type )
{
	char *copy, *tok, *last;

	name[0] = 0;
	type[0] = 0;

	copy = strdup(line);
	if(!copy) return 0;

	last = 0;
	tok = strtok(copy," \t\n");
	while(tok) {
		if(
			!strcmp(tok,"java.lang.Error") ||
			!strcmp(tok,"java.lang.LinkageError") ||
			!strcmp(tok,"java.lang.Exception") ||
			!strcmp(tok,"java.lang.RuntimeException")
		) {
			strcpy(type,tok);
		}
		last = tok;
		tok = strtok(0," \t\n");
	}

	if(last) {
		strcpy(name,last);
		free(copy);
		return 1;
	} else {
		free(copy);
		return 0;
	}
}

/*
Read the exception line from the status file,
and stash the necessary info in this object.
*/

int JavaProc::ParseExceptionFile( FILE *file )
{
	if(!fgets(ex_hier,sizeof(ex_hier),file)) return 0;
	if(!fgets(ex_hier,sizeof(ex_hier),file)) return 0;

	/* Kill the newline at the end of the line */
	ex_hier[strlen(ex_hier)-1] = 0;
	if(!ParseExceptionLine(ex_hier,ex_name,ex_type)) return 0;
	return 1;
}

/*
Given a POSIX exit status and the wrapper status files,
return exactly how the job exited.  Along the way, fill
in information about exceptions into the JobAd.
*/

java_exit_mode_t JavaProc::ClassifyExit( int status )
{
	FILE *file;
	int fields;

	char tmp[ATTRLIST_MAX_EXPRESSION];

	int normal_exit = WIFEXITED(status);
	int exit_code = WEXITSTATUS(status);
	int sig_num = WTERMSIG(status);

	java_exit_mode_t exit_mode;

	if(normal_exit) {
		dprintf(D_ALWAYS,"JavaProc: JVM exited normally with code %d\n",exit_code);
		file = fopen(startfile,"r");
		if(file) {
			dprintf(D_ALWAYS,"JavaProc: Wrapper left start record %s\n",startfile);
			fclose(file);
			file = fopen(endfile,"r");
			if(file) {
				dprintf(D_ALWAYS,"JavaProc: Wrapper left end record %s\n",endfile);
				fields = fscanf(file,"%s",tmp);
				if(fields!=1) {
					dprintf(D_FAILURE|D_ALWAYS,"JavaProc: Job called System.exit(%d)\n",exit_code);
					exit_mode = JAVA_EXIT_NORMAL;
				} else if(!strcmp(tmp,"normal")) {
					dprintf(D_FAILURE|D_ALWAYS,"JavaProc: Job returned from main()\n");
					exit_mode = JAVA_EXIT_NORMAL;
				} else if(!strcmp(tmp,"abnormal")) {	
					ParseExceptionFile(file);
					if(!strcmp(ex_type,"java.lang.Error")) {
						dprintf(D_FAILURE|D_ALWAYS,"JavaProc: Job threw a %s (%s), will retry it later.\n",ex_name,ex_type);
						exit_mode = JAVA_EXIT_SYSTEM_ERROR;
					} else {
						dprintf(D_FAILURE|D_ALWAYS,"JavaProc: Job threw a %s (%s), will return it to the user.\n",ex_name,ex_type);
						exit_mode = JAVA_EXIT_EXCEPTION;
					}
				} else if(!strcmp(tmp,"noexec")) {
					ParseExceptionFile(file);
					dprintf(D_FAILURE|D_ALWAYS,"JavaProc: Job could not be executed\n");
					exit_mode = JAVA_EXIT_EXCEPTION;
				} else {
					dprintf(D_FAILURE|D_ALWAYS,"JavaProc: Unknown wrapper result '%s'\n",tmp);
					exit_mode = JAVA_EXIT_SYSTEM_ERROR;
				}
				fclose(file);
			} else {
				dprintf(D_FAILURE|D_ALWAYS,"JavaProc: Wrapper did not leave end record %s\n",endfile);
				dprintf(D_ALWAYS,"JavaProc: Thus, job called System.exit(%d)\n",exit_code);
				exit_mode = JAVA_EXIT_NORMAL;
			}
		} else {
			dprintf(D_FAILURE|D_ALWAYS,"JavaProc: Wrapper did not leave start record.\n");
			dprintf(D_ALWAYS,"JavaProc: I'll assume Java is misconfigured here.\n");
			exit_mode = JAVA_EXIT_SYSTEM_ERROR;
		}
	} else {
		dprintf(D_FAILURE|D_ALWAYS,"JavaProc: JVM exited abnormally with signal %d\n",sig_num);
		exit_mode = JAVA_EXIT_SYSTEM_ERROR;
	}

	dprintf(D_ALWAYS,"JavaProc: unlinking %s and %s\n",startfile,endfile);

	priv_state s = set_priv(PRIV_ROOT);
	unlink(startfile);
	unlink(endfile);
	set_priv(s);

	return exit_mode;
}

/*
If the job exits with an exception, make it look
similar to exiting a C program with a signal.
Code that digs Java can look in the update ad for
the actual ExceptionName, and code that doesn't
dig Java still gets the general idea.
*/

#ifdef WIN32
#define EXCEPTION_EXIT_CODE 1
#else
#define EXCEPTION_EXIT_CODE (SIGTERM)
#endif

/*
If our job exited, then parse the output of the wrapper,
and return a POSIX status that resembles exactly what
happened.
*/

int JavaProc::JobCleanup(int pid, int status)
{
	if(pid==JobPid) {
		dprintf(D_ALWAYS,"JavaProc: JVM pid %d has finished\n",pid);
		dprintf( D_FULLDEBUG, "pid=%d, JobPid=%d\n",pid,JobPid );

		switch( ClassifyExit(status) ) {
			case JAVA_EXIT_NORMAL:
				/* status is unchanged */
				break;
			case JAVA_EXIT_EXCEPTION:
				status = EXCEPTION_EXIT_CODE;
				break;
			default:
			case JAVA_EXIT_SYSTEM_ERROR:
				status = 0;
				requested_exit = true;
				break;
		}

		return VanillaProc::JobCleanup(pid,status);

	} else {
		return 0;
	}
}

/*
If any exception information has been filled in,
then throw it into the update ad.
*/

bool JavaProc::PublishUpdateAd( ClassAd* ad )
{
	char tmp[ATTRLIST_MAX_EXPRESSION];

	if(ex_hier[0]) {
		sprintf(tmp,"%s=\"%s\"",ATTR_EXCEPTION_HIERARCHY,ex_hier);
		ad->InsertOrUpdate(tmp);
		dprintf(D_ALWAYS,"JavaProc: %s\n",tmp);
	}

	if(ex_name[0]) {
		sprintf(tmp,"%s=\"%s\"",ATTR_EXCEPTION_NAME,ex_name);
		ad->InsertOrUpdate(tmp);
		dprintf(D_ALWAYS,"JavaProc: %s\n",tmp);
	}

	if(ex_type[0]) {
		sprintf(tmp,"%s=\"%s\"",ATTR_EXCEPTION_TYPE,ex_type);
		ad->InsertOrUpdate(tmp);
		dprintf(D_ALWAYS,"JavaProc: %s\n",tmp);
	}

	return VanillaProc::PublishUpdateAd(ad);
}

