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
#include "condor_daemon_core.h"
#include "condor_debug.h"
#include "subsystem_info.h"
#include "condor_open.h"
#include "match_prefix.h"
#include "condor_base64.h"
#include "directory.h"
#include "condor_classad.h"
#include "condor_config.h"
#include "condor_arglist.h"
#include "basename.h"
#include "tmp_dir.h"

//-------------------------------------------------------------

	/* Here we have base64 coding routines based on gSOAP, as the one
	 * currently in the c++_util_lib based on OpenSSL are very flakey.
	 * Once they are fixed/improved, these should go away.
	 */

// For base64 coding, we use functions in the gSOAP support library
#include "stdsoap2.h"

// Caller needs to free the returned pointer
char* condor_base64_encode(const unsigned char *input, int length)
{
	char *buff = NULL;

	if ( length < 1 ) {
		buff = (char *)malloc(1);
		buff[0] = '\0';
		return buff;
	}

	int buff_len = (length+2)/3*4+1;
	buff = (char *)malloc(buff_len);
	ASSERT(buff);
	memset(buff,0,buff_len);

	struct soap soap;
	soap_init(&soap);

	soap_s2base64(&soap,input,buff,length);

	soap_destroy(&soap);
	soap_end(&soap);
	soap_done(&soap);

	return buff;
}

// Caller needs to free *output if non-NULL
void condor_base64_decode(const char *input,unsigned char **output, int *output_length)
{
	ASSERT( input );
	ASSERT( output );
	ASSERT( output_length );
	int input_length = strlen(input);

		// safe to assume output length is <= input_length
	*output = (unsigned char *)malloc(input_length + 1);
	ASSERT( *output );
	memset(*output, 0, input_length);

	struct soap soap;
	soap_init(&soap);

	soap_base642s(&soap,input,(char*)(*output),input_length,output_length);

	soap_destroy(&soap);
	soap_end(&soap);
	soap_done(&soap);

	if( *output_length < 0 ) {
		free( *output );
		*output = NULL;
	}
}


//-------------------------------------------------------------

/* Helper function: encode file filename into the resultAd as attribute attrname */
bool
stash_output_file(ClassAd* resultAd, const char* filename, const char* attrname)
{
	ASSERT(filename);
	ASSERT(attrname);
	ASSERT(resultAd);

	FILE *fp = safe_fopen_wrapper(filename,"r");
	if (!fp) return false;

	/* get the file size */
    fseek (fp, 0 , SEEK_END);
    long file_size = ftell (fp);
    rewind (fp);
   
	char *buffer = NULL;
	if ( file_size > 0 ) {
		/* allocate memory to contain the whole file */
		buffer = (char*) malloc (file_size);
		ASSERT(buffer);
		
		/* read the file into the buffer. */
		fread(buffer,1,file_size,fp);    

		/* Encode - note caller needs to free the returned pointer */
		char* encoded_data = condor_base64_encode((const unsigned char*)buffer, file_size);

		/* Shove into ad */
		if ( encoded_data ) {
			resultAd->Assign(attrname,encoded_data);
			free(encoded_data);
		}
	} else {
		resultAd->Assign(attrname,"");
	}

	if (buffer) free(buffer);
	fclose(fp);

	return true;
}

/* Helper function: write the input files mime encode in the inputAd into iwd */
bool
write_input_files(const ClassAd* inputAd, const char *iwd)
{
	bool return_value = true;
	char *file_name, *file_data;
	unsigned char *output;
	char buf[60];
	int i, output_length, fd;
	int flags = O_WRONLY | O_CREAT | O_TRUNC | O_APPEND | O_LARGEFILE;

	TmpDir tmpdir;
	MyString tmpdir_error;
	if (tmpdir.Cd2TmpDir(iwd,tmpdir_error) == false ) {
		// We failed to cd to our iwd.  This should NEVER happen.
		EXCEPT("write_input_files iwd failure: %s",tmpdir_error.Value());
	}

	i=0; file_name=NULL; file_data=NULL;output=NULL;
	while (return_value==true) {	// keep looping until break or failure
		i++;
		sprintf(buf,"INPUT_FILE_NAME_%d",i);
		inputAd->LookupString(buf,&file_name);
		if (!file_name) break;		// no more files
		sprintf(buf,"INPUT_FILE_DATA_%d",i);
		inputAd->LookupString(buf,&file_data);
		if ( file_data ) {
			// Caller needs to free *output if non-NULL
			condor_base64_decode(file_data,&output,&output_length);
			if ( output ) {
				fd = safe_open_wrapper_follow( condor_basename(file_name), flags, 0666 );
				if ( fd > -1 ) {
					write(fd,output,output_length);
					close(fd);
					dprintf(D_FULLDEBUG,"Wrote %d bytes to file %s%c%s\n",
								output_length, iwd, DIR_DELIM_CHAR, 
								condor_basename(file_name));
				} else {
					return_value = false;
				}
				free(output);
				output = NULL;
			} else {
				dprintf(D_ALWAYS,"Failed to decode base64 in attribute %s for file %s\n",
						buf,condor_basename(file_name));
				return_value = false;
			}
			free(file_data);
			file_data = NULL;
		} else {
			// INPUT_FILE_NAME specified, but no data... so just touch the file
			fd = safe_open_wrapper_follow( condor_basename(file_name), flags, 0666 );
			if ( fd > -1 ) {
				close(fd);
				dprintf(D_FULLDEBUG,"Wrote empty file %s%c%s\n",
								iwd, DIR_DELIM_CHAR, 
								condor_basename(file_name));
			}
		}
		free(file_name);
		file_name = NULL;
	}

	if ( return_value ) {
		dprintf(D_ALWAYS,"Wrote %d files to subdir %s\n",i-1,iwd);
	} else {
		dprintf(D_ALWAYS,"Failed to write file %d to subdir %s\n",i,iwd);
	}
		
	return return_value;
}

