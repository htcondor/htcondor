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
#include "condor_attributes.h"
#include "condor_config.h"
#include "starter.h"
#include "condor_ver_info.h"
#include "java_proc.h"
#include "java_config.h"

extern class Starter * Starter;

JavaProc::JavaProc( ClassAd * jobAd, const char *xdir ) : VanillaProc(jobAd)
{
	execute_dir = strdup(xdir);
}

JavaProc::~JavaProc()
{
	free(execute_dir);
}

int JavaProc::StartJob()
{
	
	std::string java_cmd;
	char* jarfiles = NULL;
	ArgList args;
	std::string arg_buf;

	// Since we are adding to the argument list, we may need to deal
	// with platform-specific arg syntax in the user's args in order
	// to successfully merge them with the additional java VM args.
	args.SetArgV1SyntaxToCurrentPlatform();

	// Construct the list of jar files for the command line
	// If a jar file is transferred locally, use its local name
	// (in the execute directory)
	// otherwise use the original name

	std::vector<std::string> jarfiles_orig_list;
	std::vector<std::string> jarfiles_local_list;
	std::vector<std::string>* jarfiles_final_list = nullptr;

	if( JobAd->LookupString(ATTR_JAR_FILES,&jarfiles) ) {
		jarfiles_orig_list = split(jarfiles);
		free( jarfiles );
		jarfiles = NULL;

		const char * base_name;
		struct stat stat_buff;
		if( starter->jic->iwdIsChanged() ) {
				// If the job's IWD has been changed (because we're
				// running in the sandbox due to file transfer), we
				// need to use a local version of the path to the jar
				// files, not the full paths from the submit machine.
			for (auto& jarfile_name: jarfiles_orig_list) {
					// Construct the local name
				base_name = condor_basename( jarfile_name.c_str() );
				std::string local_name = execute_dir;
				local_name += DIR_DELIM_CHAR;
				local_name += base_name; 

				if( stat(local_name.c_str(), &stat_buff) == 0 ) {
						// Jar file exists locally, use local name
					jarfiles_local_list.emplace_back( local_name );
				} else {
					dprintf(D_ALWAYS, "JavaProc::StartJob could not stat jar file %s: errno %d\n",
						local_name.c_str(), errno);
					jarfiles_local_list.emplace_back(local_name);
				}
			}

				// jarfiles_local_list is our real copy...
			jarfiles_final_list = &jarfiles_local_list;

		} else {  // !iwdIsChanged()

				// just use jarfiles_orig_list as our real copy...
			jarfiles_final_list = &jarfiles_orig_list;
		}			
	}

	formatstr(startfile,"%s%cjvm.start",execute_dir,DIR_DELIM_CHAR);
	formatstr(endfile,"%s%cjvm.end",execute_dir,DIR_DELIM_CHAR);

	if( !java_config(java_cmd, args, jarfiles_final_list) ) {
		dprintf(D_ERROR,"JavaProc: Java is not configured!\n");
		return 0;
	}

	JobAd->Assign(ATTR_JOB_CMD, java_cmd);

	formatstr(arg_buf,"-Dchirp.config=%s%cchirp.config",execute_dir,DIR_DELIM_CHAR);
	args.AppendArg(arg_buf.c_str());

	char *jvm_args1 = NULL;
	char *jvm_args2 = NULL;
	std::string jvm_args_error;
	bool jvm_args_success = true;
	JobAd->LookupString(ATTR_JOB_JAVA_VM_ARGS1, &jvm_args1);
	JobAd->LookupString(ATTR_JOB_JAVA_VM_ARGS2, &jvm_args2);
	if(jvm_args2) {
		jvm_args_success = args.AppendArgsV2Raw(jvm_args2, jvm_args_error);
	}
	else if(jvm_args1) {
		jvm_args_success = args.AppendArgsV1Raw(jvm_args1, jvm_args_error);
	}
	free(jvm_args1);
	free(jvm_args2);
	if (!jvm_args_success) {
		dprintf(D_ALWAYS, "JavaProc: failed to parse JVM args: %s\n",
				jvm_args_error.c_str());
		return 0;
	}

	args.AppendArg("CondorJavaWrapper");
	args.AppendArg(startfile.c_str());
	args.AppendArg(endfile.c_str());

	std::string args_error;
	if(!args.AppendArgsFromClassAd(JobAd,args_error)) {
		dprintf(D_ALWAYS,"JavaProc: failed to read job arguments: %s\n",
				args_error.c_str());
		return 0;
	}

	// We are just talking to ourselves, so it is fine to use argument
	// syntax compatible with this current version of Condor.
	CondorVersionInfo ver_info;
	if(!args.InsertArgsIntoClassAd(JobAd,&ver_info,args_error)) {
		dprintf(D_ALWAYS,"JavaProc: failed to insert java job arguments: %s\n",
				args_error.c_str());
		return 0;
	}

	dprintf(D_ALWAYS,"JavaProc: Cmd=%s\n",java_cmd.c_str());
	std::string args_string;
	args.GetArgsStringForDisplay(args_string);
	dprintf(D_ALWAYS,"JavaProc: Args=%s\n",args_string.c_str());

	return VanillaProc::StartJob();
}

/*
Scan an exception hierarchy line.
Save the last exception name as the 'name', and look for
three specific middle types to save as the 'type'.
*/

