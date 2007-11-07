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
#include "../condor_procapi/procapi.h"
#include "../condor_c++_util/exponential_backoff.h"

//***
//constant static variables
//***
// command variables
static const char* PID_FILE_OPT = "--file";
static const char* BLOCK_OPT = "--block";
static const char* NO_BLOCK_OPT = "--noblock";
static const char* PRECISION_OPT = "--precision";

// defaults
static const char* DEFAULT_PID_FILE = "pid.file";

// midwife static variables
static const char* MIDWIFE_CMD = "uniq_pid_midwife";
static const char* MIDWIFE_USAGE = "uniq_pid_midwife [--noblock] [--file file] [--precision seconds] <program> [program_args]\n";

// undertaker static variables
static const char* UNDERTAKER_CMD = "uniq_pid_undertaker";
// static const char* UNDERTAKER_USAGE = "uniq_pid_undertaker [--block] [--file pidfile] [--precision seconds]\n";

static const int UNDERTAKER_FAILURE = -1;
static const int PROCESS_DEAD = 0;
static const int PROCESS_ALIVE = 1;
static const int PROCESS_UNCERTAIN = 2;

//***
// Private Helper Function Declarations
//***

// midwife
int midwife_main(int argc, char* argv[], const char* pidfile, bool blockflag, int* precision_range = 0);
pid_t midwife_executable(int argc, char* argv[], const char* pidfile, int* precision_range = 0);
FILE* createPidFile(const char* pidfile);

// undertaker
int undertaker_main( int argc, char* argv[], const char* pidfile, bool blockflag);
int isAliveFromFile(const char* pidfile, pid_t& pid);
int blockUntilDead(ProcessId& procId);
int handleUncertain(pid_t pid);

// common
void procapi_perror(int status, int pid, char* error_str);

/*
  Tool Main function
  Determines which tool the user executed and calls the appropriate
  function.
*/
int 
main(int argc, char* argv[])
{

		// extract the uniq_pid_main command and options 
		// from the target command and options
	char* command = argv[0];
	argc--;
	argv++;
  
		// remove the path, if its included in the command
	if( strchr(command, DIR_DELIM_CHAR) != NULL ){
		command = strrchr( command, DIR_DELIM_CHAR );
		command++;
	}
  
		// set the defaults
	const char* pidfile = DEFAULT_PID_FILE;
	int* precision_range = NULL;
	bool mblockflag = true;
	bool ublockflag = false;

		// continue to read the first opt until it 
		// doesn't match a uniq pid flag
	bool moreOpts = true;
	while( argc > 0 && moreOpts ){
    
			// Check for the pid file option
		if( strcmp(PID_FILE_OPT, argv[0]) == 0){
				// ensure there is an argument for the option
			if( argc < 2 ){
				fprintf(stderr, "ERROR: %s requires a file argument\n", PID_FILE_OPT);
				exit(-1);
			}//endif
    
				// extract the argument
			pidfile = argv[1];
			argc -= 2;
			argv += 2;
      
		}//endif

			// Check for the block argument
		else if( strcmp(BLOCK_OPT, argv[0]) == 0 ){
			mblockflag = true;
			ublockflag = true;
			argc--;
			argv++;
		}

			// Check for the no block argument
		else if( strcmp(NO_BLOCK_OPT, argv[0]) == 0 ){
			mblockflag = false;
			ublockflag = false;
			argc--;
			argv++;
      
		}

			// Check for the precision argument
		else if( strcmp(PRECISION_OPT, argv[0]) == 0 ){

			// ensure there is an argument for the option
			if( argc < 2 ){
				fprintf(stderr, "ERROR: %s requires a time in seconds\n",PRECISION_OPT);
				exit(-1);
			}//endif

      
				// ensure the next value is a number
			int i = 0;
			while( (argv[1][i]) != '\0' ){
				if( !isdigit(argv[1][i]) ){
					fprintf(stderr, "ERROR: %s requires a time in seconds\n", PRECISION_OPT);
					exit(-1);
				}
	    
				i++;
			}
      
				// get the precision
			precision_range = new int( atoi(argv[1]) );

			argc -= 2;
			argv += 2;
		}

			// No more options
		else{
			moreOpts = false;
		}
    
	}//endwhile

		// determine the command executed
		// and call the appropriate function
	if( strcmp(command, MIDWIFE_CMD) == 0 ){
		midwife_main(argc, argv, pidfile, mblockflag, precision_range);
	} else if( strcmp(command, UNDERTAKER_CMD) == 0 ){
		undertaker_main(argc, argv, pidfile, ublockflag);
	} else{
		fprintf(stderr, "ERROR: Could not recognize command [%s]\n", command);
		exit(-1);
	}

		// should never reach this point
	fprintf(stderr, "ERROR: Reached unreachable code, bailing out\n");
	return -1;
}