void
handle_process_request_error( const char* reason, int req_number, ClassAd* resultAd )
{
	const char * why = reason ? reason : "unknown failure";

	dprintf(D_ALWAYS,"ERROR: failed to process request %d - %s\n",
		req_number, why);

	if ( resultAd ) {
		resultAd->Assign("ERROR_STRING",why);
	}

	return;
}

void
do_process_request(const ClassAd *inputAd, ClassAd *resultAd, const int req_number, 
				   const char *iwd, const char *stdio_iwd)
{
		// Check for inputAd
	if ( !inputAd ) {
		handle_process_request_error("No input ad",req_number,resultAd);
		return;
	}

		// Map the CMD specified in the input via the config file.
	MyString UnmappedJobName,JobName;
	if (inputAd->LookupString(ATTR_JOB_CMD,UnmappedJobName) == 0 ) {
			// no CMD specified.
		handle_process_request_error("No CMD specified",req_number,resultAd);
		return;
	}
	char *auth_commands = param("SOAPSHELL_AUTHORIZED_COMMANDS");
	StringList auth_list(auth_commands,",");
	if ( auth_commands ) free(auth_commands);
		// Each command needs four tuples; anything else is a misconfiguration
	if ( auth_list.number() % 4 != 0 ) {
		handle_process_request_error("Service is misconfigured: SOAPSHELL_AUTHORIZED_COMMANDS malformed",req_number,resultAd);
		return;
	}

	if ( auth_list.contains_anycase(UnmappedJobName.Value()) == TRUE ) {
		JobName = auth_list.next();
	}
	if ( JobName.IsEmpty() ) {
			// the CMD not authorized
		handle_process_request_error("Requested CMD not authorized via SOAPSHELL_AUTHORIZED_COMMANDS",req_number,resultAd);
		return;
	}

		// handle command line arguments.
	ArgList args;
	args.SetArgV1SyntaxToCurrentPlatform();
	args.AppendArg(JobName.Value());	// set argv[0] to command
	char *soapshell_args = auth_list.next();
	if ( soapshell_args && strcmp(soapshell_args,"*") ) {
		if(!args.AppendArgsV1RawOrV2Quoted(soapshell_args,NULL)) {
			dprintf( D_ALWAYS, "ERROR: SOAPSHELL_ARGS config macro invalid\n" );
		}
	} else if(!args.AppendArgsFromClassAd(inputAd,NULL)) {
		handle_process_request_error("Failed to setup CMD arguments",req_number,resultAd);
		return;
	}
		
		// handle the environment.
	Env job_env;
	char *env_str = auth_list.next();
	if ( env_str && strcmp(env_str,"*") ) {
		if(!job_env.MergeFromV1RawOrV2Quoted(env_str,NULL) ) {
			dprintf(D_ALWAYS,"ERROR: SOAPSHELL_ENVIRONMENT config macro invalid\n");
		}
	} else if(!job_env.MergeFrom(inputAd,NULL)) {
		// bad environment string in job ad!
		handle_process_request_error("Request has faulty environment string",req_number,resultAd);
		return;
	}

		// Write input files into iwd (we will write stdin later)
	if ( !write_input_files(inputAd, iwd) ) {
		// failed to write input files
		handle_process_request_error("Failed to write input files",req_number,resultAd);
		return;
	}

		// handle stdin, stdout, and stderr redirection
	const char* jobstdin_ = dircat(stdio_iwd,"stdin");
	MyString jobstdin(jobstdin_);
	const char* jobstdout_ = dircat(stdio_iwd,"stdout");
	MyString jobstdout(jobstdout_);
	const char* jobstderr_ = dircat(stdio_iwd,"stderr");
	MyString jobstderr(jobstderr_);
	delete [] jobstdin_;
	delete [] jobstdout_;
	delete [] jobstderr_;
	int flags = O_WRONLY | O_CREAT | O_TRUNC | O_APPEND | O_LARGEFILE;
		// write stdin file is needed
	{
		char *input = NULL;
		unsigned char *output = NULL;
		int output_length = 0;
		int fd = -1;
		inputAd->LookupString(ATTR_JOB_INPUT,&input);
		if ( input ) {
			// Caller needs to free *output if non-NULL
			condor_base64_decode(input,&output,&output_length);
			if ( output ) {
				fd = safe_open_wrapper_follow( jobstdin.Value(), flags, 0666 );
				if ( fd > -1 ) {
					write(fd,output,output_length);
					close(fd);
				}
				free(output);
			}
			free(input);
			if ( fd < 0 ) {
				handle_process_request_error("Failed to write stdin",req_number,resultAd);
				return;
			}
		}
	}
	int fds[3]; 
		// initialize these to -2 to mean they're not specified.
		// -1 will be treated as an error.
	fds[0] = -2; fds[1] = -2; fds[2] = -2;	
	fds[0] = safe_open_wrapper_follow( jobstdin.Value(), O_RDONLY | O_LARGEFILE ); // stdin	
	fds[1] = safe_open_wrapper_follow( jobstdout.Value(), flags, 0666 );	// stdout
	fds[2] = safe_open_wrapper_follow( jobstderr.Value(), flags, 0666 );	// stderr
	/* Bail out if we couldn't open stdout/err files correctly */
	if( fds[1]==-1 || fds[2]==-1 ) {
		/* only close ones that had been opened correctly */
		for ( int i = 0; i <= 2; i++ ) {
			if ( fds[i] >= 0 ) {
				daemonCore->Close_FD ( fds[i] );
			}
		}
		handle_process_request_error("Failed to write stdout/err files",req_number,resultAd);
		return;
	}

		// Print what we are about to do to the log
	MyString args_string;
	args.GetArgsStringForDisplay(&args_string,1);
	dprintf( D_ALWAYS, "About to exec %s %s\n", JobName.Value(),
				 args_string.Value() );

		// Spawn a process, baby!!!
	int JobPid = daemonCore->Create_Process( JobName.Value(),	// executable
		                                     args,				// args
		                                     PRIV_UNKNOWN,		// priv_state - TODO
		                                     0,					// reaper id - TODO
		                                     FALSE,				// want_command_port
		                                     &job_env,			// job environment
		                                     iwd,				// job iwd
		                                     NULL,				// family_info - TODO
		                                     NULL,				// sock_inherit_list
		                                     fds				// stdio redirection
										);

		// NOTE: Create_Process() saves the errno for us if it is an
		// "interesting" error.
	char const *create_process_error = NULL;
	if(JobPid == FALSE && errno) create_process_error = strerror(errno);

		// now close the descriptors in fds array.  our child has inherited
		// them already, so we should close them so we do not leak descriptors.
	for ( int i = 0; i <= 2; i++ ) {
		if ( fds[i] >= 0 ) {
			daemonCore->Close_FD ( fds[i] );
		}
	}

	if ( JobPid == FALSE ) {
		JobPid = -1;
		MyString errormsg;
		errormsg.formatstr("Create_Process failed %s",create_process_error ? create_process_error : "");
		handle_process_request_error(errormsg.Value(),req_number,resultAd);
		return;
	}


	dprintf(D_ALWAYS,"Create_Process succeeded, pid=%d\n",JobPid);

		// TODO - For now, just deal w/ one at a time. :(
		// So for now just wait for the child to exit.
#ifdef WIN32
#error This service does not yet work on Windows
#else
	{
		int exit_status;
		pid_t pid;
		for (;;) {
			pid = wait(&exit_status);
			dprintf(D_FULLDEBUG,"WAIT returned %d, errno=%d\n",pid,errno);
			if (pid == JobPid ) break;
			if (pid == -1 && errno != EINTR) {
				EXCEPT("waitpid failed errno=%d",errno);
			}
		}
		if ( WIFEXITED(exit_status) ) {
			int status = WEXITSTATUS(exit_status);
			resultAd->Assign("EXIT_STATUS",status);
		}		
	}
#endif

		// Job has completed, exit status is in the ad.  Now put
		// the output files into the result ad.
	stash_output_file(resultAd, jobstdout.Value(), ATTR_JOB_OUTPUT);
	stash_output_file(resultAd, jobstderr.Value(), ATTR_JOB_ERROR);

}

