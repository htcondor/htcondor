
#include "condor_common.h"
#include "condor_attributes.h"
#include "condor_config.h"
#include "JavaInfo.h"
#include "java_config.h"

extern int finish_main_config(void);

JavaInfo::JavaInfo()
{
	state = JAVA_INFO_STATE_VIRGIN;
	reaper_id = -1;
	query_pid = 0;
	output_file[0] = 0;
	error_file[0] = 0;
	java_vendor[0] = 0;
	java_version[0] = 0;
	java_mflops = 0;
	has_java = false;
}

/*
In most resource objects, this would cause the resource
state to be computed.  However, forking off a Java program
may take quite some time, so this compute() simply forks a
query process and returns right away.  A reconfig is forced
when the query returns.
*/

int JavaInfo::compute( amask_t how_much )
{
	if( IS_STATIC(how_much) ) {
		if(state==JAVA_INFO_STATE_VIRGIN) {
			dprintf( D_FULLDEBUG, 
					 "JavaInfo: Starting a new Java query process.\n" );
			query_create();
		} else if(state==JAVA_INFO_STATE_RUNNING) {
			dprintf( D_FULLDEBUG,
					 "JavaInfo: A query process is already running.\n" );
		} else {
			dprintf( D_FULLDEBUG, 
					 "JavaInfo: Using cached configuration.\n" );
		}
	}
	return 1;
}

/*
This function fills in Java info if it is available.
If not, it simply passes through.  When the data become
available, a reconfig is issued.
*/

int JavaInfo::publish( ClassAd *ad, amask_t how_much )
{
	char tmp[ATTRLIST_MAX_EXPRESSION];

	if( IS_STATIC(how_much) ) {
		if(state==JAVA_INFO_STATE_DONE) {
			if(has_java) {
				sprintf(tmp,"%s = TRUE",ATTR_HAS_JAVA);
				ad->InsertOrUpdate(tmp);

				if(java_vendor[0]) {
					sprintf(tmp,"%s = \"%s\"",ATTR_JAVA_VENDOR,java_vendor);
					ad->InsertOrUpdate(tmp);
				}

				if(java_version[0]) {
					sprintf(tmp,"%s = \"%s\"",ATTR_JAVA_VERSION,java_version);
					ad->InsertOrUpdate(tmp);
				}		

				if(java_mflops>0) {
					sprintf(tmp,"%s = %f",ATTR_JAVA_MFLOPS,java_mflops);
					ad->InsertOrUpdate(tmp);
				}		
			}
		}
	}
	return 1;
}

/*
Fork off a process to create a ClassAd of Java properties.
Put the output and error in files so we can figure out what
happened later.
*/

void JavaInfo::query_create()
{
	char java[_POSIX_PATH_MAX];
	char java_args[_POSIX_ARG_MAX];
	char real_args[_POSIX_ARG_MAX];
	int output_fd=-1, error_fd=-1;
	int fds[3];
	int btime;

	if(!java_config(java,java_args,0)) {
		dprintf(D_FULLDEBUG,"JavaInfo: JAVA is not configured.\n");
		state = JAVA_INFO_STATE_DONE;
		goto cleanup;
	}

	btime = param_integer("JAVA_BENCHMARK_TIME",0);

	tmpnam(output_file);
	output_fd = open(output_file,O_CREAT|O_TRUNC|O_WRONLY,0700);
	if(output_fd<0) {
		dprintf( D_ALWAYS, "JavaInfo: couldn't open %s: %s\n", 
				 output_file, strerror(errno) );
		unlink(output_file);
		goto cleanup;
	}

	tmpnam(error_file);
	error_fd = open(error_file,O_CREAT|O_TRUNC|O_WRONLY,0700);
	if(error_fd<0) {
		dprintf( D_ALWAYS, "JavaInfo: couldn't open %s: %s\n", 
				 error_file, strerror(errno) );
		unlink(output_file);
		unlink(error_file);
		goto cleanup;
	}

	if(reaper_id==-1) {
		reaper_id = daemonCore->Register_Reaper(
			"JavaInfo::query_reaper",
			(ReaperHandlercpp)&JavaInfo::query_reaper,
			"JavaInfo::query_reaper",
			this);

		if(reaper_id==FALSE) {
			dprintf( D_ALWAYS, "JavaInfo: Unable to register reaper!\n" );
			goto cleanup;
		}
	}

	fds[0] = 0;
	fds[1] = output_fd;
	fds[2] = error_fd;

	sprintf(real_args,"%s %s CondorJavaInfo old %d",java,java_args,btime);

	/*
	We must run this as a normal user.
	Many java wrappers involve a large amount of dynamic
	library manipulation.  The dynamic linker prohibits this
	manipulation if you are root or uid!=euid.
	So, we go to PRIV_USER_FINAL for user nobody.
	*/
#ifdef WIN32
	// Todd will remove this code soon... choosing the nobody login
	// name belongs inside of uids.C, not everywhere we call it.
	char nobody_login[60];
	// sprintf(nobody_login,"condor-run-dir_%d",daemonCore->getpid());
	sprintf(nobody_login,"condor-run-%d",daemonCore->getpid());
	init_user_nobody_loginname(nobody_login);
#endif

	init_user_ids("nobody");

	query_pid = daemonCore->Create_Process(java,real_args,PRIV_USER_FINAL,reaper_id,
		FALSE,NULL,NULL,FALSE,NULL,fds);
	if(query_pid==FALSE) {
		dprintf( D_ALWAYS, "JavaInfo: Unable to create query process!\n" );
		goto cleanup;
	}

 	dprintf( D_FULLDEBUG, "JavaInfo: Query process %d created.\n",
			 query_pid );  
	state = JAVA_INFO_STATE_RUNNING;

	cleanup:
	if(output_fd>=0) close(output_fd);
	if(error_fd>=0) close(error_fd);
}