////////////////////////////////////////////////////////////////////////////////
// Midwife
////////////////////////////////////////////////////////////////////////////////
/*
  Main function for the midwife program.
  Midwifes a program into existence, creating a pid file artifact
  of its creation.
*/
int 
midwife_main(int argc, char* argv[], 
			 const char* pidfile, 
			 bool blockflag, 	
			 int* precision_range)
{
		// usage check
	if( argc < 1 ){
		fprintf(stderr, MIDWIFE_USAGE);
		exit(EXIT_FAILURE);
	}

		// midwife the executable
	pid_t pid = midwife_executable(argc, argv, pidfile, precision_range);
	if( pid == -1 ){
		exit(EXIT_FAILURE);
	}

		// block until the child completes, if neccessary
	if( blockflag && waitpid(pid, NULL, 0) == -1 ){
		perror("ERROR: Could not waitpid(...) in midwife_main");
		exit(EXIT_FAILURE);
	} 

		// success
	exit(EXIT_SUCCESS);
	return 0;
}

/*
  Midwifes the given executable into existance, creating a pid
  file for use by an undertaker.
*/
pid_t 
midwife_executable(int  /* argc */, char* argv[], 
				   const char* pidfile, int* precision_range)
{
  
		// Ensure we can create and write the pid file
	FILE* fp = createPidFile(pidfile);
	if( fp == NULL ){
		fprintf(stderr, 
				"ERROR: Could not create the pidfile [%s] in midwife_executable\n",
				pidfile);
		return -1;
	}

		// fork 
		// Must indicate that the parent wants notification of 
		// child's exit.  Therefore there is no chance of a race
		// where child dies and its pid is reused before the 
		// parent calls generateSignature(...).  As long as the parent
		// is alive and hasn't reaped the child, the pid cannot
		// be reused.
	pid_t pid = fork();
  

		// CHILD:
	if( pid == 0 ){
    
			// exec the program with its args
		if( execvp(argv[0], argv) == -1 ){
			fprintf(stderr, "FAILED: Midwife child could not exec[%s]: %s", 
					argv[0], strerror(errno));
		}

		exit(EXIT_FAILURE);
	}

		// ERROR: 
	else if( pid == -1 ){
		perror("FAILED: Midwife could not fork");
		return -1;
	}

		// PARENT:
	else{

			// create a process id for the child
		int status = 0;
		ProcessId* pProcId = NULL;
		if( ProcAPI::createProcessId(pid, pProcId, status, precision_range) == PROCAPI_FAILURE ){
			procapi_perror(status, pid, "ERROR: Failed to create process id in midwife_executable\n");
			return -1;
		}
		
			// write the child's id to the pid file
		if( pProcId->write(fp) == ProcessId::FAILURE ){
			fprintf(stderr,
					"ERROR: Failed to write process id to %s in midwife_executable\n", pidfile);
			return -1;
		}

			// ensure the signature remains unique
		int sleepfor = pProcId->computeWaitTime();
		while( (sleepfor = sleep(sleepfor)) != 0 );
		

			// generate a confirmation
		if( ProcAPI::confirmProcessId(*pProcId, status) == PROCAPI_FAILURE ){
			procapi_perror(status, pid, "ERROR: Failed to confirm process id in midwife_executable\n");
			return -1;
		}

			// Some OS's do not have confirmation support yet
			// so we must check whether the confirmation took
		if( pProcId->isConfirmed() ){
			
				// write the confirmation to the pid file
			if( pProcId->writeConfirmationOnly(fp) == ProcessId::FAILURE ){
				fprintf(stderr,
						"ERROR: Failed to write process confirmation to %s in midwife_executable\n", pidfile);
				return -1;
			}
		}
			// Warn the user that confirmation didn't take
		else{
			fprintf(stderr,
					"WARNING: Process confirmation is not enabled "
					"for this operating system.  "
					"The undertaker can only be certain about process death, "
					"no about process life\n");
		}

			// clean up the memory
		delete pProcId;

			// close the pid file
		fclose(fp);

			// success
		return( pid );

	}//endelse
  

		// if anything reaches here its a failure
	return -1;
}