int JavaProc::ParseExceptionLine( const char *line, std::string &exname, std::string &type )
{
	char *copy, *tok, *last;

	exname = "";
	type = "";

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
			type = tok;
		}
		last = tok;
		tok = strtok(0," \t\n");
	}

	if(last) {
		exname = last;
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
	if(!readLine(ex_hier, file)) return 0;
	if(!readLine(ex_hier, file)) return 0;

	/* Kill the newline at the end of the line */
	chomp(ex_hier);
	if(!ParseExceptionLine(ex_hier.c_str(),ex_name,ex_type)) return 0;
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

	char tmp[11]; // enough for "abnormal"
	tmp[0] = 0;

	int normal_exit = WIFEXITED(status);
	int exit_code = WEXITSTATUS(status);
	int sig_num = WTERMSIG(status);

	java_exit_mode_t exit_mode;

	if(normal_exit) {
		dprintf(D_ALWAYS,
		        "JavaProc: JVM exited normally with code %d\n",
		        exit_code);
		TemporaryPrivSentry sentry(PRIV_USER);
		file = safe_fopen_wrapper_follow(startfile.c_str(),"r");
		if(file) {
			dprintf(D_ALWAYS,
			        "JavaProc: Wrapper left start record %s\n",
			        startfile.c_str());
			fclose(file);
			file = safe_fopen_wrapper_follow(endfile.c_str(),"r");
			if(file) {
				dprintf(D_ALWAYS,
				        "JavaProc: Wrapper left end record %s\n",
				        endfile.c_str());
				fields = fscanf(file,"%10s",tmp); // no more than sizeof(tmp)
				if(fields!=1) {
					dprintf(D_ERROR,
					        "JavaProc: Job called System.exit(%d)\n",
					        exit_code);
					exit_mode = JAVA_EXIT_NORMAL;
				} else if(!strcmp(tmp,"normal")) {
					dprintf(D_ERROR,
					        "JavaProc: Job returned from main()\n");
					exit_mode = JAVA_EXIT_NORMAL;
				} else if(!strcmp(tmp,"abnormal")) {	
					ParseExceptionFile(file);
					if(!strcmp(ex_type.c_str(),"java.lang.Error")) {
						dprintf(D_ERROR,
					            "JavaProc: Job threw a %s (%s), "
						            "will retry it later.\n",
						        ex_name.c_str(),
						        ex_type.c_str());
						exit_mode = JAVA_EXIT_SYSTEM_ERROR;
					} else {
						dprintf(D_ERROR,
						        "JavaProc: Job threw a %s (%s), "
						            "will return it to the user.\n",
						        ex_name.c_str(),
						        ex_type.c_str());
						exit_mode = JAVA_EXIT_EXCEPTION;
					}
				} else if(!strcmp(tmp,"noexec")) {
					ParseExceptionFile(file);
					dprintf(D_ERROR,
					        "JavaProc: Job could not be executed\n");
					exit_mode = JAVA_EXIT_EXCEPTION;
				} else {
					dprintf(D_ERROR,
					        "JavaProc: Unknown wrapper result '%s'\n",
					        tmp);
					exit_mode = JAVA_EXIT_SYSTEM_ERROR;
				}
				fclose(file);
			} else {
				dprintf(D_ERROR,
				        "JavaProc: Wrapper did not leave end record %s\n",
				        endfile.c_str());
				dprintf(D_ALWAYS,
				        "JavaProc: Thus, job called System.exit(%d)\n",
				        exit_code);
				exit_mode = JAVA_EXIT_NORMAL;
			}
		} else {
			dprintf(D_ERROR,
			        "JavaProc: Wrapper did not leave start record.\n");
			dprintf(D_ALWAYS,
			        "JavaProc: I'll assume Java is misconfigured here.\n");
			exit_mode = JAVA_EXIT_SYSTEM_ERROR;
		}
	} else {
		dprintf(D_ERROR,
		        "JavaProc: JVM exited abnormally with signal %d\n",
		        sig_num);
		exit_mode = JAVA_EXIT_SYSTEM_ERROR;
	}

	dprintf(D_ALWAYS,"JavaProc: unlinking %s and %s\n",startfile.c_str(),endfile.c_str());

	priv_state s = set_priv(PRIV_ROOT);
	MSC_SUPPRESS_WARNING_FIXME(6031) // return value of unlink ignored.
	unlink(startfile.c_str());
	MSC_SUPPRESS_WARNING_FIXME(6031) // return value of unlink ignored.
	unlink(endfile.c_str());
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
bool
JavaProc::JobReaper(int pid, int status)
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

		return VanillaProc::JobReaper(pid,status);

	} else {
		return false;
	}
}

/*
If any exception information has been filled in,
then throw it into the update ad.
*/

bool JavaProc::PublishUpdateAd( ClassAd* ad )
{
	if(ex_hier.length()) {
		ad->Assign(ATTR_EXCEPTION_HIERARCHY,ex_hier);
		dprintf(D_ALWAYS,"JavaProc: %s \"%s\"\n",ATTR_EXCEPTION_HIERARCHY,ex_hier.c_str());
	}

	if(ex_name.length()) {
		ad->Assign(ATTR_EXCEPTION_NAME,ex_name);
		dprintf(D_ALWAYS,"JavaProc: %s \"%s\"\n", ATTR_EXCEPTION_NAME, ex_name.c_str());
	}

	if(ex_type.length()) {
		ad->Assign(ATTR_EXCEPTION_TYPE,ex_type);
		dprintf(D_ALWAYS,"JavaProc: %s \"%s\"\n",ATTR_EXCEPTION_TYPE,ex_type.c_str());
	}

	return VanillaProc::PublishUpdateAd(ad);
}

char const *
JavaProc::getArgv0()
{
	return NULL; // java does not like argv0 to be a lie, so don't lie
}