ClassAd *
process_request(const ClassAd *inputAd)
{
	static unsigned int req_number = 0;

		// Number each new request.
	ClassAd *resultAd = new ClassAd();
	ASSERT(resultAd);
	resultAd->Assign("REQUEST_NUMBER",++req_number);

	dprintf(D_ALWAYS,"----------------------------------------\nProcessing request %d\n",req_number);

	dprintf(D_FULLDEBUG,"Contents of request classad:\n");
	if ( inputAd ) {
		((ClassAd*)inputAd)->dPrint(D_FULLDEBUG);
	}

		// Create two temp dirs, one to serve as the iwd for the command, another
		// to hold the stdout/err.
	char *iwd = create_temp_file(true);
	char *stdio_iwd = create_temp_file(true);
	if (!iwd || !stdio_iwd) {
		handle_process_request_error("failed to create temp dirs",req_number,resultAd);
		return resultAd;
	}

		// Do the work.
	do_process_request(inputAd, resultAd, req_number, iwd, stdio_iwd);

		// Blow away our temp dirs unless we are in debug mode
	bool debug_mode = param_boolean("SOAPSHELL_DEBUG_MODE",false);
	if ( !debug_mode ) {
		if ( iwd ) {
			Directory dir(iwd);
			dir.Remove_Full_Path(iwd);
			free(iwd);
		}
		if ( stdio_iwd ) {
			Directory dir(stdio_iwd);
			dir.Remove_Full_Path(stdio_iwd);
			free(stdio_iwd);
		}
	} else {
		if ( iwd ) {
			dprintf(D_ALWAYS,"SOAPSHELL_DEBUG_MODE=True so not removing iwd %s\n",
					iwd);
			free(iwd);
		}
		if ( stdio_iwd ) {
			dprintf(D_ALWAYS,"SOAPSHELL_DEBUG_MODE=True so not removing stdio_iwd %s\n",
					stdio_iwd);
			free(stdio_iwd);
		}
	}

	dprintf(D_FULLDEBUG,"Contents of result classad:\n");
	resultAd->dPrint(D_FULLDEBUG);

	dprintf(D_ALWAYS,"Finished processing request %d\n",req_number);
	return resultAd;
}