/*
  Creates the pid file
  Ensures we can create and write to it.
*/
FILE* 
createPidFile(const char* pidfile)
{

		// create and open the file
  mode_t pidfile_mode = 
    S_IRUSR | S_IWUSR | // user read/write
    S_IRGRP | S_IWGRP | // group read/write
    S_IROTH | S_IWOTH;  // everyone read/write
  int fd = safe_open_wrapper(pidfile, O_WRONLY | O_CREAT | O_EXCL, pidfile_mode);
  if( fd == -1 ){
    perror("ERROR: Could not create the pid file");
    return NULL;
  }

	  // convert to a file pointer
	  // ALL THIS NONSENSE BECAUSE fopen DOESN'T HAVE O_EXCL
  FILE* fp = fdopen(fd, "w");
  if( fp == NULL ){
    perror("ERROR: Could not convert the pid file file descriptor to a file pointer");
    return( NULL );
  }
  
	  //success
  return fp;
}


////////////////////////////////////////////////////////////////////////////////
// Undertaker
////////////////////////////////////////////////////////////////////////////////
/*
  Main function for the undertaker.  Determines whether the process
  associated with the given pid file is alive or dead.
  Returns 1 for alive and 0 for dead.  Can optionally block until
  the process dies.
*/
int 
undertaker_main( int  /* argc */ , char ** /* argv */, 
				 const char* pidfile, bool blockflag)
{

		// Open the pid file 
	FILE* fp = safe_fopen_wrapper(pidfile, "r");
	if( fp == NULL ){
		fprintf(stderr, "ERROR: Failure occured attempting to open the pidfile [%s]: %s\n", 
				pidfile, strerror(errno));
		exit(UNDERTAKER_FAILURE);
	}

		// Create the processId from the pidfile
	int status;
	ProcessId procId(fp, status);
	if( status == ProcessId::FAILURE ){
		fprintf(stderr, "ERROR: Failure occured attempting create a id from pidfile [%s]\n",
				pidfile); 
		exit(UNDERTAKER_FAILURE);
	}

		// See if the process is still alive
	int aliveVal;
	if( ProcAPI::isAlive(procId, aliveVal) == PROCAPI_FAILURE ){
		procapi_perror(aliveVal, procId.getPid(), "ERROR: Failure occured attempting to determine if process is alive from pidfile.\n");
		exit(UNDERTAKER_FAILURE);
	}

		// close the pidfile
	fclose(fp);


		// Switch on the result
	if( aliveVal == PROCAPI_ALIVE ){
			// attempt to generate a confirmation
		int dummy_status;
		int retVal = ProcAPI::confirmProcessId(procId, dummy_status);

			// only optional don't fail if we can't
		if( retVal == PROCAPI_SUCCESS ){
				// append it to pid file
			FILE* pid_fp = safe_fopen_wrapper(pidfile, "a");
			if( pid_fp != NULL ){
				procId.writeConfirmationOnly(pid_fp);
			}
		}
		
			// block until dead if neccessary
		if( blockflag ){
			if( blockUntilDead(procId) == -1 ){
				fprintf(stderr, "ERROR: Encountered an error while attempting to block until the process is dead\n");
				exit(UNDERTAKER_FAILURE);
			}
			exit(PROCESS_DEAD);
		}
    
			// or exit with "alive"
		else{
			exit(PROCESS_ALIVE);
		}
	}

	else if( aliveVal == PROCAPI_DEAD ){
			// exit "dead"
		exit(PROCESS_DEAD);
	}
	
	else if( aliveVal == PROCAPI_UNCERTAIN ){
			// even though the response is uncertain 
			// print out as much info as possible
		handleUncertain(procId.getPid());
			// exit with "uncertain" status
		exit(PROCESS_UNCERTAIN);
	}
	
		// Error
	else{
		fprintf(stderr, "Unexpected result from ProcAPI::isAlive(...) status[%d]\n", status);
		exit(UNDERTAKER_FAILURE);
	}

		// Unreachable
	fprintf(stderr, "ERROR: Reached unreachable code, bailing out\n");
	exit(UNDERTAKER_FAILURE);
	return -1;
}

