
#include "condor_common.h"
#include "condor_attributes.h"
#include "condor_config.h"

#include "java_proc.h"

JavaProc::JavaProc( ClassAd * jobAd, const char *xdir ) : VanillaProc(jobAd)
{
	execute_dir = strdup(xdir);
	startfile[0] = 0;
	endfile[0] = 0;
}

JavaProc::~JavaProc()
{
	free(execute_dir);
}

int JavaProc::StartJob()
{
	char tmp[ATTRLIST_MAX_EXPRESSION];
	char oldcmd[ATTRLIST_MAX_EXPRESSION];
	char args[ATTRLIST_MAX_EXPRESSION];
	char classpath[ATTRLIST_MAX_EXPRESSION];
	char jarfiles[ATTRLIST_MAX_EXPRESSION];
	char *java, *item;
	char *classpath_argument, *classpath_separator, *classpath_default;

	if( JobAd->LookupString( ATTR_JOB_CMD, oldcmd ) != 1 ) {
		dprintf(D_ALWAYS,"JavaProc: %s is not defined!\n",ATTR_JOB_CMD);
		return 0;
	}

	if( JobAd->LookupString( ATTR_JOB_ARGUMENTS, args ) != 1 ) {
		dprintf(D_ALWAYS,"JavaProc: %s is not defined!\n",ATTR_JOB_ARGUMENTS);
		return 0;
	}

	java = param("JAVA");
	if(!java) {
		dprintf(D_ALWAYS,"JavaProc: JAVA is not defined!\n");
		return 0;
	}

	classpath_argument = param("JAVA_CLASSPATH_ARGUMENT");
	if(!classpath_argument) classpath_argument = strdup("-classpath");

	classpath_separator = param("JAVA_CLASSPATH_SEPARATOR");
	if(!classpath_separator) classpath_separator = strdup(":");

	classpath_default = param("JAVA_CLASSPATH_DEFAULT");
	if(!classpath_default) classpath_default = strdup(".");

	classpath[0] = 0;

	StringList classpath_list(classpath_default);
	classpath_list.rewind();

	while(item=classpath_list.next()) {
		if(classpath[0]) strcat(classpath,classpath_separator);
		strcat(classpath,item);
	}

	if(JobAd->LookupString(ATTR_JAR_FILES,jarfiles)==1) {
		StringList jarfiles_list(jarfiles);
		jarfiles_list.rewind();
		while(item=jarfiles_list.next()) {
			strcat(classpath,classpath_separator);
			strcat(classpath,item);
		}
	}

	sprintf(tmp,"%s=\"%s\"",ATTR_JOB_CMD,java);
	JobAd->InsertOrUpdate(tmp);

	sprintf(startfile,"%s%cjvm.start",execute_dir,DIR_DELIM_CHAR);
	sprintf(endfile,"%s%cjvm.end",execute_dir,DIR_DELIM_CHAR);

	sprintf(tmp,
		"%s=\"%s %s -Dchirp.config=%s%cchirp.config CondorJavaWrapper %s %s %s\"",
		ATTR_JOB_ARGUMENTS,
		classpath_argument,
		classpath,
		execute_dir,
		DIR_DELIM_CHAR,
		startfile,
		endfile,
		args);

	JobAd->InsertOrUpdate(tmp);

	dprintf(D_ALWAYS,"JavaProc: Cmd=%s\n",java);
	dprintf(D_ALWAYS,"JavaProc: Args=%s\n",tmp);

	free(java);
	free(classpath_argument);
	free(classpath_separator);
	free(classpath_default);

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

	copy = strdup(line);
	if(!copy) return 0;

	last = 0;
	tok = strtok(copy," \t\n");
	while(tok) {
		if(
			!strcmp(tok,"java.lang.Error") ||
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
and place the necessary info in the JobAd.
*/

int JavaProc::ParseExceptionFile( FILE *file, char *ex_name, char *ex_type )
{
	char tmp[ATTRLIST_MAX_EXPRESSION];
	char ex_line[ATTRLIST_MAX_EXPRESSION];

	if(!fgets(ex_line,sizeof(ex_line),file)) return 0;
	if(!fgets(ex_line,sizeof(ex_line),file)) return 0;
	if(!ParseExceptionLine(ex_line,ex_name,ex_type)) return 0;

	sprintf(tmp,"%s=\"%s\"",ATTR_EXCEPTION_HIERARCHY,ex_line);
	JobAd->InsertOrUpdate(tmp);
	dprintf(D_ALWAYS,"JavaProc: %s\n",tmp);

	sprintf(tmp,"%s=\"%s\"",ATTR_EXCEPTION_NAME,ex_name);
	JobAd->InsertOrUpdate(tmp);
	dprintf(D_ALWAYS,"JavaProc: %s\n",tmp);

	sprintf(tmp,"%s=\"%s\"",ATTR_EXCEPTION_TYPE,ex_type);
	JobAd->InsertOrUpdate(tmp);
	dprintf(D_ALWAYS,"JavaProc: %s\n",tmp);

	return 1;
}

/*
Given a POSIX exit status and the wrapper status files,
return exactly hoe the job exited.  Along the way, fill
in information about exceptions into the JobAd.
*/

java_exit_mode_t JavaProc::ClassifyExit( int status )
{
	FILE *file;
	int fields;

	char tmp[ATTRLIST_MAX_EXPRESSION];
	char ex_name[ATTRLIST_MAX_EXPRESSION];
	char ex_type[ATTRLIST_MAX_EXPRESSION];

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
					dprintf(D_ALWAYS,"JavaProc: Job called System.exit(%d)\n",exit_code);
					exit_mode = JAVA_EXIT_NORMAL;
				} else if(!strcmp(tmp,"normal")) {
					dprintf(D_ALWAYS,"JavaProc: Job returned from main()\n");
					exit_mode = JAVA_EXIT_NORMAL;
				} else if(!strcmp(tmp,"abnormal")) {	
					ParseExceptionFile(file,ex_name,ex_type);
					if(!strcmp(ex_type,"java.lang.Error")) {
						dprintf(D_ALWAYS,"JavaProc: Job threw a %s (%s), will retry it later.\n",ex_name,ex_type);
						exit_mode = JAVA_EXIT_SYSTEM_ERROR;
					} else {
						dprintf(D_ALWAYS,"JavaProc: Job threw a %s (%s), will return it to the user.\n",ex_name,ex_type);
						exit_mode = JAVA_EXIT_EXCEPTION;
					}
				} else if(!strcmp(tmp,"noexec")) {
					dprintf(D_ALWAYS,"JavaProc: Job could not be executed\n");
					exit_mode = JAVA_EXIT_EXCEPTION;
				} else {
					dprintf(D_ALWAYS,"JavaProc: Unknown wrapper result '%s'\n",tmp);
					exit_mode = JAVA_EXIT_SYSTEM_ERROR;
				}
				fclose(file);
			} else {
				dprintf(D_ALWAYS,"JavaProc: Wrapper did not leave end record %s\n",endfile);
				dprintf(D_ALWAYS,"JavaProc: Thus, job called System.exit(%d)\n",exit_code);
				exit_mode = JAVA_EXIT_NORMAL;
			}
		} else {
			dprintf(D_ALWAYS,"JavaProc: Wrapper did not leave start record.\n");
			dprintf(D_ALWAYS,"JavaProc: I'll assume Java is misconfigured here.\n");
			exit_mode = JAVA_EXIT_SYSTEM_ERROR;
		}
	} else {
		dprintf(D_ALWAYS,"JavaProc: JVM exited abnormally with signal %d\n",sig_num);
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
If our job exited, then parse the output of the wrapper,
and return a POSIX status that resembles exactly what
happened.
*/

int JavaProc::JobExit(int pid, int status)
{
	if(pid==JobPid) {
		dprintf(D_ALWAYS,"JavaProc: JVM pid %d has finished\n",pid);

		switch( ClassifyExit(status) ) {
			case JAVA_EXIT_NORMAL:
				/* status is unchanged */
				break;
			case JAVA_EXIT_EXCEPTION:
				status = W_EXITCODE(1,0);
				break;
			default:
			case JAVA_EXIT_SYSTEM_ERROR:
				status = W_STOPCODE(SIGKILL);
				Requested_Exit = TRUE;
				break;
		}

		return VanillaProc::JobExit(pid,status);

	} else {
		return 0;
	}
}

/*
We don't have anything to put in the ad ad this point,
but if ParseExceptionFile has been run, it throws the
exception types into the ad.
*/

bool JavaProc::PublishUpdateAd( ClassAd* ad )
{
	return VanillaProc::PublishUpdateAd(ad);
}