//-------------------------------------------------------------

void main_init(int  argc , char *  argv  [])
{
	char *testfile = NULL;
	ClassAd *inputAd = NULL;
	int i;

	dprintf(D_ALWAYS, "main_init() called\n");

	for (i=1; i<argc; i++ ) {
	
		if (match_prefix(argv[i],"-withfile")) {
			i++;
			if (argc <= i) {
				fprintf(stderr,
						"ERROR: Argument -withfile requires a parameter\n ");
				exit(1);
			}
			testfile = argv[i];
		}
	
	}	// end of parsing command line options

	if ( testfile ) {
		FILE* fp = safe_fopen_wrapper(testfile,"r");
		if (!fp) {
			fprintf(stderr,"ERROR: Unable to open test file %s\n",
					testfile);
			DC_Exit(1);
		}
		int EndFlag=0, ErrorFlag=0, EmptyFlag=0;
        if( !( inputAd=new ClassAd(fp,"***", EndFlag, ErrorFlag, EmptyFlag) ) ){
            fprintf( stderr, "ERROR:  Out of memory\n" );
            DC_Exit( 1 );
        }
		fclose(fp);
		if ( ErrorFlag || EmptyFlag ) {
			fprintf( stderr, "ERROR - file %s does not contain a parseable ClassAd\n",
					 testfile);
			DC_Exit(1);
		}
		// since this option is for testing, process then exit
		ClassAd * resultAd =  process_request(inputAd);
		resultAd->dPrint(D_ALWAYS);
		DC_Exit( 0 );
	}
}

//-------------------------------------------------------------

void 
main_config()
{
	dprintf(D_ALWAYS, "main_config() called\n");
}

//-------------------------------------------------------------

void main_shutdown_fast()
{
	dprintf(D_ALWAYS, "main_shutdown_fast() called\n");
	DC_Exit(0);
}

//-------------------------------------------------------------

void main_shutdown_graceful()
{
	dprintf(D_ALWAYS, "main_shutdown_graceful() called\n");
	DC_Exit(0);
}

//-------------------------------------------------------------

int
main( int argc, char **argv )
{
	set_mySubSystem("SOAPSHELL", SUBSYSTEM_TYPE_DAEMON );	// used by Daemon Core

	dc_main_init = main_init;
	dc_main_config = main_config;
	dc_main_shutdown_fast = main_shutdown_fast;
	dc_main_shutdown_graceful = main_shutdown_graceful;
	return dc_main( argc, argv );
}