/*
This function is called when the Java query process completes.
All of the Java properties are found in a file containing a
ClassAd.  If the file can be parsed, slurp up the properties,
and force a reconfig to publish them to the world.  If not, grab
the error file and send it to the debug stream.
*/

int JavaInfo::query_reaper( int pid, int status )
{
	bool show_error = false;

	if(pid!=query_pid) return 0;

	int normal_exit = WIFEXITED(status);
	int exit_code = WEXITSTATUS(status);
	int sig_num = WTERMSIG(status);

	if(normal_exit) {
		dprintf( D_FULLDEBUG, 
				 "JavaInfo: Query pid %d exited normally with code %d\n",
				 pid, exit_code );
	} else {
		dprintf( D_ALWAYS, 
				 "JavaInfo: Query pid %d exited abnormally with signal %d\n",
				 pid, sig_num );
	}

	java_vendor[0] = 0;
	java_version[0] = 0;
	has_java = false;

	if(normal_exit) {
		FILE *file = fopen(output_file,"r");
		if(file) {
			int is_eof=0, is_error=0, is_empty=0;
			ClassAd ad(file,"***",is_eof,is_error,is_empty);
			if(is_error) {
				dprintf( D_FAILURE|D_FULLDEBUG, 
						 "JavaInfo: Query result is not a valid ClassAd.\n" );
				show_error = true;
			} else if(is_empty) {
				dprintf( D_FAILURE|D_FULLDEBUG, "JavaInfo: Query result is empty.\n" );
				show_error = true;
			} else {
				has_java = true;
				if(ad.LookupString(ATTR_JAVA_VERSION,java_version)!=1) {
					java_version[0] = 0;
				} else {
					dprintf( D_FULLDEBUG, "JavaInfo: JavaVersion=%s\n",
							 java_version );
				}
				if(ad.LookupString(ATTR_JAVA_VENDOR,java_vendor)!=1) {
					java_vendor[0] = 0;
				} else {
					dprintf( D_FULLDEBUG, "JavaInfo: JavaVendor=%s\n",
							 java_vendor );
				}
				if(ad.LookupFloat(ATTR_JAVA_MFLOPS,java_mflops)!=1) {
					java_mflops = 0;
				} else {
					dprintf( D_FULLDEBUG, "JavaInfo: JavaMFlops=%f\n",
							 java_mflops );
				}
			}
			fclose(file);
		} else {
			dprintf( D_FAILURE|D_FULLDEBUG, "JavaInfo: Query process did not leave any "
					 "output in %s\n", output_file );
			show_error = true;
		}		
	} else  {
		dprintf( D_FAILURE|D_FULLDEBUG, "JavaInfo: Java is not installed.\n" );
		show_error = true;
	}

	if(show_error) {
		char line[ATTRLIST_MAX_EXPRESSION];
		FILE *file;

		dprintf(D_FULLDEBUG,"JavaInfo: Output stream from JVM:\n");
		file = fopen(output_file,"r");
		if(file) {
			while(fgets(line,sizeof(line),file)) {
				dprintf(D_FULLDEBUG,"JavaInfo: %s",line);
			}
			fclose(file);
		}

		dprintf(D_FULLDEBUG,"JavaInfo: Error stream from JVM:\n");
		file = fopen(error_file,"r");
		if(file) {
			while(fgets(line,sizeof(line),file)) {
				dprintf(D_FULLDEBUG,"JavaInfo: %s",line);
			}
			fclose(file);
		}

	}

	unlink(error_file);
	unlink(output_file);
	state = JAVA_INFO_STATE_DONE;

	finish_main_config();

	return 0;
}