/*
  Blocks until the process associated with with the log is dead.
*/
int 
blockUntilDead(ProcessId& procId)
{
		// make an exponential backoff object
	ExponentialBackoff backoff(0, 30, 1.0);

	// prime the pump
	int aliveVal;
	int retVal = ProcAPI::isAlive(procId, aliveVal);
	
		// continue checking until the process is dead
	while( aliveVal != PROCAPI_DEAD && retVal != PROCAPI_FAILURE){

			//back off the appropriate amount
		backoff.nextBackoff();
    
			// try again
		retVal = ProcAPI::isAlive(procId, aliveVal);
	}
	
		// Error
	if( retVal == PROCAPI_FAILURE ){
		procapi_perror(aliveVal, procId.getPid(), "ERROR: Failure occured attempting to determine if process is alive from pidfile.\n");
		exit(UNDERTAKER_FAILURE);
	}
	
		//success
	return 0;
}

/*
  Handles the case where the undertaker is unable to determine
  whether a process is alive or dead.
  Attempts to print out information about the process so the
  user can decide.
*/
int 
handleUncertain(pid_t pid)
{

		// sanity check
	if( pid < 0 ){
		fprintf(stderr, 
				"Could not be certain of processes status because pid is illegal [%d] \n", 
				pid);
	}

	else{
			// get information about the process from procapi
		piPTR pi = NULL;
		int status;
		int result = ProcAPI::getProcInfo( pid, pi, status );
    
			// print it
		if( result != PROCAPI_FAILURE ){
			ProcAPI::printProcInfo(stderr, pi);
		} else{
			fprintf(stderr, 
					"Information about process currently unavailable\n");
		}
		
			// clean up the memory
		if( pi != NULL ){
			delete pi;
			pi = NULL;
		}
    
	}

		// success
	return 0;
}


////////////////////////////////////////////////////////////////////////////////
// Common
////////////////////////////////////////////////////////////////////////////////
/*
  Like perror but for ProcAPI::getProcInfo(...)  This is not in
  ProcAPI because its really only useful for tools, as opposed to log
  writing most callers into ProcAPI would require.
*/
void 
procapi_perror(int status, int pid, char* error_str)
{

		// print user error message if necessary
	if( error_str != NULL ){
		fprintf(stderr, "%s:\t", error_str);
	}

	switch(status){
    
			// failure due to nonexistance of pid
	case PROCAPI_NOPID:
		fprintf(stderr, "ERROR: Could not find the pid[%i]\n", pid);
		break;

			// failure due to permissions
	case PROCAPI_PERM:
		fprintf(stderr, "ERROR: Do not have permission to get info about pid[%i]\n", pid);
		break;

			// sometimes a kernel simply screws up and gives us back garbage
	case PROCAPI_GARBLED:
		fprintf(stderr, "ERROR: Kernel gave us garbage for pid[%i]\n", pid);
		break;
    
			// an error happened, but we didn't specify exactly what it was
	case PROCAPI_UNSPECIFIED:
		fprintf(stderr, "ERROR: Unknown error occured trying to get info for pid[%i]\n", pid);
		break;
    
	default:
		fprintf(stderr, "ERROR: Procapi returned an unknown status[%i] for pid[%i]", status, pid);
	}
}
